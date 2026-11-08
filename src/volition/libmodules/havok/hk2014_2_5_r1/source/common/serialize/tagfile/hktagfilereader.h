// Original: volition/libmodules/havok/hk2014_2_5_r1/source/common/serialize/tagfile/hktagfilereader.h
// Implementation: volition/libmodules/havok/hk2014_2_5_r1/source/common/serialize/tagfile/hktagfilereader.cpp
//
// Havok 2014.2.5 (stub_impl.md S4, serializer SD): the tagfile reader interface (2014.1 hkTagfileReader.h).
#pragma once

#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/memory/memoryclasses/hkmemoryclasses.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/object/hkreferencedobject.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/types/hkbasetypes.h"

class hkDataObject;
class hkDataWorld;
class hkStreamReader;

// size 0x8
class hkTagfileReader : public hkReferencedObject
{
public:
    enum FormatType
    {
        FORMAT_ERROR = 0,
        FORMAT_UNKNOWN = 1,
        FORMAT_BINARY = 2,
    };

    HK_DECLARE_CLASS_ALLOCATOR(); // delete(void* p, unsigned int nbytes) 0x00F44B70
    hkTagfileReader() {} // 0x00F42AE0 (implicit, emitted in hkSerializeUtil.obj)
    virtual ~hkTagfileReader() {} // vtbl+0x00 0x00F42AC0 (implicit)
    static hkEnum<enum FormatType,int> detectFormat(hkStreamReader* stream); // 0x00F6EBC0
    virtual hkDataObject load(hkStreamReader*, hkDataWorld&) = 0; // vtbl+0x10
};
