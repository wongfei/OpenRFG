// Original: volition/rfg/code/gameplay/activities/activity_continuous_attack_internal.h
//
// The continuous attack activity: its definitions (base_continuous_attack_data), the attacking squads and the attack
// type (target or patrol). Placement of cont_attack_type inferred.
#pragma once

#include "volition/rfg/code/gameplay/activities/activity_internal.h"
#include "volition/rfg/code/gameplay/activities/reinforcement_squad.h"
#include "volition/vlib/code/math/vector3.h"
#include "volition/vlib/code/time/timestamp.h"
#include "volition/vlib/code/util/varray.h"

class object;
struct squad_definition;
class vehicle_spawn_group_info;
struct xml_element;

enum cont_attack_type
{
    CONT_ATTACK_TYPE_NONE = 0,
    CONT_ATTACK_TYPE_PATROL = 1,
    NUM_CONT_ATTACK_TYPES = 2,
};

// size 0x14
class continuous_attack_squad_data
{
public:
    continuous_attack_squad_data(); // 0x00769230 (.h:29)
    bool load(xml_element* xml_squad_info); // 0x007573F0 (activity_continuous_attack.cpp:258)
    void reset(); // 0x00754720 (.h:44)

    const squad_definition* squad_def; // 0x00
    const vehicle_spawn_group_info* veh_group; // 0x04
    unsigned int sub_group; // 0x08
    float veh_hp_multiplier; // 0x0C
    struct
    {
        bool stay_in_vehicle : 1; // 0x00:0
    } squad_flags; // 0x10
};

// size 0x168
class base_continuous_attack_data
{
public:
    base_continuous_attack_data(); // 0x0077ED00 (.h:57)
    bool load(xml_element* xml_root); // 0x00780180 (activity_continuous_attack.cpp:296)
    void reset(); // 0x00769250 (.h:82)

    int name_crc; // 0x00
    const char* continuous_attack_name; // 0x04
    int attacker_count; // 0x08
    int spawn_period_ms; // 0x0C
    cont_attack_type type; // 0x10
    bool show_squad_icons; // 0x14
    farray<continuous_attack_squad_data,16> squad_list; // 0x18
};

// size 0xA80
class activity_continuous_attack_data : public activity_base_free
{
public:
    activity_continuous_attack_data(); // 0x00795A70 (.h:115)
    virtual void system_init(); // vtbl+0x1C 0x007A0CD0 (activity_continuous_attack.cpp:753)
    virtual void level_init(); // vtbl+0x24 0x007A8DC0 (activity_continuous_attack.cpp:359)
    virtual bool activity_start(); // vtbl+0x38 0x007ACF90 (activity_continuous_attack.cpp:386)
    virtual void activity_end(); // vtbl+0x3C 0x007A0C90 (activity_continuous_attack.cpp:396)
    virtual void activity_kill(); // vtbl+0x40 0x007A0CB0 (activity_continuous_attack.cpp:405)
    virtual void activity_do_frame_active(); // vtbl+0x60 0x007B7330 (activity_continuous_attack.cpp:414)
    virtual void render_icons() const; // vtbl+0x74 0x0076A360 (activity_continuous_attack.cpp:486)
    void activity_do_frame_active_patrol(); // 0x00780340 (activity_continuous_attack.cpp:515)
    bool attack_init(const object* target_objp, const base_continuous_attack_data* attack_data); // 0x007574D0 (activity_continuous_attack.cpp:810)
    bool is_squad_member(unsigned int squad_handle); // 0x0076A420 (activity_continuous_attack.cpp:707)
    int get_attacker_count() const; // 0x00754740 (.h:149)
    void change_attacker_count(int new_attacker_count); // 0x00754750 (.h:152)
    void set_spawn_group(int spawn_group); // 0x00754760 (.h:154)
    int get_squads(int max_nums, unsigned int* squads); // 0x0076A450 (activity_continuous_attack.cpp:840)
    unsigned int get_target_handle(); // 0x00754770 (.h:158)
private:
    reinforcement_squad* find_unused_slot(); // 0x0076A3F0 (activity_continuous_attack.cpp:565)
    int get_squad_count() const; // 0x0076A340 (activity_continuous_attack.cpp:469)
    void add_new_squad(); // 0x007B5760 (activity_continuous_attack.cpp:624)
    bool pick_squad_index(int& squad_index); // 0x00780530 (activity_continuous_attack.cpp:579)
    void do_frame_update_pos(); // 0x00780670 (activity_continuous_attack.cpp:720)
    void reset(); // 0x007A0BB0 (activity_continuous_attack.cpp:367)
public:
    virtual ~activity_continuous_attack_data(); // vtbl+0x00 0x0077ED60

private:
    unsigned int attack_target_handle; // 0x448
    const base_continuous_attack_data* base_spawn_info; // 0x44C
    farray<reinforcement_squad,16> reinforcement_squad_list; // 0x450
    timestamp polling_timer; // 0xA60
    timestamp spawn_timer; // 0xA64
    vector last_target_pos; // 0xA68
    int attacker_count; // 0xA74
    int num_squad_patrols_finished; // 0xA78
    unsigned int active_sub_group; // 0xA7C
};

extern activity_continuous_attack_data Continuous_attack_data; // 0x01652350
