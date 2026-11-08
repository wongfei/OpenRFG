// Original: volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/filter/hkpshapecollectionfilter.h
//
// Havok 2014.2.5: full declaration from rfg.pdb (havok_impl.md HK0).
#pragma once


class hkAabb;
class hkBool;
class hkClass;
struct hkInternalClassMember;
class hkpBvTreeShape;
class hkpCdBody;
struct hkpCollisionInput;
class hkpShapeContainer;

// size 0x4
class hkpShapeCollectionFilter
{
public:
    static const hkClass& staticClass(); // 0x00D7C6E0
    virtual hkBool isCollisionEnabled(const hkpCollisionInput&, const hkpCdBody&, const hkpCdBody&, const hkpShapeContainer&, const hkpShapeContainer&, unsigned int, unsigned int) const = 0; // vtbl+0x00
    virtual hkBool isCollisionEnabled(const hkpCollisionInput&, const hkpCdBody&, const hkpCdBody&, const hkpShapeContainer&, unsigned int) const = 0; // vtbl+0x04
    virtual int numShapeKeyHitsLimitBreached(const hkpCollisionInput&, const hkpCdBody&, const hkpCdBody&, const hkpBvTreeShape*, hkAabb&, unsigned int*, int) const = 0; // vtbl+0x08
    virtual ~hkpShapeCollectionFilter(); // vtbl+0x0C 0x0080CF90 (.h:67)
    hkpShapeCollectionFilter(); // 0x00D7ECE0

    // 0x00 vfptr
    static const hkInternalClassMember Members[];
};
