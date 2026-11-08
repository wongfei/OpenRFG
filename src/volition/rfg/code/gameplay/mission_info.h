// Original: volition/rfg/code/gameplay/mission_info.h
// Implementation: volition/rfg/code/gameplay/mission_info.cpp
//
// mission_info, the table entry of a mission (missions.xtbl): names, district, start node, mode, rewards, the
// managed layers and scripts it loads, checkpoints and flags. Placement of mission_mode inferred.
#pragma once

#include "volition/rfg/code/gameplay/mission_info_shared.h"
#include "volition/rfg/code/objects/humans/human_voice/human_voice_shared.h"
#include "volition/vlib/code/util/string_pool.h"
#include "volition/vlib/code/util/varray.h"

struct cfile;
class managed_layer_set;
class spawn_resource_data;
struct xml_element;

enum mission_mode
{
    MISSION_MODE_DEFAULT = 0,
    MISSION_MODE_PARTYTIME = 1,
    MISSION_MODE_WMR = 2,
    MISSION_MODE_ANTS_VS_MAG = 3,
    MISSION_MODE_TUTORIAL = 4,
    MISSION_MODE_FMC = 5,
    MISSION_MODE_SNIPER = 6,
    MISSION_MODE_HIGHWAY_TO_HELL = 7,
    MISSION_MODE_CUTSCENE = 8,
    NUM_MISSION_MODES = 9,
};

// size 0x60
class mission_info
{
public:
    mission_info(); // 0x0077E570 (.h:64)
    ~mission_info(); // 0x0077E5C0 (.h:65)
    void reset(); // 0x00774B40 (:609)
    void level_init(); // 0x0075EB60 (:628)
    bool is_unlocked(); // 0x0075EB70 (:634)
    bool is_hidden() const; // 0x00754420 (.h:130)
    bool needs_notify();
    void mission_completed(); // 0x007ABC10 (:640)
    void load(cfile* file); // 0x0075EB90 (:798)
    bool load(xml_element* xml_root); // 0x0079DE40 (:669)
    bool save(cfile* file); // 0x0075EBB0 (:805)
    mission_info& operator=(const mission_info& __that); // 0x007AC830

    const char* name; // 0x00
    const char* description; // 0x04
    const char* district_name; // 0x08
    const char* crit_area_message; // 0x0C
    unsigned int start_node_handle; // 0x10
    mission_mode mis_mode; // 0x14
    int reward_salvage; // 0x18
    int morale_multiplier; // 0x1C
    bool capstone; // 0x20
    bool hidden; // 0x21
    bool final_mission; // 0x22
    bool no_autosave; // 0x23
    bool cutscene_played; // 0x24
    managed_layer_set* managed_layers; // 0x28
    const spawn_resource_data* srm_data; // 0x2C
    int required_voice_personas; // 0x30
    voice_line_handle crit_area_line; // 0x34
    farray<int,2> managed_script_list; // 0x38
    int checkpoint_managed_script[2]; // 0x50
    struct
    {
        bool complete : 1; // 0x00:0
        bool layers_loaded : 1; // 0x00:1
        bool scripts_loaded : 1; // 0x00:2
        bool checkpoint_script_loaded : 1; // 0x00:3
        bool attempted : 1; // 0x00:4
        bool viewed : 1; // 0x00:5
        bool unlocked : 1; // 0x00:6
        bool unlock_notified : 1; // 0x00:7
        bool ready : 1; // 0x01:0
        bool checkpoint2_script_loaded : 1; // 0x01:1
    } mission_flags; // 0x58
    const char* special_score_name; // 0x5C
};

enum mission_unlock_handle
{
    INVALID_MISSION_UNLOCK_HANDLE = -1,
    MISSION_UNLOCK_HANDLE_FORCE_TO_32_BIT = 0x7FFFFFFF,
};

// size 0x28
struct mission_unlock_group
{
    void reset(); // 0x0075EBD0 (:817)
    bool load(xml_element* xml_root); // 0x0079E150 (:834)
    bool link_unlocks(xml_element* xml_root); // 0x0078E9B0 (:863)
    void unlock_missions(); // 0x0078EA30 (:881)

    const char* name; // 0x00
    bool unlocked_by_default; // 0x04
    mission_handle mission[4]; // 0x08
    mission_unlock_handle unlocks[4]; // 0x18
};

// stub_impl.md S5 (downloadable_content.cpp marks and restores them; placement inferred).
extern farray<mission_info,26> All_mission_infos; // 0x02246C18
extern string_pool Mission_name_string_pool; // 0x02248050

void unused_dcf_mission_status(); // 0x00774A00 (:127)
void unused_dcf_complete_missions(); // 0x007B0A40 (:220)
void unused_dcf_complete_mission(); // 0x007B0A90 (:227)
void unused_dcf_liberate_district(); // 0x007B0B20 (:249)
void mission_info_load_table(int dlc_id); // 0x007B5030 (:280)
void mission_unlock_load_table(int dlc_id); // 0x007A5970 (:387)
void unlock_default_missions(); // 0x007B0C10 (:452)
void set_unlock_all_missions(bool enabled); // 0x0075EB50 (:482)
void mission_info_system_init(); // 0x007B51C0 (:900)
void mission_info_level_init_pre(); // 0x007B0DC0 (:929)
void mission_info_level_init_post(); // 0x0078EAD0 (:941)
mission_handle mission_handle_find(const char* mission_name); // 0x0078EAE0 (:948)
mission_info* mission_info_get(const char* mission_name); // 0x0078EB20 (:965)
mission_info* mission_info_get(mission_handle m_handle); // 0x00774B80 (:977)
bool mission_is_complete(const char* mission_name); // 0x0078EB80 (:990)
bool mission_is_complete(mission_handle m_handle); // 0x00774BB0 (:995)
bool mission_final_mission_complete(); // 0x0075EC00 (:1002)
bool mission_prevent_activity_spawn(); // 0x0078EBA0 (:1010)
bool mission_evaluate_final_mission_complete(); // 0x00774BE0 (:1021)
bool mission_info_save(cfile* file); // 0x00774CA0 (:1067)
void mission_info_load(cfile* file); // 0x0078EC10 (:1091)
int mission_get_num_completed_missions(); // 0x00774D10 (:1126)
int missions_in_district(unsigned int mission_district); // 0x0079E230 (:1139)
int missions_left_in_district(unsigned int mission_district, bool count_hidden); // 0x0079E280 (:1156)
void missions_get_hud_info(unsigned int mission_district, int& num_missions, int& remaining_missions, int& locked_missions); // 0x0079E320 (:1174)
mission_handle mission_capstone_in_district(unsigned int mission_district); // 0x0079E3E0 (:1202)
void mission_notify_new(); // 0x007ABCD0 (:1218)
bool mission_unlock_capstone_in_district(unsigned int mission_district); // 0x007A5C20 (:1255)
void mission_unlock_from_liberation(); // 0x0078EDC0 (:1284)
void mission_skip_tutorial(); // 0x0075EC10 (:1306)
