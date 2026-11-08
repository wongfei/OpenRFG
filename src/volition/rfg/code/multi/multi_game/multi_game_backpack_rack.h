// Original: volition/rfg/code/multi/multi_game/multi_game_backpack_rack.h
// Implementation: volition/rfg/code/multi/multi_game/multi_game_backpack_rack.cpp
//
// Backpack racks: the racks backpacks spawn on, their sync to clients, and their destruction and repair.
#pragma once

#include "volition/rfg/code/objects/humans/human_teams.h"

class multi_connection;
class multi_object_backpack;
class multi_object_marker;
class player;
struct sender;

void multi_game_backpack_rack_level_init_post(); // 0x00979130 (:169)
void multi_game_backpack_rack_round_begin(); // 0x009DC6E0 (:180)
void multi_game_backpack_rack_round_end(); // 0x0094F190 (:270)
void multi_game_backpack_rack_process(); // 0x009F0200 (:277)
void multi_game_backpack_rack_render(); // 0x00979150 (:380)
void multi_game_backpack_rack_destroyed(multi_object_marker* rack); // 0x009DCA60 (:528)
void multi_game_backpack_rack_repaired(multi_object_marker* rack, human_teams repairing_team); // 0x009DCB50 (:566)
void multi_game_backpack_rack_backpack_picked_up(multi_object_marker* rack, multi_object_backpack* backpack); // 0x00979910 (:586)
void multi_game_backpack_rack_backpack_dropped(multi_object_marker* rack, multi_object_backpack* backpack); // 0x009ABD00 (:601)
void multi_game_backpack_rack_player_died(multi_object_marker* rack, player* pp); // 0x009ABDE0 (:629)
void send_backpack_rack_sync(multi_connection* cp); // 0x009BC360 (:640)
void multi_game_backpack_rack_process_sync(unsigned char* packet_data, const sender& from); // 0x009BC510 (:666)
void multi_game_backpack_rack_process_destroyed(unsigned char* packet_data, const sender& from); // 0x009DCBC0 (:707)
void multi_game_backpack_rack_process_repaired(unsigned char* packet_data, const sender& from); // 0x009DCC80 (:735)
