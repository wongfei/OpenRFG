// Original: volition/rfg/code/interface/ui_document_hud_multiplayer.cpp
//
// The multiplayer HUD documents (stub_impl.md S7): the hint bar, the main HUD (timer, round info, respawn, stamps,
// objectives, the engineer box, the spectator lines), the mode documents, the experience popups, the in-game scoreboard,
// the how-to-play splash; the kill messages with their phrase tokens.

#include "volition/rfg/code/interface/ui_document_hud_multiplayer.h"

#include "volition/audio_lib/code/audiolib_api.h"
#include "volition/interface_lib/code/objects/vint_object_base.h"
#include "volition/interface_lib/code/vint_data_group.h"
#include "volition/interface_lib/code/vint_data_item.h"
#include "volition/interface_lib/code/vint_external.h"
#include "volition/interface_lib/code/vint_variant.h"
#include "volition/rfg/code/control/control_gameplay.h"
#include "volition/rfg/code/control/control_map.h"
#include "volition/rfg/code/gameplay/gameplay.h"
#include "volition/rfg/code/gamewide/game_clock.h"
#include "volition/rfg/code/gamewide/gameaudio.h"
#include "volition/rfg/code/hud/hud.h"
#include "volition/rfg/code/hud/hud_message.h"
#include "volition/rfg/code/interface/loading.h"
#include "volition/rfg/code/interface/localization/localization_shared.h"
#include "volition/rfg/code/interface/multi_player_menus/multi_player_menu_system.h"
#include "volition/rfg/code/interface/ui_document_hud.h"
#include "volition/rfg/code/interface/ui_document_hud_tool_tips.h"
#include "volition/rfg/code/interface/ui_document_manager.h"
#include "volition/rfg/code/interface/ui_message_box.h"
#include "volition/rfg/code/multi/multi_core/include/multi.h"
#include "volition/rfg/code/multi/multi_core/include/multi_core_platform.h"
#include "volition/rfg/code/multi/multi_core/include/multi_core_session_base.h"
#include "volition/rfg/code/multi/multi_core/include/multi_player_uid.h"
#include "volition/rfg/code/multi/multi_core/include/multi_session.h"
#include "volition/rfg/code/multi/multi_game/modes/multi_game_mode_common.h"
#include "volition/rfg/code/multi/multi_game/modes/multi_game_mode_king_of_the_hill.h"
#include "volition/rfg/code/multi/multi_game/modes/multi_game_mode_siege.h"
#include "volition/rfg/code/multi/multi_game/multi_game.h"
#include "volition/rfg/code/multi/multi_game/multi_game_badge.h"
#include "volition/rfg/code/multi/multi_game/multi_game_camera.h"
#include "volition/rfg/code/multi/multi_game/multi_game_data.h"
#include "volition/rfg/code/multi/multi_game/multi_game_object_backpack.h"
#include "volition/rfg/code/multi/multi_game/multi_game_render.h"
#include "volition/rfg/code/multi/multi_game/multi_game_settings.h"
#include "volition/rfg/code/multi/multi_game/multi_game_stats.h"
#include "volition/rfg/code/multi/multi_game/multi_gameplay_mode.h"
#include "volition/rfg/code/multi/multi_game/multi_gameplay_mode_shared.h"
#include "volition/rfg/code/objects/humans/human.h"
#include "volition/rfg/code/objects/humans/human_info.h"
#include "volition/rfg/code/objects/humans/human_teams.h"
#include "volition/rfg/code/objects/humans/player.h"
#include "volition/rfg/code/weapons/weapons.h"
#include "volition/render_lib/code/gr/gr.h"
#include "volition/render_lib/code/rl_render_lib.h"
#include "volition/vlib/code/input/mouse.h"
#include "volition/vlib/code/math/vector2.h"
#include "volition/vlib/code/os/vconsole.h"
#include "volition/vlib/code/time/timestamp.h"
#include "volition/vlib/code/util/color.h"
#include "volition/vlib/code/util/string_util.h"
#include "volition/vlib/code/util/unicode_tools.h"
#include "volition/vlib/code/util/varray.h"

#include <stdio.h>
#include <string.h>
#include <wchar.h>

// ---- file-private types (rfg.pdb)
// size 0x10
struct mp_data_item_info
{
    hud_multiplayer_docs doc; // 0x00
    void (*init)(vint_data_item*); // 0x04
    void (*process)(vint_data_item*); // 0x08
    const char* name; // 0x0C
};
static_assert(sizeof(mp_data_item_info) == 0x10, "mp_data_item_info");

// size 0x18
struct mp_doc_info
{
    const char* name; // 0x00
    void (*init)(int); // 0x04
    void (*process)(int); // 0x08
    bool (*should_render)(int); // 0x0C
    void (*shutdown)(int); // 0x10
    multi_gameplay_mode mode; // 0x14
};
static_assert(sizeof(mp_doc_info) == 0x18, "mp_doc_info");

static bool should_render_multiplayer_doc_hints(int doc_handle);
static bool should_render_multiplayer_doc_main(int doc_handle);
static void init_multiplayer_doc_xp(int doc_handle);
static void process_multiplayer_doc_xp(int doc_handle);
static bool should_render_multiplayer_doc_xp(int doc_handle);
static void shutdown_multiplayer_doc_xp(int doc_handle);
static void init_multiplayer_doc_scoreboard(int doc_handle);
static void process_multiplayer_doc_scoreboard(int doc_handle);
static bool should_render_multiplayer_doc_scoreboard(int doc_handle);
static void shutdown_multiplayer_doc_scoreboard(int doc_handle);
static void process_multiplayer_doc_info(int doc_handle);
static bool should_render_multiplayer_doc_info(int doc_handle);
static void init_multiplayer_data_item_info_title(vint_data_item* item);
static void process_multiplayer_data_item_info_title(vint_data_item* item);
static void init_multiplayer_data_item_info_text(vint_data_item* item);
static void process_multiplayer_data_item_info_text(vint_data_item* item);
static void init_multiplayer_data_item_info_hints(vint_data_item* item);
static void process_multiplayer_data_item_info_hints(vint_data_item* item);
static void init_multiplayer_data_item_main(vint_data_item* item);
static void process_multiplayer_data_item_main(vint_data_item* item);
static void init_multiplayer_data_item_xp(vint_data_item* item);
static void process_multiplayer_data_item_xp(vint_data_item* item);
static void init_multiplayer_data_item_timer(vint_data_item* item);
static void process_multiplayer_data_item_timer(vint_data_item* item);
static void init_multiplayer_data_item_mode_info(vint_data_item* item);
static void process_multiplayer_data_item_mode_info(vint_data_item* item);
static void init_multiplayer_data_item_hints(vint_data_item* item);
static void process_multiplayer_data_item_hints(vint_data_item* item);
static void init_multiplayer_data_item_respawn(vint_data_item* item);
static void process_multiplayer_data_item_respawn(vint_data_item* item);
static void init_multiplayer_data_item_stamp(vint_data_item* item);
static void init_multiplayer_data_item_objective(vint_data_item* item);
static void init_multiplayer_data_item_engineer(vint_data_item* item);
static void init_multiplayer_data_item_scoreboard_team_header(vint_data_item* item);
static void process_multiplayer_data_item_scoreboard_team_header(vint_data_item* item);
static void init_multiplayer_data_item_scoreboard_ffa_header(vint_data_item* item);
static void process_multiplayer_data_item_scoreboard_ffa_header(vint_data_item* item);
static void init_multiplayer_data_item_scoreboard_score(vint_data_item* item);
static void process_multiplayer_data_item_scoreboard_score(vint_data_item* item);
static void init_multiplayer_data_item_specator(vint_data_item* item);
static void process_multiplayer_data_item_spectator(vint_data_item* item);

// ---- file-static globals
// 0x01300F00
static const mp_data_item_info Multi_data_item_info[16] = {
    {HMD_HINTS, init_multiplayer_data_item_hints, process_multiplayer_data_item_hints, "mp_hint_bar_mode_spectator_hints"},
    {HMD_MAIN, init_multiplayer_data_item_main, process_multiplayer_data_item_main, "multiplayer_hud_main"},
    {HMD_MAIN, init_multiplayer_data_item_timer, process_multiplayer_data_item_timer, "multiplayer_hud_timer"},
    {HMD_MAIN, init_multiplayer_data_item_mode_info, process_multiplayer_data_item_mode_info, "multiplayer_hud_mode_info"},
    {HMD_MAIN, init_multiplayer_data_item_respawn, process_multiplayer_data_item_respawn, "multiplayer_hud_respawn"},
    {HMD_MAIN, init_multiplayer_data_item_stamp, nullptr, "multiplayer_hud_stamp_info"},
    {HMD_MAIN, init_multiplayer_data_item_objective, nullptr, "multiplayer_hud_objective"},
    {HMD_MAIN, init_multiplayer_data_item_engineer, nullptr, "multiplayer_hud_demo_change"},
    {HMD_XP, init_multiplayer_data_item_xp, process_multiplayer_data_item_xp, "multiplayer_hud_xp"},
    {HMD_SCOREBOARD, init_multiplayer_data_item_scoreboard_team_header, process_multiplayer_data_item_scoreboard_team_header, "multiplayer_scoreboard_team_header"},
    {HMD_SCOREBOARD, init_multiplayer_data_item_scoreboard_ffa_header, process_multiplayer_data_item_scoreboard_ffa_header, "multiplayer_scoreboard_ffa_header"},
    {HMD_SCOREBOARD, init_multiplayer_data_item_scoreboard_score, process_multiplayer_data_item_scoreboard_score, "multiplayer_scoreboard_score"},
    {HMD_MAIN, init_multiplayer_data_item_specator, process_multiplayer_data_item_spectator, "mp_hint_bar_mode_spectator"},
    {HMD_INFO, init_multiplayer_data_item_info_title, process_multiplayer_data_item_info_title, "mp_wc_splash_title"},
    {HMD_INFO, init_multiplayer_data_item_info_text, process_multiplayer_data_item_info_text, "mp_wc_splash_info"},
    {HMD_INFO, init_multiplayer_data_item_info_hints, process_multiplayer_data_item_info_hints, "mp_wc_splash_hints"},
};
// 0x01301000
static const vint_variant_type players_template_elements[18] = {VINT_PROP_TYPE_INT, VINT_PROP_TYPE_BOOL, VINT_PROP_TYPE_BOOL, VINT_PROP_TYPE_BOOL, VINT_PROP_TYPE_STRING, VINT_PROP_TYPE_INT, VINT_PROP_TYPE_STRING, VINT_PROP_TYPE_INT, VINT_PROP_TYPE_INT, VINT_PROP_TYPE_BOOL, VINT_PROP_TYPE_INT, VINT_PROP_TYPE_BOOL, VINT_PROP_TYPE_INT, VINT_PROP_TYPE_BOOL, VINT_PROP_TYPE_INT, VINT_PROP_TYPE_BOOL, VINT_PROP_TYPE_INT, VINT_PROP_TYPE_BOOL};
// UI.md UI5: a shared static of rfg.exe's interface unity (the multiplayer menus use it), external here.
int Mp_msgbox_handle = -1; // 0x016632E0
static bool Scoreboard_selection_first_side = true; // 0x01664897
// 0x01664968. Original quirk kept: the King of the Hill document is rfg_ui_mp_siege too.
static mp_doc_info Multi_doc_info[7] = {
    {"rfg_ui_mp_hint_bar", nullptr, nullptr, should_render_multiplayer_doc_hints, nullptr, MGPM_NONE},
    {"rfg_ui_multiplayer_hud", nullptr, nullptr, should_render_multiplayer_doc_main, nullptr, MGPM_NONE},
    {"rfg_ui_mp_siege", siege_mode_hud_doc_init, siege_mode_hud_doc_process, siege_mode_hud_doc_should_render, siege_mode_hud_doc_shutdown, (multi_gameplay_mode)3},
    {"rfg_ui_mp_siege", king_of_the_hill_mode_hud_doc_init, king_of_the_hill_mode_hud_doc_process, king_of_the_hill_mode_hud_doc_should_render, king_of_the_hill_mode_hud_doc_shutdown, (multi_gameplay_mode)4},
    {"rfg_ui_mp_hud_xp", init_multiplayer_doc_xp, process_multiplayer_doc_xp, should_render_multiplayer_doc_xp, shutdown_multiplayer_doc_xp, MGPM_NONE},
    {"rfg_ui_mp_scoreboard", init_multiplayer_doc_scoreboard, process_multiplayer_doc_scoreboard, should_render_multiplayer_doc_scoreboard, shutdown_multiplayer_doc_scoreboard, MGPM_NONE},
    {"rfg_ui_mp_wc_splash", nullptr, process_multiplayer_doc_info, should_render_multiplayer_doc_info, nullptr, MGPM_NONE},
};
static int Multi_doc_handles[7] = {-1, -1, -1, -1, -1, -1, -1}; // 0x01664A10
static int Scoreboard_input_id = -1; // 0x01664A2C
static int Info_text_input_id = -1; // 0x01664A30
static int Xp_sound_play_id = -1; // 0x01664A34
static farray<enum hud_message_handle,5> Multi_kill_messages; // 0x01667054
static farray<enum hud_message_handle,5> Multi_kill_message_shadows; // 0x01667078
static timestamp New_backpack_timestamp; // 0x0166709C, initial data ffffffff
static timestamp Info_text_timer; // 0x016670A0, initial data ffffffff
// 0x016670A8: the tokens of a kill phrase (localized text such as "%attacker killed %victim %icon").
static kill_phrase_token Kill_phrase_tokens[5] = {L"icon", L"attacker", L"victim", L"his", L"himself"};
static vector2 Verts[8] = {{235.0f, 240.0f}, {235.0f, 480.0f}, {330.0f, 75.0f}, {330.0f, 645.0f}, {950.0f, 75.0f}, {950.0f, 645.0f}, {1045.0f, 240.0f}, {1045.0f, 480.0f}}; // 0x01667120
static int Cached_handle; // 0x02BF2200
static unsigned int Multi_data_item_handles[16]; // 0x02BFD430
static bool Scoreboard_needs_reinit; // 0x02C0015A
static unsigned int Multi_mode_data_item_handle; // 0x02C00ED8
static bool Assets_loaded_uimulti; // 0x02C0803B
static bool Active_uimulti; // 0x02C0803C
static bool Data_items_created_uimulti; // 0x02C0803D
static bool Scoreboard_active; // 0x02C0803E
static bool Scoreboard_sticky; // 0x02C0803F
static vint_data_group* Player_data_group; // 0x02C08040
static bool Selected_player_can_view_gamercard; // 0x02C08048
static bool Info_text_active; // 0x02C08049
static int Scoreboard_selection_index; // 0x02C0804C
static float KILL_MESSAGE_START_X; // 0x02C08050
static float KILL_MESSAGE_START_Y; // 0x02C08054
static float KILL_MESSAGE_SHADOW_START_X; // 0x02C08058
static float KILL_MESSAGE_SHADOW_START_Y; // 0x02C0805C
static int Last_displayed_mp_second; // 0x02C08060

// 0x008970B0
kill_phrase_token::kill_phrase_token(const wchar_t* name_in)
{
    name = name_in;
    name_len = (int)wcslen(name_in);
}

// 0x008970E0
void unused_dcf_show_kill_message()
{
    if (console_get_arg(2, 1))
    {
        hud_message_add_user(Console_arg, HUMT_MP_KILL, KILL_MESSAGE_START_X, KILL_MESSAGE_START_Y, false, 7.0f);
    }
}

// 0x00897130 (LTCG: mgd in EAX, encoded_name in ECX)
static void get_encoded_display_name_from_from_multi_game_data(const multi_game_data* mgd, int size, char* encoded_name, const wchar_t* default_name)
{
    vint_encode_wide_text(encoded_name, size, mgd != nullptr ? mgd->display_name : default_name);
}

// 0x00897160
static bool should_render_multiplayer_doc_hints(int doc_handle)
{
    return true;
}

// 0x008E9C10
static bool should_render_multiplayer_doc_main(int doc_handle)
{
    return !hud_ui_multiplayer_splash_screen_is_active();
}

// 0x00897170
static void init_multiplayer_doc_xp(int doc_handle)
{
}

// 0x00897180
static void process_multiplayer_doc_xp(int doc_handle)
{
}

// 0x00897190
static bool should_render_multiplayer_doc_xp(int doc_handle)
{
    return true;
}

// 0x008971A0
static void shutdown_multiplayer_doc_xp(int doc_handle)
{
}

// 0x008971B0
static void init_scoreboard_data()
{
    local_player_info local_info;
    player_get_local_player_info(&local_info);
    // Magic number: up to 16 players each side.
    unsigned int players[32];
    int num_players = 0;
    if (multi_gameplay_mode_is_team_mode(multi_gameplay_mode_get()))
    {
        multi_game_stats_sort_players_based_on_xp(local_info.team, players, num_players);
        unsigned int others[16];
        int num_others = 0;
        multi_game_stats_sort_players_based_on_xp(multi_game_mode_get_opposite_team(local_info.team), others, num_others);
        memcpy(players + num_players, others, num_others * sizeof(unsigned int));
        num_players += num_others;
    }
    else
    {
        multi_game_stats_sort_players_based_on_score(HUMAN_TEAM_NONE, players, num_players);
    }
    for (int i = 0; i < num_players; i++)
    {
        vint_data_item* item = Player_data_group->add_data_item(nullptr);
        if (item == nullptr)
        {
            return;
        }
        item->set_template(players_template_elements, 18);
        const multi_game_data* mgd = multi_game_data_get(players[i], true, false);
        if (mgd != nullptr)
        {
            item->set_element(0, vint_variant(i));
            item->set_element(5, vint_variant((int)mgd->team));
        }
    }
}

// 0x008D8CA0
static void init_multiplayer_doc_scoreboard(int doc_handle)
{
    Player_data_group = vint_data_group::create_new("multiplayer_scoreboard_players");
    if (Player_data_group != nullptr)
    {
        ui_add_data_group(doc_handle, Player_data_group->get_handle());
        Cached_handle = doc_handle;
        init_scoreboard_data();
    }
}

// A Button_info button pressed (and not waiting for its release) with the given input focus.
static bool mp_hud_button_pressed(int input_id, int button)
{
    const int lock = Current_input_lock_id != -1 ? Current_input_lock_id : 0;
    bool focus = input_id == -1 && lock == 0;
    if (!focus)
    {
        const int top = Current_input_lock_id != -1 ? Current_input_lock_id : (Input_types.size() - 1 < 0 ? 1 : Input_types[Input_types.size() - 1].id);
        focus = input_id == top;
    }
    return focus && !Button_info.binfo[button].m_button_wait_for_up && Button_info.binfo[button].m_button_input > 0.0f;
}

// 0x0090FFB0
static void process_multiplayer_doc_scoreboard(int doc_handle)
{
    if (Scoreboard_needs_reinit)
    {
        // Original quirk kept: the frame the group goes away the rest is skipped; the next frame creates it.
        if (Player_data_group != nullptr)
        {
            vint_doc_info* info = ui_get_doc_info(Cached_handle);
            if (info != nullptr && info->m_doc != nullptr)
            {
                unsigned int handle = Player_data_group->get_handle();
                info->m_group_handles.remove_matching(handle);
            }
            Player_data_group->destroy();
            Player_data_group = nullptr;
            return;
        }
        Player_data_group = vint_data_group::create_new("multiplayer_scoreboard_players");
        if (Player_data_group != nullptr)
        {
            ui_add_data_group(Cached_handle, Player_data_group->get_handle());
            init_scoreboard_data();
        }
        Scoreboard_needs_reinit = false;
    }
    local_player_info local_info;
    player_get_local_player_info(&local_info);
    if (controls_is_button_just_pressed(CBA_GAC_SCOREBOARD, Player_controller_index))
    {
        if (!Scoreboard_active)
        {
            hud_ui_multiplayer_scoreboard_enter(true);
        }
        else if (Scoreboard_sticky && !multi_player_menu_system_has_open_message_box())
        {
            hud_ui_multiplayer_scoreboard_exit();
        }
    }
    else if ((!Scoreboard_sticky || controls_is_button_just_pressed(CBA_MENU_ABORT, Player_controller_index)) && Scoreboard_active)
    {
        const int msgbox = Mp_msgbox_handle;
        if (ui_is_msgbox_active(msgbox))
        {
            if (msgbox == -1)
            {
                hud_ui_multiplayer_scoreboard_exit();
            }
        }
        else
        {
            Mp_msgbox_handle = -1;
            hud_ui_multiplayer_scoreboard_exit();
        }
    }
    if (Player_data_group == nullptr)
    {
        return;
    }
    const multi_game_data* my_data = multi_game_data_get_my_data(NUM_SESSION_TYPES, false, false);
    if (my_data == nullptr)
    {
        return;
    }
    unsigned int players[32];
    int num_players = 0;
    int first_side = 0;
    int second_side = 0;
    if (multi_gameplay_mode_is_team_mode(multi_gameplay_mode_get()))
    {
        multi_game_stats_sort_players_based_on_xp(local_info.team, players, first_side);
        unsigned int others[16];
        multi_game_stats_sort_players_based_on_xp(multi_game_mode_get_opposite_team(local_info.team), others, second_side);
        memcpy(players + first_side, others, second_side * sizeof(unsigned int));
        num_players = second_side + first_side;
    }
    else
    {
        multi_game_stats_sort_players_based_on_score(HUMAN_TEAM_NONE, players, num_players);
        first_side = (num_players + 1) / 2;
        second_side = num_players - first_side;
    }
    int selected_side = second_side;
    int other_side = first_side;
    if (Scoreboard_selection_first_side)
    {
        selected_side = first_side;
        other_side = second_side;
    }
    if (selected_side < 1)
    {
        Scoreboard_selection_first_side = !Scoreboard_selection_first_side;
        selected_side = other_side;
        other_side = 0;
    }
    bool switched_side = false;
    bool clicked = false;
    if (Multiplayer && Scoreboard_active)
    {
        if (mouse_just_released(0))
        {
            vint_document* doc = ui_get_vint_document(Multi_doc_handles[HMD_SCOREBOARD]);
            char name[0x40] = {};
            for (int i = 0; i < first_side; i++)
            {
                if (i == 0)
                {
                    keen::formatString(name, 0x40, "%s", "scoreboard_t1_row");
                }
                else
                {
                    keen::formatString(name, 0x40, "%s%d", "scoreboard_t1_row", i + 1);
                }
                if (ui_is_mouse_over_vint_object(vint_object_base::find_object(name, doc)))
                {
                    Scoreboard_selection_first_side = true;
                    clicked = true;
                    Scoreboard_selection_index = i;
                }
            }
            for (int i = 0; i < second_side; i++)
            {
                if (i == 0)
                {
                    keen::formatString(name, 0x40, "%s", "scoreboard_t2_row");
                }
                else
                {
                    keen::formatString(name, 0x40, "%s%d", "scoreboard_t2_row", i + 1);
                }
                if (ui_is_mouse_over_vint_object(vint_object_base::find_object(name, doc)))
                {
                    Scoreboard_selection_first_side = false;
                    clicked = true;
                    Scoreboard_selection_index = i;
                }
            }
        }
        const bool up = mp_hud_button_pressed(Scoreboard_input_id, BC_UP);
        const bool down = mp_hud_button_pressed(Scoreboard_input_id, BC_DOWN);
        const bool left = mp_hud_button_pressed(Scoreboard_input_id, BC_LEFT);
        const bool right = mp_hud_button_pressed(Scoreboard_input_id, BC_RIGHT);
        if (up || down || left || right)
        {
            if (up)
            {
                Scoreboard_selection_index--;
            }
            else if (down)
            {
                Scoreboard_selection_index++;
            }
            else if (other_side > 0)
            {
                Scoreboard_selection_first_side = !Scoreboard_selection_first_side;
                switched_side = true;
                selected_side = Scoreboard_selection_first_side ? first_side : second_side;
            }
            audiolib_cue_play(Ga_cue_ids.ui_sound_move_up, nullptr, nullptr);
        }
    }
    if (Scoreboard_selection_index < 0)
    {
        Scoreboard_selection_index = selected_side - 1;
    }
    else if (selected_side <= Scoreboard_selection_index)
    {
        Scoreboard_selection_index = switched_side ? selected_side - 1 : 0;
    }
    const multi_game_data* selected = nullptr;
    vint_data_item* item = nullptr;
    const bool has_teams = multi_game_mode_has_teams();
    if (num_players < 1)
    {
        Selected_player_can_view_gamercard = false;
        return;
    }
    for (int i = 0; i < num_players; i++)
    {
        const multi_game_data* mgd = multi_game_data_get(players[i], false, false);
        if (mgd == nullptr)
        {
            continue;
        }
        item = item == nullptr ? Player_data_group->first_data_item() : Player_data_group->next_data_item(item);
        if (item == nullptr)
        {
            break;
        }
        const multi_player_stats* stats = multi_game_stats_get_player_stats(mgd->handle);
        if (stats == nullptr)
        {
            continue;
        }
        bool is_selected = false;
        if (i < first_side)
        {
            if (Scoreboard_selection_first_side && i == Scoreboard_selection_index)
            {
                is_selected = true;
                selected = mgd;
            }
        }
        else if (!Scoreboard_selection_first_side && i - first_side == Scoreboard_selection_index)
        {
            is_selected = true;
            selected = mgd;
        }
        char button[0x20];
        char encoded[0xC0];
        controls_get_button_image_name(button, 0x20, ui_get_control_button_value(BC_SELECT), Player_controller_index);
        vint_encode_wide_text(encoded, 0xC0, mgd->display_name);
        const int team = has_teams ? (int)mgd->team : (first_side <= i ? 1 : 0);
        item->set_element(0, vint_variant(i));
        item->set_element(1, vint_variant(Game_session->get_player_connection(mgd->player_uid) != nullptr));
        item->set_element(2, vint_variant(mgd == my_data));
        item->set_element(3, vint_variant(is_selected));
        item->set_element(4, vint_variant((const char*)button));
        item->set_element(5, vint_variant((int)mgd->player_badge));
        item->set_element(6, vint_variant((const char*)encoded));
        item->set_element(7, vint_variant(team));
        item->set_element(8, vint_variant((int)mgd->player_level));
        item->set_element(9, vint_variant(true));
        item->set_element(10, vint_variant((int)stats->score));
        item->set_element(11, vint_variant(!has_teams));
        item->set_element(12, vint_variant((int)stats->xp));
        item->set_element(13, vint_variant(true));
        item->set_element(14, vint_variant(stats->kills));
        item->set_element(15, vint_variant(true));
        item->set_element(16, vint_variant(stats->deaths));
        item->set_element(17, vint_variant(true));
    }
    if (selected == nullptr || !selected->player_uid.is_official() || !selected->player_uid.is_online())
    {
        Selected_player_can_view_gamercard = false;
        return;
    }
    Selected_player_can_view_gamercard = true;
    if (!Multiplayer || !Scoreboard_active)
    {
        return;
    }
    if (mp_hud_button_pressed(Scoreboard_input_id, BC_SELECT) || clicked)
    {
        Button_info.binfo[BC_SELECT].m_button_wait_for_up = true;
        Choice_last = -1;
        if (!multi_core_platform_ui_is_active())
        {
            const bool same_team = !has_teams || selected->team == my_data->team;
            multi_player_menu_system_pull_up_player_option_message_box(selected->player_uid, selected->display_name, same_team);
        }
    }
}

// 0x008B2670
static bool should_render_multiplayer_doc_scoreboard(int doc_handle)
{
    return hud_ui_multiplayer_scoreboard_is_active();
}

// 0x00897310
static void shutdown_multiplayer_doc_scoreboard(int doc_handle)
{
    Player_data_group = nullptr;
}

// 0x008F4E50
static void process_multiplayer_doc_info(int doc_handle)
{
    if (!Info_text_active && controls_is_button_down(CBA_GAC_HELP, Player_controller_index))
    {
        if (!Info_text_timer.valid())
        {
            // Magic number: held for half a second.
            Info_text_timer.set(500);
            return;
        }
        if (Info_text_timer.elapsed())
        {
            hud_ui_multiplayer_info_text_enter();
            Info_text_timer.invalidate();
        }
        return;
    }
    Info_text_timer.invalidate();
    if (Info_text_active && (mp_hud_button_pressed(Info_text_input_id, BC_SELECT) || controls_is_button_just_pressed(CBA_GAC_HELP, Player_controller_index)))
    {
        hud_ui_multiplayer_info_text_exit();
    }
}

// 0x008B2680
static bool should_render_multiplayer_doc_info(int doc_handle)
{
    return hud_ui_multiplayer_info_text_is_active();
}

// 0x00897320
static void init_multiplayer_data_item_info_title(vint_data_item* item)
{
    const vint_variant_type types[2] = {VINT_PROP_TYPE_STRING, VINT_PROP_TYPE_STRING};
    item->set_template(types, 2);
}

// 0x00897350
static void process_multiplayer_data_item_info_title(vint_data_item* item)
{
    item->set_element(0, vint_variant(multi_gameplay_mode_get_display_name(multi_gameplay_mode_get())));
    item->set_element(1, vint_variant("HM_HOW_TO_PLAY"));
}

// 0x008973C0
static void init_multiplayer_data_item_info_text(vint_data_item* item)
{
}

// 0x008973D0
static void process_multiplayer_data_item_info_text(vint_data_item* item)
{
    const multi_game_mode_info* info = multi_game_mode_get_info(multi_gameplay_mode_get());
    if (info != nullptr)
    {
        for (int i = 0; i < 3; i++)
        {
            item->set_element(i, vint_variant((const char*)info->info_text[i]));
        }
        item->set_element(3, vint_variant(true));
    }
}

// 0x00897460
static void init_multiplayer_data_item_info_hints(vint_data_item* item)
{
    const vint_variant_type types[2] = {VINT_PROP_TYPE_STRING, VINT_PROP_TYPE_STRING};
    item->set_template(types, 2);
}

// 0x00897490
static void process_multiplayer_data_item_info_hints(vint_data_item* item)
{
    char button[0x20];
    controls_get_button_image_name(button, 0x20, CBA_MENU_SELECT, Player_controller_index);
    item->set_element(0, vint_variant((const char*)button));
    item->set_element(1, vint_variant("MENU_ACCEPT"));
}

// 0x00897530
static void init_multiplayer_data_item_main(vint_data_item* item)
{
    const vint_variant_type types[5] = {VINT_PROP_TYPE_STRING, VINT_PROP_TYPE_STRING, VINT_PROP_TYPE_INT, VINT_PROP_TYPE_INT, VINT_PROP_TYPE_INT};
    item->set_template(types, 5);
    char name[0x44];
    multi_gameplay_mode_get_decorated_name(name);
    item->set_element(0, vint_variant((const char*)name));
    item->set_element(1, vint_variant("HM_ROUND_COLON"));
    item->set_element(2, vint_variant((int)Multi_match_info.current_round));
    item->set_element(3, vint_variant((int)Multi_match_info.num_rounds));
    item->set_element(4, vint_variant((int)Mp_server_options.generic_options.point_limit));
}

// 0x00897670
static void process_multiplayer_data_item_main(vint_data_item* item)
{
}

// 0x00897680
static void init_multiplayer_data_item_xp(vint_data_item* item)
{
    const vint_variant_type types[5] = {VINT_PROP_TYPE_STRING, VINT_PROP_TYPE_INT, VINT_PROP_TYPE_BOOL, VINT_PROP_TYPE_INT, VINT_PROP_TYPE_INT};
    item->set_template(types, 5);
}

// 0x008976D0
static void process_multiplayer_data_item_xp(vint_data_item* item)
{
}

// 0x008976E0
static void init_multiplayer_data_item_timer(vint_data_item* item)
{
    const vint_variant_type type = VINT_PROP_TYPE_STRING;
    item->set_template(&type, 1);
}

// 0x008B2690
static void process_multiplayer_data_item_timer(vint_data_item* item)
{
    int time_ms = multi_gameplay_mode_get_time_ms();
    if (time_ms < 1)
    {
        time_ms = 0;
    }
    const int minutes = (time_ms / 1000) / 60;
    const int seconds = (time_ms / 1000) % 60;
    if (Last_displayed_mp_second != seconds && Multi_match_info.match_state == MGMS_GAMEPLAY)
    {
        if (minutes == 0 && seconds < 6)
        {
            gameplay_play_sfx(GSFX_LOW_TIME_TICK);
        }
        else if (minutes == 0 && seconds == 10)
        {
            gameplay_play_sfx(GSFX_TEN_SECONDS_REMAINING);
        }
        else if (seconds == 0 && minutes == 1)
        {
            gameplay_play_sfx(GSFX_ONE_MINUTE_REMAINING);
        }
    }
    Last_displayed_mp_second = seconds;
    char text[20];
    sprintf(text, "%d:%.2d", minutes, seconds);
    item->set_element(0, vint_variant((const char*)text));
}

// 0x00897700
static void init_multiplayer_data_item_mode_info(vint_data_item* item)
{
}

// 0x008E9C70
static void process_multiplayer_data_item_mode_info(vint_data_item* item)
{
    if (multi_game_spectating())
    {
        console_change_chat_window_pos(VCP_ENDGAME);
    }
    if (Multi_match_info.match_state == MGMS_COUNTDOWN)
    {
        item->set_element(0, vint_variant(true));
        item->set_element(1, vint_variant(true));
        // Original bug kept: the level is not checked here (it is at the end of a round below).
        item->set_element(2, vint_variant((const char*)Mp_server_options.get_level()->display_name));
        char objective[0x80] = {};
        multi_gameplay_mode_get_objective_description(objective);
        wchar_t wide[0x100];
        if (localization_does_tag_exist(objective))
        {
            wcsncpy(wide, localization_get_display_string_req(objective, nullptr), 0x80);
            wide[0x7F] = L'\0';
        }
        else
        {
            copy_to_wide(wide, objective, 0x80);
        }
        char encoded[0x300];
        vint_encode_wide_text(encoded, 0x300, wide);
        item->set_element(3, vint_variant((const char*)encoded));
    }
    else if (Multi_match_info.match_state == MGMS_VIEWING_RESULTS)
    {
        item->set_element(0, vint_variant(true));
        item->set_element(1, vint_variant(false));
        wchar_t wide[0x100] = {};
        const multi_level_info* level = Mp_server_options.get_level();
        if (level != nullptr)
        {
            if (localization_does_tag_exist(level->display_name))
            {
                wcsncpy(wide, localization_get_display_string_req(level->display_name, nullptr), 0x100);
            }
            else
            {
                copy_to_wide(wide, level->display_name, 0x100);
            }
        }
        char encoded[0x300];
        vint_encode_wide_text(encoded, 0x300, wide);
        item->set_element(2, vint_variant((const char*)encoded));
        const bool last_round = multi_game_mode_is_last_round();
        const char* result;
        if (multi_game_spectating())
        {
            result = "HM_MATCH_OVER";
        }
        else
        {
            const multi_game_status status = multi_game_mode_get_mode_status(!last_round);
            result = status == MGS_WIN ? "HM_YOU_WON" : (status == MGS_LOSE ? "HM_YOU_LOST" : "HM_YOU_TIED");
        }
        item->set_element(3, vint_variant(last_round ? result : "HM_HALF_OVER"));
        item->set_element(4, vint_variant(0.0f));
    }
    else if (Multi_match_info.match_state == MGMS_PREGAME)
    {
        item->set_element(0, vint_variant(true));
        item->set_element(1, vint_variant(true));
        item->set_element(2, vint_variant((const char*)Mp_server_options.get_level()->display_name));
        item->set_element(3, vint_variant("HM_WAIT_FOR_PLAYERS"));
    }
    else
    {
        item->set_element(0, vint_variant(false));
    }
}

// 0x00897710
static void init_multiplayer_data_item_hints(vint_data_item* item)
{
    const vint_variant_type types[2] = {VINT_PROP_TYPE_BOOL, VINT_PROP_TYPE_STRING};
    item->set_template(types, 2);
}

// 0x008EA070
static void process_multiplayer_data_item_hints(vint_data_item* item)
{
    char encoded[0x400];
    if (Multiplayer && Scoreboard_active)
    {
        char select[0x20];
        char scoreboard[0x20];
        char left[0x20];
        char right[0x20];
        char up[0x20];
        char down[0x20];
        controls_get_button_image_name(select, 0x20, CBA_MENU_SELECT, Player_controller_index);
        controls_get_button_image_name(scoreboard, 0x20, CBA_GAC_SCOREBOARD, Player_controller_index);
        controls_get_button_image_name(left, 0x20, CBA_MENU_LEFT, Player_controller_index);
        controls_get_button_image_name(right, 0x20, CBA_MENU_RIGHT, Player_controller_index);
        controls_get_button_image_name(up, 0x20, CBA_MENU_UP, Player_controller_index);
        controls_get_button_image_name(down, 0x20, CBA_MENU_DOWN, Player_controller_index);
        wchar_t wselect[0x20];
        wchar_t wscoreboard[0x20];
        wchar_t wleft[0x20];
        wchar_t wright[0x20];
        wchar_t wup[0x20];
        wchar_t wdown[0x20];
        copy_to_wide(wselect, select, 0x20);
        copy_to_wide(wscoreboard, scoreboard, 0x20);
        copy_to_wide(wleft, left, 0x20);
        copy_to_wide(wright, right, 0x20);
        copy_to_wide(wup, up, 0x20);
        copy_to_wide(wdown, down, 0x20);
        wchar_t hints[0x200];
        const wchar_t* close_text = localization_get_display_string_opt("HUD_CLOSE_SCOREBOARD");
        const wchar_t* move_text = localization_get_display_string_opt("MENU_MOVE_CURSOR");
        if (!Selected_player_can_view_gamercard)
        {
            _snprintf_safe(hints, 0x200, "[image:%ls][image:%ls][image:%ls][image:%ls] %ls  [image:%ls] %ls", wleft, wright, wup, wdown, move_text, wscoreboard, close_text);
        }
        else
        {
            const wchar_t* options_text = localization_get_display_string_opt("MENU_MBOX_GAMER_OPTIONS");
            _snprintf_safe(hints, 0x200, "[image:%ls][image:%ls][image:%ls][image:%ls] %ls  [image:%ls] %ls  [image:%ls] %ls", wleft, wright, wup, wdown, move_text, wselect, options_text, wscoreboard, close_text);
        }
        vint_encode_wide_text(encoded, 0x400, hints);
        item->set_element(0, vint_variant(true));
        item->set_element(1, vint_variant((const char*)encoded));
        return;
    }
    bool show = multi_game_camera_is_running();
    if (multi_game_spectating())
    {
        show = multi_game_camera_is_spectator_hud_visible();
    }
    if (show)
    {
        multi_game_camera_get_control_hints(encoded, 0x400);
        item->set_element(0, vint_variant(true));
        item->set_element(1, vint_variant((const char*)encoded));
    }
    else
    {
        item->set_element(0, vint_variant(false));
        item->set_element(1, vint_variant(""));
    }
}

// 0x00897740
static void init_multiplayer_data_item_respawn(vint_data_item* item)
{
    const vint_variant_type types[3] = {VINT_PROP_TYPE_BOOL, VINT_PROP_TYPE_STRING, VINT_PROP_TYPE_STRING};
    item->set_template(types, 3);
}

// 0x008B2780
static void process_multiplayer_data_item_respawn(vint_data_item* item)
{
    player* pp = player_get_local_player();
    if (pp == nullptr)
    {
        return;
    }
    if (Multi_match_info.match_state == MGMS_GAMEPLAY && pp->is_killed())
    {
        item->set_element(0, vint_variant(true));
        // Magic number: the respawn shows 750 ms longer than the wait.
        int time_ms = pp->mp_time_until_player_can_respawn.time_until() + 750;
        if (time_ms < 1)
        {
            time_ms = 0;
        }
        char text[20];
        sprintf(text, "%d", (time_ms / 1000) % 60);
        item->set_element(1, vint_variant("HM_RESPAWN_IN"));
        item->set_element(2, vint_variant((const char*)text));
    }
    else
    {
        item->set_element(0, vint_variant(false));
    }
}

// 0x00897790
static void init_multiplayer_data_item_stamp(vint_data_item* item)
{
    const vint_variant_type types[2] = {VINT_PROP_TYPE_BOOL, VINT_PROP_TYPE_STRING};
    item->set_template(types, 2);
}

// 0x008977C0
static void init_multiplayer_data_item_objective(vint_data_item* item)
{
}

// 0x008977D0
static void init_multiplayer_data_item_engineer(vint_data_item* item)
{
}

// 0x008977E0 (LTCG: item in ECX)
static void standard_free_for_all_init(vint_data_item* item)
{
    const vint_variant_type types[6] = {VINT_PROP_TYPE_BOOL, VINT_PROP_TYPE_STRING, VINT_PROP_TYPE_INT, VINT_PROP_TYPE_BOOL, VINT_PROP_TYPE_STRING, VINT_PROP_TYPE_INT};
    item->set_template(types, 6);
}

// 0x00897840 (LTCG: item in ESI): the leader against the local player (or the second when the local player leads).
static void standard_free_for_all_process(vint_data_item* item)
{
    unsigned int players[32];
    int num_players = 0;
    multi_game_stats_sort_players_based_on_round_score(HUMAN_TEAM_NONE, players, num_players);
    if (num_players == 0)
    {
        item->set_element(0, vint_variant(false));
        item->set_element(1, vint_variant(""));
        item->set_element(2, vint_variant(0));
        item->set_element(3, vint_variant(false));
        item->set_element(4, vint_variant(""));
        item->set_element(5, vint_variant(0));
        return;
    }
    unsigned int my_handle = 0xFFFFFFFF;
    unsigned int other = players[1];
    if (player_get_local_player() == nullptr || (my_handle = multi_game_data_get_my_handle(NUM_SESSION_TYPES)) == 0xFFFFFFFF || players[0] == my_handle)
    {
        // Original quirk kept: with more players than one the second entry stays.
        if (num_players == 1)
        {
            other = players[0];
        }
    }
    else
    {
        other = my_handle;
    }
    item->set_element(0, vint_variant(players[0] == my_handle));
    char encoded[0xC0];
    get_encoded_display_name_from_from_multi_game_data(multi_game_data_get(players[0], false, false), 0xC0, encoded, L"MENU_UNKNOWN");
    item->set_element(1, vint_variant((const char*)encoded));
    const multi_player_stats* stats = multi_game_stats_get_player_stats(players[0]);
    item->set_element(2, vint_variant(stats != nullptr ? (int)stats->round_score : 0));
    item->set_element(3, vint_variant(other == my_handle));
    get_encoded_display_name_from_from_multi_game_data(multi_game_data_get(other, false, false), 0xC0, encoded, L"MENU_UNKNOWN");
    item->set_element(4, vint_variant((const char*)encoded));
    stats = multi_game_stats_get_player_stats(other);
    item->set_element(5, vint_variant(stats != nullptr ? (int)stats->round_score : 0));
}

// 0x00897B40 (LTCG: item in ECX)
static void standard_team_init(vint_data_item* item)
{
    const vint_variant_type types[3] = {VINT_PROP_TYPE_INT, VINT_PROP_TYPE_INT, VINT_PROP_TYPE_INT};
    item->set_template(types, 3);
}

// 0x00897B80 (LTCG: item in EDI)
static void standard_team_process(vint_data_item* item)
{
    const multi_game_data* mgd = multi_game_data_get_my_data(NUM_SESSION_TYPES, false, false);
    if (mgd != nullptr)
    {
        item->set_element(0, vint_variant((int)mgd->team));
    }
    for (int i = 1; i <= 2; i++)
    {
        const multi_team_stats* stats = multi_game_stats_get_team_stats((human_teams)(i - HUMAN_TEAM_EDF));
        item->set_element(i, vint_variant(stats != nullptr ? (int)stats->score : 0));
    }
}

// 0x00897C20
static void init_multiplayer_data_item_scoreboard_team_header(vint_data_item* item)
{
    const vint_variant_type types[7] = {VINT_PROP_TYPE_BOOL, VINT_PROP_TYPE_STRING, VINT_PROP_TYPE_STRING, VINT_PROP_TYPE_STRING, VINT_PROP_TYPE_INT, VINT_PROP_TYPE_INT, VINT_PROP_TYPE_INT};
    item->set_template(types, 7);
    item->set_element(0, vint_variant(false));
    if (multi_game_mode_has_teams())
    {
        char guerilla[0x80] = {};
        char edf[0x80] = {};
        vint_encode_wide_text(guerilla, 0x80, human_team_get_display_name(HUMAN_TEAM_GUERILLA));
        vint_encode_wide_text(edf, 0x80, human_team_get_display_name(HUMAN_TEAM_EDF));
        char name[0x40];
        multi_gameplay_mode_get_decorated_name(name);
        item->set_element(0, vint_variant(true));
        item->set_element(1, vint_variant((const char*)name));
        item->set_element(2, vint_variant((const char*)guerilla));
        item->set_element(3, vint_variant((const char*)edf));
        item->set_element(4, vint_variant((int)Multi_match_info.current_round));
        item->set_element(5, vint_variant((int)Multi_match_info.num_rounds));
        item->set_element(6, vint_variant((int)Mp_server_options.generic_options.point_limit));
    }
}

// 0x00897E60
static void process_multiplayer_data_item_scoreboard_team_header(vint_data_item* item)
{
}

// 0x00897E70
static void init_multiplayer_data_item_scoreboard_ffa_header(vint_data_item* item)
{
    const vint_variant_type types[7] = {VINT_PROP_TYPE_BOOL, VINT_PROP_TYPE_STRING, VINT_PROP_TYPE_UINT, VINT_PROP_TYPE_STRING, VINT_PROP_TYPE_INT, VINT_PROP_TYPE_INT, VINT_PROP_TYPE_STRING};
    item->set_template(types, 7);
    item->set_element(0, vint_variant(false));
}

// 0x008B28C0
static void process_multiplayer_data_item_scoreboard_ffa_header(vint_data_item* item)
{
    if (multi_game_mode_has_teams())
    {
        return;
    }
    const multi_game_data* mgd = multi_game_data_get_my_data(NUM_SESSION_TYPES, false, false);
    if (mgd == nullptr)
    {
        return;
    }
    const multi_player_stats* stats = multi_game_stats_get_player_stats(mgd->handle);
    if (stats == nullptr)
    {
        return;
    }
    unsigned int badge;
    multi_game_badge_get_my_badge(badge);
    char encoded[0xC0];
    vint_encode_wide_text(encoded, 0xC0, mgd->display_name);
    unsigned int players[16];
    int num_players = 0;
    multi_game_stats_sort_players_based_on_round_score(HUMAN_TEAM_NONE, players, num_players);
    const int rank = find_in_array<unsigned int>(mgd->handle, players, num_players);
    char name[0x40];
    multi_gameplay_mode_get_decorated_name(name);
    item->set_element(0, vint_variant(true));
    item->set_element(1, vint_variant((const char*)name));
    item->set_element(2, vint_variant(badge));
    item->set_element(3, vint_variant((const char*)encoded));
    item->set_element(4, vint_variant((int)stats->round_score));
    item->set_element(5, vint_variant(rank + 1));
    item->set_element(6, vint_variant("MENU_RANK"));
}

// 0x00897F00
static void init_multiplayer_data_item_scoreboard_score(vint_data_item* item)
{
    const vint_variant_type types[4] = {VINT_PROP_TYPE_INT, VINT_PROP_TYPE_INT, VINT_PROP_TYPE_INT, VINT_PROP_TYPE_INT};
    item->set_template(types, 4);
}

// 0x00897F50
static void process_multiplayer_data_item_scoreboard_score(vint_data_item* item)
{
    const multi_team_stats* guerilla = multi_game_stats_get_team_stats(HUMAN_TEAM_GUERILLA);
    const multi_team_stats* edf = multi_game_stats_get_team_stats(HUMAN_TEAM_EDF);
    if (guerilla != nullptr && edf != nullptr)
    {
        item->set_element(0, vint_variant((int)guerilla->score));
        item->set_element(1, vint_variant((int)edf->score));
        item->set_element(2, vint_variant(guerilla->round_wins));
        item->set_element(3, vint_variant(edf->round_wins));
    }
}

// 0x00898040
static void init_multiplayer_data_item_specator(vint_data_item* item)
{
    const vint_variant_type types[9] = {VINT_PROP_TYPE_BOOL, VINT_PROP_TYPE_STRING, VINT_PROP_TYPE_STRING, VINT_PROP_TYPE_INT, VINT_PROP_TYPE_BOOL, VINT_PROP_TYPE_STRING, VINT_PROP_TYPE_STRING, VINT_PROP_TYPE_STRING, VINT_PROP_TYPE_STRING};
    item->set_template(types, 9);
}

// 0x008EA4F0
static void process_multiplayer_data_item_spectator(vint_data_item* item)
{
    if (!multi_game_spectating() || !multi_game_camera_is_spectator_hud_visible() || hud_ui_multiplayer_splash_screen_is_active())
    {
        item->set_element(0, vint_variant(false));
        item->set_element(1, vint_variant(""));
        item->set_element(2, vint_variant(""));
        item->set_element(3, vint_variant(0));
        return;
    }
    char image[0x20] = {};
    char mode_name[0x180];
    if (control_is_controller_in_use())
    {
        controls_get_dpad_image_name(image, 0x20);
        vint_encode_wide_text(mode_name, 0x180, multi_game_camera_get_camera_mode_name());
        item->set_element(0, vint_variant(true));
        item->set_element(1, vint_variant((const char*)image));
        item->set_element(2, vint_variant((const char*)mode_name));
        item->set_element(3, vint_variant((int)multi_game_camera_get_mode()));
        item->set_element(4, vint_variant(false));
        item->set_element(5, vint_variant(""));
        item->set_element(6, vint_variant(""));
        item->set_element(7, vint_variant(""));
        item->set_element(8, vint_variant(""));
    }
    else
    {
        vint_encode_wide_text(mode_name, 0x180, multi_game_camera_get_camera_mode_name());
        item->set_element(0, vint_variant(true));
        item->set_element(1, vint_variant(""));
        item->set_element(2, vint_variant((const char*)mode_name));
        item->set_element(3, vint_variant((int)multi_game_camera_get_mode()));
        item->set_element(4, vint_variant(true));
        controls_get_button_image_name(image, 0x20, CBA_SPECTATOR_MODE_CUSTOM, Player_controller_index);
        item->set_element(5, vint_variant((const char*)image));
        controls_get_button_image_name(image, 0x20, CBA_SPECTATOR_MODE_AQUARIUM, Player_controller_index);
        item->set_element(6, vint_variant((const char*)image));
        controls_get_button_image_name(image, 0x20, CBA_SPECTATOR_MODE_PLAYER, Player_controller_index);
        item->set_element(7, vint_variant((const char*)image));
        controls_get_button_image_name(image, 0x20, CBA_SPECTATOR_MODE_ACTION, Player_controller_index);
        item->set_element(8, vint_variant((const char*)image));
    }
}

// 0x00916D80
void hud_ui_multiplayer_enter()
{
    for (int& handle : Multi_doc_handles)
    {
        handle = -1;
    }
    memset(Multi_data_item_handles, 0, sizeof(Multi_data_item_handles));
    Multi_mode_data_item_handle = 0;
    Assets_loaded_uimulti = false;
    Data_items_created_uimulti = false;
    Active_uimulti = false;
    Scoreboard_active = false;
    Info_text_active = false;
    hud_ui_multiplayer_load_items(false);
    New_backpack_timestamp.invalidate();
    const rl_screen_info* screen = rl_render_lib::get_instance()->get_screen_info();
    // Magic numbers: the kill messages' line 4 pixels below the safe area, their shadows 2 pixels on.
    KILL_MESSAGE_START_X = (float)gr_safe_area_left() / (float)screen->get_screen_width();
    KILL_MESSAGE_START_Y = (float)(gr_safe_area_top() + 4) / (float)screen->get_screen_height();
    KILL_MESSAGE_SHADOW_START_X = (float)(gr_safe_area_left() + 2) / (float)screen->get_screen_width();
    KILL_MESSAGE_SHADOW_START_Y = (float)(gr_safe_area_top() + 6) / (float)screen->get_screen_height();
}

// 0x00904BA0
void hud_ui_multiplayer_exit()
{
    hud_ui_multiplayer_unload_items(false);
}

// 0x00910A60
void hud_ui_multiplayer_load_items(bool assets_only)
{
    if (Active_uimulti)
    {
        return;
    }
    const multi_gameplay_mode mode = multi_gameplay_mode_get();
    for (int i = 0; i < NUM_HUD_MULTIPLAYER_DOCS; i++)
    {
        if (Multi_doc_info[i].mode == mode || Multi_doc_info[i].mode == MGPM_NONE)
        {
            if (!assets_only)
            {
                Multi_doc_handles[i] = ui_load_document(Multi_doc_info[i].name, DFIT_ALPHA, 0.5f, DLT_NORMAL, false, STREAM_PRIORITY_AUDIO_CACHE_LOAD, false);
            }
            else if (Multi_doc_handles[i] != -1)
            {
                ui_reload_doc_assets(Multi_doc_handles[i]);
            }
        }
    }
    Assets_loaded_uimulti = false;
    Active_uimulti = true;
}

// 0x008F4F70
void hud_ui_multiplayer_unload_items(bool assets_only)
{
    if (!Active_uimulti && assets_only)
    {
        return;
    }
    hud_ui_multiplayer_scoreboard_exit();
    hud_ui_multiplayer_info_text_exit();
    for (int i = 0; i < NUM_HUD_MULTIPLAYER_DOCS; i++)
    {
        if (Multi_doc_handles[i] == -1)
        {
            continue;
        }
        if (assets_only)
        {
            ui_unload_doc_assets(Multi_doc_handles[i]);
        }
        else
        {
            if (Multi_doc_info[i].shutdown != nullptr)
            {
                Multi_doc_info[i].shutdown(Multi_doc_handles[i]);
            }
            ui_unload_document(Multi_doc_handles[i], DFOT_NONE, 0.5f);
            Multi_doc_handles[i] = -1;
        }
    }
    if (!assets_only)
    {
        for (int i = 0; i < Multi_kill_messages.size(); i++)
        {
            if (Multi_kill_messages[i] != INVALID_HUD_MESSAGE_HANDLE)
            {
                hud_message_remove_user(Multi_kill_messages[i]);
            }
        }
        Multi_kill_messages.remove_all();
        for (int i = 0; i < Multi_kill_message_shadows.size(); i++)
        {
            if (Multi_kill_message_shadows[i] != INVALID_HUD_MESSAGE_HANDLE)
            {
                hud_message_remove_user(Multi_kill_message_shadows[i]);
            }
        }
        Multi_kill_message_shadows.remove_all();
        Data_items_created_uimulti = false;
    }
    Active_uimulti = false;
}

// 0x008F50B0
void hud_ui_multiplayer_process(float dtime)
{
    if (Scoreboard_active)
    {
        mp_game_input_process_chatmouse();
    }
    if (!Assets_loaded_uimulti && Active_uimulti)
    {
        Assets_loaded_uimulti = true;
        const multi_gameplay_mode mode = multi_gameplay_mode_get();
        for (int i = 0; i < NUM_HUD_MULTIPLAYER_DOCS; i++)
        {
            if (Multi_doc_info[i].mode == mode || Multi_doc_info[i].mode == MGPM_NONE)
            {
                switch (ui_get_doc_asset_state(Multi_doc_handles[i]))
                {
                    case ST_UNLOADED:
                    case ST_ERROR:
                        hud_ui_multiplayer_unload_items(false);
                        break;
                    case ST_LOADING:
                        Assets_loaded_uimulti = false;
                        break;
                    default:
                        break;
                }
            }
        }
        if (Assets_loaded_uimulti && !Data_items_created_uimulti)
        {
            Data_items_created_uimulti = true;
            for (int i = 0; i < NUM_HUD_MULTIPLAYER_DOCS; i++)
            {
                if ((Multi_doc_info[i].mode == mode || Multi_doc_info[i].mode == MGPM_NONE) && Multi_doc_info[i].init != nullptr)
                {
                    Multi_doc_info[i].init(Multi_doc_handles[i]);
                }
            }
            for (int i = 0; i < 16; i++)
            {
                const mp_data_item_info& info = Multi_data_item_info[i];
                Multi_data_item_handles[i] = 0;
                if (info.init != nullptr && Multi_doc_handles[info.doc] != -1)
                {
                    vint_data_item* item = vint_data_item::create_new(info.name);
                    if (item != nullptr)
                    {
                        Multi_data_item_handles[i] = item->get_handle();
                        info.init(item);
                        ui_add_data_item(Multi_doc_handles[info.doc], Multi_data_item_handles[i]);
                    }
                }
            }
            vint_data_item* mode_item = vint_data_item::create_new(multi_gameplay_mode_get_hud_data_item_name());
            // Original bug kept: the item is not checked.
            Multi_mode_data_item_handle = mode_item->get_handle();
            const multi_gameplay_mode current = multi_gameplay_mode_get();
            if (!multi_gameplay_mode_hud_init(mode_item))
            {
                if (multi_gameplay_mode_is_team_mode(current))
                {
                    standard_team_init(mode_item);
                }
                else
                {
                    standard_free_for_all_init(mode_item);
                }
            }
            ui_add_data_item(Multi_doc_handles[HMD_MAIN], Multi_mode_data_item_handle);
        }
    }
    if (!Assets_loaded_uimulti && !(game_is_paused() && Data_items_created_uimulti))
    {
        return;
    }
    const multi_gameplay_mode mode = multi_gameplay_mode_get();
    for (int i = 0; i < 16; i++)
    {
        if (Multi_data_item_handles[i] != 0 && Multi_data_item_info[i].process != nullptr)
        {
            vint_data_item* item = vint_data_item::find(Multi_data_item_handles[i]);
            if (item != nullptr)
            {
                Multi_data_item_info[i].process(item);
            }
        }
    }
    if (Multi_mode_data_item_handle != 0)
    {
        vint_data_item* item = vint_data_item::find(Multi_mode_data_item_handle);
        if (item != nullptr && !multi_gameplay_mode_hud_process(item))
        {
            if (multi_gameplay_mode_is_team_mode(mode))
            {
                standard_team_process(item);
            }
            else
            {
                standard_free_for_all_process(item);
            }
        }
    }
    for (int i = 0; i < NUM_HUD_MULTIPLAYER_DOCS; i++)
    {
        if (Multi_doc_handles[i] != -1)
        {
            if (Multi_doc_info[i].process != nullptr)
            {
                Multi_doc_info[i].process(Multi_doc_handles[i]);
            }
            ui_add_for_processing(Multi_doc_handles[i]);
        }
    }
}

// 0x008D8CE0
void hud_ui_multiplayer_render()
{
    if (!Assets_loaded_uimulti)
    {
        return;
    }
    for (int i = 0; i < NUM_HUD_MULTIPLAYER_DOCS; i++)
    {
        if (Multi_doc_handles[i] != -1 && (Multi_doc_info[i].should_render == nullptr || Multi_doc_info[i].should_render(Multi_doc_handles[i])))
        {
            ui_add_for_rendering(Multi_doc_handles[i]);
        }
    }
}

// 0x008980A0
int hud_ui_multiplayer_get_doc_handle(hud_multiplayer_docs doc)
{
    // Unsigned compare (JA at 0x008980A7), so HMD_NONE (-1) answers -1.
    if ((unsigned int)doc < (unsigned int)NUM_HUD_MULTIPLAYER_DOCS)
    {
        return Multi_doc_handles[doc];
    }
    return -1;
}

// 0x008980C0
void hud_ui_multiplayer_show_stamp(const char* message)
{
    vint_data_item* item = vint_data_item::find(Multi_data_item_handles[5]);
    if (item != nullptr)
    {
        item->set_element(0, vint_variant(true));
        item->set_element(1, vint_variant(message));
        item->set_updated();
    }
}

// 0x00898140
void hud_ui_multiplayer_hide_stamp()
{
    vint_data_item* item = vint_data_item::find(Multi_data_item_handles[5]);
    if (item != nullptr)
    {
        item->set_element(0, vint_variant(false));
    }
}

// 0x008B2AC0
void hud_ui_multiplayer_trigger_objective(const wchar_t* title, const wchar_t* label, human_teams team)
{
    color col;
    multi_game_render_get_team_color(col, MTCT_ICON, team);
    hud_ui_multiplayer_trigger_objective(title, label, team, col);
}

// 0x008B2AF0
void hud_ui_multiplayer_trigger_objective(const wchar_t* title, const wchar_t* label, const object* obj)
{
    const human_teams team = human_teams_get_team(obj);
    color col;
    multi_game_render_get_team_color(col, MTCT_ICON, obj);
    hud_ui_multiplayer_trigger_objective(title, label, team, col);
}

// 0x00898180
void hud_ui_multiplayer_trigger_objective(const wchar_t* title, const wchar_t* label, human_teams team, const color& col)
{
    vint_data_item* item = vint_data_item::find(Multi_data_item_handles[6]);
    if (item != nullptr)
    {
        char encoded[0x180];
        vint_encode_wide_text(encoded, 0x180, title);
        item->set_element(0, vint_variant((const char*)encoded));
        vint_encode_wide_text(encoded, 0x180, label);
        item->set_element(1, vint_variant((const char*)encoded));
        item->set_element(2, vint_variant((float)col.red / 255.0f));
        item->set_element(3, vint_variant((float)col.green / 255.0f));
        item->set_element(4, vint_variant((float)col.blue / 255.0f));
        item->set_element(5, vint_variant((int)team));
        item->force_immediate_update();
    }
}

// 0x00898350
void hud_ui_multiplayer_trigger_engineer(const multi_game_engineer_hud* hud_info)
{
    vint_data_item* item = vint_data_item::find(Multi_data_item_handles[7]);
    if (item == nullptr)
    {
        return;
    }
    char encoded[0x180];
    item->set_element(0, vint_variant(true));
    vint_encode_wide_text(encoded, 0x180, hud_info->title);
    item->set_element(1, vint_variant((const char*)encoded));
    item->set_element(2, vint_variant(hud_info->flash_first_line));
    for (int i = 0; i < 3; i++)
    {
        if (hud_info->text[i] == nullptr)
        {
            item->set_element(3 + i * 2, vint_variant((const char*)nullptr));
            item->set_element(3 + i * 2 + 1, vint_variant(0.0f));
        }
        else
        {
            vint_encode_wide_text(encoded, 0x180, hud_info->text[i]);
            item->set_element(3 + i * 2, vint_variant((const char*)encoded));
            item->set_element(3 + i * 2 + 1, vint_variant(hud_info->text_alpha[i]));
        }
    }
    item->force_immediate_update();
}

// 0x00898500
void hud_ui_multiplayer_hide_engineer()
{
    vint_data_item* item = vint_data_item::find(Multi_data_item_handles[7]);
    if (item != nullptr)
    {
        item->set_element(0, vint_variant(false));
    }
}

// 0x008EA960
void hud_ui_multiplayer_scoreboard_enter(bool sticky)
{
    if (!Active_uimulti || Scoreboard_active || hud_ui_multiplayer_splash_screen_is_active())
    {
        return;
    }
    if (multi_game_spectating() && hud_is_hidden() && !multi_game_camera_is_spectator_hud_visible())
    {
        multi_game_camera_show_spectator_hud(true);
    }
    console_change_chat_window_pos(VCP_INGAME_SCOREBOARD);
    audiolib_cue_play(Ga_cue_ids.general_tooltip_pop_up, nullptr, nullptr);
    Scoreboard_active = true;
    Scoreboard_sticky = sticky;
    Scoreboard_selection_first_side = true;
    Scoreboard_selection_index = 0;
    Scoreboard_input_id = ui_register_for_processed_input(IRT_UNLIMITED_INPUT, true);
    if (!Cursor_visible)
    {
        Cursor_visible = true;
        mouse_auto_centering(false);
    }
    control_enable_player_input(false);
    control_enable_player_camera_input(false);
}

// 0x008C6210
void hud_ui_multiplayer_scoreboard_exit()
{
    if (get_console_chat_window_pos() == VCP_INGAME_SCOREBOARD)
    {
        console_change_chat_window_pos(VCP_INGAME);
    }
    audiolib_cue_play(Ga_cue_ids.general_tooltip_pop_up, nullptr, nullptr);
    ui_relinquish_input_id(Scoreboard_input_id);
    Scoreboard_input_id = -1;
    Scoreboard_active = false;
    Scoreboard_sticky = false;
    if (Cursor_visible)
    {
        Cursor_visible = false;
        mouse_auto_centering(true);
    }
    control_enable_player_input(true);
    control_enable_player_camera_input(true);
}

// 0x00898540
bool hud_ui_multiplayer_scoreboard_is_active()
{
    return Multiplayer && Scoreboard_active;
}

// 0x008EAAA0
void hud_ui_multiplayer_info_text_enter()
{
    if (Active_uimulti && !Info_text_active && !hud_ui_multiplayer_splash_screen_is_active() && !multi_game_spectating())
    {
        console_disable_chat_render(true);
        audiolib_cue_play(Ga_cue_ids.general_tooltip_pop_up, nullptr, nullptr);
        Info_text_active = true;
        Info_text_input_id = ui_register_for_processed_input(IRT_UNLIMITED_INPUT, true);
    }
}

// 0x008C6290
void hud_ui_multiplayer_info_text_exit()
{
    audiolib_cue_play(Ga_cue_ids.ui_sound_press_select, nullptr, nullptr);
    ui_relinquish_input_id(Info_text_input_id);
    Info_text_input_id = -1;
    Info_text_active = false;
    console_disable_chat_render(false);
}

// 0x00898550
bool hud_ui_multiplayer_info_text_is_active()
{
    return Multiplayer && Info_text_active;
}

// 0x008D8D30
bool hud_ui_multiplayer_splash_screen_is_active()
{
    return Multiplayer && (Scoreboard_active || Info_text_active || (Utility_doc_handle != -1 && Message_box_info_set.msgbox_info_pool.get_count(0) != 0));
}

// 0x00898560
void hud_ui_multiplayer_player_removed(player* pp)
{
}

// A player's name in a kill message: name, plain and colored with the team color (for the local player: "you").
static void kill_message_player_name(const player* pp, const player* local, wchar_t* colored, wchar_t* plain)
{
    const multi_game_data* mgd = multi_game_data_get(pp->mp_data, false, false);
    if (mgd == nullptr)
    {
        return;
    }
    wcsncpy(plain, mgd->display_name, 0x80);
    plain[0x7F] = L'\0';
    color team_color = color::WHITE;
    multi_game_render_get_team_color(team_color, MTCT_ICON, (const object*)pp);
    char hex[0x20];
    str_color2s_hex(hex, 0x20, team_color, false);
    wchar_t whex[0x20];
    copy_to_wide(whex, hex, 0x20);
    wchar_t name[0x20];
    wcsncpy(name, mgd->display_name, 0x20);
    name[0x1F] = L'\0';
    if (pp == local)
    {
        const wchar_t* you = localization_get_display_string_req("HM_KILL_YOU", nullptr);
        _snprintf_safe(colored, 0x80, L"[color:#%ls]%ls[color:#FFFFFFFF]", whex, you);
        wcsncpy(plain, localization_get_display_string_req("HM_KILL_YOU", nullptr), 0x80);
    }
    else if (human_teams_are_friendly(pp, local) || human_teams_are_enemies(pp, local))
    {
        _snprintf_safe(colored, 0x80, L"[color:#%ls]%ls[color:#FFFFFFFF]", whex, name);
    }
    else
    {
        wcsncpy(colored, name, 0x80);
        colored[0x7F] = L'\0';
    }
}

static void set_kill_phrase_replacements(kill_phrase_token& token, const wchar_t* colored, const wchar_t* plain)
{
    token.colored_replacement = colored;
    token.colored_replacement_len = (int)wcslen(colored);
    token.replacement = plain;
    token.replacement_len = (int)wcslen(plain);
}

// 0x008EAB00
void hud_ui_multiplayer_player_died(player* victim, player* killer, damaged_by_types damage_type, const weapon_info* w_info)
{
    if (victim == nullptr || killer == nullptr)
    {
        return;
    }
    player* local = player_get_local_player();
    if (local == nullptr)
    {
        return;
    }
    if (local == victim)
    {
        hud_ui_multiplayer_show_stamp("HM_YOU_DIED");
        audiolib_cue_play(Ga_cue_ids.multiplayer_player_death, nullptr, nullptr);
    }
    if (local == killer && killer != victim)
    {
        audiolib_cue_play(Ga_cue_ids.multiplayer_player_kills_player, nullptr, nullptr);
    }
    wchar_t victim_colored[0x80];
    wchar_t killer_colored[0x80];
    wchar_t victim_plain[0x80];
    wchar_t killer_plain[0x80];
    wcsncpy(victim_colored, localization_get_display_string_req("HM_KILL_UNKOWN_PAREN", nullptr), 0x80);
    wcsncpy(killer_colored, localization_get_display_string_req("HM_KILL_UNKOWN_PAREN", nullptr), 0x80);
    wcsncpy(victim_plain, localization_get_display_string_req("HM_KILL_UNKOWN_PAREN", nullptr), 0x80);
    wcsncpy(killer_plain, localization_get_display_string_req("HM_KILL_UNKOWN_PAREN", nullptr), 0x80);
    wchar_t icon_colored[0x80];
    wchar_t icon_plain[0x80];
    copy_to_wide(icon_colored, "[image:ui_hud_weapon_icon_sm_skull]", 0x80);
    copy_to_wide(icon_plain, "[image_tint:ui_hud_weapon_icon_sm_skull]", 0x80);
    kill_message_player_name(victim, local, victim_colored, victim_plain);
    kill_message_player_name(killer, local, killer_colored, killer_plain);
    if (damage_type != DBT_CAR && w_info != nullptr)
    {
        const char* icon = w_info->small_icon_name;
        // Magic number: the small icon at full scale, the large one at half.
        float scale = 1.0f;
        if (icon == nullptr)
        {
            icon = w_info->icon_name;
            scale = 0.5f;
        }
        if (icon != nullptr)
        {
            wchar_t wicon[0x80];
            copy_to_wide(wicon, icon, 0x80);
            _snprintf_safe(icon_plain, 0x80, L"[format][scale:%.2f][image_tint:%ls][/format][scale:1.0]", (double)scale, wicon);
            _snprintf_safe(icon_colored, 0x80, L"[format][scale:%.2f][image:%ls][/format][scale:1.0]", (double)scale, wicon);
        }
    }
    char phrase_tag[0x80];
    strncpy(phrase_tag, "HM_KILL_PHRASE_STANDARD", 0x80);
    if (damage_type == DBT_MELEE)
    {
        strncpy(phrase_tag, "HM_KILL_PHRASE_MELEE", 0x80);
    }
    else if (w_info != nullptr && w_info->mp_kill_phrase != nullptr)
    {
        strncpy(phrase_tag, w_info->mp_kill_phrase, 0x80);
    }
    if (local == killer)
    {
        keen::appendString(phrase_tag, 0x80, "_BY_YOU");
    }
    wchar_t phrase[0x80];
    wcsncpy(phrase, localization_get_display_string_req(phrase_tag, nullptr), 0x80);
    const bool victim_is_local = victim == player_get_local_player();
    wchar_t his[0x40];
    wcsncpy(his, localization_get_display_string_req(victim_is_local ? "HM_YOUR" : (victim->h_info->flags.female ? "HM_HER" : "HM_HIS"), nullptr), 0x40);
    wchar_t himself[0x40];
    wcsncpy(himself, localization_get_display_string_req(victim_is_local ? "HM_YOURSELF" : (victim->h_info->flags.female ? "HM_HERSELF" : "HM_HIMSELF"), nullptr), 0x40);

    set_kill_phrase_replacements(Kill_phrase_tokens[0], icon_colored, icon_plain);
    set_kill_phrase_replacements(Kill_phrase_tokens[1], killer_colored, killer_plain);
    set_kill_phrase_replacements(Kill_phrase_tokens[2], killer != victim ? victim_colored : himself, killer != victim ? victim_plain : himself);
    set_kill_phrase_replacements(Kill_phrase_tokens[3], his, his);
    set_kill_phrase_replacements(Kill_phrase_tokens[4], himself, himself);

    // Magic number: the carriage return keeps the text left to right.
    wchar_t colored_buffer[0x200] = {L'\r'};
    wchar_t shadow_buffer[0x200] = L"\r[color:#000000FF]";
    wchar_t temp[0x200];
    const wchar_t* rest = phrase;
    const wchar_t* percent = wcschr(rest, L'%');
    while (percent != nullptr)
    {
        const wchar_t* next = nullptr;
        for (const kill_phrase_token& token : Kill_phrase_tokens)
        {
            if (_v_strnicmp(percent + 1, token.name, token.name_len) == 0)
            {
                const int before = (int)(percent - rest);
                if (before > 0)
                {
                    wcsncpy(temp, rest, before + 1);
                    wcsncat(colored_buffer, temp, before);
                    wcsncat(shadow_buffer, temp, before);
                }
                wcsncpy(temp, token.colored_replacement, token.colored_replacement_len + 1);
                wcsncat(colored_buffer, temp, token.colored_replacement_len);
                wcsncpy(temp, token.replacement, token.replacement_len);
                wcsncat(shadow_buffer, temp, token.replacement_len);
                next = percent + token.name_len + 1;
                break;
            }
        }
        if (next == nullptr)
        {
            // Original bug kept: an unknown token drops the text up to the next space.
            next = wcschr(percent, L' ');
            if (next == nullptr)
            {
                rest = nullptr;
                break;
            }
        }
        rest = next;
        percent = wcschr(rest, L'%');
    }
    if (rest != nullptr && *rest != L'\0')
    {
        wcsncpy(temp, rest, 0x200);
        wcscat(colored_buffer, temp);
        wcscat(shadow_buffer, temp);
    }
    hud_ui_multiplayer_add_player_died_message(colored_buffer, shadow_buffer, local == victim || local == killer);
}

// 0x008C62D0
void hud_ui_multiplayer_add_player_died_message(const wchar_t* colored_buffer, const wchar_t* shadow_buffer, bool outline)
{
    if (Multi_kill_messages.capacity() <= Multi_kill_messages.size())
    {
        hud_message_remove_user(Multi_kill_messages[0]);
        hud_message_remove_user(Multi_kill_message_shadows[0]);
        Multi_kill_messages.remove(0);
        Multi_kill_message_shadows.remove(0);
    }
    // Magic number: a kill message shows for 7 s.
    Multi_kill_message_shadows.add(hud_message_add_user(shadow_buffer, HUMT_MP_KILL_SHADOW, KILL_MESSAGE_SHADOW_START_X, KILL_MESSAGE_SHADOW_START_Y, false, 7.0f));
    Multi_kill_messages.add(hud_message_add_user(colored_buffer, HUMT_MP_KILL, KILL_MESSAGE_START_X, KILL_MESSAGE_START_Y, outline, 7.0f));
}

// 0x008EB530
void hud_ui_multiplayer_local_player_picked_up_backpack()
{
    player* pp = player_get_local_player();
    if (pp != nullptr && pp->backpack != nullptr && Multiplayer)
    {
        const char* name = pp->backpack->get_backpack_info()->pickup_tooltip;
        if (name != nullptr)
        {
            ui_tool_tips_add_tip(name, 0, nullptr);
        }
    }
}

// 0x00898570
void hud_ui_multiplayer_local_player_dropped_backpack()
{
    New_backpack_timestamp.invalidate();
}

// 0x00898580
void hud_ui_multiplayer_local_player_triggered_xp_event(const char* event_name, int xp_amount, bool bonus_event, int total_xp, int total_bonus_xp, bool leveled_up)
{
    if (!audiolib_is_playing(Xp_sound_play_id, true))
    {
        Xp_sound_play_id = audiolib_cue_play(Ga_cue_ids.multiplayer_xp_gain, nullptr, nullptr);
    }
    vint_data_item* item = vint_data_item::find(Multi_data_item_handles[8]);
    if (item != nullptr)
    {
        item->set_element(0, vint_variant(event_name));
        item->set_element(1, vint_variant(xp_amount));
        item->set_element(2, vint_variant(bonus_event));
        item->set_element(3, vint_variant(total_xp));
        item->set_element(4, vint_variant(total_bonus_xp));
        item->set_element(5, vint_variant(leveled_up));
        item->force_immediate_update();
    }
}

// 0x008986E0
void hud_ui_multiplayer_add_backpack_data_item(vint_data_item* item)
{
    if (item != nullptr)
    {
        const vint_variant_type types[6] = {VINT_PROP_TYPE_BOOL, VINT_PROP_TYPE_BOOL, VINT_PROP_TYPE_STRING, VINT_PROP_TYPE_STRING, VINT_PROP_TYPE_STRING, VINT_PROP_TYPE_STRING};
        item->set_template(types, 6);
    }
}

// 0x008D8D80
void hud_ui_multiplayer_process_backpack_data_item(vint_data_item* item)
{
    if (item != nullptr && player_get_local_player() != nullptr)
    {
        item->set_element(0, vint_variant(false));
    }
}

// 0x008EB570
void hud_ui_multiplayer_player_joined(unsigned int game_data)
{
    if (Player_data_group != nullptr)
    {
        Scoreboard_needs_reinit = true;
    }
    const multi_game_data* mgd = multi_game_data_get(game_data, false, false);
    if (mgd == nullptr)
    {
        return;
    }
    color team_color;
    multi_game_render_get_team_color(team_color, MTCT_ICON, mgd->team);
    char hex[0x10];
    str_color2s_hex(hex, 0x10, team_color, false);
    wchar_t whex[0x10];
    copy_to_wide(whex, hex, 0x10);
    wchar_t shadow_name[0x80];
    wchar_t colored_name[0x80];
    _snprintf_safe(shadow_name, 0x80, "[color:#000000FF][image_tint:ui_hud_player_left] %ls", mgd->display_name);
    _snprintf_safe(colored_name, 0x80, "[color:#%ls][image:ui_hud_player_left] %ls[color:#FFFFFFFF]", whex, mgd->display_name);
    wchar_t colored[0x100];
    wchar_t shadow[0x100];
    // Original bug kept: the localized text is not checked for null.
    _snprintf_safe(colored, 0x100, localization_get_display_string_opt("HM_PLAYER_JOINED"), colored_name);
    _snprintf_safe(shadow, 0x100, localization_get_display_string_opt("HM_PLAYER_JOINED"), shadow_name);
    hud_ui_multiplayer_add_player_died_message(colored, shadow, false);
}
