// Original: volition/rfg/code/control/control_map.cpp
//
// Partial (player_minimal.md P6): game actions on top of vlib input. controls_poll polls vlib and reads the frame's
// controls into Control_info[0]; the controls_* queries look the action's bound controls up in the current control
// config (Control_config, filled from the default binding tables by controls_system_init) and test them. Compiled in
// rfg_control_unity.cpp. Also (stub_impl.md S5): the configs, bindings, action groups and mandatory controls the
// options screens edit. The button images (stub_impl.md S7). Not ported: the rebinding functions, the button and axis names, the axis
// images, the gyro and Steam controller paths beyond what the queries reach.
// Multiple-op float expressions are computed in double and rounded once, as the SSE2 code does.

#include "volition/rfg/code/control/control_map.h"

#include "volition/rfg/code/camera/camera_first_person.h"
#include "volition/rfg/code/gamewide/gameseq.h"
#include "volition/rfg/code/interface/localization/localization_shared.h"
#include "volition/rfg/code/interface/ui_document_manager.h"
#include "volition/rfg/code/level/world.h"
#include "volition/rfg/code/objects/humans/player.h"
#include "volition/rfg/code/rfg.h"
#include "volition/rfg/code/vehicle/vehicle_class.h"
#include "volition/vlib/code/input/input.h"
#include "volition/vlib/code/input/key.h"
#include "volition/vlib/code/input/mouse.h"
#include "volition/vlib/code/os/os.h"
#include "volition/vlib/code/os/frametime.h"
#include "volition/vlib/code/os/vconsole.h"
#include "volition/vlib/code/util/hash_table.h"
#include "volition/vlib/code/util/string_util.h"
#include "volition/vlib/code/util/unicode_tools.h"
#include "keen/base/stringencoding.hpp"
#include <stdio.h>

#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "volition/rfg/code/cutscene/video_cutscene.h"
#include "volition/rfg/code/gameplay/wrecking_crew/wrecking_crew.h"
#include "volition/rfg/code/interface/ui_document_hud_multiplayer.h"
#include "volition/rfg/code/interface/ui_message_box.h"
#include "volition/rfg/code/multi/multi_core/include/multi.h"
#include "volition/rfg/code/multi/multi_core/include/multi_core_platform.h"
#include "volition/rfg/code/multi/multi_game/multi_game.h"
#include "volition/rfg/code/multi/multi_game/multi_game_camera.h"
#include "volition/rfg/code/vehicle/vehicle_system.h"

// ---- control encodings (control_map.h)
static const unsigned int CONTROL_INVALID = 0xFFFFFFFF;
static const unsigned int CONTROL_TYPE_MASK = 0xF0000000;
static const unsigned int CONTROL_TYPE_KEYBOARD = 0x40000000;
static const unsigned int CONTROL_TYPE_MOUSE = 0x20000000;
static const unsigned int CONTROL_AXIS_AS_BUTTONS = 0x08000000; // bit 27: an axis bound to two buttons or keys
static const unsigned int CONTROL_AXISDIR_POSITIVE = 0x00800000; // the positive direction of a stick_dir

control_config Control_config[1][NUM_CONTROL_CONFIG_SETTINGS]; // 0x01E25778
unsigned int Player_controller_index; // 0x01E2A9CC

static control_info Control_info[1]; // 0x01E2A9E8, filled by controls_poll
static bool Mouse_spike_normalization_enabled = true; // 0x0164AD32 (read by the mouse spike filter, not ported)
static float Control_stick_digital_sensitivity = 0.6f; // 0x0164ADD8, a stick past this counts as a pressed direction
static float Mouse_spike_max_value = 10.0f; // 0x0164AF4C
static float Mouse_spike_control_contribution = 0.6f; // 0x0164B628
static float Mouse_spike_movement_threshold = 20.0f; // 0x0164B62C
static float Mouse_spike_decay = 0.5f; // 0x0164B630
static float Adaptive_Trigger_threshold = 0.1f; // 0x0164B690, PDB: global
static float Mouse_cap = 1.0f; // 0x0164B694, the largest analog value of the mouse axes
static bool Control_bounceback_protection; // 0x01E2A9D0
static bool Accept_and_back_reversed; // 0x01E2A9D1

#include "volition/rfg/code/control/control_map_bindings.inl"

static int Num_button_bindings_keyboard_mouse = 99; // 0x0164B634
static int Num_axis_bindings_keyboard_mouse = 17; // 0x0164B638
static int Num_button_bindings_xbox = 100; // 0x0164B63C
static int Num_axis_bindings_xbox = 17; // 0x0164B640
static int Num_button_bindings_generic = 100; // 0x0164B644
static int Num_axis_bindings_generic = 17; // 0x0164B648
static int Num_button_bindings_steam = 100; // 0x0164B64C
static int Num_axis_bindings_steam = 17; // 0x0164B650
static int Num_reversed_accept_cancel_bindings = 4; // 0x0164B68C

// 0x0164B654, indexed by control_config_setting: the PS4, Xbox One and Switch settings use the Xbox table.
static const control_button_binding_info* Control_button_default_configs[NUM_CONTROL_CONFIG_SETTINGS] = {
    Button_bindings_keyboard_mouse, Button_bindings_xbox, Button_bindings_xbox, Button_bindings_xbox,
    Button_bindings_generic, Button_bindings_steam, Button_bindings_xbox};
// 0x0164B670
static const control_axis_binding_info* Control_axis_default_configs[NUM_CONTROL_CONFIG_SETTINGS] = {
    Axis_bindings_keyboard_mouse, Axis_bindings_xbox, Axis_bindings_xbox, Axis_bindings_xbox, Axis_bindings_generic,
    Axis_bindings_steam, Axis_bindings_xbox};
// 0x01E2B26C and 0x01E2B250: filled by dynamic initializers from the Num_* counts above
static int Control_button_default_config_sizes[NUM_CONTROL_CONFIG_SETTINGS] = {
    Num_button_bindings_keyboard_mouse, Num_button_bindings_xbox, Num_button_bindings_xbox, Num_button_bindings_xbox,
    Num_button_bindings_generic, Num_button_bindings_steam, Num_button_bindings_xbox};
static int Control_axis_default_config_sizes[NUM_CONTROL_CONFIG_SETTINGS] = {
    Num_axis_bindings_keyboard_mouse, Num_axis_bindings_xbox, Num_axis_bindings_xbox, Num_axis_bindings_xbox,
    Num_axis_bindings_generic, Num_axis_bindings_steam, Num_axis_bindings_xbox};

// 0x0164B6A8 and 0x0164BAE8: action names (hashed by localization_generate_hash_from_key) to actions, for the
// binding files.
static data_hash_table<unsigned int, control_button_action, &hash_uint, 108, unsigned char> Button_action_name_hash(false);
static data_hash_table<unsigned int, control_axis_action, &hash_uint, 26, unsigned char> Stick_action_name_hash(false);

static void controls_create_hash_tables(); // 0x007030A0 (:2572)
static int bound_buttons_from_action(control_button_action action, unsigned int controller_id, unsigned int* bits); // 0x006FE0F0 (:3291)
static bool check_axisdir_down(unsigned int dir, unsigned int controller_id); // 0x006F89A0 (:3310)
static bool check_axisdir_just_pressed(unsigned int dir, unsigned int controller_id); // 0x006F8A10 (:3363)
static bool check_and_mask_axisdir_just_pressed(unsigned int dir, unsigned int controller_id); // 0x006F8A80 (:3390)
// check_axisdir_pressed (0x006F8AF0) is external here (control_map.h): control_info names it as a friend.
static void controls_check_swap_confirm_cancel(const control_button_binding_info* button_bindings, control_config_setting control_setting, control_config& config); // 0x006FE760 (:3825)

// ---- analog_axis_info

// 0x006FDE40 (:729)
analog_axis_info::analog_axis_info()
{
    value = 0.0f;
    last_value = 0.0f;
    raw_value = 0.0f;
    repeat = 0;
    mask_axisdir_negative = false;
    masked = false;
    mask_axisdir_positive = false;
}

// 0x006F77F0 (:734)
void analog_axis_info::clear_masks()
{
    mask_axisdir_negative = false;
    masked = false;
    mask_axisdir_positive = false;
}

// 0x006F7800 (:739)
void analog_axis_info::clear()
{
    square_mapped = 0.0f;
    raw_value = 0.0f;
    last_value = 0.0f;
    value = 0.0f;
    repeat = 0;
    clear_masks(); // inlined in the binary
}

// ---- button_bits: one bit per keyboard key (0x40000000 | key), mouse button (0x20000000 | button) and game pad
// button (below 64 in the low word, 64-127 in the high word)

// 0x006F7830 (:757)
void button_bits::clear()
{
    for (int i = 0; i < 8; i++)
    {
        m_keyboard_bits[i] = 0;
    }
    m_mouse_bits = 0;
    m_controller_bits_high = 0;
    m_controller_bits_low = 0;
}

// 0x006F7860 (:766)
void button_bits::clear(unsigned int button_id)
{
    if (button_id == CONTROL_INVALID)
    {
        return;
    }
    if ((button_id & CONTROL_TYPE_MASK) == CONTROL_TYPE_KEYBOARD)
    {
        const unsigned int key = button_id & ~CONTROL_TYPE_KEYBOARD;
        m_keyboard_bits[key >> 5] &= ~(1u << (key & 31));
        return;
    }
    if ((button_id & CONTROL_TYPE_MASK) == CONTROL_TYPE_MOUSE)
    {
        m_mouse_bits &= ~(unsigned char)(1u << (button_id & 31));
        return;
    }
    m_controller_bits_high &= ~(button_id < 64 ? 0ull : 1ull << (button_id - 64));
    if (button_id < 64)
    {
        m_controller_bits_low &= ~(1ull << button_id);
    }
}

// 0x006F7940 (:794)
void button_bits::set_all()
{
    for (int i = 0; i < 8; i++)
    {
        m_keyboard_bits[i] = 0xFFFFFFFF;
    }
    m_controller_bits_high = ~0ull;
    m_controller_bits_low = ~0ull;
    m_mouse_bits = 0xFF;
}

// 0x006F7970 (:803)
void button_bits::set(unsigned int button_id)
{
    if (button_id == CONTROL_INVALID)
    {
        return;
    }
    if ((button_id & CONTROL_TYPE_MASK) == CONTROL_TYPE_KEYBOARD)
    {
        const unsigned int key = button_id & ~CONTROL_TYPE_KEYBOARD;
        m_keyboard_bits[key >> 5] |= 1u << (key & 31);
        return;
    }
    if ((button_id & CONTROL_TYPE_MASK) == CONTROL_TYPE_MOUSE)
    {
        m_mouse_bits |= (unsigned char)(1u << (button_id & 31));
        return;
    }
    m_controller_bits_high |= button_id < 64 ? 0ull : 1ull << (button_id - 64);
    if (button_id < 64)
    {
        m_controller_bits_low |= 1ull << button_id;
    }
}

// 0x006F7A40 (:831)
// Keyboard keys do not count while the console is open.
bool button_bits::is_set(unsigned int button_id)
{
    if (button_id == CONTROL_INVALID)
    {
        return false;
    }
    if ((button_id & CONTROL_TYPE_MASK) == CONTROL_TYPE_KEYBOARD)
    {
        const unsigned int key = button_id & ~CONTROL_TYPE_KEYBOARD;
        return (m_keyboard_bits[key >> 5] & (1u << (button_id & 31))) != 0 && !console_is_active();
    }
    if ((button_id & CONTROL_TYPE_MASK) == CONTROL_TYPE_MOUSE)
    {
        return (m_mouse_bits & (unsigned char)(1u << (button_id & 31))) != 0;
    }
    if (button_id < 64)
    {
        return (m_controller_bits_low & (1ull << button_id)) != 0;
    }
    return (m_controller_bits_high & (1ull << (button_id - 64))) != 0;
}

// ---- control_info

// 0x006FDE60 (:906)
control_info::control_info()
{
    // the member constructors (analog_axis_info), then clear
    clear();
}

// 0x006F7AD0 (:912)
// Also flushes the keyboard.
void control_info::clear()
{
    for (int s = 0; s < 32; s++)
    {
        stick[s][0].clear();
        stick[s][1].clear();
    }
    for (int i = 0; i < 3; i++)
    {
        mouse[i].clear();
        mouse_spike_values[i] = 0.0f;
    }
    button_down.clear();
    button_pressed.clear();
    button_pressed_mask.clear();
    button_just_pressed.clear();
    button_just_pressed_mask.clear();
    key_flush();
}

// 0x006F7D30 (:1076)
float control_info::get_stick_value(unsigned int stick_index, unsigned int axis_index) const
{
    return stick[stick_index][axis_index].masked ? 0.0f : stick[stick_index][axis_index].value;
}

// 0x006F7D70 (:1082)
float control_info::get_mouse_value(int axis_index) const
{
    return mouse[axis_index].masked ? 0.0f : mouse[axis_index].value;
}

// 0x006F7DB0 (:1088)
float control_info::get_raw_stick_value(unsigned int stick_index, unsigned int axis_index) const
{
    return stick[stick_index][axis_index].masked ? 0.0f : stick[stick_index][axis_index].raw_value;
}

// 0x006F7DF0 (:1094)
float control_info::get_raw_mouse_value(int axis_index) const
{
    return mouse[axis_index].masked ? 0.0f : mouse[axis_index].raw_value;
}

// 0x006F7E40 (:1114): as get_stick_value (the scaled value is the value).
float control_info::get_scaled_stick_value(int stick_index, int axis_index) const
{
    return stick[stick_index][axis_index].masked ? 0.0f : stick[stick_index][axis_index].value;
}

// 0x006F7E80 (:1120): as get_mouse_value.
float control_info::get_scaled_mouse_value(int axis_index) const
{
    return mouse[axis_index].masked ? 0.0f : mouse[axis_index].value;
}

// 0x006F7EC0 (:1127)
bool control_info::check_stick_as_digital_negative(int stick_index, int axis_index) const
{
    const analog_axis_info& a = stick[stick_index][axis_index];
    return !a.mask_axisdir_negative && a.value < -Control_stick_digital_sensitivity;
}

// 0x006F7F10 (:1133)
bool control_info::check_stick_as_digital_positive(int stick_index, int axis_index) const
{
    const analog_axis_info& a = stick[stick_index][axis_index];
    return !a.mask_axisdir_positive && Control_stick_digital_sensitivity < a.value;
}

// 0x006F7F50 (:1139)
bool control_info::check_mouse_as_digital_negative(int axis_index) const
{
    const analog_axis_info& a = mouse[axis_index];
    return !a.mask_axisdir_negative && a.value < -Control_stick_digital_sensitivity;
}

// 0x006F7FA0 (:1145)
bool control_info::check_mouse_as_digital_positive(int axis_index) const
{
    const analog_axis_info& a = mouse[axis_index];
    return !a.mask_axisdir_positive && Control_stick_digital_sensitivity < a.value;
}

// 0x006F7FE0 (:1193)
bool control_info::check_stick_as_digital_just_pressed_negative(int stick_index, int axis_index) const
{
    const analog_axis_info& a = stick[stick_index][axis_index];
    return !a.mask_axisdir_negative && a.value < -Control_stick_digital_sensitivity && -Control_stick_digital_sensitivity <= a.last_value;
}

// 0x006F8040 (:1199)
bool control_info::check_stick_as_digital_just_pressed_positive(int stick_index, int axis_index) const
{
    const analog_axis_info& a = stick[stick_index][axis_index];
    return !a.mask_axisdir_positive && Control_stick_digital_sensitivity < a.value && a.last_value <= Control_stick_digital_sensitivity;
}

// 0x006F80A0 (:1205)
bool control_info::check_mouse_as_digital_just_pressed_negative(int axis_index) const
{
    const analog_axis_info& a = mouse[axis_index];
    return !a.mask_axisdir_negative && a.value < -Control_stick_digital_sensitivity && -Control_stick_digital_sensitivity <= a.last_value;
}

// 0x006F8100 (:1211)
bool control_info::check_mouse_as_digital_just_pressed_positive(int axis_index) const
{
    const analog_axis_info& a = mouse[axis_index];
    return !a.mask_axisdir_positive && Control_stick_digital_sensitivity < a.value && a.last_value <= Control_stick_digital_sensitivity;
}

// 0x006F8150 (:1218)
bool control_info::check_and_mask_stick_as_digital_just_pressed_negative(int stick_index, int axis_index)
{
    if (!check_stick_as_digital_just_pressed_negative(stick_index, axis_index))
    {
        return false;
    }
    stick[stick_index][axis_index].mask_axisdir_negative = true;
    return true;
}

// 0x006F81B0 (:1228)
bool control_info::check_and_mask_stick_as_digital_just_pressed_positive(int stick_index, int axis_index)
{
    if (!check_stick_as_digital_just_pressed_positive(stick_index, axis_index))
    {
        return false;
    }
    stick[stick_index][axis_index].mask_axisdir_positive = true;
    return true;
}

// 0x006F8210 (:1238)
bool control_info::check_and_mask_mouse_as_digital_just_pressed_negative(int axis_index)
{
    if (!check_mouse_as_digital_just_pressed_negative(axis_index))
    {
        return false;
    }
    mouse[axis_index].mask_axisdir_negative = true;
    return true;
}

// 0x006F8270 (:1248)
bool control_info::check_and_mask_mouse_as_digital_just_pressed_positive(int axis_index)
{
    if (!check_mouse_as_digital_just_pressed_positive(axis_index))
    {
        return false;
    }
    mouse[axis_index].mask_axisdir_positive = true;
    return true;
}

// 0x006F8350 (:1282)
// orig_main.md OM7 (the interface message box): a repeat in the direction, used up.
bool control_info::check_and_mask_stick_repeat_negative(int stick_index, int axis_index)
{
    if (stick[stick_index][axis_index].repeat < 0)
    {
        stick[stick_index][axis_index].repeat = 0;
        return true;
    }
    return false;
}

// 0x006F8380 (:1292)
bool control_info::check_and_mask_stick_repeat_positive(int stick_index, int axis_index)
{
    if (stick[stick_index][axis_index].repeat > 0)
    {
        stick[stick_index][axis_index].repeat = 0;
        return true;
    }
    return false;
}

// 0x006F83B0 (:1302)
bool control_info::check_and_mask_mouse_repeat_negative(int axis_index)
{
    if (mouse[axis_index].repeat < 0)
    {
        mouse[axis_index].repeat = 0;
        return true;
    }
    return false;
}

// 0x006F83E0 (:1312)
bool control_info::check_and_mask_mouse_repeat_positive(int axis_index)
{
    if (mouse[axis_index].repeat > 0)
    {
        mouse[axis_index].repeat = 0;
        return true;
    }
    return false;
}

// 0x006F8410 (:1322)
bool control_info::check_button_down(unsigned int button)
{
    return button_down.is_set(button);
}

// 0x006F8420 (:1327)
// Pressed: down and repeating (the first frame and every repeat), and not masked.
bool control_info::check_button_pressed(unsigned int button)
{
    return button_pressed.is_set(button) && button_pressed_mask.is_set(button);
}

// 0x006F8460 (:1332)
bool control_info::check_button_just_pressed(unsigned int button)
{
    return button_just_pressed.is_set(button) && button_just_pressed_mask.is_set(button);
}

// 0x006F84A0 (:1337)
void control_info::mask_button(unsigned int button)
{
    button_pressed_mask.clear(button);
    button_just_pressed_mask.clear(button);
}

// 0x006F8500 (:1349)
bool control_info::button_is_masked(unsigned int button)
{
    return !button_pressed_mask.is_set(button);
}

// 0x006F8520 (:1354)
// orig_main.md OM7: every button masked (pressed and just pressed), and every stick and mouse direction.
void control_info::mask_all_buttons()
{
    button_pressed_mask.clear();
    button_just_pressed_mask.clear();
    for (int i = 0; i < 32; i++)
    {
        for (int axis = 0; axis < 2; axis++)
        {
            stick[i][axis].mask_axisdir_positive = true;
            stick[i][axis].mask_axisdir_negative = true;
        }
    }
    for (int axis = 0; axis < 3; axis++)
    {
        mouse[axis].mask_axisdir_positive = true;
        mouse[axis].mask_axisdir_negative = true;
    }
}

// 0x006F8630 (:1386)
// orig_main.md OM7: every stick and mouse axis reads 0 for the rest of the frame.
void control_info::mask_all_axes()
{
    for (int i = 0; i < 32; i++)
    {
        stick[i][0].masked = true;
        stick[i][1].masked = true;
    }
    for (int axis = 0; axis < 3; axis++)
    {
        mouse[axis].masked = true;
    }
}

// 0x006FE150 (:3472)
// The frame's controls: the game pad buttons (with the adaptive trigger deadzone of buttons 6 and 7), the sticks, the
// keys, the mouse buttons and the mouse axes (as analog values, ConvertMouseValueToAnalog). The masks start set
// (nothing masked).
void control_info::read_controls(unsigned int controller_id)
{
    if (controller_id != 0)
    {
        return;
    }
    button_down.clear();
    button_pressed.clear();
    button_just_pressed.clear();
    button_pressed_mask.set_all();
    button_just_pressed_mask.set_all();
    for (unsigned int b = 0; b < 128; b++)
    {
        const bool pressed = input_is_pressed(0, b);
        if (pressed)
        {
            button_down.set(b);
        }
        if (b == 6 || b == 7)
        {
            // The triggers: the deadzone follows the pull, so a trigger counts as pressed again after it was released
            // by the threshold (adaptive triggers).
            const float raw = input_get_raw_value(0, b);
            const float deadzone = input_get_deadzone(0, b);
            float new_zone;
            if (pressed)
            {
                new_zone = raw - Adaptive_Trigger_threshold;
                if (deadzone - (raw - Adaptive_Trigger_threshold) >= 0.0f)
                {
                    new_zone = deadzone;
                }
            }
            else
            {
                new_zone = Adaptive_Trigger_threshold + raw;
                if (deadzone - new_zone < 0.0f)
                {
                    new_zone = deadzone;
                }
            }
            input_set_deadzone(0, b, new_zone);
        }
        if (input_did_repeat(0, b))
        {
            button_pressed.set(b);
        }
        if (input_just_pressed(0, b))
        {
            button_just_pressed.set(b);
        }
    }
    for (unsigned int s = 0; s < 32; s++)
    {
        analog_axis_info& x = stick[s][0];
        analog_axis_info& y = stick[s][1];
        x.last_value = x.value;
        y.last_value = y.value;
        input_get_joy(x.value, y.value, 0, s);
        input_get_joy_raw(x.raw_value, y.raw_value, 0, s);
        if (Control_bounceback_protection)
        {
            // a stick that snaps back past the center reads as centered for that frame
            if ((x.last_value < 0.0f && 0.0f < x.value) || (0.0f < x.last_value && x.value < 0.0f))
            {
                x.value = 0.0f;
            }
            if ((y.last_value < 0.0f && 0.0f < y.value) || (0.0f < y.last_value && y.value < 0.0f))
            {
                y.value = 0.0f;
            }
        }
        for (unsigned int a = 0; a < 2; a++)
        {
            analog_axis_info& axis = stick[s][a];
            if (input_did_scroll(0, s, a * 2))
            {
                axis.repeat = -1;
            }
            else
            {
                axis.repeat = input_did_scroll(0, s, a * 2 + 1) ? 1 : 0;
            }
            axis.clear_masks();
        }
    }
    for (unsigned int k = 0; k < 256; k++)
    {
        if (key_down(k))
        {
            const unsigned int control = k | CONTROL_TYPE_KEYBOARD;
            button_down.set(control);
            bool just_pressed;
            if (key_pressed(k, &just_pressed))
            {
                button_pressed.set(control);
            }
            if (just_pressed)
            {
                button_just_pressed.set(control);
            }
        }
    }
    for (unsigned int m = 0; m < 5; m++)
    {
        if (mouse_is_pressed(m))
        {
            const unsigned int control = m | CONTROL_TYPE_MOUSE;
            button_down.set(control);
            if (mouse_did_repeat(m))
            {
                button_pressed.set(control);
            }
            if (mouse_just_pressed(m))
            {
                button_just_pressed.set(control);
            }
        }
    }
    int deltas[3] = {0, 0, 0};
    if (os_foreground())
    {
        mouse_get_deltas(deltas[0], deltas[1], deltas[2]);
    }
    for (unsigned int i = 0; i < 3; i++)
    {
        analog_axis_info& axis = mouse[i];
        axis.last_value = axis.value;
        float uncapped;
        const float v = ConvertMouseValueToAnalog(deltas[i], i, uncapped);
        axis.value = v;
        axis.raw_value = uncapped;
        if (Control_bounceback_protection && ((axis.last_value < 0.0f && 0.0f < v) || (0.0f < axis.last_value && v < 0.0f)))
        {
            axis.value = 0.0f;
        }
        if (mouse_did_scroll(i, false))
        {
            axis.repeat = -1;
        }
        else
        {
            axis.repeat = mouse_did_scroll(i, true) ? 1 : 0;
        }
        axis.clear_masks();
    }
}

// ---- stick and mouse directions bound as buttons (stick_dir): mouse or stick, axis in bits 0-7, stick in bits 8-15,
// CONTROL_AXISDIR_POSITIVE for the positive direction

// 0x006F89A0 (:3310)
static bool check_axisdir_down(unsigned int dir, unsigned int controller_id)
{
    if (controller_id != 0 || dir == CONTROL_INVALID)
    {
        return false;
    }
    const bool positive = (dir & CONTROL_AXISDIR_POSITIVE) == CONTROL_AXISDIR_POSITIVE;
    const unsigned int axis_index = dir & 0xFF;
    const unsigned int stick_index = (dir >> 8) & 0xFF;
    if ((dir & CONTROL_TYPE_MASK) != CONTROL_TYPE_MOUSE)
    {
        return positive ? Control_info[0].check_stick_as_digital_positive(stick_index, axis_index) : Control_info[0].check_stick_as_digital_negative(stick_index, axis_index);
    }
    return positive ? Control_info[0].check_mouse_as_digital_positive(axis_index) : Control_info[0].check_mouse_as_digital_negative(axis_index);
}

// 0x006F8A10 (:3363)
static bool check_axisdir_just_pressed(unsigned int dir, unsigned int controller_id)
{
    if (controller_id != 0 || dir == CONTROL_INVALID)
    {
        return false;
    }
    const bool positive = (dir & CONTROL_AXISDIR_POSITIVE) == CONTROL_AXISDIR_POSITIVE;
    const unsigned int axis_index = dir & 0xFF;
    const unsigned int stick_index = (dir >> 8) & 0xFF;
    if ((dir & CONTROL_TYPE_MASK) != CONTROL_TYPE_MOUSE)
    {
        return positive ? Control_info[0].check_stick_as_digital_just_pressed_positive(stick_index, axis_index) : Control_info[0].check_stick_as_digital_just_pressed_negative(stick_index, axis_index);
    }
    return positive ? Control_info[0].check_mouse_as_digital_just_pressed_positive(axis_index) : Control_info[0].check_mouse_as_digital_just_pressed_negative(axis_index);
}

// 0x006F8A80 (:3390)
static bool check_and_mask_axisdir_just_pressed(unsigned int dir, unsigned int controller_id)
{
    if (controller_id != 0 || dir == CONTROL_INVALID)
    {
        return false;
    }
    const bool positive = (dir & CONTROL_AXISDIR_POSITIVE) == CONTROL_AXISDIR_POSITIVE;
    const unsigned int axis_index = dir & 0xFF;
    const unsigned int stick_index = (dir >> 8) & 0xFF;
    if ((dir & CONTROL_TYPE_MASK) != CONTROL_TYPE_MOUSE)
    {
        return positive ? Control_info[0].check_and_mask_stick_as_digital_just_pressed_positive(stick_index, axis_index) : Control_info[0].check_and_mask_stick_as_digital_just_pressed_negative(stick_index, axis_index);
    }
    return positive ? Control_info[0].check_and_mask_mouse_as_digital_just_pressed_positive(axis_index) : Control_info[0].check_and_mask_mouse_as_digital_just_pressed_negative(axis_index);
}

// 0x006F8B90
// orig_main.md OM7: a repeat of the stick or mouse in the direction, used up.
static bool check_and_mask_axisdir_pressed(unsigned int dir, unsigned int controller_id)
{
    if (controller_id != 0 || dir == CONTROL_INVALID)
    {
        return false;
    }
    const bool positive = (dir & CONTROL_AXISDIR_POSITIVE) == CONTROL_AXISDIR_POSITIVE;
    const unsigned int axis_index = dir & 0xFF;
    const unsigned int stick_index = (dir >> 8) & 0xFF;
    if ((dir & CONTROL_TYPE_MASK) != CONTROL_TYPE_MOUSE)
    {
        return positive ? Control_info[0].check_and_mask_stick_repeat_positive(stick_index, axis_index) : Control_info[0].check_and_mask_stick_repeat_negative(stick_index, axis_index);
    }
    return positive ? Control_info[0].check_and_mask_mouse_repeat_positive(axis_index) : Control_info[0].check_and_mask_mouse_repeat_negative(axis_index);
}

// 0x006F8AF0 (:3417)
// Pressed: the stick or mouse scrolled (repeat) in the direction.
bool check_axisdir_pressed(unsigned int dir, unsigned int controller_id)
{
    if (controller_id != 0 || dir == CONTROL_INVALID)
    {
        return false;
    }
    const bool positive = (dir & CONTROL_AXISDIR_POSITIVE) == CONTROL_AXISDIR_POSITIVE;
    const unsigned int axis_index = dir & 0xFF;
    const unsigned int stick_index = (dir >> 8) & 0xFF;
    if ((dir & CONTROL_TYPE_MASK) != CONTROL_TYPE_MOUSE)
    {
        const int repeat = Control_info[0].stick[stick_index][axis_index].repeat;
        return positive ? 0 < repeat : repeat < 0;
    }
    const int repeat = Control_info[0].mouse[axis_index].repeat;
    return positive ? 0 < repeat : repeat < 0;
}

// ---- configuration

// 0x006F8C10 (:3917)
control_config_setting controls_get_current_config_setting()
{
    if (!input_get_playing_with_controller())
    {
        return CCS_KEYBOARD_MOUSE;
    }
    switch (input_get_controller_type())
    {
    case keen::ControllerType_DualShock4:
        return CCS_PS4;
    case keen::ControllerType_Xbox360:
        return CCS_XBOX;
    case keen::ControllerType_XBone:
        return CCS_XBONE;
    case keen::ControllerType_Steam:
        return CCS_STEAM;
    case keen::ControllerType_JoyConPair:
    case keen::ControllerType_SwitchProController:
        return CCS_SWITCH;
    default:
        return CCS_GENERIC;
    }
}

// 0x006FE760 (:3825)
// The Switch layout swaps accept and cancel.
static void controls_check_swap_confirm_cancel(const control_button_binding_info* button_bindings, control_config_setting control_setting, control_config& config)
{
    if (control_setting != CCS_SWITCH)
    {
        return;
    }
    Accept_and_back_reversed = true;
    for (int i = 0; i < Num_reversed_accept_cancel_bindings; i++)
    {
        const control_button_binding_info& b = Reversed_accept_cancel_bindings[i];
        button_binding_data& data = config.button_bindings[b.action];
        // Original bug kept: the alternate button and stick direction come from the default table being installed
        // (button_bindings), not from the reversed table.
        data.button[0] = b.button;
        data.button[1] = button_bindings[i].alt_button;
        data.stick_dir = button_bindings[i].stick_dir;
    }
}

// 0x006F9390 (:4633)
void controls_set_button_binding(control_button_action action, control_config_setting control_setting, unsigned int button, unsigned int alt_button, unsigned int stick_dir, unsigned int controller_id)
{
    if (controller_id == 0)
    {
        button_binding_data& data = Control_config[0][control_setting].button_bindings[action];
        data.button[0] = button;
        data.button[1] = alt_button;
        data.stick_dir = stick_dir;
    }
}

// 0x006F93E0 (:4643)
void controls_set_axis_binding(control_axis_action action, control_config_setting control_setting, unsigned int axis, bool inverted, unsigned int alt_axis, bool alt_inverted, unsigned int controller_id)
{
    if (controller_id == 0)
    {
        axis_binding_data& data = Control_config[0][control_setting].axis_bindings[action];
        data.axis[0] = axis;
        data.inverted[0] = inverted;
        data.axis[1] = alt_axis;
        data.inverted[1] = alt_inverted;
    }
}

// 0x006FEDB0 (:4713)
// All-zero entries are skipped.
void controls_set_button_bindings(const control_button_binding_info* button_bindings, int num_button_bindings, control_config_setting control_setting, unsigned int controller_id)
{
    if (controller_id != 0)
    {
        return;
    }
    for (int i = 0; i < num_button_bindings; i++)
    {
        // Original: an inlined memcmp of the whole binding against a zeroed one.
        const control_button_binding_info& b = button_bindings[i];
        static const control_button_binding_info zero_binding = {};
        if (memcmp(&b, &zero_binding, sizeof(b)) != 0)
        {
            controls_set_button_binding(b.action, control_setting, b.button, b.alt_button, b.stick_dir, 0);
        }
    }
    controls_check_swap_confirm_cancel(button_bindings, control_setting, Control_config[0][control_setting]);
}

// 0x006F95E0 (:4794)
bool controls_get_axis_action_inverted_state(control_axis_action action, unsigned int controller_id, bool primary)
{
    if ((int)action > CAA_MAP_PAN_CAMERA_UP_DOWN || controller_id != 0)
    {
        return false;
    }
    const axis_binding_data& data = Control_config[0][controls_get_current_config_setting()].axis_bindings[action];
    if (data.axis[!primary] == CONTROL_INVALID)
    {
        return false;
    }
    return data.inverted[!primary];
}

// ---- system and per-frame update

// 0x006FDFA0 (:3252)
// A mouse delta (pixels this frame) as an analog value: scaled by the sensitivity and to a 30 Hz frame, square-rooted
// and capped at +-Mouse_cap. uncapped_value is the scaled delta before the root. The wheel (axis 2) is its sign.
float ConvertMouseValueToAnalog(int source_value, unsigned int axis_idx, float& uncapped_value)
{
    uncapped_value = 0.0f;
    if (axis_idx == 1)
    {
        source_value = -source_value; // mouse y grows downwards
    }
    else if (axis_idx == 2)
    {
        if (source_value == 0)
        {
            return 0.0f;
        }
        return (float)(source_value / abs(source_value));
    }
    const float scaled = (float)((double)(float)source_value * (double)Mouse_sensitivity * (double)Mouse_pixel_multiplier);
    float frametime = Frametime;
    if ((float)((double)Frametime - (double)0.001f) < 0.0f)
    {
        frametime = 0.001f;
    }
    // 0.033: one frame at 30 Hz, so the value does not depend on the frame rate
    const float rate = (float)((double)0.033f / (double)frametime);
    const float v = (float)((double)rate * (double)scaled);
    uncapped_value = v;
    float root;
    if (0.0f > v)
    {
        root = -(float)sqrt((double)fabsf(v));
    }
    else
    {
        root = (float)sqrt((double)v);
    }
    if ((double)-Mouse_cap > (double)root)
    {
        return -Mouse_cap;
    }
    if ((double)root > (double)Mouse_cap)
    {
        return Mouse_cap;
    }
    return root;
}

// 0x006FE0F0 (:3291)
// LTCG: controller_id in EDI, bits in ESI. The action's bound buttons (up to two) into bits, their count returned.
static int bound_buttons_from_action(control_button_action action, unsigned int controller_id, unsigned int* bits)
{
    if (controller_id >= 1)
    {
        return 0;
    }
    bits[1] = CONTROL_INVALID;
    bits[0] = CONTROL_INVALID;
    const button_binding_data& data = Control_config[controller_id][controls_get_current_config_setting()].button_bindings[action];
    int count = 0;
    if (data.button[0] != CONTROL_INVALID)
    {
        bits[0] = data.button[0];
        count = 1;
    }
    if (data.button[1] != CONTROL_INVALID)
    {
        bits[count] = data.button[1];
        count++;
    }
    return count;
}

// 0x007030A0 (:2572)
static void controls_create_hash_tables()
{
    // The binary adds each name in straight-line code: clear, then hash_table::add and the entry data per name.
    static const struct
    {
        const char* name;
        control_button_action action;
    } button_names[] = {
        {"weapon_menu", CBA_GAC_WEAPON_MENU},
        {"hold_object", CBA_GAC_HOLD_OBJECT},
        {"scoreboard", CBA_GAC_SCOREBOARD},
        {"switch_class", CBA_GAC_SWITCH_CLASS},
        {"respawn", CBA_GAC_RESPAWN},
        {"use_backpack", CBA_GAC_TOGGLE_BACKPACK_ACTIVATION},
        {"vote_yes", CBA_GAC_VOTE_YES},
        {"vote_no", CBA_GAC_VOTE_NO},
        {"help", CBA_GAC_HELP},
        {"spec_next", CBA_SPECTATOR_NEXT_TARGET},
        {"spec_prev", CBA_SPECTATOR_PREV_TARGET},
        {"spec_lock", CBA_SPECTATOR_LOCK_CAMERA},
        {"spec_unlock", CBA_SPECTATOR_UNLOCK_CAMERA},
        {"spec_next_play", CBA_SPECTATOR_NEXT_PLAYER},
        {"spec_prev_play", CBA_SPECTATOR_PREV_PLAYER},
        {"spec_custom", CBA_SPECTATOR_MODE_CUSTOM},
        {"spec_aquarium", CBA_SPECTATOR_MODE_AQUARIUM},
        {"spec_action", CBA_SPECTATOR_MODE_ACTION},
        {"spec_player", CBA_SPECTATOR_MODE_PLAYER},
        {"spec_hud", CBA_SPECTATOR_TOGGLE_HUD},
        {"primary_attack", CBA_OFC_ATTACK_PRIMARY},
        {"melee_attack", CBA_OFC_ATTACK_MELEE},
        {"jump", CBA_OFC_JUMP},
        {"sprint", CBA_OFC_SPRINT},
        {"crouch", CBA_OFC_CROUCH},
        {"reload", CBA_OFC_RELOAD},
        {"zoom", CBA_OFC_ZOOM},
        {"cover", CBA_OFC_COVER},
        {"of_action", CBA_OFC_ACTION},
        {"of_detonate", CBA_OFC_CHARGE_DETONATE},
        {"veh_accelerate", CBA_VDC_ACCELERATE},
        {"veh_brake", CBA_VDC_BRAKE_REVERSE},
        {"veh_handbrake", CBA_VDC_HANDBRAKE},
        {"veh_firearm", CBA_VDC_FIREARM},
        {"veh_camera", CBA_VDC_CYCLE_CAMERA_MODE},
        {"veh_rear_view", CBA_VDC_LOOK_BACK},
        {"veh_mech_melee1", CBA_VDC_MECH_MELEE1},
        {"veh_mech_melee2", CBA_VDC_MECH_MELEE2},
        {"veh_mech_melee3", CBA_VDC_MECH_MELEE3},
        {"veh_mech_melee4", CBA_VDC_MECH_MELEE4},
        {"veh_mech_charge", CBA_VDC_MECH_CHARGE},
        {"veh_action", CBA_VDC_ACTION},
        {"veh_zoom", CBA_VDC_TURRET_ZOOM},
        {"veh_change_seat", CBA_VDC_CHANGE_SEAT},
        {"veh_tank_fire_pri", CBA_VDC_TANK_FIRE_PRIMARY},
        {"veh_tank_fire_sec", CBA_VDC_TANK_FIRE_SECONDARY},
        {"wm_up", CBA_WEAPON_MENU_UP},
        {"wm_dpad_up", CBA_WEAPON_MENU_UP_DPAD},
        {"wm_down", CBA_WEAPON_MENU_DOWN},
        {"wm_dpad_down", CBA_WEAPON_MENU_DOWN_DPAD},
        {"wm_left", CBA_WEAPON_MENU_LEFT},
        {"wm_dpad_left", CBA_WEAPON_MENU_LEFT_DPAD},
        {"wm_right", CBA_WEAPON_MENU_RIGHT},
        {"wm_dpad_right", CBA_WEAPON_MENU_RIGHT_DPAD},
        {"map", CBA_MAP},
        {"menu_up_no_stick", CBA_MENU_UP_NO_STICK},
        {"menu_down_no_stick", CBA_MENU_DOWN_NO_STICK},
        {"menu_left_no_stick", CBA_MENU_LEFT_NO_STICK},
        {"menu_right_no_stick", CBA_MENU_RIGHT_NO_STICK},
        {"menu_up", CBA_MENU_UP},
        {"menu_down", CBA_MENU_DOWN},
        {"menu_left", CBA_MENU_LEFT},
        {"menu_right", CBA_MENU_RIGHT},
        {"menu_alt_up", CBA_MENU_ALT_UP},
        {"menu_alt_down", CBA_MENU_ALT_DOWN},
        {"menu_alt_left", CBA_MENU_ALT_LEFT},
        {"menu_alt_right", CBA_MENU_ALT_RIGHT},
        {"menu_start", CBA_MENU_START},
        {"menu_quit", CBA_MENU_QUIT},
        {"menu_accept", CBA_MENU_SELECT},
        {"menu_abort", CBA_MENU_ABORT},
        {"menu_page_up", CBA_MENU_PGUP},
        {"menu_page_down", CBA_MENU_PGDN},
        {"menu_x", CBA_MENU_X},
        {"menu_y", CBA_MENU_Y},
        {"menu_lt", CBA_MENU_L1},
        {"menu_lb", CBA_MENU_L2},
        {"menu_rt", CBA_MENU_R1},
        {"menu_rb", CBA_MENU_R2},
        {"minimap_zoom_in", CBA_MINIMAP_ZOOM_IN},
        {"minimap_zoom_out", CBA_MINIMAP_ZOOM_OUT},
        {"skip_modal", CBA_SKIP_MODAL_SEQUENCE},
        {"sat_zoom", CBA_SAT_ZOOM},
        {"sat_fire", CBA_SAT_FIRE},
        {"sat_hold", CBA_SAT_HOLD},
        {"sat_return", CBA_SAT_RETURN},
        {"gps_marker", CBA_GPS_MARKER},
        {"menu_back", CBA_MENU_BACK},
        {"gameplay_pause", CBA_GAMEPLAY_PAUSE},
        {"tooltip_resume", CBA_TOOLTIP_RESUME},
        {"start_game", CBA_START_GAME},
        {"fsm_place_beacon", CBA_FSM_PLACE_BEACON},
        {"view_gamercard", CBA_MP_VIEW_GAMERCARD},
        {"cntrl_cfg", CBA_CNTRL_CONFIG},
        {"cntrl_cfg_clear", CBA_CNTRL_CLEAR},
        {"ctrl_cfg_invert", CBA_CNTRL_INVERT},
        {"open_chat", CBA_OPEN_CHAT},
        {"open_team_chat", CBA_OPEN_TEAM_CHAT},
    };
    static const struct
    {
        const char* name;
        control_axis_action action;
    } stick_names[] = {
        {"cam_rotate", CAA_CAMERA_ROTATE},
        {"cam_elevate", CAA_CAMERA_ELEVATE},
        {"move", CAA_WALK_FORWARD_BACKWARD},
        {"strafe", CAA_WALK_TURN_LEFT_RIGHT},
        {"veh_steer", CAA_DRIVE_STEER},
        {"veh_steer_vert", CAA_DRIVE_STEER_VERT},
        {"veh_cam_rotate", CAA_DRIVE_CAMERA_ROTATE},
        {"veh_cam_elevate", CAA_DRIVE_CAMERA_ELEVATE},
        {"tank_steer", CAA_TANK_STEER},
        {"tank_steer_vert", CAA_TANK_STEER_VERT},
        {"tank_cam_rotate", CAA_TANK_CAMERA_ROTATE},
        {"tank_cam_elevate", CAA_TANK_CAMERA_ELEVATE},
        {"zoom_in_out", CAA_ZOOM_IN_OUT},
        {"pan_left_right", CAA_MAP_PAN_CAMERA_LEFT_RIGHT},
        {"pan_up_down", CAA_MAP_PAN_CAMERA_UP_DOWN},
    };
    Button_action_name_hash.clear();
    for (const auto& n : button_names)
    {
        Button_action_name_hash.insert_key_data(localization_generate_hash_from_key(n.name), n.action);
    }
    Stick_action_name_hash.clear();
    for (const auto& n : stick_names)
    {
        Stick_action_name_hash.insert_key_data(localization_generate_hash_from_key(n.name), n.action);
    }
}

// 0x007046A0 (:3951)
void controls_system_init()
{
    for (int setting = CCS_KEYBOARD_MOUSE; setting < NUM_CONTROL_CONFIG_SETTINGS; setting++)
    {
        control_config& config = Control_config[0][setting];
        for (int i = 0; i < NUM_CONTROL_BUTTON_ACTIONS; i++)
        {
            config.button_bindings[i].button[0] = CONTROL_INVALID;
            config.button_bindings[i].button[1] = CONTROL_INVALID;
            config.button_bindings[i].stick_dir = CONTROL_INVALID;
        }
        controls_set_button_bindings(Control_button_default_configs[setting], Control_button_default_config_sizes[setting], (control_config_setting)setting, 0);
        const control_axis_binding_info* axes = Control_axis_default_configs[setting];
        for (int i = 0; i < Control_axis_default_config_sizes[setting]; i++)
        {
            controls_set_axis_binding(axes[i].action, (control_config_setting)setting, axes[i].axis, axes[i].inverted, axes[i].alt_axis, axes[i].alt_inverted, 0);
        }
    }
    controls_create_hash_tables();
}

// 0x00702230 (:3985)
void controls_poll()
{
    // The Steam controller action set: 0 on foot, 1/2 in a car/tank, 3 in a flyer or walker, 4 in menus.
    unsigned int action_set;
    bool skip_action_set = false;
    if (ui_is_msgbox_active(-2) || video_cutscene_pending_or_playing() || gameseq_get_state() != GS_GAMEPLAY)
    {
        action_set = 4;
    }
    else if (hud_ui_multiplayer_scoreboard_is_active() || (Multiplayer && (hud_ui_multiplayer_info_text_is_active() || hud_ui_multiplayer_splash_screen_is_active() || multi_game_camera_is_running())) || (wrecking_crew_is_active() && !wrecking_crew_can_player_move()) || multi_game_spectating())
    {
        action_set = 4;
    }
    else
    {
        player* pp = player_get_local_player();
        if (pp == nullptr || human_in_vehicle(pp) == nullptr)
        {
            action_set = 0;
        }
        else
        {
            vehicle* vp = World.object_from_handle<vehicle>(pp->vehicle_handle);
            action_set = 0;
            if (vp == nullptr)
            {
                skip_action_set = true;
            }
            else if (vp->get_sub_type() == 4)
            {
                action_set = vehicle_is_tank(vp) ? 2 : 1;
            }
            else if ((unsigned int)(vp->get_sub_type() - 5) < 2)
            {
                action_set = 3;
            }
            else
            {
                VLIB_CRITICAL_FAILURE();
                skip_action_set = true;
            }
        }
    }
    if (!skip_action_set)
    {
        keen::input::setSteamControllerActionSet(Keen_input_system, action_set);
    }
    input_poll();
    key_poll();
    mouse_poll();
    // Original behaviour kept: os_received_input runs every poll, except when the first just released game pad button
    // is button 0 and the changed axis found next is game pad stick 0's x axis (control 0).
    bool received = true;
    for (unsigned int b = 0; b < 128; b++)
    {
        if (input_just_released_no_clear(0, b))
        {
            if (b == 0)
            {
                unsigned int changed = controls_any_mouse_axis_changed();
                if (changed == CONTROL_INVALID)
                {
                    changed = controls_any_gamepad_axis_changed();
                }
                if (changed != CONTROL_INVALID && changed == 0)
                {
                    received = false;
                }
            }
            break;
        }
    }
    if (received)
    {
        os_received_input();
    }
    const bool clear = multi_game_should_clear_controls() || multi_core_platform_ui_is_active();
    Control_info[0].read_controls(0);
    if (clear)
    {
        Control_info[0].clear();
    }
}

// ---- queries used by the simulation

// 0x006FE8C0 (:4113)
bool controls_test_button_action(control_button_action action, unsigned int controller_id, bool (control_info::*test_func)(unsigned int), mask_state should_mask)
{
    unsigned int bits[2];
    const int count = bound_buttons_from_action(action, controller_id, bits);
    bool result = false;
    for (int i = 0; i < count; i++)
    {
        if ((Control_info[controller_id].*test_func)(bits[i]))
        {
            result = true;
            break;
        }
    }
    if (should_mask == USE_MASKING && result)
    {
        for (int i = 0; i < count; i++)
        {
            Control_info[controller_id].button_pressed_mask.clear(bits[i]);
            Control_info[controller_id].button_just_pressed_mask.clear(bits[i]);
        }
    }
    return result;
}

// 0x006FE970 (:4142)
bool controls_is_button_down(control_button_action action, unsigned int controller_id)
{
    if (controller_id != 0)
    {
        return false;
    }
    if (controls_test_button_action(action, 0, &control_info::check_button_down, NO_MASKING))
    {
        return true;
    }
    return check_axisdir_down(Control_config[0][controls_get_current_config_setting()].button_bindings[action].stick_dir, 0);
}

// 0x006FE9D0 (:4164)
bool controls_is_button_pressed(control_button_action action, unsigned int controller_id)
{
    if (controller_id != 0)
    {
        return false;
    }
    if (controls_test_button_action(action, 0, &control_info::check_button_pressed, NO_MASKING))
    {
        return true;
    }
    return check_axisdir_pressed(Control_config[0][controls_get_current_config_setting()].button_bindings[action].stick_dir, 0);
}

// 0x006FEA30 (:4176)
bool controls_is_button_just_pressed(control_button_action action, unsigned int controller_id)
{
    if (controller_id != 0)
    {
        return false;
    }
    if (controls_test_button_action(action, 0, &control_info::check_button_just_pressed, NO_MASKING))
    {
        return true;
    }
    return check_axisdir_just_pressed(Control_config[0][controls_get_current_config_setting()].button_bindings[action].stick_dir, 0);
}

// 0x006F8C80 (:4188)
// 1 for a pressed key or mouse button, the analog value of a pressed game pad button, else 0.
float controls_get_button_analog(control_button_action action, unsigned int controller_id)
{
    if (controller_id != 0)
    {
        return 0.0f;
    }
    for (unsigned int i = 0; i < 2; i++)
    {
        const unsigned int button_id = Control_config[0][controls_get_current_config_setting()].button_bindings[action].button[i];
        if (button_id == CONTROL_INVALID)
        {
            continue;
        }
        if ((button_id & CONTROL_TYPE_MASK) == CONTROL_TYPE_KEYBOARD)
        {
            if (key_pressed(button_id & ~CONTROL_TYPE_KEYBOARD, nullptr) && !console_is_active())
            {
                return 1.0f;
            }
        }
        else if ((button_id & CONTROL_TYPE_MASK) == CONTROL_TYPE_MOUSE)
        {
            if (mouse_is_pressed(button_id & ~CONTROL_TYPE_MOUSE))
            {
                return 1.0f;
            }
        }
        else
        {
            const float value = input_get_raw_value(0, button_id);
            if (0.0f < value)
            {
                return value;
            }
        }
    }
    return 0.0f;
}

// 0x006F8D40 (:4232)
int controls_get_button_for_action(control_button_action action, unsigned int controller_id)
{
    if (controller_id != 0)
    {
        return 0;
    }
    if (Control_config[0][controls_get_current_config_setting()].button_bindings[action].button[0] == CONTROL_INVALID)
    {
        return Control_config[0][controls_get_current_config_setting()].button_bindings[action].button[1];
    }
    return Control_config[0][controls_get_current_config_setting()].button_bindings[action].button[0];
}

// 0x006FEA90 (:4257)
// orig_main.md OM7: pressed (with repeats), and then masked for the rest of the frame.
bool controls_process_button_pressed(control_button_action action, unsigned int controller_id)
{
    if (controller_id != 0)
    {
        return false;
    }
    if (controls_test_button_action(action, 0, &control_info::check_button_pressed, USE_MASKING))
    {
        return true;
    }
    return check_and_mask_axisdir_pressed(Control_config[0][controls_get_current_config_setting()].button_bindings[action].stick_dir, 0);
}

// 0x006FEAF0 (:4269)
// Just pressed, and then masked for the rest of the frame.
bool controls_process_button_just_pressed(control_button_action action, unsigned int controller_id)
{
    if (controller_id != 0)
    {
        return false;
    }
    if (controls_test_button_action(action, 0, &control_info::check_button_just_pressed, USE_MASKING))
    {
        return true;
    }
    return check_and_mask_axisdir_just_pressed(Control_config[0][controls_get_current_config_setting()].button_bindings[action].stick_dir, 0);
}

// The value of one bound axis control (shared by controls_get_analog and controls_get_raw_analog_uncapped, whose
// bodies the binary repeats): two keys or buttons give 1, 0 or -1, a mouse axis or stick its (raw) value.
static float controls_axis_control_value(unsigned int axis, bool raw)
{
    const bool as_buttons = ((axis >> 27) & 1) != 0;
    unsigned int positive;
    unsigned int negative;
    if ((axis & CONTROL_TYPE_MASK) == CONTROL_TYPE_KEYBOARD)
    {
        positive = ((axis >> 8) & 0xFF) | CONTROL_TYPE_KEYBOARD;
        negative = (axis & 0xFF) | CONTROL_TYPE_KEYBOARD;
    }
    else if ((axis & CONTROL_TYPE_MASK) == CONTROL_TYPE_MOUSE)
    {
        if (!as_buttons)
        {
            return raw ? Control_info[0].get_raw_mouse_value(axis & 0xFF) : Control_info[0].get_mouse_value(axis & 0xFF);
        }
        positive = ((axis >> 8) & 0xFF) | CONTROL_TYPE_MOUSE;
        negative = (axis & 0xFF) | CONTROL_TYPE_MOUSE;
    }
    else
    {
        if (!as_buttons)
        {
            return raw ? Control_info[0].get_raw_stick_value((axis >> 8) & 0xFF, axis & 0xFF) : Control_info[0].get_stick_value((axis >> 8) & 0xFF, axis & 0xFF);
        }
        positive = (axis >> 8) & 0xFF;
        negative = axis & 0xFF;
    }
    const float p = Control_info[0].check_button_pressed(positive) ? 1.0f : 0.0f;
    const float n = Control_info[0].check_button_pressed(negative) ? 1.0f : 0.0f;
    return (float)(((double)p + 0.0) - (double)n);
}

// 0x006F8E30 (:4282)
// The sum of the action's (up to two) bound axes, inverted per binding, clamped to -1..1.
float controls_get_analog(control_axis_action action, unsigned int controller_id)
{
    if (controller_id != 0)
    {
        return 0.0f;
    }
    float sum = 0.0f;
    for (int i = 0; i < 2; i++)
    {
        const unsigned int axis = Control_config[0][controls_get_current_config_setting()].axis_bindings[action].axis[i];
        if (axis == CONTROL_INVALID)
        {
            continue;
        }
        float v = controls_axis_control_value(axis, false);
        if (Control_config[0][controls_get_current_config_setting()].axis_bindings[action].inverted[i])
        {
            v = -v;
        }
        sum = v + sum;
    }
    if (sum - -1.0f < 0.0f)
    {
        sum = -1.0f;
    }
    if (sum - 1.0f >= 0.0f)
    {
        sum = 1.0f;
    }
    return sum;
}

// 0x006F90B0 (:4375)
float controls_get_raw_analog_uncapped(control_axis_action action, unsigned int controller_id)
{
    if (controller_id != 0)
    {
        return 0.0f;
    }
    float sum = 0.0f;
    for (int i = 0; i < 2; i++)
    {
        const unsigned int axis = Control_config[0][controls_get_current_config_setting()].axis_bindings[action].axis[i];
        if (axis == CONTROL_INVALID)
        {
            continue;
        }
        float v = controls_axis_control_value(axis, true);
        if (Control_config[0][controls_get_current_config_setting()].axis_bindings[action].inverted[i])
        {
            v = -v;
        }
        sum = v + sum;
    }
    return sum;
}

// 0x006F92E0 (:4468)
float controls_get_raw_analog(control_axis_action action, unsigned int controller_id)
{
    float v = controls_get_raw_analog_uncapped(action, controller_id);
    if (v - -1.0f < 0.0f)
    {
        v = -1.0f;
    }
    if (v - 1.0f >= 0.0f)
    {
        v = 1.0f;
    }
    return v;
}

// 0x006FEB50 (:4475)
// As controls_get_analog with the scaled stick and mouse values (the ladder's climb stick reads it).
float controls_get_scaled_analog(control_axis_action action, unsigned int controller_id)
{
    if (controller_id != 0)
    {
        return 0.0f;
    }
    float sum = 0.0f;
    for (int i = 0; i < 2; i++)
    {
        const unsigned int axis = Control_config[0][controls_get_current_config_setting()].axis_bindings[action].axis[i];
        if (axis == CONTROL_INVALID)
        {
            continue;
        }
        const bool as_buttons = ((axis >> 27) & 1) != 0;
        float v;
        if ((axis & CONTROL_TYPE_MASK) == CONTROL_TYPE_KEYBOARD || as_buttons)
        {
            v = controls_axis_control_value(axis, false); // inlined: the buttons' difference
        }
        else if ((axis & CONTROL_TYPE_MASK) == CONTROL_TYPE_MOUSE)
        {
            v = Control_info[0].get_scaled_mouse_value(axis & 0xFF);
        }
        else
        {
            v = Control_info[0].get_scaled_stick_value((axis >> 8) & 0xFF, axis & 0xFF);
        }
        if (Control_config[0][controls_get_current_config_setting()].axis_bindings[action].inverted[i])
        {
            v = -v;
        }
        sum = v + sum;
    }
    if (sum < -1.0f)
    {
        sum = -1.0f;
    }
    else if (1.0f < sum)
    {
        sum = 1.0f;
    }
    return sum;
}

// 0x006FEEB0 (:4866)
void controls_mask_button(control_button_action action, unsigned int controller_id)
{
    if (controller_id >= 1)
    {
        return;
    }
    unsigned int bits[2];
    const int count = bound_buttons_from_action(action, controller_id, bits);
    for (int i = 0; i < count; i++)
    {
        Control_info[controller_id].mask_button(bits[i]);
    }
}

// 0x006FEF50 (:4894)
bool controls_button_is_masked(control_button_action action, unsigned int controller_id)
{
    if (controller_id != 0)
    {
        return false;
    }
    unsigned int bits[2];
    const int count = bound_buttons_from_action(action, 0, bits);
    if (count == 0)
    {
        return false;
    }
    for (int i = 0; i < count; i++)
    {
        if (!Control_info[0].button_is_masked(bits[i]))
        {
            return false;
        }
    }
    return true;
}

// 0x006F98D0 (:4989)
// orig_main.md OM7 (the interface message box): every button and axis of the controller masked for the rest of the
// frame (mask_all_axes inlined).
void controls_mask_all(unsigned int controller_id)
{
    if (controller_id == 0)
    {
        Control_info[0].mask_all_buttons();
        Control_info[0].mask_all_axes();
    }
}

// 0x006F9930 (:5001)
void controls_map_stick_to_circle(float& h, float& v, float sh, float sv)
{
    const float len_sq = sv * sv + sh * sh;
    if (len_sq <= 1.0f)
    {
        h = sh;
        v = sv;
        return;
    }
    const float len = sqrtf(len_sq);
    h = sh / len;
    v = sv / len;
}

// 0x006FA120 (:5433)
// The mouse axis that moved more than 2 pixels (x, y, then the wheel as 0x20000003), or 0xFFFFFFFF.
unsigned int controls_any_mouse_axis_changed()
{
    int x = 0;
    int y = 0;
    int wheel = 0;
    mouse_get_deltas(x, y, wheel);
    if (2 < abs(x))
    {
        return CONTROL_TYPE_MOUSE;
    }
    if (2 < abs(y))
    {
        return CONTROL_TYPE_MOUSE | 1;
    }
    return abs(wheel) < 3 ? CONTROL_INVALID : (CONTROL_TYPE_MOUSE | 3) - 1;
}

// 0x006FA1A0 (:5457)
unsigned int controls_any_gamepad_axis_changed()
{
    for (unsigned int joy_id = 0; joy_id < 32; joy_id++)
    {
        float x = 0.0f;
        float y = 0.0f;
        input_get_joy_raw(x, y, 0, joy_id);
        if (0.2f < fabsf(x))
        {
            return joy_id << 8;
        }
        if (0.2f < fabsf(y))
        {
            return (joy_id << 8) | 1;
        }
    }
    return CONTROL_INVALID;
}

// 0x006FAFD0 (:6097)
// A game pad with the weapon menu on a game pad button (not a key or the mouse).
bool controls_is_using_weapon_menu_button()
{
    if (!input_get_playing_with_controller())
    {
        return false;
    }
    const control_config& config = Control_config[Player_controller_index][controls_get_current_config_setting()];
    static const control_button_action actions[] = {CBA_GAC_WEAPON_MENU, CBA_WEAPON_MENU_DOWN, CBA_WEAPON_MENU_UP, CBA_WEAPON_MENU_LEFT, CBA_WEAPON_MENU_RIGHT};
    for (control_button_action a : actions)
    {
        if ((config.button_bindings[a].button[0] & CONTROL_TYPE_MASK) == 0 || (config.button_bindings[a].button[1] & CONTROL_TYPE_MASK) == 0)
        {
            return true;
        }
    }
    return false;
}

// 0x006FB0A0 (:6173)
bool control_is_controller_in_use()
{
    return Input_playing_with_controller;
}

// 0x006FB0B0 (:6179)
// Whether the camera is turned by the mouse: the action's primary horizontal or alternate vertical binding is a
// mouse axis.
bool control_mouse_camera_control(control_axis_action h_action, control_axis_action v_action)
{
    const control_config& config = Control_config[Player_controller_index][controls_get_current_config_setting()]; // inlined in the binary
    const unsigned int h = config.axis_bindings[h_action].axis[0];
    // Original: the vertical action's alternate binding, not its primary one.
    const unsigned int v = config.axis_bindings[v_action].axis[1];
    const bool h_mouse = (h & CONTROL_TYPE_MASK) == CONTROL_TYPE_MOUSE && ((h >> 27) & 1) == 0;
    const bool v_mouse = (v & CONTROL_TYPE_MASK) == CONTROL_TYPE_MOUSE && ((v >> 27) & 1) == 0;
    return h_mouse || v_mouse;
}

// 0x006FB1A0 (:6189)
bool control_steam_camera_control()
{
    return input_get_playing_with_controller() && input_get_controller_type() == keen::ControllerType_Steam;
}

// 0x006FB380 (:7244)
// A localized hint text as the escaped UTF-16 the vint text reads in a char string (UI.md UI4, the menus' hints):
// 0x80, then per character a tag and its two bytes (tag 1; +2 when the low byte is 0, +4 when the high byte is 0, a
// zero byte written as 0xFF), then 0x08 and the terminator. Original bug kept: a character starting one or two bytes
// before the end still writes its three bytes, up to two past the buffer.
void controls_fill_options_button_hint(char* out_buf, unsigned int buf_capacity, const wchar_t* loca_text)
{
    // Magic number: room for the start byte, the end byte and the terminator.
    if (out_buf == nullptr || buf_capacity < 4)
    {
        return;
    }
    if (loca_text == nullptr || *loca_text == L'\0')
    {
        *out_buf = '\0';
        return;
    }
    out_buf[0] = '\x80';
    char* const end = out_buf + (buf_capacity - 1);
    char* p = out_buf + 1;
    for (; *loca_text != L'\0' && p < end; p += 3, loca_text++)
    {
        const unsigned char low = (unsigned char)(*loca_text & 0xFF);
        const unsigned char high = (unsigned char)(*loca_text >> 8);
        p[0] = 1;
        if (low == 0)
        {
            p[0] = 3;
            p[1] = '\xFF';
        }
        else
        {
            p[1] = (char)low;
        }
        if (high == 0)
        {
            p[0] += 4;
            p[2] = '\xFF';
        }
        else
        {
            p[2] = (char)high;
        }
    }
    if (p >= end - 1)
    {
        end[-1] = 8;
        end[0] = '\0';
        return;
    }
    p[0] = 8;
    p[1] = '\0';
}

// ---- configs, bindings and action groups (stub_impl.md S5)
// The options screens (controller_options.cpp) edit a copy of a control config and install it.

// 0x0164ADDC, 0x0164ADE8: the actions every config must bind (verified by controls_verify_mandatory_controls, whose
// results the options screen shows from Unmapped_mandatory_*: true when the action is mapped).
static control_button_action Mandatory_button_actions[3] = {CBA_OFC_ATTACK_PRIMARY, CBA_GAMEPLAY_PAUSE, CBA_OFC_ACTION};
static control_axis_action Mandatory_axis_actions[4] = {CAA_CAMERA_ROTATE, CAA_CAMERA_ELEVATE, CAA_WALK_FORWARD_BACKWARD, CAA_WALK_TURN_LEFT_RIGHT};
static bool Unmapped_mandatory_button_actions[3]; // 0x01E2A9D4
static bool Unmapped_mandatory_axis_actions[4]; // 0x01E2A9D8

// 0x0164ADF8..0x0164AED8: the action groups (on foot, driving, the weapon/map menus, on foot without combat); a
// control may be bound once per group, so rebinding removes it from the other actions of the group.
static control_axis_action Control_axis_action_group_0[5] = {CAA_CAMERA_ROTATE, CAA_CAMERA_ELEVATE, CAA_WALK_FORWARD_BACKWARD, CAA_WALK_TURN_LEFT_RIGHT, CAA_CYCLE_WEAPONS}; // 0x0164ADF8
static control_axis_action Control_axis_action_group_1[3] = {CAA_DRIVE_CAMERA_ROTATE, CAA_DRIVE_CAMERA_ELEVATE, CAA_DRIVE_STEER}; // 0x0164AE0C
static control_axis_action Control_axis_action_group_3[4] = {CAA_TANK_CAMERA_ROTATE, CAA_TANK_CAMERA_ELEVATE, CAA_TANK_STEER, CAA_TANK_STEER_VERT}; // 0x0164AE18
static control_button_action Control_button_action_group_0[13] = {CBA_OFC_ATTACK_PRIMARY, CBA_OFC_ATTACK_MELEE, CBA_OFC_JUMP, CBA_OFC_SPRINT, CBA_OFC_CROUCH, CBA_OFC_RELOAD, CBA_OFC_ZOOM, CBA_OFC_ACTION, CBA_OFC_CHARGE_DETONATE, CBA_WEAPON_MENU_UP, CBA_WEAPON_MENU_DOWN, CBA_WEAPON_MENU_LEFT, CBA_WEAPON_MENU_RIGHT}; // 0x0164AE28
static control_button_action Control_button_action_group_3[10] = {CBA_OFC_ATTACK_PRIMARY, CBA_OFC_ATTACK_MELEE, CBA_OFC_JUMP, CBA_OFC_CROUCH, CBA_OFC_RELOAD, CBA_OFC_ZOOM, CBA_OFC_ACTION, CBA_OFC_CHARGE_DETONATE, CBA_GAC_TOGGLE_BACKPACK_ACTIVATION, CBA_GAC_SCOREBOARD}; // 0x0164AE5C
static control_button_action Control_button_action_group_1[14] = {CBA_VDC_ACCELERATE, CBA_VDC_BRAKE_REVERSE, CBA_VDC_HANDBRAKE, CBA_VDC_FIREARM, CBA_VDC_LOOK_BACK, CBA_VDC_TANK_FIRE_SECONDARY, CBA_VDC_MECH_MELEE1, CBA_VDC_MECH_MELEE2, CBA_VDC_MECH_MELEE3, CBA_VDC_MECH_MELEE4, CBA_VDC_MECH_CHARGE, CBA_VDC_ACTION, CBA_VDC_TURRET_ZOOM, CBA_VDC_CHANGE_SEAT}; // 0x0164AE84
static control_button_action Control_button_action_group_2[7] = {CBA_MAP, CBA_MENU_BACK, CBA_MINIMAP_ZOOM_IN, CBA_MINIMAP_ZOOM_OUT, CBA_GAC_WEAPON_MENU, CBA_GAMEPLAY_PAUSE, CBA_PUSHTOTALK}; // 0x0164AEBC
static control_axis_action Control_axis_action_group_2[1] = {CAA_ZOOM_IN_OUT}; // 0x0164AED8
static control_axis_action* Control_axis_action_groups[4] = {Control_axis_action_group_0, Control_axis_action_group_1, Control_axis_action_group_2, Control_axis_action_group_3}; // 0x0164AEDC
static control_button_action* Control_button_action_groups[4] = {Control_button_action_group_0, Control_button_action_group_1, Control_button_action_group_2, Control_button_action_group_3}; // 0x0164AEEC
static int Control_button_action_group_sizes[4] = {13, 14, 7, 10}; // 0x0164AEFC
static int Control_axis_action_group_sizes[4] = {5, 3, 1, 4}; // 0x0164AF0C

// The buttons that share a binding: the first one's is copied to the rest (controls_copy_grouped_controls).
// size 0x8
struct button_group
{
    control_button_action* group; // 0x00
    unsigned int group_size; // 0x04
};
static control_button_action Grouped_buttons_0[2] = {CBA_OFC_SPRINT, CBA_OFC_COVER}; // 0x0164AF1C
static control_button_action Grouped_buttons_1[2] = {CBA_OFC_ACTION, CBA_VDC_ACTION}; // 0x0164AF24
static control_button_action Grouped_buttons_2[2] = {CBA_VDC_FIREARM, CBA_VDC_TANK_FIRE_PRIMARY}; // 0x0164AF2C
static button_group Grouped_buttons[3] = {{Grouped_buttons_0, 2}, {Grouped_buttons_1, 2}, {Grouped_buttons_2, 2}}; // 0x0164AF34

// 0x006F8660 (:2810)
unsigned int control_config_get_button_for_action(control_config& config, control_button_action action, int buttonNum)
{
    if (buttonNum != 0 && buttonNum != 1)
    {
        return CONTROL_INVALID;
    }
    return config.button_bindings[action].button[buttonNum];
}

// 0x006F8690 (:2817)
unsigned int control_config_get_axis_for_action(control_config& config, control_axis_action action, int axisNum)
{
    if (axisNum != 0 && axisNum != 1)
    {
        return CONTROL_INVALID;
    }
    return config.axis_bindings[action].axis[axisNum];
}

// 0x006F86C0 (:2824)
void control_config_invert_axis_for_action(control_config& config, control_axis_action action, int axisNum)
{
    if (axisNum == 0 || axisNum == 1)
    {
        config.axis_bindings[action].inverted[axisNum] = !config.axis_bindings[action].inverted[axisNum];
    }
}

// 0x006F86F0 (:2839)
bool control_config_get_invert_axis_for_action(control_config& config, control_axis_action action, int axisNum)
{
    if (axisNum != 0 && axisNum != 1)
    {
        return false;
    }
    return config.axis_bindings[action].inverted[axisNum];
}

// 0x006FE6B0 (:3805)
// The config of the current input (keyboard and mouse, or the controller type) becomes new_config.
void controls_set_custom_config(const control_config& new_config)
{
    Control_config[Player_controller_index][controls_get_current_config_setting()] = new_config; // inlined in the binary
}

// 0x006F8C00 (:3810)
// Empty in rfg.exe.
void controls_set_custom_from_current()
{
}

// 0x006FE7B0 (:3870)
// The default bindings of the current input.
void control_config_populate_with_defaults(control_config& config)
{
    control_config_populate_with_defaults(config, controls_get_current_config_setting()); // both inlined in the binary
}

// 0x006FE870 (:3878)
void control_config_populate_with_defaults(control_config& config, control_config_setting config_setting)
{
    controls_set_button_bindings(config, Control_button_default_configs[config_setting], Control_button_default_config_sizes[config_setting]);
    controls_set_axis_bindings(config, Control_axis_default_configs[config_setting], Control_axis_default_config_sizes[config_setting]);
    controls_check_swap_confirm_cancel(Control_button_default_configs[config_setting], config_setting, config);
}

// 0x006F9360 (:4578)
// After a config change: the hint icons follow the input device, the camera its sensitivity.
void controls_set_control_config()
{
    ui_set_hint_icons_enabled(input_get_playing_with_controller());
    camera_first_person_update_camera_sensitivity();
}

// 0x006F9430 (:4668)
void controls_set_button_binding(control_config& config, control_button_action action, unsigned int button, unsigned int alt_button, unsigned int stick_dir)
{
    button_binding_data& data = config.button_bindings[action];
    data.button[0] = button;
    data.button[1] = alt_button;
    data.stick_dir = stick_dir;
}

// 0x006F9460 (:4677)
void controls_set_axis_binding(control_config& config, control_axis_action action, unsigned int axis, bool inverted, unsigned int alt_axis, bool alt_inverted)
{
    axis_binding_data& data = config.axis_bindings[action];
    data.axis[0] = axis;
    data.inverted[0] = inverted;
    data.axis[1] = alt_axis;
    data.inverted[1] = alt_inverted;
}

// 0x006F94A0 (:4693)
// Every entry, the zeroed ones included (unlike the per-setting overload).
void controls_set_button_bindings(control_config& config, const control_button_binding_info* button_bindings, int num_button_bindings)
{
    for (int i = 0; i < num_button_bindings; i++)
    {
        const control_button_binding_info& b = button_bindings[i];
        controls_set_button_binding(config, b.action, b.button, b.alt_button, b.stick_dir); // inlined in the binary
    }
}

// 0x006F94E0 (:4702)
void controls_set_axis_bindings(control_config& config, const control_axis_binding_info* axis_bindings, int num_axis_bindings)
{
    for (int i = 0; i < num_axis_bindings; i++)
    {
        const control_axis_binding_info& b = axis_bindings[i];
        controls_set_axis_binding(config, b.action, b.axis, b.inverted, b.alt_axis, b.alt_inverted); // inlined in the binary
    }
}

// 0x006F95A0 (:4750)
// An unbound axis is not inverted.
bool controls_get_axis_action_inverted_state(control_axis_action action, const control_config& config, bool primary)
{
    if ((int)action < NUM_CONTROL_AXIS_ACTIONS && config.axis_bindings[action].axis[!primary] != CONTROL_INVALID)
    {
        return config.axis_bindings[action].inverted[!primary];
    }
    return false;
}

// 0x006F9650 (:4804)
void controls_set_axis_action_inverted_state(control_axis_action action, bool inverted, control_config& config_setting, bool primary)
{
    if ((int)action < NUM_CONTROL_AXIS_ACTIONS && config_setting.axis_bindings[action].axis[!primary] != CONTROL_INVALID)
    {
        config_setting.axis_bindings[action].inverted[!primary] = inverted;
    }
}

// 0x006F9690 (:4842)
// In the current config of the controller.
void controls_set_axis_action_inverted_state(control_axis_action action, bool inverted, unsigned int controller_id, bool primary)
{
    if ((int)action < NUM_CONTROL_AXIS_ACTIONS && controller_id == 0)
    {
        axis_binding_data& data = Control_config[0][controls_get_current_config_setting()].axis_bindings[action];
        if (data.axis[!primary] != CONTROL_INVALID)
        {
            data.inverted[!primary] = inverted;
        }
    }
}

// 0x006F99B0 (:5015)
// The stick (clamped to the unit circle) stretched onto the unit square along its direction: the larger component
// becomes the length.
void controls_map_stick_to_square(float& h, float& v, float sh, float sv)
{
    const float len_sq = (float)((double)sv * (double)sv + (double)sh * (double)sh);
    if (1.0f < len_sq)
    {
        const float len = (float)sqrt((double)len_sq);
        sh = sh / len;
        sv = sv / len;
    }
    if (sv == 0.0f)
    {
        v = 0.0f;
        h = sh;
        return;
    }
    if (sh == 0.0f)
    {
        h = 0.0f;
        v = sv;
        return;
    }
    const float ratio = (float)fabs((double)sh / (double)sv);
    const float len = (float)sqrt((double)(float)((double)sv * (double)sv + (double)sh * (double)sh));
    if (1.0f < ratio)
    {
        h = (float)(0.0f <= sh ? 1 : -1) * len;
        v = (float)fabs((double)sv / (double)sh) * (float)(0.0f <= sv ? 1 : -1) * len;
        return;
    }
    v = (float)(0.0f <= sv ? 1 : -1) * len;
    h = (float)(0.0f <= sh ? 1 : -1) * len * ratio;
}

// 0x006FEFC0 (:5037)
// The first control just pressed: a key, a mouse button (released: the mouse reports clicks on release) or a pad
// button, in that order; CONTROL_INVALID without one.
unsigned int controls_is_any_button_pressed()
{
    for (unsigned int scancode = 0; scancode < 256; scancode++)
    {
        if (key_just_pressed(scancode))
        {
            return scancode | CONTROL_TYPE_KEYBOARD; // controls_any_key_just_pressed, inlined in the binary
        }
    }
    for (unsigned int button = 0; button < 5; button++)
    {
        if (mouse_just_released(button))
        {
            return button | CONTROL_TYPE_MOUSE;
        }
    }
    for (unsigned int button = 0; button < 128; button++)
    {
        if (input_just_released_no_clear(0, button))
        {
            return button;
        }
    }
    return CONTROL_INVALID;
}

// 0x006FF040 (:5085)
// As controls_is_any_button_pressed, for controls held down.
unsigned int controls_is_any_button_down()
{
    for (unsigned int scancode = 0; scancode < 256; scancode++)
    {
        if (key_down(scancode))
        {
            return scancode | CONTROL_TYPE_KEYBOARD;
        }
    }
    for (unsigned int button = 0; button < 5; button++)
    {
        if (mouse_is_pressed(button))
        {
            return button | CONTROL_TYPE_MOUSE;
        }
    }
    for (unsigned int button = 0; button < 128; button++)
    {
        if (input_is_pressed(0, button))
        {
            return button;
        }
    }
    return CONTROL_INVALID;
}

// 0x006FF0C0 (:5421)
unsigned int controls_is_any_axis_pressed()
{
    const unsigned int axis = controls_any_mouse_axis_changed();
    if (axis != CONTROL_INVALID)
    {
        return axis;
    }
    return controls_any_gamepad_axis_changed();
}

// 0x00702930 (:5623)
// Unbinds what the current input cannot produce: pad buttons past 127, pad axes past 127 (an axis as two buttons:
// either button). Keys and mouse controls are always valid.
bool controls_remove_invalid_mappings(control_config& config)
{
    bool changed = false;
    for (int i = 0; i < NUM_CONTROL_BUTTON_ACTIONS; i++)
    {
        for (int j = 0; j < 2; j++)
        {
            unsigned int& button = config.button_bindings[i].button[j];
            const unsigned int type = button & CONTROL_TYPE_MASK;
            if (button == CONTROL_INVALID || type == CONTROL_TYPE_MOUSE || type == CONTROL_TYPE_KEYBOARD)
            {
                continue;
            }
            if (type == 0)
            {
                if ((button & 0xFF) < 0x80)
                {
                    continue;
                }
            }
            else
            {
                VLIB_CRITICAL_FAILURE();
            }
            button = CONTROL_INVALID;
            changed = true;
        }
    }
    for (int i = 0; i < NUM_CONTROL_AXIS_ACTIONS; i++)
    {
        for (int j = 0; j < 2; j++)
        {
            unsigned int& axis = config.axis_bindings[i].axis[j];
            if ((axis & CONTROL_TYPE_MASK) != 0)
            {
                continue;
            }
            const bool invalid = (axis & CONTROL_AXIS_AS_BUTTONS) == 0 ? (axis & 0xFF) > 0x7F : ((axis & 0xFF00) > 0x7FFF || (axis & 0xFF) > 0x7F);
            if (invalid)
            {
                axis = CONTROL_INVALID;
                changed = true;
            }
        }
    }
    return changed;
}

// 0x00702A90 (:5656)
// The current input's config.
bool controls_remove_invalid_mappings()
{
    return controls_remove_invalid_mappings(Control_config[Player_controller_index][controls_get_current_config_setting()]); // inlined in the binary
}

// 0x006FA450 (:5716)
bool controls_verify_control_button_is_mapped(const control_config& config, control_button_action action)
{
    const button_binding_data& data = config.button_bindings[action];
    return data.button[0] != CONTROL_INVALID || data.button[1] != CONTROL_INVALID || data.stick_dir != CONTROL_INVALID;
}

// 0x006FA490 (:5729)
bool controls_verify_control_axis_is_mapped(const control_config& config, control_axis_action action)
{
    const axis_binding_data& data = config.axis_bindings[action];
    return data.axis[0] != CONTROL_INVALID || data.axis[1] != CONTROL_INVALID;
}

// 0x006FA4E0 (:5743)
// Every mandatory action is checked (and its result kept for the options screen).
bool controls_verify_mandatory_controls(const control_config& config)
{
    bool all = true;
    for (int i = 0; i < 4; i++)
    {
        Unmapped_mandatory_axis_actions[i] = controls_verify_control_axis_is_mapped(config, Mandatory_axis_actions[i]); // inlined in the binary
        all = all && Unmapped_mandatory_axis_actions[i];
    }
    for (int i = 0; i < 3; i++)
    {
        Unmapped_mandatory_button_actions[i] = controls_verify_control_button_is_mapped(config, Mandatory_button_actions[i]);
        all = all && Unmapped_mandatory_button_actions[i];
    }
    return all;
}

// 0x00702B30 (:6080)
// The current input's config loses its invalid mappings, then its mandatory controls are checked. warnUser is not read.
void controls_verify_control_config(bool warnUser)
{
    controls_remove_invalid_mappings();
    controls_verify_mandatory_controls(Control_config[Player_controller_index][controls_get_current_config_setting()]); // inlined in the binary
}

// 0x006FA8F0 (:5854)
// A control of the same type is part of an axis bound as two buttons.
bool controls_check_control_contains(unsigned int control, unsigned int other_control)
{
    if (((control ^ other_control) & CONTROL_TYPE_MASK) != 0 || (other_control & CONTROL_AXIS_AS_BUTTONS) == 0)
    {
        return false;
    }
    return (control & 0xFF) == ((other_control >> 8) & 0xFF) || (control & 0xFF) == (other_control & 0xFF);
}

// 0x006FA940 (:5877)
// In the current config: the axes of the actions that use the button (as half of a two-button axis) lose it.
void controls_remove_control_button_from_axis_bindings(unsigned int control, control_axis_action* caa_array, int count)
{
    if (caa_array == nullptr)
    {
        return;
    }
    control_config& config = Control_config[Player_controller_index][controls_get_current_config_setting()];
    for (unsigned int i = 0; i < (unsigned int)count; i++)
    {
        axis_binding_data& data = config.axis_bindings[caa_array[i]];
        if (controls_check_control_contains(control, data.axis[0]))
        {
            data.axis[0] = CONTROL_INVALID;
        }
        if (controls_check_control_contains(control, data.axis[1]))
        {
            data.axis[1] = CONTROL_INVALID;
        }
    }
}

// 0x006FA9D0 (:5895)
void controls_remove_control_button_from_button_bindings(unsigned int control, control_button_action* cba_array, int count)
{
    if (cba_array == nullptr)
    {
        return;
    }
    control_config& config = Control_config[Player_controller_index][controls_get_current_config_setting()];
    for (unsigned int i = 0; i < (unsigned int)count; i++)
    {
        button_binding_data& data = config.button_bindings[cba_array[i]];
        if (data.button[0] == control)
        {
            data.button[0] = CONTROL_INVALID;
        }
        if (data.button[1] == control)
        {
            data.button[1] = CONTROL_INVALID;
        }
    }
}

// 0x006FAA40 (:5912)
void controls_remove_control_axis_from_axis_bindings(unsigned int control, control_axis_action* caa_array, int count)
{
    if (caa_array == nullptr)
    {
        return;
    }
    control_config& config = Control_config[Player_controller_index][controls_get_current_config_setting()];
    for (unsigned int i = 0; i < (unsigned int)count; i++)
    {
        axis_binding_data& data = config.axis_bindings[caa_array[i]];
        if (data.axis[0] == control)
        {
            data.axis[0] = CONTROL_INVALID;
        }
        if (data.axis[1] == control)
        {
            data.axis[1] = CONTROL_INVALID;
        }
    }
}

// 0x006FAAB0 (:5929)
// An axis bound as two buttons frees those buttons; a stick axis frees the stick direction.
void controls_remove_control_axis_from_button_bindings(unsigned int control, control_button_action* cba_array, int count)
{
    if (cba_array == nullptr)
    {
        return;
    }
    control_config& config = Control_config[Player_controller_index][controls_get_current_config_setting()];
    for (unsigned int i = 0; i < (unsigned int)count; i++)
    {
        button_binding_data& data = config.button_bindings[cba_array[i]];
        if ((control & CONTROL_AXIS_AS_BUTTONS) == 0)
        {
            if (data.stick_dir == control)
            {
                data.stick_dir = CONTROL_INVALID;
            }
        }
        else
        {
            unsigned int positive = 0;
            unsigned int negative = 0;
            controls_split_axis_into_two_buttons(control, positive, negative);
            if (data.button[0] == positive || data.button[0] == negative)
            {
                data.button[0] = CONTROL_INVALID;
            }
            if (data.button[1] == positive || data.button[1] == negative)
            {
                data.button[1] = CONTROL_INVALID;
            }
        }
    }
}

// The group of an action in one of the four group tables (inlined in the four lookups of the binary).
template<typename A, typename R>
static R* controls_find_group(A action, A* const* groups, const int* sizes, R* const* results, const int* result_sizes, int& size)
{
    for (unsigned int g = 0; g < 4; g++)
    {
        for (int i = 0; i < sizes[g]; i++)
        {
            if (groups[g][i] == action)
            {
                size = result_sizes[g];
                return results[g];
            }
        }
    }
    return nullptr;
}

// 0x006FAE50 (:6013)
control_axis_action* controls_get_axis_action_group(control_axis_action action, int& size)
{
    return controls_find_group(action, Control_axis_action_groups, Control_axis_action_group_sizes, Control_axis_action_groups, Control_axis_action_group_sizes, size);
}

// 0x006FAEA0 (:6029)
control_button_action* controls_get_button_action_group(control_button_action action, int& size)
{
    return controls_find_group(action, Control_button_action_groups, Control_button_action_group_sizes, Control_button_action_groups, Control_button_action_group_sizes, size);
}

// 0x006FAEF0 (:6048)
// The axis actions of the group a button action is in.
control_axis_action* controls_get_axis_action_group_for_button(control_button_action action, int& size)
{
    return controls_find_group(action, Control_button_action_groups, Control_button_action_group_sizes, Control_axis_action_groups, Control_axis_action_group_sizes, size);
}

// 0x006FAF50 (:6061)
control_button_action* controls_get_button_action_group_for_axis(control_axis_action action, int& size)
{
    return controls_find_group(action, Control_axis_action_groups, Control_axis_action_group_sizes, Control_button_action_groups, Control_button_action_group_sizes, size);
}

// 0x006FB250 (:6196)
// Each group's first button binding is copied to the others.
bool controls_copy_grouped_controls(control_config& config)
{
    for (int g = 0; g < 3; g++)
    {
        const button_group& group = Grouped_buttons[g];
        const button_binding_data first = config.button_bindings[group.group[0]];
        for (unsigned int i = 1; i < group.group_size; i++)
        {
            config.button_bindings[group.group[i]] = first;
        }
    }
    return true;
}

// 0x006F88D0
bool controls_get_dpad_image_name(char* storage, int storage_len)
{
    if (storage == nullptr || storage_len < 0x20)
    {
        return false;
    }
    const char* name;
    switch (input_get_controller_type())
    {
        case keen::ControllerType_DualShock4:
            name = "ui_ps4_dpad";
            break;
        case keen::ControllerType_Xbox360:
        case keen::ControllerType_XBone:
            name = "ui_xbone_dpad";
            break;
        case keen::ControllerType_JoyConPair:
        case keen::ControllerType_SwitchProController:
            name = "ui_switch_dpad";
            break;
        default:
            name = "key_unbound";
            break;
    }
    _snprintf_safe(storage, storage_len, "%s", name);
    return true;
}

// ---- the button images

// 0x0164AF50
static const char* Control_pc_key_image_names[182] =
{
    "key_01", "key_02", "key_03", "key_04", "key_05", "key_06", "key_07", "key_08",
    "key_09", "key_10", "dinput_button_1", "dinput_button_9", "dinput_button_17", "dinput_axis_1", "dinput_axis_16", "key_16",
    "key_17", "key_18", "mouse_x1_click", "key_20", "key_21", "key_01", "key_01", "key_24",
    "key_25", "key_26", "key_27", "key_28", "key_29", "key_30", "key_31", "key_32",
    "key_33", "dinput_button_2", "dinput_button_10", "dinput_button_18", "dinput_axis_2", "dinput_axis_15", "key_39", "key_40",
    "key_41", "mouse_left_click", "key_43", "key_44", "key_45", "key_46", "key_47", "key_48",
    "key_49", "key_50", "key_51", "key_52", "key_53", "key_54", "key_55", "key_56",
    "dinput_button_3", "dinput_button_11", "dinput_button_19", "dinput_axis_3", "dinput_axis_14", "key_62", "key_63", "key_64",
    "mouse_right_click", "key_66", "key_67", "key_01", "key_01", "key_70", "key_71", "key_72",
    "key_73", "key_74", "key_75", "key_76", "key_77", "key_78", "key_79", "dinput_button_4",
    "dinput_button_12", "dinput_button_20", "dinput_axis_4", "dinput_axis_13", "key_85", "key_86", "key_87", "mouse_x2_click",
    "key_89", "key_90", "key_01", "key_01", "key_93", "key_94", "key_95", "key_96",
    "key_97", "key_98", "key_99", "key_100", "key_101", "key_102", "dinput_button_5", "dinput_button_13",
    "key_105", "dinput_axis_5", "dinput_axis_12", "key_108", "key_109", "key_110", "mouse_middle_click", "key_112",
    "key_113", "key_01", "key_115", "key_116", "key_117", "key_118", "key_119", "key_120",
    "key_121", "key_122", "key_123", "key_124", "key_125", "dinput_button_6", "dinput_button_14", "key_128",
    "dinput_axis_6", "dinput_axis_11", "dinput_axis_20", "key_132", "mouse_wheel_up_down", "key_134", "key_135", "key_136",
    "key_137", "key_138", "dinput_button_21", "key_140", "key_141", "key_142", "key_143", "key_144",
    "key_145", "key_146", "key_147", "key_148", "dinput_button_7", "dinput_button_15", "key_01", "dinput_axis_7",
    "dinput_axis_10", "key_154", "key_155", "mouse_move_up_down", "key_157", "key_158", "key_159", "key_160",
    "key_01", "key_162", "key_163", "key_164", "key_165", "key_166", "key_167", "key_168",
    "key_169", "key_170", "key_171", "dinput_button_8", "dinput_button_16", "key_01", "dinput_axis_8", "dinput_axis_9",
    "key_177", "key_178", "mouse_move_left_right", "key_180", "key_181", "key_182",
};

// 0x0164B228: the key captions of keen::input::getButtonCaption, by scan code.
static const char* Control_pc_key_image_captions[256] =
{
    "", "esc", "1", "2", "3", "4", "5", "6", "7", "8", "9", "0",
    "-", "=", "bckspc", "tab", "q", "w", "e", "r", "t", "y", "u", "i",
    "o", "p", "[", "]", "return", "l ctrl", "a", "s", "d", "f", "g", "h",
    "j", "k", "l", ";", "'", "`", "l shift", "\\", "z", "x", "c", "v",
    "b", "n", "m", ", ", ".", "/", "r shift", "num x", "l alt", "space", "lock", "f1",
    "f2", "f3", "f4", "f5", "f6", "f7", "f8", "f9", "f10", "pause", "scroll", "num 7",
    "num 8", "num 9", "num -", "num 4", "num 5", "num 6", "num +", "num 1", "num 2", "num 3", "num 0", "num .",
    "", "", "", "f11", "f12", "", "", "", "", "", "", "",
    "", "", "", "", "", "", "", "", "", "", "", "",
    "", "", "", "", "", "", "", "", "", "", "", "",
    "", "", "", "", "", "", "", "", "", "", "", "",
    "", "", "", "", "", "", "", "", "", "", "", "",
    "", "", "", "", "", "", "", "", "", "", "", "",
    "num return", "r ctrl", "", "", "", "", "", "", "", "", "", "",
    "", "", "", "", "", "", "", "", "", "", "", "",
    "", "num /", "", "print", "r alt", "", "", "", "", "", "", "",
    "", "", "", "", "", "nm lck", "", "home", "up", "pgeup", "", "left",
    "", "right", "", "end", "down", "pgedwn", "insert", "delete", "", "", "", "",
    "", "", "", "", "", "", "", "", "", "", "", "",
    "", "", "", "", "", "", "", "", "", "", "", "",
    "", "", "", "", "", "", "", "", "", "", "", "",
    "", "", "", "",
};

// 0x012D1E70: the index in Control_pc_key_image_names of each caption, -1: none.
static const int Control_pc_key_image_indicies[256] =
{
    -1, 123, 19, 42, 65, 88, 111, 134, 157, 180, 20, 43, 119, 120, 16, 40, 4, 5, 23, 115,
    24, 25, 26, 27, 28, 46, 116, 117, 177, 131, 47, 48, 49, 50, 51, 69, 70, 71, 72, 140,
    139, 124, 154, 118, 73, 74, 92, 93, 94, 95, 96, 143, 142, 141, 108, 168, 62, 17, 63, 138,
    161, 162, 163, 164, 165, 166, 167, 144, 121, 147, 169, 53, 76, 99, 145, 6, 7, 30, 122, 75,
    52, 29, 31, 77, -1, -1, -1, 98, 97, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 8, 85, -1, -1,
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
    -1, 54, -1, 146, 39, -1, -1, -1, -1, 50, -1, -1, -1, -1, -1, -1, -1, -1, -1, 78,
    2, 100, -1, 3, -1, 0, -1, 32, 1, 101, 55, 9, -1, -1, -1, -1, -1, -1, -1, -1,
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
};

// 0x012D1BCC
static const int Control_pc_mouse_image_indicies[5] = {41, 110, 64, 18, 87};

// 0x012D2270: the 21 buttons of a pad seen as the PC's gamepad, the rest 0.
static const int Control_pc_gamepad_button_image_indicies[128] =
{
    10, 33, 56, 79, 102, 125, 148, 171, 11, 34, 57, 80, 103, 126, 149, 172, 12, 35, 58, 81, 138,
};

// size 0x8
struct control_action_set_mapping
{
    unsigned int default_set; // 0x00
    unsigned int supported; // 0x04, a bit per Steam action set
};

// 0x012D49D0: the Steam action sets of each control_button_action.
static const control_action_set_mapping Steam_button_action_set_mapping[108] =
{
    {0, 1}, {0, 1}, {0, 17}, {0, 1}, {0, 1}, {0, 1}, {0, 1}, {0, 1}, {0, 1}, {4, 16},
    {4, 16}, {4, 16}, {4, 16}, {4, 16}, {4, 16}, {4, 16}, {4, 16}, {4, 16}, {4, 16}, {4, 16},
    {0, 3}, {0, 1}, {0, 1}, {0, 1}, {0, 1}, {0, 1}, {0, 1}, {0, 1}, {0, 1}, {0, 1},
    {1, 2}, {1, 2}, {1, 2}, {1, 2}, {1, 14}, {1, 2}, {3, 8}, {3, 8}, {3, 8}, {3, 8},
    {3, 8}, {2, 12}, {2, 15}, {1, 14}, {1, 15}, {1, 2}, {0, 1}, {0, 1}, {0, 1}, {0, 1},
    {0, 17}, {0, 17}, {0, 17}, {0, 17}, {0, 17}, {0, 17}, {0, 17}, {0, 17}, {0, 31}, {4, 16},
    {4, 16}, {4, 16}, {4, 16}, {4, 16}, {4, 16}, {4, 16}, {4, 16}, {4, 16}, {4, 16}, {4, 16},
    {4, 16}, {4, 16}, {4, 16}, {4, 16}, {4, 16}, {4, 16}, {4, 16}, {4, 16}, {4, 16}, {4, 16},
    {4, 16}, {4, 16}, {4, 16}, {4, 16}, {4, 16}, {4, 16}, {0, 31}, {0, 15}, {0, 15}, {4, 16},
    {4, 16}, {4, 16}, {4, 16}, {0, 0}, {0, 0}, {0, 0}, {0, 17}, {0, 0}, {0, 15}, {0, 1},
    {0, 1}, {4, 16}, {4, 16}, {1, 2}, {1, 2}, {0, 17}, {0, 17}, {0, 1},
};

// A key or Steam origin with no image of its own is drawn as a blank key with its caption on it: the text "image,
// width, height, caption in hex" that the vint [image:] tag reads. Magic numbers: the caption's text size on the key.
static unsigned int controls_format_key_caption(char* out, unsigned int capacity, const char* caption)
{
    char shortened[64];
    rfg_shorten_display_string(caption, shortened, 0x40, 8, false);
    // key_110 is the wide blank key, for captions of more than one character.
    const bool wide = keen::getStringLengthUTF8(shortened) > 1;
    unsigned int len = keen::formatString(out, capacity, "%s,%d,%d,", wide ? "key_110" : "key_unbound", wide ? 10 : 6, 5) - 1;
    for (const char* c = shortened; *c != '\0'; c++)
    {
        len += keen::formatString(out + len, capacity - len, "%2x", (unsigned int)(unsigned char)*c) - 1;
    }
    return len;
}

// 0x006F8750 (:2897)
const char* controls_get_key_image_name(unsigned int key)
{
    static char return_caption[0x80]; // 0x01E2B288
    char caption[64];
    if (!keen::input::getButtonCaption(caption, 0x40, keen::ControllerType_Keyboard, key))
    {
        return "key_unbound";
    }
    for (unsigned int i = 0; i < 256; i++)
    {
        if (keen::isStringEqualNoCase(caption, Control_pc_key_image_captions[i]))
        {
            if (Control_pc_key_image_indicies[i] >= 0)
            {
                return Control_pc_key_image_names[Control_pc_key_image_indicies[i]];
            }
            break;
        }
    }
    controls_format_key_caption(return_caption, 0x80, caption);
    return return_caption;
}

// 0x006FF320 (:6237)
const char* controls_bake_steam_origins_string(const keen::SteamControllerActionOrigin* origins, unsigned int count)
{
    static char output_string[0x40]; // 0x01E2B308
    unsigned int len = 0;
    output_string[0] = '\0';
    // Original quirk kept: only the first origin is baked, the separator below never runs.
    const unsigned int num_origins = count != 0 ? 1 : 0;
    for (unsigned int i = 0; i < num_origins; i++)
    {
        const char* name = nullptr;
        switch (origins[i])
        {
            case keen::SteamControllerActionOrigin_A: case keen::SteamControllerActionOrigin_XBoxOne_A: case keen::SteamControllerActionOrigin_XBox360_A: case keen::SteamControllerActionOrigin_SteamV2_A: name = "ui_xbone_a"; break;
            case keen::SteamControllerActionOrigin_B: case keen::SteamControllerActionOrigin_XBoxOne_B: case keen::SteamControllerActionOrigin_XBox360_B: case keen::SteamControllerActionOrigin_SteamV2_B: name = "ui_xbone_b"; break;
            case keen::SteamControllerActionOrigin_X: case keen::SteamControllerActionOrigin_XBoxOne_X: case keen::SteamControllerActionOrigin_XBox360_X: case keen::SteamControllerActionOrigin_SteamV2_X: name = "ui_xbone_x"; break;
            case keen::SteamControllerActionOrigin_Y: case keen::SteamControllerActionOrigin_XBoxOne_Y: case keen::SteamControllerActionOrigin_XBox360_Y: case keen::SteamControllerActionOrigin_SteamV2_Y: name = "ui_xbone_y"; break;
            case keen::SteamControllerActionOrigin_LeftBumper: case keen::SteamControllerActionOrigin_SteamV2_LeftBumper: name = "ui_steam_shoulder_l"; break;
            case keen::SteamControllerActionOrigin_RightBumper: case keen::SteamControllerActionOrigin_SteamV2_RightBumper: name = "ui_steam_shoulder_r"; break;
            case keen::SteamControllerActionOrigin_LeftGrip: case keen::SteamControllerActionOrigin_SteamV2_LeftGrip: name = "ui_steam_grip_l"; break;
            case keen::SteamControllerActionOrigin_RightGrip: case keen::SteamControllerActionOrigin_SteamV2_RightGrip: name = "ui_steam_grip_r"; break;
            case keen::SteamControllerActionOrigin_Start: case keen::SteamControllerActionOrigin_SteamV2_Start: name = "ui_steam_button_start"; break;
            case keen::SteamControllerActionOrigin_Back: case keen::SteamControllerActionOrigin_SteamV2_Back: name = "ui_steam_button_select"; break;
            case keen::SteamControllerActionOrigin_LeftPad_Touch: case keen::SteamControllerActionOrigin_SteamV2_LeftPad_Touch: name = "ui_steam_pad_l_touch"; break;
            case keen::SteamControllerActionOrigin_LeftPad_Swipe: case keen::SteamControllerActionOrigin_SteamV2_LeftPad_Swipe: name = "ui_steam_pad_l_swipe"; break;
            case keen::SteamControllerActionOrigin_LeftPad_Click: case keen::SteamControllerActionOrigin_SteamV2_LeftPad_Click: name = "ui_steam_pad_l_click"; break;
            case keen::SteamControllerActionOrigin_LeftPad_DPadNorth: case keen::SteamControllerActionOrigin_SteamV2_LeftPad_DPadNorth: name = "ui_steam_pad_l_dpad_n"; break;
            case keen::SteamControllerActionOrigin_LeftPad_DPadSouth: case keen::SteamControllerActionOrigin_SteamV2_LeftPad_DPadSouth: name = "ui_steam_pad_l_dpad_s"; break;
            case keen::SteamControllerActionOrigin_LeftPad_DPadWest: case keen::SteamControllerActionOrigin_SteamV2_LeftPad_DPadWest: name = "ui_steam_pad_l_dpad_w"; break;
            case keen::SteamControllerActionOrigin_LeftPad_DPadEast: case keen::SteamControllerActionOrigin_SteamV2_LeftPad_DPadEast: name = "ui_steam_pad_l_dpad_e"; break;
            case keen::SteamControllerActionOrigin_RightPad_Touch: case keen::SteamControllerActionOrigin_SteamV2_RightPad_Touch: name = "ui_steam_pad_r_touch"; break;
            case keen::SteamControllerActionOrigin_RightPad_Swipe: case keen::SteamControllerActionOrigin_SteamV2_RightPad_Swipe: name = "ui_steam_pad_r_swipe"; break;
            case keen::SteamControllerActionOrigin_RightPad_Click: case keen::SteamControllerActionOrigin_SteamV2_RightPad_Click: name = "ui_steam_pad_r_click"; break;
            case keen::SteamControllerActionOrigin_RightPad_DPadNorth: case keen::SteamControllerActionOrigin_SteamV2_RightPad_DPadNorth: name = "ui_steam_pad_r_dpad_n"; break;
            case keen::SteamControllerActionOrigin_RightPad_DPadSouth: case keen::SteamControllerActionOrigin_SteamV2_RightPad_DPadSouth: name = "ui_steam_pad_r_dpad_s"; break;
            case keen::SteamControllerActionOrigin_RightPad_DPadWest: case keen::SteamControllerActionOrigin_SteamV2_RightPad_DPadWest: name = "ui_steam_pad_r_dpad_w"; break;
            case keen::SteamControllerActionOrigin_RightPad_DPadEast: case keen::SteamControllerActionOrigin_SteamV2_RightPad_DPadEast: name = "ui_steam_pad_r_dpad_e"; break;
            case keen::SteamControllerActionOrigin_LeftTrigger_Pull: case keen::SteamControllerActionOrigin_SteamV2_LeftTrigger_Pull: name = "ui_steam_trigger_l_pull"; break;
            case keen::SteamControllerActionOrigin_LeftTrigger_Click: case keen::SteamControllerActionOrigin_SteamV2_LeftTrigger_Click: name = "ui_steam_trigger_l_click"; break;
            case keen::SteamControllerActionOrigin_RightTrigger_Pull: case keen::SteamControllerActionOrigin_SteamV2_RightTrigger_Pull: name = "ui_steam_trigger_r_pull"; break;
            case keen::SteamControllerActionOrigin_RightTrigger_Click: case keen::SteamControllerActionOrigin_SteamV2_RightTrigger_Click: name = "ui_steam_trigger_r_click"; break;
            case keen::SteamControllerActionOrigin_LeftStick_Move: case keen::SteamControllerActionOrigin_SteamV2_LeftStick_Move: name = "ui_steam_stick_l_move"; break;
            case keen::SteamControllerActionOrigin_LeftStick_Click: case keen::SteamControllerActionOrigin_SteamV2_LeftStick_Click: name = "ui_steam_stick_l_click"; break;
            case keen::SteamControllerActionOrigin_LeftStick_DPadNorth: case keen::SteamControllerActionOrigin_SteamV2_LeftStick_DPadNorth: name = "ui_steam_stick_dpad_n"; break;
            case keen::SteamControllerActionOrigin_LeftStick_DPadSouth: case keen::SteamControllerActionOrigin_SteamV2_LeftStick_DPadSouth: name = "ui_steam_stick_dpad_s"; break;
            case keen::SteamControllerActionOrigin_LeftStick_DPadWest: case keen::SteamControllerActionOrigin_SteamV2_LeftStick_DPadWest: name = "ui_steam_stick_dpad_w"; break;
            case keen::SteamControllerActionOrigin_LeftStick_DPadEast: case keen::SteamControllerActionOrigin_SteamV2_LeftStick_DPadEast: name = "ui_steam_stick_dpad_e"; break;
            case keen::SteamControllerActionOrigin_Gyro_Move: case keen::SteamControllerActionOrigin_PS4_Gyro_Move: case keen::SteamControllerActionOrigin_SteamV2_Gyro_Move: name = "ui_steam_gyro"; break;
            case keen::SteamControllerActionOrigin_PS4_X: name = "ui_ps4_cross"; break;
            case keen::SteamControllerActionOrigin_PS4_Circle: name = "ui_ps4_circle"; break;
            case keen::SteamControllerActionOrigin_PS4_Triangle: name = "ui_ps4_triangle"; break;
            case keen::SteamControllerActionOrigin_PS4_Square: name = "ui_ps4_square"; break;
            case keen::SteamControllerActionOrigin_PS4_LeftBumper: name = "ui_ps4_l1"; break;
            case keen::SteamControllerActionOrigin_PS4_RightBumper: name = "ui_ps4_r1"; break;
            case keen::SteamControllerActionOrigin_PS4_Options: name = "ui_ps4_options"; break;
            case keen::SteamControllerActionOrigin_PS4_LeftPad_Touch: name = "ui_steam_ps4_pad_l_touch"; break;
            case keen::SteamControllerActionOrigin_PS4_LeftPad_Swipe: name = "ui_steam_ps4_pad_l_swipe"; break;
            case keen::SteamControllerActionOrigin_PS4_LeftPad_Click: name = "ui_steam_ps4_pad_l_click"; break;
            case keen::SteamControllerActionOrigin_PS4_LeftPad_DPadNorth: name = "ui_steam_ps4_pad_l_dpad_n"; break;
            case keen::SteamControllerActionOrigin_PS4_LeftPad_DPadSouth: name = "ui_steam_ps4_pad_l_dpad_s"; break;
            case keen::SteamControllerActionOrigin_PS4_LeftPad_DPadWest: name = "ui_steam_ps4_pad_l_dpad_w"; break;
            case keen::SteamControllerActionOrigin_PS4_LeftPad_DPadEast: name = "ui_steam_ps4_pad_l_dpad_e"; break;
            case keen::SteamControllerActionOrigin_PS4_RightPad_Touch: name = "ui_steam_ps4_pad_r_touch"; break;
            case keen::SteamControllerActionOrigin_PS4_RightPad_Swipe: name = "ui_steam_ps4_pad_r_swipe"; break;
            case keen::SteamControllerActionOrigin_PS4_RightPad_Click: name = "ui_steam_ps4_pad_r_click"; break;
            case keen::SteamControllerActionOrigin_PS4_RightPad_DPadNorth: name = "ui_steam_ps4_pad_r_dpad_n"; break;
            case keen::SteamControllerActionOrigin_PS4_RightPad_DPadSouth: name = "ui_steam_ps4_pad_r_dpad_s"; break;
            case keen::SteamControllerActionOrigin_PS4_RightPad_DPadWest: name = "ui_steam_ps4_pad_r_dpad_w"; break;
            case keen::SteamControllerActionOrigin_PS4_RightPad_DPadEast: name = "ui_steam_ps4_pad_r_dpad_e"; break;
            case keen::SteamControllerActionOrigin_PS4_CenterPad_Touch: name = "ui_steam_ps4_pad_center_touch"; break;
            case keen::SteamControllerActionOrigin_PS4_CenterPad_Swipe: name = "ui_steam_ps4_pad_center_swipe"; break;
            case keen::SteamControllerActionOrigin_PS4_CenterPad_Click: name = "ui_steam_ps4_pad_center_click"; break;
            case keen::SteamControllerActionOrigin_PS4_CenterPad_DPadNorth: name = "ui_steam_ps4_pad_center_dpad_n"; break;
            case keen::SteamControllerActionOrigin_PS4_CenterPad_DPadSouth: name = "ui_steam_ps4_pad_center_dpad_s"; break;
            case keen::SteamControllerActionOrigin_PS4_CenterPad_DPadWest: name = "ui_steam_ps4_pad_center_dpad_w"; break;
            case keen::SteamControllerActionOrigin_PS4_CenterPad_DPadEast: name = "ui_steam_ps4_pad_center_dpad_e"; break;
            case keen::SteamControllerActionOrigin_PS4_LeftTrigger_Pull: case keen::SteamControllerActionOrigin_PS4_LeftTrigger_Click: name = "ui_ps4_l2"; break;
            case keen::SteamControllerActionOrigin_PS4_RightTrigger_Pull: case keen::SteamControllerActionOrigin_PS4_RightTrigger_Click: name = "ui_ps4_r2"; break;
            case keen::SteamControllerActionOrigin_PS4_LeftStick_Move: name = "ui_ps4_ls"; break;
            case keen::SteamControllerActionOrigin_PS4_LeftStick_Click: name = "ui_ps4_l3"; break;
            case keen::SteamControllerActionOrigin_PS4_RightStick_Move: name = "ui_ps4_rs"; break;
            case keen::SteamControllerActionOrigin_PS4_RightStick_Click: name = "ui_ps4_r3"; break;
            case keen::SteamControllerActionOrigin_PS4_DPad_North: name = "ui_ps4_dpad_up_small"; break;
            case keen::SteamControllerActionOrigin_PS4_DPad_South: name = "ui_ps4_dpad_down_small"; break;
            case keen::SteamControllerActionOrigin_PS4_DPad_West: name = "ui_ps4_dpad_left_small"; break;
            case keen::SteamControllerActionOrigin_PS4_DPad_East: name = "ui_ps4_dpad_right_small"; break;
            case keen::SteamControllerActionOrigin_XBoxOne_LeftBumper: case keen::SteamControllerActionOrigin_XBox360_LeftBumper: name = "ui_xbone_lb"; break;
            case keen::SteamControllerActionOrigin_XBoxOne_RightBumper: case keen::SteamControllerActionOrigin_XBox360_RightBumper: name = "ui_xbone_rb"; break;
            case keen::SteamControllerActionOrigin_XBoxOne_Menu: case keen::SteamControllerActionOrigin_XBox360_Start: name = "ui_xbone_menu"; break;
            case keen::SteamControllerActionOrigin_XBoxOne_View: case keen::SteamControllerActionOrigin_XBox360_Back: name = "ui_xbone_view"; break;
            case keen::SteamControllerActionOrigin_XBoxOne_LeftTrigger_Pull: case keen::SteamControllerActionOrigin_XBoxOne_LeftTrigger_Click: case keen::SteamControllerActionOrigin_XBox360_LeftTrigger_Pull: case keen::SteamControllerActionOrigin_XBox360_LeftTrigger_Click: name = "ui_xbone_lt"; break;
            case keen::SteamControllerActionOrigin_XBoxOne_RightTrigger_Pull: case keen::SteamControllerActionOrigin_XBoxOne_RightTrigger_Click: case keen::SteamControllerActionOrigin_XBox360_RightTrigger_Pull: case keen::SteamControllerActionOrigin_XBox360_RightTrigger_Click: name = "ui_xbone_rt"; break;
            case keen::SteamControllerActionOrigin_XBoxOne_LeftStick_Move: case keen::SteamControllerActionOrigin_XBoxOne_LeftStick_Click: case keen::SteamControllerActionOrigin_XBox360_LeftStick_Move: case keen::SteamControllerActionOrigin_XBox360_LeftStick_Click: name = "ui_xbone_ls"; break;
            case keen::SteamControllerActionOrigin_XBoxOne_RightStick_Move: case keen::SteamControllerActionOrigin_XBoxOne_RightStick_Click: case keen::SteamControllerActionOrigin_XBox360_RightStick_Move: case keen::SteamControllerActionOrigin_XBox360_RightStick_Click: name = "ui_xbone_rs"; break;
            case keen::SteamControllerActionOrigin_XBoxOne_DPad_North: case keen::SteamControllerActionOrigin_XBox360_DPad_North: name = "ui_xbone_dpad_up_small"; break;
            case keen::SteamControllerActionOrigin_XBoxOne_DPad_South: case keen::SteamControllerActionOrigin_XBox360_DPad_South: name = "ui_xbone_dpad_down_small"; break;
            case keen::SteamControllerActionOrigin_XBoxOne_DPad_West: case keen::SteamControllerActionOrigin_XBox360_DPad_West: name = "ui_xbone_dpad_left_small"; break;
            case keen::SteamControllerActionOrigin_XBoxOne_DPad_East: case keen::SteamControllerActionOrigin_XBox360_DPad_East: name = "ui_xbone_dpad_right_small"; break;
            case keen::SteamControllerActionOrigin_SwitchProCon_A: name = "ui_switch_a"; break;
            case keen::SteamControllerActionOrigin_SwitchProCon_B: name = "ui_switch_b"; break;
            case keen::SteamControllerActionOrigin_SwitchProCon_X: name = "ui_switch_x"; break;
            case keen::SteamControllerActionOrigin_SwitchProCon_Y: name = "ui_switch_y"; break;
            case keen::SteamControllerActionOrigin_SwitchProCon_L: name = "ui_switch_l"; break;
            case keen::SteamControllerActionOrigin_SwitchProCon_R: name = "ui_switch_r"; break;
            case keen::SteamControllerActionOrigin_SwitchProCon_Plus: name = "ui_switch_plus"; break;
            case keen::SteamControllerActionOrigin_SwitchProCon_Minus: name = "ui_switch_minus"; break;
            case keen::SteamControllerActionOrigin_SwitchProCon_ZL_Pull: case keen::SteamControllerActionOrigin_SwitchProCon_ZL_Click: name = "ui_switch_zl"; break;
            case keen::SteamControllerActionOrigin_SwitchProCon_ZR_Pull: case keen::SteamControllerActionOrigin_SwitchProCon_ZR_Click: name = "ui_switch_zr"; break;
            case keen::SteamControllerActionOrigin_SwitchProCon_LeftStick_Move: name = "ui_switch_ls"; break;
            case keen::SteamControllerActionOrigin_SwitchProCon_LeftStick_Click: name = "ui_switch_ls_click"; break;
            case keen::SteamControllerActionOrigin_SwitchProCon_RightStick_Move: name = "ui_switch_rs"; break;
            case keen::SteamControllerActionOrigin_SwitchProCon_RichtStick_Click: name = "ui_switch_rs_click"; break;
            case keen::SteamControllerActionOrigin_SwitchProCon_DPad_North: name = "ui_steam_switch_button_dpad_n"; break;
            case keen::SteamControllerActionOrigin_SwitchProCon_DPad_South: name = "ui_steam_switch_button_dpad_s"; break;
            case keen::SteamControllerActionOrigin_SwitchProCon_DPad_West: name = "ui_steam_switch_button_dpad_w"; break;
            case keen::SteamControllerActionOrigin_SwitchProCon_DPad_East: name = "ui_steam_switch_button_dpad_e"; break;
            default: break;
        }
        if (i != 0)
        {
            len += keen::formatString(output_string + len, 0x40 - len, "][image:") - 1;
        }
        if (name != nullptr)
        {
            len += keen::formatString(output_string + len, 0x40 - len, "%s", name) - 1;
        }
        else
        {
            const char* text = keen::input::getSteamControllerActionOriginText(origins[i]);
            if (text != nullptr)
            {
                len += controls_format_key_caption(output_string + len, 0x40 - len, text);
            }
        }
    }
    return output_string;
}

// 0x006FFB00 (:6788)
const char* controls_get_button_action_image_names_steam(unsigned int button, unsigned int action_set)
{
    keen::SteamControllerAction action;
    action.axisMode = keen::SteamControllerAxisMode_XAndY;
    action.buttonIndex = button;
    action.type = keen::SteamControllerActionType_Button;
    action.pName = nullptr;
    keen::SteamControllerActionOrigin origins[8];
    unsigned int count = 0;
    keen::input::getSteamControllerActionOrigins(Keen_input_system, action, action_set, origins, 8, &count);
    if (count == 0)
    {
        origins[0] = keen::SteamControllerActionOrigin_None;
        count = 1;
    }
    return controls_bake_steam_origins_string(origins, count);
}

// 0x006FFB90 (:6813)
const char* controls_get_button_image_name(unsigned int button, unsigned int action_set)
{
    if (input_get_playing_with_controller() && input_get_controller_type() == keen::ControllerType_Steam)
    {
        return controls_get_button_action_image_names_steam(button, action_set);
    }
    const keen::ControllerType type = input_get_controller_type();
    switch (type)
    {
        case keen::ControllerType_DualShock4:
        {
            static const char* const names[16] = {"ui_ps4_cross", "ui_ps4_circle", "ui_ps4_square", "ui_ps4_triangle", "ui_ps4_l1", "ui_ps4_r1", "ui_ps4_l2", "ui_ps4_r2", "ui_ps4_dpad_right_small", "ui_ps4_dpad_up_small", "ui_ps4_dpad_left_small", "ui_ps4_dpad_down_small", "ui_ps4_options", "ui_ps4_touchpad", "ui_ps4_l3", "ui_ps4_r3"};
            return button < 16 ? names[button] : nullptr;
        }
        case keen::ControllerType_Xbox360:
        case keen::ControllerType_XBone:
        {
            static const char* const names[16] = {"ui_xbone_a", "ui_xbone_b", "ui_xbone_x", "ui_xbone_y", "ui_xbone_lb", "ui_xbone_rb", "ui_xbone_lt", "ui_xbone_rt", "ui_xbone_dpad_right_small", "ui_xbone_dpad_up_small", "ui_xbone_dpad_left_small", "ui_xbone_dpad_down_small", "ui_xbone_menu", "ui_xbone_view", "ui_xbone_ls", "ui_xbone_rs"};
            return button < 16 ? names[button] : nullptr;
        }
        case keen::ControllerType_JoyConPair:
        case keen::ControllerType_SwitchProController:
        {
            static const char* const names[16] = {"ui_switch_b", "ui_switch_a", "ui_switch_y", "ui_switch_x", "ui_switch_l", "ui_switch_r", "ui_switch_zl", "ui_switch_zr", "ui_switch_dpad_right_small", "ui_switch_dpad_up_small", "ui_switch_dpad_left_small", "ui_switch_dpad_down_small", "ui_switch_plus", "ui_switch_minus", "ui_switch_ls_click", "ui_switch_rs_click"};
            static const char* const pro_dpad[4] = {"ui_steam_switch_button_dpad_e", "ui_steam_switch_button_dpad_n", "ui_steam_switch_button_dpad_w", "ui_steam_switch_button_dpad_s"};
            if (type == keen::ControllerType_SwitchProController && button >= 8 && button <= 11)
            {
                return pro_dpad[button - 8];
            }
            return button < 16 ? names[button] : nullptr;
        }
        default:
        {
            // The 20 buttons of a DirectInput pad, then its 4 POV directions (key_04, key_01, key_03, key_02).
            static const char* const names[20] = {"dinput_button_1", "dinput_button_2", "dinput_button_3", "dinput_button_4", "dinput_button_5", "dinput_button_6", "dinput_button_7", "dinput_button_8", "dinput_button_9", "dinput_button_10", "dinput_button_11", "dinput_button_12", "dinput_button_13", "dinput_button_14", "dinput_button_15", "dinput_button_16", "dinput_button_17", "dinput_button_18", "dinput_button_19", "dinput_button_20"};
            static const char* const pov[4] = {"key_04", "key_01", "key_03", "key_02"};
            if (button < 20)
            {
                return names[button];
            }
            if (button >= 0x20 && button <= 0x23)
            {
                return pov[button - 0x20];
            }
            return "key_unbound";
        }
    }
}

// 0x00702170 (:3087)
bool controls_get_button_image_name(char* storage, int storage_len, unsigned int button, unsigned int action_set)
{
    if (storage == nullptr || storage_len < 0x20)
    {
        return false;
    }
    const char* name;
    if (button == CONTROL_INVALID)
    {
        name = "key_unbound";
    }
    else if (input_get_playing_with_controller())
    {
        name = controls_get_button_image_name(button, action_set);
    }
    else
    {
        name = Control_pc_key_image_names[Control_pc_gamepad_button_image_indicies[button & 0xFF]];
    }
    if (name == nullptr)
    {
        return false;
    }
    _snprintf_safe(storage, storage_len, "%s", name);
    return true;
}

// 0x007043B0 (:2967)
bool controls_get_button_image_name_pc(char* storage, int storage_len, unsigned int button, unsigned int action_set)
{
    if (storage == nullptr || storage_len < 0x20)
    {
        return false;
    }
    if (button == CONTROL_INVALID)
    {
        _snprintf_safe(storage, storage_len, "%s", "key_unbound");
        return false;
    }
    const char* name;
    if ((button & CONTROL_TYPE_MASK) == CONTROL_TYPE_KEYBOARD)
    {
        name = controls_get_key_image_name(button & 0xFF);
    }
    else if ((button & CONTROL_TYPE_MASK) == CONTROL_TYPE_MOUSE)
    {
        // Original bug kept: no check against the 5 mouse buttons of the table.
        name = Control_pc_key_image_names[Control_pc_mouse_image_indicies[button & 0xFF]];
    }
    else
    {
        return controls_get_button_image_name(storage, storage_len, button, action_set);
    }
    if (name == nullptr)
    {
        return false;
    }
    _snprintf_safe(storage, storage_len, "%s", name);
    return true;
}

// The binding's action set: the current one if the action supports it. Inlined at each use in rfg.exe.
static unsigned int controls_image_action_set(control_button_action action)
{
    const unsigned int action_set = keen::input::getSteamControllerActionSet(Keen_input_system);
    if ((Steam_button_action_set_mapping[action].supported & (1u << (action_set & 0x1F))) == 0)
    {
        return Steam_button_action_set_mapping[action].default_set;
    }
    return action_set;
}

// 0x00704E90 (:2870)
bool controls_get_button_image_name(const control_config& config, char* storage, int storage_len, control_button_action action)
{
    const unsigned int first = config.button_bindings[action].button[0];
    if (first != CONTROL_INVALID && controls_get_button_image_name_pc(storage, storage_len, first, controls_image_action_set(action)))
    {
        return true;
    }
    return controls_get_button_image_name_pc(storage, storage_len, config.button_bindings[action].button[1], controls_image_action_set(action));
}

// 0x00704DB0 (:2794)
bool controls_get_button_image_name(char* storage, int storage_len, control_button_action action, unsigned int controller_id)
{
    if (controller_id != 0)
    {
        return false;
    }
    return controls_get_button_image_name(Control_config[0][controls_get_current_config_setting()], storage, storage_len, action);
}

// ---- stub_misc.md group 3: the control names and images the menus and vint show, in file order. They follow the tables
// above, which is why they are not at their PDB lines among the older functions.

// 0x012D1E64
static const int Control_pc_mouse_axis_image_indicies[3] = {178, 155, 132};

// 0x012D2470: the 16 axes of a pad seen as the PC's gamepad, 4 more on image 130, the rest 0.
static const int Control_pc_gamepad_axis_image_indicies[128] =
{
    13, 36, 59, 82, 105, 128, 151, 174, 175, 152, 129, 106, 83, 60, 37, 14, 130, 130, 130, 130,
};

// 0x012D4D30: the Steam action sets of each control_axis_action.
static const control_action_set_mapping Steam_axis_action_set_mapping[26] =
{
    {0, 9}, {0, 9}, {0, 9}, {0, 9}, {2, 4}, {2, 4}, {2, 4}, {2, 4}, {1, 2}, {1, 2},
    {1, 2}, {1, 2}, {4, 16}, {0, 1}, {0, 0}, {0, 0}, {0, 0}, {0, 0}, {0, 0}, {0, 0},
    {0, 0}, {4, 16}, {1, 2}, {1, 2}, {4, 16}, {4, 16},
};

// 0x012D4E00: the first axis action of the pair each axis action belongs to.
static const unsigned int Steam_axis_pair_mapping[26] =
{
    0, 0, 2, 2, 4, 4, 6, 6, 8, 8, 10, 10, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 24,
};

static char out_button_name[0x40]; // 0x01E2B348
static char out_axis_name[0x40]; // 0x01E2B388

// 0x00704D00 (:2774)
bool controls_get_button_image_text_pc(char* storage, int storage_len, unsigned int button, unsigned int action_set)
{
    if (storage == nullptr || storage_len < 0x20)
    {
        return false;
    }
    char name[256];
    if (controls_get_button_image_name_pc(name, 0x100, button, action_set))
    {
        _snprintf_safe(storage, storage_len, "[image:%s]", name);
        return true;
    }
    _snprintf_safe(storage, storage_len, "");
    return false;
}

// 0x00702120 (:2883)
bool controls_get_gamepad_stick_image_name(char* storage, int storage_len, unsigned int stick, unsigned int axis, unsigned int action_set)
{
    if (storage == nullptr || storage_len < 0x20)
    {
        return false;
    }
    const char* name = controls_get_axis_image_name(stick, axis, action_set);
    if (name == nullptr)
    {
        return false;
    }
    _snprintf_safe(storage, storage_len, "%s", name);
    return true;
}

// 0x00704460 (:3010)
// An axis's images: a key pair or a pad's button pair fills both storages, the rest only the first.
bool controls_get_axis_image_name_pc(char* storage, char* storage2, int storage_len, unsigned int axis, unsigned int action_set)
{
    if (storage == nullptr || storage2 == nullptr || storage_len < 0x20)
    {
        return false;
    }
    memset(storage, 0, storage_len);
    memset(storage2, 0, storage_len);
    if (axis == CONTROL_INVALID)
    {
        _snprintf_safe(storage, storage_len, "%s", "key_unbound");
        return false;
    }
    const char* name;
    const unsigned int type = axis & CONTROL_TYPE_MASK;
    if (type == CONTROL_TYPE_KEYBOARD)
    {
        name = controls_get_key_image_name(axis & 0xFF);
        if (name != nullptr)
        {
            _snprintf_safe(storage2, storage_len, "%s", name);
        }
        name = controls_get_key_image_name((axis >> 8) & 0xFF);
    }
    else if (type == CONTROL_TYPE_MOUSE)
    {
        // Original bug kept: no check against the 3 mouse axes of the table.
        name = Control_pc_key_image_names[Control_pc_mouse_axis_image_indicies[axis & 0xFF]];
    }
    else if (type == 0)
    {
        if (input_get_playing_with_controller())
        {
            // Magic number: bit 27 marks an axis made of two buttons.
            if (((axis >> 27) & 1) == 0)
            {
                controls_get_gamepad_stick_image_name(storage, storage_len, (axis >> 8) & 0xFF, axis & 0xFF, action_set);
                return true;
            }
            controls_get_button_image_name(storage, storage_len, axis & 0xFF, action_set);
            controls_get_button_image_name(storage2, storage_len, (axis >> 8) & 0xFF, action_set);
            return true;
        }
        name = Control_pc_key_image_names[Control_pc_gamepad_axis_image_indicies[axis & 0xFF]];
    }
    else
    {
        return false;
    }
    if (name == nullptr)
    {
        return false;
    }
    _snprintf_safe(storage, storage_len, "%s", name);
    return true;
}

// 0x00704F30 (:3160)
// What vint's [button:name] tag asks for (vint_resource_cb.button_display_info): a button action, a stick action or the
// dpad by name. A stick action takes its primary binding when that has an image, else its secondary.
bool controls_get_display_info(char* storage, char* storage2, int storage_len, const char* name)
{
    if (storage_len < 1)
    {
        return false;
    }
    *storage = '\0';
    *storage2 = '\0';
    control_button_action button_action;
    if (Button_action_name_hash.get(localization_generate_hash_from_key(name), button_action))
    {
        return controls_get_button_image_name(storage, storage_len, button_action, Player_controller_index);
    }
    control_axis_action axis_action;
    if (!Stick_action_name_hash.get(localization_generate_hash_from_key(name), axis_action))
    {
        if (v_stricmp(name, "dpad") == 0)
        {
            return controls_get_dpad_image_name(storage, storage_len);
        }
        return false;
    }
    const unsigned int primary = Control_config[Player_controller_index][controls_get_current_config_setting()].axis_bindings[axis_action].axis[0];
    if (primary != CONTROL_INVALID && controls_get_axis_image_name_pc(storage, storage2, storage_len, primary, controls_get_action_set_from_axis_action(axis_action)))
    {
        return true;
    }
    const unsigned int secondary = Control_config[Player_controller_index][controls_get_current_config_setting()].axis_bindings[axis_action].axis[1];
    return controls_get_axis_image_name_pc(storage, storage2, storage_len, secondary, controls_get_action_set_from_axis_action(axis_action));
}

// 0x006F9DF0 (:5218)
void controls_prepend_control_type_wide(wchar_t* result, int str_len, unsigned int button, const wchar_t* btn_name)
{
    switch (button & CONTROL_TYPE_MASK)
    {
        // Original: the three cases are the same; nothing is prepended in the release build.
        case CONTROL_TYPE_MOUSE:
        case CONTROL_TYPE_KEYBOARD:
        case 0:
            _snprintf_safe(result, str_len, "%s", btn_name);
            return;
        default:
            // Int3 in the debug build.
            return;
    }
}

// 0x00704860 (:5232)
bool controls_get_button_name_wide(wchar_t* btnName, int str_len, unsigned int button, unsigned int action_set)
{
    if (button == CONTROL_INVALID)
    {
        wcsncpy(btnName, localization_get_display_string_req("MENU_NONE", nullptr), str_len);
        btnName[str_len - 1] = L'\0';
        return false;
    }
    wchar_t name[256];
    if ((button & CONTROL_TYPE_MASK) == CONTROL_TYPE_KEYBOARD)
    {
        char text[256];
        // Original bug kept: the text stays unset for a key code of 256 or more.
        if ((button & ~CONTROL_TYPE_KEYBOARD) < 0x100)
        {
            keen::formatString(text, 0x100, "[image:%s]", controls_get_key_image_name(button & ~CONTROL_TYPE_KEYBOARD));
        }
        keen::convertUTF8ToUTF16((uint16*)name, 0x100, text);
    }
    else if ((button & CONTROL_TYPE_MASK) == CONTROL_TYPE_MOUSE)
    {
        controls_get_mouse_button_name_wide(name, 0x100, button);
    }
    else
    {
        controls_get_gamepad_button_name_wide(name, 0x100, button, action_set);
    }
    controls_prepend_control_type_wide(btnName, str_len, button, name);
    return true;
}

// 0x006F9E50 (:5259)
bool controls_get_mouse_button_name_wide(wchar_t* btnName, int str_len, unsigned int button)
{
    const unsigned int index = button & ~CONTROL_TYPE_MOUSE;
    if (index >= 5)
    {
        return false;
    }
    _snprintf_safe(btnName, str_len, "[image:%S]", Control_pc_key_image_names[Control_pc_mouse_image_indicies[index]]);
    return true;
}

// 0x00702460 (:5281)
bool controls_get_gamepad_button_name_wide(wchar_t* btnName, int str_len, unsigned int button, unsigned int action_set)
{
    const char* name = controls_get_button_image_name(button, action_set);
    if (name != nullptr)
    {
        keen::formatString(out_button_name, 0x40, "[image:%s]", name);
        keen::convertUTF8ToUTF16((uint16*)btnName, str_len, out_button_name);
        return true;
    }
    _snprintf_safe(btnName, str_len, L"Button %d", button + 1);
    return true;
}

// 0x00704990 (:5294)
bool controls_get_axis_name_wide(wchar_t* btnName, int str_len, unsigned int button, unsigned int action_set, bool inverted)
{
    if (button == CONTROL_INVALID)
    {
        wcsncpy(btnName, localization_get_display_string_req("MENU_NONE", nullptr), str_len);
        btnName[str_len - 1] = L'\0';
        return false;
    }
    if (((button >> 27) & 1) != 0)
    {
        controls_get_button_axis_name_wide(btnName, str_len, button, action_set);
    }
    else if ((button & CONTROL_TYPE_MASK) == CONTROL_TYPE_MOUSE)
    {
        controls_get_mouse_axis_name_wide(btnName, str_len, button);
    }
    else
    {
        controls_get_gamepad_axis_name_wide(btnName, str_len, button, action_set);
    }
    if (inverted)
    {
        // Original bug kept: the destination is also the first argument.
        _snwprintf_safe(btnName, str_len, L"%s %s", btnName, localization_get_display_string_req("MENU_PC_INV", nullptr));
    }
    return true;
}

// 0x006F9F10 (:5317)
bool controls_get_mouse_axis_name_wide(wchar_t* btnName, int str_len, unsigned int button)
{
    const unsigned int index = button & ~CONTROL_TYPE_MOUSE;
    if (index >= 3)
    {
        return false;
    }
    _snprintf_safe(btnName, str_len, "[image:%S]", Control_pc_key_image_names[Control_pc_mouse_axis_image_indicies[index]]);
    return true;
}

// 0x006F9F50 (:5329)
bool controls_get_key_button_axis_name_wide(wchar_t* btnName, int str_len, unsigned int button)
{
    const unsigned int keys = button & ~CONTROL_TYPE_KEYBOARD;
    char first[256];
    char second[256];
    keen::formatString(first, 0x100, "[image:%s]", controls_get_key_image_name(keys & 0xFF));
    keen::formatString(second, 0x100, "[image:%s]", controls_get_key_image_name((keys >> 8) & 0xFF));
    wchar_t first_wide[256];
    wchar_t second_wide[256];
    keen::convertUTF8ToUTF16((uint16*)first_wide, 0x200, first);
    keen::convertUTF8ToUTF16((uint16*)second_wide, 0x200, second);
    _snprintf_safe(btnName, str_len, "%s, %s", first_wide, second_wide);
    return true;
}

// 0x006FA050 (:5353)
bool controls_get_mouse_button_axis_name_wide(wchar_t* btnName, int str_len, unsigned int button)
{
    const unsigned int buttons = button & ~CONTROL_TYPE_MOUSE;
    wchar_t first[256];
    wchar_t second[256];
    // Original bug kept: a text stays unset for a button number of 5 or more.
    if ((buttons & 0xFF) < 5)
    {
        _snprintf_safe(first, 0x100, "[image:%S]", Control_pc_key_image_names[Control_pc_mouse_image_indicies[buttons & 0xFF]]);
    }
    if (((buttons >> 8) & 0xFF) < 5)
    {
        _snprintf_safe(second, 0x100, "[image:%S]", Control_pc_key_image_names[Control_pc_mouse_image_indicies[(buttons >> 8) & 0xFF]]);
    }
    _snprintf_safe(btnName, str_len, "%s, %s", first, second);
    return true;
}

// 0x007024D0 (:5370)
bool controls_get_gamepad_button_axis_name_wide(wchar_t* btnName, int str_len, unsigned int button, unsigned int action_set)
{
    wchar_t names[2][256];
    const unsigned int ids[2] = {button & 0xFF, (button >> 8) & 0xFF};
    for (int i = 0; i < 2; i++)
    {
        const char* name = controls_get_button_image_name(ids[i], action_set);
        if (name == nullptr)
        {
            _snprintf_safe(names[i], 0x100, L"Button %d", ids[i] + 1);
        }
        else
        {
            keen::formatString(out_button_name, 0x40, "[image:%s]", name);
            keen::convertUTF8ToUTF16((uint16*)names[i], 0x100, out_button_name);
        }
    }
    _snprintf_safe(btnName, str_len, "%s, %s", names[0], names[1]);
    return true;
}

// 0x00702600 (:5385)
bool controls_get_button_axis_name_wide(wchar_t* btnName, int str_len, unsigned int button, unsigned int action_set)
{
    switch (button & CONTROL_TYPE_MASK)
    {
        case CONTROL_TYPE_MOUSE:
            controls_get_mouse_button_axis_name_wide(btnName, str_len, button);
            return true;
        case CONTROL_TYPE_KEYBOARD:
            controls_get_key_button_axis_name_wide(btnName, str_len, button);
            return true;
        case 0:
            controls_get_gamepad_button_axis_name_wide(btnName, str_len, button, action_set);
            return true;
        default:
            // Int3 in the debug build.
            return false;
    }
}

// 0x00702680 (:5401)
bool controls_get_gamepad_axis_name_wide(wchar_t* btnName, int str_len, unsigned int button, unsigned int action_set)
{
    const char* name = controls_get_axis_image_name((button >> 8) & 0xFF, button & 0xFF, action_set);
    if (name != nullptr)
    {
        // Original: the config setting is read and not used.
        controls_get_current_config_setting();
        keen::formatString(out_axis_name, 0x40, "[image:%s]", name);
        keen::convertUTF8ToUTF16((uint16*)btnName, str_len, out_axis_name);
        return true;
    }
    // Original: the whole control value, where the button text prints its number + 1.
    _snprintf_safe(btnName, str_len, L"Axis %d", button);
    return true;
}

// 0x00704A40 (:5479)
bool controls_get_axis_name(char* btnName, int str_len, unsigned int button, unsigned int action_set, bool inverted)
{
    if (button == CONTROL_INVALID)
    {
        _snprintf_safe(btnName, str_len, "MENU_NONE");
        return false;
    }
    if (((button >> 27) & 1) != 0)
    {
        controls_get_button_axis_name(btnName, str_len, button, action_set);
    }
    else if ((button & CONTROL_TYPE_MASK) == CONTROL_TYPE_MOUSE)
    {
        controls_get_mouse_axis_name(btnName, str_len, button);
    }
    else
    {
        controls_get_gamepad_axis_name(btnName, str_len, button, action_set);
    }
    if (inverted)
    {
        // Original bug kept: the destination is also the first argument, and sprintf has no length.
        sprintf(btnName, "%s %S", btnName, localization_get_display_string_req("MENU_PC_INV", nullptr));
    }
    return true;
}

// 0x006FA240 (:5502)
bool controls_get_mouse_axis_name(char* btnName, int str_len, unsigned int button)
{
    const unsigned int index = button & ~CONTROL_TYPE_MOUSE;
    if (index >= 3)
    {
        return false;
    }
    _snprintf_safe(btnName, str_len, "[image:%s]", Control_pc_key_image_names[Control_pc_mouse_axis_image_indicies[index]]);
    return true;
}

// 0x006FA280 (:5514)
bool controls_get_key_button_axis_name(char* btnName, int str_len, unsigned int button)
{
    const unsigned int keys = button & ~CONTROL_TYPE_KEYBOARD;
    char first[256];
    char second[256];
    keen::formatString(first, 0x100, "[image:%s]", controls_get_key_image_name(keys & 0xFF));
    keen::formatString(second, 0x100, "[image:%s]", controls_get_key_image_name((keys >> 8) & 0xFF));
    _snprintf_safe(btnName, str_len, "%s, %s", first, second);
    return true;
}

// 0x006FA340 (:5531)
bool controls_get_mouse_button_axis_name(char* btnName, int str_len, unsigned int button)
{
    const unsigned int buttons = button & ~CONTROL_TYPE_MOUSE;
    char first[256];
    char second[256];
    // Original bug kept: a text stays unset for a button number of 5 or more.
    if ((buttons & 0xFF) < 5)
    {
        _snprintf_safe(first, 0x100, "[image:%s]", Control_pc_key_image_names[Control_pc_mouse_image_indicies[buttons & 0xFF]]);
    }
    if (((buttons >> 8) & 0xFF) < 5)
    {
        _snprintf_safe(second, 0x100, "[image:%s]", Control_pc_key_image_names[Control_pc_mouse_image_indicies[(buttons >> 8) & 0xFF]]);
    }
    _snprintf_safe(btnName, str_len, "%s, %s", first, second);
    return true;
}

// 0x00702700 (:5548)
bool controls_get_gamepad_button_axis_name(char* btnName, int str_len, unsigned int button, unsigned int action_set)
{
    char names[2][256];
    const unsigned int ids[2] = {button & 0xFF, (button >> 8) & 0xFF};
    for (int i = 0; i < 2; i++)
    {
        const char* name = controls_get_button_image_name(ids[i], action_set);
        if (name == nullptr)
        {
            _snprintf_safe(names[i], 0x100, "Button %d", ids[i] + 1);
        }
        else
        {
            keen::formatString(out_button_name, 0x40, "[image:%s]", name);
            strncpy_safe(names[i], out_button_name, 0x100);
        }
    }
    _snprintf_safe(btnName, str_len, "%s, %s", names[0], names[1]);
    return true;
}

// 0x00702830 (:5563)
bool controls_get_button_axis_name(char* btnName, int str_len, unsigned int button, unsigned int action_set)
{
    switch (button & CONTROL_TYPE_MASK)
    {
        case CONTROL_TYPE_MOUSE:
            controls_get_mouse_button_axis_name(btnName, str_len, button);
            return true;
        case CONTROL_TYPE_KEYBOARD:
            controls_get_key_button_axis_name(btnName, str_len, button);
            return true;
        case 0:
            controls_get_gamepad_button_axis_name(btnName, str_len, button, action_set);
            return true;
        default:
            // Int3 in the debug build.
            return false;
    }
}

// 0x007028B0 (:5579)
bool controls_get_gamepad_axis_name(char* btnName, int str_len, unsigned int button, unsigned int action_set)
{
    const char* name = controls_get_axis_image_name((button >> 8) & 0xFF, button & 0xFF, action_set);
    if (name != nullptr)
    {
        // Original: the config setting is read and not used.
        controls_get_current_config_setting();
        keen::formatString(out_axis_name, 0x40, "[image:%s]", name);
        strncpy_safe(btnName, out_axis_name, str_len);
        return true;
    }
    // Original: the whole control value as the axis number, as in the wide version.
    _snprintf_safe(btnName, str_len, "Axis %d", button);
    return true;
}

// 0x006FB350 (:7048)
unsigned int controls_get_action_set_from_axis_action(control_axis_action action)
{
    const unsigned int action_set = keen::input::getSteamControllerActionSet(Keen_input_system);
    if ((Steam_axis_action_set_mapping[action].supported & (1u << (action_set & 0x1F))) == 0)
    {
        return Steam_axis_action_set_mapping[action].default_set;
    }
    return action_set;
}

// 0x006FFFA0 (:7061)
const char* controls_get_axis_action_image_names_steam(unsigned int axis, unsigned int stick, unsigned int action_set)
{
    const unsigned int axis_action = axis + stick * 2;
    keen::SteamControllerAction action;
    action.axisMode = keen::SteamControllerAxisMode_XAndY;
    action.buttonIndex = axis_action;
    action.type = keen::SteamControllerActionType_Axis;
    action.pName = nullptr;
    keen::SteamControllerActionOrigin origins[8];
    unsigned int count = 0;
    keen::input::getSteamControllerActionOrigins(Keen_input_system, action, action_set, origins, 8, &count);
    // Magic number: 26, NUM_CONTROL_AXIS_ACTIONS. The first of a pair shows a "none" origin when it has none.
    if (count == 0 && axis_action < 26 && Steam_axis_pair_mapping[axis_action] == axis_action)
    {
        origins[0] = keen::SteamControllerActionOrigin_None;
        count = 1;
    }
    return controls_bake_steam_origins_string(origins, count);
}

// 0x00700050 (:7093)
const char* controls_get_axis_image_name(unsigned int stick, unsigned int axis, unsigned int action_set)
{
    if (input_get_playing_with_controller() && input_get_controller_type() == keen::ControllerType_Steam)
    {
        return controls_get_axis_action_image_names_steam(axis, stick, action_set);
    }
    const keen::ControllerType type = input_get_controller_type();
    const bool ps4 = type == keen::ControllerType_DualShock4;
    const bool xbox = type == keen::ControllerType_Xbox360 || type == keen::ControllerType_XBone;
    const bool nintendo = type == keen::ControllerType_JoyConPair || type == keen::ControllerType_SwitchProController;
    if (!ps4 && !xbox && !nintendo)
    {
        // A DirectInput pad numbers its axes.
        static const char* const names[16] = {"dinput_axis_1", "dinput_axis_2", "dinput_axis_3", "dinput_axis_4", "dinput_axis_5", "dinput_axis_6", "dinput_axis_7", "dinput_axis_8", "dinput_axis_9", "dinput_axis_10", "dinput_axis_11", "dinput_axis_12", "dinput_axis_13", "dinput_axis_14", "dinput_axis_15", "dinput_axis_16"};
        const unsigned int index = axis + stick * 2;
        return index < 16 ? names[index] : "dinput_axis_unknown";
    }
    if (stick == 0)
    {
        if (ps4)
        {
            return axis == 0 ? "ui_ps4_ls_x" : axis == 1 ? "ui_ps4_ls_y" : "ui_ps4_ls";
        }
        if (xbox)
        {
            return axis == 0 ? "ui_xbone_ls_x" : axis == 1 ? "ui_xbone_ls_y" : "ui_xbone_ls";
        }
        return axis == 0 ? "ui_switch_ls_x" : axis == 1 ? "ui_switch_ls_y" : "ui_switch_ls";
    }
    if (stick == 1)
    {
        if (ps4)
        {
            return axis == 0 ? "ui_ps4_rs_x" : axis == 1 ? "ui_ps4_rs_y" : "ui_ps4_rs";
        }
        if (xbox)
        {
            return axis == 0 ? "ui_xbone_rs_x" : axis == 1 ? "ui_xbone_rs_y" : "ui_xbone_rs";
        }
        return axis == 0 ? "ui_switch_rs_x" : axis == 1 ? "ui_switch_rs_y" : "ui_switch_rs";
    }
    return nullptr;
}
