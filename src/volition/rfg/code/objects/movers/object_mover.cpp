// Original: volition/rfg/code/objects/movers/object_mover.cpp
//
// object_mover, the base of the destructible and movable level objects (decompiled; header comment in object_mover.h).
// Compiled in rfg_objects_unity.cpp.
//
// mover_flags (object_mover::mover_flags), from the decompiled uses:
//   0x4 movable by humans (a dynamic prop), 0x8 force dynamic, 0x10 casts shadows, 0x20 dynamically created,
//   0x40 render changed, 0x80 render created, 0x100/0x200 fading in/out, 0x400 faded out, 0x800 marked for fade,
//   0x1000 fade done, 0x4000 casts drop shadows, 0x10000 control value set, 0x40000 moved, 0x80000 hidden,
//   0x200000 show on map, 0x400000/0x800000 damaged/destroyed script event pending, 0x1000000 anchors found,
//   0x2000000 ignore importance, 0x4000000 world anchor, 0x8000000 dynamic link, 0x10000000 restore when streamed
//   (outside the rail activity), 0x40000000 invulnerable.
// The chunk flags of the zone (Chunk_flags_table) are mapped onto these in serialize.
//
// Notes (decompiled behaviour, kept as is):
// - maybe_start_nano_dissolve checks gm for nullptr only for the first tests and dereferences it afterwards.
// - object_mover_mark_for_fade sets the timer of a nullptr mover.
// - object_movers_record_starting_points sorts by descending sum of distances with a comparator that never returns 1.
// - script_point_finish_death calls script_point_destroy even when the effect was not stopped.

#include "volition/rfg/code/objects/movers/object_mover.h"

#include "volition/rfg/code/code_effects/explosion.h"
#include "volition/rfg/code/code_effects/explosion_shared.h"
#include "volition/rfg/code/destruction/rfg_dynamics.h"
#include "volition/rfg/code/destruction/rfg_mover.h"
#include "volition/rfg/code/destruction/rfg_shapes.h"
#include "volition/rfg/code/effects/effects.h"
#include "volition/rfg/code/effects/object_effect.h"
#include "volition/rfg/code/event_logging/definitions_events/events.h"
#include "volition/rfg/code/event_logging/gamelog.h"
#include "volition/rfg/code/game_render/game_render_task.h"
#include "volition/rfg/code/gameplay/activities/activity.h"
#include "volition/rfg/code/gameplay/activities/activity_demolitions_master_internal.h"
#include "volition/rfg/code/gameplay/activities/activity_register_damage.h"
#include "volition/rfg/code/gameplay/gameflow/gfm_control_morale.h"
#include "volition/rfg/code/gameplay/mission_info.h"
#include "volition/rfg/code/gameplay/object_district.h"
#include "volition/rfg/code/gameplay/wrecking_crew/wrecking_crew.h"
#include "volition/rfg/code/gamewide/game_clock.h"
#include "volition/rfg/code/gamewide/game_render.h"
#include "volition/rfg/code/gamewide/gameseq.h"
#include "volition/rfg/code/gamewide/tweak_table.h"
#include "volition/rfg/code/havok/havok.h"
#include "volition/rfg/code/level/rfg_chunk_shared.h"
#include "volition/rfg/code/level/world.h"
#include "volition/rfg/code/level/world.inl"
#include "volition/rfg/code/multi/multi_core/include/multi.h"
#include "volition/rfg/code/multi/multi_core/include/multi_core_compression.h"
#include "volition/rfg/code/multi/multi_game/multi_game_object_backpack.h"
#include "volition/rfg/code/multi/multi_game/multi_game_object_movers.h"
#include "volition/rfg/code/multi/multi_game/multi_game_weapons.h"
#include "volition/rfg/code/objects/custom_object.h"
#include "volition/rfg/code/objects/gameplay_properties.h"
#include "volition/rfg/code/objects/humans/player.h"
#include "volition/rfg/code/objects/level_object_properties.h"
#include "volition/rfg/code/objects/movers/general_mover.h"
#include "volition/rfg/code/objects/movers/object_mover_lists.h"
#include "volition/rfg/code/objects/object_prop_names_shared.h"
#include "volition/rfg/code/pathfinding/nav_cell_detour.h"
#include "volition/rfg/code/rfg.h"
#include "volition/rfg/code/scripting/action_script/global_event_data.h"
#include "volition/rfg/code/scripting/action_script/global_event_manager.h"
#include "volition/rfg/code/scripting/public_address.h"
#include "volition/rfg/code/vehicle/vehicle_class.h"
#include "volition/rfg/code/weapons/weapons.h"
#include "volition/render_lib/code/rl_renderable_instance.h"
#include "volition/stream2/code/stream2.h"
#include "volition/vlib/code/cfile/cfile.h"
#include "volition/vlib/code/os/timer.h"
#include "volition/vlib/code/util/string_comp.h"
#include "volition/vlib/code/util/varray.h"

#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/agent/hkpprocesscollisioninput.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/dynamics/entity/hkprigidbody.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/dynamics/world/hkpworld.h"

#include "keen/base/trace.hpp"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>

static const int MAX_SCRIPT_POINT_INSTS = 4; // level_object_inst_properties::m_script_point_effect_insts
static const int MAX_MOVER_REFERENCE_POINTS = 512; // Mover_reference_points
static const int MAX_MOVER_REFERENCE_INDICES = 256; // reference indices are sent as bytes (multiplayer)
static const int MELEE_DAMAGE_DELAY_MS = 500; // a mover takes melee damage at most every half second after streaming
static const int NANO_DISSOLVE_FADE_MS = 1000;
static const int FADE_RANGE_DURATION_MS = 350; // fading in or out of view distance (0x15E)
static const int FADE_DELETE_DURATION_MS = 250; // fading out before deletion (0xFA)

// ---- globals
// Built by the variadic constructor (0x01075EA0, :84): chunk flag names of the zone files.
str_data_hash_table<unsigned int, 32, 32, unsigned char> Chunk_flags_table( // 0x03065F90
    "casts_shadows", 0x1u,
    "regenerate", 0x2u,
    "show_on_map", 0x4u,
    "force_dynamic", 0x8u,
    "disable_collapse_effect", 0x10u,
    "casts_drop_shadows", 0x20u,
    "regrow_on_stream", 0x40u,
    "inherit_damage_pct", 0x80u,
    "invulnerable", 0x100u,
    "plume_on_death", 0x200u,
    "one_of_many", 0x400u,
    "mining", 0x800u,
    "supply_crate", 0x1000u,
    "touch_terrain", 0x2000u,
    "kiosk", 0x4000u,
    "propaganda", 0x8000u,
    "no_cover", 0x10000u,
    "world_anchor", 0x20000u,
    "dynamic_link", 0x40000u,
    "building", 0x80000u,
    "child_gives_control", 0x100000u,
    nullptr, 0u);

// Built by the variadic constructor (0x01075F80, :94): mover type names, one bit per mover_flag_types value.
str_data_hash_table<unsigned int, 32, 32, unsigned char> Mover_type_table( // 0x0304B0F0
    "Communications", 1u << MOVER_TYPE_FLAG_COMMUNICATIONS,
    "Player_Base", 1u << MOVER_TYPE_FLAG_PLAYER_BASE,
    "House", 1u << MOVER_TYPE_FLAG_HOUSE,
    "Raid", 1u << MOVER_TYPE_RAID,
    "Bridge", 1u << MOVER_TYPE_BRIDGE,
    "Force_Field", 1u << MOVER_TYPE_FORCE_FIELD,
    "Dynamic", 1u << MOVER_TYPE_DYNAMIC,
    nullptr, 0u);

lf_node_queue<unsigned int> Mover_damaged_event_free_queue; // 0x0169CEE8
lf_node_queue<unsigned int> Mover_damaged_event_queue; // 0x0169CF38
lf_node_queue<unsigned int> Mover_destroyed_event_free_queue; // 0x0169CF88
lf_node_queue<unsigned int> Mover_destroyed_event_queue; // 0x0169CFD8
lf_node<unsigned int> Mover_damaged_event_nodes[64]; // 0x030266C0
lf_node<unsigned int> Mover_destroyed_event_nodes[128]; // 0x03029B60

// ---- file-static globals
static float RFG_object_mover_energy_scale = 1.0f; // 0x01694530
static float Bullet_movable_scalar = 0.3f; // 0x01694534, bullet impulse per point of damage
static farray<unsigned int, 7072> Find_anchors_delayed; // 0x01696058, movers still looking for their anchors
// farray<object*,16384> Object_update_render_list (0x0169D028) is used here too; placed in object.cpp.
// object_pool<rfg_mover,4000,1> rfg_moverpool (0x01F59580) is used here too; placed in rfg_mover.cpp.
// int Ladder_slide_land_cue (0x030269C0) is used here too; placed in human.cpp.
// player* Player (0x03023874) is used here too; declared in player.h, defined in player.cpp.
// float Character_collision_damage_speed_max (0x0302A160) is used here too; placed in human_collide.cpp (with its tweak item).
// gameplay_properties* Default_gameplay_properties (0x0302F040) is used here too; placed in gameplay_properties.cpp.
static bool Mover_ref_points_inited; // 0x0302F0E2
static int Num_mover_reference_points; // 0x0302F118
// game_render_task Pre_stream_render_task_gen_mover (0x030FCE48) is used here too; placed in general_mover.cpp.
static mover_reference_record Mover_reference_points[MAX_MOVER_REFERENCE_POINTS]; // 0x034FEC68
// tweak_table_item Tweak_table_item_Human_vehicle_collision_speed_low (0x03500C68) is used here too; placed in human.cpp.

// ---- header inlines (global_event.h, global_event_data.h)
// 0x005DBDA0 (global_event.h:115)
event_data::event_data(event_type in_type)
    : type(in_type)
{
}

// 0x007539B0 (global_event.h:111)
event_type event_data::get_type() const
{
    return type;
}

// 0x00A05840 (global_event_data.h:34)
building_damaged_event::building_damaged_event()
    : event_data(EVENT_BUILDING_DAMAGED)
{
}

// 0x00710EA0 (global_event_data.h:39)
building_damaged_by_explosion_event::building_damaged_by_explosion_event()
    : event_data(EVENT_BUILDING_DAMAGED_BY_EXPLOSION)
{
}

// 0x00A05850 (global_event_data.h:45)
building_destroyed_event::building_destroyed_event()
    : event_data(EVENT_BUILDING_DESTROYED)
{
}

// 0x00710EB0 (global_event_data.h:168)
mover_stream_in_event::mover_stream_in_event()
    : event_data(EVENT_MOVER_STREAM_IN)
{
}

// ---- object_mover (header inlines)
// 0x007166B0
object_mover::object_mover()
{
}

// 0x00716700
object_mover::~object_mover()
{
}

// 0x005DBE60 (.h:122)
const object_type object_mover::get_type_static()
{
    return OT_MOVER;
}

// 0x005DBE70 (.h:122)
const object_sub_type object_mover::get_subtype_static()
{
    return OT_SUB_UNDEFINED;
}

// 0x005DBE80 (.h:122)
int object_mover::get_bpo_index_static()
{
    return 0;
}

// 0x007166E0 (.h:122)
const char* object_mover::get_classname() const
{
    return "object_mover";
}

// 0x007166F0 (.h:122)
void object_mover::pool_free()
{
}

// 0x005DBE90 (.h:214)
int object_mover::mover_is_type(mover_flag_types mover_type) const
{
    return mover_type_bit_flags & (1 << mover_type);
}

// 0x007542A0 (.h:220)
bool object_mover::show_on_map() const
{
    return (mover_flags & 0x200000) != 0;
}

// 0x00A05DC0 (.h:229)
bool object_mover::was_dynamically_created() const
{
    return (mover_flags & 0x20) != 0;
}

// 0x00A05DD0 (.h:237)
unsigned short object_mover::get_render_opacity() const
{
    return m_render_opacity;
}

// 0x00710EF0 (.h:242)
void object_mover::set_render_opacity(unsigned short render_opacity)
{
    m_render_opacity = render_opacity;
}

// 0x00710F00 (.h:252)
void object_mover::set_render_created()
{
    mover_flags |= 0x80;
}

// 0x00710F10 (.h:253)
void object_mover::set_render_changed()
{
    mover_flags |= 0x40;
}

// ---- mover_reference_record
// 0x00A135E0
mover_reference_record::mover_reference_record()
{
}

// ---- obj_mover_props
// 0x0071ADA0 (.h:320)
// The flags property carries the shadow casting (mover_flags 0x10 and 0x4000).
obj_mover_props::obj_mover_props(const vector& pos, const matrix& orient, const char* chunk_name, const char* base_prop_name, bool cast_shadows, bool cast_drop_shadows, const char* name, int mover_type_bit_flags, object_importance_level importance_level, human_teams team, int civilian_count)
    : obj_prop_block(pos, orient, name)
{
    rfg_prop_block::set(OM_PROP_CHUNK_NAME, chunk_name);
    if (v_stricmp(base_prop_name, "Default") != 0)
    {
        rfg_prop_block::set(OM_PROP_BASE_PROPS, base_prop_name);
    }
    set(OM_PROP_TYPE_FLAGS_INTERNAL, mover_type_bit_flags);
    set(OM_PROP_CIV_COUNT, civilian_count);
    set(OM_PROP_IMPORTANCE_INTERNAL, importance_level);
    set(OM_PROP_TEAM_INTERNAL, team);
    unsigned int flags = 0;
    if (cast_shadows)
    {
        flags = 0x10;
    }
    if (cast_drop_shadows)
    {
        flags |= 0x4000;
    }
    set(OM_PROP_FLAGS, flags);
}

// 0x00710F20
obj_mover_props::~obj_mover_props()
{
}

// ---- internal functions
// 0x00A8BC40 (:184)
// LTCG: omp in EDI. Stops the running script point effects and frees their instances.
static void free_script_point_insts(object_mover* omp)
{
    for (int i = 0; i < MAX_SCRIPT_POINT_INSTS; i++)
    {
        script_point_effect_inst*& speip = omp->m_level_obj_properties->m_script_point_effect_insts[i];
        if (speip == nullptr)
        {
            continue;
        }
        if (speip->current_effect != 0xFFFFFFFF)
        {
            object* effect = World.object_from_handle(speip->current_effect);
            if (effect != nullptr && effect->get_obj_type() == OT_EFFECT)
            {
                ((object_effect*)effect)->stop(object_effect::SS_STOP_EMITTING);
            }
        }
        lopm_free_script_point_effect_inst(speip); // inlined
        speip = nullptr;
    }
}

// 0x00AA2470 (:205)
// LTCG: gm in ESI. Nano weapons dissolve props: they fade out within a second (unless invulnerable or exploding on
// death while intact) and count as damaged and destroyed.
static void maybe_start_nano_dissolve(general_mover* gm)
{
    if (gm != nullptr)
    {
        if (gm->gameplay_props != nullptr && (gm->gameplay_props->chunk_flags & 0x100) != 0) // invulnerable
        {
            return;
        }
        if (gm->fade_out_timer.valid())
        {
            return;
        }
    }
    // see the notes
    if ((gm->mover_flags & 0x4) == 0 && !gm->is_killed() && gm->m_level_obj_properties->m_flags.m_death_explosion)
    {
        return;
    }
    if (!gm->fade_out_timer.valid())
    {
        gm->fade_out_timer.set(NANO_DISSOLVE_FADE_MS);
        gm->mover_flags |= 0x800;
    }
    gm->setup_nano_dissolve();
    object_mover_maybe_add_to_damaged_event_queue(gm);
    object_mover_maybe_add_to_destroyed_event_queue(gm);
}

// 0x00A13600 (:233)
// LTCG: omp in ESI.
static void object_mover_maybe_post_damaged_event(object_mover* omp)
{
    if ((omp->mover_flags & 0x400000) == 0)
    {
        return;
    }
    building_damaged_event event;
    event.building = omp->get_handle();
    event.percent = omp->percent_destroyed();
    gem_post_event(event);
    omp->mover_flags &= ~0x400000u;
}

// 0x00A13650 (:249)
// LTCG: omp in ESI.
static void object_mover_maybe_post_destroyed_event(object_mover* omp)
{
    if ((omp->mover_flags & 0x800000) == 0)
    {
        return;
    }
    building_destroyed_event event;
    event.building = omp->get_handle();
    gem_post_event(event);
    omp->mover_flags &= ~0x800000u;
}

// 0x00A5F0D0 (:264)
// Posts the queued script events of the movers that still exist and returns the nodes to their free queues.
static void object_mover_post_damaged_destroyed_events()
{
    for (unsigned int* handlep = Mover_damaged_event_queue.pop(); handlep != nullptr; handlep = Mover_damaged_event_queue.pop())
    {
        if (*handlep != 0xFFFFFFFF)
        {
            object* objp = World.object_from_handle(*handlep);
            if (objp != nullptr && objp->get_obj_type() == OT_MOVER)
            {
                object_mover_maybe_post_damaged_event((object_mover*)objp); // inlined
            }
        }
        Mover_damaged_event_free_queue.push(handlep);
    }
    for (unsigned int* handlep = Mover_destroyed_event_queue.pop(); handlep != nullptr; handlep = Mover_destroyed_event_queue.pop())
    {
        if (*handlep != 0xFFFFFFFF)
        {
            object* objp = World.object_from_handle(*handlep);
            if (objp != nullptr && objp->get_obj_type() == OT_MOVER)
            {
                object_mover_maybe_post_destroyed_event((object_mover*)objp); // inlined
            }
        }
        Mover_destroyed_event_free_queue.push(handlep);
    }
}

// 0x00A382F0 (:315)
// LTCG: objp in ECX. The human behind an attack: the attacker itself or a vehicle's most recent driver.
static unsigned int resolve_attacker_object_mover(const object* objp)
{
    if (objp != nullptr)
    {
        if (objp->get_obj_type() == OT_HUMAN)
        {
            return objp->get_handle();
        }
        if (objp->get_obj_type() == OT_VEHICLE)
        {
            return ((const vehicle*)objp)->most_recent_driver;
        }
    }
    return 0xFFFFFFFF;
}

// ---- object_mover
// 0x00AA2500 (:344)
void object_mover::update_pos_and_orient(const vector& update_pos, const matrix& update_orient, bool set_havok_data)
{
    object::update_pos_and_orient(update_pos, update_orient, set_havok_data);
    mover_flags |= 0x40000;
}

// 0x00A13690 (:354)
// A mover that left its zone is deleted (single player only).
void object_mover::check_for_movement_delete()
{
    if (!Multiplayer && !World.object_physically_in_original_zone(this))
    {
        World.destroy_object(this, false);
    }
}

// 0x00A5F240 (:367)
// Script points of the chunk, by name.
int object_mover::get_tag_index_from_name(const char* tag_name) const
{
    if (tag_name == nullptr)
    {
        return -1;
    }
    const c_chunk_base* base = chunk_get_base_by_uid(chunk_uid);
    if (base == nullptr)
    {
        return -1;
    }
    const c_script_point* points = base->script_points.to_pointer();
    for (int i = 0; i < base->num_script_points; i++)
    {
        if (v_stricmp(points[i].name, tag_name) == 0)
        {
            return i;
        }
    }
    return -1;
}

// 0x00A136C0 (:393)
// The local tag transform moved by the given (or the mover's) placement.
bool object_mover::get_tag_world_coords(int tag_index, vector& tag_pos, matrix* tag_orient, vector* obj_pos, matrix* obj_orient) const
{
    if (obj_pos == nullptr)
    {
        obj_pos = (vector*)&pos;
    }
    if (obj_orient == nullptr)
    {
        obj_orient = (matrix*)&orient;
    }
    const bool found = get_tag_local_coords(tag_index, tag_pos, tag_orient);
    if (tag_orient != nullptr)
    {
        *tag_orient = *tag_orient * *obj_orient;
    }
    tag_pos.unrotate(*obj_orient);
    tag_pos += *obj_pos;
    return found;
}

// 0x00A5F2C0 (:421)
// Drops the anchors and the dynamic link; the mover looks for new anchors from object_mover_do_frame_post_sim.
void object_mover::release_anchor_points()
{
    mover_flags &= 0xF2FFFFFF; // anchors found, world anchor and dynamic link
    destruction_checksum = 0xFFFFFFFF;
    rfg_mover_delete_dynamic_link(get_handle());
    Find_anchors_delayed.add(get_handle());
}

// 0x00A137A0 (:437)
float object_mover::get_energy_scale()
{
    return RFG_object_mover_energy_scale;
}

// 0x00ADFF30 (:451)
// Load: mover types (registered in Object_mover_list_set), gameplay and level object properties, chunk flags, team
// and control, and the chunk container to stream (the chunk name plus ".rfgchunkx"); fails when the level object
// properties or the container are missing. During a destruction reset (checking_reset) only the flags are re-read.
bool object_mover::serialize(obj_prop_block& pblock, bool load)
{
    if (!object::serialize(pblock, load))
    {
        return false;
    }
    if (!load)
    {
        pblock.set(OM_PROP_CHUNK_OBJ_UID, chunk_obj_uid);
        pblock.set(OM_PROP_SHAPE_UID, shape_uid);
        pblock.rfg_prop_block::set(OM_PROP_CHUNK_NAME, stream2_get_container_name(get_srid()));
        if (destruction_checksum != 0xFFFFFFFF)
        {
            pblock.set(OM_PROP_DEST_CHECKSUM, destruction_checksum);
        }
        pblock.set(OM_PROP_FLAGS, mover_flags);
        if (v_stricmp(m_level_obj_properties->m_base_properties->m_name, "Default") != 0)
        {
            pblock.rfg_prop_block::set(OM_PROP_BASE_PROPS, m_level_obj_properties->m_base_properties->m_name);
        }
        if (v_stricmp(gameplay_props->name, "Default") != 0)
        {
            pblock.rfg_prop_block::set(OM_PROP_GAMEPLAY_PROPS, gameplay_props->name);
        }
        if (team != HUMAN_TEAM_NONE)
        {
            pblock.set(OM_PROP_TEAM_INTERNAL, team);
        }
        if (control > 0.0f)
        {
            pblock.set(OM_PROP_CONTROL, control);
        }
        return true;
    }

    obj_prop* prop;
    mover_type_bit_flags = 0;
    if (pblock.get_internal(prop, OM_PROP_TYPE_FLAGS_INTERNAL, 5) && prop->size == sizeof(int))
    {
        mover_type_bit_flags = *(int*)prop->data;
    }
    else
    {
        pblock.get_str_as_flags_optional(OM_PROP_TYPE_FLAGS, mover_type_bit_flags, Mover_type_table);
    }
    if (checking_reset < 1 && mover_type_bit_flags != 0)
    {
        for (int i = 0; i < NUM_MOVER_FLAG_TYPES; i++)
        {
            if ((mover_type_bit_flags & (1 << i)) != 0)
            {
                oml_add_new_object(get_handle(), (mover_flag_types)i); // inlined
            }
        }
    }
    fade_out_timer.invalidate();
    m_render_opacity = (unsigned short)Lparam_super_overbrighten;
    fade_start_time = -1;
    melee_damage_delay.invalidate();
    inactive_fade_time.invalidate();
    creator_handle = 0xFFFFFFFF;
    destruction_checksum = 0xFFFFFFFF;
    if (pblock.get_internal(prop, OM_PROP_DEST_CHECKSUM, 5) && prop->size == sizeof(unsigned int))
    {
        destruction_checksum = *(unsigned int*)prop->data;
    }

    char name[256];
    int chunk_flags = 0;
    if (pblock.get_optional(OM_PROP_GAMEPLAY_PROPS, &name[0]))
    {
        gameplay_props = gpm_get_properties(name);
    }
    else
    {
        gameplay_props = Default_gameplay_properties;
    }
    team = gameplay_props->team;
    control = gameplay_props->control;
    final_attacker = 0xFFFFFFFF;
    mover_flags = 0;
    if (pblock.get_internal(prop, OM_PROP_FLAGS, 5) && prop->size == sizeof(unsigned int))
    {
        mover_flags = *(unsigned int*)prop->data;
    }
    else
    {
        pblock.get_str_as_flags_optional(OM_PROP_CHUNK_FLAGS, chunk_flags, Chunk_flags_table);
    }
    chunk_flags |= gameplay_props->chunk_flags;
    mover_flags &= ~0x2000000u; // ignore importance
    if ((chunk_flags & 0x8) != 0) // force_dynamic
    {
        mover_flags |= 0x8;
    }
    if ((chunk_flags & 0x1) != 0) // casts_shadows
    {
        mover_flags |= 0x10;
    }
    if ((chunk_flags & 0x20) != 0) // casts_drop_shadows
    {
        mover_flags |= 0x4000;
    }
    if ((chunk_flags & 0x4) != 0) // show_on_map
    {
        mover_flags |= 0x200000;
    }
    if ((chunk_flags & 0x20000) != 0) // world_anchor
    {
        mover_flags |= 0x4000000;
    }
    if ((chunk_flags & 0x40000) != 0) // dynamic_link
    {
        mover_flags |= 0x8000000;
    }
    if ((chunk_flags & 0x100) != 0) // invulnerable
    {
        mover_flags |= 0x40000000;
    }
    if (pblock.get_internal(prop, OM_PROP_DYNAMIC_OBJECT, 5) && prop->size == sizeof(bool))
    {
        mover_flags |= 0x20;
    }
    if (checking_reset < 1 && kiosk_mover_is_kiosk(this))
    {
        kiosk_add_kiosk_object(get_handle());
    }
    chunk_uid = 0xFFFFFFFF;
    if (pblock.get_internal(prop, OM_PROP_CHUNK_UID, 5) && prop->size == sizeof(unsigned int))
    {
        chunk_uid = *(unsigned int*)prop->data;
    }
    reference_position_index = 0xFFFF;
    const level_object_base_properties* base_props = lopm_get_properties(pblock.get_str_ref_optional(OM_PROP_BASE_PROPS));
    if (base_props == nullptr)
    {
        return false;
    }
    if (checking_reset > 0)
    {
        return true;
    }
    m_level_obj_properties = lopm_create_instance(base_props, &pos);
    if (m_level_obj_properties == nullptr)
    {
        return false;
    }
    if ((mover_flags & 0x4) != 0)
    {
        m_level_obj_properties->m_flags.m_movable_by_humans = true;
    }
    custom_object_set_callbacks(this);
    pblock.get(OM_PROP_CHUNK_NAME, name);
    char container_name[256];
    cf_add_ext(container_name, sizeof(container_name), name, ".rfgchunkx", false);
    m_mp_error_info = nullptr;
    const unsigned int container = stream2_container_find(container_name);
    if (container == 0)
    {
        return false;
    }
    chunk_obj_uid = 0xFFFFFFFF;
    pblock.get_optional(OM_PROP_CHUNK_OBJ_UID, chunk_obj_uid);
    shape_uid = 0xFFFFFFFF;
    if (!pblock.get_optional(OM_PROP_SHAPE_UID, shape_uid))
    {
        shape_uid = get_handle();
    }
    request_streaming_resource(container, STREAM_PRIORITY_AUDIO_CACHE_LOAD);
    nav_cell_detour_request_handle = 0xFFFFFFFF;
    // A child mover takes its parent's team and has no control value, unless its chunk says child_gives_control.
    object_mover* parent_mover = World.object_from_handle<object_mover>(get_parent());
    const bool own_control = parent_mover == nullptr || (chunk_flags & 0x100000) != 0;
    if (own_control)
    {
        if (!pblock.get_optional(OM_PROP_TEAM_INTERNAL, team))
        {
            char team_name[256];
            if (pblock.get_optional(OM_PROP_TEAM, &team_name[0]))
            {
                team = human_team_from_name(team_name);
            }
        }
    }
    else
    {
        team = parent_mover->team;
    }
    if ((mover_flags & 0x4) == 0)
    {
        if (!own_control)
        {
            control = 0.0f;
        }
        else if ((mover_flags & 0x10000) == 0)
        {
            pblock.get_optional(OM_PROP_CONTROL, control);
            set_control_value();
            mover_flags |= 0x10000;
        }
        else
        {
            pblock.get_optional(OM_PROP_CONTROL, control);
        }
        check_liberation();
        adjust_control_value();
    }
    return true;
}

// 0x00A137B0 (:720)
bool object_mover::serialize_restore_if_missing_in_save_file(obj_prop_block& pblock)
{
    return false;
}

// 0x00A137C0 (:725)
// Re-reads the flags (checking_reset); invulnerable movers are not restored.
bool object_mover::serialize_restore_if_resetting_destruction(obj_prop_block& pblock)
{
    checking_reset++;
    serialize(pblock, true);
    const unsigned int flags = mover_flags;
    checking_reset--;
    return (flags & 0x40000000) == 0;
}

// 0x00A137F0 (:740)
void object_mover::set_ignore_importance(bool should_ignore)
{
    if (should_ignore)
    {
        mover_flags |= 0x2000000;
        return;
    }
    mover_flags &= ~0x2000000u;
}

// 0x00A13820 (:750)
bool object_mover::ignore_importance_level() const
{
    return (mover_flags & 0x2000000) != 0;
}

// 0x00A13830 (:756)
object_importance_level object_mover::get_importance_level() const
{
    if (!ignore_importance_level() && team != HUMAN_TEAM_NONE && gameplay_props != nullptr)
    {
        return gameplay_props->importance;
    }
    return OBJECT_IMPORTANCE_LEVEL_NONE;
}

// 0x00A8BCB0 (:781)
// Load: finds the chunk, compares its destruction checksum (a changed chunk drops the anchors), restores movers
// flagged for it. Unload: drops the nav cell detour, the script point and custom effects.
bool object_mover::stream(bool load)
{
    if (!load)
    {
        if (obj_flags.destroy_on_stream)
        {
            World.destroy_object(this, false);
        }
        chunk_uid = 0xFFFFFFFF;
        nav_cell_detour_remove_request(nav_cell_detour_request_handle);
        nav_cell_detour_request_handle = 0xFFFFFFFF;
        free_script_point_insts(this);
        custom_object_stop_custom_effects(this);
        return true;
    }
    c_chunk_base* chunk_base;
    if (chunk_uid == 0xFFFFFFFF)
    {
        chunk_base = chunk_get_base_by_name(stream2_get_container_name(get_srid()));
        chunk_uid = chunk_get_base_uid(chunk_base);
    }
    else
    {
        chunk_base = chunk_get_base_by_uid(chunk_uid);
    }
    if (chunk_base == nullptr)
    {
        return false;
    }
    const unsigned int flags = mover_flags;
    if ((flags & 0xC) != 0) // movable or force dynamic: always current
    {
        destruction_checksum = chunk_base->destruction_checksum;
    }
    if (destruction_checksum == chunk_base->destruction_checksum)
    {
        mover_flags = flags | 0x1000000;
        if ((flags & 0xC000000) == 0)
        {
            mover_flags = flags | 0x1000008;
        }
    }
    else
    {
        release_anchor_points();
    }
    melee_damage_delay.set(MELEE_DAMAGE_DELAY_MS);
    if ((mover_flags & 0x10000000) != 0 && !activity_is_active(ACTIVITY_MODE_VEHICLE_RAIL))
    {
        mover_flags &= ~0x10000000u;
        World.restore_object(get_handle());
    }
    set_render_flags(8);
    return true;
}

// 0x00A13860 (:876)
// EDF movers of a liberated district lose their team (until the final mission is complete).
void object_mover::check_liberation()
{
    const district* dp = district_get_from_pos(pos);
    if (dp->is_liberated() && team == HUMAN_TEAM_EDF && !mission_final_mission_complete())
    {
        team = HUMAN_TEAM_NONE;
        control = 0.0f;
    }
}

// 0x00A38310 (:886)
// Switches a script point effect to its prefix effect; it dies when the prefix delay runs out (script_point_do_frame).
void object_mover::script_point_kill(int idx, bool delayed)
{
    if ((unsigned int)idx >= MAX_SCRIPT_POINT_INSTS)
    {
        return;
    }
    script_point_effect_inst* speip = m_level_obj_properties->m_script_point_effect_insts[idx];
    if (speip == nullptr)
    {
        return;
    }
    const script_point_effect_info* info = m_level_obj_properties->m_base_properties->m_script_point_effect_infos[idx];
    if (info == nullptr || speip->state == SPES_DEAD || speip->state == SPES_PRE_DEATH)
    {
        return;
    }
    object_effect* effect = World.object_from_handle<object_effect>(speip->current_effect);
    if (effect != nullptr)
    {
        effect->stop(object_effect::SS_STOP_EMITTING);
        speip->current_effect = 0xFFFFFFFF;
    }
    if (info->prefix_effect != 0xFFFFFFFF)
    {
        const bool looping = (info->spe_flags & 0x4) != 0; // "Prefix Effect No Loop" (the flag is passed as is)
        speip->current_effect = effects_play(info->prefix_effect, get_handle(), speip->script_point_idx, looping, 0xFFFFFFFF, 1.0f, false);
    }
    const int delay_min = (int)(info->prefix_effect_delay_min * 1000.0f);
    if (info->prefix_effect_delay_min < info->prefix_effect_delay_max)
    {
        speip->death_timer.set_rand(delay_min, (int)(info->prefix_effect_delay_max * 1000.0f));
    }
    else
    {
        speip->death_timer.set(delay_min);
    }
    speip->hitpoints = info->prefix_hitpoints;
    speip->state = SPES_PRE_DEATH;
}

// 0x00A38430 (:925)
// Plays the death effect, stops the current one and lets the subclass remove the piece (script_point_destroy).
void object_mover::script_point_finish_death(int idx)
{
    if ((unsigned int)idx >= MAX_SCRIPT_POINT_INSTS)
    {
        return;
    }
    script_point_effect_inst* speip = m_level_obj_properties->m_script_point_effect_insts[idx];
    if (speip == nullptr)
    {
        return;
    }
    const script_point_effect_info* info = m_level_obj_properties->m_base_properties->m_script_point_effect_infos[idx];
    if (info == nullptr)
    {
        return;
    }
    if (info->death_effect != 0xFFFFFFFF)
    {
        effects_play(info->death_effect, get_handle(), speip->script_point_idx, false, 0xFFFFFFFF, 1.0f, false);
    }
    if (speip->current_effect != 0xFFFFFFFF)
    {
        object* effect = World.object_from_handle(speip->current_effect);
        if (effect != nullptr && effect->get_obj_type() == OT_EFFECT)
        {
            ((object_effect*)effect)->stop(object_effect::SS_STOP_EMITTING);
            speip->current_effect = 0xFFFFFFFF;
        }
    }
    script_point_destroy(idx); // see the notes
}

// 0x00A384D0 (:948)
// Per frame (lopm_script_point_do_frame): a script point whose piece fell off an rfg_mover dies; a finished prefix
// delay explodes (death explosion) or finishes the death; "Kill on Move" points die when the body moves; "Night Only"
// effects run at night.
void object_mover::script_point_do_frame(int idx)
{
    if (m_level_obj_properties == nullptr)
    {
        char message[48];
        sprintf(message, "Object has no level object properties: 0x%x", get_handle());
        keen::debug::nativePrintArgs(message);
        World.destroy_object(this, false);
        return;
    }
    script_point_effect_inst* speip = m_level_obj_properties->m_script_point_effect_insts[idx];
    if (speip == nullptr)
    {
        return;
    }
    const script_point_effect_info* info = m_level_obj_properties->m_base_properties->m_script_point_effect_infos[idx];
    if (get_obj_type() == OT_MOVER && get_sub_type() == OT_SUB_MOVER_RFG)
    {
        const rfg_destroyable_shape* shape = ((rfg_mover*)this)->get_shape();
        if (shape != nullptr && shape->get_subpiece(speip->shape_key)->get_owner() == nullptr)
        {
            script_point_kill(idx, false);
        }
    }
    if (speip->death_timer.elapsed())
    {
        speip->death_timer.invalidate();
        if (info->death_explosion != nullptr)
        {
            vector exp_pos;
            matrix exp_orient;
            get_tag_world_coords(speip->script_point_idx, exp_pos, &exp_orient, nullptr, nullptr);
            object* owner = nullptr;
            if (final_attacker != 0xFFFFFFFF)
            {
                owner = World.object_from_handle(final_attacker);
                if (owner != nullptr && owner->get_obj_type() != OT_HUMAN)
                {
                    owner = nullptr;
                }
            }
            const weapon_info* w_info = nullptr;
            if (Multiplayer)
            {
                w_info = multi_gw_get_custom_weapon_info(MCW_EXPLOSIVE_OBJECT);
            }
            speip->state = SPES_DEAD;
            explosion_create(info->death_explosion, this, owner, exp_pos, exp_orient, &exp_orient.uvec, w_info, false);
            return;
        }
        script_point_finish_death(idx);
        return;
    }
    if ((info->spe_flags & 0x100) != 0 && havok_handle != 0xFFFFFFFF) // Kill on Move
    {
        vector vel;
        havok_body_get_linear_velocity(havok_handle, vel);
        if (!vel.is_null())
        {
            script_point_kill(idx, false);
            return;
        }
    }
    if ((info->spe_flags & 0x80) != 0 && speip->state == SPES_NORMAL) // Night Only
    {
        object_effect* effect = World.object_from_handle<object_effect>(speip->current_effect);
        const game_clock_time_of_day time_of_day = game_clock_get_time_of_day();
        if (effect == nullptr)
        {
            if (time_of_day == GC_NIGHT)
            {
                speip->current_effect = effects_play(info->custom_effect, get_handle(), speip->script_point_idx, true, 0xFFFFFFFF, 1.0f, false);
                effect = World.object_from_handle<object_effect>(speip->current_effect);
                if (effect != nullptr)
                {
                    effect->obj_flags.destroy_on_stream = false;
                }
            }
        }
        else if (time_of_day == GC_DAY)
        {
            effect->stop(object_effect::SS_STOP_EMITTING);
        }
    }
}

// 0x00A5F330 (:1032)
// Damages the script points in the explosion's reach (each one gets the damage at its own tag position). True when
// any of them was in reach.
bool object_mover::script_points_process_explosion_hit(const object* source, const explosion_info* e_info, const vector& posp, const vector* dir)
{
    bool hit = false;
    for (int i = 0; i < MAX_SCRIPT_POINT_INSTS; i++)
    {
        const script_point_effect_inst* tag_inst = m_level_obj_properties->m_script_point_effect_insts[i];
        if (tag_inst == nullptr || tag_inst->state == SPES_DEAD)
        {
            continue;
        }
        vector tag_pos;
        get_tag_world_coords(tag_inst->script_point_idx, tag_pos, nullptr, nullptr, nullptr);
        unsigned int damage = 0;
        vector linear_impulse;
        vector angular_impulse;
        explosion_get_damage_and_impulse(damage, linear_impulse, angular_impulse, e_info, this, posp, dir, &tag_pos);
        if (damage == 0)
        {
            continue;
        }
        // The point at i is in reach; the damage goes to the first script point of its piece.
        const unsigned int shape_key = tag_inst->shape_key;
        const float fdamage = (float)damage;
        if ((!Multiplayer || multi_is_server()) && (get_sub_type() != OT_SUB_MOVER_GENERAL || (mover_flags & 0x4) != 0) &&
            (get_sub_type() != OT_SUB_MOVER_RFG || shape_key != 0xFFFFFFFF))
        {
            const c_chunk_base* chunk = chunk_get_base_by_uid(chunk_uid);
            if (chunk != nullptr && chunk_get_destroyable_object(chunk, chunk_obj_uid, nullptr) != nullptr)
            {
                for (int idx = 0; idx < MAX_SCRIPT_POINT_INSTS; idx++)
                {
                    script_point_effect_inst* speip = m_level_obj_properties->m_script_point_effect_insts[idx];
                    if (speip == nullptr || (get_sub_type() == OT_SUB_MOVER_RFG && shape_key != speip->shape_key))
                    {
                        continue;
                    }
                    if (speip->hitpoints > 0)
                    {
                        speip->hitpoints -= (int)fdamage;
                        if (speip->hitpoints < 1)
                        {
                            script_point_kill(idx, false);
                        }
                    }
                    break;
                }
                if (source != nullptr)
                {
                    final_attacker = resolve_attacker_object_mover(source); // inlined
                }
            }
        }
        hit = true;
    }
    return hit;
}

// 0x00A138B0 (:1060)
bool object_mover::has_script_point_effect(unsigned int shape_key, int* return_idx) const
{
    if (shape_key == 0xFFFFFFFF)
    {
        return false;
    }
    for (int i = 0; i < MAX_SCRIPT_POINT_INSTS; i++)
    {
        const script_point_effect_inst* speip = m_level_obj_properties->m_script_point_effect_insts[i];
        if (speip != nullptr && shape_key == speip->shape_key)
        {
            if (return_idx != nullptr)
            {
                *return_idx = i;
            }
            return true;
        }
    }
    return false;
}

// 0x00AA2530 (:1096)
// Frees the level object properties and the mover list entries, posts the pending script events.
void object_mover::free_this()
{
    wrecking_crew_mover_destroyed(this);
    lopm_free_instance(m_level_obj_properties);
    if (mover_type_bit_flags != 0)
    {
        for (int i = 0; i < NUM_MOVER_FLAG_TYPES; i++)
        {
            if ((mover_type_bit_flags & (1 << i)) != 0)
            {
                oml_remove_object(get_handle(), (mover_flag_types)i);
            }
        }
    }
    mover_type_bit_flags = 0;
    object_mover_maybe_post_damaged_event(this); // inlined
    object_mover_maybe_post_destroyed_event(this); // inlined
    object::free_this();
}

// 0x00A138F0 (:1133)
// The bodies the mover's body overlaps under the given collision type (static movers only).
void object_mover::create_anchors(unsigned int col_type, hkpCdBodyPairCollector& collector)
{
    if ((mover_flags & 0xC) != 0)
    {
        return;
    }
    hkpRigidBody* rb = havok_body_get_pointer(havok_handle);
    hkpCollidable* collidable = &rb->m_collidable;
    const unsigned int old_filter_info = collidable->m_broadPhaseHandle.m_collisionFilterInfo;
    collidable->m_broadPhaseHandle.m_collisionFilterInfo = col_type;
    Havok_world->getPenetrations(collidable, *Havok_world->m_collisionInput, collector); // the process input is a collision input
    collidable->m_broadPhaseHandle.m_collisionFilterInfo = old_filter_info;
}

// 0x00A13940 (:1167)
// Must be overridden.
rl_renderable_instance* object_mover::get_render_model_instance()
{
    keen::debug::nativePrintArgs("Int3 critical function at %s(%d)\n", __FILE__, __LINE__);
    return nullptr;
}

// 0x00AA2610 (:1174)
// Bullet damage, a nano dissolve for nano rifle hits on props, and a push along the hit normal for movable bodies.
void object_mover::process_bullet_hit(human* attacker, float damage, const weapon_fire_info& fire_info)
{
    if (get_sub_type() == OT_SUB_MOVER_GENERAL && fire_info.w_info->weapon_class == WEAPON_CLASS_NANO_RIFLE)
    {
        maybe_start_nano_dissolve((general_mover*)this);
    }
    apply_damage(damage, DBT_BULLET, fire_info.hit_pos, fire_info.hit_normal, attacker, fire_info.hit_shape_key, false);
    activity_register_damage(attacker, this, damage, -1);
    if (havok_handle != 0xFFFFFFFF && havok_body_is_movable(havok_handle))
    {
        const vector impulse = -fire_info.hit_normal * damage * Bullet_movable_scalar;
        const vector point = fire_info.hit_pos;
        havok_body_apply_point_impulse(havok_handle, impulse, point);
    }
}

// 0x00AB39F0 (:1302)
void object_mover_system_init()
{
    Pre_stream_render_task_gen_mover.m_callback = general_mover_pre_stream_process_all;
    game_render_pre_stream_render_task_register(&Pre_stream_render_task_gen_mover);
    rfg_mover_system_init();
    for (int i = 0; i < 64; i++)
    {
        Mover_damaged_event_free_queue.push_node(&Mover_damaged_event_nodes[i]);
    }
    for (int i = 0; i < 128; i++)
    {
        Mover_destroyed_event_free_queue.push_node(&Mover_destroyed_event_nodes[i]);
    }
}

// 0x00AA2770 (:1201)
// Explosion damage of a mover: posts the "damaged by explosion" script event, damages the script points; rfg_movers
// queue the explosion for the destruction code (rfg_dyn_add_pre_collide_explosion), anything else takes
// apply_damage. Bodies get the explosion impulse (scaled by mass) unless rfg_dyn_maybe_delete fades them out.
void object_mover_process_explosion_hit(object_mover* obj_mover, object* owner, float damage, const vector& linear_impulse, const vector& angular_impulse, const explosion_info* p_explosion_info, const vector& world_position, const vector* dir, const weapon_info* w_info)
{
    if (obj_mover == nullptr || p_explosion_info == nullptr)
    {
        return;
    }
    if (obj_mover->get_sub_type() == OT_SUB_MOVER_GENERAL &&
        ((w_info != nullptr && w_info->weapon_class == WEAPON_CLASS_NANO_RIFLE) || (p_explosion_info->flags & 0x20) != 0)) // nano dissolve
    {
        maybe_start_nano_dissolve((general_mover*)obj_mover);
    }
    activity_register_damage(owner, obj_mover, damage, -1);
    if ((obj_mover->mover_flags & 0x4) == 0)
    {
        building_damaged_by_explosion_event event;
        event.attacker = owner != nullptr ? owner->get_handle() : 0xFFFFFFFF;
        event.building = obj_mover->get_handle();
        event.info_handle = explosion_get_handle_from_ptr(p_explosion_info);
        gem_post_event(event);
    }
    float radius_mult = 1.0f;
    if (owner != nullptr && owner->get_obj_type() == OT_HUMAN && owner->get_sub_type() == OT_SUB_HUMAN_PLAYER && ((player*)owner)->backpack != nullptr)
    {
        radius_mult = ((player*)owner)->backpack->get_backpack_abilities()->area_damage_dealt_factor;
    }
    const bool script_point_hit = obj_mover->script_points_process_explosion_hit(owner, p_explosion_info, world_position, dir);
    if ((p_explosion_info->flags & 0x8000) == 0 && obj_mover->get_sub_type() == OT_SUB_MOVER_RFG) // not expanding
    {
        const vector world_dir = dir != nullptr ? *dir : ZERO_VECTOR;
        vector local_pos;
        vector local_dir;
        convert_point_direction_from_world_to_local(local_pos, local_dir, world_position, world_dir, obj_mover->pos, obj_mover->orient);
        if (rfg_dyn_add_pre_collide_explosion((rfg_mover*)obj_mover, owner, p_explosion_info, local_pos, local_dir, radius_mult, false))
        {
            return;
        }
    }
    if (!script_point_hit)
    {
        obj_mover->apply_damage(damage, DBT_EXPLOSION, world_position, ZERO_VECTOR, owner, 0xFFFFFFFF, false);
    }
    const float mass = obj_mover->get_mass();
    if (obj_mover->havok_handle == 0xFFFFFFFF || mass <= 0.0f)
    {
        return;
    }
    float impulse_scalar = 0.3f;
    if (obj_mover->get_sub_type() == OT_SUB_MOVER_GENERAL && (obj_mover->mover_flags & 0x4) != 0)
    {
        impulse_scalar = rfg_dyn_get_impulse_scalar(mass, obj_mover->get_physical_material(0xFFFFFFFF, nullptr, nullptr));
    }
    vector modified_linear_impulse;
    vector modified_angular_impulse;
    // 15 m/s and 15*pi rad/s caps
    explosion_modify_impulses(&modified_linear_impulse, &modified_angular_impulse, &linear_impulse, &angular_impulse, mass, impulse_scalar, impulse_scalar, 15.0f, 47.12389f);
    if (rfg_dyn_maybe_delete(obj_mover, mass, modified_linear_impulse, 10.0f))
    {
        rfg_mover* mover = nullptr;
        if (obj_mover->get_sub_type() != OT_SUB_MOVER_GENERAL)
        {
            mover = World.object_cast<rfg_mover>(obj_mover);
        }
        if (obj_mover->get_sub_type() == OT_SUB_MOVER_GENERAL || !Multiplayer)
        {
            object_mover_mark_for_fade(obj_mover, rand() % 250); // fade out within a quarter second
            return;
        }
        if (Multiplayer && multi_is_server() && !mover->obj_flags.destroyed)
        {
            rfg_dyn_add_pre_collide_mp_destroy(mover, owner, false);
        }
        return;
    }
    havok_body_apply_linear_impulse(obj_mover->havok_handle, modified_linear_impulse);
    havok_body_apply_angular_impulse(obj_mover->havok_handle, modified_angular_impulse);
}

// 0x00A13960 (:1317)
// qsort comparator: larger sums first (see the notes).
static int compare_ref_record(const void* arg1, const void* arg2)
{
    if (((const mover_reference_record*)arg2)->sum_dist_sqr < ((const mover_reference_record*)arg1)->sum_dist_sqr)
    {
        return -1;
    }
    return 0;
}

// 0x00A38740 (:1325)
// Multiplayer: the closest of the first 256 reference points (0xFFFF before they are recorded).
unsigned short object_mover_get_reference_pos_index(const vector& pos)
{
    if (!Multiplayer || !Mover_ref_points_inited)
    {
        return 0xFFFF;
    }
    int num = Num_mover_reference_points;
    if (num > MAX_MOVER_REFERENCE_INDICES - 1)
    {
        num = MAX_MOVER_REFERENCE_INDICES;
    }
    float best_dist = 3.4028235e+38f; // FLT_MAX
    unsigned short best = 0xFFFF;
    for (int i = 0; i < num; i++)
    {
        const float dist = vec_dist_squared(pos, Mover_reference_points[i].start_point);
        if (dist < best_dist)
        {
            best = (unsigned short)i;
            best_dist = dist;
        }
    }
    return best;
}

// 0x00A13990 (:1347)
void object_mover_get_reference_pos(vector& reference_pos, unsigned short index)
{
    reference_pos = Mover_reference_points[index].start_point;
}

// 0x00A5F4C0 (:1355)
// Multiplayer: records the start positions of up to 512 movers as reference points, sorted by their summed squared
// distance to all others (most spread out first), and gives each mover its closest reference point.
static void object_movers_record_starting_points()
{
    if (!Multiplayer)
    {
        return;
    }
    const int num_movers = World.get_num_objects<object_mover>();
    Num_mover_reference_points = 0;
    for (int i = 0; i < num_movers && Num_mover_reference_points < MAX_MOVER_REFERENCE_POINTS; i++)
    {
        mover_reference_record& record = Mover_reference_points[Num_mover_reference_points];
        record.start_point = World.get_object<object_mover>(i)->pos;
        record.sum_dist_sqr = 0.0f;
        Num_mover_reference_points++;
    }
    for (int i = 0; i < Num_mover_reference_points; i++)
    {
        for (int j = 0; j < Num_mover_reference_points; j++)
        {
            const float dist = vec_dist_squared(Mover_reference_points[i].start_point, Mover_reference_points[j].start_point);
            Mover_reference_points[i].sum_dist_sqr += dist;
            Mover_reference_points[j].sum_dist_sqr += dist;
        }
    }
    if (Num_mover_reference_points > MAX_MOVER_REFERENCE_INDICES)
    {
        qsort(Mover_reference_points, Num_mover_reference_points, sizeof(mover_reference_record), compare_ref_record);
    }
    Mover_ref_points_inited = true;
    for (int i = 0; i < World.get_num_objects<object_mover>(); i++)
    {
        object_mover* omp = World.get_object<object_mover>(i);
        omp->reference_position_index = object_mover_get_reference_pos_index(omp->pos);
    }
}

// 0x00A8BDE0 (:1400)
void object_mover_level_init_post()
{
    havok_add_entity_flush();
    rfg_mover_do_frame_pre_sim();
    object_mover_do_frame_post_sim();
    rfg_mover_level_init_post();
    multi_game_general_mover_reset_destroyed_count();
    if (Multiplayer)
    {
        create_level_general_mover_remote_objects();
    }
    object_movers_record_starting_points();
}

// 0x00A139B0 (:1418)
void object_mover_level_init_pre()
{
    Mover_ref_points_inited = false;
}

// 0x00A5F630 (:1424)
// General movers may be destroyed without network messages; pending script events are dropped.
void object_mover_level_shutdown()
{
    const int num = World.get_num_objects<general_mover>();
    for (int i = 0; i < num; i++)
    {
        general_mover* gm = World.get_object<general_mover>(i);
        gm->remote_obj_flags.client_destroy_permission = true;
        gm->remote_obj_flags.no_destroy_packet = true;
    }
    rfg_mover_level_shutdown();
    for (unsigned int* handlep = Mover_damaged_event_queue.pop(); handlep != nullptr; handlep = Mover_damaged_event_queue.pop())
    {
        Mover_damaged_event_free_queue.push(handlep);
    }
    for (unsigned int* handlep = Mover_destroyed_event_queue.pop(); handlep != nullptr; handlep = Mover_destroyed_event_queue.pop())
    {
        Mover_destroyed_event_free_queue.push(handlep);
    }
}

// 0x00A8BE10 (:1442)
void object_mover_do_frame_pre_sim()
{
    general_mover_do_frame_pre_sim();
    rfg_mover_do_frame_pre_sim();
    object_mover_post_damaged_destroyed_events();
    lopm_script_point_do_frame();
}

// 0x00A5F700 (:1455)
// Updates the render opacity of every mover and retries the anchors of the delayed movers: all of them while loading
// (or while Demolitions Master loads its layers), otherwise one per frame. A mover whose anchors are not found yet
// goes to the back of the list; one whose anchors are found leaves it.
void object_mover_do_frame_post_sim()
{
    World.sanity_check_object_lists();
    const int num_movers = World.get_num_objects<object_mover>();
    for (int i = 0; i < num_movers; i++)
    {
        World.get_object<object_mover>(i)->update_render_opacity();
    }
    int num = Find_anchors_delayed.size();
    if (gameseq_get_state() != GS_LOAD && Demolitions_master_activity.current_state != DM_STATE_LOADING_LAYERS && num > 0)
    {
        num = 1;
    }
    for (int i = 0; i < num; i++)
    {
        const unsigned int handle = Find_anchors_delayed[i];
        object* objp = handle != 0xFFFFFFFF ? World.object_from_handle(handle) : nullptr;
        if (objp != nullptr && objp->get_obj_type() == OT_MOVER)
        {
            object_mover* omp = (object_mover*)objp;
            if (!omp->find_anchor_points())
            {
                Find_anchors_delayed.add(omp->get_handle());
            }
            else if ((omp->mover_flags & 0x1000000) == 0)
            {
                continue;
            }
        }
        Find_anchors_delayed.remove(i);
        num--;
        i--;
    }
}

// 0x00A139C0 (:1496)
// Melee damage (at most every MELEE_DAMAGE_DELAY_MS after streaming) and a push for movable non-rfg bodies.
bool object_mover::process_melee_hit(object* attacker, float damage, const vector& impulse, const vector& hit_pos, const vector& hit_normal, unsigned int shape_key, const weapon_info& w_info)
{
    if (!melee_damage_delay.elapsed())
    {
        return true;
    }
    const bool result = apply_damage(damage, DBT_MELEE, hit_pos, hit_normal, attacker, shape_key, false);
    if (get_sub_type() != OT_SUB_MOVER_RFG && havok_handle != 0xFFFFFFFF && havok_body_is_movable(havok_handle))
    {
        havok_body_apply_point_impulse(havok_handle, impulse, hit_pos);
    }
    activity_register_damage(attacker, this, damage, -1);
    return result;
}

// 0x00A13A60 (:1541)
// A destroyed EDF mover keeps its control only by importance (low: none, medium/high: 1).
void object_mover::adjust_control_value()
{
    if (Multiplayer || team != HUMAN_TEAM_EDF || control <= 0.0f || !World.object_was_destroyed(get_handle()))
    {
        return;
    }
    switch (get_importance_level())
    {
    case OBJECT_IMPORTANCE_LEVEL_NONE:
        break;
    case OBJECT_IMPORTANCE_LEVEL_LOW:
        control = 0.0f;
        break;
    case OBJECT_IMPORTANCE_LEVEL_MEDIUM:
    case OBJECT_IMPORTANCE_LEVEL_HIGH:
        control = 1.0f;
        break;
    default:
        keen::debug::nativePrintArgs("Int3 critical function at %s(%d)\n", __FILE__, __LINE__);
        break;
    }
}

// 0x00A13AF0 (:1582)
// Maps the control level of the zone (0..3) to control points by importance: low 0/1/2/3 (other values 1), medium
// 0/10/12/15, high 80 (for 1 or negative values, others kept); no importance means no control.
void object_mover::set_control_value()
{
    const float level = control;
    object_importance_level importance = OBJECT_IMPORTANCE_LEVEL_NONE;
    if (!ignore_importance_level() && team != HUMAN_TEAM_NONE && gameplay_props != nullptr) // get_importance_level, inlined
    {
        importance = gameplay_props->importance;
        if (importance > OBJECT_IMPORTANCE_LEVEL_HIGH)
        {
            keen::debug::nativePrintArgs("Int3 critical function at %s(%d)\n", __FILE__, __LINE__);
            return;
        }
    }
    switch (importance)
    {
    case OBJECT_IMPORTANCE_LEVEL_LOW:
        if (level == 0.0f)
        {
            control = 0.0f;
        }
        else if (level == 2.0f)
        {
            control = 2.0f;
        }
        else if (level == 3.0f)
        {
            control = 3.0f;
        }
        else
        {
            control = 1.0f; // 0.5, 1.0 and anything else
        }
        return;
    case OBJECT_IMPORTANCE_LEVEL_MEDIUM:
        if (level == 0.0f)
        {
            control = 0.0f;
        }
        else if (level == 2.0f)
        {
            control = 12.0f;
        }
        else if (level == 3.0f)
        {
            control = 15.0f;
        }
        else
        {
            control = 10.0f;
        }
        return;
    case OBJECT_IMPORTANCE_LEVEL_HIGH:
        if (level >= 0.0f && level != 1.0f)
        {
            return;
        }
        control = 80.0f;
        return;
    default:
        control = 0.0f;
        return;
    }
}

// 0x00A13C60 (:1628)
// Collision damage: from vehicles as car damage, from anything else as level object damage.
void object_mover_process_collision(object_mover* target, object* obj_other, float damage, const vector& hit_pos, const vector& hit_normal, const vector& hit_vel, unsigned int hit_shape_key)
{
    const damaged_by_types from_type = obj_other->get_obj_type() == OT_VEHICLE ? DBT_CAR : DBT_LEVEL_OBJECT;
    target->apply_damage(damage, from_type, hit_pos, hit_normal, obj_other, hit_shape_key, false);
}

// 0x00A13CB0 (:1663)
void object_mover_apply_point_velocity(object_mover* obj_mover, const vector* velocity, const vector* point)
{
    if (obj_mover->havok_handle != 0xFFFFFFFF)
    {
        havok_body_apply_point_velocity(obj_mover->havok_handle, *velocity, *point);
    }
}

// 0x00A13CD0 (:1678)
void object_mover_apply_linear_velocity(object_mover* obj_mover, const vector* velocity)
{
    if (obj_mover->havok_handle != 0xFFFFFFFF)
    {
        havok_body_apply_linear_velocity(obj_mover->havok_handle, *velocity);
    }
}

// 0x00A5F850 (:1691)
// LTCG: base in EAX. A dynamic general mover of a chunk holding exactly one general object (no collision when the
// object's flag 0x20 is set).
static general_mover* gm_create_from_chunk(const vector& pos, const matrix& orient, const c_chunk_base* base, unsigned int parent_handle)
{
    if (base->num_general_objects != 1 || base->general_objects.to_pointer() == nullptr)
    {
        return nullptr;
    }
    const unsigned int collision_type = (base->general_objects.to_pointer()[0].flags & 0x20) != 0 ? COLLIDE_NO_COLLISION : COLLIDE_MOVER;
    gm_prop_block pblock(pos, orient, base->name, base->object_identifiers.to_pointer()[0].unique_id, "default", "no name", collision_type, true, true, 1 << MOVER_TYPE_DYNAMIC);
    pblock.set(OM_PROP_DYNAMIC_OBJECT, true);
    general_mover* gm = World.create_object<general_mover>(pblock, parent_handle); // inlined (general_mover::get_classname_hash)
    gm->obj_flags.no_save = true;
    return gm;
}

// 0x00A5F910 (:1715)
// LTCG: pos in ECX, orient in EDX. A dynamic rfg_mover of a destroyable chunk (not shown on the map).
static rfg_mover* rfgm_create_from_chunk(const vector& pos, const matrix& orient, const c_chunk_base* base, unsigned int parent_handle)
{
    obj_mover_props pblock(pos, orient, base->name, "default", true, false, nullptr, 1 << MOVER_TYPE_DYNAMIC, OBJECT_IMPORTANCE_LEVEL_NONE, HUMAN_TEAM_NONE, 0);
    pblock.set(RFGM_PROP_MOVE_TYPE, (int)0);
    pblock.set(OM_PROP_CHUNK_OBJ_UID, base->object_identifiers.to_pointer()[0].unique_id);
    pblock.set(RFGM_PROP_SHOW_ON_MAP, false);
    pblock.set(OM_PROP_DYNAMIC_OBJECT, true);
    rfg_mover* rm = World.create_object<rfg_mover>(pblock, parent_handle); // inlined
    rm->obj_flags.no_save = true;
    return rm;
}

// 0x00A8BE30 (:1740)
object_mover* object_mover_create_from_chunk(const vector& pos, const matrix& orient, const char* file_name, unsigned int parent_handle)
{
    return object_mover_create_from_chunk(pos, orient, chunk_get_base_by_name(file_name), parent_handle); // inlined
}

// 0x00A5FA00 (:1752)
// A general mover for a chunk with general objects, an rfg_mover for one with destroyables.
object_mover* object_mover_create_from_chunk(const vector& pos, const matrix& orient, const c_chunk_base* base, unsigned int parent_handle)
{
    if (base == nullptr)
    {
        return nullptr;
    }
    if (base->num_general_objects > 0)
    {
        return gm_create_from_chunk(pos, orient, base, parent_handle);
    }
    if (base->num_base_destroyables > 0)
    {
        return rfgm_create_from_chunk(pos, orient, base, parent_handle);
    }
    return nullptr;
}

// 0x00A13CF0 (:1810)
bool object_mover_is_visible(const object_mover* mover)
{
    return mover->get_render_opacity() != 0;
}

// 0x00A13D10 (:1816)
bool object_mover_is_fading_for_delete(const object_mover* mover)
{
    return mover != nullptr && mover->fade_out_timer.valid();
}

// 0x00A13D30 (:1826)
// Starts the delete fade after time_until_fade ms (once).
void object_mover_mark_for_fade(object_mover* mover, int time_until_fade)
{
    if (mover != nullptr && mover->fade_out_timer.valid())
    {
        return;
    }
    mover->fade_out_timer.set(time_until_fade); // see the notes
    mover->mover_flags |= 0x800;
}

// 0x00AA2AA0 (:1852)
// Fades by view distance, or out for deletion once the fade timer elapsed (then the mover is destroyed; in
// multiplayer only on the server or for movers that do not sync). Returns 0 when the mover is no longer visible.
int object_mover::handle_fading(float dist_squared, float max_dist_squared)
{
    const unsigned int flags = mover_flags;
    if ((flags & 0x1000) != 0)
    {
        return 0;
    }
    if ((flags & 0x800) == 0)
    {
        if (max_dist_squared < dist_squared)
        {
            return handle_fading_out(FADE_RANGE_DURATION_MS);
        }
        if ((flags & 0x700) == 0)
        {
            return 1;
        }
        return handle_fading_in(FADE_RANGE_DURATION_MS);
    }
    if (!fade_out_timer.elapsed() || handle_fading_out(FADE_DELETE_DURATION_MS) != 0)
    {
        return 1;
    }
    mover_flags |= 0x1000;
    if (Multiplayer)
    {
        if (get_obj_type() == OT_MOVER && get_sub_type() == OT_SUB_MOVER_RFG)
        {
            if (Multiplayer && multi_is_server())
            {
                rfg_dyn_add_pre_collide_mp_destroy((rfg_mover*)this, nullptr, false);
            }
            return 0;
        }
        if (mover_should_sync() && (!Multiplayer || !multi_is_server()))
        {
            return 0;
        }
    }
    World.destroy_object(this, false);
    return 0;
}

// 0x00A8BE90 (:1916)
// Ramps the opacity up to full over fade_duration ms; a running fade out is reversed from its current point.
int object_mover::handle_fading_in(int fade_duration)
{
    const unsigned int flags = mover_flags;
    if ((flags & 0x200) != 0)
    {
        mover_flags = (flags & ~0x200u) | 0x100;
        fade_start_time = timer_get(TIMER_MILLISECONDS) * 2 - fade_duration - fade_start_time;
        return m_render_opacity != 0;
    }
    if (fade_start_time == -1)
    {
        mover_flags = (flags & ~0x400u) | 0x100;
        fade_start_time = timer_get(TIMER_MILLISECONDS);
        return m_render_opacity != 0;
    }
    int elapsed = timer_get(TIMER_MILLISECONDS) - fade_start_time;
    if (elapsed < fade_duration)
    {
        if (elapsed < 0)
        {
            elapsed = 0;
        }
        m_render_opacity = (unsigned short)(int)floorf((float)Lparam_super_overbrighten * ((float)elapsed / (float)fade_duration) + 0.5f);
    }
    else
    {
        mover_flags &= ~0x100u;
        fade_start_time = -1;
        m_render_opacity = (unsigned short)Lparam_super_overbrighten;
    }
    set_render_flags(2); // inlined
    return m_render_opacity != 0;
}

// 0x00A8C000 (:1956)
// Ramps the opacity down to 0 over fade_duration ms; a running fade in is reversed from its current point.
int object_mover::handle_fading_out(int fade_duration)
{
    const unsigned int flags = mover_flags;
    if ((flags & 0x400) != 0)
    {
        return 0;
    }
    if ((flags & 0x100) != 0)
    {
        mover_flags = (flags & ~0x100u) | 0x200;
        fade_start_time = timer_get(TIMER_MILLISECONDS) * 2 - fade_start_time - fade_duration;
        return m_render_opacity != 0;
    }
    if (fade_start_time == -1)
    {
        mover_flags = flags | 0x200;
        fade_start_time = timer_get(TIMER_MILLISECONDS);
        return m_render_opacity != 0;
    }
    int elapsed = timer_get(TIMER_MILLISECONDS) - fade_start_time;
    if (elapsed < fade_duration)
    {
        if (elapsed < 0)
        {
            elapsed = 0;
        }
        m_render_opacity = (unsigned short)(int)floorf((float)Lparam_super_overbrighten * (1.0f - (float)elapsed / (float)fade_duration) + 0.5f);
    }
    else
    {
        mover_flags = (mover_flags & ~0x200u) | 0x400;
        fade_start_time = -1;
        m_render_opacity = 0;
    }
    set_render_flags(2);
    return m_render_opacity != 0;
}

// 0x00A387E0 (:1995)
// Visibility from the opacity (and the hidden flag), placement, and the opacity as instance transparency.
void object_mover::update_render(rl_renderer*)
{
    rl_renderable_instance* ri = get_render_model_instance();
    if (ri == nullptr)
    {
        return;
    }
    const bool visible = m_render_opacity != 0 && (mover_flags & 0x80000) == 0;
    if (ri->get_is_visible() != visible)
    {
        ri->set_is_visible(visible);
    }
    if ((obj_flags.render_flags & 0xC) != 0)
    {
        ri->set_position(pos); // inlined
        ri->set_rotation(orient); // inlined
        game_render_scene_update(ri);
    }
    if ((obj_flags.render_flags & 0xA) != 0)
    {
        ri->set_opacity((float)m_render_opacity / 255.0f); // inlined, transparent below 1
    }
}

// 0x00A13D70 (:2024)
// A destroyed EDF target: recorded in the destroyed table, EDF control of its district drops by its control value
// (with a HUD message for low importance), and important targets are logged.
void object_mover::drop_control()
{
    if (ignore_importance_level() || team != HUMAN_TEAM_EDF)
    {
        return;
    }
    if (control <= 0.0f && (gameplay_props == nullptr || gameplay_props->importance <= OBJECT_IMPORTANCE_LEVEL_LOW))
    {
        return;
    }
    if (World.object_was_destroyed(get_handle()))
    {
        return;
    }
    World.object_add_to_destroyed_table(get_handle());
    if (control <= 0.0f)
    {
        return;
    }
    district* dp = district_get_from_pos(pos);
    if (get_importance_level() < OBJECT_IMPORTANCE_LEVEL_MEDIUM)
    {
        gfm_change_edf_control(-control, dp, "HM_EDF_PROPERTY_DESTROYED", L"ui_hud_diversion_icon_edf_low");
        return;
    }
    gfm_change_edf_control(-control, dp, nullptr, nullptr);
    const vector player_pos = gamelog_get_human_pos(Player);
    const int duration = gamelog_get_game_duration();
    const char difficulty = gamelog_get_difficulty();
    const int locale_id = gamelog_get_locale();
    const char pct_game_complete = gamelog_get_pct_complete();
    const int time_played = gamelog_get_time_played();
    const int target_crc = gamelog_get_mover_crc(this);
    const int time = gamelog_get_logging_time();
    const int player_checksum = gamelog_get_player_checksum();
    log_event_target_destroyed(player_checksum, time, target_crc, time_played, pct_game_complete, locale_id, difficulty, duration, player_pos);
}

// 0x00A38940 (:2059)
// Damage to the script point of the hit piece (bullets and melee unless the point ignores them). True when a point
// took damage.
bool object_mover::script_point_apply_damage(const object* source, float damage, damaged_by_types from_type, unsigned int hit_shape_key, bool from_server)
{
    if (Multiplayer && !multi_is_server() && !from_server)
    {
        return false;
    }
    if (get_sub_type() == OT_SUB_MOVER_GENERAL && (mover_flags & 0x4) == 0)
    {
        return false;
    }
    if (get_sub_type() == OT_SUB_MOVER_RFG && hit_shape_key == 0xFFFFFFFF)
    {
        return false;
    }
    const c_chunk_base* chunk = chunk_get_base_by_uid(chunk_uid);
    if (chunk == nullptr || chunk_get_destroyable_object(chunk, chunk_obj_uid, nullptr) == nullptr)
    {
        return false;
    }
    bool damaged = false;
    for (int idx = 0; idx < MAX_SCRIPT_POINT_INSTS; idx++)
    {
        script_point_effect_inst* speip = m_level_obj_properties->m_script_point_effect_insts[idx];
        if (speip == nullptr || (get_sub_type() == OT_SUB_MOVER_RFG && hit_shape_key != speip->shape_key))
        {
            continue;
        }
        const script_point_effect_info* info = m_level_obj_properties->m_base_properties->m_script_point_effect_infos[idx];
        const bool immune = (from_type == DBT_BULLET && (info->spe_flags & 0x8) != 0) || // No Bullet Damage
                            (from_type == DBT_MELEE && (info->spe_flags & 0x10) != 0); // No Melee Damage
        if (!immune && speip->hitpoints > 0)
        {
            speip->hitpoints -= (int)damage;
            if (speip->hitpoints < 1)
            {
                script_point_kill(idx, false);
            }
            damaged = true;
        }
        break;
    }
    if (source != nullptr)
    {
        final_attacker = resolve_attacker_object_mover(source); // inlined
    }
    return damaged;
}

// 0x00A38A70 (:2139)
// An exploding script point finishes its death; a prop whose own explosion went off runs its death step next frame.
void object_mover_handle_source_exploded(object* obj, const explosion_info* e_info)
{
    if (obj == nullptr || obj->get_obj_type() != OT_MOVER)
    {
        return;
    }
    object_mover* omp = (object_mover*)obj;
    for (int idx = 0; idx < MAX_SCRIPT_POINT_INSTS; idx++)
    {
        const script_point_effect_inst* speip = omp->m_level_obj_properties->m_script_point_effect_insts[idx];
        if (speip != nullptr && speip->state == SPES_DEAD && e_info == omp->m_level_obj_properties->m_base_properties->m_script_point_effect_infos[idx]->death_explosion)
        {
            omp->script_point_finish_death(idx);
            return;
        }
    }
    if (obj->get_sub_type() == OT_SUB_MOVER_GENERAL)
    {
        // gm_flags (0xF8), not mover_flags: 0x800 death explosion done is replaced by 0x4 play the death effect / swap
        // to the corpse model or delete (general_mover::do_frame_pre_sim).
        general_mover* gm = (general_mover*)omp;
        gm->gm_flags = (gm->gm_flags & ~0x800u) | 0x4;
    }
}

// 0x00A5FA50 (:2176)
// Queues the damaged script event (once until posted) of a static mover.
void object_mover_maybe_add_to_damaged_event_queue(object_mover* omp)
{
    if ((omp->mover_flags & 0x4) != 0 || (omp->mover_flags & 0x400000) != 0)
    {
        return;
    }
    unsigned int* handlep = Mover_damaged_event_free_queue.pop();
    if (handlep != nullptr)
    {
        omp->mover_flags |= 0x400000;
        *handlep = omp->get_handle();
        Mover_damaged_event_queue.push(handlep);
    }
}

// 0x00A5FAB0 (:2195)
// Queues the destroyed script event (once until posted) of a static mover.
void object_mover_maybe_add_to_destroyed_event_queue(object_mover* omp)
{
    if ((omp->mover_flags & 0x4) != 0 || (omp->mover_flags & 0x800000) != 0)
    {
        return;
    }
    unsigned int* handlep = Mover_destroyed_event_free_queue.pop();
    if (handlep != nullptr)
    {
        omp->mover_flags |= 0x800000;
        *handlep = omp->get_handle();
        Mover_destroyed_event_queue.push(handlep);
    }
}
