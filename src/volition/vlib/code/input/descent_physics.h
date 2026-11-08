// Original: volition/vlib/code/input/descent_physics.h
// Implementation: volition/vlib/code/input/descent_physics.cpp
//
// Descent-style free flight input (six degrees of freedom from mouse and keyboard), used by the debug slew camera.
#pragma once


class matrix;
class vector;

enum desnt_action
{
    DESNT_SPEED_UP = 0,
    DESNT_SPEED_DOWN = 1,
    DESNT_MOVE_UP = 2,
    DESNT_MOVE_DOWN = 3,
    DESNT_MOVE_FORWARD = 4,
    DESNT_MOVE_BACKWARD = 5,
    DESNT_MOVE_LEFT = 6,
    DESNT_MOVE_RIGHT = 7,
    DESNT_YAW_LEFT = 8,
    DESNT_YAW_RIGHT = 9,
    DESNT_PITCH_UP = 10,
    DESNT_PITCH_DOWN = 11,
    DESNT_ROLL_RIGHT = 12,
    DESNT_ROLL_LEFT = 13,
    DESNT_AUTO_LEVEL = 14,
    DESNT_NUM_ACTIONS = 15,
};

enum desnt_analog_stick_dir
{
    DESNT_STICK_DIR_DEFAULT = -3,
    DESNT_STICK_DIR_NO_CHANGE = -2,
    DESNT_STICK_DIR_UNBOUND = -1,
    DESNT_LEFT_STICK_DIR_UP = 0,
    DESNT_LEFT_STICK_DIR_DOWN = 1,
    DESNT_LEFT_STICK_DIR_RIGHT = 2,
    DESNT_LEFT_STICK_DIR_LEFT = 3,
    DESNT_RIGHT_STICK_DIR_UP = 4,
    DESNT_RIGHT_STICK_DIR_DOWN = 5,
    DESNT_RIGHT_STICK_DIR_RIGHT = 6,
    DESNT_RIGHT_STICK_DIR_LEFT = 7,
};

enum desnt_mouse_dir
{
    DESNT_MOUSE_DIR_DEFAULT = -3,
    DESNT_MOUSE_DIR_NO_CHANGE = -2,
    DESNT_MOUSE_DIR_UNBOUND = -1,
    DESNT_MOUSE_DIR_RIGHT = 0,
    DESNT_MOUSE_DIR_LEFT = 1,
    DESNT_MOUSE_DIR_UP = 2,
    DESNT_MOUSE_DIR_DOWN = 3,
    DESNT_MOUSE_DIR_WHEEL_UP = 4,
    DESNT_MOUSE_DIR_WHEEL_DOWN = 5,
};

// size 0x1C
struct desnt_binding_info
{
    desnt_action action; // 0x00
    int controller_button; // 0x04, -1 unbound; in descent_physics_set_action_bindings -2 no change, -3 default
    int key; // 0x08
    desnt_analog_stick_dir stick_dir; // 0x0C
    desnt_mouse_dir mouse_dir; // 0x10
    int mouse_button; // 0x14
    float scaling_factor; // 0x18, 0 in descent_physics_set_action_bindings: no change
};

void unused_dcf_mouse_sensitivity(); // 0x005B7880 (:91)
void descent_physics_init(float max_vel, float max_rotvel, float damp, float rotdamp); // 0x00599980 (:218)
void descent_physics_move(vector& pos, matrix& orient, float frametime, int controller_id, bool invert_y); // 0x005B78D0 (:251)
void descent_physics_sim(float damping, float desired_vel, float initial_vel, float t, float* new_vel, float* delta_pos); // 0x00566BD0 (:387)
void descent_physics_set_action_bindings(desnt_binding_info* new_bindings, int num_bindings); // 0x00566CF0 (:415)
void descent_physics_set_scaling_factor(desnt_action action, float scaling_factor); // 0x00566E60 (:454)
void descent_physics_set_max_velocity(float new_max_vel); // 0x00566E80 (:460)
void descent_physics_slew_clamp(bool clamp); // 0x00566EA0 (:468)
bool descent_physics_is_slew_clamped(); // 0x00566EB0 (:474)
void descent_physics_set_damping(float damping, float rot_damping); // 0x00566EC0 (:480)
void descent_physics_keyboard_enable(); // 0x00566EE0 (:488)
void descent_physics_keyboard_disable(); // 0x00566EF0 (:495)
void descent_physics_mouse_enable(); // 0x00566F00 (:502)
void descent_physics_mouse_disable(); // 0x00566F10 (:509)
void descent_physics_position_controls_enable(); // 0x00566F20 (:516)
void descent_physics_position_controls_disable(); // 0x00566F30 (:523)
void descent_physics_rotation_controls_enable(); // 0x00566F40 (:530)
void descent_physics_rotation_controls_disable(); // 0x00566F50 (:537)
void descent_physics_auto_level_lock(); // 0x00566F60 (:544)
void descent_physics_auto_level_unlock(); // 0x00566F70 (:551)
