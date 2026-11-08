// Original: volition/rfg/code/gameplay/activities/raid_common.cpp

#include "volition/rfg/code/gameplay/activities/raid_common.h"
#include "volition/rfg/code/gameplay/activities/raid_common_internal.h"

#include "volition/rfg/code/code_effects/explosion_shared.h"
#include "volition/rfg/code/gameplay/activities/activity.h"
#include "volition/rfg/code/gameplay/activities/activity_raid.h"
#include "volition/rfg/code/gameplay/activities/base_activity_data.h"
#include "volition/rfg/code/geom/vanim.h"
#include "volition/rfg/code/hud/hud_ui_mission_briefing.h"
#include "volition/rfg/code/level/world.h"
#include "volition/rfg/code/objects/humans/human.h"
#include "volition/rfg/code/objects/humans/human_teams.h"
#include "volition/rfg/code/objects/humans/npc.h"
#include "volition/rfg/code/objects/object_navpoint.h"
#include "volition/rfg/code/objects/object_path_road.h"
#include "volition/rfg/code/objects/object_squad.h"
#include "volition/rfg/code/pathfinding/bb_pathfinding.h"
#include "volition/rfg/code/pathfinding/nav_cell.h"
#include "volition/rfg/code/rfg.h"
#include "volition/rfg/code/spawn/spawn_object_group.h"
#include "volition/vlib/code/math/ix.h"
#include "volition/vlib/code/math/min_max.h"
#include "volition/vlib/code/memory/object_pool.h"
#include "volition/vlib/code/memory/rfg_split_mempool.h"
#include "volition/vlib/code/memory/static_mempool.h"
#include "volition/vlib/code/parse/xml_table_tools.h"
#include "volition/vlib/code/util/fastxml.h"
#include "volition/vlib/code/util/string_pool.h"
#include "volition/vlib/code/util/string_util.h"

#include <stdio.h>

// Magic number: flag 8 of a nav cell shuts it out of the raid's searches.
static const unsigned int NAV_CELL_RAID_EXCLUDED = 8;

static object_pool<base_raid_definition_data, 64, 1> Raid_def_pool; // 0x02266280
static farray<base_raid_definition_data*, 5> Dlc_raid_data_list; // 0x016573AC
static string_pool Raid_string_pool("Raid Sting Pool"); // 0x0224CFC0, the original's spelling
static char* Raid_string_pool_data; // 0x02229604
static const char* RAID_NAME_TO_STATE[NUM_RAID_TEAM_STATES] = {nullptr, "Withdraw", "Standard", "Attack", nullptr}; // 0x0165139C

// 0x0075BA50
raid_exit_cell::raid_exit_cell()
{
}

// 0x0075B8C0
splined_search_data::splined_search_data()
{
}

// 0x0075B6A0 (:151). Original: leaves the members unset.
road_evaluation_set::road_evaluation_set()
{
}

// 0x0075B6B0 (:160)
void road_evaluation_set::reset()
{
    prp = nullptr;
    road_distance = 3.4028235e+38f;
    nearest_point.zero();
    min_dist = 0.0f;
    at_min_point = true;
    at_max_point = true;
    max_dist = 3.4028235e+38f;
    min_point.zero();
    max_point.zero();
}

// 0x00788F20 (:212). rfg.exe: LTCG, raid_name in EDI. A slot of that name, or a new one.
static base_raid_definition_data* find_raid_definition_slot(const char* raid_name)
{
    for (base_raid_definition_data* data = Raid_def_pool.get_first(0); data != nullptr; data = Raid_def_pool.get_next(data))
    {
        if (v_stricmp(raid_name, data->raid_name) == 0)
        {
            return data;
        }
    }
    return Raid_def_pool.alloc(0);
}

// 0x0079B140 (:227). Original: only the first call for the base table parses it. A later one walks a null root.
void load_raid_data(int dlc_id)
{
    const bool is_dlc = dlc_id != -1;
    xml_element* root = nullptr;
    if (is_dlc)
    {
        char filename[256];
        Raid_string_pool.set_thread_ownership();
        Raid_string_pool.mark();
        sprintf(filename, "dlc%02d_%s", dlc_id, "raid_data.xtbl");
        root = xml_table_parse(filename);
    }
    else if (Raid_string_pool_data == nullptr)
    {
        Raid_string_pool_data = static_cast<char*>(Permanent_mempool.alloc(0x400, 0)); // Magic number: 1 KB of names
        if (Raid_string_pool_data == nullptr)
        {
            keen::debug::nativePrintArgs("Insufficient memory to allocate the raid string table");
        }
        Raid_string_pool.init(Raid_string_pool_data, 0x400, "Raid Sting Pool", 0, 0);
        Raid_string_pool.clear();
        root = xml_table_parse("raid_data.xtbl");
    }
    for (xml_element* entry = xml_find(root, "RaidData"); entry != nullptr; entry = xml_find_next(root, entry, "RaidData"))
    {
        xml_element* name = xml_find(entry, "Name");
        if (name == nullptr)
        {
            continue;
        }
        base_raid_definition_data* data = find_raid_definition_slot(name->text);
        if (data == nullptr)
        {
            break;
        }
        data->reset();
        if (!data->load(entry))
        {
            Raid_def_pool.free(data);
        }
        else if (is_dlc)
        {
            if (Dlc_raid_data_list.size() < Dlc_raid_data_list.capacity())
            {
                Dlc_raid_data_list.add(data);
            }
            else
            {
                keen::debug::nativePrintArgs("Int3 critical function at %s(%d)\n", __FILE__, __LINE__);
            }
        }
    }
    xml_table_close(root);
}

// 0x0079B2F0 (:290)
void raid_data_unload_dlc()
{
    for (int i = 0; i < Dlc_raid_data_list.size(); i++)
    {
        Raid_def_pool.free(Dlc_raid_data_list[i]);
    }
    Dlc_raid_data_list.clear();
    Raid_string_pool.set_thread_ownership();
    Raid_string_pool.restore_to_mark(0);
}

// 0x0075B700 (:359). Halves the road's range, up to 16 times, for the point at least 6 metres from the last one.
static bool find_next_destination_point(const splined_search_data& search_data, float min_search_dist, float max_search_dist, float& result_dist, vector& result_point, int depth)
{
    if ((double)depth >= 16.0 || max_search_dist - min_search_dist <= 0.1f)
    {
        return false;
    }
    const float u = (min_search_dist + max_search_dist) * 0.5f;
    vector point;
    search_data.prp->get_point(point, u);
    if (vec_dist_squared(point, search_data.region_center) <= search_data.region_radius_sqrd)
    {
        if (vec_dist_squared(point, search_data.previous_point) >= 36.0f)
        {
            const bool found = search_data.search_down
                ? find_next_destination_point(search_data, min_search_dist, u, result_dist, result_point, depth + 1)
                : find_next_destination_point(search_data, u, max_search_dist, result_dist, result_point, depth + 1);
            if (!found)
            {
                result_dist = u;
                result_point = point;
            }
            return true;
        }
        if (search_data.search_down)
        {
            return find_next_destination_point(search_data, u, max_search_dist, result_dist, result_point, depth + 1);
        }
    }
    else if (!search_data.search_down)
    {
        return find_next_destination_point(search_data, u, max_search_dist, result_dist, result_point, depth + 1);
    }
    return find_next_destination_point(search_data, min_search_dist, u, result_dist, result_point, depth + 1);
}

// 0x00788FA0 (:424). rfg.exe: LTCG, search_data in EAX, current_set in EDI.
static bool add_new_point_high(farray<vector, 64>& external_dest_list, road_evaluation_set& current_set, splined_search_data& search_data)
{
    if (current_set.at_max_point)
    {
        return false;
    }
    search_data.search_down = true;
    search_data.previous_point = current_set.max_point;
    if (find_next_destination_point(search_data, current_set.max_dist, (float)(current_set.prp->spline_data.num_points - 1), current_set.max_dist, current_set.max_point, 0))
    {
        external_dest_list.add(current_set.max_point);
        return true;
    }
    current_set.at_max_point = true;
    return false;
}

// 0x00789020 (:448). rfg.exe: LTCG, search_data in EAX, current_set in EDI.
static bool add_new_point_low(farray<vector, 64>& external_dest_list, road_evaluation_set& current_set, splined_search_data& search_data)
{
    if (current_set.at_min_point)
    {
        return false;
    }
    search_data.search_down = false;
    search_data.previous_point = current_set.min_point;
    if (find_next_destination_point(search_data, 0.0f, current_set.min_dist, current_set.min_dist, current_set.min_point, 0))
    {
        external_dest_list.add(current_set.min_point);
        return true;
    }
    current_set.at_min_point = true;
    return false;
}

// 0x00789090 (:476). rfg.exe: LTCG, current_set in EDI.
static bool add_new_point(farray<vector, 64>& external_dest_list, road_evaluation_set& current_set, const vector& center, float radius_sqrd)
{
    splined_search_data search_data;
    search_data.previous_point.zero();
    search_data.region_center = center;
    search_data.prp = current_set.prp;
    search_data.region_radius_sqrd = radius_sqrd;
    search_data.search_down = false;
    const bool high = add_new_point_high(external_dest_list, current_set, search_data);
    if (high && external_dest_list.size() >= external_dest_list.capacity())
    {
        return true;
    }
    return add_new_point_low(external_dest_list, current_set, search_data) || high;
}

// 0x00789120 (:500). rfg.exe: LTCG, the list in ECX, center in EDX, radius in XMM0.
static void build_external_destination_list(farray<vector, 64>& external_dest_list, const vector& center, float radius)
{
    // Magic number: 322 roads, from the array's size in rfg.exe.
    farray<object_path_road const*, 322> roads;
    farray<road_evaluation_set, 322> sets;
    const float radius_sqrd = radius * radius;
    external_dest_list.clear();
    const vector region_min(center.x - radius, center.y - radius, center.z - radius);
    const vector region_max(center.x + radius, center.y + radius, center.z + radius);
    for (int i = 0; i < World.get_num_objects<object_path_road>(); i++)
    {
        const object_path_road* road = World.get_object<object_path_road>(i);
        vector bmin;
        vector bmax;
        road->get_bbox(bmin, bmax);
        if (ix_box_box_aligned(region_min, region_max, bmin, bmax))
        {
            if (roads.size() < roads.capacity())
            {
                roads.add(road);
            }
            else
            {
                keen::debug::nativePrintArgs("Int3 critical function at %s(%d)\n", __FILE__, __LINE__);
            }
        }
    }
    for (int i = 0; i < roads.size(); i++)
    {
        road_evaluation_set set;
        set.reset();
        set.prp = roads[i];
        set.road_distance = roads[i]->find_closest_point_on_road(center, set.nearest_point, false);
        if (vec_dist_squared(center, set.nearest_point) <= radius_sqrd)
        {
            if (sets.size() < sets.capacity())
            {
                sets.add(set);
            }
            else
            {
                keen::debug::nativePrintArgs("Int3 critical function at %s(%d)\n", __FILE__, __LINE__);
            }
        }
    }
    for (int i = 0; i < sets.size(); i++)
    {
        road_evaluation_set& set = sets[i];
        external_dest_list.add(set.nearest_point);
        if (external_dest_list.size() >= external_dest_list.capacity())
        {
            return;
        }
        set.at_min_point = set.road_distance <= 0.0f;
        set.at_max_point = (float)(set.prp->spline_data.num_points - 1) <= set.road_distance;
        set.min_dist = set.road_distance;
        set.max_dist = set.road_distance;
        set.max_point = set.nearest_point;
        set.min_point = set.nearest_point;
    }
    // Takes turns between the roads; a road with no point left drops out. Magic number: at most 64 turns.
    int index = 0;
    for (int turn = 0; turn < 64 && sets.size() > 0; turn++)
    {
        if (add_new_point(external_dest_list, sets[index], center, radius_sqrd)) // rfg.exe inlines it
        {
            index++;
            if (external_dest_list.size() >= external_dest_list.capacity())
            {
                return;
            }
        }
        else
        {
            sets.remove(index);
            if (sets.size() < 1)
            {
                return;
            }
        }
        if (index >= sets.size())
        {
            index = 0;
        }
    }
}

// 0x007897D0 (:598). Original: raid_radius is not read. Magic number: navpoint type 11 is the building's way in.
bool find_seed_nav_cells(const object* objp, float raid_radius, farray<nav_cell const*, 8>& nav_cell_seeds, const base_array<unsigned int>& raid_target_list)
{
    nav_cell_seeds.clear();
    for (int i = 0; i < raid_target_list.size(); i++)
    {
        const object* target = World.object_from_handle(raid_target_list[i]);
        if (target == nullptr)
        {
            continue;
        }
        const object* first = target->child_ptr;
        for (const object* child = first; child != nullptr; )
        {
            if (child->get_obj_type() == OT_NAVPOINT && static_cast<const navpoint*>(child)->type == 11)
            {
                const nav_cell* cell = bb_pf_get_nav_cell(child->pos, false, nullptr, false, NAV_CELL_TRAVERSAL_ENABLED, nullptr);
                if (cell != nullptr && (cell->flags & NAV_CELL_RAID_EXCLUDED) == 0)
                {
                    nav_cell_seeds.add_unique(cell);
                    if (nav_cell_seeds.size() >= nav_cell_seeds.capacity())
                    {
                        return true;
                    }
                }
            }
            child = child->child_next;
            if (child == first)
            {
                break;
            }
        }
    }
    if (nav_cell_seeds.size() >= 1)
    {
        return true;
    }
    nav_cell* start = bb_pf_get_nav_cell(objp->pos, false, nullptr, false, NAV_CELL_TRAVERSAL_ALL, nullptr);
    if (start == nullptr)
    {
        vector closest;
        if (!rfg_pf_find_closest_point_on_nav_mesh_using_collect(objp->pos, false, closest, start, 50.0f)) // Magic number: 50 metres
        {
            keen::debug::nativePrintArgs("Int3 critical function at %s(%d)\n", __FILE__, __LINE__);
            return false;
        }
        if (start == nullptr)
        {
            return false;
        }
    }
    // Widens from the start until it finds a cell that is not excluded.
    farray<nav_cell*, 32> open;
    open.add(start);
    for (int i = 0; i < open.size(); i++)
    {
        nav_cell* cell = open[i];
        if ((cell->flags & NAV_CELL_RAID_EXCLUDED) == 0)
        {
            nav_cell_seeds.add(cell);
            return true;
        }
        for (int a = 0; a < 3; a++)
        {
            nav_cell* adjacent = cell->adjacent_cells[a].to_pointer();
            if (adjacent != nullptr)
            {
                open.add_unique(adjacent);
            }
        }
    }
    return false;
}

// 0x007719C0 (:676). rfg.exe: LTCG, the list in ESI. Magic number: within five metres.
static bool is_near_destination_point(const vector& pos, const farray<vector, 64>& external_dest_list)
{
    for (int i = 0; i < external_dest_list.size(); i++)
    {
        if (vec_xz_dist_squared(pos, external_dest_list[i]) < 25.0f)
        {
            return true;
        }
    }
    return false;
}

// 0x007899F0 (:694). rfg.exe: LTCG, the list in EAX, raid_target_list in EDX, radius in XMM1.
static void build_default_destination_list(const object* raid_objp, farray<vector, 64>& external_dest_list, const vector& center, float radius, const base_array<unsigned int>& raid_target_list)
{
    farray<nav_cell const*, 512> open;
    farray<nav_cell const*, 8> seeds;
    external_dest_list.clear();
    if (!find_seed_nav_cells(raid_objp, radius, seeds, raid_target_list) || seeds.size() < 1)
    {
        return;
    }
    for (int i = 0; i < seeds.size(); i++)
    {
        open.add(seeds[i]);
    }
    for (int i = 0; i < open.size(); i++)
    {
        const nav_cell* cell = open[i];
        if (cell == nullptr)
        {
            keen::debug::nativePrintArgs("Int3 critical function at %s(%d)\n", __FILE__, __LINE__);
            continue;
        }
        if ((cell->flags & NAV_CELL_RAID_EXCLUDED) != 0)
        {
            continue;
        }
        vector centroid;
        nav_cell_get_centroid(&centroid, cell);
        if (!is_in_cylindrical_region(centroid, center, radius * radius))
        {
            continue;
        }
        if (!is_near_destination_point(centroid, external_dest_list))
        {
            external_dest_list.add(centroid);
            if (external_dest_list.size() >= external_dest_list.capacity())
            {
                return;
            }
        }
        if (open.size() < open.capacity())
        {
            for (int a = 0; a < 3; a++)
            {
                const nav_cell* adjacent = cell->adjacent_cells[a].to_pointer();
                if (adjacent != nullptr && (adjacent->flags & NAV_CELL_RAID_EXCLUDED) == 0)
                {
                    open.add_unique(adjacent);
                    if (open.size() >= open.capacity())
                    {
                        break;
                    }
                }
            }
        }
    }
}

// 0x00768DE0 (.h:59)
common_raid_squad_info::common_raid_squad_info()
{
    reset();
}

// 0x0075B8D0 (:778)
void common_raid_squad_info::reset()
{
    squad_def = nullptr;
    squad_flags.required = false;
    spawn_delay_ms = -1;
    veh_spawn_group = nullptr;
}

// 0x0075B8F0 (:788)
human_teams common_raid_squad_info::get_team() const
{
    return squad_definition_get_team(squad_def);
}

// 0x00771A00 (:794)
bool common_raid_squad_info::load(const xml_element* xml_squad_info_root)
{
    reset();
    xml_element* root = const_cast<xml_element*>(xml_squad_info_root);
    xml_element* e = xml_find(root, "SquadType");
    if (e == nullptr)
    {
        return false;
    }
    squad_def = squad_definition_from_name(e->text);
    if (squad_def == nullptr)
    {
        return false;
    }
    e = xml_find(root, "SpawnDelay");
    if (e == nullptr)
    {
        return false;
    }
    const int delay_ms = xml_get_int(e, 0) * 1000;
    spawn_delay_ms = delay_ms < 0 ? 0 : delay_ms;
    e = xml_find(root, "VehicleType");
    veh_spawn_group = e == nullptr ? nullptr : sog_get_vehicle_group(e->text);
    e = xml_find(root, "required_spawn");
    if (e == nullptr)
    {
        return false;
    }
    squad_flags.required = xml_get_bool(e, false);
    return true;
}

// 0x00768E00 (.h:101)
raid_squad_info::raid_squad_info()
{
    reset();
}

// 0x0075B900 (:835)
void raid_squad_info::reset()
{
    squad_def = nullptr;
    veh_spawn_group = nullptr;
    spawn_delay_ms = 0;
    spawn_object = 0xFFFFFFFF;
    defended_object = 0xFFFFFFFF;
    orders_handle = 0xFFFFFFFF;
    squad_handle = 0xFFFFFFFF;
    orders_creation_time.invalidate();
    orders_destination_loc.zero();
    orders_state = RAID_TEAM_STATE_WAIT;
    raid_squad_flags.spawned = false;
    raid_squad_flags.attacking_team = false;
    raid_squad_flags.defending_team = false;
    raid_squad_flags.inital_spawn = false;
    raid_squad_flags.interior_spawn = false;
    raid_squad_flags.guard_spawn_object = false;
    raid_squad_flags.required = false;
    raid_squad_flags.members_checked = false;
}

// 0x0075B940 (:851)
human_teams raid_squad_info::get_team() const
{
    return squad_definition_get_team(squad_def);
}

// 0x0077DEF0 (.h:158)
base_raid_definition_data::base_raid_definition_data()
{
    reset();
}

// 0x00771AD0 (:861)
void base_raid_definition_data::reset()
{
    raid_name = nullptr;
    explosive_info = nullptr;
    start_radio_line = INVALID_VOICE_LINE_HANDLE;
    demolition_radio_line = INVALID_VOICE_LINE_HANDLE;
    defending_team = HUMAN_TEAM_NONE;
    radius = 0.0f;
    radius_sqrd = 0.0f;
    level = 0;
    explosive_delay = 0xFA; // Magic number: 250 ms
    raid_flags.disable_grenades = false;
    squad_prop_list.clear();
    for (int i = 0; i < squad_prop_list.capacity(); i++)
    {
        squad_prop_list[i].reset();
    }
    intro_line = INVALID_VOICE_LINE_HANDLE;
    vehicle_entry_line = INVALID_VOICE_LINE_HANDLE;
    travel_line = INVALID_VOICE_LINE_HANDLE;
    intro_anim_action = ANIMATION_ACTION_NONE;
    attacker_min = 0;
    attacker_max = 0;
    defender_min = 0;
    defender_max = 0;
}

// 0x00789BA0 (:898)
bool base_raid_definition_data::load(xml_element* xml_root)
{
    static const animation_action DEFAULT_INTRO_ANIM = vanim_lookup_action("talk yell"); // 0x02273DEC
    raid_name = Raid_string_pool.add_unique(xml_table_get_req_string_ref(xml_root, "Name"));
    if (raid_name == nullptr)
    {
        return false;
    }
    const char* text = xml_table_get_opt_string_ref(xml_root, "base_activity_data");
    if (text != nullptr)
    {
        base_data = activity_data_find_handle(text);
    }
    xml_table_get_req_int(level, xml_root, "activity_level");
    float delay = 0.0f;
    xml_table_get_req_float(delay, xml_root, "raid_start_delay");
    const int delay_ms = (int)(delay * 1000.0f);
    max_startup_delay_ms = delay_ms < 0 ? 0 : delay_ms;
    defending_team = human_team_from_name(xml_table_get_req_string_ref(xml_root, "DefendingTeamName"));
    if (defending_team == HUMAN_TEAM_NONE)
    {
        return false;
    }
    bool flag;
    xml_table_get_req_bool(flag, xml_root, "disable_grenades");
    raid_flags.disable_grenades = flag;
    text = xml_table_get_opt_string_ref(xml_root, "StartRadioLine");
    start_radio_line = text == nullptr ? INVALID_VOICE_LINE_HANDLE : voice_line_handle_from_name(text);
    text = xml_table_get_opt_string_ref(xml_root, "DemolitionRadioLine");
    demolition_radio_line = text == nullptr ? INVALID_VOICE_LINE_HANDLE : voice_line_handle_from_name(text);
    xml_table_get_req_float(radius, xml_root, "RaidRadius");
    radius_sqrd = radius * radius;
    xml_table_get_req_float(safe_distance, xml_root, "safe_demolition_distance");
    safe_distance_sqrd = safe_distance * safe_distance;
    text = xml_table_get_opt_string_ref(xml_root, "explosive_type");
    explosive_info = text == nullptr ? nullptr : explosion_lookup(text);
    explosive_delay = 0xFA;
    if (explosive_info != nullptr)
    {
        xml_table_get_req_int(explosive_delay, xml_root, "explosive_delay");
    }
    xml_element* list = xml_find(xml_root, "SquadList");
    if (list == nullptr)
    {
        return false;
    }
    for (xml_element* entry = xml_find(list, "SquadDefinition"); entry != nullptr; entry = xml_find_next(list, entry, "SquadDefinition"))
    {
        if (squad_prop_list.size() >= squad_prop_list.capacity())
        {
            keen::debug::nativePrintArgs("Int3 critical function at %s(%d)\n", __FILE__, __LINE__);
            break;
        }
        common_raid_squad_info squad;
        if (squad.load(entry))
        {
            squad_prop_list.add(squad);
        }
    }
    xml_element* attacker = xml_find(xml_root, "attacker_ai_props");
    xml_element* defender = xml_find(xml_root, "defender_ai_props");
    if (attacker == nullptr || defender == nullptr)
    {
        return false;
    }
    xml_table_get_req_int(attacker_min, attacker, "standard_min");
    xml_table_get_req_int(attacker_max, attacker, "standard_max");
    xml_table_get_req_int(defender_min, defender, "standard_min");
    xml_table_get_req_int(defender_max, defender, "standard_max");
    // Magic number: at most 256 of a side.
    CAP<int>(attacker_min, 0, 0x100);
    CAP<int>(attacker_max, attacker_min, 0x100);
    CAP<int>(defender_min, 0, 0x100);
    CAP<int>(defender_max, defender_min, 0x100);
    load_standard_rewards(standard_rewards, xml_root);
    xml_element* lines = xml_find(xml_root, "intro_lines");
    if (lines == nullptr)
    {
        return true;
    }
    xml_element* e = xml_find(lines, "raid_intro_line");
    if (e == nullptr)
    {
        return false;
    }
    intro_line = voice_line_handle_from_name(e->text);
    e = xml_find(lines, "vehicle_entry_line");
    vehicle_entry_line = e == nullptr ? INVALID_VOICE_LINE_HANDLE : voice_line_handle_from_name(e->text);
    e = xml_find(lines, "travel_line");
    travel_line = e == nullptr ? INVALID_VOICE_LINE_HANDLE : voice_line_handle_from_name(e->text);
    e = xml_find(lines, "into_anim"); // the original's spelling
    intro_anim_action = e != nullptr ? vanim_lookup_action(e->text) : DEFAULT_INTRO_ANIM;
    return true;
}

// 0x0079B3F0 (:1053)
void raid_common_system_init()
{
    load_raid_data(-1);
}

// 0x00771B60 (:1061)
int activity_raid_find_raid_handle(const base_raid_definition_data* raid_info)
{
    return raid_info == nullptr ? -1 : Raid_def_pool.get_index(raid_info);
}

// 0x00789FF0 (:1108)
const base_raid_definition_data* raid_get_def_by_name(const char* raid_name)
{
    for (base_raid_definition_data* data = Raid_def_pool.get_first(0); data != nullptr; data = Raid_def_pool.get_next(data))
    {
        if (v_stricmp(raid_name, data->raid_name) == 0)
        {
            return data;
        }
    }
    return nullptr;
}

// 0x0078A070 (:1130)
void raid_build_reinforcement_waypoints(const object* raid_objp, farray<vector, 64>& external_dest_list, const vector& center, float radius, const base_array<unsigned int>& raid_target_list)
{
    build_external_destination_list(external_dest_list, center, radius);
    if (external_dest_list.size() < 1)
    {
        build_default_destination_list(raid_objp, external_dest_list, center, radius, raid_target_list);
    }
}

// 0x0075B950 (:1141)
bool activity_raid_go_to_raid(const npc* np)
{
    const activity_modes mode = activity_get_mode();
    if (mode != ACTIVITY_MODE_RAID_DEFEND && mode != ACTIVITY_MODE_RAID_ATTACK)
    {
        return false;
    }
    return (np->hflags.activity_raid_required || np->hflags.activity_raid_optional) && !np->npc_flags.entered_raid;
}

// 0x0075B990 (:1152). rfg.exe: LTCG, pos in ESI. All three corners lie past the radius.
static bool is_outside_raid_radius(const nav_cell* cell, const vector& pos, float radius_sqrd)
{
    if (cell == nullptr)
    {
        return false;
    }
    vector a;
    vector b;
    vector c;
    nav_cell_get_vertices(&a, &b, &c, cell);
    return radius_sqrd < vec_xz_dist_squared(a, pos) && radius_sqrd < vec_xz_dist_squared(b, pos) && radius_sqrd < vec_xz_dist_squared(c, pos);
}

// 0x0078A0C0 (:1177). Original: find_seed_nav_cells gets the squared radius, which it does not read.
void raid_build_exit_points(const object* objp, float raid_radius, const vector& target_pos, float demo_radius, farray<raid_exit_cell, 128>& exit_cell_list, const base_array<unsigned int>& raid_target_list)
{
    farray<nav_cell const*, 1024> open;
    farray<nav_cell const*, 8> seeds;
    const float raid_radius_sqrd = raid_radius * raid_radius;
    const float demo_radius_sqrd = demo_radius * demo_radius;
    if (!find_seed_nav_cells(objp, raid_radius_sqrd, seeds, raid_target_list))
    {
        keen::debug::nativePrintArgs("Int3 critical function at %s(%d)\n", __FILE__, __LINE__);
        return;
    }
    if (seeds.size() < 1)
    {
        return;
    }
    for (int i = 0; i < seeds.size(); i++)
    {
        open.add(seeds[i]);
    }
    exit_cell_list.clear();
    for (int i = 0; i < open.size(); i++)
    {
        const nav_cell* cell = open[i];
        if (is_outside_raid_radius(cell, target_pos, demo_radius_sqrd) && is_outside_raid_radius(cell, target_pos, raid_radius_sqrd))
        {
            raid_exit_cell exit_cell;
            exit_cell.cell_handle = nav_cell_get_handle(cell);
            nav_cell_get_centroid(&exit_cell.center_pos, cell);
            exit_cell_list.add(exit_cell);
            if (exit_cell_list.size() >= exit_cell_list.capacity())
            {
                return;
            }
        }
        else if (open.size() < open.capacity())
        {
            for (int a = 0; a < 3; a++)
            {
                const nav_cell* adjacent = cell->adjacent_cells[a].to_pointer();
                if (adjacent != nullptr && (adjacent->flags & NAV_CELL_RAID_EXCLUDED) == 0)
                {
                    open.add_unique(adjacent);
                    if (open.size() >= open.capacity())
                    {
                        break;
                    }
                }
            }
        }
    }
}

// 0x0075BA60 (:1255)
raid_team_states raid_common_resolve_state(const char* state_name)
{
    for (int i = 0; i < NUM_RAID_TEAM_STATES; i++)
    {
        if (RAID_NAME_TO_STATE[i] != nullptr && v_stricmp(RAID_NAME_TO_STATE[i], state_name) == 0)
        {
            return (raid_team_states)i;
        }
    }
    return RAID_TEAM_STATE_ANY;
}

// 0x0075BAA0 (:1271)
human_teams raid_get_defending_team(const base_raid_definition_data* data)
{
    return data->defending_team;
}
