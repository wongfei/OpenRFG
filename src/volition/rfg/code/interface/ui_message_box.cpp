// Original: volition/rfg/code/interface/ui_message_box.cpp
//
// The message boxes of the utility document (rfg_ui_utility, UI.md UI3/UI4): a pool of 4 boxes, each a menu on the
// utility document ("_MB" surtag) with its caption items and hints, an input lock and a callback answered with the
// selection and a msgbox_choice. Also the utility document's other data: subtitles, the save icon, the voice chat
// speaker list and the text chat hint, the resolution change image, the debug text items and the cursor.

#include "volition/rfg/code/interface/ui_message_box.h"

#include "keen/base/crc32.hpp"
#include "keen/base/string.hpp"
#include "keen/base/stringencoding.hpp"
#include "keen/base/systemtimer.hpp"
#include "keen/base/trace.hpp"
#include "keen/savedata/savedatahandler.hpp"
#include "kaiko/gamelink/gamelink.hpp"
#include "unit4/gamesession/localgamesession.hpp"
#include "unit4/gamesession/remotegamesession.hpp"
#include "volition/interface_lib/code/objects/animation/vint_object_animation.h"
#include "volition/interface_lib/code/objects/vint_object_base.h"
#include "volition/interface_lib/code/vint_data_item.h"
#include "volition/interface_lib/code/vint_document.h"
#include "volition/interface_lib/code/vint_variant.h"
#include "volition/rfg/code/control/control_gameplay.h"
#include "volition/rfg/code/control/control_map.h"
#include "volition/rfg/code/cutscene/video_cutscene.h"
#include "volition/rfg/code/gamewide/game_clock.h"
#include "volition/rfg/code/gamewide/game_save.h"
#include "volition/rfg/code/gamewide/gameseq.h"
#include "volition/rfg/code/hud/hud_ui_handbook.h"
#include "volition/rfg/code/hud/hud_ui_mission_briefing.h"
#include "volition/rfg/code/interface/loading.h"
#include "volition/rfg/code/interface/localization/localization_shared.h"
#include "volition/rfg/code/interface/ui_document_hud.h"
#include "volition/rfg/code/interface/ui_document_manager.h"
#include "volition/rfg/code/interface/ui_menu.h"
#include "volition/rfg/code/multi/multi_core/include/multi.h"
#include "volition/rfg/code/multi/multi_core/include/multi_core_platform.h"
#include "volition/rfg/code/multi/multi_core/include/multi_core_platform_internal.h"
#include "volition/rfg/code/multi/multi_core/include/multi_core_session_base.h"
#include "volition/rfg/code/multi/multi_game/multi_game.h"
#include "volition/rfg/code/multi/multi_game/multi_game_data.h"
#include "volition/rfg/code/multi/multi_game/multi_game_render.h"
#include "volition/rfg/code/rfg.h"
#include "volition/vlib/code/cfile/cfile.h"
#include "volition/vlib/code/input/input.h"
#include "volition/vlib/code/input/input_internal.h"
#include "volition/vlib/code/input/mouse.h"
#include "volition/vlib/code/math/crc.h"
#include "volition/vlib/code/math/vector2.h"
#include "volition/vlib/code/util/string_comp.h"
#include "volition/vlib/code/util/string_util.h"

#include "volition/vlib/code/time/timestamp.h"

#include <stdio.h>
#include <string.h>

// button_info and ui_button_info are in ui_document_manager.h since UI4 (Button_info is in ui_document_manager.cpp).

// ---- file-static globals
// bool s_localization_is_loading (0x01663023) is used here too; placed in localization_shared.h.
// bool Input_enabled (0x01664A4A) is used here too; placed in ui_document_manager.cpp.
// int Choice_last (0x01664B38) is used here too; placed in ui_document_manager.cpp.
// int Loading_screen_handle (0x01664B44) is used here too; placed in loading.h.
// int Current_input_lock_id (0x01664B4C) is used here too; placed in loading.cpp.
// int Current_input_lock_index (0x01664B50) is used here too; placed in loading.cpp.
// bool Input_enabled_uimen (0x01664FD0) is used here too; placed in loading.cpp.
static const char* cUI_Screen_Utility = {"rfg_ui_utility"}; // 0x01664FD4
static const char* cDebug_text_tl = {"debug_text_tl"}; // 0x01664FD8
static const char* cDebug_text_tr = {"debug_text_tr"}; // 0x01664FDC
static const char* cDebug_text_bl = {"debug_text_bl"}; // 0x01664FE0
static const char* cDebug_text_br = {"debug_text_br"}; // 0x01664FE4
static const char* cDebug_text_cl = {"debug_text_cl"}; // 0x01664FE8
static const char* cDebug_text_cr = {"debug_text_cr"}; // 0x01664FEC
// const char* cBlank_string (0x01664FF0) is used here too; placed in loading.cpp.
static const char* kVintCursorName = {"cursor"}; // 0x01664FF4
// int Utility_doc_handle (0x01664FF8) is used here too; placed in loading.cpp.
// int Msg_box_data_item_handle (0x01664FFC) is used here too; placed in loading.cpp.
static int Subtitle_data_item_handle = -1; // 0x01665000
static int Autosave_icon_data_item_handle = -1; // 0x01665004
static int Voice_chat_data_item_handle = -1; // 0x01665008
// bool Player_input_was_enabled (0x0166500C) is used here too; placed in loading.cpp.
// bool Player_camera_input_was_enabled (0x0166500D) is used here too; placed in loading.cpp.
static int Utility_background_image_handle = -1; // 0x01665010
static int gmbox_type = 1; // 0x01665014
static int gmbox_adds = 6; // 0x01665018
// farray<input_request_data,16> Input_types (0x016673C8) is used here too; placed in loading.cpp.
static timestamp Subtitle_timer; // 0x016676C8, initial data ffffffff
static float Autosave_current_duration = 3.0f; // 0x016676CC
static char cButton_text[128][256]; // 0x02BE9E30
// ui_button_info Button_info (0x02BF1E30) is used here too; placed in ui_document_manager.cpp.
int mp_stats_hint_config[7]; // 0x02BF214C (file static in the binary; external since UI5, the statistics menu)
char Background_image_name[64]; // 0x02BFE030 (external since UI6: the resolution change shows its image)
static char Voice_chat_speaker_list_text[2048]; // 0x02C00F00
static char Voice_chat_speaker_list_text_shadow[2048]; // 0x02C01700
// int Loading_screen_ref_cnt (0x02C080A0) is used here too; placed in loading.h.
static bool Subtitle_displaying; // 0x02C081AC
static int8_t Msg_box_handle_count; // 0x02C081AD
// bool Paused_game (0x02C081AE) is used here too; placed in loading.cpp.
static bool Kill_subtitle; // 0x02C081AF
static save_icon_type Autosave_icon_type; // 0x02C081B0
// bool Locked_controls (0x02C081B4) is used here too; placed in loading.cpp.
// bool Cursor_visible (0x02C081B5) is used here too; placed in ui_message_box.h.
bool Display_background_image; // 0x02C081B6 (external since UI6)
bool Ui_text_chat_enabled; // 0x02C081B7 (file static in the binary; external since UI5, the game lobby)
static unsigned int Voice_chat_speaker_list_index; // 0x02C081B8
static float Ui_text_chat_enabled_time; // 0x02C081BC
static bool Ui_text_chat_was_teamsay; // 0x02C081C0
static bool gtest_mbox; // 0x02C081C1
static int gmbox_int[6]; // 0x02C081C4
static int Bink_handle_startup; // 0x02C081DC
static voice_chat_speak_duration Voice_chat_speaker_list[2][16]; // 0x02C53368
msgbox_info_set Message_box_info_set; // 0x02CA0340

bool Subtitles_enabled = true; // 0x01664FD2
static bool subtitle_toggle_name = true; // 0x01664FD1
// The utility document's hint groups and the menu item group the mouse reads.
static const char* kMsgBoxHintGroup1 = {"hints_scale_group"}; // 0x016686B4
static const char* kMsgBoxHintGroup2 = {"hint_group2"}; // 0x016686B0
static const char* cMenu_group_namel = {"item_group"}; // 0x016686AC
// Input_enabled is a static of rfg.exe's interface unity (ui_document_manager.cpp, external since UI4).
extern bool Input_enabled; // 0x01664A4A
extern int cur_id; // 0x01668594, loading.cpp

// Magic number: a save box stays up at least 3.6 s (a console certification requirement, "TCR").
static const float Save_box_min_time = 3.6f;
// Magic number: the save icon shows for 3 s.
static const float Autosave_icon_time = 3.0f;

// ---- internal functions
static msgbox_info* ui_get_current_msgbox_info();
static bool ui_kill_msgbox(msgbox_info* mbi, bool ignore_tcr_time_requirements, bool call_exit_cb); // 0x008DC980 (:285)
static msgbox_info* ui_get_msgbox_info(int mbh); // 0x008CC350 (:550)
static void ui_process_utility_data(float dtime); // 0x008DCAE0 (:565)
static void ui_process_subtitle_data(float dtime); // 0x008A18A0 (:652)
static void ui_process_save_data(float dtime); // 0x008B96F0 (:729)
static void ui_process_voice_chat_data(float dtime); // 0x008EFBF0 (:797)
static void my_selection_cb(int selected_item); // 0x008F0300 (:993)
static void my_item_change_cb(int selected_item, void* new_value); // 0x008A1B40 (:1017)
static bool test_notify_me(int handle, int selection, msgbox_choice mbox_choice); // 0x008A1B50 (:1092)
static void ui_msgbox_test(); // 0x00909070 (:1121)

// ---- UI.md UI3: what the document manager needs of this file: the variables, the utility document
// and its data items, the debug text items, the current box, the loading box's hint callback and the cursor.

// Cursor_visible is a static of rfg.exe's interface unity (external, ui_message_box.h).
bool Cursor_visible; // 0x02C081B5
// The cursor's state of ui_cursor_update (statics of the functions in the binary).
static bool cursor_hidden;
static unsigned int auto_hide_start_time;

// 0x008EFB90 (:368)
// Original: under the pool's spinlock.
void ui_msgbox_init_variables()
{
    for (msgbox_info* box = Message_box_info_set.msgbox_info_pool.get_first(0); box != nullptr;)
    {
        box = Message_box_info_set.msgbox_info_pool.free(box);
    }
    Kill_subtitle = true;
    Cursor_visible = false;
}

// 0x00908E70 (:428)
// The utility document (permanent: message boxes, subtitles, the autosave icon, voice chat, the cursor, the
// resolution change image) and its data items; the cfile lock is lifted while it loads.
void ui_init_utility_doc()
{
    const bool locked = cf_is_locked();
    if (locked)
    {
        cf_open_lock(false);
    }
    Utility_doc_handle = ui_load_document(cUI_Screen_Utility, DFIT_NONE, 0.0f, DLT_PERMANENTLY_LOAD, false, STREAM_PRIORITY_AUDIO_CACHE_LOAD, false);
    // Magic numbers: the data items' element types (bool, string 5, float 4, int 3).
    vint_data_item* item = vint_data_item::create_new("MsgBoxInfo");
    if (item != nullptr)
    {
        const vint_variant_type types[6] = {VINT_PROP_TYPE_BOOL, (vint_variant_type)5, (vint_variant_type)4, (vint_variant_type)4, (vint_variant_type)4, (vint_variant_type)5};
        item->set_template(types, 6);
        Msg_box_data_item_handle = (int)item->get_handle();
        ui_add_data_item(Utility_doc_handle, Msg_box_data_item_handle);
    }
    item = vint_data_item::create_new("SubtitleTextUpdate");
    if (item != nullptr)
    {
        const vint_variant_type types[4] = {VINT_PROP_TYPE_BOOL, (vint_variant_type)5, (vint_variant_type)4, (vint_variant_type)3};
        item->set_template(types, 4);
        Subtitle_data_item_handle = (int)item->get_handle();
        ui_add_data_item(Utility_doc_handle, Subtitle_data_item_handle);
    }
    item = vint_data_item::create_new("AutoSaveUpdate");
    if (item != nullptr)
    {
        const vint_variant_type types[2] = {VINT_PROP_TYPE_BOOL, (vint_variant_type)4};
        item->set_template(types, 2);
        Autosave_icon_data_item_handle = (int)item->get_handle();
        ui_add_data_item(Utility_doc_handle, Autosave_icon_data_item_handle);
    }
    item = vint_data_item::create_new("VoiceChatUpdate");
    if (item != nullptr)
    {
        const vint_variant_type types[6] = {VINT_PROP_TYPE_BOOL, (vint_variant_type)4, (vint_variant_type)4, (vint_variant_type)5, (vint_variant_type)4, (vint_variant_type)5};
        item->set_template(types, 6);
        Voice_chat_data_item_handle = (int)item->get_handle();
        ui_add_data_item(Utility_doc_handle, Voice_chat_data_item_handle);
    }
    item = vint_data_item::create_new("ResChangeImage");
    if (item != nullptr)
    {
        const vint_variant_type types[2] = {VINT_PROP_TYPE_BOOL, (vint_variant_type)4};
        item->set_template(types, 2);
        Utility_background_image_handle = (int)item->get_handle();
        ui_add_data_item(Utility_doc_handle, Utility_background_image_handle);
    }
    if (locked)
    {
        cf_open_lock(true);
    }
}

// 0x008CC2E0 (:538)
// The last box of the pool's in-use list.
static msgbox_info* ui_get_current_msgbox_info()
{
    msgbox_info* last = nullptr;
    for (msgbox_info* box = Message_box_info_set.msgbox_info_pool.get_first(0); box != nullptr;
         box = Message_box_info_set.msgbox_info_pool.get_next(box))
    {
        last = box;
    }
    return last;
}

// 0x008F0350 (:1029)
// The boxes' menu hint callback: accept/cancel and yes/no boxes answer their callback (abort or OK with the
// selected item) and hide when it agrees; a loading box with cancel aborts; an OK box answers OK.
void my_selection_hint_cb(hint_type hint)
{
    msgbox_info* box = ui_get_current_msgbox_info();
    if (box == nullptr)
    {
        return;
    }
    int selection = -1;
    if (box->type == MBT_ACCEPT_CANCEL || box->type == MBT_YES_NO)
    {
        if (box->callback != nullptr)
        {
            msgbox_choice choice = MBC_ABORT;
            if (hint != HT_ABORT)
            {
                if (hint != HT_SELECTION)
                {
                    return;
                }
                choice = MBC_OK;
            }
            if (box->has_function_items && box->menu2_handle != -1)
            {
                selection = ui_menu_get_selected_item(box->menu2_handle);
            }
            if (!box->callback((int)box->handle, selection, choice))
            {
                return;
            }
        }
        ui_hide_msgbox((int)box->handle, false);
    }
    else if (box->type == MBT_LOADING_CANCEL && hint == HT_ABORT)
    {
        if (box->callback == nullptr || box->callback((int)box->handle, -1, MBC_ABORT))
        {
            ui_hide_msgbox((int)box->handle, false);
        }
    }
    else if (box->type == MBT_OK)
    {
        if (box->callback != nullptr)
        {
            if (box->has_function_items && box->menu2_handle != -1)
            {
                selection = ui_menu_get_selected_item(box->menu2_handle);
            }
            if (!box->callback((int)box->handle, selection, MBC_OK))
            {
                return;
            }
        }
        ui_hide_msgbox((int)box->handle, false);
    }
}

// 0x008CC3E0 (:1177)
// Removes the utility document's six debug text objects (retail shows no debug text). Original: a missing object
// would be dereferenced.
void ui_init_debug_text_items()
{
    vint_doc_info* info = ui_get_doc_info(Utility_doc_handle);
    if (info == nullptr || info->m_doc == nullptr)
    {
        return;
    }
    const char* names[6] = {cDebug_text_tl, cDebug_text_tr, cDebug_text_bl, cDebug_text_br, cDebug_text_cl, cDebug_text_cr};
    for (int i = 0; i < 6; i++)
    {
        vint_object_base* obj = vint_object_base::find_object(names[i], info->m_doc);
        obj->remove_from_parent();
        obj->destroy();
    }
}

// 0x008CC860 (:2022)
// The utility document's cursor object shown or hidden.
void ui_cursor_set_visible_internal(bool visible)
{
    vint_doc_info* info = ui_get_doc_info(Utility_doc_handle);
    if (info == nullptr || info->m_doc == nullptr)
    {
        return;
    }
    vint_object_base* cursor = vint_object_base::find_object(kVintCursorName, info->m_doc);
    if (cursor != nullptr)
    {
        cursor->set_property_value(crc_str("visible", 0), vint_variant(visible), 0);
        Ui_cursor_visible = visible;
    }
}

// 0x008A1C40 (:2041)
// The menus' cursor: the mouse stops auto centring while it shows.
void ui_cursor_set_visible(bool visible)
{
    if (Cursor_visible != visible)
    {
        Cursor_visible = visible;
        mouse_auto_centering(!visible);
    }
}

// 0x008A1C70 (:2052)
bool ui_cursor_get_visible()
{
    return Cursor_visible;
}

// 0x008CC8E0 (:2057)
void ui_cursor_set_pos(float x, float y)
{
    vint_doc_info* info = ui_get_doc_info(Utility_doc_handle);
    if (info == nullptr || info->m_doc == nullptr)
    {
        return;
    }
    vint_object_base* cursor = vint_object_base::find_object(kVintCursorName, info->m_doc);
    if (cursor != nullptr)
    {
        vint_variant anchor;
        anchor.set_type(VINT_PROP_TYPE_VECTOR2F);
        anchor = vector2(x, y);
        cursor->set_property_value(crc_str("anchor", 0), anchor, 0);
    }
}

// 0x008CCA60 (:2127)
// Without mouse movement for 3 s while a controller is in use, the cursor hides.
void ui_cursor_check_for_auto_hide(bool visible)
{
    if ((float)Mouse_dx == 0.0f && (float)Mouse_dy == 0.0f)
    {
        // Magic number: 3 s.
        if (input_get_playing_with_controller() && keen::SystemTimer::getCurrentMilliseconds() - auto_hide_start_time > 2999)
        {
            ui_cursor_set_visible_internal(false);
            return;
        }
    }
    else
    {
        auto_hide_start_time = keen::SystemTimer::getCurrentMilliseconds();
    }
    ui_cursor_set_visible_internal(visible);
}

// 0x008DCF60 (:2074)
// Each rendered frame: the cursor object follows the mouse while it is over the window (hidden outside it, and
// while the platform UI is up).
void ui_cursor_update()
{
    bool visible;
    if (Cursor_visible && !multi_core_platform_ui_is_active())
    {
        if (mouse_over_window())
        {
            cursor_hidden = false;
            ui_cursor_set_pos(Mouse_scaled_x, Mouse_scaled_y);
            visible = !cursor_hidden && Cursor_visible;
        }
        else
        {
            cursor_hidden = true;
            visible = false;
        }
    }
    else
    {
        visible = !cursor_hidden && Cursor_visible;
    }
    ui_cursor_check_for_auto_hide(visible);
}

// ---- UI.md UI4: the rest of the file

// True while a box is open (the pool has a box in use).
static bool any_msgbox_open()
{
    const object_pool_server<msgbox_info>& server = Message_box_info_set.msgbox_info_pool;
    return server.get_capacity() != (unsigned int)server.get_free();
}

// The boxes in use.
static int open_msgbox_count()
{
    const object_pool_server<msgbox_info>& server = Message_box_info_set.msgbox_info_pool;
    return (int)server.get_capacity() - server.get_free();
}

// 0x00908E60 (:56)
void unused_dcf_show_loading_screen()
{
    ui_show_loading_screen(false, "MENU_LOADING");
}

// 0x008EFB40 (:61)
// ui_hide_loading_screen, inlined.
void unused_dcf_hide_loading_screen()
{
    const int handle = Loading_screen_handle;
    if (Loading_screen_ref_cnt > 0)
    {
        Loading_screen_ref_cnt--;
        if (Loading_screen_ref_cnt < 1)
        {
            if (Loading_screen_handle != -1 && ui_is_msgbox_active(Loading_screen_handle))
            {
                ui_hide_msgbox(handle, false);
            }
            Loading_screen_handle = -1;
        }
    }
}

// 0x008A17B0 (:66)
void unused_dcf_toggle_subtitle_name()
{
    subtitle_toggle_name = !subtitle_toggle_name;
}

// 0x008F6B90 (:70)
// Magic number: 4 s without a duration argument.
void unused_dcf_subtitle_add()
{
    if (Console_command != 0)
    {
        char text[256];
        console_get_arg(2, 0);
        strncpy_safe(text, Console_arg, sizeof(text));
        console_get_arg(8, 0);
        int duration = Console_arg_int * 1000;
        if (duration == 0)
        {
            duration = 4000;
        }
        ui_add_subtitle(crc_str(text, 0), "text", duration);
        return;
    }
    if (Console_help != 0)
    {
        console_output("Usage: subtitle_add 'message' [duration in seconds]", nullptr, false);
    }
}

// 0x008F6C50 (:152)
// The pool frees a box through msgbox_info::clear.
msgbox_info_set::msgbox_info_set()
    : msgbox_info_pool(nullptr, &msgbox_info::clear, 0)
{
    save_load_active = false;
}

// 0x008CC270
msgbox_info_set::~msgbox_info_set()
{
}

// 0x008A17E0
voice_chat_speak_duration::voice_chat_speak_duration()
{
}

// 0x008BE230
voice_chat_speak_duration::voice_chat_speak_duration(const voice_chat_speak_duration& __that)
{
    user_id = __that.user_id;
    memcpy(user_name, __that.user_name, sizeof(user_name));
    speech_duration = __that.speech_duration;
    team_color = __that.team_color;
}

// 0x008A17D0 (:249)
float voice_chat_speak_duration::getKey(const voice_chat_speak_duration& data)
{
    return data.speech_duration;
}

// The last box closed: input back, the single player pause and the locked controls undone.
static void msgbox_all_closed()
{
    Input_enabled = true;
    Message_box_info_set.save_load_active = false;
    if (!Multiplayer)
    {
        if (Paused_game && game_is_paused())
        {
            game_unpause();
        }
        if (gameseq_state_is_active(GS_VIDEO_CUTSCENE_PLAY))
        {
            video_cutscene_unpause();
        }
        if (gameseq_state_is_active(GS_INGAME_MISSION_BRIEFING))
        {
            hud_ui_mission_briefing_pause(false, false);
        }
        if (gameseq_state_is_active(GS_INGAME_HANDBOOK_HUD))
        {
            hud_ui_handbook_pause_video(false);
        }
    }
    if (Locked_controls)
    {
        control_enable_player_input(Player_input_was_enabled);
        control_enable_player_camera_input(Player_camera_input_was_enabled);
        Locked_controls = false;
    }
}

// The box's menu and input lock go and the box goes back to the pool (the inlined free of ui_kill_msgbox and the
// timed boxes).
static void msgbox_free(msgbox_info* box)
{
    ui_menu_destroy(box->menu2_handle);
    ui_relinquish_input_id(box->input_id);
    Message_box_info_set.msgbox_info_pool.free(box);
    if (!any_msgbox_open())
    {
        msgbox_all_closed();
    }
    ui_wait_for_button_ups();
}

// 0x008DC980 (:285)
// LTCG: mbi in ESI. With call_exit_cb the callback is told (MBC_FORCED_EXIT) and may refuse. A save box not yet up
// for its minimum time only gets its remove time.
static bool ui_kill_msgbox(msgbox_info* mbi, bool ignore_tcr_time_requirements, bool call_exit_cb)
{
    if (mbi == nullptr)
    {
        return false;
    }
    if (call_exit_cb && mbi->callback != nullptr)
    {
        if (!mbi->callback((int)mbi->handle, 0, MBC_FORCED_EXIT))
        {
            return false;
        }
        mbi->callback = nullptr;
    }
    if (!ignore_tcr_time_requirements && mbi->remove_time < 0.0f && (mbi->type == MBT_SAVE || mbi->type == MBT_SAVE_PROFILE) && mbi->display_time < Save_box_min_time)
    {
        mbi->remove_time = Save_box_min_time;
        return true;
    }
    msgbox_free(mbi);
    return true;
}

// 0x008CC280 (:383)
void ui_util_background_image_create_data()
{
    vint_data_item* item = vint_data_item::create_new("ResChangeImage");
    if (item != nullptr)
    {
        const vint_variant_type types[2] = {VINT_PROP_TYPE_BOOL, VINT_PROP_TYPE_STRING};
        item->set_template(types, 2);
        Utility_background_image_handle = (int)item->get_handle();
        ui_add_data_item(Utility_doc_handle, Utility_background_image_handle);
    }
}

// 0x008A17F0 (:404)
void ui_util_show_background_image(const char* img_name)
{
    if (img_name != nullptr)
    {
        strncpy(Background_image_name, img_name, sizeof(Background_image_name));
        Display_background_image = true;
        return;
    }
    Display_background_image = false;
}

// 0x008A1820 (:415)
void ui_util_process_background_image_data()
{
    vint_data_item* item = vint_data_item::find((unsigned int)Utility_background_image_handle);
    if (item != nullptr)
    {
        item->set_element(0, vint_variant(Display_background_image));
        item->set_element(1, vint_variant((const char*)Background_image_name));
    }
}

// 0x008CC350 (:550)
static msgbox_info* ui_get_msgbox_info(int mbh)
{
    for (msgbox_info* box = Message_box_info_set.msgbox_info_pool.get_first(0); box != nullptr;
         box = Message_box_info_set.msgbox_info_pool.get_next(box))
    {
        if (box->handle == mbh)
        {
            return box;
        }
    }
    return nullptr;
}

// 0x008DCAE0 (:565)
// The "MsgBoxInfo" item from the top box: shown, loading, caption, loading text, the select button's image and
// whether the caption changed. Loading and save boxes count their display time and close at their remove time.
static void ui_process_utility_data(float dtime)
{
    bool shown = false;
    bool loading = false;
    bool modified = false;
    const char* caption = cBlank_string;
    const char* loading_text = cBlank_string;
    msgbox_info* box = nullptr;
    if (Utility_doc_handle != -1 && any_msgbox_open())
    {
        box = ui_get_current_msgbox_info();
    }
    if (box != nullptr)
    {
        switch (box->type)
        {
        case MBT_LOADING:
        case MBT_LOADING_CANCEL:
            loading_text = box->loading_text;
            loading = true;
            // fall through
        case MBT_SAVE:
        case MBT_SAVE_PROFILE:
            box->display_time += dtime;
            if (box->remove_time > 0.0f && box->remove_time <= box->display_time)
            {
                msgbox_free(box);
            }
            // fall through
        default:
            // Original: a box freed just above is still read (the pool's storage keeps its text).
            shown = true;
            caption = box->caption;
            if (!ui_menu_is_fading_in(box->menu2_handle))
            {
                modified = box->is_modified;
            }
            break;
        }
    }
    vint_data_item* item = vint_data_item::find((unsigned int)Msg_box_data_item_handle);
    if (item != nullptr)
    {
        item->set_element(0, vint_variant(shown));
        item->set_element(1, vint_variant(loading));
        item->set_element(2, vint_variant(caption));
        item->set_element(3, vint_variant(loading_text));
        char button[128];
        char image[128];
        controls_get_button_image_name(button, sizeof(button), CBA_MENU_SELECT, Player_controller_index);
        keen::formatString(image, sizeof(image), "[image:%s]", button);
        item->set_element(4, vint_variant((const char*)image));
        item->set_element(5, vint_variant(modified));
    }
    ui_util_process_background_image_data();
}

// The "SubtitleTextUpdate" item: shown (always true), on, text (null keeps it), duration.
static void subtitle_set(bool on, const char* text, bool set_duration, float duration)
{
    vint_data_item* item = vint_data_item::find((unsigned int)Subtitle_data_item_handle);
    if (item == nullptr)
    {
        return;
    }
    item->set_element(0, vint_variant(true));
    item->set_element(1, vint_variant(on));
    if (text != nullptr)
    {
        item->set_element(2, vint_variant(text));
    }
    if (set_duration)
    {
        item->set_element(3, vint_variant(duration));
    }
}

// 0x008A18A0 (:652)
// A pause hides the subtitle (but in the upgrades and video states); after the pause the subtitle comes back for the
// rest of its time; a kill clears it.
static void ui_process_subtitle_data(float)
{
    bool paused_hide = false;
    if (game_is_paused())
    {
        const game_state state = gameseq_get_state();
        if (state != GS_INGAME_UPGRADES_HUD && state != GS_VIDEO_CUTSCENE_PLAY)
        {
            paused_hide = true;
            if (Subtitle_displaying)
            {
                Subtitle_displaying = false;
                subtitle_set(false, nullptr, true, 0.0f);
            }
        }
    }
    if (!paused_hide && !Subtitle_displaying && Subtitle_timer.valid() && !Subtitle_timer.elapsed())
    {
        const int remaining = Subtitle_timer.time_until();
        subtitle_set(true, nullptr, true, (float)remaining / 1000.0f);
        Subtitle_displaying = true;
    }
    if (Kill_subtitle)
    {
        subtitle_set(false, "", true, 0.0f);
        Kill_subtitle = false;
        Subtitle_displaying = false;
        Subtitle_timer.invalidate();
    }
}

// 0x008B96F0 (:729)
// The "AutoSaveUpdate" item: shown while a save runs or for 3 s after ui_show_save_icon (not when the local user's
// profile has saving off), and its text.
static void ui_process_save_data(float dtime)
{
    vint_data_item* item = vint_data_item::find((unsigned int)Autosave_icon_data_item_handle);
    unit4::LocalGameSession* session = rfg_get_local_game_session();
    bool shown = true;
    if (session == nullptr || !session->isSaveInProgress())
    {
        if (!game_save_is_writing_ini_file() && game_save_get_state() != GSS_SAVING_WRITE && Autosave_current_duration >= Autosave_icon_time)
        {
            shown = false;
        }
    }
    if (Autosave_current_duration >= Autosave_icon_time)
    {
        // Original: the session is read without a null check. Private members of LocalGameSession, at their offsets:
        // m_pSaveDataHandler (0x04), m_userData.m_pData (0x18); the user's state (0x110) and save profile (0x11C).
        unsigned char* user = *(unsigned char**)((unsigned char*)session + 0x18);
        if (*(int*)(user + 0x110) == 3) // LocalUserState_Savedata
        {
            keen::SaveDataHandler* handler = *(keen::SaveDataHandler**)((unsigned char*)session + 0x04);
            if (!handler->isSaveEnabled(*(unsigned int*)(user + 0x11C)))
            {
                shown = false;
            }
        }
    }
    if (item != nullptr)
    {
        item->set_element(0, vint_variant(shown));
        const char* text;
        if (Autosave_icon_type == SAVE_ICON_TYPE_MANUAL)
        {
            text = "MENU_SAVING_GENERIC";
        }
        else if (Autosave_icon_type == SAVE_ICON_TYPE_OPTIONS)
        {
            text = "WAIT_FOR_OPTIONS_SAVE";
        }
        else if (Autosave_icon_type == SAVE_ICON_TYPE_PROFILE)
        {
            text = "MENU_SAVING_PROFILE";
        }
        else
        {
            text = "MENU_SAVING_AUTOSAVE";
        }
        if (s_localization_is_loading)
        {
            text = "SAVING...";
        }
        item->set_element(1, vint_variant(text));
    }
    Autosave_current_duration += dtime;
}

// 0x008B9840 (:786)
// The icon's 3 s start over unless the local user's profile has saving off.
void ui_show_save_icon(save_icon_type icon_type)
{
    unit4::LocalGameSession* session = rfg_get_local_game_session();
    // LocalGameSession's private m_userData.m_pData (0x18) and m_pSaveDataHandler (0x04), as above.
    unsigned char* user = *(unsigned char**)((unsigned char*)session + 0x18);
    if (*(int*)(user + 0x110) == 3) // LocalUserState_Savedata
    {
        keen::SaveDataHandler* handler = *(keen::SaveDataHandler**)((unsigned char*)session + 0x04);
        if (!handler->isSaveEnabled(*(unsigned int*)(user + 0x11C)))
        {
            Autosave_icon_type = icon_type;
            return;
        }
    }
    Autosave_current_duration = 0.0f;
    Autosave_icon_type = icon_type;
}

// keen::quickSort<voice_chat_speak_duration, float, voice_chat_speak_duration, 16> 0x008CE660 (sort.hpp:30) as the
// speaker list uses it: 16 entries never reach its partition step, so it is the stable insertion sort by
// speech_duration, ascending.
static void sort_speakers(voice_chat_speak_duration* data, unsigned int count)
{
    for (unsigned int i = 1; i < count; i++)
    {
        const voice_chat_speak_duration entry = data[i];
        unsigned int j = i;
        while (j != 0 && voice_chat_speak_duration::getKey(entry) < voice_chat_speak_duration::getKey(data[j - 1]))
        {
            data[j] = data[j - 1];
            j--;
        }
        data[j] = entry;
    }
}

// 0x008EFBF0 (:797)
// The "VoiceChatUpdate" item: in a lobby of a game link session (but on the lobby menus), the stations' speaking
// times (two lists, swapped each frame) become the speaker list text with a mute icon per speaker; then the text
// chat's open hint (1 s after the chat window opens, without a box).
static void ui_process_voice_chat_data(float dtime)
{
    vint_data_item* item = vint_data_item::find((unsigned int)Voice_chat_data_item_handle);
    g_gameLinkMutex.lock(0);
    kaiko::gamelink::GameLink* link = nullptr;
    unit4::RemoteGameSession* remote = get_remote_game_session();
    if (remote != nullptr)
    {
        link = remote->getGameLinkSession();
    }
    const unsigned int previous = (Voice_chat_speaker_list_index - 1) & 1;
    voice_chat_speak_duration* list = Voice_chat_speaker_list[Voice_chat_speaker_list_index];
    memset(list, 0, sizeof(Voice_chat_speaker_list[0]));
    if (link == nullptr || link->getCurrentState() != kaiko::GameLinkState_InLobby || gameseq_get_state() == GS_MULTIPLAYER_GAME_LOBBY || gameseq_get_state() == GS_MULTIPLAYER_PARTY_LOBBY)
    {
        item->set_element(0, vint_variant(false));
        item->set_element(1, vint_variant((const char*)nullptr));
        item->set_element(2, vint_variant((const char*)nullptr));
    }
    else
    {
        const kaiko::GameLinkRemoteStation* stations[16];
        const unsigned int count = link->getLobbyMembers(stations, 16);
        for (unsigned int i = 0; i < count; i++)
        {
            const kaiko::GameLinkRemoteStation* station = stations[i];
            kaiko::GameLinkVoiceChatPlayerInfo info;
            info.hasHeadset = false;
            info.isMuted = false;
            info.hasJustSpoken = false;
            bool has_info = false;
            float last_duration = 0.0f;
            for (unsigned int j = 0; j < 16; j++)
            {
                if (Voice_chat_speaker_list[previous][j].user_id.is_equal(station->id))
                {
                    if (station->myStation)
                    {
                        has_info = link->getVoiceChatLocalPlayerInfo(&info);
                    }
                    else
                    {
                        has_info = link->getVoiceChatRemotePlayerInfo(&info, *station);
                    }
                    last_duration = Voice_chat_speaker_list[previous][j].speech_duration;
                    break;
                }
            }
            voice_chat_speak_duration& entry = list[i];
            if (!station->myStation)
            {
                // Magic numbers: names shortened to 20 characters.
                rfg_shorten_display_string(station->userName, entry.user_name, sizeof(entry.user_name), 20, true);
            }
            else
            {
                // The local player is "you" (HM_KILL_YOU).
                const unsigned int key = keen::getCrc32LwrValue("HM_KILL_YOU");
                wchar_t* text = nullptr;
                if (key != 0 && !s_localization_is_loading)
                {
                    Localization_hash.get(key, text);
                }
                keen::convertUTF16ToUTF8(entry.user_name, sizeof(entry.user_name), (const uint16*)text);
            }
            entry.speech_duration = has_info && info.hasJustSpoken && !info.isMuted ? last_duration + dtime : 0.0f;
            entry.user_id = station->id;
            entry.team_color.set(255, 255, 255, 255);
            if (!station->myStation)
            {
                // Original: a multi_player_uid of the station is made and not used; the color is the local player's
                // team color (the game, transition or party session's own data).
                const multi_game_data* data = multi_game_data_get_my_data(ST_GAME, true, false);
                if (data == nullptr)
                {
                    data = multi_game_data_get_my_data(ST_TRANSITION, true, false);
                }
                if (data == nullptr)
                {
                    data = multi_game_data_get_my_data(ST_PARTY, true, false);
                }
                if (data != nullptr)
                {
                    multi_game_render_get_team_color(entry.team_color, MTCT_ICON, data->team);
                }
            }
            else
            {
                // Magic numbers: the local speaker's color.
                entry.team_color.set(0x78, 0xAA, 0xBE, 0xFF);
            }
        }
        sort_speakers(list, 16);
        int text_length = 0;
        int shadow_length = 0;
        Voice_chat_speaker_list_text[0] = '\0';
        Voice_chat_speaker_list_text_shadow[0] = '\0';
        for (unsigned int i = 0; i < 16; i++)
        {
            if (list[i].speech_duration <= 0.0f)
            {
                continue;
            }
            char hex[16];
            str_color2s_hex(hex, sizeof(hex), list[i].team_color, false);
            // Original: the line break goes before every entry but the list's first slot (not the first speaker).
            const char* separator = i != 0 ? "\n" : "";
            text_length += (int)keen::formatString(Voice_chat_speaker_list_text + text_length, sizeof(Voice_chat_speaker_list_text) - text_length, "%s[color:#%s]%s\\s[image:ui_fend_icon_mute]", separator, hex, list[i].user_name) - 1;
            shadow_length += (int)keen::formatString(Voice_chat_speaker_list_text_shadow + shadow_length, sizeof(Voice_chat_speaker_list_text_shadow) - shadow_length, "%s%s\\s[image_tint:ui_fend_icon_mute]", separator, list[i].user_name) - 1;
        }
        item->set_element(0, vint_variant(true));
        item->set_element(1, vint_variant((const char*)Voice_chat_speaker_list_text));
        item->set_element(2, vint_variant((const char*)Voice_chat_speaker_list_text_shadow));
    }
    Voice_chat_speaker_list_index = previous;
    bool teamsay_changed = false;
    if (!Ui_text_chat_enabled)
    {
        Ui_text_chat_enabled_time = 0.0f;
        item->set_element(3, vint_variant(false));
        item->set_element(4, vint_variant((const char*)nullptr));
    }
    else
    {
        Ui_text_chat_enabled_time += dtime;
        if (Ui_text_chat_enabled_time <= 1.0f || (Utility_doc_handle != -1 && any_msgbox_open()))
        {
            item->set_element(3, vint_variant(false));
            item->set_element(4, vint_variant((const char*)nullptr));
        }
        else
        {
            item->set_element(3, vint_variant(true));
            const bool teamsay = multi_game_allow_chat_teamsay();
            item->set_element(4, vint_variant(teamsay ? "MENU_CBA_OPEN_TEAM_CHAT_BUTTON" : "MENU_CBA_OPEN_CHAT_BUTTON"));
            if (teamsay != Ui_text_chat_was_teamsay)
            {
                Ui_text_chat_was_teamsay = teamsay;
                teamsay_changed = true;
            }
        }
    }
    item->set_element(5, vint_variant(teamsay_changed));
    g_gameLinkMutex.unlock();
}

// 0x008F0300 (:993)
// A caption item's selection: the callback gets its index with MBC_OK (an accept/cancel/select box answers abort
// for the first item, the cancel one, and OK for the other, with selection -1); the box closes when it agrees.
static void my_selection_cb(int selected_item)
{
    msgbox_info* box = ui_get_current_msgbox_info();
    if (box == nullptr)
    {
        return;
    }
    if (box->callback != nullptr)
    {
        msgbox_choice choice = MBC_OK;
        if (box->type == MBT_ACCEPT_CANCEL_SELECT)
        {
            choice = selected_item == 0 ? MBC_ABORT : MBC_OK;
            selected_item = -1;
        }
        if (!box->callback((int)box->handle, selected_item, choice))
        {
            return;
        }
    }
    ui_hide_msgbox((int)box->handle, false);
}

// 0x008A1B40 (:1017)
static void my_item_change_cb(int, void*)
{
}

// 0x008A1B50 (:1092)
static bool test_notify_me(int, int, msgbox_choice)
{
    return true;
}

// 0x008A1B60 (:1098)
void unused_dcf_ui_msgbox_test()
{
    console_get_arg(2, 0);
    if (v_stricmp(Console_arg, "OK") == 0)
    {
        gtest_mbox = true;
        gmbox_type = MBT_OK;
        return;
    }
    if (v_stricmp(Console_arg, "ACCEPT_CANCEL") == 0)
    {
        gtest_mbox = true;
        gmbox_type = MBT_ACCEPT_CANCEL;
        return;
    }
    if (v_stricmp(Console_arg, "SAVE") == 0)
    {
        gtest_mbox = true;
        gmbox_type = MBT_SAVE;
        return;
    }
    console_output("Usage: ui_msgbox_test ( OK | ACCEPT_CANCEL | SAVE )", nullptr, false);
}

// 0x008F0450 (:1116)
void unused_dcf_ui_kill_all_msgbox()
{
    ui_hide_msgbox(-2, true);
}

// 0x00909070 (:1121)
// The console's test box (ui_msgbox_test command): ui_add_msgbox inlined with a test caption and 6 int items.
static void ui_msgbox_test()
{
    if (!gtest_mbox)
    {
        return;
    }
    char title[128];
    char description[128];
    if (gmbox_type == MBT_OK)
    {
        _snprintf_safe(title, sizeof(title), "MBT_OK");
        _snprintf_safe(description, sizeof(description), "Please press the A-button");
    }
    else if (gmbox_type == MBT_ACCEPT_CANCEL)
    {
        _snprintf_safe(title, sizeof(title), "MBT_ACCEPT_CANCEL");
        _snprintf_safe(description, sizeof(description), "Press A- or B-button");
    }
    else if (gmbox_type == MBT_SAVE)
    {
        _snprintf_safe(title, sizeof(title), "MBT_SAVE");
        _snprintf_safe(description, sizeof(description), "Saving some schtuff...input means NOTHING to me!");
    }
    // Original: the test box's title is the literal "TITLE" (the formatted title is unused) and its description is
    // uninitialized for other types.
    const int handle = ui_add_msgbox((msgbox_type)gmbox_type, "TITLE", description, test_notify_me, false, false, nullptr, nullptr, false);
    if (handle != -1)
    {
        gtest_mbox = false;
        if (gmbox_type != MBT_SAVE && gmbox_type != MBT_SAVE_PROFILE)
        {
            for (int i = 0; i < gmbox_adds; i++)
            {
                // Magic number: gmbox_int holds 6 ints.
                if (i >= 6)
                {
                    return;
                }
                char caption[128];
                sprintf(caption, "Int%d", i);
                ui_msgbox_add_int_item(handle, caption, &gmbox_int[i], -10, 10, 1);
            }
        }
    }
}

// 0x008CC480 (:1279)
// The pool's boxes take the handles 0..3 in turn when the pool is built.
msgbox_info::msgbox_info()
{
    type = MBT_OK;
    callback = nullptr;
    caption[0] = '\0';
    loading_text[0] = '\0';
    menu2_item_handles.remove_all();
    has_function_items = false;
    is_modified = false;
    handle = Msg_box_handle_count;
    // Magic number: 4 boxes.
    Msg_box_handle_count = (int8_t)((Msg_box_handle_count + 1) % 4);
    remove_time = -1.0f;
    display_time = 0.0f;
    input_id = -1;
    menu2_handle = -1;
}

// 0x008B9890 (:1287)
void msgbox_info::clear()
{
    remove_time = -1.0f;
    type = MBT_OK;
    callback = nullptr;
    display_time = 0.0f;
    input_id = -1;
    caption[0] = '\0';
    loading_text[0] = '\0';
    menu2_handle = -1;
    menu2_item_handles.remove_all();
    has_function_items = false;
    is_modified = false;
}

// 0x008F0460 (:1306)
// Original: works on Message_box_info_set, not this.
void msgbox_info_set::clear_all()
{
    for (msgbox_info* box = Message_box_info_set.msgbox_info_pool.get_first(0); box != nullptr;)
    {
        box = Message_box_info_set.msgbox_info_pool.free(box);
    }
}

// 0x008CC4F0 (:1312)
// -2 asks for any box.
bool ui_is_msgbox_active(int mbh)
{
    if (Utility_doc_handle == -1 || mbh == -1)
    {
        return false;
    }
    if (mbh == -2)
    {
        return any_msgbox_open();
    }
    return ui_get_msgbox_info(mbh) != nullptr;
}

// 0x008A1BF0 (:1337)
void ui_kill_subtitle()
{
    Kill_subtitle = true;
}

// 0x008F04B0 (:1342)
// Not during the mission briefing (it shows its own text).
void ui_add_subtitle(unsigned int text_crc, const char* persona_name, int duration)
{
    if (!Subtitles_enabled || text_crc == 0 || gameseq_get_state() == GS_INGAME_MISSION_BRIEFING)
    {
        return;
    }
    char text[1024];
    text[0] = '\0';
    Subtitle_displaying = true;
    Subtitle_timer.set(duration);
    hud_ui_get_subtitle_display_text(text_crc, persona_name, subtitle_toggle_name, text, sizeof(text));
    subtitle_set(true, text, true, (float)duration / 1000.0f);
}

// 0x009095E0 (:1377)
void ui_msgbox_init()
{
    ui_msgbox_init_variables();
    ui_init_utility_doc();
    ui_init_debug_text_items();
}

// 0x008F0620 (:1390)
void ui_msgbox_shutdown()
{
    Message_box_info_set.clear_all();
    Subtitle_data_item_handle = -1;
    Msg_box_data_item_handle = -1;
    Autosave_icon_data_item_handle = -1;
    Voice_chat_data_item_handle = -1;
    if (Utility_doc_handle != -1)
    {
        // Magic number: a 0.5 s fade out.
        ui_unload_document(Utility_doc_handle, DFOT_NONE, 0.5f);
        Utility_doc_handle = -1;
    }
}

// 0x009095F0 (:1411)
// The utility document's data and its own process each frame; in single player gameplay an open box pauses.
void ui_msgbox_process(float dtime)
{
    ui_msgbox_test();
    vint_doc_info* info = ui_get_doc_info(Utility_doc_handle);
    if (info != nullptr && info->m_doc != nullptr)
    {
        ui_process_utility_data(dtime);
        ui_process_subtitle_data(dtime);
        ui_process_save_data(dtime);
        ui_process_voice_chat_data(dtime);
        ui_msgbox_process_mouse();
        info->m_doc->process(-1.0f);
    }
    if (!Multiplayer && Utility_doc_handle != -1 && any_msgbox_open() && gameseq_get_state() == GS_GAMEPLAY && !game_is_paused())
    {
        game_pause(true);
        Paused_game = true;
    }
}

// 0x008F06B0 (:1449)
void ui_msgbox_render()
{
    vint_doc_info* info = ui_get_doc_info(Utility_doc_handle);
    if (info != nullptr && info->m_doc != nullptr)
    {
        ui_cursor_update();
        info->m_doc->render();
    }
}

// 0x008F6C70 (:1471)
// A box: the utility document's highlight hint shows; save and loading types block other boxes (unless told to
// ignore that); in single player gameplay the pause popup's animation restarts; the first box pauses single player
// gameplay or locks the multiplayer player's controls; the box takes a locked input id and a menu with the hints of
// its type (or the overrides; an accept/cancel/select box gets them as caption items, cancel first).
int ui_add_msgbox(msgbox_type type, const char* title, const char* description, bool (*callback_fn)(int, int, msgbox_choice), bool surpress_accept_sounds, bool ignore_save_load_status, const char* button1_override, const char* button2_override, bool pause_music)
{
    int handle = -1;
    vint_doc_info* info = ui_get_doc_info(Utility_doc_handle);
    if (info != nullptr && info->m_doc != nullptr)
    {
        vint_object_base* hint = vint_object_base::find_object("highlight_hint", info->m_doc);
        if (hint != nullptr)
        {
            hint->set_property_value("visible", vint_variant(true), 0);
        }
    }
    if (Utility_doc_handle == -1)
    {
        return -1;
    }
    if (Message_box_info_set.save_load_active && !ignore_save_load_status)
    {
        return -1;
    }
    if (type >= MBT_SAVE && type <= MBT_LOADING_CANCEL)
    {
        Message_box_info_set.save_load_active = true;
    }
    if (gameseq_get_state() == GS_GAMEPLAY && !Multiplayer)
    {
        info = ui_get_doc_info(Utility_doc_handle);
        if (info != nullptr && info->m_doc != nullptr)
        {
            vint_object_base* fade = vint_object_base::find_object("popup_game_pause_fade_in", info->m_doc);
            if (fade != nullptr && fade->is_type(vint_object_animation::Object_type))
            {
                fade->set_property_value("start_time", vint_variant(info->m_doc->get_time_index()), 0);
                fade->set_property_value("is_paused", (const char*)nullptr, 0);
            }
        }
    }
    msgbox_info* box = Message_box_info_set.msgbox_info_pool.alloc(0);
    if (box != nullptr)
    {
        if (type == MBT_LOADING || type == MBT_LOADING_CANCEL)
        {
            strncpy(box->loading_text, title, sizeof(box->loading_text));
            title = cBlank_string;
        }
        handle = (int)box->handle;
        box->callback = callback_fn;
        // An unknown type is MBT_INVALID.
        box->type = (unsigned int)type < (unsigned int)NUM_MESSAGE_BOX_TYPES ? type : MBT_INVALID;
        if (open_msgbox_count() < 2)
        {
            Paused_game = false;
            Locked_controls = false;
            if (gameseq_state_is_active(GS_VIDEO_CUTSCENE_PLAY))
            {
                video_cutscene_pause();
            }
            if (!Multiplayer && gameseq_state_is_active(GS_GAMEPLAY))
            {
                if (gameseq_state_is_active(GS_INGAME_MISSION_BRIEFING))
                {
                    hud_ui_mission_briefing_pause(true, false);
                }
                if (gameseq_state_is_active(GS_INGAME_HANDBOOK_HUD))
                {
                    hud_ui_handbook_pause_video(true);
                }
                game_pause(pause_music);
                Paused_game = true;
            }
            else if (Multiplayer && gameseq_state_is_active(GS_GAMEPLAY))
            {
                Player_input_was_enabled = control_player_input_enabled();
                Player_camera_input_was_enabled = control_player_camera_input_enabled();
                control_enable_player_input(false);
                control_enable_player_camera_input(false);
                Locked_controls = true;
            }
        }
        // ui_request_input_id(IRT_UNLIMITED_INPUT, locked), inlined.
        if (cur_id == -1)
        {
            cur_id = 1;
        }
        int input_id;
        // Magic number: 16, the size of Input_types.
        if (Input_types.size() < 16)
        {
            input_request_data request;
            request.type = IRT_UNLIMITED_INPUT;
            request.locked = true;
            request.id = cur_id;
            Current_input_lock_index = Input_types.add(request);
            Current_input_lock_id = cur_id;
            input_id = cur_id;
            cur_id++;
        }
        else
        {
            keen::debug::nativePrintArgs("Int3 critical function at %s(%d)\n", __FILE__, __LINE__);
            input_id = -1;
        }
        box->input_id = input_id;
        strncpy(box->caption, description, sizeof(box->caption));
        box->menu2_handle = ui_menu_add(Utility_doc_handle, title, nullptr, my_selection_hint_cb, "_MB", true, 1, box->input_id, surpress_accept_sounds);
        if (box->menu2_handle == -1)
        {
            ui_hide_msgbox(handle, false);
            handle = -1;
        }
        else
        {
            const char* select = nullptr;
            const char* abort = nullptr;
            control_button_action abort_action = CBA_MENU_ABORT;
            if (type == MBT_OK)
            {
                // The OK box's only hint is its abort hint, on the select button.
                abort = "MENU_OK";
                abort_action = CBA_MENU_SELECT;
            }
            else if (type == MBT_ACCEPT_SELECT)
            {
                select = "MENU_OK";
            }
            else if (type == MBT_ACCEPT_CANCEL || type == MBT_ACCEPT_CANCEL_SELECT)
            {
                select = "MENU_ACCEPT";
                abort = "MENU_CANCEL";
            }
            else if (type == MBT_YES_NO)
            {
                select = "MENU_SIGN_IN_YES";
                abort = "MENU_SIGN_IN_NO";
            }
            else if (type == MBT_LOADING_CANCEL)
            {
                abort = "MENU_CANCEL";
            }
            if (button1_override != nullptr)
            {
                select = button1_override;
            }
            if (button2_override != nullptr)
            {
                abort = button2_override;
            }
            if (type == MBT_ACCEPT_CANCEL_SELECT)
            {
                ui_msgbox_add_caption_item(handle, abort, false, false);
                ui_msgbox_add_caption_item(handle, select, false, false);
            }
            else
            {
                if (select != nullptr)
                {
                    ui_menu_add_selection_hint(box->menu2_handle, select, CBA_MENU_SELECT);
                }
                if (abort != nullptr)
                {
                    ui_menu_add_abort_hint(box->menu2_handle, abort, abort_action);
                }
            }
            vint_data_item* item = vint_data_item::find((unsigned int)Msg_box_data_item_handle);
            if (item != nullptr)
            {
                // m_updated (protected; 0x42C) as the inlined set writes it.
                *(bool*)((unsigned char*)item + 0x42C) = true;
            }
        }
    }
    Input_enabled_uimen = true;
    input_set_feedback(Player_controller_index != 0xFFFFFFFF ? Player_controller_index : 0, 0.0f, 0.0f);
    return handle;
}

// A new item's handle in the box's list (the inlined farray add).
static void msgbox_add_item_handle(msgbox_info* box, int item_handle)
{
    if (box->menu2_item_handles.add(item_handle) == -1)
    {
        keen::debug::nativePrintArgs("Int3 critical function at %s(%d)\n", __FILE__, __LINE__);
    }
}

// 0x008F06E0 (:1647)
// A selectable line of the box (my_selection_cb answers it). Accept/cancel, yes/no and accept/select boxes then
// keep only a select hint without text.
int ui_msgbox_add_caption_item(int mbh, const char* caption, bool surpress_accept_sound, bool set_active_controller)
{
    msgbox_info* box = ui_get_msgbox_info(mbh);
    if (box == nullptr)
    {
        return -1;
    }
    const int item = ui_menu_add_function_item(box->menu2_handle, caption, my_selection_cb, true, surpress_accept_sound, set_active_controller);
    if (item != -1)
    {
        msgbox_add_item_handle(box, item);
        // Original: the index passed is the item count (one past the new item), so no item is disabled (enabled is
        // false: the index is past the menu's items).
        ui_menu_enable_menu_item(box->menu2_handle, box->menu2_item_handles.size(), false);
        box->has_function_items = true;
    }
    if (box->type == MBT_ACCEPT_CANCEL || box->type == MBT_ACCEPT_CANCEL_SELECT || box->type == MBT_YES_NO || box->type == MBT_ACCEPT_SELECT)
    {
        ui_menu_remove_selection_hint(box->menu2_handle);
        ui_menu_add_selection_hint(box->menu2_handle, nullptr, CBA_MENU_SELECT);
    }
    return item;
}

// 0x008CC570 (:1672)
int ui_msgbox_add_int_item(int mbh, const char* caption, int* variable, int min, int max, int incr)
{
    msgbox_info* box = ui_get_msgbox_info(mbh);
    if (box == nullptr)
    {
        return -1;
    }
    const int item = ui_menu_add_int_item(box->menu2_handle, caption, variable, min, max, incr, my_item_change_cb, true);
    if (item != -1)
    {
        msgbox_add_item_handle(box, item);
    }
    return item;
}

// 0x008CC600 (:1687)
int ui_msgbox_add_exp_item(int mbh, const char* caption, int* variable, int min, int max, int exp)
{
    msgbox_info* box = ui_get_msgbox_info(mbh);
    if (box == nullptr)
    {
        return -1;
    }
    const int item = ui_menu_add_exp_item(box->menu2_handle, caption, variable, min, max, exp, my_item_change_cb, true);
    if (item != -1)
    {
        msgbox_add_item_handle(box, item);
    }
    return item;
}

// 0x008CC690 (:1702)
int ui_msgbox_add_slider_item(int mbh, const char* caption, int* variable, int min, int max, int incr)
{
    msgbox_info* box = ui_get_msgbox_info(mbh);
    if (box == nullptr)
    {
        return -1;
    }
    const int item = ui_menu_add_slider_item(box->menu2_handle, caption, variable, min, max, incr, my_item_change_cb, true);
    if (item != -1)
    {
        msgbox_add_item_handle(box, item);
    }
    return item;
}

// 0x008CC720 (:1717)
int ui_msgbox_add_bool_item(int mbh, const char* caption, bool* variable)
{
    msgbox_info* box = ui_get_msgbox_info(mbh);
    if (box == nullptr)
    {
        return -1;
    }
    const int item = ui_menu_add_bool_item(box->menu2_handle, caption, variable, my_item_change_cb, true);
    if (item != -1)
    {
        msgbox_add_item_handle(box, item);
    }
    return item;
}

// 0x008CC7A0 (:1732)
int ui_msgbox_add_char_list_item(int mbh, const char* caption, const char** list, int num_chars)
{
    msgbox_info* box = ui_get_msgbox_info(mbh);
    if (box == nullptr)
    {
        return -1;
    }
    const int item = ui_menu_add_char_list_item(box->menu2_handle, caption, list, num_chars, my_item_change_cb, 0, true);
    if (item != -1)
    {
        msgbox_add_item_handle(box, item);
    }
    return item;
}

// 0x008DCE30 (:1747)
// -2 closes every box without a remove time (all with ignore_tcr_time_requirements), telling their callbacks, and
// stops at the first that refuses; a single box (without a remove time) closes without its callback. True without a
// box to close.
bool ui_hide_msgbox(int handle, bool ignore_tcr_time_requirements)
{
    if (handle == -1)
    {
        return false;
    }
    if (handle == -2)
    {
        msgbox_info* boxes[4];
        int count = 0;
        for (msgbox_info* box = Message_box_info_set.msgbox_info_pool.get_first(0); box != nullptr;
             box = Message_box_info_set.msgbox_info_pool.get_next(box))
        {
            boxes[count++] = box;
        }
        bool result = true;
        for (int i = 0; i < count; i++)
        {
            if (boxes[i]->remove_time < 0.0f || ignore_tcr_time_requirements)
            {
                result = result && ui_kill_msgbox(boxes[i], ignore_tcr_time_requirements, true);
            }
        }
        return result;
    }
    if (Utility_doc_handle == -1 || !any_msgbox_open())
    {
        return true;
    }
    for (msgbox_info* box = Message_box_info_set.msgbox_info_pool.get_first(0); box != nullptr;
         box = Message_box_info_set.msgbox_info_pool.get_next(box))
    {
        if (box->handle == handle && box->remove_time < 0.0f)
        {
            return ui_kill_msgbox(box, ignore_tcr_time_requirements, false);
        }
    }
    return true;
}

// 0x008A1C00 (:1802)
// Compiled out of retail: no debug text.
unsigned int ui_add_debug_text(const char*, debug_text_posn)
{
    return 0xFFFFFFFF;
}

// 0x008A1C10 (:1859)
bool ui_update_debug_text(unsigned int, const char*)
{
    return false;
}

// 0x008A1C20 (:1888)
bool ui_remove_debug_text(unsigned int)
{
    return false;
}

// 0x008CC820 (:1928)
int ui_msgbox_get_menu_handle(int mbh)
{
    msgbox_info* box = ui_get_msgbox_info(mbh);
    return box != nullptr ? box->menu2_handle : -1;
}

// 0x008CC840 (:1941)
float ui_msgbox_get_remove_time(int mbh)
{
    msgbox_info* box = ui_get_msgbox_info(mbh);
    return box != nullptr ? box->remove_time : -1.0f;
}

// 0x008A1C30 (:1951)
void ui_set_subtitles(bool on)
{
    Subtitles_enabled = on;
}

// 0x008F07B0 (:1955)
// The top box's hints and items under the mouse: a clicked hint answers as its button (an accept/cancel box with
// caption items and a loading box with cancel answer abort), a released click on an item selects it.
void ui_msgbox_process_mouse()
{
    msgbox_info* box = ui_get_current_msgbox_info();
    if (box == nullptr || Utility_doc_handle == -1 || !any_msgbox_open())
    {
        return;
    }
    const char* hint_groups[2] = {kMsgBoxHintGroup1, kMsgBoxHintGroup2};
    int hint = ui_mouse_process_hints(hint_groups, 2, Utility_doc_handle, true);
    if (hint > -1)
    {
        if ((box->type == MBT_ACCEPT_CANCEL && box->has_function_items) || box->type == MBT_LOADING_CANCEL)
        {
            hint = HT_ABORT;
        }
        my_selection_hint_cb((hint_type)hint);
    }
    const int index = ui_menu_process_mouseover(Utility_doc_handle, box->menu2_handle, cMenu_group_namel);
    if (Mouse_buttons[0].just_released && index > -1)
    {
        ui_menu_selection_clicked(box->menu2_handle, index, false, false);
    }
}

// 0x008CC980 (:2100)
// The box's caption (or a loading box's text) changes, unless it is the same text and not forced.
void ui_update_msgbox_caption(int mbh, const char* new_caption, bool force)
{
    msgbox_info* box = ui_get_msgbox_info(mbh);
    if (box == nullptr)
    {
        return;
    }
    if (box->type != MBT_LOADING && box->type != MBT_LOADING_CANCEL)
    {
        if (!force && strcmp(box->caption, new_caption) == 0)
        {
            return;
        }
        box->is_modified = true;
        strcpy_s(box->caption, sizeof(box->caption), new_caption);
        return;
    }
    if (!force && strcmp(box->loading_text, new_caption) == 0)
    {
        return;
    }
    box->is_modified = true;
    strcpy_s(box->loading_text, sizeof(box->loading_text), new_caption);
}

// 0x008CCAC0 (:2148)
void ui_msgbox_hide_selection_hints(bool hide)
{
    vint_doc_info* info = ui_get_doc_info(Utility_doc_handle);
    if (info == nullptr || info->m_doc == nullptr)
    {
        return;
    }
    vint_object_base* hint = vint_object_base::find_object("highlight_hint", info->m_doc);
    if (hint != nullptr)
    {
        hint->set_property_value("visible", vint_variant(!hide), 0);
    }
}

// 0x008A1C80 (:2156)
void ui_enable_chat_window(vchat_position chat_position)
{
    Ui_text_chat_enabled = true;
    console_enable_chat_window(chat_position);
}

// 0x008A1C90 (:2164)
void ui_disable_chat_window()
{
    console_disable_chat_window();
    Ui_text_chat_enabled = false;
}
