// Original: volition/rfg/code/gameplay/activities/activity_assassin.cpp

#include "volition/rfg/code/gameplay/activities/activity_assassin.h"
#include "volition/rfg/code/gameplay/activities/activity_assassin_internal.h"

#include "volition/rfg/code/ai/self_preservation_goals.h"
#include "volition/rfg/code/destruction/rfg_mover.h"
#include "volition/rfg/code/gameplay/gameflow/gfm_control_morale.h"
#include "volition/rfg/code/gameplay/gameflow/gfm_orders_manager.h"
#include "volition/rfg/code/gameplay/squad_spawn.h"
#include "volition/rfg/code/hud/hud_message.h"
#include "volition/rfg/code/hud/minimap.h"
#include "volition/rfg/code/level/world.h"
#include "volition/rfg/code/multi/multi_core/include/multi.h"
#include "volition/rfg/code/objects/humans/npc.h"
#include "volition/rfg/code/objects/humans/player.h"
#include "volition/rfg/code/objects/object_squad.h"
#include "volition/rfg/code/rfg.h"
#include "volition/rfg/code/spawn/spawn_object_group.h"
#include "volition/rfg/code/spawn/spawn_request.h"
#include "volition/rfg/code/spawn/spawn_resource_manager.h"
#include "volition/vlib/code/math/ix.h"
#include "volition/vlib/code/math/vector3.h"
#include "volition/vlib/code/memory/virtual_mempool.h"
#include "volition/vlib/code/util/fastxml.h"

#include <math.h>

extern gfm_orders_flags ACTIVITY_ORDERS_FLAGS; // 0x02231B21 (gfm_smash_building_coordinator.cpp)

activity_assassin_data Assassin_activity_data; // 0x022458B0

static bool cover_is_blown; // file-static in rfg.exe

// 0x0077EBA0 (.h:20)
assassin_definition::assassin_definition()
{
    reset();
}

// 0x0077EBD0
assassin_definition::~assassin_definition()
{
}

// 0x007691C0 (.h:28)
void assassin_definition::reset()
{
    num_bureaucrats_to_spawn = 0;
    reward = 0;
    num_bureaucrats_per_level = 0;
    bodyguard_def_list.clear();
}

// 0x00795930 (.h:43)
activity_assassin_data::activity_assassin_data()
    : activity_base_free(ACTIVITY_MODE_ASSASSIN)
{
}

// 0x0077EBE0
activity_assassin_data::~activity_assassin_data()
{
}

// A live npc from a handle, or null.
static npc* assassin_npc_from_handle(unsigned int handle)
{
    if (handle == 0xFFFFFFFF)
    {
        return nullptr;
    }
    object* objp = World.object_from_handle(handle);
    if (objp == nullptr || objp->get_obj_type() != OT_HUMAN || objp->get_sub_type() != OT_SUB_HUMAN_NPC)
    {
        return nullptr;
    }
    return static_cast<npc*>(objp);
}

// 0x00796710 (:60). Original: dist_sq reads -1 when the walk finds nobody.
npc* activity_assassin_get_closest_bodyguard(const npc* bureaucrat, float* dist_sq)
{
    if (!Assassin_activity_data.is_active())
    {
        return nullptr;
    }
    npc* closest = nullptr;
    float closest_dist = -1.0f;
    const farray<unsigned int, 10>& orders_handles = Assassin_activity_data.orders_handles;
    for (int i = 0; i < orders_handles.size(); i++)
    {
        gfm_orders* orders = Orders_manager.get_orders_from_handle(orders_handles[i]);
        if (orders == nullptr || orders->get_target_object() != bureaucrat)
        {
            continue;
        }
        object_squad* squad = orders->get_assigned_squad_object();
        if (squad == nullptr)
        {
            continue;
        }
        for (int m = 0; m < squad->member_array.size(); m++)
        {
            npc* member = assassin_npc_from_handle(squad->member_array[m]);
            if (member != nullptr && !member->is_killed())
            {
                const float dist = vec_dist_squared(bureaucrat->pos, member->pos);
                if (closest == nullptr || dist < closest_dist)
                {
                    closest_dist = dist;
                    closest = member;
                }
            }
        }
    }
    if (dist_sq != nullptr)
    {
        *dist_sq = closest_dist;
    }
    return closest;
}

// 0x007968A0 (:113)
void activity_assassin_data::system_init()
{
    current_level = 0;
    num_levels = 0;
    load_definition_from_table();
}

// 0x007ACD60 (:125)
void activity_assassin_data::level_init()
{
    activity_base_free::level_init(); // rfg.exe inlines it
    reset_activity_instance();
}

// 0x007ACD90 (:133)
bool activity_assassin_data::activity_start()
{
    if (!activity_base_free::activity_start())
    {
        return false;
    }
    cover_is_blown = true;
    return true;
}

// 0x007ACDB0 (:146)
void activity_assassin_data::activity_end()
{
    activity_base_free::activity_end(); // rfg.exe inlines it
    if (levels[current_level].num_bureaucrats_per_level <= num_bureaucrats_killed)
    {
        current_level++;
        num_bureaucrats_killed = 0;
    }
    reset_activity_instance();
}

// 0x007ACE00 (:161)
void activity_assassin_data::activity_kill()
{
    activity_base_free::activity_kill(); // rfg.exe inlines it
    reset_activity_instance();
}

// 0x007B5740 (:173)
void activity_assassin_data::activity_do_frame_active()
{
    monitor_bureaucrats();
    if (bureaucrat_handles.size() == 0)
    {
        activity_end();
    }
}

// 0x007ACE20 (:186)
void activity_assassin_data::activity_do_frame_ready()
{
    activity_base::activity_do_frame_ready();
    if (office_handle == 0xFFFFFFFF)
    {
        return;
    }
    object* office = World.object_from_handle(office_handle);
    if (office == nullptr || office->get_obj_type() != OT_MOVER || office->get_sub_type() != OT_SUB_MOVER_RFG)
    {
        office = nullptr;
    }
    // Original: a null office is dereferenced here.
    if (!office->obj_flags.streamed)
    {
        reset_activity_instance();
        return;
    }
    if (is_local_player_in_office_bbox()) // rfg.exe inlines it
    {
        activity_start();
        return;
    }
    for (int i = 0; i < bureaucrat_handles.size(); i++)
    {
        const npc* np = assassin_npc_from_handle(bureaucrat_handles[i]);
        if (np != nullptr && np->obj_flags.streamed && ai_get_enemy_flee_belief(np) != nullptr)
        {
            activity_start();
            return;
        }
    }
}

// 0x00757280 (:231)
bool activity_assassin_data::is_player_cover_blown() const
{
    return !Multiplayer && cover_is_blown;
}

// 0x0076A170 (:242). Original: reads the bounding box of each bureaucrat and does nothing with it.
void activity_assassin_data::render_icons() const
{
    if (office_handle == 0xFFFFFFFF || !is_active())
    {
        return;
    }
    for (int i = 0; i < bureaucrat_handles.size(); i++)
    {
        const npc* np = assassin_npc_from_handle(bureaucrat_handles[i]);
        if (np != nullptr && !np->is_killed())
        {
            Cartographer.add_temporary_icon(np->pos, MAP_ELEMENT_STAR_RED_ICON, false, 0xFFFFFFFF, Default_map_item_color, false, 0xFFFFFFFF);
        }
    }
}

// 0x007572A0 (:344)
void activity_assassin_data::report_bureaucrat_running()
{
    if (!is_active())
    {
        activity_start();
    }
}

// 0x0077FC10 (:357). Original: the early returns leave the common pool unrestored.
void activity_assassin_data::load_definition_from_table()
{
    const unsigned int mark = Common_mempool.mark();
    xml_element* root = xml_parse("activity_assassin.xtbl", reinterpret_cast<mempool_base*>(&Common_mempool));
    if (root == nullptr)
    {
        return;
    }
    root = xml_find(root, "Table");
    if (root == nullptr)
    {
        return;
    }
    root = xml_find(root, "assassin");
    if (root == nullptr)
    {
        return;
    }
    xml_element* level_list = xml_find(root, "levels");
    if (level_list == nullptr)
    {
        return;
    }
    for (xml_element* entry = xml_find(level_list, "level"); entry != nullptr; entry = xml_find_next(level_list, entry, "level"))
    {
        if (num_levels > 9)
        {
            return;
        }
        xml_element* bodyguard = xml_find(entry, "bodyguard");
        if (bodyguard == nullptr)
        {
            return;
        }
        assassin_definition& def = levels[num_levels];
        def.reset();
        for (xml_element* squad = xml_find(bodyguard, "squad"); squad != nullptr && def.bodyguard_def_list.size() < def.bodyguard_def_list.capacity(); squad = xml_find_next(bodyguard, squad, "squad"))
        {
            const char* name = xml_get_string(squad, nullptr);
            if (name != nullptr)
            {
                const squad_definition* squad_def = squad_definition_from_name(name);
                if (squad_def != nullptr)
                {
                    def.bodyguard_def_list.add(squad_def);
                }
            }
        }
        xml_element* e = xml_find(entry, "num_bureaucrats_to_spawn");
        if (e == nullptr)
        {
            return;
        }
        def.num_bureaucrats_to_spawn = xml_get_int(e, 0);
        e = xml_find(entry, "reward");
        if (e == nullptr)
        {
            return;
        }
        def.reward = xml_get_int(e, 0);
        e = xml_find(entry, "num_bureaucrats_per_level");
        if (e == nullptr)
        {
            return;
        }
        def.num_bureaucrats_per_level = xml_get_int(e, 0);
        num_levels++;
    }
    Common_mempool.restore_to_mark(mark);
}

// 0x007A8D30 (:443)
void activity_assassin_data::reset_activity_instance()
{
    for (int i = 0; i < orders_handles.size(); i++)
    {
        gfm_orders* orders = Orders_manager.get_orders_from_handle(orders_handles[i]);
        if (orders != nullptr)
        {
            Orders_manager.destroy_orders(orders);
        }
    }
    office_handle = 0xFFFFFFFF;
    office_bldg_bbox.min.zero();
    office_bldg_bbox.max.zero();
    bureaucrat_handles.clear();
    orders_handles.clear();
    cover_is_blown = false;
}

// 0x007968C0 (:463). No caller in rfg.exe.
bool activity_assassin_data::spawn_bureaucrats(const rfg_mover* obj)
{
    static const human_spawn_group_info* bureaucrat_group = sog_get_human_group("EDF Bureaucrat");
    if (bureaucrat_group == nullptr)
    {
        return false;
    }
    const human_info* info = srm_pick_loaded_human_info(bureaucrat_group);
    if (info == nullptr)
    {
        return false;
    }
    for (int i = 0; i < levels[current_level].num_bureaucrats_to_spawn; i++)
    {
        npc_spawn_param_list list(NPC_MANAGER_ACTIVITY_ASSASSIN);
        list.spawn_pos = obj->pos;
        list.assign_spawn_parent(const_cast<rfg_mover*>(obj));
        npc_spawn_params params;
        params.reset();
        params.hum_info = info;
        list.npc_spawn_info.add(params);
        list.spawn_flags.spawn_on_node = true;
        if (object_spawn_do_spawn(list) == SPAWN_STATUS_FAILURE)
        {
            return false;
        }
        for (int n = 0; n < list.npc_spawn_info.size(); n++)
        {
            npc* np = list.npc_spawn_info[n].np;
            if (np == nullptr)
            {
                return false;
            }
            np->npc_flags.bureaucrat = 1;
            bureaucrat_handles.add(np->get_handle());
        }
    }
    return true;
}

// 0x007B17C0 (:503). No caller in rfg.exe.
bool activity_assassin_data::spawn_bodyguards(const rfg_mover* obj)
{
    if (bureaucrat_handles.size() < 1)
    {
        return false;
    }
    for (int b = 0; b < bureaucrat_handles.size(); b++)
    {
        const assassin_definition& def = levels[current_level];
        for (int s = 0; s < def.bodyguard_def_list.size(); s++)
        {
            const squad_definition* squad_def = def.bodyguard_def_list[s];
            if (squad_def == nullptr)
            {
                return false;
            }
            squad_spawn_parameters params;
            params.reset();
            params.orders.location_target = obj->pos;
            params.orders.orders_type = GFM_ORDERS_TYPE_DEFEND;
            params.orders.object_target = bureaucrat_handles[b];
            params.squad.squad_flags.disallow_reassignment = 1;
            params.orders.ofp = ACTIVITY_ORDERS_FLAGS;
            const npc* bureaucrat = World.object_from_handle<npc>(bureaucrat_handles[b]);
            if (bureaucrat == nullptr)
            {
                return false;
            }
            params.squad.squad_pos = bureaucrat->pos;
            params.squad.squad_orient = obj->orient;
            params.squad.squad_def = squad_def;
            params.squad.veh_spawn_group = nullptr;
            if (params.spawn_squad() != nullptr && params.created_orders != nullptr)
            {
                orders_handles.add(params.created_orders->get_orders_handle());
            }
        }
    }
    return true;
}

// 0x007572C0 (:552)
bool activity_assassin_data::is_local_player_in_office_bbox()
{
    return ix_point_in_box(player_get_local_player()->pos, office_bldg_bbox.min, office_bldg_bbox.max);
}

// 0x007B1960 (:560). Original: the walk steps over the entry after one it removes, and a null npc is dereferenced.
void activity_assassin_data::monitor_bureaucrats()
{
    for (int i = 0; i < bureaucrat_handles.size(); i++)
    {
        npc* np = assassin_npc_from_handle(bureaucrat_handles[i]);
        if (np->is_killed())
        {
            num_bureaucrats_killed++;
            hud_message_add("Kill EDF bureaucrats to gain a control boost.", 5.0f, nullptr, nullptr, nullptr);
            gfm_change_edf_control((float)levels[current_level].reward, nullptr, nullptr, nullptr);
            bureaucrat_handles.remove(i);
        }
        else
        {
            const vector& pos = player_get_local_player()->pos;
            const float dx = pos.x - np->pos.x;
            const float dz = pos.z - np->pos.z;
            if (sqrtf(dx * dx + dz * dz) > 200.0f) // Magic number: 200 metres
            {
                hud_message_add("A bureaucrat got away!", 5.0f, nullptr, nullptr, nullptr);
                bureaucrat_handles.remove(i);
                World.destroy_object(np, false);
            }
        }
    }
}
