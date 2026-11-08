// Original: volition/rfg/code/weapons/weapon_firing_patterns.h
// Implementation: volition/rfg/code/weapons/weapon_firing_patterns.cpp
//
// NPC firing patterns (weapon_firing_patterns table): burst sizes and intervals at up to eight range control points,
// the minimum effective range by remaining ammo, and modifiers for alert level, vehicle targets and covering fire.
#pragma once


struct xml_element;

// size 0xEC
struct weapon_firing_pattern
{
    // size 0x14
    struct control_point
    {
        float range_pct; // 0x00
        float min_burst_size; // 0x04
        float max_burst_size; // 0x08
        float min_interval; // 0x0C
        float max_interval; // 0x10
    };

    // size 0x8
    struct ammo_point
    {
        float ammo_pct; // 0x00
        float min_effective_range; // 0x04
    };

    // size 0x8
    struct modifier
    {
        void modify(float& mult, float& adj) const; // 0x005E2E10 (.h:42)
        modifier(); // 0x00BD9580 (.h:43)

        float multiplier; // 0x00
        float adjustment; // 0x04
    };

    float get_min_effective_range(float ammo_pct) const; // 0x00BDAC30 (:413)
    int get_burst_size(float range_pct) const; // 0x00BDFEA0 (:443)
    int get_max_burst_size(float range_pct) const; // 0x00BDACF0 (:485)
    int get_firing_interval(float range_pct) const; // 0x00BDAE20 (:514)
    int get_max_firing_interval() const; // 0x00BDAFB0 (:540)
    weapon_firing_pattern(); // 0x00BDABC0 (:392)

    char* name; // 0x00
    control_point points[8]; // 0x04
    int num_control_points; // 0xA4
    ammo_point ammo_points[3]; // 0xA8
    int num_ammo_points; // 0xC0
    modifier alert_mod; // 0xC4
    modifier flyer_mod; // 0xCC
    modifier heavy_vehicle_mod; // 0xD4
    modifier normal_vehicle_mod; // 0xDC
    modifier covering_fire_mod; // 0xE4
};

extern int Num_weapon_firing_patterns; // 0x03882CCC

bool parse_modifier(weapon_firing_pattern::modifier& mod, xml_element* entry, const char* el_name); // 0x00BDA9F0 (:207)
int round_rand_range(float minimum, float maximum); // 0x00BDFE60 (:375)
int sec_to_ms_range(float minimum, float maximum); // 0x00BDAB80 (:380)
const weapon_firing_pattern* weapon_firing_pattern_lookup(const char* name); // 0x00BDAFE0 (:562)
const weapon_firing_pattern* weapon_firing_pattern_from_handle(int handle); // 0x00BDB000 (:573)
int weapon_firing_pattern_get_handle(const weapon_firing_pattern* pattern); // 0x00BDB020 (:585)
void weapon_firing_patterns_table_init(int dlc_id); // 0x00BE0130 (:593)
