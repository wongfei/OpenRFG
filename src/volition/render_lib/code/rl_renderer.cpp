// Original: volition/render_lib/code/rl_renderer.cpp
//
// rl_renderer: a render context on a keen GraphicsCommandWriter. It keeps the render state in an rl_state_manager,
// mirrors it into the shader constant buffers (common, instance, decal, material; flushed by flush_states when dirty)
// and maps the primitive macro states to rl_render_lib's precreated device states.

#include "volition/render_lib/code/rl_renderer.h"

#include "volition/render_lib/code/rl_dyn_texture.h"
#include "volition/render_lib/code/rl_hdr_compositor.h"
#include "volition/render_lib/code/rl_light.h"
#include "volition/render_lib/code/rl_material_defines.h"
#include "volition/render_lib/code/rl_material_library.h"
#include "volition/render_lib/code/rl_mesh.h"
#include "volition/render_lib/code/rl_mesh_data.h"
#include "volition/render_lib/code/rl_mesh_recompile.h"
#include "volition/render_lib/code/rl_outline_compositor.h"
#include "volition/render_lib/code/rl_particle_renderer.h"
#include "volition/render_lib/code/rl_render_lib.h"
#include "volition/render_lib/code/rl_render_texture.h"
#include "volition/render_lib/code/rl_renderer_core.h"
#include "volition/render_lib/code/rl_scene.h"
#include "volition/render_lib/code/rl_scene_renderer.h"
#include "volition/render_lib/code/rl_shader.h"
#include "volition/render_lib/code/rl_shader_library.h"
#include "volition/render_lib/code/rl_texture_defines.h"
#include "volition/render_lib/code/shader/lightingshaderdata.hpp"
#include "volition/vlib/code/bmpman/bmpman.h"
#include "volition/vlib/code/bmpman/bmpman_defs.h"
#include "library/modules/runtime/graphics/sources/d3d11/graphicscommandbuffer_d3d11.hpp"
#include "library/modules/runtime/graphics/sources/d3d11/rendergeometry_d3d11.hpp"
#include "library/modules/runtime/graphics/sources/d3d11/rendertarget_d3d11.hpp"
#include "library/modules/runtime/graphics/sources/d3d11/texturedata_d3d11.hpp"
#include "keen/graphics/graphicscommandwriter.hpp"
#include "keen/graphics/rendergeometry.hpp"
#include "keen/graphics/texturedata.hpp"
#include "keen/math/matrix43.hpp"

#include <string.h>

const unsigned int rl_renderer::kMaxTmus = 16; // 0x012B80A4
const unsigned int rl_renderer::DYNAMIC_IB_SIZE = 0x200000; // 0x012B80A8
const unsigned int rl_renderer::DYNAMIC_VB_SIZE = 0x200000; // 0x012B80AC
const unsigned int rl_renderer::DYNAMIC_CLONE_SIZE = 0x200000; // 0x012B80B0
const unsigned int rl_renderer::DYNAMIC_MESH_INSTB_SIZE = 0x180000; // 0x012B80B4
const unsigned int rl_renderer::DYNAMIC_DECAL_INSTB_SIZE = 0; // 0x012B80B8
const unsigned int rl_renderer::DYNAMIC_BUFFERS_SIZE = 0x1B00000; // 0x012B80BC
const unsigned int rl_renderer::kSliClearEnabled = 0x2; // 0x012B80C0
const unsigned int rl_renderer::kLodChanged = 0x4; // 0x012B80C4
const unsigned int rl_renderer::kAlphaTest = 0x8; // 0x012B80C8

// The shading tweaks (debug console variables in development builds), written into the common fragment constants.
static bool g_substanceDiffuseScaleEnabled = true; // 0x014FA303
static float g_substanceSpecAlphaScale = 7.0f; // 0x014FA3A4
static float g_substanceSpecPowerScale = 16.0f; // 0x014FA3A8
static float g_substanceFresnelAlphaScale = 1.0f; // 0x014FA3AC
static float g_substanceFresnelPowerScale = 0.6f; // 0x014FA3B0
static float g_glassFresnelBias = 0.3f; // 0x014FA3B4
static float g_glassFresnelPower = 1.4f; // 0x014FA3B8
static bool g_glassDirtEnabled = true; // 0x014FA3BC
static bool g_glassReflectionEnabled = true; // 0x014FA3BD
static bool g_indirectLightEnable = true; // 0x014FA3BE
static bool g_ambientSpecularEnable = true; // 0x014FA3BF
static float g_indirectLightScale = 0.019f; // 0x014FA3C0
static float g_indirectLightAmount = 0.5f; // 0x014FA3C4
static float g_mainLightColorScale = 0.8f; // 0x014FA3C8
static float g_ambientColorScale = 0.65f; // 0x014FA3CC
static float g_backAmbientColorScale = 0.29f; // 0x014FA3D0
static float g_ambientSpecularScale = 0.57f; // 0x014FA3D4
static bool g_enableHemisphericAmbient = true; // 0x014FA3D8

// The alpha test reference of materials with alpha test (a header constant in the original, one copy per TU).
static const unsigned char Rl_alpha_test_ref = 128; // 0x014FA088

// Constant buffer slots.
static const unsigned int Common_constants_slot = 0;
static const unsigned int Lighting_constants_slot = 1;
static const unsigned int Material_constants_slot = 2;
static const unsigned int Skinning_constants_slot = 4;
static const unsigned int Decal_constants_slot = 5;
static const unsigned int Instance_constants_slot = 6;

// 0x004C4D60 (:65)
void get_topology(rl_dev_primitive_topology prim_type, unsigned int num_prims, keen::PrimitiveType& topology, unsigned int& num_verts)
{
    switch (prim_type)
    {
    case RL_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP:
        num_verts = num_prims + 2;
        topology = keen::PrimitiveType_TriangleStrip;
        return;
    case RL_PRIMITIVE_TOPOLOGY_LINELIST:
        num_verts = num_prims * 2;
        topology = keen::PrimitiveType_LineList;
        return;
    case RL_PRIMITIVE_TOPOLOGY_TRIANGLELIST:
        num_verts = num_prims * 3;
        topology = keen::PrimitiveType_TriangleList;
        return;
    case RL_PRIMITIVE_TOPOLOGY_POINTLIST:
        num_verts = num_prims;
        topology = keen::PrimitiveType_Invalid;
        return;
    default:
        VLIB_CRITICAL_FAILURE();
        return;
    }
}

// 0x004F1990 (:147)
rl_renderer::rl_renderer()
{
    m_state_p = nullptr;
    m_flags = 0;
    m_last_dev_status = RL_ACQUIRED;
    m_override_shader = 0xFFFFFFFF;
}

// 0x004C4E20 (:155)
rl_renderer::~rl_renderer()
{
}

// 0x0052D030 (:91)
// Creates the state manager, the instancing buffer and the dynamic constant buffers.
void rl_renderer::initialize(const rl_init_params& params)
{
    m_pRenderLib = rl_render_lib::get_instance();
    m_state_p = new rl_state_manager();
    m_state_p->set_screen_info(*m_pRenderLib->get_screen_info());
    // Magic number: 1.5 MB of instance data.
    m_instancing_buffer = keen::graphics::createGeometryInstancingBuffer(params.m_pKeenGraphicsSystem, Init_params.m_pBufferAllocator, 0x180000);
    m_pCommonVertexConstants = keen::graphics::createDynamicConstantBuffer(params.m_pKeenGraphicsSystem, 0, sizeof(keen::CommonVertexShaderConstants));
    m_pCommonFragmentConstants = keen::graphics::createDynamicConstantBuffer(params.m_pKeenGraphicsSystem, 0, sizeof(keen::CommonFragmentShaderConstants));
    // Magic number: 64 bones of 3 rows.
    m_pSkinningVertexConstants = keen::graphics::createDynamicConstantBuffer(params.m_pKeenGraphicsSystem, 0, 0xC00);
    m_pDecalVertexConstants = keen::graphics::createDynamicConstantBuffer(params.m_pKeenGraphicsSystem, 0, sizeof(keen::DecalVertexShaderConstants));
    m_pLightingVertexConstants = keen::graphics::createDynamicConstantBuffer(params.m_pKeenGraphicsSystem, 0, sizeof(keen::LightingVertexShaderConstants));
    m_pLightingFragmentConstants = keen::graphics::createDynamicConstantBuffer(params.m_pKeenGraphicsSystem, 0, sizeof(keen::LightingFragmentShaderConstants));
    m_pMaterialVertexConstants = keen::graphics::createDynamicConstantBuffer(params.m_pKeenGraphicsSystem, 0, sizeof(m_materialConstantData));
    m_pMaterialFragmentConstants = keen::graphics::createDynamicConstantBuffer(params.m_pKeenGraphicsSystem, 0, sizeof(m_materialConstantData));
    m_pInstanceVertexConstants = keen::graphics::createDynamicConstantBuffer(params.m_pKeenGraphicsSystem, 0, sizeof(keen::InstanceVertexShaderConstants));
    m_pInstanceFragmentConstants = keen::graphics::createDynamicConstantBuffer(params.m_pKeenGraphicsSystem, 0, sizeof(keen::InstanceFragmentShaderConstants));
    memset(&m_commonVertexData, 0, sizeof(m_commonVertexData));
    memset(&m_commonFragmentData, 0, sizeof(m_commonFragmentData));
    memset(&m_decalVertexData, 0, sizeof(m_decalVertexData));
    memset(m_materialConstantData, 0, sizeof(m_materialConstantData));
    memset(&m_instanceVertexData, 0, sizeof(m_instanceVertexData));
    memset(&m_instanceFragmentData, 0, sizeof(m_instanceFragmentData));
    m_constantDataDirtyMask = 0xFFFFFFFF;
    memset(m_vertexBufferData, 0, sizeof(m_vertexBufferData));
    set_tint_color(rl_color_float(1.0f, 1.0f, 1.0f, 1.0f));
    set_fade_color(rl_color_float(1.0f, 1.0f, 1.0f, 1.0f));
    set_saturation(1.0f);
    m_z_far = 1.0f;
    m_viewPort.m_left = 0;
    m_viewPort.m_top = 0;
    m_pipeline_p = nullptr;
    m_merged_target_stack_index = 0;
    m_z_near = 0.0f;
    m_viewPort.m_right = 0;
    m_viewPort.m_bottom = 0;
    memset(m_merged_targets_p, 0, sizeof(m_merged_targets_p));
}

// 0x0052D3B0 (:159)
// Starts recording: full screen viewport, the default macro state (color writes on), the back buffer and the shading
// tweaks.
void rl_renderer::begin(keen::GraphicsCommandWriter* pCommandWriter)
{
    m_pCommandWriter = pCommandWriter;
    m_state_p->set_screen_info(*m_pRenderLib->get_screen_info());
    const rl_screen_info* screen_info_p = rl_render_lib::get_instance()->get_screen_info();
    set_viewport(rl_rect(0, 0, screen_info_p->get_screen_width(), screen_info_p->get_screen_height()));
    rl_primitive_state default_state;
    default_state.set_color_write_mode(RLPCW_RGBA);
    set_macro_state(default_state, true);
    begin_pass(keen::graphics::getBackBufferRenderTarget(Init_params.m_pKeenGraphicsSystem));
    m_commonFragmentData.indirectLightParams.x = g_indirectLightEnable ? g_indirectLightScale : 0.0f;
    m_commonFragmentData.indirectLightParams.y = g_indirectLightAmount;
    m_commonFragmentData.indirectLightParams.z = g_ambientSpecularEnable ? g_ambientSpecularScale : 0.0f;
    m_commonFragmentData.indirectLightParams.w = 0.0f;
    m_commonFragmentData.substanceSpecAlphaScale = g_substanceSpecAlphaScale;
    m_commonFragmentData.substanceSpecPowerScale = g_substanceSpecPowerScale;
    m_commonFragmentData.substanceDiffuseScale = g_substanceDiffuseScaleEnabled ? 1.0f : 0.0f;
    m_commonFragmentData.substanceFresnelAlphaScale = g_substanceFresnelAlphaScale;
    m_commonFragmentData.substanceFresnelPowerScale = g_substanceFresnelPowerScale;
    m_commonFragmentData.glassFresnelBias = g_glassFresnelBias;
    m_commonFragmentData.glassFresnelPower = g_glassFresnelPower;
    m_commonFragmentData.glassReflectionEnabled = g_glassReflectionEnabled ? 1.0f : 0.0f;
    m_commonFragmentData.glassDirtEnabled = g_glassDirtEnabled ? 1.0f : 0.0f;
}

// 0x00515D50 (:204)
void rl_renderer::end()
{
    apply_macro_state(m_state_p->get_macro_state(), m_state_p->get_set_sampler_macro_states());
    commit_rasterizer_state();
    end_pass();
    m_pCommandWriter = nullptr;
}

// 0x0055D060 (:212)
// Recreates the size-dependent render targets after a resolution or LOD change (rl_render_lib::check_device_status).
void rl_renderer::updateResourceSizes(keen::GraphicsCommandWriter* pCommandWriter, bool force, bool highQualityOnConsoles)
{
    if ((m_pRenderLib == nullptr || !m_pRenderLib->hasTargetSizeChanged()) && !force)
    {
        return;
    }
    m_pCommandWriter = pCommandWriter;
    release_all_resources();
    rl_scene_renderer* scene_renderer_p = Active_scene_p->m_scene_renderer_p;
    scene_renderer_p->resize(m_pRenderLib->get_screen_info()->get_screen_width(), m_pRenderLib->get_screen_info()->get_screen_height(), highQualityOnConsoles);
    rl_particle_renderer* particle_renderer_p = rl_singleton_phoenix<rl_particle_renderer>::get_instance();
    particle_renderer_p->m_quarter_width = scene_renderer_p->m_main_scene_target_width >> 2;
    particle_renderer_p->m_quarter_height = scene_renderer_p->m_main_scene_target_height >> 2;
    renew_all_resources();
    m_pCommandWriter = nullptr;
}

// 0x004B7A30 (.h:128)
keen::GraphicsCommandWriter* rl_renderer::getCommandWriter()
{
    return m_pCommandWriter;
}

// 0x004C4E30 (:229)
void rl_renderer::flip()
{
    keen::graphics::flipGeometryInstancingBuffer(m_instancing_buffer);
}

// The keen clear mask of rl_clear_flags (bit 0 color, 1 depth, 2 stencil, 3 color when SLI clears are enabled). Not in
// the binary (inlined in both clear overloads).
// Magic numbers: the ClearSettings mask bits are 2 color, 4 depth and 8 stencil (keen::graphics::writeClearCommand),
// not the RenderTargetPartFlagMask values.
static unsigned int get_clear_mask(unsigned int clear_flags, bool sli_clear_enabled)
{
    if (sli_clear_enabled && (clear_flags & 8) != 0)
    {
        clear_flags |= 1;
    }
    unsigned int clear_mask = 0;
    if ((clear_flags & 1) != 0)
    {
        clear_mask = 2;
    }
    if ((clear_flags & 2) != 0)
    {
        clear_mask |= 4;
    }
    if ((clear_flags & 4) != 0)
    {
        clear_mask |= 8;
    }
    return clear_mask;
}

// 0x004C4E40 (:245)
// Nothing is written for an empty mask.
void rl_renderer::clear(const rl_color& clear_color, unsigned int clear_flags, float z_value, unsigned int stencil_value)
{
    const unsigned int clear_mask = get_clear_mask(clear_flags, (m_flags & kSliClearEnabled) != 0);
    if (clear_mask == 0)
    {
        return;
    }
    const uint32 color = (uint32)clear_color.red | (uint32)clear_color.green << 8 | (uint32)clear_color.blue << 16 | (uint32)clear_color.alpha << 24;
    m_pCommandWriter->writeClearCommand(keen::ClearSettings(color, z_value, (uint8)stencil_value, clear_mask));
}

// 0x004C4FD0 (:268)
// Nothing is written for an empty mask.
void rl_renderer::clear(const rl_color_float& clear_color, unsigned int clear_flags, float z_value, unsigned int stencil_value)
{
    const unsigned int clear_mask = get_clear_mask(clear_flags, (m_flags & kSliClearEnabled) != 0);
    if (clear_mask == 0)
    {
        return;
    }
    // The color is quantized to 8 bits per channel.
    const uint32 red = (uint32)(int)(clear_color.red * 255.0f + 0.5f) & 0xFF;
    const uint32 green = (uint32)(int)(clear_color.green * 255.0f + 0.5f) & 0xFF;
    const uint32 blue = (uint32)(int)(clear_color.blue * 255.0f + 0.5f) & 0xFF;
    const uint32 alpha = (uint32)(int)(clear_color.alpha * 255.0f + 0.5f) & 0xFF;
    m_pCommandWriter->writeClearCommand(keen::ClearSettings(red | green << 8 | blue << 16 | alpha << 24, z_value, (uint8)stencil_value, clear_mask));
}

// 0x004C51C0 (:291)
void rl_renderer::set_viewport(const rl_rect& vp)
{
    m_viewPort = vp;
    keen::ViewportSettings settings;
    settings.x = (uint32)vp.m_left;
    settings.y = (uint32)vp.m_top;
    settings.width = (uint32)(vp.m_right - vp.m_left);
    settings.height = (uint32)(vp.m_bottom - vp.m_top);
    settings.minDepth = m_z_near;
    settings.maxDepth = m_z_far;
    m_pCommandWriter->writeSetViewportSettingsCommand(&settings);
}

// 0x004C52A0 (:305)
void rl_renderer::set_viewport_z_range(const float z_near, const float z_far)
{
    m_z_near = z_near;
    m_z_far = z_far;
    set_viewport(m_viewPort);
}

// 0x004C52D0 (:312)
void rl_renderer::get_viewport(rl_rect& vp) const
{
    vp = m_viewPort;
}

// 0x004C52F0 (:317)
void rl_renderer::get_viewport_z_range(float& z_near, float& z_far) const
{
    z_near = m_z_near;
    z_far = m_z_far;
}

// 0x00515D90 (:323)
// Negative left/top edges are clamped to 0 (the width is not adjusted).
void rl_renderer::set_scissor_region(const rl_rect& scissor_rect)
{
    m_state_p->set_scissor_region(scissor_rect);
    keen::ScissorRectangle rectangle;
    rectangle.x = scissor_rect.m_left < 1 ? 0 : (uint32)scissor_rect.m_left;
    rectangle.y = scissor_rect.m_top < 1 ? 0 : (uint32)scissor_rect.m_top;
    rectangle.width = (uint32)(scissor_rect.m_right - scissor_rect.m_left);
    rectangle.height = (uint32)(scissor_rect.m_bottom - scissor_rect.m_top);
    m_pCommandWriter->writeSetScissorRectangleCommand(&rectangle);
}

// 0x004F1A00 (:337)
void rl_renderer::get_scissor_region(rl_rect& rect) const
{
    rect = m_state_p->get_scissor_region();
}

// 0x00515E40 (:342)
void rl_renderer::set_primitive_color(const rl_color_float& primitive_color)
{
    m_state_p->set_primitive_color(primitive_color);
}

// 0x004F1A30 (:347)
void rl_renderer::get_primitive_color(rl_color_float& color) const
{
    color = m_state_p->get_primitive_color();
}

// 0x00543B20 (:352)
void rl_renderer::set_primitive_texture(int tex)
{
    m_state_p->set_primitive_texture(tex);
}

// 0x004F1A50 (:357)
int rl_renderer::get_primitive_texture() const
{
    return m_state_p->get_primitive_texture();
}

// 0x0052D560 (:362)
void rl_renderer::set_primitive_material(unsigned int mat)
{
    m_state_p->set_primitive_material(mat);
}

// 0x004F1A60 (:367)
unsigned int rl_renderer::get_primitive_material() const
{
    return m_state_p->get_primitive_material();
}

// 0x00515E70 (:372)
void rl_renderer::set_primitive_bink_textures(int y_texture, int cr_texture, int cb_texture)
{
    m_state_p->set_primitive_bink_textures(y_texture, cr_texture, cb_texture);
}

// 0x004F1A70 (:377)
void rl_renderer::get_primitive_bink_textures(int& y_texture, int& cr_texture, int& cb_texture) const
{
    m_state_p->get_primitive_bink_textures(y_texture, cr_texture, cb_texture);
}

// 0x00515EC0 (:382)
// Stores the macro state; its cull, scissor, msaa and depth bias parts are copied to the state manager here, the rest
// is applied by apply_macro_state.
void rl_renderer::set_macro_state(const rl_primitive_state& state, const bool set_sampler_states)
{
    m_state_p->set_macro_state(state);
    m_state_p->set_set_sampler_macro_states(set_sampler_states);
    switch (state.get_cull_mode())
    {
    case RLPCM_NONE:
        m_state_p->set_cull_mode(RL_CULL_NONE);
        break;
    case RLPCM_CULL:
        m_state_p->set_cull_mode(RL_CULL_SHADER_DRIVEN);
        break;
    case RLPCM_FRONT:
        m_state_p->set_cull_mode(RL_CULL_CW);
        break;
    case RLPCM_BACK:
        m_state_p->set_cull_mode(RL_CULL_CCW);
        break;
    default:
        VLIB_CRITICAL_FAILURE();
        break;
    }
    m_state_p->set_scissor_test_enabled(state.get_scissor_enabled());
    m_state_p->set_msaa_enabled(state.get_msaa_enabled());
    m_state_p->set_zbias_mode(state.get_zbias_mode());
}

// 0x004F1AA0 (:412)
const rl_primitive_state& rl_renderer::get_macro_state() const
{
    return m_state_p->get_macro_state();
}

// 0x00515FC0 (:417)
// Selects the technique's pipeline and binds the shader's samplers.
void rl_renderer::set_current_shader(rl_shader* const shader, const int technique)
{
    m_state_p->set_shader(shader, technique);
    if (shader == nullptr)
    {
        m_input_layouts_p = nullptr;
        m_pipeline_p = nullptr;
        return;
    }
    // rl_shader::get_shader_pipeline inlined: the technique is clamped to the last one.
    const int last_technique = shader->m_shader_data_p->num_techniques - 1;
    rl_shader_technique_runtime& technique_data = shader->m_shader_data_p->shader_techniques[technique <= last_technique ? technique : last_technique];
    m_input_layouts_p = technique_data.bindings_p;
    m_pipeline_p = &technique_data.pipeline;
    const rl_shader_header_runtime* shader_data_p = shader->m_shader_data_p;
    for (int i = 0; i < shader_data_p->num_tex_fixups; i++)
    {
        const rl_shader_tex_fixup_runtime& fixup = shader_data_p->tex_fixups[i];
        m_pCommandWriter->writeSetFragmentShaderSamplerStateCommand(fixup.constLocData.const_loc, fixup.colorData.internal_sampler_state_p);
    }
}

// 0x004F1AB0 (:439)
void rl_renderer::get_current_shader(rl_shader*& shader, int& technique) const
{
    m_state_p->get_shader(shader, technique);
}

// 0x005160A0 (:444)
void rl_renderer::clear_current_shader()
{
    m_state_p->set_shader(nullptr, -1);
    m_input_layouts_p = nullptr;
    m_pipeline_p = nullptr;
}

// 0x004F1AD0 (:449)
void rl_renderer::set_shader_type(const rl_shader_type type)
{
    m_state_p->set_shader_type(type);
}

// 0x004F1AE0 (:454)
const rl_shader_type rl_renderer::get_shader_type() const
{
    return m_state_p->get_shader_type();
}

// 0x004F1AF0 (:459)
void rl_renderer::clear_shader_type()
{
    m_state_p->set_shader_type(shader_type_generic);
}

// 0x004F1B00 (:464)
void rl_renderer::set_shader_configuration(const int config)
{
    m_state_p->set_shader_configuration(config);
}

// 0x004F1B10 (:469)
const int rl_renderer::get_shader_configuration() const
{
    return m_state_p->get_shader_configuration();
}

// 0x004F1B20 (:474)
void rl_renderer::clear_shader_configuration()
{
    m_state_p->set_shader_configuration(0);
}

// 0x004F1B30 (:479)
void rl_renderer::set_vertex_format(rl_dev_vertex_format id)
{
    m_state_p->set_vertex_format(id);
    m_vertex_format = id;
}

// 0x004F1B50 (:485)
const rl_dev_vertex_format rl_renderer::get_vertex_format() const
{
    return m_state_p->get_vertex_format();
}

// 0x004F1B60 (:490)
// Applies the depth/stencil, blend and (optionally) sampler states of the macro state. Unless a material set its own
// alpha test (kAlphaTest), the stencil-write modes test alpha against 1/255 and RLPAM_ALPHA_TEST against the constant
// alpha.
bool rl_renderer::apply_macro_state(const rl_primitive_state& state, const bool set_sampler_states)
{
    const rl_prim_alpha_mode alpha_mode = state.get_alpha_mode();
    const rl_prim_stencil_mode stencil_mode = state.get_stencil_mode();
    if ((m_flags & kAlphaTest) == 0)
    {
        float alpha_ref = 0.0f;
        bool alpha_test = false;
        if (stencil_mode == RLPSM_WRITE || stencil_mode == RLPSM_WRITE_NOCOLOR || stencil_mode == RLPSM_CLEAR_NOCOLOR)
        {
            // Magic number: 1/255, the smallest non-zero alpha.
            alpha_ref = 1.0f / 255.0f;
            alpha_test = true;
        }
        else if (alpha_mode == RLPAM_ALPHA_TEST)
        {
            alpha_ref = (float)state.get_const_alpha() / 255.0f;
            alpha_test = true;
        }
        set_alpha_test(alpha_test, alpha_ref);
    }
    // Magic numbers: the stencil reference values (bit 0 for stencil writes and tests, bit 4 for the layered material
    // mask, bits 4 and 5 for its base).
    uint32 stencil_ref = 0;
    switch (stencil_mode)
    {
    case RLPSM_WRITE:
    case RLPSM_TEST:
    case RLPSM_WRITE_NOCOLOR:
        stencil_ref = 0x01;
        break;
    case RLPSM_LAYERED_MATERIAL_MASKING:
    case RLPSM_LAYERED_MATERIAL_TEST_MASKING_FINAL:
    case RLPSM_LAYERED_MATERIAL_TEST_MASKING:
        stencil_ref = 0x10;
        break;
    case RLPSM_LAYERED_MATERIAL_BASE:
    case RLPSM_LAYERED_MATERIAL_TEST_BASE_FINAL:
    case RLPSM_LAYERED_MATERIAL_TEST_BASE:
        stencil_ref = 0x30;
        break;
    default:
        break;
    }
    const bool color_writes = stencil_mode != RLPSM_WRITE_NOCOLOR && stencil_mode != RLPSM_CLEAR_NOCOLOR;
    m_pCommandWriter->writeSetDepthStencilStateCommand(m_pRenderLib->m_prim_depth_stencil_states[stencil_mode][state.get_zbuf_mode()].m_internal_state_p, stencil_ref);
    rl_prim_color_write_mode color_write_mode = state.get_color_write_mode();
    if (color_write_mode == RLPCW_UNDEFINED)
    {
        color_write_mode = color_writes ? RLPCW_RGBA : RLPCW_NONE;
    }
    m_pCommandWriter->writeSetBlendStateCommand(m_pRenderLib->m_prim_blend_states[alpha_mode][color_write_mode].m_internal_state_p);
    if (set_sampler_states)
    {
        // The first three samplers follow the macro state's clamp mode.
        const keen::SamplerState* sampler_p = m_pRenderLib->m_prim_sampler_states[state.get_clamp_mode()].m_internal_state_p;
        m_pCommandWriter->writeSetFragmentShaderSamplerStateCommand(0, sampler_p);
        m_pCommandWriter->writeSetFragmentShaderSamplerStateCommand(1, sampler_p);
        m_pCommandWriter->writeSetFragmentShaderSamplerStateCommand(2, sampler_p);
    }
    return true;
}

// 0x004F1E10 (:563)
void rl_renderer::set_cull_mode(const rl_cull_mode mode)
{
    m_state_p->set_cull_mode(mode);
}

// 0x004F1E20 (:568)
void rl_renderer::set_shader_cull_mode(const rl_cull_mode mode)
{
    m_state_p->set_shader_cull_mode(mode);
}

// 0x004F1E30 (:573)
const rl_cull_mode rl_renderer::get_cull_mode() const
{
    return m_state_p->get_cull_mode();
}

// 0x004F1E40 (:578)
const rl_cull_mode rl_renderer::get_shader_cull_mode() const
{
    return m_state_p->get_shader_cull_mode();
}

// 0x004F1E50 (:583)
// The keen texture of a texture handle: a bitmap handle, or a dynamic texture handle (bit 24 set).
rl_dev_texture get_dev_texture(const int texture)
{
    if (texture != -1)
    {
        // Magic number: bit 24 marks dynamic texture handles (rl_dyn_texture_base).
        if ((texture & 0x1000000) == 0)
        {
            bitmap_entry* entry_p = bm_get_entry(texture, true);
            if (entry_p != nullptr && entry_p->current_peg_entry != nullptr)
            {
                return rl_dev_texture((keen::TextureData*)entry_p->current_peg_entry->cache[0]);
            }
        }
        else
        {
            rl_dyn_texture_base* dyn_texture_p = rl_dyn_texture_base::find_by_handle(texture);
            if (dyn_texture_p != nullptr)
            {
                return dyn_texture_p->get_texture_data();
            }
        }
    }
    return rl_dev_texture(nullptr);
}

// 0x00543B60 (:610)
// Draws an immediate primitive (the primitive renderer's playback): the material, the textures, and 2D positions
// converted from pixels to clip space.
// Notes (decompiled behaviour, kept as is): the constants are not set, and the 2D conversion assumes a strip (num_prims
// + 2 vertices of 0x1C bytes).
void rl_renderer::draw_primitive(rl_primitive_render_type prim_type, int num_prims, rl_dev_vertex_format vertex_format, unsigned int vertex_stride, const void* vertex_data_p, unsigned int material, int num_textures, rl_texture_binding* textures, int num_constants, rl_constant_binding* constants)
{
    set_vertex_format(vertex_format);
    m_state_p->set_shader_type(shader_type_standard);
    m_state_p->set_shader_configuration(0);
    if (material != 0xFFFFFFFF)
    {
        rl_material_library_set_render_material(*this, material, 0);
    }
    for (int i = 0; i < num_textures; i++)
    {
        m_pCommandWriter->writeSetFragmentShaderTextureCommand(textures[i].m_tmu, get_dev_texture(textures[i].m_handle).pData);
    }
    if (vertex_format == RLVF_PRIMITIVE_2D)
    {
        const rl_screen_info* screen_info_p = rl_render_lib::get_instance()->get_screen_info();
        const int half_width = screen_info_p->get_screen_width() >> 1;
        const int half_height = screen_info_p->get_screen_height() >> 1;
        float* position_p = (float*)vertex_data_p;
        // Magic number: rl_primitive_2d_vert is 7 floats.
        for (int i = 0; i < num_prims + 2; i++)
        {
            position_p[0] = position_p[0] * (1.0f / (float)half_width) - 1.0f;
            position_p[1] = 1.0f - position_p[1] * (1.0f / (float)half_height);
            position_p += 7;
        }
    }
    rl_dev_primitive_topology topology = RL_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP;
    switch (prim_type)
    {
    case RLPRT_LINELIST_2D:
    case RLPRT_TEXTURED_LINELIST_2D:
    case RLPRT_LINELIST_3D:
    case RLPRT_TEXTURED_LINELIST_3D:
        topology = RL_PRIMITIVE_TOPOLOGY_LINELIST;
        break;
    case RLPRT_TRISTRIP_2D:
    case RLPRT_TEXTURED_TRISTRIP_2D:
    case RLPRT_TRISTRIP_3D:
    case RLPRT_TEXTURED_TRISTRIP_3D:
    case RLPRT_BINKED_TRISTRIP_2D:
    case RLPRT_MATERIAL_TRISTRIP_2D:
    case RLPRT_MATERIAL_TRISTRIP_3D:
        topology = RL_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP;
        break;
    default:
        VLIB_CRITICAL_FAILURE();
        break;
    }
    draw_primitive(topology, num_prims, vertex_data_p, vertex_stride);
}

// 0x0052D5A0 (:681)
// Flushes the state and maps num_prims primitives of immediate vertices; end_dynamic draws them.
void* rl_renderer::begin_dynamic(rl_dev_primitive_topology prim_type, unsigned int num_prims, unsigned int vertex_stride)
{
    flush_states();
    keen::PrimitiveType topology;
    unsigned int num_verts;
    get_topology(prim_type, num_prims, topology, num_verts);
    return m_pCommandWriter->beginImmediateGeometryCommand(topology, num_verts, vertex_stride);
}

// 0x004C5310 (:691)
void rl_renderer::end_dynamic()
{
    m_pCommandWriter->endImmediateGeometryCommand();
}

// 0x004C54F0 (:696)
void* rl_renderer::begin_instancing_batch(keen::GeometryInstancingBatch* batch_p, unsigned int num_instances, unsigned int instance_size)
{
    return m_pCommandWriter->beginInstancingBatch(batch_p, m_instancing_buffer, num_instances, instance_size);
}

// 0x004C5590 (:708)
void rl_renderer::end_instancing_batch()
{
    m_pCommandWriter->endInstancingBatch(m_instancing_buffer);
}

// 0x0052D690 (:713)
void rl_renderer::draw_primitive(rl_dev_primitive_topology prim_type, unsigned int num_prims, const void* vertex_data_p, unsigned int vertex_stride)
{
    flush_states();
    keen::PrimitiveType topology;
    unsigned int num_verts;
    get_topology(prim_type, num_prims, topology, num_verts);
    void* vertices_p = m_pCommandWriter->beginImmediateGeometryCommand(topology, num_verts, vertex_stride);
    if (vertices_p != nullptr)
    {
        memcpy(vertices_p, vertex_data_p, num_verts * vertex_stride);
        m_pCommandWriter->endImmediateGeometryCommand();
    }
}

// 0x0052D960 (:728)
// Selects the material's shader technique (the depth-only shader in depth passes, unless the material needs its pixel
// shader) and binds its constants (material constant slot) and textures.
void rl_renderer::set_material(const rl_shader_technique_type override_technique_type, const int override_technique_index, rl_material_data* mat_data_p)
{
    const bool depth_only = m_state_p->get_depth_only();
    const unsigned int mat_flags = mat_data_p->mat_flags;
    unsigned int shader_handle = mat_data_p->shader_handle;
    // Magic numbers: material flags 0x02 (alpha test) and 0x80 need the pixel shader in depth passes.
    const bool needs_pixel_shader = (mat_flags & 0x82) != 0;
    if (depth_only && !needs_pixel_shader)
    {
        shader_handle = rl_shader_library::get_instance()->get_depth_only_shader();
    }
    // Magic number: material flag 0x10 (primitive materials) keeps the macro state's alpha test.
    if ((mat_flags & 0x10) == 0)
    {
        m_state_p->set_depth_requires_pixel_shader((mat_flags & 0x02) != 0);
        m_flags |= kAlphaTest;
        set_alpha_test((mat_flags & 0x02) != 0, (float)Rl_alpha_test_ref / 255.0f);
    }
    // Magic number: material flag 0x40 is two-sided.
    m_state_p->set_shader_cull_mode((mat_data_p->mat_flags & 0x40) == 0 ? RL_CULL_CCW : RL_CULL_NONE);
    rl_shader_technique_type technique_type = override_technique_type;
    int technique_index = override_technique_index;
    if (override_technique_type == rl_shader_technique_none)
    {
        technique_type = m_state_p->get_is_base_pass() ? rl_shader_technique_base : rl_shader_technique_lighting;
        technique_index = -1;
        if (depth_only)
        {
            technique_type = rl_shader_technique_depth_only;
        }
    }
    set_active_shader(shader_handle, technique_type, technique_index);
    if (depth_only && !needs_pixel_shader)
    {
        return;
    }
    if (mat_data_p->num_constants != 0)
    {
        void* constants_p = m_pCommandWriter->beginDynamicConstantUpdate(m_pMaterialVertexConstants, keen::ShaderMask_Vertex | keen::ShaderMask_Fragment);
        memcpy(constants_p, mat_data_p->constant_block.to_pointer(), mat_data_p->num_constants * sizeof(rl_material_const));
        m_pCommandWriter->endDynamicConstantUpdate(Material_constants_slot, keen::ShaderMask_Vertex | keen::ShaderMask_Fragment);
    }
    for (int i = 0; i < mat_data_p->num_textures; i++)
    {
        const rl_texture_desc& texture = mat_data_p->textures.to_pointer()[i];
        m_pCommandWriter->writeSetFragmentShaderTextureCommand(texture.data.rt.texture_stage, get_dev_texture(texture.texture_handle).pData);
        // Magic number: texture flags 0x7 clamp U, V and W (a clamped variant of the shader's sampler).
        if ((texture.data.rt.texture_flags & 0x7) != 0)
        {
            m_pCommandWriter->writeSetFragmentShaderSamplerStateCommand(texture.data.rt.texture_stage, rl_Clamped_sampler_states[texture.data.rt.sampler_state_idx].m_internal_state_p);
        }
    }
}

// 0x004F1ED0 (:800)
const bool rl_renderer::get_depth_only() const
{
    return m_state_p->get_depth_only();
}

// 0x004F1EE0 (:805)
void rl_renderer::set_depth_only(const bool depth_only)
{
    m_state_p->set_depth_only(depth_only);
}

// 0x004F1F00 (:810)
unsigned int rl_renderer::get_depth_only_shader() const
{
    return rl_shader_library::get_instance()->get_depth_only_shader();
}

// 0x004F1F10 (:815)
const bool rl_renderer::get_depth_requires_pixel_shader() const
{
    return m_state_p->get_depth_requires_pixel_shader();
}

// 0x004F1F20 (:820)
void rl_renderer::set_depth_requires_pixel_shader(const bool depth_required)
{
    m_state_p->set_depth_requires_pixel_shader(depth_required);
}

// 0x004F1F40 (:825)
const bool rl_renderer::get_is_base_pass() const
{
    return m_state_p->get_is_base_pass();
}

// 0x004F1F50 (:830)
void rl_renderer::set_is_base_pass(const bool is_base_pass)
{
    m_state_p->set_is_base_pass(is_base_pass);
}

// 0x005160C0 (:835)
// The instance tint is tint * fade.
void rl_renderer::set_fade_color(const rl_color_float& fade_color)
{
    m_state_p->set_fade_color(fade_color);
    const rl_color_float& tint = m_state_p->get_tint_color();
    m_instanceFragmentData.tint.x = tint.red * fade_color.red;
    m_instanceFragmentData.tint.y = tint.green * fade_color.green;
    m_instanceFragmentData.tint.z = tint.blue * fade_color.blue;
    m_instanceFragmentData.tint.w = tint.alpha * fade_color.alpha;
    m_constantDataDirtyMask |= ConstantDataMask_InstanceFragmentData;
}

// 0x00516170 (:845)
void rl_renderer::set_tint_color(const rl_color_float& tint_color)
{
    m_state_p->set_tint_color(tint_color);
    const rl_color_float& fade = m_state_p->get_fade_color();
    m_instanceFragmentData.tint.x = fade.red * tint_color.red;
    m_instanceFragmentData.tint.y = fade.green * tint_color.green;
    m_instanceFragmentData.tint.z = fade.blue * tint_color.blue;
    m_instanceFragmentData.tint.w = fade.alpha * tint_color.alpha;
    m_constantDataDirtyMask |= ConstantDataMask_InstanceFragmentData;
}

// 0x00516220 (:855)
void rl_renderer::set_saturation(float saturation)
{
    set_saturation(saturation, 0.0f);
}

// 0x00516260 (:863)
void rl_renderer::set_saturation(float saturation0, float saturation1)
{
    m_state_p->set_saturation(saturation0);
    m_commonFragmentData.saturation.x = saturation0;
    m_commonFragmentData.saturation.y = saturation1;
    m_constantDataDirtyMask |= ConstantDataMask_CommonFragmentData;
}

// 0x004F1F70 (:871)
const rl_color_float& rl_renderer::get_fade_color() const
{
    return m_state_p->get_fade_color();
}

// 0x004F1F80 (:876)
const rl_color_float& rl_renderer::get_tint_color() const
{
    return m_state_p->get_tint_color();
}

// 0x004F1F90 (:881)
const float rl_renderer::get_saturation() const
{
    return m_state_p->get_saturation();
}

// 0x005162A0 (:886)
// Notes (decompiled behaviour, kept as is): the shader constant swaps the first two parameters.
void rl_renderer::set_fog_parameters(float height_density, float atmosphere_scale, float density, float density_offset)
{
    m_state_p->set_fog_parameters(height_density, atmosphere_scale, density, density_offset);
    m_commonVertexData.fogDistance.x = atmosphere_scale;
    m_commonVertexData.fogDistance.y = height_density;
    m_commonVertexData.fogDistance.z = density;
    m_commonVertexData.fogDistance.w = density_offset;
    m_constantDataDirtyMask |= ConstantDataMask_CommonVertexData;
}

// 0x004F1FA0 (:895)
void rl_renderer::get_fog_parameters(float& height_density, float& atmosphere_scale, float& density, float& density_offset) const
{
    m_state_p->get_fog_parameters(height_density, atmosphere_scale, density, density_offset);
}

// 0x00516310 (:900)
void rl_renderer::set_fog_color(const rl_color_float& fog_color)
{
    m_state_p->set_fog_color(fog_color);
    m_commonFragmentData.fogColor.x = fog_color.red;
    m_commonFragmentData.fogColor.y = fog_color.green;
    m_commonFragmentData.fogColor.z = fog_color.blue;
    m_commonFragmentData.fogColor.w = fog_color.alpha;
    m_constantDataDirtyMask |= ConstantDataMask_CommonFragmentData;
}

// 0x004F1FE0 (:908)
const rl_color_float& rl_renderer::get_fog_color() const
{
    return m_state_p->get_fog_color();
}

// 0x00516370 (:913)
void rl_renderer::set_eye_position(const vector& eye_pos)
{
    m_state_p->set_eye_position(eye_pos);
    m_commonVertexData.camPos.x = eye_pos.x;
    m_commonVertexData.camPos.y = eye_pos.y;
    m_commonVertexData.camPos.z = eye_pos.z;
    m_commonVertexData.camPos.w = 0.0f;
    m_constantDataDirtyMask |= ConstantDataMask_CommonVertexData;
}

// 0x004C55B0 (:921)
void rl_renderer::set_eye_orientation(const matrix& orientation)
{
    m_commonVertexData.camRot.row0[0] = orientation.rvec.x;
    m_commonVertexData.camRot.row0[1] = orientation.rvec.y;
    m_commonVertexData.camRot.row0[2] = orientation.rvec.z;
    m_commonVertexData.camRot.row0[3] = 0.0f;
    m_commonVertexData.camRot.row1[0] = orientation.uvec.x;
    m_commonVertexData.camRot.row1[1] = orientation.uvec.y;
    m_commonVertexData.camRot.row1[2] = orientation.uvec.z;
    m_commonVertexData.camRot.row1[3] = 0.0f;
    m_commonVertexData.camRot.row2[0] = orientation.fvec.x;
    m_commonVertexData.camRot.row2[1] = orientation.fvec.y;
    m_commonVertexData.camRot.row2[2] = orientation.fvec.z;
    m_commonVertexData.camRot.row2[3] = 0.0f;
    m_constantDataDirtyMask |= ConstantDataMask_CommonVertexData;
}

// 0x004F1FF0 (:941)
const vector& rl_renderer::get_eye_position() const
{
    return m_state_p->get_eye_position();
}

// 0x005163D0 (:946)
void rl_renderer::set_time_value(const vector4& time_val)
{
    m_state_p->set_time_value(time_val);
    m_commonVertexData.time.x = time_val.x;
    m_commonVertexData.time.y = time_val.y;
    m_commonVertexData.time.z = time_val.z;
    m_commonVertexData.time.w = time_val.w;
    m_commonFragmentData.time.x = time_val.x;
    m_commonFragmentData.time.y = time_val.y;
    m_commonFragmentData.time.z = time_val.z;
    m_commonFragmentData.time.w = time_val.w;
    m_constantDataDirtyMask |= ConstantDataMask_CommonVertexData | ConstantDataMask_CommonFragmentData;
}

// 0x004F2000 (:955)
const vector4& rl_renderer::get_time_value() const
{
    return m_state_p->get_time_value();
}

// 0x00516460 (:960)
// The ambient color is scaled by g_ambientColorScale (the constant's alpha is 1). Without hemispheric ambient, the back
// ambient constant follows the ambient color.
// Notes (decompiled behaviour, kept as is): the state's back ambient color is left as is in that case.
void rl_renderer::set_ambient_color(const rl_color_float& ambient_color)
{
    const float scale = g_ambientColorScale;
    const rl_color_float scaled(ambient_color.red * scale, ambient_color.green * scale, ambient_color.blue * scale, ambient_color.alpha * scale);
    m_state_p->set_ambient_color(scaled);
    m_commonFragmentData.ambient.x = scaled.red;
    m_commonFragmentData.ambient.y = scaled.green;
    m_commonFragmentData.ambient.z = scaled.blue;
    m_commonFragmentData.ambient.w = 1.0f;
    m_constantDataDirtyMask |= ConstantDataMask_CommonFragmentData;
    if (!g_enableHemisphericAmbient)
    {
        m_state_p->set_back_ambient_color(m_state_p->get_back_ambient_color());
        m_commonFragmentData.backAmbient.x = scaled.red;
        m_commonFragmentData.backAmbient.y = scaled.green;
        m_commonFragmentData.backAmbient.z = scaled.blue;
        m_commonFragmentData.backAmbient.w = 1.0f;
    }
}

// 0x004F2010 (:979)
const rl_color_float& rl_renderer::get_ambient_color() const
{
    return m_state_p->get_ambient_color();
}

// 0x00516590 (:984)
// Only with hemispheric ambient; scaled by g_backAmbientColorScale.
void rl_renderer::set_back_ambient_color(const rl_color_float& back_ambient_color)
{
    if (!g_enableHemisphericAmbient)
    {
        return;
    }
    const float scale = g_backAmbientColorScale;
    const rl_color_float scaled(back_ambient_color.red * scale, back_ambient_color.green * scale, back_ambient_color.blue * scale, back_ambient_color.alpha * scale);
    m_state_p->set_back_ambient_color(scaled);
    m_commonFragmentData.backAmbient.x = scaled.red;
    m_commonFragmentData.backAmbient.y = scaled.green;
    m_commonFragmentData.backAmbient.z = scaled.blue;
    m_commonFragmentData.backAmbient.w = 1.0f;
    m_constantDataDirtyMask |= ConstantDataMask_CommonFragmentData;
}

// 0x004F2020 (:1000)
const rl_color_float& rl_renderer::get_back_ambient_color() const
{
    return m_state_p->get_back_ambient_color();
}

// 0x004C5630 (:1005)
const float rl_renderer::get_texel_offset() const
{
    return 0.0f;
}

// 0x0054CFC0 (:1010)
// The simulation/render handshake: the per-frame particle pools are reset and the finished mesh recompiles applied.
void rl_renderer::handshake_begin()
{
    rl_singleton_phoenix<rl_particle_renderer>::get_instance();
    rl_particle_renderer::ms_particle_iterative_pool.set_thread_ownership();
    rl_particle_renderer::ms_particle_iterative_pool.clear();
    rl_particle_renderer::ms_particle_parametric_pool.set_thread_ownership();
    rl_particle_renderer::ms_particle_parametric_pool.clear();
    rl_particle_renderer::ms_particle_tail_pool.set_thread_ownership();
    rl_particle_renderer::ms_particle_tail_pool.clear();
    rl_mesh_recompile_finalize();
}

// 0x004F2030 (:1016)
// Copies rows of read_pitch bytes (at most the texture's row pitch) into level 0.
void rl_renderer::update_dynamic_texture(const rl_dev_texture tex, const unsigned char* data_p, unsigned int, unsigned int, unsigned int read_pitch)
{
    keen::MappedTextureInfo mapped;
    unsigned char* dest_p = (unsigned char*)m_pCommandWriter->mapTextureData(&mapped, tex.pData, 0, keen::GraphicsMapType_WriteDiscard, 0);
    if (dest_p == nullptr)
    {
        return;
    }
    const unsigned int row_size = read_pitch < mapped.rowPitch ? read_pitch : mapped.rowPitch;
    for (unsigned int row = tex.pData->description.height; row != 0; row--)
    {
        memcpy(dest_p, data_p, row_size);
        dest_p += mapped.rowPitch;
        data_p += read_pitch;
    }
    m_pCommandWriter->unmapTextureData(tex.pData, 0);
}

// 0x004F20C0 (:1038)
void rl_renderer::set_active_camera(rl_camera* camera_p)
{
    m_state_p->set_active_camera(camera_p);
}

// 0x004F20D0 (:1043)
rl_camera* rl_renderer::get_active_camera() const
{
    return m_state_p->get_active_camera();
}

// 0x00516670 (:1048)
void rl_renderer::set_3d_transforms(const matrix44& world_view_transform, const matrix44& projection_transform)
{
    m_state_p->set_3d_transforms(world_view_transform, projection_transform);
    update_transforms();
}

// 0x004F20E0 (:1054)
void rl_renderer::get_3d_transforms(matrix44& world_view_transform, matrix44& projection_transform)
{
    m_state_p->get_3d_transforms(world_view_transform, projection_transform);
}

// 0x004F2110 (:1059)
void rl_renderer::set_render_offset(const vector& offset)
{
    m_state_p->set_render_offset(offset);
}

// 0x004C5640 (:1064)
void rl_renderer::set_shadow_fade_params(const vector4& params)
{
    m_commonFragmentData.shadowFadeParams.x = params.x;
    m_commonFragmentData.shadowFadeParams.y = params.y;
    m_commonFragmentData.shadowFadeParams.z = params.z;
    m_commonFragmentData.shadowFadeParams.w = params.w;
    m_constantDataDirtyMask |= ConstantDataMask_CommonFragmentData;
}

// 0x004F2140 (:1070)
const vector& rl_renderer::get_render_offset() const
{
    return m_state_p->get_render_offset();
}

// 0x004F2150 (:1075)
// A color target and a separate depth target are merged into a temporary render target, destroyed by end_pass.
void rl_renderer::begin_pass(const rl_dev_render_target& rt, const rl_dev_depth_stencil_target& ds)
{
    m_render_target = rt;
    m_depth_stencil_target = ds;
    const keen::RenderTarget* target_p = rt.pData;
    if (target_p == nullptr)
    {
        target_p = ds.pData;
    }
    else if (ds.pData != nullptr)
    {
        target_p = keen::graphics::createRenderTarget(Init_params.m_pKeenGraphicsSystem, target_p, ds.pData);
        m_merged_targets_p[m_merged_target_stack_index] = target_p;
        m_merged_target_stack_index++;
    }
    begin_pass(target_p);
}

// 0x004C5680 (:1096)
void rl_renderer::begin_pass(const keen::RenderTarget* pRenderTarget)
{
    m_pCommandWriter->writeBeginRenderPassCommand(pRenderTarget, keen::RenderTargetPartFlagMask_None, nullptr);
    m_current_target_p = pRenderTarget;
    m_render_target.pData = pRenderTarget;
    m_depth_stencil_target.pData = pRenderTarget;
    set_viewport(m_viewPort);
}

// 0x004F21C0 (:1107)
void rl_renderer::end_pass()
{
    m_pCommandWriter->writeEndRenderPassCommand(keen::RenderTargetPartFlagMask_None);
    const unsigned int merged_index = m_merged_target_stack_index;
    if (merged_index != 0 && m_current_target_p == m_merged_targets_p[merged_index - 1] && m_current_target_p != nullptr)
    {
        m_merged_target_stack_index = merged_index - 1;
        keen::graphics::destroyRenderTarget(Init_params.m_pKeenGraphicsSystem, m_merged_targets_p[merged_index - 1]);
        m_merged_targets_p[m_merged_target_stack_index] = nullptr;
    }
    m_current_target_p = m_pCommandWriter->getCurrentRenderTarget();
    m_render_target.pData = nullptr;
    m_depth_stencil_target.pData = nullptr;
    set_viewport(m_viewPort);
}

// 0x004C57B0 (:1123)
const rl_dev_render_target& rl_renderer::get_render_target()
{
    return m_render_target;
}

// 0x004C57C0 (:1128)
const rl_dev_depth_stencil_target& rl_renderer::get_depth_stencil_target()
{
    return m_depth_stencil_target;
}

// 0x004F2360 (:1133)
void rl_renderer::set_texture(const unsigned int tmu, const int handle)
{
    m_pCommandWriter->writeSetFragmentShaderTextureCommand(tmu, get_dev_texture(handle).pData);
}

// 0x004C57D0 (:1138)
// Material constants (shared by the vertex and fragment material buffers).
void rl_renderer::set_vertex_shader_constant(unsigned int start_register, const float* data_p, unsigned int num_consts)
{
    memcpy(&m_materialConstantData[0].x + start_register, data_p, num_consts * sizeof(float));
    m_constantDataDirtyMask |= ConstantDataMask_MaterialData;
}

// 0x004C5810 (:1149)
void rl_renderer::set_pixel_shader_constant(unsigned int start_register, const float* data_p, unsigned int num_consts)
{
    memcpy(&m_materialConstantData[0].x + start_register, data_p, num_consts * sizeof(float));
    m_constantDataDirtyMask |= ConstantDataMask_MaterialData;
}

// 0x004DCA40 (.h:181)
void rl_renderer::set_pixel_shader_constant(unsigned int start_register, const unsigned int* data_p, unsigned int num_consts)
{
    memcpy(&m_materialConstantData[0].x + start_register, data_p, num_consts * sizeof(unsigned int));
    m_constantDataDirtyMask |= ConstantDataMask_MaterialData;
}

// 0x004F23C0 (:1160)
// The object transform constant is the current local world transform relative to the render offset, transposed to
// three rows.
void rl_renderer::update_local_transform()
{
    const matrix43& world = m_state_p->get_current_local_world_transform();
    const matrix rotation(world.m_rotation);
    const vector& offset = m_state_p->get_render_offset();
    const float translation_x = world.m_translation.x - offset.x;
    const float translation_y = world.m_translation.y - offset.y;
    const float translation_z = world.m_translation.z - offset.z;
    keen::GraphicsMatrix43 object_transform;
    object_transform.row0[0] = rotation.rvec.x;
    object_transform.row0[1] = rotation.uvec.x;
    object_transform.row0[2] = rotation.fvec.x;
    object_transform.row0[3] = translation_x;
    object_transform.row1[0] = rotation.rvec.y;
    object_transform.row1[1] = rotation.uvec.y;
    object_transform.row1[2] = rotation.fvec.y;
    object_transform.row1[3] = translation_y;
    object_transform.row2[0] = rotation.rvec.z;
    object_transform.row2[1] = rotation.uvec.z;
    object_transform.row2[2] = rotation.fvec.z;
    object_transform.row2[3] = translation_z;
    memcpy(&m_instanceVertexData.objTM, &object_transform, sizeof(object_transform));
    m_constantDataDirtyMask |= ConstantDataMask_InstanceVertexData;
}

// 0x004F2500 (:1188)
// The common vertex constants take the transposed world->clip (projTM) and view transforms.
void rl_renderer::update_transforms()
{
    update_local_transform();
    const vector& offset = m_state_p->get_render_offset();
    m_commonVertexData.renderOffset.x = offset.x;
    m_commonVertexData.renderOffset.y = offset.y;
    m_commonVertexData.renderOffset.z = offset.z;
    const matrix44 world_clip = m_state_p->get_world_clip_transform().copy_transpose();
    memcpy(&m_commonVertexData.projTM, &world_clip, sizeof(world_clip));
    const matrix44 view = m_state_p->get_view_transform().copy_transpose();
    memcpy(&m_commonVertexData.viewTM, &view, sizeof(view));
    m_constantDataDirtyMask |= ConstantDataMask_CommonVertexData;
}

// 0x005166B0 (:1213)
void rl_renderer::set_world_transform(const matrix43& world_transform)
{
    m_state_p->set_world_transform(world_transform);
    update_local_transform();
}

// 0x004F26C0 (:1220)
void rl_renderer::set_shadow_texture_transform(const matrix44& shadow_texture_matrix)
{
    m_state_p->set_shadow_texture_transform(shadow_texture_matrix);
}

// 0x004C5850 (:1226)
void rl_renderer::update_shadow_transforms()
{
}

// 0x004F26E0 (:1238)
// Uploads a dirty constant buffer (mask & testMask) to a vertex shader slot.
void flushDynamicVertexConstantBuffer(keen::GraphicsCommandWriter* pCommandWriter, unsigned int mask, unsigned int testMask, keen::DynamicConstantBuffer* pBuffer, unsigned int slotIndex, const void* pBufferSourceData, unsigned int bufferCapacity)
{
    if ((testMask & mask) == 0)
    {
        return;
    }
    void* pConstants = pCommandWriter->beginDynamicVertexConstants(pBuffer, slotIndex);
    memcpy(pConstants, pBufferSourceData, bufferCapacity);
    pCommandWriter->endDynamicVertexConstants();
}

// 0x004C5860 (:1248)
// Uploads a dirty constant buffer (mask & testMask) to a fragment shader slot.
void flushDynamicFragmentConstantBuffer(keen::GraphicsCommandWriter* pCommandWriter, unsigned int mask, unsigned int testMask, keen::DynamicConstantBuffer* pBuffer, unsigned int slotIndex, const void* pBufferSourceData, unsigned int bufferCapacity)
{
    if ((testMask & mask) == 0)
    {
        return;
    }
    void* pConstants = pCommandWriter->beginDynamicFragmentConstants(pBuffer, slotIndex);
    memcpy(pConstants, pBufferSourceData, bufferCapacity);
    pCommandWriter->endDynamicFragmentConstants();
}

// 0x00516720 (:1258)
// Commits the device state, the input layout and the dirty constant buffers before a draw.
void rl_renderer::flush_states()
{
    apply_macro_state(m_state_p->get_macro_state(), m_state_p->get_set_sampler_macro_states());
    commit_rasterizer_state();
    commit_input_layout();
    keen::GraphicsCommandWriter* pCommandWriter = m_pCommandWriter;
    flushDynamicVertexConstantBuffer(pCommandWriter, m_constantDataDirtyMask, ConstantDataMask_CommonVertexData, m_pCommonVertexConstants, Common_constants_slot, &m_commonVertexData, sizeof(m_commonVertexData));
    flushDynamicFragmentConstantBuffer(pCommandWriter, m_constantDataDirtyMask, ConstantDataMask_CommonFragmentData, m_pCommonFragmentConstants, Common_constants_slot, &m_commonFragmentData, sizeof(m_commonFragmentData));
    flushDynamicVertexConstantBuffer(pCommandWriter, m_constantDataDirtyMask, ConstantDataMask_DecalVertexData, m_pDecalVertexConstants, Decal_constants_slot, &m_decalVertexData, sizeof(m_decalVertexData));
    flushDynamicVertexConstantBuffer(pCommandWriter, m_constantDataDirtyMask, ConstantDataMask_MaterialData, m_pMaterialVertexConstants, Material_constants_slot, m_materialConstantData, sizeof(m_materialConstantData));
    flushDynamicFragmentConstantBuffer(pCommandWriter, m_constantDataDirtyMask, ConstantDataMask_MaterialData, m_pMaterialFragmentConstants, Material_constants_slot, m_materialConstantData, sizeof(m_materialConstantData));
    flushDynamicVertexConstantBuffer(pCommandWriter, m_constantDataDirtyMask, ConstantDataMask_InstanceVertexData, m_pInstanceVertexConstants, Instance_constants_slot, &m_instanceVertexData, sizeof(m_instanceVertexData));
    flushDynamicFragmentConstantBuffer(pCommandWriter, m_constantDataDirtyMask, ConstantDataMask_InstanceFragmentData, m_pInstanceFragmentConstants, Instance_constants_slot, &m_instanceFragmentData, sizeof(m_instanceFragmentData));
    m_constantDataDirtyMask = 0;
}

// 0x00516860 (:1279)
// Draws num_verts indices (or vertices) of a geometry from start_idx. 0 draws all of it.
void rl_renderer::draw_geometry(const keen::RenderGeometry* pGeometry, const unsigned int stride, const unsigned int num_verts, const unsigned int start_idx)
{
    flush_states();
    keen::VertexBufferData vertex_buffer;
    vertex_buffer.pBuffer = pGeometry->pVertexBuffer;
    vertex_buffer.vertexCount = pGeometry->vertexCount;
    vertex_buffer.offset = 0;
    const keen::IndexFormat index_format = keen::graphics::getIndexFormat(pGeometry);
    keen::IndexBufferData index_buffer;
    index_buffer.pBuffer = pGeometry->pIndexBuffer;
    index_buffer.indexCount = pGeometry->indexCount;
    index_buffer.offset = (index_format == keen::IndexFormat_Uint16 ? 2 : 4) * start_idx;
    index_buffer.indexFormat = pGeometry->indexFormat;
    keen::PrimitiveData primitive;
    primitive.primitiveTopology = pGeometry->primitiveTopology;
    primitive.vertexDrawCount = num_verts;
    if (num_verts == 0)
    {
        primitive.vertexDrawCount = pGeometry->pIndexBuffer == nullptr ? pGeometry->vertexCount : pGeometry->indexCount;
    }
    m_pCommandWriter->writeDrawVertexBuffersCommand(keen::Matrix43(), &vertex_buffer, 1, &index_buffer, &primitive);
}

// 0x004F2790 (:1299)
// Uploads the bone matrices (transposed to three rows each) to the skinning constants.
void rl_renderer::set_bone_transforms(const matrix43* bone_matrices, int num_bone_matrices)
{
    keen::GraphicsMatrix43* bones_p = (keen::GraphicsMatrix43*)m_pCommandWriter->beginDynamicVertexConstants(m_pSkinningVertexConstants, Skinning_constants_slot);
    for (int i = 0; i < num_bone_matrices; i++)
    {
        const matrix43& bone = bone_matrices[i];
        bones_p[i].row0[0] = bone.m_rotation.rvec.x;
        bones_p[i].row0[1] = bone.m_rotation.uvec.x;
        bones_p[i].row0[2] = bone.m_rotation.fvec.x;
        bones_p[i].row0[3] = bone.m_translation.x;
        bones_p[i].row1[0] = bone.m_rotation.rvec.y;
        bones_p[i].row1[1] = bone.m_rotation.uvec.y;
        bones_p[i].row1[2] = bone.m_rotation.fvec.y;
        bones_p[i].row1[3] = bone.m_translation.y;
        bones_p[i].row2[0] = bone.m_rotation.rvec.z;
        bones_p[i].row2[1] = bone.m_rotation.uvec.z;
        bones_p[i].row2[2] = bone.m_rotation.fvec.z;
        bones_p[i].row2[3] = bone.m_translation.z;
    }
    m_pCommandWriter->endDynamicVertexConstants();
}

// 0x004F2990 (:1319)
void rl_renderer::setVertexBufferData(unsigned int streamIndex, keen::VertexBufferData* pData)
{
    if (pData != nullptr)
    {
        m_vertexBufferData[streamIndex] = *pData;
        return;
    }
    m_vertexBufferData[streamIndex].pBuffer = nullptr;
    m_vertexBufferData[streamIndex].vertexCount = 0;
    m_vertexBufferData[streamIndex].offset = 0;
}

// 0x00516980 (:1333)
// Draws one render block of a mesh with the vertex streams set by setVertexBufferData (stream 0 is the mesh's).
void rl_renderer::render(const rl_mesh* mesh_p, int sub_mesh_idx, int block_idx)
{
    const rl_mesh_data_runtime* mesh_data_p = mesh_p->m_data_set ? &mesh_p->m_data : nullptr;
    const rl_render_block& block = mesh_data_p->sub_meshes[sub_mesh_idx].render_blocks.to_pointer()[block_idx];
    set_vertex_format((rl_dev_vertex_format)mesh_data_p->vertex_buffer.vertex_format);
    flush_states();
    const keen::RenderGeometry& geometry = mesh_p->m_geometry;
    m_vertexBufferData[0].pBuffer = geometry.pVertexBuffer;
    m_vertexBufferData[0].offset = 0;
    m_vertexBufferData[0].vertexCount = geometry.vertexCount;
    const keen::IndexFormat index_format = keen::graphics::getIndexFormat(&geometry);
    keen::IndexBufferData index_buffer;
    index_buffer.pBuffer = geometry.pIndexBuffer;
    index_buffer.indexCount = geometry.indexCount;
    index_buffer.offset = block.start_index * (index_format == keen::IndexFormat_Uint16 ? 2 : 4);
    index_buffer.indexFormat = geometry.indexFormat;
    keen::PrimitiveData primitive;
    primitive.primitiveTopology = geometry.primitiveTopology;
    primitive.vertexDrawCount = block.num_indices;
    if (primitive.vertexDrawCount == 0)
    {
        primitive.vertexDrawCount = geometry.pIndexBuffer == nullptr ? geometry.vertexCount : geometry.indexCount;
    }
    // Magic number: three vertex streams.
    m_pCommandWriter->writeDrawVertexBuffersCommand(keen::Matrix43(), m_vertexBufferData, 3, &index_buffer, &primitive);
}

// 0x004F29D0 (:1385)
const matrix43& rl_renderer::get_world_transform() const
{
    return m_state_p->get_world_transform();
}

// 0x004F29F0 (:1390)
const matrix44& rl_renderer::get_projection_transform() const
{
    return m_state_p->get_projection_transform();
}

// 0x004F2A00 (:1395)
const matrix44& rl_renderer::get_clip_screen_transform() const
{
    return m_state_p->get_clip_screen_transform();
}

// 0x004F2A10 (:1400)
const matrix44& rl_renderer::get_local_clip_transform() const
{
    return m_state_p->get_local_clip_transform();
}

// 0x004F2A20 (:1405)
const matrix44& rl_renderer::get_world_clip_transform() const
{
    return m_state_p->get_world_clip_transform();
}

// 0x004C5910 (:1410)
void rl_renderer::set_terrain_subzone_offset(const vector& offset)
{
    m_instanceVertexData.Terrain_subzone_offset.x = offset.x;
    m_instanceVertexData.Terrain_subzone_offset.y = offset.y;
    m_instanceVertexData.Terrain_subzone_offset.z = offset.z;
    m_constantDataDirtyMask |= ConstantDataMask_InstanceVertexData;
}

// 0x00516B20 (:1416)
void rl_renderer::push_world_transform(const matrix43& world_transform)
{
    m_state_p->push_world_transform(world_transform);
    update_local_transform();
}

// 0x00516B70 (:1423)
void rl_renderer::pop_world_transform()
{
    m_state_p->pop_world_transform();
    update_local_transform();
}

// 0x004F2A30 (:1430)
// targetDimensions is (0, 0, 1 / width, 1 / height).
void rl_renderer::set_target_dimensions(const unsigned int width, const unsigned int height)
{
    m_state_p->set_target_dimensions(width, height);
    const float fwidth = (float)width;
    const float fheight = (float)height;
    m_commonFragmentData.targetDimensions.x = 0.0f / fwidth;
    m_commonFragmentData.targetDimensions.y = 0.0f / fheight;
    m_commonFragmentData.targetDimensions.z = 1.0f / fwidth;
    m_commonFragmentData.targetDimensions.w = 1.0f / fheight;
    m_commonVertexData.targetDimensions.x = 0.0f / fwidth;
    m_commonVertexData.targetDimensions.y = 0.0f / fheight;
    m_commonVertexData.targetDimensions.z = 1.0f / fwidth;
    m_commonVertexData.targetDimensions.w = 1.0f / fheight;
    m_constantDataDirtyMask |= ConstantDataMask_CommonVertexData | ConstantDataMask_CommonFragmentData;
}

// 0x004F2B20 (:1442)
void rl_renderer::get_target_dimensions(unsigned int& width, unsigned int& height)
{
    m_state_p->get_target_dimensions(width, height);
}

// 0x004F2B50 (:1447)
// Uploads the light to the lighting constants; the main (sun) light's color is scaled by g_mainLightColorScale.
void rl_renderer::set_light(const rl_light& light, bool isMainLight)
{
    keen::LightingVertexShaderConstants* vertex_constants_p = (keen::LightingVertexShaderConstants*)m_pCommandWriter->beginDynamicVertexConstants(m_pLightingVertexConstants, Lighting_constants_slot);
    light.fillLightingVertexShaderData(vertex_constants_p, m_state_p->get_render_offset());
    m_pCommandWriter->endDynamicVertexConstants();
    keen::LightingFragmentShaderConstants* fragment_constants_p = (keen::LightingFragmentShaderConstants*)m_pCommandWriter->beginDynamicFragmentConstants(m_pLightingFragmentConstants, Lighting_constants_slot);
    light.fillLightingFragmentShaderData(fragment_constants_p, isMainLight ? g_mainLightColorScale : 1.0f);
    m_pCommandWriter->endDynamicFragmentConstants();
}

// 0x004F2CA0 (:1461)
// Only the light position (the first 3 floats of the lighting vertex constants) is written.
void rl_renderer::set_dummy_light_pos(const vector& pos)
{
    float* constants_p = (float*)m_pCommandWriter->beginDynamicVertexConstants(m_pLightingVertexConstants, Lighting_constants_slot);
    constants_p[0] = pos.x;
    constants_p[1] = pos.y;
    constants_p[2] = pos.z;
    m_pCommandWriter->endDynamicVertexConstants();
}

// 0x004C5940 (:1470)
void rl_renderer::set_decal_transform(const vector& pos, const matrix& rotation)
{
    m_decalVertexData.pos.x = pos.x;
    m_decalVertexData.pos.y = pos.y;
    m_decalVertexData.pos.z = pos.z;
    m_decalVertexData.xvec.x = rotation.rvec.x;
    m_decalVertexData.xvec.y = rotation.rvec.y;
    m_decalVertexData.xvec.z = rotation.rvec.z;
    m_decalVertexData.yvec.x = rotation.uvec.x;
    m_decalVertexData.yvec.y = rotation.uvec.y;
    m_decalVertexData.yvec.z = rotation.uvec.z;
    m_decalVertexData.zvec.x = rotation.fvec.x;
    m_decalVertexData.zvec.y = rotation.fvec.y;
    m_decalVertexData.zvec.z = rotation.fvec.z;
    m_constantDataDirtyMask |= ConstantDataMask_DecalVertexData;
}

// 0x004C59E0 (:1480)
void rl_renderer::set_decal_params(const vector4& tint, const vector4& params1, const vector4& params2)
{
    m_decalVertexData.tint.x = tint.x;
    m_decalVertexData.tint.y = tint.y;
    m_decalVertexData.tint.z = tint.z;
    m_decalVertexData.tint.w = tint.w;
    m_decalVertexData.params1.x = params1.x;
    m_decalVertexData.params1.y = params1.y;
    m_decalVertexData.params1.z = params1.z;
    m_decalVertexData.params1.w = params1.w;
    m_decalVertexData.params2.x = params2.x;
    m_decalVertexData.params2.y = params2.y;
    m_decalVertexData.params2.z = params2.z;
    m_decalVertexData.params2.w = params2.w;
    m_constantDataDirtyMask |= ConstantDataMask_DecalVertexData;
}

// 0x004F2D50 (:1489)
void rl_renderer::set_fill_mode(const rl_fill_mode mode)
{
    m_state_p->set_fill_mode(mode);
}

// 0x004F2D60 (:1494)
const rl_fill_mode rl_renderer::get_fill_mode() const
{
    return m_state_p->get_fill_mode();
}

// 0x004F2D70 (:1499)
void rl_renderer::set_scissor_test_enabled(const bool enabled)
{
    m_state_p->set_scissor_test_enabled(enabled);
}

// 0x004F2D90 (:1504)
const bool rl_renderer::get_scissor_test_enabled() const
{
    return m_state_p->get_scissor_test_enabled();
}

// 0x004F2DA0 (:1509)
void rl_renderer::set_msaa_enabled(const bool enabled)
{
    m_state_p->set_msaa_enabled(enabled);
}

// 0x004F2DC0 (:1514)
const bool rl_renderer::get_msaa_enabled() const
{
    return m_state_p->get_msaa_enabled();
}

// 0x004F2DD0 (:1519)
void rl_renderer::set_zbias_mode(const rl_prim_zbias_mode enabled)
{
    m_state_p->set_zbias_mode(enabled);
}

// 0x004F2DF0 (:1524)
const rl_prim_zbias_mode rl_renderer::get_zbias_mode() const
{
    return m_state_p->get_zbias_mode();
}

// 0x004F2E00 (:1529)
// Notes (decompiled behaviour, kept as is): the fill mode is ignored (always solid).
void rl_renderer::commit_rasterizer_state()
{
    rl_cull_mode cull_mode = m_state_p->get_cull_mode();
    if (cull_mode == RL_CULL_SHADER_DRIVEN)
    {
        cull_mode = m_state_p->get_shader_cull_mode();
    }
    int cull_index;
    if (cull_mode == RL_CULL_CCW)
    {
        cull_index = 0;
    }
    else if (cull_mode == RL_CULL_CW)
    {
        cull_index = 1;
    }
    else
    {
        if (cull_mode != RL_CULL_NONE)
        {
            VLIB_CRITICAL_FAILURE();
        }
        cull_index = 2;
    }
    const keen::RasterizerState* state_p = m_pRenderLib->m_engine_rasterizer_states[m_state_p->get_zbias_mode()][0][cull_index][m_state_p->get_scissor_test_enabled() ? 1 : 0].m_internal_state_p;
    m_pCommandWriter->writeSetRasterizerStateCommand(state_p);
}

// 0x004C5A90 (:1570)
void rl_renderer::resolve_msaa_target(const rl_dev_render_target&, const rl_dev_texture&)
{
}

// 0x0054D050 (:1575)
void rl_renderer::release_all_resources()
{
    Active_scene_p->m_scene_renderer_p->release_resources();
    rl_singleton_phoenix<rl_particle_renderer>::get_instance()->release_resources();
    rl_dyn_texture_base::release_resources();
}

// 0x00553DF0 (:1582)
void rl_renderer::renew_all_resources()
{
    rl_dyn_texture_base::renew_resources(*this);
    rl_singleton_phoenix<rl_particle_renderer>::get_instance()->create_render_targets();
    rl_scene_renderer* scene_renderer_p = Active_scene_p->m_scene_renderer_p;
    keen::GraphicsCommandWriter* pCommandWriter = m_pCommandWriter;
    scene_renderer_p->m_level_of_detail = scene_renderer_p->m_new_level_of_detail;
    scene_renderer_p->m_hdr_compositor_p->m_level_of_detail = scene_renderer_p->m_level_of_detail;
    scene_renderer_p->create_render_textures(pCommandWriter);
    if (scene_renderer_p->m_hdr_compositor_p != nullptr)
    {
        scene_renderer_p->m_hdr_compositor_p->create_render_textures(pCommandWriter);
    }
    if (scene_renderer_p->m_outline_compositor_p != nullptr)
    {
        scene_renderer_p->m_outline_compositor_p->renew_resources();
    }
}

// 0x004F2EA0 (:1592)
void rl_renderer::commit_device_state()
{
    apply_macro_state(m_state_p->get_macro_state(), m_state_p->get_set_sampler_macro_states());
    commit_rasterizer_state();
}

// 0x004C5AA0 (:1598)
void rl_renderer::cleanup_material()
{
    m_flags &= ~kAlphaTest;
}

// 0x004C5AB0 (:1603)
void rl_renderer::set_texture(unsigned int tmu, const rl_dev_texture& texture)
{
    m_pCommandWriter->writeSetFragmentShaderTextureCommand(tmu, texture.pData);
}

// 0x004F2ED0 (:1608)
void rl_renderer::copy_render_texture(rl_render_texture& src, rl_dev_texture& dst)
{
    if (src.m_msaa_texture.pData != nullptr && (src.m_flags & 1) != 0)
    {
        src.m_flags &= ~1u;
    }
    const rl_dev_texture source = get_dev_texture(src.m_dyn_texture_p != nullptr ? src.m_dyn_texture_p->get_texture_handle() : -1);
    m_pCommandWriter->writeCopyTextureDataCommand(dst.pData, source.pData);
}

// 0x004C5B00 (:1613)
void rl_renderer::set_shader_pipeline(const keen::ShaderPipeline* pipeline_p, const keen::VertexInputBinding** bindings_p)
{
    m_input_layouts_p = bindings_p;
    m_pipeline_p = pipeline_p;
}

// 0x004F2F30 (:1619)
// Binds the pipeline with the input binding of the current vertex format, created on first use (under the render
// lib's input binding mutex: renderers on other threads share the shader's binding cache).
void rl_renderer::commit_input_layout()
{
    const keen::VertexInputBinding* binding_p = nullptr;
    if (m_input_layouts_p != nullptr)
    {
        if (m_input_layouts_p[m_vertex_format] == nullptr)
        {
            rl_render_lib* render_lib_p = rl_render_lib::get_instance();
            const keen::VertexFormat* vertex_format_p = render_lib_p->m_vertex_formats_p[m_vertex_format];
            render_lib_p->lockInputBindingMutex();
            if (m_input_layouts_p[m_vertex_format] == nullptr)
            {
                // Magic number: all geometry modes (0x7).
                m_input_layouts_p[m_vertex_format] = keen::graphics::createVertexInputBinding(Init_params.m_pKeenGraphicsSystem, vertex_format_p, m_pipeline_p, 7);
            }
            render_lib_p->unlockInputBindingMutex();
        }
        binding_p = m_input_layouts_p[m_vertex_format];
    }
    if (m_pipeline_p != nullptr && binding_p != nullptr)
    {
        m_pCommandWriter->writeSetShaderPipelineCommand(m_pipeline_p, binding_p, false);
    }
}

// 0x00516B90 (:1653)
// Selects the shader of a library handle for the current shader type and configuration (or the override shader).
bool rl_renderer::set_active_shader(unsigned int handle, rl_shader_technique_type technique_type, int user_technique_idx)
{
    if (handle == 0xFFFFFFFF)
    {
        return false;
    }
    const rl_shader_type shader_type = m_state_p->get_shader_type();
    const int configuration = m_state_p->get_shader_configuration();
    if (m_override_shader != 0xFFFFFFFF)
    {
        handle = m_override_shader;
    }
    rl_shader* shader_p = rl_shader_library::get_instance()->get_rl_shader(handle, shader_type, configuration);
    if (shader_p == nullptr)
    {
        return false;
    }
    set_current_shader(shader_p, shader_p->find_technique_to_use(technique_type, user_technique_idx));
    return true;
}

// 0x004C5B20 (:1692)
void rl_renderer::set_override_shader(unsigned int handle)
{
    m_override_shader = handle;
}

// 0x004C5B30 (:1697)
void rl_renderer::clear_override_shader()
{
    m_override_shader = 0xFFFFFFFF;
}

// 0x004C5B40 (:1702)
// Retail strips the debug renderer (render_debug.md).
rl_renderer* rl_getDebugRenderer()
{
    return nullptr;
}

// 0x004F3050 (:1707)
// Copies the constants and the render state of another renderer (the main renderer to the deferred one).
void rl_renderer::copy(const rl_renderer& other)
{
    memcpy(&m_commonVertexData, &other.m_commonVertexData, sizeof(m_commonVertexData));
    memcpy(&m_commonFragmentData, &other.m_commonFragmentData, sizeof(m_commonFragmentData));
    memcpy(&m_instanceVertexData, &other.m_instanceVertexData, sizeof(m_instanceVertexData));
    memcpy(&m_instanceFragmentData, &other.m_instanceFragmentData, sizeof(m_instanceFragmentData));
    memcpy(&m_decalVertexData, &other.m_decalVertexData, sizeof(m_decalVertexData));
    memcpy(m_materialConstantData, other.m_materialConstantData, sizeof(m_materialConstantData));
    // Magic number: every constant buffer is dirty.
    m_constantDataDirtyMask = 0xFF;
    memcpy(m_state_p, other.m_state_p, sizeof(rl_state_manager));
}

// 0x004C5B50 (:1720)
void rl_renderer::dump() const
{
}

// 0x004B7A40 (.h:153)
rl_material_state_store* rl_renderer::get_state_store()
{
    return &m_state_store;
}

// 0x004B7A50 (.h:279)
void rl_renderer::set_object_instance_parameters(const vector4& inst_params)
{
    m_instanceVertexData.instanceData.x = inst_params.x;
    m_instanceVertexData.instanceData.y = inst_params.y;
    m_instanceVertexData.instanceData.z = inst_params.z;
    m_instanceVertexData.instanceData.w = inst_params.w;
    m_constantDataDirtyMask |= ConstantDataMask_InstanceVertexData;
}

// 0x004B7A90 (.h:285)
void rl_renderer::set_object_transform(const matrix43& transform)
{
    memcpy(&m_instanceVertexData.objTM, &transform, sizeof(transform));
    m_constantDataDirtyMask |= ConstantDataMask_InstanceVertexData;
}

// 0x004DCA80 (.h:384)
void rl_renderer::set_full_state(const rl_state_manager& other)
{
    memcpy(m_state_p, &other, sizeof(rl_state_manager));
}

// 0x007B99C0 (.h:385)
const rl_state_manager& rl_renderer::get_full_state()
{
    return *m_state_p;
}

// 0x004B7AC0 (.h:413)
void rl_renderer::set_alpha_test(bool doAlphaTest, float alphaRef)
{
    m_constantDataDirtyMask |= ConstantDataMask_InstanceFragmentData;
    m_instanceFragmentData.alphaTestRef = alphaRef;
    m_instanceFragmentData.doExplicitAlphaTest = doAlphaTest ? 1 : 0;
}

// 0x004B7AF0 (.h:490)
void rl_renderer::set_flag(const unsigned int flag, const bool val)
{
    if (val)
    {
        m_flags |= flag;
        return;
    }
    m_flags &= ~flag;
}

// 0x004B7B20 (.h:491)
const bool rl_renderer::check_flag(const unsigned int flag)
{
    return (m_flags & flag) == flag;
}

// 0x004B7A20 (.h:101)
rl_index_stream_info::rl_index_stream_info()
{
}

// 0x004DCAA0 (.h:528)
int RL_screen_width()
{
    return rl_render_lib::get_instance()->get_screen_info()->get_screen_width();
}

// 0x004DCAB0 (.h:533)
int RL_screen_height()
{
    return rl_render_lib::get_instance()->get_screen_info()->get_screen_height();
}
