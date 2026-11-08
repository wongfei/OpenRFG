// Original: volition/rfg/code/objects/custom_object.cpp
//
// Scripted behaviour of special movers, chosen by the mover's level object properties name in
// custom_object_set_callbacks: windmills, the jk widget, mission computers, EDF power cores, the excavator blade and
// engine, kiosks, fire barrels, smokestacks and vents that stop their effects when damaged or moved, and objects that
// die when moved. The four global event callbacks registered by custom_object_level_init_pre forward a mover's stream
// in, damage, destroy and use events to the callbacks stored on the mover; a callback that answers EVENT_HANDLED is
// dropped. Movers that need watching every frame are kept in Object_process_pool and run by custom_object_do_frame.
// Compiled in rfg_objects_unity.cpp.

#include "volition/rfg/code/objects/custom_object.h"

#include "volition/rfg/code/code_effects/bullet_sparks.h"
#include "volition/rfg/code/destruction/rfg_materials.h"
#include "volition/rfg/code/destruction/rfg_mover.h"
#include "volition/rfg/code/effects/effects.h"
#include "volition/rfg/code/effects/object_effect.h"
#include "volition/rfg/code/gamewide/tweak_table.h"
#include "volition/rfg/code/havok/constraint_template.h"
#include "volition/rfg/code/havok/havok.h"
#include "volition/rfg/code/havok/havok_convert.h"
#include "volition/rfg/code/havok/havok_ragdoll.h"
#include "volition/rfg/code/level/rfg_chunk_shared.h"
#include "volition/rfg/code/level/world.h"
#include "volition/rfg/code/objects/humans/human.h"
#include "volition/rfg/code/objects/humans/human_collide.h"
#include "volition/rfg/code/objects/humans/human_ragdoll.h"
#include "volition/rfg/code/objects/level_object_properties.h"
#include "volition/rfg/code/objects/movers/general_mover.h"
#include "volition/rfg/code/objects/movers/object_mover.h"
#include "volition/rfg/code/objects/object.h"
#include "volition/rfg/code/objects/object_constraint.h"
#include "volition/rfg/code/scripting/action_script/global_event_data.h"
#include "volition/rfg/code/scripting/action_script/global_event_manager.h"
#include "volition/rfg/code/scripting/public_address.h"
#include "volition/rfg/code/weapons/weapons.h"
#include "volition/vlib/code/math/matrix.h"
#include "volition/vlib/code/math/rand.h"
#include "volition/vlib/code/math/vector3.h"
#include "volition/vlib/code/util/string_comp.h"
#include "volition/vlib/code/util/varray.h"

#include <math.h>

// ---- globals

farray<custom_object_do_frame_callbacks,64> Object_process_pool; // 0x033C63C0

static float Excavator_player_damage = 250.0f; // 0x0169422C
static tweak_table_item Tweak_table_item_Excavator_player_damage("Excavator_player_damage", &Excavator_player_damage); // 0x034309BC
static float Excavator_radial_damage_vel_scalar = 2.0f; // 0x01694230
static tweak_table_item Tweak_table_item_Excavator_radial_damage_vel_scalar("Excavator_radial_damage_vel_scalar", &Excavator_radial_damage_vel_scalar);
static float Excavator_tangent_damage_vel_scalar = 6.0f; // 0x01694234
static tweak_table_item Tweak_table_item_Excavator_tangent_damage_vel_scalar("Excavator_tangent_damage_vel_scalar", &Excavator_tangent_damage_vel_scalar);
static float Excavator_stop_blade_force = 10000.0f; // 0x01694238
static tweak_table_item Tweak_table_item_Excavator_stop_blade_force("Excavator_stop_blade_force", &Excavator_stop_blade_force);

// Magic numbers shared by the callbacks below.
static const unsigned int CUSTOM_OBJECT_MOVER_FLAG_LINKED = 0x40000; // object_mover::mover_flags bit 18: linked to its host
static const unsigned int CUSTOM_OBJECT_MOVER_FLAG_NO_CALLBACKS = 0x4; // object_mover::mover_flags bit 2: no custom callbacks
static const float CUSTOM_OBJECT_KILL_DAMAGE = 1000.0f; // the damage that kills a windmill motor
static const int WINDMILL_BLADE_FALL_MS = 3000; // after the blade's first damage, the time until it breaks off
static const int WINDMILL_BLADE_KILL_MOTOR_MS = 1751; // 0x6D7: the time before that when the motors die
static const int FIRE_BARREL_TIPPED_MS = 3000; // a tipped fire barrel burns this long before its effects stop

// 0x00A05D80 (.h:29)
custom_object_do_frame_callbacks::custom_object_do_frame_callbacks()
    : object_handle(0xFFFFFFFF)
    , event_handle(nullptr)
    , frame_process_callback(nullptr)
{
}

// ---- internal functions

// 0x00A2C700 (:63)
// A linked mover whose properties say the link affects its effects does not start them.
static bool custom_effects_should_start_effects(const object_mover* obj)
{
    if (obj != nullptr && obj->get_obj_type() == OT_MOVER && obj->get_sub_type() == OT_SUB_MOVER_GENERAL && obj->m_level_obj_properties->m_base_properties->m_flags.m_link_affects_effects &&
        (obj->mover_flags & CUSTOM_OBJECT_MOVER_FLAG_LINKED) != 0)
    {
        return false;
    }
    return true;
}

// 0x00A51940 (:76). LTCG: obj in ESI.
// Starts the three custom effects of the mover's properties: at the chunk script point named by the effect's attachment
// point when there is one, otherwise on the mover. The effects outlive the mover's streaming.
// Original: with obj null custom_effects_should_start_effects answers true and is_killed is called on null; no caller
// passes null.
static void custom_effects_start_all(const object_mover* obj)
{
    if (!custom_effects_should_start_effects(obj) || obj->is_killed())
    {
        return;
    }
    level_object_inst_properties* props = obj->m_level_obj_properties;
    for (int i = 0; i < 3; i++)
    {
        const custom_effect_info& info = props->m_base_properties->m_custom_effect_infos[i];
        const unsigned int effect_handle = info.effect_handle;
        if (effect_handle == 0xFFFFFFFF)
        {
            continue;
        }
        unsigned int effect = 0xFFFFFFFF;
        bool played = false;
        const char* point_name = info.attachment_point_name;
        if (point_name != nullptr)
        {
            const c_chunk_base* chunk = chunk_get_base_by_uid(obj->chunk_uid);
            if (chunk != nullptr)
            {
                for (int point = 0; point < chunk->num_script_points; point++)
                {
                    if (v_stricmp(chunk->script_points.to_pointer()[point].name, point_name) == 0)
                    {
                        effect = effects_play_at_script_point(effect_handle, obj->get_handle(), point, true, nullptr, nullptr);
                        played = true;
                        break;
                    }
                }
            }
        }
        if (!played)
        {
            effect = effects_play(effect_handle, obj->pos, obj->orient, true, obj->get_handle(), -1, 0xFFFFFFFF, 1.0f, false, EFFECTS_CATEGORY_NONE);
        }
        props->m_custom_effects[i] = effect;
        if (props->m_custom_effects[i] != 0xFFFFFFFF)
        {
            object* effect_objp = World.object_from_handle(props->m_custom_effects[i]);
            if (effect_objp != nullptr && effect_objp->get_obj_type() == OT_EFFECT)
            {
                effect_objp->obj_flags.destroy_on_stream = false;
            }
        }
    }
}

// 0x00A2C740 (:138). LTCG: handle in ESI.
static int processing_pool_find(unsigned int handle)
{
    for (int i = 0; i < Object_process_pool.size(); i++)
    {
        if (Object_process_pool[i].object_handle == handle)
        {
            return i;
        }
    }
    return -1;
}

// 0x00A2C770 (:150). LTCG: handle in ESI.
static void remove_object_from_processing_pool(unsigned int handle)
{
    const int index = processing_pool_find(handle);
    if (index != -1)
    {
        Object_process_pool.remove(index);
    }
}

// The mover of a processing pool entry, or null when it is gone or not a mover.
static object_mover* processing_pool_get_mover(int index)
{
    const unsigned int handle = Object_process_pool[index].object_handle;
    if (handle == 0xFFFFFFFF)
    {
        return nullptr;
    }
    object* objp = World.object_from_handle(handle);
    if (objp == nullptr || objp->get_obj_type() != OT_MOVER)
    {
        return nullptr;
    }
    return (object_mover*)objp;
}

// The mover of a mover event (stream in, damage, destroy), or null when it is gone or not a mover.
static object_mover* custom_object_event_mover(unsigned int handle)
{
    if (handle == 0xFFFFFFFF)
    {
        return nullptr;
    }
    object* objp = World.object_from_handle(handle);
    if (objp == nullptr || objp->get_obj_type() != OT_MOVER)
    {
        return nullptr;
    }
    return (object_mover*)objp;
}

// Adds a processing pool entry for handle; false when the pool is full.
static bool processing_pool_add(unsigned int handle, notify_info* event_handle, void (*callback)(int))
{
    custom_object_do_frame_callbacks entry;
    entry.object_handle = handle;
    entry.event_handle = event_handle;
    entry.frame_process_callback = callback;
    if (Object_process_pool.size() >= Object_process_pool.capacity())
    {
        return false;
    }
    return Object_process_pool.add(entry) != -1;
}

// 0x00A51AD0 (:166)
// The default stream in: restarts the custom effects.
static event_result object_mover_stream_in_callback(const event_data& in_event, notify_info* info)
{
    if (in_event.get_type() != EVENT_MOVER_STREAM_IN)
    {
        return EVENT_IGNORED;
    }
    object_mover* obj = custom_object_event_mover(((const mover_stream_in_event&)in_event).handle);
    if (obj == nullptr)
    {
        return EVENT_IGNORED;
    }
    custom_object_stop_custom_effects(obj);
    custom_effects_start_all(obj);
    return EVENT_OK;
}

// 0x00A51B20 (:186)
// The default destroy: stops the custom effects, once.
static event_result object_mover_destroyed_callback(const event_data& in_event, notify_info* info)
{
    if (in_event.get_type() != EVENT_BUILDING_DESTROYED)
    {
        return EVENT_IGNORED;
    }
    object_mover* obj = custom_object_event_mover(((const building_destroyed_event&)in_event).building);
    if (obj == nullptr)
    {
        return EVENT_IGNORED;
    }
    custom_object_stop_custom_effects(obj);
    return EVENT_HANDLED;
}

// 0x00A51B60 (:209)
// Smokestacks and vents: the effects stop once the mover moves, or when it streams out or loses its body.
static void stop_effect_on_move_do_frame_callback(int index)
{
    object_mover* obj = processing_pool_get_mover(index);
    if (obj == nullptr)
    {
        Object_process_pool.remove(index);
        return;
    }
    if (!obj->obj_flags.streamed || obj->havok_handle == 0xFFFFFFFF)
    {
        custom_object_stop_custom_effects(obj);
        Object_process_pool.remove(index);
        return;
    }
    vector vel;
    havok_body_get_linear_velocity(obj->havok_handle, vel);
    if (!vel.is_null())
    {
        custom_object_stop_custom_effects(obj);
        Object_process_pool.remove(index);
    }
}

// 0x00A51C70 (:236)
// A damaged windmill blade breaks off when its timer runs out: its constraints go and its effects stop. 1.75 s before,
// while it still has hitpoints, the motors of its host's other children are killed and its hitpoints zeroed.
static void windmill_blade_do_frame_callback(int index)
{
    object_mover* obj = processing_pool_get_mover(index);
    if (obj == nullptr || obj->get_sub_type() != OT_SUB_MOVER_RFG)
    {
        Object_process_pool.remove(index);
        return;
    }
    level_object_inst_properties* props = obj->m_level_obj_properties;
    if (!props->m_death_timer.valid())
    {
        return;
    }
    if (props->m_death_timer.elapsed())
    {
        constraint* cp = obj->host_constraint_ptr;
        while (cp != nullptr)
        {
            constraint* next = cp->host_constraint_next;
            constraint* first = obj->host_constraint_ptr;
            World.destroy_object(cp->get_handle(), false);
            cp = next != first ? next : nullptr;
        }
        cp = obj->child_constraint_ptr;
        while (cp != nullptr)
        {
            constraint* next = cp->child_constraint_next;
            constraint* first = obj->child_constraint_ptr;
            World.destroy_object(cp->get_handle(), false);
            cp = next != first ? next : nullptr;
        }
        custom_object_stop_custom_effects(obj);
        Object_process_pool.remove(index);
        return;
    }
    if (props->m_hitpoints == 0 || props->m_death_timer.time_until() >= WINDMILL_BLADE_KILL_MOTOR_MS)
    {
        return;
    }
    constraint* cp = obj->child_constraint_ptr;
    while (cp != nullptr)
    {
        constraint* current = cp;
        cp = current->child_constraint_next != obj->child_constraint_ptr ? current->child_constraint_next : nullptr;
        if (current->constraint_info.type != HAVOK_CONSTRAINT_HINGE || current->host_handle == 0xFFFFFFFF)
        {
            continue;
        }
        object* host = World.object_from_handle(current->host_handle);
        if (host == nullptr || host->get_obj_type() != OT_MOVER)
        {
            continue;
        }
        constraint* hcp = host->host_constraint_ptr;
        while (hcp != nullptr)
        {
            if (hcp->child_handle != 0xFFFFFFFF)
            {
                object* child = World.object_from_handle(hcp->child_handle);
                if (child != nullptr && child->get_obj_type() == OT_MOVER && child->get_sub_type() == OT_SUB_MOVER_GENERAL)
                {
                    ((object_mover*)child)->apply_damage(CUSTOM_OBJECT_KILL_DAMAGE, DBT_LEVEL_OBJECT, child->pos, ZERO_VECTOR, nullptr, 0xFFFFFFFF, false);
                }
            }
            hcp = hcp->host_constraint_next;
            if (hcp == host->host_constraint_ptr)
            {
                break;
            }
        }
    }
    props->m_hitpoints = 0;
}

// 0x00A2C7D0 (:305)
// A windmill motor dies once it turns at 1 m/s or more.
static void windmill_motor_do_frame_callback(int index)
{
    object_mover* obj = processing_pool_get_mover(index);
    if (obj == nullptr || obj->get_sub_type() != OT_SUB_MOVER_GENERAL)
    {
        Object_process_pool.remove(index);
        return;
    }
    hkpRigidBody* rb = havok_body_get_pointer(obj->havok_handle);
    if (rb != nullptr)
    {
        vector vel;
        havok_to_game_vector(vel, rb->m_motion.m_linearVelocity);
        const float speed_squared = (float)((double)vel.x * vel.x + (double)vel.y * vel.y + (double)vel.z * vel.z);
        if (speed_squared < 1.0f)
        {
            return;
        }
        obj->apply_damage(CUSTOM_OBJECT_KILL_DAMAGE, DBT_LEVEL_OBJECT, obj->pos, ZERO_VECTOR, nullptr, 0xFFFFFFFF, false);
    }
    Object_process_pool.remove(index);
}

// 0x00A51EC0 (:334)
// The excavator blade keeps its body awake while its motor turns and it stands upright; otherwise it stops colliding
// with humans and its effects stop. A blade that streams out or loses its body keeps the human collision flag set.
static void excavator_blade_do_frame_callback(int index)
{
    object_mover* obj = processing_pool_get_mover(index);
    if (obj == nullptr)
    {
        gem_unregister_for_event(Object_process_pool[index].event_handle);
        Object_process_pool.remove(index);
        return;
    }
    if (!obj->obj_flags.streamed || obj->havok_handle == 0xFFFFFFFF)
    {
        gem_unregister_for_event(Object_process_pool[index].event_handle);
        obj->m_level_obj_properties->m_flags.m_generate_human_collision_event = true;
        custom_object_stop_custom_effects(obj);
        Object_process_pool.remove(index);
        return;
    }
    const constraint* cp = obj->child_constraint_ptr;
    // Magic number: 0.1 = the largest |rvec.y| of an upright blade.
    if (cp == nullptr || !havok_constraint_has_motor(cp->hk_constraint) || 0.1f <= fabsf(obj->orient.rvec.y))
    {
        gem_unregister_for_event(Object_process_pool[index].event_handle);
        obj->m_level_obj_properties->m_flags.m_generate_human_collision_event = false;
        custom_object_stop_custom_effects(obj);
        Object_process_pool.remove(index);
        return;
    }
    if (!havok_body_is_active(obj->havok_handle))
    {
        havok_body_force_activate(obj->havok_handle);
    }
}

// 0x00A52050 (:377)
// A fire barrel that tips over (uvec.y below 0.9) burns 3 more seconds.
static void fire_barrel_do_frame_callback(int index)
{
    object_mover* obj = processing_pool_get_mover(index);
    if (obj == nullptr || !obj->obj_flags.streamed)
    {
        Object_process_pool.remove(index);
        return;
    }
    custom_object_do_frame_callbacks& entry = Object_process_pool[index];
    if (!entry.timer.valid() && obj->orient.uvec.y < 0.9f)
    {
        entry.timer.set(FIRE_BARREL_TIPPED_MS);
    }
    if (entry.timer.elapsed())
    {
        custom_object_stop_custom_effects(obj);
        Object_process_pool.remove(index);
    }
}

// 0x00A2C920 (:400)
// PA speakers and portable comms die (their full hitpoints as damage) once they move.
static void kill_on_move_do_frame_callback(int index)
{
    object_mover* obj = processing_pool_get_mover(index);
    if (obj == nullptr)
    {
        Object_process_pool.remove(index);
        return;
    }
    if (obj->havok_handle != 0xFFFFFFFF)
    {
        vector vel;
        havok_body_get_linear_velocity(obj->havok_handle, vel);
        if (vel.is_null())
        {
            return;
        }
        obj->apply_damage((float)obj->m_level_obj_properties->m_base_properties->m_max_hitpoints, DBT_LEVEL_OBJECT, obj->pos, ZERO_VECTOR, nullptr, 0xFFFFFFFF, false);
    }
    Object_process_pool.remove(index);
}

// Stops and forgets custom effect i of a mover when it is still running.
static void custom_object_stop_effect(level_object_inst_properties* props, int i)
{
    if (props->m_custom_effects[i] == 0xFFFFFFFF)
    {
        return;
    }
    object_effect* effect = World.object_from_handle<object_effect>(props->m_custom_effects[i]);
    if (effect != nullptr)
    {
        effect->stop(object_effect::SS_STOP_EMITTING);
        props->m_custom_effects[i] = 0xFFFFFFFF;
    }
}

// 0x00A2CA10 (:433)
// The jk widget: past 40 % destroyed (sub piece 0) both effects stop for good; otherwise effect 1 stops once sub piece 1
// takes any damage.
static event_result jk_widget_damaged_callback(const event_data& in_event, notify_info* info)
{
    if (in_event.get_type() != EVENT_BUILDING_DAMAGED)
    {
        return EVENT_IGNORED;
    }
    const unsigned int handle = ((const building_damaged_event&)in_event).building;
    if (handle == 0xFFFFFFFF)
    {
        return EVENT_IGNORED;
    }
    object* objp = World.object_from_handle(handle);
    if (objp == nullptr || objp->get_obj_type() != OT_MOVER || objp->get_sub_type() != OT_SUB_MOVER_RFG)
    {
        return EVENT_IGNORED;
    }
    rfg_mover* mover = (rfg_mover*)objp;
    level_object_inst_properties* props = mover->m_level_obj_properties;
    float percent = 0.0f;
    // Magic number: 0.4 = the part of sub piece 0 destroyed that kills the widget.
    if (mover->get_sub_piece_percent_destroyed(percent, 0) && 0.4f < percent)
    {
        custom_object_stop_effect(props, 0);
        custom_object_stop_effect(props, 1);
        return EVENT_HANDLED;
    }
    if (props->m_custom_effects[1] != 0xFFFFFFFF && mover->get_sub_piece_percent_destroyed(percent, 1) && 0.0f < percent)
    {
        object_effect* effect = World.object_from_handle<object_effect>(props->m_custom_effects[1]);
        if (effect != nullptr)
        {
            effect->stop(object_effect::SS_STOP_EMITTING);
            props->m_custom_effects[1] = 0xFFFFFFFF;
        }
    }
    return EVENT_OK;
}

// 0x00A52140 (:493)
// Vents and antennas: watched every frame until they move. The effects start only when the pool takes the mover.
static event_result stop_effect_on_move_stream_in_callback(const event_data& in_event, notify_info* info)
{
    if (in_event.get_type() != EVENT_MOVER_STREAM_IN)
    {
        return EVENT_IGNORED;
    }
    const unsigned int handle = ((const mover_stream_in_event&)in_event).handle;
    object_mover* obj = custom_object_event_mover(handle);
    if (obj == nullptr)
    {
        return EVENT_IGNORED;
    }
    if (processing_pool_add(handle, nullptr, stop_effect_on_move_do_frame_callback))
    {
        custom_effects_start_all(obj);
    }
    return EVENT_OK;
}

// 0x00A521D0 (:516)
// Smokestacks: a destroyable one that is already partly destroyed streams in without its effects.
static event_result stop_effect_on_destroyable_stream_in_callback(const event_data& in_event, notify_info* info)
{
    if (in_event.get_type() != EVENT_MOVER_STREAM_IN)
    {
        return EVENT_IGNORED;
    }
    object_mover* obj = custom_object_event_mover(((const mover_stream_in_event&)in_event).handle);
    if (obj == nullptr)
    {
        return EVENT_IGNORED;
    }
    if (obj->get_sub_type() == OT_SUB_MOVER_RFG && obj->percent_destroyed() > 0.0f)
    {
        return EVENT_OK;
    }
    custom_effects_start_all(obj);
    return EVENT_OK;
}

// 0x00A52230 (:542)
// A destroyable smokestack or vent stops its effects at its first damage; a general mover drops this callback.
static event_result stop_effect_on_destroyable_damaged_callback(const event_data& in_event, notify_info* info)
{
    if (in_event.get_type() != EVENT_BUILDING_DAMAGED)
    {
        return EVENT_IGNORED;
    }
    const unsigned int handle = ((const building_damaged_event&)in_event).building;
    object_mover* obj = custom_object_event_mover(handle);
    if (obj == nullptr)
    {
        return EVENT_IGNORED;
    }
    if (obj->get_sub_type() == OT_SUB_MOVER_GENERAL)
    {
        return EVENT_HANDLED;
    }
    custom_object_stop_custom_effects(obj);
    remove_object_from_processing_pool(handle);
    return EVENT_OK;
}

// 0x00A52280 (:568)
static event_result stop_effect_on_move_destroyed_callback(const event_data& in_event, notify_info* info)
{
    if (in_event.get_type() != EVENT_BUILDING_DESTROYED)
    {
        return EVENT_IGNORED;
    }
    const unsigned int handle = ((const building_destroyed_event&)in_event).building;
    object_mover* obj = custom_object_event_mover(handle);
    if (obj == nullptr)
    {
        return EVENT_IGNORED;
    }
    custom_object_stop_custom_effects(obj);
    remove_object_from_processing_pool(handle);
    return EVENT_HANDLED;
}

// 0x00A522D0 (:592)
// The windmill blades' first damage starts their 3 s fall (windmill_blade_do_frame_callback); the callback is dropped.
// Original: the custom effects start only when the pool cannot take the blade.
static event_result windmill_blade_damaged_callback(const event_data& in_event, notify_info* info)
{
    if (in_event.get_type() != EVENT_BUILDING_DAMAGED)
    {
        return EVENT_IGNORED;
    }
    const unsigned int handle = ((const building_damaged_event&)in_event).building;
    object_mover* obj = custom_object_event_mover(handle);
    if (obj == nullptr || obj->get_sub_type() != OT_SUB_MOVER_RFG)
    {
        return EVENT_IGNORED;
    }
    obj->m_level_obj_properties->m_death_timer.set(WINDMILL_BLADE_FALL_MS);
    if (!processing_pool_add(handle, nullptr, windmill_blade_do_frame_callback))
    {
        custom_effects_start_all(obj);
    }
    return EVENT_HANDLED;
}

// 0x00A52380 (:617). LTCG: custom_effects_start_all gets the blade in ESI.
// The blades stream in turned by a random angle around their hub (the constraint's position).
static event_result windmill_blade_stream_in_callback(const event_data& in_event, notify_info* info)
{
    if (in_event.get_type() != EVENT_MOVER_STREAM_IN)
    {
        return EVENT_IGNORED;
    }
    object_mover* obj = custom_object_event_mover(((const mover_stream_in_event&)in_event).handle);
    if (obj == nullptr || obj->child_constraint_ptr == nullptr)
    {
        return EVENT_IGNORED;
    }
    const vector hub = obj->child_constraint_ptr->pos;
    matrix new_orient(obj->orient);
    // Magic number: 6.2831855 = 2 pi.
    new_orient.rotate_around_local_z(fl_rand_range(0.0f, 6.2831855f));
    vector new_pos(obj->pos.x - hub.x, obj->pos.y - hub.y, obj->pos.z - hub.z);
    new_pos.rotate(obj->orient);
    new_pos.unrotate(new_orient);
    new_pos.x = new_pos.x + hub.x;
    new_pos.y = new_pos.y + hub.y;
    new_pos.z = new_pos.z + hub.z;
    obj->update_pos_and_orient(new_pos, new_orient, true);
    custom_effects_start_all(obj);
    return EVENT_OK;
}

// 0x00A2CB50 (:653)
// The windmill base remembers its motor (the general mover among its constraint children).
// Original: the handle is kept in m_prefix_effects[0] of the base's properties.
static event_result windmill_base_stream_in_callback(const event_data& in_event, notify_info* info)
{
    if (in_event.get_type() != EVENT_MOVER_STREAM_IN)
    {
        return EVENT_IGNORED;
    }
    object_mover* obj = custom_object_event_mover(((const mover_stream_in_event&)in_event).handle);
    if (obj == nullptr)
    {
        return EVENT_IGNORED;
    }
    constraint* cp = obj->host_constraint_ptr;
    while (cp != nullptr)
    {
        constraint* current = cp;
        cp = current->host_constraint_next != obj->host_constraint_ptr ? current->host_constraint_next : nullptr;
        if (World.object_from_handle<general_mover>(current->child_handle) != nullptr)
        {
            obj->m_level_obj_properties->m_prefix_effects[0] = current->child_handle;
        }
    }
    return EVENT_OK;
}

// 0x00A52520 (:678)
// The windmill base's first real damage watches its motor (windmill_motor_do_frame_callback); the callback is dropped.
// Original: the custom effects start only when the pool cannot take the motor.
static event_result windmill_base_damaged_callback(const event_data& in_event, notify_info* info)
{
    if (in_event.get_type() != EVENT_BUILDING_DAMAGED)
    {
        return EVENT_IGNORED;
    }
    const building_damaged_event& damaged = (const building_damaged_event&)in_event;
    object_mover* obj = custom_object_event_mover(damaged.building);
    if (obj == nullptr || obj->get_sub_type() != OT_SUB_MOVER_RFG)
    {
        return EVENT_IGNORED;
    }
    if (damaged.percent == 0.0f)
    {
        return EVENT_OK;
    }
    const unsigned int motor_handle = obj->m_level_obj_properties->m_prefix_effects[0];
    if (motor_handle != 0xFFFFFFFF)
    {
        object* motor = World.object_from_handle(motor_handle);
        if (motor != nullptr && motor->get_obj_type() == OT_MOVER && motor->get_sub_type() == OT_SUB_MOVER_GENERAL)
        {
            if (!processing_pool_add(obj->m_level_obj_properties->m_prefix_effects[0], nullptr, windmill_motor_do_frame_callback))
            {
                custom_effects_start_all(obj);
            }
        }
    }
    return EVENT_HANDLED;
}

// Plays custom effect i of a mover on it and keeps the effect alive when the mover streams out.
static void custom_object_play_effect(object_mover* obj, int i)
{
    level_object_inst_properties* props = obj->m_level_obj_properties;
    props->m_custom_effects[i] = effects_play(props->m_base_properties->m_custom_effect_infos[i].effect_handle, obj->pos, obj->orient, true, obj->get_handle(), -1, 0xFFFFFFFF, 1.0f, false, EFFECTS_CATEGORY_NONE);
    object_effect* effect = World.object_from_handle<object_effect>(props->m_custom_effects[i]);
    if (effect != nullptr)
    {
        effect->obj_flags.destroy_on_stream = false;
    }
}

// 0x00A2CBD0 (:713)
// Mission computers start their idle effect (0) and the effect in slot 2.
// Original: slot 2 plays when its handle is not 0, so a running effect (0xFFFFFFFF too) is replaced and leaks; slot 0
// plays only when none runs.
static event_result computer_stream_in_callback(const event_data& in_event, notify_info* info)
{
    if (in_event.get_type() != EVENT_MOVER_STREAM_IN)
    {
        return EVENT_IGNORED;
    }
    object_mover* obj = custom_object_event_mover(((const mover_stream_in_event&)in_event).handle);
    if (obj == nullptr)
    {
        return EVENT_IGNORED;
    }
    if (custom_effects_should_start_effects(obj))
    {
        level_object_inst_properties* props = obj->m_level_obj_properties;
        if (props->m_base_properties->m_custom_effect_infos[0].effect_handle != 0xFFFFFFFF && props->m_custom_effects[0] == 0xFFFFFFFF)
        {
            custom_object_play_effect(obj, 0);
        }
        if (props->m_base_properties->m_custom_effect_infos[2].effect_handle != 0xFFFFFFFF && props->m_custom_effects[2] != 0)
        {
            custom_object_play_effect(obj, 2);
        }
    }
    return EVENT_OK;
}

// 0x00A2CCF0 (:766)
static event_result computer_destroyed_callback(const event_data& in_event, notify_info* info)
{
    if (in_event.get_type() != EVENT_BUILDING_DESTROYED)
    {
        return EVENT_IGNORED;
    }
    object_mover* obj = custom_object_event_mover(((const building_destroyed_event&)in_event).building);
    if (obj == nullptr)
    {
        return EVENT_IGNORED;
    }
    level_object_inst_properties* props = obj->m_level_obj_properties;
    for (int i = 0; i < 3; i++)
    {
        if (props->m_custom_effects[i] != 0xFFFFFFFF)
        {
            object_effect* effect = World.object_from_handle<object_effect>(props->m_custom_effects[i]);
            if (effect != nullptr)
            {
                World.destroy_object(effect->get_handle(), false);
                props->m_custom_effects[i] = 0xFFFFFFFF;
            }
        }
    }
    return EVENT_HANDLED;
}

// 0x00A2CD70 (:790)
// A human starting to use a computer's action node swaps its effect from slot 2 to slot 1; stopping swaps back.
static event_result computer_use_action_callback(const event_data& in_event, notify_info* info)
{
    if (in_event.get_type() != EVENT_HUMAN_USING_ACTION_NODE)
    {
        return EVENT_IGNORED;
    }
    const human_using_action_node_event& using_event = (const human_using_action_node_event&)in_event;
    if (using_event.action_node_handle == 0xFFFFFFFF)
    {
        return EVENT_IGNORED;
    }
    object* node = World.object_from_handle(using_event.action_node_handle);
    if (node == nullptr || node->get_obj_type() != OT_ACTION_NODE || node->get_parent() == 0xFFFFFFFF)
    {
        return EVENT_IGNORED;
    }
    object_mover* obj = custom_object_event_mover(node->get_parent());
    if (obj == nullptr)
    {
        return EVENT_IGNORED;
    }
    level_object_inst_properties* props = obj->m_level_obj_properties;
    const int stop_slot = using_event.start ? 2 : 1;
    const int play_slot = using_event.start ? 1 : 2;
    if (props->m_custom_effects[stop_slot] != 0xFFFFFFFF)
    {
        object_effect* effect = World.object_from_handle<object_effect>(props->m_custom_effects[stop_slot]);
        if (effect != nullptr)
        {
            World.destroy_object(effect->get_handle(), false);
            props->m_custom_effects[stop_slot] = 0xFFFFFFFF;
        }
    }
    if (props->m_base_properties->m_custom_effect_infos[play_slot].effect_handle == 0xFFFFFFFF || props->m_custom_effects[play_slot] != 0xFFFFFFFF)
    {
        return EVENT_OK;
    }
    custom_object_play_effect(obj, play_slot);
    return EVENT_OK;
}

// 0x00A2CF10 (:864). LTCG: omp in EDI.
// Tells the power core's effects how damaged it is (EFFECTS_VAR_PERCENT).
static void power_core_set_effect_variables(object_mover* omp, float percent)
{
    for (int i = 0; i < 3; i++)
    {
        const unsigned int handle = omp->m_level_obj_properties->m_custom_effects[i];
        if (handle == 0xFFFFFFFF)
        {
            continue;
        }
        object* effect = World.object_from_handle(handle);
        if (effect != nullptr && effect->get_obj_type() == OT_EFFECT)
        {
            ((object_effect*)effect)->set_expression_variable(EFFECTS_VAR_PERCENT, percent);
        }
    }
}

// 0x00A52610 (:875)
static event_result power_core_stream_in_callback(const event_data& in_event, notify_info* info)
{
    if (in_event.get_type() != EVENT_MOVER_STREAM_IN)
    {
        return EVENT_IGNORED;
    }
    object_mover* obj = custom_object_event_mover(((const mover_stream_in_event&)in_event).handle);
    if (obj == nullptr)
    {
        return EVENT_IGNORED;
    }
    custom_object_stop_custom_effects(obj);
    custom_effects_start_all(obj);
    const level_object_inst_properties* props = obj->m_level_obj_properties;
    float percent = (float)(1.0 - (double)(float)props->m_hitpoints / (double)(float)props->m_base_properties->m_max_hitpoints);
    if (percent < 0.0f)
    {
        percent = 0.0f;
    }
    if (percent >= 1.0f)
    {
        percent = 1.0f;
    }
    power_core_set_effect_variables(obj, percent);
    return EVENT_OK;
}

// 0x00A2CF70 (:896)
// The damage percentage drives the effects; destroyed (1.0) drops the callback.
static event_result power_core_damaged_callback(const event_data& in_event, notify_info* info)
{
    if (in_event.get_type() != EVENT_BUILDING_DAMAGED)
    {
        return EVENT_IGNORED;
    }
    const building_damaged_event& damaged = (const building_damaged_event&)in_event;
    object_mover* obj = custom_object_event_mover(damaged.building);
    if (obj == nullptr)
    {
        return EVENT_IGNORED;
    }
    power_core_set_effect_variables(obj, damaged.percent);
    if (damaged.percent == 1.0f)
    {
        return EVENT_HANDLED;
    }
    return EVENT_OK;
}

// 0x00AD8FF0 (:920)
// A human touching the turning excavator blade is damaged (all its hitpoints, or Excavator_player_damage for the
// player), thrown as a ragdoll away from the hub (and along the spin while it lives), with grinder sparks.
static event_result excavator_blade_human_collision_callback(const event_data& in_event, notify_info* info)
{
    if (in_event.get_type() != EVENT_MOVER_HUMAN_SPECIAL_COLLISION)
    {
        return EVENT_IGNORED;
    }
    const mover_human_special_collision_event& collision = (const mover_human_special_collision_event&)in_event;
    if (info->data != (void*)collision.mover_handle || collision.human_handle == 0xFFFFFFFF)
    {
        return EVENT_IGNORED;
    }
    object* human_objp = World.object_from_handle(collision.human_handle);
    if (human_objp == nullptr || human_objp->get_obj_type() != OT_HUMAN || collision.mover_handle == 0xFFFFFFFF)
    {
        return EVENT_IGNORED;
    }
    human* hp = (human*)human_objp;
    object* blade = World.object_from_handle(collision.mover_handle);
    if (blade == nullptr || blade->get_obj_type() != OT_MOVER || blade->child_constraint_ptr == nullptr || !havok_constraint_has_motor(blade->child_constraint_ptr->hk_constraint))
    {
        return EVENT_IGNORED;
    }
    float damage = hp->hit_points;
    if (object_is_player(hp))
    {
        damage = Excavator_player_damage;
    }
    damage = human_collide_calculate_damage_with_timer(hp, damage);
    human_damage(hp, nullptr, damage, DBT_LEVEL_OBJECT, nullptr, nullptr, true, false);
    if (!human_ragdoll_start(hp, false, 0xFFFFFFFF))
    {
        return EVENT_OK;
    }
    vector center = hp->pos;
    hp->get_center_pos(center);
    const vector hub = blade->child_constraint_ptr->pos;
    vector spin;
    spin.zero();
    havok_body_get_angular_velocity(blade->havok_handle, spin);
    if (spin.is_null())
    {
        return EVENT_IGNORED;
    }
    spin.normalize_safe();
    vector radial(center.x - hub.x, center.y - hub.y, center.z - hub.z);
    radial.normalize_safe();
    if (damage > 0.0f)
    {
        // The cross product of the spin and the radial direction, each component in double (SSE2 code).
        const float tangent_x = (float)((double)spin.y * radial.z - (double)spin.z * radial.y);
        const float tangent_y = (float)((double)spin.z * radial.x - (double)radial.z * spin.x);
        const float tangent_z = (float)((double)radial.y * spin.x - (double)spin.y * radial.x);
        const vector vel(radial.x * Excavator_radial_damage_vel_scalar + tangent_x * Excavator_tangent_damage_vel_scalar, radial.y * Excavator_radial_damage_vel_scalar + tangent_y * Excavator_tangent_damage_vel_scalar,
                         radial.z * Excavator_radial_damage_vel_scalar + tangent_z * Excavator_tangent_damage_vel_scalar);
        havok_ragdoll_apply_velocity(hp->ragdoll_idx, vel, true);
        vector head = hp->pos;
        human_get_head_pos(hp, head);
        const vector push(radial.x * Excavator_radial_damage_vel_scalar, radial.y * Excavator_radial_damage_vel_scalar, radial.z * Excavator_radial_damage_vel_scalar);
        havok_ragdoll_apply_point_velocity(hp->ragdoll_idx, push, head);
    }
    else
    {
        const vector vel(radial.x * Excavator_radial_damage_vel_scalar, radial.y * Excavator_radial_damage_vel_scalar, radial.z * Excavator_radial_damage_vel_scalar);
        havok_ragdoll_apply_velocity(hp->ragdoll_idx, vel, true);
    }
    const weapon_info* w_info = weapons_get_weapon_info("grinder");
    if (w_info != nullptr)
    {
        const unsigned char material = hp->get_effect_material(0xFFFFFFFF, nullptr, nullptr);
        bullet_spark_create(center, radial, w_info, material, MATERIAL_EVENT_SHAPE_SOLID, nullptr, 0xFFFFFFFF, -1.0f);
    }
    return EVENT_OK;
}

// 0x00AD94A0 (:993)
// An excavator blade on its machine, with a motor and standing upright, collides with humans and is watched every frame
// (excavator_blade_do_frame_callback); its effects play at its hub on the machine's orientation, not attached.
static event_result excavator_blade_stream_in_callback(const event_data& in_event, notify_info* info)
{
    if (in_event.get_type() != EVENT_MOVER_STREAM_IN)
    {
        return EVENT_IGNORED;
    }
    object_mover* obj = custom_object_event_mover(((const mover_stream_in_event&)in_event).handle);
    if (obj == nullptr)
    {
        return EVENT_IGNORED;
    }
    notify_info collision_info;
    collision_info.type = EVENT_MOVER_HUMAN_SPECIAL_COLLISION;
    collision_info.callback = excavator_blade_human_collision_callback;
    collision_info.data = (void*)obj->get_handle();
    notify_info* event_handle = gem_register_for_event(collision_info);
    if (obj->get_parent() == 0xFFFFFFFF)
    {
        // Original: the registration leaks.
        return EVENT_IGNORED;
    }
    object* machine = World.object_from_handle(obj->get_parent());
    if (machine == nullptr || machine->get_obj_type() != OT_MOVER)
    {
        // Original: the registration leaks.
        return EVENT_IGNORED;
    }
    const constraint* cp = obj->child_constraint_ptr;
    // Magic number: 0.1 = the largest |rvec.y| of an upright blade. Original: a blade on its machine that fails these
    // tests keeps its registration.
    if (cp == nullptr || cp->child_constraint_ptr == nullptr || !(fabsf(obj->orient.rvec.y) < 0.1f))
    {
        return EVENT_OK;
    }
    if (processing_pool_add(obj->get_handle(), event_handle, excavator_blade_do_frame_callback))
    {
        level_object_inst_properties* props = obj->m_level_obj_properties;
        for (int i = 0; i < 3; i++)
        {
            const unsigned int effect_handle = props->m_base_properties->m_custom_effect_infos[i].effect_handle;
            if (effect_handle == 0xFFFFFFFF)
            {
                continue;
            }
            props->m_custom_effects[i] = effects_play(effect_handle, obj->child_constraint_ptr->pos, machine->orient, true, 0xFFFFFFFF, -1, 0xFFFFFFFF, 1.0f, false, EFFECTS_CATEGORY_NONE);
            object_effect* effect = World.object_from_handle<object_effect>(props->m_custom_effects[i]);
            if (effect != nullptr)
            {
                effect->obj_flags.destroy_on_stream = false;
            }
        }
        props->m_flags.m_generate_human_collision_event = true;
        return EVENT_OK;
    }
    gem_unregister_for_event(event_handle);
    return EVENT_OK;
}

// 0x00A526E0 (:1053)
// Destroying the excavator engine stops the machine's blade (motor to 0 with Excavator_stop_blade_force), its human
// collisions, its effects and its frame callback.
static event_result excavator_engine_destroyed_callback(const event_data& in_event, notify_info* info)
{
    if (in_event.get_type() != EVENT_BUILDING_DESTROYED)
    {
        return EVENT_IGNORED;
    }
    object_mover* engine = custom_object_event_mover(((const building_destroyed_event&)in_event).building);
    if (engine == nullptr)
    {
        return EVENT_IGNORED;
    }
    if (engine->get_parent() == 0xFFFFFFFF)
    {
        return EVENT_IGNORED;
    }
    object* machine = World.object_from_handle(engine->get_parent());
    if (machine == nullptr || machine->get_obj_type() != OT_MOVER)
    {
        return EVENT_IGNORED;
    }
    object_mover* blade = nullptr;
    object* child = machine->child_ptr;
    while (child != nullptr)
    {
        if (child->get_obj_type() == OT_MOVER && v_stricmp(((object_mover*)child)->m_level_obj_properties->m_base_properties->m_name, "0101machine_big_mining_a_blade") == 0)
        {
            blade = (object_mover*)child;
            break;
        }
        child = child->child_next;
        if (child == machine->child_ptr)
        {
            break;
        }
    }
    if (blade == nullptr)
    {
        return EVENT_HANDLED;
    }
    if (blade->child_constraint_ptr != nullptr && havok_constraint_has_motor(blade->child_constraint_ptr->hk_constraint))
    {
        havok_constraint_update_motor(blade->child_constraint_ptr->hk_constraint, 0.0f, Excavator_stop_blade_force);
    }
    blade->m_level_obj_properties->m_flags.m_generate_human_collision_event = false;
    custom_object_stop_custom_effects(blade);
    const int index = processing_pool_find(blade->get_handle());
    if (index != -1)
    {
        gem_unregister_for_event(Object_process_pool[index].event_handle);
        Object_process_pool.remove(index);
    }
    return EVENT_HANDLED;
}

// 0x00A52810 (:1106)
// An upright fire barrel burns (custom effects) until it tips over (fire_barrel_do_frame_callback).
// Original: the pool add is not checked, so the effects start even when the pool is full.
static event_result fire_barrel_stream_in_callback(const event_data& in_event, notify_info* info)
{
    if (in_event.get_type() != EVENT_MOVER_STREAM_IN)
    {
        return EVENT_IGNORED;
    }
    const unsigned int handle = ((const mover_stream_in_event&)in_event).handle;
    object_mover* obj = custom_object_event_mover(handle);
    if (obj == nullptr)
    {
        return EVENT_IGNORED;
    }
    // Magic number: 0.9 = the smallest uvec.y of an upright barrel.
    if (0.9f <= obj->orient.uvec.y)
    {
        custom_object_do_frame_callbacks entry;
        entry.object_handle = handle;
        entry.event_handle = nullptr;
        entry.frame_process_callback = fire_barrel_do_frame_callback;
        Object_process_pool.add(entry);
        custom_effects_start_all(obj);
    }
    return EVENT_OK;
}

// 0x00A528A0 (:1140)
// Original: the pool add is not checked, as for fire barrels.
static event_result kill_on_move_stream_in_callback(const event_data& in_event, notify_info* info)
{
    if (in_event.get_type() != EVENT_MOVER_STREAM_IN)
    {
        return EVENT_IGNORED;
    }
    const unsigned int handle = ((const mover_stream_in_event&)in_event).handle;
    object_mover* obj = custom_object_event_mover(handle);
    if (obj == nullptr)
    {
        return EVENT_IGNORED;
    }
    custom_object_do_frame_callbacks entry;
    entry.object_handle = handle;
    entry.event_handle = nullptr;
    entry.frame_process_callback = kill_on_move_do_frame_callback;
    Object_process_pool.add(entry);
    custom_effects_start_all(obj);
    return EVENT_OK;
}

// 0x00A2CFE0 (:1168)
static event_result kiosk_stream_in_callback(const event_data& in_event, notify_info* info)
{
    if (in_event.get_type() != EVENT_MOVER_STREAM_IN)
    {
        return EVENT_IGNORED;
    }
    object_mover* obj = custom_object_event_mover(((const mover_stream_in_event&)in_event).handle);
    if (obj == nullptr)
    {
        return EVENT_IGNORED;
    }
    kiosk_update_effects_and_action_nodes(obj);
    return EVENT_OK;
}

// 0x00A2D020 (:1185)
static event_result kiosk_damaged_callback(const event_data& in_event, notify_info* info)
{
    if (in_event.get_type() != EVENT_BUILDING_DAMAGED)
    {
        return EVENT_IGNORED;
    }
    object_mover* obj = custom_object_event_mover(((const building_damaged_event&)in_event).building);
    if (obj == nullptr)
    {
        return EVENT_IGNORED;
    }
    kiosk_update_effects_and_action_nodes(obj);
    return EVENT_OK;
}

// ---- external functions

// 0x00AD9670 (:1213)
// Picks the mover's callbacks by its properties name; any other mover restarts its effects on stream in and stops them
// when destroyed. A mover flagged without callbacks keeps none. Always true.
bool custom_object_set_callbacks(object_mover* obj)
{
    obj->on_stream_in_callback = nullptr;
    obj->on_damaged_callback = nullptr;
    obj->on_destroyed_callback = nullptr;
    obj->on_use_action_callback = nullptr;
    if ((obj->mover_flags & CUSTOM_OBJECT_MOVER_FLAG_NO_CALLBACKS) != 0)
    {
        return true;
    }
    const char* name = obj->m_level_obj_properties->m_base_properties->m_name;
    if (v_stricmp(name, "jk_widget") == 0)
    {
        // Original: the jk widget also gets the default stream in and destroy below.
        obj->on_damaged_callback = jk_widget_damaged_callback;
    }
    else
    {
        if (v_stricmp(name, "0101windmill_a_blades") == 0)
        {
            obj->on_destroyed_callback = nullptr;
            obj->on_stream_in_callback = windmill_blade_stream_in_callback;
            obj->on_damaged_callback = windmill_blade_damaged_callback;
            return true;
        }
        if (v_stricmp(name, "0101windmill_a") == 0)
        {
            obj->on_destroyed_callback = nullptr;
            obj->on_stream_in_callback = windmill_base_stream_in_callback;
            obj->on_damaged_callback = windmill_base_damaged_callback;
            return true;
        }
        if (v_stricmp(name, "miss_comp_adv_A") == 0)
        {
            obj->on_damaged_callback = nullptr;
            obj->on_stream_in_callback = computer_stream_in_callback;
            obj->on_destroyed_callback = computer_destroyed_callback;
            obj->on_use_action_callback = computer_use_action_callback;
            return true;
        }
        if (v_stricmp(name, "EDFPower_Core") == 0)
        {
            obj->on_stream_in_callback = power_core_stream_in_callback;
            obj->on_damaged_callback = power_core_damaged_callback;
            obj->on_destroyed_callback = object_mover_destroyed_callback;
            return true;
        }
        if (v_stricmp(name, "0101machine_big_mining_a_blade") == 0)
        {
            obj->on_stream_in_callback = excavator_blade_stream_in_callback;
            obj->on_destroyed_callback = object_mover_destroyed_callback;
            return true;
        }
        if (v_stricmp(name, "0101machine_big_mining_a_engine") == 0)
        {
            obj->on_stream_in_callback = object_mover_stream_in_callback;
            obj->on_destroyed_callback = excavator_engine_destroyed_callback;
            return true;
        }
        if (kiosk_mover_is_kiosk(obj))
        {
            obj->on_stream_in_callback = kiosk_stream_in_callback;
            obj->on_damaged_callback = kiosk_damaged_callback;
            return true;
        }
        if (v_stricmp(name, "tank_fire01_a") == 0)
        {
            obj->on_stream_in_callback = fire_barrel_stream_in_callback;
            obj->on_destroyed_callback = stop_effect_on_move_destroyed_callback;
            return true;
        }
        if (v_stricmp(name, "smokestack_a") == 0 || v_stricmp(name, "0102smokestack_a") == 0 || v_stricmp(name, "0105smokestack_a") == 0)
        {
            obj->on_stream_in_callback = stop_effect_on_destroyable_stream_in_callback;
            obj->on_damaged_callback = stop_effect_on_destroyable_damaged_callback;
            return true;
        }
        if (v_stricmp(name, "ind_vent_wall01") == 0 || v_stricmp(name, "res_vent_roof_A") == 0 || v_stricmp(name, "res_vent_wall_A") == 0 || v_stricmp(name, "EDF_antenna") == 0)
        {
            obj->on_stream_in_callback = stop_effect_on_move_stream_in_callback;
            obj->on_damaged_callback = stop_effect_on_destroyable_damaged_callback;
            obj->on_destroyed_callback = stop_effect_on_move_destroyed_callback;
            return true;
        }
        if (v_stricmp(name, "PA_speaker") == 0 || v_stricmp(name, "edf_portable_comm_a") == 0)
        {
            obj->on_damaged_callback = nullptr;
            obj->on_stream_in_callback = kill_on_move_stream_in_callback;
            obj->on_destroyed_callback = object_mover_destroyed_callback;
            return true;
        }
    }
    obj->on_stream_in_callback = object_mover_stream_in_callback;
    obj->on_destroyed_callback = object_mover_destroyed_callback;
    return true;
}

// 0x00A2D060 (:1341)
// Stops the mover's three custom effects (they finish emitting) and forgets them.
void custom_object_stop_custom_effects(object_mover* obj)
{
    for (int i = 0; i < 3; i++)
    {
        const unsigned int handle = obj->m_level_obj_properties->m_custom_effects[i];
        if (handle == 0xFFFFFFFF)
        {
            continue;
        }
        object* effect = World.object_from_handle(handle);
        if (effect != nullptr && effect->get_obj_type() == OT_EFFECT)
        {
            ((object_effect*)effect)->stop(object_effect::SS_STOP_EMITTING);
        }
        obj->m_level_obj_properties->m_custom_effects[i] = 0xFFFFFFFF;
    }
}

// 0x00A2D0C0 (:1359)
event_result custom_object_mover_stream_in_callback(const event_data& in_event, notify_info* info)
{
    if (in_event.get_type() != EVENT_MOVER_STREAM_IN)
    {
        return EVENT_IGNORED;
    }
    object_mover* obj = custom_object_event_mover(((const mover_stream_in_event&)in_event).handle);
    if (obj == nullptr)
    {
        return EVENT_IGNORED;
    }
    if (obj->on_stream_in_callback != nullptr && obj->on_stream_in_callback(in_event, info) == EVENT_HANDLED)
    {
        obj->on_stream_in_callback = nullptr;
    }
    return EVENT_OK;
}

// 0x00A2D120 (:1382)
event_result custom_object_mover_damaged_callback(const event_data& in_event, notify_info* info)
{
    if (in_event.get_type() != EVENT_BUILDING_DAMAGED)
    {
        return EVENT_IGNORED;
    }
    object_mover* obj = custom_object_event_mover(((const building_damaged_event&)in_event).building);
    if (obj == nullptr)
    {
        return EVENT_IGNORED;
    }
    if (obj->on_damaged_callback != nullptr && obj->on_damaged_callback(in_event, info) == EVENT_HANDLED)
    {
        obj->on_damaged_callback = nullptr;
    }
    return EVENT_OK;
}

// 0x00A2D180 (:1405)
event_result custom_object_mover_destroyed_callback(const event_data& in_event, notify_info* info)
{
    if (in_event.get_type() != EVENT_BUILDING_DESTROYED)
    {
        return EVENT_IGNORED;
    }
    object_mover* obj = custom_object_event_mover(((const building_destroyed_event&)in_event).building);
    if (obj == nullptr)
    {
        return EVENT_IGNORED;
    }
    if (obj->on_destroyed_callback != nullptr && obj->on_destroyed_callback(in_event, info) == EVENT_HANDLED)
    {
        obj->on_destroyed_callback = nullptr;
    }
    return EVENT_OK;
}

// 0x00A2D1E0 (:1428)
// The use event names the action node; its parent mover gets it.
event_result custom_object_use_action_callback(const event_data& in_event, notify_info* info)
{
    if (in_event.get_type() != EVENT_HUMAN_USING_ACTION_NODE)
    {
        return EVENT_IGNORED;
    }
    const unsigned int node_handle = ((const human_using_action_node_event&)in_event).action_node_handle;
    if (node_handle == 0xFFFFFFFF)
    {
        return EVENT_IGNORED;
    }
    object* node = World.object_from_handle(node_handle);
    if (node == nullptr || node->get_obj_type() != OT_ACTION_NODE || node->get_parent() == 0xFFFFFFFF)
    {
        return EVENT_IGNORED;
    }
    object_mover* obj = custom_object_event_mover(node->get_parent());
    if (obj == nullptr)
    {
        return EVENT_IGNORED;
    }
    if (obj->on_use_action_callback != nullptr && obj->on_use_action_callback(in_event, info) == EVENT_HANDLED)
    {
        obj->on_use_action_callback = nullptr;
    }
    return EVENT_OK;
}

// 0x00A05F40 (:1457): empty.
void custom_object_system_init()
{
}

// 0x00A05F50 (:1461): empty.
void custom_object_system_shutdown()
{
}

// 0x00A2D260 (:1465)
// Empties the processing pool and registers the four forwarding callbacks.
// Original: they are never unregistered (custom_object_level_shutdown is empty).
void custom_object_level_init_pre()
{
    Object_process_pool.remove_all();
    notify_info ni;
    ni.data = nullptr;
    ni.callback = custom_object_mover_stream_in_callback;
    ni.type = EVENT_MOVER_STREAM_IN;
    gem_register_for_event(ni);
    ni.callback = custom_object_mover_damaged_callback;
    ni.type = EVENT_BUILDING_DAMAGED;
    gem_register_for_event(ni);
    ni.callback = custom_object_mover_destroyed_callback;
    ni.type = EVENT_BUILDING_DESTROYED;
    gem_register_for_event(ni);
    ni.callback = custom_object_use_action_callback;
    ni.type = EVENT_HUMAN_USING_ACTION_NODE;
    gem_register_for_event(ni);
}

// 0x00A05F60 (:1487): empty.
void custom_object_level_shutdown()
{
}

// 0x00A2D2E0 (:1494)
// Runs each pooled object's frame callback; empty entries are removed.
// Original: the index moves on after any removal (here or inside a callback), so the next entry waits a frame.
void custom_object_do_frame()
{
    for (int i = 0; i < Object_process_pool.size(); i++)
    {
        void (*callback)(int) = Object_process_pool[i].frame_process_callback;
        if (Object_process_pool[i].object_handle == 0xFFFFFFFF || callback == nullptr)
        {
            Object_process_pool.remove(i);
        }
        else
        {
            callback(i);
        }
    }
}
