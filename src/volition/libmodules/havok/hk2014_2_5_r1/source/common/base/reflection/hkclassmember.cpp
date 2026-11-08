// Original: volition/libmodules/havok/hk2014_2_5_r1/source/common/base/reflection/hkclassmember.cpp
//
// Havok 2014.2.5 (stub_impl.md S4, serializer SB): reflected members (sizes, alignment, type names and the parsing of
// C++ type spellings), from rfg.exe.

#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/reflection/hkclassmember.h"

#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/container/string/hkstring.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/container/string/hkstringbuf.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/reflection/hkclass.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/reflection/hkclassenum.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/reflection/hkcustomattributes.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/reflection/util/hkvariantdatautil.h"

// 0x016F4780: type, name, size and alignment of each member type (-1 for the types sized by their class, enum
// storage or C array); values from rfg.exe
static hkClassMember::TypeProperties ClassMemberProperties[] =
{
    { { hkClassMember::TYPE_VOID }, "void", -1, -1 },
    { { hkClassMember::TYPE_BOOL }, "hkBool", 1, 1 },
    { { hkClassMember::TYPE_CHAR }, "hkChar", 1, 1 },
    { { hkClassMember::TYPE_INT8 }, "hkInt8", 1, 1 },
    { { hkClassMember::TYPE_UINT8 }, "hkUint8", 1, 1 },
    { { hkClassMember::TYPE_INT16 }, "hkInt16", 2, 2 },
    { { hkClassMember::TYPE_UINT16 }, "hkUint16", 2, 2 },
    { { hkClassMember::TYPE_INT32 }, "hkInt32", 4, 4 },
    { { hkClassMember::TYPE_UINT32 }, "hkUint32", 4, 4 },
    { { hkClassMember::TYPE_INT64 }, "hkInt64", 8, 8 },
    { { hkClassMember::TYPE_UINT64 }, "hkUint64", 8, 8 },
    { { hkClassMember::TYPE_REAL }, "hkReal", 4, 4 },
    { { hkClassMember::TYPE_VECTOR4 }, "hkVector4", 16, 16 },
    { { hkClassMember::TYPE_QUATERNION }, "hkQuaternion", 16, 16 },
    { { hkClassMember::TYPE_MATRIX3 }, "hkMatrix3", 48, 16 },
    { { hkClassMember::TYPE_ROTATION }, "hkRotation", 48, 16 },
    { { hkClassMember::TYPE_QSTRANSFORM }, "hkQsTransform", 48, 16 },
    { { hkClassMember::TYPE_MATRIX4 }, "hkMatrix4", 64, 16 },
    { { hkClassMember::TYPE_TRANSFORM }, "hkTransform", 64, 16 },
    { { hkClassMember::TYPE_ZERO }, "hkZero", -1, -1 },
    { { hkClassMember::TYPE_POINTER }, "hkPointer", 4, 4 },
    { { hkClassMember::TYPE_FUNCTIONPOINTER }, "hkFunctionPointer", 4, 4 },
    { { hkClassMember::TYPE_ARRAY }, "hkArray", 12, 4 },
    { { hkClassMember::TYPE_INPLACEARRAY }, "hkInplaceArray", -1, -1 },
    { { hkClassMember::TYPE_ENUM }, "hkEnum", -1, -1 },
    { { hkClassMember::TYPE_STRUCT }, "hkStruct", -1, -1 },
    { { hkClassMember::TYPE_SIMPLEARRAY }, "hkSimpleArray", 8, 4 },
    { { hkClassMember::TYPE_HOMOGENEOUSARRAY }, "hkHomogeneousArray", 12, 4 },
    { { hkClassMember::TYPE_VARIANT }, "hkVariant", 8, 4 },
    { { hkClassMember::TYPE_CSTRING }, "char*", 4, 4 },
    { { hkClassMember::TYPE_ULONG }, "hkUlong", 4, 4 },
    { { hkClassMember::TYPE_FLAGS }, "hkFlags", -1, -1 },
    { { hkClassMember::TYPE_HALF }, "hkHalf", 2, 2 },
    { { hkClassMember::TYPE_STRINGPTR }, "hkStringPtr", 4, 4 },
    { { hkClassMember::TYPE_RELARRAY }, "hkRelArray", 4, 2 },
    { { hkClassMember::TYPE_MAX }, "hkTypeMax", -1, -1 },
};

// 0x00D0DFF0 (:130)
const hkClassMember::TypeProperties& hkClassMember::getClassMemberTypeProperties(Type type)
{
    return ClassMemberProperties[type];
}

// 0x00D0E0E0 (:135): -1 for the types without a size of their own (void, zero, in-place array)
int hkClassMember::getSizeInBytes() const
{
    switch (m_type)
    {
        case TYPE_BOOL:
        case TYPE_CHAR:
        case TYPE_INT8:
        case TYPE_UINT8:
        case TYPE_INT16:
        case TYPE_UINT16:
        case TYPE_INT32:
        case TYPE_UINT32:
        case TYPE_INT64:
        case TYPE_UINT64:
        case TYPE_REAL:
        case TYPE_VECTOR4:
        case TYPE_QUATERNION:
        case TYPE_MATRIX3:
        case TYPE_ROTATION:
        case TYPE_QSTRANSFORM:
        case TYPE_MATRIX4:
        case TYPE_TRANSFORM:
        case TYPE_POINTER:
        case TYPE_FUNCTIONPOINTER:
        case TYPE_ARRAY:
        case TYPE_SIMPLEARRAY:
        case TYPE_HOMOGENEOUSARRAY:
        case TYPE_VARIANT:
        case TYPE_CSTRING:
        case TYPE_ULONG:
        case TYPE_HALF:
        case TYPE_STRINGPTR:
        case TYPE_RELARRAY:
        {
            int nelem = getCstyleArraySize() ? getCstyleArraySize() : 1;
            return ClassMemberProperties[m_type].m_size * nelem;
        }
        case TYPE_ENUM:
        case TYPE_FLAGS:
        {
            int nelem = getCstyleArraySize() ? getCstyleArraySize() : 1;
            return ClassMemberProperties[m_subtype].m_size * nelem;
        }
        case TYPE_STRUCT:
        {
            int nelem = getCstyleArraySize() ? getCstyleArraySize() : 1;
            return getStructClass().getObjectSize() * nelem;
        }
        default:
            return -1;
    }
}

// 0x00D0E1D0 (:200)
hkBool hkClassMember::isNotOwner() const
{
    return m_flags.allAreSet(NOT_OWNED);
}

// 0x00D0E1F0 (:205): a struct's largest member alignment (each member's computed twice, original), raised to a forced
// ALIGN_8/16/32
int hkClassMember::getAlignment() const
{
    Type type = getType();
    if (type == TYPE_ENUM || type == TYPE_FLAGS)
    {
        type = getSubType();
    }
    int align;
    if (type == TYPE_STRUCT)
    {
        align = 1;
        for (int i = 0; i < m_class->getNumMembers(); ++i)
        {
            if (align < m_class->getMember(i).getAlignment())
            {
                align = m_class->getMember(i).getAlignment();
            }
        }
    }
    else
    {
        align = ClassMemberProperties[type].m_align;
    }
    if (m_flags.anyIsSet(ALIGN_8 | ALIGN_16 | ALIGN_32))
    {
        int forced = m_flags.anyIsSet(ALIGN_32) ? 32 : (m_flags.anyIsSet(ALIGN_16) ? 16 : 8);
        if (align < forced)
        {
            align = forced;
        }
    }
    return align;
}

// 0x00D0E000 (:242): the types named by their own spelling
static bool isSimpleType(hkClassMember::Type type)
{
    return type >= 0 && (type < hkClassMember::TYPE_POINTER || type == hkClassMember::TYPE_VARIANT ||
                         type == hkClassMember::TYPE_ULONG || type == hkClassMember::TYPE_CSTRING ||
                         type == hkClassMember::TYPE_HALF || type == hkClassMember::TYPE_STRINGPTR);
}

// 0x00D0E030 (:251)
static void getSimpleTypeName(hkClassMember::Type type, int nelem, hkStringBuf& ret)
{
    if (nelem == 0)
    {
        ret = ClassMemberProperties[type].m_name;
    }
    else
    {
        ret.printf("%s[%i]", ClassMemberProperties[type].m_name, nelem);
    }
}

// 0x00D0E670 (:264): the C++ spelling into buf (the template brackets as HTML entities, original), its length returned
int hkClassMember::getTypeName(char* buf, int bufLen) const
{
    Type type = getType();
    const char* className = m_class != nullptr ? m_class->getName() : "unknown";
    const char* enumName = m_enum != nullptr ? m_enum->getName() : "unknown";
    hkStringBuf s;
    if (isSimpleType(type) || type == TYPE_HOMOGENEOUSARRAY)
    {
        getSimpleTypeName(type, m_cArraySize, s);
    }
    else if (type == TYPE_POINTER)
    {
        if (m_class != nullptr)
        {
            s.printf("struct %s*", m_class->getName());
        }
        else if (m_subtype == TYPE_CHAR)
        {
            s = "char*";
        }
        else
        {
            s = "void*";
        }
    }
    else if (type == TYPE_ARRAY || type == TYPE_SIMPLEARRAY || type == TYPE_RELARRAY)
    {
        Type arrayType = getArrayType();
        const char* arrayName = type == TYPE_ARRAY ? "hkArray" : (type == TYPE_SIMPLEARRAY ? "hkSimpleArray" : "hkRelArray");
        if (isSimpleType(arrayType))
        {
            s.printf("%s&lt;%s&gt;", arrayName, ClassMemberProperties[arrayType].m_name);
        }
        else if (arrayType == TYPE_POINTER)
        {
            if (m_class != nullptr)
            {
                s.printf("%s&lt;%s*&gt;", arrayName, className);
            }
            else
            {
                s.printf("%s&lt;void*&gt;", arrayName);
            }
        }
        else if (arrayType == TYPE_STRUCT)
        {
            s.printf("%s&lt;struct %s&gt;", arrayName, className);
        }
    }
    else if (type == TYPE_ENUM)
    {
        s.printf("enum %s", enumName);
    }
    else if (type == TYPE_FLAGS)
    {
        s.printf("flags %s", enumName);
    }
    else if (type == TYPE_STRUCT)
    {
        if (m_cArraySize == 0)
        {
            s.printf("struct %s", className);
        }
        else
        {
            s.printf("struct %s[%i]", className, int(m_cArraySize));
        }
    }
    hkString::strNcpy(buf, s.cString(), bufLen);
    return s.getLength();
}

// 0x00D0E2A0 (:346)
hkClassMember::Type hkClassMember::getArrayType() const
{
    return m_subtype;
}

// 0x00D0E2B0 (:356): -1 for enums and flags
int hkClassMember::getArrayMemberSize() const
{
    if (getArrayType() != TYPE_ENUM && getArrayType() != TYPE_FLAGS)
    {
        if (getArrayType() == TYPE_STRUCT)
        {
            return getStructClass().getObjectSize();
        }
        return ClassMemberProperties[getArrayType()].m_size;
    }
    return -1;
}

// 0x00D0E070 (:381)
const hkClass& hkClassMember::getStructClass() const
{
    return *m_class;
}

// 0x00D0E080 (:386)
const hkClass* hkClassMember::getClass() const
{
    return m_class;
}

// 0x00D0E090 (:391)
const hkClassEnum& hkClassMember::getEnumClass() const
{
    return *m_enum;
}

// 0x00D0E0A0 (:396)
int hkClassMember::getCstyleArraySize() const
{
    return m_cArraySize;
}

// 0x00D0E0B0 (:401)
const hkClassEnum& hkClassMember::getEnumType() const
{
    return *m_enum;
}

// 0x00D0E2F0 (:407): the value read with the enum's storage type
int hkClassMember::getEnumValue(const void* memberAddress) const
{
    return static_cast<int>(hkVariantDataUtil::getInt(getType(), getSubType(), memberAddress));
}

// 0x00D0E310 (:412)
void hkClassMember::setEnumValue(void* memberAddress, int value) const
{
    hkVariantDataUtil::setInt(getType(), getSubType(), memberAddress, value);
}

// 0x00D0E0C0 (:417)
const hkVariant* hkClassMember::getAttribute(const char* id) const
{
    return m_attributes != nullptr ? m_attributes->getAttribute(id) : nullptr;
}

// 0x00D0E340 (:422): the member type of a C++ spelling; an array suffix ("[4]") is ignored
hkClassMember::Type hkClassMember::getTypeOf(const char* name)
{
    if (hkString::strNcmp(name, "enum ", 5) == 0)
    {
        return TYPE_ENUM;
    }
    if (hkString::strNcmp(name, "flags ", 6) == 0)
    {
        return TYPE_FLAGS;
    }
    if (hkString::strNcmp(name, "hkArray<", 8) == 0)
    {
        return TYPE_ARRAY;
    }
    if (hkString::strNcmp(name, "hkRelArray<", 11) == 0)
    {
        return TYPE_RELARRAY;
    }
    if (hkString::strNcmp(name, "hkSimpleArray<", 14) == 0)
    {
        return TYPE_SIMPLEARRAY;
    }
    if (hkString::strNcmp(name, "char*", 5) == 0)
    {
        return TYPE_CSTRING;
    }
    if (hkString::strNcmp(name, "hkStringPtr", 11) == 0)
    {
        return TYPE_STRINGPTR;
    }
    const char* star = hkString::strRchr(name, '*');
    if (star != nullptr && star[1] == 0)
    {
        return TYPE_POINTER;
    }
    hkStringBuf s(name);
    const char* bracket = hkString::strChr(name, '[');
    if (bracket != nullptr)
    {
        s.slice(0, static_cast<int>(bracket - name));
    }
    for (int i = 0; i < TYPE_MAX; ++i)
    {
        if (s == ClassMemberProperties[i].m_name)
        {
            return static_cast<Type>(i);
        }
    }
    return TYPE_VOID;
}

// 0x00D0E4D0 (:474): the element type of an array spelling; for a C array, its size (returned as a Type, original)
hkClassMember::Type hkClassMember::getSubtypeOf(const char* name)
{
    if (hkString::strNcmp(name, "hkArray<", 8) == 0)
    {
        hkStringBuf s(name + 8, hkString::strLen(name + 8) - 1);
        return getTypeOf(s.cString());
    }
    if (hkString::strNcmp(name, "hkRelArray<", 11) == 0)
    {
        hkStringBuf s(name + 11, hkString::strLen(name + 11) - 1);
        return getTypeOf(s.cString());
    }
    if (hkString::strNcmp(name, "hkSimpleArray<", 14) == 0)
    {
        hkStringBuf s(name + 14, hkString::strLen(name + 14) - 1);
        return getTypeOf(s.cString());
    }
    const char* open = hkString::strChr(name, '[');
    if (open != nullptr)
    {
        const char* close = hkString::strChr(name, ']');
        hkStringBuf s(open + 1, static_cast<int>(close - open - 1));
        return static_cast<Type>(hkString::atoi(s.cString(), 0));
    }
    return TYPE_VOID;
}
