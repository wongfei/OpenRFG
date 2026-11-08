// Original: volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/internal/collide/mopp/machine/hkpmoppraybundlevirtualmachine.h
//
// Havok 2014.2.5: full declaration from rfg.pdb (havok_impl.md HK0).
#pragma once

#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/memory/memoryclasses/hkmemoryclasses.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/math/vector/hkvector4f.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/math/vector/hkvector4fcomparison.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/internal/collide/mopp/machine/hkpmoppvirtualmachine.h"

class hkpCdBody;
class hkpMoppCode;
class hkpRayHitCollector;
class hkpShapeContainer;
struct hkpShapeRayBundleCastInput;
struct hkpShapeRayBundleCastOutput;

// size 0x30
struct alignas(16) RayPointBundle
{
    hkVector4f m_vec[3]; // 0x00
};

// size 0xE0
class alignas(16) hkpMoppRayBundleVirtualMachine : public hkpMoppVirtualMachine
{
public:
    // size 0x40
    struct alignas(16) QueryInt
    {
        hkVector4f m_FtoBoffset; // 0x00
        int m_shift; // 0x10
        hkVector4f m_FtoBScale; // 0x20
        unsigned int m_primitiveOffset; // 0x30
        unsigned int m_properties[1]; // 0x34
    };

    // size 0x70
    struct QueryBundle
    {
        RayPointBundle m_rayEnds[2]; // 0x00
        hkVector4fComparison m_activeMask; // 0x60
    };

    HK_DECLARE_NONVIRTUAL_CLASS_ALLOCATOR(hkpMoppRayBundleVirtualMachine);
    hkpMoppRayBundleVirtualMachine(); // 0x00D99FC0
    ~hkpMoppRayBundleVirtualMachine(); // 0x00D99FD0
    hkVector4fComparison queryRayBundle(const hkpShapeContainer* collection, const hkpMoppCode* code, const hkpShapeRayBundleCastInput& input, hkpShapeRayBundleCastOutput& rayResult, const hkVector4fComparison& mask); // 0x00E6B220
public:
    void queryRayOnTree(const QueryInt* query, const unsigned char* PC, QueryBundle* const fQuery, int chunkId); // 0x00E69F00
    void addHit(unsigned int key, const unsigned int* properties, QueryBundle* fQuery); // 0x00E6B4C0
    void queryRayBundleSub(const hkpMoppCode* code, const hkpShapeRayBundleCastInput& input, const hkVector4fComparison& mask); // 0x00E6B6D0

    const hkpMoppCode* m_code; // 0x10
    hkVector4f m_ItoFScale; // 0x20
    const hkpShapeRayBundleCastInput* m_rays; // 0x30
    RayPointBundle m_to; // 0x40
    RayPointBundle m_from; // 0x70
    hkVector4fComparison m_hitsFound; // 0xA0
    hkVector4f m_earlyOutHitFraction; // 0xB0
    hkpShapeRayBundleCastOutput* m_rayResultPtr; // 0xC0
    hkpRayHitCollector* m_collector; // 0xC4
    const hkpCdBody* m_body; // 0xC8
    const hkpShapeContainer* m_collection; // 0xCC
    int m_reindexingMask; // 0xD0
};
