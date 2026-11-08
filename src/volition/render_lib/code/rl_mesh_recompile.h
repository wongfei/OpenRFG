// Original: volition/render_lib/code/rl_mesh_recompile.h
// Implementation: volition/render_lib/code/rl_mesh_recompile.cpp
//
// Mesh recompilation (rebuilding vertex data at run time, for example after destruction), queued on an spsc_queue.
// spsc_queue<T> is written by hand; the addresses are of the <rl_mesh_recompile_parameters*> instantiation.
#pragma once

#include "library/modules/runtime/graphics/sources/d3d11/rendergeometry_d3d11.hpp"
#include "volition/render_lib/code/rl_device.h"

class rl_destroyable_model_instance;
struct rl_render_block;

// size 0x4C
struct rl_mesh_recompile_data
{
    rl_mesh_recompile_data(); // 0x004C3140 (:237)
    ~rl_mesh_recompile_data(); // 0x004E98C0 (:242)

    keen::RenderGeometry geometry; // 0x00
    unsigned short vertex_stride; // 0x2C
    unsigned short index_stride; // 0x2E
    unsigned int num_verts; // 0x30
    rl_dev_vertex_format vertex_format; // 0x34
    rl_dev_primitive_topology prim_type; // 0x38
    unsigned int num_render_blocks; // 0x3C
    rl_render_block* render_blocks_p; // 0x40
    void* index_data_p; // 0x44
    void* vertex_data_p; // 0x48
};

// size 0xC
struct rl_mesh_recompile_parameters
{
    rl_destroyable_model_instance* m_instance_p; // 0x00
    rl_mesh_recompile_data* m_recompile_data_p; // 0x04
    volatile unsigned int m_valid; // 0x08
};

// size 0xC
template<typename T>
class spsc_queue
{
public:
    // size 0x8
    struct node
    {
        node(const T& datum); // 0x004D7BF0 (.h:49)

        T val; // 0x00
        node* next_p; // 0x04
    };

    spsc_queue(); // 0x004D5D30 (.h:62)
    void create(); // 0x00504600 (.h:68)
    void destroy(); // 0x004D5D40 (.h:74)
    void enqueue(const T& t); // 0x00504640 (.h:84)
    bool dequeue(T& result); // 0x004D5D60 (.h:100)

private:
    node* first_p; // 0x00
    node* divider_p; // 0x04
    node* last_p; // 0x08
};

// template instance: void __thiscall spsc_queue<rl_mesh_recompile_parameters*>::spsc_queue<rl_mesh_recompile_parameters*>(); // 0x004D5D30 (.h:62)
extern bool Rl_recompile_mesh_enable; // 0x014FA134
extern volatile unsigned int Rl_recompile_mesh_pause_count; // 0x01B7BCDC

void rl_delete_mesh_recompile_data(rl_mesh_recompile_data* pData); // 0x00528CD0 (:43)
void rl_mesh_recompile_block(); // 0x004C30E0 (:62)
void rl_mesh_recompile_pause(); // 0x004C3100 (:70)
void rl_mesh_recompile_unpause(); // 0x004C3120 (:79)
int mesh_recompile_thread(void*); // 0x00512720 (:88)
void rl_mesh_recompile_initialize(); // 0x00528D80 (:113)
void rl_mesh_recompile_finalize(); // 0x00528EC0 (:142)
void rl_mesh_recompile_shutdown(); // 0x004E9840 (:188)
void rl_start_recompile_mesh_instance(rl_destroyable_model_instance* instance_p, bool initing); // 0x005290D0 (:201)
