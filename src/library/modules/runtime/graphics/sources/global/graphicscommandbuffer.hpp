// Original: library/modules/runtime/graphics/sources/global/graphicscommandbuffer.hpp
// Implementation: library/modules/runtime/graphics/sources/d3d11/graphicscommandbuffer_d3d11.cpp
//
// The command buffer functions implemented out of line by the D3D11 backend (map/unmap of textures and geometry, depth
// copy/downsample). GraphicsCommandBuffer and the inline command functions are in
// sources/d3d11/graphicscommandbuffer_d3d11.hpp.
#pragma once

#include "keen/base/types.hpp"
#include "keen/graphics/graphicssystem.hpp"

namespace keen
{
    struct GraphicsCommandBuffer;
    struct MappedRenderGeometryInfo;
    struct RenderGeometry;
    struct RenderTarget;
    struct TextureData;

    namespace graphics
    {
        void* mapTextureData(MappedTextureInfo* pMappedData, GraphicsCommandBuffer* pCommandBuffer, TextureData* pTexture, uint32 level, GraphicsMapType mapType, uint32 mapFlags); // 0x00C5B850 (graphicscommandbuffer_d3d11.cpp:29)
        void unmapTextureData(GraphicsCommandBuffer* pCommandBuffer, TextureData* pTexture, uint32 level); // 0x00C5B8C0 (graphicscommandbuffer_d3d11.cpp:50)
        void* mapRenderGeometryVertexData(GraphicsCommandBuffer* pCommandBuffer, RenderGeometry* pRenderGeometry, GraphicsMapType mapType, uint32 mapFlags, uint32 startOffset, uint32 numBytes, MappedRenderGeometryInfo* pMappedData); // 0x00C5B8E0 (graphicscommandbuffer_d3d11.cpp:58)
        void unmapRenderGeometryVertexData(GraphicsCommandBuffer* pCommandBuffer, RenderGeometry* pRenderGeometry, MappedRenderGeometryInfo* pMappedData); // 0x00C5B940 (graphicscommandbuffer_d3d11.cpp:81)
        void* mapRenderGeometryIndexData(GraphicsCommandBuffer* pCommandBuffer, RenderGeometry* pRenderGeometry, GraphicsMapType mapType, uint32 mapFlags, uint32 startOffset, uint32 numBytes, MappedRenderGeometryInfo* pMappedData); // 0x00C5B960 (graphicscommandbuffer_d3d11.cpp:89)
        void unmapRenderGeometryIndexData(GraphicsCommandBuffer* pCommandBuffer, RenderGeometry* pRenderGeometry, MappedRenderGeometryInfo* pMappedData); // 0x00C5B9C0 (graphicscommandbuffer_d3d11.cpp:111)
        void writeDownsampleDepthBufferCommand(GraphicsCommandBuffer* pCommandBuffer, const RenderTarget* pTarget, const TextureData* pSourceData); // 0x00C6A5D0 (graphicscommandbuffer_d3d11.cpp:158)
        void writeCopyDepthBufferCommand(GraphicsCommandBuffer* pCommandBuffer, const RenderTarget* pTarget, const TextureData* pSourceData); // 0x00C6A7B0 (graphicscommandbuffer_d3d11.cpp:172)
    }
}
