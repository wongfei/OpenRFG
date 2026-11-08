// Original: volition/render_lib/code/rl_landmark_lod_mesh_instance.h
// Implementation: volition/render_lib/code/rl_landmark_lod_mesh_instance.cpp
//
// rl_landmark_lod_mesh_instance, distant landmark LOD meshes.
#pragma once

#include "volition/render_lib/code/pool_list.h"
#include "volition/render_lib/code/rl_scene_entity.h"

class mempool;
class rl_class_factory_info;
class rl_material_map;
class rl_mesh;
class rl_renderable;
class rl_renderer;
class vector;

// size 0xD0
class rl_landmark_lod_mesh_instance : public rl_scene_entity
{
protected:
    rl_landmark_lod_mesh_instance(); // 0x0053E4F0 (:18)
    virtual ~rl_landmark_lod_mesh_instance(); // vtbl+0x00 0x0053E870 (:26)
private:
    static const pool_list<rl_landmark_lod_mesh_instance*>& get_object_pool(); // 0x004C22D0 (.h:15)
public:
    static void set_objects_max(int);
    static int get_objects_max(); // 0x004C22E0 (.h:15)
    static int get_objects_used();
    static int get_objects_free();
    static void factory_init(mempool& factory_mempool); // 0x00510470 (:15)
    static rl_landmark_lod_mesh_instance* create_placement(rl_landmark_lod_mesh_instance*);
    static rl_landmark_lod_mesh_instance* create(); // 0x0054BB10 (:15)
    static bool can_create(); // 0x004E89C0 (:15)
    virtual void release(); // vtbl+0x04 0x0053E470 (:15)
    virtual void render(rl_renderer& renderer, unsigned int flags); // vtbl+0x10 0x00526AC0 (:90)
    bool init(rl_mesh* mesh_p, const rl_material_map* mat_map_p, unsigned int submesh_id); // 0x004E89D0 (:124)
    const rl_material_map* get_material_map();
    int get_sub_mesh_index() const;
    void set_sub_mesh_index(int);
    const vector& get_target_position() const;
    void set_target_position(const vector&);
    virtual void set_renderable(rl_renderable* renderable_p); // vtbl+0x14 0x0053E650 (.h:61)
    virtual rl_renderable* get_renderable() const; // vtbl+0x18 0x0053E560 (.h:70)
private:
    void set_material(rl_renderer& renderer, int material_idx); // 0x00526980 (:39)
    void render_internal(rl_renderer& renderer); // 0x00526A20 (:67)
    void set_shader_types(rl_renderer& renderer); // 0x00510570 (:109)

public:
    static const unsigned int m___class_id; // 0x012BEE1C
    static rl_class_factory_info ms_class_factory_info; // 0x01514C34
protected:
    static pool_list<rl_landmark_lod_mesh_instance*> ms_object_pool; // 0x01514C64
private:
    const rl_material_map* m_material_map_p; // 0xC0
    int m_submesh_index; // 0xC4
    rl_renderable* m_renderable_p; // 0xC8
};
