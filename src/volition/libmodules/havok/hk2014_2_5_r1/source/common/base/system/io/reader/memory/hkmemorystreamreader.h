// Original: volition/libmodules/havok/hk2014_2_5_r1/source/common/base/system/io/reader/memory/hkmemorystreamreader.h
//
// Havok 2014.2.5: full declaration from rfg.pdb (stub_impl.md S4, serializer).
#pragma once

#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/memory/memoryclasses/hkmemoryclasses.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/system/io/reader/hkstreamreader.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/types/hkbasetypes.h"

// size 0x1C
class hkMemoryStreamReader : public hkSeekableStreamReader
{
public:
    enum MemoryType
    {
        MEMORY_COPY = 0,
        MEMORY_TAKE = 1,
        MEMORY_INPLACE = 2,
    };

    HK_DECLARE_CLASS_ALLOCATOR(); // new(unsigned int nbytes) 0x00FC0500, delete(void* p, unsigned int nbytes) 0x00FC3BE0
    hkMemoryStreamReader(const void* mem, int memSize, MemoryType mt); // 0x00FC3B30
    virtual ~hkMemoryStreamReader(); // vtbl+0x00 0x00FC3BA0
    virtual hkBool isOk() const; // vtbl+0x10 0x00FC3A80
    virtual int read(void* buf, int nbytes); // vtbl+0x14 0x00FC3A00
    virtual int skip(int nbytes); // vtbl+0x18 0x00FC3A50
    virtual int peek(void* buf, int nbytes); // vtbl+0x1C 0x00FC3A90
    virtual hkResult seek(int relOffset, hkSeekableStreamReader::SeekWhence whence); // vtbl+0x24 0x00FC3AC0
    virtual int tell() const; // vtbl+0x28 0x00FC39E0

public:
    char* m_buf; // 0x08
    int m_bufCurrent; // 0x0C
    int m_bufSize; // 0x10
    MemoryType m_memType; // 0x14
    hkBool m_hitEof; // 0x18
};
