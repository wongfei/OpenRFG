// Original: volition/rfg/code/interface/option_menus/sony_options.h
// Implementation: volition/rfg/code/interface/option_menus/sony_options.cpp
//
// The Sony controller options. Placement of sony_options_mode inferred.
#pragma once

#include "volition/rfg/code/gamewide/gameseq.h"

class multi_player_uid;
class rl_renderer;

enum sony_options_mode
{
    SONY_OPTIONS_LOGIN = 0,
    SONY_OPTIONS_SAVE = 1,
    SONY_OPTIONS_LOAD = 2,
    SONY_OPTIONS_DELETE = 3,
    SONY_OPTIONS_FRIENDS_UI = 4,
    SONY_OPTIONS_ADD_FRIEND_UI = 5,
    SONY_OPTIONS_APP_DEFINED_MSG = 6,
    SONY_OPTIONS_READ_MESSAGES = 7,
    NUM_SONY_OPTIONS_MODES = 8,
};

void sony_options_init(); // 0x00918CE0 (:99)
void sony_options_enter(game_state prev_state); // 0x0090FDF0 (:113)
void sony_options_process(); // 0x009165F0 (:155)
void sony_options_handshake(rl_renderer*); // 0x008930D0 (:277)
void sony_options_render(rl_renderer*); // 0x00904670 (:283)
void sony_options_exit(game_state new_state); // 0x008F4280 (:289)
void sony_options_singal_exit(); // 0x008930E0 (:313)
void sony_options_signal_sending_invite(bool value); // 0x008930F0 (:318)
void sony_options_start(sony_options_mode mode); // 0x00893100 (:323)
void sony_options_add_friend(const multi_player_uid& player_uid); // 0x00893120 (:329)
