// Original: volition/rfg/code/multi/multi_game/modes/multi_game_mode_demolition.cpp
//
// The Demolition row of the mode table: each team has an engineer (the VIP). The building damage an active engineer
// does scores for its team (a point per 1 / (0.00025 x the level's demolition multiplier) of structural mass; damage
// over 2500 that scores nothing still gives a point). The server picks the engineers: a weighted random choice among
// the team's living players, preferring those who said yes to the "become the engineer" vote (each player not chosen
// gets more weight); a chosen engineer becomes active at once, and one that scores nothing for 30 s is replaced. The
// engineer can hand the role to a teammate (the swap action). Killing the enemy engineer scores 5 for the team. The
// server tells everyone (PT_MODE_SPECIFIC: 0 the team scores, 1 a new engineer, 2 an engineer active, 3 a swap
// request, 4 no engineer found, 5 a vote).
// Experience events without a weapon pass 0xFF as the weapon id, as rfg.exe does.
// Part of the multi unity TU (rfg_multi_unity.obj, every multi/ .cpp in path order): the file statics are shared
// by the whole tree, and a shared one is placed in its first user (tools/hdrgen/specs/mp0_stubs.py).

#include "volition/rfg/code/multi/multi_game/modes/multi_game_mode_demolition.h"
#include "volition/rfg/code/multi/multi_game/modes/multi_game_mode_common.h"
#include "volition/rfg/code/multi/multi_game/multi_game.h"
#include "volition/rfg/code/multi/multi_game/multi_game_announcer.h"
#include "volition/rfg/code/multi/multi_game/multi_game_data.h"
#include "volition/rfg/code/multi/multi_game/multi_game_object_backpack.h"
#include "volition/rfg/code/multi/multi_game/multi_game_render.h"
#include "volition/rfg/code/multi/multi_game/multi_game_settings.h"
#include "volition/rfg/code/multi/multi_game/multi_game_stats.h"
#include "volition/rfg/code/multi/multi_game/multi_game_xp.h"
#include "volition/rfg/code/multi/multi_core/include/multi.h"
#include "volition/rfg/code/multi/multi_core/include/multi_connection.h"
#include "volition/rfg/code/multi/multi_core/include/multi_core_io.h"
#include "volition/rfg/code/multi/multi_core/include/multi_core_objects.h"
#include "volition/rfg/code/multi/multi_core/include/multi_core_packet_types.h"
#include "volition/rfg/code/multi/multi_core/include/multi_session.h"
#include "volition/audio_lib/code/audiolib_api.h"
#include "volition/rfg/code/camera/rfg_camera.h"
#include "volition/rfg/code/control/control_map.h"
#include "volition/rfg/code/gameplay/activities/activity_raid_internal.h"
#include "volition/rfg/code/gameplay/safehouse_vip.h"
#include "volition/rfg/code/gamewide/game_render.h"
#include "volition/rfg/code/gamewide/gameaudio.h"
#include "volition/rfg/code/hud/hud_icon.h"
#include "volition/rfg/code/hud/hud_message.h"
#include "volition/rfg/code/interface/localization/localization_shared.h"
#include "volition/rfg/code/interface/ui_document_hud_multiplayer.h"
#include "volition/rfg/code/level/world.h"
#include "volition/rfg/code/objects/humans/human.h"
#include "volition/rfg/code/objects/humans/human_teams.h"
#include "volition/rfg/code/objects/humans/player.h"
#include "volition/rfg/code/objects/object_player_start.h"
#include "volition/rfg/code/rfg.h"
#include "volition/rfg/code/weapons/weapons.h"
#include "volition/render_lib/code/rl_outline_compositor_defines.h"
#include "volition/render_lib/code/rl_scene.h"
#include "volition/interface_lib/code/vint_data_item.h"
#include "volition/interface_lib/code/vint_external.h"
#include "volition/interface_lib/code/vint_variant.h"
#include "volition/vlib/code/math/rand.h"
#include "volition/vlib/code/math/vector3.h"
#include "volition/vlib/code/memory/object_pool.h"
#include "volition/vlib/code/os/frametime.h"
#include "volition/vlib/code/time/timestamp.h"
#include "volition/vlib/code/util/color.h"
#include "volition/vlib/code/util/string_util.h"
#include "volition/vlib/code/util/unicode_tools.h"

#include <math.h>
#include <string.h>
#include <wchar.h>

// ---- file-private types (rfg.pdb)
// size 0x4
struct loaded_client_reference
{
    multi_connection* cp; // 0x00
};

// 0x0094D540: the timers start invalid (their constructors); the rest is not initialized
team_demolition_info::team_demolition_info()
{
}

// 0x0094D560: the fade timer starts invalid (its constructor)
vip_line_info::vip_line_info()
{
}

// ---- file-static globals
// use_object_icon Team_swap_icons_ctf[2] (0x0167DEA8) is used here too; placed in multi_game_mode_common.cpp.
static int Score_play_id = -1; // 0x0167DEB0
static hud_message_handle Demolition_hud_message = INVALID_HUD_MESSAGE_HANDLE; // 0x0167DEB4
// Damage over this much that scores nothing still gives one point (structural mass).
static float Demolition_damage_minimum_for_free_point = 2500.0f; // 0x0167DEB8
// The outline glow over the level's glow multiplier (also used by multi_game_mode_siege.cpp).
static float Mp_glow_multiplier = 1.0f; // 0x0167F2A8
// dynamic_uint_hash_table<remote_object*> Game_handle_to_rop_hash (0x01691B40) is used here too; placed in multi_core_delta_compression.cpp.
// dynamic_uint_hash_table<network_game_handle*> Reserved_game_to_network_hash (0x01691C18) is used here too; placed in multi_core_objects.cpp.
// The score sync of the server (every 100 ms).
static timestamp Demolition_score_sync_timestamp; // 0x01691DC8, initial data ffffffff
// color Mp_team_colors[3][2] (0x016932A8) is used here too; placed in multi_game_mode_bagman.cpp.
// multi_team_stats Mp_team_stats[2] (0x02FE82F0) is used here too; placed in multi_game_mode_bagman.cpp.
// point_of_interest Camera_points_of_interest[2] (0x02FE8300) is used here too; placed in multi_game_camera.cpp (not
// used by this file in the port).
// The engineer choice weights by player index (1 when chosen, +1 each time another is chosen).
static int Demolition_vip_weighting[16]; // 0x02FE88B8
// The players' answers to the "become the engineer" vote, by player index.
static vip_request_statuses Demolition_vip_request_statuses[16]; // 0x02FE8D60
// bool Server (0x02FEB590) is used here too; placed in multi_core_session_generic.cpp.
// Whether the engineer's first HUD line flashes (the "do destruction" line).
static bool Demolition_vip_line_flash; // 0x02FEB63B
// bool Spectating (0x02FEB659) is used here too; placed in multi.cpp.
// mp_game_data_wrapper* Mp_game_data (0x02FEB698) is used here too; placed in multi_connection.cpp.
// multi_player_stats* Mp_player_stats (0x02FEB720) is used here too; placed in multi_game_mode_bagman.cpp.
// The local engineer's HUD lines (the newest first).
static vip_line_info Demolition_vip_lines[3]; // 0x02FED570
// The clients whose loaded packet arrived, waiting for the server's next frame (multi_game_io.cpp). Moved here from
// rfg_multi_unity_pending_early.inl.
static object_pool<loaded_client_reference,16,1> Multi_game_join_loaded_clients; // 0x02FED940
// multi_xp_event Multi_xp_events[84] (0x0300D240) is used here too; placed in multi_game_mode_bagman.cpp.
static team_demolition_info Demolition_team_info[2]; // 0x030176D8

// The PT_MODE_SPECIFIC subtypes (the byte after the header).
static const unsigned char Demolition_scores = 0;
static const unsigned char Demolition_new_vip = 1;
static const unsigned char Demolition_activate_vip = 2;
static const unsigned char Demolition_swap_vip = 3;
static const unsigned char Demolition_no_vip = 4;
static const unsigned char Demolition_request_vip = 5;
// The score per structural mass is this over the level's demolition damage multiplier.
static const double Demolition_score_per_mass = 0.00025;
// The server syncs the scores every 100 ms; a team without an engineer looks again after 10 s; an engineer that
// scores nothing for 30 s is replaced (checked every second); its icon pulses for 5 s after scoring; its "do
// destruction" line comes back after 15 s.
static const int Demolition_score_sync_ms = 100;
static const int Demolition_vip_pass_ms = 10000;
static const int Demolition_vip_auto_swap_ms = 30000;
static const int Demolition_vip_auto_swap_check_ms = 1000;
static const int Demolition_vip_icon_ms = 5000;
static const int Demolition_vip_notification_ms = 15000;
// The HUD line of a score fades in 3 s.
static const int Demolition_vip_line_fade_ms = 3000;
// Killing the enemy engineer scores 5 for the team; the swap reaches 10 m (squared).
static const int Demolition_engineer_kill_points = 5;
static const float Demolition_swap_dist_squared = 100.0f;
static const int Demolition_num_teams = 2;
static const int Demolition_num_vip_lines = 3;

// The PT_MODE_SPECIFIC packets of this file and the scores they carry are written with the port helpers of
// multi_join.cpp (write_u32_be, write_u16_be, packet_header_size, packet_set_size).

// ---- internal functions
static void make_inactive_vip(const player* pp); // 0x009BA710 (:372)
static void swap_vip(const player* pp); // 0x009BAA50 (:525)

// Port helper: the player stats of a game data handle, as rfg.exe inlines it. Original: index 16 is accepted
// although Mp_player_stats has 16 entries.
static multi_player_stats* demolition_stats_of(unsigned int handle)
{
    if (handle == 0xFFFFFFFFu || (handle & 0xFFFF) > 16)
    {
        return nullptr;
    }
    return &Mp_player_stats[handle & 0xFFFF];
}

// Port helper: the player of a handle (null unless a player), as rfg.exe inlines it.
static player* demolition_player_from_handle(unsigned int handle)
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

// Port helper: the vote status slot of a player (null without game data), as rfg.exe inlines it. (rfg.exe also
// compares the 16-bit index with -1, which it never is.)
static vip_request_statuses* demolition_request_status_of(const player* pp)
{
    if (pp->mp_data == 0xFFFFFFFFu)
    {
        return nullptr;
    }
    return &Demolition_vip_request_statuses[pp->mp_data & 0xFFFF];
}

// Port helper: the engineer's highest and lowest destruction take its current one (inlined three times).
static void demolition_record_destruction(multi_player_stats* stats)
{
    const int current = stats->d_current_destruction;
    stats->d_highest_destruction = stats->d_highest_destruction <= current ? current : stats->d_highest_destruction;
    stats->d_lowest_destruction = stats->d_lowest_destruction < current ? stats->d_lowest_destruction : current;
}

// Port helper: the votes start unknown and the weights at 1 (inlined in demolition_mode_reset and _enter).
static void demolition_reset_votes()
{
    for (int i = 0; i < 16; i++)
    {
        Demolition_vip_request_statuses[i] = VRS_UNKNOWN;
    }
    for (int i = 0; i < 16; i++)
    {
        Demolition_vip_weighting[i] = 1;
    }
}

// 0x009765B0 (:166): the points of an amount of structural mass damage
static int score_from_damage(float damage)
{
    const float score_per_mass = (float)(Demolition_score_per_mass / (double)Mp_server_options.get_level()->demolition_damage_multiplier);
    return (int)((double)score_per_mass * (double)damage);
}

// 0x009765F0 (:174): the structural mass damage of a number of points
static float damage_from_score(int score)
{
    const float score_per_mass = (float)(Demolition_score_per_mass / (double)Mp_server_options.get_level()->demolition_damage_multiplier);
    return (float)((double)(float)score / (double)score_per_mass);
}

// 0x0094D570 (:182): the local engineer's HUD lines. No points: only "do destruction" (with the seconds left before
// the engineer is replaced, once 15 s or less), flashing for 15 s. Points: the older lines move down (those still
// showing, with their remaining time) and "+<points>" fades in 3 s.
static void display_engineer_objective(int points)
{
    local_player_info info;
    player_get_local_player_info(&info);
    if (points < 1)
    {
        for (int i = 0; i < Demolition_num_vip_lines; i++)
        {
            Demolition_vip_lines[i].fade_timer.invalidate();
        }
        timestamp& auto_swap = Demolition_team_info[info.team].vip_auto_swap_timer;
        if (auto_swap.elapsed() || auto_swap.time_until() > Demolition_vip_notification_ms)
        {
            wcsncpy(Demolition_vip_lines[0].buffer, localization_get_display_string_req("HM_DEMOLITION_DO_DESTRUCTION", nullptr), 128);
            Demolition_vip_lines[0].buffer[127] = L'\0';
        }
        else
        {
            // The seconds rounded to the nearest.
            const int seconds = (auto_swap.time_until() + 500) / 1000;
            _snprintf_safe(Demolition_vip_lines[0].buffer, 128, "%ls (%d)", localization_get_display_string_req("HM_DEMOLITION_DO_DESTRUCTION", nullptr), seconds);
        }
        Demolition_vip_lines[0].fade_timer.set(Demolition_vip_notification_ms);
        Demolition_vip_line_flash = true;
        return;
    }
    if (!Demolition_vip_line_flash)
    {
        for (int i = Demolition_num_vip_lines - 2; i >= 0; i--)
        {
            vip_line_info& line = Demolition_vip_lines[i];
            vip_line_info& next = Demolition_vip_lines[i + 1];
            if (line.fade_timer.valid() && !line.fade_timer.elapsed())
            {
                wcsncpy(next.buffer, line.buffer, 128);
                next.buffer[127] = L'\0';
                next.fade_timer.set(line.fade_timer.time_until());
            }
            else
            {
                next.fade_timer.invalidate();
            }
        }
    }
    _snprintf_safe(Demolition_vip_lines[0].buffer, 128, "%ls +%d", localization_get_display_string_req("HM_SHOTGUN_DESTRUCTION", nullptr), points);
    Demolition_vip_lines[0].fade_timer.set(Demolition_vip_line_fade_ms);
    Demolition_vip_line_flash = false;
}

// 0x0094D730 (:221)
static void remove_vip_request_hud_message()
{
    if (Demolition_hud_message != INVALID_HUD_MESSAGE_HANDLE)
    {
        hud_message_remove(Demolition_hud_message);
        Demolition_hud_message = INVALID_HUD_MESSAGE_HANDLE;
    }
}

// 0x0094D750 (:229): the "become the engineer" vote (20 s) while the living local player's team is looking for one
static void maybe_add_vip_request_hud_message()
{
    remove_vip_request_hud_message(); // inlined
    player* pp = player_get_local_player();
    if (pp == nullptr || pp->is_killed())
    {
        return;
    }
    const timestamp& pass = Demolition_team_info[pp->current_team].vip_pass_timer;
    if (pass.valid() && !pass.elapsed())
    {
        // Magic number: the vote shows for 20 s.
        Demolition_hud_message = hud_message_add(localization_get_display_string_req("HM_DESTROYER_REQUEST", nullptr), 20.0f, nullptr, nullptr, nullptr);
    }
}

// 0x009BA4A0 (:241): the team's chosen engineer becomes active: the HUD objective (on its own machine), the engineer
// lines and cue, a destruction stat, the announcer (not while spectating), the vote ends for its team. The server
// gives it the engineer weapon (the options') when it has room and tells everyone. Original: the objective shows only
// on the engineer's machine although its label handles another player.
static void activate_vip(human_teams team)
{
    Demolition_team_info[team].vip_active_timer.invalidate();
    player* pp = demolition_player_from_handle(Demolition_team_info[team].vip_handle);
    if (pp == nullptr)
    {
        return;
    }
    local_player_info info;
    player_get_local_player_info(&info);
    if (pp == player_get_local_player())
    {
        const wchar_t* label = L"";
        if (object_is_local_player(pp))
        {
            label = localization_get_display_string_req("HM_KILL_YOU", nullptr);
        }
        else
        {
            const multi_game_data* data = multi_game_data_get(pp->mp_data, false, false); // inlined
            if (data != nullptr)
            {
                label = data->display_name;
            }
        }
        hud_ui_multiplayer_trigger_objective(localization_get_display_string_req("HM_DEMOLITION_NEW_ENGINEER", nullptr), label, pp->current_team);
    }
    for (int i = 0; i < Demolition_num_vip_lines; i++)
    {
        Demolition_vip_lines[i].fade_timer.invalidate();
    }
    if (object_is_local_player(pp))
    {
        display_engineer_objective(0);
        audiolib_cue_play(Ga_cue_ids.multiplayer_become_engineer, nullptr, nullptr);
    }
    multi_player_stats* stats = multi_game_stats_get_player_stats(pp->mp_data);
    if (stats != nullptr)
    {
        stats->d_num_destructions++;
    }
    if (!Spectating)
    {
        multi_game_announcer_play_line(human_teams_are_friendly(pp->current_team, info.team) ? MAL_DMO_ALLY_ENG_NEW : MAL_DMO_ENEMY_ENG_NEW, 0xFFFFFFFF);
    }
    player* local = player_get_local_player();
    if (local != nullptr && local->current_team == team)
    {
        remove_vip_request_hud_message();
    }
    if (!(Multiplayer && Server))
    {
        return;
    }
    const unsigned int engineer_weapon = Mp_server_options.mode_options.demolition.engineer_weapon;
    if (engineer_weapon != 0xFFFFFFFFu && player_has_room_in_inventory(pp))
    {
        const weapon_info* w_info = weapons_get_weapon_info(engineer_weapon);
        if (w_info != nullptr)
        {
            human_add_item_to_inventory(pp, w_info->weapon_inv_item_info, 0, -1, -1, true, false, true);
        }
    }
    unsigned char packet[1200];
    packet[0] = PT_MODE_SPECIFIC;
    unsigned short packet_size = packet_header_size(PT_MODE_SPECIFIC);
    packet[packet_size++] = Demolition_activate_vip;
    packet[packet_size++] = (unsigned char)team;
    packet_set_size(packet, packet_size);
    multi_io_send_to_all_reliable(Game_session, packet, packet_size);
}

// 0x00976630 (:315): the team has no engineer: its icon goes (not the local player's own), its destruction stats are
// recorded, the team looks for a new one in 10 s, the vote shows (show_vip_request_hud) for the local player's team
static void vip_is_gone(human_teams team, bool show_vip_request_hud)
{
    player* local = player_get_local_player();
    local_player_info info;
    player_get_local_player_info(&info);
    team_demolition_info& team_info = Demolition_team_info[team];
    if (local == nullptr || team_info.vip_handle != local->get_handle())
    {
        hud_icon_remove(team_info.vip_handle);
    }
    const player* pp = demolition_player_from_handle(team_info.vip_handle);
    multi_player_stats* stats = pp == nullptr ? nullptr : demolition_stats_of(pp->mp_data);
    if (stats != nullptr)
    {
        demolition_record_destruction(stats); // inlined
        stats->d_current_destruction = 0;
    }
    team_info.vip_handle = 0xFFFFFFFFu;
    team_info.vip_pass_timer.set(Demolition_vip_pass_ms);
    team_info.vip_active_timer.invalidate();
    team_info.vip_icon_timer.invalidate();
    team_info.vip_notification_timer.invalidate();
    team_info.vip_auto_swap_timer.invalidate();
    team_info.xp_score_counter = 0;
    team_info.team_xp_score_counter = 0;
    if (show_vip_request_hud && local != nullptr && local->current_team == team)
    {
        maybe_add_vip_request_hud_message();
    }
    hud_ui_multiplayer_hide_engineer();
}

// 0x009BA710 (:372): pp becomes its team's engineer (the old one is gone), active from the next frame; its weight
// starts over and its team votes again. The server tells everyone. LTCG: pp in EDI.
static void make_inactive_vip(const player* pp)
{
    if (pp == nullptr)
    {
        return;
    }
    const human_teams team = pp->current_team;
    team_demolition_info& team_info = Demolition_team_info[team];
    const player* old_vip = demolition_player_from_handle(team_info.vip_handle);
    if (old_vip != nullptr)
    {
        vip_is_gone(old_vip->current_team, false);
    }
    team_info.vip_handle = pp->get_handle();
    team_info.vip_active_timer.set(0);
    team_info.vip_pass_timer.invalidate();
    team_info.vip_icon_timer.invalidate();
    team_info.vip_notification_timer.set(Demolition_vip_notification_ms);
    team_info.vip_auto_swap_timer.set(Demolition_vip_auto_swap_ms);
    if (pp->mp_data != 0xFFFFFFFFu)
    {
        Demolition_vip_weighting[pp->mp_data & 0xFFFF] = 1;
    }
    const int count = World.get_num_objects<player>();
    for (int i = 0; i < count; i++)
    {
        const player* teammate = World.get_object<player>(i);
        vip_request_statuses* status = demolition_request_status_of(teammate);
        if (status != nullptr && teammate->current_team == team)
        {
            *status = VRS_UNKNOWN;
        }
    }
    if (!(Multiplayer && Server))
    {
        return;
    }
    unsigned char packet[1200];
    packet[0] = PT_MODE_SPECIFIC;
    unsigned short packet_size = packet_header_size(PT_MODE_SPECIFIC);
    packet[packet_size++] = Demolition_new_vip;
    write_u16_be(packet, packet_size, multi_core_network_handle_from_game_handle(pp->get_handle(), false));
    packet_set_size(packet, packet_size);
    multi_io_send_to_all_reliable(Game_session, packet, packet_size);
}

// 0x00993750 (:426): the team's living players with game data other than its engineer, those with the best vote
// answer only; without any, the living engineer itself. LTCG: num_players in EDI.
static void populate_vip_choices(const player** players, int& num_players, human_teams team)
{
    num_players = 0;
    vip_request_statuses best = VRS_NO;
    const int count = World.get_num_objects<player>();
    for (int i = 0; i < count; i++)
    {
        const player* pp = World.get_object<player>(i);
        const vip_request_statuses* status = demolition_request_status_of(pp);
        if (status == nullptr || pp->current_team != team || pp->is_killed() || pp->get_handle() == Demolition_team_info[team].vip_handle)
        {
            continue;
        }
        if (*status < best)
        {
            continue;
        }
        if (*status > best)
        {
            num_players = 0;
            best = *status;
        }
        players[num_players++] = pp;
    }
    if (num_players != 0)
    {
        return;
    }
    const player* vip = demolition_player_from_handle(Demolition_team_info[team].vip_handle);
    if (vip != nullptr && !vip->is_killed())
    {
        players[0] = vip;
        num_players++;
    }
}

// 0x009BA8F0 (:466): server: a weighted random choice among the choices becomes the engineer (every other choice
// gets more weight); without any choice the team looks again in 10 s and tells everyone
static void choose_vip(human_teams team)
{
    const player* players[16];
    int num_players = 0;
    populate_vip_choices(players, num_players, team);
    if (num_players < 1)
    {
        Demolition_team_info[team].vip_pass_timer.set(Demolition_vip_pass_ms);
        unsigned char packet[1200];
        packet[0] = PT_MODE_SPECIFIC;
        unsigned short packet_size = packet_header_size(PT_MODE_SPECIFIC);
        packet[packet_size++] = Demolition_no_vip;
        packet[packet_size++] = (unsigned char)team;
        packet_set_size(packet, packet_size);
        multi_io_send_to_all_reliable(Game_session, packet, packet_size);
        return;
    }
    int total = 0;
    for (int i = 0; i < num_players; i++)
    {
        if (players[i]->mp_data != 0xFFFFFFFFu)
        {
            total += Demolition_vip_weighting[players[i]->mp_data & 0xFFFF];
        }
    }
    const int choice = int_rand_range(1, total);
    const player* chosen = nullptr;
    int sum = 0;
    for (int i = 0; i < num_players; i++)
    {
        if (players[i]->mp_data == 0xFFFFFFFFu)
        {
            continue;
        }
        int& weight = Demolition_vip_weighting[players[i]->mp_data & 0xFFFF];
        sum += weight;
        if (chosen == nullptr && choice <= sum)
        {
            chosen = players[i];
        }
        else
        {
            weight++;
        }
    }
    make_inactive_vip(chosen);
}

// 0x009BAA50 (:525): the engineer hands the role to pp. The server: pp (alive, an ally of the engineer) becomes the
// active engineer; a client asks the server. Original: the server does not check that the asking player is the
// engineer. LTCG: pp in ECX.
static void swap_vip(const player* pp)
{
    if (Multiplayer && Server)
    {
        const player* vip = demolition_player_from_handle(Demolition_team_info[pp->current_team].vip_handle);
        if (vip == nullptr)
        {
            return;
        }
        if (Multiplayer && Server && (human_teams_get_disposition(vip, pp) != TEAM_DISPOSITION_ALLIED || pp->is_killed()))
        {
            return;
        }
        make_inactive_vip(pp);
        activate_vip(pp->current_team);
        return;
    }
    unsigned char packet[1200];
    packet[0] = PT_MODE_SPECIFIC;
    unsigned short packet_size = packet_header_size(PT_MODE_SPECIFIC);
    packet[packet_size++] = Demolition_swap_vip;
    write_u16_be(packet, packet_size, multi_core_network_handle_from_game_handle(pp->get_handle(), false)); // inlined
    packet_set_size(packet, packet_size);
    multi_io_send_reliable(Game_session->get_host_connection(), packet, packet_size, nullptr, nullptr, nullptr);
}

// 0x0094D7F0 (:560): one of the teams' engineers. LTCG: pp in EAX.
static bool player_is_vip(const player* pp)
{
    if (pp == nullptr)
    {
        return false;
    }
    for (int i = 0; i < Demolition_num_teams; i++)
    {
        if (pp->get_handle() == Demolition_team_info[i].vip_handle)
        {
            return true;
        }
    }
    return false;
}

// 0x0094D820 (:575): a living engineer whose team's active timer has run (it is active). LTCG: pp in ESI.
static bool player_is_active_vip(const player* pp)
{
    if (pp == nullptr || pp->is_killed())
    {
        return false;
    }
    for (int i = 0; i < Demolition_num_teams; i++)
    {
        if (pp->get_handle() == Demolition_team_info[i].vip_handle)
        {
            return !Demolition_team_info[pp->current_team].vip_active_timer.valid();
        }
    }
    return false;
}

// 0x009BAC30 (:580): the swap action's callback: the teammate the engineer looks at within 10 m; do_swap hands it
// the role (the cue on the engineer's machine)
static bool player_wants_to_swap_vip(player* pp, bool do_swap)
{
    const player* target = demolition_player_from_handle(pp->action_object.handle);
    if (target == nullptr || vec_dist_squared(pp->pos, target->pos) > Demolition_swap_dist_squared)
    {
        return false;
    }
    if (do_swap)
    {
        if (pp == player_get_local_player())
        {
            audiolib_cue_play(Ga_cue_ids.multiplayer_engineer_passed, nullptr, nullptr);
        }
        swap_vip(target);
    }
    return true;
}

// 0x009EC090 (:606): the team scored score_delta: the engineer's icon pulses and its replacement waits again, its HUD
// line and score cue (on its machine). The points become the demolition experience (one per the event's parameter
// in points) for the engineer and the team demolition experience for its team. Original: a zero parameter loops
// forever (the shipped table has one for both). LTCG: team in EDX, score_delta in ECX.
static void score_has_been_updated(human_teams team, int score_delta)
{
    team_demolition_info& team_info = Demolition_team_info[team];
    const player* vip = demolition_player_from_handle(team_info.vip_handle);
    if (score_delta > 0)
    {
        team_info.vip_icon_timer.set(Demolition_vip_icon_ms);
        team_info.vip_notification_timer.set(Demolition_vip_notification_ms);
        team_info.vip_auto_swap_timer.set(Demolition_vip_auto_swap_ms);
        if (object_is_local_player(vip))
        {
            display_engineer_objective(score_delta);
            if (!audiolib_is_playing(Score_play_id, true))
            {
                Score_play_id = audiolib_cue_play(Ga_cue_ids.multiplayer_destruction_points, nullptr, nullptr);
            }
        }
    }
    team_info.xp_score_counter += score_delta;
    team_info.team_xp_score_counter += score_delta;
    const int points_per_event = (int)multi_game_xp_get_event_parameter(MXE_DEMOLITION); // inlined
    int counter = team_info.xp_score_counter;
    int repetitions = 0;
    if (points_per_event <= counter)
    {
        do
        {
            counter -= points_per_event;
            repetitions++;
        } while (points_per_event <= counter);
        team_info.xp_score_counter = counter;
        if (repetitions > 0 && vip != nullptr)
        {
            multi_game_xp_trigger_event(vip->mp_data, MXE_DEMOLITION, repetitions, 0xFF, -1, false); // inlined
        }
    }
    const int team_points_per_event = (int)multi_game_xp_get_event_parameter(MXE_DEMOLITION_TEAM); // inlined
    counter = team_info.team_xp_score_counter;
    repetitions = 0;
    if (team_points_per_event > counter)
    {
        return;
    }
    do
    {
        counter -= team_points_per_event;
        repetitions++;
    } while (team_points_per_event <= counter);
    team_info.team_xp_score_counter = counter;
    if (repetitions > 0)
    {
        const player* team_vip = demolition_player_from_handle(team_info.vip_handle);
        if (team_vip != nullptr)
        {
            multi_game_xp_trigger_event(team_vip->current_team, MXE_DEMOLITION_TEAM, repetitions, false); // inlined
        }
    }
}

// 0x00993870 (:658): no engineers; a team with a player looks for one in 10 s; the votes and weights start over
static void demolition_mode_reset()
{
    Demolition_score_sync_timestamp.set(Demolition_score_sync_ms);
    for (int team = 0; team < Demolition_num_teams; team++)
    {
        team_demolition_info& team_info = Demolition_team_info[team];
        team_info.damage_delta = 0.0f;
        team_info.vip_handle = 0xFFFFFFFFu;
        team_info.vip_active_timer.invalidate();
        team_info.vip_icon_timer.invalidate();
        team_info.vip_notification_timer.invalidate();
        team_info.vip_auto_swap_timer.invalidate();
        team_info.vip_pass_timer.invalidate();
        const int count = World.get_num_objects<player>();
        for (int i = 0; i < count; i++)
        {
            if (World.get_object<player>(i)->current_team == team)
            {
                team_info.vip_pass_timer.set(Demolition_vip_pass_ms);
                break;
            }
        }
    }
    demolition_reset_votes(); // inlined
}

// 0x009BACB0 (:688): the local answer to the vote; the server records a player's answer, a client sends it.
// LTCG: status in ECX.
static void request_vip(const player* pp, vip_request_statuses status)
{
    if (Multiplayer && Server)
    {
        vip_request_statuses* slot = demolition_request_status_of(pp);
        if (slot != nullptr)
        {
            *slot = status;
        }
        return;
    }
    unsigned char packet[1200];
    packet[0] = PT_MODE_SPECIFIC;
    unsigned short packet_size = packet_header_size(PT_MODE_SPECIFIC);
    packet[packet_size++] = Demolition_request_vip;
    packet[packet_size++] = (unsigned char)status;
    packet_set_size(packet, packet_size);
    multi_io_send_reliable(Game_session->get_host_connection(), packet, packet_size, nullptr, nullptr, nullptr);
}

// 0x009939D0 (:720): the destruction stats start over (the round's players); the votes and weights too
void demolition_mode_enter()
{
    for (unsigned int handle = multi_game_data_handles_begin(ST_GAME_SNAPSHOT, false, false); handle != 0xFFFFFFFFu;
         handle = multi_game_data_handles_next(handle, false, false))
    {
        multi_player_stats* stats = demolition_stats_of(handle);
        if (stats != nullptr)
        {
            stats->d_total_destruction = 0;
            stats->d_current_destruction = 0;
            stats->d_highest_destruction = 0;
            stats->d_lowest_destruction = 0x7FFFFFFF;
            stats->d_num_destructions = 0;
            stats->d_engineer_kills = 0;
        }
    }
    demolition_reset_votes(); // inlined
}

// 0x0094D870 (:741)
void demolition_mode_exit()
{
}

// 0x009EC540 (:747): the server: every 100 ms each team's damage becomes points (a free point for unscored damage over
// the minimum) with the engineer's destruction stats and is sent with the scores; engineers are chosen when a
// team's pass timer runs and activated when the active timer does. Each engineer's icon pulses while it loses health;
// the local engineer's HUD (its "do destruction" line again after 15 s); the server replaces an engineer that scored
// nothing for 30 s. The local player's answer to the vote (yes or no).
void demolition_mode_process()
{
    if (Multiplayer && Server)
    {
        if (Demolition_score_sync_timestamp.elapsed())
        {
            for (int team = 0; team < Demolition_num_teams; team++)
            {
                team_demolition_info& team_info = Demolition_team_info[team];
                int points = score_from_damage(team_info.damage_delta); // inlined
                if (points == 0 && team_info.damage_delta > Demolition_damage_minimum_for_free_point)
                {
                    team_info.damage_delta = 0.0f;
                    points = 1;
                }
                else
                {
                    team_info.damage_delta = (float)((double)team_info.damage_delta - (double)damage_from_score(points)); // inlined
                }
                const player* vip = demolition_player_from_handle(team_info.vip_handle);
                multi_player_stats* stats = vip == nullptr ? nullptr : demolition_stats_of(vip->mp_data);
                if (stats != nullptr && points > 0)
                {
                    stats->d_total_destruction += points;
                    stats->d_current_destruction += points;
                }
                multi_game_stats_alter_team_score((human_teams)team, points);
                score_has_been_updated((human_teams)team, points);
            }
            unsigned char packet[1200];
            packet[0] = PT_MODE_SPECIFIC;
            unsigned short packet_size = packet_header_size(PT_MODE_SPECIFIC);
            packet[packet_size++] = Demolition_scores;
            multi_game_stats_write_all_team_scores(packet, packet_size);
            packet_set_size(packet, packet_size);
            multi_io_send_to_all_reliable(Game_session, packet, packet_size);
            Demolition_score_sync_timestamp.set(Demolition_score_sync_ms);
        }
        for (int team = 0; team < Demolition_num_teams; team++)
        {
            if (Demolition_team_info[team].vip_pass_timer.elapsed())
            {
                choose_vip((human_teams)team);
            }
            else if (Demolition_team_info[team].vip_active_timer.elapsed())
            {
                activate_vip((human_teams)team);
            }
        }
    }
    bool local_is_vip = false;
    for (int team = 0; team < Demolition_num_teams; team++)
    {
        team_demolition_info& team_info = Demolition_team_info[team];
        const player* vip = demolition_player_from_handle(team_info.vip_handle);
        if (vip != nullptr && vip->pflags.mp_lost_hp)
        {
            team_info.vip_icon_timer.set(Demolition_vip_icon_ms);
        }
        if (object_is_local_player(vip))
        {
            local_is_vip = true;
            if (team_info.vip_notification_timer.elapsed())
            {
                display_engineer_objective(0);
            }
            multi_game_engineer_hud hud_info;
            hud_info.flash_first_line = Demolition_vip_line_flash;
            hud_info.title = localization_get_display_string_req("HM_ENGINEER", nullptr);
            for (int i = 0; i < Demolition_num_vip_lines; i++)
            {
                const timestamp& fade = Demolition_vip_lines[i].fade_timer;
                if (!fade.valid() || fade.elapsed())
                {
                    hud_info.text[i] = nullptr;
                    hud_info.text_alpha[i] = 0.0f;
                    continue;
                }
                hud_info.text[i] = Demolition_vip_lines[i].buffer;
                hud_info.text_alpha[i] = Demolition_vip_line_flash ? 1.0f : (float)((double)(float)fade.time_until() / (double)Demolition_vip_line_fade_ms);
            }
            hud_ui_multiplayer_trigger_engineer(&hud_info);
        }
        if (Multiplayer && Server && team_info.vip_auto_swap_timer.elapsed())
        {
            team_info.vip_auto_swap_timer.set(Demolition_vip_auto_swap_check_ms);
            choose_vip((human_teams)team);
        }
    }
    if (!local_is_vip)
    {
        hud_ui_multiplayer_hide_engineer();
    }
    player* pp = player_get_local_player();
    if (pp == nullptr || hud_ui_multiplayer_splash_screen_is_active() || Demolition_hud_message == INVALID_HUD_MESSAGE_HANDLE)
    {
        return;
    }
    if (controls_process_button_just_pressed(CBA_GAC_VOTE_YES, Player_controller_index))
    {
        request_vip(pp, VRS_YES);
        audiolib_cue_play(Ga_cue_ids.ui_sound_press_misc, nullptr, nullptr);
    }
    else if (controls_process_button_just_pressed(CBA_GAC_VOTE_NO, Player_controller_index))
    {
        request_vip(pp, VRS_NO);
        audiolib_cue_play(Ga_cue_ids.ui_sound_press_abort, nullptr, nullptr);
    }
    else
    {
        return;
    }
    remove_vip_request_hud_message(); // inlined
}

// 0x00993B80 (:867): the outlines: each active engineer (and the engineers of the local team) in its team's glow
// color (the level's glow), a layer each; the local team's color takes the next layer when no ally engineer has one.
// A living local engineer sees the explosive weapons lying around in the ally engineer's layer. All pulse.
void demolition_mode_handshake()
{
    local_player_info info;
    player_get_local_player_info(&info);
    const player* local = player_get_local_player();
    const bool local_is_living_vip = player_is_vip(local) && !local->is_killed(); // inlined
    rl_scene* scene = game_render_get_main_scene();
    if (scene == nullptr)
    {
        return;
    }
    rl_outline_state state;
    state.colors[0].set(0.0f, 0.0f, 0.0f, 1.0f);
    state.colors[1].set(0.0f, 0.0f, 0.0f, 1.0f);
    for (int i = 0; i < 4; i++)
    {
        state.layer[i] = RLOL_DISABLED;
    }
    int explosives_layer = -1;
    int num_layers = 0;
    const int count = World.get_num_objects<player>();
    for (int i = 0; i < count; i++)
    {
        player* pp = World.get_object<player>(i);
        pp->disable_object_outline();
        const bool active = !pp->is_killed() && player_is_vip(pp) && !Demolition_team_info[pp->current_team].vip_active_timer.valid();
        if (!active && !(player_is_vip(pp) && human_teams_get_disposition(pp->current_team, info.team) == TEAM_DISPOSITION_ALLIED))
        {
            continue;
        }
        pp->enable_object_outline(num_layers);
        local_player_info viewer;
        player_get_local_player_info(&viewer);
        const bool ally = human_teams_get_disposition(viewer.team, human_teams_get_team(pp)) == TEAM_DISPOSITION_ALLIED || player_get_local_player() == pp;
        const color team_color = Mp_team_colors[1][!ally];
        float glow = Multi_match_info.use_default_time_of_day ? Mp_server_options.get_level()->glow_multiplier : Mp_server_options.get_level()->alt_glow_multiplier;
        glow = (float)((double)glow * (double)Mp_glow_multiplier);
        const unsigned char red = (unsigned char)(int)((double)team_color.red * (double)glow);
        const unsigned char green = (unsigned char)(int)((double)team_color.green * (double)glow);
        const unsigned char blue = (unsigned char)(int)((double)team_color.blue * (double)glow);
        if (human_teams_get_disposition(pp->current_team, info.team) == TEAM_DISPOSITION_ALLIED)
        {
            explosives_layer = num_layers;
        }
        // Original: a third outlined engineer (none in a two-team game) would write past the two colors.
        state.colors[num_layers].set((float)(red / 255.0), (float)(green / 255.0), (float)(blue / 255.0), 1.0f);
        state.layer[num_layers] = (rl_outline_layer)(num_layers + 1);
        num_layers++;
    }
    if (num_layers < 2 && explosives_layer == -1)
    {
        explosives_layer = num_layers;
        color team_color(0x80, 0x80, 0x80, 0xFF);
        multi_game_render_get_team_color(team_color, MTCT_OUTLINE, info.team);
        state.colors[num_layers].set((float)(team_color.red / 255.0), (float)(team_color.green / 255.0), (float)(team_color.blue / 255.0), 1.0f);
        state.layer[num_layers] = (rl_outline_layer)(num_layers + 1);
    }
    // A 0.5..1.5 pulse with a period of 1000 * pi ms (Frametime_total_milliseconds as unsigned).
    const float pulse = (float)sin((float)((double)(unsigned int)Frametime_total_milliseconds / 500.0)) * 0.5f + 1.0f;
    for (int i = 0; i < 2; i++)
    {
        state.colors[i].set(state.colors[i].red * pulse, state.colors[i].green * pulse, state.colors[i].blue * pulse, state.colors[i].alpha * pulse);
    }
    scene->set_outline_state(state); // inlined
    const int num_weapons = World.get_num_objects<weapon>();
    for (int i = 0; i < num_weapons; i++)
    {
        weapon* wp = World.get_object<weapon>(i);
        const bool outline = local_is_living_vip && wp->w_info->is_explosive() && wp->get_parent() == 0xFFFFFFFFu && explosives_layer != -1;
        wp->disable_object_outline();
        if (outline)
        {
            wp->enable_object_outline(explosives_layer);
        }
    }
}

// 0x00976760 (:946): a leaving engineer is gone
void demolition_mode_remove_player(player* pp)
{
    if (player_is_vip(pp)) // inlined
    {
        vip_is_gone(pp->current_team, true);
    }
}

// 0x009EC960 (:955): an engineer died (its cue): it is gone; killed by an enemy, the killer's team scores 5 and the
// killer gets the kill engineer experience and stat. An enemy killed by a teammate of the killer's engineer near it
// (the event's parameter in m, the victim or the killer): the protect engineer experience. The dead player's vote ends.
// Original: the killer is not checked for null, nor its stats.
void demolition_mode_player_died(player* victim, player* killer, damaged_by_types damage_type, const weapon_info* w_info)
{
    const int weapon_id = w_info != nullptr ? w_info->unique_id : 0xFF;
    const int backpack_id = killer->backpack != nullptr ? killer->backpack->get_backpack_info()->unique_id : -1;
    if (player_is_vip(victim)) // inlined
    {
        if (object_is_local_player(victim))
        {
            audiolib_cue_play(Ga_cue_ids.multiplayer_engineer_killed, nullptr, nullptr);
        }
        vip_is_gone(victim->current_team, true);
        if (human_teams_get_disposition(victim, killer) == TEAM_DISPOSITION_HOSTILE)
        {
            multi_game_stats_alter_team_score(killer->current_team, Demolition_engineer_kill_points);
            multi_game_xp_trigger_event(killer->mp_data, MXE_KILL_ENGINEER, 1, weapon_id, backpack_id, false); // inlined
            demolition_stats_of(killer->mp_data)->d_engineer_kills++;
        }
    }
    if (human_teams_get_disposition(victim, killer) == TEAM_DISPOSITION_HOSTILE)
    {
        const player* vip = demolition_player_from_handle(Demolition_team_info[killer->current_team].vip_handle);
        if (vip != nullptr && vip != killer)
        {
            const float protect_dist = multi_game_xp_get_event_parameter(MXE_PROTECT_ENGINEER); // inlined
            const float protect_dist_squared = protect_dist * protect_dist;
            if (vec_dist_squared(victim->pos, vip->pos) <= protect_dist_squared || vec_dist_squared(killer->pos, vip->pos) <= protect_dist_squared)
            {
                multi_game_xp_trigger_event(killer->mp_data, MXE_PROTECT_ENGINEER, 1, weapon_id, backpack_id, false); // inlined
            }
        }
    }
    if (victim == player_get_local_player())
    {
        remove_vip_request_hud_message(); // inlined
    }
    if (Multiplayer && Server)
    {
        vip_request_statuses* status = demolition_request_status_of(victim);
        if (status != nullptr)
        {
            *status = VRS_UNKNOWN;
        }
    }
}

// 0x0094D880 (:1012): the respawned local player sees the vote again
void demolition_mode_player_respawn(player* pp, int player_start_index)
{
    if (player_get_local_player() == pp)
    {
        maybe_add_vip_request_hud_message();
    }
}

// 0x009767A0 (:1019): respawning near the enemy engineer costs up to 20 (20 m, scaled by the level's distance
// multiplier), near our own gains up to 10 (30 m)
float demolition_mode_adjust_respawn_weight(const player* pp, const player_start* pstart)
{
    float weight = 0.0f;
    for (int team = 0; team < Demolition_num_teams; team++)
    {
        const player* vip = demolition_player_from_handle(Demolition_team_info[team].vip_handle);
        if (vip == nullptr)
        {
            continue;
        }
        float dist = vec_dist(pstart->pos, vip->pos) / Mp_server_options.get_level()->distance_multiplier;
        if (human_teams_get_disposition(vip->current_team, pp->current_team) == TEAM_DISPOSITION_HOSTILE)
        {
            dist /= 20.0f;
            if (dist - 1.0f >= 0.0f)
            {
                dist = 1.0f;
            }
            weight = (dist * 20.0f - 20.0f) + weight;
        }
        else if (human_teams_get_disposition(vip->current_team, pp->current_team) == TEAM_DISPOSITION_ALLIED)
        {
            dist /= 30.0f;
            if (dist - 1.0f >= 0.0f)
            {
                dist = 1.0f;
            }
            weight = (dist * -10.0f + 10.0f) + weight;
        }
    }
    return weight;
}

// 0x0094D8A0 (:1047): server: the damage of an active engineer counts for its team
void demolition_mode_building_damage_changed(player* pp, rfg_mover* building, float structural_mass_delta)
{
    if (pp != nullptr && Multiplayer && Server && structural_mass_delta < 0.0f && player_is_active_vip(pp))
    {
        Demolition_team_info[pp->current_team].damage_delta -= structural_mass_delta;
    }
}

// 0x009BADA0 (:1061): the engineer's action is the swap with the living, visible teammate within 10 m it looks at
// most directly (at least 0.91 of the camera direction)
bool demolition_mode_player_set_action_object(const player* local_player, t_usable_object& action_object)
{
    if (!player_is_vip(local_player)) // inlined
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
        if (pp->current_team != local_player->current_team || dist_squared > Demolition_swap_dist_squared)
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
    action_object.dot_prod = best_dot;
    action_object.action_type = GENERAL_ACTION_MP_MODE_SPECIFIC;
    action_object.action_callback = player_wants_to_swap_vip;
    action_object.dist_squared = best_dist_squared;
    action_object.handle = best->get_handle();
    action_object.action_icon = Team_swap_icons_ctf[local_player->current_team];
    return true;
}

// 0x00994100 (:1105): the new server starts over (demolition_mode_reset; rfg.exe jumps into it)
void demolition_server_migration_post()
{
    demolition_mode_reset();
}

// 0x009ECF20 (:1111): PT_MODE_SPECIFIC (packet_data: the subtype byte)
void demolition_mode_process_mode_specific(unsigned char* packet_data, const sender& from)
{
    switch (packet_data[0])
    {
    case Demolition_scores:
    {
        // The scores arrive; each team's change goes through score_has_been_updated.
        unsigned short old_scores[2];
        for (int team = 0; team < Demolition_num_teams; team++)
        {
            old_scores[team] = Mp_team_stats[team].round_score;
        }
        int offset = 1;
        multi_game_stats_read_all_team_scores(packet_data, offset);
        for (int team = 0; team < Demolition_num_teams; team++)
        {
            score_has_been_updated((human_teams)team, (int)Mp_team_stats[team].round_score - (int)old_scores[team]);
        }
        return;
    }
    case Demolition_new_vip:
    {
        int offset = 1;
        const unsigned int handle = multi_core_game_handle_from_network_handle(read_u16_be(packet_data, offset), true);
        const player* pp = demolition_player_from_handle(handle);
        if (pp != nullptr)
        {
            make_inactive_vip(pp);
        }
        return;
    }
    case Demolition_activate_vip:
        if (packet_data[1] < HUMAN_TEAM_CIVILIAN)
        {
            activate_vip((human_teams)packet_data[1]);
        }
        return;
    case Demolition_swap_vip:
    {
        int offset = 1;
        const unsigned int handle = multi_core_game_handle_from_network_handle(read_u16_be(packet_data, offset), true);
        const player* pp = demolition_player_from_handle(handle);
        if (pp == nullptr || from.connection == nullptr)
        {
            return;
        }
        const player* sender_player = multi_game_data_get_player_instance(from.connection->game_data);
        if (sender_player != nullptr && sender_player->current_team == pp->current_team)
        {
            swap_vip(pp);
        }
        return;
    }
    case Demolition_no_vip:
    {
        const human_teams team = (human_teams)packet_data[1];
        if (team >= HUMAN_TEAM_CIVILIAN)
        {
            return;
        }
        Demolition_team_info[team].vip_pass_timer.set(Demolition_vip_pass_ms);
        const player* local = player_get_local_player();
        if (local != nullptr && local->current_team == team)
        {
            remove_vip_request_hud_message();
            maybe_add_vip_request_hud_message();
        }
        return;
    }
    case Demolition_request_vip:
        if (from.connection != nullptr)
        {
            const player* pp = multi_game_data_get_player_instance(from.connection->game_data);
            if (pp != nullptr)
            {
                request_vip(pp, (vip_request_statuses)packet_data[1]);
            }
        }
        return;
    default:
        return;
    }
}

// 0x00994110 (:1214)
void demolition_mode_round_begin()
{
    demolition_mode_reset();
    Demolition_hud_message = INVALID_HUD_MESSAGE_HANDLE;
    maybe_add_vip_request_hud_message();
}

// 0x00994130 (:1222): the vote and the engineer HUD go; the round's players' destruction stats are recorded
void demolition_mode_round_over(multi_round_winner winner)
{
    remove_vip_request_hud_message(); // inlined
    hud_ui_multiplayer_hide_engineer();
    for (unsigned int handle = multi_game_data_handles_begin(ST_GAME_SNAPSHOT, false, false); handle != 0xFFFFFFFFu;
         handle = multi_game_data_handles_next(handle, false, false))
    {
        multi_player_stats* stats = demolition_stats_of(handle);
        if (stats != nullptr)
        {
            demolition_record_destruction(stats); // inlined
        }
    }
}

// 0x00976930 (:1237): the team with the better round score (a tie has no winner)
multi_round_winner demolition_mode_get_round_winner()
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

// 0x0094D900 (:1245): the local team, then per team: the round score, the engineer state (0 icon shown, 1 unknown,
// 2 dead or missing, 3 looking for one), the name, whether it is the local player, the distance, the angle, the
// seconds
void demolition_mode_hud_init(vint_data_item* item)
{
    static const vint_variant_type demolition_template[15] = {
        VINT_PROP_TYPE_INT,
        VINT_PROP_TYPE_INT, VINT_PROP_TYPE_INT, VINT_PROP_TYPE_STRING, VINT_PROP_TYPE_BOOL, VINT_PROP_TYPE_FLOAT, VINT_PROP_TYPE_FLOAT, VINT_PROP_TYPE_INT,
        VINT_PROP_TYPE_INT, VINT_PROP_TYPE_INT, VINT_PROP_TYPE_STRING, VINT_PROP_TYPE_BOOL, VINT_PROP_TYPE_FLOAT, VINT_PROP_TYPE_FLOAT, VINT_PROP_TYPE_INT};
    item->set_template(demolition_template, 15);
}

// 0x00976970 (:1269): the HUD item (demolition_mode_hud_init's template); the seconds are those until the team looks
// for an engineer or until its engineer becomes active, rounded up from half a second
void demolition_mode_hud_process(vint_data_item* item)
{
    local_player_info info;
    player_get_local_player_info(&info);
    item->set_element(0, vint_variant((int)info.team));
    int element = 1;
    for (int team = 0; team < Demolition_num_teams; team++)
    {
        const team_demolition_info& team_info = Demolition_team_info[team];
        const multi_team_stats& team_stats = Mp_team_stats[team];
        int state = 1;
        const wchar_t* name = L"(!!unknown)";
        float dist = 0.0f;
        float angle = 0.0f;
        bool is_local = false;
        if (team_info.vip_pass_timer.valid() || team_info.vip_active_timer.valid())
        {
            state = 3;
        }
        else
        {
            player* vip = demolition_player_from_handle(team_info.vip_handle);
            is_local = object_is_local_player(vip);
            if (vip == nullptr || vip->is_killed() || team_info.vip_pass_timer.valid())
            {
                state = 2;
            }
            if (vip != nullptr)
            {
                const hud_icon* icon = hud_icon_get(vip->get_handle());
                if (icon != nullptr && icon->visible)
                {
                    state = 0;
                }
                const multi_game_data* data = multi_game_data_get(vip->mp_data, false, false); // inlined
                if (data != nullptr)
                {
                    name = data->display_name;
                }
                vector to_vip = vip->pos - *info.pos;
                dist = to_vip.flatten_mag_and_normalize_safe();
                angle = vec_delta_ang_flattened(to_vip, Rfg_camera.real_orient.fvec);
            }
        }
        char encoded[256];
        vint_encode_wide_text(encoded, sizeof(encoded), name);
        int ms = 0;
        const timestamp* timer = team_info.vip_pass_timer.valid() ? &team_info.vip_pass_timer : team_info.vip_active_timer.valid() ? &team_info.vip_active_timer : nullptr;
        if (timer != nullptr)
        {
            ms = timer->time_until() + 500;
            if (ms < 0)
            {
                ms = 0;
            }
        }
        item->set_element(element, vint_variant((int)team_stats.round_score));
        item->set_element(element + 1, vint_variant(state));
        item->set_element(element + 2, vint_variant((const char*)encoded));
        item->set_element(element + 3, vint_variant(is_local));
        item->set_element(element + 4, vint_variant(dist));
        item->set_element(element + 5, vint_variant(-angle));
        item->set_element(element + 6, vint_variant(ms / 1000));
        element += 7;
    }
}

// 0x0094D990 (:1340)
void demolition_mode_get_objective_description(char* buffer)
{
    memcpy(buffer, "HM_DEMOLITION_OBJECTIVE", sizeof("HM_DEMOLITION_OBJECTIVE"));
}

// 0x0094D9C0 (:1345): an engineer has the engineer icon (status rings for 5 s after it scored); without the options'
// "always on" its second line is "Engineer" and the icon follows the visibility rules
bool demolition_mode_alter_player_icon(const player* pp, hud_icon* icon)
{
    if (!player_is_vip(pp)) // inlined
    {
        icon->text[1][0] = L'\0';
        return false;
    }
    icon->bitmap_id = hud_icon_get_bitmap_id(HI_ENGINEER);
    const timestamp& icon_timer = Demolition_team_info[pp->current_team].vip_icon_timer;
    if (icon_timer.valid() && !icon_timer.elapsed())
    {
        icon->status_ring_color.red = 0xFF;
        icon->status_ring_color.green = 0xFF;
        icon->status_ring_color.blue = 0x99;
        icon->status_ring_color.alpha = icon->icon_color.alpha;
        hud_icon_enable_status_rings(icon->obj, true);
        return true;
    }
    if (Mp_server_options.mode_options.demolition.engineer_icon_always_on)
    {
        return true;
    }
    wcsncpy(icon->text[1], localization_get_display_string_req("HM_ENGINEER", nullptr), 32);
    icon->text[1][31] = L'\0';
    return false;
}

// 0x00976D80 (:1369): server: the destruction stats (BE)
void demolition_mode_append_mode_specific_stats(unsigned char* packet_data, unsigned short& packet_size, unsigned int player_game_data_handle)
{
    const multi_player_stats* stats = demolition_stats_of(player_game_data_handle);
    if (stats == nullptr || !Multiplayer || !Server)
    {
        return;
    }
    write_u32_be(packet_data, packet_size, (unsigned int)stats->d_total_destruction);
    write_u32_be(packet_data, packet_size, (unsigned int)stats->d_current_destruction);
    write_u32_be(packet_data, packet_size, (unsigned int)stats->d_highest_destruction);
    write_u32_be(packet_data, packet_size, (unsigned int)stats->d_lowest_destruction);
    write_u32_be(packet_data, packet_size, (unsigned int)stats->d_num_destructions);
    write_u32_be(packet_data, packet_size, (unsigned int)stats->d_engineer_kills);
}

// 0x00976FF0 (:1385): client
void demolition_mode_read_mode_specific_stats(unsigned char* packet_data, int& packet_offset, unsigned int player_game_data_handle)
{
    multi_player_stats* stats = demolition_stats_of(player_game_data_handle);
    if (stats == nullptr || !Multiplayer || Server)
    {
        return;
    }
    stats->d_total_destruction = (int)read_u32_be(packet_data, packet_offset);
    stats->d_current_destruction = (int)read_u32_be(packet_data, packet_offset);
    stats->d_highest_destruction = (int)read_u32_be(packet_data, packet_offset);
    stats->d_lowest_destruction = (int)read_u32_be(packet_data, packet_offset);
    stats->d_num_destructions = (int)read_u32_be(packet_data, packet_offset);
    stats->d_engineer_kills = (int)read_u32_be(packet_data, packet_offset);
}

// 0x0094DAA0 (:1401)
int demolition_mode_get_mode_specific_stat_count()
{
    return 4;
}

// 0x0094DAB0 (:1406): the results screen's column names (string ids)
void demolition_mode_get_mode_specific_stat_name(char* name, const int max_name_len, int stat_index)
{
    if (name == nullptr)
    {
        return;
    }
    static const char* const stat_names[4] = {"MENU_DEM_TOTAL_DESTRUCTION", "MENU_DEM_HIGH_DESTRUCTION", "MENU_DEM_AVERAGE_DESTRUCTION", "MENU_DEM_ENGINEER_KILLS"};
    _snprintf_safe(name, max_name_len, "%s", (unsigned int)stat_index < 4 ? stat_names[stat_index] : "");
}

// 0x009772B0 (:1429): the column values: total, highest, average per engineer turn (0 without one), engineer kills.
// Original: no null check of the stats.
void demolition_mode_get_mode_specific_stat_value(char* value, const int max_value_len, int stat_index, unsigned int player_game_data_handle)
{
    const multi_player_stats* stats = demolition_stats_of(player_game_data_handle);
    switch (stat_index)
    {
    case 0:
        _snprintf_safe(value, max_value_len, "%d", stats->d_total_destruction);
        return;
    case 1:
        _snprintf_safe(value, max_value_len, "%d", stats->d_highest_destruction);
        return;
    case 2:
        _snprintf_safe(value, max_value_len, "%d", stats->d_num_destructions > 0 ? stats->d_total_destruction / stats->d_num_destructions : 0);
        return;
    case 3:
        _snprintf_safe(value, max_value_len, "%d", stats->d_engineer_kills);
        return;
    default:
        _snprintf_safe(value, max_value_len, "%s", "");
        return;
    }
}

// 0x0094DB60 (:1456)
bool demolition_mode_is_vote_active()
{
    return Demolition_hud_message != INVALID_HUD_MESSAGE_HANDLE;
}
