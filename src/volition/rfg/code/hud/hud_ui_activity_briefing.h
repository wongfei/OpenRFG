// Original: volition/rfg/code/hud/hud_ui_activity_briefing.h
// Implementation: volition/rfg/code/hud/hud_ui_activity_briefing.cpp
//
// The activity briefing screen. Placement of activity_briefing_type inferred.
#pragma once

#include "volition/rfg/code/gamewide/gameseq.h"
#include "volition/rfg/code/objects/humans/human_voice/human_voice_internal_shared.h"

class rl_renderer;

enum activity_briefing_type
{
    ABT_DEMOLITIONS_MASTER = 0,
    ABT_LAST = 1,
};

void hud_ui_activity_briefing_init(); // 0x00855FB0 (:383)
void hud_ui_activity_briefing_enter(game_state prev_state); // 0x00845640 (:399)
void hud_ui_activity_briefing_exit(game_state new_state); // 0x008456F0 (:430)
void hud_ui_activity_briefing_process(); // 0x00845750 (:447)
void hud_ui_activity_briefing_handshake(rl_renderer*); // 0x00845940 (:535)
void hud_ui_activity_briefing_render(rl_renderer*); // 0x00845950 (:542)
void hud_ui_activity_briefing_do_frame(); // 0x00845990 (:556)
void hud_ui_activity_briefing_level_shutdown(); // 0x008459D0 (:568)
void hud_ui_activity_briefing_start(activity_briefing_type type, voice_line_handle line_handle); // 0x008459E0 (:574)
void hud_ui_activity_briefing_done(); // 0x00845A00 (:586)
