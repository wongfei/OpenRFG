// Original: volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/filter/hkpconvexlistfilter.h
//
// Seam only (Havok 2014.2.5).
#pragma once

#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/object/hkreferencedobject.h"

class hkClass;
class hkFinishLoadedObjectFlag;
class hkpCdBody;
struct hkpCollisionInput;

// size 0x8
class hkpConvexListFilter : public hkReferencedObject
{
public:
    enum ConvexListCollisionType
    {
        TREAT_CONVEX_LIST_AS_NORMAL = 0,
        TREAT_CONVEX_LIST_AS_LIST = 1,
        TREAT_CONVEX_LIST_AS_CONVEX = 2,
    };

    static const hkClass& staticClass(); // 0x00D7C750
    virtual ConvexListCollisionType getConvexListCollisionType(const hkpCdBody&, const hkpCdBody&, const hkpCollisionInput&) const = 0; // vtbl+0x10
public:
    hkpConvexListFilter(); // 0x00D9D0C0
public:
    hkpConvexListFilter(hkFinishLoadedObjectFlag flag); // 0x00D7F300
    virtual ~hkpConvexListFilter(); // vtbl+0x00 0x00D7F310

};
