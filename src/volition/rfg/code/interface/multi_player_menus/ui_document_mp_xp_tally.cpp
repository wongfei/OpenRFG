// Original: volition/rfg/code/interface/multi_player_menus/ui_document_mp_xp_tally.cpp
//
// The experience tally (GS_MULTIPLAYER_XP_TALLY, UI.md UI5) on rfg_ui_mp_xp_tally: after a match (above the results)
// the match's and its bonus experience count down into the total (25 a second each, with the tally loop sound and
// the level up slam), the level bar, the unlocks reached (paged), the bonus experience events of the match, the hidden
// challenges; from the statistics menu (or the live menu) the lifetime figures and all unlocks. Its input pages the
// results or the statistics underneath, scrolls the events and continues.

#include "volition/rfg/code/interface/multi_player_menus/ui_document_mp_xp_tally.h"

#include "kaiko/gamelink/gamelink.hpp"
#include "keen/base/string.hpp"
#include "keen/base/trace.hpp"
#include "volition/audio_lib/code/audiolib_api.h"
#include "volition/audio_lib/code/audiolib_att_loop_rel.h"
#include "volition/interface_lib/code/objects/vint_object_base.h"
#include "volition/interface_lib/code/vint_data_group.h"
#include "volition/interface_lib/code/vint_data_item.h"
#include "volition/interface_lib/code/vint_document.h"
#include "volition/interface_lib/code/vint_string_processing.h"
#include "volition/interface_lib/code/vint_variant.h"
#include "volition/rfg/code/control/control_map.h"
#include "volition/rfg/code/gamewide/downloadable_content.h"
#include "volition/rfg/code/gamewide/gameaudio.h"
#include "volition/rfg/code/gamewide/gameseq.h"
#include "volition/rfg/code/gamewide/stats.h"
#include "volition/rfg/code/interface/loading.h"
#include "volition/rfg/code/interface/localization/localization_shared.h"
#include "volition/rfg/code/interface/main_menu.h"
#include "volition/rfg/code/interface/multi_player_menus/multi_player_menu_system.h"
#include "volition/rfg/code/interface/multi_player_menus/ui_document_mp_results.h"
#include "volition/rfg/code/interface/multi_player_menus/ui_document_mp_stats.h"
#include "volition/rfg/code/interface/ui_document_hud_multiplayer.h"
#include "volition/rfg/code/interface/ui_document_manager.h"
#include "volition/rfg/code/interface/ui_menu.h"
#include "volition/rfg/code/interface/ui_message_box.h"
#include "volition/rfg/code/multi/multi_core/include/multi.h"
#include "volition/rfg/code/multi/multi_core/include/multi_core_platform_internal.h"
#include "volition/rfg/code/multi/multi_game/multi_game_badge.h"
#include "volition/rfg/code/multi/multi_game/multi_game_character.h"
#include "volition/rfg/code/multi/multi_game/multi_game_data.h"
#include "volition/rfg/code/multi/multi_game/multi_game_party.h"
#include "volition/rfg/code/multi/multi_game/multi_game_playlists.h"
#include "volition/rfg/code/multi/multi_game/multi_game_stats.h"
#include "volition/rfg/code/multi/multi_game/multi_game_xp.h"
#include "volition/rfg/code/objects/inventory_item.h"
#include "volition/rfg/code/weapons/weapons.h"
#include "volition/vlib/code/input/mouse.h"
#include "volition/vlib/code/math/floating.h"
#include "volition/vlib/code/os/frametime.h"
#include "volition/vlib/code/time/timestamp.h"
#include "volition/vlib/code/util/color.h"
#include "volition/vlib/code/util/string_util.h"
#include "volition/vlib/code/util/unicode_tools.h"
#include "volition/vlib/code/util/varray.h"

#include <math.h>
#include <stdlib.h>

// ---- file-private types (rfg.pdb)
enum mp_menu_exit_actions_tally
{
    MMEA_XT_POP = 0,
    MMEA_XT_PUSH = 1,
    NUM_MP_XT_MENU_EXIT_ACTIONS = 2,
};

enum mp_menu_states_tally
{
    MMS_XT_LOADING = 0,
    MMS_XT_LOADED = 1,
    MMS_XT_UNLOADING = 2,
    MMS_XT_UNLOADED = 3,
    NUM_MP_XT_MENU_STATES = 4,
};

// size 0x88
struct mp_xp_tally_shell_info
{

    mp_menu_states_tally processed_state; // 0x00
    mp_menu_states_tally state; // 0x04
    mp_menu_states_tally pending_state; // 0x08
    mp_menu_exit_actions_tally exit_action; // 0x0C
    game_state exit_push_target; // 0x10
    int doc_handle; // 0x14
    int data_item_handles[5]; // 0x18
    int events_data_group_handle; // 0x2C
    farray<int,16> events_data_item_handles; // 0x30
    unsigned int control_id; // 0x80
    int input_id; // 0x84
};
static_assert(sizeof(mp_xp_tally_shell_info) == 0x88, "mp_xp_tally_shell_info");

// size 0x8
struct xp_info_entry
{
    const multi_xp_event* xp_event; // 0x00
    int count_of_event; // 0x04
};
static_assert(sizeof(xp_info_entry) == 0x8, "xp_info_entry");

enum xp_tally_hint_choices
{
    XPT_HINT_INVALID = -1,
    XPT_HINT_ABORT = 0,
    XPT_HINT_SELECT = 1,
    XPT_HINT_LEFT = 2,
    XPT_HINT_RIGHT = 3,
    NUM_XPT_HINTS = 4,
};

// ---- file-static globals
// bool s_localization_is_loading (0x01663023) is used here too; placed in localization_shared.h.
// int Mp_msgbox_handle (0x016632E0) is used here too; placed in ui_document_hud_multiplayer.cpp.
bool Xp_tally_connected_when_entered; // 0x016631C7 (declared in ui_document_mp_xp_tally.h)
static const char* Ui_mp_shell_name_tally = {"rfg_ui_mp_xp_tally"}; // 0x01663D14
static const char* Menu_data_item_names_tally[5] = {"multiplayer_xp_tally_labels", "multiplayer_xp_tally_values", "multiplayer_xp_tally_unlocks", "multiplayer_xp_tally_hints", "multiplayer_xp_tally_challenges"}; // 0x01663D18
static const char* Match_xp_string = {"MENU_MATCH_XP"}; // 0x01663D2C
static const char* Lifetime_xp_string = {"MENU_LIFETIME_XP"}; // 0x01663D30
static const char* Match_bonus_xp_string = {"MENU_MATCH_BONUS_XP"}; // 0x01663D34
static const char* Lifetime_bonus_xp_string = {"MENU_LIFETIME_BONUS_XP"}; // 0x01663D38
static const char* Total_xp_string = {"MENU_TOTAL_XP"}; // 0x01663D3C
static const char* To_next_level_string = {"MENU_TO_NEXT_LEVEL"}; // 0x01663D40
static const char* Level_up_string = {"MENU_LEVEL_UP"}; // 0x01663D44
static const char* Bonus_xp_event_string = {"MENU_BONUS_XP_EVENT"}; // 0x01663D48
static const char* Playlist_string_tally = {"MENU_PLAYLIST"}; // 0x01663D4C
static const char* Unlocks_noun_string = {"MENU_UNLOCKS_NOUN"}; // 0x01663D58
static const char* Continue_string = {"MENU_CONTINUE"}; // 0x01663D5C
static const char* Back_string_tally = {"MENU_BACK"}; // 0x01663D60
static const char* None_string_tally = {"MENU_NONE"}; // 0x01663D64
static xp_tally_hint_choices Current_mouse_hint_choice_tally = XPT_HINT_INVALID; // 0x01663D68, initial data ffffffff
static mp_stats_hint_choices Current_mp_stats_choice = MPS_HINT_INVALID; // 0x01663D6C, initial data ffffffff
// mp_match_results_shell_info Mp_match_results_info (0x01666900) is used here too; placed in ui_document_mp_results.cpp.
// timestamp_realtime Auto_continue_timestamp (0x0166698C) is used here too; placed in ui_document_mp_results.cpp.
// mp_match_stats_shell_info Mp_match_stats_info (0x01666A20) is used here too; placed in ui_document_mp_stats.cpp.
static mp_xp_tally_shell_info Xp_tally_menu_info; // 0x01666C68
static timestamp Xp_countdown_delay; // 0x01666CF0, initial data ffffffff
static int xp_tally_hint_config[4]; // 0x02BF2208
static xp_info_entry My_xp_events[84]; // 0x02BF4510
static int Xp_tally_alr; // 0x02BFD470
// bool No_matchmaking (0x02C03637) and bool Pending_pop (0x02C03640) are placed in ui_document_mp_results.cpp.
static bool s_nextGame; // 0x02C03642
static bool Xp_tally_from_game; // 0x02C03643
int Xp_tally_num_unlocks; // 0x02C036AC (declared in ui_document_mp_xp_tally.h)
static float Current_match_xp; // 0x02C036B0
static float Current_match_bonus_xp; // 0x02C036B4
static float Current_total_xp; // 0x02C036B8
static float Last_frame_total_xp; // 0x02C036BC
static int Final_xp; // 0x02C036C0
static int Xp_event_index_offset; // 0x02C036C4
static const multi_xp_unlock* Xp_unlocks; // 0x02C036C8
static int Num_my_xp_events; // 0x02C036CC
static int Current_unlock_tally; // 0x02C036D0
static int Xp_tally_menu_selection; // 0x02C036D4
static int xp_tally_num_hints; // 0x02C036D8
// The events' scroll bar (a header constant in rfg.exe, this file's copy).
static const char* kScrollBarOutline = {"event_scrollbar"}; // 0x0166868C

// ---- internal functions

// The events shown at once: 16, or 12 in Arabic (ar-EG).
static int xp_tally_visible_events()
{
    // Magic numbers: 12 + 4 rows.
    return (Current_language != keen::LanguageId_Ar_Eg ? 4 : 0) + 12;
}

// 0x008E5EB0
// LTCG: raw_text in ESI, unicode_buf in EDI. A tag's localized text (or "!!" and the tag), coloured when asked.
static void get_colorized_localized_text_tally(wchar_t* unicode_buf, int unicode_buf_size, const char* raw_text, bool colorize, const color* text_color)
{
    wchar_t hex[32];
    if (colorize)
    {
        char hex_text[32];
        str_color2s_hex(hex_text, sizeof(hex_text), *text_color, false);
        copy_to_wide(hex, hex_text, 0x20);
    }
    const wchar_t* text = raw_text != nullptr ? localization_get_display_string_req(raw_text, nullptr) : nullptr;
    wchar_t missing[512];
    if (text == nullptr)
    {
        wchar_t raw[512];
        copy_to_wide(raw, raw_text, 0x200);
        _snprintf_safe(missing, 0x200, L"!!%ls", raw);
        text = missing;
    }
    if (!colorize)
    {
        _snprintf_safe(unicode_buf, (unsigned int)unicode_buf_size, L"%ls", text);
        return;
    }
    _snprintf_safe(unicode_buf, (unsigned int)unicode_buf_size, L"\r[color:#%ls]%ls[color:#FFFFFFFF]", hex, text);
}

static void xp_tally_load_global_data_items();
static void xp_tally_unload_ui_information();

// 0x008F3970
// Loaded: the data items and the events' group (the results' first frame is over); unloading: popped (and, going on
// to the next matchmaking game, the results too) or pushed.
static void xp_tally_do_state_change()
{
    do
    {
        Xp_tally_menu_info.state = Xp_tally_menu_info.pending_state;
        if (Xp_tally_menu_info.pending_state == MMS_XT_LOADED)
        {
            xp_tally_load_global_data_items();
            vint_data_group* group = vint_data_group::create_new("multiplayer_xp_tally_event");
            if (group != nullptr)
            {
                Xp_tally_menu_info.events_data_group_handle = group->get_handle();
                ui_add_data_group(Xp_tally_menu_info.doc_handle, Xp_tally_menu_info.events_data_group_handle);
                Xp_tally_menu_info.events_data_item_handles.clear();
            }
            Entering_results = false;
        }
        else
        {
            if (Xp_tally_menu_info.pending_state != MMS_XT_UNLOADING)
            {
                return;
            }
            xp_tally_unload_ui_information();
            if (Xp_tally_menu_info.exit_action == MMEA_XT_POP)
            {
                gameseq_pop_state();
                if (s_nextGame)
                {
                    s_nextGame = false;
                    if (!Multi_syslink)
                    {
                        gameseq_pop_state();
                    }
                }
            }
            else if (Xp_tally_menu_info.exit_action == MMEA_XT_PUSH)
            {
                gameseq_push_state(Xp_tally_menu_info.exit_push_target, true, false);
            }
            Xp_tally_menu_info.pending_state = MMS_XT_UNLOADED;
        }
    } while (Xp_tally_menu_info.state != Xp_tally_menu_info.pending_state);
}

// 0x0088F090
// LTCG: new_state in EAX.
static void xp_tally_set_state(mp_menu_states_tally new_state)
{
    Xp_tally_menu_info.pending_state = new_state;
}

// 0x0088F0A0
static void xp_tally_init_state()
{
    Xp_tally_menu_info.processed_state = MMS_XT_UNLOADED;
    Xp_tally_menu_info.state = MMS_XT_UNLOADED;
    Xp_tally_menu_info.pending_state = MMS_XT_UNLOADED;
}

// 0x0088F0C0
static void xp_tally_init_info()
{
    for (int i = 0; i < 5; i++)
    {
        Xp_tally_menu_info.data_item_handles[i] = -1;
    }
}

// 0x008D5CA0
// The five data items: labels {uint badge, then strings and an int count}, values {5 ints, bool, float}, unlocks
// (6 strings), hints (2 strings), challenges (16 strings).
static void xp_tally_load_global_data_items()
{
    unsigned int handle = 0;
    for (int i = 0; i < 5; i++)
    {
        vint_data_item* item = vint_data_item::create_new(Menu_data_item_names_tally[i]);
        if (item != nullptr)
        {
            switch (i)
            {
            case 0:
            {
                vint_variant_type types[10] = {VINT_PROP_TYPE_UINT, VINT_PROP_TYPE_STRING, VINT_PROP_TYPE_STRING, VINT_PROP_TYPE_STRING, VINT_PROP_TYPE_STRING,
                                               VINT_PROP_TYPE_STRING, VINT_PROP_TYPE_STRING, VINT_PROP_TYPE_STRING, VINT_PROP_TYPE_STRING, VINT_PROP_TYPE_INT};
                item->set_template(types, 10);
                break;
            }
            case 1:
            {
                vint_variant_type types[7] = {VINT_PROP_TYPE_INT, VINT_PROP_TYPE_INT, VINT_PROP_TYPE_INT, VINT_PROP_TYPE_INT, VINT_PROP_TYPE_INT, VINT_PROP_TYPE_BOOL, VINT_PROP_TYPE_FLOAT};
                item->set_template(types, 7);
                break;
            }
            case 2:
            {
                vint_variant_type types[6] = {VINT_PROP_TYPE_STRING, VINT_PROP_TYPE_STRING, VINT_PROP_TYPE_STRING, VINT_PROP_TYPE_STRING, VINT_PROP_TYPE_STRING, VINT_PROP_TYPE_STRING};
                item->set_template(types, 6);
                break;
            }
            case 3:
            {
                vint_variant_type types[2] = {VINT_PROP_TYPE_STRING, VINT_PROP_TYPE_STRING};
                item->set_template(types, 2);
                break;
            }
            case 4:
            {
                vint_variant_type types[16];
                for (int t = 0; t < 16; t++)
                {
                    types[t] = VINT_PROP_TYPE_STRING;
                }
                item->set_template(types, 16);
                break;
            }
            default:
                break;
            }
            handle = item->get_handle();
        }
        // Original: a failed create stores (and adds) the previous item's handle (0 for the first).
        Xp_tally_menu_info.data_item_handles[i] = (int)handle;
        ui_add_data_item(Xp_tally_menu_info.doc_handle, (int)handle);
    }
}

// 0x008D5DF0
static void xp_tally_load_global_data_groups()
{
    vint_data_group* group = vint_data_group::create_new("multiplayer_xp_tally_event");
    if (group != nullptr)
    {
        Xp_tally_menu_info.events_data_group_handle = group->get_handle();
        ui_add_data_group(Xp_tally_menu_info.doc_handle, Xp_tally_menu_info.events_data_group_handle);
        Xp_tally_menu_info.events_data_item_handles.clear();
    }
}

// 0x008D5E30
static void xp_tally_load_ui_information()
{
    xp_tally_load_global_data_items();
    xp_tally_load_global_data_groups();
}

// 0x008E6010
// LTCG: the document handle in EDI. The tally loop sound stops (100 ms release).
static void xp_tally_unload_ui_information()
{
    vint_doc_info* info = ui_get_doc_info(Xp_tally_menu_info.doc_handle);
    if (info != nullptr)
    {
        info->release_data_groups();
        info->release_data_items();
        info->release_data_events();
    }
    xp_tally_init_info();
    if (Mp_msgbox_handle != -1)
    {
        ui_hide_msgbox(Mp_msgbox_handle, false);
        Playercard_player_uid.invalidate();
    }
    Mp_msgbox_handle = -1;
    Mp_msgbox_type = MP_MBT_NONE;
    if (Xp_tally_alr != -1)
    {
        // Magic number: a 100 ms release.
        audiolib_att_loop_rel_stop(Xp_tally_alr, 100);
        Xp_tally_alr = -1;
    }
}

// 0x0088F0E0
static void xp_tally_process_data_item_labels()
{
    vint_data_item* vdi = vint_data_item::find((unsigned int)Xp_tally_menu_info.data_item_handles[0]);
    if (vdi == nullptr)
    {
        return;
    }
    char name[192];
    vint_encode_wide_text(name, sizeof(name), multi_game_data_get_my_name());
    unsigned int badge;
    multi_game_badge_get_my_badge(badge);
    vdi->set_element(0, vint_variant(badge));
    vdi->set_element(1, vint_variant((const char*)name));
    vdi->set_element(2, vint_variant(Xp_tally_from_game ? Match_xp_string : Lifetime_xp_string));
    vdi->set_element(3, vint_variant(Xp_tally_from_game ? Match_bonus_xp_string : Lifetime_bonus_xp_string));
    vdi->set_element(4, vint_variant(Total_xp_string));
    vdi->set_element(5, vint_variant(To_next_level_string));
    vdi->set_element(6, vint_variant(To_next_level_string));
    vdi->set_element(7, vint_variant(Xp_tally_from_game ? Level_up_string : (const char*)nullptr));
    vdi->set_element(8, vint_variant("MENU_EVENTS"));
    vdi->set_element(9, vint_variant(Num_my_xp_events));
}

// 0x008AF180
// After a 2.5 s delay the match's experience, then its bonus, count into the total at 25 a second; the level bar
// follows the total, with the slam on a level up.
static void xp_tally_process_data_item_values()
{
    vint_data_item* vdi = vint_data_item::find((unsigned int)Xp_tally_menu_info.data_item_handles[1]);
    if (vdi == nullptr)
    {
        return;
    }
    Last_frame_total_xp = Current_total_xp;
    if (Xp_countdown_delay.elapsed() || !Xp_countdown_delay.valid())
    {
        if (Xp_tally_alr == -1 && (Current_match_xp > 0.0f || Current_match_bonus_xp > 0.0f))
        {
            audiolib_att_loop_rel_start(Ga_cue_ids.mission_point_tally_alr, &Xp_tally_alr, false, nullptr, nullptr);
        }
        // Magic number: 25 experience points a second.
        if (Current_match_xp > 0.0f)
        {
            float step = Frametime * 25.0f;
            if (Current_match_xp - Frametime * 25.0f < 0.0f)
            {
                step = Current_match_xp;
            }
            Current_match_xp -= step;
            Current_total_xp += step;
        }
        else if (Current_match_bonus_xp > 0.0f)
        {
            float step = Frametime * 25.0f;
            if (Current_match_bonus_xp - Frametime * 25.0f < 0.0f)
            {
                step = Current_match_bonus_xp;
            }
            Current_match_bonus_xp -= step;
            Current_total_xp += step;
        }
        else
        {
            Current_total_xp = (float)Final_xp;
            if (Xp_tally_alr != -1)
            {
                audiolib_att_loop_rel_stop(Xp_tally_alr, 100);
                Xp_tally_alr = -1;
            }
        }
        // Magic number: 999999, the most the tally shows.
        if (Current_total_xp >= 0.0f)
        {
            if (Current_total_xp > 999999.0f)
            {
                Current_total_xp = 999999.0f;
            }
        }
        else
        {
            Current_total_xp = 0.0f;
        }
    }
    const unsigned int level = multi_game_xp_calculate_level((int)Current_total_xp);
    const unsigned int last_level = multi_game_xp_calculate_level((int)Last_frame_total_xp);
    const int to_next = multi_game_xp_til_next_level((int)Current_total_xp);
    float progress = 1.0f;
    if ((int)level < multi_game_xp_max_level())
    {
        progress = fl_parameter(Current_total_xp, (float)multi_game_xp_level_threshold((int)level), (float)multi_game_xp_level_threshold((int)level + 1), false);
    }
    if (last_level < level)
    {
        audiolib_cue_play(Ga_cue_ids.unlock_slam, nullptr, nullptr);
    }
    const int bonus = Xp_tally_from_game ? (int)Current_match_bonus_xp : multi_game_xp_get_my_bonus_xp();
    const int xp = Xp_tally_from_game ? (int)Current_match_xp : multi_game_xp_get_my_xp() - bonus;
    vdi->set_element(0, vint_variant(xp));
    vdi->set_element(1, vint_variant(bonus));
    vdi->set_element(2, vint_variant((int)Current_total_xp));
    vdi->set_element(3, vint_variant(to_next));
    vdi->set_element(4, vint_variant((int)level));
    vdi->set_element(5, vint_variant(last_level < level));
    vdi->set_element(6, vint_variant(progress));
}

// 0x008E60B0
// The current unlock ("type - name", locked ones before their level show "locked" and the experience they need), its
// number among them, and the paging buttons' images.
static void xp_tally_process_data_item_unlocks()
{
    vint_data_item* vdi = vint_data_item::find((unsigned int)Xp_tally_menu_info.data_item_handles[2]);
    if (vdi == nullptr)
    {
        return;
    }
    char left_name[64];
    char right_name[64];
    char left_image[128];
    char right_image[128];
    controls_get_button_image_name(left_name, sizeof(left_name), (control_button_action)Button_info.binfo[3].m_button_choice, Player_controller_index);
    controls_get_button_image_name(right_name, sizeof(right_name), (control_button_action)Button_info.binfo[4].m_button_choice, Player_controller_index);
    keen::formatString(left_image, sizeof(left_image), "[image:%s]", left_name);
    keen::formatString(right_image, sizeof(right_image), "[image:%s]", right_name);
    const bool locked = !Xp_tally_from_game && (int)multi_game_xp_get_my_level() < Xp_unlocks[Current_unlock_tally].level_required;
    wchar_t description[256];
    wchar_t title[128];
    wchar_t count[128];
    char encoded_count[384];
    char encoded_title[384];
    char encoded_description[768];
    description[0] = L'\0';
    title[0] = L'\0';
    count[0] = L'\0';
    encoded_description[0] = '\0';
    encoded_title[0] = '\0';
    encoded_count[0] = '\0';
    if (Xp_tally_num_unlocks >= 1)
    {
        const multi_xp_unlock& unlock = Xp_unlocks[Current_unlock_tally];
        wchar_t type_text[128];
        wchar_t name_text[128];
        const char* type_tag = nullptr;
        const char* name_tag = nullptr;
        bool colorized = false;
        switch (unlock.unlock_type)
        {
        case MXUT_PLAYLIST:
        {
            const multi_game_playlist* playlist = multi_game_playlist_get_by_id(unlock.data.playlist_id);
            _snprintf_safe(title, 0x80, L"%ls - %ls", localization_get_display_string_req(Playlist_string_tally, nullptr),
                           localization_get_display_string_req(locked ? "MENU_LOCKED" : playlist->display_name, nullptr));
            description[0] = L'\0';
            break;
        }
        case MXUT_BONUS_XP_EVENT:
        {
            const multi_xp_event* event = multi_game_xp_get_event_info(unlock.data.xp_event);
            _snprintf_safe(title, 0x80, L"%ls - %ls", localization_get_display_string_req(Bonus_xp_event_string, nullptr),
                           localization_get_display_string_req(locked ? "MENU_LOCKED" : event->display_name, nullptr));
            if (localization_does_tag_exist(event->description))
            {
                _snprintf_safe(description, 0x100, L"%ls", localization_get_display_string_req(event->description, nullptr));
            }
            else
            {
                wchar_t raw[256];
                copy_to_wide(raw, event->description, 0x100);
                _snprintf_safe(description, 0x100, L"!!%ls", raw);
            }
            break;
        }
        case MXUT_WEAPON:
        {
            const weapon_info* weapon = weapons_get_weapon_info_from_weapon_unique_id(unlock.data.weapon_unique_id);
            if (weapon != nullptr && weapon->weapon_inv_item_info != nullptr)
            {
                type_tag = "MENU_SLEDGEHAMMER";
                name_tag = locked ? "MENU_LOCKED" : weapon->weapon_inv_item_info->display_name;
                colorized = true;
            }
            break;
        }
        case MXUT_CHARACTER:
            type_tag = "MENU_CHARACTER";
            name_tag = locked ? "MENU_LOCKED" : multi_game_character_get_unlock_character_name(unlock.data.character_id);
            colorized = true;
            break;
        case MXUT_BADGE_ICON:
            type_tag = "MENU_BADGE_ICON";
            name_tag = locked ? "MENU_LOCKED" : multi_game_badge_get_xp_unlock_group_name(unlock.data.badge_id);
            colorized = true;
            break;
        case MXUT_BADGE_EMBLEM:
            type_tag = "MENU_BADGE_EMBLEM";
            name_tag = locked ? "MENU_LOCKED" : multi_game_badge_get_xp_unlock_group_name(unlock.data.badge_id);
            colorized = true;
            break;
        case MXUT_BADGE_COLOR_SCHEME:
            type_tag = "MENU_BADGE_COLOR_SCHEME";
            name_tag = locked ? "MENU_LOCKED" : multi_game_badge_get_xp_unlock_group_name(unlock.data.badge_id);
            colorized = true;
            break;
        default:
            break;
        }
        if (colorized)
        {
            // Original: the red colour is passed but colorize is always false.
            get_colorized_localized_text_tally(type_text, 0x80, type_tag, false, &color::RED);
            get_colorized_localized_text_tally(name_text, 0x80, name_tag, false, &color::RED);
            _snprintf_safe(title, 0x80, L"%ls - %ls", type_text, name_text);
            _snprintf_safe(description, 0x100, L"");
        }
        if (locked)
        {
            _snprintf_safe(description, 0x100, localization_get_display_string_req("MENU_UNLOCKED_AT", nullptr), multi_game_xp_get_xp_for_level(unlock.level_required));
        }
        _snprintf_safe(count, 0x80, L"%d/%d", Current_unlock_tally + 1, Xp_tally_num_unlocks);
    }
    vint_encode_wide_text(encoded_count, sizeof(encoded_count), count);
    vint_encode_wide_text(encoded_title, sizeof(encoded_title), title);
    vint_encode_wide_text(encoded_description, sizeof(encoded_description), description);
    vdi->set_element(0, vint_variant(Unlocks_noun_string));
    vdi->set_element(1, vint_variant(Xp_tally_num_unlocks < 1 ? None_string_tally : (const char*)encoded_count));
    vdi->set_element(2, vint_variant(Xp_tally_num_unlocks < 1 ? (const char*)nullptr : (const char*)encoded_title));
    vdi->set_element(3, vint_variant(Xp_tally_num_unlocks < 1 ? (const char*)nullptr : (const char*)encoded_description));
    vdi->set_element(4, vint_variant(Xp_tally_num_unlocks < 2 ? (const char*)nullptr : (const char*)left_image));
    vdi->set_element(5, vint_variant(Xp_tally_num_unlocks < 2 ? (const char*)nullptr : (const char*)right_image));
}

// 0x0088F310
// Continue (select) after a match, else back.
static void xp_tally_process_data_item_hints()
{
    vint_data_item* vdi = vint_data_item::find((unsigned int)Xp_tally_menu_info.data_item_handles[3]);
    multi_player_menu_system_clear_hint_config(xp_tally_hint_config, 4, xp_tally_num_hints);
    if (vdi == nullptr)
    {
        return;
    }
    char image[32];
    if (Xp_tally_from_game)
    {
        controls_get_button_image_name(image, sizeof(image), CBA_MENU_SELECT, Player_controller_index);
        multi_player_menu_system_add_hint_config(xp_tally_num_hints, xp_tally_hint_config, XPT_HINT_SELECT);
        vdi->set_element(0, vint_variant((const char*)image));
        vdi->set_element(1, vint_variant(Continue_string));
    }
    else
    {
        controls_get_button_image_name(image, sizeof(image), CBA_MENU_ABORT, Player_controller_index);
        multi_player_menu_system_add_hint_config(xp_tally_num_hints, xp_tally_hint_config, XPT_HINT_ABORT);
        vdi->set_element(0, vint_variant((const char*)image));
        vdi->set_element(1, vint_variant(Back_string_tally));
    }
}

// 0x0088F450
// The hidden challenges' icons (a lock until achieved), 16 at most.
static void xp_tally_process_data_item_challenges()
{
    vint_data_item* vdi = vint_data_item::find((unsigned int)Xp_tally_menu_info.data_item_handles[4]);
    if (vdi == nullptr)
    {
        return;
    }
    for (int i = 0; i < stats_get_num_hidden_challenges(); i++)
    {
        // Magic number: 16 challenge elements.
        if (i > 15)
        {
            return;
        }
        const hidden_challenge* challenge = stats_get_hidden_challenge(i);
        vdi->set_element(i, vint_variant(challenge->has_been_achieved ? (const char*)challenge->icon_name : "ui_fend_icon_lock"));
    }
}

// 0x008C2AC0
// An event row: {index, name, count, even row, scroll position}.
static void xp_tally_add_event()
{
    vint_data_group* group = vint_data_group::find((unsigned int)Xp_tally_menu_info.events_data_group_handle);
    if (group == nullptr || Xp_tally_menu_info.events_data_item_handles.size() >= xp_tally_visible_events())
    {
        return;
    }
    vint_data_item* item = group->add_data_item(nullptr);
    if (item == nullptr)
    {
        return;
    }
    vint_variant_type types[5] = {VINT_PROP_TYPE_INT, VINT_PROP_TYPE_STRING, VINT_PROP_TYPE_INT, VINT_PROP_TYPE_BOOL, VINT_PROP_TYPE_FLOAT};
    item->set_template(types, 5);
    Xp_tally_menu_info.events_data_item_handles.add((int)item->get_handle());
}

// 0x008AF580
static void xp_tally_remove_event()
{
    vint_data_group* group = vint_data_group::find((unsigned int)Xp_tally_menu_info.events_data_group_handle);
    const int last = Xp_tally_menu_info.events_data_item_handles.size() - 1;
    group->remove_data_item((unsigned int)Xp_tally_menu_info.events_data_item_handles[last]);
    Xp_tally_menu_info.events_data_item_handles.remove(last);
}

// 0x008C2B70
// The visible events from the scroll offset (row parity from the event index; the scroll bar position from 0 to 0.95).
static void xp_tally_process_events_list()
{
    const int first = Xp_event_index_offset;
    int last = xp_tally_visible_events() - 1 + first;
    if (last > Num_my_xp_events - 1)
    {
        last = Num_my_xp_events - 1;
    }
    const int rows = last - first + 1;
    vint_data_group* group = vint_data_group::find((unsigned int)Xp_tally_menu_info.events_data_group_handle);
    while ((int)group->get_num_data_items() < rows)
    {
        xp_tally_add_event();
    }
    for (int row = 0; row < rows; row++)
    {
        vint_data_item* item = vint_data_item::find((unsigned int)Xp_tally_menu_info.events_data_item_handles[row]);
        if (item == nullptr)
        {
            continue;
        }
        const int index = first + row;
        const xp_info_entry& entry = My_xp_events[index];
        item->set_element(0, vint_variant(index + 1));
        item->set_element(1, vint_variant(entry.xp_event->display_name));
        item->set_element(2, vint_variant(entry.count_of_event));
        item->set_element(3, vint_variant((index % 2) == 0));
        float position = 0.0f;
        if (xp_tally_visible_events() < Num_my_xp_events)
        {
            // Magic number: the bar runs to 95%.
            position = ((float)Xp_event_index_offset / (float)(Num_my_xp_events - xp_tally_visible_events())) * 0.95f;
        }
        item->set_element(4, vint_variant(position));
    }
    while (rows < (int)group->get_num_data_items())
    {
        xp_tally_remove_event();
    }
}

// 0x008E67B0
static void xp_tally_process_data_items()
{
    xp_tally_process_data_item_labels();
    xp_tally_process_data_item_values();
    xp_tally_process_data_item_unlocks();
    xp_tally_process_data_item_hints();
    xp_tally_process_data_item_challenges();
    xp_tally_process_events_list();
}

// 0x008F3A30
static void xp_tally_process_loading()
{
    switch (ui_get_doc_asset_state(Xp_tally_menu_info.doc_handle))
    {
    case ST_LOADING:
        return;
    case ST_LOADED:
        Xp_tally_menu_info.pending_state = MMS_XT_LOADED;
        return;
    case ST_UNLOADED:
        keen::debug::nativePrintArgs("Int3 critical function at %s(%d)\n", __FILE__, __LINE__);
        // fall through
    case ST_ERROR:
        // Magic number: a 0.5 s fade out.
        ui_unload_document(Xp_tally_menu_info.doc_handle, DFOT_NONE, 0.5f);
        Xp_tally_menu_info.doc_handle = -1;
        Xp_tally_menu_info.pending_state = MMS_XT_UNLOADING;
        return;
    default:
        keen::debug::nativePrintArgs("Int3 critical function at %s(%d)\n", __FILE__, __LINE__);
        return;
    }
}

// 0x008D5E70 (:939)
// The wheel scrolls the events; dragging on the scroll bar sets the offset.
void xp_tally_process_scroll_bar()
{
    if (!mouse_just_pressed(0) && !mouse_just_released(0) && !mouse_is_pressed(0) && !mouse_just_scrolled_wheel())
    {
        return;
    }
    if (xp_tally_visible_events() >= Num_my_xp_events)
    {
        return;
    }
    const int range = Num_my_xp_events - xp_tally_visible_events();
    int dx;
    int dy;
    int wheel;
    mouse_get_deltas(dx, dy, wheel);
    if (wheel > 0)
    {
        if (Xp_event_index_offset > 0)
        {
            audiolib_cue_play(Audio_info[1], nullptr, nullptr);
            Xp_event_index_offset--;
            return;
        }
    }
    else if (wheel < 0 && Xp_event_index_offset < range)
    {
        audiolib_cue_play(Audio_info[0], nullptr, nullptr);
        Xp_event_index_offset++;
        return;
    }
    vint_doc_info* info = ui_get_doc_info(Xp_tally_menu_info.doc_handle);
    if (info == nullptr || info->m_doc == nullptr)
    {
        return;
    }
    vint_object_base* bar = vint_object_base::find_object(kScrollBarOutline, info->m_doc);
    if (bar == nullptr || !mouse_is_pressed(0) || !ui_is_mouse_over_vint_object(bar))
    {
        return;
    }
    float x = 0.0f;
    float y = 0.0f;
    float height = 0.0f;
    ui_get_vint_obj_position(bar, x, y);
    ui_get_vint_obj_size(bar, x, height);
    // Rounded to nearest by flooring +0.5 (the SSE2 floor of rfg.exe).
    const int offset = (int)floor((double)(((Mouse_scaled_y - y) / height) * (float)range + 0.5f));
    if (offset != Xp_event_index_offset)
    {
        audiolib_cue_play(Audio_info[1], nullptr, nullptr);
        Xp_event_index_offset = offset;
    }
}

// 0x008D60B0
// With the focus on top: the statistics' mouse hints, the scroll bar, the unlock paging hints (half opacity unless
// hovered, a click pages).
static void xp_tally_process_mouse_input(bool& events_consumed)
{
    const int top = Current_input_lock_id != -1 ? Current_input_lock_id : (Input_types.size() - 1 < 0 ? 1 : Input_types[Input_types.size() - 1].id);
    if (Xp_tally_menu_info.input_id != top || gameseq_get_state() != GS_MULTIPLAYER_XP_TALLY)
    {
        return;
    }
    Current_mp_stats_choice = stats_internal_process_mouse(Xp_tally_menu_info.input_id);
    xp_tally_process_scroll_bar();
    vint_doc_info* info = ui_get_doc_info(Xp_tally_menu_info.doc_handle);
    if (info == nullptr || info->m_doc == nullptr)
    {
        return;
    }
    vint_object_base* left = vint_object_base::find_object("xp_tally_unlock_left_hint", info->m_doc);
    vint_object_base* right = vint_object_base::find_object("xp_tally_unlock_right_hint", info->m_doc);
    if (left == nullptr || right == nullptr)
    {
        return;
    }
    // Magic numbers: half opacity, full when hovered.
    ui_pc_set_object_opacity(left, 0.5f);
    ui_pc_set_object_opacity(right, 0.5f);
    if (ui_is_mouse_over_vint_object(left))
    {
        ui_pc_set_object_opacity(left, 1.0f);
        if (mouse_just_released(0))
        {
            Current_mouse_hint_choice_tally = XPT_HINT_LEFT;
        }
    }
    else if (ui_is_mouse_over_vint_object(right))
    {
        ui_pc_set_object_opacity(right, 1.0f);
        if (mouse_just_released(0))
        {
            Current_mouse_hint_choice_tally = XPT_HINT_RIGHT;
        }
    }
}

// A Button_info button pressed (and not waiting for its release) with the tally's input focus.
static bool xp_tally_button_pressed(int button)
{
    const int lock = Current_input_lock_id != -1 ? Current_input_lock_id : 0;
    bool focus = Xp_tally_menu_info.input_id == -1 && lock == 0;
    if (!focus)
    {
        const int top = Current_input_lock_id != -1 ? Current_input_lock_id : (Input_types.size() - 1 < 0 ? 1 : Input_types[Input_types.size() - 1].id);
        focus = Xp_tally_menu_info.input_id == top;
    }
    return focus && !Button_info.binfo[button].m_button_wait_for_up && Button_info.binfo[button].m_button_input > 0.0f;
}

// 0x008D6200
// Matchmaking: the next game (button 14, or the auto continue) deletes the session and leaves (s_nextGame pops the
// results too).
static void xp_tally_process_input()
{
    bool next_game = xp_tally_button_pressed(0xE);
    g_gameLinkMutex.lock(0);
    kaiko::gamelink::GameLink* link = get_game_link_session();
    if (!Lobby_session->get_info()->systemlink_or_lan && !Lobby_session->get_info()->party_lobby
        && (next_game || (Auto_continue_timestamp.valid() && Auto_continue_timestamp.elapsed()))
        && (multi_game_party_am_i_leader(false) || !link->isPartyEnabled()))
    {
        s_nextGame = true;
        Lobby_session->session_delete();
        Xp_tally_menu_info.exit_action = MMEA_XT_POP;
        Xp_tally_menu_info.pending_state = MMS_XT_UNLOADING;
        audiolib_cue_play(Audio_info[6], nullptr, nullptr);
    }
    Current_mouse_hint_choice_tally = XPT_HINT_INVALID;
    Current_mp_stats_choice = MPS_HINT_INVALID;
    bool events_consumed = false;
    xp_tally_process_mouse_input(events_consumed);
    g_gameLinkMutex.unlock();
}

// 0x008FC9B0
// Buttons (Button_info indices): 14 continue after a matchmaking match, 11 back (out of the results or the
// statistics), 3/4 the unlocks, 0x11/0x13 the results' or statistics' pages, 2/1 scroll the events.
static void xp_tally_process_loaded()
{
    xp_tally_process_data_items();
    xp_tally_process_input();
    if (Xp_tally_from_game && xp_tally_button_pressed(0xE) && !Lobby_session->get_info()->systemlink_or_lan && !Lobby_session->get_info()->party_lobby
        && multi_game_party_am_i_leader(false))
    {
        ui_document_mp_results_ui_pop(false);
        audiolib_cue_play(Audio_info[4], nullptr, nullptr);
        return;
    }
    const int input_id = Xp_tally_menu_info.input_id;
    const mp_stats_hint_choices stats_choice = Current_mp_stats_choice;
    // Test seam (UI.md 2.7 item 2, off by default): the command line's results time ran out (as back).
    const bool seam_back = Xp_tally_from_game && g_openrfgResultsContinue != nullptr && g_openrfgResultsContinue();
    if (!xp_tally_button_pressed(0xB) && stats_choice != MPS_HINT_ABORT && !seam_back)
    {
        if (ui_get_button_info(3, input_id) > 0.0f || stats_choice == MPS_HINT_CYCLE_UNLOCKS_LEFT || Current_mouse_hint_choice_tally == XPT_HINT_LEFT)
        {
            int unlock = Current_unlock_tally - 1;
            if (unlock < 0)
            {
                unlock = Xp_tally_num_unlocks - 1;
            }
            const bool changed = unlock != Current_unlock_tally;
            Current_unlock_tally = unlock;
            if (changed)
            {
                audiolib_cue_play(Audio_info[7], nullptr, nullptr);
            }
            return;
        }
        if (ui_get_button_info(4, input_id) > 0.0f || stats_choice == MPS_HINT_CYCLE_UNLOCKS_RIGHT || Current_mouse_hint_choice_tally == XPT_HINT_RIGHT)
        {
            const int previous = Current_unlock_tally;
            Current_unlock_tally++;
            if (Current_unlock_tally >= Xp_tally_num_unlocks)
            {
                Current_unlock_tally = 0;
            }
            if (Current_unlock_tally != previous)
            {
                audiolib_cue_play(Audio_info[8], nullptr, nullptr);
            }
            return;
        }
        if (ui_get_button_info(0x11, input_id) > 0.0f || stats_choice == MPS_HINT_CYCLE_LEADERBOARD_LEFT)
        {
            if (!Xp_tally_from_game)
            {
                ui_document_mp_stats_prev_page();
                audiolib_cue_play(Audio_info[9], nullptr, nullptr);
            }
            else if (Xp_tally_connected_when_entered)
            {
                ui_document_mp_results_prev_page();
                audiolib_cue_play(Audio_info[9], nullptr, nullptr);
            }
            return;
        }
        if (ui_get_button_info(0x13, input_id) > 0.0f || stats_choice == MPS_HINT_CYCLE_LEADERBOARD_RIGHT)
        {
            if (!Xp_tally_from_game)
            {
                ui_document_mp_stats_next_page();
                audiolib_cue_play(Audio_info[10], nullptr, nullptr);
            }
            else if (Xp_tally_connected_when_entered)
            {
                ui_document_mp_results_next_page();
                audiolib_cue_play(Audio_info[10], nullptr, nullptr);
            }
            return;
        }
        if (ui_get_button_info(2, input_id) > 0.0f)
        {
            // Down: the next event, else back to the top.
            if (Xp_event_index_offset + xp_tally_visible_events() < Num_my_xp_events)
            {
                Xp_event_index_offset++;
                audiolib_cue_play(Audio_info[0], nullptr, nullptr);
                return;
            }
            if (xp_tally_visible_events() < Num_my_xp_events)
            {
                Xp_event_index_offset = 0;
                return;
            }
            audiolib_cue_play(Audio_info[11], nullptr, nullptr);
            return;
        }
        if (ui_get_button_info(1, input_id) > 0.0f)
        {
            // Up: the previous event, else to the bottom.
            if (Xp_event_index_offset > 0)
            {
                Xp_event_index_offset--;
                audiolib_cue_play(Audio_info[1], nullptr, nullptr);
                return;
            }
            if (Num_my_xp_events <= xp_tally_visible_events())
            {
                audiolib_cue_play(Audio_info[11], nullptr, nullptr);
                return;
            }
            Xp_event_index_offset = Num_my_xp_events - xp_tally_visible_events();
            return;
        }
        if (ui_get_button_info(0xB, input_id) > 0.0f)
        {
            if (!Xp_tally_from_game)
            {
                Mp_match_stats_info.exit_action = MMEA_ST_POP;
                Mp_match_stats_info.pending_state = MMS_ST_UNLOADING;
            }
            else if (!multi_game_party_in_party())
            {
                ui_document_mp_results_ui_pop(true);
            }
            else
            {
                multi_player_menu_system_pull_up_leave_confirm_message_box();
            }
            audiolib_cue_play(Audio_info[4], nullptr, nullptr);
            return;
        }
        if (Pending_pop)
        {
            ui_document_mp_results_ui_pop(true);
        }
        return;
    }
    // Back (button 11 pressed, or the statistics' back hint).
    if (!Xp_tally_from_game)
    {
        Mp_match_stats_info.exit_action = MMEA_ST_POP;
        Mp_match_stats_info.pending_state = MMS_ST_UNLOADING;
    }
    else if (!multi_game_party_in_party() || multi_game_party_am_i_leader(false))
    {
        Pending_pop = false;
        No_matchmaking = true;
        Mp_match_results_info.exit_action = MMEA_R_POP;
        Mp_match_results_info.pending_state = MMS_R_EXITING;
    }
    else
    {
        multi_player_menu_system_pull_up_leave_confirm_message_box();
    }
    audiolib_cue_play(Audio_info[4], nullptr, nullptr);
}

// 0x0088F4E0
// LTCG: new_state in EAX.
static void xp_tally_ui_push(game_state new_state)
{
    Xp_tally_menu_info.exit_action = MMEA_XT_PUSH;
    Xp_tally_menu_info.exit_push_target = new_state;
    Xp_tally_menu_info.pending_state = MMS_XT_UNLOADING;
}

// 0x0088F500
static void xp_tally_ui_pop()
{
    Xp_tally_menu_info.exit_action = MMEA_XT_POP;
    Xp_tally_menu_info.pending_state = MMS_XT_UNLOADING;
}

// 0x008E67D0
// qsort order: plain events before bonus ones, then by count (most first), then by localized name.
static int xp_event_compare(const void* p1, const void* p2)
{
    const xp_info_entry* a = (const xp_info_entry*)p1;
    const xp_info_entry* b = (const xp_info_entry*)p2;
    if (!a->xp_event->bonus && b->xp_event->bonus)
    {
        return -1;
    }
    if (a->xp_event->bonus && !b->xp_event->bonus)
    {
        return 1;
    }
    if (b->count_of_event < a->count_of_event)
    {
        return -1;
    }
    if (b->count_of_event > a->count_of_event)
    {
        return 1;
    }
    const wchar_t* name_a = L"ZZZ1";
    const wchar_t* name_b = L"ZZZ2";
    if (localization_does_tag_exist(a->xp_event->display_name))
    {
        name_a = localization_get_display_string_req(a->xp_event->display_name, nullptr);
    }
    if (localization_does_tag_exist(b->xp_event->display_name))
    {
        name_b = localization_get_display_string_req(b->xp_event->display_name, nullptr);
    }
    for (;; name_a++, name_b++)
    {
        if (*name_a != *name_b)
        {
            return (unsigned short)*name_a < (unsigned short)*name_b ? -1 : 1;
        }
        if (*name_a == L'\0')
        {
            return 0;
        }
    }
}

// ---- state callbacks (file statics in rfg.exe, external since UI5)

// 0x0090EA30 (:1254)
// From a match: its experience (and the total before it); from the live or statistics menu: the lifetime figures. The
// unlocks of the levels gained (or all), the bonus experience events (the match's, or the lifetime stats), sorted.
void ui_document_mp_xp_tally_enter(game_state prev_state)
{
    xp_tally_init_state();
    Xp_tally_menu_info.exit_action = MMEA_XT_POP;
    Xp_tally_menu_info.exit_push_target = GS_MULTIPLAYER_LIVE;
    Xp_tally_menu_selection = 0;
    Xp_event_index_offset = 0;
    xp_tally_init_info();
    Xp_tally_menu_info.doc_handle = ui_load_document(Ui_mp_shell_name_tally, DFIT_NONE, 0.0f, DLT_NORMAL, false, STREAM_PRIORITY_AUDIO_CACHE_LOAD, false);
    Xp_tally_menu_info.control_id = ui_request_control_id();
    Xp_tally_menu_info.input_id = ui_register_for_processed_input(IRT_UNLIMITED_INPUT, false);
    Xp_tally_from_game = !(prev_state == GS_MULTIPLAYER_LIVE || prev_state == GS_MULTIPLAYER_STATS);
    const multi_player_stats* stats = multi_game_stats_get_my_stats();
    int total;
    if (stats == nullptr || !Xp_tally_from_game)
    {
        Current_match_xp = 0.0f;
        Current_match_bonus_xp = 0.0f;
        total = multi_game_xp_get_my_xp();
    }
    else
    {
        Current_match_xp = (float)stats->xp;
        Current_match_bonus_xp = (float)stats->bonus_xp;
        total = multi_game_xp_get_my_xp() - (int)stats->bonus_xp - (int)stats->xp;
    }
    Current_total_xp = (float)total;
    Final_xp = multi_game_xp_get_my_xp();
    // Magic number: 999999, the most the tally shows.
    if (Current_total_xp < 0.0f)
    {
        Current_total_xp = 0.0f;
    }
    else if (Current_total_xp > 999999.0f)
    {
        Current_total_xp = 999999.0f;
    }
    if (Final_xp < 0)
    {
        Final_xp = 0;
    }
    else if (Final_xp > 999999)
    {
        Final_xp = 999999;
    }
    if (!Xp_tally_from_game)
    {
        Xp_tally_connected_when_entered = false;
    }
    Last_frame_total_xp = Current_total_xp;
    const unsigned int start_level = multi_game_xp_calculate_level((int)Current_total_xp);
    const unsigned int end_level = multi_game_xp_calculate_level(Final_xp);
    if (Xp_tally_from_game)
    {
        Xp_unlocks = multi_game_xp_get_unlocks_for_level_range(Xp_tally_num_unlocks, (int)start_level + 1, (int)end_level);
    }
    else
    {
        Xp_unlocks = multi_game_xp_get_all_unlocks(Xp_tally_num_unlocks);
    }
    Current_unlock_tally = 0;
    Num_my_xp_events = 0;
    // Magic number: 84 experience events.
    for (int event = MXE_KILL; event < 0x54; event++)
    {
        const multi_xp_event* info = multi_game_xp_get_event_info((multi_xp_events)event);
        if (info->minimum_level > (int)multi_game_xp_get_my_level() || (info->bagman_locked && !Bagman_dlc_unlocked))
        {
            continue;
        }
        if (!Xp_tally_from_game)
        {
            My_xp_events[Num_my_xp_events].xp_event = info;
            // Magic number: the lifetime event counters start at stat 0x7B.
            My_xp_events[Num_my_xp_events].count_of_event = stats_get_stat_value((tracked_stats_indices)(event + 0x7B));
            Num_my_xp_events++;
        }
        else if (multi_game_stats_get_my_stats()->xp_event_frequency[event] != 0)
        {
            My_xp_events[Num_my_xp_events].xp_event = info;
            My_xp_events[Num_my_xp_events].count_of_event = (int)multi_game_stats_get_my_stats()->xp_event_frequency[event];
            Num_my_xp_events++;
        }
    }
    qsort(My_xp_events, (size_t)Num_my_xp_events, sizeof(xp_info_entry), xp_event_compare);
    // Magic number: 2.5 s before the count starts.
    Xp_countdown_delay.set(2500);
    Xp_tally_alr = -1;
    s_nextGame = false;
}

// 0x008F3AD0 (:1342)
void ui_document_mp_xp_tally_exit(game_state new_state)
{
    xp_tally_unload_ui_information();
    // Magic number: a 0.5 s fade out.
    ui_unload_document(Xp_tally_menu_info.doc_handle, DFOT_NONE, 0.5f);
    ui_relinquish_control_id(Xp_tally_menu_info.control_id);
    ui_relinquish_input_id(Xp_tally_menu_info.input_id);
}

// 0x009158C0 (:1356)
// Only while on top (pushed under the results by the state stack restore it waits); its input id comes back if it
// had none.
void ui_document_mp_xp_tally_process()
{
    if (Bink_info_main.handle != 0)
    {
        mm_process_bink_video();
    }
    if (gameseq_get_state() != GS_MULTIPLAYER_XP_TALLY)
    {
        return;
    }
    if (Xp_tally_menu_info.input_id == -1)
    {
        Xp_tally_menu_info.input_id = ui_register_for_processed_input(IRT_UNLIMITED_INPUT, false);
    }
    multi_player_menu_system_process();
    ui_add_for_processing(Xp_tally_menu_info.doc_handle);
    Xp_tally_menu_info.processed_state = Xp_tally_menu_info.state;
    if (Xp_tally_menu_info.state == MMS_XT_LOADING)
    {
        xp_tally_process_loading();
    }
    else if (Xp_tally_menu_info.state == MMS_XT_LOADED)
    {
        xp_tally_process_loaded();
    }
    else if (Xp_tally_menu_info.state == MMS_XT_UNLOADED)
    {
        xp_tally_set_state(MMS_XT_LOADING);
    }
    ui_process(Xp_tally_menu_info.control_id, false);
}

// 0x008F3B70 (:1401)
void ui_document_mp_xp_tally_handshake(rl_renderer*)
{
    if (Xp_tally_menu_info.pending_state != Xp_tally_menu_info.state)
    {
        xp_tally_do_state_change();
    }
}

// 0x008FCEB0 (:1408)
// Not drawn while the results are being entered (their first frame).
void ui_document_mp_xp_tally_render(rl_renderer*)
{
    if (Xp_tally_menu_info.state == MMS_XT_LOADED && Xp_tally_menu_info.processed_state == MMS_XT_LOADED && Xp_tally_menu_info.doc_handle != -1 && Xp_tally_menu_info.control_id != 0)
    {
        ui_add_for_rendering(Xp_tally_menu_info.doc_handle);
    }
    if (!Entering_results)
    {
        ui_render(Xp_tally_menu_info.control_id, false);
    }
}

// 0x009159B0 (:1435)
void ui_document_mp_xp_tally_init()
{
    state_info info;
    info.enter = ui_document_mp_xp_tally_enter;
    info.exit = ui_document_mp_xp_tally_exit;
    info.process = ui_document_mp_xp_tally_process;
    info.handshake = ui_document_mp_xp_tally_handshake;
    info.render = ui_document_mp_xp_tally_render;
    info.transparent = false;
    info.pause_beneath = false;
    gameseq_register_state(GS_MULTIPLAYER_XP_TALLY, info);
}
