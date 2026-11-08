// Original: volition/libmodules/havok/hk2014_2_5_r1/source/common/serialize/serialize/xml/hkxmlobjectreader.cpp
//
// Havok 2014.2.5 (stub_impl.md S4, serializer SF): the XML object reader of the XML packfile reader, from rfg.exe. An
// object's "<hkparam>" members are read with hkXmlParser into a packfile style buffer: plain values in place, arrays
// and strings after the object, pointers as imports (by name) and locals (offsets) in the hkRelocationInfo. Most file
// statics are LTCG register-argument functions in rfg.exe (read from the asm).

#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/serialize/serialize/xml/hkxmlobjectreader.h"

#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/container/localarray/hklocalarray.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/container/string/hkstring.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/container/string/hkstringbuf.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/reflection/hkclass.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/reflection/hkclassenum.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/reflection/hkclassmember.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/system/error/hkerror.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/system/io/istream/hkistream.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/system/io/ostream/hkostream.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/system/io/reader/hkstreamreader.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/serialize/serialize/hkrelocationinfo.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/serialize/util/xml/hkxmlparser.h"

extern const hkClass hkClassClass;

namespace
{
    // The member layouts the reader fills (hkArray, hkHomogeneousArray; rfg.exe also looks up hkRelArray<unsigned char>)
    struct XmlObjectReader_DummyArray
    {
        void* m_data;
        int m_size;
        int m_capacityAndFlags;
    };

    struct XmlObjectReader_DummyHomogeneousArray
    {
        const hkClass* m_class;
        void* m_data;
        int m_size;
    };

    struct XmlObjectReader_DummyRelArray
    {
        unsigned short m_size;
        unsigned short m_offset;
    };

    // <XmlObjectReader_DummyArray> 0x01056FB0, <hkRelArray<unsigned char>> 0x01056FC0,
    // <XmlObjectReader_DummyHomogeneousArray> 0x01056FF0 (:38)
    template<typename T>
    T& lookupMember(void* start)
    {
        return *static_cast<T*>(start);
    }

    // The object being read, growing at its end
    struct Buffer
    {
        enum Pad
        {
            PAD_NONE = 1,
            PAD_16 = 16,
        };

        // 0x01054B10 (:56)
        Buffer(hkArray<char,hkContainerHeapAllocator>& c)
            : m_buf(&c)
        {
        }

        // 0x01057170 (:62): zeroes room for nbytes (padded) after the end without growing; the offset of the room
        int reserve(int nbytes, Pad pad)
        {
            const int size = m_buf->getSize();
            m_buf->setSize((size + nbytes + pad - 1) & ~(pad - 1), 0);
            m_buf->setSizeUnchecked(size);
            return size;
        }

        // 0x010570B0 (:73)
        void advance(int nbytes, Pad pad)
        {
            m_buf->setSizeUnchecked((m_buf->getSize() + nbytes + pad - 1) & ~(pad - 1));
        }

        // 0x01057000 (:80)
        void* pointerAt(int offset)
        {
            return m_buf->begin() + offset;
        }

        hkArray<char,hkContainerHeapAllocator>* m_buf;
    };

    // 0x01054B20 (:91)
    void* addByteOffset(void* p, int n)
    {
        return static_cast<char*>(p) + n;
    }
}

// 0x01054E80 (:99): the reader's own parser when none is given
hkXmlObjectReader::hkXmlObjectReader(hkXmlParser* parser, hkStringMap<void*,hkContainerHeapAllocator>* nameToObject)
    : m_parser(parser)
    , m_nameToObject(nameToObject)
{
    if (parser != nullptr)
    {
        parser->addReference();
        return;
    }
    m_parser = new hkXmlParser();
}

// 0x01054E10 (:111)
hkXmlObjectReader::~hkXmlObjectReader()
{
    m_parser->removeReference();
}

// 0x01055450 (:116): the text (terminator included) after the object, 16 aligned, a local from the member to it.
// A text holding only a zero character fails. LTCG: reloc in ESI.
static hkResult extractCstring(int memberStartOffset, const hkStringBuf& text, Buffer& buffer, hkRelocationInfo& reloc)
{
    const int nbytes = text.getLength() + 1;
    if (nbytes == 2 && text[0] == 0)
    {
        return HK_FAILURE;
    }
    const int offset = buffer.reserve(nbytes, Buffer::PAD_16);
    hkString::memCpy(buffer.m_buf->begin() + offset, text.cString(), nbytes);
    reloc.addLocal(memberStartOffset, offset);
    buffer.advance(nbytes, Buffer::PAD_16);
    return HK_SUCCESS;
}

// 0x01054B30 (:130)
static hkBool isSpace(int c)
{
    return c == ' ' || c == '\t' || c == '\n' || c == '\r';
}

// 0x01054B50 (:135)
static int getNumElementsInMember(const hkClassMember& member)
{
    const int n = member.getCstyleArraySize();
    return n ? n : 1;
}

// 0x01054EF0 (:144): one value of a plain member type; vectors and matrices by columns (the w of a 3-row column 0,
// a transform's last w 1). LTCG: curp in EAX, is in EDI.
static hkResult readSinglePodMember(hkClassMember::Type mtype, void* curp, hkIstream& is)
{
    float* f = static_cast<float*>(curp);
    switch (mtype)
    {
        case hkClassMember::TYPE_BOOL:
        {
            is >> *static_cast<hkBool*>(curp);
            break;
        }
        case hkClassMember::TYPE_CHAR:
        {
            is.read(curp, 1);
            break;
        }
        case hkClassMember::TYPE_INT8:
        case hkClassMember::TYPE_UINT8:
        {
            int v;
            is >> v;
            *static_cast<char*>(curp) = (char)v;
            break;
        }
        case hkClassMember::TYPE_INT16:
        {
            is >> *static_cast<short*>(curp);
            break;
        }
        case hkClassMember::TYPE_UINT16:
        {
            is >> *static_cast<unsigned short*>(curp);
            break;
        }
        case hkClassMember::TYPE_INT32:
        {
            is >> *static_cast<int*>(curp);
            break;
        }
        case hkClassMember::TYPE_UINT32:
        {
            is >> *static_cast<unsigned int*>(curp);
            break;
        }
        case hkClassMember::TYPE_INT64:
        {
            is >> *static_cast<int64_t*>(curp);
            break;
        }
        case hkClassMember::TYPE_UINT64:
        {
            is >> *static_cast<uint64_t*>(curp);
            break;
        }
        case hkClassMember::TYPE_REAL:
        {
            is >> *f;
            break;
        }
        case hkClassMember::TYPE_VECTOR4:
        case hkClassMember::TYPE_QUATERNION:
        {
            is >> f[0] >> f[1] >> f[2] >> f[3];
            break;
        }
        case hkClassMember::TYPE_MATRIX3:
        case hkClassMember::TYPE_ROTATION:
        {
            hkString::memSet(curp, 0, 0x30);
            is >> f[0] >> f[1] >> f[2];
            is >> f[4] >> f[5] >> f[6];
            is >> f[8] >> f[9] >> f[10];
            break;
        }
        case hkClassMember::TYPE_QSTRANSFORM:
        {
            is >> f[0] >> f[1] >> f[2];
            is >> f[4] >> f[5] >> f[6] >> f[7];
            is >> f[8] >> f[9] >> f[10];
            f[11] = 0.0f;
            f[3] = 0.0f;
            break;
        }
        case hkClassMember::TYPE_MATRIX4:
        {
            is >> f[0] >> f[1] >> f[2] >> f[3];
            is >> f[4] >> f[5] >> f[6] >> f[7];
            is >> f[8] >> f[9] >> f[10] >> f[11];
            is >> f[12] >> f[13] >> f[14] >> f[15];
            break;
        }
        case hkClassMember::TYPE_TRANSFORM:
        {
            is >> f[0] >> f[1] >> f[2];
            is >> f[4] >> f[5] >> f[6];
            is >> f[8] >> f[9] >> f[10];
            is >> f[12] >> f[13] >> f[14];
            f[11] = 0.0f;
            f[7] = 0.0f;
            f[3] = 0.0f;
            f[15] = 1.0f;
            break;
        }
        case hkClassMember::TYPE_ULONG:
        {
            // read as 64 bits, stored as 32
            uint64_t v;
            is >> v;
            *static_cast<unsigned long*>(curp) = (unsigned long)v;
            break;
        }
        case hkClassMember::TYPE_HALF:
        {
            // the float's top 16 bits
            float v;
            is >> v;
            unsigned int bits;
            hkString::memCpy(&bits, &v, sizeof(bits));
            *static_cast<short*>(curp) = (short)(bits >> 16);
            break;
        }
        default:
        {
            char buf[512];
            hkErrStream os(buf, sizeof(buf));
            os << "Class member unknown / unhandled: " << (int)mtype;
            if (hkErrorFwd::messageError(0x19FCA9AD, buf, "Serialize\\Xml\\hkXmlObjectReader.cpp", 280) != 0)
            {
                __debugbreak();
            }
            break;
        }
    }
    return is.isOk() ? HK_SUCCESS : HK_FAILURE;
}

// 0x01055320 (:287): the text node's text into ret (canonicalized with "()," as spaces when asked); true unless it was
// null. An end element is put back (true). Original bug kept: the node is not checked for null. LTCG: parser in EAX,
// reader in ECX; start is unused.
static unsigned int extractText(hkXmlParser& parser, hkStreamReader* reader, hkXmlParser::StartElement* start, bool canonicalize, hkStringBuf& ret)
{
    (void)start;
    hkXmlParser::Node* node = nullptr;
    parser.nextNode(&node, reader);
    if (hkXmlParser::Characters* c = node->asCharacters())
    {
        if (canonicalize)
        {
            c->canonicalize("(),");
        }
        ret = c->text.cString();
        const unsigned int notNull = c->text.cString() != nullptr;
        delete node;
        return notNull;
    }
    if (node->asEnd())
    {
        parser.putBack(node);
    }
    return 1;
}

static bool sameName(const char* a, const char* b)
{
    if (a == nullptr || b == nullptr)
    {
        return a == b;
    }
    return hkString::strCmp(a, b) == 0;
}

// 0x010553D0 (:314): the next node must end start. LTCG: parser in ECX, reader in EDX.
static hkResult consumeEndElement(hkXmlParser& parser, hkStreamReader* reader, hkXmlParser::StartElement* start)
{
    hkResult result = HK_SUCCESS;
    hkXmlParser::Node* node = nullptr;
    parser.nextNode(&node, reader);
    hkXmlParser::EndElement* end = node->asEnd();
    if (end == nullptr || !sameName(end->name.cString(), start->name.cString()))
    {
        result = HK_FAILURE;
    }
    if (node)
    {
        delete node;
    }
    return result;
}

// 0x010557B0 (:330): white space separated values (plain types) or names (pointers; a variant is an object name and
// a class name) after the object; the element count. Each value is read into zeroed room, which is then kept.
// LTCG: member in EAX.
static int loadSimpleArray(const hkClassMember& member, const hkStringBuf& text, Buffer& buffer, hkRelocationInfo& reloc)
{
    int count = 0;
    const hkClassMember::Type type = member.getArrayType();
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
            const int elemSize = member.getArrayMemberSize();
            hkIstream is(text.cString(), text.getLength());
            int offset = buffer.reserve(elemSize, Buffer::PAD_NONE);
            while (readSinglePodMember(type, buffer.pointerAt(offset), is) == HK_SUCCESS)
            {
                count++;
                char separator;
                is >> separator;
                buffer.advance(elemSize, Buffer::PAD_NONE);
                offset = buffer.reserve(elemSize, Buffer::PAD_NONE);
            }
            break;
        }
        case hkClassMember::TYPE_POINTER:
        case hkClassMember::TYPE_FUNCTIONPOINTER:
        case hkClassMember::TYPE_VARIANT:
        {
            const int len = text.getLength();
            const char* s = text.cString();
            int start = 0;
            for (int i = 0; i < len; i++)
            {
                if (isSpace(s[i]))
                {
                    if (start != i)
                    {
                        const int n = i - start;
                        hkLocalArray<char> name(n + 1);
                        name.setSize(n + 1);
                        hkString::memCpy(name.begin(), s + start, n);
                        name[n] = 0;
                        reloc.addImport(buffer.reserve(4, Buffer::PAD_NONE), name.begin());
                        buffer.advance(4, Buffer::PAD_NONE);
                        count++;
                    }
                    start = i + 1;
                }
            }
            if (start != len)
            {
                reloc.addImport(buffer.reserve(4, Buffer::PAD_NONE), s + start);
                buffer.advance(4, Buffer::PAD_NONE);
                count++;
            }
            if (type == hkClassMember::TYPE_VARIANT)
            {
                count /= 2;
            }
            break;
        }
        default:
        {
            break;
        }
    }
    buffer.reserve(0, Buffer::PAD_16);
    buffer.advance(0, Buffer::PAD_16);
    return count;
}

// 0x01054B60 (:441): skips the member's content up to the end element closing it (put back for the caller).
// LTCG: parser in ESI, reader in EDI; klass, member and startElement are unused.
static void skipDataForPartialReflected(const hkClass& klass, const hkClassMember* member, hkXmlParser& parser, hkXmlParser::StartElement* startElement, hkStreamReader* reader)
{
    (void)klass;
    (void)member;
    (void)startElement;
    int depth = 1;
    bool sawText = false;
    hkXmlParser::Node* node = nullptr;
    if (parser.nextNode(&node, reader) != HK_SUCCESS)
    {
        return;
    }
    while (node != nullptr)
    {
        switch (node->type)
        {
            case hkXmlParser::START_ELEMENT:
            {
                depth++;
                break;
            }
            case hkXmlParser::END_ELEMENT:
            {
                if (--depth == 0)
                {
                    parser.putBack(node);
                    return;
                }
                break;
            }
            case hkXmlParser::CHARACTERS:
            {
                if (!sawText)
                {
                    sawText = true;
                }
                break;
            }
            default:
            {
                break;
            }
        }
        delete node;
        node = nullptr;
        if (parser.nextNode(&node, reader) != HK_SUCCESS)
        {
            return;
        }
    }
}

static int readEmbeddedHomogeneousClass(hkXmlParser& parser, hkStreamReader* reader, hkXmlParser::StartElement* startElement, Buffer& localBuffer, Buffer& buffer, hkRelocationInfo& reloc, hkStringMap<void*,hkContainerHeapAllocator>* nameToObject);

// 0x01055B50 (:494): the "<hkparam name="">" members of an object at classStartOffset, up to topElement's end
// element. Unknown members are skipped whole; each member must be followed by its end element.
static hkResult readClassBody(const hkClass& klass, int classStartOffset, Buffer& buffer, hkXmlParser::StartElement* topElement, hkXmlParser& parser, hkStreamReader* reader, hkRelocationInfo& reloc, hkStringMap<void*,hkContainerHeapAllocator>* nameToObject)
{
    hkXmlParser::Node* node;
    if (parser.nextNode(&node, reader) != HK_SUCCESS)
    {
        return HK_FAILURE;
    }
    for (;;)
    {
        hkXmlParser::StartElement* se = node->asStart();
        if (se == nullptr)
        {
            hkXmlParser::EndElement* end = node->asEnd();
            if (end && topElement && sameName(end->name.cString(), topElement->name.cString()))
            {
                delete node;
                return HK_SUCCESS;
            }
            if (node)
            {
                delete node;
            }
        }
        else
        {
            const hkClassMember* member = klass.getMemberByName(se->getAttribute("name", nullptr));
            if (member == nullptr)
            {
                hkXmlParser::Tree tree;
                parser.expandNode(se, tree, reader);
                se->removeReference();
            }
            else
            {
                const hkClassMember::Type type = member->getType();
                const int memberOffset = classStartOffset + member->getOffset();
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
                        hkStringBuf text;
                        extractText(parser, reader, se, true, text);
                        hkIstream is(text.cString(), text.getLength());
                        void* curp = buffer.pointerAt(memberOffset);
                        const int n = getNumElementsInMember(*member);
                        const int elemSize = member->getSizeInBytes() / n;
                        for (int i = 0; i < n; i++)
                        {
                            readSinglePodMember(type, curp, is);
                            curp = addByteOffset(curp, elemSize);
                        }
                        break;
                    }
                    case hkClassMember::TYPE_CSTRING:
                    case hkClassMember::TYPE_STRINGPTR:
                    {
                        hkStringBuf text;
                        if (extractText(parser, reader, se, false, text))
                        {
                            extractCstring(memberOffset, text, buffer, reloc);
                        }
                        break;
                    }
                    case hkClassMember::TYPE_POINTER:
                    case hkClassMember::TYPE_FUNCTIONPOINTER:
                    {
                        hkStringBuf text;
                        if (type == hkClassMember::TYPE_POINTER && member->getSubType() == hkClassMember::TYPE_CHAR)
                        {
                            if (extractText(parser, reader, se, false, text))
                            {
                                extractCstring(memberOffset, text, buffer, reloc);
                            }
                            break;
                        }
                        // the names of a C-style array of pointers, one import each
                        extractText(parser, reader, se, true, text);
                        int count = 0;
                        const int n = getNumElementsInMember(*member);
                        const int elemSize = member->getSizeInBytes() / n;
                        const int len = text.getLength();
                        const char* s = text.cString();
                        int start = 0;
                        int offset = memberOffset;
                        if (n > 0)
                        {
                            for (int i = 0; i < len; i++)
                            {
                                if (isSpace(s[i]))
                                {
                                    if (start != i)
                                    {
                                        const int nameLen = i - start;
                                        hkLocalArray<char> name(nameLen + 1);
                                        name.setSize(nameLen + 1);
                                        hkString::memCpy(name.begin(), s + start, nameLen);
                                        name[nameLen] = 0;
                                        reloc.addImport(offset, name.begin());
                                        offset += elemSize;
                                        count++;
                                    }
                                    start = i + 1;
                                }
                                if (count >= n)
                                {
                                    break;
                                }
                            }
                        }
                        if (start != len && count < n)
                        {
                            reloc.addImport(memberOffset + elemSize * count, s + start);
                        }
                        break;
                    }
                    case hkClassMember::TYPE_ARRAY:
                    case hkClassMember::TYPE_INPLACEARRAY:
                    case hkClassMember::TYPE_SIMPLEARRAY:
                    case hkClassMember::TYPE_RELARRAY:
                    {
                        // Original quirk kept: an array of enums or flags reads nothing and gets a size of -1
                        int numElems = -1;
                        const int dataOffset = buffer.reserve(0, Buffer::PAD_NONE);
                        const hkClassMember::Type subType = member->getSubType();
                        if (subType == hkClassMember::TYPE_STRUCT)
                        {
                            numElems = hkString::atoi(se->getAttribute("numelements", nullptr), 0);
                            const hkClass* sclass = member->getClass();
                            if (sclass)
                            {
                                const int objectSize = sclass->getObjectSize();
                                buffer.reserve(objectSize * numElems, Buffer::PAD_16);
                                buffer.advance(objectSize * numElems, Buffer::PAD_16);
                                int offset = dataOffset;
                                for (int i = numElems; i > 0; i--)
                                {
                                    hkXmlParser::Node* child = nullptr;
                                    parser.nextNode(&child, reader);
                                    readClassBody(*sclass, offset, buffer, child->asStart(), parser, reader, reloc, nameToObject);
                                    if (child)
                                    {
                                        delete child;
                                    }
                                    offset += objectSize;
                                }
                            }
                            else
                            {
                                skipDataForPartialReflected(klass, member, parser, se, reader);
                            }
                        }
                        else if (subType == hkClassMember::TYPE_ENUM || subType == hkClassMember::TYPE_FLAGS)
                        {
                        }
                        else if (member->getArrayType() == hkClassMember::TYPE_CSTRING || member->getArrayType() == hkClassMember::TYPE_STRINGPTR)
                        {
                            // "<hkcstring>text</hkcstring>" items
                            numElems = hkString::atoi(se->getAttribute("numelements", nullptr), 0);
                            buffer.reserve(numElems * 4, Buffer::PAD_16);
                            buffer.advance(numElems * 4, Buffer::PAD_16);
                            int offset = dataOffset;
                            for (int i = numElems; i > 0; i--)
                            {
                                hkXmlParser::Node* child = nullptr;
                                parser.nextNode(&child, reader);
                                hkStringBuf text;
                                if (extractText(parser, reader, nullptr, false, text))
                                {
                                    extractCstring(offset, text, buffer, reloc);
                                }
                                if (child)
                                {
                                    delete child;
                                }
                                parser.nextNode(&child, reader);
                                if (child)
                                {
                                    delete child;
                                }
                                offset += 4;
                            }
                        }
                        else if (subType == hkClassMember::TYPE_VOID)
                        {
                            skipDataForPartialReflected(klass, member, parser, se, reader);
                            numElems = 0;
                        }
                        else
                        {
                            hkStringBuf text;
                            extractText(parser, reader, se, subType != hkClassMember::TYPE_STRUCT, text);
                            numElems = loadSimpleArray(*member, text, buffer, reloc);
                        }
                        if (type != hkClassMember::TYPE_RELARRAY)
                        {
                            XmlObjectReader_DummyArray& arr = lookupMember<XmlObjectReader_DummyArray>(buffer.pointerAt(memberOffset));
                            arr.m_data = nullptr;
                            arr.m_size = numElems;
                            if (numElems > 0)
                            {
                                reloc.addLocal(memberOffset, dataOffset);
                            }
                            if (type != hkClassMember::TYPE_SIMPLEARRAY)
                            {
                                arr.m_capacityAndFlags = numElems | 0x80000000; // DONT_DEALLOCATE
                            }
                        }
                        else
                        {
                            XmlObjectReader_DummyRelArray& rel = lookupMember<XmlObjectReader_DummyRelArray>(buffer.pointerAt(memberOffset));
                            rel.m_size = (unsigned short)numElems;
                            rel.m_offset = 0;
                            if (numElems > 0)
                            {
                                rel.m_offset = (unsigned short)(dataOffset - memberOffset);
                            }
                        }
                        break;
                    }
                    case hkClassMember::TYPE_ENUM:
                    {
                        hkStringBuf text;
                        extractText(parser, reader, se, false, text);
                        if (text.getLength() != 0)
                        {
                            const hkClassEnum& e = member->getEnumType();
                            int value = 0;
                            e.getValueOfName(text.cString(), &value);
                            member->setEnumValue(buffer.pointerAt(memberOffset), value);
                        }
                        break;
                    }
                    case hkClassMember::TYPE_FLAGS:
                    {
                        // '|' separated names or numbers
                        hkStringBuf text;
                        extractText(parser, reader, se, false, text);
                        if (text.getLength() != 0)
                        {
                            const hkClassEnum& e = member->getEnumType();
                            hkArray<char,hkContainerHeapAllocator> copy;
                            const int size = text.getLength() + 1;
                            copy.reserve(size);
                            hkString::strNcpy(copy.begin(), text.cString(), size);
                            copy.setSizeUnchecked(size);
                            int value = 0;
                            for (char* p = copy.begin(); p != nullptr;)
                            {
                                char* next = nullptr;
                                if (char* bar = const_cast<char*>(hkString::strChr(p, '|')))
                                {
                                    *bar = 0;
                                    next = bar + 1;
                                }
                                if (p[0] >= '0' && p[0] <= '9')
                                {
                                    value |= hkString::atoi(p, 0);
                                }
                                else
                                {
                                    int v = 0;
                                    if (e.getValueOfName(p, &v) == HK_SUCCESS)
                                    {
                                        value |= v;
                                    }
                                }
                                p = next;
                            }
                            member->setEnumValue(buffer.pointerAt(memberOffset), value);
                        }
                        break;
                    }
                    case hkClassMember::TYPE_STRUCT:
                    {
                        // Original bug kept: a struct element whose class is unknown is skipped and its node leaks
                        for (int i = 0; i < getNumElementsInMember(*member); i++)
                        {
                            hkXmlParser::Node* child = nullptr;
                            parser.nextNode(&child, reader);
                            hkXmlParser::StartElement* cs = child->asStart();
                            if (cs == nullptr)
                            {
                                parser.putBack(child);
                                break;
                            }
                            if (member->getClass())
                            {
                                const hkClass& sclass = member->getStructClass();
                                readClassBody(sclass, memberOffset + sclass.getObjectSize() * i, buffer, cs, parser, reader, reloc, nameToObject);
                                delete child;
                            }
                            else
                            {
                                skipDataForPartialReflected(klass, member, parser, cs, reader);
                            }
                        }
                        break;
                    }
                    case hkClassMember::TYPE_HOMOGENEOUSARRAY:
                    {
                        // the class embedded ("<hkobject>", read into a local buffer and copied after the object) or
                        // by name, then numelements objects of it
                        const int numElems = hkString::atoi(se->getAttribute("numelements", nullptr), 0);
                        hkXmlParser::Node* peek = nullptr;
                        parser.nextNode(&peek, reader);
                        parser.putBack(peek);
                        if (peek->type == hkXmlParser::END_ELEMENT)
                        {
                            break;
                        }
                        hkLocalArray<char> local(0x400);
                        Buffer localBuffer(local);
                        // Original bug kept: for another node type the class is never set (an uninitialised local in
                        // rfg.exe, null here) and is then dereferenced
                        const hkClass* hclass = nullptr;
                        if (peek->type == hkXmlParser::START_ELEMENT)
                        {
                            const int classOffset = readEmbeddedHomogeneousClass(parser, reader, se, localBuffer, buffer, reloc, nameToObject);
                            hclass = reinterpret_cast<const hkClass*>(local.begin());
                            reloc.addLocal(memberOffset, classOffset);
                        }
                        else if (peek->type == hkXmlParser::CHARACTERS)
                        {
                            hkStringBuf name;
                            extractText(parser, reader, se, true, name);
                            hclass = static_cast<const hkClass*>(nameToObject->getWithDefault(name.cString(), nullptr));
                            reloc.addImport(memberOffset, name.cString());
                        }
                        const int objectSize = hclass->getObjectSize();
                        const int dataOffset = buffer.reserve(0, Buffer::PAD_NONE);
                        buffer.reserve(objectSize * numElems, Buffer::PAD_16);
                        buffer.advance(objectSize * numElems, Buffer::PAD_16);
                        int offset = dataOffset;
                        for (int i = numElems; i > 0; i--)
                        {
                            hkXmlParser::Node* child = nullptr;
                            parser.nextNode(&child, reader);
                            readClassBody(*hclass, offset, buffer, child->asStart(), parser, reader, reloc, nameToObject);
                            if (child)
                            {
                                delete child;
                            }
                            offset += objectSize;
                        }
                        XmlObjectReader_DummyHomogeneousArray& harr = lookupMember<XmlObjectReader_DummyHomogeneousArray>(buffer.pointerAt(memberOffset));
                        harr.m_class = nullptr;
                        harr.m_data = nullptr;
                        harr.m_size = numElems;
                        if (numElems > 0)
                        {
                            reloc.addLocal(memberOffset + 4, dataOffset);
                        }
                        break;
                    }
                    case hkClassMember::TYPE_VARIANT:
                    {
                        // "object class": two imports
                        hkStringBuf text;
                        extractText(parser, reader, se, true, text);
                        const int space = text.indexOf(' ', 0, 0x7FFFFFFF);
                        if (space != -1)
                        {
                            hkStringBuf part(text);
                            part.slice(0, space);
                            reloc.addImport(memberOffset, part.cString());
                            part = text;
                            part.chompStart(space + 1);
                            reloc.addImport(memberOffset + 4, part.cString());
                        }
                        break;
                    }
                    default:
                    {
                        break;
                    }
                }
                if (consumeEndElement(parser, reader, se) == HK_FAILURE)
                {
                    if (node)
                    {
                        delete node;
                    }
                    return HK_FAILURE;
                }
                if (node)
                {
                    delete node;
                }
            }
        }
        if (parser.nextNode(&node, reader) != HK_SUCCESS)
        {
            return HK_FAILURE;
        }
    }
}

// 0x010554F0 (:941): an embedded hkClass read into localBuffer (at offset 0) with its own relocations, copied after
// the object in buffer with the relocations moved to it; the copy's offset. The local relocations are then applied
// to the local copy, which the caller reads the class from. LTCG: parser in EAX; startElement is unused.
static int readEmbeddedHomogeneousClass(hkXmlParser& parser, hkStreamReader* reader, hkXmlParser::StartElement* startElement, Buffer& localBuffer, Buffer& buffer, hkRelocationInfo& reloc, hkStringMap<void*,hkContainerHeapAllocator>* nameToObject)
{
    (void)startElement;
    hkXmlParser::Node* node = nullptr;
    parser.nextNode(&node, reader);
    hkXmlParser::StartElement* start = node->asStart();
    hkRelocationInfo localReloc;
    localBuffer.reserve(hkClassClass.getObjectSize(), Buffer::PAD_16);
    localBuffer.advance(hkClassClass.getObjectSize(), Buffer::PAD_16);
    readClassBody(hkClassClass, 0, localBuffer, start, parser, reader, localReloc, nameToObject);
    const int size = localBuffer.reserve(0, Buffer::PAD_16);
    const int offset = buffer.reserve(size, Buffer::PAD_16);
    buffer.advance(size, Buffer::PAD_16);
    hkString::memCpy(buffer.m_buf->begin() + offset, localBuffer.m_buf->begin(), size);
    for (int i = 0; i < localReloc.m_local.getSize(); i++)
    {
        const hkRelocationInfo::Local& l = localReloc.m_local[i];
        reloc.m_local.pushBack(hkRelocationInfo::Local(l.m_fromOffset + offset, l.m_toOffset + offset));
    }
    for (int i = 0; i < localReloc.m_global.getSize(); i++)
    {
        const hkRelocationInfo::Global& g = localReloc.m_global[i];
        reloc.m_global.pushBack(hkRelocationInfo::Global(g.m_fromOffset + offset, g.m_toAddress, g.m_toClass, g.m_related));
    }
    for (int i = 0; i < localReloc.m_finish.getSize(); i++)
    {
        const hkRelocationInfo::Finish& fin = localReloc.m_finish[i];
        reloc.m_finish.pushBack(hkRelocationInfo::Finish(fin.m_fromOffset + offset, fin.m_className));
    }
    for (int i = 0; i < localReloc.m_imports.getSize(); i++)
    {
        const hkRelocationInfo::Import& imp = localReloc.m_imports[i];
        reloc.addImport(imp.m_fromOffset + offset, imp.m_identifier);
    }
    localReloc.applyLocalAndGlobal(localBuffer.m_buf->begin());
    if (node)
    {
        delete node;
    }
    return offset;
}

// 0x01056CD0 (:1025): an "<hkobject>" block into array (16 aligned at its end), the class registered for its finish
// step. Anything else fails.
hkResult hkXmlObjectReader::readObject(hkStreamReader* reader, hkArray<char,hkContainerHeapAllocator>& array, const hkClass& klass, hkRelocationInfo& reloc)
{
    hkResult result = HK_FAILURE;
    Buffer buffer(array);
    hkXmlParser::Node* node;
    if (m_parser->nextNode(&node, reader) != HK_SUCCESS)
    {
        return result;
    }
    if (hkXmlParser::StartElement* start = node->asStart())
    {
        if (start->name == "hkobject")
        {
            const int offset = buffer.reserve(klass.getObjectSize(), Buffer::PAD_16);
            buffer.advance(klass.getObjectSize(), Buffer::PAD_16);
            reloc.m_finish.pushBack(hkRelocationInfo::Finish(offset, klass.getName()));
            result = readClassBody(klass, offset, buffer, start, *m_parser, reader, reloc, m_nameToObject);
        }
    }
    else if (hkXmlParser::Characters* c = node->asCharacters())
    {
        c->canonicalize(nullptr);
    }
    else if (!node->asEnd())
    {
        char buf[512];
        hkErrStream os(buf, sizeof(buf));
        os << "Unknown element type returned from XML parser.";
        if (hkErrorFwd::messageError(0x6A858EC3, buf, "Serialize\\Xml\\hkXmlObjectReader.cpp", 1060) != 0)
        {
            __debugbreak();
        }
    }
    if (node)
    {
        delete node;
    }
    return result;
}

// 0x01056E80 (:1006): the object's bytes, or -1 when it fails or does not fit
int hkXmlObjectReader::readObject(hkStreamReader* reader, void* buf, int bufSize, const hkClass& klass, hkRelocationInfo& reloc)
{
    hkArray<char,hkContainerHeapAllocator> array;
    if (bufSize > 0)
    {
        array.reserve(bufSize);
    }
    if (readObject(reader, array, klass, reloc) == HK_SUCCESS && array.getSize() <= bufSize)
    {
        hkString::memCpy(buf, array.begin(), array.getSize());
        return array.getSize();
    }
    return -1;
}

// 0x01054C00 (:1069): -1 at the end
static int readChar(hkStreamReader* r)
{
    char c;
    return r->read(&c, 1) == 1 ? (int)c : -1;
}

// 0x01054C20 (:1079): skips white space and then reads as many characters as marker has, without comparing them.
// LTCG: reader in EDI.
static void eatMarker(hkStreamReader* reader, const char* marker)
{
    int c;
    do
    {
        c = readChar(reader);
    } while (c == ' ' || c == '\t' || c == '\n' || c == '\r');
    for (const char* p = marker + 1; *p != 0; p++)
    {
        readChar(reader);
    }
}

// 0x01054E30 (:1103): "<![CDATA[" base64 "]]>"
hkResult hkXmlObjectReader::readRaw(hkStreamReader* reader, void* buf, int bufLen)
{
    eatMarker(reader, "<![CDATA[");
    const hkResult res = base64read(reader, buf, bufLen);
    if (res == HK_SUCCESS)
    {
        eatMarker(reader, "]]>");
    }
    return res;
}

// 0x014C5770: base64 character values, 0xFF for the others
static const unsigned char base64read_ascii2bin[128] =
{
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 62, 0xFF, 0xFF, 0xFF, 63,
    52, 53, 54, 55, 56, 57, 58, 59, 60, 61, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
    0xFF, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14,
    15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
    0xFF, 26, 27, 28, 29, 30, 31, 32, 33, 34, 35, 36, 37, 38, 39, 40,
    41, 42, 43, 44, 45, 46, 47, 48, 49, 50, 51, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
};

// 0x01054C60 (:1125): len bytes from base64 text (characters outside the alphabet skipped), then the '=' padding.
// Original quirk kept: the character after the data is consumed whether or not it is '='. Deviation: rfg.exe checks
// the character masked to 7 bits but reads its value unmasked (past the table for bytes >= 0x80); the port masks.
hkResult hkXmlObjectReader::base64read(hkStreamReader* sr, void* buf, int len)
{
    static const int charsForRemainder[3] = { 0, 2, 3 };
    static const int bytesForChars[4] = { 0, 0, 1, 2 };
    unsigned char* out = static_cast<unsigned char*>(buf);
    int charsLeft = (len / 3) * 4 + charsForRemainder[len % 3];
    unsigned char quad[4] = { 0, 0, 0, 0 };
    int n = 0;
    while (charsLeft > 0)
    {
        unsigned char c;
        if (sr->read(&c, 1) != 1)
        {
            return HK_FAILURE;
        }
        if (base64read_ascii2bin[c & 0x7F] == 0xFF)
        {
            continue;
        }
        quad[n++] = base64read_ascii2bin[c & 0x7F];
        charsLeft--;
        if (n == 4)
        {
            out[0] = (unsigned char)((quad[0] << 2) | (quad[1] >> 4));
            out[1] = (unsigned char)((quad[1] << 4) | (quad[2] >> 2));
            out[2] = (unsigned char)((quad[2] << 6) | quad[3]);
            out += 3;
            n = 0;
            quad[0] = quad[1] = quad[2] = quad[3] = 0;
        }
    }
    unsigned char c = 0;
    while (sr->peek(&c, 1) == 1)
    {
        sr->skip(1);
        if (c != '=')
        {
            break;
        }
    }
    if (n != 0)
    {
        unsigned char tail[3];
        tail[0] = (unsigned char)((quad[0] << 2) | (quad[1] >> 4));
        tail[1] = (unsigned char)((quad[1] << 4) | (quad[2] >> 2));
        tail[2] = (unsigned char)((quad[2] << 6) | quad[3]);
        const int count = bytesForChars[n];
        if (count > 0)
        {
            hkString::memCpy(out, tail, count); // a CRT memcpy in rfg.exe
        }
    }
    return HK_SUCCESS;
}
