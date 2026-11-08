// Original: library/modules/runtime/resources/sources/krofmanager.hpp
// Implementation: library/modules/runtime/resources/sources/krofmanager.cpp
//
// KrofManager, loads krof files: chunks, sections and relocations, with load failure reports. Placement of
// LoadFailReason and KrofFileEntry inferred.
#pragma once

#include "keen/base/types.hpp"
#include "keen/base/chunkedsizedarray.hpp"
#include "keen/base/memorytracker.hpp"
#include "keen/base/resourceconfiguration.hpp"
#include "keen/base/sizedarray.hpp"
#include "keen/base/staticarray.hpp"
#include "keen/resources/initializationdatareadstream.hpp"
#include "keen/resources/resourcemanager.hpp"
#include "keen/resources/resourceobjectformat.hpp"

namespace keen
{
    class File;
    struct FileSystem;
    struct LoadFailReport;
    class MemoryAllocator;
    class Resource;
    struct ResourceAllocationContext;
    struct ResourceContextDescriptor;
    class ResourceFactory;
    struct WeakResourceReferenceHolder;

    enum LoadFailReason
    {
        LoadFailReason_None = 0,
        LoadFailReason_Unknown = 1,
        LoadFailReason_FileNotFound = 2,
        LoadFailReason_FileCorrupt = 3,
        LoadFailReason_Memory = 4,
        LoadFailReason_Factory = 5,
        LoadFailReason_ResourceInitialize = 6,
        LoadFailReason_InvalidContext = 7,
        LoadFailReason_Abort = 8,
    };

    // size 0x18
    struct SectionHeaderRuntimeData : public SectionHeader
    {
        SectionHeaderRuntimeData(); // 0x00CDC4D0 (.h:98)
        SectionHeaderRuntimeData(SectionHeader other); // 0x00CDC4A0 (.h:89)

        uint32 sectionOffset; // 0x10
        uint32 chunkIndex; // 0x14
    };

    // size 0x1C
    struct KrofFileEntry
    {
        KrofFileEntry(const KrofFileEntry& __that); // 0x00CDE910
        KrofFileEntry(); // 0x00CDE0B0
        ~KrofFileEntry(); // 0x00CDE0C0
        KrofFileEntry& operator=(const KrofFileEntry& __that); // 0x00CDEA00

        ExternalReferences externalReferences; // 0x00
        StaticArray<AllocationEntry> allocationEntries; // 0x08
        ResourceEntry* pResources; // 0x10
        uint32 resourceEntryCount; // 0x14
        uint32 resourceContextIndex; // 0x18
    };

    // size 0x988
    class KrofManager
    {
    public:
        enum
        {
            MaxReadStreamCount = 1,
        };

        // size 0x8
        struct ChunkInfo
        {
            uint32 startSectionIndex; // 0x00
            uint32 endSectionIndex; // 0x04
        };

        // size 0xA60
        struct KrofLoadContext
        {
            KrofLoadContext(); // 0x00CDD870 (:91)

            const ResourceContextDescriptor* pResourceContext; // 0x00
            uint32 resourceContextIndex; // 0x04
            char fileName[256]; // 0x08
            ResourceObjectFileHeader fileHeader; // 0x108
            ChunkInfo chunks[20]; // 0x110
            SectionHeaderRuntimeData sections[64]; // 0x1B0
            void* pSectionData[64]; // 0x7B0
            uint32 sectionCount; // 0x8B0
            uint32 currentSectionIndex; // 0x8B4
            ResourceReferenceSectionHeader* resReferencesSections[20]; // 0x8B8
            uint32 resReferencesSectionChunkIndex[20]; // 0x908
            ResourceReferenceSectionHeader* weakResReferencesSections[20]; // 0x958
            uint32 weakResReferenceSectionIndicies[20]; // 0x9A8
            uint32 resReferenceSectionCount; // 0x9F8
            uint32 weakResReferenceSectionCount; // 0x9FC
            ExternalReferences externalReferences; // 0xA00
            KrofFileEntry* pKrofFileEntry; // 0xA08
            Resource* pResourceToUse; // 0xA0C
            LoadFailReport* pLoadFailReport; // 0xA10
            uint64 startPosition; // 0xA18
            bool convertDataEndianness[64]; // 0xA20
        };

        KrofManager(); // 0x00CDEAE0 (:108)
        ~KrofManager(); // 0x00CDD8F0 (:119)
        void create(MemoryAllocator* pAllocator, MemoryAllocator* pScratchAllocator, uint32 numberOfResourceTypes, uint32 numberOfFiles, uint32 numberOfAdditionalAllocations, FileSystem* pFileSystem, const char* pBasePath); // 0x00CDD900 (:125)
        void destroy(); // 0x00CDFDE0 (:141)
        void setSupportedConfiguration(const ResourceConfigurationCompatibility& configuration); // 0x00CDC5E0 (:165)
        void setResourceContexts(const ResourceContextDescriptor* pContextDescriptor, uint32 numDescriptors); // 0x00CDC600 (:170)
        void setFileSystem(FileSystem* pFileSystem, const char* pBasePath); // 0x00CDC620 (:182)
        FileSystem* getFileSystem(); // 0x00CDC4F0 (.h:145)
        MemoryAllocator* getScratchAllocator(); // 0x00CDC500 (.h:147)
        bool addResourceType(ResourceFactory* pFactory); // 0x00CDD9C0 (:188)
        void removeResourceType(ResourceFactory* pFactory); // 0x00CDEBC0 (:237)
        ResourceFactory* removeResourceType(uint32 resourceType); // 0x00CDEB60 (:209)
        void setFallbackFactory(ResourceFactory* pFactory); // 0x00CDC510 (.h:153)
        ResourceFactory* getResourceFactoryForType(uint32 resourceType); // 0x00CDD9F0 (:224)
        Resource* findResource(uint32 type, uint32 key, uint32 resourceContextIndex); // 0x00CDDA20 (:257)
        KrofFileEntry* findKrofFileEntryForResource(Resource* pResource, uint32 resourceContextIndex); // 0x00CDDBA0 (:311)
        KrofFileEntry* findKrofFileEntryForResource(uint32 key, uint32 resourceContextIndex); // 0x00CDDB30 (:290)
    private:
        const KrofFileEntry* load(File& file, uint32 contextIndex, uint8 tag, const char* pFilename, LoadFailReport* pLoadFailReport, Resource* pResourceToUse); // 0x00CE0B40 (:1160)
    public:
        const KrofFileEntry* load(const char* pFilename, uint32 contextIndex, uint8 tag, LoadFailReport* pLoadFailReport); // 0x00CE1110 (:332)
        void getUnresolvedReferences(const KrofFileEntry*, ExternalReference**, uint32*);
        void setAbortLoading(bool* pAbortLoading); // 0x00CDCAD0 (:1796)
        bool unload(const KrofFileEntry* pEntry, bool keepResource); // 0x00CDFE60 (:1525)
        void unloadUnreferenced(); // 0x00CE0010 (:1599)
        ResourceFactory* findFactory(uint32 resourceType); // 0x00CDE080 (:1628)
        uint32 findUnresolvedReferences(ExternalReference* pFirstReference, uint32 numReferences); // 0x00CDE110 (:1690)
        bool resolveWeakReference(WeakResourceReferenceHolder* pReferenceHolder, const KrofFileEntry* pKrofFileEntry, uint8 tag); // 0x00CE17C0 (:1347)
        KrofFileEntry* getNewKrofEntry(uint32 resourceContextIndex); // 0x00CDFB40 (:1641)
        void allocateKrofEntryResources(KrofFileEntry* pKrofEntry, uint32 resourcesCount); // 0x00CDE0D0 (:1667)
        void setKrofEntryFilename(KrofFileEntry* pEntry, const char* pFilename); // 0x00CDCAC0 (:1675)
        void removeUnresolvedReference(const Resource* pResource, uint32 type); // 0x00CDEDF0 (:1724)
        void renameResource(uint32 type, const char* pOldName, const char* pNewName); // 0x00CDE380 (:1844)
    private:
        Resource* unloadKrofEntryExceptResource(uint32 type, uint32 key); // 0x00CE0240 (:1770)
        void freeExternalReferences(ExternalReferences* pExternalReferences, const ResourceAllocationContext* pAllocationContext); // 0x00CDE280 (:1801)
        MemoryAllocator* getEntryAllocator(const AllocationEntry& entry, const ResourceAllocationContext* pAllocationContext); // 0x00CDE440 (:1892)
        MemoryAllocator* getEntryAllocator(const KrofFileEntry& entry, uint32 allocationEntry); // 0x00CDEE10 (:1887)
        void freeAllocationEntry(const AllocationEntry& entry, const ResourceAllocationContext* pAllocationContext); // 0x00CDEDA0 (:1519)
        Resource* findAndRemoveUnresolvedReference(uint32 type, uint32 key); // 0x00CDE1C0 (:1731)
        void setReferredResource(const void** pTargetPointer, ExternalReferences* pExternalReferences, const ResourceReference& currentReference, const char* pStringTableStart, uint32 resourceContextIndex, ResourceEntry* pInternalResources, uint32 internalResourceCount, uint8 tag); // 0x00CDEBE0 (:1409)
        bool readKrofHeader(File& file, KrofLoadContext* pContext); // 0x00CDDC00 (:390)
        bool readDataSection(File& file, KrofLoadContext* pContext); // 0x00CDDF40 (:548)
        bool readRelocationSection(File& file, KrofLoadContext* pContext); // 0x00CDC660 (:605)
        bool readResourceReferenceSection(File& file, KrofLoadContext* pContext); // 0x00CDC980 (:703)
        bool readResourcesSection(File& file, KrofLoadContext* pContext); // 0x00CDC9E0 (:720)
        bool readDebugDataSection(File&, KrofLoadContext*);
        bool readResourceReferences(File& file, KrofLoadContext* pContext, ResourceReferenceSectionHeader** ppHeaderPointer, MemoryAllocator* pAllocator); // 0x00CDC7F0 (:658)
        bool createResources(File& file, KrofLoadContext* pContext, uint8 tag); // 0x00CDF3F0 (:750)
        bool patchResourceReferences(KrofLoadContext* pContext, uint8 tag); // 0x00CDF8F0 (:923)
        bool initializeResources(KrofLoadContext* pContext); // 0x00CE0850 (:1010)

        SizedArray<KrofFileEntry> m_krofFiles; // 0x00
        MemoryAllocator* m_pAllocator; // 0x0C
        MemoryAllocator* m_pScratchAllocator; // 0x10
        SizedArray<FactoryEntry> m_factories; // 0x14
        ResourceFactory* m_pFallbackFactory; // 0x20
        FileSystem* m_pFileSystem; // 0x24
        char m_basePath[260]; // 0x28
        volatile bool* m_pAbortLoading; // 0x12C
        const ResourceContextDescriptor* m_pResourceContexts; // 0x130
        uint32 m_numResourceContexts; // 0x134
        ResourceConfigurationCompatibility m_supportedResourceConfiguration; // 0x138
        uint8 m_readStreamBuffer[2048]; // 0x148
        InitializationDataReadStream m_readStreams[1]; // 0x948
        uint32 m_readStreamCount; // 0x980
    };

    // size 0x44
    struct LoadFailReport
    {
        LoadFailReport(); // 0x00CDC4E0 (.h:119)

        LoadFailReason reason; // 0x00
        char string[64]; // 0x04
    };

    void copyAllocationsToKrofEntry(KrofFileEntry* pEntry, const ChunkedSizedArray<AllocationEntry>& allocationEntries, MemoryAllocator* pAllocator); // 0x00CDF300 (:76)
}
