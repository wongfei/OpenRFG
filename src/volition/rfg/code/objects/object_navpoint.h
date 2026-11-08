// Original: volition/rfg/code/objects/object_navpoint.h
// Implementation: volition/rfg/code/objects/object_navpoint.cpp
//
// Nav points (OT_NAVPOINT): the nodes of the ped and patrol path networks, with up to ten links (each disabled by a
// reason mask), the nav cell they are in, the pathfinding heap data and flags (doors, bridges, branches, road
// following); paths are queried over them (navpoint_query_for_path). obj_nav_props creates one.
#pragma once

#include "volition/rfg/code/objects/obj_enums.h"
#include "volition/rfg/code/objects/object.h"
#include "volition/rfg/code/objects/object_type.h"
#include "volition/rfg/code/pathfinding/nav_heap.h"

class matrix;
struct nav_cell;
class navpoint_heap;
class rfg_mover;
class vector;

enum navpoint_disabled_by_reason
{
    NPDR_DESTRUCTION = 0,
    NPDR_SAFEHOUSE = 1,
    NPDR_BLOCKED = 2,
    NPDR_CONSOLE = 3,
};

// size 0x1
struct nav_point_flags
{
    unsigned char single_node_link : 1; // 0x00:0
    unsigned char dont_follow_road : 1; // 0x00:1
    unsigned char ignore_lanes : 1; // 0x00:2
    unsigned char door : 1; // 0x00:3
    unsigned char start_branch : 1; // 0x00:4
    unsigned char end_branch : 1; // 0x00:5
    unsigned char bridge : 1; // 0x00:6
};

// size 0xFC
extern bool Navpoint_treat_all_as_enabled; // 0x0302F158

class navpoint : public object
{
public:
    navpoint(); // 0x00A1A0B0 (:94)
    static const object_type get_type_static(); // 0x005DBAB0 (.h:83)
    static const object_sub_type get_subtype_static(); // 0x005DBAC0 (.h:83)
    static const char* get_classname_static(); // 0x00A059B0 (.h:83)
    static int get_bpo_index_static(); // 0x005DBAD0 (.h:83)
    virtual const char* get_classname() const; // vtbl+0x0C 0x00A1A0D0 (.h:83)
    static unsigned int get_classname_hash(); // 0x00A1A0A0 (:93)
    static int pool_remaining(); // 0x00A3CE90 (:93)
    virtual void pool_free(); // vtbl+0x80 0x00A921B0 (:93)
    virtual bool serialize(obj_prop_block& pblock, bool load); // vtbl+0x14 0x00AAA020 (:106)
    virtual obj_serialize_mode serialize_mode(); // vtbl+0x18 0x00A1A0F0 (:326)
    virtual void hierarchy_notify(hierarchy_notify_code code); // vtbl+0x70 0x00A1A110 (:337)
    virtual bool stream(bool load); // vtbl+0x1C 0x00A921C0 (:386)
    virtual bool serialize_restore_if_missing_in_save_file(obj_prop_block& pblock); // vtbl+0x20 0x00A1A140 (:597)
    virtual bool serialize_restore_if_resetting_destruction(obj_prop_block& pblock); // vtbl+0x24 0x00A1A150 (:602)
protected:
    virtual void free_this(); // vtbl+0x28 0x00A926F0 (:618)
public:
    virtual void get_bbox(vector& bmin, vector& bmax) const; // vtbl+0x04 0x00A1A160 (:649)
    virtual void get_local_bbox(vector& bmin, vector& bmax) const; // vtbl+0x08 0x00A1A230 (:657)
    void propogate_set_info(); // 0x00A1A280 (:663)
    void set_disabled_by(navpoint_disabled_by_reason reason); // 0x00A059C0 (.h:125)
    void clear_disabled_by(navpoint_disabled_by_reason reason); // 0x00A059E0 (.h:126)
    bool get_disabled_by(navpoint_disabled_by_reason reason) const; // 0x00AE9ED0 (.h:127)
    bool get_disabled() const; // 0x005DBAE0 (.h:128)
    void set_link_disabled_by(int link_index, navpoint_disabled_by_reason reason); // 0x00A05A00 (.h:130)
    void clear_link_disabled_by(int link_index, navpoint_disabled_by_reason reason); // 0x00A05A20 (.h:131)
    bool get_link_disabled_by(int link_index, navpoint_disabled_by_reason reason) const; // 0x00AE9EF0 (.h:132)
    bool get_link_disabled(int link_index) const; // 0x005DBB00 (.h:133)
    unsigned int get_next_navpoint(); // 0x005DBB30 (.h:135)
    void set_next_navpoint(unsigned int navp_handle); // 0x00A3CEC0 (:674)
    void clear_next_navpoint(); // 0x00A05A40 (.h:137)
private:
    void remove_navcell_reference(); // 0x00A67310 (:352)
public:
    virtual ~navpoint(); // vtbl+0x00 0x00A1A0E0

    nav_cell* nav_cell_ptr; // 0x9C
    navpoint* links[10]; // 0xA0
    float radius; // 0xC8
    float speed_limit; // 0xCC
    navpoint* prev_path_navpoint; // 0xD0
    navpoint* next_path_navpoint; // 0xD4
    navpoint_heap_data heap_data; // 0xD8
    unsigned short set; // 0xE8
    short num_links; // 0xEA
    unsigned char link_disabled_by[10]; // 0xEC
    signed char type; // 0xF6
    nav_point_flags nav_flags; // 0xF7
private:
    unsigned int next_navpoint_internal; // 0xF8
};

// size 0xC
class obj_nav_props : public obj_prop_block
{
public:
    obj_nav_props(const vector& pos, const matrix& orient, signed char type); // 0x00A517C0 (.h:152)
    ~obj_nav_props(); // 0x00A1A130

};

unsigned short get_next_navpoint_set_id(); // 0x00A1A040 (:74)
object* alloc_navpoint(); // 0x00A67300 (:93)
void types_navpoint(signed char& type, signed char& subtype); // 0x00A1A070 (:93)
int bpo_index_navpoint(); // 0x00A1A080 (:93)
const char* classname_navpoint(); // 0x00A1A090 (:93)
float navpoint_pathfinding_cost(navpoint* navp); // 0x00A1A2E0 (:705)
void navpoint_query_for_path(navpoint* navp, navpoint* arrival_navpoint, float arrival_cost, const vector& goal_pos, navpoint_heap* nhp); // 0x00A1A310 (:716)
void delete_all_navpoints_in_building(rfg_mover* mover); // 0x00A3CF00 (:753)
void link_navpoints(navpoint* navpoint1, navpoint* navpoint2, bool mark_as_modified); // 0x00A1A400 (:770)
void remove_navpoint_link(navpoint* navpoint1, navpoint* navpoint2); // 0x00A1A500 (:809)
void disable_navpoint_link(navpoint* navpoint1, navpoint* navpoint2, navpoint_disabled_by_reason reason); // 0x00A1A620 (:827)
void enable_navpoint_link(navpoint* navpoint1, navpoint* navpoint2, navpoint_disabled_by_reason reason); // 0x00A1A690 (:845)
int navpoint_get_dynamic_navpoint_count(); // 0x00A1A710 (:1142)
void navpoint_level_init_pre(); // 0x00A1A720 (:1147)
void navpoint_set_treat_all_as_enabled(bool value); // 0x00A1A730 (:1152)
