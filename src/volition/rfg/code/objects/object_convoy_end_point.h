// Original: volition/rfg/code/objects/object_convoy_end_point.h
// Implementation: volition/rfg/code/objects/object_convoy_end_point.cpp
//
// Convoy end points: the start and end points of convoy activities, with their convoy definitions and home district.
#pragma once

#include "volition/rfg/code/objects/obj_enums.h"
#include "volition/rfg/code/objects/object.h"
#include "volition/rfg/code/objects/object_type.h"
#include "volition/vlib/code/time/timestamp.h"
#include "volition/vlib/code/util/varray.h"

class convoy_definition;
class district;
class vector;

// size 0xAC
class object_convoy_end_point : public object
{
public:
    object_convoy_end_point(); // 0x00A163A0 (:16)
    static const object_type get_type_static(); // 0x00754E30 (.h:24)
    static const object_sub_type get_subtype_static(); // 0x00754E40 (.h:24)
    static const char* get_classname_static(); // 0x00A16070 (.h:24)
    static int get_bpo_index_static(); // 0x00A16080 (.h:24)
    virtual const char* get_classname() const; // vtbl+0x0C 0x00A163D0 (.h:24)
    static unsigned int get_classname_hash(); // 0x00A16390 (:15)
    static int pool_remaining(); // 0x00A3A330 (:15)
    virtual void pool_free(); // vtbl+0x80 0x00A8F240 (:15)
    virtual bool serialize(obj_prop_block& pblock, bool load); // vtbl+0x14 0x00AA5BE0 (:30)
    virtual obj_serialize_mode serialize_mode(); // vtbl+0x18 0x00A163F0 (:25)
protected:
    virtual void free_this(); // vtbl+0x28 0x00A8F250 (:20)
public:
    bool get_end_point(vector& convoy_end) const; // 0x00A16400 (:113)
    bool find_convoy_end_point(); // 0x00A16420 (:128)
    const district* get_end_district() const; // 0x00A16430 (:171)
    bool get_completed_flag() const; // 0x00754E50 (.h:53)
    void set_completed_flag(bool new_status); // 0x00754E60 (.h:54)
    bool can_spawn() const; // 0x00A3A360 (:143)
    void disable_convoy(); // 0x00B1FC70 (.h:68)
    void enable_convoy(); // 0x00754E80 (.h:69)
    void suspend_convoy(); // 0x00B1FC80 (.h:72)
    void release_convoy(); // 0x00B1FC90 (.h:73)
    bool is_disabled() const;
    bool is_suspended() const;
    bool is_suspended_or_disabled(); // 0x00754E90 (.h:82)
    unsigned int get_home_district_handle() const; // 0x00754EB0 (.h:85)
    virtual ~object_convoy_end_point(); // vtbl+0x00 0x00A163E0

    timestamp last_used_time; // 0x9C
    const convoy_definition* convoy_def; // 0xA0
private:
    unsigned int home_district_handle; // 0xA4
    struct
    {
        unsigned int completed : 1; // 0x00:0
        unsigned int disabled : 1; // 0x00:1
        unsigned int suspend : 1; // 0x00:2
    } convoy_node_flags; // 0xA8
};

void types_object_convoy_end_point(signed char& type, signed char& subtype); // 0x00A16360 (:15)
int bpo_index_object_convoy_end_point(); // 0x00A16370 (:15)
const char* classname_object_convoy_end_point(); // 0x00A16380 (:15)
object* alloc_object_convoy_end_point(); // 0x00A62340 (:15)
bool convoy_end_get_all_points(farray<object_convoy_end_point const*,16>& end_points); // 0x00A62350 (:193)
void convoy_end_find_end_points(); // 0x00A16460 (:207)
const char* const convoy_end_point_get_name(const object_convoy_end_point* const ocep); // 0x00A16470 (:220)

// 0x00754E30 (.h:24)
inline const object_type object_convoy_end_point::get_type_static()
{
    return OT_CONVOY_END;
}

// 0x00754E40 (.h:24)
inline const object_sub_type object_convoy_end_point::get_subtype_static()
{
    return OT_SUB_UNDEFINED;
}

// 0x00754E50 (.h:53)
inline bool object_convoy_end_point::get_completed_flag() const
{
    return convoy_node_flags.completed != 0;
}
