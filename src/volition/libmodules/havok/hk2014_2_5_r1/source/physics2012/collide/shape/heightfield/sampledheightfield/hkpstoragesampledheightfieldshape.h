// Original: volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/shape/heightfield/sampledheightfield/hkpstoragesampledheightfieldshape.h
//
// Havok 2014.2.5: full declaration from rfg.pdb (havok_impl.md HK0).
#pragma once

#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/memory/memoryclasses/hkmemoryclasses.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/container/array/hkarray.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/container/hkcontainerallocators.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/types/hkbasetypes.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/shape/heightfield/hkpheightfieldshape.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/shape/heightfield/sampledheightfield/hkpsampledheightfieldshape.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/shape/hkpshape.h"

class hkClass;
struct hkInternalClassMember;
class hkVector4f;
class hkpSampledHeightFieldBaseCinfo;

// size 0x90
class hkpStorageSampledHeightFieldShape : public hkpSampledHeightFieldShape
{
public:
    HK_DECLARE_CLASS_ALLOCATOR(); // new(unsigned int, void* p) 0x00D7E010, delete(void* p, unsigned int nbytes) 0x00D7E120
    static const hkClass& staticClass(); // 0x00D7E020
    hkpStorageSampledHeightFieldShape(hkFinishLoadedObjectFlag f); // 0x00D7E0E0
    hkpStorageSampledHeightFieldShape(const hkpSampledHeightFieldBaseCinfo& info, hkArray<float,hkContainerHeapAllocator>& samples); // 0x00DA6E20
    hkpStorageSampledHeightFieldShape(const hkpSampledHeightFieldShape* hf); // 0x00DA7390
    virtual ~hkpStorageSampledHeightFieldShape(); // vtbl+0x00 0x00D7E180
    virtual int calcSizeForSpu(const hkpShape::CalcSizeForSpuInput& input, int spuBufferSizeLeft) const; // vtbl+0x44 0x00DA6DD0
    virtual void collideSpheres(const hkpHeightFieldShape::CollideSpheresInput& input, hkVector4f* outputArray) const; // vtbl+0x48 0x00DA6F60
    void afterReflectNew(); // 0x00D7E0C0
    virtual float getHeightAtImpl(int x, int z) const; // vtbl+0x54 0x00D7E160
    virtual hkBool getTriangleFlipImpl() const; // vtbl+0x58 0x00D7E110

    static const hkInternalClassMember Members[];
    hkArray<float,hkContainerHeapAllocator> m_storage; // 0x80
    hkBool m_triangleFlip; // 0x8C
};
