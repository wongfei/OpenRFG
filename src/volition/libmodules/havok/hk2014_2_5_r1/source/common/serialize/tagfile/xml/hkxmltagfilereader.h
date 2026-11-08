// Original: volition/libmodules/havok/hk2014_2_5_r1/source/common/serialize/tagfile/xml/hkxmltagfilereader.h
//
// Havok 2014.2.5: full declaration from rfg.pdb (stub_impl.md S4, serializer); constructor and destructor inline
// (nobuild).
#pragma once

#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/memory/memoryclasses/hkmemoryclasses.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/serialize/tagfile/xml/hkxmltagfilecommon.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/serialize/tagfile/hktagfilereader.h"

class hkDataObject;
class hkDataWorld;
struct hkResult;
class hkStreamReader;

// size 0x8
class hkXmlTagfileReader : public hkTagfileReader
{
public:
    HK_DECLARE_CLASS_ALLOCATOR(); // delete(void* p, unsigned int nbytes) 0x00F44BD0
    // the destructor (vtbl+0x00 0x00F42AD0) is compiler-generated
    hkResult readHeader(hkStreamReader* stream, hkXmlTagfile::Header& out); // 0x00F55D30
    hkXmlTagfileReader() {} // 0x00F44BC0
    virtual hkDataObject load(hkStreamReader* stream, hkDataWorld& world); // vtbl+0x10 0x00F55C70
};
