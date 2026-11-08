// Original: volition/rfg/code/hud/objectives.h
// Implementation: volition/rfg/code/hud/objectives.cpp
//
// Objectives (single player).
#pragma once

#include "volition/rfg/code/hud/minimap.h"
#include "volition/vlib/code/util/varray.h"

#include <string.h>

class vector;

// size 0x194
struct objective
{
    bool operator==(const objective& test_objective) const // 0x0083F600 (.h:41)
    {
        return strcmp(name, test_objective.name) == 0;
    }
    // Defaults: the "" and false the array constructor of `objectives` passes (0x00879FA0).
    objective(const char* in_name = "", const char* in_description = "", const char* in_display = "", bool in_mission = false) // 0x00866A70 (.h:43)
    {
        // Magic numbers: the sizes of the three strings below.
        strncpy(name, in_name, 31);
        strncpy(description, in_description, 255);
        strncpy(display_name, in_display, 31);
        name[31] = '\0';
        description[255] = '\0';
        display_name[31] = '\0';
        mission = in_mission;
        reset();
    }
    void reset() // 0x00850A00 (.h:57)
    {
        active = false;
        complete = false;
        icons.clear();
    }
    ~objective() = default; // 0x00864FB0
    objective& operator=(const objective& __that) = default; // 0x008805A0
    objective(const objective& __that) = default;

    bool mission; // 0x00
    bool active; // 0x01
    bool complete; // 0x02
    char name[32]; // 0x03
    char description[256]; // 0x23
    char display_name[32]; // 0x123
    farray<unsigned int,16> icons; // 0x144
};

void init_dlc_objectives(int dlc_id); // 0x00882B60 (:71)
void remove_dlc_objectives(); // 0x00881870 (:107)
void load_objectives(); // 0x008829D0 (:18)
void init_objectives(); // 0x00882C80 (:116)
void objectives_level_init(); // 0x00864F80 (:122)
void create_objective(const char* name, const char* description, const char* display_name, bool mission); // 0x008818E0 (:132)
void add_objective(int handle); // 0x00864FC0 (:156)
void remove_objective(int handle); // 0x00875D00 (:169)
void complete_objective(int handle, bool show_hud); // 0x00875D50 (:189)
void add_objective_icon(int handle, unsigned int object_handle, minimap_elements icon_bitmap, bool only_render_closest); // 0x00879D00 (:209)
void remove_objective_icon(int handle, unsigned int object_handle); // 0x00875D90 (:240)
int objective_get_index_from_name(const char* name); // 0x00875DF0 (:259)
int get_num_active_objectives(); // 0x00864FF0 (:268)
bool get_objective_position(vector& pos, const objective* obj); // 0x00865030 (:289)
bool is_objective_handle(unsigned int object_handle); // 0x008650F0 (:318)
int get_total_num_objectives(); // 0x00865140 (:339)
const char* get_objective_display_name(unsigned int which); // 0x00865150 (:345)
const char* get_objective_description(unsigned int which); // 0x00865180 (:354)
bool get_objective_active(unsigned int which); // 0x008651A0 (:363)
bool get_objective_is_mission(unsigned int which); // 0x008651C0 (:372)
bool get_objective_complete(unsigned int which); // 0x008651E0 (:381)
