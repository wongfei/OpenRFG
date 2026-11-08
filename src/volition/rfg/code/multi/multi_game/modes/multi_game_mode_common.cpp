// Original: volition/rfg/code/multi/multi_game/modes/multi_game_mode_common.cpp
//
// The match flow shared by the modes: the match state machine (pregame, countdown, gameplay, endgame sync, results),
// the mode table parse (mp_modes.xtbl), match and round begin, the round-over checks, the PT_MODE_COMMON packets, the
// team balance queries and the common mode hooks the mode table falls back to.
// Ported in multiplayer.md MP4, MP6, MP9 and stub_impl.md S6 (the migration hooks, the rack popup render, the ranking
// board report, the experience events of the kill and match end, the kill streak message, the score logs).
// Part of the multi unity TU (rfg_multi_unity.obj, every multi/ .cpp in path order): the file statics are shared
// by the whole tree, and a shared one is placed in its first user (tools/hdrgen/specs/mp0_stubs.py).

#include "volition/rfg/code/multi/multi_game/modes/multi_game_mode_common.h"
#include "volition/rfg/code/multi/multi_game/modes/multi_game_mode_spectator.h"
#include "volition/rfg/code/multi/multi_game/multi_game.h"
#include "volition/rfg/code/multi/multi_game/multi_game_announcer.h"
#include "volition/rfg/code/multi/multi_game/multi_game_backpack_rack.h"
#include "volition/rfg/code/multi/multi_game/multi_game_camera.h"
#include "volition/rfg/code/multi/multi_game/multi_game_data.h"
#include "volition/rfg/code/multi/multi_game/multi_game_io.h"
#include "volition/rfg/code/multi/multi_game/multi_game_lobby.h"
#include "volition/rfg/code/multi/multi_game/multi_game_object_backpack.h"
#include "volition/rfg/code/multi/multi_game/multi_game_object_marker.h"
#include "volition/rfg/code/multi/multi_game/multi_game_player.h"
#include "volition/rfg/code/multi/multi_game/multi_game_render.h"
#include "volition/rfg/code/multi/multi_game/multi_game_respawn.h"
#include "volition/rfg/code/multi/multi_game/multi_game_rubberbanding.h"
#include "volition/rfg/code/multi/multi_game/multi_game_settings.h"
#include "volition/rfg/code/multi/multi_game/multi_game_stats.h"
#include "volition/rfg/code/multi/multi_game/multi_game_timer.h"
#include "volition/rfg/code/multi/multi_game/multi_game_xp.h"
#include "volition/rfg/code/multi/multi_game/multi_gameplay_mode.h"
#include "volition/rfg/code/multi/multi_core/include/multi.h"
#include "volition/rfg/code/multi/multi_core/include/multi_connection.h"
#include "volition/rfg/code/multi/multi_core/include/multi_core_game_bandwidth.h"
#include "volition/rfg/code/multi/multi_core/include/multi_core_io.h"
#include "volition/rfg/code/multi/multi_core/include/multi_core_objects.h"
#include "volition/rfg/code/multi/multi_core/include/multi_core_packet_types.h"
#include "volition/rfg/code/multi/multi_core/include/multi_core_platform.h"
#include "volition/rfg/code/multi/multi_core/include/multi_core_qos.h"
#include "volition/rfg/code/multi/multi_game/multi_game_destruction.h"
#include "volition/rfg/code/effects/object_effect.h"
#include "volition/rfg/code/hud/hud.h"
#include "volition/rfg/code/objects/object_item.h"
#include "volition/rfg/code/interface/multi_player_menus/multi_player_menu_system.h"
#include "volition/rfg/code/rfg.h"
#include "unit4/gamesession/localgamesession.hpp"
#include "volition/rfg/code/multi/multi_core/include/multi_player_uid.h"
#include "volition/rfg/code/multi/multi_core/include/multi_session.h"
#include "volition/rfg/code/code_effects/dust.h"
#include "volition/rfg/code/code_effects/explosion.h"
#include "volition/rfg/code/objects/humans/human.h"
#include "volition/rfg/code/objects/humans/human_combat.h"
#include "volition/rfg/code/objects/humans/human_teams.h"
#include "volition/vlib/code/math/floating.h"
#include "volition/vlib/code/math/vector3.h"
#include "volition/rfg/code/destruction/rfg_dynamics.h"
#include "volition/rfg/code/event_logging/definitions_events/events.h"
#include "volition/rfg/code/event_logging/gamelog.h"
#include "volition/audio_lib/code/audiolib_api.h"
#include "volition/rfg/code/gamewide/game_clock.h"
#include "volition/rfg/code/gamewide/gameaudio.h"
#include "volition/rfg/code/gamewide/gamewide.h"
#include "volition/rfg/code/gamewide/gameseq.h"
#include "volition/rfg/code/gamewide/stats.h"
#include "volition/rfg/code/gamewide/tweak_table.h"
#include "volition/rfg/code/interface/localization/localization_shared.h"
#include "volition/rfg/code/interface/ui_document_hud_multiplayer.h"
#include "volition/rfg/code/interface/ui_message_box.h"
#include "volition/rfg/code/level/world.h"
#include "volition/rfg/code/objects/humans/human_voice/human_voice.h"
#include "volition/rfg/code/objects/humans/player.h"
#include "volition/rfg/code/objects/object.h"
#include "volition/rfg/code/objects/object_player_start.h"
#include "volition/rfg/code/objects/projectile.h"
#include "volition/rfg/code/weapons/weapons.h"
#include "volition/rfg/code/weather/wind.h"
#include "volition/interface_lib/code/bitmap_sheets.h"
#include "volition/interface_lib/code/vint_data_group.h"
#include "volition/interface_lib/code/vint_data_event.h"
#include "volition/interface_lib/code/vint_data_item.h"
#include "volition/interface_lib/code/vint_variant.h"
#include "volition/interface_lib/code/vint_external.h"
#include "volition/vlib/code/math/rand.h"
#include "volition/vlib/code/os/vconsole.h"
#include "volition/vlib/code/parse/xml_table_tools.h"
#include "volition/vlib/code/util/string_util.h"
#include "volition/vlib/code/util/unicode_tools.h"
#include "volition/vlogger_lib/include/logger.h"
#include "keen/base/trace.hpp"
#include "unit4/systemservices/systemservices.hpp"

#include <math.h>
#include <string.h>

// ---- file-private types (rfg.pdb)
// size 0x8
struct time_left_announcement
{
    multi_announcer_lines line; // 0x00
    int time_left_ms; // 0x04
};

// size 0x8
struct ui_match_event
{
    const char* name; // 0x00
    vint_data_event* event; // 0x04
};

enum multi_camera_view_modes
{
    MCVM_DUMB = 0,
    MCVM_MANUAL = 1,
    MCVM_STATIC_TRACKING = 2,
    MCVM_STATIC_PAN = 3,
    MCVM_LAGGING_TRACKING = 4,
    MCVM_TARGET_POV = 5,
    MCVM_TARGET_REVERSE_POV = 6,
    MCVM_TARGET_TRACKING = 7,
    NUM_MULTI_CAMERA_VIEW_MODES = 8,
};

// size 0x14
struct backpack_rack_popup
{
    backpack_rack_popup(); // 0x0094F1A0

    multi_object_marker* rack; // 0x00
    timestamp_percent fade_in_timer; // 0x04
    timestamp_percent fade_out_timer; // 0x0C
};

// 0x0094F1A0: the timers start invalid (their constructors)
backpack_rack_popup::backpack_rack_popup()
{
}

// size 0x8
struct tracked_level_reference
{
    const multi_level_info* l_info; // 0x00
    tracked_stats_indices games_played_stat; // 0x04
};

// Moved here from multi_gameplay_mode.cpp (port): this file indexes the mode table before multi_gameplay_mode.cpp
// defines it in the unity TU, so the row type lives with its first user.
// size 0xB8
struct multi_gameplay_mode_funcs
{
    void (*round_begin_jip)(multi_connection*); // 0x00
    void (*memory_init)(mempool_base&); // 0x04
    void (*memory_shutdown)(); // 0x08
    void (*level_init_pre)(); // 0x0C
    void (*level_init_post)(); // 0x10
    void (*level_shutdown)(); // 0x14
    void (*enter)(); // 0x18
    void (*exit)(); // 0x1C
    void (*process)(); // 0x20
    void (*handshake)(); // 0x24
    void (*render)(); // 0x28
    void (*add_player)(player*); // 0x2C
    void (*remove_player)(player*); // 0x30
    void (*player_died)(player*, player*, damaged_by_types, const weapon_info*); // 0x34
    void (*player_respawn)(player*, int); // 0x38
    float (*adjust_respawn_weight)(const player*, const player_start*); // 0x3C
    void (*building_damage_changed)(player*, rfg_mover*, float); // 0x40
    bool (*can_item_respawn)(const item*, bool&); // 0x44
    bool (*item_touched)(const player*, const item*); // 0x48
    bool (*player_set_action_object)(const player*, t_usable_object&); // 0x4C
    bool (*player_can_swap_weapons)(const player*); // 0x50
    bool (*player_can_attack)(const player*, const weapon_info*, damaged_by_types); // 0x54
    void (*get_default_equipment)(const player*, multi_game_equipment_loadout*); // 0x58
    void (*process_mode_specific_packet)(unsigned char*, const sender&); // 0x5C
    int (*get_time_ms)(); // 0x60
    void (*countdown_begin)(); // 0x64
    void (*round_begin)(); // 0x68
    void (*round_over)(multi_round_winner); // 0x6C
    bool (*is_round_over)(); // 0x70
    multi_round_winner (*get_round_winner)(); // 0x74
    void (*server_migration_pre)(); // 0x78
    void (*server_migration_post)(); // 0x7C
    void (*server_migration_update_clients)(); // 0x80
    void (*append_mode_specific_stats)(unsigned char*, unsigned short&, unsigned int); // 0x84
    void (*read_mode_specific_stats)(unsigned char*, int&, unsigned int); // 0x88
    int (*get_mode_specific_stat_count)(); // 0x8C
    void (*get_mode_specific_stat_name)(char*, const int, int); // 0x90
    void (*get_mode_specific_stat_value)(char*, const int, int, unsigned int); // 0x94
    void (*get_decorated_name)(char*); // 0x98
    void (*get_objective_description)(char*); // 0x9C
    const char* hud_data_item_name; // 0xA0
    void (*hud_init)(vint_data_item*); // 0xA4
    void (*hud_process)(vint_data_item*); // 0xA8
    bool (*alter_player_icon)(const player*, hud_icon*); // 0xAC
    multi_announcer_lines intro_line; // 0xB0
    bool is_team_mode; // 0xB4
};

// Defined in multi_gameplay_mode.cpp (later in the unity TU); external linkage in the port for that reason.
extern multi_gameplay_mode_funcs Multi_gameplay_mode_funcs[8]; // 0x0167DEE8
// Defined in multi_game.cpp (later in the unity TU); external linkage in the port for that reason.
extern bool Multi_game_has_migrated; // 0x02FEB6C8
extern bool Multi_game_has_skipped_game_migration; // 0x02FEB6C9

// ---- globals
bool Mp_last_man_standing; // 0x0167DD7A
multi_match_info Multi_match_info; // 0x01691DA0

// ---- file-static globals
// int Bandwidth_filling_time_ms (0x0167DE28) is used here too; placed in multi_core_bandwidth.cpp.
// player_bandwidth_info Player_bandwidth[6] (0x0167DE2C) is used here too; placed in multi_core_game_bandwidth.cpp.
static int Mp_respawn_invulnerable_time_ms = 5000; // 0x0167DE74
// Placed with its variable: rfg.exe's item is multi_voice.cpp's (later in this unity TU).
static tweak_table_item Tweak_table_item_Mp_respawn_invulnerable_time_ms("Mp_respawn_invulnerable_time_ms", &Mp_respawn_invulnerable_time_ms); // 0x02FEBC7C
// The countdown announcements: five minutes and one minute left.
static time_left_announcement Time_left_announcements[2] = { // 0x0167DE78
    {MAL_GEN_TIME_FIVE_MINUTES, 300000},
    {MAL_GEN_TIME_ONE_MINUTE, 60000},
};
static int Num_time_left_announcements = 2; // 0x0167DE88
// The HUD's match events, created at match init: round over (an int), kill streak (a string), hidden challenge
// complete (two strings).
static ui_match_event Ui_match_events[3] = { // 0x0167DE8C
    {"multiplayer_round_over", nullptr},
    {"multiplayer_set_streak", nullptr},
    {"multiplayer_set_challenge", nullptr},
};
static int Recent_hit_time_ms = 5000; // 0x0167DEA4
static use_object_icon Team_swap_icons_ctf[2] = {(use_object_icon)6, (use_object_icon)7}; // 0x0167DEA8
// multi_gameplay_mode Multi_gameplay_mode (0x0167DEE0) is used here too; placed in multi_core_platform.cpp.
// human_voice_persona_handle Multi_general_announcer (0x0167DEE4) is used here too; placed in multi_game_mode_bagman.cpp.
static const char* Multi_general_announcer_persona_name = {"MPANC"}; // 0x0167E508
static const char* EMPTY_CHECKBOX_NAME = {"ui_fsmap_uncheck_box"}; // 0x0167E50C
static int Empty_checkbox_image = -1; // 0x0167E510
static multi_camera_mode Current_mode_camera = MCM_NONE; // 0x0167E544
// unsigned int Local_game_data_handles[4] (0x0167E5F4) is used here too; placed in multi_join.cpp.
static const char* Multi_custom_weapon_names[10] = {"mp_explosive_object", "mp_debris", "mp_killzone_cliff_kill", "mp_killzone_cliff_suicide", "mp_killzone_mine_kill", "mp_killzone_mine_suicide", "mp_charge_stick", "mp_rocket_hit", "mp_thermobaric_rocket_hit", "mp_killzone_cliff_suicide"}; // 0x0167F4CC
// timestamp Bandwidth_ready (0x016916C4) is used here too; placed in multi.cpp.
// dynamic_uint_hash_table<remote_object*> Network_handle_to_rop_hash (0x01691B88) is used here too; placed in multi_core_delta_compression.cpp.
// dynamic_uint_hash_table<network_game_handle*> Reserved_network_to_game_hash (0x01691C60) is used here too; placed in multi_core_delta_compression.cpp.
// timestamp Bandwidth_filling_timestamp (0x01691CC4) is used here too; placed in multi.cpp.
static timestamp New_lead_timer; // 0x01691DC4, initial data ffffffff
// multi_announcer_line_info Multi_announcer_lines[52] (0x01691F00) is used here too; placed in multi_game_mode_capture_the_flag.cpp.
static farray<backpack_rack_popup,4> Backpack_rack_popups; // 0x01692588
static timestamp Camera_change_timestamp; // 0x01692754, initial data ffffffff
static timestamp_realtime Mgm_allow_migration; // 0x01692DC4, initial data ffffffff
static color Timer_color_normal(255, 255, 255, 255); // 0x01693A50
static color Timer_color_caution(255, 255, 0, 255); // 0x01693A54
static multi_timer Timer_color_swap_time; // 0x01693A5C, initial data ffffffff
// multi_timer Timer_time (0x01693A64) is used here too; placed in multi_core_platform.cpp.
static float TS_bagman_draw_chance; // 0x02FE5C64
static float TS_skill_mismatch; // 0x02FE6D48
// External linkage in the port: multi_game_mode_capture_the_flag.cpp (earlier in the unity TU) registers its tweak.
float TS_anarchy_draw_chance; // 0x02FE6D4C
static int TS_bad_lag_duration_ms; // 0x02FE73C0
static multi_game_mode_info Multi_game_modes[8]; // 0x02FE73C8
static float TS_ctf_draw_chance; // 0x02FE80E8
static float TS_team_anarchy_draw_chance; // 0x02FE80EC
// multi_team_stats Mp_team_stats[2] (0x02FE82F0) is used here too; placed in multi_game_mode_bagman.cpp.
// point_of_interest Camera_points_of_interest[2] (0x02FE8300) is used here too; placed in multi_game_mode_bagman.cpp.
static multi_round_winner Winner_last_check; // 0x02FE8340
// multi_game_lobby_state_machine Lobby_state (0x02FE8344) is used here too; placed in multi_core_session_base.cpp.
static float TS_undesired_playlist; // 0x02FE87D4
static float TS_bad_lag; // 0x02FE8DBC
static multi_player_uid Winner_player_uid; // 0x02FE8DCC
static float TS_siege_draw_chance; // 0x02FE8E0C
static int TS_bad_lag_rtt_ms; // 0x02FE8E40
static float TS_demolition_draw_chance; // 0x02FE8E44
static float TS_team_bagman_draw_chance; // 0x02FE8E5C
static float TS_damage_control_draw_chance; // 0x02FEB380
static float TS_server_migration; // 0x02FEB4A8
// unsigned int Clock_current_time (0x02FEB538) is used here too; placed in multi.cpp.
// bool Server (0x02FEB590) is used here too; placed in multi_core_session_generic.cpp.
// bool Bandwidth_enabled (0x02FEB5D1) is used here too; placed in multi.cpp.
// bool Scoreboard_system_initd (0x02FEB5DE) is used here too; placed in multi.cpp.
// bool (*Game_voice_cb)(multi_connection*) (0x02FEB628) is used here too; placed in multi_voice.cpp.
static bool Client_authorized_to_send_endgame_stats; // 0x02FEB639
static bool Skill_mismatch; // 0x02FEB63A
static int Logging_match_handle; // 0x02FEB63C
static int Logging_round_handle; // 0x02FEB640
// bool Spectating (0x02FEB659) is used here too; placed in multi.cpp.
static int Mp_announce_num_pending_dialogues; // 0x02FEB668
static multi_camera_view_modes Current_view_mode; // 0x02FEB680
// mp_game_data_wrapper* Mp_game_data (0x02FEB698) is used here too; placed in multi_connection.cpp.
static bool Vetoes_allowed; // 0x02FEB6B3
static bool Vetoed_map; // 0x02FEB6B8
// migration_state Migration_state (0x02FEB6C4) is used here too; placed in multi.cpp.
// multi_player_stats* Mp_player_stats (0x02FEB720) is used here too; placed in multi_game_mode_bagman.cpp.
static const multi_player_stats* My_player_stats; // 0x02FEB728
static tracked_level_reference* Stat_tracked_levels; // 0x02FEB750
static int Num_stat_tracked_levels; // 0x02FEB754
static int Time_last_check; // 0x02FEB758
// tweak_table_item Tweak_table_item_TS_anarchy_draw_chance (0x02FEB884) is used here too; placed in multi_game_mode_capture_the_flag.cpp.
static tweak_table_item Tweak_table_item_TS_ctf_draw_chance("TS_ctf_draw_chance", &TS_ctf_draw_chance); // 0x02FEB954
static tweak_table_item Tweak_table_item_TS_bagman_draw_chance("TS_bagman_draw_chance", &TS_bagman_draw_chance); // 0x02FEBB3C
static tweak_table_item Tweak_table_item_TS_server_migration("TS_server_migration", &TS_server_migration); // 0x02FEBBCC
static tweak_table_item Tweak_table_item_TS_bad_lag_rtt_ms("TS_bad_lag_rtt_ms", &TS_bad_lag_rtt_ms); // 0x02FEBBEC
static tweak_table_item Tweak_table_item_TS_team_anarchy_draw_chance("TS_team_anarchy_draw_chance", &TS_team_anarchy_draw_chance); // 0x02FEBBF8
static tweak_table_item Tweak_table_item_TS_team_bagman_draw_chance("TS_team_bagman_draw_chance", &TS_team_bagman_draw_chance); // 0x02FEBC04
static keen::SystemTimer s_preGameWaitForClientsTimer; // 0x02FEBC1C
static tweak_table_item Tweak_table_item_TS_skill_mismatch("TS_skill_mismatch", &TS_skill_mismatch); // 0x02FEBC20
static tweak_table_item Tweak_table_item_TS_demolition_draw_chance("TS_demolition_draw_chance", &TS_demolition_draw_chance); // 0x02FED24C
static tweak_table_item Tweak_table_item_TS_bad_lag_duration_ms("TS_bad_lag_duration_ms", &TS_bad_lag_duration_ms); // 0x02FED528
static tweak_table_item Tweak_table_item_TS_undesired_playlist("TS_undesired_playlist", &TS_undesired_playlist); // 0x02FED87C
static tweak_table_item Tweak_table_item_TS_siege_draw_chance("TS_siege_draw_chance", &TS_siege_draw_chance); // 0x02FED888
// multi_xp_event Multi_xp_events[84] (0x0300D240) is used here too; placed in multi_game_mode_bagman.cpp.
static tweak_table_item Tweak_table_item_TS_damage_control_draw_chance("TS_damage_control_draw_chance", &TS_damage_control_draw_chance); // 0x030173A0
static tweak_table_item Tweak_table_item_TS_bad_lag("TS_bad_lag", &TS_bad_lag); // 0x030173AC
static color Timer_color; // 0x03017740

// The PREGAME wait for the clients' loaded packets before the server starts the countdown anyway.
static const int Pregame_wait_for_clients_ms = 30000;
// The PT_MODE_COMMON subtypes (the byte after the header).
static const unsigned char Mode_common_countdown_start = 0;
static const unsigned char Mode_common_scores = 1;
static const unsigned char Mode_common_round_begin = 2;
static const unsigned char Mode_common_round_over = 3;

// ---- internal functions
// 0x00975A60 (:333): a streak of 5 kills or more shows "<n> kill streak" (the HUD's kill streak event). LTCG dropped
// player_index (rfg.exe passes only pp).
static void announce_skill(int player_index, const player* pp)
{
    const multi_game_data* data = multi_game_data_get(pp->mp_data, false, false); // inlined
    if (data == nullptr)
    {
        return;
    }
    const multi_player_stats* stats = (pp->mp_data & 0xFFFF) > 16 ? nullptr : &Mp_player_stats[pp->mp_data & 0xFFFF];
    // Magic number: the announcement starts at 5 kills in a row.
    if (stats == nullptr || stats->kills_in_a_row <= 0 || stats->kills_in_a_row <= 4 || Ui_match_events[1].event == nullptr)
    {
        return;
    }
    wchar_t text[64];
    _snprintf_safe(text, 64, "%d %ls", stats->kills_in_a_row, localization_get_display_string_req("HM_KILL_STREAK", nullptr));
    char encoded[192];
    vint_encode_wide_text(encoded, sizeof(encoded), text);
    Ui_match_events[1].event->set_element(0, vint_variant((const char*)encoded));
}

// Port helper: multi_gameplay_mode_is_team_mode, as rfg.exe inlines it here.
static bool current_mode_is_team_mode()
{
    return Multiplayer && Multi_gameplay_mode >= 0 && Multi_gameplay_mode < 8 && Multi_gameplay_mode_funcs[Multi_gameplay_mode].is_team_mode;
}

// 0x009B9FE0 (:357): tells the clients when the server's countdown started (the server's clock)
static void multi_gm_send_countdown_start()
{
    unsigned char packet[1200];
    packet[0] = PT_MODE_COMMON;
    unsigned short packet_size = packet_header_size(PT_MODE_COMMON);
    packet[packet_size++] = Mode_common_countdown_start;
    write_u32_be(packet, packet_size, Clock_current_time);
    packet_set_size(packet, packet_size);
    multi_io_send_to_all_reliable(multi_session_get_by_type(ST_GAME), packet, packet_size);
}

// 0x00992370 (:380): Game_voice_cb in team modes: voice goes to allies only (marauders hear nobody)
static bool team_voice_should_send(multi_connection* cp)
{
    const multi_game_data* my_data = multi_game_data_get(Local_game_data_handles[ST_GAME], false, false);
    if (my_data == nullptr)
    {
        return false;
    }
    const multi_game_data* data = multi_game_data_get(cp->game_data, false, false);
    return data != nullptr && my_data->team != HUMAN_TEAM_MARAUDER && human_teams_get_disposition(my_data->team, data->team) == TEAM_DISPOSITION_ALLIED;
}

// 0x00992420 (:408): Game_voice_cb in the other modes
static bool non_team_voice_should_send(multi_connection* cp)
{
    const multi_game_data* my_data = multi_game_data_get(Local_game_data_handles[ST_GAME], false, false);
    return my_data != nullptr && my_data->team != HUMAN_TEAM_MARAUDER;
}

// 0x0094D010 (:431)
static void mp_close_all_message_boxes()
{
    // -2: every message box
    if (ui_is_msgbox_active(-2))
    {
        ui_hide_msgbox(-2, false);
    }
}

// Port helper: rfg.exe inlines the loops over the world's lists of one object type.
template<typename T>
static void hide_objects()
{
    const int count = World.get_num_objects<T>();
    for (int i = 0; i < count; i++)
    {
        World.get_object<T>(i)->set_is_visible(false);
    }
}

// 0x009E9080 (:440): the round's stats are synchronized: the players, items and effects disappear, the results show
// for 10 s. After the last round the local player's profile stats count the match (a win, loss or tie; a server alone
// in the end ties) and, after the last of the rounds, are reported to the ranking boards. The server logs each player's
// score (original: with checksum 0 and no null check of the stats) and in team modes the team scores.
static void multi_game_mode_move_to_view_results_state()
{
    Multi_match_info.synchronizing_endgame = false;
    hide_objects<player>();
    hide_objects<item>();
    hide_objects<object_effect>();
    // Magic number: 10 s of results.
    multi_game_timer_set(10000); // inlined
    Multi_match_info.match_state = MGMS_VIEWING_RESULTS;
    if (multi_game_mode_is_last_round())
    {
        // The server is alone when every other player timed out.
        bool alone = false;
        if (Multiplayer && Server)
        {
            alone = true;
            for (const multi_game_data* data = multi_game_data_begin(ST_GAME_SNAPSHOT, false, false); data != nullptr; data = multi_game_data_next(data, false, false))
            {
                const multi_player_stats* stats = multi_game_stats_get_player_stats(data->handle); // inlined
                log_event_player_score(0, gamelog_get_logging_time(), gamelog_get_player_index(data), (char)data->team, (short)stats->score, (char)data->player_level);
                if (!data->timed_out && !data->player_uid.get_internal_id().is_equal(My_player_uid.get_internal_id()))
                {
                    alone = false;
                }
            }
            if (current_mode_is_team_mode())
            {
                log_event_team_score(gamelog_get_player_checksum(), gamelog_get_logging_time(), 1, (short)Mp_team_stats[HUMAN_TEAM_EDF].score); // inlined
                log_event_team_score(gamelog_get_player_checksum(), gamelog_get_logging_time(), 0, (short)Mp_team_stats[HUMAN_TEAM_GUERILLA].score); // inlined
            }
            // 0x1E: the match event
            const int match_handle = Logging_match_handle;
            if (log_record_event(0x1E))
            {
                log_stop_continuous_event(match_handle);
            }
            Logging_match_handle = 0;
        }
        const multi_round_winner winner = Multi_match_info.round_winner;
        if (Spectating)
        {
            stats_increment_by_one(STATS_MP_GAMES_SPECTATED_TOTAL);
            const multi_session_info_base* info = Game_session->get_info();
            if (!info->systemlink_or_lan && !info->private_match)
            {
                stats_increment_by_one(STATS_MP_GAMES_SPECTATED_RANKED_OR_SOCIAL);
            }
        }
        else
        {
            bool won = false;
            bool tie = false;
            const multi_gameplay_mode mode = Mp_server_options.get_mode();
            if (mode > MGPM_DEMOLITION || !Multi_gameplay_mode_funcs[mode].is_team_mode)
            {
                // The winner's player (by its network handle); without one the match is a tie.
                const player* winner_player = nullptr;
                if (winner.player_game_data_handle != 0xFFFFFFFFu && game_data_of(winner.player_game_data_handle)->in_use)
                {
                    const unsigned int handle = multi_core_game_handle_from_network_handle(game_data_of(winner.player_game_data_handle)->player_network_handle, true);
                    object* objp = handle != 0xFFFFFFFFu ? World.object_from_handle(handle) : nullptr;
                    if (objp != nullptr && objp->get_obj_type() == OT_HUMAN && objp->get_sub_type() == OT_SUB_HUMAN_PLAYER)
                    {
                        winner_player = (const player*)objp;
                    }
                }
                if (winner_player != nullptr)
                {
                    won = winner_player == player_get_local_player();
                }
                else
                {
                    tie = true;
                }
            }
            else
            {
                // Original: our data is not checked for null.
                const multi_game_data* my_data = multi_game_data_get(Local_game_data_handles[ST_GAME_SNAPSHOT], false, false);
                won = winner.player_game_data_handle == (unsigned int)my_data->team;
                tie = winner.player_game_data_handle == 0xFFFFFFFFu;
            }
            if (won && alone)
            {
                won = false;
                tie = true;
            }
            unit4::LocalGameSession* session = rfg_get_local_game_session();
            stats_increment_by_one(STATS_MP_TOTAL_GAMES_PLAYED_OVERALL);
            // Magic numbers: hero stats 4 (games played) and 5 (wins).
            if (session != nullptr)
            {
                session->updateHeroStatAdd(nullptr, 4, 1);
            }
            if (won)
            {
                stats_increment_by_one(STATS_MP_TOTAL_WINS_OVERALL);
                if (session != nullptr)
                {
                    session->updateHeroStatAdd(nullptr, 5, 1);
                }
            }
            else
            {
                stats_increment_by_one(tie ? STATS_MP_TOTAL_TIES_OVERALL : STATS_MP_TOTAL_LOSSES_OVERALL);
            }
            for (int i = 0; i < Num_stat_tracked_levels; i++)
            {
                if (Stat_tracked_levels[i].l_info == Mp_server_options.get_level())
                {
                    stats_increment_by_one(Stat_tracked_levels[i].games_played_stat);
                }
            }
            const multi_session_info_base* info = Game_session->get_info();
            if (info->systemlink_or_lan)
            {
                stats_increment_by_one(STATS_MP_TOTAL_GAMES_PLAYED_SYSLINK_GAMES);
                stats_increment_by_one(won ? STATS_MP_TOTAL_WINS_SYSLINK_GAMES : tie ? STATS_MP_TOTAL_TIES_SYSLINK_GAMES : STATS_MP_TOTAL_LOSSES_SYSLINK_GAMES);
            }
            else if (info->private_match)
            {
                stats_increment_by_one(STATS_MP_TOTAL_GAMES_PLAYED_PRIVATE_GAMES);
                stats_increment_by_one(won ? STATS_MP_TOTAL_WINS_PRIVATE_GAMES : tie ? STATS_MP_TOTAL_TIES_PRIVATE_GAMES : STATS_MP_TOTAL_LOSSES_PRIVATE_GAMES);
            }
            else if (Current_game_type_context == PGTC_PUBLIC)
            {
                stats_increment_by_one(STATS_MP_TOTAL_GAMES_PLAYED_SOCIAL_GAMES);
                stats_increment_by_one(STATS_MP_TOTAL_GAMES_PLAYED_RANKED_OR_SOCIAL);
                if (won)
                {
                    stats_increment_by_one(STATS_MP_TOTAL_WINS_SOCIAL_GAMES);
                    stats_increment_by_one(STATS_MP_TOTAL_WINS_RANKED_OR_SOCIAL);
                }
                else
                {
                    stats_increment_by_one(tie ? STATS_MP_TOTAL_TIES_SOCIAL_GAMES : STATS_MP_TOTAL_LOSSES_SOCIAL_GAMES);
                }
            }
            else
            {
                stats_increment_by_one(STATS_MP_TOTAL_GAMES_PLAYED_RANKED_GAMES);
                stats_increment_by_one(STATS_MP_TOTAL_GAMES_PLAYED_RANKED_OR_SOCIAL);
                if (won)
                {
                    stats_increment_by_one(STATS_MP_TOTAL_WINS_RANKED_GAMES);
                    stats_increment_by_one(STATS_MP_TOTAL_WINS_RANKED_OR_SOCIAL);
                    stats_increment_by_one(STATS_MP_MATCH_WIN_STREAK);
                    stats_update_best(STATS_MP_MATCH_WIN_STREAK_HIGHEST, stats_get_stat_value(STATS_MP_MATCH_WIN_STREAK));
                }
                else
                {
                    stats_increment_by_one(tie ? STATS_MP_TOTAL_TIES_RANKED_GAMES : STATS_MP_TOTAL_LOSSES_RANKED_GAMES);
                    stats_reset(STATS_MP_MATCH_WIN_STREAK);
                }
                if (My_player_stats != nullptr)
                {
                    // The damage and repair totals as whole numbers.
                    const unsigned int damage = (unsigned int)My_player_stats->total_damage;
                    const int repair = (int)My_player_stats->total_repair;
                    stats_increment(STATS_MP_TOTAL_DAMAGE_RANKED, &damage);
                    stats_increment(STATS_MP_TOTAL_REPAIR_RANKED, &repair);
                    stats_update_best(STATS_MP_HIGHEST_DAMAGE_RANKED, (int)My_player_stats->highest_damage);
                }
            }
            if (Multiplayer && (unsigned int)Multi_gameplay_mode < 8)
            {
                // The switch over the modes in MGPM_* order: games played, then the win, loss or tie.
                static const tracked_stats_indices mode_stats[8][4] = {
                    {STATS_MP_TOTAL_GAMES_PLAYED_DEATHMATCH, STATS_MP_TOTAL_WINS_DEATHMATCH, STATS_MP_TOTAL_LOSSES_DEATHMATCH, STATS_MP_TOTAL_TIES_DEATHMATCH},
                    {STATS_MP_TOTAL_GAMES_PLAYED_TEAM_DEATHMATCH, STATS_MP_TOTAL_WINS_TEAM_DEATHMATCH, STATS_MP_TOTAL_LOSSES_TEAM_DEATHMATCH, STATS_MP_TOTAL_TIES_TEAM_DEATHMATCH},
                    {STATS_MP_TOTAL_GAMES_PLAYED_CAPTURE_THE_FLAG, STATS_MP_TOTAL_WINS_CAPTURE_THE_FLAG, STATS_MP_TOTAL_LOSSES_CAPTURE_THE_FLAG, STATS_MP_TOTAL_TIES_CAPTURE_THE_FLAG},
                    {STATS_MP_TOTAL_GAMES_PLAYED_SIEGE, STATS_MP_TOTAL_WINS_SIEGE, STATS_MP_TOTAL_LOSSES_SIEGE, STATS_MP_TOTAL_TIES_SIEGE},
                    {STATS_MP_TOTAL_GAMES_PLAYED_KING_OF_THE_HILL, STATS_MP_TOTAL_WINS_KING_OF_THE_HILL, STATS_MP_TOTAL_LOSSES_KING_OF_THE_HILL, STATS_MP_TOTAL_TIES_KING_OF_THE_HILL},
                    {STATS_MP_TOTAL_GAMES_PLAYED_BAGMAN, STATS_MP_TOTAL_WINS_BAGMAN, STATS_MP_TOTAL_LOSSES_BAGMAN, STATS_MP_TOTAL_TIES_BAGMAN},
                    {STATS_MP_TOTAL_GAMES_PLAYED_TEAM_BAGMAN, STATS_MP_TOTAL_WINS_TEAM_BAGMAN, STATS_MP_TOTAL_LOSSES_TEAM_BAGMAN, STATS_MP_TOTAL_TIES_TEAM_BAGMAN},
                    {STATS_MP_TOTAL_GAMES_PLAYED_DEMOLITION, STATS_MP_TOTAL_WINS_DEMOLITION, STATS_MP_TOTAL_LOSSES_DEMOLITION, STATS_MP_TOTAL_TIES_DEMOLITION},
                };
                const tracked_stats_indices* stats = mode_stats[Multi_gameplay_mode];
                stats_increment_by_one(stats[0]);
                stats_increment_by_one(won ? stats[1] : tie ? stats[3] : stats[2]);
            }
            // Playing with someone who completed the single player game (outside system link).
            const multi_game_data* my_game_data = multi_game_data_get(Local_game_data_handles[ST_GAME], false, false);
            if (!info->systemlink_or_lan && my_game_data != nullptr && my_game_data->completed_single_player)
            {
                for (const multi_game_data* data = multi_game_data_begin(ST_GAME, false, false); data != nullptr; data = multi_game_data_next(data, false, false))
                {
                    if (data != my_game_data && data->completed_single_player)
                    {
                        stats_set_stat(STATS_MP_RED_FACTION_MEMBER, 1);
                    }
                }
            }
        }
    }
    if (Multi_match_info.current_round == Multi_match_info.num_rounds)
    {
        multi_game_mode_report_stats();
    }
}

// 0x00992480 (:709): players stay hidden until the round begins
static void multi_game_mode_hide_all_players(bool hide)
{
    const int count = World.get_num_objects<player>();
    for (int i = 0; i < count; i++)
    {
        object_set_visibility_plus_children(World.get_object<player>(i), !hide);
    }
}

// 0x0094D030 (:723): mp_modes.xtbl, one entry per mode in MGPM_* order (8 at most)
static void multi_game_parse_mp_modes_table()
{
    xml_element* root = xml_table_parse("mp_modes.xtbl");
    xml_element* entry = xml_table_find_opt(root, "mp_modes_list");
    for (multi_game_mode_info* info = Multi_game_modes; entry != nullptr; entry = xml_table_find_next(root, entry, "mp_modes_list"), info++)
    {
        if (info >= Multi_game_modes + 8)
        {
            break;
        }
        memset(info, 0, sizeof(*info));
        xml_table_get_req_string(info->name, sizeof(info->name), entry, "Name");
        xml_table_get_req_string(info->display_name, sizeof(info->display_name), entry, "Display_Name");
        xml_table_get_req_string(info->description, sizeof(info->description), entry, "Description");
        xml_element* classes = xml_table_find_opt(entry, "selectable_classes");
        info->num_classes = xml_table_count(classes, "class");
        xml_element* class_entry = xml_table_find_opt(classes, "class");
        for (int i = 0; i < info->num_classes; i++)
        {
            // The count is kept even past the 4 names stored.
            if (i < 4)
            {
                xml_table_get_req_string(info->classes[i], sizeof(info->classes[i]), class_entry, "name");
            }
            class_entry = xml_table_find_next(classes, class_entry, "class");
        }
        xml_element* info_text = xml_table_find_req(entry, "info_text");
        xml_element* text_entry = xml_table_find_opt(info_text, "entry");
        for (int i = 0; i < 3; i++)
        {
            if (text_entry == nullptr)
            {
                info->info_text[i][0] = '\0';
            }
            else
            {
                strncpy_safe(info->info_text[i], text_entry->text, sizeof(info->info_text[i]));
                text_entry = xml_table_find_next(info_text, text_entry, "entry");
            }
        }
    }
    xml_table_close(root);
}

// 0x009E9D30 (:779): the round winner (a team in team modes, a player otherwise) gets the round win and the win match
// experience
static void multi_game_mode_update_round_wins_stat(multi_round_winner winner)
{
    if (current_mode_is_team_mode())
    {
        if (winner.player_game_data_handle != 0xFFFFFFFFu)
        {
            multi_game_stats_record_team_round_win(winner.team);
            multi_game_xp_trigger_event(winner.team, MXE_WIN_MATCH, 1, false); // inlined
        }
        return;
    }
    if (winner.player_game_data_handle == 0xFFFFFFFFu || Mp_game_data == nullptr)
    {
        return;
    }
    const multi_game_data* data = multi_game_data_get(winner.player_game_data_handle, true, false);
    if (data != nullptr)
    {
        multi_game_stats_record_round_win(data);
        multi_game_xp_trigger_event(winner.player_game_data_handle, MXE_WIN_MATCH, 1, 0xFF, -1, false); // inlined
    }
}

// 0x009BA0B0 (:798): the round's remaining time, to one client (cp, a JIP) or to all (nullptr)
// LTCG: cp in ESI.
static void send_round_begin_packet(multi_connection* cp)
{
    unsigned char packet[1200];
    packet[0] = PT_MODE_COMMON;
    unsigned short packet_size = packet_header_size(PT_MODE_COMMON);
    packet[packet_size++] = Mode_common_round_begin;
    write_u32_be(packet, packet_size, (unsigned int)Timer_time.time_until());
    packet_set_size(packet, packet_size);
    if (cp == nullptr)
    {
        multi_io_send_to_all_reliable(Game_session, packet, packet_size);
        return;
    }
    multi_io_send_reliable(cp, packet, packet_size, nullptr, nullptr, nullptr);
}

// 0x009BA1B0 (:824)
static void send_round_over_packet(multi_round_winner winner)
{
    unsigned char packet[1200];
    packet[0] = PT_MODE_COMMON;
    unsigned short packet_size = packet_header_size(PT_MODE_COMMON);
    packet[packet_size++] = Mode_common_round_over;
    write_u32_be(packet, packet_size, winner.player_game_data_handle);
    packet_set_size(packet, packet_size);
    multi_io_send_to_all_reliable(Game_session, packet, packet_size);
}

// 0x009924E0 (:848)
// LTCG: countdown_time_ms in EAX.
static void multi_game_mode_enter_countdown(int countdown_time_ms)
{
    multi_game_timer_set(countdown_time_ms);
    if (Multi_gameplay_mode_funcs[Multi_gameplay_mode].countdown_begin != nullptr)
    {
        Multi_gameplay_mode_funcs[Multi_gameplay_mode].countdown_begin();
    }
    Multi_match_info.setting_up_round = false;
    Multi_match_info.match_state = MGMS_COUNTDOWN;
    console_change_chat_window_pos(VCP_ENDGAME);
}

// 0x00975BB0 (:862)
static void multi_game_mode_set_time_of_day(bool use_default)
{
    if (Mp_server_options.get_level() == nullptr)
    {
        return;
    }
    if (use_default)
    {
        game_clock_set_time(Mp_server_options.get_level()->time_of_day_hour, Mp_server_options.get_level()->time_of_day_minute, 0);
        return;
    }
    game_clock_set_time(Mp_server_options.get_level()->alt_time_of_day_hour, Mp_server_options.get_level()->alt_time_of_day_minute, 0);
}

// 0x009AADE0 (:875): the start of a match (multi_gameplay_mode_enter, and multi_game_mode_enter_common jumps here)
static void multi_game_mode_match_init()
{
    s_preGameWaitForClientsTimer.start();
    Multi_match_info.round_time_ms = (unsigned int)Mp_server_options.generic_options.round_time_limit_minutes * 60000;
    Multi_game_has_migrated = false;
    Multi_game_has_skipped_game_migration = false;
    Multi_left_game_by_choice = false;
    Multi_match_info.setting_up_round = true;
    Multi_match_info.synchronizing_endgame = false;
    Multi_match_info.num_rounds = Mp_server_options.generic_options.num_rounds;
    Multi_match_info.current_round = 0;
    Multi_match_info.countdown_time_ms = 5000; // RFG default: 10000
    Multi_match_info.match_state = MGMS_PREGAME;
    Multi_match_info.playing_match = true;
    Multi_match_info.max_num_players = 0;
    if (Multiplayer && Server)
    {
        Multi_match_info.use_default_time_of_day = Mp_server_options.generic_options.time_of_day == MGTOD_DEFAULT ||
            (Mp_server_options.generic_options.time_of_day == MGTOD_RANDOM && bool_rand());
    }
    // multi_game_mode_set_time_of_day, inlined
    if (Mp_server_options.get_level() != nullptr)
    {
        if (Multi_match_info.use_default_time_of_day)
        {
            game_clock_set_time(Mp_server_options.get_level()->time_of_day_hour, Mp_server_options.get_level()->time_of_day_minute, 0);
        }
        else
        {
            game_clock_set_time(Mp_server_options.get_level()->alt_time_of_day_hour, Mp_server_options.get_level()->alt_time_of_day_minute, 0);
        }
    }
    multi_game_timer_set(Multi_match_info.countdown_time_ms);
    multi_game_stats_match_init();
    Timer_color = Timer_color_normal;
    Timer_time.invalidate();
    Timer_color_swap_time.invalidate();
    multi_game_xp_match_init();
    Multi_general_announcer = human_voice_persona_handle_from_name(Multi_general_announcer_persona_name);
    Mp_announce_num_pending_dialogues = 0;
    Bandwidth_enabled = true;
    Bandwidth_ready.set(10000);
    Bandwidth_filling_timestamp.set(Bandwidth_filling_time_ms);
    multi_core_qos_game_started();

    // Last man standing: on unless the match has nobody to fight (a single player, or everyone on one team in a team
    // mode), which only a system link or private match allows.
    int num_players = 0;
    human_teams first_team = HUMAN_TEAM_NONE;
    bool all_on_one_team = true;
    for (const multi_game_data* data = multi_game_data_begin(ST_GAME_SNAPSHOT, false, false); data != nullptr;
         data = multi_game_data_next(data, false, false))
    {
        if (data->team != HUMAN_TEAM_MARAUDER)
        {
            if (first_team != HUMAN_TEAM_NONE && first_team != data->team)
            {
                all_on_one_team = false;
            }
            else
            {
                first_team = data->team;
            }
            num_players++;
        }
    }
    bool has_opponents;
    if (current_mode_is_team_mode())
    {
        has_opponents = !all_on_one_team;
    }
    else
    {
        has_opponents = num_players != 1;
    }
    const bool lan_or_private = Multi_syslink || Game_session->get_info()->private_match;
    Mp_last_man_standing = has_opponents || !lan_or_private;

    Game_voice_cb = current_mode_is_team_mode() ? team_voice_should_send : non_team_voice_should_send;

    for (int i = 0; i < 3; i++)
    {
        vint_data_event* event = vint_data_event::create_new(Ui_match_events[i].name);
        Ui_match_events[i].event = event;
        if (event == nullptr)
        {
            continue;
        }
        if (i == 0)
        {
            const vint_variant_type types[1] = {VINT_PROP_TYPE_INT};
            event->set_template(types, 1);
        }
        else if (i == 1)
        {
            const vint_variant_type types[1] = {VINT_PROP_TYPE_STRING};
            event->set_template(types, 1);
        }
        else
        {
            const vint_variant_type types[2] = {VINT_PROP_TYPE_STRING, VINT_PROP_TYPE_STRING};
            event->set_template(types, 2);
        }
    }
    gamelog_mark_game_start_time();
    if (!Multiplayer || !Server || Mp_server_options.get_level() == nullptr)
    {
        return;
    }
    Logging_match_handle = log_start_event_mp_match(gamelog_get_player_checksum(), gamelog_get_logging_time(), (char)Game_session->get_slots_used(),
                                                    Mp_server_options.get_level()->filename_crc.get_crc(), (unsigned char)Mp_server_options.get_mode(),
                                                    Mp_server_options.generic_options.num_rounds, (unsigned short)Mp_server_options.generic_options.round_time_limit_minutes,
                                                    Mp_server_options.generic_options.point_limit, Mp_server_options.generic_options.friendly_fire);
    for (const multi_game_data* data = multi_game_data_begin(ST_GAME_SNAPSHOT, false, false); data != nullptr;
         data = multi_game_data_next(data, false, false))
    {
        // The uid is logged as the 8 provider bytes.
        int64_t uid_value;
        memcpy(&uid_value, data->player_uid.get_internal_id().providerData, sizeof(uid_value));
        log_event_match_player(0, gamelog_get_player_index(data), uid_value, (char)data->team, data->locale, data->language);
    }
}

// 0x009EA100 (:1028): the end of a match (multi_game_mode_exit_common is the same code)
static void multi_game_mode_match_shutdown()
{
    // 0x1E: the match event, 0x1C: the round event
    if (Logging_match_handle != 0)
    {
        if (log_record_event(0x1E))
        {
            log_stop_continuous_event(Logging_match_handle);
        }
        Logging_match_handle = 0;
    }
    if (Logging_round_handle != 0)
    {
        if (log_record_event(0x1C))
        {
            log_stop_continuous_event(Logging_round_handle);
        }
        Logging_round_handle = 0;
    }
    multi_game_xp_match_shutdown();
    multi_game_camera_stop();
    for (int i = 0; i < 3; i++)
    {
        if (Ui_match_events[i].event != nullptr)
        {
            Ui_match_events[i].event->destroy();
            Ui_match_events[i].event = nullptr;
        }
    }
    Multi_match_info.playing_match = false;
    Multi_match_info.match_state = MGMS_NONE;
    gamelog_local_reinit(true, nullptr);
    ui_disable_chat_window();
}

// 0x0094D1F0 (:1071)
void multi_game_mode_system_init()
{
    multi_game_parse_mp_modes_table();
}

// 0x00A00EC0 (:1077): the match state machine, every frame of a multiplayer game
void multi_game_mode_system_process()
{
    if (Multi_match_info.match_state < MGMS_VIEWING_RESULTS && Multi_match_info.match_state != MGMS_NONE)
    {
        // The lobby session dropped under the match.
        const multi_session_state lobby_state = Lobby_session->get_state();
        if (lobby_state == MSS_READY)
        {
            keen::debug::nativePrintArgs("Lobby_session->get_state() == MSS_READY. This shouldn't be hit. If it is, there's a problem.");
            game_boot_to_main_menu(BC_GAME_MIGRATION_FAIL);
        }
        else if (lobby_state == MSS_NONE)
        {
            game_boot_to_main_menu(BC_GAME_MIGRATION_FAIL);
        }
    }
    multi_game_process_player_names();
    switch (Multi_match_info.match_state)
    {
    case MGMS_PREGAME:
        multi_game_camera_process();
        multi_game_mode_hide_all_players(true);
        if (Multiplayer && Server)
        {
            if (multi_game_mode_all_players_ready())
            {
                multi_game_mode_enter_countdown(Multi_match_info.countdown_time_ms);
                multi_gm_send_countdown_start();
                multi_core_cheat_detect_start();
                return;
            }
            if (s_preGameWaitForClientsTimer.hasTimeElapsed(Pregame_wait_for_clients_ms))
            {
                multi_session* session = multi_session_get_by_type(ST_GAME);
                if (session == nullptr)
                {
                    keen::debug::nativePrintArgs("preGameWaitForClientsTimer has elapsed. session == nullptr! Boot to main menu ...\n");
                    game_boot_to_main_menu(BC_GAME_MIGRATION_FAIL);
                    return;
                }
                size_t num_disconnected = 0;
                multi_connection* first = session->get_connection_list();
                for (multi_connection* cp = first; cp != nullptr;)
                {
                    if (cp != session->get_host_connection() && cp->connection_status != CS_CONNECTED_GAMEPLAY)
                    {
                        multi_connection_disconnect(cp, CDR_SESSION_START_FAILED);
                        num_disconnected++;
                    }
                    cp = cp->next;
                    if (cp == first)
                    {
                        break;
                    }
                }
                keen::debug::nativePrintArgs("preGameWaitForClientsTimer has elapsed. Disconnected %zu clients.\n", num_disconnected);
                multi_game_mode_enter_countdown(Multi_match_info.countdown_time_ms);
                multi_gm_send_countdown_start();
                multi_core_cheat_detect_start();
                return;
            }
        }
        break;
    case MGMS_COUNTDOWN:
    {
        multi_game_camera_process();
        multi_game_mode_hide_all_players(true);
        multi_game_timer_process();
        // The mode's intro line in the last 7.5 s of the countdown.
        const multi_announcer_lines line = Multi_gameplay_mode_funcs[Multi_gameplay_mode].intro_line;
        if (line != INVALID_MULTI_ANNOUNCER_LINE && !multi_game_announcer_line_played_or_pending_this_round(line) && Timer_time.time_until() < 7500)
        {
            multi_game_announcer_play_line(line, 0xFFFFFFFF);
        }
        if (Multiplayer && Server && multi_game_timer_elapsed())
        {
            multi_gameplay_mode_round_begin();
            return;
        }
        break;
    }
    case MGMS_GAMEPLAY:
        multi_game_timer_process();
        multi_game_camera_process();
        multi_gameplay_mode_process();
        return;
    case MGMS_ENDGAME_SYNC:
        if (Multiplayer && Server)
        {
            if (multi_game_stats_are_finalized() || Multi_match_info.server_endgame_sync_wait_timestamp.elapsed())
            {
                multi_game_stats_endgame_sync();
                multi_game_mode_move_to_view_results_state();
                Multi_match_info.server_endgame_sync_wait_timestamp.elapsed();
                return;
            }
        }
        else if (Client_authorized_to_send_endgame_stats)
        {
            multi_game_stats_endgame_sync();
            Multi_match_info.match_state = MGMS_WAIT_ON_SYNC;
            Multi_match_info.client_wait_on_sync_timestamp.set(5000);
            Client_authorized_to_send_endgame_stats = false;
            return;
        }
        break;
    case MGMS_WAIT_ON_SYNC:
        if (multi_game_stats_are_finalized() || Multi_match_info.client_wait_on_sync_timestamp.elapsed())
        {
            multi_game_mode_move_to_view_results_state();
            Multi_match_info.client_wait_on_sync_timestamp.elapsed();
            return;
        }
        break;
    case MGMS_VIEWING_RESULTS:
        multi_game_timer_process();
        multi_game_camera_process();
        if (Timer_time.time_until() < 1 || !Timer_time.valid())
        {
            mp_close_all_message_boxes();
            if (multi_game_mode_is_last_round())
            {
                gameseq_set_state(GS_MAINMENU, false);
                return;
            }
            // The next round reloads the level on every machine.
            multi_session* session = multi_session_get_by_type(ST_GAME);
            if (Multiplayer && Server)
            {
                multi_connection* first = session->get_connection_list();
                for (multi_connection* cp = first; cp != nullptr;)
                {
                    if (cp != session->get_host_connection())
                    {
                        multi_game_send_join_game_start(cp, Mp_server_options.get_level()->filename);
                    }
                    cp = cp->next;
                    if (cp == first)
                    {
                        break;
                    }
                }
                Client_state = CLIENT_STATE_LOADING_GAME;
                World.set_pending_reload();
                return;
            }
        }
        break;
    case MGMS_NONE:
        break;
    default:
        keen::debug::nativePrintArgs("Unhandled multi game common state");
        break;
    }
}

// 0x00975BF0 (:1256)
void multi_game_mode_system_handshake()
{
    if (Multi_match_info.match_state == MGMS_GAMEPLAY && Multi_gameplay_mode_funcs[Multi_gameplay_mode].handshake != nullptr)
    {
        Multi_gameplay_mode_funcs[Multi_gameplay_mode].handshake();
    }
}

// 0x009AB2C0 (:1268)
void multi_game_mode_system_render()
{
    if (Multi_match_info.match_state == MGMS_GAMEPLAY)
    {
        multi_game_backpack_rack_render();
        if (Multi_gameplay_mode_funcs[Multi_gameplay_mode].render != nullptr)
        {
            Multi_gameplay_mode_funcs[Multi_gameplay_mode].render();
        }
    }
}

// 0x009AB2F0 (:1368): a jump to multi_game_mode_match_init
void multi_game_mode_enter_common()
{
    multi_game_mode_match_init();
}

// 0x009EA1B0 (:1373): the same code as multi_game_mode_match_shutdown
void multi_game_mode_exit_common()
{
    multi_game_mode_match_shutdown();
}

// 0x00975C20 (:1378)
void multi_game_mode_level_init_pre_common()
{
    Multi_match_info.round_time_ms = (unsigned int)Mp_server_options.generic_options.round_time_limit_minutes * 60000;
    Client_state = CLIENT_STATE_LOADING_GAME;
    Multi_match_info.setting_up_round = true;
    Multi_match_info.synchronizing_endgame = false;
    Multi_match_info.match_state = MGMS_PREGAME;
    s_preGameWaitForClientsTimer.start();
    Winner_player_uid.invalidate();
}

// 0x009AB300 (:1399)
void multi_game_mode_level_init_post_common()
{
    multi_game_marker_level_init_post();
    Empty_checkbox_image = vint_bms_load(EMPTY_CHECKBOX_NAME);
    Backpack_rack_popups.clear();
    // Original: current_level is not checked here.
    if (Mp_server_options.get_level()->wind_intensity > 0.0f)
    {
        weather_wind_start();
        weather_wind_set_limits(0.0f, 100.0f);
        weather_wind_set_speed(Mp_server_options.get_level()->wind_intensity, Mp_server_options.get_level()->wind_direction, 0.0f);
        dust_enable(Mp_server_options.get_level()->play_wind_dust_effect);
    }
    else
    {
        weather_wind_stop();
        dust_enable(false);
    }
    multi_game_camera_start();
    Multi_match_info.current_round++;
    ui_enable_chat_window(VCP_INGAME);
}

// 0x0094D200 (:1426)
void multi_game_mode_level_init_shutdown_common()
{
}

// 0x00992520 (:1430): server: a client whose mean ping stays over half TS_bad_lag_rtt_ms for TS_bad_lag_duration_ms
// gets the bad-lag stat
static void multi_gameplay_mode_check_lag()
{
    multi_session* session = multi_session_get_by_type(ST_GAME);
    if (session == nullptr)
    {
        return;
    }
    multi_connection* first = session->get_connection_list();
    for (multi_connection* cp = first; cp != nullptr;)
    {
        if (cp != session->get_me_connection())
        {
            unsigned int handle = cp->game_data;
            if ((int)cp->mean_ping > TS_bad_lag_rtt_ms / 2)
            {
                // Original: the game data is indexed without the in-use check.
                if (handle != 0xFFFFFFFFu && game_data_of(handle)->bad_lag_time.time_since() < 0 && cp->game_data != 0xFFFFFFFFu)
                {
                    game_data_of(cp->game_data)->bad_lag_time.set(0);
                }
                handle = cp->game_data;
                const int lag_time = handle == 0xFFFFFFFFu ? 0 : game_data_of(handle)->bad_lag_time.time_since();
                if (lag_time > TS_bad_lag_duration_ms && cp->game_data != 0xFFFFFFFFu && (cp->game_data & 0xFFFF) < 16)
                {
                    Mp_player_stats[cp->game_data & 0xFFFF].bad_lag = 1;
                }
            }
            else if (handle != 0xFFFFFFFFu)
            {
                game_data_of(handle)->bad_lag_time.invalidate();
            }
        }
        cp = cp->next;
        if (cp == first)
        {
            break;
        }
    }
}

// 0x009FDDD0 (:1459): every gameplay frame before the mode's own process; false once the round is over
bool multi_game_mode_process_common()
{
    // Spawn invulnerability ends when its timer runs out, or at once when the player is no longer invulnerable.
    const int num_players = World.get_num_objects<player>();
    for (int i = 0; i < num_players; i++)
    {
        player* pp = World.get_object<player>(i);
        if (pp->mp_time_until_player_leaves_invulnerable.valid() && (pp->mp_time_until_player_leaves_invulnerable.elapsed() || !pp->hflags.invulnerable))
        {
            pp->mp_time_until_player_leaves_invulnerable.invalidate();
            player_set_player_invulnerable(pp, false);
        }
    }
    Multi_match_info.max_num_players = (unsigned char)num_players;
    multi_game_backpack_rack_process();
    multi_game_rubberbanding_process();

    const int time_left = Timer_time.time_until();
    for (int i = 0; i < Num_time_left_announcements; i++)
    {
        const time_left_announcement& announcement = Time_left_announcements[i];
        // Lines past MAL_SGE_ENEMY_TARGET_WIN_NEAR have no play record; announcements longer than the round are skipped.
        if (time_left <= announcement.time_left_ms &&
            (announcement.line > MAL_SGE_ENEMY_TARGET_WIN_NEAR || !Multi_announcer_lines[announcement.line].last_played_time.valid()) &&
            announcement.time_left_ms <= Multi_match_info.round_time_ms)
        {
            multi_game_announcer_play_line(announcement.line, 0xFFFFFFFF);
        }
    }

    // A new leader is announced at most once a second.
    if (New_lead_timer.elapsed() || !New_lead_timer.valid())
    {
        multi_round_winner winner = Winner_last_check;
        if (!current_mode_is_team_mode())
        {
            int best_score = (int)0x80000001;
            int num_best = 0;
            multi_player_uid leader;
            player* local_player = player_get_local_player();
            for (int i = 0; i < World.get_num_objects<player>(); i++)
            {
                const player* pp = World.get_object<player>(i);
                const unsigned int handle = pp->mp_data;
                // Original: index 16 is accepted here (Mp_player_stats has 16 entries).
                if (handle == 0xFFFFFFFFu || (handle & 0xFFFF) > 16)
                {
                    continue;
                }
                const multi_player_stats* stats = &Mp_player_stats[handle & 0xFFFF];
                if (stats->round_score == 0)
                {
                    continue;
                }
                if ((int)stats->round_score > best_score)
                {
                    // Original: the game data is not checked for null.
                    leader = multi_game_data_get(handle, false, false)->player_uid;
                    best_score = stats->round_score;
                    num_best = 1;
                }
                else if (stats->round_score == best_score)
                {
                    num_best++;
                }
            }
            if (local_player != nullptr && best_score != (int)0x80000001 && num_best == 1 && !Winner_player_uid.get_internal_id().is_equal(leader.get_internal_id()))
            {
                Winner_player_uid = leader;
                multi_game_announcer_play_line(leader.get_internal_id().is_equal(My_player_uid.get_internal_id()) ? MAL_GEN_ALLY_NEW_LEAD : MAL_GEN_FFA_NEW_LEAD, 0xFFFFFFFF);
            }
        }
        else
        {
            // Team 0 leads unless team 1 has more points; a tie has no leader.
            multi_round_winner leader;
            leader.player_game_data_handle = 0;
            const unsigned int best_score = Mp_team_stats[0].round_score;
            if (Mp_team_stats[1].round_score > best_score)
            {
                leader.player_game_data_handle = 1;
            }
            else if (Mp_team_stats[1].round_score == best_score)
            {
                leader.player_game_data_handle = 0xFFFFFFFFu;
            }
            if (Multi_gameplay_mode != MGPM_SIEGE && leader.player_game_data_handle != Winner_last_check.player_game_data_handle && leader.player_game_data_handle != 0xFFFFFFFFu)
            {
                winner = leader;
                player* local_player = player_get_local_player();
                if (local_player != nullptr)
                {
                    if (human_teams_get_disposition(leader.team, local_player->current_team) == TEAM_DISPOSITION_HOSTILE)
                    {
                        multi_game_announcer_play_line(MAL_GEN_ENEMY_NEW_LEAD, 0xFFFFFFFF);
                    }
                    else if (human_teams_get_disposition(leader.team, local_player->current_team) == TEAM_DISPOSITION_ALLIED)
                    {
                        multi_game_announcer_play_line(MAL_GEN_ALLY_NEW_LEAD, 0xFFFFFFFF);
                    }
                }
            }
        }
        Winner_last_check = winner;
        New_lead_timer.set(1000);
    }

    if (Multiplayer && Server)
    {
        const bool common_over = multi_game_mode_is_round_over_common();
        if ((Multi_gameplay_mode_funcs[Multi_gameplay_mode].is_round_over != nullptr && Multi_gameplay_mode_funcs[Multi_gameplay_mode].is_round_over()) || common_over)
        {
            multi_round_winner winner;
            winner.player_game_data_handle = 0xFFFFFFFFu;
            if (Multi_gameplay_mode_funcs[Multi_gameplay_mode].get_round_winner == nullptr)
            {
                keen::debug::nativePrintArgs("All gameplay modes are required to have a get_round_winner callback.");
            }
            else
            {
                winner = Multi_gameplay_mode_funcs[Multi_gameplay_mode].get_round_winner();
            }
            if (Mp_last_man_standing)
            {
                player* last = multi_game_mode_get_last_man_standing();
                if (last != nullptr)
                {
                    if (current_mode_is_team_mode())
                    {
                        winner.team = last->current_team;
                    }
                    else
                    {
                        winner.player_game_data_handle = last->mp_data;
                    }
                }
            }
            if (Multi_gameplay_mode_funcs[Multi_gameplay_mode].round_over != nullptr)
            {
                Multi_gameplay_mode_funcs[Multi_gameplay_mode].round_over(winner);
            }
            multi_game_mode_round_over_common(winner);
            return false;
        }
        multi_gameplay_mode_check_lag();
    }
    return true;
}

// 0x0094D210 (:1580)
void multi_game_mode_handshake_common()
{
}

// 0x00992670 (:1586): a jump to multi_game_backpack_rack_render (the rack popups)
void multi_game_mode_render_common()
{
    multi_game_backpack_rack_render();
}

// 0x0094D220 (:1593)
void multi_game_mode_add_player_common(player* pp)
{
    object_set_visibility_plus_children(pp, Multi_match_info.match_state != MGMS_PREGAME && Multi_match_info.match_state != MGMS_COUNTDOWN);
}

// 0x00992680 (:1609): server: a leaving or respawning player's mines fade out and its singularity bombs go off
static void multi_game_mode_eliminate_player_mines_and_bombs(unsigned int player_handle)
{
    if (!Multiplayer || !Server)
    {
        return;
    }
    const int count = World.get_num_objects<projectile>();
    for (int i = 0; i < count; i++)
    {
        projectile* proj = World.get_object<projectile>(i);
        if (proj->owner_handle != player_handle || proj->pflags.fading)
        {
            continue;
        }
        if (proj->w_info->weapon_class == WEAPON_CLASS_THROWN_MINE)
        {
            proj->fade_stamp.set(1000);
            proj->pflags.fading = 1;
        }
        else if (proj->w_info->weapon_class == WEAPON_CLASS_SINGULARITY_BOMB)
        {
            proj->detonate_stamp.set(0);
            proj->detonator_handle = 0xFFFFFFFFu;
        }
    }
}

// 0x00992750 (:1626): server: a leaving or respawning player's remote charges fade out
static void multi_game_mode_fade_player_charges(unsigned int player_handle)
{
    if (!Multiplayer || !Server)
    {
        return;
    }
    const int count = World.get_num_objects<projectile>();
    for (int i = 0; i < count; i++)
    {
        projectile* proj = World.get_object<projectile>(i);
        if (proj->owner_handle == player_handle && !proj->pflags.fading && proj->w_info->weapon_class == WEAPON_CLASS_THROWN_CHARGE)
        {
            proj->fade_stamp.set(1000);
            proj->pflags.fading = 1;
        }
    }
}

// 0x009BA2A0 (:1641): a player left: the "player left" HUD message, its explosives and its backpack go
void multi_game_mode_remove_player_common(player* pp)
{
    if (Migration_state == MIGRATION_STATE_NONE && !object_is_local_player(pp))
    {
        const multi_game_data* data = multi_game_data_get(pp->mp_data, false, false);
        if (data != nullptr)
        {
            color team_color;
            char hex[16];
            wchar_t hex_wide[16];
            wchar_t colored_name[128];
            wchar_t shadow_name[128];
            wchar_t shadow_buffer[256];
            wchar_t colored_buffer[256];
            multi_game_render_get_team_color(team_color, MTCT_ICON, pp);
            str_color2s_hex(hex, sizeof(hex), team_color, false);
            copy_to_wide(hex_wide, hex, 16);
            _snprintf_safe(shadow_name, 128, "[color:#000000FF][image_tint:ui_hud_player_left] %ls", data->display_name);
            _snprintf_safe(colored_name, 128, "[color:#%ls][image:ui_hud_player_left] %ls[color:#FFFFFFFF]", hex_wide, data->display_name);
            _snprintf_safe(colored_buffer, 256, localization_get_display_string_req("HM_PLAYER_LEFT", nullptr), colored_name);
            _snprintf_safe(shadow_buffer, 256, localization_get_display_string_req("HM_PLAYER_LEFT", nullptr), shadow_name);
            hud_ui_multiplayer_add_player_died_message(colored_buffer, shadow_buffer, false);
        }
    }
    multi_game_mode_eliminate_player_mines_and_bombs(pp->get_handle());
    multi_game_mode_fade_player_charges(pp->get_handle());
    multi_object_backpack* backpack = pp->backpack;
    if (backpack != nullptr && backpack->get_player_attached_to() != nullptr)
    {
        if (backpack->get_backpack_rack() != nullptr)
        {
            multi_game_backpack_rack_backpack_dropped(backpack->get_backpack_rack(), backpack);
        }
        World.destroy_object(backpack, false);
    }
    hud_ui_multiplayer_player_removed(pp);
}

// Port helper (multi_game_mode_player_died_common's kill block, split out): the experience of a kill of an enemy, in
// rfg.exe's order. The parameters are the events' (mp_xp.xtbl; seconds, meters, fractions or counts); every event
// passes the kill's weapon and the killer's backpack. Original: the camera orientations are not checked for null.
static void multi_game_mode_award_kill_bonus_events(player* victim, player* killer, damaged_by_types damage_type, const weapon_info* w_info, const multi_player_stats* killer_stats, int weapon_id,
                                                    int backpack_id, int now)
{
    const unsigned int killer_data = killer->mp_data;
    // How the kill was done: melee, the special kill weapons (Multi_custom_weapon_names: the explosive object, debris,
    // the kill zones, the charge, the rocket hits), or a repair tool.
    if (damage_type == DBT_MELEE)
    {
        multi_game_xp_trigger_event(killer_data, MXE_MELEE_KILL, 1, weapon_id, backpack_id, false);
    }
    else if (w_info == weapons_get_weapon_info(Multi_custom_weapon_names[0]))
    {
        multi_game_xp_trigger_event(killer->mp_data, MXE_BARREL_KILL, 1, weapon_id, backpack_id, false);
    }
    else if (w_info == weapons_get_weapon_info(Multi_custom_weapon_names[1]))
    {
        multi_game_xp_trigger_event(killer->mp_data, MXE_DEBRIS_KILL, 1, weapon_id, backpack_id, false);
    }
    else if (w_info == weapons_get_weapon_info(Multi_custom_weapon_names[2]) || w_info == weapons_get_weapon_info(Multi_custom_weapon_names[4]))
    {
        multi_game_xp_trigger_event(killer->mp_data, MXE_CLIFF_KILL, 1, weapon_id, backpack_id, false);
    }
    else if (w_info == weapons_get_weapon_info(Multi_custom_weapon_names[6]))
    {
        multi_game_xp_trigger_event(killer->mp_data, MXE_STICKY_KILL, 1, weapon_id, backpack_id, false);
        if (victim->pflags.jetpack_started && killer == player_get_local_player())
        {
            stats_increment_by_one(STATS_MP_THE_HIGH_AND_MIGHTY);
        }
    }
    else if (w_info == weapons_get_weapon_info(Multi_custom_weapon_names[7]) || w_info == weapons_get_weapon_info(Multi_custom_weapon_names[8]))
    {
        multi_game_xp_trigger_event(killer->mp_data, MXE_DIRECT_HIT_KILL, 1, weapon_id, backpack_id, false);
    }
    else if (w_info != nullptr && w_info->ammo_type == WEAPON_AMMO_TYPE_REPAIR)
    {
        multi_game_xp_trigger_event(killer->mp_data, MXE_DOING_IT_WRONG, 1, weapon_id, backpack_id, false);
    }
    // The victim died by its own explosion (the server knows the owner).
    if (damage_type == DBT_EXPLOSION && Multiplayer && Server)
    {
        const player* owner = explosion_owner_of_last_explosion();
        if (owner != nullptr && owner == victim)
        {
            multi_game_xp_trigger_event(killer->mp_data, MXE_BACKFIRE, 1, weapon_id, backpack_id, false);
        }
    }
    if (victim->backpack != nullptr && victim->backpack->is_fully_stealthed())
    {
        multi_game_xp_trigger_event(killer->mp_data, MXE_GHOST_KILL, 1, weapon_id, backpack_id, false);
    }
    // An explosive the victim threw sticks to a living ally of the killer: once per saved ally.
    bool saved[16] = {};
    const int num_projectiles = World.get_num_objects<projectile>();
    for (int i = 0; i < num_projectiles; i++)
    {
        const projectile* proj = World.get_object<projectile>(i);
        if (proj->get_owner_handle() != victim->get_handle() || proj->w_info == nullptr || !proj->w_info->is_explosive())
        {
            continue;
        }
        const player* stuck = World.object_from_handle<player>(proj->get_parent());
        if (stuck == nullptr || stuck->is_killed() || !human_teams_are_allies(stuck, killer) || stuck->mp_data == 0xFFFFFFFFu)
        {
            continue;
        }
        bool& was_saved = saved[stuck->mp_data & 0xFFFF];
        if (!was_saved)
        {
            multi_game_xp_trigger_event(killer->mp_data, MXE_STICKY_SAVE, 1, weapon_id, backpack_id, false);
            was_saved = true;
        }
    }
    if (human_is_ragdolled_or_getting_up(victim) && damage_type == DBT_MELEE)
    {
        multi_game_xp_trigger_event(killer->mp_data, MXE_EASY_KILL, 1, weapon_id, backpack_id, false);
    }
    const float back_off_dist = multi_game_xp_get_event_parameter(MXE_BACK_OFF_KILL); // inlined
    if (human_combat_playing_combat_move(victim) && (double)vec_dist_squared(victim->pos, killer->pos) <= (double)back_off_dist * (double)back_off_dist)
    {
        multi_game_xp_trigger_event(killer->mp_data, MXE_BACK_OFF_KILL, 1, weapon_id, backpack_id, false);
    }
    if (human_is_ragdolled(victim) && human_is_in_air(victim))
    {
        multi_game_xp_trigger_event(killer->mp_data, MXE_KUNG_FU_KILL, 1, weapon_id, backpack_id, false);
    }
    if (human_is_jumping(victim))
    {
        multi_game_xp_trigger_event(killer->mp_data, MXE_BUNNY_KILL, 1, weapon_id, backpack_id, false);
    }
    // The victim killed the killer last, and the killer respawned less than the parameter's seconds ago.
    const float revenge_seconds = multi_game_xp_get_event_parameter(MXE_REVENGE_KILL); // inlined
    if (victim->get_handle() == killer->combat.last_killed_by_handle &&
        (unsigned int)(long long)((double)revenge_seconds * 1000.0) + killer->mp_last_respawn_time > Clock_current_time)
    {
        multi_game_xp_trigger_event(killer->mp_data, MXE_REVENGE_KILL, 1, weapon_id, backpack_id, false);
    }
    // Another kill within the parameter's seconds.
    const float multiple_seconds = multi_game_xp_get_event_parameter(MXE_MULTIPLE_KILLS); // inlined
    if (victim->get_handle() != killer->combat.last_kill_other_handle && killer->combat.last_kill_time_ms - (int)((double)multiple_seconds * -1000.0) > now)
    {
        multi_game_xp_trigger_event(killer->mp_data, MXE_MULTIPLE_KILLS, 1, weapon_id, backpack_id, false);
    }
    // The victim had just killed an ally of the killer (within the parameter's seconds).
    const player* victims_victim = World.object_from_handle<player>(victim->combat.last_kill_handle);
    if (victims_victim != nullptr && victims_victim != killer && human_teams_are_allies(victims_victim, killer))
    {
        const float retaliation_seconds = multi_game_xp_get_event_parameter(MXE_RETALIATION_KILL); // inlined
        if (victim->combat.last_kill_time_ms - (int)((double)retaliation_seconds * -1000.0) > timer_get(TIMER_MILLISECONDS))
        {
            multi_game_xp_trigger_event(killer->mp_data, MXE_RETALIATION_KILL, 1, weapon_id, backpack_id, false);
        }
    }
    const multi_object_backpack* killer_backpack = killer->backpack;
    if (killer_backpack != nullptr && killer_backpack->get_backpack_abilities()->rhino && killer_backpack->did_rhino_hit_mover_this_run())
    {
        multi_game_xp_trigger_event(killer->mp_data, MXE_CHARGE_KILL, 1, weapon_id, backpack_id, false);
    }
    // A dead killer whose death was over the parameter's seconds ago (a late projectile).
    const float zombie_seconds = multi_game_xp_get_event_parameter(MXE_ZOMBIE_KILL); // inlined
    if (human_is_dead(killer) && killer->combat.last_killed_time_ms - (int)((double)zombie_seconds * -1000.0) < now)
    {
        multi_game_xp_trigger_event(killer->mp_data, MXE_ZOMBIE_KILL, 1, weapon_id, backpack_id, false);
    }
    // The killer has been in the air longer than the parameter's seconds.
    const float flying_seconds = multi_game_xp_get_event_parameter(MXE_FLYING_KILL); // inlined
    if (human_is_in_air(killer) && killer->air_time.time_since() > (int)((double)flying_seconds * 1000.0))
    {
        multi_game_xp_trigger_event(killer->mp_data, MXE_FLYING_KILL, 1, weapon_id, backpack_id, false);
    }
    // The killer's allies: a knocked down ally near the victim or the killer is rescued, allies near the killer are the
    // pack, allies who did the assist share of the victim's health get the assist, an ally who killed an enemy within
    // 2 s nearby made a synchronized kill. Meanwhile the round leader in kills (the killer loses ties to others).
    float rescue_dist = multi_game_xp_get_event_parameter(MXE_RESCUE_KILL); // inlined
    rescue_dist = rescue_dist * rescue_dist;
    float pack_dist = multi_game_xp_get_event_parameter(MXE_PACK_HUNTER); // inlined
    pack_dist = pack_dist * pack_dist;
    float synchronized_dist = multi_game_xp_get_event_parameter(MXE_SYNCHRONIZED_KILL); // inlined
    synchronized_dist = synchronized_dist * synchronized_dist;
    const float assist_fraction = multi_game_xp_get_event_parameter(MXE_KILL_ASSIST); // inlined
    const player* leader = nullptr;
    // Magic number: rfg.exe starts at INT_MIN + 1.
    int leader_kills = -0x7FFFFFFF;
    const int count = World.get_num_objects<player>();
    for (int i = 0; i < count; i++)
    {
        player* pp = World.get_object<player>(i);
        if (human_teams_are_allies(pp, killer) && pp != killer)
        {
            if (!pp->is_killed())
            {
                if (human_is_ragdolled_or_getting_up(pp) && (vec_dist_squared(pp->pos, victim->pos) <= rescue_dist || vec_dist_squared(pp->pos, killer->pos) <= rescue_dist))
                {
                    multi_game_xp_trigger_event(killer->mp_data, MXE_RESCUE_KILL, 1, weapon_id, backpack_id, false);
                }
                if (vec_dist_squared(pp->pos, killer->pos) <= pack_dist)
                {
                    multi_game_xp_trigger_event(pp->mp_data, MXE_PACK_HUNTER, 1, weapon_id, backpack_id, false);
                }
            }
            if (human_teams_are_enemies(pp->current_team, victim->current_team) && victim != pp && pp->mp_data != 0xFFFFFFFFu &&
                assist_fraction <= victim->mp_player_damage_history[pp->mp_data & 0xFFFF] / (float)victim->max_hit_points)
            {
                multi_game_xp_trigger_event(pp->mp_data, MXE_KILL_ASSIST, 1, weapon_id, backpack_id, false);
            }
            const player* pp_victim = World.object_from_handle<player>(pp->combat.last_kill_other_handle);
            // Magic number: the synchronized kill window is 2 s.
            if (pp_victim != nullptr && human_teams_are_enemies(pp_victim->current_team, pp->current_team) && now <= pp->combat.last_kill_other_time_ms + 2000 &&
                vec_dist_squared(pp->pos, killer->pos) <= synchronized_dist)
            {
                multi_game_xp_trigger_event(killer->mp_data, MXE_SYNCHRONIZED_KILL, 1, weapon_id, backpack_id, false);
                multi_game_xp_trigger_event(pp->mp_data, MXE_SYNCHRONIZED_KILL, 1, weapon_id, backpack_id, false);
            }
        }
        const multi_player_stats* stats = multi_game_stats_get_player_stats(pp->mp_data);
        if (stats != nullptr && (leader_kills < stats->round_kills || (leader == killer && leader_kills <= stats->round_kills)))
        {
            leader = pp;
            leader_kills = stats->round_kills;
        }
    }
    if (leader == victim)
    {
        multi_game_xp_trigger_event(killer->mp_data, MXE_WHOS_NUMBER_ONE, 1, weapon_id, backpack_id, false);
    }
    if (victim->backpack != nullptr && killer->backpack == nullptr)
    {
        multi_game_xp_trigger_event(killer->mp_data, MXE_BACKWHAT, 1, weapon_id, backpack_id, false);
    }
    if (victim->backpack != nullptr && killer->backpack != nullptr && victim->backpack->get_backpack_info()->unique_id == killer->backpack->get_backpack_info()->unique_id)
    {
        multi_game_xp_trigger_event(killer->mp_data, MXE_DOUBLEBACK, 1, weapon_id, backpack_id, false);
    }
    // Magic number: a standing killer moves under 0.0001 on each axis.
    vector vel = ZERO_VECTOR;
    human_controller_get_vel(killer, vel);
    if (fabsf(vel.x - ZERO_VECTOR.x) <= 0.0001f && fabsf(vel.y - ZERO_VECTOR.y) <= 0.0001f && fabsf(vel.z - ZERO_VECTOR.z) <= 0.0001f)
    {
        multi_game_xp_trigger_event(killer->mp_data, MXE_CAMPER, 1, weapon_id, backpack_id, false);
    }
    // A long distance kill: with a projectile weapon (Hail Mary) or a bullet weapon (sharpshooter).
    const float hail_mary_dist = multi_game_xp_get_event_parameter(MXE_HAIL_MARY); // inlined
    if (w_info != nullptr && w_info->ammo_type != WEAPON_AMMO_TYPE_BULLET &&
        (double)hail_mary_dist * (double)hail_mary_dist <= (double)vec_dist_squared(killer->pos, victim->pos))
    {
        multi_game_xp_trigger_event(killer->mp_data, MXE_HAIL_MARY, 1, weapon_id, backpack_id, false);
    }
    const float sharpshooter_dist = multi_game_xp_get_event_parameter(MXE_SHARPSHOOTER); // inlined
    if (w_info != nullptr && w_info->ammo_type == WEAPON_AMMO_TYPE_BULLET &&
        (double)sharpshooter_dist * (double)sharpshooter_dist <= (double)vec_dist_squared(killer->pos, victim->pos))
    {
        multi_game_xp_trigger_event(killer->mp_data, MXE_SHARPSHOOTER, 1, weapon_id, backpack_id, false);
    }
    // A steep kill: the direction to the victim is at most the parameter (a cosine) from the horizontal.
    vector to_victim = victim->pos - killer->pos;
    to_victim.normalize_safe();
    const vector flat = to_victim.copy_flatten_normalize();
    if (flat.x * to_victim.x + flat.y * to_victim.y + flat.z * to_victim.z <= multi_game_xp_get_event_parameter(MXE_VERTIGO)) // inlined
    {
        multi_game_xp_trigger_event(killer->mp_data, MXE_VERTIGO, 1, weapon_id, backpack_id, false);
    }
    // Both cameras look the same way along the line (the victim faced away).
    const matrix* killer_orient = nullptr;
    const matrix* victim_orient = nullptr;
    player_get_camera_pos_and_orient(killer, nullptr, &killer_orient);
    player_get_camera_pos_and_orient(victim, nullptr, &victim_orient);
    if (killer_orient->fvec * flat >= 0.0f && victim_orient->fvec * flat >= 0.0f)
    {
        multi_game_xp_trigger_event(killer->mp_data, MXE_BLINDSIDE, 1, weapon_id, backpack_id, false);
    }
    // At full health and no longer respawn invulnerable.
    if ((!killer->mp_time_until_player_leaves_invulnerable.valid() || killer->mp_time_until_player_leaves_invulnerable.elapsed()) &&
        fl_equal_robust(killer->hit_points / (float)killer->max_hit_points, 1.0f, 0.0001f))
    {
        multi_game_xp_trigger_event(killer->mp_data, MXE_NO_PROBLEM, 1, weapon_id, backpack_id, false);
    }
    // The killer's active backpack: Commando with a melee weapon, Thrust's own attack (from above or below), Tremor.
    if (killer_backpack != nullptr && killer_backpack->is_activated())
    {
        const int id = killer_backpack->get_backpack_info()->unique_id;
        multi_xp_events event = MXE_NONE;
        if (multi_game_backpack_get_backpack_id("Commando") == id && w_info != nullptr && w_info->ammo_type == WEAPON_AMMO_TYPE_MELEE)
        {
            event = MXE_POWER_OVERWHELMING;
        }
        else if (multi_game_backpack_get_backpack_id("Thrust") == id && w_info == killer_backpack->get_backpack_abilities()->w_info)
        {
            event = killer_backpack->get_jump_boost_direction_up() ? MXE_DEATH_FROM_BELOW : MXE_DEATH_FROM_ABOVE;
        }
        else if (multi_game_backpack_get_backpack_id("Tremor") == id)
        {
            event = MXE_SHAKEN_NOT_STIRRED;
        }
        if (event != MXE_NONE)
        {
            multi_game_xp_trigger_event(killer->mp_data, event, 1, weapon_id, id, false);
        }
    }
    const multi_object_backpack* victim_backpack = victim->backpack;
    if (victim_backpack != nullptr && victim_backpack->is_activated() && victim_backpack->get_backpack_abilities()->health_regen)
    {
        multi_game_xp_trigger_event(killer->mp_data, MXE_TASTE_THE_RAINBOW, 1, weapon_id, backpack_id, false);
    }
    const int streak = (int)multi_game_xp_get_event_parameter(MXE_KILL_STREAK); // inlined
    if (streak != 0 && streak <= killer_stats->kills_in_a_row)
    {
        multi_game_xp_trigger_event(killer->mp_data, MXE_KILL_STREAK, 1, weapon_id, backpack_id, false);
    }
    multi_game_xp_trigger_event(killer->mp_data, MXE_KILL, 1, weapon_id, backpack_id, false);
}

// 0x009EA1C0 (:1680)
// A player died (every peer; the server first while the match timer runs). A client plays the pain sound and a nano
// rifle kill's fade. The cameras follow the killer; both need multiplayer data and stats. On the server a suicide
// within Recent_hit_time_ms of another player's hit counts for that player (with its weapon, unless a kill zone), and
// a kill zone suicide gets the suicide weapon. Ending the victim's kill streak gives the streak stopper experience; the
// kill goes to the stats. A kill of an enemy gives the killer the kill experience and the bonus events it earns (see
// below; some go to the killer's allies); the local killer or victim gets the kill streak announcement, everyone the
// HUD kill message. The victim's respawn waits the options' respawn time (5 s more for a suicide, scaled by the
// rubberbanding), and the server tells everyone (multi_game_human_died).
void multi_game_mode_player_died_common(player*& victim, player*& killer, damaged_by_types& damage_type, const weapon_info*& w_info)
{
    if (victim == nullptr || killer == nullptr)
    {
        return;
    }
    if (Multiplayer)
    {
        if (Server)
        {
            if (Timer_time.elapsed() || !Timer_time.valid()) // inlined
            {
                return;
            }
        }
        else
        {
            human_play_pain_sound(victim, killer, w_info, damage_type, false);
            if (w_info != nullptr && w_info->weapon_class == WEAPON_CLASS_NANO_RIFLE && damage_type == DBT_BULLET)
            {
                victim->start_nano_rifle_fade_out(true, killer, false, victim->pos);
            }
        }
    }
    multi_game_camera_player_killed(victim, killer);
    if (multi_game_data_get(victim->mp_data, false, false) == nullptr || multi_game_data_get(killer->mp_data, false, false) == nullptr)
    {
        return;
    }
    const multi_player_stats* victim_stats = multi_game_stats_get_player_stats(victim->mp_data);
    const multi_player_stats* killer_stats = multi_game_stats_get_player_stats(killer->mp_data);
    if (victim_stats == nullptr || killer_stats == nullptr)
    {
        return;
    }
    const int now = timer_get(TIMER_MILLISECONDS);
    multi_game_mode_fade_player_charges(victim->get_handle());
    if (Multiplayer && Server && victim == killer && timer_get_since(victim->combat.last_hit_by_other_time_ms, TIMER_MILLISECONDS) < Recent_hit_time_ms)
    {
        player* hitter = World.object_from_handle<player>(victim->combat.last_hit_by_other_handle);
        if (hitter != nullptr)
        {
            killer = hitter;
            if (!victim->hflags.killed_by_killzone)
            {
                w_info = weapons_get_weapon_info(victim->combat.last_hit_by_other_weapon_handle);
            }
        }
    }
    if (victim->hflags.killed_by_killzone)
    {
        const weapon_info* cliff_kill = weapons_get_weapon_info(Multi_custom_weapon_names[2]);
        const weapon_info* mine_kill = weapons_get_weapon_info(Multi_custom_weapon_names[4]);
        if (victim == killer)
        {
            if (w_info == cliff_kill)
            {
                w_info = weapons_get_weapon_info(Multi_custom_weapon_names[3]); // the cliff suicide
            }
            else if (w_info == mine_kill)
            {
                w_info = weapons_get_weapon_info(Multi_custom_weapon_names[5]); // the mine suicide
            }
        }
    }
    const int weapon_id = w_info != nullptr ? w_info->unique_id : 0xFF;
    const int backpack_id = killer->backpack != nullptr ? killer->backpack->get_backpack_info()->unique_id : -1;
    if (victim != killer && human_teams_are_enemies(victim, killer))
    {
        // The victim's streak (before this death counts) reached the event's parameter.
        if ((int)multi_game_xp_get_event_parameter(MXE_STREAK_STOPPER) <= victim_stats->kills_in_a_row) // inlined
        {
            multi_game_xp_trigger_event(killer->mp_data, MXE_STREAK_STOPPER, 1, weapon_id, backpack_id, false);
        }
    }
    multi_game_stats_record_kill(killer, victim, w_info, damage_type);
    if (victim != killer && human_teams_are_enemies(victim, killer))
    {
        multi_game_mode_award_kill_bonus_events(victim, killer, damage_type, w_info, killer_stats, weapon_id, backpack_id, now);
    }
    if (killer == player_get_local_player() && killer->mp_data != 0xFFFFFFFFu)
    {
        announce_skill((int)(killer->mp_data & 0xFFFF), killer);
    }
    if (killer != victim && victim == player_get_local_player() && victim->mp_data != 0xFFFFFFFFu)
    {
        announce_skill((int)(victim->mp_data & 0xFFFF), victim);
    }
    hud_ui_multiplayer_player_died(victim, killer, damage_type, w_info);
    // Magic number: a suicide waits 5 s more.
    int respawn_ms = (int)Mp_server_options.generic_options.respawn_time_seconds * 1000;
    if (victim == killer)
    {
        respawn_ms += 5000;
    }
    const float scale = multi_game_rubberbanding_get_stat_value(victim->mp_data, MRS_RESPAWN_TIME);
    victim->mp_time_until_player_can_respawn.set((int)((float)respawn_ms * scale));
    if (Multiplayer && Server)
    {
        multi_game_human_died(victim, killer, damage_type, w_info);
    }
}

// 0x009F7CD0 (:2137)
// A player respawned at a player start (every peer): the local player gets its team's voice and the respawn cue; its
// mines and charges go, the cameras follow, the damage history and ladder delays clear, it leaves the spectator view.
// At the start: the spawn point and time are remembered, the corrections reset, its attached projectiles go, it shows,
// its health follows the rubberbanding. The server picks a random unlocked rack's backpack when the options say so,
// sends the respawn (PT_RESPAWN_PLAYER) and clears its weapon changes; the mode's starting backpack is created and
// picked up, the player is invulnerable for Mp_respawn_invulnerable_time_ms and its current damage stat clears.
void multi_game_mode_player_respawn_common(player* pp, int player_start_index)
{
    if (object_is_local_player(pp))
    {
        const char* persona = nullptr;
        if (pp->current_team == HUMAN_TEAM_GUERILLA)
        {
            persona = "PCM01";
        }
        else if (pp->current_team == HUMAN_TEAM_EDF)
        {
            persona = "PCM02";
        }
        if (persona != nullptr)
        {
            pp->voice = human_voice_persona_from_name(persona);
        }
        audiolib_cue_play(Ga_cue_ids.multiplayer_player_respawn, nullptr, nullptr);
    }
    multi_game_mode_eliminate_player_mines_and_bombs(pp->get_handle());
    multi_game_mode_fade_player_charges(pp->get_handle());
    multi_game_camera_player_respawn(pp);
    memset(pp->mp_player_damage_history, 0, sizeof(pp->mp_player_damage_history));
    pp->mp_ladder_state.remote_ladder_mount_delay.set(0);
    pp->mp_ladder_state.local_ladder_mount_delay.set(0);
    multi_exit_spectator_view(pp, true);
    pp->pflags.mp_display_respawn_msg = 0;
    const unsigned int now = Clock_current_time;
    pp->pflags.mp_has_reloaded = 0;
    // Original: the index is not checked.
    player_start* start = World.get_object<player_start>(player_start_index);
    if (start == nullptr)
    {
        return;
    }
    pp->mp_last_spawn_point_handle = start->get_handle();
    pp->mp_last_respawn_time = now;
    pp->mp_edge_correction_info.increasing_velocity_count = 0;
    pp->mp_edge_correction_info.vertical_velocity = 0.0f;
    pp->mp_blocked_info.raycast_blocked_count = 0;
    for (int i = 0; i < World.get_num_objects<projectile>(); i++)
    {
        projectile* proj = World.get_object<projectile>(i);
        if (proj->get_parent() == pp->get_handle())
        {
            proj->remote_obj_flags.client_destroy_permission = 1;
            World.destroy_object(proj, false);
        }
    }
    object_set_visibility_plus_children(pp, true);
    pp->mp_time_until_player_can_respawn.invalidate();
    const float health_scale = multi_game_rubberbanding_get_stat_value(pp->mp_data, MRS_HEALTH);
    pp->max_hit_points = (int)((float)pp->initial_max_hit_points * health_scale);
    pp->refill_health(1.0f);
    if (Multiplayer && Server)
    {
        if (Mp_server_options.generic_options.starting_backpack == MGSBO_RANDOM)
        {
            farray<int, 16> backpacks;
            for (int i = World.get_num_objects<multi_object_marker>() - 1; i >= 0; i--)
            {
                if (Multi_markers_prioritized[i]->get_marker_type() == MMT_BACKPACK_RACK && !multi_game_settings_is_backpack_locked(Multi_markers_prioritized[i]->backpack_id))
                {
                    backpacks.add_unique(Multi_markers_prioritized[i]->backpack_id);
                }
            }
            int backpack = -1;
            if (backpacks.get_random_element(backpack))
            {
                pp->mp_respawn_backpack_type = backpack;
            }
        }
        multi_game_respawn_send_respawn_to_all(pp, player_start_index, false);
        multi_game_player_clear_weapon_changes(pp);
    }
    multi_game_equipment_loadout loadout;
    multi_gameplay_mode_get_default_equipment(pp, &loadout);
    if (loadout.starting_backpack_type != -1)
    {
        multi_object_backpack* backpack = multi_game_backpack_create(loadout.starting_backpack_type, pp->pos, pp->orient);
        if (backpack == nullptr)
        {
            return;
        }
        backpack->event_player_picked_up(pp, false, false);
    }
    pp->mp_time_until_player_leaves_invulnerable.set(Mp_respawn_invulnerable_time_ms);
    player_set_player_invulnerable(pp, true);
    multi_player_stats* stats = multi_game_stats_get_player_stats(pp->mp_data);
    if (stats != nullptr)
    {
        stats->current_damage = 0.0f;
    }
}

// 0x0094D250 (:2249)
float multi_game_mode_adjust_respawn_weight_common(const player* pp, const player_start* pstart)
{
    return 0.0f;
}

// 0x00975C80 (:2254): the damage and repair stats, in structural mass / 4000
void multi_game_mode_building_damage_change_common(player* pp, rfg_mover* building, float damage_delta)
{
    if (pp == nullptr)
    {
        return;
    }
    const float delta = damage_delta * 0.00025f;
    // Original: index 16 is accepted here (Mp_player_stats has 16 entries).
    if (pp->mp_data == 0xFFFFFFFFu || (pp->mp_data & 0xFFFF) > 16)
    {
        return;
    }
    multi_player_stats* stats = &Mp_player_stats[pp->mp_data & 0xFFFF];
    if (delta < 0.0f)
    {
        stats->total_damage -= delta;
        stats->current_damage -= delta;
        if (stats->current_damage - stats->highest_damage >= 0.0f)
        {
            stats->highest_damage = stats->current_damage;
        }
        return;
    }
    stats->total_repair += delta;
}

// 0x0094D260 (:2276)
bool multi_game_mode_player_can_swap_weapons_common(const player* pp)
{
    return true;
}

// 0x0094D270 (:2281)
bool multi_game_mode_player_can_attack_common(const player* pp, const weapon_info* w_info, damaged_by_types attack_type)
{
    return true;
}

// 0x0094D280 (:2286)
void multi_game_mode_get_default_equipment_common(const player* pp, multi_game_equipment_loadout* loadout)
{
    loadout->replace_starting_weapon = Mp_server_options.generic_options.replace_starting_weapon;
    loadout->starting_weapon = Mp_server_options.generic_options.starting_weapon;
    loadout->third_weapon = Mp_server_options.generic_options.third_weapon;
    loadout->fourth_weapon = Mp_server_options.generic_options.fourth_weapon;
    if (Mp_server_options.generic_options.starting_backpack == MGSBO_RANDOM)
    {
        loadout->starting_backpack_type = pp->mp_respawn_backpack_type;
        return;
    }
    loadout->starting_backpack_type = Mp_server_options.generic_options.starting_backpack_type;
}

// 0x00975D50 (:2302)
int multi_game_mode_get_time_ms_common()
{
    return Timer_time.time_until();
}

// 0x00975D60 (:2307)
void multi_game_mode_countdown_begin_common()
{
}

// 0x009DB740 (:2332): server: a player joined the round in progress: the round state, a weapon, a spawn and the stats
void multi_game_mode_round_begin_common_jip(multi_connection* cp, unsigned int game_data_handle)
{
    if (game_data_handle == 0xFFFFFFFFu)
    {
        return;
    }
    Multi_match_info.max_num_players = 0;
    send_backpack_rack_sync(cp);
    send_round_begin_packet(cp);
    int count = World.get_num_objects<player>();
    for (int i = 0; i < count; i++)
    {
        player* pp = World.get_object<player>(i);
        if (pp->mp_data == game_data_handle)
        {
            pp->pflags.mp_ignore_player_for_respawn_collisions = 1;
            player_set_player_invulnerable(pp, false);
        }
    }
    count = World.get_num_objects<player>();
    for (int i = 0; i < count; i++)
    {
        player* pp = World.get_object<player>(i);
        const multi_game_data* data = multi_game_data_get(pp->mp_data, false, false);
        if (data == nullptr)
        {
            continue;
        }
        if (pp->mp_data != game_data_handle)
        {
            multi_game_respawn_send_respawn_to_player(cp, pp, 0, false);
            continue;
        }
        // Original: current_level is not checked here.
        if (Mp_server_options.get_level()->num_player_weapons > 0 && data->desired_weapon_inv_item_info_handle == 0xFFFFFFFFu)
        {
            const unsigned int weapon = Mp_server_options.get_level()->player_weapons[int_rand_range(0, Mp_server_options.get_level()->num_player_weapons - 1)];
            multi_game_player_set_desired_weapon(multi_session_get_by_type(ST_GAME), data->handle, weapon);
        }
        player_respawn(pp, multi_game_respawn_get_best_spawn_point(pp, true), true);
        pp->pflags.mp_ignore_player_for_respawn_collisions = 0;
        multi_game_player_weapons_sync(cp);
        const int player_index = multi_game_data_get_player_index(game_data_handle);
        if ((unsigned int)(player_index - 1) < 16)
        {
            multi_game_stats_match_init_jip((unsigned short)player_index);
            multi_game_stats_sync_jip_player(cp);
        }
        multi_session* session = multi_session_get_by_type(ST_GAME);
        if (session != nullptr)
        {
            session->force_migration_host_update();
        }
    }
    multi_gameplay_mode_round_begin_jip(cp);
}

// 0x009EB660 (:2405): the countdown ran out (server) or the round begin packet arrived (client)
void multi_game_mode_round_begin_common()
{
    Multi_join_in_progress = false;
    multi_game_stats_round_init();
    for (int i = 0; i < 52; i++)
    {
        Multi_announcer_lines[i].last_played_time.invalidate();
    }
    // A vetoed map counts against the playlist in public matchmaking games.
    if (!Spectating && Vetoes_allowed)
    {
        const multi_session_info_base* info = Lobby_session->get_info();
        if (!info->private_match && !info->systemlink_or_lan && Lobby_state >= 7 && Vetoed_map && !multi_game_stats_are_finalizing())
        {
            // Original: the local player is not checked for null.
            const player* local_player = player_get_local_player();
            if (local_player->mp_data != 0xFFFFFFFFu && (local_player->mp_data & 0xFFFF) < 16)
            {
                Mp_player_stats[local_player->mp_data & 0xFFFF].undesired_playlist = 1;
            }
        }
    }
    multi_game_timer_set(Multi_match_info.round_time_ms);
    Multi_match_info.setting_up_round = false;
    Multi_match_info.round_winner.player_game_data_handle = 0xFFFFFFFFu;
    multi_game_backpack_rack_round_begin();
    if (Multiplayer && Server)
    {
        send_round_begin_packet(nullptr);
        // Destruction is enabled from here on.
        rfg_dyn_add_pre_collide_destruction_mode(nullptr, 0, false);
        multi_game_respawn_all_players(true);
        Logging_round_handle = log_start_event_mp_round(gamelog_get_player_checksum(), gamelog_get_logging_time(), Multi_match_info.current_round);
    }
    Winner_last_check.player_game_data_handle = 0xFFFFFFFFu;
    Mgm_allow_migration.set(10000);
    Multi_match_info.match_state = MGMS_GAMEPLAY;
    if (Spectating && Current_mode_camera != MCM_ACTION)
    {
        Current_mode_camera = MCM_ACTION;
        Camera_change_timestamp.invalidate();
        Current_view_mode = MCVM_DUMB;
    }
}

// 0x009AB3E0 (:2460)
static void move_to_endgame_sync()
{
    Multi_match_info.synchronizing_endgame = true;
    Multi_match_info.match_state = MGMS_ENDGAME_SYNC;
    console_change_chat_window_pos(VCP_ENDGAME);
    if (Multiplayer && Server)
    {
        Multi_match_info.server_endgame_sync_wait_timestamp.set(3000);
        multi_connection* me = Lobby_session == nullptr ? nullptr : Lobby_session->get_me_connection();
        if (me != nullptr)
        {
            tracked_bandwidth_info_set_info(My_player_uid, me->calculated_upstream_bits_sec, me->calculated_upstream_bits_sec, 0);
        }
        return;
    }
    Client_authorized_to_send_endgame_stats = true;
}

// 0x009FC970 (:2481): the round is over (the server decides and tells the clients, PT_MODE_COMMON round over): the
// multiplayer camera, the winner, the finished match's experience for every player (the last round), the announcer's
// round or match over and the match result, the round win, our stats start to finalize, the backpacks go, the endgame
// sync begins.
void multi_game_mode_round_over_common(multi_round_winner winner)
{
    multi_game_destruction_update_jip_status(true);
    const bool last_round = multi_game_mode_is_last_round();
    player* local_player = player_get_local_player();
    if (local_player != nullptr)
    {
        player_set_zoom_state(local_player, PZS_OFF, true);
    }
    multi_game_camera_start();
    if (Mp_last_man_standing && multi_game_mode_get_last_man_standing() != nullptr)
    {
        multi_player_menu_system_pull_up_no_opponents_message_box();
    }
    hud_reset_damage_indicators();
    Multi_match_info.round_winner = winner;
    const bool match_over = multi_game_mode_is_last_round();
    if (match_over)
    {
        const int count = World.get_num_objects<player>();
        for (int i = 0; i < count; i++)
        {
            multi_game_xp_trigger_event(World.get_object<player>(i)->mp_data, MXE_FINISH_MATCH, 1, 0xFF, -1, false); // inlined
        }
    }
    multi_game_announcer_play_line(match_over ? MAL_GEN_MATCH_OVER : MAL_GEN_ROUND_OVER, 0xFFFFFFFF);
    if (last_round)
    {
        multi_announcer_lines line;
        if (current_mode_is_team_mode())
        {
            line = winner.player_game_data_handle == 0 ? MAL_GEN_GUERRILLAS_WIN_MATCH : winner.player_game_data_handle == 1 ? MAL_GEN_EDF_WIN_MATCH : MAL_GEN_TIE_MATCH;
        }
        else
        {
            const multi_game_status status = multi_game_mode_get_mode_status(false);
            line = status == MGS_TIE ? MAL_GEN_TIE_MATCH : status == MGS_WIN ? MAL_GEN_ALLY_WIN_MATCH : MAL_GEN_ENEMY_WIN_MATCH;
        }
        multi_game_announcer_play_line(line, 0xFFFFFFFF);
    }
    multi_game_mode_update_round_wins_stat(winner);
    if (Multiplayer && Server)
    {
        send_round_over_packet(winner);
        // 0x1C: the round event
        const int round_handle = Logging_round_handle;
        if (log_record_event(0x1C))
        {
            log_stop_continuous_event(round_handle);
        }
        Logging_round_handle = 0;
    }
    multi_game_stats_start_finalizing();
    vint_bms_release(Empty_checkbox_image);
    multi_game_backpack_round_over();
    for (int i = 0; i < World.get_num_objects<player>(); i++)
    {
        player_stop_material_effect(World.get_object<player>(i));
    }
    // The HUD's round over event: the winning team.
    if (last_round && Ui_match_events[0].event != nullptr && current_mode_is_team_mode())
    {
        const vint_variant value((int)winner.player_game_data_handle);
        Ui_match_events[0].event->set_element(0, value);
    }
    move_to_endgame_sync(); // inlined
}

// 0x009AB460 (:2637)
bool multi_game_mode_is_round_over_common()
{
    bool over = false;
    if (!Multiplayer)
    {
        return false;
    }
    if (Server)
    {
        // The time limit (0: none), the point limit, nobody left, or one side left.
        if ((Timer_time.time_until() < 1 || !Timer_time.valid()) && Mp_server_options.generic_options.round_time_limit_minutes != 0)
        {
            over = true;
        }
        if (multi_game_mode_point_limit_reached())
        {
            over = true;
        }
        if (World.get_num_objects<player>() == 0)
        {
            over = true;
        }
        if (Mp_last_man_standing && multi_game_mode_get_last_man_standing() != nullptr)
        {
            over = true;
        }
    }
    if (!Server && Multi_match_info.synchronizing_endgame)
    {
        return true;
    }
    return over;
}

// 0x009AB4E0 (:2673)
bool multi_game_mode_get_round_winner_common(multi_round_winner* winner)
{
    if (!Mp_last_man_standing)
    {
        return false;
    }
    player* last = multi_game_mode_get_last_man_standing();
    if (last == nullptr)
    {
        return false;
    }
    if (current_mode_is_team_mode())
    {
        winner->team = last->current_team;
        return true;
    }
    winner->player_game_data_handle = last->mp_data;
    return true;
}

// 0x009EB7D0 (:2695): before a host migration every backpack is dropped (the new server recreates them); the server
// restarts each wearer's equip log event
void multi_game_mode_server_migration_pre_common()
{
    const int count = World.get_num_objects<player>();
    for (int i = 0; i < count; i++)
    {
        player* pp = World.get_object<player>(i);
        if (pp->backpack == nullptr)
        {
            continue;
        }
        if (Multiplayer && Server)
        {
            log_stop_event_backpack_equipped(pp->backpack_equip_log_handle);
            const char player_xp_level = gamelog_get_player_xp_level(pp);
            const int backpack_crc = gamelog_get_backpack_crc(pp->backpack);
            const char player_index = gamelog_get_player_index((const human*)pp);
            const int time = gamelog_get_logging_time();
            pp->backpack_equip_log_handle = log_start_event_backpack_equipped(gamelog_get_player_checksum(), time, player_index, backpack_crc, player_xp_level);
        }
        pp->backpack->event_player_dropped(false);
    }
}

// 0x009AB540 (:2712): after a host migration: a round that was ending syncs again; in gameplay the new server
// respawns the items that were waiting to
void multi_game_mode_server_migration_post_common()
{
    if (Multi_match_info.match_state == MGMS_ENDGAME_SYNC || Multi_match_info.match_state == MGMS_WAIT_ON_SYNC)
    {
        move_to_endgame_sync();
        return;
    }
    if (Multi_match_info.match_state != MGMS_GAMEPLAY || !Multiplayer || !Server)
    {
        return;
    }
    const int count = World.get_num_objects<item>();
    for (int i = 0; i < count; i++)
    {
        item* itemp = World.get_object<item>(i);
        if (itemp->waiting_to_respawn())
        {
            itemp->respawn();
        }
    }
}

// 0x009EB8B0 (:2733): the new server respawns the dead players (in gameplay) and sends everyone the scores
// (PT_MODE_COMMON scores)
void multi_game_mode_server_migration_update_clients_common()
{
    if (!Multiplayer || !Server)
    {
        return;
    }
    if (Multi_match_info.match_state == MGMS_GAMEPLAY)
    {
        multi_game_respawn_all_players(false);
    }
    unsigned char packet[1200];
    packet[0] = PT_MODE_COMMON;
    unsigned short packet_size = packet_header_size(PT_MODE_COMMON);
    packet[packet_size++] = Mode_common_scores;
    multi_game_stats_write_all_player_scores(packet, packet_size);
    multi_game_stats_write_all_team_scores(packet, packet_size);
    packet_set_size(packet, packet_size);
    multi_io_send_to_all_reliable(Game_session, packet, packet_size);
}

// 0x00975D70 (:2762): the mode's info text, until the mode's played-count stat says it was seen
void multi_game_mode_hud_init_common()
{
    const multi_gameplay_mode mode = Multiplayer ? Multi_gameplay_mode : MGPM_NONE;
    // 0x152: the first multiplayer mode played stat, in MGPM_* order
    if (stats_get_stat_value((tracked_stats_indices)(mode + 0x152)) > 0)
    {
        return;
    }
    hud_ui_multiplayer_info_text_enter();
}

// 0x009FE2C0 (:2773): PT_MODE_COMMON (packet_data: the subtype byte)
void multi_game_mode_process_mode_common(unsigned char* packet_data, const sender& from)
{
    const bool join_in_progress = Multi_join_in_progress;
    int packet_offset = 1;
    switch (packet_data[0])
    {
    case Mode_common_countdown_start:
    {
        // The countdown runs from the server's start time; past its end the round time absorbs the difference.
        const unsigned int start_time = read_u32_be(packet_data, packet_offset);
        int countdown_time_ms = Multi_match_info.countdown_time_ms - (int)(Clock_current_time - start_time);
        if ((int)(Multi_match_info.countdown_time_ms + start_time) < (int)Clock_current_time)
        {
            Multi_match_info.round_time_ms += countdown_time_ms - 100;
            if (Multi_match_info.round_time_ms < 0)
            {
                Multi_match_info.round_time_ms = 0;
            }
            countdown_time_ms = 100;
        }
        multi_game_mode_enter_countdown(countdown_time_ms);
        return;
    }
    case Mode_common_scores:
        if (Multiplayer && !Server)
        {
            multi_game_stats_read_all_player_scores(packet_data, packet_offset);
            multi_game_stats_read_all_team_scores(packet_data, packet_offset);
        }
        return;
    case Mode_common_round_begin:
        multi_gameplay_mode_round_begin();
        // A joining client takes the server's remaining time.
        if (join_in_progress)
        {
            multi_game_timer_set((int)read_u32_be(packet_data, packet_offset));
        }
        return;
    case Mode_common_round_over:
    {
        // The two branches (team or player winner) read the same big-endian value.
        multi_game_mode_has_teams();
        multi_round_winner winner;
        winner.player_game_data_handle = read_u32_be(packet_data, packet_offset);
        multi_gameplay_mode_round_over(winner);
        return;
    }
    }
}

// 0x009EB980 (:2872): whether the local player won, lost or tied: the round's winner, or for the match the last man
// standing, the best score (a tie when another player has the same score as the best one and either is us) or the
// team with the most round wins
multi_game_status multi_game_mode_get_mode_status(bool for_current_round)
{
    const player* local_player = player_get_local_player();
    if (local_player == nullptr)
    {
        return MGS_TIE;
    }
    if (for_current_round)
    {
        if (Multi_match_info.round_winner.player_game_data_handle == 0xFFFFFFFFu)
        {
            return MGS_TIE;
        }
        if (current_mode_is_team_mode())
        {
            if (Multi_match_info.round_winner.player_game_data_handle == (unsigned int)local_player->current_team)
            {
                return MGS_WIN;
            }
            return MGS_LOSE;
        }
        const player* winner = multi_game_data_get_player_instance(Multi_match_info.round_winner.player_game_data_handle);
        if (winner == nullptr)
        {
            return MGS_TIE;
        }
        return winner == local_player ? MGS_WIN : MGS_LOSE;
    }
    if (Mp_last_man_standing && multi_game_mode_get_last_man_standing() != nullptr)
    {
        return MGS_WIN;
    }
    if (!current_mode_is_team_mode())
    {
        const player* best = nullptr;
        int best_score = (int)0x80000001;
        for (int i = 0; i < World.get_num_objects<player>(); i++)
        {
            const player* pp = World.get_object<player>(i);
            const multi_player_stats* stats = multi_game_stats_get_player_stats(pp->mp_data);
            if (stats != nullptr && best_score < (int)stats->score)
            {
                best = pp;
                best_score = stats->score;
            }
        }
        for (int i = 0; i < World.get_num_objects<player>(); i++)
        {
            const player* pp = World.get_object<player>(i);
            const multi_player_stats* stats = multi_game_stats_get_player_stats(pp->mp_data);
            if (stats != nullptr && (int)stats->score == best_score && pp != best && (pp == local_player || best == local_player))
            {
                return MGS_TIE;
            }
        }
        return best == local_player ? MGS_WIN : MGS_LOSE;
    }
    human_teams best_team = HUMAN_TEAM_NONE;
    int best_wins = (int)0x80000001;
    for (int team = 0; team < 2; team++)
    {
        if (best_wins < Mp_team_stats[team].round_wins)
        {
            best_team = (human_teams)team;
            best_wins = Mp_team_stats[team].round_wins;
        }
    }
    for (int team = 0; team < 2; team++)
    {
        if (Mp_team_stats[team].round_wins == best_wins && team != best_team)
        {
            return MGS_TIE;
        }
    }
    return local_player->current_team == best_team ? MGS_WIN : MGS_LOSE;
}

// 0x0094D2D0 (:2974)
bool multi_game_mode_is_pregame()
{
    return Multi_match_info.match_state == MGMS_PREGAME;
}

// 0x0094D2E0 (:2979)
bool multi_game_mode_is_countdown()
{
    return Multi_match_info.match_state == MGMS_COUNTDOWN;
}

// 0x0094D2F0 (:2984)
bool multi_game_mode_is_gameplay()
{
    return Multi_match_info.match_state == MGMS_GAMEPLAY;
}

// 0x0094D300 (:2989)
bool multi_game_mode_is_gameplay_or_later()
{
    return Multi_match_info.match_state > MGMS_COUNTDOWN;
}

// 0x00975DA0 (:2999)
bool multi_game_mode_allows_class_selection(multi_gameplay_mode mode)
{
    if ((unsigned int)mode < 8)
    {
        return Multi_game_modes[mode].num_classes > 1;
    }
    return false;
}

// 0x00992800 (:3018): server: every client has loaded (CS_CONNECTED_GAMEPLAY); client: both ends have
bool multi_game_mode_all_players_ready()
{
    multi_session* session = multi_session_get_by_type(ST_GAME);
    if (session == nullptr || session->get_me_connection() == nullptr || session->get_host_connection() == nullptr)
    {
        return false;
    }
    multi_connection* host = session->get_host_connection();
    if (Multiplayer && Server)
    {
        multi_connection* first = session->get_connection_list();
        for (multi_connection* cp = first; cp != nullptr;)
        {
            if (cp != host && cp->connection_status != CS_CONNECTED_GAMEPLAY)
            {
                return false;
            }
            cp = cp->next;
            if (cp == first)
            {
                break;
            }
        }
        return true;
    }
    return session->get_me_connection()->connection_status == CS_CONNECTED_GAMEPLAY && host->connection_status == CS_CONNECTED_GAMEPLAY;
}

// 0x0094D310 (:3066)
bool multi_game_mode_setting_up_round()
{
    return Multi_match_info.setting_up_round;
}

// 0x0094D320 (:3073)
bool multi_game_mode_synching_endgame()
{
    return Multi_match_info.synchronizing_endgame;
}

// 0x0094D330 (:3080)
bool multi_game_mode_is_viewing_results()
{
    return Multi_match_info.match_state == MGMS_VIEWING_RESULTS;
}

// 0x009BA480 (:3087)
bool multi_game_mode_is_endgame()
{
    return Multi_match_info.match_state == MGMS_VIEWING_RESULTS && multi_game_mode_is_last_round();
}

// 0x00975DD0 (:3094)
bool multi_game_mode_has_teams()
{
    return current_mode_is_team_mode();
}

// 0x00992870 (:3099): the players of the team; with connecting_data_handle, only those connected to the game session
// and not the connecting player
int multi_game_mode_get_team_size(session_types session_type, human_teams team, unsigned int connecting_data_handle)
{
    int size = 0;
    for (unsigned int handle = multi_game_data_handles_begin(session_type, false, false); handle != 0xFFFFFFFFu;
         handle = multi_game_data_handles_next(handle, false, false))
    {
        // Original: the game data is not checked for null.
        const multi_game_data* data = multi_game_data_get(handle, false, false);
        if (connecting_data_handle != 0xFFFFFFFFu && Game_session != nullptr && Game_session->get_player_connection(data->player_uid) == nullptr)
        {
            continue;
        }
        if (data->team != team)
        {
            continue;
        }
        if (connecting_data_handle != 0xFFFFFFFFu)
        {
            const multi_game_data* connecting = multi_game_data_get(connecting_data_handle, false, false);
            if (connecting != nullptr && connecting->player_uid.get_internal_id().is_equal(data->player_uid.get_internal_id()))
            {
                continue;
            }
            if (handle == connecting_data_handle)
            {
                continue;
            }
        }
        size++;
    }
    return size;
}

// 0x009929D0 (:3141): the sum of the team's player levels
int multi_game_mode_get_team_xp(session_types session_type, human_teams team, unsigned int connecting_data_handle)
{
    int xp = 0;
    for (unsigned int handle = multi_game_data_handles_begin(session_type, false, false); handle != 0xFFFFFFFFu;
         handle = multi_game_data_handles_next(handle, false, false))
    {
        // Original: the game data is not checked for null.
        const multi_game_data* data = multi_game_data_get(handle, false, false);
        if (data->team == team && (connecting_data_handle == 0xFFFFFFFFu || handle != connecting_data_handle))
        {
            xp += data->player_level;
        }
    }
    return xp;
}

// 0x0094D340 (:3163)
human_teams multi_game_mode_get_opposite_team(human_teams team)
{
    if (team == HUMAN_TEAM_GUERILLA)
    {
        return HUMAN_TEAM_EDF;
    }
    if (team == HUMAN_TEAM_EDF)
    {
        return HUMAN_TEAM_GUERILLA;
    }
    return HUMAN_TEAM_NONE;
}

// 0x00992AD0 (:3179): the smaller team; at equal sizes the team the joining player's level helps most (none if the
// level ratios are within 0.13)
human_teams multi_game_mode_get_jip_disadvantaged_team(session_types session_type, unsigned int connecting_data_handle)
{
    const int edf_size = multi_game_mode_get_team_size(session_type, HUMAN_TEAM_EDF, connecting_data_handle);
    const int guerilla_size = multi_game_mode_get_team_size(session_type, HUMAN_TEAM_GUERILLA, connecting_data_handle);
    const int edf_xp = multi_game_mode_get_team_xp(session_type, HUMAN_TEAM_EDF, connecting_data_handle);
    const int guerilla_xp = multi_game_mode_get_team_xp(session_type, HUMAN_TEAM_GUERILLA, connecting_data_handle);
    if (edf_size != guerilla_size)
    {
        return edf_size <= guerilla_size ? HUMAN_TEAM_EDF : HUMAN_TEAM_GUERILLA;
    }
    const multi_game_data* connecting = multi_game_data_get(connecting_data_handle, false, false);
    if (connecting != nullptr && edf_xp != 0 && guerilla_xp != 0)
    {
        const float edf_ratio = (float)(int)(connecting->player_level + edf_xp) / (float)guerilla_xp;
        const float guerilla_ratio = (float)(int)(connecting->player_level + guerilla_xp) / (float)edf_xp;
        const float diff = edf_ratio - guerilla_ratio;
        if ((diff < 0.0f ? -diff : diff) < 0.13f)
        {
            return HUMAN_TEAM_NONE;
        }
        return edf_ratio < guerilla_ratio ? HUMAN_TEAM_EDF : HUMAN_TEAM_GUERILLA;
    }
    // Equal sizes here: HUMAN_TEAM_NONE.
    if (edf_size > guerilla_size)
    {
        return HUMAN_TEAM_GUERILLA;
    }
    return edf_size < guerilla_size ? HUMAN_TEAM_EDF : HUMAN_TEAM_NONE;
}

// 0x00992C30 (:3240): the smaller team, none at equal sizes
human_teams multi_game_mode_get_disadvantaged_team(session_types session_type, unsigned int connecting_data_handle)
{
    const int edf_size = multi_game_mode_get_team_size(session_type, HUMAN_TEAM_EDF, connecting_data_handle);
    const int guerilla_size = multi_game_mode_get_team_size(session_type, HUMAN_TEAM_GUERILLA, connecting_data_handle);
    if (edf_size > guerilla_size)
    {
        return HUMAN_TEAM_GUERILLA;
    }
    return edf_size < guerilla_size ? HUMAN_TEAM_EDF : HUMAN_TEAM_NONE;
}

// 0x00992C70 (:3254): the larger team, none at equal sizes
human_teams multi_game_mode_get_advantaged_team(session_types session_type)
{
    const int edf_size = multi_game_mode_get_team_size(session_type, HUMAN_TEAM_EDF, 0xFFFFFFFFu);
    const int guerilla_size = multi_game_mode_get_team_size(session_type, HUMAN_TEAM_GUERILLA, 0xFFFFFFFFu);
    if (edf_size > guerilla_size)
    {
        return HUMAN_TEAM_EDF;
    }
    return edf_size < guerilla_size ? HUMAN_TEAM_GUERILLA : HUMAN_TEAM_NONE;
}

// 0x00992CB0 (:3268): 8 players a team
bool multi_game_mode_team_is_full(session_types session_type, human_teams team)
{
    return multi_game_mode_get_team_size(session_type, team, 0xFFFFFFFFu) > 7;
}

// 0x00992CD0 (:3273): the lobby's team for a player: civilian in free-for-all modes, else the preferred team unless it
// is full, else the smaller team (EDF when none is smaller); the game session keeps the lobby's choice
human_teams multi_game_mode_choose_appropriate_team(session_types session_type, human_teams preferred_team, human_teams current_team, multi_gameplay_mode mode, unsigned int connecting_data_handle)
{
    if (session_type == ST_GAME)
    {
        return preferred_team;
    }
    if (mode == MGPM_NONE)
    {
        mode = Mp_server_options.get_mode();
    }
    if (mode > MGPM_DEMOLITION || !Multi_gameplay_mode_funcs[mode].is_team_mode)
    {
        return HUMAN_TEAM_CIVILIAN;
    }
    const bool has_preference = preferred_team != HUMAN_TEAM_CIVILIAN && preferred_team != HUMAN_TEAM_NONE;
    human_teams team = current_team;
    if (current_team == HUMAN_TEAM_CIVILIAN || current_team == HUMAN_TEAM_NONE)
    {
        if (has_preference && !multi_game_mode_team_is_full(session_type, preferred_team))
        {
            team = preferred_team;
        }
        else
        {
            team = multi_game_mode_get_disadvantaged_team(session_type, connecting_data_handle);
        }
    }
    else if (has_preference && preferred_team != current_team)
    {
        team = multi_game_mode_team_is_full(session_type, preferred_team) ? multi_game_mode_get_disadvantaged_team(session_type, connecting_data_handle) : preferred_team;
    }
    if (team != HUMAN_TEAM_NONE && team != HUMAN_TEAM_CIVILIAN)
    {
        return team;
    }
    return HUMAN_TEAM_EDF;
}

// 0x00992D90 (:3321): as multi_game_mode_choose_appropriate_team, balancing with the joining player's level
human_teams multi_game_mode_jip_choose_appropriate_team(session_types session_type, human_teams preferred_team, human_teams current_team, multi_gameplay_mode mode, unsigned int connecting_data_handle)
{
    if (session_type == ST_GAME)
    {
        return preferred_team;
    }
    if (mode == MGPM_NONE)
    {
        mode = Mp_server_options.get_mode();
    }
    if (mode > MGPM_DEMOLITION || !Multi_gameplay_mode_funcs[mode].is_team_mode)
    {
        return HUMAN_TEAM_CIVILIAN;
    }
    const bool has_preference = preferred_team != HUMAN_TEAM_CIVILIAN && preferred_team != HUMAN_TEAM_NONE;
    human_teams team = current_team;
    if (current_team == HUMAN_TEAM_CIVILIAN || current_team == HUMAN_TEAM_NONE)
    {
        if (has_preference && !multi_game_mode_team_is_full(session_type, preferred_team))
        {
            team = preferred_team;
        }
        else
        {
            team = multi_game_mode_get_jip_disadvantaged_team(session_type, connecting_data_handle);
        }
    }
    else if (has_preference && preferred_team != current_team)
    {
        team = multi_game_mode_team_is_full(session_type, preferred_team) ? multi_game_mode_get_jip_disadvantaged_team(session_type, connecting_data_handle) : preferred_team;
    }
    if (team != HUMAN_TEAM_NONE && team != HUMAN_TEAM_CIVILIAN)
    {
        return team;
    }
    return HUMAN_TEAM_EDF;
}

// 0x009AB5D0 (:3370): the round count reached, a team that cannot be caught any more, or a last man standing
bool multi_game_mode_is_last_round()
{
    if (Multi_match_info.current_round == Multi_match_info.num_rounds)
    {
        return true;
    }
    if (current_mode_is_team_mode())
    {
        int leader = -1;
        int leader_wins = 0;
        for (int i = 0; i < 2; i++)
        {
            if (Mp_team_stats[i].round_wins > leader_wins)
            {
                leader = i;
                leader_wins = Mp_team_stats[i].round_wins;
            }
        }
        const int rounds_left = (int)Multi_match_info.num_rounds - (int)Multi_match_info.current_round;
        bool decided = true;
        for (int i = 0; i < 2; i++)
        {
            if (i != leader && Mp_team_stats[i].round_wins + rounds_left >= leader_wins)
            {
                decided = false;
                break;
            }
        }
        if (decided)
        {
            return true;
        }
    }
    return Mp_last_man_standing && multi_game_mode_get_last_man_standing() != nullptr;
}

// 0x00992E50 (:3419): a team's or a player's round score reached the point limit (0: none)
bool multi_game_mode_point_limit_reached()
{
    const unsigned short point_limit = Mp_server_options.generic_options.point_limit;
    if (point_limit == 0)
    {
        return false;
    }
    if (current_mode_is_team_mode())
    {
        return Mp_team_stats[1].round_score >= point_limit || Mp_team_stats[0].round_score >= point_limit;
    }
    for (const multi_game_data* data = multi_game_data_begin(ST_GAME_SNAPSHOT, false, false); data != nullptr;
         data = multi_game_data_next(data, false, false))
    {
        // Original: index 16 is accepted, and a null stats pointer would be read.
        const unsigned int handle = data->handle;
        const multi_player_stats* stats = (handle == 0xFFFFFFFFu || (handle & 0xFFFF) > 16) ? nullptr : &Mp_player_stats[handle & 0xFFFF];
        if (stats->round_score >= point_limit)
        {
            return true;
        }
    }
    return false;
}

// 0x00975E00 (:3445)
int multi_game_mode_num_players()
{
    return multi_game_data_count_players(ST_GAME_SNAPSHOT);
}

// 0x0094D360 (:3452)
const multi_game_mode_info* multi_game_mode_get_info(multi_gameplay_mode mode)
{
    return &Multi_game_modes[mode];
}

// 0x009DB910 (:3458)
player* multi_game_mode_get_round_winner_player()
{
    if (Multi_match_info.round_winner.player_game_data_handle != 0xFFFFFFFFu)
    {
        return multi_game_data_get_player_instance(Multi_match_info.round_winner.player_game_data_handle);
    }
    return nullptr;
}

// 0x0094D370 (:3472)
human_teams multi_game_mode_get_round_winner_team()
{
    return Multi_match_info.round_winner.team;
}

// 0x00992FC0 (:3482): the last player when every player left is on one side (the last player when there is one), else
// nullptr
player* multi_game_mode_get_last_man_standing()
{
    player* last = nullptr;
    const int count = World.get_num_objects<player>();
    for (int i = 0; i < count; i++)
    {
        player* pp = World.get_object<player>(i);
        if (last != nullptr && human_teams_get_disposition(pp, last) == TEAM_DISPOSITION_HOSTILE)
        {
            return nullptr;
        }
        // Original: the first player stays the candidate once there is one.
        if (last == nullptr)
        {
            last = pp;
        }
    }
    return last;
}

// 0x00975E10 (:3512): the player's stats count as reported (marauders, the spectators, are never reported)
void multi_game_mode_report_stats_for_player(const multi_game_data* data)
{
    if (data->team != HUMAN_TEAM_MARAUDER)
    {
        multi_game_data_set_reported_stats(data->handle); // inlined
    }
}

// Port helper: a float score detail goes to the ranking board as its bits.
static unsigned int ranking_detail_from_float(float value)
{
    unsigned int bits;
    memcpy(&bits, &value, sizeof(bits));
    return bits;
}

// 0x00975E50 (:3741): an online match (not system link) in progress sends our ranked totals to the four ranking
// boards (Steam leaderboards; the request fails without a signed-in user): 0 experience (bonus, average per game,
// best match), 1 wins (losses, wins per loss, best win streak), 2 kills (deaths, best match kills, best match deaths),
// 3 damage (repair, average per game, best).
void multi_game_mode_report_stats()
{
    if (Lobby_session->get_state() != MSS_IN_GAME || Multi_syslink)
    {
        return;
    }
    int games_played = stats_get_stat_value(STATS_MP_TOTAL_GAMES_PLAYED_RANKED_GAMES);
    if (games_played < 1)
    {
        games_played = 1;
    }
    const int total_xp = stats_get_stat_value(STATS_MP_TOTAL_XP_RANKED_GAMES);
    const int wins = stats_get_stat_value(STATS_MP_TOTAL_WINS_RANKED_GAMES);
    const int losses = stats_get_stat_value(STATS_MP_TOTAL_LOSSES_RANKED_GAMES);
    const int total_damage = stats_get_stat_value(STATS_MP_TOTAL_DAMAGE_RANKED);
    const int bonus_xp = stats_get_stat_value(STATS_MP_TOTAL_BONUS_XP_RANKED_GAMES);
    const float games = (float)games_played;
    const float average_xp = (float)total_xp / games;
    const int best_match_xp = stats_get_stat_value(STATS_MP_MATCH_HIGH_XP_RANKED_GAMES);
    unsigned int details[3];
    if (Scoreboard_system_initd)
    {
        unit4::LocalGameSession* session = rfg_get_local_game_session();
        details[0] = (unsigned int)bonus_xp;
        details[1] = ranking_detail_from_float(average_xp);
        details[2] = (unsigned int)best_match_xp;
        unit4::systemservices::startSendRankingEntry(session->getSystemServices(), My_player_uid.get_internal_id(), 0, (unsigned int)total_xp, details, 3);
    }
    // The losses, at least 1.
    float loss_count = (float)losses;
    if (1.0f - loss_count >= 0.0f)
    {
        loss_count = 1.0f;
    }
    const float win_ratio = (float)wins / loss_count;
    const int best_win_streak = stats_get_stat_value(STATS_MP_MATCH_WIN_STREAK_HIGHEST);
    if (Scoreboard_system_initd)
    {
        unit4::LocalGameSession* session = rfg_get_local_game_session();
        details[0] = (unsigned int)losses;
        details[1] = ranking_detail_from_float(win_ratio);
        details[2] = (unsigned int)best_win_streak;
        unit4::systemservices::startSendRankingEntry(session->getSystemServices(), My_player_uid.get_internal_id(), 1, (unsigned int)wins, details, 3);
    }
    const int kills = stats_get_stat_value(STATS_MP_TOTAL_KILLS_RANKED_OR_SOCIAL);
    const int deaths = stats_get_stat_value(STATS_MP_TOTAL_DEATHS_RANKED_GAMES);
    const int best_match_kills = stats_get_stat_value(STATS_MP_MATCH_HIGH_KILLS_RANKED_GAMES);
    const int best_match_deaths = stats_get_stat_value(STATS_MP_MATCH_HIGH_DEATHS_RANKED_GAMES);
    if (Scoreboard_system_initd)
    {
        unit4::LocalGameSession* session = rfg_get_local_game_session();
        details[0] = (unsigned int)deaths;
        details[1] = (unsigned int)best_match_kills; // an int here (the other boards' middle detail is a float)
        details[2] = (unsigned int)best_match_deaths;
        unit4::systemservices::startSendRankingEntry(session->getSystemServices(), My_player_uid.get_internal_id(), 2, (unsigned int)kills, details, 3);
    }
    const int damage = stats_get_stat_value(STATS_MP_TOTAL_DAMAGE_RANKED);
    const int repair = stats_get_stat_value(STATS_MP_TOTAL_REPAIR_RANKED);
    const float average_damage = (float)total_damage / games;
    const int best_damage = stats_get_stat_value(STATS_MP_HIGHEST_DAMAGE_RANKED);
    if (Scoreboard_system_initd)
    {
        unit4::LocalGameSession* session = rfg_get_local_game_session();
        details[0] = (unsigned int)repair;
        details[1] = ranking_detail_from_float(average_damage);
        details[2] = (unsigned int)best_damage;
        unit4::systemservices::startSendRankingEntry(session->getSystemServices(), My_player_uid.get_internal_id(), 3, (unsigned int)damage, details, 3);
    }
}

// 0x00993040 (:3876)
// orig_main.md OM7 (the in-game options' Quit Game): leaving a match in progress counts as a lost game in the
// statistics. Not for spectators, system link matches or outside the round's gameplay.
void multi_game_mode_common_report_loss_for_leaving_early()
{
    if (Game_session == nullptr || Spectating || Multi_syslink || Multi_match_info.match_state != MGMS_GAMEPLAY)
    {
        return;
    }
    unit4::LocalGameSession* session = rfg_get_local_game_session();
    stats_increment_by_one(STATS_MP_TOTAL_GAMES_PLAYED_OVERALL);
    // Magic number: hero stat 4 (games played).
    if (session != nullptr)
    {
        session->updateHeroStatAdd(nullptr, 4, 1);
    }
    stats_increment_by_one(STATS_MP_TOTAL_LOSSES_OVERALL);
    multi_game_stats_record_played_match();
    const multi_session_info_base* info = Game_session->get_info();
    if (!info->private_match)
    {
        if (Current_game_type_context == PGTC_PUBLIC)
        {
            stats_increment_by_one(STATS_MP_TOTAL_GAMES_PLAYED_SOCIAL_GAMES);
            stats_increment_by_one(STATS_MP_TOTAL_GAMES_PLAYED_RANKED_OR_SOCIAL);
            stats_increment_by_one(STATS_MP_TOTAL_LOSSES_SOCIAL_GAMES);
        }
        else
        {
            stats_increment_by_one(STATS_MP_TOTAL_GAMES_PLAYED_RANKED_GAMES);
            stats_increment_by_one(STATS_MP_TOTAL_GAMES_PLAYED_RANKED_OR_SOCIAL);
            stats_increment_by_one(STATS_MP_TOTAL_LOSSES_RANKED_GAMES);
            stats_reset(STATS_MP_MATCH_WIN_STREAK);
            if (My_player_stats != nullptr)
            {
                // The damage and repair totals as whole numbers.
                const int damage = (int)My_player_stats->total_damage;
                const int repair = (int)My_player_stats->total_repair;
                stats_increment(STATS_MP_TOTAL_DAMAGE_RANKED, &damage);
                stats_increment(STATS_MP_TOTAL_REPAIR_RANKED, &repair);
                stats_update_best(STATS_MP_HIGHEST_DAMAGE_RANKED, (int)My_player_stats->highest_damage);
            }
        }
    }
    else
    {
        stats_increment_by_one(STATS_MP_TOTAL_GAMES_PLAYED_PRIVATE_GAMES);
        stats_increment_by_one(STATS_MP_TOTAL_LOSSES_PRIVATE_GAMES);
    }
    if (Multiplayer && (unsigned int)Multi_gameplay_mode < 8)
    {
        // The switch over the modes in MGPM_* order: games played, then the loss.
        static const tracked_stats_indices mode_stats[8][2] = {
            {STATS_MP_TOTAL_GAMES_PLAYED_DEATHMATCH, STATS_MP_TOTAL_LOSSES_DEATHMATCH},
            {STATS_MP_TOTAL_GAMES_PLAYED_TEAM_DEATHMATCH, STATS_MP_TOTAL_LOSSES_TEAM_DEATHMATCH},
            {STATS_MP_TOTAL_GAMES_PLAYED_CAPTURE_THE_FLAG, STATS_MP_TOTAL_LOSSES_CAPTURE_THE_FLAG},
            {STATS_MP_TOTAL_GAMES_PLAYED_SIEGE, STATS_MP_TOTAL_LOSSES_SIEGE},
            {STATS_MP_TOTAL_GAMES_PLAYED_KING_OF_THE_HILL, STATS_MP_TOTAL_LOSSES_KING_OF_THE_HILL},
            {STATS_MP_TOTAL_GAMES_PLAYED_BAGMAN, STATS_MP_TOTAL_LOSSES_BAGMAN},
            {STATS_MP_TOTAL_GAMES_PLAYED_TEAM_BAGMAN, STATS_MP_TOTAL_LOSSES_TEAM_BAGMAN},
            {STATS_MP_TOTAL_GAMES_PLAYED_DEMOLITION, STATS_MP_TOTAL_LOSSES_DEMOLITION},
        };
        stats_increment_by_one(mode_stats[Multi_gameplay_mode][0]);
        stats_increment_by_one(mode_stats[Multi_gameplay_mode][1]);
    }
    // Playing with someone who completed the single player game (outside system link).
    const multi_game_data* my_game_data = multi_game_data_get(Local_game_data_handles[ST_GAME], false, false);
    if (!info->systemlink_or_lan && my_game_data != nullptr && my_game_data->completed_single_player)
    {
        for (const multi_game_data* data = multi_game_data_begin(ST_GAME, false, false); data != nullptr; data = multi_game_data_next(data, false, false))
        {
            if (data != my_game_data && data->completed_single_player)
            {
                stats_set_stat(STATS_MP_RED_FACTION_MEMBER, 1);
            }
        }
    }
    multi_game_mode_report_stats();
}

// 0x0094D380 (:3803)
void multi_game_mode_append_mode_specific_stats(unsigned char* packet_data, unsigned short& packet_size)
{
}

// 0x0094D390 (:3808)
void multi_game_mode_read_mode_specific_stats(unsigned char* packet_data, int& packet_offset)
{
}

// 0x0094D3A0 (:3813)
void multi_game_mode_get_mode_specific_stat_names(char** names, const int num_values, const int max_name_len)
{
}

// 0x0094D3B0 (:3818)
void multi_game_mode_get_mode_specific_stat_values(char** values, const int num_values, const int max_value_len)
{
}

// 0x0094D3C0 (:3824)
void multi_game_mode_common_calculate_skill_mismatch()
{
}

// 0x0094D3D0 (:3858)
void multi_game_mode_common_set_skill_mismatch(bool mismatch)
{
    Skill_mismatch = mismatch;
}

// 0x0094D3E0 (:3863)
bool multi_game_mode_common_get_skill_mismatch()
{
    return Skill_mismatch;
}

// 0x0094D3F0 (:3868)
void multi_game_mode_hidden_challenge_complete(const hidden_challenge* challengep)
{
    if (Ui_match_events[2].event == nullptr)
    {
        return;
    }
    Ui_match_events[2].event->set_element(0, vint_variant("MENU_HIDDEN_CHALLENGE_COMPLETE"));
    Ui_match_events[2].event->set_element(1, vint_variant(challengep->display_name));
}
