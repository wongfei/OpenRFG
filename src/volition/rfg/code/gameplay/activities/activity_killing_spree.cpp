// Original: volition/rfg/code/gameplay/activities/activity_killing_spree.cpp

#include "volition/rfg/code/gameplay/activities/activity_killing_spree_internal.h"

#include "volition/audio_lib/code/audiolib_api.h"
#include "volition/rfg/code/gameplay/activities/activity_register_damage.h"
#include "volition/rfg/code/gameplay/gameflow/gfm_control_morale.h"
#include "volition/rfg/code/gameplay/wrecking_crew/wrecking_crew_enums.h"
#include "volition/rfg/code/gamewide/gameaudio.h"
#include "volition/rfg/code/gamewide/stats.h"
#include "volition/rfg/code/gamewide/tweak_table.h"
#include "volition/rfg/code/interface/ui_document_hud_tool_tips.h"
#include "volition/rfg/code/multi/multi_core/include/multi.h"
#include "volition/rfg/code/nvidia_highlights.h"
#include "volition/rfg/code/objects/humans/human.h"
#include "volition/rfg/code/objects/humans/human_combat.h"
#include "volition/rfg/code/objects/humans/human_teams.h"
#include "volition/rfg/code/objects/humans/player.h"
#include "volition/vlib/code/util/string_util.h"

#include "keen/base/systemtimer.hpp"

extern wrecking_crew_states Wrecking_crew_state; // (wrecking crew)

activity_killing_spree_data Killing_spree_data; // 0x016532B0

static int Killing_spree_initial_kills; // 0x02225810, from the tweak table
static int Killing_spree_initial_time_ms; // 0x02225AE0, from the tweak table
static int Killing_spree_additional_period_ms; // 0x022267D0, from the tweak table
static int Killing_spree_start_period_ms; // 0x022267D4, from the tweak table
static unsigned int s_timeFirstKillAdded; // 0x02225AEC
static tweak_table_item Tweak_table_item_Killing_spree_initial_kills("Killing_spree_initial_kills", &Killing_spree_initial_kills);
static tweak_table_item Tweak_table_item_Killing_spree_initial_time_ms("Killing_spree_initial_time_ms", &Killing_spree_initial_time_ms);
static tweak_table_item Tweak_table_item_Killing_spree_additional_period_ms("Killing_spree_additional_period_ms", &Killing_spree_additional_period_ms);
// Registered in rfg.exe, read by nothing there.
static float Killing_spree_base_morale_delta; // 0x022258B4
static tweak_table_item Tweak_table_item_Killing_spree_base_morale_delta("Killing_spree_base_morale_delta", &Killing_spree_base_morale_delta); // 0x02246BA4
static tweak_table_item Tweak_table_item_Killing_spree_start_period_ms("Killing_spree_start_period_ms", &Killing_spree_start_period_ms);

// 0x00754490
activity_ks_data::activity_ks_data()
    : kill_count(0)
{
}

// 0x0077E6E0 (.h:20)
activity_killing_spree_data::activity_killing_spree_data()
    : activity_base_free(ACTIVITY_MODE_KILLING_SPREE)
{
}

// 0x00769110
activity_killing_spree_data::~activity_killing_spree_data()
{
}

// 0x0076E8A0 (:72)
void activity_killing_spree_data::level_init()
{
    activity_base_free::level_init(); // rfg.exe inlines it
    reset();
}

// 0x00758C70 (:79)
void activity_killing_spree_data::reset()
{
    activity_data.kill_count = 0;
    activity_data.expiration_time.invalidate();
}

// 0x00758C90 (:86)
const char* activity_killing_spree_data::get_title() const
{
    return "ACT_KILLING_SPREE";
}

// 0x00758CA0 (:99)
const char* activity_killing_spree_data::get_icon_image_name() const
{
    return "ui_hud_diversion_icon_killspree";
}

// 0x00758CB0 (:105)
bool activity_killing_spree_data::get_ui_display_info(char* value_text, int& time_til_end, int buffer_size) const
{
    if (value_text == nullptr)
    {
        return false;
    }
    time_til_end = activity_data.expiration_time.time_until();
    _snprintf_safe(value_text, buffer_size, "%i", activity_data.kill_count);
    return true;
}

// 0x00758D00 (:116)
bool activity_killing_spree_data::render_hud(int x_pos, int y_pos)
{
    return false;
}

// 0x00758D10 (:144)
void activity_killing_spree_data::activity_do_frame_active()
{
    if (activity_data.expiration_time.valid() && activity_data.expiration_time.elapsed())
    {
        activity_end();
    }
}

// 0x007AD9C0 (:155)
void activity_killing_spree_data::start_killing_spree()
{
    if (!activity_base_free::activity_start())
    {
        return;
    }
    activity_data.expiration_time.set(Killing_spree_start_period_ms);
    audiolib_cue_play(Ga_cue_ids.popup_killing_spree, nullptr, nullptr);
    if (!Multiplayer && Wrecking_crew_state == WCS_NONE)
    {
        ui_tool_tips_add_tip("Killing_Spree", 500, nullptr); // Magic number: half a second in
    }
}

// 0x007ADA20 (:174)
void activity_killing_spree_data::activity_end()
{
    const unsigned int now = keen::SystemTimer::getCurrentMilliseconds();
    int start_delta = (int)(s_timeFirstKillAdded - now);
    // Magic number: the clip starts seven to thirty seconds back.
    if (start_delta < -30000)
    {
        start_delta = -30000;
    }
    else if (start_delta > -7000)
    {
        start_delta = -7000;
    }
    nvidia_highlights_save_video(s_highlightIdKillingSpree, s_groupIdGameplay, start_delta, 0);
    s_timeFirstKillAdded = 0;
    if (!Multiplayer && Wrecking_crew_state == WCS_NONE)
    {
        gfm_change_district_morale((float)activity_data.kill_count, nullptr, nullptr, nullptr);
        stats_increment_by_one(STATS_KILLING_SPREES);
    }
    activity_base_free::activity_end(); // rfg.exe inlines it
    reset();
}

// 0x00798990 (:198)
void activity_killing_spree_data::activity_kill()
{
    if (activity_data.kill_count >= Killing_spree_initial_kills)
    {
        stats_increment_by_one(STATS_KILLING_SPREES);
    }
    activity_base_free::activity_kill(); // rfg.exe inlines it
    reset();
}

// 0x00758D40 (:210)
void activity_killing_spree_data::add_kill_to_tally(const human* hp)
{
    if (activity_data.kill_count == 0)
    {
        s_timeFirstKillAdded = keen::SystemTimer::getCurrentMilliseconds();
    }
    activity_data.kill_count++;
}

// 0x007ADAD0 (:220)
void activity_killing_spree_data::register_death(const object* target_obj, const object* attacker_obj)
{
    if (target_obj == nullptr || target_obj->get_obj_type() != OT_HUMAN)
    {
        return;
    }
    player* pp = player_get_local_player();
    if (pp == nullptr)
    {
        return;
    }
    const human* target = static_cast<const human*>(target_obj);
    bool ragdoll_kill = false;
    if (human_is_ragdolled(target) && object_is_player(target->ragdoll_source_human_handle))
    {
        // Magic number: the three damage types that count as the player's throw.
        const int damage_type = (int)target->combat.last_hit_by_damage_type;
        ragdoll_kill = damage_type == 1 || damage_type == 4 || damage_type == 6;
    }
    if (!activity_register_damage_is_player_controlled(attacker_obj) && !ragdoll_kill)
    {
        return;
    }
    if (human_teams_get_disposition(target, pp) == TEAM_DISPOSITION_HOSTILE)
    {
        if (player_find_shortest_player_xz_dist_sqrd(target_obj->pos) > 22500.0f) // Magic number: 150 metres
        {
            return;
        }
        if (is_active())
        {
            record_kill(target);
            return;
        }
        if (!activity_data.expiration_time.valid() || activity_data.expiration_time.elapsed())
        {
            reset();
            activity_data.expiration_time.set(Killing_spree_initial_time_ms);
        }
        add_kill_to_tally(target);
        if (activity_data.kill_count >= Killing_spree_initial_kills)
        {
            start_killing_spree();
        }
    }
    else if ((int)human_teams_get_disposition(target, pp) > 2 && is_active()) // Magic number: 3 is allied
    {
        activity_end();
    }
}

// 0x00758D60 (:287)
void activity_killing_spree_data::record_kill(const human* hp)
{
    add_kill_to_tally(hp); // rfg.exe inlines it
    if (activity_data.expiration_time.time_until() < Killing_spree_additional_period_ms)
    {
        activity_data.expiration_time.set(Killing_spree_additional_period_ms);
    }
}
