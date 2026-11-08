// Original: volition/render_lib/code/rl_render_group.h
// Implementation: volition/render_lib/code/rl_render_group.cpp
//
// rl_render_group, instances of one material and geometry drawn together.
#pragma once

#include "volition/render_lib/code/rl_mesh_data.h"

struct rl_batched_instance;
class rl_compiled_render_group;

namespace keen
{
    struct RenderGeometry;
}

// size 0x30
struct rl_render_group
{
    rl_render_group(); // 0x004BA740

    unsigned int material_handle; // 0x00
    unsigned int visibility_flags; // 0x04
    rl_batched_instance* instances; // 0x08
    unsigned int num_instances; // 0x0C
    rl_vertex_buffer_data_runtime vbuffer_info; // 0x10
    rl_index_buffer_data_runtime ibuffer_info; // 0x1C
    const keen::RenderGeometry* geometry_p; // 0x28
    rl_compiled_render_group* compiled_group; // 0x2C
};
