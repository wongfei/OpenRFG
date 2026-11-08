// Original: volition/vlib/code/input/input_internal.h
//
// vlib input state shared by input.cpp, key.cpp and mouse.cpp. The methods below are inline in this header
// (PDB line info). The key and cached_* structs have no methods; their placement here is inferred.
#pragma once

#include "keen/input/inputevents.hpp"
#include "volition/vlib/code/os/timer.h"

// Turns a stick axis into discrete "scroll" steps for menus.
// size 0xC
struct scroll_info
{
    void reset(); // 0x00566F80 (.h:33)
    void eval(float value); // 0x00599A60 (.h:40)

    int start_time; // 0x00
    int last_scroll; // 0x04
    bool scrolled; // 0x08
};

// One analog or digital controller button. Times are milliseconds of Global_time (timer.h).
// size 0x24
struct button
{
    void flush(); // 0x00566FA0 (.h:67)

    float value; // 0x00
    float last_value; // 0x04
    float deadzone; // 0x08, pressed means value > deadzone
    unsigned int repeat_rate; // 0x0C
    unsigned int repeat_delay; // 0x10
    unsigned int time_down; // 0x14
    unsigned int last_repeat; // 0x18
    bool just_pressed; // 0x1C
    bool repeated; // 0x1D
    bool just_released; // 0x1E
    bool was_down; // 0x1F
    bool masked; // 0x20, cleared every input_poll
};

// size 0x40
struct joy
{
    void flush(); // 0x00566FC0 (.h:80)

    float x_val; // 0x00
    float y_val; // 0x04
    float deadzone; // 0x08
    bool invert_x; // 0x0C
    bool invert_y; // 0x0D
    bool masked_x; // 0x0E
    bool masked_y; // 0x0F
    scroll_info scroll[4]; // 0x10, -x, +x, -y, +y
};

// size 0x4
struct gyro
{
    void flush(); // 0x00567000 (.h:94)

    float value; // 0x00
};

// size 0x1A24
struct controller
{
    void flush(); // 0x00567010 (.h:110)
    void clear(); // 0x005670D0 (.h:127)
    void clear_masking(); // 0x005671A0 (.h:148)

    button buttons[128]; // 0x00
    joy joys[32]; // 0x1200
    gyro gyros[7]; // 0x1A00, indexed by gyro_sensor
    float idle_time; // 0x1A1C, seconds without input
    bool active; // 0x1A20, any input this poll
};

// size 0x10
struct mouse_button
{
    void flush(); // 0x005671D0 (.h:167)

    unsigned int last_click_time; // 0x00
    unsigned int last_repeat; // 0x04
    bool just_pressed; // 0x08
    bool just_released; // 0x09
    bool repeated; // 0x0A
    bool double_clicked; // 0x0B
    bool down; // 0x0C
};

// size 0xC
struct key
{
    bool down; // 0x00
    bool just_down; // 0x01
    bool masked; // 0x02
    int time_down; // 0x04
    int last_repeat; // 0x08
};

// ---- raw keen input, written on the main thread and read by input_poll on the sim thread (see input.h)

// size 0x8
struct cached_controller_stick
{
    float x; // 0x00
    float y; // 0x04
};

// size 0x4
struct cached_controller_gyro
{
    float value; // 0x00
};

// size 0x328
struct cached_controller_state
{
    unsigned int keen_controller_index; // 0x00, 0xFFFFFFFF when none
    bool connected; // 0x04
    float buttons_down[128]; // 0x08
    cached_controller_stick stick_values[32]; // 0x208
    cached_controller_gyro gyro_values[7]; // 0x308
    keen::ControllerType controller_type; // 0x324
};

// size 0x1C
struct cached_mouse_state
{
    float pos_x; // 0x00
    float pos_y; // 0x04
    float relative_move_x; // 0x08, accumulated between polls
    float relative_move_y; // 0x0C
    float relative_move_wheel; // 0x10
    bool buttons[5]; // 0x14
};

// size 0x100
struct cached_keyboard_state
{
    bool keys[256]; // 0x00
};

// size 0x444
struct cached_input_state
{
    cached_controller_state controllers[1]; // 0x00
    cached_mouse_state mouse; // 0x328
    cached_keyboard_state keyboard; // 0x344
};

// ---- inline methods (input_internal.h line numbers in the PDB)

// 0x00566F80 (.h:33)
inline void scroll_info::reset()
{
    start_time = -1;
    last_scroll = -1;
    scrolled = false;
}

// 0x00599A60 (.h:40)
// A scroll on the first frame past half way, then one every 150 ms after a 500 ms delay.
inline void scroll_info::eval(float value)
{
    scrolled = false;
    if (value <= 0.5f)
    {
        start_time = -1;
        last_scroll = -1;
        return;
    }
    const int now = timer_get(TIMER_MILLISECONDS);
    if (start_time == -1)
    {
        start_time = now;
        last_scroll = now;
        scrolled = true;
        return;
    }
    if (500 < now - start_time && 150 < now - last_scroll)
    {
        scrolled = true;
        last_scroll = now;
    }
}

// 0x00566FA0 (.h:67)
inline void button::flush()
{
    value = 0.0f;
    last_value = 0.0f;
    last_repeat = 0;
    time_down = 0;
    repeated = false;
    just_released = false;
    was_down = false;
    masked = false;
    just_pressed = false;
}

// 0x00566FC0 (.h:80)
inline void joy::flush()
{
    y_val = 0.0f;
    x_val = 0.0f;
    masked_x = false;
    masked_y = false;
    for (int i = 0; i < 4; i++)
    {
        scroll[i].reset();
    }
}

// 0x00567000 (.h:94)
inline void gyro::flush()
{
    value = 0.0f;
}

// 0x00567010 (.h:110)
inline void controller::flush()
{
    for (int i = 0; i < 128; i++)
    {
        buttons[i].flush();
    }
    for (int i = 0; i < 32; i++)
    {
        joys[i].flush();
    }
    for (int i = 0; i < 7; i++)
    {
        gyros[i].flush();
    }
}

// 0x005670D0 (.h:127)
// flush, plus the default deadzones (none), repeat rate (150 ms) and delay (500 ms), and no inversion.
inline void controller::clear()
{
    for (int i = 0; i < 128; i++)
    {
        buttons[i].flush();
        buttons[i].deadzone = 0.0f;
        buttons[i].repeat_delay = 500;
        buttons[i].repeat_rate = 150;
    }
    for (int i = 0; i < 32; i++)
    {
        joys[i].flush();
        joys[i].deadzone = 0.0f;
        joys[i].invert_x = false;
        joys[i].invert_y = false;
    }
    for (int i = 0; i < 7; i++)
    {
        gyros[i].flush();
    }
}

// 0x005671A0 (.h:148)
inline void controller::clear_masking()
{
    for (int i = 0; i < 128; i++)
    {
        buttons[i].masked = false;
    }
    for (int i = 0; i < 32; i++)
    {
        joys[i].masked_x = false;
        joys[i].masked_y = false;
    }
}

// 0x005671D0 (.h:167)
inline void mouse_button::flush()
{
    last_click_time = 0;
    last_repeat = 0;
    just_released = false;
    repeated = false;
    double_clicked = false;
    down = false;
    just_pressed = false;
}

extern controller Controllers[1]; // 0x01CDBA18
extern mouse_button Mouse_buttons[5]; // 0x01CE7F78
extern scroll_info Mouse_scroll[3][2]; // 0x01CE2578
extern key Key_states[256]; // 0x01CDD7D0
// orig_main.md OM5: key.cpp's file statics that vconsole.cpp (the same unity build in rfg.exe) uses.
extern unsigned int Key_buffer_head; // 0x01CDB4FC
extern unsigned int Key_buffer_tail; // 0x01CD2900
extern bool Key_lock_states[256]; // 0x01CDD540, num lock 0, caps lock 1, scroll lock 2
extern bool Key_inited; // 0x01CE86D5
extern unsigned int Key_state_change_head; // 0x01CE86D8
extern unsigned int Key_state_change_tail; // 0x01CE86DC
extern bool All_keys_masked; // 0x01CE86D6
unsigned int key_buf_pop(); // 0x00568880 (key.cpp:245)
extern cached_input_state Cached_input_states[2]; // 0x01CD3720
extern unsigned char Current_cached_input_state; // 0x01CE86BF, read by the sim
extern unsigned char Next_cached_input_state; // 0x01518A38, written by keen::rfg::Application::processInputEvent
