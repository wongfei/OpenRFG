// Original: volition/rfg/code/multi/multi_game/modes/multi_game_mode_spectator.cpp
//
// The spectator view: an invulnerable player in HS_SPECTATOR that follows the living players (of its own team when
// Spectator_same_team_only), entered and left by PT_SPECTATOR from the server.
// Part of the multi unity TU (rfg_multi_unity.obj, every multi/ .cpp in path order): the file statics are shared
// by the whole tree, and a shared one is placed in its first user (tools/hdrgen/specs/mp0_stubs.py).

#include "volition/rfg/code/multi/multi_game/modes/multi_game_mode_spectator.h"
#include "volition/rfg/code/multi/multi_game/multi_game.h"
#include "volition/rfg/code/multi/multi_game/multi_game_data.h"
#include "volition/rfg/code/multi/multi_core/include/multi.h"
#include "volition/rfg/code/multi/multi_core/include/multi_core_io.h"
#include "volition/rfg/code/multi/multi_core/include/multi_core_objects.h"
#include "volition/rfg/code/multi/multi_core/include/multi_core_packet_types.h"
#include "volition/rfg/code/multi/multi_core/include/multi_session.h"
#include "volition/rfg/code/camera/rfg_camera.h"
#include "volition/rfg/code/control/control.h"
#include "volition/rfg/code/hud/hud.h"
#include "volition/rfg/code/level/world.h"
#include "volition/rfg/code/objects/humans/human.h"
#include "volition/rfg/code/objects/humans/player.h"
#include "keen/base/trace.hpp"

// ---- file-static globals
static bool Spectator_same_team_only = true; // 0x0167D83B
// bool Server (0x02FEB590) is used here too; placed in multi_core_session_generic.cpp.
static int Spectator_cycle_index; // 0x02FEB650
// mp_game_data_wrapper* Mp_game_data (0x02FEB698) is used here too; placed in multi_connection.cpp.

// 0x00978820 (:47)
void multi_enter_spectator_view(player* pp)
{
    if (multi_game_data_get(pp->mp_data, false, false) == nullptr)
    {
        return;
    }
    pp->hflags.invulnerable = 1;
    hud_reset_damage_indicators();
    human_set_state(pp, HS_SPECTATOR, HMS_IDLE, -1.0f);
    if (!human_is_ragdolled(pp))
    {
        human_set_movement_mode(pp, HMM_NONE, HMSM_NONE);
    }
    if (object_is_local_player(pp))
    {
        control_set_mode(CM_SPECTATOR);
    }
}

// 0x009788D0 (:92)
void multi_exit_spectator_view(player* pp, bool set_state)
{
    if (pp->cur_state != HS_SPECTATOR || multi_game_data_get(pp->mp_data, false, false) == nullptr)
    {
        return;
    }
    pp->hflags.invulnerable = 0;
    if (set_state)
    {
        human_set_state(pp, pp->is_killed() ? HS_DEAD : HS_STANDARD, HMS_CURRENT, -1.0f);
    }
    if (object_is_local_player(pp))
    {
        control_set_mode(CM_GAMEPLAY);
        camera_level_init_post();
    }
}

// 0x0094E310 (:135)
bool multi_is_player_in_spectator_view(const player* pp)
{
    return pp != nullptr && pp->cur_state == HS_SPECTATOR;
}

// The living players the spectator may follow (16 at most).
static int collect_spectator_targets(player** targets)
{
    int num_targets = 0;
    const int count = World.get_num_objects<player>();
    for (int i = 0; i < count; i++)
    {
        player* pp = World.get_object<player>(i);
        // Original: the local player is not checked for null.
        if (pp != nullptr && !pp->is_killed() && (!Spectator_same_team_only || pp->current_team == player_get_local_player()->current_team))
        {
            targets[num_targets++] = pp;
        }
    }
    return num_targets;
}

// The camera's target when it is a player, else the local player.
static player* current_spectator_target()
{
    if (Rfg_camera.target_handle != 0xFFFFFFFFu)
    {
        object* objp = World.object_from_handle(Rfg_camera.target_handle);
        if (objp != nullptr && objp->get_obj_type() == OT_HUMAN && objp->get_sub_type() == OT_SUB_HUMAN_PLAYER)
        {
            return (player*)objp;
        }
    }
    return player_get_local_player();
}

// 0x00995150 (:144)
player* multi_find_next_spectator_target()
{
    player* targets[16];
    const int num_targets = collect_spectator_targets(targets);
    if (num_targets != 0)
    {
        Spectator_cycle_index++;
        if (Spectator_cycle_index >= num_targets)
        {
            Spectator_cycle_index = 0;
        }
        return targets[Spectator_cycle_index];
    }
    return current_spectator_target();
}

// 0x00995230 (:185)
player* multi_find_prev_spectator_target()
{
    player* targets[16];
    const int num_targets = collect_spectator_targets(targets);
    if (num_targets != 0)
    {
        Spectator_cycle_index--;
        if (Spectator_cycle_index < 0)
        {
            Spectator_cycle_index = num_targets - 1;
        }
        return targets[Spectator_cycle_index];
    }
    return current_spectator_target();
}

// 0x00978990 (:226)
bool multi_spectator_target_is_valid(unsigned int target_handle)
{
    if (target_handle == 0xFFFFFFFFu)
    {
        return false;
    }
    object* objp = World.object_from_handle(target_handle);
    if (objp == nullptr || objp->get_obj_type() != OT_HUMAN || objp->get_sub_type() != OT_SUB_HUMAN_PLAYER || objp->is_killed())
    {
        return false;
    }
    if (Spectator_same_team_only && player_get_local_player() != nullptr && ((player*)objp)->current_team != player_get_local_player()->current_team)
    {
        return false;
    }
    return true;
}

// 0x009BB480 (:251): server: PT_SPECTATOR (the player's network handle, BE, and start/stop)
void multi_send_spectator_to_all(const player* pp, bool start)
{
    if (!Multiplayer || !Server)
    {
        return;
    }
    unsigned char packet[1200];
    packet[0] = PT_SPECTATOR;
    unsigned short packet_size = packet_header_size(PT_SPECTATOR);
    write_u16_be(packet, packet_size, multi_core_network_handle_from_game_handle(pp->get_handle(), false));
    if (multi_game_data_get(pp->mp_data, false, false) == nullptr)
    {
        return;
    }
    packet[packet_size++] = start;
    packet_set_size(packet, packet_size);
    multi_io_send_to_all_reliable(multi_session_get_by_type(ST_GAME), packet, packet_size);
}

// 0x009DBCB0 (:276): PT_SPECTATOR (client)
void multi_process_spectator(unsigned char* packet_data, const sender& from)
{
    if (!Multiplayer || Server)
    {
        return;
    }
    player* pp = player_from_network_handle((unsigned short)(packet_data[0] << 8 | packet_data[1]));
    if (pp == nullptr)
    {
        keen::debug::nativePrintArgs("Can't find player to prcess spectator view");
    }
    // Original: a missing player is dereferenced after the message.
    if (multi_game_data_get(pp->mp_data, false, false) == nullptr)
    {
        return;
    }
    if (packet_data[2] != 0)
    {
        multi_enter_spectator_view(pp);
        return;
    }
    multi_exit_spectator_view(pp, true);
}

// 0x0094E330 (:308)
void multi_game_mode_spectator_render()
{
}
