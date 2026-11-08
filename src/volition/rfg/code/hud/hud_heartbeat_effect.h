// Original: volition/rfg/code/hud/hud_heartbeat_effect.h
// Implementation: volition/rfg/code/hud/hud_heartbeat_effect.cpp
//
// The low health heartbeat effect (screen pulse and sound). Placement of heartbeat_table_position inferred.
#pragma once

// size 0xC
struct heartbeat_table_position
{
    float interpolation_percent; // 0x00
    int lower_index; // 0x04
    int higher_index; // 0x08
};

void unused_dcf_reload_heartbeat(); // 0x008559D0 (:79)
float heartbeat_get_health(); // 0x00843230 (:84)
heartbeat_table_position get_table_position(float player_health); // 0x00843280 (:100)
void heartbeat_load_data(); // 0x008433A0 (:146)
void heartbeat_reset_effects(); // 0x00843620 (:216)
void heartbeat_process(float hit_damage_amount); // 0x008436A0 (:228)
float heartbeat_get_ring_alpha(); // 0x00843CD0 (:311)
void heartbeat_reset_timestamp(); // 0x00843DA0 (:328)
float heartbeat_get_current_blur(); // 0x00843DB0 (:334)
