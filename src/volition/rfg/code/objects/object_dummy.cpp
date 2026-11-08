// Original: volition/rfg/code/objects/object_dummy.cpp

#include "volition/rfg/code/objects/object_dummy.h"

#include "volition/rfg/code/destruction/rfg_mover.h"
#include "volition/rfg/code/level/rfg_chunk_shared.h"
#include "volition/rfg/code/level/rfg_prop_block_shared.h"
#include "volition/rfg/code/level/world.h"
#include "volition/rfg/code/objects/movers/object_mover.h"
#include "volition/rfg/code/objects/object_cover_node.h"
#include "volition/rfg/code/objects/object_prop_names_shared.h"
#include "volition/rfg/code/pathfinding/bb_pathfinding.h"
#include "volition/vlib/code/memory/object_pool.h"
#include "volition/vlib/code/util/hash_table.h"
#include "volition/vlib/code/util/varray.h"

static const int MAX_DUMMY_OBJECTS = 256;

static object_pool<object_dummy, MAX_DUMMY_OBJECTS, 1> object_dummypool; // 0x033B5910
static const c_chunk_base* BARRICADE_CHUNK; // 0x0302F13C

// Built by the variadic constructor (0x01077340). "Tech Reponse Pos" (0x01319744) is misspelled in rfg.exe, and the zone data has to match it.
static str_data_hash_table<unsigned int, 32, 32, unsigned char> Dummy_object_type_table( // 0x033D6178
    "None", (unsigned int)DUMMY_OBJECT_TYPE_NONE,
    "Demolition", (unsigned int)DUMMY_OBJECT_TYPE_DEMOLITION_POINT,
    "Smoke Plume", (unsigned int)DUMMY_OBJECT_TYPE_RAID_SMOKE_PLUME,
    "Reinforced_Fence", (unsigned int)DUMMY_OBJECT_TYPE_REINFORCED_FENCE,
    "Barricade", (unsigned int)DUMMY_OBJECT_TYPE_ACTIVITY_BARRICADE,
    "Rally", (unsigned int)DUMMY_OBJECT_TYPE_RALLY,
    "Air Bomb", (unsigned int)DUMMY_OBJECT_TYPE_AIR_BOMB_POINT,
    "Cutscene", (unsigned int)DUMMY_OBJECT_TYPE_CUTSCENE,
    "Demo Master", (unsigned int)DUMMY_OBJECT_TYPE_DEMO_MASTER,
    "VRail Spawn", (unsigned int)DUMMY_OBJECT_TYPE_VRAIL_SPAWN,
    "Tech Reponse Pos", (unsigned int)DUMMY_OBJECT_TYPE_TECH_RESPONSE,
    nullptr, 0u);

// 0x00A633D0 (:49). LTCG: objp in ESI, chunk on the stack. A barricade mover with a cover node on each side, facing away
// from it. Magic numbers: the nodes stand 0.6 out, with 70 degree defensive and 60 degree offensive arcs each way.
static void create_barricade(const object_dummy* objp, const c_chunk_base* chunk)
{
    if (objp == nullptr || chunk == nullptr)
    {
        return;
    }
    obj_prop_block props;
    vector pos = objp->pos;
    rfg_snap_to_ground(pos);
    object_mover* mover = object_mover_create_from_chunk(pos, objp->orient, chunk, objp->get_handle());
    if (mover == nullptr)
    {
        return;
    }
    mover->obj_flags.no_save = 1;
    mover->obj_flags.destroy_on_stream = 1;

    props.set<float>(CN_PROP_DEFENSIVE_ANGLE_LEFT, 70.0f);
    props.set<float>(CN_PROP_DEFENSIVE_ANGLE_RIGHT, 70.0f);
    props.set<float>(CN_PROP_OFFENSIVE_ANGLE_LEFT, 60.0f);
    props.set<float>(CN_PROP_OFFENSIVE_ANGLE_RIGHT, 60.0f);
    cover_node_flags node_flags = {};
    node_flags.crouch = 1;
    node_flags.fire_popup = 1;
    props.set<cover_node_flags>(CN_PROP_ALL_FLAGS, node_flags);

    object_properties node;
    node.pos = pos + objp->orient.fvec * 0.6f;
    rfg_snap_to_ground(node.pos);
    node.orient = objp->orient;
    node.orient.rvec = -objp->orient.rvec;
    node.orient.fvec = -objp->orient.fvec;
    props.set<object_properties>(OBJ_PROP_COMBINED, node);
    World.create_object<cover_node>(props, mover->get_handle());

    node.pos = pos - objp->orient.fvec * 0.6f;
    rfg_snap_to_ground(node.pos);
    node.orient = objp->orient;
    props.set<object_properties>(OBJ_PROP_COMBINED, node);
    World.create_object<cover_node>(props, mover->get_handle());
}

// 0x00A17D30 (:115)
void types_object_dummy(signed char& type, signed char& subtype)
{
    type = OT_DUMMY;
    subtype = OT_SUB_UNDEFINED;
}

// 0x00A17D40 (:115)
int bpo_index_object_dummy()
{
    return 1;
}

// 0x00A17D50 (:115)
const char* classname_object_dummy()
{
    return "object_dummy";
}

// 0x00A63850 (:115)
object* alloc_object_dummy()
{
    return object_dummypool.alloc(0);
}

// Magic numbers: rfg.exe's arguments (0x010773D0), the pool's bytes and capacity.
static obj_class_instantiator object_dummy_instantiator("object_dummy", alloc_object_dummy, types_object_dummy, bpo_index_object_dummy, classname_object_dummy, object_dummy::pool_remaining, 0xA540, MAX_DUMMY_OBJECTS); // 0x031A203C

// 0x00A17D60 (:115)
unsigned int object_dummy::get_classname_hash()
{
    return object_dummy_instantiator.this_hash;
}

// 0x00A3AFF0 (:115)
int object_dummy::pool_remaining()
{
    return object_dummypool.get_free();
}

// 0x00A901C0 (:115)
void object_dummy::pool_free()
{
    object_dummypool.free(this);
}

// 0x00A17D70 (:116)
object_dummy::object_dummy()
{
}

// 0x00A17DA0
object_dummy::~object_dummy()
{
}

// 0x00A15C80 (.h:50)
const char* object_dummy::get_classname_static()
{
    return "object_dummy";
}

// 0x00A17D90 (.h:50)
const char* object_dummy::get_classname() const
{
    return "object_dummy";
}

// 0x00A15C90 (.h:50)
int object_dummy::get_bpo_index_static()
{
    return 1;
}

// 0x00A17DB0 (:121)
void object_dummy::hierarchy_notify(hierarchy_notify_code code)
{
    if (code == HN_PARENT_DESTROYED)
    {
        World.destroy_object(this, false);
    }
}

// 0x00AA6EF0 (:129)
bool object_dummy::serialize(obj_prop_block& pblock, bool load)
{
    if (!object::serialize(pblock, load))
    {
        return false;
    }
    if (!load)
    {
        pblock.set<dummy_object_types>(DUMMY_OBJECT_PROP_TYPE_INTERNAL, dummy_type);
        return true;
    }
    dummy_type = DUMMY_OBJECT_TYPE_INVALID;
    if (!pblock.get_optional(DUMMY_OBJECT_PROP_TYPE_INTERNAL, dummy_type))
    {
        pblock.get_str_as_enum(DUMMY_OBJECT_PROP_TYPE, dummy_type, Dummy_object_type_table);
    }
    if (dummy_type == DUMMY_OBJECT_TYPE_INVALID)
    {
        return false;
    }
    // Magic number: byte 2 of a handle is 0xFF on a dummy created at run time, which is not saved. Inferred from the check.
    if (((get_handle() >> 16) & 0xFF) == 0xFF)
    {
        obj_flags.no_save = 1;
    }
    return true;
}

// 0x00AA6FC0 (:161)
void object_dummy::create_dummy_object() const
{
    farray<const rfg_mover*, 4> created;
    world_collect_descendants<rfg_mover>(this, created, nullptr, nullptr);
    if (created.size() > 0)
    {
        return;
    }
    // A reinforced fence only reaches a debug trap in rfg.exe ("Int3 critical function"), so it creates nothing.
    if (dummy_type == DUMMY_OBJECT_TYPE_ACTIVITY_BARRICADE)
    {
        const c_chunk_base* chunk = obj_dummy_get_barricade_chunk();
        if (chunk != nullptr)
        {
            create_barricade(this, chunk);
        }
    }
}

// 0x00A17DD0 (:197)
const c_chunk_base* obj_dummy_get_barricade_chunk()
{
    if (BARRICADE_CHUNK == nullptr)
    {
        BARRICADE_CHUNK = chunk_get_base_by_name("edfbarricade_vehicle_a.rfgchunkx");
    }
    return BARRICADE_CHUNK;
}

// 0x00758440 (.h:65)
bool object_dummy::is_creatable_object() const
{
    return dummy_type == DUMMY_OBJECT_TYPE_ACTIVITY_BARRICADE || dummy_type == DUMMY_OBJECT_TYPE_REINFORCED_FENCE;
}
