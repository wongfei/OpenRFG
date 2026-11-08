// Original: volition/render_lib/code/rl_ssao.cpp
//
// Screen space ambient occlusion (config.ini ssao_enabled). rl_scene_renderer::render_scene_part2 runs renderSsaoEffect
// on the scene depth before the base pass: the ssaocalc shader (one of six falloff functions) writes the occlusion of
// the pixels nearer than g_depthThreshold into the first blur target, the farther ones are cleared, then two bilateral
// blur passes (horizontal into the second blur target, vertical into the CLB target's red channel) smooth it. The
// lighting reads it from the CLB texture; the first blur target is also the sun shafts' sky mask (see
// rl_hdr_compositor.cpp). Every pass is a screen quad at the threshold's depth, so the depth test (greater, or less
// equal for the clear) splits the near and far pixels. rfg.exe inlines all the command writer calls.
//
// Not ported (no callers in rfg.exe): renderMinifyEffect and imagePlanePixelsPerMeter (inlined into renderSsaoEffect
// instead); destroySsaoContext is ported but nothing calls it.

#include "volition/render_lib/code/rl_ssao.h"

#include "volition/render_lib/code/rl_camera.h"
#include "volition/render_lib/code/rl_render_lib.h"
#include "volition/render_lib/code/rl_screen_info.h"
#include "volition/render_lib/code/shader/ssaoblurdata.hpp"
#include "volition/render_lib/code/shader/ssaocalcdata.hpp"
#include "keen/base/memoryallocator.hpp"
#include "keen/graphics/graphicscommandwriter.hpp"
#include "keen/graphics/graphicsstateobjects.hpp"
#include "keen/graphics/graphicssystem.hpp"
#include "keen/graphics/rendertarget.hpp"
#include "library/modules/runtime/graphics/sources/d3d11/texturedata_d3d11.hpp"

#include <math.h>
#include <string.h>

float calculate_horizontal_fov_perspective(float vertical_fov, bool use_pixel_aspect_ratio); // 0x004DFB20 (rl_camera.cpp)

// The SSAO tweakables (dev console only in rfg.exe).
static float g_bias = 0.02f; // 0x014FA5FC
static float g_aoClamp = 0.83f; // 0x014FA600
static float g_depthFadeRange = 1.0f; // 0x014FA604
static float g_depthThreshold = 18.0f; // 0x014FA608
static float g_edgeSharpness = 10.0f; // 0x014FA60C
static bool g_enabled = true; // 0x014FA610
static float g_epsilon = 0.4f; // 0x014FA614
static uint32 g_falloff = 1; // 0x014FA618
static float g_intensity = 4.0f; // 0x014FA61C
static float g_imagePlanePixelsPerMeterFactor = 0.32f; // 0x014FA620
static float g_radius = 1.2f; // 0x014FA624

// The immediate vertex (GraphicsStockVertexFormat_Immediate): position, color, texture coordinate.
struct SsaoScreenQuadVertex
{
    float x; // 0x00
    float y; // 0x04
    float z; // 0x08
    uint32 color; // 0x0C
    float u; // 0x10
    float v; // 0x14
};

// 0x004FCDB0 (:60)
// The shader permutations, the calc (0x80 bytes) and blur (0x10 bytes) fragment constant buffers, a point sampler with
// border addressing for the depth, depth tests greater (the near pixels, behind the quad) and less equal (the far
// ones), red-only and RGBA opaque blend states, a solid unculled rasterizer state and the immediate vertex input
// bindings of the first five calc and the three blur permutations.
SsaoContext* createSsaoContext(keen::MemoryAllocator* pAllocator, keen::GraphicsSystem* pGraphicsSystem)
{
    SsaoContext* pContext = (SsaoContext*)pAllocator->allocate(sizeof(SsaoContext), 4, 0, nullptr);
    pContext->ssaoCalcVariants.create(pGraphicsSystem);
    pContext->ssaoBlurVariants.create(pGraphicsSystem);
    pContext->ssaoMinifyVariants.create(pGraphicsSystem);
    pContext->pCalcFragmentShaderConstants = keen::graphics::createDynamicConstantBuffer(pGraphicsSystem, 0, sizeof(keen::SsaoCalcFragmentShaderConstants));
    pContext->pBlurFragmentShaderConstants = keen::graphics::createDynamicConstantBuffer(pGraphicsSystem, 0, sizeof(keen::SsaoBlurFragmentShaderConstants));

    keen::SamplerParameters samplerParameters;
    memset(&samplerParameters, 0, sizeof(samplerParameters));
    samplerParameters.minLod = 0.0f;
    // Magic number: max LOD 1000.
    samplerParameters.maxLod = 1000.0f;
    samplerParameters.maxAnisotropyLevel = 0;
    samplerParameters.minFilter = keen::SamplerFilterMode_Nearest;
    samplerParameters.magFilter = keen::SamplerFilterMode_Nearest;
    samplerParameters.mipFilter = keen::SamplerFilterMode_Nearest;
    samplerParameters.addressU = keen::SamplerAddressMode_Border;
    samplerParameters.addressV = keen::SamplerAddressMode_Border;
    samplerParameters.addressW = keen::SamplerAddressMode_Border;
    samplerParameters.comparisonFunction = keen::ComparisonFunction_Never;
    samplerParameters.alphaKillEnabled = false;
    pContext->pDepthMapSamplerState = keen::graphics::createSamplerState(pGraphicsSystem, samplerParameters);

    pContext->pDepthStencilState[0] = keen::graphics::createDepthStencilState(pGraphicsSystem, true, keen::ComparisonFunction_Greater, false);
    pContext->pDepthStencilState[1] = keen::graphics::createDepthStencilState(pGraphicsSystem, true, keen::ComparisonFunction_LessEqual, false);

    // Opaque copies: render target 0 red only (state 0) or RGBA (state 1), the other targets RGBA.
    keen::BlendParametersEx blendParameters;
    blendParameters.blendOpColor = keen::BlendOperation_None;
    blendParameters.sourceFactorColor = keen::BlendFactor_One;
    blendParameters.destFactorColor = keen::BlendFactor_Zero;
    blendParameters.blendOpAlpha = keen::BlendOperation_None;
    blendParameters.sourceFactorAlpha = keen::BlendFactor_One;
    blendParameters.destFactorAlpha = keen::BlendFactor_Zero;
    for (uint32 i = 0; i < 8; ++i)
    {
        // Magic number: 0xF = RGBA.
        blendParameters.colorWriteMask[i] = 0xF;
    }
    // Magic number: 1 = red.
    blendParameters.colorWriteMask[0] = 0x1;
    pContext->pBlendState[0] = keen::graphics::createBlendState(pGraphicsSystem, blendParameters);
    blendParameters.colorWriteMask[0] = 0xF;
    pContext->pBlendState[1] = keen::graphics::createBlendState(pGraphicsSystem, blendParameters);

    keen::RasterizerParameters rasterizerParameters;
    rasterizerParameters.cullMode = keen::CullMode_None;
    rasterizerParameters.fillMode = keen::FillMode_Solid;
    rasterizerParameters.windingOrder = keen::WindingOrder_Cw;
    rasterizerParameters.flags = 0;
    rasterizerParameters.depthBias = 0.0f;
    rasterizerParameters.depthBiasSlopeScale = 0.0f;
    rasterizerParameters.depthBiasClamp = 0.0f;
    pContext->pRasterizerState = keen::graphics::createRasterizerState(pGraphicsSystem, rasterizerParameters);

    const keen::VertexFormat* pVertexFormat = keen::graphics::getStockVertexFormat(pGraphicsSystem, keen::GraphicsStockVertexFormat_Immediate);
    // Notes (decompiled behaviour, kept as is): only the first five of the six calc permutations get an input binding,
    // so g_falloff 5 would read the first blur binding (pInputBindingBlur follows pInputBindingCalc).
    for (uint32 i = 0; i < 5; ++i)
    {
        pContext->pInputBindingCalc[i] = keen::graphics::createVertexInputBinding(pGraphicsSystem, pVertexFormat, pContext->ssaoCalcVariants.getPipeline(i), keen::GraphicsGeometryModeMask_Immediate);
    }
    for (uint32 i = 0; i < 3; ++i)
    {
        pContext->pInputBindingBlur[i] = keen::graphics::createVertexInputBinding(pGraphicsSystem, pVertexFormat, pContext->ssaoBlurVariants.getPipeline(i), keen::GraphicsGeometryModeMask_Immediate);
    }
    return pContext;
}

// 0x004FD030 (:92)
void destroySsaoContext(SsaoContext* pContext, keen::MemoryAllocator* pAllocator, keen::GraphicsSystem* pGraphicsSystem)
{
    for (uint32 i = 0; i < 3; ++i)
    {
        keen::graphics::destroyVertexInputBinding(pGraphicsSystem, pContext->pInputBindingBlur[i]);
    }
    for (uint32 i = 0; i < 5; ++i)
    {
        keen::graphics::destroyVertexInputBinding(pGraphicsSystem, pContext->pInputBindingCalc[i]);
    }
    keen::graphics::destroyRasterizerState(pGraphicsSystem, pContext->pRasterizerState);
    keen::graphics::destroyBlendState(pGraphicsSystem, pContext->pBlendState[1]);
    keen::graphics::destroyBlendState(pGraphicsSystem, pContext->pBlendState[0]);
    keen::graphics::destroyDepthStencilState(pGraphicsSystem, pContext->pDepthStencilState[1]);
    keen::graphics::destroyDepthStencilState(pGraphicsSystem, pContext->pDepthStencilState[0]);
    keen::graphics::destroySamplerState(pGraphicsSystem, pContext->pDepthMapSamplerState);
    keen::graphics::destroyDynamicConstantBuffer(pGraphicsSystem, pContext->pBlurFragmentShaderConstants);
    keen::graphics::destroyDynamicConstantBuffer(pGraphicsSystem, pContext->pCalcFragmentShaderConstants);
    pContext->ssaoCalcVariants.destroy(pGraphicsSystem);
    pContext->ssaoBlurVariants.destroy(pGraphicsSystem);
    pContext->ssaoMinifyVariants.destroy(pGraphicsSystem);
    pAllocator->free(pContext);
}

// 0x004D1780 (:119)
// One white triangle that covers the screen at depth z: (-1, 1), (-1, -3), (3, 1).
// Notes (decompiled behaviour, kept as is): the far texture coordinates are u1 * 2 and v1 * 2, which equals the linear
// extrapolation only when u0 and v0 are 0.
void drawScreenQuadWithDepth(keen::GraphicsCommandWriter* pCommandWriter, float z, float u0, float v0, float u1, float v1)
{
    SsaoScreenQuadVertex* pVertices = (SsaoScreenQuadVertex*)pCommandWriter->beginImmediateGeometryCommand(keen::PrimitiveType_TriangleList, 3, sizeof(SsaoScreenQuadVertex));
    if (pVertices == nullptr)
    {
        return;
    }
    // Magic number: 0xFFFFFFFF, opaque white.
    pVertices[0].x = -1.0f;
    pVertices[0].y = 1.0f;
    pVertices[0].z = z;
    pVertices[0].color = 0xFFFFFFFF;
    pVertices[0].u = u0;
    pVertices[0].v = v0;
    pVertices[1].x = -1.0f;
    pVertices[1].y = -3.0f;
    pVertices[1].z = z;
    pVertices[1].color = 0xFFFFFFFF;
    pVertices[1].u = u0;
    pVertices[1].v = (float)((double)v1 * 2.0);
    pVertices[2].x = 3.0f;
    pVertices[2].y = 1.0f;
    pVertices[2].z = z;
    pVertices[2].color = 0xFFFFFFFF;
    pVertices[2].u = (float)((double)u1 * 2.0);
    pVertices[2].v = v0;
    pCommandWriter->endImmediateGeometryCommand();
}

// 0x004D1A90 (:152)
// The pixels farther than the quad at depth (the less equal test) set by the blur permutation 1 (the "no occlusion"
// value).
void clearDepthClippedPixelsInternal(SsaoContext* pContext, keen::GraphicsCommandWriter* pCommandWriter, float depth)
{
    pCommandWriter->writeSetDepthStencilStateCommand(pContext->pDepthStencilState[1], 0);
    pCommandWriter->writeSetShaderPipelineCommand(pContext->ssaoBlurVariants.getPipeline(1), pContext->pInputBindingBlur[1], false);
    drawScreenQuadWithDepth(pCommandWriter, depth, 0.0f, 0.0f, 1.0f, 1.0f);
}

// 0x004FD680 (:190)
// One bilateral blur pass along axis (in texels of the output) from pInputTexture into pOutputTarget, for the pixels
// nearer than depth; with clearRemainingPixels the farther ones are cleared and all channels are written, otherwise
// only red (the CLB target's SSAO channel).
void renderBilateralBlurEffect(SsaoContext* pContext, keen::GraphicsCommandWriter* pCommandWriter, float depth, bool clearRemainingPixels, const keen::float2& axis, const keen::TextureData* pInputTexture, const keen::RenderTarget* pOutputTarget)
{
    pCommandWriter->writeBeginRenderPassCommand(pOutputTarget, keen::RenderTargetPartFlagMask_None, nullptr);
    pCommandWriter->writeSetRasterizerStateCommand(pContext->pRasterizerState);
    pCommandWriter->writeSetBlendStateCommand(pContext->pBlendState[clearRemainingPixels ? 1 : 0]);
    pCommandWriter->writeSetDepthStencilStateCommand(pContext->pDepthStencilState[0], 0);
    pCommandWriter->writeSetFragmentShaderSamplerStateCommand(0, pContext->pDepthMapSamplerState);
    pCommandWriter->writeSetFragmentShaderTextureCommand(0, pInputTexture);
    // Notes (decompiled behaviour, kept as is): the padding float of the constants is not written.
    keen::SsaoBlurFragmentShaderConstants* pConstants = (keen::SsaoBlurFragmentShaderConstants*)pCommandWriter->beginDynamicFragmentConstants(pContext->pBlurFragmentShaderConstants, 0);
    pConstants->axis.x = axis.x;
    pConstants->axis.y = axis.y;
    pConstants->edgeSharpness = g_edgeSharpness;
    pCommandWriter->endDynamicFragmentConstants();
    pCommandWriter->writeSetShaderPipelineCommand(pContext->ssaoBlurVariants.getPipeline(0), pContext->pInputBindingBlur[0], false);
    drawScreenQuadWithDepth(pCommandWriter, depth, 0.0f, 0.0f, 1.0f, 1.0f);
    if (clearRemainingPixels)
    {
        clearDepthClippedPixelsInternal(pContext, pCommandWriter, depth);
    }
    pCommandWriter->writeEndRenderPassCommand(keen::RenderTargetPartFlagMask_None);
}

// 0x004FDB90 (:219)
// With g_enabled: the occlusion of the pixels nearer than g_depthThreshold into pBlurIntermediateTarget0 (the rest
// cleared), blurred horizontally into pBlurIntermediateTarget1 (the rest cleared) and vertically into the red channel
// of pOutputTarget. Without it pOutputTarget's red channel is cleared. The calc constants: intensity, AO clamp, bias,
// epsilon; projScale (the depth image plane's pixels per meter at 1 m times g_imagePlanePixelsPerMeterFactor);
// minMaxDepth (settings' min z, 1 / max z); the depth threshold, 1 / far clip, the fade start and 1 / fade range;
// radius (r, r^2, r^6, intensity / r^6); projInfo (tan(hfov / 2), -tan(vfov / 2), 2 tan(hfov / 2) / width,
// -2 tan(vfov / 2) / height); clipInfo (near * far, far, far - near, near); the output and depth sizes with their
// reciprocals.
//
// The x87 arithmetic of rfg.exe runs at double precision (MSVC's default control word), hence the doubles.
void renderSsaoEffect(SsaoContext* pContext, keen::GraphicsCommandWriter* pCommandWriter, const SsaoRenderSettings& settings, rl_camera& camera, const keen::TextureData* pInputDepthTexture, const keen::RenderTarget* pOutputTarget, const keen::RenderTarget* pBlurIntermediateTarget0, const keen::RenderTarget* pBlurIntermediateTarget1)
{
    if (!g_enabled)
    {
        pCommandWriter->writeBeginRenderPassCommand(pOutputTarget, keen::RenderTargetPartFlagMask_None, nullptr);
        pCommandWriter->writeSetRasterizerStateCommand(pContext->pRasterizerState);
        pCommandWriter->writeSetBlendStateCommand(pContext->pBlendState[0]);
        pCommandWriter->writeSetDepthStencilStateCommand(pContext->pDepthStencilState[1], 0);
        pCommandWriter->writeSetShaderPipelineCommand(pContext->ssaoBlurVariants.getPipeline(1), pContext->pInputBindingBlur[1], false);
        drawScreenQuadWithDepth(pCommandWriter, 0.0f, 0.0f, 0.0f, 1.0f, 1.0f);
        pCommandWriter->writeEndRenderPassCommand(keen::RenderTargetPartFlagMask_None);
        return;
    }
    const float output_width = (float)keen::graphics::getRenderTargetWidth(pOutputTarget);
    const float output_height = (float)keen::graphics::getRenderTargetHeight(pOutputTarget);
    const float depth_width = (float)pInputDepthTexture->description.width;
    const float depth_height = (float)pInputDepthTexture->description.height;
    // The camera accessors are inlined.
    const float near_clip = camera.get_near_clip();
    const float far_clip = camera.get_far_clip();
    const float far_times_near = (float)((double)far_clip * (double)near_clip);
    const float far_minus_near = (float)((double)far_clip - (double)near_clip);
    // The device depth of g_depthThreshold: D3D's z = (far - far * near / d) / (far - near).
    const float threshold_depth = (float)-(((double)far_times_near / (double)g_depthThreshold - (double)far_clip) / (double)far_minus_near);

    pCommandWriter->writeBeginRenderPassCommand(pBlurIntermediateTarget0, keen::RenderTargetPartFlagMask_None, nullptr);
    pCommandWriter->writeSetRasterizerStateCommand(pContext->pRasterizerState);
    pCommandWriter->writeSetBlendStateCommand(pContext->pBlendState[1]);
    pCommandWriter->writeSetDepthStencilStateCommand(pContext->pDepthStencilState[0], 0);
    keen::SsaoCalcFragmentShaderConstants* pConstants = (keen::SsaoCalcFragmentShaderConstants*)pCommandWriter->beginDynamicFragmentConstants(pContext->pCalcFragmentShaderConstants, 0);
    pConstants->intensity = g_intensity;
    pConstants->aoClamp = g_aoClamp;
    pConstants->bias = g_bias;
    pConstants->epsilon = g_epsilon;
    const float fov = camera.get_fov();
    float horizontal_fov;
    if (camera.get_type() == rl_camera::RLCT_PERSPECTIVE)
    {
        horizontal_fov = calculate_horizontal_fov_perspective(fov, false);
    }
    else
    {
        horizontal_fov = (float)((double)rl_render_lib::get_instance()->get_screen_info()->get_aspect_ratio() * (double)fov);
    }
    // imagePlanePixelsPerMeter(fov, depth height), inlined. Magic numbers: 0.5 (the half angle) and 0.017453 (degrees
    // to radians, as a float).
    const float tan_half_v_scale = (float)tan((double)(float)((double)fov * 0.5 * (double)0.017453f));
    const float twice_tan = tan_half_v_scale + tan_half_v_scale;
    const float pixels_per_meter = (float)((double)depth_height / (double)twice_tan);
    pConstants->_dummy0 = 0.0f;
    pConstants->projScale = pixels_per_meter * g_imagePlanePixelsPerMeterFactor;
    pConstants->minMaxDepth.x = settings.minZ;
    pConstants->minMaxDepth.y = (float)(1.0 / (double)settings.maxZ);
    pConstants->depthThreshold = g_depthThreshold;
    pConstants->depthThresholdRcp = (float)(1.0 / (double)far_clip);
    pConstants->depthFadeStart = (float)((double)g_depthThreshold - (double)g_depthFadeRange);
    pConstants->depthVisibleRangeRcp = (float)(1.0 / (double)g_depthFadeRange);
    const float radius_squared = (float)((double)g_radius * (double)g_radius);
    const float radius_cubed = (float)((double)g_radius * (double)radius_squared);
    const float radius_6 = (float)((double)radius_cubed * (double)radius_cubed);
    pConstants->radius.x = g_radius;
    pConstants->radius.y = radius_squared;
    pConstants->radius.z = radius_6;
    pConstants->radius.w = (float)((double)g_intensity / (double)radius_6);
    // Magic numbers: 0.017453 (degrees to radians, as a float) and 0.5 (the half angle).
    const float tan_half_h = (float)tan((double)(float)((double)horizontal_fov * (double)0.017453f * 0.5));
    const float tan_half_v = (float)tan((double)(float)((double)fov * (double)0.017453f * 0.5));
    pConstants->projInfo.x = tan_half_h;
    pConstants->projInfo.y = -tan_half_v;
    // Magic number: 2.0, the clip space width.
    pConstants->projInfo.z = (float)((double)tan_half_h * 2.0 / (double)output_width);
    pConstants->projInfo.w = (float)-((double)tan_half_v * 2.0 / (double)output_height);
    pConstants->clipInfo.x = far_times_near;
    pConstants->clipInfo.y = far_clip;
    pConstants->clipInfo.z = far_minus_near;
    pConstants->clipInfo.w = near_clip;
    const float inverse_output_width = (float)(1.0 / (double)output_width);
    const float inverse_output_height = (float)(1.0 / (double)output_height);
    pConstants->targetSize.x = output_width;
    pConstants->targetSize.y = output_height;
    pConstants->targetSize.z = inverse_output_width;
    pConstants->targetSize.w = inverse_output_height;
    pConstants->sourceSize.x = depth_width;
    pConstants->sourceSize.y = depth_height;
    pConstants->sourceSize.z = (float)(1.0 / (double)depth_width);
    pConstants->sourceSize.w = (float)(1.0 / (double)depth_height);
    pCommandWriter->endDynamicFragmentConstants();
    pCommandWriter->writeSetFragmentShaderSamplerStateCommand(0, pContext->pDepthMapSamplerState);
    pCommandWriter->writeSetFragmentShaderTextureCommand(0, pInputDepthTexture);
    pCommandWriter->writeSetShaderPipelineCommand(pContext->ssaoCalcVariants.getPipeline(g_falloff), pContext->pInputBindingCalc[g_falloff], false);
    drawScreenQuadWithDepth(pCommandWriter, threshold_depth, 0.0f, 0.0f, 1.0f, 1.0f);
    clearDepthClippedPixelsInternal(pContext, pCommandWriter, threshold_depth);
    pCommandWriter->writeEndRenderPassCommand(keen::RenderTargetPartFlagMask_None);

    const keen::TextureData* pOcclusionTexture = keen::graphics::getRenderTargetColorBuffer(pBlurIntermediateTarget0, 0);
    const keen::TextureData* pHorizontalTexture = keen::graphics::getRenderTargetColorBuffer(pBlurIntermediateTarget1, 0);
    const keen::float2 horizontal = { inverse_output_width, 0.0f };
    renderBilateralBlurEffect(pContext, pCommandWriter, threshold_depth, true, horizontal, pOcclusionTexture, pBlurIntermediateTarget1);
    const keen::float2 vertical = { 0.0f, inverse_output_height };
    renderBilateralBlurEffect(pContext, pCommandWriter, threshold_depth, false, vertical, pHorizontalTexture, pOutputTarget);
}

// 0x004FEA90 (:337)
// The SSAO texture shown in pOutputTarget with the blur permutation 2 (g_ssaoVisEnabled).
void renderSsaoDebug(SsaoContext* pContext, keen::GraphicsCommandWriter* pCommandWriter, const keen::RenderTarget* pOutputTarget, const keen::TextureData* pSsaoTexture)
{
    pCommandWriter->writeBeginRenderPassCommand(pOutputTarget, keen::RenderTargetPartFlagMask_None, nullptr);
    pCommandWriter->writeSetRasterizerStateCommand(pContext->pRasterizerState);
    pCommandWriter->writeSetBlendStateCommand(pContext->pBlendState[1]);
    pCommandWriter->writeSetDepthStencilStateCommand(pContext->pDepthStencilState[1], 0);
    pCommandWriter->writeSetFragmentShaderSamplerStateCommand(0, pContext->pDepthMapSamplerState);
    pCommandWriter->writeSetFragmentShaderTextureCommand(0, pSsaoTexture);
    pCommandWriter->writeSetShaderPipelineCommand(pContext->ssaoBlurVariants.getPipeline(2), pContext->pInputBindingBlur[2], false);
    drawScreenQuadWithDepth(pCommandWriter, 0.0f, 0.0f, 0.0f, 1.0f, 1.0f);
    pCommandWriter->writeEndRenderPassCommand(keen::RenderTargetPartFlagMask_None);
}
