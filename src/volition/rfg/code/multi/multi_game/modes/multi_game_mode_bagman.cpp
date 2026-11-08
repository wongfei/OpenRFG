// Original: volition/rfg/code/multi/multi_game/modes/multi_game_mode_bagman.cpp
//
// The Bagman row of the mode table (and most of Team Bagman's): one bag (an item, multi_object_bagman_flag) that a
// player picks up by touching it; the carrier (or its team) scores a point every second it holds the bag. A dropped
// bag falls to the ground and returns to its base after the options' return time; teammates can take the bag over
// (the swap action). The server decides and tells the clients (PT_MODE_SPECIFIC: 0 picked up, 1 dropped, 2 returned,
// 3 swapped; each with the scores).
// Part of the multi unity TU (rfg_multi_unity.obj, every multi/ .cpp in path order): the file statics are shared
// by the whole tree, and a shared one is placed in its first user (tools/hdrgen/specs/mp0_stubs.py).

#include "volition/rfg/code/multi/multi_game/modes/multi_game_mode_bagman.h"
#include "volition/rfg/code/multi/multi_game/modes/multi_game_mode_common.h"
#include "volition/rfg/code/multi/multi_game/multi_game_announcer.h"
#include "volition/rfg/code/multi/multi_game/multi_game_data.h"
#include "volition/rfg/code/multi/multi_game/multi_game_render.h"
#include "volition/rfg/code/multi/multi_game/multi_game_settings.h"
#include "volition/rfg/code/multi/multi_game/multi_game_stats.h"
#include "volition/rfg/code/multi/multi_game/multi_game_xp.h"
#include "volition/rfg/code/multi/multi_game/multi_gameplay_mode.h"
#include "volition/rfg/code/multi/multi_game/multi_game_object_backpack.h"
#include "volition/rfg/code/multi/multi_core/include/multi.h"
#include "volition/rfg/code/multi/multi_core/include/multi_core_io.h"
#include "volition/rfg/code/multi/multi_core/include/multi_core_objects.h"
#include "volition/rfg/code/multi/multi_core/include/multi_core_packet_types.h"
#include "volition/rfg/code/multi/multi_core/include/multi_session.h"
#include "volition/rfg/code/camera/rfg_camera.h"
#include "volition/rfg/code/effects/effects.h"
#include "volition/rfg/code/effects/effects_shared.h"
#include "volition/rfg/code/effects/object_effect.h"
#include "volition/rfg/code/gamewide/downloadable_content.h"
#include "volition/rfg/code/gamewide/game_render.h"
#include "volition/rfg/code/gamewide/gameaudio.h"
#include "volition/rfg/code/havok/havok_raycast.h"
#include "volition/rfg/code/hud/hud_icon.h"
#include "volition/rfg/code/interface/localization/localization_shared.h"
#include "volition/rfg/code/interface/ui_document_hud.h"
#include "volition/rfg/code/interface/ui_document_hud_multiplayer.h"
#include "volition/rfg/code/level/world.h"
#include "volition/rfg/code/objects/humans/human.h"
#include "volition/rfg/code/objects/humans/human_teams.h"
#include "volition/rfg/code/objects/humans/player.h"
#include "volition/rfg/code/objects/object_item.h"
#include "volition/rfg/code/rfg.h"
#include "volition/render_lib/code/rl_outline_compositor_defines.h"
#include "volition/render_lib/code/rl_scene.h"
#include "volition/audio_lib/code/audiolib_api.h"
#include "volition/interface_lib/code/vint_data_item.h"
#include "volition/interface_lib/code/vint_external.h"
#include "volition/interface_lib/code/vint_variant.h"
#include "volition/vlib/code/math/vector3.h"
#include "volition/vlib/code/os/frametime.h"
#include "volition/vlib/code/util/bit_util.h"
#include "volition/vlib/code/util/color.h"
#include "volition/vlib/code/util/string_util.h"
#include "volition/vlib/code/util/unicode_tools.h"
#include "volition/rfg/code/weapons/weapons.h"
#include "volition/rfg/code/objects/object_player_start.h"

#include <math.h>
#include <string.h>

// 0x0094BE50: the timers start invalid (their constructors); the handles are not initialized
bagman_flag::bagman_flag()
{
}

// ---- file-static globals
// The swap action's icon, per team.
static use_object_icon Team_swap_icons[2] = {(use_object_icon)8, (use_object_icon)9}; // 0x0167DE64
static int Bag_picked_up_play_id = -1; // 0x0167DE6C
// multi_gameplay_mode Multi_gameplay_mode (0x0167DEE0) is used here too; placed in multi_core_platform.cpp.
// Moved here from rfg_multi_unity_pending_early.inl (this file is its first user in the unity TU).
static human_voice_persona_handle Multi_general_announcer = (human_voice_persona_handle)-1; // 0x0167DEE4
// unsigned int Local_game_data_handles[4] (0x0167E5F4) is used here too; placed in multi_join.cpp.
// dynamic_uint_hash_table<remote_object*> Game_handle_to_rop_hash (0x01691B40) is used here too; placed in multi_core_delta_compression.cpp.
// dynamic_uint_hash_table<network_game_handle*> Reserved_game_to_network_hash (0x01691C18) is used here too; placed in multi_core_objects.cpp.
static bagman_flag Bagman_flag; // 0x01691D80
// The team colors: [team][0 ally, 1 enemy]; the third row is the free-for-all pair.
static color Mp_team_colors[3][2] = { // 0x016932A8
    {color(0x78, 0xAA, 0xBE, 0xFF), color(0xC9, 0x2D, 0x2D, 0xFF)},
    {color(0x00, 0x05, 0x0E, 0xFF), color(0x0E, 0x01, 0x01, 0xFF)},
    {color(0x00, 0x50, 0xFF, 0xFF), color(0xFF, 0x14, 0x14, 0xFF)},
};
// Moved here from rfg_multi_unity_pending_early.inl (this file is its first user in the unity TU).
static multi_team_stats Mp_team_stats[2]; // 0x02FE82F0
// point_of_interest Camera_points_of_interest[2] (0x02FE8300) is used here too; placed in multi_game_camera.cpp (not
// used by this file in the port).
// bool Server (0x02FEB590) is used here too; placed in multi_core_session_generic.cpp.
static const item_info* Bagman_flag_item_info; // 0x02FEB630
static const effect_info* Bagman_flag_effect_info; // 0x02FEB634
// Declared in downloadable_content.h, which writes it. Placed by address (inferred: rfg.pdb records no file for it): it
// sits between this file's statics and multi_game_mode_common.cpp's in the multi unity TU's data.
bool Bagman_dlc_unlocked; // 0x02FEB638
// mp_game_data_wrapper* Mp_game_data (0x02FEB698) is used here too; placed in multi_connection.cpp.
// Moved here from rfg_multi_unity_pending_early.inl (this file is its first user in the unity TU).
static multi_player_stats* Mp_player_stats; // 0x02FEB720
// The experience events in multi_xp_events order, by their mp_xp.xtbl names (dynamic initializer at 0x010723A0;
// multi_game_xp_init fills them from the table). Moved here from rfg_multi_unity_pending_early.inl.
static multi_xp_event Multi_xp_events[84] = { // 0x0300D240
    "Kill", "Finish match", "Win match", "Deathmatch kill", "Deathmatch kill solo", "Pick up bag", "Hold bag",
    "Hold bag solo", "Hold bag team", "Kill bagman", "Protect bagman", "Bagman kill", "Capture flag",
    "Capture flag team", "Kill flag carrier", "Protect flag carrier", "Protect flag", "Pick up flag", "Return flag",
    "Destroy King of the Hill target", "Destroy assist King of the Hill target", "Repair King of the Hill target",
    "Repair assist King of the Hill target", "Hold King of the Hill target", "Protect King of the Hill target",
    "Destroy Siege target", "Destroy assist Siege target", "Destroy Siege target team", "Protect Siege target",
    "Defend Siege target", "Demolition", "Demolition team", "Kill engineer", "Protect engineer", "Multiple kills",
    "Melee kill", "Barrel kill", "Debris kill", "Heal teammate", "Kill assist", "Kill streak", "Cliff kill",
    "Bunny kill", "Kung fu kill", "Headshot kill", "Streak stopper", "Ghost kill", "Sticky kill", "Direct hit kill",
    "Retaliation kill", "X-ray kill", "Sticky save", "Easy kill", "Revenge kill", "Charge kill", "Survivor",
    "Group knockdown", "Good timing", "Zombie kill", "Back off kill", "Flying kill", "Rescue kill",
    "Synchronized kill", "Pack hunter", "BBQ", "Lined up", "Full control", "Whos number one", "Backwhat", "Flagman",
    "Doubleback", "Camper", "Blindside", "Hail Mary", "Vertigo", "Sharpshooter", "No problem", "Shaken not stirred",
    "Taste the rainbow", "Doing it wrong", "Backfire", "Death from above", "Death from below", "Power overwhelming"};

// The PT_MODE_SPECIFIC subtypes (the byte after the header).
static const unsigned char Bagman_picked_up = 0;
static const unsigned char Bagman_dropped = 1;
static const unsigned char Bagman_returned = 2;
static const unsigned char Bagman_swapped = 3;
// The carrier scores a point a second; the swap reaches 10 m (squared), the kill bonus is 5 points.
static const int Bagman_score_interval_ms = 1000;
static const float Bagman_swap_dist_squared = 100.0f;
static const int Bagman_kill_bonus_points = 5;
// The "near the point limit" announcements start 50 points before a limit over 50.
static const int Bagman_win_near_points = 50;

// The PT_MODE_SPECIFIC packets of this file and the scores they carry are written with the port helpers of
// multi_join.cpp (write_u32_be, write_u16_be, packet_header_size, packet_set_size).

// ---- internal functions
// Port helper: multi_gameplay_mode_is_team_mode of the current mode, as rfg.exe inlines it here (multi_game_mode_common.cpp
// defines the table later in the unity TU).
static bool bagman_is_team_mode()
{
    return Multiplayer && Multi_gameplay_mode >= 0 && multi_gameplay_mode_is_team_mode(Multi_gameplay_mode);
}

// Port helper: the player stats of a game data handle, as rfg.exe inlines it. Original: index 16 is accepted
// although Mp_player_stats has 16 entries.
static multi_player_stats* bagman_stats_of(unsigned int handle)
{
    if (handle == 0xFFFFFFFFu || (handle & 0xFFFF) > 16)
    {
        return nullptr;
    }
    return &Mp_player_stats[handle & 0xFFFF];
}

// Port helper: the player of a handle (null unless a player), as rfg.exe inlines it.
static player* bagman_player_from_handle(unsigned int handle)
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

// Port helper: the HUD title of a player (inlined: "you" for the local player, its name otherwise).
static const wchar_t* bagman_player_title(const player* pp)
{
    if (object_is_local_player(pp))
    {
        return localization_get_display_string_req("HM_KILL_YOU", nullptr);
    }
    if (!pp->pflags.mp_removing_player)
    {
        const multi_game_data* data = multi_game_data_get(pp->mp_data, false, false);
        if (data != nullptr)
        {
            return data->display_name;
        }
    }
    return L"";
}

// 0x0094BE60 (:97): the name of the game data (the default without one), encoded for the HUD. LTCG: mgd in EAX,
// encoded_name in ECX. No caller in rfg.exe outside this file's inlined copies.
static void get_encoded_display_name_from_from_multi_game_data_bagman(const multi_game_data* mgd, int size, char* encoded_name, const wchar_t* default_name)
{
    vint_encode_wide_text(encoded_name, size, mgd == nullptr ? default_name : mgd->display_name);
}

// 0x00991090 (:109): the carrier's score (1, its team's or its own) or all scores (0). LTCG: packet_data in ECX,
// packet_size in EAX, pp in ESI.
static void write_bagman_scores(unsigned char* packet_data, unsigned short& packet_size, const player* pp)
{
    if (pp != nullptr)
    {
        packet_data[packet_size++] = 1;
        if (bagman_is_team_mode())
        {
            multi_game_stats_write_team_score(pp->current_team, packet_data, packet_size);
            return;
        }
        multi_game_stats_write_player_score(pp->mp_data, packet_data, packet_size);
        return;
    }
    packet_data[packet_size++] = 0;
    if (bagman_is_team_mode())
    {
        multi_game_stats_write_all_team_scores(packet_data, packet_size);
        return;
    }
    multi_game_stats_write_all_player_scores(packet_data, packet_size);
}

// 0x00991130 (:130): LTCG: packet_data in ESI, packet_offset in ECX.
static void read_bagman_scores(const unsigned char* packet_data, int& packet_offset)
{
    const unsigned char one = packet_data[packet_offset++];
    if (one != 0)
    {
        if (bagman_is_team_mode())
        {
            multi_game_stats_read_team_score(packet_data, packet_offset);
            return;
        }
        multi_game_stats_read_player_score(packet_data, packet_offset);
        return;
    }
    if (bagman_is_team_mode())
    {
        multi_game_stats_read_all_team_scores(packet_data, packet_offset);
        return;
    }
    multi_game_stats_read_all_player_scores(packet_data, packet_offset);
}

// 0x0094BE90 (:150): LTCG: pp in EDX.
static bool player_has_bagman_flag(const player* pp)
{
    return Bagman_flag.carrier_handle == pp->get_handle();
}

// 0x0094BEA0 (:155): LTCG: itemp in EAX.
static bool item_is_bagman_flag(const item* itemp)
{
    return itemp->info == Bagman_flag_item_info;
}

// 0x00974600 (:164): the dropped bag's icon sits on top of the bag and shows the seconds until it returns (rounded)
static bool flag_hud_icon_callback(vector& world_pos, hud_icon* icon)
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
    const bagman_flag* flag = (const bagman_flag*)icon->parameter;
    if (flag->return_timer.valid())
    {
        // Rounded to the nearest second, not below 0.
        int seconds = (flag->return_timer.time_until() + 500) / 1000;
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

// 0x009747B0 (:192): LTCG: flag in EDI. Inlined into create_bagman_flag in rfg.exe; no other caller.
static void add_flag_icon(const bagman_flag* flag)
{
    hud_icon* icon = hud_icon_add(flag->bag_handle);
    if (icon != nullptr)
    {
        icon->bitmap_id = hud_icon_get_bitmap_id(HI_BAGMAN_FLAG_DROPPED);
        icon->callback = flag_hud_icon_callback;
        icon->parameter = flag;
        icon->text_font = Number_font;
        // Magic number: the countdown's text scale.
        icon->text_scale = 1.4f;
    }
}

// 0x00974800 (:204): the bag item with its marker effect and its HUD icon. LTCG: pos in EDX.
static item* create_bagman_flag(bagman_flag* flag_info, const vector& pos)
{
    obj_item_flags flags;
    memset(&flags, 0, sizeof(flags));
    obj_item_props props(Bagman_flag_item_info, pos, IDENTITY_MATRIX, flags, COLLIDE_ITEM, -1.0f, INVALID_SALVAGE_MATERIAL);
    item* itemp = World.create_object<item>(props, 0xFFFFFFFF);
    if (itemp == nullptr)
    {
        return nullptr;
    }
    flag_info->bag_effect_handle = effects_play(Bagman_flag_effect_info, itemp->get_handle(), -1, true, 0xFFFFFFFF, 1.0f, false);
    flag_info->bag_handle = itemp->get_handle();
    add_flag_icon(flag_info); // inlined
    return itemp;
}

// 0x00974900 (:221): LTCG: flag_info in ESI.
static void destroy_bagman_flag(bagman_flag* flag_info)
{
    hud_icon_remove(flag_info->bag_handle);
    World.destroy_object(flag_info->bag_handle, false);
    if (flag_info->bag_effect_handle != 0xFFFFFFFFu)
    {
        object_effect* effect = (object_effect*)World.object_from_handle(flag_info->bag_effect_handle);
        if (effect != nullptr && effect->get_obj_type() == OT_EFFECT)
        {
            effect->stop(object_effect::SS_STOP_EMITTING);
        }
    }
    flag_info->bag_effect_handle = 0xFFFFFFFFu;
    flag_info->bag_handle = 0xFFFFFFFFu;
}

// 0x009B8C40 (:235): the bag goes to a teammate of its carrier (the server checks: allied and alive; a client applies
// the server's swap); the server tells everyone with the new carrier's score, a client asks the server. LTCG:
// new_carrier in ECX.
static void swap_bagman_flag(player* new_carrier, bool from_server)
{
    if (new_carrier == nullptr)
    {
        return;
    }
    if ((Multiplayer && Server) || from_server)
    {
        player* carrier = bagman_player_from_handle(Bagman_flag.carrier_handle);
        if (carrier == nullptr)
        {
            return;
        }
        if (Multiplayer && Server)
        {
            if (human_teams_get_disposition(carrier, new_carrier) != TEAM_DISPOSITION_ALLIED || new_carrier->is_killed())
            {
                return;
            }
        }
        Bagman_flag.carrier_handle = new_carrier->get_handle();
        if (!audiolib_is_playing(Bag_picked_up_play_id, true))
        {
            Bag_picked_up_play_id = audiolib_cue_play(Ga_cue_ids.multiplayer_bag_picked_up, nullptr, nullptr);
        }
    }
    if (!(Multiplayer && Server) && from_server)
    {
        return;
    }
    unsigned char packet[1200];
    packet[0] = PT_MODE_SPECIFIC;
    unsigned short packet_size = packet_header_size(PT_MODE_SPECIFIC);
    packet[packet_size++] = Bagman_swapped;
    write_u16_be(packet, packet_size, multi_core_network_handle_from_game_handle(new_carrier->get_handle(), false)); // inlined
    if (Multiplayer && Server)
    {
        write_bagman_scores(packet, packet_size, new_carrier);
    }
    packet_set_size(packet, packet_size);
    if (!Multiplayer)
    {
        return;
    }
    if (Server)
    {
        multi_io_send_to_all_reliable(Game_session, packet, packet_size);
        return;
    }
    multi_io_send_reliable(Game_session->get_host_connection(), packet, packet_size, nullptr, nullptr, nullptr);
}

// 0x009B8EC0 (:289): the swap action's callback: the action's player is within 10 m
static bool player_wants_to_swap_bagman_flag(player* pp, bool do_swap)
{
    player* target = bagman_player_from_handle(pp->action_object.handle);
    if (target == nullptr || vec_dist_squared(pp->pos, target->pos) > Bagman_swap_dist_squared)
    {
        return false;
    }
    if (do_swap)
    {
        swap_bagman_flag(target, false);
    }
    return true;
}

// 0x009E53E0 (:318): a player picked the bag up: the bag item goes, the carrier's hold restarts and scores its first
// point, the HUD and the announcer tell (in team modes, or to everyone but the carrier), the pick-up experience; the
// server tells the clients.
static void bagman_event_flag_picked_up(player* carrier)
{
    if (carrier == nullptr)
    {
        return;
    }
    Bagman_flag.carrier_handle = carrier->get_handle();
    destroy_bagman_flag(&Bagman_flag); // inlined
    multi_player_stats* stats = bagman_stats_of(carrier->mp_data);
    if (stats != nullptr)
    {
        stats->b_time_holding = 0.0f;
    }
    if (bagman_is_team_mode())
    {
        multi_game_stats_alter_team_score(carrier->current_team, 1);
    }
    else
    {
        multi_game_stats_alter_player_score(carrier->mp_data, 1);
    }
    Bagman_flag.score_timer.set(Bagman_score_interval_ms);
    Bagman_flag.return_timer.invalidate();
    const wchar_t* title = bagman_player_title(carrier);
    if (!audiolib_is_playing(Bag_picked_up_play_id, true))
    {
        Bag_picked_up_play_id = audiolib_cue_play(Ga_cue_ids.multiplayer_bag_picked_up, nullptr, nullptr);
    }
    hud_ui_multiplayer_trigger_objective(title, localization_get_display_string_req("HM_BAGMAN_FLAG_PICKED_UP", nullptr), carrier);
    if (bagman_is_team_mode() || !object_is_local_player(carrier))
    {
        local_player_info info;
        player_get_local_player_info(&info);
        const bool ally = object_is_local_player(carrier) || human_teams_get_disposition(info.team, carrier->current_team) == TEAM_DISPOSITION_ALLIED;
        multi_game_announcer_play_line(ally ? MAL_BAG_ALLY_PICKUP : MAL_BAG_ENEMY_PICKUP, 0xFFFFFFFF);
    }
    multi_game_xp_trigger_event(carrier->mp_data, MXE_PICK_UP_BAG, 1, 0xFF, -1, false); // inlined
    if (Multiplayer && Server)
    {
        unsigned char packet[1200];
        packet[0] = PT_MODE_SPECIFIC;
        unsigned short packet_size = packet_header_size(PT_MODE_SPECIFIC);
        packet[packet_size++] = Bagman_picked_up;
        write_u16_be(packet, packet_size, multi_core_network_handle_from_game_handle(carrier->get_handle(), false));
        write_bagman_scores(packet, packet_size, carrier);
        packet_set_size(packet, packet_size);
        multi_io_send_to_all_reliable(multi_session_get_by_type(ST_GAME), packet, packet_size);
    }
}

// 0x009B8F30 (:388): the carrier lost the bag (died, left): a new bag item at pos with the return countdown, the
// announcer and the HUD (the carrier's name; in team modes in the other team's color); the server tells the clients
// with the old carrier's score (all scores without a carrier).
static void bagman_event_flag_dropped(const vector& pos)
{
    player* carrier = bagman_player_from_handle(Bagman_flag.carrier_handle);
    Bagman_flag.carrier_handle = 0xFFFFFFFFu;
    if (create_bagman_flag(&Bagman_flag, pos) == nullptr)
    {
        return;
    }
    // The team and free-for-all branches set the same time.
    Bagman_flag.return_timer.set((int)Mp_server_options.mode_options.bagman.bag_return_time_seconds * 1000);
    multi_game_announcer_play_line(MAL_BAG_BAG_DROPPED, 0xFFFFFFFF);
    if (carrier != nullptr)
    {
        const wchar_t* title = bagman_player_title(carrier);
        audiolib_cue_play(Ga_cue_ids.multiplayer_bag_dropped, nullptr, nullptr);
        if (bagman_is_team_mode())
        {
            human_teams other_team;
            if (carrier->current_team == HUMAN_TEAM_GUERILLA)
            {
                other_team = HUMAN_TEAM_EDF;
            }
            else if (carrier->current_team == HUMAN_TEAM_EDF)
            {
                other_team = HUMAN_TEAM_GUERILLA;
            }
            else
            {
                other_team = HUMAN_TEAM_NONE;
            }
            hud_ui_multiplayer_trigger_objective(title, localization_get_display_string_req("HM_BAGMAN_FLAG_DROPPED", nullptr), other_team);
        }
        else
        {
            const color col = object_is_local_player(carrier) ? Mp_team_colors[0][1] : Mp_team_colors[0][0];
            hud_ui_multiplayer_trigger_objective(title, localization_get_display_string_req("HM_BAGMAN_FLAG_DROPPED", nullptr), HUMAN_TEAM_CIVILIAN, col);
        }
    }
    if (Multiplayer && Server)
    {
        unsigned char packet[1200];
        packet[0] = PT_MODE_SPECIFIC;
        unsigned short packet_size = packet_header_size(PT_MODE_SPECIFIC);
        packet[packet_size++] = Bagman_dropped;
        const float coords[3] = {pos.x, pos.y, pos.z};
        for (int i = 0; i < 3; i++)
        {
            const float swapped = PACKET_SWAP_FLOAT(coords[i]);
            memcpy(packet + packet_size, &swapped, 4);
            packet_size += 4;
        }
        packet[packet_size++] = 0; // no return countdown (the clients start their own)
        write_bagman_scores(packet, packet_size, carrier);
        packet_set_size(packet, packet_size);
        multi_io_send_to_all_reliable(multi_session_get_by_type(ST_GAME), packet, packet_size);
    }
}

// 0x009B9290 (:452): the bag returns to its base; the server tells the clients
static void bagman_event_flag_returned()
{
    destroy_bagman_flag(&Bagman_flag); // inlined
    Bagman_flag.return_timer.invalidate();
    if (create_bagman_flag(&Bagman_flag, Bagman_flag.base_position) == nullptr)
    {
        return;
    }
    multi_game_announcer_play_line(MAL_BAG_BAG_RESPAWNED, 0xFFFFFFFF);
    // Original: the local player's info is fetched and not used.
    local_player_info info;
    player_get_local_player_info(&info);
    hud_ui_multiplayer_trigger_objective(localization_get_display_string_req("HM_BAGMAN_FLAG", nullptr), localization_get_display_string_req("HM_BAGMAN_FLAG_RETURNED", nullptr), HUMAN_TEAM_CIVILIAN, color::WHITE);
    if (Multiplayer && Server)
    {
        unsigned char packet[1200];
        packet[0] = PT_MODE_SPECIFIC;
        unsigned short packet_size = packet_header_size(PT_MODE_SPECIFIC);
        packet[packet_size++] = Bagman_returned;
        packet_set_size(packet, packet_size);
        multi_io_send_to_all_reliable(multi_session_get_by_type(ST_GAME), packet, packet_size);
    }
}

// Port helper: a float in a packet as its big endian bits (rfg.exe's inlined swap, without PACKET_SWAP_FLOAT).
static void write_float_bits_be(unsigned char* packet_data, unsigned short& packet_size, float value)
{
    unsigned int bits;
    memcpy(&bits, &value, sizeof(bits));
    write_u32_be(packet_data, packet_size, bits);
}

static float read_float_bits_be(const unsigned char* packet_data, int& packet_offset)
{
    const unsigned int bits = read_u32_be(packet_data, packet_offset);
    float value;
    memcpy(&value, &bits, sizeof(value));
    return value;
}

// 0x009B93F0 (:486): server: a player joined the round in progress: the carrier (picked up), or the lying bag with
// its remaining return time (dropped), with all scores
void bagman_round_begin_jip(multi_connection* cp)
{
    unsigned char packet[1200];
    packet[0] = PT_MODE_SPECIFIC;
    unsigned short packet_size = packet_header_size(PT_MODE_SPECIFIC);
    if (Bagman_flag.carrier_handle == 0xFFFFFFFFu)
    {
        object* bag = World.object_from_handle(Bagman_flag.bag_handle);
        if (bag == nullptr)
        {
            return;
        }
        packet[packet_size++] = Bagman_dropped;
        write_float_bits_be(packet, packet_size, bag->pos.x);
        write_float_bits_be(packet, packet_size, bag->pos.y);
        write_float_bits_be(packet, packet_size, bag->pos.z);
        packet[packet_size++] = 1; // the return countdown follows
        write_u32_be(packet, packet_size, Bagman_flag.return_timer.valid() ? (unsigned int)Bagman_flag.return_timer.time_until() : 0);
        packet[packet_size++] = 0; // all scores
    }
    else
    {
        packet[packet_size++] = Bagman_picked_up;
        write_u16_be(packet, packet_size, multi_core_network_handle_from_game_handle(Bagman_flag.carrier_handle, false)); // inlined
        packet[packet_size++] = 0; // all scores
    }
    if (bagman_is_team_mode())
    {
        multi_game_stats_write_all_team_scores(packet, packet_size);
    }
    else
    {
        multi_game_stats_write_all_player_scores(packet, packet_size);
    }
    packet_set_size(packet, packet_size);
    multi_io_send_reliable(cp, packet, packet_size, nullptr, nullptr, nullptr);
}

// 0x009911C0 (:530): the level's bag item marks the base: one bag is created there, the placed items go
void bagman_mode_level_init_post()
{
    Bagman_flag.bag_handle = 0xFFFFFFFFu;
    Bagman_flag.carrier_handle = 0xFFFFFFFFu;
    Bagman_flag.score_timer.invalidate();
    Bagman_flag.return_timer.invalidate();
    Bagman_flag_item_info = object_item_get_info("multi_object_bagman_flag");
    Bagman_flag_effect_info = effects_get_effect_info("mp_bagman_marker");
    const int count = World.get_num_objects<item>();
    for (int i = 0; i < count; i++)
    {
        item* itemp = World.get_object<item>(i);
        if (itemp->info != Bagman_flag_item_info)
        {
            continue;
        }
        if (Bagman_flag.bag_handle == 0xFFFFFFFFu)
        {
            item* bag = create_bagman_flag(&Bagman_flag, itemp->pos);
            Bagman_flag.base_position = bag->pos;
        }
        World.destroy_object(itemp, false);
    }
}

// 0x009912A0 (:557): the hold stats start over
void bagman_mode_enter()
{
    for (unsigned int handle = multi_game_data_handles_begin(ST_GAME, false, false); handle != 0xFFFFFFFFu;
         handle = multi_game_data_handles_next(handle, false, false))
    {
        multi_player_stats* stats = bagman_stats_of(handle);
        if (stats != nullptr)
        {
            stats->b_hold_kills = 0;
            stats->b_total_time_holding = 0.0f;
            stats->b_longest_hold = 0.0f;
            stats->b_time_holding = 0.0f;
        }
    }
}

// 0x009E5880 (:573): the carrier holds the bag (the hold experience, a point a second, the hold stats); a lying bag
// falls to the ground (5 m/s); the server returns it when its time is up; "near the point limit" announcements
void bagman_mode_process()
{
    player* carrier = bagman_player_from_handle(Bagman_flag.carrier_handle);
    if (carrier != nullptr)
    {
        if (bagman_is_team_mode())
        {
            multi_game_xp_trigger_event(carrier->mp_data, MXE_HOLD_BAG, 1, 0xFF, -1, false); // inlined
            multi_game_xp_trigger_event(carrier->current_team, MXE_HOLD_BAG_TEAM, 1, false); // inlined
        }
        else
        {
            multi_game_xp_trigger_event(carrier->mp_data, MXE_HOLD_BAG_SOLO, 1, 0xFF, -1, false); // inlined
        }
        if (Bagman_flag.score_timer.elapsed())
        {
            if (bagman_is_team_mode())
            {
                multi_game_stats_alter_team_score(carrier->current_team, 1);
            }
            else
            {
                multi_game_stats_alter_player_score(carrier->mp_data, 1);
            }
            Bagman_flag.score_timer.set(Bagman_score_interval_ms);
        }
        multi_player_stats* stats = bagman_stats_of(carrier->mp_data);
        if (stats != nullptr)
        {
            stats->b_time_holding += Frametime_real;
            stats->b_total_time_holding += Frametime_real;
            if (stats->b_longest_hold < stats->b_time_holding)
            {
                stats->b_longest_hold = stats->b_time_holding;
            }
        }
    }
    else
    {
        object* bag = World.object_from_handle(Bagman_flag.bag_handle);
        if (bag == nullptr)
        {
            return;
        }
        // Magic numbers: the ray starts 10 cm above the bag and reaches 100 m down.
        raycast_input in;
        in.start = bag->pos + UP_VECTOR * 0.1f;
        in.end = bag->pos - UP_VECTOR * 100.0f;
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
            if (fabsf(bag->pos.y - out.m_hits[0].hit_pos.y) >= fall)
            {
                new_pos = bag->pos - UP_VECTOR * fall;
            }
            else
            {
                new_pos = out.m_hits[0].hit_pos;
            }
            bag->update_pos_and_orient(new_pos, bag->orient, true);
        }
    }
    if (Multiplayer && Server && Bagman_flag.return_timer.elapsed())
    {
        bagman_event_flag_returned();
    }
    const unsigned short point_limit = Mp_server_options.generic_options.point_limit;
    if (point_limit == 0)
    {
        return;
    }
    local_player_info info;
    player_get_local_player_info(&info);
    multi_announcer_lines line = INVALID_MULTI_ANNOUNCER_LINE;
    if (bagman_is_team_mode())
    {
        for (int team = HUMAN_TEAM_GUERILLA; team <= HUMAN_TEAM_EDF; team++)
        {
            const multi_team_stats* stats = multi_game_stats_get_team_stats((human_teams)team);
            if (stats != nullptr && point_limit <= stats->round_score + Bagman_win_near_points && point_limit > Bagman_win_near_points)
            {
                line = human_teams_are_friendly(info.team, (human_teams)team) ? MAL_BAG_ALLY_WIN_NEAR : MAL_BAG_ENEMY_WIN_NEAR;
            }
        }
    }
    else
    {
        for (const multi_game_data* data = multi_game_data_begin(ST_GAME, false, false); data != nullptr; data = multi_game_data_next(data, false, false))
        {
            const multi_player_stats* stats = multi_game_stats_get_player_stats(data->handle);
            if (stats == nullptr || point_limit > stats->round_score + Bagman_win_near_points || point_limit <= Bagman_win_near_points)
            {
                continue;
            }
            const bool ally = multi_game_data_is_my_handle(data->handle) || human_teams_are_allies(data->team, info.team);
            line = ally ? MAL_BAG_ALLY_WIN_NEAR : MAL_BAG_ENEMY_WIN_NEAR;
        }
    }
    if (line != INVALID_MULTI_ANNOUNCER_LINE && !multi_game_announcer_line_played_or_pending_this_round(line))
    {
        multi_game_announcer_play_line(line, 0xFFFFFFFF);
    }
}

// 0x009913A0 (:687): the carrier gets a pulsing outline, glowing with the level's glow (less while stealthed). The team
// color it fetches is not used (original).
void bagman_mode_handshake()
{
    local_player_info info;
    player_get_local_player_info(&info);
    rl_scene* scene = game_render_get_main_scene();
    if (scene == nullptr)
    {
        return;
    }
    player* carrier = nullptr;
    const int count = World.get_num_objects<player>();
    for (int i = 0; i < count; i++)
    {
        player* pp = World.get_object<player>(i);
        pp->disable_object_outline();
        if (Bagman_flag.carrier_handle == pp->get_handle())
        {
            carrier = pp;
        }
    }
    if (carrier == nullptr)
    {
        return;
    }
    carrier->enable_object_outline(0);
    rl_outline_state state;
    state.colors[1].set(0.0f, 0.0f, 0.0f, 1.0f);
    color team_color = color::WHITE;
    multi_game_render_get_team_color(team_color, MTCT_OUTLINE, carrier);
    float glow = Multi_match_info.use_default_time_of_day ? Mp_server_options.get_level()->glow_multiplier : Mp_server_options.get_level()->alt_glow_multiplier;
    glow = (float)((1.0 - (double)carrier->m_stealth_percent) * glow);
    // Magic numbers: the outline color is the glow times (0.02, 0.075, 0.01), alpha 1.
    const float red = glow * 0.02f;
    const float green = glow * 0.075f;
    const float blue = glow * 0.01f;
    // A 0.5..1.5 pulse with a period of 1000 * pi ms (Frametime_total_milliseconds as unsigned).
    const float pulse = (float)sin((float)((double)(unsigned int)Frametime_total_milliseconds / 500.0)) * 0.5f + 1.0f;
    state.colors[0].set(red * pulse, green * pulse, blue * pulse, 1.0f * pulse);
    state.layer[0] = RLOL_USE_COLOR0;
    state.layer[1] = RLOL_DISABLED;
    state.layer[2] = RLOL_DISABLED;
    state.layer[3] = RLOL_DISABLED;
    scene->set_outline_state(state);
}

// 0x009B97D0 (:745)
void bagman_mode_remove_player(player* pp)
{
    if (Bagman_flag.carrier_handle == pp->get_handle() && Multiplayer && Server)
    {
        bagman_event_flag_dropped(pp->pos);
    }
}

// 0x009E6330 (:757): the carrier died: the bag drops (server) and its enemy killer gets the kill bagman experience.
// An enemy killed by the carrier: a hold kill, the kill bonus (options) and the bagman kill experience; by a teammate
// of the carrier near it (the event's parameter in m, the victim or the killer): the protect experience.
void bagman_mode_player_died(player* victim, player* killer, damaged_by_types damage_type, const weapon_info* w_info)
{
    const int weapon_id = w_info != nullptr ? w_info->unique_id : 0xFF;
    const int backpack_id = killer->backpack != nullptr ? killer->backpack->get_backpack_info()->unique_id : -1;
    if (Bagman_flag.carrier_handle == victim->get_handle())
    {
        if (Multiplayer && Server)
        {
            bagman_event_flag_dropped(victim->pos);
        }
        if (victim == killer)
        {
            return;
        }
        if (human_teams_get_disposition(victim, killer) == TEAM_DISPOSITION_HOSTILE)
        {
            multi_game_xp_trigger_event(killer->mp_data, MXE_KILL_BAGMAN, 1, weapon_id, backpack_id, false); // inlined
        }
    }
    if (victim == killer || human_teams_get_disposition(victim, killer) != TEAM_DISPOSITION_HOSTILE)
    {
        return;
    }
    if (Bagman_flag.carrier_handle == killer->get_handle())
    {
        multi_player_stats* stats = bagman_stats_of(killer->mp_data);
        if (stats != nullptr)
        {
            stats->b_hold_kills++;
        }
        if (Mp_server_options.mode_options.bagman.bagman_kill_bonus)
        {
            multi_game_stats_alter_player_score(killer->mp_data, Bagman_kill_bonus_points);
            if (multi_game_mode_has_teams())
            {
                multi_game_stats_alter_team_score(killer->current_team, Bagman_kill_bonus_points);
            }
        }
        multi_game_xp_trigger_event(killer->mp_data, MXE_BAGMAN_KILL, 1, weapon_id, backpack_id, false); // inlined
        return;
    }
    const float protect_dist = multi_game_xp_get_event_parameter(MXE_PROTECT_BAGMAN); // inlined
    const float protect_dist_squared = protect_dist * protect_dist;
    player* carrier = bagman_player_from_handle(Bagman_flag.carrier_handle);
    if (carrier == nullptr || human_teams_get_disposition(carrier, killer) != TEAM_DISPOSITION_ALLIED)
    {
        return;
    }
    if (vec_dist_squared(victim->pos, carrier->pos) > protect_dist_squared && vec_dist_squared(killer->pos, carrier->pos) > protect_dist_squared)
    {
        return;
    }
    multi_game_xp_trigger_event(killer->mp_data, MXE_PROTECT_BAGMAN, 1, weapon_id, backpack_id, false);
}

// 0x00974950 (:820): respawning near the carrier: up to 15 for its enemies, 30 for its teammates (100 m, scaled by
// the level's distance multiplier, to nothing)
float bagman_mode_adjust_respawn_weight(const player* pp, const player_start* pstart)
{
    float weight = 0.0f;
    const player* carrier = bagman_player_from_handle(Bagman_flag.carrier_handle);
    if (carrier == nullptr)
    {
        return weight;
    }
    float dist = vec_dist(pstart->pos, carrier->pos) / Mp_server_options.get_level()->distance_multiplier;
    if (human_teams_get_disposition(carrier->current_team, pp->current_team) == TEAM_DISPOSITION_HOSTILE)
    {
        dist /= 100.0f;
        if (dist - 1.0f >= 0.0f)
        {
            dist = 1.0f;
        }
        weight = dist * -15.0f + 15.0f;
    }
    else
    {
        if (human_teams_get_disposition(carrier->current_team, pp->current_team) != TEAM_DISPOSITION_ALLIED)
        {
            return 0.0f;
        }
        dist /= 100.0f;
        if (dist - 1.0f >= 0.0f)
        {
            dist = 1.0f;
        }
        weight = dist * -30.0f + 30.0f;
    }
    return weight + 0.0f;
}

// 0x009E68D0 (:845): PT_MODE_SPECIFIC (packet_data: the subtype byte)
void bagman_mode_process_mode_specific(unsigned char* packet_data, const sender& from)
{
    switch (packet_data[0])
    {
    case Bagman_picked_up:
    {
        int offset = 1;
        const unsigned int handle = multi_core_game_handle_from_network_handle(read_u16_be(packet_data, offset), false);
        if (handle == 0xFFFFFFFFu)
        {
            return;
        }
        player* carrier = bagman_player_from_handle(handle);
        if (carrier == nullptr)
        {
            return;
        }
        bagman_event_flag_picked_up(carrier);
        read_bagman_scores(packet_data, offset);
        return;
    }
    case Bagman_dropped:
    {
        int offset = 1;
        vector pos;
        pos.x = read_float_bits_be(packet_data, offset);
        pos.y = read_float_bits_be(packet_data, offset);
        pos.z = read_float_bits_be(packet_data, offset);
        const bool has_return_time = packet_data[offset++] != 0;
        unsigned int return_time_ms = 0;
        if (has_return_time)
        {
            return_time_ms = read_u32_be(packet_data, offset);
            destroy_bagman_flag(&Bagman_flag);
        }
        bagman_event_flag_dropped(pos);
        if (has_return_time)
        {
            if (return_time_ms != 0)
            {
                Bagman_flag.return_timer.set((int)return_time_ms);
            }
            else
            {
                Bagman_flag.return_timer.invalidate();
            }
        }
        read_bagman_scores(packet_data, offset);
        return;
    }
    case Bagman_returned:
        bagman_event_flag_returned();
        return;
    case Bagman_swapped:
    {
        int offset = 1;
        const unsigned int handle = multi_core_game_handle_from_network_handle(read_u16_be(packet_data, offset), true);
        swap_bagman_flag(handle == 0xFFFFFFFFu ? nullptr : bagman_player_from_handle(handle), true);
        if (Multiplayer && !Server)
        {
            read_bagman_scores(packet_data, offset);
        }
        return;
    }
    default:
        return;
    }
}

// 0x0094BEB0 (:915)
void bagman_mode_round_over(multi_round_winner winner)
{
}

// 0x00991650 (:919)
multi_round_winner bagman_mode_get_round_winner()
{
    multi_round_winner winner;
    winner.player_game_data_handle = multi_game_stats_get_round_winning_player(false, false);
    return winner;
}

// 0x00991660 (:927): the HUD item's elements. Team: our team, the two team scores, carried, carried by us, the carrier's
// team, distance, angle. Free-for-all: the leader (us?, name, score), the second (us?, name, score), carried, carried
// by us, the carrier's name, distance, angle.
void bagman_mode_hud_init(vint_data_item* item)
{
    if (bagman_is_team_mode())
    {
        static const vint_variant_type team_template[8] = {
            VINT_PROP_TYPE_INT, VINT_PROP_TYPE_INT, VINT_PROP_TYPE_INT, VINT_PROP_TYPE_BOOL,
            VINT_PROP_TYPE_BOOL, VINT_PROP_TYPE_INT, VINT_PROP_TYPE_FLOAT, VINT_PROP_TYPE_FLOAT};
        item->set_template(team_template, 8);
        return;
    }
    static const vint_variant_type solo_template[11] = {
        VINT_PROP_TYPE_BOOL, VINT_PROP_TYPE_STRING, VINT_PROP_TYPE_INT, VINT_PROP_TYPE_BOOL, VINT_PROP_TYPE_STRING,
        VINT_PROP_TYPE_INT, VINT_PROP_TYPE_BOOL, VINT_PROP_TYPE_BOOL, VINT_PROP_TYPE_STRING, VINT_PROP_TYPE_FLOAT,
        VINT_PROP_TYPE_FLOAT};
    item->set_template(solo_template, 11);
}

// Port helper: a player's round score for the HUD (0 without stats).
static int bagman_round_score_of(unsigned int handle)
{
    const multi_player_stats* stats = bagman_stats_of(handle);
    return stats == nullptr ? 0 : (int)stats->round_score;
}

// 0x00991740 (:961): the HUD item's elements (see bagman_mode_hud_init)
void bagman_mode_hud_process(vint_data_item* item)
{
    local_player_info info;
    player_get_local_player_info(&info);
    char encoded[192];
    int index;
    if (!bagman_is_team_mode())
    {
        // Port: rfg.exe leaves the array uninitialized (read without players).
        unsigned int handles[16];
        memset(handles, 0xFF, sizeof(handles));
        int num_handles = 0;
        multi_game_stats_sort_players_based_on_round_score(HUMAN_TEAM_NONE, handles, num_handles); // inlined
        // The second line is us unless we lead; with one player, the leader again.
        unsigned int my_handle = 0xFFFFFFFFu;
        unsigned int second = handles[1];
        if (player_get_local_player() != nullptr)
        {
            my_handle = Local_game_data_handles[ST_GAME_SNAPSHOT];
        }
        if (my_handle == 0xFFFFFFFFu || handles[0] == my_handle)
        {
            if (num_handles == 1)
            {
                second = handles[0];
            }
        }
        else
        {
            second = my_handle;
        }
        item->set_element(0, vint_variant(handles[0] == my_handle));
        get_encoded_display_name_from_from_multi_game_data_bagman(multi_game_data_get(handles[0], false, false), sizeof(encoded), encoded, L""); // inlined
        item->set_element(1, vint_variant((const char*)encoded));
        item->set_element(2, vint_variant(bagman_round_score_of(handles[0])));
        item->set_element(3, vint_variant(second == my_handle));
        get_encoded_display_name_from_from_multi_game_data_bagman(multi_game_data_get(second, false, false), sizeof(encoded), encoded, L""); // inlined
        item->set_element(4, vint_variant((const char*)encoded));
        item->set_element(5, vint_variant(bagman_round_score_of(second)));
        index = 6;
    }
    else
    {
        item->set_element(0, vint_variant((int)info.team));
        index = 1;
        for (int team = HUMAN_TEAM_GUERILLA; team <= HUMAN_TEAM_EDF; team++)
        {
            item->set_element(index++, vint_variant((int)Mp_team_stats[team].round_score));
        }
    }
    player* carrier = bagman_player_from_handle(Bagman_flag.carrier_handle);
    object* target = World.object_from_handle(Bagman_flag.bag_handle);
    if (target == nullptr)
    {
        target = carrier;
        if (target == nullptr)
        {
            return;
        }
    }
    item->set_element(index, vint_variant(Bagman_flag.carrier_handle != 0xFFFFFFFFu));
    player* local_player = player_get_local_player();
    item->set_element(index + 1, vint_variant(local_player != nullptr && Bagman_flag.carrier_handle == local_player->get_handle()));
    if (bagman_is_team_mode())
    {
        item->set_element(index + 2, vint_variant(carrier == nullptr ? -1 : (int)carrier->current_team));
    }
    else
    {
        const multi_game_data* data = carrier == nullptr ? nullptr : multi_game_data_get(carrier->mp_data, false, false);
        if (data != nullptr)
        {
            vint_encode_wide_text(encoded, sizeof(encoded), data->display_name);
            item->set_element(index + 2, vint_variant((const char*)encoded));
        }
        else
        {
            // Original: a null string without a carrier name.
            item->set_element(index + 2, vint_variant((const char*)nullptr));
        }
    }
    item->set_element(index + 3, vint_variant(vec_dist_approx(target->pos, *info.pos)));
    const vector to_target = target->pos - *info.pos;
    item->set_element(index + 4, vint_variant(-vec_delta_ang_flattened(to_target, Rfg_camera.real_orient.fvec)));
}

// 0x0094BEC0 (:1088)
void bagman_mode_get_objective_description(char* buffer)
{
    memcpy(buffer, "HM_BAGMAN_OBJECTIVE", sizeof("HM_BAGMAN_OBJECTIVE"));
}

// 0x0094BEF0 (:1093): the carrier's icon is the held bag
bool bagman_mode_alter_player_icon(const player* pp, hud_icon* icon)
{
    if (Bagman_flag.carrier_handle == pp->get_handle())
    {
        icon->bitmap_id = hud_icon_get_bitmap_id(HI_BAGMAN_FLAG_HELD);
        return true;
    }
    return false;
}

// 0x009B9800 (:1104): the carrier's action is the swap with the living, visible teammate within 10 m it looks at most
// directly (at least 0.91 of the camera direction)
bool bagman_mode_player_set_action_object(const player* local_player, t_usable_object& action_object)
{
    if (local_player == nullptr || Bagman_flag.carrier_handle != local_player->get_handle())
    {
        return false;
    }
    // Magic number: the swap needs the teammate's head within about 24 degrees of the camera direction.
    float best_dot = 0.91f;
    float best_dist_squared = 0.0f;
    player* best = nullptr;
    const int count = World.get_num_objects<player>();
    for (int i = 0; i < count; i++)
    {
        player* pp = World.get_object<player>(i);
        if (pp == local_player || pp->is_killed() || !pp->pflags.mp_local_player_can_see_me)
        {
            continue;
        }
        const float dist_squared = vec_dist_squared(pp->pos, local_player->pos);
        if (human_teams_get_disposition(pp, local_player) != TEAM_DISPOSITION_ALLIED || dist_squared > Bagman_swap_dist_squared)
        {
            continue;
        }
        vector head_pos;
        human_get_head_pos(pp, head_pos);
        const float dot = vec_dot_to_point(Rfg_camera.real_orient.fvec, Rfg_camera.real_pos, head_pos);
        if (best_dot <= dot)
        {
            best = pp;
            best_dot = dot;
            best_dist_squared = dist_squared;
        }
    }
    if (best == nullptr)
    {
        return false;
    }
    action_object.action_type = GENERAL_ACTION_MP_MODE_SPECIFIC;
    action_object.dot_prod = best_dot;
    action_object.action_icon = Team_swap_icons[local_player->current_team];
    action_object.action_callback = player_wants_to_swap_bagman_flag;
    action_object.dist_squared = best_dist_squared;
    action_object.handle = best->get_handle();
    return true;
}

// 0x00974AC0 (:1148): the new server starts over with a bag at the base
void bagman_mode_server_migration_post()
{
    destroy_bagman_flag(&Bagman_flag); // inlined
    Bagman_flag.carrier_handle = 0xFFFFFFFFu;
    Bagman_flag.return_timer.invalidate();
    Bagman_flag.score_timer.invalidate();
    item* bag = create_bagman_flag(&Bagman_flag, Bagman_flag.base_position);
    if (bag != nullptr)
    {
        Bagman_flag.bag_handle = bag->get_handle();
    }
}

// 0x00974B50 (:1162): server: hold kills (BE), longest hold and total hold time (PACKET_SWAP_FLOAT)
void bagman_mode_append_mode_specific_stats(unsigned char* packet_data, unsigned short& packet_size, unsigned int player_game_data_handle)
{
    const multi_player_stats* stats = bagman_stats_of(player_game_data_handle);
    if (stats == nullptr || !Multiplayer || !Server)
    {
        return;
    }
    write_u32_be(packet_data, packet_size, (unsigned int)stats->b_hold_kills);
    const float values[2] = {stats->b_longest_hold, stats->b_total_time_holding};
    for (int i = 0; i < 2; i++)
    {
        const float swapped = PACKET_SWAP_FLOAT(values[i]);
        memcpy(packet_data + packet_size, &swapped, 4);
        packet_size += 4;
    }
}

// 0x00974CA0 (:1175): client
void bagman_mode_read_mode_specific_stats(unsigned char* packet_data, int& packet_offset, unsigned int player_game_data_handle)
{
    multi_player_stats* stats = bagman_stats_of(player_game_data_handle);
    if (stats == nullptr || !Multiplayer || Server)
    {
        return;
    }
    stats->b_hold_kills = (int)read_u32_be(packet_data, packet_offset);
    float* values[2] = {&stats->b_longest_hold, &stats->b_total_time_holding};
    for (int i = 0; i < 2; i++)
    {
        memcpy(values[i], packet_data + packet_offset, 4);
        *values[i] = PACKET_SWAP_FLOAT(*values[i]);
        packet_offset += 4;
    }
}

// 0x0094BF20 (:1188): the longest hold, the average hold, the hold kills, the bagman kills
int bagman_mode_get_mode_specific_stat_count()
{
    return 4;
}

// 0x0094BF30 (:1193): the results screen's column names (string ids)
void bagman_mode_get_mode_specific_stat_name(char* name, const int max_name_len, int stat_index)
{
    const char* id;
    switch (stat_index)
    {
    case 0:
        id = "MENU_LONGEST_HOLD";
        break;
    case 1:
        id = "MENU_AVERAGE_HOLD";
        break;
    case 2:
        id = "MENU_HOLD_KILLS";
        break;
    case 3:
        id = "MENU_BAGMAN_KILLS";
        break;
    default:
        id = "";
        break;
    }
    _snprintf_safe(name, max_name_len, "%s", id);
}

// 0x00974E00 (:1214): the column values; the average hold divides by the pick-ups (the pick up bag experience count),
// the bagman kills are the kill bagman experience count. Original: no null check of the stats.
void bagman_mode_get_mode_specific_stat_value(char* value, const int max_value_len, int stat_index, unsigned int player_game_data_handle)
{
    const multi_player_stats* stats = bagman_stats_of(player_game_data_handle);
    float time_seconds;
    switch (stat_index)
    {
    case 0:
        time_seconds = stats->b_longest_hold;
        break;
    case 1:
        time_seconds = 0.0f;
        if (stats->xp_event_frequency[MXE_PICK_UP_BAG] != 0)
        {
            time_seconds = stats->b_total_time_holding / (float)stats->xp_event_frequency[MXE_PICK_UP_BAG];
        }
        break;
    case 2:
        _snprintf_safe(value, max_value_len, "%d", stats->b_hold_kills);
        return;
    case 3:
        _snprintf_safe(value, max_value_len, "%d", (int)stats->xp_event_frequency[MXE_KILL_BAGMAN]);
        return;
    default:
        _snprintf_safe(value, max_value_len, "%s", "");
        return;
    }
    hud_ui_convert_time_for_output(value, max_value_len, time_seconds);
}

// 0x009E6C10 (:1256): the item touch callback of the bag: the server's player picks it up (no pick-up prompt)
bool item_touch_bagman_flag(human* hp, item* obj_itemp, bool& play_secondary_sound, bool& ask_pickup)
{
    ask_pickup = false;
    play_secondary_sound = false;
    if (!Multiplayer || !Server || hp == nullptr || hp->get_obj_type() != OT_HUMAN || hp->get_sub_type() != OT_SUB_HUMAN_PLAYER || obj_itemp->obj_flags.destroyed)
    {
        return false;
    }
    bagman_event_flag_picked_up((player*)hp);
    return true;
}
