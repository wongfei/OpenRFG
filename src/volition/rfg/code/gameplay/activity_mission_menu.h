// Original: volition/rfg/code/gameplay/activity_mission_menu.h
// Implementation: volition/rfg/code/gameplay/activity_mission_menu.cpp
//
// The activity and mission start menus: activity nodes, and launching a mission or activity from the menu.
#pragma once


class object;

void unused_dcf_mission_start(); // 0x007AF700 (:240)
void unused_dcf_activity_start(); // 0x007AF800 (:275)
object* activity_node_get(const char* name); // 0x0078AF60 (:391)
void activity_mission_menu_init_post(); // 0x007A9EA0 (:472)
void activity_mission_menu_do_frame(); // 0x0075C140 (:481)
void mission_menu_launch(); // 0x007A9F60 (:506)
void activity_menu_launch(); // 0x007AA250 (:592)
