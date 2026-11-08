// Original: volition/rfg/code/objects/object_convoy_end_point.cpp
//
// Convoy end points (NPC.md NP1): zone objects that name a convoy definition and own the patrols convoys drive along.
// Registering the class means zones now create them. serialize fails when the definition is unknown, so they only exist
// once convoy_definitions_init has run.

#include "volition/rfg/code/objects/object_convoy_end_point.h"

#include "volition/rfg/code/gameplay/mission_info.h"
#include "volition/rfg/code/gameplay/object_district.h"
#include "volition/rfg/code/level/rfg_prop_block_shared.h"
#include "volition/rfg/code/level/world.h"
#include "volition/rfg/code/objects/object_convoy.h"
#include "volition/rfg/code/objects/object_prop_names_shared.h"
#include "volition/vlib/code/memory/object_pool.h"

static const int MAX_CONVOY_END_POINTS = 16; // the instantiator's capacity, and the farray of convoy_end_get_all_points
static const int CONVOY_END_POINT_LAST_USED_MS = 600000; // 10 minutes
static const int CONVOY_END_POINT_NAME_LEN = 256;

static object_pool<object_convoy_end_point,MAX_CONVOY_END_POINTS,1> object_convoy_end_pointpool; // 0x03429830

// 0x00A16360 (:15)
void types_object_convoy_end_point(signed char& type, signed char& subtype)
{
    type = OT_CONVOY_END;
    subtype = OT_SUB_UNDEFINED;
}

// 0x00A16370 (:15)
int bpo_index_object_convoy_end_point()
{
    return 1;
}

// 0x00A16380 (:15)
const char* classname_object_convoy_end_point()
{
    return "object_convoy_end_point";
}

// 0x00A62340 (:15)
object* alloc_object_convoy_end_point()
{
    return object_convoy_end_pointpool.alloc(0);
}

// Magic numbers: rfg.exe's arguments, the pool's bytes and capacity (0x01076CEB, 0x01076CF6).
static obj_class_instantiator object_convoy_end_point_instantiator("object_convoy_end_point", alloc_object_convoy_end_point, types_object_convoy_end_point, bpo_index_object_convoy_end_point, classname_object_convoy_end_point, object_convoy_end_point::pool_remaining, 0xB50, MAX_CONVOY_END_POINTS); // 0x030FCE44

// 0x00A16070 (.h:24)
const char* object_convoy_end_point::get_classname_static()
{
    return "object_convoy_end_point";
}

// 0x00A16080 (.h:24)
int object_convoy_end_point::get_bpo_index_static()
{
    return 1;
}

// 0x00A163D0 (.h:24)
const char* object_convoy_end_point::get_classname() const
{
    return "object_convoy_end_point";
}

// 0x00A16390 (:15)
unsigned int object_convoy_end_point::get_classname_hash()
{
    return object_convoy_end_point_instantiator.this_hash;
}

// 0x00A3A330 (:15)
int object_convoy_end_point::pool_remaining()
{
    return object_convoy_end_pointpool.get_free();
}

// 0x00A8F240 (:15)
void object_convoy_end_point::pool_free()
{
    object_convoy_end_pointpool.free(this);
}

// 0x00A163A0 (:16)
object_convoy_end_point::object_convoy_end_point()
{
}

// 0x00A163E0
object_convoy_end_point::~object_convoy_end_point()
{
}

// 0x00A8F250 (:20)
void object_convoy_end_point::free_this()
{
    object::free_this();
}

// 0x00A163F0 (:25)
obj_serialize_mode object_convoy_end_point::serialize_mode()
{
    return OSM_FULL;
}

// 0x00AA5BE0 (:30)
bool object_convoy_end_point::serialize(obj_prop_block& pblock, bool load)
{
    if (!object::serialize(pblock, load))
    {
        return false;
    }
    if (!load)
    {
        if (convoy_def != nullptr)
        {
            pblock.set(CONVOY_END_POINT_PROP_TYPE, convoy_def->name);
            if (convoy_node_flags.completed)
            {
                pblock.set(ACTIVITY_COMPLETED_PROP, true);
            }
            if (convoy_node_flags.disabled)
            {
                pblock.set(CONVOY_END_POINT_DISABLED_FLAG, true);
            }
            if (convoy_node_flags.suspend)
            {
                pblock.set(CONVOY_END_POINT_SUSPENDED_FLAG, true);
            }
            const object* district_objp = home_district_handle != 0xFFFFFFFF ? World.object_from_handle(home_district_handle) : nullptr;
            if (district_objp != nullptr && district_objp->get_obj_type() == OT_DISTRICT)
            {
                pblock.set(CONVOY_END_POINT_HOME_DISTRICT, World.get_object_name(district_objp));
            }
        }
        return true;
    }

    home_district_handle = 0xFFFFFFFF;
    convoy_node_flags.completed = 0;
    convoy_node_flags.disabled = 0;
    convoy_node_flags.suspend = 0;
    char type_name[CONVOY_END_POINT_NAME_LEN];
    pblock.get(CONVOY_END_POINT_PROP_TYPE, type_name);
    convoy_def = convoy_definition_from_name(type_name);
    if (convoy_def == nullptr)
    {
        return false;
    }
    last_used_time.set(CONVOY_END_POINT_LAST_USED_MS);
    bool flag;
    if (pblock.get_optional(CONVOY_END_POINT_DISABLED_FLAG, flag))
    {
        convoy_node_flags.disabled = flag;
    }
    if (pblock.get_optional(ACTIVITY_COMPLETED_PROP, flag))
    {
        convoy_node_flags.completed = flag;
    }
    if (pblock.get_optional(CONVOY_END_POINT_SUSPENDED_FLAG, flag))
    {
        convoy_node_flags.suspend = flag;
    }
    char district_name[CONVOY_END_POINT_NAME_LEN];
    if (pblock.get_optional(CONVOY_END_POINT_HOME_DISTRICT, district_name))
    {
        home_district_handle = get_district_by_name(district_name);
    }
    else
    {
        home_district_handle = district_handle_from_pos(pos);
    }
    return home_district_handle != 0xFFFFFFFF;
}

// 0x00A16400 (:113)
bool object_convoy_end_point::get_end_point(vector& convoy_end) const
{
    convoy_end = pos;
    return true;
}

// 0x00A16420 (:128): always true in rfg.exe.
bool object_convoy_end_point::find_convoy_end_point()
{
    return true;
}

// 0x00A3A360 (:143)
bool object_convoy_end_point::can_spawn() const
{
    if (mission_final_mission_complete())
    {
        return true;
    }
    if (convoy_node_flags.completed)
    {
        return false;
    }
    const district* dp = district_get_from_pos(pos);
    return dp == nullptr || !dp->is_liberated();
}

// 0x00A16430 (:171)
const district* object_convoy_end_point::get_end_district() const
{
    return district_get_from_pos(pos);
}

// 0x00754E60 (.h:54)
void object_convoy_end_point::set_completed_flag(bool new_status)
{
    convoy_node_flags.completed = new_status;
}

// 0x00B1FC70 (.h:68)
void object_convoy_end_point::disable_convoy()
{
    convoy_node_flags.disabled = 1;
}

// 0x00754E80 (.h:69)
void object_convoy_end_point::enable_convoy()
{
    convoy_node_flags.disabled = 0;
}

// 0x00B1FC80 (.h:72)
void object_convoy_end_point::suspend_convoy()
{
    convoy_node_flags.suspend = 1;
}

// 0x00B1FC90 (.h:73)
void object_convoy_end_point::release_convoy()
{
    convoy_node_flags.suspend = 0;
}

// 0x00754E90 (.h:82)
bool object_convoy_end_point::is_suspended_or_disabled()
{
    return convoy_node_flags.disabled || convoy_node_flags.suspend;
}

// 0x00754EB0 (.h:85)
unsigned int object_convoy_end_point::get_home_district_handle() const
{
    return home_district_handle;
}

// 0x00A62350 (:193)
bool convoy_end_get_all_points(farray<object_convoy_end_point const*,16>& end_points)
{
    end_points.clear();
    const int count = World.get_num_objects<object_convoy_end_point>();
    for (int i = 0; i < count; i++)
    {
        end_points.add(World.get_object<object_convoy_end_point>(i));
    }
    return end_points.size() > 0;
}

// 0x00A16460 (:207): empty in rfg.exe, gameplay.cpp still calls it.
void convoy_end_find_end_points()
{
}

// 0x00A16470 (:220)
const char* const convoy_end_point_get_name(const object_convoy_end_point* const ocep)
{
    if (ocep != nullptr && ocep->convoy_def != nullptr)
    {
        return ocep->convoy_def->name;
    }
    return nullptr;
}
