// Original: library/modules/runtime/resources/include/keen/resources/resourcemanager.hpp
// Implementation: library/modules/runtime/resources/sources/resourcemanager.cpp
//
// ResourceManager: loads krof files through the KrofManager, keeps resource entries by key, runs the registered
// factories (FactoryEntry) and handles load requests, references and unloading. Placement of the entry and init
// parameter types inferred.
#pragma once

#include "keen/base/types.hpp"
#include "keen/base/array.hpp"
#include "keen/base/debugcommand.hpp"
#include "keen/base/internallist.hpp"
#include "keen/base/memoryallocator.hpp"
#include "keen/base/resourceconfiguration.hpp"
#include "keen/base/tlsfmemoryallocator.hpp"
#include "keen/base/win32/event_win32.hpp"
#include "keen/base/win32/mutex_win32.hpp"
#include "keen/base/win32/thread_win32.hpp"
#include "keen/resources/fallbackresource.hpp"
#include "keen/resources/resourceallocationcontexts.hpp"
#include "keen/resources/resourcerequest.hpp"

namespace keen
{
    struct FileSystem;
    class KrofManager;
    class Resource;
    class ResourceFactory;
    struct ResourceManagerInitParams;

    // size 0x2DC
    class ResourceManager : private DebugCommandHandler
    {
    public:
        ResourceManager(); // 0x00CDFBA0 (:42)
        ~ResourceManager(); // 0x00CDE4E0 (:51)
        void initialize(MemoryAllocator* pAllocator, const ResourceManagerInitParams& parameters); // 0x00CE27E0 (:55)
        void shutdown(MemoryAllocator* pAllocator); // 0x00CE02F0 (:125)
        void abortLoadingForExit(); // 0x00CDCD80 (:174)
        void addLoadResourceRequest(ResourceRequest* pRequest, bool waitUntilLoaded); // 0x00CE29C0 (:226)
        void addReleaseResourceRequest(void* handle); // 0x00CDE600 (:512)
        void addReleaseResourceRequest(void* handle, uint32 type); // 0x00CDEEA0 (:506)
        void abortResourceRequest(ResourceRequest* pRequest); // 0x00CE2A60 (:428)
        bool isIdle() const; // 0x00CDCDD0 (:471)
        void waitUntilLoadingIsFinished(ResourceRequest* pRequest); // 0x00CE2620 (:449)
        void waitUntilAllRequestsDone(); // 0x00CE2660 (:479)
        void waitUntilAllReleaseRequestsDone(); // 0x00CE26E0 (:500)
        void killRequest(ResourceRequest* pRequest); // 0x00CE2B00 (:522)
        void setResourceContext(uint32 index, uint32 parentIndex, const ResourceAllocationContext* pAllocationContext, uint32 resourceTag); // 0x00CDE5B0 (:198)
        void setResourceContext(uint32 index, uint32 parentIndex, const ResourceAllocationContext* pAllocationContext); // 0x00CDEE50 (:193)
        ResourceAllocationContext* getDefaultResourceAllocationContext();
        const ResourceAllocationContext* getDefaultResourceAllocationContext() const;
        void getCurrentRequestName(char* pBuffer, uint32 capacity); // 0x00CDCE20 (:542)
        void update(); // 0x00CE25F0 (:404)
        void sleep(); // 0x00CDCDC0 (:421)
        void lock(); // 0x00CDCE90 (:570)
        void unlock(); // 0x00CDCEA0 (:575)
    private:
        void renameResource(uint32 type, const char* pOldName, const char* pNewName); // 0x00CDE640 (:558)
    public:
        void unloadUnreferenced(); // 0x00CE0700 (:675)
        bool addResourceType(ResourceFactory* pFactory); // 0x00CDE680 (:653)
        void removeResourceType(ResourceFactory* pFactory); // 0x00CE2750 (:664)
        ResourceFactory* removeResourceType(uint32 resourceType); // 0x00CE2710 (:658)
        ResourceFactory* getResourceFactoryForType(uint32 resourceType); // 0x00CDE6B0 (:670)
        Resource* findResource(uint32, uint32, uint32);
    private:
        void unloadResources(); // 0x00CDCE70 (:564)
        void updateReleaseResource(); // 0x00CE0480 (:255)
        bool updateLoadResource(); // 0x00CE2440 (:280)
        void updateFlipResourceContexts();

        KrofManager* m_pKrofManager; // 0x00
        ResourceAllocationContext* m_pDefaultResourceAllocationContext; // 0x04
        Array<ResourceContextDescriptor> m_resourceContexts; // 0x08
        AutoMemoryBlock m_krofMemory; // 0x10
        AutoMemoryBlock m_krofScratchMemory; // 0x1C
        TlsfMemoryAllocator m_krofAllocator; // 0x28
        TlsfMemoryAllocator m_krofScratchAllocator; // 0x108
        Thread m_loadingThread; // 0x1E8
        Event m_requestEvent; // 0x23C
        Mutex m_requestMutex; // 0x240
        Mutex m_krofMutex; // 0x278
        FallbackResourceFactory m_fallbackFactory; // 0x2B0
        InternalList<ResourceRequest> m_resourceRequests; // 0x2B8
        const char* m_pCurrentRequestName; // 0x2D0
        uint32 m_checkReleasedResourceCounter; // 0x2D4
        bool m_runAsynchronous; // 0x2D8
        volatile bool m_isUpdating; // 0x2D9
        bool m_resolveReferences; // 0x2DA
    };

    // size 0x8
    struct FactoryEntry
    {
        ResourceFactory* pFactory; // 0x00
        uint32 resourceType; // 0x04
    };

    // size 0x8
    struct ResourceEntry
    {
        Resource* pResource; // 0x00
        uint32 resourceType; // 0x04
    };

    // size 0x38
    struct ResourceManagerInitParams
    {
        ResourceManagerInitParams();

        ResourceConfigurationCompatibility supportedResourceConfigurations; // 0x00
        uint32 krofAllocatorSize; // 0x10
        uint32 krofScratchAllocatorSize; // 0x14
        uint32 numberOfResourceTypes; // 0x18
        uint32 numberOfFiles; // 0x1C
        uint32 numberOfAdditionalAllocations; // 0x20
        uint32 resourceContextCount; // 0x24
        FileSystem* pFileSystem; // 0x28
        const char* pBasePath; // 0x2C
        bool runAsynchronous; // 0x30
        bool resolveReferences; // 0x31
        bool useDebugCommands; // 0x32
        bool useFallbackFactory; // 0x33
        ResourceAllocationContext* pDefaultAllocationContext; // 0x34
    };
}
