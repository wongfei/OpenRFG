// Original: volition/rfg/code/gameplay/activities/activity_raid_attack_internal.h
//
// The offensive raid: waiting for a start, picking the raid node (raid_node_selection_info), new raid squads and the
// vehicle voice lines.
#pragma once

#include "volition/rfg/code/gameplay/activities/activity_raid_internal.h"
#include "volition/rfg/code/objects/humans/human_teams.h"
#include "volition/vlib/code/math/vector3.h"
#include "volition/vlib/code/time/timestamp.h"

class base_raid_definition_data;
class npc;
class object;
class object_activity_spawn;
class object_raid_node;
class object_squad;

// size 0x10
struct raid_node_selection_info
{
    const object* node_objp; // 0x00
    const object_raid_node* raid_node; // 0x04
    const object_activity_spawn* node; // 0x08
    float dist_sqrd; // 0x0C
};

// size 0x1F70
class offensive_raid_data : public activity_raid_data
{
public:
    offensive_raid_data(); // 0x0077E1A0 (.h:29)
    virtual void activity_do_frame_inactive(); // vtbl+0x50 0x007B6CF0 (activity_raid_attack.cpp:784)
    virtual void activity_do_frame_ready(); // vtbl+0x58 0x00799A80 (activity_raid_attack.cpp:591)
    virtual void level_init(); // vtbl+0x24 0x007851F0 (activity_raid_attack.cpp:581)
    bool wait_do_frame_active(); // 0x007850D0 (activity_raid_attack.cpp:280)
    void wait_do_frame_inactive(); // 0x007B6C40 (activity_raid_attack.cpp:486)
    virtual bool activity_initialize(const object* spawn_objp, unsigned int new_district_handle); // vtbl+0x2C 0x007B6AC0 (activity_raid_attack.cpp:362)
    virtual void clear_all_flags(); // vtbl+0xE8 0x007999A0 (activity_raid_attack.cpp:459)
    virtual const char* get_title() const; // vtbl+0x80 0x00759C90 (activity_raid_attack.cpp:878)
    virtual const char* get_subtitle() const; // vtbl+0x84 0x00759CB0 (activity_raid_attack.cpp:896)
    virtual const char* get_icon_image_name() const; // vtbl+0x88 0x00759CD0 (activity_raid_attack.cpp:906)
    virtual void activity_kill(); // vtbl+0x40 0x00799BC0 (activity_raid_attack.cpp:869)
    virtual void activity_end(); // vtbl+0x3C 0x007AEAC0 (activity_raid_attack.cpp:859)
    virtual void render_icons() const; // vtbl+0x74 0x00770370 (activity_raid_attack.cpp:418)
    virtual bool is_defending_team(human_teams team) const; // vtbl+0xE0 0x00759C30 (activity_raid_attack.cpp:803)
    virtual bool is_attacking_team(human_teams team) const; // vtbl+0xE4 0x00759C60 (activity_raid_attack.cpp:815)
    void do_raid_clean_up(); // 0x00770430 (activity_raid_attack.cpp:829)
    bool can_start_raid() const; // 0x00770310 (activity_raid_attack.cpp:342)
    bool should_stop_raid() const; // 0x007701E0 (activity_raid_attack.cpp:209)
    bool start_new_raid(); // 0x00799930 (activity_raid_attack.cpp:187)
    void play_vehicle_lines(); // 0x00785360 (activity_raid_attack.cpp:697)
    void render_inactive_icons(); // 0x007853F0 (activity_raid_attack.cpp:730)
    npc* get_raid_start_npc() const; // 0x00785200 (activity_raid_attack.cpp:640)
    npc* get_intro_npc(const object_squad* squad) const; // 0x00770240 (activity_raid_attack.cpp:236)
    void spawn_new_raid_squad(const raid_node_selection_info& spawn_node_info); // 0x007B2BC0 (activity_raid_attack.cpp:514)
    virtual ~offensive_raid_data(); // vtbl+0x00 0x0077E1C0

    struct
    {
        unsigned int wait_for_player : 1; // 0x00:0
        unsigned int played_vehicle_entry : 1; // 0x00:1
        unsigned int played_intro_line : 1; // 0x00:2
        unsigned int gps_triggered : 1; // 0x00:3
    } offensive_raid_flags; // 0x1F48
    unsigned int start_up_squad_handle; // 0x1F4C
    unsigned int raid_spawn_node; // 0x1F50
    unsigned int new_raid_handle; // 0x1F54
    unsigned int start_effect_handle; // 0x1F58
    vector start_up_pos; // 0x1F5C
    timestamp vehicle_line_delay; // 0x1F68
    const base_raid_definition_data* base_raid_def; // 0x1F6C
};

extern offensive_raid_data Offensive_raid; // 0x0226EDC8
