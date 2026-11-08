// Original: volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/dynamics/world/broadphaseborder/hkpbroadphaseborder.cpp
//
// Havok 2014.2.5 (havok_impl.md HK4): six flat AABB phantoms on the faces of the world's broadphase box; an entity
// touching one has left the broadphase and is fixed or removed (maxPositionExceededCallback, which the game
// overrides), at once or sorted by uid after the step. From rfg.exe.

#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/dynamics/world/broadphaseborder/hkpbroadphaseborder.h"

#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/algorithm/sort/hksort.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/agent/collidable/hkpcollidable.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/dynamics/entity/hkpentity.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/dynamics/entity/hkprigidbody.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/dynamics/phantom/hkpaabbphantom.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/dynamics/phantom/hkpphantom.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/dynamics/phantom/hkpphantomoverlaplistener.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/dynamics/world/hkpworld.h"


static const int HK_BROAD_PHASE_ENTITY = 1; // hkpWorldObject::BroadPhaseType
static const int HK_BROAD_PHASE_BORDER = 3;

// 0x00DF2ED0: the entities' processing order (by uid, deterministic).
bool hkpBroadPhaseBorder_EntityCmpLess(const hkpEntity* a, const hkpEntity* b)
{
    return a->m_uid < b->m_uid;
}

// 0x00DF3100
// The border holds a reference on itself until the world is deleted. The phantoms (filter info 0) cover the +x, -x,
// +y, -y, +z and -z faces, in that order, with no thickness.
hkpBroadPhaseBorder::hkpBroadPhaseBorder(hkpWorld* world, hkpWorldCinfo::BroadPhaseBorderBehaviour type, hkBool postponeAndSortCallbacks)
{
    m_world = world;
    m_type = type;
    m_postponeAndSortCallbacks = postponeAndSortCallbacks;
    addReference();
    m_world->addWorldDeletionListener(this);
    m_world->addWorldPostSimulationListener(this);

    const hkVector4f lo = m_world->m_broadPhaseExtents[0];
    const hkVector4f hi = m_world->m_broadPhaseExtents[1];
    const __m128 xzMask = _mm_castsi128_ps(_mm_set_epi32(0, -1, 0, -1)); // 0x013619A0: x and z
    hkAabb faces[6];
    faces[0].m_min.m_quad = _mm_move_ss(lo.m_quad, hi.m_quad); // (hi.x, lo.y, lo.z, lo.w)
    faces[0].m_max = hi;
    faces[1].m_min = lo;
    faces[1].m_max.m_quad = _mm_move_ss(hi.m_quad, lo.m_quad); // (lo.x, hi.y, hi.z, hi.w)
    faces[2].m_min.m_quad = _mm_or_ps(_mm_and_ps(xzMask, lo.m_quad), _mm_andnot_ps(xzMask, hi.m_quad)); // (lo.x, hi.y, lo.z, hi.w)
    faces[2].m_max = hi;
    faces[3].m_min = lo;
    faces[3].m_max.m_quad = _mm_or_ps(_mm_and_ps(xzMask, hi.m_quad), _mm_andnot_ps(xzMask, lo.m_quad)); // (hi.x, lo.y, hi.z, lo.w)
    faces[4].m_min.m_quad = _mm_shuffle_ps(lo.m_quad, hi.m_quad, 0xE4); // (lo.x, lo.y, hi.z, hi.w)
    faces[4].m_max = hi;
    faces[5].m_min = lo;
    faces[5].m_max.m_quad = _mm_shuffle_ps(hi.m_quad, lo.m_quad, 0xE4); // (hi.x, hi.y, lo.z, lo.w)
    for (int i = 0; i < 6; i++)
    {
        hkpAabbPhantom* phantom = new hkpAabbPhantom(faces[i], 0);
        phantom->m_collidable.m_broadPhaseHandle.m_type = HK_BROAD_PHASE_BORDER;
        phantom->addPhantomOverlapListener(this);
        m_phantoms[i] = phantom;
        world->addPhantom(phantom);
    }
}

// 0x00DF34A0
hkpBroadPhaseBorder::~hkpBroadPhaseBorder()
{
    for (int i = 0; i < 6; i++)
    {
        if (m_phantoms[i] != nullptr)
        {
            m_phantoms[i]->removeReference();
            m_phantoms[i] = nullptr;
        }
    }
}

// 0x00DF3090: an entity entering a phantom has left the broadphase.
void hkpBroadPhaseBorder::collidableAddedCallback(const hkpCollidableAddedEvent& event)
{
    const hkpCollidable* collidable = event.m_collidable;
    if (collidable->m_broadPhaseHandle.m_type != HK_BROAD_PHASE_ENTITY)
    {
        return;
    }
    hkpEntity* entity = (hkpEntity*)collidable->getOwner();
    if (entity == nullptr)
    {
        return;
    }
    if (m_postponeAndSortCallbacks.m_bool)
    {
        m_entitiesExitingBroadPhase.pushBack(entity);
        entity->addReference();
        return;
    }
    maxPositionExceededCallback(entity);
}

// 0x00DF2EC0
void hkpBroadPhaseBorder::collidableRemovedCallback(const hkpCollidableRemovedEvent& event)
{
}

// 0x00DF2F70: removes the phantoms from the world and stops listening.
void hkpBroadPhaseBorder::deactivate()
{
    if (m_world == nullptr)
    {
        return;
    }
    m_world->removePhantomBatch(m_phantoms, 6);
    for (int i = 0; i < 6; i++)
    {
        m_phantoms[i]->removePhantomOverlapListener(this);
        m_phantoms[i]->removeReference();
        m_phantoms[i] = nullptr;
    }
    static_cast<hkpWorldDeletionListener*>(this)->worldDeletedCallback(m_world);
    m_world = nullptr;
}

// 0x00DF2EF0
void hkpBroadPhaseBorder::maxPositionExceededCallback(hkpEntity* entity)
{
    if (m_type == hkpWorldCinfo::BROADPHASE_BORDER_ASSERT || m_type == hkpWorldCinfo::BROADPHASE_BORDER_FIX_ENTITY)
    {
        static_cast<hkpRigidBody*>(entity)->setMotionType(hkpMotion::MOTION_FIXED, HK_ENTITY_ACTIVATION_DO_ACTIVATE, HK_UPDATE_FILTER_ON_ENTITY_FULL_CHECK);
    }
    else if (m_type == hkpWorldCinfo::BROADPHASE_BORDER_REMOVE_ENTITY)
    {
        if (entity->m_world == m_world)
        {
            m_world->removeEntity(entity);
        }
    }
}

// 0x00DF3020: the postponed entities, by uid.
void hkpBroadPhaseBorder::postSimulationCallback(hkpWorld* world)
{
    if (m_entitiesExitingBroadPhase.m_size == 0)
    {
        return;
    }
    hkAlgorithm::quickSort(m_entitiesExitingBroadPhase.m_data, m_entitiesExitingBroadPhase.m_size, hkpBroadPhaseBorder_EntityCmpLess);
    for (int i = 0; i < m_entitiesExitingBroadPhase.m_size; i++)
    {
        maxPositionExceededCallback(m_entitiesExitingBroadPhase.m_data[i]);
    }
    hkReferencedObject::removeReferences((const hkReferencedObject* const*)m_entitiesExitingBroadPhase.m_data, m_entitiesExitingBroadPhase.m_size, sizeof(hkpEntity*));
    m_entitiesExitingBroadPhase.m_size = 0;
}

// 0x00DF2F30: drops the border's own reference.
void hkpBroadPhaseBorder::worldDeletedCallback(hkpWorld* world)
{
    m_world->removeWorldDeletionListener(this);
    m_world->removeWorldPostSimulationListener(this);
    removeReference();
}
