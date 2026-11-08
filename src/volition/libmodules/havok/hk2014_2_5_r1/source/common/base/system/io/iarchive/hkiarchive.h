// Original: volition/libmodules/havok/hk2014_2_5_r1/source/common/base/system/io/iarchive/hkiarchive.h
//
// Havok 2014.2.5: full declaration from rfg.pdb (stub_impl.md S4, serializer).
#pragma once

#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/memory/memoryclasses/hkmemoryclasses.h"
#include <stdint.h>
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/object/hkreferencedobject.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/system/io/reader/hkstreamreader.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/types/hkbasetypes.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/types/hkrefptr.h"

// size 0x10
class hkIArchive : public hkReferencedObject
{
public:
    HK_DECLARE_CLASS_ALLOCATOR(); // delete(void* p, unsigned int nbytes) 0x00FC31A0
    hkIArchive(const void* mem, int memSize, hkBool byteswap); // 0x00FC3040
    hkIArchive(const char* filename, hkBool bs); // 0x00FC2FF0
    hkIArchive(hkStreamReader* sb, hkBool bs); // 0x00FC2FB0
    virtual ~hkIArchive(); // vtbl+0x00 0x00FC30B0
    signed char read8();
    unsigned char read8u(); // 0x00F6AB70
    short read16();
    unsigned short read16u(); // 0x00F6AB90
    int read32(); // 0x00F6ABB0
    unsigned int read32u();
    int64_t read64();
    uint64_t read64u();
    float readFloat32(); // 0x00F6ABD0
    double readDouble64();
    void readArray8(signed char*, int);
    void readArray8u(unsigned char* buf, int nelem); // 0x00F6ABF0
    void readArray16(short*, int);
    void readArray16u(unsigned short*, int);
    void readArray32(int*, int);
    void readArray32u(unsigned int*, int);
    void readArray64(int64_t*, int);
    void readArray64u(uint64_t*, int);
    void readArrayFloat32(double* buf, int nelem); // 0x00FC30E0
    void readArrayFloat32(float* buf, int nelem); // 0x00F6AC10
    void readArrayDouble32u(double*, int);
    void readArrayGeneric(void* array, int elemsize, int arraySize); // 0x00FC2EB0
    int readRaw(void* buf, int nbytes); // 0x00FC2FA0
    void setByteSwap(hkBool);
    hkBool getByteSwap() const;
    hkBool isOk() const; // 0x00FC3120
    hkStreamReader* getStreamReader(); // 0x00FC3160
    void setStreamReader(hkStreamReader* newBuf); // 0x00FC3170

public:
    hkRefPtr<hkStreamReader> m_streamReader; // 0x08
    hkBool m_byteSwap; // 0x0C
};
