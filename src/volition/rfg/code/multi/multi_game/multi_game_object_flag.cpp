// Original: volition/rfg/code/multi/multi_game/multi_game_object_flag.cpp
//
// The flag object (item subtype multi_object_flag, a team's flag with its aura effect): at its base, carried
// (attached to the carrier's flag prop point), dropped (falls to the ground; returns after the mode's return time) and
// reset to its base. Capture the Flag in rfg.exe uses its own flag items (ctf_flag, multi_game_mode_capture_the_flag.cpp);
// this class is reached through multi_object_flag_process and the player queries.
// Part of the multi unity TU (rfg_multi_unity.obj, every multi/ .cpp in path order): the file statics are shared
// by the whole tree, and a shared one is placed in its first user (tools/hdrgen/specs/mp0_stubs.py).

#include "volition/rfg/code/multi/multi_game/multi_game_object_flag.h"

#include "volition/rfg/code/effects/effects.h"
#include "volition/rfg/code/effects/effects_shared.h"
#include "volition/rfg/code/effects/object_effect.h"
#include "volition/rfg/code/havok/havok_raycast.h"
#include "volition/rfg/code/level/rfg_prop_block_shared.h"
#include "volition/rfg/code/level/world.h"
#include "volition/rfg/code/level/world.inl"
#include "volition/rfg/code/multi/multi_core/include/multi.h"
#include "volition/rfg/code/multi/multi_game/multi_game_settings.h"
#include "volition/rfg/code/multi/multi_game/multi_gameplay_mode.h"
#include "volition/rfg/code/objects/humans/human_info.h"
#include "volition/rfg/code/objects/humans/player.h"
#include "volition/rfg/code/objects/object_prop_names_shared.h"
#include "volition/vlib/code/memory/object_pool.h"
#include "volition/vlib/code/os/frametime.h"

#include <math.h>
#include <string.h>

static const int MULTI_OBJECT_FLAG_POOL_SIZE = 1;
static const int MULTI_OBJECT_FLAG_POOL_USAGE = 0x1D0; // memory footprint registered with the instantiator

// ---- file-static globals
// multi_gameplay_mode Multi_gameplay_mode (0x0167DEE0) is used here too; placed in multi_core_platform.cpp.
// bool Server (0x02FEB590) is used here too; placed in multi_core_session_generic.cpp.
static object_pool<multi_object_flag, MULTI_OBJECT_FLAG_POOL_SIZE, 1> multi_object_flagpool; // 0x02FEB960 (0x010711A0)
static obj_class_instantiator multi_object_flag_instantiator("multi_object_flag", alloc_multi_object_flag, types_multi_object_flag, bpo_index_multi_object_flag, classname_multi_object_flag,
                                                             multi_object_flag::pool_remaining, MULTI_OBJECT_FLAG_POOL_USAGE, MULTI_OBJECT_FLAG_POOL_SIZE); // 0x0300F854 (0x010711F0)

// ---- class registration (:47)
// 0x0094BD90 (.h:24)
const object_type multi_object_flag::get_type_static()
{
    return OT_ITEM;
}

// 0x0094BDA0 (.h:24)
const object_sub_type multi_object_flag::get_subtype_static()
{
    return OT_SUB_ITEM_MULTI_FLAG;
}

// 0x0094BDB0 (.h:24)
const char* multi_object_flag::get_classname_static()
{
    return "multi_object_flag";
}

// 0x0094BDC0 (.h:24)
int multi_object_flag::get_bpo_index_static()
{
    return 0;
}

// 0x00959150 (.h:24)
const char* multi_object_flag::get_classname() const
{
    return "multi_object_flag";
}

// 0x0094BDD0 (.h:53)
player* multi_object_flag::get_carrier() const
{
    return carrier;
}

// 0x0094BDE0 (.h:56)
human_teams multi_object_flag::get_team() const
{
    return team;
}

// 0x0097D4B0 (:38): an item with the flag's team property
obj_flag_props::obj_flag_props(item_info* infop, const vector& pos, const matrix& orient, obj_item_flags& flags, human_teams team)
    : obj_item_props(infop, pos, orient, flags, COLLIDE_ITEM, -1.0f, INVALID_SALVAGE_MATERIAL)
{
    set(FLAG_PROP_MP_TEAM, human_team_get_internal_name(team));
}

// 0x0097D6B0
obj_flag_props::~obj_flag_props()
{
}

// 0x009590E0
void types_multi_object_flag(signed char& type, signed char& subtype)
{
    type = OT_ITEM;
    subtype = OT_SUB_ITEM_MULTI_FLAG;
}

// 0x009590F0
int bpo_index_multi_object_flag()
{
    return 0;
}

// 0x00959100
const char* classname_multi_object_flag()
{
    return "multi_object_flag";
}

// 0x00959110
unsigned int multi_object_flag::get_classname_hash()
{
    return multi_object_flag_instantiator.this_hash;
}

// 0x0097D520
int multi_object_flag::pool_remaining()
{
    return multi_object_flagpool.get_free();
}

// 0x009B0890
void multi_object_flag::pool_free()
{
    multi_object_flagpool.free(this);
}

// 0x0099ADA0
object* alloc_multi_object_flag()
{
    return multi_object_flagpool.alloc(0);
}

// 0x00959120 (:48)
multi_object_flag::multi_object_flag()
{
    dropped_time.invalidate();
    flag_state = MOFS_NONE;
}

// 0x00959160
multi_object_flag::~multi_object_flag()
{
}

// 0x00959170 (:53): multiplayer only; the team property
bool multi_object_flag::serialize(obj_prop_block& pblock, bool load)
{
    if (!Multiplayer || !item::serialize(pblock, load))
    {
        return false;
    }
    char team_name[64];
    if (load)
    {
        pblock.get(FLAG_PROP_MP_TEAM, team_name);
        team = human_team_from_name(team_name);
    }
    else
    {
        strcpy(team_name, human_team_get_internal_name(team));
        pblock.set(FLAG_PROP_MP_TEAM, team_name);
    }
    return true;
}

// 0x00959240 (:79): no physics; on load the base is the placed position, and the team's aura effect plays on the
// "FX" tag (or at the flag)
bool multi_object_flag::stream(bool load)
{
    item_flags.no_physics = 1;
    if (!item::stream(load))
    {
        return false;
    }
    if (!load)
    {
        return true;
    }
    base_position = pos;
    base_orient = orient;
    flag_state = MOFS_AT_BASE;
    carrier = nullptr;
    effect = 0xFFFFFFFFu;
    effect_tag_index = get_tag_index_from_name("FX");
    vector effect_pos;
    matrix effect_orient;
    if (effect_tag_index < 0)
    {
        effect_pos = pos;
        effect_orient = orient;
    }
    else
    {
        get_tag_world_coords(effect_tag_index, effect_pos, &effect_orient, nullptr, nullptr);
    }
    // The red flag's info name has "red" in it.
    const char* effect_name = strstr(info->name, "red") != nullptr ? "mp_flag_aura_red_a" : "mp_flag_aura_blue_a";
    effect = effects_play(effects_get_handle(effect_name), effect_pos, effect_orient, true, get_handle(), effect_tag_index, 0xFFFFFFFFu, 1.0f, false, EFFECTS_CATEGORY_NONE);
    return true;
}

// 0x0097D550 (:128)
void multi_object_flag::free_this()
{
    if (effect != 0xFFFFFFFFu)
    {
        object_effect* effectp = (object_effect*)World.object_from_handle(effect);
        if (effectp != nullptr && effectp->get_obj_type() == OT_EFFECT)
        {
            effectp->stop(object_effect::SS_STOP_EMITTING);
            effect = 0xFFFFFFFFu;
        }
    }
    item::free_this();
}

// 0x009B08A0 (:152): a flag not carried (and not about to return on the server) falls to the ground (5 m/s). Original:
// unlike the bag (multi_game_mode_bagman.cpp), a fall faster than a frame's step moves the flag straight onto the hit's
// x and z and lowers it by one step.
void multi_object_flag::process()
{
    if (flag_state == MOFS_PICKED_UP)
    {
        return;
    }
    if (Multiplayer && Server && flag_state == MOFS_DROPPED && dropped_time.valid() && dropped_time.elapsed())
    {
        return;
    }
    // Magic numbers: the ray starts 10 cm above the flag and reaches 100 m down.
    raycast_input in;
    in.start.set(pos.x, pos.y + 0.1f, pos.z);
    in.end.set(pos.x, pos.y - 100.0f, pos.z);
    in.radius = 0.0f;
    in.filter_params = nullptr;
    in.filter_callback = nullptr;
    in.m_filter_info = COLLIDE_RAY_DEFAULT;
    in.m_hit_collector_type = raycast_input::COLLECT_CLOSEST_HIT;
    in.m_flags = 0;
    raycast_output out;
    if (!havok_raycast(in, out))
    {
        return;
    }
    const float y = pos.y;
    const float fall = Frametime * 5.0f;
    const vector hit = out.m_hits[0].hit_pos;
    if (fabsf(y - hit.y) >= fall)
    {
        pos.x = hit.x;
        pos.z = hit.z;
        pos.y = y - fall;
    }
    else
    {
        pos = hit;
    }
    update_pos_and_orient(pos, orient, true);
}

// 0x0097D590 (:195): back to the base, at no carrier
void multi_object_flag::reset()
{
    if (carrier != nullptr)
    {
        carrier->remove_child(this);
    }
    carrier = nullptr;
    flag_state = MOFS_AT_BASE;
    update_pos_and_orient(base_position, base_orient, true);
    dropped_time.invalidate();
}

// 0x0097D5E0 (:213)
void multi_object_flag::event_picked_up(player* carrier_player)
{
    carrier = carrier_player;
    flag_state = MOFS_PICKED_UP;
    attach_to_player(carrier_player);
    dropped_time.invalidate();
}

// 0x0097D610 (:224): the return countdown of the mode (Capture the Flag's flag return time; in the bagman modes the
// first byte of the mode options, the bag return time). Original: the carrier is not checked for null.
void multi_object_flag::event_dropped()
{
    carrier->remove_child(this);
    carrier = nullptr;
    flag_state = MOFS_DROPPED;
    orient.make_identity();
    if (!Multiplayer)
    {
        return;
    }
    if (Multi_gameplay_mode == MGPM_CAPTURE_THE_FLAG)
    {
        dropped_time.set((int)Mp_server_options.mode_options.capture_the_flag.flag_return_time_seconds * 1000);
    }
    else if (Multi_gameplay_mode == MGPM_BAGMAN || Multi_gameplay_mode == MGPM_TEAM_BAGMAN)
    {
        dropped_time.set((int)Mp_server_options.mode_options.bagman.bag_return_time_seconds * 1000);
    }
}

// 0x009593D0 (:241): the flag hangs on each prop point of the carrier's flag props (PROP_TYPE_MP_FLAG) made for this
// flag's item
void multi_object_flag::attach_to_player(player* pp)
{
    const human_info* h_info = pp->h_info;
    for (int i = 0; i < h_info->num_general_props; i++)
    {
        const human_info::human_prop_general_info& general = h_info->general_props[i];
        if (general.m_prop_type != PROP_TYPE_MP_FLAG)
        {
            continue;
        }
        for (int j = 0; j < h_info->general_props[i].num_props; j++)
        {
            const human_info::human_prop_info& prop = h_info->general_props[i].props[j];
            if (prop.item == info)
            {
                const int child_tag = get_tag_index_from_name(prop.item_prop_tag);
                pp->set_child(this, prop.character_prop_index, child_tag, false, true);
            }
        }
    }
}

// 0x009594A0 (:265)
void multi_object_flag::detach_from_player(player* pp)
{
    pp->remove_child(this);
}

// 0x0099ADB0 (:279): the team's flag item (blue for the Guerillas, red for the EDF; no info for other teams)
multi_object_flag* multi_object_flag_create(const vector& pos, human_teams team)
{
    item_info* infop = nullptr;
    if (team == HUMAN_TEAM_GUERILLA)
    {
        infop = object_item_get_info("multi_object_flag_blue");
    }
    else if (team == HUMAN_TEAM_EDF)
    {
        infop = object_item_get_info("multi_object_flag_red");
    }
    matrix orient;
    orient.make_identity();
    // rfg.exe builds the flags in the team argument's slot: no physics, simulated, casting shadows (0x6006).
    obj_item_flags flags;
    memset(&flags, 0, sizeof(flags));
    flags.no_physics = 1;
    flags.simulate = 1;
    flags.cast_shadows = 1;
    flags.cast_drop_shadows = 1;
    obj_item_props props(infop, pos, orient, flags, COLLIDE_ITEM, -1.0f, INVALID_SALVAGE_MATERIAL);
    props.set(FLAG_PROP_MP_TEAM, human_team_get_internal_name(team));
    return World.create_object<multi_object_flag>(props, 0xFFFFFFFF);
}

// 0x009B0A90 (:306)
void multi_object_flag_process()
{
    const int count = World.get_num_objects<multi_object_flag>();
    for (int i = 0; i < count; i++)
    {
        World.get_object<multi_object_flag>(i)->process();
    }
}

// 0x0099AE70 (:318)
multi_object_flag* player_get_flag(const player* pp)
{
    if (pp == nullptr)
    {
        return nullptr;
    }
    const int count = World.get_num_objects<multi_object_flag>();
    for (int i = 0; i < count; i++)
    {
        multi_object_flag* flag = World.get_object<multi_object_flag>(i);
        if (flag->get_carrier() == pp)
        {
            return flag;
        }
    }
    return nullptr;
}

// 0x0099AEC0 (:340)
bool player_has_flag(const player* pp)
{
    return player_get_flag(pp) != nullptr;
}

// 0x0099AEE0 (:349)
multi_object_flag* multi_object_flag_get_flag_by_team(human_teams team)
{
    const int count = World.get_num_objects<multi_object_flag>();
    for (int i = 0; i < count; i++)
    {
        multi_object_flag* flag = World.get_object<multi_object_flag>(i);
        if (flag->get_team() == team)
        {
            return flag;
        }
    }
    return nullptr;
}

// 0x0099AF30 (:363): every flag back to its base (multi_object_flag::reset inlined)
void multi_object_flag_reset()
{
    const int count = World.get_num_objects<multi_object_flag>();
    for (int i = 0; i < count; i++)
    {
        World.get_object<multi_object_flag>(i)->reset();
    }
}
