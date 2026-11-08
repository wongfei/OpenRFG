// Original: volition/libmodules/havok/hk2014_2_5_r1/source/common/base/reflection/hkclassmemberaccessor.h
//
// Havok 2014.2.5: full declaration from rfg.pdb (stub_impl.md S4, serializer).
#pragma once

#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/memory/memoryclasses/hkmemoryclasses.h"
#include <stdint.h>
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/serialize/resource/hkobjectresource.h"

class hkBool;
class hkClass;
class hkClassAccessor;
class hkClassMember;
class hkHalf;
class hkStringPtr;

// size 0x8
class hkClassMemberAccessor
{
public:
    typedef void* Pointer;
    typedef char* Cstring;

    // size 0x8
    struct SimpleArray
    {
        HK_DECLARE_NONVIRTUAL_CLASS_ALLOCATOR(SimpleArray);

        void* data; // 0x00
        int size; // 0x04
    };

    // size 0xC
    struct HomogeneousArray
    {
        HK_DECLARE_NONVIRTUAL_CLASS_ALLOCATOR(HomogeneousArray);

        hkClass* klass; // 0x00
        void* data; // 0x04
        int size; // 0x08
    };

    // size 0x10
    struct Vector4
    {
        void setCross(const Vector4&, const Vector4&);
        HK_DECLARE_NONVIRTUAL_CLASS_ALLOCATOR(Vector4);

        float r[4]; // 0x00
    };

    // size 0x30
    struct Matrix3
    {
        HK_DECLARE_NONVIRTUAL_CLASS_ALLOCATOR(Matrix3);

        Vector4 v[3]; // 0x00
    };

    // size 0x40
    struct Transform
    {
        HK_DECLARE_NONVIRTUAL_CLASS_ALLOCATOR(Transform);

        Vector4 r[3]; // 0x00
        Vector4 t; // 0x30
    };

    struct Rotation; // no definition in PDB

    HK_DECLARE_NONVIRTUAL_CLASS_ALLOCATOR(hkClassMemberAccessor);
    hkClassMemberAccessor();
    hkClassMemberAccessor(void* obj, const hkClass& klass, const char* memberName); // 0x00FC06F0
    hkClassMemberAccessor(const hkVariant& var, const char* memberName); // 0x00FC06D0
    hkClassMemberAccessor(void* obj, const hkClassMember* mem); // 0x00FC06B0
    hkBool isOk() const; // 0x00FC0710
    const hkClassMember& getClassMember() const; // 0x00FC08A0
    void* asRaw() const; // 0x00FC0720
    void* getAddress() const; // 0x00FC0730
    void*& asPointer(int index) const; // 0x00FC0740
    char*& asCstring(int index) const; // 0x00FC0750
    hkStringPtr& asStringPtr(int index) const; // 0x00FC0760
    hkBool& asBool(int index) const; // 0x00FC0770
    float& asReal(int index) const; // 0x00FC0780
    hkHalf& asHalf(int index) const; // 0x00FC0790
    int& asInt32(int index) const; // 0x00FC07A0
    unsigned int& asUint32(int index) const; // 0x00FC07B0
    int64_t& asInt64(int index) const; // 0x00FC07C0
    uint64_t& asUint64(int index) const; // 0x00FC07D0
    unsigned long& asUlong(int index) const; // 0x00FC07E0
    short& asInt16(int index) const; // 0x00FC07F0
    unsigned short& asUint16(int index) const; // 0x00FC0800
    signed char& asInt8(int index) const; // 0x00FC0810
    unsigned char& asUint8(int index) const; // 0x00FC0820
    SimpleArray& asSimpleArray(int index) const; // 0x00FC0870
    HomogeneousArray& asHomogeneousArray(int index) const; // 0x00FC0880
    hkVariant& asVariant(int index) const; // 0x00FC0890
    Vector4& asVector4(int index) const; // 0x00FC0830
    Matrix3& asMatrix3(int index) const; // 0x00FC0840
    Transform& asTransform(int index) const; // 0x00FC0850
    Matrix3& asRotation(int index) const; // 0x00FC0860
    hkClassMemberAccessor member(const char* name) const; // 0x00FC08B0
    hkClassAccessor object() const; // 0x00FC0920
public:
    void connect(void* obj, const hkClass& klass, const char* memberName); // 0x00FC0680
    void connect(void* obj, const hkClassMember* mem); // 0x00FC0660

    void* m_address; // 0x00
    const hkClassMember* m_member; // 0x04
};

// size 0x8
class hkClassAccessor
{
public:
    HK_DECLARE_NONVIRTUAL_CLASS_ALLOCATOR(hkClassAccessor);
    hkClassAccessor(hkVariant& v); // 0x00FC0900
    hkClassAccessor(void* object, const hkClass* klass); // 0x00FC08E0
    hkClassMemberAccessor member(const hkClassMember* mem) const { return hkClassMemberAccessor(m_variant.m_object, mem); } // 0x00F57EB0 (.h:223)
    hkClassMemberAccessor member(const char* memberName) const { return hkClassMemberAccessor(m_variant, memberName); } // 0x00F57E90 (.h:218)
    const hkClass& getClass() const { return *m_variant.m_class; } // 0x00F57330 (.h:228)
    void* getAddress() const { return m_variant.m_object; } // 0x00F57340 (.h:233)

public:
    hkVariant m_variant; // 0x00
};
