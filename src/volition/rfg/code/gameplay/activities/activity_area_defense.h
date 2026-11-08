// Original: volition/rfg/code/gameplay/activities/activity_area_defense.h
// Implementation: volition/rfg/code/gameplay/activities/activity_area_defense.cpp
//
// Area defense ("defend the area" activity): loading the area defense table (and its DLC entries), starting, replaying
// and exiting the activity, the start voice line and the kill totals shown by the interface.
#pragma once


struct activity_area_defense_info;
class object;
class object_area_defense_node;

void unused_dcf_area_defense_start(); // 0x007A89B0 (:151)
void load_area_defense_table(bool reload, int dlc_id); // 0x00796240 (:178)
void area_defense_unload_dlc(); // 0x00756C40 (:326)
void activity_area_defense_exit(); // 0x007570A0 (:1591)
void activity_area_defense_replay(); // 0x007570E0 (:1604)
void activity_area_defense_process_player_death(); // 0x00757100 (:1613)
bool activity_area_defense_start(const object_area_defense_node* node); // 0x007A0AC0 (:1620)
bool activity_area_defense_can_be_started(); // 0x007A0B10 (:1627)
bool activity_area_defense_play_start_voiceline(object_area_defense_node* ad_node); // 0x007A0B70 (:1677)
bool activity_area_defense_is_running(); // 0x00757180 (:1696)
void activity_area_defense_get_kill_totals(int& num_killed, int& kill_goal, bool& target_vehicles); // 0x007571B0 (:1701)
object_area_defense_node* area_defense_find_node_generic(object* node); // 0x0077FB90 (:1730)
const activity_area_defense_info* get_area_defense_info_by_name(const char* name); // 0x00757230 (:1752)
