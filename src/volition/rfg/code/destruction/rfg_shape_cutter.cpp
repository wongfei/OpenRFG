// Original: volition/rfg/code/destruction/rfg_shape_cutter.cpp
//
// shape_cutter (decompiled; header comment in rfg_shape_cutter.h). Compiled in rfg_destruction_unity.cpp.
//
// A shape cutter is created by an explosion (or with a plain radius). After the explosion's delay it applies the
// explosion to the objects around it once (apply_explosion: damage and impulses, shatter events for rfg_movers), then
// cuts the movers in its sphere every frame while the explosion lasts (expanding_explosion_duration), the radius
// growing with the elapsed time: rfg_movers get shape cutter events, general movers 50000 explosion damage.
// Explosions breaking over time (time_between_breaks, the nano rifle) only break links (apply_link_break) every
// time_between_breaks, with a tenth of the impulse and the damage spread over the expansions. One cutter cuts per
// frame (shape_cutter_do_frame goes round them), all of them while loading.
//
// Partial: unused_dcf_shape_cutter_test (a console command) is not recovered.

#include "volition/rfg/code/destruction/rfg_shape_cutter.h"

#include "volition/rfg/code/code_effects/explosion.h"
#include "volition/rfg/code/code_effects/explosion_shared.h"
#include "volition/rfg/code/destruction/rfg_dynamics.h"
#include "volition/rfg/code/destruction/rfg_mover.h"
#include "volition/rfg/code/gameplay/activities/activity_demolitions_master_internal.h"
#include "volition/rfg/code/gamewide/gameseq.h"
#include "volition/rfg/code/havok/havok.h"
#include "volition/rfg/code/level/world.h"
#include "volition/rfg/code/level/world.inl"
#include "volition/rfg/code/objects/humans/human.h"
#include "volition/rfg/code/objects/humans/human_combat.h"
#include "volition/rfg/code/objects/humans/player.h"
#include "volition/rfg/code/objects/movers/object_mover.h"
#include "volition/rfg/code/objects/object_debris.h"
#include "volition/rfg/code/objects/object_item.h"
#include "volition/rfg/code/objects/object_prop_names_shared.h"
#include "volition/rfg/code/objects/projectile.h"
#include "volition/rfg/code/objects/turret.h"
#include "volition/rfg/code/scripting/action_script/global_event_data.h"
#include "volition/rfg/code/vehicle/vehicle_class.h"
#include "volition/rfg/code/weapons/weapons.h"
#include "volition/vlib/code/memory/object_pool.h"

static const int SHAPE_CUTTER_POOL_SIZE = 103;
static const int SHAPE_CUTTER_POOL_USAGE = 0x4F90; // memory footprint registered with the instantiator
static const int MAX_CUT_OBJECTS = 100;
static const float GENERAL_MOVER_CUT_DAMAGE = 50000.0f;
static const float LINK_BREAK_IMPULSE_SCALE = 0.1f;

// ---- file-static globals
static int Current_cutter_index; // 0x01EF82E4, where shape_cutter_do_frame goes on
static object_pool<shape_cutter, SHAPE_CUTTER_POOL_SIZE, 1> shape_cutterpool; // 0x01F541F0
static obj_class_instantiator shape_cutter_instantiator("shape_cutter", alloc_shape_cutter, types_shape_cutter, bpo_index_shape_cutter, classname_shape_cutter, shape_cutter::pool_remaining, SHAPE_CUTTER_POOL_USAGE, SHAPE_CUTTER_POOL_SIZE); // 0x01F133A4 (0x01069E60)

// 0x00717E60
// Everything but humans, the cutter and its source.
static bool expanding_explosion_callback(object* hit_objp, unsigned int shape_key, const void* user_data)
{
    if (hit_objp == nullptr || hit_objp->get_obj_type() == OT_HUMAN)
    {
        return false;
    }
    return hit_objp != user_data && hit_objp->get_handle() != ((const shape_cutter*)user_data)->source_handle;
}

// ---- class registration (:131)
// 0x00717EA0
void types_shape_cutter(signed char& type, signed char& subtype)
{
    type = OT_SHAPE_CUTTER;
    subtype = OT_SUB_UNDEFINED;
}

// 0x00717EB0
int bpo_index_shape_cutter()
{
    return 1;
}

// 0x007302B0
void shape_cutter::pool_free()
{
    shape_cutterpool.free(this);
}

// 0x00720570
int shape_cutter::pool_remaining()
{
    return shape_cutterpool.get_free();
}

// 0x00717ED0
unsigned int shape_cutter::get_classname_hash()
{
    return shape_cutter_instantiator.this_hash;
}

// 0x00717EC0
const char* classname_shape_cutter()
{
    return "shape_cutter";
}

// 0x007283F0
object* alloc_shape_cutter()
{
    return shape_cutterpool.alloc(0);
}

// 0x00717EE0 (:132)
shape_cutter::shape_cutter()
{
    // delay and lifetime: timestamp() invalidates them
}

// 0x00717F20
shape_cutter::~shape_cutter()
{
}

// 0x007302C0 (:137)
// One cutter cuts per frame, going round them; all of them while a level loads (or while the demolitions master
// loads its layers), and all of those not made by an explosion.
void shape_cutter_do_frame()
{
    const int num = World.get_num_objects<shape_cutter>();
    if (num <= Current_cutter_index)
    {
        if (num == 0)
        {
            return;
        }
        Current_cutter_index = 0;
    }
    for (int i = 0; i < num; i++)
    {
        const int index = (Current_cutter_index + i) % num;
        shape_cutter* cutter = World.get_object<shape_cutter>(index);
        if (cutter == nullptr || !cutter->do_cut())
        {
            continue;
        }
        if (cutter->lifetime.elapsed())
        {
            World.destroy_object(cutter, false);
        }
        Current_cutter_index = index + 1;
        if (explosion_lookup_by_unique_id(cutter->exp_info_unique_id) != nullptr)
        {
            return;
        }
        if (gameseq_get_state() != GS_LOAD && Demolitions_master_activity.current_state != DM_STATE_LOADING_LAYERS)
        {
            return;
        }
    }
}

// 0x00728400 (:179)
// Returns whether it acted this frame (streamed in only).
bool shape_cutter::do_cut()
{
    if ((flags & 1) == 0)
    {
        return false;
    }
    const explosion_info* e_info = explosion_lookup_by_unique_id(exp_info_unique_id);
    if (delay.valid())
    {
        if (!delay.elapsed())
        {
            return false;
        }
        delay.invalidate();
        if (e_info != nullptr)
        {
            if (!break_links_only)
            {
                apply_explosion();
            }
            else
            {
                delay.set(e_info->time_between_breaks);
            }
            if (!lifetime.valid())
            {
                lifetime.set(e_info->expanding_explosion_duration);
                return true;
            }
        }
    }
    float cut_radius = radius;
    if (!lifetime.elapsed())
    {
        if (e_info == nullptr)
        {
            return true;
        }
        cut_radius = (1.0f - (float)lifetime.time_until() / (float)e_info->expanding_explosion_duration) * cut_radius; // grows over the duration
    }
    if (break_links_only)
    {
        apply_link_break(cut_radius);
        return true;
    }
    object* owner = World.object_from_handle(owner_handle);
    unsigned int handles[MAX_CUT_OBJECTS];
    const int num_handles = World.collect_objects<object_mover>(pos, cut_radius, handles, MAX_CUT_OBJECTS, nullptr, nullptr);
    for (int i = 0; i < num_handles; i++)
    {
        object_mover* mover = World.object_from_handle<object_mover>(handles[i]);
        if (mover == nullptr || !mover->obj_flags.streamed)
        {
            continue;
        }
        if (mover->get_sub_type() == OT_SUB_MOVER_RFG)
        {
            vector local_pos = pos;
            vec_change_to_local_coords(local_pos, mover->pos, mover->orient);
            rfg_dyn_add_pre_collide_shape_cutter((rfg_mover*)mover, owner, cut_radius, local_pos, exp_info_unique_id, false); // inlined
        }
        else if (mover->get_sub_type() == OT_SUB_MOVER_GENERAL && mover->get_handle() != source_handle)
        {
            mover->apply_damage(GENERAL_MOVER_CUT_DAMAGE, DBT_EXPLOSION, mover->pos, UP_VECTOR, owner, 0xFFFFFFFF, false);
        }
    }
    return true;
}

// 0x007205A0 (:274)
// The explosion's damage and impulses for everything within the radius (but humans and the source): projectiles,
// items, rfg_movers (script points, a shatter event with the vehicle damage, a building damaged event for buildings
// that are not removable), other movers, vehicles, debris and turrets; then the humans and the movers whose source
// exploded.
void shape_cutter::apply_explosion()
{
    const explosion_info* e_info = explosion_lookup_by_unique_id(exp_info_unique_id);
    if (e_info == nullptr)
    {
        return;
    }
    unsigned int handles[MAX_CUT_OBJECTS];
    const int num_handles = havok_collect_objects_fast(pos, radius, handles, MAX_CUT_OBJECTS, COLLIDE_OBJECT_COLLECTOR, expanding_explosion_callback, this);
    human* attacker = World.object_from_handle<human>(owner_handle);
    object* source = World.object_from_handle(source_handle);
    for (int i = 0; i < num_handles; i++)
    {
        object* target = World.object_from_handle(handles[i]);
        unsigned int damage;
        vector linear_impulse;
        vector angular_impulse;
        if (target == nullptr || !explosion_get_damage_and_impulse(damage, linear_impulse, angular_impulse, e_info, target, pos, nullptr, nullptr))
        {
            continue;
        }
        switch (target->get_obj_type())
        {
        case OT_ITEM:
            if (target->get_sub_type() == OT_SUB_ITEM_PROJECTILE)
            {
                ((projectile*)target)->process_explosion_hit(&linear_impulse, &angular_impulse, attacker);
            }
            else
            {
                item_process_explosion_hit((item*)target, damage, linear_impulse, angular_impulse);
            }
            break;
        case OT_MOVER:
            if (target->get_sub_type() == OT_SUB_MOVER_RFG && ((rfg_mover*)target)->get_move_type() == RFGM_FIXED)
            {
                vector local_pos = pos;
                vec_change_to_local_coords(local_pos, target->pos, target->orient);
                rfg_mover* rm = World.object_cast<rfg_mover>(target);
                rm->script_points_process_explosion_hit(attacker, e_info, pos, nullptr);
                rfg_dyn_add_pre_collide_shatter(rm, attacker, 0xFFFFFFFF, e_info->m_vehicle_max_damage_hitpoints, local_pos, ZERO_VECTOR, false); // inlined
                if ((rm->mover_flags & 4) == 0)
                {
                    building_damaged_by_explosion_event event;
                    event.attacker = attacker != nullptr ? attacker->get_handle() : 0xFFFFFFFF;
                    event.building = rm->get_handle();
                    event.info_handle = explosion_get_handle_from_ptr(e_info);
                    gem_post_event(event);
                }
            }
            else
            {
                object_mover_process_explosion_hit((object_mover*)target, attacker, (float)damage, linear_impulse, angular_impulse, e_info, pos, nullptr, w_info);
            }
            break;
        case OT_VEHICLE:
            vehicle_process_explosion_hit((vehicle*)target, attacker, source, pos, radius, damage, &linear_impulse, &angular_impulse, w_info, e_info);
            break;
        case OT_DEBRIS:
            object_debris_process_explosion_hit((object_debris*)target, &linear_impulse, &angular_impulse, false);
            break;
        case OT_TURRET:
            ((turret*)target)->process_explosion_hit(damage, linear_impulse, angular_impulse);
            break;
        default:
            break;
        }
    }
    object* exploded = source_handle != 0xFFFFFFFF ? World.object_from_handle(source_handle) : nullptr;
    explosion_process_humans(exploded, attacker, w_info, pos, nullptr, e_info, nullptr);
    object_mover_handle_source_exploded(exploded, e_info);
}

// 0x00720A70 (:368)
// One step of an explosion breaking over time within radiusp: a tenth of the impulses and the damage spread over the
// expansions. rfg_movers get shape cutter events; general movers the damage (none when removable); vehicles the damage
// (not from the nano rifle; the local player in it sees a hit indicator); debris impulses; turrets (not of vehicles)
// die.
void shape_cutter::apply_link_break(float radiusp)
{
    const explosion_info* e_info = explosion_lookup_by_unique_id(exp_info_unique_id);
    if (e_info == nullptr)
    {
        return;
    }
    unsigned int handles[MAX_CUT_OBJECTS];
    const int num_handles = havok_collect_objects_fast(pos, radiusp, handles, MAX_CUT_OBJECTS, COLLIDE_OBJECT_COLLECTOR, expanding_explosion_callback, this);
    human* attacker = World.object_from_handle<human>(owner_handle);
    for (int i = 0; i < num_handles; i++)
    {
        object* target = World.object_from_handle(handles[i]);
        unsigned int damage;
        vector linear_impulse;
        vector angular_impulse;
        if (target == nullptr || !explosion_get_damage_and_impulse(damage, linear_impulse, angular_impulse, e_info, target, pos, nullptr, nullptr))
        {
            continue;
        }
        linear_impulse = linear_impulse * LINK_BREAK_IMPULSE_SCALE;
        angular_impulse = angular_impulse * LINK_BREAK_IMPULSE_SCALE;
        damage /= (unsigned int)num_expansions;
        switch (target->get_obj_type())
        {
        case OT_MOVER:
        {
            rfg_mover* rm = World.object_cast<rfg_mover>(target);
            if (rm == nullptr || rm->get_shape() == nullptr)
            {
                if (target->get_sub_type() == OT_SUB_MOVER_GENERAL)
                {
                    if ((((object_mover*)target)->mover_flags & 4) != 0)
                    {
                        damage = 0;
                    }
                    object_mover_process_explosion_hit((object_mover*)target, attacker, (float)damage, linear_impulse, angular_impulse, e_info, pos, nullptr, nullptr);
                }
                break;
            }
            vector local_pos = pos;
            vec_change_to_local_coords(local_pos, rm->pos, rm->orient);
            rfg_dyn_add_pre_collide_shape_cutter(rm, attacker, radiusp, local_pos, exp_info_unique_id, false); // inlined
            break;
        }
        case OT_VEHICLE:
        {
            vehicle* vp = (vehicle*)target;
            if (w_info == nullptr || w_info->weapon_class != WEAPON_CLASS_NANO_RIFLE)
            {
                vehicle_apply_damage(vp, attacker, w_info, DBT_NONE, &pos, &angular_impulse, radiusp, damage, nullptr, false);
            }
            player* local_player = player_get_local_player();
            if (local_player != nullptr && human_in_vehicle(local_player, vp))
            {
                human_combat_process_hit_indicator(local_player, &pos, nullptr, (float)damage);
            }
            break;
        }
        case OT_DEBRIS:
            object_debris_process_explosion_hit((object_debris*)target, &linear_impulse, &angular_impulse, false);
            break;
        case OT_TURRET:
            if (!((turret*)target)->turret_flags.vehicle_turret)
            {
                ((turret*)target)->kill(nullptr, nullptr);
            }
            break;
        default:
            break;
        }
    }
    explosion_process_humans(nullptr, attacker, w_info, pos, nullptr, e_info, nullptr);
}

// 0x00728820 (:472)
// Saved: flags, radius, explosion, source and owner. Loading a saved cutter (with a radius) restarts it as a plain
// cutter; a new cutter takes the explosion's radius and delay (and breaks links over time when the explosion does).
bool shape_cutter::serialize(obj_prop_block& pblock, bool load)
{
    if (!object::serialize(pblock, load))
    {
        return false;
    }
    if (!load)
    {
        pblock.set(PROP_CUTTER_FLAGS, flags);
        pblock.set(PROP_CUTTER_RADIUS, radius);
        pblock.set(PROP_CUTTER_EXP_INFO, exp_info_unique_id);
        pblock.set(PROP_CUTTER_SOURCE_HANDLE, source_handle);
        pblock.set(PROP_CUTTER_OWNER_HANDLE, owner_handle);
        return true;
    }
    break_links_only = 0;
    num_expansions = 0;
    flags = 0;
    pblock.get_optional(PROP_CUTTER_FLAGS, flags);
    if (pblock.get_optional(PROP_CUTTER_RADIUS, radius))
    {
        lifetime.set(0);
        delay.invalidate();
        source_handle = 0xFFFFFFFF;
        owner_handle = 0xFFFFFFFF;
        pblock.get_optional(PROP_CUTTER_SOURCE_HANDLE, source_handle);
        pblock.get_optional(PROP_CUTTER_OWNER_HANDLE, owner_handle);
        exp_info_unique_id = 0xFF;
        pblock.get_optional(PROP_CUTTER_EXP_INFO, exp_info_unique_id);
        apply_impulse_no_delete = explosion_lookup_by_unique_id(exp_info_unique_id) != nullptr;
        w_info = nullptr;
        return true;
    }
    apply_impulse_no_delete = 0;
    pblock.get_optional(PROP_CUTTER_EXP_INFO, exp_info_unique_id);
    const explosion_info* e_info = explosion_lookup_by_unique_id(exp_info_unique_id);
    if (e_info == nullptr)
    {
        return false;
    }
    pblock.get(PROP_CUTTER_SOURCE_HANDLE, source_handle);
    pblock.get(PROP_CUTTER_OWNER_HANDLE, owner_handle);
    if (e_info->time_between_breaks != 0)
    {
        break_links_only = 1;
        num_expansions = (int)e_info->expanding_explosion_duration / e_info->time_between_breaks;
    }
    pblock.get(PROP_CUTTER_WEAPON_INFO, w_info);
    radius = e_info->m_radius;
    delay.set(e_info->expanding_explosion_delay);
    lifetime.invalidate();
    return true;
}

// 0x00717F30 (:563)
// Cuts only while streamed in (flag 0x1).
bool shape_cutter::stream(bool load)
{
    if (!object::stream(load))
    {
        return false;
    }
    if (load)
    {
        flags |= 1;
    }
    else
    {
        flags &= ~1;
    }
    return true;
}

// 0x00717F70 (:585)
void shape_cutter::get_bbox(vector& bmin, vector& bmax) const
{
    get_local_bbox(bmin, bmax);
    bmin += pos;
    bmax += pos;
}

// 0x00718040 (:593)
void shape_cutter::get_local_bbox(vector& bmin, vector& bmax) const
{
    bmin.set(-radius, -radius, -radius);
    bmax.set(radius, radius, radius);
}

// 0x00711620 (.h:31)
const char* shape_cutter::get_classname_static()
{
    return "shape_cutter";
}

// 0x00711630 (.h:31)
int shape_cutter::get_bpo_index_static()
{
    return 1;
}

// 0x00711600 (.h:31)
const object_type shape_cutter::get_type_static()
{
    return OT_SHAPE_CUTTER;
}

// 0x00711610 (.h:31)
const object_sub_type shape_cutter::get_subtype_static()
{
    return OT_SUB_UNDEFINED;
}

// 0x00717F10 (.h:31)
const char* shape_cutter::get_classname() const
{
    return "shape_cutter";
}

// 0x0071B8C0 (.h:78)
shape_cutter_props::shape_cutter_props(const vector& pos, const matrix& orient, const float radius, const unsigned char exp_info, const unsigned int source_handle, const unsigned int owner_handle)
    : obj_prop_block(pos, orient, nullptr)
{
    set(PROP_CUTTER_RADIUS, radius);
    set(PROP_CUTTER_EXP_INFO, exp_info);
    set(PROP_CUTTER_SOURCE_HANDLE, source_handle);
    set(PROP_CUTTER_OWNER_HANDLE, owner_handle);
}

// 0x006E75E0 (.h:95)
shape_cutter_props::shape_cutter_props(const vector& pos, const matrix& orient, const unsigned char exp_info, const weapon_info* w_info, const unsigned int source_handle, const unsigned int owner_handle)
    : obj_prop_block(pos, orient, nullptr)
{
    set(PROP_CUTTER_EXP_INFO, exp_info);
    set(PROP_CUTTER_SOURCE_HANDLE, source_handle);
    set(PROP_CUTTER_OWNER_HANDLE, owner_handle);
    set(PROP_CUTTER_WEAPON_INFO, w_info);
}

// 0x006E1DE0
shape_cutter_props::~shape_cutter_props()
{
}
