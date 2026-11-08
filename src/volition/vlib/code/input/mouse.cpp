// Original: volition/vlib/code/input/mouse.cpp
//
// The mouse (player_minimal.md P6). mouse_poll runs from controls_poll once per sim frame: the PC back end
// (mouse_generic_poll, mouse_generic.cpp) reads the snapshot of Cached_input_states into the deltas and marks the
// buttons into OS_mouse_buttons, which are then copied to Mouse_buttons and cleared for the next poll. Compiled in
// vlib_unity.cpp: mouse_generic.cpp reads this file's statics. The functions that read Global_time inline its
// getElapsedTime; here they call timer_get(TIMER_MILLISECONDS), which is the same.

#include "volition/vlib/code/input/input_internal.h"
#include "volition/vlib/code/input/mouse.h"
#include "volition/vlib/code/os/os.h"

int Mouse_dx; // 0x01CDAE48
int Mouse_dy; // 0x01CDB904
int Mouse_wheel; // 0x01CDBA10
mouse_button Mouse_buttons[5]; // 0x01CE7F78
scroll_info Mouse_scroll[3][2]; // 0x01CE2578, [x, y, wheel][negative, positive]
float Mouse_sensitivity = 1.0f; // 0x01518A30
float Mouse_pixel_multiplier = 0.09f; // 0x01518A34

static bool Mouse_enabled = true; // 0x01518A2D
static mouse_button OS_mouse_buttons[5]; // 0x01CE2710, marked by the back end, copied to Mouse_buttons by mouse_poll
static int Mouse_x; // 0x01CE845C
static int Mouse_y; // 0x01CDB4F0
static int Mouse_raw_x; // 0x01CE2648
static int Mouse_raw_y; // 0x01CDB4F8
int Mouse_dwheel; // 0x01CDA5C4 (external since UI6: the control options' scroll bar)
float Mouse_x_scale = 1.0f; // 0x01518A5C
float Mouse_y_scale = 1.0f; // 0x01518B58
float Mouse_x_offset; // 0x01CE86E0
float Mouse_y_offset; // 0x01CE86E4
// Global in rfg.pdb (multi_game.cpp reads them); declared in mouse.h.
float Mouse_scaled_x; // 0x01CE2764
float Mouse_scaled_y; // 0x01CDA5C0
static bool Mouse_in_window; // 0x01CE86D7
static bool Mouse_using_relative_movement; // 0x01CE86E8
static bool Mouse_visible; // 0x01CE86EA

static const unsigned int MOUSE_DOUBLE_CLICK_MS = 400;
static const unsigned int MOUSE_REPEAT_DELAY_MS = 500; // the first repeat after more than this (0x1F5 > ...)
static const unsigned int MOUSE_REPEAT_RATE_MS = 150; // then one every more than this (0x97 > ...)

// 0x00568EA0 (:27)
void reset_scroll()
{
    for (int axis = 0; axis < 3; axis++)
    {
        Mouse_scroll[axis][0].reset();
        Mouse_scroll[axis][1].reset();
    }
}

// 0x0059A3F0 (:59)
void mouse_init()
{
    for (int i = 0; i < 5; i++)
    {
        Mouse_buttons[i].flush();
    }
    reset_scroll();
}

// 0x0059A4C0 (:71)
void mouse_auto_centering(bool center_it)
{
    Mouse_using_relative_movement = center_it;
}

// 0x0059A4D0 (:77)
// Original behaviour kept: the double click compares with Mouse_buttons' last click and the press time goes into
// Mouse_buttons' last_repeat as well as OS_mouse_buttons' last_click_time; mouse_update_from_os then overwrites
// Mouse_buttons with OS_mouse_buttons.
void mouse_mark_button(unsigned int button_id, bool down)
{
    mouse_button& b = OS_mouse_buttons[button_id];
    if (b.down == down)
    {
        return;
    }
    if (!down)
    {
        b.just_released = true;
        b.last_repeat = 0;
        b.down = false;
        return;
    }
    b.just_pressed = true;
    b.repeated = true;
    const unsigned int now = (unsigned int)timer_get(TIMER_MILLISECONDS);
    if (now - Mouse_buttons[button_id].last_click_time < MOUSE_DOUBLE_CLICK_MS)
    {
        b.double_clicked = true;
    }
    const unsigned int click_time = (unsigned int)timer_get(TIMER_MILLISECONDS);
    Mouse_buttons[button_id].last_repeat = click_time;
    b.last_click_time = click_time;
    b.down = true;
}

// 0x00568F10 (:95)
void mouse_update_from_os()
{
    for (int i = 0; i < 5; i++)
    {
        Mouse_buttons[i] = OS_mouse_buttons[i];
    }
}

// 0x00568FF0 (:103)
void mouse_flush_from_os()
{
    for (int i = 0; i < 5; i++)
    {
        OS_mouse_buttons[i].just_pressed = false;
        OS_mouse_buttons[i].just_released = false;
        OS_mouse_buttons[i].repeated = false;
        OS_mouse_buttons[i].double_clicked = false;
        OS_mouse_buttons[i].last_repeat = 0;
    }
    reset_scroll();
}

// 0x005C3660 (:121)
void mouse_poll()
{
    mouse_generic_poll();
    mouse_update_from_os();
    mouse_flush_from_os();
    const unsigned int now = (unsigned int)timer_get(TIMER_MILLISECONDS);
    for (int i = 0; i < 5; i++)
    {
        mouse_button& b = Mouse_buttons[i];
        if (b.down && MOUSE_REPEAT_DELAY_MS < now - b.last_click_time && MOUSE_REPEAT_RATE_MS < now - b.last_repeat)
        {
            b.last_repeat = now;
            b.repeated = true;
        }
        else
        {
            b.repeated = false;
        }
    }
    const float deltas[3] = {(float)Mouse_dx, (float)-Mouse_dy, (float)Mouse_wheel};
    for (int axis = 0; axis < 3; axis++)
    {
        Mouse_scroll[axis][0].eval(-deltas[axis]);
        Mouse_scroll[axis][1].eval(deltas[axis]);
    }
}

// 0x00569090 (:166)
void mouse_get_location(unsigned int& x, unsigned int& y, int& wheel)
{
    x = Mouse_x;
    y = Mouse_y;
    wheel = Mouse_wheel;
}

// 0x0059A5B0 (:177)
void mouse_set_location(unsigned int x, unsigned int y, int wheel)
{
    Mouse_x = x;
    Mouse_y = y;
    Mouse_wheel = wheel;
}

// 0x005690C0 (:186)
void mouse_set_wheel_pos(int wheel)
{
    Mouse_wheel = wheel;
}

// 0x005690D0 (:196)
void mouse_get_deltas(int& x, int& y, int& wheel)
{
    x = Mouse_dx;
    y = Mouse_dy;
    wheel = Mouse_dwheel;
}

// 0x00569100 (:205)
bool mouse_is_pressed(unsigned int button_id)
{
    return Mouse_buttons[button_id].down;
}

// 0x00569110 (:210)
bool mouse_did_repeat(unsigned int button_id)
{
    return Mouse_buttons[button_id].down && Mouse_buttons[button_id].repeated;
}

// 0x00569140 (:217)
bool mouse_just_pressed(unsigned int button_id)
{
    return Mouse_buttons[button_id].just_pressed;
}

// 0x00569150 (:224)
bool mouse_just_released(unsigned int button_id)
{
    return Mouse_buttons[button_id].just_released;
}

// 0x00569160 (:231)
bool mouse_double_clicked(unsigned int button_id)
{
    return Mouse_buttons[button_id].double_clicked;
}

// 0x00569170 (:261)
// The flags (down too) and the repeat time; the last click time stays.
void mouse_flush()
{
    for (int i = 0; i < 5; i++)
    {
        Mouse_buttons[i].just_pressed = false;
        Mouse_buttons[i].just_released = false;
        Mouse_buttons[i].repeated = false;
        Mouse_buttons[i].double_clicked = false;
        Mouse_buttons[i].down = false;
        Mouse_buttons[i].last_repeat = 0;
    }
    reset_scroll();
}

// 0x00569210 (:333)
bool mouse_is_supported()
{
    return true;
}

// 0x00569220 (:342)
bool mouse_is_connected()
{
    return true;
}

// 0x00569230 (:353)
bool mouse_did_scroll(unsigned int which_axis, bool positive)
{
    return Mouse_scroll[which_axis][positive].scrolled;
}

// 0x00569250 (:359)
bool mouse_just_moved()
{
    return 0 < abs(Mouse_dwheel) + abs(Mouse_dy) + abs(Mouse_dx);
}

// 0x00569280 (:367)
bool mouse_just_scrolled_wheel()
{
    return Mouse_dwheel != 0;
}

// 0x0059A5D0 (:372)
bool mouse_over_window()
{
    return Mouse_in_window;
}

// 0x005C3790 (:377)
void mouse_update_visibility()
{
    Mouse_visible = !Os_app_active || !Mouse_in_window;
}
