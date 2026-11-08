// Original: volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/internal/collide/mopp/machine/hkpmoppobbvirtualmachine.h
//
// Havok 2014.2.5: full declaration from rfg.pdb (havok_impl.md HK0), with hkpMoppObbVirtualMachineQuery (HK3).
// Hand-maintained (nobuild in the hdrgen specs).
#pragma once

#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/memory/memoryclasses/hkmemoryclasses.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/container/array/hkarray.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/types/hkbasetypes.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/internal/collide/mopp/machine/hkpmoppvirtualmachine.h"

class hkAabb;
struct hkContainerHeapAllocator;
class hkTransformf;
class hkVector4f;
class hkpMoppCode;

// size 0x40: the query box in the fixed point frame of the current node (16 bits shifted by m_shift)
struct alignas(16) hkpMoppObbVirtualMachineQuery
{
    int m_xHi; // 0x00
    int m_yHi; // 0x04
    int m_zHi; // 0x08
    int m_HiPadding; // 0x0C
    int m_xLo; // 0x10
    int m_yLo; // 0x14
    int m_zLo; // 0x18
    int m_LoPadding; // 0x1C
    int m_offset_x; // 0x20
    int m_offset_y; // 0x24
    int m_offset_z; // 0x28
    unsigned int m_primitiveOffset; // 0x2C
    int m_shift; // 0x30
    unsigned int m_properties[1]; // 0x34
};

// size 0x40
class alignas(16) hkpMoppObbVirtualMachine : public hkpMoppVirtualMachine
{
public:
    typedef hkArray<hkpMoppPrimitiveInfo,hkContainerHeapAllocator>* hkpPrimitiveOutputArray;

    HK_DECLARE_NONVIRTUAL_CLASS_ALLOCATOR(hkpMoppObbVirtualMachine);
    hkpMoppObbVirtualMachine(); // 0x00D9A5C0
    ~hkpMoppObbVirtualMachine(); // 0x00D99FB0
    void queryObb(const hkpMoppCode* code, const hkTransformf& BvToWorld, const hkVector4f& extent, const float radius, hkArray<hkpMoppPrimitiveInfo,hkContainerHeapAllocator>* primitives_out); // 0x00E683E0
    void queryAabb(const hkpMoppCode* code, const hkAabb& aabb, hkArray<hkpMoppPrimitiveInfo,hkContainerHeapAllocator>* primitives_out); // 0x00E685B0
public:
    void queryAabbOnTree(const hkpMoppObbVirtualMachineQuery* query, const unsigned char* PC, int chunkOffset); // 0x00E67C00
    void generateQueryFromNode(const hkVector4f& extent, const hkTransformf& BvToWorld, const float radius, hkpMoppObbVirtualMachineQuery& query); // 0x00E688E0
    void generateQueryFromAabb(const hkVector4f& aabbMin, const hkVector4f& aabbMax, hkpMoppObbVirtualMachineQuery& query); // 0x00E68730

    int m_xHi; // 0x10
    int m_yHi; // 0x14
    int m_zHi; // 0x18
    int m_HiPadding; // 0x1C
    int m_xLo; // 0x20
    int m_yLo; // 0x24
    int m_zLo; // 0x28
    int m_LoPadding; // 0x2C
    hkPadSpu<hkpMoppCode const*> m_code; // 0x30
    hkPadSpu<int> m_reindexingMask; // 0x34
};
