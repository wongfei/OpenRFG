// Original: volition/rfg/code/interface/multi_player_menus/multi_player_menu_system.h
// Implementation: volition/rfg/code/interface/multi_player_menus/multi_player_menu_system.cpp
//
// The multiplayer menu system: the mp_shell documents' data items (mp_shell_data_item_info), the shared message boxes
// (migration, no opponents, unable to launch, leave confirm), the mouse arrows, the LAN search results the system link
// browser lists, and the menus' multi init/shutdown. Placement of the data item types inferred.
#pragma once

#include "volition/rfg/code/control/control_map.h"
#include "volition/rfg/code/interface/ui_message_box.h"
#include "volition/rfg/code/multi/multi_core/include/multi_core_session_base.h"
#include "volition/rfg/code/multi/multi_core/include/multi_find_servers.h"
#include "volition/rfg/code/multi/multi_game/multi_game_lobby.h"
#include "volition/rfg/code/multi/multi_core/include/multi_player_uid.h"
#include "volition/vlib/code/time/timestamp.h"

class mempool_base;
class multi_player_uid;
class vint_data_item;

enum mp_shell_data_item_types
{
    MPSDIT_HINT_INFO = 0,
    MPSDIT_TITLES = 1,
    MPSDIT_INFO_TEXT = 2,
    MPSDIT_SELECTION_INFO = 3,
    MPSDIT_MENU_TYPE = 4,
    MPSDIT_MATCHING_HINT = 5,
    MPSDIT_GAME_INFO = 6,
    MPSDIT_PARTY_INFO = 7,
    MPSDIT_PLAYLIST_INFO = 8,
    MPSDIT_PRESS_START = 9,
    MPSDIT_VETO_INFO = 10,
    MPSDIT_TIMER = 11,
    MPSDIT_TIPS = 12,
    MPSDIT_CHARACTER_SELECT = 13,
    MPSDIT_VIDEO = 14,
    MPSDIT_WC_CHARACTER = 15,
    NUM_MP_SHELL_DATA_ITEM_TYPES = 16,
};

enum mp_mouse_arrow_click_info
{
    MP_ARROW_CLICK_NONE = 0,
    MP_ARROW_CLICK_LEFT = 1,
    MP_ARROW_CLICK_RIGHT = 2,
    NUM_MP_ARROW_CLICK_STATES = 3,
};

// size 0x8
struct mp_shell_data_item_info
{
    mp_shell_data_item_types item_type; // 0x00
    void (*process_callback)(vint_data_item*); // 0x04
};

extern unsigned int s_lan_num_search_results; // 0x02C03598
extern bool s_lan_search_running; // 0x02C0357B
extern multi_server_info s_lan_server_infos[16]; // 0x02C4FAE0

// ---- UI.md UI5: shared statics of rfg.exe's interface unity, placed in multi_player_menu_system.cpp (UI0) and
// external here because the menu documents' files use them too.
enum mp_message_box_type
{
    MP_MBT_NONE = 0,
    MP_MBT_GENERIC = 1,
    MP_MBT_PLAYER_OPTIONS = 2,
    MP_MBT_CONFIRM_PLAYER_KICK = 3,
    MP_MBT_PARTY_INVITE = 4,
    MP_MBT_PARTY_IS_TO_LARGE = 5,
    MP_MBT_CONFIRM_LEAVE = 6,
    MP_MBT_CONFIRM_DESTROY_PARTY = 7,
    MP_MBT_UNABLE_TO_LAUNCH = 8,
    MP_MBT_CANNOT_JOIN = 9,
    MP_MBT_LOST_CONNECTION = 10,
    MP_MBT_MIGRATION = 11,
    MP_MBT_NO_OPPONENTS = 12,
};

extern const char* Mp_shell_doc_name; // 0x0166320C
extern int Mp_shell_doc_handle; // 0x0166325C
extern timestamp_realtime Server_search_ping_interval; // 0x01666274
extern multi_player_uid Playercard_player_uid; // 0x02BE9E14
extern int mpps_hint_config[2]; // 0x02C00E00
extern mp_shell_data_item_info* Mp_shell_data_item_infos; // 0x02C0359C
extern int Num_mp_shell_data_item_infos; // 0x02C035A0
extern bool Search_search_pong_enabled; // 0x02C035A4
extern mp_message_box_type Mp_msgbox_type; // 0x02C035A8
extern vint_data_item* Hint_data_item; // 0x02C035B0
extern int Num_hints_added; // 0x02C035B4
// The menus' row and hint object names: header constants in rfg.exe (one copy per file of the interface unity).
extern const char* kHintText1; // 0x016685E0 "hint_text_1"
extern const char* kHintText2; // 0x016685E4 "hint_text_2"
extern const char* kHintText3; // 0x016685E8 "hint_text_3"
extern const char* kHintText4; // 0x016685EC "hint_text_4"

void multi_player_menu_system_init(); // 0x009193B0 (:798)
void multi_player_menu_multi_init(mempool_base& pool); // 0x008DEC40 (:827)
void multi_player_menu_multi_shutdown(); // 0x008AC630 (:836)
void multi_player_menu_system_process(); // 0x008CF770 (:846)
void multi_player_menu_system_shell_load_doc(int& doc_handle, bool show_loading_bar); // 0x0090A590 (:901)
void multi_player_menu_system_shell_unload_doc(int& doc_handle); // 0x008F1B50 (:913)
void multi_player_menu_system_shell_load_data_items(mp_shell_data_item_info* infos, int num_infos); // 0x008CF7F0 (:923)
void multi_player_menu_system_shell_unload_data_items(); // 0x008CF8A0 (:962)
void multi_player_menu_system_shell_process_data_items(); // 0x0088A2F0 (:974)
void multi_player_menu_system_shell_process_game_info_item(vint_data_item* vdi); // 0x008DEC60 (:1018)
void multi_player_menu_system_start_searching_for_syslink_servers(); // 0x008AC650 (:1209)
void multi_player_menu_system_stop_searching_for_syslink_servers(); // 0x0088A3C0 (:1230)
void multi_player_menu_system_enable_syslink_pong(); // 0x0088A3D0 (:1236)
void multi_player_menu_system_disable_syslink_pong(); // 0x0088A3E0 (:1241)
bool multi_player_menu_system_has_open_message_box(); // 0x008CF8F0 (:1246)
void multi_player_menu_system_pull_up_player_option_message_box(const multi_player_uid& player_uid, const wchar_t* player_name, bool allow_mute); // 0x0090A5D0 (:1256)
void multi_player_menu_system_close_leave_confirm_box(); // 0x008DF500 (:1384)
void multi_player_menu_system_pull_up_leave_confirm_message_box(); // 0x008F84D0 (:1393)
void multi_player_menu_system_pull_up_migrate_message_box(); // 0x008F8860 (:1400)
void multi_player_menu_system_pull_up_lost_connection_box(); // 0x008F8C00 (:1408)
void multi_player_menu_system_pull_up_no_opponents_message_box(); // 0x008F8F80 (:1416)
void multi_player_menu_system_pull_up_party_too_large_message_box(); // 0x008F9310 (:1424)
void multi_player_menu_system_pull_up_dlc_problem_message_box(); // 0x008F96A0 (:1432)
void multi_player_menu_system_pull_up_cannot_join_this_session(); // 0x008F9A20 (:1440)
void multi_player_menu_system_pull_up_kick_from_party_confirm_message_box(const multi_player_uid& uid); // 0x008F9DB0 (:1448)
void multi_player_menu_system_pull_up_unable_to_launch_message_box(client_start_test_results result); // 0x008FA1F0 (:1471)
void multi_player_menu_system_pull_up_generic_message_box(const char* title, const char* message); // 0x008FA6E0 (:1502)
void multi_player_menu_system_pull_up_generic_message_box(msgbox_type mb_type, const char* title, const char* message, bool (*callback_fn)(int, int, msgbox_choice), const char* button1_override, const char* button2_override); // 0x008FAA60 (:1510)
void multi_player_menu_system_close_message_box(); // 0x008DF540 (:1518)
void multi_player_menu_system_setup_hint_item(vint_data_item* vdi); // 0x0088A3F0 (:1528)
void multi_player_menu_system_begin_hints(vint_data_item* vdi); // 0x0088A400 (:1552)
void multi_player_menu_system_add_hint(const wchar_t* hint_text, control_button_action button); // 0x0088A420 (:1560)
void multi_player_menu_system_end_hints(); // 0x0088A510 (:1580)
void multi_player_menu_system_state_stack_load(); // 0x0090B060 (:1661)
void multi_player_menu_system_state_stack_unload(); // 0x008F1B90 (:1704)
bool multi_player_menu_system_state_stack_will_be_pushed(); // 0x0088A650 (:1712)
void multi_player_menu_system_level_image_handshake(); // 0x008AC6C0 (:1724)
void multi_player_menu_system_level_image_reset(); // 0x008AC7A0 (:1762)
const char* multi_player_menu_system_level_image_get(); // 0x008AC820 (:1775)
const char* multi_player_menu_system_get_connection_status_string(multi_session_join_status status); // 0x0088A670 (:1786)
int multi_player_menu_system_input_mouse_hints(int docHandle, int num_hints); // 0x008CF920 (:1810)
void multi_player_menu_system_set_menu_selection(int new_selection, int& selection_var, int num_in_menu); // 0x008AC880 (:1848)
void multi_player_menu_system_handle_menu_key_input(int& selection_var, int num_in_menu, int input_id, bool& events_consumed); // 0x008CF9D0 (:1875)
int multi_player_menu_system_handle_menu_mouse_input(int& selection_var, int num_in_menu, int input_id, int doc_handle, bool events_consumed); // 0x008CFB70 (:1900)
void multi_player_menu_system_handle_arrow_clicks(mp_mouse_arrow_click_info& click_info, int doc_handle); // 0x008CFCA0 (:1932)
void multi_player_menu_system_clear_hint_config(int* config_array, int size, int& current_count); // 0x0088A700 (:1960)
void multi_player_menu_system_add_hint_config(int& num_hints, int* hint_config, int new_value); // 0x0088A730 (:1968)
