// Original: library/modules/runtime/base/sources/win32/memory_win32.hpp
//
// Win32 memory copy/fill and memory barrier.
#pragma once

#include "keen/base/types.hpp"

namespace keen
{
    void fillMemoryUint8(void* pMemory, uint8 value, uint32 sizeInBytes); // 0x00458950 (.h:11)
    void copyMemoryNonOverlapping(void* pTarget, const void* pSource, uint32 size); // 0x0045ABB0 (.h:16)
    void keenMemoryBarrier(); // 0x00482B80 (.h:35)
}
