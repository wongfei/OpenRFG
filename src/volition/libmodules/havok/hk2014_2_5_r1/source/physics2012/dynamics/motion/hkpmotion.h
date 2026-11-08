// Original: volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/dynamics/motion/hkpmotion.h
//
// Havok 2014.2.5: full declaration from rfg.pdb (havok_impl.md HK0).
#pragma once

#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/memory/memoryclasses/hkmemoryclasses.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/math/vector/hkvector4f.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/object/hkreferencedobject.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/types/hkbasetypes.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/types/physics/motionstate/hkmotionstate.h"

class hkClass;
struct hkInternalClassMember;
class hkMatrix3f;
class hkQuaternionf;
class hkSimdFloat32;
class hkTransformf;
class hkpMaxSizeMotion;

// size 0x120
class hkpMotion : public hkReferencedObject
{
public:
    enum MotionType
    {
        MOTION_INVALID = 0,
        MOTION_DYNAMIC = 1,
        MOTION_SPHERE_INERTIA = 2,
        MOTION_BOX_INERTIA = 3,
        MOTION_KEYFRAMED = 4,
        MOTION_FIXED = 5,
        MOTION_THIN_BOX_INERTIA = 6,
        MOTION_CHARACTER = 7,
        MOTION_MAX_ID = 8,
    };

    enum
    {
        NUM_INACTIVE_FRAMES_TO_DEACTIVATE = 5,
    };

    static const hkClass& staticClass(); // 0x00DED970
    HK_DECLARE_CLASS_ALLOCATOR(); // delete(void* p, unsigned int nbytes) 0x00B96720
    MotionType getType() const;
    hkpMotion(hkFinishLoadedObjectFlag flag); // 0x00DDEB00
    hkpMotion(const hkVector4f& position, const hkQuaternionf& rotation, bool wantDeactivation); // 0x00DF0D70
    hkpMotion(); // 0x00DF0CD0
    float getMass() const; // 0x00DF0E70
    hkSimdFloat32 getMassInv() const; // 0x00B711F0 (hkpmotion.inl:21)
    virtual void setMass(const hkSimdFloat32& mass); // vtbl+0x10 0x00DF0E10
    virtual void setMass(float mass); // vtbl+0x14 0x00DF0B40
    virtual void setMassInv(const hkSimdFloat32& massInv); // vtbl+0x18 0x00DF0C40
    virtual void setMassInv(float massInv); // vtbl+0x1C 0x00DF0C20
    virtual void getInertiaLocal(hkMatrix3f&) const = 0; // vtbl+0x20
    virtual void getInertiaWorld(hkMatrix3f&) const = 0; // vtbl+0x24
    virtual void setInertiaLocal(const hkMatrix3f&) = 0; // vtbl+0x28
    virtual void setInertiaInvLocal(const hkMatrix3f&) = 0; // vtbl+0x2C
    virtual void getInertiaInvLocal(hkMatrix3f&) const = 0; // vtbl+0x30
    virtual void getInertiaInvWorld(hkMatrix3f&) const = 0; // vtbl+0x34
    virtual void setCenterOfMassInLocal(const hkVector4f& centerOfMass); // vtbl+0x38 0x00DF0B70
    const hkVector4f& getCenterOfMassLocal() const; // 0x0080CB30 (hkpmotion.inl:27)
    const hkVector4f& getCenterOfMassInWorld() const; // 0x0080CB40 (hkpmotion.inl:32)
    const hkMotionState* getMotionState() const; // 0x00E0D360
    hkMotionState* getMotionState(); // 0x00B62DD0 (hkpmotion.inl:10)
    const hkVector4f& getPosition() const; // 0x0080CB50 (hkpmotion.inl:40)
    virtual void setPosition(const hkVector4f& position); // vtbl+0x3C 0x00DF0B90
    const hkQuaternionf& getRotation() const; // 0x0080CB60 (hkpmotion.inl:46)
    virtual void setRotation(const hkQuaternionf& rotation); // vtbl+0x40 0x00DF0BB0
    virtual void setPositionAndRotation(const hkVector4f& position, const hkQuaternionf& rotation); // vtbl+0x44 0x00DF0BD0
    const hkTransformf& getTransform() const; // 0x0080CB70 (hkpmotion.inl:52)
    void approxTransformAt(float time, hkTransformf& transformOut); // 0x00DF0C70
    virtual void setTransform(const hkTransformf& transform); // vtbl+0x48 0x00DF0BF0
    const hkVector4f& getLinearVelocity() const; // 0x0080CB80 (hkpmotion.inl:64)
    virtual void setLinearVelocity(const hkVector4f& newVel); // vtbl+0x4C 0x00DF0C90
    const hkVector4f& getAngularVelocity() const; // 0x0080CB90 (hkpmotion.inl:69)
    virtual void setAngularVelocity(const hkVector4f& newVel); // vtbl+0x50 0x00DF0CB0
    void getPointVelocity(const hkVector4f& p, hkVector4f& pointVelOut) const; // 0x0080CBA0 (hkpmotion.inl:75)
    virtual void getProjectedPointVelocity(const hkVector4f&, const hkVector4f&, float&, float&) const = 0; // vtbl+0x54
    virtual void getProjectedPointVelocitySimd(const hkVector4f&, const hkVector4f&, hkSimdFloat32&, hkSimdFloat32&) const = 0; // vtbl+0x58
    virtual void applyLinearImpulse(const hkVector4f& imp); // vtbl+0x5C 0x00DF0EB0
    virtual void applyPointImpulse(const hkVector4f&, const hkVector4f&) = 0; // vtbl+0x60
    virtual void applyAngularImpulse(const hkVector4f&) = 0; // vtbl+0x64
    virtual void applyForce(const float, const hkVector4f&, const hkVector4f&) = 0; // vtbl+0x68
    virtual void applyForce(const float, const hkVector4f&) = 0; // vtbl+0x6C
    virtual void applyTorque(const float, const hkVector4f&) = 0; // vtbl+0x70
    float getLinearDamping(); // 0x00814170 (hkpmotion.inl:89)
    void setLinearDamping(float d); // 0x008141B0 (hkpmotion.inl:104)
    float getAngularDamping(); // 0x00814190 (hkpmotion.inl:94)
    void setAngularDamping(float d); // 0x008141E0 (hkpmotion.inl:110)
    float getTimeFactor(); // 0x00E12540
    void setTimeFactor(float f); // 0x00DF2DA0
    float getGravityFactor();
    void setGravityFactor(float);
    int getDeactivationClass(); // 0x00DF28F0
    void setDeactivationClass(int deactivationClass); // 0x00DF0C10
    void enableDeactivation(bool value, int randomNumber, int worldFlag0, int worldFlag1, int worldDeactivationIntegrateCounter); // 0x00DF2900
    bool isDeactivationEnabled() const; // 0x00DDE6E0
    void afterReflectNew(); // 0x00DDE670
    virtual void getMotionStateAndVelocitiesAndDeactivationType(hkpMotion* motionOut); // vtbl+0x74 0x00DF0F60
    int getNumInactiveFrames(int select); // 0x00DDE6F0
    int getNumInactiveFramesMt(int select, int worldDeactivationNumInactiveFramesSelectFlag); // 0x00E0AC30
    void setWorldSelectFlagsNeg(int worldFlag0, int worldFlag1, int worldDeactivationIntegrateCounter); // 0x00DDE680
    void incrementNumInactiveFramesMt(int select, int worldDeactivationNumInactiveFramesSelectFlag); // 0x00E12460
    void zeroNumInactiveFramesMt(int select, int worldDeactivationNumInactiveFramesSelectFlag); // 0x00E12490
    void requestDeactivation(); // 0x00DF0EE0
public:
    void init(const hkVector4f& position, const hkQuaternionf& rotation, bool wantDeactivation); // 0x00DF1030
public:
    virtual ~hkpMotion(); // vtbl+0x00 0x00B62DC0

    static const hkInternalClassMember Members[];
    hkEnum<enum MotionType,unsigned char> m_type; // 0x08
    unsigned char m_deactivationIntegrateCounter; // 0x09
    unsigned short m_deactivationNumInactiveFrames[2]; // 0x0A
    hkMotionState m_motionState; // 0x10
    hkVector4f m_inertiaAndMassInv; // 0xC0
    hkVector4f m_linearVelocity; // 0xD0
    hkVector4f m_angularVelocity; // 0xE0
    hkVector4f m_deactivationRefPosition[2]; // 0xF0
    unsigned int m_deactivationRefOrientation[2]; // 0x110
    hkpMaxSizeMotion* m_savedMotion; // 0x118
    unsigned short m_savedQualityTypeIndex; // 0x11C
    hkHalf m_gravityFactor; // 0x11E
};
