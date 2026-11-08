// Original: volition/rfg/code/interface/ui_document_hud_missions.cpp
//
// The mission and activity corner of the single player HUD (rfg_ui_hud_missions): the mission title and objective, the
// status line of a running activity, diversions, the script timer and the hostage icons. It fills vint data items the
// document's Lua reads. The activity queries it calls are gameflow.md GF4's and answer zero until then, so only the
// mission half does anything today.
//
// LTCG: several hud_ui_missions_get_* functions take their first string in a register and drop parameters that are
// constants in their only caller. The signatures below are the PDB's.

#include "volition/rfg/code/interface/ui_document_hud_missions.h"

#include "volition/audio_lib/code/audiolib_api.h"
#include "volition/interface_lib/code/vint_data_group.h"
#include "volition/interface_lib/code/vint_data_item.h"
#include "volition/interface_lib/code/vint_variant.h"
#include "volition/rfg/code/gameplay/activities/activity.h"
#include "volition/rfg/code/gameplay/activities/activity_area_defense.h"
#include "volition/rfg/code/gameplay/activities/activity_convoy.h"
#include "volition/rfg/code/gameplay/activities/activity_courier_chase.h"
#include "volition/rfg/code/gameplay/activities/activity_courier_chase_internal.h"
#include "volition/rfg/code/gameplay/activities/activity_delivery.h"
#include "volition/rfg/code/gameplay/activities/activity_demolitions_master_internal.h"
#include "volition/rfg/code/gameplay/activities/activity_house_arrest.h"
#include "volition/rfg/code/gameplay/activities/activity_house_arrest_internal.h"
#include "volition/rfg/code/gameplay/activities/activity_raid.h"
#include "volition/rfg/code/gameplay/activities/activity_vrail.h"
#include "volition/rfg/code/gameplay/missions.h"
#include "volition/rfg/code/gameplay/upgrades.h"
#include "volition/rfg/code/gamewide/game_clock.h"
#include "volition/rfg/code/gamewide/gameaudio.h"
#include "volition/rfg/code/interface/ui_document_manager.h"
#include "volition/rfg/code/level/world.h"
#include "volition/rfg/code/objects/humans/npc.h"
#include "volition/rfg/code/objects/humans/player.h"
#include "volition/vlib/code/math/floating.h"
#include "volition/vlib/code/math/vector3.h"
#include "volition/vlib/code/util/string_util.h"
#include "volition/vlib/code/util/unicode_tools.h"

#include "keen/base/string.hpp"
#include "keen/base/trace.hpp"

#include <string.h>

// size 0x74
struct activity_info_set
{
    bool m_show_mission_activity_screen; // 0x00
    int m_last_activity_state; // 0x04
    int m_activity_state; // 0x08
    farray<squad_health_info,3> m_health_info; // 0x0C
    char m_mission_caption[64]; // 0x34
};

// size 0x2C
struct hud_hostage_info
{
    int data_item_handle; // 0x00
    int num_hostages; // 0x04
    unsigned int hostage_handles[3]; // 0x08
    int hostage_status[3]; // 0x14
    float last_hostage_health[3]; // 0x20
};

// size 0xC1
struct objective_complete_info
{
    char m_title[64]; // 0x00
    char m_caption[64]; // 0x40
    char m_icon[64]; // 0x80
    bool m_updated; // 0xC0
};

// size 0x8C
struct hud_timer_info
{
    char time_string[132]; // 0x00
    int data_item_handle; // 0x84
    bool visible; // 0x88
    bool updated; // 0x89
};

// size 0x874
struct diversion_group_info
{
    int data_group_handle; // 0x00
    farray<diversion_data,8> data; // 0x04
};

// The activity's instance. gameflow.md GF4 defines it with the activity; src/stubs holds zero storage until then.
extern activity_courier_chase_data Courier_chase_data; // 0x02242BA8

// ---- file-static globals
static const char* cUI_Child_Hud_Missions = "rfg_ui_hud_missions"; // 0x01664910
static const char* cBlankString_uimis = ""; // 0x01664914
static const char* cComplete_uimis = "MISS_COMPLETE_MALE"; // 0x0166491C
static const char* cFailed = "MISS_FAILED"; // 0x01664920
static const char* cEscaped = "MISS_ESCAPED"; // 0x01664924
static const char* cTrueString = "true"; // 0x01664928
static const char* cDiversion_started = "MISS_DIVERSION_STARTED"; // 0x0166492C
static const char* cDiversion_ended = "MISS_DIVERSION_ENDED"; // 0x01664930
static const char* cMayhem_started = "MISS_DIVERSION_STARTED_FEMININE"; // 0x01664934
static const char* cMayhem_ended = "MISS_DIVERSION_ENDED_FEMININE"; // 0x01664938
static const char* cBuildingDestroyed = "MISS_DESTROYED"; // 0x0166493C
static const char* cObjective = "HM_MAP_OBJECTIVE"; // 0x01664940
static const char* cObjectiveImg = "ui_map_icon_objective"; // 0x01664944
static const char* cBuildingDefaultName = "MISS_DEFAULT_BUILDING_NAME"; // 0x01664948
static const char* cBuildingMediumImg = "ui_map_icon_pmedium"; // 0x0166494C
static const char* cBuildingHighImg = "ui_map_icon_phigh"; // 0x01664950
static const float Fade_diversion_time_ms = 6000.0f; // 0x013007F4
static float Status_time_left = -1.0f; // 0x01664954
static int Doc_handle_uimis = -1; // 0x01664958
static int Data_handles_uimis[3]; // 0x0166495C: mission info, mission status, objective status
static activity_info_set Activity_set; // 0x01666FE0
static hud_hostage_info Hud_hostage_info; // 0x02BF53F4
static objective_complete_info Objective_info; // 0x02C00098
static hud_timer_info Hud_timer_info; // 0x02C02BB8
static bool Assets_loaded_uimis; // 0x02C08038
static bool Active_uimis; // 0x02C08039
static bool Data_items_created_uimis; // 0x02C0803A
static diversion_group_info Diversion_info; // 0x02C52AF0

// Magic number: a hostage's health before it has one to compare against, 2^32.
static const float Hostage_health_unset = 4294967296.0f;
// Magic number: 6 seconds, how long an activity's result and a diversion stay on the screen.
static const double Activity_result_time_ms = 6000.0;
// Magic number: the diversion's "started" text shows for its first second.
static const int Diversion_started_time_ms = 1000;
// Magic number: the 0.001f the asm widens to a double to turn milliseconds into seconds.
static const double Ms_to_s = (double)0.001f;

// 0x00895940 (:60)
squad_health_info::squad_health_info()
{
    handle = 0;
    health_pct = 0.0f;
}

// 0x00895960 (:61)
squad_health_info::squad_health_info(const squad_health_info& shi)
{
    handle = shi.handle;
    health_pct = shi.health_pct;
}

// 0x00895980 (:93)
diversion_data::diversion_data(bool is_ended, int key, vint_data_item* vdi, const char* diversion_name, const char* diversion_value)
{
    ended = is_ended;
    diversion_key = key;
    data_item = vdi;
    if (diversion_name != nullptr)
    {
        keen::formatString(name, 0x80, diversion_name);
    }
    if (diversion_value != nullptr)
    {
        keen::formatString(value, 0x80, diversion_value);
    }
}

// 0x008959E0 (:201)
static void hud_ui_missions_init_activity_info_set()
{
    Activity_set.m_show_mission_activity_screen = false;
    Activity_set.m_activity_state = -1;
    Activity_set.m_last_activity_state = -1;
}

// 0x008B21B0 (:218)
static void hud_ui_missions_clear_diversion_info()
{
    Diversion_info.data_group_handle = 0;
    Diversion_info.data.clear();
}

// 0x00895A00 (:225)
static void hud_ui_missions_clear_objective_complete_info()
{
    strncpy(Objective_info.m_title, cObjective, 0x40);
    strncpy(Objective_info.m_caption, cComplete_uimis, 0x40);
    strncpy(Objective_info.m_icon, cObjectiveImg, 0x40);
    Objective_info.m_updated = false;
}

// 0x00895A50 (:233)
static void hud_ui_missions_clear_hostage_info()
{
    Hud_hostage_info.num_hostages = 0;
    for (int i = 0; i < 3; i++)
    {
        Hud_hostage_info.last_hostage_health[i] = Hostage_health_unset;
        Hud_hostage_info.hostage_handles[i] = 0xFFFFFFFF;
        Hud_hostage_info.hostage_status[i] = -1;
    }
}

// 0x00895AA0 (:246)
static bool hud_ui_missions_activity_has_interface(activity_modes mode)
{
    switch (mode)
    {
    case ACTIVITY_MODE_VEHICLE_RAIL:
    case ACTIVITY_MODE_DELIVERY:
    case ACTIVITY_MODE_COURIER:
    case ACTIVITY_MODE_RAID_DEFEND:
    case ACTIVITY_MODE_RAID_ATTACK:
    case ACTIVITY_MODE_CONVOY:
    case ACTIVITY_MODE_AREA_DEFENSE:
        return true;
    case ACTIVITY_MODE_HOUSE_ARREST:
        return activity_house_arrest_has_interface();
    case ACTIVITY_MODE_DEMOLITIONS_MASTER:
        return demolitions_master_has_interface();
    default:
        return false;
    }
}

// 0x00895AF0 (:276)
static bool hud_ui_diversion_has_interface(activity_modes mode)
{
    return mode != ACTIVITY_MODE_MISSION_VRAIL && mode != ACTIVITY_MODE_CONTINUOUS_ATTACK;
}

// 0x00895B00 (:286)
static void hud_ui_missions_set_caption(const char* caption)
{
    if (caption == nullptr)
    {
        caption = cBlankString_uimis;
    }
    strncpy_safe(Activity_set.m_mission_caption, caption, 0x40);
}

// 0x00895B20 (:297)
void hud_ui_missions_complete_objective()
{
    strncpy(Objective_info.m_title, cObjective, 0x40);
    strncpy(Objective_info.m_caption, cComplete_uimis, 0x40);
    strncpy(Objective_info.m_icon, cObjectiveImg, 0x40);
    Objective_info.m_updated = true;
    audiolib_cue_play(Ga_cue_ids.mission_complete_text_hit, nullptr, nullptr);
}

// 0x00895B80 (:308)
void hud_ui_mission_set_building_destroyed(const char* building_name, object_importance_level building_importance, bool show_icon)
{
    // Magic number: below 2 (OBJECT_IMPORTANCE_LEVEL_MEDIUM per this file's icons), nothing shows.
    if ((int)building_importance < 2)
    {
        return;
    }
    Objective_info.m_updated = true;
    if (*building_name == '\0')
    {
        building_name = cBuildingDefaultName;
    }
    strncpy(Objective_info.m_title, building_name, 0x40);
    strncpy(Objective_info.m_caption, cBuildingDestroyed, 0x40);
    if (show_icon && !(mission_is_active() && v_stricmp(mission_get_name(), "Tutorial") == 0))
    {
        strncpy(Objective_info.m_icon, building_importance == OBJECT_IMPORTANCE_LEVEL_HIGH ? cBuildingHighImg : cBuildingMediumImg, 0x40);
    }
    else
    {
        Objective_info.m_icon[0] = '\0';
    }
    audiolib_cue_play(Ga_cue_ids.target_destroyed, nullptr, nullptr);
}

// 0x00895C40 (:330)
static void hud_ui_missions_process_activity_status(float dtime)
{
    (void)dtime;
    vint_data_item* item = vint_data_item::find(Data_handles_uimis[1]);
    if (item == nullptr)
    {
        return;
    }
    bool keep_last_status = false;
    int status_icon = -1;
    char status_text[64];
    keen::formatString(status_text, 0x40, cBlankString_uimis);
    const int mode = Activity_set.m_activity_state;
    if (!Activity_set.m_show_mission_activity_screen || mission_is_active() || mode == 0 || mode == -1)
    {
        Status_time_left = -1.0f;
    }
    else
    {
        const activity_interface_states state = activity_get_interface_mode((activity_modes)mode);
        if (state == ACTIVITY_INTERFACE_STATE_VICTORY)
        {
            keen::formatString(status_text, 0x40, cComplete_uimis);
            if (Status_time_left < 0.0f && !game_is_paused())
            {
                const float duration = (float)activity_interface_mode_duration((activity_modes)mode);
                Status_time_left = (float)((Activity_result_time_ms - (double)duration) * Ms_to_s);
            }
            switch (mode)
            {
            case ACTIVITY_MODE_COURIER:
            case ACTIVITY_MODE_RAID_DEFEND:
            case ACTIVITY_MODE_RAID_ATTACK:
            case ACTIVITY_MODE_HOUSE_ARREST:
            case ACTIVITY_MODE_CONVOY:
            case ACTIVITY_MODE_AREA_DEFENSE:
                status_icon = 1;
                break;
            default:
                break;
            }
        }
        else if (state == ACTIVITY_INTERFACE_STATE_LOSS)
        {
            const char* text = cFailed;
            if (mode == ACTIVITY_MODE_COURIER && activity_cc_most_recent_courier_escaped())
            {
                text = cEscaped;
            }
            keen::formatString(status_text, 0x40, text);
            if (Status_time_left < 0.0f && !game_is_paused())
            {
                const float duration = (float)activity_interface_mode_duration((activity_modes)mode);
                Status_time_left = (float)((Activity_result_time_ms - (double)duration) * Ms_to_s);
            }
            if (mode == ACTIVITY_MODE_COURIER || mode == ACTIVITY_MODE_CONVOY)
            {
                status_icon = 0;
            }
        }
        else if (Activity_set.m_last_activity_state != 0 && Activity_set.m_last_activity_state != -1)
        {
            keep_last_status = true;
        }
    }
    str_make_upper(status_text);
    if (!keep_last_status)
    {
        item->set_element(0, vint_variant(status_icon));
        item->set_element(1, vint_variant(status_text));
        item->set_element(2, vint_variant(Status_time_left));
    }
}

// 0x00895ED0 (:454)
bool hud_ui_activity_interface_active()
{
    return Status_time_left > 0.0f;
}

// 0x00895EF0 (:460)
void hud_ui_missions_get_default_mission_info(char* mission_title, unsigned int mission_title_size, char* mission_caption, unsigned int mission_caption_size)
{
    const char* description = mission_get_description();
    if (description != nullptr)
    {
        keen::formatString(mission_title, mission_title_size, description);
    }
    keen::formatString(mission_caption, mission_caption_size, Activity_set.m_mission_caption);
}

// 0x00895F30 (:479)
static bool hud_ui_missions_get_partytime_mission_info(char* mission_title, unsigned int mission_title_size, char* mission_caption, unsigned int mission_caption_size, char* distance, unsigned int distance_size, char* to_close, unsigned int to_close_size, char* to_far, unsigned int to_far_size)
{
    const char* description = mission_get_description();
    if (description != nullptr)
    {
        strncpy_safe(mission_title, description, mission_title_size);
    }
    keen::formatString(mission_caption, mission_caption_size, Activity_set.m_mission_caption);
    const mission_data* data = mission_get_data();
    if (data == nullptr || !data->valid)
    {
        return false;
    }
    const object* target = World.object_from_handle(data->handle1);
    if (target == nullptr)
    {
        return false;
    }
    const player* pp = player_get_local_player();
    if (pp == nullptr)
    {
        return false;
    }
    const float dist = vec_xz_dist(target->pos, pp->pos);
    keen::formatString(distance, distance_size, "%.0f", (double)dist);
    if (fl_is_nan(data->fl_value1) || fl_is_nan(data->fl_value2))
    {
        return false;
    }
    if (data->fl_value1 > dist)
    {
        keen::formatString(to_close, to_close_size, "%s", cTrueString);
    }
    else if (dist > data->fl_value2)
    {
        keen::formatString(to_far, to_far_size, "%s", cTrueString);
    }
    return true;
}

// 0x00896060 (:537)
static bool hud_ui_missions_get_fmc_mission_info(char* mission_title, unsigned int mission_title_size, char* mission_caption, unsigned int mission_caption_size, char* distance, unsigned int distance_size)
{
    const mission_data* data = mission_get_data();
    if (data == nullptr || !data->valid)
    {
        return false;
    }
    const object* target = World.object_from_handle(data->handle1);
    if (target == nullptr)
    {
        return false;
    }
    const player* pp = player_get_local_player();
    if (pp == nullptr)
    {
        return false;
    }
    const float dist = vec_xz_dist(target->pos, pp->pos);
    keen::formatString(distance, distance_size, "%.0f", (double)dist);
    hud_ui_missions_get_default_mission_info(mission_title, mission_title_size, mission_caption, mission_caption_size);
    return true;
}

// 0x008960E0 (:574)
static void hud_ui_missions_process_missions_info(float dtime)
{
    (void)dtime;
    static int last_mission_state = 0; // 0x02CA14BC
    vint_data_item* item = vint_data_item::find(Data_handles_uimis[0]);
    if (item == nullptr)
    {
        return;
    }
    int state = 0;
    char title[64];
    char caption[64];
    char distance[64];
    char to_close[64];
    char to_far[64];
    distance[0] = '\0';
    to_close[0] = '\0';
    to_far[0] = '\0';
    title[0] = '\0';
    caption[0] = '\0';
    Activity_set.m_show_mission_activity_screen = mission_is_active();
    if (Activity_set.m_show_mission_activity_screen)
    {
        const mission_mode mode = mission_get_mode();
        if (mode == MISSION_MODE_PARTYTIME && hud_ui_missions_get_partytime_mission_info(title, 0x40, caption, 0x40, distance, 0x40, to_close, 0x40, to_far, 0x40))
        {
            state = -2;
        }
        else if (mode == MISSION_MODE_FMC && hud_ui_missions_get_fmc_mission_info(title, 0x40, caption, 0x40, distance, 0x40))
        {
            state = -2;
        }
        else if (mode != MISSION_MODE_CUTSCENE)
        {
            state = -1;
            hud_ui_missions_get_default_mission_info(title, 0x40, caption, 0x40);
        }
    }
    if (last_mission_state != state)
    {
        if (state != 0 && strstr(title, "MISS_TUT") == nullptr)
        {
            audiolib_cue_play(Ga_cue_ids.mission_heading, nullptr, nullptr);
        }
        last_mission_state = state;
    }
    item->set_element(0, vint_variant(state));
    item->set_element(1, vint_variant(title));
    item->set_element(2, vint_variant(caption));
    item->set_element(3, vint_variant(0.0f));
    item->set_element(4, vint_variant(distance));
    item->set_element(5, vint_variant(to_close));
    item->set_element(6, vint_variant(to_far));
}

// 0x00896380 (:672)
static int hud_ui_missions_get_lua_diversion_type(int actual_diversion_type)
{
    switch (actual_diversion_type)
    {
    case ACTIVITY_MODE_KILLING_SPREE:
        return 3;
    case ACTIVITY_MODE_MAYHEM:
        return 1;
    case ACTIVITY_MODE_CAR_BOMB:
        return 5;
    default:
        return 0;
    }
}

// 0x008C5490 (:705)
static void hud_ui_missions_determine_diversion_items(vint_data_group* dg)
{
    if (dg == nullptr)
    {
        return;
    }
    int i = 0;
    while (i < Diversion_info.data.size())
    {
        diversion_data& data = Diversion_info.data[i];
        const activity_modes mode = (activity_modes)data.diversion_key;
        if (activity_is_active(mode))
        {
            if (data.ended)
            {
                data.ended = false;
            }
            i++;
        }
        else if (activity_interface_mode_duration(mode) < 1001) // Magic number: a second after it ended, it goes.
        {
            data.ended = true;
            i++;
        }
        else
        {
            if (data.data_item != nullptr)
            {
                dg->remove_data_item(data.data_item->get_handle());
            }
            Diversion_info.data.remove(i);
        }
    }
    for (int m = ACTIVITY_MODE_VEHICLE_RAIL; m < NUM_OF_ACTIVITY_MODES; m++)
    {
        const activity_modes mode = (activity_modes)m;
        if (!activity_is_diversion(mode) || !hud_ui_diversion_has_interface(mode) || activity_get_interface_mode(mode) != ACTIVITY_INTERFACE_STATE_RUNNING)
        {
            continue;
        }
        bool found = false;
        for (int j = 0; j < Diversion_info.data.size(); j++)
        {
            if (Diversion_info.data[j].diversion_key == m)
            {
                found = true;
                break;
            }
        }
        if (found)
        {
            continue;
        }
        vint_data_item* item = dg->add_data_item(nullptr);
        if (item != nullptr)
        {
            static const vint_variant_type types[7] = {VINT_PROP_TYPE_INT, VINT_PROP_TYPE_INT, VINT_PROP_TYPE_BOOL, VINT_PROP_TYPE_BOOL, VINT_PROP_TYPE_STRING, VINT_PROP_TYPE_STRING, VINT_PROP_TYPE_STRING};
            item->set_template(types, 7);
        }
        Diversion_info.data.add(diversion_data(false, m, item, activity_get_name(mode), nullptr));
    }
}

// 0x008C56A0 (:807)
static void hud_ui_missions_process_diversion_info(float dtime)
{
    (void)dtime;
    vint_data_group* dg = vint_data_group::find(Diversion_info.data_group_handle);
    if (dg == nullptr)
    {
        return;
    }
    hud_ui_missions_determine_diversion_items(dg);
    const int count = Diversion_info.data.size();
    for (int i = 0; i < count; i++)
    {
        diversion_data& data = Diversion_info.data[i];
        const activity_modes mode = (activity_modes)data.diversion_key;
        vint_data_item* item = data.data_item;
        const int lua_type = hud_ui_missions_get_lua_diversion_type(mode);
        bool fade = false;
        bool started = false;
        if (item == nullptr)
        {
            continue;
        }
        char status[128];
        const activity_interface_states state = activity_get_interface_mode(mode);
        keen::formatString(status, 0x80, cBlankString_uimis);
        if (state == ACTIVITY_INTERFACE_STATE_RUNNING)
        {
            if (activity_get_name(mode) == nullptr)
            {
                keen::formatString(data.name, 0x80, cBlankString_uimis);
            }
            const int duration = activity_interface_mode_duration(mode);
            if (duration < 1)
            {
                keen::formatString(data.value, 0x80, cBlankString_uimis);
            }
            int time_til_end;
            activity_get_ui_text(data.value, time_til_end, 0x80, mode);
            if ((float)time_til_end < Fade_diversion_time_ms)
            {
                fade = true;
            }
            if (mode == ACTIVITY_MODE_CAR_BOMB)
            {
                if (duration < Diversion_started_time_ms)
                {
                    started = true;
                    keen::formatString(status, 0x80, "ACT_BOMB_INSTALLED");
                }
                if (data.value[0] == 't') // Magic number: the bomb's ui text is "true" once it is armed (see the blank below).
                {
                    keen::formatString(status, 0x80, "ACT_BOMB_ARMED");
                }
                keen::formatString(data.value, 0x80, cBlankString_uimis);
            }
            else if (mode == ACTIVITY_MODE_MAYHEM || mode == ACTIVITY_MODE_KILLING_SPREE)
            {
                if (duration < Diversion_started_time_ms)
                {
                    started = true;
                    keen::formatString(status, 0x80, cMayhem_started);
                }
            }
            else if (duration < Diversion_started_time_ms)
            {
                started = true;
                keen::formatString(status, 0x80, cDiversion_started);
            }
        }
        else if (mode != ACTIVITY_MODE_CAR_BOMB)
        {
            keen::formatString(status, 0x80, mode == ACTIVITY_MODE_MAYHEM || mode == ACTIVITY_MODE_KILLING_SPREE ? cMayhem_ended : cDiversion_ended);
        }
        str_make_upper(status);
        str_make_upper(data.value);
        item->set_element(0, vint_variant(i + 1));
        item->set_element(1, vint_variant(lua_type));
        item->set_element(2, vint_variant(started));
        item->set_element(3, vint_variant(fade));
        item->set_element(4, vint_variant(data.name));
        item->set_element(5, vint_variant(status));
        item->set_element(6, vint_variant(data.value));
    }
}

// 0x008963B0 (:998)
static void hud_ui_missions_get_activity_raid_info(float& float_value, char* mission_title, unsigned int mission_title_size, char* mission_caption, unsigned int mission_caption_size, char* num_guerrillas, unsigned int num_guerrillas_size, char* num_edf, unsigned int num_edf_size, char* show_meter, unsigned int show_meter_size)
{
    if (mission_title == nullptr || mission_caption == nullptr)
    {
        return;
    }
    const activity_modes mode = (activity_modes)Activity_set.m_activity_state;
    const char* title = activity_title(mode);
    keen::formatString(mission_title, mission_title_size, title != nullptr ? title : "MISS_RAID");
    const char* subtitle = activity_subtitle(mode);
    keen::formatString(mission_caption, mission_caption_size, subtitle != nullptr ? subtitle : "MISS_RAID_INFO");
    int friendly;
    int enemy;
    activity_raid_get_team_totals(friendly, enemy);
    keen::formatString(num_guerrillas, num_guerrillas_size, "%i", friendly);
    keen::formatString(num_edf, num_edf_size, "%i", enemy);
    keen::formatString(show_meter, show_meter_size, "%c", activity_is_active(mode) ? 'T' : 'F');
    // Double chain per the asm. 0 of 0 is NaN, and the first test turns it into 0.
    float pct = (float)((double)(float)friendly / (double)(float)(friendly + enemy) * 100.0);
    if (!((float)((double)pct - 0.0) >= 0.0f))
    {
        pct = 0.0f;
    }
    if ((float)((double)pct - 100.0) >= 0.0f)
    {
        pct = 100.0f;
    }
    float_value = pct;
}

// 0x008C5A60 (:1041)
static void hud_ui_missions_get_activity_house_arrest_info(float& float_value, char* mission_title, unsigned int mission_title_size, char* mission_caption, unsigned int mission_caption_size, char* string1, unsigned int string1_size, char* string2, unsigned int string2_size, char* string3, unsigned int string3_size, float dtime)
{
    (void)float_value;
    (void)dtime;
    if (mission_title == nullptr || mission_caption == nullptr || string1 == nullptr)
    {
        return;
    }
    const activity_modes mode = (activity_modes)Activity_set.m_activity_state;
    const char* title = activity_title(mode);
    keen::formatString(mission_title, mission_title_size, title != nullptr ? title : "MISS_HOUSE_ARREST");
    const char* subtitle = activity_subtitle(mode);
    keen::formatString(mission_caption, mission_caption_size, subtitle != nullptr ? subtitle : "MISS_HOUSE_ARREST_INFO");
    farray<hostage_info,3> hostages;
    activity_house_arrest_hostage_status(hostages);
    farray<squad_health_info,3> health;
    const int num_hostages = hostages.size();
    for (int i = 0; i < 3; i++)
    {
        unsigned int handle = 0;
        int status = -1;
        float health_pct = 0.0f;
        if (i < num_hostages && hostages[i].np != nullptr)
        {
            const npc* np = hostages[i].np;
            status = hostages[i].status;
            handle = np->get_handle();
            health_pct = (float)((double)np->hit_points / (double)(float)np->max_hit_points);
            for (int j = 0; j < Activity_set.m_health_info.size(); j++)
            {
                if (Activity_set.m_health_info[j].handle == handle)
                {
                    // Magic number: the same "hurt" status as hud_ui_missions_process_hud_hostage.
                    if (Activity_set.m_health_info[j].health_pct > health_pct)
                    {
                        status = 5;
                    }
                    break;
                }
            }
        }
        if (i == 0)
        {
            keen::formatString(string1, string1_size, "%d", status);
        }
        else if (i == 1)
        {
            keen::formatString(string2, string2_size, "%d", status);
        }
        else
        {
            keen::formatString(string3, string3_size, "%d", status);
        }
        squad_health_info info;
        info.handle = handle;
        info.health_pct = health_pct;
        health.add(info);
    }
    Activity_set.m_health_info.clear();
    for (int i = 0; i < health.size(); i++)
    {
        Activity_set.m_health_info.add(health[i]);
    }
}

// 0x00896510 (:1139)
static void hud_ui_missions_get_activity_demolitions_master_info(float& float_value, char* mission_title, unsigned int mission_title_size, char* mission_caption, unsigned int mission_caption_size, char* string1, unsigned int string1_size, char* string2, unsigned int string2_size, char* string3, unsigned int string3_size)
{
    if (mission_title == nullptr || mission_caption == nullptr || string1 == nullptr)
    {
        return;
    }
    const char* title = activity_title((activity_modes)Activity_set.m_activity_state);
    keen::formatString(mission_title, mission_title_size, title != nullptr ? title : "MISS_DEMO_MASTER");
    const char* subtitle = demolitions_master_get_corner_hud_subtitle();
    keen::formatString(mission_caption, mission_caption_size, subtitle != nullptr ? subtitle : "MISS_DEMO_MASTER_INFO");
    const int stress = demolitions_master_get_stress_active_indicator();
    float_value = (float)stress;
    // Magic number: rounds the milliseconds up to whole seconds.
    int display_time = demolitions_master_get_display_time() + 999;
    const int minutes = display_time / 60000;
    display_time -= minutes * 60000;
    keen::formatString(string1, string1_size, "%02d:%02d", minutes, display_time / 1000);
    keen::formatString(string2, string2_size, "ACT_DM_BUILDING_STRUCTURE");
    // Magic number: a stress indicator of 2 is critical; anything else, even 0, reads unstable.
    keen::formatString(string3, string3_size, "%s", stress == 2 ? "ACT_DM_BS_CRITICAL" : "ACT_DM_BS_UNSTABLE");
}

// 0x00896630 (:1182)
static void hud_ui_missions_get_activity_courier_chase_info(float& float_value, char* mission_title, unsigned int mission_title_size, char* mission_caption, unsigned int mission_caption_size, char* string1, unsigned int string1_size, char* string2, unsigned int string2_size, char* string3, unsigned int string3_size)
{
    (void)string3_size;
    if (mission_title == nullptr || mission_caption == nullptr || string1 == nullptr)
    {
        return;
    }
    const activity_modes mode = (activity_modes)Activity_set.m_activity_state;
    const char* title = activity_title(mode);
    keen::formatString(mission_title, mission_title_size, title != nullptr ? title : "MISS_CTC");
    const char* subtitle = activity_subtitle(mode);
    keen::formatString(mission_caption, mission_caption_size, subtitle != nullptr ? subtitle : "MISS_CTC_INFO");
    const player* pp = player_get_local_player();
    const object_squad* squad = Courier_chase_data.get_courier_squad();
    if (pp != nullptr && squad != nullptr)
    {
        const float dist = vec_xz_dist(pp->pos, ((const object*)squad)->pos);
        float_value = dist;
        keen::formatString(string1, string1_size, "%i", (int)dist);
    }
    if (Courier_chase_data.out_of_bounds_timestamp.valid())
    {
        float seconds = (float)((double)(float)Courier_chase_data.out_of_bounds_timestamp.time_until() / 1000.0);
        if ((float)(0.0 - (double)seconds) >= 0.0f)
        {
            seconds = 0.0f;
        }
        keen::formatString(string2, string2_size, "%0.02f", (double)seconds);
        string3[0] = '\0';
    }
    else
    {
        string2[0] = '\0';
        string3[0] = '\0';
    }
}

// 0x00896790 (:1225)
static void hud_ui_missions_get_activity_convoy_info(float& float_value, char* mission_title, unsigned int mission_title_size, char* mission_caption, unsigned int mission_caption_size, char* string1, unsigned int string1_size, char* string2, unsigned int string2_size, char* string3, unsigned int string3_size)
{
    if (mission_title == nullptr || mission_caption == nullptr || string1 == nullptr || string2 == nullptr || string3 == nullptr)
    {
        return;
    }
    const activity_modes mode = (activity_modes)Activity_set.m_activity_state;
    const activity_interface_states state = activity_get_interface_mode(mode);
    int current = 0;
    int total = 0;
    float dist = 0.0f;
    float health = 0.0f;
    if (state == ACTIVITY_INTERFACE_STATE_RUNNING)
    {
        current = activity_convoy_get_num_current_vehicles();
        total = activity_convoy_get_total_num_start_vehicles();
        dist = activity_convoy_get_nearest_target_dist();
        health = activity_convoy_get_nearest_target_health();
        activity_convoy_get_level(); // called for nothing, as in rfg.exe
    }
    float_value = (float)(int)activity_convoy_get_type();
    const char* title = activity_title(mode);
    keen::formatString(mission_title, mission_title_size, title != nullptr ? title : "MISS_CONVOY");
    const char* subtitle = state == ACTIVITY_INTERFACE_STATE_RUNNING ? activity_subtitle(mode) : cBlankString_uimis;
    keen::formatString(mission_caption, mission_caption_size, subtitle != nullptr ? subtitle : cBlankString_uimis);
    keen::formatString(string1, string1_size, total < 100 ? "%02d/%02d" : "%03d/%03d", current, total);
    keen::formatString(string2, string2_size, "%.0f", (double)dist);
    keen::formatString(string3, string3_size, "%.0f%%", (double)health);
}

// 0x00896910 (:1283)
static void hud_ui_missions_get_activity_delivery_info(float& float_value, char* mission_title, unsigned int mission_title_size, char* mission_caption, unsigned int mission_caption_size, char* string1, unsigned int string1_size, char* string2, unsigned int string2_size)
{
    (void)float_value;
    if (mission_title == nullptr || mission_caption == nullptr || string1 == nullptr || string2 == nullptr)
    {
        return;
    }
    float dist = activity_delivery_get_destination_dist();
    const activity_modes mode = (activity_modes)Activity_set.m_activity_state;
    int minutes;
    int seconds;
    activity_delivery_get_display_time(minutes, seconds, activity_delivery_get_remaining_time());
    const char* title = activity_title(mode);
    if (title != nullptr)
    {
        keen::formatString(mission_title, mission_title_size, "%s", title);
    }
    else
    {
        keen::formatString(mission_title, mission_title_size, "MISS_G_EXPRESS");
    }
    const char* subtitle = activity_subtitle(mode);
    keen::formatString(mission_caption, mission_caption_size, subtitle != nullptr ? subtitle : "MISS_G_EXPRESS_INFO");
    keen::formatString(string1, string1_size, "%02d:%02d", minutes, seconds);
    const bool near_target = dist < 10.0f; // Magic number: under 10 meters the distance shows hundredths.
    if ((float)(0.0 - (double)dist) >= 0.0f)
    {
        dist = 0.0f;
    }
    keen::formatString(string2, string2_size, near_target ? "%.2f" : "%.0f", (double)dist);
}

// 0x00896A70 (:1329)
static void hud_ui_missions_get_activity_riding_shotgun_info(float& float_value, char* mission_title, unsigned int mission_title_size, char* mission_caption, unsigned int mission_caption_size, char* string1, unsigned int string1_size, char* string2, unsigned int string2_size, char* string3, unsigned int string3_size)
{
    (void)float_value;
    (void)string1_size;
    (void)string2_size;
    if (!activity_vrail_is_riding_shotgun_running() || mission_title == nullptr || mission_caption == nullptr || string1 == nullptr || string2 == nullptr || string3 == nullptr)
    {
        return;
    }
    const activity_modes mode = (activity_modes)Activity_set.m_activity_state;
    const char* title = activity_title(mode);
    keen::formatString(mission_title, mission_title_size, title != nullptr ? title : "MISS_RIDIN_SHOTGUN");
    const char* subtitle = activity_subtitle(mode);
    keen::formatString(mission_caption, mission_caption_size, subtitle != nullptr ? subtitle : "MISS_RIDIN_SHOTGUN_INFO");
    if (activity_vrail_is_riding_shotgun_running())
    {
        keen::formatString(string3, string3_size, cTrueString);
    }
    // Magic number: 0x80, the size of the caller's buffers; rfg.exe passes the constant.
    activity_riding_shotgun_get_display_score_string(string1, 0x80);
    activity_riding_shotgun_get_target_score_string(string2, 0x80);
}

// 0x00896B50 (:1367)
static void hud_ui_missions_get_activity_area_defense_info(float& float_value, char* mission_title, unsigned int mission_title_size, char* mission_caption, unsigned int mission_caption_size, char* num_killed_str, unsigned int num_killed_str_size)
{
    if (!activity_area_defense_is_running() || mission_title == nullptr || mission_caption == nullptr)
    {
        return;
    }
    const activity_modes mode = (activity_modes)Activity_set.m_activity_state;
    const activity_interface_states state = activity_get_interface_mode(mode);
    const char* title = activity_title(mode);
    // Original bug, kept: the fallback title is riding shotgun's.
    keen::formatString(mission_title, mission_title_size, title != nullptr ? title : "MISS_RIDIN_SHOTGUN");
    const char* subtitle = state == ACTIVITY_INTERFACE_STATE_RUNNING ? activity_subtitle(mode) : cBlankString_uimis;
    keen::formatString(mission_caption, mission_caption_size, subtitle != nullptr ? subtitle : cBlankString_uimis);
    int num_killed;
    int kill_goal;
    bool target_vehicles;
    activity_area_defense_get_kill_totals(num_killed, kill_goal, target_vehicles);
    keen::formatString(num_killed_str, num_killed_str_size, "%i / %i", num_killed, kill_goal);
    float_value = target_vehicles ? 0.0f : 1.0f;
}

// 0x008C5D10 (:1404)
static void hud_ui_missions_process_activities_info(float dtime)
{
    (void)dtime;
    static int last_activity_state = 0; // 0x02CA151C
    vint_data_item* item = vint_data_item::find(Data_handles_uimis[0]);
    if (item == nullptr)
    {
        return;
    }
    int state = 0;
    float float_value = 0.0f;
    char title[128];
    char caption[128];
    char string1[128];
    char string2[128];
    char string3[128];
    title[0] = '\0';
    caption[0] = '\0';
    string1[0] = '\0';
    string2[0] = '\0';
    string3[0] = '\0';
    // Magic numbers: the state values the document's Lua switches on.
    switch (Activity_set.m_activity_state)
    {
    case ACTIVITY_MODE_VEHICLE_RAIL:
        hud_ui_missions_get_activity_riding_shotgun_info(float_value, title, 0x80, caption, 0x80, string1, 0x80, string2, 0x80, string3, 0x80);
        state = 5;
        break;
    case ACTIVITY_MODE_DELIVERY:
        hud_ui_missions_get_activity_delivery_info(float_value, title, 0x80, caption, 0x80, string1, 0x80, string2, 0x80);
        state = 4;
        break;
    case ACTIVITY_MODE_COURIER:
        hud_ui_missions_get_activity_courier_chase_info(float_value, title, 0x80, caption, 0x80, string1, 0x80, string2, 0x80, string3, 0x80);
        state = 7;
        break;
    case ACTIVITY_MODE_RAID_DEFEND:
        hud_ui_missions_get_activity_raid_info(float_value, title, 0x80, caption, 0x80, string1, 0x80, string2, 0x80, string3, 0x80);
        state = 1;
        break;
    case ACTIVITY_MODE_RAID_ATTACK:
        hud_ui_missions_get_activity_raid_info(float_value, title, 0x80, caption, 0x80, string1, 0x80, string2, 0x80, string3, 0x80);
        state = 9;
        break;
    case ACTIVITY_MODE_HOUSE_ARREST:
        hud_ui_missions_get_activity_house_arrest_info(float_value, title, 0x80, caption, 0x80, string1, 0x80, string2, 0x80, string3, 0x80, dtime);
        state = 2;
        break;
    case ACTIVITY_MODE_DEMOLITIONS_MASTER:
        hud_ui_missions_get_activity_demolitions_master_info(float_value, title, 0x80, caption, 0x80, string1, 0x80, string2, 0x80, string3, 0x80);
        state = 6;
        break;
    case ACTIVITY_MODE_CONVOY:
        hud_ui_missions_get_activity_convoy_info(float_value, title, 0x80, caption, 0x80, string1, 0x80, string2, 0x80, string3, 0x80);
        state = 3;
        break;
    case ACTIVITY_MODE_AREA_DEFENSE:
        hud_ui_missions_get_activity_area_defense_info(float_value, title, 0x80, caption, 0x80, string1, 0x80);
        state = 8;
        break;
    default:
        break;
    }
    if (last_activity_state != state)
    {
        if (state != 0)
        {
            audiolib_cue_play(Ga_cue_ids.mission_heading, nullptr, nullptr);
        }
        last_activity_state = state;
    }
    item->set_element(0, vint_variant(state));
    item->set_element(1, vint_variant(title));
    item->set_element(2, vint_variant(caption));
    item->set_element(3, vint_variant(float_value));
    item->set_element(4, vint_variant(string1));
    item->set_element(5, vint_variant(string2));
    item->set_element(6, vint_variant(string3));
}

// 0x00896C50 (:1524)
static vint_data_item* hud_ui_missions_add_mission_data_items_info(int& data_handle)
{
    vint_data_item* item = vint_data_item::create_new("MissionInfo");
    if (item != nullptr)
    {
        static const vint_variant_type types[7] = {VINT_PROP_TYPE_INT, VINT_PROP_TYPE_STRING, VINT_PROP_TYPE_STRING, VINT_PROP_TYPE_FLOAT, VINT_PROP_TYPE_STRING, VINT_PROP_TYPE_STRING, VINT_PROP_TYPE_STRING};
        item->set_template(types, 7);
        data_handle = (int)item->get_handle();
    }
    return item;
}

// 0x00896CD0 (:1552)
static vint_data_item* hud_ui_mission_add_objective_data_item(int& data_handle)
{
    vint_data_item* item = vint_data_item::create_new("ObjectiveStatus");
    if (item != nullptr)
    {
        static const vint_variant_type types[4] = {VINT_PROP_TYPE_BOOL, VINT_PROP_TYPE_STRING, VINT_PROP_TYPE_STRING, VINT_PROP_TYPE_STRING};
        item->set_template(types, 4);
        data_handle = (int)item->get_handle();
    }
    return item;
}

// 0x00896D40 (:1576)
static vint_data_item* hud_ui_missions_add_mission_status_data_items_info(int& data_handle)
{
    vint_data_item* item = vint_data_item::create_new("MissionStatus");
    if (item != nullptr)
    {
        static const vint_variant_type types[3] = {VINT_PROP_TYPE_INT, VINT_PROP_TYPE_STRING, VINT_PROP_TYPE_FLOAT};
        item->set_template(types, 3);
        data_handle = (int)item->get_handle();
    }
    return item;
}

// 0x008D89E0 (:1600)
static bool hud_ui_missions_add_diversion_items_info()
{
    vint_data_group* group = vint_data_group::create_new("DiversionInfo");
    if (group == nullptr)
    {
        return false;
    }
    ui_add_data_group(Doc_handle_uimis, (int)group->get_handle());
    Diversion_info.data_group_handle = (int)group->get_handle();
    return true;
}

// 0x008D8A20 (:1614)
static bool hud_ui_missions_add_timer_data_item_info()
{
    vint_data_item* item = vint_data_item::create_new("HudTimerInfo");
    if (item == nullptr)
    {
        return false;
    }
    Hud_timer_info.data_item_handle = (int)item->get_handle();
    ui_add_data_item(Doc_handle_uimis, Hud_timer_info.data_item_handle);
    return true;
}

// 0x008D8A60 (:1627)
static bool hud_ui_missions_add_hostage_data_item_info()
{
    vint_data_item* item = vint_data_item::create_new("HudHostageInfo");
    if (item == nullptr)
    {
        return false;
    }
    static const vint_variant_type types[4] = {VINT_PROP_TYPE_BOOL, VINT_PROP_TYPE_INT, VINT_PROP_TYPE_INT, VINT_PROP_TYPE_INT};
    item->set_template(types, 4);
    Hud_hostage_info.data_item_handle = (int)item->get_handle();
    ui_add_data_item(Doc_handle_uimis, Hud_hostage_info.data_item_handle);
    return true;
}

// 0x00896DB0 (:1654)
static void hud_ui_missions_process_hud_timer()
{
    if (!Hud_timer_info.updated)
    {
        return;
    }
    vint_data_item* item = vint_data_item::find(Hud_timer_info.data_item_handle);
    if (item == nullptr)
    {
        return;
    }
    item->set_element(0, vint_variant(Hud_timer_info.visible));
    item->set_element(1, vint_variant(Hud_timer_info.time_string));
    Hud_timer_info.updated = false;
}

// 0x008B21C0 (:1670)
static void hud_ui_missions_process_hud_hostage()
{
    vint_data_item* item = vint_data_item::find(Hud_hostage_info.data_item_handle);
    if (item == nullptr)
    {
        return;
    }
    if (Hud_hostage_info.num_hostages == 0)
    {
        item->set_element(0, vint_variant(false));
        item->set_element(1, vint_variant(0));
        item->set_element(2, vint_variant(0));
        item->set_element(3, vint_variant(0));
        return;
    }
    for (int i = 0; i < 3; i++)
    {
        if (i >= Hud_hostage_info.num_hostages || Hud_hostage_info.hostage_handles[i] == 0xFFFFFFFF)
        {
            Hud_hostage_info.hostage_status[i] = -1;
            continue;
        }
        npc* hp = World.object_from_handle<npc>(Hud_hostage_info.hostage_handles[i]);
        if (hp == nullptr)
        {
            continue;
        }
        // Magic numbers: the document's hostage states. 0 tied up, 1 free, 3 dead, 4 in the player's vehicle, 5 hurt.
        if (hp->is_killed())
        {
            Hud_hostage_info.hostage_status[i] = 3;
        }
        else if (Hud_hostage_info.last_hostage_health[i] > hp->hit_points)
        {
            Hud_hostage_info.hostage_status[i] = 5;
            Hud_hostage_info.last_hostage_health[i] = hp->hit_points;
        }
        else if (hp->npc_flags.tied_up)
        {
            Hud_hostage_info.hostage_status[i] = 0;
            Hud_hostage_info.last_hostage_health[i] = hp->hit_points;
        }
        else
        {
            Hud_hostage_info.hostage_status[i] = 1;
            player* pp = player_get_local_player();
            if (pp != nullptr)
            {
                vehicle* vp = human_in_vehicle(pp);
                if (vp != nullptr && human_in_vehicle(hp, vp))
                {
                    Hud_hostage_info.hostage_status[i] = 4;
                }
            }
            Hud_hostage_info.last_hostage_health[i] = hp->hit_points;
        }
    }
    item->set_element(0, vint_variant(true));
    item->set_element(1, vint_variant(Hud_hostage_info.hostage_status[0]));
    item->set_element(2, vint_variant(Hud_hostage_info.hostage_status[1]));
    item->set_element(3, vint_variant(Hud_hostage_info.hostage_status[2]));
    item->set_updated();
}

// 0x008D8AF0 (:1763)
static void hud_ui_missions_create_data_items()
{
    for (int i = 0; i < 3; i++)
    {
        int data_handle = -1;
        switch (i)
        {
        case 0:
            hud_ui_missions_add_mission_data_items_info(data_handle);
            break;
        case 1:
            hud_ui_missions_add_mission_status_data_items_info(data_handle);
            break;
        case 2:
            hud_ui_mission_add_objective_data_item(data_handle);
            break;
        default:
            keen::debug::nativePrintArgs("Int3 critical function at %s(%d)\n", __FILE__, __LINE__);
            break;
        }
        Data_handles_uimis[i] = data_handle;
        ui_add_data_item(Doc_handle_uimis, data_handle);
    }
    hud_ui_missions_add_diversion_items_info();
    hud_ui_missions_add_timer_data_item_info();
    hud_ui_missions_add_hostage_data_item_info();
}

// 0x00896E40 (:1806)
static void hud_ui_missions_set_mission_screen_active(bool active)
{
    if (Activity_set.m_show_mission_activity_screen != active)
    {
        if (!active)
        {
            strncpy_safe(Activity_set.m_mission_caption, cBlankString_uimis, 0x40);
        }
        Activity_set.m_show_mission_activity_screen = active;
    }
}

// 0x00896E70 (:1818)
bool hud_ui_missions_get_mission_screen_active()
{
    return Activity_set.m_show_mission_activity_screen;
}

// 0x00896E80 (:1824)
static void hud_ui_missions_set_mission_activity_state(int activity_state)
{
    Activity_set.m_last_activity_state = Activity_set.m_activity_state;
    Activity_set.m_activity_state = activity_state;
}

// 0x00896EA0 (:1833)
static void hud_ui_mission_determine_objective_info(float dtime)
{
    (void)dtime;
    vint_data_item* item = vint_data_item::find(Data_handles_uimis[2]);
    if (item == nullptr)
    {
        return;
    }
    item->set_element(0, vint_variant(Objective_info.m_updated));
    item->set_element(1, vint_variant(Objective_info.m_title));
    item->set_element(2, vint_variant(Objective_info.m_caption));
    item->set_element(3, vint_variant(Objective_info.m_icon));
    Objective_info.m_updated = false;
}

// 0x008B2450 (:1852)
static void hud_ui_missions_determine_mission_screen_status()
{
    bool active = mission_is_active();
    activity_modes activity = ACTIVITY_MODE_INVALID;
    if (!active)
    {
        for (int m = ACTIVITY_MODE_VEHICLE_RAIL; m < NUM_OF_ACTIVITY_MODES; m++)
        {
            const activity_modes mode = (activity_modes)m;
            if (activity_is_diversion(mode))
            {
                continue;
            }
            const activity_interface_states state = activity_get_interface_mode(mode);
            if (state != ACTIVITY_INTERFACE_STATE_RUNNING && state != ACTIVITY_INTERFACE_STATE_VICTORY && state != ACTIVITY_INTERFACE_STATE_LOSS)
            {
                continue;
            }
            if (Activity_set.m_activity_state == -1 && state != ACTIVITY_INTERFACE_STATE_RUNNING)
            {
                continue;
            }
            if (!hud_ui_missions_activity_has_interface(mode))
            {
                continue;
            }
            if (state == ACTIVITY_INTERFACE_STATE_RUNNING || (float)activity_interface_mode_duration(mode) <= (float)Activity_result_time_ms)
            {
                if (Activity_set.m_last_activity_state != mode)
                {
                    Activity_set.m_health_info.clear();
                }
                active = true;
                activity = mode;
                break;
            }
            upgrade_unlock_from_activities();
        }
    }
    hud_ui_missions_set_mission_screen_active(active);
    hud_ui_missions_set_mission_activity_state(activity);
}

// 0x00896F80 (:1920)
void hud_ui_missions_add_objective(const char* caption)
{
    hud_ui_missions_set_caption(caption);
}

// 0x00896FA0 (:1926)
const char* hud_ui_missions_get_objective()
{
    return Activity_set.m_mission_caption;
}

// 0x00896FB0 (:1932)
void hud_ui_missions_remove_objective()
{
    strncpy_safe(Activity_set.m_mission_caption, cBlankString_uimis, 0x40);
}

// 0x008F4C50 (:1938)
void hud_ui_missions_unload_items(bool assets_only)
{
    if (Active_uimis)
    {
        if (assets_only)
        {
            ui_unload_doc_assets(Doc_handle_uimis);
            Active_uimis = false;
            return;
        }
    }
    else if (assets_only)
    {
        return;
    }
    hud_ui_missions_clear_diversion_info();
    hud_ui_missions_clear_objective_complete_info();
    if (Doc_handle_uimis != -1)
    {
        ui_unload_document(Doc_handle_uimis, DFOT_NONE, 0.5f);
    }
    Data_items_created_uimis = false;
    Active_uimis = false;
    Doc_handle_uimis = -1;
    Status_time_left = -1.0f;
}

// 0x0090FF40 (:1976)
void hud_ui_missions_load_items(bool reload_flag)
{
    if (Active_uimis)
    {
        return;
    }
    if (reload_flag)
    {
        ui_reload_doc_assets(Doc_handle_uimis);
    }
    else
    {
        Doc_handle_uimis = ui_load_document(cUI_Child_Hud_Missions, DFIT_ALPHA, 0.5f, DLT_NORMAL, false, STREAM_PRIORITY_AUDIO_CACHE_LOAD, false);
    }
    Assets_loaded_uimis = false;
    Active_uimis = true;
}

// 0x008B2590 (:1993)
void hud_ui_missions_enter()
{
    Assets_loaded_uimis = false;
    Data_items_created_uimis = false;
    Active_uimis = false;
    Status_time_left = -1.0f;
    hud_ui_missions_clear_diversion_info();
    hud_ui_missions_init_activity_info_set();
    hud_ui_missions_clear_objective_complete_info();
    hud_ui_missions_clear_hostage_info();
}

// 0x008F4D20 (:2008)
void hud_ui_missions_process(float dtime)
{
    if (game_is_paused())
    {
        return;
    }
    if (Assets_loaded_uimis)
    {
        hud_ui_missions_determine_mission_screen_status();
        hud_ui_missions_process_hud_hostage();
        hud_ui_mission_determine_objective_info(dtime);
        if (Activity_set.m_activity_state < 1)
        {
            hud_ui_missions_process_missions_info(dtime);
        }
        else
        {
            hud_ui_missions_process_activities_info(dtime);
        }
        hud_ui_missions_process_activity_status(dtime);
        hud_ui_missions_process_diversion_info(dtime);
        hud_ui_missions_process_hud_timer();
        if (Doc_handle_uimis != -1)
        {
            ui_add_for_processing(Doc_handle_uimis);
        }
        return;
    }
    if (!Active_uimis)
    {
        return;
    }
    switch (ui_get_doc_asset_state(Doc_handle_uimis))
    {
    case ST_UNLOADED:
        keen::debug::nativePrintArgs("Int3 critical function at %s(%d)\n", __FILE__, __LINE__);
        hud_ui_missions_unload_items(false);
        break;
    case ST_ERROR:
        hud_ui_missions_unload_items(false);
        break;
    case ST_LOADING:
        break;
    case ST_LOADED:
        Assets_loaded_uimis = true;
        if (!Data_items_created_uimis)
        {
            hud_ui_missions_create_data_items();
            Data_items_created_uimis = true;
        }
        ui_fade_in_doc(Doc_handle_uimis, DFIT_ALPHA, 0.5f);
        break;
    default:
        keen::debug::nativePrintArgs("Int3 critical function at %s(%d)\n", __FILE__, __LINE__);
        break;
    }
}

// 0x008D8C80 (:2095)
void hud_ui_missions_render()
{
    if (Assets_loaded_uimis && Doc_handle_uimis != -1)
    {
        ui_add_for_rendering(Doc_handle_uimis);
    }
}

// 0x008C61D0 (:2107)
void hud_ui_missions_get_doc_handles(farray<int,16>& handles)
{
    handles.add(Doc_handle_uimis);
}

// 0x00896FD0 (:2118)
bool hud_ui_missions_set_timer(bool is_visible, const wchar_t* display_string)
{
    if (is_visible)
    {
        Hud_timer_info.visible = true;
        Hud_timer_info.updated = true;
        if (display_string != nullptr)
        {
            copy_to_ascii(Hud_timer_info.time_string, display_string, 0x80);
        }
        return true;
    }
    if (Hud_timer_info.visible)
    {
        Hud_timer_info.visible = false;
        Hud_timer_info.updated = true;
    }
    return false;
}

// 0x00897020 (:2140)
bool hud_ui_missions_set_hostages(int num_hostages, const unsigned int* handle_list)
{
    for (int i = 0; i < 3; i++)
    {
        if (num_hostages > i)
        {
            Hud_hostage_info.hostage_handles[i] = handle_list[i];
            Hud_hostage_info.last_hostage_health[i] = 0.0f;
        }
        else
        {
            Hud_hostage_info.hostage_handles[i] = 0xFFFFFFFF;
        }
        Hud_hostage_info.hostage_status[i] = -1;
    }
    Hud_hostage_info.num_hostages = num_hostages;
    return true;
}
