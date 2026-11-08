// Original: volition/rfg/code/ai/player_attack_limiter.h
// Implementation: volition/rfg/code/ai/player_attack_limiter.cpp
//
// The player attack limiter: limits how many NPCs attack the player at once, scoring the requesting NPCs.
#pragma once


class npc;

int find_record(unsigned int npc_handle); // 0x005F0650 (:181)
float calculate_score(const npc* np, bool was_on_list); // 0x005F0680 (:198)
void player_attack_limiter_level_init_pre(); // 0x005F0730 (:235)
bool player_attack_limiter_request(const npc* np); // 0x006402F0 (:252)
bool player_attack_limiter_check(const npc* np); // 0x005F0760 (:386)
