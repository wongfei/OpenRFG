// Original: volition/libmodules/havok/hk2014_2_5_r1/source/common/serialize/tagfile/binary/hkbinarytagfilereader.h
// Implementation: volition/libmodules/havok/hk2014_2_5_r1/source/common/serialize/tagfile/binary/hkbinarytagfilereader.cpp
//
// Havok 2014.2.5 (stub_impl.md S4, serializer SD): reads a binary tagfile into a data world (2014.1
// hkBinaryTagfileReader.h).
#pragma once

#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/memory/memoryclasses/hkmemoryclasses.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/serialize/tagfile/binary/hkbinarytagfilecommon.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/serialize/tagfile/hktagfilereader.h"

class hkDataObject;
class hkDataWorld;
struct hkResult;
class hkStreamReader;

// size 0x8
class hkBinaryTagfileReader : public hkTagfileReader
{
public:
    HK_DECLARE_CLASS_ALLOCATOR(); // delete(void* p, unsigned int nbytes) 0x00F6B1D0 (.h:18)
    hkBinaryTagfileReader(); // 0x00F6B140
    // the destructor (vtbl+0x00 0x00F44BB0, emitted in hkSerializeUtil.obj) is compiler-generated
    hkResult readHeader(hkStreamReader* stream, hkBinaryTagfile::Header& out); // 0x00F6B150
    virtual hkDataObject load(hkStreamReader* stream, hkDataWorld& cont); // vtbl+0x10 0x00F6B190
};
