// Original: volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/shape/misc/bv/hkpbvshape.h
//
// Havok 2014.2.5: full declaration from rfg.pdb (havok_impl.md HK0).
#pragma once

#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/memory/memoryclasses/hkmemoryclasses.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/shape/hkpshape.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/shape/hkpshapecontainer.h"

class hkAabb;
class hkBool;
class hkClass;
class hkFinishLoadedObjectFlag;
struct hkInternalClassMember;
class hkTransformf;
class hkpCdBody;
class hkpRayHitCollector;
struct hkpShapeRayCastInput;
struct hkpShapeRayCastOutput;

// size 0x1C
class hkpBvShape : public hkpShape
{
public:
    enum
    {
        THIS_SHAPE_TYPE = 30,
    };

    HK_DECLARE_CLASS_ALLOCATOR(); // new(unsigned int, void* p) 0x00D7FB80, new(unsigned int nbytes) 0x00816030, delete(void* p, unsigned int nbytes) 0x00D9CD50
    static const hkClass& staticClass(); // 0x00D7FB90
    hkpBvShape(hkFinishLoadedObjectFlag flag); // 0x00D9CB90
    hkpBvShape(const hkpShape* boundingVolumeShape, const hkpShape* childShape); // 0x00D9CB20
    virtual ~hkpBvShape(); // vtbl+0x00 0x00D9CBC0
    virtual void getAabb(const hkTransformf& localToWorld, float tolerance, hkAabb& out) const; // vtbl+0x14 0x00D9CAE0
    const hkpShape* getBoundingVolumeShape() const; // 0x00D9CAC0
    const hkpShape* getChildShape() const; // 0x00D9CAD0
    virtual hkBool castRay(const hkpShapeRayCastInput& input, hkpShapeRayCastOutput& results) const; // vtbl+0x18 0x00D9CC00
    virtual void castRayWithCollector(const hkpShapeRayCastInput& input, const hkpCdBody& cdBody, hkpRayHitCollector& collector) const; // vtbl+0x1C 0x00D9CCA0
    virtual const hkpShapeContainer* getContainer() const; // vtbl+0x3C 0x00D9CB10

    static const hkInternalClassMember Members[];
    const hkpShape* m_boundingVolumeShape; // 0x10
    hkpSingleShapeContainer m_childShape; // 0x14
};
