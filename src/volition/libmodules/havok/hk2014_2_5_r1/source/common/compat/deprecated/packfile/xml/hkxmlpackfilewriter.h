// Original: volition/libmodules/havok/hk2014_2_5_r1/source/common/compat/deprecated/packfile/xml/hkxmlpackfilewriter.h
//
// Havok 2014.2.5: full declaration from rfg.pdb (stub_impl.md S4, serializer).
#pragma once

#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/memory/memoryclasses/hkmemoryclasses.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/serialize/packfile/hkpackfilewriter.h"

struct hkResult;
class hkStreamWriter;

// size 0xE0
class hkXmlPackfileWriter : public hkPackfileWriter
{
public:
    HK_DECLARE_CLASS_ALLOCATOR(); // delete(void* p, unsigned int nbytes) 0x00FB0C70
    hkXmlPackfileWriter(const hkPackfileWriter::Options& options); // 0x00FB0630
    virtual ~hkXmlPackfileWriter(); // vtbl+0x00 0x00FA1560
    virtual hkResult save(hkStreamWriter* stream, const hkPackfileWriter::Options& options); // vtbl+0x20 0x00FB08E0

    static const unsigned int CURRENT_FILE_VERSION; // 0x014BC080
};
