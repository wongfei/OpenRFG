// Original: library/modules/runtime/graphics/sources/d3d11/graphicscommandbuffer_d3d11.hpp
// Implementation: library/modules/runtime/graphics/sources/d3d11/graphicscommandbuffer_d3d11.cpp
//
// GraphicsCommandBuffer, the D3D11 command executor: commands are executed immediately on a device context (immediate
// or deferred), with a 64 KiB scratch buffer for immediate geometry and quads. The command functions are inline here.
#pragma once

#include "keen/base/types.hpp"
#include <d3d11.h>
#include <stdint.h>
#include "keen/graphics/primitivetype.hpp"
#include "keen/graphics/rendertarget.hpp"

struct ID3D11Buffer;
struct ID3D11DeviceContext;

namespace keen
{
    struct BlendState;
    struct ClearSettings;
    struct DepthStencilState;
    struct DownsampleDepthContext;
    struct DynamicConstantBuffer;
    struct GeometryInstancingBatch;
    struct GeometryInstancingBuffer;
    struct IndexBufferData;
    struct Matrix43;
    struct OcclusionQuery;
    struct PrimitiveData;
    struct RasterizerState;
    struct RecordedCommandBuffer;
    struct RenderGeometry;
    struct RenderTarget;
    struct SamplerState;
    struct ScissorRectangle;
    struct ShaderPipeline;
    struct SkinnedGeometryInstance;
    struct SkinningBatch;
    class SkinningD3D11;
    struct TextureData;
    struct VertexBufferData;
    struct VertexFormat;
    struct VertexInputBinding;
    struct ViewportSettings;

    // size 0x1003C
    struct GraphicsCommandBuffer
    {
        ID3D11DeviceContext* pContext; // 0x00
        ID3D11Buffer* pMappedConstantBuffer; // 0x04
        const RenderTarget* pCurrentRenderTarget; // 0x08
        SkinningD3D11* pSkinningBuffer; // 0x0C
        ID3D11Buffer* pImmediateVertexData; // 0x10
        uint32 immediateVertexBufferOffset; // 0x14
        uint32 immediateVertexBufferSize; // 0x18
        uint32 immediateVertexDataStride; // 0x1C
        uint32 immediateVertexCount; // 0x20
        PrimitiveType immediatePrimitiveType; // 0x24
        DownsampleDepthContext* pDownsampleDepthContext; // 0x28
        const VertexFormat* pCurrentlyBoundVertexFormat; // 0x2C
        void* pRenderCommandBufferStorage; // 0x30
        bool quadlistImmediateCommand; // 0x34
        uint8* pCurrentImmediateBuffer; // 0x38
        uint8 quadBuffer[65536]; // 0x3C
    };

    // size 0x10040 (no definition in the PDB; layout from createCommandBuffer/beginCommandBuffer): a deferred context
    // command buffer and the command list it finished into.
    struct RecordedCommandBuffer
    {
        ID3D11CommandList* pCommandList; // 0x00
        GraphicsCommandBuffer commandBuffer; // 0x04
    };

    namespace graphics
    {
        void writeClearCommand(GraphicsCommandBuffer* pCommandBuffer, const ClearSettings& clearSettings); // 0x004B2470 (.h:84)
        void writeBeginRenderPassCommand(GraphicsCommandBuffer* pCommandBuffer, const RenderTarget* pRenderTarget, RenderTargetPartFlagMask restoreFlags, const ClearSettings* pClearSettings); // 0x004B2680 (.h:155)
        void writeEndRenderPassCommand(GraphicsCommandBuffer* pCommandBuffer, RenderTargetPartFlagMask resolveFlags, const RenderTarget* pRenderTarget); // 0x004B2860 (.h:171)
        void writeSetViewportSettingsCommand(GraphicsCommandBuffer* pCommandBuffer, const ViewportSettings* pSettings); // 0x004B2960 (.h:178)
        void writeSetScissorRectangleCommand(GraphicsCommandBuffer* pCommandBuffer, const ScissorRectangle* pRectangle); // 0x004B2A70 (.h:202)
        void writeSetBlendStateCommand(GraphicsCommandBuffer* pCommandBuffer, const BlendState* pState); // 0x004B2B00 (.h:222)
        void writeSetRasterizerStateCommand(GraphicsCommandBuffer* pCommandBuffer, const RasterizerState* pState); // 0x004B2B60 (.h:237)
        void writeSetDepthStencilStateCommand(GraphicsCommandBuffer* pCommandBuffer, const DepthStencilState* pState, uint32 stencilRefValue); // 0x004B2B80 (.h:246)
        void writeSetFragmentShaderTextureCommand(GraphicsCommandBuffer* pCommandBuffer, uint32 stageIndex, const TextureData* pTexture); // 0x004B2BA0 (.h:255)
        void writeSetFragmentShaderSamplerStateCommand(GraphicsCommandBuffer* pCommandBuffer, uint32 samplerIndex, const SamplerState* pSampler); // 0x004B2BE0 (.h:273)
        void writeSetVertexShaderTextureCommand(GraphicsCommandBuffer* pCommandBuffer, uint32 stageIndex, const TextureData* pTexture); // 0x00C5B6C0 (.h:289)
        void writeSetVertexShaderSamplerStateCommand(GraphicsCommandBuffer* pCommandBuffer, uint32 samplerIndex, const SamplerState* pSampler); // 0x00C5B700 (.h:305)
        void writeSetShaderPipelineCommand(GraphicsCommandBuffer* pCommandBuffer, const ShaderPipeline* pPipeline, const VertexInputBinding* pInputBinding, bool ignoreCurrentlySetResources); // 0x004B2C20 (.h:321)
        void* beginImmediateGeometryCommand(GraphicsCommandBuffer* pCommandBuffer, PrimitiveType type, uint32 vertexCount, uint32 vertexStride); // 0x004B2C90 (.h:362)
        void endImmediateGeometryCommand(GraphicsCommandBuffer* pCommandBuffer); // 0x004B2D60 (.h:425)
        void writeInvalidateTextureCacheCommand(GraphicsCommandBuffer* pCommandBuffer, const TextureData* pData); // 0x004B2F30 (.h:488)
        void writeCopyTextureDataCommand(GraphicsCommandBuffer* pCommandBuffer, TextureData* pTarget, const TextureData* pSource); // 0x004B2F40 (.h:498)
        void writeCopyTextureLevelDataCommand(GraphicsCommandBuffer* pCommandBuffer, TextureData* pTarget, uint32 targetLevel, const TextureData* pSource, uint32 sourceLevel); // 0x00C5B740 (.h:506)
        const SkinnedGeometryInstance* skinGeometries(SkinningBatch* pSkinningBatch, const RenderGeometry* pFirstGeometry, uint32 geometryCount, uint32 geometryStride, const Matrix43* pJointMatrices, uint32 jointMatrixCount); // 0x00CC9060 (.h:552)
        const SkinnedGeometryInstance* morphAndskinGeometries(SkinningBatch* pSkinningBatch, const RenderGeometry* pFirstGeometry, uint32 geometryCount, uint32 geometryStride, const Matrix43* pJointMatrices, uint32 jointMatrixCount, const float* pMorphWeights, uint32 morphWeightCount); // 0x00CC9140 (.h:596)
        void* beginInstancingBatch(GraphicsCommandBuffer* pCommandBuffer, GeometryInstancingBatch* pBatch, GeometryInstancingBuffer* pInstancingBuffer, uint32 instanceCount, uint32 instanceDataSize); // 0x004B2F70 (.h:666)
        void endInstancingBatch(GraphicsCommandBuffer* pCommandBuffer, GeometryInstancingBuffer* pInstancingBuffer); // 0x004B3010 (.h:717)
        void clearVertexShaderConstantBuffers(GraphicsCommandBuffer* pCommandBuffer); // 0x00C5B780 (.h:727)
        void clearTextureAndSamplerBindings(GraphicsCommandBuffer* pCommandBuffer); // 0x004B3030 (.h:736)
        void* beginDynamicConstantUpdate(GraphicsCommandBuffer* pCommandBuffer, DynamicConstantBuffer* pBuffer, uint32 shaderMask); // 0x004B30B0 (.h:744)
        void endDynamicConstantUpdate(GraphicsCommandBuffer* pCommandBuffer, uint32 slotIndex, uint32 shaderMask); // 0x004B30F0 (.h:763)
        void clearFragmentShaderConstantBuffers(GraphicsCommandBuffer* pCommandBuffer); // 0x00C5B7D0 (.h:781)
        void writeDrawGeometryCommand(GraphicsCommandBuffer* pCommandBuffer, const RenderGeometry* pGeometry, const Matrix43& worldMatrix, const SkinnedGeometryInstance* pSkinnedInstance); // 0x004DB910 (.h:804)
        void writeDrawInstancedVertexBuffersCommand(GraphicsCommandBuffer* pCommandBuffer, const VertexBufferData* pVertexBuffers, uint32 bufferCount, const IndexBufferData* pIndexBuffer, const PrimitiveData* pPrimitiveData, const GeometryInstancingBatch* pInstancingBatch); // 0x004B3150 (.h:882)
        void writeDrawVertexBuffersCommand(GraphicsCommandBuffer* pCommandBuffer, const Matrix43& worldMatrix, const VertexBufferData* pVertexBuffers, uint32 bufferCount, const IndexBufferData* pIndexBuffer, const PrimitiveData* pPrimitiveData); // 0x004B3280 (.h:924)
        void callCommandBuffer(GraphicsCommandBuffer* pCommandBuffer, RecordedCommandBuffer* pOtherBuffer); // 0x004B3380 (.h:984)
        void beginOcclusionQuery(GraphicsCommandBuffer* pCommandBuffer, OcclusionQuery* pOcclusionQuery); // 0x004B33C0 (.h:996)
        void endOcclusionQuery(GraphicsCommandBuffer* pCommandBuffer, OcclusionQuery* pOcclusionQuery); // 0x004B33E0 (.h:1004)
        bool pollOcclusionQueryResult(GraphicsCommandBuffer* pCommandBuffer, OcclusionQuery* pOcclusionQuery, uint64_t* pResult); // 0x00C5B820 (.h:1012)
    }
}
