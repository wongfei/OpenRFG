// Original: volition/libmodules/havok/hk2014_2_5_r1/source/common/serialize/data/dict/hkdataobjectdict.h
//
// Havok 2014.2.5: full declaration from rfg.pdb (stub_impl.md S4, serializer); ObjectTracker moved to the .cpp
// by hand (nobuild).
#pragma once

#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/memory/memoryclasses/hkmemoryclasses.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/container/array/hkarray.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/container/hkcontainerallocators.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/container/pointermap/hkpointermap.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/container/stringmap/hkstringmap.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/types/hkbasetypes.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/serialize/data/hkdataobject.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/serialize/typemanager/hktypemanager.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/serialize/util/hkserializemultimap.h"

struct _hkDataObject_MemberHandle;
class hkClass;
class hkDataArrayImpl;
class hkDataClassImpl;
struct hkDataClass_MemberInfo;
class hkDataObjectImpl;
class hkMemoryAllocator;

class hkDataClassDict;
class hkDataObjectDict;

// size 0x98
class hkDataWorldDict : public hkDataWorld
{
public:
    // size 0x7C: the class and object registry of the world, defined in hkdataobjectdict.cpp (its members use the
    // interned string types of that file, as in the 2014.1 SDK).
    class ObjectTracker;

    HK_DECLARE_CLASS_ALLOCATOR(); // delete(void* p, unsigned int nbytes) 0x00F51070
    hkDataWorldDict(hkMemoryAllocator* mall); // 0x00F483D0
    hkDataWorldDict(); // 0x00F48360
    virtual ~hkDataWorldDict(); // vtbl+0x00 0x00F48430
    virtual hkDataClassImpl* newClass(const hkDataClass::Cinfo& cinfo); // vtbl+0x10 0x00F47DD0
    virtual hkDataObjectImpl* newObject(const hkDataClass& klass, bool createdDuringPatching) const; // vtbl+0x14 0x00F47D30
    virtual hkDataArrayImpl* newArray(hkDataObject& obj, const _hkDataObject_MemberHandle* handle, const hkDataClass_MemberInfo& minfo) const; // vtbl+0x18 0x00F47230
    virtual hkEnum<enum hkDataWorld::DataWorldType,int> getType() const; // vtbl+0x1C 0x00F46D80
    virtual hkDataObject getContents() const; // vtbl+0x20 0x00F46D20
    virtual void findAllClasses(hkArray<hkDataClassImpl*,hkContainerTempAllocator>& classesOut) const; // vtbl+0x24 0x00F46EB0
    virtual hkDataClassImpl* findClass(const char* name) const; // vtbl+0x28 0x00F46D90
    hkDataClassImpl* wrapClass(const hkClass& klass); // 0x00F47F60
    virtual hkDataObject findObject(const hkDataObject_Handle& handle) const; // vtbl+0x2C 0x00F46D40
    virtual hkTypeManager& getTypeManager(); // vtbl+0x30 0x00F3DBA0
    virtual void renameClass(hkDataClass& klass, const char* newName); // vtbl+0x34 0x00F46E30
    virtual void removeClass(hkDataClass& klass); // vtbl+0x38 0x00F48200
    virtual void setClassVersion(hkDataClass& klass, int newVersion); // vtbl+0x3C 0x00F46D60
    virtual void setClassParent(hkDataClass& klass, hkDataClass& parent); // vtbl+0x40 0x00F48250
    virtual void addClassMember(hkDataClass& klass, const char* name, hkTypeManager::Type* type, const void* valuePtr); // vtbl+0x44 0x00F46EC0
    virtual void setClassMemberDefault(hkDataClass& klass, const char* name, const void* valuePtr); // vtbl+0x48 0x00F46DB0
    virtual void renameClassMember(hkDataClass& klass, const char* oldName, const char* newName); // vtbl+0x4C 0x00F472A0
    virtual void removeClassMember(hkDataClass& klass, const char* name); // vtbl+0x50 0x00F474B0
    virtual void findObjectsByBaseClass(const char* className, hkArray<hkDataObjectImpl*,hkContainerTempAllocator>& objectsOut) const; // vtbl+0x54 0x00F47710
    virtual void findObjectsByExactClass(const char* className, hkArray<hkDataObjectImpl*,hkContainerTempAllocator>& objectsOut) const; // vtbl+0x58 0x00F476F0
    virtual void castObject(hkDataObject& obj, const hkDataClass& castClass); // vtbl+0x5C 0x00F482B0
    virtual hkDataClassImpl* copyClassFromWorld(const char* name, const hkDataWorld& worldFrom); // vtbl+0x60 0x00F48480

    ObjectTracker* m_tracker; // 0x08
    hkMemoryAllocator* m_allocator; // 0x0C
    hkTypeManager m_typeManager; // 0x10
};
