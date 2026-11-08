// Original: volition/rfg/code/gameplay/mission_info.cpp
//
// The mission table (missions.xtbl) and its unlock groups (mission_unlock.xtbl).

#include "volition/rfg/code/gameplay/mission_info.h"

#include "volition/rfg/code/gamewide/game_trace.h"

#include "keen/base/trace.hpp"
#include "volition/audio_lib/code/audiolib_api.h"
#include "volition/rfg/code/effects/effects_shared.h"
#include "volition/rfg/code/gameplay/activities/activity_internal.h"
#include "volition/rfg/code/gameplay/activities/activity.h"
#include "volition/rfg/code/gameplay/gameflow/gfm_control_morale.h"
#include "volition/rfg/code/gameplay/gameplay.h"
#include "volition/rfg/code/gameplay/managed_layer_loading.h"
#include "volition/rfg/code/gameplay/managed_layer_loading_shared.h"
#include "volition/rfg/code/gameplay/wrecking_crew/wrecking_crew_enums.h"
#include "volition/rfg/code/interface/ui_document_hud.h"
#include "volition/rfg/code/multi/multi_core/include/multi.h"
#include "volition/rfg/code/gameplay/object_district.h"
#include "volition/rfg/code/gamewide/stats.h"
#include "volition/rfg/code/gamewide/gameaudio.h"
#include "volition/rfg/code/hud/hud_message.h"
#include "volition/rfg/code/hud/hud_ui_weapon_cabinet.h"
#include "volition/rfg/code/interface/localization/localization_shared.h"
#include "volition/rfg/code/level/rfg_zonex_shared.h"
#include "volition/rfg/code/level/world.h"
#include "volition/rfg/code/objects/humans/human.h"
#include "volition/rfg/code/objects/humans/human_voice/human_voice.h"
#include "volition/rfg/code/objects/humans/human_voice/human_voice_lines.h"
#include "volition/rfg/code/objects/humans/player.h"
#include "volition/rfg/code/objects/humans/player_meta_data.h"
#include "volition/rfg/code/objects/object_mission_start_node.h"
#include "volition/rfg/code/rfg.h"
#include "volition/rfg/code/scripting/managed_script_group.h"
#include "volition/rfg/code/spawn/spawn_resource_data.h"
#include "volition/vlib/code/cfile/cfile.h"
#include "volition/vlib/code/memory/rfg_split_mempool.h"
#include "volition/vlib/code/memory/virtual_mempool.h"
#include "volition/vlib/code/os/vconsole.h"
#include "volition/vlib/code/util/fastxml.h"
#include "volition/vlib/code/util/hash_table.h"
#include "volition/vlib/code/parse/xml_table_tools.h"

#include <stdio.h>

extern bool Unlock_all_missions; // 0x0222966D (missions.cpp)
extern bool Mission_parker_liberation_warp;
extern wrecking_crew_states Wrecking_crew_state; // 0x01651888 (wrecking_crew.cpp)
unsigned int get_default_district_handle(); // object_district.cpp
extern activity_base* All_activities[17]; // 0x01651248 (gameplay.cpp)

static const char MISSION_NAME_STRING_POOL_NAME[14] = "Mission Names"; // 0x012DBDC8
bool Mission_final_mission_complete; // 0x0222966F
unsigned int Mission_effect_info_handle; // used by object_mission_start_node.cpp
farray<mission_info,26> All_mission_infos; // 0x02246C18
string_pool Mission_name_string_pool(MISSION_NAME_STRING_POOL_NAME); // 0x02248050
static const char MISSION_EFFECT_NAME[23] = "gp_marker_flare_blue_a"; // 0x012DBDD8
static const char TUTORIAL_MISSION_NAME[9] = "Tutorial"; // 0x012DBDF0
static const char* Mission_mode_names[9] = {"default", "partytime", "WMR", "ants vs mag", "tutorial", "FMC", "SniperHunter", "H2H", "Cutscene"}; // 0x01651518
static farray<mission_unlock_group,30> All_mission_unlocks; // 0x01658058
extern unsigned int District_map[257]; // 0x022267D8 (object_district.cpp)
static bool Skip_tutorial; // 0x0222966E
static char* Mission_name_string_pool_data;
static str_data_hash_table_cached<mission_handle,26,unsigned char> Mission_hash_table;
static str_data_hash_table_cached<mission_unlock_handle,30,unsigned char> Mission_unlock_hash_table;

// Magic numbers: the upgrades these missions unlock, from unlock_radiation.
static const int UPGRADE_RADIATION_SHIELD = 0x12;
static const int UPGRADE_EOS_SECOND_MISSIONS = 0x1B;
// Magic numbers: upgrades that gate hidden missions, the capstone of Save the Guerrilla Camp and Guns of Tharsis.
static const int UPGRADE_SAVE_CAMP_CAPSTONE = 0x0F;
static const int UPGRADE_LIBERATED_A = 0x17;
static const int UPGRADE_LIBERATED_B = 0x18;
// Magic numbers: the type of district objects and of mission start nodes (object_type values).
static const int OBJ_TYPE_DISTRICT = 0x20;
static const int OBJ_TYPE_MISSION_START_NODE = 0x2D;

static void unlock_new_missions(mission_handle completed_mission);

// 0x00774A00 (:127)
void unused_dcf_mission_status()
{
    int complete = 0;
    for (int i = 0; i < All_mission_infos.size(); i++)
    {
        const mission_info& info = All_mission_infos[i];
        complete += info.mission_flags.complete ? 1 : 0;
        char line[256];
        sprintf(line, "  %s  %s  %s", info.mission_flags.unlocked || Unlock_all_missions ? "unlocked" : "LOCKED ", info.mission_flags.complete ? "COMPLETE " : "incomplete", info.name);
        console_output(line, nullptr, false);
    }
    console_printf("missions complete: %d\n", complete);
}

// 0x0079DC30 (:141)
static void unlock_radiation()
{
    mission_info* info = mission_info_get("Save the Guerrilla Camp");
    if (info != nullptr && info->mission_flags.complete)
    {
        player_meta_data* meta = player_get_meta_data();
        if (meta != nullptr)
        {
            meta->unlock_upgrade(UPGRADE_RADIATION_SHIELD, 1, false);
            // Sic: rfg.exe's spelling.
            console_output("Radiation sheild upgrade unlocked", nullptr, false);
        }
    }
    info = mission_info_get("Marauder Temple");
    if (info != nullptr && info->mission_flags.complete)
    {
        player_meta_data* meta = player_get_meta_data();
        if (meta != nullptr)
        {
            meta->unlock_upgrade(UPGRADE_EOS_SECOND_MISSIONS, 1, false);
            console_output("Eos second set of missions unlocked", nullptr, false);
        }
    }
}

// 0x007B07F0 (:162)
static void complete_missions_in_district(unsigned int district_handle)
{
    // Magic number: five passes, so each pass's completions can unlock the next pass's missions.
    for (int pass = 0; pass < 5; pass++)
    {
        for (int i = 0; i < All_mission_infos.size(); i++)
        {
            mission_info& info = All_mission_infos[i];
            if (info.district_name != nullptr && get_district_by_name(info.district_name) == district_handle && info.mission_flags.unlocked && !info.mission_flags.complete)
            {
                info.mission_completed();
            }
        }
        mission_info* camp = mission_info_get("Save the Guerrilla Camp");
        if (camp != nullptr && camp->mission_flags.unlocked && !camp->mission_flags.complete)
        {
            camp->mission_completed();
            console_output("Completed Save the Guerrilla Camp", nullptr, false);
            unlock_radiation();
        }
        mission_info* temple = mission_info_get("Marauder Temple");
        if (temple != nullptr && temple->mission_flags.unlocked && !temple->mission_flags.complete)
        {
            temple->mission_completed();
            console_output("Completed Marauder Temple", nullptr, false);
            unlock_radiation();
        }
    }
    bool forced = false;
    for (int i = 0; i < All_mission_infos.size(); i++)
    {
        mission_info& info = All_mission_infos[i];
        if (info.district_name != nullptr && get_district_by_name(info.district_name) == district_handle && !info.mission_flags.complete)
        {
            info.mission_completed();
            forced = true;
        }
    }
    object_mission_start_node_update_markers();
    mission_notify_new();
    console_output(forced ? "Had to force some missions unlocked to get the job done" : "Missions completed successfully and unlocked properly", nullptr, false);
}

// 0x007B0A40 (:220)
void unused_dcf_complete_missions()
{
    player* pp = player_get_local_player();
    district* dp = pp != nullptr ? district_get_from_pos(pp->pos) : World.get_object<district>(0);
    complete_missions_in_district(dp != nullptr ? dp->get_handle() : 0xFFFFFFFF);
}

// 0x007B0A90 (:227)
void unused_dcf_complete_mission()
{
    if (console_get_arg(2, 1))
    {
        mission_info* info = mission_info_get(Console_arg);
        if (info == nullptr)
        {
            console_output("Could not find mission. Use mission_status for mission names and use quotes", nullptr, false);
        }
        else if (info->mission_flags.complete)
        {
            console_output("Mission already complete.", nullptr, false);
        }
        else
        {
            info->mission_completed();
            object_mission_start_node_update_markers();
            console_output("Completed Mission", nullptr, false);
            mission_evaluate_final_mission_complete();
        }
    }
    unlock_radiation();
    mission_notify_new();
}

// 0x007B0B20 (:249)
void unused_dcf_liberate_district()
{
    unsigned int handle;
    if (console_get_arg(2, 1))
    {
        handle = get_district_by_name(Console_arg);
    }
    else
    {
        player* pp = player_get_local_player();
        district* dp = pp != nullptr ? district_get_from_pos(pp->pos) : World.get_object<district>(0);
        if (dp == nullptr)
        {
            console_output("Unable to find district.  Use Manufacturing for Free Fire Zone", nullptr, false);
            return;
        }
        handle = dp->get_handle();
    }
    object* objp = handle != 0xFFFFFFFF ? World.object_from_handle(handle) : nullptr;
    if (objp == nullptr || objp->get_obj_type() != OBJ_TYPE_DISTRICT || handle == get_district_by_name("Default"))
    {
        console_output("Unable to find district.  Use Manufacturing for Free Fire Zone", nullptr, false);
        return;
    }
    district* dp = (district*)objp;
    if (!gfm_set_edf_control(0.0f, dp))
    {
        console_output("Set EDF control to 0 has failed!", nullptr, false);
    }
    Mission_parker_liberation_warp = false;
    complete_missions_in_district(handle);
    if (!dp->is_liberated())
    {
        console_output("District liberation has failed!", nullptr, false);
    }
}

// 0x007B5030 (:280)
void mission_info_load_table(int dlc_id)
{
    const char* system_name;
    const char* table_name;
    char dlc_name[256];
    if (dlc_id == -1)
    {
        All_mission_infos.clear();
        system_name = "mission_containers";
        table_name = "missions.xtbl";
    }
    else
    {
        system_name = "dlc_mission_containers";
        sprintf(dlc_name, "dlc%02d_%s", dlc_id, "missions.xtbl");
        table_name = dlc_name;
    }
    // Magic number: 7, this table's container category (rfg.exe's constant).
    managed_layer_loading_parse_shared(system_name, table_name, "mission", 7, &Common_mempool, &Permanent_mempool, Use_packfiles, PLATFORM_PC, false, dlc_id);
    xml_element* root = xml_table_parse(table_name);
    if (root == nullptr)
    {
        return;
    }
    for (xml_element* element = xml_find(root, "mission"); element != nullptr; element = xml_find_next(root, element, "mission"))
    {
        int index;
        mission_info* info = All_mission_infos.add_in_place(index);
        if (info == nullptr)
        {
            break;
        }
        if (info->load(element))
        {
            if (!Mission_hash_table.exists(info->name))
            {
                Mission_hash_table.insert_key_data(info->name, (mission_handle)index);
            }
        }
        else
        {
            info->reset();
            All_mission_infos.remove_unsorted(index);
        }
    }
    xml_table_close(root);
}

// 0x0078E970 (:337)
static mission_unlock_handle mission_unlock_find(const char* unlock_name)
{
    mission_unlock_handle handle;
    return Mission_unlock_hash_table.get(unlock_name, handle) ? handle : INVALID_MISSION_UNLOCK_HANDLE;
}

// 0x00774AC0 (:353)
static mission_unlock_group* mission_unlock_get(mission_unlock_handle m_handle)
{
    if (m_handle != INVALID_MISSION_UNLOCK_HANDLE && (int)m_handle >= 0 && (int)m_handle < All_mission_unlocks.size())
    {
        return &All_mission_unlocks[(int)m_handle];
    }
    return nullptr;
}

// 0x00774AF0 (:366)
static mission_unlock_group* mission_unlock_find_by_mission(mission_handle mission_handle_to_find)
{
    if (mission_handle_to_find == INVALID_MISSION_HANDLE)
    {
        return nullptr;
    }
    // Magic numbers: the array's capacity, read past its count as rfg.exe does; 4 missions per group.
    for (int i = 0; i < 30; i++)
    {
        for (int j = 0; j < 4; j++)
        {
            if (All_mission_unlocks[i].mission[j] == mission_handle_to_find)
            {
                return &All_mission_unlocks[i];
            }
        }
    }
    return nullptr;
}

// 0x007A5970 (:387)
void mission_unlock_load_table(int dlc_id)
{
    const char* table_name = "mission_unlock.xtbl";
    char dlc_name[64];
    if (dlc_id >= 0)
    {
        sprintf(dlc_name, "dlc%02d_%s", dlc_id, table_name);
        if (!cf_exist(dlc_name))
        {
            return;
        }
        table_name = dlc_name;
    }
    xml_element* root = xml_table_parse(table_name);
    Mission_unlock_hash_table.clear();
    All_mission_unlocks.clear();
    if (root == nullptr)
    {
        return;
    }
    for (xml_element* element = xml_find(root, "mission_unlock"); element != nullptr; element = xml_find_next(root, element, "mission_unlock"))
    {
        int index;
        mission_unlock_group* group = All_mission_unlocks.add_in_place(index);
        if (group == nullptr)
        {
            break;
        }
        if (group->load(element))
        {
            if (!Mission_unlock_hash_table.exists(group->name))
            {
                Mission_unlock_hash_table.insert_key_data(group->name, (mission_unlock_handle)index);
            }
        }
        else
        {
            group->reset();
            All_mission_unlocks.remove_unsorted(index);
        }
    }
    for (xml_element* element = xml_find(root, "mission_unlock"); element != nullptr; element = xml_find_next(root, element, "mission_unlock"))
    {
        xml_element* name = xml_find(element, "Name");
        if (name != nullptr)
        {
            const mission_unlock_handle handle = mission_unlock_find(name->text);
            mission_unlock_group* group = handle != INVALID_MISSION_UNLOCK_HANDLE ? mission_unlock_get(handle) : nullptr;
            if (group != nullptr)
            {
                group->link_unlocks(element);
            }
        }
    }
    xml_table_close(root);
}

// 0x007B0C10 (:452)
void unlock_default_missions()
{
    for (int i = 0; i < All_mission_unlocks.size(); i++)
    {
        mission_unlock_group& group = All_mission_unlocks[i];
        if (group.unlocked_by_default)
        {
            for (int j = 0; j < 4; j++)
            {
                mission_info* info = mission_info_get(group.mission[j]);
                if (group.mission[j] != INVALID_MISSION_HANDLE && info != nullptr)
                {
                    info->mission_flags.unlocked = true;
                    info->mission_flags.unlock_notified = true;
                }
            }
        }
    }
    if (Skip_tutorial)
    {
        mission_info* tutorial = mission_info_get(TUTORIAL_MISSION_NAME);
        if (tutorial != nullptr)
        {
            tutorial->mission_completed();
        }
        Skip_tutorial = false;
    }
}

// 0x0075EB50 (:482)
void set_unlock_all_missions(bool enabled)
{
    Unlock_all_missions = enabled;
}

// 0x007ABAB0 (:488) LTCG: completed_mission in EDI and on the stack (the same value)
static void unlock_new_missions(mission_handle completed_mission)
{
    mission_unlock_group* group = mission_unlock_find_by_mission(completed_mission);
    if (group != nullptr)
    {
        bool all_complete = true;
        for (int j = 0; j < 4; j++)
        {
            mission_info* info = mission_info_get(group->mission[j]);
            if (info != nullptr && !info->mission_flags.complete)
            {
                all_complete = false;
            }
        }
        if (all_complete)
        {
            for (int j = 0; j < 4; j++)
            {
                mission_unlock_group* next = mission_unlock_get(group->unlocks[j]);
                if (next != nullptr)
                {
                    next->unlock_missions();
                }
            }
        }
    }
    if (!Skip_tutorial && completed_mission != INVALID_MISSION_HANDLE)
    {
        mission_info* info = mission_info_get(completed_mission);
        if (info != nullptr && info->district_name != nullptr && !info->final_mission)
        {
            const unsigned int mission_district = get_district_by_name(info->district_name);
            mission_unlock_capstone_in_district(mission_district);
            object* objp = mission_district != 0xFFFFFFFF ? World.object_from_handle(mission_district) : nullptr;
            if (objp != nullptr && objp->get_obj_type() == OBJ_TYPE_DISTRICT)
            {
                ((district*)objp)->liberate_district();
            }
        }
    }
}

// 0x007A5B80 (:540)
static district* capstone_mission_unlocked()
{
    district* found = nullptr;
    for (int i = 0; i < All_mission_infos.size(); i++)
    {
        if (found != nullptr)
        {
            return found;
        }
        mission_info& info = All_mission_infos[i];
        // Unlocked, not complete and not yet notified: flag bits 0x40, 0x01 and 0x80 of the byte rfg.exe tests.
        if (info.district_name != nullptr && info.mission_flags.unlocked && !info.mission_flags.complete && !info.mission_flags.unlock_notified)
        {
            const unsigned int mission_district = get_district_by_name(info.district_name);
            if ((info.capstone || missions_left_in_district(mission_district, false) == 1) && !info.final_mission)
            {
                object* objp = mission_district != 0xFFFFFFFF ? World.object_from_handle(mission_district) : nullptr;
                found = objp != nullptr && objp->get_obj_type() == OBJ_TYPE_DISTRICT ? (district*)objp : nullptr;
            }
        }
    }
    return found;
}

// 0x0079DD40 (:563) LTCG: num_missions_unlocked in EDI, all_unlocks_in_players_district on the stack
static void mission_clear_unlock_notify(int& num_missions_unlocked, bool& all_unlocks_in_players_district)
{
    unsigned int player_district = 0xFFFFFFFF;
    player* pp = player_get_local_player();
    if (pp != nullptr)
    {
        int x;
        int z;
        if (rfg_zonex_get_grid_coords(pp->pos, x, z))
        {
            // Magic number: the district map is 16 zones wide.
            const int cell = z * 16 + x;
            if (cell < 257)
            {
                player_district = District_map[cell];
                if (player_district == 0xFFFFFFFF)
                {
                    player_district = get_default_district_handle();
                }
            }
        }
    }
    num_missions_unlocked = 0;
    all_unlocks_in_players_district = true;
    for (int i = 0; i < All_mission_infos.size(); i++)
    {
        mission_info& info = All_mission_infos[i];
        if (info.mission_flags.unlocked && !info.mission_flags.unlock_notified)
        {
            info.mission_flags.unlock_notified = true;
            num_missions_unlocked++;
            if (info.district_name == nullptr || (all_unlocks_in_players_district && get_district_by_name(info.district_name) != player_district))
            {
                all_unlocks_in_players_district = false;
            }
            object* node = info.start_node_handle != 0xFFFFFFFF ? World.object_from_handle(info.start_node_handle) : nullptr;
            if (node != nullptr && node->get_obj_type() == OBJ_TYPE_MISSION_START_NODE)
            {
                ((object_mission_start_node*)node)->enable_markers();
            }
        }
    }
}

// 0x0077E570 (.h:64)
mission_info::mission_info()
{
    reset();
}

// 0x0077E5C0 (.h:65)
mission_info::~mission_info()
{
}

// 0x007AC830, compiler generated in rfg.exe (farray<mission_info,26>::remove_unsorted). Written out because a farray
// must copy its elements, not its pointer to them.
mission_info& mission_info::operator=(const mission_info& __that)
{
    name = __that.name;
    description = __that.description;
    district_name = __that.district_name;
    crit_area_message = __that.crit_area_message;
    start_node_handle = __that.start_node_handle;
    mis_mode = __that.mis_mode;
    reward_salvage = __that.reward_salvage;
    morale_multiplier = __that.morale_multiplier;
    capstone = __that.capstone;
    hidden = __that.hidden;
    final_mission = __that.final_mission;
    no_autosave = __that.no_autosave;
    cutscene_played = __that.cutscene_played;
    managed_layers = __that.managed_layers;
    srm_data = __that.srm_data;
    required_voice_personas = __that.required_voice_personas;
    crit_area_line = __that.crit_area_line;
    managed_script_list.copy(__that.managed_script_list); // farray::operator= with its by-value result dropped
    checkpoint_managed_script[0] = __that.checkpoint_managed_script[0];
    checkpoint_managed_script[1] = __that.checkpoint_managed_script[1];
    mission_flags = __that.mission_flags;
    special_score_name = __that.special_score_name;
    return *this;
}

// 0x00774B40 (:609)
void mission_info::reset()
{
    name = nullptr;
    description = nullptr;
    crit_area_message = nullptr;
    managed_script_list.clear();
    checkpoint_managed_script[0] = -1;
    checkpoint_managed_script[1] = -1;
    managed_layers = nullptr;
    srm_data = nullptr;
    reward_salvage = 0;
    morale_multiplier = 1;
    start_node_handle = 0xFFFFFFFF;
    required_voice_personas = INVALID_VOICE_SET_HANDLE;
    memset(&mission_flags, 0, sizeof(mission_flags));
}

// 0x0075EB60 (:628)
void mission_info::level_init()
{
    start_node_handle = 0xFFFFFFFF;
    memset(&mission_flags, 0, sizeof(mission_flags));
}

// 0x0075EB70 (:634)
bool mission_info::is_unlocked()
{
    return mission_flags.unlocked || Unlock_all_missions;
}

// 0x00754420 (.h:130)
bool mission_info::is_hidden() const
{
    return hidden;
}

// 0x007ABC10 (:640)
void mission_info::mission_completed()
{
    GAME_TRACE("mission", "%s completed: unlocks the next missions", name);
    mission_flags.complete = true;
    mission_flags.unlocked = true;
    mission_flags.viewed = true;
    mission_flags.attempted = true;
    unlock_new_missions(mission_handle_find(name));
    if (mis_mode == MISSION_MODE_TUTORIAL)
    {
        stats_set_stat(STATS_TUTORIAL_COMPLETED, 1);
        stats_update_game_progress_hero_stat();
        hud_ui_weapon_cabinet_reset();
    }
    else if (mis_mode != MISSION_MODE_CUTSCENE)
    {
        stats_increment_by_one(STATS_MISSIONS_COMPLETED);
    }
    for (int i = 0; i < 17; i++)
    {
        if (All_activities[i] != nullptr)
        {
            // Magic number: flag 2, the activities a mission disabled (the tutorial's mayhem) come back.
            All_activities[i]->activity_disable_flags &= 0xFFFFFFFD;
        }
    }
    mission_evaluate_final_mission_complete();
    if (Mission_final_mission_complete)
    {
        activity_reinstate_activities(ADR_OVERRIDE_ALL_REASONS);
    }
}

// 0x0079DE40 (:669)
bool mission_info::load(xml_element* xml_root)
{
    reset();
    xml_element* element = xml_find(xml_root, "Name");
    if (element == nullptr)
    {
        return false;
    }
    name = Mission_name_string_pool.add_unique(element->text);
    if (name == nullptr)
    {
        return false;
    }
    element = xml_find(xml_root, "description");
    if (element == nullptr)
    {
        return false;
    }
    description = Mission_name_string_pool.add_unique(element->text);
    if (description == nullptr)
    {
        return false;
    }
    element = xml_find(xml_root, "district");
    district_name = element != nullptr ? Mission_name_string_pool.add_unique(element->text) : nullptr;
    xml_table_get_req_bool(capstone, xml_root, "capstone");
    xml_table_get_req_bool(hidden, xml_root, "hidden");
    xml_table_get_req_bool(final_mission, xml_root, "final_mission");
    element = xml_find(xml_root, "crit_area_line");
    if (element == nullptr)
    {
        return false;
    }
    crit_area_line = voice_line_handle_from_name(element->text);
    element = xml_find(xml_root, "crit_area_text");
    crit_area_message = element != nullptr ? Mission_name_string_pool.add_unique(element->text) : nullptr;
    element = xml_find(xml_root, "resource_reservation");
    if (element != nullptr)
    {
        srm_data = spawn_res_data_find(element->text);
        if (srm_data == nullptr)
        {
            return false;
        }
    }
    element = xml_table_find_req(xml_root, "mode");
    if (element == nullptr)
    {
        return false;
    }
    mis_mode = (mission_mode)xml_table_get_type_from_list(Mission_mode_names, 9, element);
    xml_table_get_req_int(reward_salvage, xml_root, "reward_salvage");
    xml_table_get_req_int(morale_multiplier, xml_root, "morale_multiplier");
    managed_layers = managed_layer_load(name, xml_root, true);
    required_voice_personas = human_voice_load_set(xml_root);
    xml_element* groups = xml_find(xml_root, "script_groups");
    if (groups != nullptr)
    {
        for (element = xml_find(groups, "script_group_name"); element != nullptr; element = xml_find_next(groups, element, "script_group_name"))
        {
            int handle = managed_script_get_handle(element->text);
            if (handle == -1)
            {
                handle = managed_script_create_handle(element->text);
            }
            if (handle != -1)
            {
                managed_script_list.add(handle);
            }
        }
    }
    groups = xml_find(xml_root, "checkpoint_script_groups");
    if (groups != nullptr)
    {
        int index = 0;
        for (element = xml_find(groups, "script_group_name"); element != nullptr; element = xml_find_next(groups, element, "script_group_name"))
        {
            if (index > 1 || checkpoint_managed_script[index] != -1)
            {
                return false;
            }
            checkpoint_managed_script[index] = managed_script_create_handle(element->text);
            if (checkpoint_managed_script[index] == -1)
            {
                return false;
            }
            index++;
        }
    }
    element = xml_find(xml_root, "special_score");
    special_score_name = element != nullptr ? Mission_name_string_pool.add_unique(element->text) : nullptr;
    xml_table_get_req_bool(no_autosave, xml_root, "No_Autosave");
    xml_table_get_req_bool(cutscene_played, xml_root, "cutscene_played");
    return true;
}

// 0x0075EB90 (:798)
void mission_info::load(cfile* file)
{
    cf_read(&mission_flags, 2, file, false);
}

// 0x0075EBB0 (:805)
bool mission_info::save(cfile* file)
{
    return cf_write(&mission_flags, 2, file) != 0;
}

// 0x0075EBD0 (:817)
void mission_unlock_group::reset()
{
    name = nullptr;
    unlocked_by_default = false;
    for (int i = 0; i < 4; i++)
    {
        mission[i] = INVALID_MISSION_HANDLE;
        unlocks[i] = INVALID_MISSION_UNLOCK_HANDLE;
    }
}

// 0x0079E150 (:834)
bool mission_unlock_group::load(xml_element* xml_root)
{
    reset();
    xml_element* element = xml_find(xml_root, "Name");
    if (element == nullptr)
    {
        return false;
    }
    name = Mission_name_string_pool.add_unique(element->text);
    if (name == nullptr)
    {
        return false;
    }
    xml_table_get_req_bool(unlocked_by_default, xml_root, "unlocked");
    xml_element* missions = xml_find(xml_root, "missions");
    if (missions == nullptr)
    {
        return false;
    }
    int index = 0;
    for (element = xml_find(missions, "mission"); element != nullptr; element = xml_find_next(missions, element, "mission"))
    {
        if (index > 3)
        {
            return false;
        }
        mission[index++] = mission_handle_find(element->text);
    }
    return true;
}

// 0x0078E9B0 (:863)
bool mission_unlock_group::link_unlocks(xml_element* xml_root)
{
    xml_element* groups = xml_find(xml_root, "mission_unlocks");
    if (groups == nullptr)
    {
        return false;
    }
    int index = 0;
    for (xml_element* element = xml_find(groups, "mission_group"); element != nullptr; element = xml_find_next(groups, element, "mission_group"))
    {
        if (index > 3)
        {
            return false;
        }
        unlocks[index++] = mission_unlock_find(element->text);
    }
    return true;
}

// 0x0078EA30 (:881)
void mission_unlock_group::unlock_missions()
{
    for (int i = 0; i < 4; i++)
    {
        mission_info* info = mission_info_get(mission[i]);
        if (info != nullptr)
        {
            info->mission_flags.unlocked = true;
        }
    }
}

// 0x007B51C0 (:900)
void mission_info_system_init()
{
    if (Mission_name_string_pool_data == nullptr)
    {
        // Magic number: the pool's size in rfg.exe.
        Mission_name_string_pool_data = (char*)Permanent_mempool.alloc(0x5AA, 0);
        if (Mission_name_string_pool_data == nullptr)
        {
            keen::debug::nativePrintArgs("Insufficient memory to allocate the mission name string table");
        }
        Mission_name_string_pool.init(Mission_name_string_pool_data, 0x5AA, MISSION_NAME_STRING_POOL_NAME, 0, 0);
    }
    Mission_name_string_pool.clear();
    Mission_hash_table.clear();
    Mission_effect_info_handle = effects_get_handle(MISSION_EFFECT_NAME);
    All_mission_infos.clear();
    mission_info_load_table(-1);
    mission_unlock_load_table(-1);
    unlock_default_missions();
}

// 0x007B0DC0 (:929)
void mission_info_level_init_pre()
{
    for (int i = 0; i < All_mission_infos.size(); i++)
    {
        All_mission_infos[i].level_init();
    }
    unlock_default_missions();
}

// 0x0078EAD0 (:941)
void mission_info_level_init_post()
{
    mission_evaluate_final_mission_complete();
}

// 0x0078EAE0 (:948)
mission_handle mission_handle_find(const char* mission_name)
{
    mission_handle handle;
    return Mission_hash_table.get(mission_name, handle) ? handle : INVALID_MISSION_HANDLE;
}

// 0x0078EB20 (:965)
mission_info* mission_info_get(const char* mission_name)
{
    return mission_info_get(mission_handle_find(mission_name));
}

// 0x00774B80 (:977)
mission_info* mission_info_get(mission_handle m_handle)
{
    if (m_handle != INVALID_MISSION_HANDLE && (int)m_handle >= 0 && (int)m_handle < All_mission_infos.size())
    {
        return &All_mission_infos[(int)m_handle];
    }
    return nullptr;
}

// 0x0078EB80 (:990)
bool mission_is_complete(const char* mission_name)
{
    mission_info* info = mission_info_get(mission_name);
    return info != nullptr && info->mission_flags.complete;
}

// 0x00774BB0 (:995)
bool mission_is_complete(mission_handle m_handle)
{
    mission_info* info = mission_info_get(m_handle);
    return info != nullptr && info->mission_flags.complete;
}

// 0x0075EC00 (:1002)
bool mission_final_mission_complete()
{
    return Mission_final_mission_complete;
}

// 0x0078EBA0 (:1010)
bool mission_prevent_activity_spawn()
{
    mission_info* info = mission_info_get("Save the Guerrilla Camp");
    return info != nullptr && info->mission_flags.complete && !Mission_final_mission_complete;
}

// 0x00774BE0 (:1021)
bool mission_evaluate_final_mission_complete()
{
    if (Multiplayer || Wrecking_crew_state != WCS_NONE)
    {
        return false;
    }
    bool found = false;
    for (int i = 0; i < All_mission_infos.size(); i++)
    {
        mission_info& info = All_mission_infos[i];
        if (info.final_mission && info.start_node_handle != 0xFFFFFFFF)
        {
            object* node = World.object_from_handle(info.start_node_handle);
            if (node != nullptr && node->get_obj_type() == OBJ_TYPE_MISSION_START_NODE)
            {
                found = true;
                if (!info.mission_flags.complete)
                {
                    Mission_final_mission_complete = false;
                    return false;
                }
            }
        }
    }
    if (found)
    {
        if (World.dlc_bundle_id == -1)
        {
            stats_set_stat(STATS_GAME_COMPLETED, 1);
            stats_set_stat(STATS_GAME_COMPLETED_PROFILE, 1);
            stats_update_game_progress_hero_stat();
        }
        Mission_final_mission_complete = true;
        return true;
    }
    return false;
}

// 0x00774CA0 (:1067)
bool mission_info_save(cfile* file)
{
    const int count = All_mission_infos.size();
    if (!cf_write_int32(count, file))
    {
        return false;
    }
    for (int i = 0; i < count; i++)
    {
        if (!cf_write_string(All_mission_infos[i].name, file) || !All_mission_infos[i].save(file))
        {
            return false;
        }
    }
    return true;
}

// 0x0078EC10 (:1091)
void mission_info_load(cfile* file)
{
    const int count = (int)cf_read_uint32(file);
    for (int i = 0; i < count; i++)
    {
        char name[512];
        if (!cf_read_string(name, sizeof(name), file))
        {
            keen::debug::nativePrintArgs("Int3 critical function at %s(%d)\n", "mission_info.cpp", 1091);
            return;
        }
        mission_info* info = mission_info_get(name);
        if (info != nullptr)
        {
            info->load(file);
        }
        else
        {
            keen::debug::nativePrintArgs("Int3 critical function at %s(%d)\n", "mission_info.cpp", 1091);
            unsigned short flags;
            cf_read(&flags, 2, file, false);
        }
    }
}

// 0x00774D10 (:1126)
int mission_get_num_completed_missions()
{
    int count = 0;
    for (int i = 0; i < All_mission_infos.size(); i++)
    {
        count += All_mission_infos[i].mission_flags.complete ? 1 : 0;
    }
    return count;
}

// 0x0079E230 (:1139)
int missions_in_district(unsigned int mission_district)
{
    int count = 0;
    for (int i = 0; i < All_mission_infos.size(); i++)
    {
        if (All_mission_infos[i].district_name != nullptr && get_district_by_name(All_mission_infos[i].district_name) == mission_district)
        {
            count++;
        }
    }
    return count;
}

static bool player_has_eos_second_missions()
{
    if (Multiplayer || Wrecking_crew_state != WCS_NONE)
    {
        return false;
    }
    player_meta_data* meta = player_get_meta_data();
    return meta != nullptr && meta->get_upgrade_value(UPGRADE_EOS_SECOND_MISSIONS) != 0;
}

// 0x0079E280 (:1156)
int missions_left_in_district(unsigned int mission_district, bool count_hidden)
{
    const bool hidden_too = count_hidden || player_has_eos_second_missions();
    int count = 0;
    for (int i = 0; i < All_mission_infos.size(); i++)
    {
        const mission_info& info = All_mission_infos[i];
        if (info.district_name != nullptr && get_district_by_name(info.district_name) == mission_district && !info.mission_flags.complete && (hidden_too || !info.hidden))
        {
            count++;
        }
    }
    return count;
}

// 0x0079E320 (:1174)
void missions_get_hud_info(unsigned int mission_district, int& num_missions, int& remaining_missions, int& locked_missions)
{
    num_missions = 0;
    remaining_missions = 0;
    locked_missions = 0;
    const bool hidden_too = player_has_eos_second_missions();
    for (int i = 0; i < All_mission_infos.size(); i++)
    {
        const mission_info& info = All_mission_infos[i];
        if (info.district_name != nullptr && get_district_by_name(info.district_name) == mission_district && (hidden_too || !info.hidden))
        {
            num_missions++;
            if (!info.mission_flags.complete)
            {
                remaining_missions++;
            }
            if (!info.mission_flags.unlocked)
            {
                locked_missions++;
            }
        }
    }
}

// 0x0079E3E0 (:1202)
mission_handle mission_capstone_in_district(unsigned int mission_district)
{
    for (int i = 0; i < All_mission_infos.size(); i++)
    {
        const mission_info& info = All_mission_infos[i];
        if (info.district_name != nullptr && get_district_by_name(info.district_name) == mission_district && info.capstone)
        {
            return (mission_handle)i;
        }
    }
    return INVALID_MISSION_HANDLE;
}

// 0x007ABCD0 (:1218)
void mission_notify_new()
{
    district* capstone = capstone_mission_unlocked();
    bool all_in_players_district = true;
    int unlocked = 0;
    mission_clear_unlock_notify(unlocked, all_in_players_district);
    if (unlocked <= 0)
    {
        return;
    }
    wchar_t text[256];
    if (capstone == nullptr)
    {
        hud_message_get_icon_with_localized_text(text, 256, L"ui_map_icon_mission_start", unlocked < 2 ? "HM_NEW_MISSION_AVAILABLE" : "HM_NEW_MISSIONS_AVAILABLE");
    }
    else
    {
        wchar_t format[256];
        hud_message_get_icon_with_localized_text(format, 256, L"ui_map_icon_mission_start", "HM_FINAL_MISSION_AVAILABLE");
        _snwprintf_s(text, 256, _TRUNCATE, format, localization_get_display_string_req(capstone->get_localized_name_hash()));
        capstone->play_capstone_radio_lines();
    }
    // Magic number: 5 s on screen.
    hud_message_add_minor(text, 5.0f, true);
    audiolib_cue_play(Ga_cue_ids.general_new_mission_pop_up, nullptr, nullptr);
    if (all_in_players_district)
    {
        player* pp = player_get_local_player();
        district* dp = pp != nullptr ? district_get_from_pos(pp->pos) : World.get_object<district>(0);
        if (dp != nullptr)
        {
            hud_ui_change_control(gfm_get_edf_control_display_value(dp), gfm_get_edf_control_percent(dp), 0, 0.0f, dp->get_handle(), false, unlocked);
        }
    }
}

// 0x007A5C20 (:1255)
bool mission_unlock_capstone_in_district(unsigned int mission_district)
{
    const mission_handle handle = mission_capstone_in_district(mission_district);
    if (handle == INVALID_MISSION_HANDLE)
    {
        return false;
    }
    mission_info* info = mission_info_get(handle);
    object* objp = mission_district != 0xFFFFFFFF ? World.object_from_handle(mission_district) : nullptr;
    district* dp = objp != nullptr && objp->get_obj_type() == OBJ_TYPE_DISTRICT ? (district*)objp : nullptr;
    if (info == nullptr || dp == nullptr || gfm_get_edf_control(dp) != 0.0f || missions_left_in_district(mission_district, false) != 1 || info->mission_flags.unlocked)
    {
        return false;
    }
    if (info == mission_info_get("Save the Guerrilla Camp"))
    {
        player_meta_data* meta = player_get_meta_data();
        if (meta == nullptr || meta->get_upgrade_value(UPGRADE_SAVE_CAMP_CAPSTONE) <= 0)
        {
            return false;
        }
    }
    info->mission_flags.unlocked = true;
    return true;
}

// 0x0078EDC0 (:1284)
void mission_unlock_from_liberation()
{
    mission_info* info = mission_info_get("Guns of Tharsis");
    if (info == nullptr)
    {
        keen::debug::nativePrintArgs("Int3 critical function at %s(%d)\n", "mission_info.cpp", 1284);
        return;
    }
    if (!info->mission_flags.unlocked)
    {
        player_meta_data* meta = player_get_meta_data();
        if (meta != nullptr && meta->get_upgrade_value(UPGRADE_LIBERATED_A) != 0 && meta->get_upgrade_value(UPGRADE_LIBERATED_B) != 0)
        {
            info->mission_flags.unlocked = true;
            object_mission_start_node_update_markers();
        }
    }
}

// 0x0075EC10 (:1306)
void mission_skip_tutorial()
{
    Skip_tutorial = true;
}
