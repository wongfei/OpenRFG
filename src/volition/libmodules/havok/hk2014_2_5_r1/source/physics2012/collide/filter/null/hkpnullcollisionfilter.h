// Original: volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/filter/null/hkpnullcollisionfilter.h
//
// Havok 2014.2.5: full declaration from rfg.pdb (havok_impl.md HK0).
#pragma once

#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/memory/memoryclasses/hkmemoryclasses.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/filter/hkpcollisionfilter.h"

class hkBool;
class hkClass;
class hkFinishLoadedObjectFlag;
struct hkInternalClassMember;
class hkpCdBody;
class hkpCollidable;
struct hkpCollisionInput;
class hkpShapeContainer;
struct hkpShapeRayCastInput;
struct hkpWorldRayCastInput;

// size 0x30
class hkpNullCollisionFilter : public hkpCollisionFilter
{
public:
    HK_DECLARE_CLASS_ALLOCATOR(); // new(unsigned int, void* p) 0x00D7ED10, new(unsigned int nbytes) 0x00DEA120, delete(void* p, unsigned int nbytes) 0x00D7EFC0
    static const hkClass& staticClass(); // 0x00D7ED20
    hkpNullCollisionFilter(hkFinishLoadedObjectFlag flag); // 0x00D7EED0
    hkpNullCollisionFilter(); // 0x00D9ED00
    virtual ~hkpNullCollisionFilter(); // vtbl+0x00 0x00DAA7F0
    virtual hkBool isCollisionEnabled(const hkpShapeRayCastInput& aInput, const hkpShapeContainer& bContainer, unsigned int bKey) const; // vtbl+0x00 0x00D7EF60
    virtual hkBool isCollisionEnabled(const hkpCollisionInput& input, const hkpCdBody& collectionBodyA, const hkpCdBody& collectionBodyB, const hkpShapeContainer& containerShapeA, const hkpShapeContainer& containerShapeB, unsigned int keyA, unsigned int keyB) const; // vtbl+0x00 0x00D7EF50
    virtual hkBool isCollisionEnabled(const hkpWorldRayCastInput& a, const hkpCollidable& collidableB) const; // vtbl+0x04 0x00D7EF70
    virtual hkBool isCollisionEnabled(const hkpCollisionInput& input, const hkpCdBody& a, const hkpCdBody& b, const hkpShapeContainer& bContainer, unsigned int bKey) const; // vtbl+0x04 0x00D7EF40
    virtual hkBool isCollisionEnabled(const hkpCollidable& a, const hkpCollidable& b) const; // vtbl+0x04 0x00D7EF30
    void afterReflectNew(); // 0x00D7EE50

    static const hkInternalClassMember Members[];
};
