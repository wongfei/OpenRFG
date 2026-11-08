// Original: volition/libmodules/havok/hk2014_2_5_r1/source/common/compat/deprecated/version/hkversionregistry.h
//
// Havok 2014.2.5: full declaration from rfg.pdb (stub_impl.md S4, serializer).
#pragma once

#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/memory/memoryclasses/hkmemoryclasses.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/container/array/hkarray.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/container/hkcontainerallocators.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/container/stringmap/hkstringmap.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/object/hkreferencedobject.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/object/hksingleton.h"

class hkClassNameRegistry;
class hkDynamicClassNameRegistry;
class hkObjectUpdateTracker;
struct hkResult;
class hkStaticClassNameRegistry;
struct hkVariant;

// size 0x24
class hkVersionRegistry : public hkReferencedObject, public hkSingleton<hkVersionRegistry>
{
public:
    enum SignatureFlags
    {
        AUTO_SIGNATURE = -1,
    };

    enum VersionFlags
    {
        VERSION_MANUAL = 2,
        VERSION_INPLACE = 4,
        VERSION_COPY = 8,
        VERSION_REMOVED = 16,
        VERSION_VARIANT = 32,
        VERSION_HOMOGENEOUSARRAY = 64,
    };

    typedef void (*VersionFunc)(hkVariant&, hkVariant&, hkObjectUpdateTracker&);
    typedef hkResult (*UpdateFunction)(hkArray<hkVariant,hkContainerHeapAllocator>&, hkObjectUpdateTracker&);

    // size 0x8
    struct ClassRename
    {
        HK_DECLARE_NONVIRTUAL_CLASS_ALLOCATOR(ClassRename);

        const char* oldName; // 0x00
        const char* newName; // 0x04
    };

    // size 0x14
    struct ClassAction
    {
        HK_DECLARE_NONVIRTUAL_CLASS_ALLOCATOR(ClassAction);

        unsigned int oldSignature; // 0x00
        unsigned int newSignature; // 0x04
        int versionFlags; // 0x08
        const char* oldClassName; // 0x0C
        void (*versionFunc)(hkVariant&, hkVariant&, hkObjectUpdateTracker&); // 0x10
    };

    // size 0x10
    struct UpdateDescription
    {
        UpdateDescription(const ClassRename* renames, const ClassAction* actions, const hkClassNameRegistry* staticNewClassRegistry); // 0x00F858C0
        HK_DECLARE_NONVIRTUAL_CLASS_ALLOCATOR(UpdateDescription);

        const ClassRename* m_renames; // 0x00
        const ClassAction* m_actions; // 0x04
        const hkClassNameRegistry* m_newClassRegistry; // 0x08
        UpdateDescription* m_next; // 0x0C
    };

    // size 0x10
    struct Updater
    {
        HK_DECLARE_NONVIRTUAL_CLASS_ALLOCATOR(Updater);

        const char* fromVersion; // 0x00
        const char* toVersion; // 0x04
        UpdateDescription* desc; // 0x08
        hkResult (*optionalCustomFunction)(hkArray<hkVariant,hkContainerHeapAllocator>&, hkObjectUpdateTracker&); // 0x0C
    };

    HK_DECLARE_CLASS_ALLOCATOR(); // new(unsigned int nbytes) 0x00FA3850, delete(void* p, unsigned int nbytes) 0x00FA3D50
    hkVersionRegistry(); // 0x00FA2B10
    virtual ~hkVersionRegistry(); // vtbl+0x00 0x00FA2B60
    void registerUpdater(const Updater* updater); // 0x00FA2450
    hkResult getVersionPath(const char* fromVersion, const char* toVersion, hkArray<Updater const*,hkContainerTempAllocator>& pathOut) const; // 0x00FA2490
    const hkClassNameRegistry* getClassNameRegistry(const char* versionString) const; // 0x00FA3270
    hkResult registerStaticClassRegistry(const hkStaticClassNameRegistry& staticReg); // 0x00FA2F40
    hkResult registerUpdateDescription(UpdateDescription& updateDescription, const char* fromVersion, const char* toVersion); // 0x00FA2F90
    hkDynamicClassNameRegistry* getDynamicClassNameRegistry(const char* versionString) const; // 0x00FA2C60

    hkArray<Updater const*,hkContainerHeapAllocator> m_updaters; // 0x08
    static const Updater* StaticLinkedUpdaters[]; // 0x0165FCD8
    static const hkStaticClassNameRegistry* StaticLinkedClassRegistries[]; // 0x0165FD58
public:
    hkStringMap<hkDynamicClassNameRegistry*,hkContainerHeapAllocator> m_versionToClassNameRegistryMap; // 0x14
};
