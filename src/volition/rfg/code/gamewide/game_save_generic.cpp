// Original: volition/rfg/code/gamewide/game_save_generic.cpp
//
// The save system on keen's save data: every save slot, the options and the profile live in one RfgSaveData block that
// the local game session reads at start-up (Save_system_save_data points at it from then on) and writes back as one
// file when asked (LocalGameSession::startSave). "Loading" and "writing" a slot here are copies in and out of the block.
// The "threads" kept their names from the console versions; on PC they run in the caller. The ini thread is a real one.
//
// Original bugs and quirks kept:
// - game_save_write_thread with a null save_info (the profile import) stores nothing but the profile; a real save_info
//   whose buffer is too large is dropped silently.
// - game_save_load_thread does not check that the slot exists.
// - game_save_delete's scan of the directory cache has no error when the slot is missing.
// - game_save_get_list ignores the sizes it is given.

#include "volition/rfg/code/gamewide/game_save.h"

#include "keen/base/crc32.hpp"
#include "keen/base/filepath.hpp"
#include "keen/base/filesystemtypes.hpp"
#include "keen/base/nativefile.hpp"
#include "keen/base/string.hpp"
#include "keen/base/thread.hpp"
#include "keen/base/trace.hpp"
#include "keen/base/win32/event_win32.hpp"
#include "volition/rfg/code/gamewide/downloadable_content.h"
#include "volition/rfg/code/gamewide/game_options.h"
#include "volition/rfg/code/gamewide/game_save_importer.h"
#include "volition/rfg/code/gamewide/game_trace.h"
#include "volition/rfg/code/gamewide/player_profile.h"
#include "volition/rfg/code/gamewide/rfgsaveinterface.hpp"
#include "volition/rfg/code/gamewide/stats.h"
#include "volition/rfg/code/interface/ui_document_hud_tool_tips.h"
#include "volition/rfg/code/interface/ui_message_box.h"
#include "volition/rfg/code/rfg.h"

#include <string.h>

// Magic numbers: 12 save slots, the room of RfgSaveFile::data.
static const unsigned int Num_save_slots = 12;
static const int Max_save_file_size = 0x280010;
static_assert(sizeof(keen::rfg::RfgSaveFile::data) == Max_save_file_size, "RfgSaveFile::data");

// ---- globals
keen::rfg::RfgSaveData* Save_system_save_data; // 0x025338E4
bool Save_system_ini_reader_is_active; // 0x025338F1
bool Has_imported_any_save_data; // 0x025338F3
bool Can_import_profile_data; // 0x025338F2
unsigned int Num_imported_save_file_infos; // 0x025338F4
game_save_info Imported_save_file_infos[257]; // 0x02505528
// Deviation: twice rfg.exe's 257 entries. qsort_imported_save_data's slot pointer bug can walk up to about twice the
// row count, past the array and over the globals after it in rfg.exe; the spare entries only keep that inside this array.
unsigned int Imported_save_file_info_slots[514]; // 0x0252D2C0

// rfg_game_options Last_options_from_save_data (0x02535858) is listed here too; placed in game_options.cpp.
extern rfg_game_options Last_options_from_save_data;

// ---- file-static globals
static keen::rfg::RfgSaveInterface Game_save_interface; // 0x0165E950
static keen::rfg::RfgSaveFile Import_save_file_buffer; // 0x02284FD8
static keen::rfg::RfgSaveProfileData Import_profile_data_buffer; // 0x025113A0
static game_save_info Directory_cache[Num_save_slots]; // 0x02532ED0
static bool Autosave_present; // 0x025338DD
static bool Dlc_autosave_present; // 0x025338DE
static bool Directory_cache_loaded; // 0x025338DF
static unsigned int Save_system_current_ini_write; // 0x025338E8
static unsigned int Save_system_last_ini_write; // 0x025338EC
static bool Save_system_ini_writer_is_active; // 0x025338F0
static keen::Event Save_system_ini_event; // 0x02535DBC
static keen::Thread Save_system_ini_thread; // 0x0253C8E0

// ---- internal functions

// 0x007C3560 (:75)
static keen::rfg::RfgSaveData* game_save_non_blocking_return_save_data(unit4::LocalGameSession* pGameSession)
{
    return (keen::rfg::RfgSaveData*)pGameSession->getSaveData(nullptr);
}

// The options and profile caches into the block, the checksum over all of it with the checksum field as 0, then the
// session writes the block (inlined at each of its users).
static void game_save_store_block(keen::rfg::RfgSaveData* save_data, unit4::LocalGameSession* session)
{
    memcpy(&save_data->options, &Last_options_from_save_data, sizeof(rfg_game_options));
    memcpy(&save_data->profile_data, &Last_profile_data_from_save_data, sizeof(player_profile_data));
    save_data->header.checksum = 0;
    save_data->header.checksum = keen::getCrc32Value(save_data, sizeof(keen::rfg::RfgSaveData));
    session->startSave(nullptr);
}

// 0x007EFE30 (:86). LTCG in rfg.exe: save_info in EAX, buffer_size in ECX (the buffer's capacity, only the callback
// sees it, and only when there is no buffer), callback and buffer on the stack.
static void game_save_load_thread(game_save_info* save_info, void (*callback)(void*, int), void* buffer, int buffer_size)
{
    keen::rfg::RfgSaveData* save_data = Save_system_save_data;
    if (save_data == nullptr)
    {
        return;
    }
    if (buffer != nullptr)
    {
        const keen::rfg::RfgSaveFile& file = save_data->save_files[save_info->save_slot_index];
        buffer_size = file.size_used;
        memcpy(buffer, file.data, buffer_size);
    }
    memcpy(&Last_profile_data_from_save_data, &save_data->profile_data, sizeof(player_profile_data));
    stats_serialize_in_profile_stats();
    ui_tool_tips_system_load_from_profile();
    if (callback != nullptr)
    {
        callback(buffer, buffer_size);
    }
}

// 0x007D0E20 (:128)
static void game_save_write_thread(game_save_info* save_info, void* buffer, int buffer_size)
{
    unit4::LocalGameSession* session = rfg_get_local_game_session();
    keen::rfg::RfgSaveData* save_data = Save_system_save_data;
    if (buffer_size > Max_save_file_size || save_data == nullptr)
    {
        return;
    }
    if (buffer != nullptr)
    {
        keen::rfg::RfgSaveFile& file = save_data->save_files[save_info->save_slot_index];
        file.exists = true;
        file.info = *save_info;
        file.size_used = buffer_size;
        memcpy(file.data, buffer, buffer_size);
        if (save_info->auto_save)
        {
            if (save_info->dlc_id == -1)
            {
                Autosave_present = true;
            }
            else
            {
                Dlc_autosave_present = true;
            }
        }
    }
    Has_imported_any_save_data = true;
    Last_profile_data_from_save_data.new_data.imported_save_data = true;
    game_save_store_block(save_data, session);
    Save_system_current_ini_write++;
    Save_system_ini_event.signal();
}

// The first directory cache entry that is not in use (inlined in game_save_write and game_save_import_save_slot);
// Num_save_slots when all are.
static unsigned int game_save_find_free_slot()
{
    for (unsigned int i = 0; i < Num_save_slots; i++)
    {
        if (!Directory_cache[i].used_slot)
        {
            return i;
        }
    }
    return Num_save_slots;
}

// 0x007EE860 (:279). Writes the ini file when asked; reads it once the save data is in, and first looks for saves of the
// original release to import.
static int game_save_ini_thread(const keen::Thread& thread)
{
    while (!thread.isQuitRequested())
    {
        Save_system_ini_event.sleepUntilSignaled();
        while (Save_system_last_ini_write < Save_system_current_ini_write)
        {
            Save_system_last_ini_write = Save_system_current_ini_write;
            Save_system_ini_writer_is_active = true;
            game_options_write_ini_file_from_cache();
        }
        Save_system_ini_writer_is_active = false;
        if (Save_system_ini_reader_is_active)
        {
            Num_imported_save_file_infos = 0;
            Can_import_profile_data = importSaveGameProfile(nullptr, getSaveGameImportDirectory());
            // Magic number: the original release's slots.
            for (unsigned int index = 0; index < 0x100; index++)
            {
                keen::FilePath path(getSaveGameImportDirectory(), true);
                char file_name[64];
                keen::formatString(file_name, sizeof(file_name), "rfasave_%02d.rfas_pc", index);
                path.setFileNameWithExtension(file_name);
                if (importSaveGameInfo(&Imported_save_file_infos[Num_imported_save_file_infos], index, getSaveGameImportDirectory()))
                {
                    keen::FileStatus status;
                    // Magic number: the size of the name's buffer as in rfg.exe.
                    if (keen::getNativeFileStatusByName(&status, path.getCompletePath(), 0x10) == keen::IoError_Ok)
                    {
                        Imported_save_file_info_slots[Num_imported_save_file_infos] = index;
                        Num_imported_save_file_infos++;
                    }
                }
            }
            if (Can_import_profile_data)
            {
                // The profile is one more row, without a save.
                const unsigned int row = Num_imported_save_file_infos;
                memset(&Imported_save_file_infos[row], 0, sizeof(game_save_info));
                // Magic number: not a slot.
                Imported_save_file_info_slots[row] = 0xFFFFFFFF;
                Num_imported_save_file_infos++;
                Imported_save_file_infos[row].used_slot = false;
            }
            game_options_load_ini_file_into_cache();
            Save_system_ini_reader_is_active = false;
        }
    }
    return 0;
}

// ---- external interface

// 0x007C3570 (:201)
int game_save_delete(game_save_info* game)
{
    unit4::LocalGameSession* session = rfg_get_local_game_session();
    keen::rfg::RfgSaveData* save_data = Save_system_save_data;
    // Magic number: the error code of rfg.exe, nothing reads it.
    if (save_data == nullptr || !save_data->save_files[game->save_slot_index].exists)
    {
        return -5;
    }
    game->used_slot = false;
    if (game->auto_save)
    {
        if (game->dlc_id == -1)
        {
            Autosave_present = false;
        }
        else
        {
            Dlc_autosave_present = false;
        }
    }
    keen::rfg::RfgSaveFile& file = save_data->save_files[game->save_slot_index];
    file.exists = false;
    file.info = *game;
    file.size_used = 0;
    for (unsigned int i = 0; i < Num_save_slots; i++)
    {
        if (Directory_cache[i].save_slot_index == game->save_slot_index)
        {
            Directory_cache[i] = *game;
            break;
        }
    }
    game_save_store_block(save_data, session);
    ui_show_save_icon(SAVE_ICON_TYPE_MANUAL);
    return 0;
}

// 0x007D99D0 (:342): the original release's multiplayer progress; what this profile completed stays completed.
void game_save_import_profile_data()
{
    memset(&Import_profile_data_buffer, 0, sizeof(Import_profile_data_buffer));
    if (!importSaveGameProfile(&Import_profile_data_buffer, getSaveGameImportDirectory()))
    {
        keen::debug::nativePrintArgs("[Save Data Import] Importing multiplayer progress failed.\n");
        return;
    }
    if (Last_profile_data_from_save_data.game_completed)
    {
        Import_profile_data_buffer.profile_data.game_completed = true;
    }
    if (Last_profile_data_from_save_data.golden_hammer_unlocked)
    {
        Import_profile_data_buffer.profile_data.golden_hammer_unlocked = true;
    }
    if (Last_profile_data_from_save_data.new_data.game_completed_dlc)
    {
        Import_profile_data_buffer.profile_data.new_data.game_completed_dlc = true;
    }
    memcpy(&Last_profile_data_from_save_data, &Import_profile_data_buffer, sizeof(player_profile_data));
    game_save_write_thread(nullptr, nullptr, 0);
    ui_show_save_icon(SAVE_ICON_TYPE_MANUAL);
}

// 0x007E9080 (:377): one save of the original release into a slot.
void game_save_import_save_slot(game_save_info* target, unsigned int imported_slot)
{
    if (target == nullptr)
    {
        return;
    }
    unsigned int slot = target->save_slot_index;
    if (!target->used_slot)
    {
        slot = game_save_find_free_slot();
        if (slot == Num_save_slots)
        {
            return;
        }
        target->save_slot_index = Directory_cache[slot].save_slot_index;
    }
    const unsigned int save_slot_index = target->save_slot_index;
    memset(&Import_save_file_buffer, 0, sizeof(Import_save_file_buffer));
    if (importSaveGame(&Import_save_file_buffer, imported_slot, getSaveGameImportDirectory()))
    {
        *target = Import_save_file_buffer.info;
        target->save_slot_index = save_slot_index;
        target->auto_save = false;
        target->used_slot = true;
        game_save_write_thread(target, Import_save_file_buffer.data, Import_save_file_buffer.size_used);
        ui_show_save_icon(SAVE_ICON_TYPE_MANUAL);
    }
    else
    {
        keen::debug::nativePrintArgs("[Save Data Import] Importing save slot %zu failed.\n", imported_slot);
    }
    // Magic numbers: the missions of the campaign and of the bonus campaign. Original: read even when the import failed
    // (the buffer is then zero, so nothing unlocks).
    if (Import_save_file_buffer.info.dlc_id == -1)
    {
        if (Import_save_file_buffer.info.num_missions_completed > 19)
        {
            Last_profile_data_from_save_data.game_completed = true;
            Last_profile_data_from_save_data.golden_hammer_unlocked = true;
        }
    }
    else if (Import_save_file_buffer.info.dlc_id == 1 && Import_save_file_buffer.info.num_missions_completed > 2)
    {
        Last_profile_data_from_save_data.new_data.game_completed_dlc = true;
    }
    Directory_cache[slot] = *target;
    stats_level_init(false);
}

// 0x007C36A0 (:460)
bool game_save_is_writing_ini_file()
{
    return Save_system_ini_writer_is_active;
}

// 0x007EEA20 (:469)
void game_save_system_init()
{
    Directory_cache_loaded = false;
    memset(Directory_cache, 0, sizeof(Directory_cache));
    game_options_reset(&Last_options_from_save_data);
    memset(&Last_profile_data_from_save_data, 0, sizeof(player_profile_data));
    // Magic number: no sledge.
    Last_profile_data_from_save_data.sledge_unique_id = 0xFF;
    stats_reset_profile_stats();
    Save_system_ini_event.create("INI Writer Event", 0u);
    // Magic numbers: priority 0, a 4 KB stack, core 4.
    Save_system_ini_thread.create("INI Writer Thread", game_save_ini_thread, 0, 0x1000u, 4u);
    Save_system_ini_thread.start(nullptr);
}

// 0x007C36B0 (:486)
void game_save_process()
{
    if (Save_system_save_data != nullptr)
    {
        return;
    }
    Save_system_save_data = game_save_non_blocking_return_save_data(rfg_get_local_game_session());
    if (Save_system_save_data != nullptr)
    {
        Has_imported_any_save_data = Save_system_save_data->profile_data.profile_data.new_data.imported_save_data;
        Save_system_ini_reader_is_active = true;
        Save_system_ini_event.signal();
    }
}

// 0x007C36F0 (:512)
bool game_save_op_completed()
{
    return Save_system_save_data != nullptr && !Save_system_ini_reader_is_active;
}

// 0x007C3710 (:522)
game_save_state game_save_get_state()
{
    return Save_system_save_data != nullptr && !Save_system_ini_reader_is_active ? GSS_NONE : GSS_LOADING_READ;
}

// 0x007C3730 (:532)
int game_save_get_list_buf_size(int max_list_size)
{
    return max_list_size * (int)sizeof(game_save_info);
}

// 0x007D0F40 (:537)
void game_save_check_load_directory_cache()
{
    if (Directory_cache_loaded)
    {
        return;
    }
    Autosave_present = false;
    Dlc_autosave_present = false;
    const keen::rfg::RfgSaveData* save_data = Save_system_save_data;
    for (unsigned int i = 0; i < Num_save_slots; i++)
    {
        game_save_info& entry = Directory_cache[i];
        if (save_data == nullptr || !save_data->save_files[i].exists)
        {
            memset(&entry, 0, sizeof(entry));
            entry.save_slot_index = i;
            entry.used_slot = false;
        }
        else
        {
            entry = save_data->save_files[i].info;
            entry.save_slot_index = i;
            entry.used_slot = true;
            if (entry.auto_save)
            {
                if (entry.dlc_id == -1)
                {
                    Autosave_present = true;
                }
                else
                {
                    Dlc_autosave_present = true;
                }
            }
        }
    }
    Directory_cache_loaded = true;
}

// 0x007C3740 (:594)
void game_save_clear_load_directory_cache()
{
    Directory_cache_loaded = false;
}

// 0x007D1010 (:599)
int game_save_get_list(game_save_info* buf, int, int, bool displayAllSavegames)
{
    int count = 0;
    game_save_check_load_directory_cache();
    for (unsigned int i = 0; i < Num_save_slots; i++)
    {
        if (Directory_cache[i].used_slot && (displayAllSavegames || Directory_cache[i].dlc_id == dlc_get_current_id()))
        {
            buf[count] = Directory_cache[i];
            count++;
        }
    }
    return count;
}

// 0x007C3750 (:629)
bool game_save_is_autosave_present()
{
    return Autosave_present;
}

// 0x007C3760 (:634)
bool game_save_is_dlc_autosave_present()
{
    return Dlc_autosave_present;
}

// 0x007D1160 (:640)
game_save_info* game_save_get_autosave(game_save_info* buf, int buf_size, int max_list_size, int, bool displayAllSavegames, int dlc_id)
{
    const int count = game_save_get_list(buf, buf_size, max_list_size, displayAllSavegames);
    for (int i = 0; i < count; i++)
    {
        if (buf[i].auto_save && buf[i].dlc_id == dlc_id && buf[i].used_slot)
        {
            return &buf[i];
        }
    }
    return nullptr;
}

// 0x007D11C0 (:661)
int game_save_write(game_save_info* game, void* data, int len)
{
    // Magic numbers: the error codes of rfg.exe, nothing reads them.
    if (game == nullptr)
    {
        return -10;
    }
    unsigned int slot = game->save_slot_index;
    if (!game->used_slot)
    {
        slot = game_save_find_free_slot();
        if (slot == Num_save_slots)
        {
            return -7;
        }
        game->save_slot_index = Directory_cache[slot].save_slot_index;
    }
    game->used_slot = true;
    Directory_cache[slot] = *game;
    GAME_TRACE("save", "game_save_write: slot %u auto=%d, %d bytes", game->save_slot_index, game->auto_save ? 1 : 0, len);
    game_save_write_thread(game, data, len);
    ui_show_save_icon(game->auto_save ? SAVE_ICON_TYPE_AUTO : SAVE_ICON_TYPE_MANUAL);
    return 0;
}

// 0x007EFEB0 (:702)
int game_save_load(game_save_info* game, void* buf, void (*completion_callback)(void*, int))
{
    // Magic numbers: the error codes of rfg.exe.
    if (game == nullptr)
    {
        return -10;
    }
    if (!game->used_slot)
    {
        return -11;
    }
    GAME_TRACE("save", "game_save_load: slot %u", game->save_slot_index);
    game_save_load_thread(game, completion_callback, buf, Max_save_file_size);
    return 0;
}

// 0x007D12A0 (:713)
bool game_save_is_saving_enbaled()
{
    unit4::LocalGameSession* session = rfg_get_local_game_session();
    if (session == nullptr || session->didLocalPlayerExit(nullptr))
    {
        return false;
    }
    return !session->isSavingDisabledForUser(nullptr);
}

// 0x007C3770 (:721)
void game_save_start_get_save_data()
{
    Save_system_save_data = nullptr;
}

// 0x007C3780 (:726)
bool game_save_has_save_data()
{
    return Save_system_save_data != nullptr && !Save_system_ini_reader_is_active;
}

// 0x007C37A0 (:735)
void game_save_clear_save_data()
{
    Save_system_save_data = nullptr;
}

// 0x007EFEF0 (:741)
int game_save_load_options()
{
    if (Save_system_save_data != nullptr)
    {
        memcpy(&Last_profile_data_from_save_data, &Save_system_save_data->profile_data, sizeof(player_profile_data));
        stats_serialize_in_profile_stats();
        ui_tool_tips_system_load_from_profile();
    }
    return 0;
}

// The options and the profile written, with the ini file (the body of game_save_write_options and _profile_data).
static void game_save_write_settings()
{
    unit4::LocalGameSession* session = rfg_get_local_game_session();
    keen::rfg::RfgSaveData* save_data = Save_system_save_data;
    if (save_data != nullptr)
    {
        Has_imported_any_save_data = true;
        Last_profile_data_from_save_data.new_data.imported_save_data = true;
        game_save_store_block(save_data, session);
        Save_system_current_ini_write++;
        Save_system_ini_event.signal();
    }
}

// 0x007D12F0 (:749)
int game_save_write_options()
{
    game_save_write_settings();
    ui_show_save_icon(SAVE_ICON_TYPE_OPTIONS);
    return 0;
}

// 0x007D1380 (:759)
int game_save_write_profile_data()
{
    game_save_write_settings();
    ui_show_save_icon(SAVE_ICON_TYPE_PROFILE);
    return 0;
}

// 0x007C37B0 (:769)
unit4::SaveDataDescriptionInterface* game_save_get_save_interface_for_game_session()
{
    return &Game_save_interface;
}
