// Original: volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/internal/collide/mopp/machine/hkpmopplongrayvirtualmachine.h
//
// Havok 2014.2.5: full declaration from rfg.pdb (havok_impl.md HK0).
#pragma once

#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/memory/memoryclasses/hkmemoryclasses.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/math/vector/hkvector4f.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/shape/query/hkpshaperaycastinput.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/internal/collide/mopp/machine/hkpmoppvirtualmachine.h"

class hkBool;
class hkpCdBody;
class hkpMoppCode;
class hkpRayHitCollector;
class hkpShapeContainer;
struct hkpShapeRayCastOutput;

// size 0x70
class hkpMoppLongRayVirtualMachine : public hkpMoppVirtualMachine
{
public:
    // size 0x20
    struct QueryInt
    {
        hkVector4f m_FtoBoffset; // 0x00
        int m_shift; // 0x10
        float m_FtoBScale; // 0x14
        unsigned int m_primitiveOffset; // 0x18
        unsigned int m_properties[1]; // 0x1C
    };

    // size 0x20
    struct QueryFloat
    {
        hkVector4f m_rayEnds[2]; // 0x00
    };

    HK_DECLARE_NONVIRTUAL_CLASS_ALLOCATOR(hkpMoppLongRayVirtualMachine);
    hkpMoppLongRayVirtualMachine(); // 0x00D99F80
    ~hkpMoppLongRayVirtualMachine(); // 0x00D99FA0
    void queryLongRay(const hkpShapeContainer* collection, const hkpMoppCode* code, const hkpShapeRayCastInput& input, const hkpCdBody& body, hkpRayHitCollector& collector); // 0x00E69B20
    hkBool queryLongRay(const hkpShapeContainer* collection, const hkpMoppCode* code, const hkpShapeRayCastInput& input, hkpShapeRayCastOutput& rayResult); // 0x00E69A30
public:
    void queryRayOnTree(const QueryInt* query, const unsigned char* PC, QueryFloat* const fQuery, int chunkOffset); // 0x00E68CA0
    void addHit(unsigned int key, const unsigned int* properties); // 0x00E68B60
    void queryLongRaySub(const hkpMoppCode* code, const hkpShapeRayCastInput& input); // 0x00E69C10

    const hkpMoppCode* m_code; // 0x10
    float m_ItoFScale; // 0x14
    hkpShapeRayCastInput m_ray; // 0x20
    bool m_hitFound; // 0x50
    float m_earlyOutHitFraction; // 0x54
    hkpShapeRayCastOutput* m_rayResultPtr; // 0x58
    hkpRayHitCollector* m_collector; // 0x5C
    const hkpCdBody* m_body; // 0x60
    const hkpShapeContainer* m_collection; // 0x64
    int m_reindexingMask; // 0x68
};
