// Original: game/build/win32/steam/master/generated_sources/memorylayoutdefinition.cpp
//
// Generated: the game's memory layout, recovered from .data (s_memoryBlocks 0x01348348, 15 blocks). Every block is main
// memory, 1 KB aligned for the RFG blocks and 128 bytes for the GameFramework ones, with protection 0x13 (read, write
// and bit 0x10). A size of 0 leaves the block empty. The indices are what the code locks: 1, 2, 3, 5, 6, 7, 8, 9 in
// rfg_init_keen_memory_allocators, 10 (the default allocator) and 11 (shader objects) in GameFramework::initialize.
// Sizes are decimal in the source (the hex values are 1,000, 80,000,000, ...).

#include "game/build/win32/steam/master/generated_sources/memorylayoutdefinition.hpp"
#include "library/modules/runtime/base/sources/global/memorylayout.hpp"

namespace keen
{
    static const MemoryBlockDefinition s_memoryBlocks[15] = // 0x01348348
    {
        { 1000u, 0x400u, MemoryType_MainMemory, 0u, 0x13u, "RFG_SysMempoolProvider" }, // 0
        { 80000000u, 0x400u, MemoryType_MainMemory, 0u, 0x13u, "RFG_VramMempoolProvider" }, // 1
        { 1146000000u, 0x400u, MemoryType_MainMemory, 0u, 0x13u, "RFG_MempoolProvider" }, // 2
        { 300000u, 0x400u, MemoryType_MainMemory, 0u, 0x13u, "RFG_GpuScratchMem" }, // 3
        { 0u, 0x400u, MemoryType_MainMemory, 0u, 0x13u, "RFG_Graphics" }, // 4
        { 0u, 0x400u, MemoryType_MainMemory, 0u, 0x13u, "RFG_Framebuffers" }, // 5
        { 220000000u, 0x400u, MemoryType_MainMemory, 0u, 0x13u, "RFG_Level" }, // 6
        { 500000000u, 0x400u, MemoryType_MainMemory, 0u, 0x13u, "RFG_D3Dscratch" }, // 7
        { 190000000u, 0x400u, MemoryType_MainMemory, 0u, 0x13u, "RFG_Audio_Bank" }, // 8
        { 20000000u, 0x400u, MemoryType_MainMemory, 0u, 0x13u, "RFG_Audio" }, // 9
        { 180000000u, 0x80u, MemoryType_MainMemory, 0u, 0x13u, "GameFramework_Default" }, // 10
        { 10000000u, 0x80u, MemoryType_MainMemory, 0u, 0x13u, "GameFramework_ShaderObjects" }, // 11
        { 0u, 0x80u, MemoryType_MainMemory, 0u, 0x13u, "GameFramework_Content_Scratch" }, // 12
        { 0u, 0x80u, MemoryType_MainMemory, 0u, 0x13u, "GameFramework_Content_ResourceObjects" }, // 13
        { 64000u, 0x80u, MemoryType_MainMemory, 0u, 0x13u, "GameFramework_DebugTexture" }, // 14
    };

    static const MemoryLayoutDefinition s_memoryLayoutDefinition = { s_memoryBlocks, 15u }; // 0x01348474

    // 0x00CE6120 (:36)
    const MemoryLayoutDefinition& getMemoryLayoutDefinition()
    {
        return s_memoryLayoutDefinition;
    }
}
