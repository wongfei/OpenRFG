// Original: volition/rfg/code/multi/multi_game/multi_game_render.h
// Implementation: volition/rfg/code/multi/multi_game/multi_game_render.cpp
//
// Multiplayer rendering: player names over heads and the team icons and colors.
#pragma once

#include "volition/rfg/code/objects/humans/human_teams.h"

class color;
class object;

enum multi_team_color_types
{
    MTCT_ICON = 0,
    MTCT_OUTLINE = 1,
    MTCT_RIM_LIGHTING = 2,
    NUM_MULTI_TEAM_COLOR_TYPES = 3,
};

void multi_game_render_system_init(); // 0x009610F0 (:113)
void multi_game_render_system_shutdown(); // 0x00961100 (:117)
void multi_game_render_match_init(); // 0x00961110 (:121)
void multi_game_process_player_names(); // 0x009A04F0 (:343)
void multi_game_render_player_names(); // 0x00961120 (:410)
int multi_game_render_get_team_icon(human_teams team); // 0x00961130 (:416)
void multi_game_render_get_team_color(color& output_color, multi_team_color_types type, const object* obj); // 0x00984050 (:452)
void multi_game_render_get_team_color(color& output_color, multi_team_color_types type, human_teams team); // 0x009840C0 (:472)
void multi_game_render_get_friendly_team_color(color& output); // 0x00984170 (:493)
void multi_game_render_get_enemy_team_color(color& output); // 0x00984180 (:498)
