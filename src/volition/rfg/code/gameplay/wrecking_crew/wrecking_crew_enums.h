// Original: volition/rfg/code/gameplay/wrecking_crew/wrecking_crew_enums.h
//
// The Wrecking Crew enums: the match state machine, the modes, the walker choices, the stress modes of the buildings and
// the result of a menu mouse click. Placement inferred: the PDB records no file for enums.
#pragma once

enum wrecking_crew_states
{
    WCS_NONE = -1,
    WCS_RESTORE_MAP = 0,
    WCS_CHALLENGE_BEGIN = 1,
    WCS_MATCH_BEGIN = 2,
    WCS_CHALLENGE_ROUND_BEGIN = 3,
    WCS_ROUND_BEGIN = 4,
    WCS_PRE_TURN_BREAK = 5,
    WCS_PRE_TURN = 6,
    WCS_PLAYERS_TURN = 7,
    WCS_PRE_BREAK = 8,
    WCS_BREAK = 9,
    WCS_ROUND_END = 10,
    WCS_MATCH_END = 11,
    WCS_EXITING = 12,
    NUM_WRECKING_CREW_STATES = 13,
};

enum wrecking_crew_modes
{
    WCM_NONE = -1,
    WCM_MAYHEM = 0,
    WCM_FALL_GUY = 1,
    WCM_RAMPAGE = 2,
    WCM_TOTAL_CHAOS = 3,
    WCM_WALKER = 4,
    NUM_WRECKING_CREW_MODES = 5,
};

enum wrecking_crew_walker_choices
{
    WCWC_NONE = -1,
    WCWC_FIRST_WALKER = 0,
    WCWC_CIVILIAN = 0,
    WCWC_MINING = 1,
    WCWC_FLAMER = 2,
    NUM_WRECKING_CREW_WALKER_CHOICES = 3,
};

enum wrecking_crew_stress_modes
{
    WCSS_OFF = 0,
    WCSS_LOW = 1,
    WCSS_HIGH = 2,
    NUM_WRECKING_CREW_STRESS_MODES = 3,
};

enum Wc_mouse_click_result
{
    WC_MCR_NONE = -1,
    WC_MCR_SPLASH_CONTINUE = 0,
    WC_MCR_SPLASH_VETO_WEAPON = 1,
    WC_MCR_SPLASH_VETO_PACK = 2,
    WC_MCR_HINT_CONTINUE = 3,
    WC_MCR_HINT_PREV_CAM = 4,
    WC_MCR_HINT_NEXT_CAM = 5,
};
