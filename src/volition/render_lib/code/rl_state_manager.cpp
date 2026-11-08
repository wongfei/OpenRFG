// Original: volition/render_lib/code/rl_state_manager.cpp
//
// rl_state_manager: the render state of an rl_renderer (primitive state, colors, fog, transforms) and its compact
// serialization into the primitive renderer's deferred state buffer.

#include "volition/render_lib/code/rl_state_manager.h"

#include "volition/render_lib/code/rl_renderer.h"
#include "volition/render_lib/code/rl_screen_info.h"

#include <string.h>

const unsigned int rl_state_manager::kMaxPixelShaderTextures = 16; // 0x012B8040
const unsigned int rl_state_manager::kSetSamplerMacroStates = 0x01; // 0x012B8044
const unsigned int rl_state_manager::kDepthOnly = 0x02; // 0x012B8048
const unsigned int rl_state_manager::kDepthRequiresPixelShader = 0x04; // 0x012B804C
const unsigned int rl_state_manager::kIsBasePass = 0x08; // 0x012B8050
const unsigned int rl_state_manager::kScissorEnabled = 0x10; // 0x012B8054
const unsigned int rl_state_manager::kMsaaEnabled = 0x20; // 0x012B8058
const unsigned int rl_state_manager::kIsPrimitiveStateDirty = 0x40; // 0x012B805C

// Magic numbers: the serialized primitive render state is a 4-byte signature followed by 0xDC bytes of state.
static const unsigned int Primitive_render_state_signature = 0x21436587;
static const int Primitive_render_state_size = 0xDC;

// 0x004D2170 (:51)
// The matrices other than the first local world transform are left uninitialized.
rl_state_manager::rl_state_manager()
{
    m_flags = kIsBasePass;
    m_primitive_color.set(0.0f, 0.0f, 0.0f, 0.0f);
    m_shader_type = shader_type_generic;
    m_shader_configuration = 0;
    m_shader_p = nullptr;
    m_cull_mode = RL_CULL_CCW;
    m_shader_cull_mode = RL_CULL_CCW;
    m_primitive_texture = -1;
    m_primitive_bink_y_texture = -1;
    m_primitive_bink_cr_texture = -1;
    m_primitive_bink_cb_texture = -1;
    m_primitive_material = 0xFFFFFFFF;
    m_vertex_format = RLVF_INVALID;
    m_shader_technique = -1;
    m_fade_color.set(0.0f, 0.0f, 0.0f, 0.0f);
    m_tint_color.set(0.0f, 0.0f, 0.0f, 0.0f);
    m_local_world_stack_top = 0;
    m_saturation = 0.0f;
    m_active_camera_p = nullptr;
    m_zbias_mode = RLPZB_NONE;
    m_macro_state.invalidate();
    m_local_world_transform[m_local_world_stack_top] = IDENTITY_MATRIX43;
    m_scissor_region.m_bottom = 0;
    m_scissor_region.m_top = 0;
    m_scissor_region.m_right = 0;
    m_scissor_region.m_left = 0;
}

// 0x004D2280 (:58)
rl_state_manager::~rl_state_manager()
{
}

// 0x004D2290 (:63)
void rl_state_manager::operator=(const rl_state_manager& other)
{
    memcpy(this, &other, sizeof(rl_state_manager));
}

// 0x004FEF50 (:68)
void rl_state_manager::set_primitive_color(const rl_color_float& color)
{
    set_primitive_buffered_state_is_dirty(true);
    m_primitive_color = color;
}

// 0x004D22B0 (:74)
const rl_color_float& rl_state_manager::get_primitive_color() const
{
    return m_primitive_color;
}

// 0x00531B90 (:79)
// A texture replaces the material and the bink textures.
void rl_state_manager::set_primitive_texture(const int tex)
{
    if (m_primitive_texture == tex)
    {
        return;
    }
    set_primitive_buffered_state_is_dirty(true);
    m_primitive_texture = tex;
    if (tex != -1)
    {
        set_primitive_material(0xFFFFFFFF);
        set_primitive_buffered_state_is_dirty(true);
        m_primitive_bink_y_texture = -1;
        m_primitive_bink_cr_texture = -1;
        m_primitive_bink_cb_texture = -1;
    }
}

// 0x004D22C0 (:91)
const int rl_state_manager::get_primitive_texture() const
{
    return m_primitive_texture;
}

// 0x00519670 (:96)
// A material replaces the texture and the bink textures.
void rl_state_manager::set_primitive_material(const unsigned int mat)
{
    if (m_primitive_material == mat)
    {
        return;
    }
    set_primitive_buffered_state_is_dirty(true);
    m_primitive_material = mat;
    if (mat != 0xFFFFFFFF)
    {
        set_primitive_texture(-1);
        set_primitive_buffered_state_is_dirty(true);
        m_primitive_bink_y_texture = -1;
        m_primitive_bink_cr_texture = -1;
        m_primitive_bink_cb_texture = -1;
    }
}

// 0x004D22D0 (:108)
const unsigned int rl_state_manager::get_primitive_material() const
{
    return m_primitive_material;
}

// 0x004FEF70 (:113)
// A complete set of bink (YCrCb) textures replaces the texture and the material.
void rl_state_manager::set_primitive_bink_textures(const int y, const int cr, const int cb)
{
    set_primitive_buffered_state_is_dirty(true);
    m_primitive_bink_y_texture = y;
    m_primitive_bink_cr_texture = cr;
    m_primitive_bink_cb_texture = cb;
    if (y != -1 && cr != -1 && cb != -1)
    {
        set_primitive_texture(-1);
        set_primitive_material(0xFFFFFFFF);
    }
}

// 0x004D22E0 (:125)
void rl_state_manager::get_primitive_bink_textures(int& y, int& cr, int& cb) const
{
    y = m_primitive_bink_y_texture;
    cr = m_primitive_bink_cr_texture;
    cb = m_primitive_bink_cb_texture;
}

// 0x004D2300 (:132)
void rl_state_manager::set_vertex_format(rl_dev_vertex_format id)
{
    m_vertex_format = id;
}

// 0x004D2310 (:137)
const rl_dev_vertex_format rl_state_manager::get_vertex_format() const
{
    return m_vertex_format;
}

// 0x004D2320 (:142)
void rl_state_manager::set_shader_type(const rl_shader_type type)
{
    m_shader_type = type;
}

// 0x004D2330 (:147)
const rl_shader_type rl_state_manager::get_shader_type() const
{
    return m_shader_type;
}

// 0x004D2340 (:152)
void rl_state_manager::set_shader_configuration(const int config)
{
    m_shader_configuration = config;
}

// 0x004D2350 (:157)
const int rl_state_manager::get_shader_configuration() const
{
    return m_shader_configuration;
}

// 0x004D2360 (:162)
void rl_state_manager::set_shader(rl_shader* const shader_p, const int tech)
{
    m_shader_p = shader_p;
    m_shader_technique = tech;
}

// 0x004D2380 (:168)
void rl_state_manager::get_shader(rl_shader*& shader_p, int& tech) const
{
    shader_p = m_shader_p;
    tech = m_shader_technique;
}

// 0x004FEFB0 (:174)
void rl_state_manager::set_macro_state(const rl_primitive_state& state)
{
    set_primitive_buffered_state_is_dirty(true);
    m_macro_state = state;
}

// 0x004D23A0 (:180)
const rl_primitive_state& rl_state_manager::get_macro_state() const
{
    return m_macro_state;
}

// 0x004D23B0 (:185)
void rl_state_manager::set_cull_mode(const rl_cull_mode mode)
{
    m_cull_mode = mode;
}

// 0x004D23C0 (:190)
const rl_cull_mode rl_state_manager::get_cull_mode() const
{
    return m_cull_mode;
}

// 0x004D23D0 (:195)
void rl_state_manager::set_shader_cull_mode(const rl_cull_mode mode)
{
    m_shader_cull_mode = mode;
}

// 0x004D23E0 (:200)
const rl_cull_mode rl_state_manager::get_shader_cull_mode() const
{
    return m_shader_cull_mode;
}

// 0x004D23F0 (:205)
const bool rl_state_manager::get_depth_only() const
{
    return (m_flags & kDepthOnly) != 0;
}

// 0x004D2400 (:210)
void rl_state_manager::set_depth_only(const bool depth_only)
{
    m_flags = depth_only ? (m_flags | kDepthOnly) : (m_flags & ~kDepthOnly);
}

// 0x004D2420 (:215)
const bool rl_state_manager::get_depth_requires_pixel_shader() const
{
    return (m_flags & kDepthRequiresPixelShader) != 0;
}

// 0x004D2430 (:220)
void rl_state_manager::set_depth_requires_pixel_shader(const bool depth_required)
{
    m_flags = depth_required ? (m_flags | kDepthRequiresPixelShader) : (m_flags & ~kDepthRequiresPixelShader);
}

// 0x004D2450 (:225)
const bool rl_state_manager::get_is_base_pass() const
{
    return (m_flags & kIsBasePass) != 0;
}

// 0x004D2460 (:230)
void rl_state_manager::set_is_base_pass(const bool is_base_pass)
{
    m_flags = is_base_pass ? (m_flags | kIsBasePass) : (m_flags & ~kIsBasePass);
}

// 0x004D2480 (:235)
const rl_color_float& rl_state_manager::get_fade_color() const
{
    return m_fade_color;
}

// 0x004FEFC0 (:240)
void rl_state_manager::set_fade_color(const rl_color_float& color)
{
    set_primitive_buffered_state_is_dirty(true);
    m_fade_color = color;
}

// 0x004D2490 (:246)
const rl_color_float& rl_state_manager::get_tint_color() const
{
    return m_tint_color;
}

// 0x004FEFE0 (:251)
void rl_state_manager::set_tint_color(const rl_color_float& color)
{
    set_primitive_buffered_state_is_dirty(true);
    m_tint_color = color;
}

// 0x004D24A0 (:257)
const float rl_state_manager::get_saturation() const
{
    return m_saturation;
}

// 0x004FF000 (:262)
void rl_state_manager::set_saturation(const float sat)
{
    set_primitive_buffered_state_is_dirty(true);
    m_saturation = sat;
}

// 0x004D24B0 (:268)
rl_camera* const rl_state_manager::get_active_camera() const
{
    return m_active_camera_p;
}

// 0x004D24C0 (:273)
void rl_state_manager::set_active_camera(rl_camera* camera_p)
{
    m_active_camera_p = camera_p;
}

// 0x004D24D0 (:278)
// world->clip = view * projection, world->screen = world->clip * clip->screen and local->clip = the current local
// world transform (as a 4x4 matrix) * world->clip.
void rl_state_manager::calculate_utility_transformations()
{
    m_world_clip_transform = m_world_view_transform * m_projection_transform;
    m_world_screen_transform = m_world_clip_transform * m_clip_screen_transform;
    const matrix43& local_world = m_local_world_transform[m_local_world_stack_top];
    const matrix44 local_world44(
        vector4(local_world.m_rotation.rvec.x, local_world.m_rotation.rvec.y, local_world.m_rotation.rvec.z, 0.0f),
        vector4(local_world.m_rotation.uvec.x, local_world.m_rotation.uvec.y, local_world.m_rotation.uvec.z, 0.0f),
        vector4(local_world.m_rotation.fvec.x, local_world.m_rotation.fvec.y, local_world.m_rotation.fvec.z, 0.0f),
        vector4(local_world.m_translation.x, local_world.m_translation.y, local_world.m_translation.z, 1.0f));
    m_local_clip_transform = local_world44 * m_world_clip_transform;
}

// 0x004D3870 (:292)
void rl_state_manager::set_3d_transforms(const matrix44& world_view_transform, const matrix44& projection_transform)
{
    m_world_view_transform = world_view_transform;
    m_projection_transform = projection_transform;
    calculate_utility_transformations();
}

// 0x004D38B0 (:302)
void rl_state_manager::get_3d_transforms(matrix44& world_view_transform, matrix44& projection_transform)
{
    world_view_transform = m_world_view_transform;
    projection_transform = m_projection_transform;
}

// 0x004D38E0 (:309)
// clip->screen maps x and y from [-1, 1] to pixels (y down).
void rl_state_manager::set_screen_info(const rl_screen_info& screen_info)
{
    const int width = screen_info.get_screen_width();
    const int height = screen_info.get_screen_height();
    m_clip_screen_transform.set(
        (float)width * 0.5f, 0.0f, 0.0f, 0.0f,
        0.0f, (float)height * -0.5f, 0.0f, 0.0f,
        0.0f, 0.0f, 1.0f, 0.0f,
        (float)width * 0.5f, (float)height * 0.5f, 0.0f, 1.0f);
    calculate_utility_transformations();
}

// 0x004FF020 (:322)
void rl_state_manager::set_eye_position(const vector& eye_pos)
{
    set_primitive_buffered_state_is_dirty(true);
    m_eye_position = eye_pos;
}

// 0x004D39D0 (:328)
const vector& rl_state_manager::get_eye_position() const
{
    return m_eye_position;
}

// 0x004FF040 (:333)
void rl_state_manager::set_fog_parameters(float height_density, float atmosphere_scale, float density, float density_offset)
{
    set_primitive_buffered_state_is_dirty(true);
    m_fog_parameters.x = height_density;
    m_fog_parameters.y = atmosphere_scale;
    m_fog_parameters.z = density;
    m_fog_parameters.w = density_offset;
}

// 0x004D39E0 (:342)
void rl_state_manager::get_fog_parameters(float& height_density, float& atmosphere_scale, float& density, float& density_offset) const
{
    height_density = m_fog_parameters.x;
    atmosphere_scale = m_fog_parameters.y;
    density = m_fog_parameters.z;
    density_offset = m_fog_parameters.w;
}

// 0x004FF080 (:350)
void rl_state_manager::set_fog_color(const rl_color_float& fog_color)
{
    set_primitive_buffered_state_is_dirty(true);
    m_fog_color = fog_color;
}

// 0x004D3A20 (:356)
const rl_color_float& rl_state_manager::get_fog_color() const
{
    return m_fog_color;
}

// 0x004FF0B0 (:361)
void rl_state_manager::set_ambient_color(const rl_color_float& val)
{
    set_primitive_buffered_state_is_dirty(true);
    m_ambient_color = val;
}

// 0x004D3A30 (:367)
const rl_color_float& rl_state_manager::get_ambient_color() const
{
    return m_ambient_color;
}

// 0x004FF0E0 (:372)
void rl_state_manager::set_back_ambient_color(const rl_color_float& val)
{
    set_primitive_buffered_state_is_dirty(true);
    m_back_ambient_color = val;
}

// 0x004D3A40 (:378)
const rl_color_float& rl_state_manager::get_back_ambient_color() const
{
    return m_back_ambient_color;
}

// 0x004FF110 (:383)
void rl_state_manager::set_time_value(const vector4& time_value)
{
    set_primitive_buffered_state_is_dirty(true);
    m_time_values = time_value;
}

// 0x004D3A50 (:389)
const vector4& rl_state_manager::get_time_value() const
{
    return m_time_values;
}

// 0x004D3A60 (:394)
void rl_state_manager::set_render_offset(const vector& val)
{
    m_render_offset = val;
}

// 0x004D3A80 (:399)
const vector& rl_state_manager::get_render_offset() const
{
    return m_render_offset;
}

// 0x004D3A90 (:404)
const matrix43& rl_state_manager::get_current_local_world_transform() const
{
    return m_local_world_transform[m_local_world_stack_top];
}

// 0x004FF140 (:410)
// Resets the local world stack to this transform.
void rl_state_manager::set_world_transform(const matrix43& world_transform)
{
    set_primitive_buffered_state_is_dirty(true);
    m_local_world_transform[0] = world_transform;
    m_local_world_stack_top = 0;
}

// 0x004FF1B0 (:417)
// Pushes world_transform applied before the current transform. Magic number: the stack holds 16 transforms.
void rl_state_manager::push_world_transform(const matrix43& world_transform)
{
    set_primitive_buffered_state_is_dirty(true);
    if (m_local_world_stack_top + 1 < 16 && m_local_world_stack_top > -1)
    {
        m_local_world_transform[m_local_world_stack_top + 1].multiply_two(world_transform, m_local_world_transform[m_local_world_stack_top]);
        m_local_world_stack_top++;
    }
}

// 0x004FF1F0 (:426)
// Notes (decompiled behaviour, kept as is): there is no underflow check.
void rl_state_manager::pop_world_transform()
{
    set_primitive_buffered_state_is_dirty(true);
    m_local_world_stack_top--;
}

// 0x004D3AB0 (:433)
const matrix43& rl_state_manager::get_world_transform() const
{
    return m_local_world_transform[m_local_world_stack_top];
}

// 0x004D3AD0 (:438)
const matrix44& rl_state_manager::get_projection_transform() const
{
    return m_projection_transform;
}

// 0x004D3AE0 (:443)
const matrix44& rl_state_manager::get_view_transform() const
{
    return m_world_view_transform;
}

// 0x004D3AF0 (:448)
const matrix44& rl_state_manager::get_clip_screen_transform() const
{
    return m_clip_screen_transform;
}

// 0x004D3B00 (:453)
const matrix44& rl_state_manager::get_local_clip_transform() const
{
    return m_local_clip_transform;
}

// 0x004D3B10 (:458)
const matrix44& rl_state_manager::get_world_clip_transform() const
{
    return m_world_clip_transform;
}

// 0x004D3B20 (:463)
void rl_state_manager::set_target_dimensions(const unsigned int width, const unsigned int height)
{
    m_target_width = width;
    m_target_height = height;
}

// 0x004D3B40 (:469)
void rl_state_manager::get_target_dimensions(unsigned int& width, unsigned int& height)
{
    width = m_target_width;
    height = m_target_height;
}

// 0x004D3B60 (:475)
void rl_state_manager::set_fill_mode(const rl_fill_mode mode)
{
    m_fill_mode = mode;
}

// 0x004D3B70 (:480)
const rl_fill_mode rl_state_manager::get_fill_mode() const
{
    return m_fill_mode;
}

// 0x004D3B80 (:485)
void rl_state_manager::set_scissor_test_enabled(const bool enabled)
{
    m_flags = enabled ? (m_flags | kScissorEnabled) : (m_flags & ~kScissorEnabled);
}

// 0x004D3BA0 (:490)
const bool rl_state_manager::get_scissor_test_enabled() const
{
    return (m_flags & kScissorEnabled) != 0;
}

// 0x004FF200 (:495)
void rl_state_manager::set_scissor_region(const rl_rect& rect)
{
    set_primitive_buffered_state_is_dirty(true);
    m_scissor_region = rect;
}

// 0x004D3BB0 (:501)
const rl_rect& rl_state_manager::get_scissor_region() const
{
    return m_scissor_region;
}

// 0x004D3BC0 (:506)
void rl_state_manager::set_set_sampler_macro_states(const bool val)
{
    m_flags = val ? (m_flags | kSetSamplerMacroStates) : (m_flags & ~kSetSamplerMacroStates);
}

// 0x004D3BE0 (:511)
const bool rl_state_manager::get_set_sampler_macro_states() const
{
    return (m_flags & kSetSamplerMacroStates) != 0;
}

// 0x004D3BF0 (:516)
void rl_state_manager::set_msaa_enabled(const bool enabled)
{
    m_flags = enabled ? (m_flags | kMsaaEnabled) : (m_flags & ~kMsaaEnabled);
}

// 0x004D3C10 (:521)
const bool rl_state_manager::get_msaa_enabled() const
{
    return (m_flags & kMsaaEnabled) != 0;
}

// 0x004D3C20 (:526)
void rl_state_manager::set_zbias_mode(const rl_prim_zbias_mode mode)
{
    m_zbias_mode = mode;
}

// 0x004D3C30 (:531)
const rl_prim_zbias_mode rl_state_manager::get_zbias_mode() const
{
    return m_zbias_mode;
}

// 0x004D3C40 (:536)
void rl_state_manager::set_shadow_texture_transform(const matrix44& shadow_texture_transform)
{
    m_shadow_texture_transform = shadow_texture_transform;
}

// 0x004D3C60 (:541)
const matrix44& rl_state_manager::get_shadow_texture_transform() const
{
    return m_shadow_texture_transform;
}

// 0x004D3C70 (:546)
void rl_state_manager::set_primitive_buffered_state_is_dirty(const bool val)
{
    m_flags = val ? (m_flags | kIsPrimitiveStateDirty) : (m_flags & ~kIsPrimitiveStateDirty);
}

// 0x004D3C90 (:551)
const bool rl_state_manager::get_primitive_buffered_state_is_dirty() const
{
    return (m_flags & kIsPrimitiveStateDirty) == kIsPrimitiveStateDirty;
}

// Serialized primitive render state (0xDC bytes after the signature).
struct rl_serialized_primitive_render_state
{
    rl_rect scissor_region; // 0x00
    rl_color_float tint_color; // 0x10
    rl_color_float fade_color; // 0x20
    rl_color_float ambient_color; // 0x30
    rl_color_float back_ambient_color; // 0x40
    float saturation; // 0x50
    vector4 fog_parameters; // 0x54
    rl_color_float fog_color; // 0x64
    vector eye_position; // 0x74
    int primitive_texture; // 0x80
    int primitive_bink_y_texture; // 0x84
    int primitive_bink_cr_texture; // 0x88
    int primitive_bink_cb_texture; // 0x8C
    unsigned int primitive_material; // 0x90
    rl_color_float primitive_color; // 0x94
    rl_primitive_state macro_state; // 0xA4
    matrix43 world_transform; // 0xA8
    bool set_sampler_macro_states; // 0xD8
};

// 0x004FF230 (:556)
// Writes the signature and the state at offset (4-byte aligned against buffer). False when it does not fit.
// Notes (decompiled behaviour, kept as is): when buffer itself is misaligned, offset is overwritten with the padding
// instead of being padded, and the fit check counts 0xE0 bytes.
bool rl_state_manager::serialize_primitive_render_state(unsigned char* buffer, int& offset, int max_size) const
{
    if (((unsigned int)buffer & 3) != 0)
    {
        offset = 4 - ((unsigned int)buffer & 3);
    }
    if (max_size < offset + serialize_primitive_render_state_size())
    {
        return false;
    }
    *(unsigned int*)(buffer + offset) = Primitive_render_state_signature;
    offset += 4;
    rl_serialized_primitive_render_state* state_p = (rl_serialized_primitive_render_state*)(buffer + offset);
    state_p->scissor_region = m_scissor_region;
    state_p->tint_color = m_tint_color;
    state_p->fade_color = m_fade_color;
    state_p->ambient_color = m_ambient_color;
    state_p->back_ambient_color = m_back_ambient_color;
    state_p->saturation = m_saturation;
    state_p->fog_parameters = m_fog_parameters;
    state_p->fog_color = m_fog_color;
    state_p->eye_position = m_eye_position;
    state_p->primitive_texture = m_primitive_texture;
    state_p->primitive_bink_y_texture = m_primitive_bink_y_texture;
    state_p->primitive_bink_cr_texture = m_primitive_bink_cr_texture;
    state_p->primitive_bink_cb_texture = m_primitive_bink_cb_texture;
    state_p->primitive_material = m_primitive_material;
    state_p->primitive_color = m_primitive_color;
    state_p->macro_state = m_macro_state;
    state_p->world_transform = m_local_world_transform[m_local_world_stack_top];
    state_p->set_sampler_macro_states = (m_flags & kSetSamplerMacroStates) != 0;
    offset += Primitive_render_state_size;
    return true;
}

// 0x00546B00 (:602)
// Applies a state written by serialize_primitive_render_state when the signature matches.
void rl_state_manager::set_primitive_render_state_to_renderer(rl_renderer& renderer, unsigned char* buffer, int& offset, int max_size)
{
    if (*(unsigned int*)(buffer + offset) != Primitive_render_state_signature)
    {
        return;
    }
    offset += 4;
    rl_serialized_primitive_render_state* state_p = (rl_serialized_primitive_render_state*)(buffer + offset);
    renderer.set_scissor_region(state_p->scissor_region);
    renderer.set_tint_color(state_p->tint_color);
    renderer.set_fade_color(state_p->fade_color);
    renderer.set_ambient_color(state_p->ambient_color);
    renderer.set_back_ambient_color(state_p->back_ambient_color);
    renderer.set_saturation(state_p->saturation);
    renderer.set_fog_parameters(state_p->fog_parameters.x, state_p->fog_parameters.y, state_p->fog_parameters.z, state_p->fog_parameters.w);
    renderer.set_fog_color(state_p->fog_color);
    renderer.set_eye_position(state_p->eye_position);
    renderer.set_primitive_texture(state_p->primitive_texture);
    renderer.set_primitive_bink_textures(state_p->primitive_bink_y_texture, state_p->primitive_bink_cr_texture, state_p->primitive_bink_cb_texture);
    renderer.set_primitive_material(state_p->primitive_material);
    renderer.set_primitive_color(state_p->primitive_color);
    renderer.set_macro_state(state_p->macro_state, state_p->set_sampler_macro_states);
    renderer.set_world_transform(state_p->world_transform);
    offset += Primitive_render_state_size;
}

// 0x004D3CA0 (:630)
int rl_state_manager::serialize_primitive_render_state_size() const
{
    // Magic number: the signature and the state.
    return 0xE0;
}

// 0x004D3CB0 (:640)
void rl_state_manager::copy(const rl_state_manager& other)
{
    memcpy(this, &other, sizeof(rl_state_manager));
}

// 0x004C4E00
rl_material_state_store::rl_material_state_store()
{
}
