// Original: volition/render_lib/code/rl_skybox_renderable_instance.cpp
//
// rl_skybox_renderable_instance, the scene's skybox: up to four sky meshes drawn in order around the camera (the dome,
// then the cloud and mountain layers blended over it) with the orbitals (sun, moons, planets: camera-facing quads of
// the rfg-orbital shader), the star field and the meteor shower drawn before the part m_render_orbitals_before.

#include "volition/render_lib/code/rl_skybox_renderable_instance.h"

#include "volition/render_lib/code/rl_camera.h"
#include "volition/render_lib/code/rl_factory_rtti.h"
#include "volition/render_lib/code/rl_material_library.h"
#include "volition/render_lib/code/rl_meteor_shower.h"
#include "volition/render_lib/code/rl_renderer.h"
#include "volition/render_lib/code/rl_renderer_core.h"
#include "volition/render_lib/code/rl_shader.h"
#include "volition/render_lib/code/rl_shader_library.h"
#include "volition/render_lib/code/rl_star_field.h"
#include "volition/render_lib/code/rl_state_manager.h"
#include "volition/vlib/code/math/crc.h"
#include "volition/vlib/code/math/matrix.h"

// :12
// <rl_skybox_renderable_instance::factory_init> 0x00518F50 (:12)
// <rl_skybox_renderable_instance::create> 0x0054E160 (:12)
RL_FACTORY_CLASS_CORE(rl_skybox_renderable_instance, 0x44CD27A0, 2)

unsigned int rl_skybox_renderable_instance::m_orbital_shader = 0xFFFFFFFF; // 0x014FA5F8
rl_shader_const_ref rl_skybox_renderable_instance::m_orbtial_map_ref; // 0x01516380
rl_shader_const_ref rl_skybox_renderable_instance::m_orbital_tint_ref; // 0x01516384
bool rl_skybox_renderable_instance::initialized = false; // 0x01B7BD35
bool RL_skybox_star_field = true; // 0x014FA54E
bool RL_skybox_meteor_showers = true; // 0x014FA54F
vector RL_skybox_ambient; // 0x01B7BD70, the ambient color of the sky meshes (black)

// size 0x18: the vertex of an orbital quad (RLVF_PRIMITIVE_3D).
struct rl_orbital_vertex
{
    vector pos; // 0x00
    float u; // 0x0C
    float v; // 0x10
    unsigned int color; // 0x14
};

// 0x004D16A0
rl_skybox_orbital_params::rl_skybox_orbital_params()
{
}

// 0x00531370 (:12)
void rl_skybox_renderable_instance::release()
{
    if (rl_is_locked())
    {
        rl_add_deferred_op(RLDO_RELEASE_OBJECT, this);
        return;
    }
    if (ms_object_pool.size() < 1 && ms_class_factory_info.get_display_exhausted_factories_warning())
    {
        rl_class_factory_info::decrement_num_exhausted_factories();
    }
    ms_object_pool.add(this);
    this->~rl_skybox_renderable_instance();
}

// 0x005465B0 (:81)
rl_skybox_renderable_instance::rl_skybox_renderable_instance()
{
    m_num_skybox_parts = 0;
    m_skybox_parts[0] = nullptr;
    m_skybox_parts[1] = nullptr;
    m_skybox_parts[2] = nullptr;
    m_skybox_parts[3] = nullptr;
    m_num_orbitals = 0;
    m_render_orbitals_before = 0;
}

// 0x005465F0 (:88)
rl_skybox_renderable_instance::~rl_skybox_renderable_instance()
{
    m_num_skybox_parts = 0;
    m_skybox_parts[0] = nullptr;
    m_skybox_parts[1] = nullptr;
    m_skybox_parts[2] = nullptr;
    m_skybox_parts[3] = nullptr;
}

// The star field and the meteor shower (inlined three times in render, after the orbitals).
static void render_sky_objects(rl_renderer& renderer, rl_camera& camera)
{
    if (RL_skybox_star_field)
    {
        rl_star_field::get_instance()->render(renderer, camera);
    }
    if (RL_skybox_meteor_showers)
    {
        rl_meteor_shower::get_instance()->render(renderer, camera);
    }
}

// 0x00546620 (:93): in the base pass, with the sky's ambient (black), depth read only and RGB writes. The parts after
// the first are blended over it (premultiplied filter); those from m_render_orbitals_before on also drop the stencil
// mode.
void rl_skybox_renderable_instance::render(rl_renderer& renderer, rl_camera& camera, unsigned int flags)
{
    const rl_primitive_state old_state = renderer.get_macro_state();
    rl_primitive_state state = old_state;
    state.set_zbuf_mode(RLPZM_READ);
    state.set_color_write_mode(RLPCW_RGB);
    state.set_zbias_mode(RLPZB_NONE);
    renderer.set_macro_state(state, false);
    const rl_color_float old_ambient = renderer.get_ambient_color();
    renderer.set_ambient_color(rl_color_float(RL_skybox_ambient.x, RL_skybox_ambient.y, RL_skybox_ambient.z, 1.0f));
    const bool was_base_pass = renderer.get_is_base_pass();
    renderer.set_is_base_pass(true);

    if (m_num_skybox_parts == 0)
    {
        render_orbital_objects(renderer, camera);
        render_sky_objects(renderer, camera);
    }
    for (int i = 0; i < m_num_skybox_parts; i++)
    {
        if (i == m_render_orbitals_before)
        {
            render_orbital_objects(renderer, camera);
            render_sky_objects(renderer, camera);
        }
        rl_renderable_instance* part_p = m_skybox_parts[i];
        if (part_p == nullptr)
        {
            continue;
        }
        if (i > 0)
        {
            state.set_alpha_mode(RLPAM_FILTER_PREMULT_ALPHA);
            if (m_render_orbitals_before <= i)
            {
                state.set_stencil_mode(RLPSM_NONE);
            }
            renderer.set_macro_state(state, false);
        }
        part_p->set_position(get_position());
        part_p->set_rotation(IDENTITY_MATRIX);
        part_p->render(renderer, camera, flags);
        if (i > 0)
        {
            state.set_alpha_mode(RLPAM_NONE);
            renderer.set_macro_state(state, false);
        }
    }
    if (m_num_skybox_parts <= m_render_orbitals_before)
    {
        render_orbital_objects(renderer, camera);
        render_sky_objects(renderer, camera);
    }

    renderer.set_is_base_pass(was_base_pass);
    renderer.set_ambient_color(old_ambient);
    renderer.set_macro_state(old_state, false);
}

// 0x004D16B0 (:183)
bool rl_skybox_renderable_instance::add_subpart(rl_renderable_instance* new_subpart_p)
{
    if (m_num_skybox_parts == 4)
    {
        return false;
    }
    m_skybox_parts[m_num_skybox_parts] = new_subpart_p;
    m_num_skybox_parts++;
    return true;
}

// 0x004D16E0 (:195)
void rl_skybox_renderable_instance::clear_subparts()
{
    m_num_skybox_parts = 0;
    m_skybox_parts[0] = nullptr;
    m_skybox_parts[1] = nullptr;
    m_skybox_parts[2] = nullptr;
    m_skybox_parts[3] = nullptr;
}

// 0x005313F0 (:204): a camera-facing quad of 2 * scale at the orbital's position around the skybox, alpha blended
// (filter) or additive, depth read only, the stencil clear mode, no culling.
void rl_skybox_renderable_instance::render_orbital_object(rl_renderer& renderer, rl_camera& camera, const rl_skybox_orbital_params& orbital_params)
{
    if (!initialized)
    {
        return;
    }
    renderer.set_world_transform(IDENTITY_MATRIX43);
    const vector& sky_pos = get_position();
    const vector center(orbital_params.position.x + sky_pos.x, orbital_params.position.y + sky_pos.y, orbital_params.position.z + sky_pos.z);
    const matrix& camera_orient = camera.get_rotation();
    const vector down(-camera_orient.uvec.x, -camera_orient.uvec.y, -camera_orient.uvec.z);
    const vector& right = camera_orient.rvec;
    const rl_primitive_state old_state = renderer.get_macro_state();
    rl_primitive_state state = old_state;
    state.set_alpha_mode(orbital_params.alpha_blend ? RLPAM_FILTER : RLPAM_ALPHA_ADDITIVE);
    state.set_clamp_mode(RLPCM_WRAP);
    state.set_zbuf_mode(RLPZM_READ);
    state.set_stencil_mode(RLPSM_CLEAR);
    state.set_cull_mode(RLPCM_NONE);
    renderer.set_macro_state(state, false);
    renderer.set_shader_type(shader_type_standard);
    renderer.set_shader_configuration(0);
    renderer.set_active_shader(m_orbital_shader, rl_shader_technique_base, -1);
    renderer.set_texture(m_orbtial_map_ref.get_ref(), orbital_params.texture_handle);
    renderer.set_pixel_shader_constant(m_orbital_tint_ref.get_ref(), &orbital_params.tint.red, 4);
    renderer.set_primitive_material(0xFFFFFFFF);
    renderer.set_vertex_format(RLVF_PRIMITIVE_3D);
    rl_orbital_vertex* vertex = (rl_orbital_vertex*)renderer.begin_dynamic(RL_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP, 2, sizeof(rl_orbital_vertex));
    // The four corners (right, down) = (-s, s), (s, s), (-s, -s), (s, -s) with uvs (0, 1), (1, 1), (0, 0), (1, 0).
    const float corners[4][4] = { { -1.0f, 1.0f, 0.0f, 1.0f }, { 1.0f, 1.0f, 1.0f, 1.0f }, { -1.0f, -1.0f, 0.0f, 0.0f }, { 1.0f, -1.0f, 1.0f, 0.0f } };
    for (int i = 0; i < 4; i++)
    {
        const float r = corners[i][0] * orbital_params.scale;
        const float d = corners[i][1] * orbital_params.scale;
        vertex[i].pos.x = r * right.x + center.x + down.x * d;
        vertex[i].pos.y = right.y * r + center.y + down.y * d;
        vertex[i].pos.z = right.z * r + center.z + down.z * d;
        vertex[i].u = corners[i][2];
        vertex[i].v = corners[i][3];
        vertex[i].color = rl_color::WHITE.uint_color;
    }
    renderer.end_dynamic();
    renderer.set_macro_state(old_state, false);
}

// 0x00531B30 (:268)
void rl_skybox_renderable_instance::render_orbital_objects(rl_renderer& renderer, rl_camera& camera)
{
    const rl_primitive_state old_state = renderer.get_macro_state();
    for (int i = 0; i < m_num_orbitals; i++)
    {
        render_orbital_object(renderer, camera, m_orbital_params[i]);
    }
    renderer.set_macro_state(old_state, false);
}

// 0x00519050 (:281)
// Looks up the orbital (planets and moons) shader once; a missing shader is retried on the next call.
void rl_skybox_renderable_instance::initialize()
{
    if (initialized)
    {
        return;
    }
    rl_shader_library* library_p = rl_shader_library::get_instance();
    m_orbital_shader = library_p->lookup("rfg-orbital");
    if (m_orbital_shader == 0xFFFFFFFF)
    {
        return;
    }
    m_orbtial_map_ref.set_ref(library_p->get_shader_tmu_index(m_orbital_shader, crc_stri("orbital_map", 0)));
    m_orbital_tint_ref.set_ref(library_p->get_shader_const_index(m_orbital_shader, crc_stri("orbital_tint", 0)));
    initialized = true;
}

// 0x004D1710 (:294)
void rl_skybox_renderable_instance::clear_orbitals()
{
    m_num_orbitals = 0;
}

// 0x004D1720 (:299): at most 15.
void rl_skybox_renderable_instance::add_orbital(const rl_skybox_orbital_params& orbital_params)
{
    if (m_num_orbitals < 15)
    {
        m_orbital_params[m_num_orbitals] = orbital_params;
        m_num_orbitals++;
    }
}
