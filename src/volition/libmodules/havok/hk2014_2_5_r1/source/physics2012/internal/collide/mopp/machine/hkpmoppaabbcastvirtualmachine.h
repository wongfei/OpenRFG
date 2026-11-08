// Original: volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/internal/collide/mopp/machine/hkpmoppaabbcastvirtualmachine.h
//
// Havok 2014.2.5: full declaration from rfg.pdb (havok_impl.md HK0).
#pragma once

#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/memory/memoryclasses/hkmemoryclasses.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/math/vector/hkvector4f.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/geometry/collide/shapes/hkcdshape.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/internal/collide/mopp/machine/hkpmoppvirtualmachine.h"

class hkpCdBody;
class hkpCdPointCollector;
struct hkpLinearCastCollisionInput;
struct hkpMoppAabbCastVirtualMachineQueryFloat;
struct hkpMoppAabbCastVirtualMachineQueryInt;
class hkpMoppCode;

// size 0x34
class hkpMoppAabbCastVirtualMachine : public hkpMoppVirtualMachine
{
public:
    // size 0x40
    struct hkpAabbCastInput
    {
        HK_DECLARE_NONVIRTUAL_CLASS_ALLOCATOR(hkpAabbCastInput);

        hkVector4f m_from; // 0x00
        hkVector4f m_to; // 0x10
        hkVector4f m_extents; // 0x20
        const hkpLinearCastCollisionInput* m_collisionInput; // 0x30
        const hkpCdBody* m_castBody; // 0x34
        const hkpCdBody* m_moppBody; // 0x38
    };

    HK_DECLARE_NONVIRTUAL_CLASS_ALLOCATOR(hkpMoppAabbCastVirtualMachine);
    hkpMoppAabbCastVirtualMachine(); // 0x00DB45F0
    ~hkpMoppAabbCastVirtualMachine(); // 0x00DB4600
    void aabbCast(const hkpAabbCastInput& input, hkpCdPointCollector& castCollector, hkpCdPointCollector* startCollector); // 0x00E7E1E0
public:
    void queryRayOnTree(const hkpMoppAabbCastVirtualMachineQueryInt* query, const unsigned char* PC, hkpMoppAabbCastVirtualMachineQueryFloat* const fQuery, int chunkOffset); // 0x00E7D2D0
    void addHit(unsigned int key, const unsigned int* properties, unsigned long chunkOffset_unused); // 0x00E7D160

    const hkpMoppCode* m_code; // 0x10
    float m_ItoFScale; // 0x14
    hkcdShapeType::ShapeTypeEnum m_castObjectType; // 0x18
    float m_earlyOutFraction; // 0x1C
    float m_refEarlyOutFraction; // 0x20
    hkpCdPointCollector* m_castCollector; // 0x24
    hkpCdPointCollector* m_startPointCollector; // 0x28
    const hkpAabbCastInput* m_input; // 0x2C
    int m_reindexingMask; // 0x30
};

// size 0x40
struct alignas(16) hkpMoppAabbCastVirtualMachineQueryInt
{
    HK_DECLARE_NONVIRTUAL_CLASS_ALLOCATOR(hkpMoppAabbCastVirtualMachineQueryInt);

    hkVector4f m_FtoBoffset; // 0x00
    hkVector4f m_extents; // 0x10
    float m_extentsSum3; // 0x20
    int m_shift; // 0x24
    float m_FtoBScale; // 0x28
    unsigned int m_primitiveOffset; // 0x2C
    unsigned int m_properties[1]; // 0x30
};
