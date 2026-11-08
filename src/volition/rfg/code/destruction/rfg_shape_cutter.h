// Original: volition/rfg/code/destruction/rfg_shape_cutter.h
// Implementation: volition/rfg/code/destruction/rfg_shape_cutter.cpp
//
// shape_cutter, a short-lived object that applies an explosion's destruction to the movers around it after a delay
// (do_cut: explosion damage, or link breaking only), expanding a number of times; created from shape_cutter_props.
#pragma once

#include "volition/rfg/code/objects/object.h"
#include "volition/rfg/code/objects/object_type.h"
#include "volition/vlib/code/time/timestamp.h"

class matrix;
class vector;
struct weapon_info;

// size 0xC
class shape_cutter_props : public obj_prop_block
{
public:
    shape_cutter_props(const vector& pos, const matrix& orient, const unsigned char exp_info, const weapon_info* w_info, const unsigned int source_handle, const unsigned int owner_handle); // 0x006E75E0 (.h:95)
    shape_cutter_props(const vector& pos, const matrix& orient, const float radius, const unsigned char exp_info, const unsigned int source_handle, const unsigned int owner_handle); // 0x0071B8C0 (.h:78)
    ~shape_cutter_props(); // 0x006E1DE0

};

// size 0xC0
class shape_cutter : public object
{
public:
    shape_cutter(); // 0x00717EE0 (:132)
    static const object_type get_type_static(); // 0x00711600 (.h:31)
    static const object_sub_type get_subtype_static(); // 0x00711610 (.h:31)
    static const char* get_classname_static(); // 0x00711620 (.h:31)
    static int get_bpo_index_static(); // 0x00711630 (.h:31)
    virtual const char* get_classname() const; // vtbl+0x0C 0x00717F10 (.h:31)
    static unsigned int get_classname_hash(); // 0x00717ED0 (:131)
    static int pool_remaining(); // 0x00720570 (:131)
    virtual void pool_free(); // vtbl+0x80 0x007302B0 (:131)
private:
    void apply_explosion(); // 0x007205A0 (:274)
    void apply_link_break(float radiusp); // 0x00720A70 (:368)
public:
    bool do_cut(); // 0x00728400 (:179)
    virtual bool serialize(obj_prop_block& pblock, bool load); // vtbl+0x14 0x00728820 (:472)
    virtual bool stream(bool load); // vtbl+0x1C 0x00717F30 (:563)
    virtual void get_bbox(vector& bmin, vector& bmax) const; // vtbl+0x04 0x00717F70 (:585)
    virtual void get_local_bbox(vector& bmin, vector& bmax) const; // vtbl+0x08 0x00718040 (:593)
    virtual ~shape_cutter(); // vtbl+0x00 0x00717F20

    float radius; // 0x9C
    timestamp delay; // 0xA0
    timestamp lifetime; // 0xA4
    unsigned char exp_info_unique_id; // 0xA8
    unsigned int source_handle; // 0xAC
    unsigned int owner_handle; // 0xB0
    unsigned char flags; // 0xB4
    unsigned char apply_impulse_no_delete; // 0xB5
    unsigned char break_links_only; // 0xB6
    int num_expansions; // 0xB8
    const weapon_info* w_info; // 0xBC
};

void unused_dcf_shape_cutter_test(); // 0x007376A0 (:55)
void types_shape_cutter(signed char& type, signed char& subtype); // 0x00717EA0 (:131)
int bpo_index_shape_cutter(); // 0x00717EB0 (:131)
const char* classname_shape_cutter(); // 0x00717EC0 (:131)
object* alloc_shape_cutter(); // 0x007283F0 (:131)
void shape_cutter_do_frame(); // 0x007302C0 (:137)
