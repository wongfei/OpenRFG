// Original: volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/shape/misc/phantomcallback/hkpphantomcallbackshape.h
//
// Havok 2014.2.5: full declaration from rfg.pdb (havok_impl.md HK0).
#pragma once

#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/memory/memoryclasses/hkmemoryclasses.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/shape/hkpshape.h"

class hkAabb;
class hkBool;
class hkClass;
class hkFinishLoadedObjectFlag;
struct hkInternalClassMember;
class hkTransformf;
class hkpCdBody;
class hkpCollidable;
struct hkpCollisionInput;
class hkpRayHitCollector;
struct hkpShapeRayCastInput;
struct hkpShapeRayCastOutput;

// size 0x10
class hkpPhantomCallbackShape : public hkpShape
{
public:
    enum
    {
        THIS_SHAPE_TYPE = 32,
    };

    HK_DECLARE_CLASS_ALLOCATOR(); // new(unsigned int nbytes) 0x00815DC0, delete(void* p, unsigned int nbytes) 0x00815E00
    static const hkClass& staticClass(); // 0x00D7C720
    hkpPhantomCallbackShape(hkFinishLoadedObjectFlag flag); // 0x00D83F10
    hkpPhantomCallbackShape(); // 0x00815DE0 (.h:36)
    virtual ~hkpPhantomCallbackShape(); // vtbl+0x00 0x0080D970
    virtual void getAabb(const hkTransformf& localToWorld, float tolerance, hkAabb& out) const; // vtbl+0x14 0x00D83EE0
    virtual hkBool castRay(const hkpShapeRayCastInput& input, hkpShapeRayCastOutput& results) const; // vtbl+0x18 0x00D83F00
    virtual void castRayWithCollector(const hkpShapeRayCastInput& input, const hkpCdBody& cdBody, hkpRayHitCollector& collector) const; // vtbl+0x1C 0x00D83ED0
    virtual void phantomEnterEvent(const hkpCollidable*, const hkpCollidable*, const hkpCollisionInput&) = 0; // vtbl+0x48
    virtual void phantomLeaveEvent(const hkpCollidable*, const hkpCollidable*) = 0; // vtbl+0x4C

    static const hkInternalClassMember Members[];
};
