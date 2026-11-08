// Original: volition/render_lib/code/rl_star_field.h
// Implementation: volition/render_lib/code/rl_star_field.cpp
//
// rl_star_field, the night sky star field (point and billboard vertices).
#pragma once

#include "volition/render_lib/code/rl_base_object.h"
#include "volition/render_lib/code/rl_shader.h"
#include "volition/render_lib/code/rl_singleton.h"

class rl_camera;
class rl_class_dynamic_info;
class rl_renderer;
struct rl_star_field_billboard_vertex;
struct rl_star_field_point_vertex;

namespace keen
{
    class MemoryAllocator;
}

// size 0x2C
class rl_star_field : public rl_base_object, public rl_singleton_phoenix<rl_star_field>
{
protected:
    rl_star_field(); // 0x004FEED0 (:200)
    virtual ~rl_star_field(); // vtbl+0x00 0x004FEF30 (:207)
public:
    static int get_objects_peak();
    static int get_objects_used();
    static rl_star_field* create(); // 0x00519120 (:195)
    static bool can_create(); // 0x004D1D80 (:195)
    virtual void release(); // vtbl+0x04 0x004D1D90 (:195)
    void initialize(); // 0x005191D0 (:213)
    void shutdown(); // 0x004D1DB0 (:264)
    void render(rl_renderer& renderer, rl_camera& camera); // 0x005193A0 (:392)
    void set_strength(float star_strength); // 0x004D1DD0 (:271)
protected:
    void prerender(); // 0x004D1DE0 (:278)
    void generate_point_stars(int num_stars, rl_star_field_point_vertex* vertex); // 0x004D1E00 (:307)
    void generate_billboard_stars(int num_stars, rl_star_field_billboard_vertex* vertex); // 0x004D1F90 (:359)

public:
    static rl_class_dynamic_info ms_class_dynamic_info; // 0x01516388
protected:
    unsigned int m_star_field_shader; // 0x0C
    rl_shader_technique_ref m_point_tech; // 0x10
    rl_shader_technique_ref m_billboard_tech; // 0x14
    int m_star_texture_handle; // 0x18
    rl_shader_const_ref m_star_diffuse_sampler; // 0x1C
    rl_shader_const_ref m_star_strength_param; // 0x20
    rl_shader_const_ref m_draw_distance_param; // 0x24
    float m_star_strength; // 0x28
};

// size 0xC
struct rl_star_field_billboard_vertex
{
    short m_pitch; // 0x00
    short m_heading; // 0x02
    short m_brightness; // 0x04
    short m_random; // 0x06
    short m_u; // 0x08
    short m_v; // 0x0A
};

// size 0x8
struct rl_star_field_point_vertex
{
    short m_pitch; // 0x00
    short m_heading; // 0x02
    short m_brightness; // 0x04
    short m_random; // 0x06
};

keen::MemoryAllocator* getGpuBufferAllocator(); // 0x004D1BE0 (:70)
