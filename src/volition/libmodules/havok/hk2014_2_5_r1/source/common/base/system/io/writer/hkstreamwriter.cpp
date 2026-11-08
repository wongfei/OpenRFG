// Original: volition/libmodules/havok/hk2014_2_5_r1/source/common/base/system/io/writer/hkstreamwriter.cpp
//
// Havok 2014.2.5 (stub_impl.md S4, serializer SA): the defaults of a stream writer (no seeking, not buffered), from
// rfg.exe.

#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/system/io/writer/hkstreamwriter.h"

#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/types/hkbasetypes.h"

// 0x00CFED80
int hkStreamWriter::tell() const
{
    return -1;
}

// 0x00CFED90
hkBool hkStreamWriter::seekTellSupported() const
{
    return false;
}

// 0x00CFEDA0
hkResult hkStreamWriter::seek(int offset, SeekWhence whence)
{
    return HK_FAILURE;
}

// 0x00CFEDB0
hkBool hkStreamWriter::isBuffered() const
{
    return false;
}
