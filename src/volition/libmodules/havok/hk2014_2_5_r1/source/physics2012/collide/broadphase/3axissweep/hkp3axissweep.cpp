// Original: volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/broadphase/3axissweep/hkp3axissweep.cpp
//
// Havok 2014.2.5 (havok_impl.md HK4): the 16-bit sweep and prune broadphase, from rfg.exe (no SDK source; the 2014.1
// header and .inl give the layout and the inline conversions).
//
// - Each axis keeps the sorted endpoints of all nodes (equal values ordered by node index, so the order is
//   deterministic); a node keeps the indices of its six endpoints. Moving an endpoint past others (updateAabb) reports
//   the pairs whose overlap begins or ends: on an axis an interval starts overlapping when a minimum passes a maximum,
//   and the other two axes are then checked on the endpoint indices.
// - A query flips one bit per node for every endpoint at or left of the query on the x axis (a marker's list of
//   overlapping objects gives the state at the marker, so only the endpoints after it are scanned), sets the bits of
//   the nodes starting inside the query interval, then tests y and z on the nodes whose bit is set.
// - Batches sort the new endpoints and merge them into the axes, then find the pairs by a one-axis sweep over the new
//   nodes and the old ones the batch's box overlaps (collide1Axis).
// - The ray and box casts walk the three axes like a 3D DDA, one bit per axis and node in a byte array, and report a
//   node when all three bits are set.
//
// Temporary arrays come from hkContainerTempAllocator and the ray cast's early-out distances from the LIFO allocator,
// in the original's order (the memory statistics stay in step with rfg.exe). The original's bugs are kept and
// commented: a marker that does not list an object being removed loses its entry -1 (written before the array).

#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/broadphase/3axissweep/hkp3axissweep.h"

#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/config/hkoptionalcomponent.h"

#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/algorithm/sort/hkradixsort.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/algorithm/sort/hksort.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/container/localarray/hklocalarray.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/memory/allocator/lifo/hklifoallocator.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/memory/router/hkmemoryrouter.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/types/geometry/aabb/hkaabbutil.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/agent/collidable/hkpcollidable.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/broadphase/hkpbroadphasecastcollector.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/broadphase/hkpbroadphasehandle.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/broadphase/hkpbroadphasehandlepair.h"

#include <emmintrin.h>

typedef hkp3AxisSweep::hkpBpNode hkpBpNode;
typedef hkp3AxisSweep::hkpBpEndPoint hkpBpEndPoint;
typedef hkp3AxisSweep::hkpBpMarker hkpBpMarker;
typedef hkp3AxisSweep::BpInt BpInt;
typedef hkArray<hkpBroadPhaseHandlePair, hkContainerHeapAllocator> hkpBpPairArray;

// ---- globals
__m128 hkp3AxisSweep::s_MaxVal = { 65532.0f, 65532.0f, 65532.0f, 65532.0f }; // 0x016F6120 (AABB_MAX_FVALUE)
// The byte offsets in hkpBpNode of min_x, max_x, min_y, max_y, min_z, max_z (index axis * 2 + max).
const int hkp3AxisSweep::hkpBpNode::s_memberOffsets[] = { 8, 10, 0, 4, 2, 6 }; // 0x016F6130

// ---- helpers

// hkpSortData of a node sort: the endpoint index on the x axis as the key, the node as user data.
typedef hkRadixSort::SortData16 hkpBpSortData;

// hkAlgorithm::ValueIntPair<hkUint16> (defragment's sort, ordered by value only): 0x00D8D980 sorts these.
struct ValueIntPair16
{
    unsigned short m_value; // 0x00
    unsigned short m_oldIndex; // 0x02
    bool operator<(const ValueIntPair16& other) const { return m_value < other.m_value; }
};

static HK_FORCE_INLINE void hk3AxisSweep_addPair(hkpBpPairArray& pairs, hkpBroadPhaseHandle* a, hkpBroadPhaseHandle* b)
{
    hkpBroadPhaseHandlePair& pair = pairs.expandOne();
    pair.m_a = a;
    pair.m_b = b;
}

// Removes an object from a marker's list: the last entry moves into its slot. A missing object gives index -1, and
// the original then writes the last entry before the array (kept).
static HK_FORCE_INLINE void hk3AxisSweep_removeFromMarker(hkpBpMarker& marker, int nodeIndex)
{
    hkArray<unsigned short, hkContainerHeapAllocator>& list = marker.m_overlappingObjects;
    const int index = list.indexOf((unsigned short)nodeIndex);
    list.m_size--;
    if (list.m_size != index)
    {
        list.m_data[index] = list.m_data[list.m_size];
    }
}

static HK_FORCE_INLINE void hk3AxisSweep_flipBit(hkUint32* bitField, int index)
{
    bitField[index >> 5] ^= 1u << (index & 31);
}

static HK_FORCE_INLINE void hk3AxisSweep_clearBit(hkUint32* bitField, int index)
{
    bitField[index >> 5] &= ~(1u << (index & 31));
}

// Clears whole 16-byte blocks: (numNodes >> 7) + 1 of them (the bit fields hold (numNodes >> 5) + 8 words).
static HK_FORCE_INLINE void hk3AxisSweep_clearBitField(hkUint32* bitField, int numNodes)
{
    for (int i = numNodes >> 7; i >= 0; i--)
    {
        bitField[0] = 0;
        bitField[1] = 0;
        bitField[2] = 0;
        bitField[3] = 0;
        bitField += 4;
    }
}

// The bits of the marker below the query: the marker itself, its list of overlapping objects (all but the query node)
// and none of the nodes starting inside its interval (the caller flips those again while it scans from the marker).
// Returns the first endpoint after the marker's minimum, or null when there is no marker below.
static const hkpBpEndPoint* hk3AxisSweep_setMarkerBits(const hkp3AxisSweep* sweep, int x_value, int excludedNodeIndex, hkUint32* bitField)
{
    if (sweep->m_numMarkers == 0)
    {
        return HK_NULL;
    }
    const int markerIndex = x_value >> (16 - sweep->m_ld2NumMarkers);
    if (markerIndex <= 0)
    {
        return HK_NULL;
    }
    const hkpBpMarker& marker = sweep->m_markers[markerIndex - 1];
    hk3AxisSweep_flipBit(bitField, marker.m_nodeIndex);
    const unsigned short* objects = marker.m_overlappingObjects.m_data;
    for (int i = marker.m_overlappingObjects.m_size - 1; i >= 0; i--)
    {
        const unsigned short object = *objects++;
        if (object != excludedNodeIndex)
        {
            hk3AxisSweep_flipBit(bitField, object);
        }
    }
    const hkpBpEndPoint* axis = sweep->m_axis[0].m_endPoints.m_data;
    const hkpBpNode& markerNode = sweep->m_nodes.m_data[marker.m_nodeIndex];
    for (const hkpBpEndPoint* ep = axis + markerNode.min_x + 1; ep < axis + markerNode.max_x; ep++)
    {
        if ((ep->m_value & 1) == 0)
        {
            hk3AxisSweep_clearBit(bitField, ep->m_nodeIndex);
        }
    }
    return axis + markerNode.min_x + 1;
}

// Sorts nodes by their x minimum (radix sort on 16-bit keys, padded to a multiple of 4 with 0xFFFF keys) into
// sortedOut, which ends with a node whose min_x is 0xFFFF (collide1Axis's sentinel).
static void hk3AxisSweep_sortNodesByMinX(const hkpBpNode* nodes, hkpBpSortData* keys, int numNodes, hkpBpNode* sortedOut)
{
    hkArray<hkpBpSortData, hkContainerTempAllocator> buffer(numNodes + 4);
    keys[numNodes].m_key = 0xFFFF;
    keys[numNodes + 1].m_key = 0xFFFF;
    keys[numNodes + 2].m_key = 0xFFFF;
    hkRadixSort::sort16(keys, (numNodes + 3) & ~3, buffer.m_data);
    for (int i = 0; i < numNodes; i++)
    {
        sortedOut[i] = nodes[keys[i].m_userData];
    }
    sortedOut[numNodes].min_x = 0xFFFF;
}

// ---- construction

// 0x00D89EB0: the hkp3AxisSweep optional component's creation function.
hkpBroadPhase* hk3AxisSweep16CreateBroadPhase(const hkVector4f& worldMin, const hkVector4f& worldMax, int numMarkers)
{
    HK_OPTIONAL_COMPONENT_MARK_USED(hkp3AxisSweep);
    return new hkp3AxisSweep(worldMin, worldMax, numMarkers);
}

// 0x00D86A40
// numMarkers is rounded down to a power of two (at least 1), and that minus one markers are made: their x interval
// starts at (i + 1) << (16 - ld2) and they cover the whole y and z axes.
hkp3AxisSweep::hkp3AxisSweep(const hkVector4f& worldMin, const hkVector4f& worldMax, int numMarkers)
    : hkpBroadPhase(BROADPHASE_16BIT, sizeof(hkp3AxisSweep), SUPPORT_SPU_RAY_CAST | SUPPORT_SPU_LINEAR_CAST | SUPPORT_SPU_CLOSEST_POINTS | SUPPORT_SPU_CHAR_PROXY_INT | ISA_SWEEP_AND_PRUNE)
{
    m_aabb.m_min = worldMin;
    m_aabb.m_max = worldMax;
    if (numMarkers == 0)
    {
        numMarkers = 1;
    }
    int ld2NumMarkers = -1;
    for (; numMarkers > 0; numMarkers >>= 1)
    {
        ld2NumMarkers++;
    }
    m_nodes.reserve(255);

    // Measure the rounding of the float to int conversion: bisect [10, 11] for the largest x whose box [x, x + 1]
    // converts below 12 at scale 1 (23 steps, the float mantissa), keep x - 11.
    m_offsetLow.setZero();
    m_offsetHigh.setZero();
    m_scale = hkVector4f::getConstant<HK_QUADREAL_1>();
    {
        hkVector4f lo; lo.setAll(10.0f);
        hkVector4f hi; hi.setAll(11.0f);
        for (int i = 0; i < 23; i++)
        {
            hkVector4f mid;
            mid.setAdd(hi, lo);
            mid.mul(hkVector4f::getConstant<HK_QUADREAL_INV_2>());
            hkAabb aabb;
            aabb.m_min = mid;
            aabb.m_max.setAdd(mid, hkVector4f::getConstant<HK_QUADREAL_1>());
            hkAabbUint32 aabbInt;
            convertAabbToInt(aabb, aabbInt);
            if (aabbInt.m_max[0] < 12)
            {
                lo = mid;
            }
            else
            {
                hi = mid;
            }
        }
        hkVector4f mid;
        mid.setAdd(hi, lo);
        mid.mul(hkVector4f::getConstant<HK_QUADREAL_INV_2>());
        hkVector4f eleven; eleven.setAll(11.0f);
        mid.sub(eleven);
        m_intToFloatFloorCorrection.m_real = mid.m_quad;
    }
    m_scale.setZero();
    m_offsetLow.setZero();
    m_offsetHigh.setZero();

    // Node 0: the world, with the sentinel endpoints (value 0 at index 0 and 0xFFFD at the end of each axis).
    hkpBpNode* worldNode = &m_nodes.expandOne();
    worldNode->min_z = 0;
    worldNode->min_y = 0;
    worldNode->min_x = 0;
    worldNode->m_handle = HK_NULL;
    numMarkers = 1 << ld2NumMarkers;
    m_axis[0].m_endPoints.reserve(numMarkers * 2 + 510);
    m_axis[1].m_endPoints.reserve(512);
    m_axis[2].m_endPoints.reserve(512);
    hkpBpEndPoint ep;
    ep.m_value = 0;
    ep.m_nodeIndex = 0;
    for (int i = 0; i < 3; i++)
    {
        m_axis[i].m_endPoints.m_data[m_axis[i].m_endPoints.m_size++] = ep;
    }

    m_ld2NumMarkers = ld2NumMarkers;
    m_numMarkers = numMarkers - 1;
    m_markers = HK_NULL;
    if (m_numMarkers != 0)
    {
        m_markers = (hkpBpMarker*)hkMemoryRouter::easyAlloc(hkMemoryRouter::getInstance().heap(), m_numMarkers * (int)sizeof(hkpBpMarker));
    }
    for (int i = 0; i < m_numMarkers; i++)
    {
        hkpBpMarker* marker = new (&m_markers[i]) hkpBpMarker();
        ep.m_nodeIndex = (unsigned short)m_nodes.m_size;
        ep.m_value = (unsigned short)((i + 1) << (16 - m_ld2NumMarkers));
        marker->m_nodeIndex = ep.m_nodeIndex;
        marker->m_value = ep.m_value;
        hkpBpNode& node = m_nodes.expandOne();
        node.min_x = (unsigned short)m_axis[0].m_endPoints.m_size;
        m_axis[0].m_endPoints.m_data[m_axis[0].m_endPoints.m_size++] = ep;
        ep.m_value |= 1;
        node.max_x = (unsigned short)m_axis[0].m_endPoints.m_size;
        m_axis[0].m_endPoints.m_data[m_axis[0].m_endPoints.m_size++] = ep;
        node.m_handle = (hkpBroadPhaseHandle*)(hkUlong)(i * (int)sizeof(hkpBpMarker) | 1);
        // Original bug kept: a marker has no y or z endpoints, so these indices are the world node's sentinels; moving
        // a marker node (a removal before it, defragment) writes its index into them, and ties at 0 and 0xFFFD then
        // step off the endpoint arrays. The game creates its world without markers, so this is not reached.
        node.min_z = 0;
        node.min_y = 0;
        node.max_z = 1;
        node.max_y = 1;
    }

    // The original copies m_nodes[0] through the reference it took when it added the node (the array does not move:
    // 255 nodes are reserved), then sets the world's maxima to the sentinels.
    *worldNode = m_nodes.m_data[0];
    worldNode->max_x = (unsigned short)m_axis[0].m_endPoints.m_size;
    worldNode->max_y = (unsigned short)m_axis[1].m_endPoints.m_size;
    worldNode->max_z = (unsigned short)m_axis[2].m_endPoints.m_size;
    ep.m_value = 0xFFFD;
    ep.m_nodeIndex = 0;
    for (int i = 0; i < 3; i++)
    {
        m_axis[i].m_endPoints.m_data[m_axis[i].m_endPoints.m_size++] = ep;
    }
}

// 0x00D89EF0
hkp3AxisSweep::~hkp3AxisSweep()
{
    for (int i = 0; i < m_numMarkers; i++)
    {
        m_markers[i].m_overlappingObjects.clearAndDeallocate();
    }
    hkMemoryRouter::easyFree(hkMemoryRouter::getInstance().heap(), m_markers);
}

// 0x00D83FC0
hkpBroadPhase::BroadPhaseType hkp3AxisSweep::getType() const
{
    return BROADPHASE_16BIT;
}

// 0x00D842E0: the world node is not counted.
int hkp3AxisSweep::getNumObjects() const
{
    return m_nodes.m_size - 1;
}

// 0x00D840E0
void hkp3AxisSweep::checkDeterminism()
{
}

// 0x00D8F930
void hkp3AxisSweep::getOffsetLowHigh32bit(hkVector4f& offsetLow32bit, hkVector4f& offsetHigh32bit) const
{
    offsetLow32bit = m_offsetLow32bit;
    offsetHigh32bit = m_offsetHigh32bit;
}

// 0x00D84520
// The 16-bit quantization maps the world box to [0, AABB_MAX_FVALUE]; the 32-bit one (the collision input's) is kept
// for addObject(hkAabb).
void hkp3AxisSweep::set32BitOffsetAndScale(const hkVector4f& offsetLow, const hkVector4f& offsetHigh, const hkVector4f& scale)
{
    m_offsetLow32bit = offsetLow;
    m_offsetHigh32bit = offsetHigh;
    m_scale32bit = scale;
    hkVector4f span;
    span.setSub(m_aabb.m_max, m_aabb.m_min);
    span.setComponent<3>(hkSimdFloat32::getConstant<HK_QUADREAL_1>());
    m_offsetLow = offsetLow;
    hkVector4f maxVal; maxVal.m_quad = s_MaxVal;
    hkVector4f unit; unit.m_quad = _mm_div_ps(span.m_quad, maxVal.m_quad);
    m_offsetHigh.setAdd(m_offsetLow, unit);
    m_scale.m_quad = _mm_div_ps(maxVal.m_quad, span.m_quad);
    m_scale.zeroComponent<3>();
    m_offsetLow.zeroComponent<3>();
    m_offsetHigh.zeroComponent<3>();
}

// ---- axis maintenance

// 0x00D84070: the first endpoint in [start, end) whose value is not below value (halving while more than 16 remain).
const hkpBpEndPoint* hkp3AxisSweep::hkpBpAxis::find(const hkpBpEndPoint* start, const hkpBpEndPoint* end, unsigned short value) const
{
    int numBytes = (int)((const char*)end - (const char*)start);
    while ((numBytes & ~3) > 0x40)
    {
        const hkpBpEndPoint* mid = start + (numBytes >> 3);
        if (mid->m_value < value)
        {
            start = mid;
        }
        else
        {
            end = mid;
        }
        numBytes = (int)((const char*)end - (const char*)start);
    }
    while (start->m_value < value)
    {
        start++;
    }
    return start;
}

// 0x00D84D90: inserts a node's two endpoints, after the equal values of lower nodes. The caller updates the other
// nodes' indices (updateNodesAfterInsert).
void hkp3AxisSweep::hkpBpAxis::insert(hkpBpNode* nodes, int nodeIndex, unsigned short minPosition, unsigned short maxPosition, unsigned short& minInsertPositionOut, unsigned short& maxInsertPositionOut)
{
    const int size = m_endPoints.m_size;
    m_endPoints.reserve(size + 2);
    hkpBpEndPoint* ep = m_endPoints.m_data + size - 1;
    m_endPoints.m_size = size + 2;
    ep[2] = ep[0];
    ep--;
    while (maxPosition < ep->m_value)
    {
        ep[2] = ep[0];
        ep--;
    }
    while (maxPosition == ep->m_value && (unsigned int)nodeIndex < ep->m_nodeIndex)
    {
        ep[2] = ep[0];
        ep--;
    }
    ep[2].m_value = maxPosition;
    ep[2].m_nodeIndex = (unsigned short)nodeIndex;
    maxInsertPositionOut = (unsigned short)((ep - m_endPoints.m_data) + 2);
    while (minPosition < ep->m_value)
    {
        ep[1] = ep[0];
        ep--;
    }
    while (minPosition == ep->m_value && (unsigned int)nodeIndex < ep->m_nodeIndex)
    {
        ep[1] = ep[0];
        ep--;
    }
    ep[1].m_nodeIndex = (unsigned short)nodeIndex;
    ep[1].m_value = minPosition;
    minInsertPositionOut = (unsigned short)((ep - m_endPoints.m_data) + 1);
}

// 0x00D845C0: removes the endpoints at minPosition and maxPosition.
void hkp3AxisSweep::hkpBpAxis::remove(int minPosition, int maxPosition)
{
    hkpBpEndPoint* data = m_endPoints.m_data;
    for (int i = minPosition; i < maxPosition - 1; i++)
    {
        data[i] = data[i + 1];
    }
    m_endPoints.m_size -= 2;
    for (int i = maxPosition - 1; i < m_endPoints.m_size; i++)
    {
        data[i] = data[i + 2];
    }
}

// <0> 0x00D8CC50, <1> 0x00D8CD50, <2> 0x00D8CE50
// Batch insertion, part 1: moves the old endpoints above the last new value (with the end sentinel) to the end of the
// grown axis. Returns the index after the gap that mergeRest fills.
template<int axis>
int hkp3AxisSweep::hkpBpAxis::insertTail(hkpBpNode* nodes, hkpBpEndPoint* newNodes, int numNewEndPoints)
{
    const int size = m_endPoints.m_size;
    const int newSize = size + numNewEndPoints;
    m_endPoints.reserve(newSize);
    m_endPoints.m_size = newSize;
    const BpInt lastNewValue = newNodes[numNewEndPoints - 1].m_value;
    hkpBpEndPoint* data = m_endPoints.m_data;
    hkpBpEndPoint* dst = data + newSize - 1;
    hkpBpEndPoint* src = data + size - 1;
    *dst = *src;
    nodes[src->m_nodeIndex].setElem<axis>(src->m_value & 1, (int)(dst - m_endPoints.m_data));
    src--;
    dst--;
    while (lastNewValue < src->m_value)
    {
        *dst = *src;
        nodes[src->m_nodeIndex].setElem<axis>(src->m_value & 1, (int)(dst - m_endPoints.m_data));
        src--;
        dst--;
    }
    return (int)(dst - m_endPoints.m_data) + 1;
}

// <0> 0x00D8CF50, <1> 0x00D8D010, <2> 0x00D8D0D0
// Batch insertion, part 2: merges the sorted new endpoints with the old ones below the tail, from the top down. A new
// endpoint goes above an old one of the same value (new nodes have higher indices).
template<int axis>
void hkp3AxisSweep::hkpBpAxis::mergeRest(hkpBpNode* nodes, int startOfTail, hkpBpEndPoint* newEndPoints, int numNewEndPoints)
{
    hkpBpEndPoint* data = m_endPoints.m_data;
    int writeIndex = startOfTail - 1;
    hkpBpEndPoint* dst = data + writeIndex;
    hkpBpEndPoint* old = data + (startOfTail - numNewEndPoints) - 1;
    hkpBpEndPoint* newEp = newEndPoints + numNewEndPoints - 1;
    BpInt oldValue = old->m_value;
    BpInt newValue = newEp->m_value;
    for (;;)
    {
        while (newValue < oldValue)
        {
            *dst = *old;
            nodes[old->m_nodeIndex].setElem<axis>(old->m_value & 1, writeIndex);
            old--;
            dst--;
            writeIndex--;
            oldValue = old->m_value;
        }
        *dst = *newEp;
        nodes[newEp->m_nodeIndex].setElem<axis>(newEp->m_value & 1, writeIndex);
        newEp--;
        dst--;
        writeIndex--;
        if (newEp < newEndPoints)
        {
            break;
        }
        newValue = newEp->m_value;
    }
}

// <0> 0x00D8D190, <1> 0x00D8D230, <2> 0x00D8D2D0
// Drops the endpoints of removed nodes (relocation -1) and renumbers the others.
template<int axis>
void hkp3AxisSweep::hkpBpAxis::removeBatch(hkpBpNode* nodes, const hkArrayBase<int>& nodeRelocations)
{
    hkpBpEndPoint* src = m_endPoints.m_data;
    hkpBpEndPoint* end = src + m_endPoints.m_size;
    hkpBpEndPoint* dst = src;
    int index = 0;
    for (; src < end; src++)
    {
        const int relocated = nodeRelocations.m_data[src->m_nodeIndex];
        if (relocated >= 0)
        {
            *dst = *src;
            dst->m_nodeIndex = (unsigned short)relocated;
            nodes[relocated].setElem<axis>(src->m_value & 1, index);
            dst++;
            index++;
        }
    }
    m_endPoints.setSize(index);
}

// 0x00D84620: after a node's endpoints went in at newNode's indices, the indices from there on move up (by 1 past the
// minimum, by 2 past the maximum). Packed 16-bit signed compares in the original.
void hkp3AxisSweep::updateNodesAfterInsert(hkpBpNode* nodes, int numNodes, hkpBpNode& newNode)
{
    const short mins[6] = { (short)newNode.min_y, (short)newNode.min_z, (short)newNode.min_y, (short)newNode.min_z, (short)newNode.min_x, (short)newNode.min_x };
    const short maxs[6] = { (short)newNode.max_y, (short)newNode.max_z, (short)newNode.max_y, (short)newNode.max_z, (short)newNode.max_x, (short)newNode.max_x };
    for (hkpBpNode* node = nodes; node < nodes + numNodes; node++)
    {
        unsigned short* fields = &node->min_y;
        for (int i = 0; i < 6; i++)
        {
            const short value = (short)fields[i];
            fields[i] = (unsigned short)(fields[i] + (value > (short)(maxs[i] - 2)) + (value > (short)(mins[i] - 1)));
        }
    }
}

// 0x00D847C0: after a node's endpoints were removed, the indices above them move down.
void hkp3AxisSweep::updateNodesAfterDelete(hkpBpNode* nodes, int numNodes, hkpBpNode& oldNode)
{
    const short mins[6] = { (short)oldNode.min_y, (short)oldNode.min_z, (short)oldNode.min_y, (short)oldNode.min_z, (short)oldNode.min_x, (short)oldNode.min_x };
    const short maxs[6] = { (short)oldNode.max_y, (short)oldNode.max_z, (short)oldNode.max_y, (short)oldNode.max_z, (short)oldNode.max_x, (short)oldNode.max_x };
    for (hkpBpNode* node = nodes; node < nodes + numNodes; node++)
    {
        unsigned short* fields = &node->min_y;
        for (int i = 0; i < 6; i++)
        {
            const short value = (short)fields[i];
            fields[i] = (unsigned short)(fields[i] - (value > maxs[i]) - (value > mins[i]));
        }
    }
}

// <0> 0x00D8BD50, <1> 0x00D8BE00, <2> 0x00D8BEB0 (inlined in the caller)
// Equal endpoints are ordered by node index: after a node got a lower index, its endpoints move down past the equal
// endpoints of higher nodes (equal values have the same parity, so they are the same kind).
template<int axis>
static HK_FORCE_INLINE void _fixDeterministicAxisOrderAfterNodeIdWasDecreased(hkp3AxisSweep* sweep, hkpBpNode* nodes, hkpBpNode& node, unsigned int nodeIndex)
{
    for (int minmax = 0; minmax < 2; minmax++)
    {
        unsigned short& field = minmax == 0 ? node.getMin(axis) : node.getMax(axis);
        unsigned int index = field;
        hkpBpEndPoint* ep = sweep->m_axis[axis].m_endPoints.m_data + index;
        while (ep->m_value == ep[-1].m_value)
        {
            const BpInt other = ep[-1].m_nodeIndex;
            if (other <= nodeIndex)
            {
                break;
            }
            *ep = ep[-1];
            (minmax == 0 ? nodes[other].getMin(axis) : nodes[other].getMax(axis)) = (unsigned short)index;
            index--;
            ep--;
        }
        ep->m_nodeIndex = (unsigned short)nodeIndex;
        field = (unsigned short)index;
    }
}

// 0x00D84360
void hkp3AxisSweep::_fixDeterministicOrderAfterNodeIdWasDecreased(int nodeIndex)
{
    hkpBpNode* nodes = m_nodes.m_data;
    hkpBpNode& node = nodes[nodeIndex];
    _fixDeterministicAxisOrderAfterNodeIdWasDecreased<0>(this, nodes, node, nodeIndex);
    _fixDeterministicAxisOrderAfterNodeIdWasDecreased<1>(this, nodes, node, nodeIndex);
    _fixDeterministicAxisOrderAfterNodeIdWasDecreased<2>(this, nodes, node, nodeIndex);
}

// ---- overlap reports (fastcall in the original: a in ECX, b and the arrays on the stack)

// 0x00D84BB0
void __fastcall hkp3AxisSweep::beginOverlap(hkpBpNode& a, hkpBpNode& b, hkpBpPairArray& newPairsOut)
{
    hk3AxisSweep_addPair(newPairsOut, a.m_handle, b.m_handle);
}

// 0x00D84C00
void __fastcall hkp3AxisSweep::endOverlap(hkpBpNode& a, hkpBpNode& b, hkpBpPairArray& deletedPairsOut)
{
    hk3AxisSweep_addPair(deletedPairsOut, a.m_handle, b.m_handle);
}

// 0x00D84C50: a marker b records a in its list instead.
void __fastcall hkp3AxisSweep::beginOverlapCheckMarker(hkpBpMarker* markers, hkpBpNode& a, int nodeIndexA, hkpBpNode& b, hkpBpPairArray& newPairsOut)
{
    if (b.isNotMarker())
    {
        hk3AxisSweep_addPair(newPairsOut, a.m_handle, b.m_handle);
        return;
    }
    b.getMarker(markers).m_overlappingObjects.pushBack((unsigned short)nodeIndexA);
}

// 0x00D84CF0
void __fastcall hkp3AxisSweep::endOverlapCheckMarker(hkpBpMarker* markers, hkpBpNode& a, int nodeIndexA, hkpBpNode& b, hkpBpPairArray& deletedPairsOut)
{
    if (b.isNotMarker())
    {
        hk3AxisSweep_addPair(deletedPairsOut, a.m_handle, b.m_handle);
        return;
    }
    hk3AxisSweep_removeFromMarker(b.getMarker(markers), nodeIndexA);
}

// The two other axes of axis are disjoint (yz for x, xz for y, xy for z).
template<int axis>
static HK_FORCE_INLINE unsigned int hk3AxisSweep_otherAxesDisjoint(const hkpBpNode& a, const hkpBpNode& b)
{
    return axis == 0 ? a.yzDisjoint(b) : (axis == 1 ? a.xzDisjoint(b) : a.xyDisjoint(b));
}

template<int axis, int useMarker>
static HK_FORCE_INLINE void hk3AxisSweep_beginOverlap(hkp3AxisSweep* sweep, hkpBpNode& node, unsigned int nodeIndex, hkpBpNode& other, hkpBpPairArray& pairs)
{
    if (useMarker)
    {
        hkp3AxisSweep::beginOverlapCheckMarker(sweep->m_markers, node, nodeIndex, other, pairs);
    }
    else
    {
        hkp3AxisSweep::beginOverlap(node, other, pairs);
    }
}

template<int axis, int useMarker>
static HK_FORCE_INLINE void hk3AxisSweep_endOverlap(hkp3AxisSweep* sweep, hkpBpNode& node, unsigned int nodeIndex, hkpBpNode& other, hkpBpPairArray& pairs)
{
    if (useMarker)
    {
        hkp3AxisSweep::endOverlapCheckMarker(sweep->m_markers, node, nodeIndex, other, pairs);
    }
    else
    {
        hkp3AxisSweep::endOverlap(node, other, pairs);
    }
}

// <0,1> 0x00D8DDA0, <1,0> 0x00D8E170, <2,0> 0x00D8E520 (inlined in updateAabb)
// Moves a node's endpoints on one axis: first growing (minimum down, maximum up: overlaps can begin), then shrinking
// (maximum down, minimum up: overlaps can end). Only the x axis meets markers.
template<int axis, int useMarker>
static HK_FORCE_INLINE void _updateAxis(hkp3AxisSweep* sweep, hkpBpNode* nodes, hkpBpNode& node, unsigned int nodeIndex, unsigned int new_min, unsigned int new_max, hkpBpPairArray& newPairsOut, hkpBpPairArray& deletedPairsOut)
{
    hkpBpEndPoint* base = sweep->m_axis[axis].m_endPoints.m_data;

    // minimum down
    unsigned int index = node.getMin(axis);
    hkpBpEndPoint* ep = base + index;
    for (;;)
    {
        const BpInt value = ep[-1].m_value;
        if (!(new_min < value))
        {
            break;
        }
        hkpBpNode& other = nodes[ep[-1].m_nodeIndex];
        *ep = ep[-1];
        if ((value & 1) == 0)
        {
            other.getMin(axis) = (unsigned short)index;
        }
        else
        {
            const unsigned int disjoint = hk3AxisSweep_otherAxesDisjoint<axis>(node, other);
            other.getMax(axis) = (unsigned short)index;
            if (!disjoint)
            {
                hk3AxisSweep_beginOverlap<axis, useMarker>(sweep, node, nodeIndex, other, newPairsOut);
            }
        }
        index--;
        ep--;
    }
    while (new_min == ep[-1].m_value)
    {
        const BpInt other = ep[-1].m_nodeIndex;
        if (other <= nodeIndex)
        {
            break;
        }
        *ep = ep[-1];
        nodes[other].getMin(axis) = (unsigned short)index;
        index--;
        ep--;
    }
    ep->m_value = (unsigned short)new_min;
    ep->m_nodeIndex = (unsigned short)nodeIndex;
    node.getMin(axis) = (unsigned short)index;

    // maximum up
    index = node.getMax(axis);
    ep = base + index;
    for (;;)
    {
        const BpInt value = ep[1].m_value;
        if (!(value < new_max))
        {
            break;
        }
        *ep = ep[1];
        index++;
        hkpBpNode& other = nodes[ep[1].m_nodeIndex];
        if ((value & 1) == 0)
        {
            other.getMin(axis)--;
            if (!hk3AxisSweep_otherAxesDisjoint<axis>(node, other))
            {
                hk3AxisSweep_beginOverlap<axis, useMarker>(sweep, node, nodeIndex, other, newPairsOut);
            }
        }
        else
        {
            other.getMax(axis)--;
        }
        ep++;
    }
    for (;;)
    {
        if (new_max != ep[1].m_value)
        {
            break;
        }
        const unsigned int other = ep[1].m_nodeIndex;
        if (!(other < nodeIndex) || other == 0)
        {
            break;
        }
        *ep = ep[1];
        nodes[other].getMax(axis)--;
        index++;
        ep++;
    }

    // maximum down
    for (;;)
    {
        const BpInt value = ep[-1].m_value;
        if (!(new_max < value))
        {
            break;
        }
        *ep = ep[-1];
        index--;
        hkpBpNode& other = nodes[ep[-1].m_nodeIndex];
        if ((value & 1) == 0)
        {
            other.getMin(axis)++;
            if (!hk3AxisSweep_otherAxesDisjoint<axis>(node, other))
            {
                hk3AxisSweep_endOverlap<axis, useMarker>(sweep, node, nodeIndex, other, deletedPairsOut);
            }
        }
        else
        {
            other.getMax(axis)++;
        }
        ep--;
    }
    while (new_max == ep[-1].m_value)
    {
        const BpInt other = ep[-1].m_nodeIndex;
        if (other <= nodeIndex)
        {
            break;
        }
        *ep = ep[-1];
        nodes[other].getMax(axis)++;
        ep--;
        index--;
    }
    node.getMax(axis) = (unsigned short)index;
    ep->m_value = (unsigned short)new_max;
    ep->m_nodeIndex = (unsigned short)nodeIndex;

    // minimum up
    index = node.getMin(axis);
    ep = base + index;
    for (;;)
    {
        const BpInt value = ep[1].m_value;
        if (!(value < new_min))
        {
            break;
        }
        const BpInt otherIndex = ep[1].m_nodeIndex;
        *ep = ep[1];
        index++;
        hkpBpNode& other = nodes[otherIndex];
        if ((value & 1) == 0)
        {
            other.getMin(axis)--;
        }
        else
        {
            other.getMax(axis)--;
            if (!hk3AxisSweep_otherAxesDisjoint<axis>(node, other))
            {
                hk3AxisSweep_endOverlap<axis, useMarker>(sweep, node, nodeIndex, other, deletedPairsOut);
            }
        }
        ep++;
    }
    while (new_min == ep[1].m_value)
    {
        const BpInt other = ep[1].m_nodeIndex;
        if (nodeIndex <= other)
        {
            break;
        }
        *ep = ep[1];
        nodes[other].getMin(axis)--;
        ep++;
        index++;
    }
    node.getMin(axis) = (unsigned short)index;
    ep->m_value = (unsigned short)new_min;
    ep->m_nodeIndex = (unsigned short)nodeIndex;
}

// 0x00D84F30
void hkp3AxisSweep::updateAabb(hkpBroadPhaseHandle* object, const hkAabbUint32& aabb, hkpBpPairArray& newPairsOut, hkpBpPairArray& deletedPairsOut)
{
    hkpBpNode* nodes = m_nodes.m_data;
    const unsigned int nodeIndex = object->m_id;
    hkpBpNode& node = nodes[nodeIndex];
    _updateAxis<0, HK_BP_USE_MARKER>(this, nodes, node, nodeIndex, aabb.m_min[0], aabb.m_max[0], newPairsOut, deletedPairsOut);
    _updateAxis<1, HK_BP_NO_MARKER>(this, nodes, node, nodeIndex, aabb.m_min[1], aabb.m_max[1], newPairsOut, deletedPairsOut);
    _updateAxis<2, HK_BP_NO_MARKER>(this, nodes, node, nodeIndex, aabb.m_min[2], aabb.m_max[2], newPairsOut, deletedPairsOut);
}

// 0x00D85AE0
void hkp3AxisSweep::updateAabbs(hkpBroadPhaseHandle** objects, const hkAabb* aabbs, int numObjects, hkpBpPairArray& addedPairsOut, hkpBpPairArray& removedPairsOut)
{
    for (hkpBroadPhaseHandle** object = objects; object < objects + numObjects; object++, aabbs++)
    {
        hkAabbUint32 aabbInt;
        convertAabbToInt(*aabbs, aabbInt);
        updateAabb(*object, aabbInt, addedPairsOut, removedPairsOut);
    }
}

// 0x00D85BF0
void hkp3AxisSweep::updateAabbsUint32(hkpBroadPhaseHandle** objects, const hkAabbUint32* aabbs, int numObjects, hkpBpPairArray& addedPairsOut, hkpBpPairArray& removedPairsOut)
{
    for (hkpBroadPhaseHandle** object = objects; object < objects + numObjects; object++, aabbs++)
    {
        hkAabbUint32 aabbInt;
        convertAabbToBroadPhaseResolution(*aabbs, aabbInt);
        updateAabb(*object, aabbInt, addedPairsOut, removedPairsOut);
    }
}

// ---- single object queries

// 0x00D84130
// The bits of the nodes overlapping queryNode's x interval: from the marker below x_value (or the start), every
// endpoint up to the query's minimum flips its node, then the nodes starting inside the interval. The query node itself
// stays clear.
void hkp3AxisSweep::setBitsBasedOnXInterval(int numNodes, int x_value, const hkpBpNode& queryNode, unsigned short queryNodeIndex, unsigned int* bitField) const
{
    const BpInt maxX = queryNode.max_x;
    const BpInt minX = queryNode.min_x;
    hk3AxisSweep_clearBitField(bitField, numNodes);
    const hkpBpEndPoint* ep = hk3AxisSweep_setMarkerBits(this, x_value, queryNodeIndex, bitField);
    if (ep == HK_NULL)
    {
        ep = m_axis[0].m_endPoints.m_data + 1;
    }
    const hkpBpEndPoint* axis = m_axis[0].m_endPoints.m_data;
    for (; ep < axis + minX; ep++)
    {
        hk3AxisSweep_flipBit(bitField, ep->m_nodeIndex);
    }
    for (ep++; ep < axis + maxX; ep++)
    {
        if ((ep->m_value & 1) == 0)
        {
            hk3AxisSweep_flipBit(bitField, ep->m_nodeIndex);
        }
    }
}

// 0x00D860A0: the pairs of a new node; a marker overlapping it lists it instead.
void hkp3AxisSweep::querySingleAabbAddObject(hkpBroadPhaseHandle* object, int newNodeIndex, const unsigned int* bitField, hkpBpNode& refNode, hkpBpPairArray& pairs_out) const
{
    const unsigned int* end = bitField + (m_nodes.m_size >> 5) + 1;
    const hkpBpNode* nodeBase = m_nodes.m_data;
    for (; bitField < end; bitField++, nodeBase += 32)
    {
        unsigned int bits = *bitField;
        const hkpBpNode* node = nodeBase;
        while (bits != 0)
        {
            if ((bits & 0xFF) == 0)
            {
                node += 8;
                bits >>= 8;
                continue;
            }
            if ((bits & 1) != 0 && !node->yzDisjoint(refNode))
            {
                if (node->isMarker())
                {
                    node->getMarker(m_markers).m_overlappingObjects.pushBack((unsigned short)newNodeIndex);
                }
                else
                {
                    hk3AxisSweep_addPair(pairs_out, object, node->m_handle);
                }
            }
            node++;
            bits >>= 1;
        }
    }
}

// 0x00D862F0: the pairs of a node being removed; a marker overlapping it drops it from its list.
void hkp3AxisSweep::querySingleAabbRemoveObject(hkpBroadPhaseHandle* object, int newNodeIndex, const unsigned int* bitField, hkpBpNode& refNode, hkpBpPairArray& pairs_out) const
{
    const unsigned int* end = bitField + (m_nodes.m_size >> 5) + 1;
    const hkpBpNode* nodeBase = m_nodes.m_data;
    for (; bitField < end; bitField++, nodeBase += 32)
    {
        unsigned int bits = *bitField;
        const hkpBpNode* node = nodeBase;
        while (bits != 0)
        {
            if ((bits & 0xFF) == 0)
            {
                node += 8;
                bits >>= 8;
                continue;
            }
            if ((bits & 1) != 0 && !node->yzDisjoint(refNode))
            {
                if (node->isMarker())
                {
                    hk3AxisSweep_removeFromMarker(node->getMarker(m_markers), newNodeIndex);
                }
                else
                {
                    hk3AxisSweep_addPair(pairs_out, object, node->m_handle);
                }
            }
            node++;
            bits >>= 1;
        }
    }
}

// 0x00D86460: reports the marked nodes overlapping refNode on y and z (markers excluded), four bits at a time.
void hkp3AxisSweep::querySingleAabbSub(hkpBroadPhaseHandle* object, const unsigned int* bitField, hkpBpNode& refNode, hkpBpQuerySingleType type, hkpBpPairArray* pairs_out, hkArrayBase<hkpBpNode const*>* nodesOut, hkpBroadPhaseCastCollector* collector) const
{
    const unsigned int* end = bitField + (m_nodes.m_size >> 5) + 1;
    const hkpBpNode* nodeBase = m_nodes.m_data;
    for (; bitField < end; bitField++, nodeBase += 32)
    {
        const hkpBpNode* node = nodeBase;
        for (unsigned int bits = *bitField; bits != 0; bits >>= 4, node += 4)
        {
            if ((bits & 0xF) == 0)
            {
                continue;
            }
            for (int i = 0; i < 4; i++)
            {
                const hkpBpNode* n = node + i;
                if ((bits & (1u << i)) == 0 || n->yzDisjoint(refNode) || n->isMarker())
                {
                    continue;
                }
                if (type == HK_BP_REPORT_HANDLES)
                {
                    hk3AxisSweep_addPair(*pairs_out, object, n->m_handle);
                }
                else if (type == HK_BP_REPORT_NODES)
                {
                    nodesOut->m_data[nodesOut->m_size++] = n; // reserved by the caller
                }
                else
                {
                    collector->addBroadPhaseHandle(n->m_handle, 0);
                }
            }
        }
    }
}

// 0x00D88050
void hkp3AxisSweep::_querySingleAabb(const hkAabb& aabbIn, hkpBpQuerySingleType type, hkpBpPairArray* pairs_out, hkArrayBase<hkpBpNode const*>* nodesOut, hkpBroadPhaseCastCollector* collector) const
{
    const int numNodes = m_nodes.m_size;
    hkArray<hkUint32, hkContainerTempAllocator> bitField((numNodes >> 5) + 8);
    hk3AxisSweep_clearBitField(bitField.m_data, numNodes);
    alignas(16) hkAabbUint32 aabb;
    convertAabbToInt(aabbIn, aabb);

    const hkpBpEndPoint* ep = hk3AxisSweep_setMarkerBits(this, (int)aabb.m_min[0], -1, bitField.m_data);
    if (ep == HK_NULL)
    {
        ep = m_axis[0].m_endPoints.m_data + 1;
    }
    while (ep->m_value < aabb.m_min[0])
    {
        hk3AxisSweep_flipBit(bitField.m_data, ep->m_nodeIndex);
        ep++;
    }
    while (ep->m_value < aabb.m_max[0])
    {
        if ((ep->m_value & 1) == 0)
        {
            hk3AxisSweep_flipBit(bitField.m_data, ep->m_nodeIndex);
        }
        ep++;
    }

    // The query box as endpoint indices on y and z (the minimum at the first endpoint not below it, the maximum below
    // the first endpoint not below it).
    hkpBpNode refNode;
    {
        const hkpBpEndPoint* axis = m_axis[1].m_endPoints.m_data;
        const hkpBpEndPoint* end = axis + m_axis[1].m_endPoints.m_size - 2;
        refNode.min_y = (unsigned short)(m_axis[1].find(axis + 1, end, (unsigned short)aabb.m_min[1]) - axis);
        refNode.max_y = (unsigned short)(m_axis[1].find(axis + 1, end, (unsigned short)aabb.m_max[1]) - axis - 1);
    }
    {
        const hkpBpEndPoint* axis = m_axis[2].m_endPoints.m_data;
        const hkpBpEndPoint* end = axis + m_axis[2].m_endPoints.m_size - 2;
        refNode.min_z = (unsigned short)(m_axis[2].find(axis + 1, end, (unsigned short)aabb.m_min[2]) - axis);
        refNode.max_z = (unsigned short)(m_axis[2].find(axis + 1, end, (unsigned short)aabb.m_max[2]) - axis - 1);
    }
    querySingleAabbSub(HK_NULL, bitField.m_data, refNode, type, pairs_out, nodesOut, collector);
}

// 0x00D883B0: the pairs have m_a null.
void hkp3AxisSweep::querySingleAabb(const hkAabb& aabbIn, hkpBpPairArray& pairs_out) const
{
    _querySingleAabb(aabbIn, HK_BP_REPORT_HANDLES, &pairs_out, HK_NULL, HK_NULL);
}

// 0x00D883D0
void hkp3AxisSweep::querySingleAabbWithCollector(const hkAabb& aabbIn, hkpBroadPhaseCastCollector* collector) const
{
    _querySingleAabb(aabbIn, HK_BP_COLLECTOR, HK_NULL, HK_NULL, collector);
}

// 0x00D86FF0: the nodes overlapping an object already in the broadphase.
void hkp3AxisSweep::reQuerySingleObject(const hkpBroadPhaseHandle* object, hkpBpPairArray& pairs_out) const
{
    const int numNodes = m_nodes.m_size;
    hkArray<hkUint32, hkContainerTempAllocator> bitField((numNodes >> 5) + 8);
    const hkpBpNode& queryNode = m_nodes.m_data[object->m_id];
    setBitsBasedOnXInterval(numNodes, m_axis[0].m_endPoints.m_data[queryNode.min_x].m_value, queryNode, (unsigned short)object->m_id, bitField.m_data);
    const unsigned int* bits = bitField.m_data;
    const unsigned int* end = bits + (m_nodes.m_size >> 5) + 1;
    const hkpBpNode* nodeBase = m_nodes.m_data;
    for (; bits < end; bits++, nodeBase += 32)
    {
        unsigned int word = *bits;
        const hkpBpNode* node = nodeBase;
        while (word != 0)
        {
            if ((word & 0xFF) == 0)
            {
                node += 8;
                word >>= 8;
                continue;
            }
            if ((word & 1) != 0 && !node->yzDisjoint(queryNode) && node->isNotMarker())
            {
                hk3AxisSweep_addPair(pairs_out, (hkpBroadPhaseHandle*)object, node->m_handle);
            }
            node++;
            word >>= 1;
        }
    }
}

// 0x00D848D0: the integer boxes overlap (endpoint indices on all three axes).
bool hkp3AxisSweep::areAabbsOverlapping(const hkpBroadPhaseHandle* bhA, const hkpBroadPhaseHandle* bhB) const
{
    const hkpBpNode& a = m_nodes.m_data[bhA->m_id];
    const hkpBpNode& b = m_nodes.m_data[bhB->m_id];
    return !a.xyDisjoint(b) && !a.yzDisjoint(b);
}

// ---- add and remove

// 0x00D86210: the box at the collision input's 32-bit resolution, then the integer version.
void hkp3AxisSweep::addObject(hkpBroadPhaseHandle* object, const hkAabb& aabbIn, hkpBpPairArray& newPairs, bool border)
{
    alignas(16) hkAabbUint32 aabbInt;
    hkAabbUtil::convertAabbToUint32(aabbIn, m_offsetLow32bit, m_offsetHigh32bit, m_scale32bit, aabbInt);
    addObject(object, aabbInt, newPairs, border);
}

// 0x00D871C0
// A border object (the broadphase border's phantoms) is flattened onto the world's faces: each value goes to 0 below
// the middle (0x7FFE) and to AABB_MAX_VALUE above it.
void hkp3AxisSweep::addObject(hkpBroadPhaseHandle* object, const hkAabbUint32& aabbIn, hkpBpPairArray& newPairs, bool border)
{
    alignas(16) hkAabbUint32 aabb;
    convertAabbToBroadPhaseResolution(aabbIn, aabb);
    if (border)
    {
        for (int i = 0; i < 3; i++)
        {
            aabb.m_min[i] = aabb.m_min[i] < 0x7FFE ? 0 : AABB_MAX_VALUE;
            aabb.m_max[i] = ((int)aabb.m_max[i] < 0x7FFE ? 0 : AABB_MAX_VALUE) + 1;
        }
    }
    const int nodeIndex = m_nodes.m_size;
    hkpBpNode& newNode = m_nodes.expandOne();
    hkpBpNode* nodes = m_nodes.m_data;
    m_axis[0].insert(nodes, nodeIndex, (unsigned short)aabb.m_min[0], (unsigned short)aabb.m_max[0], newNode.min_x, newNode.max_x);
    m_axis[1].insert(nodes, nodeIndex, (unsigned short)aabb.m_min[1], (unsigned short)aabb.m_max[1], newNode.min_y, newNode.max_y);
    m_axis[2].insert(nodes, nodeIndex, (unsigned short)aabb.m_min[2], (unsigned short)aabb.m_max[2], newNode.min_z, newNode.max_z);
    updateNodesAfterInsert(nodes, nodeIndex, newNode);
    newNode.m_handle = object;
    object->m_id = nodeIndex;
    const int numNodes = m_nodes.m_size;
    hkArray<hkUint32, hkContainerTempAllocator> bitField((numNodes >> 5) + 8);
    setBitsBasedOnXInterval(numNodes, (int)aabb.m_min[0], newNode, (unsigned short)nodeIndex, bitField.m_data);
    querySingleAabbAddObject(object, nodeIndex, bitField.m_data, newNode, newPairs);
}

// 0x00D87420
// The last node moves into the removed node's slot (its endpoints, its handle's id, the markers listing it), and its
// endpoints are then reordered among equal values.
void hkp3AxisSweep::removeObject(hkpBroadPhaseHandle* object, hkpBpPairArray& delPairsOut)
{
    const unsigned int nodeIndex = object->m_id;
    const int numNodes = m_nodes.m_size;
    hkpBpNode& node = m_nodes.m_data[nodeIndex];
    {
        hkArray<hkUint32, hkContainerTempAllocator> bitField((numNodes >> 5) + 8);
        setBitsBasedOnXInterval(numNodes, m_axis[0].m_endPoints.m_data[node.min_x].m_value, node, (unsigned short)nodeIndex, bitField.m_data);
        querySingleAabbRemoveObject(object, nodeIndex, bitField.m_data, node, delPairsOut);
    }
    hkpBpNode* nodes = m_nodes.m_data;
    m_axis[0].remove(node.min_x, node.max_x);
    m_axis[1].remove(node.min_y, node.max_y);
    m_axis[2].remove(node.min_z, node.max_z);
    updateNodesAfterDelete(nodes, numNodes, node);
    if (nodeIndex < (unsigned int)(numNodes - 1))
    {
        node = m_nodes.m_data[numNodes - 1];
        m_axis[0].m_endPoints.m_data[node.min_x].m_nodeIndex = (unsigned short)nodeIndex;
        m_axis[0].m_endPoints.m_data[node.max_x].m_nodeIndex = (unsigned short)nodeIndex;
        if (node.isMarker())
        {
            node.getMarker(m_markers).m_nodeIndex = (unsigned short)nodeIndex;
        }
        else
        {
            m_axis[1].m_endPoints.m_data[node.min_y].m_nodeIndex = (unsigned short)nodeIndex;
            m_axis[1].m_endPoints.m_data[node.max_y].m_nodeIndex = (unsigned short)nodeIndex;
            m_axis[2].m_endPoints.m_data[node.min_z].m_nodeIndex = (unsigned short)nodeIndex;
            m_axis[2].m_endPoints.m_data[node.max_z].m_nodeIndex = (unsigned short)nodeIndex;
            node.m_handle->m_id = nodeIndex;
        }
        if (m_numMarkers != 0 && node.isNotMarker())
        {
            // The markers whose interval the moved node overlaps list it under its old index. The original replaces
            // the entry without checking it was found (index -1 writes before the array; kept).
            const int shift = 16 - m_ld2NumMarkers;
            unsigned int first = m_axis[0].m_endPoints.m_data[node.min_x].m_value >> shift;
            if (first != 0 && node.min_x < m_nodes.m_data[m_markers[first - 1].m_nodeIndex].max_x)
            {
                first--;
            }
            const int last = (m_axis[0].m_endPoints.m_data[node.max_x].m_value >> shift) - 1;
            for (int i = (int)first; i <= last; i++)
            {
                hkArray<unsigned short, hkContainerHeapAllocator>& list = m_markers[i].m_overlappingObjects;
                const int index = list.indexOf((unsigned short)(numNodes - 1));
                list.m_data[index] = (unsigned short)nodeIndex;
            }
        }
        _fixDeterministicOrderAfterNodeIdWasDecreased(nodeIndex);
    }
    m_nodes.setSize(numNodes - 1);
}

// 0x00D85FB0: the pairs among nodes sorted by min_x (ending with a 0xFFFF sentinel). Each node meets the following
// nodes that start before its x maximum; the second node is the one that can be a marker (skipped).
void hkp3AxisSweep::collide1Axis(const hkpBpNode* pa, int numA, hkpBpPairArray& pairsOut)
{
    for (numA--; numA > 0; numA--, pa++)
    {
        const BpInt maxX = pa->max_x;
        const hkpBpNode* pb = pa + 1;
        if (pb->min_x >= maxX)
        {
            continue;
        }
        do
        {
            if (!pb->yzDisjoint(*pa) && pb->isNotMarker())
            {
                hk3AxisSweep_addPair(pairsOut, pa->m_handle, pb->m_handle);
            }
            pb++;
        } while (pb->min_x < maxX);
    }
}

// 0x00D85CC0: the pairs between two lists sorted by min_x (each ending with a 0xFFFF sentinel), as a merged sweep.
// The pair is (a, b); markers can only be in the b list and are handled as markerHandlingForNodesB says (a marker
// removing an object it does not list writes before its array; kept).
void hkp3AxisSweep::collide1Axis(const hkpBpNode* pa, int numA, const hkpBpNode* pb, int numB, MarkerHandling markerHandlingForNodesB, hkpBpPairArray& pairsOut)
{
    hkpBpMarker* markers = m_markers;
    for (;;)
    {
        int remaining;
        if (pa->min_x > pb->min_x)
        {
            // b starts first: the a nodes starting before b's x maximum
            const BpInt maxX = pb->max_x;
            if (pa->min_x < maxX)
            {
                const hkpBpNode* a = pa;
                do
                {
                    if (!a->yzDisjoint(*pb))
                    {
                        if (pb->isNotMarker())
                        {
                            hk3AxisSweep_addPair(pairsOut, a->m_handle, pb->m_handle);
                        }
                        else if (markerHandlingForNodesB != MARKERS_IGNORE)
                        {
                            hkpBpMarker& marker = pb->getMarker(markers);
                            if (markerHandlingForNodesB == MARKERS_ADD_NEW_OVERLAPS)
                            {
                                marker.m_overlappingObjects.pushBack((unsigned short)a->m_handle->m_id);
                            }
                            else
                            {
                                hk3AxisSweep_removeFromMarker(marker, (unsigned short)a->m_handle->m_id);
                            }
                        }
                    }
                    a++;
                } while (a->min_x < maxX);
            }
            pb++;
            remaining = --numB;
        }
        else
        {
            // a starts first: the b nodes starting before a's x maximum
            const BpInt maxX = pa->max_x;
            if (pb->min_x < maxX)
            {
                const hkpBpNode* b = pb;
                do
                {
                    if (!b->yzDisjoint(*pa))
                    {
                        if (b->isNotMarker())
                        {
                            hk3AxisSweep_addPair(pairsOut, pa->m_handle, b->m_handle);
                        }
                        else if (markerHandlingForNodesB != MARKERS_IGNORE)
                        {
                            hkpBpMarker& marker = b->getMarker(markers);
                            if (markerHandlingForNodesB == MARKERS_ADD_NEW_OVERLAPS)
                            {
                                marker.m_overlappingObjects.pushBack((unsigned short)pa->m_handle->m_id);
                            }
                            else
                            {
                                hk3AxisSweep_removeFromMarker(marker, (unsigned short)pa->m_handle->m_id);
                            }
                        }
                    }
                    b++;
                } while (b->min_x < maxX);
            }
            pa++;
            remaining = --numA;
        }
        if (remaining < 1)
        {
            return;
        }
    }
}

// 0x00D8A010
// The new endpoints are sorted and merged into the axes; the pairs come from a one-axis sweep among the new nodes and
// between them and the old nodes overlapping the batch's box (markers there record the new nodes).
void hkp3AxisSweep::addObjectBatch(const hkArrayBase<hkpBroadPhaseHandle*>& addObjectList, const hkArrayBase<hkAabb>& addAabbList, hkpBpPairArray& newPairs)
{
    if (addAabbList.m_size < 1)
    {
        return;
    }
    const int numNew = addObjectList.m_size;
    const int numOld = m_nodes.m_size;
    const int numNewEndPoints = numNew * 2;
    hkArray<hkpBpEndPoint, hkContainerTempAllocator> newEndPoints0(numNewEndPoints);
    hkArray<hkpBpEndPoint, hkContainerTempAllocator> newEndPoints1(numNewEndPoints);
    hkArray<hkpBpEndPoint, hkContainerTempAllocator> newEndPoints2(numNewEndPoints);
    hkArray<const hkpBpNode*, hkContainerTempAllocator> overlaps;
    if (numOld > 0)
    {
        overlaps.reserve(numOld);
    }
    m_nodes.setSize(numOld + numNew);

    hkAabb batchAabb;
    batchAabb.m_min = hkVector4f::getConstant<HK_QUADREAL_MAX>();
    batchAabb.m_max.setNeg<4>(hkVector4f::getConstant<HK_QUADREAL_MAX>());
    hkpBpEndPoint* endPoints[3] = { newEndPoints0.m_data, newEndPoints1.m_data, newEndPoints2.m_data };
    for (int i = 0; i < numNew; i++)
    {
        const hkAabb& aabbIn = addAabbList.m_data[i];
        alignas(16) hkAabbUint32 aabb;
        convertAabbToInt(aabbIn, aabb);
        batchAabb.m_min.setMin(batchAabb.m_min, aabbIn.m_min);
        batchAabb.m_max.setMax(batchAabb.m_max, aabbIn.m_max);
        const int nodeIndex = numOld + i;
        hkpBroadPhaseHandle* object = addObjectList.m_data[i];
        m_nodes.m_data[nodeIndex].m_handle = object;
        object->m_id = nodeIndex;
        for (int axis = 0; axis < 3; axis++)
        {
            endPoints[axis][i * 2].m_value = (unsigned short)aabb.m_min[axis];
            endPoints[axis][i * 2].m_nodeIndex = (unsigned short)nodeIndex;
            endPoints[axis][i * 2 + 1].m_value = (unsigned short)aabb.m_max[axis];
            endPoints[axis][i * 2 + 1].m_nodeIndex = (unsigned short)nodeIndex;
        }
    }
    m_nodes.setSize(numOld);
    _querySingleAabb(batchAabb, HK_BP_REPORT_NODES, HK_NULL, &overlaps, HK_NULL);
    m_nodes.setSize(numOld + numNew);
    hkpBpNode* nodes = m_nodes.m_data;

    for (int axis = 0; axis < 3; axis++)
    {
        hkAlgorithm::quickSort(endPoints[axis], numNewEndPoints, hkAlgorithm::less<hkpBpEndPoint>());
    }
    const int tail0 = m_axis[0].insertTail<0>(nodes, newEndPoints0.m_data, numNewEndPoints);
    const int tail1 = m_axis[1].insertTail<1>(nodes, newEndPoints1.m_data, numNewEndPoints);
    const int tail2 = m_axis[2].insertTail<2>(nodes, newEndPoints2.m_data, numNewEndPoints);
    m_axis[0].mergeRest<0>(nodes, tail0, newEndPoints0.m_data, numNewEndPoints);
    m_axis[1].mergeRest<1>(nodes, tail1, newEndPoints1.m_data, numNewEndPoints);
    m_axis[2].mergeRest<2>(nodes, tail2, newEndPoints2.m_data, numNewEndPoints);
    if (m_numMarkers != 0)
    {
        // the markers span all of y and z
        for (int i = 0; i < m_numMarkers; i++)
        {
            hkpBpNode& marker = m_nodes.m_data[m_markers[i].m_nodeIndex];
            marker.max_y = (unsigned short)(marker.max_y + numNewEndPoints);
            marker.max_z = (unsigned short)(marker.max_z + numNewEndPoints);
        }
    }

    hkArray<hkpBpNode, hkContainerTempAllocator> newNodes(numNew + 1);
    {
        hkArray<hkpBpSortData, hkContainerTempAllocator> keys(numNew + 4);
        for (int i = 0; i < numNew; i++)
        {
            keys.m_data[i].m_userData = (unsigned short)(numOld + i);
            keys.m_data[i].m_key = m_nodes.m_data[numOld + i].min_x;
        }
        hk3AxisSweep_sortNodesByMinX(m_nodes.m_data, keys.m_data, numNew, newNodes.m_data);
    }
    const int numOverlaps = overlaps.m_size;
    hkArray<hkpBpNode, hkContainerTempAllocator> oldNodes(numOverlaps + 1);
    {
        hkArray<hkpBpSortData, hkContainerTempAllocator> keys(numOverlaps + 4);
        const hkpBpNode* base = m_nodes.m_data;
        for (int i = 0; i < numOverlaps; i++)
        {
            const hkpBpNode* node = overlaps.m_data[i];
            keys.m_data[i].m_userData = (unsigned short)(node - base);
            keys.m_data[i].m_key = node->min_x;
        }
        hk3AxisSweep_sortNodesByMinX(m_nodes.m_data, keys.m_data, numOverlaps, oldNodes.m_data);
    }
    collide1Axis(newNodes.m_data, numNew, newPairs);
    collide1Axis(newNodes.m_data, numNew, oldNodes.m_data, numOverlaps, MARKERS_ADD_NEW_OVERLAPS, newPairs);
}

// 0x00D87740
// The removed nodes get a dummy handle; the nodes at the end move into their slots (relocation table), the endpoints
// are filtered, and the pairs come from a one-axis sweep among the removed nodes and between them and the remaining
// nodes overlapping the removed nodes' combined index box (markers ignored: their lists are relocated instead).
void hkp3AxisSweep::removeObjectBatch(const hkArrayBase<hkpBroadPhaseHandle*>& removeObjectList, hkpBpPairArray& delPairsOut)
{
    const int numNodes = m_nodes.m_size;
    const int numRemove = removeObjectList.m_size;
    hkpBroadPhaseHandle removedHandle;
    removedHandle.m_id = 0;
    hkArray<hkpBpNode, hkContainerTempAllocator> removedNodes(numRemove + 1);

    // the box of the removed nodes' endpoint indices
    hkpBpNode box = m_nodes.m_data[removeObjectList.m_data[0]->m_id];
    {
        hkArray<hkpBpSortData, hkContainerTempAllocator> keys(numRemove + 4);
        for (int i = 0; i < numRemove; i++)
        {
            const unsigned int id = removeObjectList.m_data[i]->m_id;
            const hkpBpNode& node = m_nodes.m_data[id];
            keys.m_data[i].m_userData = (unsigned short)id;
            keys.m_data[i].m_key = node.min_x;
            if (node.min_x <= box.min_x) box.min_x = node.min_x;
            if (node.min_y <= box.min_y) box.min_y = node.min_y;
            if (node.min_z <= box.min_z) box.min_z = node.min_z;
            if (box.max_x <= node.max_x) box.max_x = node.max_x;
            if (box.max_y <= node.max_y) box.max_y = node.max_y;
            if (box.max_z <= node.max_z) box.max_z = node.max_z;
        }
        hkArray<hkpBpSortData, hkContainerTempAllocator> buffer(numRemove + 4);
        keys.m_data[numRemove].m_key = 0xFFFF;
        keys.m_data[numRemove + 1].m_key = 0xFFFF;
        keys.m_data[numRemove + 2].m_key = 0xFFFF;
        hkRadixSort::sort16(keys.m_data, (numRemove + 3) & ~3, buffer.m_data);
        for (int i = 0; i < numRemove; i++)
        {
            hkpBpNode& node = m_nodes.m_data[keys.m_data[i].m_userData];
            removedNodes.m_data[i] = node;
            node.m_handle->m_id = 0;
            node.m_handle = &removedHandle;
        }
        removedNodes.m_data[numRemove].min_x = 0xFFFF;
    }

    hkArray<int, hkContainerTempAllocator> movedNodes;
    if (numRemove + m_numMarkers > 0)
    {
        movedNodes.reserve(numRemove + m_numMarkers);
    }
    hkArray<int, hkContainerTempAllocator> relocations(numNodes);
    hkArray<hkpBpSortData, hkContainerTempAllocator> overlapKeys(numNodes - numRemove + 4);
    int numOverlaps = 0;
    int last = numNodes - 1;
    relocations.m_data[0] = 0;
    for (int i = 1; i <= last; )
    {
        hkpBpNode* nodes = m_nodes.m_data;
        if (nodes[last].m_handle == &removedHandle)
        {
            relocations.m_data[last] = -1;
            last--;
            continue;
        }
        hkpBpNode& node = nodes[i];
        if (node.m_handle == &removedHandle)
        {
            node = nodes[last];
            relocations.m_data[last] = i;
            relocations.m_data[i] = -1;
            movedNodes.m_data[movedNodes.m_size++] = i; // reserved
            last--;
            if (node.isMarker())
            {
                node.getMarker(m_markers).m_nodeIndex = (unsigned short)i;
                i++;
                continue;
            }
            node.m_handle->m_id = i;
        }
        else
        {
            relocations.m_data[i] = i;
        }
        if (!node.yzDisjoint(box) && !node.xyDisjoint(box))
        {
            overlapKeys.m_data[numOverlaps].m_key = node.min_x;
            overlapKeys.m_data[numOverlaps].m_userData = (unsigned short)i;
            numOverlaps++;
        }
        i++;
    }
    m_nodes.setSize(last + 1);

    hkArray<hkpBpNode, hkContainerTempAllocator> overlapNodes(numOverlaps + 1);
    {
        hkArray<hkpBpSortData, hkContainerTempAllocator> buffer(numOverlaps + 4);
        overlapKeys.m_data[numOverlaps + 1].m_key = 0xFFFF;
        overlapKeys.m_data[numOverlaps].m_key = 0xFFFF;
        overlapKeys.m_data[numOverlaps + 2].m_key = 0xFFFF;
        hkRadixSort::sort16(overlapKeys.m_data, (numOverlaps + 3) & ~3, buffer.m_data);
        for (int i = 0; i < numOverlaps; i++)
        {
            overlapNodes.m_data[i] = m_nodes.m_data[overlapKeys.m_data[i].m_userData];
        }
        overlapNodes.m_data[numOverlaps].min_x = 0xFFFF;
    }
    collide1Axis(removedNodes.m_data, numRemove, delPairsOut);
    collide1Axis(removedNodes.m_data, numRemove, overlapNodes.m_data, numOverlaps, MARKERS_IGNORE, delPairsOut);

    m_axis[0].removeBatch<0>(m_nodes.m_data, relocations);
    m_axis[1].removeBatch<1>(m_nodes.m_data, relocations);
    m_axis[2].removeBatch<2>(m_nodes.m_data, relocations);
    if (m_numMarkers != 0)
    {
        const int numRemovedEndPoints = numRemove * 2;
        for (int i = 0; i < m_numMarkers; i++)
        {
            hkpBpMarker& marker = m_markers[i];
            hkpBpNode& markerNode = m_nodes.m_data[marker.m_nodeIndex];
            markerNode.max_y = (unsigned short)(markerNode.max_y - numRemovedEndPoints);
            markerNode.max_z = (unsigned short)(markerNode.max_z - numRemovedEndPoints);
            hkArray<unsigned short, hkContainerHeapAllocator>& list = marker.m_overlappingObjects;
            int numKept = 0;
            for (int j = 0; j < list.m_size; j++)
            {
                const int relocated = relocations.m_data[list.m_data[j]];
                if (relocated >= 0)
                {
                    list.m_data[numKept++] = (unsigned short)relocated;
                }
            }
            list.setSize(numKept);
        }
    }
    for (int i = 0; i < movedNodes.m_size; i++)
    {
        _fixDeterministicOrderAfterNodeIdWasDecreased(movedNodes.m_data[i]);
    }
}

// 0x00D885B0
// Renumbers the nodes in x order (the world node stays first), so that nodes close in x are close in memory.
void hkp3AxisSweep::defragment()
{
    const int numNodes = m_nodes.m_size;
    hkArray<hkpBpNode, hkContainerTempAllocator> nodesCopy(numNodes);
    hkArray<ValueIntPair16, hkContainerTempAllocator> order(numNodes);
    hkArray<int, hkContainerTempAllocator> relocations(numNodes);
    for (int i = 0; i < m_nodes.m_size; i++)
    {
        nodesCopy.m_data[i] = m_nodes.m_data[i];
        order.m_data[i].m_value = m_nodes.m_data[i].min_x;
        order.m_data[i].m_oldIndex = (unsigned short)i;
    }
    hkAlgorithm::quickSort(order.m_data + 1, numNodes - 1, hkAlgorithm::less<ValueIntPair16>());
    for (int i = 0; i < m_nodes.m_size; i++)
    {
        const int oldIndex = order.m_data[i].m_oldIndex;
        relocations.m_data[oldIndex] = i;
        m_nodes.m_data[i] = nodesCopy.m_data[oldIndex];
    }
    for (int i = 1; i < m_nodes.m_size; i++)
    {
        hkpBpNode& node = m_nodes.m_data[i];
        if (node.isMarker())
        {
            node.getMarker(m_markers).m_nodeIndex = (unsigned short)i;
        }
        else
        {
            node.m_handle->m_id = i;
        }
    }
    for (int i = 0; i < m_numMarkers; i++)
    {
        hkArray<unsigned short, hkContainerHeapAllocator>& list = m_markers[i].m_overlappingObjects;
        for (int j = list.m_size - 1; j >= 0; j--)
        {
            list.m_data[j] = (unsigned short)relocations.m_data[list.m_data[j]];
        }
    }
    for (int axis = 0; axis < 3; axis++)
    {
        hkArray<hkpBpEndPoint, hkContainerHeapAllocator>& endPoints = m_axis[axis].m_endPoints;
        for (int i = 0; i < endPoints.m_size; i++)
        {
            endPoints.m_data[i].m_nodeIndex = (unsigned short)relocations.m_data[endPoints.m_data[i].m_nodeIndex];
        }
    }
    for (int i = 1; i < m_nodes.m_size; i++)
    {
        _fixDeterministicOrderAfterNodeIdWasDecreased(i);
    }
}

// ---- boxes

// 0x00D849E0: the float box of a node's endpoints (the inverse scale by a refined reciprocal).
void hkp3AxisSweep::getAabbFromNode(const hkpBpNode& node, hkAabb& aabb) const
{
    hkVector4f invScale;
    invScale.setReciprocal<HK_ACC_23_BIT, HK_DIV_IGNORE>(m_scale);
    const hkpBpEndPoint* x = m_axis[0].m_endPoints.m_data;
    const hkpBpEndPoint* y = m_axis[1].m_endPoints.m_data;
    const hkpBpEndPoint* z = m_axis[2].m_endPoints.m_data;
    hkIntVector mins; mins.set(x[node.min_x].m_value, y[node.min_y].m_value, z[node.min_z].m_value, 0);
    hkIntVector maxs; maxs.set(x[node.max_x].m_value, y[node.max_y].m_value, z[node.max_z].m_value, 0);
    mins.convertS32ToF32(aabb.m_min);
    maxs.convertS32ToF32(aabb.m_max);
    aabb.m_max.mul(invScale);
    aabb.m_max.sub(m_offsetLow);
    aabb.m_min.mul(invScale);
    aabb.m_min.sub(m_offsetLow);
}

// 0x00D84F10
void hkp3AxisSweep::getAabb(const hkpBroadPhaseHandle* object, hkAabb& aabb) const
{
    getAabbFromNode(m_nodes.m_data[object->m_id], aabb);
}

// 0x00D84E70: the boxes of all nodes but the markers (the world first).
void hkp3AxisSweep::getAllAabbs(hkArray<hkAabb, hkContainerHeapAllocator>& allAabbs) const
{
    const int numAabbs = m_nodes.m_size - m_numMarkers;
    allAabbs.reserveExactly(numAabbs);
    allAabbs.m_size = m_nodes.m_size - m_numMarkers;
    hkAabb* out = allAabbs.m_data;
    for (int i = 0; i < m_nodes.m_size; i++)
    {
        const hkpBpNode& node = m_nodes.m_data[i];
        if (node.isNotMarker())
        {
            getAabbFromNode(node, *out++);
        }
    }
}

// 0x00D84970: the world box the integer range covers.
void hkp3AxisSweep::getExtents(hkVector4f& worldMinOut, hkVector4f& worldMaxOut) const
{
    worldMinOut.setNeg<4>(m_offsetLow);
    worldMaxOut.setReciprocal<HK_ACC_23_BIT, HK_DIV_IGNORE>(m_scale);
    worldMaxOut.setComponent<3>(hkSimdFloat32::getConstant<HK_QUADREAL_1>());
    hkVector4f maxVal; maxVal.m_quad = s_MaxVal;
    worldMaxOut.mul(maxVal);
    worldMaxOut.setAdd(worldMinOut, worldMaxOut);
}

// 0x00D842F0: three endpoint arrays (headers and storage) for every node but the markers, plus the sentinels.
int hkp3AxisSweep::getAabbCacheSize() const
{
    return (m_nodes.m_size - m_numMarkers) * 0x18 + 0x24;
}

// 0x00D867E0
// The cache is three endpoint arrays laid out like m_axis (each holding the given nodes' endpoints sorted, between a 0
// and a 0xFFFC sentinel), followed by their storage; the casts walk it instead of the full axes.
void hkp3AxisSweep::calcAabbCacheInternal(const hkArrayBase<hkpBpNode const*>& overlaps, char* AabbCacheOut) const
{
    hkArrayBase<hkpBpEndPoint>* cache = (hkArrayBase<hkpBpEndPoint>*)AabbCacheOut;
    char* storage = AabbCacheOut + 0x24;
    const int capacity = overlaps.m_size * 2 + 2;
    for (int axis = 0; axis < 3; axis++)
    {
        cache[axis].m_data = (hkpBpEndPoint*)(storage + axis * capacity * (int)sizeof(hkpBpEndPoint));
        cache[axis].m_size = 0;
        cache[axis].m_capacityAndFlags = capacity | hkArrayBase<hkpBpEndPoint>::DONT_DEALLOCATE_FLAG;
    }
    hkpBpEndPoint ep;
    ep.m_value = 0;
    ep.m_nodeIndex = 0;
    for (int axis = 0; axis < 3; axis++)
    {
        cache[axis].m_data[cache[axis].m_size++] = ep;
    }
    const hkpBpNode* const* node = overlaps.m_data;
    for (int i = overlaps.m_size - 1; i >= 0; i--, node++)
    {
        cache[0].m_data[cache[0].m_size++] = m_axis[0].m_endPoints.m_data[(*node)->min_x];
        cache[0].m_data[cache[0].m_size++] = m_axis[0].m_endPoints.m_data[(*node)->max_x];
        cache[1].m_data[cache[1].m_size++] = m_axis[1].m_endPoints.m_data[(*node)->min_y];
        cache[1].m_data[cache[1].m_size++] = m_axis[1].m_endPoints.m_data[(*node)->max_y];
        cache[2].m_data[cache[2].m_size++] = m_axis[2].m_endPoints.m_data[(*node)->min_z];
        cache[2].m_data[cache[2].m_size++] = m_axis[2].m_endPoints.m_data[(*node)->max_z];
    }
    for (int axis = 0; axis < 3; axis++)
    {
        hkAlgorithm::quickSort(cache[axis].m_data + 1, cache[axis].m_size - 1, hkAlgorithm::less<hkpBpEndPoint>());
    }
    ep.m_value = 0xFFFC;
    for (int axis = 0; axis < 3; axis++)
    {
        cache[axis].m_data[cache[axis].m_size++] = ep;
    }
}

// 0x00D883F0: the cache of the nodes overlapping a box.
void hkp3AxisSweep::calcAabbCache(const hkAabb& aabb, char* AabbCacheOut) const
{
    hkArray<const hkpBpNode*, hkContainerTempAllocator> overlaps;
    if (m_nodes.m_size > 0)
    {
        overlaps.reserve(m_nodes.m_size);
    }
    _querySingleAabb(aabb, HK_BP_REPORT_NODES, HK_NULL, &overlaps, HK_NULL);
    calcAabbCacheInternal(overlaps, AabbCacheOut);
}

// 0x00D88490: the cache of the given collidables' nodes.
void hkp3AxisSweep::calcAabbCache(const hkArrayBase<hkpCollidable*>& overlappingCollidables, char* AabbCacheOut) const
{
    hkLocalArray<const hkpBpNode*> overlaps(overlappingCollidables.m_size);
    for (int i = 0; i < overlappingCollidables.m_size; i++)
    {
        overlaps.m_data[overlaps.m_size++] = m_nodes.m_data + overlappingCollidables.m_data[i]->m_broadPhaseHandle.m_id;
    }
    calcAabbCacheInternal(overlaps, AabbCacheOut);
}

// ---- casts

// The axis of the smallest of x, y, z (the first on ties), as hkVector4f::getIndexOfMinComponent<3> does it in SSE.
static HK_FORCE_INLINE int hk3AxisSweep_minIndex3(__m128 d)
{
    const __m128 y = _mm_shuffle_ps(d, d, 0x55);
    const __m128 x = _mm_shuffle_ps(d, d, 0x00);
    const __m128 z = _mm_shuffle_ps(d, d, 0xAA);
    __m128 m = _mm_min_ps(y, x);
    m = _mm_min_ps(z, m);
    const __m128 mask = _mm_and_ps(_mm_cmple_ps(d, m), _mm_castsi128_ps(*(const __m128i*)&hkVector4fComparison_maskToComparison[7 * 4]));
    return hkVector4Comparison_maskToFirstIndex[_mm_movemask_ps(mask)];
}

// The x, y, z minimum (all lanes).
static HK_FORCE_INLINE __m128 hk3AxisSweep_min3(__m128 d)
{
    const __m128 y = _mm_shuffle_ps(d, d, 0x55);
    const __m128 x = _mm_shuffle_ps(d, d, 0x00);
    const __m128 z = _mm_shuffle_ps(d, d, 0xAA);
    return _mm_min_ps(z, _mm_min_ps(y, x));
}

// Lane i of v (mask select and OR of the lanes), as the original reads one component.
static HK_FORCE_INLINE __m128 hk3AxisSweep_selectLane(__m128 v, int i)
{
    const __m128 masked = _mm_and_ps(_mm_castsi128_ps(*(const __m128i*)&hkVector4fComparison_maskToComparison[(1 << i) * 4]), v);
    const __m128 a = _mm_or_ps(_mm_shuffle_ps(masked, masked, 0x4E), masked);
    return _mm_or_ps(_mm_shuffle_ps(a, a, 0xB1), a);
}

// v with lane i replaced by value's lane i.
static HK_FORCE_INLINE __m128 hk3AxisSweep_setLane(__m128 v, __m128 value, int i)
{
    const __m128 mask = _mm_castsi128_ps(*(const __m128i*)&hkVector4fComparison_maskToComparison[(1 << i) * 4]);
    return _mm_or_ps(_mm_and_ps(mask, value), _mm_andnot_ps(mask, v));
}

static HK_FORCE_INLINE float hk3AxisSweep_lane(__m128 v, int i)
{
    return v.m128_f32[i];
}

static HK_FORCE_INLINE void hk3AxisSweep_setLaneValue(__m128& v, int i, float value)
{
    v.m128_f32[i] = value;
}

// The endpoint values of three axis pointers as floats (w 0).
static HK_FORCE_INLINE __m128 hk3AxisSweep_values(hkpBpEndPoint* const* ptrs)
{
    return _mm_cvtepi32_ps(_mm_set_epi32(0, ptrs[2]->m_value, ptrs[1]->m_value, ptrs[0]->m_value));
}

// 0x00D888E0
// Per node one byte: the low nibble holds the axes on which the ray's start is inside the node's interval (shared by
// all rays), the high nibble the running state of the current ray. Starting inside a node (0x77) reports it to every
// ray first. Each ray then steps through the endpoints in the order the ray reaches them (the next one per axis is at
// distance value / dir - start, scaled to [0, 1]); a node whose high nibble becomes 7 is entered and reported, and the
// collector's return value shortens the ray. Crossing the world's endpoints (node 0, byte 0x88) ends that axis
// (distance 2). Axes the ray is parallel to get distance 2 too.
void hkp3AxisSweep::castRay(const hkpBroadPhase::hkpCastRayInput& rayInput, hkpBroadPhaseCastCollector* collectorBase, int collectorStriding) const
{
    alignas(16) unsigned int fromInt[4];
    convertVectorToInt(rayInput.m_from, fromInt);
    hkArray<hkUint8, hkContainerTempAllocator> state(m_nodes.m_size + 16);
    {
        const __m128i zero = _mm_setzero_si128();
        hkUint8* p = state.m_data;
        for (int i = m_nodes.m_size >> 4; i >= 0; i--, p += 16)
        {
            _mm_store_si128((__m128i*)p, zero);
        }
    }

    // the start state, one axis bit in each nibble; startPtrs: the first endpoint above the start on each axis
    const hkpBpAxis* axes = rayInput.m_aabbCacheInfo != HK_NULL ? (const hkpBpAxis*)rayInput.m_aabbCacheInfo : m_axis;
    hkpBpEndPoint* startPtrs[3];
    hkUint8 bit = 0x11;
    for (int axis = 0; axis < 3; axis++, bit <<= 1)
    {
        const int size = axes[axis].m_endPoints.m_size;
        hkpBpEndPoint* data = axes[axis].m_endPoints.m_data;
        const unsigned int value = fromInt[axis];
        hkpBpEndPoint* ep;
        if (value < data[size >> 1].m_value)
        {
            ep = data + 1;
            while (ep < data + size - 4 && ep[3].m_value <= value)
            {
                state.m_data[ep[0].m_nodeIndex] ^= bit;
                state.m_data[ep[1].m_nodeIndex] ^= bit;
                state.m_data[ep[2].m_nodeIndex] ^= bit;
                state.m_data[ep[3].m_nodeIndex] ^= bit;
                ep += 4;
            }
            while (ep->m_value <= value)
            {
                state.m_data[ep->m_nodeIndex] ^= bit;
                ep++;
            }
        }
        else
        {
            ep = data + size - 2;
            while (ep >= data + 4 && value < ep[-3].m_value)
            {
                state.m_data[ep[0].m_nodeIndex] ^= bit;
                state.m_data[ep[-1].m_nodeIndex] ^= bit;
                state.m_data[ep[-2].m_nodeIndex] ^= bit;
                state.m_data[ep[-3].m_nodeIndex] ^= bit;
                ep -= 4;
            }
            while (value < ep->m_value)
            {
                state.m_data[ep->m_nodeIndex] ^= bit;
                ep--;
            }
            ep++;
        }
        startPtrs[axis] = ep;
    }

    // hkLocalBuffer<hkReal> earlyOuts(numCasts) on the LIFO allocator
    const int numCasts = rayInput.m_numCasts;
    const int lifoBytes = (numCasts * (int)sizeof(float) + 127) & ~127;
    float* earlyOuts = (float*)hkMemoryRouter::getInstance().stack().fastBlockAlloc(lifoBytes);
    for (int i = 0; i < rayInput.m_numCasts; i++)
    {
        earlyOuts[i] = 1.0f;
    }

    // the nodes containing the start (a byte of 0x77; the +1 test finds a low nibble of 7 in four bytes at once)
    {
        const hkUint8* p = state.m_data;
        const hkUint8* end = state.m_data + (m_nodes.m_size >> 2) * 4 + 4;
        const hkpBpNode* nodes = m_nodes.m_data;
        while (p < end)
        {
            if (((*(const hkUint32*)p + 0x01010101u) & 0x08080808u) == 0)
            {
                if (((*(const hkUint32*)(p + 4) + 0x01010101u) & 0x08080808u) == 0)
                {
                    if (((*(const hkUint32*)(p + 8) + 0x01010101u) & 0x08080808u) == 0)
                    {
                        p += 12;
                        nodes += 12;
                    }
                    else
                    {
                        p += 8;
                        nodes += 8;
                    }
                }
                else
                {
                    p += 4;
                    nodes += 4;
                }
                continue;
            }
            for (int j = 0; j < 4; j++)
            {
                if (p[j] != 0x77 || nodes[j].isMarker())
                {
                    continue;
                }
                hkpBroadPhaseCastCollector* collector = collectorBase;
                for (int i = 0; i < rayInput.m_numCasts; i++)
                {
                    const float earlyOut = earlyOuts[i];
                    const float hit = collector->addBroadPhaseHandle(nodes[j].m_handle, i);
                    earlyOuts[i] = _mm_cvtss_f32(_mm_min_ss(_mm_set_ss(earlyOut), _mm_set_ss(hit)));
                    collector = (hkpBroadPhaseCastCollector*)((char*)collector + collectorStriding);
                }
            }
            p += 4;
            nodes += 4;
        }
    }
    state.m_data[0] = 0x88; // the world node: crossing its endpoints ends the axis

    const int numNodesForReset = m_nodes.m_size;
    hkpBroadPhaseCastCollector* collector = collectorBase;
    for (int castIndex = 0; castIndex < rayInput.m_numCasts; castIndex++)
    {
        __m128 earlyOut = _mm_set1_ps(earlyOuts[castIndex]);
        hkpBpEndPoint* ptrs[3] = { startPtrs[0], startPtrs[1], startPtrs[2] };
        int steps[3] = { 0, 0, 0 }; // parallel axes never step (distance 2)
        const hkVector4f& to = *(const hkVector4f*)((const char*)rayInput.m_toBase + rayInput.m_toStriding * castIndex);
        const __m128 toQuad = _mm_loadu_ps((const float*)&to);
        const __m128 from = rayInput.m_from.m_quad;
        const __m128 scale = m_scale.m_quad;
        const __m128 dir = _mm_mul_ps(_mm_sub_ps(toQuad, from), scale);
        const __m128 rcp = _mm_rcp_ps(dir);
        __m128 invDir = _mm_mul_ps(_mm_sub_ps(*(const __m128*)hkMath::hkSse_floatTwo, _mm_mul_ps(rcp, dir)), rcp);
        const __m128 offsetLow = m_offsetLow.m_quad;
        const __m128 fromScaled = _mm_mul_ps(_mm_add_ps(from, offsetLow), scale);
        __m128 start = _mm_mul_ps(_mm_sub_ps(fromScaled, m_intToFloatFloorCorrection.m_real), invDir);
        const __m128 eps = hkVector4f::getConstant<HK_QUADREAL_EPS>().m_quad;
        const __m128 absMask = _mm_castsi128_ps(_mm_srli_epi32(_mm_set1_epi32(-1), 1));
        const __m128 toEps = _mm_and_ps(_mm_mul_ps(_mm_mul_ps(_mm_add_ps(offsetLow, toQuad), scale), eps), absMask);
        const __m128 fromEps = _mm_and_ps(_mm_mul_ps(eps, fromScaled), absMask);
        const int negative = _mm_movemask_ps(_mm_cmplt_ps(dir, _mm_setzero_ps()));
        const __m128 absDir = _mm_and_ps(dir, absMask);
        const int parallel = _mm_movemask_ps(_mm_or_ps(_mm_cmplt_ps(absDir, toEps), _mm_cmplt_ps(absDir, fromEps)));
        const __m128 two = hkVector4f::getConstant<HK_QUADREAL_2>().m_quad;
        for (int axis = 0; axis < 3; axis++)
        {
            if (parallel & (1 << axis))
            {
                hk3AxisSweep_setLaneValue(invDir, axis, 0.0f);
                start = hk3AxisSweep_setLane(start, _mm_sub_ps(_mm_setzero_ps(), two), axis);
            }
            else
            {
                steps[axis] = 1;
                if (negative & (1 << axis))
                {
                    ptrs[axis]--;
                    steps[axis] = -1;
                }
            }
        }
        __m128 dist = _mm_sub_ps(_mm_mul_ps(hk3AxisSweep_values(ptrs), invDir), start);

        for (;;)
        {
            const __m128 minDist = hk3AxisSweep_min3(dist);
            const int axis = hk3AxisSweep_minIndex3(dist);
            if (_mm_cvtss_f32(minDist) > _mm_cvtss_f32(earlyOut))
            {
                break;
            }
            const hkUint8 axisBit = (hkUint8)(1 << (axis + 4));
            bool worldExit = false;
            for (;;)
            {
                hkpBpEndPoint* ep = ptrs[axis];
                const int nodeIndex = ep->m_nodeIndex;
                state.m_data[nodeIndex] ^= axisBit;
                if (state.m_data[nodeIndex] >= 0x70)
                {
                    if (nodeIndex == 0)
                    {
                        dist = hk3AxisSweep_setLane(dist, two, axis);
                        worldExit = true;
                        break;
                    }
                    const hkpBpNode& node = m_nodes.m_data[nodeIndex];
                    if (node.isNotMarker())
                    {
                        const float hit = collector->addBroadPhaseHandle(node.m_handle, castIndex);
                        earlyOut = _mm_min_ps(earlyOut, _mm_set1_ps(hit));
                    }
                }
                const BpInt value = ep->m_value;
                ptrs[axis] = ep + steps[axis];
                if (value != ptrs[axis]->m_value)
                {
                    break;
                }
            }
            if (!worldExit)
            {
                const float value = (float)(int)ptrs[axis]->m_value;
                hk3AxisSweep_setLaneValue(dist, axis, value * hk3AxisSweep_lane(invDir, axis) - hk3AxisSweep_lane(start, axis));
            }
        }

        if (castIndex < rayInput.m_numCasts - 1)
        {
            // restore the start state for the next ray: the low nibbles into the high ones
            hkUint32* p = (hkUint32*)state.m_data;
            hkUint32* end = (hkUint32*)state.m_data + (numNodesForReset >> 2) + 1;
            for (; p < end; p += 2)
            {
                const hkUint32 a = p[0] & 0x0F0F0F0F;
                const hkUint32 b = p[1] & 0x0F0F0F0F;
                p[0] = (a << 4) | a;
                p[1] = (b << 4) | b;
            }
        }
        collector = (hkpBroadPhaseCastCollector*)((char*)collector + collectorStriding);
    }

    hkMemoryRouter::getInstance().stack().fastBlockFree(earlyOuts, lifoBytes);
}

// 0x00D89250
// The box version of castRay with one nibble per node: the start box marks the nodes it overlaps (7 = all three
// axes), which are reported first. Each axis then has a leading face (entering nodes: their minimum endpoints when
// moving up) and a trailing face (leaving nodes), each stepping through the endpoints in distance order; only the
// leading face reports. Note the parallel test here compares |dir| against the signed (not absolute) scaled positions.
void hkp3AxisSweep::castAabb(const hkpBroadPhase::hkpCastAabbInput& input, hkpBroadPhaseCastCollector& collector) const
{
    hkVector4f boxMax; boxMax.setAdd(input.m_from, input.m_halfExtents);
    hkVector4f boxMin; boxMin.setSub(input.m_from, input.m_halfExtents);
    alignas(16) unsigned int lowInt[4];
    alignas(16) unsigned int highInt[4];
    convertVectorToInt(boxMin, lowInt);
    convertVectorToInt(boxMax, highInt);
    hkArray<hkUint8, hkContainerTempAllocator> state(m_nodes.m_size + 16);
    {
        const __m128i zero = _mm_setzero_si128();
        hkUint8* p = state.m_data;
        for (int i = m_nodes.m_size >> 4; i >= 0; i--, p += 16)
        {
            _mm_store_si128((__m128i*)p, zero);
        }
    }

    // lowPtrs: the first endpoint above the box minimum, highPtrs: the first above the box maximum
    const hkpBpAxis* axes = input.m_aabbCacheInfo != HK_NULL ? (const hkpBpAxis*)input.m_aabbCacheInfo : m_axis;
    hkpBpEndPoint* lowPtrs[3];
    hkpBpEndPoint* highPtrs[3];
    hkUint8 bit = 1;
    for (int axis = 0; axis < 3; axis++, bit <<= 1)
    {
        const int size = axes[axis].m_endPoints.m_size;
        hkpBpEndPoint* data = axes[axis].m_endPoints.m_data;
        const unsigned int low = lowInt[axis];
        const unsigned int high = highInt[axis];
        hkpBpEndPoint* ep;
        if (low < data[size >> 1].m_value)
        {
            ep = data + 1;
            while (ep < data + size - 4 && ep[3].m_value <= low)
            {
                state.m_data[ep[0].m_nodeIndex] ^= bit;
                state.m_data[ep[1].m_nodeIndex] ^= bit;
                state.m_data[ep[2].m_nodeIndex] ^= bit;
                state.m_data[ep[3].m_nodeIndex] ^= bit;
                ep += 4;
            }
            while (ep->m_value <= low)
            {
                state.m_data[ep->m_nodeIndex] ^= bit;
                ep++;
            }
            lowPtrs[axis] = ep;
            while (ep->m_value <= high)
            {
                state.m_data[ep->m_nodeIndex] ^= (hkUint8)(((ep->m_value & 1) - 1) & bit); // the minima entering the box
                ep++;
            }
            highPtrs[axis] = ep;
        }
        else
        {
            ep = data + size - 2;
            while (ep >= data + 4 && high < ep[-3].m_value)
            {
                state.m_data[ep[0].m_nodeIndex] ^= bit;
                state.m_data[ep[-1].m_nodeIndex] ^= bit;
                state.m_data[ep[-2].m_nodeIndex] ^= bit;
                state.m_data[ep[-3].m_nodeIndex] ^= bit;
                ep -= 4;
            }
            while (high < ep->m_value)
            {
                state.m_data[ep->m_nodeIndex] ^= bit;
                ep--;
            }
            highPtrs[axis] = ep + 1;
            while (low < ep->m_value)
            {
                state.m_data[ep->m_nodeIndex] ^= (hkUint8)((-(ep->m_value & 1)) & bit); // the maxima inside the box
                ep--;
            }
            lowPtrs[axis] = ep + 1;
        }
    }

    __m128 earlyOut = hkVector4f::getConstant<HK_QUADREAL_1>().m_quad;
    {
        const hkUint8* p = state.m_data;
        const hkUint8* end = state.m_data + (m_nodes.m_size >> 2) * 4 + 4;
        const hkpBpNode* nodes = m_nodes.m_data;
        for (; p < end; p += 4, nodes += 4)
        {
            if (((*(const hkUint32*)p + 0x01010101u) & 0x08080808u) == 0)
            {
                continue;
            }
            for (int j = 0; j < 4; j++)
            {
                if (p[j] == 7 && nodes[j].isNotMarker())
                {
                    const float hit = collector.addBroadPhaseHandle(nodes[j].m_handle, 0);
                    earlyOut = _mm_min_ps(earlyOut, _mm_set1_ps(hit));
                }
            }
        }
    }

    const __m128 to = input.m_to.m_quad;
    const __m128 scale = m_scale.m_quad;
    const __m128 offsetLow = m_offsetLow.m_quad;
    const __m128 dir = _mm_mul_ps(_mm_sub_ps(to, input.m_from.m_quad), scale);
    const __m128 toScaled = _mm_mul_ps(_mm_add_ps(to, offsetLow), scale);
    const __m128 rcp = _mm_rcp_ps(dir);
    __m128 invDir = _mm_mul_ps(_mm_sub_ps(*(const __m128*)hkMath::hkSse_floatTwo, _mm_mul_ps(rcp, dir)), rcp);
    const __m128 positiveMask = _mm_cmplt_ps(_mm_setzero_ps(), dir);
    // trailing face (A) and leading face (B) in integer space
    const __m128 trailing = _mm_mul_ps(_mm_add_ps(_mm_or_ps(_mm_andnot_ps(positiveMask, boxMax.m_quad), _mm_and_ps(positiveMask, boxMin.m_quad)), offsetLow), scale);
    __m128 startA = _mm_mul_ps(_mm_sub_ps(trailing, m_intToFloatFloorCorrection.m_real), invDir);
    const __m128 leading = _mm_mul_ps(_mm_add_ps(_mm_or_ps(_mm_and_ps(positiveMask, boxMax.m_quad), _mm_andnot_ps(positiveMask, boxMin.m_quad)), offsetLow), scale);
    __m128 startB = _mm_mul_ps(_mm_sub_ps(leading, m_intToFloatFloorCorrection.m_real), invDir);
    const __m128 eps = hkVector4f::getConstant<HK_QUADREAL_EPS>().m_quad;
    const __m128 absDir = _mm_and_ps(dir, _mm_castsi128_ps(_mm_srli_epi32(_mm_set1_epi32(-1), 1)));
    const int parallel = _mm_movemask_ps(_mm_or_ps(_mm_cmplt_ps(absDir, _mm_mul_ps(toScaled, eps)), _mm_cmplt_ps(absDir, _mm_mul_ps(eps, trailing))));
    const int positive = _mm_movemask_ps(positiveMask);
    const __m128 two = hkVector4f::getConstant<HK_QUADREAL_2>().m_quad;

    int steps[3];
    int trailingFlags[3]; // xor with an endpoint's max bit: 1 for the trailing face's leaving maxima
    int leadingFlags[3]; // 1 for the leading face's entering minima
    for (int axis = 0; axis < 3; axis++)
    {
        if (positive & (1 << axis))
        {
            steps[axis] = 1;
            trailingFlags[axis] = 0;
            leadingFlags[axis] = 1;
        }
        else
        {
            // moving down: the faces swap, and both start one endpoint lower
            hkpBpEndPoint* low = lowPtrs[axis];
            steps[axis] = -1;
            lowPtrs[axis] = highPtrs[axis];
            highPtrs[axis] = low;
            const unsigned int lowValue = lowInt[axis];
            lowInt[axis] = highInt[axis];
            highInt[axis] = lowValue;
            lowPtrs[axis]--;
            highPtrs[axis]--;
            trailingFlags[axis] = 1;
            leadingFlags[axis] = 0;
        }
        if (parallel & (1 << axis))
        {
            hk3AxisSweep_setLaneValue(invDir, axis, 0.0f);
            const __m128 minusTwo = _mm_sub_ps(_mm_setzero_ps(), two);
            startA = hk3AxisSweep_setLane(startA, minusTwo, axis);
            startB = hk3AxisSweep_setLane(startB, minusTwo, axis);
        }
    }
    hkpBpEndPoint* trailPtrs[3] = { lowPtrs[0], lowPtrs[1], lowPtrs[2] };
    hkpBpEndPoint* leadPtrs[3] = { highPtrs[0], highPtrs[1], highPtrs[2] };
    state.m_data[0] = 0x08; // the world node: crossing its endpoints ends the axis

    __m128 distA = _mm_sub_ps(_mm_mul_ps(hk3AxisSweep_values(trailPtrs), invDir), startA);
    __m128 distB = _mm_sub_ps(_mm_mul_ps(hk3AxisSweep_values(leadPtrs), invDir), startB);
    int axisA = hk3AxisSweep_minIndex3(distA);
    int axisB = hk3AxisSweep_minIndex3(distB);
    for (;;)
    {
        if (hk3AxisSweep_lane(distA, axisA) > hk3AxisSweep_lane(distB, axisB))
        {
            // the leading face reaches an endpoint
            const __m128 d = hk3AxisSweep_selectLane(distB, axisB);
            if (_mm_cvtss_f32(d) > _mm_cvtss_f32(earlyOut))
            {
                break;
            }
            bool worldExit = false;
            for (;;)
            {
                hkpBpEndPoint* ep = leadPtrs[axisB];
                const int nodeIndex = ep->m_nodeIndex;
                const int flip = ((ep->m_value & 1) ^ leadingFlags[axisB]) << axisB;
                state.m_data[nodeIndex] ^= (hkUint8)flip;
                if (state.m_data[nodeIndex] >= 7)
                {
                    if (nodeIndex == 0)
                    {
                        distB = hk3AxisSweep_setLane(distB, two, axisB);
                        worldExit = true;
                        break;
                    }
                    const hkpBpNode& node = m_nodes.m_data[nodeIndex];
                    if (flip != 0 && node.isNotMarker())
                    {
                        const float hit = collector.addBroadPhaseHandle(node.m_handle, 0);
                        earlyOut = _mm_min_ps(earlyOut, _mm_set1_ps(hit));
                    }
                }
                const BpInt value = ep->m_value;
                leadPtrs[axisB] = ep + steps[axisB];
                if (value != leadPtrs[axisB]->m_value)
                {
                    break;
                }
            }
            if (!worldExit)
            {
                const float value = (float)(int)leadPtrs[axisB]->m_value;
                hk3AxisSweep_setLaneValue(distB, axisB, value * hk3AxisSweep_lane(invDir, axisB) - hk3AxisSweep_lane(startB, axisB));
            }
            axisB = hk3AxisSweep_minIndex3(distB);
        }
        else
        {
            // the trailing face reaches an endpoint
            const __m128 d = hk3AxisSweep_selectLane(distA, axisA);
            if (_mm_cvtss_f32(d) > _mm_cvtss_f32(earlyOut))
            {
                break;
            }
            bool worldExit = false;
            for (;;)
            {
                hkpBpEndPoint* ep = trailPtrs[axisA];
                const int nodeIndex = ep->m_nodeIndex;
                const hkUint8 flip = (hkUint8)(((ep->m_value & 1) ^ trailingFlags[axisA]) << axisA);
                state.m_data[nodeIndex] ^= flip;
                if (state.m_data[nodeIndex] > 8)
                {
                    distA = hk3AxisSweep_setLane(distA, two, axisA);
                    worldExit = true;
                    break;
                }
                const BpInt value = ep->m_value;
                trailPtrs[axisA] = ep + steps[axisA];
                if (value != trailPtrs[axisA]->m_value)
                {
                    break;
                }
            }
            if (!worldExit)
            {
                const float value = (float)(int)trailPtrs[axisA]->m_value;
                hk3AxisSweep_setLaneValue(distA, axisA, value * hk3AxisSweep_lane(invDir, axisA) - hk3AxisSweep_lane(startA, axisA));
            }
            axisA = hk3AxisSweep_minIndex3(distA);
        }
    }
}

// ---- shifting

// 0x00D89CA0
// Moves every endpoint by the shift in integer units (x87: scale * shift truncated to 64 bits, even), clamping at the
// ends; an endpoint reaching an end updates its node (pairs may appear; the ended pairs are dropped). The effective
// shift is the integer shift back in world units.
void hkp3AxisSweep::shiftAllObjects(const hkVector4f& shiftDistance, hkVector4f& effectiveShiftDistanceOut, hkpBpPairArray& newCollisionPairs)
{
    alignas(16) float shift[4];
    _mm_store_ps(shift, shiftDistance.m_quad);
    long long intShift[3];
    for (int i = 0; i < 3; i++)
    {
        const double scale = m_scale.m_quad.m128_f32[i];
        intShift[i] = (long long)(scale * (double)shift[i]) & ~1ll; // _ftol2 (x87, 53-bit precision), made even
        shift[i] = (float)((double)intShift[i] / scale);
    }
    effectiveShiftDistanceOut.m_quad = _mm_load_ps(shift);
    hkpBpPairArray deletedPairs;
    for (int axis = 0; axis < 3; axis++)
    {
        const int delta = (int)intShift[axis];
        hkArray<hkpBpEndPoint, hkContainerHeapAllocator>& endPoints = m_axis[axis].m_endPoints;
        int i, end, step;
        if (delta < 0)
        {
            i = 1;
            end = endPoints.m_size - 1;
            step = 1;
        }
        else
        {
            i = endPoints.m_size - 2;
            end = 0;
            step = -1;
        }
        for (; i != end; i += step)
        {
            hkpBpEndPoint* ep = &endPoints.m_data[i];
            const unsigned int value = ep->m_value;
            if (value - 2 > 0xFFF9)
            {
                continue; // the sentinels' values
            }
            hkpBpNode& node = m_nodes.m_data[ep->m_nodeIndex];
            const int maxBit = value & 1;
            int newValue = ((delta + (int)value) & ~1) | maxBit;
            if (newValue < 0)
            {
                newValue = maxBit;
            }
            else if (newValue >= 0xFFFC)
            {
                newValue = maxBit | 0xFFFC;
            }
            ep->m_value = (unsigned short)newValue;
            if (newValue == 0 || newValue == 0xFFFD)
            {
                alignas(16) hkAabbUint32 aabb;
                aabb.m_min[0] = m_axis[0].m_endPoints.m_data[node.min_x].m_value;
                aabb.m_min[1] = m_axis[1].m_endPoints.m_data[node.min_y].m_value;
                aabb.m_min[2] = m_axis[2].m_endPoints.m_data[node.min_z].m_value;
                aabb.m_max[0] = m_axis[0].m_endPoints.m_data[node.max_x].m_value;
                aabb.m_max[1] = m_axis[1].m_endPoints.m_data[node.max_y].m_value;
                aabb.m_max[2] = m_axis[2].m_endPoints.m_data[node.max_z].m_value;
                updateAabb(node.m_handle, aabb, newCollisionPairs, deletedPairs);
            }
        }
    }
}

// 0x00D84AC0
// Shifts the objects the other way and moves the offsets. Note the high offsets become the low ones plus one integer
// unit (unlike set32BitOffsetAndScale's world range / AABB_MAX_FVALUE).
void hkp3AxisSweep::shiftBroadPhase(const hkVector4f& shiftDistance, hkVector4f& effectiveShiftDistanceOut, hkpBpPairArray& newCollisionPairs)
{
    hkVector4f negShift;
    negShift.setNeg<4>(shiftDistance);
    hkVector4f effective;
    shiftAllObjects(negShift, effective, newCollisionPairs);
    m_offsetLow.add(effective);
    hkVector4f unit;
    unit.setReciprocal<HK_ACC_23_BIT, HK_DIV_IGNORE>(m_scale);
    unit.setComponent<3>(hkSimdFloat32::getConstant<HK_QUADREAL_1>());
    m_offsetHigh.setAdd(unit, m_offsetLow);
    m_offsetLow32bit.add(effective);
    unit.setReciprocal<HK_ACC_23_BIT, HK_DIV_IGNORE>(m_scale32bit);
    unit.setComponent<3>(hkSimdFloat32::getConstant<HK_QUADREAL_1>());
    m_offsetHigh32bit.setAdd(unit, m_offsetLow32bit);
    effectiveShiftDistanceOut.setNeg<4>(effective);
}

// explicit instances (the original has them out of line)
template int hkp3AxisSweep::hkpBpAxis::insertTail<0>(hkpBpNode*, hkpBpEndPoint*, int);
template int hkp3AxisSweep::hkpBpAxis::insertTail<1>(hkpBpNode*, hkpBpEndPoint*, int);
template int hkp3AxisSweep::hkpBpAxis::insertTail<2>(hkpBpNode*, hkpBpEndPoint*, int);
template void hkp3AxisSweep::hkpBpAxis::mergeRest<0>(hkpBpNode*, int, hkpBpEndPoint*, int);
template void hkp3AxisSweep::hkpBpAxis::mergeRest<1>(hkpBpNode*, int, hkpBpEndPoint*, int);
template void hkp3AxisSweep::hkpBpAxis::mergeRest<2>(hkpBpNode*, int, hkpBpEndPoint*, int);
template void hkp3AxisSweep::hkpBpAxis::removeBatch<0>(hkpBpNode*, const hkArrayBase<int>&);
template void hkp3AxisSweep::hkpBpAxis::removeBatch<1>(hkpBpNode*, const hkArrayBase<int>&);
template void hkp3AxisSweep::hkpBpAxis::removeBatch<2>(hkpBpNode*, const hkArrayBase<int>&);

// 0x03A287DC, constructed by `dynamic initializer for 'hkOptionalComponent_hkp3AxisSweep'' 0x01094D60 (:4783):
// hkProductFeatures::initialize sets hkpBroadPhase::s_createSweepAndPruneBroadPhaseFunction through it.
HK_OPTIONAL_COMPONENT_DEFINE(hkp3AxisSweep, hkpBroadPhase::s_createSweepAndPruneBroadPhaseFunction, hk3AxisSweep16CreateBroadPhase);
