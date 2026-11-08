// Original: volition/render_lib/code/rl_skirt_mesh_instance.h
// Implementation: volition/render_lib/code/rl_skirt_mesh_instance.cpp
//
// rl_skirt_mesh_instance, terrain skirt meshes that hide seams between terrain LODs.
#pragma once

#include "volition/render_lib/code/pool_list.h"
#include "volition/render_lib/code/rl_mesh_instance.h"

class mempool;
class rl_batched_pass;
class rl_camera;
class rl_class_factory_info;
class rl_deferred_batch_info;
class rl_material_map;
class rl_renderer;

// size 0x130
class rl_skirt_mesh_instance : public rl_mesh_instance
{
protected:
    rl_skirt_mesh_instance(); // 0x0054E100 (:24)
    virtual ~rl_skirt_mesh_instance(); // vtbl+0x00 0x0054E150 (:32)
private:
    static const pool_list<rl_skirt_mesh_instance*>& get_object_pool(); // 0x004D1640 (.h:22)
public:
    static void set_objects_max(int);
    static int get_objects_max(); // 0x004D1650 (.h:22)
    static int get_objects_used();
    static int get_objects_free();
    static void factory_init(mempool& factory_mempool); // 0x00518DD0 (:21)
    static rl_skirt_mesh_instance* create_placement(rl_skirt_mesh_instance*);
    static rl_skirt_mesh_instance* create(); // 0x00550E60 (:21)
    static bool can_create(); // 0x004FCD60 (:21)
    virtual void release(); // vtbl+0x04 0x00530DF0 (:21)
    virtual void render(rl_renderer& renderer, rl_camera& camera, unsigned int flags); // vtbl+0x10 0x00531290 (:135)
    virtual void add_to_batch(rl_batched_pass* pass_p, const rl_camera* pCamera, rl_deferred_batch_info* def_ids); // vtbl+0x1C 0x00518ED0 (:184)
    virtual void add_to_deferred(const rl_camera* pCamera, rl_deferred_batch_info* def_ids); // vtbl+0x2C 0x004D1690 (:203)
    virtual void render_for_deferred(rl_renderer& renderer, rl_camera&, unsigned int def_id, unsigned int flags); // vtbl+0x14 0x00531310 (:162)
    rl_material_map* get_terrain_material_map() const; // 0x004D1660 (.h:60)
    void set_terrain_material_map(rl_material_map* map_p); // 0x005D0C20 (.h:65)
    bool get_blending_enabled() const;
    void set_blending_enabled(bool blending_enabled); // 0x004D1670 (:178)
    rl_mesh_instance* getRootMeshInstance() const; // 0x005D0C00 (.h:38)
    void setRootMeshInstance(rl_mesh_instance* pMesh); // 0x005D0C10 (.h:39)
private:
    void render_normal_skirt(rl_renderer& renderer, rl_camera& camera); // 0x00531270 (:124)
    void render_terrain_skirt(rl_renderer& renderer); // 0x00530E70 (:43)
    void set_shader_types_internal(rl_renderer& renderer); // 0x004FCD70 (:36)

public:
    static const unsigned int m___class_id; // 0x012BEE58
    static rl_class_factory_info ms_class_factory_info; // 0x015162F0
protected:
    static pool_list<rl_skirt_mesh_instance*> ms_object_pool; // 0x01516320
private:
    rl_material_map* m_terrain_material_map; // 0x120
    bool m_blending_enabled; // 0x124
    rl_mesh_instance* m_pRootMeshInstance; // 0x128
};
