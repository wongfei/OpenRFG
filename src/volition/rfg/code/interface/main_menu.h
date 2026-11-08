// Original: volition/rfg/code/interface/main_menu.h
// Implementation: volition/rfg/code/interface/main_menu.cpp
//
// The main menu state: the title screen (the local player is caught by any input), the title music, the entries and
// their delayed selections, the attract video data. Placement of bink_data, bink_video_state and rfg_tracked_chunk_id
// inferred.
#pragma once

#include "volition/interface_lib/code/vint_external.h"
#include "volition/rfg/code/gamewide/gameseq.h"
#include "volition/rfg/code/interface/ui_document_save_load.h"

// Placement inferred (TU-private in rfg.pdb; the document manager reads Main_menu_delayed_selection).
enum menu_option_types
{
    MENU_OPTION_NONE = -1,
    MENU_OPTION_NEW_GAME = 0,
    MENU_OPTION_LOAD_GAME = 1,
    MENU_OPTION_WRECKING_CREW = 2,
    MENU_OPTION_XBOX_LIVE = 3,
    MENU_OPTION_DOWNLOAD_CONTENT = 4,
    MENU_OPTION_SYSLINK = 5,
    MENU_OPTION_OPTIONS = 6,
    MENU_OPTION_TITLE_SCREEN = 7,
    MENU_OPTION_EXIT = 8,
    NUM_MENU_OPTIONS = 9,
};

class mempool_base;
class rl_renderer;

enum bink_video_state
{
    BINK_VIDEO_TRIGGER_PLAY = 0,
    BINK_VIDEO_PAUSED = 1,
    BINK_VIDEO_PLAYING = 2,
    BINK_VIDEO_STOPPING = 3,
    BINK_VIDEO_ERROR = 4,
    BINK_VIDEO_LAST = 5,
};

enum rfg_tracked_chunk_id
{
    RFG_CHUNK_ID_SINGLEPLAYER = 0,
    RFG_CHUNK_ID_MULTIPLAYER = 0,
    RFG_CHUNK_ID_WRECKING_CREW = 0,
};

// size 0x128
struct bink_data
{
    void reset(); // 0x00889550 (:178)
    bink_data(); // 0x008895A0 (:193)

    char name[256]; // 0x00
    int handle; // 0x100
    bink_video_state cur_state; // 0x104
    float alpha; // 0x108
    vector2i position; // 0x10C
    vector2i size; // 0x114
    int total_frames; // 0x11C
    int curr_frame; // 0x120
    mempool_base* bink_pool; // 0x124
};

// Test seams (UI.md 2.7 items 2 and 3, deviation 4; off by default, main_menu.cpp): the local player is caught at
// once (--notitle, implied by --host/--join); a function run at the end of each main menu frame once the menu exists.
extern bool g_openrfgMainMenuNoTitle;
extern void (*g_openrfgMainMenuProcess)();

// UI.md UI5: file statics of rfg.exe's interface unity (main_menu.cpp), external since UI5: the multiplayer menus play
// the background video too.
extern bink_data Bink_info_main; // 0x01666148
void mm_process_bink_video(); // 0x008CF160 (:592)

// UI.md UI6: the save/load screen's mode, external since UI6 (the in-game options and the death options set it too).
extern save_load_ui_type Save_load_ui_type; // 0x02C0812C

void unused_dcf_level(); // 0x008896C0 (:381)
void unused_dcf_level_single(); // 0x008897D0 (:416)
void main_menu_clear_auto_catch_done(); // 0x008898C0 (:1060)
void main_menu_start_auto_catch(bool retry); // 0x008ABCD0 (:1066)
bool main_menu_check_chunk_ready(rfg_tracked_chunk_id chunk_id, const char* error_locatext); // 0x008F7450 (:1218)
void main_menu_init(); // 0x00919AA0 (:2546)
void main_menu_enter(game_state prev_state); // 0x0090A290 (:2584)
void main_menu_exit(game_state new_state); // 0x008F8160 (:2681)
void main_menu_pause_video(); // 0x008898D0 (:2721)
void main_menu_unpause_video(); // 0x008898F0 (:2728)
void main_menu_process_video(); // 0x008CF480 (:2735)
void main_menu_process(); // 0x00913910 (:2760)
void main_menu_handshake(rl_renderer*); // 0x008CF490 (:3283)
void main_menu_render(rl_renderer*); // 0x008F8290 (:3296)
void main_menu_show_menu(bool show); // 0x008CF4D0 (:3365)
void main_menu_start_title_music(); // 0x00889920 (:3411)
void main_menu_stop_title_music(); // 0x00889940 (:3431)
int main_menu_get_doc_handle(); // 0x00889960 (:3448)
int main_menu_get_control_id(); // 0x00889970 (:3454)
int main_menu_get_input_id(); // 0x00889980 (:3460)
int main_menu_get_selected_option(); // 0x00889990 (:3466)
bool main_menu_get_active(); // 0x008899A0 (:3472)
bool main_menu_is_processing_selection(); // 0x008899C0 (:3479)
void main_menu_notify_profile_change(); // 0x008F1AB0 (:3485)
void main_menu_display_character(bool display); // 0x008899D0 (:3504)
void main_menu_set_pending_state(game_state state); // 0x008899E0 (:3519)
