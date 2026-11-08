// Original: volition/libmodules/havok/hk2014_2_5_r1/source/common/base/algorithm/unionfind/hkunionfind.h
//
// Havok 2014.2.5: full declaration from rfg.pdb (havok_impl.md HK0).
#pragma once

#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/memory/memoryclasses/hkmemoryclasses.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/container/array/hkarray.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/container/localarray/hklocalbuffer.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/types/hkbasedefs.h"

class hkBool;
struct hkContainerHeapAllocator;
struct hkResult;

// size 0x14
class hkUnionFind
{
public:
    typedef hkFixedArray<int> IntArray;

    HK_DECLARE_NONVIRTUAL_CLASS_ALLOCATOR(hkUnionFind);
    hkUnionFind(hkFixedArray<int>& parents, int numnode); // 0x00FBF0B0
    ~hkUnionFind(); // 0x00E0A830
    void clear(); // 0x00FBEEF0
    void beginAddEdges(); // 0x00E1D480
    void addEdge(int i1, int i2); // 0x00FBEFA0
    void endAddEdges(); // 0x00E1D490
    hkResult assignGroups(hkArray<int,hkContainerHeapAllocator>& elementsPerGroup); // 0x00FBF110
    hkBool isOneGroup(); // 0x00E1DBE0
    hkResult sortByGroupId(const hkArray<int,hkContainerHeapAllocator>& elementsPerGroup, hkArray<int,hkContainerHeapAllocator>& orderedGroups) const; // 0x00FBF3F0
    void reindex(const hkFixedArray<int>& rindex, int numNewGroups, hkArray<int,hkContainerHeapAllocator>& elementsPerGroup); // 0x00FBF2D0
    int moveBiggestGroupToIndexZero(hkArray<int,hkContainerHeapAllocator>& elementsPerGroup); // 0x00FBF1C0
    int findRootOfNode(int i); // 0x00FBEF20
    void unionRoots(int r1, int r2); // 0x00FBEF60
    void merge(const hkUnionFind& uf); // 0x00FBF0D0
    int collapseTree(); // 0x00FBF050
public:
    int _findRootOfNode(int i); // 0x00FBF580
    void _unionRoots(int r1, int r2); // 0x00FBF5C0

public:
    hkFixedArray<int>& m_parents; // 0x00
    int m_numNodes; // 0x04
    unsigned int m_isCollapsed; // 0x08
    int m_numRoots; // 0x0C
    hkDebugOnlyMember<unsigned int> m_isAddingEdges; // 0x10
};
