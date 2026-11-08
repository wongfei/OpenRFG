// Original: volition/rfg/code/multi/multi_game/multi_game_respawn.cpp
//
// Every live function (multiplayer.md MP6, final_push.md step 7): when a player may respawn, the server's choice of a
// spawn point (each enabled player start of the player's team weighted by the distances to the living players, the
// recent kills, the spawns there and the enemies' attached charges, plus the mode's adjustment and a random spread; the
// best one with room to stand wins) and the respawn packet (PT_RESPAWN_PLAYER).
// Vehicle respawns: a destroyed vehicle's spawn node spawns its next vehicle through a sim event
// (multi_game_respawn_request_vehicle_respawn). multi_game_respawn_find_spawning_point_handle has no caller in rfg.exe.
// Part of the multi unity TU (rfg_multi_unity.obj, every multi/ .cpp in path order): the file statics are shared
// by the whole tree, and a shared one is placed in its first user (tools/hdrgen/specs/mp0_stubs.py).

#include "volition/rfg/code/multi/multi_game/multi_game_respawn.h"

#include "volition/rfg/code/level/world.h"
#include "volition/rfg/code/level/world.inl"
#include "volition/rfg/code/multi/multi_core/include/multi.h"
#include "volition/rfg/code/multi/multi_core/include/multi_connection.h"
#include "volition/rfg/code/multi/multi_core/include/multi_core_io.h"
#include "volition/rfg/code/multi/multi_core/include/multi_core_objects.h"
#include "volition/rfg/code/multi/multi_core/include/multi_core_packet_types.h"
#include "volition/rfg/code/multi/multi_core/include/multi_session.h"
#include "volition/rfg/code/multi/multi_game/modes/multi_game_mode_common.h"
#include "volition/rfg/code/multi/multi_game/multi_game_data.h"
#include "volition/rfg/code/multi/multi_game/multi_game_settings.h"
#include "volition/rfg/code/multi/multi_game/multi_game_sim_event.h"
#include "volition/rfg/code/multi/multi_game/multi_gameplay_mode.h"
#include "volition/rfg/code/objects/humans/human.h"
#include "volition/rfg/code/objects/humans/human_ragdoll.h"
#include "volition/rfg/code/objects/humans/human_teams.h"
#include "volition/rfg/code/objects/humans/player.h"
#include "volition/rfg/code/objects/object_player_start.h"
#include "volition/rfg/code/objects/object_spawn_node.h"
#include "volition/rfg/code/objects/projectile.h"
#include "volition/rfg/code/spawn/spawn_placement.h"
#include "volition/rfg/code/vehicle/vehicle_class.h"
#include "volition/vlib/code/math/rand.h"
#include "volition/vlib/code/math/vector3.h"
#include "volition/vlib/code/os/timer.h"

#include <stdlib.h>

// ---- file-static globals
// dynamic_uint_hash_table<remote_object*> Game_handle_to_rop_hash (0x01691B40) is used here too; placed in multi_core_delta_compression.cpp.
// dynamic_uint_hash_table<network_game_handle*> Reserved_game_to_network_hash (0x01691C18) is used here too; placed in multi_core_objects.cpp.
// unsigned int Clock_current_time (0x02FEB538) is used here too; placed in multi.cpp.
// bool Server (0x02FEB590) is used here too; placed in multi_core_session_generic.cpp.
// mp_game_data_wrapper* Mp_game_data (0x02FEB698) is used here too; placed in multi_connection.cpp.

// Magic number: the spawn point lists hold 256 entries.
static const int Max_spawn_points = 256;

// A player start of the type list (the spawn points index it).
static player_start* player_start_at(int index)
{
    return World.get_object<player_start>(index);
}

// x / divisor, at most 1 (the clamps compare in double).
static float clamp_ratio(float x, float divisor)
{
    float ratio = x / divisor;
    if ((double)ratio - 1.0 >= 0.0)
    {
        ratio = 1.0f;
    }
    return ratio;
}

// ---- internal functions
static int get_valid_player_starts(spawn_point* valid_player_starts, human_teams team, bool include_neutral, bool include_enemy); // 0x009A07C0 (:117)
static bool event_process_vehicle_respawn(void* data); // 0x00961160 (:148)
static void event_destroy_vehicle_respawn(void* data); // 0x00961170 (:155)
static int player_start_sort_compare(const void* left, const void* right); // 0x00961180 (:159)

// 0x009A07C0 (:117): the enabled starts of the team (any start without a team; the neutral ones and every team's on
// request)
// LTCG: valid_player_starts in ESI, team in EDI, the flags on the stack (the port has no register convention).
static int get_valid_player_starts(spawn_point* valid_player_starts, human_teams team, bool include_neutral, bool include_enemy)
{
    int count = 0;
    for (int i = 0; i < World.get_num_objects<player_start>(); i++)
    {
        const player_start* start = player_start_at(i);
        if (team == HUMAN_TEAM_NONE ||
            (start->enabled && (start->team == team || (start->team == HUMAN_TEAM_CIVILIAN && include_neutral) || include_enemy)))
        {
            valid_player_starts[count].object_index = i;
            valid_player_starts[count].weight = 0.0f;
            count++;
        }
    }
    return count;
}

// 0x00961180 (:159): the highest weight first
static int player_start_sort_compare(const void* left, const void* right)
{
    const spawn_point* l = *(const spawn_point* const*)left;
    const spawn_point* r = *(const spawn_point* const*)right;
    if (r->weight < l->weight)
    {
        return -1;
    }
    if (l->weight < r->weight)
    {
        return 1;
    }
    return 0;
}

// 0x009611C0 (:179)
void multi_game_respawn_init()
{
}

// 0x009611D0 (:190)
void multi_game_respawn_shutdown()
{
}

// 0x009611E0 (:198)
void multi_game_respawn_match_init()
{
}

// 0x009611F0 (:211)
void multi_game_respawn_match_shutdown()
{
}

// 0x00961200 (:222)
void multi_game_respawn_match_process()
{
}

// 0x00961210 (:234): not before the player's respawn time (a time more than 250 hours off counts as passed), and only
// in gameplay
respawn_request_deny_reasons multi_game_respawn_can_player_respawn(const player* pp)
{
    if (pp == nullptr)
    {
        return RRDR_NO;
    }
    // inlined: a valid respawn time that has not elapsed
    if (pp->mp_time_until_player_can_respawn.valid() && !pp->mp_time_until_player_can_respawn.elapsed())
    {
        return RRDR_TIME_UNTIL_RESPAWN;
    }
    return Multi_match_info.match_state != MGMS_GAMEPLAY ? RRDR_NOT_IN_GAMEPLAY : RRDR_NO_REASON;
}

// 0x009A0980 (:257)
// The server's spawn point (an index into the player starts) for a player: the candidates are the enabled starts of
// its team (team modes: its own team's on the first spawn or in siege, else with the neutral ones and, outside capture
// the flag, every team's; without teams every enabled start; none: every start). Each candidate's weight (distances in
// the level's distance multiplier): -1000 within 3 m of a living player, +5 near (fading out at 90 m), an enemy -100
// within 30 m and -20 within 50 m, a friend +10 within 20 m; the player's own death spot (40 s) and a friend's death
// (15 s) push away; a start the player respawned at in the last minute -15; an enemy's attached charge within 10 m
// -10; the mode's adjustment and +-4 of noise. The best start with room to stand (human_find_place_to_stand_from_list)
// wins; none: start 0. -1 without starts or not on the server.
int multi_game_respawn_get_best_spawn_point(const player* respawning_player, bool first_spawn)
{
    if (!Multiplayer || !Server || respawning_player == nullptr)
    {
        return -1;
    }
    const int num_starts = World.get_num_objects<player_start>();
    if (num_starts == 0)
    {
        keen::debug::nativePrintArgs("Multiplayer levels need at least one player start");
        return -1;
    }
    const multi_game_data* my_data = multi_game_data_get(respawning_player->mp_data, false, false); // inlined
    if (my_data == nullptr)
    {
        return -1;
    }
    spawn_point candidates[Max_spawn_points];
    int count = 0;
    if (multi_game_mode_has_teams())
    {
        const multi_gameplay_mode mode = multi_gameplay_mode_get();
        if (first_spawn || mode == MGPM_SIEGE)
        {
            const human_teams team = respawning_player->current_team;
            for (int i = 0; i < num_starts; i++)
            {
                const player_start* start = player_start_at(i);
                if (team == HUMAN_TEAM_NONE || (start->enabled && start->team == team))
                {
                    candidates[count].object_index = i;
                    candidates[count].weight = 0.0f;
                    count++;
                }
            }
        }
        else if (mode == MGPM_CAPTURE_THE_FLAG)
        {
            count = get_valid_player_starts(candidates, respawning_player->current_team, true, false);
        }
        else
        {
            count = get_valid_player_starts(candidates, respawning_player->current_team, true, true);
        }
    }
    else
    {
        for (int i = 0; i < num_starts; i++)
        {
            if (player_start_at(i)->enabled)
            {
                candidates[count].object_index = i;
                candidates[count].weight = 0.0f;
                count++;
            }
        }
    }
    if (count == 0)
    {
        for (count = 0; count < num_starts; count++)
        {
            candidates[count].object_index = count;
            candidates[count].weight = 0.0f;
        }
        if (count == 0)
        {
            return -1;
        }
    }
    const float distance_multiplier = Mp_server_options.get_level()->distance_multiplier;
    for (int c = 0; c < count; c++)
    {
        const player_start* start = player_start_at(candidates[c].object_index);
        float& weight = candidates[c].weight;
        for (int i = 0; i < World.get_num_objects<player>(); i++)
        {
            player* other = World.get_object<player>(i);
            if (other->obj_flags.destroyed || multi_game_data_get(other->mp_data, false, false) == nullptr)
            {
                continue;
            }
            if (!human_is_dead(other) && !other->pflags.mp_ignore_player_for_respawn_collisions)
            {
                // Magic numbers: the distances (m) and weights of the comment above.
                const float dist = vec_dist(start->pos, other->pos) / distance_multiplier;
                weight = (clamp_ratio(dist, 3.0f) * 1000.0f - 1000.0f) + weight;
                weight = clamp_ratio(dist, 90.0f) * -5.0f + 5.0f + weight;
                if (human_teams_are_enemies(other->current_team, respawning_player->current_team))
                {
                    weight = (clamp_ratio(dist, 30.0f) * 100.0f - 100.0f) + weight;
                    weight = (clamp_ratio(dist, 50.0f) * 20.0f - 20.0f) + weight;
                }
                else if (human_teams_are_friendly(other->current_team, respawning_player->current_team))
                {
                    weight = clamp_ratio(dist, 20.0f) * -10.0f + 10.0f + weight;
                }
            }
            float own_death = 0.0f;
            float friend_death = 0.0f;
            const int since_killed = timer_get(TIMER_MILLISECONDS) - other->combat.last_killed_time_ms;
            const float killed_dist = vec_dist(other->combat.last_killed_pos, start->pos) / distance_multiplier;
            if (other->combat.last_killed_time_ms != -1 && since_killed >= 0)
            {
                if (since_killed < 40000 && other == respawning_player)
                {
                    own_death = (1.0f - (float)since_killed / 40000.0f) * (clamp_ratio(killed_dist, 30.0f) * 50.0f - 50.0f);
                }
                if (since_killed < 15000 && human_teams_are_friendly(respawning_player, other))
                {
                    friend_death = (1.0f - (float)since_killed / 15000.0f) * (clamp_ratio(killed_dist, 20.0f) * 15.0f - 15.0f);
                }
            }
            const float weight_with_deaths = own_death + weight + friend_death;
            weight = weight_with_deaths;
            float recent_spawn = 0.0f;
            if (other->mp_last_respawn_time != 0 && other->mp_last_spawn_point_handle == start->get_handle())
            {
                const int since_respawn = (int)(Clock_current_time - other->mp_last_respawn_time);
                if (since_respawn >= 0)
                {
                    recent_spawn = clamp_ratio((float)since_respawn, 60000.0f) * 15.0f - 15.0f;
                }
            }
            weight = weight_with_deaths + recent_spawn;
        }
        for (int i = 0; i < World.get_num_objects<projectile>(); i++)
        {
            projectile* proj = World.get_object<projectile>(i);
            if (!proj->is_attached())
            {
                continue;
            }
            player* owner = World.object_from_handle<player>(proj->owner_handle);
            if (owner == nullptr || !human_teams_are_enemies(owner, respawning_player->current_team))
            {
                continue;
            }
            const float dist = vec_dist(proj->pos, start->pos);
            weight = (clamp_ratio(dist / distance_multiplier, 10.0f) * 10.0f - 10.0f) + weight;
        }
        weight = multi_gameplay_mode_adjust_respawn_weight(respawning_player, start) + weight;
        // Magic numbers: +-4 of noise.
        weight = fl_rand_range(-4.0f, 4.0f) + weight;
    }
    spawn_point* sorted[Max_spawn_points];
    for (int c = 0; c < count; c++)
    {
        sorted[c] = &candidates[c];
    }
    qsort(sorted, count, sizeof(sorted[0]), player_start_sort_compare);
    int best = -1;
    for (int c = 0; c < count; c++)
    {
        vector destination;
        const vector& start_pos = player_start_at(sorted[c]->object_index)->pos;
        if (best == -1 && human_find_place_to_stand_from_list(destination, start_pos, &start_pos, 1, 0xFFFFFFFF))
        {
            best = sorted[c]->object_index;
        }
    }
    return best != -1 ? best : 0;
}

// The respawn packet: the player's network handle, the start, its desired weapon and backpack. Nothing without a
// multiplayer data entry in use.
static unsigned short respawn_build_packet(unsigned char* packet, player* pp, int player_start_idx)
{
    packet[0] = PT_RESPAWN_PLAYER;
    unsigned short size = packet_header_size(PT_RESPAWN_PLAYER);
    write_u16_be(packet, size, multi_core_network_handle_from_game_handle(pp->get_handle(), true));
    packet[size++] = (unsigned char)player_start_idx;
    const multi_game_data* data = multi_game_data_get(pp->mp_data, false, false); // inlined
    if (data == nullptr)
    {
        return 0;
    }
    write_u32_be(packet, size, data->desired_weapon_inv_item_info_handle);
    write_u32_be(packet, size, (unsigned int)pp->mp_respawn_backpack_type);
    packet_set_size(packet, size);
    return size;
}

// 0x009D26E0 (:541)
void multi_game_respawn_send_respawn_to_all(player* pp, int player_start_idx, bool decrement_num_respawns)
{
    if (pp->mp_data == 0xFFFFFFFF)
    {
        return;
    }
    unsigned char packet[Packet_buffer_size];
    const unsigned short size = respawn_build_packet(packet, pp, player_start_idx);
    if (size != 0)
    {
        multi_io_send_to_all_reliable(multi_session_get_by_type(ST_GAME), packet, size);
    }
}

// 0x009D2920 (:572)
void multi_game_respawn_send_respawn_to_player(multi_connection* cp, player* pp, int player_start_idx, bool decrement_num_respawns)
{
    if (pp->mp_data == 0xFFFFFFFF)
    {
        return;
    }
    unsigned char packet[Packet_buffer_size];
    const unsigned short size = respawn_build_packet(packet, pp, player_start_idx);
    if (size != 0)
    {
        multi_io_send_reliable(cp, packet, size, nullptr, nullptr, nullptr);
    }
}

// 0x009D2B60 (:603): a client respawns the player at the start with the server's desired weapon and backpack
void multi_game_respawn_process_respawn(unsigned char* packet_data, const sender& from)
{
    int offset = 0;
    const unsigned short net_handle = read_u16_be(packet_data, offset);
    const unsigned char start_index = packet_data[offset++];
    const unsigned int desired_weapon = read_u32_be(packet_data, offset);
    const unsigned int backpack_type = read_u32_be(packet_data, offset);
    const unsigned int game_handle = multi_core_game_handle_from_network_handle(net_handle, true);
    if (game_handle == 0xFFFFFFFF)
    {
        return;
    }
    object* objp = World.object_from_handle(game_handle);
    if (objp == nullptr || objp->get_obj_type() != OT_HUMAN || objp->get_sub_type() != OT_SUB_HUMAN_PLAYER || objp->obj_flags.destroyed)
    {
        return;
    }
    player* pp = (player*)objp;
    if (pp->mp_data != 0xFFFFFFFF)
    {
        Mp_game_data->data[pp->mp_data & 0xFFFF][pp->mp_data >> 16].desired_weapon_inv_item_info_handle = desired_weapon;
    }
    pp->mp_respawn_backpack_type = (int)backpack_type;
    player_respawn(pp, start_index, true);
}

// 0x00961160 (:148): the spawn node spawns its next vehicle
static bool event_process_vehicle_respawn(void* data)
{
    return ((object_vehicle_spawn_node*)data)->spawn_mp_child();
}

// 0x00961170 (:155): the event holds the spawn node, which it does not own
static void event_destroy_vehicle_respawn(void* data)
{
}

// 0x009B21B0 (:656): a vehicle that came from a vehicle spawn node (its parent) respawns there delay_ms later
void multi_game_respawn_request_vehicle_respawn(const vehicle* vp, int delay_ms)
{
    const unsigned int parent = vp->get_parent();
    if (parent == 0xFFFFFFFF)
    {
        return;
    }
    object* spawn_node = World.object_from_handle(parent);
    if (spawn_node != nullptr && spawn_node->get_obj_type() == OT_VEHICLE_SPAWN_NODE)
    {
        multi_game_sim_event_add(Clock_current_time + delay_ms, MULTI_GAME_SIM_EVENT_VEHICLE_RESPAWN, true, spawn_node, nullptr, event_process_vehicle_respawn, event_destroy_vehicle_respawn);
    }
}
