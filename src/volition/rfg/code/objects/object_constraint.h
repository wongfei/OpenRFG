// Original: volition/rfg/code/objects/object_constraint.h
// Implementation: volition/rfg/code/objects/object_constraint.cpp
//
// Placed constraints (OT_CONSTRAINT): a Havok constraint between a host and a child object built from a
// constraint_template, linked into both objects' constraint lists so it can be transferred when an object is
// replaced (object_constraint_transfer); obj_constraint_props creates one.
#pragma once

#include "volition/rfg/code/havok/constraint_template.h"
#include "volition/rfg/code/objects/obj_enums.h"
#include "volition/rfg/code/objects/object.h"
#include "volition/rfg/code/objects/object_type.h"

class hkpConstraintInstance;
class hkpRigidBody;
class matrix;
class vector;

// size 0x164
class constraint : public object
{
public:
    constraint(); // 0x00A15EE0 (:76)
    static const object_type get_type_static(); // 0x0095BA40 (.h:22)
    static const object_sub_type get_subtype_static(); // 0x0095BA50 (.h:22)
    static const char* get_classname_static(); // 0x00A05DF0 (.h:22)
    static int get_bpo_index_static(); // 0x00A05E00 (.h:22)
    virtual const char* get_classname() const; // vtbl+0x0C 0x00A15F10 (.h:22)
    static unsigned int get_classname_hash(); // 0x00A15ED0 (:75)
    static int pool_remaining(); // 0x00A39660 (:75)
    virtual void pool_free(); // vtbl+0x80 0x00A8DD80 (:75)
    virtual bool serialize(obj_prop_block& pblock, bool load); // vtbl+0x14 0x00AA4F70 (:88)
    virtual obj_serialize_mode serialize_mode(); // vtbl+0x18 0x00A15F30 (:234)
    virtual bool stream(bool load); // vtbl+0x1C 0x00A39690 (:252)
    virtual void get_bbox(vector& bmin, vector& bmax) const; // vtbl+0x04 0x00A15F50 (:390)
    virtual void get_local_bbox(vector& bmin, vector& bmax) const; // vtbl+0x08 0x00A15FA0 (:402)
protected:
    virtual void free_this(); // vtbl+0x28 0x00AA5470 (:439)
    virtual void destroy_notify(); // vtbl+0x2C 0x00A39AF0 (:410)
private:
    void cleanup_hk_constraint(); // 0x00A15FE0 (:417)
public:
    virtual ~constraint(); // vtbl+0x00 0x00A15F20

    unsigned int host_handle; // 0x9C
    unsigned int child_handle; // 0xA0
    int host_attach_index; // 0xA4
    int child_attach_index; // 0xA8
    int host_alt_hk_body_index; // 0xAC
    int child_alt_hk_body_index; // 0xB0
    constraint_template constraint_info; // 0xB4
    hkpConstraintInstance* hk_constraint; // 0x150
    constraint* child_constraint_prev; // 0x154
    constraint* child_constraint_next; // 0x158
    constraint* host_constraint_prev; // 0x15C
    constraint* host_constraint_next; // 0x160
};

// size 0xC
class obj_constraint_props : public obj_prop_block
{
public:
    obj_constraint_props(constraint_template* ct, const unsigned int host_handle, const unsigned int child_handle, const vector& pos, const matrix& orient, const int host_attach_index, const int child_attach_index, const int host_alt_hk_body_index, const int child_alt_hk_body_index); // 0x00745010 (.h:73)
    ~obj_constraint_props(); // 0x007428A0

};

void types_constraint(signed char& type, signed char& subtype); // 0x00A15EA0 (:75)
int bpo_index_constraint(); // 0x00A15EB0 (:75)
object* alloc_constraint(); // 0x00A617E0 (:75)
const char* classname_constraint(); // 0x00A15EC0 (:75)
void object_constraint_transfer(bool transfer_host, constraint* cp, object* other_objp, object* old_objp, object* new_objp, hkpRigidBody* new_objp_rb); // 0x00AA55A0 (:477)
const char* object_constraint_get_name(constraint* cp); // 0x00A16010 (:599)
