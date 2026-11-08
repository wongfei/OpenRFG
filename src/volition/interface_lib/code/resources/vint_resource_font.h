// Original: volition/interface_lib/code/resources/vint_resource_font.h
// Implementation: volition/interface_lib/code/resources/vint_resource_font.cpp
//
// A font resource (reference counted, through the game's font callbacks) and the loaded/free lists.
#pragma once

#include "volition/interface_lib/code/vint_external.h"

// size 0x18
class vint_resource_font
{
public:
    vint_resource_font(); // 0x0045E570 (.h:31)
    ~vint_resource_font(); // 0x0045E580 (.h:32)
    int get_height() const; // 0x00461320 (:229)
    int get_char_spacing(unsigned int _char, unsigned int _next_char) const; // 0x00461400 (:267)
    int get_string_width(const wchar_t* _string, int _start, int _num_chars) const; // 0x00461360 (:239)
    int get_leading_offset(); // 0x004613B0 (:253)
    static void add_ref(unsigned int _handle); // 0x00468A20 (.h:48)
    void add_ref() const; // 0x0045E590 (.h:39)
    static void release(unsigned int _handle); // 0x00468A50 (.h:56)
    void release(); // 0x00461260 (:201)
    bool is_reversed(bool& reversed) const; // 0x00461440 (:281)
    bool should_use_hyphens(bool& use_hyphens) const; // 0x00461470 (:294)
    bool char_is_reversed(unsigned int _char, vint_char_direction& direction) const; // 0x004614A0 (:307)
    bool char_can_start_lb(unsigned int _char, bool& can_start); // 0x004614E0 (:320)
    bool char_can_end_lb(unsigned int _char, bool& can_end); // 0x00461520 (:333)
    unsigned int get_handle() const; // 0x0045E5A0 (.h:64)
    int get_title_handle() const; // 0x0045E5B0 (.h:69)
    static bool system_init(); // 0x00460F80 (:67)
    static void system_shutdown(); // 0x00461090 (:82)
    static vint_resource_font* load(const char* _name); // 0x004610A0 (:87)
    static vint_resource_font* find(unsigned int _handle); // 0x00461200 (:139)
    static vint_resource_font* find_by_title_handle(unsigned int _title_handle); // 0x00461230 (:171)

private:
    int m_title_handle; // 0x00
    unsigned int m_name_crc; // 0x04
    unsigned int m_internal_handle; // 0x08
    int m_ref_count; // 0x0C
    vint_resource_font* next; // 0x10
    vint_resource_font* prev; // 0x14
};

extern vint_resource_font* Loaded_fonts; // 0x01A48654
extern vint_resource_font* Avail_fonts; // 0x01A48658
