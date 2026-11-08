// Original: volition/libmodules/havok/hk2014_2_5_r1/source/common/base/container/pointermap/hkmap.h
//
// Hand-written (havok_impl.md HK1, from the PDB instances): the open-addressing hash map. The table holds
// m_hashMod + 1 pairs (a power of two); a free slot has an all-ones key (OPS::isValid), collisions probe linearly,
// and removal shifts the following entries back. The top bit of m_numElems (DONT_DEALLOCATE_FLAG) marks a table
// the map does not own (init over caller storage). The template functions are in hkmap.cxx (included below), as the
// instances <unsigned long, unsigned long> (hkDefaultError's disabled ids) and <unsigned __int64, unsigned __int64>
// show them.
#pragma once

#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/container/hkcontainerallocators.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/types/hkbasetypes.h"

// size 0x1
// Key operations of the integer maps: the hash multiplies the key's low 32 bits without their low 4 bits by the
// golden ratio constant 0x9E3779B1 (2654435761).
template<typename KEY>
struct hkMapOperations
{
    static unsigned int hash(KEY key, unsigned int mod) { return ((unsigned int)key >> 4) * 0x9E3779B1u & mod; }
    static void invalidate(KEY& key) { key = (KEY)-1; }
    static bool isValid(KEY key) { return key != (KEY)-1; }
    static bool equal(KEY key0, KEY key1) { return key0 == key1; }
};

// size 0xC
template<typename KEY, typename VAL, typename OPS = hkMapOperations<KEY>>
class hkMapBase
{
public:
    // size sizeof(KEY) + sizeof(VAL)
    struct Pair
    {
        KEY key;
        VAL val;
    };

    // An index into the table; getEnd() (m_hashMod + 1) means not found.
    typedef int Iterator;

    enum
    {
        DONT_DEALLOCATE_FLAG = -2147483647 - 1, // the top bit of m_numElems
        NUM_ELEMS_MASK = 0x7FFFFFFF,
    };

    hkMapBase() : m_elem(nullptr), m_numElems(0), m_hashMod(-1) {}
    hkMapBase(void* ptr, int sizeInBytes); // hkmap.cxx:36
    ~hkMapBase(); // hkmap.cxx:84

    void init(void* ptr, int sizeInBytes); // hkmap.cxx:42
    void clear(); // hkmap.cxx:59
    void clearAndDeallocate(hkMemoryAllocator& alloc); // hkmap.cxx:70
    int insert(hkMemoryAllocator& alloc, KEY key, VAL val); // hkmap.cxx:90, 1 when the key is new
    Iterator findOrInsertKey(hkMemoryAllocator& alloc, KEY key, VAL val); // hkmap.cxx
    Iterator findKey(KEY key) const; // hkmap.cxx:152
    VAL getWithDefault(KEY key, VAL def) const; // hkmap.cxx:215
    hkResult get(KEY key, VAL* out) const; // hkmap.cxx
    void remove(Iterator it); // hkmap.cxx:239
    void removeAtIterator(Iterator it) { remove(it); } // as hkPointerMap's
    hkResult remove(KEY key); // hkmap.cxx:307
    void reserve(hkMemoryAllocator& alloc, int numElements); // hkmap.cxx:319
    hkResult resizeTable(hkMemoryAllocator& alloc, int newcap); // hkmap.cxx:331
    static int getSizeInBytesFor(int numOfKeys); // hkmap.cxx:413

    int getSize() const { return m_numElems & NUM_ELEMS_MASK; }
    int getCapacity() const { return m_hashMod + 1; }
    Iterator getEnd() const { return m_hashMod + 1; }

    // Inline in Havok (hkMap.h): the first used slot, getEnd() when there is none.
    Iterator getIterator() const
    {
        int i;
        for (i = 0; i <= m_hashMod; ++i)
        {
            if (OPS::isValid(m_elem[i].key))
            {
                break;
            }
        }
        return i;
    }

    // Inline in Havok (hkMap.h): the next used slot after it.
    Iterator getNext(Iterator it) const
    {
        int i = it;
        for (i += 1; i <= m_hashMod; ++i)
        {
            if (OPS::isValid(m_elem[i].key))
            {
                break;
            }
        }
        return i;
    }

    bool isValid(Iterator it) const { return it <= m_hashMod; }
    KEY getKey(Iterator it) const { return m_elem[it].key; }
    VAL getValue(Iterator it) const { return m_elem[it].val; }
    void setValue(Iterator it, VAL val) { m_elem[it].val = val; }
    hkBool32 hasKey(KEY key) const { return isValid(findKey(key)); }

    Pair* m_elem; // 0x00
    int m_numElems; // 0x04
    int m_hashMod; // 0x08
};

// size 0xC
// <hkpPairCollisionFilter::PairFilterKey, uint64_t, PairFilterPointerMapOperations, hkContainerHeapAllocator>
template<typename KEY, typename VAL, typename OPS = hkMapOperations<KEY>, typename ALLOCATOR = hkContainerHeapAllocator>
class hkMap : public hkMapBase<KEY, VAL, OPS>
{
public:
    typedef typename hkMapBase<KEY, VAL, OPS>::Iterator Iterator;

    hkMap() {}
    ~hkMap() { hkMapBase<KEY, VAL, OPS>::clearAndDeallocate(ALLOCATOR().get(this)); }

    int insert(KEY key, VAL val) { return hkMapBase<KEY, VAL, OPS>::insert(ALLOCATOR().get(this), key, val); }
    Iterator findOrInsertKey(KEY key, VAL val) { return hkMapBase<KEY, VAL, OPS>::findOrInsertKey(ALLOCATOR().get(this), key, val); }
    void clearAndDeallocate() { hkMapBase<KEY, VAL, OPS>::clearAndDeallocate(ALLOCATOR().get(this)); }
    void reserve(int numElements) { hkMapBase<KEY, VAL, OPS>::reserve(ALLOCATOR().get(this), numElements); }

private:
    hkMap(const hkMap&);
    hkMap& operator=(const hkMap&);
};

#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/container/pointermap/hkmap.cxx"
