// Original: volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/dynamics/phantom/hkpsimpleshapephantom.cpp
//
// Havok 2014.2.5 (havok_impl.md HK6, HK7): the shape phantom that keeps the collidables its broadphase box overlaps
// (its collision details), from rfg.exe; its narrowphase queries run the dispatcher's static functions against them.

#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/dynamics/phantom/hkpsimpleshapephantom.h"

#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/agent/collidable/hkpcollidable.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/agent/hkpprocesscollisioninput.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/agent/query/hkpcdbodypaircollector.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/agent/query/hkpcdpointcollector.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/dispatch/hkpcollisiondispatcher.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/shape/hkpshape.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/dynamics/world/hkpworld.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/algorithm/sort/hksort.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/agent/query/hkplinearcastcollisioninput.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/query/castutil/hkplinearcastinput.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/dynamics/phantom/hkpphantomoverlaplistener.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/dynamics/phantom/hkpphantomtype.h"

static inline hkcdShapeType::ShapeTypeEnum hkSimpleShapePhantom_type(const hkpCollidable& collidable)
{
    return static_cast<hkcdShapeType::ShapeTypeEnum>(collidable.getShape()->m_type.m_storage);
}

// 0x00DDFC00: the overlapping collidables from the last one (the world's collision input by default)
void hkpSimpleShapePhantom::getClosestPoints(hkpCdPointCollector& collector, const hkpCollisionInput* input)
{
    if (input == nullptr)
    {
        input = m_world->m_collisionInput;
    }
    const hkpCollisionDispatcher* dispatcher = input->m_dispatcher;
    const hkpCollidable& collA = m_collidable;
    for (int i = m_collisionDetails.getSize() - 1; i >= 0; i--)
    {
        const hkpCollidable& collB = *m_collisionDetails[i].m_collidable;
        dispatcher->getGetClosestPointsFunc(hkSimpleShapePhantom_type(collA), hkSimpleShapePhantom_type(collB))(collA, collB, *input, collector);
    }
}

// 0x00DDFC80: as getClosestPoints, stopping at the collector's early out
void hkpSimpleShapePhantom::getPenetrations(hkpCdBodyPairCollector& collector, const hkpCollisionInput* input)
{
    if (input == nullptr)
    {
        input = m_world->m_collisionInput;
    }
    const hkpCollisionDispatcher* dispatcher = input->m_dispatcher;
    const hkpCollidable& collA = m_collidable;
    for (int i = m_collisionDetails.getSize() - 1; i >= 0; i--)
    {
        const hkpCollidable& collB = *m_collisionDetails[i].m_collidable;
        dispatcher->getGetPenetrationsFunc(hkSimpleShapePhantom_type(collA), hkSimpleShapePhantom_type(collB))(collA, collB, *input, collector);
        if (collector.m_earlyOut.m_bool)
        {
            return;
        }
    }
}

// 0x00DDFE30
hkpSimpleShapePhantom::hkpSimpleShapePhantom(const hkpShape* shape, const hkTransformf& transform, unsigned int collisionFilterInfo)
    : hkpShapePhantom(shape, transform)
{
    m_orderDirty.m_bool = 0;
    m_collidable.m_broadPhaseHandle.m_collisionFilterInfo = collisionFilterInfo;
}

// 0x00DEE280 (hkpsimpleshapephantom.h)
hkpSimpleShapePhantom::hkpSimpleShapePhantom(hkFinishLoadedObjectFlag flag)
    : hkpShapePhantom(flag)
{
}

// 0x00DDFE80
hkpSimpleShapePhantom::~hkpSimpleShapePhantom()
{
}

// 0x00DDFBB0
hkpPhantomType hkpSimpleShapePhantom::getType() const
{
    return HK_PHANTOM_SIMPLE_SHAPE;
}

// 0x00DDFBC0
hkBool hkpSimpleShapePhantom::isOverlappingCollidableAdded(const hkpCollidable* collidable)
{
    hkBool result;
    result.m_bool = 0;
    for (int i = 0; i < m_collisionDetails.getSize(); i++)
    {
        if (m_collisionDetails[i].m_collidable == collidable)
        {
            result.m_bool = 1;
            break;
        }
    }
    return result;
}

// 0x00DDFD90: collidables without a shape are ignored; a listener may reject
void hkpSimpleShapePhantom::addOverlappingCollidable(hkpCollidable* collidable)
{
    if (collidable->m_shape == nullptr)
    {
        return;
    }
    if (fireCollidableAdded(collidable) == HK_COLLIDABLE_ACCEPT)
    {
        CollisionDetail& detail = m_collisionDetails.expandOne();
        detail.m_collidable = collidable;
        m_orderDirty.m_bool = 1;
    }
}

// 0x00DDFD10: searched from the end; the last detail moves into the hole
void hkpSimpleShapePhantom::removeOverlappingCollidable(hkpCollidable* collidable)
{
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
            m_collisionDetails.removeAt(i);
            m_orderDirty.m_bool = 1;
            return;
        }
    }
    wasAdded.m_bool = 0;
    fireCollidableRemoved(collidable, wasAdded);
}

// 0x00DE0610: by broadphase handle id
hkBool hkpSimpleShapePhantom::OrderByUid::operator()(const CollisionDetail& a, const CollisionDetail& b)
{
    hkBool result;
    result.m_bool = a.m_collidable->m_broadPhaseHandle.m_id < b.m_collidable->m_broadPhaseHandle.m_id;
    return result;
}

// 0x00DDFDF0
void hkpSimpleShapePhantom::ensureDeterministicOrder()
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

// 0x00DE02A0
void hkpSimpleShapePhantom::deallocateInternalArrays()
{
    if (m_collisionDetails.getSize() == 0)
    {
        m_collisionDetails.clearAndDeallocate();
    }
    hkpShapePhantom::deallocateInternalArrays();
}

// 0x00DDFEE0: the shape, transform, filter info, listeners and properties (not the overlaps)
hkpPhantom* hkpSimpleShapePhantom::clone() const
{
    hkpSimpleShapePhantom* phantom = new hkpSimpleShapePhantom(m_collidable.m_shape, m_motionState.getTransform(), m_collidable.m_broadPhaseHandle.m_collisionFilterInfo);
    phantom->m_overlapListeners = m_overlapListeners;
    phantom->m_phantomListeners = m_phantomListeners;
    phantom->copyProperties(this);
    return phantom;
}

// 0x00DE0070: the phantom moves to the position, its broadphase box grows to cover the cast (grown by the start
// point tolerance), then every overlapping collidable gets the dispatcher's linear cast
void hkpSimpleShapePhantom::setPositionAndLinearCast(const hkVector4f& position, const hkpLinearCastInput& input, hkpCdPointCollector& castCollector, hkpCdPointCollector* startCollector)
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
    const hkpCollisionDispatcher* dispatcher = m_world->m_collisionDispatcher;
    for (int i = m_collisionDetails.getSize() - 1; i >= 0; i--)
    {
        const hkpCollidable* collB = m_collisionDetails[i].m_collidable;
        dispatcher->getLinearCastFunc(hkSimpleShapePhantom_type(m_collidable), hkSimpleShapePhantom_type(*collB))(m_collidable, *collB, castInput, castCollector, startCollector);
    }
}

// 0x00DE0270: the rotation is set here, the rest by setPositionAndLinearCast
void hkpSimpleShapePhantom::setTransformAndLinearCast(const hkTransformf& transform, const hkpLinearCastInput& input, hkpCdPointCollector& castCollector, hkpCdPointCollector* startCollector)
{
    m_motionState.getTransform().getRotation() = transform.getRotation();
    setPositionAndLinearCast(transform.getTranslation(), input, castCollector, startCollector);
}
