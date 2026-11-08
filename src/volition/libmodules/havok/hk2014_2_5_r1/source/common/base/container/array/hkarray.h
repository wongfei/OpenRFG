// Original: volition/libmodules/havok/hk2014_2_5_r1/source/common/base/container/array/hkarray.h
//
// Hand-written (havok_impl.md HK1, from the PDB instantiations and Havok 2014.1 hkArray.h/.inl): the array templates
// (pointer, size, capacity with flags; the inplace array adds its initial storage; the small array keeps 16-bit size
// and capacity). Storage comes from an allocator: hkArray's ALLOCATOR tag gives it (hkContainerHeapAllocator: the
// thread's Havok heap), hkArrayBase takes it as an argument (the _x functions). Growth goes through hkArrayUtil
// (hkarrayutil.cpp): a full array doubles its capacity (at least the new size); hkSmallArray grows through
// hkSmallArrayUtil (hksmallarray.cpp) on the heap. Elements are copied by assignment; elements are not constructed
// or destroyed by the storage functions (the PDB instances in scope hold PODs).
#pragma once

#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/container/hkcontainerallocators.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/types/hkbasetypes.h"

class hkFinishLoadedObjectFlag;
class hkMemoryAllocator;

namespace hkMemUtil
{
void memMove(void* dst, const void* src, int nbytes); // 0x00CF9210
void memCpyBackwards(void* dst, const void* src, int nbytes); // 0x00CF9120
}

// size 0x1, namespace-like struct of the array growth helpers (Havok 2014.1 hkArrayUtil.h)
struct hkArrayUtil
{
    // array: the data pointer at +0, the size at +4, the capacity and flags at +8.
    static hkResult _reserve(hkMemoryAllocator& alloc, void* array, int numElem, int sizeElem); // 0x00CE8E00 (hkarrayutil.cpp:17)
    static void _reserveMore(hkMemoryAllocator& alloc, void* array, int sizeElem); // 0x00CE8EA0 (hkarrayutil.cpp:55)
    static void _reduce(hkMemoryAllocator& alloc, void* array, int sizeElem, char* inplaceMem, int requestedCapacity); // 0x00CE8F30 (hkarrayutil.cpp:84)
};

// size 0xC
template<typename T>
class hkArrayBase
{
public:
    enum
    {
        CAPACITY_MASK = 0x3FFFFFFF,
        FLAG_MASK = -1073741824,
        DONT_DEALLOCATE_FLAG = -2147483647 - 1, // -2147483648
        ALLOCATED_FROM_SPU = 0x40000000,
        FORCE_SIGNED = -1,
    };

    int getSize() const { return m_size; }
    int getCapacity() const { return m_capacityAndFlags & CAPACITY_MASK; }
    bool isEmpty() const { return m_size == 0; }
    T& operator[](int i) { return m_data[i]; }
    const T& operator[](int i) const { return m_data[i]; }
    T* begin() { return m_data; }
    const T* begin() const { return m_data; }
    T* end() { return m_data + m_size; }
    const T* end() const { return m_data + m_size; }
    T& back() { return m_data[m_size - 1]; }
    void clear() { m_size = 0; }
    void popBack(int n = 1) { m_size -= n; }
    void setSizeUnchecked(int n) { m_size = n; }
    // storage the array does not own (2014.1 hkArray.inl; stub_impl.md S4 SE)
    void setDataUserFree(T* ptr, int size, int capacity)
    {
        m_data = ptr;
        m_size = size;
        m_capacityAndFlags = capacity | DONT_DEALLOCATE_FLAG;
    }
    void pushBackUnchecked(const T& t) { m_data[m_size++] = t; }
    int indexOf(const T& t, int start = 0) const
    {
        for (int i = start; i < m_size; i++)
        {
            if (m_data[i] == t)
            {
                return i;
            }
        }
        return -1;
    }
    // Order is not kept: the last element moves into the hole.
    void removeAt(int index) { m_data[index] = m_data[--m_size]; }
    void removeAtAndCopy(int index)
    {
        --m_size;
        for (int i = index; i < m_size; i++)
        {
            m_data[i] = m_data[i + 1];
        }
    }

    // ---- storage on an explicit allocator (hkarray.inl)
    // grows to at least twice the capacity (hkarray.inl)
    hkResult _reserve(hkMemoryAllocator& alloc, int n)
    {
        const int capacity = getCapacity();
        if (capacity < n)
        {
            const int cap2 = 2 * capacity;
            const int newSize = (n < cap2) ? cap2 : n;
            return hkArrayUtil::_reserve(alloc, this, newSize, sizeof(T));
        }
        return hkResult(HK_SUCCESS);
    }
    // the copy of a POD array (hkarray.inl): new storage of the exact size the allocator gives when it does not fit
    void _copyFromArray(hkMemoryAllocator& alloc, const hkArrayBase& a)
    {
        if (getCapacity() < a.m_size)
        {
            if ((m_capacityAndFlags & DONT_DEALLOCATE_FLAG) == 0)
            {
                alloc.bufFree(m_data, getCapacity() * (int)sizeof(T));
            }
            int numBytes = a.m_size * (int)sizeof(T);
            m_data = static_cast<T*>(alloc.bufAlloc(numBytes));
            m_capacityAndFlags = numBytes / (int)sizeof(T);
        }
        m_size = a.m_size;
        for (int i = 0; i < m_size; i++)
        {
            m_data[i] = a.m_data[i];
        }
    }
    T& _expandOne(hkMemoryAllocator& alloc)
    {
        if (m_size == getCapacity())
        {
            hkArrayUtil::_reserveMore(alloc, this, sizeof(T));
        }
        return m_data[m_size++];
    }
    // n uninitialised elements at the end (hkarray.inl)
    T* _expandBy(hkMemoryAllocator& alloc, int n)
    {
        const int oldSize = m_size;
        _reserve(alloc, oldSize + n);
        m_size = oldSize + n;
        return m_data + oldSize;
    }
    void _pushBack(hkMemoryAllocator& alloc, const T& t)
    {
        if (m_size == getCapacity())
        {
            hkArrayUtil::_reserveMore(alloc, this, sizeof(T));
        }
        m_data[m_size++] = t;
    }
    void _setSize(hkMemoryAllocator& alloc, int n)
    {
        _reserve(alloc, n);
        m_size = n;
    }
    // the new elements are copies of fill
    void _setSize(hkMemoryAllocator& alloc, int n, const T& fill)
    {
        _reserve(alloc, n);
        for (int i = m_size; i < n; i++)
        {
            m_data[i] = fill;
        }
        m_size = n;
    }
    // exactly n when it grows (no doubling)
    hkResult _reserveExactly(hkMemoryAllocator& alloc, int n)
    {
        if (getCapacity() < n)
        {
            return hkArrayUtil::_reserve(alloc, this, n, sizeof(T));
        }
        return hkResult(HK_SUCCESS);
    }
    void _clearAndDeallocate(hkMemoryAllocator& alloc)
    {
        m_size = 0;
        if ((m_capacityAndFlags & DONT_DEALLOCATE_FLAG) == 0)
        {
            alloc.bufFree(m_data, getCapacity() * (int)sizeof(T));
        }
        m_data = nullptr;
        m_capacityAndFlags = DONT_DEALLOCATE_FLAG;
    }
    // <char> 0x00D15BC0, <hkpEntity *> 0x00DEB3E0 (hkarray.inl:427)
    void _append(hkMemoryAllocator& alloc, const T* a, int numtoinsert)
    {
        const int newSize = m_size + numtoinsert;
        const int capacity = getCapacity();
        if (capacity < newSize)
        {
            _reserve(alloc, capacity * 2 > newSize ? capacity * 2 : newSize);
        }
        T* d = m_data + m_size;
        for (int i = 0; i < numtoinsert; i++)
        {
            d[i] = a[i];
        }
        m_size = newSize;
    }
    // <hkpAgentNnSector *> 0x00DDCF00 (hkarray.inl:403)
    // Replaces numdel elements at index by numtoinsert elements of p.
    void _spliceInto(hkMemoryAllocator& alloc, int index, int numdel, const T* p, int numtoinsert)
    {
        const int numToMove = m_size - index - numdel;
        const int newSize = m_size - numdel + numtoinsert;
        const int capacity = getCapacity();
        if (capacity < newSize)
        {
            hkArrayUtil::_reserve(alloc, this, capacity * 2 > newSize ? capacity * 2 : newSize, sizeof(T));
        }
        hkMemUtil::memMove(m_data + index + numtoinsert, m_data + index + numdel, numToMove * (int)sizeof(T));
        T* d = m_data + index;
        for (int i = 0; i < numtoinsert; i++)
        {
            d[i] = p[i];
        }
        m_size = newSize;
    }
    // <hkpAgentNnSector *> 0x00DDD0C0 (hkarray.inl:421)
    void _insertAt(hkMemoryAllocator& alloc, int index, const T* p, int numtoinsert)
    {
        _spliceInto(alloc, index, 0, p, numtoinsert);
    }
    // <hkpEntity *> 0x00E00B30 (hkarray.inl:457)
    void _insertAt(hkMemoryAllocator& alloc, int i, const T& t)
    {
        _insertAt(alloc, i, &t, 1);
    }

public:
    hkArrayBase() : m_data(nullptr), m_size(0), m_capacityAndFlags(DONT_DEALLOCATE_FLAG) {}
    // Loaded in place: the members are already set.
    hkArrayBase(hkFinishLoadedObjectFlag) {}

public:
    T* m_data; // 0x00
    int m_size; // 0x04
    int m_capacityAndFlags; // 0x08
};

// size 0xC
template<typename T, typename ALLOCATOR = hkContainerHeapAllocator> // default as in Havok (hkArray.h)
class hkArray : public hkArrayBase<T>
{
public:
    hkArray() {}
    hkArray(hkFinishLoadedObjectFlag f) : hkArrayBase<T>(f) {}
    // n elements (hkArray.inl): the storage comes straight from the allocator's bufAlloc, whose rounded size gives
    // the capacity; none for n = 0. The elements are not constructed (the instances in scope hold PODs).
    explicit hkArray(int n)
    {
        if (n != 0)
        {
            int numBytes = n * (int)sizeof(T);
            this->m_data = static_cast<T*>(ALLOCATOR().get(this).bufAlloc(numBytes));
            const int capacity = numBytes / (int)sizeof(T);
            this->m_capacityAndFlags = capacity != 0 ? capacity : hkArrayBase<T>::DONT_DEALLOCATE_FLAG;
        }
        this->m_size = n;
    }
    ~hkArray()
    {
        clearAndDeallocate();
    }

    void reserve(int n) { this->_reserve(ALLOCATOR().get(this), n); }
    T& expandOne() { return this->_expandOne(ALLOCATOR().get(this)); }
    T* expandBy(int n) { return this->_expandBy(ALLOCATOR().get(this), n); }
    void pushBack(const T& t) { this->_pushBack(ALLOCATOR().get(this), t); }
    void setSize(int n) { this->_setSize(ALLOCATOR().get(this), n); }
    void setSize(int n, const T& fill) { this->_setSize(ALLOCATOR().get(this), n, fill); }
    void reserveExactly(int n) { this->_reserveExactly(ALLOCATOR().get(this), n); }
    void clearAndDeallocate() { this->_clearAndDeallocate(ALLOCATOR().get(this)); }
    void append(const T* a, int numtoinsert) { this->_append(ALLOCATOR().get(this), a, numtoinsert); }
    void spliceInto(int index, int numdel, const T* p, int numtoinsert) { this->_spliceInto(ALLOCATOR().get(this), index, numdel, p, numtoinsert); }
    void insertAt(int index, const T* p, int numtoinsert) { this->_insertAt(ALLOCATOR().get(this), index, p, numtoinsert); }
    void insertAt(int i, const T& t) { this->_insertAt(ALLOCATOR().get(this), i, t); }

    // <hkpEntity *> 0x00E0A930 (hkarray.inl)
    void swap(hkArray& a)
    {
        T* d = this->m_data;
        this->m_data = a.m_data;
        a.m_data = d;
        int s = this->m_size;
        this->m_size = a.m_size;
        a.m_size = s;
        int c = this->m_capacityAndFlags;
        this->m_capacityAndFlags = a.m_capacityAndFlags;
        a.m_capacityAndFlags = c;
    }

    hkArray& operator=(const hkArrayBase<T>& a)
    {
        this->_copyFromArray(ALLOCATOR().get(this), a);
        return *this;
    }
    hkArray& operator=(const hkArray& a)
    {
        this->_copyFromArray(ALLOCATOR().get(this), a);
        return *this;
    }

private:
    hkArray(const hkArray&);
};

// size 0xC + N * sizeof(T)
template<typename T, unsigned int N, typename ALLOCATOR>
class hkInplaceArray : public hkArray<T, ALLOCATOR>
{
public:
    hkInplaceArray()
    {
        this->m_data = m_storage;
        this->m_capacityAndFlags = N | hkArrayBase<T>::DONT_DEALLOCATE_FLAG;
    }

    T m_storage[N]; // 0x0C (aligned for T)
};

// size 0x1, namespace-like struct of the small array growth helpers (always on the thread's heap)
struct hkSmallArrayUtil
{
    // array: the data pointer at +0, the 16-bit size at +4, the 16-bit capacity and flags at +6.
    static void _reserve(void* array, int numElem, int sizeElem); // 0x00FBE890 (hksmallarray.cpp:22)
    static void _reserveMore(void* array, int sizeElem); // 0x00FBE920 (hksmallarray.cpp:45)
    // New helper (inline hkDeallocateChunk in Havok): frees a block of the thread's heap.
    static void _free(void* data, int numBytes);
};

// size 0x8
// <hkpAction*> 0x00DDE440 (default constructor)
template<typename T>
class hkSmallArray
{
public:
    enum
    {
        CAPACITY_MASK = 16383,
        FLAG_MASK = 49152,
        DONT_DEALLOCATE_FLAG = 32768,
        LOCKED_FLAG = 16384,
        FORCE_SIGNED = -1,
    };

    typedef T* iterator;
    typedef T const* const_iterator;

    // The inline members of Havok 2014.1 (hkSmallArray.inl).
    hkSmallArray() : m_data(nullptr), m_size(0), m_capacityAndFlags(DONT_DEALLOCATE_FLAG) {}
    ~hkSmallArray() { releaseMemory(); }
    void releaseMemory()
    {
        if ((m_capacityAndFlags & DONT_DEALLOCATE_FLAG) == 0)
        {
            hkSmallArrayUtil::_free(m_data, getCapacity() * (int)sizeof(T));
        }
    }
    void clearAndDeallocate()
    {
        releaseMemory();
        m_data = nullptr;
        m_size = 0;
        m_capacityAndFlags = (unsigned short)(DONT_DEALLOCATE_FLAG | (m_capacityAndFlags & LOCKED_FLAG));
    }
    void removeAt(int i)
    {
        m_size--;
        m_data[i] = m_data[m_size];
    }
    void removeAtAndCopy(int index)
    {
        m_size--;
        for (int i = index; i < m_size; ++i)
        {
            m_data[i] = m_data[i + 1];
        }
    }
    int indexOf(const T& t) const
    {
        for (int i = 0; i < m_size; ++i)
        {
            if (m_data[i] == t)
            {
                return i;
            }
        }
        return -1;
    }
    T& expandOne()
    {
        if (m_size == getCapacity())
        {
            hkSmallArrayUtil::_reserveMore(this, sizeof(T));
        }
        return m_data[m_size++];
    }
    void pushBack(const T& t)
    {
        if (m_size == getCapacity())
        {
            hkSmallArrayUtil::_reserveMore(this, sizeof(T));
        }
        m_data[m_size++] = t;
    }
    T& back() { return m_data[m_size - 1]; }

    int getSize() const { return m_size; }
    int getCapacity() const { return m_capacityAndFlags & CAPACITY_MASK; }
    bool isEmpty() const { return m_size == 0; }
    T& operator[](int i) { return m_data[i]; }
    const T& operator[](int i) const { return m_data[i]; }
    T* begin() { return m_data; }
    T* end() { return m_data + m_size; }

    // <hkConstraintInternal> 0x00E0F920 (hksmallarray.inl:241)
    void insertAt(int index, const T* p, int numtoinsert)
    {
        const int oldSize = m_size;
        const int newSize = oldSize + numtoinsert;
        const int capacity = getCapacity();
        if (capacity < newSize)
        {
            hkSmallArrayUtil::_reserve(this, capacity * 2 > newSize ? capacity * 2 : newSize, sizeof(T));
        }
        hkMemUtil::memCpyBackwards(m_data + index + numtoinsert, m_data + index, (oldSize - index) * (int)sizeof(T));
        hkMemUtil::memCpyBackwards(m_data + index, p, numtoinsert * (int)sizeof(T));
        m_size = (unsigned short)newSize;
    }
    // <hkConstraintInternal> 0x00E0FA20 (hksmallarray.inl:258)
    void insertAt(int i, const T& t)
    {
        insertAt(i, &t, 1);
    }

    T* m_data; // 0x00
    unsigned short m_size; // 0x04
    unsigned short m_capacityAndFlags; // 0x06
};
