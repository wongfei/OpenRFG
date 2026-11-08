// Original: volition/rfg/code/interface/ui_menu.cpp
//
// The generic menu every vint menu document drives (UI.md UI4): a stack of menus (Menus, at most 8) from fixed pools,
// each with up to 16 items (function, char list, bool, increment), up to 4 button hints and the data items of its
// document (one document_info per document, shared by the menus on it): "MenuGroup<surtag>" (one data item per
// item), "TweenOnSelect<surtag>", "MenuTitle<surtag>" and "HintsInfo<surtag>". The document's Lua script draws the
// menu from them; selection, change and abort go through the menu's callbacks and the UI sounds.

#include "volition/rfg/code/interface/ui_menu.h"

#include "keen/base/string.hpp"
#include "keen/base/trace.hpp"
#include "volition/audio_lib/code/audiolib_api.h"
#include "volition/interface_lib/code/vint_data_group.h"
#include "volition/interface_lib/code/vint_data_item.h"
#include "volition/interface_lib/code/vint_variant.h"
#include "volition/render_lib/code/rl_screen_info.h"
#include "volition/rfg/code/gamewide/game_options.h"
#include "volition/rfg/code/gamewide/gameaudio.h"
#include "volition/rfg/code/gamewide/gameseq.h"
#include "volition/rfg/code/geom/anim_enums.h"
#include "volition/rfg/code/interface/loading.h"
#include "volition/rfg/code/interface/ui_character.h"
#include "volition/rfg/code/interface/ui_document_hud_missions.h"
#include "volition/rfg/code/multi/multi_core/include/multi.h"
#include "volition/rfg/code/multi/multi_core/include/multi_core_platform.h"
#include "volition/rfg/code/rfg.h"
#include "volition/vlib/code/os/vconsole.h"
#include "volition/vlib/code/time/timestamp.h"
#include "volition/vlib/code/util/string_util.h"
#include "volition/vlib/code/util/varray.h"

#include <string.h>

// ---- file-private types (rfg.pdb)
// size 0x260
struct alloc_info
{

    farray<ui_menu*,8> menu_free_vector; // 0x00
    farray<ui_menu_item_func*,32> menu_item_func_free_vector; // 0x30
    farray<ui_menu_item_char_list*,16> menu_item_char_list_free_vector; // 0xC0
    farray<ui_menu_item_bool*,16> menu_item_bool_free_vector; // 0x110
    farray<ui_menu_item_incr*,48> menu_item_incr_free_vector; // 0x160
    farray<document_info*,8> menu_document_info_free_vector; // 0x230
};
static_assert(sizeof(alloc_info) == 0x260, "alloc_info");

// ---- file-static globals
int Doc_handle_main = -1; // 0x016631AC
animation_action Play_action = ANIMATION_ACTION_NONE; // 0x016642BC, initial data ffffffff (external since final_push step 6, ui_character.cpp)
// Magic number: the character preview's melee 200 ms after a selection.
int Fx_delay = 200; // 0x016642C8 (external since final_push step 6, ui_character.cpp)
// int Choice_last (0x01664B38) is used here too; placed in ui_document_manager.cpp.
static const char* cEmpty = {""}; // 0x01664F84
static const char* cTrue = {"MENU_ENABLED"}; // 0x01664F88
static const char* cFalse = {"MENU_DISABLED"}; // 0x01664F8C
// The UI sounds' cue ids (gameaudio.cpp's Ga_cue_ids, in menu_sound_type order).
static const int* cUI_menu_sound_name[12] = {
    &Ga_cue_ids.ui_sound_move_down, &Ga_cue_ids.ui_sound_move_up, &Ga_cue_ids.ui_sound_slider_add,
    &Ga_cue_ids.ui_sound_slider_subtract, &Ga_cue_ids.ui_sound_press_select, &Ga_cue_ids.ui_sound_press_abort,
    &Ga_cue_ids.ui_sound_press_misc, &Ga_cue_ids.ui_sound_scroll_left, &Ga_cue_ids.ui_sound_scroll_right,
    &Ga_cue_ids.ui_sound_bumper_left, &Ga_cue_ids.ui_sound_bumper_right, &Ga_cue_ids.ui_sound_invalid,
}; // 0x01664F90
static const char* cMenu_group = {"MenuGroup"}; // 0x01664FC0
static const char* cTween_on_select = {"TweenOnSelect"}; // 0x01664FC4
static const char* cMenu_title = {"MenuTitle"}; // 0x01664FC8
static const char* cHints_info = {"HintsInfo"}; // 0x01664FCC
// bool Input_enabled_uimen (0x01664FD0) is used here too; placed in loading.cpp.
timestamp Fx_delay_timer; // 0x01666FD8, initial data ffffffff (file static in the binary; external since UI4, main_menu.cpp)
farray<ui_menu*,8> Menus; // 0x01667698
// ui_button_info Button_info (0x02BF1E30) is used here too; placed in ui_document_manager.cpp (the inlined
// "every button waits for its release" loops are ui_wait_for_button_ups here).
// int mp_stats_hint_config[7] (0x02BF214C) is used here too; placed in ui_message_box.cpp.
int Audio_info[12]; // 0x02BF21D0 (file static in the binary; external since UI5, the multiplayer menus)
static int Menu_handle_incr; // 0x02C081A0
static int Menu_item_base_handle_incr; // 0x02C081A4
static int Process_step; // 0x02C081A8
static ui_menu_item_func Menu_item_func_list[32]; // 0x02C0F540
static document_info Doc_info[8]; // 0x02C3CB80
// ui_menu_item_char_list Menu_item_char_list[16] (0x02C40170) is used here too; placed in ui_memory_manager.cpp.
extern ui_menu_item_char_list Menu_item_char_list[16]; // 0x02C40170
// __m128i s_value0xc8000000 (0x02C40BF0) is a compiler constant (SSE conversion), not a global of this file.
static ui_menu_item_incr Menu_item_incr_list[48]; // 0x02C49F30
// rl_level_of_detail Default_lods[5] (0x02C4BF70) is the display options' table (options_display.cpp).
static ui_menu Menu_list[8]; // 0x02C4CB60
static alloc_info Alloc_info; // 0x02C51E60
static ui_menu_item_bool Menu_item_bool_list[16]; // 0x02C52130
// diversion_group_info Diversion_info (0x02C52AF0) is the missions HUD's (ui_document_hud_missions.cpp, out of scope).


// The menu that has the handle (the inlined lookup of the public functions: the first match).
static ui_menu* find_menu(int handle)
{
    return ui_menu_get_via_handle(handle);
}

// A pool's free vector add (base_array::add as rfg.exe inlines it; a full vector logs the critical failure).
template <typename T>
static void free_vector_add(base_array<T*>& vector, T* item)
{
    if (vector.add(item) == -1)
    {
        keen::debug::nativePrintArgs("Int3 critical function at %s(%d)\n", __FILE__, __LINE__);
    }
}

// A pool's free vector take: the first entry, the rest moved down.
template <typename T>
static T* free_vector_take(base_array<T*>& vector)
{
    if (vector.size() <= 0)
    {
        return nullptr;
    }
    T* item = vector[0];
    vector.remove(0);
    return item;
}

// The data item's m_updated (protected; 0x42C) as rfg.exe's inlined code writes it.
static void data_item_set_updated(vint_data_item* item)
{
    *(bool*)((unsigned char*)item + 0x42C) = true;
}

// ---- hint_info, document_info

// 0x008A08B0 (:122)
hint_info::hint_info()
{
    button_action = NUM_CONTROL_BUTTON_ACTIONS;
    memset(hint_text, 0, sizeof(hint_text));
    memset(hint_image_name, 0, sizeof(hint_image_name));
}

// 0x008A0900 (:130)
void hint_info::set_info(hint_type htype, control_button_action action, const char* text, const char* image)
{
    if (text == nullptr)
    {
        memset(hint_text, 0, sizeof(hint_text));
    }
    else
    {
        keen::copyString(hint_text, sizeof(hint_text), text);
    }
    if (image == nullptr)
    {
        memset(hint_image_name, 0, sizeof(hint_image_name));
    }
    else
    {
        keen::copyString(hint_image_name, sizeof(hint_image_name), image);
    }
    button_action = action;
    type = htype;
    is_visible = text != nullptr;
}

// 0x008CAFC0 (:151)
document_info::document_info()
{
    needs_cleanup = -1;
    clear();
}

// 0x008B75E0
document_info::~document_info()
{
}

// 0x008B75F0 (:158)
void document_info::clear()
{
    data_handle_title = 0;
    data_handle_tween_on_select = 0;
    doc_handle = 0;
    data_handle_populate_group = 0;
    data_handle_hint_group = 0;
    data_handle_hints.remove_all();
    data_handle_populate_items.remove_all();
    memset(event_surtag, 0, sizeof(event_surtag));
}

// 0x008B7620 (:168)
unsigned int document_info::get_num_hints()
{
    return data_handle_hints.size();
}

// 0x008B7630 (:175)
unsigned int document_info::get_num_populate_items()
{
    return data_handle_populate_items.size();
}

// ---- the pools

// 0x008B7640 (:210)
// The data of freed document infos goes the step after their free (or at once when forced).
static void ui_menu_check_cleanup_of_data(bool force_cleanup)
{
    const int count = Alloc_info.menu_document_info_free_vector.size();
    for (int i = 0; i < count; i++)
    {
        document_info* info = Alloc_info.menu_document_info_free_vector[i];
        if (info == nullptr || info->needs_cleanup == -1 || (info->needs_cleanup != Process_step && !force_cleanup))
        {
            continue;
        }
        if (vint_data_group* group = vint_data_group::find((unsigned int)info->data_handle_hint_group))
        {
            group->destroy();
        }
        info->data_handle_hint_group = 0;
        if (vint_data_group* group = vint_data_group::find((unsigned int)info->data_handle_populate_group))
        {
            group->destroy();
        }
        info->data_handle_populate_group = 0;
        if (vint_data_item* item = vint_data_item::find((unsigned int)info->data_handle_title))
        {
            item->destroy();
        }
        info->data_handle_title = 0;
        if (vint_data_item* item = vint_data_item::find((unsigned int)info->data_handle_tween_on_select))
        {
            item->destroy();
        }
        info->data_handle_tween_on_select = 0;
        info->needs_cleanup = -1;
    }
}

// 0x008A0990 (:255)
static int ui_menu_item_base_get_new_handle()
{
    return Menu_item_base_handle_incr++;
}

// 0x008A09A0 (:261)
static int ui_menu_get_new_handle()
{
    return Menu_handle_incr++;
}

// 0x008B7710 (:267)
static ui_menu* ui_menu_get_topmost()
{
    if (Menus.size() > 0)
    {
        return Menus[Menus.size() - 1];
    }
    return nullptr;
}

// 0x008B7730 (:289)
ui_menu* ui_menu_get_via_handle(int handle)
{
    for (int i = 0; i < Menus.size(); i++)
    {
        if (Menus[i]->get_handle() == handle)
        {
            return Menus[i];
        }
    }
    return nullptr;
}

// 0x008B7760 (:306)
static ui_menu* ui_menu_get_new_menu()
{
    return free_vector_take(Alloc_info.menu_free_vector);
}

// 0x008CB020 (:318)
static void ui_menu_free_menu(ui_menu* menu)
{
    if (menu != nullptr)
    {
        free_vector_add(Alloc_info.menu_free_vector, menu);
    }
}

// 0x008B77B0 (:326)
static document_info* ui_menu_get_new_document_info()
{
    if (Alloc_info.menu_document_info_free_vector.size() <= 0)
    {
        return nullptr;
    }
    document_info* info = Alloc_info.menu_document_info_free_vector[0];
    info->clear();
    Alloc_info.menu_document_info_free_vector.remove(0);
    return info;
}

// 0x008CB060 (:339)
// The info's item data leaves its groups now; the groups and items themselves go with the cleanup, which is forced
// at once (so the step mark only matters for the infos a later free finds).
static void ui_menu_free_document_info(document_info* dinfo)
{
    if (dinfo == nullptr)
    {
        return;
    }
    if (vint_data_group* group = vint_data_group::find((unsigned int)dinfo->data_handle_populate_group))
    {
        for (int i = 0; i < dinfo->data_handle_populate_items.size(); i++)
        {
            group->remove_data_item((unsigned int)dinfo->data_handle_populate_items[i]);
        }
        dinfo->data_handle_populate_items.remove_all();
    }
    if (vint_data_group* group = vint_data_group::find((unsigned int)dinfo->data_handle_hint_group))
    {
        for (int i = 0; i < dinfo->data_handle_hints.size(); i++)
        {
            group->remove_data_item((unsigned int)dinfo->data_handle_hints[i]);
        }
        dinfo->data_handle_hints.remove_all();
    }
    // Magic number: the steps count modulo 128.
    dinfo->needs_cleanup = Process_step % 128;
    free_vector_add(Alloc_info.menu_document_info_free_vector, dinfo);
    ui_menu_check_cleanup_of_data(true);
}

// 0x008B7810 (:375)
static ui_menu_item_incr* ui_menu_get_new_menu_item_incr()
{
    return free_vector_take(Alloc_info.menu_item_incr_free_vector);
}

// 0x008CB150 (:387)
static void ui_menu_free_menu_item_incr(ui_menu_item_incr* item)
{
    if (item != nullptr)
    {
        free_vector_add(Alloc_info.menu_item_incr_free_vector, item);
    }
}

// 0x008B7860 (:395)
static ui_menu_item_bool* ui_menu_get_new_menu_item_bool()
{
    return free_vector_take(Alloc_info.menu_item_bool_free_vector);
}

// 0x008CB190 (:407)
static void ui_menu_free_menu_item_bool(ui_menu_item_bool* item)
{
    if (item != nullptr)
    {
        free_vector_add(Alloc_info.menu_item_bool_free_vector, item);
    }
}

// 0x008B78B0 (:415)
static ui_menu_item_char_list* ui_menu_get_new_menu_item_char_list()
{
    return free_vector_take(Alloc_info.menu_item_char_list_free_vector);
}

// 0x008CB1D0 (:427)
static void ui_menu_free_menu_item_char_list(ui_menu_item_char_list* item)
{
    if (item != nullptr)
    {
        free_vector_add(Alloc_info.menu_item_char_list_free_vector, item);
    }
}

// 0x008B7900 (:435)
static ui_menu_item_func* ui_menu_get_new_menu_item_func()
{
    return free_vector_take(Alloc_info.menu_item_func_free_vector);
}

// 0x008CB210 (:447)
static void ui_menu_free_menu_item_func(ui_menu_item_func* item)
{
    if (item != nullptr)
    {
        free_vector_add(Alloc_info.menu_item_func_free_vector, item);
    }
}

// The four data names of a document: the name with the document's surtag appended.
static void data_name(char* name, const char* base, const char* surtag)
{
    keen::copyString(name, 128, base);
    if (surtag != nullptr)
    {
        strncat(name, surtag, 128);
    }
}

// 0x008B7950 (:455)
// The document's menu group, tween item, title item and hints group (found by name, or made). A menu group that is
// still there (a menu on the document was freed) forces the pending cleanup first.
static void ui_menu_init_data_items(document_info* dinfo)
{
    if (dinfo == nullptr || dinfo->doc_handle == -1)
    {
        return;
    }
    const char* surtag = dinfo->event_surtag[0] != '\0' ? dinfo->event_surtag : nullptr;
    char name[128];
    data_name(name, cMenu_group, surtag);
    ui_menu_check_cleanup_of_data(vint_data_group::find(name) != nullptr);
    vint_data_group* group = vint_data_group::find(name);
    if (group == nullptr)
    {
        group = vint_data_group::create_new(name);
    }
    if (group != nullptr)
    {
        dinfo->data_handle_populate_group = (int)group->get_handle();
    }
    data_name(name, cTween_on_select, surtag);
    vint_data_item* item = vint_data_item::find(name);
    if (item == nullptr)
    {
        item = vint_data_item::create_new(name);
        if (item != nullptr)
        {
            dinfo->data_handle_tween_on_select = (int)item->get_handle();
            const vint_variant_type types[1] = {VINT_PROP_TYPE_INT};
            item->set_template(types, 1);
        }
    }
    else
    {
        dinfo->data_handle_tween_on_select = (int)item->get_handle();
    }
    data_name(name, cMenu_title, surtag);
    item = vint_data_item::find(name);
    if (item == nullptr)
    {
        item = vint_data_item::create_new(name);
        if (item != nullptr)
        {
            dinfo->data_handle_title = (int)item->get_handle();
            const vint_variant_type types[2] = {VINT_PROP_TYPE_BOOL, VINT_PROP_TYPE_STRING};
            item->set_template(types, 2);
        }
    }
    else
    {
        dinfo->data_handle_title = (int)item->get_handle();
    }
    data_name(name, cHints_info, surtag);
    group = vint_data_group::find(name);
    if (group == nullptr)
    {
        group = vint_data_group::create_new(name);
    }
    if (group != nullptr)
    {
        dinfo->data_handle_hint_group = (int)group->get_handle();
    }
}

// 0x008CB250 (:568)
// The document's menu group gets one data item per item of the menu (added or removed at the end).
static void ui_menu_check_add_data_items(ui_menu* menu)
{
    document_info* info = menu != nullptr ? menu->get_doc_info() : nullptr;
    if (info == nullptr || info->doc_handle == -1)
    {
        return;
    }
    int missing = menu->get_num_items() - info->data_handle_populate_items.size();
    if (missing > 0)
    {
        for (; missing != 0; missing--)
        {
            vint_data_group* group = vint_data_group::find((unsigned int)info->data_handle_populate_group);
            vint_data_item* item = group != nullptr ? group->add_data_item(nullptr) : nullptr;
            if (item != nullptr)
            {
                // fade in, last item, selected, visible, enabled, index, slider, caption, value.
                const vint_variant_type types[9] = {VINT_PROP_TYPE_BOOL, VINT_PROP_TYPE_BOOL, VINT_PROP_TYPE_BOOL, VINT_PROP_TYPE_BOOL, VINT_PROP_TYPE_BOOL, VINT_PROP_TYPE_INT, VINT_PROP_TYPE_BOOL, VINT_PROP_TYPE_STRING, VINT_PROP_TYPE_STRING};
                item->set_template(types, 9);
                info->data_handle_populate_items.add((int)item->get_handle());
            }
        }
        return;
    }
    for (int extra = -missing; extra > 0; extra--)
    {
        vint_data_group* group = vint_data_group::find((unsigned int)info->data_handle_populate_group);
        if (group != nullptr)
        {
            const int last = info->data_handle_populate_items.size() - 1;
            group->remove_data_item((unsigned int)info->data_handle_populate_items[last]);
            info->data_handle_populate_items.remove(last);
        }
    }
}

// 0x008B7B80 (:641)
static document_info* ui_menu_find_document_info(int doc_handle)
{
    for (int i = 0; i < Menus.size(); i++)
    {
        document_info* info = Menus[i]->get_doc_info();
        if (info != nullptr && info->doc_handle == doc_handle)
        {
            return info;
        }
    }
    return nullptr;
}

// 0x008B7BD0 (:657)
// Original: every menu's document info is read without a null check.
static int ui_menu_get_document_info_count(int doc_handle)
{
    int count = 0;
    for (int i = 0; i < Menus.size(); i++)
    {
        if (Menus[i]->get_doc_info()->doc_handle == doc_handle)
        {
            count++;
        }
    }
    return count;
}

// 0x008A09B0 (:673)
// The UI sounds' cue ids, once (they are precached).
static void ui_menu_init_audio_info()
{
    if (Audio_info[0] != 0)
    {
        return;
    }
    for (int i = 0; i < NUM_MENU_SOUNDS; i++)
    {
        Audio_info[i] = *cUI_menu_sound_name[i];
        audiolib_cue_precache(Audio_info[i]);
    }
}

// The button a menu action reads (ui_menu_get_button_info and the menu's hint buttons share the table).
static bool button_from_action(control_button_action action, button_choices& button)
{
    switch (action)
    {
    case CBA_MAP: button = BC_MAP; return true;
    case CBA_MENU_UP_NO_STICK:
    case CBA_MENU_UP: button = BC_UP; return true;
    case CBA_MENU_DOWN_NO_STICK:
    case CBA_MENU_DOWN: button = BC_DOWN; return true;
    case CBA_MENU_LEFT: button = BC_LEFT; return true;
    case CBA_MENU_RIGHT: button = BC_RIGHT; return true;
    case CBA_MENU_ALT_UP: button = BC_ALT_UP; return true;
    case CBA_MENU_ALT_DOWN: button = BC_ALT_DOWN; return true;
    case CBA_MENU_ALT_LEFT: button = BC_ALT_LEFT; return true;
    case CBA_MENU_ALT_RIGHT: button = BC_ALT_RIGHT; return true;
    case CBA_MENU_START: button = BC_START; return true;
    case CBA_MENU_SELECT: button = BC_SELECT; return true;
    case CBA_MENU_ABORT: button = BC_ABORT; return true;
    case CBA_MENU_PGUP: button = BC_PGUP; return true;
    case CBA_MENU_PGDN: button = BC_PGDOWN; return true;
    case CBA_MENU_X: button = BC_X; return true;
    case CBA_MENU_Y: button = BC_Y; return true;
    case CBA_MENU_L1: button = BC_L1; return true;
    case CBA_MENU_L2: button = BC_L2; return true;
    case CBA_MENU_R1: button = BC_R1; return true;
    case CBA_MENU_R2: button = BC_R2; return true;
    default: return false;
    }
}

// 0x008A09F0 (:686)
// LTCG: button_name in ESI, button_index in EAX, action in EDX. The menu button of an action and its button image's
// name (64 characters).
static bool ui_menu_get_button_info(char* button_name, button_choices& button_index, control_button_action action)
{
    button_index = BC_NUM_CHOICES;
    if (!button_from_action(action, button_index))
    {
        return false;
    }
    if (button_name != nullptr)
    {
        controls_get_button_image_name(button_name, 64, action, Player_controller_index);
    }
    return true;
}

// 0x008CB390 (:779)
void unused_dcf_enable_menu_item()
{
    bool usage = false;
    if (Console_command != 0)
    {
        if (console_get_arg(8, 0))
        {
            const int index = Console_arg_int;
            bool enabled = true;
            if (console_get_arg(8, 1) && Console_arg_int == 0)
            {
                enabled = false;
            }
            ui_menu* top = ui_menu_get_topmost();
            if (top != nullptr)
            {
                ui_menu_enable_menu_item(top->get_handle(), index, enabled);
            }
        }
        else
        {
            usage = true;
        }
    }
    if (Console_help != 0 || usage)
    {
        console_printf("Usage: enable_menu_item <index> <1/0>\n 1 specifies enabled, 0 is disabled.");
    }
}

// 0x008CB430 (:807)
void unused_dcf_show_menu_item()
{
    bool usage = false;
    if (Console_command != 0)
    {
        if (console_get_arg(8, 0))
        {
            const int index = Console_arg_int;
            bool visible = true;
            if (console_get_arg(8, 1) && Console_arg_int == 0)
            {
                visible = false;
            }
            ui_menu* top = ui_menu_get_topmost();
            if (top != nullptr)
            {
                ui_menu_show_menu_item(top->get_handle(), index, visible);
            }
        }
        else
        {
            usage = true;
        }
    }
    if (Console_help != 0 || usage)
    {
        console_printf("Usage: show_menu_item <index> <1/0>\n 1 specifies show, 0 is hide.");
    }
}

// ---- the items

// 0x008A0B50 (:841)
ui_menu_item_base::ui_menu_item_base()
{
    m_parent = nullptr;
    m_selected = false;
    m_enabled = true;
    m_visible = true;
    m_type = MI_INVALID;
    m_handle = ui_menu_item_base_get_new_handle();
}

// 0x008B7C10 (:847)
ui_menu_item_base::ui_menu_item_base(menu_item_type menu_type, const char* caption, ui_menu* parent)
{
    m_handle = ui_menu_item_base_get_new_handle();
    m_parent = parent;
    m_visible = true;
    m_selected = false;
    m_enabled = true;
    m_type = menu_type;
    if (caption != nullptr)
    {
        keen::copyString(m_caption, sizeof(m_caption), caption);
    }
    else
    {
        m_caption[0] = '\0';
    }
}

// 0x00883690 (.h:161)
ui_menu_item_base::~ui_menu_item_base()
{
}

// 0x008A0B90 (:854)
void ui_menu_item_base::ui_menu_item_base_init(menu_item_type menu_type, const char* caption, ui_menu* parent)
{
    m_visible = true;
    m_parent = parent;
    m_type = menu_type;
    m_selected = false;
    m_enabled = true;
    if (caption != nullptr)
    {
        keen::copyString(m_caption, sizeof(m_caption), caption);
    }
    else
    {
        m_caption[0] = '\0';
    }
}

// 0x008836A0 (.h:170)
void ui_menu_item_base::set_enabled(bool enabled)
{
    m_enabled = enabled;
}

// 0x008836B0 (.h:171)
bool ui_menu_item_base::get_enabled() const
{
    return m_enabled;
}

// 0x008836C0 (.h:173)
void ui_menu_item_base::set_visible(bool visible)
{
    m_visible = visible;
}

// 0x008836D0 (.h:174)
bool ui_menu_item_base::get_visible() const
{
    return m_visible;
}

// 0x008836E0 (.h:176)
int ui_menu_item_base::get_handle()
{
    return m_handle;
}

// 0x008836F0 (.h:178)
const char* ui_menu_item_base::get_caption() const
{
    return m_caption;
}

// 0x00883700 (.h:181)
menu_item_type ui_menu_item_base::get_menu_item_type()
{
    return m_type;
}

// 0x00883710 (.h:184)
bool ui_menu_item_base::get_play_change_sound()
{
    return m_play_change_sound;
}

// 0x00883720 (.h:185)
void ui_menu_item_base::set_play_change_sound(bool play)
{
    m_play_change_sound = play;
}

// 0x008A0BE0 (:875)
ui_menu_item_func::ui_menu_item_func()
{
}

// 0x008B7C80 (:880)
ui_menu_item_func::ui_menu_item_func(const char* caption, ui_menu* parent, void (*scb)(int))
{
    ui_menu_item_func_init(caption, parent, scb);
}

// 0x00883750 (.h:207)
ui_menu_item_func::~ui_menu_item_func()
{
}

// 0x008A0C40 (:886)
void ui_menu_item_func::ui_menu_item_func_init(const char* caption, ui_menu* parent, void (*scb)(int))
{
    ui_menu_item_base_init(MI_FUNCTION, caption, parent);
    m_should_tween_out = true;
    m_selection_callback = scb;
}

// 0x008A0CB0 (:895)
void ui_menu_item_func::set_should_tween_out(bool value)
{
    m_should_tween_out = value;
}

// 0x008A0CC0 (:901)
bool ui_menu_item_func::get_should_tween_out()
{
    return m_should_tween_out;
}

// 0x008A0CD0 (:908)
void ui_menu_item_func::set_surpress_accept_sound(bool value)
{
    m_surpress_accept_sound = value;
}

// 0x008A0CE0 (:914)
bool ui_menu_item_func::get_surpress_accept_sound()
{
    return m_surpress_accept_sound;
}

// 0x008B7CE0 (:920)
// The selection callback gets the item's index (-1 when it is not in its menu); every button then waits for its
// release.
void ui_menu_item_func::handle_selection()
{
    if (m_selection_callback != nullptr)
    {
        m_selection_callback(m_parent->get_menu_item_index(m_handle));
    }
    ui_wait_for_button_ups();
}

// 0x008A0CF0 (:930)
void ui_menu_item_func::get_text_value(char* buff, int buff_size) const
{
    if (buff != nullptr && buff_size > 0)
    {
        strncpy_safe(buff, cEmpty, buff_size);
    }
}

// 0x008A0D20 (:938)
void ui_menu_item_func::set_should_set_active_controller(bool value)
{
    m_set_active_controller = value;
}

// 0x008A0D30 (:944)
bool ui_menu_item_func::get_should_set_active_controller()
{
    return m_set_active_controller;
}

// 0x008A0D40 (:956)
ui_menu_item_char_list::ui_menu_item_char_list()
{
    m_which_char = 0;
    m_num_chars = 0;
    m_variable = nullptr;
    m_item_change_callback = nullptr;
}

// 0x008B7D50 (:961)
ui_menu_item_char_list::ui_menu_item_char_list(const char* caption, ui_menu* parent, const char** char_list, int num_chars, void (*iccb)(int, void*))
{
    ui_menu_item_char_list_init(caption, parent, char_list, num_chars, iccb, 0);
}

// 0x00883760 (.h:237)
ui_menu_item_char_list::~ui_menu_item_char_list()
{
}

// 0x008A0DB0 (:967)
void ui_menu_item_char_list::ui_menu_item_char_list_init(const char* caption, ui_menu* parent, const char** char_list, int num_chars, void (*iccb)(int, void*), int selected_idx)
{
    ui_menu_item_base_init(MI_CHAR_LIST, caption, parent);
    if (selected_idx < 0 || selected_idx >= num_chars)
    {
        keen::debug::nativePrintArgs("Int3 critical function at %s(%d)\n", __FILE__, __LINE__);
        m_which_char = 0;
    }
    else
    {
        m_which_char = selected_idx;
    }
    m_num_chars = num_chars;
    m_variable = char_list;
    m_item_change_callback = iccb;
}

// 0x008B7DC0 (:990)
// Up and down cycle the entries; the change callback gets the item's index and the new entry's index.
bool ui_menu_item_char_list::handle_input(increment_type type)
{
    if (m_variable == nullptr)
    {
        return true;
    }
    if (type == IT_UP)
    {
        m_which_char++;
        if (m_which_char >= m_num_chars)
        {
            m_which_char = 0;
        }
    }
    else
    {
        m_which_char--;
        if (m_which_char < 0)
        {
            m_which_char = m_num_chars - 1;
        }
    }
    if (m_item_change_callback != nullptr)
    {
        m_item_change_callback(m_parent->get_menu_item_index(m_handle), (void*)(intptr_t)m_which_char);
    }
    return true;
}

// 0x008A0E50 (:1015)
void ui_menu_item_char_list::get_text_value(char* buff, int buff_size) const
{
    if (buff == nullptr || buff_size <= 0)
    {
        return;
    }
    strncpy_safe(buff, cEmpty, buff_size);
    if (m_which_char > -1 && m_which_char < m_num_chars)
    {
        keen::copyString(buff, buff_size, m_variable[m_which_char]);
    }
}

// 0x008A0EB0 (:1029)
void ui_menu_item_char_list::set_selected_char(int index)
{
    if (index > -1 && index < m_num_chars)
    {
        m_which_char = index;
    }
}

// 0x00883770 (.h:245)
int ui_menu_item_char_list::get_num_chars()
{
    return m_num_chars;
}

// 0x008A0ED0 (:1041)
ui_menu_item_bool::ui_menu_item_bool()
{
    m_variable = nullptr;
    m_item_change_callback = nullptr;
}

// 0x008B7E40 (:1046)
ui_menu_item_bool::ui_menu_item_bool(const char* caption, ui_menu* parent, void* variable, void (*iccb)(int, void*))
{
    ui_menu_item_bool_init(caption, parent, variable, iccb);
}

// 0x00883780 (.h:264)
ui_menu_item_bool::~ui_menu_item_bool()
{
}

// 0x008A0F40 (:1052)
void ui_menu_item_bool::ui_menu_item_bool_init(const char* caption, ui_menu* parent, void* variable, void (*iccb)(int, void*))
{
    ui_menu_item_base_init(MI_BOOL, caption, parent);
    m_variable = variable;
    m_item_change_callback = iccb;
}

// 0x008B7EA0 (:1062)
// Either direction toggles the bool.
bool ui_menu_item_bool::handle_input(increment_type)
{
    if (m_variable == nullptr)
    {
        return true;
    }
    bool* value = (bool*)m_variable;
    *value = !*value;
    if (m_item_change_callback != nullptr)
    {
        m_item_change_callback(m_parent->get_menu_item_index(m_handle), m_variable);
    }
    return true;
}

// 0x008A0FB0 (:1076)
void ui_menu_item_bool::get_text_value(char* buff, int buff_size) const
{
    if (buff == nullptr || buff_size <= 0)
    {
        return;
    }
    strncpy_safe(buff, cFalse, buff_size);
    if (m_variable != nullptr)
    {
        _snprintf_safe(buff, buff_size, "%s", *(const bool*)m_variable ? cTrue : cFalse);
    }
}

// 0x008A1010 (:1094)
ui_menu_item_incr::ui_menu_item_incr()
{
    m_variable = nullptr;
    m_item_change_callback = nullptr;
    m_incr_type = MIT_INVALID;
}

// 0x008B7EF0 (:1099)
ui_menu_item_incr::ui_menu_item_incr(menu_incr_type type, const char* caption, ui_menu* parent, void* variable, void (*iccb)(int, void*))
{
    ui_menu_item_incr_init(type, caption, parent, variable, iccb);
}

// 0x00883790 (.h:285)
ui_menu_item_incr::~ui_menu_item_incr()
{
}

// 0x008A1080 (:1105)
// Magic numbers: the default ranges, 0..10 by 1 (int and exponential) and 0..100 by 10 (slider).
void ui_menu_item_incr::ui_menu_item_incr_init(menu_incr_type type, const char* caption, ui_menu* parent, void* variable, void (*iccb)(int, void*))
{
    ui_menu_item_base_init(MI_INCR, caption, parent);
    m_incr_type = type;
    m_variable = variable;
    m_item_change_callback = iccb;
    if (type == MIT_INT || type == MIT_EXPONENTIAL)
    {
        m_max.m_int = 10;
        m_incr.m_int = 1;
        m_min.m_int = 0;
    }
    else if (type == MIT_SLIDER)
    {
        m_min.m_int = 0;
        m_max.m_int = 100;
        m_incr.m_int = 10;
    }
}

// 0x008837A0 (.h:289)
menu_incr_type ui_menu_item_incr::get_incr_type() const
{
    return m_incr_type;
}

// 0x008A1130 (:1135)
void ui_menu_item_incr::set_info(variable_type min, variable_type max, variable_type incr)
{
    if ((unsigned int)m_incr_type < (unsigned int)MIT_LAST)
    {
        m_min = min;
        m_max = max;
        m_incr = incr;
    }
}

// 0x008A1160 (:1152)
void ui_menu_item_incr::get_info(variable_type& min, variable_type& max, variable_type& incr)
{
    if ((unsigned int)m_incr_type < (unsigned int)MIT_LAST)
    {
        min = m_min;
        max = m_max;
        incr = m_incr;
    }
}

// 0x008B7F50 (:1169)
// Int and slider items step by the increment within min..max; an exponential item multiplies or divides by its
// base (the increment) within base^min..base^max. False when the value did not change.
bool ui_menu_item_incr::handle_input(increment_type type)
{
    int* value = (int*)m_variable;
    if (value == nullptr)
    {
        return false;
    }
    int new_value;
    if (m_incr_type == MIT_INT || m_incr_type == MIT_SLIDER)
    {
        if (type == IT_UP)
        {
            new_value = *value + m_incr.m_int;
            if (new_value > m_max.m_int)
            {
                return false;
            }
        }
        else
        {
            new_value = *value - m_incr.m_int;
            if (new_value < m_min.m_int)
            {
                return false;
            }
        }
    }
    else if (m_incr_type == MIT_EXPONENTIAL)
    {
        // x87: pow of the int base and exponent, stored as a float, against the new value as a double.
        if (type == IT_UP)
        {
            new_value = *value * m_incr.m_int;
            const float limit = (float)pow((double)m_incr.m_int, (double)m_max.m_int);
            if (limit < (double)new_value)
            {
                return false;
            }
        }
        else
        {
            new_value = *value / m_incr.m_int;
            const float limit = (float)pow((double)m_incr.m_int, (double)m_min.m_int);
            if ((double)new_value < limit)
            {
                return false;
            }
        }
    }
    else
    {
        return false;
    }
    *value = new_value;
    if (m_item_change_callback != nullptr)
    {
        m_item_change_callback(m_parent->get_menu_item_index(m_handle), m_variable);
    }
    return true;
}

// 0x008A1190 (:1223)
// Original: the slider's format "%i%%%%" prints two percent signs.
void ui_menu_item_incr::get_text_value(char* buff, int buff_size) const
{
    if (buff == nullptr || buff_size <= 0)
    {
        return;
    }
    strncpy_safe(buff, cEmpty, buff_size);
    if (m_variable == nullptr)
    {
        return;
    }
    if (m_incr_type == MIT_INT || m_incr_type == MIT_EXPONENTIAL)
    {
        _snprintf_safe(buff, buff_size, "%d", *(const int*)m_variable);
    }
    else if (m_incr_type == MIT_SLIDER)
    {
        _snprintf_safe(buff, buff_size, "%i%%%%", *(const int*)m_variable);
    }
}

// ---- ui_menu

// 0x008CB4D0 (:1261)
ui_menu::ui_menu()
{
    m_handle = ui_menu_get_new_handle();
    m_awaiting_tween_on_select_end = false;
    m_awaiting_tween_fade_in_end = true;
    m_doc_handle = -1;
    m_choose_on_enter = -1;
    m_input_id = -1;
    m_doc_info = nullptr;
    m_num_items = 0;
    m_selected = 0;
    m_selection_change_cb = nullptr;
    m_selection_hint_cb = nullptr;
    memset(m_items, 0, sizeof(m_items));
    m_hint_info.remove_all();
    _snprintf_safe(m_title, sizeof(m_title), "Menu %d", m_handle);
}

// 0x008CB570 (:1270)
ui_menu::ui_menu(int doc_handle, const char* title)
{
    m_handle = ui_menu_get_new_handle();
    init(doc_handle, title, nullptr, nullptr, -1);
}

// 0x008B8080 (:1279)
void ui_menu::init(int doc_handle, const char* title, void (*change_cb)(int), void (*back_cb)(hint_type), int input_id)
{
    m_doc_handle = doc_handle;
    m_selection_change_cb = change_cb;
    m_selection_hint_cb = back_cb;
    m_input_id = input_id;
    m_awaiting_tween_on_select_end = false;
    m_awaiting_tween_fade_in_end = true;
    m_choose_on_enter = -1;
    m_doc_info = nullptr;
    m_num_items = 0;
    m_selected = 0;
    memset(m_items, 0, sizeof(m_items));
    m_hint_info.remove_all();
    if (title != nullptr)
    {
        keen::copyString(m_title, sizeof(m_title), title);
    }
    else
    {
        _snprintf_safe(m_title, sizeof(m_title), "Menu %d", m_handle);
    }
}

// 0x008DBF60 (:1309)
ui_menu::~ui_menu()
{
    remove_all_items();
}

// 0x008837B0 (.h:320)
int ui_menu::get_handle()
{
    return m_handle;
}

// The selected item when it is a function item: its selection runs (the end of a select tween, or a selection
// without one).
static void run_selected_function(ui_menu_item_base* item)
{
    if (item->get_menu_item_type() == MI_FUNCTION)
    {
        ((ui_menu_item_func*)item)->handle_selection();
    }
}

// 0x008DBF80 (:1316)
// The menu's input, while its fade in and select tweens are done: abort and misc hints first (their callback and
// sound), then up/down move the selection (more than one item), else the selected item: a function item selects
// (tweening out unless it is told not to), the others change with left/right. Without a function item in the menu,
// select answers the selection hint.
void ui_menu::process_input()
{
    if (m_doc_info == nullptr || multi_core_platform_ui_is_active() || m_awaiting_tween_fade_in_end || m_awaiting_tween_on_select_end)
    {
        return;
    }
    const int id = m_input_id;
    const bool select = ui_get_button_info(get_select_button(), id) > 0.0f;
    const bool left = ui_get_button_info(BC_LEFT, id) > 0.0f || ui_get_button_info(BC_ALT_LEFT, id) > 0.0f;
    const bool right = ui_get_button_info(BC_RIGHT, id) > 0.0f || ui_get_button_info(BC_ALT_RIGHT, id) > 0.0f;
    const bool up = ui_get_button_info(BC_UP, id) > 0.0f || ui_get_button_info(BC_ALT_UP, id) > 0.0f;
    const bool down = ui_get_button_info(BC_DOWN, id) > 0.0f || ui_get_button_info(BC_ALT_DOWN, id) > 0.0f;
    const bool abort = ui_get_button_info(get_abort_button(), m_input_id) > 0.0f;
    const bool misc = ui_get_button_info(get_misc_button(), m_input_id) > 0.0f;
    const bool misc_2 = ui_get_button_info(get_misc_2_button(), m_input_id) > 0.0f;
    if (m_selection_hint_cb != nullptr)
    {
        if (abort)
        {
            m_selection_hint_cb(HT_ABORT);
            audiolib_cue_play(Audio_info[MENU_SOUND_PRESS_ABORT], nullptr, nullptr);
            ui_wait_for_button_ups();
            return;
        }
        if (misc || misc_2)
        {
            m_selection_hint_cb(misc ? HT_MISC : HT_MISC_2);
            audiolib_cue_play(Audio_info[MENU_SOUND_PRESS_MISC], nullptr, nullptr);
            ui_wait_for_button_ups();
            return;
        }
    }
    if (m_num_items > 1 && (up || down))
    {
        adjust_selected_item(up);
        if (m_selection_change_cb != nullptr)
        {
            m_selection_change_cb(m_selected);
        }
        audiolib_cue_play(Audio_info[up ? MENU_SOUND_MOVE_UP : MENU_SOUND_MOVE_DOWN], nullptr, nullptr);
        return;
    }
    bool has_function = false;
    for (unsigned int i = 0; i < m_num_items; i++)
    {
        ui_menu_item_base* item = m_items[i];
        if (item == nullptr || !item->get_enabled() || !item->get_visible())
        {
            continue;
        }
        if (!has_function)
        {
            has_function = item->get_menu_item_type() == MI_FUNCTION;
        }
        if (i != (unsigned int)m_selected)
        {
            continue;
        }
        if (item->get_menu_item_type() == MI_FUNCTION)
        {
            if (!select)
            {
                continue;
            }
            ui_menu_item_func* func = (ui_menu_item_func*)item;
            m_awaiting_tween_on_select_end = true;
            if (!func->get_should_tween_out())
            {
                run_selected_function(get_menu_item_via_index(m_selected));
                m_force_update = true;
                m_awaiting_tween_on_select_end = false;
            }
            if (!func->get_surpress_accept_sound() && !m_surpress_accept_sounds)
            {
                audiolib_cue_play(Audio_info[MENU_SOUND_PRESS_SELECT], nullptr, nullptr);
            }
            if (func->get_should_set_active_controller())
            {
                // Original: the select button is asked for again and not used; the options are applied (outside the
                // boot) as the active controller's.
                get_select_button();
                if (gameseq_get_state() != GS_BOOT)
                {
                    game_options_load_from_cache();
                }
            }
            ui_wait_for_button_ups();
            ui_character_trigger(true, m_selected);
            continue;
        }
        const bool play = item->get_play_change_sound();
        bool changed = false;
        if (!left && !right)
        {
            continue;
        }
        menu_incr_type incr_type;
        const menu_item_type type = get_menu_item_type(item->get_handle(), incr_type);
        int sound = -1;
        // Left steps down, right up.
        const increment_type step = left ? IT_DOWN : IT_UP;
        if (type == MI_CHAR_LIST)
        {
            changed = ((ui_menu_item_char_list*)item)->handle_input(step);
            if (changed && ((ui_menu_item_char_list*)item)->get_num_chars() == 2)
            {
                // A two-entry list sounds like a toggle.
                sound = MENU_SOUND_SLIDER_ADD;
            }
        }
        else if (type == MI_BOOL)
        {
            changed = ((ui_menu_item_bool*)item)->handle_input(step);
            if (changed)
            {
                sound = MENU_SOUND_SLIDER_ADD;
            }
        }
        else if (type == MI_INCR)
        {
            changed = ((ui_menu_item_incr*)item)->handle_input(step);
            if (changed && incr_type == MIT_SLIDER)
            {
                sound = left ? MENU_SOUND_SLIDER_SUBTRACT : MENU_SOUND_SLIDER_ADD;
            }
        }
        if (!play)
        {
            continue;
        }
        if (sound == -1)
        {
            sound = changed ? (left ? MENU_SOUND_SCROLL_LEFT : MENU_SOUND_SCROLL_RIGHT) : MENU_SOUND_INVALID_INPUT;
        }
        audiolib_cue_play(Audio_info[sound], nullptr, nullptr);
    }
    if (has_function)
    {
        return;
    }
    if (select && m_selection_hint_cb != nullptr)
    {
        m_selection_hint_cb(HT_SELECTION);
        if (!m_surpress_accept_sounds)
        {
            audiolib_cue_play(Audio_info[MENU_SOUND_PRESS_SELECT], nullptr, nullptr);
        }
        ui_wait_for_button_ups();
    }
}

// 0x008DC420 (:1549)
void ui_menu::process_data_items()
{
    if (m_doc_info != nullptr)
    {
        process_populate_items();
        process_title();
        process_tween_on_select_end();
        process_hints();
        m_force_update = false;
    }
}

// 0x008B8130 (:1561)
// Each item's data item: fade in, last item while fading in, selected, visible, enabled, the 1-based index, slider,
// caption and value text (64 characters).
void ui_menu::process_populate_items()
{
    if (m_doc_info == nullptr || m_awaiting_tween_on_select_end)
    {
        return;
    }
    vint_data_group* group = vint_data_group::find((unsigned int)m_doc_info->data_handle_populate_group);
    if (group == nullptr)
    {
        return;
    }
    group->get_num_data_items();
    const unsigned int count = (unsigned int)m_doc_info->data_handle_populate_items.size();
    for (unsigned int i = 0; i < count; i++)
    {
        ui_menu_item_base* item = m_items[i];
        vint_data_item* data = vint_data_item::find((unsigned int)m_doc_info->data_handle_populate_items[i]);
        if (data == nullptr || item == nullptr)
        {
            continue;
        }
        if (m_force_update)
        {
            data_item_set_updated(data);
        }
        char value[64];
        char caption[128];
        item->get_text_value(value, sizeof(value));
        keen::copyString(caption, sizeof(caption), item->get_caption());
        keen::copyString(value, sizeof(value), value);
        data->set_element(0, vint_variant(m_awaiting_tween_fade_in_end));
        data->set_element(1, vint_variant(m_awaiting_tween_fade_in_end && i == m_num_items - 1));
        data->set_element(2, vint_variant((unsigned int)m_selected == i));
        data->set_element(3, vint_variant(item->get_visible()));
        data->set_element(4, vint_variant(item->get_enabled()));
        data->set_element(5, vint_variant((int)(i + 1)));
        const bool slider = item->get_menu_item_type() == MI_INCR && ((ui_menu_item_incr*)item)->get_incr_type() == MIT_SLIDER;
        data->set_element(6, vint_variant(slider));
        data->set_element(7, vint_variant((const char*)caption));
        data->set_element(8, vint_variant((const char*)value));
    }
}

// 0x008A1210 (:1637)
void ui_menu::process_title()
{
    if (m_doc_info == nullptr)
    {
        return;
    }
    vint_data_item* item = vint_data_item::find((unsigned int)m_doc_info->data_handle_title);
    if (item != nullptr)
    {
        item->set_element(0, vint_variant(Multiplayer));
        item->set_element(1, vint_variant((const char*)m_title));
    }
}

// 0x008A12A0 (:1651)
// The 1-based index of the item whose select tween runs, 0 without one.
void ui_menu::process_tween_on_select_end()
{
    if (m_doc_info == nullptr)
    {
        return;
    }
    vint_data_item* item = vint_data_item::find((unsigned int)m_doc_info->data_handle_tween_on_select);
    if (item != nullptr)
    {
        item->set_element(0, vint_variant(m_awaiting_tween_on_select_end ? m_selected + 1 : 0));
    }
}

// 0x008CB5C0 (:1669)
// One data item per visible hint in the hints group: the 1-based index, the text and "[image:<button>]".
void ui_menu::process_hints()
{
    if (m_doc_info == nullptr)
    {
        return;
    }
    vint_data_group* group = vint_data_group::find((unsigned int)m_doc_info->data_handle_hint_group);
    if (group == nullptr)
    {
        return;
    }
    int items = (int)group->get_num_data_items();
    const int hints = m_hint_info.size();
    int visible = 0;
    for (int i = 0; i < hints; i++)
    {
        if (m_hint_info[i].is_visible)
        {
            visible++;
        }
    }
    if (visible < items)
    {
        while (visible < items)
        {
            items--;
            group->remove_data_item((unsigned int)m_doc_info->data_handle_hints[items]);
            m_doc_info->data_handle_hints.remove(items);
        }
    }
    else
    {
        while (items < visible)
        {
            vint_data_item* item = group->add_data_item(nullptr);
            if (item == nullptr)
            {
                break;
            }
            const vint_variant_type types[3] = {VINT_PROP_TYPE_INT, VINT_PROP_TYPE_STRING, VINT_PROP_TYPE_STRING};
            item->set_template(types, 3);
            m_doc_info->data_handle_hints.add((int)item->get_handle());
            items++;
        }
    }
    int index = 0;
    for (int i = 0; i < hints; i++)
    {
        const hint_info& hint = m_hint_info[i];
        if (!hint.is_visible)
        {
            continue;
        }
        if (index >= m_doc_info->data_handle_hints.size())
        {
            keen::debug::nativePrintArgs("Int3 critical function at %s(%d)\n", __FILE__, __LINE__);
            return;
        }
        vint_data_item* item = vint_data_item::find((unsigned int)m_doc_info->data_handle_hints[index]);
        if (item != nullptr)
        {
            item->set_element(0, vint_variant(index + 1));
            item->set_element(1, vint_variant((const char*)hint.hint_text));
            char image[128];
            keen::formatString(image, sizeof(image), "[image:%s]", hint.hint_image_name);
            item->set_element(2, vint_variant((const char*)image));
        }
        index++;
    }
}

// 0x008DC450 (:1752)
void ui_menu::process(float, bool)
{
    if (((unsigned int)m_selected >= m_num_items || !m_items[m_selected]->get_enabled() || !m_items[m_selected]->get_visible()) && m_num_items > 1)
    {
        adjust_selected_item();
    }
    process_data_items();
    if (Input_enabled_uimen)
    {
        process_input();
    }
}

// 0x008A1320 (:1768)
int ui_menu::get_num_items()
{
    return (int)m_num_items;
}

// 0x008A1330 (:1773)
int ui_menu::get_num_visible_items()
{
    int count = 0;
    for (unsigned int i = 0; i < m_num_items; i++)
    {
        if (m_items[i]->get_visible())
        {
            count++;
        }
    }
    return count;
}

// 0x008A1360 (:1786)
ui_menu_item_base* ui_menu::get_menu_item_via_index(int which)
{
    if (which > -1 && which < (int)m_num_items)
    {
        return m_items[which];
    }
    return nullptr;
}

// 0x008A1380 (:1796)
ui_menu_item_base* ui_menu::get_menu_item_via_handle(int handle)
{
    for (unsigned int i = 0; i < m_num_items; i++)
    {
        if (m_items[i]->get_handle() == handle)
        {
            return m_items[i];
        }
    }
    return nullptr;
}

// 0x008A13C0 (:1811)
bool ui_menu::select_menu_item_by_index(int which)
{
    if (which > -1 && which < (int)m_num_items)
    {
        m_selected = which;
        return true;
    }
    return false;
}

// 0x008A13E0 (:1824)
bool ui_menu::select_menu_item(int handle)
{
    const int index = get_menu_item_index(handle);
    if (index == -1)
    {
        return false;
    }
    m_selected = index;
    return true;
}

// 0x008A1420 (:1843)
int ui_menu::get_menu_item_handle(int index)
{
    if (index > -1 && index < (int)m_num_items)
    {
        return m_items[index]->get_handle();
    }
    return -1;
}

// 0x008A1440 (:1854)
int ui_menu::get_menu_item_index(int handle)
{
    for (unsigned int i = 0; i < m_num_items; i++)
    {
        if (m_items[i]->get_handle() == handle)
        {
            return (int)i;
        }
    }
    return -1;
}

// 0x008B83E0 (:1870)
// Original: always returns false. Disabling the selected item moves the selection.
bool ui_menu::enable_menu_item(int handle, bool enable)
{
    const int index = get_menu_item_index(handle);
    if (index != -1)
    {
        const bool was = m_items[index]->get_enabled();
        m_items[index]->set_enabled(enable);
        // Original: the selection moves when the selected item was enabled before (also when it is enabled again).
        if (m_selected == index && was)
        {
            adjust_selected_item();
        }
    }
    return false;
}

// 0x008A1480 (:1894)
bool ui_menu::is_menu_item_enabled(int handle)
{
    const int index = get_menu_item_index(handle);
    return index != -1 && m_items[index]->get_enabled();
}

// 0x008A14C0 (:1910)
bool ui_menu::is_menu_item_enabled(unsigned int index)
{
    return index < m_num_items && m_items[index]->get_enabled();
}

// 0x008B8440 (:1921)
// Original: always returns false. Hiding the selected item moves the selection.
bool ui_menu::show_menu_item(int handle, bool visible)
{
    const int index = get_menu_item_index(handle);
    if (index != -1)
    {
        const bool was = m_items[index]->get_visible();
        m_items[index]->set_visible(visible);
        if (m_selected == index && was && !visible)
        {
            adjust_selected_item();
        }
    }
    return false;
}

// 0x008A14E0 (:1945)
bool ui_menu::is_menu_item_visible(int handle)
{
    const int index = get_menu_item_index(handle);
    return index != -1 && m_items[index]->get_visible();
}

// 0x008A1520 (:1961)
bool ui_menu::is_menu_item_visible(unsigned int index)
{
    return index < m_num_items && m_items[index]->get_visible();
}

// 0x008A1540 (:1972)
menu_item_type ui_menu::get_menu_item_type(int handle, menu_incr_type& incr_type)
{
    incr_type = MIT_INVALID;
    ui_menu_item_base* item = get_menu_item_via_handle(handle);
    if (item == nullptr)
    {
        return MI_INVALID;
    }
    const menu_item_type type = item->get_menu_item_type();
    if (type == MI_INCR)
    {
        incr_type = ((ui_menu_item_incr*)item)->get_incr_type();
    }
    return type;
}

// The item back to its pool.
static void free_item(ui_menu_item_base* item)
{
    switch (item->get_menu_item_type())
    {
    case MI_FUNCTION:
        ui_menu_free_menu_item_func((ui_menu_item_func*)item);
        break;
    case MI_CHAR_LIST:
        ui_menu_free_menu_item_char_list((ui_menu_item_char_list*)item);
        break;
    case MI_BOOL:
        ui_menu_free_menu_item_bool((ui_menu_item_bool*)item);
        break;
    case MI_INCR:
        ui_menu_free_menu_item_incr((ui_menu_item_incr*)item);
        break;
    default:
        break;
    }
}

// 0x008CB820 (:1992)
// Original: always returns false.
bool ui_menu::remove_item(int handle)
{
    const int index = get_menu_item_index(handle);
    if (index == -1)
    {
        return false;
    }
    ui_menu_item_base* item = m_items[index];
    m_items[index] = nullptr;
    free_item(item);
    for (unsigned int i = (unsigned int)index + 1; i < m_num_items; i++)
    {
        m_items[i - 1] = m_items[i];
    }
    m_num_items--;
    return false;
}

// 0x008CB8B0 (:2036)
// The items go back to their pools (first to last) and the menu leaves its document.
bool ui_menu::remove_all_items()
{
    while (m_num_items != 0)
    {
        remove_item(m_items[0]->get_handle());
    }
    m_doc_info = nullptr;
    return true;
}

// Original: the item limits are the pools' sizes (32 function, 48 increment items), not the menu's 16 slots, so a
// 17th function or increment item writes past m_items.
static const unsigned int Max_function_items = 32;
static const unsigned int Max_incr_items = 48;
static const unsigned int Max_list_items = 16;

// 0x008B84A0 (:2047)
int ui_menu::add_function_item(const char* caption, void (*scb)(int), bool tween_out, bool surpress_accept_sound, bool set_active_controller)
{
    if (m_num_items + 1 >= Max_function_items)
    {
        return -1;
    }
    ui_menu_item_func* item = ui_menu_get_new_menu_item_func();
    if (item == nullptr)
    {
        return -1;
    }
    item->ui_menu_item_func_init(caption, this, scb);
    item->set_should_tween_out(tween_out);
    item->set_surpress_accept_sound(surpress_accept_sound);
    item->set_should_set_active_controller(set_active_controller);
    m_items[m_num_items++] = item;
    return item->get_handle();
}

// An increment item of the pool, set up.
static ui_menu_item_incr* new_incr_item(ui_menu* menu, menu_incr_type type, const char* caption, int* variable, int min, int max, int incr, void (*iccb)(int, void*))
{
    ui_menu_item_incr* item = ui_menu_get_new_menu_item_incr();
    if (item == nullptr)
    {
        return nullptr;
    }
    // The inlined init without its defaults: the range is set right after.
    item->ui_menu_item_incr_init(MIT_INVALID, caption, menu, variable, iccb);
    variable_type vmin, vmax, vincr;
    vmin.m_int = min;
    vmax.m_int = max;
    vincr.m_int = incr;
    *(menu_incr_type*)((unsigned char*)item + 0xA0) = type; // m_incr_type (protected)
    item->set_info(vmin, vmax, vincr);
    return item;
}

// 0x008B8510 (:2068)
int ui_menu::add_int_item(const char* caption, int* variable, int min, int max, int incr, void (*iccb)(int, void*))
{
    if (m_num_items + 1 >= Max_incr_items)
    {
        return -1;
    }
    ui_menu_item_incr* item = new_incr_item(this, MIT_INT, caption, variable, min, max, incr, iccb);
    if (item == nullptr)
    {
        return -1;
    }
    m_items[m_num_items++] = item;
    return item->get_handle();
}

// 0x008B85D0 (:2091)
int ui_menu::add_exp_item(const char* caption, int* variable, int min, int max, int exp, void (*iccb)(int, void*))
{
    if (m_num_items + 1 >= Max_incr_items || exp <= 0)
    {
        return -1;
    }
    ui_menu_item_incr* item = new_incr_item(this, MIT_EXPONENTIAL, caption, variable, min, max, exp, iccb);
    if (item == nullptr)
    {
        return -1;
    }
    m_items[m_num_items++] = item;
    return item->get_handle();
}

// 0x008B86A0 (:2114)
int ui_menu::add_slider_item(const char* caption, int* variable, int min, int max, int incr, void (*iccb)(int, void*))
{
    if (m_num_items + 1 >= Max_incr_items)
    {
        return -1;
    }
    ui_menu_item_incr* item = new_incr_item(this, MIT_SLIDER, caption, variable, min, max, incr, iccb);
    if (item == nullptr)
    {
        return -1;
    }
    m_items[m_num_items++] = item;
    return item->get_handle();
}

// 0x008B8760 (:2137)
int ui_menu::add_bool_item(const char* caption, bool* variable, void (*iccb)(int, void*))
{
    if (m_num_items + 1 >= Max_list_items)
    {
        return -1;
    }
    ui_menu_item_bool* item = ui_menu_get_new_menu_item_bool();
    if (item == nullptr)
    {
        return -1;
    }
    item->ui_menu_item_bool_init(caption, this, variable, iccb);
    m_items[m_num_items++] = item;
    return item->get_handle();
}

// 0x008B87C0 (:2156)
int ui_menu::add_char_list_item(const char* caption, const char** list, int num_chars, void (*iccb)(int, void*), int selected_idx)
{
    if (m_num_items + 1 >= Max_list_items)
    {
        return -1;
    }
    ui_menu_item_char_list* item = ui_menu_get_new_menu_item_char_list();
    if (item == nullptr)
    {
        return -1;
    }
    item->ui_menu_item_char_list_init(caption, this, list, num_chars, iccb, selected_idx);
    m_items[m_num_items++] = item;
    return item->get_handle();
}

// 0x008A1590 (:2174)
void ui_menu::set_doc_info(document_info* dinfo)
{
    m_doc_info = dinfo;
}

// 0x008837C0 (.h:355)
document_info* ui_menu::get_doc_info() const
{
    return m_doc_info;
}

// 0x008837D0 (.h:357)
bool ui_menu::get_awaiting_on_select_end() const
{
    return m_awaiting_tween_on_select_end;
}

// 0x008837E0 (.h:358)
bool ui_menu::get_awaiting_fade_in_end() const
{
    return m_awaiting_tween_fade_in_end;
}

// 0x008837F0 (.h:360)
int ui_menu::get_selected_item() const
{
    return m_selected;
}

// 0x00883800 (.h:368)
void ui_menu::force_data_item_update()
{
    m_force_update = true;
}

// 0x008A15A0 (:2181)
// The end of the select tween runs the selected function item.
void ui_menu::set_awaiting_on_select_end(bool value)
{
    if (m_awaiting_tween_on_select_end == value)
    {
        return;
    }
    if (!value)
    {
        run_selected_function(get_menu_item_via_index(m_selected));
        m_force_update = true;
    }
    m_awaiting_tween_on_select_end = value;
}

// 0x008A15E0 (:2206)
void ui_menu::set_awaiting_fade_in_end(bool value)
{
    m_awaiting_tween_fade_in_end = value;
}

// 0x008A15F0 (:2212)
void ui_menu::set_should_tween_out(bool value)
{
    m_should_tween_out = value;
}

// 0x008A1600 (:2218)
void ui_menu::set_surpress_accept_sounds(bool value)
{
    m_surpress_accept_sounds = value;
}

// 0x008A1610 (:2224)
void ui_menu::set_choose_on_enter_index(int index)
{
    m_choose_on_enter = index;
}

// The button of the menu's last hint of a type (BC_NUM_CHOICES without one, or for an action without a button).
static button_choices hint_button(const farray<hint_info, 4>& hints, hint_type type)
{
    button_choices button = BC_NUM_CHOICES;
    for (int i = 0; i < hints.size(); i++)
    {
        if (hints[i].type == type)
        {
            button = BC_NUM_CHOICES;
            button_from_action(hints[i].button_action, button);
        }
    }
    return button;
}

// 0x008B8820 (:2230)
button_choices ui_menu::get_select_button()
{
    return hint_button(m_hint_info, HT_SELECTION);
}

// 0x008B8970 (:2244)
button_choices ui_menu::get_abort_button()
{
    return hint_button(m_hint_info, HT_ABORT);
}

// 0x008B8AC0 (:2258)
button_choices ui_menu::get_misc_button()
{
    return hint_button(m_hint_info, HT_MISC);
}

// 0x008B8C10 (:2272)
button_choices ui_menu::get_misc_2_button()
{
    return hint_button(m_hint_info, HT_MISC_2);
}

// 0x008B8D60 (:2286)
// The last hint of the type goes.
void ui_menu::remove_hint(hint_type ht)
{
    for (int i = m_hint_info.size() - 1; i > -1; i--)
    {
        if (m_hint_info[i].type == ht)
        {
            m_hint_info.remove(i);
            return;
        }
    }
}

// 0x008CBA50 (:2299)
// A hint replaces the menu's hint of its type (at most 4; it needs a button image).
bool ui_menu::add_hint(hint_type ht, control_button_action action, const char* hint_text, const char* image_name)
{
    if (image_name == nullptr || m_hint_info.size() >= 4)
    {
        return false;
    }
    // Original: the index of the new hint is read before the old hint of the type is removed, so a replaced hint
    // that was not the last one leaves the new hint's info written one slot past it (the added hint keeps its
    // empty info).
    const int index = m_hint_info.size();
    remove_hint(ht);
    hint_info hint;
    const bool added = m_hint_info.add(hint) != -1;
    m_hint_info.elements()[index].set_info(ht, action, hint_text, image_name);
    return added;
}

// 0x008A1620 (:2322)
// A selected item that is hidden or disabled hands the selection to the next item that is neither.
void ui_menu::adjust_selected_item()
{
    if (m_selected < 0 || m_selected >= (int)m_num_items)
    {
        return;
    }
    ui_menu_item_base* item = m_items[m_selected];
    if (item == nullptr || (item->get_visible() && item->get_enabled()))
    {
        return;
    }
    for (unsigned int i = 0; i < m_num_items; i++)
    {
        const int next = (int)((i + 1 + (unsigned int)m_selected) % m_num_items);
        ui_menu_item_base* candidate = get_menu_item_via_index(next);
        if (candidate->get_enabled() && candidate->get_visible())
        {
            m_selected = next;
            return;
        }
    }
}

// 0x008A1690 (:2345)
void ui_menu::adjust_selected_item(bool move_up)
{
    int index = m_selected;
    for (unsigned int i = 0; i < m_num_items; i++)
    {
        if (move_up)
        {
            index = (index - 1) % (int)m_num_items;
            if (index < 0)
            {
                index = (int)m_num_items - 1;
            }
        }
        else
        {
            index = (index + 1) % (int)m_num_items;
        }
        ui_menu_item_base* candidate = get_menu_item_via_index(index);
        if (candidate->get_enabled() && candidate->get_visible())
        {
            m_selected = index;
            return;
        }
    }
}

// 0x008A16F0 (:2373)
void ui_menu::process_mouseover(int index)
{
    if (index > -1 && index < (int)m_num_items)
    {
        m_selected = index;
    }
    if (m_selection_change_cb != nullptr)
    {
        m_selection_change_cb(m_selected);
    }
}

// 0x008DC4C0 (:2384)
// A click on the selected item: a function item starts its select tween, the others change with the arrows.
void ui_menu::process_mouse_click(int index, bool leftArrow, bool rightArrow)
{
    ui_menu_item_base* item = m_items[index];
    if (item == nullptr || !item->get_enabled() || !item->get_visible() || index != m_selected)
    {
        return;
    }
    if (item->get_menu_item_type() == MI_FUNCTION)
    {
        if (leftArrow || rightArrow)
        {
            return;
        }
        m_awaiting_tween_on_select_end = true;
        audiolib_cue_play(Audio_info[MENU_SOUND_PRESS_SELECT], nullptr, nullptr);
        // ui_character_trigger's options case, inlined (the main menu case is not).
        gameseq_get_state();
        if (gameseq_get_state() != GS_MAINMENU_IDLE && Doc_handle_main != -1 && gameseq_get_state() == GS_MAINMENU_OPTIONS)
        {
            Play_action = ANIMATION_ACTION_MELEE2;
            Fx_delay_timer.set(Fx_delay);
        }
        return;
    }
    if (!leftArrow && !rightArrow)
    {
        return;
    }
    const increment_type step = leftArrow ? IT_DOWN : IT_UP;
    bool changed = false;
    switch (item->get_menu_item_type())
    {
    case MI_CHAR_LIST:
        changed = ((ui_menu_item_char_list*)item)->handle_input(step);
        break;
    case MI_BOOL:
        changed = ((ui_menu_item_bool*)item)->handle_input(step);
        break;
    case MI_INCR:
        changed = ((ui_menu_item_incr*)item)->handle_input(step);
        break;
    default:
        break;
    }
    if (!item->get_play_change_sound())
    {
        return;
    }
    if (!changed)
    {
        audiolib_cue_play(Audio_info[MENU_SOUND_INVALID_INPUT], nullptr, nullptr);
        return;
    }
    const menu_item_type type = item->get_menu_item_type();
    if (type != MI_BOOL && (type != MI_CHAR_LIST || ((ui_menu_item_char_list*)item)->get_num_chars() != 2))
    {
        // Original: the slider test reads the item's 0xA0 field for every type (a char list's selection callback).
        if (*(int*)((unsigned char*)item + 0xA0) != MIT_SLIDER)
        {
            audiolib_cue_play(Audio_info[leftArrow ? MENU_SOUND_SCROLL_LEFT : MENU_SOUND_SCROLL_RIGHT], nullptr, nullptr);
            return;
        }
        if (leftArrow)
        {
            audiolib_cue_play(Audio_info[MENU_SOUND_SLIDER_SUBTRACT], nullptr, nullptr);
            return;
        }
    }
    audiolib_cue_play(Audio_info[MENU_SOUND_SLIDER_ADD], nullptr, nullptr);
}

// 0x008A1720 (:2463)
// The index of the visible item at a position among the visible items.
int ui_menu::get_menu_item_index_by_location(int index)
{
    for (unsigned int i = 0; i < m_num_items; i++)
    {
        if (m_items[i]->get_visible())
        {
            index--;
            if (index < 0)
            {
                return (int)i;
            }
        }
    }
    return -1;
}

// ---- the menu system

// 0x008CBB20 (:2480)
// The pools: 8 menus and document infos, 48 increment, 16 char list, 16 bool and 32 function items.
void ui_menu_init()
{
    for (int i = 0; i < 8; i++)
    {
        free_vector_add(Alloc_info.menu_free_vector, &Menu_list[i]);
        free_vector_add(Alloc_info.menu_document_info_free_vector, &Doc_info[i]);
    }
    for (int i = 0; i < 48; i++)
    {
        free_vector_add(Alloc_info.menu_item_incr_free_vector, &Menu_item_incr_list[i]);
    }
    for (int i = 0; i < 16; i++)
    {
        free_vector_add(Alloc_info.menu_item_char_list_free_vector, &Menu_item_char_list[i]);
    }
    for (int i = 0; i < 16; i++)
    {
        free_vector_add(Alloc_info.menu_item_bool_free_vector, &Menu_item_bool_list[i]);
    }
    for (int i = 0; i < 32; i++)
    {
        free_vector_add(Alloc_info.menu_item_func_free_vector, &Menu_item_func_list[i]);
    }
}

// The menu leaves the stack: its items go back, it goes back to its pool and its document's info with it when it was
// the document's last menu. Original: the other menus' document infos are read without a null check.
static void destroy_menu(int stack_index)
{
    ui_menu* menu = Menus[stack_index];
    document_info* info = menu->get_doc_info();
    int count = -1;
    if (info != nullptr)
    {
        count = ui_menu_get_document_info_count(info->doc_handle);
    }
    menu->remove_all_items();
    ui_menu_free_menu(menu);
    if (count == 1)
    {
        ui_menu_free_document_info(info);
    }
    Menus.remove(stack_index);
}

// 0x008DC640 (:2503)
// Every menu leaves (the first menu's handle each time), then the pools are emptied.
void ui_menu_shutdown()
{
    while (Menus.size() > 0)
    {
        const int first = Menus[0]->get_handle();
        int index = -1;
        for (int i = 0; i < Menus.size(); i++)
        {
            if (Menus[i]->get_handle() == first)
            {
                index = i;
                break;
            }
        }
        destroy_menu(index);
        if (Menus.size() == 0)
        {
            ui_wait_for_button_ups();
        }
    }
    Alloc_info.menu_free_vector.remove_all();
    Alloc_info.menu_item_func_free_vector.remove_all();
    Alloc_info.menu_item_char_list_free_vector.remove_all();
    Alloc_info.menu_item_bool_free_vector.remove_all();
    Alloc_info.menu_item_incr_free_vector.remove_all();
    Alloc_info.menu_document_info_free_vector.remove_all();
}

// 0x008CBD10 (:2521)
// A menu on a document (at most 7 at once, it needs a title): the menus of a document share its document info and
// data items, made with the first one.
int ui_menu_add(int doc_handle, const char* title, void (*change_cb)(int), void (*back_cb)(hint_type), const char* event_surtag, bool fade_in, int choose_on_enter_index, int input_id, bool surpress_accept_sounds)
{
    ui_menu_init_audio_info();
    // Original: the stack holds 8 menus, the add stops at 7.
    if (title == nullptr || Menus.size() + 1 > 7)
    {
        return -1;
    }
    ui_wait_for_button_ups();
    ui_menu* menu = ui_menu_get_new_menu();
    if (menu == nullptr)
    {
        return -1;
    }
    Menus.add(menu);
    menu->set_surpress_accept_sounds(surpress_accept_sounds);
    menu->init(doc_handle, title, change_cb, back_cb, input_id);
    menu->set_choose_on_enter_index(choose_on_enter_index);
    menu->set_awaiting_fade_in_end(fade_in);
    document_info* info = ui_menu_find_document_info(doc_handle);
    if (info == nullptr)
    {
        info = ui_menu_get_new_document_info();
        if (info != nullptr)
        {
            info->doc_handle = doc_handle;
            if (event_surtag != nullptr)
            {
                keen::copyString(info->event_surtag, sizeof(info->event_surtag), event_surtag);
            }
            ui_menu_init_data_items(info);
        }
    }
    menu->set_doc_info(info);
    return menu->get_handle();
}

// 0x008DC7C0 (:2583)
// From the top of the stack down, the top menu of each document is processed (its data items and its input).
void ui_menu_process(float)
{
    ui_menu_check_cleanup_of_data(false);
    farray<int, 8> done;
    for (int i = Menus.size() - 1; i > -1; i--)
    {
        ui_menu* menu = Menus[i];
        if (menu == nullptr || menu->get_doc_info() == nullptr)
        {
            continue;
        }
        const int doc_handle = menu->get_doc_info()->doc_handle;
        if (done.contains(doc_handle) != 0)
        {
            continue;
        }
        if (done.add(doc_handle) == -1)
        {
            keen::debug::nativePrintArgs("Int3 critical function at %s(%d)\n", __FILE__, __LINE__);
        }
        ui_menu_check_add_data_items(menu);
        menu->process(0.0f, false);
    }
    // Magic number: the steps count modulo 128.
    Process_step = (Process_step + 1) % 128;
}

// 0x008CBE00 (:2620)
bool ui_menu_destroy(int menu_handle)
{
    bool destroyed = false;
    for (int i = 0; i < Menus.size(); i++)
    {
        if (Menus[i]->get_handle() == menu_handle)
        {
            destroy_menu(i);
            destroyed = true;
            break;
        }
    }
    if (Menus.size() == 0)
    {
        ui_wait_for_button_ups();
    }
    return destroyed;
}

// 0x008B8DE0 (:2666)
bool ui_menu_get_tween_on_select_end(int menu_handle)
{
    ui_menu* menu = find_menu(menu_handle);
    return menu != nullptr && menu->get_awaiting_on_select_end();
}

// 0x008B8E30 (:2677)
// Only starts the wait (rfg.exe sets the member directly, without running the selection).
void ui_menu_set_tween_on_select_end(int menu_handle)
{
    ui_menu* menu = find_menu(menu_handle);
    if (menu != nullptr && !menu->get_awaiting_on_select_end())
    {
        *(bool*)((unsigned char*)menu + 0x06) = true; // m_awaiting_tween_on_select_end (private)
    }
}

// 0x008CBF20 (:2686)
// The document's select tween ended (its Lua calls back): the top menu on it runs its selected function item.
// Original: the menus' document infos are read without a null check.
void ui_menu_tween_on_select_end(int vint_doc_handle)
{
    const int doc_handle = ui_get_doc_handle_from_vint_doc_handle(vint_doc_handle);
    for (int i = Menus.size() - 1; i > -1; i--)
    {
        ui_menu* menu = Menus[i];
        if (menu == nullptr || menu->get_doc_info()->doc_handle != doc_handle)
        {
            continue;
        }
        menu->set_awaiting_on_select_end(false);
        menu->force_data_item_update();
        return;
    }
}

// 0x008CBF90 (:2717)
void ui_menu_tween_fade_in_end(int vint_doc_handle)
{
    const int doc_handle = ui_get_doc_handle_from_vint_doc_handle(vint_doc_handle);
    for (int i = Menus.size() - 1; i > -1; i--)
    {
        ui_menu* menu = Menus[i];
        if (menu != nullptr && menu->get_doc_info() != nullptr && menu->get_doc_info()->doc_handle == doc_handle)
        {
            menu->set_awaiting_fade_in_end(false);
            return;
        }
    }
}

// 0x008B8E70 (:2754)
void ui_menu_enable_menu_item(int menu_handle, int index, bool enabled)
{
    ui_menu* menu = find_menu(menu_handle);
    if (menu == nullptr)
    {
        return;
    }
    const int handle = menu->get_menu_item_handle(index);
    if (handle != -1)
    {
        menu->enable_menu_item(handle, enabled);
    }
}

// 0x008B8ED0 (:2766)
bool ui_menu_is_menu_item_enabled(int menu_handle, int index)
{
    ui_menu* menu = find_menu(menu_handle);
    if (menu == nullptr)
    {
        return false;
    }
    const int handle = menu->get_menu_item_handle(index);
    return handle != -1 && menu->is_menu_item_enabled(handle);
}

// 0x008B8F30 (:2780)
void ui_menu_show_menu_item(int menu_handle, int index, bool visible)
{
    ui_menu* menu = find_menu(menu_handle);
    if (menu == nullptr)
    {
        return;
    }
    const int handle = menu->get_menu_item_handle(index);
    if (handle != -1)
    {
        menu->show_menu_item(handle, visible);
    }
}

// 0x008B8F90 (:2792)
bool ui_menu_is_menu_item_visible(int menu_handle, int index)
{
    ui_menu* menu = find_menu(menu_handle);
    if (menu == nullptr)
    {
        return false;
    }
    const int handle = menu->get_menu_item_handle(index);
    return handle != -1 && menu->is_menu_item_visible(handle);
}

// 0x008B8FF0 (:2806)
int ui_menu_add_function_item(int menu_handle, const char* caption, void (*scbz)(int), bool tween_out, bool surpress_accept_sound, bool set_active_controller)
{
    ui_menu* menu = find_menu(menu_handle);
    if (menu == nullptr)
    {
        return -1;
    }
    return menu->add_function_item(caption, scbz, tween_out, surpress_accept_sound, set_active_controller);
}

// The new item's change sound.
static int set_play_change_sound(ui_menu* menu, int handle, bool play_change_sound)
{
    if (handle != -1)
    {
        menu->get_menu_item_via_handle(handle)->set_play_change_sound(play_change_sound);
    }
    return handle;
}

// 0x008B9050 (:2818)
int ui_menu_add_int_item(int menu_handle, const char* caption, int* variable, int min, int max, int incr, void (*iccb)(int, void*), bool play_change_sound)
{
    ui_menu* menu = find_menu(menu_handle);
    if (menu == nullptr)
    {
        return -1;
    }
    return set_play_change_sound(menu, menu->add_int_item(caption, variable, min, max, incr, iccb), play_change_sound);
}

// 0x008B90D0 (:2833)
int ui_menu_add_exp_item(int menu_handle, const char* caption, int* variable, int min, int max, int exp, void (*iccb)(int, void*), bool play_change_sound)
{
    ui_menu* menu = find_menu(menu_handle);
    if (menu == nullptr)
    {
        return -1;
    }
    return set_play_change_sound(menu, menu->add_exp_item(caption, variable, min, max, exp, iccb), play_change_sound);
}

// 0x008B9150 (:2848)
int ui_menu_add_slider_item(int menu_handle, const char* caption, int* variable, int min, int max, int incr, void (*iccb)(int, void*), bool play_change_sound)
{
    ui_menu* menu = find_menu(menu_handle);
    if (menu == nullptr)
    {
        return -1;
    }
    return set_play_change_sound(menu, menu->add_slider_item(caption, variable, min, max, incr, iccb), play_change_sound);
}

// 0x008B91D0 (:2863)
int ui_menu_add_bool_item(int menu_handle, const char* caption, bool* variable, void (*iccb)(int, void*), bool play_change_sound)
{
    ui_menu* menu = find_menu(menu_handle);
    if (menu == nullptr)
    {
        return -1;
    }
    return set_play_change_sound(menu, menu->add_bool_item(caption, variable, iccb), play_change_sound);
}

// 0x008B9260 (:2878)
int ui_menu_add_char_list_item(int menu_handle, const char* caption, const char** list, int num_chars, void (*iccb)(int, void*), int selected_idx, bool play_change_sound)
{
    ui_menu* menu = find_menu(menu_handle);
    if (menu == nullptr)
    {
        return -1;
    }
    return set_play_change_sound(menu, menu->add_char_list_item(caption, list, num_chars, iccb, selected_idx), play_change_sound);
}

// A hint of the action's button (its image) on the menu.
static bool add_hint(int menu_handle, hint_type type, const char* hint_text, control_button_action action)
{
    ui_menu* menu = find_menu(menu_handle);
    if (menu == nullptr)
    {
        return false;
    }
    char image[256];
    button_choices button;
    if (!ui_menu_get_button_info(image, button, action))
    {
        return false;
    }
    return menu->add_hint(type, action, hint_text, image);
}

// 0x008CBFE0 (:2893)
bool ui_menu_add_selection_hint(int menu_handle, const char* hint_text, control_button_action action)
{
    return add_hint(menu_handle, HT_SELECTION, hint_text, action);
}

// 0x008B92E0 (:2907)
void ui_menu_remove_selection_hint(int menu_handle)
{
    if (ui_menu* menu = find_menu(menu_handle))
    {
        menu->remove_hint(HT_SELECTION);
    }
}

// 0x008B9330 (:2915)
void ui_menu_remove_all_hints(int menu_handle)
{
    ui_menu* menu = find_menu(menu_handle);
    if (menu == nullptr)
    {
        return;
    }
    for (int type = HT_SELECTION; type < HT_LAST; type++)
    {
        menu->remove_hint((hint_type)type);
    }
}

// 0x008CC080 (:2926)
bool ui_menu_add_abort_hint(int menu_handle, const char* hint_text, control_button_action action)
{
    return add_hint(menu_handle, HT_ABORT, hint_text, action);
}

// 0x008B9380 (:2940)
void ui_menu_remove_abort_hint(int menu_handle)
{
    if (ui_menu* menu = find_menu(menu_handle))
    {
        menu->remove_hint(HT_ABORT);
    }
}

// 0x008CC120 (:2949)
bool ui_menu_add_misc_hint(int menu_handle, const char* hint_text, control_button_action action)
{
    return add_hint(menu_handle, HT_MISC, hint_text, action);
}

// 0x008CC1C0 (:2964)
bool ui_menu_add_misc_hint_2(int menu_handle, const char* hint_text, control_button_action action)
{
    return add_hint(menu_handle, HT_MISC_2, hint_text, action);
}

// 0x008B93D0 (:2979)
// Original: always returns false.
bool ui_menu_set_char_list_index(int menu_handle, int index, int select_idx)
{
    ui_menu* menu = find_menu(menu_handle);
    if (menu == nullptr)
    {
        return false;
    }
    ui_menu_item_base* item = menu->get_menu_item_via_index(index);
    if (item != nullptr && item->get_menu_item_type() == MI_CHAR_LIST && select_idx > -1)
    {
        ui_menu_item_char_list* list = (ui_menu_item_char_list*)item;
        if (select_idx < list->get_num_chars())
        {
            list->set_selected_char(select_idx);
        }
    }
    return false;
}

// 0x008B9440 (:2994)
int ui_menu_get_selected_item(int menu_handle)
{
    ui_menu* menu = find_menu(menu_handle);
    return menu != nullptr ? menu->get_selected_item() : -1;
}

// 0x008A1750 (:3004)
void ui_menu_set_input_enabled(bool enabled)
{
    Input_enabled_uimen = enabled;
    if (!enabled)
    {
        ui_wait_for_button_ups();
    }
}

// 0x008A1790 (:3014)
void ui_menu_play_sound(menu_sound_type sound_type)
{
    if ((unsigned int)sound_type < (unsigned int)NUM_MENU_SOUNDS)
    {
        audiolib_cue_play(Audio_info[sound_type], nullptr, nullptr);
    }
}

// 0x008B9490 (:3021)
void ui_menu_set_selected_item(int menu_handle, int sel_idx)
{
    if (ui_menu* menu = find_menu(menu_handle))
    {
        menu->select_menu_item_by_index(sel_idx);
    }
}

// 0x008B94E0 (:3030)
int ui_menu_get_visible_item_count(int menu_handle)
{
    ui_menu* menu = find_menu(menu_handle);
    return menu != nullptr ? menu->get_num_visible_items() : 0;
}

// 0x008B9540 (:3038)
int ui_menu_get_total_item_count(int menu_handle)
{
    ui_menu* menu = find_menu(menu_handle);
    return menu != nullptr ? menu->get_num_items() : 0;
}

// 0x008B9580 (:3045)
void ui_menu_select_item_by_index(int menu_handle, int index)
{
    if (ui_menu* menu = find_menu(menu_handle))
    {
        menu->process_mouseover(index);
    }
}

// 0x008DC930 (:3054)
void ui_menu_selection_clicked(int menu_handle, int index, bool leftArrow, bool rightArrow)
{
    if (ui_menu* menu = find_menu(menu_handle))
    {
        menu->process_mouse_click(index, leftArrow, rightArrow);
    }
}

// 0x008B95E0 (:3063)
int ui_menu_get_index_by_location(int menu_handle, int index)
{
    ui_menu* menu = find_menu(menu_handle);
    return menu != nullptr ? menu->get_menu_item_index_by_location(index) : -1;
}

// 0x008B9640 (:3074)
bool ui_menu_is_fading_in(int menu_handle)
{
    ui_menu* menu = find_menu(menu_handle);
    return menu != nullptr && menu->get_awaiting_fade_in_end();
}

// 0x008B9690 (:3084)
document_info* ui_menu_get_doc_info(int menu_handle)
{
    ui_menu* menu = find_menu(menu_handle);
    return menu != nullptr ? menu->get_doc_info() : nullptr;
}
