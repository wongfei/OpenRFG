// Original: volition/rfg/code/gameplay/activities/activity_assassin_internal.h
//
// The assassination activity (a free activity running alongside the world): bureaucrats and their bodyguards spawned
// from the assassin table definitions, the office bounding box and the player's cover.
#pragma once

#include "volition/rfg/code/gameplay/activities/activity_internal.h"
#include "volition/vlib/code/math/bbox.h"
#include "volition/vlib/code/math/vector3.h"
#include "volition/vlib/code/time/timestamp.h"
#include "volition/vlib/code/util/varray.h"

class rfg_mover;
struct squad_definition;

// size 0x44
class assassin_definition
{
public:
    assassin_definition(); // 0x0077EBA0 (.h:20)
    void reset(); // 0x007691C0 (.h:28)
    ~assassin_definition(); // 0x0077EBD0

    int num_bureaucrats_to_spawn; // 0x00
    int reward; // 0x04
    int num_bureaucrats_per_level; // 0x08
    farray<squad_definition const*,10> bodyguard_def_list; // 0x0C
};

// size 0x784
class activity_assassin_data : public activity_base_free
{
public:
    activity_assassin_data(); // 0x00795930 (.h:43)
    virtual void system_init(); // vtbl+0x1C 0x007968A0 (activity_assassin.cpp:113)
    virtual void level_init(); // vtbl+0x24 0x007ACD60 (activity_assassin.cpp:125)
    virtual bool activity_start(); // vtbl+0x38 0x007ACD90 (activity_assassin.cpp:133)
    virtual void activity_end(); // vtbl+0x3C 0x007ACDB0 (activity_assassin.cpp:146)
    virtual void activity_kill(); // vtbl+0x40 0x007ACE00 (activity_assassin.cpp:161)
    virtual void activity_do_frame_ready(); // vtbl+0x58 0x007ACE20 (activity_assassin.cpp:186)
    virtual void activity_do_frame_active(); // vtbl+0x60 0x007B5740 (activity_assassin.cpp:173)
    virtual bool is_player_cover_blown() const; // vtbl+0x9C 0x00757280 (activity_assassin.cpp:231)
    virtual void render_icons() const; // vtbl+0x74 0x0076A170 (activity_assassin.cpp:242)
    void report_bureaucrat_running(); // 0x007572A0 (activity_assassin.cpp:344)
private:
    void load_definition_from_table(); // 0x0077FC10 (activity_assassin.cpp:357)
    void reset_activity_instance(); // 0x007A8D30 (activity_assassin.cpp:443)
    bool spawn_bureaucrats(const rfg_mover* obj); // 0x007968C0 (activity_assassin.cpp:463)
    bool spawn_bodyguards(const rfg_mover* obj); // 0x007B17C0 (activity_assassin.cpp:503)
    bool is_local_player_in_office_bbox(); // 0x007572C0 (activity_assassin.cpp:552)
    bool are_bureaucrats_dead();
    void monitor_bureaucrats(); // 0x007B1960 (activity_assassin.cpp:560)
public:
    virtual ~activity_assassin_data(); // vtbl+0x00 0x0077EBE0

    farray<unsigned int,10> orders_handles; // 0x448
private:
    int num_levels; // 0x480
    int current_level; // 0x484
    int num_bureaucrats_killed; // 0x488
    farray<unsigned int,4> bureaucrat_handles; // 0x48C
    unsigned int office_handle; // 0x4AC
    bbox office_bldg_bbox; // 0x4B0
    vector office_pos; // 0x4C8
    int assassin_likelyhood; // 0x4D4
    timestamp assassin_timer; // 0x4D8
    assassin_definition levels[10]; // 0x4DC
};

extern activity_assassin_data Assassin_activity_data; // 0x022458B0
