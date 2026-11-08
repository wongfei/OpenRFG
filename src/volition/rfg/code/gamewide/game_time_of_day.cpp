// Original: volition/rfg/code/gamewide/game_time_of_day.cpp
//
// The time of day: the current time (from the game clock), the TOD parameter sets of the districts around the camera
// blended by its position in its grid cell (TOD_blend_info), and the per-frame render update that turns them into the
// sun light (direction and color), ambient and back ambient, fog, HDR, color correction, particle lighting and the
// sky's fog values. The sky colors, cloud lighting, horizon mountains and orbitals are read by object_skybox.
// Compiled in rfg_gamewide_unity.cpp, after game_render.cpp, whose file statics (fog, HDR state, color correction,
// particle lighting, the update-render task list) it writes. Not done: the debug console commands (unused_dcf_*).
// game_time_of_day_render_debug is a debug draw, stripped (render_debug.md).

#include "volition/rfg/code/gamewide/game_time_of_day.h"

#include "volition/render_lib/code/rl_hdr_state.h"
#include "volition/render_lib/code/rl_light.h"
#include "volition/render_lib/code/rl_partition_system.h"
#include "volition/render_lib/code/rl_renderer_core.h"
#include "volition/render_lib/code/rl_scene.h"
#include "volition/render_lib/code/rl_shadow_system.h"
#include "volition/render_lib/code/rl_skybox_renderable_instance.h"
#include "volition/rfg/code/camera/rfg_camera.h"
#include "volition/rfg/code/effects/effects.h"
#include "volition/rfg/code/effects/object_effect.h"
#include "volition/rfg/code/gameplay/object_district.h"
#include "volition/rfg/code/gamewide/game_clock.h"
#include "volition/rfg/code/level/rfg_zonex_shared.h"
#include "volition/rfg/code/level/world.h"
#include "volition/rfg/code/objects/object_skybox.h"
#include "volition/rfg/code/objects/object_skybox_shared.h"
#include "volition/rfg/code/rfg.h"
#include "volition/vlib/code/bmpman/bmpman.h"
#include "volition/vlib/code/cfile/cfile.h"
#include "volition/vlib/code/math/matrix.h"
#include "volition/vlib/code/os/frametime.h"
#include "volition/vlib/code/memory/virtual_mempool.h"

extern game_clock Game_clock; // 0x025054AC, gamestate_gameplay.cpp

#include "keen/base/trace.hpp"

#include <stdio.h>
#include <string.h>

static const int TOD_MAX_LOADED_DISTRICTS = 9; // TOD_loaded_districts holds this many (allocated per level)

// ---- globals
float Current_time_of_day = 11.0f; // 0x0165CC80, hours
bool TOD_tint_primary_color = true; // 0x0165CC77: normalize the color correction tint by its largest component
static float g_TOD = 12.0f; // 0x0165CC7C, the forced time when g_enableTOD is set
static bool g_enableTOD; // 0x025338F8
float Tod_light_min_angle_to_horizon = 40.0f; // 0x0165CC84, degrees
float Tod_particle_light_min_angle_to_horizon = 90.0f; // 0x0165CC88
float TOD_particle_light_fudge = 0.5f; // 0x0165CC8C, how far the particle light direction is pulled up
bool Tod_blend_enabled = true; // 0x0165CC90
bool TOD_light_relative_to_camera = true; // 0x0165CC91
bool TOD_light_color_animate = true; // 0x0165CC92
bool TOD_ambient_animate = true; // 0x0165CC93
float Tod_blend_percent = 0.1f; // 0x0165CC94, the part of a grid cell (from each edge) blended with the neighbours
float TOD_light_distance = 2000.0f; // 0x0165CC98
static float Cloud_scroll_separate_clock_scale_factor = 150.0f; // 0x0165CC9C
bool TOD_back_ambient_animate = true; // 0x0165CCA0
bool TOD_fog_color_animate = true; // 0x0165CCA1
bool TOD_fog_ground_animate = true; // 0x0165CCA2
bool TOD_fog_atmosphere_scale_animate = true; // 0x0165CCA3
bool TOD_fog_density_animate = true; // 0x0165CCA4
bool TOD_fog_density_offset_animate = true; // 0x0165CCA5
bool TOD_light_angles_animate = true; // 0x0165CCA6
bool TOD_enabled = true; // 0x0165CCA7
bool TOD_cloud_horizon_strenghts_animate = true; // 0x0165CCA8
bool TOD_cloud_overhead_strenghts_animate = true; // 0x0165CCA9
static float Drop_shadow_light_height = 1.5f; // 0x0165CCAC
rl_color_float TOD_light_color_override(0.0f, 0.0f, 0.0f, 1.0f); // 0x0165EADC
vector4 TOD_light_on_off(7.5f, 8.5f, 17.5f, 18.5f); // 0x0165EAF0: lights fade out 7:30-8:30, back in 17:30-18:30
rl_color_float TOD_ambient_override(0.0f, 0.0f, 0.0f, 1.0f); // 0x0165EB00
rl_color_float TOD_back_ambient_override(0.0f, 0.0f, 0.0f, 1.0f); // 0x0165EB10
rl_color_float TOD_fog_color_override(0.0f, 0.0f, 0.0f, 1.0f); // 0x0165EB20
tod_district_info* TOD_loaded_districts; // 0x02512564
static bool Enable_tutorial_hacks; // 0x025338F9
bool TOD_clouds_fixed; // 0x025338FA
bool TOD_pause; // 0x025338FB
static int Num_tod_loaded_districts; // 0x025338FC
static rl_light* Game_time_of_day_light_p; // 0x02533900
static rl_light* Game_drop_shadow_light_p; // 0x02533904
static float Cloud_scroll_time; // 0x02533908
static float Cloud_layer01_scroll_pos; // 0x0253390C
static float Cloud_layer23_scroll_pos; // 0x02533910
float TOD_rot_around_y; // 0x02533914, degrees
float TOD_fog_ground_override; // 0x02533918
float TOD_fog_atmosphere_scale_override; // 0x0253391C
float TOD_fog_density_override; // 0x02533920
float TOD_fog_density_offset_override; // 0x02533924
bool Tod_show_sun_path; // 0x02533928
static vector4 Current_horizon_strengths; // 0x02533970
static vector4 Current_overhead_strengths; // 0x02533980
vector TOD_light_angles_override; // 0x02533990
vector4 TOD_cloud_horizon_override; // 0x025339A0
vector4 TOD_cloud_overhead_override; // 0x025339B0
static TOD_blend_info Current_blend; // 0x025356DC
static game_render_task Update_tod_render_task; // 0x0253663C
tod_global_parameters TOD_global_info; // 0x02536650

// ---- TOD_blend_info / tod_district_info
// 0x007BFF40 (.h:60)
TOD_blend_info::TOD_blend_info()
{
    sources[0] = nullptr;
    sources[1] = nullptr;
    sources[2] = nullptr;
    sources[3] = nullptr;
    blend_valid = false;
}

// 0x007DFA00
tod_district_info::tod_district_info()
{
}

// The blend of a variable over the four sources of Current_blend at the current time (inlined everywhere).
static float tod_blend(tod_interpolated_var<float> tod_district_parameters::* var)
{
    float value = 0.0f;
    for (int i = 0; i < 4; i++)
    {
        value += (Current_blend.sources[i]->*var).get_value_at_time(Current_time_of_day) * Current_blend.blend_weights[i];
    }
    return value;
}

static vector4 tod_blend(tod_interpolated_var<vector4> tod_district_parameters::* var)
{
    vector4 value(0.0f, 0.0f, 0.0f, 0.0f);
    for (int i = 0; i < 4; i++)
    {
        value = value + (Current_blend.sources[i]->*var).get_value_at_time(Current_time_of_day) * Current_blend.blend_weights[i];
    }
    return value;
}

static vector4 tod_blend(tod_interpolated_var<vector4> (tod_district_parameters::* var)[4], int index)
{
    vector4 value(0.0f, 0.0f, 0.0f, 0.0f);
    for (int i = 0; i < 4; i++)
    {
        value = value + (Current_blend.sources[i]->*var)[index].get_value_at_time(Current_time_of_day) * Current_blend.blend_weights[i];
    }
    return value;
}

// ---- internal functions
// 0x007C56E0 (:633, LTCG)
static tod_district_info* game_time_of_day_find_district_file(unsigned int district_handle)
{
    for (int i = 0; i < Num_tod_loaded_districts; i++)
    {
        if (TOD_loaded_districts[i].m_district_handle == district_handle)
        {
            return &TOD_loaded_districts[i];
        }
    }
    return nullptr;
}

// 0x007C5720 (:667)
static tod_district_info* game_time_of_day_find_free()
{
    if (Num_tod_loaded_districts < TOD_MAX_LOADED_DISTRICTS)
    {
        return &TOD_loaded_districts[Num_tod_loaded_districts];
    }
    return nullptr;
}

// 0x007C5740 (:706, LTCG): pos's position in the box, per axis, 0 at bmin and 1 at bmax.
static void calculate_blend(vector& blend_pct, const vector& pos, const vector& bmin, const vector& bmax)
{
    blend_pct.x = (pos.x - bmin.x) / (bmax.x - bmin.x);
    blend_pct.y = (pos.y - bmin.y) / (bmax.y - bmin.y);
    blend_pct.z = (pos.z - bmin.z) / (bmax.z - bmin.z);
}

// 0x007D19D0 (:719): the camera's district, and with blending its neighbours across the nearest cell edges, weighted
// bilinearly. Within Tod_blend_percent (at most 0.4) of an edge the weights move from 1 to 0.5.
static void game_time_of_day_update_blend()
{
    unsigned int district_handle = district_handle_from_pos(Rfg_camera.real_pos);
    if (district_handle == 0xFFFFFFFF)
    {
        district_handle = district_get_default();
    }
    float blend_x = 0.0f;
    float blend_z = 0.0f;
    float blend_percent = Tod_blend_percent;
    if (blend_percent - 0.0f < 0.0f)
    {
        blend_percent = 0.0f;
    }
    if (0.0f <= blend_percent - 0.4f)
    {
        blend_percent = 0.4f;
    }
    if (!Tod_blend_enabled || blend_percent <= 0.0f)
    {
        tod_district_info* info = game_time_of_day_find_district_file(district_handle);
        Current_blend.sources[0] = &info->m_params;
        Current_blend.blend_weights[0] = 1.0f;
        Current_blend.blend_weights[1] = 0.0f;
        Current_blend.blend_weights[2] = 0.0f;
        Current_blend.sources[1] = Current_blend.sources[0];
        Current_blend.sources[2] = Current_blend.sources[0];
        Current_blend.sources[3] = Current_blend.sources[0];
        Current_blend.blend_weights[3] = 0.0f;
        Current_blend.blend_valid = true;
        return;
    }

    unsigned int handles[4];
    handles[0] = district_handle;
    int grid_x;
    int grid_y;
    if (rfg_zonex_get_grid_coords(Rfg_camera.real_pos, grid_x, grid_y))
    {
        vector bmin;
        vector bmax;
        rfg_zonex_get_zone_bbox(grid_x, grid_y, bmin, bmax);
        const float cell_x = (Rfg_camera.real_pos.x - bmin.x) / (bmax.x - bmin.x);
        const float cell_z = (Rfg_camera.real_pos.z - bmin.z) / (bmax.z - bmin.z);
        int neighbour_x;
        float edge;
        if (0.5f < cell_x)
        {
            edge = (1.0f - cell_x) / blend_percent;
            neighbour_x = grid_x + 1;
        }
        else
        {
            edge = cell_x / blend_percent;
            neighbour_x = grid_x - 1;
        }
        if (edge - 0.0f < 0.0f)
        {
            edge = 0.0f;
        }
        if (0.0f <= edge - 1.0f)
        {
            edge = 1.0f;
        }
        blend_x = edge * 0.5f + 0.5f;
        // The grid rows run against z: the neighbour toward +z is row - 1.
        int neighbour_y;
        if (0.5f < cell_z)
        {
            edge = (1.0f - cell_z) / blend_percent;
            neighbour_y = grid_y - 1;
        }
        else
        {
            edge = cell_z / blend_percent;
            neighbour_y = grid_y + 1;
        }
        if (edge - 0.0f < 0.0f)
        {
            edge = 0.0f;
        }
        if (0.0f <= edge - 1.0f)
        {
            edge = 1.0f;
        }
        blend_z = edge * 0.5f + 0.5f;
        // Clamped to the 16x16 grid (an index of 0 or below becomes 0, one of 15 or above 15).
        if (neighbour_x < 1)
        {
            neighbour_x = 0;
        }
        else if (14 < neighbour_x)
        {
            neighbour_x = 15;
        }
        if (neighbour_y < 1)
        {
            neighbour_y = 0;
        }
        else if (14 < neighbour_y)
        {
            neighbour_y = 15;
        }
        unsigned int handle = district_handle_from_grid_location(neighbour_x, grid_y);
        handles[1] = handle != 0xFFFFFFFF ? handle : district_handle;
        handle = district_handle_from_grid_location(grid_x, neighbour_y);
        handles[2] = handle != 0xFFFFFFFF ? handle : district_handle;
        handle = district_handle_from_grid_location(neighbour_x, neighbour_y);
        handles[3] = handle != 0xFFFFFFFF ? handle : district_handle;
    }
    else
    {
        // The original leaves handles[1..3] uninitialized here (it only prints); they are looked up like the others.
        keen::debug::nativePrintArgs("Int3 critical function at %s(%d)\n", __FILE__, __LINE__);
        handles[1] = district_handle;
        handles[2] = district_handle;
        handles[3] = district_handle;
    }
    tod_district_info* infos[4];
    for (int i = 0; i < 4; i++)
    {
        infos[i] = game_time_of_day_find_district_file(handles[i]);
        if (infos[i] == nullptr)
        {
            infos[i] = game_time_of_day_find_district_file(district_get_default());
        }
    }
    Current_blend.sources[0] = &infos[0]->m_params;
    Current_blend.sources[1] = infos[1] != nullptr ? &infos[1]->m_params : Current_blend.sources[0];
    Current_blend.sources[2] = infos[2] != nullptr ? &infos[2]->m_params : Current_blend.sources[0];
    Current_blend.sources[3] = infos[3] != nullptr ? &infos[3]->m_params : Current_blend.sources[0];
    Current_blend.blend_weights[0] = blend_z * blend_x;
    Current_blend.blend_weights[1] = blend_z * (1.0f - blend_x);
    Current_blend.blend_weights[2] = blend_x * (1.0f - blend_z);
    Current_blend.blend_weights[3] = (1.0f - blend_z) * (1.0f - blend_x);
    Current_blend.blend_valid = true;
}

// ---- functions
// 0x007DBE80 (:883): the update-render task (Task_list_update_render, run in the sim/render handshake).
void game_time_of_day_render_update(void*, rl_renderer*)
{
    const vector& camera_pos = Camera_third_person_lock ? Rfg_camera.render_pos : Rfg_camera.real_pos;

    // The sun.
    vector light_dir;
    if (TOD_light_angles_animate)
    {
        light_dir = tod_shared_calcaulate_light_direction_at_time(Current_time_of_day, TOD_rot_around_y, Tod_light_min_angle_to_horizon);
    }
    else
    {
        const matrix orient(TOD_light_angles_override.x, TOD_light_angles_override.y, TOD_light_angles_override.z);
        light_dir = vector(orient.fvec.x * -1.0f, orient.fvec.y * -1.0f, orient.fvec.z * -1.0f);
    }
    Game_time_of_day_light_p->set_light_direction(light_dir);

    // The drop shadow light hangs above the camera (magic number: offset by 15% of its height in x and z).
    if (Game_drop_shadow_light_p != nullptr)
    {
        const float offset = Drop_shadow_light_height * 0.15f;
        Game_drop_shadow_light_p->set_position(vector(camera_pos.x + offset, camera_pos.y + Drop_shadow_light_height, camera_pos.z + offset));
    }
    rl_shadow_system::m_drop_shadow_light = Game_drop_shadow_light_p;

    // The LDR range first: the color conversions below use it without HDR.
    const float ldr_min = tod_blend(&tod_district_parameters::ldr_min);
    const float ldr_max = tod_blend(&tod_district_parameters::ldr_max);
    set_hdr_to_ldr_conversion_range(ldr_min, ldr_max);

    rl_color_float color;
    hdr_color_from_vector4(color, tod_blend(&tod_district_parameters::tod_light_color));
    Game_time_of_day_light_p->set_color(TOD_light_color_animate ? color : TOD_light_color_override);

    hdr_color_from_vector4(color, tod_blend(&tod_district_parameters::ambient_color));
    const rl_color_float& ambient = TOD_ambient_animate ? color : TOD_ambient_override;
    World.level_ambient.x = ambient.red;
    World.level_ambient.y = ambient.green;
    World.level_ambient.z = ambient.blue;

    hdr_color_from_vector4(color, tod_blend(&tod_district_parameters::back_ambient_color));
    const rl_color_float& back_ambient = TOD_back_ambient_animate ? color : TOD_back_ambient_override;
    World.level_back_ambient.x = back_ambient.red;
    World.level_back_ambient.y = back_ambient.green;
    World.level_back_ambient.z = back_ambient.blue;

    // Fog.
    hdr_color_from_vector4(color, tod_blend(&tod_district_parameters::fog_color));
    if (!r_fog_override)
    {
        Fog_color = TOD_fog_color_animate ? color : TOD_fog_color_override;
    }
    const float fog_ground = TOD_fog_ground_animate ? tod_blend(&tod_district_parameters::fog_ground) : TOD_fog_ground_override;
    if (!r_fog_override)
    {
        Fog_ground = fog_ground;
    }
    const float fog_atmosphere_scale = TOD_fog_atmosphere_scale_animate ? tod_blend(&tod_district_parameters::fog_atmosphere_scale) : TOD_fog_atmosphere_scale_override;
    if (!r_fog_override)
    {
        Fog_atmosphere_scale = fog_atmosphere_scale;
    }
    const float fog_density = TOD_fog_density_animate ? tod_blend(&tod_district_parameters::fog_density) : TOD_fog_density_override;
    if (!r_fog_override)
    {
        Fog_density = fog_density;
    }
    const float fog_density_offset = TOD_fog_density_offset_animate ? tod_blend(&tod_district_parameters::fog_density_offset) : TOD_fog_density_offset_override;
    if (!r_fog_override)
    {
        Fog_density_offset = fog_density_offset;
    }
    R_fog_ground_camera = !Enable_tutorial_hacks && TOD_global_info.fog_camera_follow;

    // The sky's fog.
    object_skybox_set_matte_fog_scale(tod_blend(&tod_district_parameters::fog_matte_scale));
    object_skybox_set_matte_fog_offset(tod_blend(&tod_district_parameters::fog_matte_offset));
    object_skybox_set_sky_fog_scale(tod_blend(&tod_district_parameters::fog_skybox_scale));
    object_skybox_set_sky_fog_offset(tod_blend(&tod_district_parameters::fog_skybox_offset));
    object_skybox_set_clouds_fog_scale(tod_blend(&tod_district_parameters::fog_clouds_scale));
    object_skybox_set_clouds_fog_offset(tod_blend(&tod_district_parameters::fog_clouds_offset));
    object_skybox_set_sky_fog_camera_enabled(!Enable_tutorial_hacks && TOD_global_info.fog_camera_follow);

    // HDR.
    rl_hdr_state hdr;
    hdr.m_bloom_new = true;
    hdr.m_bloom_exposure = tod_blend(&tod_district_parameters::bloom_exposure);
    hdr.m_iris_rate = tod_blend(&tod_district_parameters::iris_rate);
    hdr.m_luminance_max = tod_blend(&tod_district_parameters::luminance_max);
    hdr.m_luminance_min = tod_blend(&tod_district_parameters::luminance_min);
    hdr.m_luminance_mask_max = tod_blend(&tod_district_parameters::luminance_mask_max);
    hdr.m_eye_adaption_base = tod_blend(&tod_district_parameters::eye_adaption_base);
    hdr.m_eye_adaption_amount = tod_blend(&tod_district_parameters::eye_adaption_amount);
    hdr.m_eye_fade_min = tod_blend(&tod_district_parameters::eye_fade_min);
    hdr.m_eye_fade_max = tod_blend(&tod_district_parameters::eye_fade_max);
    hdr.m_brightpass_threshold = tod_blend(&tod_district_parameters::brightpass_threshold);
    hdr.m_brightpass_offset = tod_blend(&tod_district_parameters::brightpass_offset);
    hdr.m_bloom_amount = tod_blend(&tod_district_parameters::bloom_amount);
    hdr.m_bloom_theta = tod_blend(&tod_district_parameters::bloom_theta);
    hdr.m_bloom_slope_A = tod_blend(&tod_district_parameters::bloom_slope_A);
    hdr.m_bloom_slope_B = tod_blend(&tod_district_parameters::bloom_slope_B);
    hdr.m_lum_range = tod_blend(&tod_district_parameters::tonemap_lum_range);
    hdr.m_lum_offset = tod_blend(&tod_district_parameters::tonemap_lum_offset);

    // Particle lighting: the colors as they are (no HDR conversion), the light pulled up toward +y.
    const vector4 particle_ambient = tod_blend(&tod_district_parameters::particle_ambient_color);
    const vector4 particle_tod = tod_blend(&tod_district_parameters::particle_tod_color);
    const vector particle_dir = tod_shared_calcaulate_light_direction_at_time(Current_time_of_day, TOD_rot_around_y, Tod_particle_light_min_angle_to_horizon);
    const float keep = 1.0f - TOD_particle_light_fudge;
    vector particle_light_dir(particle_dir.x * keep + TOD_particle_light_fudge * 0.0f, keep * particle_dir.y + TOD_particle_light_fudge, keep * particle_dir.z + TOD_particle_light_fudge * 0.0f);
    particle_light_dir.normalize();
    Hdr_state = hdr;

    // Color correction.
    vector4 tint = tod_blend(&tod_district_parameters::color_correct_tint);
    if (TOD_tint_primary_color)
    {
        float largest = tint.y;
        if (0.0f <= tint.x - tint.y)
        {
            largest = tint.x;
        }
        if (largest - tint.z < 0.0f)
        {
            largest = tint.z;
        }
        tint = tint * (1.0f / largest);
    }
    Scene_main_tint = rl_color_float(tint.x, tint.y, tint.z, tint.w);
    Scene_main_saturation = tod_blend(&tod_district_parameters::color_correct_saturation);

    Particle_lighting_state.m_ambient_color = rl_color_float(particle_ambient.x, particle_ambient.y, particle_ambient.z, particle_ambient.w);
    Particle_lighting_state.m_tod_color = rl_color_float(particle_tod.x, particle_tod.y, particle_tod.z, particle_tod.w);
    Particle_lighting_state.m_tod_light_dir = particle_light_dir;

    // Re-inserted into its partition, as after a move.
    rl_partition_system* partition_system_p = Game_time_of_day_light_p != nullptr ? Game_time_of_day_light_p->get_partition_system() : nullptr;
    if (partition_system_p != nullptr && Game_time_of_day_light_p->get_is_visible())
    {
        partition_system_p->insert(Game_time_of_day_light_p);
    }

    // The lights of the level (object_light): off by day, fading out after sunrise and in before sunset.
    if (TOD_light_on_off.x <= Current_time_of_day)
    {
        if (Current_time_of_day < TOD_light_on_off.y)
        {
            Time_of_day_onoff_value = 1.0f - (Current_time_of_day - TOD_light_on_off.x) / (TOD_light_on_off.y - TOD_light_on_off.x);
            return;
        }
        if (Current_time_of_day < TOD_light_on_off.z)
        {
            Time_of_day_onoff_value = 0.0f;
            return;
        }
        if (Current_time_of_day < TOD_light_on_off.w)
        {
            Time_of_day_onoff_value = (Current_time_of_day - TOD_light_on_off.z) / (TOD_light_on_off.w - TOD_light_on_off.z);
            return;
        }
    }
    Time_of_day_onoff_value = 1.0f;
}

// 0x007DF9A0 (:1046)
void game_time_of_day_system_init()
{
    Update_tod_render_task.m_callback = game_time_of_day_render_update;
    Task_list_update_render.append(&Update_tod_render_task);
}

// The two directional lights the level init creates (inlined twice): no attenuation within 10 km, white, a 10 km
// bounding sphere and clip box.
static rl_light* create_tod_light()
{
    rl_light* light_p = rl_light::create();
    light_p->set_attenuation_start(10000.0f);
    light_p->set_light_type(rl_light::LIGHT_TYPE_DIRECTIONAL);
    light_p->set_attenuation_end(10001.0f);
    light_p->set_color(rl_color_float(rl_color::WHITE));
    light_p->set_local_bounding_sphere(rl_sphere(ZERO_VECTOR, 10001.0f));
    light_p->set_clip_min(vector(-10001.0f, -10001.0f, -10001.0f));
    light_p->set_clip_max(vector(10001.0f, 10001.0f, 10001.0f));
    light_p->set_is_visible(false);
    return light_p;
}

// 0x007E9260 (:1053)
void game_time_of_day_level_init_pre()
{
    tod_district_info* infos = (tod_district_info*)Level_mempool.alloc(sizeof(tod_district_info) * TOD_MAX_LOADED_DISTRICTS, 16);
    if (infos == nullptr)
    {
        keen::debug::nativePrintArgs("Int3 critical function at %s(%d)\n", __FILE__, __LINE__);
    }
    else
    {
        for (int i = 0; i < TOD_MAX_LOADED_DISTRICTS; i++)
        {
            new (&infos[i]) tod_district_info();
        }
    }
    TOD_loaded_districts = infos;
    for (int i = 0; i < TOD_MAX_LOADED_DISTRICTS; i++)
    {
        TOD_loaded_districts[i].m_district_handle = 0xFFFFFFFF;
    }
    TOD_rot_around_y = 0.0f;
    Enable_tutorial_hacks = false;
    Current_blend.blend_valid = false;
    Current_time_of_day = 11.0f;
    Game_time_of_day_light_p = create_tod_light();
    Game_drop_shadow_light_p = create_tod_light();
    // The drop shadow light only serves the shadow system; it is taken out of the scene's light list.
    rl_get_active_scene()->remove_light(Game_drop_shadow_light_p);
}

// 0x007EEAA0 (:1120): loads the TOD parameters of every district and makes the sun the scene's largest light.
void game_time_of_day_level_init_post()
{
    for (int i = 0; i < World.get_num_objects<district>(); i++)
    {
        game_time_of_day_load_district(World.get_object<district>(i)->get_handle());
    }
    game_render_set_TOD_light(Game_time_of_day_light_p);
    for (int i = 0; i < TOD_global_info.orbital_objects.size(); i++)
    {
        object_effect* effect_p = game_time_of_day_get_orbital_effect(i);
        if (effect_p != nullptr)
        {
            effect_p->set_skybox_attached_orbital(i);
        }
    }
    game_time_of_day_do_frame();
}

// 0x007C5810 (:1149)
void game_time_of_day_level_shutdown()
{
    Scene_main_p->set_largest_light(nullptr);
    if (Game_time_of_day_light_p != nullptr)
    {
        Game_time_of_day_light_p->release();
        Game_time_of_day_light_p = nullptr;
    }
    if (Game_drop_shadow_light_p != nullptr)
    {
        Game_drop_shadow_light_p->release();
        Game_drop_shadow_light_p = nullptr;
    }
    Num_tod_loaded_districts = 0;
    TOD_loaded_districts = nullptr;
    Current_blend.blend_valid = false;
}

// 0x007DFA10 (:1181): the sky dome's ten colors: the four west hemisphere bands, the zenith, the four east bands and
// the zenith again.
void game_time_of_day_get_skybox_tod_colors(rl_color_float* colors)
{
    for (int i = 0; i < 4; i++)
    {
        hdr_color_from_vector4(colors[i], tod_blend(&tod_district_parameters::west_hemisphere, i));
    }
    hdr_color_from_vector4(colors[4], tod_blend(&tod_district_parameters::zenith));
    for (int i = 0; i < 4; i++)
    {
        hdr_color_from_vector4(colors[5 + i], tod_blend(&tod_district_parameters::east_hemisphere, i));
    }
    hdr_color_from_vector4(colors[9], tod_blend(&tod_district_parameters::zenith));
}

// 0x007E27B0 (:1199): the time from the game clock (or g_TOD), and the cloud scrolling on a clock running
// Cloud_scroll_separate_clock_scale_factor times faster.
void game_time_of_day_do_frame()
{
    game_time_of_day_update_blend();
    if (!TOD_pause)
    {
        Current_time_of_day = ((float)Gamewide_game_clock->m_minutes * 60.0f + (float)Gamewide_game_clock->m_seconds) / 3600.0f + (float)Gamewide_game_clock->m_hour;
    }
    if (g_enableTOD)
    {
        Current_time_of_day = g_TOD;
    }
    const float scroll_hours = Frametime / 3600.0f * Cloud_scroll_separate_clock_scale_factor;
    Cloud_scroll_time = Cloud_scroll_time + scroll_hours;
    if (24.0f <= Cloud_scroll_time)
    {
        Cloud_scroll_time = Cloud_scroll_time - 24.0f;
    }
    if (24.0f <= Current_time_of_day)
    {
        Current_time_of_day = 0.0f;
    }
    if (24.0f <= Cloud_scroll_time)
    {
        Cloud_scroll_time = 0.0f;
    }
    Cloud_layer01_scroll_pos = tod_blend(&tod_district_parameters::cloud_layer01_speed) * scroll_hours + Cloud_layer01_scroll_pos;
    Cloud_layer23_scroll_pos = tod_blend(&tod_district_parameters::cloud_layer23_speed) * scroll_hours + Cloud_layer23_scroll_pos;
    if (10.0f <= Cloud_layer01_scroll_pos)
    {
        Cloud_layer01_scroll_pos = Cloud_layer01_scroll_pos - 10.0f;
    }
    if (10.0f <= Cloud_layer23_scroll_pos)
    {
        Cloud_layer23_scroll_pos = Cloud_layer23_scroll_pos - 10.0f;
    }
    if (TOD_clouds_fixed)
    {
        Cloud_layer01_scroll_pos = 0.0f;
        Cloud_layer23_scroll_pos = 0.0f;
    }
    if (!TOD_cloud_horizon_strenghts_animate)
    {
        Current_horizon_strengths = TOD_cloud_horizon_override;
    }
    else
    {
        Current_horizon_strengths.x = tod_blend(&tod_district_parameters::horizon_layer0_strength);
        Current_horizon_strengths.y = tod_blend(&tod_district_parameters::horizon_layer1_strength);
        Current_horizon_strengths.z = tod_blend(&tod_district_parameters::horizon_layer2_strength);
        Current_horizon_strengths.w = tod_blend(&tod_district_parameters::horizon_layer3_strength);
    }
    if (!TOD_cloud_overhead_strenghts_animate)
    {
        Current_overhead_strengths = TOD_cloud_overhead_override;
        return;
    }
    Current_overhead_strengths.x = tod_blend(&tod_district_parameters::overhead_layer0_strength);
    Current_overhead_strengths.y = tod_blend(&tod_district_parameters::overhead_layer1_strength);
    Current_overhead_strengths.z = tod_blend(&tod_district_parameters::overhead_layer2_strength);
    Current_overhead_strengths.w = tod_blend(&tod_district_parameters::overhead_layer3_strength);
}

// 0x007EEB50 (:1272): <territory>.gtodx, else default.gtodx.
bool game_time_of_day_load_global(const char* filename)
{
    char name[256];
    memset(name, 0, sizeof(name));
    cf_drop_ext(name, sizeof(name), filename);
    sprintf(name + strlen(name), "%s", ".gtodx");
    if (tod_shared_load_global_parameters(TOD_global_info, name, &Common_mempool))
    {
        return true;
    }
    if (tod_shared_load_global_parameters(TOD_global_info, "default.gtodx", &Common_mempool))
    {
        return true;
    }
    TOD_global_info.horizon_mountain_enabled = true;
    TOD_global_info.fog_camera_follow = false;
    return false;
}

// 0x007E9760 (:1294): a debug draw (sun path, district names), stripped like the other debug renderers.
// Deviation from rfg.exe: rfg.exe dereferences rl_getDebugRenderer()'s nullptr (set_primitive_color) when
// Tod_show_sun_path is on; the port leaves the dead body empty.
void game_time_of_day_render_debug()
{
}

// 0x007E31B0 (:1358)
void game_time_of_day_get_cloud_lighting(rl_color_float& horizon_front_ambient, rl_color_float& horizon_back_ambient, rl_color_float& overhead_front_ambient, rl_color_float& overhead_back_ambient, float& backlight_strength, float& backlight_power, vector& light_direction)
{
    hdr_color_from_vector4(horizon_front_ambient, tod_blend(&tod_district_parameters::clouds_horizon_front_light_color));
    hdr_color_from_vector4(horizon_back_ambient, tod_blend(&tod_district_parameters::clouds_horizon_rear_light_color));
    hdr_color_from_vector4(overhead_front_ambient, tod_blend(&tod_district_parameters::clouds_front_light_color));
    hdr_color_from_vector4(overhead_back_ambient, tod_blend(&tod_district_parameters::clouds_rear_light_color));
    backlight_strength = tod_blend(&tod_district_parameters::cloud_backlight_strength);
    backlight_power = tod_blend(&tod_district_parameters::cloud_backlight_power);
    // Magic number: the clouds are lit from at least 15 degrees above the horizon.
    light_direction = tod_shared_calcaulate_light_direction_at_time(Current_time_of_day, TOD_rot_around_y, 15.0f);
}

// 0x007E4590 (:1376)
void get_time_of_day_get_horizon_mountain_params(horizon_mountain_parameters& parameters)
{
    hdr_color_from_vector4(parameters.color_front, tod_blend(&tod_district_parameters::horizon_mountain_color_front));
    hdr_color_from_vector4(parameters.color_back, tod_blend(&tod_district_parameters::horizon_mountain_color_back));
    hdr_color_from_vector4(parameters.fog_color, tod_blend(&tod_district_parameters::horizon_mountain_fog_color));
    parameters.fog_density = tod_blend(&tod_district_parameters::horizon_mountain_fog_density);
    parameters.enabled = TOD_global_info.horizon_mountain_enabled;
}

// 0x007C5880 (:1390)
void game_time_of_day_get_weather_settings(vector4& horizon_strengths, vector4& overhead_strengths)
{
    horizon_strengths = Current_horizon_strengths;
    overhead_strengths = Current_overhead_strengths;
}

// 0x007D20C0 (:1400)
int game_time_of_day_get_num_orbitals()
{
    return TOD_global_info.orbital_objects.size();
}

// 0x007E5400 (:1410): the orbital's position on its circle, scale (in hundredths), tint (HDR converted, the alpha is
// the opacity) and texture (<map_name>.tga through bmpman).
void game_time_of_day_get_orbital_params(int idx, rl_skybox_orbital_params& params)
{
    tod_orbital_object& orbital_obj = TOD_global_info.orbital_objects[idx];
    vector pos;
    tod_shared_calculate_orbital_position(pos, orbital_obj, Current_time_of_day);
    const float scale = orbital_obj.scale.get_value_at_time(Current_time_of_day) / 100.0f;
    const vector4 tint = orbital_obj.tint.get_value_at_time(Current_time_of_day);
    const float opacity = orbital_obj.opacity.get_value_at_time(Current_time_of_day);
    rl_color_float color;
    hdr_color_from_vector4(color, tint);
    char texture_name[260];
    cf_add_ext(texture_name, 256, orbital_obj.map_name, ".tga", false);
    params.position = pos;
    params.scale = scale;
    params.tint = rl_color_float(color.red, color.green, color.blue, opacity);
    params.texture_handle = bm_load(texture_name, false);
    params.alpha_blend = orbital_obj.alpha_blend;
}

// 0x007D20D0 (:1439): the effect of an orbital, started at the orbital's position around the camera on first use.
object_effect* game_time_of_day_get_orbital_effect(int idx)
{
    tod_orbital_object& orbital_obj = TOD_global_info.orbital_objects[idx];
    vector pos;
    tod_shared_calculate_orbital_position(pos, orbital_obj, Current_time_of_day);
    if (orbital_obj.m_effect_info_handle == 0xFFFFFFFF)
    {
        return nullptr;
    }
    if (orbital_obj.m_effect_handle == 0xFFFFFFFF)
    {
        const vector effect_pos(Rfg_camera.real_pos.x + pos.x, Rfg_camera.real_pos.y + pos.y, Rfg_camera.real_pos.z + pos.z);
        orbital_obj.m_effect_handle = effects_play(orbital_obj.m_effect_info_handle, effect_pos, IDENTITY_MATRIX, true, 0xFFFFFFFF, -1, 0xFFFFFFFF, 1.0f, false, EFFECTS_CATEGORY_NONE);
    }
    if (orbital_obj.m_effect_handle != 0xFFFFFFFF)
    {
        object* objp = World.object_from_handle(orbital_obj.m_effect_handle);
        if (objp != nullptr && objp->get_obj_type() == OT_EFFECT)
        {
            return (object_effect*)objp;
        }
    }
    return nullptr;
}

// 0x007C58C0 (:1463)
void game_time_of_day_get_cloud_scroll_values(float& layer01_scroll, float& layer23_scroll)
{
    layer01_scroll = Cloud_layer01_scroll_pos;
    layer23_scroll = Cloud_layer23_scroll_pos;
}

// 0x007E9BE0 (:1475): <territory>_<district name>.dtodx (spaces as underscores); the default district falls back to
// default.dtodx.
bool game_time_of_day_load_district(unsigned int district_handle)
{
    if (game_time_of_day_find_district_file(district_handle) != nullptr)
    {
        return true;
    }
    tod_district_info* info = game_time_of_day_find_free();
    if (info == nullptr)
    {
        keen::debug::nativePrintArgs("game_time_of_day_load_district: failed. no free tod_district_info. district_handle: %u\n", district_handle);
        keen::debug::nativePrintArgs("game_time_of_day_load_district: failed. end of function. district_handle: %u\n", district_handle);
        return false;
    }
    object* objp = district_handle != 0xFFFFFFFF ? World.object_from_handle(district_handle) : nullptr;
    if (objp == nullptr || objp->get_obj_type() != OT_DISTRICT)
    {
        keen::debug::nativePrintArgs("game_time_of_day_load_district: failed. district_pointer == nullptr. district_handle: %u\n", district_handle);
        return false;
    }
    char filename[256];
    sprintf(filename, "%s_%s%s", World.get_territory_name(), objp->get_name(), ".dtodx");
    for (char* space = strstr(filename, " "); space != nullptr; space = strstr(space, " "))
    {
        *space = '_';
    }
    if (!tod_shared_load_district_parameters(info->m_params, filename, &Common_mempool))
    {
        keen::debug::nativePrintArgs("game_time_of_day_load_district: failed to load district parameters. district_handle: %u , district_filename: %s\n", district_handle, filename);
        if (district_handle != district_get_default())
        {
            keen::debug::nativePrintArgs("game_time_of_day_load_district: failed to load default district parameters. district_handle: %u , district_filename: %s\n", district_handle, filename);
            keen::debug::nativePrintArgs("game_time_of_day_load_district: failed. end of function. district_handle: %u\n", district_handle);
            return false;
        }
        sprintf(filename, "default%s", ".dtodx");
        if (!tod_shared_load_district_parameters(info->m_params, filename, &Common_mempool))
        {
            keen::debug::nativePrintArgs("game_time_of_day_load_district: failed. end of function. district_handle: %u\n", district_handle);
            return false;
        }
    }
    info->m_district_handle = district_handle;
    Num_tod_loaded_districts++;
    return true;
}

// 0x007C58F0 (:1550): the prologue's fixed time, 09:30:00 on the current date, with the sun around the world's y axis
// at 0 and the tutorial's fog rules (Enable_tutorial_hacks).
void game_time_of_day_set_tutorial_tod()
{
    TOD_rot_around_y = 0.0f;
    Game_clock.m_hour = 9;
    Game_clock.m_minutes = 30;
    Game_clock.m_seconds = 0;
    game_clock_update_day_ticks_from_time_of_day();
    Enable_tutorial_hacks = true;
}
