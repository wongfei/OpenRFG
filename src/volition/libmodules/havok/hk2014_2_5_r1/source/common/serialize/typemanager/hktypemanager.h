// Original: volition/libmodules/havok/hk2014_2_5_r1/source/common/serialize/typemanager/hktypemanager.h
//
// Havok 2014.2.5: full declaration from rfg.pdb (stub_impl.md S4, serializer); inline code as the 2014.1 SDK
// (checked against rfg.exe, SC).
#pragma once

#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/memory/memoryclasses/hkmemoryclasses.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/container/array/hkarray.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/container/hkcontainerallocators.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/container/pointermultimap/hkpointermultimap.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/container/string/hkstring.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/container/stringmap/hkstoragestringmap.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/memory/allocator/freelist/hkfreelist.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/object/hkreferencedobject.h"

class hkBool;
class hkOstream;
class hkStringPtr;
struct hkSubString;

// size 0x1
// The type codes of the tagfile and packfile formats (hkLegacyType::Type in rfg.pdb; the struct itself has no
// PDB record).
struct hkLegacyType
{
    enum Type
    {
        TYPE_VOID = 0,
        TYPE_BYTE = 1,
        TYPE_INT = 2,
        TYPE_REAL = 3,
        TYPE_VEC_4 = 4,
        TYPE_VEC_8 = 5,
        TYPE_VEC_12 = 6,
        TYPE_VEC_16 = 7,
        TYPE_OBJECT = 8,
        TYPE_STRUCT = 9,
        TYPE_CSTRING = 10,
        TYPE_NUM_BASIC_TYPES = 11,
        TYPE_MASK_BASIC_TYPES = 15,
        TYPE_ARRAY = 16,
        TYPE_ARRAY_BYTE = 17,
        TYPE_ARRAY_INT = 18,
        TYPE_ARRAY_REAL = 19,
        TYPE_ARRAY_VEC_4 = 20,
        TYPE_ARRAY_VEC_8 = 21,
        TYPE_ARRAY_VEC_12 = 22,
        TYPE_ARRAY_VEC_16 = 23,
        TYPE_ARRAY_OBJECT = 24,
        TYPE_ARRAY_STRUCT = 25,
        TYPE_ARRAY_CSTRING = 26,
        TYPE_TUPLE = 32,
        TYPE_TUPLE_BYTE = 33,
        TYPE_TUPLE_INT = 34,
        TYPE_TUPLE_REAL = 35,
        TYPE_TUPLE_VEC_4 = 36,
        TYPE_TUPLE_VEC_8 = 37,
        TYPE_TUPLE_VEC_12 = 38,
        TYPE_TUPLE_VEC_16 = 39,
        TYPE_TUPLE_OBJECT = 40,
        TYPE_TUPLE_STRUCT = 41,
        TYPE_TUPLE_CSTRING = 42,
    };
};

// size 0x88
class hkTypeManager : public hkReferencedObject
{
public:
    enum SubType
    {
        SUB_TYPE_INVALID = 0,
        SUB_TYPE_VOID = 1,
        SUB_TYPE_BYTE = 2,
        SUB_TYPE_REAL = 3,
        SUB_TYPE_INT = 4,
        SUB_TYPE_CSTRING = 5,
        SUB_TYPE_CLASS = 6,
        SUB_TYPE_POINTER = 7,
        SUB_TYPE_ARRAY = 8,
        SUB_TYPE_TUPLE = 9,
        SUB_TYPE_COUNT_OF = 10,
    };

    // size 0xC
    struct Type
    {
        HK_DECLARE_NONVIRTUAL_CLASS_ALLOCATOR(Type);
        hkBool isClass(char* name) const { return isClass() && hkString::strCmp(getTypeName(), name) == 0; }
        hkBool isClass() const { return m_subType == SUB_TYPE_CLASS; } // 0x00F460E0
        hkBool isTuple() const { return m_subType == SUB_TYPE_TUPLE; } // 0x00F460F0

        // 0x00F52BF0: a tuple of 4, 8, 12 or 16 reals
        hkBool isVec() const
        {
            return isTuple() && m_parent->isReal()
                && (m_extra.m_size == 4 || m_extra.m_size == 8 || m_extra.m_size == 12 || m_extra.m_size == 16);
        }

        hkBool isArray() const { return m_subType == SUB_TYPE_ARRAY; } // 0x00F46100
        hkBool isVoid() const { return m_subType == SUB_TYPE_VOID; } // 0x00F46110
        hkBool isPointer() const { return m_subType == SUB_TYPE_POINTER; } // 0x00F49540
        hkBool isInt() const { return m_subType == SUB_TYPE_INT; } // 0x00F46120
        hkBool isByte() const { return m_subType == SUB_TYPE_BYTE; } // 0x00F46130
        hkBool isReal() const { return m_subType == SUB_TYPE_REAL; } // 0x00F46140
        hkBool isCstring() const { return m_subType == SUB_TYPE_CSTRING; } // 0x00F46150
        hkBool isClassPointer() const { return isPointer() && getParent()->isClass(); } // 0x00F5D3E0
        hkBool isValid() const { return m_subType != SUB_TYPE_INVALID; } // 0x00F51520
        hkBool isTerminal() const { return m_parent == nullptr; }
        Type* findTerminal(); // 0x00F51610
        Type* getParent() const { return m_parent; } // 0x00F46160
        int getTupleSize() const; // 0x00F51540
        const char* getTypeName() const; // 0x00F51550
        SubType getSubType() const { return m_subType; } // 0x00F46170
        unsigned int calcHash() const; // 0x00F515A0
        static void asText(const Type* cur, hkOstream& stream); // 0x00F51630
        void asText(hkOstream& stream) const; // 0x00F51720
        hkStringPtr asString() const; // 0x00F523B0
        hkBool isEqual(const Type* type) const; // 0x00F517E0
        static void getTypePath(Type* type, hkArray<Type*,hkContainerHeapAllocator>& types); // 0x00F51AF0
    public:
        static unsigned int _calcHash(const Type* cur); // 0x00F51560
        hkBool _equals(const Type& rhs) const; // 0x00F515C0
        static hkBool _isEqual(const Type* a, const Type* b); // 0x00F51740

        SubType m_subType; // 0x00
        Type* m_parent; // 0x04
        union
        {
            const char* m_name; // 0x00
            int m_size; // 0x00
        } m_extra; // 0x08
    };

    HK_DECLARE_CLASS_ALLOCATOR(); // delete(void* p, unsigned int nbytes) 0x00F53110
    Type* addClass(const char* clsName); // 0x00F51E90
    void renameClass(const char* clsName, const char* newName); // 0x00F51D10
    void findTypesUsingClass(Type* clsType, hkArray<Type*,hkContainerHeapAllocator>& types); // 0x00F51C70
    void removeClass(Type* clsType); // 0x00F524A0
    Type* getClass(const char* clsName) const; // 0x00F51A50
    hkBool isOwned(Type* type) const; // 0x00F51FF0
    Type* copyType(Type* type); // 0x00F528B0
    Type* replaceClass(Type*, const char*);
    Type* getHomogenousClass() const { return m_homogenousClass; } // 0x00F51530
    void garbageCollect(); // 0x00F52620
    Type* getVoid() const { return m_builtInTypes[SUB_TYPE_VOID]; }
    Type* getByte() const { return m_builtInTypes[SUB_TYPE_BYTE]; }
    Type* getReal() const { return m_builtInTypes[SUB_TYPE_REAL]; }
    Type* getCstringType() const { return m_builtInTypes[SUB_TYPE_CSTRING]; }
    Type* getIntType() const { return m_builtInTypes[SUB_TYPE_INT]; }
    Type* getSubType(SubType type) const { return m_builtInTypes[type]; } // 0x00F46180
    Type* makePointer(Type* parent); // 0x00F51E00
    Type* makeArray(SubType subType); // 0x00F51FB0
    Type* makeArray(Type* parent); // 0x00F51E30
    Type* makeTuple(Type* parent, int tupleSize); // 0x00F51E60
    Type* getClassPointer(const char* name); // 0x00F51FD0
    Type* replaceParent(Type* typeIn, Type* parent); // 0x00F51F60
    Type* replaceTerminal(Type* typeIn, Type* newTerminal); // 0x00F527B0
    Type* parseTypeExpression(const char* typeExpression); // 0x00F52080
    static void appendTypeExpression(const Type* type, hkOstream& stream); // 0x00F51880
    Type* getType(hkLegacyType::Type type, const char* className, int tupleSize); // 0x00F522B0
    static hkLegacyType::Type getTerminalLegacyType(Type* type, const char** classNameOut); // 0x00F51970
    static hkLegacyType::Type getLegacyType(Type* type, const char** className, int& numTupleOut); // 0x00F51A70
    hkTypeManager(); // 0x00F526C0
    virtual ~hkTypeManager(); // vtbl+0x00 0x00F51B40
    Type* _addBuiltIn(SubType subType); // 0x00F52470
    Type* _addType(const Type& typeIn); // 0x00F51B80
    static hkBool _isValidClassName(const char* name); // 0x00F51810
    static hkSubString _findClass(const char*);

    Type* m_homogenousClass; // 0x08
    Type* m_builtInTypes[10]; // 0x0C
    hkStorageStringMap<Type*,hkContainerHeapAllocator> m_classMap; // 0x34
    hkPointerMultiMap<unsigned int,Type*> m_typeMultiMap; // 0x44
    hkFreeList m_typeFreeList; // 0x50
};
