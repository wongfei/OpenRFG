// Original: volition/rfg/code/multi/multi_game/multi_game_backpack_rack.cpp
//
// Decompiled whole (multiplayer.md MP10). The backpack racks: MMT_BACKPACK_RACK markers, most on a building
// (rfg_mover) whose attach tags hold the backpacks. The server fills them at each round begin (the map's backpack, one
// chosen by the server options, or shuffled among the racks of a group and team) and sends their types to the clients
// (PT_SYNC_BACKPACK_RACKS), which hang the same backpacks. A backpack taken off a rack is replaced at once by a new
// one. A destroyed rack building loses its backpacks (but those being worn) until it is repaired; the repairing
// player's team owns it then. A rack's popup (its backpack's name and a box per backpack still on it) shows over a
// rack the local player looks at.
// Part of the multi unity TU (rfg_multi_unity.obj, every multi/ .cpp in path order): the file statics are shared
// by the whole tree, and a shared one is placed in its first user (tools/hdrgen/specs/mp0_stubs.py).

#include "volition/rfg/code/multi/multi_game/multi_game_backpack_rack.h"

#include "volition/interface_lib/code/bitmap_sheets.h"
#include "volition/render_lib/code/gr/gr.h"
#include "volition/render_lib/code/gr/gr_font.h"
#include "volition/rfg/code/camera/rfg_camera.h"
#include "volition/rfg/code/destruction/rfg_mover.h"
#include "volition/rfg/code/effects/effects_shared.h"
#include "volition/rfg/code/havok/havok_raycast.h"
#include "volition/rfg/code/hud/hud.h"
#include "volition/rfg/code/hud/minimap.h"
#include "volition/rfg/code/interface/localization/localization_shared.h"
#include "volition/rfg/code/interface/ui_document_hud.h"
#include "volition/rfg/code/level/world.h"
#include "volition/rfg/code/level/world.inl"
#include "volition/rfg/code/multi/multi_core/include/multi.h"
#include "volition/rfg/code/multi/multi_core/include/multi_connection.h"
#include "volition/rfg/code/multi/multi_core/include/multi_core_io.h"
#include "volition/rfg/code/multi/multi_core/include/multi_core_objects.h"
#include "volition/rfg/code/multi/multi_core/include/multi_session.h"
#include "volition/rfg/code/multi/multi_game/modes/multi_game_mode_common.h"
#include "volition/rfg/code/multi/multi_game/multi_game.h"
#include "volition/rfg/code/multi/multi_game/multi_game_object_backpack.h"
#include "volition/rfg/code/multi/multi_game/multi_game_object_marker.h"
#include "volition/rfg/code/multi/multi_game/multi_game_settings.h"
#include "volition/rfg/code/objects/humans/human_teams.h"
#include "volition/rfg/code/objects/humans/player.h"
#include "volition/rfg/code/rfg.h"
#include "volition/vlib/code/math/rand.h"
#include "volition/vlib/code/util/vfont.h"
#include "volition/vlib/code/util/varray.h"
#include "volition/rfg/code/gamewide/tweak_table.h"

#include <stdio.h>

// The rack groups the shuffle sorts the racks into, and the racks of each.
static const int MAX_BACKPACK_RACK_GROUPS = 32;
static const int MAX_RACKS_PER_GROUP = 128;

// ---- file-static globals
// const char* EMPTY_CHECKBOX_NAME (0x0167E50C) is used here too; placed in multi_game_mode_common.cpp.
// int Empty_checkbox_image (0x0167E510) is used here too; placed in multi_game_mode_common.cpp.
static float Mp_backpack_rack_popup_min_dot = 0.975f; // 0x0167E514, how straight the camera looks at the rack
// Placed with its variable: rfg.exe's item is multi_voice.cpp's (later in this unity TU).
static tweak_table_item Tweak_table_item_Mp_backpack_rack_popup_min_dot("Mp_backpack_rack_popup_min_dot", &Mp_backpack_rack_popup_min_dot); // 0x02FEB770
static float Mp_backpack_rack_popup_max_visible_distance = 50.0f; // 0x0167E518
static float Mp_backpack_rack_popup_text_scale = 0.55f; // 0x0167E51C
static float Mp_backpack_rack_popup_checkbox_scale = 1.0f; // 0x0167E520
// multi_camera_mode Current_mode_camera (0x0167E544) is used here too; placed in multi_game_mode_common.cpp.
// dynamic_uint_hash_table<remote_object*> Game_handle_to_rop_hash (0x01691B40) is used here too; placed in multi_core_delta_compression.cpp.
// dynamic_uint_hash_table<remote_object*> Network_handle_to_rop_hash (0x01691B88) is used here too; placed in multi_core_delta_compression.cpp.
// dynamic_uint_hash_table<network_game_handle*> Reserved_game_to_network_hash (0x01691C18) is used here too; placed in multi_core_objects.cpp.
// dynamic_uint_hash_table<network_game_handle*> Reserved_network_to_game_hash (0x01691C60) is used here too; placed in multi_core_delta_compression.cpp.
static color CHECKBOX_COLOR(0xFE, 0xD7, 0x3C, 0xFF); // 0x01692580, initial data fed73cff
// farray<backpack_rack_popup,4> Backpack_rack_popups (0x01692588) is used here too; placed in multi_game_mode_common.cpp.
static multi_backpack_info* Backpack_info[16]; // 0x02FE8D18
// bool Server (0x02FEB590) is used here too; placed in multi_core_session_generic.cpp.
// bool Dedicated (0x02FEB591) is used here too; placed in multi.cpp.
// bool Spectating (0x02FEB659) is used here too; placed in multi.cpp.
// (a static initializer in rfg.exe; the tweak table is not ported).

// ---- internal functions
// The rack's building (an rfg_mover parent), null for none.
static rfg_mover* rack_building(const multi_object_marker* rack)
{
    const unsigned int parent = rack->get_parent();
    if (parent == 0xFFFFFFFF)
    {
        return nullptr;
    }
    object* objp = World.object_from_handle(parent);
    if (objp == nullptr || objp->get_obj_type() != OT_MOVER || objp->get_sub_type() != OT_SUB_MOVER_RFG)
    {
        return nullptr;
    }
    return (rfg_mover*)objp;
}

static multi_object_backpack* backpack_from_handle(unsigned int handle)
{
    if (handle == 0xFFFFFFFF)
    {
        return nullptr;
    }
    object* objp = World.object_from_handle(handle);
    if (objp == nullptr || objp->get_obj_type() != OT_ITEM || objp->get_sub_type() != OT_SUB_ITEM_MULTI_BACKPACK)
    {
        return nullptr;
    }
    return (multi_object_backpack*)objp;
}

// The rack of a network handle (the reserved handles first, then the remote objects), null when unknown.
static multi_object_marker* rack_from_network_handle(unsigned short net_handle)
{
    if (net_handle == 0xFFFF)
    {
        return nullptr;
    }
    object* objp = World.object_from_handle(multi_core_game_handle_from_network_handle(net_handle, false));
    if (objp == nullptr || objp->get_obj_type() != OT_MULTI_MARKER)
    {
        return nullptr;
    }
    return (multi_object_marker*)objp;
}

// 0x009ABB70 (:88): the rack's backpacks (an unlocked backpack type only), hung on its building's tags, and the rack
// light for the local player (neutral, allied blue, enemy red). LTCG: rack in EDI.
static void create_backpacks_on_rack(multi_object_marker* rack)
{
    rfg_mover* building = rack_building(rack);
    // Original: the info is not checked for null.
    const multi_backpack_info* binfo = multi_game_backpack_get_info(rack->backpack_id);
    if (multi_game_settings_is_backpack_locked(binfo->unique_id))
    {
        return;
    }
    const matrix orient = rack->orient;
    const vector pos = rack->pos;
    for (int i = 0; i < rack->num_backpacks; i++)
    {
        multi_object_backpack* backpack = multi_game_backpack_create(rack->backpack_id, pos, orient);
        if (backpack == nullptr)
        {
            return;
        }
        backpack->set_backpack_rack(rack, i);
        if (rack->can_attach_packs() && building != nullptr)
        {
            building->set_child(backpack, i, backpack->get_backpack_rack_attach_tag_index(), false, true);
        }
        rack->backpacks[i] = backpack->get_handle();
    }
    player* local = player_get_local_player();
    if (local == nullptr || building == nullptr || rack->destroyed)
    {
        return;
    }
    const char* name;
    if (rack->team == HUMAN_TEAM_CIVILIAN)
    {
        name = "mp_rack_light_neutral";
    }
    else if (human_teams_get_disposition(local->current_team, rack->team) == TEAM_DISPOSITION_ALLIED)
    {
        name = "mp_rack_light_blue";
    }
    else
    {
        name = "mp_rack_light_red";
    }
    const effect_info* light = effects_get_effect_info(name);
    if (light != nullptr)
    {
        rack->backpack_rack_light = effects_play(light, building->get_handle(), -1, true, 0xFFFFFFFF, 1.0f, false);
    }
}

// 0x009790A0 (:140): the popup's line of sight passes the local player, its children, the rack's building and the rack's
// own backpacks
static bool backpack_rack_popup_filter(object* hit_obj, void* void_rack)
{
    const multi_object_marker* rack = (const multi_object_marker*)void_rack;
    player* local = player_get_local_player();
    if (hit_obj == local || (local != nullptr && hit_obj->get_parent() == local->get_handle()) || hit_obj->get_handle() == rack->get_parent())
    {
        return false;
    }
    for (int i = 0; i < rack->num_backpacks; i++)
    {
        const multi_object_backpack* backpack = backpack_from_handle(rack->backpacks[i]);
        if (hit_obj == backpack && backpack->get_parent() == rack->get_parent())
        {
            return false;
        }
    }
    return true;
}

// 0x009BC5F0 (:693): server: the rack's building was destroyed
static void send_backpack_rack_destroyed(multi_object_marker* rack)
{
    if (rack == nullptr)
    {
        return;
    }
    unsigned char packet[1200];
    packet[0] = PT_BACKPACK_RACK_DESTROYED;
    unsigned short packet_size = packet_header_size(PT_BACKPACK_RACK_DESTROYED);
    write_u16_be(packet, packet_size, multi_core_network_handle_from_game_handle(rack->get_handle(), false));
    packet_set_size(packet, packet_size);
    multi_io_send_to_all_reliable(multi_session_get_by_type(ST_GAME), packet, packet_size);
}

// 0x009BC690 (:719): server: the rack's building was repaired by a team
static void send_backpack_rack_repaired(multi_object_marker* rack, human_teams repair_team)
{
    if (rack == nullptr)
    {
        return;
    }
    unsigned char packet[1200];
    packet[0] = PT_BACKPACK_RACK_REPAIRED;
    unsigned short packet_size = packet_header_size(PT_BACKPACK_RACK_REPAIRED);
    write_u16_be(packet, packet_size, multi_core_network_handle_from_game_handle(rack->get_handle(), false));
    packet[packet_size++] = (unsigned char)repair_team;
    packet_set_size(packet, packet_size);
    multi_io_send_to_all_reliable(multi_session_get_by_type(ST_GAME), packet, packet_size);
}

// ---- external functions
// 0x00979130 (:169)
void multi_game_backpack_rack_level_init_post()
{
    Empty_checkbox_image = vint_bms_load(EMPTY_CHECKBOX_NAME);
    Backpack_rack_popups.clear();
}

// 0x009DC6E0 (:180): server: the racks' backpack types (the server's choice for every rack, or the racks of each
// random group and team, or with shuffle every rack's group, swapping their types at random), then their backpacks,
// then the types to the clients
void multi_game_backpack_rack_round_begin()
{
    if (Mp_server_options.generic_options.backpack_spawns == MGBSO_NONE || !Multiplayer || !Server)
    {
        return;
    }
    const int num_markers = World.get_num_objects<multi_object_marker>();
    if (Mp_server_options.generic_options.backpack_spawns == MGBSO_SPECIFIC)
    {
        for (int i = num_markers - 1; i >= 0; i--)
        {
            if (Multi_markers_prioritized[i]->get_marker_type() == MMT_BACKPACK_RACK)
            {
                Multi_markers_prioritized[i]->backpack_id = Mp_server_options.generic_options.backpack_spawn_type;
            }
        }
    }
    else
    {
        // The racks to shuffle by group (and team): a random rack's group, or group 0 when every rack shuffles.
        // Deviation from rfg.exe: this and the types array are function-local statics, where they are on its stack.
        static farray<multi_object_marker*, MAX_RACKS_PER_GROUP> groups[MAX_BACKPACK_RACK_GROUPS];
        int num_groups = 0;
        for (int g = 0; g < MAX_BACKPACK_RACK_GROUPS; g++)
        {
            groups[g].clear();
        }
        for (int i = num_markers - 1; i >= 0; i--)
        {
            multi_object_marker* rack = Multi_markers_prioritized[i];
            if (rack->get_marker_type() != MMT_BACKPACK_RACK || (!rack->is_random_rack() && Mp_server_options.generic_options.backpack_spawns != MGBSO_SHUFFLE))
            {
                continue;
            }
            const int group = rack->is_random_rack() ? rack->backpack_rack_group : 0;
            int found = -1;
            for (int g = 0; g < num_groups; g++)
            {
                if (groups[g][0]->backpack_rack_group == group && groups[g][0]->team == rack->team)
                {
                    found = g;
                    break;
                }
            }
            if (found != -1)
            {
                groups[found].add(rack);
                continue;
            }
            // Original: a 33rd group overruns the 32 (only "You can only have %d backpack rack groups." is formatted).
            groups[num_groups].add(rack);
            num_groups++;
        }
        // Each group's backpack types shuffled among its racks.
        static farray<int, MAX_RACKS_PER_GROUP> types[MAX_BACKPACK_RACK_GROUPS];
        for (int g = 0; g < MAX_BACKPACK_RACK_GROUPS; g++)
        {
            types[g].clear();
        }
        for (int g = 0; g < num_groups; g++)
        {
            for (int i = 0; i < groups[g].size(); i++)
            {
                types[g].add(groups[g][i]->backpack_id);
            }
            const int count = groups[g].size();
            for (int i = 0; i < count; i++)
            {
                const int j = int_rand_range(0, count - 1);
                const int swap = types[g][i];
                types[g][i] = types[g][j];
                types[g][j] = swap;
            }
            for (int i = 0; i < groups[g].size(); i++)
            {
                groups[g][i]->backpack_id = types[g][i];
            }
        }
    }
    for (int i = World.get_num_objects<multi_object_marker>() - 1; i >= 0; i--)
    {
        if (Multi_markers_prioritized[i]->get_marker_type() == MMT_BACKPACK_RACK)
        {
            create_backpacks_on_rack(Multi_markers_prioritized[i]);
        }
    }
    send_backpack_rack_sync(nullptr);
}

// 0x0094F190 (:270)
void multi_game_backpack_rack_round_end()
{
    vint_bms_release(Empty_checkbox_image);
}

// 0x009F0200 (:277): the server watches the rack buildings (destroyed, repaired); every player but a spectator (and a
// dedicated server) fades the popups and opens one for a rack the camera looks at, near enough and in sight
void multi_game_backpack_rack_process()
{
    if (Multiplayer && Server)
    {
        for (int i = World.get_num_objects<multi_object_marker>() - 1; i >= 0; i--)
        {
            multi_object_marker* rack = Multi_markers_prioritized[i];
            if (rack->get_marker_type() != MMT_BACKPACK_RACK || rack->get_parent() == 0xFFFFFFFF)
            {
                continue;
            }
            // Original: the building is not checked for null.
            rfg_mover* building = rack_building(rack);
            if (rack->destroyed)
            {
                if (!building->is_killed())
                {
                    human_teams repairing_team = HUMAN_TEAM_NONE;
                    player* repairer = multi_game_last_player_to_repair_rfg_mover(building);
                    if (repairer != nullptr)
                    {
                        repairing_team = repairer->current_team;
                    }
                    multi_game_backpack_rack_repaired(rack, repairing_team);
                }
            }
            else if (building->is_killed())
            {
                multi_game_backpack_rack_destroyed(rack);
            }
        }
        if (Dedicated)
        {
            return;
        }
    }
    if (Spectating)
    {
        return;
    }
    for (int i = 0; i < Backpack_rack_popups.size(); i++)
    {
        backpack_rack_popup& popup = Backpack_rack_popups[i];
        if (popup.fade_out_timer.elapsed())
        {
            Backpack_rack_popups.remove(i);
            i--;
        }
        else if (popup.fade_in_timer.elapsed())
        {
            popup.fade_in_timer.invalidate();
        }
    }
    for (int i = World.get_num_objects<multi_object_marker>() - 1; i >= 0; i--)
    {
        multi_object_marker* rack = Multi_markers_prioritized[i];
        if (rack->get_marker_type() != MMT_BACKPACK_RACK)
        {
            continue;
        }
        rfg_mover* building = rack_building(rack);
        if (building == nullptr)
        {
            continue;
        }
        vector bmin;
        vector bmax;
        building->get_local_bbox(bmin, bmax);
        // Magic number: the popup is at 60% of the building's height.
        const float height = (bmax.y - bmin.y) * 0.6f;
        vector to_rack = building->pos + building->orient.uvec * height - Rfg_camera.real_pos;
        const float dist = to_rack.mag_and_normalize_safe();
        if (Backpack_rack_popups.size() >= Backpack_rack_popups.capacity())
        {
            continue;
        }
        if (dist / Mp_server_options.get_level()->distance_multiplier > Mp_backpack_rack_popup_max_visible_distance)
        {
            continue;
        }
        if (to_rack.y * Rfg_camera.real_orient.fvec.y + to_rack.x * Rfg_camera.real_orient.fvec.x + to_rack.z * Rfg_camera.real_orient.fvec.z < Mp_backpack_rack_popup_min_dot)
        {
            continue;
        }
        raycast_input in;
        in.start = Rfg_camera.real_pos;
        in.end = to_rack * dist + Rfg_camera.real_pos;
        in.radius = 0.0f;
        in.filter_params = rack;
        in.filter_callback = backpack_rack_popup_filter;
        in.m_filter_info = COLLIDE_RAY_DEFAULT;
        in.m_hit_collector_type = raycast_input::COLLECT_CLOSEST_HIT;
        // Magic number: the raycast flags of the popups (as in rfg.exe).
        in.m_flags = 10;
        raycast_output out;
        if (havok_raycast(in, out))
        {
            continue;
        }
        int found = -1;
        for (int p = 0; p < Backpack_rack_popups.size(); p++)
        {
            if (Backpack_rack_popups[p].rack == rack)
            {
                found = p;
                break;
            }
        }
        if (found != -1)
        {
            // Magic number: a popup fades out 0.25 s after the rack is last looked at.
            Backpack_rack_popups[found].fade_out_timer.set(250);
            continue;
        }
        backpack_rack_popup popup;
        popup.rack = rack;
        // Magic numbers: 0.15 s fade in, 0.25 s fade out.
        popup.fade_in_timer.set(150);
        popup.fade_out_timer.set(250);
        Backpack_rack_popups.add(popup);
    }
}

// 0x00979150 (:380): the popups: the backpack's name and a box per backpack slot (filled while the backpack is on the
// rack), fading in and out, over the rack building's screen box
void multi_game_backpack_rack_render()
{
    if (!hud_ui_is_loaded() || Current_mode_camera != MCM_NONE || Mp_server_options.generic_options.backpack_spawns == MGBSO_NONE)
    {
        return;
    }
    const float res_scale = X_res_multiplier - Y_res_multiplier < 0.0f ? X_res_multiplier : Y_res_multiplier;
    const float checkbox_scale = res_scale * Mp_backpack_rack_popup_checkbox_scale;
    const float text_scale = res_scale * Mp_backpack_rack_popup_text_scale;
    const bm_image* image = vint_bms_get_image_data(Empty_checkbox_image);
    const float box_w = checkbox_scale * (float)image->bmw;
    const float box_h = (float)image->bmh * checkbox_scale;
    const int num_popups = Backpack_rack_popups.size();
    for (int i = 0; i < num_popups; i++)
    {
        backpack_rack_popup& popup = Backpack_rack_popups[i];
        multi_object_marker* rack = popup.rack;
        if (multi_game_settings_is_backpack_locked(rack->backpack_id))
        {
            continue;
        }
        // Original: a popup whose rack lost its building ends the drawing of every popup.
        rfg_mover* building = rack_building(rack);
        if (building == nullptr)
        {
            return;
        }
        color c = CHECKBOX_COLOR;
        if (popup.fade_in_timer.valid())
        {
            c.alpha = (unsigned char)(int)(((double)popup.fade_in_timer.time_since_set() / 150.0) * 255.0);
        }
        else
        {
            c.alpha = (unsigned char)(int)(((double)popup.fade_out_timer.time_until() / 250.0) * 255.0);
        }
        vector bmin;
        vector bmax;
        building->get_bbox(bmin, bmax);
        float x;
        float y;
        float w;
        float h;
        gr_get_bbox_2d_rect(bmin, bmax, x, y, w, h);
        if (rack->num_backpacks != 0 && !rack->destroyed)
        {
            int on_rack = 0;
            for (int b = 0; b < rack->num_backpacks; b++)
            {
                const multi_object_backpack* backpack = backpack_from_handle(rack->backpacks[b]);
                if (backpack != nullptr && backpack->get_parent() == rack->get_parent())
                {
                    on_rack++;
                }
            }
            const float half_h = box_h * 0.5f;
            y = y - half_h;
            float box_x = (w * 0.5f + x) - (float)(rack->num_backpacks - 1) * box_w * 0.5f;
            const float box_y = y;
            // Magic numbers: the shadow 2 pixels off, the fill 4 pixels in.
            const float shadow_y = y + 2.0f;
            for (int b = 0; b < rack->num_backpacks; b++)
            {
                gr_set_color(0, 0, 0, c.alpha);
                minimap_render_bitmap(Empty_checkbox_image, box_x + 2.0f, shadow_y, 0.0f, checkbox_scale, checkbox_scale, GM_FILTER);
                gr_set_color(c);
                minimap_render_bitmap(Empty_checkbox_image, box_x, box_y, 0.0f, checkbox_scale, checkbox_scale, GM_FILTER);
                if (on_rack > 0)
                {
                    color fill = c;
                    fill.alpha = (unsigned char)(int)((double)c.alpha * 0.75);
                    gr_set_color(fill);
                    gr_rect(box_x - (box_w - 8.0f) * 0.5f, box_y - (box_h - 8.0f) * 0.5f, box_w - 8.0f, box_h - 8.0f, GM_FILTER);
                    on_rack--;
                }
                box_x = box_x + box_w;
            }
            y = y - half_h;
        }
        const char* id_name = multi_game_backpack_get_display_name(rack->backpack_id);
        int text_w = 0;
        int text_h = 0;
        font_get_string_size(text_w, text_h, localization_get_display_string_req(id_name, nullptr), -1, Body_font, false);
        text_w = (int)((float)text_w * text_scale);
        text_h = (int)((float)text_h * text_scale);
        y = y - (float)text_h;
        gr_set_color(0, 0, 0, c.alpha);
        gr_string_scale((int)(((w * 0.5f + x) - (float)(text_w / 2)) + 2.0f), (int)(y + 2.0f), localization_get_display_string_req(id_name, nullptr), text_scale, Body_font, GM_FILTER);
        gr_set_color(c);
        gr_string_scale((int)((w * 0.5f + x) - (float)(text_w / 2)), (int)y, localization_get_display_string_req(id_name, nullptr), text_scale, Body_font, GM_FILTER);
    }
}

// 0x009DCA60 (:528): the rack is out (its building neutral): its backpacks go but those being worn (they leave the rack),
// its light goes; the server tells the clients
void multi_game_backpack_rack_destroyed(multi_object_marker* rack)
{
    rack->destroyed = true;
    // Original: the building is not checked for null.
    rfg_mover* building = rack_building(rack);
    building->team = HUMAN_TEAM_CIVILIAN;
    for (int i = 0; i < rack->num_backpacks; i++)
    {
        // Original: the backpack is not checked for null.
        multi_object_backpack* backpack = backpack_from_handle(rack->backpacks[i]);
        if (backpack->get_player_attached_to() == nullptr)
        {
            World.destroy_object(rack->backpacks[i], false);
            rack->backpacks[i] = 0xFFFFFFFF;
        }
        else
        {
            backpack->set_backpack_rack(nullptr, -1);
        }
    }
    World.destroy_object(rack->backpack_rack_light, false);
    rack->backpack_rack_light = 0xFFFFFFFF;
    if (Multiplayer && Server)
    {
        send_backpack_rack_destroyed(rack);
    }
}

// 0x009DCB50 (:566): the rack is back, owned by the repairing team, with new backpacks; the server tells the clients
void multi_game_backpack_rack_repaired(multi_object_marker* rack, human_teams repairing_team)
{
    // Original: the building is not checked for null.
    rfg_mover* building = rack_building(rack);
    rack->team = repairing_team;
    building->team = repairing_team;
    rack->destroyed = false;
    create_backpacks_on_rack(rack);
    if (Multiplayer && Server)
    {
        send_backpack_rack_repaired(rack, repairing_team);
    }
}

// 0x00979910 (:586): the backpack leaves its rack's building
void multi_game_backpack_rack_backpack_picked_up(multi_object_marker* rack, multi_object_backpack* backpack)
{
    if (!rack->can_attach_packs())
    {
        return;
    }
    rfg_mover* building = rack_building(rack);
    if (building != nullptr)
    {
        building->remove_child(backpack);
    }
}

// 0x009ABD00 (:601): a new backpack takes the old one's place on the rack (the old one leaves the rack)
void multi_game_backpack_rack_backpack_dropped(multi_object_marker* rack, multi_object_backpack* backpack)
{
    const int index = backpack->get_backpack_rack_index();
    const matrix orient = rack->orient;
    const vector pos = rack->pos;
    multi_object_backpack* replacement = multi_game_backpack_create(rack->backpack_id, pos, orient);
    if (replacement == nullptr)
    {
        return;
    }
    replacement->set_backpack_rack(rack, index);
    backpack->set_backpack_rack(nullptr, -1);
    if (rack->can_attach_packs())
    {
        rfg_mover* building = rack_building(rack);
        if (building != nullptr)
        {
            building->set_child(replacement, index, replacement->get_backpack_rack_attach_tag_index(), false, true);
        }
    }
    rack->backpacks[index] = replacement->get_handle();
}

// 0x009ABDE0 (:629)
void multi_game_backpack_rack_player_died(multi_object_marker* rack, player* pp)
{
    multi_game_backpack_rack_backpack_dropped(rack, pp->backpack);
}

// 0x009BC360 (:640): server: every rack's backpack type (rack handle, type; 0xFFFF ends), to one client (joining) or all
void send_backpack_rack_sync(multi_connection* cp)
{
    unsigned char packet[1200];
    packet[0] = PT_SYNC_BACKPACK_RACKS;
    unsigned short packet_size = packet_header_size(PT_SYNC_BACKPACK_RACKS);
    for (int i = World.get_num_objects<multi_object_marker>() - 1; i >= 0; i--)
    {
        const multi_object_marker* rack = Multi_markers_prioritized[i];
        if (rack->get_marker_type() != MMT_BACKPACK_RACK)
        {
            continue;
        }
        write_u16_be(packet, packet_size, multi_core_network_handle_from_game_handle(rack->get_handle(), false));
        packet[packet_size++] = (unsigned char)rack->backpack_id;
    }
    write_u16_be(packet, packet_size, 0xFFFF);
    packet_set_size(packet, packet_size);
    if (cp == nullptr)
    {
        multi_io_send_to_all_reliable(multi_session_get_by_type(ST_GAME), packet, packet_size);
        return;
    }
    multi_io_send_reliable(cp, packet, packet_size, nullptr, nullptr, nullptr);
}

// 0x009BC510 (:666): client: the racks' types, and their backpacks. Original: the first unknown rack ends the packet.
void multi_game_backpack_rack_process_sync(unsigned char* packet_data, const sender& from)
{
    int offset = 0;
    for (;;)
    {
        const unsigned short net_handle = read_u16_be(packet_data, offset);
        if (net_handle == 0xFFFF)
        {
            return;
        }
        const unsigned int game_handle = multi_core_game_handle_from_network_handle(net_handle, false);
        if (game_handle == 0xFFFFFFFF)
        {
            return;
        }
        // Original: the marker is not checked for null.
        multi_object_marker* rack = rack_from_network_handle(net_handle);
        rack->backpack_id = packet_data[offset++];
        create_backpacks_on_rack(rack);
    }
}

// 0x009DCBC0 (:707)
void multi_game_backpack_rack_process_destroyed(unsigned char* packet_data, const sender& from)
{
    int offset = 0;
    multi_object_marker* rack = rack_from_network_handle(read_u16_be(packet_data, offset));
    if (rack != nullptr)
    {
        multi_game_backpack_rack_destroyed(rack);
    }
}

// 0x009DCC80 (:735)
void multi_game_backpack_rack_process_repaired(unsigned char* packet_data, const sender& from)
{
    int offset = 0;
    multi_object_marker* rack = rack_from_network_handle(read_u16_be(packet_data, offset));
    if (rack != nullptr)
    {
        multi_game_backpack_rack_repaired(rack, (human_teams)packet_data[2]);
    }
}
