// Original: volition/rfg/code/interface/ui_document_hud_internal.h
//
// The HUD documents' message sets: on-screen messages and radio messages with their pools.
#pragma once

#include "volition/rfg/code/hud/hud_message.h"
#include "volition/vlib/code/math/vector2.h"
#include "volition/vlib/code/memory/object_pool.h"

class human;
class vint_document;

// size 0x47C
struct message_info
{
    void clear(); // 0x008941E0 (ui_document_hud.cpp:483)
    message_info(); // 0x008B0CF0 (.h:203)

    unsigned int m_obj_handle; // 0x00
    unsigned int m_text_obj_handle; // 0x04
    unsigned int m_fadein_handle; // 0x08
    unsigned int m_fadeout_handle; // 0x0C
    unsigned int m_anim_handle; // 0x10
    unsigned int m_anim2_handle; // 0x14
    unsigned int m_fadein2_handle; // 0x18
    unsigned int m_fadeout2_handle; // 0x1C
    unsigned int m_bg2_handle; // 0x20
    vector2 m_obj_scale; // 0x24
    float m_display_time; // 0x2C
    float m_running_time; // 0x30
    float m_fadein_time; // 0x34
    float m_fadein2_time; // 0x38
    float m_fadeout_time; // 0x3C
    float m_fadeout2_time; // 0x40
    int m_width; // 0x44
    int m_height; // 0x48
    int m_base_width; // 0x4C
    int m_base_height; // 0x50
    wchar_t m_text[512]; // 0x54
    unsigned int m_crc; // 0x454
    void* m_data1; // 0x458
    void* m_data2; // 0x45C
    bool m_dirty; // 0x460
    bool m_use_secondary_anim; // 0x461
    int* m_audio_cue; // 0x464
    hud_message_handle m_handle; // 0x468
    vector2 m_position; // 0x46C
    hud_user_message_types m_user_message_type; // 0x474
    bool m_outlined; // 0x478
};

// size 0x4DA0
struct hud_ui_message_set
{
    hud_ui_message_set(); // 0x008E80C0 (.h:228)
    ~hud_ui_message_set(); // 0x008C4F30

    vint_document* m_msg_doc; // 0x00
    int m_primary_font; // 0x04
    int m_secondary_font; // 0x08
    int m_user_font; // 0x0C
    vector2 m_primary_scale; // 0x10
    vector2 m_secondary_scale; // 0x18
    vector2 m_user_scale; // 0x20
    object_pool<message_info,3,1> m_primary_pool; // 0x30
    object_pool<message_info,3,1> m_secondary_pool; // 0xE00
    object_pool<message_info,11,1> m_user_pool; // 0x1BD0
};

// size 0x478
struct radio_message_info
{
    void drop(); // 0x00894190 (ui_document_hud.cpp:472)
    void clear(); // 0x00894120 (ui_document_hud.cpp:445)
    radio_message_info(); // 0x008B0CE0 (.h:163)

    const char* m_display_name; // 0x00
    unsigned int m_crc; // 0x04
    unsigned int m_start_time; // 0x08
    float m_radio_message_time_left; // 0x0C
    float m_char_uncover_time; // 0x10
    float m_radio_freq; // 0x14
    int m_char_uncover_count; // 0x18
    char m_last_state; // 0x1C
    char m_state; // 0x1D
    char m_radio_message[1024]; // 0x1E
    char m_icon_name[64]; // 0x41E
    int m_msg_sound_cue_id; // 0x460
    int m_intro_sound_play_id; // 0x464
    int m_msg_sound_play_id; // 0x468
    int m_outro_sound_play_id; // 0x46C
    human* m_human; // 0x470
    bool m_dirty : 1; // 0x474:0
    bool m_kill_me : 1; // 0x474:1
    bool m_paused : 1; // 0x474:2
    bool m_display_as_subtitle : 1; // 0x474:3
    short m_rolling_count; // 0x476
};

// size 0x4810
struct radio_msg_info_set
{
    radio_msg_info_set(); // 0x008E80A0 (.h:211)
    ~radio_msg_info_set(); // 0x008C4F20

    object_pool<radio_message_info,16,1> m_radio_info_pool; // 0x00
};
