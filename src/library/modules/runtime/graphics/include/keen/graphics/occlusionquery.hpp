// Original: library/modules/runtime/graphics/include/keen/graphics/occlusionquery.hpp
// Implementation: library/modules/runtime/graphics/sources/global/occlusionquery.cpp
//
// OcclusionQueryManager, a pool of occlusion queries. OcclusionQuery is defined in sources/d3d11/occlusionquery_d3d11.hpp.
#pragma once

#include "keen/base/types.hpp"
#include "keen/base/array.hpp"
#include "keen/base/stack.hpp"
#include "keen/base/zonememoryallocator.hpp"
#include "library/modules/runtime/graphics/sources/d3d11/occlusionquery_d3d11.hpp"

namespace keen
{
    struct GraphicsCommandBuffer;
    struct GraphicsSystem;
    class MemoryAllocator;
    struct MemoryBlock;

    // size 0xF8
    class OcclusionQueryManager
    {
    public:
        enum
        {
            Handle_Invalid = -1,
        };

        typedef uint32 Handle;

        void create(GraphicsSystem* pGraphicsSystem, MemoryAllocator* pAllocator, uint32 maxQueryCount, MemoryBlock gpuMemory); // 0x00C6CFF0 (:7)
        void destroy(GraphicsSystem* pGraphicsSystem, MemoryAllocator* pAllocator); // 0x00C6E580 (:28)
        uint32 allocateAndBeginOcclusionQuery(GraphicsCommandBuffer* pCommandBuffer); // 0x00C6D130 (:39)
        void endOcclusionQuery(GraphicsCommandBuffer* pCommandBuffer, uint32 queryHandle); // 0x00C655F0 (:53)
        bool pollOcclusionQueryResult(GraphicsCommandBuffer* pCommandBuffer, uint32 queryHandle, uint64* pResult); // 0x00C65610 (:59)
        void freeOcclusionQuery(uint32 queryHandle); // 0x00C6D170 (:65)
        OcclusionQueryManager(); // 0x0052DD40
        ~OcclusionQueryManager(); // 0x00516C50

    private:
        Array<OcclusionQuery> m_occlusionQueryPool; // 0x00
        Stack<uint32> m_freeOcclusionQueries; // 0x08
        ZoneMemoryAllocator m_gpuMemoryAllocator; // 0x14
    };

    namespace graphics
    {
        bool createOcclusionQuery(GraphicsSystem* pGraphicsSystem, OcclusionQuery* pOcclusionQuery, MemoryAllocator* pGpuAllocator, const char* pDebugName); // 0x00C5C4F0 (occlusionquery_d3d11.cpp:10)
        void destroyOcclusionQuery(GraphicsSystem* pGraphicsSystem, OcclusionQuery* pOcclusionQuery, MemoryAllocator* pGpuAllocator); // 0x00C5C560 (occlusionquery_d3d11.cpp:29)
    }
}
