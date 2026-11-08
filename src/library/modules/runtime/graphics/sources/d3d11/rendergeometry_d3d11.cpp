// Original: library/modules/runtime/graphics/sources/d3d11/rendergeometry_d3d11.cpp
//
// RenderGeometry on D3D11: a vertex buffer, an optional index buffer and the D3D primitive topology. The accessors and
// buffer fill helpers marked (.h:N) are inline in rendergeometry_d3d11.hpp in the original; the description
// constructors marked (rendergeometry.hpp:N) are inline in rendergeometry.hpp.

#include "library/modules/runtime/graphics/sources/d3d11/rendergeometry_d3d11.hpp"
#include "library/modules/runtime/graphics/sources/d3d11/geometryinstancing_d3d11.hpp"
#include "library/modules/runtime/graphics/sources/d3d11/graphicssystem_d3d11.hpp"
#include "keen/graphics/rendergeometry.hpp"
#include "keen/graphics/vertexformat.hpp"

#include <stdlib.h>
#include <string.h>

namespace keen
{
    // 0x013412D8: D3D topology per PrimitiveType (quad lists are drawn as triangle lists after expansion).
    static const D3D11_PRIMITIVE_TOPOLOGY s_primitiveTopology[PrimitiveType_Count] =
    {
        D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST,
        D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP,
        D3D11_PRIMITIVE_TOPOLOGY_LINELIST,
        D3D11_PRIMITIVE_TOPOLOGY_LINESTRIP,
        D3D11_PRIMITIVE_TOPOLOGY_UNDEFINED,
    };

    // 0x013412EC: D3D CPU access flags per RenderGeometryAccessMode.
    static const uint32 s_cpuAccessFlags[RenderGeometryAccessMode_Count] =
    {
        0, D3D11_CPU_ACCESS_READ, D3D11_CPU_ACCESS_WRITE, D3D11_CPU_ACCESS_READ | D3D11_CPU_ACCESS_WRITE,
    };

    // 0x004B22F0 (rendergeometry.hpp:83)
    RenderGeometryDescription::RenderGeometryDescription()
    {
        primitiveType = PrimitiveType_Invalid;
        pVertexFormat = nullptr;
        indexFormat = IndexFormat_Uint16;
        vertexCount = 0;
        indexCount = 0;
        cpuAccessMode = RenderGeometryAccessMode_None;
        streamIndex = 0;
    }

    // 0x004B2320 (rendergeometry.hpp:105)
    RenderGeometryInitializationData::RenderGeometryInitializationData()
    {
        pVertexData = nullptr;
        pIndexData = nullptr;
        pSkinningData = nullptr;
        pPlatformData = nullptr;
        pMorphingData = nullptr;
    }

    // 0x00C5C8B0 (:40)
    D3D_PRIMITIVE_TOPOLOGY getPrimitiveTopologyFromPrimitiveType(PrimitiveType type)
    {
        return s_primitiveTopology[type];
    }

    namespace graphics
    {
        // 0x00C5B580 (.h:72)
        uint32 getVertexCount(const RenderGeometry* pGeometry)
        {
            return pGeometry->vertexCount;
        }

        // 0x00C5B590 (.h:73)
        uint32 getIndexCount(const RenderGeometry* pGeometry)
        {
            return pGeometry->indexCount;
        }

        // 0x004B2230 (.h:74)
        uint32 getIndexStride(const RenderGeometry* pGeometry)
        {
            return pGeometry->indexFormat == DXGI_FORMAT_R32_UINT ? 4 : 2;
        }

        // 0x004B2250 (.h:76)
        const VertexFormat* getVertexFormat(const RenderGeometry* pGeometry)
        {
            return pGeometry->pVertexFormat;
        }

        // 0x00CC83E0 (.h:77)
        uint32 getSkinningBufferSize(const RenderGeometry* pGeometry)
        {
            return pGeometry->pVertexFormat->streamStride[0] * pGeometry->vertexCount;
        }

        // 0x004B2260 (.h:80)
        void fillVertexBufferData(VertexBufferData* pOut, const RenderGeometry* pGeometry, uint32 offset)
        {
            pOut->pBuffer = pGeometry->pVertexBuffer;
            pOut->offset = offset;
            pOut->vertexCount = pGeometry->vertexCount;
        }

        // 0x004B2280 (.h:87)
        void fillIndexBufferData(IndexBufferData* pOut, const RenderGeometry* pGeometry, uint32 offset)
        {
            pOut->pBuffer = pGeometry->pIndexBuffer;
            pOut->offset = offset;
            pOut->indexCount = pGeometry->indexCount - offset / getIndexStride(pGeometry);
            pOut->indexFormat = pGeometry->indexFormat;
        }

        // 0x004B22C0 (.h:95)
        void fillPrimitiveData(PrimitiveData* pOut, const RenderGeometry* pGeometry, uint32 vertexDrawCount)
        {
            pOut->primitiveTopology = pGeometry->primitiveTopology;
            if (vertexDrawCount != 0)
            {
                pOut->vertexDrawCount = vertexDrawCount;
            }
            else
            {
                pOut->vertexDrawCount = pGeometry->pIndexBuffer != nullptr ? pGeometry->indexCount : pGeometry->vertexCount;
            }
        }

        // 0x004B23D0 (geometryinstancing_d3d11.hpp:41)
        void fillGeometryInstancingBatch(GeometryInstancingBatch* pOut, const GeometryInstancingBatch* pSource, uint32 offset, uint32 instanceCount)
        {
            pOut->pVertexBuffer = pSource->pVertexBuffer;
            pOut->vertexBufferByteOffset = pSource->vertexBufferByteOffset + offset;
            pOut->instanceCount = instanceCount;
            pOut->vertexBufferStride = pSource->vertexBufferStride;
        }

        // 0x00C5C8C0 (:46)
        void setIndexFormatOnBuffer(IndexBufferData* pBuffer, IndexFormat indexFormat)
        {
            pBuffer->indexFormat = indexFormat != IndexFormat_Uint16 ? DXGI_FORMAT_R32_UINT : DXGI_FORMAT_R16_UINT;
        }

        // 0x00C5C8E0 (:52)
        void setPrimitiveTypeOnPrimitiveData(PrimitiveData* pData, PrimitiveType type)
        {
            pData->primitiveTopology = s_primitiveTopology[type];
        }

        // 0x00C62580 (:57)
        bool allocateRenderGeometry(RenderGeometry* pGeometry, GraphicsSystem* pGraphicsSystem, MemoryAllocator* pDataAllocator, const RenderGeometryDescription& description, const char* pDebugName)
        {
            ID3D11Device* pDevice = pGraphicsSystem->pDevice;
            pGeometry->pVertexFormat = description.pVertexFormat;
            pGeometry->pSkinGeometry = nullptr;
            pGeometry->skinningBufferOffset = 0;

            D3D11_BUFFER_DESC bufferDesc;
            memset(&bufferDesc, 0, sizeof(bufferDesc));
            switch (description.cpuAccessMode)
            {
            case RenderGeometryAccessMode_None:
                bufferDesc.Usage = D3D11_USAGE_DEFAULT;
                bufferDesc.CPUAccessFlags = 0;
                break;
            case RenderGeometryAccessMode_Read:
                bufferDesc.Usage = D3D11_USAGE_STAGING;
                bufferDesc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
                break;
            case RenderGeometryAccessMode_Write:
                bufferDesc.Usage = D3D11_USAGE_DYNAMIC;
                bufferDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
                break;
            case RenderGeometryAccessMode_ReadWrite:
                bufferDesc.Usage = D3D11_USAGE_STAGING;
                bufferDesc.CPUAccessFlags = D3D11_CPU_ACCESS_READ | D3D11_CPU_ACCESS_WRITE;
                break;
            default:
                exit(1);
            }
            // Staging buffers cannot be bound.
            const bool isStaging = bufferDesc.Usage == D3D11_USAGE_STAGING;

            if (pGeometry->pVertexFormat == nullptr)
            {
                pGeometry->pVertexBuffer = nullptr;
                pGeometry->vertexCount = 0;
            }
            else
            {
                bufferDesc.ByteWidth = description.pVertexFormat->streamStride[description.streamIndex] * description.vertexCount;
                bufferDesc.BindFlags = isStaging ? 0 : D3D11_BIND_VERTEX_BUFFER;
                const HRESULT result = pDevice->CreateBuffer(&bufferDesc, nullptr, &pGeometry->pVertexBuffer);
                if (FAILED(result))
                {
                    handleFatalHResult(pGraphicsSystem, result, "Create Vertex Buffer");
                }
                pGeometry->vertexCount = description.vertexCount;
            }
            pGeometry->primitiveTopology = s_primitiveTopology[description.primitiveType];
            if (description.indexFormat == IndexFormat_Invalid)
            {
                pGeometry->pIndexBuffer = nullptr;
                pGeometry->indexCount = 0;
                pGeometry->indexFormat = 0;
            }
            else
            {
                // Notes (decompiled behaviour, kept as is): allocated index buffers are always 16-bit.
                bufferDesc.ByteWidth = description.indexCount * 2;
                bufferDesc.BindFlags = isStaging ? 0 : D3D11_BIND_INDEX_BUFFER;
                const HRESULT result = pDevice->CreateBuffer(&bufferDesc, nullptr, &pGeometry->pIndexBuffer);
                if (FAILED(result))
                {
                    handleFatalHResult(pGraphicsSystem, result, "Create Index Buffer");
                }
                pGeometry->indexFormat = DXGI_FORMAT_R16_UINT;
                pGeometry->indexCount = description.indexCount;
            }
            pGeometry->pDebugName = nullptr;
            return true;
        }

        // 0x00C62710 (:182)
        bool initializeRenderGeometry(RenderGeometry* pGeometry, GraphicsSystem* pGraphicsSystem, const RenderGeometryDescription& description, const RenderGeometryInitializationData& initData, const char* pDebugName)
        {
            ID3D11Device* pDevice = pGraphicsSystem->pDevice;
            pGeometry->pVertexFormat = description.pVertexFormat;
            pGeometry->pSkinGeometry = (const SoftwareSkinGeometry*)initData.pSkinningData;

            // Geometry without CPU access is immutable, otherwise dynamic.
            D3D11_BUFFER_DESC bufferDesc;
            bufferDesc.Usage = description.cpuAccessMode != RenderGeometryAccessMode_None ? D3D11_USAGE_DYNAMIC : D3D11_USAGE_IMMUTABLE;
            bufferDesc.CPUAccessFlags = s_cpuAccessFlags[description.cpuAccessMode];
            bufferDesc.MiscFlags = 0;
            bufferDesc.StructureByteStride = 0;
            D3D11_SUBRESOURCE_DATA subresourceData;
            subresourceData.SysMemPitch = 0;
            subresourceData.SysMemSlicePitch = 0;

            if (description.vertexCount == 0)
            {
                pGeometry->pVertexBuffer = nullptr;
            }
            else
            {
                // Skinned geometry keeps its static attributes in stream 1 (positions come from the skinning buffer).
                const uint32 stride = initData.pSkinningData != nullptr ? description.pVertexFormat->streamStride[1] : description.pVertexFormat->streamStride[description.streamIndex];
                bufferDesc.ByteWidth = description.vertexCount * stride;
                bufferDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
                subresourceData.pSysMem = initData.pVertexData;
                const HRESULT result = pDevice->CreateBuffer(&bufferDesc, &subresourceData, &pGeometry->pVertexBuffer);
                if (FAILED(result))
                {
                    handleFatalHResult(pGraphicsSystem, result, "Create Vertex Buffer");
                }
            }
            pGeometry->vertexCount = description.vertexCount;
            pGeometry->primitiveTopology = s_primitiveTopology[description.primitiveType];
            if (description.indexFormat == IndexFormat_Invalid)
            {
                pGeometry->pIndexBuffer = nullptr;
                pGeometry->indexCount = 0;
                pGeometry->indexFormat = 0;
            }
            else
            {
                bufferDesc.ByteWidth = (description.indexFormat != IndexFormat_Uint16 ? 4 : 2) * description.indexCount;
                bufferDesc.BindFlags = D3D11_BIND_INDEX_BUFFER;
                subresourceData.pSysMem = initData.pIndexData;
                const HRESULT result = pDevice->CreateBuffer(&bufferDesc, &subresourceData, &pGeometry->pIndexBuffer);
                if (FAILED(result))
                {
                    handleFatalHResult(pGraphicsSystem, result, "Create Index Buffer");
                }
                pGeometry->indexFormat = description.indexFormat != IndexFormat_Uint16 ? DXGI_FORMAT_R32_UINT : DXGI_FORMAT_R16_UINT;
                pGeometry->indexCount = description.indexCount;
            }
            pGeometry->pMorphData = (const SoftwareMorphData*)initData.pMorphingData;
            pGeometry->pDebugName = nullptr;
            return true;
        }

        // 0x00C5C900 (:300)
        void freeRenderGeometry(RenderGeometry* pGeometry, GraphicsSystem* pGraphicsSystem, MemoryAllocator* pDataAllocator)
        {
            if (pGeometry->pIndexBuffer != nullptr)
            {
                pGeometry->pIndexBuffer->Release();
            }
            if (pGeometry->pVertexBuffer != nullptr)
            {
                pGeometry->pVertexBuffer->Release();
            }
        }

        // 0x00C5C930 (:322)
        IndexFormat getIndexFormat(const RenderGeometry* pGeometry)
        {
            if (pGeometry->indexFormat == DXGI_FORMAT_R32_UINT)
            {
                return IndexFormat_Uint32;
            }
            return pGeometry->indexFormat == DXGI_FORMAT_R16_UINT ? IndexFormat_Uint16 : IndexFormat_Invalid;
        }

        // 0x00C5C950 (:338)
        PrimitiveType getPrimitiveType(const RenderGeometry* pGeometry)
        {
            switch (pGeometry->primitiveTopology)
            {
            case D3D11_PRIMITIVE_TOPOLOGY_LINELIST: return PrimitiveType_LineList;
            case D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST: return PrimitiveType_TriangleList;
            case D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP: return PrimitiveType_TriangleStrip;
            default: return PrimitiveType_Invalid;
            }
        }
    }
}
