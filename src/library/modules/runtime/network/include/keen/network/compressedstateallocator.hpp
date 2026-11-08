// Original: library/modules/runtime/network/include/keen/network/compressedstateallocator.hpp
// Implementation: library/modules/runtime/network/sources/global/compressedstateallocator.cpp
//
// Compressed states: snapshots of an entity's state, kept in a list per sender/receiver so that updates can be delta
// compressed against a state both sides know. The allocator hands out fixed size blocks from one memory block. Placement
// of CompressedState inferred.
#pragma once

#include "keen/base/types.hpp"
#include "keen/base/tlsfallocator.hpp"

namespace keen
{
    class MemoryAllocator;
    struct MemoryBlock;

    // size 0xC
    struct CompressedState
    {
        CompressedState* pNext; // 0x00
        void* pData; // 0x04
        uint32 id; // 0x08
    };

    // size 0x10
    struct CompressedStateAllocator
    {
        CompressedStateAllocator(); // 0x00C80690

        TlsfAllocator allocator; // 0x00
        uint32 allocationCount; // 0x0C
    };

    namespace compressedstate
    {
        uint32 getCompressedStateAllocatorSize(); // 0x00C80670 (:16)
        uint32 getAllocationOverhead(); // 0x00C80680 (:21)
        CompressedStateAllocator* createCompressedStateAllocator(MemoryAllocator* pAllocator, MemoryBlock memory); // 0x00C827E0 (:26)
        MemoryBlock destroyCompressedStateAllocator(MemoryAllocator* pAllocator, CompressedStateAllocator* pStateAllocator); // 0x00C82840 (:44)
        CompressedState* allocateCompressedState(CompressedStateAllocator* pStateAllocator, uint32 stateSizeInBytes); // 0x00C806A0 (:52)
        void freeCompressedState(CompressedStateAllocator* pStateAllocator, CompressedState* pState); // 0x00C806E0 (:74)
        void freeCompressedStateList(CompressedStateAllocator* pStateAllocator, CompressedState* pFirstToBeDeleted); // 0x00C80700 (:85)
        void freeOldCompressedStates(CompressedStateAllocator* pStateAllocator, CompressedState* pStartState, uint32 oldestRequiredStateId); // 0x00C80730 (:96)
        void freeSurplusCompressedStates(CompressedStateAllocator* pStateAllocator, CompressedState* pStartState, uint32 maxStateCount); // 0x00C80780 (:122)
        const CompressedState* findCompressedStateById(const CompressedState* pStartState, uint32 stateId); // 0x00C807C0 (:141)
    }
}
