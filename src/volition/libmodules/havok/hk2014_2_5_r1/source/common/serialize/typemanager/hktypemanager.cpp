// Original: volition/libmodules/havok/hk2014_2_5_r1/source/common/serialize/typemanager/hktypemanager.cpp
//
// Havok 2014.2.5 (stub_impl.md S4, serializer SC): the type manager of the data worlds. A type is a chain from its
// outermost modifier (pointer, array, tuple) to a terminal (a built-in or a class); every type is interned once, keyed
// by its hash, so types of one manager compare by pointer. From the decompile and asm.
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/serialize/typemanager/hktypemanager.h"

#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/container/string/hkstringptr.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/system/io/ostream/hkostream.h"

// 0x00F51540 (:21)
int hkTypeManager::Type::getTupleSize() const
{
    if (m_subType == SUB_TYPE_TUPLE)
    {
        return m_extra.m_size;
    }
    return -1;
}

// 0x00F51550 (:26)
const char* hkTypeManager::Type::getTypeName() const
{
    if (m_subType == SUB_TYPE_CLASS)
    {
        return m_extra.m_name;
    }
    return nullptr;
}

// 0x00F51560 (:31)
// Rotate left by one, then mix in the sub type times the golden ratio constant 0x9E3779B1; a class adds its own
// address (classes are interned), a tuple its size.
unsigned int hkTypeManager::Type::_calcHash(const Type* cur)
{
    unsigned int hash = 0;
    do
    {
        hash = ((hash << 1) | (hash >> 31)) ^ ((unsigned int)cur->m_subType * 0x9E3779B1u);
        if (cur->m_subType == SUB_TYPE_CLASS)
        {
            hash ^= (unsigned int)(hkUlong)cur;
        }
        else if (cur->m_subType == SUB_TYPE_TUPLE)
        {
            hash ^= (unsigned int)cur->m_extra.m_size;
        }
        cur = cur->m_parent;
    } while (cur);
    return hash;
}

// 0x00F515A0 (:65)
// -1 is the multimap's free key, so it is replaced by 0x7AF1F32A.
unsigned int hkTypeManager::Type::calcHash() const
{
    const unsigned int hash = _calcHash(this);
    return hash == 0xFFFFFFFFu ? 0x7AF1F32Au : hash;
}

// 0x00F515C0 (:72)
hkBool hkTypeManager::Type::_equals(const Type& rhs) const
{
    if (m_subType != rhs.m_subType || m_parent != rhs.m_parent)
    {
        return false;
    }
    if (m_subType == SUB_TYPE_CLASS || m_subType == SUB_TYPE_TUPLE)
    {
        return m_extra.m_name == rhs.m_extra.m_name;
    }
    return true;
}

// 0x00F51610 (:88)
hkTypeManager::Type* hkTypeManager::Type::findTerminal()
{
    Type* cur = this;
    while (cur->m_parent)
    {
        cur = cur->m_parent;
    }
    return cur;
}

// 0x00F51630 (:98)
void hkTypeManager::Type::asText(const Type* cur, hkOstream& stream)
{
    while (cur)
    {
        switch (cur->m_subType)
        {
            case SUB_TYPE_INVALID:
                stream << "!";
                break;
            case SUB_TYPE_VOID:
                stream << "void";
                break;
            case SUB_TYPE_BYTE:
                stream << "byte";
                break;
            case SUB_TYPE_REAL:
                stream << "real";
                break;
            case SUB_TYPE_INT:
                stream << "int";
                break;
            case SUB_TYPE_CSTRING:
                stream << "cstring";
                break;
            case SUB_TYPE_CLASS:
                if (cur->getTypeName())
                {
                    stream << "class " << cur->getTypeName();
                }
                else
                {
                    stream << "homogeneous/variant class";
                }
                break;
            case SUB_TYPE_POINTER:
                stream << "*";
                break;
            case SUB_TYPE_ARRAY:
                stream << "[]";
                break;
            case SUB_TYPE_TUPLE:
                stream << "{" << cur->m_extra.m_size << "}";
                break;
            default:
                break;
        }
        cur = cur->m_parent;
    }
}

// 0x00F51720 (:132)
void hkTypeManager::Type::asText(hkOstream& stream) const
{
    asText(this, stream);
}

// 0x00F523B0 (:137)
hkStringPtr hkTypeManager::Type::asString() const
{
    hkArray<char> buf;
    hkOstream stream(buf);
    asText(this, stream);
    buf.pushBack(0);
    return hkStringPtr(buf.begin());
}

// 0x00F51AF0 (:149)
// From this type to its terminal.
void hkTypeManager::Type::getTypePath(Type* type, hkArray<Type*>& types)
{
    types.clear();
    for (; type; type = type->m_parent)
    {
        types.pushBack(type);
    }
}

// 0x00F51740 (:159)
// Compares across managers: by sub type, class name and tuple size along both chains. A nameless (homogeneous) class
// on either side matches the rest of the other chain.
hkBool hkTypeManager::Type::_isEqual(const Type* a, const Type* b)
{
    while (a && b)
    {
        if (a->m_subType != b->m_subType)
        {
            return false;
        }
        if (a->m_subType == SUB_TYPE_CLASS)
        {
            const char* aName = a->getTypeName();
            const char* bName = b->getTypeName();
            if (aName == nullptr || bName == nullptr)
            {
                return true;
            }
            if (hkString::strCmp(aName, bName) != 0)
            {
                return false;
            }
        }
        else if (a->m_subType == SUB_TYPE_TUPLE)
        {
            if (a->getTupleSize() != b->getTupleSize())
            {
                return false;
            }
        }
        a = a->m_parent;
        b = b->m_parent;
    }
    return a == nullptr && b == nullptr;
}

// 0x00F517E0 (:205)
hkBool hkTypeManager::Type::isEqual(const Type* type) const
{
    if (type == this)
    {
        return true;
    }
    return _isEqual(this, type);
}

// 0x00F526C0 (:222)
// Element size 12 (sizeof(Type)), alignment 4, blocks of 0x800 bytes. The homogeneous class (no name) is interned but
// not in the class map.
hkTypeManager::hkTypeManager()
    : m_typeFreeList(sizeof(Type), 4, 0x800, nullptr, nullptr)
{
    hkString::memSet(m_builtInTypes, 0, sizeof(m_builtInTypes));
    _addBuiltIn(SUB_TYPE_VOID);
    _addBuiltIn(SUB_TYPE_INT);
    _addBuiltIn(SUB_TYPE_REAL);
    _addBuiltIn(SUB_TYPE_CSTRING);
    _addBuiltIn(SUB_TYPE_BYTE);

    Type* type = (Type*)m_typeFreeList.alloc();
    type->m_subType = SUB_TYPE_CLASS;
    type->m_parent = nullptr;
    type->m_extra.m_name = nullptr;
    m_typeMultiMap.insert(type->calcHash(), type);
    m_homogenousClass = type;
}

// 0x00F51B40 (:246)
// The free list's destructor frees every type; the class map frees its name copies.
hkTypeManager::~hkTypeManager()
{
}

// 0x00F52470 (:250)
hkTypeManager::Type* hkTypeManager::_addBuiltIn(SubType subType)
{
    Type type;
    type.m_subType = subType;
    type.m_parent = nullptr;
    type.m_extra.m_name = nullptr;
    Type* added = _addType(type);
    m_builtInTypes[subType] = added;
    return added;
}

// 0x00F51B80 (:263)
// The interned equal type, or a new copy of typeIn.
hkTypeManager::Type* hkTypeManager::_addType(const Type& typeIn)
{
    const unsigned int hash = typeIn.calcHash();
    for (hkPointerMultiMap<unsigned int, Type*>::Iterator it = m_typeMultiMap.findKey(hash); m_typeMultiMap.isValid(it);
         it = m_typeMultiMap.getNext(it, hash))
    {
        Type* type = m_typeMultiMap.getValue(it);
        if (typeIn._equals(*type))
        {
            return type;
        }
    }
    Type* type = (Type*)m_typeFreeList.alloc();
    *type = typeIn;
    m_typeMultiMap.insert(hash, type);
    return type;
}

// 0x00F51C70 (:289)
// Every interned type whose terminal is clsType (the class itself included).
void hkTypeManager::findTypesUsingClass(Type* clsType, hkArray<Type*>& types)
{
    types.clear();
    for (hkPointerMultiMap<unsigned int, Type*>::Iterator it = m_typeMultiMap.getIterator(); m_typeMultiMap.isValid(it);
         it = m_typeMultiMap.getNext(it))
    {
        Type* type = m_typeMultiMap.getValue(it);
        if (type->findTerminal() == clsType)
        {
            types.pushBack(type);
        }
    }
}

// 0x00F51D10 (:307)
// Nothing happens when the class does not exist or the new name is taken.
void hkTypeManager::renameClass(const char* clsName, const char* newName)
{
    Type* type = getClass(clsName);
    if (type == nullptr || getClass(newName) != nullptr)
    {
        return;
    }
    m_classMap.remove(clsName);
    type->m_extra.m_name = m_classMap.insert(newName, type);
}

// 0x00F524A0 (:329)
// The class and the types built on it stay allocated but invalid (garbageCollect frees them).
void hkTypeManager::removeClass(Type* clsType)
{
    const char* name = clsType->getTypeName();
    hkArray<Type*> types;
    findTypesUsingClass(clsType, types);

    // the hashes before invalidating anything (the chains still lead to the class)
    hkArray<unsigned int> hashes;
    hashes.setSize(types.getSize());
    for (int i = 0; i < types.getSize(); i++)
    {
        hashes[i] = types[i]->calcHash();
    }
    for (int i = 0; i < types.getSize(); i++)
    {
        Type* type = types[i];
        m_typeMultiMap.remove(hashes[i], type);
        type->m_subType = SUB_TYPE_INVALID;
        type->m_parent = nullptr;
    }
    m_classMap.remove(name);
}

// 0x00F51DB0 (:364)
// Collects the allocated types that were invalidated (m_subType 0 in their first word).
static void _freeListCallback(void* start, unsigned int size, bool allocated, int pool, void* param)
{
    if (allocated && *(int*)start == hkTypeManager::SUB_TYPE_INVALID)
    {
        static_cast<hkArray<void*>*>(param)->pushBack(start);
    }
}

// 0x00F52620 (:377)
void hkTypeManager::garbageCollect()
{
    hkArray<void*> invalid;
    m_typeFreeList.walkMemory(_freeListCallback, 0, &invalid);
    for (int i = 0; i < invalid.getSize(); i++)
    {
        m_typeFreeList.free(invalid[i]);
    }
    m_typeFreeList.findGarbage();
    m_typeFreeList.freeAllFreeBlocks();
}

// 0x00F51E00 (:390)
hkTypeManager::Type* hkTypeManager::makePointer(Type* parent)
{
    Type type;
    type.m_subType = SUB_TYPE_POINTER;
    type.m_parent = parent;
    type.m_extra.m_name = nullptr;
    return _addType(type);
}

// 0x00F51E30 (:402)
// Original: m_extra is left uninitialised (the stack garbage becomes part of the interned array type; _equals and the
// hash ignore it).
hkTypeManager::Type* hkTypeManager::makeArray(Type* parent)
{
    Type type;
    type.m_subType = SUB_TYPE_ARRAY;
    type.m_parent = parent;
    return _addType(type);
}

// 0x00F51E60 (:413)
hkTypeManager::Type* hkTypeManager::makeTuple(Type* parent, int tupleSize)
{
    Type type;
    type.m_subType = SUB_TYPE_TUPLE;
    type.m_parent = parent;
    type.m_extra.m_size = tupleSize;
    return _addType(type);
}

// 0x00F51810 (:426)
// A C identifier, with ':' allowed after the first character.
hkBool hkTypeManager::_isValidClassName(const char* name)
{
    if (name == nullptr || *name == 0)
    {
        return false;
    }
    const char c0 = *name;
    if (!((c0 >= 'a' && c0 <= 'z') || (c0 >= 'A' && c0 <= 'Z') || c0 == '_'))
    {
        return false;
    }
    for (const char* p = name + 1; *p; p++)
    {
        const char c = *p;
        if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == ':' || c == '_'))
        {
            return false;
        }
    }
    return true;
}

// 0x00F51A50 (:465)
hkTypeManager::Type* hkTypeManager::getClass(const char* clsName) const
{
    return m_classMap.getWithDefault(clsName, nullptr);
}

// 0x00F51E90 (:470)
// The type's name is the class map's copy of clsName.
hkTypeManager::Type* hkTypeManager::addClass(const char* clsName)
{
    Type* type = getClass(clsName);
    if (type == nullptr)
    {
        type = (Type*)m_typeFreeList.alloc();
        type->m_subType = SUB_TYPE_CLASS;
        type->m_parent = nullptr;
        type->m_extra.m_name = m_classMap.insert(clsName, type);
        m_typeMultiMap.insert(type->calcHash(), type);
    }
    return type;
}

// 0x00F51F60 (:493)
// Only modifiers have parents: a terminal gives nullptr.
hkTypeManager::Type* hkTypeManager::replaceParent(Type* typeIn, Type* parent)
{
    if (typeIn->m_parent == parent)
    {
        return typeIn;
    }
    switch (typeIn->m_subType)
    {
        case SUB_TYPE_POINTER:
            return makePointer(parent);
        case SUB_TYPE_ARRAY:
            return makeArray(parent);
        case SUB_TYPE_TUPLE:
            return makeTuple(parent, typeIn->getTupleSize());
        default:
            return nullptr;
    }
}

// 0x00F527B0 (:513)
// Rebuilds the modifier chain of typeIn over newTerminal, innermost first.
hkTypeManager::Type* hkTypeManager::replaceTerminal(Type* typeIn, Type* newTerminal)
{
    hkInplaceArray<Type*, 16, hkContainerHeapAllocator> types;
    Type* cur = typeIn;
    do
    {
        types.pushBack(cur);
        cur = cur->m_parent;
    } while (cur);
    if (types.back() == newTerminal)
    {
        return typeIn;
    }
    // Original: the loop starts at the terminal itself (index size - 1), so replaceParent(terminal) gives nullptr
    // unless the terminal's parent (nullptr) is newTerminal; the modifiers are then rebuilt over that result.
    for (int i = types.getSize() - 1; i >= 0; i--)
    {
        newTerminal = replaceParent(types[i], newTerminal);
    }
    return newTerminal;
}

// 0x00F51FB0 (:541)
hkTypeManager::Type* hkTypeManager::makeArray(SubType subType)
{
    if (m_builtInTypes[subType] == nullptr)
    {
        return nullptr;
    }
    return makeArray(m_builtInTypes[subType]);
}

// 0x00F51FD0 (:551)
hkTypeManager::Type* hkTypeManager::getClassPointer(const char* name)
{
    return makePointer(addClass(name));
}

// 0x00F51FF0 (:556)
hkBool hkTypeManager::isOwned(Type* type) const
{
    const unsigned int hash = type->calcHash();
    for (hkPointerMultiMap<unsigned int, Type*>::Iterator it = m_typeMultiMap.findKey(hash); m_typeMultiMap.isValid(it);
         it = m_typeMultiMap.getNext(it, hash))
    {
        if (m_typeMultiMap.getValue(it) == type)
        {
            return true;
        }
    }
    return false;
}

// 0x00F528B0 (:577)
// The equal type of this manager (a type of another manager is rebuilt from its terminal outwards).
hkTypeManager::Type* hkTypeManager::copyType(Type* type)
{
    if (isOwned(type))
    {
        return type;
    }
    hkInplaceArray<Type*, 16, hkContainerHeapAllocator> types;
    Type::getTypePath(type, types);
    Type* terminal = types[types.getSize() - 1];
    Type* cur;
    if (terminal->m_subType == SUB_TYPE_CLASS)
    {
        cur = addClass(terminal->getTypeName());
    }
    else
    {
        cur = m_builtInTypes[terminal->m_subType];
    }
    if (cur == nullptr)
    {
        return nullptr;
    }
    for (int i = types.getSize() - 2; i >= 0; i--)
    {
        cur = replaceParent(types[i], cur);
    }
    return cur;
}

// 0x00F51880 (:619)
// The compact form parseTypeExpression reads: v b r i s, C<name>; for a class, * pointer, [] array, {n} tuple.
void hkTypeManager::appendTypeExpression(const Type* type, hkOstream& stream)
{
    while (type)
    {
        switch (type->m_subType)
        {
            case SUB_TYPE_INVALID:
                stream << "!";
                break;
            case SUB_TYPE_VOID:
                stream << "v";
                break;
            case SUB_TYPE_BYTE:
                stream << "b";
                break;
            case SUB_TYPE_REAL:
                stream << "r";
                break;
            case SUB_TYPE_INT:
                stream << "i";
                break;
            case SUB_TYPE_CSTRING:
                stream << "s";
                break;
            case SUB_TYPE_CLASS:
                stream << "C" << type->getTypeName() << ";";
                break;
            case SUB_TYPE_POINTER:
                stream << "*";
                break;
            case SUB_TYPE_ARRAY:
                stream << "[]";
                break;
            case SUB_TYPE_TUPLE:
                stream << "{" << type->getTupleSize() << "}";
                break;
            default:
                break;
        }
        type = type->m_parent;
    }
}

// 0x00F52080 (:642)
// Class names are limited to 127 characters, tuple sizes to 9 digits.
hkTypeManager::Type* hkTypeManager::parseTypeExpression(const char* typeExpression)
{
    switch (*typeExpression)
    {
        case '!':
            return m_builtInTypes[SUB_TYPE_INVALID];
        case 'v':
            return m_builtInTypes[SUB_TYPE_VOID];
        case 'b':
            return m_builtInTypes[SUB_TYPE_BYTE];
        case 'r':
            return m_builtInTypes[SUB_TYPE_REAL];
        case 'i':
            return m_builtInTypes[SUB_TYPE_INT];
        case 's':
            return m_builtInTypes[SUB_TYPE_CSTRING];
        case 'C':
        {
            const char* start = typeExpression + 1;
            const char* cur = start;
            for (;; cur++)
            {
                const char c = *cur;
                if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '_' || c == ':'))
                {
                    break;
                }
            }
            const int len = (int)(cur - start);
            if (len > 0 && len < 128 && *cur == ';')
            {
                char buf[128];
                hkString::strNcpy(buf, start, len);
                buf[len] = 0;
                return addClass(buf);
            }
            return nullptr;
        }
        case '*':
            return makePointer(parseTypeExpression(typeExpression + 1));
        case '[':
            // skips "[]"
            return makeArray(parseTypeExpression(typeExpression + 2));
        case '{':
        {
            const char* start = typeExpression + 1;
            const char* cur = start;
            while (*cur >= '0' && *cur <= '9')
            {
                cur++;
            }
            const int len = (int)(cur - start);
            if (len > 0 && len < 10 && *cur == '}')
            {
                char buf[12];
                hkString::strNcpy(buf, start, len);
                buf[len] = 0;
                const int tupleSize = hkString::atoi(buf, 0);
                return makeTuple(parseTypeExpression(cur + 1), tupleSize);
            }
            return nullptr;
        }
        default:
            return nullptr;
    }
}

// 0x00F522B0 (:725)
// The VEC_n types are tuples of reals; OBJECT is a pointer to the class (the homogeneous class without a name), STRUCT
// the class itself.
hkTypeManager::Type* hkTypeManager::getType(hkLegacyType::Type type, const char* className, int tupleSize)
{
    Type* terminal;
    switch (type & hkLegacyType::TYPE_MASK_BASIC_TYPES)
    {
        case hkLegacyType::TYPE_VOID:
            terminal = m_builtInTypes[SUB_TYPE_VOID];
            break;
        case hkLegacyType::TYPE_BYTE:
            terminal = m_builtInTypes[SUB_TYPE_BYTE];
            break;
        case hkLegacyType::TYPE_INT:
            terminal = m_builtInTypes[SUB_TYPE_INT];
            break;
        case hkLegacyType::TYPE_REAL:
            terminal = m_builtInTypes[SUB_TYPE_REAL];
            break;
        case hkLegacyType::TYPE_VEC_4:
            terminal = makeTuple(m_builtInTypes[SUB_TYPE_REAL], 4);
            break;
        case hkLegacyType::TYPE_VEC_8:
            terminal = makeTuple(m_builtInTypes[SUB_TYPE_REAL], 8);
            break;
        case hkLegacyType::TYPE_VEC_12:
            terminal = makeTuple(m_builtInTypes[SUB_TYPE_REAL], 12);
            break;
        case hkLegacyType::TYPE_VEC_16:
            terminal = makeTuple(m_builtInTypes[SUB_TYPE_REAL], 16);
            break;
        case hkLegacyType::TYPE_OBJECT:
            terminal = makePointer(className ? addClass(className) : m_homogenousClass);
            break;
        case hkLegacyType::TYPE_STRUCT:
            terminal = className ? addClass(className) : m_homogenousClass;
            break;
        case hkLegacyType::TYPE_CSTRING:
            terminal = m_builtInTypes[SUB_TYPE_CSTRING];
            break;
        default:
            return nullptr;
    }
    if (terminal == nullptr)
    {
        return nullptr;
    }
    if (type & hkLegacyType::TYPE_ARRAY)
    {
        return makeArray(terminal);
    }
    if (type & hkLegacyType::TYPE_TUPLE)
    {
        return makeTuple(terminal, tupleSize);
    }
    return terminal;
}

// 0x00F51970 (:804)
hkLegacyType::Type hkTypeManager::getTerminalLegacyType(Type* type, const char** classNameOut)
{
    *classNameOut = nullptr;
    switch (type->m_subType)
    {
        case SUB_TYPE_BYTE:
            return hkLegacyType::TYPE_BYTE;
        case SUB_TYPE_REAL:
            return hkLegacyType::TYPE_REAL;
        case SUB_TYPE_INT:
            return hkLegacyType::TYPE_INT;
        case SUB_TYPE_CSTRING:
            return hkLegacyType::TYPE_CSTRING;
        case SUB_TYPE_CLASS:
            *classNameOut = type->getTypeName();
            return hkLegacyType::TYPE_STRUCT;
        case SUB_TYPE_POINTER:
            if (type->m_parent->m_subType == SUB_TYPE_CLASS)
            {
                *classNameOut = type->m_parent->getTypeName();
                return hkLegacyType::TYPE_OBJECT;
            }
            break;
        case SUB_TYPE_TUPLE:
            if (type->m_parent->m_subType == SUB_TYPE_REAL)
            {
                switch (type->getTupleSize())
                {
                    case 4:
                        return hkLegacyType::TYPE_VEC_4;
                    case 8:
                        return hkLegacyType::TYPE_VEC_8;
                    case 12:
                        return hkLegacyType::TYPE_VEC_12;
                    case 16:
                        return hkLegacyType::TYPE_VEC_16;
                }
            }
            break;
        default:
            break;
    }
    return hkLegacyType::TYPE_VOID;
}

// 0x00F51A70 (:853)
// A vec tuple is a basic type; other tuples report their size.
hkLegacyType::Type hkTypeManager::getLegacyType(Type* type, const char** className, int& numTupleOut)
{
    *className = nullptr;
    numTupleOut = 0;
    if (type->m_subType == SUB_TYPE_ARRAY)
    {
        return (hkLegacyType::Type)(getTerminalLegacyType(type->m_parent, className) | hkLegacyType::TYPE_ARRAY);
    }
    if (type->m_subType == SUB_TYPE_TUPLE && !type->isVec())
    {
        numTupleOut = type->getTupleSize();
        return (hkLegacyType::Type)(getTerminalLegacyType(type->m_parent, className) | hkLegacyType::TYPE_TUPLE);
    }
    return getTerminalLegacyType(type, className);
}
