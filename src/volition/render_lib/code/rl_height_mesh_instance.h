// Original: volition/render_lib/code/rl_height_mesh_instance.h
// Implementation: volition/render_lib/code/rl_height_mesh_instance.cpp
//
// rl_height_mesh_instance, terrain height mesh instance.
#pragma once

#include "volition/render_lib/code/pool_list.h"
#include "volition/render_lib/code/rl_renderable_instance.h"
#include "volition/vlib/code/math/vector3.h"

class mempool;
class rl_camera;
class rl_class_factory_info;
class rl_decal;
class rl_decal_parameters;
class rl_material_map;
class rl_renderable;
class rl_renderer;

// size 0x130
class rl_height_mesh_instance : public rl_renderable_instance
{
protected:
    rl_height_mesh_instance(); // 0x0054BA90 (:29)
    virtual ~rl_height_mesh_instance(); // vtbl+0x00 0x0054BB00 (:39)
private:
    static const pool_list<rl_height_mesh_instance*>& get_object_pool(); // 0x004C1DE0 (.h:24)
public:
    static void set_objects_max(int);
    static int get_objects_max(); // 0x004C1DF0 (.h:24)
    static int get_objects_used();
    static int get_objects_free();
    static void factory_init(mempool& factory_mempool); // 0x005101C0 (:26)
    static rl_height_mesh_instance* create_placement(rl_height_mesh_instance*);
    static rl_height_mesh_instance* create(); // 0x0054F3A0 (:26)
    static bool can_create(); // 0x004E85E0 (:26)
    virtual void release(); // vtbl+0x04 0x0053E310 (:26)
    virtual void render(rl_renderer& renderer, rl_camera&, unsigned int flags); // vtbl+0x10 0x00525F90 (:107)
    virtual void set_renderable(rl_renderable* renderable_p); // vtbl+0x44 0x0054BAE0 (.h:85)
    virtual void set_material_map(const rl_material_map* mat_p); // vtbl+0x4C 0x004C2200 (:322)
    virtual const rl_material_map* get_material_map(); // vtbl+0x50 0x0054BAF0 (.h:90)
    int get_sub_mesh_index() const; // 0x004C1E00 (.h:65)
    void set_sub_mesh_index(int sub_mesh_index); // 0x005D0C30 (.h:70)
    const vector& get_target_position() const; // 0x004C1E10 (.h:75)
    void set_target_position(const vector& target_position); // 0x005D0C40 (.h:80)
    virtual unsigned int add_decal(const rl_decal_parameters* decal_params, unsigned int reserved_handle); // vtbl+0x30 0x0053E390 (:162)
    virtual void render_decals(rl_renderer& renderer, int num_decals, rl_decal** decal_list_p); // vtbl+0x40 0x00526140 (:183)
private:
    void set_material(rl_renderer& renderer, int material_idx); // 0x00525CB0 (:43)
    void render_internal(rl_renderer& renderer, bool rendering_decal); // 0x00525D50 (:79)
    void set_shader_types(rl_renderer& renderer); // 0x005102C0 (:154)

public:
    static const unsigned int m___class_id; // 0x012BEDEC
    static rl_class_factory_info ms_class_factory_info; // 0x01514BA4
protected:
    static pool_list<rl_height_mesh_instance*> ms_object_pool; // 0x01514BD4
private:
    const rl_material_map* m_material_map_p; // 0x110
    int m_sub_mesh_index; // 0x114
    vector m_target_position; // 0x118
};

// Not in the binary: the terrain subzone grid cell of a world position, which every terrain draw inlines
// (rl_height_mesh_instance.cpp).
vector rl_terrain_subzone_offset(const vector& position);
