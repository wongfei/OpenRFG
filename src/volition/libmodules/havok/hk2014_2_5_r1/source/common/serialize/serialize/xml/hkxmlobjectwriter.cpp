// Original: volition/libmodules/havok/hk2014_2_5_r1/source/common/serialize/serialize/xml/hkxmlobjectwriter.cpp
//
// Havok 2014.2.5 (stub_impl.md S4, serializer SF): the XML object writer of the XML packfile writer, from rfg.exe. The
// file statics taking the indent, a member type or a string are LTCG register-argument functions in rfg.exe (read
// from the asm).

#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/serialize/serialize/xml/hkxmlobjectwriter.h"

#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/container/string/hkstring.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/container/string/hkstringptr.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/reflection/hkclass.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/reflection/hkclassenum.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/reflection/hkclassmember.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/system/error/hkerror.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/system/io/oarchive/hkoarchive.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/system/io/ostream/hkostream.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/system/io/writer/hkstreamwriter.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/serialize/resource/hkobjectresource.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/serialize/serialize/hkrelocationinfo.h"

// 0x01058600 (:30). Original bug kept: a new address is printed with the map's size before the insert but stored as
// size + 1, so its first name ("#0000" for the first address) differs from the later ones ("#0001").
int hkXmlObjectWriter::SequentialNameFromAddress::nameFromAddress(const void* addr, char* buf, int bufSize)
{
    int id = m_map.getWithDefault(addr, 0);
    if (id == 0)
    {
        id = m_map.getSize();
        m_map.insert(addr, id + 1);
    }
    return hkString::snprintf(buf, bufSize, "#%04i", id);
}

// 0x01058790 (:42)
hkXmlObjectWriter::hkXmlObjectWriter(NameFromAddress& n, hkBool writeSerializeIgnored)
    : m_nameFromAddress(n)
    , m_writeSerializeIgnored(writeSerializeIgnored)
{
    m_indent.pushBack(0);
    m_indent.popBack();
}

// 0x01058660 (:50): grows with tabs and keeps the zero after the last; LTCG: in in ESI, delta in ECX
static void adjustIndent(hkArray<char,hkContainerHeapAllocator>& in, int delta)
{
    in.setSize(in.getSize() + delta + 1, '\t');
    in.back() = 0;
    in.popBack();
}

// 0x010586C0 (:57)
static void increaseIndent(hkArray<char,hkContainerHeapAllocator>& in)
{
    adjustIndent(in, 1);
}

// 0x010586D0 (:58)
static void decreaseIndent(hkArray<char,hkContainerHeapAllocator>& in)
{
    adjustIndent(in, -1);
}

// <hkBool> 0x01059540, <char> 0x01059550, <signed char> 0x01059560, <unsigned char> 0x01059570, <short> 0x01059580,
// <unsigned short> 0x01059590, <int> 0x010595A0, <unsigned int> 0x010595B0, <__int64> 0x010595C0,
// <unsigned __int64> 0x010595D0, <unsigned long> 0x010595E0, <float> 0x010595F0, <hkHalf> 0x01059600,
// <const void*> 0x01059610, <hkVariant> 0x01059620 (:62)
template<typename T>
static const T& lookupMember(const void* start)
{
    return *static_cast<const T*>(start);
}

// 0x01057900 (:67): a C-style array's length, 1 for a single value
static int xmlObjectWriter_getNumElementsInMember(const hkClassMember& member)
{
    const int n = member.getCstyleArraySize();
    return n ? n : 1;
}

// 0x01058800 (:73): '<name a="v" ...>' on a new indented line (prefixNewline); the indent grows
void hkXmlObjectWriter::beginElement(hkStreamWriter* writer, const char* name, const char* const * attributes, hkBool prefixNewline)
{
    if (prefixNewline)
    {
        writer->write("\n", 1);
        writer->write(m_indent.begin(), m_indent.getSize());
    }
    hkOstream os(writer);
    os.printf("<%s", name);
    if (attributes)
    {
        for (int i = 0; attributes[i] != nullptr; i += 2)
        {
            os.printf(" %s=\"%s\"", attributes[i], attributes[i + 1]);
        }
    }
    os.printf(">");
    adjustIndent(1);
}

// 0x010588C0 (:94)
void hkXmlObjectWriter::endElement(hkStreamWriter* writer, const char* name, hkBool prefixNewline)
{
    adjustIndent(-1);
    if (prefixNewline)
    {
        writer->write("\n", 1);
        writer->write(m_indent.begin(), m_indent.getSize());
    }
    writer->write("</", 2);
    writer->write(name, hkString::strLen(name));
    writer->write(">", 1);
}

// 0x01057910 (:109): base64, 19 groups (76 characters) a line; the stream is checked after each full group
hkResult hkXmlObjectWriter::base64write(hkStreamWriter* sw, const void* buf, int len)
{
    static const char table[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    hkOArchive oa(sw, false);
    const unsigned char* p = static_cast<const unsigned char*>(buf);
    int groupsLeft = 19;
    while (len > 0)
    {
        unsigned char in[3] = { 0, 0, 0 };
        const int n = len < 3 ? len : 3;
        hkString::memCpy(in, p, n);
        char out[4];
        out[0] = table[in[0] >> 2];
        out[1] = table[(in[1] >> 4) | ((in[0] & 3) << 4)];
        out[2] = table[(in[2] >> 6) | ((in[1] & 0xF) << 2)];
        out[3] = table[in[2] & 0x3F];
        p += n;
        len -= n;
        if (n < 3)
        {
            if (n == 1)
            {
                out[2] = '=';
                out[3] = '=';
            }
            else
            {
                out[3] = '=';
            }
            oa.writeRaw(out, 4);
            return HK_SUCCESS;
        }
        oa.writeRaw(out, 4);
        if (--groupsLeft == 0)
        {
            oa.writeRaw("\n", 1);
            groupsLeft = 19;
        }
        if (!sw->isOk())
        {
            return HK_FAILURE;
        }
    }
    return HK_SUCCESS;
}

// The five XML entities, the replaced character first
static const char* const s_entities[] = { "<&lt;", ">&gt;", "&&amp;", "\"&quot;", "'&apos;", nullptr };

// 0x01057B00 (:182): control and non-ASCII characters (signed char <= 0x14 or >= 0x7F) as "&#N;", the five entities
// by name; a null string as "&#9216;". LTCG: str in EAX.
static hkResult writeCstring(const char* str, hkOstream& os)
{
    if (str == nullptr)
    {
        os << "&#9216;";
        return HK_SUCCESS;
    }
    const char* start = str;
    for (; *str != 0; str++)
    {
        const char c = *str;
        if (c <= 0x14 || c >= 0x7F)
        {
            os.write(start, int(str - start));
            os.printf("&#%u;", (unsigned int)(unsigned char)c);
            start = str + 1;
        }
        else if (c == '"' || c == '&' || c == '\'' || c == '<' || c == '>')
        {
            os.write(start, int(str - start));
            start = str + 1;
            for (int i = 0; s_entities[i] != nullptr; i++)
            {
                if (s_entities[i][0] == c)
                {
                    os.write(s_entities[i] + 1, hkString::strLen(s_entities[i] + 1));
                    break;
                }
            }
        }
    }
    os.write(start, int(str - start));
    return HK_SUCCESS;
}

// 0x01057C50 (:232): the strings back to back (rfg.exe inlines writeCstring)
static hkResult writeCstrings(const char* const * array, int nelem, hkOstream& os)
{
    for (int i = 0; i < nelem; i++)
    {
        writeCstring(array[i], os);
    }
    return HK_SUCCESS;
}

// 0x01057E40 (:241)
static hkResult writeCstringPtrs(const hkStringPtr* array, int nelem, hkOstream& os)
{
    for (int i = 0; i < nelem; i++)
    {
        writeCstring(array[i].cString(), os);
    }
    return HK_SUCCESS;
}

// 0x01057FC0 (:250): one value of a plain member type; vectors and matrices as "(x y z w)" groups (columns), pointers
// by name. LTCG: mtype in EAX, memberAddress in ECX, os in EDI.
static void writePodMember(hkClassMember::Type mtype, const void* memberAddress, hkOstream& os, hkXmlObjectWriter& xml)
{
    const float* f = static_cast<const float*>(memberAddress);
    switch (mtype)
    {
        case hkClassMember::TYPE_BOOL:
        {
            os << (lookupMember<hkBool>(memberAddress) ? "true" : "false");
            break;
        }
        case hkClassMember::TYPE_CHAR:
        {
            os.printf("%c", (int)lookupMember<char>(memberAddress));
            break;
        }
        case hkClassMember::TYPE_INT8:
        {
            os.printf("%i", (int)lookupMember<signed char>(memberAddress));
            break;
        }
        case hkClassMember::TYPE_UINT8:
        {
            os.printf("%u", (unsigned int)lookupMember<unsigned char>(memberAddress));
            break;
        }
        case hkClassMember::TYPE_INT16:
        {
            os.printf("%i", (int)lookupMember<short>(memberAddress));
            break;
        }
        case hkClassMember::TYPE_UINT16:
        {
            os.printf("%u", (unsigned int)lookupMember<unsigned short>(memberAddress));
            break;
        }
        case hkClassMember::TYPE_INT32:
        {
            os.printf("%i", lookupMember<int>(memberAddress));
            break;
        }
        case hkClassMember::TYPE_UINT32:
        {
            os.printf("%u", lookupMember<unsigned int>(memberAddress));
            break;
        }
        case hkClassMember::TYPE_INT64:
        {
            os.printf("%I64i", lookupMember<int64_t>(memberAddress));
            break;
        }
        case hkClassMember::TYPE_UINT64:
        {
            os.printf("%I64u", lookupMember<uint64_t>(memberAddress));
            break;
        }
        case hkClassMember::TYPE_ULONG:
        {
            os.printf("%u", lookupMember<unsigned long>(memberAddress));
            break;
        }
        case hkClassMember::TYPE_REAL:
        {
            os.printf("%f", (double)lookupMember<float>(memberAddress));
            break;
        }
        case hkClassMember::TYPE_HALF:
        {
            // the half is the float's top 16 bits
            const unsigned int bits = (unsigned int)(int)*static_cast<const short*>(memberAddress) << 16;
            float v;
            hkString::memCpy(&v, &bits, sizeof(v));
            os.printf("%f", (double)v);
            break;
        }
        case hkClassMember::TYPE_VECTOR4:
        case hkClassMember::TYPE_QUATERNION:
        {
            os.printf("(%f %f %f %f)", (double)f[0], (double)f[1], (double)f[2], (double)f[3]);
            break;
        }
        case hkClassMember::TYPE_MATRIX3:
        case hkClassMember::TYPE_ROTATION:
        {
            for (int c = 0; c < 3; c++)
            {
                os.printf("(%f %f %f)", (double)f[c * 4 + 0], (double)f[c * 4 + 1], (double)f[c * 4 + 2]);
            }
            break;
        }
        case hkClassMember::TYPE_QSTRANSFORM:
        {
            os.printf("(%f %f %f)", (double)f[0], (double)f[1], (double)f[2]);
            os.printf("(%f %f %f %f)", (double)f[4], (double)f[5], (double)f[6], (double)f[7]);
            os.printf("(%f %f %f)", (double)f[8], (double)f[9], (double)f[10]);
            break;
        }
        case hkClassMember::TYPE_MATRIX4:
        {
            for (int c = 0; c < 4; c++)
            {
                os.printf("(%f %f %f %f)", (double)f[c * 4 + 0], (double)f[c * 4 + 1], (double)f[c * 4 + 2], (double)f[c * 4 + 3]);
            }
            break;
        }
        case hkClassMember::TYPE_TRANSFORM:
        {
            for (int c = 0; c < 4; c++)
            {
                os.printf("(%f %f %f)", (double)f[c * 4 + 0], (double)f[c * 4 + 1], (double)f[c * 4 + 2]);
            }
            break;
        }
        case hkClassMember::TYPE_POINTER:
        {
            char buf[256];
            const int n = xml.nameFromAddress(lookupMember<const void*>(memberAddress), buf, sizeof(buf));
            os.write(buf, n);
            break;
        }
        case hkClassMember::TYPE_CSTRING:
        {
            os.printf("<hkcstring>");
            writeCstring(*static_cast<const char* const*>(memberAddress), os);
            os.printf("</hkcstring>");
            break;
        }
        case hkClassMember::TYPE_STRINGPTR:
        {
            os.printf("<hkcstring>");
            writeCstring(static_cast<const hkStringPtr*>(memberAddress)->cString(), os);
            os.printf("</hkcstring>");
            break;
        }
        default:
        {
            break;
        }
    }
}

static void writeMember(hkArray<char,hkContainerHeapAllocator>& indent, const hkClassMember& member, const void* objectData, hkOstream& os, hkXmlObjectWriter& xml);
static hkResult writeStruct(hkArray<char,hkContainerHeapAllocator>& indent, const hkClass& klass, const void* objectData, hkOstream& os, hkXmlObjectWriter& xml);

// 0x01058940 (:403): the elements on indented lines, 16 plain values a line (1 for vectors and matrices, each string
// on its own line). LTCG: member in EAX.
static void writeArray(hkArray<char,hkContainerHeapAllocator>& indent, const hkClassMember& member, const void* begin, int size, hkOstream& os, hkXmlObjectWriter& xml)
{
    const int elemSize = member.getArrayMemberSize();
    const hkClassMember::Type type = member.getArrayType();
    if (size == 0)
    {
        return;
    }
    adjustIndent(indent, 1);
    const char* p = static_cast<const char*>(begin);
    switch (type)
    {
        case hkClassMember::TYPE_BOOL:
        case hkClassMember::TYPE_CHAR:
        case hkClassMember::TYPE_INT8:
        case hkClassMember::TYPE_UINT8:
        case hkClassMember::TYPE_INT16:
        case hkClassMember::TYPE_UINT16:
        case hkClassMember::TYPE_INT32:
        case hkClassMember::TYPE_UINT32:
        case hkClassMember::TYPE_INT64:
        case hkClassMember::TYPE_UINT64:
        case hkClassMember::TYPE_REAL:
        case hkClassMember::TYPE_VECTOR4:
        case hkClassMember::TYPE_QUATERNION:
        case hkClassMember::TYPE_MATRIX3:
        case hkClassMember::TYPE_ROTATION:
        case hkClassMember::TYPE_QSTRANSFORM:
        case hkClassMember::TYPE_MATRIX4:
        case hkClassMember::TYPE_TRANSFORM:
        case hkClassMember::TYPE_POINTER:
        case hkClassMember::TYPE_CSTRING:
        case hkClassMember::TYPE_ULONG:
        case hkClassMember::TYPE_HALF:
        case hkClassMember::TYPE_STRINGPTR:
        {
            // TYPE_VECTOR4 .. TYPE_ZERO: one a line
            const int numPerLine = (unsigned int)(type - hkClassMember::TYPE_VECTOR4) <= 6 ? 1 : 16;
            for (int i = 0; i < size; i++)
            {
                if (i % numPerLine == 0 || type == hkClassMember::TYPE_CSTRING || type == hkClassMember::TYPE_STRINGPTR)
                {
                    os.printf("\n%s", indent.begin());
                }
                else
                {
                    os << ' ';
                }
                writePodMember(type, p, os, xml);
                p += elemSize;
            }
            break;
        }
        case hkClassMember::TYPE_ZERO:
        {
            os.printf("<!-- zero array %s -->", member.getName());
            break;
        }
        case hkClassMember::TYPE_STRUCT:
        {
            for (int i = 0; i < size; i++)
            {
                writeStruct(indent, member.getStructClass(), p, os, xml);
                p += elemSize;
            }
            break;
        }
        case hkClassMember::TYPE_VARIANT:
        {
            for (int i = 0; i < size; i++)
            {
                const hkVariant& v = lookupMember<hkVariant>(p);
                char objectName[256];
                char className[256];
                xml.nameFromAddress(v.m_object, objectName, sizeof(objectName));
                xml.nameFromAddress(v.m_class, className, sizeof(className));
                os.printf("(%s %s%s)", objectName, className, i + 1 < size ? " " : "");
                p += elemSize;
            }
            break;
        }
        default:
        {
            break;
        }
    }
    adjustIndent(indent, -1);
    os.printf("\n%s", indent.begin());
}

// 0x010586E0 (:516): '<hkobject>' with the class's members. LTCG: indent in EAX.
static hkResult writeStruct(hkArray<char,hkContainerHeapAllocator>& indent, const hkClass& klass, const void* objectData, hkOstream& os, hkXmlObjectWriter& xml)
{
    os.printf("\n%s<hkobject>", indent.begin());
    adjustIndent(indent, 1);
    for (int i = 0; i < klass.getNumMembers(); i++)
    {
        writeMember(indent, klass.getMember(i), objectData, os, xml);
    }
    adjustIndent(indent, -1);
    os.printf("\n%s</hkobject>", indent.begin());
    return HK_SUCCESS;
}

// 0x01058BB0 (:529): '<hkparam name="">' with the member's value (and "numelements" for arrays); serialize-ignored
// members and null char pointers are written as comments. Original quirk kept: an enum name is printed as a format
// string.
static void writeMember(hkArray<char,hkContainerHeapAllocator>& indent, const hkClassMember& member, const void* objectData, hkOstream& os, hkXmlObjectWriter& xml)
{
    const void* addr = static_cast<const char*>(objectData) + member.getOffset();
    const hkClassMember::Type type = member.getType();
    if ((member.getFlags().get() & hkClassMember::SERIALIZE_IGNORED) && !xml.getWriteSerializeIgnored())
    {
        os.printf("\n%s<!-- %s SERIALIZE_IGNORED -->", indent.begin(), member.getName());
        return;
    }
    if (type == hkClassMember::TYPE_POINTER && member.getSubType() == hkClassMember::TYPE_CHAR && *static_cast<const void* const*>(addr) == nullptr)
    {
        os.printf("\n%s<!-- <hkparam name=\"%s\">(null)</hkparam> -->", indent.begin(), member.getName());
        return;
    }
    os.printf("\n%s<hkparam name=\"%s\"", indent.begin(), member.getName());
    switch (type)
    {
        case hkClassMember::TYPE_ARRAY:
        case hkClassMember::TYPE_INPLACEARRAY:
        case hkClassMember::TYPE_SIMPLEARRAY:
        {
            os.printf(" numelements=\"%i\"", static_cast<const int*>(addr)[1]);
            break;
        }
        case hkClassMember::TYPE_HOMOGENEOUSARRAY:
        {
            os.printf(" numelements=\"%i\"", static_cast<const int*>(addr)[2]);
            break;
        }
        case hkClassMember::TYPE_RELARRAY:
        {
            os.printf(" numelements=\"%i\"", (int)*static_cast<const unsigned short*>(addr));
            break;
        }
        default:
        {
            break;
        }
    }
    os.printf(">");
    switch (type)
    {
        case hkClassMember::TYPE_BOOL:
        case hkClassMember::TYPE_CHAR:
        case hkClassMember::TYPE_INT8:
        case hkClassMember::TYPE_UINT8:
        case hkClassMember::TYPE_INT16:
        case hkClassMember::TYPE_UINT16:
        case hkClassMember::TYPE_INT32:
        case hkClassMember::TYPE_UINT32:
        case hkClassMember::TYPE_INT64:
        case hkClassMember::TYPE_UINT64:
        case hkClassMember::TYPE_REAL:
        case hkClassMember::TYPE_VECTOR4:
        case hkClassMember::TYPE_QUATERNION:
        case hkClassMember::TYPE_MATRIX3:
        case hkClassMember::TYPE_ROTATION:
        case hkClassMember::TYPE_QSTRANSFORM:
        case hkClassMember::TYPE_MATRIX4:
        case hkClassMember::TYPE_TRANSFORM:
        case hkClassMember::TYPE_ULONG:
        case hkClassMember::TYPE_HALF:
        {
            // a char array as text, other C-style arrays space separated with a line break every 50 values
            const int n = xmlObjectWriter_getNumElementsInMember(member);
            const int elemSize = member.getSizeInBytes() / n;
            const char* p = static_cast<const char*>(addr);
            for (int i = 0; i < n; i++)
            {
                if (type == hkClassMember::TYPE_CHAR)
                {
                    os.printf("%c", (int)*p);
                }
                else
                {
                    if (i != 0)
                    {
                        os.printf(i % 50 == 0 ? "\n" : " ");
                    }
                    writePodMember(type, p, os, xml);
                }
                p += elemSize;
            }
            break;
        }
        case hkClassMember::TYPE_ZERO:
        {
            os.printf("<!-- zero %s -->", member.getName());
            break;
        }
        case hkClassMember::TYPE_CSTRING:
        {
            writeCstrings(static_cast<const char* const*>(addr), xmlObjectWriter_getNumElementsInMember(member), os);
            break;
        }
        case hkClassMember::TYPE_STRINGPTR:
        {
            writeCstringPtrs(static_cast<const hkStringPtr*>(addr), xmlObjectWriter_getNumElementsInMember(member), os);
            break;
        }
        case hkClassMember::TYPE_POINTER:
        {
            const int n = xmlObjectWriter_getNumElementsInMember(member);
            if (member.getSubType() == hkClassMember::TYPE_CHAR)
            {
                writeCstrings(static_cast<const char* const*>(addr), n, os);
                break;
            }
            const void* const* ptrs = static_cast<const void* const*>(addr);
            for (int i = 0; i < n; i++)
            {
                if (ptrs[i] != nullptr)
                {
                    char buf[256];
                    xml.nameFromAddress(ptrs[i], buf, sizeof(buf));
                    os.printf("%s", buf);
                }
                else
                {
                    os << "null";
                }
                if (i < n - 1)
                {
                    os << " ";
                }
            }
            break;
        }
        case hkClassMember::TYPE_FUNCTIONPOINTER:
        {
            for (int i = xmlObjectWriter_getNumElementsInMember(member); i > 0; i--)
            {
                os << "null";
            }
            break;
        }
        case hkClassMember::TYPE_ARRAY:
        case hkClassMember::TYPE_INPLACEARRAY:
        case hkClassMember::TYPE_SIMPLEARRAY:
        {
            writeArray(indent, member, static_cast<const void* const*>(addr)[0], static_cast<const int*>(addr)[1], os, xml);
            break;
        }
        case hkClassMember::TYPE_RELARRAY:
        {
            const unsigned short* rel = static_cast<const unsigned short*>(addr);
            writeArray(indent, member, reinterpret_cast<const char*>(addr) + rel[1], rel[0], os, xml);
            break;
        }
        case hkClassMember::TYPE_ENUM:
        {
            const hkClassEnum& e = member.getEnumType();
            const int value = member.getEnumValue(addr);
            const char* name = nullptr;
            if (e.getNameOfValue(value, &name) == HK_SUCCESS)
            {
                // Original bug kept: the enum name is used as the format string.
                os.printf(name);
            }
            else
            {
                os.printf("INVALID_VALUE_%i", value);
            }
            break;
        }
        case hkClassMember::TYPE_FLAGS:
        {
            const hkClassEnum& e = member.getEnumType();
            const int value = member.getEnumValue(addr);
            hkArray<const char*,hkContainerHeapAllocator> bits;
            int leftover;
            e.decomposeFlags(value, bits, leftover);
            for (int i = 0; i < bits.getSize(); i++)
            {
                os.printf("%s%s", i != 0 ? "|" : "", bits[i]);
            }
            if (leftover != 0)
            {
                os.printf("%s<!-- UNKNOWN BITS -->0x%x", bits.getSize() != 0 ? "|" : "", leftover);
            }
            else if (bits.getSize() == 0)
            {
                os.printf("0");
            }
            break;
        }
        case hkClassMember::TYPE_STRUCT:
        {
            adjustIndent(indent, 1);
            const hkClass& sclass = member.getStructClass();
            const int n = xmlObjectWriter_getNumElementsInMember(member);
            const int size = sclass.getObjectSize();
            const char* p = static_cast<const char*>(addr);
            for (int i = 0; i < n; i++)
            {
                writeStruct(indent, sclass, p, os, xml);
                p += size;
            }
            adjustIndent(indent, -1);
            os << '\n' << indent.begin();
            break;
        }
        case hkClassMember::TYPE_HOMOGENEOUSARRAY:
        {
            // { const hkClass* klass; void* data; int size; }
            const hkClass* klass = *static_cast<const hkClass* const*>(addr);
            if (klass == nullptr)
            {
                break;
            }
            adjustIndent(indent, 1);
            os.printf("\n%s<!-- Homogeneous Class -->", indent.begin());
            char buf[256];
            xml.nameFromAddress(klass, buf, sizeof(buf));
            os.printf("%s", buf);
            const int size = klass->getObjectSize();
            const char* data = static_cast<const char* const*>(addr)[1];
            for (int i = 0; i < static_cast<const int*>(addr)[2]; i++)
            {
                writeStruct(indent, *klass, data + i * size, os, xml);
            }
            adjustIndent(indent, -1);
            break;
        }
        case hkClassMember::TYPE_VARIANT:
        {
            const int n = xmlObjectWriter_getNumElementsInMember(member);
            const hkVariant* v = static_cast<const hkVariant*>(addr);
            for (int i = 0; i < n; i++)
            {
                if (v[i].m_object != nullptr && v[i].m_class != nullptr)
                {
                    char objectName[256];
                    char className[256];
                    xml.nameFromAddress(v[i].m_object, objectName, sizeof(objectName));
                    xml.nameFromAddress(v[i].m_class, className, sizeof(className));
                    os.printf("(%s %s%s)", objectName, className, i + 1 < n ? " " : "");
                }
            }
            break;
        }
        default:
        {
            char buf[512];
            hkErrStream errs(buf, sizeof(buf));
            errs << "Unhandled member type found!";
            if (hkErrorFwd::messageError(0x40A18B57, buf, "Serialize\\Xml\\hkXmlObjectWriter.cpp", 802) != 0)
            {
                __debugbreak();
            }
            break;
        }
    }
    os.printf("</hkparam>");
}

// 0x010592D0 (:809): '<hkobject name="" class="" ...>' around the members
hkResult hkXmlObjectWriter::writeObjectWithElement(hkStreamWriter* writer, const void* objectData, const hkClass& klass, const char* objectName, const char* const * attributes)
{
    int numAttributes = 0;
    if (attributes)
    {
        while (attributes[numAttributes] != nullptr)
        {
            numAttributes++;
        }
    }
    hkArray<const char*,hkContainerHeapAllocator> attrs;
    attrs.reserve(numAttributes + 5);
    if (objectName)
    {
        attrs.pushBackUnchecked("name");
        attrs.pushBackUnchecked(objectName);
    }
    attrs.pushBackUnchecked("class");
    attrs.pushBackUnchecked(klass.getName());
    if (attributes)
    {
        hkString::memCpy(attrs.end(), attributes, numAttributes * (int)sizeof(const char*));
        attrs.setSizeUnchecked(attrs.getSize() + numAttributes);
    }
    attrs.pushBackUnchecked(nullptr);
    beginElement(writer, "hkobject", attrs.begin(), true);
    hkRelocationInfo reloc;
    writeObject(writer, objectData, klass, reloc);
    endElement(writer, "hkobject", true);
    return writer->isOk() ? HK_SUCCESS : HK_FAILURE;
}

// 0x010594B0 (:839): the members only
hkResult hkXmlObjectWriter::writeObject(hkStreamWriter* writer, const void* objectData, const hkClass& klass, hkRelocationInfo& reloc)
{
    hkOstream os(writer);
    for (int i = 0; i < klass.getNumMembers(); i++)
    {
        writeMember(m_indent, klass.getMember(i), objectData, os, *this);
    }
    return os.isOk() ? HK_SUCCESS : HK_FAILURE;
}

// 0x01057DD0 (:852): '<hkrawdata size="">' with the bytes in base64 in a CDATA section
hkResult hkXmlObjectWriter::writeRaw(hkStreamWriter* writer, const void* buf, int len)
{
    hkOstream os(writer);
    os.printf("\n%s<hkrawdata size=\"%i\"><![CDATA[\n", m_indent.begin(), len);
    const hkResult res = base64write(writer, buf, len);
    os.printf("\n]]></hkrawdata>");
    return res;
}

// 0x01058770 (:863)
void hkXmlObjectWriter::adjustIndent(int delta)
{
    ::adjustIndent(m_indent, delta);
}
