// Original: volition/rfg/code/hud/minimap.cpp
//
// The minimap and the full screen map's layers (stub_impl.md S7, UI.md UI9). Single player only: process returns at once
// in multiplayer and wrecking crew. In rfg_ui; the test programs have the constructors in tests/stubs/rfg/hud/hud.cpp.
// Not ported: gps_look_ahead, draw_compass and draw_gps_prototypes (0x0084C420, 0x00869DF0, 0x0086A400): nothing in
// rfg.exe calls draw_gps_prototypes, the only caller of the other two.

#include "volition/rfg/code/hud/hud.h"
#include "volition/rfg/code/hud/minimap.h"
#include "volition/rfg/code/hud/minimap_icon_info.h"

#include "volition/audio_lib/code/audiolib_api.h"
#include "volition/audio_lib/code/audiolib_att_loop_rel.h"
#include "volition/interface_lib/code/bitmap_sheets.h"
#include "volition/interface_lib/code/vint_document.h"
#include "volition/interface_lib/code/objects/vint_element_base.h"
#include "volition/interface_lib/code/vint_project.h"
#include "volition/render_lib/code/gr/gr.h"
#include "volition/render_lib/code/gr/gr_font.h"
#include "volition/render_lib/code/rl_draw.h"
#include "volition/render_lib/code/rl_material.h"
#include "volition/render_lib/code/rl_material_defines.h"
#include "volition/render_lib/code/rl_material_library.h"
#include "volition/render_lib/code/rl_primitive_defines.h"
#include "volition/render_lib/code/rl_primitive_state.h"
#include "volition/render_lib/code/rl_shader_library.h"
#include "volition/render_lib/code/rl_texture_defines.h"
#include "volition/rfg/code/ai/ai_system.h"
#include "volition/rfg/code/camera/rfg_camera.h"
#include "volition/rfg/code/control/control_gameplay.h"
#include "volition/rfg/code/control/control_map.h"
#include "volition/rfg/code/control/control_spectator.h"
#include "volition/rfg/code/destruction/rfg_mover.h"
#include "volition/rfg/code/gameplay/activities/activity.h"
#include "volition/rfg/code/gameplay/activities/activity_car_bomb.h"
#include "volition/rfg/code/gameplay/activities/activity_convoy.h"
#include "volition/rfg/code/gameplay/activities/activity_courier_chase.h"
#include "volition/rfg/code/gameplay/activities/activity_enum.h"
#include "volition/rfg/code/gameplay/activities/activity_house_arrest.h"
#include "volition/rfg/code/gameplay/gameflow/alert_level_manager.h"
#include "volition/rfg/code/gameplay/activities/activity_area_defense_internal.h"
#include "volition/rfg/code/gameplay/activities/activity_courier_chase_internal.h"
#include "volition/rfg/code/gameplay/activities/activity_vrail_internal.h"
#include "volition/rfg/code/gameplay/activities/delivery_common.h"
#include "volition/rfg/code/gameplay/activities/raid_common_internal.h"
#include "volition/rfg/code/gameplay/mission_info.h"
#include "volition/rfg/code/gameplay/missions.h"
#include "volition/rfg/code/gameplay/upgrades.h"
#include "volition/rfg/code/gameplay/wrecking_crew/wrecking_crew.h"
#include "volition/rfg/code/gamewide/game_clock.h"
#include "volition/rfg/code/gamewide/gameaudio.h"
#include "volition/rfg/code/gamewide/gameseq.h"
#include "volition/rfg/code/hud/fog_of_war.h"
#include "volition/rfg/code/hud/fullscreen_map.h"
#include "volition/rfg/code/hud/in_world_gps.h"
#include "volition/rfg/code/interface/ui_document_hud.h"
#include "volition/rfg/code/interface/ui_document_hud_multiplayer.h"
#include "volition/rfg/code/interface/ui_document_hud_tool_tips.h"
#include "volition/rfg/code/interface/ui_document_manager.h"
#include "volition/rfg/code/level/rfg_chunk_shared.h"
#include "volition/rfg/code/level/rfg_terrainx_shared.h"
#include "volition/rfg/code/level/world.h"
#include "volition/rfg/code/multi/multi_core/include/multi.h"
#include "volition/rfg/code/multi/multi_game/multi_game.h"
#include "volition/rfg/code/objects/gameplay_properties.h"
#include "volition/rfg/code/objects/humans/human_info.h"
#include "volition/rfg/code/objects/object_navpoint.h"
#include "volition/rfg/code/weapons/weapons.h"
#include "volition/rfg/code/objects/humans/human.h"
#include "volition/rfg/code/objects/humans/human_teams.h"
#include "volition/rfg/code/objects/humans/player.h"
#include "volition/rfg/code/objects/humans/player_meta_data.h"
#include "volition/rfg/code/objects/movers/general_mover.h"
#include "volition/rfg/code/objects/movers/object_mover.h"
#include "volition/rfg/code/objects/level_object_properties.h"
#include "volition/rfg/code/objects/object_activity_spawn.h"
#include "volition/rfg/code/objects/object_area_defense_node.h"
#include "volition/rfg/code/objects/object_bftp_node.h"
#include "volition/rfg/code/objects/object_bounding_box.h"
#include "volition/rfg/code/objects/object_convoy.h"
#include "volition/rfg/code/objects/object_convoy_end_point.h"
#include "volition/rfg/code/objects/object_courier_end_point.h"
#include "volition/rfg/code/objects/object_delivery_node.h"
#include "volition/rfg/code/objects/object_demolitions_master_node.h"
#include "volition/rfg/code/objects/object_house_arrest_node.h"
#include "volition/rfg/code/objects/object_path_road.h"
#include "volition/rfg/code/objects/object_patrol.h"
#include "volition/rfg/code/objects/object_raid_node.h"
#include "volition/rfg/code/objects/object_restricted_area.h"
#include "volition/rfg/code/objects/object_riding_shotgun_node.h"
#include "volition/rfg/code/objects/object_safehouse.h"
#include "volition/rfg/code/pathfinding/bb_pathfinding.h"
#include "volition/rfg/code/vehicle/vehicle_class.h"
#include "volition/vlib/code/bmpman/bmpman.h"
#include "volition/vlib/code/cfile/cfile.h"
#include "volition/vlib/code/math/floating.h"
#include "volition/vlib/code/math/ix.h"
#include "volition/vlib/code/math/spline.h"
#include "volition/vlib/code/os/frametime.h"
#include "volition/rfg/code/gameplay/object_district.h"
#include "volition/rfg/code/objects/object_zone.h"
#include "volition/rfg/code/objects/projectile.h"
#include "volition/rfg/code/objects/humans/npc.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <wchar.h>
#include <string.h>

// ---- globals
extern int Number_font; // 0x0163F164
static bool Show_important_bldgs = true; // 0x01660FF2
bool Show_minimap = true; // 0x01660FF3
bool Usability_hud_enable = true; // 0x01664895
static unsigned int Most_recent_icon_handle = 0xFFFFFFFF; // 0x016611B4
static float Minimap_gps_radius = 20.0f; // 0x016611B8
static float Minimap_gps_delay = 2.0f; // 0x016611BC
// Only bit 0 is read: whether the minimap draws the GPS path (the tens were draw_gps_prototypes's display, not called).
static int Minimap_gps_display = 1; // 0x016611C0
static float Minimap_min_radius = 40.0f; // 0x016611D0
static float Minimap_max_radius = 80.0f; // 0x016611D4
static float Minimap_zoom_speed = 50.0f; // 0x016611D8
static float Minimap_radar_update_rate = 150.0f; // 0x016611DC
static float Minimap_radar_blip_scale = 2.0f; // 0x016611E0
static float Minimap_radar_blip_scale_rate = 6.0f; // 0x016611E4
static float Fsm_min_icon_scale = 0.5f; // 0x016611E8
static float Minimap_min_icon_scale = 0.75f; // 0x016611EC
static float Mini_static_fade_rate = 0.75f; // 0x016611F0
static float Mini_static_max_alpha = 0.9f; // 0x016611F4
static int Blip_color_add = 128; // 0x016611F8
static float Minimap_blip_opacity = 128.0f; // 0x016611FC
static float Minimap_blip_start_scale = 0.5f; // 0x01661200
static float GPS_trail_rate = 16.0f; // 0x01661204
static int Distance_between_sections = 16; // 0x01661208
static float Min_GPS_dist = 10.0f; // 0x0166120C
static float Fsm_min_visible_dist = 0.2f; // 0x01661210
static float Fsm_max_visible_dist = 0.1f; // 0x01661214
static float Fsm_num_path_points_scalar = 1.5f; // 0x01661218
static float Fsm_repulse_icons_radius_mod = 0.7f; // 0x0166121C
static float Fsm_icon_scale = 1.5f; // 0x01661220
static float Mini_icon_scale = 1.0f; // 0x01661224
static float Liberated_icon_scale_zoom_in = 2.25f; // 0x01661228
static float Liberated_icon_scale_zoom_out = 0.5f; // 0x0166122C
static float Mini_bldg_collect_radius = 45.0f; // 0x01661230
static float Mining_alpha_depth = 35.0f; // 0x01661234
static int GPS_end_line_alpha = 128; // 0x01661238
static int Gps_lookahead_points = 1; // 0x0166123C
static const char* Map_element_string_mask = "map_mini_mask.tga"; // 0x01661240
static const char* Map_element_string_radar_scan = "ui_minimap_scan_line.tga"; // 0x01661244
static const char* Map_element_string_static = "ui_fsmap_static.tga"; // 0x01661248
static float North_icon_pixel_size = 20.0f; // 0x0166124C
static float Map_icon_pixel_size = 13.0f; // 0x01661250
static int Minimap_render_closest_num = 3; // 0x01661254
static int Bftp_icon_lifetime_ms = 750; // 0x01661258
static int Bftp_icon_timer_ms = 3000; // 0x0166125C
static float Font_scale = 0.5f; // 0x01661264
static int trail_size = 4; // 0x0166273C
color Default_map_item_color(255, 255, 255, 255); // 0x016621A0
color Completed_icon_color(130, 130, 130, 255); // 0x016621A4
static color EDF_route_color(0x94, 0x0D, 0x00, 0xFF); // 0x016621A8
static color EDF_route_highlight_color(0xFC, 0x16, 0x00, 0xFF); // 0x016621AC
static color RF_route_color(0x78, 0x64, 0x1A, 0xFF); // 0x016621B0
static color RF_route_highlight_color(0xF0, 0xC8, 0x34, 0xFF); // 0x016621B4
static color GPS_color(0xE6, 0x9E, 0x02, 0xFF); // 0x016621B8
static color GPS_highlight_color(0xFA, 0xE6, 0x00, 0xFF); // 0x016621BC

static const char* cProp_map_name = "map_overlay"; // 0x01661194
static const char* cProp_screen_size_minimap = "screen_size"; // 0x01661198
static const char* cEdf_building_texture = "map_mini_generic_building.tga"; // 0x0166119C
static const char* cGeneric_building_texture = "map_mini_generic_building.tga"; // 0x016611A0
static int Char_padding = 2; // 0x016611A8
static int X_padding = 10; // 0x016611AC
static int Y_padding = 10; // 0x016611B0
bool Rl_clamp_minimap = true; // 0x01661260
float Player_scale = 5.0f; // 0x01661268
extern float Scroll_speed; // 0x01660DD4, fullscreen_map.cpp's
static float icon_fudge = 0.75f; // 0x01662740
static float one_over_cos_of_45 = 1.41421f; // 0x01662BD0
extern float Lock_target_dist_squared; // 0x02BB71F4, fullscreen_map.cpp's
static rl_material_data Edf_mat_data; // 0x016621C0
static rl_material_data Non_edf_mat_data; // 0x016621DC
static rl_texture_desc Edf_textures[2]; // 0x02B95674
static rl_texture_desc Non_edf_textures[2]; // 0x02B958FC
static rl_material_const Edf_structure_color_const[6]; // 0x02B957F8
static rl_material_const Non_edf_structure_color_const[6]; // 0x02B9DA30
static int Map_element_id_radar_scan; // 0x02B953DC
static int Map_element_id_static; // 0x02B953F4
static int Map_element_id_mask; // 0x02B95588
static int Map_element_id_overlay; // 0x02B9DA28
extern int Num_gps_refined_path_segments; // 0x02B9DD38, in_world_gps.cpp's
bool Will_switch_to_full; // 0x02B9DD48
bool Found_icon; // 0x02B9DD49
static bool Add_temp_icons; // 0x02B9DD4A
bool Minimap_render_humans; // 0x02B9DD4B
static int Current_gps_point; // 0x02B9DD4C
map_icon_temp* Most_recent_icon; // 0x02B9DD50
bool Minimap_render_restricted; // 0x02B9DD54
bool Minimap_render_old_mode; // 0x02B9DD55
bool Minimap_show_gps_lookahead; // 0x02B9DD56
bool Minimap_show_compass; // 0x02B9DD57
bool Zoom_map_out_on_start; // 0x02B9DD58, fullscreen_map.cpp reads it
// The map rotation option (the game options set it; UI.md UI6).
bool Minimap_rot0; // 0x02B9DD5A
bool Minimap_scissor_enabled; // 0x02B9DD5B
static float Minimap_radar_min_alpha; // 0x02B9DD5C
bool use_3d_spline_gps; // 0x02B9DD60
cartographer Cartographer; // 0x02BB71F8
static checksum_stri Fullscreen_map_tool_tip_name_crc; // 0x02BD80CC
static vector2 Resolution_minimap_scale; // 0x02BD9140
static rl_color Structure_colors[6]; // 0x02BE5120

// Magic numbers: Fs_info.zoom_level runs from 0.1 (zoomed out) to 0.8.
static float fsm_zoom_percent()
{
    return (Fs_info.zoom_level - 0.1f) / 0.7f;
}

// Whether the full screen map is up and drawn. Inlined in rfg.exe.
static bool fsm_is_up()
{
    return !Fs_info.flags.wants_exit && !Fs_info.flags.exiting && Fs_info.flags.fully_loaded && Fs_info.flags.rendered_a_grid;
}

// 0x0084C310 (:372)
void minimap_gr_convert_to_rl_state(rl_primitive_state& rls, const gr_state& grs)
{
    gr_convert_to_rl_state(rls, grs);
    rls.set_scissor_enabled(Minimap_scissor_enabled);
}

// 0x0086C4D0 (:1075)
static district* district_from_name(const char* name)
{
    for (int i = 0; i < World.get_num_objects<district>(); i++)
    {
        district* dp = World.get_object<district>(i);
        if (v_stricmp(dp->get_name(), name) == 0)
        {
            return dp;
        }
    }
    return nullptr;
}

// 0x0086C540 (:1089)
static checksum_stri get_fullscreen_map_tool_tip_and_zoom_out_info(bool& zoom_out_on_start)
{
    const char* name = mission_get_name();
    const bool in_intro = name != nullptr && v_stricmp(name, "Intro 1") == 0;
    const bool opened_in_intro = Fullscreen_map_has_been_opened_through_script && in_intro;
    if (ui_tool_tips_tips_are_enabled() && (mission_is_complete("Intro 1") || opened_in_intro))
    {
        if (!ui_tool_tips_tip_has_been_played("Use_GPS", false))
        {
            return checksum_stri("Use_GPS", 0);
        }
        if (!ui_tool_tips_tip_has_been_played("Map_zoom", false))
        {
            const district* parker = district_from_name("Parker");
            if (parker != nullptr && parker->is_liberated())
            {
                zoom_out_on_start = true;
                return checksum_stri("Map_zoom", 0);
            }
        }
    }
    return checksum_stri::INVALID;
}

// 0x0084C700 (:1123)
// pos is not used.
map_icon::map_icon(unsigned int in_obj, int in_bitmap, vector2 pos)
{
    bitmap = in_bitmap;
    handle = in_obj;
    only_show_closest = false;
    time_added.set(0);
}

// 0x0085B4D0 (:1129)
static bool cartographer_mover_collect_cb(object* hit_objp, unsigned int shape_key, const void* user_data)
{
    if (hit_objp->get_obj_type() != OT_MOVER)
    {
        return false;
    }
    bool is_child_piece;
    if (hit_objp->get_sub_type() == OT_SUB_MOVER_RFG)
    {
        rfg_mover* mp = (rfg_mover*)hit_objp;
        vector local_min, local_max;
        mp->get_local_bbox(local_min, local_max);
        if (mp->flags.mining && !mp->flags.game_destroyed)
        {
            return true;
        }
        // Magic number: 2 square meters, smaller than that is not drawn.
        if ((local_max.z - local_min.z) * (local_max.x - local_min.x) < 2.0f)
        {
            return false;
        }
        if (mp->get_importance_level() < 2)
        {
            vector bmin, bmax;
            mp->get_bbox(bmin, bmax);
            // Original bug kept: bmin - bmin, so the "center" is the corner bmin.
            vector center((bmin.x - bmin.x) * 0.5f + bmin.x, 0.0f, (bmin.z - bmin.z) * 0.5f + bmin.z);
            const vector map_center = Cartographer.get_center_pos_world();
            if (Cartographer.get_zoom_radius() + Mini_bldg_collect_radius < vec_xz_dist(center, map_center))
            {
                return false;
            }
        }
        const unsigned int parent = mp->get_parent();
        const object* parent_objp = parent != 0xFFFFFFFF ? World.object_from_handle(parent) : nullptr;
        is_child_piece = parent_objp != nullptr && parent_objp->get_obj_type() == OT_MOVER && mp->flags.inherit_damage_pct;
        if (mp->get_move_type() != 0)
        {
            return false;
        }
        // Magic number: bit 0x15 of the object_mover flags.
        if ((mp->mover_flags >> 0x15 & 1) != 0)
        {
            return true;
        }
        // A piece that takes its damage from a parent mover is collected, other movers are not.
        return is_child_piece;
    }
    object_mover* mp = (object_mover*)hit_objp;
    if (mp->get_importance_level() < 1)
    {
        return false;
    }
    if ((mp->mover_flags >> 0x15 & 1) == 0 || mp->team != HUMAN_TEAM_EDF)
    {
        return false;
    }
    vector local_min, local_max;
    mp->get_local_bbox(local_min, local_max);
    if ((local_max.z - local_min.z) * (local_max.x - local_min.x) < 2.0f)
    {
        return false;
    }
    if (mp->get_importance_level() < 2)
    {
        vector bmin, bmax;
        mp->get_bbox(bmin, bmax);
        // Original bug kept: bmin - bmin, as above.
        vector center((bmin.x - bmin.x) * 0.5f + bmin.x, 0.0f, (bmin.z - bmin.z) * 0.5f + bmin.z);
        const vector map_center = Cartographer.get_center_pos_world();
        if (Cartographer.get_zoom_radius() + Mini_bldg_collect_radius < vec_xz_dist(center, map_center))
        {
            return false;
        }
    }
    if (World.object_from_handle(mp->get_parent()) == nullptr)
    {
        return true;
    }
    // Magic number: 0x100000, a gameplay_properties chunk flag.
    return (mp->gameplay_props->chunk_flags & 0x100000) != 0;
}

// 0x0086C610 (:1207)
static bool cartographer_mover_collect_full_cb(object* hit_objp, unsigned int shape_key, const void* user_data)
{
    if (hit_objp->get_obj_type() != OT_MOVER)
    {
        return false;
    }
    // Magic number: 4, an object_mover flag.
    return Cartographer.get_structure_importance_icon((object_mover*)hit_objp) > 0 && (((object_mover*)hit_objp)->mover_flags & 4) == 0;
}

// 0x0084C860 (:1242)
void get_uv_information_bmpsheet(const bm_image* image_data, const int bmp_handle, vector2& tl, vector2& tr, vector2& bl, vector2& br)
{
    if (image_data == nullptr)
    {
        return;
    }
    int w, h;
    float scale_x, scale_y;
    bm_get_dimensions(bmp_handle, w, h, scale_x, scale_y);
    const float u_scale = (1.0f / (float)w) / scale_x;
    const float v_scale = (1.0f / (float)h) / scale_y;
    const float v0 = v_scale * (float)image_data->sy;
    bl.x = u_scale * (float)image_data->sx;
    bl.y = v0;
    br.x = (float)(image_data->bmw + image_data->sx) * u_scale;
    br.y = v0;
    const float v1 = (float)(image_data->bmh + image_data->sy) * v_scale;
    tl.x = bl.x;
    tl.y = v1;
    tr.x = br.x;
    tr.y = v1;
}

// 0x0084C960 (:1263) (LTCG: bmax in EAX, bottom_right in EDX, top_right in EDI, bottom_left in ESI)
// The box's corners turned by the camera's flattened right vector; only x and z are written.
static void get_converted_bbox(const vector& bmin, const vector& bmax, const vector& rvec, vector& top_left, vector& top_right, vector& bottom_left, vector& bottom_right)
{
    const float nz = -rvec.z;
    const float rx = rvec.x;
    top_left.x = bmax.z * nz + bmin.x * rx;
    top_left.z = bmax.z * rx - bmin.x * nz;
    top_right.x = bmax.z * nz + bmax.x * rx;
    top_right.z = bmax.z * rx - bmax.x * nz;
    bottom_left.x = bmin.z * nz + bmin.x * rx;
    bottom_left.z = bmin.z * rx - bmin.x * nz;
    bottom_right.x = bmin.z * nz + bmax.x * rx;
    bottom_right.z = bmin.z * rx - bmax.x * nz;
}

// 0x0085B8A0 (LTCG: xform in ESI, source_size in EDI). screen is rfg.exe's first local, the bitmap's center.
static void map_render_draw_bitmap_internal(int bitmap_handle, const float* xform, const vector2& screen, const vector2& pos, const unsigned int* source_size, rl_primitive_state state)
{
    gr_set_texture(bitmap_handle);
    const rl_color_float& clr = rl_draw::get_instance()->get_rl_state().get_primitive_color();
    const float red = clr.red;
    const float green = clr.green;
    const float blue = clr.blue;
    const float alpha = clr.alpha;
    int w, h;
    float scale_x, scale_y;
    bm_get_dimensions(bitmap_handle, w, h, scale_x, scale_y);
    const float u_scale = (1.0f / (float)w) / scale_x;
    const float v_scale = (1.0f / (float)h) / scale_y;
    const float u0 = (float)(int)pos.x * u_scale;
    const float v0 = (float)(int)pos.y * v_scale;
    const float u1 = (float)source_size[0] * u_scale + u0;
    const float v1 = (float)source_size[1] * v_scale + v0;
    rl_draw::get_instance()->tristrip_2d_begin(state);
    rl_primitive_2d_vert* verts = rl_draw::get_instance()->tristrip_2d_allocate_tris(2);
    if (verts != nullptr)
    {
        // xform: elements 0, 3 the bitmap's width vector; 1, 4 its height vector; 2, 5 its corner from the center.
        const float x = xform[2] + screen.x;
        const float y = xform[5] + screen.y;
        verts[0].pt.x = x;
        verts[0].pt.y = y;
        verts[0].uvs.x = u0;
        verts[0].uvs.y = v0;
        verts[1].pt.x = xform[0] + x;
        verts[1].pt.y = xform[3] + y;
        verts[1].uvs.x = u1;
        verts[1].uvs.y = v0;
        verts[2].pt.x = xform[1] + x;
        verts[2].pt.y = xform[4] + y;
        verts[2].uvs.x = u0;
        verts[2].uvs.y = v1;
        verts[3].pt.x = xform[1] + xform[0] + x;
        verts[3].pt.y = xform[4] + xform[3] + y;
        verts[3].uvs.x = u1;
        verts[3].uvs.y = v1;
        for (int i = 0; i < 4; i++)
        {
            verts[i].color.red = (unsigned char)(int)(red * 255.0f + 0.5f);
            verts[i].color.green = (unsigned char)(int)(green * 255.0f + 0.5f);
            verts[i].color.blue = (unsigned char)(int)(blue * 255.0f + 0.5f);
            verts[i].color.alpha = (unsigned char)(int)(alpha * 255.0f + 0.5f);
        }
        rl_draw::get_instance()->tristrip_2d_end();
    }
}

// 0x0086C650 (:1352) (LTCG: path in ESI)
// The route's spline as evenly spaced points for the arrows; z is the point's index along the route, for the trail.
// dots and line_to_dest and dest of the original signature were dropped by the compiler.
static void create_path_route_points(path_info& path, bool is_full, float zoom_level)
{
    cardinal_spline_2d spline(path.num_spline_pts, path.spline_points, 0.0f);
    // Magic numbers: 384, the capacity of pts: no end of line yet. 17.5 and 10, the points' spacing in meters.
    path.last_line_index = 0x180;
    const float spacing = (float)(int)floor((double)zoom_level) * (is_full ? 17.5f : 10.0f);
    path.pts.clear();
    int index = 0;
    float dist = 0.0f;
    vector2 last_added(0.0f, 0.0f);
    vector2 last_point;
    float u = 1.0f;
    bool first = true;
    if ((double)(spline.num_pts - 2) < 1.0)
    {
        return;
    }
    do
    {
        vector2 point;
        spline.get_point(point, u);
        dist = (first ? 0.0f : vec2_dist(point, last_point)) + dist;
        last_point = point;
        if (first || spacing <= dist)
        {
            if (spacing < dist && path.pts.size() - 1 > -1)
            {
                vector2 dir(point.x - last_added.x, point.y - last_added.y);
                if (0.0f < dir.x * dir.x + dir.y * dir.y)
                {
                    dir.normalize();
                    point.x = dir.x * spacing + last_added.x;
                    point.y = dir.y * spacing + last_added.y;
                }
            }
            if (0.0f < dist)
            {
                dist = dist - spacing;
            }
            last_added = point;
            // Magic numbers: the minimap keeps points up to twice its radius (the radius squared, times 4).
            const float radius_scale = Cartographer.get_map_mode() == MAP_MODE_FULL ? 1.0f : 4.0f;
            const vector center = Cartographer.get_center_pos_world();
            if ((center.z - point.y) * (center.z - point.y) + (center.y - 0.0f) * (center.y - 0.0f) + (center.x - point.x) * (center.x - point.x) <= Cartographer.get_current_size_info().zoom_radius_squared * radius_scale)
            {
                path.pts.add(vector(point.x, point.y, (float)index));
                if (path.pts.size() == 0x180)
                {
                    return;
                }
            }
            index++;
        }
        u = u + 0.1f;
        first = (float)(spline.num_pts - 2) < u;
    } while (u <= (float)(spline.num_pts - 2));
}

// 0x0085BC00 (:1454)
bool set_icon_zoomed_alpha(minimap_elements icon_bitmap, bool is_full, int& new_alpha, int base_alpha)
{
    new_alpha = base_alpha;
    if (is_full && (icon_bitmap > MAP_ELEMENT_CAMERA_CONE || !minimap_element_is_visible_when_zoomed(icon_bitmap)))
    {
        float percent = fsm_zoom_percent();
        if (percent <= Fsm_max_visible_dist)
        {
            new_alpha = 0;
            return true;
        }
        if (percent <= Fsm_min_visible_dist)
        {
            percent = (percent - Fsm_max_visible_dist) / (Fsm_min_visible_dist - Fsm_max_visible_dist);
            if (0.0f <= 0.0f - percent)
            {
                percent = 0.0f;
            }
            new_alpha = (int)((float)base_alpha * percent);
            gr_set_alpha((int)((float)base_alpha * percent));
        }
    }
    return false;
}

// 0x0083FD50 (.h:219)
bool map_icon::operator==(const map_icon& test_icon)
{
    return handle == test_icon.handle;
}

// 0x0083FD60 (.h:262)
map_icon_temp::map_icon_temp(const vector& in_pos, unsigned int in_activity_handle)
{
    pos.x = in_pos.x;
    pos.y = in_pos.z;
    activity_handle = in_activity_handle;
}

// 0x0084CAB0 (:1471)
bool map_icon_temp::operator==(const map_icon_temp& test_icon)
{
    if (activity_handle == 0xFFFFFFFF || test_icon.activity_handle == 0xFFFFFFFF)
    {
        return pos.x == test_icon.pos.x && pos.y == test_icon.pos.y;
    }
    return activity_handle == test_icon.activity_handle || (pos.x == test_icon.pos.x && pos.y == test_icon.pos.y);
}

// 0x0084CB50 (:1486) (LTCG: end in EAX, start in ECX)
static float get_directional_arrow_angle(const vector2& start, const vector2& end)
{
    // Magic number: 3.14154, rfg.exe's approximation of pi here.
    return 3.14154f - (float)atan2((double)(end.x - start.x), (double)(end.y - start.y));
}

// 0x0084CB90 (:1505) (LTCG: target_player in EAX, icon_bitmap in ESI; only obj_pos.y is passed)
// A mission objective above or below the player gets the up or down arrow; 2 meters either way count as level.
static void adjust_objective_height(minimap_elements& icon_bitmap, float obj_pos_y, player* target_player)
{
    if (icon_bitmap != MAP_ELEMENT_MISSION_OBJECTIVE || target_player == nullptr)
    {
        return;
    }
    float height = target_player->pos.y;
    const vehicle* vp = human_in_vehicle(target_player);
    if (vp != nullptr && vp->get_sub_type() == OT_SUB_VEHICLE_FLYER)
    {
        height = vp->pos.y;
    }
    if (height + 2.0f < obj_pos_y)
    {
        icon_bitmap = MAP_ELEMENT_MISSION_OBJECTIVE_UP;
    }
    else if (obj_pos_y < height - 2.0f)
    {
        icon_bitmap = MAP_ELEMENT_MISSION_OBJECTIVE_DOWN;
    }
}

// 0x0084CC20 (:1534)
void highlight_item::clear()
{
    m_dsqr = 3.4028235e+38f; // FLT_MAX
    m_type = IIT_LAST;
    m_valid = false;
    m_obj_handle = 0xFFFFFFFF;
    m_act_handle = 0xFFFFFFFF;
    m_icon_index = -1;
    m_angle_rad = 0.0f;
    m_col = Default_map_item_color;
    m_obj_pos.x = 0.0f;
    m_obj_pos.y = 0.0f;
    m_obj_pos.z = 0.0f;
}

// 0x0084CC70 (:1548)
void highlight_item::set(highlight_item& item)
{
    m_type = item.m_type;
    m_valid = item.m_valid;
    m_obj_handle = item.m_obj_handle;
    m_act_handle = item.m_act_handle;
    m_icon_index = item.m_icon_index;
    m_dsqr = item.m_dsqr;
    m_angle_rad = item.m_angle_rad;
    m_col = item.m_col;
    m_obj_pos = item.m_obj_pos;
    m_fake_pos = item.m_fake_pos;
    m_render_pos = item.m_render_pos;
}

// 0x00877470 (:1573)
// The member arrays, hash table and timestamps construct themselves (farray capacities, invalid timers); the rest is
// set here, the zoom from the minimum radius.
cartographer::cartographer()
    : visible_over_fow_list(false) // the table is cleared (slots 0xFF, free chain 0..0xFE)
{
    m_determine_onscreen_posn = false;
    m_gps_timer = 0.0f;
    m_gps_index = 0.0f;
    m_bftp_timer = 0;
    m_mats_created = false;
    m_mini_size_info.zoom_radius = Minimap_min_radius;
    m_current_min_zoom = Minimap_min_radius;
}

// The destructor 0x00877380 is compiler-generated (no line info) and left implicit.

// 0x0085BCF0 (:1580)
void cartographer::shutdown()
{
    if (m_edf_bldg_mat != nullptr)
    {
        rl_material_library_unregister(m_edf_bldg_mat);
        m_edf_bldg_mat->release();
    }
    if (m_non_edf_bldg_mat != nullptr)
    {
        rl_material_library_unregister(m_non_edf_bldg_mat);
        m_non_edf_bldg_mat->release();
    }
    m_edf_bldg_mat_handle = 0xFFFFFFFF;
    m_non_edf_bldg_mat_handle = 0xFFFFFFFF;
    m_mats_created = false;
}

// 0x0087FF50 (:1587)
void cartographer::level_init_pre()
{
    Resolution_minimap_scale = vint_project_get_scale(VINT_RES_SCALE_MAINTAIN_RATIO, VINT_RES_SCALE_MAINTAIN_RATIO);
    // Magic numbers: the pixel sizes at the UI's resolution.
    Map_icon_pixel_size = Resolution_minimap_scale.y * 13.0f;
    North_icon_pixel_size = Resolution_minimap_scale.y * 20.0f;
    Add_temp_icons = false;
    m_replace_gps_on_add = false;
    Map_element_id_overlay = vint_bms_load("ui_map_overlay");
    Map_element_id_mask = bm_load(Map_element_string_mask, false);
    Map_element_id_radar_scan = bm_load(Map_element_string_radar_scan, false);
    Map_element_id_static = bm_load(Map_element_string_static, false);
    refresh_minimap_icon_bitmap_ids();
    m_vehicle_posn = 0;
    m_vehicle_sums = 0.0f;
    m_vehicle_speed.clear();
    m_icons.clear();
    m_temp_icons.clear();
    Found_icon = false;
    Most_recent_icon = nullptr;
    m_sticky_icons.clear();
    m_decals.clear();
    m_bmp_info.clear();
    m_important_bldgs.clear();
    m_patrol_routes.clear();
    m_gps_path.pts.clear();
    m_convoy_route.handle = 0xFFFFFFFF;
    for (int i = 0; i < NUM_MAP_ELEMENTS; i++)
    {
        const int bitmap_id = get_minimap_bitmap_id(i);
        if (bitmap_id >= 0)
        {
            cached_bmp_info* info = m_bmp_info.add_in_place();
            info->mii.image_data = vint_bms_get_image_data(bitmap_id);
            info->mii.bitmap_handle = vint_bms_get_bitmap(bitmap_id);
            get_uv_information_bmpsheet(info->mii.image_data, info->mii.bitmap_handle, info->bui.uvs[0], info->bui.uvs[1], info->bui.uvs[2], info->bui.uvs[3]);
        }
    }
    m_collected_structures[0].num_movers = 0;
    m_collected_structure_buf_index = 0;
    m_collected_structures_timer[0].set(0);
    m_map_mode = MAP_MODE_MINI;
    m_counts[0] = 0;
    m_counts[1] = 0;
    m_counts[2] = 0;
    m_human_list.clear();
    m_vehicle_list.clear();
    m_mission_gps = false;
    m_determine_onscreen_posn = true;
    m_has_player_marker = false;
    m_player_marker_item.m_valid = false;
    m_player_marker_item.m_obj_handle = 0xFFFFFFFF;
    m_static_target = 0xFFFFFFFF;
    m_static_radius = 0.0f;
    if (!m_mats_created)
    {
        m_edf_bldg_mat_handle = 0xFFFFFFFF;
        m_non_edf_bldg_mat_handle = 0xFFFFFFFF;
        m_non_edf_bldg_mat = nullptr;
        m_edf_bldg_mat = nullptr;
        // Magic numbers: the structure colors, red first (little endian 0xAABBGGRR).
        Structure_colors[0] = rl_color(0xFF, 0x00, 0x00, 0xFF);
        Structure_colors[1] = rl_color(0x00, 0x0D, 0x94, 0xFF);
        Structure_colors[2] = rl_color(0x00, 0x00, 0xDC, 0xFF);
        Structure_colors[3] = rl_color(0xFF, 0x00, 0xFF, 0xFF);
        Structure_colors[4] = rl_color(0x37, 0x37, 0x37, 0xFF);
        Structure_colors[5] = rl_color(0x37, 0x37, 0x37, 0xFF);
        m_mats_created = create_materials(&m_edf_bldg_mat, &m_non_edf_bldg_mat);
    }
    visible_over_fow_list.clear();
}

// 0x00882D00 (:1697)
void cartographer::render()
{
    m_last_highlight.set(m_highlight);
    m_highlight.clear();
    if (m_has_player_marker && Add_temp_icons)
    {
        // The marker's activity is gone from the temporary icons, or its mission marker's objective moved.
        const unsigned int act_handle = m_player_marker_item.m_act_handle;
        const int icon = m_player_marker_item.m_icon_index;
        if (act_handle == 0xFFFFFFFF || icon == MAP_ELEMENT_RAID_ICON || icon == MAP_ELEMENT_DEMOLITIONS_MASTER || icon == MAP_ELEMENT_GUERRILLA_EXPRESS)
        {
            if (m_mission_gps && m_mission_gps_pos.x == m_player_marker_item.m_obj_pos.x && m_mission_gps_pos.y == m_player_marker_item.m_obj_pos.y && m_mission_gps_pos.z == m_player_marker_item.m_obj_pos.z)
            {
                remove_player_marker(true);
            }
        }
        else
        {
            object* objp = World.object_from_handle(act_handle);
            if (objp != nullptr)
            {
                map_icon_temp test(objp->pos, act_handle);
                if (m_temp_icons.find(test, false) < 0)
                {
                    remove_player_marker(false);
                }
            }
        }
    }
    set_current_map_size_info(m_map_mode == MAP_MODE_MINI ? m_mini_size_info : m_full_size_info);
    if (m_set_gps_marker_to_activity)
    {
        const bool on_mission_pos = m_mission_gps && m_player_marker_pos.x == m_mission_gps_pos.x && m_player_marker_pos.y == m_mission_gps_pos.y && m_player_marker_pos.z == m_mission_gps_pos.z;
        if (!on_mission_pos && m_has_player_marker && m_player_marker_item.m_act_handle != 0xFFFFFFFF)
        {
            if (m_has_player_marker)
            {
                audiolib_cue_play(Ga_cue_ids.map_gps_point_removed, nullptr, nullptr);
            }
            m_has_player_marker = false;
            m_player_marker_item.m_valid = false;
            m_player_marker_item.m_obj_handle = 0xFFFFFFFF;
            m_player_marker_item.m_act_handle = 0xFFFFFFFF;
            m_gps_path.pts.clear();
            Num_gps_refined_path_segments = 0;
            if (m_mission_gps && mission_is_active())
            {
                add_player_marker(m_mission_gps_pos, true, 0xFFFFFFFF, false, false);
            }
        }
        else
        {
            set_gps_to_spawned_activity();
        }
        m_set_gps_marker_to_activity = false;
    }
    if (m_map_mode == MAP_MODE_MINI)
    {
        if (Usability_hud_enable)
        {
            render_mini();
        }
        if (!Will_switch_to_full)
        {
            Add_temp_icons = true;
            Found_icon = false;
            m_temp_icons.clear();
            Most_recent_icon = nullptr;
        }
        return;
    }
    m_gps_timer = Minimap_gps_delay;
    Add_temp_icons = false;
    render_full();
}

// 0x0086CAD0 (:1762)
void cartographer::render_pre_fog()
{
    if (m_map_mode != MAP_MODE_FULL)
    {
        return;
    }
    set_current_map_size_info(m_full_size_info);
    gr_state state(GM_FILTER);
    minimap_gr_convert_to_rl_state(m_rl_render_state, state);
    gr_set_scissor_region((int)m_full_size_info.map_posn.x, (int)m_full_size_info.map_posn.y, (int)m_full_size_info.map_end_posn.x, (int)m_full_size_info.map_end_posn.y, 0);
    if (Minimap_render_restricted)
    {
        render_restricted_areas();
    }
}

// 0x00877710 (:1793)
void cartographer::handshake()
{
    if (Show_minimap && Usability_hud_enable && m_map_mode == MAP_MODE_MINI)
    {
        collect_terrain();
    }
    m_collected_structure_buf_index = m_collected_structure_buf_index == 0 ? 1 : 0;
}

// 0x0087CE30 (:1817)
void cartographer::process()
{
    const vector2 scale = vint_project_get_scale(VINT_RES_SCALE_MAINTAIN_RATIO, VINT_RES_SCALE_MAINTAIN_RATIO);
    if (Resolution_minimap_scale.x != scale.x || Resolution_minimap_scale.y != scale.y)
    {
        Map_icon_pixel_size = scale.y * 13.0f;
        North_icon_pixel_size = scale.y * 20.0f;
        Resolution_minimap_scale = scale;
        m_determine_onscreen_posn = true;
    }
    if (Multiplayer || wrecking_crew_is_active())
    {
        return;
    }
    update_bldg_health_flash_alpha();
    if (m_map_mode == MAP_MODE_MINI)
    {
        if (game_is_paused())
        {
            Add_temp_icons = false;
        }
        change_minimap_zoom();
        set_current_map_size_info(m_mini_size_info);
        vector pos(Rfg_camera.real_pos);
        player* pp = get_target_player();
        if (pp != nullptr)
        {
            pos = pp->pos;
        }
        // Magic number: 100 meters around the player are uncovered.
        fog_of_war_update(pos, 100.0f);
        determine_nearest_safehouse();
        if (gameseq_get_state() == GS_GAMEPLAY && controls_is_button_just_pressed(CBA_GPS_MARKER, Player_controller_index))
        {
            m_set_gps_marker_to_activity = true;
        }
    }
    else
    {
        set_current_map_size_info(m_full_size_info);
    }
    m_objective_selected_time = m_objective_selected_time + Frametime_real;
    m_gps_index = m_gps_index - GPS_trail_rate * Frametime_real;
    if (m_gps_index < 0.0f)
    {
        m_gps_index = (float)Distance_between_sections;
    }
    m_convoy_index = m_convoy_index - GPS_trail_rate * Frametime_real;
    if (m_convoy_index < 0.0f)
    {
        m_convoy_index = (float)Distance_between_sections;
    }
    determine_onscreen_position();
    if (m_map_mode == MAP_MODE_MINI)
    {
        determine_map_information_mini();
    }
    else
    {
        determine_map_information_full();
    }
    if (!game_is_paused())
    {
        m_decals.clear();
    }
    clear_invalid_icons();
    if (!Multiplayer && !wrecking_crew_is_active() && control_player_input_enabled() && controls_process_button_just_pressed(CBA_MAP, Player_controller_index)
        && m_map_mode == MAP_MODE_MINI && !game_is_paused() && gameseq_get_pending_state() != GS_INGAME_OPTIONS && hud_ui_get_loaded_module() != HUD_MODULE_SATELLITE)
    {
        bool zoom_out_on_start = false;
        const checksum_stri tip = get_fullscreen_map_tool_tip_and_zoom_out_info(zoom_out_on_start);
        minimap_start_fullscreen_map(tip, zoom_out_on_start, false);
    }
    if (Fullscreen_map_tool_tip_name_crc != checksum_stri::INVALID && m_map_mode == MAP_MODE_FULL)
    {
        ui_tool_tips_add_tip(Fullscreen_map_tool_tip_name_crc, 0, nullptr);
        Fullscreen_map_tool_tip_name_crc = checksum_stri::INVALID;
    }
    if (Show_minimap && Usability_hud_enable)
    {
        if (m_map_mode == MAP_MODE_MINI)
        {
            collect_humans();
            collect_bftp_nodes();
            collect_vehicles();
            collect_projectiles();
        }
        collect_restricted_areas();
        collect_convoys();
        m_gps_timer = m_gps_timer + Frametime_real;
        if (Minimap_gps_delay <= m_gps_timer)
        {
            m_gps_timer = 0.0f;
            update_gps();
            if (m_map_mode == MAP_MODE_MINI && m_valid_gps_path)
            {
                create_path_route_points(m_gps_path, false, 1.0f);
            }
            process_patrol_routes();
        }
        if (m_map_mode == MAP_MODE_FULL && m_valid_gps_path)
        {
            process_patrol_routes();
            create_path_route_points(m_gps_path, true, (1.0f - fsm_zoom_percent()) * Fsm_num_path_points_scalar + 1.0f);
        }
        collect_structures();
    }
}

// 0x0084CD30 (:1994)
bool cartographer::point_on_map(const vector& point, float radius_scalar)
{
    const float dx = m_current_size_info.center_pos_world.x - point.x;
    const float dy = m_current_size_info.center_pos_world.y - point.y;
    const float dz = m_current_size_info.center_pos_world.z - point.z;
    return dz * dz + dy * dy + dx * dx <= m_current_size_info.zoom_radius_squared * radius_scalar;
}

// 0x0086CBA0 (:2001)
vector cartographer::get_player_position()
{
    vector pos(Rfg_camera.real_pos);
    player* pp = get_target_player();
    if (pp != nullptr)
    {
        // Original quirk kept: the vehicle's position is read and overwritten by the player's.
        pos = pp->pos;
    }
    return pos;
}

// 0x0084CDF0 (:2025)
vector cartographer::get_center_pos_world()
{
    return m_current_size_info.center_pos_world;
}

// 0x0084CE20 (:2031)
matrix cartographer::get_camera_orientation(bool is_full)
{
    return Rfg_camera.real_orient;
}

// 0x0086CC20 (:2067)
matrix cartographer::get_player_orientation(bool is_full)
{
    matrix orient(Rfg_camera.real_orient);
    player* pp = get_target_player();
    if (pp != nullptr)
    {
        if (human_in_vehicle(pp) != nullptr)
        {
            orient = World.object_from_handle(pp->vehicle_handle)->orient;
        }
        else
        {
            orient = pp->orient;
        }
    }
    return orient;
}

// 0x0084CE40 (:2101)
void cartographer::set_target(unsigned int in_target_handle)
{
    m_target_handle = in_target_handle;
}

// 0x0085BD50 (:2107)
player* cartographer::get_target_player() const
{
    if (m_target_handle != 0xFFFFFFFF)
    {
        object* objp = World.object_from_handle(m_target_handle);
        if (objp != nullptr && objp->get_obj_type() == OT_HUMAN && objp->get_sub_type() == OT_SUB_HUMAN_PLAYER)
        {
            return (player*)objp;
        }
    }
    if (Multiplayer && !multi_is_dedicated_server() && !multi_game_spectating())
    {
        // A spectated player who left: on to the next one.
        control_spectator_next_viewed_player(true);
        if (m_target_handle != 0xFFFFFFFF)
        {
            object* objp = World.object_from_handle(m_target_handle);
            if (objp != nullptr && objp->get_obj_type() == OT_HUMAN && objp->get_sub_type() == OT_SUB_HUMAN_PLAYER)
            {
                return (player*)objp;
            }
        }
    }
    return player_get_local_player();
}

// 0x0085BDD0 (:2128)
void cartographer::set_map_mode(map_mode_type type)
{
    if (type != MAP_MODE_MINI && type != MAP_MODE_FULL)
    {
        return;
    }
    m_map_mode = type;
    if (type == MAP_MODE_MINI)
    {
        m_temp_icons.clear();
        Found_icon = false;
        Most_recent_icon = nullptr;
        m_collected_structures_timer[0].set(0);
    }
    Will_switch_to_full = false;
}

// 0x0084CE50 (:2147)
void cartographer::set_current_map_size_info(const map_size_info& info)
{
    m_current_size_info.center_pos_world = info.center_pos_world;
    m_current_size_info.map_end_posn = info.map_end_posn;
    m_current_size_info.map_posn = info.map_posn;
    m_current_size_info.map_scale = info.map_scale;
    m_current_size_info.map_size = info.map_size;
    m_current_size_info.zoom_radius = info.zoom_radius;
    m_current_size_info.zoom_radius_squared = info.zoom_radius * info.zoom_radius;
}

// 0x0084CEE0 (:2161)
void cartographer::set_full_map_size_info(const map_size_info& info)
{
    if (m_map_mode != MAP_MODE_FULL)
    {
        return;
    }
    m_full_size_info.center_pos_world = info.center_pos_world;
    m_full_size_info.map_end_posn = info.map_end_posn;
    m_full_size_info.map_posn = info.map_posn;
    m_full_size_info.map_scale = info.map_scale;
    m_full_size_info.map_size = info.map_size;
    m_full_size_info.zoom_radius = info.zoom_radius;
    m_full_size_info.zoom_radius_squared = info.zoom_radius * info.zoom_radius;
}

// The GPS route's arrows, the same code in render_full and render_mini.
void cartographer::render_gps_route(float icon_scale, bool is_full)
{
    farray<vector,384> screen_pts;
    get_path_screen_pts(screen_pts, m_gps_path, is_full, false);
    const int last_line_index = m_gps_path.last_line_index;
    if (!m_valid_gps_path || (is_full && !fsm_is_up()))
    {
        return;
    }
    map_image_info mii;
    bmp_uv_information bui;
    get_image_info(mii, bui, get_minimap_bitmap_id(MAP_ELEMENT_DIRECTIONAL_ARROW));
    const int num_pts = screen_pts.size();
    const int trail_index = (int)m_gps_index;
    for (int i = 0; i < num_pts; i++)
    {
        const vector& pt = screen_pts[i];
        color col(GPS_color);
        // Magic number: 45 pixels off the map still draw.
        if (m_current_size_info.map_posn.x > pt.x + 45.0f || m_current_size_info.map_posn.y > pt.y + 45.0f || pt.x - 45.0f > m_current_size_info.map_end_posn.x || pt.y - 45.0f > m_current_size_info.map_end_posn.y || i == 0)
        {
            continue;
        }
        int next = num_pts - 1;
        if (i == next)
        {
            gr_set_color(col);
            gr_set_texture(mii.bitmap_handle);
            continue;
        }
        if (i + Gps_lookahead_points < next)
        {
            next = i + Gps_lookahead_points;
        }
        const vector2 start(pt.x, pt.y);
        const vector2 end(screen_pts[next].x, screen_pts[next].y);
        const float angle = get_directional_arrow_angle(start, end);
        // The moving highlight: trail_size arrows of each section fade to the highlight color.
        const int section = ((int)pt.z + trail_index) % Distance_between_sections;
        if (Distance_between_sections - trail_size - 1 < section)
        {
            const float t = (float)(section - Distance_between_sections + 1 + trail_size) / (float)trail_size;
            col.red = (unsigned char)(int)(((float)GPS_highlight_color.red - (float)GPS_color.red) * t + (float)GPS_color.red);
            col.green = (unsigned char)(int)(((float)GPS_highlight_color.green - (float)col.green) * t + (float)col.green);
            col.blue = (unsigned char)(int)(((float)GPS_highlight_color.blue - (float)col.blue) * t + (float)col.blue);
            col.alpha = 0xFF;
        }
        if ((float)last_line_index <= pt.z)
        {
            col.alpha = (unsigned char)GPS_end_line_alpha;
        }
        if (is_full || (Minimap_gps_display & 1) != 0)
        {
            gr_set_color(col);
            gr_set_texture(mii.bitmap_handle);
            // Magic numbers: the arrow's box, 10 by 10 UI pixels ahead of its point.
            const vector bmax(icon_scale * 9.0f * Resolution_minimap_scale.y, icon_scale * 0.0f * Resolution_minimap_scale.y, icon_scale * 5.0f * Resolution_minimap_scale.y);
            const vector bmin(icon_scale * -1.0f * Resolution_minimap_scale.x, icon_scale * 0.0f * Resolution_minimap_scale.x, icon_scale * -5.0f * Resolution_minimap_scale.x);
            render_bitmap_direct_oriented(&bui, start, angle, bmin, bmax, &mii, col.alpha);
        }
    }
}

// 0x00881A10 (:2189)
void cartographer::render_full()
{
    const float icon_scale = (1.0f - (1.0f - fsm_zoom_percent()) * (1.0f - Fsm_min_icon_scale)) * Fsm_icon_scale;
    gr_state state(GM_FILTER);
    minimap_gr_convert_to_rl_state(m_rl_render_state, state);
    gr_set_scissor_region((int)m_full_size_info.map_posn.x, (int)m_full_size_info.map_posn.y, (int)m_full_size_info.map_end_posn.x, (int)m_full_size_info.map_end_posn.y, 0);
    if (m_gps_path.pts.size() > 0 && m_has_player_marker)
    {
        render_gps_route(icon_scale, true);
    }
    render_district_liberated_icons(icon_scale);
    render_structures_full(icon_scale);
    determine_render_positions(icon_scale, true);
    render_bldg_icons(icon_scale, true);
    render_decals(true);
    render_player_marker(icon_scale, true);
    render_icons(icon_scale, true);
    render_convoy_routes(icon_scale, true);
    render_patrol_routes(icon_scale, true);
    render_sticky_icons(icon_scale, true);
    render_temp_icons(icon_scale, true);
    render_player(true);
    const vector2i& screen = ui_get_screen_dimensions();
    gr_set_scissor_region(0, 0, screen.x, screen.y, 0);
}

// 0x00882130 (:2284)
void cartographer::render_mini()
{
    gr_set_color(0, 0, 0, 0xFF);
    const float icon_scale = (1.0f - (1.0f - (1.0f - (m_current_min_zoom - Minimap_min_radius) / (Minimap_max_radius - Minimap_min_radius))) * (1.0f - Minimap_min_icon_scale)) * Mini_icon_scale;
    render_alpha_mask();
    gr_state state(GM_FILTER);
    state.set_stencil_mode(STENCIL_TEST);
    Minimap_scissor_enabled = true;
    minimap_gr_convert_to_rl_state(m_rl_render_state, state);
    // Magic number: 10 pixels of the map's frame stay in the scissor.
    gr_set_scissor_region((int)(m_mini_size_info.map_posn.x - 10.0f), (int)(m_mini_size_info.map_posn.y - 10.0f), (int)(m_mini_size_info.map_end_posn.x + 10.0f), (int)(m_mini_size_info.map_end_posn.y + 10.0f), 0);
    gr_set_color(0xFF, 0xFF, 0xFF, 0xFF);
    render_terrain();
    render_structures(icon_scale);
    determine_render_positions(icon_scale, false);
    render_convoy_routes(icon_scale, false);
    render_patrol_routes(icon_scale, false);
    if (m_gps_path.pts.size() > 0 && m_has_player_marker)
    {
        render_gps_route(icon_scale, false);
    }
    render_vehicles(icon_scale);
    render_blast_from_the_past_tags(icon_scale);
    render_humans(icon_scale);
    render_guerrilla_converts(icon_scale);
    if (mission_is_active() || !activity_none())
    {
        render_bldg_icons(icon_scale, false);
    }
    render_icons(icon_scale, false);
    render_projectiles(icon_scale);
    render_scan_line();
    render_static_zones();
    if (Minimap_render_restricted)
    {
        render_restricted_areas();
    }
    render_decals(false);
    state = GM_FILTER;
    Minimap_scissor_enabled = true;
    minimap_gr_convert_to_rl_state(m_rl_render_state, state);
    render_overlay_map();
    render_player_marker(icon_scale, false);
    render_north();
    if (!mission_is_active() && activity_none())
    {
        render_bldg_icons(icon_scale, false);
    }
    render_sticky_icons(icon_scale, false);
    render_temp_icons(icon_scale, false);
    render_structures_post();
    render_player(false);
    const vector2i& screen = ui_get_screen_dimensions();
    gr_set_scissor_region(0, 0, screen.x, screen.y, 0);
    Minimap_scissor_enabled = false;
    m_rl_render_state.set_scissor_enabled(false);
    rl_draw::get_instance()->get_rl_state().set_macro_state(m_rl_render_state);
}

// 0x0084CF80 (:2441)
void cartographer::get_image_info(map_image_info& mii, bmp_uv_information& bui, int bmp_lookup)
{
    mii.image_data = vint_bms_get_image_data(bmp_lookup);
    mii.bitmap_handle = vint_bms_get_bitmap(bmp_lookup);
    get_uv_information_bmpsheet(mii.image_data, mii.bitmap_handle, bui.uvs[0], bui.uvs[1], bui.uvs[2], bui.uvs[3]);
}

// 0x0085BE20 (:2454)
void cartographer::render_bitmap_direct(bmp_uv_information* bui, const vector2& screen_pos, const vector& bmin, const vector& bmax, const map_image_info* mii, unsigned char alpha)
{
    if (bui == nullptr)
    {
        return;
    }
    if (mii != nullptr)
    {
        gr_set_texture(mii->bitmap_handle);
        rl_draw::get_instance()->tristrip_2d_begin(m_rl_render_state);
    }
    rl_primitive_2d_vert* verts = rl_draw::get_instance()->tristrip_2d_allocate_tris(2);
    if (verts != nullptr)
    {
        const rl_color_float& clr = rl_draw::get_instance()->get_rl_state().get_primitive_color();
        const vector2 corners[4] = {vector2(bmin.x, bmax.z), vector2(bmax.x, bmax.z), vector2(bmin.x, bmin.z), vector2(bmax.x, bmin.z)};
        for (int i = 0; i < 4; i++)
        {
            verts[i].color.green = (unsigned char)(int)(clr.green * 255.0f + 0.5f);
            verts[i].color.blue = (unsigned char)(int)(clr.blue * 255.0f + 0.5f);
            verts[i].color.red = (unsigned char)(int)(clr.red * 255.0f + 0.5f);
            verts[i].color.alpha = alpha;
            verts[i].uvs.x = bui->uvs[i].x;
            verts[i].uvs.y = bui->uvs[i].y;
            verts[i].pt.x = corners[i].x + screen_pos.x;
            verts[i].pt.y = corners[i].y + screen_pos.y;
        }
    }
    if (mii != nullptr)
    {
        rl_draw::get_instance()->tristrip_2d_end();
    }
}

// 0x0085C0C0 (:2527)
void cartographer::render_bitmap_direct_oriented(bmp_uv_information* bui, const vector2& screen_pos, float rotation_rad, const vector& bmin, const vector& bmax, const map_image_info* mii, unsigned char alpha)
{
    if (bui == nullptr)
    {
        return;
    }
    if (mii != nullptr)
    {
        gr_set_texture(mii->bitmap_handle);
        rl_draw::get_instance()->tristrip_2d_begin(m_rl_render_state);
    }
    rl_primitive_2d_vert* verts = rl_draw::get_instance()->tristrip_2d_allocate_tris(2);
    if (verts != nullptr)
    {
        const rl_color_float& clr = rl_draw::get_instance()->get_rl_state().get_primitive_color();
        vector corners[4] = {vector(bmin.x, 0.0f, bmax.z), vector(bmax.x, 0.0f, bmax.z), vector(bmin.x, 0.0f, bmin.z), vector(bmax.x, 0.0f, bmin.z)};
        matrix rotation;
        rotation.make_identity();
        rotation.rotate_around_local_y(rotation_rad);
        for (int i = 0; i < 4; i++)
        {
            corners[i].rotate(rotation);
            verts[i].color.green = (unsigned char)(int)(clr.green * 255.0f + 0.5f);
            verts[i].color.blue = (unsigned char)(int)(clr.blue * 255.0f + 0.5f);
            verts[i].color.red = (unsigned char)(int)(clr.red * 255.0f + 0.5f);
            verts[i].color.alpha = alpha;
            verts[i].uvs.x = bui->uvs[i].x;
            verts[i].uvs.y = bui->uvs[i].y;
            verts[i].pt.x = screen_pos.x + corners[i].x;
            verts[i].pt.y = screen_pos.y + corners[i].z;
        }
    }
    if (mii != nullptr)
    {
        rl_draw::get_instance()->tristrip_2d_end();
    }
}

// The world position on the map's window. Inlined at every use in rfg.exe.
vector2 cartographer::world_to_window(float x, float z) const
{
    const float dx = x - m_map_corner_posns[0].x;
    const float dz = z - m_map_corner_posns[0].z;
    return vector2(m_current_size_info.map_scale.x * (m_ne_to_nw_vector.x * dx + m_ne_to_nw_vector.z * dz) + m_current_size_info.map_posn.x, m_current_size_info.map_scale.y * (m_sw_to_nw_vector.x * dx + m_sw_to_nw_vector.z * dz) + m_current_size_info.map_posn.y);
}

// 0x0085C400 (:2606)
void cartographer::render_bitmap(bmp_uv_information* bui, const vector& world_pos, const vector& bmin, const vector& bmax, const map_image_info* mii)
{
    if (bui == nullptr)
    {
        return;
    }
    if (mii != nullptr)
    {
        gr_set_texture(mii->bitmap_handle);
        rl_draw::get_instance()->tristrip_2d_begin(m_rl_render_state);
    }
    rl_primitive_2d_vert* verts = rl_draw::get_instance()->tristrip_2d_allocate_tris(2);
    if (verts != nullptr)
    {
        const rl_color_float& clr = rl_draw::get_instance()->get_rl_state().get_primitive_color();
        const vector2 center = world_to_window(world_pos.x, world_pos.z);
        const vector2 corners[4] = {vector2(bmin.x, bmax.z), vector2(bmax.x, bmax.z), vector2(bmin.x, bmin.z), vector2(bmax.x, bmin.z)};
        for (int i = 0; i < 4; i++)
        {
            verts[i].color.green = (unsigned char)(int)(clr.green * 255.0f + 0.5f);
            verts[i].color.blue = (unsigned char)(int)(clr.blue * 255.0f + 0.5f);
            verts[i].color.alpha = (unsigned char)(int)(clr.alpha * 255.0f + 0.5f);
            verts[i].color.red = (unsigned char)(int)(clr.red * 255.0f + 0.5f);
            verts[i].uvs.x = bui->uvs[i].x;
            verts[i].uvs.y = bui->uvs[i].y;
            verts[i].pt.x = center.x + corners[i].x;
            verts[i].pt.y = center.y + corners[i].y;
        }
    }
    if (mii != nullptr)
    {
        rl_draw::get_instance()->tristrip_2d_end();
    }
}

// The bitmap's uvs turned in place, inlined in both render_bitmap_oriented. Original quirk kept: bui is the caller's, so
// a shared cached_bmp_info stays turned.
static void rotate_uvs(bmp_uv_information* bui, rot_uvs uv_flag)
{
    const bmp_uv_information old = *bui;
    switch (uv_flag)
    {
        case RUVS_90:
            bui->uvs[0] = old.uvs[1];
            bui->uvs[1] = old.uvs[3];
            bui->uvs[2] = old.uvs[0];
            bui->uvs[3] = old.uvs[2];
            break;
        case RUVS_180:
            bui->uvs[0] = old.uvs[3];
            bui->uvs[1] = old.uvs[2];
            bui->uvs[2] = old.uvs[1];
            bui->uvs[3] = old.uvs[0];
            break;
        case RUVS_270:
            bui->uvs[0] = old.uvs[2];
            bui->uvs[1] = old.uvs[0];
            bui->uvs[2] = old.uvs[3];
            bui->uvs[3] = old.uvs[1];
            break;
        case RUVS_FLIP_LR:
            bui->uvs[0] = old.uvs[1];
            bui->uvs[1] = old.uvs[0];
            bui->uvs[2] = old.uvs[3];
            bui->uvs[3] = old.uvs[2];
            break;
        case RUVS_FLIP_UD:
            bui->uvs[0] = old.uvs[2];
            bui->uvs[1] = old.uvs[3];
            bui->uvs[2] = old.uvs[0];
            bui->uvs[3] = old.uvs[1];
            break;
        default:
            break;
    }
}

// The four window positions with the primitive color and bui's uvs.
static void fill_map_quad(rl_primitive_2d_vert* verts, const vector2* window, const bmp_uv_information* bui)
{
    const rl_color_float& clr = rl_draw::get_instance()->get_rl_state().get_primitive_color();
    for (int i = 0; i < 4; i++)
    {
        verts[i].color.green = (unsigned char)(int)(clr.green * 255.0f + 0.5f);
        verts[i].color.blue = (unsigned char)(int)(clr.blue * 255.0f + 0.5f);
        verts[i].color.alpha = (unsigned char)(int)(clr.alpha * 255.0f + 0.5f);
        verts[i].color.red = (unsigned char)(int)(clr.red * 255.0f + 0.5f);
        if (bui != nullptr)
        {
            verts[i].uvs.x = bui->uvs[i].x;
            verts[i].uvs.y = bui->uvs[i].y;
        }
        verts[i].pt.x = window[i].x;
        verts[i].pt.y = window[i].y;
    }
}

// 0x0085C7B0 (:2680)
// bmin and bmax are world positions here, the map's own axes.
void cartographer::render_bitmap_oriented(bmp_uv_information* bui, const vector& bmin, const vector& bmax, const map_image_info* mii, rot_uvs uv_flag, bool ignore_scale)
{
    if (bui == nullptr)
    {
        return;
    }
    if (mii != nullptr)
    {
        gr_set_texture(mii->bitmap_handle);
        rl_draw::get_instance()->tristrip_2d_begin(m_rl_render_state);
    }
    rl_primitive_2d_vert* verts = rl_draw::get_instance()->tristrip_2d_allocate_tris(2);
    if (verts != nullptr)
    {
        float min_x = bmin.x;
        float min_z = bmin.z;
        float max_x = bmax.x;
        float max_z = bmax.z;
        if (ignore_scale)
        {
            max_x = max_x / m_current_size_info.map_scale.x;
            min_x = min_x / m_current_size_info.map_scale.x;
            min_z = min_z / m_current_size_info.map_scale.y;
            max_z = max_z / m_current_size_info.map_scale.y;
        }
        const vector2 window[4] = {world_to_window(min_x, min_z), world_to_window(max_x, min_z), world_to_window(min_x, max_z), world_to_window(max_x, max_z)};
        rotate_uvs(bui, uv_flag);
        fill_map_quad(verts, window, bui);
    }
    if (mii != nullptr)
    {
        rl_draw::get_instance()->tristrip_2d_end();
    }
}

// 0x0085CEA0 (:2818)
void cartographer::render_bitmap_oriented(bmp_uv_information* bui, const vector& world_pos, const matrix& mat, const vector& bmin, const vector& bmax, const map_image_info* mii, rot_uvs uv_flag, bool ignore_scale)
{
    if (bui == nullptr)
    {
        return;
    }
    if (mii != nullptr)
    {
        gr_set_texture(mii->bitmap_handle);
        rl_draw::get_instance()->tristrip_2d_begin(m_rl_render_state);
    }
    rl_primitive_2d_vert* verts = rl_draw::get_instance()->tristrip_2d_allocate_tris(2);
    if (verts != nullptr)
    {
        float min_x = bmin.x;
        float min_z = bmin.z;
        float max_x = bmax.x;
        float max_z = bmax.z;
        if (ignore_scale)
        {
            min_x = min_x / m_current_size_info.map_scale.x;
            min_z = min_z / m_current_size_info.map_scale.y;
            max_x = max_x / m_current_size_info.map_scale.x;
            max_z = max_z / m_current_size_info.map_scale.y;
        }
        const float nz = -mat.rvec.z;
        const float rx = mat.rvec.x;
        const vector2 window[4] =
        {
            world_to_window(world_pos.x + rx * min_x + nz * max_z, (rx * max_z - nz * min_x) + world_pos.z),
            world_to_window(world_pos.x + rx * max_x + nz * max_z, (rx * max_z - nz * max_x) + world_pos.z),
            world_to_window(world_pos.x + nz * min_z + rx * min_x, (rx * min_z - nz * min_x) + world_pos.z),
            world_to_window(world_pos.x + nz * min_z + rx * max_x, (rx * min_z - nz * max_x) + world_pos.z),
        };
        rotate_uvs(bui, uv_flag);
        fill_map_quad(verts, window, bui);
    }
    if (mii != nullptr)
    {
        rl_draw::get_instance()->tristrip_2d_end();
    }
}

// The uvs that tile a source of src_w by src_h across the quad's lengths on screen.
static void fill_tiled_map_quad(rl_primitive_2d_vert* verts, const vector2* window, unsigned int src_w, unsigned int src_h)
{
    const float dx0 = window[1].x - window[0].x;
    const float dy0 = window[1].y - window[0].y;
    const float u = (float)sqrt((double)(dy0 * dy0 + dx0 * dx0)) / (float)src_w;
    const float dx1 = window[2].x - window[0].x;
    const float dy1 = window[2].y - window[0].y;
    const float v = (float)sqrt((double)(dy1 * dy1 + dx1 * dx1)) / (float)src_h;
    verts[0].uvs.x = 0.0f;
    verts[0].uvs.y = 0.0f;
    verts[1].uvs.y = 0.0f;
    verts[1].uvs.x = u;
    verts[2].uvs.x = 0.0f;
    verts[2].uvs.y = v;
    verts[3].uvs.y = v;
    verts[3].uvs.x = u;
    fill_map_quad(verts, window, nullptr);
}

// 0x0085D7E0 (:2959)
// Original quirk kept: on a failed allocation the strip is ended here, the caller's begin is its own.
void cartographer::render_tiled_bitmap_oriented(const vector& bmin, const vector& bmax, unsigned int src_w, unsigned int src_h)
{
    rl_primitive_2d_vert* verts = rl_draw::get_instance()->tristrip_2d_allocate_tris(2);
    if (verts == nullptr)
    {
        rl_draw::get_instance()->tristrip_2d_end();
        return;
    }
    const vector2 window[4] = {world_to_window(bmin.x, bmin.z), world_to_window(bmax.x, bmin.z), world_to_window(bmin.x, bmax.z), world_to_window(bmax.x, bmax.z)};
    fill_tiled_map_quad(verts, window, src_w, src_h);
}

// 0x0085DE90 (:3014)
void cartographer::render_tiled_bitmap_oriented(const vector& world_pos, const matrix& orient, const vector& bmin, const vector& bmax, unsigned int src_w, unsigned int src_h)
{
    rl_primitive_2d_vert* verts = rl_draw::get_instance()->tristrip_2d_allocate_tris(2);
    if (verts == nullptr)
    {
        rl_draw::get_instance()->tristrip_2d_end();
        return;
    }
    const float nz = -orient.rvec.z;
    const float rx = orient.rvec.x;
    const vector2 window[4] =
    {
        world_to_window(rx * bmin.x + bmax.z * nz + world_pos.x, (bmax.z * rx - nz * bmin.x) + world_pos.z),
        world_to_window(rx * bmax.x + bmax.z * nz + world_pos.x, (bmax.z * rx - nz * bmax.x) + world_pos.z),
        world_to_window(bmin.z * nz + rx * bmin.x + world_pos.x, (bmin.z * rx - nz * bmin.x) + world_pos.z),
        world_to_window(bmin.z * nz + rx * bmax.x + world_pos.x, (bmin.z * rx - nz * bmax.x) + world_pos.z),
    };
    fill_tiled_map_quad(verts, window, src_w, src_h);
}

// 0x0085E700 (:3156)
void cartographer::render_bitmap_via_box(bmp_uv_information* bui, const vector& world_pos, const vector2& top_left, const vector2& top_right, const vector2& bottom_left, const vector2& bottom_right, const map_image_info* mii)
{
    if (bui == nullptr)
    {
        return;
    }
    if (mii != nullptr)
    {
        gr_set_texture(mii->bitmap_handle);
        rl_draw::get_instance()->tristrip_2d_begin(m_rl_render_state);
    }
    rl_primitive_2d_vert* verts = rl_draw::get_instance()->tristrip_2d_allocate_tris(2);
    if (verts != nullptr)
    {
        const vector2 window[4] =
        {
            world_to_window(world_pos.x + top_left.x, world_pos.z + top_left.y),
            world_to_window(world_pos.x + top_right.x, world_pos.z + top_right.y),
            world_to_window(world_pos.x + bottom_left.x, world_pos.z + bottom_left.y),
            world_to_window(world_pos.x + bottom_right.x, world_pos.z + bottom_right.y),
        };
        fill_map_quad(verts, window, bui);
    }
    if (mii != nullptr)
    {
        rl_draw::get_instance()->tristrip_2d_end();
    }
}

// 0x0085ED60 (:3217)
void cartographer::render_bitmap_alt(int bitmap_id, float x, float y, float angle, float scalew, float scaleh, const gr_state& state)
{
    const bm_image* image = vint_bms_get_image_data(bitmap_id);
    const int bitmap_handle = vint_bms_get_bitmap(bitmap_id);
    if (image == nullptr)
    {
        return;
    }
    const float width = (float)image->bmw;
    const float height = (float)image->bmh;
    const float sine = (float)sin((double)angle);
    const float cosine = (float)cos((double)angle);
    float xform[6];
    xform[0] = cosine * width * scalew;
    xform[1] = -(sine * height * scaleh);
    xform[3] = sine * width * scalew;
    xform[4] = cosine * height * scaleh;
    // The rotated corner: half of both vectors back from the center.
    xform[2] = xform[1] * -0.5f + xform[0] * -0.5f;
    xform[5] = xform[4] * -0.5f + xform[3] * -0.5f;
    const vector2 pos((float)image->sx, (float)image->sy);
    const vector2 screen(x, y);
    const unsigned int source_size[2] = {image->bmw, image->bmh};
    minimap_gr_convert_to_rl_state(m_rl_render_state, state);
    map_render_draw_bitmap_internal(bitmap_handle, xform, screen, pos, source_size, m_rl_render_state);
}

// 0x0085EF30 (:3235)
void cartographer::render_bitmap_alt_3d(int bitmap_id, const vector& pt, const matrix& orient, float radius, const gr_state& state)
{
    const bm_image* image = vint_bms_get_image_data(bitmap_id);
    const int bitmap_handle = vint_bms_get_bitmap(bitmap_id);
    if (image == nullptr)
    {
        return;
    }
    gr_set_texture(bitmap_handle);
    const unsigned int width = image->bmw;
    const unsigned int height = image->bmh;
    // radius is the short side's half size, the long side keeps the image's ratio.
    float half_w = radius;
    float half_h = radius;
    if (width < height)
    {
        half_h = ((float)height * radius) / (float)width;
    }
    else if (height < width)
    {
        half_w = ((float)width * radius) / (float)height;
    }
    rl_color_float clr;
    rl_draw::get_instance()->get_color(clr);
    const unsigned char green = (unsigned char)(int)(clr.green * 255.0f);
    const unsigned char blue = (unsigned char)(int)(clr.blue * 255.0f);
    const unsigned char alpha = (unsigned char)(int)(clr.alpha * 255.0f);
    const unsigned char red = (unsigned char)(int)(clr.red * 255.0f);
    int w, h;
    float scale_x, scale_y;
    bm_get_dimensions(bitmap_handle, w, h, scale_x, scale_y);
    const float u_scale = (1.0f / (float)w) / scale_x;
    const float v_scale = (1.0f / (float)h) / scale_y;
    const float u0 = (float)(int)(float)image->sx * u_scale;
    const float v0 = (float)(int)(float)image->sy * v_scale;
    const float u1 = (float)(int)width * u_scale + u0;
    const float v1 = (float)(int)height * v_scale + v0;
    minimap_gr_convert_to_rl_state(m_rl_render_state, state);
    rl_draw::get_instance()->tristrip_3d_begin(m_rl_render_state);
    rl_primitive_3d_vert* verts = rl_draw::get_instance()->tristrip_3d_allocate_tris(2);
    const float sign_r[4] = {-half_w, half_w, -half_w, half_w};
    const float sign_u[4] = {half_h, half_h, -half_h, -half_h};
    const float us[4] = {u0, u1, u0, u1};
    const float vs[4] = {v0, v0, v1, v1};
    for (int i = 0; i < 4; i++)
    {
        verts[i].pt.x = pt.x + orient.rvec.x * sign_r[i] + orient.uvec.x * sign_u[i];
        verts[i].pt.y = orient.rvec.y * sign_r[i] + pt.y + orient.uvec.y * sign_u[i];
        verts[i].pt.z = pt.z + sign_r[i] * orient.rvec.z + orient.uvec.z * sign_u[i];
        verts[i].uvs.x = us[i];
        verts[i].uvs.y = vs[i];
        verts[i].color.green = green;
        verts[i].color.blue = blue;
        verts[i].color.red = red;
        verts[i].color.alpha = alpha;
    }
    rl_draw::get_instance()->tristrip_3d_end();
}

// 0x0084CFC0 (:3389)
void cartographer::render_control_morale()
{
}

// 0x0086CCD0 (:3394)
// The average of the last 64 speeds; each call also grows the zoom (half the average, per call).
float cartographer::get_avg_vehicle_speed()
{
    player* pp = get_target_player();
    if (pp != nullptr && human_in_vehicle(pp) != nullptr)
    {
        vehicle* vp = (vehicle*)World.object_from_handle(pp->vehicle_handle);
        if (vp->rdd.is_loaded())
        {
            float speed = 0.0f;
            vehicle_get_speed(vp, speed);
            speed = fabsf(speed);
            int count = m_vehicle_speed.size();
            if (count == 0x40)
            {
                m_vehicle_sums = m_vehicle_sums - m_vehicle_speed[m_vehicle_posn];
                m_vehicle_speed[m_vehicle_posn] = speed;
            }
            else
            {
                m_vehicle_speed.add(speed);
                count = count + 1;
            }
            m_vehicle_sums = m_vehicle_sums + speed;
            const float average = m_vehicle_sums / (float)count;
            m_vehicle_posn = (m_vehicle_posn + 1) % 0x40;
            m_mini_size_info.zoom_radius = average * 0.5f + m_mini_size_info.zoom_radius;
            return average;
        }
    }
    m_vehicle_speed.clear();
    m_vehicle_posn = 0;
    m_vehicle_sums = 0.0f;
    return 0.0f;
}

// 0x0086CE50 (:3439)
void cartographer::determine_nearest_safehouse()
{
    m_nearest_safehouse = 0;
    m_num_safehouse = 0;
    vector pos(Rfg_camera.real_pos);
    player* pp = get_target_player();
    if (pp != nullptr)
    {
        pos = pp->pos;
    }
    float best = 3.4028235e+38f; // FLT_MAX
    for (int i = 0; i < m_sticky_icons.size(); i++)
    {
        const map_icon& icon = m_sticky_icons[i];
        if (icon.bitmap != MAP_ELEMENT_SAFE_HOUSE || icon.handle == 0xFFFFFFFF)
        {
            continue;
        }
        object_safehouse* sp = (object_safehouse*)World.object_from_handle(icon.handle);
        if (sp == nullptr || sp->get_obj_type() != OT_SAFEHOUSE || !sp->is_visible())
        {
            continue;
        }
        m_num_safehouse++;
        const float dx = pos.x - sp->pos.x;
        const float dy = pos.y - sp->pos.y;
        const float dz = pos.z - sp->pos.z;
        const float dist = dz * dz + dy * dy + dx * dx;
        if (dist < best)
        {
            m_nearest_safehouse = icon.handle;
            best = dist;
        }
    }
}

// 0x0085F7A0 (:3476)
void cartographer::render_terrain()
{
    rl_primitive_state state;
    gr_convert_to_rl_state(state, GM_STENCIL_TEST);
    state.set_scissor_enabled(Minimap_scissor_enabled);
    state.set_clamp_mode(Rl_clamp_minimap ? (rl_prim_clamp_mode)1 : (rl_prim_clamp_mode)0);
    for (int i = 0; i < m_counts[0]; i++)
    {
        const object_info_terrain_bbox& terrain = m_terrain[i];
        if (terrain.mat_handle == 0xFFFFFFFF || Minimap_render_old_mode)
        {
            rl_draw::get_instance()->set_texture(terrain.bm_handle);
        }
        else
        {
            rl_draw::get_instance()->set_material(terrain.mat_handle);
        }
        rl_draw::get_instance()->tristrip_2d_begin(state);
        rl_primitive_2d_vert* verts = rl_draw::get_instance()->tristrip_2d_allocate_tris(2);
        if (verts != nullptr)
        {
            for (int v = 0; v < 4; v++)
            {
                const vector2 window = world_to_window(terrain.corners[v].x, terrain.corners[v].y);
                verts[v].color.red = 0xFF;
                verts[v].color.green = 0xFF;
                verts[v].color.blue = 0xFF;
                verts[v].color.alpha = 0xFF;
                verts[v].pt.x = window.x;
                verts[v].pt.y = window.y;
            }
            verts[0].uvs.x = 0.0f;
            verts[0].uvs.y = 1.0f;
            verts[1].uvs.x = 1.0f;
            verts[1].uvs.y = 1.0f;
            verts[2].uvs.x = 0.0f;
            verts[2].uvs.y = 0.0f;
            verts[3].uvs.x = 1.0f;
            verts[3].uvs.y = 0.0f;
        }
        rl_draw::get_instance()->tristrip_2d_end();
    }
}

// 0x0086D000 (:3557)
void cartographer::collect_projectiles()
{
    m_projectile_list.clear();
    for (int i = 0; i < World.get_num_objects<projectile>() && m_projectile_list.size() < m_projectile_list.capacity(); i++)
    {
        const projectile* pp = World.get_object<projectile>(i);
        const weapon_class_type wclass = pp->w_info->weapon_class;
        if (wclass != WEAPON_CLASS_THROWN_MINE && wclass != WEAPON_CLASS_THROWN_CHARGE && wclass != WEAPON_CLASS_SINGULARITY_BOMB)
        {
            continue;
        }
        const float dx = pp->pos.x - m_current_size_info.center_pos_world.x;
        const float dz = pp->pos.z - m_current_size_info.center_pos_world.z;
        if (dx * dx + dz * dz <= m_current_size_info.zoom_radius_squared)
        {
            m_projectile_list.add(pp);
        }
    }
}

// 0x0086D140 (:3576)
void cartographer::collect_humans()
{
    m_human_list.clear();
    player* target = get_target_player();
    if (Multiplayer)
    {
        for (int i = 0; i < World.get_num_objects<player>(); i++)
        {
            player* pp = World.get_object<player>(i);
            if (!pp->obj_flags._dont_use_me || pp->hflags.hidden || pp->is_killed() || pp == target || human_in_vehicle(pp) != nullptr || !pp->mp_should_render_on_minimap() || !object_is_in_range_radius_squared(pp))
            {
                continue;
            }
            const human* hp = pp;
            m_human_list.add(hp);
            if (m_human_list.size() >= m_human_list.capacity())
            {
                return;
            }
        }
        return;
    }
    for (int i = 0; i < World.get_num_objects<human>(); i++)
    {
        human* hp = World.get_object<human>(i);
        if (!hp->obj_flags._dont_use_me || hp->hflags.hidden || hp->is_killed() || hp == target || human_in_vehicle(hp) != nullptr)
        {
            continue;
        }
        bool add = false;
        if (target == nullptr)
        {
            add = Minimap_render_humans;
        }
        else
        {
            // Magic number: upgrade 0x11 shows all humans on the map.
            const human_team_dispositions disposition = human_teams_get_disposition(target, hp);
            if (disposition == TEAM_DISPOSITION_HOSTILE)
            {
                add = Minimap_render_humans || hp->render_on_minimap() || player_get_meta_data()->get_upgrade_value(0x11) > 0;
            }
            else if (disposition == TEAM_DISPOSITION_ALLIED)
            {
                npc* np = World.object_cast<npc>(hp);
                ai_system* ai = np != nullptr ? np->get_ai() : nullptr;
                if (ai == nullptr || ai->is_in_player_squad(false) != target)
                {
                    // Original bug kept: np is not checked here.
                    add = np->npc_flags.activity_related || np->npc_flags.always_show_on_minimap || gsm_get_alert_level() != ALERT_LEVEL_GREEN;
                }
            }
            else if (disposition == TEAM_DISPOSITION_FRIENDLY)
            {
                npc* np = World.object_cast<npc>(hp);
                if (player_get_meta_data()->get_upgrade_value(0x11) > 0)
                {
                    // Original bug kept: np is not checked here.
                    add = np->hflags.activity_ha_hostage;
                }
            }
            else
            {
                add = Minimap_render_humans;
            }
        }
        if (add)
        {
            const float dx = hp->pos.x - m_current_size_info.center_pos_world.x;
            const float dz = hp->pos.z - m_current_size_info.center_pos_world.z;
            if (dx * dx + dz * dz <= m_current_size_info.zoom_radius_squared)
            {
                const human* added = hp;
                m_human_list.add(added);
                if (m_human_list.capacity() <= m_human_list.size())
                {
                    return;
                }
            }
        }
    }
}

// 0x0086D450 (:3677)
void cartographer::collect_bftp_nodes()
{
    m_bftp_nodes.clear();
    if (Multiplayer)
    {
        return;
    }
    for (int i = 0; i < World.get_num_objects<object_bftp_node>(); i++)
    {
        object_bftp_node* np = World.get_object<object_bftp_node>(i);
        if (np->is_collected() || !np->is_tag_node())
        {
            continue;
        }
        const float dx = np->pos.x - m_current_size_info.center_pos_world.x;
        const float dz = np->pos.z - m_current_size_info.center_pos_world.z;
        if (dx * dx + dz * dz <= m_current_size_info.zoom_radius_squared)
        {
            const object_bftp_node* added = np;
            m_bftp_nodes.add(added);
            if (m_bftp_nodes.capacity() <= m_bftp_nodes.size())
            {
                return;
            }
        }
    }
}

// The local player's vehicle, or null. Inlined in collect_vehicles and render_vehicles.
static vehicle* minimap_local_vehicle()
{
    player* pp = player_get_local_player();
    if (pp == nullptr || human_in_vehicle(pp) == nullptr || pp->vehicle_handle == 0xFFFFFFFF)
    {
        return nullptr;
    }
    object* objp = World.object_from_handle(pp->vehicle_handle);
    return objp != nullptr && objp->get_obj_type() == OT_VEHICLE ? (vehicle*)objp : nullptr;
}

// 0x0086D590 (:3709)
void cartographer::collect_vehicles()
{
    m_vehicle_list.clear();
    const vehicle* own = minimap_local_vehicle();
    for (int i = 0; i < World.get_num_objects<vehicle>(); i++)
    {
        vehicle* vp = World.get_object<vehicle>(i);
        if (vp == nullptr)
        {
            continue;
        }
        const float dx = vp->pos.x - m_current_size_info.center_pos_world.x;
        const float dz = vp->pos.z - m_current_size_info.center_pos_world.z;
        if (dx * dx + dz * dz <= m_current_size_info.zoom_radius_squared && !vp->is_killed() && !vp->m_flags.m_corpse && vp != own)
        {
            const vehicle* added = vp;
            m_vehicle_list.add(added);
            if (m_vehicle_list.capacity() <= m_vehicle_list.size())
            {
                return;
            }
        }
    }
}

// 0x0085FCC0 (:3743)
void cartographer::render_projectiles(float zoom_scale)
{
    const int count = m_projectile_list.size();
    if (count <= 0)
    {
        return;
    }
    const int bitmap_id = get_minimap_bitmap_id(MAP_ELEMENT_BLIP);
    const bm_image* image = vint_bms_get_image_data(bitmap_id);
    const int bitmap_handle = vint_bms_get_bitmap(bitmap_id);
    bmp_uv_information bui;
    get_uv_information_bmpsheet(image, bitmap_handle, bui.uvs[0], bui.uvs[1], bui.uvs[2], bui.uvs[3]);
    // Magic numbers: the half sizes of the blip (2.5) and of the singularity bomb's ring (7.5).
    const vector bmin(-2.5f, 0.0f, -2.5f);
    const vector bmax(2.5f, 0.0f, 2.5f);
    gr_set_texture(bitmap_handle);
    rl_draw::get_instance()->tristrip_2d_begin(m_rl_render_state);
    for (int i = 0; i < count; i++)
    {
        const projectile* pp = m_projectile_list[i];
        // Twice a second, 0 to 1.
        float pulse = (float)pp->age_timer.time_since() * 0.001f * 2.0f;
        pulse = pulse - (float)(int)pulse;
        const float grow = pulse + 1.0f;
        const weapon_class_type wclass = pp->w_info->weapon_class;
        if (wclass == WEAPON_CLASS_THROWN_CHARGE)
        {
            // Dimmed while it flies, blinking once it stuck.
            color col(0x80, 0x80, 0x80, 0x40);
            if (pp->child_constraint_ptr != nullptr || pp->get_parent() != 0xFFFFFFFF)
            {
                col = color(0xFF, 0xFF, 0xFF, (unsigned char)(int)(pulse * 255.0f));
            }
            gr_set_color(col);
            render_bitmap(&bui, pp->pos, bmin, bmax, nullptr);
        }
        else if (wclass == WEAPON_CLASS_THROWN_MINE)
        {
            gr_set_color(color(0xFF, 0x80, 0x00, 0xFF));
            render_bitmap(&bui, pp->pos, bmin, bmax, nullptr);
            const vector ring_min(grow * -2.5f, grow * 0.0f, grow * -2.5f);
            const vector ring_max(grow * 2.5f, grow * 0.0f, grow * 2.5f);
            gr_set_alpha((int)(pulse * 128.0f));
            render_bitmap(&bui, pp->pos, ring_min, ring_max, nullptr);
        }
        else if (wclass == WEAPON_CLASS_SINGULARITY_BOMB)
        {
            gr_set_color(color(0x00, 0x80, 0x80, 0xFF));
            render_bitmap(&bui, pp->pos, bmin, bmax, nullptr);
            const vector ring_min(-7.5f, 0.0f, -7.5f);
            const vector ring_max(7.5f, 0.0f, 7.5f);
            gr_set_alpha((int)(pulse * 128.0f));
            render_bitmap(&bui, pp->pos, ring_min, ring_max, nullptr);
        }
    }
    rl_draw::get_instance()->tristrip_2d_end();
}

// 0x0086D710 (:3809)
void cartographer::render_humans(float zoom_scale)
{
    const float scale = zoom_scale * 1.5f;
    player* target = get_target_player();
    const int count = m_human_list.size();
    if (count <= 0)
    {
        return;
    }
    const int human_id = get_minimap_bitmap_id(MAP_ELEMENT_BLIP_HUMAN);
    const int human_handle = vint_bms_get_bitmap(human_id);
    bmp_uv_information human_bui;
    get_uv_information_bmpsheet(vint_bms_get_image_data(human_id), human_handle, human_bui.uvs[0], human_bui.uvs[1], human_bui.uvs[2], human_bui.uvs[3]);
    const int ring_id = get_minimap_bitmap_id(MAP_ELEMENT_RING_ICON);
    const int ring_handle = vint_bms_get_bitmap(ring_id);
    bmp_uv_information ring_bui;
    get_uv_information_bmpsheet(vint_bms_get_image_data(ring_id), ring_handle, ring_bui.uvs[0], ring_bui.uvs[1], ring_bui.uvs[2], ring_bui.uvs[3]);
    // The rings the radar's line leaves behind.
    gr_set_texture(ring_handle);
    rl_draw::get_instance()->tristrip_2d_begin(m_rl_render_state);
    for (int i = 0; i < count; i++)
    {
        const human* hp = m_human_list[i];
        const human_team_dispositions disposition = target != nullptr ? human_teams_get_disposition(target, hp) : TEAM_DISPOSITION_NONE;
        const color col = get_human_color(hp, disposition);
        const float y = world_to_window(hp->pos.x, hp->pos.z).y;
        float behind = y - m_scan_line_pos;
        const float cycle = (m_mini_size_info.map_end_posn.y - m_mini_size_info.map_posn.y) * 2.0f;
        if (0.0f < behind)
        {
            behind = y - (m_scan_line_pos + cycle);
        }
        const float t = (-behind * Minimap_radar_blip_scale_rate) / cycle;
        float size = Minimap_blip_start_scale + t;
        if (0.0f <= 0.0f - size)
        {
            size = 0.0f;
        }
        const float fade = (1.0f - t) + 1.0f;
        const float opacity = 0.0f - fade < 0.0f ? fade : 0.0f;
        const int blue = col.blue + Blip_color_add;
        const int green = col.green + Blip_color_add;
        const int red = col.red + Blip_color_add;
        const color ring((unsigned char)(red < 0x100 ? (red < 0 ? 0 : red) : 0xFF), (unsigned char)(green < 0x100 ? (green < 0 ? 0 : green) : 0xFF), (unsigned char)(blue < 0x100 ? (blue < 0 ? 0 : blue) : 0xFF), (unsigned char)(int)(Minimap_blip_opacity * opacity));
        gr_set_color(ring);
        // Magic number: 4, the ring's half size.
        const vector bmax(scale * (size * 4.0f * Minimap_radar_blip_scale), (size * 0.0f * Minimap_radar_blip_scale) * scale, (size * 4.0f * Minimap_radar_blip_scale) * scale);
        const vector bmin(scale * (size * -4.0f * Minimap_radar_blip_scale), (size * 0.0f * Minimap_radar_blip_scale) * scale, (size * -4.0f * Minimap_radar_blip_scale) * scale);
        render_bitmap(&ring_bui, hp->pos, bmin, bmax, nullptr);
    }
    rl_draw::get_instance()->tristrip_2d_end();
    // The humans, fading since the line passed them.
    gr_set_texture(human_handle);
    rl_draw::get_instance()->tristrip_2d_begin(m_rl_render_state);
    const vector bmin(scale * -4.0f, scale * 0.0f, scale * -4.0f);
    const vector bmax(scale * 4.0f, scale * 0.0f, scale * 4.0f);
    for (int i = 0; i < count; i++)
    {
        const human* hp = m_human_list[i];
        const human_team_dispositions disposition = target != nullptr ? human_teams_get_disposition(target, hp) : TEAM_DISPOSITION_NONE;
        const color col = get_human_color(hp, disposition);
        const float y = world_to_window(hp->pos.x, hp->pos.z).y;
        float behind = y - m_scan_line_pos;
        const float cycle = (m_mini_size_info.map_end_posn.y - m_mini_size_info.map_posn.y) * 2.0f;
        if (0.0f < behind)
        {
            behind = y - (cycle + m_scan_line_pos);
        }
        float t = -behind / cycle;
        if (1.0f < t)
        {
            t = 1.0f;
        }
        float alpha = (255.0f - t * 255.0f) + Minimap_radar_min_alpha;
        if (255.0f - alpha < 0.0f)
        {
            alpha = 255.0f;
        }
        gr_set_color(col);
        gr_set_alpha((int)(alpha <= 0.0f ? 0.0f : alpha));
        render_bitmap(&human_bui, hp->pos, bmin, bmax, nullptr);
    }
    rl_draw::get_instance()->tristrip_2d_end();
}

// 0x00860020 (:3918)
void cartographer::render_guerrilla_converts(float zoom_scale)
{
    const int bitmap_id = vint_bms_load("ui_hud_house_arrest_head_icon");
    map_image_info mii;
    bmp_uv_information bui;
    get_image_info(mii, bui, bitmap_id);
    const float width = (float)mii.image_data->bmw;
    const float height = (float)mii.image_data->bmh;
    int finished = -1;
    for (int i = 0; i < m_guerrilla_converts.size(); i++)
    {
        convert_guerrilla_icon& icon = m_guerrilla_converts[i];
        if (icon.m_timer.valid() && icon.m_timer.elapsed())
        {
            finished = i;
            continue;
        }
        // Magic numbers: each head shows for 500 ms, the next one follows after 250 ms.
        if (m_guerrilla_convert_delay.valid() && !icon.m_timer.valid())
        {
            if (!m_guerrilla_convert_delay.elapsed())
            {
                break;
            }
            icon.m_timer.set(500);
            m_guerrilla_convert_delay.set(0xFA);
        }
        else if (!m_guerrilla_convert_delay.valid())
        {
            icon.m_timer.set(500);
            m_guerrilla_convert_delay.set(0xFA);
        }
        const float t = (float)icon.m_timer.time_until() / 500.0f;
        const float scale = t * 0.5f * zoom_scale;
        object* objp = World.object_from_handle(icon.m_human_handle);
        if (objp == nullptr)
        {
            finished = i;
            continue;
        }
        gr_set_color(0xF0, 200, 0x34, (int)(t * 191.0f));
        const vector bmax(width * scale, scale * 0.0f, height * scale);
        const vector bmin(-width * scale, scale * 0.0f, -height * scale);
        render_bitmap(&bui, objp->pos, bmin, bmax, &mii);
    }
    if (finished != -1)
    {
        m_guerrilla_converts.remove(finished);
    }
}

// 0x00860310 (:3970)
void cartographer::render_blast_from_the_past_tags(float zoom_scale)
{
    if (m_bftp_timer < Bftp_icon_lifetime_ms)
    {
        const unsigned char alpha = (unsigned char)(int)((1.0f - (float)m_bftp_timer / (float)Bftp_icon_lifetime_ms) * 255.0f);
        m_bftp_timer = m_bftp_timer + fl_round(Frametime * 1000.0f);
        const int count = m_bftp_nodes.size();
        if (count <= 0)
        {
            return;
        }
        const int bitmap_id = get_minimap_bitmap_id(MAP_ELEMENT_BLIP);
        const bm_image* image = vint_bms_get_image_data(bitmap_id);
        const int bitmap_handle = vint_bms_get_bitmap(bitmap_id);
        bmp_uv_information bui;
        get_uv_information_bmpsheet(image, bitmap_handle, bui.uvs[0], bui.uvs[1], bui.uvs[2], bui.uvs[3]);
        gr_set_texture(bitmap_handle);
        rl_draw::get_instance()->tristrip_2d_begin(m_rl_render_state);
        // Magic number: 3.5, the tag's half size.
        const vector bmax(zoom_scale * 3.5f, zoom_scale * 0.0f, zoom_scale * 3.5f);
        const vector bmin(zoom_scale * -3.5f, zoom_scale * 0.0f, zoom_scale * -3.5f);
        for (int i = 0; i < count; i++)
        {
            gr_set_color(color(0x40, 0xFF, 0x40, alpha));
            render_bitmap(&bui, m_bftp_nodes[i]->pos, bmin, bmax, nullptr);
        }
        rl_draw::get_instance()->tristrip_2d_end();
        return;
    }
    if (m_bftp_timer < Bftp_icon_timer_ms)
    {
        m_bftp_timer = m_bftp_timer + fl_round(Frametime * 1000.0f);
        return;
    }
    m_bftp_timer = 0;
}

// 0x0086DED0 (:4021)
void cartographer::render_vehicles(float zoom_scale)
{
    const vehicle* own = minimap_local_vehicle();
    for (int i = 0; i < m_vehicle_list.size(); i++)
    {
        const vehicle* vp = m_vehicle_list[i];
        const color col = get_vehicle_color(vp);
        map_image_info mii;
        bmp_uv_information bui;
        get_image_info(mii, bui, get_minimap_bitmap_id(MAP_ELEMENT_VEHICLE));
        gr_set_texture(mii.bitmap_handle);
        gr_set_color(col);
        const vector pos(vp != own ? vp->pos : m_current_size_info.center_pos_world);
        matrix orient;
        orient.make(vp->orient.fvec);
        // The radar's line: the alpha fades with the distance to the line, on both sides.
        const float behind = (world_to_window(pos.x, pos.z).y) - m_scan_line_pos;
        const float height = m_mini_size_info.map_end_posn.y - m_mini_size_info.map_posn.y;
        int alpha;
        if (behind > 0.0f)
        {
            float t = behind / (height * 2.0f);
            if (1.0f < t)
            {
                t = 1.0f;
            }
            const float value = t * 255.0f + Minimap_radar_min_alpha;
            alpha = 0.0f <= 255.0f - value ? (int)(0.0f < value ? value : 0.0f) : 0xFF;
        }
        else if (0.0f <= behind)
        {
            alpha = 0xFF;
        }
        else
        {
            float t = -behind / (height * 2.0f);
            if (1.0f < t)
            {
                t = 1.0f;
            }
            const float value = (255.0f - t * 255.0f) + Minimap_radar_min_alpha;
            alpha = 0.0f <= 255.0f - value ? (int)(0.0f < value ? value : 0.0f) : 0xFF;
        }
        gr_set_alpha(alpha);
        // Magic number: 4.875, the vehicle icon's half size in meters.
        const vector bmax(4.875f, 0.0f, 4.875f);
        const vector bmin(-4.875f, 0.0f, -4.875f);
        render_bitmap_oriented(&bui, pos, orient, bmin, bmax, &mii, RUVS_180, false);
    }
}

// 0x00860560 (:4103)
void cartographer::render_restricted_areas()
{
    rl_draw::get_instance()->linelist_2d_begin(m_rl_render_state);
    for (int i = 0; i < m_counts[1]; i++)
    {
        if (m_restricted_areas[i] == 0xFFFFFFFF)
        {
            continue;
        }
        object_restricted_area* area = (object_restricted_area*)World.object_from_handle(m_restricted_areas[i]);
        if (area == nullptr || area->get_obj_type() != OT_RESTRICTED_AREA)
        {
            continue;
        }
        const int yellow = area->get_yellow_num_points();
        if (yellow <= 2)
        {
            continue;
        }
        gr_set_color(0xFF, 0xFF, 0, 0xFF);
        vector2 point;
        area->get_yellow_point(point, yellow - 1);
        vector2 last = world_to_window(point.x, point.y);
        for (int p = 0; p < yellow; p++)
        {
            area->get_yellow_point(point, p);
            const vector2 next = world_to_window(point.x, point.y);
            rl_draw::get_instance()->linelist_2d_add_line(last.x, last.y, next.x, next.y);
            last = next;
        }
        const int warning = area->get_warning_num_points();
        if (warning <= 2)
        {
            continue;
        }
        gr_set_color(0x7F, 0x7F, 0x7F, 0xFF);
        area->get_warning_point(point, warning - 1);
        last = world_to_window(point.x, point.y);
        for (int p = 0; p < warning; p++)
        {
            area->get_warning_point(point, p);
            const vector2 next = world_to_window(point.x, point.y);
            rl_draw::get_instance()->linelist_2d_add_line(last.x, last.y, next.x, next.y);
            last = next;
        }
    }
    rl_draw::get_instance()->linelist_2d_end();
}

// 0x00860B10 (:4181)
// Sets each mover's flags: 0x10 mining; 4 or 8, an EDF or other mover with its own thumbnail; 1 or 2, one without.
void cartographer::sort_structures(structure_buff* structures)
{
    structures->edf_buildings_used = false;
    structures->non_edf_buildings_used = false;
    if (structures->num_movers > 0)
    {
        memset(structures->flags, 0, structures->num_movers);
    }
    for (int i = 0; i < structures->num_movers; i++)
    {
        if (structures->movers[i] == 0xFFFFFFFF)
        {
            continue;
        }
        object* objp = World.object_from_handle(structures->movers[i]);
        if (objp == nullptr || objp->get_obj_type() != OT_MOVER)
        {
            continue;
        }
        object_mover* mp = (object_mover*)objp;
        // Magic number: 0x20000, an object_mover flag.
        if ((mp->mover_flags & 0x20000) != 0)
        {
            structures->flags[i] |= 0x10;
        }
        c_chunk_base* chunk = mp->obj_flags.streamed ? chunk_get_base_by_uid(mp->chunk_uid) : nullptr;
        human_teams team = mp->team;
        // A piece's team is its parent's.
        if (mp->get_sub_type() == OT_SUB_MOVER_RFG && ((rfg_mover*)mp)->flags.inherit_damage_pct && mp->get_parent() != 0xFFFFFFFF)
        {
            object* parent = World.object_from_handle(mp->get_parent());
            if (parent != nullptr && parent->get_obj_type() == OT_MOVER)
            {
                team = ((object_mover*)parent)->team;
            }
        }
        const bool edf = team == HUMAN_TEAM_EDF;
        bool has_thumbnail = false;
        if (m_mats_created && chunk != nullptr)
        {
            char base_name[64];
            char thumbnail[64];
            cf_drop_ext(base_name, 0x40, chunk->filename);
            sprintf(thumbnail, "%s_thumb", base_name);
            has_thumbnail = vint_bms_load(thumbnail) != -1;
        }
        if (has_thumbnail)
        {
            structures->flags[i] |= edf ? 4 : 8;
        }
        else
        {
            structures->flags[i] |= edf ? 1 : 2;
            if (edf)
            {
                structures->edf_buildings_used = true;
            }
            else
            {
                structures->non_edf_buildings_used = true;
            }
        }
    }
}

// A building's remaining health, 0 to 1. Inlined at each use in rfg.exe.
static float mover_health_percent(object_mover* mp)
{
    if (mp->get_sub_type() == OT_SUB_MOVER_RFG)
    {
        rfg_mover* rp = (rfg_mover*)mp;
        float destroyed = rp->percent_destroyed() / rp->game_destroyed_pct;
        if (0.0f <= destroyed - 1.0f)
        {
            destroyed = 1.0f;
        }
        return 1.0f - destroyed;
    }
    // Magic number: 0x10, a general_mover flag: destroyed.
    const int hitpoints = (((general_mover*)mp)->gm_flags & 0x10) == 0 ? mp->m_level_obj_properties->m_hitpoints : 0;
    return (float)hitpoints / (float)mp->m_level_obj_properties->m_base_properties->m_max_hitpoints;
}

// The medium or high importance list of a collected mover. Inlined twice in render_structures.
static void sort_important_mover(cartographer* map, object_mover* mp, unsigned int handle, farray<unsigned int,512>& med, farray<unsigned int,512>& high)
{
    const int icon = map->get_structure_importance_icon(mp);
    if (icon < 0)
    {
        // Magic number: 0x20000, an object_mover flag: mining.
        if ((mp->mover_flags & 0x20000) != 0 && !mp->is_killed())
        {
            high.add_unique(handle);
        }
    }
    else if (icon == MAP_ELEMENT_IMPORTANCE_MEDIUM)
    {
        med.add_unique(handle);
    }
    else
    {
        high.add_unique(handle);
    }
}

// 0x0087D2E0 (:4254)
void cartographer::render_structures(float zoom_scale)
{
    clear_important_bldg_health_flags();
    const int count = m_collected_structures[0].num_movers;
    float pulse = (float)m_bftp_timer / (float)Bftp_icon_lifetime_ms;
    if (2.0f < pulse)
    {
        pulse = 4.0f - pulse;
    }
    const unsigned char mining_add = (unsigned char)(int)(Mining_alpha_depth * pulse);
    if (count == 0)
    {
        return;
    }
    structure_buff& structures = m_collected_structures[0];
    farray<unsigned int,512> high;
    farray<unsigned int,512> med;
    // Original bug kept: 32 entries each and no check, for up to 512 movers.
    object_mover* masked_edf[32];
    object_mover* masked_non_edf[32];
    int num_masked_edf = 0;
    int num_masked_non_edf = 0;
    for (int i = 0; i < count; i++)
    {
        const unsigned char flags = structures.flags[i];
        const unsigned int handle = structures.movers[i];
        object* objp = handle != 0xFFFFFFFF ? World.object_from_handle(handle) : nullptr;
        object_mover* mp = objp != nullptr && objp->get_obj_type() == OT_MOVER ? (object_mover*)objp : nullptr;
        if ((flags & 0x10) != 0 && mp != nullptr)
        {
            add_important_bldg(handle, mover_health_percent(mp));
        }
        if ((flags & 4) != 0)
        {
            if (mp != nullptr)
            {
                masked_edf[num_masked_edf++] = mp;
                sort_important_mover(this, mp, handle, med, high);
            }
        }
        else if ((flags & 8) != 0 && mp != nullptr)
        {
            masked_non_edf[num_masked_non_edf++] = mp;
        }
    }
    if (structures.edf_buildings_used)
    {
        const int texture = bm_load(cEdf_building_texture, false);
        int tex_w, tex_h;
        float scale_x, scale_y;
        bm_get_dimensions(texture, tex_w, tex_h, scale_x, scale_y);
        gr_set_texture(texture);
        rl_draw::get_instance()->tristrip_2d_begin(m_rl_render_state);
        for (int i = 0; i < count; i++)
        {
            if ((structures.flags[i] & 1) == 0 || structures.movers[i] == 0xFFFFFFFF)
            {
                continue;
            }
            object* objp = World.object_from_handle(structures.movers[i]);
            if (objp == nullptr || objp->get_obj_type() != OT_MOVER)
            {
                continue;
            }
            object_mover* mp = (object_mover*)objp;
            sort_important_mover(this, mp, structures.movers[i], med, high);
            vector bmin, bmax;
            mp->get_local_bbox(bmin, bmax);
            const rl_color rgba = get_structure_color(mp);
            gr_set_color(color(rgba.red, rgba.green, rgba.blue, rgba.alpha));
            matrix orient;
            orient.make(mp->orient.fvec);
            render_tiled_bitmap_oriented(mp->pos, orient, bmin, bmax, tex_w, tex_h);
        }
        rl_draw::get_instance()->tristrip_2d_end();
    }
    if (structures.non_edf_buildings_used)
    {
        const int texture = bm_load(cGeneric_building_texture, false);
        int tex_w, tex_h;
        float scale_x, scale_y;
        bm_get_dimensions(texture, tex_w, tex_h, scale_x, scale_y);
        gr_set_texture(texture);
        rl_draw::get_instance()->tristrip_2d_begin(m_rl_render_state);
        for (int i = 0; i < count; i++)
        {
            if ((structures.flags[i] & 2) == 0 || structures.movers[i] == 0xFFFFFFFF)
            {
                continue;
            }
            object* objp = World.object_from_handle(structures.movers[i]);
            if (objp == nullptr || objp->get_obj_type() != OT_MOVER)
            {
                continue;
            }
            object_mover* mp = (object_mover*)objp;
            vector bmin, bmax;
            mp->get_local_bbox(bmin, bmax);
            const rl_color rgba = get_structure_color(mp);
            gr_set_color(color(rgba.red, rgba.green, rgba.blue, rgba.alpha));
            if (mp->get_sub_type() == OT_SUB_MOVER_RFG && ((rfg_mover*)mp)->flags.mining)
            {
                // A mining target: a small pulsing square. Magic numbers: its base color and 1.5 m half size.
                gr_set_color(color(mining_add + 0x0A, 0x00, mining_add + 0x28, 0xFF));
                bmin.z = -1.5f;
                bmin.x = -1.5f;
                bmax.z = 1.5f;
                bmax.x = 1.5f;
            }
            matrix orient;
            orient.make(mp->orient.fvec);
            render_tiled_bitmap_oriented(mp->pos, orient, bmin, bmax, tex_w, tex_h);
        }
        rl_draw::get_instance()->tristrip_2d_end();
    }
    render_masked_structures(masked_edf, num_masked_edf, masked_non_edf, num_masked_non_edf);
    set_up_important_bldgs(med, high, false, zoom_scale);
}

// 0x0087D960 (:4440)
void cartographer::render_structures_full(float zoom_scale)
{
    const int count = m_collected_structures[0].num_movers;
    if (count <= 0)
    {
        return;
    }
    farray<unsigned int,512> high;
    farray<unsigned int,512> med;
    for (int i = 0; i < count; i++)
    {
        const unsigned int handle = m_collected_structures[0].movers[i];
        if (handle == 0xFFFFFFFF)
        {
            continue;
        }
        object* objp = World.object_from_handle(handle);
        if (objp == nullptr || objp->get_obj_type() != OT_MOVER)
        {
            continue;
        }
        const int icon = get_structure_importance_icon((object_mover*)objp);
        if (icon < 0)
        {
            continue;
        }
        if (fog_of_war_is_posn_obscured(objp->pos) && !visible_over_fow_list.exists(objp->get_handle()))
        {
            continue;
        }
        const float dx = m_current_size_info.center_pos_world.x - objp->pos.x;
        const float dy = m_current_size_info.center_pos_world.y - objp->pos.y;
        const float dz = m_current_size_info.center_pos_world.z - objp->pos.z;
        if (dz * dz + dy * dy + dx * dx <= m_current_size_info.zoom_radius_squared)
        {
            (icon == MAP_ELEMENT_IMPORTANCE_MEDIUM ? med : high).add(handle);
        }
    }
    set_up_important_bldgs(med, high, true, zoom_scale);
}

// One mover of render_masked_structures: the material takes the color index in alpha, the thumbnail's uvs in red and
// green and the flash in blue; the vertex uvs tile the building texture.
void cartographer::render_masked_mover(object_mover* mp, int tex_w, int tex_h, bool use_own_orient)
{
    rl_primitive_2d_vert* verts = rl_draw::get_instance()->tristrip_2d_allocate_tris(2);
    if (verts == nullptr)
    {
        return;
    }
    float flash = 0.0f;
    const unsigned char color_index = (unsigned char)get_structure_color_index(mp, flash);
    for (int i = 0; i < 4; i++)
    {
        verts[i].color.alpha = color_index;
    }
    if (mp->chunk_uid == 0xFFFFFFFF)
    {
        return;
    }
    c_chunk_base* chunk = chunk_get_base_by_uid(mp->chunk_uid);
    if (chunk == nullptr)
    {
        return;
    }
    vector bmin, bmax;
    mp->get_local_bbox(bmin, bmax);
    char base_name[64];
    char thumbnail[64];
    cf_drop_ext(base_name, 0x40, chunk->filename);
    sprintf(thumbnail, "%s_thumb", base_name);
    const int bitmap_id = vint_bms_load(thumbnail);
    const int bitmap_handle = vint_bms_get_bitmap(bitmap_id);
    bmp_uv_information bui;
    get_uv_information_bmpsheet(vint_bms_get_image_data(bitmap_id), bitmap_handle, bui.uvs[0], bui.uvs[1], bui.uvs[2], bui.uvs[3]);
    // Original quirk kept: EDF movers use their own right vector, the others one made from the forward vector.
    matrix orient(mp->orient);
    if (!use_own_orient)
    {
        orient.make(mp->orient.fvec);
    }
    const float nz = -orient.rvec.z;
    const float rx = orient.rvec.x;
    const vector2 window[4] =
    {
        world_to_window(mp->pos.x + rx * bmin.x + bmax.z * nz, (bmax.z * rx - nz * bmin.x) + mp->pos.z),
        world_to_window(mp->pos.x + rx * bmax.x + bmax.z * nz, (bmax.z * rx - nz * bmax.x) + mp->pos.z),
        world_to_window(mp->pos.x + bmin.z * nz + rx * bmin.x, (bmin.z * rx - nz * bmin.x) + mp->pos.z),
        world_to_window(mp->pos.x + bmin.z * nz + rx * bmax.x, (bmin.z * rx - nz * bmax.x) + mp->pos.z),
    };
    const float dx0 = window[1].x - window[0].x;
    const float dy0 = window[1].y - window[0].y;
    const float u = sqrtf(dx0 * dx0 + dy0 * dy0) / (float)tex_w;
    const float dx1 = window[2].x - window[0].x;
    const float dy1 = window[2].y - window[0].y;
    const float v = sqrtf(dx1 * dx1 + dy1 * dy1) / (float)tex_h;
    verts[0].uvs.x = 0.0f;
    verts[0].uvs.y = 0.0f;
    verts[1].uvs.x = u;
    verts[1].uvs.y = 0.0f;
    verts[2].uvs.x = 0.0f;
    verts[2].uvs.y = v;
    verts[3].uvs.x = u;
    verts[3].uvs.y = v;
    for (int i = 0; i < 4; i++)
    {
        verts[i].pt.x = window[i].x;
        verts[i].pt.y = window[i].y;
        verts[i].color.red = (unsigned char)(int)(bui.uvs[i].x * 255.0f);
        verts[i].color.green = (unsigned char)(int)(bui.uvs[i].y * 255.0f);
        verts[i].color.blue = (unsigned char)(int)(flash * 255.0f);
    }
}

// 0x0086E2A0 (:4483)
void cartographer::render_masked_structures(object_mover** edf_array, int num_edf, object_mover** non_edf_array, int num_non_edf)
{
    int generic_w, generic_h, edf_w, edf_h;
    float scale_x, scale_y;
    bm_get_dimensions(bm_load(cGeneric_building_texture, false), generic_w, generic_h, scale_x, scale_y);
    bm_get_dimensions(bm_load(cEdf_building_texture, false), edf_w, edf_h, scale_x, scale_y);
    // Magic numbers: the map's state with bits 9-12 set to 2, as rfg.exe sets them.
    rl_primitive_state state;
    state.set_bits((m_rl_render_state.get_bits() & 0xFFFFE5FF) | 0x400);
    if (num_edf > 0)
    {
        rl_draw::get_instance()->set_material(m_edf_bldg_mat_handle);
        rl_draw::get_instance()->tristrip_2d_begin(state);
        for (int i = num_edf - 1; i > -1; i--)
        {
            render_masked_mover(edf_array[i], edf_w, edf_h, true);
        }
        rl_draw::get_instance()->tristrip_2d_end();
    }
    if (num_non_edf > 0)
    {
        rl_draw::get_instance()->set_material(m_non_edf_bldg_mat_handle);
        rl_draw::get_instance()->tristrip_2d_begin(state);
        for (int i = num_non_edf - 1; i > -1; i--)
        {
            render_masked_mover(non_edf_array[i], generic_w, generic_h, false);
        }
        rl_draw::get_instance()->tristrip_2d_end();
    }
}

// 0x00877750 (:4665)
void cartographer::set_up_important_bldgs(const farray<unsigned int,512>& med, const farray<unsigned int,512>& high, bool is_full, float zoom_scale)
{
    const int num_med = med.size();
    const int total = high.size() + num_med;
    m_bldg_icons.clear();
    for (int i = 0; i < total; i++)
    {
        const bool is_med = i < num_med;
        // Magic numbers: medium buildings show within 150 meters, high ones within 250.
        const float range = is_med ? 150.0f : 250.0f;
        const unsigned int handle = is_med ? med[i] : high[i - num_med];
        const minimap_elements element = is_med ? MAP_ELEMENT_IMPORTANCE_MEDIUM : MAP_ELEMENT_IMPORTANCE_HIGH;
        if (World.object_was_destroyed(handle) || handle == 0xFFFFFFFF)
        {
            continue;
        }
        object* objp = World.object_from_handle(handle);
        if (objp == nullptr || objp->get_obj_type() != OT_MOVER)
        {
            continue;
        }
        object_mover* mp = (object_mover*)objp;
        const float health = mover_health_percent(mp);
        if (health <= 0.0f)
        {
            continue;
        }
        add_important_bldg(handle, health);
        if (!is_full && handle == m_player_marker_item.m_obj_handle && m_player_marker_item.m_valid)
        {
            continue;
        }
        const bool visible = !fog_of_war_is_posn_obscured(mp->pos) || visible_over_fow_list.exists(mp->get_handle());
        const float dx = mp->pos.x - m_current_size_info.center_pos_world.x;
        const float dz = mp->pos.z - m_current_size_info.center_pos_world.z;
        int alpha;
        if (((is_full && !set_icon_zoomed_alpha(element, true, alpha, 0xFF)) || (dx * dx + dz * dz < range * range && (mp->mover_flags & 0x20000) == 0 && visible)) && m_bldg_icons.size() < m_bldg_icons.capacity())
        {
            map_bldg_icon icon(mp->pos, element, health, handle);
            m_bldg_icons.add(icon);
        }
    }
    if (is_full || !m_player_marker_item.m_valid || m_player_marker_item.m_obj_handle == 0xFFFFFFFF)
    {
        return;
    }
    // The marked building always shows on the minimap.
    object* objp = World.object_from_handle(m_player_marker_item.m_obj_handle);
    if (objp == nullptr || objp->get_obj_type() != OT_MOVER || objp->is_killed())
    {
        return;
    }
    object_mover* mp = (object_mover*)objp;
    const float health = mover_health_percent(mp);
    if (health > 0.0f)
    {
        const int icon_index = get_structure_importance_icon(mp);
        if (icon_index != -1 && m_bldg_icons.size() < m_bldg_icons.capacity())
        {
            map_bldg_icon icon(mp->pos, icon_index, health, m_player_marker_item.m_obj_handle);
            m_bldg_icons.add(icon);
        }
    }
}

// 0x0086F4D0 (:4772)
void cartographer::render_bldg_icons(float zoom_scale, bool is_full)
{
    if (!Show_important_bldgs && mission_is_active())
    {
        return;
    }
    // Magic number: icons past 90% of the radius stick to the edge.
    const float edge = m_current_size_info.zoom_radius * 0.9f;
    const float edge_squared = edge * edge;
    static float timer; // 0x02BE5920
    timer = Frametime_real + timer;
    const float pulse = timer * 2.0f - (float)(int)(timer * 2.0f);
    const float pulse_scale = pulse * 0.75f + 1.0f;
    const unsigned char pulse_alpha = (unsigned char)(int)((1.0f - pulse) * 255.0f);
    for (int i = 0; i < m_bldg_icons.size(); i++)
    {
        const float pos_x = m_bldg_icons[i].pos.x;
        const float pos_z = m_bldg_icons[i].pos.y;
        const unsigned int handle = m_bldg_icons[i].handle;
        const minimap_elements icon_bitmap = (minimap_elements)m_bldg_icons[i].bitmap;
        const float health = m_bldg_icons[i].health;
        const vector world_pos(pos_x, 0.0f, pos_z);
        check_highlight_for_item(world_pos, m_bldg_icons[i].render_pos, handle, icon_bitmap, IIT_BLDG, 0.0f, Default_map_item_color, 0xFFFFFFFF);
        if (is_full && (is_old_highlight_item(world_pos, handle, icon_bitmap, IIT_BLDG) || !fullscreen_map_is_loaded()))
        {
            continue;
        }
        const float flash = 0.5f < health ? 1.0f : m_bldg_health_flash_alpha;
        const int bitmap_id = icon_bitmap < NUM_MAP_ELEMENTS ? get_minimap_bitmap_id(icon_bitmap) : bm_get_bogus_bitmap_handle();
        const bm_image* image = vint_bms_get_image_data(bitmap_id);
        const int bitmap_handle = vint_bms_get_bitmap(bitmap_id);
        bmp_uv_information bui;
        get_uv_information_bmpsheet(image, bitmap_handle, bui.uvs[0], bui.uvs[1], bui.uvs[2], bui.uvs[3]);
        gr_set_texture(bitmap_handle);
        int alpha = (int)(flash * 255.0f);
        gr_set_color(0xFF, 0xFF, 0xFF, alpha);
        if (is_full && (icon_bitmap > MAP_ELEMENT_CAMERA_CONE || !minimap_element_is_visible_when_zoomed(icon_bitmap)))
        {
            float percent = fsm_zoom_percent();
            if (percent <= Fsm_max_visible_dist)
            {
                continue;
            }
            if (percent <= Fsm_min_visible_dist)
            {
                percent = (percent - Fsm_max_visible_dist) / (Fsm_min_visible_dist - Fsm_max_visible_dist);
                if (0.0f <= 0.0f - percent)
                {
                    percent = 0.0f;
                }
                alpha = (int)((float)alpha * percent);
                gr_set_alpha(alpha);
            }
        }
        float min_x = (float)image->bmw * -0.5f;
        float min_y = (float)image->bmh * -0.5f;
        float max_x = (float)image->bmw * 0.5f;
        float max_y = (float)image->bmh * 0.5f;
        if (!is_full)
        {
            // Magic number: the minimap's icons are 3/4 size.
            min_x = min_x * 0.75f;
            min_y = min_y * 0.75f;
            max_x = max_x * 0.75f;
            max_y = max_y * 0.75f;
        }
        vector draw_pos(pos_x, 0.0f, pos_z);
        const float dx = pos_x - m_current_size_info.center_pos_world.x;
        const float dz = pos_z - m_current_size_info.center_pos_world.z;
        const float dist_squared = dx * dx + dz * dz;
        const float scaled_max_x = zoom_scale * max_x;
        const float scaled_max_y = zoom_scale * max_y;
        const float scaled_min_x = zoom_scale * min_x;
        const float scaled_min_y = zoom_scale * min_y;
        const vector2 icon_max(scaled_max_x * Resolution_minimap_scale.y, scaled_max_y * Resolution_minimap_scale.y);
        const vector2 icon_min(scaled_min_x * Resolution_minimap_scale.x, scaled_min_y * Resolution_minimap_scale.x);
        if (is_full)
        {
            render_single_icon_direct(m_bldg_icons[i].render_pos, icon_min, icon_max, icon_bitmap, alpha, m_rl_render_state);
        }
        else
        {
            if (dist_squared > edge_squared && !mission_is_active() && activity_none())
            {
                draw_pos.x = (edge / sqrtf(dist_squared)) * dx + m_current_size_info.center_pos_world.x;
                draw_pos.z = (edge / sqrtf(dist_squared)) * dz + m_current_size_info.center_pos_world.z;
            }
            render_single_icon(draw_pos, icon_min, icon_max, icon_bitmap, m_rl_render_state);
        }
        const bool marked = m_has_player_marker && m_player_marker_item.m_valid && ((m_player_marker_item.m_obj_handle != 0xFFFFFFFF && m_player_marker_item.m_obj_handle == handle) || (m_player_marker_item.m_obj_pos.x == pos_x && m_player_marker_item.m_obj_pos.y == 0.0f && m_player_marker_item.m_obj_pos.z == pos_z));
        if (marked)
        {
            gr_set_alpha(pulse_alpha);
            set_icon_zoomed_alpha(icon_bitmap, is_full, alpha, pulse_alpha);
            const vector2 pulse_max(scaled_max_x * pulse_scale * Resolution_minimap_scale.y, scaled_max_y * pulse_scale * Resolution_minimap_scale.y);
            const vector2 pulse_min(scaled_min_x * pulse_scale * Resolution_minimap_scale.x, scaled_min_y * pulse_scale * Resolution_minimap_scale.x);
            if (is_full)
            {
                render_single_icon_direct(m_bldg_icons[i].render_pos, pulse_min, pulse_max, icon_bitmap, alpha, m_rl_render_state);
            }
            else
            {
                render_single_icon(draw_pos, pulse_min, pulse_max, icon_bitmap, m_rl_render_state);
            }
        }
    }
}

// 0x0086FF50 (:4863)
// The health percentages of recently damaged buildings.
void cartographer::render_structures_post()
{
    const int count = m_important_bldgs.size();
    if (count <= 0)
    {
        return;
    }
    const float edge = m_current_size_info.zoom_radius * 0.9f;
    const float edge_squared = edge * edge;
    const int medium_id = get_minimap_bitmap_id(MAP_ELEMENT_IMPORTANCE_MEDIUM);
    const int high_id = get_minimap_bitmap_id(MAP_ELEMENT_IMPORTANCE_HIGH);
    const bm_image* medium_image = vint_bms_get_image_data(medium_id);
    const bm_image* high_image = vint_bms_get_image_data(high_id);
    map_image_info mii;
    bmp_uv_information bui;
    get_image_info(mii, bui, vint_bms_load("ui_map_base_square"));
    for (int i = 0; i < count; i++)
    {
        const bldg_health_info& info = m_important_bldgs[i];
        if (info.m_time_since_damaged < 0.0f)
        {
            continue;
        }
        // Magic number: shown for 3 seconds, then a second to fade.
        float alpha = 1.0f;
        if (3.0f < info.m_time_since_damaged)
        {
            alpha = 1.0f - (info.m_time_since_damaged - 3.0f);
            if (alpha < 0.0f)
            {
                alpha = 0.0f;
            }
        }
        if (info.m_mover_handle == 0xFFFFFFFF)
        {
            continue;
        }
        object* objp = World.object_from_handle(info.m_mover_handle);
        if (objp == nullptr || objp->get_obj_type() != OT_MOVER)
        {
            continue;
        }
        const bm_image* image = get_structure_importance_icon((object_mover*)objp) == MAP_ELEMENT_IMPORTANCE_MEDIUM ? medium_image : high_image;
        const float icon_radius = (float)(image->bmw < image->bmh ? image->bmw : image->bmh) * 0.5f;
        // Between 1% and 99%, a damaged building never reads 0 or 100.
        float percent = info.m_last_health_pct * 100.0f;
        if (percent - 1.0f < 0.0f)
        {
            percent = 1.0f;
        }
        if (percent - 99.0f >= 0.0f)
        {
            percent = 99.0f;
        }
        wchar_t text[16];
        swprintf(text, 16, L"%d%%", (int)percent);
        const float str_width = (float)ui_get_usage_character_width(text, Number_font) * Font_scale;
        const float str_height = (float)ui_get_usage_character_height(text, Number_font) * Font_scale;
        vector pos(objp->pos);
        const float dx = pos.x - m_current_size_info.center_pos_world.x;
        const float dz = pos.z - m_current_size_info.center_pos_world.z;
        const float dist_squared = dx * dx + dz * dz;
        if (edge_squared < dist_squared)
        {
            const float scale = edge / sqrtf(dist_squared);
            pos.x = scale * dx + m_current_size_info.center_pos_world.x;
            pos.z = scale * dz + m_current_size_info.center_pos_world.z;
        }
        bbox box;
        if (get_best_placement_info_for_string(box, pos, str_width, str_height, icon_radius))
        {
            const int text_alpha = (int)(alpha * 255.0f) & 0xFF;
            gr_set_texture(mii.bitmap_handle);
            gr_set_color(0, 0, 0, text_alpha);
            const vector2 center((float)fl_round((box.min.x + box.max.x) * 0.5f), (float)fl_round((box.min.z + box.max.z) * 0.5f));
            const vector box_min((box.max.x - box.min.x) * -0.5f, 0.0f, (box.max.z - box.min.z) * -0.5f);
            const vector box_max(-box_min.x, 0.0f, -box_min.z);
            render_bitmap_direct(&bui, center, box_min, box_max, &mii, 0xFF);
            gr_set_color(0xFF, 0xFF, 0xFF, text_alpha);
            gr_string_scale((int)((float)Char_padding + box.min.x), (int)((float)Char_padding + box.min.z), text, Font_scale, Number_font, GM_FILTER);
        }
    }
}

// 0x0087DB60 (:4952)
void cartographer::render_district_liberated_icons(float zoom_scale)
{
    map_image_info mii;
    bmp_uv_information bui;
    get_image_info(mii, bui, vint_bms_load("ui_fsmap_guerrilla_logo"));
    const float width = (float)mii.image_data->bmw;
    const float height = (float)mii.image_data->bmh;
    const float percent = fsm_zoom_percent();
    const float scale = (Liberated_icon_scale_zoom_in - Liberated_icon_scale_zoom_out) * percent + Liberated_icon_scale_zoom_out;
    if (percent >= 0.75f)
    {
        return;
    }
    // Magic numbers: 0.75 opaque, fading out while zooming from 50% to 75%.
    float alpha = 0.75f;
    if (0.5f <= percent)
    {
        alpha = (0.75f - percent) * 4.0f;
        if (0.0f <= 0.0f - alpha)
        {
            alpha = 0.0f;
        }
        alpha = alpha * 0.75f;
    }
    gr_set_color(0xFF, 0xFF, 0xFF, (int)(alpha * 255.0f));
    for (int i = 0; i < fsm_district_liberated_icons.size(); i++)
    {
        const district_liberated_icon& icon = fsm_district_liberated_icons[i];
        // Magic numbers: zones are 511 meters, the world's corner is at 4088, x and z count 2 meter steps.
        const vector pos(((float)(icon.zone_xz >> 4) * 511.0f - 4088.0f) + (float)icon.x * 2.0f, 0.0f, -((float)(icon.zone_xz & 0xF) * 511.0f - 4088.0f) - (float)icon.z * 2.0f);
        // The district boundaries are compared on the map's window.
        const vector2 window = world_to_window(pos.x, pos.z);
        const district* dp = get_district_from_point_2d(window);
        if (dp != nullptr && dp->is_liberated())
        {
            const vector bmax(width * scale, scale * 0.0f, height * scale);
            const vector bmin(-width * scale, scale * 0.0f, -height * scale);
            render_bitmap(&bui, pos, bmin, bmax, &mii);
        }
    }
}

// 0x00860D10 (:4992)
void cartographer::render_static_zones()
{
    player* pp = get_target_player();
    if (pp == nullptr)
    {
        return;
    }
    bool in_zone = false;
    if (m_static_target != 0xFFFFFFFF)
    {
        object* target = World.object_from_handle(m_static_target);
        if (target == nullptr)
        {
            m_static_target = 0xFFFFFFFF;
        }
        else
        {
            const vector player_pos(pp->pos.x, 0.0f, pp->pos.z);
            const vector center(target->pos.x, 0.0f, target->pos.z);
            in_zone = ix_point_in_sphere(center, m_static_radius, player_pos);
        }
    }
    static float target_alpha = Mini_static_max_alpha; // 0x02BE58C0
    static int static_uv_clock; // 0x02BE58BC
    static float variation_timer; // 0x02BE58B8
    static float static_alpha; // 0x02BE58B4
    static_uv_clock = (static_uv_clock + 1) % 4;
    variation_timer = Frametime + variation_timer;
    if (1.0f <= variation_timer)
    {
        variation_timer = 0.0f;
        // Magic number: up to 0.24 below the maximum.
        target_alpha = (float)((double)Mini_static_max_alpha - (double)(rand() % 0x19) / 100.0);
    }
    if (m_map_mode != MAP_MODE_MINI)
    {
        return;
    }
    if (in_zone)
    {
        if (static_alpha < target_alpha)
        {
            static_alpha = Mini_static_fade_rate * Frametime + static_alpha;
        }
        else if (static_alpha > target_alpha)
        {
            static_alpha = static_alpha - Mini_static_fade_rate * Frametime;
        }
    }
    else
    {
        if (static_alpha <= 0.0f)
        {
            return;
        }
        static_alpha = static_alpha - Mini_static_fade_rate * Frametime;
    }
    if (static_alpha <= 0.0f)
    {
        return;
    }
    gr_set_texture(bm_load("ui_fsmap_static.tga", false));
    rl_draw::get_instance()->tristrip_2d_begin(m_rl_render_state);
    rl_primitive_2d_vert* verts = rl_draw::get_instance()->tristrip_2d_allocate_tris(2);
    if (verts != nullptr)
    {
        // Magic numbers: the static's color; the quad's uvs turn each frame.
        static const vector2 clock_uvs[4][4] =
        {
            {vector2(0.0f, 1.0f), vector2(1.0f, 1.0f), vector2(0.0f, 0.0f), vector2(1.0f, 0.0f)},
            {vector2(1.0f, 1.0f), vector2(1.0f, 0.0f), vector2(0.0f, 1.0f), vector2(0.0f, 0.0f)},
            {vector2(1.0f, 0.0f), vector2(0.0f, 0.0f), vector2(1.0f, 1.0f), vector2(0.0f, 1.0f)},
            {vector2(0.0f, 0.0f), vector2(0.0f, 1.0f), vector2(1.0f, 0.0f), vector2(1.0f, 1.0f)},
        };
        const float x0 = m_current_size_info.map_posn.x;
        const float y0 = m_current_size_info.map_posn.y;
        const float x1 = m_current_size_info.map_size.x + x0;
        const float y1 = m_current_size_info.map_size.y + y0;
        const vector2 corners[4] = {vector2(x0, y0), vector2(x1, y0), vector2(x0, y1), vector2(x1, y1)};
        for (int i = 0; i < 4; i++)
        {
            verts[i].color.red = 0x74;
            verts[i].color.green = 0x76;
            verts[i].color.blue = 0x4C;
            verts[i].color.alpha = (unsigned char)(int)(static_alpha * 255.0f);
            verts[i].pt.x = corners[i].x;
            verts[i].pt.y = corners[i].y;
            verts[i].uvs.x = clock_uvs[static_uv_clock][i].x;
            verts[i].uvs.y = clock_uvs[static_uv_clock][i].y;
        }
    }
    rl_draw::get_instance()->tristrip_2d_end();
}

// 0x0084CFD0 (:5096)
void cartographer::render_convoys(bool is_full)
{
}

// 0x008705C0 (:5102)
void cartographer::get_path_screen_pts(farray<vector,384>& csp, const path_info& path_data, bool is_full, bool convoy)
{
    csp.clear();
    const int count = path_data.pts.size();
    int first = 0;
    if (!convoy)
    {
        player* pp = get_target_player();
        if (pp != nullptr && Current_gps_point < count - 1)
        {
            // The route's passed points are left out: past the middle of the next segment, on to the next point.
            const vector& point = path_data.pts[Current_gps_point];
            const vector& next = path_data.pts[Current_gps_point + 1];
            const float mid_x = (point.x + next.x) * 0.5f;
            const float mid_y = (point.y + next.y) * 0.5f;
            first = Current_gps_point;
            if (0.0f < (pp->pos.z - mid_y) * (next.y - mid_y) + (pp->pos.x - mid_x) * (next.x - mid_x))
            {
                Current_gps_point = Current_gps_point + 1;
                first = Current_gps_point;
            }
        }
    }
    for (int i = first; i < count; i++)
    {
        const vector& point = path_data.pts[i];
        const vector2 window = world_to_window(point.x, point.y);
        csp.add(vector(window.x, window.y, point.z));
    }
}

// 0x00881350 (:5158)
void cartographer::add_patrol_path(unsigned int obj, bool edf_patrol)
{
    if (obj == 0xFFFFFFFF)
    {
        return;
    }
    object* objp = World.object_from_handle(obj);
    if (objp == nullptr || objp->get_obj_type() != OT_PATROL)
    {
        return;
    }
    for (int i = 0; i < m_patrol_routes.size(); i++)
    {
        if (m_patrol_routes[i].handle == obj)
        {
            return;
        }
    }
    static path_info empty_route; // a local of rfg.exe; 10 KB, the farray copies it
    const int index = m_patrol_routes.add(empty_route);
    if (index == -1)
    {
        return;
    }
    path_info& route = m_patrol_routes[index];
    route.handle = obj;
    route.edf_route = edf_patrol;
    farray<navpoint*,512> nodes;
    if (!((object_patrol*)objp)->get_all_patrol_nodes(nodes))
    {
        m_patrol_routes.remove(index);
        return;
    }
    const int num_nodes = nodes.size();
    if (num_nodes < 2)
    {
        m_patrol_routes.remove(index);
        return;
    }
    // The spline's end points are doubled.
    route.spline_points[0].x = nodes[0]->pos.x;
    route.spline_points[0].y = nodes[0]->pos.z;
    // Magic number: 0x1FF (511), the copy stops one short of the 512 node cap. Original bug kept: with 512
    // nodes the last middle point is skipped, and the doubled end at num_nodes + 1 leaves a gap in the spline.
    for (int i = 0; i < num_nodes && i < 0x1FF; i++)
    {
        route.spline_points[i + 1].x = nodes[i]->pos.x;
        route.spline_points[i + 1].y = nodes[i]->pos.z;
    }
    route.spline_points[num_nodes + 1].x = nodes[num_nodes - 1]->pos.x;
    route.spline_points[num_nodes + 1].y = nodes[num_nodes - 1]->pos.z;
    route.num_spline_pts = num_nodes + 2;
    m_gps_timer = 0.0f;
}

// 0x00881680 (:5217)
void cartographer::remove_patrol_path(unsigned int obj)
{
    for (int i = 0; i < m_patrol_routes.size(); i++)
    {
        if (m_patrol_routes[i].handle == obj)
        {
            m_patrol_routes.remove(i);
            return;
        }
    }
}

// 0x00870880 (:5228)
void cartographer::process_patrol_routes()
{
    const bool is_full = m_map_mode == MAP_MODE_FULL;
    for (int i = 0; i < m_patrol_routes.size(); i++)
    {
        create_path_route_points(m_patrol_routes[i], is_full, is_full ? (1.0f - fsm_zoom_percent()) * Fsm_num_path_points_scalar + 1.0f : 1.0f);
    }
}

// The player's marker goes. Inlined in render and remove_all_paths.
void cartographer::clear_player_marker()
{
    if (m_has_player_marker)
    {
        audiolib_cue_play(Ga_cue_ids.map_gps_point_removed, nullptr, nullptr);
    }
    m_has_player_marker = false;
    m_player_marker_item.m_valid = false;
    m_player_marker_item.m_obj_handle = 0xFFFFFFFF;
    m_player_marker_item.m_act_handle = 0xFFFFFFFF;
    m_gps_path.pts.clear();
    Num_gps_refined_path_segments = 0;
    if (m_mission_gps && mission_is_active())
    {
        add_player_marker(m_mission_gps_pos, true, 0xFFFFFFFF, false, false);
    }
}

// 0x008802D0 (:5238)
void cartographer::remove_all_paths()
{
    m_patrol_routes.clear();
    m_gps_path.pts.clear();
    m_convoy_route.pts.clear();
    m_convoy_route.handle = 0xFFFFFFFF;
    remove_player_marker(true);
    clear_player_marker();
}

// 0x00861100 (:5277)
void cartographer::render_path_filler_pts(const vector2& curr_pt, const vector2& prev_pt, const vector& bmin, const vector& bmax, bmp_uv_information* bui, map_image_info* mii, bool is_full)
{
    const float zoom = Fs_info.zoom_level;
    // Magic numbers: filler arrows appear from zoom 0.17, two more from 0.38.
    if (bui == nullptr || (zoom <= 0.17f && is_full))
    {
        return;
    }
    const vector2 mid((curr_pt.x + prev_pt.x) * 0.5f, (prev_pt.y + curr_pt.y) * 0.5f);
    float alpha = 1.0f;
    if (is_full && 0.38f > zoom)
    {
        alpha = (zoom - 0.17f) / 0.21f;
    }
    render_bitmap_direct_oriented(bui, mid, 3.14154f - (float)atan2((double)(curr_pt.x - mid.x), (double)(curr_pt.y - mid.y)), bmin, bmax, mii, (unsigned char)(int)(alpha * 255.0f));
    if (0.38f < zoom || !is_full)
    {
        const vector2 forward((curr_pt.x + mid.x) * 0.5f, (curr_pt.y + mid.y) * 0.5f);
        const vector2 back((mid.x + prev_pt.x) * 0.5f, (prev_pt.y + mid.y) * 0.5f);
        if (is_full)
        {
            alpha = (zoom - 0.38f) / 0.42000002f;
        }
        const unsigned char filler_alpha = (unsigned char)(int)(alpha * 255.0f);
        render_bitmap_direct_oriented(bui, forward, 3.14154f - (float)atan2((double)(forward.x - mid.x), (double)(forward.y - mid.y)), bmin, bmax, mii, filler_alpha);
        render_bitmap_direct_oriented(bui, back, 3.14154f - (float)atan2((double)(back.x - prev_pt.x), (double)(back.y - prev_pt.y)), bmin, bmax, mii, filler_alpha);
    }
}

// 0x00861410 (:5329)
void cartographer::render_path_route(const farray<vector,384>& path_pt_list, bool is_full, bool dots, float zoom_scale, int last_line_index, bool edf_route, bool convoy_route)
{
    if (!m_valid_gps_path && !convoy_route)
    {
        return;
    }
    if (is_full && !fsm_is_up())
    {
        return;
    }
    int end_handle = -1;
    bmp_uv_information end_bui;
    if (!dots)
    {
        const int end_id = get_minimap_bitmap_id(MAP_ELEMENT_CONVOY_END_POINT);
        end_handle = vint_bms_get_bitmap(end_id);
        get_uv_information_bmpsheet(vint_bms_get_image_data(end_id), end_handle, end_bui.uvs[0], end_bui.uvs[1], end_bui.uvs[2], end_bui.uvs[3]);
    }
    map_image_info mii;
    bmp_uv_information bui;
    get_image_info(mii, bui, get_minimap_bitmap_id(MAP_ELEMENT_DIRECTIONAL_ARROW));
    const int count = path_pt_list.size();
    if (count <= 0)
    {
        return;
    }
    const color& base_color = convoy_route ? (edf_route ? EDF_route_color : RF_route_color) : GPS_color;
    const color& highlight_color = convoy_route ? (edf_route ? EDF_route_highlight_color : RF_route_highlight_color) : GPS_highlight_color;
    const int trail_index = (int)(convoy_route ? m_convoy_index : m_gps_index);
    bool draw_start = false;
    bool draw_end = false;
    for (int i = 0; i < count; i++)
    {
        const vector& pt = path_pt_list[i];
        color col(base_color);
        if (m_current_size_info.map_posn.x > pt.x + 45.0f || m_current_size_info.map_posn.y > pt.y + 45.0f || pt.x - 45.0f > m_current_size_info.map_end_posn.x || pt.y - 45.0f > m_current_size_info.map_end_posn.y)
        {
            continue;
        }
        if (i == 0)
        {
            draw_start = true;
            continue;
        }
        int next = count - 1;
        if (i == next)
        {
            gr_set_color(col);
            gr_set_texture(mii.bitmap_handle);
            draw_end = true;
            continue;
        }
        if (Gps_lookahead_points + i < next)
        {
            next = Gps_lookahead_points + i;
        }
        const vector2 start(pt.x, pt.y);
        const vector2 end(path_pt_list[next].x, path_pt_list[next].y);
        const float angle = get_directional_arrow_angle(start, end);
        const int section = ((int)pt.z + trail_index) % Distance_between_sections;
        if (Distance_between_sections - trail_size - 1 < section)
        {
            const float t = (float)(section - Distance_between_sections + 1 + trail_size) / (float)trail_size;
            col.red = (unsigned char)(int)(((float)highlight_color.red - (float)col.red) * t + (float)col.red);
            col.green = (unsigned char)(int)(((float)highlight_color.green - (float)col.green) * t + (float)col.green);
            col.blue = (unsigned char)(int)(((float)highlight_color.blue - (float)col.blue) * t + (float)col.blue);
            col.alpha = 0xFF;
        }
        if ((float)last_line_index <= pt.z)
        {
            col.alpha = (unsigned char)GPS_end_line_alpha;
        }
        if ((Minimap_gps_display & 1) != 0 || is_full)
        {
            gr_set_color(col);
            gr_set_texture(mii.bitmap_handle);
            const vector bmax(zoom_scale * 9.0f * Resolution_minimap_scale.y, zoom_scale * 0.0f * Resolution_minimap_scale.y, zoom_scale * 5.0f * Resolution_minimap_scale.y);
            const vector bmin(zoom_scale * -1.0f * Resolution_minimap_scale.x, zoom_scale * 0.0f * Resolution_minimap_scale.x, zoom_scale * -5.0f * Resolution_minimap_scale.x);
            render_bitmap_direct_oriented(&bui, start, angle, bmin, bmax, &mii, col.alpha);
        }
    }
    if ((draw_end || draw_start) && !dots)
    {
        gr_set_color(base_color);
        gr_set_texture(end_handle);
        rl_draw::get_instance()->tristrip_2d_begin(m_rl_render_state);
        const vector bmax(zoom_scale * Map_icon_pixel_size, zoom_scale * 0.0f, Map_icon_pixel_size * zoom_scale);
        const vector bmin(zoom_scale * -Map_icon_pixel_size, zoom_scale * 0.0f, -Map_icon_pixel_size * zoom_scale);
        if (draw_start)
        {
            render_bitmap_direct(&end_bui, vector2(path_pt_list[0].x, path_pt_list[0].y), bmin, bmax, nullptr, 0xFF);
        }
        if (draw_end)
        {
            render_bitmap_direct(&end_bui, vector2(path_pt_list[count - 1].x, path_pt_list[count - 1].y), bmin, bmax, nullptr, 0xFF);
        }
        rl_draw::get_instance()->tristrip_2d_end();
    }
}

// 0x00870940 (:5479)
void cartographer::render_convoy_routes(float zoom_scale, bool is_full)
{
    if (activity_convoy_player_in_vehicle())
    {
        return;
    }
    object* objp = m_convoy_route.handle != 0xFFFFFFFF ? World.object_from_handle(m_convoy_route.handle) : nullptr;
    if (objp == nullptr || objp->get_obj_type() != OT_CONVOY)
    {
        m_convoy_route.pts.clear();
        m_convoy_route.handle = 0xFFFFFFFF;
        return;
    }
    farray<vector,384> screen_pts;
    get_path_screen_pts(screen_pts, m_convoy_route, is_full, true);
    render_path_route(screen_pts, is_full, false, zoom_scale, 0x180, true, true);
}

// 0x00881700 (:5503)
void cartographer::render_patrol_routes(float zoom_scale, bool is_full)
{
    const int count = m_patrol_routes.size();
    farray<int,4> gone;
    for (int i = 0; i < count; i++)
    {
        const path_info& route = m_patrol_routes[i];
        object* objp = route.handle != 0xFFFFFFFF ? World.object_from_handle(route.handle) : nullptr;
        if (objp == nullptr || objp->get_obj_type() != OT_PATROL)
        {
            gone.add(i);
            continue;
        }
        farray<vector,384> screen_pts;
        get_path_screen_pts(screen_pts, route, is_full, true);
        render_path_route(screen_pts, is_full, false, zoom_scale, 0x180, route.edf_route, true);
    }
    // Original bug kept: each removal shifts the later indices, so a second removal in a frame takes the wrong route.
    for (int i = 0; i < gone.size(); i++)
    {
        m_patrol_routes.remove(gone[i]);
    }
}

// 0x00870A20 (:5533)
void cartographer::render_decals(bool is_full)
{
    for (int i = 0; i < m_decals.size(); i++)
    {
        map_decal& decal = m_decals[i];
        const vector pos(decal.pos.x, 0.0f, decal.pos.y);
        // Original quirk kept: the decal's bitmap (a bitmap id) is read as a map element here.
        const minimap_elements element = (minimap_elements)decal.bitmap;
        const bool known = (unsigned int)decal.bitmap < NUM_MAP_ELEMENTS;
        if (is_full)
        {
            if (!(known && minimap_element_visible_over_fow(element)) && Fog_of_war_render && fog_of_war_is_posn_obscured(pos))
            {
                continue;
            }
            if (!known || !minimap_element_visible_on_fs_map(element))
            {
                continue;
            }
        }
        const float scale_x = m_current_size_info.map_scale.x;
        const float scale_y = m_current_size_info.map_scale.y;
        const vector bmin(scale_x * -decal.scale.x, 0.0f, scale_y * -decal.scale.y);
        const vector bmax(scale_x * decal.scale.x, 0.0f, scale_y * decal.scale.y);
        map_image_info mii;
        bmp_uv_information bui;
        get_image_info(mii, bui, decal.bitmap);
        if (decal.inverted)
        {
            // An inverted decal: the shape in black with one raw state, then the whole map in the decal's color with another.
            rl_draw::get_instance()->set_material(0xFFFFFFFF);
            rl_draw::get_instance()->set_bink_textures(-1, -1, -1);
            // Magic numbers: the raw primitive states of rfg.exe.
            rl_primitive_state state;
            state.set_bits(((unsigned int)Minimap_scissor_enabled << 0x12) | 0x80000A00);
            rl_draw::get_instance()->set_texture(mii.bitmap_handle);
            rl_draw::get_instance()->tristrip_2d_begin(state);
            rl_draw::get_instance()->set_color(rl_color_float(0.0f, 0.0f, 0.0f, 1.0f));
            if (!decal.uses_orient)
            {
                render_bitmap(&bui, pos, bmin, bmax, nullptr);
            }
            else
            {
                render_bitmap_oriented(&bui, pos, decal.orient, bmin, bmax, nullptr, RUVS_0, false);
            }
            rl_draw::get_instance()->tristrip_2d_end();
            state.set_bits(((unsigned int)Minimap_scissor_enabled << 0x12) | (rl_primitive_state::FILTER.get_bits() & 0xFFFBE5FF) | 0x400);
            rl_draw::get_instance()->set_texture(-1);
            rl_draw::get_instance()->tristrip_2d_begin(state);
            rl_primitive_2d_vert* verts = rl_draw::get_instance()->tristrip_2d_allocate_tris(2);
            if (verts != nullptr)
            {
                const vector2 corners[4] = {vector2(m_mini_size_info.map_posn.x, m_mini_size_info.map_posn.y), vector2(m_mini_size_info.map_end_posn.x, m_mini_size_info.map_posn.y), vector2(m_mini_size_info.map_posn.x, m_mini_size_info.map_end_posn.y), vector2(m_mini_size_info.map_end_posn.x, m_mini_size_info.map_end_posn.y)};
                const vector2 uvs[4] = {vector2(0.0f, 0.0f), vector2(1.0f, 0.0f), vector2(0.0f, 1.0f), vector2(1.0f, 1.0f)};
                for (int v = 0; v < 4; v++)
                {
                    verts[v].pt.x = corners[v].x;
                    verts[v].pt.y = corners[v].y;
                    verts[v].color.red = (unsigned char)(int)(((float)decal.bitmap_color.red / 255.0f) * 255.0f + 0.5f);
                    verts[v].color.green = (unsigned char)(int)(((float)decal.bitmap_color.green / 255.0f) * 255.0f + 0.5f);
                    verts[v].color.blue = (unsigned char)(int)(((float)decal.bitmap_color.blue / 255.0f) * 255.0f + 0.5f);
                    verts[v].color.alpha = (unsigned char)(int)(((float)decal.bitmap_color.alpha / 255.0f) * 255.0f + 0.5f);
                    verts[v].uvs.x = uvs[v].x;
                    verts[v].uvs.y = uvs[v].y;
                }
            }
            rl_draw::get_instance()->tristrip_2d_end();
        }
        else
        {
            gr_set_color(decal.bitmap_color);
            gr_set_texture(mii.bitmap_handle);
            if (!decal.uses_orient)
            {
                render_bitmap(&bui, pos, bmin, bmax, &mii);
            }
            else
            {
                render_bitmap_oriented(&bui, pos, decal.orient, bmin, bmax, &mii, RUVS_0, false);
            }
        }
    }
}

// The car bomb's icon shows only while no bomb activity runs and the target isn't driving the bomb. Inlined three times.
static bool minimap_car_bomb_hidden(const player* target)
{
    if (activity_car_bomb_is_active() || activity_is_active(ACTIVITY_MODE_DELIVERY))
    {
        return true;
    }
    const unsigned int vehicle_handle = target->vehicle_handle;
    if (vehicle_handle == 0xFFFFFFFF)
    {
        return false;
    }
    object* objp = World.object_from_handle(vehicle_handle);
    // Magic numbers: sub type 4, an automobile; its field at 0x3D68 is 1 for the bomb car.
    return objp != nullptr && objp->get_obj_type() == OT_VEHICLE && objp->get_sub_type() == 4 && *(int*)((char*)objp + 0x3D68) == 1;
}

// 0x00870F80 (:5662)
void cartographer::render_temp_icons(float zoom_scale, bool is_full)
{
    static float timer; // 0x02BE5924
    timer = Frametime_real + timer;
    const float pulse = timer * 2.0f - (float)(int)(timer * 2.0f);
    const float pulse_scale = pulse * 0.75f + 1.0f;
    const unsigned int pulse_alpha = (unsigned int)(int)((1.0f - pulse) * 255.0f) & 0xFF;
    bool convoy_on_edge = false;
    for (int i = 0; i < m_temp_icons.size(); i++)
    {
        map_icon_temp& icon = m_temp_icons[i];
        const minimap_elements icon_bitmap = (minimap_elements)icon.bitmap;
        const bool known = (unsigned int)icon.bitmap < NUM_MAP_ELEMENTS;
        gr_set_color(icon.bitmap_color);
        vector draw_pos(icon.pos.x, 0.0f, icon.pos.y);
        if (icon_bitmap == MAP_ELEMENT_CARBOMB && minimap_car_bomb_hidden(get_target_player()))
        {
            continue;
        }
        if (is_full)
        {
            if (!(known && minimap_element_visible_over_fow(icon_bitmap)) && Fog_of_war_render && fog_of_war_is_posn_obscured(draw_pos) && !visible_over_fow_list.exists(icon.activity_handle))
            {
                continue;
            }
            if (!known || !minimap_element_visible_on_fs_map(icon_bitmap))
            {
                continue;
            }
        }
        const float dx = icon.pos.x - m_current_size_info.center_pos_world.x;
        const float dz = icon.pos.y - m_current_size_info.center_pos_world.z;
        const float dist_squared = dx * dx + dz * dz;
        if (!((icon.stick && m_map_mode == MAP_MODE_MINI) || dist_squared <= m_current_size_info.zoom_radius_squared))
        {
            continue;
        }
        check_highlight_for_item(draw_pos, icon.render_pos, icon.handle, icon.bitmap, IIT_TEMP_ICON, 0.0f, icon.bitmap_color, icon.activity_handle);
        if (is_full && m_player_marker_item.m_fake_pos.x == icon.pos.x && m_player_marker_item.m_fake_pos.y == icon.pos.y && m_player_marker_item.m_type == IIT_BLDG)
        {
            continue;
        }
        const float edge = m_current_size_info.zoom_radius * 0.9f;
        if (edge * edge < dist_squared)
        {
            // Only one convoy icon stays on the map's edge.
            if (icon_bitmap == MAP_ELEMENT_CONVOY_ICON)
            {
                if (convoy_on_edge)
                {
                    continue;
                }
                convoy_on_edge = true;
            }
            draw_pos.x = dx * (edge / sqrtf(dist_squared)) + m_current_size_info.center_pos_world.x;
            draw_pos.z = dz * (edge / sqrtf(dist_squared)) + m_current_size_info.center_pos_world.z;
        }
        bool pulsing = false;
        if (icon.stick || (known && minimap_element_is_objective_icon(icon_bitmap)))
        {
            if (!known || !minimap_element_is_activity_icon(icon_bitmap) || icon.blink)
            {
                pulsing = m_has_player_marker && m_player_marker_item.m_valid && ((m_player_marker_item.m_obj_handle != 0xFFFFFFFF && m_player_marker_item.m_obj_handle == icon.handle) || (m_player_marker_item.m_obj_pos.x == icon.pos.x && m_player_marker_item.m_obj_pos.y == 0.0f && m_player_marker_item.m_obj_pos.z == icon.pos.y));
            }
            else
            {
                pulsing = true;
            }
        }
        if (is_full && (is_old_highlight_item(draw_pos, icon.handle, icon.bitmap, IIT_TEMP_ICON) || !fullscreen_map_is_loaded()))
        {
            continue;
        }
        int alpha;
        if (icon.bmin.x == ZERO_VECTOR2.x && icon.bmin.y == ZERO_VECTOR2.y && icon.bmax.x == ZERO_VECTOR2.x && icon.bmax.y == ZERO_VECTOR2.y)
        {
            if (set_icon_zoomed_alpha(icon_bitmap, is_full, alpha, 0xFF))
            {
                continue;
            }
            if (icon.blink)
            {
                gr_set_alpha(pulse_alpha);
            }
            if (is_full)
            {
                render_single_icon_direct(icon.render_pos, Map_icon_pixel_size * zoom_scale, icon.bitmap, alpha, m_rl_render_state);
            }
            else
            {
                render_single_icon(draw_pos, Map_icon_pixel_size * zoom_scale, icon.bitmap, m_rl_render_state);
            }
            if (pulsing && !icon.blink)
            {
                color col(icon.bitmap_color);
                col.alpha = (unsigned char)pulse_alpha;
                gr_set_color(col);
                set_icon_zoomed_alpha(icon_bitmap, is_full, alpha, pulse_alpha);
                const float size = Map_icon_pixel_size * pulse_scale * zoom_scale;
                if (is_full)
                {
                    render_single_icon_direct(icon.render_pos, size, icon.bitmap, alpha, m_rl_render_state);
                }
                else
                {
                    render_single_icon(draw_pos, size, icon.bitmap, m_rl_render_state);
                }
            }
        }
        else
        {
            alpha = 0xFF;
            if (icon.blink)
            {
                alpha = pulse_alpha;
                gr_set_alpha(alpha);
            }
            const vector2 bmin(icon.bmin.x * zoom_scale, icon.bmin.y * zoom_scale);
            const vector2 bmax(icon.bmax.x * zoom_scale, icon.bmax.y * zoom_scale);
            if (is_full)
            {
                render_single_icon_direct(icon.render_pos, bmin, bmax, icon.bitmap, alpha, m_rl_render_state);
            }
            else
            {
                render_single_icon(draw_pos, bmin, bmax, icon.bitmap, m_rl_render_state);
            }
            if (pulsing && !icon.blink)
            {
                color col(icon.bitmap_color);
                col.alpha = (unsigned char)pulse_alpha;
                gr_set_color(col);
                set_icon_zoomed_alpha(icon_bitmap, is_full, alpha, pulse_alpha);
                const vector2 pulse_min(icon.bmin.x * pulse_scale * zoom_scale, icon.bmin.y * pulse_scale * zoom_scale);
                const vector2 pulse_max(icon.bmax.x * pulse_scale * zoom_scale, icon.bmax.y * pulse_scale * zoom_scale);
                if (is_full)
                {
                    render_single_icon_direct(icon.render_pos, pulse_min, pulse_max, icon.bitmap, alpha, m_rl_render_state);
                }
                else
                {
                    render_single_icon(draw_pos, pulse_min, pulse_max, icon.bitmap, m_rl_render_state);
                }
            }
        }
    }
}

// 0x0084CFE0 (:5813)
static bool should_remove_obj_icon(const object* obj)
{
    return obj == nullptr || obj->is_killed();
}

// The human's vehicle stands in for a human driving. Inlined at each icon list.
static object* icon_object_or_vehicle(unsigned int handle, object* objp)
{
    if (handle != 0xFFFFFFFF)
    {
        object* icon_obj = World.object_from_handle(handle);
        if (icon_obj != nullptr && icon_obj->get_obj_type() == OT_HUMAN && ((human*)icon_obj)->vehicle_handle != 0xFFFFFFFF)
        {
            object* vp = World.object_from_handle(((human*)icon_obj)->vehicle_handle);
            if (vp != nullptr)
            {
                return vp;
            }
        }
    }
    return objp;
}

// 0x00877C20 (:5827)
void cartographer::render_icons(float zoom_scale, bool is_full)
{
    farray<unsigned int,32> dead;
    static const vector2 zero_vec(ZERO_VECTOR2); // 0x02BE5938
    for (int i = 0; i < m_icons.size(); i++)
    {
        map_icon& icon = m_icons[i];
        minimap_elements icon_bitmap = (minimap_elements)icon.bitmap;
        object* objp = World.object_from_handle(icon.handle);
        if (icon_bitmap == MAP_ELEMENT_MISSION_START && mission_is_active())
        {
            continue;
        }
        if (should_remove_obj_icon(objp))
        {
            dead.add(icon.handle);
            continue;
        }
        if (icon_bitmap == MAP_ELEMENT_CARBOMB && minimap_car_bomb_hidden(get_target_player()))
        {
            continue;
        }
        adjust_objective_height(icon_bitmap, objp->pos.y, get_target_player());
        objp = icon_object_or_vehicle(icon.handle, objp);
        const vector pos(objp->pos.x, 0.0f, objp->pos.z);
        // Icons still at the world's origin are not placed yet.
        if (vec2_dist_squared(vector2(pos.x, pos.z), zero_vec) <= 0.01f)
        {
            continue;
        }
        if (is_full)
        {
            const bool over_fow = icon_bitmap < NUM_MAP_ELEMENTS && minimap_element_visible_over_fow(icon_bitmap);
            if (!over_fow && Fog_of_war_render && fog_of_war_is_posn_obscured(pos))
            {
                continue;
            }
            if ((icon_bitmap > MAP_ELEMENT_CAMERA_CONE || !minimap_element_visible_on_fs_map(icon_bitmap)) && (objp->get_obj_type() != OT_SAFEHOUSE || !((object_safehouse*)objp)->is_visible()))
            {
                continue;
            }
        }
        const float dx = m_current_size_info.center_pos_world.x - pos.x;
        const float dy = m_current_size_info.center_pos_world.y - pos.y;
        const float dz = m_current_size_info.center_pos_world.z - pos.z;
        if (dz * dz + dy * dy + dx * dx > m_current_size_info.zoom_radius_squared)
        {
            continue;
        }
        check_highlight_for_item(pos, icon.render_pos, icon.handle, icon_bitmap, IIT_ICON, 0.0f, Default_map_item_color, 0xFFFFFFFF);
        gr_set_color(Default_map_item_color);
        int alpha;
        if (set_icon_zoomed_alpha(icon_bitmap, is_full, alpha, 0xFF))
        {
            continue;
        }
        if (is_full)
        {
            // Original quirk kept: the stored element, not the one the height changed, is tested.
            if (!is_old_highlight_item(pos, icon.handle, icon.bitmap, IIT_ICON) && fullscreen_map_is_loaded())
            {
                render_single_icon_direct(icon.render_pos, Map_icon_pixel_size * zoom_scale, icon_bitmap, alpha, m_rl_render_state);
            }
        }
        else
        {
            render_single_icon(pos, Map_icon_pixel_size * zoom_scale, icon_bitmap, m_rl_render_state);
        }
    }
    for (int i = 0; i < dead.size(); i++)
    {
        map_icon test(dead[i]);
        m_icons.remove_matching(test);
        m_sticky_icons.remove_matching(test);
    }
}

// The sticky icon's pulse, for objectives, targets and the marked item.
bool cartographer::sticky_icon_pulses(const map_icon& icon, unsigned int icon_bitmap, const object* objp) const
{
    if (icon_bitmap < NUM_MAP_ELEMENTS && (minimap_element_is_objective_icon((minimap_elements)icon_bitmap) || minimap_element_is_target_icon((minimap_elements)icon_bitmap)))
    {
        return true;
    }
    return m_has_player_marker && m_player_marker_item.m_valid && ((m_player_marker_item.m_obj_handle != 0xFFFFFFFF && m_player_marker_item.m_obj_handle == icon.handle) || (m_player_marker_item.m_obj_pos.x == objp->pos.x && m_player_marker_item.m_obj_pos.y == objp->pos.y && m_player_marker_item.m_obj_pos.z == objp->pos.z));
}

// 0x00878180 (:5953)
void cartographer::render_sticky_icons(float zoom_scale, bool is_full)
{
    // Magic number: 10, the size of rfg.exe's closest lists; Minimap_render_closest_num is 3.
    int closest_index[10];
    float closest_dist[10];
    float closest_dx[10];
    float closest_dz[10];
    int num_closest = 0;
    farray<unsigned int,48> dead;
    farray<int,48> objectives;
    static const vector2 zero_vec(ZERO_VECTOR2); // 0x02BE5944
    const float edge = m_current_size_info.zoom_radius * 0.9f;
    const float edge_squared = edge * edge;
    player* target = get_target_player();
    const int count = m_sticky_icons.size();
    for (int i = 0; i < count; i++)
    {
        map_icon& icon = m_sticky_icons[i];
        const minimap_elements icon_bitmap = (minimap_elements)icon.bitmap;
        object* objp = World.object_from_handle(icon.handle);
        if (icon_bitmap == MAP_ELEMENT_MISSION_START && mission_is_active())
        {
            continue;
        }
        if (!is_full && icon_bitmap == MAP_ELEMENT_MISSION_START)
        {
            // Magic number: mission starts show on the minimap within 250 meters.
            if ((!m_player_marker_item.m_valid || m_player_marker_item.m_obj_handle != icon.handle) && target != nullptr && objp != nullptr)
            {
                if (62500.0f <= vec2_dist_squared(vector2(target->pos.x, target->pos.z), vector2(objp->pos.x, objp->pos.z)))
                {
                    continue;
                }
            }
        }
        else if (icon_bitmap == MAP_ELEMENT_CARBOMB && minimap_car_bomb_hidden(get_target_player()))
        {
            continue;
        }
        if (is_full)
        {
            if (icon_bitmap > MAP_ELEMENT_CAMERA_CONE || !minimap_element_visible_on_fs_map(icon_bitmap))
            {
                continue;
            }
        }
        else if (edge_squared < 10000.0f && icon_bitmap == MAP_ELEMENT_AMMO_BOX)
        {
            // Magic number: ammo boxes show within 100 meters, when the player can use them.
            if (10000.0f <= vec2_dist_squared(vector2(target->pos.x, target->pos.z), vector2(objp->pos.x, objp->pos.z)) || !ammo_box_is_usable_by_player((item*)objp, nullptr))
            {
                continue;
            }
        }
        else if (icon_bitmap == MAP_ELEMENT_SAFE_HOUSE && (icon.handle != m_nearest_safehouse || mission_is_active()))
        {
            continue;
        }
        map_icon test(icon.handle);
        const bool also_in_icons = m_icons.find(test, false) != -1;
        if (objp == nullptr || objp->is_killed() || (m_player_marker_item.m_obj_handle == icon.handle && !m_player_marker_item.m_valid && also_in_icons))
        {
            dead.add(icon.handle);
            continue;
        }
        objp = icon_object_or_vehicle(icon.handle, objp);
        vector pos(objp->pos.x, 0.0f, objp->pos.z);
        if (vec2_dist_squared(vector2(pos.x, pos.z), zero_vec) <= 0.01f)
        {
            continue;
        }
        const float dx = pos.x - m_current_size_info.center_pos_world.x;
        const float dz = pos.z - m_current_size_info.center_pos_world.z;
        const float dist_squared = dx * dx + dz * dz;
        if (m_map_mode == MAP_MODE_FULL && edge_squared < dist_squared)
        {
            continue;
        }
        if (!is_full && icon.only_show_closest)
        {
            if (num_closest < Minimap_render_closest_num)
            {
                closest_index[num_closest] = i;
                closest_dist[num_closest] = dist_squared;
                closest_dx[num_closest] = dx;
                closest_dz[num_closest] = dz;
                num_closest++;
            }
            else
            {
                int farthest = 0;
                for (int c = 1; c < num_closest; c++)
                {
                    if (closest_dist[farthest] < closest_dist[c])
                    {
                        farthest = c;
                    }
                }
                if (dist_squared < closest_dist[farthest])
                {
                    closest_index[farthest] = i;
                    closest_dist[farthest] = dist_squared;
                    closest_dx[farthest] = dx;
                    closest_dz[farthest] = dz;
                }
            }
            continue;
        }
        if (icon_bitmap == MAP_ELEMENT_MISSION_OBJECTIVE)
        {
            objectives.add(i);
            continue;
        }
        if (!icon_is_visible_over_fow(icon_bitmap, is_full, true, pos) && (objp->get_obj_type() != OT_SAFEHOUSE || !((object_safehouse*)objp)->is_visible()))
        {
            continue;
        }
        // Magic number: nothing past 1000 meters on the minimap, except the safe houses.
        if (m_map_mode == MAP_MODE_MINI && 1e+06f < dist_squared && icon_bitmap != MAP_ELEMENT_SAFE_HOUSE)
        {
            continue;
        }
        gr_set_color(Default_map_item_color);
        int alpha;
        if (set_icon_zoomed_alpha(icon_bitmap, is_full, alpha, 0xFF))
        {
            continue;
        }
        check_highlight_for_item(pos, icon.render_pos, icon.handle, icon_bitmap, IIT_STICKY, 0.0f, Default_map_item_color, 0xFFFFFFFF);
        if (is_full)
        {
            if (is_old_highlight_item(pos, icon.handle, icon_bitmap, IIT_STICKY) || !fullscreen_map_is_loaded())
            {
                continue;
            }
            render_single_icon_direct(icon.render_pos, Map_icon_pixel_size * zoom_scale, icon_bitmap, alpha, m_rl_render_state);
        }
        else
        {
            if (edge_squared < dist_squared)
            {
                pos.x = dx * (edge / sqrtf(dist_squared)) + m_current_size_info.center_pos_world.x;
                pos.z = dz * (edge / sqrtf(dist_squared)) + m_current_size_info.center_pos_world.z;
            }
            render_single_icon(pos, Map_icon_pixel_size * zoom_scale, icon_bitmap, m_rl_render_state);
        }
        if (minimap_element_is_objective_icon(icon_bitmap) || minimap_element_is_target_icon(icon_bitmap) || is_player_marker_item(icon.handle, objp->pos))
        {
            float pulse = (float)icon.time_added.time_since() * 0.001f * 2.0f;
            pulse = pulse - (float)(int)pulse;
            const float size = (pulse * 0.75f + 1.0f) * zoom_scale;
            color col(Default_map_item_color);
            col.alpha = (unsigned char)(int)((1.0f - pulse) * 255.0f);
            gr_set_color(col);
            if (is_full)
            {
                render_single_icon_direct(icon.render_pos, size * Map_icon_pixel_size, icon_bitmap, col.alpha, m_rl_render_state);
            }
            else
            {
                render_single_icon(pos, size * Map_icon_pixel_size, icon_bitmap, m_rl_render_state);
            }
        }
    }
    if (!is_full)
    {
        for (int c = 0; c < num_closest; c++)
        {
            map_icon& icon = m_sticky_icons[closest_index[c]];
            minimap_elements icon_bitmap = (minimap_elements)icon.bitmap;
            object* objp = icon_object_or_vehicle(icon.handle, World.object_from_handle(icon.handle));
            vector pos(0.0f, 0.0f, 0.0f);
            if (objp != nullptr)
            {
                adjust_objective_height(icon_bitmap, objp->pos.y, get_target_player());
                pos.x = objp->pos.x;
                pos.z = objp->pos.z;
            }
            gr_set_color(Default_map_item_color);
            check_highlight_for_item(pos, icon.render_pos, icon.handle, icon_bitmap, IIT_STICKY, 0.0f, Default_map_item_color, 0xFFFFFFFF);
            if (edge_squared < closest_dist[c])
            {
                const float scale = edge / sqrtf(closest_dist[c]);
                pos.x = closest_dx[c] * scale + m_current_size_info.center_pos_world.x;
                pos.z = closest_dz[c] * scale + m_current_size_info.center_pos_world.z;
            }
            // Original quirk kept: the closest icons don't scale with the zoom, the pulse's part does.
            render_single_icon(pos, Map_icon_pixel_size, icon_bitmap, m_rl_render_state);
            if (sticky_icon_pulses(icon, icon_bitmap, objp))
            {
                float pulse = (float)icon.time_added.time_since() * 0.001f * 2.0f;
                pulse = pulse - (float)(int)pulse;
                const float size = pulse * 0.75f * zoom_scale + 1.0f;
                color col(Default_map_item_color);
                col.alpha = (unsigned char)(int)((1.0f - pulse) * 255.0f);
                gr_set_color(col);
                render_single_icon(pos, size * Map_icon_pixel_size, icon_bitmap, m_rl_render_state);
            }
        }
    }
    // The mission objectives draw over the rest, with their height arrows.
    for (int o = 0; o < objectives.size(); o++)
    {
        map_icon& icon = m_sticky_icons[objectives[o]];
        minimap_elements icon_bitmap = (minimap_elements)icon.bitmap;
        object* objp = icon_object_or_vehicle(icon.handle, World.object_from_handle(icon.handle));
        vector pos(0.0f, 0.0f, 0.0f);
        if (objp != nullptr)
        {
            pos.x = objp->pos.x;
            pos.z = objp->pos.z;
            adjust_objective_height(icon_bitmap, objp->pos.y, get_target_player());
        }
        const float dx = pos.x - m_current_size_info.center_pos_world.x;
        const float dz = pos.z - m_current_size_info.center_pos_world.z;
        const float dist_squared = dx * dx + dz * dz;
        gr_set_color(Default_map_item_color);
        int alpha = 0xFF;
        if (is_full && (icon_bitmap > MAP_ELEMENT_CAMERA_CONE || !minimap_element_is_visible_when_zoomed(icon_bitmap)))
        {
            float percent = fsm_zoom_percent();
            if (percent <= Fsm_max_visible_dist)
            {
                continue;
            }
            if (percent <= Fsm_min_visible_dist)
            {
                percent = (percent - Fsm_max_visible_dist) / (Fsm_min_visible_dist - Fsm_max_visible_dist);
                if (0.0f <= 0.0f - percent)
                {
                    percent = 0.0f;
                }
                alpha = (int)(percent * 255.0f);
                gr_set_alpha(alpha);
            }
        }
        check_highlight_for_item(pos, icon.render_pos, icon.handle, icon_bitmap, IIT_STICKY, 0.0f, Default_map_item_color, 0xFFFFFFFF);
        if (is_full)
        {
            if (is_old_highlight_item(pos, icon.handle, icon_bitmap, IIT_STICKY) || !fullscreen_map_is_loaded())
            {
                continue;
            }
            render_single_icon_direct(icon.render_pos, Map_icon_pixel_size * zoom_scale, icon_bitmap, alpha, m_rl_render_state);
        }
        else
        {
            if (edge_squared < dist_squared)
            {
                pos.x = (edge / sqrtf(dist_squared)) * dx + m_current_size_info.center_pos_world.x;
                pos.z = dz * (edge / sqrtf(dist_squared)) + m_current_size_info.center_pos_world.z;
            }
            render_single_icon(pos, Map_icon_pixel_size * zoom_scale, icon_bitmap, m_rl_render_state);
        }
        if (sticky_icon_pulses(icon, icon_bitmap, objp))
        {
            float pulse = (float)icon.time_added.time_since() * 0.001f * 2.0f;
            pulse = pulse - (float)(int)pulse;
            const float size = (pulse * 0.75f + 1.0f) * zoom_scale;
            color col(Default_map_item_color);
            col.alpha = (unsigned char)(int)((1.0f - pulse) * 255.0f);
            gr_set_color(col);
            if (is_full)
            {
                render_single_icon_direct(icon.render_pos, size * Map_icon_pixel_size, icon_bitmap, col.alpha, m_rl_render_state);
            }
            else
            {
                render_single_icon(pos, size * Map_icon_pixel_size, icon_bitmap, m_rl_render_state);
            }
        }
    }
    for (int i = 0; i < dead.size(); i++)
    {
        map_icon test(dead[i]);
        const bool in_icons = m_icons.find(test, false) != -1;
        if (m_player_marker_item.m_obj_handle == dead[i])
        {
            if (in_icons)
            {
                m_sticky_icons.remove_matching(test);
            }
        }
        else
        {
            m_icons.remove_matching(test);
            m_sticky_icons.remove_matching(test);
        }
    }
}

// The element's bitmap id, or the bogus bitmap past the table. Inlined at each render_single_icon.
static int minimap_icon_bitmap_id(int icon_index)
{
    return (unsigned int)icon_index < NUM_MAP_ELEMENTS ? get_minimap_bitmap_id(icon_index) : bm_get_bogus_bitmap_handle();
}

// 0x00861C80 (:6376)
void cartographer::render_single_icon(const vector& pos, const vector2& in_bmin, const vector2& in_bmax, int icon_index, rl_primitive_state& rl_state)
{
    map_image_info mii;
    bmp_uv_information bui;
    get_image_info(mii, bui, minimap_icon_bitmap_id(icon_index));
    gr_set_texture(mii.bitmap_handle);
    render_bitmap(&bui, pos, vector(in_bmin.x, 0.0f, in_bmin.y), vector(in_bmax.x, 0.0f, in_bmax.y), &mii);
}

// 0x00861D70 (:6390)
void cartographer::render_single_icon(const vector& pos, float icon_size, int icon_index, rl_primitive_state& rl_state)
{
    map_image_info mii;
    bmp_uv_information bui;
    get_image_info(mii, bui, minimap_icon_bitmap_id(icon_index));
    gr_set_texture(mii.bitmap_handle);
    render_bitmap(&bui, pos, vector(-icon_size, 0.0f, -icon_size), vector(icon_size, 0.0f, icon_size), &mii);
}

// 0x00861E50 (:6405)
void cartographer::render_single_icon_direct(const vector2& pos, const vector2& in_bmin, const vector2& in_bmax, int icon_index, int alpha, rl_primitive_state& rl_state)
{
    map_image_info mii;
    bmp_uv_information bui;
    get_image_info(mii, bui, minimap_icon_bitmap_id(icon_index));
    gr_set_texture(mii.bitmap_handle);
    render_bitmap_direct(&bui, pos, vector(in_bmin.x, 0.0f, in_bmin.y), vector(in_bmax.x, 0.0f, in_bmax.y), &mii, (unsigned char)alpha);
}

// 0x00861F40 (:6419)
void cartographer::render_single_icon_direct(const vector2& pos, float icon_size, int icon_index, int alpha, rl_primitive_state& rl_state)
{
    map_image_info mii;
    bmp_uv_information bui;
    get_image_info(mii, bui, minimap_icon_bitmap_id(icon_index));
    gr_set_texture(mii.bitmap_handle);
    render_bitmap_direct(&bui, pos, vector(-icon_size, 0.0f, -icon_size), vector(icon_size, 0.0f, icon_size), &mii, (unsigned char)alpha);
}

// 0x00871A60 (:6434)
void cartographer::get_mission_handles(farray<int,64>& handles)
{
    handles.clear();
    for (int i = 0; i < m_sticky_icons.size(); i++)
    {
        if (m_sticky_icons[i].bitmap == MAP_ELEMENT_MISSION_START)
        {
            handles.add((int)m_sticky_icons[i].handle);
        }
    }
    for (int i = 0; i < m_icons.size(); i++)
    {
        if (m_icons[i].bitmap == MAP_ELEMENT_MISSION_START)
        {
            handles.add((int)m_icons[i].handle);
        }
    }
}

// 0x0084D000 (:6457)
bool cartographer::has_player_marker()
{
    if (m_mission_gps && m_player_marker_pos.x == m_mission_gps_pos.x && m_player_marker_pos.y == m_mission_gps_pos.y && m_player_marker_pos.z == m_mission_gps_pos.z)
    {
        return false;
    }
    return m_has_player_marker;
}

// 0x0084D080 (:6466)
bool cartographer::player_marker_is_highlight_item()
{
    return m_has_player_marker && m_highlight.m_valid && m_player_marker_item.m_obj_pos.x == m_highlight.m_obj_pos.x && m_player_marker_item.m_obj_pos.y == m_highlight.m_obj_pos.y && m_player_marker_item.m_obj_pos.z == m_highlight.m_obj_pos.z;
}

// 0x0084D100 (:6471)
bool cartographer::has_mission_marker()
{
    return m_has_player_marker && m_mission_gps && m_player_marker_pos.x == m_mission_gps_pos.x && m_player_marker_pos.y == m_mission_gps_pos.y && m_player_marker_pos.z == m_mission_gps_pos.z;
}

// 0x0087DFB0 (:6477)
void cartographer::add_player_marker(const vector& pos, bool mission, unsigned int act_handle, bool player_placed, bool replace_on_add)
{
    if (player_placed)
    {
        audiolib_cue_play(Ga_cue_ids.map_gps_point_placed, nullptr, nullptr);
    }
    if (!m_highlight.m_valid)
    {
        map_icon_temp test(pos, act_handle);
        const int index = m_temp_icons.find(test, false);
        if (index < 0)
        {
            m_player_marker_item.m_valid = false;
        }
        else
        {
            const map_icon_temp& icon = m_temp_icons[index];
            m_player_marker_item.m_valid = true;
            m_player_marker_item.m_dsqr = 0.0f;
            m_player_marker_item.m_type = IIT_TEMP_ICON;
            m_player_marker_item.m_obj_pos.x = icon.pos.x;
            m_player_marker_item.m_obj_pos.y = 0.0f;
            m_player_marker_item.m_obj_pos.z = icon.pos.y;
            m_player_marker_item.m_icon_index = icon.bitmap;
            m_player_marker_item.m_obj_handle = icon.handle;
            m_player_marker_item.m_act_handle = icon.activity_handle;
            m_player_marker_item.m_angle_rad = 0.0f;
            m_player_marker_item.m_col = color(0, 0, 0, 0);
            m_player_marker_item.m_render_pos = icon.render_pos;
            // The render position back to the world (get_world_posn inlined).
            vector fake_pos;
            get_world_posn(m_player_marker_item.m_render_pos, fake_pos);
            m_player_marker_item.m_fake_pos.y = 0.0f;
            m_player_marker_item.m_fake_pos.x = fake_pos.x;
            m_player_marker_item.m_fake_pos.z = fake_pos.z;
        }
    }
    else
    {
        m_player_marker_item.set(m_highlight);
        if (does_icon_exist(m_highlight.m_obj_handle) && m_player_marker_item.m_type != IIT_BLDG)
        {
            // The marked icon stays on the map's edge.
            if (World.object_from_handle(m_player_marker_item.m_obj_handle) != nullptr && m_sticky_icons.size() < m_sticky_icons.capacity())
            {
                map_icon sticky(m_player_marker_item.m_obj_handle, m_player_marker_item.m_icon_index);
                sticky.time_added.set(0);
                sticky.only_show_closest = false;
                sticky.sticky = true;
                m_sticky_icons.add(sticky);
            }
        }
    }
    m_has_player_marker = true;
    m_mission_gps = mission || m_mission_gps;
    m_valid_gps_path = false;
    Num_gps_refined_path_segments = 0;
    m_player_marker_pos = pos;
    m_gps_path.pts.clear();
    m_replace_gps_on_add = replace_on_add;
    if (mission)
    {
        m_mission_gps_pos = pos;
    }
    m_gps_timer = Minimap_gps_delay;
}

// 0x0087E410 (:6531)
void cartographer::remove_player_marker(bool mission)
{
    bool remove = true;
    if (mission)
    {
        m_mission_gps = false;
        const bool item_on_mission = m_player_marker_item.m_obj_pos.x == m_mission_gps_pos.x && m_player_marker_item.m_obj_pos.y == m_mission_gps_pos.y && m_player_marker_item.m_obj_pos.z == m_mission_gps_pos.z;
        const bool pos_on_mission = m_player_marker_pos.x == m_mission_gps_pos.x && m_player_marker_pos.y == m_mission_gps_pos.y && m_player_marker_pos.z == m_mission_gps_pos.z;
        remove = item_on_mission || pos_on_mission;
    }
    if (remove)
    {
        if (m_has_player_marker && !mission)
        {
            audiolib_cue_play(Ga_cue_ids.map_gps_point_removed, nullptr, nullptr);
        }
        m_has_player_marker = false;
        m_player_marker_item.m_valid = false;
        m_player_marker_item.m_obj_handle = 0xFFFFFFFF;
        m_player_marker_item.m_act_handle = 0xFFFFFFFF;
        m_gps_path.pts.clear();
        Num_gps_refined_path_segments = 0;
    }
    if (m_mission_gps && !mission && mission_is_active())
    {
        add_player_marker(m_mission_gps_pos, true, 0xFFFFFFFF, false, false);
    }
}

// 0x00862020 (:6560)
void cartographer::render_north()
{
    gr_set_color(Default_map_item_color);
    if (m_map_mode != MAP_MODE_MINI)
    {
        return;
    }
    map_image_info mii;
    bmp_uv_information bui;
    get_image_info(mii, bui, get_minimap_bitmap_id(MAP_ELEMENT_NORTH));
    gr_set_texture(mii.bitmap_handle);
    const vector bmin(-North_icon_pixel_size, 0.0f, -North_icon_pixel_size);
    const vector bmax(North_icon_pixel_size, 0.0f, North_icon_pixel_size);
    float heading = 0.0f;
    if (!Minimap_rot0)
    {
        matrix orient(Rfg_camera.real_orient);
        heading = orient.extract_heading();
    }
    // Magic number: degrees per radian.
    heading = heading * 57.29583f;
    if (heading < 0.0f)
    {
        heading = heading + 360.0f;
    }
    render_bitmap_direct(&bui, get_point_along_overlay(heading), bmin, bmax, &mii, 0xFF);
}

// 0x00871B30 (:6591)
void cartographer::render_player_marker(float zoom_scale, bool is_full)
{
    vector player_pos(Rfg_camera.real_pos);
    player* pp = get_target_player();
    if (pp != nullptr)
    {
        player_pos = pp->pos;
    }
    vector closest_point;
    float closest_dist;
    const object_path_road* road = object_road_get_closest_road(player_pos, true, closest_point, closest_dist, Min_GPS_dist, true, nullptr, nullptr, false);
    const bool on_mission_pos = m_mission_gps && m_player_marker_pos.x == m_mission_gps_pos.x && m_player_marker_pos.y == m_mission_gps_pos.y && m_player_marker_pos.z == m_mission_gps_pos.z;
    const bool shown = (!on_mission_pos && m_has_player_marker) || (m_has_player_marker && on_mission_pos && road == nullptr);
    if (!shown || (m_player_marker_item.m_valid && road != nullptr))
    {
        return;
    }
    static float player_flash_timer; // 0x02BE592C
    static float pulse_timer; // 0x02BE5928
    static int sign = 1; // 0x01662BCC
    player_flash_timer = (float)sign * Frametime_real + player_flash_timer;
    pulse_timer = pulse_timer + Frametime_real;
    if (1.0f < pulse_timer)
    {
        pulse_timer = 0.0f;
    }
    if (0.5f < player_flash_timer)
    {
        player_flash_timer = 0.5f;
        sign = -1;
    }
    else if (player_flash_timer < 0.0f)
    {
        player_flash_timer = 0.0f;
        sign = 1;
    }
    const float fade = 1.0f - pulse_timer;
    gr_set_color(0xFF, 0xFF, 0xFF, 0xFF);
    vector pos(road != nullptr || !m_valid_gps_path ? m_player_marker_pos : m_first_gps_point);
    check_highlight_for_item(pos, world_to_window(pos.x, pos.z), 0, MAP_ELEMENT_CONVOY_END_POINT, IIT_PLAYER_MARKER, 0.0f, Default_map_item_color, 0xFFFFFFFF);
    if (is_full && (is_old_highlight_item(pos, 0, MAP_ELEMENT_CONVOY_END_POINT, IIT_PLAYER_MARKER) || !fsm_is_up()))
    {
        return;
    }
    map_image_info mii;
    bmp_uv_information bui;
    get_image_info(mii, bui, get_minimap_bitmap_id(MAP_ELEMENT_CONVOY_END_POINT));
    const float grow = pulse_timer + 1.0f;
    const float dx = pos.x - m_current_size_info.center_pos_world.x;
    const float dz = pos.z - m_current_size_info.center_pos_world.z;
    const float dist_squared = dx * dx + dz * dz;
    if (m_current_size_info.zoom_radius_squared < dist_squared)
    {
        if (is_full)
        {
            return;
        }
        const float scale = (m_current_size_info.zoom_radius * 0.9f) / sqrtf(dist_squared);
        pos.x = scale * dx + m_current_size_info.center_pos_world.x;
        pos.z = scale * dz + m_current_size_info.center_pos_world.z;
    }
    gr_set_color(GPS_highlight_color);
    gr_set_texture(mii.bitmap_handle);
    render_bitmap(&bui, pos, vector(zoom_scale * -Map_icon_pixel_size, zoom_scale * 0.0f, -Map_icon_pixel_size * zoom_scale), vector(zoom_scale * Map_icon_pixel_size, zoom_scale * 0.0f, Map_icon_pixel_size * zoom_scale), &mii);
    gr_set_alpha((int)(fade * 255.0f));
    const float size = Map_icon_pixel_size * grow;
    render_bitmap(&bui, pos, vector(zoom_scale * -size, grow * 0.0f * zoom_scale, -size * zoom_scale), vector(zoom_scale * size, grow * 0.0f * zoom_scale, size * zoom_scale), &mii);
}

// 0x008723A0 (:6669)
void cartographer::render_player(bool is_full)
{
    const vector2 scale = vint_project_get_scale(VINT_RES_SCALE_MAINTAIN_RATIO, VINT_RES_SCALE_MAINTAIN_RATIO);
    static float time; // 0x02BE5930
    time = Frametime_real + time;
    // Magic numbers: the player's icon breathes on the full map.
    float icon_size = Player_scale * 4.0f * scale.y;
    if (is_full)
    {
        icon_size = Player_scale * ((1.0f - (float)cos((double)(time * 5.0f)) * 0.125f) * 2.5f);
    }
    const float cone_size = Player_scale * 7.0f * scale.y;
    vector pos(Rfg_camera.real_pos);
    player* pp = get_target_player();
    if (pp != nullptr)
    {
        pos = pp->pos;
    }
    pos.y = 0.0f;
    const float dx = m_current_size_info.center_pos_world.x - pos.x;
    const float dy = m_current_size_info.center_pos_world.y - 0.0f;
    const float dz = m_current_size_info.center_pos_world.z - pos.z;
    if (dz * dz + dy * dy + dx * dx > m_current_size_info.zoom_radius_squared)
    {
        return;
    }
    if (!is_full || fsm_is_up())
    {
        vector bmin(-cone_size, 0.0f, 0.0f);
        vector bmax(cone_size, 0.0f, cone_size * 2.0f);
        map_image_info mii;
        bmp_uv_information bui;
        get_image_info(mii, bui, get_minimap_bitmap_id(MAP_ELEMENT_CAMERA_CONE));
        gr_set_color(color(0xFF, 0xD7, 0x00, 0xFF));
        gr_set_texture(mii.bitmap_handle);
        if (Minimap_rot0 || is_full)
        {
            matrix orient(Rfg_camera.real_orient);
            render_bitmap_oriented(&bui, pos, orient, bmin, bmax, &mii, RUVS_180, true);
        }
        else
        {
            bmin.z = -bmax.z;
            bmax.z = 0.0f;
            render_bitmap(&bui, pos, bmin, bmax, &mii);
        }
    }
    const color green(0x00, 0xFF, 0x00, 0xFF);
    const vector bmin(-icon_size, 0.0f, -icon_size);
    const vector bmax(icon_size, 0.0f, icon_size);
    const matrix orient = get_player_orientation(is_full);
    matrix heading_orient(orient);
    const float heading = heading_orient.extract_heading();
    if (is_full)
    {
        check_highlight_for_item(pos, world_to_window(pos.x, pos.z), 0, MAP_ELEMENT_PLAYER, IIT_PLAYER, heading, green, 0xFFFFFFFF);
        if (is_old_highlight_item(pos, 0, MAP_ELEMENT_PLAYER, IIT_PLAYER) || !fullscreen_map_is_loaded())
        {
            return;
        }
    }
    map_image_info mii;
    bmp_uv_information bui;
    get_image_info(mii, bui, get_minimap_bitmap_id(MAP_ELEMENT_PLAYER));
    gr_set_color(green);
    gr_set_texture(mii.bitmap_handle);
    render_bitmap_oriented(&bui, pos, orient, bmin, bmax, &mii, RUVS_180, true);
}

// 0x0084D180 (:6759)
void cartographer::get_window_coordinate_info(window_coord_info& wci, const vector& world_posn)
{
    const float dx = world_posn.x - m_map_corner_posns[0].x;
    const float dz = world_posn.z - m_map_corner_posns[0].z;
    wci.x_dot_product = m_ne_to_nw_vector.z * dz + m_ne_to_nw_vector.x * dx;
    wci.y_dot_product = m_sw_to_nw_vector.z * dz + m_sw_to_nw_vector.x * dx;
    wci.actual_posn.y = m_current_size_info.map_scale.y * wci.y_dot_product + m_current_size_info.map_posn.y;
    wci.actual_posn.x = m_current_size_info.map_scale.x * wci.x_dot_product + m_current_size_info.map_posn.x;
}

// 0x0084D290 (:6833)
void cartographer::get_world_posn(const vector2& window_posn, vector& world_posn)
{
    const float nz = m_ne_to_nw_vector.z;
    const float sx = m_sw_to_nw_vector.x;
    const float sz = m_sw_to_nw_vector.z;
    const float wy = (window_posn.y - m_current_size_info.map_posn.y) / m_current_size_info.map_scale.y;
    const float x = (((window_posn.x - m_current_size_info.map_posn.x) / m_current_size_info.map_scale.x) * sz - nz * wy) / (m_ne_to_nw_vector.x * sz - sx * nz);
    world_posn.y = 0.0f;
    world_posn.x = m_map_corner_posns[0].x + x;
    world_posn.z = m_map_corner_posns[0].z + (wy - sx * x) / sz;
}

// A quad over the minimap's window with one color and given uvs. Inlined in the renderers below.
void cartographer::fill_mini_quad(rl_primitive_2d_vert* verts, unsigned char value, const vector2* uvs)
{
    const vector2 corners[4] = {vector2(m_mini_size_info.map_posn.x, m_mini_size_info.map_posn.y), vector2(m_mini_size_info.map_end_posn.x, m_mini_size_info.map_posn.y), vector2(m_mini_size_info.map_posn.x, m_mini_size_info.map_end_posn.y), vector2(m_mini_size_info.map_end_posn.x, m_mini_size_info.map_end_posn.y)};
    for (int i = 0; i < 4; i++)
    {
        verts[i].pt.x = corners[i].x;
        verts[i].pt.y = corners[i].y;
        verts[i].color.red = value;
        verts[i].color.green = value;
        verts[i].color.blue = value;
        verts[i].color.alpha = 0xFF;
        verts[i].uvs.x = uvs[i].x;
        verts[i].uvs.y = uvs[i].y;
    }
}

// 0x00862170 (:6848)
void cartographer::render_alpha_mask()
{
    // Magic number: the raw primitive state of rfg.exe.
    rl_primitive_state state;
    state.set_bits(((unsigned int)Minimap_scissor_enabled << 0x12) | 0x80000202);
    gr_set_texture(Map_element_id_mask);
    rl_draw::get_instance()->tristrip_2d_begin(state);
    rl_primitive_2d_vert* verts = rl_draw::get_instance()->tristrip_2d_allocate_tris(2);
    if (verts != nullptr)
    {
        const vector2 uvs[4] = {vector2(0.0f, 0.0f), vector2(1.0f, 0.0f), vector2(0.0f, 1.0f), vector2(1.0f, 1.0f)};
        fill_mini_quad(verts, 0x00, uvs);
    }
    rl_draw::get_instance()->tristrip_2d_end();
}

// 0x00862280 (:6884)
void cartographer::render_overlay_map()
{
    const int bitmap_handle = vint_bms_get_bitmap(Map_element_id_overlay);
    bmp_uv_information bui;
    get_uv_information_bmpsheet(vint_bms_get_image_data(Map_element_id_overlay), bitmap_handle, bui.uvs[0], bui.uvs[1], bui.uvs[2], bui.uvs[3]);
    gr_set_color(0xFF, 0xFF, 0xFF, 0xFF);
    gr_set_texture(bitmap_handle);
    rl_draw::get_instance()->tristrip_2d_begin(m_rl_render_state);
    rl_primitive_2d_vert* verts = rl_draw::get_instance()->tristrip_2d_allocate_tris(2);
    if (verts != nullptr)
    {
        const vector2 uvs[4] = {bui.uvs[3], bui.uvs[2], bui.uvs[1], bui.uvs[0]};
        fill_mini_quad(verts, 0xFF, uvs);
    }
    rl_draw::get_instance()->tristrip_2d_end();
}

// 0x00862400 (:6927)
color cartographer::get_human_color(const human* hp, human_team_dispositions disposition)
{
    static const color no_color(200, 200, 200, 0xFF);
    static const color ally_color(0xF0, 200, 0x34, 0xFF);
    static const color friendly_color(0xF0, 200, 0x34, 0xFF);
    static const color enemy_color(0x94, 0x0D, 0x00, 0xFF);
    static const color enemy_officer_color(0x94, 0x0D, 0x00, 0xFF);
    static const color enemy_marauder_color(0x78, 0x50, 0x82, 0xFF);
    if (hp == nullptr)
    {
        return no_color;
    }
    player* target = get_target_player();
    const ai_system* ai = hp->get_obj_type() == OT_HUMAN && hp->get_sub_type() == OT_SUB_HUMAN_NPC ? ((const npc*)hp)->get_ai() : nullptr;
    if (Multiplayer)
    {
        if (disposition == TEAM_DISPOSITION_ALLIED)
        {
            return ally_color;
        }
        return disposition == TEAM_DISPOSITION_HOSTILE ? enemy_color : no_color;
    }
    if (hp->hflags.activity_ha_hostage)
    {
        return ally_color;
    }
    switch (disposition)
    {
        case TEAM_DISPOSITION_ALLIED:
            // The player's squad keeps the neutral color.
            if (ai != nullptr && ai->is_in_player_squad(false) == target)
            {
                return no_color;
            }
            return ally_color;
        case TEAM_DISPOSITION_FRIENDLY:
            return friendly_color;
        case TEAM_DISPOSITION_HOSTILE:
        case TEAM_DISPOSITION_UNFRIENDLY:
            if (hp->current_team == HUMAN_TEAM_MARAUDER)
            {
                return enemy_marauder_color;
            }
            return hp->h_info->flags.officer ? enemy_officer_color : enemy_color;
        default:
            return no_color;
    }
}

// 0x00862680 (:7005)
color cartographer::get_vehicle_color(const vehicle* vp)
{
    static const color no_color(200, 200, 200, 0xFF);
    static const color ally_color(0xF0, 200, 0x34, 0xFF);
    static const color enemy_color(0x94, 0x0D, 0x00, 0xFF);
    static const color enemy_marauder_color(0x78, 0x50, 0x82, 0xFF);
    if (vp == nullptr)
    {
        return no_color;
    }
    player* local = player_get_local_player();
    const human_teams player_team = local != nullptr ? local->current_team : HUMAN_TEAM_GUERILLA;
    human* driver = vehicle_passenger_in_seat(vp, VSI_DRIVER);
    if (driver == nullptr)
    {
        driver = vehicle_passenger_in_seat(vp, VSI_PASSENGER_1);
    }
    const human_teams vehicle_team = driver != nullptr ? driver->current_team : HUMAN_TEAM_NONE;
    const int disposition = (int)human_teams_get_disposition(player_team, vehicle_team);
    if (disposition < 3)
    {
        if (disposition < 2)
        {
            return vehicle_team == HUMAN_TEAM_MARAUDER ? enemy_marauder_color : enemy_color;
        }
    }
    else if (vehicle_team == HUMAN_TEAM_GUERILLA || vehicle_team == HUMAN_TEAM_MARAUDER)
    {
        return ally_color;
    }
    return no_color;
}

// The mover's control and its "destroyed for good" flag, from the parent for a piece. Inlined in the next two.
static float structure_control(const object_mover* mp)
{
    const object* parent = mp->get_parent() != 0xFFFFFFFF ? World.object_from_handle(mp->get_parent()) : nullptr;
    if (parent != nullptr && parent->get_obj_type() != OT_MOVER)
    {
        parent = nullptr;
    }
    if (mp->get_sub_type() == OT_SUB_MOVER_RFG && ((const rfg_mover*)mp)->flags.inherit_damage_pct && parent != nullptr)
    {
        return ((const object_mover*)parent)->control;
    }
    return mp->control;
}

// 0x00862850 (:7048)
rl_color cartographer::get_structure_color(const object_mover* mp)
{
    if (mp == nullptr)
    {
        return Structure_colors[5];
    }
    rl_color result;
    const human_teams team = mp->team;
    const float control = structure_control(mp);
    // Magic numbers: 0x20000000 and 0x20000, object_mover flags (always hostile, mining).
    if ((mp->mover_flags & 0x20000000) != 0)
    {
        result = Structure_colors[mp->is_killed() ? 4 : 1];
    }
    else if (team < NUM_HUMAN_TEAMS && (team != HUMAN_TEAM_EDF || (!Multiplayer && (0.0f < control || (mp->mover_flags & 0x20000) != 0 || mp->is_killed()))))
    {
        result = mp->is_killed() ? Structure_colors[4] : Structure_colors[team];
    }
    else
    {
        result = mp->get_importance_level() > 1 ? Structure_colors[1] : Structure_colors[5];
    }
    if (!mp->is_killed())
    {
        // Magic number: 5 seconds of a flash a second after damage.
        static const float one_over_time_per_flash = 1.0f; // 0x02BE58FC
        const float seconds = (float)mp->time_since_last_damage() * 0.001f;
        if (0.0f < seconds && seconds < 5.0f)
        {
            float flash = fabsf((seconds - (float)(int)(one_over_time_per_flash * seconds)) * one_over_time_per_flash * 2.0f - 1.0f);
            if (flash - 0.0f < 0.0f)
            {
                flash = 0.0f;
            }
            if (0.0f <= flash - 1.0f)
            {
                flash = 1.0f;
            }
            const float add = (1.0f - flash) * 255.0f;
            result.red = (unsigned char)(int)((float)result.red * flash + add);
            result.green = (unsigned char)(int)(flash * (float)result.green + add);
            result.blue = (unsigned char)(int)((float)result.blue * flash + add);
        }
    }
    return result;
}

// 0x00862B00 (:7104)
int cartographer::get_structure_color_index(const object_mover* mp, float& flash_percent)
{
    flash_percent = 1.0f;
    // Magic number: past the table's teams: index 5, the generic color.
    if (mp == nullptr)
    {
        return 5;
    }
    int index = mp->team;
    const float control = structure_control(mp);
    if ((mp->mover_flags & 0x20000000) != 0)
    {
        index = mp->is_killed() ? 4 : 1;
    }
    else if (index > HUMAN_TEAM_MARAUDER || (index == HUMAN_TEAM_EDF && (Multiplayer || (control <= 0.0f && (mp->mover_flags & 0x20000) == 0 && !mp->is_killed()))))
    {
        index = mp->get_importance_level() < 2 ? 5 : 1;
    }
    else if (mp->is_killed())
    {
        index = 4;
    }
    if (!mp->is_killed())
    {
        static const float one_over_time_per_flash = 1.0f; // 0x02BE5904
        const float seconds = (float)mp->time_since_last_damage() * 0.001f;
        if (0.0f < seconds && seconds < 5.0f)
        {
            float flash = fabsf((seconds - (float)(int)(one_over_time_per_flash * seconds)) * one_over_time_per_flash * 2.0f - 1.0f);
            if (flash - 0.0f < 0.0f)
            {
                flash = 0.0f;
            }
            if (0.0f <= flash - 1.0f)
            {
                flash = 1.0f;
            }
            flash_percent = flash;
            return index;
        }
        flash_percent = 1.0f;
    }
    return index;
}

// 0x00862D30 (:7159)
int cartographer::get_structure_importance_icon(const object_mover* mp)
{
    if (mp == nullptr || mp->is_killed())
    {
        return -1;
    }
    if ((mp->mover_type_bit_flags & 1) != 0)
    {
        return -2;
    }
    const object* parent = mp->get_parent() != 0xFFFFFFFF ? World.object_from_handle(mp->get_parent()) : nullptr;
    if (parent != nullptr && parent->get_obj_type() != OT_MOVER)
    {
        parent = nullptr;
    }
    const object_importance_level level = mp->get_importance_level();
    if (level > 0 && mp->team == HUMAN_TEAM_EDF && parent == nullptr)
    {
        if (level == OBJECT_IMPORTANCE_LEVEL_MEDIUM)
        {
            return MAP_ELEMENT_IMPORTANCE_MEDIUM;
        }
        if (level == OBJECT_IMPORTANCE_LEVEL_HIGH)
        {
            return MAP_ELEMENT_IMPORTANCE_HIGH;
        }
    }
    return -1;
}

// 0x0084D3C0 (:7194)
void cartographer::determine_onscreen_position()
{
    if (!m_determine_onscreen_posn)
    {
        return;
    }
    const int handle = Multiplayer ? hud_ui_multiplayer_get_doc_handle(HMD_MAIN) : hud_ui_get_document_handle(HDI_BASE);
    vint_document* document = ui_get_vint_document(handle);
    if (document == nullptr)
    {
        return;
    }
    vint_element_base* element = (vint_element_base*)vint_object_base::find_object(cProp_map_name, document);
    if (element != nullptr)
    {
        const vector2 scale = vint_project_get_scale(VINT_RES_SCALE_MAINTAIN_RATIO, VINT_RES_SCALE_MAINTAIN_RATIO);
        const vector2 offset = vint_project_get_offset(VINT_RES_OFFSET_START, VINT_RES_OFFSET_END);
        vint_variant size_value;
        element->get_property_value(cProp_screen_size_minimap, size_value, 0);
        vector2 size(0.0f, 0.0f);
        if (size_value.get_type() == VINT_PROP_TYPE_VECTOR2F)
        {
            size = (vector2)size_value;
        }
        m_mini_size_info.map_size.x = size.x * scale.x;
        m_mini_size_info.map_size.y = size.y * scale.y;
        element->get_global_anchor(m_mini_size_info.map_posn);
        m_mini_size_info.map_posn.x = scale.x * m_mini_size_info.map_posn.x + offset.x;
        m_mini_size_info.map_posn.y = m_mini_size_info.map_posn.y * scale.y + offset.y;
        m_mini_size_info.map_end_posn.x = m_mini_size_info.map_posn.x + m_mini_size_info.map_size.x;
        m_mini_size_info.map_end_posn.y = m_mini_size_info.map_size.y + m_mini_size_info.map_posn.y;
    }
    m_determine_onscreen_posn = false;
}

// 0x00872980 (:7247)
void cartographer::determine_map_information_mini()
{
    float center_x = Rfg_camera.real_pos.x;
    float center_z = Rfg_camera.real_pos.z;
    player* pp = get_target_player();
    if (pp != nullptr)
    {
        center_x = pp->pos.x;
        center_z = pp->pos.z;
    }
    m_mini_size_info.center_pos_world.x = center_x;
    m_mini_size_info.center_pos_world.y = 0.0f;
    m_mini_size_info.center_pos_world.z = center_z;
    float heading = 0.0f;
    if (!Minimap_rot0)
    {
        matrix orient(Rfg_camera.real_orient);
        heading = orient.extract_heading();
    }
    m_mini_size_info.zoom_radius = m_current_min_zoom;
    // get_avg_vehicle_speed adds half the average too, so a vehicle's speed counts twice.
    const float speed = get_avg_vehicle_speed();
    m_mini_size_info.zoom_radius = speed * 0.5f + m_mini_size_info.zoom_radius;
    m_mini_size_info.zoom_radius_squared = m_mini_size_info.zoom_radius * m_mini_size_info.zoom_radius;
    const float radius = m_mini_size_info.zoom_radius;
    m_map_corner_posns[0] = vector(-radius, 0.0f, radius);
    m_map_corner_posns[1] = vector(radius, 0.0f, radius);
    m_map_corner_posns[2] = vector(-radius, 0.0f, -radius);
    m_map_corner_posns[3] = vector(radius, 0.0f, -radius);
    for (int i = 0; i < 4; i++)
    {
        m_map_corner_posns[i].rotate_around_y_axis(heading);
        m_map_corner_posns[i].x = m_mini_size_info.center_pos_world.x + m_map_corner_posns[i].x;
        m_map_corner_posns[i].z = m_mini_size_info.center_pos_world.z + m_map_corner_posns[i].z;
    }
    m_ne_to_nw_vector = vector(m_map_corner_posns[1].x - m_map_corner_posns[0].x, 0.0f, m_map_corner_posns[1].z - m_map_corner_posns[0].z);
    m_sw_to_nw_vector = vector(m_map_corner_posns[2].x - m_map_corner_posns[0].x, 0.0f, m_map_corner_posns[2].z - m_map_corner_posns[0].z);
    if (0.0001f <= m_ne_to_nw_vector.x * m_ne_to_nw_vector.x + m_ne_to_nw_vector.y * m_ne_to_nw_vector.y + m_ne_to_nw_vector.z * m_ne_to_nw_vector.z)
    {
        m_ne_to_nw_vector.normalize_safe();
    }
    if (0.0001f <= m_sw_to_nw_vector.x * m_sw_to_nw_vector.x + m_sw_to_nw_vector.y * m_sw_to_nw_vector.y + m_sw_to_nw_vector.z * m_sw_to_nw_vector.z)
    {
        m_sw_to_nw_vector.normalize_safe();
    }
    m_mini_size_info.map_scale.x = (m_mini_size_info.map_size.x * 0.5f) / radius;
    m_mini_size_info.map_scale.y = (m_mini_size_info.map_size.y * 0.5f) / radius;
}

// 0x0084D580 (:7295)
void cartographer::determine_map_information_full()
{
    const float radius = m_full_size_info.zoom_radius;
    const float cx = m_full_size_info.center_pos_world.x;
    const float cz = m_full_size_info.center_pos_world.z;
    m_map_corner_posns[0] = vector(cx - radius, 0.0f, cz + radius);
    m_map_corner_posns[1] = vector(cx + radius, 0.0f, cz + radius);
    m_map_corner_posns[2] = vector(cx - radius, 0.0f, cz - radius);
    m_map_corner_posns[3] = vector(cx + radius, 0.0f, cz - radius);
    m_ne_to_nw_vector = vector(m_map_corner_posns[1].x - m_map_corner_posns[0].x, 0.0f, m_map_corner_posns[1].z - m_map_corner_posns[0].z);
    m_sw_to_nw_vector = vector(m_map_corner_posns[2].x - m_map_corner_posns[0].x, 0.0f, m_map_corner_posns[2].z - m_map_corner_posns[0].z);
    m_ne_to_nw_vector.normalize_safe();
    m_sw_to_nw_vector.normalize_safe();
}

// 0x00872D20 (:7324)
void cartographer::collect_terrain()
{
    m_counts[0] = 0;
    for (int i = 0; i < World.get_num_objects<obj_zone>(); i++)
    {
        obj_zone* zone = World.get_object<obj_zone>(i);
        vector bmin, bmax;
        zone->get_bbox(bmin, bmax);
        const float half = (bmax.x - bmin.x) * 0.5f;
        const float dx = (bmin.x + half) - m_current_size_info.center_pos_world.x;
        const float dz = ((bmax.z - bmin.z) * 0.5f + bmin.z) - m_current_size_info.center_pos_world.z;
        const float reach = one_over_cos_of_45 * half + m_mini_size_info.zoom_radius;
        if (dx * dx + dz * dz < reach * reach)
        {
            add_terrain(zone);
        }
    }
}

// 0x00872E80 (:7363)
void cartographer::collect_structures()
{
    if (m_map_mode != MAP_MODE_FULL && !m_collected_structures_timer[0].elapsed())
    {
        return;
    }
    // Magic numbers: 250 meters around the minimap, once a second (1000 ms).
    float radius = 250.0f;
    bool (*collect_cb)(object*, unsigned int, const void*) = cartographer_mover_collect_cb;
    if (m_map_mode == MAP_MODE_FULL)
    {
        radius = m_current_size_info.zoom_radius;
        collect_cb = cartographer_mover_collect_full_cb;
    }
    const vector& pos = m_current_size_info.center_pos_world;
    obj_zone* zone = World.get_zone_by_pos(pos);
    vector bmin, bmax;
    if (zone == nullptr)
    {
        bmin.y = -radius;
        bmax.y = radius;
    }
    else
    {
        // The zone's height.
        zone->get_local_bbox(bmin, bmax);
    }
    bmin.x = -radius;
    bmin.z = -radius;
    bmax.x = radius;
    bmax.z = radius;
    m_collected_structures[0].num_movers = World.collect_objects<object_mover>(bmin, bmax, pos, IDENTITY_MATRIX, m_collected_structures[0].movers, 0x200, collect_cb, nullptr);
    m_collected_structures_timer[0].set(1000);
    sort_structures(m_collected_structures);
}

// 0x0084D810 (:7408)
void cartographer::reset_structure_collect_timer()
{
    m_collected_structures_timer[0].set(0);
}

// 0x00872FC0 (:7415)
void cartographer::collect_restricted_areas()
{
    for (int i = 0; i < World.get_num_objects<object_restricted_area>(); i++)
    {
        object_restricted_area* area = World.get_object<object_restricted_area>(i);
        if (area->is_disabled())
        {
            continue;
        }
        vector bmin, bmax;
        area->get_warning_bbox(bmin, bmax);
        if (bmin == ZERO_VECTOR && bmax == ZERO_VECTOR)
        {
            area->get_bbox(bmin, bmax);
        }
        const float cx = m_current_size_info.center_pos_world.x;
        const float cz = m_current_size_info.center_pos_world.z;
        const float radius = m_current_size_info.zoom_radius;
        const bool in_x = fabsf(bmin.x - cx) <= radius || fabsf(bmax.x - cx) <= radius;
        const bool in_z = fabsf(bmin.z - cz) <= radius || fabsf(bmax.z - cz) <= radius;
        if (in_x && in_z)
        {
            add_restricted(area);
        }
        else
        {
            remove_restricted(find_restricted(area->get_handle()));
        }
    }
}

// 0x00873230 (:7452)
void cartographer::collect_convoys()
{
    for (int i = 0; i < World.get_num_objects<object_convoy>(); i++)
    {
        object_convoy* cp = World.get_object<object_convoy>(i);
        const farray<vector,512>& route = cp->get_route();
        const int count = route.size();
        if (count > 1)
        {
            m_convoy_route.handle = cp->get_handle();
            // The spline's end points are doubled.
            m_convoy_route.spline_points[0].x = route[0].x;
            m_convoy_route.spline_points[0].y = route[0].z;
            for (int p = 0; p < count; p++)
            {
                m_convoy_route.spline_points[p + 1].x = route[p].x;
                m_convoy_route.spline_points[p + 1].y = route[p].z;
            }
            m_convoy_route.spline_points[count + 1].x = route[count - 1].x;
            m_convoy_route.spline_points[count + 1].y = route[count - 1].z;
            m_convoy_route.num_spline_pts = count + 2;
        }
    }
    if (m_convoy_route.handle != 0xFFFFFFFF)
    {
        const bool is_full = m_map_mode == MAP_MODE_FULL;
        create_path_route_points(m_convoy_route, is_full, is_full ? (1.0f - fsm_zoom_percent()) * Fsm_num_path_points_scalar + 1.0f : 1.0f);
    }
}

// 0x00873480 (:7488) (LTCG: path in ECX)
// dest moves to the region's nearest node when it is inside a building region of team 1.
static bool calculate_gps_route(path_info& path, vector& start, vector& dest, bool do_direct_route)
{
    Cartographer.set_first_gps_point(ZERO_VECTOR);
    for (int i = 0; i < World.get_num_objects<object_bounding_box>(); i++)
    {
        object_bounding_box* bp = World.get_object<object_bounding_box>(i);
        // Magic number: 1, the GPS region's type.
        if (bp->type != 1 || World.get_world_zone_by_pos(start) != World.get_world_zone_by_pos(bp->pos))
        {
            continue;
        }
        vector bmin, bmax;
        bp->get_local_bbox(bmin, bmax);
        bmin.y = -3.4028235e+38f; // -FLT_MAX
        bmax.y = 3.4028235e+38f; // FLT_MAX
        if (!ix_point_in_box_oriented(start, bp->pos, bp->orient, bmin, bmax))
        {
            continue;
        }
        const object* first = bp->child_ptr;
        float best = 3.4028235e+38f;
        const object* nearest = nullptr;
        if (first != nullptr)
        {
            const object* node = first;
            do
            {
                const float dx = node->pos.x - start.x;
                const float dy = node->pos.y - start.y;
                const float dz = node->pos.z - start.z;
                const float dist = dy * dy + dx * dx + dz * dz;
                if (dist < best)
                {
                    nearest = node;
                    best = dist;
                }
                node = node->child_next;
            } while (node != first && node != nullptr);
            if (nearest != nullptr)
            {
                start = nearest->pos;
                break;
            }
        }
    }
    farray<vector,512> route;
    bool valid = false;
    if (rfg_pf_create_route_segment(start, dest, route, true) && route.size() > 1)
    {
        valid = true;
    }
    else
    {
        if (!do_direct_route)
        {
            return false;
        }
        route.clear();
        route.add(start);
        route.add(dest);
    }
    Cartographer.set_first_gps_point(route[0]);
    vector pos(Rfg_camera.real_pos);
    player* pp = Cartographer.get_target_player();
    if (pp != nullptr)
    {
        pos = pp->pos;
    }
    vector closest_point;
    float closest_dist;
    const object_path_road* road = object_road_get_closest_road(pos, true, closest_point, closest_dist, Min_GPS_dist, true, nullptr, nullptr, false);
    // The spline's points: its first and last are doubled, off the roads the start itself comes first, twice.
    int count = 0;
    if (road == nullptr)
    {
        path.spline_points[0].x = route[0].x;
        path.spline_points[0].y = route[0].z;
        path.spline_heights[0] = route[0].y;
        count = 0;
        for (int i = 0; i < route.size() && i + 2 <= 0x1FF; i++)
        {
            path.spline_points[i + 1].x = route[i].x;
            path.spline_points[i + 1].y = route[i].z;
            path.spline_heights[i + 1] = route[i].y;
            count++;
        }
        path.spline_points[count + 1].x = route[count - 1].x;
        path.spline_points[count + 1].y = route[count - 1].z;
        path.spline_heights[count + 1] = route[count - 1].y;
        count = count + 2;
    }
    else
    {
        path.spline_points[0].x = start.x;
        path.spline_points[0].y = start.z;
        path.spline_heights[0] = start.y;
        path.spline_points[1].x = start.x;
        path.spline_points[1].y = start.z;
        path.spline_heights[1] = start.y;
        count = 0;
        for (int i = 0; i < route.size() && i + 3 <= 0x1FF; i++)
        {
            path.spline_points[i + 2].x = route[i].x;
            path.spline_points[i + 2].y = route[i].z;
            path.spline_heights[i + 2] = route[i].y;
            count++;
        }
        path.spline_points[count + 2].x = route[count - 1].x;
        path.spline_points[count + 2].y = route[count - 1].z;
        path.spline_heights[count + 2] = route[count - 1].y;
        count = count + 3;
    }
    path.num_spline_pts = count;
    path.render_dirty = true;
    return valid;
}

// 0x008739B0 (:7597)
void cartographer::update_gps()
{
    Num_gps_refined_path_segments = 0;
    if (!m_has_player_marker)
    {
        return;
    }
    if (m_player_marker_item.m_obj_handle != 0xFFFFFFFF)
    {
        object* objp = World.object_from_handle(m_player_marker_item.m_obj_handle);
        if (objp != nullptr)
        {
            m_player_marker_pos = objp->pos;
        }
    }
    vector player_pos(Rfg_camera.real_pos);
    player* pp = get_target_player();
    if (pp != nullptr)
    {
        player_pos = pp->pos;
    }
    // Arrived.
    const float dx = player_pos.x - m_player_marker_pos.x;
    const float dz = player_pos.z - m_player_marker_pos.z;
    if (dz * dz + dx * dx < Minimap_gps_radius * Minimap_gps_radius)
    {
        m_has_player_marker = false;
        return;
    }
    vector start(player_pos);
    Current_gps_point = 0;
    vector* target = &m_player_marker_pos;
    for (int i = 0; i < World.get_num_objects<object_bounding_box>(); i++)
    {
        object_bounding_box* bp = World.get_object<object_bounding_box>(i);
        if (bp->type != 1 || World.get_world_zone_by_pos(bp->pos) != World.get_world_zone_by_pos(m_player_marker_pos))
        {
            continue;
        }
        vector bmin, bmax;
        bp->get_local_bbox(bmin, bmax);
        // Magic number: the region's height doesn't count.
        bmin.y = -99999.0f;
        bmax.y = 99999.0f;
        if (!ix_point_in_box_oriented(m_player_marker_pos, bp->pos, bp->orient, bmin, bmax))
        {
            continue;
        }
        if (ix_point_in_box_oriented(start, bp->pos, bp->orient, bmin, bmax))
        {
            // The player is in the marker's region: no route.
            m_valid_gps_path = false;
            return;
        }
        // The marker's region is entered at its node nearest the player.
        const object* first = bp->child_ptr;
        float best = 3.4028235e+38f; // FLT_MAX
        const object* nearest = nullptr;
        if (first != nullptr)
        {
            const object* node = first;
            do
            {
                const float nx = node->pos.x - start.x;
                const float ny = node->pos.y - start.y;
                const float nz = node->pos.z - start.z;
                const float dist = nx * nx + ny * ny + nz * nz;
                if (dist < best)
                {
                    nearest = node;
                    best = dist;
                }
                node = node->child_next;
            } while (node != first && node != nullptr);
            if (nearest != nullptr)
            {
                m_alternate_marker_pos = nearest->pos;
                target = &m_alternate_marker_pos;
            }
        }
        break;
    }
    vector dest(*target);
    m_valid_gps_path = calculate_gps_route(m_gps_path, start, dest, !m_valid_gps_path);
    if (m_valid_gps_path)
    {
        in_world_gps_register_path(&m_gps_path);
    }
}

// 0x0084D820 (:7706)
bool cartographer::pos_is_in_range_radius_squared(const vector pos, float* out_dist_sq)
{
    const float dx = pos.x - m_current_size_info.center_pos_world.x;
    const float dz = pos.z - m_current_size_info.center_pos_world.z;
    const float dist = dx * dx + dz * dz;
    if (out_dist_sq != nullptr)
    {
        *out_dist_sq = dist;
    }
    return dist <= m_current_size_info.zoom_radius_squared;
}

// 0x0084D8A0 (:7726)
bool cartographer::object_is_in_range_radius_squared(const object* obj)
{
    if (obj == nullptr)
    {
        return false;
    }
    const float dx = obj->pos.x - m_current_size_info.center_pos_world.x;
    const float dz = obj->pos.z - m_current_size_info.center_pos_world.z;
    return dx * dx + dz * dz <= m_current_size_info.zoom_radius_squared;
}

// 0x0084D920 (:7740)
int cartographer::find_terrain(unsigned int handle)
{
    for (int i = 0; i < m_counts[0]; i++)
    {
        if (m_terrain[i].handle == handle)
        {
            return i;
        }
    }
    return -1;
}

// 0x0084D950 (:7755)
bool cartographer::add_terrain(const obj_zone* zone)
{
    const int index = m_counts[0];
    if (zone == nullptr || index >= 4)
    {
        return false;
    }
    vector bmin, bmax;
    zone->get_bbox(bmin, bmax);
    char name[64];
    _snprintf_safe(name, 0x40, "%scomb", zone->get_name());
    cf_add_ext(name, 0x40, name, ".tga", false);
    object_info_terrain_bbox& terrain = m_terrain[index];
    terrain.handle = zone->get_handle();
    terrain.bm_handle = bm_load(name, false);
    terrain.mat_handle = 0xFFFFFFFF;
    if (zone->get_terrain_data() != nullptr)
    {
        terrain.mat_handle = zone->get_terrain_data()->m_minimap_material_handle;
    }
    terrain.corners[0] = vector2(bmin.x, bmax.z);
    terrain.corners[1] = vector2(bmax.x, bmax.z);
    terrain.corners[2] = vector2(bmin.x, bmin.z);
    terrain.corners[3] = vector2(bmax.x, bmin.z);
    m_counts[0]++;
    return true;
}

// 0x0084DA80 (:7817)
bool cartographer::remove_terrain(int which)
{
    if (which < 0 || which >= m_counts[0])
    {
        return false;
    }
    for (int i = which; i < m_counts[0] - 1; i++)
    {
        m_terrain[i] = m_terrain[i + 1];
    }
    m_counts[0]--;
    return true;
}

// 0x0084DB00 (:7848)
int cartographer::find_restricted(unsigned int handle)
{
    for (int i = 0; i < m_counts[1]; i++)
    {
        if (m_restricted_areas[i] == handle)
        {
            return i;
        }
    }
    return -1;
}

// 0x0084DB30 (:7863)
bool cartographer::add_restricted(const object_restricted_area* area)
{
    if (area == nullptr || m_counts[1] > 0x1F || find_restricted(area->get_handle()) != -1)
    {
        return false;
    }
    m_restricted_areas[m_counts[1]] = area->get_handle();
    m_counts[1]++;
    return true;
}

// 0x0084DBA0 (:7890)
bool cartographer::remove_restricted(int which)
{
    if (which < 0 || which >= m_counts[1])
    {
        return false;
    }
    for (int i = which; i < m_counts[1] - 1; i++)
    {
        m_restricted_areas[i] = m_restricted_areas[i + 1];
    }
    m_counts[1]--;
    return true;
}

// 0x00879A20 (:7916)
void cartographer::clear_invalid_icons()
{
    // Original quirk kept: the count is read once, a removal skips the next icon this frame.
    const int count = m_icons.size();
    for (int i = 0; i < count; i++)
    {
        if (World.object_from_handle(m_icons[i].handle) == nullptr)
        {
            map_icon test(m_icons[i].handle);
            m_icons.remove_matching(test);
            m_sticky_icons.remove_matching(test);
        }
    }
}

// 0x00873E00 (:7929)
void cartographer::add_district_liberated_icon(unsigned char x, unsigned char y, unsigned char zone_xz)
{
    district_liberated_icon icon;
    icon.x = x;
    icon.z = y;
    icon.zone_xz = zone_xz;
    fsm_district_liberated_icons.add(icon);
}

// 0x00862DD0 (:7939)
void cartographer::remove_district_liberated_icons()
{
    fsm_district_liberated_icons.clear();
}

// 0x00873E70 (:7945)
void cartographer::add_decal(const vector& pos, const matrix& orient, minimap_elements decal_bitmap, float scale_x, float scale_y, const color& in_color, bool inverted)
{
    if (m_decals.size() >= m_decals.capacity())
    {
        return;
    }
    const int bitmap_id = decal_bitmap < NUM_MAP_ELEMENTS ? get_minimap_bitmap_id(decal_bitmap) : bm_get_bogus_bitmap_handle();
    map_decal decal(pos, orient, bitmap_id, scale_x, scale_y, in_color, false, inverted);
    m_decals.add(decal);
}

// 0x00873F80 (:7960)
void cartographer::add_decal(const vector& pos, minimap_elements decal_bitmap, float radius, const color& in_color, bool inverted)
{
    add_decal(pos, IDENTITY_MATRIX, decal_bitmap, radius, radius, in_color, inverted);
}

// 0x00873FD0 (:7970)
void cartographer::add_temporary_icon(const vector& pos, minimap_elements icon_bitmap, bool stick_to_edge, unsigned int in_handle, const color& in_color, bool blink, unsigned int in_activity_handle)
{
    if (!Show_minimap || !Usability_hud_enable || Hide_hud_ref_count != 0 || !Debug_show_hud || !Add_temp_icons || m_temp_icons.size() >= m_temp_icons.capacity())
    {
        return;
    }
    adjust_objective_height(icon_bitmap, pos.y, get_target_player());
    // An activity under the fog shows as a generic one.
    if (minimap_element_is_activity_icon(icon_bitmap) && !minimap_element_visible_over_fow(icon_bitmap) && fog_of_war_is_posn_obscured(pos))
    {
        icon_bitmap = MAP_ELEMENT_GENERIC_ACTIVITY;
    }
    bool stick = stick_to_edge;
    if (!stick && m_player_marker_item.m_valid)
    {
        const vector flat(pos.x, 0.0f, pos.z);
        stick = m_player_marker_item.m_obj_pos == flat;
    }
    map_icon_temp icon(pos, ZERO_VECTOR2, ZERO_VECTOR2, icon_bitmap, in_color, stick, in_handle, blink, in_activity_handle);
    m_temp_icons.add(icon);
    if (Found_icon)
    {
        return;
    }
    if (icon_bitmap != MAP_ELEMENT_CONVOY_ICON && icon_bitmap != MAP_ELEMENT_COURIER_ICON && icon_bitmap != MAP_ELEMENT_DEF_RAID_ICON && icon_bitmap != MAP_ELEMENT_HOUSE_ICON && icon_bitmap != MAP_ELEMENT_RAID_ICON)
    {
        return;
    }
    // The newest activity's icon stays "recent" for 5 seconds.
    static float recent_activity_icon_timer; // 0x02BE5934
    if (Most_recent_icon_handle == in_handle)
    {
        if (recent_activity_icon_timer < 0.0f)
        {
            Found_icon = true;
            return;
        }
        recent_activity_icon_timer = recent_activity_icon_timer - Frametime;
        if (recent_activity_icon_timer <= 0.0f)
        {
            Found_icon = true;
            return;
        }
    }
    else
    {
        recent_activity_icon_timer = 5.0f;
        Most_recent_icon_handle = in_handle;
    }
    Most_recent_icon = &m_temp_icons[m_temp_icons.size() - 1];
    Found_icon = true;
}

// 0x008741B0 (:8040)
void cartographer::add_temporary_icon(const vector& pos, minimap_elements icon_bitmap, bool stick_to_edge, const vector2& in_bmin, const vector2& in_bmax, unsigned int in_handle, const color& in_color, bool blink)
{
    if (!Add_temp_icons || m_temp_icons.size() >= m_temp_icons.capacity())
    {
        return;
    }
    adjust_objective_height(icon_bitmap, pos.y, get_target_player());
    const bool stick = stick_to_edge || (m_player_marker_item.m_valid && m_player_marker_item.m_obj_pos.x == pos.x && m_player_marker_item.m_obj_pos.y == pos.y && m_player_marker_item.m_obj_pos.z == pos.z);
    map_icon_temp icon(pos, in_bmin, in_bmax, icon_bitmap, in_color, stick, in_handle, blink, 0xFFFFFFFF);
    m_temp_icons.add(icon);
}

// 0x00879AD0 (:8053)
void cartographer::add_icon(unsigned int h_obj, minimap_elements icon_bitmap, bool snap_to_edge, bool show_closest_group, bool player_marker)
{
    if (World.object_from_handle(h_obj) == nullptr)
    {
        return;
    }
    if (!player_marker && (does_icon_exist(h_obj) || does_sticky_icon_exist(h_obj)))
    {
        remove_icon(h_obj);
    }
    map_icon icon(h_obj, icon_bitmap);
    icon.sticky = snap_to_edge;
    if (snap_to_edge || h_obj == m_player_marker_item.m_obj_handle)
    {
        if (m_sticky_icons.size() < m_sticky_icons.capacity())
        {
            icon.only_show_closest = show_closest_group;
            m_sticky_icons.add(icon);
        }
    }
    else if (m_icons.size() < m_icons.capacity())
    {
        icon.only_show_closest = snap_to_edge;
        m_icons.add(icon);
    }
}

// 0x00874320 (:8087)
void cartographer::add_converted_guerrilla(unsigned int obj_h)
{
    if (m_guerrilla_converts.size() < m_guerrilla_converts.capacity())
    {
        convert_guerrilla_icon icon;
        icon.m_human_handle = obj_h;
        m_guerrilla_converts.add(icon);
    }
}

// 0x00874380 (:8098)
void cartographer::remove_icon(unsigned int obj)
{
    map_icon test(obj);
    m_icons.remove_matching(test);
    m_sticky_icons.remove_matching(test);
}

// 0x00862DE0 (:8105)
bool cartographer::does_icon_exist(unsigned int obj) const
{
    for (int i = 0; i < m_icons.size(); i++)
    {
        if (m_icons[i].handle == obj)
        {
            return true;
        }
    }
    return false;
}

// 0x00862E50 (:8111)
bool cartographer::does_sticky_icon_exist(unsigned int obj) const
{
    for (int i = 0; i < m_sticky_icons.size(); i++)
    {
        if (m_sticky_icons[i].handle == obj)
        {
            return true;
        }
    }
    return false;
}

// 0x0084DBF0 (:8117)
bool cartographer::is_old_highlight_item(const vector& pos, unsigned int handle, int icon_index, icon_item_type type)
{
    if (type != m_last_highlight.m_type || handle != m_last_highlight.m_obj_handle || icon_index != m_last_highlight.m_icon_index)
    {
        return false;
    }
    // The map hasn't moved off the highlighted item: its caption window stands where the icon is.
    const float dx = Fs_info.map_center.x - m_last_highlight.m_fake_pos.x;
    const float dy = Fs_info.map_center.y - m_last_highlight.m_fake_pos.y;
    const float dz = Fs_info.map_center.z - m_last_highlight.m_fake_pos.z;
    const float dist = (dz * dz + dy * dy + dx * dx) - 0.0f;
    return dist < Lock_target_dist_squared * Scroll_speed && -dist < Lock_target_dist_squared * Scroll_speed && m_last_highlight.m_obj_pos.x == pos.x && m_last_highlight.m_obj_pos.y == pos.y && m_last_highlight.m_obj_pos.z == pos.z;
}

// 0x00862EC0 (:8132)
bool cartographer::check_highlight_for_item(const vector& pos, const vector2& render_pos, unsigned int handle, int icon_index, icon_item_type type, float angle_rad, color col, unsigned int act_handle)
{
    if (m_map_mode != MAP_MODE_FULL || Fs_info.flags.targeting)
    {
        return false;
    }
    if ((unsigned int)icon_index >= NUM_MAP_ELEMENTS || !minimap_element_display_caption((minimap_elements)icon_index))
    {
        return false;
    }
    int alpha;
    if (set_icon_zoomed_alpha((minimap_elements)icon_index, true, alpha, 0xFF))
    {
        return false;
    }
    const vector2 center = world_to_window(Fs_info.map_center.x, Fs_info.map_center.z);
    const float dx = render_pos.x - center.x;
    const float dy = render_pos.y - center.y;
    const float dist = dx * dx + dy * dy;
    // Magic number: within 30 pixels of the map's center.
    if (dist >= 900.0f || dist >= m_highlight.m_dsqr)
    {
        return false;
    }
    m_highlight.m_valid = true;
    m_highlight.m_dsqr = dist;
    m_highlight.m_type = type;
    get_world_posn(render_pos, m_highlight.m_fake_pos);
    m_highlight.m_obj_pos = pos;
    m_highlight.m_obj_handle = handle;
    m_highlight.m_icon_index = icon_index;
    m_highlight.m_act_handle = act_handle;
    m_highlight.m_angle_rad = angle_rad;
    m_highlight.m_col = col;
    m_highlight.m_render_pos = render_pos;
    return true;
}

// 0x00863130 (:8171)
bool cartographer::icon_is_visible_over_fow(int icon_index, bool is_full, bool is_sticky, const vector& pos)
{
    if ((unsigned int)icon_index < NUM_MAP_ELEMENTS && minimap_element_visible_over_fow((minimap_elements)icon_index))
    {
        return true;
    }
    return !(is_full && Fog_of_war_render && fog_of_war_is_posn_obscured(pos));
}

// 0x008743F0 (:8181)
void cartographer::object_set_visible_over_fow(unsigned int handle, bool visible)
{
    if (handle == 0xFFFFFFFF)
    {
        return;
    }
    if (visible)
    {
        if (!visible_over_fow_list.insert(handle))
        {
            visible_over_fow_list.exists(handle);
        }
    }
    else
    {
        visible_over_fow_list.remove(handle);
    }
}

// 0x00874430 (:8197)
void cartographer::minimap_save(cfile* file)
{
    const unsigned char count = visible_over_fow_list.get_count();
    cf_write_float(m_mini_size_info.zoom_radius, file);
    cf_write_uint32(count, file);
    unsigned int key;
    if (visible_over_fow_list.get_first_key(key))
    {
        do
        {
            cf_write_uint32(key, file);
        } while (visible_over_fow_list.get_next_key(key, key));
    }
}

// 0x0084DD70 (:8212)
void cartographer::minimap_load(cfile* file)
{
    m_mini_size_info.zoom_radius = cf_read_float(file);
    m_current_min_zoom = m_mini_size_info.zoom_radius;
    for (unsigned int count = cf_read_uint32(file); count != 0; count--)
    {
        visible_over_fow_list.insert(cf_read_uint32(file));
    }
}

// 0x00863180 (:8237)
int minimap_get_element_bitmap_id(minimap_elements element)
{
    if ((unsigned int)(element + 1) > 0x3B)
    {
        return 0;
    }
    return element < NUM_MAP_ELEMENTS ? get_minimap_bitmap_id(element) : bm_get_bogus_bitmap_handle();
}

// 0x008631B0 (:8244)
void minimap_render_bitmap(int bitmap_id, float x, float y, float angle, float scalew, float scaleh, const gr_state& state)
{
    Cartographer.render_bitmap_alt(bitmap_id, x, y, angle, scalew, scaleh, state);
}

// 0x00863210 (:8250)
void minimap_render_bitmap_3d(int bitmap_id, const vector& pnt, const matrix& orient, float radius, const gr_state& state)
{
    Cartographer.render_bitmap_alt_3d(bitmap_id, pnt, orient, radius, state);
}

// 0x0084DDC0 (:8256)
void minimap_start_fullscreen_map(const checksum_stri& tool_tip_name_crc, bool zoom_out_on_start, bool from_script)
{
    if (gameseq_get_pending_state() != GS_NONE || game_is_paused() || activity_is_loading(ACTIVITY_MODE_DEMOLITIONS_MASTER) || activity_is_ready(ACTIVITY_MODE_DEMOLITIONS_MASTER) || activity_is_loading(ACTIVITY_MODE_DELIVERY))
    {
        return;
    }
    gameseq_push_state(GS_INGAME_FULLSCREEN_MAP, false, false);
    Fullscreen_map_tool_tip_name_crc = tool_tip_name_crc;
    Will_switch_to_full = true;
    Zoom_map_out_on_start = zoom_out_on_start;
    if (from_script)
    {
        Fullscreen_map_has_been_opened_through_script = true;
    }
}

// 0x00863240 (:8271)
// Beside the icon, in the direction from the map's center; false when the icon is off the overlay's circle.
bool cartographer::get_best_placement_info_for_string(bbox& box, const vector& world_pos, float str_width, float str_height, float icon_radius)
{
    static const vector2 radius_pos = get_point_along_overlay(0.0f); // 0x02BE590C
    const float center_x = (m_mini_size_info.map_end_posn.x + m_mini_size_info.map_posn.x) * 0.5f;
    const float center_y = (m_mini_size_info.map_end_posn.y + m_mini_size_info.map_posn.y) * 0.5f;
    const vector2 window = world_to_window(world_pos.x, world_pos.z);
    const float dx = window.x - center_x;
    const float dy = window.y - center_y;
    float radius = fabsf(radius_pos.y - center_y);
    if (radius < fabsf(radius_pos.x - center_x))
    {
        radius = fabsf(radius_pos.x - center_x);
    }
    const float dist_squared = dx * dx + dy * dy;
    const float reach = icon_fudge * icon_radius + radius;
    if (dist_squared > reach * reach)
    {
        return false;
    }
    const float dist = sqrtf(dist_squared);
    if (dist <= 0.0f)
    {
        box.min.x = center_x - (float)(Char_padding + X_padding);
        box.max.x = box.min.x + str_width + (float)Char_padding;
        box.min.z = center_y - (float)(Y_padding + Char_padding);
        box.max.z = box.min.z + str_height + (float)Char_padding;
    }
    else
    {
        const float inverse = 1.0f / dist;
        // Magic numbers: 1.1 icon radii in, 25 pixels at most.
        float offset = icon_radius * 1.1f;
        if (0.0f <= offset - 25.0f)
        {
            offset = 25.0f;
        }
        box.min.x = ((dx - inverse * dx * offset) - (float)X_padding - (float)Char_padding) + center_x;
        box.max.x = box.min.x + str_width + (float)Char_padding;
        box.min.z = ((dy - dy * inverse * offset) - (float)Y_padding - (float)Char_padding) + center_y;
        box.max.z = box.min.z + str_height + (float)Char_padding;
    }
    box.max.y = 0.0f;
    box.min.y = 0.0f;
    return true;
}

// 0x00863710 (:8351)
// The entries the last frame didn't mark go; the others age, and forget their damage after 4 seconds.
void cartographer::clear_important_bldg_health_flags()
{
    int i = 0;
    while (i < m_important_bldgs.size())
    {
        bldg_health_info& info = m_important_bldgs[i];
        if (info.m_valid)
        {
            info.m_valid = false;
            if (0.0f <= info.m_time_since_damaged)
            {
                info.m_time_since_damaged = info.m_time_since_damaged + Frametime_real;
                if (4.0f < info.m_time_since_damaged)
                {
                    info.m_time_since_damaged = -1.0f;
                }
            }
            i++;
        }
        else
        {
            m_important_bldgs.remove(i);
        }
    }
}

// 0x008744C0 (:8382)
void cartographer::add_important_bldg(unsigned int handle, float health_pct)
{
    for (int i = 0; i < m_important_bldgs.size(); i++)
    {
        bldg_health_info& info = m_important_bldgs[i];
        if (info.m_mover_handle == handle)
        {
            info.m_valid = true;
            // Magic number: smaller changes are not damage.
            if (0.001f < fabsf(health_pct - info.m_last_health_pct))
            {
                info.m_time_since_damaged = 0.0f;
            }
            info.m_last_health_pct = health_pct;
            return;
        }
    }
    if (m_important_bldgs.size() + 1 < 0x71)
    {
        bldg_health_info info(handle, health_pct);
        m_important_bldgs.add(info);
    }
}

// The fade of a damaged building's text. Inlined in both.
static float important_bldg_alpha(float time_since_damaged)
{
    if (time_since_damaged < 0.0f)
    {
        return -1.0f;
    }
    if (3.0f >= time_since_damaged)
    {
        return 1.0f;
    }
    const float alpha = 1.0f - (time_since_damaged - 3.0f);
    return alpha < 0.0f ? 0.0f : alpha;
}

// 0x008637E0 (:8414)
float cartographer::get_important_bldg_alpha(unsigned int handle)
{
    for (int i = 0; i < m_important_bldgs.size(); i++)
    {
        if (m_important_bldgs[i].m_mover_handle == handle)
        {
            return important_bldg_alpha(m_important_bldgs[i].m_time_since_damaged);
        }
    }
    return -1.0f;
}

// 0x00863880 (:8448)
float cartographer::get_important_bldg_alpha(int index)
{
    if (index < 0 || index >= m_important_bldgs.size())
    {
        return -1.0f;
    }
    return important_bldg_alpha(m_important_bldgs[index].m_time_since_damaged);
}

// 0x0084DE40 (:8479)
void cartographer::update_bldg_health_flash_alpha()
{
    static float bldg_flash_timer; // 0x01662290
    static int sign = 1; // 0x01662294
    bldg_flash_timer = (float)sign * Frametime_real + bldg_flash_timer;
    if (0.5f < bldg_flash_timer)
    {
        sign = -1;
        bldg_flash_timer = 0.5f;
    }
    else if (bldg_flash_timer < 0.0f)
    {
        sign = 1;
        bldg_flash_timer = 0.0f;
    }
    // Magic number: 0.66999996, the flash's depth.
    m_bldg_health_flash_alpha = 1.0f - (1.0f - bldg_flash_timer) * 0.66999996f;
}

// 0x0084DEE0 (:8496)
// The overlay's edge at a heading: an octagon, its corners at 22.5 degrees past the multiples of 45.
vector2 cartographer::get_point_along_overlay(float heading_deg)
{
    static bool first = true; // 0x01662298
    static float cosines[8]; // 0x02BE5874
    static float sines[8]; // 0x02BE5854
    if (first)
    {
        for (int i = 0; i < 8; i++)
        {
            const double angle = ((double)i * 45.0 - 22.5) * 0.01745299994945526 /* rfg.exe's degrees to radians, a float precision constant */;
            cosines[i] = (float)cos(angle);
            sines[i] = (float)sin(angle);
        }
        first = false;
    }
    const int index = ((int)((heading_deg + 22.5f) * 0.022222223f)) % 8;
    const int wrapped = index < 0 ? index + 8 : index;
    const float start = (float)wrapped * 45.0f - 22.5f;
    if (start < 0.0f && 315.0f < heading_deg)
    {
        heading_deg = heading_deg - 360.0f;
    }
    const float inverse = 1.0f / (start - ((float)wrapped * 45.0f + 22.5f));
    const int next = (wrapped + 1) % 8;
    const float half = m_current_size_info.map_size.x * 0.5f;
    const float t = (start - heading_deg) * inverse;
    vector2 result;
    result.x = (m_current_size_info.map_end_posn.x + m_current_size_info.map_posn.x) * 0.5f - (sines[wrapped] - (sines[wrapped] - sines[next]) * t) * half;
    result.y = (m_current_size_info.map_end_posn.y + m_current_size_info.map_posn.y) * 0.5f - (cosines[wrapped] - (cosines[wrapped] - cosines[next]) * t) * half;
    return result;
}

// 0x00879BD0 (:8541)
const district* cartographer::get_district_from_point(const vector& pnt)
{
    return get_district_from_point_2d(world_to_window(pnt.x, pnt.z));
}

// 0x008745D0 (:8551)
const district* cartographer::get_district_from_point_2d(const vector2& pnt)
{
    for (int d = 0; d < fsm_district_boundries.size(); d++)
    {
        const district_boundries& boundry = fsm_district_boundries[d];
        vector2 window[48];
        for (int v = 0; v < boundry.num_verts; v++)
        {
            int index = 0;
            while (index < boundry.num_indices && boundry.indices[index] != (unsigned char)v)
            {
                index++;
            }
            vector point;
            get_district_boundry_point(d, index, point);
            window[v] = world_to_window(point.x, point.z);
        }
        if (restricted_area_2d_point_in_polygon(pnt, window, boundry.num_verts))
        {
            return district_from_name(boundry.district_name);
        }
    }
    return nullptr;
}

// 0x00874850 (:8587)
void cartographer::load_district_boundries(cfile* cf)
{
    fsm_district_boundries.clear();
    for (unsigned int count = cf_read_uint8(cf); count != 0; count--)
    {
        district_boundries boundry;
        cf_read(&boundry, 0x140, cf, false);
        fsm_district_boundries.add(boundry);
    }
}

// 0x00863900 (:8600)
district_boundries* cartographer::get_district_boundry(int index)
{
    // Original bug kept: index == size passes.
    if (index <= fsm_district_boundries.size() && index > -1)
    {
        return &fsm_district_boundries[index];
    }
    return nullptr;
}

// 0x00863930 (:8609)
int cartographer::get_num_district_boundries()
{
    return fsm_district_boundries.size();
}

// 0x00863940 (:8618)
void cartographer::get_district_boundry_point(int district, int index, vector& pnt)
{
    // Original bug kept: district == size and index == num_indices pass.
    if (district > fsm_district_boundries.size() || district < 0)
    {
        return;
    }
    const district_boundries& boundry = fsm_district_boundries[district];
    if (index > boundry.num_indices || index < 0)
    {
        return;
    }
    const unsigned char vert = boundry.indices[index];
    const unsigned char zone = boundry.zone_xz[vert];
    pnt.x = ((float)(zone >> 4) * 511.0f - 4088.0f) + (float)boundry.x[vert] * 2.0f;
    pnt.y = 0.0f;
    pnt.z = -((float)(zone & 0xF) * 511.0f - 4088.0f) - (float)boundry.z[vert] * 2.0f;
}

// 0x00863A60 (:8636)
void cartographer::render_scan_line()
{
    if (!game_is_paused())
    {
        m_scan_line_pos = Frametime_real * Minimap_radar_update_rate + m_scan_line_pos;
        if (m_mini_size_info.map_end_posn.y + m_mini_size_info.map_size.y < m_scan_line_pos || m_scan_line_pos < m_mini_size_info.map_posn.y)
        {
            m_scan_line_pos = m_mini_size_info.map_posn.y;
        }
    }
    gr_set_texture(Map_element_id_radar_scan);
    rl_draw::get_instance()->tristrip_2d_begin(m_rl_render_state);
    rl_primitive_2d_vert* verts = rl_draw::get_instance()->tristrip_2d_allocate_tris(2);
    if (verts != nullptr)
    {
        const float top = m_scan_line_pos - m_mini_size_info.map_size.y;
        const vector2 corners[4] = {vector2(m_mini_size_info.map_posn.x, top), vector2(m_mini_size_info.map_end_posn.x, top), vector2(m_mini_size_info.map_posn.x, m_scan_line_pos), vector2(m_mini_size_info.map_end_posn.x, m_scan_line_pos)};
        const vector2 uvs[4] = {vector2(0.0f, 0.0f), vector2(1.0f, 0.0f), vector2(0.0f, 1.0f), vector2(1.0f, 1.0f)};
        for (int i = 0; i < 4; i++)
        {
            verts[i].pt.x = corners[i].x;
            verts[i].pt.y = corners[i].y;
            verts[i].color.red = 0xFF;
            verts[i].color.green = 0xFF;
            verts[i].color.blue = 0xFF;
            verts[i].color.alpha = 0xFF;
            verts[i].uvs.x = uvs[i].x;
            verts[i].uvs.y = uvs[i].y;
        }
    }
    rl_draw::get_instance()->tristrip_2d_end();
}

// 0x0084E150 (:8669)
void cartographer::handle_zoom_in_sounds(bool zooming_in)
{
    if (zooming_in)
    {
        if (zoom_in_alr_handle == -1)
        {
            audiolib_att_loop_rel_start(Ga_cue_ids.minimap_zoom_in_attack, &zoom_in_alr_handle, false, nullptr, nullptr);
        }
    }
    else if (zoom_in_alr_handle != -1)
    {
        audiolib_att_loop_rel_stop(zoom_in_alr_handle, 100);
        zoom_in_alr_handle = -1;
    }
}

// 0x0084E1B0 (:8688)
void cartographer::handle_zoom_out_sounds(bool zooming_out)
{
    if (zooming_out)
    {
        if (zoom_out_alr_handle == -1)
        {
            audiolib_att_loop_rel_start(Ga_cue_ids.minimap_zoom_out_start, &zoom_out_alr_handle, false, nullptr, nullptr);
        }
    }
    else if (zoom_out_alr_handle != -1)
    {
        audiolib_att_loop_rel_stop(zoom_out_alr_handle, 100);
        zoom_out_alr_handle = -1;
    }
}

// 0x0084E210 (:8707)
void cartographer::change_minimap_zoom()
{
    if (game_is_paused() || !control_player_input_enabled())
    {
        return;
    }
    const float last_zoom = m_current_min_zoom;
    if (controls_is_button_down(CBA_MINIMAP_ZOOM_IN, Player_controller_index))
    {
        m_current_min_zoom = m_current_min_zoom - Frametime * Minimap_zoom_speed;
    }
    if (controls_is_button_down(CBA_MINIMAP_ZOOM_OUT, Player_controller_index))
    {
        m_current_min_zoom = Frametime * Minimap_zoom_speed + m_current_min_zoom;
    }
    if (m_current_min_zoom < Minimap_min_radius)
    {
        m_current_min_zoom = Minimap_min_radius;
    }
    else if (Minimap_max_radius < m_current_min_zoom)
    {
        m_current_min_zoom = Minimap_max_radius;
    }
    handle_zoom_out_sounds(last_zoom < m_current_min_zoom);
    handle_zoom_in_sounds(m_current_min_zoom < last_zoom);
}

// 0x0084E3E0 (:8744)
void cartographer::enable_mini_static(unsigned int obj, float radius)
{
    m_static_target = obj;
    m_static_radius = radius;
}

// The alpha map and the building texture of a structure material. Written out twice in rfg.exe.
static bool setup_structure_material(rl_material_data& data, rl_texture_desc* textures, rl_material_const* constants, unsigned int shader, int alpha_map_handle, const char* building_texture)
{
    rl_shader_library* shaders = rl_shader_library::get_instance();
    data.textures.from_pointer(textures);
    data.num_textures = 2;
    textures[0].texture_handle = alpha_map_handle;
    textures[0].data.rt.texture_flags = 0;
    textures[0].name_checksum = crc_stri("Building_alpha_map", 0);
    const unsigned int alpha_stage = shaders->get_shader_tmu_index(shader, textures[0].name_checksum);
    if (alpha_stage == 0xFFFFFFFF)
    {
        return false;
    }
    textures[0].data.rt.texture_stage = alpha_stage;
    textures[1].texture_handle = bm_load(building_texture, false);
    textures[1].data.rt.texture_flags = 0;
    textures[1].name_checksum = crc_stri("Building_texture", 0);
    const unsigned int building_stage = shaders->get_shader_tmu_index(shader, textures[1].name_checksum);
    if (building_stage == 0xFFFFFFFF)
    {
        return false;
    }
    textures[1].data.rt.texture_stage = building_stage;
    data.num_constants = 6;
    data.max_constants = 6;
    data.constant_block.from_pointer(constants);
    memset(constants, 0, sizeof(rl_material_const) * 6);
    // Magic number: 0x110, the material's flags.
    data.mat_flags |= 0x110;
    data.shader_handle = shader;
    return true;
}

// 0x0087E5A0 (:8750)
bool cartographer::create_materials(rl_material** edf_mat, rl_material** non_edf_mat)
{
    const unsigned int shader = rl_shader_library::get_instance()->lookup("ui_map_bldg");
    if (shader == 0xFFFFFFFF)
    {
        return false;
    }
    // Magic string: the first thumbnail stands in until a building's own is set per vertex.
    const int alpha_map = vint_bms_get_bitmap(vint_bms_load("0101ba_thumb"));
    if (!setup_structure_material(Edf_mat_data, Edf_textures, Edf_structure_color_const, shader, alpha_map, cEdf_building_texture))
    {
        return false;
    }
    if (!setup_structure_material(Non_edf_mat_data, Non_edf_textures, Non_edf_structure_color_const, shader, alpha_map, cGeneric_building_texture))
    {
        return false;
    }
    set_structure_mask_constants(&Edf_mat_data, &Non_edf_mat_data);
    m_edf_mat_data = &Edf_mat_data;
    m_non_edf_mat_data = &Non_edf_mat_data;
    *edf_mat = rl_material::create();
    (*edf_mat)->set_material_data(&Edf_mat_data);
    m_edf_bldg_mat_handle = rl_material_library_register(*edf_mat);
    *non_edf_mat = rl_material::create();
    (*non_edf_mat)->set_material_data(&Non_edf_mat_data);
    m_non_edf_bldg_mat_handle = rl_material_library_register(*non_edf_mat);
    return m_edf_bldg_mat_handle != 0xFFFFFFFF && m_non_edf_bldg_mat_handle != 0xFFFFFFFF;
}

// 0x00874910 (:8873)
void cartographer::set_structure_mask_constants(rl_material_data* edf_mat_data, rl_material_data* non_edf_mat_data)
{
    for (int i = 0; i < 6; i++)
    {
        rl_color_float clr;
        clr.red = (float)Structure_colors[i].red / 255.0f;
        clr.green = (float)Structure_colors[i].green / 255.0f;
        clr.blue = (float)Structure_colors[i].blue / 255.0f;
        clr.alpha = (float)Structure_colors[i].alpha / 255.0f;
        const rl_color_float linear = clr.from_sRGB_to_linear();
        edf_mat_data->constant_block[i].set(linear.red, linear.green, linear.blue, linear.alpha);
        non_edf_mat_data->constant_block[i].set(linear.red, linear.green, linear.blue, linear.alpha);
    }
}

// 0x0084E400 (:8892)
void cartographer::release_materials()
{
    if (m_edf_bldg_mat != nullptr)
    {
        rl_material_library_unregister(m_edf_bldg_mat);
        m_edf_bldg_mat->release();
    }
    if (m_non_edf_bldg_mat != nullptr)
    {
        rl_material_library_unregister(m_non_edf_bldg_mat);
        m_non_edf_bldg_mat->release();
    }
    m_non_edf_bldg_mat_handle = 0xFFFFFFFF;
    m_edf_bldg_mat_handle = 0xFFFFFFFF;
}

// 0x0084E460 (:8908)
void cartographer::set_first_gps_point(const vector& pos)
{
    m_first_gps_point = pos;
}

// One icon of determine_render_positions: its size from its image, half each way.
static void icon_half_size(int bitmap, vector2& bmin, vector2& bmax)
{
    const int bitmap_id = minimap_icon_bitmap_id(bitmap);
    const bm_image* image = vint_bms_get_image_data(bitmap_id);
    const float width = (float)image->bmw;
    const float height = (float)image->bmh;
    bmin = vector2(width * -0.5f, height * -0.5f);
    bmax = vector2(width * 0.5f, height * 0.5f);
}

// 0x00874A30 (:8913)
void cartographer::determine_render_positions(float zoom_scale, bool is_full)
{
    for (int i = 0; i < m_icons.size(); i++)
    {
        object* objp = World.object_from_handle(m_icons[i].handle);
        if (objp != nullptr)
        {
            m_icons[i].render_pos = world_to_window(objp->pos.x, objp->pos.z);
        }
    }
    for (int i = 0; i < m_bldg_icons.size(); i++)
    {
        m_bldg_icons[i].render_pos = world_to_window(m_bldg_icons[i].pos.x, m_bldg_icons[i].pos.y);
    }
    for (int i = 0; i < m_sticky_icons.size(); i++)
    {
        object* objp = World.object_from_handle(m_sticky_icons[i].handle);
        if (objp != nullptr)
        {
            m_sticky_icons[i].render_pos = world_to_window(objp->pos.x, objp->pos.z);
        }
    }
    for (int i = 0; i < m_temp_icons.size(); i++)
    {
        m_temp_icons[i].render_pos = world_to_window(m_temp_icons[i].pos.x, m_temp_icons[i].pos.y);
    }
    if (!is_full)
    {
        return;
    }
    vector2 bmin, bmax;
    for (int i = 0; i < m_icons.size(); i++)
    {
        icon_half_size(m_icons[i].bitmap, bmin, bmax);
        if (minimap_element_visible_on_fs_map((minimap_elements)m_icons[i].bitmap))
        {
            repulse_icons(m_icons[i].render_pos, bmin, bmax, zoom_scale);
        }
    }
    for (int i = 0; i < m_bldg_icons.size(); i++)
    {
        const minimap_elements element = (minimap_elements)m_bldg_icons[i].bitmap;
        icon_half_size(element, bmin, bmax);
        if (minimap_element_visible_on_fs_map(element) && (minimap_element_is_visible_when_zoomed(element) || Fsm_max_visible_dist < fsm_zoom_percent()))
        {
            repulse_icons(m_bldg_icons[i].render_pos, bmin, bmax, zoom_scale);
        }
    }
    for (int i = 0; i < m_sticky_icons.size(); i++)
    {
        icon_half_size(m_sticky_icons[i].bitmap, bmin, bmax);
        if (minimap_element_visible_on_fs_map((minimap_elements)m_sticky_icons[i].bitmap))
        {
            repulse_icons(m_sticky_icons[i].render_pos, bmin, bmax, zoom_scale);
        }
    }
    for (int i = 0; i < m_temp_icons.size(); i++)
    {
        icon_half_size(m_temp_icons[i].bitmap, bmin, bmax);
        if (minimap_element_visible_on_fs_map((minimap_elements)m_temp_icons[i].bitmap))
        {
            repulse_icons(m_temp_icons[i].render_pos, bmin, bmax, zoom_scale);
        }
    }
}

// Pushes one icon out of in_pos's circle, along their line. Inlined four times in repulse_icons.
static void repulse_one_icon(vector2& render_pos, int bitmap, const vector2& in_pos, float in_radius, float zoom_scale)
{
    const int bitmap_id = minimap_icon_bitmap_id(bitmap);
    const bm_image* image = vint_bms_get_image_data(bitmap_id);
    const float width = (float)image->bmw;
    const float height = (float)image->bmh;
    float size = (width * 0.5f - width * -0.5f) * zoom_scale;
    const float size_y = (height * 0.5f - height * -0.5f) * zoom_scale;
    if (size <= size_y)
    {
        size = size_y;
    }
    const float radius = size * 0.5f * Fsm_repulse_icons_radius_mod;
    const vector other(render_pos.x, 0.0f, render_pos.y);
    const vector center(in_pos.x, 0.0f, in_pos.y);
    if (ix_sphere_sphere(center, other, in_radius, radius))
    {
        vector2 dir(render_pos.x - in_pos.x, render_pos.y - in_pos.y);
        dir.normalize_safe();
        // Magic number: on the same spot there is no direction: straight down.
        if (dir.y * dir.y + dir.x * dir.x < 0.95f)
        {
            dir.x = 0.0f;
            dir.y = 1.0f;
        }
        render_pos.x = dir.x * (radius + in_radius) + in_pos.x;
        render_pos.y = dir.y * (radius + in_radius) + in_pos.y;
    }
}

// 0x00863C10 (:9018)
void cartographer::repulse_icons(const vector2& in_pos, const vector2& in_bmin, const vector2& in_bmax, float zoom_scale)
{
    float size = (in_bmax.x - in_bmin.x) * zoom_scale;
    const float size_y = (in_bmax.y - in_bmin.y) * zoom_scale;
    if (size <= size_y)
    {
        size = size_y;
    }
    const float radius = size * 0.5f * Fsm_repulse_icons_radius_mod;
    const auto repulsed = [](int bitmap) -> bool
    {
        const minimap_elements element = (minimap_elements)bitmap;
        return bitmap != MAP_ELEMENT_CONVOY_ICON && (unsigned int)bitmap < NUM_MAP_ELEMENTS && !minimap_element_is_objective_icon(element) && minimap_element_visible_on_fs_map(element);
    };
    for (int i = 0; i < m_icons.size(); i++)
    {
        if ((m_icons[i].render_pos.x != in_pos.x || m_icons[i].render_pos.y != in_pos.y) && repulsed(m_icons[i].bitmap))
        {
            repulse_one_icon(m_icons[i].render_pos, m_icons[i].bitmap, in_pos, radius, zoom_scale);
        }
    }
    for (int i = 0; i < m_bldg_icons.size(); i++)
    {
        const int bitmap = m_bldg_icons[i].bitmap;
        if ((m_bldg_icons[i].render_pos.x != in_pos.x || m_bldg_icons[i].render_pos.y != in_pos.y) && bitmap != MAP_ELEMENT_IMPORTANCE_HIGH && (unsigned int)bitmap < NUM_MAP_ELEMENTS && minimap_element_visible_on_fs_map((minimap_elements)bitmap) && (minimap_element_is_visible_when_zoomed((minimap_elements)bitmap) || Fsm_max_visible_dist < fsm_zoom_percent()))
        {
            repulse_one_icon(m_bldg_icons[i].render_pos, bitmap, in_pos, radius, zoom_scale);
        }
    }
    for (int i = 0; i < m_sticky_icons.size(); i++)
    {
        if ((m_sticky_icons[i].render_pos.x != in_pos.x || m_sticky_icons[i].render_pos.y != in_pos.y) && repulsed(m_sticky_icons[i].bitmap))
        {
            repulse_one_icon(m_sticky_icons[i].render_pos, m_sticky_icons[i].bitmap, in_pos, radius, zoom_scale);
        }
    }
    for (int i = 0; i < m_temp_icons.size(); i++)
    {
        if ((m_temp_icons[i].render_pos.x != in_pos.x || m_temp_icons[i].render_pos.y != in_pos.y) && repulsed(m_temp_icons[i].bitmap))
        {
            repulse_one_icon(m_temp_icons[i].render_pos, m_temp_icons[i].bitmap, in_pos, radius, zoom_scale);
        }
    }
}

// 0x0084E480 (:9163)
bool cartographer::is_player_marker_item(const unsigned int handle, const vector& pos)
{
    return m_has_player_marker && m_player_marker_item.m_valid && ((m_player_marker_item.m_obj_handle != 0xFFFFFFFF && m_player_marker_item.m_obj_handle == handle) || (m_player_marker_item.m_obj_pos.x == pos.x && m_player_marker_item.m_obj_pos.y == pos.y && m_player_marker_item.m_obj_pos.z == pos.z));
}

// 0x0087E9F0 (:9168)
void cartographer::set_gps_to_spawned_activity()
{
    if (!activity_none() && !activity_is_ready(ACTIVITY_MODE_HOUSE_ARREST) && !activity_is_ready(ACTIVITY_MODE_RAID_ATTACK))
    {
        return;
    }
    const unsigned int handle = activity_get_last_spawned();
    object* objp = World.object_from_handle(handle);
    if (objp == nullptr)
    {
        return;
    }
    map_icon_temp test(objp->pos, handle);
    if (m_temp_icons.find(test, false) > -1)
    {
        audiolib_cue_play(Ga_cue_ids.map_activity_lock, nullptr, nullptr);
        add_player_marker(objp->pos, false, handle, false, false);
    }
}

// 0x0084E510 (:9185)
unsigned int cartographer::get_gps_target_activity_handle()
{
    const bool on_mission_pos = m_mission_gps && m_player_marker_pos.x == m_mission_gps_pos.x && m_player_marker_pos.y == m_mission_gps_pos.y && m_player_marker_pos.z == m_mission_gps_pos.z;
    if (!on_mission_pos && m_has_player_marker)
    {
        return m_player_marker_item.m_act_handle;
    }
    return 0xFFFFFFFF;
}

// 0x0084E590 (:9190)
bool cartographer::can_warp_to_safehouse()
{
    return upgrade_has_safehouse_warp() && gsm_get_alert_level() < 1 && m_num_safehouse > 1 && activity_get_mode() == ACTIVITY_MODE_NONE && !mission_is_active();
}

// 0x0084E5D0 (:9221)
bool cartographer::get_activity_pos(vector2& pos)
{
    if (Most_recent_icon == nullptr)
    {
        return false;
    }
    pos = Most_recent_icon->pos;
    return true;
}

// 0x00864B00 (:9232)
bool cartographer::get_activity_rewards(float& control, float& morale, int& salvage)
{
    morale = 0.0f;
    control = 0.0f;
    salvage = 0;
    for (int i = 0; i < m_temp_icons.size(); i++)
    {
        const map_icon_temp& icon = m_temp_icons[i];
        if (icon.pos.x != m_highlight.m_obj_pos.x || icon.pos.y != m_highlight.m_obj_pos.z)
        {
            continue;
        }
        if (icon.activity_handle == 0xFFFFFFFF || !m_highlight.m_valid || m_map_mode != MAP_MODE_FULL)
        {
            return false;
        }
        const unsigned int handle = icon.activity_handle;
        switch (icon.bitmap)
        {
            case MAP_ELEMENT_CONVOY_ICON:
            case MAP_ELEMENT_CONVOY_CAPTURE_ICON:
            {
                object_convoy_end_point* node = World.object_from_handle<object_convoy_end_point>(handle);
                if (node == nullptr || node->convoy_def == nullptr)
                {
                    return false;
                }
                control = node->convoy_def->standard_rewards.control;
                morale = node->convoy_def->standard_rewards.morale;
                salvage = node->convoy_def->standard_rewards.salvage;
                return true;
            }
            case MAP_ELEMENT_RIDING_SHOTGUN:
            {
                object_riding_shotgun_node* node = World.object_from_handle<object_riding_shotgun_node>(handle);
                if (node == nullptr || node->rs_info == nullptr)
                {
                    return false;
                }
                control = node->rs_info->standard_rewards.control;
                morale = node->rs_info->standard_rewards.morale;
                salvage = node->rs_info->standard_rewards.salvage;
                return true;
            }
            case MAP_ELEMENT_AREA_DEFENSE:
            {
                object_area_defense_node* node = World.object_from_handle<object_area_defense_node>(handle);
                if (node == nullptr || node->ad_info == nullptr)
                {
                    return false;
                }
                control = node->ad_info->standard_rewards.control;
                morale = node->ad_info->standard_rewards.morale;
                salvage = node->ad_info->standard_rewards.salvage;
                return true;
            }
            case MAP_ELEMENT_COURIER_ICON:
            {
                object_courier_end_point* node = World.object_from_handle<object_courier_end_point>(handle);
                if (node == nullptr || node->courier_def == nullptr)
                {
                    return false;
                }
                activity_cc_get_rewards(control, morale);
                salvage = 0;
                return true;
            }
            case MAP_ELEMENT_RAID_ICON:
            {
                object_activity_spawn* spawn = World.object_from_handle<object_activity_spawn>(handle);
                if (spawn == nullptr)
                {
                    return false;
                }
                object* node = spawn->get_activity_node();
                if (node == nullptr || node->get_obj_type() != OT_RAID_NODE || ((object_raid_node*)node)->base_raid_def == nullptr)
                {
                    return false;
                }
                control = ((object_raid_node*)node)->base_raid_def->standard_rewards.control;
                morale = ((object_raid_node*)node)->base_raid_def->standard_rewards.morale;
                salvage = ((object_raid_node*)node)->base_raid_def->standard_rewards.salvage;
                return true;
            }
            case MAP_ELEMENT_RAID_ICON_ALWAYS_SHOW:
            case MAP_ELEMENT_DEF_RAID_ICON:
            {
                object_raid_node* node = World.object_from_handle<object_raid_node>(handle);
                if (node == nullptr || node->base_raid_def == nullptr)
                {
                    return false;
                }
                control = node->base_raid_def->standard_rewards.control;
                morale = node->base_raid_def->standard_rewards.morale;
                salvage = node->base_raid_def->standard_rewards.salvage;
                return true;
            }
            case MAP_ELEMENT_HOUSE_ICON:
            case MAP_ELEMENT_HOUSE_ICON_ALWAYS_SHOW:
            {
                object_house_arrest_node* node;
                if (icon.bitmap == MAP_ELEMENT_HOUSE_ICON)
                {
                    object_activity_spawn* spawn = World.object_from_handle<object_activity_spawn>(house_arrest_get_activity_node_startup_squad(handle));
                    if (spawn == nullptr)
                    {
                        return false;
                    }
                    object* activity_node = spawn->get_activity_node();
                    if (activity_node == nullptr || activity_node->get_obj_type() != OT_HOUSE_ARREST_NODE)
                    {
                        return false;
                    }
                    node = (object_house_arrest_node*)activity_node;
                }
                else
                {
                    node = World.object_from_handle<object_house_arrest_node>(handle);
                    if (node == nullptr)
                    {
                        return false;
                    }
                }
                if (node->base_house_arrest_data == nullptr)
                {
                    return false;
                }
                control = node->base_house_arrest_data->standard_rewards.control;
                morale = node->base_house_arrest_data->standard_rewards.morale;
                salvage = node->base_house_arrest_data->standard_rewards.salvage;
                return true;
            }
            case MAP_ELEMENT_DEMOLITIONS_MASTER:
            {
                object_demolitions_master_node* node = World.object_from_handle<object_demolitions_master_node>(handle);
                if (node == nullptr || node->base_demolitions_master_data == nullptr || node->is_completed())
                {
                    return false;
                }
                control = node->base_demolitions_master_data->standard_rewards.control;
                morale = node->base_demolitions_master_data->standard_rewards.morale;
                salvage = node->base_demolitions_master_data->dm_stats.completed ? 0 : node->base_demolitions_master_data->standard_rewards.salvage;
                return true;
            }
            case MAP_ELEMENT_GUERRILLA_EXPRESS:
            {
                object_delivery_node* node = World.object_from_handle<object_delivery_node>(handle);
                if (node == nullptr || node->get_delivery_def() == nullptr || node->is_completed())
                {
                    return false;
                }
                control = node->get_delivery_def()->standard_rewards.control;
                morale = node->get_delivery_def()->standard_rewards.morale;
                salvage = node->is_completed() ? 0 : node->get_delivery_def()->standard_rewards.salvage;
                return true;
            }
            default:
                return false;
        }
    }
    return false;
}

// 0x0084E600 (:9389)
void cartographer::show_important_bldgs(bool show)
{
    Show_important_bldgs = show;
}

// 0x0084E610 (:9394)
bool cartographer::in_fullscreen_map()
{
    return Will_switch_to_full || m_map_mode == MAP_MODE_FULL;
}

// 0x0083FDE0 (.h:339)
bldg_health_info::bldg_health_info(unsigned int handle, float health_pct)
{
    m_last_health_pct = health_pct;
    m_valid = true;
    m_mover_handle = handle;
    m_time_since_damaged = -1.0f;
}

// 0x0083FE20 (.h:426)
bool cartographer::replace_marker_on_add()
{
    return m_replace_gps_on_add;
}

// 0x0083FE30 (.h:461)
int cartographer::get_num_safehouse()
{
    return m_num_safehouse;
}

// 0x00755050 (.h:462)
unsigned int cartographer::get_nearest_safehouse()
{
    return m_nearest_safehouse;
}

// 0x0083FE40 (.h:762)
float cartographer::get_zoom_radius() const
{
    return m_map_mode == MAP_MODE_MINI ? m_mini_size_info.zoom_radius : m_full_size_info.zoom_radius;
}

// 0x0083FE60 (.h:773)
const highlight_item* cartographer::get_highlight_item()
{
    return m_highlight.m_valid ? &m_highlight : nullptr;
}
