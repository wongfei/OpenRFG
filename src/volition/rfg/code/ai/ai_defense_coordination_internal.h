// Original: volition/rfg/code/ai/ai_defense_coordination_internal.h
// Implementation: volition/rfg/code/ai/ai_defense_coordination_internal.cpp
//
// The defense coordination objects: targets and defenders allocated from pools of the object manager, looked up by
// handle.
#pragma once

#include "volition/rfg/code/ai/ai_defense_coordination.h"
#include "volition/vlib/code/memory/object_pool.h"
#include "volition/vlib/code/time/timestamp.h"

class cs_spinlock;

// size 0x2C
class ai_defense_target
{
    friend class ai_defense_object_manager; // pool callbacks and handles (friends are not recorded in rfg.pdb)
public:
    // size 0x1
    struct ai_defense_target_flags
    {
        bool beatdown_line_played : 1; // 0x00:0
        bool acknowledged : 1; // 0x00:1
        bool shooed_away : 1; // 0x00:2
        bool in_progress : 1; // 0x00:3
    };

    unsigned int get_handle() const; // 0x005DC610 (.h:44)
    ai_defense_target(); // 0x005FB570 (.h:45)
private:
    void init(); // 0x005DD3A0 (:62)
    void release(); // 0x005DD400 (:76)

public:
    unsigned int target_handle; // 0x00
    unsigned int threatening_defender; // 0x04
    unsigned int accompanying_defender; // 0x08
    ai_defense_threaten_stages threaten_stage; // 0x0C
    timestamp last_in_area_time; // 0x10
    timestamp stage_timer; // 0x14
    timestamp arbitration_timer; // 0x18
    timestamp no_path_timer; // 0x1C
    ai_defense_target_flags flags; // 0x20
private:
    unsigned int handle; // 0x24
    timestamp keepalive_timer; // 0x28
};

// size 0x18
class ai_defense_defender
{
    friend class ai_defense_object_manager; // pool callbacks and handles (friends are not recorded in rfg.pdb)
public:
    // size 0x1
    struct ai_defense_defender_flags
    {
        bool no_path : 1; // 0x00:0
        bool anchored : 1; // 0x00:1
    };

    unsigned int get_handle() const; // 0x005DC620 (.h:71)
    ai_defense_defender(); // 0x005FB590 (.h:73)
private:
    void init(); // 0x005DD410 (:84)
    void release(); // 0x005DD440 (:93)

public:
    unsigned int defender_handle; // 0x00
    unsigned int current_target; // 0x04
    timestamp failed_timer; // 0x08
    ai_defense_defender_flags flags; // 0x0C
private:
    unsigned int handle; // 0x10
    timestamp keepalive_timer; // 0x14
};

// size 0x830
class ai_defense_object_manager
{
public:
    ai_defense_object_manager(); // 0x0068E9A0 (:110)
    void level_init(); // 0x0068E9E0 (:116)
    void do_frame(); // 0x0068EA70 (:131)
    ai_defense_target* get_new_target(); // 0x0064D3C0 (:224)
    ai_defense_defender* get_new_defender(); // 0x0064D420 (:239)
    ai_defense_defender* find_defender(unsigned int defender_handle); // 0x0064D480 (:256)
    ai_defense_target* find_target(unsigned int target_handle); // 0x0064D540 (:269)
    ai_defense_defender* defender_from_handle(unsigned int handle); // 0x005FE3B0 (:307)
    ai_defense_target* target_from_handle(unsigned int handle); // 0x005FE380 (:282)
    cs_spinlock& get_defenders_cs_wrapper();
    ai_defense_defender* get_first_defender(); // 0x005FB5A0 (.h:108)
    ai_defense_defender* get_next_defender(const ai_defense_defender* prev_defender); // 0x0064C960 (.h:109)
    cs_spinlock& get_targets_cs_wrapper();
    ai_defense_target* get_first_target(); // 0x005FB5D0 (.h:111)
    ai_defense_target* get_next_target(const ai_defense_target* prev_target); // 0x0064C970 (.h:112)
private:
    void free_target_internal(ai_defense_target* target); // 0x00673C50 (:335)
    void free_defender_internal(ai_defense_defender* defender); // 0x00673C70 (:343)
    unsigned int get_new_defense_assignment_handle(int index); // 0x005DD450 (:354)
public:
    ~ai_defense_object_manager(); // 0x00673C40

private:
    unsigned int defense_handle_uid_counter; // 0x00
    object_pool<ai_defense_target,10,1> targets; // 0x10
    object_pool<ai_defense_defender,50,1> defenders; // 0x240
};

extern ai_defense_object_manager Ai_defense_object_manager; // 0x01D9FA00
