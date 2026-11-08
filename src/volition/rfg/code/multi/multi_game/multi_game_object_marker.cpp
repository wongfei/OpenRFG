// Original: volition/rfg/code/multi/multi_game/multi_game_object_marker.cpp
//
// Decompiled whole (multiplayer.md MP10). The level's multiplayer markers (object type 0x21): siege targets, backpack
// racks, spawn nodes, flag capture zones, king of the hill targets and spectator cameras, read from the level's
// properties (type, team, priority, a box; a rack's backpack type, count, random group). A marker attached to a
// building moves with it (its box is relative to the parent); a rack on a building whose three "attach_pack_here_0N"
// tags exist hangs its backpacks there. After level load the markers are listed by priority.
// Part of the multi unity TU (rfg_multi_unity.obj, every multi/ .cpp in path order): the file statics are shared
// by the whole tree, and a shared one is placed in its first user (tools/hdrgen/specs/mp0_stubs.py).

#include "volition/rfg/code/multi/multi_game/multi_game_object_marker.h"

#include "volition/rfg/code/destruction/rfg_mover.h"
#include "volition/rfg/code/level/rfg_prop_block_shared.h"
#include "volition/rfg/code/level/world.h"
#include "volition/rfg/code/level/world.inl"
#include "volition/rfg/code/multi/multi_core/include/multi.h"
#include "volition/rfg/code/multi/multi_core/include/multi_core_objects.h"
#include "volition/rfg/code/multi/multi_game/modes/multi_game_mode_common.h"
#include "volition/rfg/code/multi/multi_game/multi_game_object_backpack.h"
#include "volition/rfg/code/objects/object_prop_names_shared.h"
#include "volition/vlib/code/memory/object_pool.h"

#include <float.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const int MULTI_OBJECT_MARKER_POOL_SIZE = 128;
static const int MULTI_OBJECT_MARKER_POOL_USAGE = 0x7AC0; // memory footprint registered with the instantiator

// ---- file-static globals
// The marker type names of the level properties, in multi_marker_types order.
static const char* Multi_marker_type_names[NUM_MULTI_MARKER_TYPES] = {"Siege target", "Backpack rack", "Spawn node", "Flag capture zone", "King of the Hill target", "Spectator camera"}; // 0x0167F06C
// The tags of a rack's building its backpacks hang on.
static const char* Backpack_rack_attach_tag_names[3] = {"attach_pack_here_01", "attach_pack_here_02", "attach_pack_here_03"}; // 0x0167F084
// The general movers' base priorities (used by multi_game_object_movers.cpp; low for every data set).
static multi_core_state_priority State_priority_base_movers[5] = {MULTI_CORE_STATE_PRIORITY_LOW, MULTI_CORE_STATE_PRIORITY_LOW, MULTI_CORE_STATE_PRIORITY_LOW, MULTI_CORE_STATE_PRIORITY_LOW, MULTI_CORE_STATE_PRIORITY_LOW}; // 0x0167F090
// multi_backpack_info* Backpack_info[16] (0x02FE8D18) is used here too; placed in multi_game_backpack_rack.cpp.
static object_pool<multi_object_marker, MULTI_OBJECT_MARKER_POOL_SIZE, 1> multi_object_markerpool; // 0x0300F8E0 (0x01071330)
static obj_class_instantiator multi_object_marker_instantiator("multi_object_marker", alloc_multi_object_marker, types_multi_object_marker, bpo_index_multi_object_marker, classname_multi_object_marker,
                                                               multi_object_marker::pool_remaining, MULTI_OBJECT_MARKER_POOL_USAGE, MULTI_OBJECT_MARKER_POOL_SIZE); // 0x02FEBBC8 (0x01071300)

multi_object_marker* Multi_markers_prioritized[128]; // 0x02FE7188

// ---- internal functions
// 0x00959E70 (:52): by priority, lowest first
static int multi_marker_sort(const void* elem1, const void* elem2)
{
    const int p1 = (*(const multi_object_marker* const*)elem1)->priority;
    const int p2 = (*(const multi_object_marker* const*)elem2)->priority;
    if (p2 < p1)
    {
        return 1;
    }
    return p2 <= p1 ? 0 : -1;
}

// ---- class registration (:76)
// 0x00959EB0
void types_multi_object_marker(signed char& type, signed char& subtype)
{
    type = OT_MULTI_MARKER;
    subtype = OT_SUB_UNDEFINED;
}

// 0x00959EC0
int bpo_index_multi_object_marker()
{
    return 1;
}

// 0x00959ED0
const char* classname_multi_object_marker()
{
    return "multi_object_marker";
}

// 0x00959EE0
unsigned int multi_object_marker::get_classname_hash()
{
    return multi_object_marker_instantiator.this_hash;
}

// 0x0097DB50
int multi_object_marker::pool_remaining()
{
    return multi_object_markerpool.get_free();
}

// 0x009B0BF0
void multi_object_marker::pool_free()
{
    multi_object_markerpool.free(this);
}

// 0x0099BA40
object* alloc_multi_object_marker()
{
    return multi_object_markerpool.alloc(0);
}

// 0x00959EF0 (:77)
multi_object_marker::multi_object_marker()
{
}

// 0x00959F20
multi_object_marker::~multi_object_marker()
{
}

// 0x00959F10 (.h:58)
const char* multi_object_marker::get_classname() const
{
    return "multi_object_marker";
}

// 0x0094CB60 (.h:58)
const object_type multi_object_marker::get_type_static()
{
    return OT_MULTI_MARKER;
}

// 0x0094CB70 (.h:58)
const object_sub_type multi_object_marker::get_subtype_static()
{
    return OT_SUB_UNDEFINED;
}

// 0x0094CB80 (.h:58)
const char* multi_object_marker::get_classname_static()
{
    return "multi_object_marker";
}

// 0x0094CB90 (.h:58)
int multi_object_marker::get_bpo_index_static()
{
    return 1;
}

// 0x0094CBA0 (.h:80)
multi_marker_types multi_object_marker::get_marker_type() const
{
    return marker_type;
}

// 0x0094CBB0 (.h:88)
bool multi_object_marker::is_random_rack() const
{
    return random_rack;
}

// 0x0094CBC0 (.h:89)
bool multi_object_marker::can_attach_packs() const
{
    return can_attach_packs_normally;
}

// 0x0099BA50 (:82)
bool multi_object_marker::serialize(obj_prop_block& pblock, bool load)
{
    if (!object::serialize(pblock, load))
    {
        return false;
    }
    if (load)
    {
        local_bbox.min.zero();
        local_bbox.max.zero();
        pblock.get(PROP_MULTI_MARKER_BB, local_bbox);
        offset = ZERO_VECTOR;
        if (get_parent() != 0xFFFFFFFF)
        {
            // Original: the parent is dereferenced before it is checked to be an rfg_mover.
            object* parent_objp = World.object_from_handle(get_parent());
            offset = parent_objp->pos - pos;
            rfg_mover* rm = parent_objp->get_obj_type() == OT_MOVER && parent_objp->get_sub_type() == OT_SUB_MOVER_RFG ? (rfg_mover*)parent_objp : nullptr;
            rfg_mover_set_no_fade_destroy(rm);
        }
        destroyed = false;
        marker_type = MMT_NONE;
        char type_name[32] = {};
        pblock.get(PROP_MULTI_MARKER_TYPE, type_name);
        marker_type = multi_game_marker_get_type_by_name(type_name);
        if (marker_type == MMT_NONE)
        {
            return false;
        }
        char team_name[64];
        if (!Multiplayer || !multi_game_mode_has_teams())
        {
            team = HUMAN_TEAM_CIVILIAN;
        }
        else
        {
            pblock.get(PROP_MULTI_MARKER_MP_TEAM, team_name);
            team = human_team_from_name(team_name);
        }
        // Original bug kept: the team is read again whatever the mode, overwriting the civilian team above.
        memset(team_name, 0, sizeof(team_name));
        pblock.get(PROP_MULTI_MARKER_MP_TEAM, team_name);
        team = human_team_from_name(team_name);
        // Magic number: no priority sorts first.
        priority = -0x7FFFFFFF;
        pblock.get(PROP_MULTI_MARKER_PRIORITY, priority);
        backpack_rack_light = 0xFFFFFFFF;
        if (marker_type == MMT_BACKPACK_RACK)
        {
            char backpack_name[32] = {};
            pblock.get(PROP_MULTI_MARKER_BACKPACK_TYPE, backpack_name);
            backpack_id = multi_game_backpack_get_backpack_id(backpack_name);
            // Original: "Invalid backpack type (%s) specified." is formatted but never shown.
            num_backpacks = 0;
            pblock.get(PROP_MULTI_MARKER_NUM_BACKPACKS, num_backpacks);
            // Original: "You can only have %d backpacks on each rack. You have %d." is formatted but never shown.
            if (num_backpacks > 2)
            {
                num_backpacks = 3;
            }
            random_rack = false;
            pblock.get(PROP_MULTI_MARKER_RANDOM_BACKPACKS, random_rack);
            backpack_rack_group = 0;
            pblock.get(PROP_MULTI_MARKER_GROUP, backpack_rack_group);
            can_attach_packs_normally = false;
        }
        backpacks[0] = 0xFFFFFFFF;
        backpacks[1] = 0xFFFFFFFF;
        backpacks[2] = 0xFFFFFFFF;
    }
    else
    {
        pblock.set(PROP_MULTI_MARKER_BB, local_bbox);
        pblock.set(PROP_MULTI_MARKER_TYPE, Multi_marker_type_names[marker_type]);
        pblock.set(PROP_MULTI_MARKER_MP_TEAM, human_team_get_internal_name(team));
        pblock.set(PROP_MULTI_MARKER_PRIORITY, priority);
        pblock.set(PROP_MULTI_MARKER_BACKPACK_TYPE, multi_game_backpack_get_internal_name(backpack_id));
        pblock.set(PROP_MULTI_MARKER_NUM_BACKPACKS, num_backpacks);
        pblock.set(PROP_MULTI_MARKER_RANDOM_BACKPACKS, random_rack);
    }
    return true;
}

// 0x0097DB80 (:185): a rack on a building hangs its backpacks on the building when it has all three attach tags
bool multi_object_marker::stream(bool load)
{
    if (!object::stream(load))
    {
        return false;
    }
    if (load && marker_type == MMT_BACKPACK_RACK && get_parent() != 0xFFFFFFFF)
    {
        object* parent_objp = World.object_from_handle(get_parent());
        if (parent_objp != nullptr && parent_objp->get_obj_type() == OT_MOVER && parent_objp->get_sub_type() == OT_SUB_MOVER_RFG)
        {
            can_attach_packs_normally = true;
            for (int i = 0; i < 3; i++)
            {
                if (parent_objp->get_tag_index_from_name(Backpack_rack_attach_tag_names[i]) == -1)
                {
                    can_attach_packs_normally = false;
                    break;
                }
            }
        }
    }
    return true;
}

// 0x00959F30 (:217): the box in the world (attached: around the parent less the offset)
void multi_object_marker::get_bbox(vector& bmin, vector& bmax) const
{
    if (get_parent() == 0xFFFFFFFF)
    {
        bmin = pos + local_bbox.min;
        bmax = pos + local_bbox.max;
        return;
    }
    const object* parent_objp = World.object_from_handle(get_parent());
    bmin = local_bbox.min + parent_objp->pos - offset;
    bmax = parent_objp->pos + local_bbox.max - offset;
}

// 0x0095A1B0 (:230)
void multi_object_marker::get_local_bbox(vector& bmin, vector& bmax) const
{
    bmin = local_bbox.min;
    bmax = local_bbox.max;
}

// 0x0095A1F0 (:238)
void multi_object_marker::hierarchy_notify(hierarchy_notify_code code)
{
    object::hierarchy_notify(code);
    if (code == HN_PARENT_DESTROYED)
    {
        World.destroy_object(this, false);
    }
}

// 0x0099BEE0 (:260): every marker, by priority
void multi_game_marker_level_init_post()
{
    memset(Multi_markers_prioritized, 0, sizeof(Multi_markers_prioritized));
    const int num = World.get_num_objects<multi_object_marker>();
    for (int i = 0; i < num; i++)
    {
        Multi_markers_prioritized[i] = World.get_object<multi_object_marker>(i);
    }
    qsort(Multi_markers_prioritized, num, sizeof(multi_object_marker*), multi_marker_sort);
}

// 0x0095A220 (:275)
multi_marker_types multi_game_marker_get_type_by_name(const char* name)
{
    for (int type = MMT_SIEGE_TARGET; type < NUM_MULTI_MARKER_TYPES; type++)
    {
        if (strcmp(name, Multi_marker_type_names[type]) == 0)
        {
            return (multi_marker_types)type;
        }
    }
    return MMT_NONE;
}

// 0x0095A280 (:289)
const char* multi_game_marker_get_name_by_type(multi_marker_types type)
{
    return Multi_marker_type_names[type];
}

// 0x0099BF50 (:298)
multi_object_marker* multi_game_marker_find_closest(multi_marker_types type, const vector& pos)
{
    float best = FLT_MAX;
    multi_object_marker* closest = nullptr;
    for (int i = World.get_num_objects<multi_object_marker>() - 1; i >= 0; i--)
    {
        multi_object_marker* marker = Multi_markers_prioritized[i];
        if (marker->get_marker_type() != type)
        {
            continue;
        }
        const float dist = vec_dist_squared(pos, marker->pos);
        if (dist < best)
        {
            closest = marker;
            best = dist;
        }
    }
    return closest;
}

// 0x0099BFD0 (:317): the marker of the same type before this one in the priority list (going down), wrapping to the
// last one
multi_object_marker* multi_game_marker_get_prev(multi_object_marker* marker)
{
    if (marker == nullptr)
    {
        return nullptr;
    }
    int wrap = -1;
    int found = -1;
    for (int i = World.get_num_objects<multi_object_marker>() - 1; i >= 0; i--)
    {
        if (Multi_markers_prioritized[i] == marker)
        {
            found = i;
            continue;
        }
        if (Multi_markers_prioritized[i]->get_marker_type() == marker->get_marker_type())
        {
            if (found != -1)
            {
                return Multi_markers_prioritized[i];
            }
            if (wrap == -1)
            {
                wrap = i;
            }
        }
    }
    return wrap != -1 ? Multi_markers_prioritized[wrap] : nullptr;
}

// 0x0099C040 (:349): the marker of the same type after this one in the priority list, wrapping to the first one
multi_object_marker* multi_game_marker_get_next(multi_object_marker* marker)
{
    if (marker == nullptr)
    {
        return nullptr;
    }
    int wrap = -1;
    int found = -1;
    for (int i = 0; i < World.get_num_objects<multi_object_marker>(); i++)
    {
        if (Multi_markers_prioritized[i] == marker)
        {
            found = i;
            continue;
        }
        if (Multi_markers_prioritized[i]->get_marker_type() == marker->get_marker_type())
        {
            if (found != -1)
            {
                return Multi_markers_prioritized[i];
            }
            if (wrap == -1)
            {
                wrap = i;
            }
        }
    }
    return wrap != -1 ? Multi_markers_prioritized[wrap] : nullptr;
}

// 0x0099C0B0 (:379)
int multi_game_marker_get_num(multi_marker_types type)
{
    int count = 0;
    for (int i = World.get_num_objects<multi_object_marker>() - 1; i >= 0; i--)
    {
        if (Multi_markers_prioritized[i]->get_marker_type() == type)
        {
            count++;
        }
    }
    return count;
}
