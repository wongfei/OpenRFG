// Original: volition/rfg/code/objects/object_restricted_area.h
// Implementation: volition/rfg/code/objects/object_restricted_area.cpp
//
// Restricted areas: polygon or box regions the player may not enter (warning and yellow boxes around them), with the
// buildings they contain; checked every frame against the player.
#pragma once

#include "volition/rfg/code/objects/object.h"
#include "volition/rfg/code/objects/object_type.h"
#include "volition/vlib/code/math/bbox.h"
#include "volition/vlib/code/math/vector2.h"
#include "volition/vlib/code/memory/object_pool.h"
#include "volition/vlib/code/util/varray.h"

class vector;

enum restricted_area_check_reuslts
{
    RESTRICTED_AREA_INSIDE_YELLOW = 0,
    RESTRICTED_AREA_INSIDE_WARNING = 1,
    RESTRICTED_AREA_OUTSIDE = 2,
    NUM_RESTRICTED_AREA_CHECK_RESULTS = 3,
};

// size 0x408
struct restricted_area_polygon_data
{
    restricted_area_polygon_data(); // 0x00A26490

    int yellow_num_points; // 0x00
    vector2 yellow_polygon[64]; // 0x04
    int warning_num_points; // 0x204
    vector2 warning_polygon[64]; // 0x208
};

extern object_pool<restricted_area_polygon_data,64,1> Polygon_pool; // 0x0339FB50

// size 0x138
class object_restricted_area : public object
{
public:
    // size 0x4
    struct restricted_area_flags
    {
        unsigned int disabled : 1; // 0x00:0
        unsigned int allow_ambient_peds : 1; // 0x00:1
        unsigned int player_in_yellow : 1; // 0x00:2
    };

    object_restricted_area(); // 0x00A6A1C0 (:105)
    static const object_type get_type_static(); // 0x005DBBD0 (.h:45)
    static const object_sub_type get_subtype_static(); // 0x005DBBE0 (.h:45)
    static const char* get_classname_static(); // 0x00A06A20 (.h:45)
    static int get_bpo_index_static(); // 0x00A06A30 (.h:45)
    virtual const char* get_classname() const; // vtbl+0x0C 0x00A6A200 (.h:45)
    static unsigned int get_classname_hash(); // 0x00A1B140 (:104)
    static int pool_remaining(); // 0x00A3D6B0 (:104)
    virtual void pool_free(); // vtbl+0x80 0x00A92A60 (:104)
    virtual bool serialize(obj_prop_block& pblock, bool load); // vtbl+0x14 0x00AAAEB0 (:112)
    virtual bool stream(bool load); // vtbl+0x1C 0x00A1B150 (:252)
    virtual void free_this(); // vtbl+0x28 0x00A92A70 (:238)
    virtual void get_bbox(vector& bmin, vector& bmax) const; // vtbl+0x04 0x00A1B190 (:277)
    virtual void get_local_bbox(vector& bmin, vector& bmax) const; // vtbl+0x08 0x00A1B1D0 (:283)
    void get_warning_bbox(vector& bmin, vector& bmax) const; // 0x00A1B2D0 (:289)
    virtual bool is_inside(const vector& point) const; // vtbl+0x84 0x00A1B310 (:296)
    virtual bool is_inside_warning_area(const vector& point) const; // vtbl+0x88 0x00A1B3C0 (:316)
    virtual restricted_area_check_reuslts check_position(const vector& point) const; // vtbl+0x8C 0x00A1B480 (:337)
    bool is_disabled() const; // 0x005DBBF0 (.h:66)
    void enable(); // 0x00754EF0 (.h:69)
    void disable(); // 0x00A1B4C0 (:354)
    void add_building(unsigned int building_handle); // 0x00A6A260 (:366)
    void remove_building(unsigned int building_handle); // 0x00A3D6C0 (:379)
    int get_yellow_num_points() const; // 0x00A1B4F0 (:389)
    int get_warning_num_points() const; // 0x00A1B500 (:394)
    void get_yellow_point(vector2& point, int i) const; // 0x00A1B510 (:399)
    void get_warning_point(vector2& point, int i) const; // 0x00A1B540 (:405)
    bool allow_ambient_peds() const; // 0x005DBC00 (.h:85)
    void process_player_state(); // 0x00A1B580 (:439)
private:
    void create_from_local_bbox(const bbox& local_bbox, float warning_radius); // 0x00A1B650 (:474)
public:
    virtual ~object_restricted_area(); // vtbl+0x00 0x00A6A210

private:
    restricted_area_flags flags; // 0x9C
    farray<unsigned int,21> contained_buildings; // 0xA0
    bbox yellow_bbox; // 0x104
    bbox warning_bbox; // 0x11C
    restricted_area_polygon_data* polygon_data; // 0x134
};

int bpo_index_object_restricted_area(); // 0x00A1B120 (:104)
const char* classname_object_restricted_area(); // 0x00A1B130 (:104)
void types_object_restricted_area(signed char& type, signed char& subtype); // 0x00A1B110 (:104)
object* alloc_object_restricted_area(); // 0x00A6A1B0 (:104)
object_restricted_area* restricted_area_find_area(const vector& point, bool include_warning_areas, bool include_disabled); // 0x00A6A2E0 (:540)
bool restricted_area_in_any_region(const vector& point, bool include_warning_areas); // 0x00A6A3B0 (:563)
bool restricted_area_player_in_restricted_area(); // 0x00A1BC50 (:570)
void restricted_area_level_init(); // 0x00A1BC60 (:575)
void restricted_area_do_frame(); // 0x00A6A3D0 (:581)
