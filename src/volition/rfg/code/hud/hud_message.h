// Original: volition/rfg/code/hud/hud_message.h
// Implementation: volition/rfg/code/hud/hud_message.cpp
//
// HUD messages (the message list, minor messages, pending messages) drawn by hud_message_render, and the user message
// types the HUD documents show. Placement of hud_user_message_types inferred: in ui_document_hud.h it made an include
// cycle with this header.
#pragma once

#include "volition/vlib/code/time/timestamp.h"

enum hud_message_handle
{
    INVALID_HUD_MESSAGE_HANDLE = -1,
    HUD_MESSAGE_HANDLE_FORCE_TO_32_BIT = 0x7FFFFFFF,
};

enum hud_user_message_types
{
    HUMT_SWAP = 0,
    HUMT_MP_KILL = 1,
    HUMT_MP_KILL_SHADOW = 2,
    HUMT_OTHER = 3,
    NUM_HUD_USER_MESSAGE_TYPES = 4,
};

// size 0x74
struct hud_message
{
    hud_message(); // 0x008448A0

    hud_message* next; // 0x00
    hud_message* prev; // 0x04
    char str[81]; // 0x08
    timestamp duration; // 0x5C
    int original_duration; // 0x60
    void* data1; // 0x64
    void* data2; // 0x68
    int* sound_effect; // 0x6C
    unsigned int uid; // 0x70
};

void hud_message_system_init(); // 0x00844920 (:131)
void hud_message_level_init_post(); // 0x00844A40 (:138)
void hud_message_process(); // 0x00844AA0 (:143)
void hud_message_render(); // 0x008559F0 (:190)
hud_message_handle hud_message_add(const char* str, float duration, void* data1, void* data2, int* sound_effect); // 0x00855CE0 (:238)
hud_message_handle hud_message_add(const wchar_t* str, float duration, void* data1, void* data2, int* sound_effect); // 0x00844AB0 (:250)
hud_message_handle hud_message_add_minor(const char* str, float display_time, bool is_special); // 0x00855D60 (:255)
hud_message_handle hud_message_add_minor(const wchar_t* str, float display_time, bool is_special); // 0x00844AE0 (:267)
void hud_message_get_icon_with_localized_text(wchar_t* out_string, int string_size, const wchar_t* icon_string, const char* unlocalized_string); // 0x00844B10 (:273)
hud_message_handle hud_message_add_user(const char* str, hud_user_message_types type, float position_x, float position_y, bool outlined, float lifespan); // 0x00855DD0 (:299)
hud_message_handle hud_message_add_user(const wchar_t* str, hud_user_message_types type, float position_x, float position_y, bool outlined, float lifespan); // 0x00844B60 (:311)
void hud_message_remove(hud_message_handle handle); // 0x00844BA0 (:316)
void hud_message_remove_user(hud_message_handle handle); // 0x00844BB0 (:321)
void hud_message_remove_minor(hud_message_handle handle); // 0x00844BC0 (:326)
void* hud_message_get_data1(hud_message_handle handle); // 0x00844BD0 (:331)
void* hud_message_get_data2(hud_message_handle handle); // 0x00844BE0 (:336)
hud_message_handle hud_message_change_message(hud_message_handle handle, const char* str, float duration, void* data1, void* data2); // 0x00855E60 (:341)
hud_message_handle hud_message_change_message(hud_message_handle handle, const wchar_t* str, float duration, void* data1, void* data2); // 0x00844BF0 (:353)
hud_message_handle hud_message_change_minor_message(hud_message_handle handle, const char* str, float duration); // 0x00855EE0 (:358)
hud_message_handle hud_message_change_minor_message(hud_message_handle handle, const wchar_t* str, float duration); // 0x00844C20 (:370)
hud_message_handle hud_message_change_user(hud_message_handle handle, const char* str); // 0x00855F50 (:375)
hud_message_handle hud_message_change_user(hud_message_handle handle, const wchar_t* str); // 0x00844C40 (:387)
