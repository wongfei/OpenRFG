// Original: volition/rfg/code/interface/ui_document_hud.h
// Implementation: volition/rfg/code/interface/ui_document_hud.cpp
//
// The HUD documents (Hud_doc_names: rfg_ui_hud, rfg_ui_messages, rfg_ui_hud_health), their modules (satellite, rail
// driver, sniper), messages, radio messages, item pickups and the swap menu; hud_ui_enter/process/render. Placement of
// hud_doc_info inferred; hud_user_message_types is in hud_message.h.
#pragma once

#include "volition/audio_lib/code/audiolib_enums.h"
#include "volition/rfg/code/hud/hud_message.h"

class human;
struct message_info;
struct radio_message_info;
class vector;

enum hud_modules
{
    HUD_MODULE_NONE = 0,
    HUD_MODULE_SATELLITE = 1,
    HUD_MODULE_RAIL_DRIVER = 2,
    HUD_MODULE_SNIPER = 3,
    HUD_MODULE_LAST = 4,
};

enum hud_doc_info
{
    HDI_HEALTH = 0,
    HDI_BASE = 1,
    HDI_MESSAGES = 2,
    HDI_LAST = 3,
};

// size 0xC
struct item_pickup_info
{
    item_pickup_info(const char* name, int picked_up, int total); // 0x00753D10 (.h:130)
    item_pickup_info() // 0x00A05340 (.h:124)
    {
        item_name = nullptr;
        num_picked_up = 0;
        total_val = 0;
    }

    const char* item_name; // 0x00
    int num_picked_up; // 0x04
    int total_val; // 0x08
};

// size 0x60
struct item_icon_info
{
    item_icon_info(); // 0x00883610 (.h:95)

    bool full; // 0x00
    char item_string[16]; // 0x01
    char item_name[64]; // 0x11
    unsigned int amount_added; // 0x54
    unsigned int data_item_handle; // 0x58
    bool is_salvage; // 0x5C
};

// size 0x12C
struct item_info_set
{
    item_info_set(); // 0x00883630 (.h:112)

    bool updated; // 0x00
    int num_icons; // 0x04
    int prev_num_icons; // 0x08
    item_icon_info icon_info[3]; // 0x0C
};

void unused_dcf_add_hud_message(); // 0x008E8100 (:343)
void unused_dcf_add_secondary_hud_message(); // 0x008E81A0 (:360)
void unused_dcf_add_secondary_hud_message_special(); // 0x008E8240 (:377)
void unused_dcf_morale_test_mission_unlock(); // 0x008940E0 (:405)
radio_message_info* RadioMsgID_to_RadioMsgInfo(unsigned short id); // 0x008B0D70 (:429)
void ui_document_hud_clear_persona_language_prebuild_flag(); // 0x00894520 (:594)
void ui_hud_fix_up_persona_names(); // 0x008E83B0 (:599)
float hud_ui_get_radio_persona_radio_freq(const char* persona_name); // 0x008D7390 (:634)
const char* hud_ui_get_radio_persona_name(const char* persona_name, bool* found); // 0x008E8520 (:655)
void hud_ui_get_subtitle_display_text(unsigned int text_crc, const char* persona_name, bool show_name, char* output_string, int output_size); // 0x008E8640 (:693)
void ui_process_primary_messages(float dtime); // 0x008E8890 (:1664)
void ui_process_secondary_messages(float dtime); // 0x008E8E50 (:1798)
void ui_process_user_messages(float dtime); // 0x008D7A10 (:1929)
void hud_ui_cancel_radio_messages(); // 0x008E96C0 (:2446)
void hud_ui_clear_current_radio_message(const unsigned short id); // 0x008B1BF0 (:2569)
void hud_ui_set_radio_message_icon(const char* icon_name); // 0x00895680 (:3058)
void hud_ui_unset_radio_message_icon(); // 0x00895690 (:3063)
radio_message_info* hud_ui_add_radio_message_common(float visible_time, int cue_id, unsigned char priority, bool player, bool display_as_subtitle, const char* persona_name, unsigned int crc, const char* msg, audiolib_result& alib_result, const vector* pos, bool play_intro, bool play_outro, human* hp); // 0x008E9810 (:3076)
unsigned short hud_ui_add_radio_message(unsigned int crc, float visible_time, const int cue_id, unsigned char priority, bool player, bool display_as_subtitle, const char* persona_name, audiolib_result& alib_result, int& play_id, const vector* pos, bool play_intro, bool play_outro, human* hp); // 0x008E9B60 (:3217)
bool hud_ui_is_radio_message_valid(unsigned short id, bool* visible, bool* radio_line); // 0x008B1D10 (:3237)
int hud_ui_radio_get_play_id_from_msg(unsigned short id); // 0x008B1DC0 (:3269)
bool hud_ui_is_radio_message_active(); // 0x008B1E00 (:3281)
int hud_ui_get_radio_message_remaining_time(unsigned short id); // 0x008B1E30 (:3288)
message_info* ui_get_free_primary_message(); // 0x008D7F50 (:3303)
message_info* ui_get_free_secondary_message(); // 0x008D7FA0 (:3325)
message_info* ui_get_free_user_message(); // 0x008D7FF0 (:3347)
bool ui_primary_string_exists(unsigned int crc); // 0x008C4F70 (:3370)
bool ui_primary_string_exists(const wchar_t* text); // 0x008C4FF0 (:3384)
bool ui_secondary_string_exists(const wchar_t* text); // 0x008C5070 (:3404)
bool ui_secondary_message_is_playing_fadeout(const wchar_t* text); // 0x008C50F0 (:3421)
hud_message_handle ui_add_primary_message(unsigned int crc, float display_time, void* data1, void* data2, int* sound); // 0x008D8170 (:3483)
hud_message_handle ui_add_primary_message(const wchar_t* text, float display_time, void* data1, void* data2, int* sound); // 0x008D81E0 (:3504)
hud_message_handle ui_add_secondary_message(const wchar_t* text, float display_time, bool use_secondary_anim, bool force_redisplay); // 0x008D8270 (:3526)
hud_message_handle ui_add_user_message(const wchar_t* text, hud_user_message_types type, float position_x, float position_y, bool outlined, float lifespan); // 0x008D83D0 (:3582)
void ui_remove_primary_message(hud_message_handle handle); // 0x008D84F0 (:3630)
void ui_remove_secondary_message(hud_message_handle handle); // 0x008D85B0 (:3649)
void ui_remove_user_message(hud_message_handle handle); // 0x008D8670 (:3668)
hud_message_handle ui_change_primary_message(hud_message_handle handle, const wchar_t* text, float display_time, void* data1, void* data2); // 0x008D8730 (:3686)
hud_message_handle ui_change_secondary_message(hud_message_handle handle, const wchar_t* text, float display_time); // 0x008C51B0 (:3739)
hud_message_handle ui_change_user_message(hud_message_handle handle, const wchar_t* text); // 0x008D88D0 (:3788)
void* ui_primary_message_get_data1(hud_message_handle handle); // 0x008C5320 (:3823)
void* ui_primary_message_get_data2(hud_message_handle handle); // 0x008C53A0 (:3842)
int hud_ui_get_document_handle(hud_doc_info info); // 0x008956A0 (:3863)
void hud_ui_reload_items(); // 0x009167A0 (:3872)
void hud_ui_unload_items(); // 0x00904690 (:3941)
bool hud_ui_is_loaded(); // 0x008956C0 (:4005)
bool hud_ui_set_loaded_module(hud_modules module); // 0x00918D30 (:4011)
hud_modules hud_ui_get_loaded_module(); // 0x008956F0 (:4130)
void hud_ui_enter(); // 0x00918ED0 (:4136)
void hud_ui_process(); // 0x00916940 (:4225)
void hud_ui_render(); // 0x00904880 (:4405)
void hud_ui_exit(); // 0x00919200 (:4499)
void hud_ui_add_item_pickup_message(const item_pickup_info* pickup_info_list, int num_pickups); // 0x00895700 (:4553)
void hud_ui_set_swap_menu_active(bool active); // 0x00895870 (:4620)
bool hud_ui_is_swap_menu_active(); // 0x00895880 (:4626)
void hud_ui_set_swap_data(unsigned int primary, unsigned int secondary, bool weapon_swap); // 0x008B1EA0 (:4632)
void hud_ui_change_morale(float new_morale_percent, float old_morale_percent, float morale_change, unsigned int district_handle); // 0x008B1FC0 (:4676)
void hud_ui_change_control(int new_control_value, float new_control_percent, int control_value_change, float delta_control_percent, unsigned int district_handle, bool mission_completed, int missions_unlocked); // 0x008B2060 (:4717)
void hud_ui_convert_time_for_output(char* output, int output_size, int time_ms); // 0x00895890 (:4781)
void hud_ui_convert_time_for_output(char* output, int output_size, float time_seconds); // 0x008958E0 (:4788)

// multiplayer.md MP6: the spectator camera hides the HUD (a reference count; placement inferred, no line info).
void hud_hide(bool hide); // 0x00841A90 (hud.cpp:1472)
