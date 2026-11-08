// Original: volition/rfg/code/hud/hud_ui_activity_complete.cpp
//
// The activity complete screen: the result, the failure reason, the score tallies of Guerrilla Express and Riding
// Shotgun, and the continue, retry and exit buttons, answered through each activity's callbacks.
//
// The types below have no other user. Their placement in this file is inferred.

#include "volition/rfg/code/hud/hud_ui_activity_complete.h"

#include "volition/audio_lib/code/audiolib_api.h"
#include "volition/audio_lib/code/audiolib_att_loop_rel.h"
#include "volition/audio_lib/code/audiolib_music.h"
#include "volition/interface_lib/code/vint_data_item.h"
#include "volition/interface_lib/code/vint_variant.h"
#include "volition/rfg/code/control/control_map.h"
#include "volition/rfg/code/event_logging/gamelog.h"
#include "volition/rfg/code/gameplay/activities/activity.h"
#include "volition/rfg/code/gameplay/activities/activity_area_defense.h"
#include "volition/rfg/code/gameplay/activities/activity_delivery.h"
#include "volition/rfg/code/gameplay/activities/activity_house_arrest.h"
#include "volition/rfg/code/gameplay/activities/activity_raid.h"
#include "volition/rfg/code/gameplay/activities/activity_vrail.h"
#include "volition/rfg/code/gameplay/upgrades.h"
#include "volition/rfg/code/gamewide/game_clock.h"
#include "volition/rfg/code/hud/hud.h"
#include "volition/rfg/code/interface/localization/localization_shared.h"
#include "volition/rfg/code/interface/ui_document_manager.h"
#include "volition/rfg/code/interface/ui_menu.h"
#include "volition/rfg/code/interface/ui_message_box.h"
#include "volition/rfg/code/level/world.h"
#include "volition/rfg/code/objects/humans/player.h"
#include "volition/rfg/code/rfg.h"
#include "volition/rfg/code/scripting/dialogue_manager.h"
#include "volition/vlib/code/math/floating.h"
#include "volition/vlib/code/os/frametime.h"
#include "volition/vlib/code/util/string_util.h"

#include "keen/base/language.hpp"
#include "keen/base/string.hpp"
#include "keen/base/trace.hpp"

#include <string.h>

enum activity_complete_result
{
    ACR_NONE = 0,
    ACR_COMPLETED = 1,
    ACR_FAILED = 2,
    ACR_LAST = 3,
};

// What the Lua side calls an activity. It indexes the tables below.
enum lua_activity_remap
{
    LAR_MISSION_PARTYTIME = -2,
    LAR_MISSION = -1,
    LAR_MISSION_ACTIVITY_INVALID = 0,
    LAR_ACTIVITY_RAID = 1,
    LAR_ACTIVITY_HOUSE_ARREST = 2,
    LAR_ACTIVITY_CONVOY = 3,
    LAR_ACTIVITY_GUERRILLA_EXPRESS = 4,
    LAR_ACTIVITY_RIDING_SHOTGUN = 5,
    LAR_ACTIVITY_DEMOLITIONS_MASTER = 6,
    LAR_ACTIVITY_COURIER = 7,
    LAR_ACTIVITY_AREA_DEFENSE = 8,
    LAR_ACTIVITY_RAID_OFFENSIVE = 9,
    LAR_TOTAL = 10,
};

// size 0x28
struct tally_info
{
    int num_amounts; // 0x00
    float tally_amounts[4]; // 0x04
    float desired_amounts[4]; // 0x14
    float tally_time; // 0x24
};

// size 0x144
struct activity_complete_info
{
    bool exiting; // 0x00
    bool load; // 0x01
    bool push_state; // 0x02
    activity_complete_result complete_result; // 0x04
    lua_activity_remap activity_type; // 0x08
    activity_modes activity_mode; // 0x0C
    const char* failure_reason; // 0x10
    int doc_handle; // 0x14
    bool assets_loaded; // 0x18
    int data_handles[2]; // 0x1C
    unsigned int control_id; // 0x24
    char activity_description[128]; // 0x28
    bool do_tallies; // 0xA8
    tally_info tallies[3]; // 0xAC
    int num_non_tally_data_items; // 0x124
    float non_tally_data[3]; // 0x128
    int sound_money_loop; // 0x134
    int sound_money_loop_release; // 0x138
    int play_money_loop; // 0x13C
    bool skip_callback; // 0x140
};

// ---- globals
int Play_sound_id; // 0x02B955A4, written here and never read

// ---- file-static globals
typedef void (*activity_complete_callback)();
static const activity_complete_callback Retry_callbacks[LAR_TOTAL] = {nullptr, activity_raid_replay, activity_house_arrest_replay, nullptr, activity_guerrilla_express_replay, activity_riding_shotgun_replay, nullptr, nullptr, activity_area_defense_replay, activity_raid_replay}; // 0x012FB268
static const activity_complete_callback Replay_callbacks[LAR_TOTAL] = {nullptr, nullptr, nullptr, nullptr, activity_guerrilla_express_replay, nullptr, nullptr, nullptr, nullptr, nullptr}; // 0x012FB290
static const activity_complete_callback Exit_callbacks[LAR_TOTAL] = {nullptr, activity_raid_exit, activity_house_arrest_exit, nullptr, activity_guerrilla_express_exit, activity_riding_shotgun_exit, nullptr, nullptr, activity_area_defense_exit, activity_raid_exit}; // 0x012FB2B8
static const char* cUI_activity_complete_doc[LAR_TOTAL] = {"", "rfg_ui_mission_complete", "rfg_ui_mission_complete", "", "rfg_ui_activity_complete", "rfg_ui_activity_complete", "", "", "rfg_ui_mission_complete", "rfg_ui_mission_complete"}; // 0x01660F60
static const char* cMC_sound_money_loop = "HUD_MIS_TALLY_LOOP"; // 0x01660F88
static const char* cMC_sound_money_loop_release = "HUD_MIS_TALLY_RELEASE"; // 0x01660F8C
static const char* cContinue = "MENU_CONTINUE"; // 0x01660F94
static const char* cExit = "MENU_EXIT"; // 0x01660F98
static const char* cRetry = "MENU_RETRY"; // 0x01660F9C
static const char* kHintImg2 = "mc_hint_2"; // 0x01662210
static const char* kHintText2 = "mc_hint_2_text"; // 0x01662214
static const char* kHintImg = "mc_hint_1"; // 0x01662218
static const char* kHintText = "mc_hint_1_text"; // 0x0166221C
static int Input_id_complete; // 0x02B958F8
static activity_complete_info Ac_info; // 0x02B9DA90
static int Tally_index; // 0x02B9DCA0
static bool loop_was_playing; // 0x02BE58B0, only hud_ui_activity_complete_process touches it

static const float cTally_seconds = 1.5f; // Magic number: how long a tally counts up, 0x012BFFA8.

// 0x00845A10 (:198)
static void huac_init_info()
{
    Ac_info.doc_handle = -1;
    Ac_info.complete_result = ACR_NONE;
    Ac_info.exiting = false;
    Ac_info.assets_loaded = false;
    Ac_info.push_state = false;
    Ac_info.control_id = 0;
}

// 0x00845A40 (:209)
static void huac_load_ui_information()
{
    if (Ac_info.doc_handle == -1)
    {
        return;
    }
    unsigned int handle = 0;
    vint_data_item* item = vint_data_item::create_new("ActivityComplete");
    if (item != nullptr)
    {
        static const vint_variant_type types[10] = {VINT_PROP_TYPE_BOOL, VINT_PROP_TYPE_INT, VINT_PROP_TYPE_BOOL, VINT_PROP_TYPE_STRING, VINT_PROP_TYPE_STRING, VINT_PROP_TYPE_STRING, VINT_PROP_TYPE_STRING, VINT_PROP_TYPE_STRING, VINT_PROP_TYPE_STRING, VINT_PROP_TYPE_BOOL};
        item->set_template(types, 10);
        handle = item->get_handle();
    }
    Ac_info.data_handles[0] = (int)handle;
    ui_add_data_item(Ac_info.doc_handle, (int)handle);
    handle = 0;
    item = vint_data_item::create_new("ActivityTallys");
    if (item != nullptr)
    {
        static const vint_variant_type types[15] = {VINT_PROP_TYPE_STRING, VINT_PROP_TYPE_STRING, VINT_PROP_TYPE_STRING, VINT_PROP_TYPE_STRING, VINT_PROP_TYPE_STRING, VINT_PROP_TYPE_STRING, VINT_PROP_TYPE_STRING, VINT_PROP_TYPE_STRING, VINT_PROP_TYPE_STRING, VINT_PROP_TYPE_STRING, VINT_PROP_TYPE_STRING, VINT_PROP_TYPE_STRING, VINT_PROP_TYPE_STRING, VINT_PROP_TYPE_STRING, VINT_PROP_TYPE_STRING};
        item->set_template(types, 15);
        handle = item->get_handle();
    }
    Ac_info.data_handles[1] = (int)handle;
    ui_add_data_item(Ac_info.doc_handle, (int)handle);
}

// 0x00845B50 (:287)
static void huac_unload_ui_information()
{
    ui_unload_document(Ac_info.doc_handle, DFOT_NONE, 0.0f);
    ui_relinquish_control_id(Ac_info.control_id);
    Ac_info.control_id = 0;
}

// Whether the second button does anything. rfg.exe inlines this test in both of its users.
static bool huac_has_second_option()
{
    return (Retry_callbacks[Ac_info.activity_type] != nullptr && Ac_info.complete_result == ACR_FAILED) || (Replay_callbacks[Ac_info.activity_type] != nullptr && Ac_info.complete_result == ACR_COMPLETED);
}

// 0x00845B80 (:295)
static void process_activity_information(float dtime)
{
    (void)dtime;
    vint_data_item* item = vint_data_item::find(Ac_info.data_handles[0]);
    if (item == nullptr)
    {
        return;
    }
    char button[128];
    char image[128];
    const bool second_option = huac_has_second_option();
    // rfg.exe reads hud.cpp's Button_input_enabled directly, through the unity file.
    const bool input_enabled = hud_is_button_input_enabled();
    item->set_element(0, vint_variant(second_option));
    item->set_element(1, vint_variant((int)Ac_info.activity_type));
    item->set_element(2, vint_variant(Ac_info.complete_result == ACR_COMPLETED));
    item->set_element(3, vint_variant(Ac_info.activity_description));
    item->set_element(4, vint_variant(Ac_info.failure_reason));
    if (input_enabled)
    {
        controls_get_button_image_name(button, 0x80, CBA_MENU_SELECT, Player_controller_index);
        keen::formatString(image, 0x80, "[image:%s]", button);
        item->set_element(5, vint_variant(image));
    }
    else
    {
        item->set_element(5, vint_variant((const char*)nullptr));
    }
    item->set_element(6, vint_variant(Retry_callbacks[Ac_info.activity_type] != nullptr && Ac_info.complete_result == ACR_FAILED ? cRetry : cContinue));
    if (input_enabled && second_option)
    {
        controls_get_button_image_name(button, 0x80, CBA_MENU_ABORT, Player_controller_index);
        keen::formatString(image, 0x80, "[image:%s]", button);
        item->set_element(7, vint_variant(image));
    }
    else
    {
        item->set_element(7, vint_variant((const char*)nullptr));
    }
    item->set_element(8, vint_variant(Replay_callbacks[Ac_info.activity_type] != nullptr && Ac_info.complete_result == ACR_COMPLETED ? cRetry : cExit));
    // True for Riding Shotgun in Spanish, Italian and German. What the document does with it was not checked.
    const keen::LanguageId language = localization_get_language();
    item->set_element(9, vint_variant(Ac_info.activity_type == LAR_ACTIVITY_RIDING_SHOTGUN && (language == keen::LanguageId_Es_Es || language == keen::LanguageId_It_It || language == keen::LanguageId_De_De)));
}

// 0x00856000 (:359)
static void process_activity_tallys(float dtime)
{
    vint_data_item* item = vint_data_item::find(Ac_info.data_handles[1]);
    if (item == nullptr)
    {
        return;
    }
    if (!hud_is_button_input_enabled() && Ac_info.do_tallies)
    {
        for (int i = 0; i < 3; i++)
        {
            tally_info& tally = Ac_info.tallies[i];
            if (tally.tally_time < 0.0f)
            {
                continue;
            }
            tally.tally_time = (float)((double)tally.tally_time + (double)dtime);
            if (tally.tally_time > cTally_seconds)
            {
                tally.tally_time = cTally_seconds;
                // The last tally of the activity stops the counting sound.
                int last_tally = 0;
                if (Ac_info.activity_type == LAR_ACTIVITY_GUERRILLA_EXPRESS)
                {
                    last_tally = 1;
                }
                else if (Ac_info.activity_type == LAR_ACTIVITY_RIDING_SHOTGUN)
                {
                    last_tally = 2;
                }
                else
                {
                    keen::debug::nativePrintArgs("Int3 critical function at %s(%d)\n", __FILE__, __LINE__);
                }
                if (i == last_tally && Ac_info.play_money_loop != -1)
                {
                    audiolib_att_loop_rel_stop(Ac_info.play_money_loop, 100); // Magic number: 100 ms, as below.
                    Ac_info.play_money_loop = -1;
                }
            }
            else if (Ac_info.play_money_loop != -1 && audiolib_is_playing(Ac_info.play_money_loop, true))
            {
                audiolib_play_set_volume(Ac_info.play_money_loop, 1.0f, 100);
            }
            bool still_counting = false;
            for (int j = 0; j < tally.num_amounts; j++)
            {
                tally.tally_amounts[j] = (float)((double)tally.tally_time / 1.5 * (double)tally.desired_amounts[j]);
                if (tally.desired_amounts[j] > 0.0f && cTally_seconds > tally.tally_time)
                {
                    still_counting = true;
                }
            }
            if (still_counting && i >= Tally_index && Ac_info.play_money_loop == -1)
            {
                audiolib_alr_params params;
                memset(&params, 0, sizeof(params));
                params.loop_cue_id = Ac_info.sound_money_loop;
                params.release_cue_id = Ac_info.sound_money_loop_release;
                params.num_shots = 1;
                audiolib_att_loop_rel_start(&params, &Ac_info.play_money_loop);
                audiolib_play_set_volume(Ac_info.play_money_loop, 1.0f, 100);
                Tally_index = i;
            }
        }
    }
    else
    {
        if (Ac_info.play_money_loop != -1)
        {
            audiolib_att_loop_rel_stop(Ac_info.play_money_loop, 100);
            Ac_info.play_money_loop = -1;
        }
        for (int i = 0; i < 3; i++)
        {
            for (int j = 0; j < Ac_info.tallies[i].num_amounts; j++)
            {
                Ac_info.tallies[i].tally_amounts[j] = Ac_info.tallies[i].desired_amounts[j];
            }
        }
    }
    char text[128];
    switch (Ac_info.activity_type)
    {
    case LAR_ACTIVITY_RAID:
    case LAR_ACTIVITY_HOUSE_ARREST:
    case LAR_ACTIVITY_AREA_DEFENSE:
    case LAR_ACTIVITY_RAID_OFFENSIVE:
        break;
    case LAR_ACTIVITY_GUERRILLA_EXPRESS:
    {
        // Times in ms, shown as minutes, seconds and hundredths. Element 0 is the document's and stays unset.
        int element = 1;
        for (int i = 0; i < 3; i++)
        {
            for (int j = 0; j < Ac_info.tallies[i].num_amounts; j++)
            {
                const float amount = Ac_info.tallies[i].tally_amounts[j];
                const int seconds = (int)((double)amount / 1000.0);
                const int hundredths = fl_round((float)((double)(float)((int)amount % 1000) / 10.0));
                _snprintf_safe(text, 0x80, "%i:%.02i.%.02i", seconds / 60, seconds % 60, hundredths);
                item->set_element(element, vint_variant(text));
                element++;
            }
        }
        break;
    }
    case LAR_ACTIVITY_RIDING_SHOTGUN:
    {
        int element = 0;
        for (; element < Ac_info.num_non_tally_data_items; element++)
        {
            _snprintf_safe(text, 0x80, "%.2f", (double)Ac_info.non_tally_data[element]);
            item->set_element(element, vint_variant(text));
        }
        for (int i = 0; i < 3; i++)
        {
            for (int j = 0; j < Ac_info.tallies[i].num_amounts; j++)
            {
                _snprintf_safe(text, 0x80, "%.2f", (double)Ac_info.tallies[i].tally_amounts[j]);
                item->set_element(element, vint_variant(text));
                element++;
            }
        }
        break;
    }
    default:
        keen::debug::nativePrintArgs("Int3 critical function at %s(%d)\n", __FILE__, __LINE__);
        break;
    }
}

// 0x00856510 (:508)
static void huac_process_data_items()
{
    process_activity_information(Frametime_real);
    process_activity_tallys(Frametime_real);
}

// 0x00845EA0 (:515)
static int check_for_mouse_input()
{
    if (ui_mouse_over_hint_combo(kHintImg, kHintText, Ac_info.doc_handle, true))
    {
        return 1;
    }
    return ui_mouse_over_hint_combo(kHintImg2, kHintText2, Ac_info.doc_handle, true) ? 2 : 0;
}

// The exit callback, which Riding Shotgun defers until it has faded out (hud_ui_activity_complete_process). rfg.exe
// inlines this in both buttons.
static void huac_leave_activity()
{
    if (Ac_info.activity_type == LAR_ACTIVITY_RIDING_SHOTGUN)
    {
        ui_fade_to_black_fade_out(false);
    }
    else if (!Ac_info.skip_callback)
    {
        Exit_callbacks[Ac_info.activity_type]();
    }
}

// 0x00845F00 (:534)
static void check_for_user_input_complete()
{
    if (ui_get_current_input_id() != Input_id_complete || ui_is_msgbox_active(-2) || Ac_info.exiting || !hud_is_button_input_enabled())
    {
        return;
    }
    const bool second_option = huac_has_second_option();
    const int mouse = check_for_mouse_input();
    // Magic number: 10, the document's accept button, as in the mission briefing.
    if (ui_get_button_info(10, Input_id_complete) != 0.0f || mouse == 1)
    {
        ui_menu_play_sound(MENU_SOUND_PRESS_SELECT);
        dialogue_manager_cancel_briefing();
        if (Retry_callbacks[Ac_info.activity_type] != nullptr && Ac_info.complete_result == ACR_FAILED)
        {
            if (!Ac_info.skip_callback)
            {
                Retry_callbacks[Ac_info.activity_type]();
            }
            Ac_info.exiting = true;
            return;
        }
        huac_leave_activity();
        if (player_get_local_player()->is_killed())
        {
            World.save_and_reload();
            Ac_info.exiting = true;
            return;
        }
        if (Ac_info.complete_result == ACR_COMPLETED)
        {
            upgrade_show_new_upgrades(false, 0);
        }
        Ac_info.exiting = true;
        return;
    }
    if (!second_option || !(controls_is_button_just_pressed(CBA_MENU_ABORT, Player_controller_index) || mouse == 2))
    {
        return;
    }
    if (Replay_callbacks[Ac_info.activity_type] != nullptr && Ac_info.complete_result == ACR_COMPLETED)
    {
        if (!Ac_info.skip_callback)
        {
            Replay_callbacks[Ac_info.activity_type]();
        }
        upgrade_show_new_upgrades(false, 0);
    }
    else
    {
        huac_leave_activity();
        if (player_get_local_player()->is_killed())
        {
            World.save_and_reload();
        }
    }
    ui_menu_play_sound(MENU_SOUND_PRESS_ABORT);
    dialogue_manager_cancel_all();
    Ac_info.exiting = true;
}

// The start of every *_load_info: the activity's title is the format string, as in rfg.exe.
static void huac_load_title()
{
    const char* title = activity_title(Ac_info.activity_mode);
    if (title != nullptr)
    {
        keen::formatString(Ac_info.activity_description, 0x80, title);
    }
}

// The end of the three activities with no tallies.
static void huac_no_tallies()
{
    Ac_info.do_tallies = false;
    Ac_info.num_non_tally_data_items = 0;
    Ac_info.tallies[0].num_amounts = 0;
    Ac_info.tallies[1].num_amounts = 0;
    Ac_info.tallies[2].num_amounts = 0;
}

// 0x00846110 (:627)
static void riding_shotgun_load_info()
{
    huac_load_title();
    float target_goal;
    float soldier_score;
    float building_score;
    float vehicle_score;
    float base_score;
    float penalty;
    float total;
    if (!Ac_info.skip_callback)
    {
        activity_riding_shotgun_get_score_results(target_goal, soldier_score, building_score, vehicle_score, base_score, penalty, total);
    }
    else
    {
        // Magic numbers: test scores. Nothing sets skip_callback in rfg.exe, so these are dead.
        target_goal = 15.5f;
        soldier_score = 1.25f;
        building_score = 10.8f;
        vehicle_score = 3.53f;
        base_score = 14.0f;
        penalty = 0.85f;
        total = 16.7f;
    }
    Ac_info.do_tallies = true;
    Ac_info.num_non_tally_data_items = 1;
    Ac_info.non_tally_data[0] = target_goal;
    Ac_info.tallies[0].num_amounts = 4;
    Ac_info.tallies[0].desired_amounts[0] = soldier_score;
    Ac_info.tallies[0].desired_amounts[1] = building_score;
    Ac_info.tallies[0].desired_amounts[2] = vehicle_score;
    Ac_info.tallies[0].desired_amounts[3] = base_score;
    Ac_info.tallies[1].num_amounts = 1;
    Ac_info.tallies[1].desired_amounts[0] = penalty;
    Ac_info.tallies[2].num_amounts = 1;
    Ac_info.tallies[2].desired_amounts[0] = total;
}

// 0x00846230 (:665)
static void guerrilla_express_load_info()
{
    huac_load_title();
    float par_time;
    float target_time;
    float best_time;
    float current_time;
    if (!Ac_info.skip_callback)
    {
        activity_guerrilla_express_get_score_results(par_time, target_time, best_time, current_time);
    }
    else
    {
        // Magic numbers: test times. Nothing sets skip_callback in rfg.exe, so these are dead.
        par_time = 30.0f;
        target_time = 100.0f;
        best_time = 25.23f;
        current_time = 46.75f;
    }
    Ac_info.do_tallies = true;
    Ac_info.num_non_tally_data_items = 0;
    Ac_info.tallies[0].num_amounts = 3;
    Ac_info.tallies[0].desired_amounts[0] = par_time;
    Ac_info.tallies[0].desired_amounts[1] = target_time;
    Ac_info.tallies[0].desired_amounts[2] = best_time;
    Ac_info.tallies[1].num_amounts = 1;
    Ac_info.tallies[1].desired_amounts[0] = current_time;
    Ac_info.tallies[2].num_amounts = 0;
}

// 0x00846310 (:697)
static void raid_load_info()
{
    huac_load_title();
    huac_no_tallies();
}

// 0x00846360 (:713)
static void house_arrest_load_info()
{
    huac_load_title();
    huac_no_tallies();
}

// 0x008463B0 (:729)
static void area_defense_load_info()
{
    huac_load_title();
    huac_no_tallies();
}

// 0x008686C0 (:751)
void hud_ui_activity_complete_init()
{
    state_info info;
    info.enter = hud_ui_activity_complete_enter;
    info.exit = hud_ui_activity_complete_exit;
    info.process = hud_ui_activity_complete_process;
    info.handshake = hud_ui_activity_complete_handshake;
    info.render = hud_ui_activity_complete_render;
    info.transparent = false;
    info.pause_beneath = false;
    gameseq_register_state(GS_INGAME_ACTIVITY_COMPLETE, info);
    huac_init_info();
}

// 0x00846400 (:769)
void hud_ui_activity_complete_enter(game_state prev_state)
{
    (void)prev_state;
    Tally_index = 0;
    Ac_info.assets_loaded = false;
    Ac_info.push_state = false;
    Input_id_complete = -1;
    Ac_info.exiting = false;
    Ac_info.doc_handle = ui_load_document(cUI_activity_complete_doc[Ac_info.activity_type], DFIT_ALPHA, 0.5f, DLT_NORMAL, true, STREAM_PRIORITY_AUDIO_CACHE_LOAD, true);
    if (Ac_info.doc_handle == -1)
    {
        keen::debug::nativePrintArgs("Int3 critical function at %s(%d)\n", __FILE__, __LINE__);
        gameseq_pop_state();
        return;
    }
    ui_set_screen_blur(true, true);
    Hide_hud_ref_count++;
    Ac_info.control_id = ui_request_control_id();
    // Magic number: -1, a tally that has not started. The document's rfg_start_activity_complete starts each one.
    Ac_info.tallies[0].tally_time = -1.0f;
    Ac_info.tallies[1].tally_time = -1.0f;
    Ac_info.tallies[2].tally_time = -1.0f;
    Play_sound_id = -1;
    Ac_info.load = true;
    Ac_info.tallies[0].num_amounts = 0;
    Ac_info.tallies[1].num_amounts = 0;
    Ac_info.tallies[2].num_amounts = 0;
    Ac_info.num_non_tally_data_items = 0;
    Show_minimap = false;
    audiolib_interactive_music_set_fade(0.04f, 3000); // Magic numbers: the music drops to 4% over 3 seconds.
    audiolib_cue_get_id(cMC_sound_money_loop, &Ac_info.sound_money_loop);
    audiolib_cue_get_id(cMC_sound_money_loop_release, &Ac_info.sound_money_loop_release);
    Ac_info.play_money_loop = -1;
    ui_wait_for_button_ups();
}

// 0x00846540 (:833)
void hud_ui_activity_complete_exit(game_state new_state)
{
    (void)new_state;
    Show_minimap = true;
    ui_relinquish_input_id(Input_id_complete);
    huac_unload_ui_information();
    if (Hide_hud_ref_count > 0)
    {
        Hide_hud_ref_count--;
    }
    ui_set_screen_blur(false, false);
    audiolib_interactive_music_stop_fade(500); // Magic number: half a second.
    if (Ac_info.play_money_loop != -1)
    {
        audiolib_att_loop_rel_stop(Ac_info.play_money_loop, 100);
        Ac_info.play_money_loop = -1;
    }
    upgrade_unlock_from_activities();
}

// 0x00856530 (:865)
void hud_ui_activity_complete_process()
{
    const bool msgbox_active = ui_is_msgbox_active(-2); // Magic number: -2, any message box.
    if (msgbox_active)
    {
        // The counting sound stops behind a message box, and restarts below.
        if (Ac_info.play_money_loop != -1)
        {
            audiolib_att_loop_rel_stop(Ac_info.play_money_loop, 100);
            Ac_info.play_money_loop = -1;
            loop_was_playing = true;
        }
        ui_process(Ac_info.control_id, true);
        return;
    }
    if (Ac_info.exiting)
    {
        if (Ac_info.activity_type == LAR_ACTIVITY_RIDING_SHOTGUN)
        {
            if (ui_fade_to_black_is_faded())
            {
                if (!Ac_info.skip_callback)
                {
                    Exit_callbacks[Ac_info.activity_type]();
                }
                ui_fade_to_black_fade_in(true);
                gameseq_pop_state();
            }
        }
        else
        {
            gameseq_pop_state();
        }
    }
    else if (!Ac_info.assets_loaded)
    {
        switch (ui_get_doc_asset_state(Ac_info.doc_handle))
        {
        case ST_UNLOADED:
            keen::debug::nativePrintArgs("Int3 critical function at %s(%d)\n", __FILE__, __LINE__);
            // fall through
        case ST_ERROR:
            ui_unload_document(Ac_info.doc_handle, DFOT_NONE, 0.0f);
            Ac_info.doc_handle = -1;
            Ac_info.exiting = true;
            break;
        case ST_LOADING:
            break;
        case ST_LOADED:
            Ac_info.assets_loaded = true;
            huac_load_ui_information();
            Input_id_complete = ui_register_for_processed_input(IRT_UNLIMITED_INPUT, false);
            switch (Ac_info.activity_type)
            {
            case LAR_ACTIVITY_RAID:
            case LAR_ACTIVITY_RAID_OFFENSIVE:
                raid_load_info();
                break;
            case LAR_ACTIVITY_HOUSE_ARREST:
                house_arrest_load_info();
                break;
            case LAR_ACTIVITY_GUERRILLA_EXPRESS:
                guerrilla_express_load_info();
                break;
            case LAR_ACTIVITY_RIDING_SHOTGUN:
                riding_shotgun_load_info();
                break;
            case LAR_ACTIVITY_AREA_DEFENSE:
                area_defense_load_info();
                break;
            default:
                break;
            }
            break;
        default:
            keen::debug::nativePrintArgs("Int3 critical function at %s(%d)\n", __FILE__, __LINE__);
            break;
        }
    }
    if (!Ac_info.assets_loaded)
    {
        ui_process(Ac_info.control_id, false);
        return;
    }
    if (loop_was_playing && Ac_info.play_money_loop == -1)
    {
        audiolib_alr_params params;
        memset(&params, 0, sizeof(params));
        params.loop_cue_id = Ac_info.sound_money_loop;
        params.release_cue_id = Ac_info.sound_money_loop_release;
        params.num_shots = 1;
        audiolib_att_loop_rel_start(&params, &Ac_info.play_money_loop);
        audiolib_play_set_volume(Ac_info.play_money_loop, 1.0f, 100);
        loop_was_playing = false;
    }
    check_for_user_input_complete();
    huac_process_data_items();
    ui_add_for_processing(Ac_info.doc_handle);
    ui_process(Ac_info.control_id, false);
}

// 0x008465D0 (:983)
void hud_ui_activity_complete_handshake(rl_renderer*)
{
}

// 0x008465E0 (:989)
void hud_ui_activity_complete_render(rl_renderer*)
{
    if (!Ac_info.exiting && Ac_info.assets_loaded)
    {
        ui_add_for_rendering(Ac_info.doc_handle);
    }
    ui_render(Ac_info.control_id, false);
}

// 0x00846620 (:1000)
void hud_ui_activity_complete_do_frame()
{
    if (Ac_info.push_state && !game_is_paused() && gameseq_get_pending_state() == GS_NONE)
    {
        gameseq_push_state(GS_INGAME_ACTIVITY_COMPLETE, true, false);
        Ac_info.push_state = false;
    }
}

// 0x00846660 (:1011)
void hud_ui_activity_complete_level_shutdown()
{
    Ac_info.push_state = false;
}

// 0x00846670 (:1017)
bool hud_ui_activity_complete_start(activity_modes mode, bool complete)
{
    // The compare is unsigned in rfg.exe, so a negative mode is refused too.
    if (Ac_info.push_state || (unsigned int)mode >= (unsigned int)NUM_OF_ACTIVITY_MODES)
    {
        return false;
    }
    Ac_info.activity_mode = mode;
    switch (mode)
    {
    case ACTIVITY_MODE_VEHICLE_RAIL:
        Ac_info.activity_type = LAR_ACTIVITY_RIDING_SHOTGUN;
        break;
    case ACTIVITY_MODE_DELIVERY:
        Ac_info.activity_type = LAR_ACTIVITY_GUERRILLA_EXPRESS;
        break;
    case ACTIVITY_MODE_RAID_DEFEND:
        Ac_info.activity_type = LAR_ACTIVITY_RAID;
        break;
    case ACTIVITY_MODE_RAID_ATTACK:
        Ac_info.activity_type = LAR_ACTIVITY_RAID_OFFENSIVE;
        break;
    case ACTIVITY_MODE_HOUSE_ARREST:
        Ac_info.activity_type = LAR_ACTIVITY_HOUSE_ARREST;
        break;
    case ACTIVITY_MODE_AREA_DEFENSE:
        Ac_info.activity_type = LAR_ACTIVITY_AREA_DEFENSE;
        break;
    default:
        // Original bug, kept: any other mode keeps the previous activity's type.
        keen::debug::nativePrintArgs("Int3 critical function at %s(%d)\n", __FILE__, __LINE__);
        break;
    }
    Ac_info.skip_callback = false;
    Ac_info.failure_reason = nullptr;
    Ac_info.complete_result = complete ? ACR_COMPLETED : ACR_FAILED;
    if (!complete)
    {
        switch (gamelog_get_activity_failed_reason())
        {
        case GLCR_PLAYER_DIED:
            Ac_info.failure_reason = "MENU_YOU_DIED";
            break;
        case GLCR_OUT_OF_TIME:
            Ac_info.failure_reason = "ACTIVITY_FAIL_OUT_OF_TIME";
            break;
        case GLCR_VEHICLE_ABANDONED:
            Ac_info.failure_reason = "ACTIVITY_FAIL_AWAY_FROM_VEH";
            break;
        case GLCR_VEHICLE_DESTROYED:
            Ac_info.failure_reason = "ACTIVITY_FAIL_VEH_DESTROYED";
            break;
        case GLCR_ESCAPED:
            Ac_info.failure_reason = "ACTIVITY_FAIL_VEH_ESCAPED";
            break;
        case GLCR_OUT_OF_RANGE:
            Ac_info.failure_reason = "ACTIVITY_FAIL_LEFT_AREA";
            break;
        case GLCR_OUT_OF_AMMO:
            Ac_info.failure_reason = "ACTIVITY_FAIL_OUT_OF_AMMO";
            break;
        case GLCR_HOSTAGES_KILLED:
            Ac_info.failure_reason = "ACTIVITY_FAIL_HOSTAGES_DEAD";
            break;
        case GLCR_BUILDING_DESTROYED:
            Ac_info.failure_reason = "ACTIVITY_FAIL_BUILDING_DESTROYED";
            break;
        case GLCR_TARGET_GOAL_NOT_REACHED:
            Ac_info.failure_reason = "ACTIVITY_FAIL_DAMAGE";
            break;
        default:
            break;
        }
    }
    Ac_info.push_state = true;
    return true;
}

// 0x00846870 (:1100)
void hud_ui_activity_complete_done()
{
    Ac_info.complete_result = ACR_NONE;
}

// 0x00846880 (:1106)
void hud_ui_activity_complete_start_tally(int num)
{
    if ((unsigned int)num < 3) // Magic number: 3, the tallies.
    {
        Ac_info.tallies[num].tally_time = 0.0f;
    }
}
