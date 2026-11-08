// Original: volition/rfg/code/multi/multi_core/include/multi_core_platform.h
// Implementation: volition/rfg/code/multi/multi_core/global/multi_core_platform.cpp
//
// The platform layer seen by the game: sign-in and profiles, presence and game contexts, achievements, skill, scoreboards
// (leaderboards), invites, remote files and the DLC mask. The platform enums and the scoreboard and skill types are
// placed here (inferred).
#pragma once

#include "kaiko/gamelink/gamelink.hpp"
#include "volition/rfg/code/multi/multi_core/include/multi_player_uid.h"

struct DiscordUser;
struct connection_address;
class mempool;
class mempool_base;

enum platform_game_type_context
{
    PGTC_RANKED = 0,
    PGTC_PUBLIC = 1,
    NUM_PLATFORM_GAME_TYPE_CONTEXTS = 2,
};

enum platform_game_mode_context
{
    PGMC_MAIN_MENU = 0,
    PGMC_SINGLE_PLAYER = 1,
    PGMC_MULTIPLAYER = 2,
    PGMC_PLAYLIST0 = 3,
    PGMC_PLAYLIST1 = 4,
    PGMC_PLAYLIST2 = 5,
    PGMC_PLAYLIST3 = 6,
    PGMC_PLAYLIST4 = 7,
    PGMC_PLAYLIST5 = 8,
    PGMC_PLAYLIST6 = 9,
    PGMC_PLAYLIST7 = 10,
    PGMC_PLAYLIST8 = 11,
    PGMC_PLAYLIST9 = 12,
    PGMC_PLAYLIST10 = 13,
    PGMC_PLAYLIST11 = 14,
    PGMC_PLAYLIST12 = 15,
    PGMC_PLAYLIST13 = 16,
    PGMC_PLAYLIST14 = 17,
    PGMC_PLAYLIST15 = 18,
    NUM_PLATFORM_GAME_MODE_CONTEXTS = 19,
};

extern const char* Platform_game_type_context_names[NUM_PLATFORM_GAME_TYPE_CONTEXTS]; // 0x0167DDB8
extern const char* Platform_game_mode_context_names[NUM_PLATFORM_GAME_MODE_CONTEXTS]; // 0x0167DDC0

enum platform_game_presence
{
    PGMP_IDLE = 0,
    PGMP_MAIN_MENU = 1,
    PGMP_MP_END_MATCH = 2,
    PGMP_MP_MATCHMAKING = 3,
    PGMP_MP_MENUS = 4,
    PGMP_MP_PARTY_LOBBY = 5,
    PGMP_MATCH = 6,
    PGMP_WC = 7,
    PGMP_LOADING = 8,
    PGMP_MP_QUICK_MATCH = 9,
    PGMP_MP_SPECTATOR = 10,
    PGMP_SP_ACTIVITY = 11,
    PGMP_SP_CUTSCENE = 12,
    PGMP_SP_DEFAULT = 13,
    PGMP_SP_MISSION = 14,
    NUM_PGMP_OPTIONS = 15,
};

enum platform_nat_type
{
    PNT_OPEN = 0,
    PNT_MODERATE = 1,
    PNT_RESTRICTED = 2,
    PNT_UNKNOWN = 3,
};

enum platform_game_property
{
    PGP_GAME_MODE = 0,
    PGP_MP_TIER = 1,
    PGP_MP_VERSION = 2,
    PGP_MP_DLC_MASK = 3,
    PGP_MP_PLAYLISTID = 4,
    NUM_PLATFORM_GAME_PROPERTIES = 5,
};

enum platform_op_status
{
    POS_SUCCESS = 0,
    POS_FAILED = 1,
    POS_IN_PROGRESS = 2,
};

enum platform_scoreboard_type
{
    PST_XP = 0,
    PST_WINS = 1,
    PST_KILLS = 2,
    PST_DESTRUCTION = 3,
    PST_WRECKING_CREW = 4,
    NUM_PLATFORM_SCOREBOARD_TYPES = 5,
};

enum platform_scoreboard_column_type
{
    PSCT_INT = 0,
    PSCT_FLOAT = 1,
    NUM_PLATFORM_SCOREBOARD_COLUMN_TYPES = 2,
};

enum scoreboard_request_type
{
    SRT_DEFAULT = 0,
    SRT_PIVOT_PLAYER = 1,
    SRT_PLAYERS = 2,
    SRT_FRIENDS = 3,
};

// size 0x44
struct scoreboard_column_info
{
    platform_scoreboard_column_type column_type; // 0x00
    char display_name[64]; // 0x04
};

// size 0x8
struct scoreboard_column
{
    platform_scoreboard_column_type column_type; // 0x00
    union
    {
        int i_val; // 0x00
        float f_val; // 0x00
    } column_value; // 0x04
};

// size 0x6C
struct scoreboard_entry
{
    wchar_t player_name[30]; // 0x00
    multi_player_uid player_uid; // 0x3C
    int rank; // 0x48
    scoreboard_column columns[4]; // 0x4C
};

// size 0x11C
struct scoreboard_info
{
    platform_scoreboard_type scoreboard_type; // 0x00
    int scoreboard_index; // 0x04
    platform_game_type_context scoreboard_context; // 0x08
    scoreboard_column_info column_info[4]; // 0x0C
};

// size 0x10
struct scoreboard_results
{
    scoreboard_entry* entries; // 0x00
    int max_entries; // 0x04
    int num_entries; // 0x08
    int total_entries_on_scoreboard; // 0x0C
};

// size 0x18
struct scoreboard_request
{
    void set(const scoreboard_info* infop, int offset, int count, unsigned char* scratchp); // 0x0094A4E0 (:529)
    void set_uids(const scoreboard_info* infop, multi_player_uid* uid_list, int count, unsigned char* scratchp); // 0x0094A510 (:543)

    const scoreboard_info* info; // 0x00
    scoreboard_request_type request_type; // 0x04
    multi_player_uid* player_uids; // 0x08
    int rank_offset; // 0x0C
    int entries_to_retrieve; // 0x10
    unsigned char* scratch; // 0x14
};

// size 0x10
struct skill_info
{
    double skill_ranking; // 0x00
    double skill_uncertainty; // 0x08
};

// size 0x30
struct multi_core_session_info
{
    multi_core_session_info(); // 0x00971750

    kaiko::GameLinkSessionParameters game_link_params; // 0x00
};

// size 0x12
struct pending_invite_info
{
    void clear(); // 0x00970360 (.h:180)
    pending_invite_info(); // 0x00459940

    kaiko::GameLinkInvitationData invitation_data; // 0x00
    bool pending; // 0x10
    bool had_invite_when_entering_multiplayer; // 0x11
};

void multi_core_platform_system_init(); // 0x00972910 (:557)
void multi_core_platform_system_shutdown(); // 0x0094A540 (:581)
void multi_core_platform_system_process(); // 0x009AA0A0 (:586)
void multi_core_platform_system_handshake(); // 0x0094A570 (:594)
void multi_core_platform_system_render(); // 0x0094A580 (:599)
platform_nat_type multi_core_platform_get_nat_type(); // 0x0094A590 (:604)
void multi_core_platform_multi_init(mempool_base& pool); // 0x00972990 (:609)
void multi_core_platform_multi_shutdown(); // 0x0094A5A0 (:616)
void multi_core_platform_set_pending_invite(const pending_invite_info* src_invite_info); // 0x0094A5B0 (:621)
void multi_core_platform_get_pending_invite(pending_invite_info* invite_info); // 0x0094A5F0 (:630)
bool multi_core_platform_has_pending_invite(); // 0x0094A620 (:639)
void multi_core_platform_clear_pending_invite(); // 0x0094A630 (:644)
bool multi_core_platform_get_had_last_invite_on_multiplayer_start(); // 0x0094A660 (:652)
void multi_core_platform_set_had_last_invite_on_multiplayer_start(bool value); // 0x0094A670 (:657)
void multi_core_platform_register_login_changed_callback(void (*login_changed_cb)(bool, bool)); // 0x0094A680 (:663)
void multi_core_platform_login_changed(bool only_changed_connectivity, bool remote_game_session_related); // 0x0094A690 (:668)
void multi_core_platform_set_active_controller(unsigned int controller_index); // 0x0094A6A0 (:675)
unsigned int multi_core_platform_get_active_controller(); // 0x0094A6B0 (:686)
bool multi_core_platform_signin_change_is_pending(); // 0x0094A6C0 (:691)
bool multi_core_platform_get_player_name(wchar_t* player_name, int max_player_name_size); // 0x009729C0 (:696)
const char* multi_core_platform_get_game_type_context_name(platform_game_type_context context); // 0x0094A6D0 (:754)
const char* multi_core_platform_get_game_mode_context_name(platform_game_mode_context context); // 0x0094A6F0 (:760)
void multi_core_platform_set_game_type_context(platform_game_type_context context); // 0x0094A710 (:766)
platform_game_type_context multi_core_platform_get_game_type_context(); // 0x0094A730 (:774)
void multi_core_platform_set_game_mode_context(platform_game_mode_context context); // 0x0094A740 (:779)
platform_game_mode_context multi_core_platform_get_game_mode_context(); // 0x0094A760 (:787)
bool multi_core_platform_is_signed_into_profile(int controller_index); // 0x009729D0 (:792)
bool multi_core_platform_is_connected_to_internet(); // 0x0094A770 (:798)
bool multi_core_platform_is_connected_to_network(); // 0x0094A7C0 (:809)
bool multi_core_platform_ui_is_active(); // 0x0094A7D0 (:818)
void multi_core_platform_show_login_ui(bool allow_non_live); // 0x0094A7E0 (:823)
void multi_core_platform_show_gamercard(const multi_player_uid& player_uid); // 0x0094A7F0 (:828)
bool multi_core_platform_allow_gamer_card_request(); // 0x0094A820 (:833)
void multi_core_platform_show_player_review(const multi_player_uid& player_uid); // 0x0094A830 (:838)
void multi_core_platform_add_friend(const multi_player_uid& player_uid); // 0x0094A840 (:843)
void multi_core_platform_show_friends_ui(); // 0x0094A850 (:850)
bool multi_core_platform_session_start(const multi_core_session_info& session_info); // 0x009AA0B0 (:857)
void multi_core_platform_session_end(); // 0x0094A880 (:862)
bool multi_core_platform_session_has_critical_error(); // 0x0094A8A0 (:867)
void multi_core_platform_register_massage_send_data_callback(bool (*callback)(const unsigned char*, int, unsigned char*, int, int&, unsigned char*, int&)); // 0x0094A8D0 (:873)
bool multi_core_platform_send_packet(const connection_address& target, unsigned char* send_data, unsigned int data_length); // 0x0094A8E0 (:880)
unsigned int multi_core_platform_receive_packet(connection_address& origin, unsigned char* data_buffer, unsigned int capacity); // 0x0098FFF0 (:885)
unsigned int multi_core_platform_get_game_property(platform_game_property pgp); // 0x0094A8F0 (:892)
void multi_core_platform_set_game_property(platform_game_property pgp, unsigned int value); // 0x0094A900 (:897)
unsigned int multi_core_platform_get_user_playlist(); // 0x0094A910 (:903)
void multi_core_platform_set_user_playlist(unsigned int playlist); // 0x0094A920 (:908)
platform_op_status multi_core_platform_get_user_playlist_state(); // 0x0094A930 (:914)
unsigned int multi_core_platform_get_user_dlc_mask(); // 0x0094A940 (:919)
void multi_core_platform_set_user_dlc_mask(unsigned int playlist_entry); // 0x0094A950 (:924)
platform_op_status multi_core_platform_get_user_dlc_mask_state(); // 0x0094A960 (:930)
unsigned int multi_core_platform_get_user_playlist_version(); // 0x0094A970 (:935)
void multi_core_platform_set_user_playlist_version(unsigned int usage); // 0x0094A980 (:940)
platform_op_status multi_core_platform_get_user_playlist_version_state(); // 0x0094A990 (:946)
unsigned int multi_core_platform_get_session_is_started(); // 0x0094A9A0 (:951)
void multi_core_platform_set_session_is_started(unsigned int started); // 0x0094A9B0 (:956)
platform_op_status multi_core_platform_get_session_is_started_state(); // 0x0094A9C0 (:962)
void multi_core_platform_retrieve_remote_file(unsigned char** file_buf, unsigned int* buf_size); // 0x0094A9D0 (:971)
platform_op_status multi_core_platform_get_retrieve_remote_file_state(); // 0x0094A9E0 (:977)
void multi_core_platform_remote_file_process(); // 0x0094A9F0 (:982)
void multi_core_platform_get_player_skill(skill_info* skill, const multi_player_uid& player_uid); // 0x009AA0C0 (:987)
void multi_core_platform_get_aggregate_skill(skill_info& aggregate_skill, skill_info* skills, unsigned int num_skills); // 0x0094AA00 (:992)
bool multi_core_platform_get_large_skill_variance(const multi_player_uid* player_uids, unsigned int num_player_uids); // 0x0094AA10 (:997)
void multi_core_platform_get_logged_in_users(unsigned int* controller_indices, int& num_users, const int max_users); // 0x0094AA20 (:1003)
int multi_core_platform_get_dlc_avail(); // 0x0094AA40 (:1008)
bool multi_core_platform_scoreboard_ready(); // 0x0094AA50 (:1013)
bool multi_core_platform_scoreboard_init(mempool* pool, int max_entries); // 0x00990000 (:1020)
void multi_core_platform_scoreboard_shutdown(); // 0x00972A20 (:1031)
const char* multi_core_platform_scoreboard_get_display_name(platform_scoreboard_type scoreboard_type, platform_game_type_context context); // 0x0094AA60 (:1040)
void multi_core_platform_scoreboard_retrieve_data(scoreboard_results* results, const scoreboard_request* request); // 0x00972A60 (:1079)
platform_op_status multi_core_platform_scoreboard_retrieve_state(); // 0x00972A70 (:1085)
void multi_core_platform_scoreboard_cancel_retrieve(); // 0x0094AAE0 (:1091)
bool multi_core_platform_scoreboard_begin_upload_score(const multi_player_uid& player_uid, unsigned short table_id, scoreboard_column* scores, unsigned int num_scores); // 0x0094AB00 (:1097)
void multi_core_platform_update_achievement_progress(int achievement_number, unsigned int progress); // 0x0094AB20 (:1105)
void multi_core_platform_award_achievement(int achievement_number); // 0x0094AB60 (:1110)
void multi_core_platform_discord_ready(const DiscordUser* request); // 0x0094ABA0 (:1125)
void multi_core_platform_discord_disconnected(int errorCode, const char* message); // 0x0094ABB0 (:1129)
void multi_core_platform_discord_errored(int errorCode, const char* message); // 0x0094ABD0 (:1134)
void multi_core_platform_discord_join_game(const char* joinSecret); // 0x00972A90 (:1139)
void multi_core_platform_discord_spectate_game(const char* spectateSecret); // 0x00972B00 (:1153)
void multi_core_platform_discord_join_request(const DiscordUser* request); // 0x0094ABF0 (:1167)
