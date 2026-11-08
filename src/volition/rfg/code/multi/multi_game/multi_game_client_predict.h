// Original: volition/rfg/code/multi/multi_game/multi_game_client_predict.h
// Implementation: volition/rfg/code/multi/multi_game/multi_game_client_predict.cpp
//
// Client prediction: the client's controls (c_command: control flags, sticks, vehicle controls) sent to the server every
// frame and applied there.
#pragma once

#include "volition/vlib/code/math/matrix.h"
#include "volition/vlib/code/math/vector2.h"
#include "volition/vlib/code/math/vector3.h"

class player;
struct sender;

// size 0x4
struct control_flags
{
    union
    {
        struct
        {
            unsigned int do_stop : 1; // 0x00:0
            unsigned int do_sprint : 1; // 0x00:1
            unsigned int desired_weapon_slot : 4; // 0x00:2
            unsigned int attack_type : 2; // 0x00:6
            unsigned int jump : 1; // 0x00:8
            unsigned int climb : 1; // 0x00:9
            unsigned int accelerate : 8; // 0x00:10
            unsigned int brake_reverse : 1; // 0x00:18
            unsigned int handbrake : 1; // 0x00:19
            unsigned int honk : 1; // 0x00:20
            unsigned int action : 1; // 0x00:21
            unsigned int pickup : 1; // 0x00:22
            unsigned int reload : 1; // 0x00:23
            unsigned int zoomed : 1; // 0x00:24
            unsigned int is_crouching : 1; // 0x00:25
            unsigned int is_taking_mp_drug : 1; // 0x00:26
            unsigned int ragdolled : 1; // 0x00:27
            unsigned int do_pant : 1; // 0x00:28
            unsigned int is_combat_blocking : 1; // 0x00:29
            unsigned int taunt : 2; // 0x00:30
        };
        unsigned int data; // 0x00
    };
};

// size 0xC
struct vehicle_controls
{
    vehicle_controls(const vehicle_controls&);
    vehicle_controls(); // 0x00941D00 (.h:51)

    union
    {
        struct
        {
            bool brake_reverse : 1; // 0x00:0
            bool handbrake : 1; // 0x00:1
            bool honk : 1; // 0x00:2
        };
        unsigned char controls; // 0x00
    };
    float accelerate; // 0x04
    float steering; // 0x08
};

// size 0xB0
struct c_command
{
    c_command(); // 0x00A104E0

    unsigned short command_id; // 0x00
    float forward; // 0x04
    float sideways; // 0x08
    bool _do_stop; // 0x0C
    bool _do_sprint; // 0x0D
    bool _is_crouching; // 0x0E
    matrix camera_orient; // 0x10
    vector camera_pos; // 0x34
    unsigned int network_time; // 0x40
    int equipped_weapon_slot; // 0x44
    unsigned char equipped_weapon_index; // 0x48
    unsigned char equipped_weapon_magazine_rounds; // 0x49
    unsigned short equipped_weapon_rounds_in_reserve; // 0x4A
    bool _jump; // 0x4C
    bool start_jump; // 0x4D
    bool climb; // 0x4E
    bool action; // 0x4F
    bool pickup; // 0x50
    bool _reload; // 0x51
    bool is_combat_blocking; // 0x52
    vehicle_controls vcontrols; // 0x54
    bool set_turnto_target; // 0x60
    float throw_pitch; // 0x64
    float throw_strength; // 0x68
    control_flags flags; // 0x6C
    vector pos; // 0x70
    vector linear_vel; // 0x7C
    vector angular_vel; // 0x88
    vector2 local_xz_vel; // 0x94
    bool in_car; // 0x9C
    bool received_vehicle_control_this_frame; // 0x9D
    bool correcting; // 0x9E
    const player* targeted_player; // 0xA0
    vector movement_dir; // 0xA4
};

void multi_game_cp_send_client_controls(); // 0x009BD560 (:627)
void multi_game_cp_process_client_controls(unsigned char* packet_data, const sender& from); // 0x009DDB90 (:835)

// 0x00A104E0: nothing to construct.
inline c_command::c_command()
{
}

// 0x00941D00 (.h:51): nothing to construct.
inline vehicle_controls::vehicle_controls()
{
}
