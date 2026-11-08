// Original: volition/render_lib/code/rl_gpu_visibility.h
// Implementation: volition/render_lib/code/rl_gpu_visibility.cpp
//
// GPU visibility (occlusion queries per mesh instance; rl_gpu_mesh_instance, rl_gpu_vis_state). Placement inferred.
#pragma once


class rl_camera;
class rl_renderer;
class rl_scene_entity;
struct vis_queue_entry;

namespace keen
{
    struct GraphicsSystem;
}

enum rl_gpu_vis_state
{
    RL_GPU_VIS_STATE_NOT_READY = 0,
    RL_GPU_VIS_STATE_OCCLUDED = 1,
    RL_GPU_VIS_STATE_VISIBLE = 2,
};

// size 0x34
struct rl_gpu_mesh_instance
{
    float transform[12]; // 0x00
    unsigned int instance_params; // 0x30
};

void rl_gpu_vis_system_init(keen::GraphicsSystem* pGraphicsSystem); // 0x0053B1F0 (:111)
void rl_gpu_vis_system_shutdown(); // 0x004E7D50 (:225)
void rl_gpu_vis_render_mesh_query(rl_renderer& renderer, vis_queue_entry* entry_p); // 0x00524C00 (:304)
void rl_gpu_vis_render_debug(rl_renderer& renderer, rl_camera& camera); // 0x00524C80 (:330)
rl_gpu_vis_state rl_gpu_vis_get_state(rl_renderer& renderer, int handle); // 0x0050FAA0 (:375)
void rl_gpu_vis_alloc(rl_renderer& renderer, rl_camera& camera, rl_scene_entity* entity); // 0x0050FCA0 (:480)
void rl_gpu_vis_free(int handle); // 0x00524E70 (:533)
void rl_gpu_vis_set_enabled(bool enable); // 0x004C1ED0 (:540)
bool rl_gpu_vis_is_enabled(); // 0x004C1EE0 (:545)
void rl_gpu_vis_render(rl_renderer& renderer, rl_camera& camera); // 0x00524EA0 (:550)
void rl_gpu_vis_prerender(rl_camera& camera); // 0x005251F0 (:645)

// Not in the binary: a census of the query queue for tests (the viewer). The entries and queries in flight are the
// queue's state now; the other counts are since the previous call (the poll results by state, the queries issued, the
// entries found with the camera inside their box, the queue resets).
struct rl_gpu_vis_census
{
    int entries = 0;
    int queries_in_flight = 0;
    int occluded = 0;
    int visible = 0;
    int not_ready = 0;
    int queries_issued = 0;
    int camera_inside = 0;
    int resets = 0;
};

void rl_gpu_vis_take_census(rl_gpu_vis_census& census);
