// Original: volition/libmodules/havok/hk2014_2_5_r1/source/common/base/algorithm/unionfind/hkunionfind.cpp
//
// Havok 2014.2.5 (havok_impl.md HK1): union-find over caller storage (the island splitting of the world). A node's
// entry is its parent, or for a root minus the size of its set. Unions attach the higher root index to the lower;
// finds compress the path. assignGroups turns the roots into group numbers (in node order) and every node into its
// group number.

#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/algorithm/unionfind/hkunionfind.h"

#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/types/hkbasetypes.h"

// 0x00FBF0B0 (:54)
hkUnionFind::hkUnionFind(hkFixedArray<int>& parents, int numnode)
    : m_parents(parents)
{
    m_isAddingEdges.member = 0;
    m_numNodes = numnode;
    clear();
}

// 0x00E0A830 (.h:33)
hkUnionFind::~hkUnionFind()
{
}

// 0x00FBEEF0 (:63)
void hkUnionFind::clear()
{
    for (int i = 0; i < m_numNodes; i++)
    {
        m_parents[i] = -1;
    }
    m_numRoots = -1;
    m_isCollapsed = 1;
}

// 0x00FBF580 (:79)
int hkUnionFind::_findRootOfNode(int i)
{
    if (m_parents[i] >= 0)
    {
        int root = i;
        while (m_parents[root] >= 0)
        {
            root = m_parents[root];
        }
        while (m_parents[i] >= 0)
        {
            const int next = m_parents[i];
            m_parents[i] = root;
            i = next;
        }
    }
    return i;
}

// 0x00FBEF20 (:102)
int hkUnionFind::findRootOfNode(int i)
{
    return _findRootOfNode(i);
}

// 0x00FBF5C0 (:111)
void hkUnionFind::_unionRoots(int r1, int r2)
{
    if (r1 < r2)
    {
        m_parents[r1] += m_parents[r2];
        m_parents[r2] = r1;
        return;
    }
    m_parents[r2] += m_parents[r1];
    m_parents[r1] = r2;
}

// 0x00FBEF60 (:128)
void hkUnionFind::unionRoots(int r1, int r2)
{
    _unionRoots(r1, r2);
}

// 0x00FBF0D0 (:135)
void hkUnionFind::merge(const hkUnionFind& uf)
{
    m_isCollapsed = 0;
    for (int i = 0; i < m_numNodes; i++)
    {
        const int parent = uf.m_parents[i];
        if (parent >= 0)
        {
            addEdge(i, parent);
        }
    }
}

// 0x00FBEFA0 (:158)
void hkUnionFind::addEdge(int i1, int i2)
{
    const int r1 = _findRootOfNode(i1);
    const int r2 = _findRootOfNode(i2);
    if (r1 != r2)
    {
        _unionRoots(r1, r2);
    }
}

// 0x00E1D480 (.h:93)
void hkUnionFind::beginAddEdges()
{
    m_isCollapsed = 0;
}

// 0x00E1D490 (.h:99)
void hkUnionFind::endAddEdges()
{
}

// 0x00FBF050 (:175)
// Points every node straight at its root; returns the number of roots.
int hkUnionFind::collapseTree()
{
    if (m_isCollapsed != 0)
    {
        return m_numRoots;
    }
    int numRoots = m_numNodes;
    for (int i = 0; i < m_numNodes; i++)
    {
        if (m_parents[i] >= 0)
        {
            numRoots--;
            while (m_parents[m_parents[i]] >= 0)
            {
                m_parents[i] = m_parents[m_parents[i]];
            }
        }
    }
    m_isCollapsed = 1;
    m_numRoots = numRoots;
    return numRoots;
}

// 0x00E1DBE0 (.h:50)
hkBool hkUnionFind::isOneGroup()
{
    hkBool result;
    result.m_bool = m_parents[0] == -m_numNodes;
    return result;
}

// 0x00FBF110 (:207)
// Appends each group's size to elementsPerGroup; afterwards every node holds its group number.
hkResult hkUnionFind::assignGroups(hkArray<int,hkContainerHeapAllocator>& elementsPerGroup)
{
    const int numGroups = collapseTree();
    const int capacity = elementsPerGroup.getCapacity();
    if (capacity < numGroups)
    {
        if (hkArrayUtil::_reserve(hkContainerHeapAllocator::s_alloc, &elementsPerGroup, capacity * 2 > numGroups ? capacity * 2 : numGroups, sizeof(int)).m_enum != HK_SUCCESS)
        {
            return hkResult(HK_FAILURE);
        }
    }
    int group = 0;
    for (int i = 0; i < m_numNodes; i++)
    {
        const int parent = m_parents[i];
        if (parent < 0)
        {
            elementsPerGroup.m_data[elementsPerGroup.m_size++] = -parent;
            m_parents[i] = group++;
        }
        else
        {
            m_parents[i] = m_parents[parent]; // the root comes first, so it holds its group number already
        }
    }
    return hkResult(HK_SUCCESS);
}

// 0x00FBF1C0 (:241)
// Swaps the largest group (the first of equal sizes) with group 0; returns its old index.
int hkUnionFind::moveBiggestGroupToIndexZero(hkArray<int,hkContainerHeapAllocator>& elementsPerGroup)
{
    const int numGroups = elementsPerGroup.getSize();
    int biggest = 0;
    int biggestSize = elementsPerGroup[0];
    if (numGroups < 2)
    {
        return 0;
    }
    for (int i = 1; i < numGroups; i++)
    {
        if (biggestSize < elementsPerGroup[i])
        {
            biggestSize = elementsPerGroup[i];
            biggest = i;
        }
    }
    if (biggest == 0)
    {
        return 0;
    }
    // hkLocalArray<int> on the temp allocator: the group renumbering
    hkArray<int, hkContainerTempAllocator> remap;
    remap.reserve(numGroups);
    for (int i = 0; i < numGroups; i++)
    {
        remap.m_data[i] = i;
    }
    remap.m_data[0] = biggest;
    remap.m_data[biggest] = 0;
    const int t = elementsPerGroup[biggest];
    elementsPerGroup[biggest] = elementsPerGroup[0];
    elementsPerGroup[0] = t;
    for (int i = 0; i < m_numNodes; i++)
    {
        m_parents[i] = remap.m_data[m_parents[i]];
    }
    return biggest;
}

// 0x00FBF2D0 (:286)
// Renumbers the groups (rindex maps old to new numbers) and sums their sizes into numNewGroups groups.
void hkUnionFind::reindex(const hkFixedArray<int>& rindex, int numNewGroups, hkArray<int,hkContainerHeapAllocator>& elementsPerGroup)
{
    for (int i = 0; i < m_numNodes; i++)
    {
        m_parents[i] = rindex[m_parents[i]];
    }
    hkArray<int, hkContainerTempAllocator> sizes;
    sizes.reserve(numNewGroups);
    for (int i = 0; i < numNewGroups; i++)
    {
        sizes.m_data[i] = 0;
    }
    for (int i = 0; i < elementsPerGroup.getSize(); i++)
    {
        sizes.m_data[rindex[i]] += elementsPerGroup[i];
    }
    const int capacity = elementsPerGroup.getCapacity();
    if (capacity < numNewGroups)
    {
        hkArrayUtil::_reserve(hkContainerHeapAllocator::s_alloc, &elementsPerGroup, capacity * 2 > numNewGroups ? capacity * 2 : numNewGroups, sizeof(int));
    }
    elementsPerGroup.m_size = numNewGroups;
    for (int i = 0; i < numNewGroups; i++)
    {
        elementsPerGroup.m_data[i] = sizes.m_data[i];
    }
}

// 0x00FBF3F0 (:312)
// orderedGroups lists the nodes group by group (counting sort; node order kept within a group).
hkResult hkUnionFind::sortByGroupId(const hkArray<int,hkContainerHeapAllocator>& elementsPerGroup, hkArray<int,hkContainerHeapAllocator>& orderedGroups) const
{
    const int numNodes = m_numNodes;
    const int capacity = orderedGroups.getCapacity();
    if (capacity < numNodes)
    {
        if (hkArrayUtil::_reserve(hkContainerHeapAllocator::s_alloc, &orderedGroups, capacity * 2 > numNodes ? capacity * 2 : numNodes, sizeof(int)).m_enum != HK_SUCCESS)
        {
            return hkResult(HK_FAILURE);
        }
    }
    orderedGroups.m_size = numNodes;
    const int numGroups = elementsPerGroup.getSize();
    if (numGroups < 1)
    {
        return hkResult(HK_SUCCESS);
    }
    hkArray<int, hkContainerTempAllocator> starts;
    if (hkArrayUtil::_reserve(hkContainerTempAllocator::s_alloc, &starts, numGroups, sizeof(int)).m_enum != HK_SUCCESS)
    {
        return hkResult(HK_FAILURE);
    }
    int start = 0;
    for (int i = 0; i < numGroups; i++)
    {
        starts.m_data[i] = start;
        start += elementsPerGroup[i];
    }
    for (int i = 0; i < m_numNodes; i++)
    {
        orderedGroups.m_data[starts.m_data[m_parents[i]]++] = i;
    }
    return hkResult(HK_SUCCESS);
}
