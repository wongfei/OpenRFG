// Original: library/modules/runtime/resources/include/keen/resources/resourceobjectformat.hpp
//
// The krof resource object file format: file header, resource headers, section headers, resource references and
// external references. Placement inferred.
#pragma once

#include "keen/base/types.hpp"

namespace keen
{
    class Resource;

    enum ResourceObjectAllocatorType
    {
        ResourceObjectAllocatorType_Generic_ResourceObjects = 0,
        ResourceObjectAllocatorType_Generic_CPUAccessible = 1,
        ResourceObjectAllocatorType_PS3_RSX = 2,
        ResourceObjectAllocatorType_XB360_Physical = 2,
        ResourceObjectAllocatorType_Wii_Memory1 = 2,
        ResourceObjectAllocatorType_Wii_Memory2 = 3,
        ResourceObjectAllocatorType_CTR_Device = 2,
        ResourceObjectAllocatorType_WiiU_Memory1 = 2,
        ResourceObjectAllocatorType_PS4_Garlic = 2,
        ResourceObjectAllocatorType_PSV_GPUMapped = 2,
        ResourceObjectAllocatorType_XBone_APU = 3,
        ResourceObjectAllocatorType_Switch_Audiobuffers = 3,
        ResourceObjectAllocatorType_Count = 4,
        ResourceObjectAllocatorType_StreamData = 93,
    };

    // size 0x14
    struct ResourceHeader
    {
        uint32 resourceKey; // 0x00
        uint32 offsetInResourceDataSection; // 0x04
        uint32 resourceType; // 0x08
        uint32 contentFormatKey; // 0x0C
        uint16 resourceIntializationDataSize; // 0x10
        uint16 resourceId; // 0x12
    };

    // size 0x4
    struct ResourceSectionHeader
    {
        uint16 resourcesCount; // 0x00
        uint16 relocationEntryCount; // 0x02
    };

    // size 0x10
    struct SectionHeader
    {
        uint8 sectionType; // 0x00
        uint8 pad; // 0x01
        uint8 alignment; // 0x02
        uint8 allocatorType; // 0x03
        uint32 fileOffset; // 0x04
        uint32 sectionDataSize; // 0x08
        uint32 fileDataSize; // 0x0C
    };

    // size 0x8
    struct ResourceObjectFileHeader
    {
        uint32 headerKey; // 0x00
        uint8 formatRevision; // 0x04
        uint8 chunkCount; // 0x05
        uint8 totalSectionCount; // 0x06
        uint8 pad; // 0x07
    };

    // size 0xC
    struct ResourceReference
    {
        uint8 sectionIndex; // 0x00
        uint8 referenceType; // 0x01
        union
        {
            uint16 fileNameStringOffset; // 0x02
            uint16 resourceId; // 0x02
        };
        uint32 resourceType; // 0x04
        uint32 offsetToResourcePointer; // 0x08
    };

    // size 0x4
    struct ResourceReferenceSectionHeader
    {
        uint32 referencesCount; // 0x00
    };

    // size 0x14
    struct ExternalReference
    {
        Resource* pResource; // 0x00
        uint32 type; // 0x04
        uint32 key; // 0x08
        const char* pOptionalResourceName; // 0x0C
        bool isResolved; // 0x10
        bool isOwner; // 0x11
    };

    // size 0x8
    struct ExternalReferences
    {
        ExternalReference* pReferences; // 0x00
        uint32 referenceCount; // 0x04
    };
}
