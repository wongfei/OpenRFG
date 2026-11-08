// Original: volition/libmodules/havok/hk2014_2_5_r1/source/common/base/system/io/iarchive/hkiarchive.cpp
//
// Havok 2014.2.5 (stub_impl.md S4, serializer SA): binary input with optional byte swapping (2, 4 and 8 byte
// elements), from rfg.exe. Only the functions the serializer reaches are ported so far.

#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/system/io/iarchive/hkiarchive.h"

// 0x00FC2EB0: read in one go, then swapped in place (other sizes stay)
void hkIArchive::readArrayGeneric(void* array, int elemsize, int arraySize)
{
    m_streamReader->read(array, elemsize * arraySize);
    if (!m_byteSwap || (elemsize != 2 && elemsize != 4 && elemsize != 8))
    {
        return;
    }
    unsigned char* p = (unsigned char*)array;
    for (int i = 0; i < arraySize; i++, p += elemsize)
    {
        for (int a = 0, b = elemsize - 1; a < b; a++, b--)
        {
            const unsigned char t = p[a];
            p[a] = p[b];
            p[b] = t;
        }
    }
}

// 0x00FC2FA0
int hkIArchive::readRaw(void* buf, int nbytes)
{
    return m_streamReader->read(buf, nbytes);
}

// 0x00FC2FB0
hkIArchive::hkIArchive(hkStreamReader* sb, hkBool bs)
    : m_streamReader(sb), m_byteSwap(bs)
{
}

// 0x00FC30B0
hkIArchive::~hkIArchive()
{
    m_streamReader = nullptr;
}

// 0x00FC3120
hkBool hkIArchive::isOk() const
{
    return m_streamReader.val() != nullptr && m_streamReader->isOk();
}
