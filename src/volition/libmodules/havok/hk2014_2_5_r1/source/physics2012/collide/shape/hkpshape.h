// Original: volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/shape/hkpshape.h
//
// Havok 2014.2.5: full declaration from rfg.pdb (havok_impl.md HK0); hkpShapeKeyPair added by hand (HK6).
#pragma once

#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/memory/memoryclasses/hkmemoryclasses.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/geometry/collide/shapes/hkcdshape.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/shape/hkpshapebase.h"

class hkClass;
class hkFinishLoadedObjectFlag;
struct hkInternalClassMember;
class hkVector4f;
class hkVector4fComparison;
class hkpShapeContainer;
struct hkpShapeRayBundleCastInput;
struct hkpShapeRayBundleCastOutput;

// size 0x10
class hkpShape : public hkpShapeBase
{
public:
    enum
    {
        THIS_SHAPE_TYPE = 34,
    };

    // size 0x3
    struct CalcSizeForSpuInput
    {
        HK_DECLARE_NONVIRTUAL_CLASS_ALLOCATOR(CalcSizeForSpuInput);

        bool m_midphaseAgent3Registered; // 0x00
        bool m_isFixedOrKeyframed; // 0x01
        bool m_hasDynamicMotionSaved; // 0x02
    };

    HK_DECLARE_CLASS_ALLOCATOR(); // new(unsigned int, void* p) 0x00D7EB60, delete(void* p, unsigned int nbytes) 0x0071A9A0
    static const hkClass& staticClass(); // 0x00D7EB00
    hkpShape(hkFinishLoadedObjectFlag flag); // 0x00D746A0
    hkpShape(hkcdShapeType::ShapeTypeEnum);
    hkpShape();
    void afterReflectNew(); // 0x00D745D0
    virtual ~hkpShape(); // vtbl+0x00 0x007108D0
    virtual hkVector4fComparison castRayBundle(const hkpShapeRayBundleCastInput& input, hkpShapeRayBundleCastOutput& output, const hkVector4fComparison& mask) const; // vtbl+0x20 0x00D74470
    virtual const hkpShapeContainer* getContainer() const; // vtbl+0x3C 0x0071A990 (.h:92)
    unsigned long getUserData() const; // 0x00D76030
    void setUserData(unsigned long data); // 0x007108E0 (hkpshape.inl:20)
    virtual float getMaximumProjection(const hkVector4f& direction) const; // vtbl+0x40 0x00D745E0
    virtual int calcSizeForSpu(const CalcSizeForSpuInput& input, int spuBufferSizeLeft) const; // vtbl+0x44 0x00D74460

    static const hkInternalClassMember Members[];
    unsigned long m_userData; // 0x0C
};

// size 0x8 (hand-maintained since HK6: inline operators of the 2014.1 header)
class hkpShapeKeyPair
{
public:
    HK_DECLARE_NONVIRTUAL_CLASS_ALLOCATOR(hkpShapeKeyPair);

    // 0x00DCB690
    bool operator==(const hkpShapeKeyPair& p1) const
    {
        return m_shapeKeyB == p1.m_shapeKeyB && m_shapeKeyA == p1.m_shapeKeyA;
    }

    // 0x00DB9300
    bool operator<(const hkpShapeKeyPair& p1) const
    {
        return (m_shapeKeyA < p1.m_shapeKeyA) || (m_shapeKeyA == p1.m_shapeKeyA && m_shapeKeyB < p1.m_shapeKeyB);
    }

    unsigned int m_shapeKeyA; // 0x00 (only used by the nm machine)
    unsigned int m_shapeKeyB; // 0x04 (used by the 1n and nm machines)
};

// size 0x4
// Changes a shape in place (hkpRigidBody::updateShape); declared in hkpShape.h as in Havok 2014.1. HK7.
class hkpShapeModifier
{
public:
    virtual ~hkpShapeModifier() {} // vtbl+0x00
    virtual void modifyShape(hkpShape* shapeInOut) = 0; // vtbl+0x04
};
