// Original: volition/render_lib/code/rl_state_manager.h
// Implementation: volition/render_lib/code/rl_state_manager.cpp
//
// rl_state_manager, tracks the material/primitive state of a renderer (colors, textures, fog, lighting, shader
// constants) and the material state store. Placement of rl_material_state_store inferred.
#pragma once

#include "volition/render_lib/code/rl_color.h"
#include "volition/render_lib/code/rl_device.h"
#include "volition/render_lib/code/rl_primitive_defines.h"
#include "volition/render_lib/code/rl_primitive_state.h"
#include "volition/render_lib/code/rl_rect.h"
#include "volition/render_lib/code/rl_shader_defines.h"
#include "volition/render_lib/code/rl_state_defines.h"
#include "volition/vlib/code/math/matrix.h"
#include "volition/vlib/code/math/vector3.h"
#include "volition/vlib/code/math/vector4.h"

class rl_camera;
class rl_renderer;
class rl_screen_info;
class rl_shader;

// size 0x5C0
class rl_state_manager
{
    // Access inferred from rl_primitive_renderer::record_draw_command.
    friend class rl_primitive_renderer;
public:
    rl_state_manager(); // 0x004D2170 (:51)
    ~rl_state_manager(); // 0x004D2280 (:58)
    void operator=(const rl_state_manager& other); // 0x004D2290 (:63)
    void set_primitive_color(const rl_color_float& color); // 0x004FEF50 (:68)
    const rl_color_float& get_primitive_color() const; // 0x004D22B0 (:74)
    void set_primitive_texture(const int tex); // 0x00531B90 (:79)
    const int get_primitive_texture() const; // 0x004D22C0 (:91)
    void set_primitive_material(const unsigned int mat); // 0x00519670 (:96)
    const unsigned int get_primitive_material() const; // 0x004D22D0 (:108)
    void set_primitive_bink_textures(const int y, const int cr, const int cb); // 0x004FEF70 (:113)
    void get_primitive_bink_textures(int& y, int& cr, int& cb) const; // 0x004D22E0 (:125)
    void set_vertex_format(rl_dev_vertex_format id); // 0x004D2300 (:132)
    const rl_dev_vertex_format get_vertex_format() const; // 0x004D2310 (:137)
    void set_shader_type(const rl_shader_type type); // 0x004D2320 (:142)
    const rl_shader_type get_shader_type() const; // 0x004D2330 (:147)
    void set_shader_configuration(const int config); // 0x004D2340 (:152)
    const int get_shader_configuration() const; // 0x004D2350 (:157)
    void set_shader(rl_shader* const shader_p, const int tech); // 0x004D2360 (:162)
    void get_shader(rl_shader*& shader_p, int& tech) const; // 0x004D2380 (:168)
    void set_macro_state(const rl_primitive_state& state); // 0x004FEFB0 (:174)
    const rl_primitive_state& get_macro_state() const; // 0x004D23A0 (:180)
    const bool get_depth_only() const; // 0x004D23F0 (:205)
    void set_depth_only(const bool depth_only); // 0x004D2400 (:210)
    const bool get_depth_requires_pixel_shader() const; // 0x004D2420 (:215)
    void set_depth_requires_pixel_shader(const bool depth_required); // 0x004D2430 (:220)
    const bool get_is_base_pass() const; // 0x004D2450 (:225)
    void set_is_base_pass(const bool is_base_pass); // 0x004D2460 (:230)
    const rl_color_float& get_fade_color() const; // 0x004D2480 (:235)
    void set_fade_color(const rl_color_float& color); // 0x004FEFC0 (:240)
    const rl_color_float& get_tint_color() const; // 0x004D2490 (:246)
    void set_tint_color(const rl_color_float& color); // 0x004FEFE0 (:251)
    const float get_saturation() const; // 0x004D24A0 (:257)
    void set_saturation(const float sat); // 0x004FF000 (:262)
    rl_camera* const get_active_camera() const; // 0x004D24B0 (:268)
    void set_active_camera(rl_camera* camera_p); // 0x004D24C0 (:273)
    void set_screen_info(const rl_screen_info& screen_info); // 0x004D38E0 (:309)
    void set_eye_position(const vector& eye_pos); // 0x004FF020 (:322)
    const vector& get_eye_position() const; // 0x004D39D0 (:328)
    void set_fog_parameters(float height_density, float atmosphere_scale, float density, float density_offset); // 0x004FF040 (:333)
    void get_fog_parameters(float& height_density, float& atmosphere_scale, float& density, float& density_offset) const; // 0x004D39E0 (:342)
    void set_fog_color(const rl_color_float& fog_color); // 0x004FF080 (:350)
    const rl_color_float& get_fog_color() const; // 0x004D3A20 (:356)
    void set_ambient_color(const rl_color_float& val); // 0x004FF0B0 (:361)
    const rl_color_float& get_ambient_color() const; // 0x004D3A30 (:367)
    void set_back_ambient_color(const rl_color_float& val); // 0x004FF0E0 (:372)
    const rl_color_float& get_back_ambient_color() const; // 0x004D3A40 (:378)
    void set_time_value(const vector4& time_value); // 0x004FF110 (:383)
    const vector4& get_time_value() const; // 0x004D3A50 (:389)
    void calculate_utility_transformations(); // 0x004D24D0 (:278)
    void set_world_transform(const matrix43& world_transform); // 0x004FF140 (:410)
    void get_3d_transforms(matrix44& world_view_transform, matrix44& projection_transform); // 0x004D38B0 (:302)
    void set_3d_transforms(const matrix44& world_view_transform, const matrix44& projection_transform); // 0x004D3870 (:292)
    const matrix43& get_current_local_world_transform() const; // 0x004D3A90 (:404)
    const matrix43& get_world_transform() const; // 0x004D3AB0 (:433)
    const matrix44& get_projection_transform() const; // 0x004D3AD0 (:438)
    const matrix44& get_view_transform() const; // 0x004D3AE0 (:443)
    const matrix44& get_clip_screen_transform() const; // 0x004D3AF0 (:448)
    const matrix44& get_local_clip_transform() const; // 0x004D3B00 (:453)
    const matrix44& get_world_clip_transform() const; // 0x004D3B10 (:458)
    void set_shadow_texture_transform(const matrix44& shadow_texture_transform); // 0x004D3C40 (:536)
    const matrix44& get_shadow_texture_transform() const; // 0x004D3C60 (:541)
    void push_world_transform(const matrix43& world_transform); // 0x004FF1B0 (:417)
    void pop_world_transform(); // 0x004FF1F0 (:426)
    void set_target_dimensions(const unsigned int width, const unsigned int height); // 0x004D3B20 (:463)
    void get_target_dimensions(unsigned int& width, unsigned int& height); // 0x004D3B40 (:469)
    void set_render_offset(const vector& val); // 0x004D3A60 (:394)
    const vector& get_render_offset() const; // 0x004D3A80 (:399)
    void set_cull_mode(const rl_cull_mode mode); // 0x004D23B0 (:185)
    const rl_cull_mode get_cull_mode() const; // 0x004D23C0 (:190)
    void set_shader_cull_mode(const rl_cull_mode mode); // 0x004D23D0 (:195)
    const rl_cull_mode get_shader_cull_mode() const; // 0x004D23E0 (:200)
    void set_fill_mode(const rl_fill_mode mode); // 0x004D3B60 (:475)
    const rl_fill_mode get_fill_mode() const; // 0x004D3B70 (:480)
    void set_scissor_region(const rl_rect& rect); // 0x004FF200 (:495)
    const rl_rect& get_scissor_region() const; // 0x004D3BB0 (:501)
    void set_scissor_test_enabled(const bool enabled); // 0x004D3B80 (:485)
    const bool get_scissor_test_enabled() const; // 0x004D3BA0 (:490)
    void set_msaa_enabled(const bool enabled); // 0x004D3BF0 (:516)
    const bool get_msaa_enabled() const; // 0x004D3C10 (:521)
    void set_zbias_mode(const rl_prim_zbias_mode mode); // 0x004D3C20 (:526)
    const rl_prim_zbias_mode get_zbias_mode() const; // 0x004D3C30 (:531)
    void set_set_sampler_macro_states(const bool val); // 0x004D3BC0 (:506)
    const bool get_set_sampler_macro_states() const; // 0x004D3BE0 (:511)
    void set_primitive_buffered_state_is_dirty(const bool val); // 0x004D3C70 (:546)
    const bool get_primitive_buffered_state_is_dirty() const; // 0x004D3C90 (:551)
    bool serialize_primitive_render_state(unsigned char* buffer, int& offset, int max_size) const; // 0x004FF230 (:556)
    static void set_primitive_render_state_to_renderer(rl_renderer& renderer, unsigned char* buffer, int& offset, int max_size); // 0x00546B00 (:602)
    int serialize_primitive_render_state_size() const; // 0x004D3CA0 (:630)
    void copy(const rl_state_manager& other); // 0x004D3CB0 (:640)

private:
    static const unsigned int kMaxPixelShaderTextures; // 0x012B8040
    static const unsigned int kSetSamplerMacroStates; // 0x012B8044
    static const unsigned int kDepthOnly; // 0x012B8048
    static const unsigned int kDepthRequiresPixelShader; // 0x012B804C
    static const unsigned int kIsBasePass; // 0x012B8050
    static const unsigned int kScissorEnabled; // 0x012B8054
    static const unsigned int kMsaaEnabled; // 0x012B8058
    static const unsigned int kIsPrimitiveStateDirty; // 0x012B805C
    unsigned int m_flags; // 0x00
    rl_color_float m_primitive_color; // 0x04
    int m_primitive_texture; // 0x14
    int m_primitive_bink_y_texture; // 0x18
    int m_primitive_bink_cr_texture; // 0x1C
    int m_primitive_bink_cb_texture; // 0x20
    unsigned int m_primitive_material; // 0x24
    rl_dev_vertex_format m_vertex_format; // 0x28
    rl_shader_type m_shader_type; // 0x2C
    int m_shader_configuration; // 0x30
    int m_shader_technique; // 0x34
    rl_shader* m_shader_p; // 0x38
    rl_primitive_state m_macro_state; // 0x3C
    rl_cull_mode m_cull_mode; // 0x40
    rl_cull_mode m_shader_cull_mode; // 0x44
    rl_fill_mode m_fill_mode; // 0x48
    rl_color_float m_fade_color; // 0x4C
    rl_color_float m_tint_color; // 0x5C
    float m_saturation; // 0x6C
    rl_camera* m_active_camera_p; // 0x70
    vector m_eye_position; // 0x74
    vector m_render_offset; // 0x80
    rl_color_float m_fog_color; // 0x8C
    rl_color_float m_ambient_color; // 0x9C
    rl_color_float m_back_ambient_color; // 0xAC
    vector4 m_fog_parameters; // 0xC0
    vector4 m_time_values; // 0xD0
    unsigned int m_target_width; // 0xE0
    unsigned int m_target_height; // 0xE4
    rl_prim_zbias_mode m_zbias_mode; // 0xE8
    rl_rect m_scissor_region; // 0xEC
    int m_local_world_stack_top; // 0xFC
    matrix43 m_local_world_transform[16]; // 0x100
    matrix44 m_world_view_transform; // 0x400
    matrix44 m_projection_transform; // 0x440
    matrix44 m_clip_screen_transform; // 0x480
    matrix44 m_world_clip_transform; // 0x4C0
    matrix44 m_world_screen_transform; // 0x500
    matrix44 m_local_clip_transform; // 0x540
    matrix44 m_shadow_texture_transform; // 0x580
};

// size 0x24
struct rl_material_state_store
{
    rl_material_state_store(); // 0x004C4E00

    rl_primitive_state old_state[8]; // 0x00
    rl_primitive_state implicit_state; // 0x20
};
