// Original: volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/filter/hkpraycollidablefilter.h
//
// Havok 2014.2.5: full declaration from rfg.pdb (havok_impl.md HK0).
#pragma once


class hkBool;
class hkClass;
struct hkInternalClassMember;
class hkpCollidable;
struct hkpWorldRayCastInput;

// size 0x4
class hkpRayCollidableFilter
{
public:
    static const hkClass& staticClass(); // 0x00D7C710
    virtual ~hkpRayCollidableFilter(); // vtbl+0x00 0x0080CF60 (.h:26)
    virtual hkBool isCollisionEnabled(const hkpWorldRayCastInput&, const hkpCollidable&) const = 0; // vtbl+0x04
    hkpRayCollidableFilter(); // 0x00D7ED00

    // 0x00 vfptr
    static const hkInternalClassMember Members[];
};
