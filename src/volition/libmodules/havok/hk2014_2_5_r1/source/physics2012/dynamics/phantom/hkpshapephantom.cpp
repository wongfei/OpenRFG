// Original: volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/dynamics/phantom/hkpshapephantom.cpp
//
// Havok 2014.2.5 (havok_impl.md HK7): the phantom with a shape and a transform, from rfg.exe.

#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/dynamics/phantom/hkpshapephantom.h"

#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/math/hkmath.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/agent/hkpprocesscollisioninput.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/shape/hkpshape.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/dynamics/world/hkpworld.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/dynamics/world/util/hkpworldcallbackutil.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/dynamics/world/util/hkpworldoperationutil.h"

// 0x00DF4E20: the motion state starts at the transform (rotation as a quaternion)
hkpShapePhantom::hkpShapePhantom(const hkpShape* shape, const hkTransformf& transform)
    : hkpPhantom(shape)
{
    hkQuaternionf rotation;
    rotation.set(transform.getRotation());
    m_motionState.initMotionState(transform.getTranslation(), rotation);
    m_collidable.m_motion = &m_motionState;
}

// 0x00DEE1A0
hkpShapePhantom::hkpShapePhantom(hkFinishLoadedObjectFlag flag)
    : hkpPhantom(flag)
{
}

// 0x00DDFB60
hkpShapePhantom::~hkpShapePhantom()
{
}

// 0x00DF4BC0
hkMotionState* hkpShapePhantom::getMotionState()
{
    return &m_motionState;
}

// 0x00DF4C50: grown by half the world's collision tolerance (none outside a world)
void hkpShapePhantom::calcAabb(hkAabb& aabb)
{
    const float tolerance = m_world != nullptr ? m_world->m_collisionInput->m_tolerance.val() * 0.5f : 0.0f;
    m_collidable.m_shape->getAabb(m_motionState.getTransform(), tolerance, aabb);
}

// 0x00DF4BE0: only the translation changes; in a world the broadphase box follows (grown by the extra tolerance)
void hkpShapePhantom::setPosition(const hkVector4f& position, float extraTolerance)
{
    m_motionState.getTransform().setTranslation(position);
    if (m_world != nullptr)
    {
        hkAabb aabb;
        m_collidable.m_shape->getAabb(m_motionState.getTransform(), m_world->m_collisionInput->m_tolerance.val() * 0.5f + extraTolerance, aabb);
        updateBroadPhase(aabb);
    }
}

// 0x00DF4DA0
void hkpShapePhantom::setTransform(const hkTransformf& transform)
{
    m_motionState.getTransform() = transform;
    if (m_world != nullptr)
    {
        hkAabb aabb;
        m_collidable.m_shape->getAabb(transform, m_world->m_collisionInput->m_tolerance.val() * 0.5f, aabb);
        updateBroadPhase(aabb);
    }
}

// 0x00DF4C90: queued (WORLD_OBJECT_SET_SHAPE) while the world's critical operations are locked; in the world the
// phantom leaves the broadphase and goes back in with the new shape
hkWorldOperation::Result hkpShapePhantom::setShape(const hkpShape* shape)
{
    hkpWorld* world = m_world;
    if (world != nullptr)
    {
        if (world->areCriticalOperationsLocked())
        {
            hkWorldOperation::SetWorldObjectShape op;
            op.m_worldObject = this;
            op.m_shape = shape;
            world->queueOperation(op);
            return hkWorldOperation::POSTPONED;
        }
        world->lockCriticalOperations();
        hkpWorldOperationUtil::removePhantomBP(m_world, this);
    }
    shape->addReference();
    if (m_collidable.m_shape != nullptr)
    {
        m_collidable.m_shape->removeReference();
    }
    m_collidable.m_shape = shape;
    if (m_world != nullptr)
    {
        hkpWorldCallbackUtil::firePhantomShapeSet(m_world, this);
    }
    firePhantomShapeSet();
    if (m_world != nullptr)
    {
        hkpWorldOperationUtil::addPhantomBP(m_world, this);
        m_world->unlockAndAttemptToExecutePendingOperations();
    }
    return hkWorldOperation::DONE;
}

// 0x00DF4BD0
void hkpShapePhantom::deallocateInternalArrays()
{
    hkpPhantom::deallocateInternalArrays();
}
