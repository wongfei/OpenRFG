// Original: volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/dynamics/motion/rigid/hkpcharactermotion.h
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

// size 0x120
class hkpCharacterMotion : public hkpMotion
{
public:
    HK_DECLARE_CLASS_ALLOCATOR(); // new(unsigned int, void* p) 0x00DDD1D0, delete(void* p, unsigned int nbytes) 0x00DDF060
    static const hkClass& staticClass(); // 0x00DF0340
    hkpCharacterMotion(hkFinishLoadedObjectFlag flag); // 0x00DDF030
    hkpCharacterMotion(const hkVector4f& position, const hkQuaternionf& rotation); // 0x00DF2AE0
    virtual void getInertiaLocal(hkMatrix3f& inertia) const; // vtbl+0x20 0x00E15520
    virtual void getInertiaWorld(hkMatrix3f& inertia) const; // vtbl+0x24 0x00E155A0
    virtual void setInertiaLocal(const hkMatrix3f& inertia); // vtbl+0x28 0x00E15830
    virtual void setInertiaInvLocal(const hkMatrix3f& inertiaInv); // vtbl+0x2C 0x00E158C0
    virtual void getInertiaInvLocal(hkMatrix3f& inertiaInv) const; // vtbl+0x30 0x00E15600
    virtual void getInertiaInvWorld(hkMatrix3f& inertiaInv) const; // vtbl+0x34 0x00E15640
    virtual void getProjectedPointVelocity(const hkVector4f& pos, const hkVector4f& normal, float& velOut, float& invVirtMassOut) const; // vtbl+0x54 0x00E15710
    virtual void getProjectedPointVelocitySimd(const hkVector4f& pos, const hkVector4f& normal, hkSimdFloat32& velOut, hkSimdFloat32& invVirtMassOut) const; // vtbl+0x58 0x00E157A0
    virtual void applyPointImpulse(const hkVector4f& imp, const hkVector4f& p); // vtbl+0x60 0x00E15680
    virtual void applyAngularImpulse(const hkVector4f& imp); // vtbl+0x64 0x00E15490
    virtual void applyForce(const float deltaTime, const hkVector4f& force, const hkVector4f& p); // vtbl+0x68 0x00E154B0
    virtual void applyForce(const float deltaTime, const hkVector4f& force); // vtbl+0x6C 0x00E156E0
    virtual void applyTorque(const float deltaTime, const hkVector4f& torque); // vtbl+0x70 0x00E154F0
public:
    void _applyForces(const float);
public:
    virtual ~hkpCharacterMotion(); // vtbl+0x00 0x00DDF0A0

    static const hkInternalClassMember Members[];
};
