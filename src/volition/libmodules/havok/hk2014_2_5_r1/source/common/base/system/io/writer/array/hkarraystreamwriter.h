// Original: volition/libmodules/havok/hk2014_2_5_r1/source/common/base/system/io/writer/array/hkarraystreamwriter.h
//
// Havok 2014.2.5: full declaration from rfg.pdb (stub_impl.md S4, serializer); the members rfg.pdb places in the
// header made inline by hand (nobuild).
#pragma once

#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/container/array/hkarray.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/container/hkcontainerallocators.h"

#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/memory/memoryclasses/hkmemoryclasses.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/container/array/hkarray.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/system/io/writer/hkstreamwriter.h"

class hkBool;
class hkClass;
struct hkContainerHeapAllocator;
class hkMemoryAllocator;
struct hkResult;

// size 0x18
// the writer's reflection class (generated with the reflection data, stub_impl.md S4 SB)
extern const hkClass hkArrayStreamWriterClass;

// Writes into a char array at an offset (seekable; seeking past the end zero-fills), keeping a 0 after the data.
class hkArrayStreamWriter : public hkStreamWriter
{
public:
    enum ArrayOwnership
    {
        ARRAY_TAKE = 0,
        ARRAY_BORROW = 1,
    };

    HK_DECLARE_CLASS_ALLOCATOR(); // new(unsigned int nbytes) 0x00CE88F0, delete(void* p, unsigned int nbytes) 0x00CE8C00
    // 0x00CE8AF0: room for a 0 after the data, written
    void nullTerminate()
    {
        m_arr->_reserve(m_allocator, m_arr->getSize() + 1);
        m_arr->begin()[m_arr->getSize()] = 0;
    }
    void* getData();
    int getDataSize();
    hkArrayStreamWriter(hkArrayBase<char>*, hkMemoryAllocator&, ArrayOwnership);
    // 0x00CE8B40: appends to the array's data
    hkArrayStreamWriter(hkArray<char,hkContainerHeapAllocator>* arr, ArrayOwnership o)
        : m_arr(arr), m_allocator(hkContainerHeapAllocator::s_alloc), m_offset(arr->getSize()), m_ownerShip(o)
    {
        nullTerminate();
    }
    virtual ~hkArrayStreamWriter(); // vtbl+0x00 0x00CE8D30
    virtual const hkClass* getClassType() const { return &hkArrayStreamWriterClass; } // vtbl+0x08 0x00CE8BE0
    virtual hkBool isOk() const { return true; } // vtbl+0x10 0x00CE8BB0
    virtual int write(const void* mem, int size); // vtbl+0x14 0x00CFF7B0
    virtual hkBool seekTellSupported() const { return true; } // vtbl+0x1C 0x00CE8BC0
    virtual hkResult seek(int offset, hkStreamWriter::SeekWhence whence); // vtbl+0x20 0x00CFF960
    virtual int tell() const { return m_offset; } // vtbl+0x24 0x00CE8BD0
    virtual void clear(); // vtbl+0x2C 0x00CFF750
    static const hkClass& staticClass();

public:
    hkArrayBase<char>* m_arr; // 0x08
    hkMemoryAllocator& m_allocator; // 0x0C
    int m_offset; // 0x10
    ArrayOwnership m_ownerShip; // 0x14
};
