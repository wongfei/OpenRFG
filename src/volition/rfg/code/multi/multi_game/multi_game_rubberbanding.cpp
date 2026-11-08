// Original: volition/rfg/code/multi/multi_game/multi_game_rubberbanding.cpp
//
// Rubberbanding (the handicap of the match options): mp_rubberbanding.xtbl gives per level (none, low, medium, high)
// a curve of stat multipliers (health, regeneration, ammo pickup, speed, aim, respawn, backpack recharge, reload) over
// the player's score position (0 = leading, 1 = last). The level and options set pools of multi_game_settings.cpp
// are placed here (their first user in the unity TU). multi_game_rubberbanding_process applies the health multiplier
// to the players each frame.
// Part of the multi unity TU (rfg_multi_unity.obj, every multi/ .cpp in path order): the file statics are shared
// by the whole tree, and a shared one is placed in its first user (tools/hdrgen/specs/mp0_stubs.py).

#include "volition/rfg/code/multi/multi_game/multi_game_rubberbanding.h"
#include "volition/rfg/code/multi/multi_game/modes/multi_game_mode_common.h"
#include "volition/rfg/code/multi/multi_game/multi_game_data.h"
#include "volition/rfg/code/multi/multi_game/multi_game_stats.h"
#include "volition/rfg/code/multi/multi_core/include/multi.h"
#include "volition/rfg/code/multi/multi_core/include/multi_session.h"
#include "volition/rfg/code/level/world.h"
#include "volition/rfg/code/objects/humans/player.h"
#include "volition/vlib/code/cfile/cfile.h"
#include "volition/vlib/code/math/floating.h"
#include "volition/vlib/code/parse/xml_table_tools.h"
#include "volition/rfg/code/gamewide/tweak_table.h"
#include "volition/vlib/code/util/fastxml.h"

#include <string.h>

// ---- file-static globals
// multi_gameplay_mode Multi_gameplay_mode (0x0167DEE0) is used here too; placed in multi_gameplay_mode.cpp.
// human_voice_persona_handle Multi_general_announcer (0x0167DEE4) is used here too; placed in multi_game_mode_bagman.cpp.
static checksum_stri_pair Rubberbanding_stat_names[NUM_MULTI_RUBBERBANDING_STATS] = { // 0x016932D0
    checksum_stri_pair("value_health"),
    checksum_stri_pair("value_regeneration"),
    checksum_stri_pair("value_ammo_pickup"),
    checksum_stri_pair("value_move_speed"),
    checksum_stri_pair("value_move_acceleration"),
    checksum_stri_pair("value_auto_aim"),
    checksum_stri_pair("value_aim_assist"),
    checksum_stri_pair("value_respawn"),
    checksum_stri_pair("value_backpack_recharge"),
    checksum_stri_pair("value_reload_speed"),
};
static multi_rubberbanding_level_info Rubberbanding_levels[NUM_MULTI_RUBBERBANDING_LEVELS] = { // 0x01693320
    multi_rubberbanding_level_info("None", "MENU_HANDICAP_NONE"),
    multi_rubberbanding_level_info("Low", "MENU_HANDICAP_LOW"),
    multi_rubberbanding_level_info("Medium", "MENU_HANDICAP_MEDIUM"),
    multi_rubberbanding_level_info("High", "MENU_HANDICAP_HIGH"),
};
static dynamic_object_pool<multi_level_info,1> Level_info_pool; // 0x01693990
static dynamic_object_pool<multi_game_options_full_set,1> Options_sets_pool; // 0x016939D0
// mp_game_data_wrapper* Mp_game_data (0x02FEB698) is used here too; placed in multi_connection.cpp.
static int Rubberbanding_minimum_score; // 0x02FEB710
// multi_player_stats* Mp_player_stats (0x02FEB720) is used here too; placed in multi_game_mode_bagman.cpp.
static tweak_table_item Tweak_table_item_Rubberbanding_minimum_score("Rubberbanding_minimum_score", &Rubberbanding_minimum_score); // 0x02FEBB6C

// 0x00961260 (:33)
multi_rubberbanding_stat::multi_rubberbanding_stat()
{
    valid = false;
}

// 0x00961270 (:40)
multi_rubberbanding_level_info::multi_rubberbanding_level_info(const char* internal_name, const char* disp_name)
    : internal_name_pair(internal_name)
{
    display_name = disp_name;
    valid = false;
}

// ---- internal functions

// 0x00984190 (:91): each level's curve (a valid one starts at 0, ends at 1 and has at least two points); a level read
// once keeps its curve. The "_Editor" element is skipped.
static void parse_rubberbanding_table()
{
    for (int i = 0; i < NUM_MULTI_RUBBERBANDING_LEVELS; i++)
    {
        for (int stat = 0; stat < NUM_MULTI_RUBBERBANDING_STATS; stat++)
        {
            Rubberbanding_levels[i].stats[stat].valid = false;
        }
        Rubberbanding_levels[i].valid = false;
    }
    xml_element* root = multi_game_settings_retrieve_settings_file("mp_rubberbanding.xtbl");
    xml_element* table = xml_table_find_opt(root, "this_is_it");
    if (table != nullptr)
    {
        for (xml_element* entry = xml_table_find_req(table, "Name")->next; entry != nullptr; entry = entry->next)
        {
            if (strcmp(entry->name, "_Editor") == 0)
            {
                continue;
            }
            const checksum_stri crc(entry->name, 0);
            multi_rubberbanding_level_info* level = nullptr;
            for (int i = 0; i < NUM_MULTI_RUBBERBANDING_LEVELS; i++)
            {
                if (Rubberbanding_levels[i].internal_name_pair.str_checksum == crc)
                {
                    level = &Rubberbanding_levels[i];
                }
            }
            if (level == nullptr || level->valid)
            {
                continue;
            }
            level->num_levels = 0;
            bool stat_found[NUM_MULTI_RUBBERBANDING_STATS] = {};
            for (xml_element* point = entry->elements; point != nullptr; point = point->next)
            {
                xml_element* position = xml_table_find_req(point, "level");
                level->levels[level->num_levels] = xml_get_float(position, 0.0f);
                level->num_levels++;
                for (xml_element* value = position->next; value != nullptr; value = value->next)
                {
                    const checksum_stri stat_crc(value->name, 0);
                    for (int stat = 0; stat < NUM_MULTI_RUBBERBANDING_STATS; stat++)
                    {
                        if (Rubberbanding_stat_names[stat].str_checksum == stat_crc)
                        {
                            if (!level->stats[stat].valid)
                            {
                                level->stats[stat].values[level->num_levels - 1] = xml_get_float(value, 0.0f);
                                stat_found[stat] = true;
                            }
                            break;
                        }
                    }
                }
            }
            for (int stat = 0; stat < NUM_MULTI_RUBBERBANDING_STATS; stat++)
            {
                if (stat_found[stat])
                {
                    level->stats[stat].valid = true;
                }
            }
            if (level->num_levels >= 2 && level->levels[0] == 0.0f && level->levels[level->num_levels - 1] == 1.0f)
            {
                level->valid = true;
            }
        }
    }
    xml_table_close(root);
}

// ---- external functions

// 0x009844C0 (:231): console command (unregistered in retail)
void unused_dcf_mp_reload_rubberbanding_table()
{
    cf_refresh_file_size("mp_rubberbanding.xtbl");
    parse_rubberbanding_table();
}

// 0x009844E0 (:243)
void multi_game_rubberbanding_init()
{
    parse_rubberbanding_table();
}

// 0x009B2200 (:248): every living player's maximum health is its initial one times the health multiplier (the health
// restore target follows it); the health is cut down to it.
void multi_game_rubberbanding_process()
{
    const int count = World.get_num_objects<player>();
    for (int i = 0; i < count; i++)
    {
        player* pp = World.get_object<player>(i);
        if (pp->mp_data == 0xFFFFFFFF || pp->is_killed())
        {
            continue;
        }
        const float multiplier = multi_game_rubberbanding_get_stat_value(pp->mp_data, MRS_HEALTH);
        pp->max_hit_points = (int)((double)(float)pp->initial_max_hit_points * (double)multiplier);
        const float max_hit_points = (float)pp->max_hit_points;
        pp->health_restore_hit_points = max_hit_points;
        if ((float)((double)pp->hit_points - (double)max_hit_points) >= 0.0f)
        {
            pp->hit_points = max_hit_points;
        }
    }
}

// 0x009612D0 (:264)
void multi_game_rubberbanding_render()
{
}

// 0x009612E0 (:286)
const char* multi_game_rubberbanding_get_level_internal_name(multi_rubberbanding_levels level)
{
    if ((unsigned int)level < NUM_MULTI_RUBBERBANDING_LEVELS)
    {
        return Rubberbanding_levels[level].internal_name_pair.str;
    }
    return "Invalid rubberbanding level";
}

// 0x00961300 (:292)
const char* multi_game_rubberbanding_get_level_display_name(multi_rubberbanding_levels level)
{
    if ((unsigned int)level < NUM_MULTI_RUBBERBANDING_LEVELS)
    {
        return Rubberbanding_levels[level].display_name;
    }
    return "Invalid rubberbanding level";
}

// 0x00961320 (:298)
multi_rubberbanding_levels multi_game_rubberbanding_get_level(const char* level_name)
{
    if (level_name != nullptr)
    {
        const checksum_stri crc(level_name, 0);
        for (int i = 0; i < NUM_MULTI_RUBBERBANDING_LEVELS; i++)
        {
            if (Rubberbanding_levels[i].internal_name_pair.str_checksum == crc)
            {
                return (multi_rubberbanding_levels)i;
            }
        }
    }
    return MRL_NONE;
}

// 0x00961360 (:312)
const char* multi_game_rubberbanding_get_stat_internal_name(multi_rubberbanding_stats stat)
{
    if ((unsigned int)stat < NUM_MULTI_RUBBERBANDING_STATS)
    {
        return Rubberbanding_stat_names[stat].str;
    }
    return "Invalid rubberbanding stat";
}

// 0x009A1630 (:320): 0 for the leader, towards 1 the further behind: the player's round experience against the best
// player's (with a minimum score); in a team mode also the team's score and experience against the leading team's
float multi_game_rubberbanding_get_player_score(unsigned int multi_game_data_handle)
{
    if (!Multiplayer)
    {
        return 0.0f;
    }
    float player_ratio = 1.0f;
    float team_ratio = 1.0f;
    const multi_game_data* data = multi_game_data_get(multi_game_data_handle, false, false);
    if (data == nullptr)
    {
        return 0.0f;
    }
    const multi_player_stats* stats = multi_game_stats_get_player_stats(multi_game_data_handle);
    if (stats == nullptr)
    {
        return 0.0f;
    }
    const unsigned int best = multi_game_stats_get_highest_round_xp_player();
    if (best != 0xFFFFFFFF)
    {
        const multi_player_stats* best_stats = multi_game_stats_get_player_stats(best);
        if (best_stats != nullptr)
        {
            int padding = Rubberbanding_minimum_score - (int)best_stats->round_xp;
            if (padding < 1)
            {
                padding = 0;
            }
            const int best_score = (int)best_stats->round_xp + padding;
            if (best_score != 0)
            {
                player_ratio = (float)(padding + (int)stats->round_xp) / (float)best_score;
            }
        }
    }
    if (!multi_gameplay_mode_is_team_mode(multi_gameplay_mode_get()))
    {
        return 1.0f - player_ratio;
    }
    const human_teams team = data->team;
    if (team == HUMAN_TEAM_NONE || team == HUMAN_TEAM_CIVILIAN)
    {
        return 0.0f;
    }
    const multi_team_stats* team_stats = multi_game_stats_get_team_stats(team);
    if (team_stats != nullptr)
    {
        const human_teams leader = multi_game_stats_get_round_winning_team();
        if (leader != HUMAN_TEAM_NONE)
        {
            const multi_team_stats* leader_stats = multi_game_stats_get_team_stats(leader);
            if (leader_stats != nullptr && leader_stats->round_score != 0)
            {
                team_ratio = (float)team_stats->round_score / (float)leader_stats->round_score;
            }
        }
    }
    // the round experience of the connected players of each of the two teams
    int team_xp[2] = {};
    for (const multi_game_data* other = multi_game_data_begin(ST_GAME_SNAPSHOT, false, false); other != nullptr;
         other = multi_game_data_next(other, false, false))
    {
        if (Game_session->get_player_connection(other->player_uid) == nullptr || other->team == HUMAN_TEAM_MARAUDER ||
            other->team == HUMAN_TEAM_NONE || (int)other->team >= 2)
        {
            continue;
        }
        const multi_player_stats* other_stats = multi_game_stats_get_player_stats(other->handle);
        if (other_stats != nullptr)
        {
            team_xp[other->team] += other_stats->round_xp;
        }
    }
    int best_team_xp = team_xp[0] > -0x7FFFFFFF ? team_xp[0] : -0x7FFFFFFF;
    if (best_team_xp < team_xp[1])
    {
        best_team_xp = team_xp[1];
    }
    int padding = Rubberbanding_minimum_score - best_team_xp;
    if (padding < 1)
    {
        padding = 0;
    }
    best_team_xp += padding;
    float xp_ratio = 1.0f;
    if (best_team_xp != 0)
    {
        xp_ratio = (float)(padding + team_xp[team]) / (float)best_team_xp;
    }
    // max<float>/min<float> of min_max.h
    const float team_squared = team_ratio * team_ratio;
    xp_ratio = xp_ratio - team_squared < 0.0f ? team_squared : xp_ratio;
    const float best_ratio = player_ratio - team_ratio < 0.0f ? team_ratio : player_ratio;
    return 1.0f - (best_ratio - xp_ratio < 0.0f ? best_ratio : xp_ratio);
}

// 0x009A1910 (:460): the stat's multiplier on the level's curve at the player's score (1 outside gameplay)
float multi_game_rubberbanding_get_stat_value(unsigned int multi_game_data_handle, multi_rubberbanding_stats stat)
{
    if (!Multiplayer || Multi_match_info.match_state != MGMS_GAMEPLAY)
    {
        return 1.0f;
    }
    multi_rubberbanding_levels level_index = Mp_server_options.generic_options.rubberbanding_level;
    if (level_index == MRL_NONE)
    {
        level_index = MRL_DEFAULT;
    }
    if ((int)stat > MRS_RELOAD_SPEED)
    {
        return 1.0f;
    }
    const multi_rubberbanding_level_info& level = Rubberbanding_levels[level_index];
    if (!level.valid || !level.stats[stat].valid || level.num_levels < 2)
    {
        return 1.0f;
    }
    const float score = multi_game_rubberbanding_get_player_score(multi_game_data_handle);
    int segment = level.num_levels - 1;
    for (int i = 1; i < level.num_levels; i++)
    {
        if (score < level.levels[i])
        {
            segment = i;
            break;
        }
    }
    const float y0 = level.stats[stat].values[segment - 1];
    const float y1 = level.stats[stat].values[segment];
    const float t = fl_parameter(score, level.levels[segment - 1], level.levels[segment], false);
    return t * (y1 - y0) + y0;
}
