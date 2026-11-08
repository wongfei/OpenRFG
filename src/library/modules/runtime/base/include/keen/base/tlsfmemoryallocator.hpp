// Original: library/modules/runtime/base/include/keen/base/tlsfmemoryallocator.hpp
#pragma once

#include "keen/base/basememoryallocator.hpp"
#include "keen/base/tlsfallocator.hpp"

namespace keen
{
    // The default keen heap: GameFrameworkSystem::defaultAllocator, PakFileDevice chunk allocator, ...
    // size 0xE0
    class TlsfMemoryAllocator : public BaseMemoryAllocator<TlsfAllocator>
    {
    public:
        TlsfMemoryAllocator() // 0x005D84F0 (implicit)
        {
        }
        virtual ~TlsfMemoryAllocator() // vtbl+0x00 0x005D4750 (implicit)
        {
        }
    };
}
