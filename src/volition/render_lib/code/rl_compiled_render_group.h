// Original: volition/render_lib/code/rl_compiled_render_group.h
// Implementation: volition/render_lib/code/rl_compiled_render_group.cpp
//
// rl_compiled_render_group, a render group compiled into instanced draw calls.
#pragma once

#include "library/modules/runtime/graphics/sources/d3d11/geometryinstancing_d3d11.hpp"
#include "volition/render_lib/code/pool_list.h"
#include "volition/render_lib/code/rl_base_object.h"

class mempool;
struct rl_batched_instance;
class rl_class_factory_info;
struct rl_index_buffer_data_runtime;
class rl_mesh_instance_stream;
struct rl_render_group;
class rl_renderer;
struct rl_vertex_buffer_data_runtime;

namespace keen
{
    class MemoryAllocator;
    struct RenderGeometry;
}

// size 0x18
struct rl_compiled_render_call
{
    int m_first_index; // 0x00
    int m_num_indices; // 0x04
    int m_instance_offset; // 0x08
    int m_num_instances; // 0x0C
    rl_compiled_render_call* prev; // 0x10
    rl_compiled_render_call* next; // 0x14
};

// size 0x1
struct sort_by_block_id
{
    bool operator()(const rl_batched_instance& pA, const rl_batched_instance& pB) const; // 0x004BC7E0 (:100)

};

// size 0x30
class rl_compiled_render_group : public rl_base_object
{
protected:
    rl_compiled_render_group(); // 0x004BC800 (:115)
    virtual ~rl_compiled_render_group(); // vtbl+0x00 0x004BC820 (:124)
private:
    static const pool_list<rl_compiled_render_group*>& get_object_pool(); // 0x004BA720 (.h:40)
public:
    static void set_objects_max(int);
    static int get_objects_max(); // 0x004BA730 (.h:40)
    static int get_objects_used();
    static int get_objects_free();
    static void factory_init(mempool& factory_mempool); // 0x0050AB40 (:112)
    static rl_compiled_render_group* create_placement(rl_compiled_render_group* _data_p); // 0x004E3630 (:112)
    static rl_compiled_render_group* create(); // 0x00522070 (:112)
    static bool can_create(); // 0x004E3660 (:112)
    virtual void release(); // vtbl+0x04 0x0050AC40 (:112)
    virtual void build(rl_renderer& renderer, rl_render_group* group, const rl_mesh_instance_stream* inst_stream); // vtbl+0x08 0x005559B0 (:145)
    virtual void render(rl_renderer& renderer); // vtbl+0x0C 0x005220E0 (:593)
    static void render_immediate_begin(rl_renderer& renderer, rl_render_group* group, const rl_mesh_instance_stream* inst_stream); // 0x0050ACC0 (:666)
    static void render_immediate(rl_renderer& renderer, rl_render_group* grp_p, rl_batched_instance* inst_p); // 0x00522330 (:693)
    static void render_immediate_end(rl_renderer&); // 0x004BCA00 (:736)

    static const unsigned int m___class_id; // 0x012BECB4
    static rl_class_factory_info ms_class_factory_info; // 0x015148B8
protected:
    static pool_list<rl_compiled_render_group*> ms_object_pool; // 0x015148E8
private:
    const rl_vertex_buffer_data_runtime* m_vbuffer_info; // 0x08
    const rl_index_buffer_data_runtime* m_ibuffer_info; // 0x0C
    const keen::RenderGeometry* m_geometry_p; // 0x10
    keen::GeometryInstancingBatch m_instancing_batch; // 0x14
    int m_instance_offset; // 0x24
    rl_compiled_render_call* m_render_calls; // 0x28
    bool m_is_built; // 0x2C
};

void rl_compiled_render_group_get_call_stats(int& high_watermark, int& max_capacity); // 0x004BCA20 (:752)
void rl_compiled_render_group_reset_call_stats(); // 0x004BCA40 (:758)
void rl_compiled_render_group_init(keen::MemoryAllocator* pAllocator); // 0x004E3690 (:763)
void rl_compiled_render_group_reset(); // 0x004BCA50 (:770)
