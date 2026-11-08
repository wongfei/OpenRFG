// Original: volition/rfg/code/objects/object_constraint.cpp
//
// Placed constraints (multiplayer.md MP8: a projectile sticks to what it hits through one): the class registration,
// the serialization (the template, the host and child links, the child parented to the host), the stream-in that
// builds the Havok constraint, freeing (attached projectiles come loose) and the transfer to a replacement object.
// The Havok constraint creation functions (havok.cpp) are not ported; they stay test-side stand-ins that return no
// constraint, so a placed constraint only links and parents its objects. Compiled in rfg_objects_unity.cpp.
// The external interface is declared in object_constraint.h.

#include "volition/rfg/code/objects/object_constraint.h"

#include "volition/rfg/code/code_effects/material_effects.h"
#include "volition/rfg/code/code_effects/material_effects_shared.h"
#include "volition/rfg/code/effects/object_effect.h"
#include "volition/rfg/code/geom/character_instance.h"
#include "volition/rfg/code/geom/cloth_simulation.h"
#include "volition/rfg/code/havok/havok.h"
#include "volition/rfg/code/level/world.h"
#include "volition/rfg/code/objects/object_item.h"
#include "volition/rfg/code/objects/object_prop_names_shared.h"
#include "volition/rfg/code/objects/projectile.h"
#include "volition/vlib/code/math/bbox.h"

#include "keen/base/trace.hpp"

#include <stdio.h>

// ---- file-static globals
static object_pool<constraint,512,1> constraintpool; // 0x03506CA0
// read from rfg.exe: pool usage 0x2D240 bytes, 512 constraints.
obj_class_instantiator constraint_instantiator("constraint", alloc_constraint, types_constraint, bpo_index_constraint, classname_constraint, constraint::pool_remaining, 0x2D240, 512); // 0x033C67DC

// Adds cp to the circular child or host constraint list of objp (inlined everywhere).
static void constraint_list_add_child(object* objp, constraint* cp)
{
    if (objp->child_constraint_ptr == nullptr)
    {
        cp->child_constraint_next = cp;
        cp->child_constraint_prev = cp;
        objp->child_constraint_ptr = cp;
        return;
    }
    cp->child_constraint_prev = objp->child_constraint_ptr->child_constraint_prev;
    cp->child_constraint_next = objp->child_constraint_ptr;
    objp->child_constraint_ptr->child_constraint_prev->child_constraint_next = cp;
    objp->child_constraint_ptr->child_constraint_prev = cp;
}

static void constraint_list_add_host(object* objp, constraint* cp)
{
    if (objp->host_constraint_ptr == nullptr)
    {
        cp->host_constraint_next = cp;
        cp->host_constraint_prev = cp;
        objp->host_constraint_ptr = cp;
        return;
    }
    cp->host_constraint_prev = objp->host_constraint_ptr->host_constraint_prev;
    cp->host_constraint_next = objp->host_constraint_ptr;
    objp->host_constraint_ptr->host_constraint_prev->host_constraint_next = cp;
    objp->host_constraint_ptr->host_constraint_prev = cp;
}

static void constraint_list_remove_child(object* objp, constraint* cp)
{
    if (objp->child_constraint_ptr == cp)
    {
        if (cp->child_constraint_next == cp)
        {
            cp->child_constraint_next = nullptr;
            cp->child_constraint_prev = nullptr;
            objp->child_constraint_ptr = nullptr;
            return;
        }
        objp->child_constraint_ptr = cp->child_constraint_next;
    }
    cp->child_constraint_prev->child_constraint_next = cp->child_constraint_next;
    cp->child_constraint_next->child_constraint_prev = cp->child_constraint_prev;
    cp->child_constraint_next = nullptr;
    cp->child_constraint_prev = nullptr;
}

static void constraint_list_remove_host(object* objp, constraint* cp)
{
    if (objp->host_constraint_ptr == cp)
    {
        if (cp->host_constraint_next == cp)
        {
            cp->host_constraint_next = nullptr;
            cp->host_constraint_prev = nullptr;
            objp->host_constraint_ptr = nullptr;
            return;
        }
        objp->host_constraint_ptr = cp->host_constraint_next;
    }
    cp->host_constraint_prev->host_constraint_next = cp->host_constraint_next;
    cp->host_constraint_next->host_constraint_prev = cp->host_constraint_prev;
    cp->host_constraint_next = nullptr;
    cp->host_constraint_prev = nullptr;
}

// 0x00AA4F50 (:61): a projectile freed from its constraint comes loose.
static void constraint_cleanup(object* objp)
{
    if (objp != nullptr && objp->get_obj_type() == OT_ITEM && objp->get_sub_type() == OT_SUB_ITEM_PROJECTILE)
    {
        ((projectile*)objp)->detach();
    }
}

// ---- class registration (:75)
// 0x0095BA40 (.h:22)
const object_type constraint::get_type_static()
{
    return OT_CONSTRAINT;
}

// 0x0095BA50 (.h:22)
const object_sub_type constraint::get_subtype_static()
{
    return OT_SUB_UNDEFINED;
}

// 0x00A05DF0 (.h:22)
const char* constraint::get_classname_static()
{
    return "constraint";
}

// 0x00A05E00 (.h:22)
int constraint::get_bpo_index_static()
{
    return 1;
}

// 0x00A15F10 (.h:22)
const char* constraint::get_classname() const
{
    return "constraint";
}

// 0x00A617E0 (:75)
object* alloc_constraint()
{
    return constraintpool.alloc(0);
}

// 0x00A15ED0 (:75)
unsigned int constraint::get_classname_hash()
{
    return constraint_instantiator.this_hash;
}

// 0x00A39660 (:75)
int constraint::pool_remaining()
{
    return constraintpool.get_free();
}

// 0x00A15EA0 (:75)
void types_constraint(signed char& type, signed char& subtype)
{
    type = OT_CONSTRAINT;
    subtype = OT_SUB_UNDEFINED;
}

// 0x00A15EC0 (:75)
const char* classname_constraint()
{
    return "constraint";
}

// 0x00A15EB0 (:75)
int bpo_index_constraint()
{
    return 1;
}

// 0x00A8DD80 (:75)
void constraint::pool_free()
{
    constraintpool.free(this);
}

// 0x00A15EE0 (:76): the template member resets itself.
constraint::constraint()
{
}

// 0x00A15F20
constraint::~constraint()
{
}

// 0x00AA4F70 (:88): saves the template and links; on load the child (or a motor's constrained object) gets the
// constraint as its parent child and its list entry, the host its list entry, and the child is parented to the host.
// Constraints made at run time are not saved.
bool constraint::serialize(obj_prop_block& pblock, bool load)
{
    if (!object::serialize(pblock, load))
    {
        return false;
    }
    if (!load)
    {
        pblock.set<constraint_template>(PROP_CONSTRAINT_TEMPLATE, constraint_info);
        pblock.set<unsigned int>(PROP_CONSTRAINT_HOST_HANDLE, host_handle);
        pblock.set<unsigned int>(PROP_CONSTRAINT_CHILD_HANDLE, child_handle);
        pblock.set<int>(PROP_CONSTRAINT_HOST_ATTACH_INDEX, host_attach_index);
        pblock.set<int>(PROP_CONSTRAINT_CHILD_ATTACH_INDEX, child_attach_index);
        pblock.set<int>(PROP_CONSTRAINT_HOST_HK_BODY_HANDLE_INDEX, host_alt_hk_body_index);
        pblock.set<int>(PROP_CONSTRAINT_CHILD_HK_BODY_HANDLE_INDEX, child_alt_hk_body_index);
        return true;
    }
    pblock.get<constraint_template>(PROP_CONSTRAINT_TEMPLATE, constraint_info);
    pblock.get<unsigned int>(PROP_CONSTRAINT_HOST_HANDLE, host_handle);
    pblock.get<unsigned int>(PROP_CONSTRAINT_CHILD_HANDLE, child_handle);
    if (child_handle == host_handle)
    {
        World.destroy_object(this, false);
    }
    child_attach_index = -1;
    host_attach_index = -1;
    pblock.get<int>(PROP_CONSTRAINT_HOST_ATTACH_INDEX, host_attach_index);
    pblock.get<int>(PROP_CONSTRAINT_CHILD_ATTACH_INDEX, child_attach_index);
    child_alt_hk_body_index = -1;
    host_alt_hk_body_index = -1;
    pblock.get<int>(PROP_CONSTRAINT_HOST_HK_BODY_HANDLE_INDEX, host_alt_hk_body_index);
    pblock.get<int>(PROP_CONSTRAINT_CHILD_HK_BODY_HANDLE_INDEX, child_alt_hk_body_index);
    hk_constraint = nullptr;
    object* host = World.object_from_handle(host_handle);
    if (host == nullptr)
    {
        host_handle = 0xFFFFFFFF;
    }
    object* child = World.object_from_handle(child_handle);
    if (child == nullptr)
    {
        return false;
    }
    // A motor drives the constraint its child is held by; it hangs off that constraint.
    object* parent_obj = child;
    unsigned int expected_parent = child_handle;
    if (constraint_info.type == HAVOK_CONSTRAINT_MOTOR && child->child_constraint_ptr != nullptr)
    {
        parent_obj = child->child_constraint_ptr->child_constraint_prev;
        expected_parent = parent_obj->get_handle();
    }
    constraint_list_add_child(parent_obj, this);
    if (get_parent() != 0xFFFFFFFF && get_parent() != expected_parent && !remove_from_parent())
    {
        char message[128];
        sprintf(message, "Constraint has an invalid parent assigned: 0x%x, should be 0x%x", get_parent(), expected_parent);
        keen::debug::nativePrintArgs(message);
        World.destroy_object(this, false);
        return false;
    }
    if (get_parent() == 0xFFFFFFFF)
    {
        parent_obj->set_child(this);
    }
    if (host != nullptr)
    {
        constraint_list_add_host(host, this);
        if (child->get_parent() == 0xFFFFFFFF)
        {
            host->set_child(child);
        }
    }
    if (!obj_flags.original)
    {
        obj_flags.no_save = 1;
        obj_flags.destroy_on_stream = 1;
    }
    return true;
}

// 0x00A15F30 (:234): fake constraints are not saved.
obj_serialize_mode constraint::serialize_mode()
{
    return (obj_serialize_mode)(constraint_info.type != HAVOK_CONSTRAINT_FAKE);
}

// 0x00A39690 (:252): the Havok constraint between the host's and the child's bodies (or their alt bodies), placed at
// the child's transform of body a; the attach indices are found on the first stream-in. Without a host only motors
// stay. Unloading releases the Havok constraint.
bool constraint::stream(bool load)
{
    if (!load)
    {
        if (obj_flags.destroy_on_stream)
        {
            World.destroy_object(this, false);
        }
        cleanup_hk_constraint();
        return true;
    }
    object* host = World.object_from_handle(host_handle);
    object* child = World.object_from_handle(child_handle);
    if (((host == nullptr || host->obj_flags.destroyed) && constraint_info.type != HAVOK_CONSTRAINT_MOTOR) || child == nullptr || child->obj_flags.destroyed)
    {
        World.destroy_object(this, false);
        return false;
    }
    const vector constraint_pos = constraint_info.body_a_transform.m_translation.copy_unrotate(child->orient) + child->pos;
    matrix constraint_orient = constraint_info.body_a_transform.m_rotation * child->orient;
    unsigned int host_body = 0xFFFFFFFF;
    if (child_attach_index == -1)
    {
        child_attach_index = child->attach_index(constraint_pos, child->havok_handle);
    }
    if (host != nullptr)
    {
        if (host_attach_index == -1)
        {
            const vector host_pos = host->pos + constraint_info.body_b_transform.m_translation.copy_unrotate(host->orient);
            host_attach_index = host->attach_index(host_pos, host->havok_handle);
        }
        if (host_alt_hk_body_index == -1 || (host_body = host->get_alt_hk_body_handle(host_alt_hk_body_index)) == 0xFFFFFFFF)
        {
            host_body = host->havok_handle;
        }
    }
    unsigned int child_body;
    if (child_alt_hk_body_index == -1 || (child_body = child->get_alt_hk_body_handle(child_alt_hk_body_index)) == 0xFFFFFFFF)
    {
        child_body = child->havok_handle;
    }
    update_pos_and_orient(constraint_pos, constraint_orient, true);
    switch (constraint_info.type)
    {
    case HAVOK_CONSTRAINT_POINT:
        hk_constraint = havok_create_point_constraint(host_body, child_body, constraint_pos, constraint_info.threshold);
        return true;
    case HAVOK_CONSTRAINT_HINGE:
        hk_constraint = havok_create_hinge_constraint(host_body, child_body, constraint_pos, constraint_orient.uvec, constraint_info.hinge_data.limited != 0, constraint_info.hinge_data.limit_min_angle,
                                                      constraint_info.hinge_data.limit_max_angle, constraint_info.threshold);
        break;
    case HAVOK_CONSTRAINT_PRISMATIC:
        hk_constraint = havok_create_slide_constraint(host_body, child_body, constraint_orient.uvec, constraint_info.prismatic_data.limit_min, constraint_info.prismatic_data.limit_max,
                                                      constraint_info.threshold);
        break;
    case HAVOK_CONSTRAINT_RAGDOLL:
    {
        // Original: placed ragdoll constraints are not supported (an uninitialized info is passed).
        keen::debug::nativePrintArgs("Int3 critical function at %s(%d)\n");
        ragdoll_constraint_info rci;
        hk_constraint = havok_create_ragdoll_constraint(host_body, child_body, rci, constraint_info.point_data.y_limit_max, constraint_info.point_data.z_limit_min, constraint_info.threshold);
        return true;
    }
    case HAVOK_CONSTRAINT_MOTOR:
    {
        // The motor drives the constraint holding its child (the last one of the child's list).
        constraint* driven = child->child_constraint_ptr != nullptr ? child->child_constraint_ptr->child_constraint_prev : nullptr;
        if (driven == this)
        {
            if (child->get_obj_type() != OT_CONSTRAINT)
            {
                return false;
            }
            driven = (constraint*)child;
        }
        if (driven == nullptr || driven->constraint_info.type == HAVOK_CONSTRAINT_MOTOR)
        {
            return false;
        }
        havok_constraint_add_motor(driven->hk_constraint, constraint_info.motor_data.angular_speed, constraint_info.motor_data.gain);
        object* driven_parent = World.object_from_handle(driven->get_parent());
        if (driven_parent != nullptr)
        {
            havok_body_force_activate(driven_parent->havok_handle);
        }
        return true;
    }
    default:
        return true;
    }
    // Hinges and prismatic constraints: the friction of the limit.
    const float friction = constraint_info.prismatic_data.limit_friction;
    if (hk_constraint != nullptr && friction > 0.0f)
    {
        havok_constraint_set_friction(hk_constraint, friction);
    }
    return true;
}

// 0x00A15F50 (:390)
void constraint::get_bbox(vector& bmin, vector& bmax) const
{
    vector local_min;
    vector local_max;
    get_local_bbox(local_min, local_max);
    bbox_instance(bmin, bmax, orient, pos, local_min, local_max);
}

// 0x00A15FA0 (:402): a 1 m cube.
void constraint::get_local_bbox(vector& bmin, vector& bmax) const
{
    bmin.x = -0.5f;
    bmin.y = -0.5f;
    bmin.z = -0.5f;
    bmax.x = 0.5f;
    bmax.y = 0.5f;
    bmax.z = 0.5f;
}

// 0x00A39AF0 (:410)
void constraint::destroy_notify()
{
    remove_constraints();
    cleanup_hk_constraint();
}

// 0x00A15FE0 (:417)
void constraint::cleanup_hk_constraint()
{
    hkpConstraintInstance* ci = hk_constraint;
    if (ci != nullptr)
    {
        hk_constraint = nullptr;
        havok_constraint_remove_motor(ci);
        havok_constraint_release(ci);
    }
}

// 0x00AA5470 (:439): unlinks from the child (its parent) and the host; the child leaves the host; projectiles on
// either end come loose.
void constraint::free_this()
{
    object* child = World.object_from_handle(get_parent());
    if (child != nullptr && child->child_constraint_ptr != nullptr)
    {
        constraint_list_remove_child(child, this);
    }
    object* host = World.object_from_handle(host_handle);
    if (host != nullptr)
    {
        if (host->host_constraint_ptr != nullptr)
        {
            constraint_list_remove_host(host, this);
        }
        host->remove_child(child);
        constraint_cleanup(host);
    }
    constraint_cleanup(child);
    object::free_this();
}

// 0x00AA55A0 (:477): moves a constraint to the object replacing one of its ends (a destroyed mover's piece, a new
// body): the lists, the parenting and the Havok constraint follow; without a body the constraint goes. A host change
// removes a motor; an item with a cloth simulation loses its constraint (and, flagged, errodes with the
// reconstructor's shards); a fake constraint's effect moves to the new host.
void object_constraint_transfer(bool transfer_host, constraint* cp, object* other_objp, object* old_objp, object* new_objp, hkpRigidBody* new_objp_rb)
{
    if (old_objp == nullptr || new_objp == nullptr || old_objp->get_handle() != new_objp->get_handle())
    {
        cp->obj_flags.no_save = 1;
        cp->obj_flags.destroy_on_stream = 1;
    }
    if (!transfer_host)
    {
        if (old_objp != new_objp)
        {
            constraint_list_remove_child(old_objp, cp);
            old_objp->remove_child(cp);
            constraint_list_add_child(new_objp, cp);
            new_objp->set_child(cp);
            other_objp->remove_child(old_objp);
            if (new_objp->get_parent() == 0xFFFFFFFF)
            {
                other_objp->set_child(new_objp);
            }
        }
        cp->child_handle = new_objp->get_handle();
        if (new_objp->havok_handle != 0xFFFFFFFF && cp->hk_constraint != nullptr)
        {
            if (new_objp_rb == nullptr)
            {
                new_objp_rb = havok_body_get_pointer(new_objp->havok_handle);
            }
            hkpConstraintInstance* ci = cp->hk_constraint;
            cp->hk_constraint = nullptr;
            cp->hk_constraint = havok_constraint_transfer(ci, new_objp_rb, (hkpRigidBody*)ci->m_entities[1]);
            return;
        }
        World.destroy_object(cp, false);
        return;
    }
    if (havok_constraint_has_motor(cp->hk_constraint))
    {
        havok_constraint_remove_motor(cp->hk_constraint);
        World.destroy_object(cp->child_constraint_ptr, false);
    }
    if (old_objp != new_objp)
    {
        constraint_list_remove_host(old_objp, cp);
        constraint_list_add_host(new_objp, cp);
        if (old_objp->remove_child(other_objp))
        {
            new_objp->set_child(other_objp);
        }
    }
    if (other_objp != nullptr && other_objp->get_obj_type() == OT_ITEM && ((item*)other_objp)->cloth_sim != nullptr)
    {
        item* ip = (item*)other_objp;
        World.destroy_object(cp, false);
        ip->cloth_deletion_timer.set(0);
        if (ip->cloth_sim->do_explosion_gust)
        {
            // Magic number: a 2.2 s errode.
            const material_effect_parameters params = material_effects_get_parameters(1, 0x898);
            if (ip->char_inst->rdd.is_loaded())
            {
                material_effects_apply((rl_renderable_instance*)ip->char_inst->rdd.get_data()->renderable_instance_p, material_effects_get_reconstructor_shards_id(), &params);
                ip->item_flags.doing_errode = 1;
            }
        }
    }
    cp->host_handle = new_objp->get_handle();
    if (cp->hk_constraint == nullptr)
    {
        if (cp->constraint_info.type == HAVOK_CONSTRAINT_FAKE && other_objp->get_obj_type() == OT_EFFECT)
        {
            ((object_effect*)other_objp)->attach_to_host(new_objp->get_handle(), -1);
        }
        return;
    }
    if (new_objp->havok_handle != 0xFFFFFFFF)
    {
        if (new_objp_rb == nullptr)
        {
            new_objp_rb = havok_body_get_pointer(new_objp->havok_handle);
        }
        hkpConstraintInstance* ci = cp->hk_constraint;
        cp->hk_constraint = nullptr;
        cp->hk_constraint = havok_constraint_transfer(ci, (hkpRigidBody*)ci->m_entities[0], new_objp_rb);
        return;
    }
    World.destroy_object(cp, false);
}

// 0x00A16010 (:599)
const char* object_constraint_get_name(constraint* cp)
{
    switch (cp->constraint_info.type)
    {
    case HAVOK_CONSTRAINT_POINT:
        return "point";
    case HAVOK_CONSTRAINT_HINGE:
        return "hinge";
    case HAVOK_CONSTRAINT_PRISMATIC:
        return "prismatic";
    case HAVOK_CONSTRAINT_RAGDOLL:
        return "ragdoll";
    case HAVOK_CONSTRAINT_MOTOR:
        return "motor";
    case HAVOK_CONSTRAINT_FAKE:
        return "fake";
    default:
        return "INVALID";
    }
}

// 0x00745010 (.h:73): the constraint's template and links at its position.
obj_constraint_props::obj_constraint_props(constraint_template* ct, const unsigned int host_handle, const unsigned int child_handle, const vector& pos, const matrix& orient, const int host_attach_index,
                                           const int child_attach_index, const int host_alt_hk_body_index, const int child_alt_hk_body_index)
    : obj_prop_block(pos, orient, nullptr)
{
    set<constraint_template>(PROP_CONSTRAINT_TEMPLATE, *ct);
    set<unsigned int>(PROP_CONSTRAINT_HOST_HANDLE, host_handle);
    set<unsigned int>(PROP_CONSTRAINT_CHILD_HANDLE, child_handle);
    set<int>(PROP_CONSTRAINT_HOST_ATTACH_INDEX, host_attach_index);
    set<int>(PROP_CONSTRAINT_CHILD_ATTACH_INDEX, child_attach_index);
    set<int>(PROP_CONSTRAINT_HOST_HK_BODY_HANDLE_INDEX, host_alt_hk_body_index);
    set<int>(PROP_CONSTRAINT_CHILD_HK_BODY_HANDLE_INDEX, child_alt_hk_body_index);
}

// 0x007428A0: the base destructors release the property buffer.
obj_constraint_props::~obj_constraint_props()
{
}
