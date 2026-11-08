// Original: volition/rfg/code/gameplay/activities/activity_mayhem.cpp

#include "volition/rfg/code/gameplay/activities/activity_mayhem_internal.h"

#include "volition/audio_lib/code/audiolib_api.h"
#include "volition/rfg/code/destruction/rfg_mover.h"
#include "volition/rfg/code/gameplay/activities/activity_register_damage.h"
#include "volition/rfg/code/gameplay/gameflow/gfm_control_morale.h"
#include "volition/rfg/code/gameplay/wrecking_crew/wrecking_crew_enums.h"
#include "volition/rfg/code/gamewide/gameaudio.h"
#include "volition/rfg/code/gamewide/tweak_table.h"
#include "volition/rfg/code/interface/ui_document_hud_tool_tips.h"
#include "volition/rfg/code/multi/multi_core/include/multi.h"
#include "volition/rfg/code/objects/humans/human_teams.h"
#include "volition/rfg/code/objects/object.h"
#include "volition/rfg/code/rfg.h"
#include "volition/rfg/code/vehicle/vehicle_class.h"
#include "volition/rfg/code/vehicle/vehicle_info.h"
#include "volition/render_lib/code/gr/gr.h"
#include "volition/render_lib/code/gr/gr_font.h"
#include "volition/vlib/code/parse/xml_table_tools.h"
#include "volition/vlib/code/util/string_util.h"

#include <stdio.h>

extern wrecking_crew_states Wrecking_crew_state; // (wrecking crew)

activity_mayhem Mayhem_activity; // 0x01653700

static int Mayhem_max_duration = 10000; // 0x01651354, ten seconds, from the tweak table
static int Mayhem_min_duration = 1000; // 0x01651358, one second, from the tweak table
static int Mayhem_start_duration = 3000; // 0x0165135C, three seconds, from the tweak table
static float Mayhem_time_per_score = 100000.0f; // 0x01651360, from activity_mayhem.xtbl
static float Mayhem_structure_destroyed_bonus = 0.5f; // 0x01651364, from activity_mayhem.xtbl
static bool Mayhem_count_only_enemy; // 0x022295C6. Nothing sets it.
static bool Mayhem_debug; // 0x022295C7. Nothing sets it.
static mayhem_level Mayhem_event_level[5]; // 0x02226780, Magic number: five levels, from rfg.pdb
static int y_start = 500; // 0x0165B100, Magic number: the debug text's row
static tweak_table_item Tweak_table_item_Mayhem_max_duration("Mayhem_max_duration", &Mayhem_max_duration);
static tweak_table_item Tweak_table_item_Mayhem_min_duration("Mayhem_min_duration", &Mayhem_min_duration);
static tweak_table_item Tweak_table_item_Mayhem_start_duration("Mayhem_start_duration", &Mayhem_start_duration);

// 0x0077DE50 (.h:11)
activity_mayhem::activity_mayhem()
    : activity_base_free(ACTIVITY_MODE_MAYHEM)
{
}

// 0x00768D40
activity_mayhem::~activity_mayhem()
{
}

// 0x00753EA0 (.h:59)
float activity_mayhem::get_current_score() const
{
    return total_score;
}

// 0x007989E0 (:103)
void activity_mayhem::register_damage(const object* obj_a, const object* obj_b, float damage, int original_hitpoints)
{
    if (Multiplayer || damage < 100.0f) // Magic number: smaller hits do not start the timer
    {
        return;
    }
    const bool a_controlled = activity_register_damage_is_player_controlled(obj_a);
    const bool b_controlled = activity_register_damage_is_player_controlled(obj_b);
    if ((a_controlled || b_controlled) && !mayhem_timer.valid())
    {
        add_time(Mayhem_start_duration);
    }
}

// 0x00798A50 (:126)
void activity_mayhem::register_death(const object* target_obj, const object* attacker_obj)
{
    if (Multiplayer || target_obj == nullptr || target_obj->get_sub_type() == OT_SUB_MOVER_RFG)
    {
        return;
    }
    if (!activity_register_damage_is_player_controlled(attacker_obj) && !activity_register_damage_is_player_controlled(target_obj))
    {
        return;
    }
    if (!mayhem_timer.valid())
    {
        add_time(Mayhem_start_duration);
    }
    const float cost = activity_register_damage_get_cost(target_obj);
    if (count_object_damage(target_obj) && cost != 0.0f)
    {
        add_to_score(cost, 0.1f); // rfg.exe inlines it. Magic number: a death gives a tenth of the time.
        if (target_obj->get_obj_type() == OT_HUMAN)
        {
            humans_killed++;
        }
        else if (target_obj->get_obj_type() == OT_VEHICLE)
        {
            vehicles_destroyed++;
        }
    }
}

// 0x00798B50 (:157)
void activity_mayhem::register_building_damage(const rfg_mover* objp, float damage_percent_delta, float original_damage_percent)
{
    if (Multiplayer || objp == nullptr || !activity_register_damage_is_player_controlled(objp) || !count_object_damage(objp))
    {
        return;
    }
    add_to_score(activity_register_damage_get_cost(objp) * damage_percent_delta, 1.0f); // rfg.exe inlines it
}

// 0x00798BD0 (:171)
void activity_mayhem::register_building_destroyed(const rfg_mover* objp)
{
    if (Multiplayer || objp == nullptr || objp->get_sub_type() == OT_SUB_MOVER_GENERAL || !activity_register_damage_is_player_controlled(objp) || !count_object_damage(objp))
    {
        return;
    }
    add_to_score(activity_register_damage_get_cost(objp) * Mayhem_structure_destroyed_bonus, 1.0f); // rfg.exe inlines it
    if (objp->has_building_size() && objp->has_building_mass())
    {
        structures_destroyed++;
    }
}

// 0x00783540 (:196)
void activity_mayhem::register_explosion(const object* owner, const explosion_info* p_explosion_info)
{
    if (!Multiplayer && object_is_player(owner))
    {
        add_time(1000); // Magic number: an explosion gives one second
        explosions_caused++;
    }
}

// 0x00758DB0 (:211). Original: more than five levels in the table overrun Mayhem_event_level.
void activity_mayhem::system_init()
{
    for (int i = 0; i < 5; i++)
    {
        Mayhem_event_level[i].threshold = (float)i;
        Mayhem_event_level[i].reward = 0.0f;
    }
    xml_element* root = xml_table_parse("activity_mayhem.xtbl");
    if (root == nullptr)
    {
        return;
    }
    xml_element* mayhem = xml_table_find_req(root, "Activity_Mayhem");
    if (mayhem == nullptr)
    {
        return;
    }
    xml_element* levels = xml_table_find_req(mayhem, "Levels");
    if (levels == nullptr)
    {
        return;
    }
    int index = 0;
    // Original: walks the next levels from the activity's element, not from the list.
    for (xml_element* level = xml_table_find_opt(levels, "Level"); level != nullptr; level = xml_table_find_next(mayhem, level, "Level"))
    {
        xml_table_get_req_float(Mayhem_event_level[index].threshold, level, "Threshold");
        xml_table_get_req_float(Mayhem_event_level[index].reward, level, "Reward");
        index++;
    }
    int time_per_score;
    xml_table_get_req_int(time_per_score, mayhem, "Time_per_score");
    Mayhem_time_per_score = (float)time_per_score;
    xml_table_get_req_float(Mayhem_structure_destroyed_bonus, mayhem, "Destroy_bonus");
    xml_table_close(root);
}

// 0x00758F00 (:255)
void activity_mayhem::level_init()
{
    activity_base_free::level_init(); // rfg.exe inlines it
    high_score = 0.0f;
    activity_reset();
}

// 0x007ADC40 (:266)
bool activity_mayhem::activity_start()
{
    add_time(Mayhem_max_duration);
    audiolib_cue_play(Ga_cue_ids.popup_mayhem_started, nullptr, nullptr);
    if (!Multiplayer)
    {
        ui_tool_tips_add_tip("Mayhem", 3000, nullptr); // Magic number: three seconds in
    }
    return activity_base_free::activity_start();
}

// 0x007ADC90 (:281)
void activity_mayhem::activity_end()
{
    activity_base_free::activity_end(); // rfg.exe inlines it
    mayhem_timer.invalidate();
    if (high_score - total_score < 0.0f)
    {
        high_score = total_score;
    }
    const int level = get_level();
    if (level >= 0)
    {
        gfm_change_district_morale(Mayhem_event_level[level].reward, nullptr, nullptr, nullptr);
    }
}

// 0x00798C80 (:300)
void activity_mayhem::activity_kill()
{
    activity_base_free::activity_kill(); // rfg.exe inlines it
    activity_reset();
}

// 0x00758F40 (:308)
void activity_mayhem::activity_reset()
{
    activity_base_free::activity_reset(); // rfg.exe inlines it
    total_score = 0.0f;
    humans_killed = 0;
    structures_destroyed = 0;
    vehicles_destroyed = 0;
    explosions_caused = 0;
    total_time.set(0);
    mayhem_timer.invalidate();
}

// 0x0076E8E0 (:324)
void activity_mayhem::activity_do_frame_inactive()
{
    if (get_level() >= 0 && mayhem_timer.valid() && Wrecking_crew_state == WCS_NONE)
    {
        activity_start();
    }
}

// 0x00758FD0 (:337)
void activity_mayhem::activity_do_frame_active()
{
    if (mayhem_timer.elapsed())
    {
        activity_end();
    }
}

// 0x0076E940 (:349). Original: answers false whatever it draws.
bool activity_mayhem::render_hud(int x_pos, int y_pos)
{
    if (!is_active())
    {
        return false;
    }
    const int level = get_level();
    if (level < 0 && !Mayhem_debug)
    {
        return false;
    }
    int alpha = 255;
    if (mayhem_timer.time_until() < Mayhem_min_duration)
    {
        alpha = mayhem_timer.time_until() * 255 / Mayhem_min_duration; // fades over the last moments
    }
    const int y = y_start;
    gr_set_color(255, 0, 0, alpha);
    gr_string(0x8000, y, "Mayhem", Header_font, GM_FILTER); // Magic number: 0x8000 centres the text
    gr_set_color(255, 255, 255, alpha);
    char text[256]; // rfg.exe prints to its thread-local buffer
    sprintf(text, "(%.1f Seconds Remaining)", (double)mayhem_timer.time_until() / 1000.0);
    gr_string(0x8000, y + 0x23, text, General_font, GM_FILTER);
    sprintf(text, "Level %d - $%.2f Million in Damages", level + 1, (double)total_score);
    gr_string(0x8000, y + 0x37, text, General_font, GM_FILTER);
    sprintf(text, "%d Structures Destroyed - %d Vehicles Destroyed - %d Explosions Caused", structures_destroyed, vehicles_destroyed, explosions_caused);
    gr_string(0x8000, y + 0x4B, text, General_font, GM_FILTER);
    return false;
}

// 0x00758FF0 (:394)
const char* activity_mayhem::get_title() const
{
    return "ACT_MAYHEM";
}

// 0x00759000 (:399)
const char* activity_mayhem::get_icon_image_name() const
{
    return "ui_hud_diversion_icon_mayhem";
}

// 0x00759010 (:405)
bool activity_mayhem::get_ui_display_info(char* value_text, int& time_til_end, int buffer_size) const
{
    if (value_text == nullptr)
    {
        return false;
    }
    time_til_end = mayhem_timer.time_until();
    _snprintf_safe(value_text, buffer_size, "%.2f", (double)total_score);
    return true;
}

// 0x0076EB00 (:428)
void activity_mayhem::add_time(int period_ms)
{
    if (!mayhem_timer.valid() || mayhem_timer.elapsed())
    {
        if (is_active())
        {
            return;
        }
        activity_reset();
        mayhem_timer.set(period_ms);
        return;
    }
    int remaining = mayhem_timer.time_until() + period_ms;
    if (remaining < Mayhem_min_duration)
    {
        remaining = Mayhem_min_duration;
    }
    else if (remaining > Mayhem_max_duration)
    {
        remaining = Mayhem_max_duration;
    }
    mayhem_timer.set(remaining);
}

// 0x0076EB90 (:444)
void activity_mayhem::add_to_score(float value, float scale)
{
    total_score += value;
    add_time((int)(Mayhem_time_per_score * value * scale));
}

// 0x00759070 (:452)
int activity_mayhem::get_level() const
{
    for (int i = 4; i >= 0; i--)
    {
        if (Mayhem_event_level[i].threshold <= total_score)
        {
            return i;
        }
    }
    return -1;
}

// 0x007590A0 (:461)
bool activity_mayhem::count_object_damage(const object* op) const
{
    if (op == nullptr || (op->get_sub_type() == OT_SUB_MOVER_RFG && op->get_parent() != 0xFFFFFFFF))
    {
        return false;
    }
    human_teams team = human_teams_get_team(op);
    if (op->get_obj_type() == OT_VEHICLE)
    {
        // Original: compares the whole flag word with 1, so only a vehicle with no other spawn flag counts as EDF.
        const vehicle* vp = static_cast<const vehicle*>(op);
        team = *reinterpret_cast<const unsigned int*>(&vp->stream_placement_flags) != 1 ? vp->info->default_team : HUMAN_TEAM_EDF;
    }
    const human_team_dispositions disposition = human_teams_get_disposition(HUMAN_TEAM_GUERILLA, team);
    if (Mayhem_count_only_enemy)
    {
        return disposition == TEAM_DISPOSITION_HOSTILE;
    }
    return (int)disposition < 3; // Magic number: 3 is allied
}
