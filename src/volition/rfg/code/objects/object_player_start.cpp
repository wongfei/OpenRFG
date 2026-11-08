// Original: volition/rfg/code/objects/object_player_start.cpp
//
// player_start (header comment in object_player_start.h). Compiled in rfg_objects_unity.cpp.

#include "volition/rfg/code/objects/object_player_start.h"

#include "volition/rfg/code/gameplay/mission_info.h"
#include "volition/rfg/code/level/world.h"
#include "volition/rfg/code/objects/object_prop_names_shared.h"
#include "volition/vlib/code/memory/object_pool.h"

static const int PLAYER_START_POOL_SIZE = 256;
static const int PLAYER_START_POOL_USAGE = 0xB540; // memory footprint registered with the instantiator
static const int MISSION_NAME_LENGTH = 256;
static const int TEAM_NAME_LENGTH = 52;

// ---- file-static globals
static object_pool<player_start, PLAYER_START_POOL_SIZE, 1> player_startpool; // 0x035452A0
static obj_class_instantiator player_start_instantiator("player_start", alloc_player_start, types_player_start, bpo_index_player_start, classname_player_start, player_start::pool_remaining, PLAYER_START_POOL_USAGE, PLAYER_START_POOL_SIZE); // 0x0306EF50 (0x01242960)

// ---- class registration (:28)
// 0x00A1ADB0
void types_player_start(signed char& type, signed char& subtype)
{
    type = OT_PLAYER_START;
    subtype = OT_SUB_UNDEFINED;
}

// 0x00A1ADC0
int bpo_index_player_start()
{
    return 1;
}

// 0x00A92A40
void player_start::pool_free()
{
    player_startpool.free(this);
}

// 0x00A6A010
object* alloc_player_start()
{
    return player_startpool.alloc(0);
}

// 0x00A3D5C0
int player_start::pool_remaining()
{
    return player_startpool.get_free();
}

// 0x00A1ADD0
const char* classname_player_start()
{
    return "player_start";
}

// 0x00A1ADE0
unsigned int player_start::get_classname_hash()
{
    return player_start_instantiator.this_hash;
}

// 0x00A1ADF0 (:29)
player_start::player_start()
{
}

// 0x00A1AE20
player_start::~player_start()
{
}

// 0x00AAAA80 (:41)
// Loaded: enabled; indoor, the multiplayer team (by name, none without one), the spawn flags (false when missing)
// and, for checkpoint respawns, the mission it starts. Saved the same way, the mission by name when there is one.
bool player_start::serialize(obj_prop_block& pblock, bool load)
{
    if (!object::serialize(pblock, load))
    {
        return false;
    }
    if (!load)
    {
        pblock.set(PSTART_PROP_INDOOR, indoor);
        pblock.set(PSTART_PROP_MP_TEAM, human_team_get_internal_name(team)); // inlined
        pblock.set(PSTART_PROP_INITIAL_SPAWN, available_for_initial_spawn);
        pblock.set(PSTART_PROP_RESPAWN, available_for_respawn);
        pblock.set(PSTART_PROP_CHECKPOINT_RESPAWN, checkpoint_respawn);
        pblock.set(PSTART_PROP_ACTIVITY_RESPAWN, activity_respawn);
        if (m_handle != INVALID_MISSION_HANDLE)
        {
            pblock.set(PSTART_PROP_MISSION_START_INFO, mission_info_get(m_handle)->name);
        }
        return true;
    }
    enabled = true;
    pblock.get(PSTART_PROP_INDOOR, indoor); // kept unchanged when missing
    team = HUMAN_TEAM_NONE;
    char team_name[TEAM_NAME_LENGTH];
    if (pblock.get_optional(PSTART_PROP_MP_TEAM, team_name))
    {
        team = human_team_from_name(team_name);
    }
    if (!pblock.get_optional(PSTART_PROP_INITIAL_SPAWN, available_for_initial_spawn))
    {
        available_for_initial_spawn = false;
    }
    if (!pblock.get_optional(PSTART_PROP_RESPAWN, available_for_respawn))
    {
        available_for_respawn = false;
    }
    if (!pblock.get_optional(PSTART_PROP_CHECKPOINT_RESPAWN, checkpoint_respawn))
    {
        checkpoint_respawn = false;
    }
    if (!pblock.get_optional(PSTART_PROP_ACTIVITY_RESPAWN, activity_respawn))
    {
        activity_respawn = false;
    }
    m_handle = INVALID_MISSION_HANDLE;
    if (!checkpoint_respawn)
    {
        return true;
    }
    char mission_name[MISSION_NAME_LENGTH];
    mission_name[0] = '\0';
    pblock.get(PSTART_PROP_MISSION_START_INFO, mission_name);
    m_handle = mission_handle_find(mission_name);
    return true;
}

// 0x00A1AE30 (:115)
bool player_start::stream(bool load)
{
    if (!load && obj_flags.destroy_on_stream)
    {
        World.destroy_object(this, false);
    }
    return true;
}

// 0x00A1AE50 (:126)
// Not rotated.
void player_start::get_bbox(vector& bmin, vector& bmax) const
{
    get_local_bbox(bmin, bmax);
    bmin += pos;
    bmax += pos;
}

// 0x00A1AF20 (:134)
// A 1 m cube.
void player_start::get_local_bbox(vector& bmin, vector& bmax) const
{
    bmin.set(-0.5f, -0.5f, -0.5f);
    bmax.set(0.5f, 0.5f, 0.5f);
}

// 0x00755000 (.h:29)
const object_type player_start::get_type_static()
{
    return OT_PLAYER_START;
}

// 0x00755010 (.h:29)
const object_sub_type player_start::get_subtype_static()
{
    return OT_SUB_UNDEFINED;
}

// 0x00A100F0 (.h:29)
const char* player_start::get_classname_static()
{
    return "player_start";
}

// 0x0091C090 (.h:29)
int player_start::get_bpo_index_static()
{
    return 1;
}

// 0x00A1AE10 (.h:29)
const char* player_start::get_classname() const
{
    return "player_start";
}
