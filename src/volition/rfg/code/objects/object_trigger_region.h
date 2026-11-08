// Original: volition/rfg/code/objects/object_trigger_region.h
// Implementation: volition/rfg/code/objects/object_trigger_region.cpp
//
// trigger_region, a box or sphere volume placed in zones (OT_TRIGGER_REGION) that fires script events when objects
// enter or leave it, or kills humans inside (kill types). trigger_region_do_frame runs once per frame. Placement of the
// enums inferred.
#pragma once

#include "volition/rfg/code/objects/obj_enums.h"
#include "volition/rfg/code/objects/object.h"
#include "volition/rfg/code/objects/object_type.h"
#include "volition/vlib/code/math/bbox.h"
#include "volition/vlib/code/math/vector3.h"

class matrix;

enum trigger_region_shape
{
    TRIGGER_REGION_SHAPE_BOX = 0,
    TRIGGER_REGION_SHAPE_SPHERE = 1,
    NUM_TRIGGER_REGION_SHAPES = 2,
};

enum trigger_region_type
{
    TRIGGER_REGION_TYPE_DEFAULT = 0,
    TRIGGER_REGION_TYPE_KILL_HUMAN = 1,
    NUM_TRIGGER_REGION_TYPES = 2,
};

enum trigger_region_kill_human_types
{
    TRKHT_INVALID = -1,
    TRKHT_CLIFF = 0,
    TRKHT_MINE = 1,
    NUM_TRIGGER_REGION_KILL_HUMAN_TYPES = 2,
};

// size 0xF0
class trigger_region : public object
{
public:
    trigger_region(); // 0x00A1DC60 (:98)
    static const object_type get_type_static(); // 0x00756BC0 (.h:54)
    static const object_sub_type get_subtype_static(); // 0x00756BD0 (.h:54)
    static const char* get_classname_static(); // 0x00A06A60 (.h:54)
    static int get_bpo_index_static(); // 0x00A06A70 (.h:54)
    virtual const char* get_classname() const; // vtbl+0x0C 0x00A1DC80 (.h:54)
    static unsigned int get_classname_hash(); // 0x00A1DC50 (:97)
    static int pool_remaining(); // 0x00A40CA0 (:97)
    virtual void pool_free(); // vtbl+0x80 0x00A952F0 (:97)
protected:
    virtual void free_this(); // vtbl+0x28 0x00A95300 (:238)
public:
    virtual bool serialize(obj_prop_block& pblock, bool load); // vtbl+0x14 0x00AAD690 (:110)
    virtual obj_serialize_mode serialize_mode(); // vtbl+0x18 0x00A1DCA0 (:195)
    virtual bool stream(bool load); // vtbl+0x1C 0x00A1DCB0 (:207)
    virtual void get_bbox(vector& bmin, vector& bmax) const; // vtbl+0x04 0x00A1DCD0 (:244)
    virtual void get_local_bbox(vector& bmin, vector& bmax) const; // vtbl+0x08 0x00A1DD10 (:251)
    virtual void update_pos_and_orient(const vector& update_pos, const matrix& update_orient, bool set_havok_data); // vtbl+0x30 0x00A1DD50 (:261)
    float get_radius() const; // 0x00A1DDE0 (:282)
    virtual void destroy_notify(); // vtbl+0x2C 0x00A1DDD0 (:272)
    void enable(); // 0x00A1DDF0 (:310)
    void disable(); // 0x00A1DE00 (:316)
    bool is_enabled() const; // 0x00A1DE10 (:322)
    trigger_region_type get_trigger_region_type() const; // 0x00A1DE50 (:339)
    trigger_region_shape get_trigger_region_shape() const; // 0x00A1DE60 (:344)
    trigger_region_kill_human_types get_trigger_region_kill_human_type() const; // 0x00A06A80 (.h:104)
    virtual ~trigger_region(); // vtbl+0x00 0x00A1DC90

private:
    bbox bb; // 0x9C
    bbox local_bb; // 0xB4
    float radius; // 0xCC
    trigger_region_shape tr_shape; // 0xD0
    bool enabled; // 0xD4
    trigger_region_type tr_type; // 0xD8
    trigger_region_kill_human_types tr_kill_type; // 0xDC
    int tr_flags; // 0xE0
public:
    vector last_pos; // 0xE4
};

void types_trigger_region(signed char& type, signed char& subtype); // 0x00A1DC20 (:97)
int bpo_index_trigger_region(); // 0x00A1DC30 (:97)
const char* classname_trigger_region(); // 0x00A1DC40 (:97)
object* alloc_trigger_region(); // 0x00A71130 (:97)
void trigger_region_system_init(); // 0x00A1DE70 (:368)
bool trigger_region_point_inside(const trigger_region* trigger, const vector& point); // 0x00A1DE90 (:405)
trigger_region_type trigger_region_type_by_name(const char* name); // 0x00A1DF10 (:423)
trigger_region_shape trigger_region_shape_by_name(const char* name); // 0x00A1DF50 (:435)
trigger_region_kill_human_types trigger_region_kill_human_type_by_name(const char* name); // 0x00A1DF90 (:447)
void trigger_region_do_frame(); // 0x00AD6600 (:460)
