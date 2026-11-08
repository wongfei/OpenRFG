// Original: volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/broadphase/3axissweep/hkp3axissweep.h
//
// Havok 2014.2.5: full declaration from rfg.pdb (havok_impl.md HK0). Hand-maintained since HK4 (nobuild in the hdrgen
// specs): the inline helpers of Havok 2014.1 (hkp3AxisSweep.h/.inl, SSE paths of the 16-bit broadphase) are added,
// and hkpBpMarker is declared before hkpBpNode (the generated header declared it inside the node).
#pragma once

#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/memory/memoryclasses/hkmemoryclasses.h"
#include <xmmintrin.h>
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/container/array/hkarray.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/container/hkcontainerallocators.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/math/vector/hksimdfloat32.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/math/vector/hkvector4f.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/math/vector/hkintvector.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/types/geometry/aabb/hkaabb.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/broadphase/hkpbroadphase.h"

class hkBool;
class hkpBroadPhaseCastCollector;
class hkpBroadPhaseHandle;
class hkpBroadPhaseHandlePair;
class hkpCollidable;

// size 0xF0
// The sweep and prune broadphase over 16-bit integer boxes. Node 0 is the world box (its endpoints are the sentinels
// at both ends of each axis: value 0 and 0xFFFD); markers are optional nodes that split the x axis into equal intervals
// and keep the objects overlapping them, so a query only scans from the closest marker. Nodes store the indices of
// their endpoints in the three axes; endpoints store their value (even for a minimum, odd for a maximum) and node.
class hkp3AxisSweep : public hkpBroadPhase
{
public:
    enum MarkerHandling
    {
        MARKERS_ADD_NEW_OVERLAPS = 0,
        MARKERS_IGNORE = 1,
        MARKERS_REMOVE_NEW_OVERLAPS = 2,
    };

    enum
    {
        AABB_MIN_VALUE = 0,
        AABB_MAX_VALUE = 65532,
        AABB_MAX_FVALUE = 65532,
        HK_BP_NUM_VALUE_BITS = 16,
    };

    enum hkpBpMarkerUse
    {
        HK_BP_NO_MARKER = 0,
        HK_BP_USE_MARKER = 1,
    };

    enum hkpBpQuerySingleType
    {
        HK_BP_REPORT_HANDLES = 0,
        HK_BP_REPORT_NODES = 1,
        HK_BP_COLLECTOR = 2,
    };

    typedef unsigned short BpInt;

    // size 0x10
    class hkpBpMarker
    {
    public:
        HK_DECLARE_NONVIRTUAL_CLASS_ALLOCATOR(hkpBpMarker); // new(unsigned int n, void* p) 0x00D8AA70, delete(void* p, unsigned int nbytes) 0x00D8B930
        hkpBpMarker() {} // 0x00D8F010
        ~hkpBpMarker() {} // 0x00D8F030

        unsigned short m_nodeIndex; // 0x00
        unsigned short m_value; // 0x02
        hkArray<unsigned short,hkContainerHeapAllocator> m_overlappingObjects; // 0x04
    };

    // size 0x10
    class hkpBpNode
    {
    public:
        HK_DECLARE_NONVIRTUAL_CLASS_ALLOCATOR(hkpBpNode);
        unsigned short& getMin(int index) { return index == 0 ? min_x : (index == 1 ? min_y : min_z); }
        unsigned short& getMax(int index) { return index == 0 ? max_x : (index == 1 ? max_y : max_z); }
        // The endpoint index of axis * 2 + max (s_memberOffsets).
        template<int axis> void setElem(int minmax, int value)
        {
            *(unsigned short*)((char*)&min_y + s_memberOffsets[axis * 2 + minmax]) = (unsigned short)value;
        }
        // A marker's handle is its byte offset in m_markers with the lowest bit set.
        bool isMarker() const { return ((hkUlong)m_handle & 1) != 0; } // 0x00D8AA80
        bool isNotMarker() const { return ((hkUlong)m_handle & 1) == 0; } // 0x00D8AA90
        hkpBpMarker& getMarker(hkpBpMarker* markers) const { return *(hkpBpMarker*)((char*)markers + ((hkUlong)m_handle & ~(hkUlong)1)); } // 0x00D8B960
        // Disjoint on the two axes: the SSE versions (2014.1 hkp3AxisSweep.inl) subtract the zero-extended values and
        // return the sign mask, which is nonzero exactly when one maximum is below the other node's minimum.
        unsigned int xyDisjoint(const hkpBpNode& other) const { return (max_y < other.min_y) | (max_x < other.min_x) | (other.max_y < min_y) | (other.max_x < min_x); } // 0x00D8C820
        unsigned int xzDisjoint(const hkpBpNode& other) const { return (max_x < other.min_x) | (max_z < other.min_z) | (other.max_x < min_x) | (other.max_z < min_z); } // 0x00D8C870
        unsigned int yzDisjoint(const hkpBpNode& other) const { return (max_y < other.min_y) | (max_z < other.min_z) | (other.max_y < min_y) | (other.max_z < min_z); } // 0x00D8C7E0

        static const int s_memberOffsets[]; // 0x016F6130
        unsigned short min_y; // 0x00
        unsigned short min_z; // 0x02
        unsigned short max_y; // 0x04
        unsigned short max_z; // 0x06
        unsigned short min_x; // 0x08
        unsigned short max_x; // 0x0A
        hkpBroadPhaseHandle* m_handle; // 0x0C
    };

    // size 0x4
    class hkpBpEndPoint
    {
    public:
        HK_DECLARE_NONVIRTUAL_CLASS_ALLOCATOR(hkpBpEndPoint);
        static unsigned int isMaxPoint(int value) { return value & 1; } // 0x00D8AAB0
        int isMaxPoint() const { return m_value & 1; } // 0x00D8AAA0
        // The multithreaded build's order: by value, then by node (2014.1 hkp3AxisSweep.h).
        hkBool operator<(const hkpBpEndPoint& b) const { return m_value < b.m_value || (m_value == b.m_value && m_nodeIndex < b.m_nodeIndex); } // 0x00D8AAC0

        unsigned short m_value; // 0x00
        unsigned short m_nodeIndex; // 0x02
    };

    // size 0xC
    class hkpBpAxis
    {
    public:
        HK_DECLARE_NONVIRTUAL_CLASS_ALLOCATOR(hkpBpAxis); // new(unsigned int n, void* p) 0x00D8AB00
        hkpBpAxis(hkpBpEndPoint* ep, int initialArraySize); // 0x00D8EFB0
        hkpBpAxis() {} // 0x00D8EF90
        void insert(hkpBpNode* nodes, int nodeIndex, unsigned short minPosition, unsigned short maxPosition, unsigned short& minInsertPositionOut, unsigned short& maxInsertPositionOut); // 0x00D84D90
        void remove(int minPosition, int maxPosition); // 0x00D845C0
        const hkpBpEndPoint* find(const hkpBpEndPoint* start, const hkpBpEndPoint* end, unsigned short value) const; // 0x00D84070
        template<int axis> int insertTail(hkpBpNode* nodes, hkpBpEndPoint* newNodes, int numNewEndPoints); // <0> 0x00D8CC50, <1> 0x00D8CD50, <2> 0x00D8CE50
        template<int axis> void mergeRest(hkpBpNode* nodes, int startOfTail, hkpBpEndPoint* newEndPoints, int numNewEndPoints); // <0> 0x00D8CF50, <1> 0x00D8D010, <2> 0x00D8D0D0
        template<int axis> void removeBatch(hkpBpNode* nodes, const hkArrayBase<int>& nodeRelocations); // <0> 0x00D8D190, <1> 0x00D8D230, <2> 0x00D8D2D0
        ~hkpBpAxis() {} // 0x00D8EFD0

        hkArray<hkpBpEndPoint,hkContainerHeapAllocator> m_endPoints; // 0x00
    };

    HK_DECLARE_CLASS_ALLOCATOR(); // new(unsigned int nbytes) 0x008162F0, delete(void* p, unsigned int nbytes) 0x00D8F8F0
    hkp3AxisSweep(const hkVector4f& worldMin, const hkVector4f& worldMax, int numMarkers); // 0x00D86A40
    virtual ~hkp3AxisSweep(); // vtbl+0x00 0x00D89EF0
    void checkConsistency() {} // 0x00D84050
    virtual hkpBroadPhase::BroadPhaseType getType() const; // vtbl+0x10 0x00D83FC0
    virtual void addObject(hkpBroadPhaseHandle* object, const hkAabb& aabbIn, hkArray<hkpBroadPhaseHandlePair,hkContainerHeapAllocator>& newPairs, bool border); // vtbl+0x18 0x00D86210
    virtual void addObject(hkpBroadPhaseHandle* object, const hkAabbUint32& aabbIn, hkArray<hkpBroadPhaseHandlePair,hkContainerHeapAllocator>& newPairs, bool border); // vtbl+0x1C 0x00D871C0
    virtual void addObjectBatch(const hkArrayBase<hkpBroadPhaseHandle*>& addObjectList, const hkArrayBase<hkAabb>& addAabbList, hkArray<hkpBroadPhaseHandlePair,hkContainerHeapAllocator>& newPairs); // vtbl+0x20 0x00D8A010
    virtual void removeObject(hkpBroadPhaseHandle* object, hkArray<hkpBroadPhaseHandlePair,hkContainerHeapAllocator>& delPairsOut); // vtbl+0x24 0x00D87420
    virtual void removeObjectBatch(const hkArrayBase<hkpBroadPhaseHandle*>& removeObjectList, hkArray<hkpBroadPhaseHandlePair,hkContainerHeapAllocator>& delPairsOut); // vtbl+0x28 0x00D87740
    virtual int getNumObjects() const; // vtbl+0x2C 0x00D842E0
    virtual void updateAabbs(hkpBroadPhaseHandle** objects, const hkAabb* aabbs, int numObjects, hkArray<hkpBroadPhaseHandlePair,hkContainerHeapAllocator>& addedPairsOut, hkArray<hkpBroadPhaseHandlePair,hkContainerHeapAllocator>& removedPairsOut); // vtbl+0x30 0x00D85AE0
    virtual void updateAabbsUint32(hkpBroadPhaseHandle** objects, const hkAabbUint32* aabbs, int numObjects, hkArray<hkpBroadPhaseHandlePair,hkContainerHeapAllocator>& addedPairsOut, hkArray<hkpBroadPhaseHandlePair,hkContainerHeapAllocator>& removedPairsOut); // vtbl+0x34 0x00D85BF0
    virtual void defragment(); // vtbl+0x38 0x00D885B0
    virtual void getAllAabbs(hkArray<hkAabb,hkContainerHeapAllocator>& allAabbs) const; // vtbl+0x40 0x00D84E70
    virtual void getAabb(const hkpBroadPhaseHandle* object, hkAabb& aabb) const; // vtbl+0x44 0x00D84F10
    virtual void getExtents(hkVector4f& worldMinOut, hkVector4f& worldMaxOut) const; // vtbl+0x48 0x00D84970
    virtual void querySingleAabb(const hkAabb& aabbIn, hkArray<hkpBroadPhaseHandlePair,hkContainerHeapAllocator>& pairs_out) const; // vtbl+0x4C 0x00D883B0
    virtual void reQuerySingleObject(const hkpBroadPhaseHandle* object, hkArray<hkpBroadPhaseHandlePair,hkContainerHeapAllocator>& pairs_out) const; // vtbl+0x50 0x00D86FF0
    virtual void querySingleAabbWithCollector(const hkAabb& aabbIn, hkpBroadPhaseCastCollector* collector) const; // vtbl+0x54 0x00D883D0
    virtual bool areAabbsOverlapping(const hkpBroadPhaseHandle* bhA, const hkpBroadPhaseHandle* bhB) const; // vtbl+0x58 0x00D848D0
    virtual void castRay(const hkpBroadPhase::hkpCastRayInput& rayInput, hkpBroadPhaseCastCollector* collectorBase, int collectorStriding) const; // vtbl+0x68 0x00D888E0
    virtual int getAabbCacheSize() const; // vtbl+0x6C 0x00D842F0
    virtual void calcAabbCache(const hkArrayBase<hkpCollidable*>& overlappingCollidables, char* AabbCacheOut) const; // vtbl+0x70 0x00D88490
    virtual void calcAabbCache(const hkAabb& aabb, char* AabbCacheOut) const; // vtbl+0x74 0x00D883F0
    virtual void castAabb(const hkpBroadPhase::hkpCastAabbInput& input, hkpBroadPhaseCastCollector& collector) const; // vtbl+0x78 0x00D89250
public:
    void calcAabbCacheInternal(const hkArrayBase<hkpBpNode const*>& overlaps, char* AabbCacheOut) const; // 0x00D867E0
public:
    virtual void shiftAllObjects(const hkVector4f& shiftDistance, hkVector4f& effectiveShiftDistanceOut, hkArray<hkpBroadPhaseHandlePair,hkContainerHeapAllocator>& newCollisionPairs); // vtbl+0x5C 0x00D89CA0
    virtual void shiftBroadPhase(const hkVector4f& shiftDistance, hkVector4f& effectiveShiftDistanceOut, hkArray<hkpBroadPhaseHandlePair,hkContainerHeapAllocator>& newCollisionPairs); // vtbl+0x60 0x00D84AC0
    const hkAabb& getOriginalAabb() const { return m_aabb; } // 0x00D840D0
    virtual void getOffsetLowHigh32bit(hkVector4f& offsetLow32bit, hkVector4f& offsetHigh32bit) const; // vtbl+0x64 0x00D8F930
    void collide1Axis(const hkpBpNode* pa, int numA, hkArray<hkpBroadPhaseHandlePair,hkContainerHeapAllocator>& pairsOut); // 0x00D85FB0
    void collide1Axis(const hkpBpNode* pa, int numA, const hkpBpNode* pb, int numB, MarkerHandling markerHandlingForNodesB, hkArray<hkpBroadPhaseHandlePair,hkContainerHeapAllocator>& pairsOut); // 0x00D85CC0
    static void __fastcall beginOverlapCheckMarker(hkpBpMarker* markers, hkpBpNode& a, int nodeIndexA, hkpBpNode& b, hkArray<hkpBroadPhaseHandlePair,hkContainerHeapAllocator>& newPairsOut); // 0x00D84C50
    static void __fastcall endOverlapCheckMarker(hkpBpMarker* markers, hkpBpNode& a, int nodeIndexA, hkpBpNode& b, hkArray<hkpBroadPhaseHandlePair,hkContainerHeapAllocator>& deletedPairsOut); // 0x00D84CF0
    static void __fastcall beginOverlap(hkpBpNode& a, hkpBpNode& b, hkArray<hkpBroadPhaseHandlePair,hkContainerHeapAllocator>& newPairsOut); // 0x00D84BB0
    static void __fastcall endOverlap(hkpBpNode& a, hkpBpNode& b, hkArray<hkpBroadPhaseHandlePair,hkContainerHeapAllocator>& deletedPairsOut); // 0x00D84C00
    static HK_FORCE_INLINE void _convertAabbToInt(const hkAabb& aabb, const hkVector4f& offsetLow, const hkVector4f& offsetHigh, const hkVector4f& scale, hkAabbUint32& aabbOut); // 0x00D8DA50
    HK_FORCE_INLINE void convertAabbToInt(const hkAabb& aabb, hkAabbUint32& aabbOut) const; // 0x00D8DAF0
    HK_FORCE_INLINE void convertVectorToInt(const hkVector4f& vec, unsigned int* intsOut) const; // 0x00D8DB90
    HK_FORCE_INLINE void convertAabbToBroadPhaseResolution(const hkAabbUint32& aabbIn, hkAabbUint32& aabbOut) const; // 0x00D8DBD0
    int getNumMarkers() const { return m_numMarkers; } // 0x00D8AB10
    virtual void set32BitOffsetAndScale(const hkVector4f& offsetLow, const hkVector4f& offsetHigh, const hkVector4f& scale); // vtbl+0x7C 0x00D84520
    void _fixDeterministicOrderAfterNodeIdWasDecreased(int nodeIndex); // 0x00D84360
    void updateAabb(hkpBroadPhaseHandle* object, const hkAabbUint32& aabb, hkArray<hkpBroadPhaseHandlePair,hkContainerHeapAllocator>& newPairsOut, hkArray<hkpBroadPhaseHandlePair,hkContainerHeapAllocator>& deletedPairsOut); // 0x00D84F30
public:
    void updateNodesAfterInsert(hkpBpNode* nodes, int numNodes, hkpBpNode& newNode); // 0x00D84620
    void updateNodesAfterDelete(hkpBpNode* nodes, int numNodes, hkpBpNode& oldNode); // 0x00D847C0
    void updateNodesAfterBatchTailInsert(hkpBpNode* nodes, int numNodes, int numNewEndPoints, int* offsets); // 0x00D84740
    void querySingleAabbAddObject(hkpBroadPhaseHandle* object, int newNodeIndex, const unsigned int* bitField, hkpBpNode& refNode, hkArray<hkpBroadPhaseHandlePair,hkContainerHeapAllocator>& pairs_out) const; // 0x00D860A0
    void querySingleAabbRemoveObject(hkpBroadPhaseHandle* object, int newNodeIndex, const unsigned int* bitField, hkpBpNode& refNode, hkArray<hkpBroadPhaseHandlePair,hkContainerHeapAllocator>& pairs_out) const; // 0x00D862F0
    void setBitsBasedOnXInterval(int numNodes, int x_value, const hkpBpNode& queryNode, unsigned short queryNodeIndex, unsigned int* bitField) const; // 0x00D84130
    void querySingleAabbSub(hkpBroadPhaseHandle* object, const unsigned int* bitField, hkpBpNode& refNode, hkpBpQuerySingleType type, hkArray<hkpBroadPhaseHandlePair,hkContainerHeapAllocator>* pairs_out, hkArrayBase<hkpBpNode const*>* nodesOut, hkpBroadPhaseCastCollector* collector) const; // 0x00D86460
    void _querySingleAabb(const hkAabb& aabbIn, hkpBpQuerySingleType type, hkArray<hkpBroadPhaseHandlePair,hkContainerHeapAllocator>* pairs_out, hkArrayBase<hkpBpNode const*>* nodesOut, hkpBroadPhaseCastCollector* collector) const; // 0x00D88050
    void getAabbFromNode(const hkpBpNode& node, hkAabb& aabb) const; // 0x00D849E0
public:
    virtual void checkDeterminism(); // vtbl+0x3C 0x00D840E0

    static __m128 s_MaxVal; // 0x016F6120
    hkAabb m_aabb; // 0x20
    hkVector4f m_offsetLow; // 0x40
    hkVector4f m_offsetHigh; // 0x50
    hkVector4f m_scale; // 0x60
    hkVector4f m_offsetLow32bit; // 0x70
    hkVector4f m_offsetHigh32bit; // 0x80
    hkVector4f m_scale32bit; // 0x90
    hkArray<hkpBpNode,hkContainerHeapAllocator> m_nodes; // 0xA0
    hkpBpAxis m_axis[3]; // 0xAC
    int m_numMarkers; // 0xD0
    int m_ld2NumMarkers; // 0xD4
    hkpBpMarker* m_markers; // 0xD8
    // A correction such that convertAabbToInt(x + m_intToFloatFloorCorrection) gives int(floor(x)) whatever the
    // rounding mode of the float to int conversion (2014.1 hkp3AxisSweep.h); measured by the constructor.
    hkSimdFloat32 m_intToFloatFloorCorrection; // 0xE0
};

// Out-of-line copies in rfg.exe that nothing calls (the callers inline them); here file statics of hkp3axissweep.cpp:
// template instance: void _updateAxis<0,1>(...) 0x00D8DDA0, <1,0> 0x00D8E170, <2,0> 0x00D8E520
// template instance: void _fixDeterministicAxisOrderAfterNodeIdWasDecreased<0>(...) 0x00D8BD50, <1> 0x00D8BE00, <2> 0x00D8BEB0
// template instance: void hkp3AxisSweep_appendPair<0>(...) 0x00D8D680, <1> 0x00D8D5A0
// template instance: void hkp3AxisSweep_scanList<0>(...) 0x00D8EA50, <1> 0x00D8E900
// void calcCurDist(...) 0x00D8B970, int getMinComponentIndex(...) 0x00D8DC30, int getMinComponent(...) 0x00D8DC70
hkpBroadPhase* hk3AxisSweep16CreateBroadPhase(const hkVector4f& worldMin, const hkVector4f& worldMax, int numMarkers); // 0x00D89EB0

// ---- inline conversions (2014.1 hkp3AxisSweep.inl, SIMD path)

// Minimum rounded down to even, maximum up to odd: an endpoint's lowest bit says whether it is a maximum.
HK_FORCE_INLINE void hkp3AxisSweep::_convertAabbToInt(const hkAabb& aabb, const hkVector4f& offsetLow, const hkVector4f& offsetHigh, const hkVector4f& scale, hkAabbUint32& aabbOut)
{
    hkVector4f clipMin; clipMin.setZero();
    hkVector4f clipMax; clipMax.m_quad = s_MaxVal;
    {
        hkVector4f x;
        x.setAdd(aabb.m_min, offsetLow);
        x.mul(scale);
        x.setMin(x, clipMax);
        x.setMax(x, clipMin);
        hkIntVector i;
        i.setConvertF32toS32(x); // signed conversion is ok because min = 0
        i.setShiftRight32<1>(i);
        i.setShiftLeft32<1>(i); // clear LSB
        aabbOut.m_min[0] = i.getU32<0>();
        aabbOut.m_min[1] = i.getU32<1>();
        aabbOut.m_min[2] = i.getU32<2>();
    }
    {
        hkVector4f x;
        x.setAdd(aabb.m_max, offsetHigh);
        x.mul(scale);
        x.setMin(x, clipMax);
        x.setMax(x, clipMin);
        hkIntVector i;
        i.setConvertF32toS32(x);
        i.setOr(i, hkIntVector::getConstant<HK_QUADINT_1>());
        aabbOut.m_max[0] = i.getU32<0>();
        aabbOut.m_max[1] = i.getU32<1>();
        aabbOut.m_max[2] = i.getU32<2>();
    }
}

HK_FORCE_INLINE void hkp3AxisSweep::convertAabbToInt(const hkAabb& aabb, hkAabbUint32& aabbOut) const
{
    _convertAabbToInt(aabb, m_offsetLow, m_offsetHigh, m_scale, aabbOut);
}

HK_FORCE_INLINE void hkp3AxisSweep::convertVectorToInt(const hkVector4f& vec, unsigned int* intsOut) const
{
    hkVector4f clipMin; clipMin.setZero();
    hkVector4f clipMax; clipMax.m_quad = s_MaxVal;
    hkVector4f x;
    x.setAdd(vec, m_offsetLow);
    x.mul(m_scale);
    x.setMin(x, clipMax);
    x.setMax(x, clipMin);
    hkIntVector i;
    i.setConvertF32toS32(x);
    intsOut[0] = i.getU32<0>();
    intsOut[1] = i.getU32<1>();
    intsOut[2] = i.getU32<2>();
    intsOut[3] = i.getU32<3>();
}

// The 32-bit box (the collision input's resolution) to the 16-bit one: >> 15, the maximum + 1 saturated (as 16 bits).
HK_FORCE_INLINE void hkp3AxisSweep::convertAabbToBroadPhaseResolution(const hkAabbUint32& aabbIn, hkAabbUint32& aabbOut) const
{
    const hkIntVector one = hkIntVector::getConstant<HK_QUADINT_1>();
    hkIntVector mins; mins.set((int)aabbIn.m_min[0], (int)aabbIn.m_min[1], (int)aabbIn.m_min[2], *(const int*)&aabbIn.m_expansionMin[0]);
    hkIntVector maxs; maxs.set((int)aabbIn.m_max[0], (int)aabbIn.m_max[1], (int)aabbIn.m_max[2], *(const int*)&aabbIn.m_expansionMax[0]);
    mins.setShiftRight32<15>(mins);
    maxs.setShiftRight32<15>(maxs);
    maxs.setAddSaturateU16(maxs, one); // note 'one' is setup S32
    mins.setShiftRight32<1>(mins);
    mins.setShiftLeft32<1>(mins); // clear LSB
    aabbOut.m_min[0] = mins.getU32<0>();
    aabbOut.m_min[1] = mins.getU32<1>();
    aabbOut.m_min[2] = mins.getU32<2>();
    maxs.setOr(maxs, one);
    aabbOut.m_max[0] = maxs.getU32<0>();
    aabbOut.m_max[1] = maxs.getU32<1>();
    aabbOut.m_max[2] = maxs.getU32<2>();
}
