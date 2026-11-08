// Original: volition/libmodules/havok/hk2014_2_5_r1/source/common/base/system/io/writer/offsetonly/hkoffsetonlystreamwriter.cpp
//
// Havok 2014.2.5 (stub_impl.md S4, serializer SA): a writer that only counts (the packfile writer's sizing pass), from
// rfg.exe: the offset and the furthest offset reached.

#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/system/io/writer/offsetonly/hkoffsetonlystreamwriter.h"

// 0x00FC2D50
int hkOffsetOnlyStreamWriter::write(const void*, int n)
{
    m_offset += n;
    m_eofOffset = m_offset > m_eofOffset ? m_offset : m_eofOffset;
    return n;
}

// 0x00FC2D70
int hkOffsetOnlyStreamWriter::tell() const
{
    return m_offset;
}

// 0x00FC2D80
hkOffsetOnlyStreamWriter::hkOffsetOnlyStreamWriter()
{
    m_offset = 0;
    m_eofOffset = 0;
}

// 0x00FC2DA0
hkBool hkOffsetOnlyStreamWriter::isOk() const
{
    return true;
}

// 0x00FC2DB0
hkBool hkOffsetOnlyStreamWriter::seekTellSupported() const
{
    return true;
}

// 0x00FC2DC0: always succeeds (an unknown whence keeps the offset)
hkResult hkOffsetOnlyStreamWriter::seek(int offset, hkStreamWriter::SeekWhence whence)
{
    if (whence == STREAM_SET)
    {
        m_offset = offset;
    }
    else if (whence == STREAM_CUR)
    {
        m_offset += offset;
    }
    else if (whence == STREAM_END)
    {
        m_offset = m_eofOffset - offset;
    }
    m_eofOffset = m_offset > m_eofOffset ? m_offset : m_eofOffset;
    return HK_SUCCESS;
}
