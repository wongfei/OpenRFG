// Original: volition/render_lib/code/rl_fog_volume.h
// Implementation: volition/render_lib/code/rl_fog_volume.cpp
//
// rl_fog_volume, a local fog volume.
#pragma once

#include "keen/base/types.hpp"
#include "volition/render_lib/code/pool_list.h"
#include "volition/render_lib/code/rl_color.h"
#include "volition/render_lib/code/rl_renderable_instance.h"
#include "volition/render_lib/code/rl_shader.h"
#include "volition/vlib/code/math/vector3.h"

class mempool;
class rl_camera;
class rl_class_factory_info;
class rl_mesh;
struct rl_mesh_data;
struct rl_render_block;
class rl_renderer;
struct rl_submesh_data;

// size 0x180
class rl_fog_volume : public rl_renderable_instance
{
public:
    enum type
    {
        TYPE_BOX = 0,
        TYPE_ELLIPSOID = 1,
    };

    // size 0x18
    struct BoxVertex
    {
        keen::float4 pos; // 0x00
        keen::float2 uv; // 0x10
    };

protected:
    rl_fog_volume(); // 0x0054A8B0 (:30)
    virtual ~rl_fog_volume(); // vtbl+0x00 0x0054A9E0 (:43)
private:
    static const pool_list<rl_fog_volume*>& get_object_pool(); // 0x004BEC10 (.h:23)
public:
    static void set_objects_max(int objects_max); // 0x007C1F20 (.h:23)
    static int get_objects_max(); // 0x004BEC20 (.h:23)
    static int get_objects_used();
    static int get_objects_free();
    static void factory_init(mempool& factory_mempool); // 0x0050E030 (:11)
    static rl_fog_volume* create_placement(rl_fog_volume*);
    static rl_fog_volume* create(); // 0x0054E790 (:11)
    static bool can_create(); // 0x004E4E00 (:11)
    virtual void release(); // vtbl+0x04 0x0053AAD0 (:11)
    bool init(); // 0x005241B0 (:47)
    virtual void render(rl_renderer& renderer, rl_camera& camera, unsigned int flags); // vtbl+0x10 0x004BEC30 (:81)
    virtual void render_for_deferred(rl_renderer& renderer, rl_camera& camera, unsigned int def_id, unsigned int flags); // vtbl+0x14 0x0054A9C0 (.h:34)
    virtual void render_for_deferred(unsigned int def_id, unsigned int flags); // vtbl+0x54 0x004BEC40 (:98)
    void calculate_local_aabb(); // 0x004E4E10 (:69)
    static void system_init(); // 0x0053AB50 (:276)
    static void system_shutdown(); // 0x004E5070 (:444)
private:
    static void* alloc_mesh_memory(int size); // 0x004BEC60 (:457)

public:
    static const unsigned int m___class_id; // 0x012BEDE0
    static rl_class_factory_info ms_class_factory_info; // 0x01514AB0
protected:
    static pool_list<rl_fog_volume*> ms_object_pool; // 0x01514AE0
private:
    vector m_scale; // 0x110
    rl_color_float m_fog_color; // 0x11C
    float m_density; // 0x12C
    float m_falloff_offset; // 0x130
    float m_falloff_scale; // 0x134
    float m_yaw; // 0x138
    float m_pitch; // 0x13C
    type m_type; // 0x140
    static const int BOX_POSITIONS_COUNT; // 0x012BEDE4
    static BoxVertex* ms_box_positions; // 0x01B7BC9C
    static const int BOX_INDEX_COUNT; // 0x012BEDE8
    static unsigned short* ms_box_indices; // 0x01B7BC98
    static rl_mesh* ms_box_mesh_p; // 0x01B7BC88
    static rl_mesh_data* ms_mesh_data; // 0x01B7BC8C
    static rl_submesh_data* ms_submesh; // 0x01B7BC90
    static rl_render_block* ms_renderblock; // 0x01B7BC94
    static bool ms_initialized; // 0x01B7BC84
    unsigned int m_shader_handle; // 0x144
    rl_shader_const_ref m_eye_pos_world_const; // 0x148
    rl_shader_const_ref m_camera_lookat_world_const; // 0x14C
    rl_shader_const_ref m_projtm_inv_const; // 0x150
    rl_shader_const_ref m_fog_density_const; // 0x154
    rl_shader_const_ref m_viewer_is_outside_const; // 0x158
    rl_shader_const_ref m_inv_objtm_const; // 0x15C
    rl_shader_const_ref m_eye_pos_local_const; // 0x160
    rl_shader_const_ref m_height_fall_off_dir_scaled_const; // 0x164
    rl_shader_const_ref m_height_fall_off_base_const; // 0x168
    rl_shader_const_ref m_near_far_clip_scale_const; // 0x16C
    rl_shader_const_ref m_scale_const; // 0x170
    rl_shader_const_ref m_outside_soft_edges_const; // 0x174
    rl_shader_const_ref m_depth_sampler_const; // 0x178
};
