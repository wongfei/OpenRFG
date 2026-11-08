// Original: volition/rfg/code/interface/ui_message_box.h
// Implementation: volition/rfg/code/interface/ui_message_box.cpp
//
// Message boxes (in rfg_ui_utility): type, title, text, caption items, choices and their callback, the open box set, the
// save icon, the voice chat speaker list and the debug text items. Placement of the info types and enums inferred.
#pragma once

#include "unit4/systemservices/systemservices.hpp"
#include "volition/vlib/code/memory/object_pool.h"
#include "volition/vlib/code/os/vconsole.h"
#include "volition/vlib/code/util/color.h"
#include "volition/vlib/code/util/varray.h"

enum msgbox_type
{
    MBT_INVALID = -1,
    MBT_OK = 0,
    MBT_ACCEPT_CANCEL = 1,
    MBT_YES_NO = 2,
    MBT_SAVE = 3,
    MBT_SAVE_PROFILE = 4,
    MBT_LOADING = 5,
    MBT_LOADING_CANCEL = 6,
    MBT_ACCEPT_CANCEL_SELECT = 7,
    MBT_ACCEPT_SELECT = 8,
    MBT_CONFIG_CONTROL = 9,
    NUM_MESSAGE_BOX_TYPES = 10,
};

enum msgbox_choice
{
    MBC_OK = 0,
    MBC_YES = 0,
    MBC_ABORT = 1,
    MBC_FORCED_EXIT = 2,
    MBC_INVALID = 3,
};

enum save_icon_type
{
    SAVE_ICON_TYPE_MANUAL = 0,
    SAVE_ICON_TYPE_AUTO = 1,
    SAVE_ICON_TYPE_OPTIONS = 2,
    SAVE_ICON_TYPE_PROFILE = 3,
};

enum debug_text_posn
{
    DT_TOP_LEFT = 0,
    DT_TOP_RIGHT = 1,
    DT_BOT_LEFT = 2,
    DT_BOT_RIGHT = 3,
    DT_CENTER_LEFT = 4,
    DT_CENTER_RIGHT = 5,
    DT_LAST = 6,
};

// size 0x64
struct voice_chat_speak_duration
{
    static float getKey(const voice_chat_speak_duration& data); // 0x008A17D0 (:249)
    voice_chat_speak_duration(const voice_chat_speak_duration& __that); // 0x008BE230
    voice_chat_speak_duration(); // 0x008A17E0

    unit4::RemoteUserId user_id; // 0x00
    char user_name[81]; // 0x0A
    float speech_duration; // 0x5C
    color team_color; // 0x60
};

// size 0x430
struct msgbox_info
{
    void clear(); // 0x008B9890 (:1287)
    msgbox_info(); // 0x008CC480 (:1279)

    signed char handle; // 0x00
    int menu2_handle; // 0x04
    farray<int,16> menu2_item_handles; // 0x08
    msgbox_type type; // 0x58
    float display_time; // 0x5C
    float remove_time; // 0x60
    bool (*callback)(int, int, msgbox_choice); // 0x64
    int input_id; // 0x68
    char caption[768]; // 0x6C
    char loading_text[192]; // 0x36C
    bool has_function_items; // 0x42C
    bool is_modified; // 0x42D
};

// size 0x1130
struct msgbox_info_set
{
    void clear_all(); // 0x008F0460 (:1306)
    msgbox_info_set(); // 0x008F6C50 (:152)
    ~msgbox_info_set(); // 0x008CC270

    object_pool<msgbox_info,4,1> msgbox_info_pool; // 0x00
    bool save_load_active; // 0x1120
};

extern bool Cursor_visible; // 0x02C081B5 (file static in the binary)
// UI.md UI6: the background image behind the resolution change (options_display.cpp sets them).
extern char Background_image_name[64]; // 0x02BFE030
extern bool Display_background_image; // 0x02C081B6
extern msgbox_info_set Message_box_info_set; // 0x02CA0340 (file static in the binary; external since UI3)
extern bool Subtitles_enabled; // 0x01664FD2
// UI.md UI5: the lobby's text chat (a file static in the binary).
extern bool Ui_text_chat_enabled; // 0x02C081B7
extern int mp_stats_hint_config[7]; // 0x02BF214C (the statistics menu's hints)

void unused_dcf_show_loading_screen(); // 0x00908E60 (:56)
void unused_dcf_hide_loading_screen(); // 0x008EFB40 (:61)
void unused_dcf_toggle_subtitle_name(); // 0x008A17B0 (:66)
void unused_dcf_subtitle_add(); // 0x008F6B90 (:70)
void ui_util_background_image_create_data(); // 0x008CC280 (:383)
void ui_util_show_background_image(const char* img_name); // 0x008A17F0 (:404)
void ui_util_process_background_image_data(); // 0x008A1820 (:415)
void ui_show_save_icon(save_icon_type icon_type); // 0x008B9840 (:786)
void unused_dcf_ui_msgbox_test(); // 0x008A1B60 (:1098)
void unused_dcf_ui_kill_all_msgbox(); // 0x008F0450 (:1116)
bool ui_is_msgbox_active(int mbh); // 0x008CC4F0 (:1312)
void ui_kill_subtitle(); // 0x008A1BF0 (:1337)
void ui_add_subtitle(unsigned int text_crc, const char* persona_name, int duration); // 0x008F04B0 (:1342)
void ui_msgbox_init(); // 0x009095E0 (:1377)
void ui_msgbox_shutdown(); // 0x008F0620 (:1390)
void ui_msgbox_process(float dtime); // 0x009095F0 (:1411)
void ui_msgbox_render(); // 0x008F06B0 (:1449)
int ui_add_msgbox(msgbox_type type, const char* title, const char* description, bool (*callback_fn)(int, int, msgbox_choice), bool surpress_accept_sounds, bool ignore_save_load_status, const char* button1_override, const char* button2_override, bool pause_music); // 0x008F6C70 (:1471)
int ui_msgbox_add_caption_item(int mbh, const char* caption, bool surpress_accept_sound, bool set_active_controller); // 0x008F06E0 (:1647)
int ui_msgbox_add_int_item(int mbh, const char* caption, int* variable, int min, int max, int incr); // 0x008CC570 (:1672)
int ui_msgbox_add_exp_item(int mbh, const char* caption, int* variable, int min, int max, int exp); // 0x008CC600 (:1687)
int ui_msgbox_add_slider_item(int mbh, const char* caption, int* variable, int min, int max, int incr); // 0x008CC690 (:1702)
int ui_msgbox_add_bool_item(int mbh, const char* caption, bool* variable); // 0x008CC720 (:1717)
int ui_msgbox_add_char_list_item(int mbh, const char* caption, const char** list, int num_chars); // 0x008CC7A0 (:1732)
bool ui_hide_msgbox(int handle, bool ignore_tcr_time_requirements); // 0x008DCE30 (:1747)
unsigned int ui_add_debug_text(const char* text, debug_text_posn type); // 0x008A1C00 (:1802)
bool ui_update_debug_text(unsigned int handle, const char* text); // 0x008A1C10 (:1859)
bool ui_remove_debug_text(unsigned int handle); // 0x008A1C20 (:1888)
int ui_msgbox_get_menu_handle(int mbh); // 0x008CC820 (:1928)
float ui_msgbox_get_remove_time(int mbh); // 0x008CC840 (:1941)
void ui_set_subtitles(bool on); // 0x008A1C30 (:1951)
void ui_msgbox_process_mouse(); // 0x008F07B0 (:1955)
void ui_cursor_set_visible_internal(bool visible); // 0x008CC860 (:2022)
void ui_cursor_set_visible(bool visible); // 0x008A1C40 (:2041)
bool ui_cursor_get_visible(); // 0x008A1C70 (:2052)
void ui_cursor_set_pos(float x, float y); // 0x008CC8E0 (:2057)
void ui_cursor_update(); // 0x008DCF60 (:2074)
void ui_update_msgbox_caption(int mbh, const char* new_caption, bool force); // 0x008CC980 (:2100)
void ui_cursor_check_for_auto_hide(bool visible); // 0x008CCA60 (:2127)
void ui_msgbox_hide_selection_hints(bool hide); // 0x008CCAC0 (:2148)
void ui_enable_chat_window(vchat_position chat_position); // 0x008A1C80 (:2156)
void ui_disable_chat_window(); // 0x008A1C90 (:2164)
