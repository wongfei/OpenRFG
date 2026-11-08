// Original: volition/vlib/code/input/input.cpp
//
// The vlib controller layer (player_minimal.md P6). keen::rfg::Application::processInputEvent (main thread) writes the
// raw keen state into Cached_input_states[Next_cached_input_state]; input_poll (sim thread, from controls_poll) takes
// that buffer as the frame's snapshot and derives Controllers[0]. The functions that read Global_time inline its
// getElapsedTime (with the Timer_inited check); here they call timer_get(TIMER_MILLISECONDS), which is the same.
// Multiple-op float expressions are computed in double and rounded once, as the SSE2 code does; the radial deadzone's
// angle math is x87 on the double CRT (__CIatan, __CIcos, __CIsin).

#include "volition/vlib/code/input/input.h"
#include "volition/vlib/code/input/input_internal.h"
#include "volition/vlib/code/math/floating.h"
#include "volition/vlib/code/os/timer.h"

#include "keen/input/inputsystem.hpp"

#include <math.h>
#include <string.h>

controller Controllers[1]; // 0x01CDBA18
cached_input_state Cached_input_states[2]; // 0x01CD3720
unsigned char Current_cached_input_state; // 0x01CE86BF
unsigned char Next_cached_input_state = 1; // 0x01518A38
keen::InputSystem* Keen_input_system; // 0x01CE86C0
bool Input_playing_with_controller; // 0x01CE86C4
keen::ControllerType Input_controller_type; // 0x01CE86C8
bool Input_all_input_masked; // 0x01CE86C6

static bool Input_feedback_enabled = true; // 0x01518A39
static bool Input_deadzone_normalize = true; // 0x01518A3A, rescale the range outside the deadzone to 0..1
static bool Input_deadzone_square = true; // 0x01518A3B, radial stick deadzone (see input_apply_deadzone)
// 0x01518A3C, [gyro_mode][gyro_camera_axis]. Only the third person x axis is 1 in .data; the options screen sets the
// others from the profile.
static float Input_gyro_sensitivity[NUM_GYRO_MODES][NUM_GYRO_CAMERA_AXES] = {{1.0f, 0.0f}, {0.0f, 0.0f}, {0.0f, 0.0f}, {0.0f, 0.0f}};
// 0x0163B1E8 and 0x0163B1F8 (.data)
static gyro_axis_config Input_gyro_orientation_configs[NUM_GYRO_CAMERA_AXES] = {{GYRO_ORIENTATION_Z, 1.0f}, {GYRO_ORIENTATION_X, -1.0f}};
static gyro_axis_config Input_gyro_velocity_configs[NUM_GYRO_CAMERA_AXES] = {{GYRO_ANGULAR_VELOCITY_Z, 1.0f}, {GYRO_ANGULAR_VELOCITY_X, -1.0f}};
static bool Idle_timer_paused; // 0x01CE86C5
static bool Input_init; // 0x01CE86C7
static unsigned int Last_poll_time; // 0x01CE86CC
static bool Input_gyro_enabled[NUM_GYRO_MODES]; // 0x01CE86D0
static bool Input_feedback_unwanted; // 0x01CE86D4

static const float INPUT_MIN_DEADZONE = 0.0001f; // a deadzone below this counts as this (0x012BAEF4)

// The deadzone as the code uses it: at least INPUT_MIN_DEADZONE.
static float input_effective_deadzone(float deadzone)
{
    return INPUT_MIN_DEADZONE - deadzone >= 0.0f ? INPUT_MIN_DEADZONE : deadzone;
}

// The current snapshot's controller state (the read side of the double buffer).
static const cached_controller_state& input_cached_controller(unsigned int controller_id)
{
    return Cached_input_states[Current_cached_input_state].controllers[controller_id];
}

// Clears a cached controller's buttons, sticks and gyros (no keen controller behind it).
static void input_clear_cached_controller(cached_controller_state& c)
{
    memset(c.buttons_down, 0, sizeof(c.buttons_down));
    memset(c.stick_values, 0, sizeof(c.stick_values));
    for (int i = 0; i < NUM_GYRO_SENSORS; i++)
    {
        c.gyro_values[i].value = 0.0f;
    }
}

// 0x005671E0 (:109)
void input_init()
{
    if (Input_init)
    {
        return;
    }
    Input_init = true;
    memset(Cached_input_states, 0, sizeof(Cached_input_states));
    Current_cached_input_state = 0;
    Next_cached_input_state = 1;
    Cached_input_states[0].controllers[0].keen_controller_index = 0xFFFFFFFF;
    Cached_input_states[1].controllers[0].keen_controller_index = 0xFFFFFFFF;
    Controllers[0].clear();
}

// 0x005672E0 (:129)
void input_system_shutdown()
{
    if (Input_init)
    {
        Input_init = false;
        Controllers[0].clear();
    }
}

// 0x00599B30 (:151)
void input_poll()
{
    // Swap the buffers: the latest written state becomes the snapshot, and the main thread goes on writing into a copy
    // of it with the relative mouse movement (accumulated between polls) cleared.
    const unsigned char snapshot = Next_cached_input_state;
    const unsigned char writer = Current_cached_input_state;
    Next_cached_input_state = writer;
    Cached_input_states[writer] = Cached_input_states[snapshot];
    Cached_input_states[writer].mouse.relative_move_x = 0.0f;
    Cached_input_states[writer].mouse.relative_move_y = 0.0f;
    Cached_input_states[writer].mouse.relative_move_wheel = 0.0f;
    Current_cached_input_state = snapshot;
    cached_controller_state& wc = Cached_input_states[writer].controllers[0];
    if (!wc.connected || wc.keen_controller_index == 0xFFFFFFFF)
    {
        wc.keen_controller_index = 0xFFFFFFFF;
        input_clear_cached_controller(wc);
    }

    Input_all_input_masked = false;
    Controllers[0].clear_masking();
    cached_controller_state& sc = Cached_input_states[snapshot].controllers[0];
    if (sc.keen_controller_index == 0xFFFFFFFF)
    {
        input_clear_cached_controller(sc);
    }
    controller& c = Controllers[0];
    for (int i = 0; i < 128; i++)
    {
        c.buttons[i].value = sc.buttons_down[i];
    }
    for (int i = 0; i < 32; i++)
    {
        c.joys[i].x_val = sc.stick_values[i].x;
        c.joys[i].y_val = sc.stick_values[i].y;
    }
    for (int i = 0; i < NUM_GYRO_SENSORS; i++)
    {
        c.gyros[i].value = sc.gyro_values[i].value;
    }

    // Stick scrolls: -x, +x, -y, +y.
    c.active = false;
    for (unsigned int j = 0; j < 32; j++)
    {
        float x;
        float y;
        input_get_joy(x, y, 0, j);
        c.joys[j].scroll[0].eval(-x);
        c.joys[j].scroll[1].eval(x);
        c.joys[j].scroll[2].eval(-y);
        c.joys[j].scroll[3].eval(y);
        if (x != 0.0f || y != 0.0f)
        {
            c.active = true;
        }
    }

    const unsigned int now = (unsigned int)timer_get(TIMER_MILLISECONDS);
    const bool connected = input_cached_controller(0).connected;
    for (int i = 0; i < 128; i++)
    {
        button& b = c.buttons[i];
        const bool down = connected && b.deadzone < b.value && !b.masked;
        b.just_released = b.was_down && !down;
        if (!b.was_down && down)
        {
            b.time_down = now;
            b.just_pressed = true;
            b.repeated = true;
            b.last_repeat = now;
        }
        else
        {
            b.just_pressed = false;
            b.repeated = false;
            if (down && b.repeat_delay < now - b.time_down && b.repeat_rate < now - b.last_repeat)
            {
                b.repeated = true;
                b.last_repeat = now;
            }
        }
        b.was_down = down;
        if (down)
        {
            c.active = true;
        }
    }
    input_update_idle_time(); // inlined in the binary
}

// 0x005673C0 (:316)
bool input_is_connected(unsigned int controller_id)
{
    return input_cached_controller(controller_id).connected;
}

// 0x005673E0 (:328)
void input_flush(unsigned int controller_id)
{
    Controllers[controller_id].flush();
}

// 0x00567400 (:335)
float input_gyro_get_raw_value(gyro_sensor sensor_id, unsigned int controller_id)
{
    return Controllers[controller_id].gyros[sensor_id].value;
}

// 0x00567420 (:340)
float input_gyro_get_camera_orientation_value(gyro_mode mode, gyro_camera_axis axis, unsigned int controller_id)
{
    const float enabled = Input_gyro_enabled[mode] ? 1.0f : 0.0f;
    const gyro_axis_config& config = Input_gyro_orientation_configs[axis];
    return Controllers[controller_id].gyros[config.sensor_id].value * config.direction * Input_gyro_sensitivity[mode][axis] * enabled;
}

// 0x00567480 (:348)
float input_gyro_get_camera_angular_velocity_value(gyro_mode mode, gyro_camera_axis axis, unsigned int controller_id)
{
    const float enabled = Input_gyro_enabled[mode] ? 1.0f : 0.0f;
    const gyro_axis_config& config = Input_gyro_velocity_configs[axis];
    return Controllers[controller_id].gyros[config.sensor_id].value * config.direction * Input_gyro_sensitivity[mode][axis] * enabled;
}

// 0x005674E0 (:366)
void input_get_joy(float& x, float& y, unsigned int controller_id, unsigned int joy_id)
{
    if (!input_cached_controller(controller_id).connected)
    {
        y = 0.0f;
        x = 0.0f;
        return;
    }
    const joy& j = Controllers[controller_id].joys[joy_id];
    x = j.x_val;
    y = j.y_val;
    input_apply_deadzone(j.deadzone, x, y); // inlined in the binary
    if (j.masked_x)
    {
        x = 0.0f;
    }
    if (j.masked_y)
    {
        y = 0.0f;
    }
}

// 0x005678E0 (:463)
void input_get_joy_raw(float& x, float& y, unsigned int controller_id, unsigned int joy_id)
{
    if (!input_cached_controller(controller_id).connected)
    {
        y = 0.0f;
        x = 0.0f;
        return;
    }
    const joy& j = Controllers[controller_id].joys[joy_id];
    x = j.x_val;
    y = j.y_val;
    if (j.masked_x)
    {
        x = 0.0f;
    }
    if (j.masked_y)
    {
        y = 0.0f;
    }
}

// 0x00567950 (:500)
void input_get_x_sensor_raw(float& x, unsigned int controller_id)
{
    x = 0.0f;
}

// 0x00567960 (:516)
void input_get_y_sensor_raw(float& y, unsigned int controller_id)
{
    y = 0.0f;
}

// 0x00567970 (:532)
void input_get_z_sensor_raw(float& z, unsigned int controller_id)
{
    z = 0.0f;
}

// 0x00567980 (:548)
void input_get_yaw_sensor_raw(float& yaw, unsigned int controller_id)
{
    yaw = 0.0f;
}

// 0x00567990 (:564)
void input_get_roll(float& roll, unsigned int controller_id)
{
    roll = 0.0f;
}

// 0x005679A0 (:580)
void input_get_pitch(float& pitch, unsigned int controller_id)
{
    pitch = 0.0f;
}

// 0x005679B0 (:597)
bool input_did_scroll(unsigned int controller_id, unsigned int joy_id, unsigned int which_dir)
{
    return Controllers[controller_id].joys[joy_id].scroll[which_dir].scrolled;
}

// 0x005679E0 (:615)
bool input_is_pressed(unsigned int controller_id, unsigned int button_id)
{
    if (!input_cached_controller(controller_id).connected)
    {
        return false;
    }
    const button& b = Controllers[controller_id].buttons[button_id];
    return b.deadzone < b.value && !b.masked;
}

// 0x00567A50 (:636)
bool input_just_pressed(unsigned int controller_id, unsigned int button_id)
{
    const button& b = Controllers[controller_id].buttons[button_id];
    return !b.masked && b.just_pressed;
}

// 0x00567A90 (:656)
bool input_just_released(unsigned int controller_id, unsigned int button_id)
{
    button& b = Controllers[controller_id].buttons[button_id];
    if (b.masked)
    {
        return false;
    }
    const bool released = b.just_released;
    b.just_released = false;
    return released;
}

// 0x00567AC0 (:673)
bool input_just_released_no_clear(unsigned int controller_id, unsigned int button_id)
{
    const button& b = Controllers[controller_id].buttons[button_id];
    if (b.masked)
    {
        return false;
    }
    const float deadzone = input_effective_deadzone(b.deadzone);
    if (deadzone < b.last_value && b.value <= deadzone)
    {
        return true;
    }
    return b.just_released;
}

// 0x00567B60 (:696)
bool input_did_repeat(unsigned int controller_id, unsigned int button_id)
{
    const button& b = Controllers[controller_id].buttons[button_id];
    return !b.masked && b.repeated;
}

// 0x00567BA0 (:715)
// The value past the deadzone, rescaled to 0..1 with its sign.
float input_get_value(unsigned int controller_id, unsigned int button_id)
{
    const button& b = Controllers[controller_id].buttons[button_id];
    if (!input_cached_controller(controller_id).connected || b.masked)
    {
        return 0.0f;
    }
    const float deadzone = input_effective_deadzone(b.deadzone);
    const float sign = b.value >= 0.0f ? 1.0f : -1.0f;
    float past = fabsf(b.value) - deadzone;
    if (past < 0.0f)
    {
        past = 0.0f;
    }
    return (sign / (1.0f - deadzone)) * past;
}

// 0x00567CA0 (:744)
float input_get_last_raw_value(unsigned int controller_id, unsigned int button_id)
{
    if (!input_cached_controller(controller_id).connected)
    {
        return 0.0f;
    }
    return Controllers[controller_id].buttons[button_id].last_value;
}

// 0x00567CE0 (:765)
float input_get_raw_value(unsigned int controller_id, unsigned int button_id)
{
    const button& b = Controllers[controller_id].buttons[button_id];
    if (!input_cached_controller(controller_id).connected || b.masked)
    {
        return 0.0f;
    }
    return b.value;
}

// 0x00567D30 (:789)
void input_process_controller(void (*btn_callback)(unsigned int, float), unsigned int controller_id)
{
    for (unsigned int i = 0; i < 128; i++)
    {
        if (input_is_pressed(controller_id, i)) // inlined in the binary
        {
            btn_callback(i, input_get_value(controller_id, i));
        }
    }
}

// 0x00567DD0 (:806)
void input_invert_x(unsigned int controller_id, unsigned int joy_id)
{
    joy& j = Controllers[controller_id].joys[joy_id];
    j.invert_x = !j.invert_x;
}

// 0x00567E00 (:813)
void input_set_invert_x(unsigned int controller_id, unsigned int joy_id, bool value)
{
    Controllers[controller_id].joys[joy_id].invert_x = value;
}

// 0x00567E20 (:820)
bool input_get_invert_x(unsigned int controller_id, unsigned int joy_id)
{
    if (controller_id == 0 && joy_id < 32)
    {
        return Controllers[0].joys[joy_id].invert_x;
    }
    return false;
}

// 0x00567E50 (:827)
bool input_get_invert_y(unsigned int controller_id, unsigned int joy_id)
{
    if (controller_id == 0 && joy_id < 32)
    {
        return Controllers[0].joys[joy_id].invert_y;
    }
    return false;
}

// 0x00567E80 (:839)
void input_invert_y(unsigned int controller_id, unsigned int joy_id)
{
    joy& j = Controllers[controller_id].joys[joy_id];
    j.invert_y = !j.invert_y;
}

// 0x00567EB0 (:846)
void input_set_invert_y(unsigned int controller_id, unsigned int joy_id, bool value)
{
    Controllers[controller_id].joys[joy_id].invert_y = value;
}

// 0x00567ED0 (:860)
// Original bug kept: the button's deadzone is set, although callers pass joy ids too.
void input_set_deadzone(unsigned int controller_id, unsigned int button_id, float new_zone)
{
    Controllers[controller_id].buttons[button_id].deadzone = new_zone;
}

// 0x00567F00 (:876)
float input_get_deadzone(unsigned int controller_id, unsigned int button_id)
{
    return Controllers[controller_id].buttons[button_id].deadzone;
}

// 0x00567F20 (:889)
void input_reset_deadzone(unsigned int controller_id, unsigned int button_id)
{
    Controllers[controller_id].buttons[button_id].deadzone = 0.0f;
}

// 0x00567F40 (:903)
void input_set_btn_repeat_delay(unsigned int controller_id, unsigned int button_id, unsigned int new_delay)
{
    Controllers[controller_id].buttons[button_id].repeat_delay = new_delay;
}

// 0x00567F60 (:917)
unsigned int input_get_btn_repeat_delay(unsigned int controller_id, unsigned int button_id)
{
    return Controllers[controller_id].buttons[button_id].repeat_delay;
}

// 0x00567F80 (:930)
void input_set_btn_repeat_rate(unsigned int controller_id, unsigned int button_id, unsigned int new_rate)
{
    Controllers[controller_id].buttons[button_id].repeat_rate = new_rate;
}

// 0x00567FA0 (:944)
unsigned int input_get_btn_repeat_rate(unsigned int controller_id, unsigned int button_id)
{
    return Controllers[controller_id].buttons[button_id].repeat_rate;
}

// 0x00567FC0 (:957)
void input_set_feedback(unsigned int controller_id, float left_motor, float right_motor)
{
    if (!Input_feedback_enabled || Input_feedback_unwanted)
    {
        right_motor = 0.0f;
        left_motor = 0.0f;
    }
    const unsigned int keen_controller = input_cached_controller(controller_id).keen_controller_index;
    if (keen_controller != 0xFFFFFFFF)
    {
        keen::input::setRumble(Keen_input_system, keen_controller, left_motor, right_motor);
    }
}

// 0x00568030 (:975)
void input_mask_button(unsigned int controller_id, unsigned int button_id)
{
    if (controller_id == 0 && button_id < 128)
    {
        Controllers[0].buttons[button_id].masked = true;
    }
}

// 0x00568060 (:982)
void input_mask_joy_axis(unsigned int controller_id, unsigned int joy_id, unsigned int axis)
{
    if (controller_id != 0 || joy_id >= 32 || axis >= 2)
    {
        return;
    }
    if (axis == 0)
    {
        Controllers[0].joys[joy_id].masked_x = true;
    }
    else
    {
        Controllers[0].joys[joy_id].masked_y = true;
    }
}

// 0x005680A0 (:1000)
void input_mask_all()
{
    for (int i = 0; i < 128; i++)
    {
        Controllers[0].buttons[i].masked = true;
    }
    for (int i = 0; i < 32; i++)
    {
        Controllers[0].joys[i].masked_x = true;
        Controllers[0].joys[i].masked_y = true;
    }
    Input_all_input_masked = true;
}

// 0x005680E0 (:1021)
bool input_is_all_masked()
{
    return Input_all_input_masked;
}

// 0x005680F0 (:1026)
void input_clear_all_masks()
{
    Input_all_input_masked = false;
    Controllers[0].clear_masking();
}

// 0x00568120 (:1042)
bool input_get_vibration()
{
    return Input_feedback_enabled;
}

// 0x00568130 (:1047)
void input_set_vibration(bool on)
{
    Input_feedback_enabled = on;
    const unsigned int keen_controller = input_cached_controller(0).keen_controller_index;
    if (!on && keen_controller != 0xFFFFFFFF)
    {
        keen::input::setRumble(Keen_input_system, keen_controller, 0.0f, 0.0f);
    }
}

// 0x00568180 (:1060)
bool input_get_vibration_unwanted()
{
    return Input_feedback_unwanted;
}

// 0x00568190 (:1065)
void input_set_vibration_unwanted(bool unwanted)
{
    Input_feedback_unwanted = unwanted;
    const unsigned int keen_controller = input_cached_controller(0).keen_controller_index;
    if (unwanted && keen_controller != 0xFFFFFFFF)
    {
        keen::input::setRumble(Keen_input_system, keen_controller, 0.0f, 0.0f);
    }
}

// 0x005681E0 (:1077)
bool input_get_gyro_enabled(gyro_mode mode)
{
    return Input_gyro_enabled[mode];
}

// 0x005681F0 (:1082)
void input_set_gyro_enabled(gyro_mode mode, bool enabled)
{
    Input_gyro_enabled[mode] = enabled;
}

// 0x00568200 (:1087)
float input_get_gyro_sensitivity(gyro_mode mode, gyro_camera_axis axis)
{
    return Input_gyro_sensitivity[mode][axis];
}

// 0x00568220 (:1092)
void input_set_gyro_sensitivity(gyro_mode mode, gyro_camera_axis axis, float sensitivity)
{
    Input_gyro_sensitivity[mode][axis] = sensitivity;
}

// 0x00568240 (:1097)
void input_reset_gyro_orientation_origin()
{
    const unsigned int keen_controller = keen::input::getControllerIdForPlayer(nullptr, keen::ControllerClass_Controller);
    keen::input::resetGyroOrientationOrigin(Keen_input_system, keen_controller);
}

// 0x00568260 (:1104)
const gyro_axis_config& input_gyro_get_orientation_config(gyro_camera_axis axis)
{
    return Input_gyro_orientation_configs[axis];
}

// 0x00568270 (:1109)
void input_gyro_set_orientation_config(gyro_camera_axis axis, const gyro_axis_config& config)
{
    Input_gyro_orientation_configs[axis].sensor_id = config.sensor_id;
    Input_gyro_orientation_configs[axis].direction = config.direction;
}

// 0x00568290 (:1114)
const gyro_axis_config& input_gyro_get_angular_velocity_config(gyro_camera_axis axis)
{
    return Input_gyro_velocity_configs[axis];
}

// 0x005682A0 (:1119)
void input_gyro_set_angular_velocity_config(gyro_camera_axis axis, const gyro_axis_config& config)
{
    Input_gyro_velocity_configs[axis].sensor_id = config.sensor_id;
    Input_gyro_velocity_configs[axis].direction = config.direction;
}

// 0x005682C0 (:1126)
float input_get_idle_time()
{
    return 10000.0f - Controllers[0].idle_time >= 0.0f ? Controllers[0].idle_time : 10000.0f; // capped at 10000 s
}

// 0x00568300 (:1142)
void input_idle_pause(bool _pause)
{
    Idle_timer_paused = _pause;
}

// 0x00568310 (:1149)
bool input_any_controller_active()
{
    for (int i = 0; i < 1; i++)
    {
        if (Controllers[i].active)
        {
            return true;
        }
    }
    return false;
}

// 0x00568330 (:1161)
// Square mode (the default) is a radial deadzone: the stick's length past the deadzone, rescaled to 0..1, along the
// stick's angle. The deadzone is widened by 1 / cos of the angle folded into -45..45 degrees, so it is a square.
void input_apply_deadzone(float deadzone, float& x, float& y)
{
    const bool normalize = Input_deadzone_normalize;
    if (Input_deadzone_square)
    {
        const float min_dz = input_effective_deadzone(deadzone);
        if (fabsf(y) < min_dz && fabsf(x) < min_dz)
        {
            y = 0.0f;
            x = 0.0f;
            return;
        }
        const float x0 = x;
        float len = (float)sqrt((double)(float)((double)y * (double)y + (double)x0 * (double)x0));
        if (len - 0.0f < 0.0f)
        {
            len = 0.0f;
        }
        if (len - 1.0f >= 0.0f)
        {
            len = 1.0f;
        }
        // x87: atan(y / x), half a turn added for a negative x
        float angle = (float)atan((double)(float)(y / x0));
        if (x0 < 0.0f)
        {
            angle = (float)((double)angle + (double)3.14159274f);
        }
        float folded = angle;
        while ((double)folded > (double)0.785398185f)
        {
            folded = (float)((double)folded - (double)1.57079637f);
        }
        if ((double)folded < (double)-0.785398185f)
        {
            folded = (float)((double)folded + (double)1.57079637f);
        }
        if (folded - -0.785398185f < 0.0f)
        {
            folded = -0.785398185f;
        }
        if (folded - 0.785398185f >= 0.0f)
        {
            folded = 0.785398185f;
        }
        const float edge = (float)((double)deadzone / (double)(float)cos((double)folded));
        const float scaled = (float)(((double)len - (double)edge) / (1.0 - (double)edge));
        x = (float)cos((double)angle) * scaled;
        y = (float)sin((double)angle) * scaled;
        return;
    }
    input_apply_axis_deadzone(deadzone, x); // inlined in the binary
    float v = y;
    if (fabsf(v) < input_effective_deadzone(deadzone))
    {
        y = 0.0f;
    }
    else if (normalize)
    {
        const float sign = signbit(v) ? -1.0f : 1.0f;
        y = (float)(((double)v - (double)sign * (double)deadzone) / (1.0 - (double)deadzone));
    }
}

// 0x0059A080 (:1217)
// Seconds without input on the controller (input_poll sets active).
void input_update_idle_time()
{
    const unsigned int now = (unsigned int)timer_get(TIMER_MILLISECONDS);
    if (Last_poll_time != 0)
    {
        if (Controllers[0].active)
        {
            Controllers[0].idle_time = 0.0f;
            Last_poll_time = now;
            return;
        }
        if (!Idle_timer_paused)
        {
            unsigned int elapsed = now - Last_poll_time;
            if (now < Last_poll_time)
            {
                elapsed -= 1; // as in the binary (the borrow of a 64-bit difference)
            }
            Controllers[0].idle_time = (float)((double)(float)elapsed * 0.001 + (double)Controllers[0].idle_time);
        }
    }
    Last_poll_time = now;
}

// 0x005686F0 (:1245)
void input_apply_axis_deadzone(float deadzone, float& value)
{
    const float v = value;
    if (fabsf(v) < input_effective_deadzone(deadzone))
    {
        value = 0.0f;
        return;
    }
    if (Input_deadzone_normalize)
    {
        const float sign = signbit(v) ? -1.0f : 1.0f;
        value = (float)(((double)v - (double)sign * (double)deadzone) / (1.0 - (double)deadzone));
    }
}

// 0x005687B0 (:1254)
bool input_has_vibration()
{
    return true;
}

// 0x005687C0 (:1259)
bool input_has_controller()
{
    return input_cached_controller(0).connected;
}

// 0x005687E0 (:1265)
void input_set_playing_with_controller(bool with_controller, keen::ControllerType controller_type)
{
    Input_playing_with_controller = with_controller;
    Input_controller_type = controller_type;
}

// 0x00568800 (:1271)
bool input_get_playing_with_controller()
{
    return Input_playing_with_controller;
}

// 0x00568810 (:1276)
keen::ControllerType input_get_controller_type()
{
    return Input_controller_type;
}

// ---- input.h inlines

// 0x00566BB0 (.h:203)
gyro_axis_config::gyro_axis_config(gyro_sensor sensor_id, float direction)
    : sensor_id(sensor_id), direction(direction)
{
}

// 0x00753D50 (.h:199)
gyro_axis_config::gyro_axis_config()
{
}

// 0x007B9A00 (.h:210)
bool gyro_axis_config::is_equal(const gyro_axis_config& other) const
{
    return sensor_id == other.sensor_id && 0.0f < other.direction * direction;
}
