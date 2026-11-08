// Original: volition/rfg/code/multi/multi_game/multi_game_timer.h
// Implementation: volition/rfg/code/multi/multi_game/multi_game_timer.cpp
//
// Multiplayer timers on the network clock (pausable).
#pragma once

// size 0x8
class multi_timer
{
public:
    multi_timer(); // 0x00966E30 (:107)
    void set(int time_ms); // 0x00986DA0 (:110)
    int time_until() const; // 0x00966E40 (:120)
    int time_since() const; // 0x00966E50 (:125)
    bool valid() const; // 0x00966E60 (:130)
    bool elapsed() const; // 0x00966E70 (:135)
    void invalidate(); // 0x00966E80 (:140)
    void increment(int time_ms); // 0x00966E90 (:145)
    void set_pause(bool pause); // 0x00966EA0 (:152)
    bool is_paused() const; // 0x00966EB0 (:157)

private:
    int m_expiry_time_ms; // 0x00
    bool m_is_valid; // 0x04
    bool m_is_paused; // 0x05
};

void multi_game_timer_process(); // 0x00986DC0 (:188)
void multi_game_timer_render(); // 0x00986EE0 (:216)
bool multi_game_timer_elapsed(); // 0x00966F10 (:245)
int multi_game_timer_time_until(); // 0x00966F30 (:250)
void multi_game_timer_invalidate(); // 0x00966F40 (:255)
void multi_game_timer_set(int time_ms); // 0x00986FB0 (:261)
void multi_game_timer_init(); // 0x00966F50 (:278)
void multi_game_timer_pause(); // 0x00966F70 (:286)
void multi_game_timer_unpause(); // 0x009870A0 (:291)
bool multi_game_timer_is_valid(); // 0x00966F80 (:304)
