// Original: library/modules/runtime/graphics/sources/d3d11/occlusionquery_d3d11.cpp

#include "keen/graphics/occlusionquery.hpp"
#include "library/modules/runtime/graphics/sources/d3d11/graphicssystem_d3d11.hpp"
#include "keen/base/string.hpp"

namespace keen
{
    namespace graphics
    {
        // 0x00C5C4F0 (:10)
        bool createOcclusionQuery(GraphicsSystem* pGraphicsSystem, OcclusionQuery* pOcclusionQuery, MemoryAllocator* pGpuAllocator, const char* pDebugName)
        {
            D3D11_QUERY_DESC queryDesc;
            queryDesc.Query = D3D11_QUERY_OCCLUSION;
            queryDesc.MiscFlags = 0;
            if (FAILED(pGraphicsSystem->pDevice->CreateQuery(&queryDesc, &pOcclusionQuery->pQuery)))
            {
                return false;
            }
            pOcclusionQuery->pQuery->SetPrivateData(WKPDID_D3DDebugObjectName, getStringLength(pDebugName), pDebugName);
            return true;
        }

        // 0x00C5C560 (:29)
        void destroyOcclusionQuery(GraphicsSystem* pGraphicsSystem, OcclusionQuery* pOcclusionQuery, MemoryAllocator* pGpuAllocator)
        {
            pOcclusionQuery->pQuery->Release();
            pOcclusionQuery->pQuery = nullptr;
        }
    }
}
