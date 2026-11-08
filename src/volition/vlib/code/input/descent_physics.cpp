// Original: volition/vlib/code/input/descent_physics.cpp
//
// Partial: everything but the flight itself (descent_physics_move, descent_get_action_factor and the console command
// unused_dcf_mouse_sensitivity). Ghidra lists no callers of descent_physics_move, and camera_free does not call it.

#include "volition/vlib/code/input/descent_physics.h"
#include "volition/vlib/code/math/vector3.h"

#include <math.h>

// 0x012C3098: the default bindings, one per action. Controller buttons, keys (vlib KEY_* scan codes), stick and mouse
// directions; -1 is unbound.
static const desnt_binding_info Desnt_default_bindings[DESNT_NUM_ACTIONS] = {
    {DESNT_SPEED_UP, 4, 0x05, DESNT_STICK_DIR_UNBOUND, DESNT_MOUSE_DIR_UNBOUND, -1, 1.0f},
    {DESNT_SPEED_DOWN, 5, 0x06, DESNT_STICK_DIR_UNBOUND, DESNT_MOUSE_DIR_UNBOUND, -1, 1.0f},
    {DESNT_MOVE_UP, 3, 0x0F, DESNT_STICK_DIR_UNBOUND, DESNT_MOUSE_DIR_UNBOUND, -1, 1.0f},
    {DESNT_MOVE_DOWN, 0, 0x13, DESNT_STICK_DIR_UNBOUND, DESNT_MOUSE_DIR_UNBOUND, -1, 1.0f},
    {DESNT_MOVE_FORWARD, -1, 0x3F, DESNT_LEFT_STICK_DIR_UP, DESNT_MOUSE_DIR_UNBOUND, -1, 1.0f},
    {DESNT_MOVE_BACKWARD, -1, 0x3B, DESNT_LEFT_STICK_DIR_DOWN, DESNT_MOUSE_DIR_UNBOUND, -1, 1.0f},
    {DESNT_MOVE_LEFT, -1, 0x29, DESNT_LEFT_STICK_DIR_LEFT, DESNT_MOUSE_DIR_UNBOUND, -1, 1.0f},
    {DESNT_MOVE_RIGHT, -1, 0x2C, DESNT_LEFT_STICK_DIR_RIGHT, DESNT_MOUSE_DIR_UNBOUND, -1, 1.0f},
    {DESNT_YAW_LEFT, -1, 0x47, DESNT_RIGHT_STICK_DIR_LEFT, DESNT_MOUSE_DIR_LEFT, -1, 1.0f},
    {DESNT_YAW_RIGHT, -1, 0x49, DESNT_RIGHT_STICK_DIR_RIGHT, DESNT_MOUSE_DIR_RIGHT, -1, 1.0f},
    {DESNT_PITCH_UP, -1, 0x4B, DESNT_RIGHT_STICK_DIR_UP, DESNT_MOUSE_DIR_UP, -1, 1.0f},
    {DESNT_PITCH_DOWN, -1, 0x45, DESNT_RIGHT_STICK_DIR_DOWN, DESNT_MOUSE_DIR_DOWN, -1, 1.0f},
    {DESNT_ROLL_RIGHT, 2, 0x2D, DESNT_STICK_DIR_UNBOUND, DESNT_MOUSE_DIR_UNBOUND, -1, 1.0f},
    {DESNT_ROLL_LEFT, 1, 0x39, DESNT_STICK_DIR_UNBOUND, DESNT_MOUSE_DIR_UNBOUND, -1, 1.0f},
    {DESNT_AUTO_LEVEL, 15, 0x48, DESNT_STICK_DIR_UNBOUND, DESNT_MOUSE_DIR_UNBOUND, 1, 1.0f},
};

static float Damp; // 0x01CE8494
static desnt_binding_info Desnt_bindings[DESNT_NUM_ACTIONS]; // 0x01CE8498
static bool Descent_physics_inited; // 0x01CE86B9
static bool Auto_level_lock; // 0x01CE86BA
static bool Slew_clamp; // 0x01CE86BB
static bool Check_keys; // 0x01CE86BC, read by descent_physics_move (S5)
static bool Check_mouse; // 0x01CE86BD, read by descent_physics_move (S5)
static float Rotdamp; // 0x01CE2774
static vector Desired_vel; // 0x01CE997C
static vector Desired_rotvel; // 0x01CE998C
static vector Max_rotvel; // 0x01D08F74
static vector Vel; // 0x01D0CB20
static vector Max_vel; // 0x01D0D0BC
static vector Rotvel; // 0x01D38950
static bool Keyboard_enabled = true; // 0x01518A2C, true in .data
static bool Mouse_enabled = true; // 0x01518A2D, true in .data
static bool Position_controls_enabled = true; // 0x01518A2E, true in .data
static bool Rotation_controls_enabled = true; // 0x01518A2F, true in .data

// 0x00599980 (:218)
void descent_physics_init(float max_vel, float max_rotvel, float damp, float rotdamp)
{
    Max_vel.x = max_vel;
    Max_vel.y = max_vel;
    Max_vel.z = max_vel;
    Max_rotvel.x = max_rotvel;
    Max_rotvel.y = max_rotvel;
    Max_rotvel.z = max_rotvel;
    Vel.x = 0.0f;
    Vel.y = 0.0f;
    Vel.z = 0.0f;
    Desired_vel.x = 0.0f;
    Desired_vel.y = 0.0f;
    Desired_vel.z = 0.0f;
    Rotvel.x = 0.0f;
    Rotvel.y = 0.0f;
    Rotvel.z = 0.0f;
    Desired_rotvel.x = 0.0f;
    Desired_rotvel.y = 0.0f;
    Desired_rotvel.z = 0.0f;
    for (int i = 0; i < DESNT_NUM_ACTIONS; i++) // 0x69 dwords copied
    {
        Desnt_bindings[i] = Desnt_default_bindings[i];
    }
    Damp = damp;
    Rotdamp = rotdamp;
    Descent_physics_inited = true;
}

// 0x00566BD0 (:387)
void descent_physics_sim(float damping, float desired_vel, float initial_vel, float t, float* new_vel, float* delta_pos)
{
    if (damping < 0.0001f) // no damping: the velocity is reached at once
    {
        if (delta_pos != nullptr)
        {
            *delta_pos = desired_vel * t;
        }
        if (new_vel != nullptr)
        {
            *new_vel = desired_vel;
        }
        return;
    }
    const float diff = initial_vel - desired_vel;
    const float decay = (float)exp(-t / damping);
    if (delta_pos != nullptr)
    {
        *delta_pos = (1.0f - decay) * diff * damping + desired_vel * t;
    }
    if (new_vel != nullptr)
    {
        *new_vel = decay * diff + desired_vel;
    }
}

// 0x00566CF0 (:415)
void descent_physics_set_action_bindings(desnt_binding_info* new_bindings, int num_bindings)
{
    // -3: back to the default, -2: keep (the stick and mouse enums spell them DEFAULT / NO_CHANGE)
    for (int i = 0; i < num_bindings; i++)
    {
        const desnt_binding_info& b = new_bindings[i];
        desnt_binding_info& dst = Desnt_bindings[b.action];
        const desnt_binding_info& def = Desnt_default_bindings[b.action];
        if (b.controller_button == -3)
        {
            dst.controller_button = def.controller_button;
        }
        else if (b.controller_button != -2)
        {
            dst.controller_button = b.controller_button;
        }
        if (b.key == -3)
        {
            dst.key = def.key;
        }
        else if (b.key != -2)
        {
            dst.key = b.key;
        }
        if (b.stick_dir == DESNT_STICK_DIR_DEFAULT)
        {
            dst.stick_dir = def.stick_dir;
        }
        else if (b.stick_dir != DESNT_STICK_DIR_NO_CHANGE)
        {
            dst.stick_dir = b.stick_dir;
        }
        if (b.mouse_dir == DESNT_MOUSE_DIR_DEFAULT)
        {
            dst.mouse_dir = def.mouse_dir;
        }
        else if (b.mouse_dir != DESNT_MOUSE_DIR_NO_CHANGE)
        {
            dst.mouse_dir = b.mouse_dir;
        }
        if (b.mouse_button == -3)
        {
            dst.mouse_button = def.mouse_button;
        }
        else if (b.mouse_button != -2)
        {
            dst.mouse_button = b.mouse_button;
        }
        if (b.scaling_factor != 0.0f)
        {
            dst.scaling_factor = b.scaling_factor;
        }
    }
}

// 0x00566E60 (:454)
void descent_physics_set_scaling_factor(desnt_action action, float scaling_factor)
{
    Desnt_bindings[action].scaling_factor = scaling_factor;
}

// 0x00566E80 (:460)
void descent_physics_set_max_velocity(float new_max_vel)
{
    Max_vel.x = new_max_vel;
    Max_vel.y = new_max_vel;
    Max_vel.z = new_max_vel;
}

// 0x00566EA0 (:468)
void descent_physics_slew_clamp(bool clamp)
{
    Slew_clamp = clamp;
}

// 0x00566EB0 (:474)
bool descent_physics_is_slew_clamped()
{
    return Slew_clamp;
}

// 0x00566EC0 (:480)
void descent_physics_set_damping(float damping, float rot_damping)
{
    Damp = damping;
    Rotdamp = rot_damping;
}

// 0x00566EE0 (:488)
void descent_physics_keyboard_enable()
{
    Keyboard_enabled = true;
}

// 0x00566EF0 (:495)
void descent_physics_keyboard_disable()
{
    Keyboard_enabled = false;
}

// 0x00566F00 (:502)
void descent_physics_mouse_enable()
{
    Mouse_enabled = true;
}

// 0x00566F10 (:509)
void descent_physics_mouse_disable()
{
    Mouse_enabled = false;
}

// 0x00566F20 (:516)
void descent_physics_position_controls_enable()
{
    Position_controls_enabled = true;
}

// 0x00566F30 (:523)
void descent_physics_position_controls_disable()
{
    Position_controls_enabled = false;
}

// 0x00566F40 (:530)
void descent_physics_rotation_controls_enable()
{
    Rotation_controls_enabled = true;
}

// 0x00566F50 (:537)
void descent_physics_rotation_controls_disable()
{
    Rotation_controls_enabled = false;
}

// 0x00566F60 (:544)
void descent_physics_auto_level_lock()
{
    Auto_level_lock = true;
}

// 0x00566F70 (:551)
void descent_physics_auto_level_unlock()
{
    Auto_level_lock = false;
}
