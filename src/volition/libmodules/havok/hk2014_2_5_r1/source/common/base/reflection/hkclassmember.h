// Original: volition/libmodules/havok/hk2014_2_5_r1/source/common/base/reflection/hkclassmember.h
//
// Havok 2014.2.5: full declaration from rfg.pdb (stub_impl.md S4, serializer).
#pragma once

#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/memory/memoryclasses/hkmemoryclasses.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/types/hkbasetypes.h"

class hkClass;
class hkClassEnum;
class hkCustomAttributes;
struct hkInternalClassMember;
struct hkVariant;

// size 0x18
class hkClassMember
{
public:
    enum Type
    {
        TYPE_VOID = 0,
        TYPE_BOOL = 1,
        TYPE_CHAR = 2,
        TYPE_INT8 = 3,
        TYPE_UINT8 = 4,
        TYPE_INT16 = 5,
        TYPE_UINT16 = 6,
        TYPE_INT32 = 7,
        TYPE_UINT32 = 8,
        TYPE_INT64 = 9,
        TYPE_UINT64 = 10,
        TYPE_REAL = 11,
        TYPE_VECTOR4 = 12,
        TYPE_QUATERNION = 13,
        TYPE_MATRIX3 = 14,
        TYPE_ROTATION = 15,
        TYPE_QSTRANSFORM = 16,
        TYPE_MATRIX4 = 17,
        TYPE_TRANSFORM = 18,
        TYPE_ZERO = 19,
        TYPE_POINTER = 20,
        TYPE_FUNCTIONPOINTER = 21,
        TYPE_ARRAY = 22,
        TYPE_INPLACEARRAY = 23,
        TYPE_ENUM = 24,
        TYPE_STRUCT = 25,
        TYPE_SIMPLEARRAY = 26,
        TYPE_HOMOGENEOUSARRAY = 27,
        TYPE_VARIANT = 28,
        TYPE_CSTRING = 29,
        TYPE_ULONG = 30,
        TYPE_FLAGS = 31,
        TYPE_HALF = 32,
        TYPE_STRINGPTR = 33,
        TYPE_RELARRAY = 34,
        TYPE_MAX = 35,
    };

    enum FlagValues
    {
        FLAGS_NONE = 0,
        ALIGN_8 = 128,
        ALIGN_16 = 256,
        NOT_OWNED = 512,
        SERIALIZE_IGNORED = 1024,
        ALIGN_32 = 2048,
        ALIGN_REAL = 256,
    };

    enum DeprecatedFlagValues
    {
        DEPRECATED_SIZE_8 = 8,
        DEPRECATED_ENUM_8 = 8,
        DEPRECATED_SIZE_16 = 16,
        DEPRECATED_ENUM_16 = 16,
        DEPRECATED_SIZE_32 = 32,
        DEPRECATED_ENUM_32 = 32,
    };

    enum
    {
        HK_CLASS_ZERO_DEFAULT = -2,
    };

    typedef hkFlags<enum FlagValues,unsigned short> Flags;

    // size 0xC
    struct TypeProperties
    {
        HK_DECLARE_NONVIRTUAL_CLASS_ALLOCATOR(TypeProperties);

        hkEnum<enum Type,unsigned char> m_type; // 0x00
        const char* m_name; // 0x04
        short m_size; // 0x08
        short m_align; // 0x0A
    };

    HK_DECLARE_NONVIRTUAL_CLASS_ALLOCATOR(hkClassMember);
    static const hkClass& staticClass(); // 0x00CEB880
    const char* getName() const; // 0x00CF2930
    Type getType() const; // 0x00CF3600
    void setType(Type type); // 0x00FC29A0
    Type getSubType() const; // 0x00CF3610
    void setSubType(Type subtype); // 0x00FC29B0
    Type getArrayType() const; // 0x00D0E2A0
    int getSizeInBytes() const; // 0x00D0E0E0
    int getAlignment() const; // 0x00D0E1F0
    hkBool isNotOwner() const; // 0x00D0E1D0
    int getTypeName(char* buf, int bufLen) const; // 0x00D0E670
    int getArrayMemberSize() const; // 0x00D0E2B0
    hkBool hasClass() const; // 0x00F660D0
    const hkClass& getStructClass() const; // 0x00D0E070
    const hkClass* getClass() const; // 0x00D0E080
    hkBool hasEnumClass() const; // 0x00F77D40
    const hkClassEnum& getEnumClass() const; // 0x00D0E090
    int getCstyleArraySize() const; // 0x00D0E0A0
    const hkClassEnum& getEnumType() const; // 0x00D0E0B0
    int getEnumValue(const void* memberAddress) const; // 0x00D0E2F0
    void setEnumValue(void* memberAddress, int value) const; // 0x00D0E310
    int getOffset() const; // 0x00F586E0
    void setOffset(int offset); // 0x00F660C0
    hkFlags<enum FlagValues,unsigned short>& getFlags(); // 0x00FC2960
    const hkFlags<enum FlagValues,unsigned short>& getFlags() const; // 0x00D0DFE0
    const hkVariant* getAttribute(const char* id) const; // 0x00D0E0C0
    static Type getTypeOf(const char* name); // 0x00D0E340
    static Type getSubtypeOf(const char* name); // 0x00D0E4D0
    static const TypeProperties& getClassMemberTypeProperties(Type type); // 0x00D0DFF0

    static const hkInternalClassMember Members[]; // 0x016F3F80
public:
    const char* m_name; // 0x00
    const hkClass* m_class; // 0x04
    const hkClassEnum* m_enum; // 0x08
    hkEnum<enum Type,unsigned char> m_type; // 0x0C
    hkEnum<enum Type,unsigned char> m_subtype; // 0x0D
    short m_cArraySize; // 0x0E
    hkFlags<enum FlagValues,unsigned short> m_flags; // 0x10
    unsigned short m_offset; // 0x12
    const hkCustomAttributes* m_attributes; // 0x14
};

#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/reflection/hkclassmember.inl"
