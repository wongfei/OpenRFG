// Original: volition/rfg/code/havok/havok_collision_immediate_callbacks.cpp
//
// The contact point callbacks run inside the Havok step (accept or reject the contact), registered in the immediate
// unary table of havok_collision_events.cpp (shared through the unity build): an item's callback goes to the
// projectile or weapon one. A projectile passes through the vehicle its owner sits in, through movers it shatters,
// and sets off attached explosives it touches; a "detonate on impact" projectile is set to go off (its detonate
// stamp, processed by projectile::process) on any contact it keeps. A weapon shatters rfg movers and passes through
// general movers with material flag 1. A vehicle dislodges fixed general movers and passes through a big walker's
// collision model (havok_collide_vehicle_immediate).
// Compiled in rfg_havok_unity.cpp.

#include "volition/rfg/code/havok/havok_collision_immediate_callbacks.h"
#include "volition/rfg/code/havok/havok_collision_events.h"

#include "volition/rfg/code/destruction/rfg_dynamics.h"
#include "volition/rfg/code/destruction/rfg_materials.h"
#include "volition/rfg/code/destruction/rfg_mover.h"
#include "volition/rfg/code/level/world.h"
#include "volition/rfg/code/multi/multi_core/include/multi.h"
#include "volition/rfg/code/multi/multi_core/include/multi_core_compression.h"
#include "volition/rfg/code/objects/humans/human.h"
#include "volition/rfg/code/objects/movers/general_mover.h"
#include "volition/rfg/code/objects/projectile.h"
#include "volition/rfg/code/geom/vanim.h"
#include "volition/rfg/code/havok/havok.h"
#include "volition/rfg/code/havok/havok_collision_callbacks.h"
#include "volition/rfg/code/objects/level_object_properties.h"
#include "volition/rfg/code/vehicle/vehicle_class.h"
#include "volition/rfg/code/vehicle/walker.h"
#include "volition/rfg/code/weapons/weapons.h"

// 0x0081B670 (:32)
static contact_point_action projectile_collide_immediate(const collision_event* c_event)
{
    object* objp = c_event->m_object_a;
    projectile* proj = objp != nullptr && objp->get_obj_type() == OT_ITEM && objp->get_sub_type() == OT_SUB_ITEM_PROJECTILE ? (projectile*)objp : nullptr;
    object* other = c_event->m_object_b;
    const unsigned int shape_key = c_event->m_collisions[0].m_shapekey_b;
    if (other != nullptr)
    {
        if (other->get_obj_type() == OT_ITEM && other->get_sub_type() == OT_SUB_ITEM_PROJECTILE)
        {
            // Projectiles never touch each other, but an attached explosive hit by a projectile that is not thrown
            // sets both off, each credited to the other's owner.
            projectile* other_proj = (projectile*)other;
            if (proj->is_attached() && proj->w_info->is_explosive() && other_proj->w_info->weapon_class != WEAPON_CLASS_THROWN)
            {
                const unsigned int other_owner = other_proj->get_owner_handle();
                proj->detonate_stamp.set(0);
                proj->detonator_handle = other_owner;
                const unsigned int owner = proj->get_owner_handle();
                other_proj->detonate_stamp.set(0);
                other_proj->detonator_handle = owner;
                return CONTACT_POINT_REJECT;
            }
            if (other_proj->is_attached() && other_proj->w_info->is_explosive() && proj->w_info->weapon_class != WEAPON_CLASS_THROWN)
            {
                const unsigned int owner = proj->get_owner_handle();
                other_proj->detonate_stamp.set(0);
                other_proj->detonator_handle = owner;
                const unsigned int other_owner = other_proj->get_owner_handle();
                proj->detonate_stamp.set(0);
                proj->detonator_handle = other_owner;
            }
            return CONTACT_POINT_REJECT;
        }
        if (other->get_obj_type() == OT_VEHICLE && proj->get_owner_handle() != 0xFFFFFFFF)
        {
            object* owner = World.object_from_handle(proj->get_owner_handle());
            if (owner != nullptr && owner->get_obj_type() == OT_HUMAN && ((vehicle*)other)->in_vehicle((human*)owner))
            {
                return CONTACT_POINT_REJECT;
            }
        }
    }
    if (proj->obj_flags.destroyed && !proj->is_attached())
    {
        return CONTACT_POINT_REJECT;
    }
    if (other != nullptr && other->get_obj_type() == OT_MOVER)
    {
        bool gmover_shatter;
        bool rmover_shatter;
        if (!projectile_object_mover_impact_stops_projectile(proj, (object_mover*)other, shape_key, gmover_shatter, rmover_shatter))
        {
            if (rmover_shatter)
            {
                // The rfg mover shatters where the projectile goes in (the contact against its normal).
                const vector world_dir(-c_event->m_collisions[0].m_collision_normal.x, -c_event->m_collisions[0].m_collision_normal.y, -c_event->m_collisions[0].m_collision_normal.z);
                vector local_pos;
                vector local_dir;
                convert_point_direction_from_world_to_local(local_pos, local_dir, c_event->m_collisions[0].m_collision_point, world_dir, other->pos, other->orient);
                rfg_dyn_add_pre_collide_shatter((rfg_mover*)other, World.object_from_handle(proj->get_owner_handle()), shape_key, 0, local_pos, local_dir, false);
                return CONTACT_POINT_REJECT;
            }
            if (gmover_shatter && (!Multiplayer || multi_is_server() == true))
            {
                // gm_flags 0x400: the general mover is deleted (shattered).
                ((general_mover*)other)->gm_flags |= 0x400;
            }
            return CONTACT_POINT_REJECT;
        }
    }
    // Projectile flag 4 (detonate on impact): it goes off on the next process.
    if ((proj->w_info->projectile_info.flags & 4) != 0)
    {
        proj->detonate_stamp.set(0);
        proj->detonator_handle = 0xFFFFFFFF;
    }
    return CONTACT_POINT_ACCEPT;
}

// 0x0081B8F0 (:104)
static contact_point_action weapon_collide_immediate(const collision_event* c_event)
{
    object* objp = c_event->m_object_a;
    weapon* wp = objp != nullptr && objp->get_obj_type() == OT_ITEM && objp->get_sub_type() == OT_SUB_ITEM_WEAPON ? (weapon*)objp : nullptr;
    object* other = c_event->m_object_b;
    if (other != nullptr && other->get_obj_type() != OT_MOVER)
    {
        other = nullptr;
    }
    const unsigned int shape_key = c_event->m_collisions[0].m_shapekey_b;
    if (wp == nullptr || other == nullptr)
    {
        return CONTACT_POINT_ACCEPT;
    }
    if (other->get_sub_type() == OT_SUB_MOVER_GENERAL)
    {
        // Physical material flag 1: weapons pass through (the contact is rejected but its callbacks run).
        const rfg_physical_material* material = rfg_get_physical_material_by_index(other->get_physical_material(0xFFFFFFFF, nullptr, nullptr));
        return (contact_point_action)((material->flags & 1) * 2);
    }
    if (other->get_sub_type() == OT_SUB_MOVER_RFG && ((rfg_mover*)other)->is_shatterable(shape_key))
    {
        const vector world_dir(-c_event->m_collisions[0].m_collision_normal.x, -c_event->m_collisions[0].m_collision_normal.y, -c_event->m_collisions[0].m_collision_normal.z);
        vector local_pos;
        vector local_dir;
        convert_point_direction_from_world_to_local(local_pos, local_dir, c_event->m_collisions[0].m_collision_point, world_dir, other->pos, other->orient);
        rfg_dyn_add_pre_collide_shatter((rfg_mover*)other, World.object_from_handle(wp->get_owner_handle()), shape_key, 0, local_pos, local_dir, false);
        return CONTACT_POINT_REJECT;
    }
    return CONTACT_POINT_ACCEPT;
}

// 0x00824780 (:154)
// A vehicle's (object_a) contact. A fixed general mover the vehicle can dislodge ("dislodged by cars", with dislodge hit
// points) lets an impact of at least those hit points through, so the callbacks dislodge it. A big walker stomping a
// vehicle of 30 t or less (not of infinite mass) during the first 15 frames of its stomp action sets the vehicle's walker
// velocity stamp (60 ms). Contacts that are all with the walker's collision model body are rejected (the vehicle passes
// through it) and other contacts kept; for a vehicle corpse with more than 28 s left on its corpse timer it is the
// other way round.
static contact_point_action havok_collide_vehicle_immediate(const collision_event* c_event)
{
    object* other = c_event->m_object_b;
    if (other != nullptr && other->get_sub_type() == OT_SUB_MOVER_GENERAL)
    {
        object* objp_a = c_event->m_object_a;
        const vehicle* vp = objp_a != nullptr && objp_a->get_obj_type() == OT_VEHICLE ? (const vehicle*)objp_a : nullptr;
        object_mover* mover = other->get_obj_type() == OT_MOVER && other->get_sub_type() == OT_SUB_MOVER_GENERAL ? (object_mover*)other : nullptr;
        if (vp != nullptr && mover != nullptr && !havok_body_is_movable(mover->havok_handle) && mover->m_level_obj_properties->m_dislodge_hitpoints > 0)
        {
            vector position;
            vector normal;
            vector velocity;
            unsigned int best_collision;
            const int damage = (int)havok_collision_calculate_damage(c_event, position, normal, velocity, best_collision);
            const level_object_inst_properties* props = mover->m_level_obj_properties;
            if (props->m_dislodge_hitpoints <= damage && props->m_flags.m_dislodged_by_cars)
            {
                return CONTACT_POINT_REJECT_BUT_PROCESS_CALLBACKS;
            }
        }
        return CONTACT_POINT_ACCEPT;
    }
    if (other == nullptr || other->get_sub_type() != OT_SUB_VEHICLE_WALKER)
    {
        return CONTACT_POINT_ACCEPT;
    }
    object* objp_a = c_event->m_object_a;
    vehicle* vp = objp_a != nullptr && objp_a->get_obj_type() == OT_VEHICLE ? (vehicle*)objp_a : nullptr;
    walker* wp = other->get_obj_type() == OT_VEHICLE && other->get_sub_type() == OT_SUB_VEHICLE_WALKER ? (walker*)other : nullptr;
    // Original: the vehicle and the walker are read without a null check.
    if ((double)havok_body_get_mass(vp->havok_handle) > 30000.0) // Magic number: 30 t
    {
        return CONTACT_POINT_ACCEPT;
    }
    if (vp->get_infinite_mass() || wp->model_type != 2) // Magic number: model type 2, the big walker
    {
        return CONTACT_POINT_ACCEPT;
    }
    // Magic numbers: action aliases 0x3DC (stomp) and 0x216, frames under 15, a 60 ms stamp.
    if (vanim_is_playing_action(wp->vp, 0x3DC) && (wp->model_type == 0 || vanim_is_playing_action(wp->vp, 0x216)) && vanim_get_anim_frame(wp->vp, 0x3DC) < 15)
    {
        vp->m_walker_velocity_hack.set(60);
    }
    const hkpRigidBody* collision_model = wp->collision_model != 0xFFFFFFFF ? Hk_rigid_bodies[wp->collision_model] : nullptr;
    bool all_on_model = true;
    const unsigned int num_collisions = c_event->m_num_collisions;
    if (num_collisions >= 1 && num_collisions <= 4)
    {
        for (unsigned int i = num_collisions; i-- > 0;)
        {
            if (c_event->m_collisions[i].m_hk_rigid_body_b != collision_model)
            {
                all_on_model = false;
                break;
            }
        }
    }
    // Magic number: 28000 ms of the corpse timer left.
    const bool fresh_corpse = vp->m_flags.m_corpse && 28000 < vp->m_corpse_timer.time_until();
    if (all_on_model)
    {
        return fresh_corpse ? CONTACT_POINT_ACCEPT : CONTACT_POINT_REJECT;
    }
    return fresh_corpse ? CONTACT_POINT_REJECT : CONTACT_POINT_ACCEPT;
}

// 0x0081BA20 (:238)
static contact_point_action havok_collide_item_immediate(const collision_event* c_event)
{
    const object_sub_type sub_type = c_event->m_object_a->get_sub_type();
    if (sub_type == OT_SUB_ITEM_WEAPON)
    {
        return weapon_collide_immediate(c_event);
    }
    if (sub_type != OT_SUB_ITEM_PROJECTILE)
    {
        return CONTACT_POINT_ACCEPT;
    }
    return projectile_collide_immediate(c_event);
}

// 0x008249A0 (:262), inlined in havok_collision_callbacks_system_init.
void havok_collision_immediate_callbacks_system_init()
{
    Collision_event_immediate_unary_callbacks[OT_ITEM].m_callback = havok_collide_item_immediate;
    Collision_event_immediate_unary_callbacks[OT_VEHICLE].m_callback = havok_collide_vehicle_immediate;
}

// 0x00810C80 (:268)
void havok_collision_immediate_callbacks_system_shutdown()
{
}
