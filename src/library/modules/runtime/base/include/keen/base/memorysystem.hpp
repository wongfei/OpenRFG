// Original: library/modules/runtime/base/include/keen/base/memorysystem.hpp
// Implementation: library/modules/runtime/base/sources/global/memorysystem.cpp
//
// Recovery scope: the API used by GameFramework. MemorySystem (0xAF0 bytes: memory layout plus
// 128 mapped block definitions) is created from the generated getMemoryLayoutDefinition().
#pragma once

#include "keen/base/memory.hpp"

namespace keen
{
    struct MemorySystem; // size 0xAF0
    struct MemoryLayoutDefinition;
    struct MappedMemoryBlockDefinition;

    namespace memory
    {
        MemorySystem* createMemorySystem(const MemoryLayoutDefinition& layoutDefinition, uint32 debugMemoryBlockId); // 0x00C0BEB0
        void destroyMemorySystem(MemorySystem* pMemorySystem); // 0x00C08740
        MemoryBlock lockMemoryBlock(MemorySystem* pMemorySystem, uint32 memoryBlockIndex); // 0x00C087D0
        void unlockMemoryBlock(MemorySystem* pMemorySystem, uint32 memoryBlockIndex); // 0x00BF0F30
        bool lockMemoryBlocks(MemorySystem* pMemorySystem, MemoryBlock* pBlocks, const uint32* pIds, uint32 count); // 0x00C08820
        void unlockMemoryBlocks(MemorySystem* pMemorySystem, const uint32* pIds, uint32 count); // 0x00BF0F50
        void changeMemoryBlockProtection(MemorySystem* pMemorySystem, uint32 memoryBlockIndex, uint32 protectionMask); // 0x00C0BFA0
        const MappedMemoryBlockDefinition* getMemoryBlocksToMap(const MemorySystem* pMemorySystem); // 0x00BF0F90
    }
}
