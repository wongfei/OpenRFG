// Original: volition/rfg/code/spawn/spawn_object_group.h
// Implementation: volition/rfg/code/spawn/spawn_object_group.cpp
//
// Spawn groups: named lists of human and vehicle infos (with weights and team) that spawn nodes, squads and ambient
// spawning pick from, loaded from the spawn group tables; DLC can replace the default groups.
#pragma once

#include "volition/rfg/code/objects/humans/human_teams.h"
#include "volition/vlib/code/memory/object_pool.h"
#include "volition/vlib/code/util/string_pool.h"
#include "volition/vlib/code/util/varray.h"

class human_info;
class mempool;
class vehicle_info;
struct xml_element;

// size 0x1C
class human_spawn_group_info
{
public:
    human_spawn_group_info(); // 0x00B4C380 (.h:36)
    void reset(); // 0x00B47E10 (:186)
    const human_info* get_index(int i) const; // 0x00B47E20 (:194)
    int get_list_size() const;
    bool load(const human_info* h_info); // 0x00B4F7E0 (:202)
    bool load(xml_element* xml_group_root, mempool* pool); // 0x00B4F870 (:221)
    const human_info* get_random_element() const; // 0x00B47E40 (:275)
    human_teams get_team() const; // 0x00B47E70 (:293)

    const char* group_name; // 0x00
    base_array<human_info const*> h_info_list; // 0x04
    unsigned int name_crc; // 0x14
    bool homogeneous_team; // 0x18
};

// size 0x18
class vehicle_spawn_group_info
{
public:
    vehicle_spawn_group_info(); // 0x00B4C3A0 (.h:84)
    void reset(); // 0x00B47E90 (:305)
    const vehicle_info* get_index(int i) const; // 0x00B47EA0 (:313)
    int get_list_size() const; // 0x00769310 (.h:110)
    bool load(const vehicle_info* v_info); // 0x00B4F9F0 (:320)
    bool load(xml_element* xml_group_root, mempool* pool); // 0x00B4FA70 (:337)
    const vehicle_info* get_random_element() const; // 0x00B47EC0 (:387)
    human_teams get_team() const; // 0x00B47EF0 (:405)
    bool has_same_srid() const; // 0x00B47F10 (:412)
    bool has_same_srid_or_family() const; // 0x00B47F60 (:432)

    const char* group_name; // 0x00
    base_array<vehicle_info const*> v_info_list; // 0x04
    unsigned int name_crc; // 0x14
};

// stub_impl.md S5 (downloadable_content.cpp frees a DLC's spawn groups; placement inferred; the DLC lists are defined
// in downloadable_content.cpp, next to its file statics).
extern object_pool<human_spawn_group_info,64,1> Human_spawn_group_pool; // 0x03748390
extern object_pool<vehicle_spawn_group_info,136,1> Vehicle_spawn_group_pool; // 0x03742480
extern string_pool Spawn_group_string_pool; // 0x0374ACC8
extern farray<human_spawn_group_info*,22> Dlc_human_spawn_group_list; // 0x0165E0C0
extern farray<vehicle_spawn_group_info*,32> Dlc_vehicle_spawn_group_list; // 0x0165E128

void sog_system_init(); // 0x00B5FC90 (:471)
const vehicle_spawn_group_info* sog_get_vehicle_group(const char* group_name); // 0x00B4FBD0 (:503)
const human_spawn_group_info* sog_get_human_group(const char* group_name); // 0x00B4FCB0 (:526)
human_teams sog_get_team(const human_spawn_group_info* hum_group_info); // 0x00B47FD0 (:551)
void load_human_spawn_groups(int dlc_id); // 0x00B5A5C0 (:576)
void load_vehicle_spawn_groups(int dlc_id); // 0x00B5A710 (:659)
void sog_load_dlc_default_spawn_group(); // 0x00B48020 (:735)
void sog_clear_dlc_default_spawn_group(); // 0x00B42E60 (:791)
const human_spawn_group_info* sog_default_human_spawn_group(human_teams team); // 0x00B42E90 (:800)
const vehicle_spawn_group_info* sog_default_vehicle_spawn_group(human_teams team); // 0x00B42EC0 (:813)
