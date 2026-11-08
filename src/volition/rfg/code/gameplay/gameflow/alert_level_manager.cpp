// Original: volition/rfg/code/gameplay/gameflow/alert_level_manager.cpp
//
// gameflow.md GF2. The EDF alert value and level: raised by reported damage and deaths near the player and by EDF npcs
// that see the player, lowered by decay, capped by scripts. gameplay_do_frame calls gsm_do_frame.
//
// Plain float where rfg.exe uses double chains, like the AI code: nothing here is compared bit for bit. Comparisons keep
// the form rfg.exe uses (x - y < 0).

#include "volition/rfg/code/gameplay/gameflow/alert_level_manager.h"
#include "volition/rfg/code/gameplay/gameflow/alert_level_manager_internal.h"

#include "volition/audio_lib/code/audiolib_api.h"
#include "volition/rfg/code/ai/ai_defense_coordination.h"
#include "volition/rfg/code/ai/ai_system.h"
#include "volition/rfg/code/ai/percept.h"
#include "volition/rfg/code/gameplay/activities/activity.h"
#include "volition/rfg/code/gameplay/difficulty.h"
#include "volition/rfg/code/gameplay/gameflow/gfm_damage_report.h"
#include "volition/rfg/code/gameplay/gameflow/gfm_orders_manager.h"
#include "volition/rfg/code/gameplay/gameflow/reinforcement_location.h"
#include "volition/rfg/code/gameplay/missions.h"
#include "volition/rfg/code/gameplay/wrecking_crew/wrecking_crew_enums.h"
#include "volition/rfg/code/gamewide/gameaudio.h"
#include "volition/rfg/code/gamewide/stats.h"
#include "volition/rfg/code/interface/ui_document_hud_tool_tips.h"
#include "volition/rfg/code/level/world.h"
#include "volition/rfg/code/multi/multi_core/include/multi.h"
#include "volition/rfg/code/objects/humans/human_combat.h"
#include "volition/rfg/code/objects/humans/human_teams.h"
#include "volition/rfg/code/objects/humans/npc.h"
#include "volition/rfg/code/objects/humans/player.h"
#include "volition/rfg/code/objects/object_safehouse.h"
#include "volition/rfg/code/objects/projectile.h"
#include "volition/rfg/code/scripting/action_script/global_event_data.h"
#include "volition/rfg/code/scripting/action_script/global_event_manager.h"
#include "volition/rfg/code/vehicle/vehicle_class.h"
#include "volition/rfg/code/vehicle/vehicle_info.h"
#include "volition/rfg/code/vehicle/vehicle_system.h"
#include "volition/rfg/code/weapons/weapons.h"
#include "volition/vlib/code/math/vector3.h"
#include "volition/vlib/code/parse/xml_table_tools.h"
#include "volition/vlib/code/util/fastxml.h"
#include "volition/vlib/code/util/string_comp.h"

extern wrecking_crew_states Wrecking_crew_state; // 0x01651888 (wrecking_crew.cpp)
extern gfm_orders_manager Orders_manager; // 0x0222DE40 (gameplay.cpp)

// Magic number: 200 m squared, the reach of a report around a player.
static const float ALERT_REPORT_DIST_SQRD = 40000.0f;
// Magic number: 4 alert levels, 10 handles (Alerted_handles), 500 ms grace after green, 1 s between evaluations.
static const int MAX_ALERTED_HANDLES = 10;
static const int ALERT_GRACE_MS = 500;
static const int ALERT_DO_FRAME_PERIOD_MS = 1000;

gfm_alert_levels Current_alert_level; // 0x022278B4
float Current_alert_value; // 0x022258A0
static float Next_alert_delta; // 0x022258B0
static gfm_alert_levels Last_alert_level_manager; // 0x02229470
static float Min_alert_value; // 0x02229618
static gfm_alert_levels Min_alert_level; // 0x0222961C
static float Max_alert_value = 3.4028235e+38f; // 0x01651404, initial data FLT_MAX
static gfm_alert_levels Max_alert_level = ALERT_LEVEL_RED; // 0x01651408
static float Contact_growth_rate = 1.0f; // 0x0165140C
static float Max_decay_multiplier = 4.0f; // 0x01651410
static float Max_dexay_ramp_period_ms = 20.0f; // 0x01651414, rfg.pdb's spelling; it holds seconds
static bool Played_alert_decrease = true; // 0x01651418
static int Alert_level_decay_delay_ms = 20000; // 0x0165141C
static float Lha_min_decay_distance = 30.0f; // 0x01651420
static float Lha_max_decay_distance = 150.0f; // 0x01651424
static float Lha_min_decay = 10.0f; // 0x01651428
static float Lha_max_decay = 50.0f; // 0x0165142C
static float Green_building_damage_thresh; // 0x0222581C
static bool Played_red_alert_failure; // 0x02229616
static bool Reset_decay_timer; // 0x02229617
static bool Last_hostile_action_pos_active; // 0x02229620
static bool Ratchet_alert_level; // 0x02229621
static int Num_alerted_handles; // 0x02229624
static unsigned int Alerted_handles[MAX_ALERTED_HANDLES]; // 0x02227A48
static vector Last_hostile_action_pos; // 0x0226EDBC
static timestamp Do_frame_period; // 0x01657688
static timestamp Last_alert_level_change; // 0x0165768C
static timestamp Last_los_time; // 0x01657690
static timestamp Alert_in_one_second; // 0x01657694
static timestamp Alert_level_grace_timer; // 0x01657698

// Index 0 of IMPORTANCE_LEVEL_MULT_TAG is null: unimportant objects keep a multiplier of 1.
static const char* const IMPORTANCE_LEVEL_MULT_TAG[4] = { nullptr, "importance_mul_low", "importance_mul_medium", "importance_mul_high" }; // 0x016513F4
static const char* const Alert_level_type_tags[NUM_ALERT_LEVELS] = { "green", "yellow", "orange", "red" }; // 0x01651430

base_alert_level_values Alert_level_ranges[NUM_ALERT_LEVELS] = { // 0x01657598
    base_alert_level_values(ALERT_LEVEL_GREEN, nullptr, nullptr, "DecayRateGreen"),
    base_alert_level_values(ALERT_LEVEL_YELLOW, "SolidYellowMin", "SolidYellowMax", "DecayRateYellow"),
    base_alert_level_values(ALERT_LEVEL_ORANGE, "SolidOrangeMin", "SolidOrangeMax", "DecayRateOrange"),
    base_alert_level_values(ALERT_LEVEL_RED, "SolidRedMin", "SolidRedMax", "DecayRateRed"),
};
static alert_value_parameters Alert_values_info[3] = { // 0x01657628
    alert_value_parameters(OT_MOVER, "Structure"),
    alert_value_parameters(OT_HUMAN, "Human"),
    alert_value_parameters(OT_VEHICLE, "Vehicle"),
};

// 0x00753B00 (global_event_data.h:176)
alert_level_change_event::alert_level_change_event()
    : event_data(EVENT_ALERT_LEVEL_CHANGED)
{
}

// A level in the alert states yellow to red.
static bool alert_level_is_alerted()
{
    return (int)Current_alert_level >= 0 && ((int)Current_alert_level >= 4 || (int)Current_alert_level > 0);
}

// 0x00772E80 (alert_level_manager_internal.h:10)
base_alert_level_values::base_alert_level_values(gfm_alert_levels level_, const char* min_value_tag_, const char* max_value_tag_, const char* decay_rate_tag_)
    : level(level_)
    , min_value(0.0f)
    , max_value(0.0f)
    , lower_bound(0.0f)
    , upper_bound(0.0f)
    , decay_rate(0.0f)
    , min_value_tag(min_value_tag_)
    , max_value_tag(max_value_tag_)
    , decay_rate_tag(decay_rate_tag_)
{
}

// 0x0075D030 (alert_level_manager_internal.h:23)
void base_alert_level_values::reset()
{
    min_value = 0.0f;
    max_value = 0.0f;
    lower_bound = 0.0f;
    upper_bound = 0.0f;
    decay_rate = 0.0f;
}

// 0x00772EC0 (alert_level_manager_internal.h:45)
alert_value_parameters::alert_value_parameters(object_type type_, const char* type_name_)
    : type(type_)
    , type_name(type_name_)
{
    reset();
}

// 0x0075D050 (:252)
void base_alert_level_values::load(xml_element* xml_root)
{
    reset();
    if (min_value_tag != nullptr)
    {
        xml_element* e = xml_find(xml_root, min_value_tag);
        if (e == nullptr)
        {
            return;
        }
        min_value = xml_get_float(e, 0.0f);
    }
    if (max_value_tag != nullptr)
    {
        xml_element* e = xml_find(xml_root, max_value_tag);
        if (e == nullptr)
        {
            return;
        }
        max_value = xml_get_float(e, 0.0f);
    }
    xml_element* e = xml_find(xml_root, decay_rate_tag);
    if (e != nullptr)
    {
        // Original quirk kept: the clamp only has a floor of 1, so a rate in the table never falls below 1 per second.
        const float rate = xml_get_float(e, 0.0f);
        decay_rate = 0.0f <= 1.0f - rate ? 1.0f : rate;
    }
}

// 0x0075D130 (:283)
void alert_value_parameters::reset()
{
    imp_multipliers[0] = 1.0f;
    imp_multipliers[1] = 1.0f;
    imp_multipliers[2] = 1.0f;
    imp_multipliers[3] = 1.0f;
    damage_mult = 1.0f;
    death_value = 0.0f;
}

// 0x0075D160 (:294)
void alert_value_parameters::load(xml_element* xml_root)
{
    reset();
    for (int i = 0; i < 4; i++)
    {
        if (IMPORTANCE_LEVEL_MULT_TAG[i] == nullptr)
        {
            continue;
        }
        xml_element* e = xml_find(xml_root, IMPORTANCE_LEVEL_MULT_TAG[i]);
        if (e == nullptr)
        {
            return;
        }
        const float mult = xml_get_float(e, 0.0f);
        imp_multipliers[i] = mult < 0.0f ? 0.0f : mult;
    }
    xml_element* e = xml_find(xml_root, "damage_multiplier");
    if (e == nullptr)
    {
        return;
    }
    const float damage = xml_get_float(e, 0.0f);
    damage_mult = 0.0f <= 0.0f - damage ? 0.0f : damage;
    e = xml_find(xml_root, "death_value");
    if (e != nullptr)
    {
        const float death = xml_get_float(e, 0.0f);
        death_value = 0.0f <= 0.0f - death ? 0.0f : death;
    }
}

// 0x00772F00 (:359)
// Vehicles and weapons make the player's actions more or less alerting.
float alert_value_apply_multiplier(float alert_value)
{
    const player* pp = player_get_local_player();
    if (pp == nullptr)
    {
        return alert_value;
    }
    if (pp->vehicle_handle != 0xFFFFFFFF)
    {
        object* objp = World.object_from_handle(pp->vehicle_handle);
        if (objp != nullptr && objp->get_obj_type() == OT_VEHICLE)
        {
            return static_cast<vehicle*>(objp)->info->m_alert_multiplier * alert_value;
        }
        return alert_value;
    }
    const weapon* wp = pp->get_equipped_weapon();
    return wp != nullptr ? wp->w_info->alert_multiplier * alert_value : alert_value;
}

// 0x0075D2B0 (:386)
bool is_important(const human* hp)
{
    return (int)hp->importance_level > 0;
}

// 0x0075D2C0 (:392)
bool is_edf(const human* hp)
{
    return human_teams_get_disposition(hp, HUMAN_TEAM_EDF) > TEAM_DISPOSITION_NONE;
}

// 0x0075D2E0 (:398)
bool should_report_human(const human* hp)
{
    return is_important(hp) && is_edf(hp) && hp->is_killed();
}

// 0x0075D320 (:404)
bool is_occupied(const vehicle* vp)
{
    return vp->m_flags.m_occupied != 0;
}

// 0x0075D340 (:409)
bool currently_edf(const vehicle* vp)
{
    return human_teams_get_disposition(vp->team_of_most_recent_driver, HUMAN_TEAM_EDF) > TEAM_DISPOSITION_NONE;
}

// 0x0075D360 (:414)
bool is_edf(const vehicle* vp)
{
    return human_teams_get_disposition(vp->info->default_team, HUMAN_TEAM_EDF) > TEAM_DISPOSITION_NONE;
}

// 0x0075D390 (:420)
bool should_report_vehicle_damage(const vehicle* vp)
{
    return is_occupied(vp) && currently_edf(vp) && !vp->is_killed() && vp->get_reported_vehicle_damage() < vp->get_vehicle_damage();
}

// 0x0075D400 (:427)
bool should_report_vehicle_death(const vehicle* vp)
{
    return vp->m_flags.death_reported == 0 && vp->is_killed() && is_edf(vp);
}

// 0x0075D440 (:432)
static void init_alerted_handles()
{
    Num_alerted_handles = 0;
    for (int i = 0; i < MAX_ALERTED_HANDLES; i++)
    {
        Alerted_handles[i] = 0xFFFFFFFF;
    }
    Alert_in_one_second.invalidate();
}

// 0x0075D490 (:443)
bool is_near_a_player(unsigned int handle)
{
    const object* objp = World.object_from_handle(handle);
    return objp != nullptr && player_find_shortest_player_xz_dist_sqrd(objp->pos) < ALERT_REPORT_DIST_SQRD;
}

// 0x0079C440 (:468), the human in ESI
static void report_human_death(human* dead_hp)
{
    gfm_damage_report report;
    dead_hp->hflags.death_reported = 1;
    report.handle = dead_hp->get_handle();
    report.importance_level = dead_hp->importance_level;
    report.type = OT_HUMAN;
    report.report_new_damage(1.0f);
    alm_report_death(report);
}

// 0x0079C4C0 (:482), the vehicle in ESI
static void report_vehicle_damage(const vehicle* vp)
{
    gfm_damage_report report;
    report.handle = vp->get_handle();
    report.type = OT_VEHICLE;
    report.importance_level = OBJECT_IMPORTANCE_LEVEL_LOW;
    const float reported = vp->get_reported_vehicle_damage();
    report.report_new_damage(reported, vp->get_vehicle_damage());
    alm_report_damage(report);
}

// 0x0079C530 (:494), the vehicle in ESI
static void report_vehicle_death(const vehicle* vp)
{
    gfm_damage_report report;
    report.handle = vp->get_handle();
    report.type = OT_VEHICLE;
    report.importance_level = OBJECT_IMPORTANCE_LEVEL_LOW;
    report.report_new_damage(vp->get_reported_vehicle_damage(), 1.0f);
    alm_report_death(report);
}

// 0x0079C5A0 (:509)
// Called on leaving green: deaths seen while green did not count, so they count now.
static void report_dead_npcs()
{
    if (!alert_level_is_alerted())
    {
        return;
    }
    const int num = World.get_num_objects<npc>();
    for (int i = 0; i < num; i++)
    {
        npc* np = World.get_object<npc>(i);
        if (is_important(np) && is_edf(np) && np->is_killed() && np->hflags.death_reported == 0)
        {
            report_human_death(np);
        }
    }
}

// 0x0079C640 (:560), a jump to report_dead_npcs
static void change_to_alert_state()
{
    report_dead_npcs();
}

// 0x007A4250 (:568)
static void reset_from_alert_state()
{
    init_alerted_handles();
    ai_system_set_alert_level_green();
    Last_hostile_action_pos = ZERO_VECTOR;
    Last_hostile_action_pos_active = false;
    // Projectiles seen flying count as already looked into, so nobody goes on investigating after the alert.
    const int num = World.get_num_objects<projectile>();
    for (int i = 0; i < num; i++)
    {
        projectile* pp = World.get_object<projectile>(i);
        if (pp->pflags.spotted != 0)
        {
            pp->pflags.investigated = 1;
        }
    }
    Orders_manager.alert_level_cleared();
    Alert_level_grace_timer.set(ALERT_GRACE_MS);
}

// 0x007A4330 (:601). force_alert_change is unused; the level arrives in ESI.
static void change_alert_level(gfm_alert_levels new_alert_level, bool force_alert_change)
{
    (void)force_alert_change;
    if ((unsigned int)new_alert_level >= NUM_ALERT_LEVELS || Current_alert_level == new_alert_level)
    {
        return;
    }
    audiolib_cue_play(new_alert_level == ALERT_LEVEL_RED ? Ga_cue_ids.general_alert_changed_to_red : Ga_cue_ids.general_alert_changed, nullptr, nullptr);
    const bool raised = (int)Current_alert_level < (int)new_alert_level;
    const bool left_green = raised && (int)Current_alert_level <= 0;
    Last_alert_level_manager = Current_alert_level;
    Current_alert_level = new_alert_level;
    Last_alert_level_change.set(0);
    if (!Multiplayer && mission_get_mode() != MISSION_MODE_TUTORIAL && !safehouse_contains_player() && Current_alert_level != ALERT_LEVEL_GREEN)
    {
        ui_tool_tips_add_tip("Yellow_Alert", 0, nullptr);
    }
    alert_level_change_event data;
    data.alert_level = Current_alert_level;
    data.old_level = Last_alert_level_manager;
    gem_post_event(data);
    if ((int)Current_alert_level < 1)
    {
        reset_from_alert_state();
    }
    if (raised)
    {
        Reset_decay_timer = true;
        if (left_green)
        {
            change_to_alert_state();
        }
    }
    else
    {
        Played_red_alert_failure = false;
    }
}

// 0x00772F90 (:669)
void load_alert_level_ranges()
{
    xml_element* root = xml_table_parse("alert_level.xtbl");
    if (root == nullptr)
    {
        return;
    }
    xml_element* data = xml_find(root, "GameStateData");
    if (data == nullptr)
    {
        return;
    }
    for (int i = 0; i < NUM_ALERT_LEVELS; i++)
    {
        Alert_level_ranges[i].load(data);
    }
    Max_alert_value = Alert_level_ranges[ALERT_LEVEL_RED].max_value;
    for (int i = 0; i < NUM_ALERT_LEVELS; i++)
    {
        Alert_level_ranges[i].lower_bound = i == 0 ? 0.0f : Alert_level_ranges[i - 1].max_value;
        Alert_level_ranges[i].upper_bound = i == ALERT_LEVEL_RED ? Alert_level_ranges[i].max_value : Alert_level_ranges[i + 1].min_value;
    }
    // Original quirk kept: a missing tag from here on returns without closing the table.
    xml_element* e = xml_find(data, "ContactGrowRate");
    if (e == nullptr)
    {
        return;
    }
    Contact_growth_rate = xml_get_float(e, 1.0f);
    e = xml_find(data, "structure_damage_threshold");
    if (e == nullptr)
    {
        return;
    }
    Green_building_damage_thresh = xml_get_float(e, 0.0f);
    if (Green_building_damage_thresh - 0.0f < 0.0f)
    {
        Green_building_damage_thresh = 0.0f;
    }
    if (0.0f <= Green_building_damage_thresh - 1.0f)
    {
        Green_building_damage_thresh = 1.0f;
    }
    e = xml_find(data, "MaxDecayMultiplier");
    if (e == nullptr)
    {
        return;
    }
    Max_decay_multiplier = xml_get_float(e, 1.0f);
    e = xml_find(data, "decay_ramp_period");
    if (e == nullptr)
    {
        return;
    }
    Max_dexay_ramp_period_ms = xml_get_float(e, 20.0f);
    e = xml_find(data, "decay_start_period");
    if (e == nullptr)
    {
        return;
    }
    // Magic number: the table holds seconds.
    Alert_level_decay_delay_ms = xml_get_int(e, 0) * 1000;
    if (Alert_level_decay_delay_ms < 0)
    {
        Alert_level_decay_delay_ms = 0;
    }
    xml_element* min_dist = xml_find(data, "LHA_Min_Decay_Distance");
    if (min_dist == nullptr)
    {
        return;
    }
    xml_element* max_dist = xml_find(data, "LHA_Max_Decay_Distance");
    if (max_dist == nullptr)
    {
        return;
    }
    xml_element* min_decay = xml_find(data, "LHA_Minimum_Decay");
    if (min_decay == nullptr)
    {
        return;
    }
    xml_element* max_decay = xml_find(data, "LHA_Maximum_Decay");
    if (max_decay == nullptr)
    {
        return;
    }
    Lha_min_decay_distance = xml_get_float(min_dist, 0.0f);
    Lha_max_decay_distance = xml_get_float(max_dist, 0.0f);
    Lha_min_decay = xml_get_float(min_decay, 0.0f);
    Lha_max_decay = xml_get_float(max_decay, 0.0f);
    xml_table_close(root);
}

// 0x0075D4D0 (:758)
void load_alert_values()
{
    xml_element* root = xml_table_parse("alert_value.xtbl");
    if (root == nullptr)
    {
        return;
    }
    for (xml_element* cur = xml_find(root, "alert_values"); cur != nullptr; cur = xml_find_next(root, cur, "alert_values"))
    {
        const xml_element* name = xml_find(cur, "Name");
        if (name == nullptr)
        {
            continue;
        }
        for (alert_value_parameters& params : Alert_values_info)
        {
            if (v_stricmp(params.type_name, name->text) == 0)
            {
                params.load(cur);
            }
        }
    }
    xml_table_close(root);
}

// 0x0075D570 (:781), type in EDX
static const alert_value_parameters* get_alert_parameters(object_type type)
{
    for (const alert_value_parameters& params : Alert_values_info)
    {
        if (params.type == type)
        {
            return &params;
        }
    }
    return nullptr;
}

// 0x00773270 (:795), damage_level in XMM2, report in EAX, alert_params in ECX
static float get_total_alert_value(float damage_level, const gfm_damage_report& report, const alert_value_parameters* alert_params)
{
    if ((int)report.importance_level <= 0 || (int)report.importance_level >= 4)
    {
        return 0.0f;
    }
    return alert_params->imp_multipliers[report.importance_level] * damage_level * alert_params->damage_mult * difficulty_get_value(DIFFICULTY_ENEMY_ALERT, NUM_DIFFICULTY_LEVELS);
}

// 0x007732E0 (:807)
static float get_decay_delta(int time_elapsed)
{
    if (!Last_los_time.elapsed())
    {
        return 0.0f;
    }
    const int level = (int)Current_alert_level <= -1 ? 0 : ((int)Current_alert_level >= 4 ? 3 : (int)Current_alert_level);
    const float base = (float)time_elapsed / 1000.0f * Alert_level_ranges[level].decay_rate;
    // Ramps up from 1 to Max_decay_multiplier over the ramp period, in seconds since the delay ran out.
    float mult = (float)Last_los_time.time_since() / 1000.0f / Max_dexay_ramp_period_ms * Max_decay_multiplier;
    if (mult - 1.0f < 0.0f)
    {
        mult = 1.0f;
    }
    if (0.0f <= mult - Max_decay_multiplier)
    {
        mult = Max_decay_multiplier;
    }
    return mult * base;
}

// 0x0078B540 (:831)
static int get_hostile_target_count()
{
    int count = 0;
    const int num = World.get_num_objects<npc>();
    for (int i = 0; i < num; i++)
    {
        npc* np = World.get_object<npc>(i);
        if (np->is_killed() || !is_edf(np))
        {
            continue;
        }
        const ai_perception& perception = np->get_ai()->perception;
        if (perception.any_enemy_player_in_los())
        {
            const human* target = human_combat_get_target(np);
            if (target != nullptr && perception.can_engage_target_reason(target) == AI_ENGAGE_ALLOWED)
            {
                count++;
            }
        }
    }
    return count;
}

// 0x0078B5F0 (:856)
static bool has_los_to_enemy_player()
{
    const int num = World.get_num_objects<npc>();
    for (int i = 0; i < num; i++)
    {
        npc* np = World.get_object<npc>(i);
        if (np->is_killed() || !is_edf(np))
        {
            continue;
        }
        const ai_perception& perception = np->get_ai()->perception;
        if (perception.any_enemy_player_in_los())
        {
            const human* target = human_combat_get_target(np);
            if (target != nullptr && perception.can_engage_target_reason(target) == AI_ENGAGE_ALLOWED)
            {
                return true;
            }
        }
    }
    return false;
}

// 0x0078B690 (:877)
static float get_los_delta(int time_elapsed)
{
    if (alert_level_is_alerted())
    {
        return 0.0f;
    }
    return (float)get_hostile_target_count() * Contact_growth_rate * ((float)time_elapsed / 1000.0f);
}

// 0x007733E0 (:893)
static float get_dist_delta()
{
    if ((int)Current_alert_level <= -1 || ((int)Current_alert_level < 4 && Current_alert_level == ALERT_LEVEL_GREEN) || !Last_hostile_action_pos_active)
    {
        return 0.0f;
    }
    const float dist = vec_dist_squared(player_get_local_player()->pos, Last_hostile_action_pos);
    if (Lha_min_decay_distance * Lha_min_decay_distance > dist)
    {
        return 0.0f;
    }
    return Lha_max_decay_distance * Lha_max_decay_distance > dist ? Lha_min_decay : Lha_max_decay;
}

// 0x0075D5A0 (:919), current_alert_level in EAX
static gfm_alert_levels get_alert_level(gfm_alert_levels current_alert_level)
{
    int level = (int)current_alert_level;
    while (level > -1)
    {
        if (level >= 4)
        {
            return ALERT_LEVEL_RED;
        }
        if (Alert_level_ranges[level].lower_bound >= Current_alert_value)
        {
            level--;
        }
        else if (Current_alert_value >= Alert_level_ranges[level].upper_bound)
        {
            level++;
        }
        else
        {
            return (gfm_alert_levels)level;
        }
    }
    return ALERT_LEVEL_GREEN;
}

// 0x007A4430 (:946)
static void evaluate_alert_level(bool force_alert_change)
{
    float value = Current_alert_value;
    if (value - Min_alert_value < 0.0f)
    {
        value = Min_alert_value;
    }
    if (0.0f <= value - Max_alert_value)
    {
        value = Max_alert_value;
    }
    Current_alert_value = value;
    change_alert_level(get_alert_level(Current_alert_level), force_alert_change);
}

// 0x00773470 (:968)
void gsm_level_init_pre()
{
    Current_alert_level = ALERT_LEVEL_GREEN;
    Last_alert_level_manager = ALERT_LEVEL_YELLOW;
    Current_alert_value = 0.0f;
    Next_alert_delta = 0.0f;
    Do_frame_period.set(0);
    Last_alert_level_change.set(0);
    Last_los_time.set(0);
    Min_alert_value = Alert_level_ranges[ALERT_LEVEL_GREEN].min_value;
    Played_red_alert_failure = false;
    Min_alert_level = ALERT_LEVEL_GREEN;
    Played_alert_decrease = true;
    Reset_decay_timer = true;
    Max_alert_value = Alert_level_ranges[ALERT_LEVEL_RED].max_value;
    Max_alert_level = ALERT_LEVEL_RED;
    init_alerted_handles();
    Alert_level_grace_timer.invalidate();
}

// 0x00773550 (:999)
void gsm_game_init()
{
    load_alert_level_ranges();
    load_alert_values();
}

// 0x0075D610 (:1011)
gfm_alert_levels gsm_get_alert_level()
{
    if ((int)Current_alert_level < 0)
    {
        return ALERT_LEVEL_GREEN;
    }
    return (int)Current_alert_level > 3 ? ALERT_LEVEL_RED : Current_alert_level;
}

// 0x0075D630 (:1026)
float gsm_get_alert_value()
{
    return Current_alert_value;
}

// 0x007AA740 (:1032)
void gsm_set_alert_level(gfm_alert_levels new_alert_level)
{
    if ((unsigned int)new_alert_level < NUM_ALERT_LEVELS)
    {
        gsm_set_alert_value(Alert_level_ranges[new_alert_level].min_value);
    }
}

// 0x0075D640 (:1046)
void gsm_release_alert_level_cap()
{
    Min_alert_value = Alert_level_ranges[ALERT_LEVEL_GREEN].min_value;
    Max_alert_value = Alert_level_ranges[ALERT_LEVEL_RED].max_value;
    Min_alert_level = ALERT_LEVEL_GREEN;
    Max_alert_level = ALERT_LEVEL_RED;
}

// 0x007AA770 (:1056)
void gsm_set_alert_level_cap(gfm_alert_levels min, gfm_alert_levels max)
{
    if ((int)min > (int)max)
    {
        return;
    }
    Min_alert_level = min;
    Min_alert_value = Alert_level_ranges[min].min_value;
    Max_alert_value = Alert_level_ranges[max].max_value;
    Max_alert_level = max;
    if (Current_alert_value < Min_alert_value)
    {
        gsm_set_alert_value(Min_alert_value);
    }
    if (Max_alert_value < Current_alert_value)
    {
        gsm_set_alert_value(Max_alert_value);
    }
}

// 0x0075D680 (:1078)
void gsm_get_alert_level_cap(gfm_alert_levels& min, gfm_alert_levels& max)
{
    min = Min_alert_level;
    max = Max_alert_level;
    if (stats_cheat_enabled(CHEATS_ALWAYS_YELLOW_ALERT) && (int)min < 1)
    {
        min = ALERT_LEVEL_YELLOW;
    }
}

// 0x007A44B0 (:1091)
void gsm_set_alert_value(float new_alert_value)
{
    if (new_alert_value - Min_alert_value < 0.0f)
    {
        new_alert_value = Min_alert_value;
    }
    if (0.0f <= new_alert_value - Max_alert_value)
    {
        new_alert_value = Max_alert_value;
    }
    Next_alert_delta = 0.0f;
    Current_alert_value = new_alert_value;
    evaluate_alert_level(false);
}

// 0x0075D6C0 (:1102)
float gsm_find_alert_value_percentage()
{
    const gfm_alert_levels level = gsm_get_alert_level();
    if (Current_alert_value <= 0.0f)
    {
        return 0.0f;
    }
    const base_alert_level_values& range = Alert_level_ranges[level];
    if (range.min_value <= Current_alert_value && Current_alert_value <= range.max_value)
    {
        return 1.0f;
    }
    float low;
    float high;
    if (range.min_value <= Current_alert_value)
    {
        // Above this level's solid range: filling towards the next.
        low = range.max_value;
        high = range.upper_bound;
        if (high <= Current_alert_value)
        {
            return 1.0f;
        }
    }
    else
    {
        // Below it: filling up from the previous level.
        low = range.lower_bound;
        high = range.min_value;
        if (Current_alert_value <= low)
        {
            return 0.0f;
        }
    }
    float pct = (Current_alert_value - low) / (high - low);
    if (pct - 0.0f < 0.0f)
    {
        pct = 0.0f;
    }
    if (0.0f <= pct - 1.0f)
    {
        pct = 1.0f;
    }
    return pct;
}

// 0x0075D810 (:1156)
gfm_alert_levels gsm_find_alert_fill_value()
{
    const gfm_alert_levels level = gsm_get_alert_level();
    return Alert_level_ranges[level].max_value < Current_alert_value ? (gfm_alert_levels)(level + 1) : level;
}

// 0x0075D850 (:1174)
int gsm_time_since_last_alert_change(human_teams team)
{
    return team != HUMAN_TEAM_EDF ? -1 : Last_alert_level_change.time_since();
}

// 0x0078B6F0 (:1184)
void alm_report_damage(gfm_damage_report& report)
{
    if (Alert_level_grace_timer.valid() || !(report.get_damage_level() < 1.0f))
    {
        return;
    }
    const alert_value_parameters* params = get_alert_parameters(report.type);
    if (params == nullptr)
    {
        return;
    }
    if (!is_near_a_player(report.handle))
    {
        report.update_reported_damage();
        return;
    }
    if (!alert_level_is_alerted())
    {
        // While green only a structure taking a large enough hit counts.
        if (report.type != OT_MOVER || Green_building_damage_thresh > report.get_damage_delta())
        {
            report.reset_report_delay();
            return;
        }
    }
    const float total = get_total_alert_value(report.get_damage_level(), report, params) - get_total_alert_value(report.get_reported_damage_level(), report, params);
    const float value = alert_value_apply_multiplier(total);
    if (value <= 0.0f)
    {
        return;
    }
    // rfg.exe calls the multiplier a second time here and drops the result.
    Next_alert_delta += value;
    set_last_hostile_action_pos(player_get_local_player()->pos);
    report.update_reported_damage();
    reinforcement_report_damage_pos(report.handle);
}

// 0x0078B880 (:1251)
void alm_report_death(gfm_damage_report& report)
{
    if (Alert_level_grace_timer.valid())
    {
        return;
    }
    const object* objp = World.object_from_handle(report.handle);
    if (objp == nullptr || !(player_find_shortest_player_xz_dist_sqrd(objp->pos) < ALERT_REPORT_DIST_SQRD))
    {
        report.update_reported_damage();
        return;
    }
    const alert_value_parameters* params = get_alert_parameters(report.type);
    if (params == nullptr)
    {
        return;
    }
    if (!alert_level_is_alerted() && report.type != OT_MOVER && report.type != OT_VEHICLE)
    {
        report.reset_report_delay();
        return;
    }
    const int importance = (int)report.importance_level;
    if (importance <= 0 || importance >= 4)
    {
        return;
    }
    const float death_value = params->imp_multipliers[importance] * params->death_value * difficulty_get_value(DIFFICULTY_ENEMY_ALERT, NUM_DIFFICULTY_LEVELS);
    const float total = get_total_alert_value(report.get_damage_level(), report, params) - get_total_alert_value(report.get_reported_damage_level(), report, params);
    const float value = alert_value_apply_multiplier(total);
    if (0.0f >= value)
    {
        return;
    }
    Next_alert_delta = value + death_value + Next_alert_delta;
    set_last_hostile_action_pos(player_get_local_player()->pos);
    report.update_reported_damage();
    reinforcement_report_damage_pos(report.handle);
}

// 0x007AA800 (:1309)
void gsm_do_frame()
{
    if (Multiplayer || Wrecking_crew_state != WCS_NONE)
    {
        return;
    }
    if (Alert_level_grace_timer.elapsed())
    {
        Alert_level_grace_timer.invalidate();
    }
    const int time_elapsed = Do_frame_period.time_since();
    if (time_elapsed < ALERT_DO_FRAME_PERIOD_MS)
    {
        return;
    }
    Do_frame_period.set(0);
    if (Next_alert_delta > 0.0f || has_los_to_enemy_player() || Reset_decay_timer)
    {
        Reset_decay_timer = false;
        Last_los_time.set(Alert_level_decay_delay_ms);
    }
    const float decay_delta = get_decay_delta(time_elapsed);
    const float los_delta = get_los_delta(time_elapsed);
    const float dist_delta = get_dist_delta();
    // rfg.exe's sound for a falling level is gone: the flag is all that is left of it.
    Played_alert_decrease = decay_delta > 0.0f;
    if (!activity_lock_alert_level())
    {
        float delta = Next_alert_delta + los_delta - decay_delta - dist_delta;
        // No decay while the player stands in a restricted area or drives a tank or walker.
        if (delta < 0.0f && ai_defense_is_human_in_restricted_area(player_get_local_player(), true) != nullptr)
        {
            delta = 0.0f;
        }
        const player* pp = player_get_local_player();
        if (delta < 0.0f && pp->vehicle_handle != 0xFFFFFFFF)
        {
            const vehicle* vp = World.object_from_handle<vehicle>(pp->vehicle_handle);
            // Magic number: vehicle subtype 6 is a walker.
            if (vp != nullptr && (vehicle_is_tank(vp) || vp->get_sub_type() == 6))
            {
                delta = 0.0f;
            }
        }
        if ((!Ratchet_alert_level || delta >= 0.0f) && delta != 0.0f)
        {
            gsm_set_alert_value(Current_alert_value + delta);
        }
    }
    Next_alert_delta = 0.0f;
    if (Alert_in_one_second.valid())
    {
        if (alert_level_is_alerted())
        {
            init_alerted_handles();
        }
        else
        {
            int i = 0;
            while (i < Num_alerted_handles)
            {
                object* objp = Alerted_handles[i] != 0xFFFFFFFF ? World.object_from_handle(Alerted_handles[i]) : nullptr;
                // Magic number: subtype 2 is an npc.
                if (objp != nullptr && objp->get_obj_type() == OT_HUMAN && objp->get_sub_type() == 2 && !objp->is_killed())
                {
                    i++;
                    continue;
                }
                // rfg.exe's REP MOVSD copies one element more, the one past the live count, which is read past the end
                // of the array when it is full. That element is never read again, so it is left out here.
                for (int j = i; j < Num_alerted_handles - 1; j++)
                {
                    Alerted_handles[j] = Alerted_handles[j + 1];
                }
                Num_alerted_handles--;
            }
            if (Num_alerted_handles == 0)
            {
                Alert_in_one_second.invalidate();
            }
            else if (Alert_in_one_second.elapsed())
            {
                gsm_set_alert_value(Alert_level_ranges[ALERT_LEVEL_YELLOW].min_value);
            }
        }
    }
    if (stats_cheat_enabled(CHEATS_ALWAYS_YELLOW_ALERT) && ((int)Current_alert_level <= -1 || ((int)Current_alert_level < 4 && Current_alert_level == ALERT_LEVEL_GREEN)))
    {
        gsm_set_alert_value(Alert_level_ranges[ALERT_LEVEL_YELLOW].min_value);
    }
    evaluate_alert_level(false);
}

// 0x0079C650 (:1438)
void alm_report_human_death(human* dead_hp)
{
    if (dead_hp->hflags.death_reported != 0)
    {
        return;
    }
    dead_hp->hflags.death_reported = 1;
    if (Alert_level_grace_timer.valid())
    {
        return;
    }
    if (dead_hp->is_killed() && (int)Current_alert_level > -1 && ((int)Current_alert_level >= 4 || (int)Current_alert_level >= 1) && should_report_human(dead_hp))
    {
        report_human_death(dead_hp);
    }
}

// 0x0079C6B0 (:1469)
void alm_report_vehicle_damage(vehicle* vp, const human* attacker_hp)
{
    if (Alert_level_grace_timer.valid())
    {
        vp->update_last_damage_reported();
        return;
    }
    if (!should_report_vehicle_damage(vp))
    {
        return;
    }
    if (human_teams_get_disposition(attacker_hp, HUMAN_TEAM_EDF) <= TEAM_DISPOSITION_NONE)
    {
        report_vehicle_damage(vp);
    }
    vp->update_last_damage_reported();
}

// 0x0079C710 (:1490)
void alm_report_vehicle_death(vehicle* vp, const human* attacker_hp)
{
    if (Alert_level_grace_timer.valid())
    {
        vp->m_flags.death_reported = 1;
        return;
    }
    if (!should_report_vehicle_death(vp))
    {
        return;
    }
    if (human_teams_get_disposition(attacker_hp, HUMAN_TEAM_EDF) > TEAM_DISPOSITION_NONE)
    {
        // Original quirk kept: killed by its own side, so death_reported stays clear and the next call comes back here.
        vp->update_last_damage_reported();
        return;
    }
    report_vehicle_death(vp);
    vp->m_flags.death_reported = 1;
}

// 0x0075D870 (:1514)
gfm_alert_levels alm_resolve_alert_level(const char* alert_string)
{
    if (alert_string == nullptr)
    {
        return ALERT_LEVEL_INVALID;
    }
    for (int i = 0; i < NUM_ALERT_LEVELS; i++)
    {
        if (v_stricmp(alert_string, Alert_level_type_tags[i]) == 0)
        {
            return (gfm_alert_levels)i;
        }
    }
    return ALERT_LEVEL_INVALID;
}

// 0x0075D8B0 (:1530)
void alm_set_alert_level_ratcheting(bool on)
{
    Ratchet_alert_level = on;
}

// 0x0075D8C0 (:1535)
void set_last_hostile_action_pos(vector pos)
{
    if (!Alert_level_grace_timer.valid())
    {
        Last_hostile_action_pos = pos;
        Last_hostile_action_pos_active = true;
    }
}

// 0x0075D8F0 (:1545)
void gsm_raise_the_alert_level_in_one_second_if_not_dead(unsigned int alerted_handle)
{
    if (Alert_level_grace_timer.valid() || alert_level_is_alerted())
    {
        return;
    }
    if (Num_alerted_handles < MAX_ALERTED_HANDLES)
    {
        Alerted_handles[Num_alerted_handles] = alerted_handle;
        Num_alerted_handles++;
    }
    if (!Alert_in_one_second.valid())
    {
        Alert_in_one_second.set(1000);
    }
}

// 0x0075D950 (:1565)
bool alm_grace_is_active(const human* hp, const human* target)
{
    if (!Alert_level_grace_timer.valid())
    {
        return false;
    }
    if (hp->current_team == HUMAN_TEAM_EDF && human_teams_get_disposition(target, HUMAN_TEAM_GUERILLA) > TEAM_DISPOSITION_NONE)
    {
        return true;
    }
    return target->current_team == HUMAN_TEAM_EDF && human_teams_get_disposition(hp, HUMAN_TEAM_GUERILLA) > TEAM_DISPOSITION_NONE;
}
