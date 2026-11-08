// Original: volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/dynamics/motion/rigid/hkpfixedrigidmotion.h
//
// Havok 2014.2.5: full declaration from rfg.pdb (havok_impl.md HK0).
#pragma once

#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/memory/memoryclasses/hkmemoryclasses.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/dynamics/motion/rigid/hkpkeyframedrigidmotion.h"

class hkClass;
class hkFinishLoadedObjectFlag;
struct hkInternalClassMember;
class hkQuaternionf;
class hkVector4f;
class hkpMotion;

// size 0x120
class hkpFixedRigidMotion : public hkpKeyframedRigidMotion
{
public:
    HK_DECLARE_CLASS_ALLOCATOR(); // new(unsigned int, void* p) 0x00DDD190, delete(void* p, unsigned int nbytes) 0x00DDEDD0
    static const hkClass& staticClass(); // 0x00DEFB80
    hkpFixedRigidMotion(hkFinishLoadedObjectFlag flag); // 0x00DDED90
    hkpFixedRigidMotion(const hkVector4f& position, const hkQuaternionf& rotation); // 0x00E14F60
    virtual ~hkpFixedRigidMotion(); // vtbl+0x00 0x00DDEDC0
    virtual void setLinearVelocity(const hkVector4f& newVel); // vtbl+0x4C 0x00E14F40
    virtual void setAngularVelocity(const hkVector4f& newVel); // vtbl+0x50 0x00E14F50
    virtual void setStepPosition(float position, float timestep); // vtbl+0x78 0x00E14F30
    virtual void getPositionAndVelocities(hkpMotion* motionOut); // vtbl+0x80 0x00E14F90

    static const hkInternalClassMember Members[];
};
