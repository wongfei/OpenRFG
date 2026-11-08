// Original: volition/rfg/code/objects/object_trigger_region.cpp
//
// Trigger regions (stub_impl.md S9): box or sphere volumes placed in zones. trigger_region_do_frame posts the enter and
// exit events scripts wait for, and kills humans inside a kill region. It only tests humans: the prologue's
// object_in_trigger_region check on the flyer is the script function's own test (trigger_region_point_inside).

#include "volition/rfg/code/objects/object_trigger_region.h"

#include "volition/rfg/code/code_effects/explosion.h"
#include "volition/rfg/code/code_effects/explosion_shared.h"
#include "volition/rfg/code/gameplay/activities/activity.h"
#include "volition/rfg/code/gameplay/missions.h"
#include "volition/rfg/code/gameplay/wrecking_crew/wrecking_crew.h"
#include "volition/rfg/code/havok/havok_bp.h"
#include "volition/rfg/code/level/rfg_prop_name_shared.h"
#include "volition/rfg/code/level/world.h"
#include "volition/rfg/code/multi/multi_core/include/multi.h"
#include "volition/rfg/code/multi/multi_game/modes/multi_game_mode_common.h"
#include "volition/rfg/code/multi/multi_game/multi_game_weapons.h"
#include "volition/rfg/code/objects/humans/human.h"
#include "volition/rfg/code/objects/humans/player.h"
#include "volition/rfg/code/objects/object_prop_names_shared.h"
#include "volition/rfg/code/scripting/action_script/global_event_data.h"
#include "volition/rfg/code/scripting/action_script/global_event_manager.h"
#include "volition/rfg/code/vehicle/vehicle_class.h"
#include "volition/vlib/code/math/ix.h"
#include "volition/vlib/code/math/matrix.h"
#include "volition/vlib/code/memory/object_pool.h"
#include "volition/vlib/code/util/hash_table.h"
#include "volition/vlib/code/util/dictionary.h"

// Magic number: rfg.exe's pool.
static object_pool<trigger_region,106,1> trigger_regionpool; // 0x0342A3A0
static const explosion_info* Mine_killzone_explosion; // 0x0302F21C
static const char* MINE_KILLZONE_EXPLOSION_NAME = "mine_killzone"; // 0x01694704
static const char* TRIGGER_REGION_TYPE_NAMES[NUM_TRIGGER_REGION_TYPES] = { "default", "kill human" }; // 0x01694708
static const char* TRIGGER_REGION_SHAPE_NAMES[NUM_TRIGGER_REGION_SHAPES] = { "box", "sphere" }; // 0x01694710
static const char* TRIGGER_REGION_KILL_HUMAN_TYPE_NAMES[NUM_TRIGGER_REGION_KILL_HUMAN_TYPES] = { "cliff", "mine" }; // 0x01694718

// Magic numbers: tr_flags bit 0 turns the region off during a mission, bit 1 during an activity.
static const int TR_FLAG_NOT_IN_MISSION = 1;
static const int TR_FLAG_NOT_IN_ACTIVITY = 2;
static str_data_hash_table<unsigned int, 32, 32, unsigned char> Trigger_flags_table( // 0x033B4E68
    "not_in_mission", TR_FLAG_NOT_IN_MISSION,
    "not_in_activity", TR_FLAG_NOT_IN_ACTIVITY,
    nullptr, 0);

// 0x00A1DC20 (:97)
void types_trigger_region(signed char& type, signed char& subtype)
{
    type = OT_TRIGGER_REGION;
    subtype = -1;
}

// 0x00A1DC30 (:97)
int bpo_index_trigger_region()
{
    return 1;
}

// 0x00A1DC40 (:97)
const char* classname_trigger_region()
{
    return "trigger_region";
}

// 0x00A71130 (:97)
object* alloc_trigger_region()
{
    return trigger_regionpool.alloc(0);
}

static obj_class_instantiator trigger_region_instantiator("trigger_region", alloc_trigger_region, types_trigger_region, bpo_index_trigger_region, classname_trigger_region, trigger_region::pool_remaining, 0x65C0, 106);

// 0x00A1DC50 (:97)
unsigned int trigger_region::get_classname_hash()
{
    return trigger_region_instantiator.this_hash;
}

// 0x00A40CA0 (:97)
int trigger_region::pool_remaining()
{
    return trigger_regionpool.get_free();
}

// 0x00A952F0 (:97)
void trigger_region::pool_free()
{
    trigger_regionpool.free(this);
}

// 0x00A1DC60 (:98)
trigger_region::trigger_region()
{
}

// 0x00A1DC90
trigger_region::~trigger_region()
{
}

// 0x00756BC0 (.h:54)
const object_type trigger_region::get_type_static()
{
    return OT_TRIGGER_REGION;
}

// 0x00756BD0 (.h:54)
const object_sub_type trigger_region::get_subtype_static()
{
    return OT_SUB_UNDEFINED;
}

// 0x00A06A60 (.h:54)
const char* trigger_region::get_classname_static()
{
    return "trigger_region";
}

// 0x00A06A70 (.h:54)
int trigger_region::get_bpo_index_static()
{
    return 1;
}

// 0x00A1DC80 (.h:54)
const char* trigger_region::get_classname() const
{
    return "trigger_region";
}

// 0x00A06A80 (.h:104)
trigger_region_kill_human_types trigger_region::get_trigger_region_kill_human_type() const
{
    return tr_kill_type;
}

// 0x00AAD690 (:110)
bool trigger_region::serialize(obj_prop_block& pblock, bool load)
{
    if (!object::serialize(pblock, load))
    {
        return false;
    }
    if (load)
    {
        char name[64];
        tr_shape = pblock.get_optional(TR_PROP_SHAPE, name) ? trigger_region_shape_by_name(name) : TRIGGER_REGION_SHAPE_BOX;
        if (tr_shape == TRIGGER_REGION_SHAPE_BOX)
        {
            pblock.get(TR_PROP_BOX, local_bb);
            radius = 0.0f;
        }
        else
        {
            pblock.get_optional(TR_PROP_RADIUS, radius);
            local_bb.min.x = -radius;
            local_bb.min.y = -radius;
            local_bb.min.z = -radius;
            local_bb.max.x = radius;
            local_bb.max.y = radius;
            local_bb.max.z = radius;
        }
        bbox_instance(bb, orient, pos, local_bb);
        enabled = true;
        pblock.get_optional(TR_PROP_ENABLED, enabled);
        tr_type = pblock.get_optional(TR_PROP_TYPE, name) ? trigger_region_type_by_name(name) : TRIGGER_REGION_TYPE_DEFAULT;
        tr_kill_type = pblock.get_optional(TR_PROP_KILL_TYPE, name) ? trigger_region_kill_human_type_by_name(name) : TRKHT_CLIFF;
        tr_flags = 0;
        pblock.get_str_as_flags_optional(TR_PROP_FLAGS, tr_flags, Trigger_flags_table);
        last_pos = pos;
    }
    else
    {
        pblock.set(TR_PROP_BOX, local_bb);
        pblock.set(TR_PROP_RADIUS, radius);
        pblock.set(TR_PROP_SHAPE, TRIGGER_REGION_SHAPE_NAMES[tr_shape]);
        pblock.set(TR_PROP_ENABLED, enabled);
        pblock.set(TR_PROP_TYPE, TRIGGER_REGION_TYPE_NAMES[tr_type]);
    }
    return true;
}

// 0x00A1DCA0 (:195)
obj_serialize_mode trigger_region::serialize_mode()
{
    return OSM_FULL;
}

// 0x00A1DCB0 (:207)
bool trigger_region::stream(bool load)
{
    if (!load && obj_flags.destroy_on_stream)
    {
        World.destroy_object(this, false);
    }
    return true;
}

// 0x00A95300 (:238)
void trigger_region::free_this()
{
    object::free_this();
}

// 0x00A1DCD0 (:244)
void trigger_region::get_bbox(vector& bmin, vector& bmax) const
{
    bmin = bb.min;
    bmax = bb.max;
}

// 0x00A1DD10 (:251)
void trigger_region::get_local_bbox(vector& bmin, vector& bmax) const
{
    bmin = local_bb.min;
    bmax = local_bb.max;
}

// 0x00A1DD50 (:261)
void trigger_region::update_pos_and_orient(const vector& update_pos, const matrix& update_orient, bool set_havok_data)
{
    pos = update_pos;
    orient = update_orient;
    if (bp_handle != nullptr)
    {
        get_bbox(last_known_bmin, last_known_bmax);
        havok_bpo_update(bp_handle, last_known_bmin, last_known_bmax);
    }
}

// 0x00A1DDD0 (:272)
void trigger_region::destroy_notify()
{
}

// 0x00A1DDE0 (:282)
float trigger_region::get_radius() const
{
    return radius;
}

// 0x00A1DDF0 (:310)
void trigger_region::enable()
{
    enabled = true;
}

// 0x00A1DE00 (:316)
void trigger_region::disable()
{
    enabled = false;
}

// 0x00A1DE10 (:322)
bool trigger_region::is_enabled() const
{
    bool result = enabled;
    if ((tr_flags & TR_FLAG_NOT_IN_MISSION) != 0 && mission_is_active())
    {
        result = false;
    }
    if ((tr_flags & TR_FLAG_NOT_IN_ACTIVITY) != 0 && !activity_none())
    {
        return false;
    }
    return result;
}

// 0x00A1DE50 (:339)
trigger_region_type trigger_region::get_trigger_region_type() const
{
    return tr_type;
}

// 0x00A1DE60 (:344)
trigger_region_shape trigger_region::get_trigger_region_shape() const
{
    return tr_shape;
}

// 0x00A1DE70 (:368)
void trigger_region_system_init()
{
    Mine_killzone_explosion = explosion_lookup(MINE_KILLZONE_EXPLOSION_NAME);
}

// 0x00A1DE90 (:405)
bool trigger_region_point_inside(const trigger_region* trigger, const vector& point)
{
    if (trigger->get_trigger_region_shape() == TRIGGER_REGION_SHAPE_BOX)
    {
        vector bmin, bmax;
        trigger->get_local_bbox(bmin, bmax);
        return ix_point_in_box_oriented(point, trigger->pos, trigger->orient, bmin, bmax);
    }
    if (trigger->get_trigger_region_shape() == TRIGGER_REGION_SHAPE_SPHERE)
    {
        return ix_point_in_sphere(trigger->pos, trigger->get_radius(), point);
    }
    return false;
}

// 0x00A1DF10 (:423)
trigger_region_type trigger_region_type_by_name(const char* name)
{
    for (int i = 0; i < NUM_TRIGGER_REGION_TYPES; i++)
    {
        if (v_stricmp(TRIGGER_REGION_TYPE_NAMES[i], name) == 0)
        {
            return (trigger_region_type)i;
        }
    }
    return TRIGGER_REGION_TYPE_DEFAULT;
}

// 0x00A1DF50 (:435)
trigger_region_shape trigger_region_shape_by_name(const char* name)
{
    for (int i = 0; i < NUM_TRIGGER_REGION_SHAPES; i++)
    {
        if (v_stricmp(TRIGGER_REGION_SHAPE_NAMES[i], name) == 0)
        {
            return (trigger_region_shape)i;
        }
    }
    return TRIGGER_REGION_SHAPE_BOX;
}

// 0x00A1DF90 (:447)
trigger_region_kill_human_types trigger_region_kill_human_type_by_name(const char* name)
{
    for (int i = 0; i < NUM_TRIGGER_REGION_KILL_HUMAN_TYPES; i++)
    {
        if (v_stricmp(TRIGGER_REGION_KILL_HUMAN_TYPE_NAMES[i], name) == 0)
        {
            return (trigger_region_kill_human_types)i;
        }
    }
    return TRKHT_INVALID;
}

// 0x00AD6600 (:460)
void trigger_region_do_frame()
{
    player* pp = Player;
    for (int i = 0; i < World.get_num_objects<trigger_region>(); i++)
    {
        trigger_region* trigger = World.get_object<trigger_region>(i);
        if (trigger->is_enabled())
        {
            for (int j = 0; j < World.get_num_objects<human>(); j++)
            {
                human* hp = World.get_object<human>(j);
                bool was_inside = false;
                bool is_inside = false;
                if (trigger->get_trigger_region_shape() == TRIGGER_REGION_SHAPE_BOX)
                {
                    vector bmin, bmax;
                    trigger->get_local_bbox(bmin, bmax);
                    was_inside = ix_point_in_box_oriented(hp->last_pos, trigger->last_pos, trigger->orient, bmin, bmax);
                    is_inside = ix_point_in_box_oriented(hp->pos, trigger->pos, trigger->orient, bmin, bmax);
                }
                else if (trigger->get_trigger_region_shape() == TRIGGER_REGION_SHAPE_SPHERE)
                {
                    was_inside = ix_point_in_sphere(trigger->last_pos, trigger->get_radius(), hp->last_pos);
                    is_inside = ix_point_in_sphere(trigger->pos, trigger->get_radius(), hp->pos);
                }
                if (Multiplayer)
                {
                    was_inside = false;
                }
                if (is_inside && !was_inside)
                {
                    object_enter_trigger_region_event enter;
                    enter.object_handle = hp->get_handle();
                    enter.trigger_handle = trigger->get_handle();
                    gem_post_event(enter);
                    if (trigger->get_trigger_region_type() == TRIGGER_REGION_TYPE_KILL_HUMAN && (!Multiplayer || multi_is_server()) && !hp->is_killed())
                    {
                        hp->hflags.killed_by_killzone = 1;
                        hp->hflags.invulnerable = 0;
                        const weapon_info* w_info = nullptr;
                        if (trigger->get_trigger_region_kill_human_type() == TRKHT_CLIFF)
                        {
                            if (Multiplayer)
                            {
                                w_info = multi_gw_get_custom_weapon_info(MCW_KILLZONE_CLIFF_KILL);
                            }
                        }
                        else if (trigger->get_trigger_region_kill_human_type() == TRKHT_MINE)
                        {
                            bool can_explode = !Multiplayer || multi_game_mode_is_gameplay();
                            if (Multiplayer)
                            {
                                w_info = multi_gw_get_custom_weapon_info(MCW_KILLZONE_MINE_KILL);
                            }
                            if (Mine_killzone_explosion != nullptr && can_explode)
                            {
                                explosion_create(Mine_killzone_explosion, nullptr, nullptr, hp->pos, IDENTITY_MATRIX, nullptr, w_info, false);
                            }
                        }
                        vehicle* vp = nullptr;
                        if (wrecking_crew_is_active() && wrecking_crew_get_active_mode() == WCM_WALKER && hp->vehicle_handle != 0xFFFFFFFF)
                        {
                            vp = World.object_from_handle<vehicle>(hp->vehicle_handle);
                        }
                        if (vp != nullptr)
                        {
                            vehicle_blow_up(vp, nullptr, false, nullptr, true);
                        }
                        else
                        {
                            // Magic number: -1 damage is lethal whatever the hit points.
                            human_damage(hp, nullptr, -1.0f, DBT_NONE, w_info, nullptr, true, false);
                        }
                    }
                }
                // Single player: a kill region kills the player every frame he stays in, unless never_die is set
                // (the prologue's Turn_Never_Die_Off script clears it around the cliffs for 3 s).
                if (!Multiplayer && trigger->get_trigger_region_type() == TRIGGER_REGION_TYPE_KILL_HUMAN && pp != nullptr && pp->get_handle() == hp->get_handle() && is_inside && pp->pflags.never_die == 0 &&
                    !pp->is_killed())
                {
                    human_damage(hp, nullptr, -1.0f, DBT_NONE, nullptr, nullptr, true, false);
                }
                if (was_inside && !is_inside)
                {
                    object_exit_trigger_region_event exit;
                    exit.object_handle = hp->get_handle();
                    exit.trigger_handle = trigger->get_handle();
                    gem_post_event(exit);
                }
            }
        }
        trigger->last_pos = trigger->pos;
    }
}
