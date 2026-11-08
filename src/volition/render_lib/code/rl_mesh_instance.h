// Original: volition/render_lib/code/rl_mesh_instance.h
// Implementation: volition/render_lib/code/rl_mesh_instance.cpp
//
// rl_mesh_instance, an instance of an rl_mesh with its material map.
#pragma once

#include "volition/render_lib/code/pool_list.h"
#include "volition/render_lib/code/rl_renderable_instance.h"
#include "volition/vlib/code/math/matrix.h"
#include "volition/vlib/code/math/vector4.h"

class mempool;
class rl_batched_pass;
class rl_camera;
class rl_class_factory_info;
class rl_decal;
class rl_decal_parameters;
class rl_material_map;
class rl_mesh;
class rl_renderer;

// size 0x120
class rl_mesh_instance : public rl_renderable_instance
{
public:
    enum flags
    {
        FLAGS_ALPHA_CACHE_VALID = 2048,
        FLAGS_ALPHA_CACHE = 4096,
        FLAGS_DISABLE_DEFERRED = 8192,
        NUM_FLAGS = 14,
    };

protected:
    rl_mesh_instance(); // 0x0054A850 (.h:132)
    virtual ~rl_mesh_instance(); // vtbl+0x00 0x0054A8A0 (.h:142)
private:
    static const pool_list<rl_mesh_instance*>& get_object_pool(); // 0x004BEBE0 (.h:31)
public:
    static void set_objects_max(int objects_max); // 0x007C1F00 (.h:31)
    static int get_objects_max(); // 0x004BEBF0 (.h:31)
    static int get_objects_used();
    static int get_objects_free();
    static void factory_init(mempool& factory_mempool); // 0x00511B70 (:36)
    static rl_mesh_instance* create_placement(rl_mesh_instance*);
    static rl_mesh_instance* create(); // 0x0054BD80 (:36)
    static bool can_create(); // 0x004E94D0 (:36)
    virtual void release(); // vtbl+0x04 0x0053F6D0 (:36)
    virtual void render(rl_renderer& renderer, rl_camera&, unsigned int flags); // vtbl+0x10 0x005281E0 (:151)
    virtual void set_shader_types(rl_renderer& renderer); // vtbl+0x54 0x00511C70 (:145)
    virtual void on_visible_for_frame(rl_renderer& renderer); // vtbl+0x0C 0x00528310 (:241)
    virtual void add_to_batch(rl_batched_pass* pass_p, const rl_camera* pCamera, rl_deferred_batch_info* def_ids); // vtbl+0x1C 0x00511F90 (:261)
    virtual void render_for_deferred(rl_renderer& renderer, rl_camera&, unsigned int, unsigned int flags); // vtbl+0x14 0x00528370 (:464)
    virtual int get_indices_for_batch(const unsigned char** indices, unsigned int& first_index, unsigned int& block_id, unsigned int sub_id) const; // vtbl+0x20 0x005125D0 (:523)
    virtual int get_instance_stream_index(unsigned int sub_id) const; // vtbl+0x24 0x004C3090 (:564)
    virtual void get_index_range(int& index_min, int& index_max, unsigned int sub_id) const; // vtbl+0x28 0x00512660 (:544)
    virtual unsigned int add_decal(const rl_decal_parameters* decal_params, unsigned int reserved_handle); // vtbl+0x30 0x0053F750 (:593)
    virtual void render_decals(rl_renderer& renderer, int num_decals, rl_decal** decal_list_p); // vtbl+0x40 0x005284A0 (:639)
    int get_decal_count(); // 0x004C30A0 (:627)
    virtual bool init(rl_mesh* mesh_p, const rl_material_map* mat_map_p, unsigned int submesh_id); // vtbl+0x58 0x004E9740 (:578)
    void set_submesh_index(unsigned int submesh_id); // 0x007F20B0 (.h:73)
    unsigned int get_submesh_index() const; // 0x004BEC00 (.h:74)
    virtual void set_material_map(const rl_material_map* mat_p); // vtbl+0x4C 0x004C30C0 (:770)
    void calculate_local_aabb(); // 0x004E94E0 (:49)
    virtual const rl_material_map* get_material_map(); // vtbl+0x50 0x0054A890 (.h:82)
    void set_disable_deferred(bool disable_deferred); // 0x006E36A0 (.h:91)
protected:
    void get_material_block_counts(int* buffer, const rl_camera* pCamera); // 0x00511CA0 (:181)
    void prerender(rl_renderer& renderer); // 0x00528100 (:134)
    void postrender(rl_renderer&); // 0x004C3080 (:141)
    void render_internal(rl_renderer& renderer, unsigned int material_flags); // 0x00527F10 (:64)
private:
    void add_material_stats_to_batch(rl_batched_pass* pass_p, const rl_camera* pCamera); // 0x00511EC0 (:221)

public:
    static const unsigned int m___class_id; // 0x012BEDDC
    static rl_class_factory_info ms_class_factory_info; // 0x01514DFC
protected:
    static pool_list<rl_mesh_instance*> ms_object_pool; // 0x01514E2C
    const rl_material_map* m_mat_map; // 0x110
    unsigned int m_submesh_index; // 0x114
    int m_render_instance_index; // 0x118
};

// size 0x40
struct rl_mesh_instance_info
{
    rl_mesh_instance_info(); // 0x004BCF60

    matrix43 transform; // 0x00
    vector4 instance_params; // 0x30
};
