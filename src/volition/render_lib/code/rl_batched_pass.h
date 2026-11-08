// Original: volition/render_lib/code/rl_batched_pass.h
// Implementation: volition/render_lib/code/rl_batched_pass.cpp
//
// rl_batched_pass, collects visible instances and decals per material for a pass (lock-free from several threads) and
// draws them batched.
#pragma once

#include <stdint.h>
#include "volition/render_lib/code/pool_list.h"
#include "volition/render_lib/code/rl_base_object.h"
#include "volition/render_lib/code/rl_lf_hash.h"

class mempool;
struct rl_batched_decal;
struct rl_batched_instance;
struct rl_batched_material_info;
class rl_camera;
class rl_class_factory_info;
class rl_mesh;
struct rl_mesh_instance_info;
class rl_mesh_instance_stream;
struct rl_render_group;
class rl_renderable_instance;
class rl_renderer;

// size 0x38
class rl_batched_pass : public rl_base_object
{
protected:
    rl_batched_pass(); // 0x00536DA0 (:55)
    virtual ~rl_batched_pass(); // vtbl+0x00 0x004BA750 (:74)
private:
    static const pool_list<rl_batched_pass*>& get_object_pool(); // 0x004BA230 (.h:83)
public:
    static void set_objects_max(int);
    static int get_objects_max(); // 0x004BA240 (.h:83)
    static int get_objects_used();
    static int get_objects_free();
    static void factory_init(mempool& factory_mempool); // 0x00509AE0 (:47)
    static rl_batched_pass* create_placement(rl_batched_pass* _data_p); // 0x00548E30 (:47)
    static rl_batched_pass* create(); // 0x00548DD0 (:47)
    static bool can_create(); // 0x004DF850 (:47)
    virtual void release(); // vtbl+0x04 0x00509BE0 (:47)
    virtual void begin_construction(); // vtbl+0x08 0x00555790 (:171)
    virtual void finalize_construction(rl_renderer& renderer); // vtbl+0x0C 0x00555800 (:185)
    int add_transform(const rl_mesh_instance_info& new_inst); // 0x00509C60 (:280)
    bool add_unbatched_renderable_instance(rl_renderable_instance* new_inst); // 0x004DF8D0 (:312)
    bool add_decals(rl_batched_decal* decals, int num_decals); // 0x004DF880 (:292)
    void add_material_stats(unsigned int mat_handle, const rl_mesh* mesh, unsigned int vis_flags, int num_instances); // 0x004DF910 (:331)
    rl_batched_instance* allocate_instances(unsigned int mat_handle, const rl_mesh* mesh, unsigned int vis_flags, int num_instances); // 0x004DF9E0 (:378)
    virtual void render(rl_renderer& renderer, rl_camera& camera, unsigned int vis_pass_flags, bool skip_non_batched_decals); // vtbl+0x10 0x00536E20 (:421)
    virtual void render_decals(rl_renderer& renderer); // vtbl+0x14 0x00520AF0 (:531)
private:
    void partition_batched_instances(); // 0x00555670 (:113)

public:
    static const unsigned int m___class_id; // 0x012BECB0
    static rl_class_factory_info ms_class_factory_info; // 0x01514730
protected:
    static pool_list<rl_batched_pass*> ms_object_pool; // 0x01514760
    volatile unsigned int m_num_instances; // 0x08
    rl_batched_instance* m_instances; // 0x0C
    rl_mesh_instance_stream* m_transform_stream; // 0x10
    volatile unsigned int m_num_unbatched_renderables; // 0x14
    rl_renderable_instance** m_unbatched_renderables; // 0x18
    volatile unsigned int m_num_decals; // 0x1C
    rl_batched_decal* m_decals; // 0x20
    volatile unsigned int m_num_batched_materials; // 0x24
    rl_batched_material_info* m_batched_materials; // 0x28
    rl_lf_static_hash<uint64_t,rl_batched_material_info,&rl_hash_uint64,2048>* m_batched_material_hash; // 0x2C
    volatile unsigned int m_num_render_groups; // 0x30
    rl_render_group* m_render_groups; // 0x34
};

bool sort_material_infos(const rl_batched_material_info& lhs, const rl_batched_material_info& rhs); // 0x004BA7B0 (:97)
bool sort_decals(const rl_batched_decal& lhs, const rl_batched_decal& rhs); // 0x004BA7E0 (:154)
void rl_batched_pass_get_instance_stats(int& high_watermark, int& max_capacity); // 0x004BA800 (:397)
void rl_batched_pass_get_unbatched_renderable_stats(int& high_watermark, int& max_capacity); // 0x004BA820 (:403)
void rl_batched_pass_get_decal_stats(int& high_watermark, int& max_capacity); // 0x004BA840 (:409)
void rl_batched_pass_get_material_stats(int& high_watermark, int& max_capacity); // 0x004BA860 (:415)
