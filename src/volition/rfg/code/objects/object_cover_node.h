// Original: volition/rfg/code/objects/object_cover_node.h
// Implementation: volition/rfg/code/objects/object_cover_node.cpp
//
// Cover nodes (OT_COVER_NODE): placed or dynamic cover positions with their stances, firing positions and offensive
// and defensive angles (compressed to degrees in a byte), disabled temporarily or when unreachable, hit too often,
// too many died there or destruction removed them.
#pragma once

#include <stdint.h>
#include "volition/rfg/code/objects/obj_enums.h"
#include "volition/rfg/code/objects/object.h"
#include "volition/rfg/code/objects/object_cover_node_enums.h"
#include "volition/rfg/code/objects/object_type.h"
#include "volition/vlib/code/math/vector3.h"
#include "volition/vlib/code/time/timestamp.h"

class human;
class rfg_destruction_event;
class rfg_mover;

// size 0x2
struct cover_node_flags
{
    unsigned char disabled_reason : 4; // 0x00:0
    unsigned char dynamic : 1; // 0x00:4
    unsigned char zone : 1; // 0x00:5
    unsigned char vehicle : 1; // 0x00:6
    unsigned char crouch : 1; // 0x00:7
    unsigned char fire_popup : 1; // 0x01:0
    unsigned char fire_left : 1; // 0x01:1
    unsigned char fire_right : 1; // 0x01:2
    unsigned char off_navmesh : 1; // 0x01:3
    unsigned char on_mover : 1; // 0x01:4
};

// size 0xC0
class cover_node : public object
{
public:
    cover_node(); // 0x00A16790 (:477)
    static const object_type get_type_static(); // 0x005DB9D0 (.h:57)
    static const object_sub_type get_subtype_static(); // 0x005DB9E0 (.h:58)
    static const char* get_classname_static(); // 0x00A05A50 (.h:59)
    static int get_bpo_index_static(); // 0x005DB9F0 (.h:60)
    virtual const char* get_classname() const; // vtbl+0x0C 0x00A167C0 (.h:61)
    static unsigned int get_classname_hash(); // 0x00A16780 (:476)
    static int pool_remaining(); // 0x00A3A4C0 (:476)
    virtual void pool_free(); // vtbl+0x80 0x00A8F7D0 (:476)
    virtual bool serialize(obj_prop_block& pblock, bool load); // vtbl+0x14 0x00AA6000 (:496)
    virtual obj_serialize_mode serialize_mode(); // vtbl+0x18 0x00A167E0 (:483)
    virtual bool stream(bool load); // vtbl+0x1C 0x00A62510 (:754)
    virtual void hierarchy_notify(hierarchy_notify_code code); // vtbl+0x70 0x00A16800 (:815)
protected:
    virtual void free_this(); // vtbl+0x28 0x00AA6560 (:797)
public:
    virtual void get_bbox(vector& bmin, vector& bmax) const; // vtbl+0x04 0x00A16F40 (:1325)
    virtual void get_local_bbox(vector& bmin, vector& bmax) const; // vtbl+0x08 0x00A17010 (:1333)
    bool has_standing_stepout() const; // 0x00A16B10 (:908)
    bool get_approximate_firing_position(vector& fire_pos, const vector& target_pos, cover_node_stances* stance, cover_node_firing_positions* firing_position) const; // 0x00A16820 (:834)
    bool is_defense_only() const; // 0x005DBA00 (.h:91)
    cover_check_results check_cover_type(const human* hp, const vector& target_pos, bool check_occupied, bool check_los, bool check_range, const human* target_hp) const; // 0x00A8FB10 (:1068)
    cover_check_results check_cover_type(const human* hp, const object* target, bool check_occupied, bool check_los, bool check_range) const; // 0x00AB3D60 (:1049)
    bool will_provide_offensive_cover(const human* hp, const vector& target_pos, bool check_occupied, bool check_los, bool check_range, const human* target_hp) const; // 0x005DBA50 (.h:97)
    bool will_provide_offensive_cover(const human* hp, const object* target, bool check_occupied, bool check_los, bool check_range) const; // 0x005DBA20 (.h:94)
    bool will_provide_defensive_cover(const human* hp, const vector& target_pos, bool check_occupied) const; // 0x00A8FE00 (:1164)
    bool will_provide_defensive_cover(const human* hp, const object* target, bool check_occupied) const; // 0x00AA65C0 (:1160)
    const human* is_occupied(const human* checking_human) const; // 0x00A62540 (:1246)
    void disable(cover_node_disabled_reasons reason, int time); // 0x00A16D20 (:1182)
    void reenable(); // 0x00A16DB0 (:1209)
    bool is_disabled() const; // 0x005DBA80 (.h:109)
    const timestamp& get_disabled_timer() const { return disabled_timer; } // spawn_placement.cpp reads the field directly
    bool is_permanently_disabled() const; // 0x00A05A60 (.h:110)
    cover_node_disabled_reasons get_disabled_reason() const;
    bool is_inside(const vector& point) const; // 0x00A16F10 (:1317)
    void increment_death_count(); // 0x00A16DD0 (:1217)
    bool register_hit(const human* attacker); // 0x00A16E90 (:1292)
    void locate_node(); // 0x00A3A4F0 (:1339)
private:
    bool in_firing_arc(const vector& target_xz_dir) const; // 0x00A16B40 (:921)
    bool in_defensive_arc(const vector& target_xz_dir) const; // 0x00A16C30 (:945)
    bool has_line_of_fire(const vector& target_pos, const human* attacker_hp, const human* target_hp) const; // 0x00A8F7E0 (:977)
public:
    virtual ~cover_node(); // vtbl+0x00 0x00A167D0

    vector closest_point; // 0x9C
    uint64_t closest_nav_cell_handle; // 0xA8
private:
    timestamp death_count_timer; // 0xB0
    timestamp disabled_timer; // 0xB4
public:
    cover_node_flags flags; // 0xB8
private:
    unsigned char death_count; // 0xBA
    unsigned char recorded_hits; // 0xBB
    unsigned char off_left_deg; // 0xBC
    unsigned char off_right_deg; // 0xBD
    unsigned char def_left_deg; // 0xBE
    unsigned char def_right_deg; // 0xBF
};

float get_compressed_cosine_ubyte(unsigned char degrees); // 0x00A16600 (:433)
float get_compressed_sine_ubyte(unsigned char degrees); // 0x00A16630 (:448)
float get_compressed_cosine_int(int degrees); // 0x00A16660 (:463)
float get_compressed_sine_int(int degrees); // 0x00A166B0 (:471)
const char* classname_cover_node(); // 0x00A16770 (:476)
int bpo_index_cover_node(); // 0x00A16760 (:476)
object* alloc_cover_node(); // 0x00A62500 (:476)
void types_cover_node(signed char& type, signed char& subtype); // 0x00A16750 (:476)
void disable_all_cover_nodes_in_a_building(rfg_mover* mover); // 0x00A3A8F0 (:1548)
void enable_all_cover_nodes_in_a_building(rfg_mover* mover); // 0x00A3A950 (:1567)
void cover_dyn_process_destruction_event(const rfg_destruction_event* eventp); // 0x00A62640 (:1590)
cover_node_stances cover_stance_from_state(const human* hp); // 0x00A17050 (:1642)
cover_node_firing_positions cover_firing_position_from_state(const human* hp); // 0x00A170D0 (:1671)

// 0x005DB9D0 (.h:57)
inline const object_type cover_node::get_type_static()
{
    return OT_COVER_NODE;
}

// 0x005DB9E0 (.h:58)
inline const object_sub_type cover_node::get_subtype_static()
{
    return OT_SUB_UNDEFINED;
}
