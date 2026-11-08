// Original: volition/rfg/code/hud/fullscreen_map.cpp
//
// The full screen map: 16 x 16 zone images streamed around the view (or one image of the whole territory when zoomed out),
// the fog of war, districts, icons, the player's GPS marker, the selected item's details and the hints.
//
// The activity and district queries it calls are gameflow.md GF3 and GF4's stand-ins.

#include "volition/rfg/code/hud/fullscreen_map.h"

#include "volition/audio_lib/code/audiolib_api.h"
#include "volition/audio_lib/code/audiolib_att_loop_rel.h"
#include "volition/audio_lib/code/audiolib_music.h"
#include "volition/interface_lib/code/bitmap_sheets.h"
#include "volition/interface_lib/code/objects/vint_object_base.h"
#include "volition/interface_lib/code/vint_data_group.h"
#include "volition/interface_lib/code/vint_data_item.h"
#include "volition/interface_lib/code/vint_document.h"
#include "volition/interface_lib/code/vint_external.h"
#include "volition/interface_lib/code/vint_project.h"
#include "volition/interface_lib/code/vint_variant.h"
#include "volition/render_lib/code/gr/gr.h"
#include "volition/render_lib/code/rl_draw.h"
#include "volition/render_lib/code/rl_primitive_defines.h"
#include "volition/render_lib/code/rl_singleton.h"
#include "volition/rfg/code/camera/rfg_camera.h"
#include "volition/rfg/code/control/control_map.h"
#include "volition/rfg/code/gameplay/activities/activity.h"
#include "volition/rfg/code/gameplay/activities/activity_convoy.h"
#include "volition/rfg/code/gameplay/activities/delivery_common.h"
#include "volition/rfg/code/gameplay/difficulty.h"
#include "volition/rfg/code/gameplay/mission_info.h"
#include "volition/rfg/code/gameplay/missions.h"
#include "volition/rfg/code/gameplay/object_district.h"
#include "volition/rfg/code/gamewide/game_clock.h"
#include "volition/rfg/code/gamewide/gameaudio.h"
#include "volition/rfg/code/gamewide/gamemusic.h"
#include "volition/rfg/code/geom/foliage_mesh.h"
#include "volition/rfg/code/hud/fog_of_war.h"
#include "volition/rfg/code/hud/hud.h"
#include "volition/rfg/code/hud/in_world_gps.h"
#include "volition/rfg/code/hud/minimap.h"
#include "volition/rfg/code/hud/minimap_icon_info.h"
#include "volition/rfg/code/hud/objectives.h"
#include "volition/rfg/code/interface/localization/localization_shared.h"
#include "volition/rfg/code/interface/ui_document_hud.h"
#include "volition/rfg/code/interface/ui_document_manager.h"
#include "volition/rfg/code/interface/ui_menu.h"
#include "volition/rfg/code/interface/ui_message_box.h"
#include "volition/rfg/code/level/rfg_zonex.h"
#include "volition/rfg/code/level/territory_shared.h"
#include "volition/rfg/code/level/world.h"
#include "volition/rfg/code/objects/movers/object_mover.h"
#include "volition/rfg/code/objects/object_delivery_node.h"
#include "volition/rfg/code/objects/object_demolitions_master_node.h"
#include "volition/rfg/code/objects/object_mission_start_node.h"
#include "volition/rfg/code/objects/object_safehouse.h"
#include "volition/rfg/code/objects/humans/player.h"
#include "volition/rfg/code/objects/humans/player_meta_data.h"
#include "volition/rfg/code/rfg.h"
#include "volition/rfg/code/util/special_allocators.h"
#include "volition/stream2/code/stream2.h"
#include "volition/vlib/code/bmpman/bmpman.h"
#include "volition/vlib/code/cfile/cfile.h"
#include "volition/vlib/code/input/input.h"
#include "volition/vlib/code/input/mouse.h"
#include "volition/vlib/code/math/bbox.h"
#include "volition/vlib/code/math/floating.h"
#include "volition/vlib/code/math/rand.h"
#include "volition/vlib/code/math/vector2.h"
#include "volition/vlib/code/memory/rfg_split_mempool.h"
#include "volition/vlib/code/memory/split_mempool.h"
#include "volition/vlib/code/memory/virtual_mempool.h"
#include "volition/vlib/code/memory/vmemory.h"
#include "volition/vlib/code/os/frametime.h"
#include "volition/vlib/code/time/timestamp.h"
#include "volition/vlib/code/util/color.h"
#include "volition/vlib/code/util/string_util.h"
#include "volition/vlib/code/util/unicode_tools.h"
#include "volition/vlib/code/util/varray.h"

#include "keen/base/string.hpp"
#include "keen/base/trace.hpp"

#include <math.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include <wchar.h>

static_assert(sizeof(fullscreen) == 0x1CA8, "fullscreen");
static_assert(sizeof(zone_map_information) == 0x18, "zone_map_information");
static_assert(sizeof(stream_information) == 0x10, "stream_information");
static_assert(sizeof(fsm_poi) == 0x23, "fsm_poi");
static_assert(offsetof(fullscreen, map_center) == 0xC0, "fullscreen::map_center");
static_assert(offsetof(fullscreen, stream_information) == 0x194, "fullscreen::stream_information");
static_assert(offsetof(fullscreen, mission_infos) == 0x1B7C, "fullscreen::mission_infos");

// ---- globals
fullscreen Fs_info; // 0x02BD9328
stream2_slot_allocator Fullscreen_map_stream_allocator; // 0x016612AC
bool Fullscreen_map_has_been_opened_through_script; // 0x02B9DD59

// ---- minimap.cpp's, one unity with this file in rfg.exe
extern bool Show_minimap; // 0x01660FF3
extern bool Usability_hud_enable; // 0x01664895
extern bool Will_switch_to_full; // 0x02B9DD48
extern bool Found_icon; // 0x02B9DD49
extern map_icon_temp* Most_recent_icon; // 0x02B9DD50

// ---- file-static globals
static const char* cUI_district_filter = "HM_FSM_BOUNDARIES"; // 0x01660D90
static const char* cUI_district_control = "HM_FSM_CONTROL"; // 0x01660D94
static const char* cUI_district_morale = "HM_FSM_MORALE"; // 0x01660D98
static const char* cUI_district_none = "HM_FSM_NONE"; // 0x01660D9C
static const char* cDistrictHeader = "MENU_DISTRICT"; // 0x01660DA0
static const char* cUI_hint_place_nav_beacon = "HM_FSM_PLACE_NAV"; // 0x01660DA8
static const char* cUI_hint_remove_nav_beacon = "HM_FSM_REMOVE_NAV"; // 0x01660DAC
static const char* cUI_hint_pan = "HM_FSM_PAN"; // 0x01660DB0
static const char* cUI_hint_zoom = "MENU_ZOOM_WITH_BUTTON"; // 0x01660DB4
static const char* cUI_hint_back = "MENU_BACK_WITH_BUTTON"; // 0x01660DB8
static const char* cUI_hint_center = "HM_FSM_CENTER"; // 0x01660DBC
static const char* cUI_fsm_salvage = "MENU_CREDITS"; // 0x01660DC0
static bool Use_full_screen_image = true; // 0x01660DC4
static const char* cFullscreen_map_allocator = "Fullscreen_map_allocator"; // 0x01660DCC
static const char* cUI_fullscreen_doc = "rfg_ui_fullscreen"; // 0x01660DD0
float Scroll_speed = 1.0f; // 0x01660DD4
static float Min_scroll_gravity = 0.2f; // 0x01660DD8
static float Max_scroll_gravity = 0.9f; // 0x01660DDC
static float Lock_target_dist = 4.0f; // 0x01660DE0
static float Full_screen_image_alpha_start = 0.1f; // 0x01660DE4
static float Full_screen_image_zoom_max = 0.205f; // 0x01660DE8
static int Input_id = -1; // 0x01660DEC
static int Msgbox_handle = -1; // 0x01660DF0
static float Scroll_null_zone_sqrd = 0.040000003f; // 0x016612D0
static vector2 View_box(850.0f, 512.0f); // 0x016612D4
static farray<fsm_poi,32> fsm_points_of_interest; // 0x016612E0
static float repeat = 2.0f; // 0x016626DC
static int UV_FLIP_COUNTER = 4; // 0x01660D84
static bool Mouse_has_clicked_hint; // 0x02B9DC06
static mempool_base* Mempool_to_use; // 0x02B9DC08
static int District_alpha; // 0x02B9DC0C
extern bool Zoom_map_out_on_start; // 0x02B9DD58 (minimap.cpp)
float Lock_target_dist_squared = (float)((double)Lock_target_dist * (double)Lock_target_dist); // 0x02BB71F4
static float analog_smoothed; // 0x02BE5830
static float scrolling_time; // 0x02BE5834
static bool scroll_kicked_off; // 0x02BE5838, kicked_off in fsm_handle_scroll_sounds
static bool zoom_in_kicked_off; // 0x02BE5839, kicked_off in fsm_handle_zoom_in_sounds
static bool was_displaying_highlight; // 0x02BE5951

// 0x0087A680 (:387)
void unused_dcf_reload_fsmi()
{
    fullscreen_map_terr_info_init(-1);
}

// 0x0083FE90 (:396)
static void fsm_get_sound_handles()
{
    audiolib_cue_precache(Ga_cue_ids.map_scroll);
    audiolib_cue_precache(Ga_cue_ids.map_zoom_in_attack);
    audiolib_cue_precache(Ga_cue_ids.map_zoom_out_start);
    audiolib_cue_precache(Ga_cue_ids.map_close);
    Fs_info.audio_info.play_static_blink = -1;
    Fs_info.audio_info.play_static_screen = -1;
    Fs_info.audio_info.play_scroll_loop = -1;
    Fs_info.audio_info.play_zoom_in_alr = -1;
    Fs_info.audio_info.play_zoom_out_alr = -1;
}

// 0x0083FEE0 (:415)
// Magic numbers: 16 x 16 zones.
static short fsm_get_stream_information_index(int x, int y)
{
    if ((unsigned int)x < 16 && (unsigned int)y < 16)
    {
        return (short)(y * 16 + x);
    }
    return -1;
}

// 0x0083FF00 (:425)
static unsigned int fsm_get_stream_resource_id(int x, int y)
{
    if ((unsigned int)x < 16 && (unsigned int)y < 16)
    {
        return Fs_info.stream_information[y * 16 + x].srid;
    }
    return 0;
}

// 0x0083FF20 (:439)
static void fsm_stream_request(unsigned int srid)
{
    stream2_request(srid);
}

// 0x0083FF30 (:445)
static void fsm_stream_release(unsigned int srid)
{
    stream2_release(srid);
}

// 0x0083FF40 (:451)
static void fsm_stream_do_frame()
{
}

// 0x0083FF50 (:456)
int fsm_get_selected_district_show_type()
{
    return Fs_info.district_value;
}

// 0x0083FF60 (:462)
bool fsm_is_targeting_player()
{
    return Fs_info.flags.targeting;
}

// 0x00850C30 (:468)
bool fsm_get_district_color(const char* dist_name, unsigned char& r, unsigned char& g, unsigned char& b, unsigned char& a)
{
    // Magic number: 119, the color of a zone with no district.
    r = 119;
    g = 119;
    b = 119;
    a = (unsigned char)District_alpha;
    const unsigned int handle = get_district_by_name(dist_name);
    if (handle != 0xFFFFFFFF)
    {
        district* dp = World.object_from_handle<district>(handle);
        if (dp != nullptr)
        {
            dp->get_grid_color(r, g, b);
            return true;
        }
    }
    return false;
}

// 0x00850C80 (:485)
bool fsm_get_district_color(const char* dist_name, float& r, float& g, float& b, float& a)
{
    r = 119.0f;
    g = 119.0f;
    b = 119.0f;
    a = (float)(unsigned char)District_alpha;
    unsigned char red = 119;
    unsigned char green = 119;
    unsigned char blue = 119;
    const unsigned char alpha = (unsigned char)District_alpha;
    bool found = false;
    const unsigned int handle = get_district_by_name(dist_name);
    if (handle != 0xFFFFFFFF)
    {
        district* dp = World.object_from_handle<district>(handle);
        if (dp != nullptr)
        {
            dp->get_grid_color(red, green, blue);
            found = true;
        }
    }
    // Magic number: 1 / 255.
    r = (float)red * 0.003921569f;
    g = (float)green * 0.003921569f;
    b = (float)blue * 0.003921569f;
    a = (float)alpha * 0.003921569f;
    return found;
}

// 0x0083FF70 (:583)
static const char* fsm_get_bogus_file(int which)
{
    return which != 0 ? "RFG_rock_1024N" : "RFG_rock_1024";
}

// 0x0083FF90 (:594)
static void fsm_clear_zone_map_info(zone_map_information* zmi)
{
    if (zmi != nullptr)
    {
        zmi->fully_obscured_by_fow = false;
        zmi->alpha_not_loaded = 0.5f;
        zmi->stream_state = ZSS_UNLOADED;
        zmi->bitmap_handle = -1;
        zmi->gid.x = 0;
        zmi->gid.y = 0;
    }
}

// 0x00867190 (:608)
static void fsm_init_zone_map_info()
{
    Fs_info.zone_free_info.clear();
    Fs_info.zone_used_info.clear();
    for (int i = 0; i < 77; i++)
    {
        fsm_clear_zone_map_info(&Fs_info.zone_map_info[i]);
        Fs_info.zone_free_info.add(&Fs_info.zone_map_info[i]);
    }
}

// 0x00850D80 (:628)
static bool fsm_is_current_zone_map_info(int x, int y)
{
    for (int i = 0; i < Fs_info.zone_used_info.size(); i++)
    {
        const zone_map_information* zmi = Fs_info.zone_used_info[i];
        if (zmi != nullptr && zmi->gid.x == x && zmi->gid.y == y)
        {
            return true;
        }
    }
    return false;
}

// 0x00850DC0 (:643)
static zone_map_information* fsm_get_free_zmi()
{
    if (Fs_info.zone_free_info.size() == 0)
    {
        return nullptr;
    }
    zone_map_information* zmi = Fs_info.zone_free_info[Fs_info.zone_free_info.size() - 1];
    Fs_info.zone_free_info.remove(Fs_info.zone_free_info.size() - 1);
    return zmi;
}

// The part of fsm_cancel_streaming and fsm_unload_associated_zones rfg.exe inlines in both. True when the zone is free.
static bool fsm_release_used_zone(int index, zone_map_information* zmi)
{
    const unsigned int srid = fsm_get_stream_resource_id(zmi->gid.x, zmi->gid.y);
    if (zmi->stream_state == ZSS_STREAMING || zmi->stream_state == ZSS_LOADED)
    {
        if (zmi->bitmap_handle != -1)
        {
            bm_unload(zmi->bitmap_handle, false);
        }
        // Magic number: srids 0 and 1 mean no image.
        if (srid > 1)
        {
            zmi->stream_state = ZSS_UNLOADING;
            fsm_stream_release(srid);
        }
    }
    if (zmi->stream_state != ZSS_LOADED_BOGUS && stream2_get_status(srid) == ST_UNLOADED)
    {
        zmi->stream_state = ZSS_UNLOADED;
    }
    if (zmi->stream_state != ZSS_UNLOADED && zmi->stream_state != ZSS_LOADED_BOGUS)
    {
        return false;
    }
    Fs_info.zone_used_info.remove(index);
    fsm_clear_zone_map_info(zmi);
    Fs_info.zone_free_info.add(zmi);
    return true;
}

// 0x00867220 (:656)
void fsm_cancel_streaming()
{
    int i = 0;
    while (i < Fs_info.zone_used_info.size())
    {
        zone_map_information* zmi = Fs_info.zone_used_info[i];
        if (zmi == nullptr || !fsm_release_used_zone(i, zmi))
        {
            i++;
        }
    }
    if (!Use_full_screen_image)
    {
        return;
    }
    zone_map_information& image = Fs_info.full_screen_image_info;
    if (image.stream_state == ZSS_STREAMING || image.stream_state == ZSS_LOADED)
    {
        if (image.bitmap_handle != -1)
        {
            bm_unload(image.bitmap_handle, false);
        }
        if (Fs_info.full_screen_image_srid > 1)
        {
            image.stream_state = ZSS_UNLOADING;
            stream2_release(Fs_info.full_screen_image_srid);
        }
    }
    if ((image.stream_state != ZSS_LOADED_BOGUS && stream2_get_status(Fs_info.full_screen_image_srid) == ST_UNLOADED) || image.stream_state == ZSS_UNLOADED || image.stream_state == ZSS_LOADED_BOGUS)
    {
        fsm_clear_zone_map_info(&image);
    }
}

// 0x00867420 (:778)
static void fsm_unload_associated_zones(const farray<vector2i,77>& grid_coords)
{
    int i = 0;
    while (i < Fs_info.zone_used_info.size())
    {
        zone_map_information* zmi = Fs_info.zone_used_info[i];
        bool keep = zmi == nullptr;
        for (int j = 0; j < grid_coords.size() && !keep; j++)
        {
            keep = grid_coords[j].x == zmi->gid.x && grid_coords[j].y == zmi->gid.y;
        }
        if (keep || !fsm_release_used_zone(i, zmi))
        {
            i++;
        }
    }
}

// 0x0083FFC0 (:850)
static unsigned char* fsm_cpu_alloc_cb(int size, int align)
{
    return Mempool_to_use != nullptr ? (unsigned char*)Mempool_to_use->alloc(size, align) : nullptr;
}

// 0x0083FFE0 (:869)
static unsigned char* fsm_gpu_alloc_cb(int size, int align)
{
    // Magic number: 0x40 is the pool's split flag.
    if (Mempool_to_use != nullptr && (Mempool_to_use->get_flags() & 0x40) != 0)
    {
        return (unsigned char*)((split_mempool*)Mempool_to_use)->alloc_gpu(size, align);
    }
    return nullptr;
}

// 0x00840010 (:882)
// Which sides of a zone have no neighbor: 1 left, 2 right, 4 top, 8 bottom.
static void fsm_determine_corner_info(zone_map_information* zmi, const vector2i& gcoord)
{
    if (zmi == nullptr)
    {
        return;
    }
    const short index = fsm_get_stream_information_index(gcoord.x, gcoord.y);
    if (index == -1 || !Fs_info.stream_information[index].exists)
    {
        return;
    }
    const short left = fsm_get_stream_information_index(gcoord.x - 1, gcoord.y);
    const short right = fsm_get_stream_information_index(gcoord.x + 1, gcoord.y);
    const short top = fsm_get_stream_information_index(gcoord.x, gcoord.y - 1);
    const short bottom = fsm_get_stream_information_index(gcoord.x, gcoord.y + 1);
    zmi->corner_info = 0;
    if (left == -1 || !Fs_info.stream_information[left].exists)
    {
        zmi->corner_info |= 1;
    }
    if (right == -1 || !Fs_info.stream_information[right].exists)
    {
        zmi->corner_info |= 2;
    }
    if (top == -1 || !Fs_info.stream_information[top].exists)
    {
        zmi->corner_info |= 4;
    }
    if (bottom == -1 || !Fs_info.stream_information[bottom].exists)
    {
        zmi->corner_info |= 8;
    }
}

// 0x00840150 (:908)
// Samples five by five points. rfg.exe inlines fog_of_war_is_posn_obscured.
static bool fsm_is_box_obscured_by_fow(bbox box)
{
    const float step_x = (box.max.x - box.min.x) / 5.0f;
    const float step_z = (box.max.z - box.min.z) / 5.0f;
    for (float x = box.min.x; x < box.max.x; x += step_x)
    {
        for (float z = box.min.z; z < box.max.z; z += step_z)
        {
            if (!fog_of_war_is_posn_obscured(vector(x, 0.0f, z)))
            {
                return false;
            }
        }
    }
    return true;
}

// 0x008675B0 (:928)
static void fsm_load_associated_zones(const farray<vector2i,77>& gcoords)
{
    if (!Fs_info.flags.allocated)
    {
        return;
    }
    for (int i = 0; i < gcoords.size(); i++)
    {
        const vector2i gid = gcoords[i];
        if (fsm_is_current_zone_map_info(gid.x, gid.y))
        {
            continue;
        }
        zone_map_information* zmi = fsm_get_free_zmi();
        if (zmi == nullptr)
        {
            continue;
        }
        zmi->stream_state = ZSS_STREAMING;
        zmi->alpha_not_loaded = 0.5f;
        zmi->gid = gid;
        zmi->corner_info = 0;
        fsm_determine_corner_info(zmi, gid);
        const unsigned int srid = fsm_get_stream_resource_id(zmi->gid.x, zmi->gid.y);
        if (srid > 1)
        {
            fsm_stream_request(srid);
        }
        else
        {
            zmi->stream_state = ZSS_LOADED_BOGUS;
            zmi->bitmap_handle = bm_load(fsm_get_bogus_file(srid != 0 ? 1 : 0), false);
        }
        vector zone_min;
        vector zone_max;
        rfg_zonex_get_zone_bbox(zmi->gid.x, zmi->gid.y, zone_min, zone_max);
        bbox box;
        box.min = zone_min;
        box.max = zone_max;
        zmi->fully_obscured_by_fow = fsm_is_box_obscured_by_fow(box);
        Fs_info.zone_used_info.add(zmi);
        Fs_info.flags.updated_gids = 1;
    }
}

// 0x008677E0 (:984)
static bool fsm_allocator_setup()
{
    fsm_init_zone_map_info();
    Mempool_to_use = salloc_gameplay_gpu_lock();
    if (Mempool_to_use == nullptr)
    {
        gameseq_pop_state();
        return true;
    }
    Fs_info.inteface_mempool_mark = (int)Interface_mempool.mark();
    Fs_info.cpu_mempool_mark = (int)Mempool_to_use->mark();
    // Magic number: 0x40 is the pool's split flag.
    Fs_info.gpu_mempool_mark = (Mempool_to_use->get_flags() & 0x40) != 0 ? (int)((split_mempool*)Mempool_to_use)->mark_gpu() : 0;
    // Magic numbers: 77 zone images, and one image of the whole territory.
    int brackets[6] = {0x20400, 0, 77, 0xB0000, 0, 1};
    if (Fullscreen_map_stream_allocator.init(fsm_cpu_alloc_cb, 0x800, fsm_gpu_alloc_cb, 0x800, brackets, 2, cFullscreen_map_allocator))
    {
        Fs_info.static_bitmap_id = bm_load("ui_fsmap_static.tga", false);
        Fs_info.corner_bitmap_id = bm_load("ui_fsmap_corner.tga", false);
        const int image_id = vint_bms_load("ui_fsmap_cracks");
        Fs_info.side_image_info = (bm_image*)vint_bms_get_image_data(image_id);
        Fs_info.side_bitmap_id = vint_bms_get_bitmap(image_id);
        return true;
    }
    Fs_info.flags.exiting = 1;
    return false;
}

// 0x00867930 (:1053)
// Moves the zones in view to the end of the list.
static void fsm_order_gids_based_on_visibility(farray<vector2i,77>& gids)
{
    farray<vector2i,77> visible;
    int i = 0;
    while (i < gids.size())
    {
        vector zone_min;
        vector zone_max;
        rfg_zonex_get_zone_bbox(gids[i].x, gids[i].y, zone_min, zone_max);
        if (Fs_info.current_view_bbox.max.x < zone_min.x || zone_max.x < Fs_info.current_view_bbox.min.x || Fs_info.current_view_bbox.max.z < zone_min.z || zone_max.z < Fs_info.current_view_bbox.min.z)
        {
            i++;
        }
        else
        {
            visible.add(gids[i]);
            gids.remove(i);
        }
    }
    for (int j = 0; j < visible.size(); j++)
    {
        gids.add(visible[j]);
    }
}

// 0x00867AF0 (:1085)
static void fsm_determine_zones_to_load()
{
    Fs_info.flags.updated_gids = 0;
    farray<vector2i,77> grid_coords;
    // Magic numbers: a window of 5110 x 3066 m around the center, sampled every 511 m, the zone's size.
    float start_x = Fs_info.map_center.x - 2299.5f;
    if (Fs_info.world_extents_bbox.min.x > start_x)
    {
        start_x = Fs_info.world_extents_bbox.min.x;
    }
    else if (Fs_info.world_extents_bbox.max.x < start_x + 5110.0f)
    {
        start_x = Fs_info.world_extents_bbox.max.x - 5110.0f;
    }
    for (int x = 0; x < 5110; x += 511)
    {
        const float pos_x = (float)x + start_x;
        float start_z = Fs_info.map_center.z - 1277.5f;
        if (Fs_info.world_extents_bbox.min.z > start_z)
        {
            start_z = Fs_info.world_extents_bbox.min.z;
        }
        else if (Fs_info.world_extents_bbox.max.z < start_z + 3066.0f)
        {
            // Original bug, kept: the limit is 2810.5 where the test uses 3066 (a half zone less).
            start_z = Fs_info.world_extents_bbox.max.z - 2810.5f;
        }
        for (int z = 0; z < 3066; z += 511)
        {
            vector pos(pos_x, Fs_info.map_center.y, (float)z + start_z);
            int grid_x;
            int grid_y;
            if (rfg_zonex_get_grid_coords(pos, grid_x, grid_y))
            {
                vector2i gid;
                gid.x = grid_x;
                gid.y = grid_y;
                grid_coords.add(gid);
            }
        }
    }
    fsm_unload_associated_zones(grid_coords);
    fsm_load_associated_zones(grid_coords);
    fsm_order_gids_based_on_visibility(grid_coords);
}

// 0x00850E00 (:1165)
static bool fsm_should_remove_player_marker()
{
    if (!Cartographer.m_mission_gps || Cartographer.m_player_marker_pos.x != Cartographer.m_mission_gps_pos.x || Cartographer.m_player_marker_pos.y != Cartographer.m_mission_gps_pos.y || Cartographer.m_player_marker_pos.z != Cartographer.m_mission_gps_pos.z)
    {
        if (!Cartographer.m_has_player_marker)
        {
            return false;
        }
        if (!Cartographer.m_replace_gps_on_add && !Cartographer.m_highlight.m_valid)
        {
            return true;
        }
    }
    return Cartographer.m_has_player_marker && Cartographer.m_highlight.m_valid && Cartographer.m_player_marker_item.m_obj_pos.x == Cartographer.m_highlight.m_obj_pos.x && Cartographer.m_player_marker_item.m_obj_pos.y == Cartographer.m_highlight.m_obj_pos.y && Cartographer.m_player_marker_item.m_obj_pos.z == Cartographer.m_highlight.m_obj_pos.z;
}

// 0x0087ECD0 (:1171)
static void fsm_determine_player_marker()
{
    // Magic number: ui button 31 places a marker, as does a release of the right mouse button.
    if ((ui_get_button_info(31, Input_id) <= 0.0f || mouse_is_pressed(2)) && !mouse_just_released(2))
    {
        return;
    }
    const bool had_marker = Cartographer.m_has_player_marker;
    if (!had_marker || !Cartographer.m_mission_gps || Cartographer.m_player_marker_pos.x != Cartographer.m_mission_gps_pos.x || Cartographer.m_player_marker_pos.y != Cartographer.m_mission_gps_pos.y || Cartographer.m_player_marker_pos.z != Cartographer.m_mission_gps_pos.z)
    {
        if (fsm_should_remove_player_marker())
        {
            if (had_marker)
            {
                audiolib_cue_play(Ga_cue_ids.map_gps_point_removed, nullptr, nullptr);
            }
            Cartographer.m_has_player_marker = false;
            Cartographer.m_player_marker_item.m_valid = false;
            Cartographer.m_player_marker_item.m_obj_handle = 0xFFFFFFFF;
            Cartographer.m_player_marker_item.m_act_handle = 0xFFFFFFFF;
            Cartographer.m_gps_path.pts.clear();
            // rfg.exe inlines this with its scratch mark scope.
            in_world_gps_register_path(nullptr);
            if (Cartographer.m_mission_gps && mission_is_active())
            {
                Cartographer.add_player_marker(Cartographer.m_mission_gps_pos, true, 0xFFFFFFFF, false, false);
            }
        }
        else
        {
            const vector position = Fs_info.flags.targeting ? Fs_info.target_position : Fs_info.map_center;
            if (!World.is_zone_border_zone(position) && (Cartographer.m_highlight.m_valid || !fog_of_war_is_posn_obscured(position)))
            {
                Cartographer.add_player_marker(position, false, 0xFFFFFFFF, true, false);
            }
            else
            {
                fsm_display_gps_warning("HM_FSM_GPS_DEFAULT_CAPTION", "HM_FSM_GPS_DEFAULT_DESC");
                ui_menu_play_sound(MENU_SOUND_INVALID_INPUT);
            }
        }
    }
    ui_wait_for_button_ups();
}

// 0x008402F0 (:1205)
static void fsm_determine_district_filters(float dtime)
{
    bool clicked = false;
    vint_document* doc = ui_get_vint_document(Fs_info.doc_handle);
    if (doc == nullptr)
    {
        return;
    }
    static const char* kHintExit = "hint_group5"; // 0x016621F8
    vint_object_base* hint = vint_object_base::find_object(kHintExit, doc);
    if (hint != nullptr)
    {
        if (ui_is_mouse_over_vint_object(hint))
        {
            ui_pc_set_object_opacity(hint, 1.0f);
            if (mouse_just_released(0))
            {
                Mouse_has_clicked_hint = true;
                clicked = true;
            }
        }
        else
        {
            ui_pc_set_object_opacity(hint, 0.5f);
        }
    }
    // Magic number: ui button 14 recenters on the player.
    if ((ui_get_button_info(14, Input_id) != 0.0f || clicked) && !Fs_info.flags.dont_center)
    {
        Fs_info.flags.targeting = 1;
        Fs_info.target_position = Rfg_camera.real_pos;
    }
    if (!Fs_info.flags.targeting)
    {
        return;
    }
    const float step = Scroll_speed * 200.0f * dtime; // Magic number: 200 m/s.
    const float dx = Fs_info.target_position.x - Fs_info.map_center.x;
    const float dz = Fs_info.target_position.z - Fs_info.map_center.z;
    bool reached_x = false;
    bool reached_z = false;
    if (fabsf(dx) <= step)
    {
        Fs_info.map_center.x += dx;
        reached_x = true;
    }
    else if (dx >= 0.0f)
    {
        Fs_info.map_center.x += step;
    }
    else
    {
        Fs_info.map_center.x -= step;
    }
    if (fabsf(dz) <= step)
    {
        Fs_info.map_center.z += dz;
        reached_z = true;
    }
    else if (dz >= 0.0f)
    {
        Fs_info.map_center.z += step;
    }
    else
    {
        Fs_info.map_center.z -= step;
    }
    if (reached_x && reached_z)
    {
        Fs_info.flags.targeting = 0;
        Fs_info.map_center.x = Fs_info.target_position.x;
        Fs_info.map_center.y = 0.0f;
        Fs_info.map_center.z = Fs_info.target_position.z;
    }
}

// 0x00840540 (:1286)
static void fsm_determine_zoom_level(float dtime)
{
    // Magic numbers: the zoom runs from 0.1 to 0.8, half a unit a second; input under 0.2 is ignored.
    if (Zoom_map_out_on_start)
    {
        Fs_info.zoom_level -= dtime * 0.5f;
        if (Fs_info.zoom_level < 0.1f)
        {
            Fs_info.zoom_level = 0.1f;
            Zoom_map_out_on_start = false;
        }
        return;
    }
    const float input = controls_get_analog(CAA_ZOOM_IN_OUT, Player_controller_index);
    float zoom_input = input;
    if (controls_get_current_config_setting() == CCS_KEYBOARD_MOUSE || controls_get_current_config_setting() == CCS_STEAM)
    {
        // The wheel gives steps; this smooths them.
        analog_smoothed *= 0.8f;
        if (fabsf(analog_smoothed) < 0.2f)
        {
            analog_smoothed = 0.0f;
        }
        if (input != 0.0f)
        {
            analog_smoothed += input;
        }
        zoom_input = analog_smoothed;
    }
    Fs_info.flags.zooming_out = 0;
    Fs_info.flags.zooming_in = 0;
    if (zoom_input > 0.2f)
    {
        Fs_info.flags.targeting = 0;
        Fs_info.zoom_level = (dtime * 0.5f * zoom_input) / 0.8f + Fs_info.zoom_level;
        if (Fs_info.zoom_level <= 0.8f)
        {
            Fs_info.flags.zooming_in = 1;
        }
        else
        {
            Fs_info.zoom_level = 0.8f;
        }
    }
    else if (zoom_input < -0.2f)
    {
        Fs_info.flags.targeting = 0;
        Fs_info.zoom_level -= (dtime * 0.5f * zoom_input) / -0.8f;
        if (Fs_info.zoom_level < 0.1f)
        {
            Fs_info.zoom_level = 0.1f;
        }
        else
        {
            Fs_info.flags.zooming_out = 1;
        }
    }
}

// 0x00840770 (:1361)
static void fsm_get_world_position(vector2& world_pos, const vector2& screen_pos)
{
    const float z = (Fs_info.current_view_bbox.max.z - Fs_info.current_view_bbox.min.z) * (1.0f - screen_pos.y / (Fs_info.map_render_window.max.y - Fs_info.map_render_window.min.y)) + Fs_info.current_view_bbox.min.z;
    world_pos.x = (Fs_info.current_view_bbox.max.x - Fs_info.current_view_bbox.min.x) * (screen_pos.x / (Fs_info.map_render_window.max.x - Fs_info.map_render_window.min.x)) + Fs_info.current_view_bbox.min.x;
    world_pos.y = z;
}

// 0x00850F00 (:1375)
static void fsm_determine_map_posn(float dtime)
{
    const float pan_x = controls_get_analog(CAA_MAP_PAN_CAMERA_LEFT_RIGHT, Player_controller_index);
    const float pan_z = controls_get_analog(CAA_MAP_PAN_CAMERA_UP_DOWN, Player_controller_index);
    const float frames = (dtime / 0.016666668f) * Scroll_speed; // Magic number: frames at 60 Hz.
    if ((mouse_just_released(0) || mouse_just_released(2)) && !Mouse_has_clicked_hint)
    {
        unsigned int mouse_x;
        unsigned int mouse_y;
        int wheel;
        mouse_get_location(mouse_x, mouse_y, wheel);
        Fs_info.target_position.x = (Fs_info.current_view_bbox.max.x - Fs_info.current_view_bbox.min.x) * ((float)mouse_x / (Fs_info.map_render_window.max.x - Fs_info.map_render_window.min.x)) + Fs_info.current_view_bbox.min.x;
        Fs_info.target_position.z = (Fs_info.current_view_bbox.max.z - Fs_info.current_view_bbox.min.z) * (1.0f - (float)mouse_y / (Fs_info.map_render_window.max.y - Fs_info.map_render_window.min.y)) + Fs_info.current_view_bbox.min.z;
        if (Fs_info.target_position.x < Fs_info.world_extents_bbox.min.x)
        {
            Fs_info.target_position.x = Fs_info.world_extents_bbox.min.x;
        }
        if (Fs_info.target_position.z < Fs_info.world_extents_bbox.min.z)
        {
            Fs_info.target_position.z = Fs_info.world_extents_bbox.min.z;
        }
        if (Fs_info.world_extents_bbox.max.x < Fs_info.target_position.x)
        {
            Fs_info.target_position.x = Fs_info.world_extents_bbox.max.x;
        }
        if (Fs_info.world_extents_bbox.max.z < Fs_info.target_position.z)
        {
            Fs_info.target_position.z = Fs_info.world_extents_bbox.max.z;
        }
        Fs_info.flags.targeting = 1;
        return;
    }
    // Magic number: 0.8, what the stick is divided by here and in fsm_determine_zoom_level.
    vector move((frames * pan_x) / 0.8f, 0.0f, (frames * pan_z) / 0.8f);
    const float scroll_x = (Scroll_speed * pan_x) / 0.8f;
    const float scroll_z = (Scroll_speed * pan_z) / 0.8f;
    const float scroll_squared = scroll_x * scroll_x + 0.0f + scroll_z * scroll_z;
    Fs_info.flags.dont_center = 0;
    Fs_info.flags.scrolling = 0;
    const highlight_item* item = Cartographer.m_highlight.m_valid ? &Cartographer.m_highlight : nullptr;
    const float old_x = Fs_info.map_center.x;
    const float old_z = Fs_info.map_center.z;
    bool keep_temp_center = false;
    if (scroll_squared <= Scroll_null_zone_sqrd)
    {
        if (item == nullptr)
        {
            keep_temp_center = true;
        }
        else
        {
            // Magic number: the pull fades over 30 m (900 is that squared).
            const float pull = ((1.0f - item->m_dsqr / 900.0f) * (Max_scroll_gravity - Min_scroll_gravity) + Min_scroll_gravity) * frames;
            if (fsm_posn_is_locked_on(item->m_fake_pos))
            {
                Fs_info.map_center = item->m_fake_pos;
                Fs_info.temp_center.set(move.x + Fs_info.map_center.x, Fs_info.map_center.y + 0.0f, move.z + Fs_info.map_center.z);
                keep_temp_center = true;
            }
            else
            {
                const vector to_item = item->m_fake_pos - Fs_info.map_center;
                const vector direction = to_item.copy_normalize_safe();
                move.x += direction.x * pull;
                move.y += direction.y * pull;
                move.z += direction.z * pull;
                const vector from_item = Fs_info.map_center - item->m_fake_pos;
                if (direction * move <= from_item.mag() || move.copy_normalize() * direction <= 0.0f)
                {
                    Fs_info.map_center += move;
                }
                else
                {
                    Fs_info.map_center = item->m_fake_pos;
                }
            }
        }
    }
    else
    {
        Fs_info.flags.dont_center = 1;
        Fs_info.flags.targeting = 0;
        Fs_info.flags.scrolling = 0;
        const float scroll = sqrtf(scroll_squared);
        // Magic number: 2.81, faster than this and the item does not pull at all.
        if (item != nullptr && scroll <= 2.81f)
        {
            float pull = ((1.0f - item->m_dsqr / 900.0f) * (Max_scroll_gravity - Min_scroll_gravity) + Min_scroll_gravity) * frames;
            if (fsm_posn_is_locked_on(item->m_fake_pos))
            {
                Fs_info.map_center = item->m_fake_pos;
                Fs_info.temp_center.set(move.x + Fs_info.map_center.x, Fs_info.map_center.y + 0.0f, move.z + Fs_info.map_center.z);
                keep_temp_center = true;
            }
            else
            {
                const vector to_item = item->m_fake_pos - Fs_info.map_center;
                const vector direction = to_item.copy_normalize_safe();
                if (scroll < pull)
                {
                    pull = scroll;
                }
                move.x += direction.x * pull;
                move.y += direction.y * pull;
                move.z += direction.z * pull;
                const vector from_item = Fs_info.map_center - item->m_fake_pos;
                if (direction * move <= from_item.mag() || move.copy_normalize() * direction <= 0.0f)
                {
                    Fs_info.map_center += move;
                }
                else
                {
                    Fs_info.map_center = item->m_fake_pos;
                }
            }
        }
        else
        {
            Fs_info.map_center.x += move.x;
            Fs_info.map_center.y += 0.0f;
            Fs_info.map_center.z += move.z;
        }
    }
    if (!keep_temp_center)
    {
        Fs_info.temp_center = Fs_info.map_center;
    }
    if (Fs_info.map_center.x < Fs_info.world_extents_bbox.min.x)
    {
        Fs_info.map_center.x = Fs_info.world_extents_bbox.min.x;
    }
    if (Fs_info.map_center.z < Fs_info.world_extents_bbox.min.z)
    {
        Fs_info.map_center.z = Fs_info.world_extents_bbox.min.z;
    }
    if (Fs_info.world_extents_bbox.max.x < Fs_info.map_center.x)
    {
        Fs_info.map_center.x = Fs_info.world_extents_bbox.max.x;
    }
    if (Fs_info.world_extents_bbox.max.z < Fs_info.map_center.z)
    {
        Fs_info.map_center.z = Fs_info.world_extents_bbox.max.z;
    }
    if (fabsf(Fs_info.map_center.x - old_x) > 0.0f || fabsf(Fs_info.map_center.z - old_z) > 0.0f)
    {
        if (item != nullptr && fsm_posn_is_locked_on(item->m_fake_pos))
        {
            return;
        }
        Fs_info.flags.scrolling = 1;
    }
}

// 0x00840870 (:1512)
static void fsm_determine_view_extents()
{
    const float half_x = (View_box.x / Fs_info.zoom_level) * 0.5f;
    const float half_z = (View_box.y / Fs_info.zoom_level) * 0.5f;
    Fs_info.current_view_bbox.max.x = Fs_info.map_center.x + half_x;
    Fs_info.current_view_bbox.min.x = Fs_info.map_center.x - half_x;
    Fs_info.current_view_bbox.min.z = Fs_info.map_center.z - half_z;
    Fs_info.current_view_bbox.max.z = Fs_info.map_center.z + half_z;
}

// 0x00851D10 (:1525)
// The asm has dropped is_full; tl and tr come in registers. static_bitmap_flip picks one of four ways to lay the noise over
// the corners.
static void fsm_render_static_bitmap(bool is_full, const vector2& bl, const vector2& br, const vector2& tl, const vector2& tr, float alpha)
{
    (void)is_full;
    static rl_color_float col_to_use(0.6509804f, 0.4627451f, 0.29803923f, 1.0f); // 0x02BE5894
    if (Fs_info.static_bitmap_id == -1)
    {
        gr_set_color(0, 0, 0, 255);
        gr_rect(tl.x, tl.y, tr.x - tl.x, bl.y - tl.y, GM_FILTER);
        return;
    }
    const float scale_x = 1.0f / (Fs_info.map_render_window.max.x - Fs_info.map_render_window.min.x);
    const float scale_y = 1.0f / (Fs_info.map_render_window.max.y - Fs_info.map_render_window.min.y);
    const double alpha_255 = (double)alpha * 255.0;
    gr_set_color((int)(col_to_use.red * 255.0f), (int)(col_to_use.green * 255.0f), (int)(col_to_use.blue * 255.0f), (int)alpha_255);
    rl_draw* draw = rl_draw::get_instance();
    draw->set_texture(Fs_info.static_bitmap_id);
    gr_state state(GM_FILTER_WRAP);
    rl_primitive_state rl_state;
    gr_convert_to_rl_state(rl_state, state);
    draw->tristrip_2d_begin(rl_state);
    rl_primitive_2d_vert* verts = draw->tristrip_2d_allocate_tris(2);
    if (verts == nullptr)
    {
        return;
    }
    verts[3].color.set((unsigned char)(int)(col_to_use.red * 255.0f + 0.5f), (unsigned char)(int)(col_to_use.green * 255.0f + 0.5f), (unsigned char)(int)(col_to_use.blue * 255.0f + 0.5f), (unsigned char)(int)(alpha_255 + 0.5));
    verts[2].color = verts[3].color;
    verts[1].color = verts[3].color;
    verts[0].color = verts[3].color;
    const float window_max_x = Fs_info.map_render_window.max.x;
    const float window_max_y = Fs_info.map_render_window.max.y;
    const float repeats = repeat;
    const vector2* corners[4];
    bool swap_axes;
    bool mirror;
    const int flip = Fs_info.static_bitmap_flip / UV_FLIP_COUNTER;
    if (flip == 0)
    {
        corners[0] = &bl;
        corners[1] = &br;
        corners[2] = &tl;
        corners[3] = &tr;
        swap_axes = false;
        mirror = false;
    }
    else if (flip == 1)
    {
        corners[0] = &tl;
        corners[1] = &tr;
        corners[2] = &bl;
        corners[3] = &br;
        swap_axes = true;
        mirror = true;
    }
    else if (flip == 2)
    {
        corners[0] = &bl;
        corners[1] = &br;
        corners[2] = &tl;
        corners[3] = &tr;
        swap_axes = false;
        mirror = true;
    }
    else
    {
        corners[0] = &tl;
        corners[1] = &tr;
        corners[2] = &bl;
        corners[3] = &br;
        swap_axes = true;
        mirror = false;
    }
    for (int i = 0; i < 4; i++)
    {
        float s = 1.0f - (window_max_x - corners[i]->x) * scale_x;
        float t = 1.0f - (window_max_y - corners[i]->y) * scale_y;
        if (mirror)
        {
            s = 1.0f - s;
            t = 1.0f - t;
        }
        verts[i].uvs.x = (swap_axes ? t : s) * repeats;
        verts[i].uvs.y = (swap_axes ? s : t) * repeats;
    }
    verts[0].pt = tl;
    verts[1].pt = tr;
    verts[2].pt = bl;
    verts[3].pt = br;
    draw->tristrip_2d_end();
}

// 0x00840930 (:1636)
static void fsm_play_static_screen_sound()
{
}

// 0x00840940 (:1656)
static void fsm_stop_static_screen_sound()
{
}

// 0x00840950 (:1666)
// The asm has dropped dtime, it reads Frametime_real.
static void fsm_handle_scroll_sounds(float dtime, bool force_stop)
{
    (void)dtime;
    if (!force_stop && (Fs_info.flags.scrolling || Fs_info.flags.targeting) && !Fs_info.flags.wants_exit && !Fs_info.flags.exiting)
    {
        if (!scroll_kicked_off && Fs_info.audio_info.play_scroll_loop == -1)
        {
            scrolling_time = Frametime_real + scrolling_time;
            if (scrolling_time >= 0.175f) // Magic number: Min_scroll_for_sound, 0x012FAFFC.
            {
                scroll_kicked_off = true;
                audiolib_att_loop_rel_start(Ga_cue_ids.map_scroll, &Fs_info.audio_info.play_scroll_loop, false, nullptr, nullptr);
            }
        }
    }
    else
    {
        scrolling_time = 0.0f;
        if (Fs_info.audio_info.play_scroll_loop != -1)
        {
            audiolib_att_loop_rel_stop(Fs_info.audio_info.play_scroll_loop, 100);
            Fs_info.audio_info.play_scroll_loop = -1;
            scroll_kicked_off = false;
        }
    }
}

// 0x00840A10 (:1703)
static void fsm_handle_zoom_in_sounds(bool force_stop)
{
    if (force_stop || !Fs_info.flags.zooming_in || Fs_info.flags.wants_exit || Fs_info.flags.exiting)
    {
        if (Fs_info.audio_info.play_zoom_in_alr != -1)
        {
            audiolib_att_loop_rel_stop(Fs_info.audio_info.play_zoom_in_alr, 100);
            Fs_info.audio_info.play_zoom_in_alr = -1;
            zoom_in_kicked_off = false;
        }
    }
    else if (Fs_info.audio_info.play_zoom_in_alr == -1)
    {
        zoom_in_kicked_off = true;
        audiolib_att_loop_rel_start(Ga_cue_ids.map_zoom_in_attack, &Fs_info.audio_info.play_zoom_in_alr, false, nullptr, nullptr);
    }
}

// 0x00840A80 (:1727)
static void fsm_handle_zoom_out_sounds(bool force_stop)
{
    if (force_stop || !Fs_info.flags.zooming_out || Fs_info.flags.wants_exit || Fs_info.flags.exiting)
    {
        if (Fs_info.audio_info.play_zoom_out_alr != -1)
        {
            audiolib_att_loop_rel_stop(Fs_info.audio_info.play_zoom_out_alr, 100);
            Fs_info.audio_info.play_zoom_out_alr = -1;
        }
    }
    else if (Fs_info.audio_info.play_zoom_out_alr == -1)
    {
        audiolib_att_loop_rel_start(Ga_cue_ids.map_zoom_out_start, &Fs_info.audio_info.play_zoom_out_alr, false, nullptr, nullptr);
    }
}

// 0x00840AE0 (:1751)
static void fsm_handle_single_static_sounds()
{
}

// 0x00840AF0 (:1775)
static void fsm_handle_sounds(float dtime)
{
    if (Fs_info.flags.loaded_one_in && !Fs_info.flags.exiting)
    {
        fsm_handle_scroll_sounds(dtime, false);
        fsm_handle_zoom_in_sounds(false);
        fsm_handle_zoom_out_sounds(false);
    }
}

// 0x00840BD0 (:1796)
static void fsm_stop_all_sounds(bool force_stop)
{
    fsm_handle_scroll_sounds(0.0f, force_stop);
    fsm_handle_zoom_in_sounds(force_stop);
    fsm_handle_zoom_out_sounds(force_stop);
}

// 0x008527C0 (:1808)
// The cracks along the sides of the map with no neighbor, and the corners where two such sides meet.
static void fsm_render_corner_info(zone_map_information* zmi, float ratio_width, float ratio_height)
{
    const int index = fsm_get_stream_information_index(zmi->gid.x, zmi->gid.y);
    vector zone_min;
    vector zone_max;
    rfg_zonex_get_zone_bbox(zmi->gid.x, zmi->gid.y, zone_min, zone_max);
    // Original bug, kept: the window's min.x and min.y are added unevenly here. They are 0 while rfg.exe runs.
    const float left = Fs_info.map_render_window.min.x + (zone_min.x - Fs_info.current_view_bbox.min.x) * ratio_width + Fs_info.map_render_window.min.x;
    const float right = (zone_max.x - Fs_info.current_view_bbox.min.x) * ratio_width;
    const float top = (Fs_info.current_view_bbox.max.z - zone_max.z) * ratio_height;
    const float bottom = (Fs_info.current_view_bbox.max.z - zone_min.z) * ratio_height;
    const float top_right = Fs_info.map_render_window.min.y + top;
    const float bottom_right = bottom + Fs_info.map_render_window.min.y;
    const float width = right - left;
    const float height = bottom - top;
    rl_primitive_state state;
    gr_convert_to_rl_state(state, GM_FILTER);
    const unsigned char alpha = (unsigned char)(int)((1.0f - zmi->alpha_not_loaded) * 255.0f + 0.5f);
    int bitmap_w = 32; // Magic number: the starting value the asm gives both before asking bmpman.
    int bitmap_h = 32;
    float scale_u;
    float scale_v;
    // Magic numbers: 128 / 1024 of the zone per unit of corner_radius.
    float radius = Fs_info.stream_information[index].corner_radius * 128.0f * 0.0009765625f;
    bm_get_dimensions(Fs_info.side_bitmap_id, bitmap_w, bitmap_h, scale_u, scale_v);
    scale_u = (1.0f / (float)bitmap_w) / scale_u;
    scale_v = (1.0f / (float)bitmap_h) / scale_v;
    const bm_image* image = Fs_info.side_image_info;
    const float u0 = scale_u * (float)(unsigned int)image->sx;
    const float v0 = scale_v * (float)(unsigned int)image->sy;
    const float u1 = (float)(unsigned int)(image->bmw + image->sx) * scale_u;
    const float v1 = (float)(unsigned int)(image->bmh + image->sy) * scale_v;
    rl_draw* draw = rl_draw::get_instance();
    draw->set_texture(Fs_info.side_bitmap_id);
    // Magic numbers: how far the cracks reach past the zone along and across their side, in world units.
    const float long_x = ratio_width * 57.0f;
    const float short_x = ratio_width * 21.0f;
    const float long_y = ratio_height * 57.0f;
    const float short_y = ratio_height * 21.0f;
    for (int i = 0; i < 4; i++)
    {
        const int side = 1 << i;
        if ((zmi->corner_info & side) == 0)
        {
            continue;
        }
        draw->tristrip_2d_begin(state);
        rl_primitive_2d_vert* verts = draw->tristrip_2d_allocate_tris(2);
        if (verts == nullptr)
        {
            return;
        }
        verts[3].color.set(255, 255, 255, alpha);
        verts[2].color = verts[3].color;
        verts[1].color = verts[3].color;
        verts[0].color = verts[3].color;
        switch (side)
        {
        case 1:
            verts[1].uvs.y = v0;
            verts[1].uvs.x = u0;
            verts[0].uvs.y = v0;
            verts[0].uvs.x = u1;
            verts[3].uvs.y = v1;
            verts[3].uvs.x = u0;
            verts[2].uvs.y = v1;
            verts[2].uvs.x = u1;
            verts[0].pt.x = left - short_x;
            verts[0].pt.y = long_y + bottom;
            verts[1].pt.x = left + radius * width;
            verts[1].pt.y = long_y + bottom_right;
            verts[2].pt.x = left - short_x;
            verts[2].pt.y = top - long_y;
            verts[3].pt.x = left + radius * width;
            verts[3].pt.y = top_right - long_y;
            break;
        case 2:
            verts[0].uvs.y = v0;
            verts[0].uvs.x = u0;
            verts[1].uvs.y = v0;
            verts[1].uvs.x = u1;
            verts[2].uvs.y = v1;
            verts[2].uvs.x = u0;
            verts[3].uvs.y = v1;
            verts[3].uvs.x = u1;
            verts[0].pt.x = right - radius * width;
            verts[0].pt.y = long_y + bottom;
            verts[1].pt.x = short_x + right;
            verts[1].pt.y = long_y + bottom_right;
            verts[2].pt.x = right - radius * width;
            verts[2].pt.y = top - long_y;
            verts[3].pt.x = short_x + right;
            verts[3].pt.y = top_right - long_y;
            break;
        case 4:
            verts[1].uvs.y = v0;
            verts[1].uvs.x = u0;
            verts[3].uvs.y = v0;
            verts[3].uvs.x = u1;
            verts[0].uvs.y = v1;
            verts[0].uvs.x = u0;
            verts[2].uvs.y = v1;
            verts[2].uvs.x = u1;
            verts[0].pt.x = left - long_x;
            verts[0].pt.y = top + radius * height;
            verts[1].pt.y = top_right + radius * height;
            verts[1].pt.x = long_x + right;
            verts[2].pt.x = left - long_x;
            verts[2].pt.y = top - short_y;
            verts[3].pt.x = long_x + right;
            verts[3].pt.y = top_right - short_y;
            break;
        case 8:
            verts[2].uvs.x = u0;
            verts[2].uvs.y = v0;
            verts[0].uvs.y = v0;
            verts[0].uvs.x = u1;
            verts[3].uvs.x = u0;
            verts[3].uvs.y = v1;
            verts[1].uvs.y = v1;
            verts[1].uvs.x = u1;
            verts[0].pt.x = left - long_x;
            verts[0].pt.y = short_y + bottom;
            verts[1].pt.y = short_y + bottom_right;
            verts[1].pt.x = long_x + right;
            verts[2].pt.x = left - long_x;
            verts[2].pt.y = bottom - radius * height;
            verts[3].pt.x = long_x + right;
            verts[3].pt.y = bottom_right - radius * height;
            break;
        default:
            break;
        }
        draw->tristrip_2d_end();
    }
    draw->set_texture(Fs_info.corner_bitmap_id);
    radius *= 3.3f; // Magic number: the corner piece's size against the cracks.
    const float low = 0.05f; // Uv_offset, 0x012FB14C.
    const float high = 0.95f;
    for (int i = 0; i < 2; i++)
    {
        const int x_side = 1 << i;
        if ((zmi->corner_info & x_side) == 0)
        {
            continue;
        }
        for (int j = 2; j < 4; j++)
        {
            const int y_side = 1 << j;
            if ((zmi->corner_info & y_side) == 0)
            {
                continue;
            }
            draw->tristrip_2d_begin(state);
            rl_primitive_2d_vert* verts = draw->tristrip_2d_allocate_tris(2);
            if (verts == nullptr)
            {
                return;
            }
            verts[3].color.set(255, 255, 255, alpha);
            verts[2].color = verts[3].color;
            verts[1].color = verts[3].color;
            verts[0].color = verts[3].color;
            if (x_side == 1)
            {
                if (y_side == 4)
                {
                    verts[0].uvs.x = 0.0f; // Magic number: 0, not low, in one vertex of each corner; so the asm has it.
                    verts[0].uvs.y = high;
                    verts[1].uvs.x = high;
                    verts[1].uvs.y = high;
                    verts[2].uvs.x = low;
                    verts[2].uvs.y = low;
                    verts[3].uvs.y = low;
                    verts[3].uvs.x = high;
                    verts[0].pt.x = left;
                    verts[0].pt.y = top + radius * height;
                    verts[1].pt.x = left + radius * width;
                    verts[1].pt.y = top_right + radius * height;
                    verts[2].pt.x = left;
                    verts[2].pt.y = top;
                    verts[3].pt.x = left + radius * width;
                    verts[3].pt.y = top_right;
                }
                else
                {
                    verts[2].uvs.x = 0.0f;
                    verts[2].uvs.y = high;
                    verts[3].uvs.x = high;
                    verts[3].uvs.y = high;
                    verts[0].uvs.x = low;
                    verts[0].uvs.y = low;
                    verts[1].uvs.y = low;
                    verts[1].uvs.x = high;
                    verts[0].pt.y = bottom;
                    verts[0].pt.x = left;
                    verts[1].pt.x = left + radius * width;
                    verts[1].pt.y = bottom_right;
                    verts[2].pt.y = bottom - radius * height;
                    verts[2].pt.x = left;
                    verts[3].pt.x = left + radius * width;
                    verts[3].pt.y = bottom_right - radius * height;
                }
            }
            else if (x_side == 2)
            {
                const float inner_x = right - radius * width;
                if (y_side != 4)
                {
                    verts[3].uvs.x = 0.0f;
                    verts[3].uvs.y = high;
                    verts[2].uvs.x = high;
                    verts[2].uvs.y = high;
                    verts[1].uvs.x = low;
                    verts[1].uvs.y = low;
                    verts[0].uvs.y = low;
                    verts[0].uvs.x = high;
                    verts[0].pt.x = inner_x;
                    verts[0].pt.y = bottom;
                    verts[1].pt.x = right;
                    verts[1].pt.y = bottom_right;
                    verts[2].pt.x = inner_x;
                    verts[2].pt.y = bottom - radius * height;
                    verts[3].pt.x = right;
                    verts[3].pt.y = bottom_right - radius * height;
                }
                else
                {
                    verts[1].uvs.x = 0.0f;
                    verts[1].uvs.y = high;
                    verts[0].uvs.x = high;
                    verts[0].uvs.y = high;
                    verts[3].uvs.x = low;
                    verts[3].uvs.y = low;
                    verts[2].uvs.y = low;
                    verts[2].uvs.x = high;
                    verts[0].pt.y = top + radius * height;
                    verts[0].pt.x = inner_x;
                    verts[1].pt.x = right;
                    verts[1].pt.y = top_right + radius * height;
                    verts[2].pt.x = inner_x;
                    verts[2].pt.y = top;
                    verts[3].pt.x = right;
                    verts[3].pt.y = top_right;
                }
            }
            draw->tristrip_2d_end();
        }
    }
}

// 0x00840BF0 (:2042)
// Rounds the corners not capped to the window, so zones meet on whole pixels.
static void fsmap_fixup_verts(vector2& sp_tl, vector2& sp_tr, vector2& sp_bl, vector2& sp_br, const bool* capped_x, const bool* capped_y)
{
    if (!capped_x[0])
    {
        sp_tl.x = (float)fl_round(sp_tl.x);
    }
    if (!capped_y[0])
    {
        sp_tl.y = (float)fl_round(sp_tl.y);
    }
    if (!capped_x[1])
    {
        sp_tr.x = (float)fl_round(sp_tr.x);
    }
    if (!capped_y[1])
    {
        sp_tr.y = (float)fl_round(sp_tr.y);
    }
    if (!capped_x[2])
    {
        sp_bl.x = (float)fl_round(sp_bl.x);
    }
    if (!capped_y[2])
    {
        sp_bl.y = (float)fl_round(sp_bl.y);
    }
    if (!capped_x[3])
    {
        sp_br.x = (float)fl_round(sp_br.x);
    }
    if (!capped_y[3])
    {
        sp_br.y = (float)fl_round(sp_br.y);
    }
}

// 0x00853400 (:2080)
static void fsm_determine_loaded_zones()
{
    if (Fs_info.flags.exiting)
    {
        Fs_info.static_alpha_time = 0.5f;
        return;
    }
    if (Fs_info.flags.wants_exit)
    {
        Fs_info.static_alpha_time = Frametime_real * 2.0f + Fs_info.static_alpha_time;
        if (Fs_info.static_alpha_time >= 0.5f)
        {
            Fs_info.flags.exiting = 1;
            Fs_info.static_alpha_time = 0.5f;
        }
        return;
    }
    if (Fs_info.flags.initial_grids_ready)
    {
        Fs_info.static_alpha_time -= Frametime_real;
        if (Fs_info.static_alpha_time < 0.0f)
        {
            Fs_info.static_alpha_time = 0.0f;
        }
        return;
    }
    unsigned int num_loaded = 0;
    for (int i = 0; i < Fs_info.zone_used_info.size(); i++)
    {
        const zone_map_information* zmi = Fs_info.zone_used_info[i];
        if (zmi != nullptr && (zmi->bitmap_handle == Fs_info.misc_static_id || zmi->stream_state == ZSS_LOADED || zmi->stream_state == ZSS_LOADED_BOGUS))
        {
            num_loaded++;
        }
    }
    if (Fs_info.full_screen_image_info.stream_state == ZSS_LOADED)
    {
        num_loaded++;
    }
    const unsigned int num_needed = Fs_info.zone_used_info.size() + (Use_full_screen_image ? 1 : 0);
    Fs_info.flags.initial_grids_ready = num_needed != 0 && num_loaded >= num_needed;
}

// 0x00867D50 (:2128)
static void fsm_render_districts()
{
    const int show_type = Fs_info.district_value;
    const int num_districts = Cartographer.fsm_district_boundries.size();
    rl_draw* draw = rl_draw::get_instance();
    for (int district_index = 0; district_index < num_districts; district_index++)
    {
        const district_boundries& boundries = Cartographer.fsm_district_boundries[district_index];
        rl_primitive_state state;
        gr_convert_to_rl_state(state, GM_FILTER_Z);
        draw->set_texture(-1);
        float r = 0.0f;
        float g = 0.0f;
        float b = 0.0f;
        float a = 0.0f;
        if (show_type != 0 || !fsm_get_district_color(boundries.district_name, r, g, b, a))
        {
            continue;
        }
        const rl_color_float color(r, g, b, a);
        draw->tristrip_2d_begin(state);
        rl_primitive_2d_vert* verts = draw->tristrip_2d_allocate_tris(boundries.num_tris);
        if (verts == nullptr)
        {
            return;
        }
        for (int i = 0; i < boundries.num_indices; i++)
        {
            verts[i].color = rl_color(color);
            verts[i].uvs.x = 0.0f;
            verts[i].uvs.y = 0.0f;
            vector point;
            Cartographer.get_district_boundry_point(district_index, i, point);
            window_coord_info info;
            Cartographer.get_window_coordinate_info(info, point);
            verts[i].pt.x = info.actual_posn.x;
            verts[i].pt.y = info.actual_posn.y;
        }
        draw->tristrip_2d_end();
    }
}

// 0x00853530 (:2182)
static void fsm_render_territories()
{
    const float ratio_x = (Fs_info.map_render_window.max.x - Fs_info.map_render_window.min.x) / (Fs_info.current_view_bbox.max.x - Fs_info.current_view_bbox.min.x);
    const float ratio_z = (Fs_info.map_render_window.max.y - Fs_info.map_render_window.min.y) / (Fs_info.current_view_bbox.max.z - Fs_info.current_view_bbox.min.z);
    bool drew_static = false;
    const unsigned int num_zones = Fs_info.zone_used_info.size();
    if (num_zones == 0)
    {
        Fs_info.flags.fully_loaded = 1;
    }
    rl_draw* draw = rl_draw::get_instance();
    for (unsigned int i = 0; i < num_zones; i++)
    {
        zone_map_information* zmi = Fs_info.zone_used_info[i];
        if (zmi == nullptr || zmi->bitmap_handle == Fs_info.misc_static_id)
        {
            continue;
        }
        vector zone_min;
        vector zone_max;
        rfg_zonex_get_zone_bbox(zmi->gid.x, zmi->gid.y, zone_min, zone_max);
        if (zone_min.x > Fs_info.current_view_bbox.max.x || Fs_info.current_view_bbox.min.x > zone_max.x || zone_min.z > Fs_info.current_view_bbox.max.z || Fs_info.current_view_bbox.min.z > zone_max.z)
        {
            continue;
        }
        const float inverse_x = 1.0f / (zone_max.x - zone_min.x);
        const float inverse_z = 1.0f / (zone_max.z - zone_min.z);
        // Corner order: top left, top right, bottom left, bottom right.
        bool capped_x[4] = {false, false, false, false};
        bool capped_y[4] = {false, false, false, false};
        float screen_left;
        float screen_right;
        float screen_top;
        float screen_bottom;
        float uv_left;
        float uv_right;
        float uv_top = 0.0f;
        float uv_bottom = 1.0f;
        if (Fs_info.current_view_bbox.min.x <= zone_min.x)
        {
            screen_left = (zone_min.x - Fs_info.current_view_bbox.min.x) * ratio_x + Fs_info.map_render_window.min.x;
            uv_left = 0.0f;
        }
        else
        {
            screen_left = 0.0f;
            capped_x[0] = true;
            capped_x[2] = true;
            uv_left = (Fs_info.current_view_bbox.min.x - zone_min.x) * inverse_x;
        }
        if (zone_max.x <= Fs_info.current_view_bbox.max.x)
        {
            screen_right = (zone_max.x - Fs_info.current_view_bbox.min.x) * ratio_x;
            uv_right = 1.0f;
        }
        else
        {
            screen_right = Fs_info.map_render_window.max.x;
            uv_right = (Fs_info.current_view_bbox.max.x - zone_min.x) * inverse_x;
            capped_x[1] = true;
            capped_x[3] = true;
        }
        if (zone_max.z <= Fs_info.current_view_bbox.max.z)
        {
            screen_top = (Fs_info.current_view_bbox.max.z - zone_max.z) * ratio_z;
        }
        else
        {
            screen_top = 0.0f;
            uv_top = (zone_max.z - Fs_info.current_view_bbox.max.z) * inverse_z;
            capped_y[0] = true;
            capped_y[1] = true;
        }
        if (Fs_info.current_view_bbox.min.z <= zone_min.z)
        {
            screen_bottom = (Fs_info.current_view_bbox.max.z - zone_min.z) * ratio_z;
        }
        else
        {
            uv_bottom = (zone_max.z - Fs_info.current_view_bbox.min.z) * inverse_z;
            capped_y[2] = true;
            capped_y[3] = true;
            screen_bottom = Fs_info.map_render_window.max.y;
        }
        // Original bug, kept: the window's min.x goes on the left twice and min.y only on the right corners. All are 0 while
        // rfg.exe runs.
        vector2 tl(screen_left + Fs_info.map_render_window.min.x, screen_top);
        vector2 tr(screen_right, screen_top + Fs_info.map_render_window.min.y);
        vector2 bl(screen_left + Fs_info.map_render_window.min.x, screen_bottom);
        vector2 br(screen_right, screen_bottom + Fs_info.map_render_window.min.y);
        fsmap_fixup_verts(tl, tr, bl, br, capped_x, capped_y);
        if (zmi->stream_state == ZSS_LOADED)
        {
            Fs_info.flags.rendered_a_grid = 1;
            zmi->alpha_not_loaded -= Frametime_real;
            if (zmi->alpha_not_loaded > 0.0f)
            {
                if (!zmi->fully_obscured_by_fow)
                {
                    fsm_render_static_bitmap(false, bl, br, tl, tr, zmi->alpha_not_loaded * 2.0f);
                    drew_static = true;
                }
            }
            else
            {
                zmi->alpha_not_loaded = 0.0f;
                if (!Fs_info.flags.fully_loaded)
                {
                    Fs_info.flags.fully_loaded = 1;
                }
            }
            gr_set_color(255, 255, 255, 255 - (int)(zmi->alpha_not_loaded * 2.0f * 255.0f));
            rl_primitive_state state;
            gr_convert_to_rl_state(state, GM_FILTER);
            draw->set_texture(zmi->bitmap_handle);
            const rl_color_float color(1.0f, 1.0f, 1.0f, 1.0f - zmi->alpha_not_loaded);
            draw->tristrip_2d_begin(state);
            rl_primitive_2d_vert* verts = draw->tristrip_2d_allocate_tris(2);
            if (verts == nullptr)
            {
                return;
            }
            verts[3].color = rl_color(color);
            verts[2].color = verts[3].color;
            verts[1].color = verts[3].color;
            verts[0].color = verts[3].color;
            verts[0].uvs.x = uv_left;
            verts[0].uvs.y = uv_bottom;
            verts[1].uvs.x = uv_right;
            verts[1].uvs.y = uv_bottom;
            verts[2].uvs.x = uv_left;
            verts[2].uvs.y = uv_top;
            verts[3].uvs.x = uv_right;
            verts[3].uvs.y = uv_top;
            verts[0].pt = bl;
            verts[1].pt = br;
            verts[2].pt = tl;
            verts[3].pt = tr;
            draw->tristrip_2d_end();
            if (Fs_info.side_bitmap_id != -1 && zmi->corner_info != 0)
            {
                fsm_render_corner_info(zmi, ratio_x, ratio_z);
            }
        }
        else if (zmi->stream_state != ZSS_LOADED_BOGUS)
        {
            fsm_render_static_bitmap(false, bl, br, tl, tr, 1.0f);
            drew_static = true;
        }
    }
    if (Use_full_screen_image && Fs_info.zoom_level < Full_screen_image_zoom_max && Fs_info.full_screen_image_info.stream_state == ZSS_LOADED && Fs_info.full_screen_image_info.bitmap_handle != Fs_info.misc_static_id)
    {
        const float inverse_x = 1.0f / (Fs_info.world_extents_bbox.max.x - Fs_info.world_extents_bbox.min.x);
        const float inverse_z = 1.0f / (Fs_info.world_extents_bbox.max.z - Fs_info.world_extents_bbox.min.z);
        bool capped_x[4] = {false, false, false, false};
        bool capped_y[4] = {false, false, false, false};
        float image_alpha = 1.0f - (Fs_info.zoom_level - Full_screen_image_alpha_start) / (Full_screen_image_zoom_max - Full_screen_image_alpha_start);
        if (image_alpha - 0.0f < 0.0f)
        {
            image_alpha = 0.0f;
        }
        if (image_alpha - 1.0f >= 0.0f)
        {
            image_alpha = 1.0f;
        }
        const float view_min_x = Fs_info.current_view_bbox.min.x;
        float screen_left;
        float screen_right;
        float screen_top;
        float screen_bottom;
        float uv_left;
        float uv_right;
        float uv_top;
        float uv_bottom;
        if (view_min_x <= Fs_info.world_extents_bbox.min.x)
        {
            screen_left = (Fs_info.world_extents_bbox.min.x - view_min_x) * ratio_x + Fs_info.map_render_window.min.x;
            uv_left = 0.0f;
        }
        else
        {
            screen_left = 0.0f;
            uv_left = (view_min_x - Fs_info.world_extents_bbox.min.x) * inverse_x;
            capped_x[0] = true;
            capped_x[2] = true;
        }
        if (Fs_info.world_extents_bbox.max.x <= Fs_info.current_view_bbox.max.x)
        {
            screen_right = (Fs_info.world_extents_bbox.max.x - view_min_x) * ratio_x;
            uv_right = 1.0f;
        }
        else
        {
            screen_right = Fs_info.map_render_window.max.x;
            uv_right = (Fs_info.current_view_bbox.max.x - Fs_info.world_extents_bbox.min.x) * inverse_x;
            capped_x[1] = true;
            capped_x[3] = true;
        }
        if (Fs_info.world_extents_bbox.max.z <= Fs_info.current_view_bbox.max.z)
        {
            screen_top = (Fs_info.current_view_bbox.max.z - Fs_info.world_extents_bbox.max.z) * ratio_z;
            uv_top = 0.0f;
        }
        else
        {
            uv_top = (Fs_info.world_extents_bbox.max.z - Fs_info.current_view_bbox.max.z) * inverse_z;
            screen_top = 0.0f;
            capped_y[0] = true;
            capped_y[1] = true;
        }
        if (Fs_info.current_view_bbox.min.z <= Fs_info.world_extents_bbox.min.z)
        {
            screen_bottom = (Fs_info.current_view_bbox.max.z - Fs_info.world_extents_bbox.min.z) * ratio_z;
            uv_bottom = 1.0f;
        }
        else
        {
            uv_bottom = (Fs_info.world_extents_bbox.max.z - Fs_info.current_view_bbox.min.z) * inverse_z;
            capped_y[2] = true;
            capped_y[3] = true;
            screen_bottom = Fs_info.map_render_window.max.y;
        }
        vector2 tl(screen_left + Fs_info.map_render_window.min.x, screen_top);
        vector2 tr(screen_right, screen_top + Fs_info.map_render_window.min.y);
        vector2 bl(screen_left + Fs_info.map_render_window.min.x, screen_bottom);
        vector2 br(screen_right, screen_bottom + Fs_info.map_render_window.min.y);
        fsmap_fixup_verts(tl, tr, bl, br, capped_x, capped_y);
        Fs_info.full_screen_image_info.alpha_not_loaded -= Frametime_real;
        if (Fs_info.full_screen_image_info.alpha_not_loaded <= 0.0f)
        {
            Fs_info.full_screen_image_info.alpha_not_loaded = 0.0f;
            if (!Fs_info.flags.fully_loaded)
            {
                Fs_info.flags.fully_loaded = 1;
            }
        }
        gr_set_color(255, 255, 255, 125); // Magic number: half transparent.
        rl_primitive_state state;
        gr_convert_to_rl_state(state, GM_FILTER);
        draw->set_texture(Fs_info.full_screen_image_info.bitmap_handle);
        draw->tristrip_2d_begin(state);
        rl_primitive_2d_vert* verts = draw->tristrip_2d_allocate_tris(2);
        if (verts == nullptr)
        {
            return;
        }
        verts[3].color.set(255, 255, 255, (unsigned char)(int)(image_alpha * 255.0f + 0.5f));
        verts[2].color = verts[3].color;
        verts[1].color = verts[3].color;
        verts[0].color = verts[3].color;
        verts[0].uvs.x = uv_left;
        verts[0].uvs.y = uv_bottom;
        verts[1].uvs.x = uv_right;
        verts[1].uvs.y = uv_bottom;
        verts[2].uvs.x = uv_left;
        verts[2].uvs.y = uv_top;
        verts[3].uvs.x = uv_right;
        verts[3].uvs.y = uv_top;
        verts[0].pt = bl;
        verts[1].pt = br;
        verts[2].pt = tl;
        verts[3].pt = tr;
        draw->tristrip_2d_end();
    }
    Fs_info.flags.has_single_static_gids = drew_static && Fs_info.flags.updated_gids;
}

// 0x0087A690 (:2721)
static void fsm_load_ui_information(rl_renderer& renderer)
{
    Fs_info.doc_handle = ui_load_document(cUI_fullscreen_doc, DFIT_NONE, 0.0f, DLT_NORMAL, false, STREAM_PRIORITY_AUDIO_CACHE_LOAD, false);
    if (Fs_info.doc_handle == -1)
    {
        return;
    }
    vint_data_group* group = vint_data_group::create_new("FS_Group");
    if (group != nullptr)
    {
        Fs_info.data_group_handle = group->get_handle();
        ui_add_data_group(Fs_info.doc_handle, Fs_info.data_group_handle);
    }
    group = vint_data_group::create_new("FS_POI_Group");
    if (group != nullptr)
    {
        Fs_info.poi_group_handle = group->get_handle();
        ui_add_data_group(Fs_info.doc_handle, Fs_info.poi_group_handle);
        for (int i = 0; i < fsm_points_of_interest.size(); i++)
        {
            vint_data_item* item = group->add_data_item(nullptr);
            if (item != nullptr)
            {
                const vint_variant_type types[6] = {VINT_PROP_TYPE_INT, VINT_PROP_TYPE_FLOAT, VINT_PROP_TYPE_FLOAT, VINT_PROP_TYPE_STRING, VINT_PROP_TYPE_FLOAT, VINT_PROP_TYPE_BOOL};
                item->set_template(types, 6);
            }
        }
    }
    for (int i = 0; i < 6; i++)
    {
        unsigned int handle = 0;
        vint_data_item* item;
        switch (i)
        {
        case 0:
            item = vint_data_item::create_new("FS_HeaderInfo");
            if (item != nullptr)
            {
                const vint_variant_type types[2] = {VINT_PROP_TYPE_STRING, VINT_PROP_TYPE_BOOL};
                item->set_template(types, 2);
                handle = item->get_handle();
            }
            break;
        case 1:
            item = vint_data_item::create_new("FS_DistrictFilter");
            if (item != nullptr)
            {
                const vint_variant_type types[1] = {VINT_PROP_TYPE_STRING};
                item->set_template(types, 1);
                handle = item->get_handle();
            }
            break;
        case 2:
            item = vint_data_item::create_new("FS_HighlightItem");
            if (item != nullptr)
            {
                const vint_variant_type types[8] = {VINT_PROP_TYPE_BOOL, VINT_PROP_TYPE_STRING, VINT_PROP_TYPE_STRING, VINT_PROP_TYPE_STRING, VINT_PROP_TYPE_FLOAT, VINT_PROP_TYPE_FLOAT, VINT_PROP_TYPE_FLOAT, VINT_PROP_TYPE_FLOAT};
                item->set_template(types, 8);
                handle = item->get_handle();
            }
            break;
        case 3:
            item = vint_data_item::create_new("FS_DistrictItem");
            if (item != nullptr)
            {
                const vint_variant_type types[11] = {VINT_PROP_TYPE_UINT, VINT_PROP_TYPE_INT, VINT_PROP_TYPE_FLOAT, VINT_PROP_TYPE_INT, VINT_PROP_TYPE_STRING, VINT_PROP_TYPE_INT, VINT_PROP_TYPE_INT, VINT_PROP_TYPE_INT, VINT_PROP_TYPE_INT, VINT_PROP_TYPE_BOOL, VINT_PROP_TYPE_INT};
                item->set_template(types, 11);
                handle = item->get_handle();
            }
            break;
        case 4:
            item = vint_data_item::create_new("FS_Message");
            if (item != nullptr)
            {
                const vint_variant_type types[3] = {VINT_PROP_TYPE_BOOL, VINT_PROP_TYPE_STRING, VINT_PROP_TYPE_STRING};
                item->set_template(types, 3);
                handle = item->get_handle();
            }
            break;
        case 5:
            item = vint_data_item::create_new("FS_Hints");
            if (item != nullptr)
            {
                const vint_variant_type types[10] = {VINT_PROP_TYPE_STRING, VINT_PROP_TYPE_STRING, VINT_PROP_TYPE_STRING, VINT_PROP_TYPE_STRING, VINT_PROP_TYPE_STRING, VINT_PROP_TYPE_BOOL, VINT_PROP_TYPE_BOOL, VINT_PROP_TYPE_BOOL, VINT_PROP_TYPE_BOOL, VINT_PROP_TYPE_BOOL};
                item->set_template(types, 10);
                handle = item->get_handle();
            }
            break;
        default:
            keen::debug::nativePrintArgs("Int3 critical function at %s(%d)\n", __FILE__, __LINE__);
            break;
        }
        Fs_info.data_item_handles[i] = handle;
        ui_add_data_item(Fs_info.doc_handle, handle);
    }
    fog_of_war_rendering_alloc(renderer);
}

// 0x00840D40 (:2923)
static void fsm_process_data_item_header()
{
    vint_data_item* item = vint_data_item::find(Fs_info.data_item_handles[0]);
    if (item != nullptr)
    {
        item->set_element(0, vint_variant(cDistrictHeader));
        item->set_element(1, vint_variant(Fs_info.flags.loaded_one_in && !Fs_info.flags.exiting));
    }
}

// 0x00840DD0 (:2936)
static void fsm_process_data_item_district_filter()
{
    vint_data_item* item = vint_data_item::find(Fs_info.data_item_handles[1]);
    if (item != nullptr)
    {
        const char* text = cUI_district_filter;
        if (Fs_info.district_value != 0)
        {
            text = cUI_district_control;
            if (Fs_info.district_value != 1)
            {
                text = Fs_info.district_value == 2 ? cUI_district_morale : cUI_district_none;
            }
        }
        item->set_element(0, vint_variant(text));
    }
}

// 0x008542A0 (:2967)
bool fsm_warp_to_safehouse_cb(int mbox_handle, int selection_index, msgbox_choice choice)
{
    (void)selection_index;
    if (Msgbox_handle != mbox_handle)
    {
        return false;
    }
    if (choice == MBC_OK)
    {
        ui_menu_play_sound(MENU_SOUND_PRESS_SELECT);
        if (!Fs_info.flags.wants_exit && !Fs_info.flags.exiting && Cartographer.m_highlight.m_valid && fsm_posn_is_locked_on(Cartographer.m_highlight.m_fake_pos) && Cartographer.m_highlight.m_obj_handle != 0xFFFFFFFF)
        {
            const object* objp = World.object_from_handle(Cartographer.m_highlight.m_obj_handle);
            if (objp != nullptr && objp->get_obj_type() == OT_SAFEHOUSE)
            {
                vector start_pos;
                matrix start_orient;
                if (safehouse_get_start_pos_orient(start_pos, start_orient, (const object_safehouse*)objp))
                {
                    World.save_and_warp(start_pos, start_orient);
                    ui_fade_to_black_instant_fade_out();
                    gameseq_pop_state();
                }
            }
        }
    }
    else if ((int)choice < 1 || (int)choice > 2)
    {
        keen::debug::nativePrintArgs("Int3 critical function at %s(%d)\n", __FILE__, __LINE__);
    }
    ui_wait_for_button_ups();
    Msgbox_handle = -1;
    return true;
}

// One best or par time line of a Guerrilla Express node. rfg.exe inlines this twice.
static void fsm_append_time_line(wchar_t* text, const char* label, unsigned int time_ms)
{
    // Magic numbers: milliseconds to seconds, minutes and hundredths.
    const int seconds = (int)((double)time_ms / 1000.0);
    const int hundredths = fl_round((float)((double)(float)((int)time_ms % 1000) / 10.0));
    wchar_t previous[400];
    wcscpy(previous, text);
    _snprintf_safe(text, 0x100, L"%ls\n[color:#E6AA00FF]%ls:\r %i:%.02i:%.02i", previous, localization_get_display_string_req(label, nullptr), seconds / 60, seconds % 60, hundredths);
}

// 0x0087A9D0 (:3013)
static void fsm_process_data_item_highlight(bool exiting)
{
    vint_data_item* data_item = vint_data_item::find(Fs_info.data_item_handles[2]);
    if (data_item == nullptr)
    {
        return;
    }
    bool visible = false;
    wchar_t text[256];
    wchar_t previous[400];
    char title[400];
    char description[768];
    text[0] = L'\0';
    title[0] = '\0';
    description[0] = '\0';
    float angle = 0.0f;
    float red = 1.0f;
    float green = 1.0f;
    float blue = 1.0f;
    const highlight_item* item = nullptr;
    if (!exiting && Cartographer.m_highlight.m_valid)
    {
        item = &Cartographer.m_highlight;
    }
    const highlight_item& highlight = Cartographer.m_highlight;
    if (item != nullptr && fsm_posn_is_locked_on(highlight.m_fake_pos))
    {
        if (!was_displaying_highlight)
        {
            audiolib_cue_play(Ga_cue_ids.map_select_point_of_interest, nullptr, nullptr);
        }
        was_displaying_highlight = true;
        visible = true;
        const bool valid_icon = (unsigned int)highlight.m_icon_index < 59; // Magic number: the size of Minimap_icon_infos.
        const char* caption = valid_icon ? Minimap_icon_infos[highlight.m_icon_index].caption : nullptr;
        const char* desc_key = valid_icon ? Minimap_icon_infos[highlight.m_icon_index].description : nullptr;
        if (caption != nullptr)
        {
            bool done = false;
            if (highlight.m_icon_index == MAP_ELEMENT_MISSION_START)
            {
                const object_mission_start_node* node = (const object_mission_start_node*)World.object_from_handle(highlight.m_obj_handle);
                const district* dp;
                if (node != nullptr && node->get_mission_info()->district_name != nullptr)
                {
                    dp = (const district*)World.object_from_handle(get_district_by_name(node->get_mission_info()->district_name));
                }
                else
                {
                    dp = Cartographer.get_district_from_point(highlight.m_obj_pos);
                }
                if (dp != nullptr)
                {
                    const wchar_t* caption_text = localization_get_display_string_req(caption, nullptr);
                    const wchar_t* district_text = localization_get_display_string_req(dp->get_localized_name_hash());
                    swprintf(previous, 400, L"%ls %ls", district_text, caption_text);
                    vint_encode_wide_text(title, 400, previous);
                    done = true;
                }
            }
            if (!done)
            {
                vint_encode_wide_text(title, 400, localization_get_display_string_req(caption, nullptr));
            }
        }
        if (desc_key != nullptr)
        {
            const wchar_t* final_text = text;
            float control = 0.0f;
            float morale = 0.0f;
            // Deviation: rfg.exe leaves these three uninitialized for get_activity_rewards to fill.
            int salvage = 0;
            object_mover* mover = highlight.m_type == IIT_BLDG ? World.object_from_handle<object_mover>(highlight.m_obj_handle) : nullptr;
            if (mover != nullptr)
            {
                const char* mover_name = mover->get_name();
                const wchar_t* desc_text = localization_get_display_string_req(desc_key, nullptr);
                const wchar_t* control_label = localization_get_display_string_req("MENU_GH_CONTROL", nullptr);
                if (mover_name != nullptr && strlen(mover_name) > 1)
                {
                    const wchar_t* name_text = localization_get_display_string_req(mover_name, nullptr);
                    if (mover->control > 0.0f)
                    {
                        const float scaled = difficulty_get_value(DIFFICULTY_CONTROL_PARKER, NUM_DIFFICULTY_LEVELS) * mover->control;
                        _snprintf_safe(text, 0x100, L"%ls\n%ls\n[color:#E6AA00FF]%ls:\r %d", desc_text, name_text, control_label, fl_round(scaled));
                    }
                    else
                    {
                        _snprintf_safe(text, 0x100, L"%ls\n%ls", desc_text, name_text);
                    }
                }
                else if (mover->control > 0.0f)
                {
                    const float scaled = difficulty_get_value(DIFFICULTY_CONTROL_PARKER, NUM_DIFFICULTY_LEVELS) * mover->control;
                    _snprintf_safe(text, 0x100, L"%ls\n[color:#E6AA00FF]%ls:\r %d", desc_text, control_label, fl_round(scaled));
                }
                else
                {
                    _snprintf_safe(text, 0x100, L"%ls", desc_text);
                }
            }
            else if (Cartographer.get_activity_rewards(control, morale, salvage))
            {
                const wchar_t* control_label = localization_get_display_string_req("MENU_GH_CONTROL", nullptr);
                const wchar_t* morale_label = localization_get_display_string_req("MENU_MISSION_COMPLETE_WORD_MORALE", nullptr);
                const wchar_t* salvage_label = localization_get_display_string_req(cUI_fsm_salvage, nullptr);
                const char* header_key = desc_key;
                if (highlight.m_icon_index == 14) // Magic number: the convoy icon.
                {
                    if (activity_convoy_get_type() == CONVOY_TYPE_CAPTURE)
                    {
                        header_key = "HUD_CAPTURE_CONVOY";
                    }
                    else if (activity_convoy_get_type() == CONVOY_TYPE_DESTROY)
                    {
                        header_key = "HUD_DESTROY_CONVOY";
                    }
                }
                const wchar_t* header_text = localization_get_display_string_req(header_key, nullptr);
                if (header_text != nullptr)
                {
                    _snprintf_safe(text, 0x100, L"%ls[color:#E6AA00FF]", header_text);
                    if (control > 0.0f)
                    {
                        wcscpy(previous, text);
                        const float scaled = difficulty_get_value(DIFFICULTY_CONTROL_PARKER, NUM_DIFFICULTY_LEVELS) * control;
                        _snprintf_safe(text, 0x100, L"%ls\n%ls:\r %d", previous, control_label, fl_round(scaled));
                    }
                    if (morale > 0.0f)
                    {
                        wcscpy(previous, text);
                        const int scaled = (int)(difficulty_get_value(DIFFICULTY_MORALE_BOOSTS, NUM_DIFFICULTY_LEVELS) * morale);
                        _snprintf_safe(text, 0x100, L"%ls\n%ls:\r %d", previous, morale_label, scaled);
                    }
                    if (salvage > 0)
                    {
                        wcscpy(previous, text);
                        _snprintf_safe(text, 0x100, L"%ls\n%ls:\r %d", previous, salvage_label, salvage);
                    }
                }
            }
            else if (highlight.m_icon_index == 38 && Cartographer.can_warp_to_safehouse()) // Magic number: the safehouse icon.
            {
                final_text = localization_get_display_string_req("MENU_WARP_TO_SAFEHOUSE_DESC", nullptr);
                // Magic number: ui button 15 confirms the warp.
                if (ui_get_button_info(15, Input_id) != 0.0f)
                {
                    fsm_stop_all_sounds(true);
                    Msgbox_handle = ui_add_msgbox(MBT_ACCEPT_CANCEL_SELECT, "MENU_WARP_TO_SAFEHOUSE_TITLE", "MENU_WARP_TO_SAFEHOUSE_CONFIRM", fsm_warp_to_safehouse_cb, false, false, nullptr, nullptr, false);
                }
            }
            else if (highlight.m_icon_index == 26) // Magic number: the Guerrilla Express icon.
            {
                _snprintf_safe(text, 0x100, L"%ls", localization_get_display_string_req(desc_key, nullptr));
                object_delivery_node* node = World.object_from_handle<object_delivery_node>(highlight.m_act_handle);
                if (node != nullptr && node->get_delivery_def() != nullptr)
                {
                    // Original bug, kept: get_best_time's bool is not read, and when it does not write the time this is
                    // the slot get_activity_rewards just wrote, the salvage.
                    unsigned int best_time = (unsigned int)salvage;
                    node->get_best_time(best_time);
                    const int par_time = node->get_delivery_def()->par_time;
                    fsm_append_time_line(text, "ACT_GEXPRESS_RESULTS_BEST_TIME", best_time);
                    fsm_append_time_line(text, "ACT_GEXPRESS_RESULTS_PAR_TIME", (unsigned int)par_time);
                }
            }
            else if (highlight.m_icon_index == 25) // Magic number: the demolitions master icon.
            {
                _snprintf_safe(text, 0x100, L"%ls", localization_get_display_string_req(desc_key, nullptr));
                object_demolitions_master_node* node = World.object_from_handle<object_demolitions_master_node>(highlight.m_act_handle);
                if (node != nullptr && node->base_demolitions_master_data != nullptr)
                {
                    const demolitions_master_level_data* data = node->base_demolitions_master_data;
                    const int par_seconds = data->par_time;
                    // Magic number: the best time is in milliseconds, the par time in seconds.
                    const int best_seconds = (int)((double)(unsigned int)data->dm_stats.best_time / 1000.0);
                    wcscpy(previous, text);
                    _snprintf_safe(text, 0x100, L"%ls\n[color:#E6AA00FF]%ls:\r %i:%.02i", previous, localization_get_display_string_req("ACT_GEXPRESS_RESULTS_BEST_TIME", nullptr), best_seconds / 60, best_seconds % 60);
                    wcscpy(previous, text);
                    _snprintf_safe(text, 0x100, L"%ls\n[color:#E6AA00FF]%ls:\r %i:%.02i", previous, localization_get_display_string_req("ACT_GEXPRESS_RESULTS_PAR_TIME", nullptr), par_seconds / 60, par_seconds % 60);
                }
            }
            else
            {
                final_text = localization_get_display_string_req(desc_key, nullptr);
            }
            vint_encode_wide_text(description, 0x300, final_text);
        }
        angle = highlight.m_angle_rad;
        // Magic number: 1 / 255.
        red = (float)highlight.m_col.red * 0.003921569f;
        green = (float)highlight.m_col.green * 0.003921569f;
        blue = (float)highlight.m_col.blue * 0.003921569f;
    }
    else
    {
        was_displaying_highlight = false;
    }
    data_item->set_element(0, vint_variant(visible));
    data_item->set_element(1, vint_variant((const char*)title));
    data_item->set_element(2, vint_variant((const char*)description));
    const char* icon_name;
    if (item == nullptr || !visible)
    {
        icon_name = "none";
    }
    else if ((unsigned int)item->m_icon_index < 59)
    {
        icon_name = Minimap_icon_infos[item->m_icon_index].icon_name;
    }
    else
    {
        icon_name = nullptr;
    }
    data_item->set_element(3, vint_variant(icon_name));
    data_item->set_element(4, vint_variant(angle));
    data_item->set_element(5, vint_variant(red));
    data_item->set_element(6, vint_variant(green));
    data_item->set_element(7, vint_variant(blue));
}

// 0x00854460 (:3258)
static void fsm_process_mission_info(unsigned int district_handle)
{
    vint_data_group* group = vint_data_group::find(Fs_info.data_group_handle);
    if (group == nullptr || district_handle == Fs_info.last_district_handle)
    {
        Fs_info.last_district_handle = district_handle;
        return;
    }
    for (int i = 0; i < Fs_info.num_data_group_items; i++)
    {
        group->remove_data_item(Fs_info.data_group_items[i]->get_handle());
    }
    Fs_info.num_data_group_items = 0;
    int number = 0;
    for (int i = 0; i < Fs_info.mission_infos.size(); i++)
    {
        const mission_info* info = Fs_info.mission_infos[i];
        const object* node = World.object_from_handle(info->start_node_handle);
        if (node == nullptr)
        {
            continue;
        }
        const vector pos(node->pos.x, 0.0f, node->pos.z);
        int grid_x;
        int grid_y;
        if (!rfg_zonex_get_grid_coords(pos, grid_x, grid_y))
        {
            continue;
        }
        const short index = fsm_get_stream_information_index(grid_x, grid_y);
        if (index == -1 || Fs_info.stream_information[index].district_handle != district_handle)
        {
            continue;
        }
        vint_data_item* item = group->add_data_item(nullptr);
        if (item != nullptr)
        {
            const vint_variant_type types[4] = {VINT_PROP_TYPE_INT, VINT_PROP_TYPE_STRING, VINT_PROP_TYPE_BOOL, VINT_PROP_TYPE_BOOL};
            item->set_template(types, 4);
        }
        // Original bug, kept: more than 6 missions in a district run off data_group_items, and a null item is not tested.
        Fs_info.data_group_items[Fs_info.num_data_group_items] = item;
        Fs_info.num_data_group_items++;
        number++;
        item->set_element(0, vint_variant(number));
        item->set_element(1, vint_variant(info->mission_flags.attempted || info->mission_flags.viewed ? info->description : "HM_UNKOWN_MISSION"));
        item->set_element(2, vint_variant((bool)info->mission_flags.complete));
        item->set_element(3, vint_variant((bool)info->mission_flags.attempted));
    }
    Fs_info.last_district_handle = district_handle;
}

// 0x00840E40 (:3352)
static void fsm_process_data_item_message()
{
    vint_data_item* item = vint_data_item::find(Fs_info.data_item_handles[4]);
    if (Fs_info.display_gps_warning && Fs_info.message_timer.elapsed())
    {
        Fs_info.display_gps_warning = false;
    }
    if (item != nullptr)
    {
        item->set_element(0, vint_variant(Fs_info.display_gps_warning));
        item->set_element(1, vint_variant((const char*)Fs_info.gps_warning_caption));
        item->set_element(2, vint_variant((const char*)Fs_info.gps_warning_desc));
    }
}

// 0x00854710 (:3367)
static void fsm_process_poi_info()
{
    vint_data_group* group = vint_data_group::find(Fs_info.poi_group_handle);
    if (group == nullptr)
    {
        return;
    }
    vint_data_item* item = group->first_data_item();
    for (int i = 0; i < fsm_points_of_interest.size(); i++)
    {
        if (item != nullptr)
        {
            const fsm_poi& poi = fsm_points_of_interest[i];
            // Magic numbers: zone_xz holds the zone's x and z in its nibbles, zones are 511 m, and the map starts 4088 m
            // (eight zones) from the center. x and z are in units of 2 m inside the zone.
            const float world_x = ((float)(poi.zone_xz >> 4) * 511.0f - 4088.0f) + (float)poi.x * 2.0f - Cartographer.m_map_corner_posns[0].x;
            const float world_z = (-((float)(poi.zone_xz & 0xF) * 511.0f - 4088.0f) - (float)poi.z * 2.0f) - Cartographer.m_map_corner_posns[0].z;
            const float screen_y = (world_z * Cartographer.m_sw_to_nw_vector.z + world_x * Cartographer.m_sw_to_nw_vector.x) * Cartographer.m_current_size_info.map_scale.y + Cartographer.m_current_size_info.map_posn.y;
            const float screen_x = (world_z * Cartographer.m_ne_to_nw_vector.z + world_x * Cartographer.m_ne_to_nw_vector.x) * Cartographer.m_current_size_info.map_scale.x + Cartographer.m_current_size_info.map_posn.x;
            item->set_element(0, vint_variant(i + 1));
            const vector2 scale = vint_project_get_scale(VINT_RES_SCALE_MAINTAIN_RATIO, VINT_RES_SCALE_MAINTAIN_RATIO);
            const vector2 offset = vint_project_get_offset(VINT_RES_OFFSET_CENTER, VINT_RES_OFFSET_CENTER);
            item->set_element(1, vint_variant(screen_x / scale.x + offset.x));
            item->set_element(2, vint_variant(screen_y / scale.y + offset.y));
            item->set_element(3, vint_variant((const char*)poi.poi_text));
            item->set_element(4, vint_variant(fullscreen_map_get_zoom_pct()));
            item->set_element(5, vint_variant(fullscreen_map_is_loaded()));
        }
        item = group->next_data_item(item);
    }
}

// 0x0087B670 (:3405)
static void fsm_process_data_item_district_info()
{
    vint_data_item* item = vint_data_item::find(Fs_info.data_item_handles[3]);
    unsigned int district_handle = 0;
    if (item != nullptr)
    {
        float control = 100.0f;
        float morale = 0.0f;
        float control_max = 100.0f;
        const int salvage = player_get_meta_data()->get_salvage();
        const int salvage_again = player_get_meta_data()->get_salvage();
        const float along_ne = (Fs_info.map_center.z - Cartographer.m_map_corner_posns[0].z) * Cartographer.m_ne_to_nw_vector.z + (Fs_info.map_center.x - Cartographer.m_map_corner_posns[0].x) * Cartographer.m_ne_to_nw_vector.x;
        const float along_sw = (Fs_info.map_center.z - Cartographer.m_map_corner_posns[0].z) * Cartographer.m_sw_to_nw_vector.z + (Fs_info.map_center.x - Cartographer.m_map_corner_posns[0].x) * Cartographer.m_sw_to_nw_vector.x;
        const vector2 window_pos(along_ne * Cartographer.m_current_size_info.map_scale.x + Cartographer.m_current_size_info.map_posn.x, along_sw * Cartographer.m_current_size_info.map_scale.y + Cartographer.m_current_size_info.map_posn.y);
        unsigned int name_hash = 0;
        int num_missions = 0;
        int remaining_missions = 0;
        int locked_missions = 0;
        const district* dp = Cartographer.get_district_from_point_2d(window_pos);
        if (dp != nullptr && v_stricmp(dp->get_name(), "End Game") != 0)
        {
            district_handle = dp->get_handle();
            name_hash = dp->get_localized_name_hash();
            control = dp->get_control();
            control_max = dp->get_control_max();
            morale = dp->get_morale();
            missions_get_hud_info(district_handle, num_missions, remaining_missions, locked_missions);
        }
        item->set_element(0, vint_variant(name_hash));
        item->set_element(1, vint_variant((int)control));
        item->set_element(2, vint_variant(morale));
        item->set_element(3, vint_variant(salvage));
        item->set_element(4, vint_variant(cUI_fsm_salvage));
        item->set_element(5, vint_variant(salvage_again));
        item->set_element(6, vint_variant(num_missions));
        item->set_element(7, vint_variant(num_missions - remaining_missions));
        item->set_element(8, vint_variant(locked_missions));
        item->set_element(9, vint_variant(mission_is_active()));
        item->set_element(10, vint_variant((int)((control / control_max) * 100.0f)));
    }
    fsm_process_mission_info(district_handle);
}

// 0x00854AA0 (:3444)
static void fsm_process_hints()
{
    vint_data_item* item = vint_data_item::find(Fs_info.data_item_handles[5]);
    if (item == nullptr)
    {
        return;
    }
    item->set_element(0, vint_variant(fsm_should_remove_player_marker() ? cUI_hint_remove_nav_beacon : cUI_hint_place_nav_beacon));
    item->set_element(1, vint_variant(cUI_hint_pan));
    item->set_element(2, vint_variant(cUI_hint_zoom));
    item->set_element(3, vint_variant(cUI_hint_back));
    item->set_element(4, vint_variant(cUI_hint_center));
    const bool on_mission_gps = Cartographer.m_has_player_marker && Cartographer.m_mission_gps && Cartographer.m_player_marker_pos.x == Cartographer.m_mission_gps_pos.x && Cartographer.m_player_marker_pos.y == Cartographer.m_mission_gps_pos.y && Cartographer.m_player_marker_pos.z == Cartographer.m_mission_gps_pos.z;
    item->set_element(5, vint_variant(on_mission_gps));
    item->set_element(6, vint_variant(false));
    item->set_element(7, vint_variant(false));
    item->set_element(8, vint_variant(false));
    item->set_element(9, vint_variant(false));
}

// 0x0087BA80 (:3463)
static void fsm_process_data_items()
{
    fsm_process_data_item_header();
    fsm_process_data_item_district_filter();
    fsm_process_data_item_highlight(false);
    fsm_process_data_item_district_info();
    fsm_process_poi_info();
    fsm_process_data_item_message();
    fsm_process_hints();
}

// 0x00840F10 (:3475)
static void fsm_unload_ui_information()
{
    ui_unload_document(Fs_info.doc_handle, DFOT_NONE, 0.5f);
    ui_relinquish_input_id(Input_id);
    Input_id = -1;
    ui_relinquish_control_id(Fs_info.control_id);
    Fs_info.control_id = 0;
}

// 0x0087EEF0 (:3488)
static void fsm_set_minimap_info()
{
    map_size_info info;
    const float view_width = Fs_info.current_view_bbox.max.x - Fs_info.current_view_bbox.min.x;
    const float view_height = Fs_info.current_view_bbox.max.z - Fs_info.current_view_bbox.min.z;
    info.map_scale.x = view_width == 0.0f ? 0.0f : (Fs_info.map_render_window.max.x - Fs_info.map_render_window.min.x) / view_width;
    info.map_scale.y = view_height == 0.0f ? 0.0f : (Fs_info.map_render_window.max.y - Fs_info.map_render_window.min.y) / view_height;
    const float window_center_x = (Fs_info.map_render_window.min.x + Fs_info.map_render_window.max.x) * 0.5f;
    const float window_center_y = (Fs_info.map_render_window.min.y + Fs_info.map_render_window.max.y) * 0.5f;
    info.center_pos_world = Fs_info.map_center;
    const float half_width = view_width * 0.5f;
    info.zoom_radius = sqrtf(half_width * 2.0f * half_width);
    info.map_posn.x = window_center_x - info.zoom_radius * info.map_scale.x;
    info.map_end_posn.x = window_center_x + info.zoom_radius * info.map_scale.x;
    info.map_end_posn.y = window_center_y + info.zoom_radius * info.map_scale.y;
    info.map_posn.y = window_center_y - info.zoom_radius * info.map_scale.y;
    info.map_size.x = info.map_scale.x * info.zoom_radius * 2.0f;
    info.zoom_radius_squared = info.zoom_radius * info.zoom_radius;
    info.map_size.y = info.map_scale.y * info.zoom_radius * 2.0f;
    Cartographer.m_map_mode = MAP_MODE_FULL;
    Will_switch_to_full = false;
    Cartographer.set_full_map_size_info(info);
    Cartographer.process();
}

// 0x008766D0 (:3552)
static void fsm_obtain_mission_infos()
{
    if (mission_is_active())
    {
        const mission_info* info = mission_info_get(mission_get_name());
        if (info != nullptr)
        {
            Fs_info.mission_infos.add(info);
        }
        return;
    }
    farray<int,64> handles;
    Cartographer.get_mission_handles(handles);
    for (int i = 0; i < handles.size(); i++)
    {
        if ((unsigned int)handles[i] == 0xFFFFFFFF)
        {
            continue;
        }
        const object* objp = World.object_from_handle(handles[i]);
        if (objp != nullptr && objp->get_obj_type() == OT_MISSION_START_NODE)
        {
            const mission_info* info = ((const object_mission_start_node*)objp)->get_mission_info();
            if (info != nullptr)
            {
                Fs_info.mission_infos.add(info);
            }
        }
    }
}

// 0x00876830 (:3580)
static void fsm_init_info()
{
    Fs_info.flags.targeting = 0;
    Fs_info.flags.has_single_static_gids = 0;
    Fs_info.flags.updated_gids = 0;
    Fs_info.flags.zooming_out = 0;
    Fs_info.flags.zooming_in = 0;
    Fs_info.flags.scrolling = 0;
    Fs_info.flags.loaded_one_in = 0;
    Fs_info.flags.rendered_a_grid = 0;
    Fs_info.flags.initial_grids_ready = 0;
    Fs_info.flags.fully_loaded = 0;
    Fs_info.flags.processing = 0;
    Fs_info.flags.allocated = 0;
    Fs_info.flags.unlocked_env = 0;
    Fs_info.flags.force_exit = 0;
    Fs_info.flags.can_exit = 0;
    Fs_info.flags.exiting = 0;
    Fs_info.flags.wants_exit = 0;
    Fs_info.static_bitmap_id = -1;
    Fs_info.side_bitmap_id = -1;
    Fs_info.cpu_mempool_mark = 0;
    Fs_info.gpu_mempool_mark = 0;
    Fs_info.zoom_level = 0.8f; // Magic number: the most zoomed in.
    Fs_info.doc_handle = -1;
    Fs_info.control_id = 0;
    Fs_info.district_value = 0;
    Fs_info.num_data_group_items = 0;
    Fs_info.misc_static_id = bm_get_bogus_bitmap_handle();
    Fs_info.static_alpha_time = 0.5f;
    Fs_info.message_timer.invalidate();
    fsm_display_gps_warning("HM_FSM_GPS_DEFAULT_CAPTION", "HM_FSM_GPS_DEFAULT_DESC");
    Fs_info.desired_state = GS_NONE;
    for (int i = 0; i < 6; i++)
    {
        Fs_info.data_item_handles[i] = -1;
    }
    Fs_info.full_screen_image_info.bitmap_handle = -1;
    Fs_info.target_position.set(0.0f, 0.0f, 0.0f);
    Fs_info.display_gps_warning = false;
    Fs_info.desired_state_priority = false;
    Fs_info.zone_free_info.clear();
    Fs_info.zone_used_info.clear();
    Fs_info.full_screen_image_info.fully_obscured_by_fow = false;
    Fs_info.full_screen_image_info.stream_state = ZSS_UNLOADED;
    Fs_info.full_screen_image_info.gid.x = 0;
    Fs_info.full_screen_image_info.gid.y = 0;
    Fs_info.last_district_handle = 0;
    Fs_info.mission_infos.clear();
    Fs_info.full_screen_image_info.alpha_not_loaded = 0.5f;
    fsm_obtain_mission_infos();
}

// 0x00867F20 (:3640)
void fsm_free_streaming()
{
    fsm_cancel_streaming();
    if (Fs_info.flags.unlocked_env)
    {
        return;
    }
    Fullscreen_map_stream_allocator.destroy();
    if (Mempool_to_use != nullptr)
    {
        // Magic number: 0x40 is the pool's split flag.
        if ((Mempool_to_use->get_flags() & 0x40) == 0)
        {
            Mempool_to_use->restore_to_mark(Fs_info.cpu_mempool_mark);
        }
        else
        {
            ((split_mempool*)Mempool_to_use)->restore_to_mark_cpu(Fs_info.cpu_mempool_mark);
        }
        if ((Interface_mempool.get_flags() & 0x40) == 0)
        {
            Interface_mempool.restore_to_mark(Fs_info.inteface_mempool_mark);
        }
        else
        {
            Interface_mempool.restore_to_mark_cpu(Fs_info.inteface_mempool_mark);
        }
        if ((Mempool_to_use->get_flags() & 0x40) != 0)
        {
            ((split_mempool*)Mempool_to_use)->restore_to_mark_gpu(Fs_info.gpu_mempool_mark);
        }
    }
    salloc_gameplay_gpu_unlock_async();
    Fs_info.flags.unlocked_env = 1;
    Mempool_to_use = nullptr;
}

// 0x00867FC0 (:3696)
static void fsm_handle_exiting()
{
    fsm_cancel_streaming();
    if (Fs_info.zone_used_info.size() != 0)
    {
        return;
    }
    if (!Fs_info.flags.can_exit)
    {
        Fs_info.flags.can_exit = 1;
        fsm_free_streaming();
        return;
    }
    if (!hud_ui_is_loaded())
    {
        hud_ui_process();
        return;
    }
    if (salloc_gameplay_gpu_unlock_async_poll())
    {
        if (Fs_info.desired_state == GS_NONE)
        {
            gameseq_pop_state();
            return;
        }
        gameseq_set_state(Fs_info.desired_state, Fs_info.desired_state_priority);
    }
}

// 0x00840F70 (:3735)
static bool fsm_check_user_wants_to_exit()
{
    static const char* kHintExit = "hint_group1"; // 0x016621FC
    bool clicked = false;
    vint_document* doc = ui_get_vint_document(Fs_info.doc_handle);
    if (doc == nullptr)
    {
        return false;
    }
    vint_object_base* hint = vint_object_base::find_object(kHintExit, doc);
    if (hint != nullptr)
    {
        if (ui_is_mouse_over_vint_object(hint))
        {
            ui_pc_set_object_opacity(hint, 1.0f);
            if (mouse_just_released(0))
            {
                clicked = true;
            }
        }
        else
        {
            ui_pc_set_object_opacity(hint, 0.5f);
        }
    }
    // Magic numbers: ui buttons 0 and 11 leave the map.
    if (ui_get_button_info(0, Input_id) <= 0.0f && ui_get_button_info(11, Input_id) <= 0.0f && !clicked)
    {
        return false;
    }
    fsm_stop_all_sounds(false);
    audiolib_cue_play(Ga_cue_ids.map_close, nullptr, nullptr);
    hud_ui_reload_items();
    return true;
}

// 0x0087F120 (:3790)
static void fsm_process_normally(float dtime)
{
    Mouse_has_clicked_hint = false;
    const vector2i& screen = ui_get_screen_dimensions();
    Fs_info.map_render_window.max.x = (float)screen.x;
    Fs_info.map_render_window.min.x = 0.0f;
    Fs_info.map_render_window.min.y = 0.0f;
    Fs_info.map_render_window.min.z = 0.0f;
    Fs_info.map_render_window.max.z = 0.0f;
    Fs_info.map_render_window.max.y = (float)screen.y;
    fsm_determine_district_filters(dtime);
    if (Fs_info.flags.fully_loaded)
    {
        fsm_determine_zoom_level(dtime);
        fsm_determine_map_posn(dtime);
    }
    fsm_determine_player_marker();
    fsm_set_minimap_info();
    fsm_determine_view_extents();
    fsm_determine_zones_to_load();
    fsm_process_data_items();
    input_set_feedback(Player_controller_index, 0.0f, 0.0f);
}

// 0x00854CD0 (:3848)
void fsm_loaded_callback(unsigned int srid)
{
    char filename[128];
    if (Use_full_screen_image && srid != 0 && srid == Fs_info.full_screen_image_srid)
    {
        _snprintf_safe(filename, 0x80, "%s_map.tga", Fs_info.territory_name);
        Fs_info.full_screen_image_info.bitmap_handle = bm_load(filename, false);
        Fs_info.flags.loaded_one_in = 1;
        Fs_info.full_screen_image_info.stream_state = ZSS_LOADED;
        return;
    }
    for (int i = 0; i < Fs_info.zone_used_info.size(); i++)
    {
        zone_map_information* zmi = Fs_info.zone_used_info[i];
        if (fsm_get_stream_resource_id(zmi->gid.x, zmi->gid.y) == srid)
        {
            _snprintf_safe(filename, 0x80, "%s_%02d_%02d_map.tga", Fs_info.territory_name, zmi->gid.x, zmi->gid.y);
            zmi->bitmap_handle = bm_load(filename, false);
            zmi->stream_state = ZSS_LOADED;
            Fs_info.flags.loaded_one_in = 1;
            return;
        }
    }
}

// 0x00854E00 (:3880)
void fsm_unloaded_callback(unsigned int srid)
{
    if (Use_full_screen_image && srid != 0 && srid == Fs_info.full_screen_image_srid)
    {
        Fs_info.full_screen_image_info.stream_state = ZSS_UNLOADED;
        return;
    }
    for (int i = 0; i < Fs_info.zone_used_info.size(); i++)
    {
        zone_map_information* zmi = Fs_info.zone_used_info[i];
        if (zmi != nullptr && fsm_get_stream_resource_id(zmi->gid.x, zmi->gid.y) == srid)
        {
            zmi->stream_state = ZSS_UNLOADED;
        }
    }
}

// 0x00876960 (:3903)
void fullscreen_map_terr_info_init(int dlc_id)
{
    char filename[32];
    if (dlc_id == -1)
    {
        _snprintf_safe(filename, 0x20, "terr01%s", ".fsmib");
    }
    else
    {
        _snprintf_safe(filename, 0x20, "dlc%02d%s", dlc_id, ".fsmib");
    }
    fsm_points_of_interest.clear();
    Cartographer.fsm_district_liberated_icons.clear();
    cfile* file = cf_open(filename, "rb", PLATFORM_PC, false);
    if (file == nullptr)
    {
        return;
    }
    mempool_mark_scope scope(&Common_mempool);
    const unsigned int size = cf_size(file);
    unsigned char* buffer = (unsigned char*)Common_mempool.alloc(size, 0);
    cf_read(buffer, size, file, false);
    cf_close(file);
    file = cf_open(buffer, size, "rb", PLATFORM_PC);
    for (unsigned int count = cf_read_uint8(file); count != 0; count--)
    {
        fsm_poi poi;
        cf_read(&poi, sizeof(poi), file, false);
        // Magic numbers: a name starting with "ui" is a liberated district icon.
        if (poi.poi_text[0] == 'u' && poi.poi_text[1] == 'i')
        {
            Cartographer.add_district_liberated_icon(poi.x, poi.z, poi.zone_xz);
        }
        else
        {
            fsm_points_of_interest.add(poi);
        }
    }
    Cartographer.load_district_boundries(file);
    cf_close(file);
}

// 0x00882C90 (:3952)
void fullscreen_map_init()
{
    load_objectives();
    fog_of_war_init();
    fullscreen_map_terr_info_init(-1);
    state_info info;
    info.enter = fullscreen_map_enter;
    info.exit = fullscreen_map_exit;
    info.process = fullscreen_map_process;
    info.handshake = fullscreen_map_handshake;
    info.render = fullscreen_map_render;
    info.exit_notify = fullscreen_map_exit_notify;
    info.transparent = false;
    info.pause_beneath = false;
    gameseq_register_state(GS_INGAME_FULLSCREEN_MAP, info);
}

// 0x00868020 (:3979)
void fullscreen_map_init_streaming_info()
{
    // Magic numbers: larger than any world position, so the first zone sets them.
    Fs_info.world_extents_bbox.min.set(10000000.0f, 10000000.0f, 10000000.0f);
    Fs_info.world_extents_bbox.max.set(-10000000.0f, -10000000.0f, -10000000.0f);
    // Original bug, kept: the territory name is passed as the format string.
    _snprintf_safe(Fs_info.territory_name, 0x20, World.get_territory_name());
    char container_name[128];
    for (int grid_x = 0; grid_x < 16; grid_x++)
    {
        for (int grid_y = 0; grid_y < 16; grid_y++)
        {
            const short index = fsm_get_stream_information_index(grid_x, grid_y);
            sprintf(container_name, "%s_%02d_%02d_map", Fs_info.territory_name, grid_x, grid_y);
            stream_information& info = Fs_info.stream_information[index];
            const unsigned int srid = stream2_container_find(container_name);
            info.srid = srid;
            if (srid == 0)
            {
                info.exists = false;
                info.size_ok = false;
                info.corner_radius = 1.0f;
                info.srid = 1; // Magic number: srid 1 means the zone has the bogus image.
                continue;
            }
            info.exists = true;
            info.district_handle = 0xFFFFFFFF;
            stream2_container* container = stream2_get_container(srid);
            if (container != nullptr)
            {
                int cpu_bytes;
                int cpu_align;
                int gpu_bytes;
                int gpu_align;
                container->calc_container_requirements(cpu_bytes, cpu_align, gpu_bytes, gpu_align);
                // Magic number: the size of a zone image's slot in fsm_allocator_setup.
                if (cpu_bytes + gpu_bytes <= 0x20400)
                {
                    info.size_ok = true;
                    info.corner_radius = fl_rand_range(1.0f, 2.5f);
                }
                else
                {
                    info.size_ok = false;
                    info.corner_radius = 1.0f;
                    info.srid = 1;
                }
            }
            vector zone_min;
            vector zone_max;
            rfg_zonex_get_zone_bbox(grid_x, grid_y, zone_min, zone_max);
            if (zone_min.x < Fs_info.world_extents_bbox.min.x)
            {
                Fs_info.world_extents_bbox.min.x = zone_min.x;
            }
            if (zone_min.z < Fs_info.world_extents_bbox.min.z)
            {
                Fs_info.world_extents_bbox.min.z = zone_min.z;
            }
            if (Fs_info.world_extents_bbox.max.x < zone_max.x)
            {
                Fs_info.world_extents_bbox.max.x = zone_max.x;
            }
            if (Fs_info.world_extents_bbox.max.z < zone_max.z)
            {
                Fs_info.world_extents_bbox.max.z = zone_max.z;
            }
        }
    }
    if (Use_full_screen_image)
    {
        sprintf(container_name, "%s_map", Fs_info.territory_name);
        Fs_info.full_screen_image_srid = stream2_container_find(container_name);
    }
    for (int i = 0; i < World.get_num_objects<district>(); i++)
    {
        district* dp = World.get_object<district>(i);
        if (dp == nullptr)
        {
            continue;
        }
        const farray<grid_info,21>& gids = dp->get_gid_list();
        for (int j = 0; j < gids.size(); j++)
        {
            unsigned char grid_x;
            unsigned char grid_y;
            dp->gid_to_grid_index(grid_x, grid_y, gids[j].gid);
            const short index = fsm_get_stream_information_index(grid_x, grid_y);
            if (index != -1)
            {
                Fs_info.stream_information[index].district_handle = dp->get_handle();
            }
        }
    }
    Fs_info.district_value = 0;
}

// 0x00876AC0 (:4087)
void fullscreen_map_enter(game_state prev_state)
{
    (void)prev_state;
    fsm_init_info();
    Fs_info.doc_handle = ui_load_document(cUI_fullscreen_doc, DFIT_ALPHA, 0.0f, DLT_NORMAL, true, STREAM_PRIORITY_AUDIO_CACHE_LOAD, false);
    if (Fs_info.doc_handle == -1)
    {
        keen::debug::nativePrintArgs("Int3 critical function at %s(%d)\n", __FILE__, __LINE__);
        Fs_info.flags.can_exit = 1;
        Fs_info.flags.wants_exit = 1;
        gameseq_pop_state();
        return;
    }
    hud_ui_unload_items();
    fsm_get_sound_handles();
    Fs_info.control_id = ui_request_control_id();
    game_pause(false);
    audiolib_cue_play(Ga_cue_ids.map_open, nullptr, nullptr);
    ui_wait_for_button_ups();
    if (Most_recent_icon == nullptr)
    {
        Fs_info.map_center = Rfg_camera.real_pos;
    }
    else
    {
        Fs_info.map_center.set(Most_recent_icon->pos.x, 0.0f, Most_recent_icon->pos.y);
    }
    const vector2i& screen = ui_get_screen_dimensions();
    Fs_info.map_render_window.max.x = (float)screen.x;
    Fs_info.map_render_window.max.y = (float)screen.y;
    Fs_info.map_render_window.min.x = 0.0f;
    Fs_info.map_render_window.min.y = 0.0f;
    Fs_info.map_render_window.min.z = 0.0f;
    Fs_info.map_render_window.max.z = 0.0f;
    audiolib_music_set_fsm_state(true);
    ui_fade_to_black_instant_fade_in();
}

// 0x00868390 (:4143)
void fullscreen_map_exit(game_state new_state)
{
    if (new_state == GS_SHUTDOWN)
    {
        return;
    }
    fsm_stop_all_sounds(false);
    Fs_info.flags.can_exit = 0;
    Fs_info.flags.exiting = 0;
    fog_of_war_rendering_dealloc();
    fsm_unload_ui_information();
    Cartographer.m_map_mode = MAP_MODE_MINI;
    Found_icon = false;
    Most_recent_icon = nullptr;
    Cartographer.m_temp_icons.clear();
    Cartographer.m_collected_structures_timer[0].set(0);
    Will_switch_to_full = false;
    Fs_info.zoom_level = 1.5f; // Magic number: the asm writes this value, past the 0.8 maximum; fsm_init_info resets it.
    if (game_is_paused())
    {
        game_unpause();
    }
    audiolib_music_set_fsm_state(false);
}

// 0x0087F2C0 (:4187)
void fullscreen_map_process()
{
    const bool msgbox_active = ui_is_msgbox_active(-2); // Magic number: -2, any message box.
    if (!msgbox_active)
    {
        Fs_info.static_bitmap_flip = (Fs_info.static_bitmap_flip + 1) % 16; // Magic number: 4 UV_FLIP_COUNTER steps of 4 frames.
        // Magic numbers: the scroll speed runs from 25 at the most zoomed out to 5 at the most zoomed in.
        Scroll_speed = 25.0f - ((0.1f - Fs_info.zoom_level) * 20.0f) / -0.7f;
        if (!Fs_info.flags.exiting && Fs_info.flags.allocated)
        {
            if (!Fs_info.flags.wants_exit && Fs_info.flags.fully_loaded)
            {
                Fs_info.flags.wants_exit = fsm_check_user_wants_to_exit();
                if (Fs_info.flags.wants_exit)
                {
                    fsm_stop_all_sounds(false);
                }
            }
            if (!Fs_info.flags.wants_exit && !Fs_info.flags.exiting)
            {
                Fs_info.flags.processing = 1;
                fsm_process_normally(Frametime_real);
            }
            else
            {
                fsm_process_data_item_highlight(true);
                fsm_process_poi_info();
            }
            ui_add_for_processing(Fs_info.doc_handle);
        }
        fsm_handle_sounds(Frametime_real);
        gamemusic_update(true);
    }
    interface_process();
    ui_process(Fs_info.control_id, msgbox_active);
}

// 0x0087BAB0 (:4240)
void fullscreen_map_handshake(rl_renderer* pRenderer)
{
    if (Fs_info.flags.exiting)
    {
        fsm_handle_exiting();
    }
    else if (!Fs_info.flags.allocated)
    {
        switch (ui_get_doc_asset_state(Fs_info.doc_handle))
        {
        case ST_UNLOADED:
            keen::debug::nativePrintArgs("Int3 critical function at %s(%d)\n", __FILE__, __LINE__);
            [[fallthrough]];
        case ST_ERROR:
            ui_unload_document(Fs_info.doc_handle, DFOT_NONE, 0.5f);
            Fs_info.flags.can_exit = 1;
            Fs_info.flags.wants_exit = 1;
            gameseq_pop_state();
            break;
        case ST_LOADING:
            ui_force_unloads_complete();
            break;
        case ST_LOADED:
            fsm_load_ui_information(*pRenderer);
            fsm_allocator_setup();
            Fs_info.flags.allocated = 1;
            if (Use_full_screen_image)
            {
                stream2_request(Fs_info.full_screen_image_srid);
            }
            Input_id = ui_register_for_processed_input(IRT_UNLIMITED_INPUT, false);
            break;
        default:
            keen::debug::nativePrintArgs("Int3 critical function at %s(%d)\n", __FILE__, __LINE__);
            break;
        }
    }
    if (Show_minimap && Usability_hud_enable && Cartographer.m_map_mode == MAP_MODE_MINI)
    {
        Cartographer.collect_terrain();
    }
    Cartographer.m_collected_structure_buf_index = Cartographer.m_collected_structure_buf_index == 0 ? 1 : 0;
}

// 0x00876C10 (:4292)
void fullscreen_map_render(rl_renderer*)
{
    gr_set_color(0, 0, 0, 255);
    gr_rect(0.0f, 0.0f, Fs_info.map_render_window.max.x - Fs_info.map_render_window.min.x, Fs_info.map_render_window.max.y - Fs_info.map_render_window.min.y, GM_FILTER);
    fsm_determine_loaded_zones();
    const vector2 tl(Fs_info.map_render_window.min.x, Fs_info.map_render_window.min.y);
    const vector2 tr(Fs_info.map_render_window.max.x, Fs_info.map_render_window.min.y);
    const vector2 bl(Fs_info.map_render_window.min.x, Fs_info.map_render_window.max.y);
    const vector2 br(Fs_info.map_render_window.max.x, Fs_info.map_render_window.max.y);
    if (!Fs_info.flags.loaded_one_in || Fs_info.flags.exiting)
    {
        Fs_info.flags.has_single_static_gids = 0;
        fsm_render_static_bitmap(true, bl, br, tl, tr, Fs_info.static_alpha_time * 2.0f);
    }
    else
    {
        fsm_render_territories();
        Cartographer.render_pre_fog();
        fog_of_war_render(Fs_info.current_view_bbox, Fs_info.map_render_window);
        fsm_render_districts();
        if (Fs_info.static_alpha_time > 0.0f)
        {
            fsm_render_static_bitmap(true, bl, br, tl, tr, Fs_info.static_alpha_time * 2.0f);
        }
    }
    if (Fs_info.flags.allocated && Fs_info.flags.processing)
    {
        ui_add_for_rendering(Fs_info.doc_handle);
    }
    ui_render(Fs_info.control_id, false);
    ui_fade_to_black_render();
}

// 0x00841120 (:4363)
void fullscreen_map_handle_state_change(game_state new_state, bool high_priority)
{
    if (Fs_info.desired_state == GS_NONE || (Fs_info.desired_state != new_state && high_priority))
    {
        Fs_info.desired_state = new_state;
        Fs_info.desired_state_priority = high_priority;
        fsm_stop_all_sounds(false);
        Fs_info.flags.wants_exit = 1;
    }
}

// 0x00841220 (:4394)
void fullscreen_map_get_zoom_info(fsmap_zoom_info& fzi)
{
    fzi.curr_zoom = Fs_info.zoom_level;
    fzi.min_zoom = 0.1f;
    fzi.max_zoom = 0.8f;
}

// 0x00841250 (:4401)
bool fsm_posn_is_locked_on(const vector& pos)
{
    const float dx = Fs_info.map_center.x - pos.x;
    const float dy = Fs_info.map_center.y - pos.y;
    const float dz = Fs_info.map_center.z - pos.z;
    const float dist = (dz * dz + dy * dy + dx * dx) - 0.0f;
    return dist < Lock_target_dist_squared * Scroll_speed && -dist < Lock_target_dist_squared * Scroll_speed;
}

// 0x00841340 (:4406)
float fullscreen_map_get_zoom_pct()
{
    return (Fs_info.zoom_level - 0.1f) / 0.7f;
}

// 0x00841360 (:4412)
void fsm_display_gps_warning(const char* caption, const char* desc)
{
    if (caption != nullptr)
    {
        strcpy(Fs_info.gps_warning_caption, caption);
    }
    if (desc != nullptr)
    {
        strcpy(Fs_info.gps_warning_desc, desc);
    }
    Fs_info.display_gps_warning = true;
    Fs_info.message_timer.set(2000); // Magic number: two seconds.
}

// 0x008413C0 (:4426)
vector fsm_get_map_center()
{
    return Fs_info.map_center;
}

// 0x008413F0 (:4431)
bool fullscreen_map_is_loaded()
{
    return !Fs_info.flags.wants_exit && !Fs_info.flags.exiting && Fs_info.flags.fully_loaded && Fs_info.flags.rendered_a_grid;
}

// 0x00841410 (:4436)
bool fullscreen_map_exit_notify(game_state new_state)
{
    (void)new_state;
    if (!Fs_info.flags.wants_exit)
    {
        Fs_info.flags.force_exit = 1;
        Fs_info.flags.wants_exit = 1;
        fsm_stop_all_sounds(false);
    }
    if (Fs_info.flags.can_exit && Fs_info.flags.force_exit)
    {
        return salloc_gameplay_gpu_unlock_async_poll();
    }
    return Fs_info.flags.can_exit;
}
