// Original: volition/rfg/code/multi/multi_game/modes/multi_game_mode_spectator.h
// Implementation: volition/rfg/code/multi/multi_game/modes/multi_game_mode_spectator.cpp
//
// The spectator view (not a mode of its own): a player in spectator view follows other players, cycling the target.
#pragma once


class player;
struct sender;

void multi_enter_spectator_view(player* pp); // 0x00978820 (:47)
void multi_exit_spectator_view(player* pp, bool set_state); // 0x009788D0 (:92)
bool multi_is_player_in_spectator_view(const player* pp); // 0x0094E310 (:135)
player* multi_find_next_spectator_target(); // 0x00995150 (:144)
player* multi_find_prev_spectator_target(); // 0x00995230 (:185)
bool multi_spectator_target_is_valid(unsigned int target_handle); // 0x00978990 (:226)
void multi_send_spectator_to_all(const player* pp, bool start); // 0x009BB480 (:251)
void multi_process_spectator(unsigned char* packet_data, const sender& from); // 0x009DBCB0 (:276)
void multi_game_mode_spectator_render(); // 0x0094E330 (:308)
