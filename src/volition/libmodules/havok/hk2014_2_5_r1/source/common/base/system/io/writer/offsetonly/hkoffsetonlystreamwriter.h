// Original: volition/libmodules/havok/hk2014_2_5_r1/source/common/base/system/io/writer/offsetonly/hkoffsetonlystreamwriter.h
//
// Havok 2014.2.5: full declaration from rfg.pdb (stub_impl.md S4, serializer); the members rfg.pdb places in the
// header made inline by hand (nobuild).
#pragma once

#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/container/array/hkarray.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/container/hkcontainerallocators.h"

#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/memory/memoryclasses/hkmemoryclasses.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/system/io/writer/hkstreamwriter.h"

class hkBool;
struct hkResult;

// size 0x10
class hkOffsetOnlyStreamWriter : public hkStreamWriter
{
public:
    HK_DECLARE_CLASS_ALLOCATOR(); // delete(void* p, unsigned int nbytes) 0x00FC2E10
    hkOffsetOnlyStreamWriter(); // 0x00FC2D80
    virtual ~hkOffsetOnlyStreamWriter() {} // vtbl+0x00 0x00F6A310
    virtual hkBool isOk() const; // vtbl+0x10 0x00FC2DA0
    virtual int write(const void*, int n); // vtbl+0x14 0x00FC2D50
    virtual hkBool seekTellSupported() const; // vtbl+0x1C 0x00FC2DB0
    virtual hkResult seek(int offset, hkStreamWriter::SeekWhence whence); // vtbl+0x20 0x00FC2DC0
    virtual int tell() const; // vtbl+0x24 0x00FC2D70

public:
    int m_offset; // 0x08
    int m_eofOffset; // 0x0C
};
