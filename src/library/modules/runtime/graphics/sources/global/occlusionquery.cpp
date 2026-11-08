// Original: library/modules/runtime/graphics/sources/global/occlusionquery.cpp
//
// OcclusionQueryManager: a pool of D3D11 occlusion queries with a stack of free handles.

#include "keen/graphics/occlusionquery.hpp"
#include "library/modules/runtime/graphics/sources/d3d11/graphicscommandbuffer_d3d11.hpp"
#include "library/modules/runtime/graphics/sources/d3d11/graphicssystem_d3d11.hpp"
#include "library/modules/runtime/graphics/sources/d3d11/occlusionquery_d3d11.hpp"
#include "keen/base/string.hpp"

namespace keen
{
    // 0x0052DD40
    OcclusionQueryManager::OcclusionQueryManager()
    {
    }

    // 0x00516C50
    OcclusionQueryManager::~OcclusionQueryManager()
    {
    }

    // 0x00C6CFF0 (:7)
    void OcclusionQueryManager::create(GraphicsSystem* pGraphicsSystem, MemoryAllocator* pAllocator, uint32 maxQueryCount, MemoryBlock gpuMemory)
    {
        // Magic number: 16-byte aligned query pool.
        m_occlusionQueryPool.create(pAllocator, maxQueryCount, 16);
        m_freeOcclusionQueries.create(maxQueryCount, pAllocator);
        if (gpuMemory.size != 0)
        {
            m_gpuMemoryAllocator.create(gpuMemory, 0, "occlusionQueryGPUMemory");
        }
        for (uint32 i = 0; i < maxQueryCount; ++i)
        {
            char debugName[64];
            formatString(debugName, sizeof(debugName), "OcclusionQueryManager Query%i", i);
            D3D11_QUERY_DESC queryDesc;
            queryDesc.Query = D3D11_QUERY_OCCLUSION;
            queryDesc.MiscFlags = 0;
            OcclusionQuery* pQuery = &m_occlusionQueryPool[i];
            if (SUCCEEDED(pGraphicsSystem->pDevice->CreateQuery(&queryDesc, &pQuery->pQuery)))
            {
                pQuery->pQuery->SetPrivateData(WKPDID_D3DDebugObjectName, getStringLength(debugName), debugName);
            }
            m_freeOcclusionQueries.push(i);
        }
    }

    // 0x00C6E580 (:28)
    // Notes (decompiled behaviour, kept as is): a query whose creation failed is released through a null pointer.
    void OcclusionQueryManager::destroy(GraphicsSystem* pGraphicsSystem, MemoryAllocator* pAllocator)
    {
        for (uint32 i = 0; i < m_occlusionQueryPool.getSize(); ++i)
        {
            OcclusionQuery* pQuery = &m_occlusionQueryPool[i];
            pQuery->pQuery->Release();
            pQuery->pQuery = nullptr;
        }
        m_freeOcclusionQueries.destroy(pAllocator);
        m_occlusionQueryPool.destroy(pAllocator);
    }

    // 0x00C6D130 (:39)
    uint32 OcclusionQueryManager::allocateAndBeginOcclusionQuery(GraphicsCommandBuffer* pCommandBuffer)
    {
        if (m_freeOcclusionQueries.getSize() == 0)
        {
            return (uint32)Handle_Invalid;
        }
        const uint32 queryHandle = m_freeOcclusionQueries.pop();
        pCommandBuffer->pContext->Begin(m_occlusionQueryPool[queryHandle].pQuery);
        return queryHandle;
    }

    // 0x00C655F0 (:53)
    // Notes (decompiled behaviour, kept as is): Handle_Invalid is not checked.
    void OcclusionQueryManager::endOcclusionQuery(GraphicsCommandBuffer* pCommandBuffer, uint32 queryHandle)
    {
        pCommandBuffer->pContext->End(m_occlusionQueryPool[queryHandle].pQuery);
    }

    // 0x00C65610 (:59)
    bool OcclusionQueryManager::pollOcclusionQueryResult(GraphicsCommandBuffer* pCommandBuffer, uint32 queryHandle, uint64* pResult)
    {
        return pCommandBuffer->pContext->GetData(m_occlusionQueryPool[queryHandle].pQuery, pResult, sizeof(uint64), D3D11_ASYNC_GETDATA_DONOTFLUSH) == S_OK;
    }

    // 0x00C6D170 (:65)
    void OcclusionQueryManager::freeOcclusionQuery(uint32 queryHandle)
    {
        m_freeOcclusionQueries.push(queryHandle);
    }
}
