// Original: volition/rfg/code/interface/ui_document_hud_wrecking_crew.cpp
//
// The wrecking crew HUD (wrecking.md WC3): three documents, the in-turn HUD (rfg_ui_mp_wc_hud: header, timer, players,
// score, weapon and backpack, stamp, message, counter, stress break), the splash screen between turns
// (rfg_ui_mp_wc_splash: title, how to play, turn end scores, Fall Guy barrels, players, the veto screen) and the
// spectator hint bar (rfg_ui_mp_hint_bar). Every item reads its text from the wrecking crew core's getters.
//
// The splash and hint bar documents are the multiplayer HUD's (ui_document_hud_multiplayer.cpp loads
// rfg_ui_mp_wc_splash too, for the how-to-play screen); here they get their own handles and data items.
//
// LTCG: wc_hud_handle_text_set, wc_hud_handle_player_list and handle_barrel_row take the data item in EDI and the
// element counter in ESI, and wc_hud_get_player_state its index in EDI; written with rfg.pdb's signatures.

#include "volition/rfg/code/interface/ui_document_hud_wrecking_crew.h"

#include "volition/audio_lib/code/audiolib_api.h"
#include "volition/interface_lib/code/vint_data_item.h"
#include "volition/interface_lib/code/vint_external.h"
#include "volition/interface_lib/code/vint_variant.h"
#include "volition/rfg/code/gameplay/gameplay.h"
#include "volition/rfg/code/gameplay/wrecking_crew/wrecking_crew.h"
#include "volition/rfg/code/gamewide/game_clock.h"
#include "volition/rfg/code/interface/localization/localization_shared.h"
#include "volition/rfg/code/interface/ui_document_manager.h"
#include "volition/rfg/code/multi/multi_game/multi_game_camera.h"
#include "volition/rfg/code/multi/multi_game/multi_game_object_backpack.h"
#include "volition/rfg/code/objects/inventory_item.h"
#include "volition/rfg/code/weapons/weapons.h"
#include "volition/stream2/code/stream2.h"
#include "volition/vlib/code/util/string_util.h"
#include "volition/vlib/code/util/unicode_tools.h"

#include <stdio.h>
#include <string.h>
#include <wchar.h>


// ---- file-private types (rfg.pdb)
// size 0x14
struct wc_hud_doc_info
{
    const char* name; // 0x00
    void (*init)(int); // 0x04
    void (*process)(int); // 0x08
    bool (*should_render)(); // 0x0C
    void (*shutdown)(int); // 0x10
};
static_assert(sizeof(wc_hud_doc_info) == 0x14, "wc_hud_doc_info");

// size 0x10
struct wc_hud_data_item_info
{
    const char* name; // 0x00
    hud_wrecking_crew_docs doc; // 0x04
    void (*init)(vint_data_item*); // 0x08
    void (*process)(vint_data_item*); // 0x0C
};
static_assert(sizeof(wc_hud_data_item_info) == 0x10, "wc_hud_data_item_info");

static void wc_hud_doc_main_init(int doc_handle);
static bool wc_hud_doc_main_should_render();
static void wc_hud_item_header_init(vint_data_item* item);
static void wc_hud_item_header_process(vint_data_item* item);
static void wc_hud_item_timer_init(vint_data_item* item);
static void wc_hud_item_timer_process(vint_data_item* item);
static void wc_hud_item_players_init(vint_data_item* item);
static void wc_hud_item_players_process(vint_data_item* item);
static void wc_hud_item_score_init(vint_data_item* item);
static void wc_hud_item_score_process(vint_data_item* item);
static void wc_hud_item_weapons_init(vint_data_item* item);
static void wc_hud_item_weapons_process(vint_data_item* item);
static void wc_hud_item_stamp_init(vint_data_item* item);
static void wc_hud_item_message_init(vint_data_item* item);
static void wc_hud_item_counter_init(vint_data_item* item);
static void wc_hud_item_stress_init(vint_data_item* item);
static void wc_hud_item_stress_process(vint_data_item* item);
static void wc_hud_doc_splash_init(int doc_handle);
static bool wc_hud_doc_splash_should_render();
static void wc_hud_item_splash_title_init(vint_data_item* item);
static void wc_hud_item_splash_title_process(vint_data_item* item);
static void wc_hud_item_splash_info_init(vint_data_item* item);
static void wc_hud_item_splash_info_process(vint_data_item* item);
static void wc_hud_item_splash_turn_end_init(vint_data_item* item);
static void wc_hud_item_splash_turn_end_process(vint_data_item* item);
static void wc_hud_item_splash_hints_init(vint_data_item* item);
static void wc_hud_item_splash_hints_process(vint_data_item* item);
static void wc_hud_item_splash_barrels_init(vint_data_item* item);
static void wc_hud_item_splash_barrels_process(vint_data_item* item);
static void wc_hud_item_splash_players_init(vint_data_item* item);
static void wc_hud_item_splash_players_process(vint_data_item* item);
static void wc_hud_item_splash_veto_init(vint_data_item* item);
static void wc_hud_item_splash_veto_process(vint_data_item* item);
static bool wc_hud_doc_hints_should_render();
static void wc_hud_item_hints_init(vint_data_item* item);
static void wc_hud_item_hints_process(vint_data_item* item);

// ---- file-static globals
// 0x01301120, one entry per hud_wrecking_crew_data_items.
static const wc_hud_data_item_info Wrecking_crew_data_item_info[NUM_HUD_WRECKING_CREW_DATA_ITEMS] = {
    {"wrecking_crew_hud_header", HWCD_MAIN, wc_hud_item_header_init, wc_hud_item_header_process},
    {"wrecking_crew_hud_timer", HWCD_MAIN, wc_hud_item_timer_init, wc_hud_item_timer_process},
    {"wrecking_crew_hud_players", HWCD_MAIN, wc_hud_item_players_init, wc_hud_item_players_process},
    {"wrecking_crew_hud_score", HWCD_MAIN, wc_hud_item_score_init, wc_hud_item_score_process},
    {"wrecking_crew_hud_weapons", HWCD_MAIN, wc_hud_item_weapons_init, wc_hud_item_weapons_process},
    {"wrecking_crew_hud_stamp", HWCD_MAIN, wc_hud_item_stamp_init, nullptr},
    {"wrecking_crew_hud_message", HWCD_MAIN, wc_hud_item_message_init, nullptr},
    {"wrecking_crew_hud_counter", HWCD_MAIN, wc_hud_item_counter_init, nullptr},
    {"wrecking_crew_hud_stress", HWCD_MAIN, wc_hud_item_stress_init, wc_hud_item_stress_process},
    {"mp_wc_splash_title", HWCD_SPLASH, wc_hud_item_splash_title_init, wc_hud_item_splash_title_process},
    {"mp_wc_splash_info", HWCD_SPLASH, wc_hud_item_splash_info_init, wc_hud_item_splash_info_process},
    {"mp_wc_splash_turn_end", HWCD_SPLASH, wc_hud_item_splash_turn_end_init, wc_hud_item_splash_turn_end_process},
    {"mp_wc_splash_hints", HWCD_SPLASH, wc_hud_item_splash_hints_init, wc_hud_item_splash_hints_process},
    {"mp_wc_splash_barrels", HWCD_SPLASH, wc_hud_item_splash_barrels_init, wc_hud_item_splash_barrels_process},
    {"mp_wc_splash_players", HWCD_SPLASH, wc_hud_item_splash_players_init, wc_hud_item_splash_players_process},
    {"mp_wc_splash_veto", HWCD_SPLASH, wc_hud_item_splash_veto_init, wc_hud_item_splash_veto_process},
    {"mp_hint_bar_mode_spectator_hints", HWCD_HINTS, wc_hud_item_hints_init, wc_hud_item_hints_process},
};
// 0x01664AF8, one entry per hud_wrecking_crew_docs.
static wc_hud_doc_info Wrecking_crew_doc_info[NUM_HUD_WRECKING_CREW_DOCS] = {
    {"rfg_ui_mp_wc_hud", wc_hud_doc_main_init, nullptr, wc_hud_doc_main_should_render, nullptr},
    {"rfg_ui_mp_wc_splash", wc_hud_doc_splash_init, nullptr, wc_hud_doc_splash_should_render, nullptr},
    {"rfg_ui_mp_hint_bar", nullptr, nullptr, wc_hud_doc_hints_should_render, nullptr},
};
static unsigned int Wrecking_crew_data_item_handles[NUM_HUD_WRECKING_CREW_DATA_ITEMS]; // 0x02BE7D98
static int Wrecking_crew_doc_handles[NUM_HUD_WRECKING_CREW_DOCS]; // 0x02BFD424
static bool Assets_loaded_uiwre; // 0x02C0807D
static bool Active_uiwre; // 0x02C0807E
static bool Data_items_created_uiwre; // 0x02C0807F
static int Last_display_time; // 0x02C08080

// ---- helpers

// 0x00899380. Fills two string elements from a text set (label and text), or two nulls when it is hidden.
static void wc_hud_handle_text_set(vint_data_item* item, int* cur_element, hud_wrecking_crew_text_set* set)
{
    char text[0x60];
    if (set->should_display)
    {
        vint_encode_wide_text(text, 0x60, set->label);
        item->set_element((*cur_element)++, vint_variant((const char*)text));
        vint_encode_wide_text(text, 0x60, set->text);
        item->set_element((*cur_element)++, vint_variant((const char*)text));
    }
    else
    {
        item->set_element((*cur_element)++, vint_variant((const char*)nullptr));
        item->set_element((*cur_element)++, vint_variant((const char*)nullptr));
    }
}

// 0x00899480. No caller in rfg.exe: wc_hud_handle_player_list has it inline.
static hud_wrecking_crew_player_state wc_hud_get_player_state(int player_index)
{
    const wrecking_crew_player_status* status = wrecking_crew_get_player_status(player_index);
    if (status != nullptr)
    {
        if (!status->enabled)
        {
            return HWCPS_DISABLED;
        }
        if (Wrecking_crew_current_player == player_index)
        {
            return HWCPS_ACTIVE;
        }
    }
    return HWCPS_NORMAL;
}

// 0x008994B0. Three elements per player (name, description, state), for all four slots.
static void wc_hud_handle_player_list(vint_data_item* item, int* cur_element)
{
    wchar_t text[0x40];
    char encoded[0xC0];
    for (int i = 0; i < Wrecking_crew_options.num_players; i++)
    {
        wrecking_crew_get_player_name(i, text, 0x40);
        vint_encode_wide_text(encoded, 0xC0, text);
        item->set_element((*cur_element)++, vint_variant((const char*)encoded));
        wrecking_crew_get_player_description(i, text, 0x40);
        vint_encode_wide_text(encoded, 0xC0, text);
        item->set_element((*cur_element)++, vint_variant((const char*)encoded));
        int state = wc_hud_get_player_state(i);
        // Between rounds and at the match end no player is shown as disabled or active.
        if (wrecking_crew_get_state() == WCS_ROUND_END || wrecking_crew_get_state() == WCS_MATCH_END)
        {
            state = HWCPS_NORMAL;
        }
        item->set_element((*cur_element)++, vint_variant(state));
    }
    // Magic number: the HUD has four player slots (Wc_player_infos[4]).
    for (int i = Wrecking_crew_options.num_players; i < 4; i++)
    {
        item->set_element((*cur_element)++, vint_variant((const char*)nullptr));
        item->set_element((*cur_element)++, vint_variant((const char*)nullptr));
        item->set_element((*cur_element)++, vint_variant(0));
    }
}

// ---- main document

// 0x008996E0
static void wc_hud_doc_main_init(int doc_handle)
{
}

// 0x008996F0. The in-turn HUD shows from the turn countdown to the stress break.
static bool wc_hud_doc_main_should_render()
{
    const wrecking_crew_states state = wrecking_crew_get_state();
    return state == WCS_PRE_TURN || state == WCS_PLAYERS_TURN || state == WCS_PRE_BREAK;
}

// 0x00899710
static void wc_hud_item_header_init(vint_data_item* item)
{
    const vint_variant_type types[1] = {VINT_PROP_TYPE_STRING};
    item->set_template(types, 1);
}

// 0x00899730
static void wc_hud_item_header_process(vint_data_item* item)
{
    item->set_element(0, vint_variant(wrecking_crew_get_mode_display_name(Wrecking_crew_options.mode)));
}

// 0x00899770
static void wc_hud_item_timer_init(vint_data_item* item)
{
    const vint_variant_type types[3] = {VINT_PROP_TYPE_STRING, VINT_PROP_TYPE_INT, VINT_PROP_TYPE_INT};
    item->set_template(types, 3);
}

// 0x008997C0. The turn clock (m:ss) with its milliseconds and icon state, and the countdown sounds.
static void wc_hud_item_timer_process(vint_data_item* item)
{
    const wrecking_crew_states state = wrecking_crew_get_state();
    int time_ms = 0;
    // Magic numbers: 500 ms rounds the display to the nearest second; the turn countdown rounds up (999 ms).
    if (state == WCS_PRE_BREAK)
    {
        time_ms = wrecking_crew_get_turn_time_left() + 500;
    }
    else if (state == WCS_PRE_TURN)
    {
        if (Wrecking_crew_timer.valid() && !Wrecking_crew_timer.elapsed())
        {
            time_ms = Wrecking_crew_timer.time_until() + 999;
        }
    }
    else if (!Wrecking_crew_options.free_play)
    {
        if (Wrecking_crew_timer.valid() && !Wrecking_crew_timer.elapsed())
        {
            time_ms = Wrecking_crew_timer.time_until() + 500;
        }
    }
    else
    {
        // Magic number: no time limit shows as 999 minutes (59,940,000 ms) on the bar; the text is "--:--".
        time_ms = 59940000;
    }
    const int minutes = time_ms / 60000;
    const int seconds = (time_ms % 60000) / 1000;
    const int display_time = minutes * 60 + seconds;
    if (Last_display_time != display_time)
    {
        if (state == WCS_PLAYERS_TURN)
        {
            // Original quirk kept: crossing one minute left plays nothing (a one minute warning without a sound).
            if (display_time <= 60 && Last_display_time > 60)
            {
            }
            else if (display_time <= 10 && Last_display_time > 10)
            {
                gameplay_play_sfx(GSFX_TEN_SECONDS_REMAINING);
            }
            else if (minutes == 0 && seconds <= 5)
            {
                gameplay_play_sfx(GSFX_LOW_TIME_TICK);
            }
        }
        else if (state == WCS_PRE_TURN && seconds != 0)
        {
            // rfg.exe passes the state itself: WCS_PRE_TURN and GSFX_COUNTDOWN_TICK are both 6.
            gameplay_play_sfx(GSFX_COUNTDOWN_TICK);
        }
    }
    Last_display_time = display_time;
    char text[0x40];
    if (!Wrecking_crew_options.free_play)
    {
        sprintf(text, "%d:%.2d", minutes, seconds);
    }
    else
    {
        sprintf(text, "--:--");
    }
    item->set_element(0, vint_variant((const char*)text));
    item->set_element(1, vint_variant(time_ms));
    item->set_element(2, vint_variant((int)wrecking_crew_get_time_icon_state()));
}

// 0x008999C0. No template: the players item is defined by the document.
static void wc_hud_item_players_init(vint_data_item* item)
{
}

// 0x008B3D70. Element 0 shows the list, 1 is the current player's name, then the player list from element 2.
static void wc_hud_item_players_process(vint_data_item* item)
{
    if (multi_game_camera_is_running())
    {
        item->set_element(0, vint_variant(false));
        return;
    }
    item->set_element(0, vint_variant(true));
    wchar_t name[0x20];
    char encoded[0x60];
    if (Wrecking_crew_current_player == -1)
    {
        item->set_element(1, vint_variant((const char*)nullptr));
    }
    else
    {
        wrecking_crew_get_player_name(Wrecking_crew_current_player, name, 0x20);
        // Original bug kept: the 0x60-byte buffer is passed as 0x180 bytes (a 32-character name encodes in 0x60).
        vint_encode_wide_text(encoded, 0x180, name);
        item->set_element(1, vint_variant((const char*)encoded));
    }
    int cur_element = 2;
    wc_hud_handle_player_list(item, &cur_element);
}

// 0x008999D0
static void wc_hud_item_score_init(vint_data_item* item)
{
    const vint_variant_type types[4] = {VINT_PROP_TYPE_STRING, VINT_PROP_TYPE_STRING, VINT_PROP_TYPE_STRING, VINT_PROP_TYPE_STRING};
    item->set_template(types, 4);
}

// 0x00899A20
static void wc_hud_item_score_process(vint_data_item* item)
{
    hud_wrecking_crew_text_set score_set;
    hud_wrecking_crew_text_set high_score_set;
    // Original: the green score flag is asked for and not shown (the score item has four elements).
    bool green_score = false;
    wrecking_crew_get_score_text(&score_set, &high_score_set, &green_score);
    int cur_element = 0;
    wc_hud_handle_text_set(item, &cur_element, &score_set);
    wc_hud_handle_text_set(item, &cur_element, &high_score_set);
}

// 0x00899AA0
static void wc_hud_item_weapons_init(vint_data_item* item)
{
    const vint_variant_type types[6] = {VINT_PROP_TYPE_STRING, VINT_PROP_TYPE_STRING, VINT_PROP_TYPE_INT, VINT_PROP_TYPE_STRING, VINT_PROP_TYPE_STRING, VINT_PROP_TYPE_INT};
    item->set_template(types, 6);
}

// 0x00899AF0. Backpack icon, text and icon state, then the weapon's.
static void wc_hud_item_weapons_process(vint_data_item* item)
{
    const hud_wrecking_crew_icon_state weapon_icon_state = wrecking_crew_get_weapon_icon_state();
    const hud_wrecking_crew_icon_state backpack_icon_state = wrecking_crew_get_backpack_icon_state();
    wchar_t text[0x10];
    char buffer[0x40];
    const wrecking_crew_backpack_info* wc_backpack = wrecking_crew_get_current_backpack_info();
    if (wc_backpack == nullptr || multi_game_camera_is_running())
    {
        item->set_element(0, vint_variant((const char*)nullptr));
        item->set_element(1, vint_variant((const char*)nullptr));
    }
    else
    {
        const multi_backpack_info* b_info = multi_game_backpack_get_info(wc_backpack->backpack_type);
        if (b_info == nullptr)
        {
            return;
        }
        bool override_icon = false;
        wrecking_crew_get_backpack_text(text, 0x10, &override_icon, buffer, 0x40);
        item->set_element(0, vint_variant(override_icon ? (const char*)buffer : b_info->icon_name));
        vint_encode_wide_text(buffer, 0x40, text);
        item->set_element(1, vint_variant((const char*)buffer));
    }
    item->set_element(2, vint_variant((int)backpack_icon_state));
    const wrecking_crew_weapon_info* wc_weapon = wrecking_crew_get_current_weapon_info();
    if (wc_weapon == nullptr || multi_game_camera_is_running())
    {
        item->set_element(3, vint_variant((const char*)nullptr));
        item->set_element(4, vint_variant((const char*)nullptr));
    }
    else
    {
        const weapon_info* w_info = weapons_get_weapon_info(wc_weapon->w_info_handle);
        if (w_info == nullptr)
        {
            return;
        }
        bool override_icon = false;
        wrecking_crew_get_weapon_text(text, 0x10, &override_icon, buffer, 0x40);
        item->set_element(3, vint_variant(override_icon ? (const char*)buffer : (const char*)w_info->icon_name));
        vint_encode_wide_text(buffer, 0x40, text);
        item->set_element(4, vint_variant((const char*)buffer));
    }
    item->set_element(5, vint_variant((int)weapon_icon_state));
}

// 0x00899D90. Shown flag and message; written by hud_ui_wrecking_crew_show_stamp.
static void wc_hud_item_stamp_init(vint_data_item* item)
{
    const vint_variant_type types[2] = {VINT_PROP_TYPE_BOOL, VINT_PROP_TYPE_STRING};
    item->set_template(types, 2);
}

// 0x00899DC0. Shown flag, title and label; written by hud_ui_wrecking_crew_show_message.
static void wc_hud_item_message_init(vint_data_item* item)
{
    const vint_variant_type types[3] = {VINT_PROP_TYPE_BOOL, VINT_PROP_TYPE_STRING, VINT_PROP_TYPE_STRING};
    item->set_template(types, 3);
}

// 0x00899E10. Shown flag, text and numeric flag; written by hud_ui_wrecking_crew_show_counter.
static void wc_hud_item_counter_init(vint_data_item* item)
{
    const vint_variant_type types[3] = {VINT_PROP_TYPE_BOOL, VINT_PROP_TYPE_STRING, VINT_PROP_TYPE_BOOL};
    item->set_template(types, 3);
}

// 0x00899E60
static void wc_hud_item_stress_init(vint_data_item* item)
{
    const vint_variant_type types[4] = {VINT_PROP_TYPE_BOOL, VINT_PROP_TYPE_STRING, VINT_PROP_TYPE_STRING, VINT_PROP_TYPE_STRING};
    item->set_template(types, 4);
}

// 0x00899EB0. The stress break banner with its seconds left.
static void wc_hud_item_stress_process(vint_data_item* item)
{
    if (wrecking_crew_get_state() != WCS_PRE_BREAK)
    {
        item->set_element(0, vint_variant(false));
        return;
    }
    item->set_element(0, vint_variant(true));
    item->set_element(1, vint_variant("HM_BREAK"));
    item->set_element(2, vint_variant(""));
    int time_ms = 0;
    if (Wrecking_crew_timer.valid() && !Wrecking_crew_timer.elapsed())
    {
        // Magic number: 500 ms rounds to the nearest second.
        time_ms = Wrecking_crew_timer.time_until() + 500;
    }
    wchar_t text[0x40];
    // Original: only the seconds are shown; the minutes are dropped.
    _snprintf_safe(text, 0x40, "%d", (time_ms % 60000) / 1000);
    char encoded[0xC0];
    vint_encode_wide_text(encoded, 0xC0, text);
    item->set_element(3, vint_variant((const char*)encoded));
}

// ---- splash document

// 0x0089A060
static void wc_hud_doc_splash_init(int doc_handle)
{
}

// 0x0089A070. The splash shows whenever the in-turn HUD does not.
static bool wc_hud_doc_splash_should_render()
{
    const wrecking_crew_states state = wrecking_crew_get_state();
    return state != WCS_PRE_TURN && state != WCS_PLAYERS_TURN && state != WCS_PRE_BREAK;
}

// 0x0089A0A0
static void wc_hud_item_splash_title_init(vint_data_item* item)
{
    const vint_variant_type types[2] = {VINT_PROP_TYPE_STRING, VINT_PROP_TYPE_STRING};
    item->set_template(types, 2);
}

// 0x008EC130. The mode (or challenge) title and a subtitle per state.
static void wc_hud_item_splash_title_process(vint_data_item* item)
{
    const wrecking_crew_states state = wrecking_crew_get_state();
    const char* title;
    if (state == WCS_CHALLENGE_BEGIN || (state == WCS_MATCH_END && Wrecking_crew_current_round >= Wrecking_crew_options.rounds_to_win - 1 && Wrecking_crew_options.current_challenge_id != -1))
    {
        title = "HM_WC_MODE_CHALLENGE";
    }
    else
    {
        title = wrecking_crew_get_mode_display_name(Wrecking_crew_options.mode);
    }
    item->set_element(0, vint_variant(title));
    switch (state)
    {
        case WCS_CHALLENGE_BEGIN:
        {
            item->set_element(1, vint_variant("HM_HOW_TO_PLAY"));
            break;
        }
        case WCS_MATCH_BEGIN:
        case WCS_CHALLENGE_ROUND_BEGIN:
        {
            if (Wrecking_crew_options.current_challenge_id == -1)
            {
                item->set_element(1, vint_variant("HM_HOW_TO_PLAY"));
                break;
            }
            // A challenge round shows its stress and barrel settings.
            const wchar_t* stress = L"(loc)Unlocalized string";
            if (Wrecking_crew_options.stress_mode == WCSS_OFF)
            {
                stress = localization_get_display_string_req("MENU_OFF", nullptr);
            }
            else if (Wrecking_crew_options.stress_mode == WCSS_LOW)
            {
                stress = localization_get_display_string_req("MENU_LOW", nullptr);
            }
            else if (Wrecking_crew_options.stress_mode == WCSS_HIGH)
            {
                stress = localization_get_display_string_req("MENU_HIGH", nullptr);
            }
            const wchar_t* barrels = localization_get_display_string_req(Wrecking_crew_options.spawn_barrels ? "MENU_ON" : "MENU_OFF", nullptr);
            const wchar_t* barrels_label = localization_get_display_string_req("MENU_WRECKING_CREW_BARRELS", nullptr);
            const wchar_t* stress_label = localization_get_display_string_req("MENU_WRECKING_CREW_STRESS", nullptr);
            wchar_t text[0x200];
            sprintf(text, L"%ls: %ls  %ls: %ls", stress_label, stress, barrels_label, barrels);
            char encoded[0x600];
            vint_encode_wide_text(encoded, 0x600, text);
            item->set_element(1, vint_variant((const char*)encoded));
            break;
        }
        case WCS_BREAK:
        {
            wchar_t name[0x40];
            char encoded[0xC0];
            wrecking_crew_get_player_name(Wrecking_crew_current_player, name, 0x40);
            vint_encode_wide_text(encoded, 0xC0, name);
            item->set_element(1, vint_variant((const char*)encoded));
            break;
        }
        default:
        {
            item->set_element(1, vint_variant(""));
            break;
        }
    }
}

// 0x0089A0D0. No template: the info item is defined by the document.
static void wc_hud_item_splash_info_init(vint_data_item* item)
{
}

// 0x0089A0E0. Three lines of mode info at the match or challenge round start, and a shown flag (element 3).
static void wc_hud_item_splash_info_process(vint_data_item* item)
{
    if (wrecking_crew_get_state() == WCS_CHALLENGE_BEGIN)
    {
        item->set_element(0, vint_variant("MENU_WC_CHALLENGE_INFO_LINE_1"));
        item->set_element(1, vint_variant("MENU_WC_CHALLENGE_INFO_LINE_2"));
        item->set_element(2, vint_variant(""));
    }
    else if (wrecking_crew_get_state() != WCS_MATCH_BEGIN && wrecking_crew_get_state() != WCS_CHALLENGE_ROUND_BEGIN)
    {
        for (int i = 0; i < 3; i++)
        {
            item->set_element(i, vint_variant((const char*)nullptr));
        }
        item->set_element(3, vint_variant(false));
        return;
    }
    else
    {
        int cur_element = 0;
        for (int entry = 0; entry < 3; entry++)
        {
            // With no time limit, the line about the time is left out: line 2 in Mayhem, line 1 in the other four
            // modes.
            if (Wrecking_crew_options.free_play)
            {
                if (Wrecking_crew_options.mode == WCM_MAYHEM)
                {
                    if (entry == 2)
                    {
                        continue;
                    }
                }
                else if (Wrecking_crew_options.mode >= WCM_FALL_GUY && Wrecking_crew_options.mode <= WCM_FALL_GUY + 3 && entry == 1)
                {
                    continue;
                }
            }
            wchar_t text[0x100];
            char encoded[0x300];
            if (wrecking_crew_get_info_text(entry, text, 0x100))
            {
                vint_encode_wide_text(encoded, 0x300, text);
                item->set_element(cur_element++, vint_variant((const char*)encoded));
            }
            else
            {
                item->set_element(cur_element++, vint_variant(""));
            }
        }
        for (; cur_element < 3; cur_element++)
        {
            item->set_element(cur_element, vint_variant(""));
        }
    }
    item->set_element(3, vint_variant(true));
}

// 0x0089A310
static void wc_hud_item_splash_turn_end_init(vint_data_item* item)
{
    const vint_variant_type types[11] = {VINT_PROP_TYPE_BOOL, VINT_PROP_TYPE_STRING, VINT_PROP_TYPE_STRING, VINT_PROP_TYPE_STRING, VINT_PROP_TYPE_STRING, VINT_PROP_TYPE_BOOL,
                                         VINT_PROP_TYPE_STRING, VINT_PROP_TYPE_STRING, VINT_PROP_TYPE_STRING, VINT_PROP_TYPE_STRING, VINT_PROP_TYPE_STRING};
    item->set_template(types, 11);
}

// 0x008B3E90. After a turn: score, high score, green flag, letters, message, and the match winner's portrait.
static void wc_hud_item_splash_turn_end_process(vint_data_item* item)
{
    const wrecking_crew_states state = wrecking_crew_get_state();
    const bool show = state == WCS_BREAK || state == WCS_ROUND_END || state == WCS_MATCH_END;
    item->set_element(0, vint_variant(show));
    if (!show)
    {
        return;
    }
    hud_wrecking_crew_text_set score_set;
    hud_wrecking_crew_text_set high_score_set;
    hud_wrecking_crew_text_set letters_set;
    hud_wrecking_crew_text_set message_set;
    bool green_score = false;
    wrecking_crew_get_turn_end_text(&score_set, &high_score_set, &green_score, &letters_set, &message_set);
    int cur_element = 1;
    wc_hud_handle_text_set(item, &cur_element, &score_set);
    wc_hud_handle_text_set(item, &cur_element, &high_score_set);
    item->set_element(cur_element++, vint_variant(green_score));
    wc_hud_handle_text_set(item, &cur_element, &letters_set);
    wc_hud_handle_text_set(item, &cur_element, &message_set);
    const char* image = nullptr;
    if (state == WCS_MATCH_END && wrecking_crew_get_match_winner() != -1)
    {
        const char* name = Wrecking_crew_characters[Wc_player_infos[wrecking_crew_get_match_winner()].character_index]->bitmap_name;
        if (name != nullptr)
        {
            const unsigned int srid = stream2_container_find(name);
            if (srid != 0 && stream2_get_status(srid) == ST_LOADED)
            {
                image = name;
            }
        }
    }
    item->set_element(cur_element, vint_variant(image));
}

// 0x0089A380
static void wc_hud_item_splash_hints_init(vint_data_item* item)
{
    const vint_variant_type types[2] = {VINT_PROP_TYPE_STRING, VINT_PROP_TYPE_STRING};
    item->set_template(types, 2);
}

// 0x0089A3B0. Always empty.
static void wc_hud_item_splash_hints_process(vint_data_item* item)
{
    item->set_element(0, vint_variant((const char*)nullptr));
    item->set_element(1, vint_variant((const char*)nullptr));
}

// 0x0089A410. Shown flag, three rows of label and three numbers, and the total row.
static void wc_hud_item_splash_barrels_init(vint_data_item* item)
{
    const vint_variant_type types[15] = {VINT_PROP_TYPE_BOOL,
                                         VINT_PROP_TYPE_STRING, VINT_PROP_TYPE_INT, VINT_PROP_TYPE_INT, VINT_PROP_TYPE_INT,
                                         VINT_PROP_TYPE_STRING, VINT_PROP_TYPE_INT, VINT_PROP_TYPE_INT, VINT_PROP_TYPE_INT,
                                         VINT_PROP_TYPE_STRING, VINT_PROP_TYPE_INT, VINT_PROP_TYPE_INT, VINT_PROP_TYPE_INT,
                                         VINT_PROP_TYPE_STRING, VINT_PROP_TYPE_INT};
    item->set_template(types, 15);
}

// 0x0089A490. A barrel row: label, its two values and their product.
static void handle_barrel_row(vint_data_item* item, int* cur_element, hud_wrecking_crew_barrel_text_row* row)
{
    char text[0x60];
    vint_encode_wide_text(text, 0x60, row->label);
    item->set_element((*cur_element)++, vint_variant((const char*)text));
    item->set_element((*cur_element)++, vint_variant(row->value_one));
    item->set_element((*cur_element)++, vint_variant(row->value_two));
    item->set_element((*cur_element)++, vint_variant(row->value_one * row->value_two));
}

// 0x0089A5A0. The Fall Guy barrel summary.
static void wc_hud_item_splash_barrels_process(vint_data_item* item)
{
    hud_wrecking_crew_barrel_text_set barrel_set;
    wrecking_crew_get_barrel_text(&barrel_set);
    int cur_element = 1;
    item->set_element(0, vint_variant(barrel_set.should_display));
    if (!barrel_set.should_display)
    {
        return;
    }
    // Original bug kept: five rows (23 elements) are written to the 15-element template of three rows; the two
    // mini rows push the third row and the total past it.
    handle_barrel_row(item, &cur_element, &barrel_set.row_one);
    handle_barrel_row(item, &cur_element, &barrel_set.mini_row_one);
    handle_barrel_row(item, &cur_element, &barrel_set.row_two);
    handle_barrel_row(item, &cur_element, &barrel_set.mini_row_two);
    handle_barrel_row(item, &cur_element, &barrel_set.row_three);
    char text[0x60];
    vint_encode_wide_text(text, 0x60, barrel_set.final_row_label);
    item->set_element(cur_element++, vint_variant((const char*)text));
    item->set_element(cur_element, vint_variant(barrel_set.final_row_value));
}

// 0x0089A6F0. No template: the players item is defined by the document.
static void wc_hud_item_splash_players_init(vint_data_item* item)
{
}

// 0x0089A700. The score list after a turn: shown flag, its heading, then the player list from element 2.
static void wc_hud_item_splash_players_process(vint_data_item* item)
{
    const wrecking_crew_states state = wrecking_crew_get_state();
    if (state != WCS_BREAK && state != WCS_ROUND_END && state != WCS_MATCH_END)
    {
        item->set_element(0, vint_variant(false));
        return;
    }
    item->set_element(0, vint_variant(true));
    item->set_element(1, vint_variant("MENU_SCORE"));
    int cur_element = 2;
    wc_hud_handle_player_list(item, &cur_element);
}

// 0x0089A7C0
static void wc_hud_item_splash_veto_init(vint_data_item* item)
{
    const vint_variant_type types[11] = {VINT_PROP_TYPE_BOOL, VINT_PROP_TYPE_STRING, VINT_PROP_TYPE_INT, VINT_PROP_TYPE_STRING, VINT_PROP_TYPE_STRING, VINT_PROP_TYPE_STRING,
                                         VINT_PROP_TYPE_STRING, VINT_PROP_TYPE_STRING, VINT_PROP_TYPE_STRING, VINT_PROP_TYPE_STRING, VINT_PROP_TYPE_STRING};
    item->set_template(types, 11);
}

// 0x008EC3D0. Before a turn: the round, the backpack and weapon the player gets with their veto prompts, the
// "press to begin" line and the player's portrait.
static void wc_hud_item_splash_veto_process(vint_data_item* item)
{
    if (wrecking_crew_get_state() != WCS_ROUND_BEGIN && wrecking_crew_get_state() != WCS_PRE_TURN_BREAK)
    {
        item->set_element(0, vint_variant(false));
        return;
    }
    item->set_element(0, vint_variant(true));
    item->set_element(1, vint_variant("HM_ROUND_COLON"));
    item->set_element(2, vint_variant(Wrecking_crew_current_round + 1));
    char buffer[0xC0];
    wchar_t text[0x40];
    hud_wrecking_crew_text_set weapon_set;
    hud_wrecking_crew_text_set backpack_set;
    bool can_veto_weapon = false;
    bool can_veto_backpack = false;
    const bool has_veto_text = wrecking_crew_get_veto_text(&weapon_set, &can_veto_weapon, &backpack_set, &can_veto_backpack);
    // Backpack: elements 3 (icon or label), 4 (name or text), 5 (veto prompt).
    if (has_veto_text)
    {
        if (backpack_set.should_display)
        {
            copy_to_ascii(buffer, backpack_set.label, 0xC0);
            item->set_element(3, vint_variant((const char*)buffer));
            copy_to_ascii(buffer, backpack_set.text, 0xC0);
            item->set_element(4, vint_variant((const char*)buffer));
        }
        else
        {
            item->set_element(3, vint_variant((const char*)nullptr));
            item->set_element(4, vint_variant((const char*)nullptr));
        }
    }
    else
    {
        const wrecking_crew_backpack_info* wc_backpack = wrecking_crew_get_current_backpack_info();
        if (wc_backpack == nullptr)
        {
            item->set_element(3, vint_variant((const char*)nullptr));
            item->set_element(4, vint_variant((const char*)nullptr));
        }
        else
        {
            const multi_backpack_info* b_info = multi_game_backpack_get_info(wc_backpack->backpack_type);
            if (b_info == nullptr)
            {
                return;
            }
            item->set_element(3, vint_variant(b_info->icon_name));
            item->set_element(4, vint_variant((const char*)b_info->display_name));
        }
    }
    if ((has_veto_text && can_veto_backpack) || wrecking_crew_can_veto_backpack())
    {
        _snprintf_safe(text, 0x40, localization_get_display_string_req("MENU_PRESS_TO_VETO", nullptr), L"[button:menu_x]");
        vint_encode_wide_text(buffer, 0xC0, text);
        item->set_element(5, vint_variant((const char*)buffer));
    }
    else
    {
        item->set_element(5, vint_variant((const char*)nullptr));
    }
    // Weapon: elements 6, 7, 8.
    if (has_veto_text)
    {
        if (weapon_set.should_display)
        {
            copy_to_ascii(buffer, weapon_set.label, 0xC0);
            item->set_element(6, vint_variant((const char*)buffer));
            copy_to_ascii(buffer, weapon_set.text, 0xC0);
            item->set_element(7, vint_variant((const char*)buffer));
        }
        else
        {
            item->set_element(6, vint_variant((const char*)nullptr));
            item->set_element(7, vint_variant((const char*)nullptr));
        }
    }
    else
    {
        const wrecking_crew_weapon_info* wc_weapon = wrecking_crew_get_current_weapon_info();
        if (wc_weapon == nullptr)
        {
            item->set_element(6, vint_variant((const char*)nullptr));
            item->set_element(7, vint_variant((const char*)nullptr));
        }
        else
        {
            const weapon_info* w_info = weapons_get_weapon_info(wc_weapon->w_info_handle);
            if (w_info == nullptr)
            {
                return;
            }
            item->set_element(6, vint_variant((const char*)w_info->icon_name));
            item->set_element(7, vint_variant((const char*)w_info->weapon_inv_item_info->display_name));
        }
    }
    // Original: unlike the backpack's, the weapon veto prompt also needs wrecking_crew_player_can_veto.
    if (((has_veto_text && can_veto_weapon) || wrecking_crew_can_veto_weapon()) && wrecking_crew_player_can_veto())
    {
        _snprintf_safe(text, 0x40, localization_get_display_string_req("MENU_PRESS_TO_VETO", nullptr), L"[button:menu_y]");
        vint_encode_wide_text(buffer, 0xC0, text);
        item->set_element(8, vint_variant((const char*)buffer));
    }
    else
    {
        item->set_element(8, vint_variant((const char*)nullptr));
    }
    // "<player> - press to begin": the player's name and the localized line are made into the format of the button.
    wchar_t name[0x20];
    wrecking_crew_get_player_name(Wrecking_crew_current_player, name, 0x20);
    const wchar_t* begin = localization_get_display_string_req("MENU_PRESS_TO_BEGIN", nullptr);
    wchar_t format[0x40];
    _snprintf_safe(format, 0x40, "%ls - %ls", name, begin);
    _snprintf_safe(text, 0x40, format, L"[button:menu_accept]");
    vint_encode_wide_text(buffer, 0xC0, text);
    item->set_element(9, vint_variant((const char*)buffer));
    // Original: Wrecking_crew_current_player is not checked against -1 here.
    const char* image = Wrecking_crew_characters[Wc_player_infos[Wrecking_crew_current_player].character_index]->bitmap_name;
    if (image != nullptr)
    {
        const unsigned int srid = stream2_container_find(image);
        if (srid == 0 || stream2_get_status(srid) != ST_LOADED)
        {
            image = nullptr;
        }
    }
    item->set_element(10, vint_variant(image));
}

// ---- hint bar document

// 0x0089A830. rfg.exe jumps straight to multi_game_camera_is_running.
static bool wc_hud_doc_hints_should_render()
{
    return multi_game_camera_is_running();
}

// 0x0089A840
static void wc_hud_item_hints_init(vint_data_item* item)
{
    const vint_variant_type types[6] = {VINT_PROP_TYPE_BOOL, VINT_PROP_TYPE_STRING, VINT_PROP_TYPE_STRING, VINT_PROP_TYPE_STRING, VINT_PROP_TYPE_STRING, VINT_PROP_TYPE_STRING};
    item->set_template(types, 6);
}

// 0x008ECA60. The hint bar while the camera runs: continue (after a turn), previous and next camera.
static void wc_hud_item_hints_process(vint_data_item* item)
{
    wchar_t text[0x80];
    char encoded[0x180];
    if (wrecking_crew_get_state() == WCS_ROUND_BEGIN || wrecking_crew_get_state() == WCS_PRE_TURN_BREAK || wrecking_crew_get_state() == WCS_PRE_BREAK)
    {
        item->set_element(0, vint_variant(true));
        item->set_element(1, vint_variant(""));
        item->set_element(2, vint_variant(""));
        item->set_element(3, vint_variant("[button:spec_prev]"));
    }
    else
    {
        const wchar_t* label = localization_get_display_string_req("MENU_CONTINUE", nullptr);
        if (wrecking_crew_get_state() == WCS_MATCH_END)
        {
            label = localization_get_display_string_req("MENU_DONE", nullptr);
        }
        _snprintf_safe(text, 0x80, "[button:menu_accept] %ls", label);
        vint_encode_wide_text(encoded, 0x180, text);
        item->set_element(0, vint_variant(true));
        item->set_element(1, vint_variant(""));
        item->set_element(2, vint_variant((const char*)encoded));
        item->set_element(3, vint_variant("[button:spec_prev]"));
    }
    swprintf(text, 0x80, L"%hs %ls", "[button:spec_next]", localization_get_display_string_req("MENU_SWITCH_CAMERA", nullptr));
    vint_encode_wide_text(encoded, 0x180, text);
    item->set_element(4, vint_variant((const char*)encoded));
    item->set_element(5, vint_variant(""));
}

// ---- public

// 0x0089A890 (:1258)
void hud_ui_wrecking_crew_system_init()
{
    for (int& handle : Wrecking_crew_doc_handles)
    {
        handle = -1;
    }
    for (unsigned int& handle : Wrecking_crew_data_item_handles)
    {
        handle = 0xFFFFFFFF;
    }
}

// 0x00916FC0 (:1270)
void hud_ui_wrecking_crew_enter()
{
    for (int& handle : Wrecking_crew_doc_handles)
    {
        handle = -1;
    }
    memset(Wrecking_crew_data_item_handles, 0, sizeof(Wrecking_crew_data_item_handles));
    Assets_loaded_uiwre = false;
    Active_uiwre = false;
    Data_items_created_uiwre = false;
    hud_ui_wrecking_crew_load_items(false);
    if (wrecking_crew_is_active())
    {
        audiolib_min_max_distance_set_extended(true);
    }
}

// 0x00904C50 (:1288)
void hud_ui_wrecking_crew_exit()
{
    hud_ui_wrecking_crew_unload_items(false);
}

// 0x00910EC0 (:1294)
void hud_ui_wrecking_crew_load_items(bool assets_only)
{
    if (Active_uiwre)
    {
        return;
    }
    for (int i = 0; i < NUM_HUD_WRECKING_CREW_DOCS; i++)
    {
        if (!assets_only)
        {
            Wrecking_crew_doc_handles[i] = ui_load_document(Wrecking_crew_doc_info[i].name, DFIT_ALPHA, 0.5f, DLT_NORMAL, false, STREAM_PRIORITY_AUDIO_CACHE_LOAD, false);
        }
        else if (Wrecking_crew_doc_handles[i] != -1)
        {
            ui_reload_doc_assets(Wrecking_crew_doc_handles[i]);
        }
    }
    Assets_loaded_uiwre = false;
    Active_uiwre = true;
}

// 0x008F5DA0 (:1318)
void hud_ui_wrecking_crew_unload_items(bool assets_only)
{
    if (!Active_uiwre && assets_only)
    {
        return;
    }
    for (int i = 0; i < NUM_HUD_WRECKING_CREW_DOCS; i++)
    {
        if (Wrecking_crew_doc_handles[i] == -1)
        {
            continue;
        }
        if (assets_only)
        {
            ui_unload_doc_assets(Wrecking_crew_doc_handles[i]);
        }
        else
        {
            if (Wrecking_crew_doc_info[i].shutdown != nullptr)
            {
                Wrecking_crew_doc_info[i].shutdown(Wrecking_crew_doc_handles[i]);
            }
            ui_unload_document(Wrecking_crew_doc_handles[i], DFOT_NONE, 0.5f);
            Wrecking_crew_doc_handles[i] = -1;
        }
    }
    if (!assets_only)
    {
        Data_items_created_uiwre = false;
    }
    Active_uiwre = false;
}

// 0x008F5E40 (:1347)
void hud_ui_wrecking_crew_process(float dtime)
{
    if (!Assets_loaded_uiwre && Active_uiwre)
    {
        Assets_loaded_uiwre = true;
        for (int i = 0; i < NUM_HUD_WRECKING_CREW_DOCS; i++)
        {
            switch (ui_get_doc_asset_state(Wrecking_crew_doc_handles[i]))
            {
                case ST_UNLOADED:
                case ST_ERROR:
                    hud_ui_wrecking_crew_unload_items(false);
                    break;
                case ST_LOADING:
                    Assets_loaded_uiwre = false;
                    break;
                default:
                    break;
            }
        }
        if (Assets_loaded_uiwre)
        {
            // Original: unlike the data items, the document init callbacks run each time the assets finish loading.
            for (int i = 0; i < NUM_HUD_WRECKING_CREW_DOCS; i++)
            {
                if (Wrecking_crew_doc_info[i].init != nullptr)
                {
                    Wrecking_crew_doc_info[i].init(Wrecking_crew_doc_handles[i]);
                }
            }
            if (!Data_items_created_uiwre)
            {
                for (int i = 0; i < NUM_HUD_WRECKING_CREW_DATA_ITEMS; i++)
                {
                    const wc_hud_data_item_info& info = Wrecking_crew_data_item_info[i];
                    Wrecking_crew_data_item_handles[i] = 0;
                    if (info.init != nullptr && Wrecking_crew_doc_handles[info.doc] != -1)
                    {
                        vint_data_item* item = vint_data_item::create_new(info.name);
                        if (item != nullptr)
                        {
                            Wrecking_crew_data_item_handles[i] = item->get_handle();
                            info.init(item);
                            ui_add_data_item(Wrecking_crew_doc_handles[info.doc], Wrecking_crew_data_item_handles[i]);
                        }
                    }
                }
                Data_items_created_uiwre = true;
            }
        }
    }
    if (Assets_loaded_uiwre || (game_is_paused() && Data_items_created_uiwre))
    {
        // rfg.exe compares unsigned: WCM_NONE processes nothing.
        if ((unsigned int)Wrecking_crew_options.mode < NUM_WRECKING_CREW_MODES)
        {
            for (int i = 0; i < NUM_HUD_WRECKING_CREW_DATA_ITEMS; i++)
            {
                if (Wrecking_crew_data_item_handles[i] != 0 && Wrecking_crew_data_item_info[i].process != nullptr)
                {
                    vint_data_item* item = vint_data_item::find(Wrecking_crew_data_item_handles[i]);
                    if (item != nullptr)
                    {
                        Wrecking_crew_data_item_info[i].process(item);
                    }
                }
            }
            for (int i = 0; i < NUM_HUD_WRECKING_CREW_DOCS; i++)
            {
                if (Wrecking_crew_doc_handles[i] != -1)
                {
                    if (Wrecking_crew_doc_info[i].process != nullptr)
                    {
                        Wrecking_crew_doc_info[i].process(Wrecking_crew_doc_handles[i]);
                    }
                    ui_add_for_processing(Wrecking_crew_doc_handles[i]);
                }
            }
        }
    }
    // The core's per-frame attack and backpack flags are cleared here, after the HUD has read them.
    Wrecking_crew_player_attacked_this_frame = false;
    Wrecking_crew_player_activated_backpack_this_frame = false;
}

// 0x008DA810 (:1449)
void hud_ui_wrecking_crew_render()
{
    if (!Assets_loaded_uiwre)
    {
        return;
    }
    for (int i = 0; i < NUM_HUD_WRECKING_CREW_DOCS; i++)
    {
        if (Wrecking_crew_doc_handles[i] != -1 && (Wrecking_crew_doc_info[i].should_render == nullptr || Wrecking_crew_doc_info[i].should_render()))
        {
            ui_add_for_rendering(Wrecking_crew_doc_handles[i]);
        }
    }
}

// 0x0089A900 (:1472)
int hud_ui_wrecking_crew_get_doc_handle(hud_wrecking_crew_docs doc)
{
    // Original bug kept: HWCD_NONE (-1) passes the check and reads before the table.
    if (doc < NUM_HUD_WRECKING_CREW_DOCS)
    {
        return Wrecking_crew_doc_handles[doc];
    }
    return -1;
}

// 0x0089A920 (:1479)
void hud_ui_wrecking_crew_show_stamp(const char* message)
{
    vint_data_item* item = vint_data_item::find(Wrecking_crew_data_item_handles[HWCDI_STAMP]);
    if (item != nullptr)
    {
        item->set_element(0, vint_variant(true));
        item->set_element(1, vint_variant(message));
        item->set_updated();
    }
}

// 0x0089A9A0 (:1491)
void hud_ui_wrecking_crew_hide_stamp()
{
    vint_data_item* item = vint_data_item::find(Wrecking_crew_data_item_handles[HWCDI_STAMP]);
    if (item != nullptr)
    {
        item->set_element(0, vint_variant(false));
    }
}

// 0x0089A9E0 (:1500)
void hud_ui_wrecking_crew_show_message(const char* title, const char* label)
{
    vint_data_item* item = vint_data_item::find(Wrecking_crew_data_item_handles[HWCDI_MESSAGE]);
    if (item != nullptr)
    {
        item->set_element(0, vint_variant(true));
        item->set_element(1, vint_variant(title));
        item->set_element(2, vint_variant(label));
        item->set_updated();
    }
}

// 0x0089AA90 (:1513)
void hud_ui_wrecking_crew_hide_message()
{
    vint_data_item* item = vint_data_item::find(Wrecking_crew_data_item_handles[HWCDI_MESSAGE]);
    if (item != nullptr)
    {
        item->set_element(0, vint_variant(false));
    }
}

// 0x0089AAD0 (:1522)
void hud_ui_wrecking_crew_show_counter(const wchar_t* text, bool is_numeric)
{
    vint_data_item* item = vint_data_item::find(Wrecking_crew_data_item_handles[HWCDI_COUNTER]);
    if (item != nullptr)
    {
        char encoded[0x60];
        vint_encode_wide_text(encoded, 0x60, text);
        item->set_element(0, vint_variant(true));
        item->set_element(1, vint_variant((const char*)encoded));
        item->set_element(2, vint_variant(is_numeric));
        // Original: unlike the stamp and the message, the counter is not marked updated.
    }
}

// 0x0089ABB0 (:1536)
void hud_ui_wrecking_crew_hide_counter()
{
    vint_data_item* item = vint_data_item::find(Wrecking_crew_data_item_handles[HWCDI_COUNTER]);
    if (item != nullptr)
    {
        item->set_element(0, vint_variant(false));
    }
}
