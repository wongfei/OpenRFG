// Original: volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/internal/collide/mopp/machine/hkpmoppaabbcastvirtualmachine.cpp
//
// Havok 2014.2.5 (havok_impl.md HK6): the MOPP byte code interpreter for a swept box (the linear cast of a body
// against a MOPP shape), from rfg.exe (no Havok source). It is the long ray machine's walk with the box's half extents
// (in the node frame) widening every split and bound: the cast body's center moves from m_from to m_to, and each leaf
// in reach gets the dispatcher's linear cast against the child shape. A closer hit (the cast collector's early out
// distance) shortens the rest of the walk. The shipped build checks the MOPP licence keycode on entry
// (HK_flyingcolors_mopp, hkpCheckKeycode); that check is not reproduced.

#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/internal/collide/mopp/machine/hkpmoppaabbcastvirtualmachine.h"

#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/memory/allocator/lifo/hklifoallocator.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/memory/router/hkmemoryrouter.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/agent/collidable/hkpcdbody.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/agent/hkpcollisioninput.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/agent/query/hkplinearcastcollisioninput.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/agent/query/hkpcdpointcollector.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/dispatch/hkpcollisiondispatcher.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/filter/hkpcollisionfilter.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/shape/compound/collection/hkpshapecollection.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/shape/compound/tree/mopp/hkpmoppbvtreeshape.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/shape/hkpshape.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/shape/hkpshapebuffer.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/shape/hkpshapecontainer.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/internal/collide/mopp/code/hkpmoppcode.h"

#include <intrin.h>

// The swept center in the current node frame: its two ends.
struct alignas(16) hkpMoppAabbCastVirtualMachineQueryFloat
{
    hkVector4f m_rayEnds[2];
};

namespace
{
typedef hkpMoppAabbCastVirtualMachineQueryInt QueryInt;
typedef hkpMoppAabbCastVirtualMachineQueryFloat QueryFloat;

// a + t * (b - a), in vector order: t * (b - a) + a
hkVector4f lerp(const hkVector4f& a, const hkVector4f& b, float t)
{
    hkVector4f d; d.setSub(b, a);
    hkVector4f tv; tv.setAll(t);
    hkVector4f r; r.setMul(tv, d);
    r.add(a);
    return r;
}

// The end of this level's segment moved to the machine's early out fraction of the whole cast, when that went down
// since the last move. Returns whether it moved.
bool updateEarlyOut(hkpMoppAabbCastVirtualMachine& vm, const QueryInt* query, QueryFloat* fQuery)
{
    const float earlyOut = vm.m_earlyOutFraction;
    if (!(vm.m_refEarlyOutFraction > earlyOut))
    {
        return false;
    }
    vm.m_refEarlyOutFraction = earlyOut;
    hkVector4f end = lerp(vm.m_input->m_from, vm.m_input->m_to, earlyOut);
    end.sub(vm.m_code->m_info.m_offset);
    hkVector4f scale; scale.setAll(query->m_FtoBScale);
    end.setMul(scale, end);
    end.sub(query->m_FtoBoffset);
    fQuery->m_rayEnds[1] = end;
    return true;
}

// The segment clipped to the slab [lo, hi] of an axis (the ends are interpolated from the unclipped segment); false
// when it misses the slab.
bool clipToSlab(QueryFloat* fQuery, int axis, float lo, float hi)
{
    const float a0 = fQuery->m_rayEnds[0](axis);
    const float a1 = fQuery->m_rayEnds[1](axis);
    int farEnd;
    if (a1 <= a0)
    {
        if (a0 < lo || hi < a1)
        {
            return false;
        }
        farEnd = 1;
    }
    else
    {
        if (a1 < lo || hi < a0)
        {
            return false;
        }
        farEnd = 0;
    }
    const hkVector4f start = fQuery->m_rayEnds[0];
    const hkVector4f end = fQuery->m_rayEnds[1];
    float d0 = a0 - hi;
    float d1 = a1 - hi;
    if (0.0f > d1 * d0)
    {
        fQuery->m_rayEnds[1 - farEnd] = lerp(start, end, d0 / (d0 - d1));
    }
    d0 = a0 - lo;
    d1 = a1 - lo;
    if (0.0f > d1 * d0)
    {
        fQuery->m_rayEnds[farEnd] = lerp(start, end, d0 / (d0 - d1));
    }
    return true;
}
}

// 0x00E7E1E0
// The cast body's center from m_from to m_to in the code's frame scaled to 16.16, the box's half extents with it.
void hkpMoppAabbCastVirtualMachine::aabbCast(const hkpAabbCastInput& input, hkpCdPointCollector& castCollector, hkpCdPointCollector* startCollector)
{
    m_input = &input;
    m_code = static_cast<const hkMoppBvTreeShapeBase*>(input.m_moppBody->getShape())->m_code;
    m_startPointCollector = startCollector;
    m_castCollector = &castCollector;
    m_earlyOutFraction = 1.0f;
    m_refEarlyOutFraction = 1.0f;
    m_castObjectType = (hkcdShapeType::ShapeTypeEnum)input.m_castBody->getShape()->m_type.m_storage;
    const hkVector4f& offset = m_code->m_info.m_offset;
    const unsigned char* PC = m_code->m_data.begin();

    alignas(16) QueryInt query;
    query.m_FtoBoffset.setZero();
    m_ItoFScale = 1.0f / offset(3);
    query.m_FtoBScale = offset(3) * (1.0f / 65536.0f); // 0x37800000
    hkVector4f scale; scale.setAll(query.m_FtoBScale);
    query.m_extents.setMul(input.m_extents, scale);
    // the extents projected on a diagonal (x + y + z) plane's scale: ((y + x) + z) * 3
    query.m_extentsSum3 = ((query.m_extents(1) + query.m_extents(0)) + query.m_extents(2)) * 3.0f;
    query.m_shift = 0;
    query.m_primitiveOffset = 0;
    query.m_properties[0] = 0;

    QueryFloat fQuery;
    fQuery.m_rayEnds[0].setSub(input.m_from, offset);
    fQuery.m_rayEnds[0].mul(scale);
    fQuery.m_rayEnds[1].setSub(input.m_to, offset);
    fQuery.m_rayEnds[1].mul(scale);
    m_reindexingMask = *PC == 0x0D ? -1 : 0; // a code starting with the reindexing opcode keeps the chunk in the keys
    queryRayOnTree(&query, PC, &fQuery, 0);
}

// 0x00E7D160: a leaf's child through the collision filter, cast by the dispatcher's linear cast function
void hkpMoppAabbCastVirtualMachine::addHit(unsigned int key, const unsigned int* properties, unsigned long chunkOffset_unused)
{
    const hkpShapeCollection* collection = static_cast<const hkpMoppBvTreeShape*>(m_input->m_moppBody->getShape())->getShapeCollection();
    const hkpShapeContainer* container = collection ? static_cast<const hkpShapeContainer*>(collection) : nullptr;
    hkLifoAllocator& stack = hkMemoryRouter::getInstance().stack();
    hkpShapeBufferStorage* buffer = static_cast<hkpShapeBufferStorage*>(stack.fastBlockAlloc(HK_SHAPE_BUFFER_SIZE));
    const hkpShape* child = container->getChildShape(key, *buffer);
    const hkpLinearCastCollisionInput& collisionInput = *m_input->m_collisionInput;
    const hkpShapeCollectionFilter* filter = collisionInput.m_filter.val();
    if (filter->isCollisionEnabled(collisionInput, *m_input->m_castBody, *m_input->m_moppBody, *container, key).m_bool)
    {
        hkpCdBody body(m_input->m_moppBody);
        body.setShape(child, key);
        const hkpCollisionDispatcher* dispatcher = collisionInput.m_dispatcher.val();
        dispatcher->getLinearCastFunc(m_castObjectType, (hkcdShapeType::ShapeTypeEnum)child->m_type.m_storage)(*m_input->m_castBody, body, collisionInput, *m_castCollector, m_startPointCollector);
        // MINSS: the old fraction unless the collector's is smaller (or NaN)
        const float earlyOut = m_castCollector->m_earlyOutDistance;
        m_earlyOutFraction = m_earlyOutFraction < earlyOut ? m_earlyOutFraction : earlyOut;
    }
    stack.fastBlockFree(buffer, HK_SHAPE_BUFFER_SIZE);
}

// 0x00E7D2D0
// The interpreter (see hkpMoppLongRayVirtualMachine::queryRayOnTree): a split's children are reached by the segment
// widened by the extents along the split axis (the diagonal planes by the extents projected on them).
void hkpMoppAabbCastVirtualMachine::queryRayOnTree(const hkpMoppAabbCastVirtualMachineQueryInt* query, const unsigned char* PC, hkpMoppAabbCastVirtualMachineQueryFloat* const fQuery, int chunkOffset)
{
    // the local query, with words after it: the properties opcodes of index 1 to 3 write past m_properties (only
    // index 0 is used: MAX_PRIMITIVE_PROPERTIES is 1)
    struct alignas(16) LocalQuery
    {
        QueryInt q;
        unsigned int extra[4];
    } localStorage;
    QueryInt& local = localStorage.q;
    unsigned int* const localProperties = local.m_properties;

    while (true)
    {
        const unsigned int op = PC[0];
        const unsigned int b1 = PC[1];
        const unsigned int b2 = PC[2];
        const unsigned int b3 = PC[3];
        int axis = 999; // none: the diagonal planes do not cut the early out short
        float a0, a1;
        float rightLimit; // the segment wholly below it reaches only the left child (b2 less the extents)
        float leftLimit; // the segment wholly above it reaches only the right child (b1 plus the extents)
        int leftOffset, rightOffset;
        const unsigned char* nodeEnd;
        switch (op)
        {
        case 0x00:
            return;

        case 0x01: case 0x02: case 0x03: case 0x04: // rescale
        {
            hkVector4f o; o.set((float)(int)b1, (float)(int)b2, (float)(int)b3, 0.0f);
            fQuery->m_rayEnds[0].sub(o);
            fQuery->m_rayEnds[1].sub(o);
            const float s = (float)(1 << op);
            hkVector4f sv; sv.setAll(s);
            fQuery->m_rayEnds[0].setMul(sv, fQuery->m_rayEnds[0]);
            fQuery->m_rayEnds[1].setMul(sv, fQuery->m_rayEnds[1]);
            local.m_FtoBoffset.setAdd(o, query->m_FtoBoffset);
            local.m_FtoBoffset.mul(sv);
            local.m_shift = query->m_shift + op;
            PC += 4;
            local.m_FtoBScale = query->m_FtoBScale * s;
            local.m_extents.setMul(query->m_extents, sv);
            local.m_extentsSum3 = s * query->m_extentsSum3;
            local.m_properties[0] = query->m_properties[0];
            local.m_primitiveOffset = query->m_primitiveOffset;
            query = &local;
            continue;
        }

        case 0x05:
            PC += b1 + 2;
            continue;
        case 0x06:
            PC += 3 + (b1 << 8) + b2;
            continue;
        case 0x07:
            PC += 4 + (((b1 << 8) + b2) << 8) + b3;
            continue;
        case 0x08:
            PC += 5 + (((((b1 << 8) + b2) << 8) + b3) << 8) + PC[4];
            continue;

        case 0x09:
            if (query != &local)
            {
                local = *query;
                query = &local;
            }
            local.m_primitiveOffset += b1;
            PC += 2;
            continue;
        case 0x0A:
            if (query != &local)
            {
                local = *query;
                query = &local;
            }
            local.m_primitiveOffset += (b1 << 8) + b2;
            PC += 3;
            continue;
        case 0x0B:
        {
            const unsigned int value = (((((b1 << 8) + b2) << 8) + b3) << 8) + PC[4];
            if (query != &local)
            {
                local = *query;
                query = &local;
            }
            local.m_primitiveOffset = value;
            PC += 5;
            continue;
        }
        case 0x0D:
            PC += 5;
            continue;

        case 0x0C:
            chunkOffset = ((b1 << 8) | b2) << 9;
            PC = m_code->m_data.begin() + chunkOffset;
            continue;
        case 0x70:
            chunkOffset = (int)((((((b1 << 8) + b2) << 8) + b3) << 8) + PC[4]);
            PC = m_code->m_data.begin() + chunkOffset;
            continue;

        case 0x10: case 0x11: case 0x12: // split on x/y/z: left up to b2, right from b1, right child at +b3
            axis = op - 0x10;
            a0 = fQuery->m_rayEnds[0](axis);
            a1 = fQuery->m_rayEnds[1](axis);
            rightLimit = (float)(int)b2 - query->m_extents(axis);
            leftLimit = (float)(int)b1 + query->m_extents(axis);
            leftOffset = 0;
            rightOffset = b3;
            nodeEnd = PC + 4;
            break;

        // the diagonal splits: the ends projected on the plane normal (in this float order), the plane values scaled,
        // the extents projected (twice the two extents, or their sum times three)
        case 0x13: case 0x14: case 0x15: case 0x16: case 0x17: case 0x18:
        {
            const hkVector4f& r0 = fQuery->m_rayEnds[0];
            const hkVector4f& r1 = fQuery->m_rayEnds[1];
            const hkVector4f& e = query->m_extents;
            float ext;
            float bias = 0.0f;
            switch (op)
            {
            case 0x13: a0 = r0(2) + r0(1); a1 = r1(2) + r1(1); ext = (e(2) + e(1)) * 2.0f; break;
            case 0x14: a0 = r0(1) - r0(2); a1 = r1(1) - r1(2); ext = (e(2) + e(1)) * 2.0f; bias = 255.0f; break;
            case 0x15: a0 = r0(2) + r0(0); a1 = r1(2) + r1(0); ext = (e(2) + e(0)) * 2.0f; break;
            case 0x16: a0 = r0(0) - r0(2); a1 = r1(0) - r1(2); ext = (e(2) + e(0)) * 2.0f; bias = 255.0f; break;
            case 0x17: a0 = r0(1) + r0(0); a1 = r1(1) + r1(0); ext = (e(1) + e(0)) * 2.0f; break;
            default: a0 = r0(0) - r0(1); a1 = r1(0) - r1(1); ext = (e(1) + e(0)) * 2.0f; bias = 255.0f; break;
            }
            if (bias != 0.0f)
            {
                rightLimit = ((float)(int)b2 * 2.0f - 255.0f) - ext;
                leftLimit = ((float)(int)b1 * 2.0f - 255.0f) + ext;
            }
            else
            {
                rightLimit = (float)(int)b2 * 2.0f - ext;
                leftLimit = (float)(int)b1 * 2.0f + ext;
            }
            leftOffset = 0;
            rightOffset = b3;
            nodeEnd = PC + 4;
            break;
        }
        case 0x19: case 0x1A: case 0x1B: case 0x1C:
        {
            const hkVector4f& r0 = fQuery->m_rayEnds[0];
            const hkVector4f& r1 = fQuery->m_rayEnds[1];
            float bias;
            switch (op)
            {
            case 0x19: a0 = r0(1) + r0(0) + r0(2); a1 = r1(1) + r1(0) + r1(2); bias = 0.0f; break;
            case 0x1A: a0 = r0(1) + r0(0) - r0(2); a1 = r1(1) + r1(0) - r1(2); bias = 255.0f; break;
            case 0x1B: a0 = r0(0) - r0(1) + r0(2); a1 = r1(0) - r1(1) + r1(2); bias = 255.0f; break;
            default: a0 = r0(0) - r0(1) - r0(2); a1 = r1(0) - r1(1) - r1(2); bias = 510.0f; break;
            }
            if (bias != 0.0f)
            {
                rightLimit = ((float)(int)b2 * 3.0f - bias) - query->m_extentsSum3;
                leftLimit = ((float)(int)b1 * 3.0f - bias) + query->m_extentsSum3;
            }
            else
            {
                rightLimit = (float)(int)b2 * 3.0f - query->m_extentsSum3;
                leftLimit = (float)(int)b1 * 3.0f + query->m_extentsSum3;
            }
            leftOffset = 0;
            rightOffset = b3;
            nodeEnd = PC + 4;
            break;
        }

        case 0x20: case 0x21: case 0x22: // single plane b1: left child up to b1 + 1 (at +3), right from b1 at +3+b2
        {
            axis = op - 0x20;
            const float ext = query->m_extents(axis);
            a0 = fQuery->m_rayEnds[0](axis);
            a1 = fQuery->m_rayEnds[1](axis);
            rightLimit = (float)(int)b1 - ext;
            leftLimit = ext + ((float)(int)b1 + 1.0f);
            leftOffset = 0;
            rightOffset = b2;
            nodeEnd = PC + 3;
            break;
        }

        case 0x23: case 0x24: case 0x25: // split with 16-bit child offsets
            axis = op - 0x23;
            a0 = fQuery->m_rayEnds[0](axis);
            a1 = fQuery->m_rayEnds[1](axis);
            leftLimit = (float)(int)b1 + query->m_extents(axis);
            rightLimit = (float)(int)b2 - query->m_extents(axis);
            leftOffset = (b3 << 8) + PC[4];
            rightOffset = (PC[5] << 8) + PC[6];
            nodeEnd = PC + 7;
            break;

        case 0x26: case 0x27: case 0x28: // bound on x/y/z in the node frame, widened by the extents
        {
            const int boundAxis = op - 0x26;
            const float ext = query->m_extents(boundAxis);
            if (!clipToSlab(fQuery, boundAxis, (float)(int)b1 - ext, (float)(int)b2 + ext))
            {
                return;
            }
            PC += 3;
            continue;
        }

        case 0x29: case 0x2A: case 0x2B: // bound on x/y/z in 24-bit code units, brought into the node frame
        {
            const int boundAxis = op - 0x29;
            const float frameOffset = query->m_FtoBoffset(boundAxis);
            const float ext = query->m_extents(boundAxis);
            const float boundLo = ((float)(int)((((b1 << 8) + b2) << 8) + b3) * m_ItoFScale * query->m_FtoBScale - frameOffset) - ext;
            const float boundHi = ((float)(int)((((PC[4] << 8) + PC[5]) << 8) + PC[6]) * m_ItoFScale * query->m_FtoBScale - frameOffset) + ext;
            if (!clipToSlab(fQuery, boundAxis, boundLo, boundHi))
            {
                return;
            }
            PC += 7;
            continue;
        }

        // leaves: the key (the primitive offset plus the terminal, with the chunk in bits 8 and up when the code is
        // reindexed)
        case 0x30: case 0x31: case 0x32: case 0x33: case 0x34: case 0x35: case 0x36: case 0x37:
        case 0x38: case 0x39: case 0x3A: case 0x3B: case 0x3C: case 0x3D: case 0x3E: case 0x3F:
        case 0x40: case 0x41: case 0x42: case 0x43: case 0x44: case 0x45: case 0x46: case 0x47:
        case 0x48: case 0x49: case 0x4A: case 0x4B: case 0x4C: case 0x4D: case 0x4E: case 0x4F:
        case 0x50: case 0x51: case 0x52: case 0x53:
        {
            unsigned int terminal;
            switch (op)
            {
            case 0x50: terminal = b1; break;
            case 0x51: terminal = (b1 << 8) + b2; break;
            case 0x52: terminal = (((b1 << 8) + b2) << 8) + b3; break;
            case 0x53: terminal = (((b1 << 8) + b2) << 16) + PC[4] + (b3 << 8); break;
            default: terminal = op - 0x30; break;
            }
            const unsigned int key = (((unsigned int)(chunkOffset >> 9) << 8) & (unsigned int)m_reindexingMask) | (query->m_primitiveOffset + terminal);
            addHit(key, query->m_properties, (unsigned long)chunkOffset);
            return;
        }

        // properties: written into the local query, which keeps them when it copies the rest of the query
        case 0x60: case 0x61: case 0x62: case 0x63:
            localProperties[op - 0x60] = b1;
            PC += 2;
            goto properties;
        case 0x64: case 0x65: case 0x66: case 0x67:
            localProperties[op - 0x64] = (b1 << 8) + b2;
            PC += 3;
            goto properties;
        case 0x68: case 0x69: case 0x6A: case 0x6B:
            localProperties[op - 0x68] = (((((b1 << 8) + b2) << 8) + b3) << 8) + PC[4];
            PC += 5;
            goto properties;

        default: // not a MOPP opcode
            __debugbreak();
            return;
        }

        // a split: nodeEnd is the end of the node, the left child at +leftOffset, the right child at +rightOffset
        if (rightLimit > a1 && rightLimit > a0)
        {
            PC = nodeEnd + leftOffset; // only the left child
            continue;
        }
        PC = nodeEnd + rightOffset;
        if (a0 > leftLimit && a1 > leftLimit)
        {
            continue; // only the right child
        }
        {
            // both children: the near one first, on a copy of the segment clipped to it; then the far one, on this
            // segment clipped to it and to the early out
            const float d0left = a0 - leftLimit;
            const float d1left = a1 - leftLimit;
            const float d0right = a0 - rightLimit;
            const float d1right = a1 - rightLimit;
            alignas(16) QueryFloat nearQuery = *fQuery;
            if (d1left > d0left)
            {
                // moving up: the left child first
                if (0.0f > d1left * d0left)
                {
                    nearQuery.m_rayEnds[1] = lerp(fQuery->m_rayEnds[0], fQuery->m_rayEnds[1], d0left / (d0left - d1left));
                }
                queryRayOnTree(query, PC + (leftOffset - rightOffset), &nearQuery, chunkOffset);
                if (0.0f > d1right * d0right)
                {
                    fQuery->m_rayEnds[0] = lerp(fQuery->m_rayEnds[0], fQuery->m_rayEnds[1], d0right / (d0right - d1right));
                }
                if (updateEarlyOut(*this, query, fQuery) && axis < 3 && rightLimit > fQuery->m_rayEnds[1](axis))
                {
                    return;
                }
            }
            else
            {
                // moving down: the right child first
                if (0.0f > d1right * d0right)
                {
                    nearQuery.m_rayEnds[1] = lerp(fQuery->m_rayEnds[0], fQuery->m_rayEnds[1], d0right / (d0right - d1right));
                }
                queryRayOnTree(query, PC, &nearQuery, chunkOffset);
                if (0.0f > d1left * d0left)
                {
                    fQuery->m_rayEnds[0] = lerp(fQuery->m_rayEnds[0], fQuery->m_rayEnds[1], d0left / (d0left - d1left));
                }
                if (updateEarlyOut(*this, query, fQuery) && axis < 3 && fQuery->m_rayEnds[1](axis) > leftLimit)
                {
                    return;
                }
                PC += leftOffset - rightOffset;
            }
        }
        continue;

    properties:
        if (query != &local)
        {
            local.m_FtoBoffset = query->m_FtoBoffset;
            local.m_extents = query->m_extents;
            local.m_extentsSum3 = query->m_extentsSum3;
            local.m_shift = query->m_shift;
            local.m_FtoBScale = query->m_FtoBScale;
            local.m_primitiveOffset = query->m_primitiveOffset;
            query = &local;
        }
        continue;
    }
}
