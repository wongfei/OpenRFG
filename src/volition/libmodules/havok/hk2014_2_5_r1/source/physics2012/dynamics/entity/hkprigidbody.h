// Original: volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/dynamics/entity/hkprigidbody.h
//
// Havok 2014.2.5: full declaration from rfg.pdb (havok_impl.md HK0).
#pragma once

#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/memory/memoryclasses/hkmemoryclasses.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/dynamics/world/util/hkpworldoperationqueue.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/agent/collidable/hkpcollidablequalitytype.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/dynamics/entity/hkpentity.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/dynamics/motion/hkpmotion.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/dynamics/world/hkpworld.h"

class hkBool;
class hkClass;
class hkFinishLoadedObjectFlag;
struct hkInternalClassMember;
class hkMatrix3f;
class hkMotionState;
class hkQuaternionf;
class hkTransformf;
class hkVector4f;
class hkpCollidable;
class hkpMaxSizeMotion;
class hkpRigidBodyCinfo;
class hkpShape;
class hkpShapeModifier;

// size 0x220
class hkpRigidBody : public hkpEntity
{
public:
    HK_DECLARE_CLASS_ALLOCATOR(); // new(unsigned int, void* p) 0x00DEF4A0, new(unsigned int nbytes) 0x00814630, delete(void* p, unsigned int nbytes) 0x00814650
    static const hkClass& staticClass(); // 0x00DEF4B0
    hkpRigidBody(hkFinishLoadedObjectFlag flag); // 0x00DF1270
    hkpRigidBody(const hkpRigidBodyCinfo& info); // 0x00DF20D0
    virtual ~hkpRigidBody(); // vtbl+0x00 0x00DF1290
    void getCinfo(hkpRigidBodyCinfo& info) const; // 0x00DF1550
    float getMass() const; // 0x0080D0D0 (hkprigidbody.inl:39)
    float getMassInv() const; // 0x00B712A0 (hkprigidbody.inl:44)
    void setMass(float m); // 0x00DF13A0
    void setMassInv(float mInv); // 0x00DF13C0
    void getInertiaLocal(hkMatrix3f& inertia) const; // 0x00DF2740
    void getInertiaWorld(hkMatrix3f&) const;
    void setInertiaLocal(const hkMatrix3f& inertia); // 0x00DF1420
    void setInertiaInvLocal(const hkMatrix3f& inertiaInv); // 0x00DF1440
    void getInertiaInvLocal(hkMatrix3f& inertiaInv) const; // 0x00F2AEF0
    void getInertiaInvWorld(hkMatrix3f& inertiaInv) const; // 0x00F1C830
    void setCenterOfMassLocal(const hkVector4f& centerOfMass); // 0x00DF19E0
    const hkVector4f& getCenterOfMassLocal() const; // 0x00B62EE0 (hkprigidbody.inl:76)
    const hkVector4f& getCenterOfMassInWorld() const; // 0x0080D0E0 (hkprigidbody.inl:82)
    const hkVector4f& getPosition() const; // 0x0080D0F0 (hkprigidbody.inl:95)
    void setPosition(const hkVector4f& position); // 0x00DF1C20
    const hkQuaternionf& getRotation() const; // 0x0080D100 (hkprigidbody.inl:101)
    void setRotation(const hkQuaternionf& rotation); // 0x00DF1C50
    void setPositionAndRotation(const hkVector4f& position, const hkQuaternionf& rotation); // 0x00DF1C80
    const hkTransformf& getTransform() const; // 0x0080D110 (hkprigidbody.inl:107)
    void setTransform(const hkTransformf& transform); // 0x00DF1CB0
    void setPositionAndRotationAsCriticalOperation(const hkVector4f& position, const hkQuaternionf& rotation); // 0x00DF1D10
    void approxTransformAt(float, hkTransformf&) const;
    void approxCurrentTransform(hkTransformf&) const;
    const hkVector4f& getLinearVelocity() const; // 0x0080D120 (hkprigidbody.inl:132)
    void setLinearVelocity(const hkVector4f& newVel); // 0x0080D220 (hkprigidbody.inl:324)
    const hkVector4f& getAngularVelocity() const; // 0x0080D130 (hkprigidbody.inl:139)
    void setAngularVelocity(const hkVector4f& newVel); // 0x0080D240 (hkprigidbody.inl:334)
    void getPointVelocity(const hkVector4f& p, hkVector4f& vecOut) const; // 0x0080D140 (hkprigidbody.inl:146)
    void setLinearVelocityAsCriticalOperation(const hkVector4f& newVel); // 0x00DF23D0
    void setAngularVelocityAsCriticalOperation(const hkVector4f& newVel); // 0x00DF2440
    void applyLinearImpulse(const hkVector4f& imp); // 0x0080D190 (hkprigidbody.inl:158)
    void applyPointImpulse(const hkVector4f& imp, const hkVector4f& p); // 0x0080D1B0 (hkprigidbody.inl:168)
    void applyAngularImpulse(const hkVector4f& imp); // 0x0080D1D0 (hkprigidbody.inl:180)
    void applyLinearImpulseAsCriticalOperation(const hkVector4f& imp); // 0x00DF24B0
    void applyPointImpulseAsCriticalOperation(const hkVector4f& imp, const hkVector4f& p); // 0x00DF1D90
    void applyAngularImpulseAsCriticalOperation(const hkVector4f& imp); // 0x00DF2520
    void applyForce(const float deltaTime, const hkVector4f& force, const hkVector4f& p); // 0x00F243A0
    void applyForce(const float deltaTime, const hkVector4f& force); // 0x0080D1F0 (hkprigidbody.inl:194)
    void applyTorque(const float deltaTime, const hkVector4f& torque); // 0x00F281D0
    float getLinearDamping() const; // 0x00814690 (hkprigidbody.inl:237)
    void setLinearDamping(float d); // 0x008146D0 (hkprigidbody.inl:253)
    float getAngularDamping() const; // 0x008146B0 (hkprigidbody.inl:242)
    void setAngularDamping(float d); // 0x00814700 (hkprigidbody.inl:260)
    float getTimeFactor() const;
    void setTimeFactor(float f); // 0x00DF2DF0
    float getGravityFactor() const;
    void setGravityFactor(float);
    float getMaxLinearVelocity() const; // 0x00DF2A40
    void setMaxLinearVelocity(float);
    float getMaxAngularVelocity() const; // 0x00DF2A90
    void setMaxAngularVelocity(float);
    void enableDeactivation(bool _enableDeactivation); // 0x00DF1A20
    bool isDeactivationEnabled() const; // 0x00DF1390
    unsigned int getCollisionFilterInfo() const; // 0x0080D260 (hkprigidbody.inl:343)
    void setCollisionFilterInfo(unsigned int info); // 0x0080D270 (hkprigidbody.inl:349)
    hkpCollidableQualityType getQualityType() const;
    void setQualityType(hkpCollidableQualityType);
    float getAllowedPenetrationDepth() const;
    void setAllowedPenetrationDepth(float);
    void setMotionType(hkpMotion::MotionType newState, hkpEntityActivation preferredActivationState, hkpUpdateCollisionFilterOnEntityMode collisionFilterUpdateMode); // 0x00DF1300
    hkpMotion::MotionType getMotionType() const;
    const hkpMotion* getStoredDynamicMotion() const; // 0x00DF1B10
    hkpMotion* getStoredDynamicMotion(); // 0x00DF1AF0
    void updateCachedShapeInfo(const hkpShape* shape, hkVector4f& extentOut); // 0x00DF1460
    float getFriction() const;
    float getRollingFrictionMultiplier() const;
    float getRestitution() const;
    void setFriction(float newFriction); // 0x00DF13E0
    void setRollingFrictionMultiplier(float multiplier); // 0x00DF1CE0
    void setRestitution(float newRestitution); // 0x00DF1400
    virtual hkWorldOperation::Result setShape(const hkpShape* shape); // vtbl+0x10 0x00DF1750
    virtual hkWorldOperation::Result updateShape(hkpShapeModifier* shapeModifier); // vtbl+0x14 0x00DF18C0
    virtual hkpRigidBody* clone() const; // vtbl+0x20 0x00DF2590
    hkpMotion* getRigidMotion() const; // 0x0080D0C0 (hkprigidbody.inl:33)
    hkBool checkPerformance() const; // 0x00DF1E20
    static void createDynamicRigidMotion(hkpMotion::MotionType motionType, const hkVector4f& position, const hkQuaternionf& rotation, float mass, const hkMatrix3f& inertiaLocal, const hkVector4f& centreOfMassLocal, float maxLinearVelocity, float maxAngularVelocity, hkpMaxSizeMotion* motionBufferOut); // 0x00DF1EF0
    static void updateBroadphaseAndResetCollisionInformationOfWarpedBody(hkpEntity* entity); // 0x00DF1B30
public:
    virtual hkMotionState* getMotionState(); // vtbl+0x18 0x00DF12F0

public:
    static const hkInternalClassMember Members[];
};

hkpRigidBody* hkpGetRigidBody(const hkpCollidable* collidable); // 0x00710FE0 (hkprigidbody.inl:11)
hkpRigidBody* hkpGetRigidBodyUnchecked(const hkpCollidable* collidable); // 0x00DF8D90

hkpRigidBody* hkpGetRigidBody(const hkpCollidable* collidable); // 0x00710FE0 (hkprigidbody.inl:11), H3
