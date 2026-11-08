// Original: volition/libmodules/havok/hk2014_2_5_r1/source/common/compat/deprecated/packfile/hkpackfilereader.h
//
// Havok 2014.2.5: full declaration from rfg.pdb (stub_impl.md S4, serializer).
#pragma once

#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/memory/memoryclasses/hkmemoryclasses.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/container/array/hkarray.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/container/hkcontainerallocators.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/container/pointermap/hkpointermap.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/object/hkreferencedobject.h"

class hkClass;
class hkClassNameRegistry;
class hkObjectUpdateTracker;
class hkPackfileData;
struct hkResult;
class hkStreamReader;
class hkTypeInfoRegistry;
struct hkVariant;

// size 0x18
class hkPackfileReader : public hkReferencedObject
{
public:
    typedef hkPackfileData AllocatedData;
    typedef const char* SectionTag;
    typedef hkPointerMap<hkClass const*,int,hkContainerHeapAllocator> UpdateFlagFromClassMap;

    HK_DECLARE_CLASS_ALLOCATOR(); // delete(void* p, unsigned int nbytes) 0x00FB3650
    hkPackfileReader(); // 0x00FB3620
    virtual ~hkPackfileReader(); // vtbl+0x00 0x00FB3590
    virtual hkResult loadEntireFile(hkStreamReader*) = 0; // vtbl+0x10
    virtual void* getContentsWithRegistry(const char*, const hkTypeInfoRegistry*) = 0; // vtbl+0x14
    virtual void* getContents(const char* className); // vtbl+0x18 0x00FB3560
    virtual const char* getContentsClassName() const = 0; // vtbl+0x1C
    virtual hkArray<hkVariant,hkContainerHeapAllocator>& getLoadedObjects() const = 0; // vtbl+0x20
    virtual hkVariant getTopLevelObject() const = 0; // vtbl+0x24
    virtual hkObjectUpdateTracker& getUpdateTracker() const = 0; // vtbl+0x28
    virtual const char* getContentsVersion() const; // vtbl+0x2C 0x00FB34A0
    virtual void setContentsVersion(const char* ver); // vtbl+0x30 0x00FB35E0
    virtual const hkClassNameRegistry* getClassNameRegistry() const = 0; // vtbl+0x34
    virtual hkPackfileData* getPackfileData() const = 0; // vtbl+0x38
    hkPackfileData* getAllocatedData() const;
    unsigned int isVersionUpToDate() const; // 0x00FB34B0
    void warnIfNotUpToDate() const; // 0x00FB34D0
    static void updateMetaDataInplace(hkClass* classInOut, int fileVersion, const char* contentsVersion, hkPointerMap<hkClass const*,int,hkContainerHeapAllocator>& updateFlagFromClass); // 0x00FB3480

    hkPointerMap<hkClass const*,int,hkContainerHeapAllocator> m_updateFlagFromClass; // 0x08
    char* m_contentsVersion; // 0x14
};
