// Original: volition/libmodules/havok/hk2014_2_5_r1/source/common/base/system/io/oarchive/hkoarchive.h
//
// Havok 2014.2.5: full declaration from rfg.pdb (stub_impl.md S4, serializer).
#pragma once

#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/memory/memoryclasses/hkmemoryclasses.h"
#include <stdint.h>
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/container/array/hkarray.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/object/hkreferencedobject.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/system/io/writer/hkstreamwriter.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/types/hkbasetypes.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/types/hkrefptr.h"

struct hkContainerHeapAllocator;

// size 0x10
class hkOArchive : public hkReferencedObject
{
public:
    HK_DECLARE_CLASS_ALLOCATOR(); // delete(void* p, unsigned int nbytes) 0x00D0F0E0
    hkOArchive(hkArray<char,hkContainerHeapAllocator>& arr, hkBool bs); // 0x00D0ED80
    hkOArchive(void* mem, int memSize, hkBool bs); // 0x00D0ED00
    hkOArchive(const char* filename, hkBool bs); // 0x00D0ECB0
    hkOArchive(hkStreamWriter* sw, hkBool bs); // 0x00D0EC70
    virtual ~hkOArchive(); // vtbl+0x00 0x00D0EDF0
    void write8(signed char c); // 0x00D0EA60
    void write8u(unsigned char u); // 0x00D0EA80
    void write16(short i); // 0x00D0EE20
    void write16u(unsigned short u); // 0x00D0EE40
    void write32(int i); // 0x00D0EE60
    void write32u(unsigned int u); // 0x00D0EE80
    void write64(int64_t i); // 0x00D0EEA0
    void write64u(uint64_t u); // 0x00D0EEC0
    void writeFloat32(float f); // 0x00D0EEE0
    void writeDouble64(double d); // 0x00D0EF00
    void writeArray8(const signed char* array, int nelem); // 0x00D0EF20
    void writeArray8u(const unsigned char* array, int nelem); // 0x00D0EF40
    void writeArray16(const short* array, int nelem); // 0x00D0EF60
    void writeArray16u(const unsigned short* array, int nelem); // 0x00D0EF80
    void writeArray32(const int* array, int nelem); // 0x00D0EFA0
    void writeArray32u(const unsigned int* array, int nelem); // 0x00D0EFC0
    void writeArray64(const int64_t* array, int nelem); // 0x00D0EFE0
    void writeArray64u(const uint64_t* array, int nelem); // 0x00D0F000
    void writeArrayFloat32(const double* array, int nelem); // 0x00D0F040
    void writeArrayFloat32(const float* array, int nelem); // 0x00D0F020
    void writeArrayDouble64(const double* array, int nelem); // 0x00D0F080
    void writeArrayGeneric(const void* ptr, int elemSize, int arraySize); // 0x00D0EAA0
    int writeRaw(const void* buf, int nbytes); // 0x00D0EC40
    void setByteSwap(hkBool on); // 0x00D0EA40
    hkBool getByteSwap() const; // 0x00D0EA50
    hkBool isOk() const; // 0x00D0EC50
    hkStreamWriter* getStreamWriter(); // 0x00D0F0A0
    void setStreamWriter(hkStreamWriter* newWriter); // 0x00D0F0B0

public:
    hkRefPtr<hkStreamWriter> m_writer; // 0x08
    hkBool m_byteSwap; // 0x0C
};
