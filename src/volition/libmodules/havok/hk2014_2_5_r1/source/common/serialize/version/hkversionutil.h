// Original: volition/libmodules/havok/hk2014_2_5_r1/source/common/serialize/version/hkversionutil.h
//
// Havok 2014.2.5: full declaration from rfg.pdb (stub_impl.md S4, serializer).
#pragma once

#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/container/array/hkarray.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/compat/deprecated/version/hkversionregistry.h"

class hkClass;
class hkClassNameRegistry;
struct hkContainerHeapAllocator;
class hkObjectUpdateTracker;
class hkOstream;
class hkPackfileReader;
struct hkResult;
struct hkVariant;

namespace hkVersionUtil
{
    const char* getDeprecatedCurrentVersion(); // 0x00FA3F10
    void updateVariantClassPointers(void* pointer, const hkClass& klass, hkClassNameRegistry& reg, int numObjs); // 0x00FA44D0
    void* copyObjects(const hkArrayBase<hkVariant>& objectsIn, hkArray<hkVariant,hkContainerHeapAllocator>& objectsOut, hkObjectUpdateTracker& tracker); // 0x00FA4F20
    hkResult updateSingleVersion(hkArray<hkVariant,hkContainerHeapAllocator>& objectsInOut, hkObjectUpdateTracker& tracker, const hkVersionRegistry::UpdateDescription& updateDescription, const hkClassNameRegistry* newClassRegistry); // 0x00FA4FE0
    hkResult updateBetweenVersions(hkArray<hkVariant,hkContainerHeapAllocator>& objectsInOut, hkObjectUpdateTracker& tracker, const hkVersionRegistry& reg, const char* fromVersion, const char* toVersion); // 0x00FA5BB0
    hkResult updateToVersion(hkPackfileReader& reader, const hkVersionRegistry& reg, const char* targetVersion); // 0x00FA5CB0
    hkResult updateToCurrentVersion(hkPackfileReader& reader, const hkVersionRegistry& reg); // 0x00FA5D40
    hkResult generateCppExternClassList(hkOstream& os, const char* headerMacro, const hkClass* const* classesToGenerate, const char* registryVariableName); // 0x00FA5D70
    hkResult generateCppClassList(hkOstream& os, const hkClass* const* classesToGenerate, const char* pchfilename, const char* registryVariableName); // 0x00FA6E90
    const char* getCurrentVersion(); // 0x00F658B0
    void renameMember(hkVariant& oldObj, const char* oldName, hkVariant& newObj, const char* newName); // 0x00F658C0
    void copyDefaults(void* obj, const hkClass& oldClass, const hkClass& newClass); // 0x00F65940
    void recomputeClassMemberOffsets(hkClass* const* classes, int classVersion); // 0x00F659E0
}
