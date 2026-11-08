// Original: volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/dynamics/entity/hkprigidbody.cpp
//
// Havok 2014.2.5 (havok_impl.md HK7): the rigid body (motion creation per motion type, shape, transform and
// motion type changes, clone), from rfg.exe, with hkpGetRigidBody (hkprigidbody.inl).

#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/dynamics/entity/hkprigidbody.h"

#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/math/hkmath.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/memory/router/hkmemoryrouter.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/types/hkufloat8.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/shape/hkpshape.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/dynamics/destruction/breakablebody/hkpbreakablebody.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/util/shapedepth/hkpshapedepthutil.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/dynamics/entity/hkprigidbodycinfo.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/dynamics/entity/util/hkpentitycallbackutil.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/dynamics/motion/rigid/hkpboxmotion.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/dynamics/motion/rigid/hkpcharactermotion.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/dynamics/motion/rigid/hkpfixedrigidmotion.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/dynamics/motion/rigid/hkpspheremotion.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/dynamics/motion/rigid/thinboxmotion/hkpthinboxmotion.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/dynamics/world/hkpworld.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/dynamics/world/simulation/hkpsimulation.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/dynamics/world/util/hkpworldcallbackutil.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/dynamics/world/util/hkpworldoperationutil.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/internal/collide/gjk/hkpgskexact.h"

#include <new>
#include <string.h>

using namespace hkGskExact;

// HK_REAL_MAX as the 2014.2.5 SDK spells it (0x7F7FFFEE, 3.40282e38 rounded down)
static inline float hkRigidBody_realMax()
{
    return lane0(bits(0x7F7FFFEE));
}

// hkHalf from a float, rounded (x * 1.00390625, 0x3F808000, then the high 16 bits)
static inline void hkRigidBody_setHalfRounded(hkHalf& h, float x)
{
    const float rounded = x * 1.00390625f;
    unsigned int b;
    memcpy(&b, &rounded, sizeof(b));
    const short value = (short)((int)b >> 16);
    memcpy(&h, &value, sizeof(value));
}

// hkHalf as a float (the high 16 bits)
static inline float hkRigidBody_halfToFloat(const hkHalf& h)
{
    short value;
    memcpy(&value, &h, sizeof(value));
    const unsigned int b = (unsigned int)(int)value << 16;
    float f;
    memcpy(&f, &b, sizeof(f));
    return f;
}

// 0x00DF12A0 (LTCG: collidable in EAX, extent in ECX): a fifth of the smallest extent below 0.5, else 0.1
static void estimateAllowedPenetrationDepth(hkpCollidable* collidable, const hkVector4f& extent)
{
    const Q e = extent.m_quad;
    const Q smallest = _mm_min_ps(bcast<2>(e), _mm_min_ps(bcast<1>(e), bcast<0>(e)));
    if (lessOrNaN(smallest, hkVector4f::getConstant<HK_QUADREAL_INV_2>().m_quad))
    {
        collidable->m_allowedPenetrationDepth = lane0(mul(hkVector4f::getConstant<HK_QUADREAL_INV_5>().m_quad, smallest));
        return;
    }
    collidable->m_allowedPenetrationDepth = 0.1f; // 0x3DCCCCCD
}

// 0x00DF1270
hkpRigidBody::hkpRigidBody(hkFinishLoadedObjectFlag flag)
    : hkpEntity(flag)
{
}

// 0x00DF1290
hkpRigidBody::~hkpRigidBody()
{
}

// 0x00DF20D0
hkpRigidBody::hkpRigidBody(const hkpRigidBodyCinfo& info)
    : hkpEntity(info.m_shape)
{
    m_material.m_responseType.m_storage = info.m_collisionResponse.m_storage;
    m_contactPointCallbackDelay = info.m_contactPointCallbackDelay;
    m_collidable.m_broadPhaseHandle.m_collisionFilterInfo = info.m_collisionFilterInfo;
    hkLocalFrame* localFrame = info.m_localFrame; // (hkRefPtr assignment)
    if (localFrame != nullptr)
    {
        ((hkReferencedObject*)localFrame)->addReference();
    }
    if (m_localFrame.val() != nullptr)
    {
        ((hkReferencedObject*)m_localFrame.val())->removeReference();
    }
    *(hkLocalFrame**)&m_localFrame = localFrame;

    if (info.m_motionType.m_storage == hkpMotion::MOTION_FIXED)
    {
        hkpMotion* motion = new (&m_motion) hkpFixedRigidMotion(info.m_position, info.m_rotation);
        motion->m_motionState.m_maxLinearVelocity = info.m_maxLinearVelocity;
        motion->m_motionState.m_maxAngularVelocity = info.m_maxAngularVelocity;
        motion->setDeactivationClass(hkpRigidBodyCinfo::SOLVER_DEACTIVATION_OFF);
        m_collidable.m_motion = &m_motion.m_motionState;
        float depth = info.m_allowedPenetrationDepth;
        if (depth <= 0.0f)
        {
            depth = hkRigidBody_realMax(); // fixed bodies never limit the penetration
        }
        m_collidable.m_allowedPenetrationDepth = depth;
    }
    else
    {
        createDynamicRigidMotion((hkpMotion::MotionType)info.m_motionType.m_storage, info.m_position, info.m_rotation, info.m_mass, info.m_inertiaTensor, info.m_centerOfMass, info.m_maxLinearVelocity, info.m_maxAngularVelocity, &m_motion);
        m_motion.setDeactivationClass(info.m_solverDeactivation.m_storage);
        activate();
        m_motion.setLinearVelocity(info.m_linearVelocity);
        activate();
        m_motion.setAngularVelocity(info.m_angularVelocity);
        m_collidable.m_motion = &m_motion.m_motionState;
        m_collidable.m_allowedPenetrationDepth = info.m_allowedPenetrationDepth;
    }
    enableDeactivation(info.m_enableDeactivation.m_bool != 0);
    hkRigidBody_setHalfRounded(m_motion.m_motionState.m_linearDamping, info.m_linearDamping);
    hkRigidBody_setHalfRounded(m_motion.m_motionState.m_angularDamping, info.m_angularDamping);
    hkRigidBody_setHalfRounded(m_motion.m_motionState.m_timeFactor, info.m_timeFactor);
    const hkpShape* shape = m_collidable.m_shape;
    if (shape != nullptr)
    {
        hkVector4f extent;
        updateCachedShapeInfo(shape, extent);
        if (m_collidable.m_allowedPenetrationDepth <= 0.0f)
        {
            estimateAllowedPenetrationDepth(&m_collidable, extent);
        }
    }
    if (info.m_qualityType.m_storage != -1) // HK_COLLIDABLE_QUALITY_INVALID
    {
        m_collidable.m_broadPhaseHandle.m_objectQualityType = info.m_qualityType.m_storage;
    }
    else if (m_motion.m_type.m_storage == hkpMotion::MOTION_FIXED)
    {
        m_collidable.m_broadPhaseHandle.m_objectQualityType = 0; // HK_COLLIDABLE_QUALITY_FIXED
    }
    else
    {
        // HK_COLLIDABLE_QUALITY_KEYFRAMED (1) or HK_COLLIDABLE_QUALITY_DEBRIS (2)
        m_collidable.m_broadPhaseHandle.m_objectQualityType = (signed char)((info.m_motionType.m_storage != hkpMotion::MOTION_KEYFRAMED) + 1);
    }
    m_autoRemoveLevel = info.m_autoRemoveLevel;
    if (info.m_forceCollideOntoPpu.m_bool)
    {
        m_collidable.m_forceCollideOntoPpu |= 1;
    }
    m_material.m_friction = info.m_friction;
    hkRigidBody_setHalfRounded(m_material.m_rollingFrictionMultiplier, info.m_rollingFrictionMultiplier);
    m_material.m_restitution = info.m_restitution;
    hkRigidBody_setHalfRounded(m_motion.m_gravityFactor, info.m_gravityFactor);
    signed char numShapeKeys = info.m_numShapeKeysInContactPointProperties;
    if (numShapeKeys < 0)
    {
        numShapeKeys = (signed char)hkpShapeDepthUtil::getShapeDepth(info.m_shape);
    }
    m_numShapeKeysInContactPointProperties = (unsigned char)numShapeKeys;
    m_responseModifierFlags = info.m_responseModifierFlags;
}

// 0x00DF1EF0: a dynamic motion becomes a sphere motion when the inertia's smallest diagonal element is above 0.8
// times the largest, else a box motion; keyframed motions get 1e6 as their velocity limits
void hkpRigidBody::createDynamicRigidMotion(hkpMotion::MotionType motionType, const hkVector4f& position, const hkQuaternionf& rotation, float mass, const hkMatrix3f& inertiaLocal, const hkVector4f& centreOfMassLocal, float maxLinearVelocity, float maxAngularVelocity, hkpMaxSizeMotion* motionBufferOut)
{
    hkpMotion* motion = nullptr;
    switch (motionType)
    {
    case hkpMotion::MOTION_DYNAMIC:
    {
        Q diag = selectQ(laneMask(2), inertiaLocal.getColumn<1>().m_quad, inertiaLocal.getColumn<0>().m_quad);
        diag = selectQ(laneMask(4), inertiaLocal.getColumn<2>().m_quad, diag);
        const Q largest = _mm_max_ps(bcast<2>(diag), _mm_max_ps(bcast<1>(diag), bcast<0>(diag)));
        const Q smallest = _mm_min_ps(bcast<2>(diag), _mm_min_ps(bcast<1>(diag), bcast<0>(diag)));
        const Q limit = mul(largest, splat(0.8f)); // 0x3F4CCCCD
        if (lane0(smallest) > lane0(limit)) // jbe: equal and NaN go to the box motion
        {
            if (motionBufferOut != nullptr)
            {
                motion = new (motionBufferOut) hkpSphereMotion(position, rotation);
            }
        }
        else if (motionBufferOut != nullptr)
        {
            motion = new (motionBufferOut) hkpBoxMotion(position, rotation);
        }
        break;
    }
    case hkpMotion::MOTION_SPHERE_INERTIA:
        if (motionBufferOut != nullptr)
        {
            motion = new (motionBufferOut) hkpSphereMotion(position, rotation);
        }
        break;
    case hkpMotion::MOTION_BOX_INERTIA:
        if (motionBufferOut != nullptr)
        {
            motion = new (motionBufferOut) hkpBoxMotion(position, rotation);
        }
        break;
    case hkpMotion::MOTION_KEYFRAMED:
        maxLinearVelocity = 1000000.0f; // 0x49742400
        maxAngularVelocity = 1000000.0f;
        if (motionBufferOut != nullptr)
        {
            motion = new (motionBufferOut) hkpKeyframedRigidMotion(position, rotation);
        }
        break;
    case hkpMotion::MOTION_THIN_BOX_INERTIA:
        if (motionBufferOut != nullptr)
        {
            motion = new (motionBufferOut) hkpThinBoxMotion(position, rotation);
        }
        break;
    case hkpMotion::MOTION_CHARACTER:
        if (motionBufferOut != nullptr)
        {
            motion = new (motionBufferOut) hkpCharacterMotion(position, rotation);
        }
        break;
    default: // MOTION_FIXED, MOTION_INVALID and out of range
        if (motionBufferOut != nullptr)
        {
            motion = new (motionBufferOut) hkpFixedRigidMotion(position, rotation);
        }
        break;
    }
    if (motionType != hkpMotion::MOTION_KEYFRAMED)
    {
        motion->setInertiaLocal(inertiaLocal);
        motion->setCenterOfMassInLocal(centreOfMassLocal);
        motion->setMass(mass);
    }
    motion->m_motionState.m_maxLinearVelocity = maxLinearVelocity;
    motion->m_motionState.m_maxAngularVelocity = maxAngularVelocity;
    motion->m_savedQualityTypeIndex = 0;
}

// 0x00DF12F0
hkMotionState* hkpRigidBody::getMotionState()
{
    return &m_motion.m_motionState;
}

// 0x00DF1300: queued (RIGIDBODY_SET_MOTION_TYPE) while the world's critical operations are locked
void hkpRigidBody::setMotionType(hkpMotion::MotionType newState, hkpEntityActivation preferredActivationState, hkpUpdateCollisionFilterOnEntityMode collisionFilterUpdateMode)
{
    if (m_world != nullptr && m_world->areCriticalOperationsLocked())
    {
        hkWorldOperation::SetRigidBodyMotionType op;
        op.m_rigidBody = this;
        op.m_motionType.m_storage = (unsigned char)newState;
        op.m_activation.m_storage = (unsigned char)preferredActivationState;
        op.m_collisionFilterUpdateMode.m_storage = (unsigned char)collisionFilterUpdateMode;
        m_world->queueOperation(op);
        return;
    }
    hkpWorldOperationUtil::setRigidBodyMotionType(this, newState, preferredActivationState, collisionFilterUpdateMode);
    if (m_world != nullptr)
    {
        hkpWorldCallbackUtil::fireEntitySetMotionType(m_world, this);
    }
    hkpEntityCallbackUtil::fireEntitySetMotionType(this);
}

// 0x00DF1390
bool hkpRigidBody::isDeactivationEnabled() const
{
    return m_motion.m_deactivationIntegrateCounter != 0xFF;
}

// 0x00DF13A0, 0x00DF13C0
void hkpRigidBody::setMass(float m)
{
    m_motion.setMass(m);
}

void hkpRigidBody::setMassInv(float mInv)
{
    m_motion.setMassInv(mInv);
}

// 0x00DF1420, 0x00DF1440, 0x00DF2740
void hkpRigidBody::setInertiaLocal(const hkMatrix3f& inertia)
{
    m_motion.setInertiaLocal(inertia);
}

void hkpRigidBody::setInertiaInvLocal(const hkMatrix3f& inertiaInv)
{
    m_motion.setInertiaInvLocal(inertiaInv);
}

void hkpRigidBody::getInertiaLocal(hkMatrix3f& inertia) const
{
    m_motion.getInertiaLocal(inertia);
}

// 0x00DF1460: the extent of the shape's local box (returned) and the motion's object radius: the distance from the
// center of mass to the farther box corner (23-bit square root)
void hkpRigidBody::updateCachedShapeInfo(const hkpShape* shape, hkVector4f& extentOut)
{
    hkAabb aabb;
    shape->getAabb(*(const hkTransformf*)&hkVector4f::getConstant<HK_QUADREAL_1000>(), 0.0f, aabb); // identity (four consecutive constants)
    extentOut.m_quad = sub(aabb.m_max.m_quad, aabb.m_min.m_quad);
    const Q com = m_motion.m_motionState.getSweptTransform().m_centerOfMassLocal.m_quad;
    const Q dMin = sub(aabb.m_min.m_quad, com);
    const Q dMax = sub(aabb.m_max.m_quad, com);
    const Q lenMin = hadd3(mul(dMin, dMin));
    const Q lenMax = hadd3(mul(dMax, dMax));
    const Q len2 = _mm_max_ps(lenMax, lenMin);
    const Q le0 = _mm_cmple_ps(len2, _mm_setzero_ps());
    const Q r = _mm_rsqrt_ps(len2);
    const Q t = mul(mul(sub(splat(3.0f), mul(mul(r, len2), r)), mul(r, splat(0.5f))), len2);
    m_motion.m_motionState.m_objectRadius = lane0(_mm_andnot_ps(le0, t));
}

// 0x00DF1550
void hkpRigidBody::getCinfo(hkpRigidBodyCinfo& info) const
{
    info.m_forceCollideOntoPpu.m_bool = m_collidable.m_forceCollideOntoPpu & 1;
    info.m_autoRemoveLevel = m_autoRemoveLevel;
    info.m_numShapeKeysInContactPointProperties = (signed char)m_numShapeKeysInContactPointProperties;
    info.m_contactPointCallbackDelay = m_contactPointCallbackDelay;
    info.m_enableDeactivation.m_bool = isDeactivationEnabled();
    info.m_localFrame = m_localFrame.val();
    info.m_friction = m_material.m_friction;
    info.m_rollingFrictionMultiplier = hkRigidBody_halfToFloat(m_material.m_rollingFrictionMultiplier);
    info.m_collisionResponse.m_storage = m_material.m_responseType.m_storage;
    info.m_restitution = m_material.m_restitution;
    info.m_linearDamping = hkRigidBody_halfToFloat(m_motion.m_motionState.m_linearDamping);
    info.m_angularDamping = hkRigidBody_halfToFloat(m_motion.m_motionState.m_angularDamping);
    info.m_linearVelocity = m_motion.m_linearVelocity;
    info.m_angularVelocity = m_motion.m_angularVelocity;
    info.m_mass = m_motion.getMass();
    m_motion.getInertiaLocal(info.m_inertiaTensor);
    info.m_motionType.m_storage = (signed char)m_motion.m_type.m_storage;
    info.m_solverDeactivation.m_storage = (signed char)m_motion.m_motionState.m_deactivationClass;
    info.m_maxLinearVelocity = m_motion.m_motionState.m_maxLinearVelocity;
    info.m_maxAngularVelocity = m_motion.m_motionState.m_maxAngularVelocity;
    info.m_position = m_motion.m_motionState.getTransform().getTranslation();
    info.m_rotation = m_motion.m_motionState.getSweptTransform().m_rotation1;
    info.m_centerOfMass = m_motion.m_motionState.getSweptTransform().m_centerOfMassLocal;
    info.m_shape = m_collidable.m_shape;
    info.m_collisionFilterInfo = m_collidable.m_broadPhaseHandle.m_collisionFilterInfo;
    info.m_allowedPenetrationDepth = m_collidable.m_allowedPenetrationDepth;
    info.m_qualityType.m_storage = m_collidable.m_broadPhaseHandle.m_objectQualityType;
    info.m_gravityFactor = hkRigidBody_halfToFloat(m_motion.m_gravityFactor);
}

// 0x00DF1750: queued (WORLD_OBJECT_SET_SHAPE) while the world's critical operations are locked; in the world the
// body leaves the broadphase, gets the new shape (reference, cached shape info, penetration depth, child box
// caches) and goes back in
hkWorldOperation::Result hkpRigidBody::setShape(const hkpShape* shape)
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
        hkpWorldOperationUtil::removeEntityBP(m_world, this);
    }
    const hkpShape* oldShape = m_collidable.m_shape;
    m_collidable.m_shape = shape;
    shape->addReference();
    if (oldShape != nullptr)
    {
        oldShape->removeReference();
    }
    hkVector4f extent;
    updateCachedShapeInfo(shape, extent);
    if (oldShape != nullptr && m_collidable.m_allowedPenetrationDepth != hkRigidBody_realMax())
    {
        m_collidable.m_allowedPenetrationDepth = -1.0f; // estimated again for the new shape
    }
    if (m_collidable.m_allowedPenetrationDepth <= 0.0f)
    {
        estimateAllowedPenetrationDepth(&m_collidable, extent);
    }
    if (m_world != nullptr)
    {
        setCachedShapeData(m_world, shape);
    }
    if (m_world != nullptr)
    {
        hkpWorldCallbackUtil::fireEntityShapeSet(m_world, this);
    }
    hkpEntityCallbackUtil::fireEntityShapeSet(this);
    if (m_world != nullptr)
    {
        hkpWorldOperationUtil::addEntityBP(m_world, this);
        m_world->unlockAndAttemptToExecutePendingOperations();
    }
    return hkWorldOperation::DONE;
}

// 0x00DF18C0: queued (WORLD_OBJECT_UPDATE_SHAPE) while locked; the modifier changes the shape in place, the child
// box caches are invalidated, and a fixed body gets new collision information and broadphase box
hkWorldOperation::Result hkpRigidBody::updateShape(hkpShapeModifier* shapeModifier)
{
    hkpWorld* world = m_world;
    if (world != nullptr)
    {
        if (world->areCriticalOperationsLocked())
        {
            hkWorldOperation::UpdateWorldObjectShape op;
            op.m_worldObject = this;
            op.m_shapeModifier = shapeModifier;
            world->queueOperation(op);
            return hkWorldOperation::POSTPONED;
        }
        world->lockCriticalOperations();
    }
    const hkpShape* shape = m_collidable.m_shape;
    if (shapeModifier != nullptr)
    {
        shapeModifier->modifyShape(const_cast<hkpShape*>(shape));
    }
    hkVector4f extent;
    updateCachedShapeInfo(shape, extent);
    if (m_world != nullptr)
    {
        m_collidable.m_boundingVolumeData.invalidate();
        if (m_motion.m_type.m_storage == hkpMotion::MOTION_FIXED)
        {
            hkpEntity* self = this;
            m_world->m_simulation->resetCollisionInformationForEntities(&self, 1, m_world, hkpSimulation::RESET_ALL);
            hkpWorldOperationUtil::updateEntityBP(m_world, this);
        }
        m_world->unlockAndAttemptToExecutePendingOperations();
        hkpWorldCallbackUtil::fireEntityShapeSet(m_world, this);
    }
    hkpEntityCallbackUtil::fireEntityShapeSet(this);
    return hkWorldOperation::DONE;
}

// 0x00DF19E0
void hkpRigidBody::setCenterOfMassLocal(const hkVector4f& centerOfMass)
{
    m_motion.setCenterOfMassInLocal(centerOfMass);
    hkVector4f extent;
    updateCachedShapeInfo(m_collidable.m_shape, extent);
}

// 0x00DF1A20: outside a world the select flags are 0
void hkpRigidBody::enableDeactivation(bool _enableDeactivation)
{
    if (!_enableDeactivation)
    {
        m_motion.m_deactivationIntegrateCounter = 0xFF;
        m_motion.m_deactivationNumInactiveFrames[0] = 0;
        m_motion.m_deactivationNumInactiveFrames[1] = 0;
        return;
    }
    if (isDeactivationEnabled())
    {
        return;
    }
    if (m_world == nullptr)
    {
        m_motion.enableDeactivation(true, (int)m_uid, 0, 0, 0);
        return;
    }
    const hkpSolverInfo& solverInfo = m_world->m_dynamicsStepInfo.m_solverInfo;
    m_motion.enableDeactivation(true, (int)m_uid, solverInfo.m_deactivationNumInactiveFramesSelectFlag[0], solverInfo.m_deactivationNumInactiveFramesSelectFlag[1], solverInfo.m_deactivationIntegrateCounter);
}

// 0x00DF1AF0, 0x00DF1B10: the dynamic motion a keyframed or fixed body keeps for when it becomes dynamic again
hkpMotion* hkpRigidBody::getStoredDynamicMotion()
{
    const unsigned char type = m_motion.m_type.m_storage;
    if (type != hkpMotion::MOTION_KEYFRAMED && type != hkpMotion::MOTION_FIXED)
    {
        return nullptr;
    }
    return m_motion.m_savedMotion;
}

const hkpMotion* hkpRigidBody::getStoredDynamicMotion() const
{
    const unsigned char type = m_motion.m_type.m_storage;
    if (type != hkpMotion::MOTION_KEYFRAMED && type != hkpMotion::MOTION_FIXED)
    {
        return nullptr;
    }
    return m_motion.m_savedMotion;
}

// 0x00DF1B30: queued (UPDATE_MOVED_BODY_INFO) while locked; the collision information is reset, the broadphase box
// recomputed, and an inactive body is activated (if the world wants that and the body is not fixed) or reported as
// moved
void hkpRigidBody::updateBroadphaseAndResetCollisionInformationOfWarpedBody(hkpEntity* entity)
{
    hkpWorld* world = entity->m_world;
    if (world == nullptr)
    {
        return;
    }
    if (world->areCriticalOperationsLocked())
    {
        hkWorldOperation::UpdateMovedBodyInfo op;
        op.m_entity = entity;
        world->queueOperation(op);
        return;
    }
    world->lockCriticalOperations();
    hkpEntity* entities = entity;
    world->m_simulation->resetCollisionInformationForEntities(&entities, 1, world, hkpSimulation::RESET_ALL);
    if (entity->m_collidable.m_shape != nullptr)
    {
        hkpSimulation::collideEntitiesBroadPhaseDiscrete(&entities, 1, world);
    }
    if (!entity->isActive().m_bool)
    {
        if (world->m_shouldActivateOnRigidBodyTransformChange.m_bool && entity->m_motion.m_type.m_storage != hkpMotion::MOTION_FIXED)
        {
            entity->activate();
        }
        hkpWorldCallbackUtil::fireInactiveEntityMoved(world, entity);
    }
    world->unlockAndAttemptToExecutePendingOperations();
}

// 0x00DF1C20, 0x00DF1C50, 0x00DF1C80, 0x00DF1CB0
void hkpRigidBody::setPosition(const hkVector4f& position)
{
    m_motion.setPosition(position);
    updateBroadphaseAndResetCollisionInformationOfWarpedBody(this);
}

void hkpRigidBody::setRotation(const hkQuaternionf& rotation)
{
    m_motion.setRotation(rotation);
    updateBroadphaseAndResetCollisionInformationOfWarpedBody(this);
}

void hkpRigidBody::setPositionAndRotation(const hkVector4f& position, const hkQuaternionf& rotation)
{
    m_motion.setPositionAndRotation(position, rotation);
    updateBroadphaseAndResetCollisionInformationOfWarpedBody(this);
}

void hkpRigidBody::setTransform(const hkTransformf& transform)
{
    m_motion.setTransform(transform);
    updateBroadphaseAndResetCollisionInformationOfWarpedBody(this);
}

// 0x00DF2590: a new body from this one's cinfo, then the motion copied over (with the stored dynamic motion, which
// keeps its own reference count), the properties, name, user data and breakable body
hkpRigidBody* hkpRigidBody::clone() const
{
    hkpRigidBodyCinfo info;
    getCinfo(info);
    hkpRigidBody* body = new hkpRigidBody(info);
    memcpy((void*)&body->m_motion, (const void*)&m_motion, sizeof(hkpMaxSizeMotion));
    if (m_motion.m_savedMotion != nullptr)
    {
        hkpMaxSizeMotion* saved = new hkpMaxSizeMotion();
        body->m_motion.m_savedMotion = saved;
        unsigned int memSizeAndRefCount; // hkReferencedObject::m_memSizeAndRefCount (+4)
        memcpy(&memSizeAndRefCount, (char*)saved + 4, sizeof(memSizeAndRefCount));
        memcpy((void*)saved, (const void*)m_motion.m_savedMotion, sizeof(hkpMaxSizeMotion));
        memcpy((char*)saved + 4, &memSizeAndRefCount, sizeof(memSizeAndRefCount)); // (two atomic 16-bit writes in the original)
    }
    body->m_collidable.m_motion = body->getMotionState();
    body->copyProperties(this);
    body->m_name = m_name.cString();
    body->m_userData = m_userData;
    if (m_breakableBody != nullptr)
    {
        body->m_breakableBody = m_breakableBody->cloneBreakableBody(body);
    }
    return body;
}

// 0x00710FE0 (hkprigidbody.inl): the owner of an entity's collidable
hkpRigidBody* hkpGetRigidBody(const hkpCollidable* collidable)
{
    if (collidable->m_broadPhaseHandle.m_type == hkpWorldObject::BROAD_PHASE_ENTITY)
    {
        return static_cast<hkpRigidBody*>((hkpEntity*)collidable->getOwner());
    }
    return nullptr;
}

// 0x00DF4F00
hkpRigidBodyCinfo::hkpRigidBodyCinfo()
{
    m_position.setZero();
    m_rotation.m_vec = hkVector4f::getConstant<HK_QUADREAL_0001>();
    m_linearVelocity.setZero();
    m_angularVelocity.setZero();
    // the identity scaled by 1 (diagonal 1 * HK_QUADREAL_1000/0100/0010)
    m_inertiaTensor.getColumn<0>().m_quad = mul(hkVector4f::getConstant<HK_QUADREAL_1000>().m_quad, hkVector4f::getConstant<HK_QUADREAL_1>().m_quad);
    m_inertiaTensor.getColumn<1>().m_quad = mul(hkVector4f::getConstant<HK_QUADREAL_0100>().m_quad, hkVector4f::getConstant<HK_QUADREAL_1>().m_quad);
    m_inertiaTensor.getColumn<2>().m_quad = mul(hkVector4f::getConstant<HK_QUADREAL_0010>().m_quad, hkVector4f::getConstant<HK_QUADREAL_1>().m_quad);
    m_centerOfMass.setZero();
    m_linearDamping = 0.0f;
    m_rollingFrictionMultiplier = 0.0f;
    m_collisionResponse.m_storage = hkpMaterial::RESPONSE_SIMPLE_CONTACT;
    m_mass = 1.0f;
    m_angularDamping = 0.05f;
    m_gravityFactor = 1.0f;
    m_restitution = 0.4f;
    m_timeFactor = 1.0f;
    m_friction = 0.5f;
    m_maxLinearVelocity = 200.0f;
    m_maxAngularVelocity = 200.0f;
    m_motionType.m_storage = hkpMotion::MOTION_DYNAMIC;
    m_enableDeactivation.m_bool = 1;
    m_solverDeactivation.m_storage = SOLVER_DEACTIVATION_LOW;
    m_collisionFilterInfo = 0;
    m_shape = nullptr;
    m_localFrame = nullptr;
    m_allowedPenetrationDepth = -1.0f;
    m_qualityType.m_storage = -1; // HK_COLLIDABLE_QUALITY_INVALID
    m_autoRemoveLevel = 0;
    m_responseModifierFlags = 0;
    m_numShapeKeysInContactPointProperties = 0;
    m_contactPointCallbackDelay = 0xFFFF;
    m_forceCollideOntoPpu.m_bool = 0;
}

// The out-of-line copies of hkprigidbody.inl: forwarders to the motion (setters and impulses activate the body
// first).

// 0x0080D0C0
hkpMotion* hkpRigidBody::getRigidMotion() const
{
    return const_cast<hkpMaxSizeMotion*>(&m_motion);
}

// hkprigidbody.inl (inlined everywhere in rfg.exe: the byte at +0xE8)
hkpMotion::MotionType hkpRigidBody::getMotionType() const
{
    return static_cast<hkpMotion::MotionType>(m_motion.m_type.m_storage);
}

// 0x0080D0D0, 0x00B712A0
float hkpRigidBody::getMass() const
{
    return getRigidMotion()->getMass();
}

float hkpRigidBody::getMassInv() const
{
    return getRigidMotion()->getMassInv().getReal();
}

// 0x00B62EE0, 0x0080D0E0
const hkVector4f& hkpRigidBody::getCenterOfMassLocal() const
{
    return getRigidMotion()->getCenterOfMassLocal();
}

const hkVector4f& hkpRigidBody::getCenterOfMassInWorld() const
{
    return getRigidMotion()->getCenterOfMassInWorld();
}

// 0x0080D0F0, 0x0080D100, 0x0080D110
const hkVector4f& hkpRigidBody::getPosition() const
{
    return getRigidMotion()->getPosition();
}

const hkQuaternionf& hkpRigidBody::getRotation() const
{
    return getRigidMotion()->getRotation();
}

const hkTransformf& hkpRigidBody::getTransform() const
{
    return getRigidMotion()->getTransform();
}

// 0x0080D120, 0x0080D220
const hkVector4f& hkpRigidBody::getLinearVelocity() const
{
    return getRigidMotion()->getLinearVelocity();
}

void hkpRigidBody::setLinearVelocity(const hkVector4f& newVel)
{
    activate();
    getRigidMotion()->setLinearVelocity(newVel);
}

// 0x0080D130, 0x0080D240
const hkVector4f& hkpRigidBody::getAngularVelocity() const
{
    return getRigidMotion()->getAngularVelocity();
}

void hkpRigidBody::setAngularVelocity(const hkVector4f& newVel)
{
    activate();
    getRigidMotion()->setAngularVelocity(newVel);
}

// 0x0080D140
void hkpRigidBody::getPointVelocity(const hkVector4f& p, hkVector4f& vecOut) const
{
    getRigidMotion()->getPointVelocity(p, vecOut);
}

// 0x0080D190, 0x0080D1B0, 0x0080D1D0
void hkpRigidBody::applyLinearImpulse(const hkVector4f& imp)
{
    activate();
    getRigidMotion()->applyLinearImpulse(imp);
}

void hkpRigidBody::applyPointImpulse(const hkVector4f& imp, const hkVector4f& p)
{
    activate();
    getRigidMotion()->applyPointImpulse(imp, p);
}

void hkpRigidBody::applyAngularImpulse(const hkVector4f& imp)
{
    activate();
    getRigidMotion()->applyAngularImpulse(imp);
}

// 0x0080D1F0
void hkpRigidBody::applyForce(const float deltaTime, const hkVector4f& force)
{
    activate();
    getRigidMotion()->applyForce(deltaTime, force);
}

// 0x0080D260, 0x0080D270
unsigned int hkpRigidBody::getCollisionFilterInfo() const
{
    return m_collidable.m_broadPhaseHandle.m_collisionFilterInfo;
}

void hkpRigidBody::setCollisionFilterInfo(unsigned int info)
{
    m_collidable.m_broadPhaseHandle.m_collisionFilterInfo = info;
}

// 0x00DF13E0
void hkpRigidBody::setFriction(float newFriction)
{
    m_material.m_friction = newFriction;
}

// 0x008146D0 (hkprigidbody.inl:253)
void hkpRigidBody::setLinearDamping(float d)
{
    getMotion()->m_motionState.m_linearDamping = d;
}

// 0x00814700 (hkprigidbody.inl:260)
void hkpRigidBody::setAngularDamping(float d)
{
    getMotion()->m_motionState.m_angularDamping = d;
}
