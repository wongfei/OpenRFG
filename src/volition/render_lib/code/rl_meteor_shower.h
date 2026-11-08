// Original: volition/render_lib/code/rl_meteor_shower.h
// Implementation: volition/render_lib/code/rl_meteor_shower.cpp
//
// rl_meteor_shower, the meteors of the Mars sky.
#pragma once

#include "volition/render_lib/code/rl_base_object.h"
#include "volition/render_lib/code/rl_shader.h"
#include "volition/render_lib/code/rl_singleton.h"
#include "volition/vlib/code/math/vector3.h"

class rl_camera;
class rl_class_dynamic_info;
struct rl_meteor_line_vertex;
class rl_renderer;

// size 0x24
struct rl_meteor
{
    rl_meteor(); // 0x004C3200

    vector m_pos; // 0x00
    vector m_vel; // 0x0C
    float m_spawn_time; // 0x18
    float m_lifespan; // 0x1C
    int m_next_free; // 0x20
};

// size 0x284
class rl_meteor_shower : public rl_base_object, public rl_singleton_phoenix<rl_meteor_shower>
{
protected:
    rl_meteor_shower(); // 0x004E9910 (:70)
    virtual ~rl_meteor_shower(); // vtbl+0x00 0x004E99D0 (:88)
public:
    static int get_objects_peak();
    static int get_objects_used();
    static rl_meteor_shower* create(); // 0x00512810 (:65)
    static bool can_create(); // 0x004C31D0 (:65)
    virtual void release(); // vtbl+0x04 0x004C31E0 (:65)
    void initialize(); // 0x005291C0 (:94)
    void shutdown(); // 0x004C3210 (:124)
    void render(rl_renderer& renderer, rl_camera& camera); // 0x0053F8C0 (:210)
    void set_strength(float meteor_strength); // 0x004C3220 (:130)
protected:
    void update(rl_renderer& renderer); // 0x00512880 (:137)
    int fill_meteor_line_vb(rl_meteor_line_vertex* vertex); // 0x004C3240 (:199)

public:
    static rl_class_dynamic_info ms_class_dynamic_info; // 0x01514E5C
protected:
    unsigned int m_meteor_shader; // 0x0C
    rl_shader_technique_ref m_line_tech; // 0x10
    rl_shader_const_ref m_meteor_strength_param; // 0x14
    int m_first_free; // 0x18
    rl_meteor m_meteor[16]; // 0x1C
    float m_meteor_strength; // 0x25C
    float m_emission_delay; // 0x260
    float m_emission_delay_rand; // 0x264
    float m_emission_lifespan; // 0x268
    float m_emission_lifespan_rand; // 0x26C
    float m_emission_speed; // 0x270
    float m_emission_speed_rand; // 0x274
    float m_last_update_time; // 0x278
    float m_emission_time; // 0x27C
    float m_next_emission; // 0x280
};

// size 0x24
struct rl_meteor_line_vertex
{
    vector m_pos; // 0x00
    float m_end; // 0x0C
    vector m_vel; // 0x10
    float m_spawn_time; // 0x1C
    float m_lifespan; // 0x20
};
