// Original: volition/libmodules/havok/hk2014_2_5_r1/source/common/base/system/io/writer/hkstreamwriter.h
//
// Havok 2014.2.5: full declaration from rfg.pdb (stub_impl.md S4, serializer); the members rfg.pdb places in the
// header made inline by hand (nobuild).
#pragma once

#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/memory/memoryclasses/hkmemoryclasses.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/object/hkreferencedobject.h"

class hkBool;
struct hkResult;

// size 0x8
class hkStreamWriter : public hkReferencedObject
{
public:
    enum SeekWhence
    {
        STREAM_SET = 0,
        STREAM_CUR = 1,
        STREAM_END = 2,
    };

    HK_DECLARE_CLASS_ALLOCATOR(); // new(unsigned int nbytes) 0x00D169F0, delete(void* p, unsigned int nbytes) 0x00CE8860
    hkStreamWriter() {} // 0x00CE7BE0
    virtual ~hkStreamWriter() {} // vtbl+0x00 0x00CE7C00
    virtual hkBool isOk() const = 0; // vtbl+0x10
    virtual int write(const void*, int) = 0; // vtbl+0x14
    virtual void flush() {} // vtbl+0x18 0x00CE7BF0
    virtual hkBool seekTellSupported() const; // vtbl+0x1C 0x00CFED90
    virtual hkResult seek(int offset, SeekWhence whence); // vtbl+0x20 0x00CFEDA0
    virtual int tell() const; // vtbl+0x24 0x00CFED80
    virtual hkBool isBuffered() const; // vtbl+0x28 0x00CFEDB0

};
