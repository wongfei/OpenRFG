// Original: volition/render_lib/code/rl_compound_mesh_instance.h
// Implementation: volition/render_lib/code/rl_compound_mesh_instance.cpp
//
// rl_compound_mesh_instance, one instance made of sub-instances (rl_sub_instance_info) drawn together.
#pragma once

#include "volition/render_lib/code/linked_list_p.h"
#include "volition/render_lib/code/pool_list.h"
#include "volition/render_lib/code/rl_renderable_instance.h"
#include "volition/vlib/code/math/matrix.h"

class mempool;
class rl_batched_pass;
class rl_camera;
class rl_class_factory_info;
class rl_decal;
class rl_decal_parameters;
class rl_fp_kdtree;
struct rl_fp_kdtree_leaf;
struct rl_fp_kdtree_node;
class rl_frustum;
class rl_material_map;
class rl_mesh;
class rl_occluder;
class rl_renderer;
struct rl_sub_instance;
struct rl_sub_instance_info;
struct rl_sub_instance_update;
class vector;

// size 0x140
class rl_compound_mesh_instance : public rl_renderable_instance
{
public:
    enum flags
    {
        FLAGS_INSTANCE_CACHES_DIRTY = 2048,
        NUM_FLAGS = 12,
    };

protected:
    rl_compound_mesh_instance(); // 0x005490B0 (.h:217)
    virtual ~rl_compound_mesh_instance(); // vtbl+0x00 0x00549120 (.h:233)
private:
    static const pool_list<rl_compound_mesh_instance*>& get_object_pool(); // 0x004BCB20 (.h:71)
public:
    static void set_objects_max(int objects_max); // 0x007C0B00 (.h:71)
    static int get_objects_max(); // 0x004BCB30 (.h:71)
    static int get_objects_used();
    static int get_objects_free();
    static void factory_init(mempool& factory_mempool); // 0x0050AE70 (:40)
    static rl_compound_mesh_instance* create_placement(rl_compound_mesh_instance*);
    static rl_compound_mesh_instance* create(); // 0x005491B0 (:40)
    static bool can_create(); // 0x004E3990 (:40)
    virtual void release(); // vtbl+0x04 0x005389A0 (:40)
    virtual void render(rl_renderer& renderer, rl_camera& camera, unsigned int flags); // vtbl+0x10 0x00538B10 (:222)
    virtual void set_shader_types(rl_renderer& renderer); // vtbl+0x54 0x0050AF70 (:216)
    virtual void on_visible_for_frame(rl_renderer& renderer); // vtbl+0x0C 0x00522980 (:366)
    virtual void add_to_batch(rl_batched_pass* pass_p, const rl_camera* pCamera, rl_deferred_batch_info* def_ids); // vtbl+0x1C 0x0050B450 (:387)
    void add_decals_to_batch(rl_batched_pass* pass_p, unsigned int non_shadow_vis_flags); // 0x004E3D60 (:1591)
    virtual void add_to_deferred(const rl_camera* pCamera, rl_deferred_batch_info* def_ids); // vtbl+0x2C 0x0050BCD0 (:588)
    virtual void render_for_deferred(rl_renderer& renderer, rl_camera& camera, unsigned int def_id, unsigned int flags); // vtbl+0x14 0x00538CA0 (:778)
    virtual void render_debug(); // vtbl+0x18 0x0055EEB0 (:909)
    virtual int get_indices_for_batch(const unsigned char** indices, unsigned int& first_index, unsigned int& block_id, unsigned int sub_id) const; // vtbl+0x20 0x0050C0B0 (:935)
    virtual int get_instance_stream_index(unsigned int sub_id) const; // vtbl+0x24 0x004BCF70 (:989)
    virtual void get_index_range(int& index_min, int& index_max, unsigned int sub_id) const; // vtbl+0x28 0x0050C150 (:961)
    virtual unsigned int add_decal(const rl_decal_parameters* decal_params, unsigned int reserved_handle); // vtbl+0x30 0x00549500 (:1258)
    virtual void render_decals(rl_renderer& renderer, int num_decals, rl_decal** decal_list_p); // vtbl+0x40 0x005229E0 (:1328)
    int get_decal_count(); // 0x004BD0A0 (:1311)
    bool init(rl_mesh* mesh_p, const rl_material_map* mat_map_p, rl_sub_instance_info* sub_inst_info_p, int num_sub_inst_infos); // 0x00549210 (:1004)
    void update_instances(rl_sub_instance_update* updates_p, int num_updates); // 0x00549480 (:1060)
    virtual void get_sub_instance_info(rl_sub_instance_info& sub_info, int base_index) const; // vtbl+0x58 0x004BCFB0 (:1076)
    void set_sub_instance_is_active(short* sub_inst_indices, int num_sub_insts, bool is_active); // 0x004BD040 (:1156)
    virtual bool process_internal_visibility(const rl_frustum& frustum, const linked_list_p<rl_occluder>& occluders); // vtbl+0x08 0x004BD030 (:1097)
    void set_position(const vector& position); // 0x004E3860 (.h:261)
    void set_rotation(const matrix& rotation); // 0x004E38D0 (.h:269)
    virtual void set_material_map(const rl_material_map* mat_p); // vtbl+0x4C 0x004BD0D0 (:1575)
    static int calculate_memory_cost(int num_sub_instances); // 0x004BD0C0 (:1475)
    void calculate_local_aabb(); // 0x004E39A0 (:96)
    virtual const rl_material_map* get_material_map(); // vtbl+0x50 0x00549110 (.h:153)
    int get_num_sub_instances();
    rl_sub_instance* get_sub_instance(int);
protected:
    void prerender(rl_renderer& renderer); // 0x005228A0 (:198)
    void postrender(); // 0x004BCF50 (:211)
    void render_sub_instance(rl_renderer& renderer, unsigned int sub_instance_idx, unsigned int material_flags, bool rendering_decal, bool skip_alpha); // 0x005232C0 (:1489)
    void render_individual_sub_instance(rl_renderer& renderer, unsigned int sub_instance_idx, bool rendering_decal); // 0x00538A20 (:142)
    virtual int find_decal_sub_insts(int* out_indices, int max_out_indices, const rl_decal_parameters* decal_params); // vtbl+0x5C 0x0050C1C0 (:1216)
    void update_xform_cache(); // 0x004BCF40 (:61)
    void get_material_block_counts(int* buffer, const rl_camera* pCamera); // 0x0050AFA0 (:259)
private:
    bool build_rbb(); // 0x00538DB0 (:1174)
    void add_material_stats_to_batch(rl_batched_pass* pass_p, const rl_camera* pCamera); // 0x0050B380 (:339)

public:
    static const unsigned int m___class_id; // 0x012BECDC
    static rl_class_factory_info ms_class_factory_info; // 0x01514918
protected:
    static pool_list<rl_compound_mesh_instance*> ms_object_pool; // 0x01514948
public:
    rl_compound_mesh_instance* prev; // 0x110
    rl_compound_mesh_instance* next; // 0x114
protected:
    const rl_material_map* m_mat_map; // 0x118
    rl_sub_instance* m_sub_instances; // 0x11C
    int m_num_sub_instances; // 0x120
    int m_num_renderable_sub_instances; // 0x124
    rl_fp_kdtree_node* m_kdtree_nodes; // 0x128
    rl_fp_kdtree_leaf* m_kdtree_leaves; // 0x12C
    rl_fp_kdtree* m_kdtree; // 0x130
};

// size 0x38
struct rl_sub_instance_info
{
    rl_sub_instance_info() { m_disable_deferred = 0; } // 0x00711490 (.h:34)

    matrix43 m_transform; // 0x00
    unsigned int m_submesh_id; // 0x30
    unsigned int m_is_active : 1; // 0x34:0
    unsigned int m_disable_deferred : 1; // 0x34:1
};

// size 0x20
struct rl_sub_instance
{
    rl_sub_instance(); // 0x004E3D50

    rl_compressed_transform m_transform; // 0x00
    unsigned short m_instance_index; // 0x18
    unsigned short m_submesh_id; // 0x1A
    unsigned int m_is_visible : 1; // 0x1C:0
    unsigned int m_is_active : 1; // 0x1C:1
    unsigned int m_unused : 29; // 0x1C:2
    unsigned int m_disable_deferred : 1; // 0x1C:31
};

// size 0x3C
struct rl_sub_instance_update
{
    rl_sub_instance_update() {} // 0x00A13080 (only m_info's constructor)

    rl_sub_instance_info m_info; // 0x00
    int m_base_index; // 0x38
};
