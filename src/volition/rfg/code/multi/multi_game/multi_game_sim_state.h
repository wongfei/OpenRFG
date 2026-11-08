// Original: volition/rfg/code/multi/multi_game/multi_game_sim_state.h
// Implementation: volition/rfg/code/multi/multi_game/multi_game_sim_state.cpp
//
// The multiplayer state snapshots that objects embed: multi_sim_state<T> keeps the last 25 received states of a remote
// object and interpolates or extrapolates them; multi_object_state is position, velocities, orientation and network time,
// the derived states add per-class data. network_to_game_handle caches the game handle of a network handle. Placement of
// the state structs inferred.
#pragma once

#include "volition/rfg/code/multi/multi_game/multi_game_client_predict.h"
#include "volition/rfg/code/objects/humans/human_enums.h"
#include "volition/vlib/code/math/matrix.h"
#include "volition/vlib/code/math/quaternion.h"
#include "volition/vlib/code/math/vector2.h"
#include "volition/vlib/code/math/vector3.h"
#include "volition/vlib/code/time/timestamp.h"

class timestamp;

// size 0x4C
struct multi_object_state
{
    multi_object_state(); // 0x009514A0

    vector position; // 0x00
    vector linear_velocity; // 0x0C
    vector angular_velocity; // 0x18
    matrix orientation; // 0x24
    unsigned int network_time; // 0x48
};

// size 0x2
struct mp_ladder_state
{
    union
    {
        struct
        {
            unsigned char l_state; // 0x00
            unsigned char l_rung; // 0x01
        };
        unsigned short data; // 0x00
    };
};

// size 0x8
class network_to_game_handle
{
public:
    network_to_game_handle(unsigned short net_handle); // 0x00963B70 (:30)
    network_to_game_handle(); // 0x00963B50 (:26)
    void set_network_handle(unsigned short net_handle); // 0x00963BA0 (:39)
    unsigned int get_game_handle() const; // 0x009D2D20 (:48)
    unsigned short get_network_handle() const; // 0x00963BC0 (:63)
    void clear(); // 0x00963B90 (:33)

private:
    unsigned short m_network_handle; // 0x00
    unsigned int m_game_handle; // 0x04
};

// size 0xAC
struct multi_player_state : public multi_object_state
{
    multi_player_state(); // 0x0097AF00

    matrix camera_orient; // 0x4C
    vector2 camera_angular_velocity; // 0x70
    float forward; // 0x78
    float sideways; // 0x7C
    float hit_points_pct; // 0x80
    unsigned int ladder_handle; // 0x84
    human_ragdoll_state ragdoll_state; // 0x88
    int knockdown_timestamp; // 0x8C
    network_to_game_handle turret_handle; // 0x90
    network_to_game_handle vehicle_handle; // 0x98
    unsigned char vehicle_seat; // 0xA0
    mp_ladder_state ladder_state; // 0xA2
    union
    {
        struct
        {
            unsigned int equip_weapon_slot : 3; // 0xA4:0
            unsigned int do_sprint : 1; // 0xA4:3
            unsigned int jump_id : 2; // 0xA4:4
            unsigned int is_on_ladder : 1; // 0xA4:6
            unsigned int reload : 1; // 0xA4:7
            unsigned int jetpack : 1; // 0xA4:8
            unsigned int is_zoomed : 1; // 0xA4:9
            unsigned int dont_deform_bones : 1; // 0xA4:10
            unsigned int in_air_detonate : 1; // 0xA4:11
            unsigned int running_attack : 1; // 0xA4:12
            unsigned int detonated_charges : 1; // 0xA4:13
            unsigned int charge_weapon : 2; // 0xA4:14
            unsigned int melee_primary_id : 2; // 0xA4:16
            unsigned int melee_secondary_id : 2; // 0xA4:18
            unsigned int melee_tertiary_id : 2; // 0xA4:20
            unsigned int melee_firearm_id : 2; // 0xA4:22
            unsigned int firearm_trigger_down_id : 2; // 0xA4:24
            unsigned int firearm_trigger_up_id : 2; // 0xA4:26
            unsigned int throw_attack_id : 2; // 0xA4:28
            unsigned int is_crouched : 1; // 0xA4:30
            unsigned int pad_bits : 1; // 0xA4:31
        };
        unsigned int data; // 0xA4
    };
    unsigned short weapon_magazine_ammo; // 0xA8
    unsigned short weapon_reserve_ammo; // 0xAA
};

// size 0x50
struct multi_mover_state : public multi_object_state
{
    multi_mover_state(); // 0x0095A970

    bool fade_delete : 1; // 0x4C:0
    bool movement_fixed : 1; // 0x4C:1
    unsigned short position_ref_index; // 0x4E
};

// size 0x6C
struct multi_projectile_state : public multi_object_state
{
    multi_projectile_state(); // 0x0095BAF0

    struct
    {
        unsigned char attach_type : 2; // 0x00:0
        unsigned char fading_out : 1; // 0x00:2
        unsigned char no_sync : 1; // 0x00:3
    } projectile_flags; // 0x4C
    unsigned short parent_net_handle; // 0x4E
    unsigned int constraint_handle; // 0x50
    vector constraint_pos; // 0x54
    short_quat constraint_orient; // 0x60
    short parent_prop_point; // 0x68
    short child_prop_point; // 0x6A
};

// size 0x50
struct multi_item_state : public multi_object_state
{
    multi_item_state(); // 0x00959670

    bool dont_render : 1; // 0x4C:0
    bool lighting_set_once : 1; // 0x4C:1
    bool simulating : 1; // 0x4C:2
};

// size 0x64
struct multi_turret_state : public multi_object_state
{
    multi_turret_state(); // 0x0097AEE0

    network_to_game_handle ng_current_operator_handle; // 0x4C
    vector2 target_orient_ph; // 0x54
    unsigned int hit_points; // 0x5C
    bool firing; // 0x60
};

// size 0x134
struct multi_automobile_state : public multi_object_state
{
    multi_automobile_state(); // 0x0097AEC0

    vehicle_controls v_controls; // 0x4C
    unsigned int hp; // 0x58
    unsigned int maxhp; // 0x5C
    unsigned int component_count; // 0x60
    bool component_unlatched[66]; // 0x64
    bool component_detached[66]; // 0xA6
    bool component_shattered[66]; // 0xE8
    unsigned char flat_tire; // 0x12A
    unsigned char lights_bitmask[7]; // 0x12B
    bool on_fire : 1; // 0x132:0
    bool smoking : 1; // 0x132:1
};

// size 0x5
struct walker_attack_data
{
    unsigned char attack_1_id; // 0x00
    unsigned char attack_2_id; // 0x01
    unsigned char attack_3_id; // 0x02
    unsigned char attack_4_id; // 0x03
    unsigned char charge_id; // 0x04
};

// size 0x13C
struct multi_walker_state : public multi_object_state
{
    multi_walker_state(); // 0x0097AED0

    vector steering_vector; // 0x4C
    bool turning_left : 1; // 0x58:0
    bool turning_right : 1; // 0x58:1
    unsigned int hp; // 0x5C
    unsigned int maxhp; // 0x60
    unsigned int component_count; // 0x64
    bool component_unlatched[66]; // 0x68
    bool component_detached[66]; // 0xAA
    bool component_shattered[66]; // 0xEC
    unsigned char lights_bitmask[7]; // 0x12E
    walker_attack_data attack_data; // 0x135
    bool on_fire : 1; // 0x13A:0
    bool smoking : 1; // 0x13A:1
    bool at_rest : 1; // 0x13A:2
};

// size (<multi_player_state>) 0x10D4
template<typename T>
class multi_sim_state
{
public:
    void reset(); // <multi_player_state> 0x00968BB0 (:106)
    void add_state(T& state); // <multi_player_state> 0x00968BD0 (:112)
    bool get_latest_state(unsigned int& update_time, T* state) const; // <multi_player_state> 0x00968C70 (:180)
    bool obtain_state(unsigned int network_time, T* state, bool& exceeded_extrap_time); // <multi_player_state> 0x00968CB0 (:152)
    void extrapolate_state(unsigned int network_time, T* state, bool& exceeded_extrap_time); // <multi_player_state> 0x00963E80 (:307)
    void interpolate_state(unsigned int network_time, int prev_index, T* state); // <multi_player_state> 0x00963BE0 (:268)
    bool should_apply_correction() const;
    void set_correction_timer(int);
    multi_sim_state(); // <multi_player_state> 0x00A5C790

private:
    int m_states_used; // 0x00
    T m_states[25]; // 0x04
    timestamp m_apply_correction_timestamp; // after m_states (<multi_player_state> 0x10D0)
};

// The state constructors (compiler generated in rfg.exe): nothing to set.
// 0x009514A0
inline multi_object_state::multi_object_state()
{
}

// 0x0095A970
inline multi_mover_state::multi_mover_state()
{
}

// 0x0095BAF0
inline multi_projectile_state::multi_projectile_state()
{
}

// 0x00959670
inline multi_item_state::multi_item_state()
{
}

void multi_game_sim_state_system_init(); // 0x00963BD0 (:96)

// The player's state has its own interpolation and extrapolation (defined in multi_game_sim_state.cpp; declared here so
// that no user instantiates the generic ones first).
template<>
bool multi_sim_state<multi_player_state>::obtain_state(unsigned int network_time, multi_player_state* state, bool& exceeded_extrap_time); // 0x00968CB0 (:152)
template<>
void multi_sim_state<multi_player_state>::extrapolate_state(unsigned int network_time, multi_player_state* state, bool& exceeded_extrap_time); // 0x00963E80 (:307)
template<>
void multi_sim_state<multi_player_state>::interpolate_state(unsigned int network_time, int prev_index, multi_player_state* state); // 0x00963BE0 (:268)

// <multi_player_state> 0x00A5C790: the states and the correction timestamp construct themselves.
template<typename T>
inline multi_sim_state<T>::multi_sim_state()
{
}

// <multi_player_state> 0x00968BB0 (:106)
template<typename T>
inline void multi_sim_state<T>::reset()
{
    m_states_used = 0;
    m_apply_correction_timestamp.invalidate();
}
