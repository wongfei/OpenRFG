// Original: volition/rfg/code/multi/multi_core/include/multi_voice_platform.h
// Implementation: volition/rfg/code/multi/multi_core/generic/multi_voice_generic.cpp
//
// The platform implementation of voice chat (multi_voice_generic.cpp on PC, through GameLink).
#pragma once


class multi_player_uid;

bool multi_voice_platform_init(); // 0x00942A50 (multi_voice_generic.cpp:41)
void multi_voice_platform_shutdown(); // 0x00942A70 (multi_voice_generic.cpp:52)
bool multi_voice_platform_engine_valid(); // 0x00942A90 (multi_voice_generic.cpp:61)
void multi_voice_platform_update_my_headset_status(); // 0x00942AD0 (multi_voice_generic.cpp:72)
void multi_voice_platform_register_local_talker(); // 0x00942B50 (multi_voice_generic.cpp:90)
void multi_voice_platform_unregister_local_talker(); // 0x00942BC0 (multi_voice_generic.cpp:105)
bool multi_voice_platform_talker_is_registered(const multi_player_uid& talker_uid); // 0x00971470 (multi_voice_generic.cpp:122)
void multi_voice_platform_register_remote_talker(multi_player_uid talker_uid); // 0x00942C30 (multi_voice_generic.cpp:134)
void multi_voice_platform_remove_remote_talker(multi_player_uid old_talker); // 0x00942C40 (multi_voice_generic.cpp:140)
bool multi_voice_platform_player_is_muted(const multi_player_uid& uid); // 0x009714C0 (multi_voice_generic.cpp:149)
void multi_voice_platform_mute_player(const multi_player_uid& uid); // 0x00942C50 (multi_voice_generic.cpp:186)
void multi_voice_platform_unmute_player(const multi_player_uid& uid); // 0x00942CF0 (multi_voice_generic.cpp:205)
bool multi_voice_platform_remote_player_is_talking(const multi_player_uid& uid); // 0x00942D90 (multi_voice_generic.cpp:224)
bool multi_voice_platform_local_player_is_talking(const multi_player_uid& uid); // 0x009715B0 (multi_voice_generic.cpp:248)
bool multi_voice_platform_chat_allowed(bool systemlink_or_lan); // 0x00942E50 (multi_voice_generic.cpp:265)
void multi_voice_platform_do_frame(); // 0x00971640 (multi_voice_generic.cpp:273)
