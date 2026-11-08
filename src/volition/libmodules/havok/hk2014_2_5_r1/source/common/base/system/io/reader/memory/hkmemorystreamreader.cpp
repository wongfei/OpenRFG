// Original: volition/libmodules/havok/hk2014_2_5_r1/source/common/base/system/io/reader/memory/hkmemorystreamreader.cpp
//
// Havok 2014.2.5 (stub_impl.md S4, serializer SA): a seekable reader over a memory block, from rfg.exe. MEMORY_COPY
// reads a heap copy, MEMORY_TAKE owns the caller's (heap) block, MEMORY_INPLACE reads the caller's block in place;
// the first two are freed with the reader. A read or skip that gets nothing for a nonzero request sets end of file
// until the next seek.

#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/system/io/reader/memory/hkmemorystreamreader.h"

#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/container/string/hkstring.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/memory/router/hkmemoryrouter.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/memory/util/hkmemutil.h"

// 0x00FC39E0
int hkMemoryStreamReader::tell() const
{
    return m_bufCurrent;
}

// 0x00FC3A00
int hkMemoryStreamReader::read(void* buf, int nbytes)
{
    int n = m_bufSize - m_bufCurrent;
    if (nbytes < n)
    {
        n = nbytes;
    }
    hkString::memCpy(buf, m_buf + m_bufCurrent, n);
    m_bufCurrent += n;
    if (n == 0 && nbytes != 0)
    {
        m_hitEof = true;
    }
    return n;
}

// 0x00FC3A50
int hkMemoryStreamReader::skip(int nbytes)
{
    int n = m_bufSize - m_bufCurrent;
    if (nbytes < n)
    {
        n = nbytes;
    }
    m_bufCurrent += n;
    if (n == 0 && nbytes != 0)
    {
        m_hitEof = true;
    }
    return n;
}

// 0x00FC3A80
hkBool hkMemoryStreamReader::isOk() const
{
    return !m_hitEof;
}

// 0x00FC3A90: no end of file here
int hkMemoryStreamReader::peek(void* buf, int nbytes)
{
    const int left = m_bufSize - m_bufCurrent;
    if (left < nbytes)
    {
        nbytes = left;
    }
    hkMemUtil::memCpy(buf, m_buf + m_bufCurrent, nbytes);
    return nbytes;
}

// 0x00FC3AC0: past the end clamps to the end and fails; before the start goes to the start and fails (an unknown
// whence counts as before the start)
hkResult hkMemoryStreamReader::seek(int relOffset, hkSeekableStreamReader::SeekWhence whence)
{
    int absOffset = relOffset;
    if (whence != STREAM_SET)
    {
        if (whence == STREAM_CUR)
        {
            absOffset = m_bufCurrent + relOffset;
        }
        else if (whence == STREAM_END)
        {
            absOffset = m_bufSize - relOffset;
        }
        else
        {
            absOffset = -1;
        }
    }
    hkResult result = HK_SUCCESS;
    if (absOffset < 0)
    {
        m_bufCurrent = 0;
        result = HK_FAILURE;
    }
    else
    {
        if (absOffset > m_bufSize)
        {
            result = HK_FAILURE;
            absOffset = m_bufSize;
        }
        m_bufCurrent = absOffset;
    }
    m_hitEof = false;
    return result;
}

// 0x00FC3B30
hkMemoryStreamReader::hkMemoryStreamReader(const void* mem, int memSize, MemoryType mt)
{
    m_memType = mt;
    m_bufCurrent = 0;
    m_bufSize = memSize;
    m_hitEof = false;
    if (m_memType == MEMORY_COPY)
    {
        m_buf = (char*)hkMemoryRouter::easyAlloc(hkMemoryRouter::getInstance().heap(), memSize);
        hkString::memCpy(m_buf, mem, memSize);
    }
    else
    {
        m_buf = (char*)mem;
    }
}

// 0x00FC3BA0
hkMemoryStreamReader::~hkMemoryStreamReader()
{
    if (m_memType == MEMORY_COPY || m_memType == MEMORY_TAKE)
    {
        hkMemoryRouter::easyFree(hkMemoryRouter::getInstance().heap(), m_buf);
    }
}
