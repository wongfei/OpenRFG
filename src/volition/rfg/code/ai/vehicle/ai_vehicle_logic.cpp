// Original: volition/rfg/code/ai/vehicle/ai_vehicle_logic.cpp

#include "volition/rfg/code/ai/vehicle/ai_vehicle_logic.h"

#include "volition/rfg/code/ai/ai_building_logic.h"
#include "volition/rfg/code/ai/ai_system.h"
#include "volition/rfg/code/ai/ambient_edf_action_manager.h"
#include "volition/rfg/code/ai/belief_iterator.h"
#include "volition/rfg/code/ai/beliefs.h"
#include "volition/rfg/code/ai/combat/combat_common.h"
#include "volition/rfg/code/ai/goals.h"
#include "volition/rfg/code/ai/planning.h"
#include "volition/rfg/code/ai/road_route.h"
#include "volition/rfg/code/ai/vehicle/vehicle_ai_actions.h"
#include "volition/rfg/code/destruction/rfg_mover.h"
#include "volition/rfg/code/gameplay/activities/activity.h"
#include "volition/rfg/code/gameplay/activities/vrail_system.h"
#include "volition/rfg/code/gameplay/gameflow/alert_level_manager.h"
#include "volition/rfg/code/gameplay/gameflow/gfm_orders_manager.h"
#include "volition/rfg/code/gamewide/tweak_table.h"
#include "volition/rfg/code/havok/havok.h"
#include "volition/rfg/code/level/world.h"
#include "volition/rfg/code/objects/gameplay_properties.h"
#include "volition/rfg/code/objects/humans/human_combat_turret.h"
#include "volition/rfg/code/objects/humans/human_teams.h"
#include "volition/rfg/code/objects/humans/npc.h"
#include "volition/rfg/code/objects/humans/player.h"
#include "volition/rfg/code/objects/object_path_road.h"
#include "volition/rfg/code/objects/object_squad.h"
#include "volition/rfg/code/objects/turret.h"
#include "volition/rfg/code/pathfinding/bb_pathfinding.h"
#include "volition/rfg/code/pathfinding/road_heap.h"
#include "volition/rfg/code/vehicle/automobile.h"
#include "volition/rfg/code/vehicle/vehicle_class.h"
#include "volition/rfg/code/vehicle/vehicle_flyer.h"
#include "volition/rfg/code/vehicle/vehicle_info.h"
#include "volition/rfg/code/vehicle/vehicle_interaction.h"
#include "volition/rfg/code/vehicle/vehicle_system.h"
#include "volition/rfg/code/vehicle_ai/vehicle_ai.h"
#include "volition/rfg/code/vehicle_ai/vehicle_navmesh_common.h"
#include "volition/rfg/code/vehicle_ai/vehicle_navmesh_path_create.h"
#include "volition/rfg/code/vehicle_ai/vehicle_path_request.h"
#include "volition/rfg/code/weapons/weapons.h"
#include "volition/vlib/code/math/bbox.h"
#include "volition/vlib/code/math/floating.h"
#include "volition/vlib/code/math/ix.h"
#include "volition/vlib/code/math/rand.h"
#include "volition/vlib/code/vlib_core/vlib_assert.h"

#include <math.h>

// Zero until tweak_table.xtbl loads.
float Tank_turret_smooth; // 0x01D6774C
static tweak_table_item Tweak_table_item_Tank_turret_smooth("Tank_turret_smooth", &Tank_turret_smooth); // (0x01068030)

static const int VEHICLE_AI_NUM_INTERACTION_SLOTS = 8; // vi_instance::humans is a farray<vi_human_manager,8>

// 0x0068AB60 (:124): no while a known important building is not an ai building.
bool ai_should_smash_buildings(ai_system* aip)
{
    belief_obj_iterator<rfg_mover> it;
    it.init(aip, AI_BELIEF_BUILDING);
    for (; it; ++it)
    {
        rfg_mover* building = it;
        if (building->flags.is_ai_building == 0 && 1 < (int)building->gameplay_props->importance)
        {
            return false;
        }
    }
    return true;
}

// 0x00645410 (:142)
static bool should_ignore_path_failure(const vehicle* vp)
{
    unsigned int patrol_vehicle;
    if (vrail_get_patrol_vehicle(patrol_vehicle) && vp->get_handle() == patrol_vehicle)
    {
        return true;
    }
    const human* driver = vehicle_get_first_passenger(vp, VSI_DRIVER);
    if (driver == nullptr || driver->get_obj_type() != OT_HUMAN || driver->get_sub_type() != OT_SUB_HUMAN_NPC ||
        static_cast<const npc*>(driver)->npc_flags.ambient_spawn != 0)
    {
        return false;
    }
    return driver->hflags.limited_vehicle_exit != 0 || driver->hflags.driverless_exit_only != 0 || driver->hflags.disallow_vehicle_exit != 0;
}

// 0x005DC1E0 (:1442)
static bool human_is_entering_vehicle(const human* hp, const vehicle* vp)
{
    return human_in_vehicle_transition(hp, vp, VI_INTERACTION_ENTER);
}

// 0x0066E740 (:176)
static bool in_player_squad_in_vehicle(const npc* np, const vehicle* vp)
{
    if (np == nullptr || vp == nullptr || np->get_ai()->get_squad_handle() == 0xFFFFFFFF)
    {
        return false;
    }
    object_squad* squad = static_cast<object_squad*>(World.object_from_handle(np->get_ai()->get_squad_handle()));
    if (squad == nullptr || squad->get_obj_type() != OT_SQUAD)
    {
        return false;
    }
    player* pl = squad->get_player();
    return pl != nullptr && vp->get_handle() == pl->vehicle_handle;
}

// 0x005F4100 (:205): the driver of a turret vehicle is a gunner on a gunner hint; a passenger on a turret is a passenger only on a passenger hint.
static ai_vehicle_seat_types get_seat_type_from_hint(const vehicle* vp, vehicle_seat_index seat_index, ai_vehicle_seat_types hint_type)
{
    if (seat_index == VSI_DRIVER)
    {
        if (hint_type == AI_VEHICLE_SEAT_TYPE_GUNNER && vehicle_get_seat_turret(vp, VSI_DRIVER) != nullptr)
        {
            return AI_VEHICLE_SEAT_TYPE_GUNNER;
        }
        return AI_VEHICLE_SEAT_TYPE_DRIVER;
    }
    if (seat_index == VSI_INVALID)
    {
        return AI_VEHICLE_SEAT_TYPE_UNDEFINED;
    }
    if (vehicle_get_seat_turret(vp, seat_index) != nullptr && hint_type != AI_VEHICLE_SEAT_TYPE_PASSENGER)
    {
        return AI_VEHICLE_SEAT_TYPE_GUNNER;
    }
    return AI_VEHICLE_SEAT_TYPE_PASSENGER;
}

// 0x0066E7A0 (:247): the seat test of ai_vehicle_is_seat_available over every passenger seat.
bool passenger_seat_available(const vehicle* vp, const npc* np)
{
    const int num_seats = vehicle_get_num_seats(vp);
    for (int seat = VSI_PASSENGER_1; seat < num_seats; seat++)
    {
        if (ai_vehicle_is_seat_available(vp, np, (vehicle_seat_index)seat))
        {
            return true;
        }
    }
    return false;
}

// 0x0066E880 (:269)
turret* gunner_seat_available(const vehicle* vp, const npc* np, bool passenger_only, vehicle_seat_index* seat_idx)
{
    if (seat_idx != nullptr)
    {
        *seat_idx = VSI_INVALID;
    }
    const human* driver = vehicle_passenger_in_entering_or_reserving_seat(vp, VSI_DRIVER);
    for (int i = 0; i < vp->m_num_turret_mounts; i++)
    {
        const vehicle_turret_mount& mount = vp->m_turret_mounts[i];
        if (mount.attached_turret == 0xFFFFFFFF)
        {
            continue;
        }
        turret* tp = static_cast<turret*>(World.object_from_handle(mount.attached_turret));
        if (tp == nullptr || tp->get_obj_type() != OT_TURRET || !tp->is_functional(false) || tp->turret_flags.locked_turret)
        {
            continue;
        }
        const vehicle_seat_index seat = mount.info->seat;
        if (seat == VSI_INVALID || (passenger_only && seat == VSI_DRIVER))
        {
            continue;
        }
        if (tp->disable_for_npcs_time.valid() && !tp->disable_for_npcs_time.elapsed() &&
            !(np != nullptr && np->vehicle_handle == vp->get_handle() && np->vehicle_seat_idx == seat))
        {
            continue;
        }
        // 3000 ms: a turret stuck for that long is no use unless someone else is going to drive.
        if (seat != VSI_DRIVER && tp->is_stuck(3000) && !(driver != nullptr && driver != np))
        {
            continue;
        }
        if (ai_vehicle_is_seat_available(vp, np, seat))
        {
            if (seat_idx != nullptr)
            {
                *seat_idx = seat;
            }
            return tp;
        }
    }
    return nullptr;
}

// 0x00645470 (:336)
human* get_first_gunner(const vehicle* vp)
{
    for (int i = 0; i < vp->m_num_turret_mounts; i++)
    {
        const vehicle_turret_mount& mount = vp->m_turret_mounts[i];
        turret* tp = nullptr;
        if (mount.attached_turret != 0xFFFFFFFF)
        {
            tp = static_cast<turret*>(World.object_from_handle(mount.attached_turret));
            if (tp != nullptr && tp->get_obj_type() != OT_TURRET)
            {
                tp = nullptr;
            }
        }
        // Original quirk: is_functional is called on a null turret when the mount is empty or the handle is stale.
        if (tp->is_functional(false) && mount.info->seat != VSI_INVALID)
        {
            human* hp = vehicle_passenger_in_seat(vp, mount.info->seat);
            if (hp != nullptr)
            {
                return hp;
            }
        }
    }
    return nullptr;
}

// 0x005F4150 (:363)
static bool can_drive_after_bad_pathfind(const npc* np, const vehicle* vp)
{
    if (np == nullptr)
    {
        return false;
    }
    if (np->npc_flags.ambient_spawn == 0 && (np->hflags.disallow_vehicle_exit != 0 || np->hflags.limited_vehicle_exit != 0 || np->hflags.driverless_exit_only != 0))
    {
        return true;
    }
    if (np->npc_flags.activity_driver_vr != 0 || np->npc_flags.activity_pursuit_vr != 0)
    {
        return true;
    }
    if (vp->get_sub_type() == OT_SUB_VEHICLE_FLYER)
    {
        return true;
    }
    return np->get_ai() != nullptr && np->get_ai()->planner.get_current_goal() == drive_to_location_goal::get_instance();
}

// 0x005F41A0 (:399)
bool ai_vehicle_ensure_seat_turret_manned(npc* np, vehicle* vp, turret* tp)
{
    if (np == nullptr || vp == nullptr || np->vehicle_handle != vp->get_handle() || np->npc_flags.no_turret_attacks != 0)
    {
        return false;
    }
    if (tp == nullptr)
    {
        tp = vehicle_get_seat_turret(vp, np->vehicle_seat_idx);
        if (tp == nullptr)
        {
            return false;
        }
    }
    if (tp->turret_flags.locked_turret)
    {
        return false;
    }
    return human_combat_man_turret(np, tp);
}

// 0x006ABD80 (:423)
ai_results ai_vehicle_use_turret(npc* np, turret* tp, vehicle* vp, bool fire_at_alternate, object* objp, bool fire_even_if_not_in_los, vector pos)
{
    ai_system* aip = np->get_ai();
    ai_perception& per = aip->perception;
    weapon* wp = tp->get_weapon_inst();
    if (wp == nullptr)
    {
        return AI_RESULT_FAILURE;
    }
    // The average is rebuilt below only for thrown weapons, and stays zero otherwise.
    vector average_velocity = tp->target_average_velocity;
    tp->target_average_velocity = ZERO_VECTOR;
    if (!tp->is_functional(false))
    {
        return AI_RESULT_FAILURE;
    }
    // Upside down (0.2 is compared as a double in rfg.exe).
    if ((double)vp->orient.uvec.y < 0.20000000298023224)
    {
        return AI_RESULT_FAILURE;
    }
    if (vp->get_supporting_rigid_body() == nullptr && vp->get_sub_type() != OT_SUB_VEHICLE_FLYER)
    {
        return AI_RESULT_FAILURE;
    }
    const human* target = human_combat_get_target(np);
    bool alternate_fire = false;
    if (vehicle_is_tank(vp) && np->combat.targets_building_handle != 0xFFFFFFFF && vp->info->vehicle_classification != BVT_ART_TANK)
    {
        rfg_mover* building = World.object_from_handle<rfg_mover>(np->combat.targets_building_handle);
        if (building != nullptr && building->gameplay_props != nullptr)
        {
            alternate_fire = 1 < (int)building->gameplay_props->importance;
        }
    }
    if (target != nullptr)
    {
        if (tp->can_hit_target(target->pos) &&
            (static_cast<const object*>(target) != per.get_attention_target() || per.get_attention_mode() != ATTENTION_LOOK_AT_OBJECT))
        {
            per.pay_attention(target, ATTENTION_LOOK_AT_OBJECT, ATTENTION_STEERING_USE_DEFAULT, ATTENTION_AWARENESS_NORMAL);
        }
        if (!per.enemy_in_los(target->get_handle()) && !fire_even_if_not_in_los)
        {
            vector effective_pos;
            if (per.get_effective_target_pos(effective_pos, target, nullptr, nullptr, false))
            {
                tp->aim_turret(effective_pos);
            }
            return AI_RESULT_FAILURE;
        }
        if (per.can_engage_target_reason(target) != AI_ENGAGE_ALLOWED)
        {
            return AI_RESULT_FAILURE;
        }
        if (per.get_attention_mode() != ATTENTION_LOOK_AT_OBJECT || static_cast<const object*>(target) != per.get_attention_target())
        {
            per.pay_attention(target, ATTENTION_LOOK_AT_OBJECT, ATTENTION_STEERING_USE_DEFAULT, ATTENTION_AWARENESS_NORMAL);
        }
    }
    vector aim_pos = ZERO_VECTOR;
    bool aiming_at_alternate = false;
    if (target != nullptr && !human_is_dead(target))
    {
        per.get_effective_target_pos(aim_pos, target, nullptr, nullptr, false);
        if (wp->w_info->ammo_type == WEAPON_AMMO_TYPE_THROWN)
        {
            const vehicle* target_vehicle = human_in_vehicle(target);
            vector target_velocity = ZERO_VECTOR;
            if (target_vehicle != nullptr)
            {
                vehicle_get_velocity(target_vehicle, target_velocity);
            }
            else
            {
                target_velocity = target->actual_velocity;
            }
            const float kept = Tank_turret_smooth - 1.0f;
            average_velocity.x = (kept * average_velocity.x + target_velocity.x) / Tank_turret_smooth;
            average_velocity.y = (kept * average_velocity.y + target_velocity.y) / Tank_turret_smooth;
            average_velocity.z = (kept * average_velocity.z + target_velocity.z) / Tank_turret_smooth;
            tp->target_average_velocity = average_velocity;
        }
    }
    else
    {
        if (!fire_at_alternate || (objp == nullptr && pos == ZERO_VECTOR))
        {
            // 20 m straight ahead of the vehicle.
            tp->aim_turret(vp->pos + vp->orient.fvec * 20.0f);
            return AI_RESULT_IN_PROGRESS;
        }
        target = nullptr;
        aim_pos = objp != nullptr ? objp->pos : pos;
        aiming_at_alternate = true;
    }
    bool electrocuted = false;
    if (aip->electrocution_timer.valid() && !aip->electrocution_timer.elapsed())
    {
        electrocuted = true;
        vector jitter;
        jitter.rand_sphere();
        aim_pos += jitter * fl_rand_range(0.5f, 5.0f);
    }
    if (!tp->turret_flags.vehicle_turret && !tp->can_hit_target(aim_pos))
    {
        tp->aim_turret(vp->pos + vp->orient.fvec * 20.0f);
        return AI_RESULT_IN_PROGRESS;
    }
    const bool aimed = tp->aim_turret(aim_pos);
    if (electrocuted)
    {
        return AI_RESULT_IN_PROGRESS;
    }
    if (!per.can_hit_close_to_target() && !(aiming_at_alternate && aimed))
    {
        return AI_RESULT_IN_PROGRESS;
    }
    if (vp->info->vehicle_classification == BVT_ART_TANK)
    {
        const weapon* tank_weapon = tp->get_weapon_inst();
        if (tank_weapon != nullptr)
        {
            const float min_dist = tank_weapon->w_info->min_engagement_dist;
            if ((vp->pos - aim_pos).mag_squared() < min_dist * min_dist)
            {
                return AI_RESULT_IN_PROGRESS;
            }
        }
    }
    ai_combat_fire_turret_at_target(np, tp, target, alternate_fire);
    return AI_RESULT_IN_PROGRESS;
}

// 0x006AC490 (:602)
bool ai_vehicle_use_turret_if_available(npc* np, vehicle* vp, bool fire_even_if_no_los, bool fire_at_alternate, object* objp, vector pos)
{
    if (np->npc_flags.no_turret_attacks != 0)
    {
        return false;
    }
    ai_results result = AI_RESULT_FAILURE;
    turret* tp = vehicle_get_seat_turret(vp, np->vehicle_seat_idx);
    if (tp != nullptr && static_cast<npc*>(human_combat_turret_get_operator(tp)) == np)
    {
        result = ai_vehicle_use_turret(np, tp, vp, fire_at_alternate, objp, fire_even_if_no_los, pos);
    }
    return result != AI_RESULT_FAILURE;
}

// 0x00645580 (:657): a driver turret counts as manned whoever sits there.
const turret* ai_vehicle_get_first_manned_turret(const vehicle* vp)
{
    for (int i = 0; i < vp->m_num_turret_mounts; i++)
    {
        const vehicle_turret_mount& mount = vp->m_turret_mounts[i];
        if (mount.attached_turret == 0xFFFFFFFF)
        {
            continue;
        }
        const turret* tp = static_cast<const turret*>(World.object_from_handle(mount.attached_turret));
        if (tp == nullptr || tp->get_obj_type() != OT_TURRET || !tp->is_functional(false) || tp->turret_flags.locked_turret)
        {
            continue;
        }
        const vehicle_seat_index seat = mount.info->seat;
        if (seat != VSI_INVALID && (seat == VSI_DRIVER || vehicle_passenger_in_seat(vp, seat) != nullptr))
        {
            return tp;
        }
    }
    return nullptr;
}

// 0x006454F0 (:624)
bool ai_vehicle_has_manned_turret(const vehicle* vp)
{
    return ai_vehicle_get_first_manned_turret(vp) != nullptr;
}

// 0x0066E9E0 (:695)
bool ai_vehicle_has_available_seat(const vehicle* vp, const npc* np, ai_vehicle_seat_types seat_type)
{
    if (vp == nullptr)
    {
        return false;
    }
    switch (seat_type)
    {
    case AI_VEHICLE_SEAT_TYPE_DRIVER:
        return ai_vehicle_is_seat_available(vp, np, VSI_DRIVER);
    case AI_VEHICLE_SEAT_TYPE_PASSENGER:
        return passenger_seat_available(vp, np);
    case AI_VEHICLE_SEAT_TYPE_GUNNER:
        return gunner_seat_available(vp, np, false, nullptr) != nullptr;
    default:
        keen::debug::nativePrintArgs("Int3 critical function at %s(%d)\n");
        return false;
    }
}

// 0x00645610 (:722): nobody else riding in, entering or reserving the seat; a reservation by a dead human does not count.
bool ai_vehicle_is_seat_available(const vehicle* vp, const npc* np, vehicle_seat_index seat_idx)
{
    const unsigned int own_handle = np == nullptr ? 0xFFFFFFFF : np->get_handle();
    for (int i = 0; i < VEHICLE_AI_NUM_INTERACTION_SLOTS; i++)
    {
        const vi_human_manager& hm = vp->vii.humans[i];
        if (hm.handle == 0xFFFFFFFF)
        {
            continue;
        }
        vehicle_seat_index seat = VSI_INVALID;
        if (hm.interaction == VI_INTERACTION_RIDE)
        {
            seat = hm.seat;
        }
        else if (hm.interaction == VI_INTERACTION_RESERVE || hm.interaction == VI_INTERACTION_ENTER)
        {
            seat = hm.enter_data.destination_seat;
        }
        if (seat != seat_idx || hm.interaction == VI_INTERACTION_NONE || hm.handle == own_handle)
        {
            continue;
        }
        if (hm.interaction != VI_INTERACTION_RESERVE)
        {
            return false;
        }
        const object* reserving = World.object_from_handle(hm.handle);
        if (reserving != nullptr && reserving->get_obj_type() == OT_HUMAN && !reserving->is_killed())
        {
            return false;
        }
    }
    return true;
}

// 0x0068AC00 (:752)
bool ai_vehicle_is_usable_and_available(const vehicle* vp, const npc* np, ai_vehicle_seat_types seat_type)
{
    return ai_vehicle_is_usable(vp, np, seat_type) && ai_vehicle_has_available_seat(vp, np, seat_type);
}

// 0x0066EA50 (:773)
bool ai_vehicle_is_usable(const vehicle* vp, const npc* np, ai_vehicle_seat_types seat_type)
{
    const ai_system* aip = nullptr;
    if (np != nullptr)
    {
        aip = np->get_ai();
        if (np->hflags.disallow_vehicle_drive != 0 && seat_type == AI_VEHICLE_SEAT_TYPE_DRIVER)
        {
            return false;
        }
    }
    if (vp == nullptr || !vehicle_is_usable(np, nullptr, vp, false, false) || vp->get_sub_type() == OT_SUB_VEHICLE_WALKER)
    {
        return false;
    }
    if (vp->m_flags.player_may_capture != 0 && vehicle_get_hit_points(vp) <= (unsigned int)vp->info->m_abandon_hitpoints)
    {
        return false;
    }
    if (np != nullptr && np->current_team == HUMAN_TEAM_MARAUDER)
    {
        if (aip == nullptr)
        {
            return false;
        }
        if (np->npc_flags.convoy_squad == 0 && aip->is_in_player_squad(true) == nullptr && vp->info->default_team != HUMAN_TEAM_MARAUDER)
        {
            return false;
        }
    }
    player* squad_player = nullptr;
    bool in_players_vehicle = false;
    bool in_vehicle = false;
    if (np != nullptr)
    {
        squad_player = aip->is_in_player_squad(true);
        if (squad_player != nullptr && squad_player->vehicle_handle == vp->get_handle())
        {
            in_players_vehicle = true;
        }
        in_vehicle = human_in_vehicle(np, vp);
        if (np->npc_flags.only_ride_with_player != 0)
        {
            const player* pl = squad_player != nullptr ? squad_player : player_get_local_player();
            if (pl != nullptr && !human_in_vehicle(pl, vp) && !human_is_entering_vehicle(pl, vp))
            {
                return false;
            }
        }
        if (aip->get_squad_handle() != 0xFFFFFFFF && !aip->usable_squad_vehicle(vp))
        {
            return false;
        }
    }
    if (!in_vehicle)
    {
        if (np != nullptr && vp->get_sub_type() == OT_SUB_VEHICLE_FLYER && np->npc_flags.man_flyer == 0)
        {
            return false;
        }
        if (!vp->no_npc_entry_timer.elapsed())
        {
            return false;
        }
    }
    // Lying over and not moving: 0.5 is the up vector's y, 1 m/s the speed.
    if ((!in_players_vehicle || !in_vehicle) && vp->get_sub_type() != OT_SUB_VEHICLE_FLYER && vp->orient.uvec.y < 0.5f)
    {
        float speed;
        vehicle_get_speed(vp, speed);
        if (speed < 1.0f)
        {
            return false;
        }
    }
    if (!in_players_vehicle)
    {
        if (vp->m_flags.m_on_fire != 0)
        {
            if (vp->m_flags.m_invulnerable == 0)
            {
                return false;
            }
        }
        else if (!in_vehicle)
        {
            if (vp->m_flags.m_engine_smoking != 0 && (np == nullptr || np->npc_flags.activity_driver_vr == 0) && vp->get_sub_type() != OT_SUB_VEHICLE_FLYER)
            {
                return false;
            }
            if (np != nullptr && human_teams_are_unfriendly(np, vp->info->default_team))
            {
                const object_squad* squad = np->get_ai()->get_squad();
                if (squad == nullptr || squad->squad_vehicle != vp->get_handle())
                {
                    return false;
                }
            }
        }
    }
    if (seat_type == AI_VEHICLE_SEAT_TYPE_DRIVER)
    {
        if (vp->m_flags.disable_for_npc_drivers != 0)
        {
            return false;
        }
        if (np != nullptr && np->should_active_despawn())
        {
            const player* pl = player_get_local_player();
            if (pl != nullptr && pl->vehicle_handle == vp->get_handle())
            {
                return false;
            }
        }
        if (!vp->disable_for_npc_drivers_timer.elapsed() && !can_drive_after_bad_pathfind(np, vp))
        {
            return false;
        }
        if (np != nullptr)
        {
            // 10: the last passenger seat. A hostile rider, unless tied up, stops a driver unless both are in an edf ambient action.
            for (int seat = VSI_PASSENGER_1; seat <= 10; seat++)
            {
                human* rider = vehicle_passenger_in_or_entering_seat(vp, (vehicle_seat_index)seat);
                if (rider == nullptr || static_cast<const human*>(np) == rider)
                {
                    continue;
                }
                const npc* rider_npc = World.object_cast<npc>(rider);
                if ((rider_npc != nullptr && rider_npc->npc_flags.tied_up != 0) || human_teams_are_allies(rider, np))
                {
                    continue;
                }
                if (rider_npc == nullptr)
                {
                    return false;
                }
                if (!amb_edf_action_participant(rider_npc) && !amb_edf_action_support_participant(rider_npc))
                {
                    return false;
                }
                if (!amb_edf_action_participant(np) && !amb_edf_action_support_participant(np))
                {
                    return false;
                }
            }
        }
        return true;
    }
    human* driver = vehicle_passenger_in_entering_or_reserving_seat(vp, VSI_DRIVER);
    if (driver == nullptr)
    {
        unsigned int patrol_vehicle;
        unsigned int patrol_squad;
        if (vrail_get_patrol_vehicle(patrol_vehicle) && patrol_vehicle == vp->get_handle() && vrail_get_patrol_squad(patrol_squad))
        {
            const object_squad* squad = World.object_from_handle<object_squad>(patrol_squad);
            if (squad != nullptr)
            {
                driver = World.object_from_handle<human>(squad->member_array[0]);
            }
        }
        if (driver == nullptr && np != nullptr)
        {
            npc* leader;
            if (ai_vehicle_get_vehicle_squad_leader_is_going_to(np, &leader) == vp)
            {
                driver = leader;
            }
        }
    }
    if (seat_type == AI_VEHICLE_SEAT_TYPE_GUNNER)
    {
        if (!vehicle_has_gunner_seat(vp))
        {
            return false;
        }
    }
    else if (driver == nullptr || driver == static_cast<const human*>(np))
    {
        return false;
    }
    if (np == nullptr)
    {
        return true;
    }
    if (driver != nullptr && !human_teams_are_allies(driver, np) && driver != static_cast<human*>(squad_player))
    {
        return false;
    }
    if (object_is_player(driver) && !in_vehicle && np->cur_state != HS_VEHICLE_ENTERING)
    {
        // Don't chase the player's vehicle: over 3 m/s when it is moving away from the npc, over 5 m/s otherwise.
        vector velocity;
        vehicle_get_velocity(vp, velocity);
        const float speed_sqrd = velocity.mag_squared();
        const float dot = velocity.x * (vp->pos.x - np->pos.x) + velocity.y * (vp->pos.y - np->pos.y) + velocity.z * (vp->pos.z - np->pos.z);
        if ((dot > 0.0f ? 9.0f : 25.0f) < speed_sqrd)
        {
            return false;
        }
    }
    npc* driver_npc = World.object_cast<npc>(driver);
    if (driver_npc == nullptr)
    {
        return true;
    }
    if (vrail_is_driver(driver_npc) && !in_player_squad_in_vehicle(np, vp))
    {
        return false;
    }
    if (driver_npc->should_despawn())
    {
        if (!np->should_despawn() && np->npc_flags.ambient_spawn == 0)
        {
            return false;
        }
    }
    else if (np->should_despawn())
    {
        const ai_system* own_ai = np->get_ai();
        if (own_ai == nullptr || own_ai->amb_action_info.action_type == AMBIENT_PED_ACTION_TYPE_NONE)
        {
            return false;
        }
    }
    return true;
}

// 0x006456C0 (:1106)
ai_vehicle_seat_types ai_vehicle_get_seat_type(const npc* np, const vehicle* vp, ai_vehicle_seat_types hint_type)
{
    if (np == nullptr)
    {
        return AI_VEHICLE_SEAT_TYPE_UNDEFINED;
    }
    if (vp == nullptr && np->vehicle_handle != 0xFFFFFFFF)
    {
        const object* objp = World.object_from_handle(np->vehicle_handle);
        if (objp != nullptr && objp->get_obj_type() == OT_VEHICLE)
        {
            vp = static_cast<const vehicle*>(objp);
        }
    }
    if (human_in_vehicle(np, vp))
    {
        return get_seat_type_from_hint(vp, vehicle_passenger_get_seat(vp, np), hint_type);
    }
    return AI_VEHICLE_SEAT_TYPE_UNDEFINED;
}

// 0x00645730 (:1126)
ai_vehicle_seat_types ai_vehicle_get_entering_seat_type(const npc* np, const vehicle* vp, ai_vehicle_seat_types hint_type)
{
    if (np == nullptr)
    {
        return AI_VEHICLE_SEAT_TYPE_UNDEFINED;
    }
    if (vp == nullptr)
    {
        if (np->vehicle_handle == 0xFFFFFFFF)
        {
            return AI_VEHICLE_SEAT_TYPE_UNDEFINED;
        }
        const object* objp = World.object_from_handle(np->vehicle_handle);
        if (objp == nullptr || objp->get_obj_type() != OT_VEHICLE)
        {
            return AI_VEHICLE_SEAT_TYPE_UNDEFINED;
        }
        vp = static_cast<const vehicle*>(objp);
        if (!human_in_vehicle_transition(np, vp, VI_INTERACTION_ENTER))
        {
            return AI_VEHICLE_SEAT_TYPE_UNDEFINED;
        }
    }
    const int index = vp->vii.find_human(np);
    if (index < 0)
    {
        return AI_VEHICLE_SEAT_TYPE_UNDEFINED;
    }
    return get_seat_type_from_hint(vp, vp->vii.humans[index].enter_data.destination_seat, hint_type);
}

// 0x0068AC40 (:1154)
vehicle* ai_vehicle_find_available_seat_type(const npc* np, ai_vehicle_seat_types seat_type, const vehicle* exclude_vehicle)
{
    belief_obj_iterator<vehicle> it;
    it.init(np->get_ai(), AI_BELIEF_VEHICLE);
    for (; it; ++it)
    {
        vehicle* vp = it;
        if (vp != exclude_vehicle && ai_vehicle_is_usable_and_available(vp, np, seat_type))
        {
            return vp;
        }
    }
    return nullptr;
}

// 0x0068AD50 (:1176)
vehicle* ai_flyer_find_available_seat_type(const npc* np, ai_vehicle_seat_types seat_type, const vehicle* exclude_vehicle)
{
    belief_obj_iterator<vehicle> it;
    it.init(np->get_ai(), AI_BELIEF_VEHICLE);
    for (; it; ++it)
    {
        vehicle* vp = it;
        if (vp->get_sub_type() == OT_SUB_VEHICLE_FLYER && vp != exclude_vehicle && ai_vehicle_is_usable_and_available(vp, np, seat_type))
        {
            return vp;
        }
    }
    return nullptr;
}

// 0x0066F030 (:1201)
static bool man_turret_in_vehicle_check(const npc* np, const ai_system* ai, const vehicle* vp)
{
    if (vp->pos == ZERO_VECTOR)
    {
        return false;
    }
    if ((np->current_team != HUMAN_TEAM_MARAUDER || ai->perception.get_belief(AI_BELIEF_ENEMY_POSITION) == nullptr) && ai->get_squad_handle() != 0xFFFFFFFF)
    {
        object_squad* squad = static_cast<object_squad*>(World.object_from_handle(ai->get_squad_handle()));
        const gfm_orders* orders = squad != nullptr && squad->get_obj_type() == OT_SQUAD ? squad->get_squad_orders() : nullptr;
        object* defended = orders != nullptr && orders->get_orders_type() == GFM_ORDERS_TYPE_DEFEND ? orders->get_target_object() : nullptr;
        if (defended != nullptr)
        {
            // Within 7 m of a defended human; a defended object is measured from its box: 10 m, 0 m for a bounding box.
            bool in_range;
            if (defended->get_obj_type() == OT_HUMAN)
            {
                in_range = vec_dist_squared(defended->pos, vp->pos) <= 49.0f;
            }
            else
            {
                bool result;
                if (np->npc_flags.activity_related != 0 && activity_outside_defensive_perimeter(np, vp->pos, result, nullptr))
                {
                    in_range = !result;
                }
                else
                {
                    const float allowed = defended->get_obj_type() == OT_BOUNDING_BOX ? 0.0f : 10.0f;
                    float dist_sqrd;
                    if (defended->get_sub_type() == OT_SUB_MOVER_RFG)
                    {
                        dist_sqrd = ai_building_find_bbox_dist_squared(vp->pos, static_cast<const rfg_mover*>(defended));
                    }
                    else
                    {
                        vector bmin;
                        vector bmax;
                        defended->get_local_bbox(bmin, bmax);
                        vector local_pos = vp->pos;
                        vec_change_to_local_coords(local_pos, defended->pos, defended->orient);
                        dist_sqrd = bbox_find_distance_squared(bmin, bmax, local_pos);
                    }
                    in_range = dist_sqrd <= allowed * allowed;
                }
            }
            if (!in_range)
            {
                return false;
            }
        }
    }
    const human* driver = vehicle_passenger_in_entering_or_reserving_seat(vp, VSI_DRIVER);
    if (driver != nullptr && human_teams_get_disposition(driver, np) != TEAM_DISPOSITION_ALLIED)
    {
        return false;
    }
    const turret* tp = gunner_seat_available(vp, np, true, nullptr);
    if (tp == nullptr)
    {
        return false;
    }
    const object* target = World.object_from_handle(np->combat.target_handle);
    if (target != nullptr && !tp->can_hit_target(target->pos))
    {
        return false;
    }
    return ai_vehicle_is_usable(vp, np, AI_VEHICLE_SEAT_TYPE_GUNNER);
}

// 0x0068AE10 (:1234)
vehicle* ai_vehicle_locate_for_man_turret_in_vehicle(const npc* np)
{
    const ai_system* ai = np->get_ai();
    if (human_is_driving(np))
    {
        return nullptr;
    }
    const gfm_alert_levels alert_level = gsm_get_alert_level();
    const ai_belief* enemy_position = ai->perception.get_belief(AI_BELIEF_ENEMY_POSITION);
    const bool calm = alert_level == ALERT_LEVEL_GREEN && enemy_position == nullptr;
    player* pl = nullptr;
    vehicle* player_vehicle = nullptr;
    if (ai->get_squad_handle() != 0xFFFFFFFF)
    {
        object_squad* squad = static_cast<object_squad*>(World.object_from_handle(ai->get_squad_handle()));
        if (squad != nullptr && squad->get_obj_type() == OT_SQUAD)
        {
            pl = squad->get_player();
            if (pl == nullptr)
            {
                const gfm_orders* orders = squad->get_squad_orders();
                if (orders != nullptr && orders->get_orders_type() == GFM_ORDERS_TYPE_DEFEND)
                {
                    pl = World.object_cast<player>(orders->get_target_object());
                }
            }
            if (pl != nullptr && pl->vehicle_handle != 0xFFFFFFFF)
            {
                object* objp = World.object_from_handle(pl->vehicle_handle);
                if (objp != nullptr && objp->get_obj_type() == OT_VEHICLE)
                {
                    player_vehicle = static_cast<vehicle*>(objp);
                }
            }
        }
    }
    // The player's escorts stay out of vehicles while the player is on foot.
    if (pl != nullptr && player_vehicle == nullptr && ai->get_squad_handle() == pl->escort_squad_handle)
    {
        return nullptr;
    }
    if (np->vehicle_handle != 0xFFFFFFFF)
    {
        object* objp = World.object_from_handle(np->vehicle_handle);
        if (objp != nullptr && objp->get_obj_type() == OT_VEHICLE)
        {
            vehicle* vp = static_cast<vehicle*>(objp);
            if ((ai_vehicle_get_seat_type(np, vp, AI_VEHICLE_SEAT_TYPE_GUNNER) == AI_VEHICLE_SEAT_TYPE_GUNNER ||
                 ai_vehicle_get_entering_seat_type(np, vp, AI_VEHICLE_SEAT_TYPE_GUNNER) == AI_VEHICLE_SEAT_TYPE_GUNNER) &&
                (!calm || vehicle_passenger_in_seat(vp, VSI_DRIVER) != nullptr))
            {
                return vp;
            }
            return nullptr;
        }
    }
    if (pl != nullptr && player_vehicle != nullptr && ai->perception.get_belief(AI_BELIEF_VEHICLE, player_vehicle) != nullptr &&
        man_turret_in_vehicle_check(np, ai, player_vehicle))
    {
        return player_vehicle;
    }
    belief_obj_iterator<vehicle> it;
    it.init(ai, AI_BELIEF_VEHICLE);
    for (; it; ++it)
    {
        vehicle* vp = it;
        // Within 20 m; not across 7 m of the last known enemy position.
        if (vec_dist_squared(np->pos, vp->pos) > 400.0f)
        {
            continue;
        }
        if (enemy_position != nullptr)
        {
            vector closest;
            ix_closest_point_on_line_segment(closest, enemy_position->pos, np->pos, vp->pos);
            if (vec_dist_squared(closest, enemy_position->pos) < 49.0f)
            {
                continue;
            }
        }
        if (man_turret_in_vehicle_check(np, ai, vp) && (!calm || vehicle_passenger_in_seat(vp, VSI_DRIVER) != nullptr))
        {
            return vp;
        }
    }
    return nullptr;
}

// 0x0068B0D0 (:1338)
vehicle* ai_vehicle_locate_tank(const npc* np)
{
    if (np->vehicle_handle != 0xFFFFFFFF)
    {
        object* objp = World.object_from_handle(np->vehicle_handle);
        if (objp != nullptr && objp->get_obj_type() == OT_VEHICLE && vehicle_is_tank(static_cast<vehicle*>(objp)))
        {
            return static_cast<vehicle*>(objp);
        }
    }
    belief_obj_iterator<vehicle> it;
    it.init(np->get_ai(), AI_BELIEF_VEHICLE);
    for (; it; ++it)
    {
        vehicle* vp = it;
        // 400: within 20 m.
        if (vehicle_is_tank(vp) && vec_dist_squared(np->pos, vp->pos) <= 400.0f && vehicle_passenger_in_seat(vp, VSI_DRIVER) == nullptr &&
            ai_vehicle_is_seat_available(vp, np, VSI_DRIVER))
        {
            return vp;
        }
    }
    return nullptr;
}

// 0x006457B0 (:1384)
human* ai_vehicle_find_human_in_seat_type(const vehicle* vp, ai_vehicle_seat_types seat_type)
{
    if (vp == nullptr)
    {
        return nullptr;
    }
    switch (seat_type)
    {
    case AI_VEHICLE_SEAT_TYPE_DRIVER:
        return vehicle_passenger_in_seat(vp, VSI_DRIVER);
    case AI_VEHICLE_SEAT_TYPE_PASSENGER:
        return vehicle_get_first_passenger(vp, VSI_PASSENGER_1);
    case AI_VEHICLE_SEAT_TYPE_GUNNER:
        return get_first_gunner(vp);
    default:
        keen::debug::nativePrintArgs("Int3 critical function at %s(%d)\n");
        return nullptr;
    }
}

// 0x00645810 (:1413)
vehicle* ai_vehicle_get_vehicle_squad_leader_is_going_to(const npc* np, npc** squad_leaderp)
{
    if (np == nullptr)
    {
        return nullptr;
    }
    if (squad_leaderp != nullptr)
    {
        *squad_leaderp = nullptr;
    }
    const unsigned int squad_handle = np->get_ai()->get_squad_handle();
    if (squad_handle == 0xFFFFFFFF)
    {
        return nullptr;
    }
    object_squad* squad = static_cast<object_squad*>(World.object_from_handle(squad_handle));
    if (squad == nullptr || squad->get_obj_type() != OT_SQUAD)
    {
        return nullptr;
    }
    npc* leader = squad->get_leader();
    if (leader == nullptr || np == leader)
    {
        return nullptr;
    }
    if (squad_leaderp != nullptr)
    {
        *squad_leaderp = leader;
    }
    const ai_action* act = leader->get_ai()->planner.get_current_action();
    if (act == goto_vehicle_action::get_instance())
    {
        return World.object_from_handle<vehicle>(leader->get_ai()->desired_vehicle);
    }
    return nullptr;
}

// 0x0066F290 (:1452)
ai_results ai_vehicle_enter_teleport(npc* np, vehicle* vp, ai_vehicle_seat_types seat_type)
{
    ai_system* aip = np->get_ai();
    if (human_in_vehicle_transition(np, vp, VI_INTERACTION_ENTER))
    {
        return AI_RESULT_IN_PROGRESS;
    }
    vi_enter_data enter_data;
    enter_data.flags.teleport = 1;
    if (seat_type == AI_VEHICLE_SEAT_TYPE_DRIVER)
    {
        enter_data.specified_seat = VSI_DRIVER;
        enter_data.seat_selection_method = VI_SEAT_SELECTION_METHOD_SPECIFIED;
    }
    else if (seat_type == AI_VEHICLE_SEAT_TYPE_PASSENGER)
    {
        enter_data.seat_selection_method = VI_SEAT_SELECTION_METHOD_BEST;
    }
    else if (seat_type == AI_VEHICLE_SEAT_TYPE_GUNNER)
    {
        vehicle_seat_index seat;
        if (gunner_seat_available(vp, np, true, &seat) == nullptr)
        {
            return AI_RESULT_FAILURE;
        }
        enter_data.specified_seat = seat;
        enter_data.seat_selection_method = VI_SEAT_SELECTION_METHOD_SPECIFIED;
    }
    if (!vehicle_interaction_request_enter(np, vp, &enter_data))
    {
        // 3 s without npcs trying this vehicle.
        vp->no_npc_entry_timer.set(3000);
        return AI_RESULT_FAILURE;
    }
    aip->perception.set_attention_mode(ATTENTION_LOOK_AT_NONE, ATTENTION_STEERING_TURN_TO_TARGET, ATTENTION_AWARENESS_NORMAL);
    ai_vehicle_maybe_adopt_vehicle(np, vp);
    return AI_RESULT_SUCCESS;
}

// 0x0066F390 (:1529)
ai_results ai_vehicle_enter(npc* np, vehicle* vp, ai_vehicle_seat_types seat_type, bool reserve_only)
{
    ai_system* aip = np->get_ai();
    if (human_in_vehicle_transition(np, vp, VI_INTERACTION_ENTER))
    {
        return reserve_only ? AI_RESULT_SUCCESS : AI_RESULT_IN_PROGRESS;
    }
    vi_enter_data enter_data;
    enter_data.flags.walk = np->npc_flags.ambient_spawn;
    enter_data.flags.reserve_seat_only = reserve_only ? 1 : 0;
    if (seat_type == AI_VEHICLE_SEAT_TYPE_DRIVER)
    {
        enter_data.specified_seat = VSI_DRIVER;
        enter_data.seat_selection_method = VI_SEAT_SELECTION_METHOD_SPECIFIED;
    }
    else if (seat_type == AI_VEHICLE_SEAT_TYPE_PASSENGER)
    {
        enter_data.seat_selection_method = VI_SEAT_SELECTION_METHOD_BEST;
    }
    else if (seat_type == AI_VEHICLE_SEAT_TYPE_GUNNER)
    {
        vehicle_seat_index seat;
        if (gunner_seat_available(vp, np, true, &seat) == nullptr)
        {
            return AI_RESULT_FAILURE;
        }
        enter_data.specified_seat = seat;
        enter_data.seat_selection_method = VI_SEAT_SELECTION_METHOD_SPECIFIED;
    }
    const bool requested = vehicle_interaction_request_enter(np, vp, &enter_data);
    if (reserve_only)
    {
        return requested ? AI_RESULT_SUCCESS : AI_RESULT_FAILURE;
    }
    if (!requested)
    {
        // 3 s without npcs trying this vehicle.
        vp->no_npc_entry_timer.set(3000);
        return AI_RESULT_FAILURE;
    }
    aip->perception.set_attention_mode(ATTENTION_LOOK_AT_NONE, ATTENTION_STEERING_TURN_TO_TARGET, ATTENTION_AWARENESS_NORMAL);
    return AI_RESULT_IN_PROGRESS;
}

// 0x006458A0 (:1594)
bool ai_vehicle_should_stop_to_let_passenger_out(const vehicle* vp)
{
    if (vp == nullptr)
    {
        return false;
    }
    const int num_seats = vehicle_get_num_seats(vp);
    for (int seat = VSI_PASSENGER_1; seat < num_seats; seat++)
    {
        const npc* np = World.object_cast<npc>(vehicle_passenger_in_seat(vp, (vehicle_seat_index)seat));
        if (np == nullptr)
        {
            continue;
        }
        const ai_action* act = np->get_ai()->planner.get_current_action();
        if (act == get_out_of_vehicle_action::get_instance() && !ai_vehicle_should_get_out_of_moving_vehicle(np, vp))
        {
            return true;
        }
        if (np->npc_flags.use_corpse_vehicle_exit != 0)
        {
            return true;
        }
    }
    int remaining = vp->vii.num_humans;
    for (int i = 0; 0 < remaining && i < VEHICLE_AI_NUM_INTERACTION_SLOTS; i++)
    {
        const vi_human_manager& hm = vp->vii.humans[i];
        if (hm.handle == 0xFFFFFFFF)
        {
            continue;
        }
        remaining--;
        if (hm.seat != VSI_DRIVER)
        {
            const human* hp = World.object_from_handle<human>(hm.handle);
            if (hp != nullptr && hp->cur_state == HS_VEHICLE_EXITING)
            {
                return true;
            }
        }
    }
    return false;
}

// 0x005F4200 (:1639)
bool ai_vehicle_should_get_out_of_moving_vehicle(const npc* np, const vehicle* vp)
{
    const human* driver = vehicle_passenger_in_seat(vp, VSI_DRIVER);
    if (driver != nullptr && !object_is_player(driver) && 1 < (int)human_teams_get_disposition(np, driver) && vp->m_flags.m_on_fire == 0 &&
        vp->m_flags.m_scripted_bail == 0 && vp->m_flags.m_scripted_abandon == 0)
    {
        return false;
    }
    return true;
}

// 0x005F4260 (:1670)
void ai_vehicle_override_speed(vehicle* vp, float speed)
{
    if (vp->vai.vai_flags.m_jumping == 0)
    {
        if (speed > 0.0f)
        {
            vehicle_ai_set_speed_override(&vp->vai, speed);
        }
        else
        {
            vehicle_ai_clear_speed_override(&vp->vai);
        }
    }
}

// 0x005F42A0 (:1691)
void ai_vehicle_stop(vehicle* vp)
{
    switch (vp->get_sub_type())
    {
    case OT_SUB_VEHICLE_AUTO:
        automobile_set_controls_when_abandoned(vp->get_handle());
        return;
    case OT_SUB_VEHICLE_FLYER:
        flyer_set_controls_when_abandoned(static_cast<flyer*>(vp));
        return;
    default:
        keen::debug::nativePrintArgs("Int3 critical function at %s(%d)\n");
        return;
    }
}

// 0x005F4300 (:1722)
bool ai_vehicle_should_update_travel_to_point(vehicle* vp)
{
    if (vp->vai.route_info.num_route_segments == 0)
    {
        return true;
    }
    return vp->vai.navmesh_path_update.valid() && vp->vai.navmesh_path_update.elapsed() && vp->vai.vai_flags.m_jumping == 0 && !vp->vai.swerve_timer.valid();
}

// 0x00645980 (:1738): flies at the terrain height under the vehicle, by way of a road when it can; otherwise straight, a point every
// 40 m when the rest of the way is over 75 m.
static ai_results ai_flying_vehicle_pathfind_to(vehicle* vp, const vector& dest_point, const pf_calc_flags& pf_flags, bool use_roads)
{
    static const int MAX_FLYER_POINTS = 8;
    static const float FLYER_POINT_SPACING = 40.0f;
    static const uint64_t NO_HINT = 0xFFFFFFFFFFFFFFFFull;
    vehicle_ai* vai = &vp->vai;
    vehicle_path_info path_info;
    vector start = vp->pos;
    havok_terrain_height(start.y, start.x, start.z, true, nullptr);
    navmesh_lane_spline_node* path = nullptr;
    if (use_roads)
    {
        pf_calc_point road_points[2] = {pf_calc_point(start, NO_HINT, 0xFFFFFFFF), pf_calc_point(dest_point, NO_HINT, 0xFFFFFFFF)};
        pf_calc_params real_params(2, road_points, pf_flags, use_roads, vai->vai_flags.use_special_roads, true, false, 0xFFFFFFFF);
        bool try_normal_pathfinding;
        path = rfg_pf_vehicle_use_road_splines(real_params, path_info, try_normal_pathfinding, vp);
    }
    if (path == nullptr)
    {
        vector turning[MAX_FLYER_POINTS];
        int num_turning = 0;
        if (pf_flags.use_smoothing != 0)
        {
            num_turning = rfg_pf_calculate_turning_info_for_flyers(vp, dest_point, MAX_FLYER_POINTS, turning);
        }
        const pf_calc_point zero_point(ZERO_VECTOR, NO_HINT, 0xFFFFFFFF);
        pf_calc_point points[MAX_FLYER_POINTS + 2] = {zero_point, zero_point, zero_point, zero_point, zero_point, zero_point, zero_point, zero_point, zero_point, zero_point};
        points[0] = pf_calc_point(start, NO_HINT, 0xFFFFFFFF);
        int count = 0;
        for (; count < num_turning; count++)
        {
            points[count + 1] = pf_calc_point(turning[count], NO_HINT, 0xFFFFFFFF);
        }
        vector last = start;
        if (0 < num_turning)
        {
            last = turning[num_turning - 1];
        }
        vector to_dest = dest_point - last;
        const float dist = to_dest.flatten_mag_and_normalize_safe();
        int num_straight = 0;
        if (75.0f < dist)
        {
            num_straight = (int)((dist - 10.0f) / FLYER_POINT_SPACING);
            if (MAX_FLYER_POINTS - num_turning < num_straight)
            {
                num_straight = MAX_FLYER_POINTS - num_turning;
            }
            for (int i = 1; i <= num_straight; i++)
            {
                vector point;
                point.x = to_dest.x * (float)i * FLYER_POINT_SPACING + last.x;
                point.y = start.y;
                point.z = to_dest.z * (float)i * FLYER_POINT_SPACING + last.z;
                points[++count] = pf_calc_point(point, NO_HINT, 0xFFFFFFFF);
            }
        }
        vector end = dest_point;
        end.y = start.y;
        points[count + 1] = pf_calc_point(end, NO_HINT, 0xFFFFFFFF);
        path = vehicle_generate_direct_path_between_points(num_straight + 2 + num_turning, points, path_info, vp);
    }
    vehicle_ai_free_all_paths(vai);
    vehicle_navmesh_common_init_path_follow(vai, path);
    vai->current_path_info = path_info;
    vai->navmesh_path_update.set_rand(500, 1000);
    vai->vai_flags.m_goal_stop_override = 0;
    return AI_RESULT_SUCCESS;
}

// 0x00645F00 (:1850)
static ai_results ai_vehicle_pathfind_to(vehicle* vp, const vector& dest_point, const uint64_t dest_cell_hint_handle, const pf_calc_flags& pf_flags, bool use_roads, bool flying)
{
    vehicle_ai* vai = &vp->vai;
    if (flying)
    {
        return ai_flying_vehicle_pathfind_to(vp, dest_point, pf_flags, use_roads);
    }
    if (!should_ignore_path_failure(vp))
    {
        vehicle_pathfind_results reason;
        if (vehicle_ai_has_cached_pf_failure(vai, vp->pos, dest_point, reason))
        {
            if (reason == VPF_RESULT_FAILURE_BAD_START)
            {
                vai->vai_flags.fallen_off_navmesh = 1;
            }
            return AI_RESULT_FAILURE;
        }
    }
    vehicle_navmesh_path_add_request(vp);
    if (!vehicle_navmesh_path_can_pathfind(vp->get_handle()))
    {
        return AI_RESULT_IN_PROGRESS;
    }
    pf_calc_point points[2] = {pf_calc_point(vp->pos, vp->m_navcell_info.last_visited_nav_cell_handle, 0xFFFFFFFF), pf_calc_point(dest_point, dest_cell_hint_handle, 0xFFFFFFFF)};
    pf_calc_params params(2, points, pf_flags, use_roads, vai->vai_flags.use_special_roads, false, false, 0xFFFFFFFF);
    if (vai->vai_flags.m_ignore_road_directionality != 0)
    {
        params.ignore_road_directionality = true;
    }
    vehicle_path_info path_info;
    navmesh_lane_spline_node* path = vehicle_navmesh_common_create_path(vai, params, path_info);
    if (path == nullptr)
    {
        vehicle_pathfind_results reason = VPF_RESULT_FAILURE_NO_PATH;
        if (path_info.error_code == PR_ERROR_START_NAVCELL)
        {
            vai->vai_flags.fallen_off_navmesh = 1;
            reason = VPF_RESULT_FAILURE_BAD_START;
        }
        else if (path_info.error_code == PR_ERROR_END_NAVCELL)
        {
            reason = VPF_RESULT_FAILURE_BAD_DESTINATION;
        }
        if (!should_ignore_path_failure(vp))
        {
            vehicle_ai_record_pf_failure(vai, vp->pos, dest_point, reason);
        }
        return AI_RESULT_FAILURE;
    }
    vehicle_ai_free_all_paths(vai);
    vehicle_navmesh_common_init_path_follow(vai, path);
    vai->vai_flags.m_jumping = 0;
    vai->current_path_info = path_info;
    if (path_info.flags.jumping != 0)
    {
        vai->vai_flags.m_jumping = 1;
    }
    vai->navmesh_path_update.set_rand(500, 1000);
    vai->vai_flags.m_goal_stop_override = 0;
    return AI_RESULT_SUCCESS;
}

// 0x00646140 (:1982)
static ai_results ai_vehicle_update_path(vehicle* vp, const vector& dest_point, const uint64_t dest_cell_hint_handle, const pf_calc_flags& pf_flags, bool use_roads, bool flying)
{
    vehicle_ai* vai = &vp->vai;
    if (flying)
    {
        return ai_flying_vehicle_pathfind_to(vp, dest_point, pf_flags, use_roads);
    }
    vehicle_navmesh_path_add_request(vp);
    if (!vehicle_navmesh_path_can_pathfind(vp->get_handle()))
    {
        return AI_RESULT_IN_PROGRESS;
    }
    pf_calc_point points[2] = {pf_calc_point(vp->pos, vp->m_navcell_info.last_visited_nav_cell_handle, 0xFFFFFFFF), pf_calc_point(dest_point, dest_cell_hint_handle, 0xFFFFFFFF)};
    pf_calc_params params(2, points, pf_flags, use_roads, vai->vai_flags.use_special_roads, false, false, 0xFFFFFFFF);
    params.ignore_road_directionality = vai->vai_flags.m_ignore_road_directionality != 0;
    vai->navmesh_path_update.set(1000);
    vehicle_path_info path_info;
    if (!vehicle_navmesh_common_update_path(vai, params, path_info))
    {
        if (path_info.error_code == PR_ERROR_START_NAVCELL)
        {
            vai->vai_flags.fallen_off_navmesh = 1;
        }
        return AI_RESULT_FAILURE;
    }
    vai->vai_flags.m_goal_stop_override = 0;
    vai->current_path_info = path_info;
    return AI_RESULT_SUCCESS;
}

// 0x0068B1F0 (:2045)
ai_results ai_vehicle_travel_to_a_point_do_frame(npc* np, vehicle* vp, const vector& dest_point, const uint64_t dest_navcell_hint_handle, bool use_roads, float dist, bool dont_stop, bool use_light, bool use_disabled, bool use_smoothing)
{
    vehicle_ai* vai = &vp->vai;
    if (!vp->disable_for_npc_drivers_timer.elapsed())
    {
        if (can_drive_after_bad_pathfind(np, vp))
        {
            if (vp->get_sub_type() == OT_SUB_VEHICLE_FLYER)
            {
                return AI_RESULT_IN_PROGRESS;
            }
        }
        else
        {
            keen::debug::nativePrintArgs("Int3 critical function at %s(%d)\n");
            vp->disable_for_npc_drivers_timer.set(0);
            vp->pf_failure_timeout.set(0);
        }
    }
    vector dest = dest_point;
    if (vai->vai_flags.m_jumping != 0)
    {
        dest = vai->current_path_info.end_pos;
    }
    const bool flying = vp->get_sub_type() == OT_SUB_VEHICLE_FLYER;
    pf_calc_flags flags(true);
    if ((use_disabled || np->npc_flags.drive_disabled != 0) && ai_should_smash_buildings(np->get_ai()))
    {
        flags.use_disabled_cells = 1;
    }
    else if (use_light)
    {
        flags.use_light_structure_disabled_cells = 1;
    }
    flags.use_smoothing = use_smoothing ? 1 : 0;
    vai->vai_flags.use_special_roads = np->npc_flags.can_use_special_roads != 0 ? 1 : 0;
    vector to_dest = dest - vp->pos;
    const float dist_to_dest = to_dest.flatten_mag_and_normalize_safe();
    // Upside down (compared as a double in rfg.exe).
    if (!flying && (double)vp->orient.uvec.y < 0.20000000298023224)
    {
        ai_vehicle_stop(vp);
        return AI_RESULT_IN_PROGRESS;
    }
    if (dist_to_dest <= dist)
    {
        vehicle_ai_free_all_paths(vai);
        vehicle_ai_flush_route(vai->route_info);
        ai_vehicle_stop(vp);
        vai->vai_flags.m_jumping = 0;
        return AI_RESULT_SUCCESS;
    }
    bool path_is_new = false;
    if (vai->route_info.num_route_segments == 0)
    {
        const ai_results result = ai_vehicle_pathfind_to(vp, dest, dest_navcell_hint_handle, flags, use_roads, flying);
        if (result == AI_RESULT_FAILURE)
        {
            if (!vp->pf_failure_timeout.valid())
            {
                vp->pf_failure_timeout.set(3000);
                return AI_RESULT_IN_PROGRESS;
            }
            if (!vp->pf_failure_timeout.elapsed())
            {
                return AI_RESULT_IN_PROGRESS;
            }
            // Long since the last failure: a new 3 s.
            if (7000 < vp->pf_failure_timeout.time_since())
            {
                vp->pf_failure_timeout.set(3000);
                return AI_RESULT_IN_PROGRESS;
            }
            vp->disable_for_npc_drivers_timer.set(flying ? 5000 : 20000);
            return AI_RESULT_FAILURE;
        }
        if (result == AI_RESULT_SUCCESS)
        {
            vp->pf_failure_timeout.invalidate();
            path_is_new = true;
            vai->update_failures = 0;
        }
    }
    if (vai->route_info.num_route_segments <= 0)
    {
        return AI_RESULT_IN_PROGRESS;
    }
    // A first spline segment of no length: the route is no use.
    const vector* knots = vai->route_info.route[0].cb_spline.knots;
    if (vec_dist_squared(knots[0], knots[1]) < 1e-10f)
    {
        vehicle_ai_free_all_paths(vai);
        vehicle_ai_flush_route(vai->route_info);
        vai->vai_flags.m_jumping = 0;
        return AI_RESULT_IN_PROGRESS;
    }
    if (!path_is_new && vai->current_path_info.flags.off_road != 0)
    {
        // Back on the road the path left: find a new path now. 2.5 m beyond the road's width.
        const object_path_road* road = World.object_from_handle<object_path_road>(vai->current_path_info.closest_road_spline);
        if (road != nullptr)
        {
            vector closest = ZERO_VECTOR;
            const float u = road->find_closest_point_on_road(vp->pos, closest, false);
            const float width = road->get_width(u);
            if (vec_xz_dist(closest, vp->pos) < width + 2.5f)
            {
                vai->navmesh_path_update.set(0);
            }
        }
    }
    if (vai->navmesh_path_update.valid() && vai->navmesh_path_update.elapsed() && vai->vai_flags.m_jumping == 0 && !vai->swerve_timer.valid())
    {
        if (ai_vehicle_update_path(vp, dest, dest_navcell_hint_handle, flags, use_roads, flying) == AI_RESULT_FAILURE)
        {
            vai->update_failures++;
            if (1 < vai->update_failures)
            {
                vai->update_failures = 0;
                vp->disable_for_npc_drivers_timer.set(flying ? 5000 : 20000);
                vehicle_ai_free_all_paths(vai);
                vehicle_ai_flush_route(vai->route_info);
                ai_vehicle_stop(vp);
                vai->vai_flags.m_jumping = 0;
                return AI_RESULT_FAILURE;
            }
        }
        else
        {
            vai->update_failures = 0;
        }
    }
    if (!dont_stop)
    {
        // Braking distance, at least 3 m (15 m flying). Under 3 m brake fully; otherwise slow to the speed the distance allows, at least 1 m/s.
        float speed;
        vehicle_get_speed(vp, speed);
        const float max_braking = vai->active_control_info.max_braking_decel;
        const float braking_dist = speed * speed / (max_braking * 2.0f);
        const float min_dist = flying ? 15.0f : 3.0f;
        const float stop_dist = braking_dist - min_dist < 0.0f ? min_dist : braking_dist;
        vai->vai_flags.m_goal_stop_override = dist_to_dest <= stop_dist ? 1 : 0;
        if (vai->vai_flags.m_goal_stop_override != 0)
        {
            if (3.0f <= dist_to_dest)
            {
                vai->target_decel_to_goal = speed * speed / (dist_to_dest * 2.0f);
                const float target_speed = sqrtf(max_braking * dist_to_dest);
                vai->target_speed_to_goal = target_speed - 1.0f < 0.0f ? 1.0f : target_speed;
            }
            else
            {
                vai->target_speed_to_goal = 0.0f;
                vai->target_decel_to_goal = max_braking;
            }
        }
    }
    if (vehicle_ai_process_stuck(vp, true))
    {
        return AI_RESULT_IN_PROGRESS;
    }
    vehicle_navmesh_common_follow_path(vp);
    return AI_RESULT_IN_PROGRESS;
}

// 0x0066F4D0 (:2276): searches the roads from the vehicle for a point over 150 m from flee_pos and over 50 m from the vehicle.
bool ai_vehicle_pick_flee_destination_road(const vehicle* vp, object_path_road* prp, vector& destination_point, const vector& flee_pos)
{
    static const float MIN_FLEE_DISTANCE_SQRD = 22500.0f; // 0x01DE4428, set on the first call in rfg.exe
    static const float MIN_DIST_FROM_START_SQRD = 2500.0f; // 0x01DE4424, set on the first call in rfg.exe
    vector on_road = ZERO_VECTOR;
    const float start_u = prp->find_closest_point_on_road(vp->pos, on_road, false);
    Road_heap.start_session();
    const unsigned short start_high = (unsigned short)(int)ceil((double)start_u);
    const unsigned short start_low = (unsigned short)(int)floor((double)start_u);
    const road_spline_point* start_points = prp->spline_data.points.to_pointer();
    // Original quirk: returning here leaves the road heap session open.
    if (!object_path_road_query_for_path(prp, start_high, nullptr, start_high, vec_dist(vp->pos, start_points[start_high].pos), flee_pos, &Road_heap, true, false) ||
        !object_path_road_query_for_path(prp, start_low, nullptr, start_low, vec_dist(vp->pos, start_points[start_low].pos), flee_pos, &Road_heap, true, false))
    {
        return false;
    }
    object_path_road* found_road = nullptr;
    unsigned short found_point = 0;
    bool found_going_back = false;
    while (0 < Road_heap.num_road_splines)
    {
        road_spline_identifier id = Road_heap.remove_best_road_spline();
        object_path_road* road = id.road_spline;
        if (road == nullptr || road->road_flags.gps_only != 0)
        {
            continue;
        }
        const unsigned short point = id.point;
        const road_heap_data* data = Road_heap.get_road_spline_data(&id);
        const road_spline_point* road_points = road->spline_data.points.to_pointer();
        if (MIN_FLEE_DISTANCE_SQRD < vec_dist_squared(road_points[point].pos, flee_pos) && MIN_DIST_FROM_START_SQRD < vec_dist_squared(road_points[point].pos, vp->pos))
        {
            found_road = road;
            found_point = point;
            found_going_back = data->arrival_point < point;
            break;
        }
        bool block_next = false;
        bool block_prev = false;
        if (road->has_breaks())
        {
            const road_broken_state here = road->is_broken_at_index(point);
            const road_broken_state before = road->is_broken_at_index(point - 1);
            block_next = here == ROAD_BROKEN_COMPLETELY || here == ROAD_BROKEN_RIGHT;
            block_prev = before == ROAD_BROKEN_COMPLETELY || before == ROAD_BROKEN_LEFT;
        }
        if (road->has_road_blocks())
        {
            if (road->is_road_blocked_at_index(point))
            {
                block_next = true;
            }
            if (road->is_road_blocked_at_index(point - 1))
            {
                block_prev = true;
            }
        }
        const unsigned short next_point = (unsigned short)(point + 1);
        if (!block_next && next_point < road->spline_data.num_points)
        {
            const float cost = vec_dist(road_points[point].pos, road_points[next_point].pos) + data->arrival_cost;
            object_path_road_query_for_path(road, next_point, road, point, cost, flee_pos, &Road_heap, true, false);
        }
        if (!block_prev && point != 0 && road->road_flags.one_way == 0)
        {
            const unsigned short prev_point = (unsigned short)(point - 1);
            const float cost = vec_dist(road_points[point].pos, road_points[prev_point].pos) + data->arrival_cost;
            object_path_road_query_for_path(road, prev_point, road, point, cost, flee_pos, &Road_heap, true, false);
        }
        const road_spline_connection* connections = road->spline_data.connections.to_pointer();
        for (int i = 0; i < road->spline_data.num_connections; i++)
        {
            const road_spline_connection& connection = connections[i];
            if (connection.from_pt != point || connection.handle == 0xFFFFFFFF)
            {
                continue;
            }
            object_path_road* other = static_cast<object_path_road*>(World.object_from_handle(connection.handle));
            if (other != nullptr && other->get_obj_type() == OT_PATH_ROAD && (int)connection.to_pt < other->spline_data.num_points)
            {
                object_path_road_query_for_path(other, connection.to_pt, road, point, data->arrival_cost, flee_pos, &Road_heap, true, false);
            }
        }
    }
    Road_heap.end_session();
    if (found_road == nullptr)
    {
        return false;
    }
    // Within a quarter of a segment of the point, on the side the search reached it from.
    const float u = (float)found_point;
    found_road->get_point(destination_point, found_going_back ? fl_rand_range(u - 0.25f, u) : fl_rand_range(u, u + 0.25f));
    return true;
}

// 0x006462D0 (:2549)
bool ai_vehicle_maybe_adopt_vehicle(npc* np, vehicle* vp)
{
    object_squad* squad = nullptr;
    const unsigned int squad_handle = np->get_ai()->get_squad_handle();
    if (squad_handle != 0xFFFFFFFF)
    {
        object* objp = World.object_from_handle(squad_handle);
        if (objp != nullptr && objp->get_obj_type() == OT_SQUAD)
        {
            squad = static_cast<object_squad*>(objp);
        }
    }
    if (np->npc_flags.activity_related != 0 || np->npc_flags.mission_related != 0)
    {
        return false;
    }
    if (object_is_player(World.object_from_handle(vp->most_recent_driver)))
    {
        return false;
    }
    // Not one still parked within 5 m of its own spawn node.
    if (vp->spawn_node_handle != 0xFFFFFFFF)
    {
        const object* node = World.object_from_handle(vp->spawn_node_handle);
        if (node != nullptr && node->get_obj_type() == OT_VEHICLE_SPAWN_NODE && vec_dist_squared(vp->pos, node->pos) < 25.0f)
        {
            return false;
        }
    }
    bool adopted = false;
    if (vp->squad_handle == 0xFFFFFFFF && squad != nullptr && squad->squad_vehicle == 0xFFFFFFFF)
    {
        vp->squad_handle = squad->get_handle();
        squad->squad_vehicle = vp->get_handle();
        for (int i = 0; i < squad->member_array.size(); i++)
        {
            npc* member = World.object_from_handle<npc>(squad->member_array[i]);
            if (member != nullptr)
            {
                member->npc_flags.despawn_on_green_alert = 1;
                adopted = true;
            }
        }
    }
    return adopted;
}

// 0x00673E20 (.h:82)
bool ai_vehicle_reserve(npc* np, vehicle* vp, ai_vehicle_seat_types seat_type)
{
    return ai_vehicle_enter(np, vp, seat_type, true) == AI_RESULT_SUCCESS;
}
