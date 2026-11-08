// Original: volition/rfg/code/gameplay/gameflow/alert_level_manager.h
// Implementation: volition/rfg/code/gameplay/gameflow/alert_level_manager.cpp
//
// The alert level manager (gsm_* / alm_*): the EDF alert level and value, its cap and ratcheting, raised by reported
// damage and deaths near the player and decaying over time; gameplay_do_frame calls gsm_do_frame.
#pragma once

#include "volition/rfg/code/gameplay/gameflow/gameflow_enums.h"
#include "volition/rfg/code/objects/humans/human_teams.h"

class gfm_damage_report;
class human;
class vector;
class vehicle;

bool is_important(const human* hp); // 0x0075D2B0 (:386)
bool is_edf(const human* hp); // 0x0075D2C0 (:392)
bool should_report_human(const human* hp); // 0x0075D2E0 (:398)
bool is_occupied(const vehicle* vp); // 0x0075D320 (:404)
bool currently_edf(const vehicle* vp); // 0x0075D340 (:409)
bool is_edf(const vehicle* vp); // 0x0075D360 (:414)
bool should_report_vehicle_damage(const vehicle* vp); // 0x0075D390 (:420)
bool should_report_vehicle_death(const vehicle* vp); // 0x0075D400 (:427)
bool is_near_a_player(unsigned int handle); // 0x0075D490 (:443)
void gsm_level_init_pre(); // 0x00773470 (:968)
void gsm_game_init(); // 0x00773550 (:999)
gfm_alert_levels gsm_get_alert_level(); // 0x0075D610 (:1011)
float gsm_get_alert_value(); // 0x0075D630 (:1026)
void gsm_set_alert_level(gfm_alert_levels new_alert_level); // 0x007AA740 (:1032)
void gsm_release_alert_level_cap(); // 0x0075D640 (:1046)
void gsm_set_alert_level_cap(gfm_alert_levels min, gfm_alert_levels max); // 0x007AA770 (:1056)
void gsm_get_alert_level_cap(gfm_alert_levels& min, gfm_alert_levels& max); // 0x0075D680 (:1078)
void gsm_set_alert_value(float new_alert_value); // 0x007A44B0 (:1091)
float gsm_find_alert_value_percentage(); // 0x0075D6C0 (:1102)
gfm_alert_levels gsm_find_alert_fill_value(); // 0x0075D810 (:1156)
int gsm_time_since_last_alert_change(human_teams team); // 0x0075D850 (:1174)
void alm_report_damage(gfm_damage_report& report); // 0x0078B6F0 (:1184)
void alm_report_death(gfm_damage_report& report); // 0x0078B880 (:1251)
void gsm_do_frame(); // 0x007AA800 (:1309)
void alm_report_human_death(human* dead_hp); // 0x0079C650 (:1438)
void alm_report_vehicle_damage(vehicle* vp, const human* attacker_hp); // 0x0079C6B0 (:1469)
void alm_report_vehicle_death(vehicle* vp, const human* attacker_hp); // 0x0079C710 (:1490)
gfm_alert_levels alm_resolve_alert_level(const char* alert_string); // 0x0075D870 (:1514)
void alm_set_alert_level_ratcheting(bool on); // 0x0075D8B0 (:1530)
void set_last_hostile_action_pos(vector pos); // 0x0075D8C0 (:1535)
void gsm_raise_the_alert_level_in_one_second_if_not_dead(unsigned int alerted_handle); // 0x0075D8F0 (:1545)
bool alm_grace_is_active(const human* hp, const human* target); // 0x0075D950 (:1565)
