// Original: volition/rfg/code/havok/havok_collision_events.cpp
//
// Collision and contact events of the game objects (decompiled). GeneralCollisionListener turns every Havok contact
// point between two entities with objects into:
// - a collision_event (up to 4 contact points per pair and thread) in Collision_events, a binary tree keyed by
//   object_a; the objects are ordered by address. havok_collision_events_end_sim calls the unary callback of each
//   object's type (the other object's sees the event swapped) and the callback of the pair of types.
// - per object contact lists (object_contact_info, contact_node with the other object's handle and the number of
//   contact points), whose first and last contact points raise CONTACT_STARTED / CONTACT_ENDED contact events (queued
//   while the world simulates, otherwise called at once).
// - immediate unary callbacks that may reject the contact point, and mass changer callbacks that scale the inverse
//   masses of a contact between two simple-contact entities.
// Callbacks are kept per object type or per pair of types (index b * (b + 1) / 2 + a for a <= b). The tables are
// file statics of havok_collision_callbacks.cpp, which registers the game's callbacks (unity build).
// Compiled in rfg_havok_unity.cpp.
//
// Notes (decompiled behaviour, kept as is):
// - havok_collision_events_add_contact_event drops a queued event of the same pair instead of adding another (a
//   contact that starts and ends within one step raises no event); the event type is not compared.
// - havok_collision_events_end_sim reads the unary callback of type -1 before checking the type (the check is done
//   first here).

#include "volition/rfg/code/havok/havok_collision_events.h"

#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/agent/collidable/hkpcollidable.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/agent/hkpprocesscollisionoutput.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/dynamics/collide/deprecated/hkpcollisionevents.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/dynamics/collide/hkpresponsemodifier.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/dynamics/constraint/hkpconstraintinstance.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/dynamics/entity/hkprigidbody.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/dynamics/world/hkpworld.h"
#include "volition/rfg/code/destruction/rfg_dynamics.h"
#include "volition/rfg/code/havok/havok.h"
#include "volition/rfg/code/level/world.h"
#include "volition/rfg/code/objects/object.h"
#include "volition/rfg/code/objects/object_constraint.h"
#include "volition/vlib/code/os/cs_wrapper.h"
#include "volition/vlib/code/os/thread.h"

#include "keen/base/trace.hpp"

#include <math.h>

static const int MAX_COLLISION_EVENTS = 100;
static const int MAX_CONTACT_EVENTS = 500;
static const int MAX_CONTACT_NODES = 5000;

// ---- file statics
static contact_event Contact_events[MAX_CONTACT_EVENTS]; // 0x02666060
static contact_node Contact_node_array[MAX_CONTACT_NODES]; // 0x02B6E980
static unsigned int Num_collision_events; // 0x02B8226C
static contact_node* Contact_node_free_list; // 0x02B82270
static unsigned int Num_contact_events; // 0x02B82274
// cs_wrapper Contact_event_wrapper (0x02B8CAC0, guards the contact lists and events) is used here too; placed in its
// first user in the unity TU, havok_collision_callbacks.cpp (havok_collide_vehicle).
static collision_event Collision_events[MAX_COLLISION_EVENTS]; // 0x02B8CB00
cs_wrapper Collision_event_wrapper_havok(0); // 0x02B9529C, guards Collision_events

// ---- circular contact node lists (inlined everywhere in the original)
static void contact_list_remove(contact_node*& head, contact_node* node)
{
    if (head == node)
    {
        if (node->next == node)
        {
            node->next = nullptr;
            node->prev = nullptr;
            head = nullptr;
            return;
        }
        head = node->next;
    }
    if (node->prev != nullptr)
    {
        node->prev->next = node->next;
    }
    if (node->next != nullptr)
    {
        node->next->prev = node->prev;
    }
    node->next = nullptr;
    node->prev = nullptr;
}

// Appends at the tail (before the head).
static void contact_list_add(contact_node*& head, contact_node* node)
{
    if (head == nullptr)
    {
        node->next = node;
        node->prev = node;
        head = node;
        return;
    }
    node->prev = head->prev;
    node->next = head;
    head->prev->next = node;
    head->prev = node;
}

// The type of a world object's object, or OT_UNDEFINED.
static object_type havok_collision_events_object_type(const object* obj)
{
    return obj != nullptr ? obj->get_obj_type() : OT_UNDEFINED;
}

// 0x0080FC20 (:322)
// LTCG: a_type in ECX, b_type in EDX. The index of a pair of types (0 when either is not a valid type).
static unsigned int havok_get_table_index(object_type a_type, object_type b_type)
{
    if ((unsigned int)a_type >= NUM_OBJECT_TYPES || (unsigned int)b_type >= NUM_OBJECT_TYPES)
    {
        return 0;
    }
    int high = a_type;
    int low = b_type;
    if (high < low)
    {
        high = b_type;
        low = a_type;
    }
    return ((high + 1) * high >> 1) + low;
}

// 0x0080FC50 (:343)
// LTCG: obj in EAX.
static object_type havok_collision_events_get_object_type(const object* obj)
{
    return havok_collision_events_object_type(obj);
}

// 0x0080FC60 (:352)
// LTCG: obj in EAX.
static unsigned int havok_collision_events_get_object_handle(const object* obj)
{
    return obj != nullptr ? obj->get_handle() : 0xFFFFFFFF;
}

// 0x0080FC70 (:368)
// LTCG: a_obj_type in EAX, b_obj_type in ECX.
static bool havok_collision_events_contact_callback_enabled(object_type a_obj_type, object_type b_obj_type)
{
    const unsigned int index = havok_get_table_index(a_obj_type, b_obj_type); // inlined
    return Contact_event_callback_table[index].m_contact_begin_callback != nullptr || Contact_event_callback_table[index].m_contact_end_callback != nullptr;
}

// 0x0080FCC0 (:381)
// LTCG: obj_a in ECX, obj_b in EAX. Adds the contact point to the pair's event of this thread (while it has room)
// or starts a new event; with all events in use the contact goes to temp_event (if any).
static collision_event* find_or_alloc_collision_event(object* obj_a, object* obj_b, const collision_event::collision_info& coll_info, collision_event* temp_event)
{
    Collision_event_wrapper_havok.enter();
    collision_event* parent = nullptr;
    const unsigned int thread_id = get_current_thread_id();
    if (Num_collision_events != 0)
    {
        int index = 0;
        do
        {
            collision_event* event = &Collision_events[index];
            if (event->m_object_a == obj_a && event->m_object_b == obj_b && event->thread_id == thread_id && event->m_num_collisions < collision_event::MAX_COLLISIONS)
            {
                event->m_collisions[event->m_num_collisions] = coll_info;
                event->m_num_collisions++;
                Collision_event_wrapper_havok.leave();
                return event;
            }
            parent = event;
            index = obj_a < event->m_object_a ? event->sooner : event->later;
        } while (index >= 0);
    }
    collision_event* event;
    if (Num_collision_events < MAX_COLLISION_EVENTS)
    {
        if (parent != nullptr)
        {
            if (obj_a < parent->m_object_a)
            {
                parent->sooner = Num_collision_events;
            }
            else
            {
                parent->later = Num_collision_events;
            }
        }
        event = &Collision_events[Num_collision_events++];
    }
    else if (temp_event != nullptr)
    {
        event = temp_event;
    }
    else
    {
        Collision_event_wrapper_havok.leave();
        return nullptr;
    }
    event->sooner = -1;
    event->later = -1;
    event->m_object_a = obj_a;
    event->m_object_b = obj_b;
    event->m_num_collisions = 0;
    event->thread_id = thread_id;
    event->m_collisions[0] = coll_info;
    event->m_num_collisions++;
    Collision_event_wrapper_havok.leave();
    return event;
}

// 0x00824240 (:481)
// LTCG: hk_rb_b in ECX, temp_event in EDX, contact_event in ESI. The contact point as seen from the object with the
// lower address (the normal is flipped when obj_b is lower).
static collision_event* havok_add_collision_event(object* obj_a, object* obj_b, hkpRigidBody* hk_rb_a, hkpRigidBody* hk_rb_b, hkpContactPointAddedEvent& contact_event, collision_event& temp_event)
{
    const bool swapped = obj_a > obj_b;
    if (swapped)
    {
        object* low = obj_b;
        obj_b = obj_a;
        obj_a = low;
    }
    const hkVector4f collision_point = contact_event.m_contactPoint->m_position;
    const hkVector4f& normal = contact_event.m_contactPoint->m_separatingNormal;
    collision_event::collision_info info;
    info.m_collision_point.set(collision_point.m_quad.m128_f32[0], collision_point.m_quad.m128_f32[1], collision_point.m_quad.m128_f32[2]);
    info.m_relative_normal_velocity = fabsf(contact_event.m_projectedVelocity);
    if (swapped)
    {
        info.m_collision_normal.set(-normal.m_quad.m128_f32[0], -normal.m_quad.m128_f32[1], -normal.m_quad.m128_f32[2]);
        info.m_shapekey_a = contact_event.m_bodyB->m_shapeKey;
        info.m_shapekey_b = contact_event.m_bodyA->m_shapeKey;
        info.m_hk_rigid_body_a = hk_rb_b;
        info.m_hk_rigid_body_b = hk_rb_a;
    }
    else
    {
        info.m_collision_normal.set(normal.m_quad.m128_f32[0], normal.m_quad.m128_f32[1], normal.m_quad.m128_f32[2]);
        info.m_shapekey_a = contact_event.m_bodyA->m_shapeKey;
        info.m_shapekey_b = contact_event.m_bodyB->m_shapeKey;
        info.m_hk_rigid_body_a = hk_rb_a;
        info.m_hk_rigid_body_b = hk_rb_b;
    }
    info.setup_point_velocities(&collision_point);
    return find_or_alloc_collision_event(obj_a, obj_b, info, &temp_event);
}

// 0x0080FDE0 (:535)
// LTCG: obj_cinfo in EDI. Moves a node from the free list to the object's contact list.
static contact_node* const havok_collision_events_next_free_contact_node(object_contact_info* obj_cinfo)
{
    contact_node* node = Contact_node_free_list;
    if (node == nullptr)
    {
        keen::debug::nativePrintArgs("Int3 critical function at %s(%d)\n", __FILE__, __LINE__);
        return nullptr;
    }
    contact_list_remove(Contact_node_free_list, node);
    contact_list_add(obj_cinfo->m_contact_list, node);
    return node;
}

// 0x0080FE80 (:557)
// LTCG: obj_cinfo in EDX, p_cnode in EAX.
static void havok_collision_events_free_contact_node(object_contact_info* obj_cinfo, contact_node* p_cnode)
{
    p_cnode->m_contacted_object = 0xFFFFFFFF;
    p_cnode->m_num_contacts = 0;
    contact_list_remove(obj_cinfo->m_contact_list, p_cnode);
    contact_list_add(Contact_node_free_list, p_cnode);
}

// 0x0080FF00 (:583)
// LTCG: obj_a in ESI, obj_b in EDI. Queues a contact event for end_sim (see the notes).
static void havok_collision_events_add_contact_event(object* obj_a, object* obj_b, contact_event::type contact_event_type)
{
    if (obj_a == nullptr || obj_b == nullptr)
    {
        return;
    }
    for (unsigned int i = 0; i < Num_contact_events; i++)
    {
        if (Contact_events[i].m_object_a == obj_a && Contact_events[i].m_object_b == obj_b)
        {
            Num_contact_events--;
            if (i < Num_contact_events)
            {
                Contact_events[i] = Contact_events[Num_contact_events];
            }
            return;
        }
    }
    if (Num_contact_events >= MAX_CONTACT_EVENTS)
    {
        keen::debug::nativePrintArgs("Int3 critical function at %s(%d)\n", __FILE__, __LINE__);
        return;
    }
    contact_event& event = Contact_events[Num_contact_events++];
    event.m_object_a = obj_a;
    event.m_object_b = obj_b;
    event.m_type = contact_event_type;
}

// 0x0080FFA0 (:635)
static void havok_collision_events_call_contact_callbacks(const contact_event* event)
{
    const object_type type_a = event->m_object_a != nullptr ? event->m_object_a->get_obj_type() : OT_UNDEFINED;
    const object_type type_b = event->m_object_b != nullptr ? event->m_object_b->get_obj_type() : OT_UNDEFINED;
    if (!havok_collision_events_contact_callback_enabled(type_a, type_b)) // inlined
    {
        return;
    }
    const contact_event_callback_info& info = Contact_event_callback_table[havok_get_table_index(type_a, type_b)];
    if (event->m_type == contact_event::CONTACT_STARTED && info.m_contact_begin_callback != nullptr)
    {
        info.m_contact_begin_callback(event);
    }
    else if (event->m_type == contact_event::CONTACT_ENDED && info.m_contact_end_callback != nullptr)
    {
        info.m_contact_end_callback(event);
    }
}

// 0x00810070 (:661)
// LTCG: obj_a in EAX. Counts a contact point of obj_a with obj_b; the first one starts the contact.
static void havok_collision_events_add_contact_point(object* obj_a, object* obj_b)
{
    Contact_event_wrapper.enter();
    object_contact_info* cinfo = havok_collision_events_get_contact_info(obj_a);
    const unsigned int other_handle = havok_collision_events_get_object_handle(obj_b); // inlined
    contact_node* node = cinfo->m_contact_list;
    while (node != nullptr)
    {
        if (node->m_contacted_object == other_handle)
        {
            node->m_num_contacts++;
            Contact_event_wrapper.leave();
            return;
        }
        node = node->next;
        if (node == cinfo->m_contact_list)
        {
            break;
        }
    }
    node = havok_collision_events_next_free_contact_node(cinfo);
    if (node == nullptr)
    {
        Contact_event_wrapper.leave();
        return;
    }
    node->m_contacted_object = other_handle;
    node->m_num_contacts = 1;
    if (Havok_world_is_simulating)
    {
        havok_collision_events_add_contact_event(obj_a, obj_b, contact_event::CONTACT_STARTED);
    }
    else
    {
        contact_event event;
        event.m_object_a = obj_a;
        event.m_object_b = obj_b;
        event.m_type = contact_event::CONTACT_STARTED;
        havok_collision_events_call_contact_callbacks(&event);
    }
    Contact_event_wrapper.leave();
}

// 0x00810150 (:734)
// LTCG: obj_a in ECX, obj_b in EAX. Uncounts a contact point; the last one ends the contact.
static void havok_collision_events_remove_contact_point(object* obj_a, object* obj_b)
{
    Contact_event_wrapper.enter();
    object_contact_info* cinfo = havok_collision_events_get_contact_info(obj_a);
    contact_node* head = cinfo->m_contact_list;
    contact_node* node = head;
    while (node != nullptr && node->m_contacted_object != obj_b->get_handle())
    {
        node = node->next;
        if (node == head)
        {
            node = nullptr;
        }
    }
    if (node != nullptr && --node->m_num_contacts == 0)
    {
        havok_collision_events_free_contact_node(cinfo, node);
        if (Havok_world_is_simulating)
        {
            havok_collision_events_add_contact_event(obj_a, obj_b, contact_event::CONTACT_ENDED);
        }
        else
        {
            contact_event event;
            event.m_object_a = obj_a;
            event.m_object_b = obj_b;
            event.m_type = contact_event::CONTACT_ENDED;
            havok_collision_events_call_contact_callbacks(&event);
        }
    }
    Contact_event_wrapper.leave();
}

// ---- GeneralCollisionListener
// 0x00810210 (:808)
void GeneralCollisionListener::contactPointConfirmedCallback(hkpContactPointConfirmedEvent& event)
{
}

// 0x00810220 (:813)
// LTCG: obj_type in EAX.
static contact_point_action (*get_immediate_unary_callback(object_type obj_type))(const collision_event*)
{
    if (obj_type == OT_UNDEFINED)
    {
        return nullptr;
    }
    return Collision_event_immediate_unary_callbacks[obj_type].m_callback;
}

// The entity of a root collidable (broadphase type 1), or null.
static hkpWorldObject* havok_contact_entity(const hkpCdBody* body)
{
    while (body->m_parent != nullptr)
    {
        body = body->m_parent;
    }
    const hkpCollidable* collidable = (const hkpCollidable*)body;
    return collidable->m_broadPhaseHandle.m_type == 1 ? (hkpWorldObject*)collidable->getOwner() : nullptr; // hkpWorldObject::BROAD_PHASE_ENTITY
}

// The object (property 0) of a world object, or null.
static object* havok_contact_object(const hkpWorldObject* wo)
{
    if (wo == nullptr || !wo->hasProperty(HAVOK_PROPERTY_OBJECT, hkpWorldObject::MULTI_THREADING_CHECKS_ENABLE))
    {
        return nullptr;
    }
    return (object*)wo->getProperty(HAVOK_PROPERTY_OBJECT, hkpWorldObject::MULTI_THREADING_CHECKS_ENABLE).m_data;
}

// 0x00824440 (:819)
// A contact point between two entities with objects: the destruction code may veto it (rfg movers), the contact
// lists and mass changers are updated, then the collision event is recorded and the immediate callbacks of both
// objects' types may reject the point (the event then drops it, unless a callback asked to process it anyway).
void GeneralCollisionListener::contactPointAddedCallback(hkpContactPointAddedEvent& add_event)
{
    hkpWorldObject* wo_a = havok_contact_entity(add_event.m_bodyA);
    hkpWorldObject* wo_b = havok_contact_entity(add_event.m_bodyB);
    object* obj_a = havok_contact_object(wo_a);
    object* obj_b = havok_contact_object(wo_b);
    if (obj_a == nullptr || obj_b == nullptr)
    {
        return;
    }
    const object_type type_a = obj_a->get_obj_type();
    const object_type type_b = obj_b->get_obj_type();
    if ((obj_a->get_sub_type() == OT_SUB_MOVER_RFG || obj_b->get_sub_type() == OT_SUB_MOVER_RFG) && !rfg_dyn_add_pre_collide_contact(add_event, obj_a, obj_b))
    {
        add_event.m_status = HK_CONTACT_POINT_REJECT;
        return;
    }
    if (havok_collision_events_contact_callback_enabled(type_a, type_b))
    {
        havok_collision_events_add_contact_point(obj_a, obj_b);
        havok_collision_events_add_contact_point(obj_b, obj_a);
    }
    hkpRigidBody* rb_a = (hkpRigidBody*)wo_a;
    hkpRigidBody* rb_b = (hkpRigidBody*)wo_b;
    if (rb_a->m_material.m_responseType.m_storage == hkpMaterial::RESPONSE_SIMPLE_CONTACT && rb_b->m_material.m_responseType.m_storage == hkpMaterial::RESPONSE_SIMPLE_CONTACT)
    {
        const collision_event_mass_changer_cb_info& changer = Collision_event_mass_changer_callback_table[havok_get_table_index(type_a, type_b)];
        if (changer.m_callback != nullptr)
        {
            collision_mass_changer_event event;
            event.m_h_object_a = obj_a->get_handle();
            event.m_h_object_b = obj_b->get_handle();
            collision_mass_changer_output output;
            if (changer.m_callback(&output, &event, changer.m_user_data))
            {
                hkVector4f factor_a;
                factor_a.setAll(output.m_inv_mass_factor_a);
                hkVector4f factor_b;
                factor_b.setAll(output.m_inv_mass_factor_b);
                hkpResponseModifier::setInvMassScalingForContact(add_event.m_internalContactMgr, rb_a, rb_b, *add_event.m_collisionOutput->m_constraintOwner.val(), factor_a, factor_b);
            }
        }
    }
    collision_event temp_event;
    collision_event* event = havok_add_collision_event(obj_a, obj_b, rb_a, rb_b, add_event, temp_event);
    if (event == nullptr)
    {
        return;
    }
    bool process_callbacks = false;
    contact_point_action (*callback_a)(const collision_event*) =
        event->m_object_a != nullptr ? get_immediate_unary_callback(event->m_object_a->get_obj_type()) : nullptr; // inlined
    if (callback_a != nullptr)
    {
        const contact_point_action action = callback_a(event);
        if (action == CONTACT_POINT_REJECT || action == CONTACT_POINT_REJECT_BUT_PROCESS_CALLBACKS)
        {
            process_callbacks = action == CONTACT_POINT_REJECT_BUT_PROCESS_CALLBACKS;
            add_event.m_status = HK_CONTACT_POINT_REJECT;
        }
    }
    contact_point_action (*callback_b)(const collision_event*) =
        event->m_object_b != nullptr ? get_immediate_unary_callback(event->m_object_b->get_obj_type()) : nullptr;
    if (callback_b != nullptr)
    {
        collision_event swapped;
        event->swap_event(&swapped);
        const contact_point_action action = callback_b(&swapped);
        if (action == CONTACT_POINT_REJECT || action == CONTACT_POINT_REJECT_BUT_PROCESS_CALLBACKS)
        {
            if (action == CONTACT_POINT_REJECT_BUT_PROCESS_CALLBACKS)
            {
                process_callbacks = true;
            }
            add_event.m_status = HK_CONTACT_POINT_REJECT;
            event->m_num_collisions = swapped.m_num_collisions;
        }
    }
    if (add_event.m_status == HK_CONTACT_POINT_REJECT && !process_callbacks)
    {
        event->m_num_collisions--;
    }
}

// 0x0081B360 (:965)
// The destruction code drops its contact of rfg movers; except between two movers, the contact lists are updated.
void GeneralCollisionListener::contactPointRemovedCallback(hkpContactPointRemovedEvent& remove_event)
{
    object* obj_a = havok_contact_object(remove_event.m_entityA);
    object* obj_b = havok_contact_object(remove_event.m_entityB);
    if (obj_a == nullptr || obj_b == nullptr)
    {
        return;
    }
    const object_type type_a = obj_a->get_obj_type();
    const object_type type_b = obj_b->get_obj_type();
    if (obj_a->get_sub_type() == OT_SUB_MOVER_RFG || obj_b->get_sub_type() == OT_SUB_MOVER_RFG)
    {
        rfg_dyn_remove_pre_collide_contact(remove_event);
        if (type_a == OT_MOVER && type_b == OT_MOVER)
        {
            return;
        }
    }
    if (havok_collision_events_contact_callback_enabled(type_a, type_b))
    {
        havok_collision_events_remove_contact_point(obj_a, obj_b);
        havok_collision_events_remove_contact_point(obj_b, obj_a);
    }
}

// 0x0080FBA0
GeneralCollisionListener::~GeneralCollisionListener()
{
}

// 0x00824200
GeneralCollisionListener::GeneralCollisionListener()
{
}

// ---- world listeners
// 0x00810230 (:1078)
void game_post_collide_listener::postCollideCallback(hkpWorld* world, const hkStepInfo& stepInfo)
{
    rfg_dyn_do_collide_post();
}

// 0x0081B2D0
game_post_collide_listener::game_post_collide_listener()
{
}

// 0x0080FBC0
game_post_collide_listener::~game_post_collide_listener()
{
}

// 0x00810240 (:1091)
void game_post_simulation_listener::postSimulationCallback(hkpWorld* world)
{
}

// 0x0081B300
game_post_simulation_listener::game_post_simulation_listener()
{
}

// 0x0080FBE0
game_post_simulation_listener::~game_post_simulation_listener()
{
}

// 0x00810250 (:1115)
void game_constraint_listener::constraintAddedCallback(hkpConstraintInstance* constraint)
{
}

// 0x0081B450 (:1122)
// A constraint Havok removes (other than the constraint data types 9..11): the game constraint object of the first
// entity's object that owns it is destroyed and, if that was its last child constraint, the body is activated.
void game_constraint_listener::constraintRemovedCallback(hkpConstraintInstance* remove_ci)
{
    const int type = remove_ci->m_data->getType();
    if (type >= 9 && type <= 11) // hkpConstraintData::CONSTRAINT_TYPE_HINGE_LIMITS .. CONSTRAINT_TYPE_POINTTOPATH
    {
        return;
    }
    object* objp = havok_contact_object(remove_ci->m_entities[0]);
    if (objp == nullptr || objp->child_constraint_ptr == nullptr)
    {
        return;
    }
    constraint* first = objp->child_constraint_ptr;
    constraint* cp = first;
    while (cp->hk_constraint != remove_ci)
    {
        cp = cp->child_constraint_next;
        if (cp == first || cp == nullptr)
        {
            return;
        }
    }
    World.destroy_object(cp, false);
    if (objp->havok_handle != 0xFFFFFFFF && objp->child_constraint_ptr == nullptr && Hk_rigid_bodies[objp->havok_handle] != nullptr)
    {
        Hk_rigid_bodies[objp->havok_handle]->activateAsCriticalOperation();
    }
}

// 0x0080FC00
game_constraint_listener::game_constraint_listener()
{
}

// 0x0080FC10
game_constraint_listener::~game_constraint_listener()
{
}

// ---- collision_event
// 0x00711410
collision_event::collision_event()
{
}

// 0x00810260 (:1185)
// The event as object_b sees it: objects, rigid bodies and shape keys exchanged, normals negated.
void collision_event::swap_event(collision_event* swapped_event) const
{
    swapped_event->m_object_a = m_object_b;
    swapped_event->m_object_b = m_object_a;
    swapped_event->m_num_collisions = m_num_collisions;
    for (unsigned int i = 0; i < m_num_collisions; i++)
    {
        const collision_info& in = m_collisions[i];
        collision_info& out = swapped_event->m_collisions[i];
        out.m_hk_rigid_body_a = in.m_hk_rigid_body_b;
        out.m_hk_rigid_body_b = in.m_hk_rigid_body_a;
        out.m_shapekey_a = in.m_shapekey_b;
        out.m_shapekey_b = in.m_shapekey_a;
        out.m_collision_point = in.m_collision_point;
        out.m_collision_normal.set(in.m_collision_normal.x * -1.0f, in.m_collision_normal.y * -1.0f, in.m_collision_normal.z * -1.0f);
        out.m_relative_point_velocity.set(in.m_relative_point_velocity.x * -1.0f, in.m_relative_point_velocity.y * -1.0f, in.m_relative_point_velocity.z * -1.0f);
        out.m_relative_normal_velocity = in.m_relative_normal_velocity;
        out.m_a_point_velocity = in.m_b_point_velocity;
        out.m_b_point_velocity = in.m_a_point_velocity;
    }
}

// 0x008103B0 (:1215)
bool collision_event::is_swapped(const collision_event* swapped_event) const
{
    return swapped_event->m_object_a == m_object_b && swapped_event->m_object_b == m_object_a;
}

// The velocity of a rigid body at a world point (linear + angular x arm).
static void havok_point_velocity(vector& velocity, const hkpRigidBody* rb, const hkVector4f& point)
{
    const hkpMotion& motion = rb->m_motion;
    const float* com = motion.m_motionState.m_sweptTransform.m_centerOfMass1.m_quad.m128_f32;
    const float* w = motion.m_angularVelocity.m_quad.m128_f32;
    const float* v = motion.m_linearVelocity.m_quad.m128_f32;
    const float rx = point.m_quad.m128_f32[0] - com[0];
    const float ry = point.m_quad.m128_f32[1] - com[1];
    const float rz = point.m_quad.m128_f32[2] - com[2];
    velocity.x = (rz * w[1] - w[2] * ry) + v[0];
    velocity.y = (rx * w[2] - w[0] * rz) + v[1];
    velocity.z = (ry * w[0] - w[1] * rx) + v[2];
}

// 0x0081B520 (:1227)
void collision_event::collision_info::setup_point_velocities(const hkVector4f* hk_collision_point)
{
    havok_point_velocity(m_a_point_velocity, m_hk_rigid_body_a, *hk_collision_point);
    havok_point_velocity(m_b_point_velocity, m_hk_rigid_body_b, *hk_collision_point);
    m_relative_point_velocity.x = m_b_point_velocity.x - m_a_point_velocity.x;
    m_relative_point_velocity.y = m_b_point_velocity.y - m_a_point_velocity.y;
    m_relative_point_velocity.z = m_b_point_velocity.z - m_a_point_velocity.z;
}

// 0x00711030 (.h:89)
collision_event::collision_info::collision_info()
{
    m_collision_point.zero();
    m_collision_normal.zero();
    m_relative_point_velocity.zero();
    m_a_point_velocity.zero();
    m_b_point_velocity.zero();
    m_shapekey_a = 0xFFFFFFFF;
    m_shapekey_b = 0xFFFFFFFF;
    m_relative_normal_velocity = 0.0f;
    m_hk_rigid_body_a = nullptr;
    m_hk_rigid_body_b = nullptr;
}

// 0x0080D4A0 (.h:130)
bool collision_event::add_collision_info(const collision_info& info)
{
    if (m_num_collisions >= MAX_COLLISIONS)
    {
        return false;
    }
    m_collisions[m_num_collisions] = info;
    m_num_collisions++;
    return true;
}

// ---- functions
// 0x008103D0 (:1267)
// Registers the listeners with the world and clears the callback tables.
void havok_collision_events_system_init()
{
    Havok_world->addContactListener((hkpContactListener*)&Collision_listener);
    Havok_world->addWorldPostCollideListener(&Post_collide_listener);
    Havok_world->addWorldPostSimulationListener(&Post_simulation_listener);
    Havok_world->addConstraintListener(&Constraint_listener);
    for (int i = 0; i < NUM_OBJECT_TYPE_PAIRS; i++)
    {
        Collision_event_callback_table[i].m_callback = nullptr;
    }
    for (int i = 0; i < NUM_OBJECT_TYPE_PAIRS; i++)
    {
        Contact_event_callback_table[i].m_contact_begin_callback = nullptr;
        Contact_event_callback_table[i].m_contact_end_callback = nullptr;
        Collision_event_mass_changer_callback_table[i].m_callback = nullptr;
        Collision_event_mass_changer_callback_table[i].m_user_data = nullptr;
    }
    for (int i = 0; i < NUM_OBJECT_TYPES; i++)
    {
        Collision_event_immediate_unary_callbacks[i].m_callback = nullptr;
    }
    for (int i = 0; i < NUM_OBJECT_TYPES; i++)
    {
        Collision_event_unary_callbacks[i].m_callback = nullptr;
    }
}

// 0x00810470 (:1328)
void havok_collision_events_system_shutdown()
{
    Havok_world->removeConstraintListener(&Constraint_listener);
    Havok_world->removeWorldPostCollideListener(&Post_collide_listener);
    Havok_world->removeWorldPostSimulationListener(&Post_simulation_listener);
    Havok_world->removeContactListener((hkpContactListener*)&Collision_listener);
}

// 0x008104C0 (:1353)
void havok_collision_events_level_init_pre()
{
    Contact_node_free_list = nullptr;
    for (int i = 0; i < MAX_CONTACT_NODES; i++)
    {
        contact_node& node = Contact_node_array[i];
        node.m_contacted_object = 0xFFFFFFFF;
        node.m_num_contacts = 0;
        node.next = nullptr;
        node.prev = nullptr;
        contact_list_add(Contact_node_free_list, &node);
    }
}

// 0x00810600 (:1377)
void havok_collision_events_begin_sim()
{
}

// 0x00810610 (:1383)
// Calls the collision callbacks of the step's events, then the queued contact callbacks.
void havok_collision_events_end_sim()
{
    for (unsigned int i = 0; i < Num_collision_events; i++)
    {
        collision_event* event = &Collision_events[i];
        if (event->m_num_collisions == 0)
        {
            continue;
        }
        const object_type type_a = havok_collision_events_object_type(event->m_object_a);
        const object_type type_b = havok_collision_events_object_type(event->m_object_b);
        if (type_a != OT_UNDEFINED && Collision_event_unary_callbacks[type_a].m_callback != nullptr)
        {
            Collision_event_unary_callbacks[type_a].m_callback(event);
        }
        if (type_b != OT_UNDEFINED && Collision_event_unary_callbacks[type_b].m_callback != nullptr)
        {
            collision_event swapped;
            event->swap_event(&swapped);
            Collision_event_unary_callbacks[type_b].m_callback(&swapped);
        }
        const collision_event_callback_info& info = Collision_event_callback_table[havok_get_table_index(type_a, type_b)];
        if (info.m_callback != nullptr && type_a != OT_UNDEFINED && type_b != OT_UNDEFINED)
        {
            info.m_callback(event);
        }
    }
    Num_collision_events = 0;
    for (unsigned int i = 0; i < Num_contact_events; i++)
    {
        havok_collision_events_call_contact_callbacks(&Contact_events[i]);
    }
    Num_contact_events = 0;
}

// 0x00810780 (:1442)
// Invalid types go to slot 0.
void havok_collision_events_add_callback(object_type obj_a_type, object_type obj_b_type, void (*callback)(const collision_event*))
{
    Collision_event_callback_table[havok_get_table_index(obj_a_type, obj_b_type)].m_callback = callback; // inlined
}

// 0x008107E0 (:1458)
void havok_collision_events_add_unary_callback(object_type obj_type, void (*callback)(const collision_event*))
{
    Collision_event_unary_callbacks[obj_type].m_callback = callback;
}

// 0x008107F0 (:1473)
void havok_collision_events_add_unary_immediate_callback(object_type obj_type, contact_point_action (*callback)(const collision_event*))
{
    Collision_event_immediate_unary_callbacks[obj_type].m_callback = callback;
}

// 0x00810800 (:1491)
void havok_collision_events_add_mass_changer_callback(object_type obj_a_type, object_type obj_b_type, bool (*callback)(collision_mass_changer_output*, const collision_mass_changer_event*, void*), void* user_data)
{
    collision_event_mass_changer_cb_info& info = Collision_event_mass_changer_callback_table[havok_get_table_index(obj_a_type, obj_b_type)];
    info.m_callback = callback;
    info.m_user_data = user_data;
}

// 0x00810850 (:1515)
void havok_collision_events_add_contact_callback(object_type obj_a_type, object_type obj_b_type, contact_event::type contact_event_type, void (*callback)(const contact_event*))
{
    contact_event_callback_info& info = Contact_event_callback_table[havok_get_table_index(obj_a_type, obj_b_type)];
    if (contact_event_type == contact_event::CONTACT_STARTED)
    {
        info.m_contact_begin_callback = callback;
    }
    else if (contact_event_type == contact_event::CONTACT_ENDED)
    {
        info.m_contact_end_callback = callback;
    }
    else
    {
        keen::debug::nativePrintArgs("Int3 critical function at %s(%d)\n", __FILE__, __LINE__);
    }
}

// 0x008108D0 (:1546)
// Averages of the event's contact points: position, normal (normalized), relative normal velocity and both objects'
// point velocities along the normal.
void havok_collision_events_calculate_averages(vector* avg_collision_point, vector* avg_collision_normal, float* avg_relative_speed, float* avg_a_projected_speed, float* avg_b_projected_speed, const collision_event* event)
{
    avg_collision_point->zero();
    avg_collision_normal->zero();
    *avg_relative_speed = 0.0f;
    *avg_a_projected_speed = 0.0f;
    *avg_b_projected_speed = 0.0f;
    for (unsigned int i = 0; i < event->m_num_collisions; i++)
    {
        const collision_event::collision_info& info = event->m_collisions[i];
        const vector& n = info.m_collision_normal;
        avg_collision_point->x += info.m_collision_point.x;
        avg_collision_point->y += info.m_collision_point.y;
        avg_collision_point->z += info.m_collision_point.z;
        avg_collision_normal->x += n.x;
        avg_collision_normal->y += n.y;
        avg_collision_normal->z += n.z;
        *avg_relative_speed += info.m_relative_normal_velocity;
        *avg_a_projected_speed += info.m_a_point_velocity.y * n.y + info.m_a_point_velocity.x * n.x + n.z * info.m_a_point_velocity.z;
        *avg_b_projected_speed += info.m_b_point_velocity.y * n.y + info.m_b_point_velocity.x * n.x + info.m_b_point_velocity.z * n.z;
    }
    const float scale = 1.0f / (float)event->m_num_collisions;
    avg_collision_point->x *= scale;
    avg_collision_point->y *= scale;
    avg_collision_point->z *= scale;
    avg_collision_normal->normalize_safe();
    *avg_relative_speed *= scale;
    *avg_a_projected_speed *= scale;
    *avg_b_projected_speed *= scale;
}

// 0x00810B30 (:1588)
bool havok_collision_events_object_in_contact(const object* p_object)
{
    Contact_event_wrapper.enter();
    const contact_node* list = havok_collision_events_get_contact_info(p_object)->m_contact_list;
    Contact_event_wrapper.leave();
    return list != nullptr;
}

// 0x00810B60 (:1606)
// Ends every contact of a freed object (both ways) and frees its contact nodes.
void havok_collision_events_object_freed(object* a_object)
{
    Contact_event_wrapper.enter();
    object_contact_info* cinfo = havok_collision_events_get_contact_info(a_object);
    contact_event ended_a;
    ended_a.m_type = contact_event::CONTACT_ENDED;
    ended_a.m_object_a = a_object;
    contact_event ended_b;
    ended_b.m_type = contact_event::CONTACT_ENDED;
    ended_b.m_object_b = a_object;
    contact_node* node = cinfo->m_contact_list;
    while (node != nullptr)
    {
        contact_node* next = node->next;
        contact_node* head = cinfo->m_contact_list;
        object* other = World.object_from_handle(node->m_contacted_object);
        ended_a.m_object_b = other;
        havok_collision_events_call_contact_callbacks(&ended_a);
        if (other != nullptr)
        {
            contact_node* other_node = other->contact_info.m_contact_list;
            while (other_node != nullptr)
            {
                contact_node* current = other_node;
                other_node = other_node->next != other->contact_info.m_contact_list ? other_node->next : nullptr;
                if (current->m_contacted_object == a_object->get_handle())
                {
                    ended_b.m_object_a = other;
                    havok_collision_events_call_contact_callbacks(&ended_b);
                    havok_collision_events_free_contact_node(&other->contact_info, current);
                }
            }
        }
        havok_collision_events_free_contact_node(cinfo, node);
        node = next != head ? next : nullptr;
    }
    Contact_event_wrapper.leave();
}

// 0x00810C60 (:1666)
// A collision event from game code (no thread or rigid body ordering); false when all events are in use.
bool havok_add_collision_event(object* obj_a, object* obj_b, const collision_event::collision_info& coll_info)
{
    return find_or_alloc_collision_event(obj_a, obj_b, coll_info, nullptr) != nullptr;
}

// 0x0080D4D0 (.h:274)
const object_contact_info* havok_collision_events_get_contact_info(const object* objp)
{
    return objp != nullptr ? &objp->contact_info : nullptr;
}

// 0x0080D4E0 (.h:282)
object_contact_info* havok_collision_events_get_contact_info(object* objp)
{
    return objp != nullptr ? &objp->contact_info : nullptr;
}
