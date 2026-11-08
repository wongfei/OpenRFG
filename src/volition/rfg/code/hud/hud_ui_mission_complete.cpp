// Original: volition/rfg/code/hud/hud_ui_mission_complete.cpp
//
// The mission results screen: a won mission tallies salvage and morale, then cleans the mission up behind a fade; a
// failed one reloads, or from a checkpoint offers to cancel the mission. Placement of mission_complete_info inferred.

#include "volition/rfg/code/hud/hud_ui_mission_complete.h"

#include "volition/audio_lib/code/audiolib_api.h"
#include "volition/audio_lib/code/audiolib_att_loop_rel.h"
#include "volition/audio_lib/code/audiolib_music.h"
#include "volition/audio_lib/code/audiolib_vehicle_engine.h"
#include "volition/interface_lib/code/objects/animation/vint_object_tween.h"
#include "volition/interface_lib/code/objects/vint_object_base.h"
#include "volition/interface_lib/code/vint_data_item.h"
#include "volition/interface_lib/code/vint_variant.h"
#include "volition/rfg/code/control/control_map.h"
#include "volition/rfg/code/gameplay/gameflow/gfm_control_morale.h"
#include "volition/rfg/code/gameplay/mission_info.h"
#include "volition/rfg/code/gameplay/missions.h"
#include "volition/rfg/code/gameplay/object_district.h"
#include "volition/rfg/code/gameplay/upgrades.h"
#include "volition/rfg/code/gamewide/game_clock.h"
#include "volition/rfg/code/gamewide/gameaudio.h"
#include "volition/rfg/code/hud/hud.h"
#include "volition/rfg/code/interface/ui_document_manager.h"
#include "volition/rfg/code/interface/ui_menu.h"
#include "volition/rfg/code/interface/ui_message_box.h"
#include "volition/rfg/code/level/world.h"
#include "volition/rfg/code/objects/object_type.h"
#include "volition/rfg/code/rfg.h"
#include "volition/vlib/code/math/floating.h"
#include "volition/vlib/code/os/frametime.h"
#include "volition/vlib/code/time/timestamp.h"
#include "volition/vlib/code/util/string_util.h"

#include "keen/base/string.hpp"
#include "keen/base/trace.hpp"

#include <string.h>

// size 0x15C
struct mission_complete_info
{
    bool exiting; // 0x00
    bool loaded; // 0x01
    bool allow_input; // 0x02
    bool restart_mission; // 0x03
    bool need_to_push_state; // 0x04
    mission_complete_result complete_result; // 0x08
    bool autosave; // 0x0C
    int doc_handle; // 0x10
    int data_handles[2]; // 0x14
    unsigned int control_id; // 0x1C
    int input_id; // 0x20
    float tally_times[3]; // 0x24
    int tally_amts[3]; // 0x30
    char mission_description[128]; // 0x3C
    char mission_subtitle[128]; // 0xBC
    int reward_salvage; // 0x13C
    int morale_multiplier; // 0x140
    bool cutscene_played; // 0x144
    bool capstone; // 0x145
    int msg_box_handle; // 0x148
    int sound_money_loop; // 0x14C
    int sound_money_loop_release; // 0x150
    int play_money_loop; // 0x154
    unsigned int district_handle; // 0x158
};

// ---- file-static globals
static const char* cUI_mission_complete_doc = "rfg_ui_mission_complete"; // 0x016610EC
static const char* cMC_sound_money_loop_complete = "HUD_MIS_TALLY_LOOP"; // 0x016610F0
static const char* cMC_sound_money_loop_release_complete = "HUD_MIS_TALLY_RELEASE"; // 0x016610F4
static const char* cDefaultFailureString = "MISS_MISSION_FAILED"; // 0x016610F8
static const char* cDefaultCompleteString = "MISS_MISSION_COMPLETE"; // 0x016610FC
static const char* cButtonToUse = "MENU_CONTINUE"; // 0x01661100
static const char* cButtonToCancle = "MENU_CANCEL_MISSION"; // 0x01661104
static const char* kHintText2 = "mc_hint_2_text"; // 0x0166226C
static const char* kHintImg2 = "mc_hint_2"; // 0x01662270
static const char* kHintText1 = "mc_hint_1_text"; // 0x01662274
static const char* kHintImg1 = "mc_hint_1"; // 0x01662278
static timestamp_realtime override_timer; // 0x016617F4
static mission_complete_info Mc_info; // 0x02B95698
static bool ExitingComplete; // 0x02B9DD05
static bool Clean_up; // 0x02B9DD06
static int Tally_iteration; // 0x02B9DD0C

// 0x0084A7D0 (:185)
static void humc_request_exit()
{
    Mc_info.exiting = true;
    if (Mc_info.complete_result == MCR_COMPLETED)
    {
        ui_fade_to_black_fade_out(false);
        ExitingComplete = true;
        ui_unload_document(Mc_info.doc_handle, DFOT_NONE, 0.5f);
        mission_notify_new();
        const district* dp = district_local_player();
        if (dp != nullptr && gfm_get_edf_control(dp) == 0.0f && missions_left_in_district(dp->get_handle(), true) == 0)
        {
            audiolib_vehicle_volume_delay(2000); // Magic number: 2 seconds.
        }
        return;
    }
    if (World.get_world_state() == WSM_CHECKPOINT && Mc_info.restart_mission)
    {
        World.set_world_state(WSM_DEFAULT);
    }
    World.set_pending_territory_load("", false, false, false, World.dlc_bundle_id, false);
}

// 0x0084A890 (:220)
static bool mission_complete_confirm_msgbox_cb(int mbox_handle, int selection_index, msgbox_choice choice)
{
    (void)selection_index;
    if (Mc_info.msg_box_handle != mbox_handle)
    {
        return false;
    }
    if (choice == MBC_OK)
    {
        Mc_info.restart_mission = true;
        humc_request_exit();
    }
    else if (choice == MBC_ABORT)
    {
        Mc_info.allow_input = false;
        Mc_info.restart_mission = false;
    }
    Mc_info.msg_box_handle = -1;
    return true;
}

// 0x0084A8E0 (:250)
static void humc_init_info()
{
    Mc_info.mission_description[0] = '\0';
    Mc_info.mission_subtitle[0] = '\0';
    Mc_info.reward_salvage = 0;
    Mc_info.morale_multiplier = 1;
    Mc_info.doc_handle = -1;
    Mc_info.complete_result = MCR_NONE;
    Mc_info.autosave = false;
    Mc_info.exiting = false;
    Mc_info.loaded = false;
    Mc_info.need_to_push_state = false;
    Mc_info.control_id = 0;
    Mc_info.input_id = -1;
    Mc_info.cutscene_played = true;
    Mc_info.capstone = false;
    Mc_info.msg_box_handle = -1;
}

// 0x0084A940 (:270)
static void humc_load_ui_information()
{
    if (Mc_info.doc_handle == -1)
    {
        return;
    }
    unsigned int handle = 0;
    vint_data_item* item = vint_data_item::create_new("MissionComplete");
    if (item != nullptr)
    {
        static const vint_variant_type types[11] = {VINT_PROP_TYPE_BOOL, VINT_PROP_TYPE_BOOL, VINT_PROP_TYPE_STRING, VINT_PROP_TYPE_INT, VINT_PROP_TYPE_INT, VINT_PROP_TYPE_STRING, VINT_PROP_TYPE_STRING, VINT_PROP_TYPE_STRING, VINT_PROP_TYPE_STRING, VINT_PROP_TYPE_STRING, VINT_PROP_TYPE_UINT};
        item->set_template(types, 11);
        handle = item->get_handle();
    }
    Mc_info.data_handles[0] = (int)handle;
    ui_add_data_item(Mc_info.doc_handle, (int)handle);
    handle = 0;
    item = vint_data_item::create_new("MissionTallys");
    if (item != nullptr)
    {
        static const vint_variant_type types[3] = {VINT_PROP_TYPE_INT, VINT_PROP_TYPE_INT, VINT_PROP_TYPE_INT};
        item->set_template(types, 3);
        handle = item->get_handle();
    }
    Mc_info.data_handles[1] = (int)handle;
    ui_add_data_item(Mc_info.doc_handle, (int)handle);
}

// 0x0084AA50 (:338)
static void humc_unload_ui_information()
{
    if (!ExitingComplete)
    {
        ui_unload_document(Mc_info.doc_handle, DFOT_NONE, 0.5f);
    }
    ui_relinquish_control_id(Mc_info.control_id);
    Mc_info.control_id = 0;
}

// The mission's district, or null. rfg.exe inlines this in both of its users.
static district* humc_get_district()
{
    if (Mc_info.district_handle == 0xFFFFFFFF)
    {
        return nullptr;
    }
    object* objp = World.object_from_handle(Mc_info.district_handle);
    return objp != nullptr && objp->get_obj_type() == OT_DISTRICT ? (district*)objp : nullptr;
}

// The district's morale as a whole percentage. It asks with a null district too, as rfg.exe does.
static int humc_get_morale_percent(district* dp)
{
    return fl_round((float)((double)gfm_get_district_morale_percent(dp) * 100.0));
}

// 0x00858CD0 (:348)
static void process_mission_information(float dtime)
{
    (void)dtime;
    vint_data_item* item = vint_data_item::find(Mc_info.data_handles[0]);
    if (item == nullptr)
    {
        return;
    }
    str_make_upper(Mc_info.mission_description);
    district* dp = humc_get_district();
    const int morale = humc_get_morale_percent(dp);
    char button[64];
    char image[128];
    item->set_element(0, vint_variant(true));
    item->set_element(1, vint_variant(Mc_info.complete_result == MCR_COMPLETED));
    item->set_element(2, vint_variant(Mc_info.mission_description));
    item->set_element(3, vint_variant(morale));
    item->set_element(4, vint_variant(Mc_info.morale_multiplier));
    if (Mc_info.allow_input)
    {
        controls_get_button_image_name(button, 0x40, ui_get_control_button_value(BC_SELECT), Player_controller_index);
        keen::formatString(image, 0x80, "[image:%s]", button);
        item->set_element(5, vint_variant(image));
        if (Mc_info.complete_result == MCR_COMPLETED || World.get_world_state() != WSM_CHECKPOINT)
        {
            item->set_element(6, vint_variant((const char*)nullptr));
        }
        else
        {
            controls_get_button_image_name(button, 0x40, ui_get_control_button_value(BC_ABORT), Player_controller_index);
            keen::formatString(image, 0x80, "[image:%s]", button);
            item->set_element(6, vint_variant(image));
        }
    }
    else
    {
        item->set_element(5, vint_variant((const char*)nullptr));
        item->set_element(6, vint_variant((const char*)nullptr));
    }
    item->set_element(7, vint_variant(cButtonToUse));
    item->set_element(8, vint_variant(cButtonToCancle));
    item->set_element(9, vint_variant(Mc_info.mission_subtitle));
    item->set_element(10, vint_variant(dp != nullptr ? dp->get_localized_name_hash() : 0u));
}

// 0x00859020 (:406)
static void process_mission_tallies(float dtime)
{
    vint_data_item* item = vint_data_item::find(Mc_info.data_handles[1]);
    if (item == nullptr)
    {
        return;
    }
    const int salvage = Mc_info.reward_salvage;
    const int morale = humc_get_morale_percent(humc_get_district()) * Mc_info.morale_multiplier;
    if (!Mc_info.allow_input)
    {
        for (int i = 0; i < 3; i++)
        {
            float& time = Mc_info.tally_times[i];
            if (time < 0.0f)
            {
                continue;
            }
            time = (float)((double)time + (double)dtime);
            if (time > 1.5f) // Magic number: how long a tally counts up, in seconds.
            {
                time = 1.5f;
                if (i == 2)
                {
                    Mc_info.allow_input = true; // The last tally has finished.
                }
                if (Tally_iteration <= i && Mc_info.play_money_loop != -1)
                {
                    audiolib_att_loop_rel_stop(Mc_info.play_money_loop, 100); // Magic number: 100 ms, as below.
                    Mc_info.play_money_loop = -1;
                }
            }
            const double fraction = (double)time / 1.5;
            if (i == 0)
            {
                Mc_info.tally_amts[0] = (int)(fraction * (double)(float)salvage);
            }
            else if (i == 1)
            {
                Mc_info.tally_amts[1] = (int)(fraction * (double)(float)morale);
            }
            else
            {
                Mc_info.tally_amts[2] = (int)(fraction * (double)(float)(morale + salvage));
            }
            if (time < 1.5f && Mc_info.play_money_loop == -1 && Mc_info.tally_amts[i] > 0)
            {
                Tally_iteration = i;
                audiolib_alr_params params;
                memset(&params, 0, sizeof(params));
                params.loop_cue_id = Mc_info.sound_money_loop;
                params.release_cue_id = Mc_info.sound_money_loop_release;
                params.num_shots = 1;
                audiolib_att_loop_rel_start(&params, &Mc_info.play_money_loop);
                audiolib_play_set_volume(Mc_info.play_money_loop, 1.0f, 100);
            }
        }
    }
    else
    {
        Mc_info.tally_amts[0] = salvage;
        Tally_iteration = 0;
        Mc_info.tally_amts[1] = morale;
        Mc_info.tally_amts[2] = morale + salvage;
        if (Mc_info.play_money_loop != -1)
        {
            audiolib_att_loop_rel_stop(Mc_info.play_money_loop, 100);
            Mc_info.play_money_loop = -1;
        }
    }
    item->set_element(0, vint_variant(Mc_info.tally_amts[0]));
    item->set_element(1, vint_variant(Mc_info.tally_amts[1]));
    item->set_element(2, vint_variant(Mc_info.tally_amts[2]));
}

// 0x00859350 (:502)
static void humc_process_data_items()
{
    if (!Mc_info.loaded)
    {
        return;
    }
    process_mission_information(Frametime_real);
    if (Mc_info.complete_result == MCR_COMPLETED)
    {
        process_mission_tallies(Frametime_real);
        return;
    }
    if (Mc_info.allow_input)
    {
        return;
    }
    // A failed mission has no tallies, so input waits for this tween of the hints.
    vint_object_base* object = vint_object_base::find_object("mc_hints_main_alpha_twn_12", ui_get_vint_document(Mc_info.doc_handle));
    if (object == nullptr)
    {
        Mc_info.allow_input = true;
        return;
    }
    vint_variant state;
    ((vint_object_tween*)object)->get_state(state, 0); // rfg.exe calls vtbl+0x44 on the object it finds.
    if ((unsigned int)state == 4) // Magic number: 4, the tween state rfg.exe tests for.
    {
        Mc_info.allow_input = true;
    }
}

// 0x008695F0 (:540)
void hud_ui_mission_complete_init()
{
    state_info info;
    info.enter = hud_ui_mission_complete_enter;
    info.exit = hud_ui_mission_complete_exit;
    info.process = hud_ui_mission_complete_process;
    info.handshake = hud_ui_mission_complete_handshake;
    info.render = hud_ui_mission_complete_render;
    info.transparent = false;
    info.pause_beneath = false;
    gameseq_register_state(GS_INGAME_MISSION_COMPLETE, info);
    humc_init_info();
}

// 0x0084AAA0 (:558)
void hud_ui_mission_complete_enter(game_state prev_state)
{
    (void)prev_state;
    // Magic number: -1, a tally that has not started. The document's Lua starts each one.
    Mc_info.tally_times[0] = -1.0f;
    Mc_info.tally_times[1] = -1.0f;
    Mc_info.tally_times[2] = -1.0f;
    Mc_info.input_id = -1;
    ExitingComplete = false;
    Clean_up = false;
    Mc_info.msg_box_handle = -1;
    Show_minimap = false;
    Mc_info.exiting = false;
    Mc_info.loaded = false;
    Mc_info.allow_input = false;
    Mc_info.restart_mission = false;
    Mc_info.need_to_push_state = false;
    Mc_info.play_money_loop = -1;
    Mc_info.doc_handle = ui_load_document(cUI_mission_complete_doc, DFIT_ALPHA, 0.5f, DLT_NORMAL, true, STREAM_PRIORITY_AUDIO_CACHE_LOAD, false);
    if (Mc_info.doc_handle == -1)
    {
        keen::debug::nativePrintArgs("Int3 critical function at %s(%d)\n", __FILE__, __LINE__);
        Mc_info.allow_input = false;
        humc_request_exit();
        upgrade_show_new_upgrades(true, 0);
        return;
    }
    Hide_hud_ref_count++;
    Mc_info.control_id = ui_request_control_id();
    audiolib_interactive_music_set_fade(0.04f, 3000); // Magic numbers: the music drops to 4% over 3 seconds.
    audiolib_cue_get_id(cMC_sound_money_loop_complete, &Mc_info.sound_money_loop);
    audiolib_cue_get_id(cMC_sound_money_loop_release_complete, &Mc_info.sound_money_loop_release);
    ui_wait_for_button_ups();
    ui_set_screen_blur(true, false);
    override_timer.set(7500); // Magic number: input is allowed after 7.5 seconds whatever the tallies do.
}

// 0x0084ABD0 (:624)
void hud_ui_mission_complete_exit(game_state new_state)
{
    Show_minimap = true;
    Mc_info.mission_subtitle[0] = '\0';
    ui_relinquish_input_id(Mc_info.input_id);
    if (Mc_info.control_id != 0)
    {
        ui_relinquish_control_id(Mc_info.control_id);
        Mc_info.control_id = 0;
    }
    // Original quirk, kept: this relinquishes control id 0 a second time.
    humc_unload_ui_information();
    if (Hide_hud_ref_count > 0)
    {
        Hide_hud_ref_count--;
    }
    if (!Mc_info.cutscene_played)
    {
        ui_fade_to_black_fade_in(false);
    }
    ui_set_screen_blur(false, false);
    audiolib_interactive_music_stop_fade(500); // Magic number: half a second.
    if (Mc_info.autosave && new_state != GS_MAINMENU)
    {
        World.save_and_continue(nullptr, false, true);
    }
    if (Mc_info.play_money_loop != -1)
    {
        audiolib_att_loop_rel_stop(Mc_info.play_money_loop, 100);
        Mc_info.play_money_loop = -1;
    }
    if (Mc_info.msg_box_handle != -1)
    {
        ui_hide_msgbox(Mc_info.msg_box_handle, false);
    }
}

// 0x0084ACE0 (:769)
// 1 over the first hint, 2 over the second, and the first wins. Text comes before image here, against the parameter
// names of ui_mouse_over_hint_combo, as in rfg.exe.
static int hud_ui_mission_complete_hint_click()
{
    const bool first = ui_mouse_over_hint_combo(kHintText1, kHintImg1, Mc_info.doc_handle, true);
    const bool second = ui_mouse_over_hint_combo(kHintText2, kHintImg2, Mc_info.doc_handle, true);
    if (first)
    {
        return 1;
    }
    return second ? 2 : 0;
}

// 0x00859400 (:795)
void hud_ui_mission_complete_process()
{
    const bool msgbox_active = ui_is_msgbox_active(-2); // Magic number: -2, any message box.
    if (!msgbox_active)
    {
        if (ExitingComplete)
        {
            // The clean up waits for the fade, and the state pops on the frame after it.
            if (Clean_up)
            {
                gameseq_pop_state();
            }
            else if (ui_fade_to_black_is_faded())
            {
                mission_terminate_cleanup();
                Clean_up = true;
            }
        }
        else if (!Mc_info.loaded)
        {
            switch (ui_get_doc_asset_state(Mc_info.doc_handle))
            {
            case ST_UNLOADED:
                keen::debug::nativePrintArgs("Int3 critical function at %s(%d)\n", __FILE__, __LINE__);
                // fall through
            case ST_ERROR:
                humc_request_exit();
                break;
            case ST_LOADING:
                break;
            case ST_LOADED:
                humc_load_ui_information();
                Mc_info.loaded = true;
                Mc_info.input_id = ui_register_for_processed_input(IRT_UNLIMITED_INPUT, false);
                break;
            default:
                keen::debug::nativePrintArgs("Int3 critical function at %s(%d)\n", __FILE__, __LINE__);
                break;
            }
        }
        // Asked even while nothing can be pressed, because it also highlights the hints.
        const int hint = hud_ui_mission_complete_hint_click();
        if (Mc_info.loaded && !Mc_info.exiting)
        {
            bool answered = false;
            if (Mc_info.allow_input)
            {
                // Magic numbers: 10 and 11, the document's accept and cancel buttons.
                if (ui_get_button_info(10, Mc_info.input_id) > 0.0f || (ui_get_current_input_id() == Mc_info.input_id && hint == 1))
                {
                    Mc_info.allow_input = false;
                    humc_request_exit();
                    upgrade_show_new_upgrades(true, 0);
                    ui_menu_play_sound(MENU_SOUND_PRESS_SELECT);
                    answered = true;
                }
                else if (Mc_info.complete_result != MCR_COMPLETED && World.get_world_state() == WSM_CHECKPOINT && (ui_get_button_info(11, Mc_info.input_id) > 0.0f || (ui_get_current_input_id() == Mc_info.input_id && hint == 2)))
                {
                    Mc_info.msg_box_handle = ui_add_msgbox(MBT_ACCEPT_CANCEL_SELECT, "MENU_WARNING", "MENU_CANCEL_MISSION_CONFIRM", mission_complete_confirm_msgbox_cb, false, false, "MENU_CANCEL_MISSION", "MENU_CONTINUE", false);
                    Mc_info.allow_input = false;
                    Mc_info.restart_mission = true;
                    ui_menu_play_sound(MENU_SOUND_PRESS_ABORT);
                    answered = true;
                }
            }
            if (!answered)
            {
                humc_process_data_items();
                ui_add_for_processing(Mc_info.doc_handle);
            }
        }
    }
    if (override_timer.elapsed())
    {
        Mc_info.allow_input = true;
    }
    ui_process(Mc_info.control_id, msgbox_active);
}

// 0x0084AD40 (:901)
void hud_ui_mission_complete_handshake(rl_renderer*)
{
}

// 0x0084AD50 (:910)
void hud_ui_mission_complete_render(rl_renderer*)
{
    if (!Mc_info.exiting && Mc_info.loaded)
    {
        ui_add_for_rendering(Mc_info.doc_handle);
    }
    ui_render(Mc_info.control_id, false);
}

// 0x0084AD90 (:924)
void hud_ui_mission_complete_set_subtitle(const char* subtitle)
{
    if (subtitle != nullptr)
    {
        strncpy(Mc_info.mission_subtitle, subtitle, 0x80);
        Mc_info.mission_subtitle[0x7F] = '\0';
        str_make_upper(Mc_info.mission_subtitle);
    }
}

// 0x0084ADD0 (:934)
void hud_ui_mission_complete_do_frame()
{
    if (Mc_info.need_to_push_state && !game_is_paused() && gameseq_get_pending_state() == GS_NONE)
    {
        gameseq_push_state(GS_INGAME_MISSION_COMPLETE, true, false);
        Mc_info.need_to_push_state = false;
    }
}

// 0x0084AE10 (:945)
void hud_ui_mission_complete_level_shutdown()
{
    Mc_info.need_to_push_state = false;
}

// 0x0084AE20 (:951)
bool hud_ui_mission_complete_start(bool complete, const mission_info* const m_info)
{
    if (Mc_info.need_to_push_state || m_info == nullptr)
    {
        return false;
    }
    Mc_info.complete_result = complete ? MCR_COMPLETED : MCR_FAILED;
    Mc_info.autosave = complete && !m_info->no_autosave;
    audiolib_cue_play(complete ? Ga_cue_ids.mission_complete : Ga_cue_ids.mission_fail, nullptr, nullptr);
    strncpy(Mc_info.mission_description, m_info->description, 0x80);
    Mc_info.mission_description[0x7F] = '\0';
    if (Mc_info.mission_subtitle[0] == '\0')
    {
        strncpy(Mc_info.mission_subtitle, complete ? cDefaultCompleteString : cDefaultFailureString, 0x80);
        Mc_info.mission_subtitle[0x7F] = '\0';
    }
    Mc_info.reward_salvage = m_info->reward_salvage;
    Mc_info.morale_multiplier = m_info->morale_multiplier;
    Mc_info.district_handle = get_district_by_name(m_info->district_name);
    Mc_info.cutscene_played = m_info->cutscene_played && complete;
    Mc_info.capstone = m_info->capstone;
    Mc_info.need_to_push_state = true;
    return true;
}

// 0x0084AF30 (:1006)
void hud_ui_mission_complete_finished()
{
    Mc_info.complete_result = MCR_NONE;
}

// 0x0084AF40 (:1012)
mission_complete_result hud_ui_get_mission_complete_result()
{
    return Mc_info.complete_result;
}

// 0x0084AF50 (:1018)
void hud_ui_mission_complete_start_tally(int num)
{
    if ((unsigned int)num < 3) // Magic number: 3, the tallies.
    {
        Mc_info.tally_times[num] = 0.0f;
    }
}
