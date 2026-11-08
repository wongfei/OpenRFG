// Original: volition/rfg/code/multi/multi_core/include/multi_core_platform_internal.h
// Implementation: volition/rfg/code/multi/multi_core/generic/multi_core_platform_generic.cpp
//
// The platform implementation behind multi_core_platform.h (multi_core_platform_generic.cpp on PC: the keen remote game
// session and GameLink, Steam profiles, achievements and leaderboards).
#pragma once

#include "volition/rfg/code/interface/ui_message_box.h"
#include "volition/rfg/code/multi/multi_core/include/multi_core_platform.h"

struct connection_address;
class mempool;
class mempool_base;
class multi_player_uid;

namespace kaiko
{
    namespace gamelink
    {
        class GameLink;
    }
}

namespace unit4
{
    class RemoteGameSession;
    struct RankingResult;
    struct SessionInteractionData;
}

namespace keen
{
    class Mutex;
}

// ---- globals of the platform implementation (no source file in the PDB; placed in multi_core_platform_generic.cpp,
// their main user, except Login_changed_cb in multi_core_platform.cpp)
extern unit4::RemoteGameSession Remote_game_session; // 0x0167F500, the GameLink sessions (LAN and internet)
extern keen::Mutex g_gameLinkMutex; // 0x02FEB77C, guards Remote_game_session (the game link is polled from jobs)
extern kaiko::GameLinkConnectionType g_gameLinkConnectionType; // 0x02FEB608, of the last session started
extern bool Multi_core_platform_last_session_start_succeeded; // 0x0167D7C9
extern float current_voicechat_volume; // 0x0167D7F4
extern int Remote_game_session_msgbox_handle; // 0x0167D7E8
extern unsigned int Remote_game_session_msgbox_selection; // 0x0167D7EC
extern unit4::SessionInteractionData Remote_game_session_last_interaction; // 0x01691500
extern platform_op_status Scoreboard_read_progress; // 0x0167D7F0
extern scoreboard_results* Last_scoreboard_results; // 0x02FEB578
extern unit4::RankingResult Current_scoreboard_ranking_results; // 0x02FE5BDC
extern unsigned int Max_num_ranking_results; // 0x02FEB574
extern scoreboard_column_info Last_scoreboard_column_infos[4]; // 0x02FEB398
extern bool Scoreboard_read_op_active; // 0x02FEB571
extern void (*Login_changed_cb)(bool, bool); // 0x02FE5BA8

unit4::RemoteGameSession* get_remote_game_session(); // 0x00941E60 (multi_core_platform_generic.cpp:106)
kaiko::gamelink::GameLink* get_game_link_session(); // 0x00941E70 (multi_core_platform_generic.cpp:111)
void multi_core_platform_internal_system_init(); // 0x00941E80 (multi_core_platform_generic.cpp:123)
void multi_core_platform_internal_system_shutdown(); // 0x00941F80 (multi_core_platform_generic.cpp:169)
bool handleRemoteGameSessionMsgboxResponse(int mbox_handle, int selection_index, msgbox_choice choice); // 0x009706F0 (multi_core_platform_generic.cpp:176)
void multi_core_platform_internal_system_process(); // 0x00970750 (multi_core_platform_generic.cpp:205)
void multi_core_platform_internal_system_handshake(); // 0x00941FB0 (multi_core_platform_generic.cpp:412)
void multi_core_platform_internal_system_render(); // 0x00941FC0 (multi_core_platform_generic.cpp:417)
bool multi_core_platform_internal_ui_is_active(); // 0x00941FD0 (multi_core_platform_generic.cpp:422)
bool multi_core_platform_internal_is_signed_into_profile(); // 0x00941FE0 (multi_core_platform_generic.cpp:427)
bool multi_core_platform_internal_is_connected_to_network(); // 0x00941FF0 (multi_core_platform_generic.cpp:435)
void multi_core_platform_internal_set_game_type_context(platform_game_type_context); // 0x00942000 (multi_core_platform_generic.cpp:440)
void multi_core_platform_internal_set_presence(platform_game_presence presence); // 0x00942010 (multi_core_platform_generic.cpp:445)
void multi_core_platform_internal_set_active_controller(int controller_idx); // 0x00942030 (multi_core_platform_generic.cpp:450)
bool multi_core_platform_internal_get_player_name(wchar_t* player_name, int max_player_name_size); // 0x00970B40 (multi_core_platform_generic.cpp:456)
int multi_core_platform_internal_get_gamer_picture_buffer_size(); // 0x00942040 (multi_core_platform_generic.cpp:482)
bool multi_core_platform_internal_get_gamer_picture(const multi_player_uid& uid, void* buffer, void (*callback)(const multi_player_uid&, bool)); // 0x00942050 (multi_core_platform_generic.cpp:488)
bool multi_core_platform_internal_format_gamer_picture(unsigned char* pic); // 0x00942060 (multi_core_platform_generic.cpp:497)
void multi_core_platform_internal_update_achievement_progress(int achievement_no, unsigned int progress); // 0x00942070 (multi_core_platform_generic.cpp:504)
void multi_core_platform_internal_award_achievement(int achievement_no); // 0x009420B0 (multi_core_platform_generic.cpp:518)
void multi_core_platform_internal_show_login_ui(bool allow_non_live); // 0x009420F0 (multi_core_platform_generic.cpp:534)
void multi_core_platform_internal_set_session_is_started(unsigned int); // 0x00942100 (multi_core_platform_generic.cpp:543)
void multi_core_platform_internal_set_user_playlist_version(unsigned int); // 0x00942110 (multi_core_platform_generic.cpp:548)
void multi_core_platform_internal_set_user_dlc_mask(unsigned int); // 0x00942120 (multi_core_platform_generic.cpp:553)
void multi_core_platform_internal_set_user_playlist(unsigned int); // 0x00942130 (multi_core_platform_generic.cpp:558)
void multi_core_platform_internal_set_game_mode_context(platform_game_mode_context); // 0x00942140 (multi_core_platform_generic.cpp:563)
platform_nat_type multi_core_platform_internal_get_nat_type(); // 0x00942150 (multi_core_platform_generic.cpp:568)
void multi_core_platform_internal_multi_init(mempool_base& pool); // 0x00942160 (multi_core_platform_generic.cpp:575)
void multi_core_platform_internal_multi_shutdown(); // 0x00942170 (multi_core_platform_generic.cpp:580)
bool multi_core_platform_internal_signin_change_is_pending(); // 0x00942180 (multi_core_platform_generic.cpp:584)
bool multi_core_platform_internal_is_signed_into_profile(int controller_index); // 0x00970BE0 (multi_core_platform_generic.cpp:590)
void multi_core_platform_internal_show_gamercard(const multi_player_uid& player_uid); // 0x00942190 (multi_core_platform_generic.cpp:597)
bool multi_core_platform_internal_allow_gamer_card_request(); // 0x009421C0 (multi_core_platform_generic.cpp:603)
void multi_core_platform_internal_show_player_review(const multi_player_uid& player_uid); // 0x009421D0 (multi_core_platform_generic.cpp:609)
void multi_core_platform_internal_show_friends_ui(); // 0x009421E0 (multi_core_platform_generic.cpp:615)
bool multi_core_platform_internal_session_start(const multi_core_session_info& session_info); // 0x009A7EF0 (multi_core_platform_generic.cpp:621)
void multi_core_platform_internal_session_end(); // 0x00942210 (multi_core_platform_generic.cpp:644)
bool multi_core_platform_internal_session_has_critical_error(); // 0x00942230 (multi_core_platform_generic.cpp:650)
void multi_core_platform_internal_register_massage_send_data_callback(bool (*callback)(const unsigned char*, int, unsigned char*, int, int&, unsigned char*, int&)); // 0x00942260 (multi_core_platform_generic.cpp:656)
bool multi_core_platform_internal_send_packet(const connection_address& target, unsigned char* send_data, unsigned int data_length); // 0x00942270 (multi_core_platform_generic.cpp:664)
unsigned int multi_core_platform_internal_receive_packet(connection_address& origin, unsigned char* data_buffer, unsigned int capacity); // 0x0098E060 (multi_core_platform_generic.cpp:699)
void multi_core_platform_internal_set_game_property(platform_game_property, unsigned int); // 0x00942300 (multi_core_platform_generic.cpp:727)
platform_op_status multi_core_platform_internal_get_user_playlist_state(); // 0x00942310 (multi_core_platform_generic.cpp:732)
platform_op_status multi_core_platform_internal_get_user_dlc_mask_state(); // 0x00942320 (multi_core_platform_generic.cpp:738)
platform_op_status multi_core_platform_internal_get_user_playlist_version_state(); // 0x00942330 (multi_core_platform_generic.cpp:744)
platform_op_status multi_core_platform_internal_get_session_is_started_state(); // 0x00942340 (multi_core_platform_generic.cpp:750)
void multi_core_platform_internal_retrieve_remote_file(unsigned char**, unsigned int*); // 0x00942350 (multi_core_platform_generic.cpp:756)
platform_op_status multi_core_platform_internal_get_retrieve_remote_file_state(); // 0x00942360 (multi_core_platform_generic.cpp:761)
void multi_core_platform_internal_remote_file_process(); // 0x00942370 (multi_core_platform_generic.cpp:767)
void multi_core_platform_internal_get_player_skill(skill_info* skill, const multi_player_uid& player_uid); // 0x009A7F70 (multi_core_platform_generic.cpp:772)
void multi_core_platform_internal_get_aggregate_skill(skill_info& aggregate_skill, skill_info* skills, unsigned int num_skills); // 0x00942380 (multi_core_platform_generic.cpp:795)
bool multi_core_platform_internal_get_large_skill_variance(const multi_player_uid*, unsigned int); // 0x00942480 (multi_core_platform_generic.cpp:809)
void multi_core_platform_internal_get_logged_in_users(unsigned int* controller_indices, int& num_users, const int max_users); // 0x00942490 (multi_core_platform_generic.cpp:815)
int multi_core_platform_internal_get_dlc_avail(); // 0x009424B0 (multi_core_platform_generic.cpp:824)
void multi_core_platform_set_my_player_uid(bool fromLocalUserAccountId); // 0x0098E100 (multi_core_platform_generic.cpp:829)
bool multi_core_platform_internal_scoreboard_init(mempool* pool, int max_entries); // 0x0098E280 (multi_core_platform_generic.cpp:881)
void multi_core_platform_internal_scoreboard_shutdown(); // 0x009424C0 (multi_core_platform_generic.cpp:897)
void multi_core_platform_internal_scoreboard_retrieve_data(scoreboard_results* results, const scoreboard_request* request); // 0x00970C30 (multi_core_platform_generic.cpp:909)
platform_op_status multi_core_platform_internal_scoreboard_retrieve_state(); // 0x00970D00 (multi_core_platform_generic.cpp:977)
void multi_core_platform_internal_scoreboard_cancel_retrieve(); // 0x009424F0 (multi_core_platform_generic.cpp:1028)
bool multi_core_platform_internal_scoreboard_begin_upload_score(const multi_player_uid& player_uid, unsigned short table_id, scoreboard_column* scores, unsigned int num_scores); // 0x00942510 (multi_core_platform_generic.cpp:1035)
