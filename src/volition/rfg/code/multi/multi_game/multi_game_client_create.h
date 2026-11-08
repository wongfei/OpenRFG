// Original: volition/rfg/code/multi/multi_game/multi_game_client_create.h
// Implementation: volition/rfg/code/multi/multi_game/multi_game_client_create.cpp
//
// Client-created objects: projectiles and weapons a client creates ahead of the server, matched by a unique id when the
// server's create arrives.
#pragma once


class projectile;
struct sender;
class vector;
struct weapon_info;

float multi_game_ca_projectile_time_offset_sec(const projectile* proj, const vector& server_create_pos); // 0x009511D0 (:235)
void multi_game_get_client_create_unique_id(unsigned int& unique_id); // 0x00997600 (:255)
void multi_game_client_create(projectile* proj); // 0x009BD260 (:272)
void multi_game_process_client_object_create(unsigned char* packet_data, const sender& from); // 0x009DD850 (:279)
void multi_game_process_client_create_no(unsigned char* packet_data, const sender& from); // 0x00951270 (:287)
bool multi_game_client_creates_weapon(const weapon_info* info); // 0x009512F0 (:304)
