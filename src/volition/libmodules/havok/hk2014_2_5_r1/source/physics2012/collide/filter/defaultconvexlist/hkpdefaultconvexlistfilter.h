// Original: volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/filter/defaultconvexlist/hkpdefaultconvexlistfilter.h
//
// Havok 2014.2.5: full declaration from rfg.pdb (havok_impl.md HK0).
#pragma once

#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/memory/memoryclasses/hkmemoryclasses.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/filter/hkpconvexlistfilter.h"

class hkClass;
class hkFinishLoadedObjectFlag;
struct hkInternalClassMember;
class hkpCdBody;
struct hkpCollisionInput;

// size 0x8
class hkpDefaultConvexListFilter : public hkpConvexListFilter
{
public:
    HK_DECLARE_CLASS_ALLOCATOR(); // new(unsigned int, void* p) 0x00D7F2B0, new(unsigned int nbytes) 0x00DEA140, delete(void* p, unsigned int nbytes) 0x00D7F3D0
    static const hkClass& staticClass(); // 0x00D7F2C0
    hkpDefaultConvexListFilter(hkFinishLoadedObjectFlag flag); // 0x00D7F320
    hkpDefaultConvexListFilter(); // 0x00D9D0D0
    virtual ~hkpDefaultConvexListFilter(); // vtbl+0x00 0x00D7F330
    virtual hkpConvexListFilter::ConvexListCollisionType getConvexListCollisionType(const hkpCdBody& convexListBody, const hkpCdBody& otherBody, const hkpCollisionInput& input) const; // vtbl+0x10 0x00DAB4D0

    static const hkInternalClassMember Members[];
};
