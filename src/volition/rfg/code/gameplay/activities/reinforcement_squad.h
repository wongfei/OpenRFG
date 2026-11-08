// Original: volition/rfg/code/gameplay/activities/reinforcement_squad.h
// Implementation: volition/rfg/code/gameplay/activities/reinforcement_squad.cpp
//
// A reinforcement squad: spawned (or simulated offscreen) towards a target, updates its orders and target beliefs and is
// released when it dies or despawns. Held by value by the game flow modules.
#pragma once

#include "volition/vlib/code/time/timestamp.h"
#include "volition/vlib/code/util/varray.h"

class human;
enum npc_despawn_type;
class object;
class object_squad;
class squad_spawn_parameters;
class vector;

// size 0x60
class reinforcement_squad
{
public:
    reinforcement_squad(); // 0x0077E2D0 (.h:24)
    void do_frame(); // 0x007AF540 (:140)
private:
    void despawn_squad(object_squad* squad, npc_despawn_type despawn_type); // 0x0078A2C0 (:281)
public:
    void despawn_squad(npc_despawn_type despawn_type); // 0x0079B570 (:270)
    void release_and_reset(); // 0x0079B5A0 (:348)
    bool spawn_squad(unsigned int new_target_handle, squad_spawn_parameters& spawn_params, bool use_reinforcement_updates, bool destroy_partial); // 0x007B4210 (:57)
    bool squad_despawned() const; // 0x00771DA0 (:399)
    bool get_squad_position(vector& squad_pos) const; // 0x00771D10 (:373)
    bool despawning_started() const;
    int get_deathtime() const; // 0x0075BAB0 (:355)
    bool squad_in_use() const; // 0x00754160 (.h:56)
private:
    void reset(); // 0x007720A0 (:587)
    void swap_to_dead_state(); // 0x007A9DB0 (:190)
    void do_frame_simulated(object_squad* squad, object* target_objp); // 0x0078A330 (:569)
    void do_frame_spawned(object_squad* squad, object* target_objp); // 0x0079B400 (:209)
    void update_orders(const object* target_objp); // 0x007A3130 (:111)
    void update_target_beliefs(object_squad* squad, object_squad* target_squad); // 0x00771C90 (:326)
    void update_target_beliefs(object_squad* squad, human* target_hp); // 0x00771C00 (:303)
    void update_target_beliefs(object_squad* squad, object* target_objp); // 0x00771E30 (:430)
    bool can_engage_player(object_squad* squad) const; // 0x00771FA0 (:516)
    bool can_engage_target(object_squad* squad, object_squad* target_squad) const; // 0x00771F20 (:477)
    bool can_engage_target(object_squad* squad, human* target_hp) const; // 0x00771EA0 (:456)
    bool can_engage_target(object_squad* squad, object* target_objp) const; // 0x00772010 (:537)
    bool is_closing_to_target(const vector& squad_pos, const vector& target_pos); // 0x0075BAE0 (:499)
public:
    ~reinforcement_squad(); // 0x007710E0

    unsigned int squad_handle; // 0x00
    unsigned int vehicle_handle; // 0x04
    unsigned int orders_handle; // 0x08
    unsigned int target_handle; // 0x0C
private:
    float dist_to_target_sqrd; // 0x10
    timestamp despawn_timeout; // 0x14
    timestamp squad_lifetime; // 0x18
    timestamp squad_deathtime; // 0x1C
    timestamp belief_refresh; // 0x20
    farray<unsigned int,10> despawning_members; // 0x24
    struct
    {
        unsigned int squad_spawned : 1; // 0x00:0
        unsigned int target_is_player : 1; // 0x00:1
        unsigned int flyer_squad : 1; // 0x00:2
        unsigned int reinforcement_update : 1; // 0x00:3
        unsigned int squad_killed : 1; // 0x00:4
        unsigned int squad_despawned : 1; // 0x00:5
        unsigned int vehicle_killed : 1; // 0x00:6
        unsigned int destroy_partial_squads : 1; // 0x00:7
    } reinforcement_squad_flags; // 0x5C
};
