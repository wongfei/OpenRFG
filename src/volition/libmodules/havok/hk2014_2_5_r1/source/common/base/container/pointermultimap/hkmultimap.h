// Original: volition/libmodules/havok/hk2014_2_5_r1/source/common/base/container/pointermultimap/hkmultimap.h
//
// Hand-written (stub_impl.md S4, serializer; layout from the PDB instance <unsigned long, unsigned long>, code as the
// 2014.1 SDK's hkMultiMap): an open-addressing hash map that allows repeated keys. The template functions are in
// hkmultimap.cxx (included below).
#pragma once

#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/container/hkcontainerallocators.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/memory/memoryclasses/hkmemoryclasses.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/types/hkbasetypes.h"

// size 0x1
// Key operations: the hash multiplies the key without its low 4 bits by the golden ratio constant 0x9E3779B1.
template<typename KEY>
struct hkMultiMapOperations
{
    static unsigned int hash(KEY key, unsigned int mod) { return (unsigned int)((hkUlong(key) >> 4) * 0x9E3779B1u) & mod; }
    static void invalidate(KEY& key) { key = KEY(-1); }
    static hkBool32 isValid(KEY key) { return key != KEY(-1); }
    static hkBool32 equal(KEY key0, KEY key1) { return key0 == key1; }
};

// size 0xC
template<typename KEY, typename VAL = KEY, typename OPS = hkMultiMapOperations<KEY>, typename ALLOC = hkContainerHeapAllocator>
class hkMultiMap
{
public:
    typedef hkMultiMap<KEY, VAL, OPS, ALLOC> ThisType;
    HK_DECLARE_NONVIRTUAL_CLASS_ALLOCATOR(ThisType);
    // A slot index; getCapacity() (m_hashMod + 1) means not found.
    typedef class Dummy* Iterator;
#define HK_MULTI_MAP_INDEX_TO_ITERATOR(i) reinterpret_cast<Iterator>(hkUlong(i))
#define HK_MULTI_MAP_ITERATOR_TO_INDEX(it) static_cast<int>(reinterpret_cast<hkUlong>(it))

    enum
    {
        NUM_ELEMS_MASK = 0x7FFFFFFF,
        DONT_DEALLOCATE_FLAG = (int)0x80000000,
    };

    // size sizeof(KEY) + sizeof(VAL)
    struct Pair
    {
        KEY key;
        VAL val;
    };

    hkMultiMap(); // <unsigned long> 0x00FC2240
    hkMultiMap(int numElements); // <unsigned long> 0x00FC21E0
    hkMultiMap(void* ptr, int sizeInBytes); // <unsigned long> 0x00FC1340
    ~hkMultiMap(); // <unsigned long> 0x00FC1380

    int findNumEntries(KEY key, VAL val) const; // <unsigned long> 0x00FC13B0
    int findNumEntries(KEY key) const; // <unsigned long> 0x00FC1400
    int removeAll(KEY key); // <unsigned long> 0x00FC2280

    // <unsigned long> 0x00F52D90
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
        return HK_MULTI_MAP_INDEX_TO_ITERATOR(i);
    }

    KEY getKey(Iterator it) const { return m_elem[HK_MULTI_MAP_ITERATOR_TO_INDEX(it)].key; } // <unsigned long> 0x00FC0B00
    VAL getValue(Iterator it) const { return m_elem[HK_MULTI_MAP_ITERATOR_TO_INDEX(it)].val; } // <unsigned long> 0x00F52AB0
    void setValue(Iterator it, VAL val) { m_elem[HK_MULTI_MAP_ITERATOR_TO_INDEX(it)].val = val; } // <unsigned long> 0x00FC0B10

    // <unsigned long> 0x00F52DB0
    // The next entry of the run with the key; the probe wraps to slot 0 at the end of the table.
    Iterator getNext(Iterator it, KEY key) const
    {
        int i = HK_MULTI_MAP_ITERATOR_TO_INDEX(it);
        i++;
        for (;;)
        {
            for (; i <= m_hashMod; ++i)
            {
                if (!OPS::isValid(m_elem[i].key))
                {
                    return HK_MULTI_MAP_INDEX_TO_ITERATOR(m_hashMod + 1);
                }
                if (OPS::equal(m_elem[i].key, key))
                {
                    return HK_MULTI_MAP_INDEX_TO_ITERATOR(i);
                }
            }
            i = 0;
        }
    }

    // <unsigned long> 0x00F52DF0
    Iterator getNext(Iterator it) const
    {
        int i = HK_MULTI_MAP_ITERATOR_TO_INDEX(it);
        for (i += 1; i <= m_hashMod; ++i)
        {
            if (OPS::isValid(m_elem[i].key))
            {
                break;
            }
        }
        return HK_MULTI_MAP_INDEX_TO_ITERATOR(i);
    }

    hkBool isValid(Iterator it) const { return HK_MULTI_MAP_ITERATOR_TO_INDEX(it) <= m_hashMod; } // <unsigned long> 0x00F52AC0
    void insert(KEY key, VAL val); // <unsigned long> 0x00FC1440
    hkResult tryInsert(KEY key, VAL val); // <unsigned long> 0x00FC22E0
    Iterator findKey(KEY key) const; // <unsigned long> 0x00FC14B0
    Iterator findKeyWithValue(KEY key, VAL val) const; // <unsigned long> 0x00FC14F0
    Iterator findOrInsertKey(KEY key, VAL val); // <unsigned long> 0x00FC2370
    hkBool hasKey(KEY key) const { return isValid(findKey(key)); } // <unsigned long> 0x00FC20F0
    VAL getWithDefault(KEY key, VAL def) const; // <unsigned long> 0x00FC1530
    hkResult get(KEY key, VAL* out) const;
    hkResult remove(KEY key, VAL val);
    hkResult remove(KEY key);
    void remove(Iterator it);
    int getSize() const { return m_numElems & NUM_ELEMS_MASK; }
    int getCapacity() const { return m_hashMod + 1; }
    void clear();
    void swap(hkMultiMap& other);
    void reserve(int numElements);

protected:
    hkResult resizeTable(int newcap);

    Pair* m_elem; // 0x00
    int m_numElems; // 0x04: and DONT_DEALLOCATE_FLAG
    int m_hashMod; // 0x08: capacity - 1
};

#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/container/pointermultimap/hkmultimap.cxx"
