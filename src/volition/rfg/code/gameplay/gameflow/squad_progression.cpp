// Original: volition/rfg/code/gameplay/gameflow/squad_progression.cpp
//
// squad_progression.xtbl (stub_impl.md S9): per squad type (primary and reinforcement EDF, guerrilla, marauder) eight
// levels of weighted human and weapon sets, the level given by the tech level or EDF progress. Reserves the resource slots
// for squad members and vehicles and picks from what has loaded.

#include "volition/rfg/code/gameplay/gameflow/squad_progression.h"
#include "volition/rfg/code/gameplay/gameflow/squad_progression_internal.h"

#include "volition/rfg/code/gameplay/gameflow/gfm_tech_level.h"
#include "volition/rfg/code/gameplay/gameflow/squad_loadout.h"
#include "volition/rfg/code/gameplay/missions.h"
#include "volition/rfg/code/gamewide/stats.h"
#include "volition/rfg/code/level/world.h"
#include "volition/rfg/code/objects/humans/human_info.h"
#include "volition/rfg/code/objects/humans/human_teams.h"
#include "volition/rfg/code/objects/humans/player.h"
#include "volition/rfg/code/objects/object_squad.h"
#include "volition/rfg/code/spawn/ambient_spawn_manager.h"
#include "volition/rfg/code/spawn/spawn_object_group.h"
#include "volition/rfg/code/spawn/spawn_resource_manager.h"
#include "volition/rfg/code/vehicle/vehicle_info.h"
#include "volition/rfg/code/vehicle/vehicle_interaction_info.h"
#include "volition/vlib/code/math/rand.h"
#include "volition/vlib/code/parse/xml_table_tools.h"
#include "volition/vlib/code/util/fastxml.h"
#include "volition/vlib/code/util/string_comp.h"

#include <stdio.h>

// Magic number: the levels of a progression.
static const int NUM_PROGRESSION_LEVELS = 8;
// Magic number: a squad's vehicle should seat at least four.
static const int SQUAD_VEHICLE_MIN_SEATS = 3;

// ---- public globals (gameplay.cpp defines Guerrilla_progression)
extern squad_progress_info Guerrilla_progression; // 0x0224CAE0
extern mission_handle Mission_active_index; // 0x0165150C (gameplay.cpp)
int EDF_locked_mission_progression_level; // 0x02229650
int Marauder_progression_level; // 0x02229654

// ---- file-static globals
static const char* const XML_SQUAD_PROGRES_LEVEL_TAGS[NUM_PROGRESSION_LEVELS] = {"squad_level_1", "squad_level_2", "squad_level_3", "squad_level_4", "squad_level_5", "squad_level_6", "squad_level_7", "squad_level_8"}; // 0x01651460
static const spawn_slot_types HUMAN_SLOT_TYPES[NUM_HUMAN_TEAMS] = {SPAWN_SLOT_HUMAN_GUERRILLA, SPAWN_SLOT_HUMAN_EDF, SPAWN_SLOT_HUMAN_GUERRILLA, SPAWN_SLOT_HUMAN_MARAUDER}; // 0x012DC954
static const spawn_slot_types SQUAD_VEHICLE_SLOT_TYPES[NUM_HUMAN_TEAMS] = {SPAWN_SLOT_VEHICLE_GUERRILLA, SPAWN_SLOT_VEHICLE_EDF_APC, SPAWN_SLOT_VEHICLE_GUERRILLA, SPAWN_SLOT_VEHICLE_MULTI_PURPOSE}; // 0x012DC964
static const spawn_slot_types MARAUDER_SQUAD_VEHICLE_SLOT_TYPES[NUM_HUMAN_TEAMS] = {SPAWN_SLOT_NONE, SPAWN_SLOT_VEHICLE_EDF_APC, SPAWN_SLOT_NONE, SPAWN_SLOT_VEHICLE_GUERRILLA}; // 0x012DC974
static squad_progress_info EDF_primary_progression; // 0x0224F868
static squad_progress_info EDF_reinforcement_progression; // 0x0226C100
static squad_progress_info Marauder_progression; // 0x0226E8D0

// 0x0078C500 (:95)
// LTCG: group_b in EAX. Whether a vehicle info is in both.
static bool squad_vehicle_groups_overlap(const vehicle_spawn_group_info* group_a, const vehicle_spawn_group_info* group_b)
{
    for (int i = 0; i < group_b->v_info_list.size(); i++)
    {
        if (group_a->v_info_list.find(group_b->v_info_list[i], false) > -1)
        {
            return true;
        }
    }
    return false;
}

// 0x0078C550 (:112)
static bool squad_vehicle_slot_in_use(human_teams team)
{
    const vehicle_spawn_group_info* group = sog_default_vehicle_spawn_group(team);
    if (group == nullptr)
    {
        return false;
    }
    const int count = World.get_num_objects<object_squad>();
    for (int i = 0; i < count; i++)
    {
        object_squad* squad = World.get_object<object_squad>(i);
        const vehicle_spawn_group_info* squad_group = squad->get_veh_spawn_group();
        if (team == squad->get_squad_team() && squad_group != nullptr && (squad_group == group || squad_vehicle_groups_overlap(group, squad_group)))
        {
            return true;
        }
    }
    return false;
}

// 0x0078C5E0 (:143)
// LTCG: slot_type in ESI.
static void squad_release_unused_vehicle_slots(human_teams team, spawn_slot_types slot_type)
{
    if (srm_get_owner(slot_type) == RESOURCE_SLOT_USER_ID_SQUAD && !squad_vehicle_slot_in_use(team))
    {
        srm_release_all_slots(slot_type, RESOURCE_SLOT_USER_ID_SQUAD);
    }
}

// 0x007A54F0 (:257)
// The marauder group in random order is the desired list.
static bool squad_member_marauder(spawn_slot_types slot_type, squad_progression_set& member_info, squad_progress_info& general_progression_info)
{
    const human_spawn_group_info* group = sog_default_human_spawn_group(HUMAN_TEAM_MARAUDER);
    if (group == nullptr)
    {
        return false;
    }
    farray<human_info const*, 256> remaining;
    farray<human_info const*, 256> shuffled;
    const int count = srm_get_availible_slots(slot_type, RESOURCE_SLOT_USER_ID_SQUAD);
    for (int i = 0; i < group->h_info_list.size(); i++)
    {
        remaining.add(group->h_info_list[i]);
    }
    while (remaining.size() > 0)
    {
        const int pick = remaining.size() == 1 ? 0 : int_rand_range(0, remaining.size() - 1);
        shuffled.add(remaining[pick]);
        remaining.remove_unsorted(pick);
    }
    general_progression_info.make_squad_reservations(count, slot_type, shuffled);
    return general_progression_info.get_squad_member_info(member_info, shuffled);
}

// 0x007A5630 (:298)
// LTCG: slot_type in ESI, general_progression_info in EDI.
static bool squad_member_common(spawn_slot_types slot_type, squad_progression_set& member_info, squad_progress_info& general_progression_info)
{
    const int count = srm_get_availible_slots(slot_type, RESOURCE_SLOT_USER_ID_SQUAD);
    if (count > 0)
    {
        general_progression_info.make_squad_reservations(count, slot_type);
    }
    return general_progression_info.get_squad_member_info(member_info);
}

// 0x0076A150 (squad_progression.h:16)
squad_progression_set::squad_progression_set()
{
    reset();
}

// 0x0075E480 (:345)
void squad_progression_set::reset()
{
    h_info = nullptr;
    member_load = -1;
    spawn_weight = 0;
}

// 0x007740E0 (:352)
bool squad_progression_set::load(xml_element* xml_root)
{
    xml_element* element = xml_find(xml_root, "human_type");
    if (element == nullptr)
    {
        return false;
    }
    h_info = human_info_find(element->text);
    if (h_info == nullptr)
    {
        return false;
    }
    element = xml_find(xml_root, "weapon_type");
    if (element == nullptr)
    {
        return false;
    }
    member_load = squad_loadout_find(element->text);
    if (member_load == -1)
    {
        return false;
    }
    element = xml_find(xml_root, "spawn_weight");
    if (element == nullptr)
    {
        return false;
    }
    spawn_weight = xml_get_int(element, 0);
    return spawn_weight > 0;
}

// 0x0078C490 (.h:27)
squad_progression_level::squad_progression_level()
{
    reset();
}

// 0x0078C4D0
squad_progression_level::~squad_progression_level()
{
}

// 0x00774170 (:381)
void squad_progression_level::reset()
{
    squad_progression_list.clear();
    total_spawn_weight = 0;
    for (int i = 0; i < 8; i++)
    {
        priority_reservations[i] = nullptr;
    }
}

// 0x007741A0 (:393)
int squad_progression_level::get_weight_for_human(unsigned int srid) const
{
    int weight = 0;
    for (int i = 0; i < squad_progression_list.size(); i++)
    {
        if (srid == squad_progression_list[i].h_info->srid)
        {
            weight += squad_progression_list[i].spawn_weight;
        }
    }
    return weight;
}

// 0x007741D0 (:408)
const human_info* squad_progression_level::find_h_info(unsigned int srid)
{
    for (int i = 0; i < squad_progression_list.size(); i++)
    {
        if (srid == squad_progression_list[i].h_info->srid)
        {
            return squad_progression_list[i].h_info;
        }
    }
    return nullptr;
}

// 0x0078C610 (:423)
// The priority reservations are the level's humans, heaviest total weight first.
bool squad_progression_level::load(xml_element* xml_root)
{
    reset();
    xml_element* properties = xml_find(xml_root, "progression_properties");
    if (properties == nullptr)
    {
        return false;
    }
    squad_progression_set set;
    for (xml_element* element = xml_find(properties, "squad_property"); element != nullptr; element = xml_find_next(properties, element, "squad_property"))
    {
        if (set.load(element))
        {
            squad_progression_list.add(set);
        }
    }
    if (squad_progression_list.size() < 1)
    {
        return false;
    }
    total_spawn_weight = 0;
    for (int i = 0; i < squad_progression_list.size(); i++)
    {
        total_spawn_weight += squad_progression_list[i].spawn_weight;
    }
    if (total_spawn_weight < 1)
    {
        return false;
    }
    farray<unsigned int, 8> srids;
    for (int i = 0; i < squad_progression_list.size(); i++)
    {
        srids.add_unique(squad_progression_list[i].h_info->srid);
    }
    for (int i = 0; i < srids.size(); i++)
    {
        for (int k = i + 1; k < srids.size(); k++)
        {
            if (get_weight_for_human(srids[i]) < get_weight_for_human(srids[k]))
            {
                const unsigned int swap = srids[i];
                srids[i] = srids[k];
                srids[k] = swap;
            }
        }
    }
    for (int i = 0; i < srids.size(); i++)
    {
        priority_reservations[i] = find_h_info(srids[i]);
        if (priority_reservations[i] == nullptr)
        {
            return false;
        }
    }
    for (int i = srids.size(); i < 8; i++)
    {
        priority_reservations[i] = nullptr;
    }
    return true;
}

// 0x0078C850 (:491)
// Original: the priority reservations stay clear.
void squad_progression_level::copy_from(const squad_progression_level& other_progression_info)
{
    reset();
    for (int i = 0; i < other_progression_info.squad_progression_list.size(); i++)
    {
        squad_progression_list.add(other_progression_info.squad_progression_list[i]);
    }
    total_spawn_weight = other_progression_info.total_spawn_weight;
}

// 0x0079D150 (.h:73)
squad_progress_info::squad_progress_info()
{
    reset();
}

// 0x0078C4E0
squad_progress_info::~squad_progress_info()
{
}

// 0x00774210 (:505)
void squad_progress_info::reset()
{
    name = nullptr;
    team = HUMAN_TEAM_NONE;
    last_member_used = 0;
    for (int i = 0; i < NUM_PROGRESSION_LEVELS; i++)
    {
        progression_list[i].reset();
    }
}

// 0x0078C900 (:519)
// A level the table leaves out, or that fails, takes the previous level's element.
bool squad_progress_info::load(xml_element* xml_root, const char* new_name)
{
    if (new_name == nullptr)
    {
        return false;
    }
    reset();
    name = new_name;
    xml_element* team_element = xml_find(xml_root, "team");
    if (team_element == nullptr)
    {
        return false;
    }
    team = human_team_from_name(team_element->text);
    xml_element* previous = xml_find(xml_root, XML_SQUAD_PROGRES_LEVEL_TAGS[0]);
    if (previous == nullptr)
    {
        return false;
    }
    if (!progression_list[0].load(previous))
    {
        keen::debug::nativePrintArgs("Int3 critical function at %s(%d)\n", __FILE__, __LINE__);
        return false;
    }
    for (int i = 1; i < NUM_PROGRESSION_LEVELS; i++)
    {
        xml_element* element = xml_find(xml_root, XML_SQUAD_PROGRES_LEVEL_TAGS[i]);
        if (element != nullptr)
        {
            if (progression_list[i].load(element))
            {
                previous = element;
                continue;
            }
            keen::debug::nativePrintArgs("Int3 critical function at %s(%d)\n", __FILE__, __LINE__);
        }
        if (!progression_list[i].load(previous))
        {
            return false;
        }
    }
    return true;
}

// 0x0078CA00 (:576)
// A member whose human is reserved and loaded, by weight; else the level below.
bool squad_progress_info::get_squad_member_info(squad_progression_set& member_info, int progression_level) const
{
    if (progression_level < 0 || progression_level >= NUM_PROGRESSION_LEVELS)
    {
        return false;
    }
    farray<squad_progression_set, 8> loaded;
    int total_weight = 0;
    const base_array<squad_progression_set>& list = progression_list[progression_level].squad_progression_list;
    for (int i = 0; i < list.size(); i++)
    {
        if (srm_human_info_reserved(list[i].h_info) && srm_human_info_loaded(list[i].h_info))
        {
            loaded.add(list[i]);
            total_weight += list[i].spawn_weight;
        }
    }
    if (loaded.size() > 0)
    {
        const int roll = total_weight < 2 ? 0 : int_rand_range(0, total_weight - 1);
        int running = 0;
        for (int i = 0; i < loaded.size(); i++)
        {
            running += loaded[i].spawn_weight;
            if (roll < running)
            {
                member_info = loaded[i];
                return true;
            }
        }
        keen::debug::nativePrintArgs("Int3 critical function at %s(%d)\n", __FILE__, __LINE__);
        member_info = loaded[int_rand_range(0, loaded.size() - 1)];
        return true;
    }
    return progression_level != 0 && get_squad_member_info(member_info, progression_level - 1);
}

// 0x0079D1D0 (:639)
bool squad_progress_info::get_squad_member_info(squad_progression_set& member_info) const
{
    for (int level = get_progression_level(); level > -1 && level < NUM_PROGRESSION_LEVELS; level++)
    {
        if (get_squad_member_info(member_info, level))
        {
            return true;
        }
    }
    return false;
}

// 0x0078CB90 (:665)
// The next human of the master list after the last one used that is desired, reserved and loaded.
const human_info* squad_progress_info::select_human_info(const base_array<human_info const*>& desired_npc_list) const
{
    const farray<human_info, 165>& list = human_info_get_master_list();
    int& last = const_cast<squad_progress_info*>(this)->last_member_used;
    for (int i = 0; i < list.size(); i++)
    {
        last++;
        if (last >= list.size())
        {
            last = 0;
        }
        else if (last < 0)
        {
            keen::debug::nativePrintArgs("Int3 critical function at %s(%d)\n", __FILE__, __LINE__);
            last = 0;
        }
        const human_info* h_info = &list[last];
        if (desired_npc_list.find(h_info, false) > -1 && srm_human_info_reserved(h_info) && srm_human_info_loaded(h_info))
        {
            return h_info;
        }
    }
    return nullptr;
}

// 0x0079D230 (:706)
// The weapon set by weight, the human from the desired list.
bool squad_progress_info::get_squad_member_info(squad_progression_set& member_info, const base_array<human_info const*>& desired_npc_list) const
{
    const int level = get_progression_level();
    if (level < 0 || level >= NUM_PROGRESSION_LEVELS || desired_npc_list.size() < 1)
    {
        return false;
    }
    const human_info* h_info = select_human_info(desired_npc_list);
    if (h_info == nullptr)
    {
        return false;
    }
    const squad_progression_level& info = progression_list[level];
    const int roll = info.total_spawn_weight < 2 ? 0 : int_rand_range(0, info.total_spawn_weight - 1);
    int running = 0;
    const squad_progression_set* chosen = nullptr;
    for (int i = 0; i < info.squad_progression_list.size(); i++)
    {
        running += info.squad_progression_list[i].spawn_weight;
        if (roll < running)
        {
            chosen = &info.squad_progression_list[i];
            break;
        }
    }
    if (chosen == nullptr)
    {
        keen::debug::nativePrintArgs("Int3 critical function at %s(%d)\n", __FILE__, __LINE__);
        chosen = &info.squad_progression_list[int_rand_range(0, info.squad_progression_list.size() - 1)];
    }
    member_info = *chosen;
    member_info.h_info = h_info;
    return true;
}

// 0x0079D320 (:755)
bool squad_progress_info::make_squad_reservations(spawn_slot_types slot_type) const
{
    const int level = get_progression_level();
    if (level < 0 || level >= NUM_PROGRESSION_LEVELS)
    {
        return false;
    }
    for (int i = 0; i < 8; i++)
    {
        const human_info* h_info = progression_list[level].priority_reservations[i];
        if (h_info != nullptr && !srm_slot_is_reserved(slot_type, RESOURCE_SLOT_USER_ID_SQUAD, h_info) && srm_reserve_slot(slot_type, RESOURCE_SLOT_USER_ID_SQUAD, h_info))
        {
            return true;
        }
    }
    return false;
}

// 0x0079D3A0 (:789)
void squad_progress_info::make_squad_reservations(int count, spawn_slot_types slot_type) const
{
    for (; count > 0; count--)
    {
        make_squad_reservations(slot_type);
    }
}

// 0x00774250 (:850)
void squad_progress_info::make_squad_reservations(int count, spawn_slot_types slot_type, const base_array<human_info const*>& desired_npc_list) const
{
    if (count < 1)
    {
        return;
    }
    int reserved = 0;
    for (int i = 0; i < desired_npc_list.size(); i++)
    {
        const human_info* h_info = desired_npc_list[i];
        if (!srm_slot_is_reserved(slot_type, RESOURCE_SLOT_USER_ID_SQUAD, h_info) && srm_reserve_slot(slot_type, RESOURCE_SLOT_USER_ID_SQUAD, h_info))
        {
            reserved++;
            if (count <= reserved)
            {
                return;
            }
        }
    }
}

// 0x0078CC70 (:881)
int squad_progress_info::get_progression_level() const
{
    switch (team)
    {
    case HUMAN_TEAM_GUERILLA:
    case HUMAN_TEAM_CIVILIAN:
        return (int)(stats_cheat_enabled(CHEATS_MAX_TECHNOLOGY) ? World.get_tech_level_max() : World.get_tech_level());
    case HUMAN_TEAM_EDF:
        if (stats_cheat_enabled(CHEATS_MAX_TECHNOLOGY))
        {
            return 7;
        }
        return Mission_active_index == INVALID_MISSION_HANDLE ? edf_progression_level_internal() : EDF_locked_mission_progression_level;
    case HUMAN_TEAM_MARAUDER:
        return stats_cheat_enabled(CHEATS_MAX_TECHNOLOGY) ? 7 : Marauder_progression_level;
    default:
        return 0;
    }
}

// 0x0078CD10 (:911)
void squad_progression_system_init(int dlc_id)
{
    const bool dlc = dlc_id != -1;
    xml_element* root;
    if (dlc)
    {
        char file_name[256];
        sprintf(file_name, "dlc%02d_%s", dlc_id, "squad_progression.xtbl");
        root = xml_table_parse(file_name);
    }
    else
    {
        root = xml_table_parse("squad_progression.xtbl");
        if (root == nullptr)
        {
            return;
        }
        EDF_primary_progression.reset();
        EDF_reinforcement_progression.reset();
        Guerrilla_progression.reset();
        Marauder_progression.reset();
    }
    // Original: a DLC table that does not parse goes on with no list.
    for (xml_element* element = xml_find(root, "squad_progression"); element != nullptr; element = xml_find_next(root, element, "squad_progression"))
    {
        xml_element* name = xml_find(element, "Name");
        if (name == nullptr)
        {
            continue;
        }
        const char* new_name;
        squad_progress_info* info;
        if (v_stricmp(name->text, "Primary EDF") == 0)
        {
            new_name = "Primary EDF";
            info = &EDF_primary_progression;
        }
        else if (v_stricmp(name->text, "Reinforcement EDF") == 0)
        {
            new_name = "Reinforcement EDF";
            info = &EDF_reinforcement_progression;
        }
        else if (v_stricmp(name->text, "Guerrilla") == 0)
        {
            new_name = "Guerrilla";
            info = &Guerrilla_progression;
        }
        else if (v_stricmp(name->text, "Marauder") == 0)
        {
            new_name = "Marauder";
            info = &Marauder_progression;
        }
        else
        {
            continue;
        }
        if (dlc)
        {
            info->reset();
        }
        if (!info->load(element, new_name))
        {
            keen::debug::nativePrintArgs("Critical error, unable to load one of the squad progression tables.");
        }
    }
    xml_table_close(root);
}

// 0x0079D410 (:989)
// LTCG: squad_def in EAX, team in ECX, member_info in ESI.
static bool mission_member_human_info(squad_progression_set& member_info, human_teams team, const squad_definition* squad_def, bool first_member)
{
    member_info.reset();
    if (squad_def != nullptr && first_member && squad_def->special_npc_info != nullptr)
    {
        member_info.h_info = squad_def->special_npc_info;
        return true;
    }
    if (team == HUMAN_TEAM_EDF)
    {
        return EDF_primary_progression.get_squad_member_info(member_info);
    }
    if (team == HUMAN_TEAM_GUERILLA || team == HUMAN_TEAM_CIVILIAN)
    {
        return Guerrilla_progression.get_squad_member_info(member_info, sog_default_human_spawn_group(HUMAN_TEAM_GUERILLA)->h_info_list);
    }
    if (team == HUMAN_TEAM_MARAUDER)
    {
        return Marauder_progression.get_squad_member_info(member_info);
    }
    keen::debug::nativePrintArgs("Int3 critical function at %s(%d)\n", __FILE__, __LINE__);
    return false;
}

// 0x007A5660 (:1029)
bool squad_progression_member_human_info(squad_progression_set& member_info, human_teams team, const squad_definition* squad_def, bool first_member, bool reinforcement)
{
    member_info.reset();
    if (Mission_active_index != INVALID_MISSION_HANDLE)
    {
        return mission_member_human_info(member_info, team, squad_def, first_member);
    }
    srm_free_unused_reserved_slots(SPAWN_SLOT_HUMAN_GUERRILLA, RESOURCE_SLOT_USER_ID_SQUAD);
    srm_free_unused_reserved_slots(SPAWN_SLOT_HUMAN_EDF, RESOURCE_SLOT_USER_ID_SQUAD);
    srm_free_unused_reserved_slots(SPAWN_SLOT_HUMAN_MARAUDER, RESOURCE_SLOT_USER_ID_SQUAD);
    if (squad_def != nullptr && first_member && squad_def->special_npc_info != nullptr)
    {
        member_info.h_info = squad_def->special_npc_info;
        return true;
    }
    if ((int)team < 0 || (int)team >= NUM_HUMAN_TEAMS || HUMAN_SLOT_TYPES[team] == SPAWN_SLOT_NONE)
    {
        return false;
    }
    const spawn_slot_types slot_type = HUMAN_SLOT_TYPES[team];
    if (team == HUMAN_TEAM_EDF)
    {
        return squad_member_common(slot_type, member_info, reinforcement ? EDF_reinforcement_progression : EDF_primary_progression);
    }
    if (team == HUMAN_TEAM_GUERILLA || team == HUMAN_TEAM_CIVILIAN)
    {
        const int count = srm_get_availible_slots(slot_type, RESOURCE_SLOT_USER_ID_SQUAD);
        const farray<human_info const*, 16>& desired = amb_spawn_desired_guerrilla_list();
        Guerrilla_progression.make_squad_reservations(count, slot_type, desired);
        return Guerrilla_progression.get_squad_member_info(member_info, desired);
    }
    if (team == HUMAN_TEAM_MARAUDER)
    {
        return squad_member_marauder(slot_type, member_info, Marauder_progression);
    }
    keen::debug::nativePrintArgs("Int3 critical function at %s(%d)\n", __FILE__, __LINE__);
    return false;
}

// 0x007742C0 (:973)
void squad_progression_level_init()
{
    extern farray<human_info const*, 8> Common_guerrilla_list; // 0x01657800 (gameplay.cpp)
    amb_spawn_info_build_common_npc_list(Common_guerrilla_list);
    Guerrilla_progression.last_member_used = int_rand_range(0, human_info_get_master_list().size() - 1);
}

// 0x0079D4A0 (:1087)
void squad_progression_reserve_members(spawn_slot_types slot_type, human_teams team, int count)
{
    srm_free_unused_reserved_slots(SPAWN_SLOT_HUMAN_GUERRILLA, RESOURCE_SLOT_USER_ID_SQUAD);
    srm_free_unused_reserved_slots(SPAWN_SLOT_HUMAN_EDF, RESOURCE_SLOT_USER_ID_SQUAD);
    srm_free_unused_reserved_slots(SPAWN_SLOT_HUMAN_MARAUDER, RESOURCE_SLOT_USER_ID_SQUAD);
    if (count < 1)
    {
        return;
    }
    if (team == HUMAN_TEAM_EDF)
    {
        EDF_primary_progression.make_squad_reservations(count, slot_type);
    }
    else if (team == HUMAN_TEAM_CIVILIAN || team == HUMAN_TEAM_GUERILLA)
    {
        Guerrilla_progression.make_squad_reservations(count, slot_type, amb_spawn_desired_guerrilla_list());
    }
    else if (team == HUMAN_TEAM_MARAUDER)
    {
        Marauder_progression.make_squad_reservations(count, slot_type);
    }
    else
    {
        keen::debug::nativePrintArgs("Int3 critical function at %s(%d)\n", __FILE__, __LINE__);
    }
}

// 0x007742F0 (:1125)
static bool spawn_groups_overlap(const vehicle_spawn_group_info* group_1, const vehicle_spawn_group_info* group_2)
{
    for (int i = 0; i < group_1->v_info_list.size(); i++)
    {
        for (int k = 0; k < group_2->v_info_list.size(); k++)
        {
            if (group_1->v_info_list[i]->srid == group_2->v_info_list[k]->srid)
            {
                return true;
            }
        }
    }
    return false;
}

// 0x0078CEE0 (:1141)
// Prefers the vehicles with room for a squad.
static void srm_reserve_guerrilla_vehicle_slot(spawn_slot_types slot_type, const vehicle_spawn_group_info* default_group)
{
    farray<vehicle_info const*, 256> roomy;
    for (int i = 0; i < default_group->v_info_list.size(); i++)
    {
        const vehicle_interaction_info* vi_info = default_group->v_info_list[i]->vi_info;
        if (vi_info != nullptr && vi_info->num_seats > SQUAD_VEHICLE_MIN_SEATS)
        {
            roomy.add(default_group->v_info_list[i]);
        }
    }
    if (roomy.size() > 0)
    {
        srm_reserve_slot(slot_type, RESOURCE_SLOT_USER_ID_SQUAD, roomy);
    }
    else
    {
        srm_reserve_slot(slot_type, RESOURCE_SLOT_USER_ID_SQUAD, default_group->v_info_list);
    }
}

// 0x0078CFB0 (:1172)
static const vehicle_info* pick_squad_vehicle_info(const base_array<vehicle_info const*>& vehicle_info_list)
{
    farray<vehicle_info const*, 256> roomy;
    for (int i = 0; i < vehicle_info_list.size(); i++)
    {
        const vehicle_interaction_info* vi_info = vehicle_info_list[i]->vi_info;
        if (vi_info != nullptr && vi_info->num_seats > SQUAD_VEHICLE_MIN_SEATS)
        {
            roomy.add(vehicle_info_list[i]);
        }
    }
    if (roomy.size() > 0)
    {
        const vehicle_info* info = srm_pick_loaded_vehicle_info(roomy);
        if (info != nullptr)
        {
            return info;
        }
    }
    return srm_pick_loaded_vehicle_info(vehicle_info_list);
}

// 0x0078D060 (:1210)
const vehicle_info* squad_progression_member_vehicle_info(human_teams team, const vehicle_spawn_group_info* veh_spawn_group)
{
    if (Mission_active_index != INVALID_MISSION_HANDLE)
    {
        return srm_pick_loaded_vehicle_info(veh_spawn_group);
    }
    squad_release_unused_vehicle_slots(HUMAN_TEAM_EDF, SPAWN_SLOT_VEHICLE_EDF_APC);
    squad_release_unused_vehicle_slots(HUMAN_TEAM_MARAUDER, SPAWN_SLOT_VEHICLE_MULTI_PURPOSE);
    // Original: the guerrilla check also looks at the EDF slot.
    squad_release_unused_vehicle_slots(HUMAN_TEAM_GUERILLA, SPAWN_SLOT_VEHICLE_EDF_APC);
    const vehicle_spawn_group_info* group = sog_default_vehicle_spawn_group(team);
    if (group == nullptr)
    {
        return nullptr;
    }
    if ((int)team > -1 && (int)team < NUM_HUMAN_TEAMS)
    {
        player* pp = player_get_local_player();
        const spawn_slot_types slot_type = pp == nullptr || pp->current_team != HUMAN_TEAM_MARAUDER ? SQUAD_VEHICLE_SLOT_TYPES[team] : MARAUDER_SQUAD_VEHICLE_SLOT_TYPES[team];
        if (slot_type != SPAWN_SLOT_NONE && spawn_groups_overlap(group, veh_spawn_group) && srm_get_availible_slots(slot_type, RESOURCE_SLOT_USER_ID_SQUAD) > 0)
        {
            if (team != HUMAN_TEAM_GUERILLA && team != HUMAN_TEAM_CIVILIAN)
            {
                srm_reserve_slot(slot_type, RESOURCE_SLOT_USER_ID_SQUAD, group);
            }
            else
            {
                srm_reserve_guerrilla_vehicle_slot(slot_type, group);
            }
        }
    }
    return pick_squad_vehicle_info(veh_spawn_group->v_info_list);
}
