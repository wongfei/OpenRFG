// Original: volition/rfg/code/multi/multi_game/modes/multi_game_mode_capture_the_flag.cpp
//
// The Capture the Flag row of the mode table. Each team's flag stands on its base building (the team's capture zone
// marker's parent, at its "Attach_Flag" tag) as a dummy item; an enemy picks it up (an action, or touching it) and
// carries it as a weapon (the flag weapon replaces the sledgehammer; the carrier cannot swap weapons and, with the
// civilian carrier option, cannot attack). A carrier that dies, leaves or drops it leaves a dummy that falls to the
// ground and returns after the return time (or when a teammate of the flag touches it); a carrier reaching its own
// zone while its own flag is at its base captures a point. The server decides and tells everyone (PT_MODE_SPECIFIC:
// 0 dropped, 1 picked up, 2 captured, 3 returned, 4 a client's drop request; a client's pick-up request is a 1).
// Part of the multi unity TU (rfg_multi_unity.obj, every multi/ .cpp in path order): the file statics are shared
// by the whole tree, and a shared one is placed in its first user (tools/hdrgen/specs/mp0_stubs.py).

#include "volition/rfg/code/multi/multi_game/modes/multi_game_mode_capture_the_flag.h"
#include "volition/rfg/code/multi/multi_game/modes/multi_game_mode_common.h"
#include "volition/rfg/code/multi/multi_game/multi_game.h"
#include "volition/rfg/code/multi/multi_game/multi_game_announcer.h"
#include "volition/rfg/code/multi/multi_game/multi_game_data.h"
#include "volition/rfg/code/multi/multi_game/multi_game_object_backpack.h"
#include "volition/rfg/code/multi/multi_game/multi_game_object_marker.h"
#include "volition/rfg/code/multi/multi_game/multi_game_render.h"
#include "volition/rfg/code/multi/multi_game/multi_game_settings.h"
#include "volition/rfg/code/multi/multi_game/multi_game_stats.h"
#include "volition/rfg/code/multi/multi_game/multi_game_xp.h"
#include "volition/rfg/code/multi/multi_game/multi_gameplay_mode.h"
#include "volition/rfg/code/multi/multi_core/include/multi.h"
#include "volition/rfg/code/multi/multi_core/include/multi_core_io.h"
#include "volition/rfg/code/multi/multi_core/include/multi_core_objects.h"
#include "volition/rfg/code/multi/multi_core/include/multi_core_packet_types.h"
#include "volition/rfg/code/multi/multi_core/include/multi_session.h"
#include "volition/audio_lib/code/audiolib_api.h"
#include "volition/rfg/code/camera/rfg_camera.h"
#include "volition/rfg/code/geom/cloth_simulation.h"
#include "volition/rfg/code/gamewide/gameaudio.h"
#include "volition/rfg/code/gamewide/tweak_table.h"
#include "volition/rfg/code/havok/havok_raycast.h"
#include "volition/rfg/code/hud/hud_icon.h"
#include "volition/rfg/code/hud/minimap.h"
#include "volition/rfg/code/interface/localization/localization_shared.h"
#include "volition/rfg/code/interface/ingame/ovl_inventory.h"
#include "volition/rfg/code/interface/ui_document_hud_multiplayer.h"
#include "volition/rfg/code/level/world.h"
#include "volition/rfg/code/objects/humans/human.h"
#include "volition/rfg/code/objects/humans/human_teams.h"
#include "volition/rfg/code/objects/humans/human_weapons.h"
#include "volition/rfg/code/objects/humans/player.h"
#include "volition/rfg/code/objects/inventory_item.h"
#include "volition/rfg/code/objects/object_item.h"
#include "volition/rfg/code/objects/object_player_start.h"
#include "volition/rfg/code/rfg.h"
#include "volition/rfg/code/weapons/weapons.h"
#include "volition/interface_lib/code/vint_data_item.h"
#include "volition/interface_lib/code/vint_external.h"
#include "volition/interface_lib/code/vint_variant.h"
#include "volition/vlib/code/math/matrix.h"
#include "volition/vlib/code/math/vector3.h"
#include "volition/vlib/code/os/frametime.h"
#include "volition/vlib/code/util/bit_util.h"
#include "volition/vlib/code/util/string_util.h"
#include "keen/base/trace.hpp"

#include <math.h>
#include <string.h>
#include <wchar.h>

// 0x0094CBD0: the timers start invalid (their constructors)
ctf_flag::ctf_flag()
{
}

// ---- file-static globals
static int Flag_event_play_id_handle = -1; // 0x0167DE70
// dynamic_uint_hash_table<remote_object*> Game_handle_to_rop_hash (0x01691B40) is used here too; placed in multi_core_delta_compression.cpp.
// dynamic_uint_hash_table<remote_object*> Network_handle_to_rop_hash (0x01691B88) is used here too; placed in multi_core_delta_compression.cpp.
// dynamic_uint_hash_table<network_game_handle*> Reserved_game_to_network_hash (0x01691C18) is used here too; placed in multi_core_objects.cpp.
// dynamic_uint_hash_table<network_game_handle*> Reserved_network_to_game_hash (0x01691C60) is used here too; placed in multi_core_delta_compression.cpp.
// The announcer lines in multi_announcer_lines order (dynamic initializer at 0x0106FE50): line, team response, minimum
// time between plays (ms), priority, suppression group (-1: none). Moved here from rfg_multi_unity_pending_early.inl.
static multi_announcer_line_info Multi_announcer_lines[52] = { // 0x01691F00
    {"mpl_anc_gen_complete_round", "mpl_res_gen_complete_round", 0, DIALOGUE_PRIORITY_BRIEFING, -1},
    {"mpl_anc_gen_complete_match", "mpl_res_gen_complete_match", 0, DIALOGUE_PRIORITY_BRIEFING, -1},
    {"mpl_anc_gen_edf_win", "mpl_res_gen_edf_win", 0, DIALOGUE_PRIORITY_BRIEFING, -1},
    {"mpl_anc_gen_gur_win", "mpl_res_gen_gur_win", 0, DIALOGUE_PRIORITY_BRIEFING, -1},
    {"mpl_anc_gen_ally_win_match", nullptr, 0, DIALOGUE_PRIORITY_BRIEFING, -1},
    {"mpl_anc_gen_enemy_win_match", nullptr, 0, DIALOGUE_PRIORITY_BRIEFING, -1},
    {"mpl_anc_gen_tie_match", "mpl_res_gen_tie", 0, DIALOGUE_PRIORITY_BRIEFING, -1},
    {"mpl_anc_gen_ally_new_lead", "mpl_res_gen_ally_new_lead", 0, DIALOGUE_PRIORITY_NORMAL, 0},
    {"mpl_anc_gen_enemy_new_lead", "mpl_res_gen_enemy_new_lead", 0, DIALOGUE_PRIORITY_NORMAL, 0},
    {"mpl_anc_gen_new_lead", nullptr, 15000, DIALOGUE_PRIORITY_NORMAL, 0},
    {"mpl_anc_gen_time_5min_remains", "mpl_res_gen_time_5min_remains", 0, DIALOGUE_PRIORITY_HIGH, -1},
    {"mpl_anc_gen_time_1min_remains", "mpl_res_gen_time_1min_remains", 0, DIALOGUE_PRIORITY_HIGH, -1},
    {"mpl_anc_anarc_intro_mode", nullptr, 0, DIALOGUE_PRIORITY_BRIEFING, -1},
    {"mpl_anc_anarc_team_intro_mode", "mpl_res_anarc_team_mode_intro", 0, DIALOGUE_PRIORITY_BRIEFING, -1},
    {"mpl_anc_anarc_ally_win_near", "mpl_res_anarc_ally_win_near", 0, DIALOGUE_PRIORITY_HIGH, -1},
    {"mpl_anc_anarc_enemy_win_near", "mpl_res_anarc_enemy_win_near", 0, DIALOGUE_PRIORITY_HIGH, -1},
    {"mpl_anc_bagnm_intro_mode", nullptr, 0, DIALOGUE_PRIORITY_BRIEFING, -1},
    // Original: the team bagman intro answers with the bag respawned response.
    {"mpl_anc_bagnm_team_intro_mode", "mpl_res_bagmn_bag_respawned", 0, DIALOGUE_PRIORITY_BRIEFING, -1},
    {"mpl_anc_bagmn_bag_dropped", "mpl_res_bagmn_bag_dropped", 0, DIALOGUE_PRIORITY_NORMAL, -1},
    {"mpl_anc_bagmn_bag_respawned", "mpl_res_bagmn_bag_respawned", 0, DIALOGUE_PRIORITY_NORMAL, -1},
    {"mpl_anc_bagmn_ally_bag_pickup", "mpl_res_bagmn_ally_bag_pickup", 0, DIALOGUE_PRIORITY_NORMAL, -1},
    {"mpl_anc_bagmn_ally_near_win", "mpl_res_bagmn_ally_win_near", 0, DIALOGUE_PRIORITY_HIGH, -1},
    {"mpl_anc_bagnm_enemy_bag_pickup", "mpl_res_bagmn_enemy_bag_pickup", 0, DIALOGUE_PRIORITY_NORMAL, -1},
    {"mpl_anc_bagnm_enemy_near_win", "mpl_res_bagmn_enemy_win_near", 0, DIALOGUE_PRIORITY_HIGH, -1},
    {"mpl_anc_cflag_intro_mode", "mpl_res_cflag_intro_mode", 0, DIALOGUE_PRIORITY_BRIEFING, -1},
    {"mpl_anc_cflag_ally_flag_captured", "mpl_res_cflag_ally_flag_captured", 0, DIALOGUE_PRIORITY_HIGH, -1},
    {"mpl_anc_cflag_ally_flag_dropped", "mpl_res_cflag_ally_flag_dropped", 15000, DIALOGUE_PRIORITY_HIGH, 4},
    {"mpl_anc_cflag_ally_flag_returned", "mpl_res_cflag_ally_flag_returned", 0, DIALOGUE_PRIORITY_HIGH, -1},
    {"mpl_anc_cflag_ally_flag_taken", "mpl_res_cflag_ally_flag_taken", 15000, DIALOGUE_PRIORITY_HIGH, 4},
    {"mpl_anc_cflag_ally_win_near", "mpl_res_cflag_ally_win_near", 0, DIALOGUE_PRIORITY_HIGH, -1},
    {"mpl_anc_cflag_enemy_flag_captured", "mpl_res_cflag_enemy_flag_captured", 0, DIALOGUE_PRIORITY_HIGH, -1},
    {"mpl_anc_cflag_enemy_flag_dropped", "mpl_res_cflag_enemy_flag_dropped", 15000, DIALOGUE_PRIORITY_HIGH, 5},
    {"mpl_anc_cflag_enemy_flag_returned", "mpl_res_cflag_enemy_flag_returned", 0, DIALOGUE_PRIORITY_HIGH, -1},
    {"mpl_anc_cflag_enemy_flag_taken", "mpl_res_cflag_enemy_flag_taken", 15000, DIALOGUE_PRIORITY_HIGH, 5},
    {"mpl_anc_cflag_enemy_win_near", "mpl_res_cflag_enemy_win_near", 0, DIALOGUE_PRIORITY_HIGH, -1},
    {"mpl_anc_dcont_intro_mode", "mpl_res_dcont_intro_mode", 0, DIALOGUE_PRIORITY_BRIEFING, -1},
    {"mpl_anc_dcont_ally_target_lost", "mpl_res_dcont_ally_target_lost", 0, DIALOGUE_PRIORITY_NORMAL, 1},
    {"mpl_anc_dcont_ally_target_repaired", "mpl_res_dcont_ally_target_repaired", 0, DIALOGUE_PRIORITY_NORMAL, 1},
    {"mpl_anc_dcont_ally_win_near", "mpl_res_dcont_ally_win_near", 0, DIALOGUE_PRIORITY_HIGH, -1},
    {"mpl_anc_dcont_enemy_target_lost", "mpl_res_dcont_enemy_target_lost", 0, DIALOGUE_PRIORITY_NORMAL, 1},
    {"mpl_anc_dcont_enemy_target_repaired", "mpl_res_dcont_enemy_target_repaired", 0, DIALOGUE_PRIORITY_NORMAL, 1},
    {"mpl_anc_dcont_enemy_win_near", "mpl_res_dcont_enemy_win_near", 0, DIALOGUE_PRIORITY_HIGH, -1},
    {"mpl_anc_demo_intro_mode", nullptr, 0, DIALOGUE_PRIORITY_BRIEFING, -1},
    {"mpl_anc_demo_ally_new_eng", "mpl_res_demo_ally_new_eng", 0, DIALOGUE_PRIORITY_NORMAL, 2},
    {"mpl_anc_demo_enemy_new_eng", "mpl_res_demo_enemy_new_eng", 0, DIALOGUE_PRIORITY_NORMAL, 3},
    {"mpl_anc_siege_intro_mode", "mpl_res_siege_intro_mode", 0, DIALOGUE_PRIORITY_BRIEFING, -1},
    {"mpl_anc_siege_inform_offense", "mpl_res_siege_inform_offense", 0, DIALOGUE_PRIORITY_BRIEFING, -1},
    {"mpl_anc_siege_inform_defense", "mpl_res_siege_inform_defense", 0, DIALOGUE_PRIORITY_BRIEFING, -1},
    {"mpl_anc_siege_ally_target_destroyed", "mpl_res_siege_ally_target_destroy", 0, DIALOGUE_PRIORITY_NORMAL, -1},
    {"mpl_anc_siege_ally_near_win", "mpl_res_siege_ally_win_near", 0, DIALOGUE_PRIORITY_HIGH, -1},
    {"mpl_anc_siege_enemy_target_destroyed", "mpl_res_siege_enemy_target_destroy", 0, DIALOGUE_PRIORITY_NORMAL, -1},
    {"mpl_anc_siege_enemy_near_win", "mpl_res_siege_enemy_win_near", 0, DIALOGUE_PRIORITY_HIGH, -1},
};
// multi_team_stats Mp_team_stats[2] (0x02FE82F0) is used here too; placed in multi_game_mode_bagman.cpp.
// bool Server (0x02FEB590) is used here too; placed in multi_core_session_generic.cpp.
static bool Mp_ctf_always_show_base_icons; // 0x02FEB623
// mp_game_data_wrapper* Mp_game_data (0x02FEB698) is used here too; placed in multi_connection.cpp.
// multi_player_stats* Mp_player_stats (0x02FEB720) is used here too; placed in multi_game_mode_bagman.cpp.
static ctf_flag CTF_flags[2]; // 0x02FEB7D8
// Defined in multi_game_render.cpp (not ported: rfg_multi_unity_pending.inl) and multi_game_mode_common.cpp, later in
// the unity TU; external linkage in the port for that reason.
extern float Mp_enemy_player_name_min_dot; // 0x0167F298
extern float TS_anarchy_draw_chance; // 0x02FE6D4C
static tweak_table_item Tweak_table_item_Mp_enemy_player_name_min_dot("Mp_enemy_player_name_min_dot", &Mp_enemy_player_name_min_dot); // 0x02FEB878 (0x010715E0)
static tweak_table_item Tweak_table_item_TS_anarchy_draw_chance("TS_anarchy_draw_chance", &TS_anarchy_draw_chance); // 0x02FEB884
// multi_xp_event Multi_xp_events[84] (0x0300D240) is used here too; placed in multi_game_mode_bagman.cpp.
// The flag weapons the carriers hold and the dummy items at the bases and on the ground, per team.
static checksum_stri CTF_flag_weapon_name_crcs[2] = {checksum_stri("flag_guerrilla", 0), checksum_stri("flag_edf", 0)}; // 0x030173B8 (0x0106FA50)
static checksum_stri CTF_flag_dummy_name_crcs[2] = {checksum_stri("flag_guerrilla_dummy", 0), checksum_stri("flag_edf_dummy", 0)}; // 0x030176C4 (0x0106FA80)

// The PT_MODE_SPECIFIC subtypes (the byte after the header).
static const unsigned char Ctf_dropped = 0;
static const unsigned char Ctf_picked_up = 1;
static const unsigned char Ctf_captured = 2;
static const unsigned char Ctf_returned = 3;
static const unsigned char Ctf_drop_request = 4;
// The pick-up reach (2.5 m) and a teammate's touch return (2.5 m), squared; beyond 1 m the pick-up needs a clear line.
static const float Ctf_pickup_dist_squared = 6.25f;
static const float Ctf_touch_return_dist_squared = 6.25f;
static const float Ctf_pickup_los_dist_squared = 1.0f;

// ---- internal functions
// Port helper: the player of a handle (null unless a player), as rfg.exe inlines it.
static player* ctf_player_from_handle(unsigned int handle)
{
    if (handle == 0xFFFFFFFFu)
    {
        return nullptr;
    }
    object* objp = World.object_from_handle(handle);
    if (objp == nullptr || objp->get_obj_type() != OT_HUMAN || objp->get_sub_type() != OT_SUB_HUMAN_PLAYER)
    {
        return nullptr;
    }
    return (player*)objp;
}

// Port helper: the player stats of a game data handle (index 16 passes, as in rfg.exe).
static multi_player_stats* ctf_stats_of(unsigned int handle)
{
    if (handle == 0xFFFFFFFFu || (handle & 0xFFFF) > 16)
    {
        return nullptr;
    }
    return &Mp_player_stats[handle & 0xFFFF];
}

// Port helper: the flag a name crc is (the weapon or the dummy), or HUMAN_TEAM_NONE; item_is_flag's loop, inlined
// elsewhere.
static human_teams flag_team_from_crc(const checksum_stri& crc)
{
    for (int team = HUMAN_TEAM_GUERILLA; team <= HUMAN_TEAM_EDF; team++)
    {
        if (crc == CTF_flag_weapon_name_crcs[team] || crc == CTF_flag_dummy_name_crcs[team])
        {
            return (human_teams)team;
        }
    }
    return HUMAN_TEAM_NONE;
}

// 0x0094CBE0 (:157): the flag the player carries. LTCG: pp in EAX.
static ctf_flag* player_has_ctf_flag(const player* pp)
{
    for (int i = 0; i < 2; i++)
    {
        if (CTF_flags[i].carrier_handle == pp->get_handle())
        {
            return &CTF_flags[i];
        }
    }
    return nullptr;
}

// 0x0094CC10 (:168): LTCG: infop in EAX.
static human_teams item_is_flag(const item_info* infop)
{
    return flag_team_from_crc(checksum_stri(infop->name, 0));
}

// 0x0094CC50 (:181): a carrier loses its flag weapon (the server removes it from the inventory) and takes the
// sledgehammer. LTCG: pp in ESI.
static void destroy_flag_weapon(player* pp)
{
    const ctf_flag* flag = player_has_ctf_flag(pp); // inlined
    if (flag == nullptr)
    {
        return;
    }
    if (Multiplayer && Server)
    {
        inventory_item* iip = inv_item_get_item_from_weapon_info(pp, weapons_get_weapon_info_handle(CTF_flag_weapon_name_crcs[flag->team]));
        if (iip != nullptr)
        {
            human_delete_item_from_inventory(pp, iip, false, false);
        }
    }
    player_equip_sledgehammer(pp);
}

// 0x00974EE0 (:198): the flag's icon sits on top of the flag and shows the seconds until a dropped flag returns
static bool flag_hud_icon_callback_ctf(vector& world_pos, hud_icon* icon)
{
    object* objp = World.object_from_handle(icon->obj);
    if (objp == nullptr)
    {
        return false;
    }
    vector bmin;
    vector bmax;
    objp->get_local_bbox(bmin, bmax);
    // The bounding box height along UP_VECTOR, in double as rfg.exe computes it.
    const float dx = bmax.x - bmin.x;
    const float dy = bmax.y - bmin.y;
    const float dz = bmax.z - bmin.z;
    const float height = (float)((double)UP_VECTOR.y * dy + (double)UP_VECTOR.x * dx + (double)UP_VECTOR.z * dz);
    world_pos.x += UP_VECTOR.x * height;
    world_pos.y += UP_VECTOR.y * height;
    world_pos.z += UP_VECTOR.z * height;
    const ctf_flag* flag = (const ctf_flag*)icon->parameter;
    if (flag->state == CTFFS_DROPPED)
    {
        // Rounded to the nearest second, not below 0.
        int seconds = (flag->dropped_time.time_until() + 500) / 1000;
        if (seconds < 0)
        {
            seconds = 0;
        }
        _snprintf_safe(icon->text[0], 32, "%d", seconds);
        return icon->visible;
    }
    icon->text[0][0] = L'\0';
    return icon->visible;
}

// 0x00991DA0 (:227): the flag item's icon in its team's color. LTCG: flag in EDI.
static void add_flag_icon(ctf_flag* flag)
{
    if (flag == nullptr)
    {
        return;
    }
    hud_icon* icon = hud_icon_add(flag->flag_handle);
    if (icon == nullptr)
    {
        return;
    }
    icon->bitmap_id = hud_icon_get_bitmap_id(HI_FLAG);
    // Magic numbers: the icon's scale and its countdown's text scale.
    icon->icon_scale = 1.5f;
    multi_game_render_get_team_color(icon->icon_color, MTCT_ICON, flag->team);
    icon->text_color = icon->icon_color;
    icon->text_font = Number_font;
    icon->text_scale = 1.4f;
    icon->snap_to_edge = true;
    icon->callback = flag_hud_icon_callback_ctf;
    icon->parameter = flag;
}

// 0x0094CCD0 (:246): LTCG: flag in EAX.
static void remove_flag_icon(ctf_flag* flag)
{
    if (flag != nullptr)
    {
        hud_icon_remove(flag->flag_handle);
    }
}

// 0x00991E30 (:254): the base's icon in the flag team's color. LTCG: flag in EDI.
static void add_flag_capture_zone_icon(const ctf_flag* flag)
{
    if (flag == nullptr)
    {
        return;
    }
    object* base = World.object_from_handle(flag->flag_base_handle);
    if (base == nullptr)
    {
        return;
    }
    color team_color = color::WHITE;
    multi_game_render_get_team_color(team_color, MTCT_ICON, flag->team);
    hud_icon_add(base->get_handle(), minimap_get_element_bitmap_id(MAP_ELEMENT_BASE_ICON), 1.5f, team_color, false, true);
}

// 0x0094CCE0 (:269): LTCG: flag in EAX.
static void remove_flag_capture_zone_icon(const ctf_flag* flag)
{
    if (flag == nullptr)
    {
        return;
    }
    object* base = World.object_from_handle(flag->flag_base_handle);
    if (base != nullptr)
    {
        hud_icon_remove(base->get_handle());
    }
}

// 0x00991EB0 (:281): the team's dummy flag item: at its base on the base building's "Attach_Flag" tag, else at pos and
// orient (zero and identity when null); with its icon. LTCG: flag_info in ECX, orient in EAX.
static item* create_ctf_flag(ctf_flag* flag_info, const vector* pos, const matrix* orient)
{
    item_info* infop = object_item_get_info(CTF_flag_dummy_name_crcs[flag_info->team]);
    if (infop == nullptr)
    {
        return nullptr;
    }
    vector flag_pos = ZERO_VECTOR;
    matrix flag_orient(IDENTITY_MATRIX);
    if (flag_info->state == CTFFS_AT_BASE)
    {
        object* base = flag_info->flag_base_handle != 0xFFFFFFFFu ? World.object_from_handle(flag_info->flag_base_handle) : nullptr;
        if (base != nullptr && base->get_obj_type() == OT_MOVER && base->get_sub_type() == OT_SUB_MOVER_RFG)
        {
            base->get_tag_world_coords(base->get_tag_index_from_name("Attach_Flag"), flag_pos, &flag_orient, nullptr, nullptr);
        }
    }
    else
    {
        if (pos != nullptr)
        {
            flag_pos = *pos;
        }
        if (orient != nullptr)
        {
            flag_orient = *orient;
        }
    }
    obj_item_flags flags;
    memset(&flags, 0, sizeof(flags));
    obj_item_props props(infop, flag_pos, flag_orient, flags, COLLIDE_ITEM, -1.0f, INVALID_SALVAGE_MATERIAL);
    item* itemp = World.create_object<item>(props, 0xFFFFFFFF);
    if (itemp == nullptr)
    {
        return nullptr;
    }
    flag_info->flag_handle = itemp->get_handle();
    add_flag_icon(flag_info);
    return itemp;
}

// 0x0094CD10 (:320): the dummy item and its icon go. LTCG: flag_info in ESI.
static void destroy_ctf_flag(ctf_flag* flag_info)
{
    object* objp = World.object_from_handle(flag_info->flag_handle);
    if (objp != nullptr)
    {
        World.destroy_object(objp, false);
        remove_flag_icon(flag_info); // inlined
    }
}

// 0x00992050 (:329): the flag back at its base (the base icon only with the always-on option). LTCG: flag in ESI.
static void reset_ctf_flag(ctf_flag* flag)
{
    flag->carrier_handle = 0xFFFFFFFFu;
    flag->dropped_time.invalidate();
    flag->state = CTFFS_AT_BASE;
    destroy_ctf_flag(flag); // inlined
    create_ctf_flag(flag, nullptr, nullptr);
    if (!Mp_ctf_always_show_base_icons)
    {
        remove_flag_capture_zone_icon(flag); // inlined
    }
}

// 0x009B9990 (:343): the server's pick-up (to everyone) or a client's request (to the server): the carrier, the flag's
// team, the inventory slot to drop for it. LTCG: carrier_handle in EDI.
static void send_flag_picked_up_event(unsigned int carrier_handle, human_teams team, int drop_slot)
{
    unsigned char packet[1200];
    packet[0] = PT_MODE_SPECIFIC;
    unsigned short packet_size = packet_header_size(PT_MODE_SPECIFIC);
    packet[packet_size++] = Ctf_picked_up;
    write_u16_be(packet, packet_size, multi_core_network_handle_from_game_handle(carrier_handle, false)); // inlined
    packet[packet_size++] = (unsigned char)team;
    packet[packet_size++] = (unsigned char)drop_slot;
    packet_set_size(packet, packet_size);
    if (Multiplayer && Server)
    {
        multi_io_send_to_all_reliable(Game_session, packet, packet_size);
        return;
    }
    multi_io_send_reliable(Game_session->get_host_connection(), packet, packet_size, nullptr, nullptr, nullptr);
}

// Port helper: the HUD objective of a flag event (the flag team's name, the event label, the team's color).
static void ctf_trigger_objective(human_teams title_team, const char* label_id, human_teams team)
{
    wchar_t title[64];
    wchar_t label[64];
    wcsncpy(title, human_team_get_display_name(title_team), 64);
    title[63] = L'\0';
    wcsncpy(label, localization_get_display_string_req(label_id, nullptr), 64);
    label[63] = L'\0';
    hud_ui_multiplayer_trigger_objective(title, label, team);
}

// Port helper: the flag event cue (the playing one stops).
static void ctf_play_flag_event(int cue_id)
{
    if (audiolib_is_playing(Flag_event_play_id_handle, true))
    {
        audiolib_play_stop(Flag_event_play_id_handle);
    }
    Flag_event_play_id_handle = audiolib_cue_play(cue_id, nullptr, nullptr);
}

// Port helper: the flag event packets of the server: the player (its network handle) and the flag's team.
static void ctf_send_flag_event(unsigned char subtype, unsigned int player_handle, bool silent, human_teams team)
{
    unsigned char packet[1200];
    packet[0] = PT_MODE_SPECIFIC;
    unsigned short packet_size = packet_header_size(PT_MODE_SPECIFIC);
    packet[packet_size++] = subtype;
    write_u16_be(packet, packet_size, multi_core_network_handle_from_game_handle(player_handle, silent));
    packet[packet_size++] = (unsigned char)team;
    packet_set_size(packet, packet_size);
    multi_io_send_to_all_reliable(multi_session_get_by_type(ST_GAME), packet, packet_size);
}

// 0x009E6C60 (:372): the carrier takes the flag: the server gives it the flag weapon (dropping the slot's weapon when
// the inventory is full); the dummy item goes, the base icon shows, the pick-up experience, the announcer, the HUD and
// the cue; the server tells everyone. LTCG: carrier in ESI, drop_slot in ECX.
static void capture_the_flag_event_flag_picked_up(player* carrier, ctf_flag* flag, int drop_slot)
{
    if (carrier == nullptr || flag == nullptr)
    {
        return;
    }
    if (Multiplayer && Server)
    {
        if (!player_has_room_in_inventory(carrier))
        {
            if (drop_slot == -1)
            {
                return;
            }
            inventory_item* iip = inv_item_get_item_in_slot(carrier, drop_slot);
            if (iip != nullptr)
            {
                human_weapons_drop_weapon(carrier, iip);
            }
        }
        const weapon_info* w_info = weapons_get_weapon_info(CTF_flag_weapon_name_crcs[flag->team]);
        if (w_info == nullptr || human_add_item_to_inventory(carrier, w_info->weapon_inv_item_info, 0, -1, -1, true, false, true) == nullptr)
        {
            return;
        }
    }
    local_player_info info;
    player_get_local_player_info(&info);
    flag->carrier_handle = carrier->get_handle();
    flag->state = CTFFS_PICKED_UP;
    flag->dropped_time.invalidate();
    destroy_ctf_flag(flag); // inlined
    if (!Mp_ctf_always_show_base_icons)
    {
        add_flag_capture_zone_icon(flag);
    }
    multi_game_xp_trigger_event(carrier->mp_data, MXE_PICK_UP_FLAG, 1, 0xFF, -1, false); // inlined
    // A carrier not friendly to us took our flag; MAL_CTF_ENEMY_* follow MAL_CTF_ALLY_* by 5.
    const bool ours = human_teams_get_disposition(carrier, info.team) < TEAM_DISPOSITION_FRIENDLY;
    multi_game_announcer_play_line(ours ? MAL_CTF_ALLY_FLAG_TAKEN : MAL_CTF_ENEMY_FLAG_TAKEN, 0xFFFFFFFF);
    multi_player_stats* stats = ctf_stats_of(carrier->mp_data);
    if (stats != nullptr)
    {
        stats->ctf_pickups++;
    }
    ctf_trigger_objective(flag->team, "HM_FLAG_PICKED_UP", carrier->current_team);
    ctf_play_flag_event(Ga_cue_ids.multiplayer_flag_taken);
    if (Multiplayer && Server)
    {
        send_flag_picked_up_event(carrier->get_handle(), flag->team, -1);
    }
}

// 0x009B9B10 (:450): the carrier lost the flag: no flag weapon, the dummy where it stood with the return countdown,
// the announcer, the HUD and the cue; the server tells everyone. LTCG: dropper in ECX, flag in EDI.
static void capture_the_flag_event_flag_dropped(player* dropper, ctf_flag* flag)
{
    if (dropper == nullptr || flag == nullptr)
    {
        return;
    }
    destroy_flag_weapon(dropper);
    flag->carrier_handle = 0xFFFFFFFFu;
    flag->state = CTFFS_DROPPED;
    flag->dropped_time.set((int)Mp_server_options.mode_options.capture_the_flag.flag_return_time_seconds * 1000);
    create_ctf_flag(flag, &dropper->pos, nullptr);
    local_player_info info;
    player_get_local_player_info(&info);
    const bool ours = human_teams_get_disposition(dropper->current_team, info.team) < TEAM_DISPOSITION_FRIENDLY;
    multi_game_announcer_play_line(ours ? MAL_CTF_ALLY_FLAG_DROP : MAL_CTF_ENEMY_FLAG_DROP, 0xFFFFFFFF);
    ctf_trigger_objective(flag->team, "HM_FLAG_DROPPED", flag->team);
    ctf_play_flag_event(Ga_cue_ids.multiplayer_flag_dropped);
    if (Multiplayer && Server)
    {
        ctf_send_flag_event(Ctf_dropped, dropper->get_handle(), false, flag->team);
    }
}

// 0x009E7060 (:504): the flag goes back to its base (returner: the teammate that touched it, or null after the return
// time): the announcer, the return experience, the HUD and the cue; the server tells everyone. LTCG: flag in ECX.
static void capture_the_flag_event_flag_returned(const player* returner, ctf_flag* flag)
{
    if (flag == nullptr)
    {
        return;
    }
    reset_ctf_flag(flag);
    local_player_info info;
    player_get_local_player_info(&info);
    const bool enemy_flag = human_teams_get_disposition(flag->team, info.team) < TEAM_DISPOSITION_FRIENDLY;
    multi_game_announcer_play_line(enemy_flag ? MAL_CTF_ENEMY_FLAG_RETURN : MAL_CTF_ALLY_FLAG_RETURN, 0xFFFFFFFF);
    if (returner != nullptr)
    {
        multi_game_xp_trigger_event(returner->mp_data, MXE_RETURN_FLAG, 1, 0xFF, -1, false); // inlined
    }
    ctf_trigger_objective(flag->team, "HM_FLAG_RETURNED", flag->team);
    ctf_play_flag_event(Ga_cue_ids.multiplayer_flag_returned);
    if (Multiplayer && Server)
    {
        ctf_send_flag_event(Ctf_returned, returner == nullptr ? 0xFFFFFFFFu : returner->get_handle(), true, flag->team);
    }
}

// 0x009E7400 (:556): a carrier reached its zone: the flag weapon goes and the flag returns, a point for the capturer's
// team, the announcer, the capture experience (player and team), the HUD and the cue; the server tells everyone.
// LTCG: flag in ECX.
static void capture_the_flag_event_flag_captured(player* capturer, ctf_flag* flag)
{
    if (capturer == nullptr || flag == nullptr)
    {
        return;
    }
    destroy_flag_weapon(capturer);
    reset_ctf_flag(flag);
    multi_game_stats_alter_team_score(capturer->current_team, 1);
    local_player_info info;
    player_get_local_player_info(&info);
    const bool ours = human_teams_get_disposition(capturer->current_team, info.team) < TEAM_DISPOSITION_FRIENDLY;
    multi_game_announcer_play_line(ours ? MAL_CTF_ALLY_FLAG_CAPTURE : MAL_CTF_ENEMY_FLAG_CAPTURE, 0xFFFFFFFF);
    multi_game_xp_trigger_event(capturer->mp_data, MXE_CAPTURE_FLAG, 1, 0xFF, -1, false); // inlined
    multi_game_xp_trigger_event(capturer->current_team, MXE_CAPTURE_FLAG_TEAM, 1, false); // inlined
    multi_player_stats* stats = ctf_stats_of(capturer->mp_data);
    if (stats != nullptr)
    {
        stats->ctf_captures++;
    }
    ctf_trigger_objective(flag->team, "HM_FLAG_CAPTURED", capturer->current_team);
    ctf_play_flag_event(Ga_cue_ids.multiplayer_flag_captured);
    if (Multiplayer && Server)
    {
        ctf_send_flag_event(Ctf_captured, capturer->get_handle(), false, flag->team);
    }
}

// 0x009E78E0 (:617): a living player with room (or an equipped item to swap) takes the flag (not again within 0.5 s);
// the local player drops its swap weapon's slot (slot 0 without one); a client asks the server. LTCG: pp in EAX.
static void player_wants_to_pick_up_ctf_flag(player* pp, human_teams flag_team, int drop_slot)
{
    // Magic number: 500 ms between pick-up attempts.
    CTF_flags[flag_team].pickup_timer.set(500);
    if (CTF_flags[flag_team].carrier_handle != 0xFFFFFFFFu || pp->is_killed())
    {
        return;
    }
    if (!player_has_room_in_inventory(pp) && pp->get_equipped_inv_item() == nullptr)
    {
        return;
    }
    if (object_is_local_player(pp))
    {
        const inventory_item* swap = player_get_swap_weapon(pp);
        drop_slot = swap != nullptr ? swap->selection_slot : -1;
        if (drop_slot == -1)
        {
            drop_slot = 0;
        }
    }
    if (Multiplayer && Server)
    {
        capture_the_flag_event_flag_picked_up(pp, &CTF_flags[flag_team], drop_slot);
        return;
    }
    send_flag_picked_up_event(pp->get_handle(), flag_team, drop_slot);
}

// 0x009E7990 (:654): the pick-up action: the action object is a flag item within about 3.2 m (10, squared)
static bool ctf_flag_action_object_callback(player* pp, bool do_swap)
{
    if (pp->action_object.handle == 0xFFFFFFFFu)
    {
        return false;
    }
    object* objp = World.object_from_handle(pp->action_object.handle);
    if (objp == nullptr || objp->get_obj_type() != OT_ITEM)
    {
        return false;
    }
    const human_teams flag_team = item_is_flag(((item*)objp)->info); // inlined
    if (flag_team == HUMAN_TEAM_NONE)
    {
        return false;
    }
    if (vec_dist_squared(pp->pos, objp->pos) > 10.0f)
    {
        return false;
    }
    if (do_swap)
    {
        player_wants_to_pick_up_ctf_flag(pp, flag_team, -1);
    }
    return true;
}

// 0x009B9D10 (:678): the carrier drops the flag it holds (its equipped weapon is a flag); a client asks the server
static void player_wants_to_drop_ctf_flag(player* pp)
{
    ctf_flag* flag = player_has_ctf_flag(pp); // inlined
    if (flag == nullptr)
    {
        return;
    }
    const inventory_item* iip = pp->get_equipped_inv_item();
    if (iip == nullptr || iip->weapon_handle == 0xFFFFFFFFu)
    {
        return;
    }
    object* objp = World.object_from_handle(iip->weapon_handle);
    if (objp == nullptr || objp->get_obj_type() != OT_ITEM || item_is_flag(((item*)objp)->info) == HUMAN_TEAM_NONE)
    {
        return;
    }
    if (Multiplayer && Server)
    {
        capture_the_flag_event_flag_dropped(pp, flag);
        return;
    }
    unsigned char packet[1200];
    packet[0] = PT_MODE_SPECIFIC;
    unsigned short packet_size = packet_header_size(PT_MODE_SPECIFIC);
    packet[packet_size++] = Ctf_drop_request;
    write_u16_be(packet, packet_size, multi_core_network_handle_from_game_handle(pp->get_handle(), false)); // inlined
    packet[packet_size++] = (unsigned char)flag->team;
    packet_set_size(packet, packet_size);
    multi_io_send_reliable(Game_session->get_host_connection(), packet, packet_size, nullptr, nullptr, nullptr);
}

// 0x009B9F50 (:725): the drop action: the action object is an item
static bool ctf_flag_drop_action_object_callback(player* pp, bool do_swap)
{
    if (pp->action_object.handle == 0xFFFFFFFFu)
    {
        return false;
    }
    object* objp = World.object_from_handle(pp->action_object.handle);
    if (objp == nullptr || objp->get_obj_type() != OT_ITEM)
    {
        return false;
    }
    if (do_swap)
    {
        player_wants_to_drop_ctf_flag(pp);
    }
    return true;
}

// 0x009920C0 (:749): each team's flag stands on the building of its team's capture zone marker
void capture_the_flag_mode_level_init_post()
{
    for (int team = HUMAN_TEAM_GUERILLA; team <= HUMAN_TEAM_EDF; team++)
    {
        ctf_flag& flag = CTF_flags[team];
        flag.team = (human_teams)team;
        flag.flag_handle = 0xFFFFFFFFu;
        flag.flag_base_handle = 0xFFFFFFFFu;
        flag.carrier_handle = 0xFFFFFFFFu;
        flag.state = CTFFS_NONE;
        flag.dropped_time.invalidate();
        flag.touch_timer.invalidate();
        flag.pickup_timer.invalidate();
    }
    for (int i = World.get_num_objects<multi_object_marker>() - 1; i >= 0; i--)
    {
        multi_object_marker* zone = Multi_markers_prioritized[i];
        if (zone->get_marker_type() != MMT_FLAG_CAPTURE_ZONE || CTF_flags[zone->team].flag_handle != 0xFFFFFFFFu)
        {
            continue;
        }
        ctf_flag& flag = CTF_flags[zone->team];
        flag.flag_base_handle = zone->get_parent();
        flag.state = CTFFS_AT_BASE;
        if (Mp_ctf_always_show_base_icons)
        {
            add_flag_capture_zone_icon(&flag);
        }
        create_ctf_flag(&flag, nullptr, nullptr);
    }
}

// 0x009921E0 (:787): the flag stats start over
void capture_the_flag_mode_enter()
{
    for (unsigned int handle = multi_game_data_handles_begin(ST_GAME, true, false); handle != 0xFFFFFFFFu;
         handle = multi_game_data_handles_next(handle, true, false))
    {
        multi_player_stats* stats = ctf_stats_of(handle);
        if (stats != nullptr)
        {
            stats->ctf_captures = 0;
            stats->ctf_carrier_kills = 0;
            stats->ctf_defenses = 0;
            stats->ctf_pickups = 0;
        }
    }
}

// 0x009E7A50 (:803): per flag: the local carrier keeps the flag weapon in hand; a dropped flag returns when a teammate
// of the flag touches it (option), stands upright and falls to the ground (5 m/s), and the server returns it when its
// time is up. "Near the point limit" (one point to go) announcements per team.
void capture_the_flag_mode_process()
{
    local_player_info info;
    player_get_local_player_info(&info);
    for (int team = HUMAN_TEAM_GUERILLA; team <= HUMAN_TEAM_EDF; team++)
    {
        ctf_flag& flag = CTF_flags[team];
        player* carrier = ctf_player_from_handle(flag.carrier_handle);
        if (flag.state == CTFFS_PICKED_UP)
        {
            if (object_is_local_player(carrier))
            {
                inventory_item* iip = inv_item_get_item_from_weapon_info(carrier, weapons_get_weapon_info_handle(CTF_flag_weapon_name_crcs[team]));
                if (iip != nullptr && carrier->get_equipped_inv_item() != iip && carrier->get_desired_inv_item() != iip)
                {
                    player_equip_item(carrier, iip, nullptr, false);
                    ovl_inventory_update_avail_weapons();
                }
            }
        }
        else
        {
            object* dummy = flag.flag_handle != 0xFFFFFFFFu ? World.object_from_handle(flag.flag_handle) : nullptr;
            if (dummy != nullptr && dummy->get_obj_type() != OT_ITEM)
            {
                dummy = nullptr;
            }
            if (flag.state == CTFFS_DROPPED && dummy != nullptr)
            {
                if (Multiplayer && Server && Mp_server_options.mode_options.capture_the_flag.flag_touch_return)
                {
                    // Original: every touching teammate returns the flag (the loop goes on after the first return).
                    const int count = World.get_num_objects<player>();
                    for (int i = 0; i < count; i++)
                    {
                        player* pp = World.get_object<player>(i);
                        if (!pp->is_killed() && human_teams_get_disposition(flag.team, pp->current_team) == TEAM_DISPOSITION_ALLIED &&
                            vec_dist_squared(dummy->pos, pp->pos) <= Ctf_touch_return_dist_squared)
                        {
                            capture_the_flag_event_flag_returned(pp, &flag);
                        }
                    }
                }
                // Original: after a touch return the destroyed dummy is still used below.
                // Magic number: the orientation counts as upright within 0.01.
                const bool tilted = !mat_equal_robust(dummy->orient, IDENTITY_MATRIX, 0.01f);
                dummy->update_pos_and_orient(dummy->pos, IDENTITY_MATRIX, false);
                item* dummy_item = (item*)dummy;
                if (tilted && dummy_item->cloth_sim != nullptr)
                {
                    dummy_item->cloth_sim->initialize_nodes = 1;
                }
                // Magic numbers: the ray starts 10 cm above the dummy and reaches 100 m down.
                raycast_input in;
                in.start = dummy->pos + UP_VECTOR * 0.1f;
                in.end = dummy->pos - UP_VECTOR * 100.0f;
                in.radius = 0.0f;
                in.filter_params = nullptr;
                in.filter_callback = nullptr;
                in.m_filter_info = COLLIDE_RAY_DEFAULT;
                in.m_hit_collector_type = raycast_input::COLLECT_CLOSEST_HIT;
                in.m_flags = 0;
                raycast_output out;
                if (havok_raycast(in, out))
                {
                    const float fall = Frametime * 5.0f;
                    vector new_pos;
                    if (fabsf(dummy->pos.y - out.m_hits[0].hit_pos.y) >= fall)
                    {
                        new_pos = dummy->pos - UP_VECTOR * fall;
                    }
                    else
                    {
                        new_pos = out.m_hits[0].hit_pos;
                    }
                    dummy->update_pos_and_orient(new_pos, dummy->orient, true);
                }
            }
            if (Multiplayer && Server && flag.state == CTFFS_DROPPED && flag.dropped_time.elapsed())
            {
                capture_the_flag_event_flag_returned(nullptr, &flag);
            }
        }
        const unsigned short point_limit = Mp_server_options.generic_options.point_limit;
        if (point_limit != 0 && point_limit <= Mp_team_stats[team].round_score + 1 && point_limit > 1)
        {
            // The enemy line is 5 lines before the ally line here; lines past MAL_SGE_ENEMY_TARGET_WIN_NEAR have no
            // play record.
            const bool enemy = human_teams_get_disposition(info.team, (human_teams)team) < TEAM_DISPOSITION_FRIENDLY;
            const multi_announcer_lines line = enemy ? MAL_CTF_ENEMY_WIN_NEAR : MAL_CTF_ALLY_WIN_NEAR;
            if (line > MAL_SGE_ENEMY_TARGET_WIN_NEAR || !Multi_announcer_lines[line].last_played_time.valid())
            {
                multi_game_announcer_play_line(line, 0xFFFFFFFF);
            }
        }
    }
}

// 0x009B9F90 (:907): a leaving carrier drops its flag (server)
void capture_the_flag_mode_remove_player(player* pp)
{
    ctf_flag* flag = player_has_ctf_flag(pp); // inlined
    if (flag != nullptr && Multiplayer && Server)
    {
        capture_the_flag_event_flag_dropped(pp, flag);
    }
}

// 0x009E8040 (:920): a carrier died: its flag drops (server), its enemy killer gets the kill carrier experience and a
// carrier kill. An enemy kill near a carrier the victim was chasing (the protect carrier distance), or near a flag on
// the ground the victim was after (the protect flag distance): the protect experience and a defense. A kill with a
// flag: the flagman experience.
void capture_the_flag_mode_player_died(player* victim, player* killer, damaged_by_types damage_type, const weapon_info* w_info)
{
    const int weapon_id = w_info != nullptr ? w_info->unique_id : 0xFF;
    const int backpack_id = killer->backpack != nullptr ? killer->backpack->get_backpack_info()->unique_id : -1;
    multi_player_stats* killer_stats = ctf_stats_of(killer->mp_data);
    ctf_flag* carried = player_has_ctf_flag(victim); // inlined
    if (carried != nullptr)
    {
        if (Multiplayer && Server)
        {
            capture_the_flag_event_flag_dropped(victim, carried);
        }
        if (human_teams_get_disposition(victim, killer) == TEAM_DISPOSITION_HOSTILE)
        {
            multi_game_xp_trigger_event(killer->mp_data, MXE_KILL_FLAG_CARRIER, 1, weapon_id, backpack_id, false); // inlined
            if (killer_stats != nullptr)
            {
                killer_stats->ctf_carrier_kills++;
            }
        }
    }
    if (human_teams_get_disposition(victim, killer) != TEAM_DISPOSITION_HOSTILE)
    {
        return;
    }
    const float carrier_dist = multi_game_xp_get_event_parameter(MXE_PROTECT_FLAG_CARRIER); // inlined
    const float carrier_dist_squared = carrier_dist * carrier_dist;
    const float flag_dist = multi_game_xp_get_event_parameter(MXE_PROTECT_FLAG); // inlined
    const float flag_dist_squared = flag_dist * flag_dist;
    for (int i = 0; i < 2; i++)
    {
        const ctf_flag& flag = CTF_flags[i];
        bool defended = false;
        const player* carrier = ctf_player_from_handle(flag.carrier_handle);
        if (carrier != nullptr && human_teams_get_disposition(victim->current_team, flag.team) != TEAM_DISPOSITION_HOSTILE)
        {
            if (vec_dist_squared(killer->pos, carrier->pos) <= carrier_dist_squared || vec_dist_squared(victim->pos, carrier->pos) <= carrier_dist_squared)
            {
                multi_game_xp_trigger_event(killer->mp_data, MXE_PROTECT_FLAG_CARRIER, 1, weapon_id, backpack_id, false); // inlined
                defended = true;
            }
        }
        const object* dummy = World.object_from_handle(flag.flag_handle);
        if (dummy != nullptr && carrier == nullptr && human_teams_get_disposition(victim->current_team, flag.team) == TEAM_DISPOSITION_HOSTILE)
        {
            if (vec_dist_squared(killer->pos, dummy->pos) <= flag_dist_squared || vec_dist_squared(victim->pos, dummy->pos) <= flag_dist_squared)
            {
                multi_game_xp_trigger_event(killer->mp_data, MXE_PROTECT_FLAG, 1, weapon_id, backpack_id, false); // inlined
                defended = true;
            }
        }
        if (killer_stats != nullptr && defended)
        {
            killer_stats->ctf_defenses++;
        }
    }
    if (w_info != nullptr && w_info->weapon_obj_item_info != nullptr && item_is_flag(w_info->weapon_obj_item_info) != HUMAN_TEAM_NONE)
    {
        multi_game_xp_trigger_event(killer->mp_data, MXE_FLAGMAN, 1, weapon_id, backpack_id, false); // inlined
    }
}

// 0x00975080 (:999): a carrier with a full inventory only picks up weapons it has
bool capture_the_flag_mode_item_touched(const player* pp, const item* itemp)
{
    if (itemp == nullptr || itemp->get_obj_type() != OT_ITEM || itemp->get_sub_type() != OT_SUB_ITEM_WEAPON)
    {
        return true;
    }
    for (int i = 0; i < 2; i++)
    {
        if (CTF_flags[i].carrier_handle == pp->get_handle() && !player_has_room_in_inventory(pp) &&
            human_has_item(pp, ((const weapon*)itemp)->w_info->weapon_inv_item_info) == nullptr)
        {
            return false;
        }
    }
    return true;
}

// 0x0094CD40 (:1017): a carrier keeps the flag in hand
bool capture_the_flag_mode_player_can_swap_weapons(const player* pp)
{
    return player_has_ctf_flag(pp) == nullptr; // inlined
}

// 0x009E8AA0 (:1029): an enemy flag within 2.5 m (beyond 1 m in sight of the player) can be picked up (not within 0.5 s
// of an attempt); the local carrier can drop its flag
bool capture_the_flag_mode_player_set_action_object(const player* pp, t_usable_object& action_object)
{
    for (int i = 0; i < 2; i++)
    {
        const ctf_flag& flag = CTF_flags[i];
        object* dummy = World.object_from_handle(flag.flag_handle);
        if (dummy != nullptr)
        {
            const float dist_squared = vec_dist_squared(dummy->pos, pp->pos);
            if (human_teams_get_disposition(pp->current_team, flag.team) == TEAM_DISPOSITION_HOSTILE && dist_squared <= Ctf_pickup_dist_squared)
            {
                bool visible = true;
                if (dist_squared > Ctf_pickup_los_dist_squared)
                {
                    vector bmin;
                    vector bmax;
                    dummy->get_bbox(bmin, bmax);
                    // Magic numbers: from 10 cm above the player's feet to 5 cm above the flag's middle.
                    const float height = (bmax.y - bmin.y) * 0.5f + 0.05f;
                    raycast_input in;
                    in.start = pp->pos + UP_VECTOR * 0.1f;
                    in.end = dummy->pos + UP_VECTOR * height;
                    in.radius = 0.0f;
                    in.filter_params = nullptr;
                    in.filter_callback = nullptr;
                    in.m_filter_info = COLLIDE_RAY_DEFAULT;
                    in.m_hit_collector_type = raycast_input::COLLECT_CLOSEST_HIT;
                    in.m_flags = 0;
                    raycast_output out;
                    visible = !havok_raycast(in, out) || out.m_hits[0].hit_object_handle == dummy->get_handle() ||
                              out.m_hits[0].hit_object_handle == flag.flag_base_handle;
                }
                if (visible && (!flag.pickup_timer.valid() || flag.pickup_timer.elapsed()))
                {
                    action_object.dot_prod = 1.0f;
                    action_object.action_type = GENERAL_ACTION_MP_MODE_SPECIFIC;
                    action_object.action_icon = USE_OBJECT_ICON_DEFAULT;
                    action_object.action_callback = ctf_flag_action_object_callback;
                    action_object.dist_squared = 0.0f;
                    action_object.handle = flag.flag_handle;
                    return true;
                }
            }
        }
        if (object_is_local_player(pp) && flag.carrier_handle == pp->get_handle() && pp->get_equipped_inv_item() != nullptr)
        {
            // Magic numbers: the drop action ranks low (dot 0.01, 0.5 m squared) so other actions win.
            action_object.dot_prod = 0.01f;
            action_object.action_type = GENERAL_ACTION_MP_MODE_SPECIFIC;
            action_object.action_icon = USE_OBJECT_ICON_INVALID;
            action_object.action_callback = ctf_flag_drop_action_object_callback;
            action_object.dist_squared = 0.5f;
            action_object.handle = pp->get_equipped_inv_item()->weapon_handle;
            return true;
        }
    }
    return false;
}

// 0x0094CD70 (:1074): with the civilian carrier option a carrier cannot attack
bool capture_the_flag_mode_player_can_attack(const player* pp, const weapon_info* w_info, damaged_by_types attack_type)
{
    if (!Mp_server_options.mode_options.capture_the_flag.civilian_flag_carrier)
    {
        return true;
    }
    return player_has_ctf_flag(pp) == nullptr; // inlined
}

// 0x009E8E60 (:1085): PT_MODE_SPECIFIC (packet_data: the subtype byte, the player's network handle, the flag's team, the
// drop slot of a pick-up)
void capture_the_flag_mode_process_mode_specific(unsigned char* packet_data, const sender& from)
{
    const unsigned char subtype = packet_data[0];
    int offset = 1;
    const unsigned short net_handle = read_u16_be(packet_data, offset);
    player* pp = net_handle == 0xFFFF ? nullptr : ctf_player_from_handle(multi_core_game_handle_from_network_handle(net_handle, true)); // inlined
    const unsigned char team = packet_data[3];
    if (team >= 2)
    {
        return;
    }
    ctf_flag* flag = &CTF_flags[team];
    const int drop_slot = packet_data[4];
    switch (subtype)
    {
    case Ctf_dropped:
        capture_the_flag_event_flag_dropped(pp, flag);
        return;
    case Ctf_picked_up:
        if (Multiplayer && !Server)
        {
            capture_the_flag_event_flag_picked_up(pp, flag, drop_slot);
            return;
        }
        player_wants_to_pick_up_ctf_flag(pp, flag->team, drop_slot);
        return;
    case Ctf_captured:
        capture_the_flag_event_flag_captured(pp, flag);
        return;
    case Ctf_returned:
        capture_the_flag_event_flag_returned(pp, flag);
        return;
    case Ctf_drop_request:
        player_wants_to_drop_ctf_flag(pp);
        return;
    default:
        keen::debug::nativePrintArgs("There was an invalid event in the CTF mode specific packet handler!");
        return;
    }
}

// 0x009750F0 (:1139): the team with the better round score; a tie has no winner (-1)
multi_round_winner capture_the_flag_mode_get_round_winner()
{
    multi_round_winner winner;
    if (Mp_team_stats[HUMAN_TEAM_EDF].round_score > Mp_team_stats[HUMAN_TEAM_GUERILLA].round_score)
    {
        winner.team = HUMAN_TEAM_EDF;
    }
    else if (Mp_team_stats[HUMAN_TEAM_EDF].round_score == Mp_team_stats[HUMAN_TEAM_GUERILLA].round_score)
    {
        winner.player_game_data_handle = 0xFFFFFFFFu;
    }
    else
    {
        winner.team = HUMAN_TEAM_GUERILLA;
    }
    return winner;
}

// 0x0094CDB0 (:1146): our team, then per team: round score, flag state, carrier name, carried by us, distance, angle
void capture_the_flag_mode_hud_init(vint_data_item* item)
{
    static const vint_variant_type ctf_template[13] = {
        VINT_PROP_TYPE_INT,
        VINT_PROP_TYPE_INT, VINT_PROP_TYPE_INT, VINT_PROP_TYPE_STRING, VINT_PROP_TYPE_BOOL, VINT_PROP_TYPE_FLOAT, VINT_PROP_TYPE_FLOAT,
        VINT_PROP_TYPE_INT, VINT_PROP_TYPE_INT, VINT_PROP_TYPE_STRING, VINT_PROP_TYPE_BOOL, VINT_PROP_TYPE_FLOAT, VINT_PROP_TYPE_FLOAT};
    item->set_template(ctf_template, 13);
}

// 0x00975130 (:1166): the HUD item's elements (see capture_the_flag_mode_hud_init); a flag with neither a dummy nor
// a carrier keeps its last values
void capture_the_flag_mode_hud_process(vint_data_item* item)
{
    local_player_info info;
    player_get_local_player_info(&info);
    item->set_element(0, vint_variant((int)info.team));
    int index = 1;
    char encoded[192];
    for (int team = HUMAN_TEAM_GUERILLA; team <= HUMAN_TEAM_EDF; team++, index += 6)
    {
        const ctf_flag& flag = CTF_flags[team];
        item->set_element(index, vint_variant((int)Mp_team_stats[team].round_score));
        player* carrier = ctf_player_from_handle(flag.carrier_handle);
        object* target = World.object_from_handle(flag.flag_handle);
        if (target == nullptr)
        {
            target = carrier;
            if (target == nullptr)
            {
                continue;
            }
        }
        item->set_element(index + 1, vint_variant((int)flag.state));
        const multi_game_data* data = carrier == nullptr ? nullptr : multi_game_data_get(carrier->mp_data, false, false);
        // Original: without a carrier the name is encoded from null.
        vint_encode_wide_text(encoded, sizeof(encoded), data == nullptr ? nullptr : data->display_name);
        item->set_element(index + 2, vint_variant((const char*)encoded));
        item->set_element(index + 3, vint_variant(object_is_local_player(carrier)));
        item->set_element(index + 4, vint_variant(vec_dist_approx(*info.pos, target->pos)));
        const vector to_target = target->pos - *info.pos;
        item->set_element(index + 5, vint_variant(-vec_delta_ang_flattened(to_target, Rfg_camera.real_orient.fvec)));
    }
}

// 0x0094CE30 (:1224)
void capture_the_flag_mode_get_objective_description(char* buffer)
{
    memcpy(buffer, "HM_CAPTURE_THE_FLAG_OBJECTIVE", sizeof("HM_CAPTURE_THE_FLAG_OBJECTIVE"));
}

// 0x0094CE70 (:1229): a carrier's icon is the held flag; shown always only with the option
bool capture_the_flag_mode_alter_player_icon(const player* pp, hud_icon* icon)
{
    if (player_has_ctf_flag(pp) == nullptr) // inlined
    {
        return false;
    }
    icon->bitmap_id = hud_icon_get_bitmap_id(HI_FLAG_HELD);
    return Mp_server_options.mode_options.capture_the_flag.flag_carrier_icon_always_on;
}

// 0x00975440 (:1244): respawning near an enemy carrier costs up to 30 (15 m, scaled by the level's distance multiplier),
// near a teammate carrying a flag gains up to 5 (30 m)
float capture_the_flag_mode_adjust_respawn_weight(const player* pp, const player_start* pstart)
{
    float weight = 0.0f;
    for (int i = 0; i < 2; i++)
    {
        const player* carrier = ctf_player_from_handle(CTF_flags[i].carrier_handle);
        if (carrier == nullptr)
        {
            continue;
        }
        float dist = vec_dist(pstart->pos, carrier->pos) / Mp_server_options.get_level()->distance_multiplier;
        if (human_teams_get_disposition(carrier->current_team, pp->current_team) == TEAM_DISPOSITION_HOSTILE)
        {
            dist /= 15.0f;
            if (dist - 1.0f >= 0.0f)
            {
                dist = 1.0f;
            }
            weight += dist * 30.0f - 30.0f;
        }
        else if (human_teams_get_disposition(carrier->current_team, pp->current_team) == TEAM_DISPOSITION_ALLIED)
        {
            dist /= 30.0f;
            if (dist - 1.0f >= 0.0f)
            {
                dist = 1.0f;
            }
            weight += dist * -5.0f + 5.0f;
        }
    }
    return weight;
}

// 0x009922E0 (:1272): the new server puts both flags back on their bases
void capture_the_flag_mode_server_migration_post()
{
    for (int i = 0; i < 2; i++)
    {
        reset_ctf_flag(&CTF_flags[i]); // inlined
    }
}

// 0x009E8FE0 (:1285): a carrier in its own team's zone captures (server) while its own flag is at its base; otherwise
// the local carrier is told to wait for its flag. Original: pp is not checked for null.
void capture_the_flag_mode_player_in_flag_capture_zone(player* pp, multi_object_marker* zone)
{
    ctf_flag* carried = player_has_ctf_flag(pp); // inlined
    if (carried == nullptr || zone->team != pp->current_team)
    {
        return;
    }
    if (CTF_flags[pp->current_team].state == CTFFS_AT_BASE)
    {
        if (Multiplayer && Server)
        {
            capture_the_flag_event_flag_captured(pp, carried);
        }
        return;
    }
    if (pp == player_get_local_player())
    {
        multi_game_trigger_tooltip(pp, checksum_stri("waiting_for_flag", 0));
    }
}

// 0x009755F0 (:1307): server: captures, carrier kills, defenses, pick-ups (BE)
void capture_the_flag_mode_append_mode_specific_stats(unsigned char* packet_data, unsigned short& packet_size, unsigned int player_game_data_handle)
{
    const multi_player_stats* stats = ctf_stats_of(player_game_data_handle);
    if (stats == nullptr || !Multiplayer || !Server)
    {
        return;
    }
    write_u32_be(packet_data, packet_size, (unsigned int)stats->ctf_captures);
    write_u32_be(packet_data, packet_size, (unsigned int)stats->ctf_carrier_kills);
    write_u32_be(packet_data, packet_size, (unsigned int)stats->ctf_defenses);
    write_u32_be(packet_data, packet_size, (unsigned int)stats->ctf_pickups);
}

// 0x009757B0 (:1321): client
void capture_the_flag_mode_read_mode_specific_stats(unsigned char* packet_data, int& packet_offset, unsigned int player_game_data_handle)
{
    multi_player_stats* stats = ctf_stats_of(player_game_data_handle);
    if (stats == nullptr || !Multiplayer || Server)
    {
        return;
    }
    stats->ctf_captures = (int)read_u32_be(packet_data, packet_offset);
    stats->ctf_carrier_kills = (int)read_u32_be(packet_data, packet_offset);
    stats->ctf_defenses = (int)read_u32_be(packet_data, packet_offset);
    stats->ctf_pickups = (int)read_u32_be(packet_data, packet_offset);
}

// 0x0094CEC0 (:1335): captures, carrier kills, defenses, pick-ups
int capture_the_flag_mode_get_mode_specific_stat_count()
{
    return 4;
}

// 0x0094CED0 (:1340): the results screen's column names (string ids)
void capture_the_flag_mode_get_mode_specific_stat_name(char* name, const int max_name_len, int stat_index)
{
    const char* id;
    switch (stat_index)
    {
    case 0:
        id = "MENU_FLAG_CAPTURES";
        break;
    case 1:
        id = "MENU_CARRIER_KILLS";
        break;
    case 2:
        id = "MENU_FLAG_DEFENDS";
        break;
    case 3:
        id = "MENU_FLAG_PICKUPS";
        break;
    default:
        id = "";
        break;
    }
    _snprintf_safe(name, max_name_len, "%s", id);
}

// 0x009759A0 (:1361): the column values. Original: no null check of the stats.
void capture_the_flag_mode_get_mode_specific_stat_value(char* value, const int max_value_len, int stat_index, unsigned int player_game_data_handle)
{
    const multi_player_stats* stats = ctf_stats_of(player_game_data_handle);
    switch (stat_index)
    {
    case 0:
        _snprintf_safe(value, max_value_len, "%d", stats->ctf_captures);
        return;
    case 1:
        _snprintf_safe(value, max_value_len, "%d", stats->ctf_carrier_kills);
        return;
    case 2:
        _snprintf_safe(value, max_value_len, "%d", stats->ctf_defenses);
        return;
    case 3:
        _snprintf_safe(value, max_value_len, "%d", stats->ctf_pickups);
        return;
    default:
        _snprintf_safe(value, max_value_len, "%s", "");
        return;
    }
}
