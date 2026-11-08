// Original: volition/libmodules/havok/hk2014_2_5_r1/source/common/serialize/data/hkdataobjectdeclarations.h
//
// Havok 2014.2.5 (stub_impl.md S4, serializer SC): the small types the data object headers share, as the 2014.1 SDK's
// hkDataObjectDeclarations.h (layouts checked against rfg.pdb).
#pragma once

#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/memory/memoryclasses/hkmemoryclasses.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/serialize/typemanager/hktypemanager.h"

class hkDataObject;
class hkDataArray;
class hkDataClass;
class hkDataWorld;

class hkDataObjectImpl;
class hkDataArrayImpl;
class hkDataClassImpl;

// Opaque handle for accessing members of an object
struct _hkDataObject_MemberHandle;
typedef const _hkDataObject_MemberHandle* hkDataObject_MemberHandle;
typedef int hkDataObject_Iterator;
typedef hkTypeManager::Type* hkDataObject_Type;

class hkDataObject_Value;
struct hkStridedBasicArray;

// size 0x10
struct hkDataClass_MemberInfo
{
    HK_DECLARE_NONVIRTUAL_CLASS_ALLOCATOR(hkDataClass_MemberInfo);
    hkDataClass_MemberInfo() : m_name(nullptr), m_owner(nullptr), m_type(nullptr), m_valuePtr(nullptr) {} // 0x00F3DA90

    const char* m_name; // 0x00
    const hkDataClassImpl* m_owner; // 0x04
    hkTypeManager::Type* m_type; // 0x08
    const void* m_valuePtr; // 0x0C: the default value, or nullptr
};

// size 0x8
// The persistent identity of an object in a world (p0 null: the null object).
struct hkDataObject_Handle
{
    HK_DECLARE_NONVIRTUAL_CLASS_ALLOCATOR(hkDataObject_Handle);
    unsigned int operator==(const hkDataObject_Handle& h) const { return p0 == h.p0 && p1 == h.p1; } // 0x00F663D0
    unsigned int operator!=(const hkDataObject_Handle& h) const { return p0 != h.p0 || p1 != h.p1; } // 0x00F5C430

    void* p0; // 0x00
    void* p1; // 0x04
};
