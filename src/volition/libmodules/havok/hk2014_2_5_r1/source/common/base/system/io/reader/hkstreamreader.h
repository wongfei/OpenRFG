// Original: volition/libmodules/havok/hk2014_2_5_r1/source/common/base/system/io/reader/hkstreamreader.h
//
// Havok 2014.2.5: full declaration from rfg.pdb (stub_impl.md S4, serializer); the members rfg.pdb places in the
// header made inline by hand (nobuild).
#pragma once

#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/memory/memoryclasses/hkmemoryclasses.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/object/hkreferencedobject.h"

class hkBool;
struct hkResult;
class hkSeekableStreamReader;

// size 0x8
class hkStreamReader : public hkReferencedObject
{
public:
    HK_DECLARE_CLASS_ALLOCATOR(); // delete(void* p, unsigned int nbytes) 0x00CFE0B0
    hkStreamReader() {} // 0x00CFE000
    virtual ~hkStreamReader() {} // vtbl+0x00 0x00CFE020
    virtual hkBool isOk() const = 0; // vtbl+0x10
    virtual int read(void*, int) = 0; // vtbl+0x14
    virtual int skip(int nbytes); // vtbl+0x18 0x00D17580
    virtual int peek(void* buf, int nbytes); // vtbl+0x1C 0x00D175E0
    virtual hkSeekableStreamReader* isSeekTellSupported() { return nullptr; } // vtbl+0x20 0x00CFE010

};

// size 0x8
class hkSeekableStreamReader : public hkStreamReader
{
public:
    enum SeekWhence
    {
        STREAM_SET = 0,
        STREAM_CUR = 1,
        STREAM_END = 2,
    };

    HK_DECLARE_CLASS_ALLOCATOR(); // new(unsigned int nbytes) 0x00D167C0, delete(void* p, unsigned int nbytes) 0x00D0CB10
    virtual ~hkSeekableStreamReader() {} // vtbl+0x00 0x00D0CB00
    virtual hkSeekableStreamReader* isSeekTellSupported() { return this; } // vtbl+0x20 0x00D0CAF0
    virtual hkResult seek(int, SeekWhence) = 0; // vtbl+0x24
    hkSeekableStreamReader() {} // 0x00D0CAE0
    virtual int tell() const = 0; // vtbl+0x28

};
