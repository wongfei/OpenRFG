// Original: volition/libmodules/havok/hk2014_2_5_r1/source/common/compat/deprecated/packfile/xml/hkxmlpackfilereader.h
//
// Havok 2014.2.5: full declaration from rfg.pdb (stub_impl.md S4, serializer).
#pragma once

#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/memory/memoryclasses/hkmemoryclasses.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/container/array/hkarray.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/container/hkcontainerallocators.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/container/pointermap/hkpointermap.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/container/stringmap/hkstringmap.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/types/hkrefptr.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/compat/deprecated/packfile/hkpackfilereader.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/compat/deprecated/version/hkpackfileobjectupdatetracker.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/serialize/resource/hkobjectresource.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/serialize/util/hkchainedclassnameregistry.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/serialize/util/hkserializedeprecated.h"

class hkClass;
class hkClassNameRegistry;
class hkObjectUpdateTracker;
class hkPackfileData;
class hkRelocationInfo;
struct hkResult;
class hkStreamReader;
class hkTypeInfoRegistry;
class hkXmlPackfileUpdateTracker;

// size 0x4C
class hkXmlPackfileReader : public hkPackfileReader
{
public:
    HK_DECLARE_CLASS_ALLOCATOR(); // delete(void* p, unsigned int nbytes) 0x00FAB360
    hkXmlPackfileReader(); // 0x00FA8E00
    virtual ~hkXmlPackfileReader(); // vtbl+0x00 0x00FA8F20
    hkResult readHeader(hkStreamReader* stream, hkSerializeDeprecated::XmlPackfileHeader& out); // 0x00FA9430
    virtual hkResult loadEntireFile(hkStreamReader* rawStream); // vtbl+0x10 0x00FA8950
    virtual void* getContentsWithRegistry(const char* expectedClassName, const hkTypeInfoRegistry* finishRegistry); // vtbl+0x14 0x00FA9380
    virtual const char* getContentsClassName() const; // vtbl+0x1C 0x00FA8970
    virtual hkArray<hkVariant,hkContainerHeapAllocator>& getLoadedObjects() const; // vtbl+0x20 0x00FA8890
    virtual hkVariant getTopLevelObject() const; // vtbl+0x24 0x00FA92F0
    virtual hkObjectUpdateTracker& getUpdateTracker() const; // vtbl+0x28 0x00FA88A0
    virtual const hkClassNameRegistry* getClassNameRegistry() const; // vtbl+0x34 0x00FA8C40
    virtual hkPackfileData* getPackfileData() const; // vtbl+0x38 0x00FA9220
    virtual hkResult loadEntireFileWithRegistry(hkStreamReader* rawStream, const hkClassNameRegistry* originalReg); // vtbl+0x3C 0x00FA9630
public:
    const hkClass* getClassByName(const char* className, hkStringMap<hkClass*,hkContainerHeapAllocator>& partiallyLoadedClasses, hkPointerMap<hkClass const*,int,hkContainerHeapAllocator>& offsetsRecomputed, int classVersion, const char* contentsVersion) const; // 0x00FA8BA0
    void handleInterObjectReferences(const char* newObjectName, void* newObject, const hkRelocationInfo& reloc, const hkStringMap<void*,hkContainerHeapAllocator>& nameToObject, hkStringMap<int,hkContainerHeapAllocator>& unresolvedReferences); // 0x00FA8FD0
    void useClassesFromRegistry(const hkClassNameRegistry& registry) const; // 0x00FA9110

    hkPackfileData* m_data; // 0x18
    hkArray<char const*,hkContainerHeapAllocator> m_knownSections; // 0x1C
    hkStringMap<int,hkContainerHeapAllocator> m_sectionTagToIndex; // 0x28
    hkArray<hkVariant,hkContainerHeapAllocator> m_loadedObjects; // 0x38
    hkXmlPackfileUpdateTracker* m_tracker; // 0x44
    hkRefPtr<hkChainedClassNameRegistry> m_packfileClassRegistry; // 0x48
};

// size 0x40
class hkXmlPackfileUpdateTracker : public hkPackfileObjectUpdateTracker
{
    void setVariantClassPointers(hkVariant* v, int numVariants, const hkPointerMap<void const*,hkClass const*,hkContainerHeapAllocator>& classFromObject); // 0x00FAACB0
    void retrackVariantAndHomogeneousClasses(void* pointer, const hkClass& klass, const hkPointerMap<void const*,hkClass const*,hkContainerHeapAllocator>& classFromObject, int numObjs); // 0x00FAADA0
public:
    hkXmlPackfileUpdateTracker(hkPackfileData* data, hkRefPtr<hkChainedClassNameRegistry>& packfileClassRegistry); // 0x00FAA730
    unsigned int isDirty(); // 0x00FAA760
    void setDirty(); // 0x00FAABA0
    virtual ~hkXmlPackfileUpdateTracker(); // vtbl+0x00 0x00FAA770
    virtual void replaceObject(void* oldObject, void* newObject, const hkClass* newClass); // vtbl+0x1C 0x00FAABC0
    int addPendingValue(void* address, int oldIndex); // 0x00FAB340
    void realizePendingPointer(void* newObject, int startIndex); // 0x00FAB2E0
    void fixAndTrackVariantAndHomogeneousClasses(hkArray<hkVariant,hkContainerHeapAllocator>& objects, const hkPointerMap<void const*,hkClass const*,hkContainerHeapAllocator>& classFromObject); // 0x00FAB040
    virtual void removeFinish(void* oldObject); // vtbl+0x24 0x00FAAC00

    unsigned int m_dirty; // 0x38
    hkRefPtr<hkChainedClassNameRegistry>& m_packfileClassRegistry; // 0x3C
};
