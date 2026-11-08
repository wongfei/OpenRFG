// Original: volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/internal/collide/mopp/machine/hkpmopplongrayvirtualmachine.cpp
//
// Havok 2014.2.5 (havok_impl.md HK3): the MOPP byte code interpreter for a ray: it walks the tree front to back along
// the ray, clipping the ray to each child, and casts the ray against the leaves' shapes; a closer hit shortens the
// rest of the walk. From rfg.exe (no Havok source). The ray is kept in floats in the frame of the current node
// (QueryFloat: its two ends; QueryInt: the frame's offset and scale from the code's frame). The shipped build checks
// the MOPP licence keycode on entry (HK_flyingcolors_mopp, hkpCheckKeycode); that check is not reproduced.

#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/internal/collide/mopp/machine/hkpmopplongrayvirtualmachine.h"

#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/memory/allocator/lifo/hklifoallocator.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/memory/router/hkmemoryrouter.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/agent/collidable/hkpcdbody.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/shape/hkpshape.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/shape/hkpshapebuffer.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/shape/hkpshapecontainer.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/shape/query/hkprayhitcollector.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/shape/query/hkprayshapecollectionfilter.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/shape/query/hkpshaperaycastoutput.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/internal/collide/mopp/code/hkpmoppcode.h"

#include <intrin.h>

namespace
{
// a + t * (b - a), in vector order: t * (b - a) + a
hkVector4f lerp(const hkVector4f& a, const hkVector4f& b, float t)
{
    hkVector4f d; d.setSub(b, a);
    hkVector4f tv; tv.setAll(t);
    hkVector4f r; r.setMul(tv, d);
    r.add(a);
    return r;
}

void queryLongRaySetup(hkpMoppLongRayVirtualMachine& vm, const hkpMoppCode* code, const hkpShapeRayCastInput& input);
bool updateEarlyOut(const hkpMoppLongRayVirtualMachine& vm, const hkpMoppLongRayVirtualMachine::QueryInt* query, hkpMoppLongRayVirtualMachine::QueryFloat* fQuery);
bool clipToSlab(hkpMoppLongRayVirtualMachine::QueryFloat* fQuery, int axis, float lo, float hi);
void castLeaf(hkpMoppLongRayVirtualMachine& vm, const hkpMoppLongRayVirtualMachine::QueryInt* query, unsigned int terminal, int chunkOffset);
}

// 0x00D99F80
hkpMoppLongRayVirtualMachine::hkpMoppLongRayVirtualMachine()
{
}

// 0x00D99FA0
hkpMoppLongRayVirtualMachine::~hkpMoppLongRayVirtualMachine()
{
}

// 0x00E69A30
// The closest hit of the ray (rayResult's hit fraction is the starting limit; the key of the hit goes to its key
// level).
hkBool hkpMoppLongRayVirtualMachine::queryLongRay(const hkpShapeContainer* collection, const hkpMoppCode* code, const hkpShapeRayCastInput& input, hkpShapeRayCastOutput& rayResult)
{
    m_collection = collection;
    m_earlyOutHitFraction = 1.0f;
    m_code = code;
    m_rayResultPtr = &rayResult;
    queryLongRaySetup(*this, code, input);
    return m_hitFound;
}

// 0x00E69B20: every hit of the ray, through the collector (whose early out fraction limits the walk)
void hkpMoppLongRayVirtualMachine::queryLongRay(const hkpShapeContainer* collection, const hkpMoppCode* code, const hkpShapeRayCastInput& input, const hkpCdBody& body, hkpRayHitCollector& collector)
{
    m_collection = collection;
    m_collector = &collector;
    m_body = &body;
    m_rayResultPtr = nullptr;
    m_earlyOutHitFraction = collector.m_earlyOutHitFraction;
    m_code = code;
    queryLongRaySetup(*this, code, input);
}

namespace
{
// the common start of queryLongRay (inline in both): the ray in the code's frame scaled to 16.16
void queryLongRaySetup(hkpMoppLongRayVirtualMachine& vm, const hkpMoppCode* code, const hkpShapeRayCastInput& input)
{
    const unsigned char* PC = code->m_data.begin();
    const hkVector4f& offset = code->m_info.m_offset;

    hkpMoppLongRayVirtualMachine::QueryInt query;
    query.m_FtoBoffset.setZero();
    query.m_shift = 0;
    query.m_FtoBScale = offset(3) * (1.0f / 65536.0f);
    query.m_primitiveOffset = 0;
    query.m_properties[0] = 0;
    vm.m_ItoFScale = 1.0f / offset(3);

    vm.m_ray = input;
    vm.m_hitFound = false;

    alignas(16) hkpMoppLongRayVirtualMachine::QueryFloat fQuery;
    hkVector4f scale; scale.setAll(query.m_FtoBScale);
    fQuery.m_rayEnds[0].setSub(input.m_from, offset);
    fQuery.m_rayEnds[0].setMul(scale, fQuery.m_rayEnds[0]);
    fQuery.m_rayEnds[1].setSub(input.m_to, offset);
    fQuery.m_rayEnds[1].setMul(scale, fQuery.m_rayEnds[1]);

    vm.m_reindexingMask = *PC == 0x0D ? -1 : 0; // a code starting with the reindexing opcode keeps the chunk in the keys
    vm.queryRayOnTree(&query, PC, &fQuery, 0);
}
}

// 0x00E68CA0
// The interpreter. fQuery (this level's ray, which the walk clips and rescales in place) is shared with the caller
// only through copies: a node whose children the ray crosses both recurses into the near child with a copy clipped to
// it and continues with the far child after clipping its own ray.
void hkpMoppLongRayVirtualMachine::queryRayOnTree(const QueryInt* query, const unsigned char* PC, QueryFloat* const fQuery, int chunkOffset)
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
        float a0, a1, lo, hi;
        int leftOffset, rightOffset;
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
            local.m_FtoBoffset.setAdd(query->m_FtoBoffset, o);
            local.m_FtoBoffset.mul(sv);
            local.m_shift = query->m_shift + op;
            local.m_FtoBScale = s * query->m_FtoBScale;
            PC += 4;
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

        case 0x10: case 0x11: case 0x12: // split on x/y/z: left up to b2... (lo = b1, hi = b2), right child at +b3
            axis = op - 0x10;
            a0 = fQuery->m_rayEnds[0](axis);
            a1 = fQuery->m_rayEnds[1](axis);
            lo = (float)(int)b1;
            hi = (float)(int)b2;
            leftOffset = 0;
            rightOffset = b3;
            PC += 4;
            break;

        // the diagonal splits: the ends projected on the plane normal (in this float order), the plane values scaled
        case 0x13: a0 = fQuery->m_rayEnds[0](2) + fQuery->m_rayEnds[0](1); a1 = fQuery->m_rayEnds[1](2) + fQuery->m_rayEnds[1](1); lo = (float)(int)b1 * 2.0f; hi = (float)(int)b2 * 2.0f; goto diagonal;
        case 0x14: a0 = fQuery->m_rayEnds[0](1) - fQuery->m_rayEnds[0](2); a1 = fQuery->m_rayEnds[1](1) - fQuery->m_rayEnds[1](2); lo = (float)(int)b1 * 2.0f - 255.0f; hi = (float)(int)b2 * 2.0f - 255.0f; goto diagonal;
        case 0x15: a0 = fQuery->m_rayEnds[0](2) + fQuery->m_rayEnds[0](0); a1 = fQuery->m_rayEnds[1](2) + fQuery->m_rayEnds[1](0); lo = (float)(int)b1 * 2.0f; hi = (float)(int)b2 * 2.0f; goto diagonal;
        case 0x16: a0 = fQuery->m_rayEnds[0](0) - fQuery->m_rayEnds[0](2); a1 = fQuery->m_rayEnds[1](0) - fQuery->m_rayEnds[1](2); lo = (float)(int)b1 * 2.0f - 255.0f; hi = (float)(int)b2 * 2.0f - 255.0f; goto diagonal;
        case 0x17: a0 = fQuery->m_rayEnds[0](1) + fQuery->m_rayEnds[0](0); a1 = fQuery->m_rayEnds[1](1) + fQuery->m_rayEnds[1](0); lo = (float)(int)b1 * 2.0f; hi = (float)(int)b2 * 2.0f; goto diagonal;
        case 0x18: a0 = fQuery->m_rayEnds[0](0) - fQuery->m_rayEnds[0](1); a1 = fQuery->m_rayEnds[1](0) - fQuery->m_rayEnds[1](1); lo = (float)(int)b1 * 2.0f - 255.0f; hi = (float)(int)b2 * 2.0f - 255.0f; goto diagonal;
        case 0x19: a0 = fQuery->m_rayEnds[0](1) + fQuery->m_rayEnds[0](0) + fQuery->m_rayEnds[0](2); a1 = fQuery->m_rayEnds[1](1) + fQuery->m_rayEnds[1](0) + fQuery->m_rayEnds[1](2); lo = (float)(int)b1 * 3.0f; hi = (float)(int)b2 * 3.0f; goto diagonal;
        case 0x1A: a0 = fQuery->m_rayEnds[0](1) + fQuery->m_rayEnds[0](0) - fQuery->m_rayEnds[0](2); a1 = fQuery->m_rayEnds[1](1) + fQuery->m_rayEnds[1](0) - fQuery->m_rayEnds[1](2); lo = (float)(int)b1 * 3.0f - 255.0f; hi = (float)(int)b2 * 3.0f - 255.0f; goto diagonal;
        case 0x1B: a0 = fQuery->m_rayEnds[0](0) - fQuery->m_rayEnds[0](1) + fQuery->m_rayEnds[0](2); a1 = fQuery->m_rayEnds[1](0) - fQuery->m_rayEnds[1](1) + fQuery->m_rayEnds[1](2); lo = (float)(int)b1 * 3.0f - 255.0f; hi = (float)(int)b2 * 3.0f - 255.0f; goto diagonal;
        case 0x1C: a0 = fQuery->m_rayEnds[0](0) - fQuery->m_rayEnds[0](1) - fQuery->m_rayEnds[0](2); a1 = fQuery->m_rayEnds[1](0) - fQuery->m_rayEnds[1](1) - fQuery->m_rayEnds[1](2); lo = (float)(int)b1 * 3.0f - 510.0f; hi = (float)(int)b2 * 3.0f - 510.0f; goto diagonal;

        case 0x20: case 0x21: case 0x22: // single plane b1: left child up to b1 + 1 (at +3), right from b1 at +3+b2
            axis = op - 0x20;
            a0 = fQuery->m_rayEnds[0](axis);
            a1 = fQuery->m_rayEnds[1](axis);
            hi = (float)(int)b1;
            lo = hi + 1.0f;
            leftOffset = 0;
            rightOffset = b2;
            PC += 3;
            break;

        case 0x23: case 0x24: case 0x25: // split with 16-bit child offsets
            axis = op - 0x23;
            a0 = fQuery->m_rayEnds[0](axis);
            a1 = fQuery->m_rayEnds[1](axis);
            lo = (float)(int)b1;
            leftOffset = (b3 << 8) + PC[4];
            rightOffset = (PC[5] << 8) + PC[6];
            hi = (float)(int)b2;
            PC += 7;
            break;

        case 0x26: case 0x27: case 0x28: // bound on x/y/z in the node frame
            if (!clipToSlab(fQuery, op - 0x26, (float)(int)b1, (float)(int)b2))
            {
                return;
            }
            PC += 3;
            continue;

        case 0x29: case 0x2A: case 0x2B: // bound on x/y/z in 24-bit code units, brought into the node frame
        {
            const int boundAxis = op - 0x29;
            const float frameOffset = query->m_FtoBoffset(boundAxis);
            const float boundLo = (float)(int)((((b1 << 8) + b2) << 8) + b3) * m_ItoFScale * query->m_FtoBScale - frameOffset;
            const float boundHi = (float)(int)((((PC[4] << 8) + PC[5]) << 8) + PC[6]) * m_ItoFScale * query->m_FtoBScale - frameOffset;
            if (!clipToSlab(fQuery, boundAxis, boundLo, boundHi))
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
            castLeaf(*this, query, op - 0x30, chunkOffset);
            return;
        case 0x50:
            castLeaf(*this, query, b1, chunkOffset);
            return;
        case 0x51:
            castLeaf(*this, query, (b1 << 8) + b2, chunkOffset);
            return;
        case 0x52:
            castLeaf(*this, query, (((b1 << 8) + b2) << 8) + b3, chunkOffset);
            return;
        case 0x53:
            castLeaf(*this, query, (b1 << 24) + (b2 << 16) + PC[4] + (b3 << 8), chunkOffset);
            return;

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

        goto split;

    diagonal:
        leftOffset = 0;
        rightOffset = b3;
        PC += 4;

    split:
    {
        // PC is the end of the node: the left child at +leftOffset, the right child at +rightOffset
        if (hi > a1 && hi > a0)
        {
            PC += leftOffset; // only the left child
            continue;
        }
        PC += rightOffset;
        if (!(a0 > lo) || !(a1 > lo))
        {
            // both children: the near one first, on a copy of the ray clipped to it; then the far one, on this ray
            // clipped to it and to the early out
            const float d0lo = a0 - lo;
            const float d1lo = a1 - lo;
            const float d0hi = a0 - hi;
            const float d1hi = a1 - hi;
            alignas(16) QueryFloat nearQuery = *fQuery;
            if (d1lo > d0lo)
            {
                if (0.0f > d1lo * d0lo)
                {
                    nearQuery.m_rayEnds[1] = lerp(fQuery->m_rayEnds[0], fQuery->m_rayEnds[1], d0lo / (d0lo - d1lo));
                }
                queryRayOnTree(query, PC + (leftOffset - rightOffset), &nearQuery, chunkOffset);
                if (0.0f > d1hi * d0hi)
                {
                    fQuery->m_rayEnds[0] = lerp(fQuery->m_rayEnds[0], fQuery->m_rayEnds[1], d0hi / (d0hi - d1hi));
                }
                if (updateEarlyOut(*this, query, fQuery) && axis < 3 && hi > fQuery->m_rayEnds[1](axis))
                {
                    return;
                }
            }
            else
            {
                if (0.0f > d1hi * d0hi)
                {
                    nearQuery.m_rayEnds[1] = lerp(fQuery->m_rayEnds[0], fQuery->m_rayEnds[1], d0hi / (d0hi - d1hi));
                }
                queryRayOnTree(query, PC, &nearQuery, chunkOffset);
                if (0.0f > d1lo * d0lo)
                {
                    fQuery->m_rayEnds[0] = lerp(fQuery->m_rayEnds[0], fQuery->m_rayEnds[1], d0lo / (d0lo - d1lo));
                }
                if (updateEarlyOut(*this, query, fQuery) && axis < 3 && fQuery->m_rayEnds[1](axis) > lo)
                {
                    return;
                }
                PC += leftOffset - rightOffset;
            }
        }
        continue; // only the right child, or the far child after the near one
    }

    properties:
        if (query != &local)
        {
            local.m_FtoBoffset = query->m_FtoBoffset;
            local.m_shift = query->m_shift;
            local.m_FtoBScale = query->m_FtoBScale;
            local.m_primitiveOffset = query->m_primitiveOffset;
            query = &local;
        }
        continue;
    }
}

namespace
{
// The end of this level's ray moved to the early out fraction of the whole ray (when it is below 1), in the node
// frame. Returns whether it moved.
bool updateEarlyOut(const hkpMoppLongRayVirtualMachine& vm, const hkpMoppLongRayVirtualMachine::QueryInt* query, hkpMoppLongRayVirtualMachine::QueryFloat* fQuery)
{
    const float earlyOut = vm.m_earlyOutHitFraction;
    if (!(earlyOut < 1.0f))
    {
        return false;
    }
    hkVector4f end = lerp(vm.m_ray.m_from, vm.m_ray.m_to, earlyOut);
    end.sub(vm.m_code->m_info.m_offset);
    hkVector4f scale; scale.setAll(query->m_FtoBScale);
    end.setMul(scale, end);
    end.sub(query->m_FtoBoffset);
    fQuery->m_rayEnds[1] = end;
    return true;
}

// The ray clipped to the slab [lo, hi] of an axis (the ends are interpolated from the unclipped ray); false when the
// ray misses the slab.
bool clipToSlab(hkpMoppLongRayVirtualMachine::QueryFloat* fQuery, int axis, float lo, float hi)
{
    const float a0 = fQuery->m_rayEnds[0](axis);
    const float a1 = fQuery->m_rayEnds[1](axis);
    int farEnd;
    if (a1 > a0)
    {
        if (lo > a1 || a0 > hi)
        {
            return false;
        }
        farEnd = 0;
    }
    else
    {
        if (lo > a0 || a1 > hi)
        {
            return false;
        }
        farEnd = 1;
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

// A leaf: the key (the primitive offset plus the terminal, with the chunk in bits 8 and up when the code is
// reindexed) through the ray's shape collection filter; its shape is built in a 512-byte LIFO block and cast one key
// level down (closest hit: a hit becomes the early out) or through the collector (its early out fraction).
void castLeaf(hkpMoppLongRayVirtualMachine& vm, const hkpMoppLongRayVirtualMachine::QueryInt* query, unsigned int terminal, int chunkOffset)
{
    const unsigned int key = (((unsigned int)(chunkOffset >> 9) << 8) & (unsigned int)vm.m_reindexingMask) | (query->m_primitiveOffset + terminal);
    if (vm.m_ray.m_rayShapeCollectionFilter && !vm.m_ray.m_rayShapeCollectionFilter->isCollisionEnabled(vm.m_ray, *vm.m_collection, key))
    {
        return;
    }
    hkLifoAllocator& stack = hkMemoryRouter::getInstance().stack();
    hkpShapeBufferStorage* buffer = static_cast<hkpShapeBufferStorage*>(stack.fastBlockAlloc(HK_SHAPE_BUFFER_SIZE));
    const hkpShape* child = vm.m_collection->getChildShape(key, *buffer);
    if (vm.m_rayResultPtr)
    {
        vm.m_rayResultPtr->changeLevel(1);
        const hkBool hit = child->castRay(vm.m_ray, *vm.m_rayResultPtr);
        vm.m_rayResultPtr->changeLevel(-1);
        if (hit)
        {
            vm.m_hitFound = true;
            vm.m_earlyOutHitFraction = vm.m_rayResultPtr->m_hitFraction;
            vm.m_rayResultPtr->setKey(key);
        }
    }
    else
    {
        hkpCdBody body(vm.m_body);
        body.setShape(child, key);
        child->castRayWithCollector(vm.m_ray, body, *vm.m_collector);
        vm.m_earlyOutHitFraction = vm.m_collector->m_earlyOutHitFraction;
    }
    stack.fastBlockFree(buffer, HK_SHAPE_BUFFER_SIZE);
}
}
