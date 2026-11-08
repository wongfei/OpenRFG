// Original: volition/rfg/code/gameplay/activities/activity_mission_vrail_internal.h
//
// The mission vehicle rail activity (a free activity): its patrol, drop points and chase vehicles, and the attacking
// squads it spawns.
#pragma once

#include "volition/rfg/code/gameplay/activities/activity_internal.h"
#include "volition/rfg/code/gameplay/activities/vrail_common.h"
#include "volition/rfg/code/gameplay/activities/vrail_system.h"
#include "volition/vlib/code/math/vector3.h"
#include "volition/vlib/code/util/varray.h"

class npc;
class object_patrol;
class object_squad;
class vehicle;
class vehicle_rail_definition;

// size 0xD48
class activity_mission_vrail_data : public activity_base_free
{
public:
    activity_mission_vrail_data(); // 0x007A0260 (.h:15)
    virtual void system_init(); // vtbl+0x1C 0x00759130 (activity_mission_vrail.cpp:76)
    virtual void level_init(); // vtbl+0x24 0x007B2650 (activity_mission_vrail.cpp:136)
    virtual bool can_initialize() const; // vtbl+0x10 0x007A1D60 (activity_mission_vrail.cpp:144)
    virtual bool activity_start(); // vtbl+0x38 0x007ADDB0 (activity_mission_vrail.cpp:190)
    virtual void activity_end(); // vtbl+0x3C 0x007ADEA0 (activity_mission_vrail.cpp:234)
    virtual void activity_kill(); // vtbl+0x40 0x00798CA0 (activity_mission_vrail.cpp:265)
    virtual void activity_do_frame_ready(); // vtbl+0x58 0x007B27B0 (activity_mission_vrail.cpp:386)
    virtual void activity_do_frame_active(); // vtbl+0x60 0x007B8160 (activity_mission_vrail.cpp:299)
    virtual bool on_alert(const npc* np) const; // vtbl+0xA4 0x00759140 (activity_mission_vrail.cpp:165)
    virtual void render_icons() const; // vtbl+0x74 0x00783600 (activity_mission_vrail.cpp:122)
    bool init(const vehicle_rail_definition* vrail_def, const object_patrol* parent_patrol, const vector& drop_pos, object_squad* squad, vehicle* vp); // 0x007ADFE0 (activity_mission_vrail.cpp:461)
    bool is_ready() const; // 0x00754270 (.h:60)
    unsigned int get_patrol() const; // 0x00754280 (.h:63)
    unsigned int get_patrol_squad() const; // 0x00754290 (.h:66)
    unsigned int get_patrol_vehicle() const; // 0x0077E430 (.h:67)
    bool get_drop_point(vector& drop_pos) const; // 0x00759170 (activity_mission_vrail.cpp:449)
    vrail_patrol_state get_patrol_state() const; // 0x00759160 (activity_mission_vrail.cpp:443)
    void set_chase_vehicle_count(int new_count); // 0x0077E460 (.h:76)
    void flag_as_complete();
    int get_squads(base_array<object_squad*>& squad_list); // 0x00798CD0 (activity_mission_vrail.cpp:559)
    void stop_squad_spawning(); // 0x007A0390 (.h:84)
protected:
    void reset(); // 0x007ADD60 (activity_mission_vrail.cpp:176)
    void clear_squad_flags() const; // 0x00759150 (activity_mission_vrail.cpp:275)
    void render_icon_player() const; // 0x00783580 (activity_mission_vrail.cpp:82)
    void render_icon_attackers() const; // 0x007835A0 (activity_mission_vrail.cpp:103)
    object_squad* spawn_squad(const object_patrol* patrol); // 0x007B26B0 (activity_mission_vrail.cpp:358)
public:
    virtual ~activity_mission_vrail_data(); // vtbl+0x00 0x0077E490

protected:
    shared_vrail_patrol_data patrol_data; // 0x448
    shared_vrail_spawn_data spawn_data; // 0x474
    const vehicle_rail_definition* current_vrail_def; // 0xD30
    unsigned int vrail_patrol_handle; // 0xD34
    vector drop_off_pos; // 0xD38
    unsigned int squad_handle; // 0xD44
};

extern activity_mission_vrail_data Mission_vrail_activity_data; // 0x02270D40
