// Original: volition/rfg/code/multi/multi_game/multi_game_classes.h
// Implementation: volition/rfg/code/multi/multi_game/multi_game_classes.cpp
//
// Player classes: the class a player spawns with (weapons and backpack), class requests to the server and changes.
#pragma once

#include "volition/rfg/code/objects/humans/human_info.h"
#include "volition/vlib/code/util/varray.h"

struct inv_item_info;
struct sender;

// size 0x138
struct mp_player_class_info
{
    bool is_unique_weapon_info(const inv_item_info* inv_info) const; // 0x00951160 (:169)
    mp_player_class_info(); // 0x0098C420

    char name[32]; // 0x00
    char display_name[32]; // 0x20
    int max_hit_points; // 0x40
    int max_knockdown_hits; // 0x44
    float max_speed; // 0x48
    farray<human_info_inventory_info,15> default_inventory; // 0x4C
    inv_item_info* default_equip_item; // 0x110
    inv_item_info* default_last_equip_item; // 0x114
    float health_regen_factor; // 0x118
    inv_item_info* class_unique_weapon; // 0x11C
    bool see_enemies_on_minimap; // 0x120
    float headshot_damage_multiplier; // 0x124
    bool see_wounded_teammates_on_minimap; // 0x128
    float light_damage_threshold; // 0x12C
    float medium_damage_threshold; // 0x130
    float heavy_damage_threshold; // 0x134
};

void multi_game_classes_system_init(); // 0x009DD370 (:193)
const char* multi_game_classes_get_current_class_name(); // 0x0097ADC0 (:206)
const char* multi_game_classes_get_desired_class_name(); // 0x00997590 (:217)
void multi_game_classes_cycle_to_next_available_class(); // 0x009DD3C0 (:239)
const mp_player_class_info* multi_game_classes_get_default_class(); // 0x009511B0 (:278)
void multi_game_classes_send_class_request(const char* class_name); // 0x009BCF40 (:286)
void multi_game_classes_process_class_request(unsigned char* packet_data, const sender& from); // 0x009BD000 (:301)
void multi_game_classes_process_class_change(unsigned char* packet_data, const sender& from); // 0x0097ADD0 (:322)
