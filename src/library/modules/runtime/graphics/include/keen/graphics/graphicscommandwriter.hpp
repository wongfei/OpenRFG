// Original: library/modules/runtime/graphics/include/keen/graphics/graphicscommandwriter.hpp
// Implementation: library/modules/runtime/graphics/sources/global/graphicscommandwriter.cpp
//
// Seam between Volition render_lib and the keen graphics backend: render_lib records into a GraphicsCommandWriter
// (rfg_set_simulationThreadCommandWriter), and GameFramework owns the main-thread instance
// (GameFrameworkSystem::commandWriter). It caches the bound state to skip redundant commands; most writers are inline
// (sources/global/graphicscommandwriter.inl) and forward to the graphics::write* functions of the command buffer.
#pragma once

#include "keen/base/types.hpp"
#include "keen/graphics/graphicssystem.hpp"
#include "keen/graphics/primitivetype.hpp"
#include "keen/graphics/rendertarget.hpp"

namespace keen
{
    struct BlendState;
    struct DepthStencilState;
    struct DynamicConstantBuffer;
    struct GeometryInstancingBatch;
    struct GeometryInstancingBuffer;
    struct GraphicsCommandBuffer;
    struct GraphicsSystem;
    struct IndexBufferData;
    struct MappedRenderGeometryInfo;
    struct Matrix43;
    struct OcclusionQuery;
    struct PrimitiveData;
    struct RasterizerState;
    struct RenderGeometry;
    struct RenderTarget;
    struct SamplerState;
    struct SkinnedGeometryInstance;
    struct SkinningBatch;
    struct StaticConstantBuffer;
    struct TextureData;
    struct VertexBufferData;
    struct VertexFormat;
    struct VertexInputBinding;

    // size 0x188
    class GraphicsCommandWriter
    {
    public:
        GraphicsCommandWriter(); // 0x00C64440 (:9)
        void beginWriting(GraphicsCommandBuffer* pTarget, GraphicsSystem* pGraphicsSystem); // 0x00C64510 (:28)
        void endWriting(); // 0x00C5D020 (:51)
        GraphicsCommandBuffer* getBuffer(); // 0x004B3400 (.h:22)
        void invalidateState(); // 0x00C5D030 (:59)
        void writeClearCommand(uint32 inColor, float inDepth, uint8 inStencilValue, uint32 inClearMask); // 0x008A67D0 (.h:29)
        void writeClearCommand(const ClearSettings& clearSettings); // 0x004B34C0 (graphicscommandwriter.inl:30)
        void writeBeginRenderPassCommand(const RenderTarget* pRenderTarget, RenderTargetPartFlagMask restoreFlags, const ClearSettings* pClearSettings); // 0x004B35E0 (graphicscommandwriter.inl:49)
        void writeEndRenderPassCommand(RenderTargetPartFlagMask resolveFlags); // 0x004B37F0 (graphicscommandwriter.inl:67)
        void writeSetViewportSettingsCommand(const ViewportSettings* pSettings); // 0x004B3910 (graphicscommandwriter.inl:135)
        void writeSetScissorRectangleCommand(const ScissorRectangle* pRectangle); // 0x004B3A20 (graphicscommandwriter.inl:140)
        void writeSetBlendStateCommand(const BlendState* pState); // 0x004B3AB0 (graphicscommandwriter.inl:145)
        void writeSetRasterizerStateCommand(const RasterizerState* pState); // 0x004B3B20 (graphicscommandwriter.inl:160)
        void writeSetDepthStencilStateCommand(const DepthStencilState* pState, uint32 stencilRefValue); // 0x004B3BA0 (graphicscommandwriter.inl:189)
        void writeSetFragmentShaderTextureCommand(uint32 stageIndex, const TextureData* pTexture, const SamplerState* pState); // 0x004DBAC0 (.h:48)
        void writeSetFragmentShaderTextureCommand(uint32 samplerIndex, const TextureData* pTexture); // 0x004B3BE0 (graphicscommandwriter.inl:205)
        void writeSetFragmentShaderSamplerStateCommand(uint32 samplerIndex, const SamplerState* pState); // 0x004B3B50 (graphicscommandwriter.inl:174)
        void writeSetVertexShaderTextureCommand(uint32 stageIndex, const TextureData* pTexture, const SamplerState* pState); // 0x00C624D0 (.h:56)
        void writeSetVertexShaderTextureCommand(uint32 stageIndex, const TextureData* pTexture); // 0x00C5C7E0 (graphicscommandwriter.inl:484)
        void writeSetVertexShaderSamplerStateCommand(uint32 samplerIndex, const SamplerState* pState); // 0x00C5C830 (graphicscommandwriter.inl:503)
        void clearAllTextureStages(); // 0x004B3C30 (graphicscommandwriter.inl:220)
        void clearAllConstantBuffers(); // 0x00C5C700 (graphicscommandwriter.inl:474)
        void clearAllVertexConstants(); // 0x00C5C580 (graphicscommandwriter.inl:225)
        void clearAllFragmentConstants(); // 0x00C5C600 (graphicscommandwriter.inl:249)
        void* beginDynamicConstantUpdate(DynamicConstantBuffer* pConstantBuffer, uint32 shaderMask); // 0x004B3CB0 (graphicscommandwriter.inl:262)
        void endDynamicConstantUpdate(uint32 slotIndex, uint32 shaderMask); // 0x004B3D00 (graphicscommandwriter.inl:279)
        void* beginDynamicVertexConstants(DynamicConstantBuffer* pConstantBuffer, uint32 slotIndex); // 0x004DBB40 (graphicscommandwriter.inl:237)
        void endDynamicVertexConstants(); // 0x004DBB90 (graphicscommandwriter.inl:243)
        void* beginDynamicFragmentConstants(DynamicConstantBuffer* pConstantBuffer, uint32 slotIndex); // 0x004B3D80 (graphicscommandwriter.inl:307)
        void endDynamicFragmentConstants(); // 0x004B3DD0 (graphicscommandwriter.inl:313)
        void writeSetStaticVertexConstantBuffer(uint32, const StaticConstantBuffer*);
        void writeSetStaticFragmentConstantBuffer(uint32, const StaticConstantBuffer*);
        void* mapStaticConstantBufferData(StaticConstantBuffer*);
        void unmapStaticConstantBufferData(StaticConstantBuffer*);
        void* mapTextureData(MappedTextureInfo* pMappedData, TextureData* pTexture, uint32 level, GraphicsMapType mapType, uint32 mapFlags); // 0x004B3E20 (graphicscommandwriter.inl:369)
        void unmapTextureData(TextureData* pTexture, uint32 level); // 0x004B3E50 (graphicscommandwriter.inl:374)
        void* mapRenderGeometryVertexData(RenderGeometry* pRenderGeometry, GraphicsMapType mapType, uint32 mapFlags, uint32 startOffset, uint32 numBytes, MappedRenderGeometryInfo* pMappedData); // 0x004B3E70 (graphicscommandwriter.inl:379)
        void unmapRenderGeometryVertexData(RenderGeometry* pRenderGeometry, MappedRenderGeometryInfo* pMappedData); // 0x004B3EA0 (graphicscommandwriter.inl:384)
        void* mapRenderGeometryIndexData(RenderGeometry* pRenderGeometry, GraphicsMapType mapType, uint32 mapFlags, uint32 startOffset, uint32 numBytes, MappedRenderGeometryInfo* pMappedData); // 0x00C5C680 (graphicscommandwriter.inl:389)
        void unmapRenderGeometryIndexData(RenderGeometry* pRenderGeometry, MappedRenderGeometryInfo* pMappedData); // 0x00C5C6E0 (graphicscommandwriter.inl:394)
        void writeInvalidateTextureCacheCommand(const TextureData* pData); // 0x004B3EC0 (graphicscommandwriter.inl:399)
        void writeDrawGeometryCommand(const RenderGeometry* pGeometry, const Matrix43& worldMatrix, const SkinnedGeometryInstance* pSkinnedInstance); // 0x004DBBE0 (graphicscommandwriter.inl:404)
        void writeDrawVertexBuffersCommand(const Matrix43& worldMatrix, const VertexBufferData* pVertexBuffers, uint32 bufferCount, const IndexBufferData* pIndexBuffer, const PrimitiveData* pPrimitiveData); // 0x004B3ED0 (graphicscommandwriter.inl:424)
        void writeDrawVertexBuffersCommand(const RenderGeometry*, const Matrix43&, const VertexBufferData*, uint32);
        void writeDrawInstancedGeometryCommand(const RenderGeometry*, const GeometryInstancingBatch*);
        void writeDrawInstancedVertexBuffersCommand(const VertexBufferData* pVertexBuffers, uint32 bufferCount, const IndexBufferData* pIndexBuffer, const PrimitiveData* pPrimitiveData, const GeometryInstancingBatch* pInstancingBatch); // 0x004B3FC0 (graphicscommandwriter.inl:443)
        void writeDrawScreenQuadCommandPos2f(float, float, float, float);
        void writeDrawScreenQuadCommandPos2fTex2f(float, float, float, float, float, float, float, float);
        void writeDrawFullscreenQuadCommandPos2fTex2f(float u0, float v0, float u1, float v1); // 0x004B4530 (graphicscommandwriter.inl:647)
        void writeDrawFullscreenQuadCommandPos2f();
        void writeSetShaderPipelineCommand(const ShaderPipeline* pPipeline, const VertexInputBinding* pInputBinding, bool ignoreCurrentlySetResources); // 0x004B4100 (graphicscommandwriter.inl:456)
        void* beginImmediateGeometryCommand(PrimitiveType type, uint32 vertexCount, uint32 vertexStride); // 0x004B4190 (graphicscommandwriter.inl:518)
        void endImmediateGeometryCommand(); // 0x004B4260 (graphicscommandwriter.inl:535)
        void* beginInstancingBatch(GeometryInstancingBatch* pBatch, GeometryInstancingBuffer* pInstancingBuffer, uint32 instanceCount, uint32 instanceDataSize); // 0x004B4430 (graphicscommandwriter.inl:548)
        void endInstancingBatch(GeometryInstancingBuffer* pInstancingBuffer); // 0x004B44D0 (graphicscommandwriter.inl:554)
        SkinningBatch* beginSkinning();
        void endSkinning(SkinningBatch*);
        void beginOcclusionQuery(OcclusionQuery* pOcclusionQuery); // 0x004B44F0 (graphicscommandwriter.inl:575)
        void endOcclusionQuery(OcclusionQuery* pOcclusionQuery); // 0x004B4510 (graphicscommandwriter.inl:581)
        bool pollOcclusionQueryResult(OcclusionQuery*, uint64*);
        void writeCopyTextureDataCommand(TextureData* pTarget, const TextureData* pSource); // 0x004B4840 (graphicscommandwriter.inl:714)
        void writeCopyTextureLevelDataCommand(TextureData* pTarget, uint32 targetLevel, const TextureData* pSource, uint32 sourceLevel); // 0x00C5C870 (graphicscommandwriter.inl:719)
        void writeCopyGeometryDataCommand(RenderGeometry*, const RenderGeometry*);
        void writeDownsampleDepthBufferCommand(const RenderTarget*, const TextureData*);
        void writeCopyDepthBufferCommand(const RenderTarget*, const TextureData*);
        const RenderTarget* getCurrentRenderTarget() const; // 0x004B3410 (.h:170)
        const RenderTarget* getBackBufferRenderTarget() const; // 0x00C62550 (.h:171)
        const RenderTarget* getNoGammaBackBufferRenderTarget() const;

    private:
        GraphicsCommandBuffer* m_pBuffer; // 0x00
        GraphicsSystem* m_pGraphicsSystem; // 0x04
        const RenderTarget* m_pRenderTarget; // 0x08
        const VertexInputBinding* m_pVertexInputBinding; // 0x0C
        const BlendState* m_pBlendState; // 0x10
        const RasterizerState* m_pRasterizerState; // 0x14
        const DepthStencilState* m_pDepthStencilState; // 0x18
        const SamplerState* m_fragmentShaderSamplerStates[16]; // 0x1C
        const TextureData* m_fragmentShaderTextures[16]; // 0x5C
        const ShaderPipeline* m_pShaderPipeline; // 0x9C
        const TextureData* m_vertexShaderTextures[16]; // 0xA0
        const SamplerState* m_vertexShaderSamplerStates[16]; // 0xE0
        const VertexFormat* m_screenQuadVertexFormats[3]; // 0x120
        const RenderTarget* m_renderPassStack[4]; // 0x12C
        const StaticConstantBuffer* m_currentStaticVertexConstantBuffers[8]; // 0x13C
        const StaticConstantBuffer* m_currentStaticFragmentConstantBuffers[8]; // 0x15C
        uint32 m_renderPassStackIndex; // 0x17C
        uint32 m_stencilRefValue; // 0x180
        uint32 m_mappedConstantBufferSlotIndex; // 0x184
    };
}
