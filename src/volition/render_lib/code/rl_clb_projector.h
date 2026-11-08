// Original: volition/render_lib/code/rl_clb_projector.h
// Implementation: volition/render_lib/code/rl_clb_projector.cpp
//
// The CLB projector (projected light/texture volumes; ClbProjectorRenderSettings). Placement inferred.
#pragma once

#include "volition/render_lib/code/rl_shadow_system.h"
#include "volition/vlib/code/math/vector3.h"

class matrix43;
class rl_camera;
class rl_renderer;

namespace keen
{
    class GraphicsCommandWriter;
    struct RenderTarget;
    struct TextureData;
    struct ViewportSettings;
}

// size 0x24
struct ClbProjectorRenderSettings
{
    ClbProjectorRenderSettings(); // 0x004C79B0

    vector renderOffset; // 0x00
    float minZ; // 0x0C
    float maxZ; // 0x10
    float shadowRangeFadeParams[4]; // 0x14
};

// 0x004C79B0
// Uninitialized.
inline ClbProjectorRenderSettings::ClbProjectorRenderSettings()
{
}

void unused_dcf_Rl_clb_lod(); // 0x004BADE0 (:54)
void rl_clb_projector_system_init(); // 0x004DFD30 (:83)
void rl_clb_projector_system_shutdown(); // 0x004BAE50 (:181)
void drawScreenQuadWithDepth(keen::GraphicsCommandWriter* pCommandWriter, float depth); // 0x004BAE60 (:185)
void drawScreenQuadWithShadowRange(keen::GraphicsCommandWriter* pCommandWriter, rl_camera& camera, float range); // 0x004BB150 (:218)
void drawPlane(keen::GraphicsCommandWriter* pCommandWriter, const matrix43& camera_transform, float tanHfov, float tanVfov, float range); // 0x004BB1D0 (:226)
float getNearRange(const float* pShadowRangeParams); // 0x004BB910 (:252)
float getFarRange(const float* pShadowRangeParams); // 0x004BB920 (:257)
unsigned char setupStencilForDepthLayer(rl_renderer& renderer, rl_camera& camera, int layer, const float* pShadowRangeParams); // 0x004E0340 (:263)
void rl_clb_projector_system_setFragmentConstants(keen::GraphicsCommandWriter* pCommandWriter, const keen::ViewportSettings& viewport, const ClbProjectorRenderSettings& settings, rl_camera& camera, const keen::TextureData* pShadowDepthTexture, const rl_shadow_render_parameters& renderParams0, const rl_shadow_render_parameters* pRenderParams1); // 0x004E05D0 (:294)
void rl_clb_projector_system_render_DepthTexture(rl_renderer& renderer, const ClbProjectorRenderSettings& settings, const keen::RenderTarget* pClbTarget, rl_camera& camera, const keen::TextureData* pSceneDepthTexture, const keen::TextureData* pShadowDepthTexture, const rl_shadow_render_parameters& render_params, rl_shadow_map_pass pass, rl_shadow_map_range range); // 0x004E18B0 (:355)
void rl_clb_projector_system_render_ProjectionTexture(rl_renderer& renderer, const ClbProjectorRenderSettings& settings, const keen::RenderTarget* pClbTarget, rl_camera& camera, const keen::TextureData* pSceneDepthTexture, const keen::TextureData* pProjectionTexture, const rl_shadow_render_parameters& render_params); // 0x004E1EB0 (:410)
void rl_clb_projector_system_render_DepthAndProjectionTexture(rl_renderer& renderer, const ClbProjectorRenderSettings& settings, const keen::RenderTarget* pClbTarget, rl_camera& camera, const keen::TextureData* pSceneDepthTexture, const keen::TextureData* pShadowDepthTexture, const rl_shadow_render_parameters& depthTextureRenderParams, const keen::TextureData* pProjectionTexture, const rl_shadow_render_parameters* pProjectionTextureRenderParams, rl_shadow_map_pass pass, rl_shadow_map_range range); // 0x004E23A0 (:448)
