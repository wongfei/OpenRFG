// Original: volition/libmodules/havok/hk2014_2_5_r1/source/common/base/system/io/oarchive/hkoarchive.cpp
//
// Havok 2014.2.5 (stub_impl.md S4, serializer SA): binary output with optional byte swapping (2, 4 and 8 byte
// elements), from rfg.exe. Only the functions the serializer reaches are ported so far.

#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/system/io/oarchive/hkoarchive.h"

#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/container/string/hkstring.h"

// the swap buffer of writeArrayGeneric
static const int HK_OARCHIVE_SWAP_BUFFER = 0x200;

// the bytes of each of n elements of size elemSize reversed in place (other sizes stay)
static void hkOArchive_swapElements(unsigned char* p, int elemSize, int n)
{
    if (elemSize != 2 && elemSize != 4 && elemSize != 8)
    {
        return;
    }
    for (int i = 0; i < n; i++, p += elemSize)
    {
        for (int a = 0, b = elemSize - 1; a < b; a++, b--)
        {
            const unsigned char t = p[a];
            p[a] = p[b];
            p[b] = t;
        }
    }
}

// 0x00D0EA60
void hkOArchive::write8(signed char c)
{
    m_writer->write(&c, 1);
}

// 0x00D0EAA0: swapped through a 512-byte stack buffer, chunk by chunk (the last chunk is the remainder)
void hkOArchive::writeArrayGeneric(const void* ptr, int elemSize, int arraySize)
{
    if (!m_byteSwap)
    {
        m_writer->write(ptr, elemSize * arraySize);
        return;
    }
    unsigned char buf[HK_OARCHIVE_SWAP_BUFFER];
    const char* src = (const char*)ptr;
    int chunkBytes = HK_OARCHIVE_SWAP_BUFFER;
    int chunkElems = HK_OARCHIVE_SWAP_BUFFER / elemSize;
    int left = elemSize * arraySize;
    const int lastBytes = left % HK_OARCHIVE_SWAP_BUFFER;
    const int lastElems = lastBytes / elemSize;
    while (left > 0)
    {
        if (left < HK_OARCHIVE_SWAP_BUFFER)
        {
            chunkElems = lastElems;
            chunkBytes = lastBytes;
        }
        hkString::memCpy(buf, src, chunkBytes);
        hkOArchive_swapElements(buf, elemSize, chunkElems);
        m_writer->write(buf, chunkBytes);
        src += chunkBytes;
        left -= chunkBytes;
    }
}

// 0x00D0EC40
int hkOArchive::writeRaw(const void* buf, int nbytes)
{
    return m_writer->write(buf, nbytes);
}

// 0x00D0EC50
hkBool hkOArchive::isOk() const
{
    return m_writer->isOk();
}

// 0x00D0EC70
hkOArchive::hkOArchive(hkStreamWriter* sw, hkBool bs)
    : m_writer(sw), m_byteSwap(bs)
{
}

// 0x00D0EDF0
hkOArchive::~hkOArchive()
{
    m_writer = nullptr;
}

// 0x00D0EE20
void hkOArchive::write16(short i)
{
    writeArrayGeneric(&i, 2, 1);
}

// 0x00D0EE40
void hkOArchive::write16u(unsigned short u)
{
    writeArrayGeneric(&u, 2, 1);
}

// 0x00D0EE60
void hkOArchive::write32(int i)
{
    writeArrayGeneric(&i, 4, 1);
}

// 0x00D0EE80
void hkOArchive::write32u(unsigned int u)
{
    writeArrayGeneric(&u, 4, 1);
}

// 0x00D0F0A0
hkStreamWriter* hkOArchive::getStreamWriter()
{
    return m_writer;
}
