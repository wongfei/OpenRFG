// Original: volition/rfg/code/objects/object_path_road.h
// Implementation: volition/rfg/code/objects/object_path_road.cpp
//
// Roads (OT_PATH_ROAD): a road spline (points with widths, and connections to other roads, stored as offsets into
// the zone data), the road heap reference used for road pathfinding, and the breaks and roadblocks that cut it.
#pragma once

#include "volition/rfg/code/objects/object.h"
#include "volition/rfg/code/objects/object_type.h"
#include "volition/vlib/code/math/vector3.h"
#include "volition/vlib/code/util/endian_type.h"
#include "volition/vlib/code/util/varray.h"

struct cfile;
class npc;
// size 0x8
struct object_path_road_heap_reference
{
    unsigned short heap_data_index; // 0x00
    unsigned int heap_session_id; // 0x04
};
class road_heap;

enum road_broken_state
{
    ROAD_BROKEN_NEITHER = 0,
    ROAD_BROKEN_LEFT = 1,
    ROAD_BROKEN_RIGHT = 2,
    ROAD_BROKEN_COMPLETELY = 3,
    NUM_ROAD_BROKEN_STATES = 4,
};

// size 0x1C
struct road_spline_point
{
    vector pos; // 0x00
    vector pt_handle; // 0x0C
    float width; // 0x18
};

// size 0x8
struct road_spline_connection
{
    unsigned int handle; // 0x00
    unsigned short from_pt; // 0x04
    unsigned short to_pt; // 0x06
};

// size 0x10
struct road_spline_data
{
    road_spline_data(); // 0x00A3CF60

    int num_points; // 0x00
    et_ptr_offset<road_spline_point,0> points; // 0x04
    int num_connections; // 0x08
    et_ptr_offset<road_spline_connection,0> connections; // 0x0C
};

// size 0xC8
class object_path_road : public object
{
public:
    object_path_road(); // 0x00A673B0 (:54)
    static const object_type get_type_static(); // 0x005DDBC0 (.h:38)
    static const object_sub_type get_subtype_static(); // 0x005DDBD0 (.h:38)
    static const char* get_classname_static(); // 0x00A16090 (.h:38)
    static int get_bpo_index_static(); // 0x005DDBE0 (.h:38)
    virtual const char* get_classname() const; // vtbl+0x0C 0x00A673E0 (.h:38)
    static unsigned int get_classname_hash(); // 0x00A1A790 (:53)
    static int pool_remaining(); // 0x00A3CF50 (:53)
    virtual void pool_free(); // vtbl+0x80 0x00A92760 (:53)
    virtual bool serialize(obj_prop_block& pblock, bool load); // vtbl+0x14 0x00AAA5A0 (:66)
    void save_breaks(cfile* f); // 0x00A1A7C0 (:204)
    virtual bool serialize_restore_if_missing_in_save_file(obj_prop_block& pblock); // vtbl+0x20 0x00A1A7A0 (:193)
    virtual bool serialize_restore_if_resetting_destruction(obj_prop_block& pblock); // vtbl+0x24 0x00A1A7B0 (:198)
    float find_closest_point_on_road(const vector& p, vector& point, bool ignore_breaks) const; // 0x00A67E50 (:342)
    float get_width(float u) const; // 0x00A67D20 (:314)
    bool break_road(int index, road_broken_state state); // 0x00A3CF70 (:439)
    bool break_road_at_point(const vector& p, road_broken_state state); // 0x00A68500 (:511)
    bool has_breaks() const; // 0x00A1A8A0 (:491)
    road_broken_state is_broken_at_index(int index) const; // 0x00A1A8B0 (:496)
    road_broken_state is_broken_at_point(const vector& p); // 0x00A685A0 (:527)
    bool rejoin_road(int index); // 0x00A1A820 (:467)
    bool rejoin_road_at_point(const vector& p); // 0x00A68550 (:519)
    bool has_road_blocks() const; // 0x00A1A900 (:535)
    bool is_road_blocked_at_index(int index) const; // 0x00A1A910 (:540)
    bool is_road_blocked_at_point(const vector& p); // 0x00A685F0 (:555)
    bool road_block_road(int index); // 0x00A1A950 (:563)
    bool road_block_road_at_point(const vector& p); // 0x00A68680 (:579)
    bool remove_road_block_at_index(int index); // 0x00A1A990 (:587)
    bool remove_road_block_at_point(const vector& p); // 0x00A68710 (:609)
    float get_max_road_distance() const; // 0x0075AF50 (.h:91)
    bool vehicle_spawning_disabled() const;
    bool allow_ambient_travel() const; // 0x005DDBF0 (.h:97)
    bool is_special_road() const; // 0x005DDC00 (.h:99)
    bool is_gps_only() const; // 0x005DDC10 (.h:101)
    bool no_reinforcement_spawns() const; // 0x005DDC20 (.h:104)
    bool can_travel_on_road(const npc* np) const; // 0x00A1AA10 (:626)
    const vector& get_road_point(unsigned short point_index) const; // 0x00A68760 (:618)
    void get_point(vector& out, float u) const; // 0x00A67420 (:215)
    void get_tangent(vector& out, float u) const; // 0x00A67850 (:266)
    virtual ~object_path_road(); // vtbl+0x00 0x00A673F0

    road_spline_data spline_data; // 0x9C
    object_path_road_heap_reference* heap_reference; // 0xAC
    unsigned int heap_session_id; // 0xB0
    unsigned short heap_data_index; // 0xB4
    struct
    {
        unsigned int vehicle_spawn_disabled : 1; // 0x00:0
        unsigned int ambient : 1; // 0x00:1
        unsigned int edf_only : 1; // 0x00:2
        unsigned int marauder : 1; // 0x00:3
        unsigned int guerrilla : 1; // 0x00:4
        unsigned int one_way : 1; // 0x00:5
        unsigned int special : 1; // 0x00:6
        unsigned int gps_only : 1; // 0x00:7
        unsigned int no_reinforcements : 1; // 0x00:8
    } road_flags; // 0xB8
private:
    unsigned char num_breaks; // 0xBC
    unsigned char breaks[4]; // 0xBD
    unsigned char num_road_blocks; // 0xC1
    unsigned char road_blocks[4]; // 0xC2
};

void unused_dcf_road_set_gps_only(); // 0x00AAA510 (:26)
object* alloc_object_path_road(); // 0x00A673A0 (:53)
const char* classname_object_path_road(); // 0x00A1A780 (:53)
void types_object_path_road(signed char& type, signed char& subtype); // 0x00A1A760 (:53)
int bpo_index_object_path_road(); // 0x00A1A770 (:53)
void object_path_road_level_init_pre(); // 0x00A1AA70 (:961)
void object_path_road_level_init_post(); // 0x00A1AA80 (:966)
bool object_path_road_query_for_path(object_path_road* road_spline, unsigned short point, object_path_road* arrival_road_spline, const unsigned short arrival_point, float arrival_cost, const vector& goal_pos, road_heap* nhp, bool reverse_cost, bool gps_path); // 0x00A687B0 (:1052)
bool object_road_break_closest_road_at_point(const vector& point); // 0x00A92770 (:1106)
bool object_road_rejoin_closest_road_at_point(const vector& point); // 0x00A92860 (:1120)
object_path_road* object_road_get_closest_road_list(const base_array<object_path_road*>& roads_search_set, const vector& point, vector& closest_point, float& u_value, bool ignore_breaks, const vector* fvec, const object_path_road* predisposed_road); // 0x00A68940 (:1150)
object_path_road* object_road_get_closest_road(const vector& point, bool use_special_roads, float search_radius, bool ignore_breaks, bool collect_gps); // 0x00A92910 (:1259)
object_path_road* object_road_get_closest_road(const vector& point, bool use_special_roads, vector& closest_point, float& u_value, float search_radius, bool ignore_breaks, const vector* fvec, const object_path_road* predisposed_road, bool collect_gps); // 0x00A68E00 (:1280)
float object_road_get_direction(object_path_road* road, const vector& fvec, float u); // 0x00A68FA0 (:1326)
bool object_road_get_roads_in_zone(const vector& point, farray<object_path_road const*,322>& road_list, bool use_special_roads); // 0x00A92950 (:1340)
bool object_road_get_roads_in_zone(unsigned short zone_id_xy, farray<object_path_road const*,322>& road_list, bool use_special_roads); // 0x00A69050 (:1349)
const object_path_road* object_road_point_on_road(const vector& pos, bool use_special_roads); // 0x00A69100 (:1376)
bool object_road_in_zone(const vector& point, bool use_special_roads); // 0x00A92970 (:1403)
bool object_road_in_zone(unsigned short zone_id_xy, bool use_special_roads); // 0x00A69190 (:1411)
object_path_road* object_road_get_closest_road_zone(const vector& point, bool use_special_roads, vector& closest_point, float& u_value, unsigned short zone_id_xy, bool ignore_breaks, bool collect_gps); // 0x00A69210 (:1438)
void road_data_save(cfile* f); // 0x00A69340 (:1485)
void road_data_load(cfile* f); // 0x00A3CFF0 (:1508)
