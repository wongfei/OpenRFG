// Original: volition/rfg/code/hud/hud_ui_district_liberation.cpp
//
// The district liberation screen, which also shows the upgrades that missions and activities unlock: a header, the
// control meter of a liberated district, then one page for each unlock.

#include "volition/rfg/code/hud/hud_ui_district_liberation.h"

#include "volition/audio_lib/code/audiolib_api.h"
#include "volition/interface_lib/code/objects/vint_object_base.h"
#include "volition/interface_lib/code/vint_data_item.h"
#include "volition/interface_lib/code/vint_external.h"
#include "volition/interface_lib/code/vint_variant.h"
#include "volition/rfg/code/control/control_map.h"
#include "volition/rfg/code/gameplay/gameflow/gfm_control_morale.h"
#include "volition/rfg/code/gameplay/mission_info.h"
#include "volition/rfg/code/gameplay/object_district.h"
#include "volition/rfg/code/gameplay/upgrades.h"
#include "volition/rfg/code/gamewide/downloadable_content.h"
#include "volition/rfg/code/gamewide/game_clock.h"
#include "volition/rfg/code/gamewide/gameaudio.h"
#include "volition/rfg/code/gamewide/gamemusic.h"
#include "volition/rfg/code/hud/hud_ui_mission_briefing.h"
#include "volition/rfg/code/interface/localization/localization_shared.h"
#include "volition/rfg/code/interface/ui_document_hud.h"
#include "volition/rfg/code/interface/ui_document_manager.h"
#include "volition/rfg/code/interface/ui_menu.h"
#include "volition/rfg/code/interface/ui_message_box.h"
#include "volition/rfg/code/level/world.h"
#include "volition/rfg/code/objects/humans/player.h"
#include "volition/rfg/code/objects/humans/player_meta_data.h"
#include "volition/rfg/code/objects/object_safehouse.h"
#include "volition/rfg/code/objects/object_type.h"
#include "volition/rfg/code/rfg.h"
#include "volition/vlib/code/math/matrix.h"
#include "volition/vlib/code/math/vector2.h"
#include "volition/vlib/code/math/vector3.h"
#include "volition/vlib/code/util/string_util.h"
#include "volition/vlib/code/util/unicode_tools.h"

#include "keen/base/string.hpp"
#include "keen/base/trace.hpp"

#include <wchar.h>

// Placement inferred: nothing else uses it.
enum liberation_states
{
    LIBERATION_STATE_INTRO = 0,
    LIBERATION_STATE_CONTROL = 1,
    LIBERATION_STATE_UNLOCKS = 2,
};

// ---- file-static globals
static const char* cDocname = "rfg_ui_district_liberated"; // 0x01660FA0
static const char* cLiberated = "MENU_LIBERATED"; // 0x01660FA8
static const char* cComplete = "MENU_MISSION_COMPLETE_TEXT"; // 0x01660FB4
static const char* cUnlocked = "MENU_UNLOCKED"; // 0x01660FB8
static const char* cBack = "MENU_CONTINUE"; // 0x01660FBC
static const char* cNew = "MENU_NEW"; // 0x01660FC0
static const char* cUnlocks = "MENU_UNLOCKS_NOUN"; // 0x01660FC4
static const char* cControl_tween = "meter_fill_scale_twn_1"; // 0x01660FC8
static const char* cControl_meter = "meter_fill"; // 0x01660FCC
static const char* kHintImg = "dl_hint_button"; // 0x01662220
static const char* kHintText = "dl_hint_text"; // 0x01662224
static bool Waiting_for_anim = true; // 0x01660EEB
static int Doc_handle_liberation = -1; // 0x01660FD0
// Magic number: 0x10, how these four handles start in rfg.exe's data and at each enter.
static int Data_header_handle = 0x10; // 0x01660FD4
static int Data_upgrade_handle = 0x10; // 0x01660FD8
static int Data_control_handle = 0x10; // 0x01660FDC
static int Data_hints_handle = 0x10; // 0x01660FE0
static int Input_id_liberation = -1; // 0x01660FE4
static unsigned int District_handle = 0xFFFFFFFF; // 0x01660FE8
static bool Tally_should_play = true; // 0x01660FF0
static int Unlocks_list[3]; // 0x02B953D0
static liberation_states Lib_state; // 0x02B955FC
static bool ExitLiberation; // 0x02B9DC9F
static bool Loaded; // 0x02B9DCA4
static bool Load_image_next_frame; // 0x02B9DCA5
static bool Push_state; // 0x02B9DCA6
static bool In_liberation_screen; // 0x02B9DCA7
static unsigned int Control_id_liberation; // 0x02B9DCA8
static liberation_ui_type Liberation_screen_type; // 0x02B9DCAC
static int Num_unlocks; // 0x02B9DCB0
static int Current_unlock; // 0x02B9DCB4
static int Num_guerrilla_actions_completed; // 0x02B9DCB8
static const char* Streamed_img; // 0x02B9DCBC

// The district of District_handle, or null if it is not one. rfg.exe inlines this test in its four users.
// Deviation from rfg.exe: this, hudl_leave and the huac_* helpers are separate functions, where it inlines
// them.
static object* hudl_district_object(unsigned int handle)
{
    if (handle == 0xFFFFFFFF)
    {
        return nullptr;
    }
    object* objp = World.object_from_handle(handle);
    return objp != nullptr && objp->get_obj_type() == OT_DISTRICT ? objp : nullptr;
}

// 0x00868740 (:120)
// Nothing calls this in rfg.exe.
void unused_dcf_show_district_liberation()
{
    if (!Push_state)
    {
        District_handle = 0xFFFFFFFF;
        Num_guerrilla_actions_completed = 0;
        Liberation_screen_type = LUT_MISSION_UNLOCKS;
        Num_unlocks = 3;
        // Magic numbers: three upgrades to show.
        Unlocks_list[0] = 1;
        Unlocks_list[1] = 4;
        Unlocks_list[2] = 3;
        Push_state = true;
        In_liberation_screen = true;
    }
}

// 0x008468A0 (:131)
void start_tally_sound()
{
    if (Tally_should_play)
    {
        audiolib_cue_play(Ga_cue_ids.district_liberation_countdown_loop, nullptr, nullptr);
        Tally_should_play = false;
    }
}

// 0x008468D0 (:142)
void stop_tally_sound()
{
}

// 0x008468E0 (:148)
int hud_ui_district_liberation_process_mouse_input()
{
    return ui_mouse_over_hint_combo(kHintImg, kHintText, Doc_handle_liberation, true) ? 1 : 0;
}

// The end of the screen. rfg.exe inlines this in two states of hud_ui_district_liberation_process_input.
static void hudl_leave()
{
    Waiting_for_anim = true;
    ui_unload_document(Doc_handle_liberation, DFOT_NONE, 0.0f);
    Doc_handle_liberation = -1;
    hud_ui_reload_items();
    ExitLiberation = true;
}

// 0x00846910 (:161)
void hud_ui_district_liberation_process_input()
{
    if (ui_get_current_input_id() != Input_id_liberation || ui_is_msgbox_active(-2)) // Magic number: -2, any message box.
    {
        return;
    }
    // Asked even while the animation runs, because it also highlights the hint.
    const bool mouse = hud_ui_district_liberation_process_mouse_input() != 0;
    // Magic number: 10, the document's accept button.
    if (Waiting_for_anim || !(ui_get_button_info(10, Input_id_liberation) > 0.0f || mouse))
    {
        return;
    }
    ui_menu_play_sound(MENU_SOUND_PRESS_SELECT);
    switch (Lib_state)
    {
    case LIBERATION_STATE_INTRO:
        // Original bug, kept: a screen of another type with no unlocks goes on to a page reading a stale Unlocks_list.
        Waiting_for_anim = true;
        Lib_state = Liberation_screen_type != LUT_LIBERATION ? LIBERATION_STATE_UNLOCKS : LIBERATION_STATE_CONTROL;
        break;
    case LIBERATION_STATE_CONTROL:
        Lib_state = LIBERATION_STATE_UNLOCKS;
        Waiting_for_anim = true;
        if (Num_unlocks == 0)
        {
            hudl_leave();
        }
        break;
    case LIBERATION_STATE_UNLOCKS:
        Current_unlock++;
        Waiting_for_anim = true;
        if (Num_unlocks <= Current_unlock)
        {
            hudl_leave();
        }
        break;
    default:
        keen::debug::nativePrintArgs("Int3 critical function at %s(%d)\n", __FILE__, __LINE__);
        break;
    }
}

// 0x00846A60 (:226)
void hud_ui_district_liberation_create_header_data()
{
    vint_data_item* item = vint_data_item::create_new("DistrictLiberated");
    if (item != nullptr)
    {
        static const vint_variant_type types[2] = {VINT_PROP_TYPE_STRING, VINT_PROP_TYPE_STRING};
        item->set_template(types, 2);
        Data_header_handle = (int)item->get_handle();
        ui_add_data_item(Doc_handle_liberation, Data_header_handle);
    }
}

// 0x00846AC0 (:247)
void hud_ui_district_liberation_create_unlock_data()
{
    vint_data_item* item = vint_data_item::create_new("DistrictUpgrade");
    if (item != nullptr)
    {
        static const vint_variant_type types[5] = {VINT_PROP_TYPE_STRING, VINT_PROP_TYPE_UINT, VINT_PROP_TYPE_STRING, VINT_PROP_TYPE_UINT, VINT_PROP_TYPE_STRING};
        item->set_template(types, 5);
        Data_upgrade_handle = (int)item->get_handle();
        ui_add_data_item(Doc_handle_liberation, Data_upgrade_handle);
    }
}

// 0x00846B40 (:271)
void hud_ui_district_liberation_create_control_data()
{
    vint_data_item* item = vint_data_item::create_new("DistrictControl");
    if (item != nullptr)
    {
        static const vint_variant_type types[3] = {VINT_PROP_TYPE_BOOL, VINT_PROP_TYPE_INT, VINT_PROP_TYPE_INT};
        item->set_template(types, 3);
        Data_control_handle = (int)item->get_handle();
        ui_add_data_item(Doc_handle_liberation, Data_control_handle);
    }
}

// 0x00846BC0 (:293)
void hud_ui_district_liberation_create_hints_data()
{
    vint_data_item* item = vint_data_item::create_new("DistrictHints");
    if (item != nullptr)
    {
        static const vint_variant_type types[2] = {VINT_PROP_TYPE_STRING, VINT_PROP_TYPE_STRING};
        item->set_template(types, 2);
        Data_hints_handle = (int)item->get_handle();
        ui_add_data_item(Doc_handle_liberation, Data_hints_handle);
    }
}

// 0x00846C20 (:314)
void hud_ui_district_liberation_create_data_items()
{
    hud_ui_district_liberation_create_header_data();
    hud_ui_district_liberation_create_hints_data();
    hud_ui_district_liberation_create_unlock_data();
    hud_ui_district_liberation_create_control_data();
}

// 0x00856860 (:323)
void hud_ui_district_liberation_process_header()
{
    vint_data_item* item = vint_data_item::find(Data_header_handle);
    if (item == nullptr)
    {
        return;
    }
    wchar_t title[128];
    char encoded[384];
    const char* subtitle = "";
    const object* objp = Liberation_screen_type == LUT_LIBERATION ? hudl_district_object(District_handle) : nullptr;
    if (objp != nullptr)
    {
        wcsncpy(title, localization_get_display_string_req(((const district*)objp)->get_localized_name_hash()), 0x80);
        title[127] = L'\0';
        subtitle = cLiberated;
    }
    else if (Liberation_screen_type == LUT_LIBERATION || Liberation_screen_type == LUT_MISSION_UNLOCKS)
    {
        wcsncpy(title, localization_get_display_string_opt(cNew), 0x80);
        title[127] = L'\0';
        subtitle = cUnlocks;
    }
    else if (Liberation_screen_type == LUT_ACTIVITY_UNLOCKS)
    {
        const wchar_t* actions;
        if (World.dlc_bundle_id == -1) // Magic number: -1, the base game.
        {
            actions = localization_get_display_string_req("MENU_GUERRILLA_ACTIONS", L"GUERRILLA ACTIONS");
        }
        else if (Num_guerrilla_actions_completed == 1)
        {
            actions = localization_get_display_string_req("MENU_DLC_ACTION", L"GUERRILLA ACTION");
        }
        else
        {
            actions = localization_get_display_string_req("MENU_DLC_ACTIONS", L"GUERRILLA ACTIONS");
        }
        _snprintf_safe(title, 0x80, "%d %ls", Num_guerrilla_actions_completed, actions);
        subtitle = cComplete;
    }
    else
    {
        // Deviation: rfg.exe would encode the uninitialized title here. No caller passes another type.
        title[0] = L'\0';
    }
    vint_encode_wide_text(encoded, 0x180, title);
    item->set_element(0, vint_variant(encoded));
    item->set_element(1, vint_variant(subtitle));
}

// 0x00846CD0 (:374)
void hud_ui_district_liberation_proccess_hints()
{
    vint_data_item* item = vint_data_item::find(Data_hints_handle);
    if (item == nullptr)
    {
        return;
    }
    if (Waiting_for_anim)
    {
        item->set_element(0, vint_variant(""));
        item->set_element(1, vint_variant(""));
        return;
    }
    char button[128];
    char image[128];
    controls_get_button_image_name(button, 0x80, CBA_MENU_SELECT, Player_controller_index);
    keen::formatString(image, 0x80, "[image:%s]", button);
    item->set_element(0, vint_variant(image));
    item->set_element(1, vint_variant(cBack));
}

// 0x00846DF0 (:398)
void hud_ui_district_liberation_process_unlock_data()
{
    vint_data_item* item = vint_data_item::find(Data_upgrade_handle);
    if (item == nullptr)
    {
        return;
    }
    const char* icon = nullptr;
    const char* blueprint = nullptr;
    unsigned int name_crc = 0;
    unsigned int description_crc = 0;
    if (Lib_state == LIBERATION_STATE_UNLOCKS && !ExitLiberation)
    {
        player_meta_data* meta = player_get_meta_data();
        if (meta != nullptr)
        {
            const upgrade_level* level = meta->get_next_upgrade_level((unsigned int)Unlocks_list[Current_unlock]);
            const upgrade_definition_data* upgrade = upgrade_get_data_by_index((unsigned int)Unlocks_list[Current_unlock]);
            if (level != nullptr && upgrade != nullptr)
            {
                icon = upgrade->get_icon_name();
                blueprint = upgrade->get_blueprint_name();
                name_crc = level->name_crc;
                // Magic numbers: -1, the base game; 1, the DLC whose upgrades have a description of their own.
                if (dlc_get_current_id() == -1)
                {
                    description_crc = localization_get_id_hash("TOOLTIP_UNLOCK_UPGRADES_DESC");
                }
                else if (dlc_get_current_id() == 1)
                {
                    description_crc = level->description_crc;
                }
                if (blueprint != nullptr && ui_get_image_status(blueprint) != ST_LOADED)
                {
                    blueprint = nullptr;
                }
            }
        }
    }
    item->set_element(0, vint_variant(icon));
    item->set_element(1, vint_variant(name_crc));
    item->set_element(2, vint_variant(cUnlocked));
    item->set_element(3, vint_variant(description_crc));
    item->set_element(4, vint_variant(blueprint));
}

// 0x00856AA0 (:442)
void hud_ui_district_liberation_process_control_data()
{
    vint_data_item* item = vint_data_item::find(Data_control_handle);
    if (item == nullptr)
    {
        return;
    }
    bool show_control = false;
    float control = 0.0f;
    int missions = 0;
    if (Liberation_screen_type != LUT_LIBERATION)
    {
        Waiting_for_anim = false;
    }
    else if (Lib_state == LIBERATION_STATE_CONTROL)
    {
        object* objp = hudl_district_object(District_handle);
        if (objp == nullptr)
        {
            keen::debug::nativePrintArgs("Int3 critical function at %s(%d)\n", __FILE__, __LINE__);
        }
        else
        {
            const district* dp = (const district*)objp;
            vint_document* doc = ui_get_vint_document(Doc_handle_liberation);
            vint_object_base* tween = vint_object_base::find_object(cControl_tween, doc);
            vint_object_base* meter = vint_object_base::find_object(cControl_meter, doc);
            if (tween != nullptr && meter != nullptr)
            {
                vint_variant start_value;
                vint_variant end_value;
                vint_variant scale_value;
                tween->get_property_value("start_value", start_value, 0);
                tween->get_property_value("end_value", end_value, 0);
                meter->get_property_value("scale", scale_value, 0);
                const float control_max = gfm_get_edf_control_max(dp);
                // The formula is rfg.exe's, in doubles as there. Only the y components are read.
                const float start_y = start_value.get_type() == VINT_PROP_TYPE_VECTOR2F ? ((vector2)start_value).y : 0.0f;
                const float end_y = end_value.get_type() == VINT_PROP_TYPE_VECTOR2F ? ((vector2)end_value).y : 0.0f;
                const float scale_y = scale_value.get_type() == VINT_PROP_TYPE_VECTOR2F ? ((vector2)scale_value).y : 0.0f;
                const float range = (float)((double)start_y - (double)end_y);
                control = (float)(((double)scale_y / (double)range + 1.0) * (double)control_max);
            }
            missions = missions_in_district(District_handle);
            start_tally_sound();
            show_control = true;
        }
    }
    item->set_element(0, vint_variant(show_control));
    item->set_element(1, vint_variant((int)control));
    item->set_element(2, vint_variant(missions));
}

// 0x00856D10 (:493)
void hud_ui_district_liberation_process_info()
{
    hud_ui_district_liberation_process_header();
    hud_ui_district_liberation_proccess_hints();
    hud_ui_district_liberation_process_unlock_data();
    hud_ui_district_liberation_process_control_data();
}

// 0x00876FA0 (:508)
void hud_ui_district_liberation_init()
{
    state_info info;
    info.enter = hud_ui_district_liberation_enter;
    info.exit = hud_ui_district_liberation_exit;
    info.process = hud_ui_district_liberation_process;
    info.handshake = hud_ui_district_liberation_handshake;
    info.render = hud_ui_district_liberation_render;
    info.transparent = true;
    info.pause_beneath = true;
    gameseq_register_state(GS_INGAME_DISTRICT_LIVERATION, info);
}

// 0x00846FB0 (:523)
void hud_ui_district_liberation_enter(game_state prev_state)
{
    (void)prev_state;
    ExitLiberation = false;
    Loaded = false;
    Tally_should_play = true;
    Lib_state = LIBERATION_STATE_INTRO;
    Waiting_for_anim = true;
    Current_unlock = 0;
    Input_id_liberation = -1;
    Doc_handle_liberation = -1;
    Data_header_handle = 0x10;
    Data_upgrade_handle = 0x10;
    Data_control_handle = 0x10;
    Data_hints_handle = 0x10;
    ui_set_screen_blur(true, true);
    Doc_handle_liberation = ui_load_document(cDocname, DFIT_ALPHA, 0.5f, DLT_NORMAL, true, STREAM_PRIORITY_AUDIO_CACHE_LOAD, false);
    if (Doc_handle_liberation == -1)
    {
        keen::debug::nativePrintArgs("Int3 critical function at %s(%d)\n", __FILE__, __LINE__);
        ExitLiberation = true;
        return;
    }
    gamemusic_event_occurred(GAMEMUSIC_DISTRICT_LIBERATED);
    hud_ui_unload_items();
    Control_id_liberation = ui_request_control_id();
    ui_wait_for_button_ups();
}

// 0x008687B0 (:568)
void hud_ui_district_liberation_exit(game_state new_state)
{
    if (Streamed_img != nullptr)
    {
        ui_release_image(Streamed_img);
        Streamed_img = nullptr;
    }
    if (Doc_handle_liberation != -1)
    {
        ui_unload_document(Doc_handle_liberation, DFOT_NONE, 0.0f);
    }
    ui_relinquish_control_id(Control_id_liberation);
    ui_relinquish_input_id(Input_id_liberation);
    ui_set_screen_blur(false, true);
    In_liberation_screen = false;
    const object* objp = hudl_district_object(District_handle);
    if (objp != nullptr && new_state != GS_MAINMENU && v_stricmp(objp->get_name(), "Parker") == 0)
    {
        hud_ui_mission_briefing_start("Parker Liberation", true);
    }
    upgrade_unlock_free_dlc_upgrades();
}

// 0x00847090 (:602)
// The blueprint image of the current unlock. A new image is requested the frame after the old one is released.
void hud_ui_district_liberation_handshake(rl_renderer*)
{
    if (!Loaded)
    {
        return;
    }
    const char* image = nullptr;
    if (Current_unlock < Num_unlocks)
    {
        const upgrade_definition_data* upgrade = upgrade_get_data_by_index((unsigned int)Unlocks_list[Current_unlock]);
        if (upgrade != nullptr)
        {
            image = upgrade->get_blueprint_name();
        }
    }
    if (Load_image_next_frame)
    {
        if (image != nullptr)
        {
            ui_request_image(image);
        }
        Streamed_img = image;
        Load_image_next_frame = false;
        return;
    }
    if (image != Streamed_img)
    {
        if (Streamed_img != nullptr)
        {
            ui_release_image(Streamed_img);
            Streamed_img = nullptr;
        }
        if (image != nullptr)
        {
            Load_image_next_frame = true;
        }
    }
}

// 0x00856D30 (:642)
void hud_ui_district_liberation_process()
{
    const bool msgbox_active = ui_is_msgbox_active(-2); // Magic number: -2, any message box.
    if (msgbox_active)
    {
        ui_process(Control_id_liberation, true);
        return;
    }
    if (ExitLiberation)
    {
        // Liberating Dust warps the player to its safehouse. Magic number: 0xC800, that safehouse's grid id.
        // ExitLiberation stays set and ui_process is skipped, so this repeats each frame until the warp's reload.
        const object* objp = hudl_district_object(District_handle);
        if (objp != nullptr && v_stricmp(objp->get_name(), "Dust") == 0)
        {
            const object_safehouse* safehouse = safehouse_find_safehouse_by_grid_id(0xC800);
            if (safehouse != nullptr)
            {
                vector pos;
                matrix orient;
                safehouse_get_start_pos_orient(pos, orient, safehouse);
                World.save_and_warp(pos, orient);
                return;
            }
        }
        if (hud_ui_is_loaded())
        {
            gameseq_pop_state();
            ExitLiberation = false;
        }
        ui_process(Control_id_liberation, false);
        return;
    }
    if (!Loaded)
    {
        switch (ui_get_doc_asset_state(Doc_handle_liberation))
        {
        case ST_UNLOADED:
            keen::debug::nativePrintArgs("Int3 critical function at %s(%d)\n", __FILE__, __LINE__);
            // fall through
        case ST_ERROR:
            ui_unload_document(Doc_handle_liberation, DFOT_NONE, 0.0f);
            Doc_handle_liberation = -1;
            gameseq_pop_state();
            break;
        case ST_LOADING:
            break;
        case ST_LOADED:
            Loaded = true;
            break;
        default:
            keen::debug::nativePrintArgs("Int3 critical function at %s(%d)\n", __FILE__, __LINE__);
            break;
        }
        if (!Loaded)
        {
            ui_process(Control_id_liberation, false);
            return;
        }
        Input_id_liberation = ui_register_for_processed_input(IRT_UNLIMITED_INPUT, false);
        hud_ui_district_liberation_create_data_items();
        hud_ui_district_liberation_process_info();
        audiolib_params params;
        params.delay_ms = 333; // Magic number: 333 ms.
        audiolib_cue_play(Liberation_screen_type == LUT_LIBERATION ? Ga_cue_ids.district_liberated : Ga_cue_ids.unlock_popup, &params, nullptr);
    }
    hud_ui_district_liberation_process_input();
    hud_ui_district_liberation_process_info();
    ui_add_for_processing(Doc_handle_liberation);
    ui_process(Control_id_liberation, false);
}

// 0x00847120 (:729)
void hud_ui_district_liberation_render(rl_renderer*)
{
    if (Loaded && gameseq_get_state() == GS_INGAME_DISTRICT_LIVERATION)
    {
        ui_add_for_rendering(Doc_handle_liberation);
    }
    ui_render(Control_id_liberation, false);
}

// 0x00847160 (:739)
void hud_ui_district_liberation_do_frame()
{
    if (Push_state && !game_is_paused() && gameseq_get_pending_state() == GS_NONE)
    {
        gameseq_push_state(GS_INGAME_DISTRICT_LIVERATION, true, false);
        Push_state = false;
    }
}

// 0x00856F80 (:749)
bool hud_ui_district_liberation_show(const int* update_idx_list, int num_idx, unsigned int district_handle, liberation_ui_type ui_type, int num_activities_completed)
{
    if (Push_state)
    {
        return false;
    }
    District_handle = district_handle;
    Liberation_screen_type = ui_type;
    Num_guerrilla_actions_completed = num_activities_completed;
    if (ui_type == LUT_LIBERATION && hudl_district_object(district_handle) == nullptr)
    {
        Liberation_screen_type = LUT_MISSION_UNLOCKS;
    }
    Num_unlocks = num_idx > 3 ? 3 : num_idx; // Magic number: 3, the size of Unlocks_list.
    for (int i = 0; i < Num_unlocks; i++)
    {
        Unlocks_list[i] = update_idx_list[i];
    }
    Push_state = true;
    In_liberation_screen = true;
    return true;
}

// 0x008471A0 (:788)
void hud_ui_district_liberation_level_shutdown()
{
    // Original bug, kept: In_liberation_screen stays set if a pending screen is dropped here, and only this screen's
    // exit clears it, so hud_ui_district_liberation_screen_active answers true until another screen has run.
    Push_state = false;
}

// 0x008471B0 (:793)
void hud_ui_district_liberation_tween_done()
{
    Waiting_for_anim = false;
}

// 0x008471C0 (:800)
// Nothing calls this in rfg.exe.
void hud_ui_district_liberation_main_text_done()
{
    audiolib_cue_play(Liberation_screen_type == LUT_LIBERATION ? Ga_cue_ids.district_liberated : Ga_cue_ids.unlock_popup, nullptr, nullptr);
}

// 0x008471F0 (:810)
bool hud_ui_district_liberation_screen_active()
{
    return In_liberation_screen;
}
