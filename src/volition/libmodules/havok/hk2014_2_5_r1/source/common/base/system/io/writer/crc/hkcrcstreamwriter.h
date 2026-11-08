// Original: volition/libmodules/havok/hk2014_2_5_r1/source/common/base/system/io/writer/crc/hkcrcstreamwriter.h
//
// Hand-written (stub_impl.md S4, serializer SA; layout from the PDB instance <uint32, 0xEDB88320>, code from rfg.exe):
// a writer that only computes the reflected CRC of what goes through it, one byte at a time without a table (the
// polynomial is a template argument). The state is kept inverted: the start value and getCrc() complement it.
#pragma once

#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/system/io/writer/hkstreamwriter.h"

// size 0xC
template<typename T, T POLYNOMIAL>
class hkCrcStreamWriter : public hkStreamWriter
{
public:
    // <uint32,0xEDB88320> 0x00CF3540
    hkCrcStreamWriter(T startCrc = 0)
    {
        m_crc = ~startCrc;
    }

    // vtbl+0x00 <uint32,0xEDB88320> 0x00CF3620: implicit

    // vtbl+0x10 <uint32,0xEDB88320> 0x00CF3560
    virtual hkBool isOk() const { return true; }

    // vtbl+0x14 <uint32,0xEDB88320> 0x00CF36D0
    virtual int write(const void* ptr, int nbytes)
    {
        const unsigned char* p = (const unsigned char*)ptr;
        T crc = m_crc;
        for (int i = 0; i < nbytes; i++)
        {
            crc = (crc >> 8) ^ calcPermute((crc ^ p[i]) & 0xFF);
        }
        m_crc = crc;
        return nbytes;
    }

    // <uint32,0xEDB88320> 0x00CF3570
    T getCrc() const { return ~m_crc; }

    // <uint32,0xEDB88320> 0x00CF3580: the table entry of p, eight shift-and-xor steps
    static T calcPermute(T p)
    {
        for (int bit = 0; bit < 8; bit++)
        {
            p = (p & 1) ? (p >> 1) ^ POLYNOMIAL : p >> 1;
        }
        return p;
    }

protected:
    T m_crc; // 0x08
};

// The reflected CRC-32 polynomial (the instance in rfg.pdb).
typedef hkCrcStreamWriter<hkUint32, 0xedb88320> hkCrc32StreamWriter;
