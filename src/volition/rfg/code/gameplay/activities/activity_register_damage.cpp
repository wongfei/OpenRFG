// Original: volition/rfg/code/gameplay/activities/activity_register_damage.cpp

#include "volition/rfg/code/gameplay/activities/activity_register_damage.h"

#include "volition/rfg/code/gameplay/activities/activity_area_defense_internal.h"
#include "volition/rfg/code/gameplay/activities/activity_internal.h"
#include "volition/rfg/code/gameplay/activities/activity_killing_spree_internal.h"
#include "volition/rfg/code/gameplay/activities/activity_mayhem_internal.h"
#include "volition/rfg/code/gameplay/activities/activity_vrail_internal.h"
#include "volition/rfg/code/destruction/rfg_mover.h"
#include "volition/rfg/code/gamewide/stats.h"
#include "volition/rfg/code/gamewide/tweak_table.h"
#include "volition/rfg/code/level/world.h"
#include "volition/rfg/code/multi/multi_core/include/multi.h"
#include "volition/rfg/code/nvidia_highlights.h"
#include "volition/rfg/code/objects/gameplay_properties.h"
#include "volition/rfg/code/objects/humans/human.h"
#include "volition/rfg/code/objects/humans/human_combat_turret.h"
#include "volition/rfg/code/objects/movers/general_mover.h"
#include "volition/rfg/code/objects/object.h"
#include "volition/rfg/code/objects/object_item.h"
#include "volition/rfg/code/objects/turret.h"
#include "volition/rfg/code/vehicle/vehicle_class.h"
#include "volition/rfg/code/vehicle/vehicle_info.h"
#include "volition/vlib/code/os/cs_wrapper.h"
#include "volition/vlib/code/util/hash_table.h"

#include "keen/base/systemtimer.hpp"

#include <math.h>

typedef hash_table<unsigned int, &uint_hash, 500, unsigned short> register_marked_list; // Magic number: 500, from rfg.pdb

cs_wrapper Activity_register_cs(0); // 0x02229C00
bool Activity_register_damage_debug; // 0x022295C8
static float Human_cost; // 0x022295CC, from the tweak table
static float Building_cost_per_volume = 1.0e-4f; // 0x01651378
static tweak_table_item Tweak_table_item_Building_cost_per_volume("Building_cost_per_volume", &Building_cost_per_volume); // 0x022487C4
static float Vehicle_cost = 0.1f; // 0x0165137C
static tweak_table_item Tweak_table_item_Vehicle_cost("Vehicle_cost", &Vehicle_cost); // 0x02246034
static float Turret_cost = 0.1f; // 0x01651380
static tweak_table_item Tweak_table_item_Turret_cost("Turret_cost", &Turret_cost); // 0x02245898
static tweak_table_item Tweak_table_item_Human_cost("Human_cost", &Human_cost);
static register_marked_list Register_marked_list_a(false); // 0x01654058
static register_marked_list Register_marked_list_b(false); // 0x01655000
static register_marked_list* Register_current_marked_list = &Register_marked_list_a; // 0x01651370
static register_marked_list* Register_previous_marked_list = &Register_marked_list_b; // 0x01651374
static timestamp Register_swap_lists_timer; // 0x01655FA8
static unsigned int s_lastTimeDestructionCaptured; // 0x02273D4C
static unsigned int s_lastTimeExplosionCaptured; // 0x02273D50

// The activities told about damage.
static activity_base* const REGISTER_DAMAGE_ACTIVITY_LIST[4] = {
    &Vehicle_rail_activity_data, &Mayhem_activity, &Killing_spree_data, &Area_defense_activity_data,
}; // 0x012DB0C4

// That activity of the list, or null if it is disabled.
static activity_base* register_activity(int index)
{
    activity_base* activity = REGISTER_DAMAGE_ACTIVITY_LIST[index];
    return !activity->is_disabled() ? activity : nullptr;
}

// 0x00770510 (:84)
void register_mark_object(const object* objp)
{
    Activity_register_cs.enter();
    if (objp == nullptr || objp->get_obj_type() == OT_ZONE)
    {
        Activity_register_cs.leave();
        return;
    }
    if (objp->get_obj_type() == OT_HUMAN)
    {
        // Original: only a dead human is marked, and a null from the second lookup is dereferenced.
        const object* hp = objp->get_handle() == 0xFFFFFFFF ? nullptr : World.object_from_handle(objp->get_handle());
        if (hp == nullptr || hp->get_obj_type() != OT_HUMAN)
        {
            hp = nullptr;
        }
        if (!hp->is_killed())
        {
            Activity_register_cs.leave();
            return;
        }
    }
    if (objp->get_sub_type() == OT_SUB_MOVER_GENERAL)
    {
        // A piece of debris counts as the mover it broke from.
        const object* owner = World.object_from_handle(static_cast<const general_mover*>(objp)->get_original_object());
        if (owner != nullptr)
        {
            objp = owner;
        }
    }
    Register_current_marked_list->insert(objp->get_handle());
    Activity_register_cs.leave();
}

// 0x00799C60 (:123)
void activity_register_damage_mark_objects(const object* obj_a, const object* obj_b)
{
    if (activity_register_damage_is_player_controlled(obj_a) || activity_register_damage_is_player_controlled(obj_b))
    {
        register_mark_object(obj_a);
        register_mark_object(obj_b);
    }
}

// 0x00799CA0 (:142)
void activity_register_damage(const object* obj_a, const object* obj_b, float damage, int original_hitpoints)
{
    if (Multiplayer)
    {
        return;
    }
    if (damage >= 100.0f) // Magic number: smaller hits do not mark
    {
        activity_register_damage_mark_objects(obj_a, obj_b); // rfg.exe inlines it
    }
    for (int i = 0; i < 4; i++)
    {
        activity_base* activity = register_activity(i);
        if (activity != nullptr)
        {
            activity->register_damage(obj_a, obj_b, damage, original_hitpoints);
        }
    }
}

// 0x00759DB0 (:160)
void activity_register_death(const object* target_obj, const object* attacker_obj)
{
    if (Multiplayer)
    {
        return;
    }
    for (int i = 0; i < 4; i++)
    {
        activity_base* activity = register_activity(i);
        if (activity != nullptr)
        {
            activity->register_death(target_obj, attacker_obj);
        }
    }
}

// 0x00759E00 (:176)
void activity_register_building_damage(const rfg_mover* objp, float damage_percent_delta, float original_damage_percent)
{
    if (Multiplayer)
    {
        return;
    }
    for (int i = 0; i < 4; i++)
    {
        activity_base* activity = register_activity(i);
        if (activity != nullptr)
        {
            activity->register_building_damage(objp, damage_percent_delta, original_damage_percent);
        }
    }
    stats_register_building_damage(objp, damage_percent_delta);
}

// 0x00759E70 (:192)
void activity_register_building_destroyed(const rfg_mover* objp)
{
    if (Multiplayer)
    {
        return;
    }
    const unsigned int now = keen::SystemTimer::getCurrentMilliseconds();
    if (now - s_lastTimeDestructionCaptured > 20000) // Magic number: one capture per twenty seconds
    {
        nvidia_highlights_save_video(s_highlightIdDestruction, s_groupIdGameplay, -5000, 15000);
        nvidia_highlights_save_screenshot(s_highlightIdDestruction, s_groupIdGameplay);
        s_lastTimeDestructionCaptured = now;
    }
    for (int i = 0; i < 4; i++)
    {
        activity_base* activity = register_activity(i);
        if (activity != nullptr)
        {
            activity->register_building_destroyed(objp);
        }
    }
    stats_register_building_damage(objp, -1.0f); // Magic number: -1 means destroyed
}

// 0x00759F20 (:220)
void activity_register_explosion(const object* owner, const explosion_info* p_explosion_info)
{
    if (Multiplayer)
    {
        return;
    }
    const unsigned int now = keen::SystemTimer::getCurrentMilliseconds();
    if (now - s_lastTimeExplosionCaptured > 10000) // Magic number: one capture per ten seconds
    {
        nvidia_highlights_save_video(s_highlightIdExplosion, s_groupIdGameplay, -3000, 3000);
        nvidia_highlights_save_screenshot(s_highlightIdExplosion, s_groupIdGameplay);
        s_lastTimeExplosionCaptured = now;
    }
    for (int i = 0; i < 4; i++)
    {
        activity_base* activity = register_activity(i);
        if (activity != nullptr)
        {
            activity->register_explosion(owner, p_explosion_info);
        }
    }
}

// 0x00785790 (:247)
bool activity_register_damage_is_player_controlled(const object* objp)
{
    if (Multiplayer || objp == nullptr)
    {
        return false;
    }
    unsigned int key = objp->get_handle();
    if (objp->get_sub_type() == OT_SUB_MOVER_GENERAL && static_cast<const general_mover*>(objp)->get_original_object() != 0xFFFFFFFF)
    {
        key = static_cast<const general_mover*>(objp)->get_original_object();
    }
    if (Register_current_marked_list->exists(key) || Register_previous_marked_list->exists(key))
    {
        return true;
    }
    switch (objp->get_obj_type())
    {
    case OT_HUMAN:
        return object_is_player(objp);
    case OT_ITEM:
        return object_is_player(static_cast<const item*>(objp)->owner_handle);
    case OT_VEHICLE:
    {
        const vehicle* vp = static_cast<const vehicle*>(objp);
        if (vehicle_player_in_vehicle(vp))
        {
            return true;
        }
        // Magic number: a vehicle the player left stays his for 40 seconds.
        return object_is_player(vp->most_recent_driver) && vp->most_recent_driver_exit_time.time_since() < 40000;
    }
    case OT_TURRET:
        return object_is_player(human_combat_turret_get_operator(static_cast<const turret*>(objp)));
    default:
        return false;
    }
}

// 0x007705C0 (:309)
float activity_register_damage_get_cost(const object* objp)
{
    if (Multiplayer || objp == nullptr)
    {
        return 0.0f;
    }
    switch (objp->get_obj_type())
    {
    case OT_HUMAN:
        return Human_cost;
    case OT_MOVER:
    {
        vector bmin;
        vector bmax;
        const rfg_mover* rmp = World.object_cast<rfg_mover>(const_cast<object*>(objp));
        if (rmp != nullptr)
        {
            rmp->get_local_bbox(bmin, bmax);
            float cost = fabsf((bmax.y - bmin.y) * (bmax.x - bmin.x) * (bmax.z - bmin.z)) * Building_cost_per_volume;
            if (rmp->can_check_building() && rmp->has_building_size() && rmp->has_building_mass())
            {
                cost *= 2.0f;
            }
            const float value = rmp->gameplay_props->monetary_value;
            if (value != -1.0f)
            {
                return value;
            }
            if (rmp->mover_type_bit_flags & 1)
            {
                return cost * 4.0f;
            }
            switch (rmp->get_importance_level())
            {
            case OBJECT_IMPORTANCE_LEVEL_LOW:
                return cost * 1.1f;
            case OBJECT_IMPORTANCE_LEVEL_MEDIUM:
                return cost * 1.5f;
            case OBJECT_IMPORTANCE_LEVEL_HIGH:
                return cost * 2.0f;
            default:
                return cost;
            }
        }
        const general_mover* gmp = World.object_cast<general_mover>(const_cast<object*>(objp));
        if (gmp != nullptr)
        {
            gmp->get_local_bbox(bmin, bmax);
            return fabsf((bmax.z - bmin.z) * (bmax.x - bmin.x) * (bmax.y - bmin.y)) * Building_cost_per_volume;
        }
        return 0.0f;
    }
    case OT_VEHICLE:
    {
        const vehicle_info* info = static_cast<const vehicle*>(objp)->info;
        return info != nullptr ? info->m_value : Vehicle_cost;
    }
    case OT_TURRET:
        return Turret_cost;
    default:
        return 0.0f;
    }
}

// 0x00770860 (:430)
void activity_register_damage_do_frame()
{
    Activity_register_cs.enter();
    if (Register_swap_lists_timer.elapsed())
    {
        register_marked_list* old_current = Register_current_marked_list;
        Register_current_marked_list = Register_previous_marked_list;
        Register_previous_marked_list = old_current;
        Register_current_marked_list->clear();
        Register_swap_lists_timer.set(20000); // Magic number: a mark lasts 20 to 40 seconds
    }
    Activity_register_cs.leave();
}

// 0x007708C0 (:451)
void activity_register_damage_level_init()
{
    Activity_register_cs.enter();
    Register_current_marked_list->clear();
    Register_previous_marked_list->clear();
    Register_swap_lists_timer.set(20000);
    Activity_register_cs.leave();
}

// 0x00759FC0 (:462). Retail strips the debug draw (render-debug-investigation), so the boxes are not drawn.
static void activity_register_damage_draw_box(unsigned int h)
{
}

// 0x007858D0 (:472). Debug render; nothing sets Activity_register_damage_debug.
void activity_register_damage_render_debug()
{
    if (!Activity_register_damage_debug)
    {
        return;
    }
    unsigned int key;
    if (Register_previous_marked_list->get_first_key(key))
    {
        do
        {
            activity_register_damage_draw_box(key);
        } while (Register_previous_marked_list->get_next_key(key, key));
    }
    if (Register_current_marked_list->get_first_key(key))
    {
        do
        {
            activity_register_damage_draw_box(key);
        } while (Register_current_marked_list->get_next_key(key, key));
    }
}
