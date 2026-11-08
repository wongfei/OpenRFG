// Original: volition/rfg/code/gameplay/gameflow/gfm_orders_manager.h
// Implementation: volition/rfg/code/gameplay/gameflow/gfm_orders_manager.cpp
//
// EDF orders: orders to attack or defend a position, building or object (gfm_orders), created with
// gfm_orders_creation_params and assigned to squads; the orders manager keeps them by handle and cleans up old ones.
#pragma once

#include "volition/rfg/code/gameplay/gameflow/gameflow_enums.h"
#include "volition/rfg/code/objects/humans/human_teams.h"
#include "volition/vlib/code/math/vector3.h"
#include "volition/vlib/code/memory/object_pool.h"
#include "volition/vlib/code/time/timestamp.h"
#include "volition/vlib/code/util/varray.h"

class npc;
class object;
class object_squad;
class rfg_mover;

// size 0x1
class gfm_orders_flags
{
public:
    void reset(); // 0x0075DC20 (:84)
    gfm_orders_flags(bool destroy_on_death_, bool limited_reassignment_); // 0x0075DC30 (:89)
    gfm_orders_flags(const gfm_orders_flags&);
    gfm_orders_flags(); // 0x005DDAE0 (.h:71)

    bool destroy_on_death : 1; // 0x00:0
    bool limited_reassignment : 1; // 0x00:1
    bool had_squad_assigned : 1; // 0x00:2
};

// size 0x1C
class gfm_orders_creation_params
{
public:
    gfm_orders_creation_params(); // 0x005DDAF0 (.h:81)
    void reset(); // 0x0075DC60 (:109)
    bool is_valid() const; // 0x0075DC90 (:119)
    human_teams get_team() const; // 0x0075DCC0 (:142)

    gfm_orders_types orders_type; // 0x00
    human_teams team; // 0x04
    vector location_target; // 0x08
    unsigned int object_target; // 0x14
    gfm_orders_flags ofp; // 0x18
};

// size 0x74
class gfm_orders
{
public:
    gfm_orders(); // 0x0077E260 (.h:123)
    void clear_orders(); // 0x0075DCD0 (:152)
    const vector& get_orders_pos() const; // 0x0075DD90 (:194)
    bool create_orders(const gfm_orders_creation_params& creation_params); // 0x0075DD10 (:171)
    void update_orders_position(const vector& new_pos); // 0x0075DDA0 (:201)
    bool reinforcement_orders() const; // 0x0075DD80 (:186)
    void populate_building_list(const farray<object const*,16>& target_building_list); // 0x0078BD10 (:207)
    rfg_mover* get_best_building(const npc* np); // 0x007A5020 (:220)
    bool any_valid_building(); // 0x007738F0 (:273)
    void set_target_object(unsigned int new_object_target); // 0x0075DDC0 (:303)
    void set_target_position(const vector& new_pos); // 0x0075DDD0 (:309)
    void set_orders_type(gfm_orders_types type); // 0x0075DE00 (:315)
    bool has_squad_assigned() const; // 0x0079CA50 (:326)
    object_squad* get_assigned_squad_object() const; // 0x0078BD70 (:332)
    unsigned int get_assigned_squad_handle() const; // 0x0078BDD0 (:347)
    bool limited_reassignment() const;
    bool destroy_when_free() const; // 0x007540E0 (.h:177)
    int time_since_last_squad() const;
    vector get_location() const; // 0x005DDB10 (.h:182)
    unsigned int get_orders_handle() const; // 0x007540F0 (.h:185)
    unsigned int get_target_object_handle() const; // 0x005DDB30 (.h:187)
    object* get_target_object() const; // 0x0075DE10 (:320)
    gfm_orders_types get_orders_type() const; // 0x005DDB40 (.h:204)
    human_teams get_team() const; // 0x00754100 (.h:205)
    void assign_handle(unsigned int new_handle); // 0x00754110 (.h:208)
    bool had_squad() const;

private:
    friend class activity_continuous_attack_data; // do_frame_update_pos moves the location target (inlined)
    friend class activity_house_arrest; // do_frame_squad_management moves the location target (inlined)
    friend class activity_courier_chase_data; // activity_initialize reads the handle (inlined)
    friend class activity_convoy_data; // update_squad_orders_attack moves the location target (inlined)
    unsigned int handle; // 0x00
    unsigned int object_target; // 0x04
    gfm_orders_types orders_type; // 0x08
    human_teams team; // 0x0C
    gfm_orders_flags orders_flags; // 0x10
    vector location_target; // 0x14
    timestamp squad_change_period; // 0x20
    farray<unsigned int,16> building_list; // 0x24
};

// size 0x3CE0
class gfm_orders_manager
{
public:
    gfm_orders_manager(); // 0x007A8070 (.h:259)
    void level_init(); // 0x007A5160 (:426)
    void do_frame(); // 0x007A53E0 (:533)
    gfm_orders* get_orders_from_handle(unsigned int orders_handle); // 0x00773970 (:561)
    unsigned int create_new_orders(const gfm_orders_creation_params& creation_params); // 0x007ABA70 (:594)
    void change_orders_priority(gfm_orders* orders, int new_priority); // 0x0075DE60 (:633)
    void change_orders_priority(unsigned int orders_handle, int new_priority); // 0x007739B0 (:622)
    void destroy_orders(gfm_orders* orders); // 0x0079CA60 (:658)
    void destroy_orders(unsigned int orders_handle); // 0x007A5450 (:646)
    void destroy_free_orders(); // 0x0079CA80 (:665)
    void destroy_all_orders_with_target(unsigned int target_handle); // 0x0079CC70 (:704)
    void destroy_all_orders_of_type(gfm_orders_types orders_type); // 0x0079CBB0 (:688)
    bool ignore_player_distance() const;
    void alert_level_cleared(); // 0x0079CD30 (:720)
private:
    void do_frame_update(); // 0x0078BDF0 (:450)
    void do_frame_death(); // 0x007A5220 (:480)
    void do_frame_clean_up_old(); // 0x007A52D0 (:514)
    bool respawn_squad();
    gfm_orders* get_orders_object(); // 0x007AB9E0 (:370)
    int get_new_instance_id(); // 0x0075DE20 (:399)
    int decompose_orders_index(unsigned int orders_handle) const; // 0x00754120 (.h:331)
    int decompose_orders_instance(unsigned int) const;
    unsigned int build_orders_handle(int index, int instance_id) const; // 0x00754130 (.h:333)
public:
    ~gfm_orders_manager(); // 0x0078BD00

    object_pool<gfm_orders,128,1> master_orders_pool; // 0x00
private:
    struct
    {
        bool orders_dirty : 1; // 0x00:0
    } flags; // 0x3CC0
    int next_orders_instance_id; // 0x3CC4
    timestamp load_npc_timestamp; // 0x3CC8
    timestamp death_polling_timer; // 0x3CCC
    timestamp update_polling_timer; // 0x3CD0
};

extern gfm_orders_manager Orders_manager; // 0x0222DE40, gameplay.cpp
