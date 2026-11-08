// Original: volition/libmodules/havok/hk2014_2_5_r1/source/common/base/system/io/writer/buffered/hkbufferedstreamwriter.h
//
// Havok 2014.2.5: full declaration from rfg.pdb (stub_impl.md S4, serializer).
#pragma once

#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/memory/memoryclasses/hkmemoryclasses.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/system/io/writer/hkstreamwriter.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/types/hkbasetypes.h"

class hkClass;

// size 0x1C
class hkBufferedStreamWriter : public hkStreamWriter
{
public:
    HK_DECLARE_CLASS_ALLOCATOR(); // new(unsigned int, void* p) 0x00CE7940, new(unsigned int nbytes) 0x00CE8910, delete(void* p, unsigned int nbytes) 0x00CFECF0
    hkBufferedStreamWriter(void* mem, int memSize, hkBool memoryIsString); // 0x00CFEB90
    hkBufferedStreamWriter(hkStreamWriter* s, int bufSize); // 0x00CFEBF0
    virtual ~hkBufferedStreamWriter(); // vtbl+0x00 0x00CFEC50
    virtual const hkClass* getClassType() const; // vtbl+0x08 0x00CFECA0
    virtual hkBool isOk() const; // vtbl+0x10 0x00CFEA40
    virtual int write(const void* mem, int memSize); // vtbl+0x14 0x00CFE9A0
    virtual void flush(); // vtbl+0x18 0x00CFEA20
    virtual hkBool seekTellSupported() const; // vtbl+0x1C 0x00CFEA80
    virtual hkResult seek(int relOffset, hkStreamWriter::SeekWhence whence); // vtbl+0x20 0x00CFEAC0
    virtual int tell() const; // vtbl+0x24 0x00CFEB50
    virtual hkBool isBuffered() const; // vtbl+0x28 0x00CFEB80
    static const hkClass& staticClass();
public:
    int flushBuffer(); // 0x00CFE950

    hkStreamWriter* m_stream; // 0x08
    char* m_buf; // 0x0C
    int m_bufSize; // 0x10
    int m_bufCapacity; // 0x14
    hkBool m_ownBuffer; // 0x18
};
