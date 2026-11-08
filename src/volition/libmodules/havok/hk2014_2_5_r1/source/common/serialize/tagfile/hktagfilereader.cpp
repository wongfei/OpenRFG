// Original: volition/libmodules/havok/hk2014_2_5_r1/source/common/serialize/tagfile/hktagfilereader.cpp
//
// Havok 2014.2.5 (stub_impl.md S4, serializer SD), from rfg.exe.

#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/serialize/tagfile/hktagfilereader.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/system/io/reader/hkstreamreader.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/serialize/tagfile/binary/hkbinarytagfilecommon.h"

// 0x00F6EBC0 (:14): the binary magic in the first 16 bytes, peeked
hkEnum<hkTagfileReader::FormatType,int> hkTagfileReader::detectFormat(hkStreamReader* stream)
{
    hkEnum<FormatType,int> format;
    hkUint32 buf[4];
    if (stream->peek(buf, sizeof(buf)) != sizeof(buf))
    {
        format = FORMAT_ERROR;
    }
    else if (hkBinaryTagfile::isBinaryMagic(buf[0], buf[1]))
    {
        format = FORMAT_BINARY;
    }
    else
    {
        format = FORMAT_UNKNOWN;
    }
    return format;
}
