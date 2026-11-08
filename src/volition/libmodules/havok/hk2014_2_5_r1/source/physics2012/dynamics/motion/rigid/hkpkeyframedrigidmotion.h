// Original: volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/dynamics/motion/rigid/hkpkeyframedrigidmotion.h
//
// Havok 2014.2.5: full declaration from rfg.pdb (havok_impl.md HK0).
#pragma once

#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/memory/memoryclasses/hkmemoryclasses.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/dynamics/motion/hkpmotion.h"

class hkClass;
class hkFinishLoadedObjectFlag;
struct hkInternalClassMember;
class hkMatrix3f;
class hkQuaternionf;
class hkSimdFloat32;
class hkVector4f;
class hkpMaxSizeMotion;

// size 0x120
class hkpKeyframedRigidMotion : public hkpMotion
{
public:
    HK_DECLARE_CLASS_ALLOCATOR(); // new(unsigned int, void* p) 0x00DDE700, delete(void* p, unsigned int nbytes) 0x00DDEB20
    static const hkClass& staticClass(); // 0x00DEF960
    hkpKeyframedRigidMotion(hkFinishLoadedObjectFlag flag); // 0x00DDEB60
public:
    hkpKeyframedRigidMotion(); // 0x00DDE710
public:
    hkpKeyframedRigidMotion(const hkVector4f& position, const hkQuaternionf& rotation); // 0x00E14CF0
    virtual ~hkpKeyframedRigidMotion(); // vtbl+0x00 0x00E14DF0
    virtual void setMass(const hkSimdFloat32& m); // vtbl+0x10 0x00E14C30
    virtual void setMass(float m); // vtbl+0x14 0x00E14C20
    virtual void setMassInv(const hkSimdFloat32& mInv); // vtbl+0x18 0x00E14C50
    virtual void setMassInv(float mInv); // vtbl+0x1C 0x00E14C40
    virtual void getInertiaLocal(hkMatrix3f& inertia) const; // vtbl+0x20 0x00E14D40
    virtual void getInertiaWorld(hkMatrix3f& inertia) const; // vtbl+0x24 0x00E14D60
    virtual void setInertiaLocal(const hkMatrix3f& inertia); // vtbl+0x28 0x00E14C60
    virtual void setInertiaInvLocal(const hkMatrix3f& inertiaInv); // vtbl+0x2C 0x00E14C70
    virtual void getInertiaInvLocal(hkMatrix3f& inertiaInv) const; // vtbl+0x30 0x00E14D80
    virtual void getInertiaInvWorld(hkMatrix3f& inertiaInvOut) const; // vtbl+0x34 0x00E14DA0
    virtual void getProjectedPointVelocity(const hkVector4f& pos, const hkVector4f& normal, float& velOut, float& invVirtMassOut) const; // vtbl+0x54 0x00E14E10
    virtual void getProjectedPointVelocitySimd(const hkVector4f& pos, const hkVector4f& normal, hkSimdFloat32& velOut, hkSimdFloat32& invVirtMassOut) const; // vtbl+0x58 0x00E14EA0
    virtual void applyLinearImpulse(const hkVector4f& imp); // vtbl+0x5C 0x00E14C80
    virtual void applyPointImpulse(const hkVector4f& imp, const hkVector4f& p); // vtbl+0x60 0x00E14C90
    virtual void applyAngularImpulse(const hkVector4f& imp); // vtbl+0x64 0x00E14CA0
    virtual void applyForce(const float deltaTime, const hkVector4f& force, const hkVector4f& p); // vtbl+0x68 0x00E14CC0
    virtual void applyForce(const float deltaTime, const hkVector4f& force); // vtbl+0x6C 0x00E14CB0
    virtual void applyTorque(const float deltaTime, const hkVector4f& torque); // vtbl+0x70 0x00E14CD0
    virtual void setStepPosition(float position, float timestep); // vtbl+0x78 0x00E14CE0
    virtual void setStoredMotion(hkpMaxSizeMotion* savedMotion); // vtbl+0x7C 0x00E14DC0

    static const hkInternalClassMember Members[];
};

// size 0x120
class alignas(16) hkpMaxSizeMotion : public hkpKeyframedRigidMotion
{
public:
    HK_DECLARE_CLASS_ALLOCATOR(); // new(unsigned int, void* p) 0x00DEFA20, new(unsigned int nbytes) 0x00DF2DD0, delete(void* p, unsigned int nbytes) 0x00DDEBE0
    static const hkClass& staticClass(); // 0x00DEF980
public:
    hkpMaxSizeMotion(const hkpMaxSizeMotion&);
public:
    hkpMaxSizeMotion(hkFinishLoadedObjectFlag flag); // 0x00DDEC20
    hkpMaxSizeMotion(); // 0x00DDE730
public:
    void operator=(const hkpMaxSizeMotion&);
public:
    virtual ~hkpMaxSizeMotion(); // vtbl+0x00 0x00DDE750

    static const hkInternalClassMember Members[];
};
