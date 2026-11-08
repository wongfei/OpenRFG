// Original: volition/rfg/code/havok/havok_collision_callbacks.cpp
//
// The game's collision responses (decompiled). havok_collision_callbacks_system_init sets up the collision events and
// fills their callback tables (file statics here, shared with havok_collision_events.cpp through the unity build):
// movers take impact damage and request collision effects, debris plays collision sounds, and the default pair
// callback applies the impact damage to both objects; items collide (projectile::collide, weapon::collide, collision
// damage). havok_collision_calculate_damage picks the contact point with the largest kinetic energy of the relative
// motion (from both bodies' masses and point velocities along the normal, or from the relative normal velocity when
// one body has no mass) and scales it by the Collision_damage_multiplier tweak.
// Vehicles (final_push.md step 3): impacts on a vehicle are queued as vehicle damage (havok_collide_vehicle_apply_damage:
// the collision thresholds, walker leaf components, the ramming share of the momentum); a vehicle hitting a mover is
// damaged by the impact of the contact with the most energy (havok_collide_vehicle_only); every vehicle collision plays
// the car's collision effects or, for a walker, finds its ground and takes fall damage (havok_collide_vehicle); the mass
// changers make a fast heavy vehicle push vehicles and movers aside and keep an attached projectile from pushing; the
// contact end callbacks stop scraping effects and keep a touched human's ragdoll active.
// Compiled in rfg_havok_unity.cpp.

#include "volition/rfg/code/havok/havok_collision_callbacks.h"
#include "volition/rfg/code/havok/havok_collision_events.h"
#include "volition/rfg/code/havok/havok_collision_immediate_callbacks.h"
#include "volition/rfg/code/havok/havok_ragdoll.h"

#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/dynamics/entity/hkprigidbody.h"
#include "volition/rfg/code/destruction/rfg_collisions.h"
#include "volition/rfg/code/gameplay/activities/activity_register_damage.h"
#include "volition/rfg/code/gamewide/tweak_table.h"
#include "volition/rfg/code/geom/vanim.h"
#include "volition/rfg/code/havok/havok.h"
#include "volition/rfg/code/level/world.h"
#include "volition/rfg/code/objects/humans/human.h"
#include "volition/rfg/code/objects/movers/object_mover.h"
#include "volition/rfg/code/objects/object.h"
#include "volition/rfg/code/objects/object_debris.h"
#include "volition/rfg/code/objects/object_item.h"
#include "volition/rfg/code/objects/projectile.h"
#include "volition/rfg/code/objects/turret.h"
#include "volition/rfg/code/vehicle/automobile.h"
#include "volition/rfg/code/vehicle/vehicle_class.h"
#include "volition/rfg/code/vehicle/vehicle_effects.h"
#include "volition/rfg/code/vehicle/walker.h"
#include "volition/rfg/code/weapons/weapons.h"
#include "volition/vlib/code/math/vector3.h"
#include "volition/vlib/code/os/cs_wrapper.h"

#include "keen/base/trace.hpp"

// 0x02B8CAC0: guards the contact lists and events of havok_collision_events.cpp (placed here, its first user in the
// unity TU).
static cs_wrapper Contact_event_wrapper(0);

// The walker toss strengths (initialized data, read only by havok_collide_vehicle).
static float Walker_large_toss_strength = 0.8f; // 0x0165FDE0
static float Walker_small_toss_strength = 0.6f; // 0x0165FDE4
static float Walker_small_toss_up = 0.4f; // 0x0165FDE8

// The ragdoll slot of havok_ragdoll.cpp (later in the unity TU) keeps simulating for ms milliseconds.
static void havok_ragdoll_keep_active(int ragdoll_idx, int ms);

static const int NUM_OBJECT_TYPE_PAIRS = NUM_OBJECT_TYPES * (NUM_OBJECT_TYPES + 1) / 2; // 1431

// ---- callback tables (havok_collision_events.cpp)
static contact_event_callback_info Contact_event_callback_table[NUM_OBJECT_TYPE_PAIRS]; // 0x0261F980
static collision_event_immediate_callback_info Collision_event_immediate_unary_callbacks[NUM_OBJECT_TYPES]; // 0x02622638
static collision_event_callback_info Collision_event_unary_callbacks[NUM_OBJECT_TYPES]; // 0x02642F98
static collision_event_mass_changer_cb_info Collision_event_mass_changer_callback_table[NUM_OBJECT_TYPE_PAIRS]; // 0x02663398
static collision_event_callback_info Collision_event_callback_table[NUM_OBJECT_TYPE_PAIRS]; // 0x0276D320

// ---- tweaks (values from tweak_table.xtbl)
static float Collision_damage_multiplier; // 0x0276E97C
static tweak_table_item Tweak_table_item_Collision_damage_multiplier("Collision_damage_multiplier", &Collision_damage_multiplier); // 0x02B83C98
// Read by havok_collide_vehicle_apply_damage; the values come from the tweak table.
static float Vehicle_min_ramming_damage_factor; // 0x02663070
static tweak_table_item Tweak_table_item_Vehicle_min_ramming_damage_factor("Vehicle_min_ramming_damage_factor", &Vehicle_min_ramming_damage_factor); // 0x02B952D8
static float Vehicle_max_ramming_damage_factor; // 0x02667814
static tweak_table_item Tweak_table_item_Vehicle_max_ramming_damage_factor("Vehicle_max_ramming_damage_factor", &Vehicle_max_ramming_damage_factor); // 0x02B952F0
static float Vehicle_collision_damage_threshold; // 0x0266605C
static tweak_table_item Tweak_table_item_Vehicle_collision_damage_threshold("Vehicle_collision_damage_threshold", &Vehicle_collision_damage_threshold); // 0x02B9523C
static float Vehicle_walker_collision_damage_threshold; // 0x02767818
static tweak_table_item Tweak_table_item_Vehicle_walker_collision_damage_threshold("Vehicle_walker_collision_damage_threshold", &Vehicle_walker_collision_damage_threshold); // 0x02B83C20
static float General_mover_collision_damage_multiplier; // 0x02642F80
static tweak_table_item Tweak_table_item_General_mover_collision_damage_multiplier("General_mover_collision_damage_multiplier", &General_mover_collision_damage_multiplier); // 0x02B95248
static float General_mover_collision_damage_cutoff; // 0x02622720
static tweak_table_item Tweak_table_item_General_mover_collision_damage_cutoff("General_mover_collision_damage_cutoff", &General_mover_collision_damage_cutoff); // 0x02B95254

// 0x00819D20 (:153)
// A mover (object_a) hit by a mover, zone, human or vehicle takes the scaled impact damage when it reaches the
// cutoff; rfg movers only where the hit shape has a script point effect. Vehicles do DBT_CAR damage, the others
// DBT_LEVEL_OBJECT.
static void havok_collide_mover_damage(const collision_event* c_event)
{
    object_mover* mover = (object_mover*)c_event->m_object_a;
    if (mover == nullptr || mover->get_obj_type() != OT_MOVER)
    {
        return;
    }
    if (mover->get_sub_type() == OT_SUB_MOVER_RFG && !mover->has_script_point_effect(c_event->m_collisions[0].m_shapekey_a, nullptr))
    {
        return;
    }
    const object_type other_type = c_event->m_object_b->get_obj_type();
    if (other_type != OT_MOVER && other_type != OT_ZONE && other_type != OT_HUMAN && other_type != OT_VEHICLE)
    {
        return;
    }
    vector position;
    vector normal;
    vector velocity;
    unsigned int best_collision;
    const float damage = General_mover_collision_damage_multiplier * havok_collision_calculate_damage(c_event, position, normal, velocity, best_collision);
    if (damage >= General_mover_collision_damage_cutoff)
    {
        mover->apply_damage(damage, other_type == OT_VEHICLE ? DBT_CAR : DBT_LEVEL_OBJECT, position, normal, c_event->m_object_b, c_event->m_collisions[0].m_shapekey_a, false);
    }
}

// The vehicle component index a body carries (Havok property 5, set by vehicle_component.cpp), or -1. rfg.exe inlines
// the property search at each use.
static unsigned int havok_collide_body_component_index(const hkpRigidBody* rb)
{
    if (rb == nullptr || !rb->hasProperty(5, hkpWorldObject::MULTI_THREADING_CHECKS_ENABLE))
    {
        return 0xFFFFFFFF;
    }
    return (unsigned int)(hkUlong)rb->getProperty(5, hkpWorldObject::MULTI_THREADING_CHECKS_ENABLE).m_data;
}

// 0x00819E10 (:181)
// The impact on a vehicle. The activity damage register sees every hit (a weight of 200). An impact under the
// collision threshold (under the walker threshold too when a walker hits) only counts when the attacker is a vehicle
// whose most recent driver is a player. A walker hit on a component without children takes a tenth of the damage; a
// walker hit by the zone outside its components takes none. The force is 0.75 % of the damage plus 0.5. The vehicle
// info scales the damage; a ramming vehicle (not a walker) scales it again by its share of the momentum along the hit.
// A walker attacker's damage is queued unscaled.
static void havok_collide_vehicle_apply_damage(vehicle* p_vehicle, object* attacker, const vector* position, const vector* normal, const vector* velocity, float damage, const collision_event* c_event)
{
    activity_register_damage(p_vehicle, attacker, 200.0f, (int)vehicle_get_hit_points(p_vehicle)); // Magic number: 200
    // Original: attacker is read without a null check here (it is checked below).
    if ((double)Vehicle_collision_damage_threshold > (double)damage &&
        (attacker->get_sub_type() != OT_SUB_VEHICLE_WALKER || (double)Vehicle_walker_collision_damage_threshold > (double)damage))
    {
        object* driver = nullptr;
        if (attacker->get_obj_type() == OT_VEHICLE)
        {
            const unsigned int driver_handle = ((vehicle*)attacker)->most_recent_driver;
            if (driver_handle != 0xFFFFFFFF)
            {
                driver = World.object_from_handle(driver_handle);
                if (driver != nullptr && driver->get_obj_type() != OT_HUMAN)
                {
                    driver = nullptr;
                }
            }
        }
        if (driver == nullptr || !object_is_player(driver))
        {
            return;
        }
    }
    if (p_vehicle->get_sub_type() == OT_SUB_VEHICLE_WALKER)
    {
        // The component the hit is on: the first collision with a component body (a's, else b's).
        unsigned int component = 0xFFFFFFFF;
        for (unsigned int i = 0; i < c_event->m_num_collisions; i++)
        {
            component = havok_collide_body_component_index(c_event->m_collisions[i].m_hk_rigid_body_a);
            if (component != 0xFFFFFFFF)
            {
                break;
            }
            component = havok_collide_body_component_index(c_event->m_collisions[i].m_hk_rigid_body_b);
            if (component != 0xFFFFFFFF)
            {
                break;
            }
        }
        // Original: the component count is read through rdd's data without a check that it is loaded.
        if (component < p_vehicle->rdd.get_data()->m_num_components)
        {
            if (((walker*)p_vehicle)->comp_info[component].has_children == 0)
            {
                damage = (float)((double)damage * (double)0.1f); // Magic number: leaf components take a tenth (a float constant)
            }
        }
        else if (attacker->get_obj_type() == OT_ZONE)
        {
            return;
        }
    }
    const float force = (float)((double)damage * (double)0.0075f + 0.5); // Magic numbers: 0.75 % of the damage (a float constant) plus 0.5
    bool ramming = false;
    damaged_by_types from_type = DBT_WORLD;
    if (attacker != nullptr)
    {
        switch (attacker->get_obj_type())
        {
        case OT_MOVER:
        case OT_TURRET:
            from_type = DBT_LEVEL_OBJECT;
            break;
        case OT_VEHICLE:
            from_type = DBT_CAR;
            if (attacker->get_sub_type() == OT_SUB_VEHICLE_WALKER)
            {
                vehicle_queue_damage(p_vehicle, attacker, nullptr, from_type, position, normal, force, (int)damage, velocity, false);
                return;
            }
            ramming = true;
            break;
        case OT_ZONE:
            break;
        default:
            keen::debug::nativePrintArgs("Int3 critical function at %s(%d)\n", __FILE__, __LINE__);
            from_type = DBT_NONE;
            break;
        }
    }
    damage = (float)((double)p_vehicle->info->m_collision_damage_scale * (double)damage);
    if (ramming)
    {
        damage = (float)((double)p_vehicle->info->m_vehicle_damage_scale * (double)damage);
        const vector dir = velocity->copy_normalize_safe();
        // Each body's momentum at the contact: its mass times its own point velocity.
        vector attacker_momentum;
        vector vehicle_momentum;
        const collision_event::collision_info& info = c_event->m_collisions[0];
        if (attacker == c_event->m_object_a)
        {
            const float attacker_mass = attacker->get_mass();
            attacker_momentum.set(attacker_mass * info.m_a_point_velocity.x, info.m_a_point_velocity.y * attacker_mass, attacker_mass * info.m_a_point_velocity.z);
            const float vehicle_mass = p_vehicle->get_mass();
            vehicle_momentum.set(vehicle_mass * info.m_b_point_velocity.x, info.m_b_point_velocity.y * vehicle_mass, vehicle_mass * info.m_b_point_velocity.z);
        }
        else
        {
            const float attacker_mass = attacker->get_mass();
            attacker_momentum.set(attacker_mass * info.m_b_point_velocity.x, info.m_b_point_velocity.y * attacker_mass, attacker_mass * info.m_b_point_velocity.z);
            const float vehicle_mass = p_vehicle->get_mass();
            vehicle_momentum.set(vehicle_mass * info.m_a_point_velocity.x, info.m_a_point_velocity.y * vehicle_mass, vehicle_mass * info.m_a_point_velocity.z);
        }
        const float attacker_along = fabsf((float)((double)dir.y * (double)attacker_momentum.y + (double)dir.x * (double)attacker_momentum.x + (double)dir.z * (double)attacker_momentum.z));
        const float vehicle_along = fabsf((float)((double)vehicle_momentum.y * (double)dir.y + (double)vehicle_momentum.x * (double)dir.x + (double)vehicle_momentum.z * (double)dir.z));
        if (attacker_along != 0.0f || vehicle_along != 0.0f)
        {
            const float share = (float)((double)attacker_along / ((double)vehicle_along + (double)attacker_along));
            damage = (float)(((double)share * (double)Vehicle_max_ramming_damage_factor + (double)Vehicle_min_ramming_damage_factor) * (double)damage);
        }
    }
    vehicle_queue_damage(p_vehicle, attacker, nullptr, from_type, position, normal, force, (int)damage, velocity, false);
}

// 0x0081A330 (:333)
// LTCG: obj in ECX, other_obj in EAX. The impact on obj: movers process it (humans and zones ignore it).
static void havok_collide_default_apply_damage(object* obj, object* other_obj, vector position, vector normal, vector velocity, float damage, unsigned int hit_shape_key, const collision_event* c_event)
{
    if (obj == nullptr)
    {
        return;
    }
    switch (obj->get_obj_type())
    {
    case OT_HUMAN:
    case OT_ZONE:
        break;
    case OT_MOVER:
        object_mover_process_collision((object_mover*)obj, other_obj, damage, position, normal, velocity, hit_shape_key);
        break;
    case OT_VEHICLE:
        havok_collide_vehicle_apply_damage((vehicle*)obj, other_obj, &position, &normal, &velocity, damage, c_event);
        break;
    case OT_TURRET:
        ((turret*)obj)->process_collision(other_obj, damage, velocity);
        break;
    default:
        keen::debug::nativePrintArgs("Int3 critical function at %s(%d)\n", __FILE__, __LINE__);
        break;
    }
}

// 0x0081A400 (:367)
// Impacts of at least 1 damage hit both objects (object_a with the velocity reversed).
static void havok_collide_default(const collision_event* c_event)
{
    vector position;
    vector normal;
    vector velocity;
    unsigned int best_collision;
    const float damage = havok_collision_calculate_damage(c_event, position, normal, velocity, best_collision);
    if (damage < 1.0f)
    {
        return;
    }
    const vector reversed_velocity(-velocity.x, -velocity.y, -velocity.z);
    havok_collide_default_apply_damage(c_event->m_object_a, c_event->m_object_b, position, normal, reversed_velocity, damage,
                                       c_event->m_collisions[best_collision].m_shapekey_a, c_event);
    havok_collide_default_apply_damage(c_event->m_object_b, c_event->m_object_a, position, normal, velocity, damage,
                                       c_event->m_collisions[best_collision].m_shapekey_b, c_event);
}

// 0x0081A570 (:389)
// A mover and a vehicle collide: the contact point with the most impact energy (as havok_collision_calculate_damage's
// two-body case, a body without Havok mass taking its object's mass, capped by the other body's) damages the vehicle
// alone, scaled by Collision_damage_multiplier, when it reaches 1. The vehicle as object_a gets the relative velocity
// reversed.
static void havok_collide_vehicle_only(const collision_event* c_event)
{
    unsigned int best = 0;
    float best_energy = 0.0f;
    for (unsigned int i = 0; i < c_event->m_num_collisions; i++)
    {
        const collision_event::collision_info& info = c_event->m_collisions[i];
        float mass_a = info.m_hk_rigid_body_a->m_motion.getMass();
        float mass_b = info.m_hk_rigid_body_b->m_motion.getMass();
        // Magic number: 0.001, a mass below it is none (fixed or keyframed bodies).
        const float ta = (float)((double)mass_a - 0.0);
        if (0.001f > ta && 0.001f > -ta)
        {
            mass_a = c_event->m_object_a->get_mass();
            if (mass_b > mass_a)
            {
                mass_b = mass_a;
            }
        }
        else
        {
            const float tb = (float)((double)mass_b - 0.0);
            if (0.001f > tb && 0.001f > -tb)
            {
                mass_b = c_event->m_object_b->get_mass();
                if (mass_a > mass_b)
                {
                    mass_a = mass_b;
                }
            }
        }
        const vector& n = info.m_collision_normal;
        const vector& va = info.m_a_point_velocity;
        const vector& vb = info.m_b_point_velocity;
        // Each body's point velocity along the normal, in the direction of that velocity.
        const float speed_a = (float)((double)va.y * (double)n.y + (double)n.x * (double)va.x + (double)n.z * (double)va.z);
        const vector dir_a = va.copy_normalize_safe();
        const float ax = (float)((double)dir_a.x * (double)speed_a);
        const float ay = (float)((double)dir_a.y * (double)speed_a);
        const float az = (float)((double)dir_a.z * (double)speed_a);
        const float speed_b = (float)((double)vb.y * (double)n.y + (double)n.x * (double)vb.x + (double)vb.z * (double)n.z);
        const vector dir_b = vb.copy_normalize_safe();
        const float bx = (float)((double)speed_b * (double)dir_b.x);
        const float by = (float)((double)dir_b.y * (double)speed_b);
        const float bz = (float)((double)dir_b.z * (double)speed_b);
        // Relative to the velocity of their common center of mass.
        const float sum_x = (float)((double)(float)((double)ax * (double)mass_a) + (double)(float)((double)bx * (double)mass_b));
        const float sum_y = (float)((double)(float)((double)ay * (double)mass_a) + (double)(float)((double)by * (double)mass_b));
        const float sum_z = (float)((double)(float)((double)az * (double)mass_a) + (double)(float)((double)bz * (double)mass_b));
        const float total = (float)((double)mass_b + (double)mass_a);
        const float cx = (float)((double)sum_x / (double)total);
        const float cy = (float)((double)sum_y / (double)total);
        const float cz = (float)((double)sum_z / (double)total);
        const float rax = (float)((double)ax - (double)cx);
        const float ray = (float)((double)ay - (double)cy);
        const float raz = (float)((double)az - (double)cz);
        const float rbx = (float)((double)bx - (double)cx);
        const float rby = (float)((double)by - (double)cy);
        const float rbz = (float)((double)bz - (double)cz);
        const float sq_b = (float)((double)rby * (double)rby + (double)rbx * (double)rbx + (double)rbz * (double)rbz);
        const float sq_a = (float)((double)ray * (double)ray + (double)rax * (double)rax + (double)raz * (double)raz);
        const float energy = (float)((double)sq_b * (double)mass_b + (double)sq_a * (double)mass_a);
        if ((double)energy > (double)best_energy)
        {
            best_energy = energy;
            best = i;
        }
    }
    const collision_event::collision_info& info = c_event->m_collisions[best];
    const float damage = (float)((double)Collision_damage_multiplier * (double)best_energy);
    if (1.0f > damage)
    {
        return;
    }
    if (c_event->m_object_a->get_obj_type() == OT_VEHICLE)
    {
        const vector reversed_velocity(-info.m_relative_point_velocity.x, -info.m_relative_point_velocity.y, -info.m_relative_point_velocity.z);
        havok_collide_default_apply_damage(c_event->m_object_a, c_event->m_object_b, info.m_collision_point, info.m_collision_normal, reversed_velocity, damage, info.m_shapekey_a, c_event);
        return;
    }
    havok_collide_default_apply_damage(c_event->m_object_b, c_event->m_object_a, info.m_collision_point, info.m_collision_normal, info.m_relative_point_velocity, damage, info.m_shapekey_b, c_event);
}

// 0x0081AC50 (:471)
// A contact of a human (object_a, else object_b) with a mover or vehicle ends: its ragdoll keeps simulating 30 s more.
static void havok_collide_human_contact_handler(const contact_event* c_event)
{
    const object* objp = c_event->m_object_a->get_obj_type() == OT_HUMAN ? c_event->m_object_a : c_event->m_object_b;
    havok_ragdoll_keep_active(((const human*)objp)->ragdoll_idx, 30000); // Magic number: 30 s
}

// 0x0081AC90 (:487)
// A car (object_a) leaves a contact: the scraping effect against that object stops, or every scraping effect when the
// other object is gone and the car touches nothing.
static void havok_collide_vehicle_contact_handler(const contact_event* c_event)
{
    automobile* ap = (automobile*)c_event->m_object_a;
    if (ap->get_obj_type() != OT_VEHICLE || ap->get_sub_type() != OT_SUB_VEHICLE_AUTO || c_event->m_type != contact_event::CONTACT_ENDED)
    {
        return;
    }
    if (c_event->m_object_b != nullptr)
    {
        automobile_stop_scraping_effect(ap, c_event->m_object_b);
        return;
    }
    if (!havok_collision_events_object_in_contact(ap))
    {
        automobile_stop_all_scraping_effects(ap);
    }
}

// 0x0080F460 (:527)
// The debris (object_a) plays its collision sound at the contact point with the largest normal velocity.
void havok_collide_debris(const collision_event* c_event)
{
    const collision_event::collision_info* best = &c_event->m_collisions[0];
    float speed = best->m_relative_normal_velocity;
    for (unsigned int i = 1; i < c_event->m_num_collisions; i++)
    {
        if (speed < c_event->m_collisions[i].m_relative_normal_velocity)
        {
            best = &c_event->m_collisions[i];
            speed = best->m_relative_normal_velocity;
        }
    }
    object_debris_play_collision_sound((object_debris*)c_event->m_object_a, &best->m_collision_point, best->m_relative_normal_velocity);
}

// 0x0082A560 (:558)
// A vehicle (object_a) collides with a mover, vehicle or the zone (or nothing). A vehicle corpse hit by a walker counts
// the hits and fades out after 300. A car plays its collision effects at each contact point (the speed into the
// surface, doubled when it pushes downward; the speed along it; scraping sounds when it is in contact); a large or small
// walker stomping it (its stomp action playing, the car not of infinite mass) tosses it away and, for the small walker
// during frames 4-14 of its other stomp action, kicks it up. A walker takes the floor it stands on (the zone contact
// with the most upward normal above 0.65, else another object's above 0.3, whose normal counts only above 0.8) and
// fall damage from its downward speed past 23.
void havok_collide_vehicle(const collision_event* c_event)
{
    vehicle* vp = (vehicle*)c_event->m_object_a;
    if (c_event->m_object_b != nullptr)
    {
        const object_type other_type = c_event->m_object_b->get_obj_type();
        if (other_type != OT_MOVER && other_type != OT_VEHICLE && other_type != OT_ZONE)
        {
            return;
        }
        if (vehicle_is_corpse(vp) && other_type == OT_VEHICLE)
        {
            if (c_event->m_object_b->get_sub_type() != OT_SUB_VEHICLE_WALKER)
            {
                return;
            }
            vp->walker_vehicle_collisions++;
            if (vp->walker_vehicle_collisions > 300) // Magic number: 300 walker hits
            {
                vehicle_fade_out(vp, -1, true, false);
            }
            return;
        }
    }
    Contact_event_wrapper.enter();
    const object_contact_info* contacts = havok_collision_events_get_contact_info(c_event->m_object_a); // inlined
    const bool in_contact = contacts->m_contact_list != nullptr;
    Contact_event_wrapper.leave();
    const object_sub_type sub_type = vp->get_sub_type();
    if (sub_type == OT_SUB_VEHICLE_AUTO)
    {
        automobile* ap = (automobile*)vp;
        for (unsigned int i = 0; i < c_event->m_num_collisions; i++)
        {
            const collision_event::collision_info& info = c_event->m_collisions[i];
            const unsigned int component = havok_collide_body_component_index(info.m_hk_rigid_body_a);
            const vector& n = info.m_collision_normal;
            const float rnv = info.m_relative_normal_velocity;
            const float nvx = (float)((double)rnv * (double)n.x);
            float nvy = (float)((double)n.y * (double)rnv);
            const float nvz = (float)((double)rnv * (double)n.z);
            if (!(0.0f < nvy)) // as rfg.exe's COMISS/JC: a downward (or zero) push counts double
            {
                nvy = (float)((double)nvy * 2.0); // Magic number: 2
            }
            const float speed_normal = (float)sqrt((double)(float)((double)nvx * (double)nvx + (double)nvy * (double)nvy + (double)nvz * (double)nvz));
            const vector& rv = info.m_relative_point_velocity;
            const float tx = (float)((double)rv.x - (double)(float)((double)rnv * (double)n.x));
            const float ty = (float)((double)rv.y - (double)(float)((double)n.y * (double)rnv));
            const float tz = (float)((double)rv.z - (double)(float)((double)rnv * (double)n.z));
            const float speed_surface = (float)sqrt((double)(float)((double)tx * (double)tx + (double)ty * (double)ty + (double)tz * (double)tz));
            object* other = c_event->m_object_b;
            if (other != nullptr && other->get_sub_type() == OT_SUB_VEHICLE_WALKER)
            {
                walker* wp = (walker*)other;
                if ((wp->model_type == 2 || wp->model_type == 0) && !ap->get_infinite_mass() && vanim_is_playing_action(wp->vp, 0x3DC)) // Magic numbers: model types 2 (large) and 0 (small), action alias 0x3DC
                {
                    float toss = wp->model_type == 2 ? Walker_large_toss_strength : Walker_small_toss_strength;
                    const float mass = havok_body_get_mass(ap->havok_handle);
                    // Magic numbers: cars heavier than 2000 kg are tossed less, by 1 per 1000 kg more.
                    if ((double)mass > 2000.0)
                    {
                        toss = (float)((double)toss / (((double)mass - 2000.0) / 1000.0 + 1.0));
                    }
                    if (wp->model_type == 0 || vanim_is_playing_action(wp->vp, 0x216)) // Magic number: action alias 0x216
                    {
                        vector toss_velocity((float)((double)n.x * (double)toss), (float)((double)n.y * (double)toss), (float)((double)n.z * (double)toss));
                        if ((float)(0.0 - (double)toss_velocity.y) >= 0.0f)
                        {
                            toss_velocity.y = 0.0f;
                        }
                        havok_body_apply_point_velocity(ap->havok_handle, toss_velocity, info.m_collision_point);
                    }
                    if (wp->model_type == 0 && vanim_is_playing_action(wp->vp, 0x216) && (unsigned int)(vanim_get_anim_frame(wp->vp, 0x216) - 4) <= 10) // Magic numbers: frames 4-14
                    {
                        vector away((float)((double)wp->pos.x - (double)ap->pos.x), 0.0f, (float)((double)wp->pos.z - (double)ap->pos.z));
                        away.normalize_safe();
                        // Magic number: 3 m from the car toward the walker.
                        const vector point((float)((double)(float)((double)away.x * 3.0) + (double)ap->pos.x), (float)((double)ap->pos.y + (double)(float)((double)away.y * 3.0)),
                                           (float)((double)ap->pos.z + (double)(float)((double)away.z * 3.0)));
                        // Magic number: 2000, the impulse of the toss up per unit.
                        vector impulse((float)((double)(float)((double)UP_VECTOR.x * (double)Walker_small_toss_up) * 2000.0),
                                       (float)((double)(float)((double)UP_VECTOR.y * (double)Walker_small_toss_up) * 2000.0),
                                       (float)((double)(float)((double)UP_VECTOR.z * (double)Walker_small_toss_up) * 2000.0));
                        if ((double)mass > 2000.0)
                        {
                            const float scale = (float)(1.0 / (double)(float)(((double)mass - 2000.0) / 1000.0 + 1.0));
                            impulse.x = (float)((double)impulse.x * (double)scale);
                            impulse.y = (float)((double)impulse.y * (double)scale);
                            impulse.z = (float)((double)impulse.z * (double)scale);
                        }
                        havok_body_apply_point_impulse(ap->havok_handle, impulse, point);
                    }
                }
            }
            automobile_play_collision_effects(ap, component, c_event->m_object_b, &info.m_collision_point, &info.m_collision_normal, info.m_hk_rigid_body_b, speed_normal, speed_surface, in_contact);
        }
        return;
    }
    if (sub_type != OT_SUB_VEHICLE_WALKER)
    {
        return;
    }
    walker* wp = (walker*)vp;
    bool first = true;
    bool found_zone = false;
    bool found_other = false;
    unsigned int zone_handle = 0xFFFFFFFF;
    unsigned int other_handle = 0xFFFFFFFF;
    vector zone_normal;
    vector zone_point;
    vector other_normal;
    vector other_point;
    for (unsigned int i = 0; i < c_event->m_num_collisions; i++)
    {
        const collision_event::collision_info& info = c_event->m_collisions[i];
        // A contact of one of the walker's components counts only when it is the event's only contact.
        if (havok_collide_body_component_index(info.m_hk_rigid_body_a) != 0xFFFFFFFF && c_event->m_num_collisions != 1)
        {
            continue;
        }
        const float ny = info.m_collision_normal.y;
        if (!(ny > 0.0f))
        {
            continue;
        }
        if (first)
        {
            first = false;
            zone_normal.y = -1.0f;
            other_normal.y = -1.0f;
        }
        const object* other = c_event->m_object_b;
        if (other->get_obj_type() == OT_ZONE)
        {
            zone_handle = other->get_handle();
            if ((double)ny > (double)zone_normal.y && ny > 0.65f) // Magic number: 0.65, the floor's least upward normal
            {
                zone_normal = info.m_collision_normal;
                zone_point = info.m_collision_point;
                found_zone = true;
            }
        }
        else
        {
            other_handle = other->get_handle();
            if ((double)ny > (double)other_normal.y && ny > 0.3f) // Magic number: 0.3
            {
                other_normal = info.m_collision_normal;
                other_point = info.m_collision_point;
                found_other = true;
            }
        }
        // Fall damage: the walker's least vertical speed so far; past 23 down, 10 times the square of the excess.
        const float fall_speed = wp->actual_velocity.y;
        if ((double)wp->ground_impact_momentum > (double)fall_speed)
        {
            wp->ground_impact_momentum = fall_speed;
        }
        float excess = (float)((double)fall_speed + 23.0); // Magic number: 23
        if ((float)((double)excess - 0.0) >= 0.0f)
        {
            excess = 0.0f;
        }
        float fall_damage = (float)((double)excess * (double)excess * 10.0); // Magic number: 10
        if (!((float)((double)fall_damage - 0.0) >= 0.0f))
        {
            fall_damage = 0.0f;
        }
        const int damage_to_apply = (int)fall_damage;
        if (damage_to_apply > 0)
        {
            wp->actual_velocity.y = 0.0f;
            vehicle_apply_damage(wp, nullptr, nullptr, DBT_NONE, &wp->pos, &info.m_collision_normal, 20.0f, damage_to_apply, nullptr, false); // Magic number: radius 20
        }
    }
    if (first)
    {
        return;
    }
    if (found_zone)
    {
        wp->ground_normal = zone_normal;
        wp->ground_world_pos = zone_point;
        wp->ground_obj_handle = zone_handle;
        return;
    }
    if (!found_other)
    {
        return;
    }
    if ((double)other_normal.y > (double)0.8f) // Magic number: 0.8 (a float constant), else the floor counts as level
    {
        wp->ground_normal = other_normal;
    }
    else
    {
        wp->ground_normal.set(0.0f, 1.0f, 0.0f);
    }
    wp->ground_world_pos = other_point;
    wp->ground_obj_handle = other_handle;
}

// 0x0081ACE0 (:766)
void havok_collide_weapon(const collision_event* c_event)
{
    object* objp = c_event->m_object_a;
    weapon* wp = objp != nullptr && objp->get_obj_type() == OT_ITEM && objp->get_sub_type() == OT_SUB_ITEM_WEAPON ? (weapon*)objp : nullptr;
    // Original: called on a null weapon too.
    wp->collide(c_event, c_event->m_object_b);
}

// 0x0081AD10 (:774): an item damaged by collisions (object_a) takes the impact damage of at least 1, scaled.
static void havok_item_apply_collision_damage(const collision_event* c_event)
{
    object* objp = c_event->m_object_a;
    item* ip = objp != nullptr && objp->get_obj_type() == OT_ITEM ? (item*)objp : nullptr;
    vector position;
    vector normal;
    vector velocity;
    unsigned int best_collision;
    const float damage = havok_collision_calculate_damage(c_event, position, normal, velocity, best_collision);
    if (damage >= 1.0f)
    {
        // Magic number: temp_item_collision_damage_multiplier (25, 0x016607B4, a constant in rfg.exe).
        ip->apply_damage(25.0f * damage, DBT_NONE, position, normal, nullptr, 0xFFFFFFFF, false);
    }
}

// 0x0081ADA0 (:797): a projectile (object_a) collides (projectile::collide); other items take collision damage, a
// turret's item hit by a vehicle passes the impact to the turret, weapons collide (weapon::collide) and the
// collision effect is requested.
void havok_collide_item(const collision_event* c_event)
{
    object* objp = c_event->m_object_a;
    if (objp->get_sub_type() == OT_SUB_ITEM_PROJECTILE)
    {
        projectile* proj = objp != nullptr && objp->get_obj_type() == OT_ITEM ? (projectile*)objp : nullptr;
        proj->collide(c_event, c_event->m_object_b);
        return;
    }
    if (((item*)objp)->damage_info.flags.damaged_by_collisions)
    {
        havok_item_apply_collision_damage(c_event);
    }
    // A turret's item hit by a vehicle passes an impact above 1 to the turret.
    turret* tp = turret_from_item((item*)objp);
    if (tp != nullptr && c_event->m_object_b->get_obj_type() == OT_VEHICLE)
    {
        vector position;
        vector normal;
        vector velocity;
        unsigned int best_collision;
        const float damage = havok_collision_calculate_damage(c_event, position, normal, velocity, best_collision);
        if (damage > 1.0f)
        {
            tp->process_collision(c_event->m_object_b, damage, velocity);
        }
    }
    if (objp->get_sub_type() == OT_SUB_ITEM_WEAPON)
    {
        ((weapon*)objp)->collide(c_event, c_event->m_object_b);
    }
    rfg_effect_candidate_collide(c_event);
}

// 0x0081AE90 (:838)
void havok_collide_mover(const collision_event* c_event)
{
    havok_collide_mover_damage(c_event);
    rfg_effect_candidate_collide(c_event);
}

// The inverse mass factor of a vehicle in a mass changer: none for infinite mass, the inverse of the extra mass, else 1
// (inlined in rfg.exe).
static float havok_collide_vehicle_inv_mass_factor(const vehicle* vp)
{
    if (vp->get_infinite_mass())
    {
        return 0.0f;
    }
    if (vp->get_extra_mass())
    {
        return (float)(1.0 / (double)vp->get_extra_mass_value());
    }
    return 1.0f;
}

// 0x0081AEB0 (:861)
// Two vehicles with different inverse mass factors (infinite or extra mass) collide with those factors, each clamped
// to [0, 1000].
static bool havok_collide_vehicle_vehicle_mass_changer(collision_mass_changer_output* output, const collision_mass_changer_event* c_event, void* user_data)
{
    object* objp_a = c_event->m_h_object_a != 0xFFFFFFFF ? World.object_from_handle(c_event->m_h_object_a) : nullptr;
    const vehicle* vp_a = objp_a != nullptr && objp_a->get_obj_type() == OT_VEHICLE ? (const vehicle*)objp_a : nullptr;
    object* objp_b = c_event->m_h_object_b != 0xFFFFFFFF ? World.object_from_handle(c_event->m_h_object_b) : nullptr;
    const vehicle* vp_b = objp_b != nullptr && objp_b->get_obj_type() == OT_VEHICLE ? (const vehicle*)objp_b : nullptr;
    // Original: both vehicles are read without a null check.
    float factor_a = havok_collide_vehicle_inv_mass_factor(vp_a);
    float factor_b = havok_collide_vehicle_inv_mass_factor(vp_b);
    if (factor_a == factor_b)
    {
        return false;
    }
    // Magic numbers: the factors are clamped to [0, 1000] (as rfg.exe's float compares: a NaN becomes 0).
    if (!(factor_a >= 0.0f))
    {
        factor_a = 0.0f;
    }
    if (factor_a >= 1000.0f)
    {
        factor_a = 1000.0f;
    }
    output->m_inv_mass_factor_a = factor_a;
    if (!(factor_b >= 0.0f))
    {
        factor_b = 0.0f;
    }
    if (factor_b >= 1000.0f)
    {
        factor_b = 1000.0f;
    }
    output->m_inv_mass_factor_b = factor_b;
    return true;
}

// 0x0081B000 (:941)
// A vehicle of infinite or extra mass hitting a mover that is not fixed or keyframed at 10 mph (4.4704 m/s) or more:
// the mover takes the full push and the vehicle a fifth (the inverse of its extra mass when it has one).
static bool havok_collide_vehicle_mover_mass_changer(collision_mass_changer_output* output, const collision_mass_changer_event* c_event, void* user_data)
{
    object* objp_a = World.object_from_handle(c_event->m_h_object_a);
    object* objp_b = World.object_from_handle(c_event->m_h_object_b);
    // Original: the objects are read without a null check.
    const vehicle* vp;
    const object* other;
    float* vehicle_factor;
    float* other_factor;
    if (objp_a->get_obj_type() == OT_VEHICLE)
    {
        vp = (const vehicle*)objp_a;
        other = objp_b;
        vehicle_factor = &output->m_inv_mass_factor_a;
        other_factor = &output->m_inv_mass_factor_b;
    }
    else
    {
        vp = (const vehicle*)objp_b;
        other = objp_a;
        vehicle_factor = &output->m_inv_mass_factor_b;
        other_factor = &output->m_inv_mass_factor_a;
    }
    if (!vp->get_infinite_mass() && !vp->get_extra_mass())
    {
        return false;
    }
    const unsigned int handle = other->havok_handle;
    if (handle != 0xFFFFFFFF && Hk_rigid_bodies[handle] != nullptr)
    {
        const unsigned char motion_type = Hk_rigid_bodies[handle]->m_motion.m_type.m_storage;
        if (motion_type == hkpMotion::MOTION_FIXED || motion_type == hkpMotion::MOTION_KEYFRAMED)
        {
            return false;
        }
    }
    vector velocity;
    vehicle_get_velocity(vp, velocity);
    const float speed = (float)sqrt((double)(float)((double)velocity.y * (double)velocity.y + (double)velocity.x * (double)velocity.x + (double)velocity.z * (double)velocity.z));
    if (4.4704f > speed) // Magic number: 10 mph in m/s
    {
        return false;
    }
    *vehicle_factor = 0.2f; // Magic number: a fifth
    *other_factor = 1.0f;
    if (vp->get_extra_mass())
    {
        *vehicle_factor = (float)(1.0 / (double)vp->get_extra_mass_value());
    }
    return true;
}

// 0x0081B130 (:1010)
// An attached projectile ("attach on impact", flag 2) touching a living object does not push it: the projectile takes
// the whole push.
static bool havok_collide_projectile_mass_changer(collision_mass_changer_output* output, const collision_mass_changer_event* c_event, void* user_data)
{
    object* objp_a = World.object_from_handle(c_event->m_h_object_a);
    object* objp_b = World.object_from_handle(c_event->m_h_object_b);
    // Original: the objects' sub types are read without a null check.
    const projectile* proj;
    object* other;
    float* projectile_factor;
    float* other_factor;
    if (objp_a->get_sub_type() == OT_SUB_ITEM_PROJECTILE)
    {
        proj = objp_a->get_obj_type() == OT_ITEM ? (const projectile*)objp_a : nullptr;
        other = objp_b;
        projectile_factor = &output->m_inv_mass_factor_a;
        other_factor = &output->m_inv_mass_factor_b;
    }
    else
    {
        if (objp_b->get_sub_type() != OT_SUB_ITEM_PROJECTILE)
        {
            return false;
        }
        proj = objp_b->get_obj_type() == OT_ITEM ? (const projectile*)objp_b : nullptr;
        other = objp_a;
        projectile_factor = &output->m_inv_mass_factor_b;
        other_factor = &output->m_inv_mass_factor_a;
    }
    if (proj == nullptr || (proj->w_info->projectile_info.flags & 2) == 0 || other == nullptr || other->is_killed())
    {
        return false;
    }
    *projectile_factor = 1.0f;
    *other_factor = 0.0f;
    return true;
}

// 0x0082AEC0 (:1056)
// The registrations of the original. Pair slots are havok_get_table_index(a, b) = high * (high + 1) / 2 + low.
void havok_collision_callbacks_system_init()
{
    havok_collision_events_system_init();
    Contact_event_callback_table[193].m_contact_end_callback = havok_collide_vehicle_contact_handler; // vehicle/zone
    Contact_event_callback_table[8].m_contact_end_callback = havok_collide_vehicle_contact_handler; // mover/vehicle
    Contact_event_callback_table[9].m_contact_end_callback = havok_collide_vehicle_contact_handler; // vehicle/vehicle
    Collision_event_mass_changer_callback_table[9].m_user_data = nullptr;
    Collision_event_mass_changer_callback_table[8].m_user_data = nullptr;
    Collision_event_mass_changer_callback_table[22].m_user_data = nullptr;
    Collision_event_mass_changer_callback_table[7].m_user_data = nullptr;
    Collision_event_unary_callbacks[OT_MOVER].m_callback = havok_collide_mover;
    Collision_event_unary_callbacks[OT_ITEM].m_callback = havok_collide_item;
    Collision_event_callback_table[193].m_callback = havok_collide_default; // vehicle/zone
    Collision_event_callback_table[9].m_callback = havok_collide_default; // vehicle/vehicle
    Collision_event_callback_table[8].m_callback = havok_collide_vehicle_only; // mover/vehicle
    Collision_event_unary_callbacks[OT_VEHICLE].m_callback = havok_collide_vehicle;
    Collision_event_mass_changer_callback_table[9].m_callback = havok_collide_vehicle_vehicle_mass_changer; // vehicle/vehicle
    Collision_event_mass_changer_callback_table[8].m_callback = havok_collide_vehicle_mover_mass_changer; // mover/vehicle
    Collision_event_mass_changer_callback_table[22].m_callback = havok_collide_projectile_mass_changer; // item/turret
    Collision_event_mass_changer_callback_table[7].m_callback = havok_collide_projectile_mass_changer; // item/vehicle
    Collision_event_unary_callbacks[OT_HUMAN].m_callback = havok_collide_ragdoll; // (ragdoll bodies; MP7)
    Collision_event_callback_table[3].m_callback = havok_collide_default; // human/mover
    Contact_event_callback_table[6].m_contact_end_callback = havok_collide_human_contact_handler; // human/vehicle
    Contact_event_callback_table[3].m_contact_end_callback = havok_collide_human_contact_handler; // human/mover
    Collision_event_unary_callbacks[OT_DEBRIS].m_callback = havok_collide_debris;
    Collision_event_callback_table[196].m_callback = havok_collide_default; // turret/zone
    Collision_event_callback_table[24].m_callback = havok_collide_default; // vehicle/turret
    Collision_event_callback_table[23].m_callback = havok_collide_default; // mover/turret
    havok_collision_immediate_callbacks_system_init(); // inlined
}

// 0x0081B1D0 (:1109)
void havok_collision_callbacks_system_shutdown()
{
    havok_collision_events_system_shutdown(); // inlined
}

// 0x0080F570 (:1118)
// The contact point with the largest impact energy (see the file comment); its position, normal and relative
// velocity are returned with the energy times Collision_damage_multiplier.
float havok_collision_calculate_damage(const collision_event* c_event, vector& position, vector& normal, vector& velocity, unsigned int& best_collision)
{
    best_collision = 0;
    float best_energy = 0.0f;
    for (unsigned int i = 0; i < c_event->m_num_collisions; i++)
    {
        const collision_event::collision_info& info = c_event->m_collisions[i];
        const float mass_a = info.m_hk_rigid_body_a->m_motion.getMass();
        const float mass_b = info.m_hk_rigid_body_b->m_motion.getMass();
        const vector& n = info.m_collision_normal;
        float energy;
        if (!(mass_a - 0.0f >= 0.001f || -(mass_a - 0.0f) >= 0.001f)) // a has no mass (fixed or keyframed)
        {
            const float v = info.m_relative_normal_velocity;
            const float vx = n.x * v;
            energy = (vx * vx + n.y * v * n.y * v + n.z * v * n.z * v) * mass_b;
        }
        else if (!(mass_b - 0.0f >= 0.001f || -(mass_b - 0.0f) >= 0.001f))
        {
            const float v = info.m_relative_normal_velocity;
            const float vx = n.x * v;
            energy = (vx * vx + n.y * v * n.y * v + n.z * v * n.z * v) * mass_a;
        }
        else
        {
            // Both bodies' point velocities along the normal, relative to their common center of mass velocity.
            const float speed_a = info.m_a_point_velocity.y * n.y + n.x * info.m_a_point_velocity.x + info.m_a_point_velocity.z * n.z;
            const vector dir_a = info.m_a_point_velocity.copy_normalize_safe();
            const float ax = speed_a * dir_a.x;
            const float ay = dir_a.y * speed_a;
            const float az = dir_a.z * speed_a;
            const float speed_b = info.m_b_point_velocity.y * n.y + n.x * info.m_b_point_velocity.x + info.m_b_point_velocity.z * n.z;
            const vector dir_b = info.m_b_point_velocity.copy_normalize_safe();
            const float bx = speed_b * dir_b.x;
            const float by = dir_b.y * speed_b;
            const float bz = dir_b.z * speed_b;
            const float total = mass_b + mass_a;
            const float cx = (ax * mass_a + bx * mass_b) / total;
            const float cy = (ay * mass_a + by * mass_b) / total;
            const float cz = (az * mass_a + bz * mass_b) / total;
            energy = ((bx - cx) * (bx - cx) + (by - cy) * (by - cy) + (bz - cz) * (bz - cz)) * mass_b +
                     ((ax - cx) * (ax - cx) + (ay - cy) * (ay - cy) + (az - cz) * (az - cz)) * mass_a;
        }
        if (best_energy < energy)
        {
            best_collision = i;
            best_energy = energy;
        }
    }
    position = c_event->m_collisions[best_collision].m_collision_point;
    normal = c_event->m_collisions[best_collision].m_collision_normal;
    velocity = c_event->m_collisions[best_collision].m_relative_point_velocity;
    return Collision_damage_multiplier * best_energy;
}
