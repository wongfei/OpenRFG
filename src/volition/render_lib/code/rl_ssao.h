// Original: volition/render_lib/code/rl_ssao.h
// Implementation: volition/render_lib/code/rl_ssao.cpp
//
// Screen space ambient occlusion (SsaoContext, SsaoRenderSettings). Placement inferred.
#pragma once

#include "game/build/win32/steam/master/shader/ssaoblur_sv.hpp"
#include "game/build/win32/steam/master/shader/ssaocalc_sv.hpp"
#include "game/build/win32/steam/master/shader/ssaominify_sv.hpp"

class rl_camera;

namespace keen
{
    struct BlendState;
    struct DepthStencilState;
    struct DynamicConstantBuffer;
    class GraphicsCommandWriter;
    struct GraphicsSystem;
    class MemoryAllocator;
    struct RasterizerState;
    struct RenderTarget;
    struct SamplerState;
    struct TextureData;
    struct VertexInputBinding;
    struct float2;
}

// size 0xC4
struct SsaoContext
{
    keen::SsaoCalcVariants ssaoCalcVariants; // 0x00
    keen::SsaoBlurVariants ssaoBlurVariants; // 0x4C
    keen::SsaoMinifyVariants ssaoMinifyVariants; // 0x74
    keen::DynamicConstantBuffer* pCalcFragmentShaderConstants; // 0x84
    keen::DynamicConstantBuffer* pBlurFragmentShaderConstants; // 0x88
    const keen::VertexInputBinding* pInputBindingCalc[5]; // 0x8C
    const keen::VertexInputBinding* pInputBindingBlur[3]; // 0xA0
    const keen::SamplerState* pDepthMapSamplerState; // 0xAC
    const keen::DepthStencilState* pDepthStencilState[2]; // 0xB0
    const keen::BlendState* pBlendState[2]; // 0xB8
    const keen::RasterizerState* pRasterizerState; // 0xC0
};

// size 0x8
struct SsaoRenderSettings
{
    float minZ; // 0x00
    float maxZ; // 0x04
};

SsaoContext* createSsaoContext(keen::MemoryAllocator* pAllocator, keen::GraphicsSystem* pGraphicsSystem); // 0x004FCDB0 (:60)
void destroySsaoContext(SsaoContext* pContext, keen::MemoryAllocator* pAllocator, keen::GraphicsSystem* pGraphicsSystem); // 0x004FD030 (:92)
void drawScreenQuadWithDepth(keen::GraphicsCommandWriter* pCommandWriter, float z, float u0, float v0, float u1, float v1); // 0x004D1780 (:119)
void clearDepthClippedPixelsInternal(SsaoContext* pContext, keen::GraphicsCommandWriter* pCommandWriter, float depth); // 0x004D1A90 (:152)
float imagePlanePixelsPerMeter(float vertical_fov, float height); // 0x004D1B90 (:159)
void renderMinifyEffect(SsaoContext* pContext, keen::GraphicsCommandWriter* pCommandWriter, const keen::float2& targetSize, const keen::float2& sourceSize, const keen::TextureData* pInputTexture, const keen::RenderTarget* pOutputTarget); // 0x004FD110 (:166)
void renderBilateralBlurEffect(SsaoContext* pContext, keen::GraphicsCommandWriter* pCommandWriter, float depth, bool clearRemainingPixels, const keen::float2& axis, const keen::TextureData* pInputTexture, const keen::RenderTarget* pOutputTarget); // 0x004FD680 (:190)
void renderSsaoEffect(SsaoContext* pContext, keen::GraphicsCommandWriter* pCommandWriter, const SsaoRenderSettings& settings, rl_camera& camera, const keen::TextureData* pInputDepthTexture, const keen::RenderTarget* pOutputTarget, const keen::RenderTarget* pBlurIntermediateTarget0, const keen::RenderTarget* pBlurIntermediateTarget1); // 0x004FDB90 (:219)
void renderSsaoDebug(SsaoContext* pContext, keen::GraphicsCommandWriter* pCommandWriter, const keen::RenderTarget* pOutputTarget, const keen::TextureData* pSsaoTexture); // 0x004FEA90 (:337)
