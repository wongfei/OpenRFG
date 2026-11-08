// Original: volition/rfg/code/havok/havok_collision_events.h
// Implementation: volition/rfg/code/havok/havok_collision_events.cpp
//
// Havok world listeners of the game (post-simulation, post-collide, constraint added/removed) and the collision event
// queue processed at the end of the simulation (havok_collision_events_end_sim).
#pragma once

#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/dynamics/collide/deprecated/hkpcollisionlistener.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/dynamics/constraint/hkpconstraintlistener.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/dynamics/world/listener/hkpworldpostcollidelistener.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/dynamics/world/listener/hkpworldpostsimulationlistener.h"
#include "volition/rfg/code/objects/object_type.h"
#include "volition/vlib/code/math/vector3.h"

class hkStepInfo;
class hkVector4f;
class hkpConstraintInstance;
struct hkpContactPointAddedEvent;
struct hkpContactPointConfirmedEvent;
struct hkpContactPointRemovedEvent;
class hkpRigidBody;
class hkpWorld;
class object;
struct object_contact_info;

// size 0x158
struct collision_event
{
    enum constants
    {
        MAX_COLLISIONS = 4,
    };

    // size 0x50
    struct collision_info
    {
        void setup_point_velocities(const hkVector4f* hk_collision_point); // 0x0081B520 (:1227)
        collision_info(); // 0x00711030 (.h:89)

        vector m_collision_point; // 0x00
        vector m_collision_normal; // 0x0C
        vector m_relative_point_velocity; // 0x18
        vector m_a_point_velocity; // 0x24
        vector m_b_point_velocity; // 0x30
        float m_relative_normal_velocity; // 0x3C
        const hkpRigidBody* m_hk_rigid_body_a; // 0x40
        unsigned int m_shapekey_a; // 0x44
        const hkpRigidBody* m_hk_rigid_body_b; // 0x48
        unsigned int m_shapekey_b; // 0x4C
    };

    void swap_event(collision_event* swapped_event) const; // 0x00810260 (:1185)
    bool is_swapped(const collision_event* swapped_event) const; // 0x008103B0 (:1215)
    bool add_collision_info(const collision_info& info); // 0x0080D4A0 (.h:130)
    collision_event(); // 0x00711410

    object* m_object_a; // 0x00
    object* m_object_b; // 0x04
    int sooner; // 0x08
    int later; // 0x0C
    unsigned int thread_id; // 0x10
    unsigned int m_num_collisions; // 0x14
    collision_info m_collisions[4]; // 0x18
};

// size 0xC
struct contact_event
{
    enum type
    {
        CONTACT_STARTED = 0,
        CONTACT_ENDED = 1,
    };

    object* m_object_a; // 0x00
    object* m_object_b; // 0x04
    type m_type; // 0x08
};

// size 0x4
class game_post_simulation_listener : public hkpWorldPostSimulationListener
{
public:
    virtual void postSimulationCallback(hkpWorld* world); // vtbl+0x04 0x00810240 (:1091)
    game_post_simulation_listener(); // 0x0081B300
    virtual ~game_post_simulation_listener(); // vtbl+0x00 0x0080FBE0

};

// size 0x4
class game_post_collide_listener : public hkpWorldPostCollideListener
{
public:
    virtual void postCollideCallback(hkpWorld* world, const hkStepInfo& stepInfo); // vtbl+0x04 0x00810230 (:1078)
    game_post_collide_listener(); // 0x0081B2D0
    virtual ~game_post_collide_listener(); // vtbl+0x00 0x0080FBC0

};

// size 0x4
class game_constraint_listener : public hkpConstraintListener
{
public:
    virtual void constraintAddedCallback(hkpConstraintInstance* constraint); // vtbl+0x04 0x00810250 (:1115)
    virtual void constraintRemovedCallback(hkpConstraintInstance* remove_ci); // vtbl+0x08 0x0081B450 (:1122)
    game_constraint_listener(); // 0x0080FC00
    virtual ~game_constraint_listener(); // vtbl+0x00 0x0080FC10

};

enum contact_point_action
{
    CONTACT_POINT_INVALID = -1,
    CONTACT_POINT_ACCEPT = 0,
    CONTACT_POINT_REJECT = 1,
    CONTACT_POINT_REJECT_BUT_PROCESS_CALLBACKS = 2,
    NUM_CONTACT_POINT_ACTIONS = 3,
};

// size 0x8
struct collision_mass_changer_event
{
    unsigned int m_h_object_a; // 0x00
    unsigned int m_h_object_b; // 0x04
};

// size 0x8
struct collision_mass_changer_output
{
    float m_inv_mass_factor_a; // 0x00
    float m_inv_mass_factor_b; // 0x04
};

// One object in contact with another (object_contact_info::m_contact_list, a circular list).
// size 0x10
struct contact_node
{
    unsigned int m_contacted_object; // 0x00, handle
    unsigned short m_num_contacts; // 0x04, contact points between the two
    contact_node* prev; // 0x08
    contact_node* next; // 0x0C
};

// Callback table entries (indexed by object type, or by a pair of types: havok_get_table_index).
// size 0x8
struct contact_event_callback_info
{
    void (*m_contact_begin_callback)(const contact_event*); // 0x00
    void (*m_contact_end_callback)(const contact_event*); // 0x04
};

// size 0x4
struct collision_event_immediate_callback_info
{
    contact_point_action (*m_callback)(const collision_event*); // 0x00
};

// size 0x4
struct collision_event_callback_info
{
    void (*m_callback)(const collision_event*); // 0x00
};

// size 0x8
struct collision_event_mass_changer_cb_info
{
    bool (*m_callback)(collision_mass_changer_output*, const collision_mass_changer_event*, void*); // 0x00
    void* m_user_data; // 0x04
};

// size 0x4
class GeneralCollisionListener : public hkpCollisionListener
{
public:
    virtual void contactPointAddedCallback(hkpContactPointAddedEvent& add_event); // vtbl+0x10 0x00824440 (:819)
    virtual void contactPointConfirmedCallback(hkpContactPointConfirmedEvent& event); // vtbl+0x1C 0x00810210 (:808)
    virtual void contactPointRemovedCallback(hkpContactPointRemovedEvent& remove_event); // vtbl+0x14 0x0081B360 (:965)
    GeneralCollisionListener(); // 0x00824200
    virtual ~GeneralCollisionListener(); // vtbl+0x0C 0x0080FBA0

};

void havok_collision_events_system_init(); // 0x008103D0 (:1267)
void havok_collision_events_system_shutdown(); // 0x00810470 (:1328)
void havok_collision_events_level_init_pre(); // 0x008104C0 (:1353)
void havok_collision_events_begin_sim(); // 0x00810600 (:1377)
void havok_collision_events_end_sim(); // 0x00810610 (:1383)
void havok_collision_events_add_callback(object_type obj_a_type, object_type obj_b_type, void (*callback)(const collision_event*)); // 0x00810780 (:1442)
void havok_collision_events_add_unary_callback(object_type obj_type, void (*callback)(const collision_event*)); // 0x008107E0 (:1458)
void havok_collision_events_add_unary_immediate_callback(object_type obj_type, contact_point_action (*callback)(const collision_event*)); // 0x008107F0 (:1473)
void havok_collision_events_add_mass_changer_callback(object_type obj_a_type, object_type obj_b_type, bool (*callback)(collision_mass_changer_output*, const collision_mass_changer_event*, void*), void* user_data); // 0x00810800 (:1491)
void havok_collision_events_add_contact_callback(object_type obj_a_type, object_type obj_b_type, contact_event::type contact_event_type, void (*callback)(const contact_event*)); // 0x00810850 (:1515)
void havok_collision_events_calculate_averages(vector* avg_collision_point, vector* avg_collision_normal, float* avg_relative_speed, float* avg_a_projected_speed, float* avg_b_projected_speed, const collision_event* event); // 0x008108D0 (:1546)
bool havok_collision_events_object_in_contact(const object* p_object); // 0x00810B30 (:1588)
void havok_collision_events_object_freed(object* a_object); // 0x00810B60 (:1606)
bool havok_add_collision_event(object* obj_a, object* obj_b, const collision_event::collision_info& coll_info); // 0x00810C60 (:1666)
const object_contact_info* havok_collision_events_get_contact_info(const object* objp); // 0x0080D4D0 (.h:274)
object_contact_info* havok_collision_events_get_contact_info(object* objp); // 0x0080D4E0 (.h:282)
