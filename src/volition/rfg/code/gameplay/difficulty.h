// Original: volition/rfg/code/gameplay/difficulty.h
// Implementation: volition/rfg/code/gameplay/difficulty.cpp
//
// Difficulty: the difficulty level (casual to insane) and the per-item values read from the difficulty table, saved
// with the game.
#pragma once


struct cfile;

enum difficulty_items
{
    DIFFICULTY_ENEMY_DAMAGE_TAKEN = 0,
    DIFFICULTY_ENEMY_VEHICLE_DAMAGE = 1,
    DIFFICULTY_ENEMY_WEAPON_SPREAD = 2,
    DIFFICULTY_ENEMY_AIM_DRIFT = 3,
    DIFFICULTY_ENEMY_BURST_SIZE = 4,
    DIFFICULTY_ENEMY_REINFORCEMENTS = 5,
    DIFFICULTY_ENEMY_ALERT = 6,
    DIFFICULTY_PLAYER_DAMAGE_TAKEN = 7,
    DIFFICULTY_PLAYER_REGEN_DELAY = 8,
    DIFFICULTY_PLAYER_REGEN_RATE = 9,
    DIFFICULTY_PLAYER_AMMO_PICKUP = 10,
    DIFFICULTY_PLAYER_AMMO_CRATES = 11,
    DIFFICULTY_PLAYER_VEHICLE_DAMAGE = 12,
    DIFFICULTY_PLAYER_VEHICLE_MASS = 13,
    DIFFICULTY_PLAYER_DEATH_BUFFER = 14,
    DIFFICULTY_EXPLOSION_DAMAGE = 15,
    DIFFICULTY_PHYSICS_COLLISIONS = 16,
    DIFFICULTY_FALLING_DAMAGE = 17,
    DIFFICULTY_UPGRADE_COST = 18,
    DIFFICULTY_MORALE_BOOSTS = 19,
    DIFFICULTY_MORALE_PENALITIES = 20,
    DIFFICULTY_MORALE_PLAYER_DEATH = 21,
    DIFFICULTY_CONTROL_PARKER = 22,
    DIFFICULTY_CONTROL_DUST = 23,
    DIFFICULTY_CONTROL_BADLANDS = 24,
    DIFFICULTY_CONTROL_OASIS = 25,
    DIFFICULTY_CONTROL_EOS = 26,
    NUM_DIFFICULTY_ITEMS = 27,
};

enum difficulty_levels
{
    DIFFICULTY_LEVEL_INVALID = -1,
    DIFFICULTY_LEVEL_EASY = 0,
    DIFFICULTY_LEVEL_MEDIUM = 1,
    DIFFICULTY_LEVEL_HARD = 2,
    DIFFICULTY_LEVEL_INSANE = 3,
    DIFFICULTY_LEVEL_CUSTOM = 4,
    NUM_DIFFICULTY_LEVELS = 5,
};

enum difficulty_choices
{
    DIFFICULTY_CASUAL = 0,
    DIFFICULTY_NORMAL = 1,
    DIFFICULTY_HARD = 2,
    DIFFICULTY_GAME_NOT_COMPLETED = 3,
    DIFFICULTY_INSANE = 3,
    NUM_DIFFICULTY_CHOICES = 4,
};

// size 0x14
struct difficulty_values
{
    float level[5]; // 0x00
};

// size 0x8
struct difficulty_index_type
{
    const char* name; // 0x00
    difficulty_items index; // 0x04
};

void unused_dcf_difficulty_level(); // 0x00772D10 (:81)
void difficulty_system_init(); // 0x0075CEE0 (:184)
void difficulty_set_level(difficulty_levels level); // 0x0075CF00 (:190)
difficulty_levels difficulty_get_level(); // 0x0075CF20 (:197)
float difficulty_get_value(difficulty_items diff_item, difficulty_levels level); // 0x00772E10 (:205)
void difficulty_change_value(difficulty_items diff_item, float new_value); // 0x00772E50 (:229)
void difficulty_set_level_reload_overwrite(difficulty_levels level); // 0x0075CF30 (:240)
void difficulty_save(cfile* save_file); // 0x0075CF40 (:245)
void difficulty_load(cfile* save_file); // 0x0075CF90 (:260)
void unused_dcf_reload_difficulty_table(); // 0x0075D000 (:286)
