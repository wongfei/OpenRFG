// Original: volition/render_lib/code/rl_destroyable_model_instance.h
// Implementation: volition/render_lib/code/rl_destroyable_model_instance.cpp
//
// rl_destroyable_model_instance, the render side of destructible objects: subpieces shown/hidden and split as the
// object breaks (shared base per model, post_split_info).
#pragma once

#include "volition/render_lib/code/pool_list.h"
#include "volition/render_lib/code/rl_base_object.h"
#include "volition/render_lib/code/rl_compound_mesh_instance.h"

class mempool;
class rl_camera;
class rl_class_factory_info;
class rl_decal;
class rl_decal_parameters;
class rl_destroyable_model_instance;
class rl_fp_kdtree;
struct rl_fp_kdtree_base_data;
struct rl_fp_kdtree_leaf;
struct rl_fp_kdtree_node;
class rl_material_map;
class rl_mesh;
struct rl_mesh_data_runtime;
struct rl_mesh_recompile_data;
struct rl_mesh_recompile_parameters;
class rl_renderable;
class rl_renderer;
struct rl_split_info;
struct split_base_array_params;
class split_memmgr;

// size 0x3C
class rl_destroyable_model_instance_shared_base : public rl_base_object
{
protected:
    rl_destroyable_model_instance_shared_base(); // 0x004BDD30 (.h:226)
    virtual ~rl_destroyable_model_instance_shared_base(); // vtbl+0x00 0x004E4140 (.h:242)
private:
    static const pool_list<rl_destroyable_model_instance_shared_base*>& get_object_pool(); // 0x004BDC90 (.h:52)
public:
    static void set_objects_max(int objects_max); // 0x007C0B10 (.h:52)
    static int get_objects_max(); // 0x004BDCA0 (.h:52)
    static int get_objects_used();
    static int get_objects_free();
    static void factory_init(mempool& factory_mempool); // 0x0050CA20 (:33)
    static rl_destroyable_model_instance_shared_base* create_placement(rl_destroyable_model_instance_shared_base* _data_p); // 0x004BDD60 (:33)
    static rl_destroyable_model_instance_shared_base* create(); // 0x00523E80 (:33)
    static bool can_create(); // 0x004E41D0 (:33)
    virtual void release(); // vtbl+0x04 0x0050CB20 (:33)
    bool init(rl_fp_kdtree_base_data* base_kdtree, int num_sub_instances); // 0x0050CCA0 (:475)
    void build_rbb(const rl_mesh_data_runtime* mesh_data); // 0x0053A090 (:562)
    unsigned int add_instance(rl_destroyable_model_instance* new_inst); // 0x004BDE50 (:449)
    void remove_instance(rl_destroyable_model_instance* inst); // 0x004BDED0 (:463)
    void update_unclaimed_rbb_inst_ids(); // 0x004BDF50 (:615)
    int find_base_sub_instance_index(int orig_sub_inst_index); // 0x004BE0E0 (:1304)
    void maybe_reset_validation_cache(); // 0x004BE1A0 (:1340)
private:
    void increment_ref_count(); // 0x004BDE00 (:90)
    void decrement_ref_count(); // 0x004BDE10 (:96)

public:
    static const unsigned int m___class_id; // 0x012BEDCC
    static rl_class_factory_info ms_class_factory_info; // 0x015149F0
protected:
    static pool_list<rl_destroyable_model_instance_shared_base*> ms_object_pool; // 0x01514A20
public:
    unsigned short* m_sub_instance_index_map; // 0x08
    unsigned short* m_sub_instance_original_index_map; // 0x0C
    unsigned short* m_sub_instance_leaf_index_map; // 0x10
    rl_sub_instance* m_base_sub_instances; // 0x14
    rl_fp_kdtree_leaf* m_kdtree_leaves; // 0x18
    rl_fp_kdtree_node* m_kdtree_nodes; // 0x1C
    rl_fp_kdtree* m_kdtree; // 0x20
    int m_num_base_sub_instances; // 0x24
    int m_num_unclaimed; // 0x28
    unsigned int m_total_instances_added; // 0x2C
    rl_destroyable_model_instance* m_instance_list; // 0x30
private:
    int m_ref_count; // 0x34
    rl_fp_kdtree_base_data* m_base_kdtree; // 0x38
};

// size 0x160
class rl_destroyable_model_instance : public rl_compound_mesh_instance
{
protected:
    rl_destroyable_model_instance(); // 0x005499A0 (.h:321)
    virtual ~rl_destroyable_model_instance(); // vtbl+0x00 0x00549A20 (.h:336)
private:
    static const pool_list<rl_destroyable_model_instance*>& get_object_pool(); // 0x004BDCB0 (.h:113)
public:
    static void set_objects_max(int objects_max); // 0x007C0B20 (.h:113)
    static int get_objects_max(); // 0x004BDCC0 (.h:113)
    static int get_objects_used(); // 0x00723860 (.h:113)
    static int get_objects_free(); // 0x0071B5D0 (.h:113)
    static void factory_init(mempool& factory_mempool); // 0x0050CBA0 (:34)
    static rl_destroyable_model_instance* create_placement(rl_destroyable_model_instance*);
    static rl_destroyable_model_instance* create(); // 0x00549AF0 (:34)
    static bool can_create(); // 0x004E41E0 (:34)
    virtual void release(); // vtbl+0x04 0x0053A010 (:34)
protected:
    void init(rl_renderable* mesh_p, const rl_material_map* mat_map_p, rl_destroyable_model_instance_shared_base* shared_base, rl_sub_instance* sub_insts, int num_sub_instances, rl_decal* decal_list); // 0x0054A090 (:801)
public:
    void init(rl_destroyable_model_instance* sibling, short* sub_inst_indices, int num_sub_insts); // 0x00549E50 (:735)
    bool init(rl_mesh* mesh_p, const rl_material_map* mat_map_p, rl_fp_kdtree_base_data* base_kdtree, rl_sub_instance_info* sub_inst_info_p, int num_sub_inst_infos, short* sub_inst_indices, int num_sub_insts); // 0x00549B50 (:639)
    virtual void render_debug(); // vtbl+0x18 0x0055EF70 (:1383)
    void update_instances(rl_sub_instance_update* updates_p, int num_updates); // 0x0054A200 (:851)
    virtual void get_sub_instance_info(rl_sub_instance_info& sub_info, int base_index) const; // vtbl+0x58 0x004BDFA0 (:885)
    void set_sub_instance_is_active(short* sub_inst_indices, int num_sub_insts, bool is_active, bool skip_renderable_update); // 0x0054A300 (:905)
    void unclaim_sub_instances(short* sub_inst_indices, int num_sub_instances); // 0x0054A3E0 (:938)
    void claim_sub_instances(short* sub_inst_indices, int num_sub_instances); // 0x0054A540 (:990)
    rl_destroyable_model_instance* split(const rl_split_info& split_info); // 0x0054A630 (:1039)
    void fade_decals_on_sub_instance(int sub_instance_index, int fadetime_ms); // 0x0050CFA0 (:1257)
    unsigned int get_decal_ri_handle(int sub_instance_index); // 0x004BE0A0 (:1271)
    static int calculate_memory_cost(int num_sub_instances, bool dynamic_kdtree); // 0x004BE0C0 (:1279)
    bool can_claim_sub_instance(int sub_inst_index); // 0x004BE1C0 (:1355)
    bool owns_sub_instance(int sub_inst_index); // 0x004BE200 (:1365)
    unsigned int get_base_uid(); // 0x004BE250 (:1377)
protected:
    virtual int find_decal_sub_insts(int* out_indices, int max_out_indices, const rl_decal_parameters* decal_params); // vtbl+0x5C 0x0050CD90 (:1197)
    void release_sub_instances_internal(rl_sub_instance* sub_insts, int num_sub_instances); // 0x004E41F0 (:108)
    void aquire_sub_instances_internal(rl_sub_instance* sub_insts, int num_sub_instances); // 0x004E44A0 (:202)
    rl_destroyable_model_instance_shared_base* get_shared_data(); // 0x004BDCD0 (.h:180)
    void set_internal_arrays(rl_sub_instance* new_sub_instances); // 0x004BDCE0 (.h:183)
    void get_internal_arrays(rl_sub_instance** sub_instances); // 0x004BDCF0 (.h:184)
private:
    void update_rbb_inst_ids(); // 0x004BE020 (:1097)
    void partition_renderable_sub_instances(); // 0x004E49D0 (:1116)
public:
    void invalidate_compiled_data_cache(bool initing); // 0x0053A2E0 (:1443)
    void set_recompile_parameters(rl_mesh_recompile_parameters* params_p); // 0x004BDD00 (.h:209)
    rl_mesh_recompile_data* set_compiled_cache_data(rl_mesh_recompile_data* data_p, rl_mesh_recompile_parameters* params_p); // 0x004BEAC0 (:1908)
    virtual void render(rl_renderer& renderer, rl_camera& camera, unsigned int flags); // vtbl+0x10 0x0053A330 (:1933)
    rl_mesh_recompile_data* compile_cache_data(rl_mesh_recompile_parameters* params_p); // 0x0050D070 (:1527)

    static const unsigned int m___class_id; // 0x012BEDD0
    static rl_class_factory_info ms_class_factory_info; // 0x01514A38
protected:
    static pool_list<rl_destroyable_model_instance*> ms_object_pool; // 0x01514A68
public:
    rl_destroyable_model_instance* prev; // 0x140
    rl_destroyable_model_instance* next; // 0x144
private:
    rl_destroyable_model_instance_shared_base* m_shared_data; // 0x148
    unsigned int m_inst_id; // 0x14C
    static const unsigned int kWasBatchable; // 0x012BEDD4
    unsigned int m_flags; // 0x150
    rl_mesh_recompile_parameters* m_recompile_parameters_p; // 0x154
    rl_mesh_recompile_data* m_recompile_data_p; // 0x158
};

// size 0x4
struct post_split_info
{
    bool get_moved(); // 0x004BDDA0 (:76)
    bool get_split(); // 0x004BDDB0 (:77)
    void set_moved(bool moved); // 0x004BDDC0 (:78)
    void set_split(bool split); // 0x004BDDE0 (:79)

    unsigned short flags; // 0x00
    unsigned short new_index; // 0x02
};

// size 0x18
struct rl_split_info
{
    int m_num_sub_insts_to_split; // 0x00
    short* m_sub_insts_to_split; // 0x04
    int m_num_sub_insts_to_disable; // 0x08
    short* m_sub_insts_to_disable; // 0x0C
    int m_num_sub_insts_to_enable; // 0x10
    short* m_sub_insts_to_enable; // 0x14
};

int int16_compare(const void* arg1, const void* arg2); // 0x004BDE30 (:282)
void split_base_array_interal(split_base_array_params& new_params, split_base_array_params& old_params, rl_destroyable_model_instance_shared_base* shared_data, short* indices_to_split, int num_indices, unsigned int parent_inst_id); // 0x004E46A0 (:302)
bool check_recompile_abort(rl_mesh_recompile_parameters* params_p); // 0x004E4BF0 (:1517)
void rl_destroyable_model_instance_system_init(split_memmgr* mgr, void (*allocation_stats)(int)); // 0x004BEB10 (:2030)
