// Original: volition/rfg/code/interface/loading.h
// Implementation: volition/rfg/code/interface/loading.cpp
//
// The boot and loading screens: the boot state (gs_boot_*), the load state (gs_load_*), the loading screen layouts and
// tips (loading_screens.xtbl), and the district images of the loading screen.
#pragma once

#include "keen/base/language.hpp"
#include "volition/rfg/code/gamewide/gameseq.h"
#include "volition/rfg/code/interface/ui_document_manager.h"
#include "volition/rfg/code/interface/ui_message_box.h"

class rl_renderer;

namespace keen
{
    class GraphicsCommandWriter;
}

enum loading_screen_layout_type
{
    LSLT_NONE = 0,
    LSLT_NEWS_BROADCAST = 1,
    LSLT_LEVEL_NAME = 2,
    LSLT_POSTER = 3,
    LSLT_NUM_TYPES = 4,
};

enum loading_screen_tip_type
{
    LSTT_INVALID = -1,
    LSTT_SINGLE_PLAYER = 0,
    LSTT_MULTIPLAYER = 1,
    LSTT_BOTH = 2,
    LSTT_NUM_TYPES = 3,
};

// size 0x8
struct loading_screen_image
{
    char* filename; // 0x00
    float rotation; // 0x04
};

// size 0x40
struct loading_screen_layout
{
    loading_screen_layout(); // 0x008882D0 (.h:89)

    char* name; // 0x00
    unsigned int name_crc; // 0x04
    loading_screen_layout_type layout_type; // 0x08
    int overlay_prob; // 0x0C
    float rotation; // 0x10
    char* bg_image; // 0x14
    char* string1; // 0x18
    char* string2; // 0x1C
    char* string3; // 0x20
    char* string4; // 0x24
    char* string5; // 0x28
    int num_fg_images; // 0x2C
    loading_screen_image fg_images[2]; // 0x30
};

// size 0x10
struct loading_screen_tip_data
{
    char* title; // 0x00
    char* text; // 0x04
    char* icon_image; // 0x08
    loading_screen_tip_type type; // 0x0C
};

// size 0x8
struct district_index_data
{
    district_index_data(); // 0x00888590 (:92)

    int territory_index; // 0x00
    int district_index; // 0x04
};

extern int Loading_screen_handle; // 0x01664B44 (file static in the binary)
extern int Loading_screen_ref_cnt; // 0x02C080A0 (file static in the binary)
extern game_state Gs_switch_next_state; // 0x01663110 (file static in the binary)
extern bool Gs_switch_transparent; // 0x02C034FA (file static in the binary)
extern bool Gs_switch_pause_beneath; // 0x02C034FB (file static in the binary)
// UI.md UI3: statics of rfg.exe's interface unity placed in loading.cpp and shared with the document manager and
// the message boxes (file statics in the binary).
extern int Current_input_lock_id; // 0x01664B4C
extern int Current_input_lock_index; // 0x01664B50
extern bool Input_enabled_uimen; // 0x01664FD0
extern const char* cBlank_string; // 0x01664FF0
extern int Utility_doc_handle; // 0x01664FF8
extern int Msg_box_data_item_handle; // 0x01664FFC
extern bool Player_input_was_enabled; // 0x0166500C
extern bool Player_camera_input_was_enabled; // 0x0166500D
extern farray<unsigned int,16> Controller_slots; // 0x01667378
extern farray<input_request_data,16> Input_types; // 0x016673C8
extern bool Fade_out; // 0x02C08097
extern int Fade_ref; // 0x02C080A8
extern bool Paused_game; // 0x02C081AE
extern bool Locked_controls; // 0x02C081B4
// UI.md UI5: the loading tips the game lobby shows too (file statics of loading.cpp in the binary).
extern farray<loading_screen_tip_data,50> Tip_data; // 0x01665D88
extern int Current_tip; // 0x02C034D0

void game_boot_thread_render(keen::GraphicsCommandWriter* pCommandWriter, rl_renderer* pRenderer); // 0x008AAC40 (:500)
bool language_select_msg_cb(int mbox_handle, int selection_index, msgbox_choice choice); // 0x008BF5D0 (:519)
const char* get_language_img_name(float& scale, keen::LanguageId language); // 0x00888620 (:568)
void loading_screen_parse_images_table(int dlc_id); // 0x008BF720 (:1148)
void loading_screen_dlc_restore(); // 0x008AAD60 (:1221)
void loading_system_init(); // 0x009136D0 (:2930)
void loading_system_shutdown(); // 0x00888DC0 (:3033)
void game_boot_thread_start(); // 0x00888DD0 (:3039)
void game_boot_thread_stop(); // 0x00888DF0 (:3052)
bool game_boot_thread_running(); // 0x00888E20 (:3062)
void game_push_multi_init(game_state next_state, bool transparent, bool pause_beneath); // 0x00888E30 (:3068)
void game_set_multi_shutdown(game_state next_state); // 0x00888E80 (:3082)
void game_push_dlc_init(game_state next_state, bool transparent, bool pause_beneath); // 0x00888ED0 (:3103)
void game_set_dlc_shutdown(game_state next_state); // 0x00888F10 (:3112)
void game_push_wc_init(game_state next_state, bool transparent, bool pause_beneath); // 0x00888F40 (:3124)
void game_set_wc_shutdown(game_state next_state); // 0x00888F80 (:3133)
void gs_boot_set_installing(bool installing); // 0x00888FB0 (:3144)
void gs_boot_set_failed_boot(bool failed); // 0x00888FC0 (:3149)
void gs_boot_set_install_time(unsigned int timeleft); // 0x00888FD0 (:3154)
void gs_boot_set_install_percentage(float percentage); // 0x00889010 (:3159)
const loading_screen_tip_data* loading_screen_tip_get_data(); // 0x008ABA30 (:3165)
void loading_screen_tip_randomize(); // 0x008BF950 (:3174)
void loading_screen_tip_cycle(bool forward); // 0x008ABA50 (:3182)
void loading_screen_set_district(int territory_index, int district_index); // 0x00889020 (:3216)
