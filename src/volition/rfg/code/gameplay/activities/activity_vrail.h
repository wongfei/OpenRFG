// Original: volition/rfg/code/gameplay/activities/activity_vrail.h
// Implementation: volition/rfg/code/gameplay/activities/activity_vrail.cpp
//
// Riding shotgun (the vehicle rail activity): Jorge's story sections and chapters, the start / replay / exit flow, the
// score, penalty and vehicle health strings for the interface. Movers ask activity_vrail_is_riding_shotgun_running.
#pragma once


class object_riding_shotgun_node;
struct riding_shotgun_info;
struct riding_shotgun_story_chapter_data;

void unused_dcf_riding_shotgun_start(); // 0x007A9780 (:177)
void unused_dcf_riding_shotgun_play_story_section(); // 0x00799DA0 (:199)
const riding_shotgun_info* get_riding_shotgun_info_by_name(const char* name); // 0x0075A450 (:262)
riding_shotgun_story_chapter_data* riding_shotgun_get_current_chapter(); // 0x0075A4A0 (:612)
void activity_riding_shotgun_exit(); // 0x007AF010 (:2260)
void activity_riding_shotgun_replay(); // 0x0075AAE0 (:2274)
void activity_riding_shotgun_process_player_death(); // 0x007B3500 (:2283)
bool activity_riding_shotgun_start(const object_riding_shotgun_node* node); // 0x007A2710 (:2290)
bool activity_vrail_is_riding_shotgun_running(); // 0x0075AB00 (:2299)
bool activity_riding_shotgun_can_be_started(); // 0x007A2750 (:2309)
bool activity_riding_shotgun_play_start_voiceline(object_riding_shotgun_node* rs_node); // 0x007A27A0 (:2343)
void activity_riding_shotgun_liberation(); // 0x0079A530 (:2374)
void activity_riding_shotgun_play_next_story_section(); // 0x00786910 (:2386)
void activity_riding_shotgun_get_display_score_string(char* buf, int size); // 0x0075AB40 (:2392)
void activity_riding_shotgun_get_target_score_string(char* buf, int size); // 0x0075AB90 (:2399)
void acitivty_riding_shotgun_get_penalty_string(char* buf, int size); // 0x0075ABE0 (:2407)
float activity_riding_shotgun_get_vehicle_health_percentage(); // 0x00786930 (:2415)
void activity_riding_shotgun_get_score_results(float& target_goal, float& soldier_score, float& building_score, float& vehicle_score, float& base_score, float& penalty, float& total); // 0x0075AC10 (:2429)
