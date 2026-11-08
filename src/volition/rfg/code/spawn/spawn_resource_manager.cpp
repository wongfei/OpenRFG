// Original: volition/rfg/code/spawn/spawn_resource_manager.cpp
//
// The spawn resource manager (stub_impl.md S9): which human and vehicle stream containers are loaded. Each info has a slot
// record; users reserve one through a slot type (at most slot_max per type, with an owner), the ambient system allocates
// free ones at low priority, and a mission replaces all reservations with its own set. A vehicle family shares one loaded
// variant. object_spawn_do_spawn and object_spawn_vehicle hand a request over to spawn placement once its resources are in.

#include "volition/rfg/code/spawn/spawn_resource_manager.h"

#include "volition/rfg/code/gameplay/activities/activity.h"
#include "volition/rfg/code/gameplay/gameflow/squad_progression.h"
#include "volition/rfg/code/gameplay/wrecking_crew/wrecking_crew.h"
#include "volition/rfg/code/level/world.h"
#include "volition/rfg/code/multi/multi_core/include/multi.h"
#include "volition/rfg/code/objects/humans/human_info.h"
#include "volition/rfg/code/objects/humans/npc.h"
#include "volition/rfg/code/objects/humans/player.h"
#include "volition/rfg/code/objects/object_spawn_node.h"
#include "volition/rfg/code/objects/object_squad.h"
#include "volition/rfg/code/objects/object_zone.h"
#include "volition/rfg/code/rfg.h"
#include "volition/rfg/code/spawn/ambient_spawn_info.h"
#include "volition/rfg/code/spawn/spawn_object_group.h"
#include "volition/rfg/code/spawn/spawn_placement.h"
#include "volition/rfg/code/spawn/spawn_request.h"
#include "volition/rfg/code/spawn/spawn_resource_data.h"
#include "volition/rfg/code/spawn/spawn_resource_manager_internal.h"
#include "volition/rfg/code/vehicle/vehicle_class.h"
#include "volition/rfg/code/vehicle/vehicle_info.h"
#include "volition/rfg/code/vehicle/vehicle_system.h"
#include "volition/rfg/code/vehicle/vehicle_table_parser.h"
#include "volition/stream2/code/stream2.h"
#include "volition/vlib/code/math/rand.h"
#include "volition/vlib/code/math/vector3.h"

#include <float.h>
#include <string.h>

// Magic numbers: 100 m from the player for a special node to count, 90 m for an object to outlive its special slot (both
// squared), and the slot time-outs in milliseconds.
static const float SPECIAL_NODE_RANGE_SQUARED = 10000.0f;
static const float SPECIAL_OBJECT_KEEP_RANGE_SQUARED = 8100.0f;

// ---- public globals
farray<human_info const*, 16> Desired_guerrilla_list; // 0x016B7980 (ambient_spawn_manager.cpp fills it)
farray<vehicle_info const*, 128> Desired_vehicle_list; // 0x016B7C78 (ambient_spawn_manager.cpp fills it)
bool Mission_mode; // 0x0374246E

// ---- file-static globals
static farray<human_info const*, 165> Availible_human_info_list; // 0x016B8558
static farray<vehicle_info const*, 163> Availible_vehicle_info_list; // 0x016B8800
static timestamp Special_slot_timer; // 0x016B8A9C
static timestamp Slot_rotation_timer; // 0x016B8AA0
static timestamp Stream_check_period; // 0x016B8AA4
static timestamp Slot_release_period; // 0x016B8AA8
static timestamp Human_slot_release_timestamp; // 0x016B8AF4
static timestamp Vehicle_slot_release_timestamp; // 0x016B8AF8
static bool Stream_check_humans = true; // 0x016B67F1
static bool Slot_release_humans = true; // 0x016B67F2
static bool Tainted_slot_reservations; // 0x0374246D
static bool Spawning_flag; // 0x0374246F

// The arrays of Human_records_list and Vehicle_records_list.
static resource_slot_record Human_master_records[165]; // 0x03743430
static resource_slot_record Vehicle_master_records[84]; // 0x0374A4E8
static resource_slot_record* Human_streamed_records[17]; // 0x0373EF90
static resource_slot_record* Vehicle_streamed_records[7]; // 0x0373EFD8

resource_slot_list Human_records_list(OT_HUMAN, 165, Human_master_records, 17, Human_streamed_records); // 0x016B8AAC
resource_slot_list Vehicle_records_list(OT_VEHICLE, 84, Vehicle_master_records, 7, Vehicle_streamed_records); // 0x016B8AD0

static resource_slot_limit Master_resource_info_list[NUM_SPAWN_SLOT_TYPES] = { // 0x016B8B00
    {SPAWN_SLOT_NONE, 0},
    {SPAWN_SLOT_MISSION, 24},
    {SPAWN_SLOT_MISSION_ALLOCATED, 24},
    {SPAWN_SLOT_HUMAN_PLAYER, 1},
    {SPAWN_SLOT_HUMAN_GUERRILLA, 9},
    {SPAWN_SLOT_HUMAN_EDF, 3},
    {SPAWN_SLOT_HUMAN_MARAUDER, 2},
    {SPAWN_SLOT_HUMAN_ACTIVITY, 1},
    {SPAWN_SLOT_HUMAN_SPECIAL, 1},
    {SPAWN_SLOT_HUMAN_SAFEHOUSE_VIP, 1},
    {SPAWN_SLOT_VEHICLE_PLAYER, 1},
    {SPAWN_SLOT_VEHICLE_GUERRILLA, 1},
    {SPAWN_SLOT_VEHICLE_EDF_APC, 1},
    {SPAWN_SLOT_VEHICLE_EDF_REINFORCEMENT, 1},
    {SPAWN_SLOT_VEHICLE_ACTIVITY, 1},
    {SPAWN_SLOT_VEHICLE_SPECIAL, 1},
    {SPAWN_SLOT_VEHICLE_MULTI_PURPOSE, 1},
};

// The DLC levels trade guerrillas for marauders.
static resource_slot_limit DLC_resource_info_list[NUM_SPAWN_SLOT_TYPES] = { // 0x016B8C58
    {SPAWN_SLOT_NONE, 0},
    {SPAWN_SLOT_MISSION, 24},
    {SPAWN_SLOT_MISSION_ALLOCATED, 24},
    {SPAWN_SLOT_HUMAN_PLAYER, 1},
    {SPAWN_SLOT_HUMAN_GUERRILLA, 7},
    {SPAWN_SLOT_HUMAN_EDF, 3},
    {SPAWN_SLOT_HUMAN_MARAUDER, 4},
    {SPAWN_SLOT_HUMAN_ACTIVITY, 1},
    {SPAWN_SLOT_HUMAN_SPECIAL, 1},
    {SPAWN_SLOT_HUMAN_SAFEHOUSE_VIP, 1},
    {SPAWN_SLOT_VEHICLE_PLAYER, 1},
    {SPAWN_SLOT_VEHICLE_GUERRILLA, 1},
    {SPAWN_SLOT_VEHICLE_EDF_APC, 1},
    {SPAWN_SLOT_VEHICLE_EDF_REINFORCEMENT, 1},
    {SPAWN_SLOT_VEHICLE_ACTIVITY, 1},
    {SPAWN_SLOT_VEHICLE_SPECIAL, 1},
    {SPAWN_SLOT_VEHICLE_MULTI_PURPOSE, 1},
};

// Forward declarations of this file's statics used before their definitions.
static float get_nearest_human(unsigned int srid);
static float get_nearest_vehicle(unsigned int srid);
static int get_object_count_human(unsigned int srid);
static int get_object_count_vehicle(unsigned int srid);

// 0x00B48E40 (.h:35)
resource_slot_limit::resource_slot_limit(spawn_slot_types slot_type_, int slot_max_)
    : slot_type(slot_type_), slot_bit_pos(1 << slot_type_), slot_max(slot_max_), owner_id(RESOURCE_SLOT_USER_ID_NONE)
{
    slot_release_timestamp.set(0);
}

// 0x00B48ED0 (.h:199)
resource_slot_list::resource_slot_list(object_type obj_type_, int master_records_size_, resource_slot_record* master_record_array_, int streamed_records_size_, resource_slot_record** streamed_records_array_)
    : obj_type(obj_type_)
{
    master_records_list.init(master_record_array_, master_records_size_, 0);
    streamed_records_list.init(streamed_records_array_, streamed_records_size_, 0);
}

// 0x00B48F90
resource_slot_list::~resource_slot_list()
{
}

// 0x00B43E40 (:233)
int get_spawn_slot_type_bit_flag(spawn_slot_types slot_type)
{
    return 1 << slot_type;
}

// 0x00B43E50 (:237)
static resource_slot_limit& find_slot_limit_info(spawn_slot_types slot_type)
{
    if ((int)slot_type > 0 && (int)slot_type < NUM_SPAWN_SLOT_TYPES)
    {
        return World.dlc_bundle_id != -1 ? DLC_resource_info_list[slot_type] : Master_resource_info_list[slot_type];
    }
    return Master_resource_info_list[0];
}

// 0x00B51A70 (:300)
static resource_slot_record* get_resource_record(const human_info* h_info)
{
    return h_info != nullptr ? Human_records_list.get_record(h_info->slot_id) : nullptr;
}

// 0x00B51AA0 (:310)
static resource_slot_record* get_resource_record(const vehicle_info* v_info)
{
    return v_info != nullptr ? Vehicle_records_list.get_record(v_info->slot_id) : nullptr;
}

// 0x00B43E80 (:349)
static unsigned int get_object_sr_id(const vehicle* vp)
{
    return vp->info->srid;
}

// 0x00B43E90 (:355)
static unsigned int get_object_sr_id(const human* hp)
{
    return hp->h_info->srid;
}

// 0x00B51AD0 (:379)
static void destroy_loaded_objects_human(unsigned int srid)
{
    const int count = World.get_num_objects<human>();
    for (int i = 0; i < count; i++)
    {
        human* hp = World.get_object<human>(i);
        if (srid == get_object_sr_id(hp))
        {
            World.destroy_object(hp, false);
        }
    }
}

// 0x00B51B40 (:397)
// The vehicles of the container, and every human in one of them.
static void destroy_loaded_objects_vehicle(unsigned int srid)
{
    int count = World.get_num_objects<vehicle>();
    for (int i = 0; i < count; i++)
    {
        vehicle* vp = World.get_object<vehicle>(i);
        if (srid == get_object_sr_id(vp))
        {
            World.destroy_object(vp, false);
        }
    }
    count = World.get_num_objects<human>();
    for (int i = 0; i < count; i++)
    {
        human* hp = World.get_object<human>(i);
        if (hp->vehicle_handle == 0xFFFFFFFF)
        {
            continue;
        }
        object* objp = World.object_from_handle(hp->vehicle_handle);
        if (objp != nullptr && objp->get_obj_type() == OT_VEHICLE && srid == get_object_sr_id((vehicle*)objp))
        {
            World.destroy_object(hp, false);
        }
    }
}

// 0x00B54190 (:438)
static bool resources_reserved_or_allocated(const vehicle_info* obj_info)
{
    resource_slot_record* record = get_resource_record(obj_info);
    return record != nullptr && (record->is_reserved() || record->is_allocated());
}

// 0x00B542E0 (:452)
// 0x00B54340 (:452)
template<typename T>
static bool resources_loaded(const T* obj_info)
{
    resource_slot_record* record = get_resource_record(obj_info);
    return record != nullptr && stream2_get_status(record->get_srid()) == ST_LOADED && record->can_be_used();
}

// 0x00B57320 (:472)
// True when another variant of the vehicle's family holds the family's slot and this one does not.
static bool object_variant_loaded(const vehicle_info* veh_info)
{
    if (resources_reserved_or_allocated(veh_info) || INVALID_VEHICLE_FAMILY_ID == veh_info->variant_family)
    {
        return false;
    }
    farray<vehicle_info const*, 163> family;
    if (!vehicle_collect_family(family, veh_info->variant_family, false))
    {
        return false;
    }
    for (int i = 0; i < family.size(); i++)
    {
        if (veh_info->srid != family[i]->srid && resources_reserved_or_allocated(family[i]))
        {
            return true;
        }
    }
    return false;
}

// 0x00B573F0 (:511)
// Original: every pass looks up veh_info itself, not the master list's entry, so record == other and nothing is destroyed.
static bool destroy_object_variants(const vehicle_info* veh_info)
{
    resource_slot_record* record = get_resource_record(veh_info);
    if (record == nullptr)
    {
        return false;
    }
    const farray<vehicle_info, 163>& list = vehicle_info_get_master_list();
    for (int i = 0; i < list.size(); i++)
    {
        resource_slot_record* other = get_resource_record(veh_info);
        if (other != nullptr && record != other && (other->is_reserved() || other->is_allocated()))
        {
            Vehicle_records_list.destroy_slot(record);
        }
    }
    return true;
}

// 0x00B43EA0 (:538)
static bool object_variant_loaded(const human_info* hum_info)
{
    return false;
}

// 0x00B43EB0 (:544)
static bool destroy_object_variants(const human_info* hum_info)
{
    keen::debug::nativePrintArgs("Int3 critical function at %s(%d)\n", __FILE__, __LINE__);
    return false;
}

// 0x00B5CD00 (:553)
static void allocate_ambient_spawn_slots_human()
{
    int free_slots = Human_records_list.count_free_slots();
    if (Desired_guerrilla_list.size() < 1 || free_slots < 1)
    {
        return;
    }
    for (int i = 0; i < Desired_guerrilla_list.size(); i++)
    {
        const human_info* info = Desired_guerrilla_list[i];
        if (resources_loaded(info))
        {
            continue;
        }
        resource_slot_record* record = get_resource_record(info);
        if (record == nullptr)
        {
            return;
        }
        Human_records_list.allocate_slot(record);
        free_slots--;
        if (free_slots < 1)
        {
            return;
        }
    }
}

// 0x00B57480 (:597)
static bool variant_resource_loaded(const vehicle_info* v_info)
{
    const farray<vehicle_info, 163>& list = vehicle_info_get_master_list();
    for (int i = 0; i < list.size(); i++)
    {
        const vehicle_info* other = &list[i];
        if (v_info == other || v_info->variant_family != other->variant_family)
        {
            continue;
        }
        resource_slot_record* record = Vehicle_records_list.get_record(other->slot_id);
        if (record != nullptr && (record->is_reserved() || record->is_allocated()))
        {
            return true;
        }
    }
    return false;
}

// 0x00B57500 (:626)
static void allocate_ambient_spawn_slots_vehicle()
{
    player* pp = player_get_local_player();
    if (pp == nullptr)
    {
        return;
    }
    obj_zone* zone = World.get_zone_by_pos(pp->pos);
    if (zone == nullptr)
    {
        return;
    }
    const ambient_spawn_info* info = zone->amb_spawn_info;
    if (info == nullptr || info->h_spawn_group == nullptr || !info->ambient_flags.veh_spawn_enabled)
    {
        return;
    }
    farray<vehicle_info const*, 256> candidates;
    const vehicle_spawn_group_info* group = info->v_spawn_group;
    for (int i = 0; i < group->v_info_list.size(); i++)
    {
        const vehicle_info* v_info = group->v_info_list[i];
        resource_slot_record* record = get_resource_record(v_info);
        if (record == nullptr || (!record->is_reserved() && !record->is_allocated()))
        {
            // Original: the result of variant_resource_loaded is not used.
            variant_resource_loaded(v_info);
            candidates.add(v_info);
        }
    }
    if (candidates.size() < 1)
    {
        return;
    }
    const int free_slots = Vehicle_records_list.count_free_slots();
    int tries = candidates.size() <= free_slots ? candidates.size() : free_slots;
    for (int n = 0; n < tries && candidates.size() > 0; n++)
    {
        const int pick = candidates.size() == 1 ? 0 : int_rand_range(0, candidates.size() - 1);
        const vehicle_info* v_info = candidates[pick];
        candidates.remove_unsorted(pick);
        if (object_variant_loaded(v_info))
        {
            continue;
        }
        resource_slot_record* record = get_resource_record(v_info);
        if (record == nullptr)
        {
            return;
        }
        Vehicle_records_list.allocate_slot(record);
        // One variant of a family at a time.
        for (int i = 0; i < candidates.size();)
        {
            if (candidates[i]->variant_family == v_info->variant_family)
            {
                candidates.remove_unsorted(i);
            }
            else
            {
                i++;
            }
        }
    }
}

// 0x00B57700 (:763)
// LTCG: v_info in EAX, slot_type in EDX.
static bool reserve_resource_slot(const vehicle_info* v_info, spawn_slot_types slot_type)
{
    if (v_info == nullptr)
    {
        return false;
    }
    return Vehicle_records_list.reserve_slot(Vehicle_records_list.get_record(v_info->slot_id), slot_type);
}

// 0x00B51C30 (:777)
// The nearest special squad or NPC node in range of the player.
static const object* find_local_special_npc_nodes()
{
    const object* best = nullptr;
    float best_distance = FLT_MAX;
    player* pp = player_get_local_player();
    for (int i = 0; i < World.get_num_objects<object_squad_spawn_node>(); i++)
    {
        object_squad_spawn_node* node = World.get_object<object_squad_spawn_node>(i);
        if (node->is_special_spawn_npc())
        {
            const float distance = vec_dist_squared(pp->pos, node->pos);
            if (distance < best_distance)
            {
                best = node;
                best_distance = distance;
            }
        }
    }
    for (int i = 0; i < World.get_num_objects<object_npc_spawn_node>(); i++)
    {
        object_npc_spawn_node* node = World.get_object<object_npc_spawn_node>(i);
        if (node->is_special_spawn_node())
        {
            const float distance = vec_dist_squared(pp->pos, node->pos);
            if (distance < best_distance)
            {
                best = node;
                best_distance = distance;
            }
        }
    }
    return best != nullptr && best_distance < SPECIAL_NODE_RANGE_SQUARED ? best : nullptr;
}

// 0x00B51D90 (:832)
// As for NPCs, and failing that, any special squad node whose vehicle group starts with vehicle class 1.
static const object* find_local_special_vehicle_nodes()
{
    const object* best = nullptr;
    float best_distance = FLT_MAX;
    player* pp = player_get_local_player();
    for (int i = 0; i < World.get_num_objects<object_squad_spawn_node>(); i++)
    {
        object_squad_spawn_node* node = World.get_object<object_squad_spawn_node>(i);
        if (node->is_special_spawn_vehicle())
        {
            const float distance = vec_dist_squared(pp->pos, node->pos);
            if (distance < best_distance)
            {
                best = node;
                best_distance = distance;
            }
        }
    }
    for (int i = 0; i < World.get_num_objects<object_vehicle_spawn_node>(); i++)
    {
        object_vehicle_spawn_node* node = World.get_object<object_vehicle_spawn_node>(i);
        if (node->is_special_spawn_node())
        {
            const float distance = vec_dist_squared(pp->pos, node->pos);
            if (distance < best_distance)
            {
                best = node;
                best_distance = distance;
            }
        }
    }
    if (best != nullptr && best_distance < SPECIAL_NODE_RANGE_SQUARED)
    {
        return best;
    }
    for (int i = 0; i < World.get_num_objects<object_squad_spawn_node>(); i++)
    {
        object_squad_spawn_node* node = World.get_object<object_squad_spawn_node>(i);
        const vehicle_spawn_group_info* group = node->get_veh_info();
        if (node->is_special_spawn_vehicle() && group != nullptr && group->v_info_list.size() > 0 && group->v_info_list[0]->vehicle_class == 1)
        {
            return node;
        }
    }
    return nullptr;
}

// 0x00B4B560 (:913)
// 0x00B4B4F0 (:913)
static void evaluate_special_spawn_nodes_npc()
{
    for (int i = 0; i < World.get_num_objects<object_npc_spawn_node>(); i++)
    {
        object_npc_spawn_node* node = World.get_object<object_npc_spawn_node>(i);
        if (node->is_special_spawn_node())
        {
            node->evaluate_spawn();
        }
    }
    for (int i = 0; i < World.get_num_objects<object_squad_spawn_node>(); i++)
    {
        object_squad_spawn_node* node = World.get_object<object_squad_spawn_node>(i);
        if (node->is_special_spawn_npc())
        {
            node->evaluate_spawn();
        }
    }
}

// 0x00B4B640 (:924)
// 0x00B4B5D0 (:924)
static void evaluate_special_spawn_nodes_vehicle()
{
    for (int i = 0; i < World.get_num_objects<object_vehicle_spawn_node>(); i++)
    {
        object_vehicle_spawn_node* node = World.get_object<object_vehicle_spawn_node>(i);
        if (node->is_special_spawn_node())
        {
            node->evaluate_spawn();
        }
    }
    for (int i = 0; i < World.get_num_objects<object_squad_spawn_node>(); i++)
    {
        object_squad_spawn_node* node = World.get_object<object_squad_spawn_node>(i);
        if (node->is_special_spawn_vehicle())
        {
            node->evaluate_spawn();
        }
    }
}

// 0x00B5CD80 (:937)
static void srm_release_special_slot_human(resource_slot_record* human_record, bool destroy_slot)
{
    Human_records_list.unreserve_slot(human_record, SPAWN_SLOT_HUMAN_SPECIAL);
    if (human_record->is_reserved())
    {
        return;
    }
    player* pp = player_get_local_player();
    const int count = World.get_num_objects<npc>();
    for (int i = 0; i < count; i++)
    {
        npc* np = World.get_object<npc>(i);
        if (human_record->get_srid() != get_object_sr_id(np))
        {
            continue;
        }
        np->destroy_on_stream();
        if (destroy_slot || vec_dist_squared(pp->pos, np->pos) >= SPECIAL_OBJECT_KEEP_RANGE_SQUARED)
        {
            World.destroy_object(np, false);
        }
    }
}

// 0x00B48FA0 (:973)
// LTCG: special_objp in EAX, human_record in ECX.
static bool has_human_reserved(const resource_slot_record* human_record, const object* special_objp)
{
    if (special_objp == nullptr)
    {
        return false;
    }
    if (special_objp->get_obj_type() == OT_SQUAD_SPAWN_NODE)
    {
        return human_record->get_srid() == ((const object_squad_spawn_node*)special_objp)->get_info()->special_npc_info->srid;
    }
    if (special_objp->get_obj_type() == OT_NPC_SPAWN_NODE)
    {
        const human_spawn_group_info* group = ((const object_npc_spawn_node*)special_objp)->get_info();
        if (group != nullptr)
        {
            for (int i = 0; i < group->h_info_list.size(); i++)
            {
                if (human_record->get_srid() == group->h_info_list[i]->srid)
                {
                    return true;
                }
            }
        }
    }
    return false;
}

// 0x00B49010 (:1005)
static const human_info* pick_human_info(const object* special_objp)
{
    if (special_objp == nullptr)
    {
        return nullptr;
    }
    if (special_objp->get_obj_type() == OT_SQUAD_SPAWN_NODE)
    {
        return ((const object_squad_spawn_node*)special_objp)->get_info()->special_npc_info;
    }
    if (special_objp->get_obj_type() == OT_NPC_SPAWN_NODE)
    {
        const human_spawn_group_info* group = ((const object_npc_spawn_node*)special_objp)->get_info();
        return group != nullptr ? group->get_random_element() : nullptr;
    }
    return nullptr;
}

// 0x00B5CE70 (:1034)
// LTCG: human_record in EDI.
static void srm_special_human_reserved(resource_slot_record* human_record)
{
    const object* special_objp = find_local_special_npc_nodes();
    if (special_objp == nullptr)
    {
        srm_release_special_slot_human(human_record, false);
        return;
    }
    if (has_human_reserved(human_record, special_objp))
    {
        return;
    }
    resource_slot_record* record = get_resource_record(pick_human_info(special_objp));
    if (record != nullptr)
    {
        srm_release_special_slot_human(human_record, true);
        Human_records_list.reserve_slot(record, SPAWN_SLOT_HUMAN_SPECIAL);
        evaluate_special_spawn_nodes_npc();
    }
}

// 0x00B5CEE0 (:1073)
static void srm_special_human_free()
{
    const object* special_objp = find_local_special_npc_nodes();
    if (special_objp == nullptr)
    {
        return;
    }
    resource_slot_record* record = get_resource_record(pick_human_info(special_objp));
    if (record == nullptr || Human_records_list.count_free_slots() < 0)
    {
        return;
    }
    if (Human_records_list.count_free_slots() > 0 || Human_records_list.free_low_priority_slots(1) > 0)
    {
        Human_records_list.reserve_slot(record, SPAWN_SLOT_HUMAN_SPECIAL);
        evaluate_special_spawn_nodes_npc();
    }
}

// 0x00B5CF60 (:1115)
static void srm_release_special_slot_vehicle(resource_slot_record* vehicle_record, bool destroy_slot)
{
    Vehicle_records_list.unreserve_slot(vehicle_record, SPAWN_SLOT_VEHICLE_SPECIAL);
    if (vehicle_record->is_reserved())
    {
        return;
    }
    player* pp = player_get_local_player();
    const int count = World.get_num_objects<vehicle>();
    for (int i = 0; i < count; i++)
    {
        vehicle* vp = World.get_object<vehicle>(i);
        if (vehicle_record->get_srid() != get_object_sr_id(vp))
        {
            continue;
        }
        vp->destroy_on_stream();
        if (destroy_slot || vec_dist_squared(pp->pos, vp->pos) >= SPECIAL_OBJECT_KEEP_RANGE_SQUARED)
        {
            World.destroy_object(vp, false);
        }
    }
}

// 0x00B49070 (:1151)
static const vehicle_spawn_group_info* get_group_from_object(const object* special_objp)
{
    if (special_objp != nullptr)
    {
        if (special_objp->get_obj_type() == OT_VEHICLE_SPAWN_NODE)
        {
            return ((const object_vehicle_spawn_node*)special_objp)->get_info();
        }
        if (special_objp->get_obj_type() == OT_SQUAD_SPAWN_NODE)
        {
            return ((const object_squad_spawn_node*)special_objp)->get_veh_info();
        }
    }
    keen::debug::nativePrintArgs("Int3 critical function at %s(%d)\n", __FILE__, __LINE__);
    return nullptr;
}

// 0x00B490B0 (:1172)
static const char* find_vehicle_family(const resource_slot_record* record)
{
    if (record->get_obj_type() == OT_VEHICLE)
    {
        const farray<vehicle_info, 163>& list = vehicle_info_get_master_list();
        for (int i = 0; i < list.size(); i++)
        {
            if (list[i].srid == record->get_srid())
            {
                return list[i].variant_family;
            }
        }
        keen::debug::nativePrintArgs("Int3 critical function at %s(%d)\n", __FILE__, __LINE__);
    }
    return INVALID_VEHICLE_FAMILY_ID;
}

// 0x00B5D050 (:1198)
static void srm_special_vehicle_reserved(resource_slot_record* vehicle_record)
{
    const object* special_objp = find_local_special_vehicle_nodes();
    if (special_objp == nullptr)
    {
        srm_release_special_slot_vehicle(vehicle_record, false);
        return;
    }
    const vehicle_spawn_group_info* group = get_group_from_object(special_objp);
    if (group == nullptr)
    {
        srm_release_special_slot_vehicle(vehicle_record, false);
        return;
    }
    // Already holding one of the group's vehicles.
    for (int i = 0; i < group->v_info_list.size(); i++)
    {
        if (vehicle_record->get_srid() == group->v_info_list[i]->srid)
        {
            return;
        }
    }
    farray<vehicle_info const*, 256> candidates;
    for (int i = 0; i < group->v_info_list.size(); i++)
    {
        const vehicle_info* info = group->v_info_list[i];
        resource_slot_record* record = get_resource_record(info);
        if (record != nullptr && !Vehicle_records_list.other_variant_loaded(record))
        {
            candidates.add(info);
        }
    }
    if (candidates.size() < 1)
    {
        return;
    }
    const int pick = candidates.size() == 1 ? 0 : int_rand_range(0, candidates.size() - 1);
    resource_slot_record* record = get_resource_record(candidates[pick]);
    if (record != nullptr)
    {
        srm_release_special_slot_vehicle(vehicle_record, true);
        Vehicle_records_list.reserve_slot(record, SPAWN_SLOT_VEHICLE_SPECIAL);
        evaluate_special_spawn_nodes_vehicle();
    }
}

// 0x00B546C0 (:1278)
static const vehicle_info* find_reserved_or_allocated_record(const base_array<vehicle_info const*>& obj_info_list)
{
    for (int i = 0; i < obj_info_list.size(); i++)
    {
        if (resources_reserved_or_allocated(obj_info_list[i]))
        {
            return obj_info_list[i];
        }
    }
    return nullptr;
}

// 0x00B58B80 (:1300)
static const vehicle_info* select_new_special_object_type(const base_array<vehicle_info const*>& obj_info_list, resource_slot_list& slot_list)
{
    farray<vehicle_info const*, 256> unused;
    for (int i = 0; i < obj_info_list.size(); i++)
    {
        const vehicle_info* info = obj_info_list[i];
        resource_slot_record* record = get_resource_record(info);
        if (record != nullptr && !record->is_allocated() && !record->is_reserved() && !slot_list.other_variant_loaded(record))
        {
            unused.add(info);
        }
    }
    if (unused.size() > 0)
    {
        return unused[unused.size() == 1 ? 0 : int_rand_range(0, unused.size() - 1)];
    }
    return find_reserved_or_allocated_record(obj_info_list);
}

// 0x00B5D290 (:1342)
static void srm_special_vehicle_free()
{
    const object* special_objp = find_local_special_vehicle_nodes();
    if (special_objp == nullptr)
    {
        return;
    }
    const vehicle_spawn_group_info* group = get_group_from_object(special_objp);
    if (group == nullptr || group->v_info_list.size() < 1)
    {
        return;
    }
    const vehicle_info* v_info = select_new_special_object_type(group->v_info_list, Vehicle_records_list);
    resource_slot_record* record = get_resource_record(v_info);
    if (record == nullptr)
    {
        return;
    }
    if (record->is_reserved() || record->is_allocated() || Vehicle_records_list.count_free_slots() > 0 || (Vehicle_records_list.count_free_slots() == 0 && Vehicle_records_list.free_low_priority_slots(1) > 0))
    {
        Vehicle_records_list.reserve_slot(record, SPAWN_SLOT_VEHICLE_SPECIAL);
        evaluate_special_spawn_nodes_vehicle();
    }
}

// 0x00B5D360 (:1401)
void srm_do_frame_special_slot()
{
    if (!Special_slot_timer.elapsed())
    {
        return;
    }
    // Magic numbers: half a second to a second.
    Special_slot_timer.set_rand(500, 1000);
    resource_slot_record* human_record = Human_records_list.find_first_reserved_record(SPAWN_SLOT_HUMAN_SPECIAL);
    resource_slot_record* vehicle_record = Vehicle_records_list.find_first_reserved_record(SPAWN_SLOT_VEHICLE_SPECIAL);
    if (human_record == nullptr)
    {
        srm_special_human_free();
    }
    else
    {
        srm_special_human_reserved(human_record);
    }
    if (vehicle_record == nullptr)
    {
        srm_special_vehicle_free();
    }
    else
    {
        srm_special_vehicle_reserved(vehicle_record);
    }
}

// 0x00B54720 (:2829)
// 0x00B54780 (:2829)
template<typename T>
static bool record_on_list(const resource_slot_record* record, const base_array<T const*>& obj_info_list)
{
    for (int i = 0; i < obj_info_list.size(); i++)
    {
        if (record == get_resource_record(obj_info_list[i]))
        {
            return true;
        }
    }
    return false;
}

// 0x00B58CA0 (:2845)
// 0x00B58D90 (:2845)
// Slots neither reserved nor on the list, with no object left, are freed.
template<typename T>
static void free_unused_slots(resource_slot_list& list, base_array<resource_slot_record*>& streamed, const base_array<T const*>& obj_info_list)
{
    farray<resource_slot_record*, 23> copy;
    copy.copy(streamed);
    for (int i = 0; i < copy.size(); i++)
    {
        resource_slot_record* record = copy[i];
        if (!record->is_reserved() && !record_on_list<T>(record, obj_info_list) && record->get_object_count() < 1)
        {
            list.destroy_slot(record);
        }
    }
}

// 0x00B5D420 (:1498)
// Humans and vehicles take turns, four times a second at most.
static void srm_do_frame_slot_clean_up()
{
    player* pp = player_get_local_player();
    if (pp == nullptr || !Slot_release_period.elapsed())
    {
        return;
    }
    // Magic numbers: a quarter to half a second.
    Slot_release_period.set_rand(250, 500);
    if (Slot_release_humans)
    {
        if (pp->current_team == HUMAN_TEAM_GUERILLA)
        {
            Human_records_list.free_unused_human_slots(Desired_guerrilla_list);
        }
        else
        {
            Human_records_list.free_unused_human_slots(sog_default_human_spawn_group(pp->current_team)->h_info_list);
        }
    }
    else
    {
        Vehicle_records_list.free_unused_vehicle_slots(Desired_vehicle_list);
    }
    Slot_release_humans = !Slot_release_humans;
}

// 0x00B43ED0 (:1529)
static bool player_slot_locked()
{
    return activity_running_or_shutting_down(ACTIVITY_MODE_DEMOLITIONS_MASTER);
}

// 0x00B5D4E0 (:1540)
// The vehicle the player drives keeps its container.
static void srm_do_frame_player()
{
    if (player_slot_locked())
    {
        return;
    }
    player* pp = player_get_local_player();
    if (pp == nullptr || pp->vehicle_handle == 0xFFFFFFFF)
    {
        return;
    }
    object* objp = World.object_from_handle(pp->vehicle_handle);
    if (objp == nullptr || objp->get_obj_type() != OT_VEHICLE)
    {
        return;
    }
    resource_slot_record* record = get_resource_record(((vehicle*)objp)->info);
    if (record == nullptr)
    {
        return;
    }
    resource_slot_record* old_record = Vehicle_records_list.find_first_reserved_record(SPAWN_SLOT_VEHICLE_PLAYER);
    if (old_record != nullptr)
    {
        if (old_record == record)
        {
            return;
        }
        Vehicle_records_list.unreserve_slot(old_record, SPAWN_SLOT_VEHICLE_PLAYER);
    }
    Vehicle_records_list.reserve_slot(record, SPAWN_SLOT_VEHICLE_PLAYER);
}

// 0x00B43EE0 (:2124)
void resource_slot_limit::reset()
{
    owner_id = RESOURCE_SLOT_USER_ID_NONE;
    slot_release_timestamp.set(0);
}

// 0x00B51F50 (:2131)
int resource_slot_limit::count_used_slots() const
{
    if (slot_type == SPAWN_SLOT_NONE)
    {
        return 0;
    }
    const int used = Human_records_list.count_slots_used_by_type(slot_type) + Vehicle_records_list.count_slots_used_by_type(slot_type);
    return used > slot_max ? slot_max : used;
}

// 0x00B51FE0 (:2146)
int resource_slot_limit::count_free_slots() const
{
    return slot_max - count_used_slots();
}

// 0x00B43F00 (:2152)
bool resource_slot_limit::is_allowed_to_use(resource_slot_user_id new_owner_id) const
{
    return owner_id == RESOURCE_SLOT_USER_ID_NONE || owner_id == new_owner_id;
}

// 0x00B43F20 (:2167)
resource_slot_record::resource_slot_record()
{
    destroyed_time.set(1000);
    memset(&slot_flags, 0, sizeof(slot_flags));
    srid = 0;
    reservation_flags = 0;
    obj_type = OT_UNDEFINED;
}

// 0x00B43F60 (:2177)
resource_slot_record::resource_slot_record(unsigned int new_srid, object_type new_obj_type)
{
    destroyed_time.set(1000);
    memset(&slot_flags, 0, sizeof(slot_flags));
    reservation_flags = 0;
    srid = new_srid;
    obj_type = new_obj_type;
}

// 0x00B43FA0 (:2188)
void resource_slot_record::reset(unsigned int new_srid, object_type new_obj_type)
{
    reservation_time.invalidate();
    destroyed_time.set(1000);
    memset(&slot_flags, 0, sizeof(slot_flags));
    srid = new_srid;
    reservation_flags = 0;
    obj_type = new_obj_type;
}

// 0x00B49110 (:2199)
void resource_slot_record::reset_internal()
{
    stream_release();
    reset(srid, obj_type);
}

// 0x00B43FD0 (:2209)
bool resource_slot_record::is_reserved() const
{
    return reservation_time.valid();
}

// 0x00B43FE0 (:2224)
bool resource_slot_record::is_allocated() const
{
    return slot_flags.allocated_slot != 0;
}

// 0x00B43DD0 (.h:81)
void resource_slot_record::set_allocated_flag()
{
    slot_flags.allocated_slot = 1;
}

void resource_slot_record::clear_allocated_flag()
{
    slot_flags.allocated_slot = 0;
}

// 0x00B48E80 (.h:85)
bool resource_slot_record::high_priority() const
{
    return is_reserved();
}

// 0x00B48E90 (.h:96)
bool resource_slot_record::low_priority() const
{
    return !is_reserved();
}

// 0x00B43FF0 (:2235)
bool resource_slot_record::is_reserved_by(spawn_slot_types slot_type) const
{
    return (reservation_flags & (1 << slot_type)) != 0;
}

// 0x00B44010 (:2241)
bool resource_slot_record::is_reserved_by_other(spawn_slot_types slot_type) const
{
    return (reservation_flags & ~(1 << slot_type)) != 0;
}

// 0x00B44030 (:2247)
void resource_slot_record::set_reservation_flag(spawn_slot_types slot_type)
{
    slot_flags.allocated_slot = 0;
    reservation_flags |= 1 << slot_type;
    if (!reservation_time.valid())
    {
        reservation_time.set(0);
        destroyed_time.invalidate();
    }
}

// 0x00B44070 (:2257)
void resource_slot_record::clear_reservation_flag(spawn_slot_types slot_type)
{
    reservation_flags &= ~(1 << slot_type);
    if (reservation_flags == 0)
    {
        reservation_time.invalidate();
    }
}

// 0x00B44090 (:2267)
bool resource_slot_record::in_use() const
{
    return srid != 0 && (reservation_time.valid() || slot_flags.allocated_slot != 0);
}

// 0x00B440C0 (:2278)
bool resource_slot_record::can_be_used() const
{
    return srid != 0 && (reservation_time.valid() || slot_flags.allocated_slot != 0);
}

// 0x00B43DE0 (.h:109)
unsigned int resource_slot_record::get_srid() const
{
    return srid;
}

// 0x00B440F0 (:2284)
bool resource_slot_record::stream_request()
{
    if (slot_flags.stream_requested == 0)
    {
        slot_flags.stream_requested = stream2_request(srid) ? 1 : 0;
    }
    return slot_flags.stream_requested != 0;
}

// 0x00B44130 (:2298)
void resource_slot_record::stream_release()
{
    if (slot_flags.stream_requested != 0)
    {
        stream2_release(srid);
        slot_flags.stream_requested = 0;
    }
}

bool resource_slot_record::stream_requested() const
{
    return slot_flags.stream_requested != 0;
}

// 0x00B44150 (:2310)
void resource_slot_record::clear_reservation_flags()
{
    reservation_flags = 0;
    reservation_time.invalidate();
}

// 0x00B43DF0 (.h:134)
object_type resource_slot_record::get_obj_type() const
{
    return obj_type;
}

// 0x00B43E00 (.h:137)
void resource_slot_record::set_player_flag()
{
    slot_flags.player_slot = 1;
}

// 0x00B43E10 (.h:140)
bool resource_slot_record::is_player_slot() const
{
    return slot_flags.player_slot != 0;
}

// 0x00B49160 (:2317)
const char* resource_slot_record::get_record_name() const
{
    if (obj_type == OT_HUMAN)
    {
        const farray<human_info, 165>& list = human_info_get_master_list();
        for (int i = 0; i < list.size(); i++)
        {
            if (list[i].srid == srid)
            {
                return list[i].name;
            }
        }
    }
    else if (obj_type == OT_VEHICLE)
    {
        const farray<vehicle_info, 163>& list = vehicle_info_get_master_list();
        for (int i = 0; i < list.size(); i++)
        {
            if (list[i].srid == srid)
            {
                return list[i].name;
            }
        }
    }
    return "[unknown name]";
}

// 0x00B44160 (:2351)
bool resource_slot_record::check_destroyed_status() const
{
    return destroyed_time.valid() && destroyed_time.elapsed();
}

// 0x00B52000 (:2363)
// The squared distance from the local player to the nearest human of the container.
static float get_nearest_human(unsigned int srid)
{
    player* pp = player_get_local_player();
    float nearest = FLT_MAX;
    const int count = World.get_num_objects<human>();
    for (int i = 0; i < count; i++)
    {
        human* hp = World.get_object<human>(i);
        if (get_object_sr_id(hp) == srid)
        {
            const float distance = vec_dist_squared(hp->pos, pp->pos);
            if (distance < nearest)
            {
                nearest = distance;
            }
        }
    }
    return nearest;
}

// 0x00B520B0 (:2389)
static float get_nearest_vehicle(unsigned int srid)
{
    player* pp = player_get_local_player();
    float nearest = FLT_MAX;
    const int count = World.get_num_objects<vehicle>();
    for (int i = 0; i < count; i++)
    {
        vehicle* vp = World.get_object<vehicle>(i);
        if (get_object_sr_id(vp) == srid)
        {
            const float distance = vec_dist_squared(vp->pos, pp->pos);
            if (distance < nearest)
            {
                nearest = distance;
            }
        }
    }
    return nearest;
}

// 0x00B52160 (:2416)
static int get_object_count_human(unsigned int srid)
{
    int total = 0;
    const int count = World.get_num_objects<human>();
    for (int i = 0; i < count; i++)
    {
        human* hp = World.get_object<human>(i);
        if (srid == get_object_sr_id(hp) && !hp->obj_flags.destroyed)
        {
            total++;
        }
    }
    return total;
}

// 0x00B521C0 (:2434)
static int get_object_count_vehicle(unsigned int srid)
{
    int total = 0;
    const int count = World.get_num_objects<vehicle>();
    for (int i = 0; i < count; i++)
    {
        vehicle* vp = World.get_object<vehicle>(i);
        if (srid == get_object_sr_id(vp) && !vp->obj_flags.destroyed)
        {
            total++;
        }
    }
    return total;
}

// 0x00B52220 (:2452)
int resource_slot_record::get_object_count() const
{
    if (obj_type == OT_HUMAN)
    {
        return get_object_count_human(srid);
    }
    if (obj_type == OT_VEHICLE)
    {
        return get_object_count_vehicle(srid);
    }
    keen::debug::nativePrintArgs("Int3 critical function at %s(%d)\n", __FILE__, __LINE__);
    return get_object_count_human(srid) + get_object_count_vehicle(srid);
}

// 0x00B52280 (:2467)
float resource_slot_record::get_object_removal_fitness() const
{
    if (obj_type == OT_HUMAN)
    {
        return get_nearest_human(srid);
    }
    if (obj_type == OT_VEHICLE)
    {
        return get_nearest_vehicle(srid);
    }
    keen::debug::nativePrintArgs("Int3 critical function at %s(%d)\n", __FILE__, __LINE__);
    const float human_distance = get_nearest_human(srid);
    const float vehicle_distance = get_nearest_vehicle(srid);
    return vehicle_distance - human_distance < 0.0f ? vehicle_distance : human_distance;
}

// 0x00B52330 (:2483)
int resource_slot_record::get_destroyed_count() const
{
    int total = 0;
    for (int i = 0; i < World.get_num_objects<human>(); i++)
    {
        human* hp = World.get_object<human>(i);
        if (srid == get_object_sr_id(hp) && hp->obj_flags.destroyed)
        {
            total++;
        }
    }
    for (int i = 0; i < World.get_num_objects<vehicle>(); i++)
    {
        vehicle* vp = World.get_object<vehicle>(i);
        if (srid == get_object_sr_id(vp) && vp->obj_flags.destroyed)
        {
            total++;
        }
    }
    return total;
}

// 0x00B5D580 (:2516)
void resource_slot_list::clear_all_reservations_except(spawn_slot_types exception_type)
{
    for (int i = 0; i < streamed_records_list.size(); i++)
    {
        if (!Mission_mode)
        {
            unreserve_slot_except(streamed_records_list[i], exception_type);
        }
    }
}

// 0x00B57740 (:2531)
// Frees up to count unreserved slots, the ones whose objects are farthest from the player first.
int resource_slot_list::free_low_priority_slots(int count)
{
    farray<resource_slot_record*, 16> candidates;
    farray<resource_slot_record*, 16> freed;
    for (int i = 0; i < streamed_records_list.size(); i++)
    {
        resource_slot_record* record = streamed_records_list[i];
        if (record->in_use() && !record->is_reserved())
        {
            candidates.add(record);
        }
    }
    if (count < candidates.size())
    {
        while (candidates.size() > 0 && freed.size() < count)
        {
            int best = 0;
            float best_fitness = candidates[0]->get_object_removal_fitness();
            for (int i = 1; i < candidates.size(); i++)
            {
                const float fitness = candidates[i]->get_object_removal_fitness();
                if (best_fitness < fitness)
                {
                    best = i;
                    best_fitness = fitness;
                }
            }
            freed.add(candidates[best]);
            candidates.remove_unsorted(best);
        }
    }
    else
    {
        for (int i = 0; i < candidates.size(); i++)
        {
            freed.add(candidates[i]);
        }
    }
    for (int i = 0; i < freed.size(); i++)
    {
        destroy_slot(freed[i]);
    }
    return freed.size();
}

// 0x00B523E0 (:2593)
void resource_slot_list::destroy_slot(resource_slot_record* record)
{
    if (obj_type != record->get_obj_type())
    {
        return;
    }
    if (obj_type == OT_HUMAN)
    {
        destroy_loaded_objects_human(record->get_srid());
    }
    else if (obj_type == OT_VEHICLE)
    {
        destroy_loaded_objects_vehicle(record->get_srid());
    }
    else
    {
        keen::debug::nativePrintArgs("Int3 critical function at %s(%d)\n", __FILE__, __LINE__);
        destroy_loaded_objects_human(record->get_srid());
        destroy_loaded_objects_vehicle(record->get_srid());
    }
    streamed_records_list.remove_matching(record);
    record->reset_internal();
}

// 0x00B524C0 (:2628)
bool resource_slot_list::reserve_slot(resource_slot_record* record, spawn_slot_types slot_type)
{
    if (record == nullptr || (int)slot_type < 1 || (int)slot_type >= NUM_SPAWN_SLOT_TYPES || Mission_mode || obj_type != record->get_obj_type())
    {
        return false;
    }
    if (record->is_reserved_by(slot_type))
    {
        return true;
    }
    if (streamed_records_list.size() >= streamed_records_list.capacity() && !(streamed_records_list.find(record, false) > -1))
    {
        return false;
    }
    if (other_variant_loaded(record) || !record->stream_request())
    {
        return false;
    }
    record->set_reservation_flag(slot_type);
    streamed_records_list.add_unique(record);
    return true;
}

// 0x00B52570 (:2670)
void resource_slot_list::reserve_slot_player(resource_slot_record* record)
{
    if (record == nullptr || Mission_mode || obj_type != record->get_obj_type())
    {
        return;
    }
    if (!record->is_reserved_by(SPAWN_SLOT_HUMAN_PLAYER))
    {
        if (streamed_records_list.size() >= streamed_records_list.capacity() && !(streamed_records_list.find(record, false) > -1))
        {
            return;
        }
        if (other_variant_loaded(record) || !record->stream_request())
        {
            return;
        }
        record->set_reservation_flag(SPAWN_SLOT_HUMAN_PLAYER);
        streamed_records_list.add_unique(record);
    }
    record->set_player_flag();
}

// 0x00B57AC0 (:2678)
void resource_slot_list::unreserve_slot(resource_slot_record* record, spawn_slot_types slot_type)
{
    if (Mission_mode || obj_type != record->get_obj_type())
    {
        return;
    }
    record->clear_reservation_flag(slot_type);
    if (!record->is_reserved())
    {
        allocate_slot(record);
    }
}

// 0x00B57B10 (:2694)
void resource_slot_list::unreserve_slot_except(resource_slot_record* record, spawn_slot_types exception_type)
{
    if (Mission_mode)
    {
        return;
    }
    const bool keep = exception_type != SPAWN_SLOT_NONE && record->is_reserved_by(exception_type);
    record->clear_reservation_flags();
    if (keep)
    {
        record->set_reservation_flag(exception_type);
    }
    else
    {
        allocate_slot(record);
    }
}

// 0x00B525F0 (:2719)
bool resource_slot_list::allocate_slot(resource_slot_record* record)
{
    if (Mission_mode || obj_type != record->get_obj_type())
    {
        return false;
    }
    if (record->is_reserved())
    {
        return true;
    }
    if (streamed_records_list.size() >= streamed_records_list.capacity() && !(streamed_records_list.find(record, false) > -1))
    {
        return false;
    }
    if (other_variant_loaded(record) || !record->stream_request())
    {
        return false;
    }
    record->set_allocated_flag();
    streamed_records_list.add_unique(record);
    return true;
}

// 0x00B57B90 (:2758)
void resource_slot_list::free_all_unused_slots()
{
    farray<resource_slot_record*, 23> copy;
    copy.copy(streamed_records_list);
    for (int i = 0; i < copy.size(); i++)
    {
        resource_slot_record* record = copy[i];
        if (!record->is_reserved() && record->get_object_count() < 1)
        {
            destroy_slot(record);
        }
    }
}

// 0x00B57C60 (:2786)
void resource_slot_list::free_all_unused_slots_except(unsigned int exception_srid)
{
    farray<resource_slot_record*, 23> copy;
    Tainted_slot_reservations = true;
    copy.copy(streamed_records_list);
    for (int i = 0; i < copy.size(); i++)
    {
        resource_slot_record* record = copy[i];
        if (record->get_srid() != exception_srid && record->get_object_count() < 1)
        {
            destroy_slot(record);
        }
    }
}

void resource_slot_list::free_unused_human_slots(const base_array<human_info const*>& obj_info_list)
{
    free_unused_slots<human_info>(*this, streamed_records_list, obj_info_list);
}

void resource_slot_list::free_unused_vehicle_slots(const base_array<vehicle_info const*>& obj_info_list)
{
    free_unused_slots<vehicle_info>(*this, streamed_records_list, obj_info_list);
}

// 0x00B491F0 (:2880)
resource_slot_record* resource_slot_list::get_record(int slot_id)
{
    if (slot_id > -1 && slot_id < master_records_list.size())
    {
        return &master_records_list[slot_id];
    }
    return nullptr;
}

// 0x00B52680 (:2888)
void resource_slot_list::reset()
{
    while (streamed_records_list.size() > 0)
    {
        destroy_slot(streamed_records_list[0]);
    }
    for (int i = 0; i < master_records_list.size(); i++)
    {
        master_records_list[i].reset_internal();
    }
    streamed_records_list.clear();
}

// 0x00B52720 (:2907)
void resource_slot_list::level_init()
{
    reset();
}

// 0x00B52730 (:2913)
void resource_slot_list::system_init()
{
    reset();
}

// 0x00B49210 (:2919)
int resource_slot_list::count_slots_used() const
{
    return streamed_records_list.size();
}

// 0x00B49220 (:2925)
int resource_slot_list::count_free_slots() const
{
    return streamed_records_list.capacity() - streamed_records_list.size();
}

// 0x00B49230 (:2931)
resource_slot_record* resource_slot_list::find_first_reserved_record(spawn_slot_types slot_type)
{
    for (int i = 0; i < streamed_records_list.size(); i++)
    {
        if (streamed_records_list[i]->is_reserved_by(slot_type))
        {
            return streamed_records_list[i];
        }
    }
    return nullptr;
}

// 0x00B49270 (:2948)
int resource_slot_list::count_slots_used_by_type(spawn_slot_types slot_type) const
{
    int count = 0;
    for (int i = 0; i < streamed_records_list.size(); i++)
    {
        if (streamed_records_list[i]->is_reserved_by(slot_type))
        {
            count++;
        }
    }
    return count;
}

// 0x00B52740 (:2965)
void resource_slot_list::reserve_mission_allocated_slots()
{
    for (int i = 0; i < streamed_records_list.size(); i++)
    {
        resource_slot_record* record = streamed_records_list[i];
        if (!record->is_reserved())
        {
            reserve_slot(record, SPAWN_SLOT_MISSION_ALLOCATED);
        }
    }
}

// 0x00B492B0 (:2980)
bool resource_slot_list::all_resources_streamed() const
{
    for (int i = 0; i < streamed_records_list.size(); i++)
    {
        if (stream2_get_status(streamed_records_list[i]->get_srid()) != ST_LOADED)
        {
            return false;
        }
    }
    return true;
}

// 0x00B57D30 (:3007)
void resource_slot_list::release_unused_reserved_slots(spawn_slot_types slot_type)
{
    for (int i = 0; i < streamed_records_list.size(); i++)
    {
        resource_slot_record* record = streamed_records_list[i];
        if (record->is_reserved_by(slot_type) && record->get_object_count() < 1)
        {
            unreserve_slot(record, slot_type);
        }
    }
}

// 0x00B57E20 (:3026)
void resource_slot_list::release_reserved_slots(spawn_slot_types slot_type)
{
    for (int i = 0; i < streamed_records_list.size(); i++)
    {
        resource_slot_record* record = streamed_records_list[i];
        if (record->is_reserved_by(slot_type))
        {
            unreserve_slot(record, slot_type);
        }
    }
}

// 0x00B492F0 (:3044)
// Only vehicles have variants: one of a family at a time.
bool resource_slot_list::other_variant_loaded(const resource_slot_record* record) const
{
    if (obj_type != record->get_obj_type() || obj_type != OT_VEHICLE)
    {
        return false;
    }
    const char* family = find_vehicle_family(record);
    if (INVALID_VEHICLE_FAMILY_ID == family)
    {
        return false;
    }
    for (int i = 0; i < streamed_records_list.size(); i++)
    {
        if (record != streamed_records_list[i] && family == find_vehicle_family(streamed_records_list[i]))
        {
            return true;
        }
    }
    return false;
}

// 0x00B49360 (:3083)
bool resource_slot_list::resources_streamed(spawn_slot_types slot_type) const
{
    for (int i = 0; i < streamed_records_list.size(); i++)
    {
        const resource_slot_record* record = streamed_records_list[i];
        if (record->is_reserved_by(slot_type) && stream2_get_status(record->get_srid()) != ST_LOADED)
        {
            return false;
        }
    }
    return true;
}

// 0x00B57E90 (:3114)
void resource_slot_list::check_slot_status()
{
    for (int i = 0; i < master_records_list.size(); i++)
    {
        resource_slot_record* record = &master_records_list[i];
        if (record->is_reserved() || record->is_allocated())
        {
            check_reserved_slot(record);
        }
        else
        {
            check_destroyed_slot(record);
        }
    }
}

// 0x00B527E0 (:3132)
// A container that failed to stream gives its slot up.
void resource_slot_list::check_reserved_slot(resource_slot_record* record)
{
    if (!record->is_reserved() && !record->is_allocated())
    {
        return;
    }
    switch (stream2_get_status(record->get_srid()))
    {
    case ST_UNLOADED:
    case ST_LOADING:
    case ST_LOADED:
        break;
    case ST_ABORTING:
    case ST_ERROR:
        destroy_slot(record);
        break;
    default:
        keen::debug::nativePrintArgs("Int3 critical function at %s(%d)\n", __FILE__, __LINE__);
        break;
    }
}

// 0x00B52860 (:3181)
// A second after its last use a container still loaded goes.
void resource_slot_list::check_destroyed_slot(resource_slot_record* record)
{
    if (record->is_reserved() || record->is_allocated() || !record->check_destroyed_status())
    {
        return;
    }
    switch (stream2_get_status(record->get_srid()))
    {
    case ST_UNLOADED:
    case ST_ABORTING:
        break;
    case ST_LOADING:
    case ST_LOADED:
    case ST_ERROR:
        destroy_slot(record);
        break;
    default:
        keen::debug::nativePrintArgs("Int3 critical function at %s(%d)\n", __FILE__, __LINE__);
        break;
    }
}

// 0x00B5D610 (:3306)
spawn_status_result object_spawn_do_spawn(npc_spawn_param_list& spawn_param_list)
{
    if (Multiplayer || !spawn_param_list.rm_check_ready())
    {
        return SPAWN_STATUS_FAILURE;
    }
    if (!World.is_zone_streamed(spawn_param_list.spawn_pos))
    {
        return SPAWN_STATUS_STREAMING;
    }
    for (int i = 0; i < spawn_param_list.npc_spawn_info.size(); i++)
    {
        npc_spawn_params& params = spawn_param_list.npc_spawn_info[i];
        resource_slot_record* record = get_resource_record(params.hum_info);
        if (record == nullptr || (!record->is_reserved() && !record->is_allocated()))
        {
            return SPAWN_STATUS_FAILURE;
        }
        if (stream2_get_status(record->get_srid()) != ST_LOADED)
        {
            return SPAWN_STATUS_STREAMING;
        }
        params.spawn_flags.rm_checked = true;
    }
    Spawning_flag = true;
    const bool spawned = sp_do_spawn(spawn_param_list);
    Spawning_flag = false;
    return spawned ? SPAWN_STATUS_SUCCESS : SPAWN_STATUS_FAILURE;
}

// 0x00B57F40 (:3363)
spawn_status_result object_spawn_vehicle(vehicle_spawn_params& spawn_param)
{
    if (spawn_param.vp != nullptr || spawn_param.veh_info == nullptr || (spawn_param.spawn_pos.is_null() && spawn_param.osrp == nullptr))
    {
        return SPAWN_STATUS_FAILURE;
    }
    if (!World.is_zone_streamed(spawn_param.spawn_pos))
    {
        return SPAWN_STATUS_STREAMING;
    }
    resource_slot_record* record = get_resource_record(spawn_param.veh_info);
    if (record == nullptr || (!record->is_reserved() && !record->is_allocated()))
    {
        return SPAWN_STATUS_FAILURE;
    }
    if (stream2_get_status(record->get_srid()) != ST_LOADED)
    {
        return SPAWN_STATUS_STREAMING;
    }
    if (object_variant_loaded(spawn_param.veh_info))
    {
        return SPAWN_STATUS_FAILURE;
    }
    Spawning_flag = true;
    const bool spawned = sp_spawn_vehicle(spawn_param);
    if (spawned)
    {
        if (spawn_param.vp == nullptr)
        {
            // Original: returns with Spawning_flag still set.
            return SPAWN_STATUS_FAILURE;
        }
        if (spawn_param.veh_spawn_flags.infinite_mass)
        {
            spawn_param.vp->set_infinite_mass(true);
        }
    }
    Spawning_flag = false;
    return spawned ? SPAWN_STATUS_SUCCESS : SPAWN_STATUS_FAILURE;
}

// 0x00B5D6F0 (:3425)
void srm_do_frame()
{
    if (Multiplayer || wrecking_crew_is_active())
    {
        return;
    }
    if (Stream_check_period.elapsed())
    {
        // Magic numbers: a quarter to half a second.
        Stream_check_period.set_rand(250, 500);
        (Stream_check_humans ? Human_records_list : Vehicle_records_list).check_slot_status();
        Stream_check_humans = !Stream_check_humans;
    }
    if (!Mission_mode)
    {
        srm_do_frame_player();
        srm_do_frame_slot_clean_up();
        srm_do_frame_special_slot();
    }
}

// 0x00B5D770 (:3483)
void srm_system_init()
{
    spawn_res_data_system_init();
    Human_records_list.reset();
    Vehicle_records_list.reset();
}

// 0x00B493B0 (:3494)
int srm_find_human_resource_slot_id(unsigned int srid)
{
    for (int i = 0; i < Human_records_list.master_records_list.size(); i++)
    {
        if (Human_records_list.master_records_list[i].get_srid() == srid)
        {
            return i;
        }
    }
    return INVALID_HUMAN_RESOURCE_SLOT_ID;
}

// 0x00B493F0 (:3508)
int srm_find_vehicle_resource_slot_id(unsigned int srid)
{
    for (int i = 0; i < Vehicle_records_list.master_records_list.size(); i++)
    {
        if (Vehicle_records_list.master_records_list[i].get_srid() == srid)
        {
            return i;
        }
    }
    return -1;
}

// 0x00B52900 (:3521)
int srm_create_human_resource_slot_id(unsigned int srid)
{
    if (srid == 0)
    {
        return INVALID_HUMAN_RESOURCE_SLOT_ID;
    }
    const int found = srm_find_human_resource_slot_id(srid);
    if (found != INVALID_HUMAN_RESOURCE_SLOT_ID)
    {
        return found;
    }
    if (Human_records_list.master_records_list.size() >= Human_records_list.master_records_list.capacity())
    {
        return INVALID_HUMAN_RESOURCE_SLOT_ID;
    }
    const int index = Human_records_list.master_records_list.size();
    Human_records_list.master_records_list.add(resource_slot_record(srid, OT_HUMAN));
    return index;
}

// 0x00B529A0 (:3551)
int srm_create_vehicle_resource_slot_id(unsigned int srid)
{
    if (srid == 0)
    {
        return -1;
    }
    const int found = srm_find_vehicle_resource_slot_id(srid);
    if (found != -1)
    {
        return found;
    }
    if (Vehicle_records_list.master_records_list.size() >= Vehicle_records_list.master_records_list.capacity())
    {
        return -1;
    }
    const int index = Vehicle_records_list.master_records_list.size();
    Vehicle_records_list.master_records_list.add(resource_slot_record(srid, OT_VEHICLE));
    return index;
}

// 0x00B52A40 (:3581)
void srm_level_init_pre()
{
    Human_records_list.reset();
    Vehicle_records_list.reset();
    // Magic number: 20 s.
    Slot_rotation_timer.set(20000);
    Special_slot_timer.set(0);
    Mission_mode = false;
    Stream_check_period.set(0);
    Slot_release_period.set(0);
    Tainted_slot_reservations = false;
    Human_slot_release_timestamp.set(0);
    Vehicle_slot_release_timestamp.set(0);
    for (int i = 0; i < NUM_SPAWN_SLOT_TYPES; i++)
    {
        Master_resource_info_list[i].reset();
        DLC_resource_info_list[i].reset();
    }
}

// 0x00B52AE0 (:3606)
void srm_level_shutdown()
{
    Human_records_list.reset();
    Vehicle_records_list.reset();
}

// 0x00B49430 (:3759)
bool srm_slots_streamed(spawn_slot_types slot_type, resource_slot_user_id owner_id)
{
    if (!find_slot_limit_info(slot_type).is_allowed_to_use(owner_id))
    {
        return false;
    }
    const bool humans = Human_records_list.resources_streamed(slot_type);
    const bool vehicles = Vehicle_records_list.resources_streamed(slot_type);
    return humans && vehicles;
}

// 0x00B59160 (:3775)
// 0x00B59230 (:3775)
// LTCG: slot_type in EDI. A vehicle first clears the way for its family.
template<typename T>
static bool local_reserve_slot(spawn_slot_types slot_type, resource_slot_user_id owner_id, const T* obj_info, resource_slot_list& master_records_list)
{
    resource_slot_limit& limit = find_slot_limit_info(slot_type);
    if (!limit.is_allowed_to_use(owner_id) || limit.count_free_slots() <= 0 || obj_info == nullptr)
    {
        return false;
    }
    resource_slot_record* record = get_resource_record(obj_info);
    if (record == nullptr)
    {
        return false;
    }
    if (record->is_reserved() || record->is_allocated())
    {
        return master_records_list.reserve_slot(record, slot_type);
    }
    if (object_variant_loaded(obj_info) && !destroy_object_variants(obj_info))
    {
        keen::debug::nativePrintArgs("Int3 critical function at %s(%d)\n", __FILE__, __LINE__);
        return false;
    }
    if (master_records_list.count_free_slots() > 0 || master_records_list.free_low_priority_slots(1) > 0)
    {
        return master_records_list.reserve_slot(record, slot_type);
    }
    return false;
}

// 0x00B58E80 (:3823)
// 0x00B58FF0 (:3823)
// LTCG: slot_type in EDI. An info of the list already loaded wins, else a random one.
template<typename T>
static bool local_reserve_slot_from_list(spawn_slot_types slot_type, resource_slot_user_id owner_id, const base_array<T const*>& obj_info_list, resource_slot_list& master_records_list)
{
    if (obj_info_list.size() < 1)
    {
        return false;
    }
    resource_slot_limit& limit = find_slot_limit_info(slot_type);
    if (!limit.is_allowed_to_use(owner_id) || limit.count_free_slots() <= 0)
    {
        return false;
    }
    farray<T const*, 256> copy;
    copy.copy(obj_info_list);
    while (copy.size() > 0)
    {
        const int pick = copy.size() == 1 ? 0 : int_rand_range(0, copy.size() - 1);
        const T* info = copy[pick];
        copy.remove_unsorted(pick);
        resource_slot_record* record = get_resource_record(info);
        if (record != nullptr && (record->is_reserved() || record->is_allocated()))
        {
            return master_records_list.reserve_slot(record, slot_type);
        }
    }
    const int pick = obj_info_list.size() == 1 ? 0 : int_rand_range(0, obj_info_list.size() - 1);
    resource_slot_record* record = get_resource_record(obj_info_list[pick]);
    if (record == nullptr)
    {
        return false;
    }
    if (master_records_list.count_free_slots() <= 0 && master_records_list.free_low_priority_slots(1) < 1)
    {
        return false;
    }
    return master_records_list.reserve_slot(record, slot_type);
}

// 0x00B541E0 (:3894)
// 0x00B54260 (:3894)
template<typename T>
static bool slot_is_reserved(spawn_slot_types slot_type, resource_slot_user_id owner_id, const T* obj_info)
{
    if (!find_slot_limit_info(slot_type).is_allowed_to_use(owner_id) || obj_info == nullptr)
    {
        return false;
    }
    resource_slot_record* record = get_resource_record(obj_info);
    return record != nullptr && record->is_reserved_by(slot_type);
}

// 0x00B58030 (:3913)
bool srm_slot_is_reserved(spawn_slot_types slot_type, resource_slot_user_id owner_id, const human_info* hum_info)
{
    return slot_is_reserved(slot_type, owner_id, hum_info);
}

// 0x00B58050 (:3919)
bool srm_slot_is_reserved(spawn_slot_types slot_type, resource_slot_user_id owner_id, const vehicle_info* veh_info)
{
    return slot_is_reserved(slot_type, owner_id, veh_info);
}

// 0x00B52B00 (:3925)
bool srm_slot_is_reserved(spawn_slot_types slot_type, const vehicle_info* veh_info)
{
    if ((int)slot_type < 1 || (int)slot_type >= NUM_SPAWN_SLOT_TYPES)
    {
        return false;
    }
    resource_slot_record* record = get_resource_record(veh_info);
    return record != nullptr && record->is_reserved_by(slot_type);
}

// 0x00B5FD30 (:3940)
bool srm_swap_player_slot(const vehicle_info* v_info)
{
    if (v_info == nullptr)
    {
        return false;
    }
    resource_slot_limit& limit = find_slot_limit_info(SPAWN_SLOT_VEHICLE_PLAYER);
    resource_slot_record* record = get_resource_record(v_info);
    if (limit.owner_id == RESOURCE_SLOT_USER_ID_NONE && record != nullptr && record->is_reserved_by(SPAWN_SLOT_VEHICLE_PLAYER))
    {
        return true;
    }
    if (!Mission_mode && limit.owner_id == RESOURCE_SLOT_USER_ID_NONE)
    {
        Human_records_list.release_reserved_slots(SPAWN_SLOT_VEHICLE_PLAYER);
        Vehicle_records_list.release_reserved_slots(SPAWN_SLOT_VEHICLE_PLAYER);
        limit.reset();
    }
    if (!local_reserve_slot(SPAWN_SLOT_VEHICLE_PLAYER, RESOURCE_SLOT_USER_ID_NONE, v_info, Vehicle_records_list))
    {
        return false;
    }
    limit.owner_id = RESOURCE_SLOT_USER_ID_NONE;
    return true;
}

// 0x00B5D790 (:3959)
bool srm_reserve_slot(spawn_slot_types slot_type, resource_slot_user_id owner_id, const human_spawn_group_info* hum_group)
{
    if (!local_reserve_slot_from_list<human_info>(slot_type, owner_id, hum_group->h_info_list, Human_records_list))
    {
        return false;
    }
    find_slot_limit_info(slot_type).owner_id = owner_id;
    return true;
}

// 0x00B5FE10 (:3972)
bool srm_reserve_slot(spawn_slot_types slot_type, resource_slot_user_id owner_id, const base_array<vehicle_info const*>& veh_info_list)
{
    if (veh_info_list.size() < 1)
    {
        return false;
    }
    if (veh_info_list.size() == 1)
    {
        return srm_reserve_slot(slot_type, owner_id, veh_info_list[0]);
    }
    if (!local_reserve_slot_from_list<vehicle_info>(slot_type, owner_id, veh_info_list, Vehicle_records_list))
    {
        return false;
    }
    find_slot_limit_info(slot_type).owner_id = owner_id;
    return true;
}

// 0x00B5FEA0 (:3991)
bool srm_reserve_slot(spawn_slot_types slot_type, resource_slot_user_id owner_id, const vehicle_spawn_group_info* veh_group)
{
    if (veh_group == nullptr)
    {
        return false;
    }
    return srm_reserve_slot(slot_type, owner_id, veh_group->v_info_list);
}

// 0x00B5D800 (:4001)
bool srm_reserve_slot(spawn_slot_types slot_type, resource_slot_user_id owner_id, const human_info* hum_info)
{
    if (!local_reserve_slot(slot_type, owner_id, hum_info, Human_records_list))
    {
        return false;
    }
    find_slot_limit_info(slot_type).owner_id = owner_id;
    return true;
}

// 0x00B5D870 (:4014)
bool srm_reserve_slot(spawn_slot_types slot_type, resource_slot_user_id owner_id, const vehicle_info* veh_info)
{
    if (!local_reserve_slot(slot_type, owner_id, veh_info, Vehicle_records_list))
    {
        return false;
    }
    find_slot_limit_info(slot_type).owner_id = owner_id;
    return true;
}

// 0x00B52B50 (:4026)
// False while a slot this type alone holds still has objects.
bool srm_can_release_vehicle_slot(spawn_slot_types slot_type, resource_slot_user_id owner_id)
{
    if (!find_slot_limit_info(slot_type).is_allowed_to_use(owner_id))
    {
        return false;
    }
    for (int i = 0; i < Vehicle_records_list.master_records_list.size(); i++)
    {
        const resource_slot_record& record = Vehicle_records_list.master_records_list[i];
        if (record.is_reserved_by(slot_type) && !record.is_reserved_by_other(slot_type) && record.get_object_count() > 0)
        {
            return false;
        }
    }
    return true;
}

// 0x00B44180 (:4065)
bool srm_can_release_human_slot(spawn_slot_types slot_type, resource_slot_user_id owner_id)
{
    keen::debug::nativePrintArgs("Int3 critical function at %s(%d)\n", __FILE__, __LINE__);
    return false;
}

// 0x00B494B0 (:4075)
bool srm_no_owner(spawn_slot_types slot_type)
{
    return find_slot_limit_info(slot_type).owner_id == RESOURCE_SLOT_USER_ID_NONE;
}

// 0x00B441A0 (:4082)
resource_slot_user_id srm_get_owner(spawn_slot_types slot_type)
{
    return find_slot_limit_info(slot_type).owner_id;
}

// 0x00B52BF0 (:4091)
int srm_get_availible_slots(spawn_slot_types slot_type, resource_slot_user_id owner_id)
{
    if ((int)owner_id < 1 || (int)owner_id >= NUM_RESOURCE_SLOT_USER_IDS || Mission_mode)
    {
        return 0;
    }
    resource_slot_limit& limit = find_slot_limit_info(slot_type);
    return limit.is_allowed_to_use(owner_id) ? limit.count_free_slots() : 0;
}

// 0x00B52C70 (:4111)
int srm_get_used_slots(spawn_slot_types slot_type, resource_slot_user_id owner_id)
{
    if ((int)owner_id < 1 || (int)owner_id >= NUM_RESOURCE_SLOT_USER_IDS)
    {
        return 0;
    }
    resource_slot_limit& limit = find_slot_limit_info(slot_type);
    return limit.is_allowed_to_use(owner_id) ? limit.count_used_slots() : 0;
}

// 0x00B58070 (:4127)
void srm_free_unused_reserved_slots(spawn_slot_types slot_type, resource_slot_user_id owner_id)
{
    resource_slot_limit& limit = find_slot_limit_info(slot_type);
    if (limit.is_allowed_to_use(owner_id) && limit.slot_release_timestamp.elapsed())
    {
        // Magic number: 200 ms.
        limit.slot_release_timestamp.set(200);
        Human_records_list.release_unused_reserved_slots(slot_type);
        Vehicle_records_list.release_unused_reserved_slots(slot_type);
    }
}

// 0x00B59330 (:4151)
// 0x00B593D0 (:4151)
// LTCG: owner_id in ECX, slot_type in EDX.
template<typename T>
static void local_release_slot(spawn_slot_types slot_type, resource_slot_user_id owner_id, const T* object_info, resource_slot_list& master_records_list)
{
    if (Mission_mode || (unsigned int)owner_id - 1 >= NUM_RESOURCE_SLOT_USER_IDS - 1)
    {
        return;
    }
    if (!find_slot_limit_info(slot_type).is_allowed_to_use(owner_id))
    {
        return;
    }
    resource_slot_record* record = get_resource_record(object_info);
    if (record == nullptr)
    {
        return;
    }
    if (record->is_reserved_by(slot_type))
    {
        master_records_list.unreserve_slot(record, slot_type);
        return;
    }
    keen::debug::nativePrintArgs("Int3 critical function at %s(%d)\n", __FILE__, __LINE__);
}

// 0x00B5D8E0 (:4180)
void srm_release_slot(spawn_slot_types slot_type, resource_slot_user_id owner_id, const human_info* hum_info)
{
    local_release_slot(slot_type, owner_id, hum_info, Human_records_list);
}

// 0x00B5D900 (:4186)
void srm_release_slot(spawn_slot_types slot_type, resource_slot_user_id owner_id, const vehicle_info* veh_info)
{
    local_release_slot(slot_type, owner_id, veh_info, Vehicle_records_list);
}

// 0x00B580F0 (:4192)
void srm_release_all_slots(spawn_slot_types slot_type, resource_slot_user_id owner_id)
{
    if (Mission_mode || owner_id >= NUM_RESOURCE_SLOT_USER_IDS)
    {
        return;
    }
    resource_slot_limit& limit = find_slot_limit_info(slot_type);
    if (limit.is_allowed_to_use(owner_id))
    {
        Human_records_list.release_reserved_slots(slot_type);
        Vehicle_records_list.release_reserved_slots(slot_type);
        limit.reset();
    }
}

// 0x00B58180 (:4227)
bool srm_all_reserved_slots_streamed()
{
    Human_records_list.check_slot_status();
    Vehicle_records_list.check_slot_status();
    const bool humans = Human_records_list.all_resources_streamed();
    const bool vehicles = Vehicle_records_list.all_resources_streamed();
    return humans && vehicles;
}

// 0x00B5D920 (:4243)
// Every player's info keeps its container for the level.
void srm_level_init_post()
{
    if (Multiplayer || wrecking_crew_is_active())
    {
        return;
    }
    Human_records_list.clear_all_reservations_except(SPAWN_SLOT_NONE);
    Vehicle_records_list.clear_all_reservations_except(SPAWN_SLOT_NONE);
    for (int i = 0; i < World.get_num_objects<player>(); i++)
    {
        player* pp = World.get_object<player>(i);
        if (pp->h_info != nullptr)
        {
            Human_records_list.reserve_slot_player(Human_records_list.get_record(pp->h_info->slot_id));
        }
    }
}

// 0x00B441E0 (:4276)
void srm_register_npc(const npc* np)
{
}

// 0x00B441F0 (:4292)
void srm_register_vehicle(const vehicle* vp)
{
}

// 0x00B49500 (:4316)
// Original: only answers how many are free, without freeing any.
int srm_free_low_priority_slots_human()
{
    return Human_records_list.count_free_slots();
}

// 0x00B49510 (:4323)
int srm_free_low_priority_slots_vehicle()
{
    return Vehicle_records_list.count_free_slots();
}

// 0x00B44210 (:4330)
void srm_remove_npc(const npc* np)
{
}

// 0x00B44220 (:4335)
void srm_remove_vehicle(const vehicle* vp)
{
}

// 0x00B5D9E0 (:4341)
void srm_allocate_ambient_spawn_slots()
{
    if (!Mission_mode)
    {
        allocate_ambient_spawn_slots_human();
        allocate_ambient_spawn_slots_vehicle();
    }
}

// 0x00B5DA00 (:4350)
void srm_allocate_ambient_spawn_slots_human()
{
    if (!Mission_mode)
    {
        allocate_ambient_spawn_slots_human();
    }
}

// 0x00B58210 (:4358)
void srm_allocate_ambient_spawn_slots_vehicle()
{
    if (!Mission_mode)
    {
        allocate_ambient_spawn_slots_vehicle();
    }
}

// 0x00B58220 (:4369)
const farray<human_info const*, 165>& srm_loaded_human_info_list()
{
    const farray<human_info, 165>& list = human_info_get_master_list();
    Availible_human_info_list.clear();
    for (int i = 0; i < list.size(); i++)
    {
        if (resources_loaded(&list[i]))
        {
            Availible_human_info_list.add(&list[i]);
        }
    }
    return Availible_human_info_list;
}

// 0x00B58300 (:4393)
const farray<vehicle_info const*, 163>& srm_loaded_vehicle_info_list()
{
    const farray<vehicle_info, 163>& list = vehicle_info_get_master_list();
    Availible_vehicle_info_list.clear();
    for (int i = 0; i < list.size(); i++)
    {
        if (resources_loaded(&list[i]))
        {
            Availible_vehicle_info_list.add(&list[i]);
        }
    }
    return Availible_vehicle_info_list;
}

// 0x00B583E0 (:4417)
const human_info* srm_pick_loaded_human_info(const base_array<human_info const*>& human_info_list)
{
    farray<human_info const*, 256> loaded;
    for (int i = 0; i < human_info_list.size(); i++)
    {
        if (human_info_list[i] != nullptr && resources_loaded(human_info_list[i]))
        {
            loaded.add(human_info_list[i]);
        }
    }
    if (loaded.size() < 1)
    {
        return nullptr;
    }
    return loaded[loaded.size() == 1 ? 0 : int_rand_range(0, loaded.size() - 1)];
}

// 0x00B58500 (:4447)
const human_info* srm_pick_loaded_human_info(const human_spawn_group_info* h_spawn_group)
{
    return srm_pick_loaded_human_info(h_spawn_group->h_info_list);
}

// 0x00B58510 (:4452)
const vehicle_info* srm_pick_loaded_vehicle_info(const base_array<vehicle_info const*>& vehicle_info_list)
{
    farray<vehicle_info const*, 256> loaded;
    for (int i = 0; i < vehicle_info_list.size(); i++)
    {
        if (vehicle_info_list[i] != nullptr && resources_loaded(vehicle_info_list[i]))
        {
            loaded.add(vehicle_info_list[i]);
        }
    }
    if (loaded.size() < 1)
    {
        return nullptr;
    }
    return loaded[loaded.size() == 1 ? 0 : int_rand_range(0, loaded.size() - 1)];
}

// 0x00B58630 (:4481)
const vehicle_info* srm_pick_loaded_vehicle_info(const vehicle_spawn_group_info* v_spawn_group)
{
    return srm_pick_loaded_vehicle_info(v_spawn_group->v_info_list);
}

// 0x00B58640 (:4487)
void srm_reserve_vehicle_spawn_group_mp(const vehicle_spawn_group_info* v_spawn_group)
{
    if (!Multiplayer)
    {
        return;
    }
    for (int i = 0; i < v_spawn_group->v_info_list.size(); i++)
    {
        reserve_resource_slot(v_spawn_group->v_info_list[i], SPAWN_SLOT_VEHICLE_MULTI_PURPOSE);
    }
}

// 0x00B58680 (:4500)
bool srm_human_info_loaded(const human_info* h_info)
{
    return resources_loaded(h_info);
}

// 0x00B52CD0 (:4506)
bool srm_human_info_reserved(const human_info* h_info)
{
    resource_slot_record* record = get_resource_record(h_info);
    return record != nullptr && record->is_reserved();
}

// 0x00B58690 (:4518)
bool srm_vehicle_info_loaded(const vehicle_info* v_info)
{
    return resources_loaded(v_info);
}

// 0x00B52D00 (:4523)
bool srm_vehicle_info_reserved(const vehicle_info* v_info)
{
    resource_slot_record* record = get_resource_record(v_info);
    return record != nullptr && record->is_reserved();
}

// 0x00B52D30 (:4535)
bool srm_human_group_reserved(const human_spawn_group_info* h_group)
{
    for (int i = 0; i < h_group->h_info_list.size(); i++)
    {
        if (srm_human_info_reserved(h_group->h_info_list[i]))
        {
            return true;
        }
    }
    return false;
}

// 0x00B52D90 (:4549)
bool srm_vehicle_group_reserved(const vehicle_spawn_group_info* v_group)
{
    for (int i = 0; i < v_group->v_info_list.size(); i++)
    {
        if (srm_vehicle_info_reserved(v_group->v_info_list[i]))
        {
            return true;
        }
    }
    return false;
}

// 0x00B586A0 (:4564)
bool allcoate_free_slot(const vehicle_info* v_info)
{
    Vehicle_records_list.free_all_unused_slots();
    resource_slot_record* record = get_resource_record(v_info);
    if (Vehicle_records_list.count_free_slots() > 0 && record != nullptr)
    {
        Vehicle_records_list.allocate_slot(record);
        return true;
    }
    return false;
}

// 0x00B58700 (:4587)
bool allcoate_free_slot(const human_info* h_info)
{
    Human_records_list.free_all_unused_slots();
    resource_slot_record* record = get_resource_record(h_info);
    if (Human_records_list.count_free_slots() > 0 && record != nullptr)
    {
        Human_records_list.allocate_slot(record);
        return true;
    }
    return false;
}

// 0x00B5DA10 (:4608)
npc* srm_spawn_single_npc_vehicle(const human_info* h_info, vehicle* vp, vehicle_seat_index vsi)
{
    if (h_info == nullptr || vp == nullptr || !resources_loaded(h_info))
    {
        return nullptr;
    }
    npc_spawn_param_list params(NPC_MANAGER_SPAWN_NODE);
    params.spawn_pos = vp->pos;
    params.spawn_orient = vp->orient;
    params.vp = vp;
    params.first_seat = vsi;
    params.npc_spawn_info.add(npc_spawn_params(h_info));
    object_spawn_do_spawn(params);
    return params.npc_spawn_info[0].np;
}

// 0x00B52DF0 (:4635)
void srm_reserved_human_info_list(farray<human_info const*, 165>& info_list, human_teams team, bool high_priority_only)
{
    const farray<human_info, 165>& list = human_info_get_master_list();
    info_list.clear();
    for (int i = 0; i < list.size(); i++)
    {
        const human_info* info = &list[i];
        if (team != info->default_team)
        {
            continue;
        }
        resource_slot_record* record = Human_records_list.get_record(info->slot_id);
        if (record != nullptr && !record->is_player_slot() && record->can_be_used() && (!high_priority_only || record->is_reserved()))
        {
            info_list.add(info);
        }
    }
}

// 0x00B5DB20 (:4704)
// A mission's human set: its squads, then its required groups, keeping what is already loaded where a choice remains.
static void srm_mission_reservation_human(const spawn_resource_data* srm_data)
{
    if (srm_data == nullptr)
    {
        return;
    }
    farray<resource_slot_record*, 165> records_to_reserve;
    farray<human_reservation_info, 16> needed;
    farray<human_info const*, 256> choices;
    Human_records_list.clear_all_reservations_except(SPAWN_SLOT_HUMAN_PLAYER);
    Human_records_list.free_all_unused_slots();
    for (int team = 0; team < NUM_HUMAN_TEAMS; team++)
    {
        if (srm_data->team_squad_counts[team] > 0)
        {
            squad_progression_reserve_members(SPAWN_SLOT_MISSION, (human_teams)team, srm_data->team_squad_counts[team]);
        }
    }
    for (int i = 0; i < Human_records_list.master_records_list.size(); i++)
    {
        resource_slot_record* record = &Human_records_list.master_records_list[i];
        if (record->is_reserved())
        {
            records_to_reserve.add(record);
        }
    }
    for (int i = 0; i < srm_data->required_human_slots.size(); i++)
    {
        needed.add(srm_data->required_human_slots[i]);
    }
    // A group of one is settled.
    for (int i = 0; i < needed.size();)
    {
        if (needed[i].count < 1)
        {
            needed.remove_unsorted(i);
        }
        else if (needed[i].spawn_group->h_info_list.size() == 1)
        {
            const human_info* info = needed[i].spawn_group->h_info_list[0];
            needed.remove_unsorted(i);
            resource_slot_record* record = get_resource_record(info);
            if (record != nullptr)
            {
                records_to_reserve.add_unique(record);
            }
        }
        else
        {
            i++;
        }
    }
    // What is loaded or already chosen counts first.
    for (int i = 0; i < needed.size();)
    {
        human_reservation_info& info = needed[i];
        if (info.count >= 1)
        {
            for (int k = 0; k < info.spawn_group->h_info_list.size(); k++)
            {
                resource_slot_record* record = get_resource_record(info.spawn_group->h_info_list[k]);
                if (record != nullptr && (record->is_reserved() || record->is_allocated() || (records_to_reserve.find(record, false) > -1)))
                {
                    records_to_reserve.add_unique(record);
                    info.count--;
                }
                if (info.count < 1)
                {
                    break;
                }
            }
        }
        if (info.count < 1)
        {
            needed.remove_unsorted(i);
        }
        else
        {
            i++;
        }
    }
    // The rest at random.
    for (int i = 0; i < needed.size(); i++)
    {
        human_reservation_info& info = needed[i];
        choices.clear();
        for (int k = 0; k < info.spawn_group->h_info_list.size(); k++)
        {
            const human_info* h_info = info.spawn_group->h_info_list[k];
            resource_slot_record* record = get_resource_record(h_info);
            if (record != nullptr && !(records_to_reserve.find(record, false) > -1))
            {
                choices.add(h_info);
            }
        }
        while (info.count > 0 && choices.size() > 0)
        {
            const int pick = choices.size() == 1 ? 0 : int_rand_range(0, choices.size() - 1);
            const human_info* h_info = choices[pick];
            choices.remove_unsorted(pick);
            resource_slot_record* record = get_resource_record(h_info);
            if (record != nullptr)
            {
                records_to_reserve.add(record);
                info.count--;
            }
        }
    }
    for (int i = 0; i < records_to_reserve.size();)
    {
        resource_slot_record* record = records_to_reserve[i];
        if (record->is_reserved() || record->is_allocated())
        {
            Human_records_list.reserve_slot(record, SPAWN_SLOT_MISSION);
            records_to_reserve.remove_unsorted(i);
        }
        else
        {
            i++;
        }
    }
    if (records_to_reserve.size() > 0)
    {
        const int free_slots = Human_records_list.count_free_slots();
        int count = records_to_reserve.size();
        if (free_slots < count)
        {
            count = free_slots + Human_records_list.free_low_priority_slots(count - free_slots);
        }
        for (int i = 0; i < count; i++)
        {
            Human_records_list.reserve_slot(records_to_reserve[i], SPAWN_SLOT_MISSION);
        }
    }
    // Guerrillas fill what is left.
    for (int n = Human_records_list.count_free_slots(); n > 0; n--)
    {
        for (int i = 0; i < Desired_guerrilla_list.size(); i++)
        {
            // Original: a guerrilla info without a record is read through.
            resource_slot_record* record = get_resource_record(Desired_guerrilla_list[i]);
            if (!record->is_reserved())
            {
                Human_records_list.reserve_slot(record, SPAWN_SLOT_MISSION_ALLOCATED);
                break;
            }
        }
    }
}

// 0x00B52EA0 (:4941)
// LTCG: records_to_reserve in ECX. The vehicle's family is banned from then on: only one variant can be loaded.
static void add_vehicle_to_reservation_list(const vehicle_info* v_info, farray<resource_slot_record*, 165>& records_to_reserve, farray<resource_slot_record*, 165>& banned_records)
{
    if (v_info == nullptr)
    {
        return;
    }
    resource_slot_record* record = get_resource_record(v_info);
    if (INVALID_VEHICLE_FAMILY_ID == v_info->variant_family)
    {
        records_to_reserve.add_unique(record);
        return;
    }
    if ((banned_records.find(record, false) > -1))
    {
        return;
    }
    records_to_reserve.add_unique(record);
    farray<vehicle_info const*, 163> family;
    if (vehicle_collect_family(family, v_info->variant_family, false))
    {
        for (int i = 0; i < family.size(); i++)
        {
            if (v_info->srid != family[i]->srid)
            {
                resource_slot_record* other = get_resource_record(family[i]);
                if (other != nullptr)
                {
                    banned_records.add_unique(other);
                }
            }
        }
    }
}

// 0x00B49520 (:4979)
// LTCG: banned_records in ECX.
static bool vehicle_on_banned_list(const vehicle_info* v_info, const farray<resource_slot_record*, 165>& banned_records)
{
    for (int i = 0; i < banned_records.size(); i++)
    {
        if (v_info->srid == banned_records[i]->get_srid())
        {
            return true;
        }
    }
    return false;
}

// 0x00B5DF70 (:5014)
static void srm_mission_reservation_vehicle(const spawn_resource_data* srm_data)
{
    if (srm_data == nullptr)
    {
        return;
    }
    farray<resource_slot_record*, 165> records_to_reserve;
    farray<resource_slot_record*, 165> banned_records;
    farray<vehicle_info const*, 256> choices;
    farray<vehicle_reservation_info, 7> needed;
    Vehicle_records_list.clear_all_reservations_except(SPAWN_SLOT_VEHICLE_PLAYER);
    Vehicle_records_list.free_all_unused_slots();
    for (int i = 0; i < srm_data->required_vehicle_slots.size(); i++)
    {
        needed.add(srm_data->required_vehicle_slots[i]);
    }
    for (int i = 0; i < needed.size();)
    {
        if (needed[i].count < 1)
        {
            needed.remove_unsorted(i);
        }
        else if (needed[i].spawn_group->v_info_list.size() == 1)
        {
            const vehicle_info* info = needed[i].spawn_group->v_info_list[0];
            needed.remove_unsorted(i);
            add_vehicle_to_reservation_list(info, records_to_reserve, banned_records);
        }
        else
        {
            i++;
        }
    }
    for (int i = 0; i < needed.size();)
    {
        vehicle_reservation_info& info = needed[i];
        if (info.count >= 1)
        {
            for (int k = 0; k < info.spawn_group->v_info_list.size(); k++)
            {
                const vehicle_info* v_info = info.spawn_group->v_info_list[k];
                if (!vehicle_on_banned_list(v_info, banned_records))
                {
                    resource_slot_record* record = get_resource_record(v_info);
                    if (record != nullptr && (record->is_reserved() || record->is_allocated() || (records_to_reserve.find(record, false) > -1)))
                    {
                        add_vehicle_to_reservation_list(v_info, records_to_reserve, banned_records);
                        info.count--;
                    }
                }
                if (info.count < 1)
                {
                    break;
                }
            }
        }
        if (info.count < 1)
        {
            needed.remove_unsorted(i);
        }
        else
        {
            i++;
        }
    }
    for (int i = 0; i < needed.size(); i++)
    {
        vehicle_reservation_info& info = needed[i];
        choices.clear();
        for (int k = 0; k < info.spawn_group->v_info_list.size(); k++)
        {
            const vehicle_info* v_info = info.spawn_group->v_info_list[k];
            resource_slot_record* record = get_resource_record(v_info);
            if (record != nullptr && !(records_to_reserve.find(record, false) > -1))
            {
                choices.add(v_info);
            }
        }
        while (info.count > 0 && choices.size() > 0)
        {
            const int pick = choices.size() == 1 ? 0 : int_rand_range(0, choices.size() - 1);
            const vehicle_info* v_info = choices[pick];
            choices.remove_unsorted(pick);
            if (!vehicle_on_banned_list(v_info, banned_records))
            {
                add_vehicle_to_reservation_list(v_info, records_to_reserve, banned_records);
                info.count--;
            }
        }
    }
    // The banned variants go before the wanted ones stream.
    for (int i = 0; i < banned_records.size(); i++)
    {
        Vehicle_records_list.destroy_slot(banned_records[i]);
    }
    for (int i = 0; i < records_to_reserve.size();)
    {
        resource_slot_record* record = records_to_reserve[i];
        if (record->is_reserved() || record->is_allocated())
        {
            Vehicle_records_list.reserve_slot(record, SPAWN_SLOT_MISSION);
            records_to_reserve.remove_unsorted(i);
        }
        else
        {
            i++;
        }
    }
    if (records_to_reserve.size() > 0)
    {
        const int free_slots = Vehicle_records_list.count_free_slots();
        int count = records_to_reserve.size();
        if (free_slots < count)
        {
            count = free_slots + Vehicle_records_list.free_low_priority_slots(count - free_slots);
        }
        for (int i = 0; i < count; i++)
        {
            Vehicle_records_list.reserve_slot(records_to_reserve[i], SPAWN_SLOT_MISSION);
        }
    }
}

// 0x00B5E410 (:5232)
void srm_mission_reservation(const spawn_resource_data* srm_data)
{
    if (Mission_mode)
    {
        return;
    }
    Human_records_list.clear_all_reservations_except(SPAWN_SLOT_HUMAN_PLAYER);
    Vehicle_records_list.clear_all_reservations_except(SPAWN_SLOT_VEHICLE_PLAYER);
    if (srm_data != nullptr)
    {
        srm_mission_reservation_human(srm_data);
        srm_mission_reservation_vehicle(srm_data);
    }
    Vehicle_records_list.reserve_mission_allocated_slots();
    Human_records_list.reserve_mission_allocated_slots();
    Mission_mode = true;
}

// 0x00B5E470 (:5258)
void srm_mission_release()
{
    if (!Mission_mode)
    {
        return;
    }
    Mission_mode = false;
    Human_records_list.clear_all_reservations_except(SPAWN_SLOT_HUMAN_PLAYER);
    Vehicle_records_list.clear_all_reservations_except(SPAWN_SLOT_VEHICLE_PLAYER);
    Human_records_list.free_all_unused_slots();
    Vehicle_records_list.free_all_unused_slots();
}

// 0x00B49550 (:5278)
unsigned int srm_human_resource_dlc_mark()
{
    return (unsigned int)Human_records_list.master_records_list.size();
}

// 0x00B49560 (:5283)
unsigned int srm_vehicle_resource_dlc_mark()
{
    return (unsigned int)Vehicle_records_list.master_records_list.size();
}

// The records above the mark go (the same in both restores).
static void restore_records_to_mark(base_array<resource_slot_record>& records, unsigned int mark)
{
    for (int i = records.size() - 1; i >= (int)mark; i--)
    {
        records[i].reset_internal();
        records.remove(i);
    }
}

// 0x00B49570 (:5289)
void srm_human_resource_dlc_restore_to_mark(unsigned int mark)
{
    restore_records_to_mark(Human_records_list.master_records_list, mark);
}

// 0x00B49650 (:5298)
void srm_vehicle_resource_dlc_restore_to_mark(unsigned int mark)
{
    restore_records_to_mark(Vehicle_records_list.master_records_list, mark);
}
