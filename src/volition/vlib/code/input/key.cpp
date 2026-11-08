// Original: volition/vlib/code/input/key.cpp
//
// The keyboard (player_minimal.md P6). key_poll queues the changes between the snapshot's keyboard
// (Cached_input_states, written by keen::rfg::Application::processInputEvent) and Key_states, then applies them in
// order with key_mark, which also fills the typed-key ring Key_buffer. Codes are keen key ids; bits 0x1000/0x2000/
// 0x4000 of a buffered code are shift/ctrl/alt (keys 5/9, 7/11, 6/10). The functions that read Global_time inline its
// getElapsedTime; here they call timer_get(TIMER_MILLISECONDS), which is the same.

#include "volition/vlib/code/input/input_internal.h"
#include "volition/vlib/code/input/key.h"

#include "keen/input/inputsystem.hpp"

#include <windows.h>

// Placement inferred.
// size 0x8
struct key_state_change
{
    unsigned int code; // 0x00
    bool down; // 0x04
};

key Key_states[256]; // 0x01CDD7D0
bool All_keys_masked; // 0x01CE86D6

static unsigned int Key_buffer[32]; // 0x01CD3320, typed keys with modifier bits (key_in)
// File statics of vlib's unity build in rfg.exe that vconsole.cpp reads and resets too (orig_main.md OM5); external
// here (input_internal.h).
unsigned int Key_buffer_head; // 0x01CDB4FC
unsigned int Key_buffer_tail; // 0x01CD2900
bool Key_lock_states[256]; // 0x01CDD540, toggled by keys 0-2 (num lock, caps lock, scroll lock)
static key_state_change Key_state_changes[32]; // 0x01CDD640, ring filled by key_mark_change and key_poll
bool Key_inited; // 0x01CE86D5
unsigned int Key_state_change_head; // 0x01CE86D8
unsigned int Key_state_change_tail; // 0x01CE86DC

// keen key ids used here (inferred from the uses)
static const unsigned int KEY_NUM_LOCK = 0;
static const unsigned int KEY_CAPS_LOCK = 1;
static const unsigned int KEY_LSHIFT = 5;
static const unsigned int KEY_LALT = 6;
static const unsigned int KEY_LCTRL = 7;
static const unsigned int KEY_RSHIFT = 9;
static const unsigned int KEY_RALT = 10;
static const unsigned int KEY_RCTRL = 11;
static const unsigned int KEY_ALTGR = 0x6B;
static const unsigned int KEY_MOD_SHIFT = 0x1000;
static const unsigned int KEY_MOD_CTRL = 0x2000;
static const unsigned int KEY_MOD_ALT = 0x4000;

static void key_buf_insert(unsigned int code); // 0x00568850 (:232)
unsigned int key_buf_pop(); // 0x00568880 (:245)
static unsigned int key_buf_peek(); // 0x005688E0 (:284)

// 0x00568820 (:168)
void key_convert_keen_key_to_text(char* output_buffer, unsigned int output_capacity, unsigned char key_event, bool holding_shift, bool holding_altgr, bool numlock_activated)
{
    if (output_capacity != 0)
    {
        *output_buffer = '\0';
    }
    keen::input::convertKeyEventToText(output_buffer, output_capacity, key_event, holding_shift, holding_altgr, numlock_activated);
}

// 0x00568850 (:232)
static void key_buf_insert(unsigned int code)
{
    const unsigned int next = (Key_buffer_tail + 1) & 0x1F;
    if (next != Key_buffer_head)
    {
        Key_buffer[Key_buffer_tail] = code;
        Key_buffer_tail = next;
    }
}

// 0x00568880 (:245)
// Original behaviour kept: it skips the flushed (zero) entries after the popped one, and returns the entry the head
// was on when called.
unsigned int key_buf_pop()
{
    if (Key_buffer_head == Key_buffer_tail)
    {
        return 0;
    }
    const unsigned int* entry = &Key_buffer[Key_buffer_head];
    do
    {
        Key_buffer_head = (Key_buffer_head + 1) & 0x1F;
        if (Key_buffer[Key_buffer_head] != 0)
        {
            break;
        }
    } while (Key_buffer_head != Key_buffer_tail);
    return *entry;
}

// 0x005688C0 (:271)
bool key_is_german_keyboard()
{
    const HKL layout = GetKeyboardLayout(GetCurrentThreadId());
    return ((unsigned int)(uintptr_t)layout & 0x3FF) == LANG_GERMAN;
}

// 0x005688E0 (:284)
static unsigned int key_buf_peek()
{
    if (Key_buffer_head == Key_buffer_tail)
    {
        return 0;
    }
    return Key_buffer[Key_buffer_head];
}

// 0x00568900 (:295)
void key_mark_change(unsigned int code, bool down)
{
    Key_state_changes[Key_state_change_tail].code = code & 0xFF;
    Key_state_changes[Key_state_change_tail].down = down;
    Key_state_change_tail = Key_state_change_tail == 31 ? 0 : Key_state_change_tail + 1;
}

// 0x0059A110 (:323)
// Applies one key change. A press (also of a key already down) buffers the key with its modifier bits. Keys 0x10, 0x12
// and 0xFF are buffered on release instead, without modifier bits. Keys 0-2 toggle their lock state.
void key_mark(unsigned int code, bool down)
{
    const unsigned int scancode = code & 0xFF;
    const bool release_key = scancode == 0x10 || scancode == 0xFF || scancode == 0x12;
    key& k = Key_states[scancode];
    if (down)
    {
        if (!k.down)
        {
            k.just_down = true;
            k.time_down = timer_get(TIMER_MILLISECONDS);
        }
        unsigned int buffered = scancode;
        if (Key_states[KEY_LSHIFT].down || Key_states[KEY_RSHIFT].down)
        {
            buffered |= KEY_MOD_SHIFT;
        }
        if (Key_states[KEY_LCTRL].down || Key_states[KEY_RCTRL].down)
        {
            buffered |= KEY_MOD_CTRL;
        }
        if (Key_states[KEY_LALT].down || Key_states[KEY_RALT].down)
        {
            buffered |= KEY_MOD_ALT;
        }
        k.last_repeat = timer_get(TIMER_MILLISECONDS);
        if (!release_key)
        {
            key_buf_insert(buffered);
        }
    }
    else if (release_key && k.down)
    {
        key_buf_insert(scancode); // inlined in the binary
    }
    k.down = down;
    if (scancode < 3 && down)
    {
        Key_lock_states[scancode] = !Key_lock_states[scancode];
    }
}

// 0x0059A270 (:396)
void key_init()
{
    if (!Key_inited)
    {
        key_flush();
        Key_inited = true;
    }
}

// 0x00568940 (:409)
void key_flush()
{
    for (int i = 0; i < 256; i++)
    {
        Key_states[i].down = false;
        Key_states[i].just_down = false;
        Key_states[i].last_repeat = 0;
        Key_states[i].time_down = 0;
    }
    Key_state_change_tail = 0;
    Key_state_change_head = 0;
    Key_buffer_tail = 0;
    Key_buffer_head = 0;
}

// 0x00568980 (:426)
void key_flush_key(unsigned int scancode)
{
    Key_states[scancode].just_down = false;
    Key_states[scancode].down = false;
    Key_states[scancode].last_repeat = 0;
    Key_states[scancode].time_down = 0;
    for (unsigned int i = Key_buffer_head; i != Key_buffer_tail; i = (i + 1) & 0x1F)
    {
        if (Key_buffer[i] == scancode)
        {
            Key_buffer[i] = 0;
        }
    }
}

// 0x005689D0 (:443)
void key_to_text(char* output_buffer, unsigned int output_capacity, unsigned int keycode)
{
    const bool holding_altgr = Key_states[KEY_ALTGR].down;
    const bool numlock = Key_lock_states[KEY_NUM_LOCK];
    if (!Key_inited)
    {
        if (output_capacity != 0)
        {
            *output_buffer = '\0';
        }
        return;
    }
    bool shift = ((keycode >> 12) & 1) != 0;
    if (Key_lock_states[KEY_CAPS_LOCK])
    {
        shift = !shift;
    }
    key_convert_keen_key_to_text(output_buffer, output_capacity, (unsigned char)(keycode & 0xFF), shift, holding_altgr, numlock); // inlined in the binary
}

// 0x00568A60 (:472)
unsigned int key_in()
{
    return key_buf_pop();
}

// 0x00568A70 (:477)
unsigned int key_in_peek()
{
    return key_buf_peek();
}

// 0x00568A90 (:484)
bool key_pressed(unsigned int scancode, bool* just_pressed)
{
    if (just_pressed != nullptr)
    {
        *just_pressed = Key_states[scancode].just_down;
    }
    return Key_states[scancode].down;
}

// 0x00568AC0 (:497)
bool key_down(unsigned int scancode)
{
    return Key_states[scancode].down && !Key_states[scancode].masked && !All_keys_masked;
}

// 0x00568AF0 (:505)
bool key_just_pressed(unsigned int scancode)
{
    return Key_states[scancode].just_down && !Key_states[scancode].masked && !All_keys_masked;
}

// Whether the code's modifier bits match the modifier keys held now (shared by the *_with_mods functions).
static bool key_mods_match(unsigned int code)
{
    const bool shift = key_down(KEY_LSHIFT) || key_down(KEY_RSHIFT);
    const bool alt = key_down(KEY_LALT) || key_down(KEY_RALT);
    const bool ctrl = key_down(KEY_LCTRL) || key_down(KEY_RCTRL);
    return ((code >> 12) & 1) == (unsigned int)shift && ((code >> 13) & 1) == (unsigned int)ctrl && ((code >> 14) & 1) == (unsigned int)alt;
}

// 0x00568B20 (:510)
bool key_pressed_with_mods(unsigned int code, bool* just_pressed)
{
    if (!key_mods_match(code))
    {
        return false;
    }
    if (just_pressed != nullptr)
    {
        *just_pressed = Key_states[code & 0xFF].just_down;
    }
    return Key_states[code & 0xFF].down;
}

// 0x00568C20 (:543)
bool key_down_with_mods(unsigned int code)
{
    return key_mods_match(code) && key_down(code & 0xFF);
}

// 0x00568D10 (:576)
bool key_just_pressed_with_mods(unsigned int code)
{
    return key_mods_match(code) && key_just_pressed(code & 0xFF);
}

// 0x00568E00 (:615)
bool key_query_modifier(unsigned int key)
{
    switch (key & 0xFF)
    {
    case KEY_LSHIFT:
    case KEY_LALT:
    case KEY_LCTRL:
    case KEY_RSHIFT:
    case KEY_RALT:
    case KEY_RCTRL:
        return true;
    default:
        return false;
    }
}

// 0x0059A290 (:633)
// Queues each key that differs from the snapshot's keyboard, clears the per-frame flags and masks, then applies the
// queued changes in order.
void key_poll()
{
    const cached_keyboard_state& keyboard = Cached_input_states[Current_cached_input_state].keyboard;
    unsigned int tail = Key_state_change_tail;
    for (unsigned int i = 0; i < 256; i++)
    {
        const bool down = keyboard.keys[i];
        if (down != Key_states[i].down)
        {
            Key_state_changes[tail].code = i & 0xFF;
            Key_state_changes[tail].down = down;
            tail = tail == 31 ? 0 : tail + 1;
        }
    }
    All_keys_masked = false;
    Key_state_change_tail = tail;
    for (int i = 0; i < 256; i++)
    {
        Key_states[i].just_down = false;
        Key_states[i].masked = false;
    }
    while (Key_state_change_head != Key_state_change_tail)
    {
        key_mark(Key_state_changes[Key_state_change_head].code, Key_state_changes[Key_state_change_head].down);
        Key_state_change_head = Key_state_change_head == 31 ? 0 : Key_state_change_head + 1;
    }
}

// 0x00568E40 (:693)
void key_mask(unsigned int scancode)
{
    if (scancode < 256)
    {
        Key_states[scancode].masked = true;
    }
}

// 0x00568E60 (:700)
void key_mask_all()
{
    All_keys_masked = true;
}

// 0x00568E70 (:706)
bool key_is_masked(unsigned int scancode)
{
    return All_keys_masked || Key_states[scancode].masked;
}
