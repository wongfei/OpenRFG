// Original: volition/render_lib/code/rl_clb_projector.cpp
//
// The CLB (cascaded light buffer) projector: renders the shadow maps and projection textures (the cloud shadows) into
// the CLB target in screen space. A full-screen triangle at a range's far distance reconstructs each pixel's view
// position from the scene depth, looks it up in the shadow map (4-tap PCF) and subtracts the shadow from the green
// channel; the mid, far and terrain ranges first mark their pixels in the stencil (setupStencilForDepthLayer).
//
// Partial: drawPlane is not ported (no caller in the retail build).

#include "volition/render_lib/code/rl_clb_projector.h"

#include "volition/render_lib/code/rl_camera.h"
#include "volition/render_lib/code/rl_render_lib.h"
#include "volition/render_lib/code/rl_renderer.h"
#include "volition/render_lib/code/rl_renderer_core.h"
#include "volition/vlib/code/math/matrix.h"
#include "game/build/win32/steam/master/shader/clbprojector_sv.hpp"
#include "game/build/win32/steam/master/shader/clbstencilopt_sv.hpp"
#include "keen/graphics/graphicscommandwriter.hpp"
#include "keen/graphics/graphicsstateobjects.hpp"
#include "keen/graphics/immediaterenderer.hpp"
#include "library/modules/runtime/graphics/sources/d3d11/texturedata_d3d11.hpp"
#include "keen/graphics/graphicssystem.hpp"

#include <math.h>
#include <string.h>

float calculate_horizontal_fov_perspective(float vertical_fov, bool use_pixel_aspect_ratio); // 0x004DFB20 (rl_camera.cpp)
extern rl_shadow_pass_parameters Shadow_pass_parameters[NUM_SHADOW_MAP_PASSES]; // 0x014FA5A8 (rl_shadow_system.cpp)

// size 0xCC
struct ClbProjectorContext
{
    keen::ClbProjectorVariants projectorVariants; // 0x00
    keen::ClbStencilOptVariants stencilOptVariants; // 0x64
    keen::DynamicConstantBuffer* pFragmentConstantBuffer; // 0x74
    keen::DynamicConstantBuffer* pVertexConstantBuffer; // 0x78
    const keen::BlendState* pBlendStateNone; // 0x7C
    const keen::BlendState* pBlendStateSubAlpha; // 0x80
    const keen::DepthStencilState* pDepthStencilStateDefault; // 0x84
    const keen::DepthStencilState* pDepthStencilStateNear; // 0x88
    const keen::DepthStencilState* pDepthStencilStateFar; // 0x8C
    const keen::DepthStencilState* pDepthStencilStateRead; // 0x90
    const keen::DepthStencilState* pDepthStencilStateNone; // 0x94
    const keen::RasterizerState* pRasterizerState; // 0x98
    const keen::SamplerState* pSamplerStatePoint; // 0x9C
    const keen::SamplerState* pSamplerStateComparison; // 0xA0
    const keen::SamplerState* pSamplerStateLinear; // 0xA4
    const keen::VertexInputBinding* pProjectorInputBindingPos2f[4]; // 0xA8
    const keen::VertexInputBinding* pProjectorInputBindingImmediate[4]; // 0xB8
    const keen::VertexInputBinding* pStencilOptInputBinding; // 0xC8
};

static ClbProjectorContext g_clbProjectorContext; // 0x01B6EB00
static bool Is_initialized = false; // 0x01B7BC50

// The stencil state of both faces.
static void set_stencil(keen::DepthStencilParameters& parameters, bool test_enabled, keen::ComparisonFunction test_func, uint32 write_mask, keen::StencilOperation op_depth_fail, keen::StencilOperation op_depth_pass)
{
    keen::DepthStencilParameters::StencilParameters* faces[2] = {&parameters.frontStencil, &parameters.backStencil};
    for (int i = 0; i < 2; i++)
    {
        faces[i]->testEnabled = test_enabled;
        faces[i]->testFunc = test_func;
        // Magic number: all 8 stencil bits are tested.
        faces[i]->testMask = 0xFF;
        faces[i]->writeMask = write_mask;
        faces[i]->opFail = keen::StencilOperation_Keep;
        faces[i]->opDepthFail = op_depth_fail;
        faces[i]->opDepthPass = op_depth_pass;
    }
}

// 0x004DFD30 (:83)
// Creates the shader variants, their input bindings (the position+uv and the immediate vertex formats, all geometry
// modes) and the fixed device states. The stencil layers mark the pixels between the shadow range planes (near
// replaces, far decrements) and the projection reads the marked pixels (equal).
void rl_clb_projector_system_init()
{
    keen::GraphicsSystem* graphics_system_p = Init_params.m_pKeenGraphicsSystem;
    g_clbProjectorContext.projectorVariants.create(graphics_system_p);
    g_clbProjectorContext.stencilOptVariants.create(graphics_system_p);
    const keen::VertexFormat* immediate_format_p = keen::graphics::getStockVertexFormat(graphics_system_p, keen::GraphicsStockVertexFormat_Immediate);
    const keen::VertexFormat* pos2f_format_p = keen::graphics::getStockVertexFormat(graphics_system_p, keen::GraphicsStockVertexFormat_Pos2f_Tex2f);
    // Magic number: all geometry modes (0x7). One binding per lod of the projectionmap 0 variants.
    for (uint32 lod = 0; lod < 4; lod++)
    {
        g_clbProjectorContext.pProjectorInputBindingPos2f[lod] = keen::graphics::createVertexInputBinding(graphics_system_p, pos2f_format_p, g_clbProjectorContext.projectorVariants.getPipeline(lod, 0), 7);
    }
    for (uint32 lod = 0; lod < 4; lod++)
    {
        g_clbProjectorContext.pProjectorInputBindingImmediate[lod] = keen::graphics::createVertexInputBinding(graphics_system_p, immediate_format_p, g_clbProjectorContext.projectorVariants.getPipeline(lod, 0), 7);
    }
    g_clbProjectorContext.pStencilOptInputBinding = keen::graphics::createVertexInputBinding(graphics_system_p, immediate_format_p, g_clbProjectorContext.stencilOptVariants.getPipeline(), 7);

    // No blending and no color writes to target 0.
    keen::BlendParametersEx blend;
    blend.blendOpColor = keen::BlendOperation_None;
    blend.sourceFactorColor = keen::BlendFactor_One;
    blend.destFactorColor = keen::BlendFactor_Zero;
    blend.blendOpAlpha = keen::BlendOperation_None;
    blend.sourceFactorAlpha = keen::BlendFactor_One;
    blend.destFactorAlpha = keen::BlendFactor_Zero;
    memset(blend.colorWriteMask, keen::ColorWriteMask_All, sizeof(blend.colorWriteMask));
    blend.colorWriteMask[0] = 0;
    g_clbProjectorContext.pBlendStateNone = keen::graphics::createBlendState(graphics_system_p, blend);
    // dest - src * alpha, green only.
    blend.blendOpColor = keen::BlendOperation_Subtract;
    blend.sourceFactorColor = keen::BlendFactor_SrcAlpha;
    blend.destFactorColor = keen::BlendFactor_One;
    blend.blendOpAlpha = keen::BlendOperation_Subtract;
    blend.sourceFactorAlpha = keen::BlendFactor_SrcAlpha;
    blend.destFactorAlpha = keen::BlendFactor_One;
    memset(blend.colorWriteMask, keen::ColorWriteMask_All, sizeof(blend.colorWriteMask));
    blend.colorWriteMask[0] = keen::ColorWriteMask_Green;
    g_clbProjectorContext.pBlendStateSubAlpha = keen::graphics::createBlendState(graphics_system_p, blend);

    keen::RasterizerParameters rasterizer;
    rasterizer.cullMode = keen::CullMode_None;
    rasterizer.fillMode = keen::FillMode_Solid;
    rasterizer.windingOrder = keen::WindingOrder_Cw;
    rasterizer.flags = 0;
    rasterizer.depthBias = 0.0f;
    rasterizer.depthBiasSlopeScale = 0.0f;
    rasterizer.depthBiasClamp = 0.0f;
    g_clbProjectorContext.pRasterizerState = keen::graphics::createRasterizerState(graphics_system_p, rasterizer);

    keen::SamplerParameters sampler;
    memset(&sampler, 0, sizeof(sampler));
    // Magic number: no LOD clamp.
    sampler.maxLod = 1000.0f;
    sampler.minLod = 0.0f;
    sampler.maxAnisotropyLevel = 0;
    sampler.comparisonFunction = keen::ComparisonFunction_Never;
    sampler.alphaKillEnabled = false;
    sampler.addressU = keen::SamplerAddressMode_Clamp;
    sampler.addressV = keen::SamplerAddressMode_Clamp;
    sampler.addressW = keen::SamplerAddressMode_Clamp;
    sampler.minFilter = keen::SamplerFilterMode_Nearest;
    sampler.magFilter = keen::SamplerFilterMode_Nearest;
    sampler.mipFilter = keen::SamplerFilterMode_Nearest;
    g_clbProjectorContext.pSamplerStatePoint = keen::graphics::createSamplerState(graphics_system_p, sampler);
    sampler.minFilter = keen::SamplerFilterMode_Linear;
    sampler.magFilter = keen::SamplerFilterMode_Linear;
    sampler.mipFilter = keen::SamplerFilterMode_Linear;
    sampler.comparisonFunction = keen::ComparisonFunction_LessEqual;
    g_clbProjectorContext.pSamplerStateComparison = keen::graphics::createSamplerState(graphics_system_p, sampler);
    sampler.addressU = keen::SamplerAddressMode_Wrap;
    sampler.addressV = keen::SamplerAddressMode_Wrap;
    sampler.comparisonFunction = keen::ComparisonFunction_Never;
    g_clbProjectorContext.pSamplerStateLinear = keen::graphics::createSamplerState(graphics_system_p, sampler);

    // Magic numbers: 0x140 and 0x40 bytes of fragment and vertex constants.
    g_clbProjectorContext.pFragmentConstantBuffer = keen::graphics::createDynamicConstantBuffer(graphics_system_p, 0, 0x140);
    g_clbProjectorContext.pVertexConstantBuffer = keen::graphics::createDynamicConstantBuffer(graphics_system_p, 0, 0x40);

    keen::DepthStencilParameters depth_stencil;
    set_stencil(depth_stencil, false, keen::ComparisonFunction_Always, 0, keen::StencilOperation_Keep, keen::StencilOperation_Keep);
    depth_stencil.depthComparisonFunction = keen::ComparisonFunction_Greater;
    depth_stencil.depthTestEnabled = true;
    depth_stencil.depthWriteEnabled = false;
    g_clbProjectorContext.pDepthStencilStateDefault = keen::graphics::createDepthStencilState(graphics_system_p, depth_stencil);
    // Magic number: 0xFF, every stencil bit is written.
    set_stencil(depth_stencil, true, keen::ComparisonFunction_Always, 0xFF, keen::StencilOperation_Zero, keen::StencilOperation_Replace);
    depth_stencil.depthComparisonFunction = keen::ComparisonFunction_LessEqual;
    depth_stencil.depthTestEnabled = true;
    depth_stencil.depthWriteEnabled = false;
    g_clbProjectorContext.pDepthStencilStateNear = keen::graphics::createDepthStencilState(graphics_system_p, depth_stencil);
    set_stencil(depth_stencil, true, keen::ComparisonFunction_Always, 0xFF, keen::StencilOperation_Decrement, keen::StencilOperation_Zero);
    depth_stencil.depthComparisonFunction = keen::ComparisonFunction_LessEqual;
    depth_stencil.depthTestEnabled = true;
    depth_stencil.depthWriteEnabled = false;
    g_clbProjectorContext.pDepthStencilStateFar = keen::graphics::createDepthStencilState(graphics_system_p, depth_stencil);
    set_stencil(depth_stencil, true, keen::ComparisonFunction_Equal, 0, keen::StencilOperation_Keep, keen::StencilOperation_Keep);
    depth_stencil.depthComparisonFunction = keen::ComparisonFunction_Greater;
    depth_stencil.depthTestEnabled = true;
    depth_stencil.depthWriteEnabled = false;
    g_clbProjectorContext.pDepthStencilStateRead = keen::graphics::createDepthStencilState(graphics_system_p, depth_stencil);
    // The read state with the depth and stencil tests off.
    depth_stencil.depthComparisonFunction = keen::ComparisonFunction_LessEqual;
    depth_stencil.depthTestEnabled = false;
    depth_stencil.frontStencil.testEnabled = false;
    depth_stencil.backStencil = depth_stencil.frontStencil;
    g_clbProjectorContext.pDepthStencilStateNone = keen::graphics::createDepthStencilState(graphics_system_p, depth_stencil);
    Is_initialized = true;
}

// 0x004BAE50 (:181)
// Notes (decompiled behaviour, kept as is): nothing is destroyed.
void rl_clb_projector_system_shutdown()
{
}

// 0x004BAE60 (:185)
// A screen-covering triangle (-1,1), (-1,-3), (3,1) at the given depth (white, uv 0..2) in immediate vertices.
void drawScreenQuadWithDepth(keen::GraphicsCommandWriter* pCommandWriter, float depth)
{
    // Magic numbers: 3 vertices of 0x18 bytes (keen::ImmediateVertex).
    keen::ImmediateVertex* pVertices = (keen::ImmediateVertex*)pCommandWriter->beginImmediateGeometryCommand(keen::PrimitiveType_TriangleList, 3, sizeof(keen::ImmediateVertex));
    if (pVertices == nullptr)
    {
        return;
    }
    const float positions[3][2] = { { -1.0f, 1.0f }, { -1.0f, -3.0f }, { 3.0f, 1.0f } };
    const float uvs[3][2] = { { 0.0f, 0.0f }, { 0.0f, 2.0f }, { 2.0f, 0.0f } };
    for (int i = 0; i < 3; i++)
    {
        pVertices[i].x = positions[i][0];
        pVertices[i].y = positions[i][1];
        pVertices[i].z = depth;
        pVertices[i].color = 0xFFFFFFFF;
        pVertices[i].u = uvs[i][0];
        pVertices[i].v = uvs[i][1];
    }
    pCommandWriter->endImmediateGeometryCommand();
}

// The projected depth of a view distance (z / w of (0, 0, range, 1) through the projection).
static float projected_depth(const matrix44& projection, float range)
{
    vector4 v(0.0f, 0.0f, range, 1.0f);
    v.rotate(projection);
    return (float)((double)v.z / (double)v.w);
}

// 0x004BB150 (:218)
void drawScreenQuadWithShadowRange(keen::GraphicsCommandWriter* pCommandWriter, rl_camera& camera, float range)
{
    drawScreenQuadWithDepth(pCommandWriter, projected_depth(camera.get_projection_transform(), range));
}

// 0x004BB910 (:252)
float getNearRange(const float* pShadowRangeParams)
{
    return pShadowRangeParams[0];
}

// 0x004BB920 (:257)
// The far distance plus the fade-out width.
float getFarRange(const float* pShadowRangeParams)
{
    return 1.0f / pShadowRangeParams[3] + pShadowRangeParams[1];
}

// 0x004E0340 (:263)
// Marks the pixels behind a range's near plane in the stencil (no color writes): a screen quad at the near distance
// replaces the stencil with layer + 2 where it passes the depth test and zeroes it where it fails. Layer 0 marks
// behind the far distance with 1 instead and returns 0.
unsigned char setupStencilForDepthLayer(rl_renderer& renderer, rl_camera& camera, int layer, const float* pShadowRangeParams)
{
    keen::GraphicsCommandWriter* pCommandWriter = renderer.getCommandWriter();
    pCommandWriter->writeSetBlendStateCommand(g_clbProjectorContext.pBlendStateNone);
    pCommandWriter->writeSetShaderPipelineCommand(g_clbProjectorContext.stencilOptVariants.getPipeline(), g_clbProjectorContext.pStencilOptInputBinding, true);
    if (layer != 0)
    {
        const unsigned int stencil_ref = (unsigned int)(layer + 2) & 0xFF;
        pCommandWriter->writeSetDepthStencilStateCommand(g_clbProjectorContext.pDepthStencilStateNear, stencil_ref);
        drawScreenQuadWithDepth(pCommandWriter, projected_depth(camera.get_projection_transform(), getNearRange(pShadowRangeParams)));
        return (unsigned char)(layer + 2);
    }
    pCommandWriter->writeSetDepthStencilStateCommand(g_clbProjectorContext.pDepthStencilStateNear, 1);
    drawScreenQuadWithDepth(pCommandWriter, projected_depth(camera.get_projection_transform(), getFarRange(pShadowRangeParams)));
    return 0;
}

// The camera space to shadow texture matrix (the camera's frame relative to the render offset, then the shadow
// texture matrix), transposed for the shader's row-major float4x4.
static void write_shadow_map_matrix(float* pTarget, const rl_camera& camera, const vector& render_offset, const rl_shadow_render_parameters& params)
{
    const matrix& orient = camera.get_rotation();
    const vector& pos = camera.get_position();
    matrix44 camera_frame;
    camera_frame.set(orient.rvec.x, orient.rvec.y, orient.rvec.z, 0.0f,
                     orient.uvec.x, orient.uvec.y, orient.uvec.z, 0.0f,
                     orient.fvec.x, orient.fvec.y, orient.fvec.z, 0.0f,
                     pos.x - render_offset.x, pos.y - render_offset.y, pos.z - render_offset.z, 1.0f);
    const matrix44 product = camera_frame * params.shadow_texture_matrix;
    for (unsigned int row = 0; row < 4; row++)
    {
        for (unsigned int col = 0; col < 4; col++)
        {
            pTarget[col * 4 + row] = product.element(row, col);
        }
    }
}

// 0x004E05D0 (:294)
// The projector's fragment constants (g_fragmentData, 0x140 bytes): shadowMapMatrix0 (camera space to the first
// texture), nearClipParams (near, far, tan(hfov / 2), tan(vfov / 2)), nearFarDepthParams (1 / near, far * near,
// far - near), minMaxDepthParams (minZ, 1 / (maxZ - minZ)), shadowFadeParams (the range fade reciprocals and their
// products with the near/far distances), shadowPercentParams0, the 4-tap PCF offsets and the shadow texture size,
// the viewport size; with a second texture also shadowPercentParams1 and shadowMapMatrix1.
void rl_clb_projector_system_setFragmentConstants(keen::GraphicsCommandWriter* pCommandWriter, const keen::ViewportSettings& viewport, const ClbProjectorRenderSettings& settings, rl_camera& camera, const keen::TextureData* pShadowDepthTexture, const rl_shadow_render_parameters& renderParams0, const rl_shadow_render_parameters* pRenderParams1)
{
    float* pConstants = (float*)pCommandWriter->beginDynamicFragmentConstants(g_clbProjectorContext.pFragmentConstantBuffer, 0);
    write_shadow_map_matrix(pConstants, camera, settings.renderOffset, renderParams0);
    const float near_clip = camera.get_near_clip();
    const float far_clip = camera.get_far_clip();
    const float fov = camera.get_fov();
    float horizontal_fov;
    if (camera.get_type() == rl_camera::RLCT_PERSPECTIVE)
    {
        horizontal_fov = calculate_horizontal_fov_perspective(fov, false);
    }
    else
    {
        horizontal_fov = rl_render_lib::get_instance()->get_screen_info()->get_aspect_ratio() * fov;
    }
    // Magic number: 0.017453, degrees to radians.
    const float tan_half_h = (float)tan((double)(horizontal_fov * 0.017453f * 0.5f));
    const float tan_half_v = (float)tan((double)(fov * 0.017453f * 0.5f));
    // nearClipParams (offset 64)
    pConstants[16] = near_clip;
    pConstants[17] = far_clip;
    pConstants[18] = tan_half_h;
    pConstants[19] = tan_half_v;
    // nearFarDepthParams (80)
    pConstants[20] = 1.0f / near_clip;
    pConstants[21] = far_clip * near_clip;
    pConstants[22] = far_clip - near_clip;
    pConstants[23] = 0.0f;
    // minMaxDepthParams (96)
    pConstants[24] = settings.minZ;
    pConstants[25] = 1.0f / (settings.maxZ - settings.minZ);
    pConstants[26] = 0.0f;
    pConstants[27] = 0.0f;
    // shadowFadeParams (112)
    pConstants[28] = settings.shadowRangeFadeParams[2];
    pConstants[29] = settings.shadowRangeFadeParams[3];
    pConstants[30] = settings.shadowRangeFadeParams[0] * settings.shadowRangeFadeParams[2];
    pConstants[31] = settings.shadowRangeFadeParams[1] * settings.shadowRangeFadeParams[3];
    // shadowPercentParams0 (128)
    pConstants[32] = renderParams0.shadow_percent;
    pConstants[33] = renderParams0.shadow_percent_bias;
    pConstants[34] = 0.0f;
    pConstants[35] = 0.0f;
    // shadowMapLoOffsets (144), shadowMapHiOffsets (160), shadowMapDimensions (176): the 4 taps at +-1 texel.
    const float texture_width = (float)pShadowDepthTexture->description.width;
    const float texture_height = (float)pShadowDepthTexture->description.height;
    const float texel_x = 1.0f / texture_width;
    const float texel_y = 1.0f / texture_height;
    const float offsets[8] = { -texel_x, -texel_y, texel_x, -texel_y, -texel_x, texel_y, texel_x, texel_y };
    for (int i = 0; i < 8; i++)
    {
        pConstants[36 + i] = offsets[i];
    }
    pConstants[44] = texture_width;
    pConstants[45] = texture_height;
    pConstants[46] = texel_x;
    pConstants[47] = texel_y;
    // viewport (192)
    const float viewport_width = (float)viewport.width;
    const float viewport_height = (float)viewport.height;
    pConstants[48] = viewport_width;
    pConstants[49] = viewport_height;
    pConstants[50] = 1.0f / viewport_width;
    pConstants[51] = 1.0f / viewport_height;
    if (pRenderParams1 != nullptr)
    {
        // _dummy0, _dummy1 (208, 224), shadowPercentParams1 (240), shadowMapMatrix1 (256).
        for (int i = 52; i < 60; i++)
        {
            pConstants[i] = 0.0f;
        }
        pConstants[60] = pRenderParams1->shadow_percent;
        pConstants[61] = pRenderParams1->shadow_percent_bias;
        pConstants[62] = 0.0f;
        pConstants[63] = 0.0f;
        write_shadow_map_matrix(pConstants + 64, camera, settings.renderOffset, *pRenderParams1);
    }
    pCommandWriter->endDynamicFragmentConstants();
}

// The projector pass setup shared by the render functions: the CLB target with the full viewport over the
// settings' z range and the projector rasterizer state. Returns the viewport.
static keen::ViewportSettings begin_projector_pass(keen::GraphicsCommandWriter* pCommandWriter, const keen::RenderTarget* pClbTarget, const ClbProjectorRenderSettings& settings)
{
    pCommandWriter->writeBeginRenderPassCommand(pClbTarget, keen::RenderTargetPartFlagMask_None, nullptr);
    keen::ViewportSettings viewport;
    viewport.x = 0;
    viewport.y = 0;
    viewport.width = keen::graphics::getRenderTargetWidth(pClbTarget);
    viewport.height = keen::graphics::getRenderTargetHeight(pClbTarget);
    viewport.minDepth = settings.minZ;
    viewport.maxDepth = settings.maxZ;
    pCommandWriter->writeSetViewportSettingsCommand(&viewport);
    pCommandWriter->writeSetRasterizerStateCommand(g_clbProjectorContext.pRasterizerState);
    return viewport;
}

// The stencil and depth state of a shadow range: the near and full ranges read the scene depth only (the quad is drawn
// at the range's far distance, depth greater); the others first mark their pixels (setupStencilForDepthLayer) and
// then read only those.
static void set_range_depth_stencil(keen::GraphicsCommandWriter* pCommandWriter, rl_renderer& renderer, rl_camera& camera, const ClbProjectorRenderSettings& settings, rl_shadow_map_range range)
{
    if ((int)range < 1 || range == SMR_FULL)
    {
        pCommandWriter->writeSetDepthStencilStateCommand(g_clbProjectorContext.pDepthStencilStateDefault, 0);
        return;
    }
    const unsigned char stencil_ref = setupStencilForDepthLayer(renderer, camera, range, settings.shadowRangeFadeParams);
    pCommandWriter->writeSetDepthStencilStateCommand(g_clbProjectorContext.pDepthStencilStateRead, stencil_ref);
}

// The PCF variant of a shadow pass: its shadow LOD - 1, clamped to 0..2.
static unsigned int pass_lod(rl_shadow_map_pass pass)
{
    int lod = Shadow_pass_parameters[pass].m_shadow_lod - 1;
    if (lod < 0)
    {
        lod = 0;
    }
    else if (2 < lod)
    {
        lod = 2;
    }
    return (unsigned int)lod;
}

// 0x004E18B0 (:355)
// Projects a shadow map into the CLB target: the scene depth (point sampled) gives each pixel's view position, the
// shadow map (comparison sampled, the pass's PCF variant) its shadow, subtracted from the green channel (sub alpha
// blend) within the range's stencil layer and fades.
void rl_clb_projector_system_render_DepthTexture(rl_renderer& renderer, const ClbProjectorRenderSettings& settings, const keen::RenderTarget* pClbTarget, rl_camera& camera, const keen::TextureData* pSceneDepthTexture, const keen::TextureData* pShadowDepthTexture, const rl_shadow_render_parameters& render_params, rl_shadow_map_pass pass, rl_shadow_map_range range)
{
    if (!Is_initialized)
    {
        return;
    }
    keen::GraphicsCommandWriter* pCommandWriter = renderer.getCommandWriter();
    const keen::ViewportSettings viewport = begin_projector_pass(pCommandWriter, pClbTarget, settings);
    set_range_depth_stencil(pCommandWriter, renderer, camera, settings, range);
    pCommandWriter->writeSetFragmentShaderSamplerStateCommand(0, g_clbProjectorContext.pSamplerStatePoint);
    pCommandWriter->writeSetFragmentShaderTextureCommand(0, pSceneDepthTexture);
    pCommandWriter->writeSetFragmentShaderSamplerStateCommand(1, g_clbProjectorContext.pSamplerStateComparison);
    pCommandWriter->writeSetFragmentShaderTextureCommand(1, pShadowDepthTexture);
    const unsigned int lod = pass_lod(pass);
    pCommandWriter->writeSetBlendStateCommand(g_clbProjectorContext.pBlendStateSubAlpha);
    rl_clb_projector_system_setFragmentConstants(pCommandWriter, viewport, settings, camera, pShadowDepthTexture, render_params, nullptr);
    pCommandWriter->writeSetShaderPipelineCommand(g_clbProjectorContext.projectorVariants.getPipeline(lod, 0), g_clbProjectorContext.pProjectorInputBindingImmediate[lod], true);
    drawScreenQuadWithDepth(pCommandWriter, projected_depth(camera.get_projection_transform(), getFarRange(settings.shadowRangeFadeParams)));
    pCommandWriter->writeEndRenderPassCommand(keen::RenderTargetPartFlagMask_None);
}

// 0x004E1EB0 (:410)
// Projects a texture's alpha (the cloud shadows) into the CLB target over the whole screen (no stencil layer), both
// constant matrices from render_params.
void rl_clb_projector_system_render_ProjectionTexture(rl_renderer& renderer, const ClbProjectorRenderSettings& settings, const keen::RenderTarget* pClbTarget, rl_camera& camera, const keen::TextureData* pSceneDepthTexture, const keen::TextureData* pProjectionTexture, const rl_shadow_render_parameters& render_params)
{
    if (!Is_initialized)
    {
        return;
    }
    keen::GraphicsCommandWriter* pCommandWriter = renderer.getCommandWriter();
    const keen::ViewportSettings viewport = begin_projector_pass(pCommandWriter, pClbTarget, settings);
    // Magic number: variant 3 with the projection map (the projection-only shader).
    pCommandWriter->writeSetShaderPipelineCommand(g_clbProjectorContext.projectorVariants.getPipeline(3, 1), g_clbProjectorContext.pProjectorInputBindingImmediate[3], true);
    pCommandWriter->writeSetFragmentShaderSamplerStateCommand(0, g_clbProjectorContext.pSamplerStatePoint);
    pCommandWriter->writeSetFragmentShaderTextureCommand(0, pSceneDepthTexture);
    pCommandWriter->writeSetFragmentShaderSamplerStateCommand(2, g_clbProjectorContext.pSamplerStateLinear);
    pCommandWriter->writeSetFragmentShaderTextureCommand(2, pProjectionTexture);
    pCommandWriter->writeSetBlendStateCommand(g_clbProjectorContext.pBlendStateSubAlpha);
    rl_clb_projector_system_setFragmentConstants(pCommandWriter, viewport, settings, camera, pProjectionTexture, render_params, &render_params);
    pCommandWriter->writeSetDepthStencilStateCommand(g_clbProjectorContext.pDepthStencilStateDefault, 0);
    drawScreenQuadWithDepth(pCommandWriter, 1.0f);
    pCommandWriter->writeEndRenderPassCommand(keen::RenderTargetPartFlagMask_None);
}

// 0x004E23A0 (:448)
// render_DepthTexture with an optional projection texture (the cloud shadows) applied by the same shader variant.
void rl_clb_projector_system_render_DepthAndProjectionTexture(rl_renderer& renderer, const ClbProjectorRenderSettings& settings, const keen::RenderTarget* pClbTarget, rl_camera& camera, const keen::TextureData* pSceneDepthTexture, const keen::TextureData* pShadowDepthTexture, const rl_shadow_render_parameters& depthTextureRenderParams, const keen::TextureData* pProjectionTexture, const rl_shadow_render_parameters* pProjectionTextureRenderParams, rl_shadow_map_pass pass, rl_shadow_map_range range)
{
    if (!Is_initialized)
    {
        return;
    }
    keen::GraphicsCommandWriter* pCommandWriter = renderer.getCommandWriter();
    const keen::ViewportSettings viewport = begin_projector_pass(pCommandWriter, pClbTarget, settings);
    set_range_depth_stencil(pCommandWriter, renderer, camera, settings, range);
    pCommandWriter->writeSetFragmentShaderSamplerStateCommand(0, g_clbProjectorContext.pSamplerStatePoint);
    pCommandWriter->writeSetFragmentShaderTextureCommand(0, pSceneDepthTexture);
    pCommandWriter->writeSetFragmentShaderSamplerStateCommand(1, g_clbProjectorContext.pSamplerStateComparison);
    pCommandWriter->writeSetFragmentShaderTextureCommand(1, pShadowDepthTexture);
    pCommandWriter->writeSetFragmentShaderSamplerStateCommand(2, g_clbProjectorContext.pSamplerStateLinear);
    pCommandWriter->writeSetFragmentShaderTextureCommand(2, pProjectionTexture);
    const unsigned int lod = pass_lod(pass);
    pCommandWriter->writeSetBlendStateCommand(g_clbProjectorContext.pBlendStateSubAlpha);
    rl_clb_projector_system_setFragmentConstants(pCommandWriter, viewport, settings, camera, pShadowDepthTexture, depthTextureRenderParams, pProjectionTextureRenderParams);
    pCommandWriter->writeSetShaderPipelineCommand(g_clbProjectorContext.projectorVariants.getPipeline(lod, pProjectionTextureRenderParams != nullptr ? 1 : 0), g_clbProjectorContext.pProjectorInputBindingImmediate[lod], true);
    drawScreenQuadWithDepth(pCommandWriter, projected_depth(camera.get_projection_transform(), getFarRange(settings.shadowRangeFadeParams)));
    pCommandWriter->writeEndRenderPassCommand(keen::RenderTargetPartFlagMask_None);
}
