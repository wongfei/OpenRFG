// Original: volition/rfg/code/gameplay/activities/gfm_reinforcement_coordinator.h
// Implementation: volition/rfg/code/gameplay/activities/gfm_reinforcement_coordinator.cpp
//
// The reinforcement coordinator (rfc): finding spawn and destination locations for reinforcement squads (roads, nodes,
// nav cells, cached per player position), disabling reinforcements, and the minimap icons.
#pragma once

#include "volition/rfg/code/objects/humans/human_teams.h"

class matrix;
class spawn_location_cache;
struct team_spawn_location;
class vector;

void rfc_dlc_shutdown(); // 0x00771540 (:2395)
void rfc_system_init(int dlc_id); // 0x007A2DC0 (:2402)
void rfc_level_init_pre(); // 0x007A9AB0 (:2447)
void rfc_do_frame(); // 0x007B9640 (:2473)
bool can_use_cached_location(team_spawn_location& spawn_info, const spawn_location_cache& cached_location); // 0x00771550 (:2832)
bool reinforcement_find_spawn_location(const vector& desired_pos, const matrix& desired_orient, human_teams team, bool vehicle, team_spawn_location& spawn_info); // 0x007886A0 (:2903)
bool reinforcement_find_destination_location(const vector& squad_pos, const vector& desired_dest_pos, bool vehicle, vector& destination_pos); // 0x00788880 (:2969)
void reinforcement_set_disable(const bool disabled); // 0x0075B570 (:3038)
void rfc_render_minimap(); // 0x007A2EF0 (:3044)
const vector& reinforcement_get_player_pos(); // 0x00788A60 (:3051)
bool reinforcement_should_render_icon(); // 0x0075B580 (:3057)
void reinforcement_guerrilla_disabled(bool disabled); // 0x0075B590 (:3063)
