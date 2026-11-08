// Original: volition/render_lib/code/rl_renderer_core.h
// Implementation: volition/render_lib/code/rl_renderer_core.cpp
//
// rl_init_params (render_lib startup settings) and the deferred operations queued by the simulation thread for the
// render thread (rl_deferred_operation, rl_buffered_command). Placement of the operation types inferred.
#pragma once

#include "keen/base/memory.hpp"
#include "volition/render_lib/code/rl_primitive_defines.h"
#include "volition/render_lib/code/rl_screen_info.h"

class rl_material_base;
class rl_scene;

namespace keen
{
    struct FileSystem;
    struct GraphicsSystem;
    class MemoryAllocator;
}

enum rl_buffered_command_type
{
    RLBC_BEGIN = 0,
    RLBC_END = 1,
    RLBC_DRAW_PRIM = 2,
    RLBC_SET_STATE = 3,
    RLBC_FLUSH = 4,
    NUM_RL_BUFFERED_COMMAND_TYPES = 5,
};

enum rl_deferred_operation_type
{
    RLDO_NOP = -1,
    RLDO_SCENE_ADD_RI = 0,
    RLDO_SCENE_ADD_LIGHT = 1,
    RLDO_SCENE_REMOVE_RI = 2,
    RLDO_SCENE_REMOVE_LIGHT = 3,
    RLDO_CREATE_DECAL = 4,
    RLDO_RELEASE_DECAL = 5,
    RLDO_RELEASE_OBJECT = 6,
    RLDO_SET_FRAMEBUFFER_RESOLVE = 7,
    RLDO_SCENE_ADD_LOW_LOD = 8,
    RLDO_SCENE_REMOVE_LOW_LOD = 9,
    RLDO_SCENE_ADD_LANDMARK_LOD = 10,
    RLDO_SCENE_REMOVE_LANDMARK_LOD = 11,
    RLDO_OVERRIDE_MATERIAL_HANDLE = 12,
    NUM_RL_DEFERRED_SCENE_OPERATION_TYPES = 13,
};

// size 0x8
class rl_deferred_operation
{
public:
    rl_deferred_operation(); // 0x004C5BA0 (:49)
    void init(rl_deferred_operation_type type, void* data_p); // 0x004C5BB0 (:51)

    rl_deferred_operation_type m_type; // 0x00
    void* m_data; // 0x04
};

// size 0x164
class rl_init_params
{
public:
    rl_init_params(); // 0x00459660 (.h:81)

    unsigned int m_uses_screen_info : 1; // 0x00:0
    unsigned int m_override_width : 1; // 0x00:1
    unsigned int m_vsync_enabled : 1; // 0x00:2
    unsigned int m_override_height; // 0x04
    rl_screen_info m_screen_info; // 0x08
    rl_level_of_detail m_lod; // 0xA8
    void (*m_memtracker_start_cb)(const char*); // 0x13C
    void (*m_memtracker_end_cb)(bool); // 0x140
    keen::GraphicsSystem* m_pKeenGraphicsSystem; // 0x144
    keen::MemoryAllocator* m_pDefaultAllocator; // 0x148
    keen::MemoryAllocator* m_pBufferAllocator; // 0x14C
    keen::MemoryAllocator* m_pVramAllocator; // 0x150
    keen::MemoryAllocator* m_pFramebufferAllocator; // 0x154
    keen::FileSystem* m_pFileSystem; // 0x158
    keen::MemoryBlock m_occlusionQueryGPUMemory; // 0x15C
};

// size 0x18
struct rl_buffered_command
{
    unsigned int id; // 0x00
    rl_buffered_command_type type; // 0x04
    int data_offset; // 0x08
    int prim_count; // 0x0C
    int num_data_segments; // 0x10
    rl_primitive_render_type prim_type; // 0x14
};

// size 0xC
struct rl_deferred_batch_item
{
    unsigned int def_id; // 0x00
    float dist_sq; // 0x04
    bool opacity_fade; // 0x08
};

// size 0x8
struct rl_deferred_material_handle_override
{
    unsigned int mat_h; // 0x00
    rl_material_base* mat_p; // 0x04
};

extern rl_init_params Init_params; // 0x01C20A60
extern rl_scene* Active_scene_p; // 0x01B7BD04

bool rl_is_initialized(); // 0x004C5BC0 (:76)
void factory_init(); // 0x004F3120 (:102)
void factory_shutdown(); // 0x004C5BF0 (:138)
void process_deferred_op(rl_deferred_operation_type type, void* data); // 0x0054D200 (:290)
void rl_fail_initialization(); // 0x004C5BD0 (:81)
void rl_track_mem_usage_start(const char* where); // 0x004C5C00 (:144)
void rl_track_mem_usage_end(bool quiet); // 0x004C5C10 (:151)
void rl_init(rl_init_params& params); // 0x005508E0 (:158)
void rl_shutdown(); // 0x0054D100 (:230)
keen::GraphicsSystem* rl_get_graphics_system(); // 0x004C5C20 (:260)
keen::MemoryAllocator* rl_get_buffer_allocator(); // 0x004C5C30 (:265)
keen::MemoryAllocator* rl_get_default_allocator(); // 0x004C5C40 (:270)
keen::FileSystem* rl_get_filesystem(); // 0x004C5C50 (:275)
void rl_set_active_scene(rl_scene* scene_p); // 0x004C5C60 (:280)
rl_scene* rl_get_active_scene(); // 0x004C5C70 (:285)
void rl_unlock(); // 0x0054D2F0 (:334)
void rl_lock(); // 0x004C5C80 (:351)
bool rl_is_locked(); // 0x004C5C90 (:356)
void rl_add_deferred_op(rl_deferred_operation_type type, void* data); // 0x0052DC50 (:362)
void rl_set_wait_for_unlock_cb(void (*callback)()); // 0x004C5CA0 (:429)
