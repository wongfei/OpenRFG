// Original: volition/render_lib/code/rl_batch_defines.h
//
// Batching records shared by the batched pass and the renderer (rl_batched_instance, rl_batched_decal,
// rl_batched_material_info, rl_render_block). Placement inferred.
#pragma once

#include "volition/vlib/code/util/bitarray.h"

class rl_clone_mesh_pool;
class rl_decal;
class rl_mesh;
class rl_renderable_instance;

// size 0x14
class rl_batched_clone_pool
{
public:
    rl_batched_clone_pool();
    ~rl_batched_clone_pool();

    rl_clone_mesh_pool* m_batched_pool; // 0x00
    bit_array m_batched_clones; // 0x04
    volatile unsigned int m_bit_guard; // 0x10
};

// size 0xC
class rl_batched_instance_block
{
public:
    rl_batched_instance_block();
    ~rl_batched_instance_block();

    rl_renderable_instance* m_inst; // 0x00
    unsigned int* m_sub_ids; // 0x04
    int m_num_instances; // 0x08
};

// size 0x10
struct rl_batched_decal
{
    unsigned int material_handle; // 0x00
    rl_renderable_instance* owner; // 0x04
    unsigned int decal_handle; // 0x08
    rl_decal* decal; // 0x0C
};

// size 0x10
struct rl_batched_instance
{
    int transform_index; // 0x00
    int first_index; // 0x04
    int num_indices; // 0x08
    unsigned int block_id; // 0x0C
};

// size 0x14
struct rl_batched_material_info
{
    unsigned int material_handle; // 0x00
    const rl_mesh* base_mesh; // 0x04
    unsigned int visibility_flags; // 0x08
    volatile unsigned int num_instances; // 0x0C
    volatile unsigned int first_instance; // 0x10
};

// size 0x14
struct rl_render_block
{
    unsigned short material_map_idx; // 0x00
    unsigned int start_index; // 0x04
    unsigned int num_indices; // 0x08
    unsigned int minimum_index; // 0x0C
    unsigned int maximum_index; // 0x10
};
