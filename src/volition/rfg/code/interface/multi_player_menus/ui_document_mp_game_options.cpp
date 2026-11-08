// Original: volition/rfg/code/interface/multi_player_menus/ui_document_mp_game_options.cpp
//
// The host's game options (GS_MULTIPLAYER_CHANGE_GAME_OPTIONS, UI.md UI5) on rfg_ui_mp_options: five tabs (basic,
// advanced, mode, weapon locks, backpack locks) of Mp_server_options' options, each row stepped left/right (the
// numeric limits, the cycled enums, the backpack and weapon pickers, the lock bits); accept checks the weapon
// selections, back restores the options as they were (after a confirm box when changed). The lobby keeps running
// underneath; a game link error closes the menu.

#include "volition/rfg/code/interface/multi_player_menus/ui_document_mp_game_options.h"

#include "keen/base/string.hpp"
#include "keen/base/trace.hpp"
#include "kaiko/gamelink/gamelink.hpp"
#include "volition/audio_lib/code/audiolib_api.h"
#include "volition/interface_lib/code/objects/vint_object_base.h"
#include "volition/interface_lib/code/vint_data_group.h"
#include "volition/interface_lib/code/vint_data_item.h"
#include "volition/interface_lib/code/vint_document.h"
#include "volition/interface_lib/code/vint_string_processing.h"
#include "volition/interface_lib/code/vint_variant.h"
#include "volition/rfg/code/control/control_map.h"
#include "volition/rfg/code/gamewide/gameseq.h"
#include "volition/rfg/code/interface/loading.h"
#include "volition/rfg/code/interface/localization/localization_shared.h"
#include "volition/rfg/code/interface/main_menu.h"
#include "volition/rfg/code/interface/multi_player_menus/multi_player_menu_system.h"
#include "volition/rfg/code/interface/ui_document_hud_multiplayer.h"
#include "volition/rfg/code/interface/ui_document_manager.h"
#include "volition/rfg/code/interface/ui_menu.h"
#include "volition/rfg/code/interface/ui_message_box.h"
#include "volition/rfg/code/multi/multi_core/include/multi_core_platform_internal.h"
#include "volition/rfg/code/multi/multi_game/multi_game_lobby.h"
#include "volition/rfg/code/multi/multi_game/multi_game_object_backpack.h"
#include "volition/rfg/code/multi/multi_game/multi_game_settings.h"
#include "volition/rfg/code/multi/multi_game/multi_game_weapons.h"
#include "volition/rfg/code/multi/multi_game/multi_gameplay_mode_shared.h"
#include "volition/vlib/code/input/mouse.h"
#include "volition/vlib/code/math/crc.h"
#include "volition/vlib/code/os/vconsole.h"
#include "volition/vlib/code/util/string_util.h"
#include "volition/vlib/code/util/unicode_tools.h"
#include "volition/vlib/code/util/varray.h"

#include <string.h>
#include <wchar.h>

// ---- file-private types (rfg.pdb)
// size 0x8
struct mpgo_data_item_info
{
    const char* name; // 0x00
    void (*update_cb)(vint_data_item*); // 0x04
};
static_assert(sizeof(mpgo_data_item_info) == 0x8, "mpgo_data_item_info");

enum mpgo_hint_choices
{
    MPGO_HINT_INVALID = -1,
    MPGO_HINT_ABORT = 0,
    MPGO_HINT_ACCEPT = 1,
    NUM_MPGO_HINTS = 2,
};

enum mp_menu_exit_actions_options
{
    MMEA_O_POP = 0,
    MMEA_O_PUSH = 1,
    NUM_MP_O_MENU_EXIT_ACTIONS = 2,
};

enum mp_menu_states_options
{
    MMS_O_LOADING = 0,
    MMS_O_LOADED = 1,
    MMS_O_DIALOG = 2,
    MMS_O_UNLOADING = 3,
    MMS_O_UNLOADED = 4,
    NUM_MP_O_MENU_STATES = 5,
};

enum mpgo_tabs
{
    MPGOT_BASIC_OPTIONS = 0,
    MPGOT_ADVANCED_OPTIONS = 1,
    MPGOT_MODE_OPTIONS = 2,
    MPGOT_WEAPON_OPTIONS = 3,
    MPGOT_BACKPACK_OPTIONS = 4,
    NUM_MPGO_TABS = 5,
};

// size 0x114
struct mpgo_shell_info
{

    mp_menu_states_options state; // 0x00
    mp_menu_states_options pending_state; // 0x04
    mp_menu_states_options prev_state; // 0x08
    mp_menu_exit_actions_options exit_action; // 0x0C
    game_state exit_push_target; // 0x10
    int doc_handle; // 0x14
    int data_item_handles[5]; // 0x18
    int data_group_handle; // 0x2C
    farray<int,16> group_item_handles; // 0x30
    unsigned int control_id; // 0x80
    int input_id; // 0x84
    multi_game_settings original_options; // 0x88
    mpgo_tabs current_tab; // 0x108
    unsigned int display_offset; // 0x10C
    int selection; // 0x110
};
static_assert(sizeof(mpgo_shell_info) == 0x114, "mpgo_shell_info");

// size 0x8 (rfg.pdb; only mpgo_get_tab_info's signature names it)
struct mpgo_tab_info
{
    int first_option; // 0x00, a multi_game_options value (0x3B: the tab has none)
    int num_options; // 0x04
};
static_assert(sizeof(mpgo_tab_info) == 0x8, "mpgo_tab_info");

// ---- file-static globals
static void mpgo_update_header(vint_data_item* vdi);
static void mpgo_update_page_tab(vint_data_item* vdi);
static void mpgo_update_hint(vint_data_item* vdi);
static void mpgo_update_info(vint_data_item* vdi);
static void mpgo_update_map(vint_data_item* vdi);
static const mpgo_data_item_info Data_item_infos[5] = {
    {"HeaderData", mpgo_update_header},
    {"PageTabData", mpgo_update_page_tab},
    {"HintData", mpgo_update_hint},
    {"InfoData", mpgo_update_info},
    {"MapData", mpgo_update_map},
}; // 0x01300720
static mpgo_hint_choices Current_mouse_hint_choice_options = MPGO_HINT_INVALID; // 0x01663474, initial data ffffffff
static mpgo_shell_info Mp_game_options_info; // 0x016664E8
static int mpgo_hint_config[2]; // 0x02C02B40
static bool Right_arrow_clicked_options; // 0x02C035ED
static bool Left_arrow_clicked_options; // 0x02C035EE
static int mpgo_num_hints; // 0x02C035F0
// The shell's object names (header constants in rfg.exe, this file's copies).
static const char* kPageHeaders[5] = {"tab_1", "tab_2", "tab_3", "tab_4", "tab_5"}; // 0x01668610
static const char* kLeftArrowGroup = {"page_left_arrow_main"}; // 0x01668624
static const char* kRightArrowGroup = {"page_right_arrow_main"}; // 0x01668628
static const char* kHintGroup2 = {"hint_2_group"}; // 0x0166862C
static const char* kHintGroup1 = {"hint_1_group"}; // 0x01668630
static const char* kLeftArrow = {"row_left_arrow"}; // 0x01668604
static const char* kRightArrow = {"row_right_arrow"}; // 0x01668608
static const char* kGroup_name = {"row_group"}; // 0x0166860C

// ---- internal functions

// 0x008AD110
// Accepted: the options as they were when the menu opened.
static bool mpgo_confirm_abort(int mbox_handle, int selection_index, msgbox_choice choice)
{
    if (choice == MBC_OK)
    {
        Mp_server_options = Mp_game_options_info.original_options;
        Mp_game_options_info.exit_action = MMEA_O_POP;
        Mp_game_options_info.pending_state = MMS_O_UNLOADING;
    }
    return true;
}

static void mpgo_load_data_items();
static void mpgo_unload_ui_information();

// 0x008F2290
static void mpgo_do_state_change()
{
    do
    {
        Mp_game_options_info.state = Mp_game_options_info.pending_state;
        if (Mp_game_options_info.pending_state == MMS_O_LOADED)
        {
            // mpgo_load_ui_information, inlined.
            vint_data_group* group = vint_data_group::create_new("RowData");
            if (group != nullptr)
            {
                Mp_game_options_info.data_group_handle = group->get_handle();
                ui_add_data_group(Mp_game_options_info.doc_handle, Mp_game_options_info.data_group_handle);
            }
            mpgo_load_data_items();
        }
        else
        {
            if (Mp_game_options_info.pending_state != MMS_O_UNLOADING)
            {
                return;
            }
            mpgo_unload_ui_information();
            if (Mp_game_options_info.exit_action == MMEA_O_POP)
            {
                gameseq_pop_state();
            }
            else if (Mp_game_options_info.exit_action == MMEA_O_PUSH)
            {
                gameseq_push_state(Mp_game_options_info.exit_push_target, true, true);
            }
            Mp_game_options_info.pending_state = MMS_O_UNLOADED;
        }
    } while (Mp_game_options_info.state != Mp_game_options_info.pending_state);
}

// 0x0088B3E0
// LTCG: new_state in EAX.
static void mpgo_set_state(mp_menu_states_options new_state)
{
    Mp_game_options_info.pending_state = new_state;
}

// 0x0088B3F0
static void mpgo_init_state()
{
    Mp_game_options_info.state = MMS_O_UNLOADED;
    Mp_game_options_info.pending_state = MMS_O_UNLOADED;
    Mp_game_options_info.prev_state = MMS_O_UNLOADED;
    Mp_game_options_info.exit_action = MMEA_O_POP;
    Mp_game_options_info.exit_push_target = GS_MULTIPLAYER_LIVE;
}

// 0x008AD150
static void mpgo_init_info()
{
    Mp_game_options_info.group_item_handles.clear();
    Mp_game_options_info.data_group_handle = -1;
}

// 0x008D0140
static void mpgo_load_data_groups()
{
    vint_data_group* group = vint_data_group::create_new("RowData");
    if (group != nullptr)
    {
        Mp_game_options_info.data_group_handle = group->get_handle();
        ui_add_data_group(Mp_game_options_info.doc_handle, Mp_game_options_info.data_group_handle);
    }
}

// 0x008BFFE0
// An option row: {index, name, value, selected, (unused), odd row, (unused), valid}.
static void mpgo_add_group_row(vint_data_group* vdg)
{
    if (vdg == nullptr)
    {
        return;
    }
    vint_data_item* item = vdg->add_data_item(nullptr);
    if (item == nullptr)
    {
        return;
    }
    vint_variant_type types[8] = {VINT_PROP_TYPE_INT, VINT_PROP_TYPE_STRING, VINT_PROP_TYPE_STRING, VINT_PROP_TYPE_BOOL,
                                  VINT_PROP_TYPE_BOOL, VINT_PROP_TYPE_BOOL, VINT_PROP_TYPE_STRING, VINT_PROP_TYPE_BOOL};
    item->set_template(types, 8);
    Mp_game_options_info.group_item_handles.add((int)item->get_handle());
}

// 0x008AD170
static void mpgo_remove_group_row(vint_data_group* vdg)
{
    if (vdg != nullptr)
    {
        const int last = Mp_game_options_info.group_item_handles.size() - 1;
        vdg->remove_data_item((unsigned int)Mp_game_options_info.group_item_handles[last]);
        Mp_game_options_info.group_item_handles.remove(last);
    }
}

// 0x008AD1C0
static void mpgo_unload_data_groups()
{
    vint_data_group* group = vint_data_group::find((unsigned int)Mp_game_options_info.data_group_handle);
    if (group != nullptr)
    {
        for (int i = Mp_game_options_info.group_item_handles.size() - 1; i >= 0; i--)
        {
            group->remove_data_item((unsigned int)Mp_game_options_info.group_item_handles[i]);
        }
        Mp_game_options_info.group_item_handles.clear();
    }
}

// 0x008D0170
// The five data items (header, page tab, hint, info, map), added to the document; the map item has no template.
static void mpgo_load_data_items()
{
    for (int i = 0; i < 5; i++)
    {
        vint_data_item* item = vint_data_item::create_new(Data_item_infos[i].name);
        if (item == nullptr)
        {
            continue;
        }
        ui_add_data_item(Mp_game_options_info.doc_handle, (int)item->get_handle());
        Mp_game_options_info.data_item_handles[i] = (int)item->get_handle();
        switch (i)
        {
        case 0:
        {
            vint_variant_type types[3] = {VINT_PROP_TYPE_STRING, VINT_PROP_TYPE_BOOL, VINT_PROP_TYPE_FLOAT};
            item->set_template(types, 3);
            break;
        }
        case 1:
        {
            vint_variant_type types[3] = {VINT_PROP_TYPE_STRING, VINT_PROP_TYPE_INT, VINT_PROP_TYPE_INT};
            item->set_template(types, 3);
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
            vint_variant_type types[3] = {VINT_PROP_TYPE_BOOL, VINT_PROP_TYPE_STRING, VINT_PROP_TYPE_STRING};
            item->set_template(types, 3);
            break;
        }
        default:
            break;
        }
    }
}

// 0x008D0310
static void mpgo_unload_data_items()
{
    if (Mp_game_options_info.doc_handle == -1)
    {
        return;
    }
    vint_doc_info* info = ui_get_doc_info(Mp_game_options_info.doc_handle);
    if (info != nullptr)
    {
        info->release_data_groups();
        info->release_data_items();
        info->release_data_events();
    }
    for (int i = 0; i < 5; i++)
    {
        Mp_game_options_info.data_item_handles[i] = -1;
    }
}

// 0x008D0370
static void mpgo_load_ui_information()
{
    mpgo_load_data_groups();
    mpgo_load_data_items();
}

// 0x008E2600
static void mpgo_unload_ui_information()
{
    mpgo_unload_data_groups();
    mpgo_unload_data_items();
    mpgo_init_info();
    multi_player_menu_system_close_message_box();
}

// 0x0088B440
// LTCG: tab in EAX, info in ESI. Each tab's options: basic 0-7, advanced 8-18, the mode's own, the 16 weapon locks
// and the 10 backpack locks (0x3B: no options).
static void mpgo_get_tab_info(mpgo_tab_info& info, mpgo_tabs tab)
{
    switch (tab)
    {
    case MPGOT_ADVANCED_OPTIONS:
        info.first_option = 8;
        info.num_options = 0xB;
        return;
    case MPGOT_MODE_OPTIONS:
        switch (Mp_server_options.get_mode())
        {
        case MGPM_DEATHMATCH:
            info.first_option = 0x13;
            info.num_options = 1;
            return;
        case MGPM_TEAM_DEATHMATCH:
            info.first_option = 0x3B;
            info.num_options = 1;
            return;
        case MGPM_CAPTURE_THE_FLAG:
            info.first_option = 0x14;
            info.num_options = 3;
            return;
        case MGPM_SIEGE:
            info.first_option = 0x17;
            info.num_options = 4;
            return;
        case MGPM_KING_OF_THE_HILL:
            info.first_option = 0x1B;
            info.num_options = 1;
            return;
        case MGPM_BAGMAN:
            info.first_option = 0x1C;
            info.num_options = 2;
            return;
        case MGPM_TEAM_BAGMAN:
            info.first_option = 0x1E;
            info.num_options = 2;
            return;
        case MGPM_DEMOLITION:
            info.first_option = 0x20;
            info.num_options = 1;
            return;
        default:
            break;
        }
        break;
    case MPGOT_WEAPON_OPTIONS:
        info.first_option = 0x21;
        info.num_options = 0x10;
        return;
    case MPGOT_BACKPACK_OPTIONS:
        info.first_option = 0x31;
        info.num_options = 10;
        return;
    default:
        break;
    }
    info.first_option = 0;
    info.num_options = 8;
}

static void mpgo_update_rows(vint_data_group* vdg);

// 0x008E26D0
static void mpgo_update_data_items()
{
    vint_data_group* group = vint_data_group::find((unsigned int)Mp_game_options_info.data_group_handle);
    if (group != nullptr)
    {
        mpgo_update_rows(group);
    }
    for (int i = 0; i < 5; i++)
    {
        vint_data_item* item = vint_data_item::find((unsigned int)Mp_game_options_info.data_item_handles[i]);
        if (item != nullptr && Data_item_infos[i].update_cb != nullptr)
        {
            Data_item_infos[i].update_cb(item);
        }
    }
}

// 0x008F2330
static void mpgo_process_loading()
{
    switch (ui_get_doc_asset_state(Mp_game_options_info.doc_handle))
    {
    case ST_LOADING:
        return;
    case ST_LOADED:
        Mp_game_options_info.pending_state = MMS_O_LOADED;
        return;
    case ST_UNLOADED:
        keen::debug::nativePrintArgs("Int3 critical function at %s(%d)\n", __FILE__, __LINE__);
        // fall through
    case ST_ERROR:
        // Magic number: a 0.5 s fade out.
        ui_unload_document(Mp_game_options_info.doc_handle, DFOT_NONE, 0.5f);
        Mp_game_options_info.doc_handle = -1;
        Mp_game_options_info.pending_state = MMS_O_UNLOADING;
        return;
    default:
        keen::debug::nativePrintArgs("Int3 critical function at %s(%d)\n", __FILE__, __LINE__);
        return;
    }
}

// The menu's input id has the focus (ui_input_id_has_focus, inlined).
static bool mpgo_has_focus()
{
    const int lock = Current_input_lock_id != -1 ? Current_input_lock_id : 0;
    if (Mp_game_options_info.input_id == -1 && lock == 0)
    {
        return true;
    }
    int top = Current_input_lock_id;
    if (Current_input_lock_id == -1)
    {
        top = Input_types.size() - 1 < 0 ? 1 : Input_types[Input_types.size() - 1].id;
    }
    return Mp_game_options_info.input_id == top;
}

// A Button_info button pressed (and not waiting for its release) with the focus.
static bool mpgo_button_pressed(int button)
{
    return mpgo_has_focus() && !Button_info.binfo[button].m_button_wait_for_up && Button_info.binfo[button].m_button_input > 0.0f;
}

// An enum option stepped down (wrapping to max) or up (wrapping to 0), as the settings' numeric template does.
template<typename T>
static void mpgo_cycle(T& value, bool down, T max)
{
    if (down)
    {
        multi_game_settings_process_numeric_option_down<T>(value, (T)1, (T)0, max);
    }
    else
    {
        multi_game_settings_process_numeric_option_up<T>(value, (T)1, (T)0, max);
    }
}

// A weapon picker stepped (none (-1) is next to both ends).
static void mpgo_step_weapon(unsigned int& weapon, bool left)
{
    if (left)
    {
        weapon = weapon != 0xFFFFFFFF ? multi_gw_get_prev_weapon_info_handle(weapon) : multi_gw_get_last_weapon_info_handle();
        audiolib_cue_play(Audio_info[7], nullptr, nullptr);
    }
    else
    {
        weapon = weapon != 0xFFFFFFFF ? multi_gw_get_next_weapon_info_handle(weapon) : multi_gw_get_first_weapon_info_handle();
        audiolib_cue_play(Audio_info[8], nullptr, nullptr);
    }
}

// A backpack picker with its enum (the backpack spawns and the starting backpack): the specific backpack steps until it
// runs out (-1), then the enum steps; reaching the enum's SPECIFIC value picks the first (or last) backpack.
template<typename T>
static void mpgo_step_backpack(T& choice, int& backpack, bool left, T max, T specific, T wrap_up)
{
    if (left)
    {
        audiolib_cue_play(Audio_info[7], nullptr, nullptr);
        if (backpack != -1)
        {
            backpack = multi_game_backpack_get_prev_backpack_id(backpack);
            if (backpack != -1)
            {
                return;
            }
        }
        const T value = (T)(choice - 1);
        if ((int)value < 0 || (int)choice < (int)value)
        {
            choice = specific;
        }
        else
        {
            choice = value;
            if (value != specific)
            {
                return;
            }
        }
        backpack = multi_game_backpack_get_prev_backpack_id(-1);
        return;
    }
    audiolib_cue_play(Audio_info[8], nullptr, nullptr);
    if (backpack != -1)
    {
        backpack = multi_game_backpack_get_next_backpack_id(backpack);
        if (backpack != -1)
        {
            return;
        }
    }
    const T value = (T)(choice + 1);
    if ((int)max < (int)value || (int)value < (int)choice)
    {
        choice = wrap_up;
        return;
    }
    choice = value;
    if (value == specific)
    {
        backpack = multi_game_backpack_get_next_backpack_id(-1);
    }
}

// 0x008D03A0
// Left (button 3 or 7, or a left arrow click) and right (4 or 8, or a right arrow click) change the selected option.
// LTCG: the tab info's out pointer in ESI (current_tab in EAX).
static void mpgo_input_process_page()
{
    const bool left = mpgo_button_pressed(3) || mpgo_button_pressed(7) || Left_arrow_clicked_options;
    const bool right = mpgo_button_pressed(4) || mpgo_button_pressed(8) || Right_arrow_clicked_options;
    mpgo_tab_info info;
    mpgo_get_tab_info(info, Mp_game_options_info.current_tab);
    multi_game_generic_options& generic = Mp_server_options.generic_options;
    multi_game_mode_options& mode = Mp_server_options.mode_options;
    switch (Mp_game_options_info.display_offset + Mp_game_options_info.selection + info.first_option)
    {
    case 0:
    {
        // The mode: switching between a team mode and a free for all one assigns the teams again.
        const bool was_team = multi_gameplay_mode_is_team_mode(Mp_server_options.get_mode());
        if (left)
        {
            Mp_server_options.set_mode(Mp_server_options.prev_mode());
            audiolib_cue_play(Audio_info[7], nullptr, nullptr);
        }
        else if (right)
        {
            Mp_server_options.set_mode(Mp_server_options.next_mode());
            audiolib_cue_play(Audio_info[8], nullptr, nullptr);
        }
        if (was_team != multi_gameplay_mode_is_team_mode(Mp_server_options.get_mode()))
        {
            multi_game_lobby_assign_teams();
        }
        break;
    }
    case 1:
        if (left)
        {
            Mp_server_options.set_level(Mp_server_options.prev_level());
            audiolib_cue_play(Audio_info[7], nullptr, nullptr);
        }
        else if (right)
        {
            Mp_server_options.set_level(Mp_server_options.next_level());
            audiolib_cue_play(Audio_info[8], nullptr, nullptr);
        }
        break;
    case 2:
    {
        // The score limit per mode: deathmatch 5..200 (steps of 10 above 50), capture the flag 1..10, the point modes
        // 25..950 (steps of 50 above 200); siege has none.
        unsigned short step;
        unsigned short min;
        unsigned short max;
        switch (Mp_server_options.get_mode())
        {
        case MGPM_DEATHMATCH:
        case MGPM_TEAM_DEATHMATCH:
            min = 5;
            max = 200;
            step = generic.point_limit > 50 || (generic.point_limit == 50 && !left) ? 10 : 5;
            break;
        case MGPM_CAPTURE_THE_FLAG:
            min = 1;
            max = 10;
            step = 1;
            break;
        case MGPM_KING_OF_THE_HILL:
        case MGPM_BAGMAN:
        case MGPM_TEAM_BAGMAN:
        case MGPM_DEMOLITION:
            min = 25;
            max = 950;
            step = generic.point_limit > 200 || (generic.point_limit == 200 && !left) ? 50 : 25;
            break;
        default:
            return;
        }
        if (left)
        {
            multi_game_settings_process_numeric_option_down<unsigned short>(generic.point_limit, step, min, max);
            audiolib_cue_play(Audio_info[3], nullptr, nullptr);
        }
        else if (right)
        {
            multi_game_settings_process_numeric_option_up<unsigned short>(generic.point_limit, step, min, max);
            audiolib_cue_play(Audio_info[2], nullptr, nullptr);
        }
        break;
    }
    case 3:
    {
        // The time limit, 1..30 minutes (steps of 5 above 15).
        const unsigned char step = generic.round_time_limit_minutes > 15 || (generic.round_time_limit_minutes == 15 && !left) ? 5 : 1;
        if (left)
        {
            multi_game_settings_process_numeric_option_down<unsigned char>(generic.round_time_limit_minutes, step, 1, 30);
            audiolib_cue_play(Audio_info[3], nullptr, nullptr);
        }
        else if (right)
        {
            multi_game_settings_process_numeric_option_up<unsigned char>(generic.round_time_limit_minutes, step, 1, 30);
            audiolib_cue_play(Audio_info[2], nullptr, nullptr);
        }
        break;
    }
    case 4:
        if (left || right)
        {
            generic.friendly_fire = !generic.friendly_fire;
            audiolib_cue_play(Audio_info[8], nullptr, nullptr);
        }
        break;
    case 5:
        if (left || right)
        {
            mpgo_cycle(generic.rubberbanding_level, left, MRL_HIGH);
            audiolib_cue_play(Audio_info[left ? 7 : 8], nullptr, nullptr);
        }
        break;
    case 6:
        if (left || right)
        {
            mpgo_cycle(generic.enemy_icon_visibility, left, MGEIV_ALWAYS);
            audiolib_cue_play(Audio_info[left ? 7 : 8], nullptr, nullptr);
        }
        break;
    case 7:
        if (left || right)
        {
            mpgo_cycle(generic.time_of_day, left, MGTOD_ALTERNATE);
            audiolib_cue_play(Audio_info[left ? 7 : 8], nullptr, nullptr);
        }
        break;
    case 8:
    {
        // The respawn time, 3..30 s (steps of 5 above 10).
        const unsigned char step = generic.respawn_time_seconds > 10 || (generic.respawn_time_seconds == 10 && !left) ? 5 : 1;
        if (left)
        {
            multi_game_settings_process_numeric_option_down<unsigned char>(generic.respawn_time_seconds, step, 3, 30);
            audiolib_cue_play(Audio_info[3], nullptr, nullptr);
        }
        else if (right)
        {
            multi_game_settings_process_numeric_option_up<unsigned char>(generic.respawn_time_seconds, step, 3, 30);
            audiolib_cue_play(Audio_info[2], nullptr, nullptr);
        }
        break;
    }
    case 9:
        if (left || right)
        {
            mpgo_step_backpack(generic.backpack_spawns, generic.backpack_spawn_type, left, MGBSO_SPECIFIC, MGBSO_SPECIFIC, MGBSO_DEFAULT);
        }
        break;
    case 10:
        if (left || right)
        {
            mpgo_step_backpack(generic.starting_backpack, generic.starting_backpack_type, left, MGSBO_SPECIFIC, MGSBO_SPECIFIC, MGSBO_NONE);
        }
        break;
    case 0xB:
        // The health regeneration speed, 50..200 in steps of 50.
        if (left)
        {
            multi_game_settings_process_numeric_option_down<unsigned char>(generic.health_regen_speed, 50, 50, 200);
            audiolib_cue_play(Audio_info[3], nullptr, nullptr);
        }
        else if (right)
        {
            multi_game_settings_process_numeric_option_up<unsigned char>(generic.health_regen_speed, 50, 50, 200);
            audiolib_cue_play(Audio_info[2], nullptr, nullptr);
        }
        break;
    case 0xC:
        // The maximum health, 50..200 in steps of 25.
        if (left)
        {
            multi_game_settings_process_numeric_option_down<unsigned short>(generic.max_health, 25, 50, 200);
            audiolib_cue_play(Audio_info[3], nullptr, nullptr);
        }
        else if (right)
        {
            multi_game_settings_process_numeric_option_up<unsigned short>(generic.max_health, 25, 50, 200);
            audiolib_cue_play(Audio_info[2], nullptr, nullptr);
        }
        break;
    case 0xD:
        // The movement speed, 70..130 in steps of 10.
        if (left)
        {
            multi_game_settings_process_numeric_option_down<unsigned char>(generic.movement_speed, 10, 70, 130);
            audiolib_cue_play(Audio_info[3], nullptr, nullptr);
        }
        else if (right)
        {
            multi_game_settings_process_numeric_option_up<unsigned char>(generic.movement_speed, 10, 70, 130);
            audiolib_cue_play(Audio_info[2], nullptr, nullptr);
        }
        break;
    case 0xE:
        if (left || right)
        {
            generic.unlimited_ammo = !generic.unlimited_ammo;
            audiolib_cue_play(Audio_info[8], nullptr, nullptr);
        }
        break;
    case 0xF:
        if (left || right)
        {
            mpgo_step_weapon(generic.starting_weapon, left);
        }
        break;
    case 0x10:
        if (left || right)
        {
            mpgo_step_weapon(generic.third_weapon, left);
        }
        break;
    case 0x11:
        if (left || right)
        {
            mpgo_step_weapon(generic.fourth_weapon, left);
        }
        break;
    case 0x12:
        if (left || right)
        {
            mpgo_cycle(generic.weapon_respawn_speed, left, MGWRS_SLOW);
            audiolib_cue_play(Audio_info[left ? 7 : 8], nullptr, nullptr);
        }
        break;
    case 0x13:
        // Deathmatch: the leader highlighting.
        if (left || right)
        {
            mode.deathmatch.leader_highlighting = !mode.deathmatch.leader_highlighting;
            audiolib_cue_play(Audio_info[8], nullptr, nullptr);
        }
        break;
    case 0x14:
        // Capture the flag: the flag return time, 5..60 s in steps of 5.
        if (left)
        {
            multi_game_settings_process_numeric_option_down<unsigned char>(mode.capture_the_flag.flag_return_time_seconds, 5, 5, 60);
            audiolib_cue_play(Audio_info[7], nullptr, nullptr);
        }
        else if (right)
        {
            multi_game_settings_process_numeric_option_up<unsigned char>(mode.capture_the_flag.flag_return_time_seconds, 5, 5, 60);
            audiolib_cue_play(Audio_info[8], nullptr, nullptr);
        }
        break;
    case 0x15:
        if (left || right)
        {
            mode.capture_the_flag.flag_touch_return = !mode.capture_the_flag.flag_touch_return;
            audiolib_cue_play(Audio_info[8], nullptr, nullptr);
        }
        break;
    case 0x16:
        if (left || right)
        {
            mode.capture_the_flag.flag_carrier_icon_always_on = !mode.capture_the_flag.flag_carrier_icon_always_on;
            audiolib_cue_play(Audio_info[8], nullptr, nullptr);
        }
        break;
    case 0x17:
        // Siege: the defenders' starting backpack.
        if (left)
        {
            mode.siege.defense_starting_backpack_type = multi_game_backpack_get_prev_backpack_id(mode.siege.defense_starting_backpack_type);
            audiolib_cue_play(Audio_info[7], nullptr, nullptr);
        }
        else if (right)
        {
            mode.siege.defense_starting_backpack_type = multi_game_backpack_get_next_backpack_id(mode.siege.defense_starting_backpack_type);
            audiolib_cue_play(Audio_info[8], nullptr, nullptr);
        }
        break;
    case 0x18:
        if (left || right)
        {
            mpgo_step_weapon(mode.siege.defense_starting_weapon, left);
        }
        break;
    case 0x19:
        if (left || right)
        {
            mpgo_step_weapon(mode.siege.defense_third_weapon, left);
        }
        break;
    case 0x1A:
        if (left || right)
        {
            mpgo_step_weapon(mode.siege.defense_fourth_weapon, left);
        }
        break;
    case 0x1B:
        if (left || right)
        {
            mpgo_cycle(mode.king_of_the_hill.all_target_multiplier, left, KOTHM_ONLY);
            audiolib_cue_play(Audio_info[left ? 7 : 8], nullptr, nullptr);
        }
        break;
    case 0x1C:
    case 0x1E:
        // Bagman and team bagman: the bag return time, 5..60 s in steps of 5.
        if (left)
        {
            multi_game_settings_process_numeric_option_down<unsigned char>(mode.bagman.bag_return_time_seconds, 5, 5, 60);
            audiolib_cue_play(Audio_info[7], nullptr, nullptr);
        }
        else if (right)
        {
            multi_game_settings_process_numeric_option_up<unsigned char>(mode.bagman.bag_return_time_seconds, 5, 5, 60);
            audiolib_cue_play(Audio_info[8], nullptr, nullptr);
        }
        break;
    case 0x1D:
    case 0x1F:
        if (left || right)
        {
            mode.bagman.bagman_kill_bonus = !mode.bagman.bagman_kill_bonus;
            audiolib_cue_play(Audio_info[8], nullptr, nullptr);
        }
        break;
    case 0x20:
        // Demolition: the engineer's icon.
        if (left || right)
        {
            mode.demolition.engineer_icon_always_on = !mode.demolition.engineer_icon_always_on;
            audiolib_cue_play(Audio_info[8], nullptr, nullptr);
        }
        break;
    default:
    {
        const int option = (int)(Mp_game_options_info.display_offset + Mp_game_options_info.selection + info.first_option);
        // The weapon (0x21-0x30) and backpack (0x31-0x3A) locks: the lock bit swapped (no sound).
        if (option >= 0x21 && option <= 0x30)
        {
            if (left || right)
            {
                multi_game_settings_swap_weapon_bit(&Mp_server_options, (multi_game_lockable_weapons)(option - 0x21));
            }
        }
        else if (option >= 0x31 && option <= 0x3A)
        {
            if (left || right)
            {
                multi_game_settings_swap_backpack_bit(&Mp_server_options, (multi_game_lockable_backpacks)(option - 0x31));
            }
        }
        else if (left || right)
        {
            audiolib_cue_play(Audio_info[11], nullptr, nullptr);
        }
        break;
    }
    }
}

// 0x008D1A00
// The row under the mouse (row_group, row_group2, ...) when it moved or clicked; a click on its arrows steps it.
// LTCG: the document handle in EDI.
static int mpgo_input_process_mouse_option_choice()
{
    if (!mouse_just_moved() && !mouse_just_released(0))
    {
        return -1;
    }
    vint_doc_info* info = ui_get_doc_info(Mp_game_options_info.doc_handle);
    if (info == nullptr || info->m_doc == nullptr)
    {
        return -1;
    }
    char name[128];
    for (int i = 0; i < Mp_game_options_info.group_item_handles.size(); i++)
    {
        if (i == 0)
        {
            keen::formatString(name, sizeof(name), "%s", kGroup_name);
        }
        else
        {
            keen::formatString(name, sizeof(name), "%s%d", kGroup_name, i + 1);
        }
        if (mouse_just_released(0))
        {
            vint_object_base* row = vint_object_base::find_object(name, info->m_doc);
            vint_object_base* arrow = row != nullptr ? row->find_child(crc_str(kRightArrow, 0)) : nullptr;
            if (arrow != nullptr && ui_is_mouse_over_vint_object(arrow))
            {
                Right_arrow_clicked_options = true;
            }
            else
            {
                row = vint_object_base::find_object(name, info->m_doc);
                arrow = row != nullptr ? row->find_child(crc_str(kLeftArrow, 0)) : nullptr;
                if (arrow != nullptr && ui_is_mouse_over_vint_object(arrow))
                {
                    Left_arrow_clicked_options = true;
                }
            }
        }
        if (ui_is_mouse_over_vint_object(vint_object_base::find_object(name, info->m_doc)))
        {
            return i;
        }
    }
    return -1;
}

// 0x008D1BB0
// The hints (accept, back), the page arrows and tab headers (on a click), then the rows.
static void mpgo_input_process_mouse(bool& events_consumed)
{
    Left_arrow_clicked_options = false;
    Right_arrow_clicked_options = false;
    const int top = Current_input_lock_id != -1 ? Current_input_lock_id : (Input_types.size() - 1 < 0 ? 1 : Input_types[Input_types.size() - 1].id);
    if (top != Mp_game_options_info.input_id)
    {
        return;
    }
    // A function static (initialized at the first call) in rfg.exe.
    static const char* hint_names[2] = {kHintGroup1, kHintGroup2}; // 0x02CA1520
    const int hint = ui_mouse_process_hints(hint_names, 2, Mp_game_options_info.doc_handle, true);
    if (hint != -1)
    {
        Current_mouse_hint_choice_options = (mpgo_hint_choices)mpgo_hint_config[hint];
        return;
    }
    vint_doc_info* info = ui_get_doc_info(Mp_game_options_info.doc_handle);
    if (info == nullptr || info->m_doc == nullptr)
    {
        return;
    }
    if (mouse_just_released(0))
    {
        if (ui_is_mouse_over_vint_object(vint_object_base::find_object(kRightArrowGroup, info->m_doc)))
        {
            Mp_game_options_info.display_offset = 0;
            Mp_game_options_info.selection = 0;
            Mp_game_options_info.current_tab = (mpgo_tabs)((Mp_game_options_info.current_tab + 1) % NUM_MPGO_TABS);
            return;
        }
        if (ui_is_mouse_over_vint_object(kLeftArrowGroup, info->m_doc))
        {
            Mp_game_options_info.current_tab = (mpgo_tabs)((Mp_game_options_info.current_tab + 4) % NUM_MPGO_TABS);
            Mp_game_options_info.display_offset = 0;
            Mp_game_options_info.selection = 0;
        }
        for (unsigned int i = 0; i < NUM_MPGO_TABS; i++)
        {
            if (ui_is_mouse_over_vint_object(vint_object_base::find_object(kPageHeaders[i], info->m_doc)))
            {
                Mp_game_options_info.current_tab = (mpgo_tabs)(i % NUM_MPGO_TABS);
            }
        }
    }
    const int selection = mpgo_input_process_mouse_option_choice();
    if (selection >= 0 && selection != Mp_game_options_info.selection)
    {
        multi_player_menu_system_set_menu_selection(selection, Mp_game_options_info.selection, Mp_game_options_info.group_item_handles.size());
    }
}

static bool mpgo_validate_all_weapon_selections();

// 0x008FBAD0
// LTCG: reads ECX (the flags' stack slot only). Accept (button 10, or its hint) checks the weapons and closes; back
// (button 11, or its hint) closes, asking first when the options changed; buttons 0x13/0x11 switch tabs.
static void mpgo_process_input()
{
    Current_mouse_hint_choice_options = MPGO_HINT_INVALID;
    if (Mp_game_options_info.input_id != (Current_input_lock_id != -1 ? Current_input_lock_id : (Input_types.size() - 1 < 0 ? 1 : Input_types[Input_types.size() - 1].id)))
    {
        Current_mouse_hint_choice_options = MPGO_HINT_INVALID;
        return;
    }
    bool events_consumed = false;
    mpgo_input_process_mouse(events_consumed);
    multi_player_menu_system_handle_menu_key_input(Mp_game_options_info.selection, Mp_game_options_info.group_item_handles.size(), Mp_game_options_info.input_id, events_consumed);
    if (events_consumed)
    {
        return;
    }
    const int input_id = Mp_game_options_info.input_id;
    const bool accept = ui_get_button_info(10, input_id) > 0.0f;
    const bool back = ui_get_button_info(0xB, input_id) > 0.0f || Current_mouse_hint_choice_options == MPGO_HINT_ABORT;
    const bool tab_left = ui_get_button_info(0x11, input_id) > 0.0f;
    const bool tab_right = ui_get_button_info(0x13, input_id) > 0.0f;
    if (!accept && Current_mouse_hint_choice_options != MPGO_HINT_ACCEPT)
    {
        if (back)
        {
            if (Mp_server_options.is_equal(Mp_game_options_info.original_options))
            {
                Mp_game_options_info.exit_action = MMEA_O_POP;
                Mp_game_options_info.pending_state = MMS_O_UNLOADING;
            }
            else
            {
                ui_add_msgbox(MBT_ACCEPT_CANCEL, "MENU_WARNING", "MENU_CHANGES_LOST", mpgo_confirm_abort, false, false, nullptr, nullptr, false);
            }
            audiolib_cue_play(Audio_info[5], nullptr, nullptr);
        }
    }
    else if (mpgo_validate_all_weapon_selections())
    {
        Mp_game_options_info.exit_action = MMEA_O_POP;
        Mp_game_options_info.pending_state = MMS_O_UNLOADING;
        audiolib_cue_play(Audio_info[4], nullptr, nullptr);
    }
    else
    {
        ui_add_msgbox(MBT_OK, "MENU_MBOX_WEAPON_SELECTION_PROBLEM", "MENU_MBOX_WEAPON_SELECTION_PROBLEM_DESC", nullptr, false, false, nullptr, nullptr, false);
    }
    if (tab_right || tab_left)
    {
        Mp_game_options_info.current_tab = (mpgo_tabs)((Mp_game_options_info.current_tab + (tab_right ? 1 : 4)) % NUM_MPGO_TABS);
        Mp_game_options_info.selection = 0;
        Mp_game_options_info.display_offset = 0;
    }
    mpgo_input_process_page();
}

// 0x008FBCB0
static void mpgo_process_loaded()
{
    multi_game_lobby_process();
    mpgo_update_data_items();
    mpgo_process_input();
}

// 0x0088B420
static void mpgo_ui_pop()
{
    Mp_game_options_info.exit_action = MMEA_O_POP;
    Mp_game_options_info.pending_state = MMS_O_UNLOADING;
}

// 0x0088B550
// LTCG: option in ESI. A weapon picker's value is valid when it differs from the other picked weapons (and, replacing
// the starting weapon, follows it).
static bool mpgo_validate_weapon_selection(multi_game_options option)
{
    const multi_game_generic_options& generic = Mp_server_options.generic_options;
    const multi_game_mode_options& mode = Mp_server_options.mode_options;
    if (!generic.replace_starting_weapon || generic.starting_weapon != 0xFFFFFFFF)
    {
        if (generic.third_weapon != 0xFFFFFFFF)
        {
            if (generic.starting_weapon == 0xFFFFFFFF)
            {
                if (option != MGO_FOURTH_WEAPON)
                {
                    goto siege;
                }
            }
            else
            {
                if (option == MGO_THIRD_WEAPON)
                {
                    if (generic.starting_weapon == generic.third_weapon)
                    {
                        return false;
                    }
                    goto siege;
                }
                if (option != MGO_FOURTH_WEAPON)
                {
                    goto siege;
                }
                if (generic.starting_weapon == generic.fourth_weapon)
                {
                    return false;
                }
            }
            if (generic.third_weapon == generic.fourth_weapon)
            {
                return false;
            }
            goto siege;
        }
    }
    else if (option == MGO_THIRD_WEAPON)
    {
        if (generic.third_weapon != 0xFFFFFFFF)
        {
            return false;
        }
        goto siege;
    }
    if (option == MGO_FOURTH_WEAPON && generic.fourth_weapon != 0xFFFFFFFF)
    {
        return false;
    }
siege:
    if (Mp_server_options.get_mode() == MGPM_SIEGE)
    {
        if (option == MGO_SIEGE_DEFENSE_THIRD_WEAPON)
        {
            if (mode.siege.defense_third_weapon != 0xFFFFFFFF)
            {
                if (generic.replace_starting_weapon && generic.starting_weapon == 0xFFFFFFFF && mode.siege.defense_starting_weapon == 0xFFFFFFFF)
                {
                    return false;
                }
                if (mode.siege.defense_third_weapon == mode.siege.defense_starting_weapon || mode.siege.defense_third_weapon == generic.starting_weapon)
                {
                    return false;
                }
            }
        }
        else if (option == MGO_SIEGE_DEFENSE_FOURTH_WEAPON && mode.siege.defense_fourth_weapon != 0xFFFFFFFF)
        {
            if (generic.third_weapon == 0xFFFFFFFF && mode.siege.defense_third_weapon == 0xFFFFFFFF)
            {
                return false;
            }
            if (mode.siege.defense_fourth_weapon == mode.siege.defense_starting_weapon || mode.siege.defense_fourth_weapon == generic.starting_weapon
                || mode.siege.defense_fourth_weapon == mode.siege.defense_third_weapon || mode.siege.defense_fourth_weapon == generic.third_weapon)
            {
                return false;
            }
        }
    }
    return true;
}

// 0x0088B670
// The weapon pickers checked as a whole (mpgo_validate_weapon_selection for each, inlined).
static bool mpgo_validate_all_weapon_selections()
{
    const multi_game_generic_options& generic = Mp_server_options.generic_options;
    const multi_game_mode_options& mode = Mp_server_options.mode_options;
    bool valid = false;
    bool third_ok;
    if (!generic.replace_starting_weapon || generic.starting_weapon != 0xFFFFFFFF)
    {
        third_ok = generic.third_weapon == 0xFFFFFFFF || generic.starting_weapon == 0xFFFFFFFF || generic.starting_weapon != generic.third_weapon;
    }
    else
    {
        third_ok = generic.third_weapon == 0xFFFFFFFF;
    }
    if (third_ok)
    {
        if (!generic.replace_starting_weapon || generic.starting_weapon != 0xFFFFFFFF)
        {
            if (generic.third_weapon == 0xFFFFFFFF)
            {
                valid = generic.fourth_weapon == 0xFFFFFFFF;
            }
            else
            {
                valid = (generic.starting_weapon == 0xFFFFFFFF || generic.starting_weapon != generic.fourth_weapon) && generic.third_weapon != generic.fourth_weapon;
            }
        }
        else
        {
            valid = generic.fourth_weapon == 0xFFFFFFFF;
        }
    }
    if (Mp_server_options.get_mode() != MGPM_SIEGE)
    {
        return valid;
    }
    if (!valid)
    {
        return false;
    }
    if (mode.siege.defense_third_weapon != 0xFFFFFFFF)
    {
        if (generic.replace_starting_weapon && generic.starting_weapon == 0xFFFFFFFF && mode.siege.defense_starting_weapon == 0xFFFFFFFF)
        {
            return false;
        }
        if (mode.siege.defense_third_weapon == mode.siege.defense_starting_weapon || mode.siege.defense_third_weapon == generic.starting_weapon)
        {
            return false;
        }
    }
    if (mode.siege.defense_fourth_weapon != 0xFFFFFFFF)
    {
        if (generic.third_weapon == 0xFFFFFFFF && mode.siege.defense_third_weapon == 0xFFFFFFFF)
        {
            return false;
        }
        if (mode.siege.defense_fourth_weapon == mode.siege.defense_starting_weapon || mode.siege.defense_fourth_weapon == generic.starting_weapon
            || mode.siege.defense_fourth_weapon == mode.siege.defense_third_weapon || mode.siege.defense_fourth_weapon == generic.third_weapon)
        {
            return false;
        }
    }
    return true;
}

// 0x008D1D60
// LTCG: the tab info's out pointer in ESI. The tab's rows (16 shown at most from display_offset): name, value (cut
// to 16 characters with "..." unless it is markup), selected, odd row, valid weapon pick; a tab without options shows
// one row. The chat window moves right off the basic tab.
static void mpgo_update_rows(vint_data_group* vdg)
{
    if (vdg == nullptr)
    {
        return;
    }
    console_change_chat_window_pos(Mp_game_options_info.current_tab == MPGOT_BASIC_OPTIONS ? VCP_PARTY : VCP_PARTY_RIGHT);
    mpgo_tab_info info;
    mpgo_get_tab_info(info, Mp_game_options_info.current_tab);
    unsigned int rows = 1;
    if (info.first_option != 0x3B)
    {
        rows = (unsigned int)info.num_options - Mp_game_options_info.display_offset;
        // Magic number: 16 rows at most.
        if (rows > 15)
        {
            rows = 16;
        }
    }
    while (vdg->get_num_data_items() < rows)
    {
        mpgo_add_group_row(vdg);
    }
    if (info.first_option == 0x3B)
    {
        vint_data_item* item = vint_data_item::find((unsigned int)Mp_game_options_info.group_item_handles[0]);
        item->set_element(0, vint_variant(1));
        item->set_element(1, vint_variant(multi_game_settings_get_option_name(NUM_MULTI_GAME_OPTIONS)));
        item->set_element(2, vint_variant(""));
        item->set_element(3, vint_variant(true));
        item->set_element(4, vint_variant(false));
        item->set_element(5, vint_variant(false));
        item->set_element(6, vint_variant(""));
    }
    else
    {
        char text[512];
        for (unsigned int row = 0; row < rows; row++)
        {
            const multi_game_options option = (multi_game_options)(Mp_game_options_info.display_offset + row + info.first_option);
            const char* name = multi_game_settings_get_option_name(option);
            multi_game_settings_get_option_text(text, sizeof(text), &Mp_server_options, option);
            const bool selected = (int)row == Mp_game_options_info.selection;
            const bool odd = (~row & 1) != 0;
            bool valid;
            switch (option)
            {
            case MGO_STARTING_WEAPON:
            case MGO_THIRD_WEAPON:
            case MGO_FOURTH_WEAPON:
            case MGO_SIEGE_DEFENSE_STARTING_WEAPON:
            case MGO_SIEGE_DEFENSE_THIRD_WEAPON:
            case MGO_SIEGE_DEFENSE_FOURTH_WEAPON:
                valid = mpgo_validate_weapon_selection(option);
                break;
            default:
                valid = true;
                break;
            }
            wchar_t value[128];
            memset(value, 0, sizeof(value));
            if (localization_does_tag_exist(text))
            {
                const wchar_t* localized = localization_get_display_string_opt(text);
                if (localized == nullptr)
                {
                    keen::debug::nativePrintArgs("Int3 critical function at %s(%d)\n", __FILE__, __LINE__);
                }
                else
                {
                    strncpy_safe(value, localized, 0x80);
                }
            }
            else
            {
                // Magic number: 20 characters of an untranslated value.
                copy_to_wide(value, text, 0x14);
            }
            // Magic numbers: a value longer than 18 characters is cut to 16 and "...".
            if (value[0] != L'[' && value[18] != L'\0')
            {
                value[16] = L'.';
                value[17] = L'.';
                value[18] = L'.';
                value[19] = L'\0';
            }
            vint_encode_wide_text(text, sizeof(text), value);
            vint_data_item* item = vint_data_item::find((unsigned int)Mp_game_options_info.group_item_handles[(int)row]);
            item->set_element(0, vint_variant((int)row + 1));
            item->set_element(1, vint_variant(name));
            item->set_element(2, vint_variant((const char*)text));
            item->set_element(3, vint_variant(selected));
            item->set_element(4, vint_variant(false));
            item->set_element(5, vint_variant(odd));
            item->set_element(6, vint_variant(""));
            item->set_element(7, vint_variant(valid));
        }
    }
    while (rows < vdg->get_num_data_items())
    {
        mpgo_remove_group_row(vdg);
    }
}

// 0x0088B860
// LTCG: the tab info's out pointer in ESI. The title, the scroll bar (more than 16 options) and its position.
static void mpgo_update_header(vint_data_item* vdi)
{
    if (vdi == nullptr)
    {
        return;
    }
    mpgo_tab_info info;
    mpgo_get_tab_info(info, Mp_game_options_info.current_tab);
    // Unsigned to float as the x87 code does (FILD, plus 2^32 when negative), divided in double precision.
    const unsigned int pages = (unsigned int)info.num_options - ((unsigned int)info.num_options & 0xF);
    const double offset = (double)Mp_game_options_info.display_offset;
    const double range = (double)pages;
    vdi->set_element(0, vint_variant("MENU_SETTINGS"));
    vdi->set_element(1, vint_variant((unsigned int)info.num_options > 16));
    vdi->set_element(2, vint_variant((float)(offset / range)));
}

// 0x0088B950
static void mpgo_update_page_tab(vint_data_item* vdi)
{
    if (vdi == nullptr)
    {
        return;
    }
    switch (Mp_game_options_info.current_tab)
    {
    case MPGOT_BASIC_OPTIONS:
        vdi->set_element(0, vint_variant("MENU_OPTIONS_BASIC"));
        break;
    case MPGOT_ADVANCED_OPTIONS:
        vdi->set_element(0, vint_variant("MENU_OPTIONS_ADVANCED"));
        break;
    case MPGOT_MODE_OPTIONS:
        vdi->set_element(0, vint_variant("MENU_OPTIONS_MODE"));
        break;
    case MPGOT_WEAPON_OPTIONS:
        vdi->set_element(0, vint_variant("MENU_WRECKING_CREW_WEAPON"));
        break;
    case MPGOT_BACKPACK_OPTIONS:
        vdi->set_element(0, vint_variant("MENU_WRECKING_CREW_BACKPACK"));
        break;
    default:
        break;
    }
    vdi->set_element(1, vint_variant((int)Mp_game_options_info.current_tab + 1));
    vdi->set_element(2, vint_variant((int)NUM_MPGO_TABS));
}

// 0x0088BAC0
// The button images (select, L2, R2, abort) and the hints' texts (back, accept); hint 1 accepts, hint 2 goes back.
static void mpgo_update_hint(vint_data_item* vdi)
{
    if (vdi == nullptr)
    {
        return;
    }
    char l2[32];
    char r2[32];
    char select[32];
    char abort[32];
    controls_get_button_image_name(l2, sizeof(l2), CBA_MENU_L2, Player_controller_index);
    controls_get_button_image_name(r2, sizeof(r2), CBA_MENU_R2, Player_controller_index);
    controls_get_button_image_name(select, sizeof(select), CBA_MENU_SELECT, Player_controller_index);
    controls_get_button_image_name(abort, sizeof(abort), CBA_MENU_ABORT, Player_controller_index);
    char l2_image[128];
    char r2_image[128];
    char select_image[128];
    char abort_image[128];
    keen::formatString(l2_image, sizeof(l2_image), "[image:%s]", l2);
    keen::formatString(r2_image, sizeof(r2_image), "[image:%s]", r2);
    keen::formatString(select_image, sizeof(select_image), "[image:%s]", select);
    keen::formatString(abort_image, sizeof(abort_image), "[image:%s]", abort);
    vdi->set_element(0, vint_variant((const char*)select_image));
    vdi->set_element(1, vint_variant((const char*)l2_image));
    vdi->set_element(2, vint_variant((const char*)r2_image));
    vdi->set_element(3, vint_variant((const char*)abort_image));
    vdi->set_element(4, vint_variant("MENU_BACK"));
    vdi->set_element(5, vint_variant("MENU_ACCEPT"));
    mpgo_hint_config[0] = MPGO_HINT_ACCEPT;
    mpgo_hint_config[1] = MPGO_HINT_ABORT;
    mpgo_num_hints = 2;
}

// 0x0088BCF0
// The selected option's name and description (the level's description is hidden: the map item shows it).
static void mpgo_update_info(vint_data_item* vdi)
{
    if (vdi == nullptr)
    {
        return;
    }
    mpgo_tab_info info;
    mpgo_get_tab_info(info, Mp_game_options_info.current_tab);
    const multi_game_options option = (multi_game_options)(Mp_game_options_info.selection + Mp_game_options_info.display_offset + info.first_option);
    vdi->set_element(1, vint_variant(multi_game_settings_get_option_name(option)));
    if (option == MGO_LEVEL)
    {
        vdi->set_element(0, vint_variant(false));
        return;
    }
    vdi->set_element(0, vint_variant(true));
    wchar_t description[512];
    char encoded[1536];
    multi_game_settings_get_option_description(description, 0x200, option);
    vint_encode_wide_text(encoded, sizeof(encoded), description);
    vdi->set_element(2, vint_variant((const char*)encoded));
}

// 0x008E2730
// With the level selected: its image, name and ideal player count.
static void mpgo_update_map(vint_data_item* vdi)
{
    mpgo_tab_info info;
    mpgo_get_tab_info(info, Mp_game_options_info.current_tab);
    if (Mp_game_options_info.selection + Mp_game_options_info.display_offset + info.first_option != MGO_LEVEL)
    {
        vdi->set_element(0, vint_variant(false));
        return;
    }
    vdi->set_element(0, vint_variant(true));
    vdi->set_element(1, vint_variant(multi_player_menu_system_level_image_get()));
    vdi->set_element(2, vint_variant("MENU_MAP"));
    const multi_level_info* level = Mp_server_options.get_level();
    if (level == nullptr)
    {
        return;
    }
    wchar_t text[256];
    char encoded[768];
    wcsncpy(text, localization_get_display_string_req(level->display_name, nullptr), 0x100);
    text[255] = L'\0';
    vint_encode_wide_text(encoded, sizeof(encoded), text);
    vdi->set_element(3, vint_variant((const char*)encoded));
    vdi->set_element(4, vint_variant("MENU_PLAYERS"));
    _snprintf_safe(text, 0x100, "%d - %d", level->ideal_min_players, level->ideal_max_players);
    vint_encode_wide_text(encoded, sizeof(encoded), text);
    vdi->set_element(5, vint_variant((const char*)encoded));
}

// ---- state callbacks (file statics in rfg.exe, external since UI5)

// 0x0090C2B0 (:3232)
// The options as they are now are kept for back.
void ui_document_mp_game_options_enter(game_state prev_state)
{
    mpgo_init_info();
    mpgo_init_state();
    // Magic number: a 0.5 s fade in.
    Mp_game_options_info.doc_handle = ui_load_document("rfg_ui_mp_options", DFIT_NONE, 0.5f, DLT_NORMAL, false, STREAM_PRIORITY_AUDIO_CACHE_LOAD, false);
    Mp_game_options_info.control_id = ui_request_control_id();
    Mp_game_options_info.input_id = ui_register_for_processed_input(IRT_UNLIMITED_INPUT, false);
    Mp_game_options_info.original_options = Mp_server_options;
    Mp_game_options_info.current_tab = MPGOT_BASIC_OPTIONS;
    Mp_game_options_info.selection = 0;
}

// 0x008F23D0 (:3263)
void ui_document_mp_game_options_exit(game_state new_state)
{
    console_change_chat_window_pos(VCP_PARTY);
    if (Mp_game_options_info.doc_handle != -1)
    {
        // Magic number: a 0.5 s fade out.
        ui_unload_document(Mp_game_options_info.doc_handle, DFOT_NONE, 0.5f);
        Mp_game_options_info.doc_handle = -1;
    }
    ui_relinquish_control_id(Mp_game_options_info.control_id);
    ui_relinquish_input_id(Mp_game_options_info.input_id);
}

// 0x009149C0 (:3280)
// A game link error (above GameLinkError 8) closes the menu.
void ui_document_mp_game_options_process()
{
    if (Bink_info_main.handle != 0)
    {
        mm_process_bink_video();
    }
    if (Mp_game_options_info.pending_state != MMS_O_UNLOADING && (Mp_game_options_info.state == MMS_O_LOADED || Mp_game_options_info.state == MMS_O_DIALOG))
    {
        g_gameLinkMutex.lock(0);
        kaiko::gamelink::GameLink* link = get_game_link_session();
        // Magic number: errors above 8 end the lobby.
        if (link != nullptr && (int)link->getError(nullptr) > 8)
        {
            mpgo_ui_pop();
        }
        g_gameLinkMutex.unlock();
    }
    ui_add_for_processing(Mp_game_options_info.doc_handle);
    switch (Mp_game_options_info.state)
    {
    case MMS_O_LOADING:
        mpgo_process_loading();
        break;
    case MMS_O_LOADED:
        mpgo_process_loaded();
        break;
    case MMS_O_UNLOADED:
        mpgo_set_state(MMS_O_LOADING);
        break;
    default:
        break;
    }
    ui_process(Mp_game_options_info.control_id, false);
}

// 0x008F2480 (:3324)
void ui_document_mp_game_options_handshake(rl_renderer*)
{
    Mp_game_options_info.prev_state = Mp_game_options_info.state;
    if (Mp_game_options_info.pending_state != Mp_game_options_info.state)
    {
        mpgo_do_state_change();
    }
    multi_player_menu_system_level_image_handshake();
}

// 0x008FBCC0 (:3334)
void ui_document_mp_game_options_render(rl_renderer*)
{
    if (Mp_game_options_info.doc_handle == -1 || Mp_game_options_info.control_id == 0)
    {
        return;
    }
    if (Mp_game_options_info.state == MMS_O_LOADED && Mp_game_options_info.prev_state == MMS_O_LOADED)
    {
        ui_add_for_rendering(Mp_game_options_info.doc_handle);
    }
    ui_render(Mp_game_options_info.control_id, false);
}

// 0x00914A90 (:3357)
void ui_document_mp_game_options_init()
{
    state_info info;
    info.enter = ui_document_mp_game_options_enter;
    info.exit = ui_document_mp_game_options_exit;
    info.process = ui_document_mp_game_options_process;
    info.handshake = ui_document_mp_game_options_handshake;
    info.render = ui_document_mp_game_options_render;
    info.transparent = false;
    info.pause_beneath = false;
    gameseq_register_state(GS_MULTIPLAYER_CHANGE_GAME_OPTIONS, info);
}
