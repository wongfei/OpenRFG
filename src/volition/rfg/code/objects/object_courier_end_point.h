// Original: volition/rfg/code/objects/object_courier_end_point.h
// Implementation: volition/rfg/code/objects/object_courier_end_point.cpp
//
// Courier end points: the end points of the courier chase activity, with their courier definitions (read from the
// activity table) and the patrol the courier drives.
#pragma once

#include "volition/rfg/code/objects/obj_enums.h"
#include "volition/rfg/code/objects/object.h"
#include "volition/rfg/code/objects/object_type.h"
#include "volition/vlib/code/time/timestamp.h"
#include "volition/vlib/code/util/varray.h"

class courier_definition;
class district;
class object_patrol;
class vector;

// size 0xA8
class object_courier_end_point : public object
{
public:
    object_courier_end_point(); // 0x00A16500 (:61)
    static const object_type get_type_static(); // 0x007544A0 (.h:109)
    static const object_sub_type get_subtype_static(); // 0x007544B0 (.h:109)
    static const char* get_classname_static(); // 0x00A16490 (.h:109)
    static int get_bpo_index_static(); // 0x00A164A0 (.h:109)
    virtual const char* get_classname() const; // vtbl+0x0C 0x00A16530 (.h:109)
    static unsigned int get_classname_hash(); // 0x00A164F0 (:60)
    static int pool_remaining(); // 0x00A3A3D0 (:60)
    virtual void pool_free(); // vtbl+0x80 0x00A8F260 (:60)
    virtual bool serialize(obj_prop_block& pblock, bool load); // vtbl+0x14 0x00AA5E60 (:70)
    virtual obj_serialize_mode serialize_mode(); // vtbl+0x18 0x00A16550 (:114)
protected:
    virtual void free_this(); // vtbl+0x28 0x00A8F270 (:65)
public:
    const courier_definition* get_definition() const;
    bool get_end_point(vector& courier_end) const; // 0x00A16570 (:125)
    bool find_courier_end_point(); // 0x00A16590 (:140)
    const district* get_end_district() const; // 0x00A165A0 (:183)
    bool get_completed_flag() const; // 0x007544C0 (.h:141)
    void set_completed_flag(bool new_status); // 0x007544D0 (.h:142)
    bool can_spawn() const; // 0x00A3A400 (:155)
    bool end_game_only_node() const; // 0x007544F0 (.h:159)
    virtual ~object_courier_end_point(); // vtbl+0x00 0x00A16540

    timestamp last_used_time; // 0x9C
    const courier_definition* courier_def; // 0xA0
private:
    struct
    {
        unsigned int completed : 1; // 0x00:0
        unsigned int end_game_only : 1; // 0x00:1
    } courier_node_flags; // 0xA4
};

object* alloc_object_courier_end_point(); // 0x00A623F0 (:60)
const char* classname_object_courier_end_point(); // 0x00A164E0 (:60)
int bpo_index_object_courier_end_point(); // 0x00A164D0 (:60)
void types_object_courier_end_point(signed char& type, signed char& subtype); // 0x00A164C0 (:60)
void courier_definitions_init(); // 0x00A8F280 (:203)
object_patrol* courier_find_patrol(const object_courier_end_point* objp); // 0x00A8F780 (:360)
const courier_definition* courier_definition_from_name(const char* name); // 0x00A3A460 (:379)
const char* const courier_get_name(const object_courier_end_point* const objp); // 0x00A165D0 (:394)
bool courier_end_get_all_points(farray<object_courier_end_point const*,16>& end_points); // 0x00A62400 (:404)
void courier_end_find_end_points(); // 0x00A165F0 (:418)

// 0x007544A0 (.h:109)
inline const object_type object_courier_end_point::get_type_static()
{
    return OT_COURIER_END;
}

// 0x007544B0 (.h:109)
inline const object_sub_type object_courier_end_point::get_subtype_static()
{
    return OT_SUB_UNDEFINED;
}

// 0x007544C0 (.h:141)
inline bool object_courier_end_point::get_completed_flag() const
{
    return courier_node_flags.completed != 0;
}
