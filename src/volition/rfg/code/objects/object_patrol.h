// Original: volition/rfg/code/objects/object_patrol.h
// Implementation: volition/rfg/code/objects/object_patrol.cpp
//
// Patrols (OT_PATROL): object_patrol, a patrol path given by its root nav point; patrol_node_handle, a position on a
// patrol path (patrol object handle plus nav point handle and direction), held by squads.
#pragma once

#include "volition/rfg/code/objects/humans/human_enums.h"
#include "volition/rfg/code/objects/object.h"
#include "volition/rfg/code/objects/object_type.h"
#include "volition/vlib/code/util/varray.h"

class navpoint;
class object_squad_spawn_node;
class rfg_mover;
class vector;

// size 0xC
class patrol_node_handle
{
public:
    patrol_node_handle(unsigned int patrol_handle_, unsigned int nav_handle_, bool reverse_, bool last_node_); // 0x00A3D380 (:511)
    patrol_node_handle(); // 0x00A3D360 (:506)
    bool is_reversed() const; // 0x00A05810 (.h:48)
    bool is_last_node() const; // 0x00A05820 (.h:51)
    bool is_valid() const; // 0x00A1AAD0 (:527)
    const rfg_mover* is_inside_building() const; // 0x00A3D430 (:595)
    bool is_near_road() const; // 0x00A92990 (:535)
    bool dont_follow_road() const; // 0x00A3D3C0 (:548)
    void reset(); // 0x00A1AAB0 (:520)
    bool is_streamed() const; // 0x00A1AAF0 (:559)
    bool get_pos(vector& pos) const; // 0x00A3D3F0 (:579)

    unsigned int patrol_handle; // 0x00
    unsigned int nav_handle; // 0x04
private:
    struct
    {
        bool reverse : 1; // 0x00:0
        bool last_node : 1; // 0x00:1
        bool node_checked : 1; // 0x00:2
        bool near_road : 1; // 0x00:3
    } node_flags; // 0x08
    friend class object_patrol;
};

// size 0xAC
class object_patrol : public object
{
    friend class patrol_path_goal; // reads patrol_flags (inferred)
    friend class patrol_check_in_action; // reads patrol_flags (inferred)
    friend class default_patrol_action; // reads patrol_flags (inferred)
    friend class patrol_in_vehicle_action; // reads patrol_flags (inferred)
    friend class follow_leader_goal; // reads patrol_flags (inferred)
    friend class drive_on_rail_goal; // reads root_node_handle (inferred)
public:
    object_patrol(); // 0x00A3D490 (:622)
    static const object_type get_type_static(); // 0x005DBDB0 (.h:93)
    static const object_sub_type get_subtype_static(); // 0x005DBDC0 (.h:93)
    static const char* get_classname_static(); // 0x00A05830 (.h:93)
    static int get_bpo_index_static(); // 0x00753A40 (.h:93)
    virtual const char* get_classname() const; // vtbl+0x0C 0x00A3D4C0 (.h:93)
    static unsigned int get_classname_hash(); // 0x00A1AB60 (:621)
    static int pool_remaining(); // 0x00A3D480 (:621)
    virtual void pool_free(); // vtbl+0x80 0x00A92A10 (:621)
    virtual bool serialize(obj_prop_block& pblock, bool load); // vtbl+0x14 0x00AAA800 (:630)
    virtual bool serialize_restore_if_missing_in_save_file(obj_prop_block& pblock); // vtbl+0x20 0x00A1AB70 (:734)
    virtual bool serialize_restore_if_resetting_destruction(obj_prop_block& pblock); // vtbl+0x24 0x00A1AB80 (:739)
    virtual void get_bbox(vector& bmin, vector& bmax) const; // vtbl+0x04 0x00A1AB90 (:753)
    virtual void get_local_bbox(vector& bmin, vector& bmax) const; // vtbl+0x08 0x00A1AC60 (:763)
protected:
    virtual void free_this(); // vtbl+0x28 0x00A92A20 (:776)
public:
    patrol_node_handle get_next_patrol_point_handle(const patrol_node_handle& node_handle, const vector& location) const; // 0x00A69C80 (:956)
    patrol_node_handle get_next_patrol_point_handle(const vector& location) const; // 0x00A699F0 (:846)
    patrol_node_handle get_first_patrol_point_handle() const; // 0x00A1AD00 (:949)
    bool is_member(const patrol_node_handle& node_handle) const; // 0x00A1ACE0 (:886)
    bool is_root_node(const patrol_node_handle& node_handle) const; // 0x00A1ACA0 (:796)
    bool get_last_patrol_node_handle(patrol_node_handle& last_node_handle) const; // 0x00A69BC0 (:920)
    bool get_last_patrol_node_pos(vector& node_pos) const; // 0x00A69B30 (:892)
    bool get_node_pos(const patrol_node_handle& node_handle, vector& patrol_point) const; // 0x00A1AD20 (:1098)
    human_movement_states get_foot_speed(const patrol_node_handle& node_handle) const; // 0x00A3D530 (:1079)
    bool get_all_patrol_nodes(farray<navpoint*,512>& nav_list) const; // 0x00A69FC0 (:1064)
    bool is_override_patrol() const; // 0x005DBDD0 (.h:146)
    bool is_ASD_truck_patrol() const;
    bool is_paused() const; // 0x005DBDE0 (.h:150)
    unsigned int get_root_nav_handle() const { return root_node_handle.nav_handle; } // inlined in object_convoy.cpp
    void set_pause_flag(bool paused); // 0x00753A50 (.h:152)
    bool marauder_raid_route() const; // 0x00753A70 (.h:155)
    patrol_node_handle get_root_node_handle() const; // 0x005DBDF0 (.h:158)
    void set_override_flag(bool override_patrol); // 0x00B1F600 (.h:160)
    bool no_patrol_check_in() const; // 0x005DBE10 (.h:165)
    bool combat_ready() const; // 0x005DBE20 (.h:168)
    const navpoint* get_root_nav() const; // 0x00A3D500 (:745)
    const navpoint* get_nearest_nav(const vector& point) const; // 0x00A698F0 (:806)
    virtual ~object_patrol(); // vtbl+0x00 0x00A3D4D0

private:
    patrol_node_handle root_node_handle; // 0x9C
    struct
    {
        unsigned int looping_patrol : 1; // 0x00:0
        unsigned int courier_patrol : 1; // 0x00:1
        unsigned int override_patrol : 1; // 0x00:2
        unsigned int is_paused : 1; // 0x00:3
        unsigned int combat_ready : 1; // 0x00:4
        unsigned int no_check_in : 1; // 0x00:5
        unsigned int marauder_raid : 1; // 0x00:6
        unsigned int ASD_truck_patrol : 1; // 0x00:7
    } patrol_flags; // 0xA8
};

bool build_patrol_point_list(navpoint* root_nav, farray<navpoint*,512>& nav_list); // 0x00A69410 (:121)
object* alloc_object_patrol(); // 0x00A698E0 (:621)
void types_object_patrol(signed char& type, signed char& subtype); // 0x00A1AB30 (:621)
const char* classname_object_patrol(); // 0x00A1AB50 (:621)
int bpo_index_object_patrol(); // 0x00A1AB40 (:621)
bool patrol_handles_are_equal(const patrol_node_handle& patrol_handle1, const patrol_node_handle& patrol_handle2); // 0x00A1AD60 (:1129)
object_squad_spawn_node* patrol_find_child_squad_node(const object_patrol* patrol); // 0x00A3D580 (:1145)
