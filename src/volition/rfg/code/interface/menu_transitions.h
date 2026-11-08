// Original: volition/rfg/code/interface/menu_transitions.h
// Implementation: volition/rfg/code/interface/menu_transitions.cpp
//
// Transitions between menus (the transition effects and their functions). Placement of transition_fx inferred.
#pragma once

enum transition_fx
{
    TFX_NONE = -1,
    TFX_SCREEN_FADE = 0,
    TFX_CROSS_FADE = 1,
    TFX_WIPE_RIGHT = 2,
    TFX_WIPE_LEFT = 3,
    NUM_TFX_MODES = 4,
};

float transition_fade_in_pct(); // 0x00889FD0 (:363)
float transition_fade_out_pct(); // 0x00889FF0 (:371)
void transition_wipe_in(); // 0x008AC3C0 (:379)
void transition_wipe_out(); // 0x008AC440 (:388)
bool transition_in_valid(transition_fx fx); // 0x0088A010 (:397)
bool transition_out_valid(transition_fx fx); // 0x0088A030 (:408)
bool transition_in(transition_fx fx, bool* finished, bool out_in_progress); // 0x0088A050 (:419)
bool transition_out(transition_fx fx, bool* finished); // 0x0088A070 (:424)
void transition_reset_in_progresses(); // 0x0088A090 (:429)
