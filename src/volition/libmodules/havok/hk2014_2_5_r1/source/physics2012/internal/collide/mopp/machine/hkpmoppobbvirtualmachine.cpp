// Original: volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/internal/collide/mopp/machine/hkpmoppobbvirtualmachine.cpp
//
// Havok 2014.2.5 (havok_impl.md HK3): the MOPP byte code interpreter that collects the keys of the leaves whose
// volume overlaps a box (an AABB, or the AABB around an OBB), from rfg.exe (no Havok source). The box is kept in the
// code's 16.16 fixed point frame (m_x/y/zLo/Hi); each node level works on 8-bit plane values of a frame shifted by
// m_shift and offset by m_offset. The shipped build checks the MOPP licence keycode on entry (HK_flyingcolors_mopp,
// hkpCheckKeycode); that check is not reproduced.

#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/internal/collide/mopp/machine/hkpmoppobbvirtualmachine.h"

#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/math/matrix/hktransformf.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/types/geometry/aabb/hkaabb.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/internal/collide/mopp/code/hkpmoppcode.h"

#include <intrin.h>
#include <string.h>
#include <xmmintrin.h>

namespace
{
// cvttss2si, saturated at the top: 2^31 and above give 0x7FFFFFFF instead of 0x80000000 (hkIntVector's
// float-to-int conversion)
int convertSaturated(float f)
{
    const int i = _mm_cvttss_si32(_mm_set_ss(f));
    return f >= 2147483648.0f ? ~i : i;
}

int truncate(float f)
{
    return _mm_cvttss_si32(_mm_set_ss(f));
}

// The query of a node level: the full box shifted to its 16 bits.
void setQueryFromBox(const hkpMoppObbVirtualMachine& vm, hkpMoppObbVirtualMachineQuery& query)
{
    query.m_xLo = vm.m_xLo >> 16;
    query.m_xHi = (vm.m_xHi >> 16) + 1;
    query.m_yLo = vm.m_yLo >> 16;
    query.m_yHi = (vm.m_yHi >> 16) + 1;
    query.m_zLo = vm.m_zLo >> 16;
    query.m_zHi = (vm.m_zHi >> 16) + 1;
    query.m_offset_x = 0;
    query.m_offset_y = 0;
    query.m_offset_z = 0;
    query.m_primitiveOffset = 0;
    query.m_shift = 0;
    query.m_properties[0] = 0;
}

int axisHi(const hkpMoppObbVirtualMachineQuery* q, int axis) { return (&q->m_xHi)[axis]; }
int axisLo(const hkpMoppObbVirtualMachineQuery* q, int axis) { return (&q->m_xLo)[axis]; }

// A leaf: the primitive offset plus the terminal, with the chunk in bits 8 and up when the code is reindexed
// (inline in the original).
void addLeaf(hkpMoppObbVirtualMachine& vm, const hkpMoppObbVirtualMachineQuery* query, unsigned int terminal, int chunkOffset)
{
    const unsigned int id = (((unsigned int)(chunkOffset >> 9) << 8) & (unsigned int)(int)vm.m_reindexingMask) | (query->m_primitiveOffset + terminal);
    vm.addHit(id, query->m_properties);
}
}

// 0x00D9A5C0
hkpMoppObbVirtualMachine::hkpMoppObbVirtualMachine()
{
}

// 0x00D99FB0
hkpMoppObbVirtualMachine::~hkpMoppObbVirtualMachine()
{
}

// 0x00E688E0: the AABB of the OBB (grown by the radius) in the code's frame
void hkpMoppObbVirtualMachine::generateQueryFromNode(const hkVector4f& extent, const hkTransformf& BvToWorld, const float radius, hkpMoppObbVirtualMachineQuery& query)
{
    const hkpMoppCode* code = m_code;
    const hkVector4f& offset = code->m_info.m_offset;
    const float scale = offset(3);
    const hkRotationf& r = BvToWorld.getRotation();
    float lo[3], hi[3];
    for (int i = 0; i < 3; i++)
    {
        const float t = BvToWorld.getTranslation()(i) - offset(i);
        const float e = fabsf(extent(1) * r.getColumn(1)(i)) + fabsf(extent(0) * r.getColumn(0)(i)) + fabsf(extent(2) * r.getColumn(2)(i)) + radius;
        hi[i] = (t + e) * scale;
        lo[i] = (-e + t) * scale;
    }
    m_xHi = (int)((unsigned int)convertSaturated(hi[0]) + 1);
    m_yLo = (int)((unsigned int)convertSaturated(lo[1]) - 1);
    m_xLo = (int)((unsigned int)convertSaturated(lo[0]) - 1);
    m_yHi = (int)((unsigned int)convertSaturated(hi[1]) + 1);
    m_zLo = (int)((unsigned int)convertSaturated(lo[2]) - 1);
    m_zHi = (int)((unsigned int)convertSaturated(hi[2]) + 1);
    setQueryFromBox(*this, query);
}

// 0x00E68730: the AABB in the code's frame (plain truncation)
void hkpMoppObbVirtualMachine::generateQueryFromAabb(const hkVector4f& aabbMin, const hkVector4f& aabbMax, hkpMoppObbVirtualMachineQuery& query)
{
    const hkpMoppCode* code = m_code;
    const float scale = code->m_info.m_offset(3);
    m_xLo = (int)((unsigned int)truncate((aabbMin(0) - code->m_info.m_offset(0)) * scale) - 1);
    m_xHi = (int)((unsigned int)truncate((aabbMax(0) - code->m_info.m_offset(0)) * scale) + 1);
    m_yLo = (int)((unsigned int)truncate((aabbMin(1) - code->m_info.m_offset(1)) * scale) - 1);
    m_yHi = (int)((unsigned int)truncate((aabbMax(1) - code->m_info.m_offset(1)) * scale) + 1);
    m_zLo = (int)((unsigned int)truncate((aabbMin(2) - code->m_info.m_offset(2)) * scale) - 1);
    m_zHi = (int)((unsigned int)truncate((aabbMax(2) - code->m_info.m_offset(2)) * scale) + 1);
    setQueryFromBox(*this, query);
}

// 0x00E683E0
void hkpMoppObbVirtualMachine::queryObb(const hkpMoppCode* code, const hkTransformf& BvToWorld, const hkVector4f& extent, const float radius, hkArray<hkpMoppPrimitiveInfo, hkContainerHeapAllocator>* primitives_out)
{
    m_primitives_out = primitives_out;
    m_code = code;
    hkpMoppObbVirtualMachineQuery query;
    generateQueryFromNode(extent, BvToWorld, radius, query);
    const unsigned char* PC = code->m_data.begin();
    m_reindexingMask = *PC == 0x0D ? -1 : 0; // a code starting with the reindexing opcode keeps the chunk in the keys
    queryAabbOnTree(&query, PC, 0);
}

// 0x00E685B0
void hkpMoppObbVirtualMachine::queryAabb(const hkpMoppCode* code, const hkAabb& aabb, hkArray<hkpMoppPrimitiveInfo, hkContainerHeapAllocator>* primitives_out)
{
    m_primitives_out = primitives_out;
    m_code = code;
    hkpMoppObbVirtualMachineQuery query;
    generateQueryFromAabb(aabb.m_min, aabb.m_max, query);
    const unsigned char* PC = code->m_data.begin();
    m_reindexingMask = *PC == 0x0D ? -1 : 0;
    queryAabbOnTree(&query, PC, 0);
}

// 0x00E67C00
// The interpreter. A node that needs both children recurses into one and continues with the other; data opcodes
// (rescale, primitive offset, properties) make a local copy of the query first. A leaf adds its key (the primitive
// offset plus the terminal, with the chunk in bits 8 and up when the code is reindexed) and ends this branch.
void hkpMoppObbVirtualMachine::queryAabbOnTree(const hkpMoppObbVirtualMachineQuery* query, const unsigned char* PC, int chunkOffset)
{
    // the local query, with one word after it: the properties opcodes of index 1 to 3 write past m_properties
    // (only index 0 is used: MAX_PRIMITIVE_PROPERTIES is 1)
    struct alignas(16) LocalQuery
    {
        hkpMoppObbVirtualMachineQuery q;
        unsigned int extra[4];
    } local;
    unsigned int* localWords = reinterpret_cast<unsigned int*>(&local.q);

    while (true)
    {
        const unsigned int op = PC[0];
        const unsigned int b1 = PC[1];
        const unsigned int b2 = PC[2];
        const unsigned int b3 = PC[3];
        int lo, hi, queryLo, queryHi;
        switch (op)
        {
        case 0x00: // return
            return;

        case 0x01: case 0x02: case 0x03: case 0x04: // rescale: a finer frame (op more bits) offset by the next node
        {
            local.q.m_offset_x = (query->m_offset_x + (int)b1) << op;
            local.q.m_offset_y = (query->m_offset_y + (int)b2) << op;
            local.q.m_offset_z = (query->m_offset_z + (int)b3) << op;
            local.q.m_shift = query->m_shift + op;
            const int shift = 16 - local.q.m_shift;
            local.q.m_xLo = (m_xLo >> shift) - local.q.m_offset_x;
            local.q.m_yLo = (m_yLo >> shift) - local.q.m_offset_y;
            local.q.m_zLo = (m_zLo >> shift) - local.q.m_offset_z;
            local.q.m_xHi = (m_xHi >> shift) - local.q.m_offset_x + 1;
            local.q.m_yHi = (m_yHi >> shift) - local.q.m_offset_y + 1;
            local.q.m_zHi = (m_zHi >> shift) - local.q.m_offset_z + 1;
            local.q.m_properties[0] = query->m_properties[0];
            local.q.m_primitiveOffset = query->m_primitiveOffset;
            query = &local.q;
            PC += 4;
            continue;
        }

        case 0x05: // jump8
            PC += b1 + 2;
            continue;
        case 0x06: // jump16
            PC += 3 + (b1 << 8) + b2;
            continue;
        case 0x07: // jump24
            PC += 4 + (((b1 << 8) + b2) << 8) + b3;
            continue;
        case 0x08: // jump32
            PC += 5 + (((((b1 << 8) + b2) << 8) + b3) << 8) + PC[4];
            continue;

        case 0x09: // primitive offset += 8 bits
            if (query != &local.q)
            {
                memcpy(&local.q, query, sizeof(hkpMoppObbVirtualMachineQuery));
                query = &local.q;
            }
            local.q.m_primitiveOffset += b1;
            PC += 2;
            continue;
        case 0x0A: // primitive offset += 16 bits
            if (query != &local.q)
            {
                memcpy(&local.q, query, sizeof(hkpMoppObbVirtualMachineQuery));
                query = &local.q;
            }
            local.q.m_primitiveOffset += (b1 << 8) + b2;
            PC += 3;
            continue;
        case 0x0B: // primitive offset = 32 bits
        {
            const unsigned int value = (((((b1 << 8) + b2) << 8) + b3) << 8) + PC[4];
            if (query != &local.q)
            {
                memcpy(&local.q, query, sizeof(hkpMoppObbVirtualMachineQuery));
                query = &local.q;
            }
            local.q.m_primitiveOffset = value;
            PC += 5;
            continue;
        }
        case 0x0D: // reindexing marker (5 bytes)
            PC += 5;
            continue;

        case 0x0C: // jump to a 512-byte chunk
            chunkOffset = ((b1 << 8) | b2) << 9;
            PC = m_code->m_data.begin() + chunkOffset;
            continue;
        case 0x70: // jump to a 32-bit chunk offset
            chunkOffset = (int)((((((b1 << 8) + b2) << 8) + b3) << 8) + PC[4]);
            PC = m_code->m_data.begin() + chunkOffset;
            continue;

        case 0x10: case 0x11: case 0x12: // split on x/y/z: left up to b1, right from b2, right child at +b3
        {
            const int axis = op - 0x10;
            const int qLo = axisLo(query, axis);
            PC += 4;
            if (axisHi(query, axis) > (int)b2)
            {
                PC += b3;
                if (qLo < (int)b1)
                {
                    queryAabbOnTree(query, PC - b3, chunkOffset);
                }
                continue;
            }
            if (qLo < (int)b1)
            {
                continue;
            }
            return;
        }

        // splits on the diagonals: the plane values are scaled by 2 (two axes) or 3, the negative directions offset
        case 0x13: queryLo = query->m_zLo + query->m_yLo; queryHi = query->m_zHi + query->m_yHi; lo = b1 * 2; hi = b2 * 2; goto split;
        case 0x14: queryLo = query->m_yLo - query->m_zHi; queryHi = query->m_yHi - query->m_zLo; lo = b1 * 2 - 0xFF; hi = b2 * 2 - 0xFF; goto split;
        case 0x15: queryLo = query->m_zLo + query->m_xLo; queryHi = query->m_zHi + query->m_xHi; lo = b1 * 2; hi = b2 * 2; goto split;
        case 0x16: queryLo = query->m_xLo - query->m_zHi; queryHi = query->m_xHi - query->m_zLo; lo = b1 * 2 - 0xFF; hi = b2 * 2 - 0xFF; goto split;
        case 0x17: queryLo = query->m_yLo + query->m_xLo; queryHi = query->m_yHi + query->m_xHi; lo = b1 * 2; hi = b2 * 2; goto split;
        case 0x18: queryLo = query->m_xLo - query->m_yHi; queryHi = query->m_xHi - query->m_yLo; lo = b1 * 2 - 0xFF; hi = b2 * 2 - 0xFF; goto split;
        case 0x19: queryLo = query->m_zLo + query->m_yLo + query->m_xLo; queryHi = query->m_zHi + query->m_yHi + query->m_xHi; lo = b1 * 3; hi = b2 * 3; goto split;
        case 0x1A: queryLo = query->m_yLo - query->m_zHi + query->m_xLo; queryHi = query->m_yHi - query->m_zLo + query->m_xHi; lo = b1 * 3 - 0xFF; hi = b2 * 3 - 0xFF; goto split;
        case 0x1B: queryLo = query->m_zLo - query->m_yHi + query->m_xLo; queryHi = query->m_zHi - query->m_yLo + query->m_xHi; lo = b1 * 3 - 0xFF; hi = b2 * 3 - 0xFF; goto split;
        case 0x1C: queryLo = query->m_xLo - query->m_zHi - query->m_yHi; queryHi = query->m_xHi - query->m_zLo - query->m_yLo; lo = b1 * 3 - 0x1FE; hi = b2 * 3 - 0x1FE; goto split;

        case 0x20: case 0x21: case 0x22: // single plane on x/y/z: left up to b1 (at +3), right after it at +3+b2
        {
            const int axis = op - 0x20;
            PC += 3;
            if (axisHi(query, axis) > (int)b1)
            {
                PC += b2;
                if (axisLo(query, axis) <= (int)b1)
                {
                    queryAabbOnTree(query, PC - b2, chunkOffset);
                }
            }
            continue;
        }

        case 0x23: case 0x24: case 0x25: // split on x/y/z with 16-bit child offsets
        {
            const int axis = op - 0x23;
            const int qLo = axisLo(query, axis);
            const int leftJump = (b3 << 8) + PC[4];
            const int rightJump = (PC[5] << 8) + PC[6];
            PC += 7;
            if (axisHi(query, axis) > (int)b2)
            {
                PC += rightJump;
                if (qLo < (int)b1)
                {
                    queryAabbOnTree(query, PC + (leftJump - rightJump), chunkOffset);
                }
                continue;
            }
            if (qLo >= (int)b1)
            {
                return;
            }
            PC += leftJump;
            continue;
        }

        case 0x26: case 0x27: case 0x28: // bound on x/y/z in the node frame: the box must reach [b1, b2)
        {
            const int axis = op - 0x26;
            if (axisHi(query, axis) < (int)b1)
            {
                return;
            }
            if (axisLo(query, axis) >= (int)b2)
            {
                return;
            }
            PC += 3;
            continue;
        }

        case 0x29: case 0x2A: case 0x2B: // bound on x/y/z against the full precision box (24-bit values)
        {
            const int axis = op - 0x29;
            const int boundLo = (int)((((b1 << 8) + b2) << 8) + b3);
            if ((&m_xHi)[axis] < boundLo)
            {
                return;
            }
            const int boundHi = (int)((((PC[4] << 8) + PC[5]) << 8) + PC[6]);
            if ((&m_xLo)[axis] > boundHi)
            {
                return;
            }
            PC += 7;
            continue;
        }

        // leaves
        case 0x30: case 0x31: case 0x32: case 0x33: case 0x34: case 0x35: case 0x36: case 0x37:
        case 0x38: case 0x39: case 0x3A: case 0x3B: case 0x3C: case 0x3D: case 0x3E: case 0x3F:
        case 0x40: case 0x41: case 0x42: case 0x43: case 0x44: case 0x45: case 0x46: case 0x47:
        case 0x48: case 0x49: case 0x4A: case 0x4B: case 0x4C: case 0x4D: case 0x4E: case 0x4F:
            addLeaf(*this, query, op - 0x30, chunkOffset);
            return;
        case 0x50:
            addLeaf(*this, query, b1, chunkOffset);
            return;
        case 0x51:
            addLeaf(*this, query, (b1 << 8) + b2, chunkOffset);
            return;
        case 0x52:
            addLeaf(*this, query, (((b1 << 8) + b2) << 8) + b3, chunkOffset);
            return;
        case 0x53:
            addLeaf(*this, query, (((((b1 << 8) + b2) << 8) + b3) << 8) + PC[4], chunkOffset);
            return;

        // properties: written into the local query, whose property 0 survives the copy of the query
        case 0x60: case 0x61: case 0x62: case 0x63:
            localWords[13 + (op - 0x60)] = b1;
            PC += 2;
            goto properties;
        case 0x64: case 0x65: case 0x66: case 0x67:
            localWords[13 + (op - 0x64)] = (b1 << 8) + b2;
            PC += 3;
            goto properties;
        case 0x68: case 0x69: case 0x6A: case 0x6B:
            localWords[13 + (op - 0x68)] = (((((b1 << 8) + b2) << 8) + b3) << 8) + PC[4];
            PC += 5;
            goto properties;

        default: // not a MOPP opcode
            __debugbreak();
            return;
        }

    split:
        // the diagonal splits: left child at +4 when the box reaches below lo, right child at +4+b3 when it reaches
        // above hi
        PC += 4;
        if (queryHi > hi)
        {
            if (queryLo < lo)
            {
                queryAabbOnTree(query, PC, chunkOffset);
            }
            PC += b3;
            continue;
        }
        if (queryLo < lo)
        {
            continue;
        }
        return;

    properties:
    {
        // original quirk kept: property 0 is read before the copy, so the other properties are lost (and property 0
        // keeps the local's old value) when the query was not local yet
        const unsigned int property0 = local.q.m_properties[0];
        if (query != &local.q)
        {
            memcpy(&local.q, query, sizeof(hkpMoppObbVirtualMachineQuery));
            query = &local.q;
        }
        local.q.m_properties[0] = property0;
        continue;
    }
    }
}
