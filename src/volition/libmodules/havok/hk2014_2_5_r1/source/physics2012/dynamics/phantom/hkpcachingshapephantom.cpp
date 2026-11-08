// Original: volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/dynamics/phantom/hkpcachingshapephantom.cpp
//
// Havok 2014.2.5 (player_minimal.md P1): the shape phantom that keeps a collision agent for each collidable its
// broadphase box overlaps (the character proxy's phantom), from rfg.exe. Its queries go through the cached agents
// instead of the dispatcher's static functions (hkpSimpleShapePhantom).

#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/dynamics/phantom/hkpcachingshapephantom.h"

#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/algorithm/sort/hksort.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/agent/collidable/hkpcollidable.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/agent/hkpcollisionagent.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/agent/hkpprocesscollisioninput.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/agent/query/hkpcdbodypaircollector.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/agent/query/hkpcdpointcollector.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/agent/query/hkplinearcastcollisioninput.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/dispatch/hkpcollisiondispatcher.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/query/castutil/hkplinearcastinput.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/shape/hkpshape.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/dynamics/constraint/hkpconstraintowner.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/dynamics/phantom/hkpphantomoverlaplistener.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/dynamics/world/hkpworld.h"

// 0x00E0E120
hkpCachingShapePhantom::hkpCachingShapePhantom(const hkpShape* shape, const hkTransformf& transform, unsigned int collisionFilterInfo)
    : hkpShapePhantom(shape, transform)
{
    m_orderDirty.m_bool = 0;
    m_collidable.m_broadPhaseHandle.m_collisionFilterInfo = collisionFilterInfo;
}

// 0x00DF0550 (hkpcachingshapephantom.h)
hkpCachingShapePhantom::hkpCachingShapePhantom(hkFinishLoadedObjectFlag flag)
    : hkpShapePhantom(flag)
{
}

// 0x00E0E170: the agents are cleaned up with a scratch constraint owner (agents of a phantom own no constraints)
hkpCachingShapePhantom::~hkpCachingShapePhantom()
{
    hkpConstraintOwner constraintOwner;
    for (int i = m_collisionDetails.getSize() - 1; i >= 0; i--)
    {
        m_collisionDetails[i].m_agent->cleanup(constraintOwner);
    }
    m_collisionDetails.clearAndDeallocate();
}

// 0x00E0DED0
hkpPhantomType hkpCachingShapePhantom::getType() const
{
    return HK_PHANTOM_CACHING_SHAPE;
}

// 0x00E0DF90
hkBool hkpCachingShapePhantom::isOverlappingCollidableAdded(const hkpCollidable* collidable)
{
    hkBool result;
    for (int i = 0; i < m_collisionDetails.getSize(); i++)
    {
        if (m_collisionDetails[i].m_collidable == collidable)
        {
            result.m_bool = 1;
            return result;
        }
    }
    result.m_bool = 0;
    return result;
}

// 0x00E0E210: collidables without a shape are ignored; a listener may reject. The agent is created with the world's
// collision input at the PSI quality and without predictive agents.
void hkpCachingShapePhantom::addOverlappingCollidable(hkpCollidable* collidable)
{
    if (collidable->m_shape == nullptr)
    {
        return;
    }
    if (fireCollidableAdded(collidable) != HK_COLLIDABLE_ACCEPT)
    {
        return;
    }
    CollisionDetail& detail = m_collisionDetails.expandOne();
    hkpProcessCollisionInput input(*m_world->m_collisionInput);
    input.m_collisionQualityInfo = input.m_dispatcher.val()->getCollisionQualityInfo(1); // hkpCollisionDispatcher::COLLISION_QUALITY_PSI
    input.m_createPredictiveAgents = 0;
    const hkpCollisionDispatcher* dispatcher = input.m_dispatcher.val();
    const int typeA = m_collidable.m_shape->m_type.m_storage;
    const int typeB = collidable->m_shape->m_type.m_storage;
    detail.m_agent = dispatcher->m_agent2Func[dispatcher->m_agent2Types[typeA][typeB]].m_createFunc(m_collidable, *collidable, input, nullptr);
    detail.m_collidable = collidable;
    m_orderDirty.m_bool = 1;
}

// 0x00E0E060: searched from the end; the agent is cleaned up and the last detail moves into the hole
void hkpCachingShapePhantom::removeOverlappingCollidable(hkpCollidable* collidable)
{
    hkpConstraintOwner constraintOwner;
    if (collidable->m_shape == nullptr)
    {
        return;
    }
    hkBool wasAdded;
    for (int i = m_collisionDetails.getSize() - 1; i >= 0; i--)
    {
        if (m_collisionDetails[i].m_collidable == collidable)
        {
            wasAdded.m_bool = 1;
            fireCollidableRemoved(collidable, wasAdded);
            hkpCollisionAgent* agent = m_collisionDetails[i].m_agent;
            if (agent != nullptr)
            {
                agent->cleanup(constraintOwner);
            }
            m_collisionDetails.removeAt(i);
            m_orderDirty.m_bool = 1;
            return;
        }
    }
    wasAdded.m_bool = 0;
    fireCollidableRemoved(collidable, wasAdded);
}

// 0x00E0E7F0: by broadphase handle id
hkBool hkpCachingShapePhantom::OrderByUid::operator()(const CollisionDetail& a, const CollisionDetail& b)
{
    hkBool result;
    result.m_bool = a.m_collidable->m_broadPhaseHandle.m_id < b.m_collidable->m_broadPhaseHandle.m_id;
    return result;
}

// 0x00E0E3A0 (quickSortRecursive instance 0x00E0E870)
void hkpCachingShapePhantom::ensureDeterministicOrder()
{
    if (m_orderDirty.m_bool)
    {
        const int size = m_collisionDetails.getSize();
        if (size > 1)
        {
            hkAlgorithm::quickSortRecursive(m_collisionDetails.begin(), 0, size - 1, OrderByUid());
        }
        m_orderDirty.m_bool = 0;
    }
}

// 0x00E0E3E0: the shape, transform, filter info, listeners and properties (not the overlaps)
hkpPhantom* hkpCachingShapePhantom::clone() const
{
    hkpCachingShapePhantom* phantom = new hkpCachingShapePhantom(m_collidable.m_shape, m_motionState.getTransform(), m_collidable.m_broadPhaseHandle.m_collisionFilterInfo);
    phantom->m_overlapListeners = m_overlapListeners;
    phantom->m_phantomListeners = m_phantomListeners;
    phantom->copyProperties(this);
    return phantom;
}

// 0x00E0DFE0: every agent re-filters its shape collection, with a copy of the world's collision input
void hkpCachingShapePhantom::updateShapeCollectionFilter()
{
    hkpCollisionInput input = *m_world->m_collisionInput;
    hkpConstraintOwner constraintOwner;
    for (int i = 0; i < m_collisionDetails.getSize(); i++)
    {
        m_collisionDetails[i].m_agent->updateShapeCollectionFilter(m_collidable, *m_collisionDetails[i].m_collidable, input, constraintOwner);
    }
}

// 0x0080D620 (hkpcachingshapephantom.inl)
hkArray<hkpCachingShapePhantom::CollisionDetail, hkContainerHeapAllocator>& hkpCachingShapePhantom::getCollisionDetails()
{
    return m_collisionDetails;
}

// 0x00E0E340
void hkpCachingShapePhantom::deallocateInternalArrays()
{
    if (m_collisionDetails.getSize() == 0)
    {
        m_collisionDetails.clearAndDeallocate();
    }
    hkpShapePhantom::deallocateInternalArrays();
}

// 0x00E0E570: the phantom moves to the position, its broadphase box grows to cover the cast (grown by the start
// point tolerance), then every cached agent casts
void hkpCachingShapePhantom::setPositionAndLinearCast(const hkVector4f& position, const hkpLinearCastInput& input, hkpCdPointCollector& castCollector, hkpCdPointCollector* startCollector)
{
    m_motionState.getTransform().setTranslation(position);
    hkAabb aabb;
    m_collidable.m_shape->getAabb(m_motionState.getTransform(), m_world->m_collisionInput->m_tolerance.val() * 0.5f + input.m_startPointTolerance, aabb);
    hkVector4f path;
    path.setSub(input.m_to, position);
    hkVector4f zero;
    zero.setZero();
    hkVector4f pathMin;
    pathMin.m_quad = _mm_min_ps(zero.m_quad, path.m_quad);
    hkVector4f pathMax;
    pathMax.m_quad = _mm_max_ps(zero.m_quad, path.m_quad);
    aabb.m_min.add(pathMin);
    aabb.m_max.add(pathMax);
    updateBroadPhase(aabb);

    hkpLinearCastCollisionInput castInput;
    castInput.set(*m_world->m_collisionInput);
    castInput.setPathAndTolerance(path, input.m_startPointTolerance);
    castInput.m_maxExtraPenetration = input.m_maxExtraPenetration;
    for (int i = m_collisionDetails.getSize() - 1; i >= 0; i--)
    {
        CollisionDetail& detail = m_collisionDetails[i];
        detail.m_agent->linearCast(m_collidable, *detail.m_collidable, castInput, castCollector, startCollector);
    }
}

// 0x00E0E740: the rotation is set here, the rest by setPositionAndLinearCast
void hkpCachingShapePhantom::setTransformAndLinearCast(const hkTransformf& transform, const hkpLinearCastInput& input, hkpCdPointCollector& castCollector, hkpCdPointCollector* startCollector)
{
    m_motionState.getTransform().getRotation() = transform.getRotation();
    setPositionAndLinearCast(transform.getTranslation(), input, castCollector, startCollector);
}

// 0x00E0DEE0: the cached agents, from the last one (the world's collision input by default)
void hkpCachingShapePhantom::getClosestPoints(hkpCdPointCollector& collector, const hkpCollisionInput* input)
{
    if (input == nullptr)
    {
        input = m_world->m_collisionInput;
    }
    for (int i = m_collisionDetails.getSize() - 1; i >= 0; i--)
    {
        CollisionDetail& detail = m_collisionDetails[i];
        detail.m_agent->getClosestPoints(m_collidable, *detail.m_collidable, *input, collector);
    }
}

// 0x00E0DF30: as getClosestPoints, stopping at the collector's early out
void hkpCachingShapePhantom::getPenetrations(hkpCdBodyPairCollector& collector, const hkpCollisionInput* input)
{
    if (input == nullptr)
    {
        input = m_world->m_collisionInput;
    }
    for (int i = m_collisionDetails.getSize() - 1; i >= 0; i--)
    {
        CollisionDetail& detail = m_collisionDetails[i];
        detail.m_agent->getPenetrations(m_collidable, *detail.m_collidable, *input, collector);
        if (collector.m_earlyOut.m_bool)
        {
            return;
        }
    }
}
