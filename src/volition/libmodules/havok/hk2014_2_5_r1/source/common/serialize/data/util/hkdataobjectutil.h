// Original: volition/libmodules/havok/hk2014_2_5_r1/source/common/serialize/data/util/hkdataobjectutil.h
//
// Havok 2014.2.5: full declaration from rfg.pdb (stub_impl.md S4, serializer).
#pragma once

#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/reflection/hkclassmember.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/serialize/typemanager/hktypemanager.h"

class hkClass;
class hkClassNameRegistry;
class hkDataObject;
class hkDataWorld;
class hkObjectResource;
class hkResource;
struct hkResult;
class hkTypeInfoRegistry;

namespace hkDataObjectUtil
{
    hkObjectResource* toObject(const hkDataObject& src, hkBool32 destroyOnCopy); // 0x00F66970
    hkObjectResource* toObjectWithRegistry(const hkDataObject& src, const hkClassNameRegistry* classReg, const hkTypeInfoRegistry* typeReg, hkBool32 destroyOnCopy); // 0x00F66820
    hkResource* toResource(const hkDataObject& src, hkBool32 destroyOnCopy); // 0x00F669A0
    hkResource* toResourceWithRegistry(const hkDataObject& src, const hkClassNameRegistry* classReg, hkBool32 destroyOnCopy); // 0x00F66860
    hkTypeManager::Type* getBasicType(hkTypeManager& typeManager, hkClassMember::Type type, const char* typeName); // 0x00F66510
    hkTypeManager::Type* getTypeFromMemberTypeClassName(hkTypeManager& typeManager, hkClassMember::Type mtype, hkClassMember::Type stype, const char* typeName, int count); // 0x00F665C0
    hkTypeManager::Type* getTypeFromMemberType(hkTypeManager& typeManager, hkClassMember::Type mtype, hkClassMember::Type stype, const hkClass* klass, int count); // 0x00F666C0
    hkResult deepCopyWorld(hkDataWorld& dst, const hkDataWorld& src); // 0x00F667E0
    int getExtraStorageSize(const hkClass* klass, const hkDataObject& obj); // 0x00F66710
}
