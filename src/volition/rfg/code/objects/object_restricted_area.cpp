// Original: volition/rfg/code/objects/object_restricted_area.cpp
//
// Whole (NPC.md NP3). A restricted area is a yellow polygon the player may not enter, usually with a larger warning polygon
// around it, both in world x and z.

#include "volition/rfg/code/objects/object_restricted_area.h"

#include "volition/rfg/code/level/rfg_prop_block_shared.h"
#include "volition/rfg/code/level/rfg_terrainx_shared.h"
#include "volition/rfg/code/level/world.h"
#include "volition/rfg/code/multi/multi_core/include/multi.h"
#include "volition/rfg/code/objects/humans/npc.h"
#include "volition/rfg/code/objects/humans/player.h"
#include "volition/rfg/code/objects/object_prop_names_shared.h"
#include "volition/rfg/code/scripting/action_script/global_event_data.h"
#include "volition/rfg/code/scripting/action_script/global_event_manager.h"
#include "volition/rfg/code/vehicle/vehicle_class.h"
#include "volition/vlib/code/memory/object_pool.h"

static const int MAX_RESTRICTED_AREAS = 64;
// Magic number: an area spans this far up and down.
static const float AREA_HALF_HEIGHT = 50.0f;
// Magic number: the warning band of a box area with no warning_radius.
static const float DEFAULT_WARNING_RADIUS = 10.0f;
// Magic number: what an npc or vehicle in a loading area takes to fade out, in milliseconds.
static const int RESTRICTED_NPC_FADE_TIME = 1000;

object_pool<restricted_area_polygon_data, MAX_RESTRICTED_AREAS, 1> Polygon_pool; // 0x0339FB50
static object_pool<object_restricted_area, MAX_RESTRICTED_AREAS, 1> object_restricted_areapool; // 0x030695C0
static int Restricted_area_player_in_yellow_counter; // 0x0302F164

// 0x00A26490
restricted_area_polygon_data::restricted_area_polygon_data()
{
}

// 0x00A6A0B0 (:68). Ambient npcs in the warning area of a new area fade out, with their vehicle if they have one.
static void remove_restricted_npcs(const object_restricted_area* area)
{
    const int count = World.get_num_objects<npc>();
    for (int i = 0; i < count; i++)
    {
        npc* np = World.get_object<npc>(i);
        if (np->npc_flags.ambient_spawn == 0 || !area->is_inside_warning_area(np->pos))
        {
            continue;
        }
        object* vehicle_obj = np->vehicle_handle != 0xFFFFFFFF ? World.object_from_handle(np->vehicle_handle) : nullptr;
        if (vehicle_obj != nullptr && vehicle_obj->get_obj_type() == OT_VEHICLE)
        {
            vehicle_fade_out(reinterpret_cast<vehicle*>(vehicle_obj), RESTRICTED_NPC_FADE_TIME, true, false);
        }
        else
        {
            np->fade_out_and_despawn(RESTRICTED_NPC_FADE_TIME, false);
        }
    }
}

// 0x00A1B110 (:104)
void types_object_restricted_area(signed char& type, signed char& subtype)
{
    type = OT_RESTRICTED_AREA;
    subtype = OT_SUB_UNDEFINED;
}

// 0x00A1B120 (:104)
int bpo_index_object_restricted_area()
{
    return 1;
}

// 0x00A1B130 (:104)
const char* classname_object_restricted_area()
{
    return "object_restricted_area";
}

// 0x00A6A1B0 (:104)
object* alloc_object_restricted_area()
{
    return object_restricted_areapool.alloc(0);
}

// Magic numbers: rfg.exe's arguments (0x0107B830), the pool's bytes and capacity.
static obj_class_instantiator object_restricted_area_instantiator("object_restricted_area", alloc_object_restricted_area, types_object_restricted_area, bpo_index_object_restricted_area, classname_object_restricted_area, object_restricted_area::pool_remaining, 0x4F80, MAX_RESTRICTED_AREAS); // 0x0353F098

// 0x00A1B140 (:104)
unsigned int object_restricted_area::get_classname_hash()
{
    return object_restricted_area_instantiator.this_hash;
}

// 0x00A3D6B0 (:104)
int object_restricted_area::pool_remaining()
{
    return object_restricted_areapool.get_free();
}

// 0x00A92A60 (:104)
void object_restricted_area::pool_free()
{
    object_restricted_areapool.free(this);
}

// 0x00A6A1C0 (:105)
object_restricted_area::object_restricted_area()
{
}

// 0x00A6A210
object_restricted_area::~object_restricted_area()
{
}

// 0x005DBBD0 (.h:45)
const object_type object_restricted_area::get_type_static()
{
    return OT_RESTRICTED_AREA;
}

// 0x005DBBE0 (.h:45)
const object_sub_type object_restricted_area::get_subtype_static()
{
    return OT_SUB_UNDEFINED;
}

// 0x00A06A20 (.h:45)
const char* object_restricted_area::get_classname_static()
{
    return "object_restricted_area";
}

// 0x00A06A30 (.h:45)
int object_restricted_area::get_bpo_index_static()
{
    return 1;
}

// 0x00A6A200 (.h:45)
const char* object_restricted_area::get_classname() const
{
    return "object_restricted_area";
}

// The box of a polygon, spanning the area's height. num_points is at least 1 for the yellow one and is not checked (original).
static void polygon_bbox(bbox& box, const vector2* polygon, int num_points)
{
    box.min.x = polygon[0].x;
    box.max.x = polygon[0].x;
    box.min.z = polygon[0].y;
    box.max.z = polygon[0].y;
    box.max.y = AREA_HALF_HEIGHT;
    box.min.y = -AREA_HALF_HEIGHT;
    vector point;
    point.y = 0.0f;
    for (int i = 1; i < num_points; i++)
    {
        point.x = polygon[i].x;
        point.z = polygon[i].y;
        bbox_expand(box, point);
    }
}

// 0x00AAAEB0 (:112). Only disabled is saved, as 4 bytes, and the load only takes it at 1 byte, so a saved value never comes
// back through here (original). A box area asks for the warning polygon too, which it does not have, and keeps the one
// create_from_local_bbox made.
bool object_restricted_area::serialize(obj_prop_block& pblock, bool load)
{
    if (!object::serialize(pblock, load))
    {
        return false;
    }
    if (!load)
    {
        const unsigned int disabled_value = flags.disabled;
        pblock.set<unsigned int>(RA_PROP_DISABLED, disabled_value);
        return true;
    }
    flags = restricted_area_flags();
    polygon_data = Polygon_pool.alloc(0);
    if (polygon_data == nullptr)
    {
        return false;
    }
    bbox box;
    if (pblock.get_optional(RA_PROP_BOX, box))
    {
        float warning_radius = DEFAULT_WARNING_RADIUS;
        pblock.get_optional(RA_PROP_WARNING_RADIUS, warning_radius);
        create_from_local_bbox(box, warning_radius);
    }
    else
    {
        pblock.get(RA_PROP_YELLOW_NUM_POINTS, polygon_data->yellow_num_points);
        pblock.get(RA_PROP_WARNING_NUM_POINTS, polygon_data->warning_num_points);
        pblock.get_buf_copy(RA_PROP_YELLOW_POLYGON, reinterpret_cast<unsigned char*>(polygon_data->yellow_polygon), polygon_data->yellow_num_points * (int)sizeof(vector2));
    }
    char flag_value;
    if (pblock.get_optional(RA_PROP_DISABLED, flag_value))
    {
        flags.disabled = flag_value != 0 ? 1 : 0;
    }
    if (pblock.get_optional(RA_PROP_ALLOW_PEDS, flag_value))
    {
        flags.allow_ambient_peds = flag_value != 0 ? 1 : 0;
    }
    polygon_bbox(yellow_bbox, polygon_data->yellow_polygon, polygon_data->yellow_num_points);
    // Magic number: a polygon needs three points.
    if (polygon_data->warning_num_points < 3)
    {
        warning_bbox.min.zero();
        warning_bbox.max.zero();
    }
    else
    {
        pblock.get_buf_copy(RA_PROP_WARNING_POLYGON, reinterpret_cast<unsigned char*>(polygon_data->warning_polygon), polygon_data->warning_num_points * (int)sizeof(vector2));
        polygon_bbox(warning_bbox, polygon_data->warning_polygon, polygon_data->warning_num_points);
    }
    remove_restricted_npcs(this);
    return true;
}

// 0x00A92A70 (:238)
void object_restricted_area::free_this()
{
    Polygon_pool.free(polygon_data);
    if (flags.player_in_yellow != 0)
    {
        Restricted_area_player_in_yellow_counter--;
        flags.player_in_yellow = 0;
    }
    object::free_this();
}

// 0x00A1B150 (:252). destroy_object frees this first, and the flag is then read from the freed object (original).
bool object_restricted_area::stream(bool load)
{
    if (!load)
    {
        if (obj_flags.destroy_on_stream != 0)
        {
            World.destroy_object(this, false);
        }
        if (flags.player_in_yellow != 0)
        {
            Restricted_area_player_in_yellow_counter--;
            flags.player_in_yellow = 0;
        }
    }
    return true;
}

// 0x00A1B190 (:277)
void object_restricted_area::get_bbox(vector& bmin, vector& bmax) const
{
    bmin = yellow_bbox.min;
    bmax = yellow_bbox.max;
}

// 0x00A1B1D0 (:283)
void object_restricted_area::get_local_bbox(vector& bmin, vector& bmax) const
{
    bmin = yellow_bbox.min - pos;
    bmax = yellow_bbox.max - pos;
}

// 0x00A1B2D0 (:289)
void object_restricted_area::get_warning_bbox(vector& bmin, vector& bmax) const
{
    bmin = warning_bbox.min;
    bmax = warning_bbox.max;
}

// 0x00A1B310 (:296)
bool object_restricted_area::is_inside(const vector& point) const
{
    if (flags.disabled != 0)
    {
        return false;
    }
    if (yellow_bbox.min.x <= point.x && yellow_bbox.min.z <= point.z && point.x <= yellow_bbox.max.x && point.z <= yellow_bbox.max.z)
    {
        const vector2 point_2d(point.x, point.z);
        return restricted_area_2d_point_in_polygon(point_2d, polygon_data->yellow_polygon, polygon_data->yellow_num_points);
    }
    return false;
}

// 0x00A1B3C0 (:316)
bool object_restricted_area::is_inside_warning_area(const vector& point) const
{
    if (flags.disabled != 0 || polygon_data->warning_num_points < 3)
    {
        return false;
    }
    if (warning_bbox.min.x <= point.x && warning_bbox.min.z <= point.z && point.x <= warning_bbox.max.x && point.z <= warning_bbox.max.z)
    {
        const vector2 point_2d(point.x, point.z);
        return restricted_area_2d_point_in_polygon(point_2d, polygon_data->warning_polygon, polygon_data->warning_num_points);
    }
    return false;
}

// 0x00A1B480 (:337)
restricted_area_check_reuslts object_restricted_area::check_position(const vector& point) const
{
    if (is_inside(point))
    {
        return RESTRICTED_AREA_INSIDE_YELLOW;
    }
    return is_inside_warning_area(point) ? RESTRICTED_AREA_INSIDE_WARNING : RESTRICTED_AREA_OUTSIDE;
}

// 0x005DBBF0 (.h:66)
bool object_restricted_area::is_disabled() const
{
    return flags.disabled != 0;
}

// 0x00754EF0 (.h:69)
void object_restricted_area::enable()
{
    flags.disabled = 0;
}

// 0x00A1B4C0 (:354). A player inside leaves the count, with no exit event.
void object_restricted_area::disable()
{
    if (flags.player_in_yellow != 0)
    {
        flags.player_in_yellow = 0;
        Restricted_area_player_in_yellow_counter--;
    }
    flags.disabled = 1;
}

// 0x00A6A260 (:366): a building past the capacity is dropped.
void object_restricted_area::add_building(unsigned int building_handle)
{
    for (int i = 0; i < contained_buildings.size(); i++)
    {
        if (contained_buildings[i] == building_handle)
        {
            return;
        }
    }
    if (!contained_buildings.full())
    {
        contained_buildings.add(building_handle);
    }
}

// 0x00A3D6C0 (:379): the last entry takes the place of the removed one.
void object_restricted_area::remove_building(unsigned int building_handle)
{
    for (int i = 0; i < contained_buildings.size(); i++)
    {
        if (contained_buildings[i] == building_handle)
        {
            contained_buildings.remove_unsorted(i);
            return;
        }
    }
}

// 0x00A1B4F0 (:389)
int object_restricted_area::get_yellow_num_points() const
{
    return polygon_data->yellow_num_points;
}

// 0x00A1B500 (:394)
int object_restricted_area::get_warning_num_points() const
{
    return polygon_data->warning_num_points;
}

// 0x00A1B510 (:399): an index out of range leaves point as it was.
void object_restricted_area::get_yellow_point(vector2& point, int i) const
{
    if (i < polygon_data->yellow_num_points && -1 < i)
    {
        point = polygon_data->yellow_polygon[i];
    }
}

// 0x00A1B540 (:405): an index out of range leaves point as it was.
void object_restricted_area::get_warning_point(vector2& point, int i) const
{
    if (i < polygon_data->warning_num_points && -1 < i)
    {
        point = polygon_data->warning_polygon[i];
    }
}

// 0x005DBC00 (.h:85)
bool object_restricted_area::allow_ambient_peds() const
{
    return flags.allow_ambient_peds != 0;
}

// 0x00A1B580 (:439). Goes straight to the polygon, without the box test of is_inside.
void object_restricted_area::process_player_state()
{
    if (flags.disabled != 0 || Player == nullptr)
    {
        return;
    }
    const vector2 player_pos(Player->pos.x, Player->pos.z);
    const bool inside = restricted_area_2d_point_in_polygon(player_pos, polygon_data->yellow_polygon, polygon_data->yellow_num_points);
    if (flags.player_in_yellow == 0 && inside)
    {
        Restricted_area_player_in_yellow_counter++;
        flags.player_in_yellow = 1;
        player_enter_restricted_area_event event;
        event.area_handle = get_handle();
        gem_post_event(event);
    }
    else if (flags.player_in_yellow != 0 && !inside)
    {
        Restricted_area_player_in_yellow_counter--;
        flags.player_in_yellow = 0;
        player_exit_restricted_area_event event;
        event.area_handle = get_handle();
        gem_post_event(event);
    }
}

// 0x00A1B650 (:474). local x runs along fvec and local z along rvec in the object's x and z, which is rfg.exe's mapping.
void object_restricted_area::create_from_local_bbox(const bbox& local_bbox, float warning_radius)
{
    polygon_data->warning_num_points = 4;
    polygon_data->yellow_num_points = 4;
    polygon_data->yellow_polygon[0] = vector2(local_bbox.min.x, local_bbox.min.z);
    polygon_data->yellow_polygon[1] = vector2(local_bbox.min.x, local_bbox.max.z);
    polygon_data->yellow_polygon[2] = vector2(local_bbox.max.x, local_bbox.max.z);
    polygon_data->yellow_polygon[3] = vector2(local_bbox.max.x, local_bbox.min.z);
    polygon_data->warning_polygon[0] = vector2(local_bbox.min.x - warning_radius, local_bbox.min.z - warning_radius);
    polygon_data->warning_polygon[1] = vector2(local_bbox.min.x - warning_radius, local_bbox.max.z + warning_radius);
    polygon_data->warning_polygon[2] = vector2(local_bbox.max.x + warning_radius, local_bbox.max.z + warning_radius);
    polygon_data->warning_polygon[3] = vector2(local_bbox.max.x + warning_radius, local_bbox.min.z - warning_radius);
    for (int polygon = 0; polygon < 2; polygon++)
    {
        vector2* points = polygon == 0 ? polygon_data->yellow_polygon : polygon_data->warning_polygon;
        for (int i = 0; i < 4; i++)
        {
            const vector2 local = points[i];
            points[i].x = local.x * orient.fvec.x + local.y * orient.fvec.z + pos.x;
            points[i].y = local.x * orient.rvec.x + local.y * orient.rvec.z + pos.z;
        }
    }
}

// 0x00A6A2E0 (:540). include_disabled can never find anything, as is_inside and is_inside_warning_area both answer false for a
// disabled area (original).
object_restricted_area* restricted_area_find_area(const vector& point, bool include_warning_areas, bool include_disabled)
{
    object_restricted_area* disabled_area = nullptr;
    const int count = World.get_num_objects<object_restricted_area>();
    for (int i = 0; i < count; i++)
    {
        object_restricted_area* area = World.get_object<object_restricted_area>(i);
        if (!area->is_disabled())
        {
            if (area->is_inside(point) || (include_warning_areas && area->is_inside_warning_area(point)))
            {
                return area;
            }
        }
        else if (include_disabled && disabled_area == nullptr && (area->is_inside(point) || (include_warning_areas && area->is_inside_warning_area(point))))
        {
            disabled_area = area;
        }
    }
    return disabled_area;
}

// 0x00A6A3B0 (:563)
bool restricted_area_in_any_region(const vector& point, bool include_warning_areas)
{
    return restricted_area_find_area(point, include_warning_areas, false) != nullptr;
}

// 0x00A1BC50 (:570)
bool restricted_area_player_in_restricted_area()
{
    return 0 < Restricted_area_player_in_yellow_counter;
}

// 0x00A1BC60 (:575)
void restricted_area_level_init()
{
    Restricted_area_player_in_yellow_counter = 0;
}

// 0x00A6A3D0 (:581)
void restricted_area_do_frame()
{
    if (Multiplayer)
    {
        return;
    }
    const int count = World.get_num_objects<object_restricted_area>();
    for (int i = 0; i < count; i++)
    {
        World.get_object<object_restricted_area>(i)->process_player_state();
    }
}
