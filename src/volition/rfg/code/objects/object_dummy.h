// Original: volition/rfg/code/objects/object_dummy.h
// Implementation: volition/rfg/code/objects/object_dummy.cpp
//
// Dummy objects (OT_DUMMY): typed placeholder positions (demolition points, raid smoke plumes, reinforced fences,
// activity barricades, rally points, air bomb points, cutscene, vehicle rail spawns, tech response); some create an
// object of their own (barricades).
#pragma once

#include "volition/rfg/code/objects/obj_enums.h"
#include "volition/rfg/code/objects/object.h"
#include "volition/rfg/code/objects/object_type.h"

struct c_chunk_base;

enum dummy_object_types
{
    DUMMY_OBJECT_TYPE_INVALID = -1,
    DUMMY_OBJECT_TYPE_NONE = 0,
    DUMMY_OBJECT_TYPE_DEMOLITION_POINT = 1,
    DUMMY_OBJECT_TYPE_RAID_SMOKE_PLUME = 2,
    DUMMY_OBJECT_TYPE_REINFORCED_FENCE = 3,
    DUMMY_OBJECT_TYPE_ACTIVITY_BARRICADE = 4,
    DUMMY_OBJECT_TYPE_RALLY = 5,
    DUMMY_OBJECT_TYPE_AIR_BOMB_POINT = 6,
    DUMMY_OBJECT_TYPE_CUTSCENE = 7,
    DUMMY_OBJECT_TYPE_DEMO_MASTER = 8,
    DUMMY_OBJECT_TYPE_VRAIL_SPAWN = 9,
    DUMMY_OBJECT_TYPE_TECH_RESPONSE = 10,
    NUM_DUMMY_OBJECT_TYPES = 11,
};

// size 0xA0
class object_dummy : public object
{
public:
    object_dummy(); // 0x00A17D70 (:116)
    static const object_type get_type_static() { return OT_DUMMY; } // 0x005DD620 (.h:50)
    static const object_sub_type get_subtype_static() { return OT_SUB_UNDEFINED; } // 0x005DD630 (.h:50)
    static const char* get_classname_static(); // 0x00A15C80 (.h:50)
    static int get_bpo_index_static(); // 0x00A15C90 (.h:50)
    virtual const char* get_classname() const; // vtbl+0x0C 0x00A17D90 (.h:50)
    static unsigned int get_classname_hash(); // 0x00A17D60 (:115)
    static int pool_remaining(); // 0x00A3AFF0 (:115)
    virtual void pool_free(); // vtbl+0x80 0x00A901C0 (:115)
    virtual bool serialize(obj_prop_block& pblock, bool load); // vtbl+0x14 0x00AA6EF0 (:129)
    virtual void hierarchy_notify(hierarchy_notify_code code); // vtbl+0x70 0x00A17DB0 (:121)
    dummy_object_types get_dummy_type() const { return dummy_type; } // 0x005DD640 (.h:59)
    void create_dummy_object() const; // 0x00AA6FC0 (:161)
    bool is_creatable_object() const; // 0x00758440 (.h:65)
    virtual ~object_dummy(); // vtbl+0x00 0x00A17DA0

private:
    dummy_object_types dummy_type; // 0x9C
};

void types_object_dummy(signed char& type, signed char& subtype); // 0x00A17D30 (:115)
int bpo_index_object_dummy(); // 0x00A17D40 (:115)
object* alloc_object_dummy(); // 0x00A63850 (:115)
const char* classname_object_dummy(); // 0x00A17D50 (:115)
const c_chunk_base* obj_dummy_get_barricade_chunk(); // 0x00A17DD0 (:197)
