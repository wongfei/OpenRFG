// Original: library/modules/runtime/graphics/include/keen/graphics/immediaterenderer.hpp
// Implementation: library/modules/runtime/graphics/sources/global/immediaterenderer.cpp
//
// ImmediateRenderer, a debug/overlay renderer that batches lines, triangles, quads and text with simple render states
// (the Immediate* enums). Placement of the enums and ImmediateVertex inferred.
#pragma once

#include "keen/base/types.hpp"
#include "keen/graphics/primitivetype.hpp"
#include "keen/graphics/camera.hpp"
#include "keen/math/matrix43.hpp"
#include "keen/math/matrix44.hpp"

namespace keen
{
    struct AxisAlignedRectangle;
    struct BlendState;
    struct DepthStencilState;
    struct DynamicConstantBuffer;
    class GraphicsCommandWriter;
    struct GraphicsSystem;
    struct RasterizerState;
    struct RenderGeometry;
    struct RenderTarget;
    struct SamplerState;
    struct ScissorRectangle;
    struct ShaderPipeline;
    struct TextureData;
    struct Vector3;
    struct VertexFormat;
    struct VertexInputBinding;

    enum ImmediateBlendMode
    {
        ImmediateBlendMode_Invalid = -1,
        ImmediateBlendMode_None = 0,
        ImmediateBlendMode_Blend_Alpha = 1,
        ImmediateBlendMode_Add_Alpha = 2,
        ImmediateBlendMode_Sub_Alpha = 3,
        ImmediateBlendMode_Add = 4,
        ImmediateBlendMode_Sub = 5,
        ImmediateBlendMode_Count = 6,
    };

    enum ImmediateAlphaMode
    {
        ImmediateAlphaMode_Invalid = -1,
        ImmediateAlphaMode_PremultipliedAlpha = 0,
        ImmediateAlphaMode_NonPremultipliedAlpha = 1,
        ImmediateAlphaMode_Count = 2,
        ImmediateAlphaMode_PlatformDefault = 1,
    };

    enum ImmediateCullMode
    {
        ImmediateCullMode_None = 0,
        ImmediateCullMode_Back = 1,
        ImmediateCullMode_Front = 2,
        ImmediateCullMode_Count = 3,
    };

    enum ImmediateFillMode
    {
        ImmediateFillMode_Solid = 0,
        ImmediateFillMode_Wireframe = 1,
        ImmediateFillMode_Count = 2,
    };

    enum ImmediateDepthTestMode
    {
        ImmediateDepthTestMode_Never = 0,
        ImmediateDepthTestMode_Less = 1,
        ImmediateDepthTestMode_Equal = 2,
        ImmediateDepthTestMode_LessEqual = 3,
        ImmediateDepthTestMode_Greater = 4,
        ImmediateDepthTestMode_NotEqual = 5,
        ImmediateDepthTestMode_GreaterEqual = 6,
        ImmediateDepthTestMode_Always = 7,
        ImmediateDepthTestMode_Count = 8,
    };

    enum ImmediateDepthWriteMode
    {
        ImmediateDepthWriteMode_Enabled = 0,
        ImmediateDepthWriteMode_Disabled = 1,
        ImmediateDepthWriteMode_Count = 2,
    };

    enum ImmediateTextureFilterMode
    {
        ImmediateTextureFilterMode_Nearest = 0,
        ImmediateTextureFilterMode_BiLinear = 1,
        ImmediateTextureFilterMode_TriLinear = 2,
        ImmediateTextureFilterMode_Count = 3,
    };

    enum ImmediateTextureAddressMode
    {
        ImmediateTextureAddressMode_Wrap = 0,
        ImmediateTextureAddressMode_Mirror = 1,
        ImmediateTextureAddressMode_Clamp = 2,
        ImmediateTextureAddressMode_Count = 3,
    };

    // size 0x18
    struct ImmediateVertex
    {
        float x; // 0x00
        float y; // 0x04
        float z; // 0x08
        uint32 color; // 0x0C
        float u; // 0x10
        float v; // 0x14
    };

    // size 0x500
    class ImmediateRenderer
    {
    public:
        void create(GraphicsSystem* pGraphicsSystem); // 0x00C73250 (:69)
        void destroy(GraphicsSystem* pGraphicsSystem); // 0x00C70790 (:150)
        void beginRendering(GraphicsCommandWriter* pCommandWriter, const RenderTarget* pRenderTarget, const Camera* pCamera); // 0x00C6E310 (:191)
        void endRendering(); // 0x00C6E350 (:209)
        void resetState(); // 0x00C6C4B0 (:221)
        const RenderTarget* getBackBufferRenderTarget() const; // 0x00C64680 (:253)
        void beginRenderPass(const RenderTarget* pRenderTarget, const Camera* pCamera); // 0x00C6C810 (:265)
        void endRenderPass(); // 0x00C6C9E0 (:277)
        void clear(uint32 inColor, float inDepth, uint8 inStencilValue, uint32 inClearMask); // 0x00C646C0 (:294)
        void setCamera(const Camera* pCamera); // 0x00C64800 (:301)
        const Camera* getCamera() const; // 0x00C5D1D0 (:317)
        void setScissorRectangle(const ScissorRectangle* pRectangle); // 0x00C64870 (:324)
        uint32 getScreenWidth() const;
        uint32 getScreenHeight() const;
        void setViewProjectionMatrix(const Matrix44&);
        const Matrix44& getViewProjectionMatrix() const;
        void setBlendMode(ImmediateBlendMode blendMode, ImmediateAlphaMode alphaMode); // 0x00C5D1E0 (:329)
        void setDepthMode(ImmediateDepthTestMode depthTestMode, ImmediateDepthWriteMode depthWriteMode); // 0x00C5D260 (:336)
        void setFillMode(ImmediateFillMode fillMode); // 0x00C5D2F0 (:350)
        void setCullMode(ImmediateCullMode cullMode); // 0x00C5D2B0 (:343)
        ImmediateCullMode getCullMode() const;
        const VertexFormat* getVertexFormat() const;
        const TextureData* getWhiteTexture() const;
        const ShaderPipeline* getShaderPipeline() const;
        void setTexture(uint32 stageIndex, const TextureData* pTexture, ImmediateTextureFilterMode filterMode, ImmediateTextureAddressMode addressMode); // 0x00C64900 (:357)
        void setVertexTexture(uint32 stageIndex, const TextureData* pTexture, ImmediateTextureFilterMode filterMode, ImmediateTextureAddressMode addressMode); // 0x00C64A50 (:373)
        void setShaderPipeline(const ShaderPipeline* pShaderPipeline, const VertexInputBinding* pInputBinding); // 0x00C5D330 (:385)
        void setFragmentShaderVolumeSlice(float sliceCoordinate); // 0x00C5DAF0 (:577)
        void* beginDynamicVertexConstants(DynamicConstantBuffer* pParameters, uint32 slotIndex); // 0x00C64B20 (:399)
        void endDynamicVertexConstants(); // 0x00C64B70 (:404)
        void* beginDynamicFragmentConstants(DynamicConstantBuffer* pParameters, uint32 slotIndex); // 0x00C5D3D0 (:409)
        void endDynamicFragmentConstants(); // 0x00C5D420 (:414)
        void setWorldMatrix(const Matrix43& worldMatrix); // 0x00C5D470 (:419)
        const Matrix43& getWorldMatrix() const; // 0x00C5D4D0 (:425)
        void drawTexturedRectangle(const AxisAlignedRectangle& rect, uint32 color, float u0, float v0, float u1, float v1); // 0x00C6E370 (:430)
        void drawTexturedFullscreenRectangle(uint32 color, float u0, float v0, float u1, float v1); // 0x00C6E460 (:454)
        uint32 getMaxVertexCountPerPrimitive() const; // 0x00C5D4E0 (:468)
        void beginPrimitive(PrimitiveType type, uint32 vertexCount); // 0x00C6CBE0 (:473)
        void addVertex(float x, float y, uint32 color, float u, float v); // 0x00C5C9F0 (.h:179)
        void addVertex(const Vector3&, uint32, float, float);
        void addVertex(float x, float y, float z, uint32 color, float u, float v); // 0x00C5C9A0 (.h:158)
        void endPrimitive(); // 0x00C64BC0 (:500)
        void* beginCustomPrimitive(PrimitiveType type, uint32 vertexCount, uint32 vertexStride); // 0x00C6CCD0 (:512)
        void endCustomPrimitive(); // 0x00C64DA0 (:527)
        void drawStaticGeometry(const RenderGeometry& geometry); // 0x00C6CDB0 (:534)
    protected:
        void flushState(); // 0x00C64F80 (:541)
        void updateProjectionMatrix(const Camera* pCamera); // 0x00C5D4F0 (:558)
    public:
        ImmediateRenderer(); // 0x00C5CA70

    protected:
        GraphicsCommandWriter* m_pCommandWriter; // 0x00
        void* m_pVertexData; // 0x04
    private:
        GraphicsSystem* m_pGraphicsSystem; // 0x08
        const RasterizerState* m_pRasterizerStates[3][2]; // 0x0C
        const BlendState* m_pBlendStates[2][6]; // 0x24
        const DepthStencilState* m_pDepthStencilState[8][2]; // 0x54
        const SamplerState* m_pSamplerState[3][3]; // 0x94
        DynamicConstantBuffer* m_pShaderParameterBuffer; // 0xB8
        DynamicConstantBuffer* m_pFragmentParameterBuffer; // 0xBC
        const ShaderPipeline* m_pShaderPipeline; // 0xC0
        const ShaderPipeline* m_pShaderPipelineVolumeSlice; // 0xC4
        const VertexFormat* m_pVertexFormat; // 0xC8
        const VertexInputBinding* m_pVertexInputBinding; // 0xCC
        const VertexInputBinding* m_pVolumeSliceInputBinding; // 0xD0
        const TextureData* m_pWhiteTexture; // 0xD4
        Matrix44 m_viewProjectionMatrix; // 0xD8
        Matrix43 m_worldMatrix; // 0x118
        uint32 m_screenWidth; // 0x148
        uint32 m_screenHeight; // 0x14C
        bool m_mvpMatrixDirty; // 0x150
        bool m_endFinalRenderPass; // 0x151
        ImmediateCullMode m_cullMode; // 0x154
        ImmediateFillMode m_fillMode; // 0x158
        Camera m_renderPassCameraData[4]; // 0x15C
        const Camera* m_renderPassCameras[4]; // 0x4EC
        uint32 m_currentRenderPassStackSize; // 0x4FC
    };
}
