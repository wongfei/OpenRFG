// Original: volition/rfg/code/multi/multi_game/multi_gameplay_mode.cpp
//
// The gameplay mode dispatch: Multi_gameplay_mode selects a row of the mode table, and each hook calls the row's slot
// when it is set, around the common layer (modes/multi_game_mode_common.cpp). The mode stays entered over level
// reloads of the same match.
// Part of the multi unity TU (rfg_multi_unity.obj, every multi/ .cpp in path order): the file statics are shared
// by the whole tree, and a shared one is placed in its first user (tools/hdrgen/specs/mp0_stubs.py).

#include "volition/rfg/code/multi/multi_game/multi_gameplay_mode.h"
#include "volition/rfg/code/multi/multi_game/modes/multi_game_mode_common.h"
#include "volition/rfg/code/multi/multi_game/modes/multi_game_mode_deathmatch.h"
#include "volition/rfg/code/multi/multi_game/modes/multi_game_mode_bagman.h"
#include "volition/rfg/code/multi/multi_game/modes/multi_game_mode_capture_the_flag.h"
#include "volition/rfg/code/multi/multi_game/modes/multi_game_mode_demolition.h"
#include "volition/rfg/code/multi/multi_game/modes/multi_game_mode_king_of_the_hill.h"
#include "volition/rfg/code/multi/multi_game/modes/multi_game_mode_siege.h"
#include "volition/rfg/code/multi/multi_game/modes/multi_game_mode_team_bagman.h"
#include "volition/rfg/code/multi/multi_game/modes/multi_game_mode_team_deathmatch.h"
#include "volition/rfg/code/multi/multi_game/multi_game.h"
#include "volition/rfg/code/multi/multi_game/multi_game_backpack_rack.h"
#include "volition/rfg/code/multi/multi_game/multi_game_settings.h"
#include "volition/rfg/code/multi/multi_game/multi_game_stats.h"
#include "volition/rfg/code/multi/multi_game/multi_game_timer.h"
#include "volition/rfg/code/multi/multi_core/include/multi.h"
#include "volition/rfg/code/gamewide/gameseq.h"
#include "volition/rfg/code/gamewide/stats.h"
#include "volition/rfg/code/interface/ui_document_hud_multiplayer.h"
#include "volition/rfg/code/objects/humans/player.h"
#include "volition/rfg/code/objects/object.h"
#include "volition/vlib/code/os/vconsole.h"
#include "keen/base/trace.hpp"

#include <string.h>

// ---- file-private types (rfg.pdb)
// multi_gameplay_mode_names_multi_game (size 0xC) is defined in multi_core_platform.cpp, the first user of its table.


// struct multi_gameplay_mode_funcs (size 0xB8) is defined in modes/multi_game_mode_common.cpp, the first user of the table.

// ---- file-static globals
// bool Current_mode_has_exited (0x0167DD7B) is used here too; placed in multi_game.cpp.
// player_bandwidth_info Player_bandwidth[6] (0x0167DE2C) is used here too; placed in multi_core_game_bandwidth.cpp.
// int Num_player_bandwidths (0x0167DE5C) is used here too; placed in multi_core_game_bandwidth.cpp.
// use_object_icon Team_swap_icons[2] (0x0167DE64) is used here too; placed in multi_game_mode_bagman.cpp.
// int Bag_picked_up_play_id (0x0167DE6C) is used here too; placed in multi_game_mode_bagman.cpp.
// int Flag_event_play_id_handle (0x0167DE70) is used here too; placed in multi_game_mode_capture_the_flag.cpp.
// int Mp_respawn_invulnerable_time_ms (0x0167DE74) is used here too; placed in multi_game_mode_common.cpp.
// time_left_announcement Time_left_announcements[2] (0x0167DE78) is used here too; placed in multi_game_mode_common.cpp.
// int Num_time_left_announcements (0x0167DE88) is used here too; placed in multi_game_mode_common.cpp.
// ui_match_event Ui_match_events[3] (0x0167DE8C) is used here too; placed in multi_game_mode_common.cpp.
// int Recent_hit_time_ms (0x0167DEA4) is used here too; placed in multi_game_mode_common.cpp.
// use_object_icon Team_swap_icons_ctf[2] (0x0167DEA8) is used here too; placed in multi_game_mode_common.cpp.
// int Score_play_id (0x0167DEB0) is used here too; placed in multi_game_mode_demolition.cpp.
// hud_message_handle Demolition_hud_message (0x0167DEB4) is used here too; placed in multi_game_mode_demolition.cpp.
// float Demolition_damage_minimum_for_free_point (0x0167DEB8) is used here too; placed in multi_game_mode_demolition.cpp.
// int Logging_snapshot_time_interval_ms (0x0167DEBC) is used here too; placed in multi_game_mode_king_of_the_hill.cpp.
// int Target_update_play_id_handle (0x0167DEC0) is used here too; placed in multi_game_mode_king_of_the_hill.cpp.
// float Mp_siege_percent_destruction_required (0x0167DEC4) is used here too; placed in multi_game_mode_siege.cpp.
// int Current_priority (0x0167DEC8) is used here too; placed in multi_game_mode_siege.cpp.
// human_teams Attacking_team (0x0167DECC) is used here too; placed in multi_game_mode_siege.cpp.
// human_teams Defending_team (0x0167DED0) is used here too; placed in multi_game_mode_siege.cpp.
// int Logging_snapshot_time_interval_ms_siege (0x0167DED4) is used here too; placed in multi_game_mode_siege.cpp.
// int Target_destroyed_play_id_handle (0x0167DED8) is used here too; placed in multi_game_mode_siege.cpp.
// unsigned int Spectated_object_handle_spectator (0x0167DEDC) is used here too; placed in multi_game_render.cpp.
// multi_gameplay_mode Multi_gameplay_mode (0x0167DEE0) is used here too; placed in multi_core_platform.cpp (its first user).
// The mode table (MGPM_* index; dumped from rfg.exe on 2026-10-19, typed in the Ghidra DB). The dispatchers of this
// file call a slot when it is set. Deathmatch has no level init hooks; team deathmatch reuses most deathmatch slots
// and team bagman most bagman slots.
// External linkage in the port: modes/multi_game_mode_common.cpp (earlier in the unity TU) declares it.
multi_gameplay_mode_funcs Multi_gameplay_mode_funcs[8] = { // 0x0167DEE8
    { // MGPM_DEATHMATCH
        nullptr, // round_begin_jip
        nullptr, // memory_init
        nullptr, // memory_shutdown
        nullptr, // level_init_pre
        nullptr, // level_init_post
        nullptr, // level_shutdown
        deathmatch_mode_enter, // enter
        nullptr, // exit
        deathmatch_mode_process, // process
        deathmatch_mode_handshake, // handshake
        nullptr, // render
        nullptr, // add_player
        nullptr, // remove_player
        deathmatch_mode_player_died, // player_died
        deathmatch_mode_player_respawn, // player_respawn
        nullptr, // adjust_respawn_weight
        nullptr, // building_damage_changed
        nullptr, // can_item_respawn
        nullptr, // item_touched
        nullptr, // player_set_action_object
        nullptr, // player_can_swap_weapons
        nullptr, // player_can_attack
        nullptr, // get_default_equipment
        nullptr, // process_mode_specific_packet
        nullptr, // get_time_ms
        nullptr, // countdown_begin
        nullptr, // round_begin
        deathmatch_mode_round_over, // round_over
        nullptr, // is_round_over
        deathmatch_mode_get_round_winner, // get_round_winner
        nullptr, // server_migration_pre
        nullptr, // server_migration_post
        nullptr, // server_migration_update_clients
        deathmatch_mode_append_mode_specific_stats, // append_mode_specific_stats
        deathmatch_mode_read_mode_specific_stats, // read_mode_specific_stats
        deathmatch_mode_get_mode_specific_stat_count, // get_mode_specific_stat_count
        deathmatch_mode_get_mode_specific_stat_name, // get_mode_specific_stat_name
        deathmatch_mode_get_mode_specific_stat_value, // get_mode_specific_stat_value
        nullptr, // get_decorated_name
        deathmatch_mode_get_objective_description, // get_objective_description
        "multiplayer_hud_mode_deathmatch", // hud_data_item_name
        nullptr, // hud_init
        nullptr, // hud_process
        nullptr, // alter_player_icon
        MAL_DM_INTRO, // intro_line
        false, // is_team_mode
    },
    { // MGPM_TEAM_DEATHMATCH
        nullptr, // round_begin_jip
        nullptr, // memory_init
        nullptr, // memory_shutdown
        nullptr, // level_init_pre
        nullptr, // level_init_post
        nullptr, // level_shutdown
        deathmatch_mode_enter, // enter
        nullptr, // exit
        team_deathmatch_mode_process, // process
        nullptr, // handshake
        nullptr, // render
        nullptr, // add_player
        nullptr, // remove_player
        deathmatch_mode_player_died, // player_died
        deathmatch_mode_player_respawn, // player_respawn
        nullptr, // adjust_respawn_weight
        nullptr, // building_damage_changed
        nullptr, // can_item_respawn
        nullptr, // item_touched
        nullptr, // player_set_action_object
        nullptr, // player_can_swap_weapons
        nullptr, // player_can_attack
        nullptr, // get_default_equipment
        nullptr, // process_mode_specific_packet
        nullptr, // get_time_ms
        nullptr, // countdown_begin
        nullptr, // round_begin
        deathmatch_mode_round_over, // round_over
        nullptr, // is_round_over
        team_deathmatch_mode_get_round_winner, // get_round_winner
        nullptr, // server_migration_pre
        nullptr, // server_migration_post
        nullptr, // server_migration_update_clients
        deathmatch_mode_append_mode_specific_stats, // append_mode_specific_stats
        deathmatch_mode_read_mode_specific_stats, // read_mode_specific_stats
        deathmatch_mode_get_mode_specific_stat_count, // get_mode_specific_stat_count
        deathmatch_mode_get_mode_specific_stat_name, // get_mode_specific_stat_name
        deathmatch_mode_get_mode_specific_stat_value, // get_mode_specific_stat_value
        nullptr, // get_decorated_name
        team_deathmatch_mode_get_objective_description, // get_objective_description
        "multiplayer_hud_mode_team_deathmatch", // hud_data_item_name
        nullptr, // hud_init
        nullptr, // hud_process
        nullptr, // alter_player_icon
        MAL_TDM_INTRO, // intro_line
        true, // is_team_mode
    },
    { // MGPM_CAPTURE_THE_FLAG
        nullptr, // round_begin_jip
        nullptr, // memory_init
        nullptr, // memory_shutdown
        nullptr, // level_init_pre
        capture_the_flag_mode_level_init_post, // level_init_post
        nullptr, // level_shutdown
        capture_the_flag_mode_enter, // enter
        nullptr, // exit
        capture_the_flag_mode_process, // process
        nullptr, // handshake
        nullptr, // render
        nullptr, // add_player
        capture_the_flag_mode_remove_player, // remove_player
        capture_the_flag_mode_player_died, // player_died
        nullptr, // player_respawn
        capture_the_flag_mode_adjust_respawn_weight, // adjust_respawn_weight
        nullptr, // building_damage_changed
        nullptr, // can_item_respawn
        capture_the_flag_mode_item_touched, // item_touched
        capture_the_flag_mode_player_set_action_object, // player_set_action_object
        capture_the_flag_mode_player_can_swap_weapons, // player_can_swap_weapons
        capture_the_flag_mode_player_can_attack, // player_can_attack
        nullptr, // get_default_equipment
        capture_the_flag_mode_process_mode_specific, // process_mode_specific_packet
        nullptr, // get_time_ms
        nullptr, // countdown_begin
        nullptr, // round_begin
        nullptr, // round_over
        nullptr, // is_round_over
        capture_the_flag_mode_get_round_winner, // get_round_winner
        nullptr, // server_migration_pre
        capture_the_flag_mode_server_migration_post, // server_migration_post
        nullptr, // server_migration_update_clients
        capture_the_flag_mode_append_mode_specific_stats, // append_mode_specific_stats
        capture_the_flag_mode_read_mode_specific_stats, // read_mode_specific_stats
        capture_the_flag_mode_get_mode_specific_stat_count, // get_mode_specific_stat_count
        capture_the_flag_mode_get_mode_specific_stat_name, // get_mode_specific_stat_name
        capture_the_flag_mode_get_mode_specific_stat_value, // get_mode_specific_stat_value
        nullptr, // get_decorated_name
        capture_the_flag_mode_get_objective_description, // get_objective_description
        "multiplayer_hud_mode_capture_the_flag", // hud_data_item_name
        capture_the_flag_mode_hud_init, // hud_init
        capture_the_flag_mode_hud_process, // hud_process
        capture_the_flag_mode_alter_player_icon, // alter_player_icon
        MAL_CTF_INTRO, // intro_line
        true, // is_team_mode
    },
    { // MGPM_SIEGE
        siege_mode_round_begin_jip, // round_begin_jip
        siege_mode_memory_init, // memory_init
        siege_mode_memory_shutdown, // memory_shutdown
        nullptr, // level_init_pre
        siege_mode_level_init_post, // level_init_post
        siege_mode_level_shutdown, // level_shutdown
        siege_mode_enter, // enter
        siege_mode_exit, // exit
        siege_mode_process, // process
        siege_mode_handshake, // handshake
        nullptr, // render
        nullptr, // add_player
        nullptr, // remove_player
        siege_mode_player_died, // player_died
        nullptr, // player_respawn
        siege_mode_adjust_respawn_weight, // adjust_respawn_weight
        siege_mode_building_damage_changed, // building_damage_changed
        nullptr, // can_item_respawn
        nullptr, // item_touched
        nullptr, // player_set_action_object
        nullptr, // player_can_swap_weapons
        nullptr, // player_can_attack
        siege_mode_get_default_equipment, // get_default_equipment
        siege_mode_process_mode_specific, // process_mode_specific_packet
        nullptr, // get_time_ms
        nullptr, // countdown_begin
        siege_mode_round_begin, // round_begin
        siege_mode_round_over, // round_over
        siege_mode_is_round_over, // is_round_over
        siege_mode_get_round_winner, // get_round_winner
        nullptr, // server_migration_pre
        siege_mode_server_migration_post, // server_migration_post
        nullptr, // server_migration_update_clients
        siege_mode_append_mode_specific_stats, // append_mode_specific_stats
        siege_mode_read_mode_specific_stats, // read_mode_specific_stats
        siege_mode_get_mode_specific_stat_count, // get_mode_specific_stat_count
        siege_mode_get_mode_specific_stat_name, // get_mode_specific_stat_name
        siege_mode_get_mode_specific_stat_value, // get_mode_specific_stat_value
        siege_mode_get_decorated_name, // get_decorated_name
        siege_mode_get_objective_description, // get_objective_description
        "multiplayer_hud_mode_siege", // hud_data_item_name
        siege_mode_hud_init, // hud_init
        siege_mode_hud_process, // hud_process
        nullptr, // alter_player_icon
        MAL_SGE_INTRO, // intro_line
        true, // is_team_mode
    },
    { // MGPM_KING_OF_THE_HILL
        nullptr, // round_begin_jip
        nullptr, // memory_init
        nullptr, // memory_shutdown
        nullptr, // level_init_pre
        king_of_the_hill_mode_level_init_post, // level_init_post
        nullptr, // level_shutdown
        king_of_the_hill_mode_enter, // enter
        nullptr, // exit
        king_of_the_hill_mode_process, // process
        king_of_the_hill_mode_handshake, // handshake
        nullptr, // render
        nullptr, // add_player
        nullptr, // remove_player
        king_of_the_hill_mode_player_died, // player_died
        nullptr, // player_respawn
        king_of_the_hill_mode_adjust_respawn_weight, // adjust_respawn_weight
        king_of_the_hill_mode_building_damage_changed, // building_damage_changed
        nullptr, // can_item_respawn
        nullptr, // item_touched
        nullptr, // player_set_action_object
        nullptr, // player_can_swap_weapons
        nullptr, // player_can_attack
        nullptr, // get_default_equipment
        king_of_the_hill_mode_process_mode_specific, // process_mode_specific_packet
        nullptr, // get_time_ms
        nullptr, // countdown_begin
        king_of_the_hill_mode_round_begin, // round_begin
        king_of_the_hill_mode_round_over, // round_over
        nullptr, // is_round_over
        king_of_the_hill_mode_get_round_winner, // get_round_winner
        nullptr, // server_migration_pre
        nullptr, // server_migration_post
        king_of_the_hill_mode_server_migration_update_clients, // server_migration_update_clients
        king_of_the_hill_mode_append_mode_specific_stats, // append_mode_specific_stats
        king_of_the_hill_mode_read_mode_specific_stats, // read_mode_specific_stats
        king_of_the_hill_mode_get_mode_specific_stat_count, // get_mode_specific_stat_count
        king_of_the_hill_mode_get_mode_specific_stat_name, // get_mode_specific_stat_name
        king_of_the_hill_mode_get_mode_specific_stat_value, // get_mode_specific_stat_value
        nullptr, // get_decorated_name
        king_of_the_hill_mode_get_objective_description, // get_objective_description
        "multiplayer_hud_mode_king_of_the_hill", // hud_data_item_name
        nullptr, // hud_init
        nullptr, // hud_process
        nullptr, // alter_player_icon
        MAL_KOTH_INTRO, // intro_line
        true, // is_team_mode
    },
    { // MGPM_BAGMAN
        bagman_round_begin_jip, // round_begin_jip
        nullptr, // memory_init
        nullptr, // memory_shutdown
        nullptr, // level_init_pre
        bagman_mode_level_init_post, // level_init_post
        nullptr, // level_shutdown
        bagman_mode_enter, // enter
        nullptr, // exit
        bagman_mode_process, // process
        bagman_mode_handshake, // handshake
        nullptr, // render
        nullptr, // add_player
        bagman_mode_remove_player, // remove_player
        bagman_mode_player_died, // player_died
        nullptr, // player_respawn
        bagman_mode_adjust_respawn_weight, // adjust_respawn_weight
        nullptr, // building_damage_changed
        nullptr, // can_item_respawn
        nullptr, // item_touched
        bagman_mode_player_set_action_object, // player_set_action_object
        nullptr, // player_can_swap_weapons
        nullptr, // player_can_attack
        nullptr, // get_default_equipment
        bagman_mode_process_mode_specific, // process_mode_specific_packet
        nullptr, // get_time_ms
        nullptr, // countdown_begin
        nullptr, // round_begin
        bagman_mode_round_over, // round_over
        nullptr, // is_round_over
        bagman_mode_get_round_winner, // get_round_winner
        nullptr, // server_migration_pre
        bagman_mode_server_migration_post, // server_migration_post
        nullptr, // server_migration_update_clients
        bagman_mode_append_mode_specific_stats, // append_mode_specific_stats
        bagman_mode_read_mode_specific_stats, // read_mode_specific_stats
        bagman_mode_get_mode_specific_stat_count, // get_mode_specific_stat_count
        bagman_mode_get_mode_specific_stat_name, // get_mode_specific_stat_name
        bagman_mode_get_mode_specific_stat_value, // get_mode_specific_stat_value
        nullptr, // get_decorated_name
        bagman_mode_get_objective_description, // get_objective_description
        "multiplayer_hud_mode_bagman", // hud_data_item_name
        bagman_mode_hud_init, // hud_init
        bagman_mode_hud_process, // hud_process
        bagman_mode_alter_player_icon, // alter_player_icon
        MAL_BAG_INTRO, // intro_line
        false, // is_team_mode
    },
    { // MGPM_TEAM_BAGMAN
        bagman_round_begin_jip, // round_begin_jip
        nullptr, // memory_init
        nullptr, // memory_shutdown
        nullptr, // level_init_pre
        bagman_mode_level_init_post, // level_init_post
        nullptr, // level_shutdown
        bagman_mode_enter, // enter
        nullptr, // exit
        bagman_mode_process, // process
        bagman_mode_handshake, // handshake
        nullptr, // render
        nullptr, // add_player
        bagman_mode_remove_player, // remove_player
        bagman_mode_player_died, // player_died
        nullptr, // player_respawn
        bagman_mode_adjust_respawn_weight, // adjust_respawn_weight
        nullptr, // building_damage_changed
        nullptr, // can_item_respawn
        nullptr, // item_touched
        bagman_mode_player_set_action_object, // player_set_action_object
        nullptr, // player_can_swap_weapons
        nullptr, // player_can_attack
        nullptr, // get_default_equipment
        bagman_mode_process_mode_specific, // process_mode_specific_packet
        nullptr, // get_time_ms
        nullptr, // countdown_begin
        nullptr, // round_begin
        bagman_mode_round_over, // round_over
        team_bagman_mode_is_round_over, // is_round_over
        team_bagman_mode_get_round_winner, // get_round_winner
        nullptr, // server_migration_pre
        bagman_mode_server_migration_post, // server_migration_post
        nullptr, // server_migration_update_clients
        bagman_mode_append_mode_specific_stats, // append_mode_specific_stats
        bagman_mode_read_mode_specific_stats, // read_mode_specific_stats
        bagman_mode_get_mode_specific_stat_count, // get_mode_specific_stat_count
        bagman_mode_get_mode_specific_stat_name, // get_mode_specific_stat_name
        bagman_mode_get_mode_specific_stat_value, // get_mode_specific_stat_value
        nullptr, // get_decorated_name
        team_bagman_mode_get_objective_description, // get_objective_description
        "multiplayer_hud_mode_team_bagman", // hud_data_item_name
        bagman_mode_hud_init, // hud_init
        bagman_mode_hud_process, // hud_process
        bagman_mode_alter_player_icon, // alter_player_icon
        MAL_TBAG_INTRO, // intro_line
        true, // is_team_mode
    },
    { // MGPM_DEMOLITION
        nullptr, // round_begin_jip
        nullptr, // memory_init
        nullptr, // memory_shutdown
        nullptr, // level_init_pre
        nullptr, // level_init_post
        nullptr, // level_shutdown
        demolition_mode_enter, // enter
        demolition_mode_exit, // exit
        demolition_mode_process, // process
        demolition_mode_handshake, // handshake
        nullptr, // render
        nullptr, // add_player
        demolition_mode_remove_player, // remove_player
        demolition_mode_player_died, // player_died
        demolition_mode_player_respawn, // player_respawn
        demolition_mode_adjust_respawn_weight, // adjust_respawn_weight
        demolition_mode_building_damage_changed, // building_damage_changed
        nullptr, // can_item_respawn
        nullptr, // item_touched
        demolition_mode_player_set_action_object, // player_set_action_object
        nullptr, // player_can_swap_weapons
        nullptr, // player_can_attack
        nullptr, // get_default_equipment
        demolition_mode_process_mode_specific, // process_mode_specific_packet
        nullptr, // get_time_ms
        nullptr, // countdown_begin
        demolition_mode_round_begin, // round_begin
        demolition_mode_round_over, // round_over
        nullptr, // is_round_over
        demolition_mode_get_round_winner, // get_round_winner
        nullptr, // server_migration_pre
        demolition_server_migration_post, // server_migration_post
        nullptr, // server_migration_update_clients
        demolition_mode_append_mode_specific_stats, // append_mode_specific_stats
        demolition_mode_read_mode_specific_stats, // read_mode_specific_stats
        demolition_mode_get_mode_specific_stat_count, // get_mode_specific_stat_count
        demolition_mode_get_mode_specific_stat_name, // get_mode_specific_stat_name
        demolition_mode_get_mode_specific_stat_value, // get_mode_specific_stat_value
        nullptr, // get_decorated_name
        demolition_mode_get_objective_description, // get_objective_description
        "multiplayer_hud_mode_demolition", // hud_data_item_name
        demolition_mode_hud_init, // hud_init
        demolition_mode_hud_process, // hud_process
        demolition_mode_alter_player_icon, // alter_player_icon
        MAL_DMO_INTRO, // intro_line
        true, // is_team_mode
    },
};
// multi_gameplay_mode_names_multi_game Multi_gameplay_mode_names[8] (0x0167E4A8) is used here too; placed in multi_core_platform.cpp
// (its first user).
// multi_timer Timer_time (0x01693A64) is used here too; placed in multi_core_platform.cpp.
// multi_player_uid Winner_player_uid (0x02FE8DCC) is used here too; placed in multi_game_mode_common.cpp.
// bool Server (0x02FEB590) is used here too; placed in multi_core_session_generic.cpp.
// multi_player_stats* Mp_player_stats (0x02FEB720) is used here too; placed in multi_game_mode_bagman.cpp.
// keen::SystemTimer s_preGameWaitForClientsTimer (0x02FEBC1C) is used here too; placed in multi_game_mode_common.cpp.

// ---- internal functions

// 0x009ABA60 (:547)
static void multi_gameplay_mode_enter()
{
    multi_game_mode_match_init();
    if (Multi_gameplay_mode_funcs[Multi_gameplay_mode].enter != nullptr)
    {
        Multi_gameplay_mode_funcs[Multi_gameplay_mode].enter();
    }
    Current_mode_has_exited = false;
}

// 0x009EFF70 (:562)
static void multi_gameplay_mode_exit()
{
    if (Current_mode_has_exited)
    {
        return;
    }
    multi_game_mode_match_shutdown();
    Current_mode_has_exited = true;
    if (Multi_gameplay_mode_funcs[Multi_gameplay_mode].exit != nullptr)
    {
        Multi_gameplay_mode_funcs[Multi_gameplay_mode].exit();
    }
}

// 0x009EFFB0 (:580). LTCG: new_mode in ESI.
static void multi_gameplay_mode_change(multi_gameplay_mode new_mode)
{
    if (Multi_gameplay_mode != MGPM_NONE)
    {
        multi_gameplay_mode_exit();
    }
    Multi_gameplay_mode = new_mode;
    if (new_mode != MGPM_NONE)
    {
        multi_gameplay_mode_enter();
    }
}

// ---- external functions

// 0x0094E9A0 (:601)
void multi_gameplay_mode_round_begin_jip(multi_connection* cp)
{
    if (Multi_gameplay_mode_funcs[Multi_gameplay_mode].round_begin_jip != nullptr)
    {
        Multi_gameplay_mode_funcs[Multi_gameplay_mode].round_begin_jip(cp);
    }
}

// 0x00978F50 (:614): every level starts in pregame, waiting for the clients
void multi_gameplay_mode_level_init_pre()
{
    // Magic number: 60,000 ms per minute.
    Multi_match_info.round_time_ms = (unsigned int)Mp_server_options.generic_options.round_time_limit_minutes * 60000;
    Client_state = CLIENT_STATE_LOADING_GAME;
    Multi_match_info.setting_up_round = true;
    Multi_match_info.synchronizing_endgame = false;
    Multi_match_info.match_state = MGMS_PREGAME;
    s_preGameWaitForClientsTimer.start();
    Winner_player_uid.invalidate();
    if (Multi_gameplay_mode_funcs[Multi_gameplay_mode].level_init_pre != nullptr)
    {
        Multi_gameplay_mode_funcs[Multi_gameplay_mode].level_init_pre();
    }
}

// 0x009ABA90 (:630)
void multi_gameplay_mode_level_init_post()
{
    multi_game_mode_level_init_post_common();
    if (Multi_gameplay_mode_funcs[Multi_gameplay_mode].level_init_post != nullptr)
    {
        Multi_gameplay_mode_funcs[Multi_gameplay_mode].level_init_post();
    }
}

// 0x009F0020 (:652): the mode stays entered over a level reload (GS_LOAD next)
void multi_gameplay_mode_level_shutdown()
{
    if (Multi_gameplay_mode_funcs[Multi_gameplay_mode].level_shutdown != nullptr)
    {
        Multi_gameplay_mode_funcs[Multi_gameplay_mode].level_shutdown();
    }
    if (gameseq_get_pending_state() != GS_LOAD)
    {
        multi_gameplay_mode_exit();
    }
}

// 0x009FE5B0 (:678)
void multi_gameplay_mode_process()
{
    if (multi_game_mode_process_common() && Multi_gameplay_mode_funcs[Multi_gameplay_mode].process != nullptr)
    {
        Multi_gameplay_mode_funcs[Multi_gameplay_mode].process();
    }
}

// 0x0094E9C0 (:695)
void multi_gameplay_mode_handshake()
{
    if (Multi_gameplay_mode_funcs[Multi_gameplay_mode].handshake != nullptr)
    {
        Multi_gameplay_mode_funcs[Multi_gameplay_mode].handshake();
    }
}

// 0x00995A30 (:712)
void multi_gameplay_mode_render()
{
    multi_game_backpack_rack_render();
    if (Multi_gameplay_mode_funcs[Multi_gameplay_mode].render != nullptr)
    {
        Multi_gameplay_mode_funcs[Multi_gameplay_mode].render();
    }
}

// 0x0094E9E0 (:731): players stay hidden until the round begins
void multi_gameplay_mode_add_player(player* pp)
{
    const bool render = Multi_match_info.match_state != MGMS_PREGAME && Multi_match_info.match_state != MGMS_COUNTDOWN;
    object_set_visibility_plus_children(pp, render);
    if (Multi_gameplay_mode_funcs[Multi_gameplay_mode].add_player != nullptr)
    {
        Multi_gameplay_mode_funcs[Multi_gameplay_mode].add_player(pp);
    }
}

// 0x009BC330 (:748)
void multi_gameplay_mode_remove_player(player* pp)
{
    multi_game_mode_remove_player_common(pp);
    if (Multi_gameplay_mode_funcs[Multi_gameplay_mode].remove_player != nullptr)
    {
        Multi_gameplay_mode_funcs[Multi_gameplay_mode].remove_player(pp);
    }
}

// 0x009F0070 (:769): the common part may change the arguments (they go by reference)
void multi_gameplay_mode_player_died(player* victim, player* killer, damaged_by_types damage_type, const weapon_info* w_info)
{
    multi_game_mode_player_died_common(victim, killer, damage_type, w_info);
    if (Multi_gameplay_mode_funcs[Multi_gameplay_mode].player_died != nullptr)
    {
        Multi_gameplay_mode_funcs[Multi_gameplay_mode].player_died(victim, killer, damage_type, w_info);
    }
}

// 0x009F80F0 (:787)
void multi_gameplay_mode_player_respawn(player* pp, int player_start_index)
{
    multi_game_mode_player_respawn_common(pp, player_start_index);
    if (Multi_gameplay_mode_funcs[Multi_gameplay_mode].player_respawn != nullptr)
    {
        Multi_gameplay_mode_funcs[Multi_gameplay_mode].player_respawn(pp, player_start_index);
    }
}

// 0x0094EA20 (:805)
float multi_gameplay_mode_adjust_respawn_weight(const player* pp, const player_start* pstart)
{
    float weight = 0.0f;
    if (Multi_gameplay_mode_funcs[Multi_gameplay_mode].adjust_respawn_weight != nullptr)
    {
        weight += Multi_gameplay_mode_funcs[Multi_gameplay_mode].adjust_respawn_weight(pp, pstart);
    }
    return weight;
}

// 0x009DC5C0 (:826): the damage or repair counts for the last player who did it
void multi_gameplay_mode_building_damage_changed(rfg_mover* building, float structural_mass_delta)
{
    player* pp;
    if (structural_mass_delta <= 0.0f)
    {
        pp = multi_game_last_player_to_damage_rfg_mover(building);
    }
    else
    {
        pp = multi_game_last_player_to_repair_rfg_mover(building);
    }
    if (pp != nullptr)
    {
        // Magic number: the stats count structural mass in units of 4,000.
        const float delta = structural_mass_delta * 0.00025f;
        multi_player_stats* stats = multi_game_stats_get_player_stats(pp->mp_data);
        if (stats != nullptr)
        {
            if (delta >= 0.0f)
            {
                stats->total_repair += delta;
            }
            else
            {
                stats->total_damage -= delta;
                stats->current_damage -= delta;
                if (stats->current_damage - stats->highest_damage >= 0.0f)
                {
                    stats->highest_damage = stats->current_damage;
                }
            }
        }
    }
    if (Multi_gameplay_mode_funcs[Multi_gameplay_mode].building_damage_changed != nullptr)
    {
        Multi_gameplay_mode_funcs[Multi_gameplay_mode].building_damage_changed(pp, building, structural_mass_delta);
    }
}

// 0x0094EA70 (:847)
bool multi_gameplay_mode_can_item_respawn(const item* itemp, bool& force_respawn)
{
    force_respawn = false;
    if (Multi_gameplay_mode_funcs[Multi_gameplay_mode].can_item_respawn != nullptr)
    {
        return Multi_gameplay_mode_funcs[Multi_gameplay_mode].can_item_respawn(itemp, force_respawn);
    }
    return true;
}

// 0x0094EAA0 (:861)
bool multi_gameplay_mode_item_touched(const player* pp, const item* itemp)
{
    if (Multi_gameplay_mode_funcs[Multi_gameplay_mode].item_touched != nullptr)
    {
        return Multi_gameplay_mode_funcs[Multi_gameplay_mode].item_touched(pp, itemp);
    }
    return true;
}

// 0x0094EAC0 (:874)
bool multi_gameplay_mode_player_set_action_object(const player* pp, t_usable_object& action_object)
{
    if (Multi_gameplay_mode_funcs[Multi_gameplay_mode].player_set_action_object != nullptr)
    {
        return Multi_gameplay_mode_funcs[Multi_gameplay_mode].player_set_action_object(pp, action_object);
    }
    return false;
}

// 0x0094EAE0 (:887)
bool multi_gameplay_mode_player_can_swap_weapons(const player* pp)
{
    if (Multi_gameplay_mode_funcs[Multi_gameplay_mode].player_can_swap_weapons != nullptr)
    {
        return Multi_gameplay_mode_funcs[Multi_gameplay_mode].player_can_swap_weapons(pp);
    }
    return true;
}

// 0x0094EB00 (:904)
bool multi_gameplay_mode_player_can_attack(const player* pp, const weapon_info* w_info, damaged_by_types attack_type)
{
    if (Multi_gameplay_mode_funcs[Multi_gameplay_mode].player_can_attack != nullptr)
    {
        return Multi_gameplay_mode_funcs[Multi_gameplay_mode].player_can_attack(pp, w_info, attack_type);
    }
    return true;
}

// 0x0094EB20 (:921): the server options' loadout, then the mode's changes
void multi_gameplay_mode_get_default_equipment(const player* pp, multi_game_equipment_loadout* loadout)
{
    loadout->replace_starting_weapon = Mp_server_options.generic_options.replace_starting_weapon;
    loadout->starting_weapon = Mp_server_options.generic_options.starting_weapon;
    loadout->third_weapon = Mp_server_options.generic_options.third_weapon;
    loadout->fourth_weapon = Mp_server_options.generic_options.fourth_weapon;
    if (Mp_server_options.generic_options.starting_backpack == MGSBO_RANDOM)
    {
        loadout->starting_backpack_type = pp->mp_respawn_backpack_type;
    }
    else
    {
        loadout->starting_backpack_type = Mp_server_options.generic_options.starting_backpack_type;
    }
    if (Multi_gameplay_mode_funcs[Multi_gameplay_mode].get_default_equipment != nullptr)
    {
        Multi_gameplay_mode_funcs[Multi_gameplay_mode].get_default_equipment(pp, loadout);
    }
}

// 0x0094EB90 (:936)
void multi_gameplay_mode_process_mode_specific(unsigned char* packet_data, const sender& from)
{
    if (Multi_gameplay_mode_funcs[Multi_gameplay_mode].process_mode_specific_packet != nullptr)
    {
        Multi_gameplay_mode_funcs[Multi_gameplay_mode].process_mode_specific_packet(packet_data, from);
    }
}

// 0x00978FC0 (:947)
void multi_gameplay_mode_countdown_begin()
{
    if (Multi_gameplay_mode_funcs[Multi_gameplay_mode].countdown_begin != nullptr)
    {
        Multi_gameplay_mode_funcs[Multi_gameplay_mode].countdown_begin();
    }
}

// 0x009F00B0 (:960)
void multi_gameplay_mode_round_begin()
{
    multi_game_mode_round_begin_common();
    if (Multi_gameplay_mode_funcs[Multi_gameplay_mode].round_begin != nullptr)
    {
        Multi_gameplay_mode_funcs[Multi_gameplay_mode].round_begin();
    }
    console_change_chat_window_pos(VCP_INGAME);
}

// 0x009FD3C0 (:976)
void multi_gameplay_mode_round_over(multi_round_winner winner)
{
    if (Multi_gameplay_mode_funcs[Multi_gameplay_mode].round_over != nullptr)
    {
        Multi_gameplay_mode_funcs[Multi_gameplay_mode].round_over(winner);
    }
    multi_game_mode_round_over_common(winner);
}

// 0x00978FE0 (:991): the match timer unless the mode keeps its own time
int multi_gameplay_mode_get_time_ms()
{
    if (Multi_gameplay_mode_funcs[Multi_gameplay_mode].get_time_ms != nullptr)
    {
        return Multi_gameplay_mode_funcs[Multi_gameplay_mode].get_time_ms();
    }
    return Timer_time.time_until();
}

// 0x009ABAB0 (:1007)
bool multi_gameplay_mode_is_round_over()
{
    const bool common_over = multi_game_mode_is_round_over_common();
    if (Multiplayer && Server && Multi_gameplay_mode_funcs[Multi_gameplay_mode].is_round_over != nullptr &&
        Multi_gameplay_mode_funcs[Multi_gameplay_mode].is_round_over())
    {
        return true;
    }
    return common_over;
}

// 0x009ABAF0 (:1028): the common rules (a forfeit) override the mode's winner
multi_round_winner multi_gameplay_mode_get_round_winner()
{
    multi_round_winner winner;
    winner.player_game_data_handle = 0xFFFFFFFF;
    if (Multi_gameplay_mode_funcs[Multi_gameplay_mode].get_round_winner == nullptr)
    {
        keen::debug::nativePrintArgs("All gameplay modes are required to have a get_round_winner callback.");
    }
    else
    {
        winner = Multi_gameplay_mode_funcs[Multi_gameplay_mode].get_round_winner();
    }
    multi_round_winner common_winner;
    if (multi_game_mode_get_round_winner_common(&common_winner))
    {
        winner = common_winner;
    }
    return winner;
}

// 0x009F00E0 (:1051)
void multi_gameplay_mode_server_migration_pre()
{
    multi_game_mode_server_migration_pre_common();
    if (Multi_gameplay_mode_funcs[Multi_gameplay_mode].server_migration_pre != nullptr)
    {
        Multi_gameplay_mode_funcs[Multi_gameplay_mode].server_migration_pre();
    }
}

// 0x009ABB50 (:1064)
void multi_gameplay_mode_server_migration_post()
{
    multi_game_mode_server_migration_post_common();
    if (Multi_gameplay_mode_funcs[Multi_gameplay_mode].server_migration_post != nullptr)
    {
        Multi_gameplay_mode_funcs[Multi_gameplay_mode].server_migration_post();
    }
}

// 0x009F0100 (:1077)
void multi_gameplay_mode_server_migration_update_clients()
{
    multi_game_mode_server_migration_update_clients_common();
    if (Multi_gameplay_mode_funcs[Multi_gameplay_mode].server_migration_update_clients != nullptr)
    {
        Multi_gameplay_mode_funcs[Multi_gameplay_mode].server_migration_update_clients();
    }
}

// 0x0094EBB0 (:1092)
bool multi_gameplay_mode_is_team_mode(multi_gameplay_mode mode)
{
    if (mode < MGPM_NUM_MODES)
    {
        return Multi_gameplay_mode_funcs[mode].is_team_mode;
    }
    return false;
}

// 0x00979000 (:1098)
void multi_gameplay_mode_get_decorated_name(char* buffer)
{
    if (Multi_gameplay_mode >= MGPM_NUM_MODES)
    {
        return;
    }
    if (Multi_gameplay_mode_funcs[Multi_gameplay_mode].get_decorated_name != nullptr)
    {
        Multi_gameplay_mode_funcs[Multi_gameplay_mode].get_decorated_name(buffer);
        return;
    }
    // Magic number: the callers' buffers hold 64 characters.
    strncpy(buffer, Multi_gameplay_mode_names[Multi_gameplay_mode].display_name, 64);
}

// 0x0094EBD0 (:1110): rfg.exe inlines every mode's objective (a localization tag) here
void multi_gameplay_mode_get_objective_description(char* buffer)
{
    if (Multi_gameplay_mode >= MGPM_NUM_MODES)
    {
        return;
    }
    buffer[0] = '\0';
    if (Multi_gameplay_mode_funcs[Multi_gameplay_mode].get_objective_description != nullptr)
    {
        Multi_gameplay_mode_funcs[Multi_gameplay_mode].get_objective_description(buffer);
    }
}

// 0x0094EC00 (:1121)
const char* multi_gameplay_mode_get_hud_data_item_name()
{
    if (Multi_gameplay_mode < MGPM_NUM_MODES)
    {
        return Multi_gameplay_mode_funcs[Multi_gameplay_mode].hud_data_item_name;
    }
    return nullptr;
}

// 0x00979040 (:1127): the mode's info text is shown until its stat says the mode was played
bool multi_gameplay_mode_hud_init(vint_data_item* item)
{
    if (Multi_gameplay_mode >= MGPM_NUM_MODES)
    {
        return false;
    }
    // Magic number: the per-mode "played" stats start at stat 0x152 (multi_gameplay_mode_get is MGPM_NONE outside
    // multiplayer).
    if (stats_get_stat_value((tracked_stats_indices)(0x152 + multi_gameplay_mode_get())) <= 0)
    {
        hud_ui_multiplayer_info_text_enter();
    }
    if (Multi_gameplay_mode_funcs[Multi_gameplay_mode].hud_init != nullptr)
    {
        Multi_gameplay_mode_funcs[Multi_gameplay_mode].hud_init(item);
        return true;
    }
    return false;
}

// 0x0094EC20 (:1141)
bool multi_gameplay_mode_hud_process(vint_data_item* item)
{
    if (Multi_gameplay_mode < MGPM_NUM_MODES && Multi_gameplay_mode_funcs[Multi_gameplay_mode].hud_process != nullptr)
    {
        Multi_gameplay_mode_funcs[Multi_gameplay_mode].hud_process(item);
        return true;
    }
    return false;
}

// 0x0094EC50 (:1152)
bool multi_gameplay_mode_alter_player_icon(const player* pp, hud_icon* icon)
{
    if (Multi_gameplay_mode < MGPM_NUM_MODES && Multi_gameplay_mode_funcs[Multi_gameplay_mode].alter_player_icon != nullptr)
    {
        return Multi_gameplay_mode_funcs[Multi_gameplay_mode].alter_player_icon(pp, icon);
    }
    return false;
}

// 0x0094EC70 (:1163)
multi_announcer_lines multi_gameplay_mode_get_intro_line()
{
    return Multi_gameplay_mode_funcs[Multi_gameplay_mode].intro_line;
}

// 0x0094EC90 (:1177)
multi_gameplay_mode multi_gameplay_mode_get()
{
    if (!Multiplayer)
    {
        return MGPM_NONE;
    }
    return Multi_gameplay_mode;
}

// 0x009F0120 (:1199). LTCG: mode in ESI.
void multi_gameplay_mode_set(multi_gameplay_mode mode)
{
    multi_gameplay_mode_change(mode);
}

// 0x009F0130 (:1212)
void multi_gameplay_mode_system_init(mempool_base& pool)
{
    multi_gameplay_mode_change(MGPM_NONE);
    for (int i = 0; i < MGPM_NUM_MODES; i++)
    {
        if (Multi_gameplay_mode_funcs[i].memory_init != nullptr)
        {
            Multi_gameplay_mode_funcs[i].memory_init(pool);
        }
    }
}

// 0x0094ECB0 (:1226)
void multi_gameplay_mode_system_process()
{
}

// 0x009F01A0 (:1238)
void multi_gameplay_mode_system_shutdown()
{
    for (int i = 0; i < MGPM_NUM_MODES; i++)
    {
        if (Multi_gameplay_mode_funcs[i].memory_shutdown != nullptr)
        {
            Multi_gameplay_mode_funcs[i].memory_shutdown();
        }
    }
    multi_gameplay_mode_change(MGPM_NONE);
}

// 0x0094ECC0 (:1252)
void multi_gameplay_mode_append_mode_specific_stats(unsigned char* packet_data, unsigned short& packet_size, unsigned int player_game_data_handle)
{
    if (Multi_gameplay_mode_funcs[Multi_gameplay_mode].append_mode_specific_stats != nullptr)
    {
        Multi_gameplay_mode_funcs[Multi_gameplay_mode].append_mode_specific_stats(packet_data, packet_size, player_game_data_handle);
    }
}

// 0x0094ECE0 (:1259)
void multi_gameplay_mode_read_mode_specific_stats(unsigned char* packet_data, int& packet_offset, unsigned int player_game_data_handle)
{
    if (Multi_gameplay_mode_funcs[Multi_gameplay_mode].read_mode_specific_stats != nullptr)
    {
        Multi_gameplay_mode_funcs[Multi_gameplay_mode].read_mode_specific_stats(packet_data, packet_offset, player_game_data_handle);
    }
}

// 0x0094ED00 (:1266)
int multi_gameplay_mode_get_mode_specific_stat_count()
{
    if (Multi_gameplay_mode_funcs[Multi_gameplay_mode].get_mode_specific_stat_count != nullptr)
    {
        return Multi_gameplay_mode_funcs[Multi_gameplay_mode].get_mode_specific_stat_count();
    }
    return 0;
}

// 0x0094ED20 (:1274)
void multi_gameplay_mode_get_mode_specific_stat_name(char* name, const int max_name_len, int stat_index)
{
    if (stat_index < multi_gameplay_mode_get_mode_specific_stat_count() &&
        Multi_gameplay_mode_funcs[Multi_gameplay_mode].get_mode_specific_stat_name != nullptr)
    {
        Multi_gameplay_mode_funcs[Multi_gameplay_mode].get_mode_specific_stat_name(name, max_name_len, stat_index);
        return;
    }
    name[0] = '\0';
}

// 0x0094ED70 (:1283)
void multi_gameplay_mode_get_mode_specific_stat_value(char* value, const int max_value_len, int stat_index, unsigned int player_game_data_handle)
{
    if (stat_index < multi_gameplay_mode_get_mode_specific_stat_count() &&
        Multi_gameplay_mode_funcs[Multi_gameplay_mode].get_mode_specific_stat_value != nullptr)
    {
        Multi_gameplay_mode_funcs[Multi_gameplay_mode].get_mode_specific_stat_value(value, max_value_len, stat_index, player_game_data_handle);
        return;
    }
    value[0] = '\0';
}
