// Original: volition/interface_lib/code/vint_input.h
//
// Input as vint sees it: the input events documents subscribe to, and the game's maps from buttons, joystick
// directions, keys and raw inputs to them (set by vint_rfg_update_input_map). Placement of the types inferred: the PDB
// records no function of vint_input.h.
#pragma once

enum vint_input_event
{
    VINT_INPUT_EVENT_UNKNOWN = -1,
    VINT_INPUT_EVENT_SELECT = 0,
    VINT_INPUT_EVENT_ALT_SELECT = 1,
    VINT_INPUT_EVENT_EXIT = 2,
    VINT_INPUT_EVENT_BACK = 3,
    VINT_INPUT_EVENT_NAV_UP = 4,
    VINT_INPUT_EVENT_NAV_DOWN = 5,
    VINT_INPUT_EVENT_NAV_LEFT = 6,
    VINT_INPUT_EVENT_NAV_RIGHT = 7,
    VINT_INPUT_EVENT_JOY_UP = 8,
    VINT_INPUT_EVENT_JOY_DOWN = 9,
    VINT_INPUT_EVENT_JOY_LEFT = 10,
    VINT_INPUT_EVENT_JOY_RIGHT = 11,
    VINT_INPUT_EVENT_KEY_UP = 12,
    VINT_INPUT_EVENT_KEY_DOWN = 13,
    VINT_INPUT_EVENT_KEY_LEFT = 14,
    VINT_INPUT_EVENT_KEY_RIGHT = 15,
    VINT_INPUT_EVENT_WHITE = 16,
    VINT_INPUT_EVENT_BLACK = 17,
    VINT_INPUT_EVENT_SCROLL_UP = 18,
    VINT_INPUT_EVENT_SCROLL_DOWN = 19,
    VINT_INPUT_EVENT_SCROLL_LEFT = 20,
    VINT_INPUT_EVENT_SCROLL_RIGHT = 21,
    VINT_INPUT_EVENT_DPAD_UP = 22,
    VINT_INPUT_EVENT_DPAD_DOWN = 23,
    VINT_INPUT_EVENT_DPAD_LEFT = 24,
    VINT_INPUT_EVENT_DPAD_RIGHT = 25,
    VINT_INPUT_EVENT_ALL_UNASSIGNED = 26,
    NUM_VINT_INPUT_EVENTS = 27,
};

enum scroll_dirs
{
    INPUT_DIR_LEFT = 0,
    INPUT_DIR_RIGHT = 1,
    INPUT_DIR_DOWN = 2,
    INPUT_DIR_UP = 3,
    NUM_SCROLL_DIRS = 4,
};

enum vint_raw_input_type
{
    VINT_RAW_INPUT_BUTTON = 0,
    VINT_RAW_INPUT_JOY = 1,
    VINT_RAW_INPUT_KB = 2,
    NUM_VINT_RAW_INPUT_TYPES = 3,
};

enum vint_input_raw_map_id; // no definition in PDB

// size 0x10
struct vint_button_map
{
    unsigned int controller_id; // 0x00
    unsigned int button_id; // 0x04
    float threshold; // 0x08
    vint_input_event event; // 0x0C
};

// size 0x18
struct vint_joy_map
{
    unsigned int controller_id; // 0x00
    unsigned int joy_id; // 0x04
    scroll_dirs direction; // 0x08
    float threshold; // 0x0C
    vint_input_event event; // 0x10
    unsigned int delay_ms; // 0x14
};

// size 0x8
struct vint_kb_map
{
    unsigned int scancode; // 0x00
    vint_input_event event; // 0x04
};

// size 0x18
struct vint_raw_map
{
    unsigned int controller_id; // 0x00
    vint_raw_input_type type; // 0x04
    unsigned int button_or_stick; // 0x08
    unsigned char axis; // 0x0C
    vint_input_raw_map_id id; // 0x10
    const char* name; // 0x14
};
