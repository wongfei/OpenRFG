// Original: volition/audio_lib/code/audiolib_soundbank.cpp
//
// Sound and wave banks (SFX.md SX2): the bank list Soundbank_list keeps each bank in the list of its load state
// (waiting for a slot, loading, ready, unloading, failed). A bank id packs the pool index (low byte) and the slot's
// rolling counter (high byte), so a stale id stops matching once the slot is reused. Banks come from soundbanks.xtbl and
// wavebanks.xtbl (and their voice/DLC variants) and are loaded at once (in-memory banks are read whole, streaming banks
// opened) or registered later with data a stream2 container owns (personas, vehicles). audiolib_process_soundbank_list
// moves banks from loading to ready on the Main Audio Thread. Compiled in audio_lib_unity.cpp.

#include "volition/audio_lib/code/audiolib_soundbank.h"

#include "keen/base/trace.hpp"
#include "volition/audio_lib/code/audiolib_hardware.h"
#include "volition/audio_lib/code/audiolib_internal.h"
#include "volition/audio_lib/code/audiolib_thread.h"
#include "volition/audio_lib/code/audiolib_vehicle_engine.h"
#include "volition/audio_lib/code/stub/audiolib_stub.h"
#include "volition/vlib/code/math/crc.h"
#include "volition/vlib/code/memory/vmemory.h"
#include "volition/vlib/code/os/cs_wrapper.h"
#include "volition/vlib/code/os/os.h"
#include "volition/vlib/code/os/timer.h"
#include "volition/vlib/code/parse/xml_table_tools.h"
#include "volition/vlib/code/util/fastxml.h"

#include <ctype.h>
#include <string.h>

// ---- public globals (rfg.pdb; no header declares them)
audiolib_soundbank_data* last_bank_ptr; // 0x01A79AF0
char soundbank_stat_list[128]; // 0x01A79B40, a debug filter for audiolib_debug_print_soundbank_status (never set)

// ---- file-static globals
static bool Audiolib_soundbanks_inited; // 0x01A789C6
static cs_wrapper Audiolib_soundbank_instances_cs(0); // 0x01A7E4B0
// object_pool<audiolib_soundbank_data,252,5> Soundbank_list (0x01A83AD0) is used here too; placed in audiolib.cpp.

// The id of the bank in a pool slot (index in the low byte, the slot's rolling counter in the high byte).
static short audiolib_soundbank_make_id(const audiolib_soundbank_data* bank)
{
    return (short)((bank->index & 0xFF) | (bank->rolling_counter << 8));
}

// 0x00482FC0 (.h:79)
bool audiolib_soundbank_data::is_valid() const
{
    return valid;
}

// 0x00482FD0 (.h:80)
void audiolib_soundbank_data::invalidate()
{
    valid = false;
}

// 0x0048A0B0 (:82)
void audiolib_soundbank_data::drop()
{
    // Soundbank_list's release callback: the slot's next ids no longer match the old ones.
    ++rolling_counter;
    valid = false;
    if (rolling_counter > 0xFF)
    {
        rolling_counter = 1;
    }
}

// 0x00482FE0 (.h:86)
audiolib_soundbank_data::audiolib_soundbank_data()
{
    // The rolling counter is kept in 1..255 from whatever the slot held.
    rolling_counter &= 0xFF;
    bank_buffer = nullptr;
    if (rolling_counter == 0)
    {
        rolling_counter = 1;
    }
    valid = false;
}

// 0x0049D140 (:92)
void audiolib_internal_panic_restore_all_soundbanks()
{
    for (int state = 0; state < NUM_AUDIOLIB_SOUNDBANK_STATES; ++state)
    {
        Soundbank_list.thread_access.enter();
        for (audiolib_soundbank_data* bank = Soundbank_list.get_first(state); bank != nullptr; bank = Soundbank_list.get_next(bank))
        {
            if (bank->is_loaded && !bank->valid)
            {
                // audiolib_hw_register_soundbank, inlined.
                audiolib_hw_register_soundbank(bank);
            }
        }
        Soundbank_list.thread_access.leave();
    }
}

// 0x00496510 (:106)
void audiolib_internal_panic_destroy_all_soundbanks()
{
    for (int state = 0; state < NUM_AUDIOLIB_SOUNDBANK_STATES; ++state)
    {
        Soundbank_list.thread_access.enter();
        for (audiolib_soundbank_data* bank = Soundbank_list.get_first(state); bank != nullptr; bank = Soundbank_list.get_next(bank))
        {
            if (bank->is_loaded && bank->valid)
            {
                // audiolib_hw_unregister_soundbank, inlined (it does nothing past its checks).
                audiolib_hw_unregister_soundbank(bank);
            }
        }
        Soundbank_list.thread_access.leave();
    }
}

// 0x004965D0 (:121)
audiolib_soundbank_data* audiolib_internal_soundbank_id_to_ptr(short id, bool must_be_ready)
{
    if (id == -1)
    {
        return nullptr;
    }
    const unsigned int index = id & 0xFF;
    // 252 slots (0xFC).
    if (index >= 0xFC || index >= Soundbank_list.get_capacity())
    {
        return nullptr;
    }
    audiolib_soundbank_data* bank = Soundbank_list.get_entry((int)index);
    if (bank == nullptr || (id >> 8) != bank->rolling_counter)
    {
        return nullptr;
    }
    if (!must_be_ready)
    {
        return bank;
    }
    if (audiolib_hw_is_sound_bank_ready(bank) >= 0 && Soundbank_list.get_category(bank) == AUDIOLIB_SOUNDBANK_STATE_READY)
    {
        return bank;
    }
    return nullptr;
}

// 0x00496640 (:151)
audiolib_soundbank_load_state audiolib_internal_get_soundbank_state(audiolib_soundbank_data* bank)
{
    const unsigned int index = (unsigned int)Soundbank_list.get_index(bank);
    if (index < Soundbank_list.get_capacity())
    {
        return (audiolib_soundbank_load_state)Soundbank_list.get_category((int)index);
    }
    // ~WAITING_FOR_SLOT: -1.
    return (audiolib_soundbank_load_state)-1;
}

// 0x00496680 (:171)
void audiolib_debug_print_soundbank_status()
{
    // The printing is compiled out in rfg.exe; only the filter tests are left, per bank type.
    for (short type = AUDIOLIB_SOUNDBANK; type <= AUDIOLIB_WAVEBANK_STREAMING; ++type)
    {
        for (int state = 0; state < NUM_AUDIOLIB_SOUNDBANK_STATES; ++state)
        {
            Soundbank_list.thread_access.enter();
            for (audiolib_soundbank_data* bank = Soundbank_list.get_first(state); bank != nullptr; bank = Soundbank_list.get_next(bank))
            {
                if (bank->bank_type == type && soundbank_stat_list[0] != '\0')
                {
                    strstr(bank->bank_name, soundbank_stat_list);
                }
            }
            Soundbank_list.thread_access.leave();
        }
    }
}

// 0x0049D230 (:238)
bool audiolib_process_soundbank_list(void* user_data, int time, int time_step)
{
    (void)user_data;
    (void)time;
    (void)time_step;
    Audiolib_soundbank_instances_cs.enter();
    // A valid soundbank waiting for its slot is loading.
    Soundbank_list.thread_access.enter();
    audiolib_soundbank_data* next = Soundbank_list.get_first(AUDIOLIB_SOUNDBANK_STATE_WAITING_FOR_SLOT);
    while (next != nullptr)
    {
        audiolib_soundbank_data* bank = next;
        next = Soundbank_list.get_next(bank);
        if (bank->bank_type == AUDIOLIB_SOUNDBANK && bank->valid)
        {
            Soundbank_list.move(bank, AUDIOLIB_SOUNDBANK_STATE_LOADING);
        }
    }
    Soundbank_list.thread_access.leave();
    // A loading bank is ready once loaded; a game file load that failed moves it to the failed list.
    Soundbank_list.thread_access.enter();
    next = Soundbank_list.get_first(AUDIOLIB_SOUNDBANK_STATE_LOADING);
    while (next != nullptr)
    {
        audiolib_soundbank_data* bank = next;
        next = Soundbank_list.get_next(bank);
        if (Audiolib_state.game_file_load_status_func != nullptr && bank->file_handle != nullptr)
        {
            const audiolib_result status = Audiolib_state.game_file_load_status_func(bank->file_handle);
            if (status == AUDIOLIB_FILE_LOAD_STATUS_ERROR)
            {
                // The free space is read and not used (a compiled-out report).
                Audiolib_state.pool->space_free();
                Soundbank_list.move(bank, AUDIOLIB_SOUNDBANK_STATE_LOAD_FAILED);
            }
            else if ((unsigned int)(status - AUDIOLIB_FILE_LOAD_STATUS_LOADED) <= 1)
            {
                bank->file_handle = nullptr;
            }
        }
        const short type = bank->bank_type;
        if ((type == AUDIOLIB_SOUNDBANK || type == AUDIOLIB_WAVEBANK || type == AUDIOLIB_WAVEBANK_STREAMING) && bank->is_loaded)
        {
            Soundbank_list.move(bank, AUDIOLIB_SOUNDBANK_STATE_READY);
        }
    }
    Soundbank_list.thread_access.leave();
    Audiolib_soundbank_instances_cs.leave();
    return false;
}

// 0x0048A0E0 (:334)
void string_tolower(char* name)
{
    for (; name != nullptr && *name != '\0'; ++name)
    {
        *name = (char)tolower(*name);
    }
}

// 0x004A2050 (:342)
audiolib_result audiolib_internal_parse_soundbank(xml_element* bank, bool voice, bool dlc, mempool* pool)
{
    char* filename = xml_table_get_req_string_ref(bank, "File");
    if (filename == nullptr)
    {
        return AUDIOLIB_ERR_FOLEY_TABLE;
    }
    string_tolower(filename);
    const char* directory = xml_table_get_req_string_ref(bank, "Directory");
    if (directory == nullptr)
    {
        return AUDIOLIB_ERR_FOLEY_TABLE;
    }
    short bank_id;
    if (audiolib_sound_bank_load(filename, directory, &bank_id, AUDIOLIB_SOUNDBANK, -1, true, pool) < 0)
    {
        return AUDIOLIB_ERR_FOLEY_TABLE;
    }
    if (voice || dlc)
    {
        audiolib_soundbank_data* data = audiolib_internal_soundbank_id_to_ptr(bank_id, false);
        data->dlc = dlc;
        data->isVoiceBank = voice;
    }
    return AUDIOLIB_ERR_NONE;
}

// 0x004A2140 (:381)
audiolib_result audiolib_parse_soundbank_xtbl(const char* filename, bool voice, bool dlc, mempool* pool)
{
    // audiolib_internal_parse_soundbank, inlined per <SndBnk>, with its own error codes.
    audiolib_result result = AUDIOLIB_ERR_NONE;
    if (filename == nullptr)
    {
        return AUDIOLIB_ERR_BAD_PARAM;
    }
    xml_element* root = xml_table_parse(filename);
    if (root == nullptr)
    {
        return result;
    }
    for (xml_element* bank = xml_find(root, "SndBnk"); bank != nullptr; bank = xml_find_next(root, bank, "SndBnk"))
    {
        char* bank_file = xml_table_get_req_string_ref(bank, "File");
        if (bank_file == nullptr)
        {
            result = AUDIOLIB_ERR_FILE_OPEN;
            continue;
        }
        string_tolower(bank_file);
        const char* directory = xml_table_get_req_string_ref(bank, "Directory");
        short bank_id;
        if (directory == nullptr || audiolib_sound_bank_load(bank_file, directory, &bank_id, AUDIOLIB_SOUNDBANK, -1, true, pool) < 0)
        {
            result = AUDIOLIB_ERR_FILE_OPEN;
            continue;
        }
        if (voice || dlc)
        {
            audiolib_soundbank_data* data = audiolib_internal_soundbank_id_to_ptr(bank_id, false);
            data->dlc = dlc;
            data->isVoiceBank = voice;
        }
    }
    xml_table_close(root);
    return result;
}

// 0x004A2280 (:402)
audiolib_result audiolib_internal_parse_wavebank(xml_element* bank, bool voice, bool dlc)
{
    audiolib_result result = AUDIOLIB_ERR_NONE;
    char* filename = xml_table_get_req_string_ref(bank, "File");
    if (filename == nullptr)
    {
        return AUDIOLIB_ERR_FOLEY_TABLE;
    }
    string_tolower(filename);
    const char* directory = xml_table_get_req_string_ref(bank, "Directory");
    if (directory == nullptr)
    {
        return AUDIOLIB_ERR_FOLEY_TABLE;
    }
    const char* type = xml_table_get_req_string_ref(bank, "WBTyp");
    if (type == nullptr)
    {
        return result;
    }
    // Original: load_at_boot is not initialized when the table leaves it out (always present in the retail tables).
    bool load_at_boot = false;
    bool load_with_persona = false;
    xml_table_get_opt_bool(load_at_boot, bank, "load_at_boot");
    xml_table_get_opt_bool(load_with_persona, bank, "load_with_persona");
    short bank_id;
    if (strcmp(type, "streaming") == 0)
    {
        if (audiolib_sound_bank_load(filename, directory, &bank_id, AUDIOLIB_WAVEBANK_STREAMING, 0, true, nullptr) < 0)
        {
            result = AUDIOLIB_ERR_FOLEY_TABLE;
        }
    }
    else
    {
        result = audiolib_sound_bank_load(filename, directory, &bank_id, AUDIOLIB_WAVEBANK, 0, load_at_boot, nullptr);
        if (result >= 0)
        {
            audiolib_soundbank_data* data = audiolib_internal_soundbank_id_to_ptr(bank_id, false);
            if (data != nullptr)
            {
                data->load_at_boot = load_at_boot;
                data->music = strstr(data->bank_name, "mus_") != nullptr;
            }
        }
    }
    if ((voice || dlc) && result == AUDIOLIB_ERR_NONE)
    {
        audiolib_soundbank_data* data = audiolib_internal_soundbank_id_to_ptr(bank_id, false);
        data->dlc = dlc;
        data->isVoiceBank = voice;
    }
    return result;
}

// 0x004A2440 (:487)
audiolib_result audiolib_parse_wavebank_xtbl(const char* filename, bool voice, bool dlc)
{
    // audiolib_internal_parse_wavebank, inlined per <WvBnk>, with its own error codes.
    audiolib_result result = AUDIOLIB_ERR_NONE;
    if (filename == nullptr)
    {
        return AUDIOLIB_ERR_BAD_PARAM;
    }
    xml_element* root = xml_table_parse(filename);
    if (root == nullptr)
    {
        return result;
    }
    // Original: one load_at_boot for the whole table, so a bank that leaves it out takes the previous bank's value (all
    // retail banks have it; the first value is uninitialized stack in rfg.exe).
    bool load_at_boot = false;
    bool load_with_persona = false;
    for (xml_element* bank = xml_find(root, "WvBnk"); bank != nullptr; bank = xml_find_next(root, bank, "WvBnk"))
    {
        audiolib_result bank_result = AUDIOLIB_ERR_NONE;
        char* bank_file = xml_table_get_req_string_ref(bank, "File");
        if (bank_file == nullptr)
        {
            result = AUDIOLIB_ERR_FILE_OPEN;
            continue;
        }
        string_tolower(bank_file);
        const char* directory = xml_table_get_req_string_ref(bank, "Directory");
        if (directory == nullptr)
        {
            result = AUDIOLIB_ERR_FILE_OPEN;
            continue;
        }
        const char* type = xml_table_get_req_string_ref(bank, "WBTyp");
        if (type == nullptr)
        {
            continue;
        }
        xml_table_get_opt_bool(load_at_boot, bank, "load_at_boot");
        xml_table_get_opt_bool(load_with_persona, bank, "load_with_persona");
        short bank_id;
        if (strcmp(type, "streaming") == 0)
        {
            if (audiolib_sound_bank_load(bank_file, directory, &bank_id, AUDIOLIB_WAVEBANK_STREAMING, 0, true, nullptr) < 0)
            {
                bank_result = AUDIOLIB_ERR_FOLEY_TABLE;
            }
        }
        else
        {
            bank_result = audiolib_sound_bank_load(bank_file, directory, &bank_id, AUDIOLIB_WAVEBANK, 0, load_at_boot, nullptr);
            if (bank_result >= 0)
            {
                audiolib_soundbank_data* data = audiolib_internal_soundbank_id_to_ptr(bank_id, false);
                if (data != nullptr)
                {
                    data->load_at_boot = load_at_boot;
                    data->music = strstr(data->bank_name, "mus_") != nullptr;
                }
            }
        }
        if ((voice || dlc) && bank_result == AUDIOLIB_ERR_NONE)
        {
            audiolib_soundbank_data* data = audiolib_internal_soundbank_id_to_ptr(bank_id, false);
            data->dlc = dlc;
            data->isVoiceBank = voice;
        }
        else if (bank_result < 0)
        {
            result = AUDIOLIB_ERR_FILE_OPEN;
        }
    }
    xml_table_close(root);
    return result;
}

// 0x004A2670 (:512)
bool audiolib_internal_soundbank_system_init()
{
    if (Audiolib_soundbanks_inited)
    {
        return true;
    }
    // Original: clear() frees list 0 only (the object pool's clear() is clear(0)).
    Soundbank_list.clear();
    Audiolib_state.estimated_mempool_usage = 0;
    if (audiolib_thread_function_register(audiolib_process_soundbank_list, "audiolib_soundbank_process", nullptr) != AUDIOLIB_ERR_NONE)
    {
        return false;
    }
    Audiolib_soundbanks_inited = true;
    return true;
}

// 0x004A26D0 (:534)
void audiolib_internal_soundbank_system_shutdown()
{
    audiolib_thread_function_unregister(audiolib_process_soundbank_list);
    Audiolib_soundbank_instances_cs.enter();
    // Retried until every bank of every list unloads.
    bool all_unloaded;
    do
    {
        all_unloaded = true;
        for (int state = 0; state < NUM_AUDIOLIB_SOUNDBANK_STATES; ++state)
        {
            Soundbank_list.thread_access.enter();
            audiolib_soundbank_data* next = Soundbank_list.get_first(state);
            while (next != nullptr)
            {
                audiolib_soundbank_data* bank = next;
                next = Soundbank_list.get_next(bank);
                if (audiolib_hw_unload_sound_bank(bank) < 0)
                {
                    all_unloaded = false;
                }
                else
                {
                    Soundbank_list.free(bank);
                }
            }
            Soundbank_list.thread_access.leave();
        }
    } while (!all_unloaded);
    Soundbank_list.clear();
    Audiolib_soundbank_instances_cs.leave();
}

// 0x004968A0 (:565)
bool audiolib_sound_bank_is_valid(short bank_id, bool is_registered)
{
    if (!Audiolib_soundbanks_inited || bank_id == -1)
    {
        return false;
    }
    Audiolib_soundbank_instances_cs.enter();
    const bool valid = audiolib_internal_soundbank_id_to_ptr(bank_id, is_registered) != nullptr;
    Audiolib_soundbank_instances_cs.leave();
    return valid;
}

// 0x004968F0 (:583)
short audiolib_sound_bank_get_id(unsigned int bank_name_crc, short bank_type)
{
    if (!Audiolib_soundbanks_inited || Soundbank_list.get_capacity() == (unsigned int)Soundbank_list.get_free())
    {
        return -1;
    }
    Audiolib_soundbank_instances_cs.enter();
    // Ready banks, then loading banks, then banks waiting for a slot. last_bank_ptr is left at the last bank tested
    // (a debug aid).
    static const int search_states[3] = {AUDIOLIB_SOUNDBANK_STATE_READY, AUDIOLIB_SOUNDBANK_STATE_LOADING, AUDIOLIB_SOUNDBANK_STATE_WAITING_FOR_SLOT};
    for (int i = 0; i < 3; ++i)
    {
        Soundbank_list.thread_access.enter();
        for (audiolib_soundbank_data* bank = Soundbank_list.get_first(search_states[i]); bank != nullptr; bank = Soundbank_list.get_next(bank))
        {
            if (i < 2)
            {
                last_bank_ptr = bank;
            }
            if (bank->bank_name_crc == bank_name_crc && bank->bank_type == bank_type)
            {
                const short id = audiolib_soundbank_make_id(bank);
                Soundbank_list.thread_access.leave();
                Audiolib_soundbank_instances_cs.leave();
                return id;
            }
        }
        Soundbank_list.thread_access.leave();
    }
    Audiolib_soundbank_instances_cs.leave();
    return -1;
}

// 0x00496B30 (:639)
short audiolib_sound_bank_get_id(const char* bank_name, short bank_type)
{
    return audiolib_sound_bank_get_id(crc_str(bank_name, 0), bank_type);
}

// 0x00496B50 (:646)
short audiolib_sound_bank_get_id_from_load_state(const char* bank_name, short bank_type, int load_state)
{
    const unsigned int bank_name_crc = crc_str(bank_name, 0);
    Soundbank_list.thread_access.enter();
    for (audiolib_soundbank_data* bank = Soundbank_list.get_first(load_state); bank != nullptr; bank = Soundbank_list.get_next(bank))
    {
        if (bank->bank_name_crc == bank_name_crc && bank->bank_type == bank_type)
        {
            const short id = audiolib_soundbank_make_id(bank);
            Soundbank_list.thread_access.leave();
            return id;
        }
    }
    Soundbank_list.thread_access.leave();
    return -1;
}

// 0x00496C40 (:665)
const char* audiolib_sound_bank_get_name_by_id(short bank_id)
{
    if (!Audiolib_soundbanks_inited)
    {
        return nullptr;
    }
    Audiolib_soundbank_instances_cs.enter();
    audiolib_soundbank_data* bank = audiolib_internal_soundbank_id_to_ptr(bank_id, true);
    Audiolib_soundbank_instances_cs.leave();
    return bank != nullptr ? bank->bank_name : nullptr;
}

// 0x00496CD0 (:681)
audiolib_result audiolib_sound_bank_get_filename_by_id(short bank_id, char* filename)
{
    if (!Audiolib_soundbanks_inited)
    {
        return AUDIOLIB_ERR_NOT_INITED;
    }
    if (filename == nullptr)
    {
        return AUDIOLIB_ERR_BAD_PARAM;
    }
    Audiolib_soundbank_instances_cs.enter();
    audiolib_soundbank_data* bank = audiolib_internal_soundbank_id_to_ptr(bank_id, false);
    if (bank == nullptr)
    {
        Audiolib_soundbank_instances_cs.leave();
        return AUDIOLIB_ERR_BAD_BANK_ID;
    }
    // The pre-conversion extensions (unused by the keen port).
    strcpy(filename, bank->bank_name);
    switch (bank->bank_type)
    {
    case AUDIOLIB_SOUNDBANK:
        strcat(filename, ".stbs_pc");
        break;
    case AUDIOLIB_WAVEBANK:
    case AUDIOLIB_WAVEBANK_STREAMING:
        strcat(filename, ".stbw_pc");
        break;
    case AUDIOLIB_SETTINGS_FILE:
        strcat(filename, ".stbgs_pc");
        break;
    }
    Audiolib_soundbank_instances_cs.leave();
    return AUDIOLIB_ERR_NONE;
}

// 0x00492350 (:711)
short audiolib_internal_soundbank_get_id_by_index(int bank_index)
{
    if (!Audiolib_soundbanks_inited)
    {
        return -1;
    }
    Audiolib_soundbank_instances_cs.enter();
    const audiolib_soundbank_data* bank = Soundbank_list.get_entry(bank_index);
    if (bank != nullptr)
    {
        const short id = audiolib_soundbank_make_id(bank);
        Audiolib_soundbank_instances_cs.leave();
        return id;
    }
    Audiolib_soundbank_instances_cs.leave();
    return -1;
}

// 0x00496E40 (:726)
int audiolib_internal_soundbank_get_count()
{
    if (!Audiolib_soundbanks_inited)
    {
        return -1;
    }
    Audiolib_soundbank_instances_cs.enter();
    const int count = (int)Soundbank_list.get_capacity() - Soundbank_list.get_free();
    Audiolib_soundbank_instances_cs.leave();
    return count;
}

// 0x0049D410 (:737)
audiolib_result audiolib_sound_bank_register(short bank_id, char* data, int data_size)
{
    if (!Audiolib_soundbanks_inited)
    {
        return AUDIOLIB_ERR_NOT_INITED;
    }
    audiolib_soundbank_data* bank = audiolib_internal_soundbank_id_to_ptr(bank_id, false);
    if (bank == nullptr)
    {
        return AUDIOLIB_ERR_BAD_PARAM;
    }
    if (bank->bank_type == AUDIOLIB_WAVEBANK)
    {
        if (!bank->is_loaded)
        {
            bank->is_loaded = true;
        }
        else if (bank->valid)
        {
            if (!bank->load_at_boot)
            {
                // Registered twice (0x2FB = line 763).
                keen::debug::nativePrintArgs("Int3 critical function at %s(%d)\n", "C:/unit4projects/rfg/root/code/volition/audio_lib/code/audiolib_soundbank.cpp", 0x2FB);
            }
            return AUDIOLIB_ERR_NONE;
        }
    }
    bank->bank_buffer = data;
    bank->bank_size = data_size;
    const audiolib_result result = audiolib_hw_register_soundbank(bank);
    if (result == AUDIOLIB_ERR_NONE)
    {
        Soundbank_list.move(bank, AUDIOLIB_SOUNDBANK_STATE_LOADING);
    }
    return result;
}

// 0x004A8D00 (:804)
audiolib_result audiolib_sound_bank_unregister(short bank_id, bool wait_for_unload)
{
    // The start and end times are taken for a compiled-out report.
    timer_get(TIMER_MILLISECONDS);
    if (!Audiolib_soundbanks_inited)
    {
        return AUDIOLIB_ERR_NOT_INITED;
    }
    audiolib_soundbank_data* bank = audiolib_internal_soundbank_id_to_ptr(bank_id, false);
    if (bank == nullptr)
    {
        return AUDIOLIB_ERR_BAD_PARAM;
    }
    audiolib_sound_bank_prepare_for_unload(bank_id);
    // Spins while the bank's plays die (each pass re-checks the id).
    while (audiolib_internal_soundbank_id_to_ptr(bank_id, false) != nullptr)
    {
        if (audiolib_internal_playlist_sounds_from_bank_killed(bank_id))
        {
            break;
        }
        if (!wait_for_unload)
        {
            return AUDIOLIB_ERR_SOUNDBANK_IN_USE;
        }
    }
    const audiolib_result result = audiolib_hw_unload_sound_bank(bank);
    bank->bank_buffer = nullptr;
    bank->bank_size = 0;
    timer_get(TIMER_MILLISECONDS);
    return result;
}

// 0x004A8880 (:843)
audiolib_result audiolib_sound_bank_prepare_for_unload(short bank_id)
{
    if (!Audiolib_soundbanks_inited)
    {
        return AUDIOLIB_ERR_NOT_INITED;
    }
    audiolib_soundbank_data* bank = audiolib_internal_soundbank_id_to_ptr(bank_id, false);
    if (bank != nullptr)
    {
        Audiolib_play_cs.enter();
        bank->is_loaded = false;
        bank->valid = false;
        Soundbank_list.move(bank, AUDIOLIB_SOUNDBANK_STATE_UNLOADING);
        Audiolib_play_cs.leave();
        audiolib_internal_vehicle_engine_kill_with_extreme_prejudice(bank_id);
        audiolib_internal_playlist_kill_sounds_from_bank(bank_id);
    }
    return AUDIOLIB_ERR_NONE;
}

// 0x0049D4F0 (:872)
audiolib_result audiolib_sound_bank_ready_for_unload(short bank_id, bool* status)
{
    if (status == nullptr)
    {
        return AUDIOLIB_ERR_BAD_PARAM;
    }
    if (audiolib_internal_soundbank_id_to_ptr(bank_id, false) != nullptr)
    {
        *status = audiolib_internal_playlist_sounds_from_bank_killed(bank_id);
        return AUDIOLIB_ERR_NONE;
    }
    *status = true;
    return AUDIOLIB_ERR_NONE;
}

// 0x0049D560 (:911)
audiolib_result audiolib_sound_bank_load(const char* filename, const char* dir, short* bank_id, short bank_type, short sector_read, bool load_now, mempool* pool)
{
    if (!Audiolib_soundbanks_inited)
    {
        return AUDIOLIB_ERR_NOT_INITED;
    }
    if (filename == nullptr)
    {
        return AUDIOLIB_ERR_BAD_PARAM;
    }
    const short id = audiolib_sound_bank_get_id(crc_str(filename, 0), bank_type);
    if (id != -1)
    {
        // Already listed: nothing to do once loaded. Original: a stale id would read through a null bank here (get_id
        // only returns current ids).
        const audiolib_soundbank_data* existing = audiolib_internal_soundbank_id_to_ptr(id, false);
        if (existing->is_loaded)
        {
            return AUDIOLIB_ERR_NONE;
        }
    }
    audiolib_result result = AUDIOLIB_ERR_NONE;
    Audiolib_soundbank_instances_cs.enter();
    audiolib_soundbank_data* bank = id == -1 ? Soundbank_list.alloc(AUDIOLIB_SOUNDBANK_STATE_WAITING_FOR_SLOT)
                                             : audiolib_internal_soundbank_id_to_ptr(id, false);
    Audiolib_soundbank_instances_cs.leave();
    if (bank == nullptr)
    {
        return AUDIOLIB_ERR_NO_SOUNDBANK_SLOTS;
    }
    strncpy(bank->bank_name, filename, 64);
    bank->sub_dir[0] = '\0';
    // Packfile builds find the banks by name.
    if (dir != nullptr && !Audiolib_state.packfile_build)
    {
        strncpy(bank->sub_dir, dir, 64);
    }
    bank->bank_name_crc = crc_str(bank->bank_name, 0);
    bank->bank_type = bank_type;
    bank->sector_read = sector_read;
    bank->index = (short)Soundbank_list.get_index(bank);
    if (bank_id != nullptr)
    {
        *bank_id = audiolib_soundbank_make_id(bank);
    }
    last_bank_ptr = bank;
    if (load_now)
    {
        switch (bank_type)
        {
        case AUDIOLIB_SOUNDBANK:
        case AUDIOLIB_WAVEBANK_STREAMING:
            result = audiolib_hw_load_sound_bank(bank, pool);
            if (result != AUDIOLIB_ERR_NONE)
            {
                break;
            }
            if (!bank->is_loaded)
            {
                Soundbank_list.move(bank, AUDIOLIB_SOUNDBANK_STATE_LOADING);
                break;
            }
            result = audiolib_hw_register_soundbank(bank);
            if (result == AUDIOLIB_ERR_NONE)
            {
                Soundbank_list.move(bank, AUDIOLIB_SOUNDBANK_STATE_READY);
            }
            break;
        case AUDIOLIB_WAVEBANK:
            // An in-memory wavebank becomes ready through audiolib_process_soundbank_list.
            result = audiolib_hw_load_sound_bank(bank, pool);
            if (result == AUDIOLIB_ERR_NONE && (!bank->is_loaded || (result = audiolib_hw_register_soundbank(bank)) == AUDIOLIB_ERR_NONE))
            {
                Soundbank_list.move(bank, AUDIOLIB_SOUNDBANK_STATE_LOADING);
            }
            break;
        default:
            break;
        }
    }
    // A bank still waiting for its slot is loading (also when the load failed: it is freed below).
    const int index = Soundbank_list.get_index(bank);
    if ((unsigned int)index >= Soundbank_list.get_capacity() || Soundbank_list.get_category(index) == AUDIOLIB_SOUNDBANK_STATE_WAITING_FOR_SLOT)
    {
        Soundbank_list.move(bank, AUDIOLIB_SOUNDBANK_STATE_LOADING);
    }
    if (result >= 0)
    {
        ++Audiolib_state.num_soundbanks;
        return AUDIOLIB_ERR_NONE;
    }
    Audiolib_soundbank_instances_cs.enter();
    Soundbank_list.free(bank);
    Audiolib_soundbank_instances_cs.leave();
    return result;
}

// 0x004A8DF0 (:1118)
audiolib_result audiolib_sound_bank_unload(short bank_id, bool free, bool wait_for_unload)
{
    if (!Audiolib_soundbanks_inited)
    {
        return AUDIOLIB_ERR_NOT_INITED;
    }
    if (bank_id == -1)
    {
        return AUDIOLIB_ERR_BAD_BANK_ID;
    }
    audiolib_result result = audiolib_sound_bank_unregister(bank_id, wait_for_unload);
    if (result < 0)
    {
        return result;
    }
    Audiolib_soundbank_instances_cs.enter();
    audiolib_soundbank_data* bank = audiolib_internal_soundbank_id_to_ptr(bank_id, false);
    if (bank == nullptr)
    {
        Audiolib_soundbank_instances_cs.leave();
        return AUDIOLIB_ERR_BAD_BANK_ID;
    }
    Soundbank_list.move(bank, AUDIOLIB_SOUNDBANK_STATE_UNLOADING);
    Audiolib_soundbank_instances_cs.leave();
    // Unloaded a second time (the unregister already did): harmless, everything is null by now.
    if (audiolib_hw_unload_sound_bank(bank) == AUDIOLIB_ERR_SOUNDBANK_IN_USE)
    {
        return AUDIOLIB_ERR_SOUNDBANK_IN_USE;
    }
    if (free)
    {
        --Audiolib_state.num_soundbanks;
        Audiolib_soundbank_instances_cs.enter();
        Soundbank_list.free(bank);
        Audiolib_soundbank_instances_cs.leave();
    }
    return AUDIOLIB_ERR_NONE;
}

// 0x00496E80 (:1156)
audiolib_result audiolib_internal_soundbank_get_cue_index(short bank_id, const char* cue_name, int* cue_index)
{
    if (!Audiolib_soundbanks_inited)
    {
        return AUDIOLIB_ERR_NOT_INITED;
    }
    if (bank_id == -1)
    {
        return AUDIOLIB_ERR_BAD_BANK_ID;
    }
    Audiolib_soundbank_instances_cs.enter();
    audiolib_soundbank_data* bank = audiolib_internal_soundbank_id_to_ptr(bank_id, false);
    if (bank == nullptr || bank->bank_type != AUDIOLIB_SOUNDBANK)
    {
        Audiolib_soundbank_instances_cs.leave();
        return AUDIOLIB_ERR_BAD_BANK_ID;
    }
    int index = -1;
    const audiolib_result result = audiolib_hw_get_cue_index(bank, cue_name, &index);
    if (result == AUDIOLIB_ERR_NONE && cue_index != nullptr)
    {
        *cue_index = index;
    }
    Audiolib_soundbank_instances_cs.leave();
    return result;
}

// 0x00496F40 (:1189)
bool audiolib_internal_soundbank_is_loaded(short bank_id)
{
    const audiolib_soundbank_data* bank = audiolib_internal_soundbank_id_to_ptr(bank_id, true);
    return bank != nullptr && bank->valid && bank->is_loaded;
}

// 0x00496FC0 (:1212)
bool audiolib_internal_soundbank_is_bank_in_RAM(short wavebank_id)
{
    // The same test as audiolib_internal_soundbank_is_loaded.
    const audiolib_soundbank_data* bank = audiolib_internal_soundbank_id_to_ptr(wavebank_id, true);
    return bank != nullptr && bank->valid && bank->is_loaded;
}

// 0x004923C0 (:1243)
bool audiolib_sounbank_get_wavebank_name(int index, char** name)
{
    if (name == nullptr)
    {
        return false;
    }
    Audiolib_soundbank_instances_cs.enter();
    audiolib_soundbank_data* bank = Soundbank_list.get_entry(index);
    if (bank != nullptr && (bank->bank_type == AUDIOLIB_WAVEBANK || bank->bank_type == AUDIOLIB_WAVEBANK_STREAMING))
    {
        *name = bank->bank_name;
        Audiolib_soundbank_instances_cs.leave();
        return true;
    }
    Audiolib_soundbank_instances_cs.leave();
    return false;
}

// 0x00497040 (:1263)
void audiolib_soundbank_dlc_init()
{
    for (int state = 0; state < NUM_AUDIOLIB_SOUNDBANK_STATES; ++state)
    {
        Soundbank_list.thread_access.enter();
        audiolib_soundbank_data* next = Soundbank_list.get_first(state);
        while (next != nullptr)
        {
            audiolib_soundbank_data* bank = next;
            next = Soundbank_list.get_next(bank);
            if (bank->hw_bank_id.is_prepared || !bank->dlc)
            {
                continue;
            }
            if (bank->bank_type == AUDIOLIB_WAVEBANK)
            {
                if (!audiolib_process_inMemory_wavebank(bank))
                {
                    keen::debug::nativePrintArgs("[Audiolib] <audiolib_voices_init> Wavebank not correct processed. Bank name: %s \n", bank->bank_name);
                }
            }
            else if (bank->bank_type == AUDIOLIB_WAVEBANK_STREAMING && !audiolib_process_streaming_wavebank(bank))
            {
                keen::debug::nativePrintArgs("[Audiolib] <audiolib_voices_init> Streaming wavebank not correct processed. Bank name: %s \n", bank->bank_name);
            }
        }
        Soundbank_list.thread_access.leave();
    }
}

// The unload loop of audiolib_soundbank_dlc_shutdown and audiolib_soundbank_voice_shutdown (inlined in both): every
// marked bank is unloaded and freed; a bank still in use ends that list's pass, and the whole pass repeats 1 ms later.
template<typename Marked>
static void audiolib_soundbank_unload_marked(Marked is_marked)
{
    for (;;)
    {
        bool all_unloaded = true;
        Audiolib_soundbank_instances_cs.enter();
        for (int state = 0; state < NUM_AUDIOLIB_SOUNDBANK_STATES; ++state)
        {
            Soundbank_list.thread_access.enter();
            audiolib_soundbank_data* next = Soundbank_list.get_first(state);
            while (next != nullptr)
            {
                audiolib_soundbank_data* bank = next;
                next = Soundbank_list.get_next(bank);
                if (!is_marked(bank))
                {
                    continue;
                }
                const short bank_id = audiolib_internal_soundbank_get_id_by_index(bank->index);
                if (audiolib_sound_bank_unload(bank_id, true, false) == AUDIOLIB_ERR_SOUNDBANK_IN_USE)
                {
                    all_unloaded = false;
                    break;
                }
            }
            Soundbank_list.thread_access.leave();
        }
        Audiolib_soundbank_instances_cs.leave();
        if (all_unloaded)
        {
            return;
        }
        os_sleep(1);
    }
}

// 0x004A8EF0 (:1290)
void audiolib_soundbank_dlc_shutdown()
{
    audiolib_soundbank_unload_marked([](const audiolib_soundbank_data* bank) { return bank->dlc; });
}

// 0x00497150 (:1328)
void audiolib_soundbank_voice_init()
{
    for (int state = 0; state < NUM_AUDIOLIB_SOUNDBANK_STATES; ++state)
    {
        Soundbank_list.thread_access.enter();
        audiolib_soundbank_data* next = Soundbank_list.get_first(state);
        while (next != nullptr)
        {
            audiolib_soundbank_data* bank = next;
            next = Soundbank_list.get_next(bank);
            if (bank->hw_bank_id.is_prepared || !bank->isVoiceBank)
            {
                continue;
            }
            if (bank->bank_type == AUDIOLIB_WAVEBANK)
            {
                if (!audiolib_process_inMemory_wavebank(bank))
                {
                    keen::debug::nativePrintArgs("[Audiolib] <audiolib_voices_init> Wavebank not correct processed. Bank name: %s \n", bank->bank_name);
                }
            }
            else if (bank->bank_type == AUDIOLIB_WAVEBANK_STREAMING && !audiolib_process_streaming_wavebank(bank))
            {
                keen::debug::nativePrintArgs("[Audiolib] <audiolib_voices_init> Streaming wavebank not correct processed. Bank name: %s \n", bank->bank_name);
            }
        }
        Soundbank_list.thread_access.leave();
    }
}

// 0x004A9060 (:1355)
void audiolib_soundbank_voice_shutdown()
{
    audiolib_soundbank_unload_marked([](const audiolib_soundbank_data* bank) { return bank->isVoiceBank; });
}
