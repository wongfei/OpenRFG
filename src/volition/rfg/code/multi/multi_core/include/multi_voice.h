// Original: volition/rfg/code/multi/multi_core/include/multi_voice.h
// Implementation: volition/rfg/code/multi/multi_core/global/multi_voice.cpp
//
// Voice chat: the talkers (local and remote players), their talk state and muting. Placement of the voice types inferred.
#pragma once

#include "volition/rfg/code/multi/multi_core/include/multi_player_uid.h"
#include "volition/vlib/code/time/timestamp.h"

class multi_connection;
struct sender;

enum voice_update_type
{
    VI_HEADSET_UPDATE = 0,
    VI_MUTE_UPDATE = 1,
};

enum multi_voice_talk_state
{
    MVTS_MUTED = 0,
    MVTS_NOT_TALKING = 1,
    MVTS_TALKING = 2,
    MVTS_NO_HEADSET = 3,
};

// size 0x28
struct voice_talker
{
    voice_talker(); // 0x0094B9E0

    multi_player_uid player_uid; // 0x00
    bool has_headset; // 0x0B
    bool has_headset_dirty; // 0x0C
    bool newly_added; // 0x0D
    timestamp last_voice_recvd; // 0x10
    multi_connection* connections[4]; // 0x14
    int num_connections; // 0x24
};

// size 0xC
struct local_player_voice_info
{
    local_player_voice_info(); // 0x0094B9F0

    bool registered_for_voice; // 0x00
    bool has_headset; // 0x01
    bool has_headset_dirty; // 0x02
    unsigned int controller_index; // 0x04
    timestamp update_check_time; // 0x08
};

extern local_player_voice_info Local_player_voice_info; // 0x01691D54

voice_talker* find_talker_registration_by_uid(const multi_player_uid& uid); // 0x0094BC40 (:402)
void multi_voice_init(bool game_is_syslink_or_lan); // 0x00990FD0 (:413)
void multi_voice_shutdown(); // 0x00974240 (:445)
void multi_voice_set_should_send_callback(bool (*callback)(multi_connection*), int session_type); // 0x0094BC90 (:455)
void multi_voice_do_frame(); // 0x009B8BB0 (:467)
void multi_voice_register_talker(multi_connection* talker_cp); // 0x009742A0 (:491)
void multi_voice_remove_talker(multi_connection* talker_cp); // 0x00974390 (:553)
void multi_voice_login_changed(); // 0x0094BCC0 (:618)
void multi_voice_login_something(); // 0x00991070 (:622)
bool multi_voice_is_headset_present(const multi_player_uid& uid); // 0x0094BCD0 (:632)
multi_voice_talk_state multi_voice_get_talk_state(const multi_player_uid& uid); // 0x00974480 (:651)
multi_voice_talk_state multi_voice_get_icon_talk_state(const multi_player_uid& uid); // 0x00974540 (:674)
void multi_voice_signal_to_send_self_update(); // 0x0094BD10 (:694)
void multi_process_game_voice_info_update(unsigned char* packet_data, const sender& from); // 0x00974570 (:699)
bool multi_voice_chat_allowed(bool lan_game); // 0x0094BD20 (:747)
bool multi_voice_push_to_talk_enabled(); // 0x0094BD70 (:755)
void multi_voice_push_to_talk_set_enabled(bool enabled); // 0x0094BD80 (:760)
void multi_voice_set_original_player_mute_state(const multi_player_uid& player_uid, bool isMuted); // 0x009AABE0 (:766)
void multi_voice_handle_voice_chat_at_game_start_and_end(bool isStarting); // 0x009AAC80 (:796)
