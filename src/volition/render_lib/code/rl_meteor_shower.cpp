// Original: volition/render_lib/code/rl_meteor_shower.cpp
//
// rl_meteor_shower, the meteors of the night sky: 16 lines emitted on a random clock and drawn around the camera with
// the time-of-day strength.

#include "volition/render_lib/code/rl_meteor_shower.h"

#include "volition/render_lib/code/rl_camera.h"
#include "volition/render_lib/code/rl_factory_rtti.h"
#include "volition/render_lib/code/rl_renderer.h"
#include "volition/render_lib/code/rl_state_manager.h"
#include "volition/render_lib/code/rl_shader_library.h"
#include "volition/vlib/code/math/crc.h"
#include "volition/vlib/code/math/matrix.h"
#include "volition/vlib/code/math/rand.h"

#include <math.h>
#include "keen/base/trace.hpp"

// :65
// <rl_meteor_shower::release> 0x004C31E0 (:65)
// <rl_meteor_shower::create> 0x00512810 (:65)
RL_DYNAMIC_CLASS_IMPLEMENTATION(rl_meteor_shower)

// 0x004C3200
rl_meteor::rl_meteor()
{
}

// 0x004E9910 (:70)
// Magic numbers: a meteor every 2-12 s, living 2.5-5 s at 100-200 units/s.
rl_meteor_shower::rl_meteor_shower()
{
    m_emission_delay_rand = 10.0f;
    m_emission_lifespan = 2.5f;
    m_emission_lifespan_rand = 2.5f;
    m_meteor_strength = 1.0f;
    m_emission_speed = 100.0f;
    m_emission_speed_rand = 100.0f;
    m_meteor_shader = 0xFFFFFFFF;
    m_emission_delay = 2.0f;
    m_last_update_time = 0.0f;
    m_emission_time = 0.0f;
    m_next_emission = 2.0f;
}

// 0x004E99D0 (:88)
rl_meteor_shower::~rl_meteor_shower()
{
}

// 0x005291C0 (:94)
// Looks up the meteor shader and links the 16 meteors into the free list.
void rl_meteor_shower::initialize()
{
    rl_shader_library* library_p = rl_shader_library::get_instance();
    m_meteor_shader = library_p->lookup("rfg-skybox-meteors");
    if (m_meteor_shader == 0xFFFFFFFF)
    {
        keen::debug::nativePrintArgs("Could not find meteor shower shader!");
        return;
    }
    m_line_tech.set_ref(library_p->get_shader_user_technique_index(m_meteor_shader, crc_stri("line", 0)));
    m_meteor_strength_param.set_ref(library_p->get_shader_const_index(m_meteor_shader, crc_stri("Meteor_strength", 0)));
    m_last_update_time = 0.0f;
    m_first_free = 0;
    // Magic number: 16 meteors.
    for (int i = 0; i < 16; i++)
    {
        m_meteor[i].m_spawn_time = 0.0f;
        m_meteor[i].m_lifespan = -1.0f;
        m_meteor[i].m_next_free = i + 1;
    }
    m_meteor[15].m_next_free = -1;
}

// 0x004C3210 (:124)
void rl_meteor_shower::shutdown()
{
}

// 0x004C3150 (:42, LTCG): the two ends of a meteor's line (the shader moves them by the velocity over its life).
static int fill_meteor_vb(rl_meteor_line_vertex* vertex, const vector& pos, const vector& vel, float spawn_time, float lifespan)
{
    for (int i = 0; i < 2; i++)
    {
        vertex[i].m_pos = pos;
        vertex[i].m_end = (float)i;
        vertex[i].m_vel = vel;
        vertex[i].m_spawn_time = spawn_time;
        vertex[i].m_lifespan = lifespan;
    }
    return 2;
}

// 0x004C3220 (:130)
void rl_meteor_shower::set_strength(float meteor_strength)
{
    m_meteor_strength = meteor_strength;
}

// 0x00512880 (:137): frees the meteors past their lifespan and emits new ones on the emission clock. A meteor starts
// at a random point of a circle (a quarter of the far clip distance wide, 200 units up) and flies at a random speed
// toward the far side of the far clip circle, within 30 degrees of straight across. The randoms come from the global
// fl_rand, as in the original (so the sky draws from the same generator as the game).
void rl_meteor_shower::update(rl_renderer& renderer)
{
    const float time = renderer.get_time_value().x;
    float delta = time - m_last_update_time;
    // Magic number: at most half a second per update.
    if (0.0f <= delta - 0.5f)
    {
        delta = 0.5f;
    }
    for (int i = 0; i < 16; i++)
    {
        rl_meteor& meteor = m_meteor[i];
        if (0.0f < meteor.m_lifespan && meteor.m_lifespan < time - meteor.m_spawn_time)
        {
            meteor.m_lifespan = -1.0f;
            meteor.m_next_free = m_first_free;
            m_first_free = i;
        }
    }
    const float far_clip = renderer.get_active_camera()->get_far_clip();
    m_emission_time = m_emission_time + delta;
    if (m_emission_time <= m_next_emission)
    {
        m_last_update_time = time;
        return;
    }
    do
    {
        if (m_first_free != -1)
        {
            // Magic numbers: 2 pi; the target is opposite the start (pi) within +-0.5236 (30 degrees).
            const float start_angle = fl_rand() * 6.2831855f;
            const float target_angle = (fl_rand() * 2.0f - 1.0f) * 0.5235988f + (start_angle + 3.1415927f);
            const float speed = fl_rand() * m_emission_speed_rand + m_emission_speed;
            rl_meteor& meteor = m_meteor[m_first_free];
            meteor.m_pos.x = cosf(start_angle) * far_clip * 0.25f;
            meteor.m_pos.y = 200.0f;
            meteor.m_pos.z = sinf(start_angle) * far_clip * 0.25f;
            meteor.m_vel.x = cosf(target_angle) * far_clip;
            meteor.m_vel.y = meteor.m_pos.y;
            meteor.m_vel.z = sinf(target_angle) * far_clip;
            meteor.m_vel.x = meteor.m_vel.x - meteor.m_pos.x;
            meteor.m_vel.y = meteor.m_vel.y - meteor.m_pos.y;
            meteor.m_vel.z = meteor.m_vel.z - meteor.m_pos.z;
            meteor.m_vel.normalize();
            meteor.m_vel.x = meteor.m_vel.x * speed;
            meteor.m_vel.y = meteor.m_vel.y * speed;
            meteor.m_vel.z = meteor.m_vel.z * speed;
            meteor.m_spawn_time = time;
            meteor.m_lifespan = fl_rand() * m_emission_lifespan_rand + m_emission_lifespan;
            m_first_free = meteor.m_next_free;
        }
        const float elapsed = m_next_emission;
        m_next_emission = m_emission_delay;
        m_emission_time = m_emission_time - elapsed;
        m_next_emission = fl_rand() * m_emission_delay_rand + m_next_emission;
    } while (m_next_emission < m_emission_time);
    m_last_update_time = time;
}

// 0x004C3240 (:199): a line per meteor, free or not (a free meteor's lifespan is -1, which the shader hides).
// Notes (decompiled behaviour, kept as is): returns 0, not the vertex count; render ignores the result.
int rl_meteor_shower::fill_meteor_line_vb(rl_meteor_line_vertex* vertex)
{
    for (int i = 0; i < 16; i++)
    {
        vertex += fill_meteor_vb(vertex, m_meteor[i].m_pos, m_meteor[i].m_vel, m_meteor[i].m_spawn_time, m_meteor[i].m_lifespan);
    }
    return 0;
}

// 0x0053F8C0 (:210)
void rl_meteor_shower::render(rl_renderer& renderer, rl_camera& camera)
{
    if (m_meteor_strength == 0.0f)
    {
        return;
    }
    update(renderer);
    const rl_primitive_state old_state = renderer.get_macro_state();
    renderer.set_tint_color(rl_color_float(1.0f, 1.0f, 1.0f, 1.0f));
    matrix43 transform;
    transform.set_orientation(IDENTITY_MATRIX);
    transform.set_translation(renderer.get_active_camera()->get_position());
    renderer.set_world_transform(transform);
    renderer.set_eye_orientation(camera.get_rotation());
    rl_primitive_state state = old_state;
    state.set_alpha_mode(RLPAM_ALPHA_ADDITIVE);
    state.set_zbuf_mode(RLPZM_READ);
    renderer.set_pixel_shader_constant(m_meteor_strength_param.get_ref(), &m_meteor_strength, 1);
    renderer.set_macro_state(state, false);
    renderer.set_shader_type(shader_type_generic);
    renderer.set_shader_configuration(0);
    renderer.set_active_shader(m_meteor_shader, rl_shader_technique_user, m_line_tech.get_ref());
    renderer.set_vertex_format(RLVF_METEOR_SHOWER_LINE);
    // Magic numbers: 16 lines of 0x24-byte vertices.
    rl_meteor_line_vertex* vertex = (rl_meteor_line_vertex*)renderer.begin_dynamic(RL_PRIMITIVE_TOPOLOGY_LINELIST, 16, sizeof(rl_meteor_line_vertex));
    if (vertex == nullptr)
    {
        renderer.set_macro_state(old_state, false);
        return;
    }
    fill_meteor_line_vb(vertex);
    renderer.end_dynamic();
    renderer.set_macro_state(old_state, false);
}
