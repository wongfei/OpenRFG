// Original: volition/rfg/code/interface/option_menus/options_audio.cpp
//
// The audio options (GS_MAINMENU_OPTIONS_AUDIO, UI.md UI6) on rfg_ui_options_template: sound on/off, the ambience,
// music, voice, voice chat, effects and overall volumes (each change heard at once through its slider cue), push to
// talk. Accept saves the game options (the menu leaves once the profile save is done), back asks before dropping
// changes, the third hint resets the defaults. The volumes go to audio_lib's category scalars.

#include "volition/rfg/code/interface/option_menus/option_menu_system.h"

#include "keen/base/trace.hpp"
#include "volition/audio_lib/code/audiolib_api.h"
#include "volition/audio_lib/code/audiolib_internal.h"
#include "volition/interface_lib/code/vint_document.h"
#include "volition/rfg/code/control/control_map.h"
#include "volition/rfg/code/gamewide/game_options.h"
#include "volition/rfg/code/gamewide/gameaudio.h"
#include "volition/rfg/code/gamewide/gamemusic.h"
#include "volition/rfg/code/gamewide/player_profile.h"
#include "volition/rfg/code/interface/localization/localization_shared.h"
#include "volition/rfg/code/interface/loading.h"
#include "volition/rfg/code/interface/main_menu.h"
#include "volition/rfg/code/interface/option_menus/ingame_options.h"
#include "volition/rfg/code/interface/ui_document_manager.h"
#include "volition/rfg/code/interface/ui_menu.h"
#include "volition/rfg/code/interface/ui_message_box.h"
#include "volition/rfg/code/multi/multi_core/include/multi.h"
#include "volition/rfg/code/multi/multi_core/include/multi_voice.h"
#include "volition/vlib/code/input/mouse.h"
#include "volition/vlib/code/math/floating.h"
#include "volition/vlib/code/time/timestamp.h"

// ---- file-private types (rfg.pdb)
// size 0x48
struct audio_info_options
{
    int m_ambience_orig; // 0x00
    int m_music_orig; // 0x04
    int m_voice_orig; // 0x08
    int m_voicechat_orig; // 0x0C
    int m_sfx_orig; // 0x10
    int m_overall_orig; // 0x14
    int m_ambience; // 0x18
    int m_music; // 0x1C
    int m_voice; // 0x20
    int m_voicechat; // 0x24
    int m_sfx; // 0x28
    int m_overall; // 0x2C
    int m_ambience_playid; // 0x30
    int m_music_playid; // 0x34
    int m_voice_playid; // 0x38
    int m_voicechat_playid; // 0x3C
    int m_sfx_playid; // 0x40
    int m_overall_playid; // 0x44
};
static_assert(sizeof(audio_info_options) == 0x48, "audio_info_options");

// audio_lib's volume categories as the options use them (magic numbers of rfg.exe): 0 ambience, 1 music, 2 and 3
// voice, 4 and 5 effects, 6 overall, 7 voice chat.
enum
{
    AUDIO_OPTIONS_CATEGORY_AMBIENCE = 0,
    AUDIO_OPTIONS_CATEGORY_MUSIC = 1,
    AUDIO_OPTIONS_CATEGORY_VOICE = 2,
    AUDIO_OPTIONS_CATEGORY_VOICE_2 = 3,
    AUDIO_OPTIONS_CATEGORY_SFX = 4,
    AUDIO_OPTIONS_CATEGORY_SFX_2 = 5,
    AUDIO_OPTIONS_CATEGORY_OVERALL = 6,
    AUDIO_OPTIONS_CATEGORY_VOICECHAT = 7,
};

// ---- file-static globals
// bool s_localization_is_loading (0x01663023) is used here too; placed in localization_shared.h.
// The header's option row object names (shared statics of the interface unity; this file's copies).
static const char* kOptionRowGroup = {"row_group"}; // 0x016630B4
static const char* kOptionHighlightArrows = {"row_highlight_arrows"}; // 0x016630B8
static const char* kOptionLeftArrow = {"left_arrow"}; // 0x016630BC
static const char* kOptionRowHighlightBG = {"row_highlight_bg"}; // 0x016630C4
// bool Display_character (0x0166316F) is used here too; placed in rfg_lua_actions.cpp.
extern bool Display_character; // 0x0166316F, rfg_lua_actions.cpp
static bool Sound_enable = true; // 0x0166411D
static const char* cPushtotalk_list[2] = {"MENU_ON", "MENU_OFF"}; // 0x0166412C
static const char* cDoc_name_aoptions = {"rfg_ui_options_template"}; // 0x01664134
static const char* Sound_enabled_list[2] = {"MENU_ENABLED", "MENU_DISABLED"}; // 0x01664138
static int Doc_handle_aoptions = -1; // 0x01664140
static int Control_id_aoptions = -1; // 0x01664144
static int Input_id_aoptions = -1; // 0x01664148
static int Menu_handle_aoptions = -1; // 0x0166414C
static int Message_box_handle_aoptions = -1; // 0x01664150
// int Utility_doc_handle (0x01664FF8) is used here too; placed in loading.cpp.
// The profile save's time limit (UI0 missed this static; address from the asm).
static timestamp Profile_save_timeout; // 0x01666D18
// farray<unsigned int,16> Controller_slots (0x01667378) is used here too; placed in loading.cpp.
// farray<input_request_data,16> Input_types (0x016673C8) is used here too; placed in loading.cpp.
// farray<ui_menu*,8> Menus (0x01667698) is used here too; placed in ui_menu.cpp.
static audio_info_options my_audio_levels; // 0x02C03320
// bool Active (0x02C03754) is used here too; placed in ui_document_hud_tool_tips.cpp.
extern bool Active; // 0x02C03754, ui_document_hud_tool_tips.cpp (the in-game options are up)
static bool Assets_loaded_aoptions; // 0x02C03756
static bool Change_made_aoptions; // 0x02C03757
static bool Waiting_on_save_completion_aoptions; // 0x02C0375C
// msgbox_info_set Message_box_info_set (0x02CA0340) is used here too; placed in ui_message_box.cpp.

// ---- internal functions

// A category's volume in percent.
static int audio_get_level(signed char category)
{
    float volume;
    audiolib_system_get_volume_scalar(category, &volume);
    // Magic number: 100, percent.
    return fl_round(volume * 100.0f);
}

// A category's volume from percent.
static void audio_set_level(signed char category, int level)
{
    // Magic number: 0.01, from percent.
    audiolib_system_set_volume_scalar(category, (float)level * 0.01f);
}

// 0x00891100 (:178)
// The current volumes become the levels and their originals; no slider cue playing.
static void audio_init_levels()
{
    Sound_enable = audiolib_get_sound_enabled();
    my_audio_levels.m_ambience = audio_get_level(AUDIO_OPTIONS_CATEGORY_AMBIENCE);
    my_audio_levels.m_music = audio_get_level(AUDIO_OPTIONS_CATEGORY_MUSIC);
    my_audio_levels.m_voice = audio_get_level(AUDIO_OPTIONS_CATEGORY_VOICE);
    my_audio_levels.m_voicechat = audio_get_level(AUDIO_OPTIONS_CATEGORY_VOICECHAT);
    // Original: category 4 is read and dropped (the effects level is category 5's).
    audio_get_level(AUDIO_OPTIONS_CATEGORY_SFX);
    my_audio_levels.m_sfx = audio_get_level(AUDIO_OPTIONS_CATEGORY_SFX_2);
    my_audio_levels.m_overall = audio_get_level(AUDIO_OPTIONS_CATEGORY_OVERALL);
    my_audio_levels.m_ambience_orig = my_audio_levels.m_ambience;
    my_audio_levels.m_music_orig = my_audio_levels.m_music;
    my_audio_levels.m_voice_orig = my_audio_levels.m_voice;
    my_audio_levels.m_overall_orig = my_audio_levels.m_overall;
    my_audio_levels.m_voicechat_orig = my_audio_levels.m_voicechat;
    my_audio_levels.m_sfx_orig = my_audio_levels.m_sfx;
    my_audio_levels.m_ambience_playid = -1;
    my_audio_levels.m_music_playid = -1;
    my_audio_levels.m_voice_playid = -1;
    my_audio_levels.m_voicechat_playid = -1;
    my_audio_levels.m_sfx_playid = -1;
    my_audio_levels.m_overall_playid = -1;
}

// 0x008912B0 (:214)
static void audio_set_sound_levels()
{
    audio_set_level(AUDIO_OPTIONS_CATEGORY_AMBIENCE, my_audio_levels.m_ambience);
    audio_set_level(AUDIO_OPTIONS_CATEGORY_MUSIC, my_audio_levels.m_music);
    audio_set_level(AUDIO_OPTIONS_CATEGORY_VOICE, my_audio_levels.m_voice);
    audio_set_level(AUDIO_OPTIONS_CATEGORY_VOICE_2, my_audio_levels.m_voice);
    audio_set_level(AUDIO_OPTIONS_CATEGORY_VOICECHAT, my_audio_levels.m_voicechat);
    audio_set_level(AUDIO_OPTIONS_CATEGORY_SFX, my_audio_levels.m_sfx);
    audio_set_level(AUDIO_OPTIONS_CATEGORY_SFX_2, my_audio_levels.m_sfx);
    audio_set_level(AUDIO_OPTIONS_CATEGORY_OVERALL, my_audio_levels.m_overall);
}

// 0x00891410 (:229)
static void audio_revert_sound_levels()
{
    my_audio_levels.m_ambience = my_audio_levels.m_ambience_orig;
    my_audio_levels.m_music = my_audio_levels.m_music_orig;
    my_audio_levels.m_voice = my_audio_levels.m_voice_orig;
    my_audio_levels.m_voicechat = my_audio_levels.m_voicechat_orig;
    my_audio_levels.m_sfx = my_audio_levels.m_sfx_orig;
    my_audio_levels.m_overall = my_audio_levels.m_overall_orig;
    audio_set_sound_levels();
}

// A slider changed: its categories set, the previous cue faded out (0.5 s), the slider's cue played.
static void audio_slider_changed(signed char category, signed char category_2, int level, int& play_id, int cue_id)
{
    audio_set_level(category, level);
    if (category_2 >= 0)
    {
        audio_set_level(category_2, level);
    }
    // Magic number: a 500 ms fade out.
    audiolib_play_fade_out(play_id, 500);
    play_id = audiolib_cue_play(cue_id, nullptr, nullptr);
}

// 0x008914C0 (:291)
static void option_change_occurred_aoptions(int choice, void* new_value)
{
    switch (choice)
    {
    case 0:
        if (new_value == (void*)0)
        {
            Sound_enable = true;
        }
        else if (new_value == (void*)1)
        {
            Sound_enable = false;
        }
        else
        {
            keen::debug::nativePrintArgs("Int3 critical function at %s(%d)\n", __FILE__, __LINE__);
        }
        audiolib_set_sound_enabled(Sound_enable);
        if (Sound_enable)
        {
            gamemusic_update(true);
        }
        break;
    case 1:
        audio_slider_changed(AUDIO_OPTIONS_CATEGORY_AMBIENCE, -1, my_audio_levels.m_ambience, my_audio_levels.m_ambience_playid, Ga_cue_ids.ui_ambience_slider_cue_id);
        break;
    case 2:
        audio_slider_changed(AUDIO_OPTIONS_CATEGORY_MUSIC, -1, my_audio_levels.m_music, my_audio_levels.m_music_playid, Ga_cue_ids.ui_music_slider_cue_id);
        break;
    case 3:
        audio_slider_changed(AUDIO_OPTIONS_CATEGORY_VOICE, AUDIO_OPTIONS_CATEGORY_VOICE_2, my_audio_levels.m_voice, my_audio_levels.m_voice_playid, Ga_cue_ids.ui_voice_slider_cue_id);
        break;
    case 4:
        audio_slider_changed(AUDIO_OPTIONS_CATEGORY_VOICECHAT, -1, my_audio_levels.m_voicechat, my_audio_levels.m_voicechat_playid, Ga_cue_ids.ui_voicechat_slider_cue_id);
        break;
    case 5:
        audio_slider_changed(AUDIO_OPTIONS_CATEGORY_SFX, AUDIO_OPTIONS_CATEGORY_SFX_2, my_audio_levels.m_sfx, my_audio_levels.m_sfx_playid, Ga_cue_ids.ui_sfx_slider_cue_id);
        break;
    case 6:
        audio_slider_changed(AUDIO_OPTIONS_CATEGORY_OVERALL, -1, my_audio_levels.m_overall, my_audio_levels.m_overall_playid, Ga_cue_ids.ui_overall_slider_cue_id);
        break;
    case 7:
        if (new_value == (void*)0)
        {
            multi_voice_push_to_talk_set_enabled(true);
        }
        else if (new_value == (void*)1)
        {
            multi_voice_push_to_talk_set_enabled(false);
        }
        break;
    default:
        return;
    }
    Change_made_aoptions = true;
}

// 0x008C3F30 (:433)
static bool audio_menu_default_controls_cb(int mbox_handle, int selection_index, msgbox_choice choice)
{
    if (Message_box_handle_aoptions != mbox_handle)
    {
        return false;
    }
    if (choice == MBC_OK)
    {
        audio_reset_defaults();
        Change_made_aoptions = true;
    }
    else if ((int)choice < 1 || (int)choice > 2)
    {
        keen::debug::nativePrintArgs("Int3 critical function at %s(%d)\n", __FILE__, __LINE__);
    }
    Message_box_handle_aoptions = -1;
    return true;
}

// 0x00891890 (:459)
// Changes dropped: the volumes back, the options reread, the menu leaves.
static bool audio_menu_cancel_message_cb(int mbox_handle, int selection_index, msgbox_choice choice)
{
    if (Message_box_handle_aoptions != mbox_handle)
    {
        return false;
    }
    if (choice == MBC_OK)
    {
        audio_revert_sound_levels();
        game_options_load_from_cache();
        Change_made_aoptions = false;
        if (gameseq_get_state() == GS_MAINMENU_OPTIONS_AUDIO)
        {
            gameseq_pop_state();
        }
    }
    else if ((int)choice < 1 || (int)choice > 2)
    {
        keen::debug::nativePrintArgs("Int3 critical function at %s(%d)\n", __FILE__, __LINE__);
    }
    Message_box_handle_aoptions = -1;
    return true;
}

// 0x009014C0 (:506)
// Accept: saved (when changed or the last profile save failed), the menu leaves once the save is done (25 s at most);
// back: a box if changed; misc: the defaults box (or a box saying they are already set).
static void audio_hint_selection(hint_type hint)
{
    if (hint == HT_SELECTION)
    {
        if (game_options_are_changed() || !player_profile_last_save_successful(Player_controller_index != 0xFFFFFFFFu ? (int)Player_controller_index : 0))
        {
            audio_set_sound_levels();
            game_options_save();
            Change_made_aoptions = false;
            Waiting_on_save_completion_aoptions = true;
            // Magic number: 25 s for the profile save.
            Profile_save_timeout.set(25000);
            return;
        }
    }
    else if (hint == HT_ABORT)
    {
        if (game_options_are_changed())
        {
            Message_box_handle_aoptions = ui_add_msgbox(MBT_ACCEPT_CANCEL, "MENU_WARNING", "MENU_LOSE_CHANGES_CONFIRM", audio_menu_cancel_message_cb, false, false, nullptr, nullptr, false);
            return;
        }
    }
    else if (hint == HT_MISC)
    {
        if (audio_are_changes_different_from_defaults())
        {
            Message_box_handle_aoptions = ui_add_msgbox(MBT_ACCEPT_CANCEL, "MENU_WARNING", "MENU_RESET_DEFAULT_CONFIRM", audio_menu_default_controls_cb, false, false, nullptr, nullptr, false);
            return;
        }
        ui_add_msgbox(MBT_OK, "MENU_WARNING", "MENU_DEFAULTS_NOT_CHANGED", nullptr, false, false, nullptr, nullptr, false);
        return;
    }
    else
    {
        keen::debug::nativePrintArgs("Int3 critical function at %s(%d)\n", __FILE__, __LINE__);
        return;
    }
    if (gameseq_get_state() == GS_MAINMENU_OPTIONS_AUDIO)
    {
        gameseq_pop_state();
    }
}

// 0x009015D0 (:655)
// The default hints, then the rows; a released click selects (on the arrows: left or right).
static void audio_menu_process_mouse_input()
{
    options_default_hint_objects hints;
    options_menu_get_default_hint_elements(Doc_handle_aoptions, hints);
    const int hint = options_menu_process_mouse_on_default_hints(hints);
    if (hint > -1)
    {
        audio_hint_selection((hint_type)hint);
        return;
    }
    const int index = ui_menu_process_mouseover(Doc_handle_aoptions, Menu_handle_aoptions, kOptionRowGroup);
    if (index < 0 || !mouse_just_released(0))
    {
        return;
    }
    vint_doc_info* info = ui_get_doc_info(Doc_handle_aoptions);
    if (info == nullptr || info->m_doc == nullptr)
    {
        return;
    }
    bool left_arrow = false;
    bool right_arrow = false;
    if (ui_vint_object_visible(kOptionHighlightArrows, info->m_doc))
    {
        left_arrow = ui_is_mouse_over_vint_object(kOptionLeftArrow, info->m_doc);
        if (!left_arrow)
        {
            // Original: right is anywhere else on the highlighted row.
            right_arrow = ui_is_mouse_over_vint_object(kOptionRowHighlightBG, info->m_doc);
        }
    }
    ui_menu_selection_clicked(Menu_handle_aoptions, index, left_arrow, right_arrow);
}

// A menu hint: the tag's localized text with its button markup, and the button's image.
static void audio_menu_add_hint(hint_type ht, control_button_action action, const char* tag)
{
    char text[128];
    char image[256];
    // Magic number: 0x80, the hint text buffer's size.
    controls_fill_options_button_hint(text, 0x80, localization_get_display_string_req(tag, nullptr));
    ui_menu* menu = ui_menu_get_via_handle(Menu_handle_aoptions);
    if (menu != nullptr)
    {
        // Original: 0x40 passed as the image buffer's size.
        controls_get_button_image_name(image, 0x40, action, Player_controller_index);
        menu->add_hint(ht, action, text, image);
    }
}

// ---- external interface

// 0x00891460 (:243)
// Sound on, every volume at 100 and push to talk on are the defaults.
bool audio_are_changes_different_from_defaults()
{
    if (Sound_enable && audiolib_get_sound_enabled() && my_audio_levels.m_ambience == 100 && my_audio_levels.m_music == 100 && my_audio_levels.m_voice == 100 && my_audio_levels.m_sfx == 100 && my_audio_levels.m_overall == 100 && my_audio_levels.m_voicechat == 100 && multi_voice_push_to_talk_enabled())
    {
        return false;
    }
    return true;
}

// 0x008C3E10 (:266)
// Sound on (the menu's first row too), every volume at 100, push to talk on; the menu remade on the same row.
void audio_reset_defaults()
{
    Sound_enable = true;
    audiolib_set_sound_enabled(true);
    if (ui_menu* menu = ui_menu_get_via_handle(Menu_handle_aoptions))
    {
        ui_menu_item_base* item = menu->get_menu_item_via_index(0);
        if (item != nullptr && item->get_menu_item_type() == MI_CHAR_LIST)
        {
            static_cast<ui_menu_item_char_list*>(item)->set_selected_char(0);
        }
    }
    // Magic number: 100, full volume.
    my_audio_levels.m_ambience = 100;
    my_audio_levels.m_music = 100;
    my_audio_levels.m_voice = 100;
    my_audio_levels.m_sfx = 100;
    my_audio_levels.m_voicechat = 100;
    my_audio_levels.m_overall = 100;
    multi_voice_push_to_talk_set_enabled(true);
    audio_set_sound_levels();
    const int selected = ui_menu_get_selected_item(Menu_handle_aoptions);
    audio_menu_create_menu();
    ui_menu_set_selected_item(Menu_handle_aoptions, selected);
}

// 0x008917F0 (:425)
// The voice chat volume times the overall volume, clamped to [0, 1].
float get_voicechat_volume()
{
    float voicechat = 0.0f;
    float overall = 0.0f;
    audiolib_system_get_volume_scalar(AUDIO_OPTIONS_CATEGORY_VOICECHAT, &voicechat);
    audiolib_system_get_volume_scalar(AUDIO_OPTIONS_CATEGORY_OVERALL, &overall);
    float volume = overall * voicechat;
    if (overall * voicechat - 0.0f < 0.0f)
    {
        volume = 0.0f;
    }
    if (volume - 1.0f >= 0.0f)
    {
        volume = 1.0f;
    }
    return volume;
}

// 0x0090F700 (:580)
// The levels read, the document (none: back out), a control id and an input id; the menu under it hidden.
void audio_menu_enter(game_state prev_state)
{
    Sound_enable = audiolib_get_sound_enabled();
    audio_init_levels();
    Assets_loaded_aoptions = false;
    Change_made_aoptions = false;
    Doc_handle_aoptions = ui_load_document(cDoc_name_aoptions, DFIT_NONE, 0.0f, DLT_NORMAL, false, STREAM_PRIORITY_AUDIO_CACHE_LOAD, false);
    if (Doc_handle_aoptions == -1)
    {
        keen::debug::nativePrintArgs("Int3 critical function at %s(%d)\n", __FILE__, __LINE__);
        gameseq_pop_state();
        return;
    }
    Control_id_aoptions = (int)ui_request_control_id();
    Input_id_aoptions = ui_register_for_processed_input(IRT_UNLIMITED_INPUT, false);
    if (!Active)
    {
        main_menu_show_menu(false);
        return;
    }
    ingame_options_show_menu(false);
}

// 0x008F3EA0 (:612)
// The box goes, unsaved changes are reread from the options, the menu, document and ids go, the menu under it shows.
void audio_menu_exit(game_state new_state)
{
    if (Message_box_handle_aoptions != -1)
    {
        ui_hide_msgbox(Message_box_handle_aoptions, true);
        Message_box_handle_aoptions = -1;
    }
    if (Change_made_aoptions)
    {
        Change_made_aoptions = false;
        game_options_load_from_cache();
    }
    ui_menu_destroy(Menu_handle_aoptions);
    Menu_handle_aoptions = -1;
    // Magic number: a 0.5 s fade out.
    ui_unload_document(Doc_handle_aoptions, DFOT_NONE, 0.5f);
    Doc_handle_aoptions = -1;
    ui_relinquish_input_id(Input_id_aoptions);
    Input_id_aoptions = -1;
    ui_relinquish_control_id((unsigned int)Control_id_aoptions);
    Control_id_aoptions = 0;
    Display_character = true;
    if (Active)
    {
        ingame_options_show_menu(true);
        return;
    }
    main_menu_show_menu(true);
}

// 0x009016E0 (:694)
// The eight rows and the accept, cancel and defaults hints; without audio every row is disabled (the first selected).
void audio_menu_create_menu()
{
    if (Menu_handle_aoptions != -1)
    {
        ui_menu_destroy(Menu_handle_aoptions);
    }
    Menu_handle_aoptions = ui_menu_add(Doc_handle_aoptions, "MENU_AUDIO", nullptr, audio_hint_selection, "OM", true, -1, -1, false);
    ui_menu_add_char_list_item(Menu_handle_aoptions, "MENU_SOUND", Sound_enabled_list, 2, option_change_occurred_aoptions, Sound_enable ? 0 : 1, true);
    // Magic numbers: the sliders go from 0 to 100 in steps of 10.
    ui_menu_add_slider_item(Menu_handle_aoptions, "MENU_SOUND_AMBIENCE", &my_audio_levels.m_ambience, 0, 100, 10, option_change_occurred_aoptions, false);
    ui_menu_add_slider_item(Menu_handle_aoptions, "MENU_SOUND_MUSIC", &my_audio_levels.m_music, 0, 100, 10, option_change_occurred_aoptions, false);
    ui_menu_add_slider_item(Menu_handle_aoptions, "MENU_SOUND_VOICE", &my_audio_levels.m_voice, 0, 100, 10, option_change_occurred_aoptions, false);
    ui_menu_add_slider_item(Menu_handle_aoptions, "MENU_SOUND_VOICECHAT", &my_audio_levels.m_voicechat, 0, 100, 10, option_change_occurred_aoptions, false);
    ui_menu_add_slider_item(Menu_handle_aoptions, "MENU_SOUND_SFX", &my_audio_levels.m_sfx, 0, 100, 10, option_change_occurred_aoptions, false);
    ui_menu_add_slider_item(Menu_handle_aoptions, "MENU_SOUND_OVERALL", &my_audio_levels.m_overall, 0, 100, 10, option_change_occurred_aoptions, false);
    ui_menu_add_char_list_item(Menu_handle_aoptions, "MENU_PUSHTOTALK", cPushtotalk_list, 2, option_change_occurred_aoptions, multi_voice_push_to_talk_enabled() ? 0 : 1, true);
    audio_menu_add_hint(HT_SELECTION, CBA_MENU_SELECT, "MENU_ACCEPT_WITH_BUTTON");
    audio_menu_add_hint(HT_ABORT, CBA_MENU_ABORT, "MENU_CANCEL_WITH_BUTTON");
    audio_menu_add_hint(HT_MISC, CBA_MENU_Y, "MENU_DEFAULT_SETTINGS");
    if (!Audiolib_state.initialized)
    {
        // Magic number: the eight rows.
        for (int i = 0; i < 8; i++)
        {
            ui_menu_enable_menu_item(Menu_handle_aoptions, i, false);
        }
        ui_menu_set_selected_item(Menu_handle_aoptions, 0);
    }
}

// 0x00915DC0 (:754)
// A finished save: its box goes (failed: answered as cancel), the menu leaves. With a box up only the document runs;
// the menu once the document is loaded, then the mouse.
void audio_menu_process()
{
    if (Waiting_on_save_completion_aoptions)
    {
        bool done = true;
        if (!Multiplayer)
        {
            if (Message_box_handle_aoptions != -1)
            {
                ui_hide_msgbox(Message_box_handle_aoptions, !player_profile_last_save_successful(Player_controller_index != 0xFFFFFFFFu ? (int)Player_controller_index : 0));
            }
            if (ui_is_msgbox_active(Message_box_handle_aoptions))
            {
                done = false;
            }
            else
            {
                Message_box_handle_aoptions = -1;
            }
        }
        if (done)
        {
            Waiting_on_save_completion_aoptions = false;
            if (gameseq_get_state() == GS_MAINMENU_OPTIONS_AUDIO)
            {
                gameseq_pop_state();
            }
        }
    }
    if (Utility_doc_handle != -1 && ui_is_msgbox_active(-2))
    {
        if (Assets_loaded_aoptions)
        {
            ui_add_for_processing(Doc_handle_aoptions);
        }
        ui_process((unsigned int)Control_id_aoptions, false);
        return;
    }
    if (!Assets_loaded_aoptions)
    {
        switch (ui_get_doc_asset_state(Doc_handle_aoptions))
        {
        case ST_LOADING:
            break;
        case ST_LOADED:
            Assets_loaded_aoptions = true;
            break;
        case ST_UNLOADED:
            keen::debug::nativePrintArgs("Int3 critical function at %s(%d)\n", __FILE__, __LINE__);
            [[fallthrough]];
        case ST_ERROR:
            if (gameseq_get_state() == GS_MAINMENU_OPTIONS_AUDIO)
            {
                gameseq_pop_state();
            }
            break;
        default:
            keen::debug::nativePrintArgs("Int3 critical function at %s(%d)\n", __FILE__, __LINE__);
            break;
        }
        if (!Assets_loaded_aoptions)
        {
            ui_process((unsigned int)Control_id_aoptions, false);
            return;
        }
        audio_menu_create_menu();
    }
    if (Utility_doc_handle == -1 || !ui_is_msgbox_active(-2))
    {
        audio_menu_process_mouse_input();
    }
    ui_add_for_processing(Doc_handle_aoptions);
    ui_process((unsigned int)Control_id_aoptions, false);
}

// 0x00891900 (:832)
void audio_menu_handshake(rl_renderer*)
{
}

// 0x00901CC0 (:837)
void audio_menu_render(rl_renderer*)
{
    if (Assets_loaded_aoptions)
    {
        ui_add_for_rendering(Doc_handle_aoptions);
    }
    ui_render((unsigned int)Control_id_aoptions, false);
}
