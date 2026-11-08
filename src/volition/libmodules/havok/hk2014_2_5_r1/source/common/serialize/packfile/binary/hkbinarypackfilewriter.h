// Original: volition/libmodules/havok/hk2014_2_5_r1/source/common/serialize/packfile/binary/hkbinarypackfilewriter.h
// Implementation: volition/libmodules/havok/hk2014_2_5_r1/source/common/serialize/packfile/binary/hkbinarypackfilewriter.cpp
//
// Seam (Havok 2014.2.5, stub_impl.md S4 serializer SD): the binary packfile writer's constants the loaders read
// (2014.1 hkBinaryPackfileWriter.h). The writer itself comes with the packfile pass (SE).
#pragma once

#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/types/hkbasetypes.h"

class hkBinaryPackfileWriter
{
public:
    typedef const char* SectionTag;

    static const hkUint32 CURRENT_FILE_VERSION; // 0x0137364C: the version this SDK writes
    static SectionTag SECTION_TAG_CLASSNAMES; // 0x016FA92C: the section of the class names to finish
};
