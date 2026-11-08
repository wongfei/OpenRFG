// Original: volition/rfg/code/interface/ui_document_save_load.cpp
//
// The save and load game screen (rfg_ui_load_game), which is also the screen that imports saves of the original release.
// A row list of up to nine saves over the selection's summary and thumbnail, driven by keyboard, pad and mouse.
//
// Original quirks kept:
// - The two hint "visible" sets that hide an object pass a null string, not a false value (the literal false picked the
//   string overload): see hud_ui_save_load_proccess_hints.
// - hud_ui_save_load_get_list re-sorts while a write requested by this screen may still point into the list.
// - hud_ui_save_load_set_selected only reports a bad index when it is too large or negative, and repairs it.

#include "volition/rfg/code/interface/ui_document_save_load.h"

#include "keen/base/crc32.hpp"
#include "keen/base/string.hpp"
#include "keen/base/trace.hpp"
#include "volition/audio_lib/code/audiolib_api.h"
#include "volition/interface_lib/code/objects/vint_object_base.h"
#include "volition/interface_lib/code/vint_data_group.h"
#include "volition/interface_lib/code/vint_data_item.h"
#include "volition/interface_lib/code/vint_external.h"
#include "volition/rfg/code/control/control_map.h"
#include "volition/rfg/code/gameplay/object_district.h"
#include "volition/rfg/code/gamewide/downloadable_content.h"
#include "volition/rfg/code/gamewide/game_save.h"
#include "volition/rfg/code/gamewide/gameaudio.h"
#include "volition/rfg/code/gamewide/player_profile.h"
#include "volition/rfg/code/gamewide/verify_gamesave.h"
#include "volition/rfg/code/interface/loading.h"
#include "volition/rfg/code/interface/localization/localization_shared.h"
#include "volition/rfg/code/interface/main_menu.h"
#include "volition/rfg/code/interface/option_menus/ingame_options.h"
#include "volition/rfg/code/interface/option_menus/player_death_options.h"
#include "volition/rfg/code/interface/ui_document_manager.h"
#include "volition/rfg/code/interface/ui_menu.h"
#include "volition/rfg/code/level/territory_shared.h"
#include "volition/rfg/code/level/world.h"
#include "volition/rfg/code/multi/multi_core/include/multi_core_platform.h"
#include "volition/rfg/code/rfg.h"
#include "volition/stream2/code/stream2.h"
#include "volition/vlib/code/input/mouse.h"
#include "volition/vlib/code/memory/page_pool.h"
#include "volition/vlib/code/memory/virtual_mempool.h"
#include "volition/vlib/code/util/string_util.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// ---- file-private types (rfg.pdb)
enum msg_box_type
{
    INVALID_MBT = -1,
    MBT_SAVE_CONFIRM = 0,
    MBT_LOAD_CONFIRM = 1,
    MBT_CONNECT_DEVICE = 2,
    MBT_DELETE_CONFIRM = 3,
    NUM_MBT = 4,
};

enum save_load_screen_transition
{
    SLST_NONE = 0,
    SLST_IMPORT_SELECT = 1,
    SLST_IMPORT_WRITE = 2,
};

// ---- file-static globals
// bool s_localization_is_loading (0x01663023) is used here too; placed in localization_shared.h.
// int Doc_handle_main (0x016631AC) is used here too; placed in ui_menu.cpp.
// int Choice_last (0x01664B38) is used here too; placed in ui_message_box.cpp.
// int Current_input_lock_id (0x01664B4C) is used here too; placed in loading.cpp.
static const char* cDocname_uisave = {"rfg_ui_load_game"}; // 0x01664DD4
static const char* cTitle_load = {"MENU_LOAD_GAME"}; // 0x01664DD8
static const char* cTitle_save = {"MENU_SAVE_GAME"}; // 0x01664DDC
static const char* cTitle_import = {"STEAM_SAVE_DATA_IMPORT_NAME"}; // 0x01664DE0
static const char* cAuto_save = {"MENU_AUTOSAVE"}; // 0x01664DE4
static const char* cAuto_save_dlc = {"DLC01_AUTOSAVE"}; // 0x01664DE8
static const char* cUser_save = {"MENU_USER_SAVE"}; // 0x01664DEC
static const char* cUser_save_dlc = {"MENU_USER_SAVE_DLC"}; // 0x01664DF0
static const char* cEmpty_slot_uisave = {"MENU_EMPTY_SLOT_DASHES"}; // 0x01664DF4
static const char* cMissions_caption = {"MENU_MISSIONS_COMPLETED"}; // 0x01664DF8
static const char* cActivities_caption = {"MENU_MINI_MISSIONS_COMPLETED"}; // 0x01664DFC
static const char* cMarauder_activitys = {"MENU_DLC_ACTIONS_COMPLETED"}; // 0x01664E00
static const char* cEdf_control_caption = {"MENU_TOTAL_EDF_CONTROL"}; // 0x01664E04
static const char* cBack_button = {"MENU_BACK"}; // 0x01664E08
static const char* cDelete = {"MENU_DELETE"}; // 0x01664E10
static const char* cResetDestruction = {"MENU_RESET_DESTRUCTION"}; // 0x01664E14
static const char* cFilename = {"MENU_FILE_NAME"}; // 0x01664E18
static const char* cDate = {"MENU_DATE"}; // 0x01664E1C
static const char* cTime = {"MENU_TIME"}; // 0x01664E20
static const char* cEmptystring_uisave = {" "}; // 0x01664E24
static const char* cSave_default_thumb = {"ui_thumb_tutorial"}; // 0x01664E28
static const char* cSave_default_thumb_dlc = {"ui_thumb_marinervalley"}; // 0x01664E2C
static int Doc_handle_uisave = -1; // 0x01664E30
static int Data_header_handle_uisave = 16; // 0x01664E34
static int Data_row_group_handle_uisave = 16; // 0x01664E38
static int Data_hints_handle_uisave = 16; // 0x01664E3C
static int Msgbox_handle_uisave = -1; // 0x01664E40
static msg_box_type Msgbox_type; // 0x01664E44, initial data ffffffff
static int Loading_msg_handle = -1; // 0x01664E48
static int Input_id_uisave = -1; // 0x01664E4C
static const char* kSaveLoadHintGroup = {"hint_1_group"}; // 0x01664E50
static const char* kSaveLoadHintGroup2 = {"hint_2_group"}; // 0x01664E54
static const char* kSaveLoadHintGroup3 = {"hint_3_group"}; // 0x01664E58
static const char* kSaveLoadHintGroup4 = {"hint_4_group"}; // 0x01664E5C
static const char* kSaveLoadRowGroup = {"row_group"}; // 0x01664E60
static const char* kSaveLoadRowHighlightBG = {"row_highlight_bg"}; // 0x01664E64
static const char* kSaveLoadRowHighlightMain = {"row_highlight_main"}; // 0x01664E68
static const char* kSaveLoadRowHighlightBtn = {"row_highlight_button"}; // 0x01664E6C
// int Utility_doc_handle (0x01664FF8) is used here too; placed in loading.cpp.
// farray<unsigned int,16> Controller_slots (0x01667378) is used here too; placed in loading.cpp.
// farray<input_request_data,16> Input_types (0x016673C8) is used here too; placed in loading.cpp.
// char cButton_text[128][256] (0x02BE9E30) is used here too; placed in ui_message_box.cpp.
// ui_button_info Button_info (0x02BF1E30) is used here too; placed in ui_message_box.cpp.
// int mp_stats_hint_config[7] (0x02BF214C) is used here too; placed in ui_message_box.cpp.
// int Audio_info[12] (0x02BF21D0) is used here too; placed in ui_menu.cpp.
static int Data_row_handles_uisave[9]; // 0x02C02C54
static bool Exit_uisave; // 0x02C08108
static bool Loaded_uisave; // 0x02C08109
static bool Auto_save_exists; // 0x02C0810A
static bool Dlc_auto_save_exists; // 0x02C0810B
static unsigned long Storage_device_id; // 0x02C0810C
static unsigned int Control_id_uisave; // 0x02C08110
static virtual_mempool* Temp_mempool; // 0x02C08114
static game_save_info* Game_save_list; // 0x02C08118
static int Num_saved_games; // 0x02C0811C
static int Display_start_index_uisave; // 0x02C08120
static int Selected_row_uisave; // 0x02C08124
static bool Update_list_uisave; // 0x02C08128
static bool Save_load_reset_destruction; // 0x02C08129
static bool Wait_for_write; // 0x02C0812A
static bool Load_image_next_frame_uisave; // 0x02C0812B
// save_load_ui_type Save_load_ui_type (0x02C0812C) is used here too; placed in main_menu.cpp.
static const char* Streamed_image_name_uisave; // 0x02C08130
static bool Save_load_waiting_for_device; // 0x02C08135
static unsigned int Save_data_import_selected_slot; // 0x02C08138
static save_load_screen_transition Save_load_screen_transition; // 0x02C0813C
static const unsigned int Default_district = keen::getCrc32LwrValue("MENU_EMPTY_SLOT"); // 0x02C0828C
static const unsigned int No_saved_games = keen::getCrc32LwrValue("MENU_NO_SAVED_GAMES"); // 0x02C0F53C
// msgbox_info_set Message_box_info_set (0x02CA0340) is used here too; placed in ui_message_box.cpp.
// Original: No_device (0x02C3CAF8, "MENU_MBOX_NO_DEVICE_SELECTED") has no reader on PC; left out.
static const char* kScrollBarOutline = "black_scrollfill"; // 0x016685B0
static const char* cImport_profile_row = "STEAM_SAVE_DATA_IMPORT_PROFILE_DATA";
static const char* cImport_confirm = "STEAM_SAVE_DATA_IMPORT_CONFIRM"; // 0x013032F0

// Magic numbers: the screen shows nine rows, and the save system has 12 slots (the import list 257: 256 and the profile).
static const int Num_rows_uisave = 9;
static const int Num_save_slots_uisave = 12;

// Magic numbers: the indices of Audio_info the screen plays.
enum
{
    AUDIO_DOWN = 0,
    AUDIO_UP = 1,
    AUDIO_SELECT = 4,
    AUDIO_BACK = 5,
    AUDIO_DELETE = 6,
    AUDIO_ERROR = 11,
};

static void play_ui_sound(int index)
{
    audiolib_cue_play(Audio_info[index], nullptr, nullptr);
}

// ---- internal functions

// 0x0089D9A0 (:172): the rows of the screen; saving has one more row for a new save unless every slot is taken (an
// autosave's slot, without its row, stays reserved).
static int game_save_get_num_saves()
{
    if (Save_load_ui_type == SLUT_IMPORT_SELECT)
    {
        return (int)Num_imported_save_file_infos;
    }
    if (Num_saved_games >= 0 && (Save_load_ui_type == SLUT_SAVE || Save_load_ui_type == SLUT_IMPORT_WRITE))
    {
        int maximum = Auto_save_exists ? Num_save_slots_uisave : Num_save_slots_uisave - 1;
        if (!Dlc_auto_save_exists)
        {
            maximum--;
        }
        return Num_saved_games + 1 > maximum ? maximum : Num_saved_games + 1;
    }
    return Num_saved_games;
}

// 0x0089D9F0 (:204): saving lists the new row first. The current bonus campaign's saves first, then the autosave first
// when loading and last when saving, then the newest first.
static int game_save_list_compare(const void* arg1, const void* arg2)
{
    const game_save_info* a = (const game_save_info*)arg1;
    const game_save_info* b = (const game_save_info*)arg2;
    if (a == nullptr || b == nullptr)
    {
        return 0;
    }
    if ((Save_load_ui_type == SLUT_SAVE || Save_load_ui_type == SLUT_IMPORT_WRITE || Save_load_ui_type == SLUT_IMPORT_SELECT) && a->used_slot != b->used_slot)
    {
        return a->used_slot ? 1 : -1;
    }
    if (a->dlc_id != b->dlc_id)
    {
        if (a->dlc_id == dlc_get_current_id())
        {
            return -1;
        }
        if (b->dlc_id == dlc_get_current_id())
        {
            return 1;
        }
        return b->dlc_id < a->dlc_id ? 1 : -1;
    }
    if (a->auto_save != b->auto_save)
    {
        const bool loading = Save_load_ui_type == SLUT_LOAD || Save_load_ui_type == SLUT_IMPORT_SELECT;
        return (loading ? a->auto_save : b->auto_save) ? -1 : 1;
    }
    const int fields_a[] = {a->year, a->month, a->day, a->hours, a->minutes, a->seconds};
    const int fields_b[] = {b->year, b->month, b->day, b->hours, b->minutes, b->seconds};
    for (int i = 0; i < 6; i++)
    {
        if (fields_a[i] != fields_b[i])
        {
            return fields_a[i] <= fields_b[i] ? 1 : -1;
        }
    }
    return 0;
}

// 0x0089DAF0 (:289): sorts the infos and their slots together: a quicksort around the middle element's address down to
// 16 rows (magic number), then an insertion sort.
// Original bug kept (original_bugs.md): the scan down from the high end moves the slot pointer up (0x0089DBB0), so above
// 16 rows the slots no longer follow their infos and the pointer can leave the 257 entries of
// Imported_save_file_info_slots (see its definition).
void qsort_imported_save_data(game_save_info* infos, unsigned int* slots, unsigned int count)
{
    while (count > 16)
    {
        game_save_info* const first = infos;
        unsigned int* const first_slot = slots;
        const unsigned int total = count;
        const game_save_info* const pivot = infos + (count >> 1);
        game_save_info* high = infos + (count - 1);
        unsigned int* high_slot = slots + (count - 1);
        while (infos <= high)
        {
            while (game_save_list_compare(infos, pivot) < 0)
            {
                slots++;
                infos++;
            }
            while (game_save_list_compare(high, pivot) > 0)
            {
                high_slot++;
                high--;
            }
            if (infos > high)
            {
                break;
            }
            const game_save_info swapped_info = *infos;
            *infos = *high;
            *high = swapped_info;
            const unsigned int swapped_slot = *slots;
            *slots = *high_slot;
            *high_slot = swapped_slot;
            infos++;
            high--;
            slots++;
            high_slot--;
        }
        qsort_imported_save_data(first, first_slot, (unsigned int)((int)(high - first) + 1));
        count = total - (unsigned int)(infos - first);
    }
    for (unsigned int i = 1; i < count; i++)
    {
        const game_save_info info = infos[i];
        const unsigned int slot = slots[i];
        unsigned int j = i;
        while (j > 0 && game_save_list_compare(&infos[j - 1], &info) > 0)
        {
            infos[j] = infos[j - 1];
            slots[j] = slots[j - 1];
            j--;
        }
        infos[j] = info;
        slots[j] = slot;
    }
}

// 0x0089DD80 (:347)
void hud_ui_save_load_clear_list()
{
    if (Game_save_list != nullptr)
    {
        memset(Game_save_list, 0, game_save_get_list_buf_size(Num_save_slots_uisave));
    }
    Num_saved_games = 0;
}

// 0x0089DDB0 (:357)
void hud_ui_save_load_get_list()
{
    const int size = game_save_get_list_buf_size(Num_save_slots_uisave);
    if (Game_save_list == nullptr)
    {
        if (!Temp_mempool->can_alloc(size, 0))
        {
            gameseq_pop_state();
            return;
        }
        Game_save_list = (game_save_info*)Temp_mempool->alloc(size, 0);
    }
    if (Game_save_list != nullptr)
    {
        memset(Game_save_list, 0, size);
    }
    Num_saved_games = 0;
    if (Save_load_ui_type == SLUT_IMPORT_SELECT)
    {
        qsort_imported_save_data(Imported_save_file_infos, Imported_save_file_info_slots, Num_imported_save_file_infos);
        return;
    }
    Num_saved_games = game_save_get_list(Game_save_list, size, Num_save_slots_uisave, Save_load_ui_type != SLUT_LOAD);
    Auto_save_exists = false;
    Dlc_auto_save_exists = false;
    for (int i = 0; i < Num_saved_games; i++)
    {
        if (Game_save_list[i].auto_save)
        {
            if (Game_save_list[i].dlc_id == -1)
            {
                Auto_save_exists = true;
            }
            else
            {
                Dlc_auto_save_exists = true;
            }
        }
    }
    const int rows = game_save_get_num_saves();
    if (rows > 1)
    {
        qsort(Game_save_list, rows, sizeof(game_save_info), game_save_list_compare);
    }
}

// 0x008C8FD0 (:413)
const char* hud_ui_save_load_get_district_name(unsigned char zone_col, unsigned char zone_row)
{
    const int count = World.get_num_objects<district>();
    for (int i = 0; i < count; i++)
    {
        district* d = World.get_object<district>(i);
        if (d->encompasses_gid(zone_col, zone_row))
        {
            return d->get_name();
        }
    }
    return nullptr;
}

// 0x008B6770 (:427)
bool hud_ui_save_load_no_space_cb(int, int, msgbox_choice choice)
{
    if (choice == MBC_OK)
    {
        play_ui_sound(AUDIO_SELECT);
        verify_gamesave_start(Game_save_list + Selected_row_uisave + Display_start_index_uisave, Save_load_reset_destruction);
    }
    ui_wait_for_button_ups();
    Msgbox_type = INVALID_MBT;
    Msgbox_handle_uisave = -1;
    return true;
}

// 0x0089DF10 (:453)
bool hud_ui_save_load_load_cb(int mbox_handle, int, msgbox_choice choice)
{
    if (Msgbox_handle_uisave != mbox_handle)
    {
        return false;
    }
    if (choice == MBC_OK)
    {
        Msgbox_handle_uisave = -1;
        verify_gamesave_start(Game_save_list + Selected_row_uisave + Display_start_index_uisave, Save_load_reset_destruction);
    }
    ui_wait_for_button_ups();
    return true;
}

// 0x0089DF80 (:470)
bool hud_ui_save_load_save_cb(int mbox_handle, int, msgbox_choice choice)
{
    if (Msgbox_handle_uisave != mbox_handle)
    {
        return false;
    }
    if (choice == MBC_OK)
    {
        bool write = true;
        if (Save_load_ui_type == SLUT_SAVE)
        {
            World.save_and_continue(Game_save_list + Selected_row_uisave + Display_start_index_uisave, false, false);
        }
        else if (Save_load_ui_type == SLUT_IMPORT_SELECT)
        {
            game_save_import_profile_data();
        }
        else if (Save_load_ui_type == SLUT_IMPORT_WRITE)
        {
            game_save_import_save_slot(Game_save_list + Selected_row_uisave + Display_start_index_uisave, Save_data_import_selected_slot);
            Exit_uisave = true;
        }
        else
        {
            write = false;
        }
        if (write)
        {
            Wait_for_write = true;
        }
    }
    ui_wait_for_button_ups();
    Msgbox_type = INVALID_MBT;
    Msgbox_handle_uisave = -1;
    return true;
}

// 0x0089E040 (:502)
bool hud_ui_save_load_delete_cb(int mbox_handle, int, msgbox_choice choice)
{
    if (Msgbox_handle_uisave != mbox_handle)
    {
        return false;
    }
    if (choice == MBC_OK)
    {
        game_save_delete(Game_save_list + Display_start_index_uisave + Selected_row_uisave);
        Wait_for_write = true;
        Update_list_uisave = true;
        if (Display_start_index_uisave > 0)
        {
            Display_start_index_uisave--;
        }
        else if (Selected_row_uisave > 0 && Selected_row_uisave >= game_save_get_num_saves() - 1)
        {
            Selected_row_uisave--;
        }
    }
    else if ((int)choice < 1 || (int)choice > 2)
    {
        keen::debug::nativePrintArgs("Int3 critical function at %s(%d)\n", __FILE__, __LINE__);
    }
    ui_wait_for_button_ups();
    Msgbox_type = INVALID_MBT;
    return true;
}

// 0x0089E100 (:541)
void hud_ui_save_load_set_selected(int new_index, int)
{
    // Magic numbers: the last row of each list.
    const int last = Save_load_ui_type == SLUT_IMPORT_SELECT ? 0xFF : 0xB;
    if (new_index > last)
    {
        keen::debug::nativePrintArgs("Int3 critical function at %s(%d)\n", __FILE__, __LINE__);
        Display_start_index_uisave = last + 1 - Num_rows_uisave;
        Selected_row_uisave = new_index - Display_start_index_uisave;
        return;
    }
    if (new_index < 0)
    {
        keen::debug::nativePrintArgs("Int3 critical function at %s(%d)\n", __FILE__, __LINE__);
        Display_start_index_uisave = 0;
        Selected_row_uisave = 0;
    }
}

// The confirmation before overwriting a save or importing (inlined at each use): box_type's box, or for a mouse click the
// variant that has no select hint.
static void show_overwrite_box(msgbox_type box_type)
{
    Msgbox_handle_uisave = ui_add_msgbox(box_type, "MENU_MBOX_OVERWRITE_DATA", "MENU_OVERWRITE_DATA", hud_ui_save_load_save_cb, false, false, nullptr, nullptr, false);
    Msgbox_type = MBT_SAVE_CONFIRM;
    play_ui_sound(AUDIO_SELECT);
}

// 0x009076E0 (:570)
void hud_ui_save_load_selection_made(bool reset_destruction)
{
    const int index = Selected_row_uisave + Display_start_index_uisave;
    Save_load_reset_destruction = reset_destruction;
    if (Save_load_ui_type == SLUT_LOAD)
    {
        if (Num_saved_games > 0)
        {
            if (gameseq_get_previous_state() == GS_INGAME_OPTIONS)
            {
                Msgbox_handle_uisave = ui_add_msgbox(MBT_ACCEPT_CANCEL, "MENU_LOAD_GAME", "MENU_CONFIRM_LOAD_GAME", hud_ui_save_load_load_cb, false, false, nullptr, nullptr, false);
                Msgbox_type = MBT_LOAD_CONFIRM;
                play_ui_sound(AUDIO_SELECT);
                return;
            }
            verify_gamesave_start(Game_save_list + index, Save_load_reset_destruction);
            Exit_uisave = true;
            play_ui_sound(AUDIO_SELECT);
            return;
        }
    }
    else if (Save_load_ui_type == SLUT_SAVE)
    {
        game_save_info* save_slot = Game_save_list + index;
        if (!save_slot->auto_save)
        {
            if (save_slot->used_slot)
            {
                show_overwrite_box(MBT_ACCEPT_CANCEL);
                return;
            }
            World.save_and_continue(save_slot, false, false);
            Wait_for_write = true;
            play_ui_sound(AUDIO_SELECT);
            return;
        }
    }
    else if (Save_load_ui_type == SLUT_IMPORT_SELECT)
    {
        if (Num_imported_save_file_infos == 0)
        {
            play_ui_sound(AUDIO_ERROR);
            return;
        }
        if (!Imported_save_file_infos[index].used_slot)
        {
            show_overwrite_box(MBT_ACCEPT_CANCEL);
            return;
        }
        Save_data_import_selected_slot = Imported_save_file_info_slots[index];
        Save_load_screen_transition = SLST_IMPORT_WRITE;
        Exit_uisave = true;
        play_ui_sound(AUDIO_SELECT);
        return;
    }
    else if (Save_load_ui_type == SLUT_IMPORT_WRITE)
    {
        if (!Game_save_list[index].auto_save)
        {
            if (Game_save_list[index].used_slot)
            {
                show_overwrite_box(MBT_ACCEPT_CANCEL);
                return;
            }
            game_save_import_save_slot(Game_save_list + index, Save_data_import_selected_slot);
            Wait_for_write = true;
            Exit_uisave = true;
            play_ui_sound(AUDIO_SELECT);
            return;
        }
    }
    else
    {
        return;
    }
    play_ui_sound(AUDIO_ERROR);
}

// 0x008C9040 (:659): the wheel and the pad's scroll stick move the list by a row; holding the mouse on the bar jumps to
// where it is.
void hud_ui_save_load_process_scroll_bar()
{
    if (!mouse_just_pressed(0) && !mouse_just_released(0) && !mouse_is_pressed(0) && !mouse_just_scrolled_wheel() && controls_get_analog(CAA_SCROLL_BAR, Player_controller_index) == 0.0f)
    {
        return;
    }
    const int rows = game_save_get_num_saves();
    if (rows <= Num_rows_uisave)
    {
        return;
    }
    const int last_start = rows - Num_rows_uisave;
    if (Mouse_dwheel > 0 || controls_get_analog(CAA_SCROLL_BAR, Player_controller_index) > 0.0f)
    {
        if (Display_start_index_uisave > 0)
        {
            play_ui_sound(AUDIO_UP);
            Display_start_index_uisave--;
            return;
        }
    }
    else if ((Mouse_dwheel < 0 || controls_get_analog(CAA_SCROLL_BAR, Player_controller_index) < 0.0f) && Display_start_index_uisave < last_start)
    {
        play_ui_sound(AUDIO_DOWN);
        Display_start_index_uisave++;
        return;
    }
    vint_doc_info* info = ui_get_doc_info(Doc_handle_uisave);
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
    // Magic number: rounds to the nearest row (rfg.exe: floor of the value plus a half, in double).
    const int start = (int)floor((double)(((Mouse_scaled_y - y) / height) * (float)last_start) + 0.5);
    if (start != Display_start_index_uisave)
    {
        play_ui_sound(AUDIO_DOWN);
        Display_start_index_uisave = start;
    }
}

// Whether this list offers to reset the destruction: loading, with the campaign of the current bonus campaign completed.
static bool hud_ui_save_load_can_reset_destruction()
{
    if (Save_load_ui_type != SLUT_LOAD)
    {
        return false;
    }
    const int dlc_id = dlc_get_current_id();
    const player_profile_data* profile = player_profile_get("save_load_ui", Player_controller_index != 0xFFFFFFFF ? Player_controller_index : 0);
    return profile != nullptr && ((dlc_id < 0 && profile->game_completed) || (dlc_id == 1 && profile->new_data.game_completed_dlc));
}

// The selected row's summary, null without saves.
static game_save_info* hud_ui_save_load_get_selected_info()
{
    if (Save_load_ui_type == SLUT_IMPORT_SELECT)
    {
        return Num_imported_save_file_infos != 0 ? Imported_save_file_infos + Display_start_index_uisave + Selected_row_uisave : nullptr;
    }
    return Num_saved_games > 0 ? Game_save_list + Display_start_index_uisave + Selected_row_uisave : nullptr;
}

// 0x009079B0 (:727)
void hud_ui_save_load_process_mouse(int num_items, int current_index)
{
    hud_ui_save_load_process_scroll_bar();
    const char* hints[4] = {kSaveLoadHintGroup, kSaveLoadHintGroup2, kSaveLoadHintGroup3, kSaveLoadHintGroup4};
    const int hint = ui_mouse_process_hints(hints, 4, Doc_handle_uisave, true);
    if (hint >= 0)
    {
        const game_save_info* selected = hud_ui_save_load_get_selected_info();
        const bool can_delete = selected != nullptr && selected->used_slot && !selected->auto_save && (Save_load_ui_type == SLUT_LOAD || Save_load_ui_type == SLUT_SAVE);
        // The hints in use follow each other: back, delete when it is there, select, reset the destruction.
        const int delete_hint = can_delete ? 1 : -1;
        const int select_hint = can_delete ? 2 : 1;
        const int reset_hint = hud_ui_save_load_can_reset_destruction() ? select_hint + 1 : -1;
        if (hint == 0)
        {
            play_ui_sound(AUDIO_BACK);
            Exit_uisave = true;
            return;
        }
        if (hint == delete_hint)
        {
            if (Num_saved_games > 0 && Game_save_list[Display_start_index_uisave + Selected_row_uisave].used_slot)
            {
                Msgbox_handle_uisave = ui_add_msgbox(MBT_ACCEPT_CANCEL, "MENU_MBOX_DELETE", "MENU_DELETE_SAVE_GAME", hud_ui_save_load_delete_cb, false, false, nullptr, nullptr, false);
                Msgbox_type = MBT_DELETE_CONFIRM;
                play_ui_sound(AUDIO_DELETE);
            }
        }
        else if (hint == select_hint || hint == reset_hint)
        {
            hud_ui_save_load_selection_made(hint == reset_hint);
        }
    }
    vint_doc_info* info = ui_get_doc_info(Doc_handle_uisave);
    if (info == nullptr || info->m_doc == nullptr || (!mouse_just_moved() && !mouse_just_released(0)))
    {
        return;
    }
    vint_document* document = info->m_doc;
    vint_object_base* row_group = vint_object_base::find_object(kSaveLoadRowGroup, document);
    vint_object_base* highlight_bg = vint_object_base::find_object(kSaveLoadRowHighlightBG, document);
    vint_object_base* highlight_main = vint_object_base::find_object(kSaveLoadRowHighlightMain, document);
    if (row_group == nullptr || highlight_bg == nullptr || highlight_main == nullptr)
    {
        return;
    }
    float left = 0.0f;
    float top = 0.0f;
    float width = 0.0f;
    float row_height = 0.0f;
    float unused = 0.0f;
    ui_get_vint_obj_position(row_group, unused, top);
    ui_get_vint_obj_size(highlight_bg, width, unused);
    ui_get_vint_obj_size(highlight_main, unused, row_height);
    ui_get_vint_obj_position(row_group, left, unused);
    // Magic number: the row highlight's background is half of a row's width.
    if (left <= Mouse_scaled_x && Mouse_scaled_x < left + width * 2.0f && top <= Mouse_scaled_y && Mouse_scaled_y <= row_height * (float)Num_rows_uisave + top)
    {
        const int row = (int)((Mouse_scaled_y - top) / row_height);
        const int index = row + Display_start_index_uisave;
        if (index >= 0 && index < num_items && row < Num_rows_uisave)
        {
            if (index != current_index)
            {
                Selected_row_uisave = row;
                hud_ui_save_load_set_selected(index, current_index);
            }
            if (mouse_just_released(0))
            {
                hud_ui_save_load_selection_made(false);
            }
        }
    }
}

// Load the selected save, asking first in the pause menu (inlined at the select and reset destruction buttons).
static void hud_ui_save_load_confirm_load(bool reset_destruction)
{
    Save_load_reset_destruction = reset_destruction;
    if (gameseq_get_previous_state() == GS_INGAME_OPTIONS)
    {
        Msgbox_handle_uisave = ui_add_msgbox(MBT_ACCEPT_CANCEL_SELECT, "MENU_LOAD_GAME", "MENU_CONFIRM_LOAD_GAME", hud_ui_save_load_load_cb, false, false, nullptr, nullptr, false);
        Msgbox_type = MBT_LOAD_CONFIRM;
    }
    else
    {
        hud_ui_save_load_load_cb(Msgbox_handle_uisave, 1, MBC_OK);
    }
    play_ui_sound(AUDIO_SELECT);
}

// The select button (the keyboard's and pad's route through hud_ui_save_load_selection_made's choices).
static void hud_ui_save_load_process_select(int current_index)
{
    switch (Save_load_ui_type)
    {
    case SLUT_LOAD:
        if (Num_saved_games > 0)
        {
            hud_ui_save_load_confirm_load(false);
            return;
        }
        break;
    case SLUT_SAVE:
    {
        game_save_info* save_slot = Game_save_list + current_index;
        if (!save_slot->auto_save)
        {
            if (save_slot->used_slot)
            {
                show_overwrite_box(MBT_ACCEPT_CANCEL_SELECT);
            }
            else
            {
                World.save_and_continue(save_slot, false, false);
                Wait_for_write = true;
                play_ui_sound(AUDIO_SELECT);
            }
            return;
        }
        break;
    }
    case SLUT_IMPORT_SELECT:
        if (Num_imported_save_file_infos == 0)
        {
            break;
        }
        if (!Imported_save_file_infos[current_index].used_slot)
        {
            show_overwrite_box(MBT_ACCEPT_CANCEL);
        }
        else
        {
            Save_data_import_selected_slot = Imported_save_file_info_slots[current_index];
            Save_load_screen_transition = SLST_IMPORT_WRITE;
            Exit_uisave = true;
            play_ui_sound(AUDIO_SELECT);
        }
        return;
    case SLUT_IMPORT_WRITE:
        if (!Game_save_list[current_index].auto_save)
        {
            if (Game_save_list[current_index].used_slot)
            {
                show_overwrite_box(MBT_ACCEPT_CANCEL_SELECT);
            }
            else
            {
                game_save_import_save_slot(Game_save_list + Selected_row_uisave + Display_start_index_uisave, Save_data_import_selected_slot);
                Wait_for_write = true;
                Exit_uisave = true;
                play_ui_sound(AUDIO_SELECT);
            }
            return;
        }
        break;
    default:
        return;
    }
    play_ui_sound(AUDIO_ERROR);
}

// 0x00907D80 (:883)
void hud_ui_save_load_process_input()
{
    // Nothing while a message box is up (the box pool is not all free).
    if (Utility_doc_handle != -1 && ui_is_msgbox_active(-2))
    {
        return;
    }
    const int current_index = Display_start_index_uisave + Selected_row_uisave;
    const int num_items = game_save_get_num_saves();
    hud_ui_save_load_process_mouse(num_items, current_index);
    const bool can_reset = hud_ui_save_load_can_reset_destruction();
    // An invitation leaves the screen as the back button does.
    if (ui_get_button_info(BC_ABORT, Input_id_uisave) > 0.0f || multi_core_platform_has_pending_invite())
    {
        play_ui_sound(AUDIO_BACK);
        Exit_uisave = true;
    }
    else if (ui_get_button_info(BC_SELECT, Input_id_uisave) > 0.0f)
    {
        hud_ui_save_load_process_select(current_index);
    }
    else if ((Save_load_ui_type == SLUT_LOAD || Save_load_ui_type == SLUT_SAVE) && ui_get_button_info(BC_X, Input_id_uisave) > 0.0f)
    {
        if (Num_saved_games > 0)
        {
            if (Game_save_list[current_index].auto_save)
            {
                play_ui_sound(AUDIO_ERROR);
            }
            else if (Game_save_list[current_index].used_slot)
            {
                Msgbox_handle_uisave = ui_add_msgbox(MBT_ACCEPT_CANCEL_SELECT, "MENU_MBOX_DELETE", "MENU_DELETE_SAVE_GAME", hud_ui_save_load_delete_cb, false, false, nullptr, nullptr, false);
                Msgbox_type = MBT_DELETE_CONFIRM;
                play_ui_sound(AUDIO_DELETE);
            }
        }
    }
    else if (can_reset && ui_get_button_info(BC_Y, Input_id_uisave) > 0.0f)
    {
        if (Num_saved_games > 0)
        {
            hud_ui_save_load_confirm_load(true);
        }
        else
        {
            play_ui_sound(AUDIO_ERROR);
        }
    }
    else if (ui_get_button_info(BC_UP, Input_id_uisave) > 0.0f)
    {
        if (Selected_row_uisave > 0)
        {
            play_ui_sound(AUDIO_UP);
            Selected_row_uisave--;
        }
        else if (Display_start_index_uisave > 0)
        {
            play_ui_sound(AUDIO_UP);
            Display_start_index_uisave--;
        }
        else if (num_items > 0)
        {
            // Wraps to the last row.
            if (num_items <= Num_rows_uisave)
            {
                Selected_row_uisave = num_items - 1;
            }
            else
            {
                Display_start_index_uisave = num_items - Num_rows_uisave;
                Selected_row_uisave = Num_rows_uisave - 1;
            }
        }
    }
    else if (ui_get_button_info(BC_DOWN, Input_id_uisave) > 0.0f)
    {
        if (current_index < num_items - 1)
        {
            play_ui_sound(AUDIO_DOWN);
            if (Selected_row_uisave + 1 < Num_rows_uisave)
            {
                Selected_row_uisave++;
            }
            else
            {
                Display_start_index_uisave++;
            }
        }
        else if (num_items > 0)
        {
            // Wraps to the first row.
            Selected_row_uisave = 0;
            if (num_items > Num_rows_uisave)
            {
                Display_start_index_uisave = 0;
            }
        }
    }
    hud_ui_save_load_set_selected(Selected_row_uisave + Display_start_index_uisave, current_index);
}

// 0x008C9300 (:1090)
void hud_ui_save_load_create_header_data()
{
    vint_data_item* item = vint_data_item::create_new("HeaderData");
    if (item != nullptr)
    {
        const vint_variant_type types[15] = {VINT_PROP_TYPE_STRING, VINT_PROP_TYPE_STRING, VINT_PROP_TYPE_STRING, VINT_PROP_TYPE_STRING, VINT_PROP_TYPE_STRING, VINT_PROP_TYPE_STRING, VINT_PROP_TYPE_STRING, VINT_PROP_TYPE_UINT,
                                             VINT_PROP_TYPE_INT, VINT_PROP_TYPE_INT, VINT_PROP_TYPE_INT, VINT_PROP_TYPE_STRING, VINT_PROP_TYPE_BOOL, VINT_PROP_TYPE_FLOAT, VINT_PROP_TYPE_BOOL};
        item->set_template(types, 15);
        Data_header_handle_uisave = item->get_handle();
        ui_add_data_item(Doc_handle_uisave, Data_header_handle_uisave);
    }
}

// 0x008C93B0 (:1123)
void hud_ui_save_load_create_row_data()
{
    vint_data_group* group = vint_data_group::create_new("RowData");
    if (group != nullptr)
    {
        Data_row_group_handle_uisave = group->get_handle();
        ui_add_data_group(Doc_handle_uisave, Data_row_group_handle_uisave);
    }
}

// 0x008C93E0 (:1136)
void hud_ui_save_load_create_hints_data()
{
    vint_data_item* item = vint_data_item::create_new("HintData");
    if (item != nullptr)
    {
        const vint_variant_type types[8] = {VINT_PROP_TYPE_STRING, VINT_PROP_TYPE_STRING, VINT_PROP_TYPE_STRING, VINT_PROP_TYPE_STRING, VINT_PROP_TYPE_STRING, VINT_PROP_TYPE_STRING, VINT_PROP_TYPE_STRING, VINT_PROP_TYPE_STRING};
        item->set_template(types, 8);
        Data_hints_handle_uisave = item->get_handle();
        ui_add_data_item(Doc_handle_uisave, Data_hints_handle_uisave);
    }
}

// 0x008C9470 (:1162)
void hud_ui_save_load_create_data_items()
{
    hud_ui_save_load_create_header_data();
    hud_ui_save_load_create_row_data();
    hud_ui_save_load_create_hints_data();
}

// 0x008DBA90 (:1169): the title, captions and the selected row's summary
void hud_ui_save_load_process_header()
{
    vint_data_item* item = vint_data_item::find(Data_header_handle_uisave);
    const int index = Selected_row_uisave + Display_start_index_uisave;
    if (item == nullptr)
    {
        return;
    }
    const int rows = game_save_get_num_saves();
    if (Save_load_ui_type == SLUT_LOAD)
    {
        item->set_element(0, vint_variant(cTitle_load));
    }
    else if (Save_load_ui_type == SLUT_SAVE || Save_load_ui_type == SLUT_IMPORT_WRITE)
    {
        item->set_element(0, vint_variant(cTitle_save));
    }
    else if (Save_load_ui_type == SLUT_IMPORT_SELECT)
    {
        item->set_element(0, vint_variant(cTitle_import));
    }
    game_save_info* list = Save_load_ui_type == SLUT_IMPORT_SELECT ? Imported_save_file_infos : Game_save_list;
    if (list == nullptr)
    {
        return;
    }
    const game_save_info* info = list + index;
    item->set_element(1, vint_variant(cMissions_caption));
    item->set_element(2, vint_variant(info->dlc_id == 1 ? cMarauder_activitys : cActivities_caption));
    item->set_element(3, vint_variant(cEdf_control_caption));
    item->set_element(4, vint_variant(cFilename));
    item->set_element(5, vint_variant(cDate));
    item->set_element(6, vint_variant(cTime));
    const t_district* save_district = territory_shared_find_district(info->territory_index, info->district_index);
    const char* thumbnail = save_district != nullptr ? save_district->thumbnail : (info->dlc_id == -1 ? cSave_default_thumb : cSave_default_thumb_dlc);
    bool show_thumbnail = info->used_slot && thumbnail != nullptr && !Exit_uisave;
    if (show_thumbnail)
    {
        const unsigned int srid = stream2_container_find(thumbnail);
        show_thumbnail = srid != 0 && stream2_get_status(srid) == ST_LOADED;
    }
    if (!show_thumbnail)
    {
        thumbnail = cEmptystring_uisave;
    }
    if (info->used_slot)
    {
        unsigned int name;
        if (save_district != nullptr)
        {
            name = save_district->localized_name_hash;
        }
        else
        {
            name = localization_get_id_hash(info->dlc_id == -1 ? cUser_save : cUser_save_dlc);
        }
        item->set_element(7, vint_variant(name));
        item->set_element(8, vint_variant(info->num_missions_completed));
        item->set_element(9, vint_variant(info->num_activities_completed));
        item->set_element(10, vint_variant(info->districts_liberated));
    }
    else
    {
        unsigned int name = No_saved_games;
        if (Save_load_ui_type == SLUT_IMPORT_SELECT)
        {
            if (Num_imported_save_file_infos != 0)
            {
                name = keen::getCrc32LwrValue(cImport_profile_row);
            }
        }
        else if (Num_saved_games > 0)
        {
            name = Default_district;
        }
        item->set_element(7, vint_variant(name));
        item->set_element(8, vint_variant(0));
        item->set_element(9, vint_variant(0));
        item->set_element(10, vint_variant(0));
    }
    item->set_element(11, vint_variant(thumbnail));
    // Magic number: the bar shows when the list does not fit. Original: its position divides by zero at exactly nine rows,
    // which the bar then does not show.
    item->set_element(12, vint_variant(rows > Num_rows_uisave));
    item->set_element(13, vint_variant((float)Display_start_index_uisave / (float)(rows - Num_rows_uisave)));
    item->set_element(14, vint_variant(info->dlc_id != 1));
}

// A localized row name, or null (inlined in hud_ui_save_load_process_rows).
static const wchar_t* hud_ui_save_load_find_text(unsigned int hash)
{
    wchar_t* text = nullptr;
    if (hash != 0 && !s_localization_is_loading)
    {
        Localization_hash.get(hash, text);
    }
    return text;
}

// 0x008EEF10 (:1264): the rows on screen
void hud_ui_save_load_process_rows()
{
    vint_data_group* group = vint_data_group::find(Data_row_group_handle_uisave);
    if (group == nullptr)
    {
        return;
    }
    int wanted = game_save_get_num_saves();
    wanted = wanted > Num_rows_uisave ? Num_rows_uisave : (wanted < 0 ? 0 : wanted);
    int have = (int)group->get_num_data_items();
    for (int i = have - 1; i >= wanted; i--)
    {
        group->remove_data_item(Data_row_handles_uisave[i]);
    }
    for (int i = have; i < wanted; i++)
    {
        vint_data_item* row = group->add_data_item(nullptr);
        if (row != nullptr)
        {
            const vint_variant_type types[7] = {VINT_PROP_TYPE_INT, VINT_PROP_TYPE_STRING, VINT_PROP_TYPE_STRING, VINT_PROP_TYPE_STRING, VINT_PROP_TYPE_BOOL, VINT_PROP_TYPE_BOOL, VINT_PROP_TYPE_BOOL};
            row->set_template(types, 7);
            Data_row_handles_uisave[i] = row->get_handle();
        }
    }
    const unsigned int count = group->get_num_data_items();
    for (unsigned int i = 0; i < count; i++)
    {
        vint_data_item* row = vint_data_item::find(Data_row_handles_uisave[i]);
        if (row == nullptr)
        {
            continue;
        }
        const int index = Display_start_index_uisave + (int)i;
        const game_save_info* info = (Save_load_ui_type == SLUT_IMPORT_SELECT ? Imported_save_file_infos : Game_save_list) + index;
        const char* name;
        char encoded_name[300];
        char date_text[16] = {};
        char time_text[16] = {};
        if (info->used_slot)
        {
            const t_district* save_district = territory_shared_find_district(info->territory_index, info->district_index);
            const wchar_t* text;
            if (info->auto_save)
            {
                text = hud_ui_save_load_find_text(keen::getCrc32LwrValue(info->dlc_id == -1 ? cAuto_save : cAuto_save_dlc));
            }
            else if (save_district == nullptr)
            {
                text = hud_ui_save_load_find_text(keen::getCrc32LwrValue(info->dlc_id == -1 ? cUser_save : cUser_save_dlc));
            }
            else
            {
                text = hud_ui_save_load_find_text(save_district->localized_name_hash);
                if (text == nullptr)
                {
                    text = L"!!Unrecognized hash value";
                }
            }
            // Original: a name that is not found goes in as null.
            vint_encode_wide_text(encoded_name, sizeof(encoded_name), text);
            name = encoded_name;
            // The year is stored as two digits.
            switch (Current_date_format)
            {
            case DATE_FORMAT_MM_DD_YYYY:
                sprintf(date_text, "%.2i.%.2i.%.4i", info->month, info->day, info->year + 2000);
                break;
            case DATE_FORMAT_DD_MM_YYYY:
                sprintf(date_text, "%.2i.%.2i.%.4i", info->day, info->month, info->year + 2000);
                break;
            case DATE_FORMAT_NONE:
            case DATE_FORMAT_YYYY_MM_DD:
                sprintf(date_text, "%.4i.%.2i.%.2i", info->year + 2000, info->month, info->day);
                break;
            default:
                break;
            }
            sprintf(time_text, "%.2i:%.2i", info->hours, info->minutes);
        }
        else
        {
            name = Save_load_ui_type == SLUT_IMPORT_SELECT ? cImport_profile_row : cEmpty_slot_uisave;
        }
        bool enabled;
        if (Save_load_ui_type == SLUT_LOAD)
        {
            enabled = index < Num_saved_games;
        }
        else if (Save_load_ui_type == SLUT_IMPORT_SELECT)
        {
            enabled = index < (int)Num_imported_save_file_infos;
        }
        else
        {
            enabled = !info->auto_save;
        }
        row->set_element(0, vint_variant((int)i + 1));
        row->set_element(1, vint_variant(name));
        row->set_element(2, vint_variant((const char*)date_text));
        row->set_element(3, vint_variant((const char*)time_text));
        row->set_element(4, vint_variant(Selected_row_uisave == (int)i));
        row->set_element(5, vint_variant(enabled));
        // Rows alternate their background.
        row->set_element(6, vint_variant(index % 2 == 0));
    }
}

// 0x008C94B0 (:1420): the hints: back, delete, the row's action, reset the destruction
void hud_ui_save_load_proccess_hints()
{
    vint_data_item* item = vint_data_item::find(Data_hints_handle_uisave);
    if (item == nullptr)
    {
        return;
    }
    char image[64];
    char hint[128];
    controls_get_button_image_name(image, sizeof(image), CBA_MENU_ABORT, Player_controller_index);
    keen::formatString(hint, sizeof(hint), "[image:%s]", image);
    item->set_element(0, vint_variant((const char*)hint));
    item->set_element(1, vint_variant(cBack_button));
    const game_save_info* selected = hud_ui_save_load_get_selected_info();
    if (selected != nullptr && selected->used_slot && !selected->auto_save && (Save_load_ui_type == SLUT_LOAD || Save_load_ui_type == SLUT_SAVE))
    {
        controls_get_button_image_name(image, sizeof(image), CBA_MENU_X, Player_controller_index);
        keen::formatString(hint, sizeof(hint), "[image:%s]", image);
        item->set_element(2, vint_variant((const char*)hint));
        item->set_element(3, vint_variant(cDelete));
    }
    else
    {
        item->set_element(2, vint_variant(cEmptystring_uisave));
        item->set_element(3, vint_variant(cEmptystring_uisave));
    }
    vint_doc_info* info = ui_get_doc_info(Doc_handle_uisave);
    if (info == nullptr || info->m_doc == nullptr)
    {
        return;
    }
    vint_document* document = info->m_doc;
    controls_get_button_image_name(image, sizeof(image), CBA_MENU_SELECT, Player_controller_index);
    keen::formatString(hint, sizeof(hint), "[image:%s]", image);
    // Magic number: the visible property; a null string is rfg.exe's way of hiding (the file comment).
    const unsigned int visible = crc_str("visible", 0);
    if (Save_load_ui_type == SLUT_LOAD)
    {
        item->set_element(4, vint_variant((const char*)hint));
        item->set_element(5, vint_variant(cTitle_load));
    }
    else if (Save_load_ui_type == SLUT_IMPORT_SELECT)
    {
        item->set_element(4, vint_variant((const char*)hint));
        item->set_element(5, vint_variant(cImport_confirm));
    }
    else if (selected == nullptr || selected->auto_save)
    {
        vint_object_base* group = vint_object_base::find_object("hint_2_group", document);
        if (group != nullptr)
        {
            group->set_property_value(visible, (const char*)nullptr, 0);
        }
        item->set_element(4, vint_variant(cEmptystring_uisave));
        item->set_element(5, vint_variant(cEmptystring_uisave));
    }
    else
    {
        vint_object_base* group = vint_object_base::find_object("hint_2_group", document);
        if (group != nullptr)
        {
            group->set_property_value(visible, vint_variant(true), 0);
        }
        item->set_element(4, vint_variant((const char*)hint));
        item->set_element(5, vint_variant(cTitle_save));
    }
    if (hud_ui_save_load_can_reset_destruction())
    {
        controls_get_button_image_name(image, sizeof(image), CBA_MENU_Y, Player_controller_index);
        keen::formatString(hint, sizeof(hint), "[image:%s]", image);
        item->set_element(6, vint_variant((const char*)hint));
        item->set_element(7, vint_variant(cResetDestruction));
    }
    else
    {
        item->set_element(6, vint_variant(cEmptystring_uisave));
        item->set_element(7, vint_variant(cEmptystring_uisave));
    }
    vint_object_base* highlight_button = vint_object_base::find_object(kSaveLoadRowHighlightBtn, document);
    if (highlight_button != nullptr)
    {
        highlight_button->set_property_value(visible, (const char*)nullptr, 0);
    }
}

// 0x008EF410 (:1530)
void hud_ui_save_load_process_info()
{
    hud_ui_save_load_process_header();
    hud_ui_save_load_process_rows();
    hud_ui_save_load_proccess_hints();
}

// 0x00917070 (:1544)
void hud_ui_save_load_init()
{
    state_info info;
    info.enter = hud_ui_save_load_enter;
    info.exit = hud_ui_save_load_exit;
    info.process = hud_ui_save_load_process;
    info.handshake = hud_ui_save_load_handshake;
    info.render = hud_ui_save_load_render;
    info.transparent = true;
    info.pause_beneath = true;
    gameseq_register_state(GS_SAVE_LOAD_SCREEN, info);
}

// 0x00908390 (:1559)
void hud_ui_save_load_enter(game_state prev_state)
{
    Save_load_screen_transition = SLST_NONE;
    Num_saved_games = 0;
    Exit_uisave = false;
    Loaded_uisave = false;
    Update_list_uisave = true;
    Display_start_index_uisave = 0;
    Selected_row_uisave = 0;
    Streamed_image_name_uisave = nullptr;
    Storage_device_id = 0;
    Wait_for_write = false;
    Doc_handle_uisave = ui_load_document(cDocname_uisave, DFIT_NONE, 0.0f, DLT_NORMAL, false, STREAM_PRIORITY_AUDIO_CACHE_LOAD, false);
    if (Doc_handle_uisave == -1)
    {
        keen::debug::nativePrintArgs("Int3 critical function at %s(%d)\n", __FILE__, __LINE__);
        gameseq_pop_state();
        return;
    }
    Input_id_uisave = ui_register_for_processed_input(IRT_UNLIMITED_INPUT, false);
    Control_id_uisave = ui_request_control_id();
    ui_wait_for_button_ups();
    if (gameseq_get_state() == GS_MAINMENU_IDLE || main_menu_get_doc_handle() == -1)
    {
        if (prev_state == GS_PLAYER_DEATH_OPTIONS)
        {
            player_death_options_show_menu(false);
        }
        else
        {
            ingame_options_show_menu(false);
        }
    }
    else
    {
        main_menu_show_menu(false);
    }
    Temp_mempool = Common_page_pool.get_virtual_mempool("load_save pool", game_save_get_list_buf_size(Num_save_slots_uisave));
}

// 0x008EF420 (:1611)
void hud_ui_save_load_exit(game_state new_state)
{
    if (Streamed_image_name_uisave != nullptr)
    {
        const unsigned int srid = stream2_container_find(Streamed_image_name_uisave);
        if (srid != 0 && stream2_get_status(srid) != ST_UNLOADED)
        {
            stream2_release(srid);
        }
        Streamed_image_name_uisave = nullptr;
    }
    ui_unload_document(Doc_handle_uisave, DFOT_NONE, 0.5f);
    for (int i = 0; i < Controller_slots.size(); i++)
    {
        if (Controller_slots[i] == Control_id_uisave)
        {
            Controller_slots.remove(i);
            break;
        }
    }
    ui_relinquish_input_id(Input_id_uisave);
    hud_ui_save_load_clear_list();
    Game_save_list = nullptr;
    Common_page_pool.release_virtual_mempool(Temp_mempool);
    if (gameseq_get_state() == GS_MAINMENU_IDLE || main_menu_get_doc_handle() == -1)
    {
        if (new_state == GS_PLAYER_DEATH_OPTIONS)
        {
            player_death_options_show_menu(true);
        }
        else
        {
            ingame_options_show_menu(true);
        }
    }
    else
    {
        main_menu_show_menu(true);
    }
    Save_load_waiting_for_device = false;
    // Importing goes from the list of saves to the choice of a slot and back.
    if (Save_load_ui_type == SLUT_IMPORT_WRITE)
    {
        Save_load_screen_transition = SLST_IMPORT_SELECT;
        Save_load_ui_type = SLUT_IMPORT_SELECT;
        gameseq_push_state(GS_SAVE_LOAD_SCREEN, true, false);
    }
    else if (Save_load_screen_transition == SLST_IMPORT_SELECT)
    {
        Save_load_ui_type = SLUT_IMPORT_SELECT;
        gameseq_push_state(GS_SAVE_LOAD_SCREEN, true, false);
    }
    else if (Save_load_screen_transition == SLST_IMPORT_WRITE)
    {
        Save_load_ui_type = SLUT_IMPORT_WRITE;
        gameseq_push_state(GS_SAVE_LOAD_SCREEN, true, false);
    }
}

// 0x0089E1B0 (:1657): the selected save's thumbnail streams in a frame after it changed
void hud_ui_save_load_handshake(rl_renderer*)
{
    const game_save_info* selected = hud_ui_save_load_get_selected_info();
    if (selected == nullptr)
    {
        if (Streamed_image_name_uisave != nullptr)
        {
            const unsigned int srid = stream2_container_find(Streamed_image_name_uisave);
            if (srid != 0 && stream2_get_status(srid) != ST_UNLOADED)
            {
                stream2_release(srid);
            }
            Streamed_image_name_uisave = nullptr;
        }
        Load_image_next_frame_uisave = false;
        return;
    }
    const t_district* save_district = territory_shared_find_district(selected->territory_index, selected->district_index);
    const char* name = save_district != nullptr ? save_district->thumbnail : (selected->dlc_id == -1 ? cSave_default_thumb : cSave_default_thumb_dlc);
    if (Load_image_next_frame_uisave)
    {
        if (name != nullptr)
        {
            const unsigned int srid = stream2_container_find(name);
            if (srid != 0)
            {
                stream2_request(srid);
            }
        }
        Load_image_next_frame_uisave = false;
        Streamed_image_name_uisave = name;
    }
    if (Wait_for_write || Streamed_image_name_uisave == name)
    {
        return;
    }
    if (Streamed_image_name_uisave != nullptr)
    {
        if (name != nullptr && v_stricmp(name, Streamed_image_name_uisave) == 0)
        {
            return;
        }
        ui_release_image(Streamed_image_name_uisave);
        Streamed_image_name_uisave = nullptr;
    }
    if (name != nullptr)
    {
        Load_image_next_frame_uisave = true;
    }
}

// 0x00912370 (:1717)
void hud_ui_save_load_process()
{
    if (Exit_uisave)
    {
        if (gameseq_get_state() == GS_SAVE_LOAD_SCREEN && !gameseq_in_transition())
        {
            gameseq_pop_state();
        }
        Exit_uisave = false;
        ui_process(Control_id_uisave, false);
        return;
    }
    if (!Loaded_uisave)
    {
        switch (ui_get_doc_asset_state(Doc_handle_uisave))
        {
        case ST_UNLOADED:
            keen::debug::nativePrintArgs("Int3 critical function at %s(%d)\n", __FILE__, __LINE__);
            // falls through, as in rfg.exe
        case ST_ERROR:
            ui_unload_document(Doc_handle_uisave, DFOT_NONE, 0.5f);
            Doc_handle_uisave = -1;
            gameseq_pop_state();
            break;
        case ST_LOADING:
            break;
        case ST_LOADED:
            Loaded_uisave = true;
            hud_ui_save_load_create_data_items();
            break;
        default:
            keen::debug::nativePrintArgs("Int3 critical function at %s(%d)\n", __FILE__, __LINE__);
            break;
        }
    }
    if (Loaded_uisave)
    {
        if (Wait_for_write && game_save_get_state() == GSS_NONE)
        {
            Update_list_uisave = true;
            Wait_for_write = false;
        }
        if (Update_list_uisave)
        {
            hud_ui_save_load_get_list();
            Update_list_uisave = false;
        }
        hud_ui_save_load_process_input();
        hud_ui_save_load_process_header();
        hud_ui_save_load_process_rows();
        hud_ui_save_load_proccess_hints();
        ui_add_for_processing(Doc_handle_uisave);
    }
    ui_process(Control_id_uisave, false);
}

// 0x008F6AF0 (:1798)
void hud_ui_save_load_render(rl_renderer*)
{
    if (Loaded_uisave)
    {
        ui_add_for_rendering(Doc_handle_uisave);
    }
    ui_render(Control_id_uisave, false);
}

// 0x0089E310 (:1810)
void hud_ui_save_load_show(save_load_ui_type type)
{
    Save_load_ui_type = type;
    gameseq_push_state(GS_SAVE_LOAD_SCREEN, true, false);
}

// 0x008EF5C0 (:1824)
void hud_ui_save_load_kill_msgbox()
{
    if (ui_is_msgbox_active(Msgbox_handle_uisave))
    {
        ui_hide_msgbox(Msgbox_handle_uisave, false);
    }
    if (ui_is_msgbox_active(Loading_msg_handle))
    {
        ui_hide_msgbox(Loading_msg_handle, false);
    }
    Loading_msg_handle = -1;
    Msgbox_handle_uisave = -1;
}
