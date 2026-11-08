// Original: volition/rfg/code/objects/object_delivery_node.h
// Implementation: volition/rfg/code/objects/object_delivery_node.cpp
//
// Delivery nodes: the paired start and end nodes of the delivery activity, with their definitions and best times.
#pragma once

#include "volition/rfg/code/objects/obj_enums.h"
#include "volition/rfg/code/objects/object.h"
#include "volition/rfg/code/objects/object_type.h"
#include "volition/vlib/code/util/varray.h"

class base_delivery_definition_data;

// size 0xB4
class object_delivery_node : public object
{
public:
    // size 0x1
    struct delivery_state_flags
    {
        bool discovered : 1; // 0x00:0
        bool completed : 1; // 0x00:1
        bool disabled_until_stream : 1; // 0x00:2
        bool beat_par_time : 1; // 0x00:3
    };

    object_delivery_node(); // 0x00A17920 (:25)
    static const object_type get_type_static(); // 0x00754430 (.h:24)
    static const object_sub_type get_subtype_static(); // 0x00754440 (.h:24)
    static const char* get_classname_static(); // 0x00A178A0 (.h:24)
    static int get_bpo_index_static(); // 0x00A178B0 (.h:24)
    virtual const char* get_classname() const; // vtbl+0x0C 0x00A17940 (.h:24)
    static unsigned int get_classname_hash(); // 0x00A17910 (:24)
    static int pool_remaining(); // 0x00A3AEF0 (:24)
    virtual void pool_free(); // vtbl+0x80 0x00A90010 (:24)
    virtual bool serialize(obj_prop_block& pblock, bool load); // vtbl+0x14 0x00AA68D0 (:29)
    virtual bool stream(bool load); // vtbl+0x1C 0x00A17960 (:77)
    virtual obj_serialize_mode serialize_mode(); // vtbl+0x18 0x00A3AF20 (:100)
    const object_delivery_node* find_pair_node() const; // 0x00A90020 (:110)
    bool get_best_time(unsigned int& best_time); // 0x00A17A20 (:193)
    void set_disabled(bool disabled); // 0x00A179C0 (:154)
    bool is_disabled(); // 0x00A179E0 (:160)
    bool is_completed(); // 0x00A17A00 (:171)
    void set_completed_flag(bool);
    bool has_beat_par_time(); // 0x00754450 (.h:59)
    void set_playable(bool);
    void record_victory(unsigned int completion_time, unsigned int par_time); // 0x00A17A60 (:208)
    const base_delivery_definition_data* get_delivery_def() const; // 0x00754460 (.h:67)
    const bool is_start_node() const; // 0x00754470 (.h:68)
    const int get_pair_number() const; // 0x00A178C0 (.h:69)
    virtual ~object_delivery_node(); // vtbl+0x00 0x00A17950

    unsigned int current_completion_time; // 0x9C
private:
    const base_delivery_definition_data* delivery_def; // 0xA0
    bool start_node; // 0xA4
    int pair_number; // 0xA8
    delivery_state_flags delivery_flags; // 0xAC
    unsigned int best_completion_time; // 0xB0
};

object* alloc_object_delivery_node(); // 0x00A62CD0 (:24)
const char* classname_object_delivery_node(); // 0x00A17900 (:24)
int bpo_index_object_delivery_node(); // 0x00A178F0 (:24)
void types_object_delivery_node(signed char& type, signed char& subtype); // 0x00A178E0 (:24)
bool object_delivery_node_get_all(farray<object_delivery_node const*,42>& delivery_nodes); // 0x00A62CE0 (:242)
const char* const object_delivery_node_get_name(const object_delivery_node* const odn); // 0x00A17AC0 (:256)

// 0x00754430 (.h:24)
inline const object_type object_delivery_node::get_type_static()
{
    return OT_DELIVERY_NODE;
}

// 0x00754440 (.h:24)
inline const object_sub_type object_delivery_node::get_subtype_static()
{
    return OT_SUB_UNDEFINED;
}
