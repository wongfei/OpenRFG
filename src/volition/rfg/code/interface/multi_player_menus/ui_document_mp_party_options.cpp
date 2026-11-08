// Original: volition/rfg/code/interface/multi_player_menus/ui_document_mp_party_options.cpp
//
// The party options menu (GS_MULTIPLAYER_PARTY_OPTIONS, stub_impl.md S7) on rfg_ui_mp_shell: one row, the party's invite
// type; accepting sets it on the lobby session and sends the party's type, backing out with changes asks first.
//
// Original bugs kept (commented): the document stays loaded on exit; the events flag starts uninitialized in rfg.exe.

#include "volition/rfg/code/interface/multi_player_menus/ui_document_mp_party_options.h"

#include "keen/base/string.hpp"
#include "keen/base/trace.hpp"
#include "volition/audio_lib/code/audiolib_api.h"
#include "volition/interface_lib/code/objects/vint_object_base.h"
#include "volition/interface_lib/code/vint_data_group.h"
#include "volition/interface_lib/code/vint_data_item.h"
#include "volition/interface_lib/code/vint_document.h"
#include "volition/interface_lib/code/vint_variant.h"
#include "volition/rfg/code/gamewide/gameseq.h"
#include "volition/rfg/code/interface/loading.h"
#include "volition/rfg/code/interface/localization/localization_shared.h"
#include "volition/rfg/code/interface/main_menu.h"
#include "volition/rfg/code/interface/multi_player_menus/multi_player_menu_system.h"
#include "volition/rfg/code/interface/multi_player_menus/ui_document_mp_party_lobby.h"
#include "volition/rfg/code/interface/ui_document_manager.h"
#include "volition/rfg/code/interface/ui_menu.h"
#include "volition/rfg/code/interface/ui_message_box.h"
#include "volition/rfg/code/multi/multi_core/generic/multi_core_session_generic.h"
#include "volition/rfg/code/multi/multi_core/include/multi.h"
#include "volition/rfg/code/multi/multi_core/include/multi_core_platform_internal.h"
#include "volition/rfg/code/multi/multi_game/multi_game_party.h"
#include "volition/vlib/code/input/mouse.h"
#include "volition/vlib/code/util/varray.h"

class rl_renderer;

// ---- file-private types (rfg.pdb)
enum mp_party_options_exit_actions
{
    MMEA_PO_POP = 0,
    MMEA_PO_PUSH = 1,
};

enum mp_party_options_states
{
    MPOS_LOADING = 0,
    MPOS_LOADED = 1,
    MPOS_UNLOADING = 2,
    MPOS_UNLOADED = 3,
};

enum mp_party_options_hints
{
    PO_HINT_INVALID = -1,
    PO_HINT_ACCEPT = 0,
    PO_HINT_ABORT = 1,
};

// size 0x38
struct mp_party_options_menu_info
{
    mp_party_options_states state; // 0x00
    mp_party_options_states pending_state; // 0x04
    mp_party_options_states prev_state; // 0x08
    mp_party_options_exit_actions exit_action; // 0x0C
    game_state exit_push_target; // 0x10
    int doc_handle; // 0x14
    int data_group_handle; // 0x18
    farray<int,1> menu_rows; // 0x1C
    unsigned int control_id; // 0x30
    int input_id; // 0x34
};
static_assert(sizeof(mp_party_options_menu_info) == 0x38, "mp_party_options_menu_info");

// ---- file-static globals
static void party_options_process_data_item_hint_text(vint_data_item* vdi);
static void party_options_process_data_item_titles(vint_data_item* vdi);
static void party_options_process_data_item_selection_info(vint_data_item* vdi);
static void party_options_process_data_item_playlist_info(vint_data_item* vdi);
static void party_options_process_data_item_menu_type(vint_data_item* vdi);
static mp_shell_data_item_info Party_options_data_items[5] = { // 0x016636C4
    {MPSDIT_HINT_INFO, party_options_process_data_item_hint_text},
    {MPSDIT_TITLES, party_options_process_data_item_titles},
    {MPSDIT_SELECTION_INFO, party_options_process_data_item_selection_info},
    {MPSDIT_INFO_TEXT, party_options_process_data_item_playlist_info},
    {MPSDIT_MENU_TYPE, party_options_process_data_item_menu_type},
};
static const char* kLeftArrow = {"row_highlight_left"}; // 0x01668634
static const char* kRightArrow = {"row_highlight_right"}; // 0x01668638
static const char* kGroup_name = {"mp_shell_row_main"}; // 0x0166863C
static const char* Back_string_poptions = {"MENU_BACK"}; // 0x016636EC
static const char* Party_options = {"MENU_PARTY_OPTIONS"}; // 0x016636F0
static const char* Menu_option_names[1] = {"MENU_PARTY_INVITE_TYPE"}; // 0x016636F4
static const char* Menu_option_descriptions[1] = {"MENU_PARTY_INVITE_TYPE_DESC"}; // 0x016636F8
static mp_party_lobby_types Desired_party_type = MPPLT_UNKNOWN; // 0x016636FC
static mp_party_options_hints Current_mouse_hint_choice_poptions = PO_HINT_INVALID; // 0x01663700
static mp_party_options_menu_info Party_options_menu_info; // 0x016668C8
static int party_options_hint_config[2]; // 0x02BE7D90
static int Party_options_menu_selection; // 0x02C03628
static session_invite_type Desired_invite_type; // 0x02C0362C
static int party_options_num_hints; // 0x02C03630
static bool Right_arrow_clicked_poptions; // 0x02C03634
static bool Left_arrow_clicked_poptions; // 0x02C03635

// ---- internal functions
static void party_options_load_ui_information();
static void party_options_unload_ui_information();

// 0x008AE170
static bool mppo_confirm_abort(int mbox_handle, int selection_index, msgbox_choice choice)
{
    if (choice == MBC_OK)
    {
        Party_options_menu_info.exit_action = MMEA_PO_POP;
        Party_options_menu_info.pending_state = MPOS_UNLOADING;
    }
    return true;
}

// 0x008F2D20
static void party_options_do_state_change()
{
    do
    {
        Party_options_menu_info.state = Party_options_menu_info.pending_state;
        if (Party_options_menu_info.pending_state == MPOS_LOADED)
        {
            party_options_load_ui_information();
        }
        else
        {
            if (Party_options_menu_info.pending_state != MPOS_UNLOADING)
            {
                return;
            }
            party_options_unload_ui_information();
            if (Party_options_menu_info.exit_action == MMEA_PO_POP)
            {
                gameseq_pop_state();
            }
            else if (Party_options_menu_info.exit_action == MMEA_PO_PUSH)
            {
                gameseq_push_state(Party_options_menu_info.exit_push_target, true, true);
            }
            Party_options_menu_info.pending_state = MPOS_UNLOADED;
        }
    } while (Party_options_menu_info.state != Party_options_menu_info.pending_state);
}

// 0x0088CF90
static void party_options_init_state()
{
    Party_options_menu_info.state = MPOS_UNLOADED;
    Party_options_menu_info.pending_state = MPOS_UNLOADED;
    Party_options_menu_info.prev_state = MPOS_UNLOADED;
}

// 0x008AE190
static void party_options_init_info()
{
    Party_options_menu_info.menu_rows.clear();
    Party_options_menu_info.data_group_handle = -1;
}

// 0x008C0EB0
static void party_options_add_menu_row(vint_data_group* vdg)
{
    vint_data_item* item = vdg->add_data_item(nullptr);
    if (item != nullptr)
    {
        const vint_variant_type types[3] = {VINT_PROP_TYPE_INT, VINT_PROP_TYPE_STRING, VINT_PROP_TYPE_STRING};
        item->set_template(types, 3);
        Party_options_menu_info.menu_rows.add((int)item->get_handle());
    }
}

// 0x008AE1B0
static void party_options_remove_menu_row(vint_data_group* vdg)
{
    const int last = Party_options_menu_info.menu_rows.size() - 1;
    vdg->remove_data_item((unsigned int)Party_options_menu_info.menu_rows[last]);
    Party_options_menu_info.menu_rows.remove(last);
}

// 0x008D3570
static void party_options_load_data_groups()
{
    vint_data_group* group = vint_data_group::create_new("MpShellMenuInfo");
    if (group != nullptr)
    {
        Party_options_menu_info.data_group_handle = group->get_handle();
        ui_add_data_group(Party_options_menu_info.doc_handle, Party_options_menu_info.data_group_handle);
        // Original: party_options_load_menu_option_data_items (0x0088CFB0) looks the group up and adds nothing.
    }
}

// 0x008D35B0
static void party_options_load_ui_information()
{
    multi_player_menu_system_shell_load_data_items(Party_options_data_items, 5);
    party_options_load_data_groups(); // inlined
}

// 0x008E41C0
static void party_options_unload_ui_information()
{
    multi_player_menu_system_shell_unload_data_items(); // inlined
    Party_options_menu_info.menu_rows.clear();
    Party_options_menu_info.data_group_handle = -1;
    multi_player_menu_system_close_message_box(); // inlined
}

// 0x008E4250
static void party_options_process_data_item_hint_text(vint_data_item* vdi)
{
    party_options_hint_config[0] = PO_HINT_INVALID;
    party_options_hint_config[1] = PO_HINT_INVALID;
    party_options_num_hints = 0;
    multi_player_menu_system_begin_hints(vdi); // inlined
    // Magic numbers: buttons 10 select, 11 back.
    multi_player_menu_system_add_hint(localization_get_display_string_req("MENU_ACCEPT", nullptr), (control_button_action)Button_info.binfo[10].m_button_choice);
    party_options_hint_config[party_options_num_hints] = PO_HINT_ACCEPT;
    party_options_num_hints++;
    multi_player_menu_system_add_hint(localization_get_display_string_req(Back_string_poptions, nullptr), (control_button_action)Button_info.binfo[11].m_button_choice);
    party_options_hint_config[party_options_num_hints] = PO_HINT_ABORT;
    party_options_num_hints++;
    multi_player_menu_system_end_hints();
}

// 0x0088CFC0
static void party_options_process_data_item_titles(vint_data_item* vdi)
{
    vdi->set_element(0, vint_variant(Party_options));
    vdi->set_element(1, vint_variant(""));
}

// 0x0088D030
static void party_options_process_data_item_selection_info(vint_data_item* vdi)
{
    vdi->set_element(0, vint_variant(Party_options_menu_selection));
    vdi->set_element(1, vint_variant((const char*)nullptr));
    vdi->set_element(2, vint_variant(true));
}

// 0x0088D0C0
static void party_options_process_data_item_menu_type(vint_data_item* vdi)
{
    // Magic number: menu type 2 (rows with a value) of rfg_ui_mp_shell.
    vdi->set_element(0, vint_variant(2));
}

// 0x0088D100
static void party_options_process_data_item_playlist_info(vint_data_item* vdi)
{
    vdi->set_element(0, vint_variant(true));
    vdi->set_element(1, vint_variant(Menu_option_descriptions[Party_options_menu_selection]));
    vdi->set_element(2, vint_variant(""));
    vdi->set_element(3, vint_variant(false));
    vdi->set_element(4, vint_variant(""));
}

// 0x008C0F50
static void party_options_process_data_group_menu_options()
{
    vint_data_group* group = vint_data_group::find((unsigned int)Party_options_menu_info.data_group_handle);
    if (group == nullptr)
    {
        return;
    }
    while (group->get_num_data_items() == 0)
    {
        party_options_add_menu_row(group);
    }
    vint_data_item* item = vint_data_item::find((unsigned int)Party_options_menu_info.menu_rows[0]);
    if (item != nullptr)
    {
        item->set_element(0, vint_variant(1));
        item->set_element(1, vint_variant(Menu_option_names[0]));
        item->set_element(2, vint_variant(Party_lobby_invite_type_names[Desired_invite_type]));
    }
    while (group->get_num_data_items() > 1)
    {
        party_options_remove_menu_row(group); // inlined
    }
}

// 0x008F2D90
static void party_options_process_loading()
{
    switch (ui_get_doc_asset_state(Party_options_menu_info.doc_handle))
    {
    case ST_LOADING:
        return;
    case ST_LOADED:
        Party_options_menu_info.pending_state = MPOS_LOADED;
        return;
    case ST_UNLOADED:
        keen::debug::nativePrintArgs("Int3 critical function at %s(%d)\n", __FILE__, __LINE__);
        // fall through
    case ST_ERROR:
        multi_player_menu_system_shell_unload_doc(Party_options_menu_info.doc_handle); // inlined
        Party_options_menu_info.pending_state = MPOS_UNLOADING;
        return;
    default:
        keen::debug::nativePrintArgs("Int3 critical function at %s(%d)\n", __FILE__, __LINE__);
        return;
    }
}

// 0x008D3600
// The row under the mouse when it moved or was released (-1 for none); a release over an arrow is noted.
static int party_options_input_process_mouse_option_choice()
{
    if (!mouse_just_moved() && !mouse_just_released(0))
    {
        return -1;
    }
    const vint_doc_info* info = ui_get_doc_info(Party_options_menu_info.doc_handle);
    if (info == nullptr || info->m_doc == nullptr)
    {
        return -1;
    }
    vint_document* document = info->m_doc;
    // One row; rfg.exe keeps the loop (and the numbered name of the later rows, "%s%d").
    for (int i = 0; i < 1; i++)
    {
        char name[128];
        if (i == 0)
        {
            keen::formatString(name, sizeof(name), "%s", kGroup_name);
        }
        else
        {
            keen::formatString(name, sizeof(name), "%s%d", kGroup_name, i);
        }
        if (mouse_just_released(0))
        {
            if (ui_is_mouse_over_vint_object(vint_object_base::find_object(kRightArrow, document)))
            {
                Right_arrow_clicked_poptions = true;
            }
            else if (ui_is_mouse_over_vint_object(vint_object_base::find_object(kLeftArrow, document)))
            {
                Left_arrow_clicked_poptions = true;
            }
        }
        if (ui_is_mouse_over_vint_object(vint_object_base::find_object(name, document)))
        {
            return i;
        }
    }
    return -1;
}

// 0x008D3770
static void party_options_input_process_mouse(bool& events_consumed)
{
    if (Utility_doc_handle == -1 || !ui_is_msgbox_active(-2))
    {
        const int doc_handle = Party_options_menu_info.doc_handle;
        int hint = -1;
        if (ui_mouse_over_hint(kHintText2, doc_handle, true))
        {
            hint = 1;
        }
        if (ui_mouse_over_hint(kHintText1, doc_handle, true))
        {
            Current_mouse_hint_choice_poptions = (mp_party_options_hints)party_options_hint_config[0];
            return;
        }
        if (hint >= 0)
        {
            Current_mouse_hint_choice_poptions = (mp_party_options_hints)party_options_hint_config[hint];
            return;
        }
    }
    const int selection = party_options_input_process_mouse_option_choice();
    if (selection >= 0 && selection != Party_options_menu_selection)
    {
        multi_player_menu_system_set_menu_selection(selection, Party_options_menu_selection, 1);
    }
}

// 0x008FBE60
// LTCG: reads ECX (the events flag's stack slot only).
static void party_options_process_input()
{
    // Original bug: rfg.exe never initializes the flag (its slot is ECX, pushed as stack space); false here.
    bool events_consumed = false;
    Current_mouse_hint_choice_poptions = PO_HINT_INVALID;
    Right_arrow_clicked_poptions = false;
    Left_arrow_clicked_poptions = false;
    party_options_input_process_mouse(events_consumed);
    multi_player_menu_system_handle_menu_key_input(Party_options_menu_selection, 1, Party_options_menu_info.input_id, events_consumed);
    if (events_consumed)
    {
        return;
    }
    const int input_id = Party_options_menu_info.input_id;
    // Magic numbers: button 10 select, 3 and 7 one way, 4 and 8 the other, 11 back.
    const float select = ui_get_button_info(10, input_id);
    const bool next_pressed = ui_get_button_info(3, input_id) > 0.0f || ui_get_button_info(7, input_id) > 0.0f;
    const bool prev_pressed = ui_get_button_info(4, input_id) > 0.0f || ui_get_button_info(8, input_id) > 0.0f;
    const float back = ui_get_button_info(11, input_id);
    const bool accept = select > 0.0f || Current_mouse_hint_choice_poptions == PO_HINT_ACCEPT;
    const bool abort = back > 0.0f || Current_mouse_hint_choice_poptions == PO_HINT_ABORT;
    const bool prev = prev_pressed || Right_arrow_clicked_poptions;
    const bool next = next_pressed || Left_arrow_clicked_poptions;
    if (accept)
    {
        Lobby_session->set_party_lobby_invite_type(Desired_invite_type);
        Party_lobby_type = Desired_party_type;
        party_lobby_send_type_update();
        Party_options_menu_info.exit_action = MMEA_PO_POP;
        Party_options_menu_info.pending_state = MPOS_UNLOADING;
        audiolib_cue_play(Audio_info[4], nullptr, nullptr);
    }
    else if (abort)
    {
        if (Desired_invite_type == Lobby_session->get_party_lobby_invite_type() && Desired_party_type == Party_lobby_type)
        {
            Party_options_menu_info.exit_action = MMEA_PO_POP;
            Party_options_menu_info.pending_state = MPOS_UNLOADING;
        }
        else
        {
            ui_add_msgbox(MBT_ACCEPT_CANCEL, "MENU_WARNING", "MENU_CHANGES_LOST", mppo_confirm_abort, false, false, nullptr, nullptr, false);
        }
        audiolib_cue_play(Audio_info[5], nullptr, nullptr);
    }
    else if (next)
    {
        if (Party_options_menu_selection == 0)
        {
            Desired_invite_type = multi_game_party_next_invite_type(Desired_invite_type);
        }
        audiolib_cue_play(Audio_info[7], nullptr, nullptr);
    }
    else if (prev)
    {
        if (Party_options_menu_selection == 0)
        {
            Desired_invite_type = multi_game_party_prev_invite_type(Desired_invite_type);
        }
        audiolib_cue_play(Audio_info[8], nullptr, nullptr);
    }
}

// ---- external interface

// 0x0090D2D0
void ui_document_mp_party_options_enter(game_state prev_state)
{
    party_options_init_state();
    Party_options_menu_info.exit_action = MMEA_PO_POP;
    Party_options_menu_info.exit_push_target = GS_MULTIPLAYER_LIVE;
    Party_options_menu_selection = 0;
    party_options_init_info();
    multi_player_menu_system_shell_load_doc(Party_options_menu_info.doc_handle, false); // inlined
    Party_options_menu_info.control_id = ui_request_control_id();
    Party_options_menu_info.input_id = ui_register_for_processed_input(IRT_UNLIMITED_INPUT, false); // inlined
    Desired_invite_type = Lobby_session->get_party_lobby_invite_type();
    Desired_party_type = Party_lobby_type;
}

// 0x008E4340
void ui_document_mp_party_options_exit(game_state new_state)
{
    party_options_unload_ui_information();
    ui_relinquish_control_id(Party_options_menu_info.control_id); // inlined
    // Original bug kept: the document is not unloaded (Party_options_menu_info.doc_handle stays).
    ui_relinquish_input_id(Party_options_menu_info.input_id);
}

// 0x009152D0
// A game link error (past 8) leaves the menu.
void ui_document_mp_party_options_process()
{
    if (Bink_info_main.handle != 0)
    {
        mm_process_bink_video();
    }
    if (Party_options_menu_info.pending_state != MPOS_UNLOADING && Party_options_menu_info.state == MPOS_LOADED)
    {
        g_gameLinkMutex.lock(0);
        kaiko::gamelink::GameLink* link = get_game_link_session();
        // Magic number: the errors past 8 end the session.
        if (link != nullptr && (int)link->getError(nullptr) > 8)
        {
            Party_options_menu_info.exit_action = MMEA_PO_POP;
            Party_options_menu_info.pending_state = MPOS_UNLOADING;
        }
        g_gameLinkMutex.unlock();
    }
    multi_player_menu_system_process();
    ui_add_for_processing(Party_options_menu_info.doc_handle);
    if (Party_options_menu_info.state == MPOS_LOADING)
    {
        party_options_process_loading();
    }
    else if (Party_options_menu_info.state == MPOS_LOADED)
    {
        // party_options_process_loaded (0x008FC0D0), inlined.
        multi_player_menu_system_shell_process_data_items();
        party_options_process_data_group_menu_options();
        party_options_process_input();
    }
    else if (Party_options_menu_info.state == MPOS_UNLOADED)
    {
        Party_options_menu_info.pending_state = MPOS_LOADING;
    }
    ui_process(Party_options_menu_info.control_id, false);
}

// 0x008F2E40
void ui_document_mp_party_options_handshake(rl_renderer*)
{
    Party_options_menu_info.prev_state = Party_options_menu_info.state;
    if (Party_options_menu_info.pending_state != Party_options_menu_info.state)
    {
        party_options_do_state_change();
    }
}

// 0x008FC0E0
void ui_document_mp_party_options_render(rl_renderer*)
{
    if (Party_options_menu_info.doc_handle == -1 || Party_options_menu_info.control_id == 0)
    {
        return;
    }
    if (Party_options_menu_info.prev_state == MPOS_LOADED)
    {
        ui_add_for_rendering(Party_options_menu_info.doc_handle);
    }
    ui_render(Party_options_menu_info.control_id, false);
}

// 0x009153A0 (:854)
void ui_document_mp_party_options_init()
{
    state_info info;
    info.enter = ui_document_mp_party_options_enter;
    info.exit = ui_document_mp_party_options_exit;
    info.process = ui_document_mp_party_options_process;
    info.handshake = ui_document_mp_party_options_handshake;
    info.render = ui_document_mp_party_options_render;
    info.transparent = false;
    info.pause_beneath = false;
    gameseq_register_state(GS_MULTIPLAYER_PARTY_OPTIONS, info);
}

// 0x0088D220 (:871)
session_invite_type ui_document_mp_party_options_get_desired_invite_type()
{
    return Desired_invite_type;
}
