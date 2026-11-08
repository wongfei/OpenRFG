// Original: library/modules/runtime/base/sources/global/tlsfallocator.cpp
//
// keen::TlsfAllocator, a thin wrapper over the third-party TLSF pool. Alignments above 4 go through tlsf_memalign.

#include "keen/base/tlsfallocator.hpp"
#include "keen/base/slice.hpp"
#include "keen/base/trace.hpp"

#include "library/modules/thirdparty/tlsf/tlsf.h"

#include "memorylayout.hpp"

namespace keen
{
    // 0x00BF2F30 (:13)
    TlsfAllocator::TlsfAllocator()
    {
        m_pPool = nullptr;
    }

    // 0x00BF2F40 (:18)
    uint32 TlsfAllocator::getAllocationOverhead()
    {
        return (uint32)tlsf_overhead();
    }

    // 0x00C09E00 (:23)
    bool TlsfAllocator::create(MemoryBlock memoryBlock)
    {
        m_pPool = tlsf_create(memoryBlock.pStart, memoryBlock.size);
        if (m_pPool == nullptr)
        {
            debug::nativePrintArgs("Could not create new TLSF allocator!\n");
            return false;
        }
        m_memoryBlock = memoryBlock;
        return true;
    }

    // 0x00BF2F50 (:37)
    MemoryBlock TlsfAllocator::destroy()
    {
        tlsf_destroy(m_pPool);
        const MemoryBlock block = m_memoryBlock;
        m_pPool = nullptr;
        m_memoryBlock.pStart = nullptr;
        m_memoryBlock.size = 0;
        return block;
    }

    // 0x00BF2F80 (:47)
    void* TlsfAllocator::allocate(uint32 size, uint32 alignment, uint32 flags)
    {
        if (alignment > 4)
        {
            return tlsf_memalign(m_pPool, alignment, size);
        }
        return tlsf_malloc(m_pPool, size);
    }

    // 0x00BF2FB0 (:66)
    void TlsfAllocator::free(void* pMemory)
    {
        tlsf_free(m_pPool, pMemory);
    }

    // 0x00BF2FD0 (:73)
    uint32 TlsfAllocator::getAllocationSize(const void* pMemory) const
    {
        return (uint32)tlsf_block_size((void*)pMemory);
    }

    // Walker context of fillAllocationInfos: the target slice and the number of blocks still to skip.
    struct TlsfBlockCountContext
    {
        Slice<MemoryBlockInfo>* pInfos;
        uint32 skipCount;
    };

    // 0x00BFB320 (:84)
    static void countTlsfBlocks(void* pBlock, size_t size, int used, void* pUser)
    {
        TlsfBlockCountContext* pContext = (TlsfBlockCountContext*)pUser;
        if (pContext->skipCount != 0)
        {
            --pContext->skipCount;
            return;
        }
        Slice<MemoryBlockInfo>* pInfos = pContext->pInfos;
        if (pInfos->getSize() != pInfos->getCapacity())
        {
            MemoryBlockInfo& info = pInfos->pushBack();
            info.block.pStart = (uint8*)pBlock;
            info.block.size = (uint32)size;
            info.flags = used != 0 ? 1u : 0u;
        }
    }

    // 0x00BFB370 (:105)
    void TlsfAllocator::fillAllocationInfos(Slice<MemoryBlockInfo>* pInfos, uint32 startIndex) const
    {
        TlsfBlockCountContext context;
        context.pInfos = pInfos;
        context.skipCount = startIndex;
        tlsf_walk_heap(m_pPool, countTlsfBlocks, &context);
    }

    // 0x00BF2FE0 (:114)
    bool TlsfAllocator::checkIntegrity() const
    {
        return m_pPool != nullptr && tlsf_check_heap(m_pPool) == 0;
    }

    const MemoryBlock& TlsfAllocator::getMemoryBlock() const
    {
        return m_memoryBlock;
    }
}
