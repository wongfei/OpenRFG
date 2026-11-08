// Original: volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/shape/misc/transform/hkptransformshape.h
//
// Havok 2014.2.5: full declaration from rfg.pdb (havok_impl.md HK0).
#pragma once

#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/memory/memoryclasses/hkmemoryclasses.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/math/matrix/hktransformf.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/math/quaternion/hkquaternionf.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/shape/hkpshape.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/shape/hkpshapecontainer.h"

class hkAabb;
class hkBool;
class hkClass;
class hkFinishLoadedObjectFlag;
struct hkInternalClassMember;
class hkVector4f;
class hkpCdBody;
class hkpRayHitCollector;
struct hkpShapeRayCastInput;
struct hkpShapeRayCastOutput;

// size 0x70
class hkpTransformShape : public hkpShape
{
public:
    enum
    {
        THIS_SHAPE_TYPE = 14,
    };

    static const hkClass& staticClass(); // 0x00D7E630
    HK_DECLARE_CLASS_ALLOCATOR(); // new(unsigned int, void* p) 0x00D7E620, delete(void* p, unsigned int nbytes) 0x00DAA1E0
    hkpTransformShape(hkFinishLoadedObjectFlag flag); // 0x00DA9B80
    hkpTransformShape(const hkpShape* childShape, const hkTransformf& transform); // 0x00DAA190
    hkpTransformShape();
    const hkpShape* getChildShape() const; // 0x00A12FE0 (hkptransformshape.inl:10)
    const hkTransformf& getTransform() const; // 0x00DC6250
    const hkQuaternionf& getRotation() const; // 0x00DC6260
    void setTransform(const hkTransformf& transform); // 0x00DAA150
    virtual ~hkpTransformShape(); // vtbl+0x00 0x00DAA220
    virtual void getAabb(const hkTransformf& localToWorld, float tolerance, hkAabb& out) const; // vtbl+0x14 0x00DA9AD0
    virtual hkBool castRay(const hkpShapeRayCastInput& input, hkpShapeRayCastOutput& results) const; // vtbl+0x18 0x00DA9CA0
    virtual void castRayWithCollector(const hkpShapeRayCastInput& input, const hkpCdBody& cdBody, hkpRayHitCollector& collector) const; // vtbl+0x1C 0x00DA9F10
    virtual const hkpShapeContainer* getContainer() const; // vtbl+0x3C 0x00DA9B20
    virtual float getMaximumProjection(const hkVector4f& direction) const; // vtbl+0x40 0x00DA9BB0
    void getChildShapeFromPpu() const;
    virtual int calcSizeForSpu(const hkpShape::CalcSizeForSpuInput& input, int spuBufferSizeLeft) const; // vtbl+0x44 0x00DA9B30

    static const hkInternalClassMember Members[]; // 0x01356440
public:
    hkpSingleShapeContainer m_childShape; // 0x10
    int m_childShapeSize; // 0x18
    hkQuaternionf m_rotation; // 0x20
    hkTransformf m_transform; // 0x30
};
