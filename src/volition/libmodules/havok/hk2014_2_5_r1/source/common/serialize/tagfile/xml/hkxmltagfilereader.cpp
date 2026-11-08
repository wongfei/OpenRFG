// Original: volition/libmodules/havok/hk2014_2_5_r1/source/common/serialize/tagfile/xml/hkxmltagfilereader.cpp
//
// Havok 2014.2.5 (stub_impl.md S4, serializer SF): the XML tagfile reader, from rfg.exe. A "<hktagfile>" block of
// "<class>" definitions and "<object>" blocks is read into a data world over hkXmlStreamParser; references
// ("<ref>#id</ref>") are resolved once every object is read. The private types are rfg.pdb's layouts.

#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/serialize/tagfile/xml/hkxmltagfilereader.h"

#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/container/string/hkstring.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/container/string/hkstringbuf.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/container/stringmap/hkstoragestringmap.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/container/stringmap/hkstringmap.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/system/error/hkerror.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/system/io/ostream/hkostream.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/serialize/data/hkdataobject.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/serialize/data/hkdataobjectimpl.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/serialize/typemanager/hktypemanager.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/serialize/util/xml/hkxmlstreamparser.h"

#include <new>

namespace
{
    // size 0x1C: an object id, the object once read, and the values that reference it
    struct ReferenceInfo
    {
        HK_DECLARE_NONVIRTUAL_CLASS_ALLOCATOR(ReferenceInfo); // new 0x00F56080, delete 0x00F560A0 (:20)

        // 0x00F56E10 (:22)
        ReferenceInfo()
            : m_object(nullptr)
        {
        }

        // 0x00F56B80 (:25): copied in place (the value handles' operator= writes through into the world)
        void remember(hkDataObject_Value value)
        {
            new (&m_objectRefs.expandOne()) hkDataObject_Value(value);
        }

        // 0x00F56BD0 (:29)
        void remember(hkDataArray_Value value)
        {
            new (&m_arrayRefs.expandOne()) hkDataArray_Value(value);
        }

        // 0x00F560D0 (:33)
        void assign()
        {
            for (int i = 0; i < m_arrayRefs.getSize(); i++)
            {
                m_arrayRefs[i] = hkDataObject(m_object);
            }
            for (int i = 0; i < m_objectRefs.getSize(); i++)
            {
                m_objectRefs[i] = m_object;
            }
        }

        hkDataObjectImpl* m_object; // 0x00
        hkArray<hkDataArray_Value,hkContainerHeapAllocator> m_arrayRefs; // 0x04
        hkArray<hkDataObject_Value,hkContainerHeapAllocator> m_objectRefs; // 0x10
    };

    // size 0xD4
    struct Reader
    {
        typedef hkXmlStreamParser::Token Token;

        Reader(hkStreamReader* sr, hkDataWorld* cont);
        ~Reader();
        hkResult parseRoot(hkDataObject& objOut);
        hkResult readHeader(hkXmlTagfile::Header& out);

    protected:
        void _pushBlock();
        hkResult _popBlock();
        const char* _getString(const hkSubString& text);
        const char* _getAttribute(const char* key);
        Token _skipWhiteSpace();
        hkBool _getBoolAttribute(const char* key, hkBool defaultValue);
        hkResult _parseClassDefinition();
        hkTypeManager::Type* _getType(const char* name, const char* className);
        hkResult _parseMemberValue(const hkDataClass_MemberInfo& minfo, hkDataObject_Value& valueOut);
        hkResult _parseInt(const char* blockName, int64_t& valueOut);
        hkResult _parseReal(const char* blockName, float& valueOut);
        hkResult _parseArrayItems(int numElems, hkDataArray& arr);
        hkResult _parseRealVector(float* out, int numReal);
        hkResult _parseString(const char*& stringOut);
        template<typename VALUE>
        hkResult _parseObjectReferenceImpl(VALUE& valueOut);
        hkResult _parseObjectReference(hkDataObject_Value& valueOut);
        hkResult _parseObjectReference(hkDataArray_Value& valueOut);
        hkResult _parseAndCreateObject(const hkDataClassImpl* klassImpl, hkDataObject& objOut);
        hkResult _parseObject(hkDataObject& obj);
        hkResult _parseIntArray(const char* blockName, hkDataArray& arr);
        hkResult _parseRealArray(const char* blockName, hkDataArray& arr);
        ReferenceInfo* _findReferenceInfo(const char* key);
        hkResult _setReferencedObject(const char* id, hkDataObject& obj);
        ReferenceInfo* _requireReferenceInfo(const char* key);
        hkResult _fixUpReferences();

        hkXmlStreamParser m_parser; // 0x00
        hkDataWorld* m_world; // 0x78
        hkArray<hkDataClassImpl*,hkContainerHeapAllocator> m_classes; // 0x7C
        hkArray<char*,hkContainerHeapAllocator> m_prevStrings; // 0x88
        int m_numPrevStringsStatic; // 0x94
        hkStorageStringMap<int,hkContainerHeapAllocator> m_stringMap; // 0x98
        hkArray<const char*,hkContainerHeapAllocator> m_blockStack; // 0xA8
        hkStringMap<ReferenceInfo*,hkContainerHeapAllocator> m_referenceMap; // 0xB4
        hkArray<hkDataObject,hkContainerHeapAllocator> m_tmpObjects; // 0xC4
        const char* m_null; // 0xD0
    };

    // 0x00F57240 (:88)
    hkResult Reader::_parseObjectReference(hkDataArray_Value& valueOut)
    {
        return _parseObjectReferenceImpl(valueOut);
    }

    // 0x00F57260 (:89)
    hkResult Reader::_parseObjectReference(hkDataObject_Value& valueOut)
    {
        return _parseObjectReferenceImpl(valueOut);
    }

    // 0x00F55470 (:131): class 0 and the first two strings ("" and null) are reserved; "#0000" is the null id
    Reader::Reader(hkStreamReader* sr, hkDataWorld* cont)
        : m_parser(sr)
        , m_world(cont)
        , m_numPrevStringsStatic(0)
    {
        m_classes.pushBack(nullptr);
        m_prevStrings.pushBack(const_cast<char*>(""));
        m_prevStrings.pushBack(nullptr);
        m_numPrevStringsStatic = 2;
        m_null = _getString(hkSubString("#0000"));
    }

    // 0x00F555E0 (:142): this port's hkArray does not destroy its elements, so the objects are released here (last
    // first, as rfg.exe does)
    Reader::~Reader()
    {
        for (hkStringMap<ReferenceInfo*,hkContainerHeapAllocator>::Iterator it = m_referenceMap.getIterator(); m_referenceMap.isValid(it); it = m_referenceMap.getNext(it))
        {
            delete m_referenceMap.getValue(it);
        }
        for (int i = m_tmpObjects.getSize() - 1; i >= 0; i--)
        {
            m_tmpObjects[i].~hkDataObject();
        }
    }

    // <hkDataArray_Value> 0x00F56F70, <hkDataObject_Value> 0x00F570A0 (:153): "<ref>#id</ref>" (remembered until the
    // fix-up; the null id sets null) or "<null/>"
    template<typename VALUE>
    hkResult Reader::_parseObjectReferenceImpl(VALUE& valueOut)
    {
        _skipWhiteSpace();
        const Token tok = m_parser.getToken();
        if (tok == hkXmlStreamParser::TOKEN_BLOCK_START)
        {
            if (m_parser.getBlockName() == "ref")
            {
                _pushBlock();
                m_parser.advance();
                if (_skipWhiteSpace() == hkXmlStreamParser::TOKEN_TEXT)
                {
                    const char* id = _getString(m_parser.getLexeme());
                    if (id == m_null)
                    {
                        valueOut.setNull();
                    }
                    else
                    {
                        _requireReferenceInfo(id)->remember(valueOut);
                    }
                    m_parser.advance();
                    _skipWhiteSpace();
                    return _popBlock();
                }
            }
        }
        else if (tok == hkXmlStreamParser::TOKEN_BLOCK_START_END)
        {
            if (m_parser.getBlockName() == "null")
            {
                valueOut.setNull();
                return HK_SUCCESS;
            }
        }
        return HK_FAILURE;
    }

    // 0x00F53E70 (:200): interned in m_stringMap (equal strings compare equal as pointers)
    const char* Reader::_getString(const hkSubString& text)
    {
        const int n = text.length();
        hkInplaceArray<char,128,hkContainerHeapAllocator> buf;
        buf.setSize(n + 1);
        hkString::strNcpy(buf.begin(), text.m_start, n);
        buf[n] = 0;
        return m_stringMap.insert(buf.begin(), 1);
    }

    // 0x00F53950 (:213): byte, int, real, string, void, vecN, ref and struct (both of className)
    hkTypeManager::Type* Reader::_getType(const char* name, const char* className)
    {
        hkTypeManager& typeManager = m_world->getTypeManager();
        switch (name[0])
        {
            case 'b':
            {
                if (hkString::strCmp(name, "byte") == 0)
                {
                    return typeManager.getSubType(hkTypeManager::SUB_TYPE_BYTE);
                }
                break;
            }
            case 'i':
            {
                if (hkString::strCmp(name, "int") == 0)
                {
                    return typeManager.getSubType(hkTypeManager::SUB_TYPE_INT);
                }
                break;
            }
            case 'r':
            {
                if (hkString::strCmp(name, "real") == 0)
                {
                    return typeManager.getSubType(hkTypeManager::SUB_TYPE_REAL);
                }
                if (hkString::strCmp(name, "ref") == 0)
                {
                    return typeManager.makePointer(typeManager.addClass(className));
                }
                break;
            }
            case 's':
            {
                if (hkString::strCmp(name, "string") == 0)
                {
                    return typeManager.getSubType(hkTypeManager::SUB_TYPE_CSTRING);
                }
                if (hkString::strCmp(name, "struct") == 0)
                {
                    return typeManager.addClass(className);
                }
                break;
            }
            case 'v':
            {
                if (hkString::strCmp(name, "void") == 0)
                {
                    return typeManager.getSubType(hkTypeManager::SUB_TYPE_VOID);
                }
                if (hkString::strNcmp(name, "vec", 3) == 0)
                {
                    const char* digits = name + 3;
                    const char* p = digits;
                    while (*p >= '0' && *p <= '9')
                    {
                        p++;
                    }
                    if (*p != 0)
                    {
                        return nullptr;
                    }
                    return typeManager.makeTuple(typeManager.getSubType(hkTypeManager::SUB_TYPE_REAL), hkString::atoi(digits, 0));
                }
                break;
            }
        }
        return nullptr;
    }

    // 0x00F53FA0 (:284)
    void Reader::_pushBlock()
    {
        m_blockStack.pushBack(_getString(m_parser.getBlockName()));
    }

    // 0x00F54000 (:290): the current block end must close the innermost block
    hkResult Reader::_popBlock()
    {
        _skipWhiteSpace();
        const char* name = _getString(m_parser.getBlockName());
        if (m_blockStack.getSize() > 0 && name == m_blockStack.back())
        {
            m_parser.advance();
            m_blockStack.popBack();
            return HK_SUCCESS;
        }
        return HK_FAILURE;
    }

    // 0x00F545A0 (:314): '<class name="" version="" parent="">' with '<member name="" type="" class="" array=""
    // count=""/>' entries. A member of an unknown type ends the class with a failure. Original quirk kept: after the
    // closing tag one more token is skipped.
    hkResult Reader::_parseClassDefinition()
    {
        _pushBlock();
        hkDataClass::Cinfo cinfo;
        cinfo.name = _getAttribute("name");
        if (cinfo.name == nullptr)
        {
            return HK_FAILURE;
        }
        int version = 0;
        m_parser.getIntAttribute("version", version);
        cinfo.parent = _getAttribute("parent");
        cinfo.version = version;
        hkTypeManager& typeManager = m_world->getTypeManager();
        m_parser.advance();
        if (_skipWhiteSpace() == hkXmlStreamParser::TOKEN_BLOCK_START_END)
        {
            while (m_parser.getBlockName() == "member")
            {
                const char* memberName = _getAttribute("name");
                const char* typeName = _getAttribute("type");
                const char* className = _getAttribute("class");
                if (memberName == nullptr || typeName == nullptr)
                {
                    return HK_FAILURE;
                }
                hkTypeManager::Type* type = _getType(typeName, className);
                if (type == nullptr)
                {
                    return HK_FAILURE;
                }
                if (_getBoolAttribute("array", false))
                {
                    type = typeManager.makeArray(type);
                }
                int count = 0;
                m_parser.getIntAttribute("count", count);
                if (count > 0)
                {
                    type = typeManager.makeTuple(type, count);
                }
                hkDataClass::Cinfo::Member& m = cinfo.members.expandOne();
                m.name = memberName;
                m.type = type;
                m.valuePtr = nullptr;
                if (className != nullptr)
                {
                    m_world->findClass(className); // result unused (an assert in the SDK)
                }
                m_parser.advance();
                if (_skipWhiteSpace() != hkXmlStreamParser::TOKEN_BLOCK_START_END)
                {
                    break;
                }
            }
        }
        if (_popBlock() == HK_FAILURE)
        {
            return HK_FAILURE;
        }
        m_parser.advance();
        m_world->newClass(cinfo);
        return HK_SUCCESS;
    }

    // 0x00F54070 (:410): "<blockName>N</blockName>"
    hkResult Reader::_parseInt(const char* blockName, int64_t& valueOut)
    {
        if (m_parser.getToken() == hkXmlStreamParser::TOKEN_BLOCK_START && m_parser.getBlockName() == blockName)
        {
            _pushBlock();
            m_parser.advance();
            if (_skipWhiteSpace() == hkXmlStreamParser::TOKEN_TEXT)
            {
                const hkResult res = hkXmlStreamParser::parseInt(m_parser.getLexeme(), valueOut);
                if (res != HK_SUCCESS)
                {
                    return res;
                }
                m_parser.advance();
                return _popBlock();
            }
        }
        return HK_FAILURE;
    }

    // 0x00F54120 (:437): "<blockName>R</blockName>"
    hkResult Reader::_parseReal(const char* blockName, float& valueOut)
    {
        if (m_parser.getToken() == hkXmlStreamParser::TOKEN_BLOCK_START && m_parser.getBlockName() == blockName)
        {
            _pushBlock();
            m_parser.advance();
            if (_skipWhiteSpace() == hkXmlStreamParser::TOKEN_TEXT)
            {
                const hkResult res = hkXmlStreamParser::parseReal(m_parser.getLexeme(), valueOut);
                if (res != HK_SUCCESS)
                {
                    return res;
                }
                m_parser.advance();
                return _popBlock();
            }
        }
        return HK_FAILURE;
    }

    // 0x00F541D0 (:488): numReal white space separated reals in the current block
    hkResult Reader::_parseRealVector(float* out, int numReal)
    {
        _pushBlock();
        m_parser.advance();
        for (int i = 0; i < numReal; i++)
        {
            if (_skipWhiteSpace() != hkXmlStreamParser::TOKEN_TEXT)
            {
                return HK_FAILURE;
            }
            const hkResult res = hkXmlStreamParser::parseReal(m_parser.getLexeme(), out[i]);
            if (res != HK_SUCCESS)
            {
                return res;
            }
            m_parser.advance();
        }
        return _popBlock();
    }

    // 0x00F547F0 (:516): "<null/>", "<string/>" or "<string>text</string>" (entities decoded, the result interned)
    hkResult Reader::_parseString(const char*& stringOut)
    {
        stringOut = nullptr;
        const hkSubString blockName = m_parser.getBlockName();
        if (blockName == "null")
        {
            m_parser.advance();
            return HK_SUCCESS;
        }
        if (!(blockName == "string"))
        {
            return HK_FAILURE;
        }
        const Token tok = m_parser.getToken();
        if (tok == hkXmlStreamParser::TOKEN_BLOCK_START_END)
        {
            m_parser.advance();
            stringOut = "";
            return HK_SUCCESS;
        }
        if (tok != hkXmlStreamParser::TOKEN_BLOCK_START)
        {
            return HK_FAILURE;
        }
        _pushBlock();
        Token t = m_parser.advance();
        if (t == hkXmlStreamParser::TOKEN_BLOCK_END)
        {
            stringOut = "";
            return _popBlock();
        }
        hkStringBuf buf;
        while (t == hkXmlStreamParser::TOKEN_TEXT || t == hkXmlStreamParser::TOKEN_WHITESPACE)
        {
            const hkSubString lexeme = m_parser.getLexeme();
            buf.append(lexeme.m_start, lexeme.length());
            t = m_parser.advance();
        }
        const hkSubString text(buf.cString(), buf.getLength());
        if (hkXmlStreamParser::needsDecode(text))
        {
            hkStringBuf decoded;
            hkXmlStreamParser::decodeString(text, decoded); // result not checked
            buf = decoded;
        }
        stringOut = m_stringMap.insert(buf.cString(), 1);
        return _popBlock();
    }

    // 0x00F54270 (:576): "<blockName>N</blockName>" items or bare numbers, one per array element
    hkResult Reader::_parseIntArray(const char* blockName, hkDataArray& arr)
    {
        const int n = arr.getSize();
        for (int i = 0; i < n; i++)
        {
            _skipWhiteSpace();
            const Token tok = m_parser.getToken();
            if (tok == hkXmlStreamParser::TOKEN_BLOCK_START)
            {
                int64_t v;
                const hkResult res = _parseInt(blockName, v);
                if (res != HK_SUCCESS)
                {
                    return res;
                }
                arr[i] = v;
            }
            else if (tok == hkXmlStreamParser::TOKEN_TEXT)
            {
                int64_t v;
                const hkResult res = hkXmlStreamParser::parseInt(m_parser.getLexeme(), v);
                if (res != HK_SUCCESS)
                {
                    return res;
                }
                arr[i] = v;
                m_parser.advance();
            }
            else
            {
                return HK_FAILURE;
            }
        }
        return HK_SUCCESS;
    }

    // 0x00F54370 (:618)
    hkResult Reader::_parseRealArray(const char* blockName, hkDataArray& arr)
    {
        const int n = arr.getSize();
        for (int i = 0; i < n; i++)
        {
            _skipWhiteSpace();
            const Token tok = m_parser.getToken();
            if (tok == hkXmlStreamParser::TOKEN_BLOCK_START)
            {
                float v;
                const hkResult res = _parseReal(blockName, v);
                if (res != HK_SUCCESS)
                {
                    return res;
                }
                arr[i] = v;
            }
            else if (tok == hkXmlStreamParser::TOKEN_TEXT)
            {
                float v;
                const hkResult res = hkXmlStreamParser::parseReal(m_parser.getLexeme(), v);
                if (res != HK_SUCCESS)
                {
                    return res;
                }
                arr[i] = v;
                m_parser.advance();
            }
            else
            {
                return HK_FAILURE;
            }
        }
        return HK_SUCCESS;
    }

    // 0x00F54A60 (:657): the array's elements by its element type. Original quirks kept: a reference that fails to
    // parse is not reported, and a tuple element type other than vecN reads nothing and succeeds.
    hkResult Reader::_parseArrayItems(int numElems, hkDataArray& arr)
    {
        hkTypeManager::Type* type = arr.getType();
        switch (type->getSubType())
        {
            case hkTypeManager::SUB_TYPE_BYTE:
            case hkTypeManager::SUB_TYPE_INT:
            {
                return _parseIntArray(type->getSubType() == hkTypeManager::SUB_TYPE_BYTE ? "byte" : "int", arr);
            }
            case hkTypeManager::SUB_TYPE_REAL:
            {
                return _parseRealArray("real", arr);
            }
            case hkTypeManager::SUB_TYPE_CSTRING:
            {
                for (int i = 0; i < numElems; i++)
                {
                    _skipWhiteSpace();
                    const char* s;
                    if (_parseString(s) != HK_SUCCESS)
                    {
                        return HK_FAILURE;
                    }
                    arr[i] = s;
                }
                break;
            }
            case hkTypeManager::SUB_TYPE_CLASS:
            {
                for (int i = 0; i < numElems; i++)
                {
                    _skipWhiteSpace();
                    hkDataObject obj = arr[i].asObject();
                    new (&m_tmpObjects.expandOne()) hkDataObject(obj);
                    const hkResult res = _parseObject(obj);
                    if (res != HK_SUCCESS)
                    {
                        return res;
                    }
                }
                break;
            }
            case hkTypeManager::SUB_TYPE_POINTER:
            {
                for (int i = 0; i < numElems; i++)
                {
                    hkDataArray_Value v(arr.getImplementation(), i);
                    _parseObjectReferenceImpl(v);
                }
                break;
            }
            case hkTypeManager::SUB_TYPE_TUPLE:
            {
                if (type->isVec())
                {
                    const int size = type->getTupleSize();
                    hkInplaceArray<float,16,hkContainerHeapAllocator> buf;
                    buf.setSize(size);
                    for (int i = 0; i < numElems; i++)
                    {
                        _skipWhiteSpace();
                        if (_parseRealVector(buf.begin(), size) != HK_SUCCESS)
                        {
                            return HK_FAILURE;
                        }
                        arr[i].setVec(buf.begin(), size);
                    }
                }
                break;
            }
            default:
            {
                return HK_FAILURE;
            }
        }
        _skipWhiteSpace();
        return HK_SUCCESS;
    }

    // 0x00F54DE0 (:754): a member's value by its type; arrays and non-vec tuples come as "<array size="">" /
    // "<tuple>" blocks of items
    hkResult Reader::_parseMemberValue(const hkDataClass_MemberInfo& minfo, hkDataObject_Value& valueOut)
    {
        hkTypeManager::Type* type = minfo.m_type;
        const hkTypeManager::SubType subType = type->getSubType();
        if (subType == hkTypeManager::SUB_TYPE_TUPLE)
        {
            int size = type->getTupleSize();
            if (type->isVec())
            {
                float buf[16];
                if (_parseRealVector(buf, size) != HK_SUCCESS)
                {
                    return HK_FAILURE;
                }
                valueOut.setVec(buf, size);
                return HK_SUCCESS;
            }
            if (m_parser.hasAttribute("size"))
            {
                const hkResult res = m_parser.getIntAttribute("size", size);
                if (res != HK_SUCCESS)
                {
                    return res;
                }
                if (size != type->getTupleSize())
                {
                    return HK_FAILURE;
                }
            }
            hkDataObject obj(valueOut.m_impl);
            hkDataArray arr = obj[minfo.m_name].asArray();
            if (m_parser.getToken() == hkXmlStreamParser::TOKEN_BLOCK_START && m_parser.getBlockName() == "tuple")
            {
                _pushBlock();
                m_parser.advance();
                const hkResult res = _parseArrayItems(type->getTupleSize(), arr);
                if (res != HK_SUCCESS)
                {
                    return res;
                }
                return _popBlock();
            }
            return HK_FAILURE;
        }
        if (subType == hkTypeManager::SUB_TYPE_ARRAY)
        {
            int size;
            const hkResult sizeRes = m_parser.getIntAttribute("size", size);
            if (sizeRes != HK_SUCCESS)
            {
                return sizeRes;
            }
            hkDataObject obj(valueOut.m_impl);
            const hkDataObject_Value member = obj[minfo.m_name];
            hkDataArray arr(m_world->newArray(obj, member.m_handle, minfo));
            arr.setSize(size);
            _pushBlock();
            m_parser.advance();
            if (_parseArrayItems(size, arr) != HK_SUCCESS)
            {
                return HK_FAILURE;
            }
            return _popBlock();
        }
        switch (subType)
        {
            case hkTypeManager::SUB_TYPE_BYTE:
            case hkTypeManager::SUB_TYPE_INT:
            {
                int64_t v;
                const hkResult res = _parseInt(subType == hkTypeManager::SUB_TYPE_BYTE ? "byte" : "int", v);
                if (res != HK_SUCCESS)
                {
                    return res;
                }
                valueOut = v;
                return HK_SUCCESS;
            }
            case hkTypeManager::SUB_TYPE_REAL:
            {
                float v;
                const hkResult res = _parseReal("real", v);
                if (res != HK_SUCCESS)
                {
                    return res;
                }
                valueOut = v;
                return HK_SUCCESS;
            }
            case hkTypeManager::SUB_TYPE_CSTRING:
            {
                const char* s;
                if (_parseString(s) != HK_SUCCESS)
                {
                    return HK_FAILURE;
                }
                valueOut = s;
                return HK_SUCCESS;
            }
            case hkTypeManager::SUB_TYPE_CLASS:
            {
                const hkDataClassImpl* klass = m_world->findClass(type->getTypeName());
                if (klass == nullptr)
                {
                    return HK_FAILURE;
                }
                hkDataObject obj;
                const hkResult res = _parseAndCreateObject(klass, obj);
                if (res != HK_SUCCESS)
                {
                    return res;
                }
                valueOut = obj;
                return HK_SUCCESS;
            }
            case hkTypeManager::SUB_TYPE_POINTER:
            {
                return _parseObjectReferenceImpl(valueOut);
            }
            default:
            {
                return HK_FAILURE;
            }
        }
    }

    // 0x00F54470 (:918): the value without its quotes, interned
    const char* Reader::_getAttribute(const char* key)
    {
        hkSubString value;
        if (m_parser.getValue(key, value) == HK_SUCCESS && value.length() > 1 && value.m_start[0] == '"')
        {
            value.m_end--;
            if (*value.m_end == '"')
            {
                value.m_start++;
                return _getString(value);
            }
        }
        return nullptr;
    }

    // 0x00F53AF0 (:937)
    hkBool Reader::_getBoolAttribute(const char* key, hkBool defaultValue)
    {
        hkSubString value;
        if (m_parser.getValue(key, value) == HK_SUCCESS)
        {
            if (value == "\"true\"")
            {
                return true;
            }
            if (value == "\"false\"")
            {
                return false;
            }
        }
        return defaultValue;
    }

    // 0x00F53B80 (:958)
    Reader::Token Reader::_skipWhiteSpace()
    {
        Token tok = m_parser.getToken();
        while (tok == hkXmlStreamParser::TOKEN_WHITESPACE)
        {
            tok = m_parser.advance();
        }
        return tok;
    }

    // 0x00F53BA0 (:968)
    ReferenceInfo* Reader::_findReferenceInfo(const char* key)
    {
        return m_referenceMap.getWithDefault(key, nullptr);
    }

    // 0x00F544F0 (:973): the key is an interned string, so the map need not own it
    ReferenceInfo* Reader::_requireReferenceInfo(const char* key)
    {
        ReferenceInfo* info = m_referenceMap.getWithDefault(key, nullptr);
        if (info == nullptr)
        {
            info = new ReferenceInfo();
            m_referenceMap.insert(key, info);
        }
        return info;
    }

    // 0x00F54560 (:986): an id may name one object only
    hkResult Reader::_setReferencedObject(const char* id, hkDataObject& obj)
    {
        ReferenceInfo* info = _requireReferenceInfo(id);
        if (info->m_object != nullptr)
        {
            return HK_FAILURE;
        }
        info->m_object = obj.getImplementation();
        return HK_SUCCESS;
    }

    // 0x00F557D0 (:999): a new object of klassImpl (or of the "type" attribute's class) read from the current
    // block and registered under its "id"
    hkResult Reader::_parseAndCreateObject(const hkDataClassImpl* klassImpl, hkDataObject& objOut)
    {
        if (klassImpl == nullptr)
        {
            const char* typeName = _getAttribute("type");
            if (typeName == nullptr)
            {
                return HK_FAILURE;
            }
            klassImpl = m_world->findClass(typeName);
            if (klassImpl == nullptr)
            {
                return HK_FAILURE;
            }
        }
        const char* id = _getAttribute("id");
        hkDataObject obj(m_world->newObject(hkDataClass(const_cast<hkDataClassImpl*>(klassImpl)), false));
        hkResult res = _parseObject(obj);
        if (res != HK_SUCCESS)
        {
            return res;
        }
        if (id != nullptr)
        {
            res = _setReferencedObject(id, obj);
            if (res != HK_SUCCESS)
            {
                return res;
            }
        }
        objOut = obj;
        return HK_SUCCESS;
    }

    // 0x00F55360 (:1044): '<member name="">value</member>' blocks; an unknown member fails
    hkResult Reader::_parseObject(hkDataObject& obj)
    {
        _pushBlock();
        const hkDataClassImpl* klass = obj.getImplementation()->getClass();
        m_parser.advance();
        while (_skipWhiteSpace() == hkXmlStreamParser::TOKEN_BLOCK_START)
        {
            const char* name = _getAttribute("name");
            int index;
            if (name == nullptr || (index = klass->getMemberIndexByName(name)) < 0)
            {
                return HK_FAILURE;
            }
            hkDataClass_MemberInfo minfo;
            klass->getMemberInfo(index, minfo);
            hkDataObject_Value value = obj[name];
            const hkResult res = _parseMemberValue(minfo, value);
            if (res != HK_SUCCESS)
            {
                return res;
            }
        }
        return _popBlock() == HK_SUCCESS ? HK_SUCCESS : HK_FAILURE;
    }

    // 0x00F53BC0 (:1097): every id referenced must name an object
    hkResult Reader::_fixUpReferences()
    {
        for (hkStringMap<ReferenceInfo*,hkContainerHeapAllocator>::Iterator it = m_referenceMap.getIterator(); m_referenceMap.isValid(it); it = m_referenceMap.getNext(it))
        {
            ReferenceInfo* info = m_referenceMap.getValue(it);
            if (info->m_object == nullptr)
            {
                return HK_FAILURE;
            }
            info->assign();
        }
        return HK_SUCCESS;
    }

    // 0x00F53CD0 (:1118): the "<hktagfile>" attributes: version (1 or 2), sdkversion, and from version 2 the
    // predicates
    hkResult Reader::readHeader(hkXmlTagfile::Header& out)
    {
        m_parser.advance();
        if (_skipWhiteSpace() == hkXmlStreamParser::TOKEN_QBLOCK)
        {
            m_parser.advance();
        }
        if (_skipWhiteSpace() == hkXmlStreamParser::TOKEN_BLOCK_START && m_parser.getBlockName() == "hktagfile")
        {
            if (m_parser.getIntAttribute("version", out.m_version) == HK_FAILURE)
            {
                return HK_FAILURE;
            }
            if ((unsigned int)(out.m_version - 1) > 1)
            {
                char buf[512];
                hkErrStream os(buf, sizeof(buf));
                os << "Unrecognised tagfile version " << out.m_version;
                // Deviation from rfg.exe: its warning macro is written out as these calls, at the macro's id and line.
                hkErrorFwd::messageWarning(0x23C6037F, buf, "Tagfile\\Xml\\hkXmlTagfileReader.cpp", 1178);
                return HK_FAILURE;
            }
            hkSubString sdk;
            if (m_parser.getValue("sdkversion", sdk) == HK_SUCCESS)
            {
                out.m_sdkVersion.set(sdk.m_start + 1, sdk.length() - 2); // without the quotes
            }
            if (out.m_version < 2)
            {
                return HK_SUCCESS;
            }
            if (m_parser.getIntAttribute("maxpredicate", out.m_maxpredicate) == HK_SUCCESS &&
                m_parser.getIntListAttribute<unsigned short>("predicates", out.m_truePredicates) != HK_FAILURE)
            {
                return HK_SUCCESS;
            }
        }
        return HK_FAILURE;
    }

    // 0x00F55920 (:1187): the class definitions, then the objects (the last one read is the contents), then the
    // reference fix-up. Original quirk kept: a class definition that fails is skipped over, not reported.
    hkResult Reader::parseRoot(hkDataObject& objOut)
    {
        m_parser.advance();
        if (_skipWhiteSpace() == hkXmlStreamParser::TOKEN_QBLOCK)
        {
            m_parser.advance();
        }
        if (_skipWhiteSpace() != hkXmlStreamParser::TOKEN_BLOCK_START)
        {
            char buf[512];
            hkErrStream os(buf, sizeof(buf));
            os << "Didn't find the root 'hktagfile' block";
            hkErrorFwd::messageWarning(0xFEED00AA, buf, "Tagfile\\Xml\\hkXmlTagfileReader.cpp", 1203);
            return HK_FAILURE;
        }
        if (!(m_parser.getBlockName() == "hktagfile"))
        {
            char buf[512];
            hkErrStream os(buf, sizeof(buf));
            os << "Expecting 'hktagfile' block";
            hkErrorFwd::messageWarning(0xFEED00AA, buf, "Tagfile\\Xml\\hkXmlTagfileReader.cpp", 1209);
            return HK_FAILURE;
        }
        int version = 0;
        m_parser.getIntAttribute("version", version);
        _pushBlock();
        m_parser.advance();
        Token tok = _skipWhiteSpace();
        while (tok == hkXmlStreamParser::TOKEN_BLOCK_START && m_parser.getBlockName() == "class")
        {
            _parseClassDefinition();
            tok = _skipWhiteSpace();
        }
        hkDataObject last;
        for (tok = _skipWhiteSpace(); tok == hkXmlStreamParser::TOKEN_BLOCK_START || tok == hkXmlStreamParser::TOKEN_BLOCK_START_END; tok = _skipWhiteSpace())
        {
            hkDataObject obj;
            if (_parseAndCreateObject(nullptr, obj) == HK_FAILURE)
            {
                return HK_FAILURE;
            }
            last = obj;
        }
        if (_popBlock() != HK_SUCCESS)
        {
            return HK_FAILURE;
        }
        if (_skipWhiteSpace() != hkXmlStreamParser::TOKEN_EOF)
        {
            return HK_FAILURE;
        }
        const hkResult res = _fixUpReferences();
        if (res != HK_SUCCESS)
        {
            return res;
        }
        objOut = last;
        return HK_SUCCESS;
    }
}

// 0x00F55C70 (:1289): the contents object, or null on a failure
hkDataObject hkXmlTagfileReader::load(hkStreamReader* stream, hkDataWorld& world)
{
    Reader reader(stream, &world);
    hkDataObject obj;
    if (reader.parseRoot(obj) == HK_SUCCESS)
    {
        return obj;
    }
    return hkDataObject();
}

// 0x00F55D30 (:1303): the header only (the reader has no world)
hkResult hkXmlTagfileReader::readHeader(hkStreamReader* stream, hkXmlTagfile::Header& out)
{
    Reader reader(stream, nullptr);
    return reader.readHeader(out);
}
