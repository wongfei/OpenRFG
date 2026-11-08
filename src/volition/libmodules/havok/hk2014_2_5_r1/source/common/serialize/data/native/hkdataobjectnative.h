// Original: volition/libmodules/havok/hk2014_2_5_r1/source/common/serialize/data/native/hkdataobjectnative.h
//
// Havok 2014.2.5: full declaration from rfg.pdb (stub_impl.md S4, serializer); the array view hkDataArrayNative is in
// the .cpp (as the object and class wrappers are in the 2014.1 SDK).
#pragma once

#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/memory/memoryclasses/hkmemoryclasses.h"
#include <stdint.h>
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/container/array/hkarray.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/container/hkcontainerallocators.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/container/stringmap/hkstringmap.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/reflection/hkclassmember.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/reflection/hkclassmemberaccessor.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/reflection/registry/hkclassnameregistry.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/reflection/registry/hktypeinforegistry.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/reflection/registry/hkvtableclassregistry.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/types/hkbasetypes.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/types/hkrefptr.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/serialize/data/hkdataobject.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/serialize/data/hkdataobjectimpl.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/serialize/resource/hkobjectresource.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/serialize/typemanager/hktypemanager.h"

struct _hkDataObject_MemberHandle;
class hkClass;
struct hkDataClass_MemberInfo;
class hkDataWorldNative;

// size 0x1C
class hkDataObjectNative : public hkDataObjectImpl
{
public:
    hkDataObjectNative(const hkDataObject_Handle& h, hkDataWorldNative* world); // 0x00F588F0
    hkDataObjectNative(const hkVariant& v, hkDataWorldNative* world, hkBool hasOwnedRef); // 0x00F57E50
    virtual ~hkDataObjectNative(); // vtbl+0x00 0x00F58940
    virtual hkDataObject_Handle getHandle() const; // vtbl+0x04 0x00F57ED0
    virtual const hkDataClassImpl* getClass() const; // vtbl+0x08 0x00F57F00
    virtual hkDataObject_Value accessByName(const char* name); // vtbl+0x0C 0x00F57F30
    virtual unsigned int hasMember(const char* name) const; // vtbl+0x10 0x00F57F90
    virtual int getMemberIterator() const; // vtbl+0x14 0x00F58010
    virtual unsigned int isValid(int it) const; // vtbl+0x18 0x00F58020
    virtual int getNextMember(int it) const; // vtbl+0x1C 0x00F58040
    virtual const char* getMemberName(int it) const; // vtbl+0x20 0x00F58960
    virtual const hkDataObject_Value getMemberValue(int it) const; // vtbl+0x24 0x00F58050
    virtual hkDataArrayImpl* asArray(const _hkDataObject_MemberHandle* handle); // vtbl+0x28 0x00F59040
    virtual const char* asString(const _hkDataObject_MemberHandle* handle); // vtbl+0x2C 0x00F59300
    virtual int64_t asInt(const _hkDataObject_MemberHandle* handle); // vtbl+0x30 0x00F59340
    virtual hkDataObjectImpl* asObject(const _hkDataObject_MemberHandle* handle); // vtbl+0x34 0x00F59380
    virtual const float* asVec(const _hkDataObject_MemberHandle* handle, int nreal); // vtbl+0x38 0x00F59410
    virtual float asReal(const _hkDataObject_MemberHandle* handle); // vtbl+0x3C 0x00F59460
    virtual void assign(const _hkDataObject_MemberHandle* handle, int64_t valueIn); // vtbl+0x40 0x00F595C0
    virtual void assign(const _hkDataObject_MemberHandle* handle, int valueIn); // vtbl+0x44 0x00F59610
    virtual void assign(const _hkDataObject_MemberHandle* handle, const float* r, int nreal); // vtbl+0x48 0x00F59570
    virtual void assign(const _hkDataObject_MemberHandle* handle, hkHalf r); // vtbl+0x4C 0x00F59520
    virtual void assign(const _hkDataObject_MemberHandle* handle, float r); // vtbl+0x50 0x00F594E0
    virtual void assign(const _hkDataObject_MemberHandle* handle, const char* value); // vtbl+0x54 0x00F594A0
    virtual void assign(const _hkDataObject_MemberHandle* handle, hkDataArrayImpl* value); // vtbl+0x58 0x00F580A0
    virtual void assign(const _hkDataObject_MemberHandle* handle, hkDataObjectImpl* val); // vtbl+0x5C 0x00F58990
    virtual void assign(const _hkDataObject_MemberHandle* handle, const hkDataObject_Value& valueIn); // vtbl+0x60 0x00F58080
    virtual void getMemberInfo(const _hkDataObject_MemberHandle* handle, hkDataClass_MemberInfo& infoOut); // vtbl+0x64 0x00F59660
    virtual unsigned int isSet(const _hkDataObject_MemberHandle* handle); // vtbl+0x68 0x00F57FD0
    virtual unsigned int createdDuringPatching() const; // vtbl+0x6C 0x00F580B0
    virtual void destroy(); // vtbl+0x70 0x00F58070
    virtual void destroyMember(const char*); // vtbl+0x74 0x00F580C0
    virtual void getAllMemberHandles(hkArrayBase<_hkDataObject_MemberHandle const*>& handles) const; // vtbl+0x78 0x00F58980

    hkClassAccessor m_object; // 0x0C
    hkDataWorldNative* m_world; // 0x14
    hkBool m_hasOwnedRef; // 0x18
};

// size 0x14
class hkDataClassNative : public hkDataClassImpl
{
public:
    hkDataClassNative(const hkClass* k, hkDataWorldNative* w); // 0x00F57CC0
    virtual ~hkDataClassNative(); // vtbl+0x00 0x00F57DF0
    virtual const hkDataWorld* getWorld() const; // vtbl+0x04 0x00F57CF0
    virtual const char* getName() const; // vtbl+0x08 0x00F57D00
    virtual int getVersion() const; // vtbl+0x0C 0x00F57D10
    virtual hkDataClassImpl* getParent() const; // vtbl+0x10 0x00F57D20
    virtual hkBool isSuperClass(const hkDataClassImpl* k) const; // vtbl+0x14 0x00F57D50
    virtual int getNumDeclaredMembers() const; // vtbl+0x18 0x00F57DB0
    virtual void getDeclaredMemberInfo(int i, hkDataClass_MemberInfo& info) const; // vtbl+0x1C 0x00F58C90
    virtual int getDeclaredMemberIndexByName(const char* name) const; // vtbl+0x20 0x00F57DC0
    static void _fillMemberInfo(const hkDataClassImpl* clsIn, hkDataWorldNative* world, const hkClassMember& mem, hkDataClass_MemberInfo& info); // 0x00F58B70
    void fillMemberInfo(const hkClassMember& mem, hkDataClass_MemberInfo& info) const; // 0x00F58C70
    virtual int getNumMembers() const; // vtbl+0x24 0x00F57DD0
    virtual void getMemberInfo(int i, hkDataClass_MemberInfo& info) const; // vtbl+0x28 0x00F58CD0
    virtual int getMemberIndexByName(const char* name) const; // vtbl+0x2C 0x00F57DE0
    void _getAllMemberHandles(hkArrayBase<_hkDataObject_MemberHandle const*>& handles) const; // 0x00F580F0
    virtual void getAllMemberInfo(hkArrayBase<hkDataClass_MemberInfo>& infos) const; // vtbl+0x30 0x00F58D10

    const hkClass* m_class; // 0x0C
    hkDataWorldNative* m_world; // 0x10
};

// size 0xC4
class hkDataWorldNative : public hkDataWorld
{
public:
    HK_DECLARE_CLASS_ALLOCATOR(); // delete(void* p, unsigned int nbytes) 0x00F596D0
    hkDataWorldNative(hkBool accessSerializeIgnored); // 0x00F57860
    virtual ~hkDataWorldNative(); // vtbl+0x00 0x00F57940
    virtual hkDataClassImpl* newClass(const hkDataClass::Cinfo& cinfo); // vtbl+0x10 0x00F57390
    virtual hkDataObjectImpl* newObject(const hkDataClass& klass, bool) const; // vtbl+0x14 0x00F577A0
    virtual hkDataArrayImpl* newArray(hkDataObject& _obj, const _hkDataObject_MemberHandle* handle, const hkDataClass_MemberInfo& minfo) const; // vtbl+0x18 0x00F573A0
    virtual hkEnum<enum hkDataWorld::DataWorldType,int> getType() const; // vtbl+0x1C 0x00F57650
    virtual hkDataObject getContents() const; // vtbl+0x20 0x00F57660
    virtual void findAllClasses(hkArray<hkDataClassImpl*,hkContainerTempAllocator>& classesOut) const; // vtbl+0x24 0x00F57AE0
    virtual hkDataClassImpl* findClass(const char* name) const; // vtbl+0x28 0x00F57530
    virtual hkDataObject findObject(const hkDataObject_Handle& handle) const; // vtbl+0x2C 0x00F575C0
    hkDataObjectImpl* wrapObject(void* object, const hkClass& klass) const; // 0x00F57500
    virtual hkTypeManager& getTypeManager(); // vtbl+0x30 0x00F596C0
    void setContents(void* object, const hkClass& klass); // 0x00F57730
    void setClassRegistry(const hkClassNameRegistry* r); // 0x00F576D0
    void setVtableRegistry(const hkVtableClassRegistry* vt); // 0x00F57700
    virtual hkTypeManager::Type* getTypeFromMemberType(hkClassMember::Type mtype, hkClassMember::Type stype, const hkClass* klass, int count); // vtbl+0x64 0x00F57360

    hkRefPtr<hkClassNameRegistry const > m_reg; // 0x08
    hkRefPtr<hkVtableClassRegistry const > m_vtable; // 0x0C
    hkRefPtr<hkTypeInfoRegistry const > m_infoReg; // 0x10
    mutable hkStringMap<hkDataClassNative*,hkContainerHeapAllocator> m_classes; // 0x14
    hkVariant m_contents; // 0x24
    hkBool m_accessSerializeIgnored; // 0x2C
    hkTypeManager m_typeManager; // 0x30
    hkArray<unsigned char,hkContainerHeapAllocator> m_buffer; // 0xB8
};

// Wraps a native object of the world (nullptr for an unregistered class).
hkDataObjectImpl* hkNativeDataObjectImplCreate(hkVariant& v, hkDataWorldNative* w, hkBool isRefOwned); // 0x00F573B0
