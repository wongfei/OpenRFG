// Original: volition/rfg/code/gameplay/wrecking_crew/wrecking_crew.cpp
//
// wrecking.md WC1: the Wrecking Crew core. wrecking_crew_init reads the challenge, map, weapon, backpack and character
// tables (base and DLC); a match (wrecking_crew_match_begin) loads the map as a single zone territory and runs the state
// machine in wrecking_crew_process: rounds of turns, one per player, each followed by a break with the score and a map
// restore. The mode table Wrecking_crew_mode_info holds every mode's callbacks (the five wrecking_crew_mode_*.cpp files).

#include "volition/rfg/code/gameplay/wrecking_crew/wrecking_crew.h"

#include "volition/rfg/code/camera/camera_third_person.h"
#include "volition/rfg/code/camera/camera_view.h"
#include "volition/rfg/code/camera/rfg_camera.h"
#include "volition/rfg/code/code_effects/explosion.h"
#include "volition/rfg/code/destruction/rfg_stress.h"
#include "volition/rfg/code/event_logging/definitions_events/events.h"
#include "volition/rfg/code/event_logging/gamelog.h"
#include "volition/rfg/code/gameplay/gameplay.h"
#include "volition/rfg/code/gameplay/wrecking_crew/wrecking_crew_internal.h"
#include "volition/rfg/code/gameplay/wrecking_crew/wrecking_crew_mode_fall_guy.h"
#include "volition/rfg/code/gameplay/wrecking_crew/wrecking_crew_mode_mayhem.h"
#include "volition/rfg/code/gameplay/wrecking_crew/wrecking_crew_mode_rampage.h"
#include "volition/rfg/code/gameplay/wrecking_crew/wrecking_crew_mode_total_chaos.h"
#include "volition/rfg/code/gameplay/wrecking_crew/wrecking_crew_mode_walker.h"
#include "volition/rfg/code/gamewide/downloadable_content.h"
#include "volition/rfg/code/gamewide/game_clock.h"
#include "volition/rfg/code/gamewide/game_options.h"
#include "volition/rfg/code/gamewide/game_render.h"
#include "volition/rfg/code/gamewide/gamemusic.h"
#include "volition/rfg/code/gamewide/stats.h"
#include "volition/rfg/code/havok/havok_ragdoll.h"
#include "volition/rfg/code/hud/hud_icon.h"
#include "volition/rfg/code/interface/loading.h"
#include "volition/rfg/code/interface/localization/localization_shared.h"
#include "volition/rfg/code/interface/option_menus/controller_options.h"
#include "volition/rfg/code/interface/option_menus/option_menu_system.h"
#include "volition/rfg/code/interface/ui_document_hud_tool_tips.h"
#include "volition/rfg/code/interface/ui_document_hud_wrecking_crew.h"
#include "volition/rfg/code/interface/ui_document_manager.h"
#include "volition/rfg/code/interface/ui_document_menu_wrecking_crew.h"
#include "volition/rfg/code/interface/ui_document_menu_wrecking_crew_character_select.h"
#include "volition/rfg/code/interface/ui_document_menu_wrecking_crew_scoreboard.h"
#include "volition/rfg/code/interface/ui_menu.h"
#include "volition/rfg/code/interface/ui_message_box.h"
#include "volition/rfg/code/level/world.h"
#include "volition/rfg/code/multi/multi_core/include/multi_core_platform.h"
#include "volition/rfg/code/multi/multi_core/include/multi_player_uid.h"
#include "volition/rfg/code/multi/multi_game/multi_game.h"
#include "volition/rfg/code/multi/multi_game/multi_game_camera.h"
#include "volition/rfg/code/multi/multi_game/multi_game_object_backpack.h"
#include "volition/rfg/code/multi/multi_game/multi_game_object_marker.h"
#include "volition/rfg/code/multi/multi_game/multi_game_settings.h"
#include "volition/rfg/code/objects/humans/human.h"
#include "volition/rfg/code/objects/humans/human_info.h"
#include "volition/rfg/code/objects/inventory_item.h"
#include "volition/rfg/code/objects/humans/human_teams.h"
#include "volition/rfg/code/objects/humans/human_weapons.h"
#include "volition/rfg/code/objects/humans/player.h"
#include "volition/rfg/code/objects/movers/general_mover.h"
#include "volition/rfg/code/objects/movers/object_mover.h"
#include "volition/rfg/code/objects/object.h"
#include "volition/rfg/code/objects/object_player_start.h"
#include "volition/rfg/code/objects/projectile.h"
#include "volition/rfg/code/rfg.h"
#include "volition/rfg/code/scripting/objective_highlight.h"
#include "volition/rfg/code/vehicle/vehicle_class.h"
#include "volition/rfg/code/vehicle/vehicle_system.h"
#include "volition/rfg/code/weapons/weapons.h"
#include "volition/vlib/code/cfile/cfile.h"
#include "volition/vlib/code/math/matrix.h"
#include "volition/vlib/code/math/rand.h"
#include "volition/vlib/code/memory/object_pool.h"
#include "volition/vlib/code/memory/rfg_split_mempool.h"
#include "volition/vlib/code/memory/split_memmgr.h"
#include "volition/vlib/code/os/timer.h"
#include "volition/vlib/code/os/vconsole.h"
#include "volition/vlib/code/parse/xml_table_tools.h"
#include "volition/vlib/code/util/fastxml.h"
#include "volition/vlib/code/util/string_pool.h"
#include "volition/vlib/code/util/unicode_tools.h"

#include <keen/base/trace.hpp>
#include <unit4/gamesession/localgamesession.hpp>

#include <stdio.h>
#include <string.h>
#include <wchar.h>

extern float Rl_sun_shafts_scale; // 0x014FA290 (rl_hdr_compositor.cpp)

// ---- the mode table (initialized data; info_text and games_played_overall_stat are filled by wrecking_crew_init)
// Magic number: 0x100 at +0x4C in every entry is skip_round_begin false, display_round_win_message true.
wrecking_crew_mode_info Wrecking_crew_mode_info[5] = { // 0x016515B8
    {"mayhem", "HM_WC_MODE_ESCALATION", wrecking_crew_mode_mayhem_match_begin, nullptr, wrecking_crew_mode_mayhem_round_begin,
     wrecking_crew_mode_mayhem_round_end, wrecking_crew_mode_mayhem_players_turn_begin, wrecking_crew_mode_mayhem_players_turn_end,
     wrecking_crew_mode_mayhem_is_players_turn_over, nullptr, wrecking_crew_mode_mayhem_can_player_attack,
     wrecking_crew_mode_mayhem_player_attacked, nullptr, nullptr, wrecking_crew_mode_mayhem_gameplay_process,
     wrecking_crew_mode_mayhem_render, nullptr, nullptr, wrecking_crew_mode_mayhem_get_weapon_ammo, false, true, nullptr, nullptr,
     wrecking_crew_mode_mayhem_get_weapon_text, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr,
     wrecking_crew_mode_mayhem_get_weapon_icon_state, nullptr, wrecking_crew_mode_mayhem_get_time_icon_state, (tracked_stats_indices)0,
     {nullptr, nullptr, nullptr}},
    {"fall_guy", "HM_WC_MODE_BARREL_DASH", wrecking_crew_mode_fall_guy_match_begin, nullptr, wrecking_crew_mode_fall_guy_round_begin,
     wrecking_crew_mode_fall_guy_round_end, wrecking_crew_mode_fall_guy_players_turn_begin, wrecking_crew_mode_fall_guy_players_turn_end,
     wrecking_crew_mode_fall_guy_is_players_turn_over, wrecking_crew_mode_fall_guy_get_current_players_score, nullptr, nullptr,
     nullptr, nullptr, wrecking_crew_mode_fall_guy_gameplay_process, wrecking_crew_mode_fall_guy_render,
     wrecking_crew_mode_fall_guy_map_is_being_restored, wrecking_crew_mode_fall_guy_mover_destroyed, nullptr, false, true, nullptr,
     wrecking_crew_mode_fall_guy_get_score_text, nullptr, nullptr, wrecking_crew_mode_fall_guy_get_turn_end_score_text, nullptr,
     nullptr, wrecking_crew_mode_fall_guy_get_barrel_text, nullptr, nullptr, nullptr, wrecking_crew_mode_fall_guy_get_time_icon_state,
     (tracked_stats_indices)0, {nullptr, nullptr, nullptr}},
    {"rampage", "HM_WC_MODE_RAMPAGE", wrecking_crew_mode_rampage_match_begin, nullptr, wrecking_crew_mode_rampage_round_begin,
     wrecking_crew_mode_rampage_round_end, wrecking_crew_mode_rampage_players_turn_begin, wrecking_crew_mode_rampage_players_turn_end,
     nullptr, nullptr, wrecking_crew_mode_rampage_can_player_attack, wrecking_crew_mode_rampage_player_attacked,
     wrecking_crew_mode_rampage_player_activated_backpack, nullptr, wrecking_crew_mode_rampage_gameplay_process,
     wrecking_crew_mode_rampage_render, nullptr, nullptr, nullptr, false, true, nullptr, nullptr, wrecking_crew_mode_rampage_get_weapon_text,
     wrecking_crew_mode_rampage_get_backpack_text, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr,
     (tracked_stats_indices)0, {nullptr, nullptr, nullptr}},
    {"total_chaos", "HM_WC_MODE_TOTAL_CHAOS", wrecking_crew_mode_total_chaos_match_begin, nullptr,
     wrecking_crew_mode_total_chaos_round_begin, wrecking_crew_mode_total_chaos_round_end,
     wrecking_crew_mode_total_chaos_players_turn_begin, wrecking_crew_mode_total_chaos_players_turn_end, nullptr, nullptr, nullptr,
     nullptr, nullptr, nullptr, wrecking_crew_mode_total_chaos_gameplay_process, wrecking_crew_mode_total_chaos_render, nullptr,
     nullptr, nullptr, false, true, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr,
     wrecking_crew_mode_total_chaos_get_time_icon_state, (tracked_stats_indices)0, {nullptr, nullptr, nullptr}},
    {"walker", "HM_WC_MODE_WALKER", wrecking_crew_mode_walker_match_begin, wrecking_crew_mode_walker_match_end,
     wrecking_crew_mode_walker_round_begin, wrecking_crew_mode_walker_round_end, wrecking_crew_mode_walker_players_turn_begin,
     wrecking_crew_mode_walker_players_turn_end, nullptr, nullptr, nullptr, nullptr, nullptr,
     wrecking_crew_mode_walker_player_respawned, wrecking_crew_mode_walker_gameplay_process, wrecking_crew_mode_walker_render,
     nullptr, nullptr, nullptr, false, true, nullptr, nullptr, wrecking_crew_mode_walker_get_weapon_text,
     wrecking_crew_mode_walker_get_backpack_text, nullptr, nullptr, nullptr, nullptr, wrecking_crew_mode_walker_get_veto_text, nullptr,
     nullptr, wrecking_crew_mode_walker_get_time_icon_state, (tracked_stats_indices)0, {nullptr, nullptr, nullptr}},
};

// ---- globals
static bool Wrecking_crew_unlimited_reserve_ammo = true; // 0x01651419
static float Wrecking_crew_death_camera_speed = 0.05f; // 0x016515B4: radians per frame the death camera turns
wrecking_crew_states Wrecking_crew_state = WCS_NONE; // 0x01651888
wrecking_crew_walker_info Wrecking_crew_walkers[3] = { // 0x0165188C: vehicle_type filled by wrecking_crew_init
    {"MENU_GH_HEAVY_WALKER", "Walker_Civilian", -1},
    {"MENU_GH_LIGHT_WALKER", "Walker_Min", -1},
    {"MENU_GH_FIRE_WALKER", "Walker_Flamer", -1},
};
static int Wrecking_crew_round_winner = -1; // 0x016518B0
static int Wrecking_crew_match_winner = -1; // 0x016518B4
static int Wrecking_crew_first_player = -1; // 0x016518B8: the player who starts the next round (last round's winner)
static int Wrecking_crew_match_start_time_seconds = -1; // 0x016518BC
static int Wrecking_crew_input_id = -1; // 0x016518C0
static wrecking_crew_states Wrecking_crew_restore_map_state = WCS_NONE; // 0x016518C4: the state after the map restore
static float Sunshafts_scale_saved = 1.0f; // 0x016518C8
// The defaults the menus start from: Total Chaos, one player, the first map, random weapon, backpack and walker, low
// stress, two round wins, no barrels, no challenge.
wrecking_crew_options Wrecking_crew_options = {WCM_TOTAL_CHAOS, 1, 0, -1, -1, -1, WCWC_NONE, WCSS_LOW, 2, false, false, -1}; // 0x016518CC
int Wrecking_crew_current_player = -1; // 0x016518F8
int Wrecking_crew_current_round = -1; // 0x016518FC
int Wrecking_crew_last_first_player = -1; // 0x01651900
static dynamic_object_pool<wc_game_character_info, 1> Wrecking_crew_character_pool; // 0x01659970
static dynamic_object_pool<wrecking_crew_map_info, 1> Wrecking_crew_map_pool; // 0x016599B0
// Magic number 256: the movers of the map, restored (or, for barrels with barrels off, destroyed) between turns.
static farray<unsigned int, 256> Wrecking_crew_mover_handles; // 0x016599F0
static farray<bool, 256> Wrecking_crew_mover_barrel_flag; // 0x01659E00
static timestamp Wrecking_crew_respawn_timer; // 0x01659F10
static timestamp_percent Wrecking_crew_respawn_blur_timer; // 0x01659F14
static timestamp_percent Wrecking_crew_splash_blur_timer; // 0x01659F1C
static timestamp Wrecking_crew_turn_move_on_timer; // 0x01659F24
static timestamp Wrecking_crew_restore_map_delay; // 0x01659F28
timestamp_percent Wrecking_crew_timer; // 0x01659F2C
farray<wc_game_character_info*, 50> Wrecking_crew_characters; // 0x01659F38
static int Num_challenge_modes; // 0x02225818
static wrecking_crew_map_info* Wrecking_crew_map_info[32]; // 0x02225820
static player* Wrecking_crew_players[4]; // 0x02226BF8
static control_config Wrecking_crew_main_player_ctrl_config_backup; // 0x02226C08
static float Challenge_player_round_scores[4][3]; // 0x022277C0: per player and challenge round
static wrecking_crew_backpack_info Wrecking_crew_backpacks[16]; // 0x022277F0
wrecking_crew_player_status Wrecking_crew_player_status[4]; // 0x022278B8
static wrecking_crew_weapon_info Wrecking_crew_weapons[16]; // 0x02227948
static bool Wrecking_crew_unlimited_magazine_ammo; // 0x02229683
// Never written in rfg.exe, so the respawn blur adds no signal noise.
static float Wrecking_crew_camera_noise_scale; // 0x02229684
static float Wrecking_crew_camera_noise_frequency; // 0x02229688
static int Wrecking_crew_num_maps; // 0x0222968C
static int Wrecking_crew_num_weapons; // 0x02229690
const wrecking_crew_weapon_info* Wrecking_crew_current_weapon; // 0x02229694
static int Wrecking_crew_num_backpacks; // 0x02229698
const wrecking_crew_backpack_info* Wrecking_crew_current_backpack; // 0x0222969C
static int Wrecking_crew_veto_player; // 0x022296A0
int Wrecking_crew_turn_time_left; // 0x022296A4
static float Wrecking_crew_round_high_score; // 0x022296A8
static int Wrecking_crew_spawn_point; // 0x022296AC: player start index
static float Wrecking_crew_pre_turn_fov; // 0x022296B0
static bool Wrecking_crew_should_restart; // 0x022296B4
static bool Wrecking_crew_restore_map_requested; // 0x022296B5
static bool Wrecking_crew_restarted; // 0x022296B6
static bool Behemoth_enabled; // 0x022296B7
static short Wrecking_crew_match_id; // 0x022296B8
static bool Check_for_walker_load; // 0x022296BA
static bool g_unlockBehemoth; // 0x022296BB: never written in rfg.exe
bool Wrecking_crew_player_attacked_this_frame; // 0x022296BC
bool Wrecking_crew_player_activated_backpack_this_frame; // 0x022296BD
wc_player_info Wc_player_infos[4]; // 0x0222AD70
static controller_options Wrecking_crew_main_player_control_options_backup; // 0x02233070
static rfg_split_mempool Wrecking_crew_mempool; // 0x02246B00
static wrecking_crew_challenge_mode Challenge_modes[32]; // 0x02247650
static string_pool Wrecking_crew_string_pool(nullptr); // 0x0224CA10
static rfg_split_mempool Wrecking_crew_interface_mempool; // 0x02272E58

// 0x00767280
wrecking_crew_map_info::wrecking_crew_map_info()
    : file_name_crc(checksum_stri::INVALID)
{
}

// 0x00767270
wc_game_character_info::wc_game_character_info()
{
}

// 0x00761030
wc_player_info::wc_player_info()
{
}

// 0x0075F860
wrecking_crew_challenge_mode::wrecking_crew_challenge_mode()
    : name_crc(checksum_stri::INVALID)
{
}

static void wrecking_crew_change_state(wrecking_crew_states state);

// The current mode's table entry, or null (rfg.exe inlines this check everywhere).
static const wrecking_crew_mode_info* wc_current_mode()
{
    if ((unsigned int)Wrecking_crew_options.mode < NUM_WRECKING_CREW_MODES)
    {
        return &Wrecking_crew_mode_info[Wrecking_crew_options.mode];
    }
    return nullptr;
}

// Copy of the control config (rfg.exe copies the 3000 bytes with a rep movsd).
static void wc_copy_control_config(control_config& dst, const control_config& src)
{
    memcpy(&dst, &src, sizeof(control_config));
}

// ---- debug console commands (unused in retail)

// 0x0075F880 (:500)
void unused_dcf_wc_change_timer()
{
    if (console_get_arg(8, 0))
    {
        Wrecking_crew_timer.set(Console_arg_int);
    }
}

// 0x00790F40 (:507)
void unused_dcf_wc_lock_map()
{
    if (console_get_arg(2, 0))
    {
        wrecking_crew_lock_map(Console_arg);
    }
}

static wrecking_crew_map_info* wrecking_crew_get_map_info(const char* map_name);

// 0x007767E0 (:514)
void unused_dcf_wc_unlock_map()
{
    if (console_get_arg(2, 0))
    {
        wrecking_crew_map_info* info = wrecking_crew_get_map_info(Console_arg);
        if (info != nullptr)
        {
            info->locked = false;
        }
    }
}

// 0x0075F8B0 (:521)
void unused_dcf_wc_give_stat_points()
{
    if (console_get_arg(8, 0))
    {
        stats_increment(STATS_WRECKING_CREW_DESTRUCTION_DONE, &Console_arg_int);
    }
}

// 0x0075F8E0 (:539)
static void session_process()
{
}

// 0x0075F8F0 (:615)
static void session_create()
{
}

// 0x00776810 (:680): upload a finished challenge to its leaderboard: the best total and the best score of each round.
static void report_stats()
{
    const wrecking_crew_challenge_mode* challenge = wrecking_crew_get_challenge_mode_by_index(Wrecking_crew_options.current_challenge_id);
    float totals[4] = {0.0f, 0.0f, 0.0f, 0.0f};
    float best_round[3] = {0.0f, 0.0f, 0.0f};
    for (int p = 0; p < 4; p++)
    {
        for (int r = 0; r < 3; r++)
        {
            const float score = Challenge_player_round_scores[p][r];
            totals[p] = score + totals[p];
            if (best_round[r] - score < 0.0f)
            {
                best_round[r] = score;
            }
        }
    }
    float best_total = totals[0];
    for (int p = 1; p < 4; p++)
    {
        if (best_total - totals[p] < 0.0f)
        {
            best_total = totals[p];
        }
    }
    if (My_player_uid.is_valid())
    {
        scoreboard_column columns[4];
        columns[0].column_type = PSCT_INT;
        columns[0].column_value.i_val = wrecking_crew_convert_score(best_total);
        columns[1].column_type = PSCT_INT;
        columns[1].column_value.i_val = wrecking_crew_convert_score(best_round[0]);
        columns[2].column_type = PSCT_INT;
        columns[2].column_value.i_val = wrecking_crew_convert_score(best_round[1]);
        columns[3].column_type = PSCT_INT;
        columns[3].column_value.i_val = wrecking_crew_convert_score(best_round[2]);
        // Original: a challenge id outside the table reads leaderboard_id through null. Magic number 4: the
        // challenge leaderboards follow four other tables.
        multi_core_platform_scoreboard_begin_upload_score(My_player_uid, (unsigned short)(challenge->leaderboard_id + 4), columns, 4);
    }
}

// 0x0075F900 (:716)
static bool wrecking_crew_player_in_turn()
{
    return Wrecking_crew_state == WCS_PRE_TURN || Wrecking_crew_state == WCS_PLAYERS_TURN;
}

// 0x0075F920 (:721)
static const wrecking_crew_mode_info* wrecking_crew_get_mode_info(wrecking_crew_modes mode)
{
    if ((unsigned int)mode < NUM_WRECKING_CREW_MODES)
    {
        return &Wrecking_crew_mode_info[mode];
    }
    return nullptr;
}

// 0x0075F940 (:733)
static wrecking_crew_map_info* wrecking_crew_get_map_info(const char* map_name)
{
    for (int i = 0; i < Wrecking_crew_num_maps; i++)
    {
        if (v_stricmp(map_name, Wrecking_crew_map_info[i]->file_name) == 0)
        {
            return Wrecking_crew_map_info[i];
        }
    }
    return nullptr;
}

// 0x0075F980 (:757)
static int wrecking_crew_get_challenge_mode_index(const char* name)
{
    const checksum_stri name_crc(name, 0);
    for (int i = 0; i < Num_challenge_modes; i++)
    {
        if (Challenge_modes[i].name_crc == name_crc)
        {
            return i;
        }
    }
    return -1;
}

// 0x0075F9C0 (:786): wrecking_crew_challenges.xtbl. A challenge counts only with a leaderboard and exactly three valid
// rounds.
static void wrecking_crew_parse_challenges(xml_element* root)
{
    for (xml_element* element = root->elements; element != nullptr; element = element->next)
    {
        const char* name = xml_table_get_req_string_ref(element, "Name");
        if (name == nullptr)
        {
            continue;
        }
        wrecking_crew_challenge_mode& challenge = Challenge_modes[Num_challenge_modes];
        challenge.name_crc = checksum_stri(name, 0);
        if (!xml_table_get_opt_int(challenge.leaderboard_id, element, "leaderboard_id"))
        {
            continue;
        }
        xml_element* round_element = xml_table_find_req(element, "challenge_rounds")->elements;
        if (round_element == nullptr)
        {
            continue;
        }
        int num_rounds = 0;
        do
        {
            if (num_rounds >= 3)
            {
                break;
            }
            wrecking_crew_challenge_mode_round& round = challenge.rounds[num_rounds];
            const char* mode = xml_table_get_req_string_ref(round_element, "mode");
            bool mode_valid = true;
            if (v_stricmp(mode, "mayhem") == 0)
            {
                round.mode = WCM_MAYHEM;
            }
            else if (v_stricmp(mode, "fall_guy") == 0)
            {
                round.mode = WCM_FALL_GUY;
            }
            else if (v_stricmp(mode, "rampage") == 0)
            {
                round.mode = WCM_RAMPAGE;
            }
            else if (v_stricmp(mode, "total_chaos") == 0)
            {
                round.mode = WCM_TOTAL_CHAOS;
            }
            else if (v_stricmp(mode, "walker") == 0)
            {
                round.mode = WCM_WALKER;
            }
            else
            {
                mode_valid = false;
            }
            if (mode_valid)
            {
                round.winfo_handle = weapons_get_weapon_info_handle(xml_table_get_req_string_ref(round_element, "weapon"));
                if (round.winfo_handle != 0xFFFFFFFF)
                {
                    round.backpack_id = multi_game_backpack_get_backpack_id(xml_table_get_req_string_ref(round_element, "backpack"));
                    if (round.backpack_id != -1)
                    {
                        round.walker_choice = WCWC_CIVILIAN;
                        const char* walker_name = xml_table_get_req_string_ref(round_element, "walker");
                        bool walker_valid = true;
                        if (v_stricmp(walker_name, "none") == 0)
                        {
                            round.walker_choice = WCWC_NONE;
                        }
                        else if (v_stricmp(walker_name, "civilian") == 0)
                        {
                            round.walker_choice = WCWC_CIVILIAN;
                        }
                        else if (v_stricmp(walker_name, "mining") == 0)
                        {
                            round.walker_choice = WCWC_MINING;
                        }
                        else if (v_stricmp(walker_name, "flamer") == 0)
                        {
                            round.walker_choice = WCWC_FLAMER;
                        }
                        else
                        {
                            walker_valid = false;
                        }
                        if (walker_valid)
                        {
                            const char* stress = xml_table_get_req_string_ref(round_element, "stress_level");
                            bool stress_valid = true;
                            if (v_stricmp(stress, "Off") == 0)
                            {
                                round.stress_mode = WCSS_OFF;
                            }
                            else if (v_stricmp(stress, "Low") == 0)
                            {
                                round.stress_mode = WCSS_LOW;
                            }
                            else if (v_stricmp(stress, "High") == 0)
                            {
                                round.stress_mode = WCSS_HIGH;
                            }
                            else
                            {
                                stress_valid = false;
                            }
                            if (stress_valid && xml_table_get_opt_bool(round.barrels, round_element, "barrels"))
                            {
                                num_rounds++;
                            }
                        }
                    }
                }
            }
            round_element = round_element->next;
        } while (round_element != nullptr);
        if (num_rounds == 3)
        {
            Num_challenge_modes++;
        }
    }
}

// 0x00776A80 (:870)
static void wrecking_crew_init_challenges(int dlc_id)
{
    xml_element* root;
    if (dlc_id == -1)
    {
        root = xml_table_parse("wrecking_crew_challenges.xtbl");
    }
    else
    {
        char name[256];
        sprintf(name, "dlc%02d_%s", dlc_id, "wrecking_crew_challenges.xtbl");
        if (!cf_exist(name))
        {
            return;
        }
        root = xml_table_parse(name);
    }
    if (root == nullptr)
    {
        return;
    }
    wrecking_crew_parse_challenges(root);
    xml_table_close(root);
}

// 0x0079E5B0 (:880): wrecking_crew.xtbl: maps, weapons, backpacks, the modes' info texts and stats, the characters, the
// walker starts that are locked and the behemoth unlock.
static void wrecking_crew_parse_wrecking_crew(xml_element* root, int dlc_id)
{
    if (root == nullptr)
    {
        return;
    }
    xml_element* entry = xml_table_find_req(root, "entry");
    if (entry == nullptr)
    {
        return;
    }
    xml_element* maps = xml_table_find_req(entry, "maps");
    if (maps == nullptr)
    {
        return;
    }
    char buffer[256];
    for (xml_element* map = xml_find(maps, "map"); map != nullptr && Wrecking_crew_num_maps < 32; map = xml_find_next(maps, map, "map"))
    {
        wrecking_crew_map_info* info = Wrecking_crew_map_pool.alloc(0);
        if (info == nullptr)
        {
            break;
        }
        info->dlc_id = dlc_id;
        info->locked = false;
        info->file_name = nullptr;
        info->file_name_crc = checksum_stri::INVALID;
        const char* value = xml_table_get_req_string_ref(map, "file_name");
        if (value != nullptr)
        {
            info->file_name = Wrecking_crew_string_pool.add_unique(value);
            info->file_name_crc = checksum_stri(info->file_name, 0);
        }
        info->display_name = nullptr;
        value = xml_table_get_req_string_ref(map, "display_name");
        if (value != nullptr)
        {
            info->display_name = Wrecking_crew_string_pool.add_unique(value);
        }
        info->img_name = nullptr;
        value = xml_table_get_req_string_ref(map, "image_name");
        if (value == nullptr)
        {
            keen::debug::nativePrintArgs("Wrecking crew maps should have an image");
        }
        else
        {
            info->img_name = Wrecking_crew_string_pool.add_unique(value);
        }
        info->load_img_name = nullptr;
        value = xml_table_get_req_string_ref(map, "Load_image");
        if (value == nullptr)
        {
            keen::debug::nativePrintArgs("Wrecking crew maps should have a loading screen image");
        }
        else
        {
            info->load_img_name = Wrecking_crew_string_pool.add_unique(value);
        }
        value = xml_table_get_req_string_ref(map, "time_of_day");
        if (value != nullptr)
        {
            multi_game_settings_parse_time_of_day(&info->time_of_day_hour, &info->time_of_day_minute, value);
        }
        value = xml_table_get_opt_string_ref(map, "Challenge_Set");
        info->challenge_id = value == nullptr ? -1 : wrecking_crew_get_challenge_mode_index(value);
        info->games_played_overall_stat = stats_get_by_name(xml_table_get_req_string_ref(map, "games_played_overall_stat"));
        for (int m = 0; m < NUM_WRECKING_CREW_MODES; m++)
        {
            sprintf(buffer, "games_played_%s_stat", Wrecking_crew_mode_info[m].internal_name);
            info->games_played_mode_stats[m] = stats_get_by_name(xml_table_get_req_string_ref(map, buffer));
        }
        Wrecking_crew_map_info[Wrecking_crew_num_maps] = info;
        Wrecking_crew_num_maps++;
    }

    const char* mode_names[NUM_WRECKING_CREW_MODES];
    for (int m = 0; m < NUM_WRECKING_CREW_MODES; m++)
    {
        mode_names[m] = Wrecking_crew_mode_info[m].internal_name;
    }
    xml_element* weapons = xml_table_find_opt(entry, "weapons");
    if (weapons != nullptr)
    {
        // Magic number 16: Wrecking_crew_weapons.
        for (xml_element* element = xml_find(weapons, "weapon"); element != nullptr && Wrecking_crew_num_weapons < 16; element = xml_find_next(weapons, element, "weapon"))
        {
            const unsigned int handle = weapons_get_weapon_info_handle(xml_table_get_req_string_ref(element, "weapon_name"));
            if (handle == 0xFFFFFFFF)
            {
                continue;
            }
            wrecking_crew_weapon_info& weapon = Wrecking_crew_weapons[Wrecking_crew_num_weapons];
            weapon.w_info_handle = handle;
            xml_table_get_req_int(weapon.rampage_time_penalty_ms, element, "rampage_time_penalty_ms");
            xml_table_get_req_float(weapon.mayhem_attack_multiplier, element, "mayhem_attack_multiplier");
            weapon.mode_flags = 0;
            xml_element* flags = xml_find(element, "mode_flags");
            if (flags != nullptr)
            {
                weapon.mode_flags = (unsigned short)xml_table_get_flags(mode_names, NUM_WRECKING_CREW_MODES, flags, true);
            }
            Wrecking_crew_num_weapons++;
        }
    }
    xml_element* backpacks = xml_table_find_opt(entry, "backpacks");
    if (backpacks != nullptr)
    {
        for (xml_element* element = xml_find(backpacks, "backpack"); element != nullptr && Wrecking_crew_num_backpacks < 16; element = xml_find_next(backpacks, element, "backpack"))
        {
            const int backpack_id = multi_game_backpack_get_backpack_id(xml_table_get_req_string_ref(element, "backpack_name"));
            if (backpack_id == -1)
            {
                continue;
            }
            wrecking_crew_backpack_info& backpack = Wrecking_crew_backpacks[Wrecking_crew_num_backpacks];
            backpack.backpack_type = backpack_id;
            xml_table_get_req_int(backpack.rampage_time_penalty_ms, element, "rampage_time_penalty_ms");
            backpack.mode_flags = 0;
            xml_element* flags = xml_find(element, "mode_flags");
            if (flags != nullptr)
            {
                backpack.mode_flags = (unsigned short)xml_table_get_flags(mode_names, NUM_WRECKING_CREW_MODES, flags, true);
            }
            Wrecking_crew_num_backpacks++;
        }
    }
    for (int m = 0; m < NUM_WRECKING_CREW_MODES; m++)
    {
        wrecking_crew_mode_info& mode = Wrecking_crew_mode_info[m];
        mode.info_text[0] = nullptr;
        mode.info_text[1] = nullptr;
        mode.info_text[2] = nullptr;
        sprintf(buffer, "%s_info_text", mode.internal_name);
        xml_element* texts = xml_find(entry, buffer);
        if (texts != nullptr)
        {
            xml_element* text = xml_find(texts, "info_text");
            for (int i = 0; text != nullptr && i < 3; i++)
            {
                mode.info_text[i] = Wrecking_crew_string_pool.add_unique(text->text);
                text = xml_find_next(texts, text, "info_text");
            }
        }
    }
    xml_element* mode_stats = xml_table_find_req(entry, "mode_stats");
    for (int m = 0; m < NUM_WRECKING_CREW_MODES; m++)
    {
        wrecking_crew_mode_info& mode = Wrecking_crew_mode_info[m];
        mode.games_played_overall_stat = STATS_INVALID;
        sprintf(buffer, "games_played_overall_%s_stat", mode.internal_name);
        const char* stat = xml_table_get_req_string_ref(mode_stats, buffer);
        if (stat != nullptr)
        {
            mode.games_played_overall_stat = stats_get_by_name(stat);
        }
    }
    xml_element* grid = xml_table_find_opt(entry, "Character_grid");
    if (grid != nullptr)
    {
        for (xml_element* element = xml_find(grid, "Character_Element"); element != nullptr; element = xml_find_next(grid, element, "Character_Element"))
        {
            const char* character_name = xml_table_get_req_string_ref(element, "Character_Name");
            const char* display_name = xml_table_get_req_string_ref(element, "Display_Name");
            // Characters are unique by display name (DLC tables repeat the base ones).
            bool found = false;
            Wrecking_crew_character_pool.thread_access.enter();
            for (wc_game_character_info* other = Wrecking_crew_character_pool.get_first(0); other != nullptr; other = Wrecking_crew_character_pool.get_next(other))
            {
                if (strcmp(other->display_name, display_name) == 0)
                {
                    found = true;
                    break;
                }
            }
            Wrecking_crew_character_pool.thread_access.leave();
            if (found)
            {
                continue;
            }
            const char* bitmap_name = xml_table_get_req_string_ref(element, "Bitmap_Name");
            const char* pooled_display_name = Wrecking_crew_string_pool.add_unique(display_name);
            const char* pooled_bitmap_name = Wrecking_crew_string_pool.add_unique(bitmap_name);
            const char* team_name = xml_table_get_req_string_ref(element, "Team");
            const human_info* hip = human_info_find(character_name);
            wc_game_character_info* info = Wrecking_crew_character_pool.alloc(0);
            if (info == nullptr)
            {
                break;
            }
            info->hip = hip;
            info->display_name = pooled_display_name;
            info->bitmap_name = pooled_bitmap_name;
            info->team = human_team_from_name(team_name);
            for (vector& offset : info->backpack_offsets)
            {
                offset.zero();
            }
            xml_element* offsets = xml_table_find_req(element, "backpack_offsets");
            for (xml_element* backpack = xml_find(offsets, "backpack"); backpack != nullptr; backpack = xml_find_next(offsets, backpack, "backpack"))
            {
                const int backpack_id = multi_game_backpack_get_backpack_id(xml_table_get_req_string_ref(backpack, "backpack_name"));
                if (backpack_id != -1)
                {
                    xml_table_get_req_vector(info->backpack_offsets[backpack_id], backpack, "backpack_offset");
                }
            }
            Wrecking_crew_characters.add(info);
        }
    }
    xml_element* locked_starts = xml_table_find_opt(entry, "locked_walker_starts");
    if (locked_starts != nullptr)
    {
        for (xml_element* element = xml_find(locked_starts, "locked_start"); element != nullptr; element = xml_find_next(locked_starts, element, "locked_start"))
        {
            const char* map_name = xml_table_get_req_string_ref(element, "map");
            if (map_name == nullptr)
            {
                continue;
            }
            const checksum_stri map_crc(map_name, 0);
            for (int i = 0; i < Wrecking_crew_num_maps; i++)
            {
                if (Wrecking_crew_map_info[i]->file_name_crc == map_crc)
                {
                    int spawn_index;
                    xml_table_get_req_int(spawn_index, element, "spawn_index");
                    wrecking_crew_mode_walker_disable_start_pos(i, spawn_index);
                    break;
                }
            }
        }
    }
    bool unlock_behemoth = false;
    xml_table_get_opt_bool(unlock_behemoth, entry, "unlock_behemoth");
    if (unlock_behemoth)
    {
        Behemoth_enabled = true;
    }
}

// 0x00776B30 (:1160): -1 for no limit.
static void wrecking_crew_get_weapon_ammo(int* magazine, int* reserve)
{
    if (Wrecking_crew_state == WCS_NONE)
    {
        return;
    }
    const wrecking_crew_mode_info* mode = wc_current_mode();
    if (mode == nullptr)
    {
        return;
    }
    if (mode->get_weapon_ammo != nullptr)
    {
        mode->get_weapon_ammo(magazine, reserve);
        return;
    }
    *reserve = -1;
}

// 0x00776B70 (:1177): the mode's ammo limits on the player's weapon; -1 means unlimited.
static void wrecking_crew_set_weapon_ammo(weapon* wp)
{
    int magazine = wp->get_rounds_in_magazine();
    int reserve = wp->get_rounds_in_reserve();
    wrecking_crew_get_weapon_ammo(&magazine, &reserve);
    if (magazine == -1)
    {
        Wrecking_crew_unlimited_magazine_ammo = true;
    }
    else
    {
        wp->set_rounds_in_magazine(magazine);
        Wrecking_crew_unlimited_magazine_ammo = false;
    }
    if (reserve == -1)
    {
        Wrecking_crew_unlimited_reserve_ammo = true;
    }
    else
    {
        wp->set_rounds_in_reserve(reserve);
        Wrecking_crew_unlimited_reserve_ammo = false;
    }
}

// 0x00776C80 (:1198)
static int wrecking_crew_get_random_weapon()
{
    int candidates[16];
    int num_candidates = 0;
    for (int i = 0; i < Wrecking_crew_num_weapons; i++)
    {
        if (wrecking_crew_is_weapon_available_for_mode(i, Wrecking_crew_options.mode))
        {
            candidates[num_candidates++] = i;
        }
    }
    if (num_candidates > 0)
    {
        return candidates[int_rand_range(0, num_candidates - 1)];
    }
    return -1;
}

// 0x00776D20 (:1213)
static void wrecking_crew_assign_weapon()
{
    int index = Wrecking_crew_options.weapon_index;
    if (index == -1)
    {
        index = wrecking_crew_get_random_weapon();
    }
    Wrecking_crew_current_weapon = wrecking_crew_get_weapon_info(index);
}

// 0x00776D60 (:1222): the weapon after the current one among those of the mode (the veto).
static int wrecking_crew_get_next_weapon()
{
    int candidates[16];
    int num_candidates = 0;
    int current = 0;
    for (int i = 0; i < Wrecking_crew_num_weapons; i++)
    {
        if (wrecking_crew_is_weapon_available_for_mode(i, Wrecking_crew_options.mode))
        {
            if (Wrecking_crew_current_weapon == &Wrecking_crew_weapons[i])
            {
                current = num_candidates;
            }
            candidates[num_candidates++] = i;
        }
    }
    if (num_candidates > 0)
    {
        return current + 1 < num_candidates ? candidates[current + 1] : candidates[0];
    }
    return -1;
}

// 0x00776E20 (:1249)
static void wrecking_crew_next_weapon()
{
    int index = Wrecking_crew_options.weapon_index;
    if (index == -1)
    {
        index = wrecking_crew_get_next_weapon();
    }
    Wrecking_crew_current_weapon = wrecking_crew_get_weapon_info(index);
}

// 0x00776E60 (:1258)
static int wrecking_crew_get_next_backpack()
{
    int candidates[16];
    int num_candidates = 0;
    int current = 0;
    for (int i = 0; i < Wrecking_crew_num_backpacks; i++)
    {
        if (wrecking_crew_is_backpack_available_for_mode(i, Wrecking_crew_options.mode))
        {
            if (Wrecking_crew_current_backpack == &Wrecking_crew_backpacks[i])
            {
                current = num_candidates;
            }
            candidates[num_candidates++] = i;
        }
    }
    if (num_candidates > 0)
    {
        return current + 1 < num_candidates ? candidates[current + 1] : candidates[0];
    }
    return -1;
}

// 0x00776F20 (:1285)
static void wrecking_crew_next_backpack()
{
    int index = Wrecking_crew_options.backpack_index;
    if (index == -1)
    {
        index = wrecking_crew_get_next_backpack();
    }
    Wrecking_crew_current_backpack = wrecking_crew_get_backpack_info(index);
}

// 0x00776F80 (:1295)
static int wrecking_crew_get_random_backpack()
{
    int candidates[16];
    int num_candidates = 0;
    for (int i = 0; i < Wrecking_crew_num_backpacks; i++)
    {
        if (wrecking_crew_is_backpack_available_for_mode(i, Wrecking_crew_options.mode))
        {
            candidates[num_candidates++] = i;
        }
    }
    if (num_candidates > 0)
    {
        return candidates[int_rand_range(0, num_candidates - 1)];
    }
    return -1;
}

// 0x00777020 (:1310): no backpack in walker mode.
static void wrecking_crew_assign_backpack()
{
    if (Wrecking_crew_options.mode == WCM_WALKER)
    {
        Wrecking_crew_current_backpack = nullptr;
        return;
    }
    int index = Wrecking_crew_options.backpack_index;
    if (index == -1)
    {
        index = wrecking_crew_get_random_backpack();
    }
    Wrecking_crew_current_backpack = wrecking_crew_get_backpack_info(index);
}

// 0x00790F60 (:1321)
static void wrecking_crew_assign_spawn_point()
{
    Wrecking_crew_spawn_point = int_rand_range(0, World.get_num_objects<player_start>() - 1);
}

// 0x00777080 (:1326): clicks on the splash's continue/veto buttons and on the hints (continue, previous and next
// camera).
Wc_mouse_click_result wrecking_crew_check_mouse_clicks()
{
    int result = WC_MCR_NONE;
    if (Wrecking_crew_state == WCS_ROUND_BEGIN || Wrecking_crew_state == WCS_PRE_TURN_BREAK || Wrecking_crew_state == WCS_PRE_BREAK)
    {
        static const char* hint_names[3] = {"mp_splash_player", "splash_weapons_hint", "splash_pack_hint"}; // 0x0165B110
        static const char* hint_names2[3] = {"hint1", "hint2", "hint3"}; // 0x0165B104
        const int splash = ui_mouse_process_hints(hint_names, 3, hud_ui_wrecking_crew_get_doc_handle(HWCD_SPLASH), true);
        if (splash != -1)
        {
            result = splash;
        }
        const int hint = ui_mouse_process_hints(hint_names2, 3, hud_ui_wrecking_crew_get_doc_handle(HWCD_HINTS), true);
        if (hint != -1)
        {
            return (Wc_mouse_click_result)(hint + WC_MCR_HINT_CONTINUE);
        }
    }
    else
    {
        static const char* hint_names[3] = {"hint1", "hint2", "hint3"}; // 0x0165B11C
        const int hint = ui_mouse_process_hints(hint_names, 3, hud_ui_wrecking_crew_get_doc_handle(HWCD_HINTS), true);
        if (hint != -1)
        {
            return (Wc_mouse_click_result)(hint + WC_MCR_HINT_CONTINUE);
        }
    }
    return (Wc_mouse_click_result)result;
}

// 0x00777110 (:1361)
static bool wrecking_crew_continue_button_pressed()
{
    if (game_is_paused())
    {
        return false;
    }
    const float button = ui_get_button_info_and_wait(10, Wrecking_crew_input_id); // magic number 10: the accept button
    const Wc_mouse_click_result click = wrecking_crew_check_mouse_clicks();
    if (button <= 0.0f && click != WC_MCR_SPLASH_CONTINUE && click != WC_MCR_HINT_CONTINUE)
    {
        return false;
    }
    ui_menu_play_sound(MENU_SOUND_PRESS_SELECT);
    return true;
}

// 0x00777170 (:1381)
static bool wrecking_crew_veto_weapon_button_pressed()
{
    if (game_is_paused() || !wrecking_crew_can_veto_weapon())
    {
        return false;
    }
    const float button = ui_get_button_info_and_wait(15, Wrecking_crew_input_id); // magic number 15: the weapon veto button
    const Wc_mouse_click_result click = wrecking_crew_check_mouse_clicks();
    if (button <= 0.0f && click != WC_MCR_SPLASH_VETO_WEAPON)
    {
        return false;
    }
    ui_menu_play_sound(MENU_SOUND_PRESS_MISC);
    return true;
}

// 0x007771D0 (:1405)
static bool wrecking_crew_veto_backpack_button_pressed()
{
    if (game_is_paused() || !wrecking_crew_can_veto_backpack())
    {
        return false;
    }
    const float button = ui_get_button_info_and_wait(14, Wrecking_crew_input_id); // magic number 14: the backpack veto button
    const Wc_mouse_click_result click = wrecking_crew_check_mouse_clicks();
    if (button <= 0.0f && click != WC_MCR_SPLASH_VETO_PACK)
    {
        return false;
    }
    ui_menu_play_sound(MENU_SOUND_PRESS_MISC);
    return true;
}

// 0x0075FD30 (:1429): the next enabled player (or the round's first one), with that player's controls.
static void wrecking_crew_move_to_next_players_turn()
{
    int next = Wrecking_crew_first_player;
    if (next == -1)
    {
        int index = Wrecking_crew_current_player;
        do
        {
            next = index + 1;
            if (Wrecking_crew_options.num_players <= next)
            {
                next = 0;
            }
            index = next;
        } while (next != Wrecking_crew_current_player && !Wrecking_crew_player_status[next].enabled);
    }
    else
    {
        Wrecking_crew_first_player = -1;
    }
    Wrecking_crew_current_player = next;
    if (next == -1)
    {
        return;
    }
    Wrecking_crew_player_status[next].turn_score = 0.0f;
    const control_config_setting setting = controls_get_current_config_setting();
    wc_copy_control_config(Control_config[Player_controller_index][setting], Wc_player_infos[Wrecking_crew_current_player].ctrl_config);
    controller_options_set_control_config(Wc_player_infos[Wrecking_crew_current_player].control_options);
}

// 0x0079EDC0 (:1799): a player whose turn it is not is unarmed, without backpack, hidden and without collision.
static void wrecking_crew_hide_player(player* pp)
{
    human_weapons_immediately_holster_weapon(pp);
    if (pp->backpack != nullptr)
    {
        pp->backpack->event_player_dropped(true);
        pp->backpack = nullptr;
    }
    human_delete_all_items_from_inventory(pp, true, false);
    object_set_visibility_plus_children(pp, false);
    pp->hflags.hidden = 1;
    human_update_collision_filter(pp, false);
    if (pp == player_get_local_player())
    {
        wrecking_crew_walker_visible(false);
    }
}

// 0x007A5EA0 (:1475)
static void wrecking_crew_change_state(wrecking_crew_states state)
{
    const int num_players = Wrecking_crew_options.num_players;
    const wrecking_crew_modes mode_index = Wrecking_crew_options.mode;
    const wrecking_crew_mode_info* mode = wc_current_mode();
    if (mode == nullptr)
    {
        return;
    }
    Wrecking_crew_state = state;
    switch (state)
    {
    case WCS_RESTORE_MAP:
        ui_fade_to_black_fade_out(false);
        Wrecking_crew_restore_map_requested = true;
        break;
    case WCS_CHALLENGE_BEGIN:
    case WCS_CHALLENGE_ROUND_BEGIN:
        break;
    case WCS_MATCH_BEGIN:
        if (mode->match_begin != nullptr)
        {
            mode->match_begin();
        }
        Wrecking_crew_timer.invalidate();
        Wrecking_crew_turn_move_on_timer.invalidate();
        Wrecking_crew_round_winner = -1;
        Wrecking_crew_match_winner = -1;
        Wrecking_crew_current_player = -1;
        Wrecking_crew_first_player = -1;
        Wrecking_crew_last_first_player = -1;
        Wrecking_crew_spawn_point = 0;
        Wrecking_crew_current_round = 0;
        Wrecking_crew_match_start_time_seconds = timer_get(TIMER_SECONDS);
        memset(Challenge_player_round_scores, 0, sizeof(Challenge_player_round_scores));
        for (int i = 0; i < 4; i++)
        {
            wrecking_crew_player_status& status = Wrecking_crew_player_status[i];
            status.player_index = i;
            status.turn_score = 0.0f;
            status.round_score = 0.0f;
            status.match_score = 0.0f;
            status.turn_destruction_done = 0.0f;
            status.round_destruction_done = 0.0f;
            status.match_destruction_done = 0.0f;
            status.round_wins = 0;
            status.had_a_turn_for_current_round = false;
            status.enabled = true;
        }
        Wrecking_crew_match_id++;
        break;
    case WCS_ROUND_BEGIN:
        Check_for_walker_load = false;
        for (int i = 0; i < num_players; i++)
        {
            wrecking_crew_player_status& status = Wrecking_crew_player_status[i];
            status.had_a_turn_for_current_round = false;
            status.turn_score = 0.0f;
            status.turn_destruction_done = 0.0f;
            status.round_score = 0.0f;
            status.round_destruction_done = 0.0f;
        }
        Wrecking_crew_round_high_score = 0.0f;
        wrecking_crew_assign_weapon();
        wrecking_crew_assign_backpack();
        wrecking_crew_assign_walker();
        wrecking_crew_assign_spawn_point();
        if (player* pp = player_get_local_player())
        {
            wrecking_crew_set_default_inventory(pp);
            wrecking_crew_move_to_next_players_turn();
            objective_highlight_remove_all();
            hud_icon_remove_all();
            if (Wrecking_crew_mode_info[mode_index].round_begin != nullptr)
            {
                Wrecking_crew_mode_info[mode_index].round_begin();
            }
        }
        break;
    case WCS_PRE_TURN_BREAK:
        if (Wrecking_crew_current_player != -1 && Wrecking_crew_player_status[Wrecking_crew_current_player].had_a_turn_for_current_round)
        {
            wrecking_crew_move_to_next_players_turn();
        }
        break;
    case WCS_PRE_TURN:
        Wrecking_crew_timer.set(3000); // magic number: the 3 s countdown before a turn
        if (Wrecking_crew_mode_info[mode_index].players_turn_begin != nullptr)
        {
            Wrecking_crew_turn_time_left = Wrecking_crew_mode_info[mode_index].players_turn_begin();
            wrecking_crew_respawn_player();
            Wrecking_crew_player_status[Wrecking_crew_current_player].had_a_turn_for_current_round = true;
            Wrecking_crew_pre_turn_fov = camera_view_get_current_fov();
            multi_game_camera_stop();
            if (player* pp = player_get_local_player())
            {
                pp->pflags.no_fineaim = 1;
                if (pp->backpack != nullptr)
                {
                    const char* tooltip = pp->backpack->get_backpack_info()->pickup_tooltip;
                    if (tooltip != nullptr)
                    {
                        ui_tool_tips_add_tip(tooltip, 0, nullptr);
                    }
                }
            }
        }
        break;
    case WCS_PLAYERS_TURN:
    {
        gameplay_play_sfx(GSFX_GO);
        wchar_t text[16];
        strncpy_safe(text, localization_get_display_string_req("HM_GO", nullptr), 16);
        hud_ui_wrecking_crew_show_counter(text, false);
        Wrecking_crew_timer.set(Wrecking_crew_turn_time_left);
        Wrecking_crew_respawn_timer.invalidate();
        Wrecking_crew_respawn_blur_timer.invalidate();
        if (player* pp = player_get_local_player())
        {
            pp->pflags.no_fineaim = 0;
        }
        break;
    }
    case WCS_PRE_BREAK:
        Wrecking_crew_turn_move_on_timer.invalidate();
        break;
    case WCS_BREAK:
    {
        if (mode->get_current_players_score != nullptr)
        {
            Wrecking_crew_player_status[Wrecking_crew_current_player].turn_score = mode->get_current_players_score(Wrecking_crew_turn_time_left);
        }
        wrecking_crew_player_status& status = Wrecking_crew_player_status[Wrecking_crew_current_player];
        int points = wrecking_crew_convert_score(status.turn_score);
        stats_increment(STATS_WRECKING_CREW_DESTRUCTION_DONE, &points);
        if (unit4::LocalGameSession* session = rfg_get_local_game_session())
        {
            session->updateHeroStatAdd(nullptr, 8, points); // magic number 8: the hero stat of Wrecking Crew points
        }
        if (Wrecking_crew_options.current_challenge_id != -1)
        {
            Challenge_player_round_scores[Wrecking_crew_current_player][Wrecking_crew_current_round] = status.turn_score + Challenge_player_round_scores[Wrecking_crew_current_player][Wrecking_crew_current_round];
        }
        status.round_score = status.turn_score + status.round_score;
        status.round_destruction_done = status.turn_destruction_done + status.round_destruction_done;
        if (Wrecking_crew_mode_info[mode_index].players_turn_end != nullptr)
        {
            Wrecking_crew_round_winner = Wrecking_crew_mode_info[mode_index].players_turn_end(Wrecking_crew_turn_time_left);
            Wrecking_crew_splash_blur_timer.set(2000);
            gameplay_play_sfx(GSFX_WRECKING_CREW_ROUND_OVER);
        }
        break;
    }
    case WCS_ROUND_END:
    {
        Wrecking_crew_player_status[Wrecking_crew_round_winner].round_wins++;
        int total_wins = 0;
        for (int i = 0; i < num_players; i++)
        {
            total_wins += Wrecking_crew_player_status[i].round_wins;
        }
        if (Wrecking_crew_options.current_challenge_id != -1)
        {
            // Every round has one winner, so the wins so far number the rounds played.
            for (int i = 0; i < num_players; i++)
            {
                Challenge_player_round_scores[i][total_wins - 1] = Wrecking_crew_player_status[i].round_score;
            }
        }
        if (Wrecking_crew_mode_info[mode_index].round_end == nullptr)
        {
            break;
        }
        Wrecking_crew_match_winner = Wrecking_crew_mode_info[mode_index].round_end();
        for (wrecking_crew_player_status& status : Wrecking_crew_player_status)
        {
            status.match_score = status.match_score + status.round_score;
            status.match_destruction_done = status.round_destruction_done + status.match_destruction_done;
        }
        Wrecking_crew_veto_player = Wrecking_crew_round_winner;
        if (Wrecking_crew_options.current_challenge_id != -1 && Wrecking_crew_options.rounds_to_win - 1 <= Wrecking_crew_current_round)
        {
            // Original: all four slots are compared, whatever num_players is.
            int winner = Wrecking_crew_player_status[0].match_score < Wrecking_crew_player_status[1].match_score ? 1 : 0;
            Wrecking_crew_match_winner = winner;
            if (Wrecking_crew_player_status[winner].match_score < Wrecking_crew_player_status[2].match_score)
            {
                winner = 2;
                Wrecking_crew_match_winner = 2;
            }
            if (Wrecking_crew_player_status[winner].match_score < Wrecking_crew_player_status[3].match_score)
            {
                Wrecking_crew_match_winner = 3;
            }
        }
        gameplay_play_sfx(GSFX_WRECKING_CREW_ROUND_OVER_STAMP);
        const weapon_info* w_info = Wrecking_crew_current_weapon != nullptr ? weapons_get_weapon_info(Wrecking_crew_current_weapon->w_info_handle) : nullptr;
        const multi_backpack_info* b_info = Wrecking_crew_current_backpack != nullptr ? multi_game_backpack_get_info(Wrecking_crew_current_backpack->backpack_type) : nullptr;
        log_event_wc_round(gamelog_get_player_checksum(), Wrecking_crew_match_id, (char)(Wrecking_crew_current_round + 1), gamelog_get_weapon_crc(w_info),
            gamelog_get_backpack_crc(b_info), wrecking_crew_convert_score(Wrecking_crew_player_status[0].round_score),
            wrecking_crew_convert_score(Wrecking_crew_player_status[1].round_score), wrecking_crew_convert_score(Wrecking_crew_player_status[2].round_score),
            wrecking_crew_convert_score(Wrecking_crew_player_status[3].round_score), (char)(Wrecking_crew_round_winner + 1));
        break;
    }
    case WCS_MATCH_END:
    {
        if (mode->match_end != nullptr)
        {
            mode->match_end();
        }
        const wrecking_crew_map_info* map = Wrecking_crew_map_info[Wrecking_crew_options.map_index];
        stats_increment_by_one(map->games_played_overall_stat);
        stats_increment_by_one(map->games_played_mode_stats[Wrecking_crew_options.mode]);
        stats_increment_by_one(Wrecking_crew_mode_info[Wrecking_crew_options.mode].games_played_overall_stat);
        stats_increment_by_one(STATS_WRECKING_CREW_GAMES_BY_MODE_TOTAL_OVERALL_OVERALL);
        gameplay_play_sfx(GSFX_WRECKING_CREW_MATCH_OVER);
        if (Wrecking_crew_options.current_challenge_id != -1)
        {
            report_stats();
        }
        log_event_wc_match(gamelog_get_player_checksum(), (char)Wrecking_crew_options.num_players, (char)Wrecking_crew_options.mode, (int)map->file_name_crc.get_crc(),
            (char)(Wrecking_crew_current_round + 1), timer_get_since(Wrecking_crew_match_start_time_seconds, TIMER_SECONDS), gamelog_get_locale(),
            gamelog_get_language(), Wrecking_crew_match_id, wrecking_crew_convert_score(Wrecking_crew_player_status[0].match_score),
            wrecking_crew_convert_score(Wrecking_crew_player_status[1].match_score), wrecking_crew_convert_score(Wrecking_crew_player_status[2].match_score),
            wrecking_crew_convert_score(Wrecking_crew_player_status[3].match_score), (char)(Wrecking_crew_match_winner + 1));
        break;
    }
    default:
        break;
    }
}

// 0x007A68D0 (:1762)
static void wrecking_crew_restore_map(wrecking_crew_states next_state)
{
    Wrecking_crew_restore_map_state = next_state;
    wrecking_crew_change_state(WCS_RESTORE_MAP);
}

// 0x0075FDF0 (:1768): the multiplayer interface memory comes from the WC mempool.
static void wrecking_crew_interface_init()
{
    int cpu_size = 0;
    int gpu_size = 0;
    multi_game_interface_memory_required(&cpu_size, &gpu_size);
    cpu_size += gpu_size;
    gpu_size = 0;
    void* cpu_mem = Wrecking_crew_mempool.static_mempool_base::alloc(cpu_size, 16);
    Wrecking_crew_interface_mempool.init_rfg_split_mempool("WC interface pool", cpu_mem, cpu_size, 4, nullptr, gpu_size, 4, 8);
    multi_game_interface_memory_init(&Wrecking_crew_interface_mempool);
}

// 0x0075FE70 (:1789)
static void wrecking_crew_interface_shutdown()
{
    multi_game_interface_memory_shutdown();
    Wrecking_crew_interface_mempool.split_mempool::set_thread_ownership();
    Wrecking_crew_interface_mempool.split_mempool::clear();
}

// 0x007A68E0 (:1819): the next round, or the match end once someone won (three rounds in a challenge).
static void wrecking_crew_do_round_end()
{
    const wrecking_crew_modes mode_index = Wrecking_crew_options.mode;
    const int round_winner = Wrecking_crew_round_winner;
    Wrecking_crew_current_round++;
    if (wc_current_mode() == nullptr)
    {
        return;
    }
    if (Wrecking_crew_options.current_challenge_id == -1)
    {
        if (Wrecking_crew_match_winner == -1)
        {
            Wrecking_crew_round_winner = -1;
            Wrecking_crew_last_first_player = round_winner;
            Wrecking_crew_first_player = round_winner;
            wrecking_crew_restore_map(WCS_ROUND_BEGIN);
            if (Wrecking_crew_mode_info[mode_index].skip_round_begin)
            {
                wrecking_crew_process();
            }
            return;
        }
    }
    else if (Wrecking_crew_current_round < 3) // magic number 3: the rounds of a challenge
    {
        Wrecking_crew_round_winner = -1;
        Wrecking_crew_last_first_player = round_winner;
        Wrecking_crew_first_player = round_winner;
        wrecking_crew_restore_map(WCS_ROUND_BEGIN);
        if (Wrecking_crew_mode_info[mode_index].skip_round_begin)
        {
            wrecking_crew_process();
        }
        // Original: the restore is requested a second time, now ending in the challenge's round splash.
        wrecking_crew_restore_map(WCS_CHALLENGE_ROUND_BEGIN);
        return;
    }
    wrecking_crew_change_state(WCS_MATCH_END);
}

// 0x0075FE90 (:1864)
bool wrecking_crew_behemoth_enabled()
{
    return Behemoth_enabled || g_unlockBehemoth;
}

// 0x0079EE20 (:1869): at boot; registers the three menu game states.
void wrecking_crew_system_init()
{
    ui_document_menu_wrecking_crew_init();
    ui_document_menu_wrecking_crew_character_select_init();
    ui_document_menu_wrecking_crew_scoreboard_init();
    Wrecking_crew_map_pool.init(Permanent_mempool, 32);
    // Magic number 0xC00: the string pool of the tables' names.
    void* address = Permanent_mempool.static_mempool_base::alloc(0xC00, 4);
    Wrecking_crew_string_pool.init(address, 0xC00, nullptr, 0, 0);
    Wrecking_crew_character_pool.init(Permanent_mempool, 50);
}

// 0x007AC290 (:1885): entering the mode (GS_WC_INIT): its 40 MB mempool and every table, base and DLC.
void wrecking_crew_init()
{
    // Magic number 0x2800000: 40 MB of level memory for the mode.
    void* memory = Level_memmgr.alloc(0x2800000, "WC memory CPU", 16);
    Wrecking_crew_mempool.init_rfg_split_mempool("WC mempool", memory, 0x2800000, 4, nullptr, 0, 4, 8);
    Wrecking_crew_map_pool.clear();
    Wrecking_crew_string_pool.clear();
    Wrecking_crew_character_pool.clear();
    Wrecking_crew_characters.remove_all();
    Wrecking_crew_num_maps = 0;
    Wrecking_crew_num_weapons = 0;
    Wrecking_crew_num_backpacks = 0;
    Wrecking_crew_options.free_play = false;
    const bool was_locked = cf_is_locked();
    if (was_locked)
    {
        cf_open_lock(false);
    }
    bool interface_marked = false;
    dlc_get_bundles_sync();
    Level_mempool.clear();
    dlc_interface_shutdown();
    loading_screen_dlc_restore();
    const int num_bundles = (int)dlc_count_callback();
    if (Use_packfiles)
    {
        game_enable_standard_access();
    }
    for (int i = 0; i < num_bundles; i++)
    {
        if (!dlc_bundle_contains_wc(i))
        {
            continue;
        }
        const int dlc_id = dlc_bundle_index_to_id(i);
        dlc_add_bundle(dlc_id, i);
        dlc_localization_init(dlc_id);
        if (!interface_marked)
        {
            char name[256];
            sprintf(name, "dlc%02d_%s", dlc_id, "rfg.vint_proj");
            if (cf_exist(name))
            {
                dlc_interface_mark();
                interface_marked = true;
            }
        }
        loading_screen_parse_images_table(dlc_id);
        dlc_interface_init(dlc_id);
    }
    wrecking_crew_walker_clear_start_positions();
    Behemoth_enabled = false;
    Num_challenge_modes = 0;
    wrecking_crew_init_challenges(-1);
    for (int i = 0; i < num_bundles; i++)
    {
        if (dlc_bundle_contains_wc(i))
        {
            wrecking_crew_init_challenges(dlc_bundle_index_to_id(i));
        }
    }
    Wrecking_crew_num_maps = 0;
    xml_element* root = wrecking_crew_parse_table("wrecking_crew.xtbl", -1);
    if (root != nullptr)
    {
        wrecking_crew_parse_wrecking_crew(root, -1);
        wrecking_crew_close_table(root, -1);
    }
    for (int i = 0; i < num_bundles; i++)
    {
        if (!dlc_bundle_contains_wc(i))
        {
            continue;
        }
        root = wrecking_crew_parse_table("wrecking_crew.xtbl", dlc_bundle_index_to_id(i));
        if (root != nullptr)
        {
            wrecking_crew_parse_wrecking_crew(root, dlc_bundle_index_to_id(i));
            wrecking_crew_close_table(root, dlc_bundle_index_to_id(i));
        }
    }
    if (Use_packfiles)
    {
        game_disable_standard_access();
    }
    if (was_locked)
    {
        cf_open_lock(true);
    }
    for (wrecking_crew_walker_info& walker : Wrecking_crew_walkers)
    {
        walker.vehicle_type = vehicle_get_type(walker.vehicle_name);
    }
    wrecking_crew_interface_init();
    if (!Wrecking_crew_restarted)
    {
        gamelog_local_reinit(false, nullptr);
    }
    Wrecking_crew_restarted = false;
    extras_menu_init_wc_maps();
    // Magic number 0x65: the scoreboard entries.
    if (!multi_core_platform_scoreboard_ready() && !multi_core_platform_scoreboard_init(&Wrecking_crew_mempool, 0x65))
    {
        keen::debug::nativePrintArgs("Failed to initialize scoreboard system");
    }
}

// 0x0075FEB0 (:2031): player one keeps the main player's controls, the others get the defaults.
void wrecking_crew_reset_controls()
{
    for (wc_player_info& info : Wc_player_infos)
    {
        info.character_index = 0;
    }
    const control_config_setting setting = controls_get_current_config_setting();
    controller_options_get_control_config(Wc_player_infos[0].control_options);
    for (int i = 1; i < 4; i++)
    {
        controller_options_reset_to_default(Wc_player_infos[i].control_options);
    }
    // Original: the main player's config goes to the slot of the controller index, which is 0 on PC.
    wc_copy_control_config(Wc_player_infos[Player_controller_index].ctrl_config, Control_config[Player_controller_index][setting]);
    control_config defaults;
    memset(&defaults, 0xFF, sizeof(defaults));
    control_config_populate_with_defaults(defaults);
    for (int i = 1; i < 4; i++)
    {
        wc_copy_control_config(Wc_player_infos[i].ctrl_config, defaults);
    }
}

// 0x0075FFB0 (:2056)
void wrecking_crew_shutdown()
{
    wrecking_crew_interface_shutdown();
    Level_memmgr.free(Wrecking_crew_mempool.static_mempool_base::get_base());
    Wrecking_crew_mempool.split_mempool::set_thread_ownership();
    Wrecking_crew_mempool.split_mempool::clear();
    dlc_interface_shutdown();
    loading_screen_dlc_restore();
}

// 0x00760000 (:2073)
void wrecking_crew_set_thread_ownership()
{
    Wrecking_crew_interface_mempool.split_mempool::set_thread_ownership();
    Wrecking_crew_mempool.split_mempool::set_thread_ownership();
}

// 0x00760020 (:2081): the table, or for a DLC its "dlcNN_" copy when that exists.
xml_element* wrecking_crew_parse_table(const char* name, int dlc_id)
{
    char dlc_name[256];
    if (dlc_id != -1)
    {
        sprintf(dlc_name, "dlc%02d_%s", dlc_id, name);
        if (!cf_exist(dlc_name))
        {
            return nullptr;
        }
        name = dlc_name;
    }
    return xml_table_parse(name);
}

// 0x007600A0 (:2103)
void wrecking_crew_close_table(xml_element* root, int dlc_id)
{
    xml_table_close(root);
}

// 0x007A69B0 (:2110): a challenge takes its first round's settings; the map loads as a single zone territory.
void wrecking_crew_match_begin()
{
    Wrecking_crew_veto_player = 0;
    if (Wrecking_crew_options.current_challenge_id != -1)
    {
        const wrecking_crew_challenge_mode* challenge = wrecking_crew_get_challenge_mode_by_index(Wrecking_crew_options.current_challenge_id);
        if (challenge == nullptr)
        {
            return;
        }
        const wrecking_crew_challenge_mode_round& round = challenge->rounds[0];
        Wrecking_crew_options.mode = round.mode;
        Wrecking_crew_options.weapon_index = wrecking_crew_get_weapon_index(round.winfo_handle);
        Wrecking_crew_options.backpack_index = wrecking_crew_get_backpack_index(round.backpack_id);
        Wrecking_crew_options.stress_mode = round.stress_mode;
        Wrecking_crew_options.walker_choice = round.walker_choice;
        Wrecking_crew_options.spawn_barrels = round.barrels;
        Wrecking_crew_options.rounds_to_win = 3;
    }
    if (wc_current_mode() == nullptr || Wrecking_crew_options.map_index < 0 || Wrecking_crew_options.map_index >= Wrecking_crew_num_maps
        || (unsigned int)(Wrecking_crew_options.num_players - 1) >= 4)
    {
        return;
    }
    const wrecking_crew_mode_info* mode = wrecking_crew_get_mode_info(Wrecking_crew_options.mode);
    if (mode == nullptr || mode->players_turn_begin == nullptr || mode->players_turn_end == nullptr)
    {
        return;
    }
    const char* file_name = wrecking_crew_get_map_file_name(Wrecking_crew_options.map_index);
    if (file_name == nullptr)
    {
        return;
    }
    char zone_name[256];
    sprintf(zone_name, "%s%s", file_name, ".rfgzone_pc");
    if (World.set_pending_territory_load(zone_name, true, true, false, -1, false))
    {
        wrecking_crew_change_state(Wrecking_crew_options.current_challenge_id != -1 ? WCS_CHALLENGE_BEGIN : WCS_MATCH_BEGIN);
    }
    Wrecking_crew_input_id = ui_register_for_processed_input(IRT_UNLIMITED_INPUT, false);
    for (player*& pp : Wrecking_crew_players)
    {
        pp = nullptr;
    }
}

// 0x007600B0 (:2155)
int wrecking_crew_dlc_bundle_id()
{
    if (Wrecking_crew_options.map_index >= 0 && Wrecking_crew_options.map_index < Wrecking_crew_num_maps)
    {
        return Wrecking_crew_map_info[Wrecking_crew_options.map_index]->dlc_id;
    }
    return -1;
}

// 0x007600D0 (:2161): back up the main player's controls; set the buildings' stress mode.
void wrecking_crew_level_init_pre()
{
    const control_config_setting setting = controls_get_current_config_setting();
    wc_copy_control_config(Wrecking_crew_main_player_ctrl_config_backup, Control_config[Player_controller_index][setting]);
    controller_options_get_control_config(Wrecking_crew_main_player_control_options_backup);
    multi_game_backpack_level_init_pre();
    if (Wrecking_crew_options.stress_mode == WCSS_LOW)
    {
        stress_evaluation_set_mode("Wrecking_Crew_Low");
    }
    else if (Wrecking_crew_options.stress_mode == WCSS_HIGH)
    {
        stress_evaluation_set_mode("Wrecking_Crew_High");
    }
}

// 0x0079EEA0 (:2180): record the map's movers (barrels destroyed when barrels are off), hide every player, start the
// overview camera and set the map's time of day.
void wrecking_crew_level_init_post()
{
    Wrecking_crew_mover_handles.remove_all();
    Wrecking_crew_mover_barrel_flag.remove_all();
    const int num_movers = World.get_num_objects<object_mover>();
    for (int i = 0; i < num_movers; i++)
    {
        object_mover* mover = World.get_object<object_mover>(i);
        Wrecking_crew_mover_handles.add(mover->get_handle());
        if (mover->get_obj_type() == OT_MOVER && mover->get_sub_type() == OT_SUB_MOVER_GENERAL)
        {
            general_mover* gm = (general_mover*)mover;
            Wrecking_crew_mover_barrel_flag.add(gm->explodes_on_death());
            if (gm->explodes_on_death() && !Wrecking_crew_options.spawn_barrels)
            {
                World.destroy_object(gm, false);
            }
        }
        else
        {
            Wrecking_crew_mover_barrel_flag.add(false);
        }
    }
    const int num_players = World.get_num_objects<player>();
    for (int i = 0; i < num_players; i++)
    {
        wrecking_crew_hide_player(World.get_object<player>(i));
    }
    multi_game_marker_level_init_post();
    multi_game_camera_start();
    const wrecking_crew_map_info* map = Wrecking_crew_map_info[Wrecking_crew_options.map_index];
    game_clock_set_time(map->time_of_day_hour, map->time_of_day_minute, 0);
    Sunshafts_scale_saved = Rl_sun_shafts_scale;
    if (strcmp(map->file_name, "wcdlc9") == 0)
    {
        Rl_sun_shafts_scale = 0.13f; // magic number: DLC map 9 has weaker sun shafts
    }
    Wrecking_crew_splash_blur_timer.set(2000);
    gamemusic_multiplayer_start();
    for (int i = 0; i < Wrecking_crew_options.num_players; i++)
    {
        const char* bitmap = Wrecking_crew_characters[Wc_player_infos[i].character_index]->bitmap_name;
        if (bitmap != nullptr)
        {
            ui_request_image(bitmap);
        }
    }
}

// 0x00777230 (:2233): release the character images and give the main player its controls back.
void wrecking_crew_level_shutdown()
{
    for (int i = 0; i < Wrecking_crew_options.num_players; i++)
    {
        const char* bitmap = Wrecking_crew_characters[Wc_player_infos[i].character_index]->bitmap_name;
        if (bitmap != nullptr)
        {
            ui_release_image(bitmap);
        }
    }
    stress_evaluation_set_mode("");
    multi_game_backpack_level_shutdown();
    Wrecking_crew_state = WCS_NONE;
    ui_relinquish_input_id(Wrecking_crew_input_id);
    Wrecking_crew_input_id = -1;
    if (!Wrecking_crew_should_restart)
    {
        gamelog_local_reinit(true, nullptr);
    }
    multi_game_camera_shutdown();
    const control_config_setting setting = controls_get_current_config_setting();
    wc_copy_control_config(Control_config[Player_controller_index][setting], Wrecking_crew_main_player_ctrl_config_backup);
    controller_options_set_control_config(Wrecking_crew_main_player_control_options_backup);
    game_options_load_from_cache();
    gamemusic_frontend_start(true);
    Rl_sun_shafts_scale = Sunshafts_scale_saved;
}

// 0x007A6B90 (:2274): the state machine, once a frame in gameplay.
void wrecking_crew_process()
{
    gamemusic_update(game_is_paused());
    const bool in_turn = gameseq_get_state() == GS_GAMEPLAY && (Wrecking_crew_state == WCS_PLAYERS_TURN || Wrecking_crew_state == WCS_PRE_TURN || Wrecking_crew_state == WCS_PRE_BREAK);
    ui_cursor_set_visible(!in_turn);
    const wrecking_crew_modes mode_index = Wrecking_crew_options.mode;
    if (wc_current_mode() == nullptr)
    {
        return;
    }
    const wrecking_crew_mode_info& mode = Wrecking_crew_mode_info[mode_index];
    // The players who are not playing wait at the spawn point.
    object* spawn = World.get_object<player_start>(Wrecking_crew_spawn_point);
    if (spawn == nullptr)
    {
        spawn = World.get_object<player_start>(0);
    }
    player* current = nullptr;
    if (Wrecking_crew_current_player != -1)
    {
        current = wrecking_crew_get_current_player();
    }
    const int num_players = World.get_num_objects<player>();
    for (int i = 0; i < num_players; i++)
    {
        player* pp = World.get_object<player>(i);
        if (pp != current || (!wrecking_crew_player_in_turn() && multi_game_camera_is_running()))
        {
            object_set_visibility_plus_children(pp, false);
            if (spawn != nullptr)
            {
                human_controller_set_pos(pp, spawn->pos);
            }
            vector zero_vel(0.0f, 0.0f, 0.0f);
            human_controller_set_vel(pp, zero_vel);
        }
    }
    if (multi_core_platform_has_pending_invite() && Wrecking_crew_state != WCS_EXITING)
    {
        wrecking_crew_change_state(WCS_EXITING);
    }
    switch (Wrecking_crew_state)
    {
    case WCS_RESTORE_MAP:
        if (!ui_fade_to_black_is_faded())
        {
            break;
        }
        if (Wrecking_crew_restore_map_requested)
        {
            if (Wrecking_crew_options.current_challenge_id != -1)
            {
                const wrecking_crew_challenge_mode* challenge = wrecking_crew_get_challenge_mode_by_index(Wrecking_crew_options.current_challenge_id);
                if (challenge == nullptr)
                {
                    return;
                }
                const wrecking_crew_challenge_mode_round& round = challenge->rounds[Wrecking_crew_current_round];
                Wrecking_crew_options.mode = round.mode;
                Wrecking_crew_options.weapon_index = wrecking_crew_get_weapon_index(round.winfo_handle);
                Wrecking_crew_options.backpack_index = wrecking_crew_get_backpack_index(round.backpack_id);
                Wrecking_crew_options.walker_choice = round.walker_choice;
                Wrecking_crew_options.stress_mode = round.stress_mode;
                Wrecking_crew_options.rounds_to_win = 3;
                Wrecking_crew_options.spawn_barrels = round.barrels;
            }
            explosion_clear_all_pending();
            for (int i = 0; i < Wrecking_crew_mover_handles.size(); i++)
            {
                if (!Wrecking_crew_mover_barrel_flag[i] || Wrecking_crew_options.spawn_barrels)
                {
                    World.restore_object(Wrecking_crew_mover_handles[i]);
                }
                else if (World.object_from_handle<object_mover>(Wrecking_crew_mover_handles[i]) != nullptr)
                {
                    World.destroy_object(Wrecking_crew_mover_handles[i], false);
                }
            }
            if (Wrecking_crew_mode_info[Wrecking_crew_options.mode].map_is_being_restored != nullptr)
            {
                Wrecking_crew_mode_info[Wrecking_crew_options.mode].map_is_being_restored();
            }
            Wrecking_crew_restore_map_requested = false;
            Wrecking_crew_restore_map_delay.set(500);
        }
        else if (!World.restore_is_pending() && Wrecking_crew_restore_map_delay.elapsed())
        {
            ui_fade_to_black_fade_in(true);
            wrecking_crew_change_state(Wrecking_crew_restore_map_state);
            Wrecking_crew_restore_map_state = WCS_NONE;
        }
        break;
    case WCS_CHALLENGE_BEGIN:
        if (wrecking_crew_continue_button_pressed())
        {
            wrecking_crew_change_state(WCS_MATCH_BEGIN);
        }
        break;
    case WCS_MATCH_BEGIN:
        if (wrecking_crew_continue_button_pressed())
        {
            wrecking_crew_change_state(WCS_ROUND_BEGIN);
            if (mode.skip_round_begin)
            {
                wrecking_crew_process();
            }
        }
        break;
    case WCS_CHALLENGE_ROUND_BEGIN:
        if (wrecking_crew_continue_button_pressed())
        {
            wrecking_crew_change_state(WCS_ROUND_BEGIN);
        }
        break;
    case WCS_ROUND_BEGIN:
        if (Check_for_walker_load)
        {
            if (wrecking_crew_walker_loaded())
            {
                wrecking_crew_change_state(WCS_PRE_TURN);
                Check_for_walker_load = false;
            }
            break;
        }
        if (wrecking_crew_veto_weapon_button_pressed())
        {
            wrecking_crew_next_weapon();
            wrecking_crew_next_walker();
        }
        if (wrecking_crew_veto_backpack_button_pressed())
        {
            wrecking_crew_next_backpack();
        }
        if (mode.skip_round_begin || wrecking_crew_continue_button_pressed())
        {
            if (Wrecking_crew_options.mode == WCM_WALKER)
            {
                wrecking_crew_create_walker();
                Check_for_walker_load = true;
            }
            else
            {
                wrecking_crew_change_state(WCS_PRE_TURN);
            }
        }
        break;
    case WCS_PRE_TURN_BREAK:
        if (wrecking_crew_continue_button_pressed())
        {
            wrecking_crew_change_state(WCS_PRE_TURN);
        }
        break;
    case WCS_PRE_TURN:
        if (mode.gameplay_process != nullptr)
        {
            mode.gameplay_process();
        }
        if (!Wrecking_crew_timer.elapsed())
        {
            // Magic number 105: the countdown zooms in from a 105 degree view.
            const float t = 1.0f - Wrecking_crew_timer.percent();
            camera_view_set_fov(t * t * (Wrecking_crew_pre_turn_fov - 105.0f) + 105.0f, -1);
        }
        else
        {
            wrecking_crew_change_state(WCS_PLAYERS_TURN);
            camera_view_set_fov(Wrecking_crew_pre_turn_fov, -1);
        }
        break;
    case WCS_PLAYERS_TURN:
        if (mode.gameplay_process != nullptr)
        {
            mode.gameplay_process();
        }
        Wrecking_crew_turn_time_left = Wrecking_crew_timer.elapsed() ? 0 : Wrecking_crew_timer.time_until();
        if (Wrecking_crew_respawn_timer.elapsed())
        {
            wrecking_crew_respawn_player();
        }
        else if (Wrecking_crew_respawn_timer.valid())
        {
            // The dead player's camera turns towards the body.
            player* pp = player_get_local_player();
            vector body_pos = pp->pos;
            if (human_is_ragdolled(pp))
            {
                havok_ragdoll_get_pos(pp->ragdoll_idx, body_pos, RAGDOLL_BODY);
            }
            vector to_body = body_pos - Rfg_camera.real_pos;
            const float dist = to_body.mag_and_normalize_safe();
            vector off_view = body_pos - (Rfg_camera.real_orient.fvec * dist + Rfg_camera.real_pos);
            off_view.normalize_safe();
            vector axis = off_view ^ to_body;
            axis.normalize();
            float angle = vec_delta_ang(to_body, Rfg_camera.real_orient.fvec, axis);
            if (angle - -Wrecking_crew_death_camera_speed < 0.0f)
            {
                angle = -Wrecking_crew_death_camera_speed;
            }
            if (angle - Wrecking_crew_death_camera_speed >= 0.0f)
            {
                angle = Wrecking_crew_death_camera_speed;
            }
            Rfg_camera.real_orient.rotate_around_local_vector(axis, -angle);
            camera_set_orient(Rfg_camera.real_orient);
        }
        if (mode.get_current_players_score != nullptr)
        {
            Wrecking_crew_player_status[Wrecking_crew_current_player].turn_score = mode.get_current_players_score(Wrecking_crew_turn_time_left);
        }
        if ((!Wrecking_crew_options.free_play && Wrecking_crew_timer.elapsed()) || (mode.is_players_turn_over != nullptr && mode.is_players_turn_over()))
        {
            // Magic numbers: the turn's aftermath runs 5 s, 10 s with building stress on.
            Wrecking_crew_timer.set(Wrecking_crew_options.stress_mode == WCSS_OFF ? 5000 : 10000);
            wrecking_crew_change_state(WCS_PRE_BREAK);
            game_render_set_camera_frame_override_blur(0.0f);
            game_render_set_camera_signal_noise_frequency(0.0f);
            game_render_set_camera_signal_noise_scale(0.0f);
        }
        break;
    case WCS_PRE_BREAK:
    {
        if (mode.gameplay_process != nullptr)
        {
            mode.gameplay_process();
        }
        if (mode.get_current_players_score != nullptr)
        {
            Wrecking_crew_player_status[Wrecking_crew_current_player].turn_score = mode.get_current_players_score(Wrecking_crew_turn_time_left);
        }
        player* pp = player_get_local_player();
        detonate_charges(pp->get_handle());
        // Projectiles still in flight once half the aftermath is over detonate now.
        bool no_projectiles = true;
        const int num_projectiles = World.get_num_objects<projectile>();
        for (int i = 0; i < num_projectiles; i++)
        {
            no_projectiles = false;
            projectile* proj = World.get_object<projectile>(i);
            if (Wrecking_crew_timer.time_since_set() >= Wrecking_crew_timer.time_until())
            {
                proj->set_detonate_time(0, 0xFFFFFFFF);
            }
        }
        if ((pp->backpack == nullptr || !pp->backpack->is_activated()) && no_projectiles && Wrecking_crew_turn_move_on_timer.valid())
        {
            if (Wrecking_crew_turn_move_on_timer.elapsed() && !multi_game_camera_is_running())
            {
                wrecking_crew_respawn_player();
                multi_game_camera_start();
            }
        }
        else
        {
            Wrecking_crew_turn_move_on_timer.set(2000);
        }
        if (Wrecking_crew_timer.elapsed())
        {
            Wrecking_crew_timer.invalidate();
            wrecking_crew_change_state(WCS_BREAK);
        }
        break;
    }
    case WCS_BREAK:
        if (wrecking_crew_continue_button_pressed())
        {
            if (Wrecking_crew_round_high_score < Wrecking_crew_player_status[Wrecking_crew_current_player].round_score)
            {
                Wrecking_crew_round_high_score = Wrecking_crew_player_status[Wrecking_crew_current_player].round_score;
            }
            if (Wrecking_crew_round_winner == -1)
            {
                wrecking_crew_restore_map(WCS_PRE_TURN_BREAK);
            }
            else if (Wrecking_crew_options.current_challenge_id == -1)
            {
                wrecking_crew_change_state(WCS_ROUND_END);
            }
            else
            {
                wrecking_crew_do_round_end();
            }
        }
        break;
    case WCS_ROUND_END:
        if (wrecking_crew_continue_button_pressed())
        {
            wrecking_crew_do_round_end();
        }
        break;
    case WCS_MATCH_END:
        if (wrecking_crew_continue_button_pressed())
        {
            wrecking_crew_change_state(WCS_EXITING);
            wrecking_crew_process();
        }
        break;
    case WCS_EXITING:
        Wrecking_crew_should_restart = !multi_core_platform_has_pending_invite();
        if (Wrecking_crew_options.current_challenge_id != -1)
        {
            Wrecking_crew_options.weapon_index = -1;
            Wrecking_crew_options.backpack_index = -1;
        }
        game_set_wc_shutdown(GS_MAINMENU);
        break;
    default:
        break;
    }
    multi_game_camera_process();
}

// 0x00760140 (:2638): the camera blur of the splash screens, the countdown and the respawn.
void wrecking_crew_handshake()
{
    switch (Wrecking_crew_state)
    {
    case WCS_RESTORE_MAP:
    case WCS_CHALLENGE_BEGIN:
    case WCS_MATCH_BEGIN:
    case WCS_CHALLENGE_ROUND_BEGIN:
    case WCS_ROUND_BEGIN:
    case WCS_PRE_TURN_BREAK:
    case WCS_BREAK:
    case WCS_ROUND_END:
    case WCS_MATCH_END:
    {
        float blur = 1.0f;
        if (Wrecking_crew_splash_blur_timer.valid())
        {
            blur = 1.0f - Wrecking_crew_splash_blur_timer.percent();
        }
        game_render_set_camera_frame_override_blur(blur);
        break;
    }
    case WCS_PRE_TURN:
        if (Wrecking_crew_timer.valid() && !Wrecking_crew_timer.elapsed())
        {
            const float t = 1.0f - Wrecking_crew_timer.percent();
            game_render_set_camera_frame_override_blur(1.0f - t * t);
        }
        else
        {
            game_render_set_camera_frame_override_blur(0.0f);
        }
        break;
    case WCS_PLAYERS_TURN:
        if (Wrecking_crew_respawn_blur_timer.valid() && !Wrecking_crew_respawn_blur_timer.elapsed())
        {
            // Magic numbers: the blur rises over 2 s and falls over the last second.
            float blur = 1.0f;
            if (Wrecking_crew_respawn_blur_timer.time_since_set() < 2000)
            {
                blur = (float)Wrecking_crew_respawn_blur_timer.time_since_set() / 2000.0f;
            }
            else if (Wrecking_crew_respawn_blur_timer.time_until() < 1000)
            {
                blur = (float)Wrecking_crew_respawn_blur_timer.time_until() / 1000.0f;
            }
            game_render_set_camera_frame_override_blur(blur);
            game_render_set_camera_signal_noise_frequency(Wrecking_crew_camera_noise_frequency * blur);
            game_render_set_camera_signal_noise_scale(Wrecking_crew_camera_noise_scale * blur);
        }
        else
        {
            Wrecking_crew_respawn_blur_timer.invalidate();
            game_render_set_camera_frame_override_blur(0.0f);
            game_render_set_camera_signal_noise_frequency(0.0f);
            game_render_set_camera_signal_noise_scale(0.0f);
        }
        break;
    default:
        break;
    }
}

// 0x00760360 (:2703): the mode's render, and the countdown seconds on the HUD.
void wrecking_crew_render()
{
    static int previous_time = -1; // 0x0165A270
    if (const wrecking_crew_mode_info* mode = wc_current_mode())
    {
        if (mode->render != nullptr)
        {
            mode->render(Wrecking_crew_state);
        }
    }
    int time = previous_time;
    if (Wrecking_crew_state == WCS_PRE_TURN && Wrecking_crew_timer.valid() && !Wrecking_crew_timer.elapsed())
    {
        time = Wrecking_crew_timer.time_until() + 999;
        if (previous_time / 1000 != time / 1000)
        {
            wchar_t text[16];
            _snprintf_safe(text, 16, "%d", time / 1000);
            hud_ui_wrecking_crew_show_counter(text, true);
        }
    }
    previous_time = time;
}

// 0x00760440 (:2745)
bool wrecking_crew_is_active()
{
    return Wrecking_crew_state != WCS_NONE;
}

// 0x00760450 (:2752)
wrecking_crew_modes wrecking_crew_get_active_mode()
{
    return Wrecking_crew_state != WCS_NONE ? Wrecking_crew_options.mode : WCM_NONE;
}

// 0x00760470 (:2763)
bool wrecking_crew_challenge_in_progress()
{
    return Wrecking_crew_state != WCS_NONE && Wrecking_crew_options.current_challenge_id != -1;
}

// 0x00760490 (:2772)
int wrecking_crew_get_num_players()
{
    return Wrecking_crew_options.num_players;
}

// 0x00777310 (:2777): a locked map can not stay selected; the selection moves to the next unlocked one.
void wrecking_crew_lock_map(const char* map_name)
{
    wrecking_crew_map_info* info = wrecking_crew_get_map_info(map_name);
    if (info == nullptr)
    {
        return;
    }
    info->locked = true;
    // Original: loops forever when every map is locked.
    while (Wrecking_crew_options.map_index >= 0 && Wrecking_crew_options.map_index < Wrecking_crew_num_maps && Wrecking_crew_map_info[Wrecking_crew_options.map_index]->locked)
    {
        Wrecking_crew_options.map_index++;
        if (Wrecking_crew_num_maps <= Wrecking_crew_options.map_index)
        {
            Wrecking_crew_options.map_index = 0;
        }
    }
}

// 0x007604A0 (:2799)
void wrecking_crew_unlock_map(const char* map_name)
{
    wrecking_crew_map_info* info = wrecking_crew_get_map_info(map_name);
    if (info != nullptr)
    {
        info->locked = false;
    }
}

// 0x007604C0 (:2808)
int wrecking_crew_get_num_maps()
{
    return Wrecking_crew_num_maps;
}

// 0x007604D0 (:2815)
const char* wrecking_crew_get_map_file_name(int map_index)
{
    return map_index >= 0 && map_index < Wrecking_crew_num_maps ? Wrecking_crew_map_info[map_index]->file_name : nullptr;
}

// 0x007604F0 (:2821)
const checksum_stri& wrecking_crew_get_map_file_name_crc(int map_index)
{
    return map_index >= 0 && map_index < Wrecking_crew_num_maps ? Wrecking_crew_map_info[map_index]->file_name_crc : checksum_stri::INVALID;
}

// 0x00760520 (:2829)
const char* wrecking_crew_get_map_display_name(int map_index)
{
    return map_index >= 0 && map_index < Wrecking_crew_num_maps ? Wrecking_crew_map_info[map_index]->display_name : nullptr;
}

// 0x00760540 (:2837)
const char* wrecking_crew_get_map_image_name(int map_index)
{
    return map_index >= 0 && map_index < Wrecking_crew_num_maps ? Wrecking_crew_map_info[map_index]->img_name : nullptr;
}

// 0x00760560 (:2843)
const char* wrecking_crew_get_loading_screen_image_name(int map_index)
{
    return map_index >= 0 && map_index < Wrecking_crew_num_maps ? Wrecking_crew_map_info[map_index]->load_img_name : nullptr;
}

// 0x00760580 (:2849): 0 for an index out of range (not -1).
int wrecking_crew_get_map_challenge_id(int map_index)
{
    return map_index >= 0 && map_index < Wrecking_crew_num_maps ? Wrecking_crew_map_info[map_index]->challenge_id : 0;
}

// 0x007605A0 (:2855)
bool wrecking_crew_is_map_locked(int map_index)
{
    return map_index >= 0 && map_index < Wrecking_crew_num_maps ? Wrecking_crew_map_info[map_index]->locked : false;
}

// 0x007605C0 (:2861): unknown maps count as locked.
bool wrecking_crew_is_map_locked(const char* map_name)
{
    const wrecking_crew_map_info* info = wrecking_crew_get_map_info(map_name);
    return info == nullptr ? true : info->locked;
}

// 0x007605E0 (:2871)
const char* wrecking_crew_get_mode_internal_name(wrecking_crew_modes mode)
{
    return (unsigned int)mode < NUM_WRECKING_CREW_MODES ? Wrecking_crew_mode_info[mode].internal_name : nullptr;
}

// 0x00760600 (:2879)
const char* wrecking_crew_get_mode_display_name(wrecking_crew_modes mode)
{
    return (unsigned int)mode < NUM_WRECKING_CREW_MODES ? Wrecking_crew_mode_info[mode].display_name : nullptr;
}

// 0x00760620 (:2887)
int wrecking_crew_get_num_weapons()
{
    return Wrecking_crew_num_weapons;
}

// 0x00760630 (:2892)
int wrecking_crew_get_weapon_index(unsigned int winfo_handle)
{
    for (int i = 0; i < Wrecking_crew_num_weapons; i++)
    {
        if (Wrecking_crew_weapons[i].w_info_handle == winfo_handle)
        {
            return i;
        }
    }
    return -1;
}

// 0x00760660 (:2904)
const wrecking_crew_weapon_info* wrecking_crew_get_weapon_info(int weapon_index)
{
    return weapon_index >= 0 && weapon_index < Wrecking_crew_num_weapons ? &Wrecking_crew_weapons[weapon_index] : nullptr;
}

// 0x00760680 (:2910)
const wrecking_crew_weapon_info* wrecking_crew_get_current_weapon_info()
{
    return Wrecking_crew_current_weapon;
}

// 0x00760690 (:2915)
bool wrecking_crew_is_weapon_available_for_mode(int weapon_index, wrecking_crew_modes mode)
{
    if (weapon_index >= 0 && weapon_index < Wrecking_crew_num_weapons)
    {
        return (Wrecking_crew_weapons[weapon_index].mode_flags & (1 << (mode & 0x1F))) != 0;
    }
    return false;
}

// 0x007606D0 (:2921): on the round splash, when the weapon is random and the mode has two or more.
bool wrecking_crew_can_veto_weapon()
{
    if (Wrecking_crew_options.weapon_index != -1 || Wrecking_crew_state != WCS_ROUND_BEGIN)
    {
        return false;
    }
    if (Wrecking_crew_options.mode == WCM_WALKER)
    {
        return Wrecking_crew_options.walker_choice == WCWC_NONE;
    }
    bool found_one = false;
    for (int i = Wrecking_crew_num_weapons - 1; i >= 0; i--)
    {
        if (wrecking_crew_is_weapon_available_for_mode(i, Wrecking_crew_options.mode))
        {
            if (found_one)
            {
                return true;
            }
            found_one = true;
        }
    }
    return false;
}

// 0x00760760 (:2958)
int wrecking_crew_get_num_backpacks()
{
    return Wrecking_crew_num_backpacks;
}

// 0x00760770 (:2963)
const wrecking_crew_backpack_info* wrecking_crew_get_backpack_info(int backpack_index)
{
    return backpack_index >= 0 && backpack_index < Wrecking_crew_num_backpacks ? &Wrecking_crew_backpacks[backpack_index] : nullptr;
}

// 0x00760790 (:2969)
int wrecking_crew_get_backpack_index(int backpack_id)
{
    for (int i = 0; i < Wrecking_crew_num_backpacks; i++)
    {
        if (Wrecking_crew_backpacks[i].backpack_type == backpack_id)
        {
            return i;
        }
    }
    return -1;
}

// 0x007607C0 (:2981)
const wrecking_crew_backpack_info* wrecking_crew_get_current_backpack_info()
{
    return Wrecking_crew_current_backpack;
}

// 0x007607D0 (:2986)
bool wrecking_crew_is_backpack_available_for_mode(int backpack_index, wrecking_crew_modes mode)
{
    if (backpack_index >= 0 && backpack_index < Wrecking_crew_num_backpacks)
    {
        return (Wrecking_crew_backpacks[backpack_index].mode_flags & (1 << (mode & 0x1F))) != 0;
    }
    return false;
}

// 0x00760810 (:2992)
bool wrecking_crew_can_veto_backpack()
{
    if (Wrecking_crew_options.backpack_index != -1 || Wrecking_crew_state != WCS_ROUND_BEGIN)
    {
        return false;
    }
    bool found_one = false;
    for (int i = Wrecking_crew_num_backpacks - 1; i >= 0; i--)
    {
        if (wrecking_crew_is_backpack_available_for_mode(i, Wrecking_crew_options.mode))
        {
            if (found_one)
            {
                return true;
            }
            found_one = true;
        }
    }
    return false;
}

// 0x00760880 (:3019)
int wrecking_crew_get_walker_vehicle_type(wrecking_crew_walker_choices choice)
{
    return (unsigned int)choice < NUM_WRECKING_CREW_WALKER_CHOICES ? Wrecking_crew_walkers[choice].vehicle_type : -1;
}

// 0x007608A0 (:3027)
const wrecking_crew_player_status* wrecking_crew_get_player_status(int player_index)
{
    return (unsigned int)player_index < 4 ? &Wrecking_crew_player_status[player_index] : nullptr;
}

// 0x007608C0 (:3036)
void wrecking_crew_enable_player(int player_index, bool enable)
{
    if ((unsigned int)player_index < 4)
    {
        Wrecking_crew_player_status[player_index].enabled = enable;
    }
}

// 0x00777360 (:3042): the player's character name.
void wrecking_crew_get_player_name(int player_index, wchar_t* buffer, int buffer_len)
{
    buffer[0] = L'\0';
    if (player_index < 0 || player_index >= Wrecking_crew_options.num_players)
    {
        return;
    }
    // Original: the "player N" tag is built and never used. "WRECKINC" is the tags' spelling.
    char tag[64];
    switch (player_index)
    {
    case 0:
        _snprintf_safe(tag, 64, "MENU_WRECKINC_CREW_PLAYER_ONE");
        break;
    case 1:
        _snprintf_safe(tag, 64, "MENU_WRECKINC_CREW_PLAYER_TWO");
        break;
    case 2:
        _snprintf_safe(tag, 64, "MENU_WRECKINC_CREW_PLAYER_THREE");
        break;
    case 3:
        _snprintf_safe(tag, 64, "MENU_WRECKINC_CREW_PLAYER_FOUR");
        break;
    default:
        keen::debug::nativePrintArgs("Unhandled case");
        break;
    }
    wcsncpy(buffer, localization_get_display_string_req(Wrecking_crew_characters[Wc_player_infos[player_index].character_index]->display_name, nullptr), buffer_len);
    buffer[buffer_len - 1] = L'\0';
}

// 0x00777460 (:3075): the score line of a player on the HUD: the score (a challenge's total), "playing" during the
// turn, and the round wins (or "total" at a challenge's end).
void wrecking_crew_get_player_description(int player_index, wchar_t* buffer, int buffer_len)
{
    buffer[0] = L'\0';
    const wrecking_crew_modes mode_index = Wrecking_crew_options.mode;
    if (player_index < 0 || player_index >= Wrecking_crew_options.num_players || wc_current_mode() == nullptr || player_index > 3)
    {
        return;
    }
    const wrecking_crew_mode_info& mode = Wrecking_crew_mode_info[mode_index];
    if (mode.get_player_description != nullptr)
    {
        mode.get_player_description(player_index, buffer, buffer_len);
    }
    else
    {
        const wrecking_crew_player_status* status = wrecking_crew_get_player_status(player_index);
        if (status == nullptr)
        {
            return;
        }
        if (!wrecking_crew_is_players_turn(player_index))
        {
            if (Wrecking_crew_options.current_challenge_id != -1)
            {
                const float* scores = Challenge_player_round_scores[player_index];
                _snprintf_safe(buffer, buffer_len, "%d", wrecking_crew_convert_score(scores[0] + scores[1] + scores[2]));
            }
            else
            {
                _snprintf_safe(buffer, buffer_len, "%d", wrecking_crew_convert_score(status->turn_score));
            }
        }
    }
    wchar_t text[64];
    if (wrecking_crew_is_players_turn(player_index))
    {
        const char* format = buffer[0] == L'\0' ? "%ls" : " %ls";
        _snprintf_safe(text, 32, format, localization_get_display_string_req("MENU_WORD_PLAYING", nullptr));
        strcat(buffer, text);
    }
    if (mode.get_player_description != nullptr)
    {
        return;
    }
    text[0] = L'\0';
    if (Wrecking_crew_state == WCS_MATCH_END && Wrecking_crew_options.current_challenge_id != -1)
    {
        _snprintf_safe(text, 64, " %ls", localization_get_display_string_req("MENU_WC_TOTAL", nullptr));
    }
    else if (Wrecking_crew_options.current_challenge_id == -1)
    {
        const wrecking_crew_player_status* status = wrecking_crew_get_player_status(player_index);
        if (status == nullptr)
        {
            return;
        }
        _snprintf_safe(text, 64, "  [image:%ls]%d", L"ui_map_icon_star_white", status->round_wins);
    }
    strcat(buffer, text);
}

// 0x007776A0 (:3138): the turn's score, and the round's high score once two players had a turn (green when beaten).
void wrecking_crew_get_score_text(hud_wrecking_crew_text_set* score_set, hud_wrecking_crew_text_set* high_score_set, bool* green_score)
{
    score_set->should_display = false;
    high_score_set->should_display = false;
    *green_score = false;
    const int player_index = Wrecking_crew_current_player;
    if ((unsigned int)player_index >= 4 || wc_current_mode() == nullptr)
    {
        return;
    }
    const wrecking_crew_mode_info& mode = Wrecking_crew_mode_info[Wrecking_crew_options.mode];
    if (mode.get_score_text != nullptr)
    {
        mode.get_score_text(score_set, high_score_set, green_score);
        return;
    }
    score_set->should_display = true;
    const int score = wrecking_crew_convert_score(Wrecking_crew_player_status[player_index].turn_score);
    wcsncpy(score_set->label, localization_get_display_string_req("MENU_SCORE", nullptr), 32);
    score_set->label[31] = L'\0';
    _snprintf_safe(score_set->text, 32, "%d", score);
    int num_had_turn = 0;
    for (int i = 0; i < Wrecking_crew_options.num_players; i++)
    {
        if (wrecking_crew_get_player_status(i)->had_a_turn_for_current_round)
        {
            num_had_turn++;
        }
    }
    int high_score = 0;
    if (num_had_turn > 1)
    {
        high_score = wrecking_crew_convert_score(Wrecking_crew_round_high_score);
        high_score_set->should_display = true;
        strncpy_safe(high_score_set->label, localization_get_display_string_req("HM_HIGH_SCORE", nullptr), 32);
        _snprintf_safe(high_score_set->text, 32, "%d", high_score);
    }
    *green_score = score > high_score;
}

// 0x007608E0 (:3188)
void wrecking_crew_get_weapon_text(wchar_t* buffer, int buffer_len, bool* override_icon, char* icon, int icon_buffer_len)
{
    buffer[0] = L'\0';
    *override_icon = false;
    const wrecking_crew_mode_info* mode = wc_current_mode();
    if (mode != nullptr && mode->get_weapon_text != nullptr)
    {
        mode->get_weapon_text(buffer, buffer_len, override_icon, icon, icon_buffer_len);
    }
}

// 0x00760930 (:3205)
void wrecking_crew_get_backpack_text(wchar_t* buffer, int buffer_len, bool* override_icon, char* icon, int icon_buffer_len)
{
    buffer[0] = L'\0';
    *override_icon = false;
    const wrecking_crew_mode_info* mode = wc_current_mode();
    if (mode != nullptr && mode->get_backpack_text != nullptr)
    {
        mode->get_backpack_text(buffer, buffer_len, override_icon, icon, icon_buffer_len);
    }
}

// 0x00760980 (:3222): a line of the mode's description; untranslated texts are shown as written.
bool wrecking_crew_get_info_text(int info_entry, wchar_t* buffer, int buffer_len)
{
    buffer[0] = L'\0';
    if (Wrecking_crew_options.mode == WCM_NONE || (int)Wrecking_crew_options.mode > WCM_WALKER || (unsigned int)info_entry > 2)
    {
        return false;
    }
    const char* tag = Wrecking_crew_mode_info[Wrecking_crew_options.mode].info_text[info_entry];
    if (tag == nullptr)
    {
        return false;
    }
    if (localization_does_tag_exist(tag))
    {
        strncpy_safe(buffer, localization_get_display_string_req(tag, nullptr), buffer_len);
        return true;
    }
    copy_to_wide(buffer, tag, buffer_len);
    return true;
}

// 0x007777F0 (:3247): the break and round end texts: score, letters and the message ("you're out", "won the round",
// "won the match").
void wrecking_crew_get_turn_end_text(hud_wrecking_crew_text_set* score_set, hud_wrecking_crew_text_set* high_score_set, bool* green_score, hud_wrecking_crew_text_set* letters_set, hud_wrecking_crew_text_set* message_set)
{
    score_set->should_display = false;
    high_score_set->should_display = false;
    letters_set->should_display = false;
    message_set->should_display = false;
    *green_score = false;
    const int player_index = Wrecking_crew_current_player;
    const wrecking_crew_modes mode_index = Wrecking_crew_options.mode;
    if (wc_current_mode() == nullptr || (unsigned int)player_index >= 4)
    {
        return;
    }
    const wrecking_crew_mode_info& mode = Wrecking_crew_mode_info[mode_index];
    if (mode.get_turn_end_score_text == nullptr)
    {
        wrecking_crew_get_score_text(score_set, high_score_set, green_score);
    }
    else
    {
        mode.get_turn_end_score_text(score_set, high_score_set, green_score);
    }
    if (mode.get_turn_end_letters_text != nullptr)
    {
        mode.get_turn_end_letters_text(letters_set);
    }
    int winner = Wrecking_crew_match_winner;
    const int round_winner = Wrecking_crew_round_winner;
    if (mode.get_turn_end_message_text != nullptr)
    {
        mode.get_turn_end_message_text(message_set);
        return;
    }
    const wchar_t* message;
    if (!Wrecking_crew_player_status[player_index].enabled)
    {
        if (Wrecking_crew_state == WCS_ROUND_END)
        {
            message_set->should_display = true;
            wrecking_crew_get_player_name(Wrecking_crew_current_player, message_set->label, 32);
            strncpy_safe(message_set->text, localization_get_display_string_req("MENU_YOURE_OUT", nullptr), 32);
            return;
        }
        if (Wrecking_crew_state != WCS_MATCH_END)
        {
            return;
        }
        message = localization_get_display_string_req("MENU_WON_THE_MATCH", nullptr);
    }
    else if (Wrecking_crew_state == WCS_ROUND_END)
    {
        if (!mode.display_round_win_message)
        {
            return;
        }
        message = localization_get_display_string_req("MENU_WON_THE_ROUND", nullptr);
        winner = round_winner;
    }
    else
    {
        if (Wrecking_crew_state != WCS_MATCH_END)
        {
            return;
        }
        message = localization_get_display_string_req("MENU_WON_THE_MATCH", nullptr);
    }
    strncpy_safe(message_set->text, message, 32);
    if (winner != -1)
    {
        message_set->should_display = true;
        wrecking_crew_get_player_name(winner, message_set->label, 32);
    }
}

// 0x00760A00 (:3302)
void wrecking_crew_get_barrel_text(hud_wrecking_crew_barrel_text_set* barrel_set)
{
    barrel_set->should_display = false;
    const wrecking_crew_mode_info* mode = wc_current_mode();
    if (mode != nullptr && mode->get_barrel_text != nullptr)
    {
        mode->get_barrel_text(barrel_set);
    }
}

// 0x00760A30 (:3318): false when the mode leaves the veto texts to the HUD.
bool wrecking_crew_get_veto_text(hud_wrecking_crew_text_set* weapon_set, bool* can_veto_weapon, hud_wrecking_crew_text_set* backpack_set, bool* can_veto_backpack)
{
    weapon_set->should_display = false;
    backpack_set->should_display = false;
    *can_veto_weapon = false;
    *can_veto_backpack = false;
    const wrecking_crew_mode_info* mode = wc_current_mode();
    if (mode != nullptr && mode->get_veto_text != nullptr)
    {
        mode->get_veto_text(weapon_set, can_veto_weapon, backpack_set, can_veto_backpack);
        return true;
    }
    return false;
}

// 0x00760A80 (:3338): pulse on an attack.
hud_wrecking_crew_icon_state wrecking_crew_get_weapon_icon_state()
{
    const wrecking_crew_mode_info* mode = wc_current_mode();
    if (mode == nullptr)
    {
        return HWCIS_NORMAL;
    }
    if (mode->get_weapon_icon_state != nullptr)
    {
        const hud_wrecking_crew_icon_state state = mode->get_weapon_icon_state();
        if (state != HWCIS_NORMAL)
        {
            return state;
        }
    }
    return Wrecking_crew_player_attacked_this_frame ? HWCIS_PULSE : HWCIS_NORMAL;
}

// 0x00760AC0 (:3360): pulse on an activation.
hud_wrecking_crew_icon_state wrecking_crew_get_backpack_icon_state()
{
    const wrecking_crew_mode_info* mode = wc_current_mode();
    if (mode == nullptr)
    {
        return HWCIS_NORMAL;
    }
    if (mode->get_backpack_icon_state != nullptr)
    {
        const hud_wrecking_crew_icon_state state = mode->get_backpack_icon_state();
        if (state != HWCIS_NORMAL)
        {
            return state;
        }
    }
    return Wrecking_crew_player_activated_backpack_this_frame ? HWCIS_PULSE : HWCIS_NORMAL;
}

// 0x00760B00 (:3382): pulse with the weapon or backpack (modes without a time icon), flash in the last quarter of the
// turn.
hud_wrecking_crew_icon_state wrecking_crew_get_time_icon_state()
{
    const wrecking_crew_mode_info* mode = wc_current_mode();
    if (mode == nullptr)
    {
        return HWCIS_NORMAL;
    }
    if (mode->get_time_icon_state == nullptr)
    {
        if (wrecking_crew_get_weapon_icon_state() == HWCIS_PULSE || wrecking_crew_get_backpack_icon_state() == HWCIS_PULSE)
        {
            return HWCIS_PULSE;
        }
    }
    else
    {
        const hud_wrecking_crew_icon_state state = mode->get_time_icon_state();
        if (state != HWCIS_NORMAL)
        {
            return state;
        }
    }
    if (Wrecking_crew_state == WCS_PLAYERS_TURN && Wrecking_crew_timer.valid() && !Wrecking_crew_timer.elapsed() && Wrecking_crew_timer.percent() <= 0.25f)
    {
        return HWCIS_FLASH;
    }
    return HWCIS_NORMAL;
}

// 0x0079F1A0 (:3414): one hidden player object per player, of the chosen character, at the spawn point.
void wrecking_crew_spawn_players()
{
    if (Wrecking_crew_players[0] != nullptr)
    {
        return;
    }
    object* spawn = World.get_object<player_start>(Wrecking_crew_spawn_point);
    if (spawn == nullptr)
    {
        return;
    }
    for (int i = 0; i < Wrecking_crew_options.num_players; i++)
    {
        const human_info* h_info = Wrecking_crew_characters[Wc_player_infos[i].character_index]->hip;
        if (h_info == nullptr)
        {
            h_info = human_info_find("rfg_PC");
        }
        obj_human_props props(h_info, spawn->pos, spawn->orient, nullptr);
        player* pp = World.create_object<player>(props, 0xFFFFFFFF);
        Wrecking_crew_players[i] = pp;
        human_set_state(pp, HS_STANDARD, HMS_IDLE, -1.0f);
        wrecking_crew_hide_player(pp);
    }
}

// 0x00760BA0 (:3444)
player* wrecking_crew_get_current_player()
{
    if (Wrecking_crew_current_player >= 0 && Wrecking_crew_current_player < Wrecking_crew_options.num_players)
    {
        return Wrecking_crew_players[Wrecking_crew_current_player];
    }
    return Wrecking_crew_players[0];
}

// 0x0079F290 (:3454): the current player becomes the local player and respawns at the spawn point; the others hide.
void wrecking_crew_respawn_player()
{
    object* spawn = World.get_object<player_start>(Wrecking_crew_spawn_point);
    if (spawn == nullptr)
    {
        return;
    }
    player_set_next_active_wrecking_crew_player();
    player* local_player = player_get_local_player();
    if (local_player == nullptr || Wrecking_crew_current_player < 0)
    {
        return;
    }
    const int num_players = World.get_num_objects<player>();
    for (int i = 0; i < num_players; i++)
    {
        player* pp = World.get_object<player>(i);
        if (pp == local_player && wrecking_crew_player_in_turn())
        {
            human_update_collision_filter(pp, true);
            pp->hflags.hidden = 0;
            object_set_visibility_plus_children(pp, true);
        }
        else
        {
            wrecking_crew_hide_player(pp);
        }
    }
    player_respawn(local_player, Wrecking_crew_spawn_point, true);
    camera_set_orient(spawn->orient);
    if (camera_get_mode() == CAMERA_DUMB_MODE)
    {
        camera_stop_dumb_mode();
        camera_start_third_person(false);
    }
    player_reequip(local_player);
    Wrecking_crew_respawn_timer.invalidate();
    const wrecking_crew_mode_info& mode = Wrecking_crew_mode_info[Wrecking_crew_options.mode];
    if (mode.player_respawned != nullptr)
    {
        mode.player_respawned();
    }
}

// 0x00760BC0 (:3497)
int wrecking_crew_get_round_winner()
{
    return Wrecking_crew_round_winner;
}

// 0x00760BD0 (:3502)
int wrecking_crew_get_match_winner()
{
    return Wrecking_crew_match_winner;
}

// 0x00760BE0 (:3507)
float wrecking_crew_get_round_high_score()
{
    return Wrecking_crew_round_high_score;
}

// 0x00760BF0 (:3512): 0 outside the turn and its aftermath.
int wrecking_crew_get_turn_time_left()
{
    return (unsigned int)(Wrecking_crew_state - WCS_PLAYERS_TURN) <= 2 ? Wrecking_crew_turn_time_left : 0;
}

// 0x00760C10 (:3519)
wrecking_crew_states wrecking_crew_get_state()
{
    return Wrecking_crew_state;
}

// 0x00760C20 (:3524)
bool wrecking_crew_player_attacked_this_frame()
{
    return Wrecking_crew_player_attacked_this_frame;
}

// 0x00760C30 (:3529)
bool wrecking_crew_player_activated_backpack_this_frame()
{
    return Wrecking_crew_player_activated_backpack_this_frame;
}

// 0x00760C40 (:3536): always outside the mode (multiplayer's jump and jetpack depend on it).
bool wrecking_crew_can_player_move()
{
    return Wrecking_crew_state == WCS_NONE || Wrecking_crew_state == WCS_PLAYERS_TURN;
}

// 0x00760C60 (:3550): during the turn and its aftermath (not in a walker's aftermath).
bool wrecking_crew_can_player_move_camera()
{
    if (Wrecking_crew_state == WCS_NONE)
    {
        return true;
    }
    if (Wrecking_crew_options.mode == WCM_WALKER && Wrecking_crew_state == WCS_PRE_BREAK)
    {
        return false;
    }
    return Wrecking_crew_state == WCS_PLAYERS_TURN || Wrecking_crew_state == WCS_PRE_BREAK;
}

// 0x00760C90 (:3571): during the turn only, and melee only with melee weapons; then the mode decides.
bool wrecking_crew_can_player_attack(const weapon_info* w_info, damaged_by_types attack_type)
{
    if (Wrecking_crew_state == WCS_NONE)
    {
        return true;
    }
    if (Wrecking_crew_state != WCS_PLAYERS_TURN || (w_info != nullptr && w_info->ammo_type != WEAPON_AMMO_TYPE_MELEE && attack_type == DBT_MELEE))
    {
        return false;
    }
    // Original: the mode index is not range checked here.
    const wrecking_crew_mode_info& mode = Wrecking_crew_mode_info[Wrecking_crew_options.mode];
    if (mode.can_player_attack != nullptr && !mode.can_player_attack(w_info, attack_type))
    {
        return false;
    }
    return true;
}

// 0x00760CF0 (:3600)
bool wrecking_crew_can_player_activate_backpack(const multi_backpack_info* b_info)
{
    return Wrecking_crew_state == WCS_NONE || Wrecking_crew_state == WCS_PLAYERS_TURN;
}

// 0x00760D10 (:3614)
bool wrecking_crew_should_run_stress()
{
    return Wrecking_crew_state != WCS_NONE && Wrecking_crew_options.stress_mode != WCSS_OFF;
}

// 0x00760D30 (:3623)
bool wrecking_crew_has_unlimited_magazine_ammo()
{
    return Wrecking_crew_state != WCS_NONE && Wrecking_crew_unlimited_magazine_ammo;
}

// 0x00760D50 (:3632)
bool wrecking_crew_has_unlimited_reserve_ammo()
{
    return Wrecking_crew_state != WCS_NONE && Wrecking_crew_unlimited_reserve_ammo;
}

// 0x00777960 (:3643): the round's weapon (the sledgehammer while the player is not streamed in yet, or without one)
// with the mode's ammo, and the round's backpack.
void wrecking_crew_set_default_inventory(player* pp)
{
    if (pp == nullptr)
    {
        return;
    }
    human_delete_all_items_from_inventory(pp, false, false);
    const wrecking_crew_weapon_info* weapon_entry = Wrecking_crew_current_weapon;
    if (weapon_entry == nullptr || !pp->obj_flags.streamed)
    {
        for (int i = 0; i < Wrecking_crew_num_weapons; i++)
        {
            const weapon_info* w_info = weapons_get_weapon_info(Wrecking_crew_weapons[i].w_info_handle);
            if (w_info == nullptr)
            {
                return;
            }
            if (w_info->weapon_class == WEAPON_CLASS_SLEDGEHAMMER)
            {
                weapon_entry = &Wrecking_crew_weapons[i];
                break;
            }
        }
    }
    if (weapon_entry == nullptr)
    {
        return;
    }
    const weapon_info* w_info = weapons_get_weapon_info(weapon_entry->w_info_handle);
    if (w_info == nullptr)
    {
        return;
    }
    inventory_item* iip = human_add_item_to_inventory(pp, w_info->weapon_inv_item_info, 0, -1, -1, true, false, true);
    if (iip == nullptr)
    {
        return;
    }
    human_equip_inventory_item(pp, iip);
    weapon* wp = World.object_from_handle<weapon>(iip->weapon_handle);
    if (wp == nullptr)
    {
        return;
    }
    wrecking_crew_set_weapon_ammo(wp);
    if (!pp->obj_flags.streamed)
    {
        return;
    }
    if (pp->backpack != nullptr)
    {
        pp->backpack->event_player_dropped(true);
    }
    if (Wrecking_crew_current_backpack != nullptr)
    {
        multi_object_backpack* backpack = multi_game_backpack_create(Wrecking_crew_current_backpack->backpack_type, pp->pos, pp->orient);
        if (backpack != nullptr)
        {
            backpack->event_player_picked_up(pp, true, false);
        }
    }
}

// 0x00760D70 (:3703): mass destroyed during the turn; modes without their own score count it as the score.
void wrecking_crew_register_building_damage(const rfg_mover* mover, float mass_delta)
{
    const int player_index = Wrecking_crew_current_player;
    const wrecking_crew_modes mode_index = Wrecking_crew_options.mode;
    if (Wrecking_crew_state == WCS_NONE || player_index == -1 || (Wrecking_crew_state != WCS_PLAYERS_TURN && Wrecking_crew_state != WCS_PRE_BREAK) || mover == nullptr || mass_delta >= 0.0f)
    {
        return;
    }
    wrecking_crew_player_status& status = Wrecking_crew_player_status[player_index];
    status.turn_destruction_done = (float)(mass_delta * -1.0 + status.turn_destruction_done);
    if ((unsigned int)mode_index < NUM_WRECKING_CREW_MODES && Wrecking_crew_mode_info[mode_index].get_current_players_score == nullptr)
    {
        // Magic number 0.0003: score points per kilogram destroyed.
        status.turn_score = (float)(mass_delta * -1.0 * 0.0003 + status.turn_score);
    }
}

// 0x00760E40 (:3745)
void wrecking_crew_mover_destroyed(const object_mover* mover)
{
    if (Wrecking_crew_state == WCS_NONE || mover == nullptr)
    {
        return;
    }
    const wrecking_crew_mode_info* mode = wc_current_mode();
    if (mode != nullptr && mode->mover_destroyed != nullptr)
    {
        mode->mover_destroyed(mover);
    }
}

// 0x00760E80 (:3768)
void wrecking_crew_register_attack(const weapon_info* w_info, damaged_by_types attack_type, bool detonating_charges)
{
    if (Wrecking_crew_state == WCS_NONE || Wrecking_crew_current_player == -1 || attack_type == DBT_NONE || detonating_charges || w_info == nullptr)
    {
        return;
    }
    const wrecking_crew_mode_info* mode = wrecking_crew_get_mode_info(Wrecking_crew_options.mode);
    if (mode == nullptr)
    {
        return;
    }
    if (mode->player_attacked != nullptr)
    {
        mode->player_attacked(w_info, attack_type);
    }
    Wrecking_crew_player_attacked_this_frame = true;
}

// 0x00760ED0 (:3794)
void wrecking_crew_register_backpack_activation()
{
    if (Wrecking_crew_state == WCS_NONE || Wrecking_crew_current_player == -1)
    {
        return;
    }
    const wrecking_crew_mode_info* mode = wc_current_mode();
    if (mode == nullptr)
    {
        return;
    }
    if (mode->player_activated_backpack != nullptr)
    {
        mode->player_activated_backpack();
    }
    Wrecking_crew_player_activated_backpack_this_frame = true;
}

// 0x00760F10 (:3815): the death camera and blur run until the respawn 2.5 s later.
void wrecking_crew_player_died(player* pp)
{
    if (Wrecking_crew_state == WCS_NONE || Wrecking_crew_current_player == -1 || pp != player_get_local_player())
    {
        return;
    }
    camera_start_dumb_mode();
    Wrecking_crew_respawn_timer.set(2500);
    Wrecking_crew_respawn_blur_timer.set(3500);
    multi_object_backpack* backpack = pp->backpack;
    if (backpack != nullptr && backpack->is_activated())
    {
        backpack->toggle_activation(0);
    }
}

// 0x00760F70 (:3841)
bool wrecking_crew_should_restart()
{
    return Wrecking_crew_should_restart;
}

// 0x00760F80 (:3846)
void wrecking_crew_should_restart(bool restart)
{
    Wrecking_crew_should_restart = restart;
}

// 0x00760F90 (:3853): back on the main menu after a match, open the Wrecking Crew menu again.
void wrecking_crew_restart()
{
    if (gameseq_get_state() == GS_MAINMENU && Wrecking_crew_should_restart)
    {
        Wrecking_crew_should_restart = false;
        Wrecking_crew_restarted = true;
        ui_wrecking_crew_show();
    }
}

// 0x00760FC0 (:3864)
bool wrecking_crew_is_map_valid(const char* map_name)
{
    return wrecking_crew_get_map_info(map_name) != nullptr;
}

// 0x00760FE0 (:3871)
int wrecking_crew_get_num_challenge_modes()
{
    return Num_challenge_modes;
}

// 0x00760FF0 (:3879)
const wrecking_crew_challenge_mode* wrecking_crew_get_challenge_mode_by_index(int index)
{
    return index < Num_challenge_modes && index >= 0 ? &Challenge_modes[index] : nullptr;
}

// 0x00761010 (:3887)
bool wrecking_crew_player_can_veto()
{
    return Wrecking_crew_current_player == Wrecking_crew_veto_player;
}

// 0x00761020 (:3895)
void wrecking_crew_force_session_end()
{
}
