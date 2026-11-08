// Original: volition/rfg/code/gameplay/activities/activity_continuous_attack.cpp

#include "volition/rfg/code/gameplay/activities/activity_continuous_attack.h"

#include "volition/rfg/code/gameplay/activities/gfm_reinforcement_coordinator.h"
#include "volition/rfg/code/gameplay/gameflow/gfm_orders_manager.h"
#include "volition/rfg/code/gameplay/squad_spawn.h"
#include "volition/rfg/code/hud/minimap.h"
#include "volition/rfg/code/level/world.h"
#include "volition/rfg/code/objects/humans/human.h"
#include "volition/rfg/code/objects/humans/npc.h"
#include "volition/rfg/code/objects/humans/player.h"
#include "volition/rfg/code/objects/object_patrol.h"
#include "volition/rfg/code/objects/object_spawn_region.h"
#include "volition/rfg/code/objects/object_squad.h"
#include "volition/rfg/code/rfg.h"
#include "volition/rfg/code/scripting/action_script/global_event_data.h"
#include "volition/rfg/code/scripting/action_script/global_event_manager.h"
#include "volition/rfg/code/spawn/spawn_object_group.h"
#include "volition/rfg/code/spawn/spawn_resource_manager.h"
#include "volition/rfg/code/vehicle/vehicle_class.h"
#include "volition/rfg/code/vehicle/vehicle_info.h"
#include "volition/vlib/code/math/matrix.h"
#include "volition/vlib/code/math/rand.h"
#include "volition/vlib/code/math/vector3.h"
#include "volition/vlib/code/memory/object_pool.h"
#include "volition/vlib/code/memory/rfg_split_mempool.h"
#include "volition/vlib/code/memory/static_mempool.h"
#include "volition/vlib/code/parse/xml_table_tools.h"
#include "volition/vlib/code/util/fastxml.h"
#include "volition/vlib/code/util/string_pool.h"
#include "volition/vlib/code/util/string_util.h"

#include <stdio.h>

extern gfm_orders_flags ACTIVITY_ORDERS_FLAGS; // 0x02231B21 (gfm_smash_building_coordinator.cpp)

activity_continuous_attack_data Continuous_attack_data; // 0x01652350

static const gfm_orders_flags DEFAULT_ORDERS_FLAGS(true, false);
static const int INVALID_CONT_ATTACK_INDEX = 0; // 0x022295B4, nothing writes it
static const char* CONT_ATTACK_TYPE_NAMES[NUM_CONT_ATTACK_TYPES] = {nullptr, "Patrol"}; // 0x0165131C
static farray<unsigned int, 8> Last_spawn_region_list; // 0x01652DD0
static object_pool<base_continuous_attack_data, 60, 1> Cont_attack_def_pool; // 0x02235010, Magic number: 60, from rfg.pdb
static farray<base_continuous_attack_data*, 60> Dlc_activity_cont_attack_list; // 0x01652250
static string_pool Cont_attack_string_pool("Cont Attack Pool"); // 0x02231B90
static char* Cont_attack_string_pool_data; // 0x022295B8

// 0x00753B20 (.h:246)
cont_attack_squad_reached_end_event::cont_attack_squad_reached_end_event()
    : event_data(EVENT_CONT_ATTACK_SQUAD_REACHED_END)
{
}

// 0x00769230 (.h:29)
continuous_attack_squad_data::continuous_attack_squad_data()
{
    reset();
}

// 0x00754720 (.h:44)
void continuous_attack_squad_data::reset()
{
    squad_def = nullptr;
    veh_group = nullptr;
    sub_group = 0;
    veh_hp_multiplier = 1.0f;
    squad_flags.stay_in_vehicle = false;
}

// 0x0077ED00 (.h:57)
base_continuous_attack_data::base_continuous_attack_data()
{
    reset();
}

// 0x00769250 (.h:82)
void base_continuous_attack_data::reset()
{
    name_crc = 0;
    continuous_attack_name = nullptr;
    type = CONT_ATTACK_TYPE_NONE;
    attacker_count = 0;
    spawn_period_ms = 0;
    show_squad_icons = false;
    squad_list.clear();
}

// 0x00795A70 (.h:115)
activity_continuous_attack_data::activity_continuous_attack_data()
    : activity_base_free(ACTIVITY_MODE_CONTINUOUS_ATTACK)
{
}

// 0x0077ED60
activity_continuous_attack_data::~activity_continuous_attack_data()
{
}

// 0x00754740 (.h:149)
int activity_continuous_attack_data::get_attacker_count() const
{
    return attacker_count;
}

// 0x00754750 (.h:152)
void activity_continuous_attack_data::change_attacker_count(int new_attacker_count)
{
    attacker_count = new_attacker_count;
}

// 0x00754760 (.h:154)
void activity_continuous_attack_data::set_spawn_group(int spawn_group)
{
    active_sub_group = spawn_group;
}

// 0x00754770 (.h:158)
unsigned int activity_continuous_attack_data::get_target_handle()
{
    return attack_target_handle;
}

// 0x0076A280. A vehicle of the group is streamed, or shares a container with the default EDF group.
static bool vehicle_resource_ready(const vehicle_spawn_group_info* veh_group)
{
    if (veh_group == nullptr)
    {
        return true;
    }
    for (int i = 0; i < veh_group->v_info_list.size(); i++)
    {
        if (srm_vehicle_info_loaded(veh_group->v_info_list[i]))
        {
            return true;
        }
    }
    const vehicle_spawn_group_info* default_group = sog_default_vehicle_spawn_group(HUMAN_TEAM_EDF);
    if (default_group == nullptr)
    {
        return false;
    }
    for (int i = 0; i < veh_group->v_info_list.size(); i++)
    {
        for (int j = 0; j < default_group->v_info_list.size(); j++)
        {
            if (veh_group->v_info_list[i]->srid == default_group->v_info_list[j]->srid)
            {
                return true;
            }
        }
    }
    return false;
}

// 0x0077FE60 (:156)
base_continuous_attack_data* find_cont_attack_data(int cont_attack_index)
{
    for (base_continuous_attack_data* data = Cont_attack_def_pool.get_first(0); data != nullptr; data = Cont_attack_def_pool.get_next(data))
    {
        if (cont_attack_index == data->name_crc)
        {
            return data;
        }
    }
    return nullptr;
}

// 0x0077FED0 (:170). Prefers a region not used lately; when only used ones are left, the list starts over.
const object_spawn_region* find_local_spawn_region(bool ped_only)
{
    farray<object_spawn_region const*, 32> fresh;
    farray<object_spawn_region const*, 32> used;
    // Magic numbers: at least 50 metres for people on foot or 150 with a vehicle, at most 350.
    const float min_dist_sq = ped_only ? 2500.0f : 22500.0f;
    for (int i = 0; i < World.get_num_objects<object_spawn_region>(); i++)
    {
        const object_spawn_region* region = World.get_object<object_spawn_region>(i);
        const float dist = player_find_shortest_player_xz_dist_sqrd(region->pos);
        if (dist < min_dist_sq || dist > 122500.0f)
        {
            continue;
        }
        bool was_used = false;
        for (int j = 0; j < Last_spawn_region_list.size(); j++)
        {
            if (Last_spawn_region_list[j] == region->get_handle())
            {
                was_used = true;
                break;
            }
        }
        farray<object_spawn_region const*, 32>& target = was_used ? used : fresh;
        if (target.size() < target.capacity())
        {
            target.add(region);
        }
        else
        {
            keen::debug::nativePrintArgs("Int3 critical function at %s(%d)\n", __FILE__, __LINE__);
        }
    }
    const object_spawn_region* picked = nullptr;
    if (fresh.size() > 0)
    {
        picked = fresh[fresh.size() == 1 ? 0 : int_rand_range(0, fresh.size() - 1)];
        if (picked == nullptr)
        {
            return nullptr;
        }
        if (Last_spawn_region_list.size() >= Last_spawn_region_list.capacity())
        {
            Last_spawn_region_list.remove(0);
        }
    }
    else
    {
        if (used.size() < 1)
        {
            return nullptr;
        }
        Last_spawn_region_list.clear();
        picked = used[used.size() == 1 ? 0 : int_rand_range(0, used.size() - 1)];
        if (picked == nullptr)
        {
            return nullptr;
        }
    }
    Last_spawn_region_list.add(picked->get_handle());
    return picked;
}

// 0x00796B70 (:239)
object_patrol* check_for_patrol(const object_spawn_region* osrp)
{
    vector bmin;
    vector bmax;
    osrp->get_local_bbox(bmin, bmax);
    unsigned int handle = 0xFFFFFFFF;
    if (World.collect_objects<object_patrol>(bmin, bmax, osrp->pos, osrp->orient, &handle, 1, nullptr, nullptr) == 0 || handle == 0xFFFFFFFF)
    {
        return nullptr;
    }
    object* objp = World.object_from_handle(handle);
    return objp != nullptr && objp->get_obj_type() == OT_PATROL ? static_cast<object_patrol*>(objp) : nullptr;
}

// 0x007573F0 (:258)
bool continuous_attack_squad_data::load(xml_element* xml_squad_info)
{
    reset();
    sub_group = xml_get_int(xml_find(xml_squad_info, "sub_group_index"), 0);
    xml_element* e = xml_find(xml_squad_info, "squad_def");
    if (e == nullptr)
    {
        return false;
    }
    squad_def = squad_definition_from_name(e->text);
    if (squad_def == nullptr)
    {
        return false;
    }
    e = xml_find(xml_squad_info, "vehicle_group");
    veh_group = e == nullptr ? nullptr : sog_get_vehicle_group(e->text);
    squad_flags.stay_in_vehicle = xml_get_bool(xml_find(xml_squad_info, "stay_in_vehicle"), false);
    e = xml_find(xml_squad_info, "veh_hp_scale_factor");
    if (e != nullptr)
    {
        veh_hp_multiplier = xml_get_float(e, 1.0f);
    }
    return true;
}

// 0x00780180 (:296)
bool base_continuous_attack_data::load(xml_element* xml_root)
{
    reset();
    xml_element* e = xml_find(xml_root, "name");
    if (e == nullptr)
    {
        return false;
    }
    continuous_attack_name = Cont_attack_string_pool.add_unique(e->text);
    if (continuous_attack_name == nullptr)
    {
        return false;
    }
    name_crc = crc_stri(continuous_attack_name, 0);
    e = xml_find(xml_root, "attacker_count");
    if (e == nullptr)
    {
        return false;
    }
    const int count = xml_get_int(e, 0);
    attacker_count = count < 0 ? 0 : count;
    e = xml_find(xml_root, "respawn_period");
    if (e == nullptr)
    {
        return false;
    }
    float period = xml_get_float(e, 0.0f);
    if (0.0f <= 0.0f - period)
    {
        period = 0.0f;
    }
    spawn_period_ms = (int)period * 1000; // Original: whole seconds, the fraction is dropped first
    e = xml_find(xml_root, "type");
    type = e == nullptr ? CONT_ATTACK_TYPE_NONE : activity_cont_attack_get_type_from_name(e->text);
    e = xml_find(xml_root, "show_squad_icons");
    if (e == nullptr)
    {
        return false;
    }
    show_squad_icons = xml_get_bool(e, false);
    xml_element* list = xml_find(xml_root, "squad_list");
    if (list == nullptr)
    {
        return false;
    }
    continuous_attack_squad_data squad;
    for (xml_element* entry = xml_find(list, "squad_info"); entry != nullptr; entry = xml_find_next(list, entry, "squad_info"))
    {
        if (squad.load(entry))
        {
            squad_list.add(squad);
        }
    }
    return squad_list.size() > 0;
}

// 0x007A8DC0 (:359)
void activity_continuous_attack_data::level_init()
{
    activity_base_free::level_init(); // rfg.exe inlines it
    reset();
}

// 0x007A0BB0 (:367). Original: the slots are walked up to the array's capacity; its count stays zero.
void activity_continuous_attack_data::reset()
{
    attack_target_handle = 0xFFFFFFFF;
    base_spawn_info = nullptr;
    for (int i = 0; i < reinforcement_squad_list.capacity(); i++)
    {
        reinforcement_squad_list[i].release_and_reset(); // rfg.exe inlines it
    }
    reinforcement_squad_list.clear();
    polling_timer.invalidate();
    spawn_timer.invalidate();
    last_target_pos.zero();
    attacker_count = 0;
    num_squad_patrols_finished = 0;
    active_sub_group = 0;
}

// 0x007ACF90 (:386)
bool activity_continuous_attack_data::activity_start()
{
    return activity_base_free::activity_start();
}

// 0x007A0C90 (:396)
void activity_continuous_attack_data::activity_end()
{
    activity_base_free::activity_end(); // rfg.exe inlines it
    reset();
}

// 0x007A0CB0 (:405)
void activity_continuous_attack_data::activity_kill()
{
    activity_base_free::activity_kill(); // rfg.exe inlines it
    reset();
}

// 0x007B7330 (:414)
void activity_continuous_attack_data::activity_do_frame_active()
{
    if (base_spawn_info->type == CONT_ATTACK_TYPE_PATROL)
    {
        activity_do_frame_active_patrol();
    }
    for (int i = 0; i < reinforcement_squad_list.capacity(); i++)
    {
        reinforcement_squad_list[i].do_frame();
    }
    for (int i = 0; i < reinforcement_squad_list.capacity(); i++)
    {
        reinforcement_squad& slot = reinforcement_squad_list[i];
        if (slot.squad_handle != 0xFFFFFFFF && slot.squad_despawned())
        {
            slot.release_and_reset(); // rfg.exe inlines it
        }
    }
    const int squad_count = get_squad_count(); // rfg.exe inlines it
    if (spawn_timer.valid())
    {
        if (spawn_timer.elapsed())
        {
            if (squad_count < attacker_count)
            {
                add_new_squad();
            }
            spawn_timer.invalidate();
        }
    }
    else if (squad_count < attacker_count)
    {
        spawn_timer.set(base_spawn_info->spawn_period_ms);
    }
}

// 0x0076A340 (:469)
int activity_continuous_attack_data::get_squad_count() const
{
    int count = 0;
    for (int i = 0; i < reinforcement_squad_list.capacity(); i++)
    {
        if (reinforcement_squad_list[i].squad_handle != 0xFFFFFFFF)
        {
            count++;
        }
    }
    return count;
}

// 0x0076A360 (:486)
void activity_continuous_attack_data::render_icons() const
{
    if (!reinforcement_should_render_icon() || !is_active() || !base_spawn_info->show_squad_icons)
    {
        return;
    }
    for (int i = 0; i < reinforcement_squad_list.capacity(); i++)
    {
        const unsigned int handle = reinforcement_squad_list[i].squad_handle;
        if (handle == 0xFFFFFFFF)
        {
            continue;
        }
        const object* objp = World.object_from_handle(handle);
        if (objp != nullptr && objp->get_obj_type() == OT_SQUAD)
        {
            Cartographer.add_temporary_icon(objp->pos, MAP_ELEMENT_MISSION_OBJECTIVE, true, 0xFFFFFFFF, Default_map_item_color, false, 0xFFFFFFFF);
        }
    }
}

// 0x00780340 (:515). Original: a squad at the end of its patrol posts the event every frame until it moves off.
void activity_continuous_attack_data::activity_do_frame_active_patrol()
{
    for (int i = 0; i < reinforcement_squad_list.capacity(); i++)
    {
        reinforcement_squad& slot = reinforcement_squad_list[i];
        if (slot.squad_handle == 0xFFFFFFFF)
        {
            continue;
        }
        object* objp = World.object_from_handle(slot.squad_handle);
        if (objp == nullptr || objp->get_obj_type() != OT_SQUAD)
        {
            continue;
        }
        const object_squad* squad = static_cast<object_squad*>(objp);
        const object_patrol* patrol = squad->get_patrol();
        vector end;
        if (patrol == nullptr || !patrol->get_last_patrol_node_pos(end) || vec_dist_squared(end, squad->pos) > 100.0f) // Magic number: ten metres
        {
            continue;
        }
        gfm_orders* orders = Orders_manager.get_orders_from_handle(slot.orders_handle);
        if (orders != nullptr)
        {
            gfm_orders_creation_params params;
            params.reset();
            params.location_target = last_target_pos;
            params.object_target = attack_target_handle;
            params.ofp = ACTIVITY_ORDERS_FLAGS;
            params.orders_type = GFM_ORDERS_TYPE_ATTACK;
            params.team = HUMAN_TEAM_EDF;
            if (orders->create_orders(params))
            {
                attacker_count++;
            }
        }
        num_squad_patrols_finished++;
        cont_attack_squad_reached_end_event data;
        data.count = num_squad_patrols_finished;
        gem_post_event(data);
    }
}

// 0x0076A3F0 (:565)
reinforcement_squad* activity_continuous_attack_data::find_unused_slot()
{
    for (int i = 0; i < reinforcement_squad_list.capacity(); i++)
    {
        if (reinforcement_squad_list[i].squad_handle == 0xFFFFFFFF)
        {
            return &reinforcement_squad_list[i];
        }
    }
    return nullptr;
}

// 0x00780530 (:579)
bool activity_continuous_attack_data::pick_squad_index(int& squad_index)
{
    if (base_spawn_info->squad_list.size() == 1)
    {
        squad_index = 0;
        return true;
    }
    farray<int, 16> candidates;
    for (int i = 0; i < base_spawn_info->squad_list.size(); i++)
    {
        const continuous_attack_squad_data& squad = base_spawn_info->squad_list[i];
        if (squad.sub_group == active_sub_group && vehicle_resource_ready(squad.veh_group))
        {
            candidates.add(i);
        }
    }
    if (candidates.size() == 0)
    {
        return false;
    }
    squad_index = candidates[candidates.size() == 1 ? 0 : int_rand_range(0, candidates.size() - 1)];
    return true;
}

// 0x007B5760 (:624)
void activity_continuous_attack_data::add_new_squad()
{
    int index;
    if (base_spawn_info->squad_list.size() < 1 || !pick_squad_index(index) || index < 0 || index >= base_spawn_info->squad_list.size())
    {
        return;
    }
    const continuous_attack_squad_data& squad = base_spawn_info->squad_list[index];
    reinforcement_squad* slot = find_unused_slot();
    if (slot == nullptr)
    {
        return;
    }
    slot->release_and_reset();
    squad_spawn_parameters params;
    params.squad.osrp = find_local_spawn_region(squad.veh_group == nullptr);
    if (params.squad.osrp == nullptr)
    {
        return;
    }
    const object_patrol* patrol = base_spawn_info->type == CONT_ATTACK_TYPE_PATROL ? check_for_patrol(params.squad.osrp) : nullptr;
    if (patrol != nullptr)
    {
        params.orders.orders_type = GFM_ORDERS_TYPE_PATROL;
        params.orders.location_target = patrol->pos;
        params.orders.object_target = patrol->get_handle();
        params.orders.ofp = DEFAULT_ORDERS_FLAGS;
    }
    else
    {
        params.orders.location_target = last_target_pos;
        params.orders.object_target = attack_target_handle;
        params.orders.ofp = ACTIVITY_ORDERS_FLAGS;
        params.orders.orders_type = GFM_ORDERS_TYPE_ATTACK;
    }
    params.squad.squad_def = squad.squad_def;
    params.squad.veh_spawn_group = squad.veh_group;
    params.squad.vehicle_hp_mult = squad.veh_hp_multiplier;
    params.squad.squad_pos = params.squad.osrp->pos;
    params.squad.squad_flags.disallow_reassignment = 1;
    params.squad.npc_flags.activity_pursuit_ca = 1;
    params.squad.hflags.limited_vehicle_exit = squad.squad_flags.stay_in_vehicle;
    vector facing = last_target_pos - params.squad.osrp->pos;
    facing.y = 0.0f;
    facing.normalize_safe();
    params.squad.squad_orient.make_quick(facing);
    slot->spawn_squad(attack_target_handle, params, false, false);
}

// 0x0076A420 (:707)
bool activity_continuous_attack_data::is_squad_member(unsigned int squad_handle)
{
    for (int i = 0; i < reinforcement_squad_list.capacity(); i++)
    {
        if (reinforcement_squad_list[i].squad_handle == squad_handle)
        {
            return true;
        }
    }
    return false;
}

// 0x00780670 (:720). No caller in rfg.exe's activity_do_frame_active, so the target position stays where it started.
void activity_continuous_attack_data::do_frame_update_pos()
{
    const object* target = World.object_from_handle(attack_target_handle);
    if (target == nullptr || vec_dist_squared(last_target_pos, target->pos) < 25.0f) // Magic number: five metres
    {
        return;
    }
    last_target_pos = target->pos;
    for (int i = 0; i < reinforcement_squad_list.capacity(); i++)
    {
        gfm_orders* orders = Orders_manager.get_orders_from_handle(reinforcement_squad_list[i].orders_handle);
        if (orders != nullptr)
        {
            orders->location_target = last_target_pos;
        }
    }
}

// 0x007A0CD0 (:753)
void activity_continuous_attack_data::system_init()
{
    if (Cont_attack_string_pool_data == nullptr)
    {
        Cont_attack_string_pool_data = static_cast<char*>(Permanent_mempool.alloc(0x380, 0)); // Magic number: 896 bytes of names
        if (Cont_attack_string_pool_data == nullptr)
        {
            keen::debug::nativePrintArgs("Insufficient memory to allocate the radio string table"); // Original's text
        }
        Cont_attack_string_pool.init(Cont_attack_string_pool_data, 0x380, "Cont Attack Pool", 0, 0);
    }
    Cont_attack_string_pool.clear();
    for (base_continuous_attack_data* data = Cont_attack_def_pool.get_first(0); data != nullptr; )
    {
        data = Cont_attack_def_pool.free(data);
    }
    xml_element* root = xml_table_parse("activity_continuous_attack.xtbl");
    if (root == nullptr)
    {
        return;
    }
    for (xml_element* entry = xml_find(root, "continuous_attack_data"); entry != nullptr; entry = xml_find_next(root, entry, "continuous_attack_data"))
    {
        base_continuous_attack_data* data = Cont_attack_def_pool.alloc(0);
        if (data != nullptr && !data->load(entry))
        {
            Cont_attack_def_pool.free(data);
        }
    }
    xml_table_close(root);
}

// 0x007574D0 (:810)
bool activity_continuous_attack_data::attack_init(const object* target_objp, const base_continuous_attack_data* attack_data)
{
    if (target_objp == nullptr || attack_data == nullptr)
    {
        return false;
    }
    unsigned int target = target_objp->get_handle();
    const object* who = nullptr;
    if (target_objp->get_obj_type() == OT_SQUAD)
    {
        who = static_cast<const object_squad*>(target_objp)->get_leader();
    }
    else if (target_objp->get_obj_type() == OT_VEHICLE)
    {
        who = vehicle_passenger_in_seat(static_cast<const vehicle*>(target_objp), VSI_DRIVER);
    }
    if (who != nullptr)
    {
        target = who->get_handle();
    }
    attack_target_handle = target;
    last_target_pos = target_objp->pos;
    polling_timer.set(0);
    spawn_timer.set(0);
    base_spawn_info = attack_data;
    attacker_count = attack_data->attacker_count;
    return true;
}

// 0x0076A450 (:840). Original: walks the array's count, which stays zero, so it always answers 0.
int activity_continuous_attack_data::get_squads(int max_nums, unsigned int* squads)
{
    int count = 0;
    for (int i = 0; i < reinforcement_squad_list.size() && i < max_nums; i++)
    {
        squads[i] = reinforcement_squad_list[i].squad_handle;
        count++;
    }
    return count;
}

// 0x00757570 (:856)
int activity_cont_attack_get_index(const char* cont_attack_name)
{
    return cont_attack_name == nullptr ? INVALID_CONT_ATTACK_INDEX : (int)crc_stri(cont_attack_name, 0);
}

// 0x00757590 (:866)
cont_attack_type activity_cont_attack_get_type_from_name(const char* type_name)
{
    for (int i = CONT_ATTACK_TYPE_PATROL; i < NUM_CONT_ATTACK_TYPES; i++)
    {
        if (v_stricmp(type_name, CONT_ATTACK_TYPE_NAMES[i]) == 0)
        {
            return (cont_attack_type)i;
        }
    }
    return CONT_ATTACK_TYPE_NONE;
}

// 0x007575D0 (:879)
bool activity_cont_attack_get_attacker_count(int& count)
{
    if (Continuous_attack_data.is_active())
    {
        count = Continuous_attack_data.get_attacker_count();
        return true;
    }
    count = 0;
    return false;
}

// 0x00757600 (:891)
void activity_cont_attack_set_attacker_count(int new_attacker_count)
{
    if ((unsigned int)new_attacker_count < 16 && Continuous_attack_data.is_active()) // Magic number: the 16 squad slots
    {
        Continuous_attack_data.change_attacker_count(new_attacker_count);
    }
}

// 0x007ACFA0 (:902)
bool activity_cont_attack_start(const object* target_obj, int cont_attack_index)
{
    if (target_obj == nullptr || cont_attack_index == INVALID_CONT_ATTACK_INDEX || Continuous_attack_data.is_active())
    {
        return false;
    }
    const base_continuous_attack_data* attack_data = find_cont_attack_data(cont_attack_index);
    if (attack_data == nullptr)
    {
        return false;
    }
    Continuous_attack_data.level_init();
    if (Continuous_attack_data.attack_init(target_obj, attack_data) && Continuous_attack_data.activity_base_free::activity_start())
    {
        return true;
    }
    Continuous_attack_data.activity_kill(); // rfg.exe inlines it
    return false;
}

// 0x007A0DE0 (:934)
void activity_cont_attack_end()
{
    Continuous_attack_data.activity_kill(); // rfg.exe inlines the same body
}

// 0x0076A4A0 (:940)
bool activity_cont_attack_check_squad(unsigned int squad_handle)
{
    return Continuous_attack_data.is_active() && Continuous_attack_data.is_squad_member(squad_handle); // rfg.exe inlines it
}

// 0x00757620 (:952)
void activity_cont_attack_set_spawn_group(unsigned int group_index)
{
    if (Continuous_attack_data.is_active())
    {
        Continuous_attack_data.set_spawn_group(group_index);
    }
}

// 0x0076A4E0 (:959)
int activity_cont_attack_get_squads(int max_num, unsigned int* squads)
{
    return Continuous_attack_data.get_squads(max_num, squads);
}

// 0x00757640 (:964)
unsigned int activity_cont_attack_get_target()
{
    return Continuous_attack_data.get_target_handle();
}

// 0x00796BF0 (:969)
void activity_cont_attack_dlc_init(int dlc_id)
{
    char filename[256];
    sprintf(filename, "dlc%02d_%s", dlc_id, "activity_continuous_attack.xtbl");
    xml_element* root = xml_table_parse(filename);
    if (root == nullptr)
    {
        return;
    }
    for (int i = 0; i < Dlc_activity_cont_attack_list.size(); i++)
    {
        Cont_attack_def_pool.free(Dlc_activity_cont_attack_list[i]);
    }
    Dlc_activity_cont_attack_list.clear();
    Cont_attack_string_pool.set_thread_ownership();
    for (xml_element* entry = xml_find(root, "continuous_attack_data"); entry != nullptr; entry = xml_find_next(root, entry, "continuous_attack_data"))
    {
        base_continuous_attack_data* data = Cont_attack_def_pool.alloc(0);
        if (data != nullptr)
        {
            if (data->load(entry))
            {
                Dlc_activity_cont_attack_list.add(data);
            }
            else
            {
                Cont_attack_def_pool.free(data);
            }
        }
    }
    xml_table_close(root);
}
