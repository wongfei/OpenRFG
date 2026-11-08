// Original: volition/interface_lib/code/resources/vint_resource_sound.h
// Implementation: volition/interface_lib/code/resources/vint_resource_sound.cpp
//
// A sound resource (a cue through the game's sound callbacks) and the loaded/free lists.
#pragma once

// size 0x18
class vint_resource_sound
{
public:
    vint_resource_sound(); // 0x00461560 (.h:30)
    ~vint_resource_sound(); // 0x00461570 (.h:31)
    unsigned int play() const; // 0x00461830 (:210)
    float get_duration() const; // 0x00461860 (:224)
    static void pause(unsigned int _handle, bool _pause); // 0x00461890 (:238)
    static void stop(unsigned int _handle); // 0x004618B0 (:245)
    static bool is_playing(unsigned int _handle); // 0x004618D0 (:252)
    static bool is_paused(unsigned int _handle); // 0x00461920 (:266)
    static void add_ref(unsigned int _handle); // 0x00469C20 (.h:53)
    void add_ref() const; // 0x00461580 (.h:49)
    static void release(unsigned int _handle); // 0x00469C50 (.h:62)
    void release(); // 0x00461770 (:182)
    unsigned int get_handle() const; // 0x00461590 (.h:70)
    int get_title_handle() const;
    static bool system_init(); // 0x004615A0 (:78)
    static void system_shutdown(); // 0x004615D0 (:93)
    static vint_resource_sound* load(const char* _name); // 0x004615E0 (:98)
    static vint_resource_sound* find(unsigned int _handle); // 0x00461740 (:155)

private:
    int m_title_handle; // 0x00
    unsigned int m_name_crc; // 0x04
    unsigned int m_internal_handle; // 0x08
    int m_ref_count; // 0x0C
    vint_resource_sound* next; // 0x10
    vint_resource_sound* prev; // 0x14
};

extern vint_resource_sound* Loaded_sounds; // 0x01A48660
extern vint_resource_sound* Avail_sounds; // 0x01A48664
