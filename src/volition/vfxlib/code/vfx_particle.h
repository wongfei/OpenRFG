// Original: volition/vfxlib/code/vfx_particle.h
//
// Particles: the common base, parametric particles (position from birth state and age) and iterative particles
// (integrated per frame, with an optional tail of past positions). alignas(16) of vfx_particle_iterative is inferred:
// its PDB size (0x130) pads the last member's end (0x124) to 16 bytes (found by the layout check).
#pragma once

#include "volition/render_lib/code/linked_list_p.h"
#include "volition/render_lib/code/rl_color.h"
#include "volition/render_lib/code/rl_particle.h"
#include "volition/vlib/code/math/vector3.h"

class vfx_particle_system_emitter_instance;

// size 0x38
class vfx_particle_base
{
public:
    vector position; // 0x00
    float rotation_angle; // 0x0C
    vector velocity; // 0x10
    float time_created; // 0x1C
    vector random_vector; // 0x20
    float random_float; // 0x2C
    float lifetime; // 0x30
    int emit_number; // 0x34
};

// size 0x40
class vfx_particle_parametric : public vfx_particle_base
{
public:
    vfx_particle_parametric* get_prev() const;
    void set_prev(vfx_particle_parametric*);
    vfx_particle_parametric* get_next() const;
    void set_next(vfx_particle_parametric*);
    void copy_to_rl_particle(rl_particle_parametric& particle) const; // 0x00C9B6F0 (.h:63)

    vfx_particle_parametric* prev; // 0x38
    vfx_particle_parametric* next; // 0x3C
};

// size 0x130
class alignas(16) vfx_particle_iterative : public vfx_particle_base
{
public:
    // size 0x30
    class tail_data
    {
    public:
        tail_data* get_prev() const;
        void set_prev(tail_data*);
        tail_data* get_next() const;
        void set_next(tail_data*);
        void set(vfx_particle_iterative& particle_rhs); // 0x00C9B7F0 (.h:170)
        void copy_to_rl_tail(rl_particle_base::tail_data& tail) const; // 0x00C9B830 (.h:179)

        tail_data* prev; // 0x00
        tail_data* next; // 0x04
        float rotation_angle; // 0x08
        vector position; // 0x0C
        rl_color_float current_color; // 0x18
        float current_size; // 0x28
        float current_length; // 0x2C
    };

    vfx_particle_iterative* get_prev() const;
    void set_prev(vfx_particle_iterative*);
    vfx_particle_iterative* get_next() const;
    void set_next(vfx_particle_iterative*);
    void copy_to_rl_particle(rl_particle_iterative& particle) const; // 0x00C9B750 (.h:83)

    vfx_particle_iterative* prev; // 0x38
    vfx_particle_iterative* next; // 0x3C
    rl_color_float current_color; // 0x40
    float density; // 0x50
    float current_color_saturation; // 0x54
    float current_size; // 0x58
    float current_length; // 0x5C
    float current_additive_key; // 0x60
    vector emission_position; // 0x64
    rl_color_float color_start; // 0x70
    rl_color_float color_end; // 0x80
    rl_color_float color_mid_A; // 0x90
    rl_color_float color_mid_B; // 0xA0
    float rotation_angular_speed_start; // 0xB0
    float rotation_angular_speed_end; // 0xB4
    float size_start; // 0xB8
    float size_end; // 0xBC
    float size_mid_A; // 0xC0
    float size_mid_B; // 0xC4
    float length_start; // 0xC8
    float length_end; // 0xCC
    float length_mid_A; // 0xD0
    float length_mid_B; // 0xD4
    float color_saturation_start; // 0xD8
    float color_saturation_end; // 0xDC
    float color_saturation_mid_A; // 0xE0
    float color_saturation_mid_B; // 0xE4
    float additive_key_start; // 0xE8
    float additive_key_end; // 0xEC
    float additive_key_mid_A; // 0xF0
    float additive_key_mid_B; // 0xF4
    vector acceleration_start; // 0xF8
    vector acceleration_end; // 0x104
    float weight; // 0x110
    float iter_life_ratio; // 0x114
    linked_list_p<tail_data> tails_list; // 0x118
    vfx_particle_system_emitter_instance* m_emitted_emitter_list; // 0x120
};

// The base part as the renderer reads it; rl_particle_base::pad takes the list links (decompiled).
inline void vfx_copy_particle_base(const vfx_particle_base& from, rl_particle_base& to, void* prev, void* next)
{
    to.position = from.position;
    to.rotation_angle = from.rotation_angle;
    to.velocity = from.velocity;
    to.time_created = from.time_created;
    to.random.x = from.random_vector.x;
    to.random.y = from.random_vector.y;
    to.random.z = from.random_vector.z;
    to.random.w = from.random_float;
    to.lifetime = from.lifetime;
    to.emit_number = from.emit_number;
    to.pad[0] = (float*)prev;
    to.pad[1] = (float*)next;
}

// 0x00C9B6F0 (.h:63)
inline void vfx_particle_parametric::copy_to_rl_particle(rl_particle_parametric& particle) const
{
    vfx_copy_particle_base(*this, particle, prev, next);
}

// 0x00C9B750 (.h:83)
inline void vfx_particle_iterative::copy_to_rl_particle(rl_particle_iterative& particle) const
{
    particle.reset();
    vfx_copy_particle_base(*this, particle, prev, next);
    particle.current_color = current_color;
    particle.density = density;
    particle.current_color_saturation = current_color_saturation;
    particle.current_size = current_size;
    particle.current_length = current_length;
    particle.current_additive_key = current_additive_key;
    particle.emission_position = emission_position;
}

// 0x00C9B7F0 (.h:170)
inline void vfx_particle_iterative::tail_data::set(vfx_particle_iterative& particle_rhs)
{
    rotation_angle = particle_rhs.rotation_angle;
    position = particle_rhs.position;
    current_color = particle_rhs.current_color;
    current_size = particle_rhs.current_size;
    current_length = particle_rhs.current_length;
}

// 0x00C9B830 (.h:179)
inline void vfx_particle_iterative::tail_data::copy_to_rl_tail(rl_particle_base::tail_data& tail) const
{
    tail.rotation_angle = rotation_angle;
    tail.position = position;
    tail.current_color = current_color;
    tail.current_size = current_size;
    tail.current_length = current_length;
}
