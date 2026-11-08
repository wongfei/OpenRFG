// Original: volition/libmodules/havok/hk2014_2_5_r1/source/common/base/system/io/istream/hkistream.h
//
// Havok 2014.2.5: full declaration from rfg.pdb (stub_impl.md S4, serializer).
#pragma once

#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/memory/memoryclasses/hkmemoryclasses.h"
#include <stdint.h>
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/object/hkreferencedobject.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/system/io/reader/hkstreamreader.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/types/hkrefptr.h"

class hkBool;
class hkMemoryTrack;
class hkStringBuf;
class hkStringPtr;

// size 0xC
class hkIstream : public hkReferencedObject
{
public:
    HK_DECLARE_CLASS_ALLOCATOR(); // delete(void* p, unsigned int nbytes) 0x00FC0540
    hkIstream(const hkMemoryTrack* track); // 0x00FBFCC0
    hkIstream(const void* mem, int memSize); // 0x00FBFC50
    hkIstream(const char* fname); // 0x00FBFC00
    hkIstream(hkStreamReader* sb); // 0x00FBFBD0
    virtual ~hkIstream(); // vtbl+0x00 0x00FBFD30
    hkBool isOk() const; // 0x00FBFD60
    hkIstream& operator>>(hkStringPtr& str); // 0x00FC0240
    hkIstream& operator>>(hkStringBuf& str); // 0x00FC0120
    hkIstream& operator>>(uint64_t& i); // 0x00FC0000
    hkIstream& operator>>(int64_t& i); // 0x00FBFFC0
    hkIstream& operator>>(double& d); // 0x00FBFFA0
    hkIstream& operator>>(float& f); // 0x00FBFF80
    hkIstream& operator>>(unsigned int& i); // 0x00FBFF40
    hkIstream& operator>>(int& i); // 0x00FBFF00
    hkIstream& operator>>(unsigned short& s); // 0x00FBFEC0
    hkIstream& operator>>(short& s); // 0x00FBFE80
    hkIstream& operator>>(unsigned char&);
    hkIstream& operator>>(signed char&);
    hkIstream& operator>>(char& c); // 0x00FBFBA0
    hkIstream& operator>>(hkBool& b); // 0x00FBFD90
    int getline(char* str, int maxlen, char delim); // 0x00FC0040
    // 0x01054B00 (hkistream.inl:20)
    hkIstream& get(char& c) { m_streamReader->read(&c, 1); return *this; }
    int read(void* buf, int nbytes); // 0x00FBFBC0
    hkStreamReader* getStreamReader() { return m_streamReader; } // 0x00F451A0 (hkistream.inl:25)
    void setStreamReader(hkStreamReader* newReader); // 0x00FC00F0

public:
    hkRefPtr<hkStreamReader> m_streamReader; // 0x08
};

// template instance: void readInteger<__int64>(hkStreamReader* reader, int64_t& tOut); // 0x00FC03C0
// template instance: void readInteger<unsigned __int64>(hkStreamReader* reader, uint64_t& tOut); // 0x00FC03F0
// template instance: void readInteger<short>(hkStreamReader* reader, short& tOut); // 0x00FC02E0
// template instance: void readInteger<unsigned int>(hkStreamReader* reader, unsigned int& tOut); // 0x00FC0390
// template instance: void readInteger<int>(hkStreamReader* reader, int& tOut); // 0x00FC0360
// template instance: void readInteger<unsigned short>(hkStreamReader* reader, unsigned short& tOut); // 0x00FC0320
