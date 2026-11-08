// Original: volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/filter/hkpcollidablecollidablefilter.h
//
// Havok 2014.2.5: full declaration from rfg.pdb (havok_impl.md HK0).
#pragma once


class hkBool;
class hkClass;
struct hkInternalClassMember;
class hkpCollidable;

// size 0x4
class hkpCollidableCollidableFilter
{
public:
    static const hkClass& staticClass(); // 0x00D7C770
    virtual ~hkpCollidableCollidableFilter(); // vtbl+0x00 0x0080CF30 (.h:25)
    virtual hkBool isCollisionEnabled(const hkpCollidable&, const hkpCollidable&) const = 0; // vtbl+0x04
    hkpCollidableCollidableFilter(); // 0x00D7ECD0

    // 0x00 vfptr
    static const hkInternalClassMember Members[];
};
