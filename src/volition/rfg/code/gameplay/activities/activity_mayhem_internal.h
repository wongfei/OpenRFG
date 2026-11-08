// Original: volition/rfg/code/gameplay/activities/activity_mayhem_internal.h
// Implementation: volition/rfg/code/gameplay/activities/activity_mayhem.cpp
//
// The mayhem activity (a free activity): scores the damage the player does to buildings, vehicles and people within a
// time limit, in levels (Mayhem_event_level). Placement of mayhem_level inferred.
#pragma once

#include "volition/rfg/code/gameplay/activities/activity_internal.h"
#include "volition/vlib/code/time/timestamp.h"

struct explosion_info;
class object;
class rfg_mover;

// size 0x8
struct mayhem_level
{
    float threshold; // 0x00
    float reward; // 0x04
};

// size 0x468
class activity_mayhem : public activity_base_free
{
public:
    activity_mayhem(); // 0x0077DE50 (.h:11)
    virtual void system_init(); // vtbl+0x1C 0x00758DB0 (activity_mayhem.cpp:211)
    virtual void level_init(); // vtbl+0x24 0x00758F00 (activity_mayhem.cpp:255)
    virtual bool activity_start(); // vtbl+0x38 0x007ADC40 (activity_mayhem.cpp:266)
    virtual void activity_end(); // vtbl+0x3C 0x007ADC90 (activity_mayhem.cpp:281)
    virtual void activity_kill(); // vtbl+0x40 0x00798C80 (activity_mayhem.cpp:300)
    virtual void activity_reset(); // vtbl+0x04 0x00758F40 (activity_mayhem.cpp:308)
    virtual void activity_do_frame_inactive(); // vtbl+0x50 0x0076E8E0 (activity_mayhem.cpp:324)
    virtual void activity_do_frame_active(); // vtbl+0x60 0x00758FD0 (activity_mayhem.cpp:337)
    virtual bool render_hud(int x_pos, int y_pos); // vtbl+0x70 0x0076E940 (activity_mayhem.cpp:349)
    virtual const char* get_title() const; // vtbl+0x80 0x00758FF0 (activity_mayhem.cpp:394)
    virtual const char* get_icon_image_name() const; // vtbl+0x88 0x00759000 (activity_mayhem.cpp:399)
    virtual bool get_ui_display_info(char* value_text, int& time_til_end, int buffer_size) const; // vtbl+0xCC 0x00759010 (activity_mayhem.cpp:405)
    virtual void register_damage(const object* obj_a, const object* obj_b, float damage, int original_hitpoints); // vtbl+0xB8 0x007989E0 (activity_mayhem.cpp:103)
    virtual void register_death(const object* target_obj, const object* attacker_obj); // vtbl+0xBC 0x00798A50 (activity_mayhem.cpp:126)
    virtual void register_building_damage(const rfg_mover* objp, float damage_percent_delta, float original_damage_percent); // vtbl+0xC0 0x00798B50 (activity_mayhem.cpp:157)
    virtual void register_building_destroyed(const rfg_mover* objp); // vtbl+0xC4 0x00798BD0 (activity_mayhem.cpp:171)
    virtual void register_explosion(const object* owner, const explosion_info* p_explosion_info); // vtbl+0xC8 0x00783540 (activity_mayhem.cpp:196)
    float get_current_score() const; // 0x00753EA0 (.h:59)
private:
    void add_time(int period_ms); // 0x0076EB00 (activity_mayhem.cpp:428)
    void add_to_score(float value, float scale); // 0x0076EB90 (activity_mayhem.cpp:444)
    int get_level() const; // 0x00759070 (activity_mayhem.cpp:452)
    bool count_object_damage(const object* op) const; // 0x007590A0 (activity_mayhem.cpp:461)
public:
    virtual ~activity_mayhem(); // vtbl+0x00 0x00768D40

private:
    float total_score; // 0x448
    int humans_killed; // 0x44C
    int structures_destroyed; // 0x450
    int vehicles_destroyed; // 0x454
    int explosions_caused; // 0x458
    timestamp mayhem_timer; // 0x45C
    timestamp total_time; // 0x460
    float high_score; // 0x464
};

extern activity_mayhem Mayhem_activity; // 0x01653700
