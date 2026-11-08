// Original: volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/shape/query/hkprayshapecollectionfilter.h
//
// Havok 2014.2.5: full declaration from rfg.pdb (havok_impl.md HK0).
#pragma once


class hkBool;
class hkClass;
struct hkInternalClassMember;
class hkpShapeContainer;
struct hkpShapeRayCastInput;

// size 0x4
class hkpRayShapeCollectionFilter
{
public:
    static const hkClass& staticClass(); // 0x00D7C700
    virtual hkBool isCollisionEnabled(const hkpShapeRayCastInput&, const hkpShapeContainer&, unsigned int) const = 0; // vtbl+0x00
    virtual ~hkpRayShapeCollectionFilter(); // vtbl+0x04 0x0080CFC0 (.h:36)
    hkpRayShapeCollectionFilter(); // 0x00D7ECF0

    // 0x00 vfptr
    static const hkInternalClassMember Members[];
};
