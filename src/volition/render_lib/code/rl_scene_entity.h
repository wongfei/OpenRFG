// Original: volition/render_lib/code/rl_scene_entity.h
// Implementation: volition/render_lib/code/rl_scene_entity.cpp
//
// rl_scene_entity, an object placed in a scene: flags, pass flags, LOD distance, world/local bounding volumes and
// visibility bookkeeping.
#pragma once

#include "volition/render_lib/code/linked_list_p.h"
#include "volition/render_lib/code/rl_3d_entity.h"
#include "volition/render_lib/code/rl_bvolume.h"
#include "volition/render_lib/code/rl_partition_sector_entry.h"

class matrix;
class rl_aabb;
class rl_color_float;
class rl_frustum;
class rl_occluder;
class rl_partition_system;
class rl_renderer;
class rl_scene;
class rl_sphere;
class vector;

// size 0xC0
class rl_scene_entity : public rl_3d_entity
{
    // Access inferred: rfg's glass_obj::create_mesh sets the shard meshes' flags directly (friends are not recorded in
    // rfg.pdb).
    friend struct glass_obj;
    friend class vehicle_component; // the tread pads' bounds, vehicle_component::create_renderable_instance
public:
    enum flags
    {
        FLAGS_WORLD_BOUNDING_VOLUME_DIRTY = 1,
        FLAGS_NOT_VISIBLE = 2,
        FLAGS_NOT_GPU_OCCLUDED = 4,
        FLAGS_TERRAIN = 8,
        FLAGS_GPU_OCCLUSION_CACHED = 16,
        NUM_FLAGS = 5,
    };

    rl_scene_entity* get_prev() const;
    void set_prev(rl_scene_entity*);
    rl_scene_entity* get_next() const;
    void set_next(rl_scene_entity*);
protected:
    rl_scene_entity(); // 0x004F5C60 (:24)
    virtual ~rl_scene_entity(); // vtbl+0x00 0x0052DDC0 (:46)
public:
    void set_position(const vector& position); // 0x004DD640 (.h:362)
    void set_rotation(const matrix& rotation); // 0x004DD690 (.h:372)
    const vector& get_position() const; // 0x004B88B0 (.h:390)
    const matrix& get_rotation() const; // 0x004B88C0 (.h:395)
    void set_local_aabb(const rl_aabb& local_aabb); // 0x004DD140 (.h:239)
    void get_local_aabb(rl_aabb& aabb) const; // 0x00B1FB00 (.h:254)
    rl_aabb get_local_aabb() const; // 0x00508E80 (.h:249)
    void set_local_bounding_sphere(const rl_sphere& local_bounding_sphere); // 0x004DD2F0 (.h:259)
    void get_local_bounding_sphere(rl_sphere&) const;
    rl_sphere get_local_bounding_sphere() const; // 0x007F2090 (.h:269)
    const rl_bvolume& get_world_bvolume() const; // 0x004DD5C0 (.h:293)
    void set_last_visibility_pass(unsigned int last_visibility_pass); // 0x004B88D0 (.h:401)
    unsigned int get_last_visibility_pass() const;
    void get_world_sphere(rl_sphere&) const;
    rl_sphere get_world_sphere() const; // 0x004DD5E0 (.h:303)
    void set_world_bounding_sphere(const rl_sphere& world_sphere); // 0x00C9E1C0 (.h:321)
    void get_world_aabb(rl_aabb& aabb) const; // 0x007451A0 (.h:336)
    rl_aabb get_world_aabb() const; // 0x00508F70 (.h:327)
    void set_world_aabb(const rl_aabb& world_aabb); // 0x004B8730 (.h:345)
    void set_bounding_volume_dirty(bool dirty); // 0x004DD610 (.h:351)
    rl_scene* get_scene() const; // 0x004B88E0 (.h:412)
    void scene_removed(); // 0x004B88F0 (.h:417)
    void scene_added(rl_scene* scene_p); // 0x004B8900 (.h:422)
    void update_partition_system(); // 0x004B8890 (.h:382)
    const rl_color_float& get_debug_color() const; // 0x004B89C0 (.h:506)
    bool get_is_visible() const; // 0x004B8910 (.h:427)
    void set_gpu_occluded(bool is_gpu_occluded); // 0x006E1380 (.h:441)
    bool get_gpu_occluded() const; // 0x004B8940 (.h:450)
    void set_is_terrain(bool is_terrain); // 0x005D0B70 (.h:459)
    bool get_is_terrain() const; // 0x004B8950 (.h:468)
    void set_is_gpu_occlusion_cached(bool is_gpu_occlusion_cached); // 0x004B8960 (.h:477)
    bool get_is_gpu_occlusion_cached() const; // 0x004B8980 (.h:486)
    float get_camera_distance_squared() const; // 0x004B8990 (.h:495)
    void set_camera_distance_squared(float camera_distance_squared); // 0x004B89A0 (.h:500)
    int get_visibility_list_index() const; // 0x004B89D0 (.h:517)
    void set_visibility_list_index(int visibility_list_index); // 0x004B89E0 (.h:522)
    unsigned int get_vis_pass_flags() const; // 0x004B89F0 (.h:527)
    void set_vis_pass_flags(unsigned int vis_pass_flags); // 0x004B8A00 (.h:532)
    rl_partition_system* get_partition_system() const; // 0x004B8A50 (.h:568)
    void set_partition_system(rl_partition_system* partition_system_p); // 0x004B8A60 (.h:573)
    rl_partition_sector_entry* get_partition_sector_entry(); // 0x004B8A70 (.h:579)
    void set_category_id(unsigned char id); // 0x004B8A80 (.h:584)
    unsigned char get_category_id(); // 0x004B8A90 (.h:589)
    void set_debug_color(unsigned char, const rl_color_float&);
    virtual bool process_internal_visibility(const rl_frustum& frustum, const linked_list_p<rl_occluder>& occluders); // vtbl+0x08 0x004F5DA0 (.h:147)
    virtual void on_visible_for_frame(rl_renderer&); // vtbl+0x0C 0x004F5DB0 (.h:150)
    unsigned int get_flags() const; // 0x004B8700 (.h:153)
    unsigned short get_render_pass_flags() const; // 0x004B8A10 (.h:538)
    void set_render_pass_flags(unsigned short);
    unsigned int get_gpu_vis_flags() const;
    void set_gpu_vis_flags(unsigned int);
    float get_lod_off_distance() const; // 0x004B8A20 (.h:548)
    void set_lod_off_distance(float lod_off_distance); // 0x00710DF0 (.h:553)
    int get_gpu_vis_index() const; // 0x004B8A30 (.h:558)
    void set_gpu_vis_index(int gpu_vis_index); // 0x004B8A40 (.h:563)
    bool get_flag(unsigned int) const;
    void set_flag(unsigned int, bool);
protected:
    void set_is_visible(bool is_visible); // 0x004B8920 (.h:432)
private:
    void compute_bounding_volume() const; // 0x004DD370 (.h:279)
public:
    rl_scene_entity& operator=(const rl_scene_entity& __that); // 0x00517940

    rl_scene_entity* prev; // 0x38
    rl_scene_entity* next; // 0x3C
protected:
    unsigned short m_flags; // 0x40
    unsigned short m_pass_flags; // 0x42
    float m_lod_off_distance; // 0x44
    int m_gpu_vis_index; // 0x48
private:
    rl_bvolume m_world_bvolume; // 0x50
    rl_bvolume m_local_bvolume; // 0x70
    unsigned int m_vis_pass_flags; // 0x90
    float m_camera_distance_squared; // 0x94
    unsigned short m_visibility_list_index; // 0x98
    unsigned char m_category_id; // 0x9A
    unsigned char m_last_visibility_pass; // 0x9B
    rl_partition_system* m_partition_system_p; // 0x9C
    rl_partition_sector_entry m_partition_sector_entry; // 0xA0
    rl_scene* m_scene_p; // 0xB0
    static rl_color_float ms_debug_colors[256]; // 0x01515230
};

// 0x00517940
// Compiler-generated: memberwise.
inline rl_scene_entity& rl_scene_entity::operator=(const rl_scene_entity& __that) = default;

// Inlined in rfg.exe (no instance): a bit of m_flags.
inline bool rl_scene_entity::get_flag(unsigned int flag) const
{
    return (m_flags & flag) != 0;
}

// Inlined in rfg.exe (no instance)
inline void rl_scene_entity::set_flag(unsigned int flag, bool set)
{
    if (set)
    {
        m_flags |= flag;
    }
    else
    {
        m_flags &= ~flag;
    }
}
