// Original: volition/rfg/code/gameplay/gameflow/gfm_control_morale.cpp
//
// gameflow.md GF2. District EDF control and civilian morale: values, maxima and display values per district, the temporary
// morale of missions and activities, the morale offset, and the changes made by kills and propaganda.

#include "volition/rfg/code/gameplay/gameflow/gfm_control_morale.h"

#include "volition/audio_lib/code/audiolib_api.h"
#include "volition/render_lib/code/gr/gr.h"
#include "volition/render_lib/code/gr/grd.h"
#include "volition/rfg/code/ai/ai_system.h"
#include "volition/rfg/code/ai/vehicle/vehicle_ai_actions.h"
#include "volition/rfg/code/event_logging/definitions_events/events.h"
#include "volition/rfg/code/event_logging/gamelog.h"
#include "volition/rfg/code/gameplay/activities/activity_internal.h"
#include "volition/rfg/code/gameplay/difficulty.h"
#include "volition/rfg/code/gameplay/missions.h"
#include "volition/rfg/code/gameplay/object_district.h"
#include "volition/rfg/code/gameplay/wrecking_crew/wrecking_crew_enums.h"
#include "volition/rfg/code/gamewide/downloadable_content.h"
#include "volition/rfg/code/gamewide/gameaudio.h"
#include "volition/rfg/code/gamewide/stats.h"
#include "volition/rfg/code/gamewide/tweak_table.h"
#include "volition/rfg/code/hud/hud_message.h"
#include "volition/rfg/code/interface/ui_document_hud.h"
#include "volition/rfg/code/interface/ui_document_hud_tool_tips.h"
#include "volition/rfg/code/level/world.h"
#include "volition/rfg/code/multi/multi_core/include/multi.h"
#include "volition/rfg/code/objects/humans/npc.h"
#include "volition/rfg/code/objects/humans/player.h"
#include "volition/rfg/code/scripting/action_script/global_event_data.h"
#include "volition/rfg/code/scripting/action_script/global_event_manager.h"
#include "volition/vlib/code/math/floating.h"
#include "volition/vlib/code/os/vconsole.h"
#include "volition/vlib/code/util/vfont.h"

#include <math.h>
#include <stdio.h>

extern wrecking_crew_states Wrecking_crew_state; // 0x01651888 (wrecking_crew.cpp)
extern activity_base* All_activities[17]; // 0x01651248 (gameplay.cpp)

// Magic numbers: the fixed slots of All_activities that matter here.
static const int ACTIVITY_SLOT_RAID_DEFEND = 8;
static const int ACTIVITY_SLOT_RAID_ATTACK = 9;
static const int ACTIVITY_SLOT_HOUSE_ARREST = 10;
// Magic number: the morale or control display of a district stays up 5 seconds for that district.
static const int CONTROL_MORALE_DISPLAY_MS = 5000;

extern float Player_kills_hostage_morale_penalty; // 0x01651334 (activity_house_arrest.cpp)
static float Max_morale_lost_dist = 30.0f; // 0x01651450
static float Temporary_morale_max = 1000.0f; // 0x01651454
static unsigned int Control_morale_last_district = 0xFFFFFFFF; // 0x01651458
static timestamp Control_morale_district_timer; // 0x0165769C
static bool Use_temporary_morale; // 0x02229622
static bool Morale_debug; // 0x02229623, nothing writes it
static float Player_killed_civilian_morale_delta; // 0x02229628
static float Player_killed_guerilla_morale_delta; // 0x0222962C
static float Player_killed_guerilla_raid_morale_delta; // 0x02229630
static float EDF_killed_civilian_morale_delta; // 0x02229634
static float EDF_killed_guerilla_morale_delta; // 0x02229638
static float Propaganda_destroyed_morale_delta; // 0x0222963C
static float Temporary_morale; // 0x02229640
static float Morale_offset; // 0x02229644

static tweak_table_item Tweak_table_item_Player_killed_civilian_morale_delta("Player_killed_civilian_morale_delta", &Player_killed_civilian_morale_delta);
static tweak_table_item Tweak_table_item_Player_killed_guerilla_morale_delta("Player_killed_guerilla_morale_delta", &Player_killed_guerilla_morale_delta);
static tweak_table_item Tweak_table_item_Player_killed_guerilla_raid_morale_delta("Player_killed_guerilla_raid_morale_delta", &Player_killed_guerilla_raid_morale_delta);
static tweak_table_item Tweak_table_item_EDF_killed_civilian_morale_delta("EDF_killed_civilian_morale_delta", &EDF_killed_civilian_morale_delta);
static tweak_table_item Tweak_table_item_EDF_killed_guerilla_morale_delta("EDF_killed_guerilla_morale_delta", &EDF_killed_guerilla_morale_delta);
static tweak_table_item Tweak_table_item_Propaganda_destroyed_morale_delta("Propaganda_destroyed_morale_delta", &Propaganda_destroyed_morale_delta);
static tweak_table_item Tweak_table_item_Max_morale_lost_dist("Max_morale_lost_dist", &Max_morale_lost_dist);

// 0x00753AC0 (global_event_data.h:108)
edf_control_lowered_event::edf_control_lowered_event()
    : event_data(EVENT_EDF_CONTROL_LOWERED)
{
}

// 0x00753AD0 (global_event_data.h:113)
civilian_morale_lowered_event::civilian_morale_lowered_event()
    : event_data(EVENT_CIVILIAN_MORALE_LOWERED)
{
}

// 0x00753AE0 (global_event_data.h:118)
civilian_morale_raised_event::civilian_morale_raised_event()
    : event_data(EVENT_CIVILIAN_MORALE_RAISED)
{
}

// 0x007AFC80 (:129)
void unused_dcf_civilian_morale_change()
{
    const bool help = Console_help != 0;
    if (Console_command != 0)
    {
        if (!console_get_arg(0x10, 1))
        {
            console_output("Usage: civilian_morale_change <delta_morale>", nullptr, false);
            return;
        }
        gfm_change_district_morale(Console_arg_float, nullptr, nullptr, nullptr);
        char text[256];
        sprintf(text, "New Morale Value: %i", (int)gfm_get_district_morale(nullptr));
        console_output(text, nullptr, false);
    }
    if (help)
    {
        console_output("Usage: civilian_morale_change <delta_morale>", nullptr, false);
    }
}

// 0x007AAC40 (:152)
void unused_dcf_set_civilian_morale()
{
    const bool help = Console_help != 0;
    if (Console_command != 0)
    {
        if (!console_get_arg(0x10, 1))
        {
            console_output("Usage: set_civilian_morale <new value>", nullptr, false);
            return;
        }
        gfm_set_district_morale(Console_arg_float, nullptr);
        char text[256];
        sprintf(text, "New Morale Value: %i", (int)gfm_get_district_morale(nullptr));
        console_output(text, nullptr, false);
    }
    if (help)
    {
        console_output("Usage: set_civilian_morale <new value>", nullptr, false);
    }
}

// 0x007B4700 (:175)
void unused_dcf_edf_control_change()
{
    const bool help = Console_help != 0;
    if (Console_command != 0)
    {
        if (!console_get_arg(0x10, 1))
        {
            console_output("Usage: edf_control_change <delta_control>", nullptr, false);
            return;
        }
        gfm_change_edf_control(Console_arg_float, nullptr, nullptr, nullptr);
    }
    if (help)
    {
        console_output("Usage: edf_control_change <delta_control>", nullptr, false);
    }
}

// 0x0075DA50 (:199). Volition's empty body.
void gfm_control_morale_init()
{
}

// 0x0075DA60 (:204)
void gfm_control_morale_level_init_pre()
{
    Use_temporary_morale = false;
    Temporary_morale = 0.0f;
    Temporary_morale_max = 1000.0f;
    Morale_offset = 0.0f;
    Control_morale_last_district = 0xFFFFFFFF;
    Control_morale_district_timer.invalidate();
}

// 0x0075DAA0 (:216). Volition's empty body.
void gfm_control_morale_do_frame()
{
}

// 0x007AFD90 (:246)
void gfm_control_morale_debug_render()
{
    grd_set_color(0, 255, 255, 255);
    if (!Morale_debug)
    {
        return;
    }
    char text[256];
    // Deviation from rfg.exe: a stack buffer, where it prints into its thread-local one.
    sprintf(text, "Effective Morale: %d / %d", (int)gfm_get_effective_morale(), (int)gfm_get_effective_morale_max());
    int w;
    int h;
    font_get_string_size(w, h, text, -1, -1, false);
    grd_string(grd_safe_area_right() - w, grd_safe_area_top(), text, GM_FILTER);
    const district* dp = district_local_player();
    const int morale_max = dp != nullptr ? (int)dp->get_morale_max() : 0;
    sprintf(text, "District Morale: %d / %d", (int)gfm_get_district_morale(nullptr), morale_max);
    font_get_string_size(w, h, text, -1, -1, false);
    grd_string(grd_safe_area_right() - w, grd_safe_area_top() + h, text, GM_FILTER);
}

// 0x007AADF0 (:286)
float gfm_get_effective_morale()
{
    float value;
    float max_value;
    if (Use_temporary_morale)
    {
        value = Temporary_morale + Morale_offset;
        max_value = Temporary_morale_max;
    }
    else
    {
        max_value = gfm_get_district_morale_max(nullptr);
        value = Morale_offset + gfm_get_district_morale(nullptr);
    }
    if (value - 0.0f < 0.0f)
    {
        value = 0.0f;
    }
    return value - max_value < 0.0f ? value : max_value;
}

// 0x007AAF80 (:294)
float gfm_get_effective_morale_max()
{
    return Use_temporary_morale ? Temporary_morale_max : gfm_get_district_morale_max(nullptr);
}

// 0x007AB000 (:302)
float gfm_get_effective_morale_percent()
{
    const float max_value = gfm_get_effective_morale_max();
    return gfm_get_effective_morale() / max_value;
}

// 0x007A4920 (:307)
void gfm_set_district_morale_max(float max_morale, district* dp)
{
    if (dp == nullptr)
    {
        dp = district_local_player();
        if (dp == nullptr)
        {
            return;
        }
    }
    dp->set_morale_max(max_morale);
}

// 0x007A49F0 (:318)
void gfm_set_district_morale(float new_morale, district* dp)
{
    if (dp == nullptr)
    {
        dp = district_local_player();
        if (dp == nullptr)
        {
            return;
        }
    }
    dp->set_morale(new_morale);
}

// 0x007AB070 (:329)
void gfm_change_district_morale(float delta_morale, district* dp, const char* reason, const wchar_t* icon)
{
    if (dlc_get_morale_disabled())
    {
        return;
    }
    if (reason != nullptr)
    {
        wchar_t text[128];
        hud_message_get_icon_with_localized_text(text, 128, icon, reason);
        hud_message_add_minor(text, -1.0f, false);
    }
    if (delta_morale == 0.0f)
    {
        return;
    }
    if (dp == nullptr)
    {
        dp = district_local_player();
        if (dp == nullptr)
        {
            return;
        }
    }
    // The change is a whole number, at least 1 either way.
    const float scaled = difficulty_get_value(0.0f <= delta_morale ? DIFFICULTY_MORALE_BOOSTS : DIFFICULTY_MORALE_PENALITIES, NUM_DIFFICULTY_LEVELS) * delta_morale;
    const float sign = scaled < 0.0f ? -1.0f : 1.0f;
    float magnitude = (float)fabs((float)(int)scaled);
    if (0.0f <= 1.0f - magnitude)
    {
        magnitude = 1.0f;
    }
    const float change = magnitude * sign;

    float new_value = gfm_get_district_morale(dp) + change;
    if (new_value - 0.0f < 0.0f)
    {
        new_value = 0.0f;
    }
    const float max_value = gfm_get_district_morale_max(dp);
    if (0.0f <= new_value - max_value)
    {
        new_value = max_value;
    }
    const float applied = new_value - gfm_get_district_morale(dp);
    if (applied == 0.0f)
    {
        const float percent = gfm_get_district_morale_percent(dp);
        hud_ui_change_morale(gfm_get_district_morale_percent(dp), percent, change, dp->get_handle());
        return;
    }

    const float old_percent = gfm_get_district_morale_percent(dp);
    gfm_set_district_morale(gfm_get_district_morale(dp) + applied, dp);
    log_event_morale_change(gamelog_get_player_checksum(), gamelog_get_logging_time(), (char)fl_round(gfm_get_district_morale(dp)), gamelog_get_time_played(), gamelog_get_pct_complete(), gamelog_get_locale(), gamelog_get_difficulty(), gamelog_get_time_played(), gamelog_get_human_pos(player_get_local_player()));
    const float new_percent = gfm_get_district_morale_percent(dp);
    if (applied <= 0.0f)
    {
        civilian_morale_lowered_event data;
        data.prev_percent = old_percent;
        data.percent = new_percent;
        gem_post_event(data);
    }
    else
    {
        civilian_morale_raised_event data;
        data.prev_percent = old_percent;
        data.percent = new_percent;
        gem_post_event(data);
        if (!ui_tool_tips_tip_is_queued("Guerrilla_Action_Complete") && Wrecking_crew_state == WCS_NONE)
        {
            ui_tool_tips_add_tip("Morale", 500, nullptr);
        }
    }
    if (!Control_morale_district_timer.valid() || Control_morale_district_timer.elapsed())
    {
        Control_morale_last_district = dp->get_handle();
        Control_morale_district_timer.set(CONTROL_MORALE_DISPLAY_MS);
    }
    else if (dp->get_handle() != Control_morale_last_district)
    {
        return;
    }
    hud_ui_change_morale(new_percent, old_percent, change, dp->get_handle());
}

// 0x007AB490 (:422)
void gfm_propaganda_morale_change(district* dp)
{
    audiolib_cue_play(Ga_cue_ids.popup_propeganda, nullptr, nullptr);
    gfm_change_district_morale(Propaganda_destroyed_morale_delta, dp, "HM_PROPAGANDA_DESTROYED", L"ui_hud_diversion_icon_propaganda");
}

// 0x007AB4D0 (:429)
int gfm_get_district_morale_display_value(district* dp)
{
    const float value = gfm_get_district_morale(dp);
    if (1.0f <= value)
    {
        return (int)floor((double)value);
    }
    return 0.0f < value ? 1 : 0;
}

// 0x007A4AA0 (:443)
float gfm_get_district_morale(district* dp)
{
    if (dp == nullptr)
    {
        dp = district_local_player();
        if (dp == nullptr)
        {
            return 0.0f;
        }
    }
    return dp->get_morale();
}

// 0x007AB570 (:454)
int gfm_get_district_morale_max_display_value(district* dp)
{
    return (int)floor((double)gfm_get_district_morale_max(dp));
}

// 0x007A4B30 (:459)
float gfm_get_district_morale_max(district* dp)
{
    if (dp == nullptr)
    {
        dp = district_local_player();
        if (dp == nullptr)
        {
            return 0.0f;
        }
    }
    return dp->get_morale_max();
}

// 0x007A4B70 (:470)
float gfm_get_district_morale_percent(district* dp)
{
    const float max_value = gfm_get_district_morale_max(dp);
    return max_value == 0.0f ? 0.0f : gfm_get_district_morale(dp) / max_value;
}

// 0x0075DAB0 (:482)
void gfm_start_temporary_morale()
{
    Use_temporary_morale = true;
}

// 0x0075DAC0 (:487)
void gfm_end_temporary_morale()
{
    Use_temporary_morale = false;
}

// 0x0075DAD0 (:492)
void gfm_set_temporary_morale(float new_morale)
{
    if (!Use_temporary_morale)
    {
        return;
    }
    if (new_morale - 0.0f < 0.0f)
    {
        new_morale = 0.0f;
    }
    Temporary_morale = 0.0f <= new_morale - Temporary_morale_max ? Temporary_morale_max : new_morale;
}

// 0x00773840 (:501)
void gfm_change_temporary_morale(float delta_morale)
{
    gfm_set_temporary_morale(Temporary_morale + delta_morale);
}

// 0x0075DB30 (:506)
bool gfm_use_temporary_morale()
{
    return Use_temporary_morale;
}

// 0x0075DB40 (:511)
float gfm_get_temporary_morale()
{
    return Temporary_morale;
}

// 0x007A4BF0 (:516)
void gfm_set_temporary_morale_max(float new_morale_max)
{
    if (Use_temporary_morale && 0.0f < new_morale_max)
    {
        Temporary_morale_max = new_morale_max;
        gfm_set_temporary_morale(gfm_get_district_morale(nullptr));
    }
}

// 0x0075DB50 (:529)
void gfm_set_morale_offset(float new_offset)
{
    Morale_offset = new_offset;
}

// 0x0075DB60 (:534)
void gfm_change_morale_offset(float delta_morale)
{
    Morale_offset = Morale_offset + delta_morale;
}

// 0x0075DB90 (:539)
float gfm_get_morale_offset()
{
    return Morale_offset;
}

// 0x007AFF20 (:544)
void gfm_set_edf_control_max(float max_control, district* dp)
{
    if (dp == nullptr)
    {
        dp = district_local_player();
        if (dp == nullptr)
        {
            return;
        }
    }
    dp->set_control_max(max_control);
}

// 0x007AFFE0 (:555)
bool gfm_set_edf_control(float new_control, district* dp)
{
    if (dp == nullptr)
    {
        dp = district_local_player();
        if (dp == nullptr)
        {
            return false;
        }
    }
    dp->set_control(new_control);
    return true;
}

// 0x007B00B0 (:569)
void gfm_change_edf_control(float delta_control, district* dp, const char* reason, const wchar_t* icon)
{
    if (Multiplayer || Wrecking_crew_state != WCS_NONE || delta_control == 0.0f)
    {
        return;
    }
    const int change = fl_round(difficulty_get_value(DIFFICULTY_CONTROL_PARKER, NUM_DIFFICULTY_LEVELS) * delta_control);
    if (dp == nullptr)
    {
        dp = district_local_player();
        if (dp == nullptr)
        {
            return;
        }
    }
    float new_value = gfm_get_edf_control(dp) + (float)change;
    if (new_value - 0.0f < 0.0f)
    {
        new_value = 0.0f;
    }
    const float max_value = gfm_get_edf_control_max(dp);
    if (0.0f <= new_value - max_value)
    {
        new_value = max_value;
    }
    const float applied = new_value - gfm_get_edf_control(dp);
    float delta_percent = 0.0f;
    if (applied != 0.0f)
    {
        // Original quirk kept: the event says "lowered" whichever way the control moved.
        edf_control_lowered_event data;
        data.prev_percent = gfm_get_edf_control_percent(dp);
        const float old_percent = gfm_get_edf_control_percent(dp);
        if (reason != nullptr)
        {
            wchar_t text[128];
            hud_message_get_icon_with_localized_text(text, 128, icon, reason);
            hud_message_add_minor(text, -1.0f, false);
        }
        gfm_set_edf_control(gfm_get_edf_control(dp) + applied, dp);
        log_event_control_change(gamelog_get_player_checksum(), gamelog_get_logging_time(), (char)fl_round(gfm_get_edf_control_percent(dp) * 100.0f), gamelog_get_time_played(), gamelog_get_pct_complete(), gamelog_get_locale(), gamelog_get_difficulty(), gamelog_get_time_played(), gamelog_get_human_pos(player_get_local_player()));
        data.percent = gfm_get_edf_control_percent(dp);
        gem_post_event(data);
        if (!Control_morale_district_timer.valid() || Control_morale_district_timer.elapsed())
        {
            Control_morale_last_district = dp->get_handle();
            Control_morale_district_timer.set(CONTROL_MORALE_DISPLAY_MS);
        }
        else if (dp->get_handle() != Control_morale_last_district)
        {
            return;
        }
        // Magic number: achievement 0x3A, for no EDF control left in a district of DLC bundle 1.
        if (World.dlc_bundle_id == 1 && gfm_get_edf_control(dp) <= 0.0f)
        {
            stats_award_achievement(0x3A);
        }
        // Magic number: the tip shows once control is below 200.
        if (gfm_get_edf_control(dp) < 200.0f && !ui_tool_tips_tip_is_queued("Guerrilla_Action_Complete"))
        {
            ui_tool_tips_add_tip("EDF_Control", 1500, nullptr);
        }
        delta_percent = gfm_get_edf_control_percent(dp) - old_percent;
    }
    hud_ui_change_control(gfm_get_edf_control_display_value(dp), gfm_get_edf_control_percent(dp), (int)(float)change, delta_percent, dp->get_handle(), false, 0);
}

// 0x007AB620 (:647)
int gfm_get_edf_control_display_value(district* dp)
{
    if (dp == nullptr)
    {
        dp = district_local_player();
        if (dp == nullptr)
        {
            return 0;
        }
    }
    const float value = dp->get_control();
    if (1.0f <= value)
    {
        return (int)floor((double)value);
    }
    return value <= 0.0f ? 0 : 1;
}

// 0x007A4CE0 (:661)
float gfm_get_edf_control(const district* dp)
{
    if (dp == nullptr)
    {
        dp = district_local_player();
        if (dp == nullptr)
        {
            return 0.0f;
        }
    }
    return dp->get_control();
}

// 0x007A4D20 (:672)
float gfm_get_edf_control_max(const district* dp)
{
    if (dp == nullptr)
    {
        dp = district_local_player();
        if (dp == nullptr)
        {
            return 0.0f;
        }
    }
    return dp->get_control_max();
}

// 0x007A4D60 (:683)
int gfm_get_edf_control_max_display_value(district* dp)
{
    return (int)floor((double)gfm_get_edf_control_max(dp));
}

// 0x007A4E10 (:688)
float gfm_get_edf_control_percent(const district* dp)
{
    const float max_value = gfm_get_edf_control_max(dp);
    return max_value == 0.0f ? 0.0f : gfm_get_edf_control(dp) / max_value;
}

// 0x007A4EE0 (:699)
float gfm_get_edf_control_gamewide_percent()
{
    float control = 0.0f;
    float control_max = 0.0f;
    const int num = World.get_num_objects<district>();
    for (int i = 0; i < num; i++)
    {
        const district* dp = World.get_object<district>(i);
        control = gfm_get_edf_control(dp) + control;
        control_max = gfm_get_edf_control_max(dp) + control_max;
    }
    return control / control_max;
}

// 0x007AB6E0 (:716)
void gfm_control_morale_human_killed(human* victim, human* attacker)
{
    if (victim == nullptr || attacker == nullptr || Multiplayer || object_is_player(victim) || mission_get_mode() == MISSION_MODE_TUTORIAL)
    {
        return;
    }
    const human_teams team = victim->current_team;
    district* dp = district_local_player();
    // Fodder dies by design.
    if (victim->get_obj_type() == OT_HUMAN && victim->get_sub_type() == 2)
    {
        const ai_system* ai = static_cast<npc*>(victim)->get_ai();
        if (ai != nullptr && ai->planner.get_current_action() == fodder_action::get_instance())
        {
            return;
        }
    }
    if (object_is_player(attacker))
    {
        const human_flags flags = victim->hflags;
        if (flags.activity_ha_hostage != 0)
        {
            activity_base* house_arrest = !Multiplayer ? All_activities[ACTIVITY_SLOT_HOUSE_ARREST] : nullptr;
            if (house_arrest != nullptr && (house_arrest->is_active() || house_arrest->was_initialized()))
            {
                return;
            }
            gfm_change_district_morale(-Player_kills_hostage_morale_penalty, dp, "HM_TRUST_BETRAYED", L"ui_hud_weapon_icon_sm_skull");
            return;
        }
        if (team == HUMAN_TEAM_CIVILIAN)
        {
            gfm_change_district_morale(Player_killed_civilian_morale_delta, dp, "HM_TRUST_BETRAYED", L"ui_hud_weapon_icon_sm_skull");
            ui_tool_tips_add_tip("Civilian_Death", 1000, nullptr);
            return;
        }
        if (team == HUMAN_TEAM_GUERILLA)
        {
            const bool raid = flags.activity_raid_required != 0 || flags.activity_raid_optional != 0;
            gfm_change_district_morale(raid ? Player_killed_guerilla_raid_morale_delta : Player_killed_guerilla_morale_delta, dp, "HM_TRUST_BETRAYED", L"ui_hud_weapon_icon_sm_skull");
        }
        return;
    }
    if (attacker->current_team != HUMAN_TEAM_EDF)
    {
        return;
    }
    if (!Multiplayer)
    {
        activity_base* attack = All_activities[ACTIVITY_SLOT_RAID_ATTACK];
        if (attack != nullptr && attack->is_active())
        {
            return;
        }
        activity_base* defend = All_activities[ACTIVITY_SLOT_RAID_DEFEND];
        if (defend != nullptr && defend->is_active())
        {
            return;
        }
    }
    // Magic: with no local player the distance is 0.
    const player* pp = player_get_local_player();
    const float dist = pp != nullptr ? vec_dist_squared(pp->pos, victim->pos) : 0.0f;
    if (Max_morale_lost_dist * Max_morale_lost_dist <= dist)
    {
        return;
    }
    if (team == HUMAN_TEAM_CIVILIAN)
    {
        gfm_change_district_morale(EDF_killed_civilian_morale_delta, dp, "HM_CIVILIAN_KILLED", L"ui_hud_weapon_icon_sm_skull");
        ui_tool_tips_add_tip("Civilian_Death", 1000, nullptr);
        return;
    }
    if (team == HUMAN_TEAM_GUERILLA)
    {
        gfm_change_district_morale(EDF_killed_guerilla_morale_delta, dp, "HM_GUERRILLA_KILLED", L"ui_hud_weapon_icon_sm_skull");
    }
}
