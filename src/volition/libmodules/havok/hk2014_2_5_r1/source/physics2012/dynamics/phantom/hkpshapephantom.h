// Original: volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/dynamics/phantom/hkpshapephantom.h
//
// Havok 2014.2.5: full declaration from rfg.pdb (havok_impl.md HK0).
#pragma once

#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/memory/memoryclasses/hkmemoryclasses.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/dynamics/world/util/hkpworldoperationqueue.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/types/physics/motionstate/hkmotionstate.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/dynamics/phantom/hkpphantom.h"

class hkAabb;
class hkClass;
class hkFinishLoadedObjectFlag;
struct hkInternalClassMember;
class hkTransformf;
class hkVector4f;
class hkpCdBodyPairCollector;
class hkpCdPointCollector;
struct hkpCollisionInput;
struct hkpLinearCastInput;
class hkpShape;

// size 0x150
class hkpShapePhantom : public hkpPhantom
{
public:
    HK_DECLARE_CLASS_ALLOCATOR(); // delete(void* p, unsigned int nbytes) 0x00DEE1F0
    static const hkClass& staticClass(); // 0x00DED950
    hkpShapePhantom(hkFinishLoadedObjectFlag flag); // 0x00DEE1A0
    hkpShapePhantom(const hkpShape* shape, const hkTransformf& transform); // 0x00DF4E20
    virtual ~hkpShapePhantom(); // vtbl+0x00 0x00DDFB60
    virtual hkWorldOperation::Result setShape(const hkpShape* shape); // vtbl+0x10 0x00DF4C90
    virtual hkMotionState* getMotionState(); // vtbl+0x18 0x00DF4BC0
    virtual void calcAabb(hkAabb& aabb); // vtbl+0x20 0x00DF4C50
    const hkTransformf& getTransform() const; // 0x0080D610 (hkpshapephantom.inl:14)
    void setTransform(const hkTransformf& transform); // 0x00DF4DA0
    void setPosition(const hkVector4f& position, float extraTolerance); // 0x00DF4BE0
    virtual void deallocateInternalArrays(); // vtbl+0x3C 0x00DF4BD0
    virtual void setPositionAndLinearCast(const hkVector4f&, const hkpLinearCastInput&, hkpCdPointCollector&, hkpCdPointCollector*) = 0; // vtbl+0x40
    virtual void setTransformAndLinearCast(const hkTransformf&, const hkpLinearCastInput&, hkpCdPointCollector&, hkpCdPointCollector*) = 0; // vtbl+0x44
    virtual void getClosestPoints(hkpCdPointCollector&, const hkpCollisionInput*) = 0; // vtbl+0x48
    virtual void getPenetrations(hkpCdBodyPairCollector&, const hkpCollisionInput*) = 0; // vtbl+0x4C

    static const hkInternalClassMember Members[];
    hkMotionState m_motionState; // 0xA0
};
