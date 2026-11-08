// Original: library/modules/runtime/base/sources/win32/memorylayout_win32.cpp
//
// The Win32 memory layout: every block of the MemoryLayoutDefinition is reserved and committed up front with
// VirtualAlloc (orig_main.md 2.8: rfg.exe's definition takes about 2.3 GB, so the program must be large address aware).
//
// Partial: createMemoryLayout and allocateSystemMemory. Not ported: destroyMemoryLayout (0x00BED790),
// setMemoryBlockProtection (0x00C0A7F0); destroyMemorySystem frees the blocks itself.

#include "library/modules/runtime/base/sources/global/memorylayout.hpp"
#include "keen/base/string.hpp"
#include "keen/base/trace.hpp"

#include <stdlib.h>
#include <windows.h>

namespace keen
{
    namespace memory
    {
        // 0x00C0A570 (:12)
        // LTCG: the size comes in ECX and the protection mask in AL; blockIndex is not passed (the PDB lists it).
        static void* allocateSystemMemory(uint32 size, uint32 protectionMask, uint32 blockIndex)
        {
            // Protection bits: 1 read, 2 write. Read and write, write only and none give PAGE_READWRITE; read only gives
            // PAGE_READONLY.
            DWORD protect;
            if ((protectionMask & 3) == 3)
            {
                protect = PAGE_READWRITE;
            }
            else
            {
                protect = (protectionMask & 1) != 1 ? PAGE_READWRITE : PAGE_READONLY;
            }
            void* pMemory = VirtualAlloc(nullptr, size, MEM_COMMIT | MEM_RESERVE, protect);
            if (pMemory == nullptr)
            {
                debug::nativePrintArgs("[base] Could not allocate %.2f MB of system memory! Error = 0x%08x\n", (double)size / (1024.0 * 1024.0), GetLastError());
                return nullptr;
            }
            return pMemory;
        }

        // 0x00C0A5E0 (:58)
        bool createMemoryLayout(MemoryLayout* pMemoryLayout, const MemoryLayoutDefinition& memoryLayoutDefinition, uint32 debugMemoryBlockId)
        {
            SYSTEM_INFO systemInfo;
            GetSystemInfo(&systemInfo);
            const uint32 allocationGranularity = systemInfo.dwAllocationGranularity;

            pMemoryLayout->memoryBlockCount = 0;
            bool allocationFailed = false;
            bool hasError = false;
            for (uint32 i = 0u; i < memoryLayoutDefinition.blockCount; ++i)
            {
                const MemoryBlockDefinition& definition = memoryLayoutDefinition.pBlockDefinitions[i];
                pMemoryLayout->memoryBlockTypes[i] = (MemoryType)definition.type;
                pMemoryLayout->memoryBlockCount++;
                pMemoryLayout->memoryBlocks[i].pStart = nullptr;
                pMemoryLayout->memoryBlocks[i].size = 0;
                if (definition.type != MemoryType_MainMemory)
                {
                    debug::nativePrintArgs("[base] Invalid memory block type '%i' found in block '%s'!\n", definition.type, definition.pDescription);
                    hasError = true;
                    continue;
                }
                // Magic number: alignment 0 means 16.
                const uint32 alignment = definition.alignment != 0u ? definition.alignment : 0x10u;
                if ((alignment & (alignment - 1u)) != 0u || alignment > allocationGranularity)
                {
                    debug::nativePrintArgs("[base] Invalid alignment %i in found in block '%s'!\n", alignment, definition.pDescription);
                    hasError = true;
                    continue;
                }
                uint32 size = definition.size;
                if (i == debugMemoryBlockId)
                {
                    // Magic number: the debug block gets 256 MB, whatever the definition says.
                    size = 0x10000000u;
                }
                else if (size == 0u)
                {
                    // An empty block (RFG_Graphics, RFG_Framebuffers and two content blocks in rfg.exe) stays null.
                    pMemoryLayout->memoryBlocks[i].pStart = nullptr;
                    pMemoryLayout->memoryBlocks[i].size = 0;
                    continue;
                }
                uint8* pStart = (uint8*)allocateSystemMemory(size, definition.initialProtectionMask, i);
                pMemoryLayout->memoryBlocks[i].pStart = pStart;
                if (pStart == nullptr)
                {
                    debug::nativePrintArgs("[base] Could not allocate memory block '%s'!\n", definition.pDescription);
                    allocationFailed = true;
                    hasError = true;
                    continue;
                }
                pMemoryLayout->memoryBlocks[i].size = size;
            }

            if (!hasError)
            {
                return true;
            }
            for (uint32 i = 0u; i < pMemoryLayout->memoryBlockCount; ++i)
            {
                if (pMemoryLayout->memoryBlocks[i].pStart != nullptr)
                {
                    VirtualFree(pMemoryLayout->memoryBlocks[i].pStart, 0, MEM_RELEASE);
                    pMemoryLayout->memoryBlocks[i].pStart = nullptr;
                    pMemoryLayout->memoryBlocks[i].size = 0;
                }
            }
            if (!allocationFailed)
            {
                return false;
            }

            // The whole layout's size (the debug block counted as 256 MB) in a message box, then the process ends.
            uint32 totalSize = 0u;
            for (uint32 i = 0u; i < memoryLayoutDefinition.blockCount; ++i)
            {
                totalSize += i == debugMemoryBlockId ? 0x10000000u : memoryLayoutDefinition.pBlockDefinitions[i].size;
            }
            char message[128];
            // The original formats with keen's thousands separator ("%,zu"); the CRT substitute of formatString does not
            // know it.
            formatString(message, sizeof(message), "Could not allocate %u bytes of memory!", totalSize);
            // OpenRFG deviation: the message box is replaced by a log line.
            // MessageBoxA(nullptr, message, "Error", MB_ICONHAND);
            debug::nativePrintArgs("[error] %s\n", message);
            _exit(1);
        }
    }
}
