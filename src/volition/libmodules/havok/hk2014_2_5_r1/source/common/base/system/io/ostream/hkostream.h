// Original: volition/libmodules/havok/hk2014_2_5_r1/source/common/base/system/io/ostream/hkostream.h
//
// Havok 2014.2.5: full declaration from rfg.pdb (stub_impl.md S4, serializer).
#pragma once

#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/memory/memoryclasses/hkmemoryclasses.h"
#include <stdint.h>
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/container/array/hkarray.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/object/hkreferencedobject.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/system/io/writer/hkstreamwriter.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/types/hkrefptr.h"

class hkBool;
class hkClass;
struct hkContainerHeapAllocator;
class hkFinishLoadedObjectFlag;
class hkMatrix3d;
class hkMatrix3f;
class hkMemoryTrack;
class hkQuaterniond;
class hkQuaternionf;
class hkSimdDouble64;
class hkSimdFloat32;
class hkStringBuf;
class hkStringPtr;
class hkTransformd;
class hkTransformf;
class hkVector4d;
class hkVector4f;

// size 0xC
class hkOstream : public hkReferencedObject
{
public:
    // size 0x4
    struct CustomFormater
    {
        HK_DECLARE_NONVIRTUAL_CLASS_ALLOCATOR(CustomFormater);
        virtual ~CustomFormater(); // vtbl+0x00
        virtual void writeTo(hkOstream&) const = 0; // vtbl+0x04

        // 0x00 vfptr
    };

    HK_DECLARE_CLASS_ALLOCATOR(); // delete(void* p, unsigned int nbytes) 0x00CE8CA0
    hkOstream(hkFinishLoadedObjectFlag);
    hkOstream(hkMemoryTrack* buf); // 0x00CE7E00
    hkOstream(hkArray<char,hkContainerHeapAllocator>& buf); // 0x00CE7D90
    hkOstream(void* mem, int memSize, hkBool isString); // 0x00CE7D20
    hkOstream(const char* filename); // 0x00CE7CD0
    hkOstream(hkStreamWriter* sw); // 0x00CE7CA0
    virtual ~hkOstream(); // vtbl+0x00 0x00CE7E70
    hkBool isOk() const; // 0x00CE7EA0
    hkOstream& operator<<(const CustomFormater&);
    hkOstream& operator<<(const hkStringBuf& str); // 0x00CE8330
    hkOstream& operator<<(const hkStringPtr& str); // 0x00CE82E0
    hkOstream& operator<<(uint64_t i); // 0x00CE8240
    hkOstream& operator<<(int64_t i); // 0x00CE81F0
    hkOstream& operator<<(const hkTransformd& t); // 0x00CE85E0
    hkOstream& operator<<(const hkMatrix3d& m); // 0x00CE8560
    hkOstream& operator<<(const hkQuaterniond& q); // 0x00CE84B0
    hkOstream& operator<<(const hkVector4d& v); // 0x00CE8400
    hkOstream& operator<<(const hkSimdDouble64& f); // 0x00CE8120
    hkOstream& operator<<(const hkTransformf& t); // 0x00CE85C0
    hkOstream& operator<<(const hkMatrix3f& m); // 0x00CE8500
    hkOstream& operator<<(const hkQuaternionf& q); // 0x00CE8450
    hkOstream& operator<<(const hkVector4f& v); // 0x00CE83A0
    hkOstream& operator<<(const hkSimdFloat32& f); // 0x00CE80C0
    hkOstream& operator<<(double);
    hkOstream& operator<<(float f); // 0x00CE8190
    hkOstream& operator<<(unsigned int u); // 0x00CE8070
    hkOstream& operator<<(int i); // 0x00CE8020
    hkOstream& operator<<(unsigned short s); // 0x00CE7FD0
    hkOstream& operator<<(short s); // 0x00CE7F80
    hkOstream& operator<<(const unsigned char*);
    hkOstream& operator<<(const signed char*);
    hkOstream& operator<<(const char* s); // 0x00CE7F60
    hkOstream& operator<<(unsigned char);
    hkOstream& operator<<(signed char);
    hkOstream& operator<<(char c); // 0x00CE7C60
    hkOstream& operator<<(hkBool b); // 0x00CE7F30
    hkOstream& operator<<(const void* p); // 0x00CE7EE0
    int write(const char* buf, int nbytes); // 0x00CE7C90
    void printf(const char*, ...); // 0x00CE8290
    void flush(); // 0x00CE7C80
    hkStreamWriter* getStreamWriter() { return m_writer.val(); } // 0x00F7AEF0 (inline, emitted in hkDeepCopier.obj)
    void setStreamWriter(hkStreamWriter* newWriter); // 0x00CE8370
    virtual const hkClass* getClassType() const; // vtbl+0x08 0x00CE7C40
    static const hkClass& staticClass(); // 0x00CE7C50

public:
    hkRefPtr<hkStreamWriter> m_writer; // 0x08
};
