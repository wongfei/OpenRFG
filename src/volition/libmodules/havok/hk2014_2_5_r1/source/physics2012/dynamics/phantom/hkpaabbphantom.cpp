// Original: volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/dynamics/phantom/hkpaabbphantom.cpp
//
// Havok 2014.2.5 (havok_impl.md HK7): the box-only phantom (broadphase overlaps, no shape), from rfg.exe. Its ray
// and linear casts are not reached in RFG.

#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/dynamics/phantom/hkpaabbphantom.h"

#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/algorithm/sort/hksort.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/agent/collidable/hkpcollidable.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/agent/hkpprocesscollisioninput.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/agent/query/hkplinearcastcollisioninput.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/dispatch/hkpcollisiondispatcher.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/filter/hkpcollisionfilter.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/query/castutil/hkplinearcastinput.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/query/castutil/hkpworldraycastoutput.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/dynamics/phantom/hkpphantomoverlaplistener.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/query/castutil/hkpworldraycastinput.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/shape/hkpshape.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/shape/query/hkpshaperaycastinput.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/dynamics/world/hkpworld.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/dynamics/phantom/hkpphantomtype.h"

// 0x00DF4140
hkpAabbPhantom::hkpAabbPhantom(const hkAabb& aabb, unsigned int collisionFilterInfo)
    : hkpPhantom(nullptr)
{
    m_orderDirty.m_bool = 0;
    m_aabb = aabb;
    m_collidable.m_broadPhaseHandle.m_collisionFilterInfo = collisionFilterInfo;
}

// 0x00DF0A70 (hkpaabbphantom.inl)
hkpAabbPhantom::hkpAabbPhantom(hkFinishLoadedObjectFlag flag)
    : hkpPhantom(flag)
{
}

// 0x00DF3E60
hkpAabbPhantom::~hkpAabbPhantom()
{
}

// 0x00DF37B0
hkpPhantomType hkpAabbPhantom::getType() const
{
    return HK_PHANTOM_AABB;
}

// 0x00811830 (hkpaabbphantom.h)
hkMotionState* hkpAabbPhantom::getMotionState()
{
    return nullptr;
}

// 0x00DF37C0
void hkpAabbPhantom::calcAabb(hkAabb& aabb)
{
    aabb = m_aabb;
}

// 0x00DF37E0: the broadphase follows at once (or the update is queued)
void hkpAabbPhantom::setAabb(const hkAabb& newAabb)
{
    m_aabb = newAabb;
    updateBroadPhase(m_aabb);
}

// 0x00DE86E0
const hkAabb& hkpAabbPhantom::getAabb() const
{
    return m_aabb;
}

// 0x00DF3810
hkBool hkpAabbPhantom::isOverlappingCollidableAdded(const hkpCollidable* collidable)
{
    hkBool result;
    result.m_bool = m_overlappingCollidables.indexOf(const_cast<hkpCollidable*>(collidable)) >= 0;
    return result;
}

// 0x00DF3DC0: unless a listener rejects it
void hkpAabbPhantom::addOverlappingCollidable(hkpCollidable* collidable)
{
    if (fireCollidableAdded(collidable) == HK_COLLIDABLE_ACCEPT)
    {
        m_overlappingCollidables.pushBack(collidable);
        m_orderDirty.m_bool = 1;
    }
}

// 0x00DF3D40: the listeners hear of the removal (whether or not it was added); the last collidable moves into the
// hole
void hkpAabbPhantom::removeOverlappingCollidable(hkpCollidable* collidable)
{
    const int i = m_overlappingCollidables.indexOf(collidable);
    hkBool wasAdded;
    wasAdded.m_bool = i >= 0;
    fireCollidableRemoved(collidable, wasAdded);
    if (i >= 0)
    {
        m_overlappingCollidables.removeAt(i);
        m_orderDirty.m_bool = 1;
    }
}

// 0x00DF4410: by broadphase handle id
hkBool hkpAabbPhantom::OrderByUid::operator()(const hkpCollidable* a, const hkpCollidable* b)
{
    hkBool result;
    result.m_bool = a->m_broadPhaseHandle.m_id < b->m_broadPhaseHandle.m_id;
    return result;
}

// 0x00DF3E20
void hkpAabbPhantom::ensureDeterministicOrder()
{
    if (m_orderDirty.m_bool)
    {
        const int size = m_overlappingCollidables.getSize();
        if (size > 1)
        {
            hkAlgorithm::quickSortRecursive(m_overlappingCollidables.begin(), 0, size - 1, OrderByUid());
        }
        m_orderDirty.m_bool = 0;
    }
}

// 0x00DF40E0
void hkpAabbPhantom::deallocateInternalArrays()
{
    if (m_overlappingCollidables.getSize() == 0)
    {
        m_overlappingCollidables.clearAndDeallocate();
    }
}

// 0x00DF41D0: the box, filter info, listeners and properties (not the overlaps)
hkpPhantom* hkpAabbPhantom::clone() const
{
    hkpAabbPhantom* phantom = new hkpAabbPhantom(m_aabb, m_collidable.m_broadPhaseHandle.m_collisionFilterInfo);
    phantom->m_overlapListeners = m_overlapListeners;
    phantom->m_phantomListeners = m_phantomListeners;
    phantom->copyProperties(this);
    return phantom;
}

// 0x00DF3850: through each overlapping collidable, in its own space (the tank tread wheels' ground casts)
void hkpAabbPhantom::castRay(const hkpWorldRayCastInput& input, hkpRayHitCollector& collector) const
{
    hkpShapeRayCastInput shape_input;
    shape_input.m_filterInfo = input.m_filterInfo;
    shape_input.m_collidable = NULL;
    if (input.m_enableShapeCollectionFilter && m_world->m_collisionFilter != NULL)
    {
        shape_input.m_rayShapeCollectionFilter = m_world->m_collisionFilter;
    }
    else
    {
        shape_input.m_rayShapeCollectionFilter = NULL;
    }
    shape_input.m_userData = input.m_userData;
    for (int i = 0; i < m_overlappingCollidables.getSize(); i++)
    {
        const hkpCollidable* collidable = m_overlappingCollidables[i];
        const hkpShape* shape = collidable->getShape();
        if (shape != NULL)
        {
            const hkTransformf& transform = collidable->getTransform();
            shape_input.m_from.setTransformedInversePos(transform, input.m_from);
            shape_input.m_to.setTransformedInversePos(transform, input.m_to);
            shape_input.m_collidable = collidable;
            shape->castRayWithCollector(shape_input, *collidable, collector);
        }
    }
}

// 0x00DF3AB0: the closest hit over the overlapping collidables, its normal back in the world
void hkpAabbPhantom::castRay(const hkpWorldRayCastInput& input, hkpWorldRayCastOutput& output) const
{
    hkpShapeRayCastInput shape_input;
    shape_input.m_filterInfo = input.m_filterInfo;
    shape_input.m_collidable = NULL;
    if (input.m_enableShapeCollectionFilter && m_world->m_collisionFilter != NULL)
    {
        shape_input.m_rayShapeCollectionFilter = m_world->m_collisionFilter;
    }
    else
    {
        shape_input.m_rayShapeCollectionFilter = NULL;
    }
    shape_input.m_userData = input.m_userData;
    for (int i = 0; i < m_overlappingCollidables.getSize(); i++)
    {
        const hkpCollidable* collidable = m_overlappingCollidables[i];
        const hkpShape* shape = collidable->getShape();
        if (shape != NULL)
        {
            const hkTransformf& transform = collidable->getTransform();
            shape_input.m_from.setTransformedInversePos(transform, input.m_from);
            shape_input.m_to.setTransformedInversePos(transform, input.m_to);
            shape_input.m_collidable = collidable;
            if (shape->castRay(shape_input, output))
            {
                output.m_rootCollidable = collidable;
            }
        }
    }
    if (output.m_rootCollidable != NULL)
    {
        output.m_normal.setRotatedDir(output.m_rootCollidable->getTransform().getRotation(), output.m_normal);
    }
}

// 0x00DF3EC0: the agents of the cast shape against each overlapping collidable the world's filter allows
void hkpAabbPhantom::linearCast(const hkpCollidable* const toBeCast, const hkpLinearCastInput& input, hkpCdPointCollector& castCollector, hkpCdPointCollector* startCollector) const
{
    hkpLinearCastCollisionInput cast_input;
    cast_input.set(*m_world->m_collisionInput);
    hkVector4f path;
    path.setSub(input.m_to, toBeCast->getTransform().getTranslation());
    cast_input.setPathAndTolerance(path, input.m_startPointTolerance);
    cast_input.m_maxExtraPenetration = input.m_maxExtraPenetration;
    const hkpCollisionDispatcher* dispatcher = m_world->m_collisionDispatcher;
    const hkpCollidableCollidableFilter* filter = m_world->m_collisionFilter;
    for (int i = 0; i < m_overlappingCollidables.getSize(); i++)
    {
        const hkpCollidable* collidable = m_overlappingCollidables[i];
        const hkBool enabled = filter->isCollisionEnabled(*toBeCast, *collidable);
        const hkpShape* shape = collidable->getShape();
        if (shape != NULL && enabled && collidable != toBeCast)
        {
            dispatcher->getLinearCastFunc(toBeCast->getShape()->getType(), shape->getType())(*toBeCast, *collidable, cast_input, castCollector, startCollector);
        }
    }
}
