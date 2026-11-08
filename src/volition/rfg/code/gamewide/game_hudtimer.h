// Original: volition/rfg/code/gamewide/game_hudtimer.h
// Implementation: volition/rfg/code/gamewide/game_hudtimer.cpp
//
// The HUD timer (mission and match countdowns): update and render.
#pragma once

void unused_dcf_hudtimer_stop(); // 0x007CD8B0 (:92)
void unused_dcf_hudtimer_pause(); // 0x007CD8F0 (:97)
void unused_dcf_hudtimer_unpause(); // 0x007CD930 (:102)
void game_hudtimer_countup(); // 0x007C19C0 (:122)
void game_hudtimer_countdown(int duration); // 0x007C19E0 (:135)
void game_hudtimer_countdown_add(int duration); // 0x007C1A00 (:149)
void game_hudtimer_update(float frametime); // 0x007C1A20 (:160)
void game_hudtimer_render(); // 0x007CD970 (:207)
void game_hudtimer_stop(); // 0x007C1B20 (:246)
void game_hudtimer_pause(bool pause); // 0x007C1B60 (:273)
int game_hudtimer_get_time(); // 0x007C1BD0 (:306)
bool game_hudtimer_running(); // 0x007C1BE0 (:313)
void game_hudtimer_set_expiration_callback(unsigned short lua_func_handle); // 0x007C1C20 (:339)
