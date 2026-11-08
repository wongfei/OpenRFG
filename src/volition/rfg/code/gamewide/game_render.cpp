// Original: volition/rfg/code/gamewide/game_render.cpp
//
// The game's render driver on top of render_lib: system and level init/shutdown (the main scene with its four
// quadtrees, the scene renderer and terrain renderer), the per-frame handshake on the simulation thread
// (game_render_setup_frame: the render camera, fog, color correction, HDR, ambient and xray state, the update-render
// tasks and the render offset) and the frame on the render thread (render_frame: the scene, then the deferred
// primitives), the render task lists, the visibility and culling helpers and the scene state setters.
//
// Partial port (render_minimal.md V3). Not ported: the console commands (unused_dcf_*), the debug drawing
// (game_render_2d_text, game_render_spline), game_render_frame (the HUD and interface passes) and
// game_render_unproject. The profile counters (Game_render_stats) are not recovered: their writes are left out (see
// the comments in place). game_render_system_init sets every pool maximum rfg.exe sets (final_push.md step 5 added
// the vfx particle pools, the renderable particles, the fog volume and the two morph factories).

#include "volition/rfg/code/gamewide/game_render.h"

#include "volition/render_lib/code/rl_camera.h"
#include "volition/render_lib/code/rl_color.h"
#include "volition/render_lib/code/rl_color_correct_state.h"
#include "volition/render_lib/code/rl_compound_mesh_instance.h"
#include "volition/render_lib/code/rl_deformable_mesh_instance.h"
#include "volition/render_lib/code/rl_destroyable_model_instance.h"
#include "volition/render_lib/code/rl_fog_state.h"
#include "volition/render_lib/code/rl_fog_volume.h"
#include "volition/render_lib/code/rl_fp_kdtree.h"
#include "volition/render_lib/code/rl_frustum.h"
#include "volition/render_lib/code/rl_hdr_state.h"
#include "volition/render_lib/code/rl_light.h"
#include "volition/render_lib/code/rl_material_map.h"
#include "volition/render_lib/code/rl_mesh.h"
#include "volition/render_lib/code/rl_mesh_instance.h"
#include "volition/render_lib/code/rl_morph_instance.h"
#include "volition/render_lib/code/rl_morph_target.h"
#include "volition/render_lib/code/rl_particle_lighting_state.h"
#include "volition/render_lib/code/rl_particle_renderer.h"
#include "volition/render_lib/code/rl_primitive_renderer.h"
#include "volition/render_lib/code/rl_primitive_state.h"
#include "volition/render_lib/code/rl_quadtree.h"
#include "volition/render_lib/code/rl_quadtree_sector.h"
#include "volition/render_lib/code/rl_render_lib.h"
#include "volition/render_lib/code/rl_renderable_particles.h"
#include "volition/render_lib/code/rl_renderer.h"
#include "volition/render_lib/code/rl_renderer_core.h"
#include "volition/render_lib/code/rl_scene.h"
#include "volition/render_lib/code/rl_scene_renderer.h"
#include "volition/render_lib/code/rl_skin_instance.h"
#include "volition/render_lib/code/rl_skirt_mesh_instance.h"
#include "volition/render_lib/code/rl_sphere.h"
#include "volition/render_lib/code/rl_terrain_renderer.h"
#include "volition/render_lib/code/rl_visibility_manager.h"
#include "volition/rfg/code/camera/rfg_camera.h"
#include "volition/rfg/code/game_render/game_render_task.h"
#include "volition/rfg/code/game_render_terrain.h"
#include "volition/rfg/code/level/world.h"
#include "volition/rfg/code/objects/humans/player.h"
#include "volition/rfg/code/objects/object_light.h"
#include "volition/rfg/code/objects/object_zone.h"
#include "volition/rfg/code/rfg.h"
#include "volition/vfxlib/code/vfx_particle_system_manager.h"
#include "volition/vlib/code/math/vector2.h"
#include "volition/vlib/code/os/frametime.h"
#include "volition/vlib/code/os/timer.h"

#include <math.h>

// hud_ui_rail_driver.cpp, xray_effect.cpp (not recovered).
vector2* hud_ui_rail_driver_get_xray_verts(int& num_verts); // 0x00BA2E60
void xray_effect_level_shutdown(); // 0x00A5D0F0

// ---- globals

// The debug free camera's input axes (strafe, forward, heading, pitch). Placement inferred: its owner (the debug input
// code) is not recovered.
float DebugAxisInput[4]; // 0x01A37830

rl_scene* Scene_main_p = nullptr; // 0x02533894
static bool Render_system_init = false; // 0x025338BD
static bool Ugly_mode = false; // 0x025338BE
static int Scene_ref_count = 0; // 0x025338D4
static int Vsync_request_count = 0; // 0x025338B4
static unsigned int Wait_for_prev_done_frame = 0; // 0x025338E0

// The render task lists: pre-stream (before stream2_do_frame), update-render (in the handshake) and debug.
static linked_list_p<game_render_task> Task_list_pre_stream_render; // 0x02535EB0
static linked_list_p<game_render_task> Task_list_update_render; // 0x02534A18
static linked_list_p<game_render_task> Task_list_debug_render; // 0x0253C89C

// The dynamic lights of the frame (released every frame).
static farray<rl_light*, 16> Dynamic_lights; // 0x0165E8C8

// The scissor region of the main renderer.
static rl_rect Scissor_region; // 0x02535EA0

// Fog.
static bool Fog_enabled = true; // 0x0165CBEA
static bool r_fog_override = false; // 0x025338C2
static rl_color_float Fog_color(1.0f, 0.75f, 0.44f, 1.0f); // 0x0165E8B8
static float Fog_density = 1.0f; // 0x0165CBF8
static float Fog_density_offset = 1.0f; // 0x0165CBFC
static float Fog_ground = 50.0f; // 0x0165CC00
static float Fog_atmosphere_scale = 260.0f; // 0x0165CC04
static float Fog_density_override_scale = 1.0f; // 0x0165CC08
// The fog ground (and density) following the camera height.
static bool R_fog_ground_camera = false; // 0x025338C3
static float R_fog_ground_camera_min = -1000.0f; // 0x0165CC6C
static float R_fog_ground_camera_max = 3.0f; // 0x0165CC70
static bool R_fog_density_camera = false; // 0x025338C8
static float R_fog_density_scale = 1.0f; // 0x0165CC54
static float R_fog_density_min = 0.001f; // 0x0165CC58
static float R_fog_density_max = 0.01f; // 0x0165CC5C
static float R_fog_height_density_min = 0.0f; // 0x025338CC
static float R_fog_height_density_max = 1000.0f; // 0x0165CC60

// Color correction.
static rl_color_float Scene_main_tint(1.0f, 1.0f, 1.0f, 1.0f); // 0x0165E918
static rl_color_float Scene_main_tint_modulate(1.0f, 1.0f, 1.0f, 1.0f); // 0x0165E928
static float Scene_main_saturation = 1.0f; // 0x0165CC0C
static float Scene_main_saturation_modulate = 1.0f; // 0x0165CC10
static float Levels_pregamma_offset = 0.0f; // 0x02533898
static float Levels_pregamma_scale = 1.0f; // 0x0165CC14
static float Levels_postgamma_offset = 0.0f; // 0x0253389C
static float Levels_postgamma_scale = 1.0f; // 0x0165CC18
static float Gamma = 1.0f; // 0x0165CC1C
static float Last_color_correct_gamma = -1.0f; // 0x0165CC20
static float Color_correct_gamma = 2.4f; // 0x0165CC24
static float Color_correct_brightness = 1.0f; // 0x0165CC28
static float Color_correct_contrast = 0.0f; // 0x025338A0
static float Color_correct_bloom_exposure_factor = 1.0f; // 0x0165CC2C

// HDR.
static rl_hdr_state Hdr_state; // 0x02534AD8
static float Bloom_override_exposure = 10.0f; // 0x0165CC48
static vector Sun_shafts_position(-10000.0f, -10000.0f, -10000.0f); // 0x0165E938

// Camera effects.
static bool Diffraction_stealth_enabled = false; // 0x0253388F
static bool Diffraction_scope_enabled = false; // 0x025338A4
static float Camera_frame_override_blur = 0.0f; // 0x025338A8
static float Camera_signal_noise_frequency = 0.0f; // 0x025338AC
static float Camera_signal_noise_scale = 0.0f; // 0x025338B0
static bool Motion_blur_enabled = false; // 0x025338A5
static float Motion_blur_amount = 1.0f; // 0x0165CC30
static vector2 Radial_blur_position(0.0f, 0.0f); // 0x025339C0
// The radial blur's strength and radius (placement inferred: also written by the radial blur effects, not recovered).
float Radial_blur_strength = 0.0f; // 0x0227AFBC
float Radial_blur_radius = 0.0f; // 0x02532C2C
static rl_particle_lighting_state Particle_lighting_state; // 0x02534504

// Decals are made (VFX.md FX7: read by object_decal.cpp only, never written in rfg.exe; placement inferred from the
// data layout).
bool R_enable_decals = true; // 0x0165CC74

// X-ray.
// The x-ray effect is available (placement inferred from the data layout).
bool Rl_xray_effect_enabled = true; // 0x0165CC75
static bool Xray_effect_enabled = false; // 0x025338A7
static float Xray_scene_saturation = 1.0f; // 0x0165CC34
static bool Xray_use_terrain_to_occlude = false; // 0x025338BC
static float Xray_fade_start = 40.0f; // 0x0165CC38
static float Xray_fade_dist = 40.0f; // 0x0165CC3C

static unsigned int Distortion_effect_material_handle_render = 0xFFFFFFFF; // 0x0165CC40
static float Time_of_day_onoff_value = 1.0f; // 0x0165CC44

// Opacity fade distances of the main scene (scoped: everything opaque).
static float Opacity_fade_start = 65.0f; // 0x0165CBF0
static float Opacity_fade_end = 75.0f; // 0x0165CBF4

// The batch render offset (the camera zone's center) keeps the shader translations small.
bool R_enable_batch_render_offset = true; // 0x0165CBEB

// The camera light (disabled in retail: r_camlight prints "Camlights are disabled").
static rl_light* Camlight_p = nullptr; // 0x025338C4
static bool Camlight_freeze = false; // 0x025338C1
static vector R_camlight_color(1.0f, 0.2f, 0.2f); // 0x0165E944
static float R_camlight_atten_start = 2.0f; // 0x0165CC4C
static float R_camlight_atten_end = 10.0f; // 0x0165CC50

// The debug free camera.
static bool s_freeCamera = false; // 0x025338C9
static bool s_currentlyOnFreeCamera = false; // 0x025338CA
static vector s_freeCamPos; // 0x02534B54
static float s_freeCamP = 0.0f; // 0x025111E4
static float s_freeCamB = 0.0f; // 0x025054C0
static float s_freeCamH = 0.0f; // 0x0253290C

extern bool Camera_third_person_lock; // 0x01DE48DB (rfg_camera_render.cpp)

// Not in the binary: rl_scene_entity's position and rotation setters (dirty flag, partition update), as the camera
// code inlines them.
static void set_entity_transform(rl_scene_entity* entity_p, const vector& position, const matrix& rotation)
{
    entity_p->set_position(position);
    entity_p->set_rotation(rotation);
}

// 0x007CF240 (:504)
// The scene lock wait callback: the frame waits for the previous one, then the scene is unlocked.
static void game_render_wait_and_unlock_scene()
{
    if (rl_is_locked())
    {
        Wait_for_prev_done_frame = Framecount_increment_always;
        rl_unlock();
    }
}

// 0x007C2570 (:807)
void game_render_dyn_light_add(const vector& pos, const vector& color, float atten_start, float atten_end)
{
}

// 0x007C2580 (:830)
void game_render_dyn_spot_light_add(const vector& pos, const vector& fvec, const vector& color, float atten_start, float atten_end, float hotspot, float hotspot_falloff)
{
}

// 0x007CF260 (:853)
// A file static in rfg.exe (rfg_gamewide_unity, also called by gamestate_gp_exit); external here because this file is in
// the render library (game_render.h).
void game_render_dyn_light_clear()
{
    for (int i = 0; i < Dynamic_lights.size(); i++)
    {
        Dynamic_lights[i]->release();
        Dynamic_lights[i] = nullptr;
    }
    Dynamic_lights.remove_all();
}

// 0x007C2590 (:870)
static void record_destroyable_memory_stats(int size)
{
}

// 0x007CF2B0 (:882)
// The pool maxima, rl_init, the destroyable instance system on the level memory manager, the main scene with its
// four quadtrees (the renderables' one 10 levels deep), the scene's init (scene and terrain renderers), the terrain
// renderer's init and the object lights.
void game_render_system_init(rl_init_params& rl_params, keen::GraphicsCommandWriter* pCommandWriter, keen::MemoryBlock deferredCommandBufferMemory)
{
    if (Render_system_init)
    {
        return;
    }
    Render_system_init = true;
    // Magic numbers: the pool maxima of the game.
    rl_scene::set_renderable_instances_max(10000);
    rl_set_wait_for_unlock_cb(game_render_wait_and_unlock_scene);
    rl_scene_renderer::set_visible_renderable_instances_max(12000);
    rl_destroyable_model_instance::set_objects_max(0x834);
    rl_destroyable_model_instance_shared_base::set_objects_max(0x898);
    // The particle system pools: parametric 0xE00 and tails 0x400 are vfx's defaults, iterative 900 is not (512).
    vfx_particle_system_manager::set_particles_parametric_max(0xE00);
    rl_particle_renderer::set_particle_parametric_pool_size(0xE00);
    vfx_particle_system_manager::set_particles_iterative_max(900);
    rl_particle_renderer::set_particle_iterative_pool_size(900);
    vfx_particle_system_manager::set_particle_tails_max(0x400);
    rl_particle_renderer::set_particle_tail_pool_size(0x400);
    rl_quadtree_sector::set_objects_max(0x12EE);
    rl_skin_instance::set_objects_max(500);
    rl_material_map::set_objects_max(0xC00);
    rl_mesh_instance::set_objects_max(4000);
    rl_compound_mesh_instance::set_objects_max(2);
    rl_renderable_particles::set_objects_max(0x800);
    rl_fog_volume::set_objects_max(1);
    rl_skin_bone_manager_set_pool_sizes(0x80, 0x4A0);
    rl_mesh::set_objects_max(0x800);
    // The kd tree maximum is the sum of the classes that own one: the deformable and skirt mesh instances (their
    // defaults, 1000 each), the skin instances, the shared bases and the destroyable instances.
    rl_fp_kdtree::set_objects_max(rl_deformable_mesh_instance::get_objects_max() + rl_skirt_mesh_instance::get_objects_max() + rl_skin_instance::get_objects_max() + rl_destroyable_model_instance_shared_base::get_objects_max() + rl_destroyable_model_instance::get_objects_max());
    rl_morph_container::set_objects_max(0x107);
    rl_dynamic_morph_instance::set_objects_max(0x20);
    rl_init(rl_params);
    game_track_mem_usage_start("game scene creation");
    rl_destroyable_model_instance_system_init(&Level_memmgr, record_destroyable_memory_stats);
    Scene_main_p = rl_scene::create();
    rl_set_active_scene(Scene_main_p);
    rl_quadtree* quadtree_p = rl_quadtree::create();
    // Magic number: the renderables' quadtree depth.
    quadtree_p->set_max_depth(10);
    Scene_main_p->set_partition_system(quadtree_p);
    Scene_main_p->set_light_partition_system(rl_quadtree::create());
    Scene_main_p->set_low_lod_partition_system(rl_quadtree::create());
    Scene_main_p->set_landmark_lod_partition_system(rl_quadtree::create());
    Scene_main_p->init(pCommandWriter, deferredCommandBufferMemory);
    Scene_main_p->get_scene_renderer()->get_terrain_renderer()->init();
    object_light_system_init();
    game_track_mem_usage_end(false);
    rl_terrain_renderer_set_enable_lowlod(true);
}

// 0x007C25A0 (:990)
// Notes (decompiled behaviour, kept as is): it clears Render_system_init, so a later game_render_system_init runs
// the whole init again.
void game_render_level_init()
{
    if (Render_system_init)
    {
        Render_system_init = false;
        game_render_terrain_level_init();
    }
}

// 0x007C25C0 (:1007)
void game_render_system_shutdown()
{
    object_light_system_shutdown();
    if (Scene_main_p != nullptr)
    {
        Scene_main_p->release();
    }
    rl_shutdown();
}

// 0x007CF490 (:1024)
void game_render_level_shutdown()
{
    rl_unlock();
    game_render_dyn_light_clear();
    game_render_terrain_level_shutdown();
    Scene_main_p->get_partition_system()->clean();
    xray_effect_level_shutdown();
}

// 0x007C2780 (:1075)
bool game_render_is_object_visible(const rl_view_frustum& frustum, const vector& bmin, const vector& bmax)
{
    const rl_aabb box(bmin, bmax);
    return !frustum.is_occluded(box);
}

// 0x007C27D0 (:1081)
bool game_render_is_object_visible(const rl_view_frustum& frustum, const vector& sphere_center, float sphere_radius)
{
    const rl_sphere sphere(sphere_center, sphere_radius);
    return !frustum.is_occluded(sphere);
}

// 0x007C2820 (:1087)
bool game_render_is_object_visible(const vector& bmin, const vector& bmax)
{
    const rl_view_frustum& frustum = Rfg_camera.get_culling_frustum();
    const rl_aabb box(bmin, bmax);
    return !frustum.is_occluded(box);
}

// 0x007C2870 (:1092)
bool game_render_is_object_visible(const vector& sphere_center, float sphere_radius)
{
    const rl_view_frustum& frustum = Rfg_camera.get_culling_frustum();
    const rl_sphere sphere(sphere_center, sphere_radius);
    return !frustum.is_occluded(sphere);
}

// 0x007C28D0 (:1097)
void game_render_clean_partition_system()
{
    Scene_main_p->get_partition_system()->clean();
}

// 0x007C28F0 (:1105)
// A visible entity in a partition system is inserted again (moved to the sectors of its new bounds).
void game_render_scene_update(rl_scene_entity* entity_p)
{
    if (entity_p != nullptr && entity_p->get_partition_system() != nullptr && entity_p->get_is_visible())
    {
        entity_p->get_partition_system()->insert(entity_p);
    }
}

// 0x007C2920 (:1113)
rl_scene* game_render_get_main_scene()
{
    return Scene_main_p;
}

// 0x007C2930 (:1118)
void game_render_set_TOD_light(rl_light* light_p)
{
    Scene_main_p->set_largest_light(light_p);
    if (light_p != nullptr && light_p->get_is_visible())
    {
        light_p->visible_update(false);
    }
}

// 0x007C2960 (:1123)
rl_light* game_render_get_TOD_light()
{
    if (Scene_main_p == nullptr)
    {
        return nullptr;
    }
    return Scene_main_p->get_largest_light();
}

// 0x007C2990 (:1129)
void game_render_set_TOD_light_color(const rl_color_float& color)
{
    if (Scene_main_p != nullptr)
    {
        rl_light* light_p = Scene_main_p->get_largest_light();
        if (light_p != nullptr)
        {
            light_p->set_color(color);
        }
    }
}

// 0x007C29E0 (:1138)
void game_render_set_ambient_light_color(const rl_color_float& color)
{
    World.level_ambient.x = color.red;
    World.level_ambient.y = color.green;
    World.level_ambient.z = color.blue;
}

// 0x007C2A20 (:1145)
void game_render_set_back_ambient_light_color(const rl_color_float& color)
{
    World.level_back_ambient.x = color.red;
    World.level_back_ambient.y = color.green;
    World.level_back_ambient.z = color.blue;
}

// 0x007C2A60 (:1152)
void game_render_get_ambient_light_color(rl_color_float& color)
{
    color.red = World.level_ambient.x;
    color.green = World.level_ambient.y;
    color.blue = World.level_ambient.z;
    color.alpha = 1.0f;
}

// 0x007C2AC0 (:1157)
void game_render_get_back_ambient_light_color(rl_color_float& color)
{
    color.red = World.level_back_ambient.x;
    color.green = World.level_back_ambient.y;
    color.blue = World.level_back_ambient.z;
    color.alpha = 1.0f;
}

// 0x007C2B20 (:1162)
bool game_render_get_stealth_diffraction_enabled()
{
    return Diffraction_stealth_enabled;
}

// 0x007C2B30 (:1167)
void game_render_set_stealth_diffraction_enabled(bool diffraction_enabled)
{
    Diffraction_stealth_enabled = diffraction_enabled;
}

// 0x007C2B40 (:1172)
bool game_render_get_scope_diffraction_enabled()
{
    return Diffraction_scope_enabled;
}

// 0x007C2B50 (:1177)
void game_render_set_scope_diffraction_enabled(bool diffraction_enabled)
{
    Diffraction_scope_enabled = diffraction_enabled;
}

// 0x007C2B60 (:1182)
// The largest request of the frame wins (game_render_setup_frame resets it).
void game_render_set_camera_frame_override_blur(float frame_override_blur)
{
    if (Camera_frame_override_blur < frame_override_blur)
    {
        Camera_frame_override_blur = frame_override_blur;
    }
}

// 0x007C2B90 (:1190)
void game_render_set_camera_signal_noise_frequency(float frequency)
{
    Camera_signal_noise_frequency = frequency;
}

// 0x007C2BA0 (:1195)
void game_render_set_camera_signal_noise_scale(float scale)
{
    Camera_signal_noise_scale = scale;
}

// 0x007C2BB0 (:1201)
void game_render_set_hdr_state(const rl_hdr_state& hdr_state)
{
    Hdr_state = hdr_state;
}

// 0x007C2BD0 (:1206)
// Relative to the camera.
void game_render_set_sun_shafts_position(const vector& position)
{
    Sun_shafts_position = position;
}

// 0x007C2BF0 (:1211)
void game_render_set_particle_lighting_state(const rl_particle_lighting_state& particle_lighting_state)
{
    Particle_lighting_state.m_ambient_color = particle_lighting_state.m_ambient_color;
    Particle_lighting_state.m_tod_color = particle_lighting_state.m_tod_color;
    Particle_lighting_state.m_tod_light_dir = particle_lighting_state.m_tod_light_dir;
}

// 0x007C2C40 (:1217)
void game_render_set_far_clip_distance(float distance)
{
    Rfg_camera.m_far_clip_dist = distance;
}

// 0x007C2C50 (:1222)
float game_render_get_far_clip_distance()
{
    return Rfg_camera.m_far_clip_dist;
}

// 0x007C2C60 (:1227)
bool game_render_get_fog_enabled()
{
    return Fog_enabled;
}

// 0x007C2C70 (:1232)
void game_render_set_fog_enabled(bool enabled)
{
    Fog_enabled = enabled;
}

// 0x007C2C80 (:1237)
void game_render_set_fog_color(const rl_color_float& color)
{
    if (!r_fog_override)
    {
        Fog_color = color;
    }
}

// 0x007C2CB0 (:1245)
void game_render_set_fog_ground(float fog_ground)
{
    if (!r_fog_override)
    {
        Fog_ground = fog_ground;
    }
}

// 0x007C2CD0 (:1253)
void game_render_get_fog_start(float& fog_ground)
{
    fog_ground = Fog_ground;
}

// 0x007C2CF0 (:1258)
void game_render_set_fog_atmosphere_scale(float fog_atmosphere_scale)
{
    if (!r_fog_override)
    {
        Fog_atmosphere_scale = fog_atmosphere_scale;
    }
}

// 0x007C2D10 (:1266)
void game_render_get_fog_end(float& fog_atmosphere_scale)
{
    fog_atmosphere_scale = Fog_atmosphere_scale;
}

// 0x007C2D30 (:1271)
void game_render_set_fog_density(float fog_density)
{
    if (!r_fog_override)
    {
        Fog_density = fog_density;
    }
}

// 0x007C2D50 (:1279)
void game_render_get_fog_density(float& fog_density)
{
    fog_density = Fog_density;
}

// 0x007C2D70 (:1284)
void game_render_set_fog_density_offset(float fog_density_offset)
{
    if (!r_fog_override)
    {
        Fog_density_offset = fog_density_offset;
    }
}

// 0x007C2D90 (:1292)
void game_render_get_fog_density_offset(float& fog_density_offset)
{
    fog_density_offset = Fog_density_offset;
}

// 0x007C2DB0 (:1298)
void game_render_set_fog_ground_camera_enabled(bool enabled)
{
    R_fog_ground_camera = enabled;
}

// 0x007C2DC0 (:1303)
void game_render_set_fog_ground_camera_min(float min_value)
{
    R_fog_ground_camera_min = min_value;
}

// 0x007C2DD0 (:1308)
void game_render_set_fog_ground_camera_max(float max_value)
{
    R_fog_ground_camera_max = max_value;
}

// 0x007C2DE0 (:1314)
void game_render_get_fog_color(rl_color_float& color)
{
    color = Fog_color;
}

// 0x007C2E00 (:1319)
void game_render_fog_set_density_override_scale(float scale)
{
    Fog_density_override_scale = scale;
}

// 0x007C2E10 (:1324)
float game_render_fog_get_density_override_scale()
{
    return Fog_density_override_scale;
}

// 0x007C2E20 (:1329)
void game_render_set_main_scene_tint(const rl_color_float& tint)
{
    Scene_main_tint = tint;
}

// 0x007C2E40 (:1334)
void game_render_set_main_scene_saturation(float saturation)
{
    Scene_main_saturation = saturation;
}

// 0x007C2E50 (:1339)
const rl_color_float& game_render_get_main_scene_tint()
{
    return Scene_main_tint;
}

// 0x007C2E60 (:1344)
float game_render_get_main_scene_saturation()
{
    return Scene_main_saturation;
}

// 0x007C2E70 (:1349)
void game_render_set_main_scene_modulate_tint(const rl_color_float& tint)
{
    Scene_main_tint_modulate = tint;
}

// 0x007C2E90 (:1354)
void game_render_set_main_scene_modulate_saturation(float saturation)
{
    Scene_main_saturation_modulate = saturation;
}

// 0x007C2EA0 (:1359)
void game_render_set_levels_pregamma_offset(float levels_pregamma_offset)
{
    Levels_pregamma_offset = levels_pregamma_offset;
}

// 0x007C2EB0 (:1364)
void game_render_set_levels_pregamma_scale(float levels_pregamma_scale)
{
    Levels_pregamma_scale = levels_pregamma_scale;
}

// 0x007C2EC0 (:1369)
void game_render_set_levels_postgamma_offset(float levels_postgamma_offset)
{
    Levels_postgamma_offset = levels_postgamma_offset;
}

// 0x007C2ED0 (:1374)
void game_render_set_levels_postgamma_scale(float levels_postgamma_scale)
{
    Levels_postgamma_scale = levels_postgamma_scale;
}

// 0x007C2EE0 (:1379)
void game_render_set_gamma(float gamma, float brightness, float contrast)
{
    Color_correct_gamma = gamma;
    Color_correct_brightness = brightness;
    Color_correct_contrast = contrast;
}

// 0x007C2F10 (:1386)
void game_render_set_bloom_exposure_factor(float exposure_factor)
{
    Color_correct_bloom_exposure_factor = exposure_factor;
}

// 0x007C2F20 (:1391)
void game_render_set_motion_blur_enabled(bool enabled)
{
    Motion_blur_enabled = enabled;
}

// 0x007C2F30 (:1396)
void game_render_set_motion_blur_amount(float amount)
{
    Motion_blur_amount = amount;
}

// 0x007C2F40 (:1401)
void game_render_set_radial_blur_params(const vector2& position, float strength, float radius)
{
    Radial_blur_position.x = position.x;
    Radial_blur_strength = strength;
    Radial_blur_position.y = position.y;
    Radial_blur_radius = radius;
}

// 0x007C2F80 (:1409)
// Only with the render lib's x-ray effect on.
void game_render_set_xray_effect_enabled(bool enabled, float scene_saturation, bool use_terrain_to_occlude)
{
    Xray_effect_enabled = Rl_xray_effect_enabled && enabled;
    Xray_scene_saturation = scene_saturation;
    Xray_use_terrain_to_occlude = use_terrain_to_occlude;
}

// 0x007C2FC0 (:1416)
void game_render_set_xray_fade_dist(float fade_start, float fade_dist)
{
    Xray_fade_start = fade_start;
    Xray_fade_dist = fade_dist;
}

// 0x007C2FE0 (:1423)
void game_render_set_distortion_material(unsigned int mat_handle)
{
    Distortion_effect_material_handle_render = mat_handle;
}

// 0x007C2FF0 (:1429)
void game_render_set_tod_on_off_value(float val)
{
    Time_of_day_onoff_value = val;
}

// 0x007C3000 (:1434)
void game_render_get_tod_on_off_value(float& val)
{
    val = Time_of_day_onoff_value;
}

// 0x007C3020 (:1439)
// Magic numbers: the far clip distance of each LOD level (0 highest).
void game_render_set_lod_level(int lod_level)
{
    switch (lod_level)
    {
    case 0:
        Rfg_camera.m_far_clip_dist = 1000.0f;
        break;
    case 1:
        Rfg_camera.m_far_clip_dist = 251.0f;
        break;
    case 2:
        Rfg_camera.m_far_clip_dist = 201.0f;
        break;
    case 3:
        Rfg_camera.m_far_clip_dist = 151.0f;
        break;
    case 4:
        Rfg_camera.m_far_clip_dist = 101.0f;
        break;
    }
}

// 0x007C30A0 (:1466)
void game_render_set_screen_clip(int x, int y, int w, int h)
{
    Scissor_region.m_left = x;
    Scissor_region.m_top = y;
    Scissor_region.m_right = x + w;
    Scissor_region.m_bottom = y + h;
}

// 0x007C30F0 (:1472)
void game_render_set_skybox(rl_renderable_instance* skybox_mesh_p)
{
    Scene_main_p->set_skybox(skybox_mesh_p);
    // The instance constructor adds every instance to the scene; the sky is drawn through m_skybox_p only.
    if (skybox_mesh_p != nullptr && skybox_mesh_p->get_scene() != nullptr)
    {
        Scene_main_p->remove_renderable_instance(skybox_mesh_p);
    }
}

// 0x007CF4C0 (:1482)
void game_render_pre_stream_render_task_register(game_render_task* task_p)
{
    Task_list_pre_stream_render.append(task_p);
}

// 0x007CF510 (:1487)
void game_render_pre_stream_render_task_unregister(game_render_task* task_p)
{
    Task_list_pre_stream_render.remove(task_p);
}

// 0x007CF520 (:1492)
void game_render_update_render_task_register(game_render_task* task_p)
{
    Task_list_update_render.append(task_p);
}

// 0x007CF570 (:1497)
void game_render_update_render_task_unregister(game_render_task* task_p)
{
    Task_list_update_render.remove(task_p);
}

// 0x007CF580 (:1502)
void game_render_debug_task_register(game_render_task* task_p)
{
    Task_list_debug_render.append(task_p);
}

// 0x007CF5D0 (:1507)
void game_render_debug_task_unregister(game_render_task* task_p)
{
    Task_list_debug_render.remove(task_p);
}

// 0x007CF5E0 (:1665)
// With the free camera on: the camera's position and angles are taken once, then the camera follows the free
// camera's position and angles.
void overwriteFreeCamera(rl_camera* camera_p)
{
    if (!s_freeCamera)
    {
        s_currentlyOnFreeCamera = false;
        return;
    }
    if (!s_currentlyOnFreeCamera)
    {
        s_freeCamPos = camera_p->get_position();
        camera_p->get_rotation().extract_angles(s_freeCamP, s_freeCamB, s_freeCamH);
        s_currentlyOnFreeCamera = true;
    }
    set_entity_transform(camera_p, s_freeCamPos, matrix(s_freeCamP, s_freeCamB, s_freeCamH));
}

// Not in the binary: the absolute value render_frame compares the debug axes with (as decompiled: through the
// difference with the negation).
static float debug_axis_magnitude(float axis)
{
    return 0.0f <= axis - -axis ? axis : -axis;
}

// 0x007CF730 (:1676)
// The free camera (debug axes: strafe and forward at 0.5 per unit, heading and pitch at 1.5 radians per second),
// the scene when render_scene is set, a depth clear (and color when the scene is not drawn), then the deferred
// primitives.
static void render_frame(rl_renderer& renderer, bool render_scene, rl_renderer& deferredRenderer)
{
    rl_camera* camera_p = camera_get_main_render_camera();
    if (s_freeCamera)
    {
        overwriteFreeCamera(camera_p);
        // Magic numbers: the axis dead zone 0.05, the turn rate 1.5 and the move step 0.5.
        if (0.05f < debug_axis_magnitude(DebugAxisInput[2]))
        {
            s_freeCamH = Frametime * DebugAxisInput[2] * 1.5f + s_freeCamH;
        }
        if (0.05f < debug_axis_magnitude(DebugAxisInput[3]))
        {
            s_freeCamP = DebugAxisInput[3] * Frametime * 1.5f + s_freeCamP;
        }
        const matrix orient(s_freeCamP, s_freeCamB, s_freeCamH);
        if (0.05f < debug_axis_magnitude(DebugAxisInput[0]))
        {
            s_freeCamPos.x = DebugAxisInput[0] * orient.rvec.x * 0.5f + s_freeCamPos.x;
            s_freeCamPos.y = DebugAxisInput[0] * orient.rvec.y * 0.5f + s_freeCamPos.y;
            s_freeCamPos.z = orient.rvec.z * DebugAxisInput[0] * 0.5f + s_freeCamPos.z;
        }
        if (0.05f < debug_axis_magnitude(DebugAxisInput[1]))
        {
            s_freeCamPos.x = orient.fvec.x * DebugAxisInput[1] * 0.5f + s_freeCamPos.x;
            s_freeCamPos.y = orient.fvec.y * DebugAxisInput[1] * 0.5f + s_freeCamPos.y;
            s_freeCamPos.z = orient.fvec.z * DebugAxisInput[1] * 0.5f + s_freeCamPos.z;
        }
        set_entity_transform(camera_p, s_freeCamPos, orient);
    }
    camera_p->render_begin(renderer);
    // Magic numbers: clear flags 4 (depth) and 1 (color).
    unsigned int clear_flags = 4;
    if (render_scene)
    {
        Scene_main_p->render(renderer, *camera_p, deferredRenderer);
    }
    else
    {
        clear_flags = 5;
    }
    renderer.clear(rl_color::BLACK, clear_flags, 1.0f, 0);
    rl_primitive_state default_state;
    default_state.set_bits(0);
    renderer.set_macro_state(default_state, false);
    rl_primitive_renderer::get_instance()->end_deferred();
    rl_primitive_renderer::get_instance()->playback_deferred(renderer);
    renderer.set_macro_state(default_state, false);
    camera_p->render_end(renderer);
    // Game_render_stats.total_render_time (timer_get before and after) is not recovered.
}

// 0x007C3170 (:1792)
void game_render_pre_stream_process()
{
    game_render_task_process_all(Task_list_pre_stream_render, nullptr);
}

// 0x007C3180 (:1799)
void game_render_handshake()
{
}

// 0x007C3190 (:1803)
void game_render_check_for_vsync_change()
{
}

// 0x007CFBD0 (:1817)
// On the simulation thread, in the handshake: the render camera from Rfg_camera (position, orientation, clip
// distances, field of view, depth of field, motion and radial blur, noise), the fog (with the camera-height ground
// and density options), color correction, HDR (with the sun shafts relative to the camera), ambient colors, particle
// lighting, x-ray and distortion state on the main scene, the update-render tasks, the render offset (the camera
// zone's center) and the time values.
//
// Notes (decompiled behaviour, kept as is):
//   - The bloom exposure blend writes Hdr_state back, so with a factor other than 1 it converges on the override
//     exposure over the frames instead of being applied once.
//   - Without a zone at the camera position the render offset is left as it was.
void game_render_setup_frame(rl_renderer& renderer)
{
    renderer.set_world_transform(IDENTITY_MATRIX43);
    rl_camera* camera_p = camera_get_main_render_camera();
    Rfg_camera.real_orient.orthogonalize();
    camera_p->set_near_clip(Rfg_camera.m_near_clip_dist);
    camera_p->set_far_clip(Rfg_camera.m_far_clip_dist);
    camera_p->set_high_lod_far_clip(Rfg_camera.m_high_lod_far_clip_dist);
    renderer.set_scissor_region(Scissor_region);
    set_entity_transform(camera_p, Rfg_camera.render_pos, Rfg_camera.render_orient);
    camera_p->set_fov(Rfg_camera.real_fov);
    camera_p->set_focus_start_A(Rfg_camera.m_focus_start_A);
    camera_p->set_focus_start_B(Rfg_camera.m_focus_start_B);
    camera_p->set_focus_end_A(Rfg_camera.m_focus_end_A);
    camera_p->set_focus_end_B(Rfg_camera.m_focus_end_B);
    camera_p->set_depth_of_field_enabled(Rfg_camera.m_dof_enable);
    if (0.0f < Radial_blur_strength)
    {
        camera_p->set_motion_blur_enabled(true);
        camera_p->set_motion_blur_type(rl_camera::MOTION_BLUR_TYPE_RADIAL);
        camera_p->set_motion_blur_amount(Radial_blur_strength);
    }
    else
    {
        camera_p->set_motion_blur_enabled(Motion_blur_enabled);
        camera_p->set_motion_blur_type(rl_camera::MOTION_BLUR_TYPE_CAMERA);
        camera_p->set_motion_blur_amount(Motion_blur_amount);
    }
    camera_p->set_radial_blur_position(Radial_blur_position);
    camera_p->set_radial_blur_radius(Radial_blur_radius);
    camera_p->set_frame_override_blur(Camera_frame_override_blur);
    camera_p->set_signal_noise_frequency(Camera_signal_noise_frequency);
    camera_p->set_signal_noise_scale(Camera_signal_noise_scale);
    overwriteFreeCamera(camera_p);
    if (s_freeCamera)
    {
        Rfg_camera.render_pos = s_freeCamPos;
    }
    Camera_frame_override_blur = 0.0f;
    player* local_player = player_get_local_player();
    Scene_main_p->get_visibility_manager()->set_lod_off_enabled(!player_is_scoped(local_player));
    // Magic number: the noise scale counts as on beyond 0.0001.
    const bool diffraction = Diffraction_stealth_enabled || Diffraction_scope_enabled || 0.0001f <= Camera_signal_noise_scale - 0.0f || 0.0001f <= -(Camera_signal_noise_scale - 0.0f);
    Scene_main_p->set_diffraction_enabled(diffraction);
    Diffraction_stealth_enabled = false;
    game_render_scene_update(camera_p);
    const vector& camera_pos = camera_p->get_position();
    if (Camlight_p != nullptr && !Camlight_freeze)
    {
        Camlight_p->set_position(camera_pos);
        Camlight_p->set_color(rl_color_float(R_camlight_color.x, R_camlight_color.y, R_camlight_color.z, 1.0f));
        Camlight_p->set_attenuation_start(R_camlight_atten_start);
        Camlight_p->set_attenuation_end(R_camlight_atten_end);
        game_render_scene_update(Camlight_p);
    }

    // Fog: off is no height density, atmosphere scale 0.01 and white.
    rl_fog_state fog;
    if (!Fog_enabled)
    {
        fog.set_height_density(0.0f);
        // Magic number: a non-zero atmosphere scale (the vertex shaders divide by it).
        fog.set_atmosphere_scale(0.01f);
        fog.set_density(0.0f);
        fog.set_density_offset(0.0f);
        fog.set_color(rl_color_float(1.0f, 1.0f, 1.0f, 1.0f));
    }
    else
    {
        // Magic number: keeps the atmosphere scale non-zero.
        const float atmosphere_scale = Fog_atmosphere_scale + 1e-05f;
        float density = Fog_density;
        float ground = Fog_ground;
        if (R_fog_ground_camera)
        {
            const float camera_y = camera_pos.y;
            float clamped = camera_y + Fog_ground;
            if (0.0f <= clamped - (camera_y + R_fog_ground_camera_max))
            {
                clamped = camera_y + R_fog_ground_camera_max;
            }
            ground = camera_y + R_fog_ground_camera_min;
            if (0.0f <= clamped - ground)
            {
                ground = clamped;
            }
            if (R_fog_density_camera)
            {
                // Magic number: 1.442695 = 1 / ln(2).
                const float scaled = (float)exp(-R_fog_density_scale * (camera_y - ground)) * 1.442695f * Fog_density;
                density = R_fog_density_min;
                if (0.0f <= scaled - R_fog_density_min)
                {
                    density = scaled;
                }
                if (0.0f <= density - R_fog_density_max)
                {
                    density = R_fog_density_max;
                }
            }
            density = Fog_density_override_scale * density;
        }
        const float height = density * 1.442695f * (float)exp(-atmosphere_scale * (camera_pos.y - ground));
        float height_density = R_fog_height_density_min;
        if (0.0f <= height - R_fog_height_density_min)
        {
            height_density = height;
        }
        if (0.0f <= height_density - R_fog_height_density_max)
        {
            height_density = R_fog_height_density_max;
        }
        fog.set_height_density(height_density);
        fog.set_atmosphere_scale(atmosphere_scale);
        fog.set_density(density);
        fog.set_density_offset(Fog_density_offset);
        fog.set_color(Fog_color);
    }

    rl_color_correct_state color_correct;
    color_correct.set_tint(rl_color_float(Scene_main_tint_modulate.red * Scene_main_tint.red, Scene_main_tint.green * Scene_main_tint_modulate.green, Scene_main_tint.blue * Scene_main_tint_modulate.blue, Scene_main_tint.alpha * Scene_main_tint_modulate.alpha));
    if (Xray_effect_enabled)
    {
        color_correct.set_saturation(Scene_main_saturation_modulate * Xray_scene_saturation);
    }
    else
    {
        color_correct.set_saturation(Scene_main_saturation * Scene_main_saturation_modulate);
    }
    color_correct.set_gamma(Gamma);
    color_correct.set_levels_pregamma_offset(Levels_pregamma_offset);
    color_correct.set_levels_pregamma_scale(Levels_pregamma_scale);
    color_correct.set_levels_postgamma_offset(Levels_postgamma_offset);
    color_correct.set_levels_postgamma_scale(Levels_postgamma_scale);

    // Magic numbers: scoped, nothing fades (100000 to 100001).
    if (player_is_scoped(local_player))
    {
        Scene_main_p->set_opacity_distance_start(100000.0f);
        Scene_main_p->set_opacity_distance_end(100001.0f);
    }
    else
    {
        Scene_main_p->set_opacity_distance_start(Opacity_fade_start);
        Scene_main_p->set_opacity_distance_end(Opacity_fade_end);
    }
    Scene_main_p->set_fog_state(fog);
    Scene_main_p->set_color_correct_state(color_correct);
    Hdr_state.m_bloom_exposure = (Hdr_state.m_bloom_exposure - Bloom_override_exposure) * Color_correct_bloom_exposure_factor + Bloom_override_exposure;
    Hdr_state.m_sun_shafts_position = vector(camera_pos.x + Sun_shafts_position.x, camera_pos.y + Sun_shafts_position.y, camera_pos.z + Sun_shafts_position.z);
    Scene_main_p->set_hdr_state(Hdr_state);
    Scene_main_p->set_ambient_color(rl_color_float(World.level_ambient.x, World.level_ambient.y, World.level_ambient.z, 1.0f));
    Scene_main_p->set_back_ambient_color(rl_color_float(World.level_back_ambient.x, World.level_back_ambient.y, World.level_back_ambient.z, 1.0f));
    Scene_main_p->set_particle_lighting_state(Particle_lighting_state);

    // X-ray: off keeps terrain occlusion on (the default state: fade 40 over 10).
    rl_xray_render_state xray;
    xray.m_xray_effect_enabled = false;
    xray.m_use_terrain_occlusion = true;
    // Magic numbers: the default x-ray fade start and distance.
    xray.m_xray_effect_fade_start = 40.0f;
    xray.m_xray_effect_fade_dist = 10.0f;
    xray.m_num_xray_outline_points = 0;
    vector2* xray_verts = nullptr;
    if (Xray_effect_enabled)
    {
        xray_verts = hud_ui_rail_driver_get_xray_verts(xray.m_num_xray_outline_points);
    }
    if (xray_verts == nullptr)
    {
        xray.m_xray_effect_enabled = false;
    }
    else
    {
        for (int i = 0; i < xray.m_num_xray_outline_points; i++)
        {
            xray.m_xray_outline_points[i] = xray_verts[i];
        }
        xray.m_xray_effect_fade_start = Xray_fade_start;
        xray.m_xray_effect_enabled = true;
        xray.m_use_terrain_occlusion = Xray_use_terrain_to_occlude;
        xray.m_xray_effect_fade_dist = Xray_fade_dist;
    }
    Scene_main_p->set_xray_render_state(xray);
    Scene_main_p->set_distortion_effect_material(Distortion_effect_material_handle_render);
    game_render_dyn_light_clear();

    rl_camera* render_camera_p = camera_get_main_render_camera();
    render_camera_p->render_begin(renderer);
    game_render_task_process_all(Task_list_update_render, &renderer);
    render_camera_p->render_end(renderer);
    // Game_render_stats.update_render_time (timer_get before and after) is not recovered.

    if (!R_enable_batch_render_offset)
    {
        Scene_main_p->set_render_offset(ZERO_VECTOR);
    }
    else
    {
        obj_zone* zone = World.get_zone_by_pos(camera_pos);
        if (zone != nullptr)
        {
            vector bmin = ZERO_VECTOR;
            vector bmax = ZERO_VECTOR;
            zone->get_bbox(bmin, bmax);
            Scene_main_p->set_render_offset(vector((bmax.x - bmin.x) * 0.5f + bmin.x, (bmax.y - bmin.y) * 0.5f + bmin.y, (bmax.z - bmin.z) * 0.5f + bmin.z));
        }
    }
    Scene_main_p->set_time_value((float)Frametime_total_milliseconds / 1000.0f);
    rl_render_lib::get_instance()->m_frametime_total_ms = Frametime_total_milliseconds;
    float tod = Time_of_day_onoff_value;
    if (Time_of_day_onoff_value - 0.0f < 0.0f)
    {
        tod = 0.0f;
    }
    Scene_main_p->set_time_of_day_value(tod - 1.0f < 0.0f ? tod : 1.0f);
}

// 0x007D0A90 (:2050)
void game_render_do_frame(rl_renderer& renderer, bool render_scene, rl_renderer& deferredRenderer)
{
    render_frame(renderer, render_scene, deferredRenderer);
}

// 0x007C31A0 (:2056)
// The gamma change is only tracked.
void game_render_begin_frame()
{
    if (Color_correct_gamma != Last_color_correct_gamma)
    {
        Last_color_correct_gamma = Color_correct_gamma;
    }
}

// 0x007C31D0 (:2068)
void game_render_end_frame()
{
}

// 0x007C31E0 (:2090)
// True when the sphere is outside the camera's culling frustum.
bool game_render_cull_sphere(const vector& center, float radius)
{
    const rl_sphere sphere(center, radius);
    return Rfg_camera.get_culling_frustum().is_occluded(sphere);
}

// 0x007C3230 (:2106)
bool game_render_cull_bounding_box(const vector& mn, const vector& mx)
{
    const rl_aabb box(mn, mx);
    return Rfg_camera.get_culling_frustum().is_occluded(box);
}

// 0x007C3280 (:2124)
// The box in the frame of pos/orient: the frustum is moved into that frame instead.
bool game_render_cull_oriented_bounding_box(const vector& mn, const vector& mx, const vector& pos, const matrix& orient)
{
    const rl_view_frustum local_frustum = Rfg_camera.get_culling_frustum().copy_rotate(orient, pos);
    const rl_aabb box(mn, mx);
    return local_frustum.is_occluded(box);
}

// 0x007C3310 (:2134)
// The distance scaled by the field of view relative to the default one.
float game_render_get_apparent_distance_from_camera(const vector& pos)
{
    // Magic number: degrees to radians (0.017453292).
    const float half_fov = (float)((double)Rfg_camera.real_fov * 0.5 * 0.01745329238474369);
    const float fov_tan = (float)tan(half_fov);
    const vector& camera_pos = Camera_third_person_lock ? Rfg_camera.render_pos : Rfg_camera.real_pos;
    const double scaled = (double)(vec_dist(pos, camera_pos) * fov_tan);
    // Magic number: pi / 8, the default half field of view (22.5 degrees).
    return (float)(scaled / (double)(float)tan(0.39269909262657166));
}

// 0x007C33D0 (:2195)
void game_render_start_handshake()
{
    if (rl_is_locked())
    {
        rl_unlock();
    }
    Scene_ref_count++;
}

// 0x007C33F0 (:2205)
void game_render_end_handshake()
{
    Scene_ref_count--;
    if (Scene_ref_count == 0 && !rl_is_locked())
    {
        rl_lock();
    }
}

// 0x007C3410 (:2214)
void game_render_request_vsync()
{
    Vsync_request_count++;
}

// 0x007C3420 (:2220)
void game_render_release_vsync()
{
    Vsync_request_count--;
}
