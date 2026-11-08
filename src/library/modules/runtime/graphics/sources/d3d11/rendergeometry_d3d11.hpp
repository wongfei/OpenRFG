// Original: library/modules/runtime/graphics/sources/d3d11/rendergeometry_d3d11.hpp
// Implementation: library/modules/runtime/graphics/sources/d3d11/rendergeometry_d3d11.cpp
//
// RenderGeometry (vertex and index buffers with formats and counts), VertexBufferData and IndexBufferData.
#pragma once

#include "keen/base/types.hpp"
#include <d3d11.h>

struct ID3D11Buffer;

namespace keen
{
    struct PrimitiveData;
    struct SoftwareMorphData;
    struct SoftwareSkinGeometry;
    struct VertexFormat;

    // size 0x2C
    struct RenderGeometry
    {
        const VertexFormat* pVertexFormat; // 0x00
        ID3D11Buffer* pVertexBuffer; // 0x04
        uint32 vertexCount; // 0x08
        ID3D11Buffer* pIndexBuffer; // 0x0C
        uint32 indexCount; // 0x10
        const SoftwareSkinGeometry* pSkinGeometry; // 0x14
        uint32 skinningBufferOffset; // 0x18
        const char* pDebugName; // 0x1C
        uint32 indexFormat; // 0x20
        uint32 primitiveTopology; // 0x24
        const SoftwareMorphData* pMorphData; // 0x28
    };

    // size 0xC
    struct VertexBufferData
    {
        ID3D11Buffer* pBuffer; // 0x00
        uint32 vertexCount; // 0x04
        uint32 offset; // 0x08
    };

    // size 0x10
    struct IndexBufferData
    {
        ID3D11Buffer* pBuffer; // 0x00
        uint32 indexCount; // 0x04
        uint32 offset; // 0x08
        uint32 indexFormat; // 0x0C
    };

    namespace graphics
    {
        uint32 getVertexCount(const RenderGeometry* pGeometry); // 0x00C5B580 (.h:72)
        uint32 getIndexCount(const RenderGeometry* pGeometry); // 0x00C5B590 (.h:73)
        uint32 getIndexStride(const RenderGeometry* pGeometry); // 0x004B2230 (.h:74)
        const VertexFormat* getVertexFormat(const RenderGeometry* pGeometry); // 0x004B2250 (.h:76)
        uint32 getSkinningBufferSize(const RenderGeometry* pGeometry); // 0x00CC83E0 (.h:77)
        void fillVertexBufferData(VertexBufferData* pOut, const RenderGeometry* pGeometry, uint32 offset); // 0x004B2260 (.h:80)
        void fillIndexBufferData(IndexBufferData* pOut, const RenderGeometry* pGeometry, uint32 offset); // 0x004B2280 (.h:87)
        void fillPrimitiveData(PrimitiveData* pOut, const RenderGeometry* pGeometry, uint32 vertexDrawCount); // 0x004B22C0 (.h:95)
    }
}
