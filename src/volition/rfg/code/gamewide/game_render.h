// Original: volition/rfg/code/gamewide/game_render.h
// Implementation: volition/rfg/code/gamewide/game_render.cpp
//
// The game's render driver on top of render_lib: system and level init/shutdown (game_render_system_init creates the
// main and deferred renderers, the main scene and its scene renderer), the per-frame entry points (game_render_setup_frame
// during the handshake on the simulation thread; game_render_begin_frame, game_render_do_frame and
// game_render_end_frame from do_game_render_frame on the render thread), visibility helpers, the main scene and
// time-of-day light access, ambient colors, diffraction and camera effect parameters, and debug 2D/3D drawing.
#pragma once


class cubic_bezier_spline;
struct game_render_task;
struct line;
class matrix;
class rl_camera;
class rl_color_float;
class rl_hdr_state;
class rl_init_params;
class rl_light;
class rl_particle_lighting_state;
class rl_renderable_instance;
class rl_renderer;
class rl_scene;
class rl_scene_entity;
class rl_view_frustum;
class vector;
class vector2;

namespace keen
{
    class GraphicsCommandWriter;
    struct MemoryBlock;
}

// The debug free camera's pad axes, written by keen::rfg::Application::processInputEvent for a pad no player holds.
extern float DebugAxisInput[4]; // 0x01A37830

void unused_dcf_r_ugly_mode(); // 0x007C1F70 (:276)
void unused_dcf_r_camlight(); // 0x007C1FC0 (:333)
void unused_dcf_r_quadtree_set_size(); // 0x007C1FE0 (:371)
void unused_dcf_r_quadtree_set_max_depth(); // 0x007C2060 (:382)
void unused_dcf_r_quadtree_reset(); // 0x007C20D0 (:393)
void unused_dcf_r_fog_density(); // 0x007C20F0 (:418)
void unused_dcf_r_fog_density_offset(); // 0x007C2190 (:427)
void unused_dcf_r_fog_atmosphere_scale(); // 0x007C2230 (:436)
void unused_dcf_r_fog_ground(); // 0x007C22D0 (:445)
void unused_dcf_r_tint(); // 0x007C2370 (:455)
void unused_dcf_r_saturation(); // 0x007C2490 (:481)
void unused_dcf_r_show_mem_usage(); // 0x007CF220 (:491)
void game_render_2d_text(const char* str, const vector& text_pos); // 0x007C2500 (:795)
void game_render_dyn_light_add(const vector& pos, const vector& color, float atten_start, float atten_end); // 0x007C2570 (:807)
void game_render_dyn_spot_light_add(const vector& pos, const vector& fvec, const vector& color, float atten_start, float atten_end, float hotspot, float hotspot_falloff); // 0x007C2580 (:830)
void game_render_system_init(rl_init_params& rl_params, keen::GraphicsCommandWriter* pCommandWriter, keen::MemoryBlock deferredCommandBufferMemory); // 0x007CF2B0 (:882)
void game_render_level_init(); // 0x007C25A0 (:990)
void game_render_system_shutdown(); // 0x007C25C0 (:1007)
void game_render_level_shutdown(); // 0x007CF490 (:1024)
void game_render_spline(const cubic_bezier_spline* cbs, int num_segments, bool lines_only); // 0x007C25E0 (:1034)
bool game_render_is_object_visible(const rl_view_frustum& frustum, const vector& bmin, const vector& bmax); // 0x007C2780 (:1075)
bool game_render_is_object_visible(const rl_view_frustum& frustum, const vector& sphere_center, float sphere_radius); // 0x007C27D0 (:1081)
bool game_render_is_object_visible(const vector& bmin, const vector& bmax); // 0x007C2820 (:1087)
bool game_render_is_object_visible(const vector& sphere_center, float sphere_radius); // 0x007C2870 (:1092)
void game_render_clean_partition_system(); // 0x007C28D0 (:1097)
void game_render_scene_update(rl_scene_entity* entity_p); // 0x007C28F0 (:1105)
rl_scene* game_render_get_main_scene(); // 0x007C2920 (:1113)
void game_render_set_TOD_light(rl_light* light_p); // 0x007C2930 (:1118)
rl_light* game_render_get_TOD_light(); // 0x007C2960 (:1123)
void game_render_set_TOD_light_color(const rl_color_float& color); // 0x007C2990 (:1129)
void game_render_set_ambient_light_color(const rl_color_float& color); // 0x007C29E0 (:1138)
void game_render_set_back_ambient_light_color(const rl_color_float& color); // 0x007C2A20 (:1145)
void game_render_get_ambient_light_color(rl_color_float& color); // 0x007C2A60 (:1152)
void game_render_get_back_ambient_light_color(rl_color_float& color); // 0x007C2AC0 (:1157)
bool game_render_get_stealth_diffraction_enabled(); // 0x007C2B20 (:1162)
void game_render_set_stealth_diffraction_enabled(bool diffraction_enabled); // 0x007C2B30 (:1167)
bool game_render_get_scope_diffraction_enabled(); // 0x007C2B40 (:1172)
void game_render_set_scope_diffraction_enabled(bool diffraction_enabled); // 0x007C2B50 (:1177)
void game_render_set_camera_frame_override_blur(float frame_override_blur); // 0x007C2B60 (:1182)
void game_render_set_camera_signal_noise_frequency(float frequency); // 0x007C2B90 (:1190)
void game_render_set_camera_signal_noise_scale(float scale); // 0x007C2BA0 (:1195)
void game_render_set_hdr_state(const rl_hdr_state& hdr_state); // 0x007C2BB0 (:1201)
void game_render_set_sun_shafts_position(const vector& position); // 0x007C2BD0 (:1206)
void game_render_set_particle_lighting_state(const rl_particle_lighting_state& particle_lighting_state); // 0x007C2BF0 (:1211)
void game_render_set_far_clip_distance(float distance); // 0x007C2C40 (:1217)
float game_render_get_far_clip_distance(); // 0x007C2C50 (:1222)
bool game_render_get_fog_enabled(); // 0x007C2C60 (:1227)
void game_render_set_fog_enabled(bool enabled); // 0x007C2C70 (:1232)
void game_render_set_fog_color(const rl_color_float& color); // 0x007C2C80 (:1237)
void game_render_set_fog_ground(float fog_ground); // 0x007C2CB0 (:1245)
void game_render_get_fog_start(float& fog_ground); // 0x007C2CD0 (:1253)
void game_render_set_fog_atmosphere_scale(float fog_atmosphere_scale); // 0x007C2CF0 (:1258)
void game_render_get_fog_end(float& fog_atmosphere_scale); // 0x007C2D10 (:1266)
void game_render_set_fog_density(float fog_density); // 0x007C2D30 (:1271)
void game_render_get_fog_density(float& fog_density); // 0x007C2D50 (:1279)
void game_render_set_fog_density_offset(float fog_density_offset); // 0x007C2D70 (:1284)
void game_render_get_fog_density_offset(float& fog_density_offset); // 0x007C2D90 (:1292)
void game_render_set_fog_ground_camera_enabled(bool enabled); // 0x007C2DB0 (:1298)
void game_render_set_fog_ground_camera_min(float min_value); // 0x007C2DC0 (:1303)
void game_render_set_fog_ground_camera_max(float max_value); // 0x007C2DD0 (:1308)
void game_render_get_fog_color(rl_color_float& color); // 0x007C2DE0 (:1314)
void game_render_fog_set_density_override_scale(float scale); // 0x007C2E00 (:1319)
float game_render_fog_get_density_override_scale(); // 0x007C2E10 (:1324)
void game_render_set_main_scene_tint(const rl_color_float& tint); // 0x007C2E20 (:1329)
void game_render_set_main_scene_saturation(float saturation); // 0x007C2E40 (:1334)
const rl_color_float& game_render_get_main_scene_tint(); // 0x007C2E50 (:1339)
float game_render_get_main_scene_saturation(); // 0x007C2E60 (:1344)
void game_render_set_main_scene_modulate_tint(const rl_color_float& tint); // 0x007C2E70 (:1349)
void game_render_set_main_scene_modulate_saturation(float saturation); // 0x007C2E90 (:1354)
void game_render_set_levels_pregamma_offset(float levels_pregamma_offset); // 0x007C2EA0 (:1359)
void game_render_set_levels_pregamma_scale(float levels_pregamma_scale); // 0x007C2EB0 (:1364)
void game_render_set_levels_postgamma_offset(float levels_postgamma_offset); // 0x007C2EC0 (:1369)
void game_render_set_levels_postgamma_scale(float levels_postgamma_scale); // 0x007C2ED0 (:1374)
void game_render_set_gamma(float gamma, float brightness, float contrast); // 0x007C2EE0 (:1379)
void game_render_set_bloom_exposure_factor(float exposure_factor); // 0x007C2F10 (:1386)
void game_render_set_motion_blur_enabled(bool enabled); // 0x007C2F20 (:1391)
void game_render_set_motion_blur_amount(float amount); // 0x007C2F30 (:1396)
void game_render_set_radial_blur_params(const vector2& position, float strength, float radius); // 0x007C2F40 (:1401)
void game_render_set_xray_effect_enabled(bool enabled, float scene_saturation, bool use_terrain_to_occlude); // 0x007C2F80 (:1409)
void game_render_set_xray_fade_dist(float fade_start, float fade_dist); // 0x007C2FC0 (:1416)
void game_render_set_distortion_material(unsigned int mat_handle); // 0x007C2FE0 (:1423)
void game_render_set_tod_on_off_value(float val); // 0x007C2FF0 (:1429)
void game_render_get_tod_on_off_value(float& val); // 0x007C3000 (:1434)
void game_render_set_lod_level(int lod_level); // 0x007C3020 (:1439)
void game_render_set_screen_clip(int x, int y, int w, int h); // 0x007C30A0 (:1466)
void game_render_set_skybox(rl_renderable_instance* skybox_mesh_p); // 0x007C30F0 (:1472)
void game_render_pre_stream_render_task_register(game_render_task* task_p); // 0x007CF4C0 (:1482)
void game_render_pre_stream_render_task_unregister(game_render_task* task_p); // 0x007CF510 (:1487)
void game_render_update_render_task_register(game_render_task* task_p); // 0x007CF520 (:1492)
void game_render_update_render_task_unregister(game_render_task* task_p); // 0x007CF570 (:1497)
void game_render_debug_task_register(game_render_task* task_p); // 0x007CF580 (:1502)
void game_render_debug_task_unregister(game_render_task* task_p); // 0x007CF5D0 (:1507)
void game_render_frame(); // 0x007C3120 (:1594)
void overwriteFreeCamera(rl_camera* camera_p); // 0x007CF5E0 (:1665)
void game_render_pre_stream_process(); // 0x007C3170 (:1792)
// A file static of game_render.cpp in rfg.exe (rfg_gamewide_unity); external here for gamestate_gp_exit.
void game_render_dyn_light_clear(); // 0x007CF260 (:853)
void game_render_handshake(); // 0x007C3180 (:1799)
void game_render_check_for_vsync_change(); // 0x007C3190 (:1803)
void game_render_setup_frame(rl_renderer& renderer); // 0x007CFBD0 (:1817)
void game_render_do_frame(rl_renderer& renderer, bool render_scene, rl_renderer& deferredRenderer); // 0x007D0A90 (:2050)
void game_render_begin_frame(); // 0x007C31A0 (:2056)
void game_render_end_frame(); // 0x007C31D0 (:2068)
bool game_render_cull_sphere(const vector& center, float radius); // 0x007C31E0 (:2090)
bool game_render_cull_bounding_box(const vector& mn, const vector& mx); // 0x007C3230 (:2106)
bool game_render_cull_oriented_bounding_box(const vector& mn, const vector& mx, const vector& pos, const matrix& orient); // 0x007C3280 (:2124)
float game_render_get_apparent_distance_from_camera(const vector& pos); // 0x007C3310 (:2134)
void game_render_unproject(line& l, float sx, float sy); // 0x007D0AB0 (:2160)
void game_render_start_handshake(); // 0x007C33D0 (:2195)
void game_render_end_handshake(); // 0x007C33F0 (:2205)
void game_render_request_vsync(); // 0x007C3410 (:2214)
void game_render_release_vsync(); // 0x007C3420 (:2220)
