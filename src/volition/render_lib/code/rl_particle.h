// Original: volition/render_lib/code/rl_particle.h
// Implementation: volition/render_lib/code/rl_particle.cpp
//
// Particles: rl_particle_base, rl_particle_iterative and rl_particle_parametric, and particle LODs. Placement of all
// but rl_particle_iterative inferred.
#pragma once

#include "volition/render_lib/code/rl_color.h"
#include "volition/vlib/code/math/vector3.h"
#include "volition/vlib/code/math/vector4.h"

enum rl_particle_lod
{
    RL_PARTICLE_LOD_LOW = 0,
    RL_PARTICLE_LOD_MED = 1,
    RL_PARTICLE_LOD_HIGH = 2,
    RL_PARTICLE_LOD_VERY_HIGH = 3,
    RL_NUM_PARTICLE_LODS = 4,
};

// size 0x40
class rl_particle_base
{
public:
    enum render_type
    {
        RENDER_TYPE_BILLBOARD = 0,
        RENDER_TYPE_POINTSPRITE = 1,
        RENDER_TYPE_RIBBON = 2,
        RENDER_TYPE_POINT = 3,
        RENDER_TYPE_LINE = 4,
        RENDER_TYPE_NODES = 5,
        RENDER_TYPE_RADIAL = 6,
        RENDER_TYPE_DROP = 7,
        RENDER_TYPE_ORIENTED = 8,
        RENDER_TYPE_CORONA = 9,
        RENDER_TYPE_MAX = 10,
    };

    // size 0x28
    class tail_data
    {
    public:
        float rotation_angle; // 0x00
        vector position; // 0x04
        rl_color_float current_color; // 0x10
        float current_size; // 0x20
        float current_length; // 0x24
    };

    static const int MAX_DROP_SEGMENTS; // 0x012BED28
    vector position; // 0x00
    float rotation_angle; // 0x0C
    vector velocity; // 0x10
    float time_created; // 0x1C
    vector4 random; // 0x20
    float lifetime; // 0x30
    int emit_number; // 0x34
    float* pad[2]; // 0x38
};

// size 0x80
class rl_particle_iterative : public rl_particle_base
{
public:
    void reset(); // 0x00C9B6E0 (.h:115)
    bool alloc_particle_tails(int count); // 0x0054C5C0 (:7)
    void set_particle_tail_list(int, rl_particle_base::tail_data*);
    int get_particle_tail_list_size() const; // 0x004BD460 (.h:127)
    const rl_particle_base::tail_data* get_particle_tail_list() const; // 0x004BD480 (.h:137)
    rl_particle_base::tail_data* get_particle_tail_list(); // 0x004BD470 (.h:132)

    rl_color_float current_color; // 0x40
    float density; // 0x50
    float current_color_saturation; // 0x54
    float current_size; // 0x58
    float current_length; // 0x5C
    float current_additive_key; // 0x60
    vector emission_position; // 0x64
private:
    int m_num_tails; // 0x70
    rl_particle_base::tail_data* m_tails_list; // 0x74
};

// size 0x40
class rl_particle_parametric : public rl_particle_base
{
};

// 0x00C9B6E0 (.h:115): no tails (the body is the first two stores of vfx_particle_iterative::copy_to_rl_particle).
inline void rl_particle_iterative::reset()
{
    m_num_tails = 0;
    m_tails_list = nullptr;
}
