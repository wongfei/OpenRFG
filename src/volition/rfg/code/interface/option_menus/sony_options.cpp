// Original: volition/rfg/code/interface/option_menus/sony_options.cpp
//
// The waiting screen around the platform's own login, save and friend dialogs (rfg_ui_menu_sony). On this build
// sony_options_enter asks to leave as soon as the state starts (see there).

#include "volition/rfg/code/interface/option_menus/sony_options.h"

#include "volition/rfg/code/gamewide/game_options.h"
#include "volition/rfg/code/gamewide/game_save.h"
#include "volition/rfg/code/gamewide/gamestate_gameplay.h"
#include "volition/rfg/code/interface/loading.h"
#include "volition/rfg/code/interface/main_menu.h"
#include "volition/rfg/code/interface/option_menus/ingame_options.h"
#include "volition/rfg/code/interface/option_menus/player_death_options.h"
#include "volition/rfg/code/interface/ui_document_manager.h"
#include "volition/rfg/code/interface/ui_menu.h"
#include "volition/rfg/code/level/world.h"
#include "volition/rfg/code/multi/multi_core/include/multi_core_platform.h"

#include "keen/base/trace.hpp"

// ---- file-static globals
extern int Doc_handle_main; // 0x016631AC, ui_menu.cpp's
static int Doc_handle_soptions = -1; // 0x01664298
static int Control_id_soptions = -1; // 0x0166429C
static int Input_id_soptions = -1; // 0x016642A0
static const char* cDocname_soptions = "rfg_ui_menu_sony"; // 0x016642A4
static sony_options_mode Sony_options_mode; // 0x02C00E6C
static bool Ready_to_exit; // 0x02C07FDC
static bool Assets_loaded_soptions; // 0x02C07FDD
static bool Sony_options_invite_ui_active; // 0x02C07FDE

// 0x00918CE0 (:99)
void sony_options_init()
{
    state_info info;
    info.enter = sony_options_enter;
    info.exit = sony_options_exit;
    info.process = sony_options_process;
    info.handshake = sony_options_handshake;
    info.render = sony_options_render;
    info.transparent = true;
    info.pause_beneath = true;
    gameseq_register_state(GS_SONY_OPTIONS, info);
}

// 0x0090FDF0 (:113)
void sony_options_enter(game_state prev_state)
{
    (void)prev_state;
    Assets_loaded_soptions = false;
    Doc_handle_soptions = ui_load_document(cDocname_soptions, DFIT_ALPHA, 0.5f, DLT_NORMAL, false, STREAM_PRIORITY_AUDIO_CACHE_LOAD, false);
    if (Doc_handle_soptions == -1)
    {
        keen::debug::nativePrintArgs("Int3 critical function at %s(%d)\n", __FILE__, __LINE__);
        gameseq_pop_state();
        return;
    }
    // Kept from the asm: the Int3 and the pop run on every entry, and the entry goes on past them.
    keen::debug::nativePrintArgs("Int3 critical function at %s(%d)\n", __FILE__, __LINE__);
    if (gameseq_get_state() == GS_SONY_OPTIONS)
    {
        gameseq_pop_state();
    }
    Control_id_soptions = (int)ui_request_control_id();
    Input_id_soptions = ui_register_for_processed_input(IRT_UNLIMITED_INPUT, false);
    ui_wait_for_button_ups();
    ui_menu_set_input_enabled(false);
    Ready_to_exit = false;
    Sony_options_invite_ui_active = false;
}

// 0x009165F0 (:155)
void sony_options_process()
{
    if (!Assets_loaded_soptions)
    {
        switch (ui_get_doc_asset_state(Doc_handle_soptions))
        {
        case ST_UNLOADED:
            keen::debug::nativePrintArgs("Int3 critical function at %s(%d)\n", __FILE__, __LINE__);
            [[fallthrough]];
        case ST_ERROR:
            ui_unload_document(Doc_handle_soptions, DFOT_NONE, 0.5f);
            Doc_handle_soptions = -1;
            if (gameseq_get_state() == GS_SONY_OPTIONS)
            {
                gameseq_pop_state();
            }
            ui_process((unsigned int)Control_id_soptions, false);
            return;
        case ST_LOADING:
            break;
        case ST_LOADED:
            Assets_loaded_soptions = true;
            break;
        default:
            keen::debug::nativePrintArgs("Int3 critical function at %s(%d)\n", __FILE__, __LINE__);
            break;
        }
        if (game_options_save_in_progress())
        {
            Assets_loaded_soptions = false;
        }
        if (!Assets_loaded_soptions)
        {
            ui_process((unsigned int)Control_id_soptions, false);
            return;
        }
        if (Sony_options_mode == SONY_OPTIONS_LOGIN)
        {
            multi_core_platform_show_login_ui(false);
        }
        else if (Sony_options_mode == SONY_OPTIONS_SAVE)
        {
            World.save_and_continue(nullptr, false, false);
        }
        else if (Sony_options_mode != SONY_OPTIONS_LOAD)
        {
            keen::debug::nativePrintArgs("Int3 critical function at %s(%d)\n", __FILE__, __LINE__);
        }
    }
    switch (Sony_options_mode)
    {
    case SONY_OPTIONS_LOGIN:
    case SONY_OPTIONS_LOAD:
    case SONY_OPTIONS_FRIENDS_UI:
    case SONY_OPTIONS_ADD_FRIEND_UI:
    case SONY_OPTIONS_APP_DEFINED_MSG:
    case SONY_OPTIONS_READ_MESSAGES:
        if (Sony_options_invite_ui_active)
        {
            Ready_to_exit = false;
        }
        else if (Ready_to_exit)
        {
            gameseq_pop_state();
        }
        break;
    default:
        if (game_save_op_completed() && gameseq_get_state() == GS_SONY_OPTIONS)
        {
            gameseq_pop_state();
        }
        break;
    }
    ui_add_for_processing(Doc_handle_soptions);
    ui_process((unsigned int)Control_id_soptions, false);
}

// 0x008930D0 (:277)
void sony_options_handshake(rl_renderer*)
{
    process_save_load();
}

// 0x00904670 (:283)
void sony_options_render(rl_renderer*)
{
    ui_add_for_rendering(Doc_handle_soptions);
    ui_render((unsigned int)Control_id_soptions, false);
}

// 0x008F4280 (:289)
void sony_options_exit(game_state new_state)
{
    ui_relinquish_input_id(Input_id_soptions);
    ui_unload_document(Doc_handle_soptions, DFOT_NONE, 0.5f);
    ui_relinquish_control_id((unsigned int)Control_id_soptions);
    Control_id_soptions = -1;
    Doc_handle_soptions = -1;
    ui_menu_set_input_enabled(true);
    if (gameseq_get_state() != GS_MAINMENU_IDLE && Doc_handle_main != -1)
    {
        main_menu_show_menu(true);
    }
    else if (new_state == GS_PLAYER_DEATH_OPTIONS)
    {
        player_death_options_show_menu(true);
    }
    else if (gameseq_get_previous_state() == GS_INGAME_OPTIONS)
    {
        ingame_options_show_menu(true);
    }
}

// 0x008930E0 (:313)
void sony_options_singal_exit()
{
    Ready_to_exit = true;
}

// 0x008930F0 (:318)
void sony_options_signal_sending_invite(bool value)
{
    Sony_options_invite_ui_active = value;
}

// 0x00893100 (:323)
void sony_options_start(sony_options_mode mode)
{
    Sony_options_mode = mode;
    gameseq_push_state(GS_SONY_OPTIONS, true, false);
}

// 0x00893120 (:329)
void sony_options_add_friend(const multi_player_uid&)
{
}
