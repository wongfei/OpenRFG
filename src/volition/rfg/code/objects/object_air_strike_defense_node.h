// Original: volition/rfg/code/objects/object_air_strike_defense_node.h
// Implementation: volition/rfg/code/objects/object_air_strike_defense_node.cpp
//
// Air strike defense nodes: the start points of the air strike defense activity, with their definitions read from
// the activity table (asd_definition).
#pragma once

#include "volition/rfg/code/objects/object.h"
#include "volition/rfg/code/objects/object_type.h"
#include "volition/vlib/code/time/timestamp.h"

class asd_definition;
class district;

// size 0xA8
class object_air_strike_defense_node : public object
{
public:
    object_air_strike_defense_node(); // 0x00A15980 (:36)
    static const object_type get_type_static(); // 0x00754E00 (.h:87)
    static const object_sub_type get_subtype_static(); // 0x00754E10 (.h:87)
    static const char* get_classname_static(); // 0x00A15910 (.h:87)
    static int get_bpo_index_static(); // 0x00A15920 (.h:87)
    virtual const char* get_classname() const; // vtbl+0x0C 0x00A159B0 (.h:87)
    static unsigned int get_classname_hash(); // 0x00A15970 (:35)
    static int pool_remaining(); // 0x00A393F0 (:35)
    virtual void pool_free(); // vtbl+0x80 0x00A8DB20 (:35)
    virtual bool serialize(obj_prop_block& pblock, bool load); // vtbl+0x14 0x00AA4890 (:45)
protected:
    virtual void free_this(); // vtbl+0x28 0x00A8DB30 (:40)
public:
    const asd_definition* get_definition() const; // 0x00754E20 (.h:102)
    bool find_air_strike_defense_node();
    const district* get_district() const; // 0x00A159D0 (:84)
    bool get_completed_flag() const;
    void set_completed_flag(bool);
    virtual ~object_air_strike_defense_node(); // vtbl+0x00 0x00A159C0

    timestamp last_used_time; // 0x9C
    const asd_definition* asd_def; // 0xA0
private:
    struct
    {
        bool completed : 1; // 0x00:0
    } asd_node_flags; // 0xA4
};

object* alloc_object_air_strike_defense_node(); // 0x00A60D40 (:35)
const char* classname_object_air_strike_defense_node(); // 0x00A15960 (:35)
void types_object_air_strike_defense_node(signed char& type, signed char& subtype); // 0x00A15940 (:35)
int bpo_index_object_air_strike_defense_node(); // 0x00A15950 (:35)
void asd_definitions_init(); // 0x00A60D50 (:98)
const asd_definition* asd_definition_from_name(const char* name); // 0x00A39420 (:194)
