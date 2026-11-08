// Original: volition/rfg/code/hud/hud_ui_activity_briefing.cpp
//
// The activity briefing screen. Only the Demolitions Master has one: its title, weapons, times and hints, before and
// after the challenge, with accept and reject answered through the activity's callbacks.

#include "volition/rfg/code/hud/hud_ui_activity_briefing.h"

#include "volition/audio_lib/code/audiolib_api.h"
#include "volition/interface_lib/code/vint_data_item.h"
#include "volition/interface_lib/code/vint_external.h"
#include "volition/interface_lib/code/vint_variant.h"
#include "volition/rfg/code/control/control_map.h"
#include "volition/rfg/code/event_logging/gamelog.h"
#include "volition/rfg/code/gameplay/activities/activity_demolitions_master_internal.h"
#include "volition/rfg/code/gameplay/upgrades.h"
#include "volition/rfg/code/gamewide/game_clock.h"
#include "volition/rfg/code/hud/hud.h"
#include "volition/rfg/code/interface/localization/localization_shared.h"
#include "volition/rfg/code/interface/ui_document_manager.h"
#include "volition/rfg/code/interface/ui_menu.h"
#include "volition/rfg/code/interface/ui_message_box.h"
#include "volition/rfg/code/level/world.h"
#include "volition/rfg/code/multi/multi_core/include/multi_core_platform.h"
#include "volition/rfg/code/objects/humans/human_voice/human_voice.h"
#include "volition/rfg/code/objects/humans/human_voice/human_voice_lines.h"
#include "volition/rfg/code/objects/humans/human_voice/human_voice_shared.h"
#include "volition/rfg/code/rfg.h"
#include "volition/rfg/code/scripting/dialogue_manager.h"
#include "volition/rfg/code/scripting/public_address.h"
#include "volition/vlib/code/input/input.h"
#include "volition/vlib/code/time/timestamp.h"

#include "keen/base/string.hpp"
#include "keen/base/trace.hpp"

#include <stdio.h>

// ---- globals
int Play_response_id; // 0x02B95694, written here and never read

// ---- file-static globals
typedef void (*briefing_callback)();
// rfg.exe's tables have one slot each, ABT_DEMOLITIONS_MASTER.
static const briefing_callback Accept_callbacks[ABT_LAST] = {demolitions_master_accept}; // 0x012FB260
static const briefing_callback Reject_callbacks[ABT_LAST] = {demolitions_master_reject}; // 0x012FB264
static const char* Briefing_persona_names[ABT_LAST] = {"DAN"}; // 0x01660F5C
static const char* cUI_demolitions_doc = "rfg_ui_demolitions"; // 0x01660F4C
static const char* kHintImg2 = "demo_hint_b"; // 0x01662200
static const char* kHintText2 = "demo_hint_text_1"; // 0x01662204
static const char* kHintImg = "demo_hint_a"; // 0x01662208
static const char* kHintText = "demo_hint_text_2"; // 0x0166220C
static int Doc_handle = -1; // 0x01660F50
static activity_briefing_type Activity_type = ABT_LAST; // 0x01660F54
static voice_line_handle Audio_line = INVALID_VOICE_LINE_HANDLE; // 0x01660F58
static timestamp_realtime Fake_load_delay; // 0x016617E4
static bool Assets_loaded; // 0x02B9DC7E
static bool Exit; // 0x02B9DC7F
static int Data_handles; // 0x02B9DC94
static unsigned int Control_id_briefing; // 0x02B9DC98
static bool Accepted; // 0x02B9DC9C
static bool Audio_played; // 0x02B9DC9D
static bool Need_to_push_state; // 0x02B9DC9E

// 0x00844C50 (:77)
// Nothing calls these two in rfg.exe.
static void activity_managed_layer_load_begin()
{
    Fake_load_delay.set(500); // Magic number: half a second.
}

// 0x00844C60 (:81)
static bool activity_managed_layer_load_complete()
{
    return Fake_load_delay.elapsed();
}

// 0x00844C70 (:88)
static void load_demolitions_master_data()
{
    if (Doc_handle == -1)
    {
        return;
    }
    vint_data_item* item = vint_data_item::create_new("DemolitionsInfo");
    if (item == nullptr)
    {
        return;
    }
    static const vint_variant_type types[26] = {
        VINT_PROP_TYPE_INT, VINT_PROP_TYPE_STRING, VINT_PROP_TYPE_STRING,
        VINT_PROP_TYPE_STRING, VINT_PROP_TYPE_INT, VINT_PROP_TYPE_STRING, VINT_PROP_TYPE_INT,
        VINT_PROP_TYPE_STRING, VINT_PROP_TYPE_INT, VINT_PROP_TYPE_STRING, VINT_PROP_TYPE_INT,
        VINT_PROP_TYPE_STRING, VINT_PROP_TYPE_STRING, VINT_PROP_TYPE_STRING, VINT_PROP_TYPE_STRING, VINT_PROP_TYPE_STRING,
        VINT_PROP_TYPE_STRING, VINT_PROP_TYPE_STRING, VINT_PROP_TYPE_STRING, VINT_PROP_TYPE_STRING, VINT_PROP_TYPE_STRING,
        VINT_PROP_TYPE_STRING, VINT_PROP_TYPE_STRING, VINT_PROP_TYPE_STRING, VINT_PROP_TYPE_STRING, VINT_PROP_TYPE_STRING};
    item->set_template(types, 26);
    Data_handles = (int)item->get_handle();
    ui_add_data_item(Doc_handle, Data_handles);
}

// 0x00844D50 (:144)
static void process_demolitions_data()
{
    vint_data_item* item = vint_data_item::find(Data_handles);
    if (item == nullptr)
    {
        return;
    }
    char label[64];
    char value[64];
    char button[128];
    char image[128];
    item->set_element(0, vint_variant(Demolitions_master_activity.get_alt_interface_state()));
    item->set_element(1, vint_variant(Demolitions_master_activity.get_title()));
    const char* subtitle = Demolitions_master_activity.get_subtitle();
    if (Demolitions_master_activity.current_state == DM_STATE_POSTGAME_BRIEFING)
    {
        switch (gamelog_get_activity_failed_reason())
        {
        case GLCR_PLAYER_DIED:
            subtitle = "MENU_YOU_DIED";
            break;
        case GLCR_OUT_OF_TIME:
            subtitle = "ACTIVITY_FAIL_OUT_OF_TIME";
            break;
        case GLCR_VEHICLE_DESTROYED:
            subtitle = "ACTIVITY_FAIL_VEH_DESTROYED";
            break;
        case GLCR_ESCAPED:
            subtitle = "ACTIVITY_FAIL_VEH_ESCAPED";
            break;
        case GLCR_OUT_OF_RANGE:
            subtitle = "ACTIVITY_FAIL_LEFT_AREA";
            break;
        case GLCR_OUT_OF_AMMO:
            subtitle = "ACTIVITY_FAIL_OUT_OF_AMMO";
            break;
        default:
            break;
        }
    }
    item->set_element(2, vint_variant(subtitle));
    // Elements 3 to 10: the four weapons' icons and ammo.
    for (int i = 0; i < 4; i++)
    {
        item->set_element(3 + i * 2, vint_variant(Demolitions_master_activity.get_weapon_icon_name(i)));
        item->set_element(4 + i * 2, vint_variant(Demolitions_master_activity.get_weapon_ammo(i)));
    }
    // A time below 1 sends a null string. The goal and par times are in seconds, the player's and best times in ms.
    vint_encode_wide_text(label, 0x40, localization_get_display_string_req("ACT_DM_RESULTS_GOAL_TIME", nullptr));
    item->set_element(11, vint_variant(label));
    int time = Demolitions_master_activity.get_goal_time();
    if (time < 1)
    {
        item->set_element(12, vint_variant((const char*)nullptr));
    }
    else
    {
        sprintf(value, "%.02d:%.02d", time / 60, time % 60);
        item->set_element(12, vint_variant(value));
    }
    item->set_element(13, vint_variant(Demolitions_master_activity.get_description()));
    vint_encode_wide_text(label, 0x40, localization_get_display_string_req("ACT_DM_RESULTS_YOU_TIME", nullptr));
    item->set_element(14, vint_variant(label));
    time = Demolitions_master_activity.get_display_time();
    if (time < 1)
    {
        item->set_element(15, vint_variant((const char*)nullptr));
    }
    else
    {
        sprintf(value, "%.02d:%.02d", time / 60000, (time % 60000) / 1000);
        item->set_element(15, vint_variant(value));
    }
    vint_encode_wide_text(label, 0x40, localization_get_display_string_req("ACT_DM_RESULTS_PAR_TIME", nullptr));
    item->set_element(16, vint_variant(label));
    time = Demolitions_master_activity.get_par_time();
    if (time < 1)
    {
        item->set_element(17, vint_variant((const char*)nullptr));
    }
    else
    {
        sprintf(value, "%.02d:%.02d", time / 60, time % 60);
        item->set_element(17, vint_variant(value));
    }
    vint_encode_wide_text(label, 0x40, localization_get_display_string_req("ACT_DM_RESULTS_BEST_TIME", nullptr));
    item->set_element(18, vint_variant(label));
    time = Demolitions_master_activity.get_best_time();
    if (time < 1)
    {
        item->set_element(19, vint_variant((const char*)nullptr));
    }
    else
    {
        sprintf(value, "%.02d:%.02d", time / 60000, (time % 60000) / 1000);
        item->set_element(19, vint_variant(value));
    }
    item->set_element(20, vint_variant(Demolitions_master_activity.get_first_hint()));
    item->set_element(21, vint_variant(Demolitions_master_activity.get_second_hint()));
    vint_encode_wide_text(label, 0x40, localization_get_display_string_req("ACT_DM_RESULTS_SALVAGE", nullptr));
    item->set_element(22, vint_variant(label));
    sprintf(value, "+%i", Demolitions_master_activity.get_salvage_received());
    item->set_element(23, vint_variant(value));
    controls_get_button_image_name(button, 0x80, CBA_MENU_SELECT, Player_controller_index);
    keen::formatString(image, 0x80, "[image:%s]", button);
    item->set_element(24, vint_variant(image));
    controls_get_button_image_name(button, 0x80, CBA_MENU_ABORT, Player_controller_index);
    keen::formatString(image, 0x80, "[image:%s]", button);
    item->set_element(25, vint_variant(image));
}

// 0x008454F0 (:296)
// 1 over the accept hint, 2 over the reject hint, which wins when both answer.
static int check_for_user_mouse_input()
{
    const bool accept = ui_mouse_over_hint_combo(kHintImg, kHintText, Doc_handle, true);
    const bool reject = ui_mouse_over_hint_combo(kHintImg2, kHintText2, Doc_handle, true);
    return reject ? 2 : (int)accept;
}

// 0x00845550 (:322)
static void check_for_user_input()
{
    // rfg.exe reads hud.cpp's Button_input_enabled directly, through the unity file.
    if (Exit || !hud_is_button_input_enabled())
    {
        return;
    }
    const int mouse = check_for_user_mouse_input();
    if (controls_is_button_just_pressed(CBA_MENU_SELECT, Player_controller_index) || mouse == 1)
    {
        dialogue_manager_cancel_all();
        Fake_load_delay.set(500); // Magic number: half a second before the screen pops.
        Accept_callbacks[Activity_type]();
        Exit = true;
        Accepted = true;
        ui_menu_play_sound(MENU_SOUND_PRESS_SELECT);
        return;
    }
    if (controls_is_button_just_pressed(CBA_MENU_ABORT, Player_controller_index) || mouse != 0)
    {
        // The results briefing cannot be rejected.
        if (Activity_type == ABT_DEMOLITIONS_MASTER && Demolitions_master_activity.current_state == DM_STATE_POSTGAME_BRIEFING)
        {
            return;
        }
        dialogue_manager_cancel_briefing();
        Reject_callbacks[Activity_type]();
        Exit = true;
        Accepted = false;
        ui_menu_play_sound(MENU_SOUND_PRESS_ABORT);
    }
    // rfg.exe goes on to test Exit here and play a sound by Accepted, which cannot run: Exit is false on this path.
}

// 0x00855FB0 (:383)
void hud_ui_activity_briefing_init()
{
    state_info info;
    info.hide_cursor = false;
    info.enter = hud_ui_activity_briefing_enter;
    info.exit = hud_ui_activity_briefing_exit;
    info.process = hud_ui_activity_briefing_process;
    info.handshake = hud_ui_activity_briefing_handshake;
    info.render = hud_ui_activity_briefing_render;
    info.transparent = false;
    info.pause_beneath = false;
    gameseq_register_state(GS_INGAME_ACTIVITY_BRIEFING, info);
}

// 0x00845640 (:399)
void hud_ui_activity_briefing_enter(game_state prev_state)
{
    (void)prev_state;
    Exit = false;
    Assets_loaded = false;
    Audio_played = false;
    Doc_handle = ui_load_document(cUI_demolitions_doc, DFIT_ALPHA, 0.5f, DLT_NORMAL, true, STREAM_PRIORITY_AUDIO_CACHE_LOAD, false);
    if (Doc_handle == -1)
    {
        keen::debug::nativePrintArgs("Int3 critical function at %s(%d)\n", __FILE__, __LINE__);
        dialogue_manager_cancel_briefing();
        Reject_callbacks[Activity_type]();
        Accepted = false;
        Exit = true;
        return;
    }
    ui_set_screen_blur(true, false);
    Hide_hud_ref_count++;
    Play_response_id = -1;
    Control_id_briefing = ui_request_control_id();
}

// 0x008456F0 (:430)
void hud_ui_activity_briefing_exit(game_state new_state)
{
    (void)new_state;
    ui_unload_document(Doc_handle, DFOT_NONE, 0.5f);
    ui_relinquish_control_id(Control_id_briefing);
    Control_id_briefing = 0;
    if (Hide_hud_ref_count > 0)
    {
        Hide_hud_ref_count--;
    }
    ui_set_screen_blur(false, true);
    Audio_line = INVALID_VOICE_LINE_HANDLE;
    upgrade_unlock_from_activities();
}

// 0x00845750 (:447)
void hud_ui_activity_briefing_process()
{
    input_set_feedback(multi_core_platform_get_active_controller(), 0.0f, 0.0f);
    const bool msgbox_active = ui_is_msgbox_active(-2); // Magic number: -2, any message box.
    if (msgbox_active)
    {
        ui_process(Control_id_briefing, true);
        return;
    }
    bool just_loaded = false;
    if (Exit)
    {
        if (!Accepted || Fake_load_delay.elapsed())
        {
            gameseq_pop_state();
        }
    }
    else if (!Assets_loaded)
    {
        switch (ui_get_doc_asset_state(Doc_handle))
        {
        case ST_UNLOADED:
            keen::debug::nativePrintArgs("Int3 critical function at %s(%d)\n", __FILE__, __LINE__);
            // fall through
        case ST_ERROR:
            ui_unload_document(Doc_handle, DFOT_NONE, 0.5f);
            Doc_handle = -1;
            gameseq_pop_state();
            break;
        case ST_LOADING:
            break;
        case ST_LOADED:
            Assets_loaded = true;
            just_loaded = true;
            load_demolitions_master_data();
            break;
        default:
            keen::debug::nativePrintArgs("Int3 critical function at %s(%d)\n", __FILE__, __LINE__);
            break;
        }
    }
    if (!Assets_loaded)
    {
        ui_process(Control_id_briefing, false);
        return;
    }
    if (just_loaded)
    {
        int cue;
        audiolib_cue_get_id("SYS_DEMOMASTER_POPUP", &cue);
        audiolib_cue_play(cue, nullptr, nullptr);
    }
    check_for_user_input();
    if (Activity_type == ABT_DEMOLITIONS_MASTER)
    {
        process_demolitions_data();
    }
    if (!Exit && !Audio_played)
    {
        Audio_played = true;
        if (Audio_line != INVALID_VOICE_LINE_HANDLE)
        {
            // Magic number: -1, the base game; a DLC bundle has the radio persona speak.
            const human_voice_persona* persona = World.dlc_bundle_id == -1 ? human_voice_persona_from_name(Briefing_persona_names[Activity_type]) : human_voice_radio_persona();
            if (persona != nullptr)
            {
                dialogue_manager_start_dialogue(Audio_line, persona, DIALOGUE_PRIORITY_BRIEFING, INVALID_PA_HANDLE, nullptr);
            }
        }
    }
    ui_add_for_processing(Doc_handle);
    ui_process(Control_id_briefing, false);
}

// 0x00845940 (:535)
void hud_ui_activity_briefing_handshake(rl_renderer*)
{
}

// 0x00845950 (:542)
void hud_ui_activity_briefing_render(rl_renderer*)
{
    if (!Exit && Assets_loaded)
    {
        ui_add_for_rendering(Doc_handle);
    }
    ui_render(Control_id_briefing, false);
}

// 0x00845990 (:556)
void hud_ui_activity_briefing_do_frame()
{
    if (Need_to_push_state && !game_is_paused() && gameseq_get_pending_state() == GS_NONE)
    {
        gameseq_push_state(GS_INGAME_ACTIVITY_BRIEFING, true, false);
        Need_to_push_state = false;
    }
}

// 0x008459D0 (:568)
void hud_ui_activity_briefing_level_shutdown()
{
    Need_to_push_state = false;
}

// 0x008459E0 (:574)
void hud_ui_activity_briefing_start(activity_briefing_type type, voice_line_handle line_handle)
{
    if ((int)type < ABT_LAST)
    {
        Activity_type = type;
        Audio_line = line_handle;
        Need_to_push_state = true;
    }
}

// 0x00845A00 (:586)
void hud_ui_activity_briefing_done()
{
}
