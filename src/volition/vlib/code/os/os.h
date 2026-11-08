// Original: volition/vlib/code/os/os.h
// Implementation: volition/vlib/code/os/os.cpp
//
// The OS layer: the game window, message pump, focus and suspend handling, date and time.
#pragma once

extern bool Os_app_active; // 0x01519591, the game window is the active one (true in .data)

unsigned long os_get_window_style(const bool fullscreen); // 0x0058B000 (:116)
bool os_get_client_to_screen_coords(int* screen_x, int* screen_y, const int client_x, const int client_y); // 0x005AD7D0 (:138)
bool os_should_close(); // 0x0058B020 (:169)
void os_set_fullscreen(const bool fullscreen); // 0x0058B030 (:174)
void os_received_input(); // 0x0058B150 (:209)
void os_set_focus_change_callback(void (*callback)(bool, bool)); // 0x0058B160 (:215)
bool os_create_window(int w, int h, bool fullscreen, const wchar_t* const device_name); // 0x005AD830 (:371)
void os_sleep(int time_milliseconds); // 0x0058B420 (:619)
void os_micro_sleep(int time_microseconds); // 0x0058B430 (:624)
unsigned int os_get_window(); // 0x0058B440 (:637)
bool os_foreground(); // 0x005ADA70 (:649)
bool os_minimized(); // 0x0058B450 (:654)
void os_hook_message(bool (*function)(unsigned int, unsigned int, unsigned int)); // 0x0058B460 (:670)
void os_unhook_message(bool (*function)(unsigned int, unsigned int, unsigned int)); // 0x0058B480 (:681)
void os_set_title(const wchar_t* title); // 0x0058B4D0 (:696)
void os_poll(); // 0x0058B510 (:708)
void os_close(); // 0x0058B520 (:713)
void os_get_date(char* out, unsigned int capacity); // 0x0058B540 (:725)
void os_get_time(char* out, unsigned int capacity); // 0x0058B5F0 (:744)
