// Original: volition/render_lib/code/rl_renderable_particles.cpp
//
// A particle system renderable (VFX.md FX2): the render type, particle material and lighting of one vfxlib emitter,
// shared by its instances. The functions of rl_renderable_particles.h are here too (the original has them inline in
// the header).

#include "volition/render_lib/code/rl_renderable_particles.h"

#include "volition/render_lib/code/rl_factory_rtti.h"
#include "volition/render_lib/code/rl_particle_material.h"
#include "volition/vlib/code/math/matrix.h"
#include "volition/vlib/code/memory/vmemory.h"

// The class factory (:6): class id 0x53A86A0C, 2048 renderables.
// <rl_renderable_particles::factory_init> 0x00515820 (:6)
// <rl_renderable_particles::release> 0x00515910 (:6)
// <rl_renderable_particles::create> 0x0052CEB0 (:6)
RL_FACTORY_CLASS_IMPLEMENTATION(rl_renderable_particles, 0x53A86A0C, 2048)

// 0x004C4BF0 (:9): a billboard with rotation enabled.
rl_renderable_particles::rl_renderable_particles()
{
    m_effect_material_percent = 0.0f;
    m_ambient_color.red = 1.0f;
    m_ambient_color.green = 1.0f;
    m_self_illum = 1.0f;
    m_diffuse_color = rl_color_float(1.0f, 1.0f, 1.0f, 1.0f);
    m_particle_type = rl_particle_base::RENDER_TYPE_BILLBOARD;
    m_material_p = nullptr;
    m_flags = FLAGS_ROTATION_ENABLED;
    m_tail_length = 0;
    m_is_iterative = false;
    m_texture_anim_random_frames = false;
    m_color_correction_matrix_p = nullptr;
    m_ambient_color.blue = 1.0f;
    m_ambient_color.alpha = 1.0f;
    // Magic number: the default soft particle depth range.
    m_soft_thickness = 0.8f;
}

// 0x004C4CA0 (:25)
rl_renderable_particles::~rl_renderable_particles()
{
}

// 0x004C4CB0 (:30): identity unless color correction is enabled.
const matrix* rl_renderable_particles::get_color_correction_matrix() const
{
    if ((m_flags & FLAGS_IS_COLOR_CORRECTION_ENABLED) != 0)
    {
        return m_color_correction_matrix_p;
    }
    return &IDENTITY_MATRIX;
}

// 0x004C4CC0 (:39)
matrix* rl_renderable_particles::get_color_correction_matrix_data() const
{
    if ((m_flags & FLAGS_IS_COLOR_CORRECTION_ENABLED) != 0)
    {
        return m_color_correction_matrix_p;
    }
    return nullptr;
}

// 0x004C4CD0 (:49): ignored unless color correction is enabled.
void rl_renderable_particles::set_color_correction_matrix(matrix* color_correction_matrix_p)
{
    if ((m_flags & FLAGS_IS_COLOR_CORRECTION_ENABLED) != 0)
    {
        m_color_correction_matrix_p = color_correction_matrix_p;
    }
}

// 0x004C4CE0 (:58): a flag of the material.
bool rl_renderable_particles::get_is_always_on_top() const
{
    return (m_material_p->flags & rl_particle_material_data::MATERIAL_FLAG_ALWAYS_ON_TOP) != 0;
}

// 0x004C4CF0 (:63)
void rl_renderable_particles::set_is_always_on_top(bool is_always_on_top)
{
    if (is_always_on_top)
    {
        m_material_p->flags |= rl_particle_material_data::MATERIAL_FLAG_ALWAYS_ON_TOP;
        return;
    }
    m_material_p->flags &= ~rl_particle_material_data::MATERIAL_FLAG_ALWAYS_ON_TOP;
}

// 0x004C4D10 (:73): a flag of the material.
bool rl_renderable_particles::get_is_soft_render() const
{
    return (m_material_p->flags & rl_particle_material_data::MATERIAL_FLAG_SOFT_RENDER) != 0;
}

// 0x004C4D20 (:78)
void rl_renderable_particles::set_is_soft_render(bool is_soft_render)
{
    if (is_soft_render)
    {
        m_material_p->flags |= rl_particle_material_data::MATERIAL_FLAG_SOFT_RENDER;
        return;
    }
    m_material_p->flags &= ~rl_particle_material_data::MATERIAL_FLAG_SOFT_RENDER;
}

// 0x004BD4B0 (.h:112)
rl_particle_material_data* rl_renderable_particles::get_material() const
{
    return m_material_p;
}

// 0x00C9E9A0 (.h:117)
void rl_renderable_particles::set_material(rl_particle_material_data* material_p)
{
    m_material_p = material_p;
}

// 0x004BD4C0 (.h:122)
rl_particle_base::render_type rl_renderable_particles::get_particle_type() const
{
    return m_particle_type;
}

// 0x00C9E9B0 (.h:127)
void rl_renderable_particles::set_particle_type(rl_particle_base::render_type particle_type)
{
    m_particle_type = particle_type;
}

// 0x004BD4D0 (.h:132)
bool rl_renderable_particles::get_is_rotation_enabled() const
{
    return (m_flags & FLAGS_ROTATION_ENABLED) != 0;
}

// 0x00C9E9C0 (.h:137)
void rl_renderable_particles::set_is_rotation_enabled(bool is_rotation_enabled)
{
    if (is_rotation_enabled)
    {
        m_flags |= FLAGS_ROTATION_ENABLED;
        return;
    }
    m_flags &= ~FLAGS_ROTATION_ENABLED;
}

// 0x004BD4E0 (.h:147)
bool rl_renderable_particles::get_local_space_emission() const
{
    return (m_flags & FLAGS_LOCAL_SPACE_EMISSION) != 0;
}

// 0x00C9E9E0 (.h:152)
void rl_renderable_particles::set_local_space_emission(bool local_space_emission)
{
    if (local_space_emission)
    {
        m_flags |= FLAGS_LOCAL_SPACE_EMISSION;
        return;
    }
    m_flags &= ~FLAGS_LOCAL_SPACE_EMISSION;
}

// 0x004BD4F0 (.h:163)
bool rl_renderable_particles::get_is_reverse_render_order() const
{
    return (m_flags & FLAGS_REVERSE_RENDER_ORDER) != 0;
}

// 0x00C9EA00 (.h:168)
void rl_renderable_particles::set_is_reverse_render_order(bool is_reverse_render_order)
{
    if (is_reverse_render_order)
    {
        m_flags |= FLAGS_REVERSE_RENDER_ORDER;
        return;
    }
    m_flags &= ~FLAGS_REVERSE_RENDER_ORDER;
}

// 0x004BD500 (.h:178)
int rl_renderable_particles::get_tail_length() const
{
    return m_tail_length;
}

// 0x00C9EA20 (.h:183)
void rl_renderable_particles::set_tail_length(int tail_length)
{
    m_tail_length = tail_length;
}

// 0x004BD510 (.h:188): the flag (m_is_iterative is a separate field vfxlib sets).
bool rl_renderable_particles::get_is_iterative() const
{
    return (m_flags & FLAGS_IS_ITERATIVE) != 0;
}

// 0x00C9EA30 (.h:193)
void rl_renderable_particles::set_is_iterative(bool is_iterative)
{
    if (is_iterative)
    {
        m_flags |= FLAGS_IS_ITERATIVE;
        return;
    }
    m_flags &= ~FLAGS_IS_ITERATIVE;
}

// 0x004BD520 (.h:203)
bool rl_renderable_particles::get_texture_anim_random_frames() const
{
    return m_texture_anim_random_frames;
}

// 0x00C9EA50 (.h:208)
void rl_renderable_particles::set_texture_anim_random_frames(bool texture_anim_random_frames)
{
    m_texture_anim_random_frames = texture_anim_random_frames;
}

// 0x00C9EA60 (.h:218)
void rl_renderable_particles::set_is_color_correction_enabled(bool is_color_correction_enabled)
{
    if (is_color_correction_enabled)
    {
        m_flags |= FLAGS_IS_COLOR_CORRECTION_ENABLED;
        return;
    }
    m_flags &= ~FLAGS_IS_COLOR_CORRECTION_ENABLED;
}

// 0x004BD530 (.h:228)
float rl_renderable_particles::get_lod_large_particle_fade_min() const
{
    return m_lod_large_particle_fade_min;
}

// 0x00C9EA80 (.h:233)
void rl_renderable_particles::set_lod_large_particle_fade_min(float lod_fade_min)
{
    m_lod_large_particle_fade_min = lod_fade_min;
}

// 0x004BD540 (.h:239)
float rl_renderable_particles::get_lod_large_particle_fade_max() const
{
    return m_lod_large_particle_fade_max;
}

// 0x00C9EA90 (.h:244)
void rl_renderable_particles::set_lod_large_particle_fade_max(float lod_fade_max)
{
    m_lod_large_particle_fade_max = lod_fade_max;
}

// 0x004BD550 (.h:250)
float rl_renderable_particles::get_effect_material_percent() const
{
    return m_effect_material_percent;
}

// 0x00C9EAA0 (.h:255)
void rl_renderable_particles::set_effect_material_percent(float effect_material_percent)
{
    m_effect_material_percent = effect_material_percent;
}

// 0x00C9EAB0 (.h:265)
void rl_renderable_particles::set_self_illum(float self_illum)
{
    m_self_illum = self_illum;
}

// 0x004BD560 (.h:271)
const rl_color_float& rl_renderable_particles::get_ambient_color() const
{
    return m_ambient_color;
}

// 0x00C9EAC0 (.h:276)
void rl_renderable_particles::set_ambient_color(const rl_color_float& ambient_color)
{
    m_ambient_color = ambient_color;
}

// 0x004BD570 (.h:282)
const rl_color_float& rl_renderable_particles::get_diffuse_color() const
{
    return m_diffuse_color;
}

// 0x00C9EAE0 (.h:287)
void rl_renderable_particles::set_diffuse_color(const rl_color_float& diffuse_color)
{
    m_diffuse_color = diffuse_color;
}

// 0x004BD580 (.h:292)
float rl_renderable_particles::get_soft_thickness() const
{
    return m_soft_thickness;
}

// 0x00C9EB00 (.h:297)
void rl_renderable_particles::set_soft_thickness(float soft_thickness)
{
    m_soft_thickness = soft_thickness;
}

// 0x007C1EE0 (.h:23)
void rl_renderable_particles::set_objects_max(int objects_max)
{
    ms_class_factory_info.m_objects_max = objects_max;
}
