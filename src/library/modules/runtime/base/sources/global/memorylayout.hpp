// Original: library/modules/runtime/base/sources/global/memorylayout.hpp
// Implementation: library/modules/runtime/base/sources/global/memorylayout.cpp
//
// MemoryLayout, the fixed memory layout of the game: memory blocks reserved at startup from a MemoryLayoutDefinition
// (generated, game/build/.../memorylayoutdefinition), with protection and sorting helpers. Placement of the definition
// types inferred.
#pragma once

#include "keen/base/types.hpp"
#include "keen/base/memory.hpp"

namespace keen
{
    struct MemoryLayout;

    enum MemoryType
    {
        MemoryType_MainMemory = 0,
        MemoryType_Ps3_RsxLocal = 1,
        MemoryType_Ps3_RsxMain = 2,
        MemoryType_Xb360_Physical = 3,
        MemoryType_Xb360_Physical_WriteCombined = 4,
        MemoryType_WiiU_Mem1 = 5,
        MemoryType_Garlic = 6,
        MemoryType_Onion = 7,
        MemoryType_PsV_VRAM = 8,
        MemoryType_GPU_Mapped = 9,
        MemoryType_XBone_APU = 10,
        MemoryType_Switch_AudioBuffers = 11,
        MemoryType_Count = 12,
    };

    // size 0x14
    struct MemoryBlockDefinition
    {
        uint32 size; // 0x00
        uint32 alignment; // 0x04
        uint16 type; // 0x08
        uint16 flags; // 0x0A
        uint32 initialProtectionMask; // 0x0C
        const char* pDescription; // 0x10
    };

    // size 0x8
    struct MappedMemoryBlockDefinition
    {
        const MemoryBlockDefinition* pBlockDefinition; // 0x00
        const MemoryBlock* pBlock; // 0x04
    };

    // size 0x8
    struct MemoryLayoutDefinition
    {
        const MemoryBlockDefinition* pBlockDefinitions; // 0x00
        uint32 blockCount; // 0x04
    };

    // size 0xC
    struct MemoryBlockInfo
    {
        MemoryBlock block; // 0x00
        uint32 flags; // 0x08
    };

    // size 0x8
    struct MemoryBlockSortByAddressEntry
    {
        static bool compare(const MemoryBlockSortByAddressEntry&, const MemoryBlockSortByAddressEntry&);

        void* pStart; // 0x00
        uint32 index; // 0x04
    };

    namespace memory
    {
        // size 0x6C8
        struct LayoutCreationHelper
        {
            // size 0xC
            struct CreationBlockInfo
            {
                static bool compare(const CreationBlockInfo& a, const CreationBlockInfo& b); // 0x00BED610 (.h:38)

                const MemoryBlockDefinition* pBlockDefinition; // 0x00
                uint32 blockDefinitionIndex; // 0x04
                bool aquireRemainingMemory; // 0x08
            };

            void create(const MemoryLayoutDefinition& memoryLayoutDefinition, uint32 debugMemoryBlockId); // 0x00BF9060 (:9)
            void allocateBlocks(MemoryLayout* pMemoryLayout, bool allocateOptionalBlocks); // 0x00BF9180 (:62)

            CreationBlockInfo infos[128]; // 0x00
            const MemoryLayoutDefinition* pLayoutDefinition; // 0x600
            uint32 memBlockSizes[12]; // 0x604
            uint32 optionalBlockSizes[12]; // 0x634
            uint32 blockAlignments[12]; // 0x664
            uint32 totalAllocatedSizes[12]; // 0x694
            uint32 totalAcquireRemainingSize; // 0x6C4
        };

    }

    // size 0x664
    struct MemoryLayout
    {
        uint32 memoryBlockCount; // 0x00
        MemoryBlock memoryBlocks[128]; // 0x04
        MemoryType memoryBlockTypes[128]; // 0x404
        MemoryBlock baseMemoryBlocks[12]; // 0x604
    };

    namespace memory
    {
        bool createMemoryLayout(MemoryLayout* pMemoryLayout, const MemoryLayoutDefinition& memoryLayoutDefinition, uint32 debugMemoryBlockId); // 0x00C0A5E0 (memorylayout_win32.cpp:58)
        void destroyMemoryLayout(MemoryLayout* pMemoryLayout, const MemoryLayoutDefinition& memoryLayoutDefinition); // 0x00BED790 (memorylayout_win32.cpp:163)
        void setMemoryBlockProtection(MemoryBlock block, uint32 protectionMask, const MemoryBlockDefinition& blockDefinition); // 0x00C0A7F0 (memorylayout_win32.cpp:197)
    }
}
