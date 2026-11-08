// Original: volition/rfg/code/objects/object_marauder_ambush_region.cpp
//
// Whole (NPC.md NP3). A box region that may spawn its child squad spawn nodes when the player enters it. With nodes of
// opposing teams it is a skirmish: the squads fight each other, otherwise they attack the player.

#include "volition/rfg/code/objects/object_marauder_ambush_region.h"

#include "volition/rfg/code/ai/ai_system.h"
#include "volition/rfg/code/destruction/rfg_mover.h"
#include "volition/rfg/code/gameplay/activities/activity.h"
#include "volition/rfg/code/gameplay/gameflow/gfm_orders_manager.h"
#include "volition/rfg/code/gameplay/missions.h"
#include "volition/rfg/code/gamewide/downloadable_content.h"
#include "volition/rfg/code/gamewide/game_clock.h"
#include "volition/rfg/code/havok/havok_bp.h"
#include "volition/rfg/code/level/rfg_prop_block_shared.h"
#include "volition/rfg/code/level/world.h"
#include "volition/rfg/code/multi/multi_core/include/multi.h"
#include "volition/rfg/code/multi/multi_game/modes/multi_game_mode_common.h"
#include "volition/rfg/code/objects/humans/human_teams.h"
#include "volition/rfg/code/objects/humans/human_voice/human_voice_shared.h"
#include "volition/rfg/code/objects/humans/npc.h"
#include "volition/rfg/code/objects/humans/player.h"
#include "volition/rfg/code/objects/object_prop_names_shared.h"
#include "volition/rfg/code/objects/object_spawn_node.h"
#include "volition/rfg/code/objects/object_squad.h"
#include "volition/rfg/code/scripting/action_script/global_event_data.h"
#include "volition/rfg/code/scripting/action_script/global_event_manager.h"
#include "volition/rfg/code/spawn/spawn_enums.h"
#include "volition/rfg/code/spawn/spawn_resource_manager.h"
#include "volition/rfg/code/vehicle/vehicle_class.h"
#include "volition/vlib/code/math/ix.h"
#include "volition/vlib/code/math/rand.h"
#include "volition/vlib/code/memory/object_pool.h"
#include "volition/vlib/code/util/varray.h"

static const int MAX_AMBUSH_REGIONS = 25;
// Magic number: farray<bool,15> on trigger_ambush's stack; a region with more spawn nodes writes past it (original).
static const int MAX_AMBUSH_SPAWN_FLAGS = 15;
// Magic numbers: squads update every 1 to 2 seconds.
static const int SQUAD_UPDATE_MIN_MS = 1000;
static const int SQUAD_UPDATE_MAX_MS = 2000;
// Magic number: 75 m. The ambush line plays from no further away than this, as squared.
static const float VOICE_MAX_DIST = 75.0f;
static const float VOICE_MAX_DIST_SQ = 5625.0f;
// Magic number: a player 400 m away from the box (squared) ends the ambush.
static const float ABORT_DIST_SQ = 160000.0f;
// Magic number: an old vehicle within 5 m of the node (squared) is destroyed to make room.
static const float CLEAN_VEHICLE_DIST_SQ = 25.0f;

static object_pool<marauder_ambush_region, MAX_AMBUSH_REGIONS, 1> marauder_ambush_regionpool; // 0x033AFEE0
static bool Ambush_voice_line_played; // 0x0302F155

// 0x00A19C60 (:58)
void types_marauder_ambush_region(signed char& type, signed char& subtype)
{
    type = OT_MARAUDER_AMBUSH_REGION;
    subtype = OT_SUB_UNDEFINED;
}

// 0x00A19C70 (:58)
int bpo_index_marauder_ambush_region()
{
    return 1;
}

// 0x00A19C80 (:58)
const char* classname_marauder_ambush_region()
{
    return "marauder_ambush_region";
}

// 0x00A66E90 (:58)
object* alloc_marauder_ambush_region()
{
    return marauder_ambush_regionpool.alloc(0);
}

// Magic numbers: rfg.exe's arguments (0x01077A50), the pool's bytes and capacity.
static obj_class_instantiator marauder_ambush_region_instantiator("marauder_ambush_region", alloc_marauder_ambush_region, types_marauder_ambush_region, bpo_index_marauder_ambush_region, classname_marauder_ambush_region, marauder_ambush_region::pool_remaining, 0x1B50, MAX_AMBUSH_REGIONS); // 0x033C144C

// 0x00A19C90 (:58)
unsigned int marauder_ambush_region::get_classname_hash()
{
    return marauder_ambush_region_instantiator.this_hash;
}

// 0x00A3CC30 (:58)
int marauder_ambush_region::pool_remaining()
{
    return marauder_ambush_regionpool.get_free();
}

// 0x00A91800 (:58)
void marauder_ambush_region::pool_free()
{
    marauder_ambush_regionpool.free(this);
}

// 0x00A19CA0 (:59). flags is not cleared here or in serialize, which sets only some of its bits, so a reused pool slot keeps
// the others (original).
marauder_ambush_region::marauder_ambush_region()
{
}

// 0x00A19CE0
marauder_ambush_region::~marauder_ambush_region()
{
}

// 0x00A19C00 (.h:48)
const object_type marauder_ambush_region::get_type_static()
{
    return OT_MARAUDER_AMBUSH_REGION;
}

// 0x00A19C10 (.h:48)
const object_sub_type marauder_ambush_region::get_subtype_static()
{
    return OT_SUB_UNDEFINED;
}

// 0x00A19C20 (.h:48)
const char* marauder_ambush_region::get_classname_static()
{
    return "marauder_ambush_region";
}

// 0x00A19C30 (.h:48)
int marauder_ambush_region::get_bpo_index_static()
{
    return 1;
}

// 0x00A19CD0 (.h:48)
const char* marauder_ambush_region::get_classname() const
{
    return "marauder_ambush_region";
}

// 0x00A19C40 (.h:122)
bool marauder_ambush_region::is_triggered() const
{
    return flags.ambush_is_triggered != 0;
}

// The child squad of a spawn node, or null.
static object_squad* spawn_node_squad(const object_squad_spawn_node* node)
{
    const unsigned int handle = node->get_child_squad_handle();
    object* objp = handle != 0xFFFFFFFF ? World.object_from_handle(handle) : nullptr;
    return objp != nullptr && objp->get_obj_type() == OT_SQUAD ? reinterpret_cast<object_squad*>(objp) : nullptr;
}

// 0x00A66EA0 (:69). The first member of the node's squad calls out once per ambush, heard from at most 75 m. A squad with no
// members reads a stale first entry (original).
static void maybe_play_ambush_voice_line(const object_squad_spawn_node* sound_squad_spawn_node)
{
    if (sound_squad_spawn_node == nullptr || Ambush_voice_line_played)
    {
        return;
    }
    const object_squad* squad = spawn_node_squad(sound_squad_spawn_node);
    if (squad == nullptr)
    {
        return;
    }
    const unsigned int member_handle = squad->member_array[0];
    object* member = member_handle != 0xFFFFFFFF ? World.object_from_handle(member_handle) : nullptr;
    if (member == nullptr || member->get_obj_type() != OT_HUMAN || member->get_sub_type() != OT_SUB_HUMAN_NPC)
    {
        return;
    }
    const human_voice_persona* voice = static_cast<const npc*>(member)->get_voice();
    if (voice == nullptr)
    {
        return;
    }
    vector offset = member->pos - Player->pos;
    if (VOICE_MAX_DIST_SQ < offset * offset)
    {
        offset.normalize();
        offset = offset * VOICE_MAX_DIST;
    }
    const vector sound_pos = Player->pos + offset;
    int play_id = -1;
    unsigned short radio_id = 0;
    if (voice->persona_play_voice_line(VOICE_LINE_MARAUDER_AMBUSH, sound_pos, AUDIOLIB_CUE_PRIORITY_DIALOGUE_NORMAL, play_id, radio_id, nullptr, false, 0))
    {
        Ambush_voice_line_played = true;
    }
}

// 0x00AA9B40 (:140). enabled is saved as 4 bytes and only loaded at 1 byte, so a saved value never comes back through here
// (original). Only the two named flags are reset on load.
bool marauder_ambush_region::serialize(obj_prop_block& pblock, bool load)
{
    if (!object::serialize(pblock, load))
    {
        return false;
    }
    if (!load)
    {
        pblock.set<bbox>(TR_PROP_BOX, local_bb);
        const unsigned int enabled_value = flags.enabled;
        pblock.set<unsigned int>(TR_PROP_ENABLED, enabled_value);
        pblock.set<float>(MAR_PROP_DAY_PROB, day_ambush_prob);
        pblock.set<float>(MAR_PROP_NIGHT_PROB, night_ambush_prob);
        pblock.set<int>(MAR_PROP_MIN_SPAWNS, min_squad_spawns);
        pblock.set<int>(MAR_PROP_MAX_SPAWNS, max_squad_spawns);
        return true;
    }
    pblock.get(TR_PROP_BOX, local_bb);
    radius = 0.0f;
    bbox_instance(bb, orient, pos, local_bb);
    flags.enabled = 1;
    char enabled_value;
    if (pblock.get_optional(TR_PROP_ENABLED, enabled_value))
    {
        flags.enabled = enabled_value != 0 ? 1 : 0;
    }
    pblock.get_optional(MAR_PROP_DAY_PROB, day_ambush_prob);
    pblock.get_optional(MAR_PROP_NIGHT_PROB, night_ambush_prob);
    pblock.get_optional(MAR_PROP_MIN_SPAWNS, min_squad_spawns);
    pblock.get_optional(MAR_PROP_MAX_SPAWNS, max_squad_spawns);
    flags.ambush_is_triggered = 0;
    flags.abort_ambush = 0;
    last_pos = pos;
    edf_squad_spawn_node = 0xFFFFFFFF;
    marauder_squad_spawn_node = 0xFFFFFFFF;
    guerrilla_squad_spawn_node = 0xFFFFFFFF;
    edf_squad = nullptr;
    marauder_squad = nullptr;
    guerrilla_squad = nullptr;
    is_day = game_clock_get_tod_partitioned() != GC_MID_NIGHT;
    squad_update_timer.set_rand(SQUAD_UPDATE_MIN_MS, SQUAD_UPDATE_MAX_MS);
    return true;
}

// 0x00A19CF0 (:216)
bool marauder_ambush_region::stream(bool load)
{
    if (!load && obj_flags.destroy_on_stream != 0)
    {
        World.destroy_object(this, false);
    }
    return true;
}

// 0x00A91810 (:247): rfg.exe has object::free_this inlined here.
void marauder_ambush_region::free_this()
{
    object::free_this();
}

// 0x00A19D10 (:253)
void marauder_ambush_region::get_bbox(vector& bmin, vector& bmax) const
{
    bmin = bb.min;
    bmax = bb.max;
}

// 0x00A19D50 (:260)
void marauder_ambush_region::get_local_bbox(vector& bmin, vector& bmax) const
{
    bmin = local_bb.min;
    bmax = local_bb.max;
}

// 0x00A19D90 (:270). bb is not moved, so get_bbox keeps the box from the load (original).
void marauder_ambush_region::update_pos_and_orient(const vector& update_pos, const matrix& update_orient, bool)
{
    pos = update_pos;
    orient = update_orient;
    if (bp_handle != nullptr)
    {
        get_bbox(last_known_bmin, last_known_bmax);
        havok_bpo_update(bp_handle, last_known_bmin, last_known_bmax);
    }
}

// 0x00A19E10 (:281): always 0, as serialize sets it (original).
float marauder_ambush_region::get_radius() const
{
    return radius;
}

// 0x00A19E20 (:329)
void marauder_ambush_region::enable()
{
    flags.enabled = 1;
}

// 0x00A19E30 (:335)
void marauder_ambush_region::disable()
{
    flags.enabled = 0;
}

// 0x00A19E40 (:341)
bool marauder_ambush_region::is_enabled() const
{
    return flags.enabled != 0;
}

// 0x00A19E50 (:348)
float marauder_ambush_region::get_ambush_chance()
{
    return game_clock_get_tod_partitioned() != GC_MID_NIGHT ? day_ambush_prob : night_ambush_prob;
}

// 0x00A19E70 (:359)
void marauder_ambush_region::disable_ambush()
{
    flags.ambush_is_triggered = 0;
}

// 0x00A67100 (:365)
void marauder_ambush_region::clean_old_vehicle(const object_squad_spawn_node* squad_node) const
{
    const unsigned int vehicle_handle = squad_node->get_child_vehicle_handle();
    object* objp = vehicle_handle != 0xFFFFFFFF ? World.object_from_handle(vehicle_handle) : nullptr;
    if (objp != nullptr && objp->get_obj_type() == OT_VEHICLE && !objp->is_killed() && vec_dist_squared(squad_node->pos, objp->pos) < CLEAN_VEHICLE_DIST_SQ)
    {
        World.destroy_object(objp, false);
    }
}

// 0x00A3CC60 (:386)
int marauder_ambush_region::count_squad_spawns()
{
    int count = 0;
    object* const first = child_ptr;
    object* child = first;
    while (child != nullptr)
    {
        if (child->get_obj_type() == OT_SQUAD_SPAWN_NODE)
        {
            count++;
        }
        child = child->child_next;
        if (child == first)
        {
            break;
        }
    }
    return count;
}

// Whether the team disposition table counts, which rfg.exe tests inline wherever it reads the guerrilla and marauder entry
// (Current_human_team_dispositions[0][3]).
static bool team_dispositions_apply()
{
    return Multiplayer != true || multi_game_mode_has_teams();
}

// 0x00A91820 (:410). The pick chooses which nodes to spawn, but the spawn loop below reads only the first flag: every node
// spawns if the first was picked, and none otherwise (original, 0x00A9198D reads the array's first byte each time). A pick
// that is refused for lack of vehicle slots still counts.
void marauder_ambush_region::trigger_ambush()
{
    flags.ambush_is_triggered = 1;
    flags.abort_ambush = 0;
    if (flags.skirmish_ambush == 0)
    {
        // Magic number: 3, a disposition; what its name means was not looked up.
        if (flags.marauders_used != 0 && team_dispositions_apply() && 3 <= (int)Current_human_team_dispositions[0][3])
        {
            return;
        }
        if (flags.guerrillas_used != 0)
        {
            return;
        }
    }
    if (flags.marauders_used != 0 && srm_get_availible_slots(SPAWN_SLOT_HUMAN_MARAUDER, RESOURCE_SLOT_USER_ID_SQUAD) <= 0 && srm_get_used_slots(SPAWN_SLOT_HUMAN_MARAUDER, RESOURCE_SLOT_USER_ID_SQUAD) <= 0)
    {
        return;
    }
    const int num_spawns = count_squad_spawns();
    if (num_spawns == 0)
    {
        return;
    }
    // Original bug kept: index below goes up to num_spawns - 1, so a region with more than 15 squad spawn nodes writes past these 15 flags.
    bool picked[MAX_AMBUSH_SPAWN_FLAGS];
    for (int i = 0; i < MAX_AMBUSH_SPAWN_FLAGS; i++)
    {
        picked[i] = false;
    }
    const int low = num_spawns < min_squad_spawns ? num_spawns : min_squad_spawns;
    const int high = num_spawns < max_squad_spawns ? num_spawns : max_squad_spawns;
    const int wanted = int_rand_range(low, high);
    int chosen = 0;
    while (chosen < wanted)
    {
        const int index = int_rand_range(0, num_spawns - 1);
        if (!picked[index])
        {
            if (0 < srm_get_availible_slots(SPAWN_SLOT_VEHICLE_MULTI_PURPOSE, RESOURCE_SLOT_USER_ID_SQUAD) || 0 < srm_get_used_slots(SPAWN_SLOT_VEHICLE_MULTI_PURPOSE, RESOURCE_SLOT_USER_ID_SQUAD))
            {
                picked[index] = true;
            }
            chosen++;
        }
    }
    object* const first = child_ptr;
    object* child = first;
    while (child != nullptr)
    {
        if (child->get_obj_type() == OT_SQUAD_SPAWN_NODE && picked[0])
        {
            object_squad_spawn_node* node = static_cast<object_squad_spawn_node*>(child);
            clean_old_vehicle(node);
            node->spawn_child_object(MAP_ELEMENT_INVALID, MAP_ICON_TYPE_NORMAL);
        }
        child = child->child_next;
        if (child == first)
        {
            break;
        }
    }
    Ambush_voice_line_played = false;
}

// The squad of a node handle, or null.
static object_squad* find_node_squad(unsigned int node_handle)
{
    object* objp = node_handle != 0xFFFFFFFF ? World.object_from_handle(node_handle) : nullptr;
    if (objp == nullptr || objp->get_obj_type() != OT_SQUAD_SPAWN_NODE)
    {
        return nullptr;
    }
    return spawn_node_squad(static_cast<object_squad_spawn_node*>(objp));
}

// 0x00A3CC90 (:509)
void marauder_ambush_region::find_skirmish_squads()
{
    edf_squad = find_node_squad(edf_squad_spawn_node);
    marauder_squad = find_node_squad(marauder_squad_spawn_node);
    guerrilla_squad = find_node_squad(guerrilla_squad_spawn_node);
}

static bool ambush_squad_alive(object_squad* squad)
{
    return squad != nullptr && !squad->is_killed();
}

// 0x00A919D0 (:536). Answers whether the squad has a live member. A skirmish attacks the first member of the opposing squad,
// and falls back on the player when there is none; the marauders and guerrillas only fight when hostile to each other.
bool marauder_ambush_region::update_squad_orders(object_squad* squad_ptr)
{
    human* enemy = Player;
    if (flags.skirmish_ambush != 0)
    {
        const human_teams team = squad_ptr->get_squad_team();
        const bool guerrillas_fight_marauders = !team_dispositions_apply() || Current_human_team_dispositions[0][3] == TEAM_DISPOSITION_HOSTILE;
        object_squad* target = nullptr;
        if (team == HUMAN_TEAM_GUERILLA)
        {
            if (ambush_squad_alive(edf_squad))
            {
                target = edf_squad;
            }
            else if (guerrillas_fight_marauders && ambush_squad_alive(marauder_squad))
            {
                target = marauder_squad;
            }
        }
        else if (team == HUMAN_TEAM_EDF)
        {
            if (ambush_squad_alive(marauder_squad))
            {
                target = marauder_squad;
            }
            else if (ambush_squad_alive(guerrilla_squad))
            {
                target = guerrilla_squad;
            }
        }
        else if (team == HUMAN_TEAM_MARAUDER)
        {
            if (ambush_squad_alive(edf_squad))
            {
                target = edf_squad;
            }
            else if (guerrillas_fight_marauders && ambush_squad_alive(guerrilla_squad))
            {
                target = guerrilla_squad;
            }
        }
        if (target != nullptr)
        {
            // Reads the first entry whatever the member count (original).
            const unsigned int member_handle = target->member_array[0];
            object* member = member_handle != 0xFFFFFFFF ? World.object_from_handle(member_handle) : nullptr;
            if (member != nullptr && member->get_obj_type() == OT_HUMAN)
            {
                enemy = static_cast<human*>(member);
            }
        }
    }
    const bool friendly = human_teams_are_friendly(enemy->current_team, squad_ptr->get_squad_team());
    gfm_orders* orders = squad_ptr->get_squad_orders();
    if (orders == nullptr)
    {
        flags.abort_ambush = 1;
        return false;
    }
    if (!friendly && (orders->get_orders_type() != GFM_ORDERS_TYPE_ATTACK || orders->get_target_object() != enemy))
    {
        gfm_orders_creation_params params;
        params.ofp.reset();
        params.reset();
        params.team = squad_ptr->get_squad_team();
        params.object_target = enemy->get_handle();
        params.location_target = enemy->pos;
        params.orders_type = GFM_ORDERS_TYPE_ATTACK;
        squad_ptr->assigned_orders_handle = Orders_manager.create_new_orders(params);
        squad_ptr->assign_orders_to_members();
    }
    bool any_alive = false;
    for (int i = 0; i < squad_ptr->member_array.size(); i++)
    {
        const unsigned int member_handle = squad_ptr->member_array[i];
        object* member = member_handle != 0xFFFFFFFF ? World.object_from_handle(member_handle) : nullptr;
        if (member != nullptr && member->get_obj_type() == OT_HUMAN && member->get_sub_type() == OT_SUB_HUMAN_NPC && !member->is_killed())
        {
            npc* np = static_cast<npc*>(member);
            any_alive = true;
            if (!friendly)
            {
                np->get_ai()->perception.alert_enemy(nullptr, enemy);
            }
            np->npc_flags.ambush_related = 1;
        }
    }
    return any_alive;
}

// 0x00A67190 (:635): which teams the spawn nodes bring. A second team makes it a skirmish. A later node of a team replaces
// the earlier one.
void marauder_ambush_region::check_teams()
{
    object* const first = child_ptr;
    object* child = first;
    while (child != nullptr)
    {
        if (child->get_obj_type() == OT_SQUAD_SPAWN_NODE)
        {
            const squad_definition* def = static_cast<object_squad_spawn_node*>(child)->squad_def;
            const human_teams team = def != nullptr ? def->squad_team : HUMAN_TEAM_NONE;
            if (team == HUMAN_TEAM_GUERILLA)
            {
                if (flags.marauders_used != 0 || flags.edf_used != 0)
                {
                    flags.skirmish_ambush = 1;
                }
                flags.guerrillas_used = 1;
                guerrilla_squad_spawn_node = child->get_handle();
            }
            else if (team == HUMAN_TEAM_EDF)
            {
                if (flags.marauders_used != 0 || flags.guerrillas_used != 0)
                {
                    flags.skirmish_ambush = 1;
                }
                flags.edf_used = 1;
                edf_squad_spawn_node = child->get_handle();
            }
            else if (team == HUMAN_TEAM_MARAUDER)
            {
                if (flags.edf_used != 0 || flags.guerrillas_used != 0)
                {
                    flags.skirmish_ambush = 1;
                }
                flags.marauders_used = 1;
                marauder_squad_spawn_node = child->get_handle();
            }
        }
        child = child->child_next;
        if (child == first)
        {
            break;
        }
    }
}

// 0x00A91C90 (:680)
void marauder_ambush_region::update_squads()
{
    const object_squad_spawn_node* voice_node = nullptr;
    bool all_gone = true;
    object* const first = child_ptr;
    object* child = first;
    while (child != nullptr)
    {
        if (child->get_obj_type() == OT_SQUAD_SPAWN_NODE)
        {
            const object_squad_spawn_node* node = static_cast<const object_squad_spawn_node*>(child);
            object_squad* squad = spawn_node_squad(node);
            if (squad != nullptr)
            {
                if (squad->is_killed())
                {
                    squad->despawn_members();
                    if (squad->base_orders_handle != 0xFFFFFFFF)
                    {
                        squad->clear_orders(squad->base_orders_handle);
                    }
                    if (squad->assigned_orders_handle != 0xFFFFFFFF)
                    {
                        squad->clear_orders(squad->assigned_orders_handle);
                    }
                    World.destroy_object(reinterpret_cast<object*>(squad), false);
                }
                else if (flags.abort_ambush == 0)
                {
                    all_gone = false;
                    if (update_squad_orders(squad) && flags.skirmish_ambush == 0 && node->squad_def != nullptr && node->squad_def->squad_team == HUMAN_TEAM_MARAUDER)
                    {
                        voice_node = node;
                    }
                }
                else
                {
                    // An aborting squad does not hold the ambush open, so the ambush can end on this update.
                    squad->request_despawn_all_members(dlc_custom_despawn_priority(), nullptr, true);
                }
            }
        }
        child = child->child_next;
        if (child == first)
        {
            break;
        }
    }
    if (all_gone)
    {
        flags.ambush_is_triggered = 0;
        if (flags.marauders_used != 0 && !mission_is_active())
        {
            srm_release_all_slots(SPAWN_SLOT_HUMAN_MARAUDER, RESOURCE_SLOT_USER_ID_SQUAD);
            srm_release_all_slots(SPAWN_SLOT_VEHICLE_MULTI_PURPOSE, RESOURCE_SLOT_USER_ID_SQUAD);
        }
    }
    if (flags.skirmish_ambush == 0 && flags.marauders_used != 0)
    {
        maybe_play_ambush_voice_line(voice_node);
    }
}

// 0x00A19E80 (:736)
void marauder_ambush_region::check_abort_conditions()
{
    if (Player == nullptr)
    {
        return;
    }
    vector bmin;
    vector bmax;
    get_bbox(bmin, bmax);
    if (ABORT_DIST_SQ < bbox_find_distance_squared(bmin, bmax, Player->pos))
    {
        flags.abort_ambush = 1;
    }
}

// 0x00A19EE0 (:770): empty in rfg.exe.
void ambush_region_system_init()
{
}

// 0x00A19EF0 (:780)
bool ambush_region_point_inside(const marauder_ambush_region* trigger, const vector& point)
{
    vector bmin;
    vector bmax;
    trigger->get_local_bbox(bmin, bmax);
    return ix_point_in_box_oriented(point, trigger->pos, trigger->orient, bmin, bmax);
}

// 0x00A91E00 (:787). During a mission or an activity no region triggers, and those already triggered are aborted.
void ambush_region_do_frame()
{
    player* pp = Player;
    if (pp == nullptr || Multiplayer)
    {
        return;
    }
    const bool busy = mission_is_active() || !activity_none();
    const int count = World.get_num_objects<marauder_ambush_region>();
    for (int i = 0; i < count; i++)
    {
        marauder_ambush_region* region = World.get_object<marauder_ambush_region>(i);
        if (busy)
        {
            if (region->flags.ambush_is_triggered != 0)
            {
                region->flags.abort_ambush = 1;
                region->update_squads();
            }
            continue;
        }
        if (region->flags.teams_checked == 0)
        {
            region->flags.teams_checked = 1;
            region->check_teams();
        }
        if (region->flags.enabled != 0)
        {
            // A building damaged past its destroyed percentage ends its region for good, and last_pos stays as it was.
            const unsigned int parent_handle = region->get_parent();
            object* parent = parent_handle != 0xFFFFFFFF ? World.object_from_handle(parent_handle) : nullptr;
            if (parent != nullptr && parent->get_obj_type() == OT_MOVER && parent->get_sub_type() == OT_SUB_MOVER_RFG)
            {
                const rfg_mover* mover = static_cast<const rfg_mover*>(parent);
                if (mover->game_destroyed_pct < mover->percent_destroyed())
                {
                    region->flags.enabled = 0;
                    continue;
                }
            }
            vector bmin;
            vector bmax;
            region->get_local_bbox(bmin, bmax);
            const bool was_inside = ix_point_in_box_oriented(pp->last_pos, region->last_pos, region->orient, bmin, bmax);
            const bool inside = ix_point_in_box_oriented(pp->pos, region->pos, region->orient, bmin, bmax);
            if (region->flags.ambush_is_triggered == 0 && inside && !was_inside)
            {
                object_enter_trigger_region_event event;
                event.object_handle = pp->get_handle();
                event.trigger_handle = region->get_handle();
                gem_post_event(event);
                if (fl_rand_chance(region->get_ambush_chance()))
                {
                    region->trigger_ambush();
                }
            }
            else
            {
                if (region->flags.ambush_is_triggered != 0 && region->squad_update_timer.elapsed())
                {
                    region->find_skirmish_squads();
                    region->update_squads();
                    region->check_abort_conditions();
                    region->squad_update_timer.set_rand(SQUAD_UPDATE_MIN_MS, SQUAD_UPDATE_MAX_MS);
                }
                if (was_inside && !inside)
                {
                    object_exit_trigger_region_event event;
                    event.object_handle = pp->get_handle();
                    event.trigger_handle = region->get_handle();
                    gem_post_event(event);
                }
            }
        }
        region->last_pos = region->pos;
    }
}
