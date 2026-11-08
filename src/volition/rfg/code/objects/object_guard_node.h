// Original: volition/rfg/code/objects/object_guard_node.h
// Implementation: volition/rfg/code/objects/object_guard_node.cpp
//
// Guard nodes (OT_GUARD_NODE): positions an NPC reserves to stand guard, with a facing range and a group; disabled
// by destruction and detours.
#pragma once

#include "volition/rfg/code/objects/obj_enums.h"
#include "volition/rfg/code/objects/object.h"
#include "volition/rfg/code/objects/object_type.h"
#include "volition/vlib/code/time/timestamp.h"

class npc;
class rfg_destruction_event;
class vector;

// size 0xA8
class object_guard_node : public object
{
public:
    // size 0x1
    struct guard_node_flags_struct
    {
        unsigned char disabled_by_detour : 1; // 0x00:0
        unsigned char disabled : 1; // 0x00:1
        unsigned char interior : 1; // 0x00:2
        unsigned char near_trans : 1; // 0x00:3
        unsigned char ground_floor : 1; // 0x00:4
        unsigned char important : 1; // 0x00:5
        unsigned char no_defensive_combat : 1; // 0x00:6
    };

    object_guard_node(); // 0x00A17E50 (:39)
    static const object_type get_type_static(); // 0x005DBB50 (.h:26)
    static const object_sub_type get_subtype_static(); // 0x005DBB60 (.h:26)
    static const char* get_classname_static(); // 0x00A17DF0 (.h:26)
    static int get_bpo_index_static(); // 0x005DBB70 (.h:26)
    virtual const char* get_classname() const; // vtbl+0x0C 0x00A17E80 (.h:26)
    static unsigned int get_classname_hash(); // 0x00A17E40 (:38)
    static int pool_remaining(); // 0x00A3B030 (:38)
    virtual void pool_free(); // vtbl+0x80 0x00A901D0 (:38)
    virtual bool serialize(obj_prop_block& pblock, bool load); // vtbl+0x14 0x00AA7060 (:45)
    virtual bool stream(bool load); // vtbl+0x1C 0x00A901E0 (:133)
protected:
    virtual void free_this(); // vtbl+0x28 0x00A90490 (:254)
public:
    virtual void get_bbox(vector& bmin, vector& bmax) const; // vtbl+0x04 0x00A17EA0 (:220)
    virtual void get_local_bbox(vector& bmin, vector& bmax) const; // vtbl+0x08 0x00A17F70 (:230)
    virtual void hierarchy_notify(hierarchy_notify_code code); // vtbl+0x70 0x00A17FB0 (:239)
    bool is_reserved(unsigned int npc_handle) const; // 0x00A18070 (:320)
    bool can_reserve(unsigned int npc_handle) const; // 0x00A904A0 (:336)
    bool is_occupied(unsigned int requesting_handle) const; // 0x00A63880 (:375)
    void reserve_node(unsigned int npc_handle); // 0x00A180B0 (:415)
    void release_node(unsigned int npc_handle); // 0x00A180E0 (:426)
    npc* get_reserving_npc() const; // 0x00A3B240 (:310)
    int time_since_occupied() const;
    signed char get_min_facing_index() const; // 0x00A18030 (:274)
    signed char get_max_facing_index() const; // 0x00A18050 (:279)
    signed char get_facing_index(const vector& cur_heading) const; // 0x00A3B060 (:284)
    vector get_facing_from_index(signed char idx) const; // 0x00A3B100 (:299)
    float get_left_angle_dot() const; // 0x00A17FD0 (:264)
    float get_right_angle_dot() const; // 0x00A18000 (:269)
    unsigned char get_group_id() const; // 0x005DBB80 (.h:87)
    void disable_node() { guard_node_flags.disabled = 1; } // 0x005DBB90 (.h:88)
    void disable_node_detour() { guard_node_flags.disabled_by_detour = 1; } // 0x00AE9F30 (.h:89)
    void reenable_node_detour() { guard_node_flags.disabled_by_detour = 0; } // 0x00AE9F40 (.h:90)
    bool is_disabled() const; // 0x005DBBA0 (.h:91)
    bool is_important() const;
    bool near_transition() const;
    bool on_ground_floor() const { return guard_node_flags.ground_floor != 0; } // 0x005DBBC0 (.h:96)
    bool is_interior() const;
    bool allows_defensive_combat() const;
    virtual ~object_guard_node(); // vtbl+0x00 0x00A17E90

private:
    unsigned int reserved_handle; // 0x9C
    timestamp reservation_timer; // 0xA0
    guard_node_flags_struct guard_node_flags; // 0xA4
    friend class go_to_guard_post_action; // disables an unreachable node (inferred)
    unsigned char group_id; // 0xA5
    unsigned char angle_left_deg; // 0xA6
    unsigned char angle_right_deg; // 0xA7
};

int bpo_index_object_guard_node(); // 0x00A17E20 (:38)
void types_object_guard_node(signed char& type, signed char& subtype); // 0x00A17E10 (:38)
object* alloc_object_guard_node(); // 0x00A63870 (:38)
const char* classname_object_guard_node(); // 0x00A17E30 (:38)
void guard_node_process_destruction_event(const rfg_destruction_event* eventp); // 0x00A63960 (:442)
