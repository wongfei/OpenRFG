// Original: volition/vlib/code/input/mouse_generic.cpp
//
// The PC mouse back end (player_minimal.md P6): reads the snapshot's mouse (Cached_input_states, written by
// keen::rfg::Application::processInputEvent) into mouse.cpp's position, deltas and buttons. In relative mode (the game
// camera) the deltas are the movement accumulated between polls; otherwise they are the cursor movement. Compiled in
// vlib_unity.cpp after mouse.cpp, whose statics it uses.

#include "volition/vlib/code/input/input.h"
#include "volition/vlib/code/input/input_internal.h"
#include "volition/vlib/code/input/mouse.h"
#include "volition/vlib/code/os/os.h"

#include "keen/input/inputsystem.hpp"

static bool Mouse_using_relative_movement_previous; // 0x01CE86E9
static int Mouse_relative_delay_frames; // 0x01CE86EC, polls without deltas after a mode switch

// 0x00569290 (:42)
void mouse_generic_init()
{
}

// 0x005692A0 (:46)
void mouse_generic_set_location(unsigned int x, unsigned int y)
{
}

// 0x005692B0 (:55)
void mouse_generic_auto_centering(bool center_it)
{
    Mouse_using_relative_movement = center_it;
}

// 0x005B88D0 (:60)
void mouse_generic_update_visibility()
{
    Mouse_visible = !Os_app_active || !Mouse_in_window;
}

// 0x005692C0 (:75)
void mouse_generic_update_front_thread()
{
    keen::input::setMouseRelativeInput(Keen_input_system, Mouse_using_relative_movement);
    keen::input::setMouseCursorVisible(Keen_input_system, Mouse_visible, Mouse_using_relative_movement);
}

// 0x005B88F0 (:84)
void mouse_generic_poll()
{
    if (Mouse_using_relative_movement != Mouse_using_relative_movement_previous)
    {
        Mouse_relative_delay_frames = 2;
        Mouse_using_relative_movement_previous = Mouse_using_relative_movement;
    }
    const cached_mouse_state& m = Cached_input_states[Current_cached_input_state].mouse;
    if (Mouse_using_relative_movement)
    {
        Mouse_dx = (int)m.relative_move_x;
        Mouse_dy = (int)m.relative_move_y;
    }
    else
    {
        Mouse_dx = (int)m.pos_x - Mouse_x;
        Mouse_dy = (int)m.pos_y - Mouse_y;
    }
    Mouse_dwheel = (int)m.relative_move_wheel;
    if (0 < Mouse_relative_delay_frames)
    {
        Mouse_relative_delay_frames--;
        Mouse_dx = 0;
        Mouse_dy = 0;
    }
    Mouse_wheel += Mouse_dwheel;
    Mouse_raw_x = (int)m.pos_x;
    Mouse_y = (int)m.pos_y;
    Mouse_in_window = Os_app_active;
    Mouse_visible = !Os_app_active; // mouse_generic_update_visibility inlined, after Mouse_in_window = Os_app_active
    Mouse_scaled_x = (float)Mouse_raw_x * Mouse_x_scale + Mouse_x_offset;
    Mouse_scaled_y = (float)Mouse_y * Mouse_y_scale + Mouse_y_offset;
    Mouse_raw_y = Mouse_y;
    Mouse_x = Mouse_raw_x;
    for (unsigned int i = 0; i < 5; i++)
    {
        mouse_mark_button(i, Cached_input_states[Current_cached_input_state].mouse.buttons[i]);
    }
}

// 0x00569300 (:147)
bool mouse_generic_over_window()
{
    return Mouse_in_window;
}
