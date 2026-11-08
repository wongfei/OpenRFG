// Original: volition/render_lib/code/rl_low_lod_mesh_instance.h
// Implementation: volition/render_lib/code/rl_low_lod_mesh_instance.cpp
//
// rl_low_lod_mesh_instance, low LOD terrain patches.
#pragma once

#include "volition/render_lib/code/pool_list.h"
#include "volition/render_lib/code/rl_scene_entity.h"

class mempool;
class rl_camera;
class rl_class_factory_info;
class rl_decal;
class rl_decal_parameters;
class rl_material_map;
class rl_mesh;
class rl_renderable;
class rl_renderer;
class vector;

// size 0xD0
class rl_low_lod_mesh_instance : public rl_scene_entity
{
protected:
    rl_low_lod_mesh_instance(); // 0x0053EB20 (:17)
    virtual ~rl_low_lod_mesh_instance(); // vtbl+0x00 0x0053EDD0 (:31)
private:
    static const pool_list<rl_low_lod_mesh_instance*>& get_object_pool(); // 0x004C29A0 (.h:22)
public:
    static void set_objects_max(int);
    static int get_objects_max(); // 0x004C29B0 (.h:22)
    static int get_objects_used();
    static int get_objects_free();
    static void factory_init(mempool& factory_mempool); // 0x00510C20 (:14)
    static rl_low_lod_mesh_instance* create_placement(rl_low_lod_mesh_instance*);
    static rl_low_lod_mesh_instance* create(); // 0x0054BC60 (:14)
    static bool can_create(); // 0x004E8B80 (:14)
    virtual void release(); // vtbl+0x04 0x0053EAA0 (:14)
    virtual void render(rl_renderer& renderer, rl_camera&, unsigned int flags); // vtbl+0x10 0x005272E0 (:76)
    virtual bool init(rl_mesh* mesh_p, const rl_material_map* mat_map_p, unsigned int submesh_id); // vtbl+0x14 0x004E8B90 (:126)
    const rl_material_map* get_material_map(); // 0x004C29D0 (.h:71)
    int get_sub_mesh_index() const; // 0x004C29C0 (.h:39)
    void set_sub_mesh_index(int);
    const vector& get_target_position() const;
    void set_target_position(const vector&);
    void set_has_high_lod_loaded(bool is_loaded); // 0x0091B1D0 (.h:90)
    bool get_has_high_lod_loaded() const; // 0x004C29E0 (.h:95)
    virtual unsigned int add_decal(const rl_decal_parameters*, unsigned int); // vtbl+0x18 0x004C29F0 (:103)
    virtual void render_decals(int, rl_decal**); // vtbl+0x1C 0x004C2A10 (:110)
    virtual void set_renderable(rl_renderable* renderable_p); // vtbl+0x20 0x0053EBB0 (.h:76)
    virtual rl_renderable* get_renderable() const; // vtbl+0x24 0x0053EBA0 (.h:85)
private:
    void set_material(rl_renderer& renderer, int material_idx); // 0x00527230 (:40)
    void render_internal(rl_renderer& renderer, bool rendering_decal); // 0x00527250 (:51)
    void set_shader_types(rl_renderer& renderer); // 0x00510D20 (:95)

public:
    static const unsigned int m___class_id; // 0x012BEE28
    static rl_class_factory_info ms_class_factory_info; // 0x01514D0C
protected:
    static pool_list<rl_low_lod_mesh_instance*> ms_object_pool; // 0x01514D3C
private:
    const rl_material_map* m_material_map_p; // 0xC0
    int m_submesh_index; // 0xC4
    rl_renderable* m_renderable_p; // 0xC8
    bool m_has_high_lod_loaded; // 0xCC
};
