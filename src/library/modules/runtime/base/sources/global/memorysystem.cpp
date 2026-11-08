// Original: library/modules/runtime/base/sources/global/memorysystem.cpp
//
// The memory system: the memory layout of the game (memorylayout_win32.cpp) with a lock per block. Blocks are handed
// out by index (lockMemoryBlock); a block whose definition asks for GPU mapping (protection bits 4/8, none in rfg.exe)
// is listed in mappedMemoryBlocks for the graphics system.
//
// Partial: create/destroy and lockMemoryBlock/unlockMemoryBlock. Not ported: lockMemoryBlocks, unlockMemoryBlocks,
// changeMemoryBlockProtection, getMemoryBlocksToMap and the memory tracker (isMemoryTrackerInitialized stays false).

#include "keen/base/memorysystem.hpp"
#include "keen/base/trace.hpp"
#include "library/modules/runtime/base/sources/global/memorylayout.hpp"

#include <string.h>
#include <windows.h>

namespace keen
{
    // size 0xAF0
    struct MemorySystem
    {
        MemoryLayoutDefinition layoutDefinition; // 0x00
        MemoryLayout memoryLayout; // 0x08
        MappedMemoryBlockDefinition mappedMemoryBlocks[128]; // 0x66C
        bool isMemoryBlockLocked[128]; // 0xA6C
        bool isMemoryTrackerInitialized; // 0xAEC
    };
    static_assert(sizeof(MemorySystem) == 0xAF0, "MemorySystem size");

    static MemorySystem s_memorySystem; // 0x038B3160
    static MemorySystem* s_pMemorySystem; // 0x038B3C84

    namespace memory
    {
        // 0x00C0BEB0
        MemorySystem* createMemorySystem(const MemoryLayoutDefinition& layoutDefinition, uint32 debugMemoryBlockId)
        {
            s_memorySystem.layoutDefinition = layoutDefinition;
            // Magic number: 12 base memory blocks, one per MemoryType.
            for (uint32 i = 0u; i < 12u; ++i)
            {
                s_memorySystem.memoryLayout.baseMemoryBlocks[i].pStart = nullptr;
                s_memorySystem.memoryLayout.baseMemoryBlocks[i].size = 0;
            }
            for (uint32 i = 0u; i < layoutDefinition.blockCount; ++i)
            {
                s_memorySystem.memoryLayout.memoryBlocks[i].pStart = nullptr;
                s_memorySystem.memoryLayout.memoryBlocks[i].size = 0;
                s_memorySystem.isMemoryBlockLocked[i] = false;
            }
            s_memorySystem.memoryLayout.memoryBlockCount = layoutDefinition.blockCount;
            if (!createMemoryLayout(&s_memorySystem.memoryLayout, s_memorySystem.layoutDefinition, debugMemoryBlockId))
            {
                return nullptr;
            }
            s_memorySystem.isMemoryTrackerInitialized = false;
            s_pMemorySystem = &s_memorySystem;
            memset(s_memorySystem.mappedMemoryBlocks, 0, sizeof(s_memorySystem.mappedMemoryBlocks));
            MappedMemoryBlockDefinition* pMapped = s_memorySystem.mappedMemoryBlocks;
            for (uint32 i = 0u; i < s_memorySystem.layoutDefinition.blockCount; ++i)
            {
                const MemoryBlockDefinition* pDefinition = &s_memorySystem.layoutDefinition.pBlockDefinitions[i];
                // Magic number: protection bits 4 and 8 ask for GPU access.
                if ((pDefinition->initialProtectionMask & 0xC) != 0u)
                {
                    pMapped->pBlockDefinition = pDefinition;
                    pMapped->pBlock = &s_memorySystem.memoryLayout.memoryBlocks[i];
                    ++pMapped;
                }
            }
            return &s_memorySystem;
        }

        // 0x00C08740
        void destroyMemorySystem(MemorySystem* pMemorySystem)
        {
            for (uint32 i = 0u; i < pMemorySystem->memoryLayout.memoryBlockCount; ++i)
            {
                if (pMemorySystem->isMemoryBlockLocked[i])
                {
                    debug::nativePrintArgs("[base] Memory block %s is still locked by the application!\n", pMemorySystem->layoutDefinition.pBlockDefinitions[i].pDescription);
                }
            }
            for (uint32 i = 0u; i < pMemorySystem->memoryLayout.memoryBlockCount; ++i)
            {
                MemoryBlock* pBlock = &pMemorySystem->memoryLayout.memoryBlocks[i];
                if (pBlock->pStart != nullptr)
                {
                    VirtualFree(pBlock->pStart, 0, MEM_RELEASE);
                    pBlock->pStart = nullptr;
                    pBlock->size = 0;
                }
            }
            s_pMemorySystem = nullptr;
        }

        // 0x00C087D0
        MemoryBlock lockMemoryBlock(MemorySystem* pMemorySystem, uint32 memoryBlockIndex)
        {
            if (pMemorySystem->isMemoryBlockLocked[memoryBlockIndex])
            {
                debug::nativePrintArgs("[memory] Could not lock memory block %s - it's already locked!\n", pMemorySystem->layoutDefinition.pBlockDefinitions[memoryBlockIndex].pDescription);
                MemoryBlock invalidBlock;
                invalidBlock.pStart = nullptr;
                invalidBlock.size = 0;
                return invalidBlock;
            }
            pMemorySystem->isMemoryBlockLocked[memoryBlockIndex] = true;
            return pMemorySystem->memoryLayout.memoryBlocks[memoryBlockIndex];
        }

        // 0x00BF0F30
        void unlockMemoryBlock(MemorySystem* pMemorySystem, uint32 memoryBlockIndex)
        {
            if (pMemorySystem->isMemoryBlockLocked[memoryBlockIndex])
            {
                pMemorySystem->isMemoryBlockLocked[memoryBlockIndex] = false;
            }
        }
    }
}
