// Original: volition/rfg/code/multi/multi_game/modes/multi_game_mode_siege.cpp
//
// The Siege row of the mode table: the defenders hold the level's siege targets (buildings with a Siege marker, 12 at
// most), unlocked group by group in priority order; the attackers score each target's destruction (a target is
// destroyed at the options' percent of its building, 100 points), plus a time bonus (a point per 6 s left) when the
// round ends early. The teams swap sides every round. As the attackers break a priority group, the defenders' player
// starts of those targets go to the attackers and the next group unlocks (the locked buildings are invulnerable).
// The server decides (PT_MODE_SPECIFIC: 0 a target destroyed, 1 the scores, 2 the time bonus).
// Experience events without a weapon pass 0xFF as the weapon id, as rfg.exe does.
// Part of the multi unity TU (rfg_multi_unity.obj, every multi/ .cpp in path order): the file statics are shared
// by the whole tree, and a shared one is placed in its first user (tools/hdrgen/specs/mp0_stubs.py).

#include "volition/rfg/code/multi/multi_game/modes/multi_game_mode_siege.h"
#include "volition/rfg/code/multi/multi_game/modes/multi_game_mode_common.h"
#include "volition/rfg/code/multi/multi_game/multi_game.h"
#include "volition/rfg/code/multi/multi_game/multi_game_announcer.h"
#include "volition/rfg/code/multi/multi_game/multi_game_object_marker.h"
#include "volition/rfg/code/multi/multi_game/multi_game_render.h"
#include "volition/rfg/code/multi/multi_game/multi_game_settings.h"
#include "volition/rfg/code/multi/multi_game/multi_game_stats.h"
#include "volition/rfg/code/multi/multi_game/multi_game_timer.h"
#include "volition/rfg/code/multi/multi_game/multi_game_xp.h"
#include "volition/rfg/code/multi/multi_game/multi_gameplay_mode.h"
#include "volition/rfg/code/multi/multi_core/include/multi.h"
#include "volition/rfg/code/multi/multi_core/include/multi_core_io.h"
#include "volition/rfg/code/multi/multi_core/include/multi_core_objects.h"
#include "volition/rfg/code/multi/multi_core/include/multi_core_packet_types.h"
#include "volition/rfg/code/multi/multi_core/include/multi_pool.inl"
#include "volition/rfg/code/multi/multi_core/include/multi_session.h"
#include "volition/audio_lib/code/audiolib_api.h"
#include "volition/rfg/code/camera/rfg_camera.h"
#include "volition/rfg/code/destruction/rfg_mover.h"
#include "volition/rfg/code/event_logging/definitions_events/events.h"
#include "volition/rfg/code/event_logging/gamelog.h"
#include "volition/rfg/code/gamewide/game_render.h"
#include "volition/rfg/code/gamewide/gameaudio.h"
#include "volition/rfg/code/hud/hud_icon.h"
#include "volition/rfg/code/hud/hud_message.h"
#include "volition/rfg/code/interface/localization/localization_shared.h"
#include "volition/rfg/code/interface/ui_document_hud_multiplayer.h"
#include "volition/rfg/code/interface/ui_document_manager.h"
#include "volition/rfg/code/level/world.h"
#include "volition/rfg/code/objects/humans/human_teams.h"
#include "volition/rfg/code/objects/humans/player.h"
#include "volition/rfg/code/objects/object_player_start.h"
#include "volition/rfg/code/rfg.h"
#include "volition/render_lib/code/rl_outline_compositor_defines.h"
#include "volition/render_lib/code/rl_renderable_instance.h"
#include "volition/render_lib/code/rl_scene.h"
#include "volition/interface_lib/code/vint_data_group.h"
#include "volition/interface_lib/code/vint_data_item.h"
#include "volition/interface_lib/code/vint_external.h"
#include "volition/interface_lib/code/vint_variant.h"
#include "volition/vlib/code/math/bbox.h"
#include "volition/vlib/code/math/vector3.h"
#include "volition/vlib/code/os/frametime.h"
#include "volition/vlib/code/time/timestamp.h"
#include "volition/vlib/code/util/color.h"
#include "volition/vlib/code/util/string_comp.h"
#include "volition/vlib/code/util/string_util.h"
#include "volition/vlib/code/util/unicode_tools.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>
#include <wchar.h>

// ---- file-static globals
// The fraction of a target's building that destroys it (the options' percent, set at level start).
static float Mp_siege_percent_destruction_required = 0.8f; // 0x0167DEC4
// The unlocked priority group.
static int Current_priority = -1; // 0x0167DEC8
static human_teams Attacking_team = HUMAN_TEAM_NONE; // 0x0167DECC
static human_teams Defending_team = HUMAN_TEAM_NONE; // 0x0167DED0
// How often the server logs the destruction.
static int Logging_snapshot_time_interval_ms_siege = 15000; // 0x0167DED4
static int Target_destroyed_play_id_handle = -1; // 0x0167DED8
// multi_gameplay_mode_names_multi_game Multi_gameplay_mode_names[8] (0x0167E4A8) is used here too; placed in multi_core_platform.cpp.
// float Mp_glow_multiplier (0x0167F2A8) is used here too; placed in multi_game_mode_demolition.cpp.
// dynamic_uint_hash_table<remote_object*> Game_handle_to_rop_hash (0x01691B40) is used here too; placed in multi_core_delta_compression.cpp.
// dynamic_uint_hash_table<network_game_handle*> Reserved_game_to_network_hash (0x01691C18) is used here too; placed in multi_core_objects.cpp.
// The outline of the locked targets.
static color SIEGE_TARGET_LOCKED_OUTLINE_COLOR(0x0A, 0x0A, 0x0A, 0xFF); // 0x01691DD8
static multi_pool<siege_target,12> Siege_target_pool; // 0x01691DE0
// The server's score sync (every 500 ms).
static timestamp Siege_score_sync_timer; // 0x01691E20, initial data ffffffff
static timestamp_realtime Logging_snapshot_timer_siege; // 0x01691E24, initial data ffffffff
// color Mp_team_colors[3][2] (0x016932A8) is used here too; placed in multi_game_mode_bagman.cpp.
// multi_timer Timer_time (0x01693A64) is used here too; placed in multi_core_platform.cpp.
// The targets per priority group. Original: a fifth group writes past the array.
static int Num_targets_on_priority[4]; // 0x02FE5AE8
static vint_data_item* All_targets_data_items[12]; // 0x02FE6D50
static vint_data_group* Current_priority_targets_data_group; // 0x02FE80FC
// short Bandwidth_buckets[243] (0x02FE8108) is used here too; placed in multi.cpp.
// multi_team_stats Mp_team_stats[2] (0x02FE82F0) is used here too; placed in multi_game_mode_bagman.cpp.
// The targets by priority, then name (null past Num_siege_targets).
static siege_target* Siege_targets[12]; // 0x02FE83A0
static vint_data_item* Current_priority_targets_data_items[12]; // 0x02FE8E10
static vint_data_group* All_targets_data_group; // 0x02FEB394
// bool Server (0x02FEB590) is used here too; placed in multi_core_session_generic.cpp.
static int Num_siege_targets; // 0x02FEB64C
// multi_player_stats* Mp_player_stats (0x02FEB720) is used here too; placed in multi_game_mode_bagman.cpp.
// multi_xp_event Multi_xp_events[84] (0x0300D240) is used here too; placed in multi_game_mode_bagman.cpp.

static const int Max_siege_targets = 12;
// The PT_MODE_SPECIFIC subtypes (the byte after the header).
static const unsigned char Siege_target_destroyed = 0;
static const unsigned char Siege_scores = 1;
static const unsigned char Siege_time_bonus = 2;
// A destroyed target is worth 100 points (the percents); the time bonus is a point per 6 s left.
static const int Siege_target_points = 100;
static const int Siege_time_bonus_ms_per_point = 6000;
// The server syncs the scores every 500 ms.
static const int Siege_score_sync_ms = 500;
// The priority sort's "no group yet" (rfg.exe's INT_MIN + 1).
static const int Siege_no_priority = -0x7FFFFFFF;

// The PT_MODE_SPECIFIC packets of this file and the scores they carry are written with the port helpers of
// multi_join.cpp (write_u32_be, write_u16_be, packet_header_size, packet_set_size).

// ---- internal functions
// Port helper: the rfg mover a target's marker sits on (null if none), as rfg.exe inlines it.
static rfg_mover* target_building(const siege_target* target)
{
    const unsigned int handle = target->marker->get_parent();
    if (handle == 0xFFFFFFFFu)
    {
        return nullptr;
    }
    object* objp = World.object_from_handle(handle);
    if (objp == nullptr || objp->get_obj_type() != OT_MOVER || objp->get_sub_type() != OT_SUB_MOVER_RFG)
    {
        return nullptr;
    }
    return (rfg_mover*)objp;
}

// 0x00977BB0 (:155): the target's player starts of previous_team (all with HUMAN_TEAM_NONE) belong to new_team.
// LTCG: target_index in EAX, previous_team in EDX, new_team in ESI.
static void swap_target_spawn_points(int target_index, human_teams previous_team, human_teams new_team)
{
    if ((unsigned int)target_index >= (unsigned int)Max_siege_targets)
    {
        return;
    }
    const multi_object_marker* marker = Siege_targets[target_index]->marker;
    object* child = marker->child_ptr;
    do
    {
        if (child == nullptr)
        {
            return;
        }
        if (child->get_obj_type() == OT_PLAYER_START)
        {
            player_start* pstart = (player_start*)child;
            if (pstart->team == previous_team || previous_team == HUMAN_TEAM_NONE)
            {
                pstart->team = new_team;
            }
        }
        child = child->child_next;
    } while (child != marker->child_ptr);
}

// 0x00977C00 (:180): the target's player starts of team (all with HUMAN_TEAM_NONE) are enabled or disabled; whether
// it has any. LTCG: target_index in ECX, team in ESI.
static bool enable_target_spawn_points(int target_index, human_teams team, bool enabled)
{
    if ((unsigned int)target_index >= (unsigned int)Max_siege_targets)
    {
        return false;
    }
    const multi_object_marker* marker = Siege_targets[target_index]->marker;
    object* child = marker->child_ptr;
    if (child == nullptr)
    {
        return false;
    }
    bool found = false;
    do
    {
        if (child->get_obj_type() == OT_PLAYER_START)
        {
            player_start* pstart = (player_start*)child;
            if (pstart->team == team || team == HUMAN_TEAM_NONE)
            {
                found = true;
                pstart->enabled = enabled;
            }
        }
        child = child->child_next;
    } while (child != marker->child_ptr && child != nullptr);
    return found;
}

// 0x0094DDB0 (:203): the target's item of the current targets' data group (priority, name, percent destroyed,
// distance, angle, team, locked, the number, the group's target count). LTCG: target_index in ESI.
static void add_siege_target_data_item(int target_index)
{
    if (target_index < 0 || target_index >= Num_siege_targets || Current_priority_targets_data_group == nullptr || Current_priority_targets_data_items[target_index] != nullptr)
    {
        return;
    }
    static const vint_variant_type target_template[9] = {VINT_PROP_TYPE_INT, VINT_PROP_TYPE_STRING, VINT_PROP_TYPE_FLOAT, VINT_PROP_TYPE_FLOAT, VINT_PROP_TYPE_FLOAT,
                                                         VINT_PROP_TYPE_INT, VINT_PROP_TYPE_BOOL, VINT_PROP_TYPE_INT, VINT_PROP_TYPE_INT};
    vint_data_item* item = Current_priority_targets_data_group->add_data_item(nullptr);
    Current_priority_targets_data_items[target_index] = item;
    if (item != nullptr)
    {
        item->set_template(target_template, 9);
    }
}

// 0x0094DE50 (:229). LTCG: target_index in ESI.
static void remove_siege_target_data_item(int target_index)
{
    if (target_index < 0 || target_index >= Num_siege_targets || Current_priority_targets_data_group == nullptr || Current_priority_targets_data_items[target_index] == nullptr)
    {
        return;
    }
    Current_priority_targets_data_group->remove_data_item(Current_priority_targets_data_items[target_index]->get_handle()); // m_handle, inlined
    Current_priority_targets_data_items[target_index] = nullptr;
}

// 0x00994BE0 (:241): the target's icon in its team's color: an unlocked target has the attack (attackers) or defend
// icon with its name, a locked one the lock (fading at 150 m)
static bool siege_hud_icon_callback(vector& center_pos, hud_icon* icon)
{
    const siege_target* target = (const siege_target*)icon->parameter;
    multi_game_render_get_team_color(icon->icon_color, MTCT_ICON, target->marker);
    local_player_info info;
    player_get_local_player_info(&info);
    if (target->marker->priority <= Current_priority)
    {
        icon->bitmap_id = hud_icon_get_bitmap_id(info.team != Attacking_team ? HI_DEFEND : HI_ATTACK);
        icon->snap_to_edge = true;
        wcsncpy(icon->text[0], target->name, 32);
        icon->text[0][31] = L'\0';
        icon->text_color = icon->icon_color;
        // Magic numbers: the icon and its name's scales.
        icon->text_scale = 0.75f;
        icon->icon_scale = 1.0f;
        icon->text_font = Body_font;
        icon->fade_distance = 0.0f;
        return true;
    }
    icon->bitmap_id = hud_icon_get_bitmap_id(HI_LOCK);
    // Magic numbers: the lock fades out at 150 m and is twice as big.
    icon->fade_distance = 150.0f;
    icon->icon_scale = 2.0f;
    return true;
}

// 0x00977C60 (:277): the target unlocks: its HUD item; the server makes its building vulnerable (a target without a
// building stops here); the defenders' player starts there are enabled. No caller in rfg.exe outside its inlined
// copies.
static void target_added(int target_index)
{
    if ((unsigned int)target_index >= (unsigned int)Max_siege_targets)
    {
        return;
    }
    add_siege_target_data_item(target_index);
    if (Multiplayer && Server)
    {
        rfg_mover* building = target_building(Siege_targets[target_index]);
        if (building == nullptr)
        {
            return;
        }
        if (building->flags.invulnerable)
        {
            building->toggle_invulnerable(false);
        }
    }
    enable_target_spawn_points(target_index, Defending_team, true);
}

// 0x00977CE0 (:305): the target's icon and HUD item go; the defenders' player starts there are disabled. LTCG:
// target_index in EDI. No caller in rfg.exe outside its inlined copies.
static void target_removed(int target_index)
{
    if ((unsigned int)target_index >= (unsigned int)Max_siege_targets)
    {
        return;
    }
    hud_icon_remove(Siege_targets[target_index]->marker->get_handle());
    remove_siege_target_data_item(target_index); // inlined
    enable_target_spawn_points(target_index, Defending_team, false);
}

// 0x00977D50 (:321): every target of the current priority unlocks (target_added inlined; a target without a building
// is skipped)
static void find_new_valid_targets()
{
    for (int i = 0; i < Num_siege_targets; i++)
    {
        siege_target* target = Siege_targets[i];
        if (target->marker->priority != Current_priority || (unsigned int)i >= (unsigned int)Max_siege_targets)
        {
            continue;
        }
        add_siege_target_data_item(i);
        if (Multiplayer && Server)
        {
            rfg_mover* building = target_building(target);
            if (building == nullptr)
            {
                continue;
            }
            if (building->flags.invulnerable)
            {
                building->toggle_invulnerable(false);
            }
        }
        enable_target_spawn_points(i, Defending_team, true);
    }
}

// 0x0094DE90 (:330): the target of a marker (-1 if none). LTCG: obj in EDX.
static int find_target_by_marker(unsigned int obj)
{
    for (int i = 0; i < Num_siege_targets; i++)
    {
        if (Siege_targets[i]->marker->get_handle() == obj)
        {
            return i;
        }
    }
    return -1;
}

// 0x0094DEC0 (:342): qsort: by priority, then name
static int target_sort_compare(const void* elem1, const void* elem2)
{
    const siege_target* target1 = *(const siege_target* const*)elem1;
    const siege_target* target2 = *(const siege_target* const*)elem2;
    if (target1->marker->priority < target2->marker->priority)
    {
        return -1;
    }
    if (target2->marker->priority < target1->marker->priority)
    {
        return 1;
    }
    return _v_stricmp(target1->name, target2->name);
}

// 0x009BB050 (:358): the server sends the team scores
static void send_score_sync()
{
    unsigned char packet[1200];
    packet[0] = PT_MODE_SPECIFIC;
    unsigned short packet_size = packet_header_size(PT_MODE_SPECIFIC);
    packet[packet_size++] = Siege_scores;
    multi_game_stats_write_all_team_scores(packet, packet_size);
    packet_set_size(packet, packet_size);
    multi_io_send_to_all_reliable(Game_session, packet, packet_size);
}

// 0x00977E00 (:372): a client reads the team scores. LTCG: packet_data in ECX, packet_offset in EAX. No caller in
// rfg.exe outside its inlined copy.
static void process_score_sync(unsigned char* packet_data, int& packet_offset)
{
    if (Multiplayer && !Server)
    {
        multi_game_stats_read_all_team_scores(packet_data, packet_offset);
    }
}

// 0x009EE720 (:392): a target is destroyed (once): the announcer (the last target too); the server gives the attackers
// the team destroy experience, the enemy destroyer the destroy experience and the enemies of the target near the
// building the assist. Its icon, HUD item and defenders' player starts go. The last target of its group unlocks the
// next group: the attackers' first player starts are disabled (group 0 only), the attackers' player starts of the
// broken group (the defenders' when it has none) are enabled, the earlier groups' disabled. The server tells
// everyone; the HUD and the cue.
static void siege_mode_event_target_destroyed(int target_index)
{
    siege_target* target = Siege_targets[target_index];
    if (target->marker->destroyed)
    {
        return;
    }
    target->marker->destroyed = true;
    local_player_info info;
    player_get_local_player_info(&info);
    // The enemy lines are 2 after the ally lines: the attackers hear that the enemy's target went.
    const bool attacker = human_teams_get_disposition(info.team, Attacking_team) >= TEAM_DISPOSITION_FRIENDLY;
    multi_game_announcer_play_line(attacker ? MAL_SGE_ENEMY_TARGET_DESTROY : MAL_SGE_ALLY_TARGET_DESTROY, 0xFFFFFFFF);
    int num_destroyed = 0;
    for (int i = 0; i < Num_siege_targets; i++)
    {
        if (Siege_targets[i]->marker->destroyed)
        {
            num_destroyed++;
        }
    }
    // (The new target counts twice: one target left.)
    if (num_destroyed + 1 == Num_siege_targets)
    {
        const bool near_attacker = human_teams_get_disposition(info.team, Attacking_team) >= TEAM_DISPOSITION_FRIENDLY;
        multi_game_announcer_play_line(near_attacker ? MAL_SGE_ENEMY_TARGET_WIN_NEAR : MAL_SGE_ALLY_TARGET_WIN_NEAR, 0xFFFFFFFF);
    }
    multi_game_xp_trigger_event(Attacking_team, MXE_DESTROY_SIEGE_TARGET_TEAM, 1, false); // inlined
    if (Multiplayer && Server)
    {
        rfg_mover* building = target_building(target);
        player* destroyer = multi_game_last_player_to_damage_rfg_mover(building);
        if (destroyer != nullptr && human_teams_get_disposition(destroyer->current_team, target->marker->team) == TEAM_DISPOSITION_HOSTILE)
        {
            multi_game_xp_trigger_event(destroyer->mp_data, MXE_DESTROY_SIEGE_TARGET, 1, 0xFF, -1, false); // inlined
            vector bmin;
            vector bmax;
            building->get_bbox(bmin, bmax);
            const float assist_dist = multi_game_xp_get_event_parameter(MXE_DESTROY_ASSIST_SIEGE_TARGET); // inlined
            const float assist_dist_squared = assist_dist * assist_dist;
            const int count = World.get_num_objects<player>();
            for (int i = 0; i < count; i++)
            {
                player* pp = World.get_object<player>(i);
                if (pp != destroyer && human_teams_get_disposition(target->marker->team, pp->current_team) == TEAM_DISPOSITION_HOSTILE &&
                    bbox_find_distance_squared(bmin, bmax, pp->pos) <= assist_dist_squared)
                {
                    multi_game_xp_trigger_event(pp->mp_data, MXE_DESTROY_ASSIST_SIEGE_TARGET, 1, 0xFF, -1, false); // inlined
                }
            }
        }
    }
    target_removed(target_index); // inlined
    // The lowest priority still standing; another target of the destroyed one's group keeps the group.
    int next_priority = 0x7FFFFFFF;
    bool group_left = false;
    for (int i = 0; i < Num_siege_targets; i++)
    {
        const multi_object_marker* marker = Siege_targets[i]->marker;
        if (marker->destroyed)
        {
            continue;
        }
        if (marker->priority == target->marker->priority)
        {
            group_left = true;
            break;
        }
        if (marker->priority < next_priority)
        {
            next_priority = marker->priority;
        }
    }
    if (!group_left)
    {
        if (Current_priority == 0)
        {
            const int num_starts = World.get_num_objects<player_start>();
            for (int i = 0; i < num_starts; i++)
            {
                player_start* pstart = World.get_object<player_start>(i);
                if (pstart->team == Attacking_team)
                {
                    pstart->enabled = false;
                }
            }
        }
        bool attackers_have_starts = false;
        for (int i = 0; i < Num_siege_targets; i++)
        {
            const int priority = Siege_targets[i]->marker->priority;
            if (priority == Current_priority)
            {
                if (enable_target_spawn_points(i, Attacking_team, true)) // inlined
                {
                    attackers_have_starts = true;
                }
            }
            else if (priority < Current_priority)
            {
                enable_target_spawn_points(i, Attacking_team, false); // inlined
            }
        }
        if (!attackers_have_starts)
        {
            for (int i = 0; i < Num_siege_targets; i++)
            {
                if (Siege_targets[i]->marker->priority == Current_priority)
                {
                    swap_target_spawn_points(i, Defending_team, Attacking_team); // inlined
                    enable_target_spawn_points(i, Attacking_team, true); // inlined
                }
            }
        }
        Current_priority = next_priority;
        audiolib_cue_play(Ga_cue_ids.multiplayer_target_unlocked, nullptr, nullptr);
        find_new_valid_targets();
    }
    if (Multiplayer && Server)
    {
        unsigned char packet[1200];
        packet[0] = PT_MODE_SPECIFIC;
        unsigned short packet_size = packet_header_size(PT_MODE_SPECIFIC);
        packet[packet_size++] = Siege_target_destroyed;
        write_u16_be(packet, packet_size, multi_core_network_handle_from_game_handle(target->marker->get_handle(), false)); // inlined
        packet_set_size(packet, packet_size);
        multi_io_send_to_all_reliable(multi_session_get_by_type(ST_GAME), packet, packet_size);
    }
    hud_ui_multiplayer_trigger_objective(target->name, localization_get_display_string_req("HM_DESTROYED", nullptr), Attacking_team);
    if (audiolib_is_playing(Target_destroyed_play_id_handle, true))
    {
        audiolib_play_stop(Target_destroyed_play_id_handle);
    }
    Target_destroyed_play_id_handle = audiolib_cue_play(Ga_cue_ids.multiplayer_target_destroyed, nullptr, nullptr);
}

// 0x009BB0E0 (:539): server, every 500 ms (or at the round's end): the attackers' score is 100 per destroyed target
// plus each standing target's percent, plus the time bonus at the round's end (returned); the scores are sent
static int siege_calculate_score(bool round_end)
{
    int time_bonus = 0;
    if (!Multiplayer || !Server || (!round_end && !Siege_score_sync_timer.elapsed()))
    {
        return time_bonus;
    }
    int score = 0;
    for (int i = 0; i < Num_siege_targets; i++)
    {
        score += Siege_targets[i]->marker->destroyed ? Siege_target_points : (int)Siege_targets[i]->percent_destroyed;
    }
    if (round_end)
    {
        const int ms_left = Timer_time.time_until();
        time_bonus = (ms_left < 1 ? 0 : ms_left) / Siege_time_bonus_ms_per_point;
        score += time_bonus;
    }
    if (Attacking_team >= HUMAN_TEAM_GUERILLA && Attacking_team <= HUMAN_TEAM_EDF)
    {
        multi_game_stats_alter_team_score(Attacking_team, score - (int)Mp_team_stats[Attacking_team].round_score);
    }
    send_score_sync();
    Siege_score_sync_timer.set(Siege_score_sync_ms);
    return time_bonus;
}

// 0x00977E20 (:571): qsort: the target whose building is nearer to the local player first (0 without buildings)
static int target_distance_compare(const void* p1, const void* p2)
{
    const siege_target* target1 = *(const siege_target* const*)p1;
    const siege_target* target2 = *(const siege_target* const*)p2;
    local_player_info info;
    player_get_local_player_info(&info);
    rfg_mover* building1 = target_building(target1);
    rfg_mover* building2 = target_building(target2);
    if (building1 == nullptr || building2 == nullptr)
    {
        return 0;
    }
    vector bmin1;
    vector bmax1;
    vector bmin2;
    vector bmax2;
    building1->get_bbox(bmin1, bmax1);
    building2->get_bbox(bmin2, bmax2);
    const float dist1 = bbox_find_distance_squared(bmin1, bmax1, *info.pos);
    const float dist2 = bbox_find_distance_squared(bmin2, bmax2, *info.pos);
    if (dist1 < dist2)
    {
        return -1;
    }
    if (dist2 < dist1)
    {
        return 1;
    }
    return 0;
}

// 0x0094DF00 (:601): the time bonus message (4 s)
static void siege_display_time_bonus(int time_bonus)
{
    wchar_t buffer[128];
    _snprintf_safe(buffer, 128, localization_get_display_string_req("HM_TIME_BONUS", nullptr), time_bonus);
    hud_message_add(buffer, 4.0f, nullptr, nullptr, nullptr);
}

// 0x009BB1C0 (:614): a joining client hears of each fully destroyed target
void siege_mode_round_begin_jip(multi_connection* cp)
{
    for (int i = 0; i < Num_siege_targets; i++)
    {
        const siege_target* target = Siege_targets[i];
        if (target == nullptr || target->percent_destroyed < 100.0f)
        {
            continue;
        }
        unsigned char packet[1200];
        packet[0] = PT_MODE_SPECIFIC;
        unsigned short packet_size = packet_header_size(PT_MODE_SPECIFIC);
        packet[packet_size++] = Siege_target_destroyed;
        write_u16_be(packet, packet_size, multi_core_network_handle_from_game_handle(target->marker->get_handle(), false)); // inlined
        packet_set_size(packet, packet_size);
        multi_io_send_reliable(cp, packet, packet_size, nullptr, nullptr, nullptr);
    }
}

// 0x009BB330 (:634)
void siege_mode_memory_init(mempool_base& pool)
{
    Siege_target_pool.init(pool);
}

// 0x009DBC10 (:639)
void siege_mode_memory_shutdown()
{
    Siege_target_pool.clear(); // inlined
}

// 0x0094DF80 (:646): the EDF attacks first (level init swaps the sides before the first round)
void siege_mode_enter()
{
    Attacking_team = HUMAN_TEAM_EDF;
    Defending_team = HUMAN_TEAM_GUERILLA;
}

// 0x0094DFA0 (:653)
void siege_mode_exit()
{
}

// 0x009AB6C0 (:659): the sides swap (an even round swaps the player starts too). The Siege markers (from the last
// priority) become the defenders' targets with their icons, named after their building's localized name; their
// player starts are disabled. Sorted by priority and name, the priorities become group numbers from 0 with their
// target counts; group 0 unlocks. Original: a marker without a building keeps its pool slot and icon and is
// overwritten by the next target.
void siege_mode_level_init_post()
{
    const human_teams old_attacking = Attacking_team;
    const human_teams old_defending = Defending_team;
    Attacking_team = old_defending;
    Defending_team = old_attacking;
    if ((Multi_match_info.current_round & 1) == 0)
    {
        const int num_starts = World.get_num_objects<player_start>();
        for (int i = 0; i < num_starts; i++)
        {
            player_start* pstart = World.get_object<player_start>(i);
            if (pstart->team == old_defending)
            {
                pstart->team = old_attacking;
            }
            else if (pstart->team == old_attacking)
            {
                pstart->team = old_defending;
            }
        }
    }
    Num_siege_targets = 0;
    for (int i = World.get_num_objects<multi_object_marker>() - 1; i >= 0; i--)
    {
        multi_object_marker* marker = Multi_markers_prioritized[i];
        if (marker->get_marker_type() != MMT_SIEGE_TARGET)
        {
            continue;
        }
        siege_target* target = Siege_target_pool.alloc(0);
        Siege_targets[Num_siege_targets] = target;
        target->marker = marker;
        target->percent_destroyed = 0.0f;
        target->percent_destroyed_accumulator = 0.0f;
        target->percent_repaired_accumulator = 0.0f;
        marker->destroyed = false;
        target->percent_destroyed_last_frame = 0.0f;
        target->marker->team = Defending_team;
        if ((unsigned int)Num_siege_targets < (unsigned int)Max_siege_targets)
        {
            // Every player start of the target is disabled (enable_target_spawn_points with any team, inlined).
            object* child = target->marker->child_ptr;
            do
            {
                if (child == nullptr)
                {
                    break;
                }
                if (child->get_obj_type() == OT_PLAYER_START)
                {
                    ((player_start*)child)->enabled = false;
                }
                child = child->child_next;
            } while (child != target->marker->child_ptr);
        }
        hud_icon* icon = hud_icon_add(target->marker->get_handle());
        if (icon != nullptr)
        {
            icon->callback = siege_hud_icon_callback;
            icon->parameter = target;
        }
        rfg_mover* building = target_building(target);
        if (building != nullptr)
        {
            target->name[0] = L'\0';
            wcsncpy(target->name, localization_get_display_string_req(building->get_name(), nullptr), 47);
            Num_siege_targets++;
            target->name[46] = L'\0';
        }
    }
    if (Num_siege_targets == 0)
    {
        return;
    }
    qsort(Siege_targets, Num_siege_targets, sizeof(siege_target*), target_sort_compare);
    int last_priority = Siege_no_priority;
    int group = 0;
    for (int i = 0; i < 4; i++)
    {
        Num_targets_on_priority[i] = 0;
    }
    for (int i = 0; i < Num_siege_targets; i++)
    {
        multi_object_marker* marker = Siege_targets[i]->marker;
        if (last_priority == Siege_no_priority)
        {
            last_priority = marker->priority;
            marker->priority = 0;
        }
        else if (last_priority == marker->priority)
        {
            marker->priority = group;
        }
        else
        {
            last_priority = marker->priority;
            group++;
            marker->priority = group;
        }
        Num_targets_on_priority[marker->priority]++;
    }
    for (int i = Num_siege_targets; i < Max_siege_targets; i++)
    {
        Siege_targets[i] = nullptr;
    }
    Current_priority = 0;
    find_new_valid_targets();
    Mp_siege_percent_destruction_required = (float)((double)Mp_server_options.mode_options.siege.percent_destruction_required / 100.0);
    if (Multiplayer && Server)
    {
        Logging_snapshot_timer_siege.set(Logging_snapshot_time_interval_ms_siege);
    }
}

// 0x009DBC60 (:774)
void siege_mode_level_shutdown()
{
    Siege_target_pool.clear(); // inlined
}

// 0x009EF260 (:781): per standing target with a building: an unlocked one's percent destroyed (of the required part);
// the server destroys it past the required part (and logs it) and accumulates the destruction and repair; every
// target's distance to the local player. The score; the server's destruction log every
// Logging_snapshot_time_interval_ms_siege.
void siege_mode_process()
{
    local_player_info info;
    player_get_local_player_info(&info);
    for (int i = 0; i < Num_siege_targets; i++)
    {
        siege_target* target = Siege_targets[i];
        if (target->marker->destroyed)
        {
            continue;
        }
        rfg_mover* building = target_building(target);
        if (building == nullptr)
        {
            continue;
        }
        if (!target->marker->destroyed && target->marker->priority == Current_priority)
        {
            target->percent_destroyed = (float)((double)(building->percent_destroyed() / Mp_siege_percent_destruction_required) * 100.0);
            if (Multiplayer && Server)
            {
                if (building->percent_destroyed() > Mp_siege_percent_destruction_required)
                {
                    siege_mode_event_target_destroyed(i);
                    const vector pos = building->pos;
                    const int time = gamelog_get_logging_time();
                    // Original: the low 16 bits of the timer's milliseconds, not seconds.
                    log_event_siege_target_destroyed(gamelog_get_player_checksum(), time, (short)Timer_time.time_until(), (char)i, pos);
                }
                if (building->percent_destroyed() != target->percent_destroyed_last_frame)
                {
                    const float delta = building->percent_destroyed() - target->percent_destroyed_last_frame;
                    if (delta > 0.0f)
                    {
                        target->percent_destroyed_accumulator = (float)((double)delta + (double)target->percent_destroyed_accumulator);
                    }
                    else
                    {
                        target->percent_repaired_accumulator = (float)((double)target->percent_repaired_accumulator - (double)delta);
                    }
                }
            }
            target->percent_destroyed_last_frame = building->percent_destroyed();
        }
        vector bmin;
        vector bmax;
        building->get_bbox(bmin, bmax);
        target->distance_to_local_player = (float)sqrt(bbox_find_distance_squared(bmin, bmax, *info.pos));
    }
    siege_calculate_score(false);
    if (!Logging_snapshot_timer_siege.elapsed())
    {
        return;
    }
    Logging_snapshot_timer_siege.set(Logging_snapshot_time_interval_ms_siege);
    char percent[Max_siege_targets] = {};
    short destruction[Max_siege_targets] = {};
    short repair[Max_siege_targets] = {};
    for (int i = 0; i < Num_siege_targets; i++)
    {
        siege_target* target = Siege_targets[i];
        percent[i] = (char)(int)(float)ceil((double)(float)((double)target->percent_destroyed_last_frame * 100.0));
        destruction[i] = (short)(int)(float)ceil((double)(float)((double)target->percent_destroyed_accumulator * 100.0));
        repair[i] = (short)(int)(float)ceil((double)(float)((double)target->percent_repaired_accumulator * 100.0));
        target->percent_destroyed_accumulator = 0.0f;
        target->percent_repaired_accumulator = 0.0f;
    }
    const short seconds_left = (short)(Timer_time.time_until() / 1000);
    log_event_siege_destruction_snapshot(gamelog_get_player_checksum(), seconds_left, percent[0], percent[1], percent[2], percent[3], percent[4], percent[5], percent[6], percent[7],
                                         percent[8], percent[9], percent[10], percent[11]);
    log_event_siege_destruction_volume(gamelog_get_player_checksum(), seconds_left, destruction[0], repair[0], destruction[1], repair[1], destruction[2], repair[2], destruction[3],
                                       repair[3], destruction[4], repair[4], destruction[5], repair[5], destruction[6], repair[6], destruction[7], repair[7], destruction[8],
                                       repair[8], destruction[9], repair[9], destruction[10], repair[10], destruction[11], repair[11]);
}

// 0x00994CF0 (:887): the outlines: the unlocked standing targets (nearest first) pulse in the first target's team
// glow color (layer 0), the locked ones in the locked color (layer 1). Original: the first target is not checked
// for null (no targets), its marker is compared with the local player, and the pulse multiplies the color again for
// every outlined target.
void siege_mode_handshake()
{
    local_player_info info;
    player_get_local_player_info(&info);
    siege_target* targets[Max_siege_targets];
    int num_targets = 0;
    for (int i = 0; i < Num_siege_targets; i++)
    {
        siege_target* target = Siege_targets[i];
        rfg_mover* building = target_building(target);
        if (building == nullptr)
        {
            continue;
        }
        building->disable_object_outline(); // inlined
        if (!target->marker->destroyed && target->marker->priority == Current_priority)
        {
            targets[num_targets++] = target;
        }
    }
    qsort(targets, num_targets, sizeof(siege_target*), target_distance_compare);
    rl_scene* scene = game_render_get_main_scene();
    if (scene == nullptr)
    {
        return;
    }
    const multi_object_marker* marker = Siege_targets[0]->marker;
    rl_outline_state state;
    state.layer[0] = RLOL_USE_COLOR0;
    state.layer[1] = RLOL_USE_COLOR1;
    state.layer[2] = RLOL_DISABLED;
    state.layer[3] = RLOL_DISABLED;
    unsigned char red;
    unsigned char green;
    unsigned char blue;
    if (marker == nullptr)
    {
        red = color::WHITE.red;
        green = color::WHITE.green;
        blue = color::WHITE.blue;
    }
    else
    {
        local_player_info viewer;
        player_get_local_player_info(&viewer);
        const bool ally = human_teams_get_disposition(viewer.team, human_teams_get_team(marker)) == TEAM_DISPOSITION_ALLIED || (const object*)player_get_local_player() == marker;
        const color team_color = Mp_team_colors[1][!ally];
        float glow = Multi_match_info.use_default_time_of_day ? Mp_server_options.get_level()->glow_multiplier : Mp_server_options.get_level()->alt_glow_multiplier;
        glow = (float)((double)glow * (double)Mp_glow_multiplier);
        blue = (unsigned char)(int)((double)team_color.blue * (double)glow);
        green = (unsigned char)(int)((double)team_color.green * (double)glow);
        red = (unsigned char)(int)((double)glow * (double)team_color.red);
    }
    state.colors[0].set((float)(red / 255.0), (float)(green / 255.0), (float)(blue / 255.0), 1.0f);
    state.colors[1].set((float)(SIEGE_TARGET_LOCKED_OUTLINE_COLOR.red / 255.0), (float)(SIEGE_TARGET_LOCKED_OUTLINE_COLOR.green / 255.0),
                        (float)(SIEGE_TARGET_LOCKED_OUTLINE_COLOR.blue / 255.0), 1.0f);
    for (int i = 0; i < num_targets; i++)
    {
        rfg_mover* building = target_building(targets[i]);
        if (building == nullptr || building->get_render_model_instance() == nullptr)
        {
            continue;
        }
        // A 0.5..1.5 pulse with a period of 1000 * pi ms (Frametime_total_milliseconds as unsigned).
        const float pulse = (float)sin((float)((double)(unsigned int)Frametime_total_milliseconds / 500.0)) * 0.5f + 1.0f;
        state.colors[0].set(state.colors[0].red * pulse, state.colors[0].green * pulse, state.colors[0].blue * pulse, pulse * state.colors[0].alpha);
        building->enable_object_outline(Current_priority < targets[i]->marker->priority ? 1 : 0); // inlined
    }
    scene->set_outline_state(state); // inlined
}

// 0x009EF770 (:956): an enemy killed near one of the killer's standing targets (to the building's box; the protect
// distance; the killer or the victim): the protect experience
void siege_mode_player_died(player* victim, player* killer, damaged_by_types damage_type, const weapon_info* w_info)
{
    if (victim == killer || human_teams_get_disposition(victim, killer) != TEAM_DISPOSITION_HOSTILE)
    {
        return;
    }
    for (int i = 0; i < Num_siege_targets; i++)
    {
        const siege_target* target = Siege_targets[i];
        if (target->marker->destroyed || human_teams_get_disposition(killer->current_team, target->marker->team) != TEAM_DISPOSITION_ALLIED)
        {
            continue;
        }
        rfg_mover* building = target_building(target);
        if (building == nullptr)
        {
            continue;
        }
        vector bmin;
        vector bmax;
        building->get_bbox(bmin, bmax);
        const float killer_dist_squared = bbox_find_distance_squared(bmin, bmax, killer->pos);
        const float victim_dist_squared = bbox_find_distance_squared(bmin, bmax, victim->pos);
        const float protect_dist = multi_game_xp_get_event_parameter(MXE_PROTECT_SIEGE_TARGET);
        if (killer_dist_squared < protect_dist * protect_dist || victim_dist_squared < protect_dist * protect_dist)
        {
            multi_game_xp_trigger_event(killer->mp_data, MXE_PROTECT_SIEGE_TARGET, 1, 0xFF, -1, false); // inlined
        }
    }
}

// 0x00977F60 (:986): respawning near an unlocked standing target gains up to 20 (100 m, scaled by the level's
// distance multiplier)
float siege_mode_adjust_respawn_weight(const player* pp, const player_start* pstart)
{
    float weight = 0.0f;
    for (int i = 0; i < Num_siege_targets; i++)
    {
        const multi_object_marker* marker = Siege_targets[i]->marker;
        if (marker->destroyed || marker->priority != Current_priority)
        {
            continue;
        }
        float dist = vec_dist(pstart->pos, marker->pos) / Mp_server_options.get_level()->distance_multiplier / 100.0f;
        if (dist - 1.0f >= 0.0f)
        {
            dist = 1.0f;
        }
        weight = dist * -20.0f + 20.0f + weight;
    }
    return weight;
}

// 0x009EFA90 (:1009): PT_MODE_SPECIFIC (packet_data: the subtype byte)
void siege_mode_process_mode_specific(unsigned char* packet_data, const sender& from)
{
    int offset = 1;
    switch (packet_data[0])
    {
    case Siege_target_destroyed:
    {
        const unsigned int handle = multi_core_game_handle_from_network_handle(read_u16_be(packet_data, offset), false);
        const int target_index = find_target_by_marker(handle); // inlined
        if (target_index != -1)
        {
            siege_mode_event_target_destroyed(target_index);
        }
        return;
    }
    case Siege_scores:
        process_score_sync(packet_data, offset); // inlined
        return;
    case Siege_time_bonus:
    {
        const int time_bonus = read_u16_be(packet_data, offset);
        siege_display_time_bonus(time_bonus);
        multi_game_stats_alter_team_score(Attacking_team, time_bonus);
        return;
    }
    default:
        return;
    }
}

// 0x00978060 (:1054): the server makes the locked targets' buildings invulnerable (original: the first locked target
// without a building ends the loop); the score sync starts; the announcer tells each side its role
void siege_mode_round_begin()
{
    if (Multiplayer && Server)
    {
        for (int i = 0; i < Num_siege_targets; i++)
        {
            if (Current_priority >= Siege_targets[i]->marker->priority)
            {
                continue;
            }
            rfg_mover* building = target_building(Siege_targets[i]);
            if (building == nullptr)
            {
                break;
            }
            if (!building->flags.invulnerable)
            {
                building->toggle_invulnerable(false);
            }
        }
    }
    Siege_score_sync_timer.set(Siege_score_sync_ms);
    local_player_info info;
    player_get_local_player_info(&info);
    // The defense line follows the offense line.
    const bool defender = human_teams_get_disposition(info.team, Attacking_team) < TEAM_DISPOSITION_FRIENDLY;
    multi_game_announcer_play_line(defender ? MAL_SGE_INFORM_DEFENSE : MAL_SGE_INFORM_OFFENSE, 0xFFFFFFFF);
}

// 0x009EFB70 (:1082): the defenders get the defend experience for each standing target
void siege_mode_round_over(multi_round_winner winner)
{
    for (int i = 0; i < Num_siege_targets; i++)
    {
        if (!Siege_targets[i]->marker->destroyed)
        {
            multi_game_xp_trigger_event(Defending_team, MXE_DEFEND_SIEGE_TARGET, 1, false); // inlined
        }
    }
}

// 0x00978130 (:1095): the attackers' total score reaches 100 per target (no attackers: over)
bool siege_mode_is_round_over()
{
    if (Attacking_team < HUMAN_TEAM_CIVILIAN)
    {
        return Num_siege_targets * Siege_target_points <= (int)Mp_team_stats[Attacking_team].score;
    }
    return true;
}

// 0x009BB350 (:1113): the final score with the time bonus (the server sends and shows the bonus); after the last
// round the team with the better total score wins (a tie has no winner), before it nobody
multi_round_winner siege_mode_get_round_winner()
{
    multi_round_winner winner;
    winner.player_game_data_handle = 0xFFFFFFFFu;
    const int time_bonus = siege_calculate_score(true);
    if (Multiplayer && Server && time_bonus != 0)
    {
        unsigned char packet[1200];
        packet[0] = PT_MODE_SPECIFIC;
        unsigned short packet_size = packet_header_size(PT_MODE_SPECIFIC);
        packet[packet_size++] = Siege_time_bonus;
        write_u16_be(packet, packet_size, (unsigned short)time_bonus);
        packet_set_size(packet, packet_size);
        multi_io_send_to_all_reliable(Game_session, packet, packet_size);
        siege_display_time_bonus(time_bonus);
    }
    if (!multi_game_mode_is_last_round())
    {
        return winner;
    }
    if (Mp_team_stats[HUMAN_TEAM_EDF].score > Mp_team_stats[HUMAN_TEAM_GUERILLA].score)
    {
        winner.team = HUMAN_TEAM_EDF;
    }
    else if (Mp_team_stats[HUMAN_TEAM_EDF].score != Mp_team_stats[HUMAN_TEAM_GUERILLA].score)
    {
        winner.team = HUMAN_TEAM_GUERILLA;
    }
    return winner;
}

// 0x009EFD30 (:1156): the new server: destroyed targets (original: siege_mode_event_target_destroyed returns at once
// for them) get vulnerable buildings, the locked ones invulnerable buildings
void siege_mode_server_migration_post()
{
    if (!Multiplayer || !Server)
    {
        return;
    }
    for (int i = 0; i < Num_siege_targets; i++)
    {
        rfg_mover* building = target_building(Siege_targets[i]);
        if (Siege_targets[i]->marker->destroyed)
        {
            siege_mode_event_target_destroyed(i);
            if (building != nullptr && building->flags.invulnerable)
            {
                building->toggle_invulnerable(false);
            }
        }
        else if (Current_priority < Siege_targets[i]->marker->priority && building != nullptr && !building->flags.invulnerable)
        {
            building->toggle_invulnerable(false);
        }
    }
}

// 0x009EFE00 (:1182): server: a defender damaging a target or an attacker repairing one gets a tooltip
void siege_mode_building_damage_changed(player* pp, rfg_mover* building, float structural_mass_delta)
{
    if (pp == nullptr || !Multiplayer || !Server)
    {
        return;
    }
    for (int i = 0; i < Num_siege_targets; i++)
    {
        if (Siege_targets[i]->marker->get_parent() != building->get_handle())
        {
            continue;
        }
        if (pp->current_team == Defending_team && structural_mass_delta < 0.0f)
        {
            multi_game_trigger_tooltip(pp, checksum_stri("damaged_friendly_target", 0));
            return;
        }
        if (pp->current_team == Attacking_team && structural_mass_delta > 0.0f)
        {
            multi_game_trigger_tooltip(pp, checksum_stri("repaired_enemy_target", 0));
        }
        return;
    }
}

// 0x0094DFB0 (:1207): the defenders get the options' defense weapons and backpack
void siege_mode_get_default_equipment(const player* pp, multi_game_equipment_loadout* loadout)
{
    if (pp->current_team != Defending_team)
    {
        return;
    }
    const multi_game_mode_options::siege_options& options = Mp_server_options.mode_options.siege;
    if (options.defense_starting_weapon != 0xFFFFFFFFu)
    {
        loadout->replace_starting_weapon = true;
        loadout->starting_weapon = options.defense_starting_weapon;
    }
    if (options.defense_third_weapon != 0xFFFFFFFFu)
    {
        loadout->third_weapon = options.defense_third_weapon;
    }
    if (options.defense_fourth_weapon != 0xFFFFFFFFu)
    {
        loadout->fourth_weapon = options.defense_fourth_weapon;
    }
    if (options.defense_starting_backpack_type != -1)
    {
        loadout->starting_backpack_type = options.defense_starting_backpack_type;
    }
}

// 0x00978160 (:1233)
void siege_mode_get_decorated_name(char* buffer)
{
    strncpy_safe(buffer, Multi_gameplay_mode_names[MGPM_SIEGE].display_name, 64);
}

// 0x0094E010 (:1257): the attackers' or the defenders' objective (nothing for a spectator)
void siege_mode_get_objective_description(char* buffer)
{
    local_player_info info;
    player_get_local_player_info(&info);
    if (info.team == Attacking_team)
    {
        memcpy(buffer, "HM_SIEGE_OBJECTIVE_ATTACK", sizeof("HM_SIEGE_OBJECTIVE_ATTACK"));
        return;
    }
    if (info.team == Defending_team)
    {
        memcpy(buffer, "HM_SIEGE_OBJECTIVE_DEFEND", sizeof("HM_SIEGE_OBJECTIVE_DEFEND"));
    }
}

// 0x0094E0B0 (:1273): the local team, the attackers, the defenders, their total scores
void siege_mode_hud_init(vint_data_item* item)
{
    static const vint_variant_type siege_template[5] = {VINT_PROP_TYPE_INT, VINT_PROP_TYPE_INT, VINT_PROP_TYPE_INT, VINT_PROP_TYPE_INT, VINT_PROP_TYPE_INT};
    item->set_template(siege_template, 5);
}

// 0x00978180 (:1285)
void siege_mode_hud_process(vint_data_item* item)
{
    const multi_team_stats* attacking = Attacking_team < HUMAN_TEAM_CIVILIAN ? &Mp_team_stats[Attacking_team] : nullptr;
    const multi_team_stats* defending = Defending_team < HUMAN_TEAM_CIVILIAN ? &Mp_team_stats[Defending_team] : nullptr;
    if (attacking == nullptr || defending == nullptr)
    {
        return;
    }
    local_player_info info;
    player_get_local_player_info(&info);
    item->set_element(0, vint_variant((int)info.team));
    item->set_element(1, vint_variant((int)Attacking_team));
    item->set_element(2, vint_variant((int)Defending_team));
    item->set_element(3, vint_variant((int)attacking->score));
    item->set_element(4, vint_variant((int)defending->score));
}

// 0x0094E100 (:1301): the HUD document's data groups: every target (the index, destroyed, locked) and the current
// targets (the unlocked group's and the first target of each later group)
void siege_mode_hud_doc_init(int doc_handle)
{
    All_targets_data_group = vint_data_group::create_new("all_siege_targets");
    if (All_targets_data_group != nullptr)
    {
        ui_add_data_group(doc_handle, (int)All_targets_data_group->get_handle()); // m_handle, inlined
        static const vint_variant_type all_template[3] = {VINT_PROP_TYPE_INT, VINT_PROP_TYPE_BOOL, VINT_PROP_TYPE_BOOL};
        for (int i = 0; i < Num_siege_targets; i++)
        {
            vint_data_item* item = All_targets_data_group->add_data_item(nullptr);
            All_targets_data_items[i] = item;
            if (item == nullptr)
            {
                break;
            }
            item->set_template(all_template, 3);
        }
    }
    Current_priority_targets_data_group = vint_data_group::create_new("current_siege_targets");
    if (Current_priority_targets_data_group == nullptr)
    {
        return;
    }
    ui_add_data_group(doc_handle, (int)Current_priority_targets_data_group->get_handle()); // m_handle, inlined
    int last_priority = Siege_no_priority;
    for (int i = 0; i < Num_siege_targets; i++)
    {
        const int priority = Siege_targets[i]->marker->priority;
        if (last_priority < priority || priority == Current_priority)
        {
            add_siege_target_data_item(i); // inlined
            last_priority = Siege_targets[i]->marker->priority;
        }
    }
}

// 0x009782C0 (:1339): every target's item (index, destroyed, locked); each standing current target with a building:
// priority, name ("<n> targets" for a locked group), percent destroyed, distance, angle, team (the attackers' when
// destroyed), locked, the number, the group's target count
void siege_mode_hud_doc_process(int doc_handle)
{
    local_player_info info;
    player_get_local_player_info(&info);
    if (All_targets_data_group != nullptr)
    {
        for (int i = 0; i < Num_siege_targets; i++)
        {
            vint_data_item* item = All_targets_data_items[i];
            if (item == nullptr)
            {
                continue;
            }
            item->set_element(0, vint_variant(i));
            item->set_element(1, vint_variant(Siege_targets[i]->marker->destroyed == true));
            item->set_element(2, vint_variant(Siege_targets[i]->marker->priority != Current_priority));
        }
    }
    if (Current_priority_targets_data_group == nullptr)
    {
        return;
    }
    int number = 1;
    for (int i = 0; i < Num_siege_targets; i++)
    {
        vint_data_item* item = Current_priority_targets_data_items[i];
        const siege_target* target = Siege_targets[i];
        if (item == nullptr || target->marker->destroyed || target_building(target) == nullptr)
        {
            continue;
        }
        vector to_target = target->marker->pos - *info.pos;
        to_target.flatten_normalize_safe();
        const float angle = vec_delta_ang_flattened(to_target, Rfg_camera.real_orient.fvec);
        const int group_count = Num_targets_on_priority[target->marker->priority];
        const bool locked = target->marker->priority != Current_priority;
        char encoded[141];
        if (locked)
        {
            wchar_t name[47];
            _snprintf_safe(name, 47, "%d %ls", group_count, localization_get_display_string_req(group_count == 1 ? "HM_TARGET_TITLE" : "HM_FSM_TARGETS", nullptr));
            vint_encode_wide_text(encoded, sizeof(encoded), name);
        }
        else
        {
            vint_encode_wide_text(encoded, sizeof(encoded), target->name);
        }
        item->set_element(0, vint_variant(target->marker->priority));
        item->set_element(1, vint_variant((const char*)encoded));
        item->set_element(2, vint_variant(target->percent_destroyed));
        item->set_element(3, vint_variant(target->distance_to_local_player));
        item->set_element(4, vint_variant(-angle));
        item->set_element(5, vint_variant((int)(target->marker->destroyed ? Attacking_team : Defending_team)));
        item->set_element(6, vint_variant(locked));
        item->set_element(7, vint_variant(number++));
        item->set_element(8, vint_variant(group_count));
    }
}

// 0x0094E290 (:1397)
bool siege_mode_hud_doc_should_render(int doc_handle)
{
    return !hud_ui_multiplayer_splash_screen_is_active();
}

// 0x0094E2A0 (:1402)
void siege_mode_hud_doc_shutdown(int doc_handle)
{
    All_targets_data_group = nullptr;
    Current_priority_targets_data_group = nullptr;
    memset(All_targets_data_items, 0, sizeof(All_targets_data_items));
    memset(Current_priority_targets_data_items, 0, sizeof(Current_priority_targets_data_items));
}

// 0x0094E2E0 (:1413): no mode specific stats travel (the columns are experience event counts)
void siege_mode_append_mode_specific_stats(unsigned char* packet_data, unsigned short& packet_size, unsigned int player_game_data_handle)
{
}

// 0x0094E2F0 (:1418)
void siege_mode_read_mode_specific_stats(unsigned char* packet_data, int& packet_offset, unsigned int player_game_data_handle)
{
}

// 0x0094E300 (:1423)
int siege_mode_get_mode_specific_stat_count()
{
    return 2;
}

// 0x00978730 (:1428): the results screen's columns: the destroy and protect experience events' names. Original: an
// invalid event is not checked for null.
void siege_mode_get_mode_specific_stat_name(char* name, const int max_name_len, int stat_index)
{
    if (name == nullptr)
    {
        return;
    }
    if (stat_index == 0)
    {
        _snprintf_safe(name, max_name_len, "%s", multi_game_xp_get_event_info(MXE_DESTROY_SIEGE_TARGET)->display_name); // inlined
        return;
    }
    if (stat_index == 1)
    {
        _snprintf_safe(name, max_name_len, "%s", multi_game_xp_get_event_info(MXE_PROTECT_SIEGE_TARGET)->display_name); // inlined
        return;
    }
    _snprintf_safe(name, max_name_len, "%s", "");
}

// 0x009787A0 (:1445): the column values: how often the player got those events. Original: no null check of the
// stats.
void siege_mode_get_mode_specific_stat_value(char* value, const int max_value_len, int stat_index, unsigned int player_game_data_handle)
{
    const multi_player_stats* stats = (player_game_data_handle == 0xFFFFFFFFu || (player_game_data_handle & 0xFFFF) > 16) ? nullptr : &Mp_player_stats[player_game_data_handle & 0xFFFF];
    if (stat_index == 0)
    {
        _snprintf_safe(value, max_value_len, "%d", (int)stats->xp_event_frequency[MXE_DESTROY_SIEGE_TARGET]);
        return;
    }
    if (stat_index == 1)
    {
        _snprintf_safe(value, max_value_len, "%d", (int)stats->xp_event_frequency[MXE_PROTECT_SIEGE_TARGET]);
        return;
    }
    _snprintf_safe(value, max_value_len, "%s", "");
}
