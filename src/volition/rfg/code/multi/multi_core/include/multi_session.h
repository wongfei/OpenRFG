// Original: volition/rfg/code/multi/multi_core/include/multi_session.h
// Implementation: volition/rfg/code/multi/multi_core/global/multi_session.cpp
//
// The game's session: creating, joining, leaving and migrating the platform session of the current game.
#pragma once

#include "volition/rfg/code/multi/multi_core/include/multi_core_session_base.h"
#include "volition/rfg/code/multi/multi_core/include/multi_pool.inl"

class multi_session;
class multi_session_info;

extern multi_session* Game_session; // 0x02FEB654
// The sessions (party, transition, game, and a spare). Defined in multi_session.cpp (:73); declared here for the
// multi unity files before it (multi_core_session_base.cpp).
extern multi_pool<multi_session, 4> Session_pool; // 0x01691D10

unsigned short mt_ntohs(unsigned short val); // 0x00941E40 (.h:68)
unsigned short mt_htons(unsigned short val); // 0x00941E50 (.h:74)
void multi_session_system_init(); // 0x009F7CB0 (:530)
void multi_session_system_shutdown(); // 0x009FC6E0 (:539)
void multi_session_system_process(); // 0x009FC8B0 (:575)
multi_session* multi_session_allocate(const char* debug_name); // 0x00990D80 (:592)
void multi_session_free(multi_session* sp); // 0x009AABD0 (:607)
multi_session* multi_session_get_by_type(session_types type); // 0x00990DB0 (:623)
bool multi_session_is_session_game_session(const multi_session* sp); // 0x0094B930 (:644)
bool multi_session_is_session_lobby_session(const multi_session* sp); // 0x0094B950 (:651)
bool multi_session_is_session_party_session(const multi_session* sp); // 0x0094B970 (:658)
bool multi_session_is_active_game_session(const multi_session* sp); // 0x0094B990 (:665)
session_types multi_session_get_session_type(const multi_session* sp); // 0x0094B9C0 (:674)
multi_session* multi_session_find_by_id(const multi_session_info& info); // 0x00990E30 (:681)
