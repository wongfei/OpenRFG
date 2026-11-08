// Original: volition/rfg/code/multi/multi_game/multi_game_destruction_log.h
// Implementation: volition/rfg/code/multi/multi_game/multi_game_destruction_log.cpp
//
// The destruction log (a debug log file of the networked destruction).
#pragma once

void destruction_log_sprintf(const char*, ...); // 0x00955A80 (:179)
void destruction_log_aux_sprintf(const char*, ...); // 0x00955A90 (:198)
void multi_game_destruction_log_system_init(); // 0x00955AA0 (:217)
void destruction_log_flush_and_close(); // 0x00955AB0 (:225)
void destruction_log_level_init_pre(); // 0x00955AC0 (:262)
