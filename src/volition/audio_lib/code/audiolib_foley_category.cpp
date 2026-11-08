// Original: volition/audio_lib/code/audiolib_foley_category.cpp
//
// Foley cues and categories (SFX.md SX2, 1.3). foley.aud_pc ('VFD '/'PC  ', version <= 6) holds the categories (0x14
// each) and the cues (0xE4 each, up to 8 variations); both are hashed by name crc into one table (foley_hash_table: a
// category name maps to its index, a cue name to its caller cue id). The cue list has the file's cues, then 64 persona
// banks of 395 slots and six fixed persona banks (PCM01, OFFCAM, RFC, SAFE, HANDBOOK, TUTORIAL) that the voice
// containers register and unregister. A caller cue id is the foley index in the low half and the low 16 bits of the
// foley entry's address in the high half (a check against stale ids). Categories carry volume fades, mute, pause,
// ducking and instance limits; audiolib_foley_category_process fades them on the Main Audio Thread. Compiled in
// audio_lib_unity.cpp.

#include "volition/audio_lib/code/audiolib_foley_category.h"

#include "keen/base/language.hpp"
#include "keen/base/string.hpp"
#include "volition/audio_lib/code/audiolib_hardware.h"
#include "volition/audio_lib/code/audiolib_internal.h"
#include "volition/audio_lib/code/audiolib_shared_bank_header.h"
#include "volition/audio_lib/code/audiolib_soundbank.h"
#include "volition/audio_lib/code/audiolib_thread.h"
#include "volition/vlib/code/cfile/cfile.h"
#include "volition/vlib/code/math/crc.h"
#include "volition/vlib/code/memory/vmemory.h"
#include "volition/vlib/code/os/cs_wrapper.h"
#include "volition/vlib/code/os/timer.h"
#include "volition/vlib/code/util/string_util.h"

#include <ctype.h>
#include <math.h>
#include <string.h>

// ---- public globals (rfg.pdb)
audiolib_foley_list Audiolib_foley_list; // 0x01A789E8
audiolib_category_list Audiolib_category_list; // 0x01A79954
audiolib_encoded_string_hash_table foley_hash_table; // 0x01A79ED0
cs_wrapper foley_hash_table_cs(0); // 0x01A97B20
// The cue being parsed (debug aids, written only).
audiolib_foley_data* my_foley_data; // 0x01A789E4
volition_foley_cue_header_persona_info* persona_cue_info; // 0x01A79A94
volition_foley_cue_header_output_info* my_cue_info; // 0x01A79AF8

// ---- file-static globals
static keen::LanguageId Audiolib_game_language = keen::LanguageId_En_Us; // 0x014F9134
static char debug_decoded_name[8][64]; // 0x01A79CD0
static cs_wrapper Audiolib_category_cs(0); // 0x01A7BBA0
static cs_wrapper Audiolib_foley_list_cs(0); // 0x01A7DEF0
// object_pool<audiolib_soundbank_data,252,5> Soundbank_list (0x01A83AD0) is used here too; placed in audiolib.cpp.

// Slots per persona bank and the cue limits of the fixed persona banks (their start indices are 469, 345, 265, 210
// and 38 apart in audiolib_load_category_and_foley_data).
static const int Persona_bank_slots = 395;
static const int Persona_PCM01_cues = 469;
static const int Persona_OFFCAM_cues = 345;
static const int Persona_RFC_cues = 265;
static const int Persona_SAFE_cues = 210;
static const int Persona_HANDBOOK_cues = 38;
static const int Persona_TUTORIAL_cues = 47;

// The caller cue id of a foley entry.
static int audiolib_foley_make_cue_id(const audiolib_foley_data* foley)
{
    return (int)((unsigned int)(foley - Audiolib_foley_list.foley) & 0xFFFF) | (int)((unsigned int)(uintptr_t)foley << 16);
}

// The slot of a crc in the foley hash table (an integer hash, then modulo the slot count).
static unsigned int audiolib_foley_hash_slot(unsigned int crc)
{
    unsigned int hash = crc * 0x1001;
    hash = (hash >> 22 ^ hash) * 0x11;
    hash = (hash >> 9 ^ hash) * 0x401;
    hash = (hash >> 2 ^ hash) * 0x81;
    return (hash >> 12 ^ hash) % (unsigned int)foley_hash_table.number_of_slots;
}

// Removes a crc from the foley hash table (audiolib_foley_hash_table_remove without the zero crc test; inlined in
// audiolib_unparse_foley_persona_data). The caller holds foley_hash_table_cs.
static void audiolib_foley_hash_table_unlink(unsigned int crc)
{
    const unsigned int slot = audiolib_foley_hash_slot(crc);
    audiolib_encoded_string_hash_entry* entry = foley_hash_table.slots[slot];
    // Original: the head entry is read before the null test.
    if (entry->crc == crc)
    {
        foley_hash_table.slots[slot] = entry->next;
    }
    else
    {
        audiolib_encoded_string_hash_entry* previous = nullptr;
        while (entry != nullptr && entry->crc != crc)
        {
            previous = entry;
            entry = entry->next;
        }
        if (entry == nullptr)
        {
            return;
        }
        previous->next = entry->next;
    }
    entry->next = foley_hash_table.free_entries;
    foley_hash_table.free_entries = entry;
}

// Clears a foley entry (inlined in rfg.exe).
static void audiolib_foley_clear(audiolib_foley_data* foley)
{
    memset(foley, 0, sizeof(*foley));
}

// Whether a persona bank holds the foley index.
static bool audiolib_foley_bank_contains(const audiolib_foley_bank_data& bank, int index)
{
    return bank.in_use && bank.source_data != nullptr && bank.source_data_size != 0 && index >= bank.start_index &&
           index < bank.start_index + bank.num_cues;
}

// Resets a persona bank's registration.
static void audiolib_foley_bank_reset(audiolib_foley_bank_data& bank)
{
    bank.persona_filename[0] = '\0';
    bank.source_data = nullptr;
    bank.source_data_size = 0;
    bank.num_cues = 0;
    bank.in_use = false;
}

// Registers a parsed persona bank.
static void audiolib_foley_bank_set(audiolib_foley_bank_data& bank, const char* filename, unsigned char* data, int data_size, int num_cues)
{
    strncpy(bank.persona_filename, filename, 32);
    bank.source_data = data;
    bank.source_data_size = data_size;
    bank.num_cues = num_cues;
    bank.in_use = true;
}

// Whether the crc is in a crc list of the settings (inlined in the parsers).
static bool audiolib_foley_in_list(const unsigned int* crcs, int count, unsigned int name_crc)
{
    for (int i = 0; i < count; ++i)
    {
        if (crcs[i] == name_crc)
        {
            return true;
        }
    }
    return false;
}

// 0x00485DA0 (:128), input_string in EAX
static bool copy_to_lower(const char* input_string, char* buffer)
{
    buffer[0] = '\0';
    if (input_string == nullptr)
    {
        return false;
    }
    for (int i = 0; i < 256; ++i)
    {
        if (input_string[i] == '\0')
        {
            buffer[i] = '\0';
            return true;
        }
        buffer[i] = (char)tolower(input_string[i]);
    }
    // Original: a 256-character name is refused (and left unterminated).
    return false;
}

// 0x00485E00 (:150)
static bool is_offcam_persona_name(const char* persona_file_name)
{
    char buffer[256];
    if (persona_file_name == nullptr || !copy_to_lower(persona_file_name, buffer))
    {
        return false;
    }
    return strstr(buffer, "offcam") != nullptr;
}

// 0x00485E80 (:166)
static bool is_player_persona_name(const char* persona_file_name)
{
    char buffer[256];
    if (persona_file_name == nullptr || !copy_to_lower(persona_file_name, buffer))
    {
        return false;
    }
    return strstr(buffer, "dsam") != nullptr || strstr(buffer, "pcm01") != nullptr;
}

// 0x00485F30 (:187)
static bool is_rfc_persona_name(const char* persona_file_name)
{
    char buffer[256];
    if (persona_file_name == nullptr || !copy_to_lower(persona_file_name, buffer))
    {
        return false;
    }
    return strstr(buffer, "rfc") != nullptr;
}

// 0x00485FB0 (:203)
static bool is_safe_persona_name(const char* persona_file_name)
{
    char buffer[256];
    if (persona_file_name == nullptr || !copy_to_lower(persona_file_name, buffer))
    {
        return false;
    }
    return strstr(buffer, "dvasha") != nullptr || strstr(buffer, "safe") != nullptr;
}

// 0x00486060 (:228)
void audiolib_set_game_language_hack(keen::LanguageId audio_lang)
{
    Audiolib_game_language = audio_lang;
}

// The crc of a bank name with its localized extension appended (".xsb_pc.<lang>" or ".xwb_pc.<lang>", lowercase,
// '-' as '_'), continued from the plain name's crc.
static unsigned int audiolib_foley_localized_bank_crc(unsigned int original_crc, short bank_type, keen::LanguageId language)
{
    char extension[64];
    keen::formatString(extension, sizeof(extension), bank_type == AUDIOLIB_SOUNDBANK ? ".xsb_pc.%s" : ".xwb_pc.%s", keen::getLanguageIdString(language));
    keen::changeStringToLowercase(extension);
    keen::replaceStringCharacters(extension, '-', '_');
    return crc_stri(extension, original_crc);
}

// 0x0049A110 (:233)
short audiolib_foley_try_find_soundbank_id(unsigned int original_crc, short expected_bank_type)
{
    // The other wavebank type is tried too (a soundbank's alternative is a streaming wavebank).
    const short other_bank_type = (short)(expected_bank_type == AUDIOLIB_WAVEBANK_STREAMING ? AUDIOLIB_WAVEBANK : AUDIOLIB_WAVEBANK_STREAMING);
    const unsigned int localized_crc = audiolib_foley_localized_bank_crc(original_crc, expected_bank_type, Audiolib_game_language);
    short bank_id = audiolib_sound_bank_get_id(localized_crc, expected_bank_type);
    if (bank_id != -1)
    {
        return bank_id;
    }
    bank_id = audiolib_sound_bank_get_id(original_crc, expected_bank_type);
    if (bank_id != -1)
    {
        return bank_id;
    }
    bank_id = audiolib_sound_bank_get_id(localized_crc, other_bank_type);
    if (bank_id != -1)
    {
        return bank_id;
    }
    bank_id = audiolib_sound_bank_get_id(original_crc, other_bank_type);
    if (bank_id != -1)
    {
        return bank_id;
    }
    // English (US) as the fallback language.
    const unsigned int english_crc = audiolib_foley_localized_bank_crc(original_crc, expected_bank_type, keen::LanguageId_En_Us);
    bank_id = audiolib_sound_bank_get_id(english_crc, expected_bank_type);
    if (bank_id == -1)
    {
        bank_id = audiolib_sound_bank_get_id(english_crc, other_bank_type);
    }
    return bank_id;
}

// 0x00486070 (:353)
bool audiolib_foley_hash_table_init(int num_cues)
{
    foley_hash_table_cs.enter();
    memset(&foley_hash_table, 0, sizeof(foley_hash_table));
    // One entry more than asked is allocated; it ends the free chain.
    const unsigned int size = (num_cues + 1) * sizeof(audiolib_encoded_string_hash_entry);
    audiolib_encoded_string_hash_entry* entries = (audiolib_encoded_string_hash_entry*)Audiolib_state.pool->alloc(size, 0);
    if (entries == nullptr)
    {
        foley_hash_table_cs.leave();
        return false;
    }
    memset(entries, 0, size);
    audiolib_encoded_string_hash_entry* entry = entries;
    for (int i = num_cues; i > 0; --i)
    {
        entry->next = entry + 1;
        ++entry;
    }
    foley_hash_table.number_of_entries = num_cues;
    foley_hash_table.number_of_slots = 1024;
    foley_hash_table.free_entries = entries;
    foley_hash_table.entry_ram = (char*)entries;
    foley_hash_table_cs.leave();
    return true;
}

// 0x00486110 (:394)
bool audiolib_foley_hash_table_insert(unsigned int string_crc, int cue_id, unsigned int encoded_offset)
{
    (void)encoded_offset;
    foley_hash_table_cs.enter();
    audiolib_encoded_string_hash_entry* entry = foley_hash_table.free_entries;
    if (entry == nullptr)
    {
        foley_hash_table_cs.leave();
        return false;
    }
    foley_hash_table.free_entries = entry->next;
    entry->crc = string_crc;
    entry->cue_id = cue_id;
    entry->next = nullptr;
    const unsigned int slot = audiolib_foley_hash_slot(string_crc);
    entry->next = foley_hash_table.slots[slot];
    foley_hash_table.slots[slot] = entry;
    foley_hash_table_cs.leave();
    return true;
}

// 0x004861B0 (:429)
bool audiolib_foley_hash_table_insert(const char* string, int cue_id, unsigned int encoded_offset)
{
    return audiolib_foley_hash_table_insert(crc_str(string, 0), cue_id, encoded_offset);
}

// 0x004861D0 (:436)
bool audiolib_foley_hash_table_set(unsigned int crc, int cue_id)
{
    foley_hash_table_cs.enter();
    for (audiolib_encoded_string_hash_entry* entry = foley_hash_table.slots[audiolib_foley_hash_slot(crc)]; entry != nullptr; entry = entry->next)
    {
        if (entry->crc == crc)
        {
            entry->cue_id = cue_id;
            foley_hash_table_cs.leave();
            return true;
        }
    }
    foley_hash_table_cs.leave();
    return false;
}

// 0x00486260 (:457)
bool audiolib_foley_hash_table_remove(unsigned int crc)
{
    if (crc == 0)
    {
        return false;
    }
    foley_hash_table_cs.enter();
    const unsigned int slot = audiolib_foley_hash_slot(crc);
    audiolib_encoded_string_hash_entry* entry = foley_hash_table.slots[slot];
    // Original: the head entry is read before the null test.
    if (entry->crc == crc)
    {
        foley_hash_table.slots[slot] = entry->next;
    }
    else
    {
        audiolib_encoded_string_hash_entry* previous = nullptr;
        while (entry->crc != crc)
        {
            previous = entry;
            if (entry->next == nullptr)
            {
                foley_hash_table_cs.leave();
                return false;
            }
            entry = entry->next;
        }
        previous->next = entry->next;
    }
    entry->next = foley_hash_table.free_entries;
    foley_hash_table.free_entries = entry;
    foley_hash_table_cs.leave();
    return true;
}

// 0x00486330 (:510)
int audiolib_foley_hash_table_get(unsigned int crc, unsigned char** encoded_offset)
{
    foley_hash_table_cs.enter();
    if (encoded_offset != nullptr)
    {
        *encoded_offset = nullptr;
    }
    for (audiolib_encoded_string_hash_entry* entry = foley_hash_table.slots[audiolib_foley_hash_slot(crc)]; entry != nullptr; entry = entry->next)
    {
        if (entry->crc == crc)
        {
            const int cue_id = entry->cue_id;
            foley_hash_table_cs.leave();
            return cue_id;
        }
    }
    foley_hash_table_cs.leave();
    return -1;
}

// 0x0048F220 (:549)
char* audiolib_internal_debug_foley_name(unsigned int crc)
{
    // Eight rotating buffers. The cue names are not in the retail data (SX0), so only "crc:0x..." could be written;
    // original: the size passed is 4, so it is "crc".
    static int name_index; // 0x01AC3738
    char* name = debug_decoded_name[name_index % 8];
    ++name_index;
    name[0] = '\0';
    if (Audiolib_state.encoded_strings == nullptr)
    {
        _snprintf_safe(name, 4, "crc:0x%x", crc);
    }
    return name;
}

// 0x004863D0 (:559)
bool audiolib_string_library_decode_string(encoded_string_info* string_library, unsigned int name_crc, char* decoded_string)
{
    if (decoded_string == nullptr)
    {
        return false;
    }
    decoded_string[0] = '\0';
    if (string_library != nullptr)
    {
        // The decoder is compiled out: nothing is decoded from a string library.
        return false;
    }
    // Original: the size passed is 4.
    _snprintf_safe(decoded_string, 4, "crc:0x%x", name_crc);
    return true;
}

// 0x00486400 (:678)
audiolib_foley_data* audiolib_get_foley_data_pointer(int caller_cue_id)
{
    const int index = caller_cue_id & 0xFFFF;
    audiolib_foley_data* foley = nullptr;
    if (index < Audiolib_foley_list.total_cues)
    {
        foley = Audiolib_foley_list.foley + index;
    }
    else
    {
        // A persona cue: only while its bank is registered.
        Audiolib_foley_list_cs.enter();
        const audiolib_foley_bank_data* fixed_banks[6] = {
            &Audiolib_foley_list.persona_PCM01_bank, &Audiolib_foley_list.persona_OFFCAM_bank, &Audiolib_foley_list.persona_RFC_bank,
            &Audiolib_foley_list.persona_SAFE_bank, &Audiolib_foley_list.persona_HANDBOOK_bank, &Audiolib_foley_list.persona_TUTORIAL_bank,
        };
        for (int i = 0; i < 6 && foley == nullptr; ++i)
        {
            if (audiolib_foley_bank_contains(*fixed_banks[i], index))
            {
                foley = Audiolib_foley_list.foley + index;
            }
        }
        for (int i = 0; i < 64 && foley == nullptr; ++i)
        {
            const audiolib_foley_bank_data& bank = Audiolib_foley_list.persona_banks[i];
            if (bank.source_data != nullptr && bank.in_use && bank.source_data_size != 0 && index >= bank.start_index &&
                index < bank.start_index + bank.num_cues)
            {
                foley = Audiolib_foley_list.foley + index;
            }
        }
        Audiolib_foley_list_cs.leave();
    }
    if (foley != nullptr &&
        (audiolib_foley_make_cue_id(foley) != caller_cue_id || !foley->in_use || ((unsigned int)(uintptr_t)foley & 0xFFFF) != ((unsigned int)caller_cue_id >> 16)))
    {
        return nullptr;
    }
    return foley;
}

// 0x00486640 (:774)
audiolib_category_data* audiolib_get_category_data_pointer(short caller_cat_id)
{
    if (caller_cat_id >= 0 && caller_cat_id < Audiolib_category_list.count)
    {
        return Audiolib_category_list.category + caller_cat_id;
    }
    return nullptr;
}

// 0x00486670 (:785)
bool audiolib_internal_category_list_init(int total)
{
    Audiolib_category_cs.enter();
    if (Audiolib_category_list.category != nullptr)
    {
        // Already set up (a reload): only the same count is accepted.
        const bool same = Audiolib_category_list.total == total;
        Audiolib_category_cs.leave();
        return same;
    }
    Audiolib_category_list.category = (audiolib_category_data*)Audiolib_state.pool->alloc(total * sizeof(audiolib_category_data), 0);
    if (Audiolib_category_list.category == nullptr)
    {
        Audiolib_category_cs.leave();
        return false;
    }
    memset(Audiolib_category_list.category, 0, total * sizeof(audiolib_category_data));
    Audiolib_category_list.total = total;
    Audiolib_category_cs.leave();
    return true;
}

// 0x00486700 (:813)
audiolib_category_data* audiolib_internal_category_find(unsigned int category_name_crc)
{
    const short cat_id = (short)audiolib_foley_hash_table_get(category_name_crc, nullptr);
    // Original: category 0 is never found.
    if (cat_id != 0 && cat_id >= 0 && cat_id < Audiolib_category_list.count)
    {
        return Audiolib_category_list.category + cat_id;
    }
    return nullptr;
}

// 0x00486740 (:829)
audiolib_category_data* audiolib_internal_add_category(unsigned int category_name_crc, unsigned int encoded_offset)
{
    Audiolib_category_cs.enter();
    // audiolib_internal_category_find, inlined (category 0 is not found, so its name can be added again).
    audiolib_category_data* category = audiolib_internal_category_find(category_name_crc);
    if (category == nullptr && Audiolib_category_list.count < Audiolib_category_list.total)
    {
        category = Audiolib_category_list.category + Audiolib_category_list.count;
        memset(category, 0, sizeof(*category));
        if (!audiolib_foley_hash_table_insert(category_name_crc, Audiolib_category_list.count, encoded_offset))
        {
            Audiolib_category_cs.leave();
            return nullptr;
        }
        category->name_crc = category_name_crc;
        category->curr_volume = 1.0f;
        category->dest_volume = 1.0f;
        category->cat_id = (short)Audiolib_category_list.count;
        ++Audiolib_category_list.count;
    }
    Audiolib_category_cs.leave();
    return category;
}

// 0x00486810 (:854)
audiolib_category_data* audiolib_internal_add_category(const char* category_name, unsigned int encoded_offset)
{
    return audiolib_internal_add_category(crc_str(category_name, 0), encoded_offset);
}

// 0x00486830 (:873)
bool audiolib_internal_foley_list_init(int total_cues, int total_variations)
{
    // One variation more than asked.
    const int variations = total_variations + 1;
    if (Audiolib_foley_list.foley != nullptr)
    {
        return Audiolib_foley_list.total_cues == total_cues && Audiolib_foley_list.total_variations == variations;
    }
    memset(&Audiolib_foley_list, 0, sizeof(Audiolib_foley_list));
    Audiolib_foley_list.foley = (audiolib_foley_data*)Audiolib_state.pool->alloc(total_cues * sizeof(audiolib_foley_data), 0);
    Audiolib_foley_list.variations = (audiolib_variation_info*)Audiolib_state.pool->alloc(variations * sizeof(audiolib_variation_info), 0);
    if (Audiolib_foley_list.foley == nullptr || Audiolib_foley_list.variations == nullptr)
    {
        return false;
    }
    memset(Audiolib_foley_list.foley, 0, total_cues * sizeof(audiolib_foley_data));
    memset(Audiolib_foley_list.variations, 0, variations * sizeof(audiolib_variation_info));
    Audiolib_foley_list.total_variations = variations;
    Audiolib_foley_list.total_cues = total_cues;
    return true;
}

// 0x004868F0 (:917)
int audiolib_internal_foley_get_number_of_cues()
{
    return Audiolib_foley_list.total_cues;
}

// 0x00486900 (:922)
audiolib_foley_data* audiolib_internal_get_foley(unsigned int name_crc)
{
    return audiolib_get_foley_data_pointer(audiolib_foley_hash_table_get(name_crc, nullptr));
}

// 0x00486920 (:930)
audiolib_foley_data* audiolib_internal_foley_change_cue_id(unsigned int name_crc, int cue_id)
{
    audiolib_foley_hash_table_set(name_crc, cue_id);
    return audiolib_get_foley_data_pointer(cue_id);
}

// 0x00486940 (:936)
audiolib_foley_data* audiolib_internal_foley_get_cue_by_index(int index)
{
    if (Audiolib_foley_list.total_cues != 0 && index >= 0 && index < Audiolib_foley_list.total_cues)
    {
        audiolib_foley_data* foley = Audiolib_foley_list.foley + index;
        return foley->in_use ? foley : nullptr;
    }
    return nullptr;
}

// 0x00486980 (:950)
audiolib_variation_info* audiolib_internal_get_variation_pointer(audiolib_foley_data* foley, int variant)
{
    if (foley == nullptr)
    {
        return nullptr;
    }
    if (variant < 0)
    {
        variant = 0;
    }
    // One variation is stored in place of the list pointer.
    if (foley->num_variants > 1)
    {
        return foley->variation_list + variant;
    }
    return &foley->one_variation;
}

// 0x004869B0 (:967)
audiolib_variation_info* audiolib_internal_get_variation_pointer(audiolib_play_data* play, int variant)
{
    if (play == nullptr)
    {
        return nullptr;
    }
    if (variant < 0)
    {
        variant = play->curr_variant;
    }
    return audiolib_internal_get_variation_pointer(&play->foley_data, variant);
}

// 0x004869F0 (:978)
audiolib_foley_data* audiolib_internal_find_or_add_foley(int index, unsigned int foley_name_crc, unsigned int encoded_offset, bool must_add)
{
    if (index >= Audiolib_foley_list.total_cues)
    {
        return nullptr;
    }
    if (audiolib_get_foley_data_pointer(audiolib_foley_hash_table_get(foley_name_crc, nullptr)) != nullptr)
    {
        // Known already: the name moves to this index (the old entry stays as it is).
        audiolib_foley_data* foley = Audiolib_foley_list.foley + index;
        foley->in_use = true;
        const int cue_id = audiolib_foley_make_cue_id(foley);
        foley->name_crc = foley_name_crc;
        audiolib_internal_foley_change_cue_id(foley_name_crc, cue_id);
        return foley;
    }
    audiolib_foley_data* foley = Audiolib_foley_list.foley + index;
    if (foley->in_use)
    {
        // The slot is taken by another name.
        return must_add ? nullptr : foley;
    }
    audiolib_foley_clear(foley);
    if (!audiolib_foley_hash_table_insert(foley_name_crc, audiolib_foley_make_cue_id(foley), encoded_offset))
    {
        return nullptr;
    }
    foley->in_use = true;
    foley->name_crc = foley_name_crc;
    return foley;
}

// 0x00486AF0 (:1012)
audiolib_foley_data* audiolib_internal_find_or_add_foley(int index, const char* name, unsigned int encoded_offset, bool must_add)
{
    return audiolib_internal_find_or_add_foley(index, crc_str(name, 0), encoded_offset, must_add);
}

// 0x00486B10 (:1025)
short audiolib_internal_get_category_id(unsigned int name_crc)
{
    if (Audiolib_category_list.count == 0)
    {
        return -1;
    }
    return (short)audiolib_foley_hash_table_get(name_crc, nullptr);
}

// 0x00486B30 (:1034)
short audiolib_internal_get_category_id(const char* category_name)
{
    if (Audiolib_category_list.count == 0 || category_name == nullptr)
    {
        return -1;
    }
    return audiolib_internal_get_category_id(crc_str(category_name, 0));
}

// The category id checks below accept count itself (original: one past the end).

// 0x00486B70 (:1043)
audiolib_result audiolib_internal_get_category_mute(short category_id, bool* mute)
{
    if (Audiolib_category_list.count == 0)
    {
        return AUDIOLIB_ERR_NOT_INITED;
    }
    if (mute == nullptr)
    {
        return AUDIOLIB_ERR_BAD_PARAM;
    }
    Audiolib_category_cs.enter();
    if (category_id != -1 && category_id <= Audiolib_category_list.count)
    {
        *mute = Audiolib_category_list.category[category_id].mute;
        Audiolib_category_cs.leave();
        return AUDIOLIB_ERR_NONE;
    }
    Audiolib_category_cs.leave();
    return AUDIOLIB_ERR_BAD_CATEGORY_ID;
}

// 0x00486BE0 (:1056)
audiolib_result audiolib_internal_set_category_mute(short category_id, bool mute)
{
    Audiolib_category_cs.enter();
    if (Audiolib_category_list.count == 0)
    {
        Audiolib_category_cs.leave();
        return AUDIOLIB_ERR_NOT_INITED;
    }
    if (category_id != -1 && category_id <= Audiolib_category_list.count)
    {
        Audiolib_category_list.category[category_id].mute = mute;
        Audiolib_category_cs.leave();
        return AUDIOLIB_ERR_NONE;
    }
    Audiolib_category_cs.leave();
    return AUDIOLIB_ERR_BAD_CATEGORY_ID;
}

// 0x00486C50 (:1069)
audiolib_result audiolib_internal_get_category_pause(short category_id, bool* pause)
{
    if (Audiolib_category_list.count == 0)
    {
        return AUDIOLIB_ERR_NOT_INITED;
    }
    if (pause == nullptr)
    {
        return AUDIOLIB_ERR_BAD_PARAM;
    }
    if (category_id != -1 && category_id <= Audiolib_category_list.count)
    {
        *pause = Audiolib_category_list.category[category_id].pause;
        return AUDIOLIB_ERR_NONE;
    }
    return AUDIOLIB_ERR_BAD_CATEGORY_ID;
}

// 0x00486CA0 (:1085)
audiolib_result audiolib_internal_set_category_pause(short category_id, bool pause)
{
    if (Audiolib_category_list.count == 0)
    {
        return AUDIOLIB_ERR_NOT_INITED;
    }
    if (category_id != -1 && category_id <= Audiolib_category_list.count)
    {
        Audiolib_category_cs.enter();
        Audiolib_category_list.category[category_id].pause = pause;
        Audiolib_category_cs.leave();
        return AUDIOLIB_ERR_NONE;
    }
    return AUDIOLIB_ERR_BAD_CATEGORY_ID;
}

// 0x00486D00 (:1098)
audiolib_result audiolib_internal_get_category_volume(short category_id, float* volume, bool include_ducking, audiolib_play_data* play)
{
    if (Audiolib_category_list.count == 0)
    {
        return AUDIOLIB_ERR_NOT_INITED;
    }
    if (volume == nullptr)
    {
        return AUDIOLIB_ERR_BAD_PARAM;
    }
    if (category_id == -1 || category_id > Audiolib_category_list.count)
    {
        return AUDIOLIB_ERR_BAD_CATEGORY_ID;
    }
    const audiolib_category_data& category = Audiolib_category_list.category[category_id];
    float result = category.curr_volume;
    // Ducked when ducking is on for the category and the duck volume scalar (index 2) is above 0.
    if (include_ducking && Audiolib_state.ducking && category.ducking && Audiolib_state.volume_scalars[2] > 0.0f)
    {
        if (play != nullptr && play->do_not_duck)
        {
            play->ducking = false;
        }
        else
        {
            result = Audiolib_state.curr_duck_volume * result;
            if (play != nullptr)
            {
                play->ducking = true;
            }
        }
    }
    else if (play != nullptr)
    {
        play->ducking = false;
    }
    *volume = result;
    return AUDIOLIB_ERR_NONE;
}

// 0x00486DE0 (:1138)
audiolib_result audiolib_internal_set_category_volume(short category_id, float volume)
{
    if (Audiolib_category_list.count == 0)
    {
        return AUDIOLIB_ERR_NOT_INITED;
    }
    if (category_id != -1 && category_id <= Audiolib_category_list.count)
    {
        Audiolib_category_cs.enter();
        Audiolib_category_list.category[category_id].dest_volume = volume;
        // A 750 ms fade.
        Audiolib_category_list.category[category_id].fade_time = timer_get(TIMER_MILLISECONDS) + 750;
        Audiolib_category_cs.leave();
        return AUDIOLIB_ERR_NONE;
    }
    return AUDIOLIB_ERR_BAD_CATEGORY_ID;
}

// 0x0049A290 (:1154)
audiolib_result audiolib_internal_category_stop(short category_id)
{
    if (Audiolib_category_list.count == 0)
    {
        return AUDIOLIB_ERR_NOT_INITED;
    }
    if (category_id != -1 && category_id <= Audiolib_category_list.count)
    {
        audiolib_internal_playlist_stop_by_category(category_id);
        return AUDIOLIB_ERR_NONE;
    }
    return AUDIOLIB_ERR_BAD_CATEGORY_ID;
}

// 0x00486E60 (:1167)
bool audiolib_internal_ok_to_start_category_instance(short category_id)
{
    Audiolib_category_cs.enter();
    if (Audiolib_category_list.count != 0 && category_id != -1 && category_id <= Audiolib_category_list.count &&
        Audiolib_category_list.category[category_id].curr_count < Audiolib_category_list.category[category_id].max_simultaneous)
    {
        ++Audiolib_category_list.category[category_id].curr_count;
        Audiolib_category_cs.leave();
        return true;
    }
    Audiolib_category_cs.leave();
    return false;
}

// 0x00486EC0 (:1187)
bool audiolib_internal_category_instance_ended(short category_id)
{
    Audiolib_category_cs.enter();
    if (Audiolib_category_list.count != 0 && category_id != -1 && category_id <= Audiolib_category_list.count)
    {
        // Original: no underflow check (0 wraps to 255).
        --Audiolib_category_list.category[category_id].curr_count;
        Audiolib_category_cs.leave();
        return true;
    }
    Audiolib_category_cs.leave();
    return false;
}

// 0x00486F10 (:1204)
bool audiolib_category_is_merged_ok(short cat_id)
{
    Audiolib_category_cs.enter();
    if (Audiolib_category_list.count != 0 && cat_id != -1 && cat_id <= Audiolib_category_list.count)
    {
        Audiolib_category_cs.leave();
        return Audiolib_category_list.category[cat_id].merging_ok;
    }
    Audiolib_category_cs.leave();
    return false;
}

// 0x00486F70 (:1222)
audiolib_result audiolib_category_set_user_category_volumes(signed char id, float volume, bool do_it_now)
{
    for (int i = 0; i < Audiolib_category_list.count; ++i)
    {
        audiolib_category_data& category = Audiolib_category_list.category[i];
        if (category.user_volume_type == id)
        {
            category.dest_volume = volume;
            if (do_it_now)
            {
                category.curr_volume = volume;
            }
        }
    }
    return AUDIOLIB_ERR_NONE;
}

// One step of a fade towards dest, whose end is at fade_time: the distance left divided by the 10 ms steps left
// (1-100). Returns false when the fade is over (then current is not changed here).
static bool audiolib_category_fade_step(float* current, float dest, int fade_time, int time)
{
    if (fade_time <= 0 || time >= fade_time)
    {
        return false;
    }
    float steps = (float)abs(fade_time - time) / 100.0f;
    if (steps - 1.0f < 0.0f)
    {
        steps = 1.0f;
    }
    if (steps - 100.0f >= 0.0f)
    {
        steps = 100.0f;
    }
    const float step = fabsf(dest - *current) / steps;
    if (*current <= dest)
    {
        *current = *current + step;
    }
    else
    {
        *current = *current - step;
    }
    return fabsf(dest - *current) >= 0.001f;
}

// 0x00486FD0 (:1243)
bool audiolib_foley_category_process(void* user_data, int time, int time_step)
{
    (void)user_data;
    (void)time_step;
    Audiolib_category_cs.enter();
    if (Audiolib_state.ducking)
    {
        bool fading = false;
        if (Audiolib_state.dest_duck_volume != Audiolib_state.curr_duck_volume)
        {
            fading = audiolib_category_fade_step(&Audiolib_state.curr_duck_volume, Audiolib_state.dest_duck_volume, Audiolib_state.duck_fade_time, time);
            if (!fading)
            {
                Audiolib_state.curr_duck_volume = Audiolib_state.dest_duck_volume;
                Audiolib_state.duck_fade_time = 0;
            }
        }
        // A duck faded back to 1 (or never started fading) ends.
        if (!fading && Audiolib_state.duck_fade_time == 0 && Audiolib_state.curr_duck_volume == 1.0f)
        {
            for (int i = 0; i < Audiolib_category_list.count; ++i)
            {
                Audiolib_category_list.category[i].ducking = false;
            }
            Audiolib_state.duck_id = -1;
            Audiolib_state.ducking = false;
        }
    }
    for (int i = 0; i < Audiolib_category_list.count; ++i)
    {
        audiolib_category_data& category = Audiolib_category_list.category[i];
        if (category.curr_volume != category.dest_volume &&
            !audiolib_category_fade_step(&category.curr_volume, category.dest_volume, category.fade_time, time))
        {
            category.fade_time = 0;
            category.curr_volume = category.dest_volume;
        }
    }
    Audiolib_category_cs.leave();
    return false;
}

// 0x004872D0 (:1333)
audiolib_result audiolib_internal_category_duck(short category_id)
{
    if (Audiolib_category_list.count == 0)
    {
        return AUDIOLIB_ERR_NOT_INITED;
    }
    if (category_id == -1 || category_id > Audiolib_category_list.count)
    {
        return AUDIOLIB_ERR_BAD_CATEGORY_ID;
    }
    if (Audiolib_state.duck_id != category_id)
    {
        Audiolib_category_cs.enter();
        // Every other category ducks; system and Guerrilla radio never do.
        for (int i = 0; i < Audiolib_category_list.count; ++i)
        {
            Audiolib_category_list.category[i].ducking = i != category_id;
        }
        Audiolib_category_list.category[Audiolib_state.system_cat_id].ducking = false;
        Audiolib_category_list.category[Audiolib_state.gur_radio_cat_id].ducking = false;
        Audiolib_state.duck_id = category_id;
        if (!Audiolib_state.ducking)
        {
            Audiolib_state.curr_duck_volume = 1.0f;
        }
        Audiolib_state.dest_duck_volume = Audiolib_state.duck_volume;
        Audiolib_state.duck_fade_time = timer_get(TIMER_MILLISECONDS) + Audiolib_settings.duck_fade_time;
        Audiolib_state.ducking = true;
        Audiolib_category_cs.leave();
    }
    return AUDIOLIB_ERR_NONE;
}

// 0x004873E0 (:1375)
audiolib_result audiolib_internal_category_duck_off()
{
    if (Audiolib_category_list.count == 0)
    {
        return AUDIOLIB_ERR_NOT_INITED;
    }
    // Fades back to 1; audiolib_foley_category_process ends the duck there.
    if (Audiolib_state.ducking && Audiolib_state.dest_duck_volume != 1.0f)
    {
        Audiolib_state.dest_duck_volume = 1.0f;
        Audiolib_state.duck_fade_time = timer_get(TIMER_MILLISECONDS) + Audiolib_settings.duck_fade_time;
    }
    return AUDIOLIB_ERR_NONE;
}

// 0x00487440 (:1387)
bool audiolib_internal_category_get_duck(short category_id)
{
    return Audiolib_category_list.category[category_id].ducking;
}

// 0x00487460 (:1398)
bool audiolib_internal_category_affected_by_low_health(short category_id)
{
    if (Audiolib_category_list.count == 0)
    {
        return false;
    }
    // The unaffected categories are listed (10 slots).
    for (int i = 0; i < 10; ++i)
    {
        if (Audiolib_state.low_health_cat_ids[i] == category_id)
        {
            return false;
        }
    }
    return true;
}

// 0x00487490 (:1413)
bool audiolib_internal_category_affected_by_environ(short category_id)
{
    if (Audiolib_category_list.count == 0)
    {
        return false;
    }
    // Original: the read is after the critical section.
    Audiolib_category_cs.enter();
    Audiolib_category_cs.leave();
    return Audiolib_category_list.category[category_id].affected_by_environ;
}

// 0x004874D0 (:1422)
bool audiolib_internal_foley_get_pitch_min_max(audiolib_foley_data* foley_data, float* min_pitch, float* max_pitch)
{
    // Pitch in octaves: +-1 when the cue allows pitch changes.
    float min = 0.0f;
    float max = 0.0f;
    if (foley_data != nullptr && foley_data->pitch_changes_allowed)
    {
        min = -1.0f;
        max = 1.0f;
    }
    if (min_pitch != nullptr)
    {
        *min_pitch = min;
    }
    if (max_pitch != nullptr)
    {
        *max_pitch = max;
    }
    return true;
}

// 0x0049A2D0 (:1453)
bool audiolib_foley_category_system_init()
{
    memset(&Audiolib_foley_list, 0, sizeof(Audiolib_foley_list));
    return audiolib_thread_function_register(audiolib_foley_category_process, "audiolib_foley_cat_process", nullptr) == AUDIOLIB_ERR_NONE;
}

// 0x004A0980 (:1470)
void audiolib_foley_category_system_shutdown()
{
    audiolib_thread_function_unregister(audiolib_foley_category_process);
}

// 0x00487510 (:1475)
audiolib_foley_data* audiolib_parse_foley_find(unsigned int foley_name_crc)
{
    return audiolib_get_foley_data_pointer(audiolib_foley_hash_table_get(foley_name_crc, nullptr));
}

// 0x00487530 (:1485)
audiolib_foley_data* audiolib_parse_foley_find(const char* foley_name)
{
    return audiolib_get_foley_data_pointer(audiolib_foley_hash_table_get(crc_str(foley_name, 0), nullptr));
}

// 0x00487550 (:1502)
audiolib_result audiolib_unparse_foley_persona_data(audiolib_foley_bank_data* foley_bank, int cues_in_bank)
{
    // The bank's whole slot range is cleared, whatever was registered.
    audiolib_foley_data* foley = Audiolib_foley_list.foley + foley_bank->start_index;
    for (int i = 0; i < cues_in_bank; ++i, ++foley)
    {
        if (foley->name_crc != 0)
        {
            foley_hash_table_cs.enter();
            audiolib_foley_hash_table_unlink(foley->name_crc);
            foley_hash_table_cs.leave();
        }
        audiolib_foley_clear(foley);
    }
    return AUDIOLIB_ERR_NONE;
}

// 0x00487660 (:1526)
bool audiolib_internal_foley_dont_duck(unsigned int name_crc)
{
    return audiolib_foley_in_list(Audiolib_settings.dont_duck_foley_crcs, Audiolib_settings.num_dont_duck_foleys, name_crc);
}

// 0x00487690 (:1540)
bool audiolib_internal_foley_satellite(unsigned int name_crc)
{
    return audiolib_foley_in_list(Audiolib_settings.satellite_foley_crcs, Audiolib_settings.num_satellite_foleys, name_crc);
}

// The wave and sound banks of a cue (shared by both parsers): the wavebank by its type (the bank found decides
// streaming or not), the soundbank with the cue's XACT index for audiolib_get_waveIndex_from_soundBank.
static void audiolib_foley_find_banks(audiolib_foley_data* foley, unsigned int soundbank_name_crc, unsigned int wavebank_name_crc, unsigned short xact_soundbank_index)
{
    foley->wavebank_id = audiolib_foley_try_find_soundbank_id(wavebank_name_crc, (short)(foley->wavebank_type == 1 ? AUDIOLIB_WAVEBANK_STREAMING : AUDIOLIB_WAVEBANK));
    if (foley->wavebank_id != -1)
    {
        // Original: a stale id would be read through a null bank (try_find only returns current ids).
        const audiolib_soundbank_data* wavebank = audiolib_internal_soundbank_id_to_ptr(foley->wavebank_id, false);
        foley->wavebank_type = wavebank->bank_type == AUDIOLIB_WAVEBANK_STREAMING ? 1 : 0;
    }
    foley->soundbank_id = audiolib_foley_try_find_soundbank_id(soundbank_name_crc, AUDIOLIB_SOUNDBANK);
    if (foley->soundbank_id != -1)
    {
        foley->internal_cue_id = (int)(((unsigned int)xact_soundbank_index << 16) | (unsigned short)foley->soundbank_id);
    }
}

// The cue flags (VOL_FOLEY_FLAG_*) as foley bits.
static void audiolib_foley_set_flags(audiolib_foley_data* foley, unsigned char flags)
{
    foley->max_dist_override = (flags & VOL_FOLEY_FLAG_MAX_DIST_OVERRIDE) != 0;
    foley->always_2d = (flags & VOL_FOLEY_FLAG_ALWAYS_2D) != 0;
    foley->do_not_kill_when_pausing = (flags & VOL_FOLEY_FLAG_NO_PAUSE_KILL) != 0;
    foley->do_not_apply_occlusion = (flags & VOL_FOLEY_FLAG_NO_OCCLUSION) != 0;
}

// The end of each parsed cue: the settings' don't-duck and satellite lists, and 80 (0.8) for a zero volume.
static void audiolib_foley_finish(audiolib_foley_data* foley)
{
    foley->dont_duck = audiolib_foley_in_list(Audiolib_settings.dont_duck_foley_crcs, Audiolib_settings.num_dont_duck_foleys, foley->name_crc);
    foley->allow_satellite = audiolib_foley_in_list(Audiolib_settings.satellite_foley_crcs, Audiolib_settings.num_satellite_foleys, foley->name_crc);
    if ((double)(float)foley->initial_volume / 100.0 == 0.0)
    {
        foley->initial_volume = 80;
    }
}

// 0x0049A330 (:1554)
audiolib_result audiolib_parse_foley_persona_data(volition_foley_cue_header_persona_info* source_data, int start_index, int num_cues)
{
    for (int i = 0; i < num_cues; ++i, ++start_index)
    {
        volition_foley_cue_header_persona_info* cue = source_data + i;
        audiolib_foley_data* foley = audiolib_internal_find_or_add_foley(start_index, cue->name_crc, cue->name_offset, true);
        if (foley == nullptr)
        {
            return AUDIOLIB_ERR_BAD_INTERNAL;
        }
        my_foley_data = foley;
        persona_cue_info = cue;
        foley->internal_cue_id = -1;
        foley->category_id = audiolib_internal_get_category_id(cue->cat_name_crc);
        foley->wavebank_type = (cue->flags & VOL_FOLEY_FLAG_WAVEBANK_TYPE) != 0 ? 1 : 0;
        audiolib_foley_find_banks(foley, cue->soundbank_name_crc, cue->wavebank_name_crc, cue->xact_soundbank_index);
        // Priorities 0-11; any other leaves the cleared value.
        if (cue->priority >= 0 && cue->priority < 12)
        {
            foley->priority = cue->priority;
        }
        foley->initial_volume = (unsigned char)(int)((double)cue->initial_volume * 100.0);
        foley->min_max_index = cue->min_max_distance_index;
        audiolib_foley_set_flags(foley, cue->flags);
        foley->max_simultaneous = 0;
        if (cue->max_simultaneous < 16)
        {
            foley->max_simultaneous = cue->max_simultaneous;
        }
        foley->curr_simul = 0;
        foley->didnt_start_last_time = false;
        foley->dont_start_percentage = cue->dont_start_percentage;
        foley->input_channels = (unsigned int)cue->input_channels;
        foley->looping = cue->loop_count != 0;
        foley->pitch_changes_allowed = (cue->flags & VOL_FOLEY_FLAG_PITCH_CHANGE_OK) != 0;
        // A persona cue has one variation, stored in place.
        foley->num_variants = 1;
        foley->one_variation.play_time = cue->play_time;
        audiolib_foley_finish(foley);
    }
    return AUDIOLIB_ERR_NONE;
}

// 0x0049A610 (:1737)
audiolib_result audiolib_parse_foley_data(volition_foley_cue_header_output_info* source_data, int start_index, int num_cues)
{
    for (int i = 0; i < num_cues; ++i, ++start_index)
    {
        volition_foley_cue_header_output_info* cue = source_data + i;
        audiolib_foley_data* foley = audiolib_internal_find_or_add_foley(start_index, cue->name_crc, cue->name_offset, false);
        my_foley_data = foley;
        my_cue_info = cue;
        if (foley != nullptr)
        {
            foley->internal_cue_id = -1;
            foley->category_id = audiolib_internal_get_category_id(cue->cat_name_crc);
            foley->always_2d = (cue->flags & VOL_FOLEY_FLAG_ALWAYS_2D) != 0;
            foley->wavebank_type = (cue->flags & VOL_FOLEY_FLAG_WAVEBANK_TYPE) != 0 ? 1 : 0;
            audiolib_foley_find_banks(foley, cue->soundbank_name_crc, cue->wavebank_name_crc, cue->xact_soundbank_index);
            // Priorities 0-11; any other is 4 (AUDIOLIB_CUE_PRIORITY_MEDIUM).
            foley->priority = cue->priority >= 0 && cue->priority < 12 ? cue->priority : 4;
            foley->initial_volume = (unsigned char)(int)((double)cue->initial_volume * 100.0);
            foley->min_max_index = cue->min_max_distance_index;
            audiolib_foley_set_flags(foley, cue->flags);
            foley->max_simultaneous = 0;
            if (cue->max_simultaneous < 16)
            {
                foley->max_simultaneous = cue->max_simultaneous;
            }
            foley->curr_simul = 0;
            foley->didnt_start_last_time = false;
            foley->dont_start_percentage = cue->dont_start_percentage;
            foley->input_channels = (unsigned int)cue->input_channels;
            foley->looping = cue->loop_count != 0;
            foley->pitch_changes_allowed = (cue->flags & VOL_FOLEY_FLAG_PITCH_CHANGE_OK) != 0;
            foley->num_variants = cue->num_variants;
            // More than one variation: a run of the shared variation list (kept on a reload).
            if (cue->num_variants > 1 && foley->variation_list == nullptr)
            {
                foley->variation_list = Audiolib_foley_list.variations + Audiolib_foley_list.variation_count;
                Audiolib_foley_list.variation_count += cue->num_variants;
            }
            for (int variant = 0; variant < cue->num_variants; ++variant)
            {
                audiolib_internal_get_variation_pointer(foley, variant)->play_time = cue->variations[variant].play_time;
            }
        }
        // Original: a null foley (an index taken by another name) is dereferenced here; the retail data has none.
        audiolib_foley_finish(foley);
    }
    return AUDIOLIB_ERR_NONE;
}

// 0x0049AA00 (:1975)
audiolib_result audiolib_load_category_and_foley_data(bool reload)
{
    char path[256];
    strncpy(path, Audiolib_state.root_dir, sizeof(path));
    strncat(path, Audiolib_state.data_dir, sizeof(path));
    strncat(path, "foley.aud_pc", sizeof(path));
    cfile* file = cf_open(path, "rb", PLATFORM_PC, false);
    // Original: an empty file is left open.
    if (file == nullptr || cf_size(file) == 0)
    {
        return AUDIOLIB_ERR_FILE_OPEN;
    }
    volition_foley_file_header header;
    if (cf_read(&header, sizeof(header), file, false) != sizeof(header) || header.signature != 0x20444656 /* 'VFD ' */ ||
        header.platform != 0x20204350 /* 'PC  ' */ || header.version > 6 || header.category_info_start > header.total_file_size ||
        header.cue_info_start > header.total_file_size || header.string_data_start > header.total_file_size)
    {
        cf_close(file);
        return AUDIOLIB_ERR_FILE_READ;
    }
    // 20 spare variations; the cue list is the file's cues rounded down to a multiple of 395, plus 27,069 slots for the
    // persona banks (64 x 395 after the file's cues rounded up, then the fixed banks).
    const int total_variations = (int)header.num_variations - (int)header.num_cues_w_1_variation + 20;
    const int total_cues = ((int)header.num_cues - (int)header.num_cues % Persona_bank_slots) + 0x69BD;
    if (!audiolib_foley_hash_table_init((int)header.num_categories + total_variations + total_cues) ||
        !audiolib_internal_category_list_init(header.num_categories) || !audiolib_internal_foley_list_init(total_cues, total_variations))
    {
        return AUDIOLIB_ERR_MEM_ALLOC_FAILED;
    }
    mempool* pool = Audiolib_state.pool;
    const unsigned int strings_mark = pool->mark();
    Audiolib_state.encoded_strings = nullptr;
    Audiolib_state.encoded_string_size = 0;
    if (reload && header.string_data_size != 0)
    {
        // The string table is never loaded (the names are not used, SX0).
        cf_close(file);
        return AUDIOLIB_ERR_FILE_READ;
    }
    pool->restore_to_mark(strings_mark);
    Audiolib_state.encoded_strings = nullptr;
    Audiolib_state.encoded_string_size = 0;

    // The categories, read into temporary pool memory.
    unsigned int mark = pool->mark();
    void* category_data = pool->alloc(header.category_data_size, 0);
    if (category_data == nullptr || !cf_seek(file, header.category_info_start, CF_SEEK_SET))
    {
        cf_close(file);
        return AUDIOLIB_ERR_MEM_ALLOC_FAILED;
    }
    if (cf_read(category_data, header.category_data_size, file, false) != header.category_data_size)
    {
        cf_close(file);
        return AUDIOLIB_ERR_FILE_READ;
    }
    const volition_foley_category_header_info* category_info = (const volition_foley_category_header_info*)category_data;
    for (int i = 0; i < (int)header.num_categories; ++i, ++category_info)
    {
        audiolib_category_data* category = audiolib_internal_add_category(category_info->name_crc, category_info->name_offset);
        if (category == nullptr)
        {
            continue;
        }
        // File rolloff types 1 and 3 are 1 and 2; the rest 0.
        switch (category_info->rolloff_type)
        {
        case 1:
            category->rolloff_type = 1;
            break;
        case 3:
            category->rolloff_type = 2;
            break;
        default:
            category->rolloff_type = 0;
            break;
        }
        // User volume types 0-3 and 5; the rest 4.
        switch (category_info->user_volume_type)
        {
        case 0:
        case 1:
        case 2:
        case 3:
        case 5:
            category->user_volume_type = category_info->user_volume_type;
            break;
        default:
            category->user_volume_type = 4;
            break;
        }
        if ((category_info->flags & 1) != 0)
        {
            category->merging_ok = true;
        }
        if ((category_info->flags & 2) != 0)
        {
            category->affected_by_environ = true;
        }
        category->emitter_cone_angle = category_info->emitter_cone_angle;
        category->emitter_cone_min_scale = category_info->emitter_cone_min_scale;
        category->max_simultaneous = category_info->max_simultaneous;
    }
    pool->restore_to_mark(mark);

    // The cues, in chunks that fit the pool (halved until one fits).
    short remaining = (short)header.num_cues;
    short chunk = (short)header.num_cues;
    int index = 0;
    mark = pool->mark();
    while (chunk != 0)
    {
        const unsigned int chunk_size = chunk * sizeof(volition_foley_cue_header_output_info);
        volition_foley_cue_header_output_info* cues = nullptr;
        if (pool->can_alloc(chunk_size, 0))
        {
            cues = (volition_foley_cue_header_output_info*)pool->alloc(chunk_size, 0);
        }
        if (cues == nullptr)
        {
            chunk = (short)(chunk / 2);
            continue;
        }
        while (remaining > 0)
        {
            const short count = chunk < remaining ? chunk : remaining;
            if (!cf_seek(file, (short)index * (int)sizeof(volition_foley_cue_header_output_info) + header.cue_info_start, CF_SEEK_SET))
            {
                cf_close(file);
                return AUDIOLIB_ERR_MEM_ALLOC_FAILED;
            }
            const unsigned int bytes = count * sizeof(volition_foley_cue_header_output_info);
            if (cf_read(cues, bytes, file, false) != bytes)
            {
                cf_close(file);
                return AUDIOLIB_ERR_FILE_READ;
            }
            audiolib_parse_foley_data(cues, (short)index, count);
            remaining = (short)(remaining - count);
            index += (unsigned short)count;
        }
        // The persona banks after the file's cues, from the next multiple of 395 on.
        index += Persona_bank_slots - (short)index % Persona_bank_slots;
        short start = 0;
        for (int i = 0; i < 64; ++i)
        {
            start = (short)index;
            audiolib_foley_bank_data& bank = Audiolib_foley_list.persona_banks[i];
            bank.start_index = start;
            bank.persona_filename[0] = '\0';
            bank.source_data = nullptr;
            bank.start = Audiolib_foley_list.foley + start;
            bank.num_cues = 0;
            bank.in_use = false;
            index += Persona_bank_slots;
        }
        Audiolib_foley_list.persona_PCM01_bank.start_index = (short)index;
        Audiolib_foley_list.persona_PCM01_bank.start = Audiolib_foley_list.foley + Audiolib_foley_list.persona_PCM01_bank.start_index;
        Audiolib_foley_list.persona_OFFCAM_bank.start_index = (short)(start + Persona_bank_slots + Persona_PCM01_cues);
        Audiolib_foley_list.persona_OFFCAM_bank.start = Audiolib_foley_list.foley + Audiolib_foley_list.persona_OFFCAM_bank.start_index;
        Audiolib_foley_list.persona_RFC_bank.start_index = (short)(Audiolib_foley_list.persona_OFFCAM_bank.start_index + Persona_OFFCAM_cues);
        Audiolib_foley_list.persona_RFC_bank.start = Audiolib_foley_list.foley + Audiolib_foley_list.persona_RFC_bank.start_index;
        Audiolib_foley_list.persona_SAFE_bank.start_index = (short)(Audiolib_foley_list.persona_RFC_bank.start_index + Persona_RFC_cues);
        Audiolib_foley_list.persona_SAFE_bank.start = Audiolib_foley_list.foley + Audiolib_foley_list.persona_SAFE_bank.start_index;
        Audiolib_foley_list.persona_HANDBOOK_bank.start_index = (short)(Audiolib_foley_list.persona_SAFE_bank.start_index + Persona_SAFE_cues);
        Audiolib_foley_list.persona_HANDBOOK_bank.start = Audiolib_foley_list.foley + Audiolib_foley_list.persona_HANDBOOK_bank.start_index;
        Audiolib_foley_list.persona_TUTORIAL_bank.start_index = (short)(Audiolib_foley_list.persona_HANDBOOK_bank.start_index + Persona_HANDBOOK_cues);
        Audiolib_foley_list.persona_TUTORIAL_bank.start = Audiolib_foley_list.foley + Audiolib_foley_list.persona_TUTORIAL_bank.start_index;
        Audiolib_state.number_of_foleys = header.num_cues;
        Audiolib_state.number_of_categories = header.num_categories;
        Audiolib_state.number_of_variations = (int)header.num_variations - (int)header.num_cues_w_1_variation;
        // The fixed banks keep their source_data_size.
        audiolib_foley_bank_data* fixed_banks[6] = {
            &Audiolib_foley_list.persona_PCM01_bank, &Audiolib_foley_list.persona_OFFCAM_bank, &Audiolib_foley_list.persona_RFC_bank,
            &Audiolib_foley_list.persona_SAFE_bank, &Audiolib_foley_list.persona_HANDBOOK_bank, &Audiolib_foley_list.persona_TUTORIAL_bank,
        };
        for (audiolib_foley_bank_data* bank : fixed_banks)
        {
            bank->persona_filename[0] = '\0';
            bank->source_data = nullptr;
            bank->num_cues = 0;
            bank->in_use = false;
        }
        pool->restore_to_mark(mark);
        cf_close(file);
        return AUDIOLIB_ERR_NONE;
    }
    cf_close(file);
    return AUDIOLIB_ERR_FILE_READ;
}

// 0x004876C0 (:2393)
bool audiolib_internal_foley_get_name_by_cue_id(int cue_id, char* output, int output_size)
{
    if (cue_id == -1 || output == nullptr || output_size == 0)
    {
        return false;
    }
    audiolib_foley_data* foley = audiolib_get_foley_data_pointer(cue_id);
    if (foley == nullptr)
    {
        return false;
    }
    char name[64];
    if (!audiolib_string_library_decode_string(Audiolib_state.encoded_strings, foley->name_crc, name) || (int)strlen(name) >= output_size)
    {
        return false;
    }
    strcpy(output, name);
    return true;
}

// 0x0049B040 (:2413)
bool audiolib_internal_cue_is_valid(int internal_cue_id)
{
    // The soundbank id in the low half, the cue's index in the bank in the high half.
    audiolib_soundbank_data* bank = audiolib_internal_soundbank_id_to_ptr((short)internal_cue_id, true);
    if (bank == nullptr)
    {
        return false;
    }
    return audiolib_hw_cue_is_valid(bank, internal_cue_id >> 16) == AUDIOLIB_ERR_NONE;
}

// 0x00487780 (:2433)
audiolib_foley_data* audiolib_internal_foley_get_data_by_cue_id(int caller_cue_id)
{
    if (caller_cue_id == -1)
    {
        return nullptr;
    }
    return audiolib_get_foley_data_pointer(caller_cue_id);
}

// 0x004877A0 (:2445)
float audiolib_internal_get_foley_vol(int caller_cue_id)
{
    if (caller_cue_id != -1)
    {
        const audiolib_foley_data* foley = audiolib_get_foley_data_pointer(caller_cue_id);
        if (foley != nullptr)
        {
            return (float)((double)(float)foley->initial_volume / 100.0);
        }
    }
    return 0.0f;
}

// 0x004877E0 (:2458)
signed char audiolib_get_rolloff_curve_from_category_id(short cat_id)
{
    return Audiolib_category_list.category[cat_id].rolloff_type;
}

// 0x00487800 (:2464)
bool audiolib_internal_category_get_emitter_cone(short cat_id, float* min_angle, float* min_scale)
{
    if (min_angle == nullptr || min_scale == nullptr)
    {
        return false;
    }
    // Defaults: a 90 degree cone, 0.8 outside it.
    *min_angle = 90.0f;
    *min_scale = 0.8f;
    if (cat_id >= 0 && cat_id < Audiolib_category_list.count)
    {
        *min_angle = Audiolib_category_list.category[cat_id].emitter_cone_angle;
        *min_scale = Audiolib_category_list.category[cat_id].emitter_cone_min_scale;
        return true;
    }
    return false;
}

// Parses a persona file's cues into a fixed bank when they fit.
static bool audiolib_foley_bank_register_fixed(audiolib_foley_bank_data& bank, int max_cues, const char* filename, unsigned char* data, int data_size)
{
    const short num_cues = *(short*)(data + 10);
    if (num_cues >= max_cues + 1)
    {
        return false;
    }
    if (audiolib_parse_foley_persona_data((volition_foley_cue_header_persona_info*)(data + 0x10), bank.start_index, num_cues) != AUDIOLIB_ERR_NONE)
    {
        return false;
    }
    audiolib_foley_bank_set(bank, filename, data, data_size, num_cues);
    return true;
}

// 0x0049B0C0 (:2485)
audiolib_result audiolib_foley_bank_register(const char* filename, unsigned char* data, int data_size)
{
    if (!Audiolib_state.initialized)
    {
        return AUDIOLIB_ERR_NOT_INITED;
    }
    if (filename == nullptr || data == nullptr || data_size == 0)
    {
        return AUDIOLIB_ERR_BAD_PARAM;
    }
    // A persona file: volition_foley_persona_file_header (0x10), then the cues.
    const volition_foley_persona_file_header* header = (const volition_foley_persona_file_header*)data;
    if (data_size < header->total_file_size)
    {
        return AUDIOLIB_FILE_LOAD_STATUS_ERROR;
    }
    Audiolib_foley_list_cs.enter();
    bool registered = false;
    if (is_player_persona_name(filename))
    {
        registered = audiolib_foley_bank_register_fixed(Audiolib_foley_list.persona_PCM01_bank, Persona_PCM01_cues, filename, data, data_size);
    }
    else if (is_offcam_persona_name(filename))
    {
        registered = audiolib_foley_bank_register_fixed(Audiolib_foley_list.persona_OFFCAM_bank, Persona_OFFCAM_cues, filename, data, data_size);
    }
    else if (is_rfc_persona_name(filename))
    {
        registered = audiolib_foley_bank_register_fixed(Audiolib_foley_list.persona_RFC_bank, Persona_RFC_cues, filename, data, data_size);
    }
    else if (is_safe_persona_name(filename))
    {
        registered = audiolib_foley_bank_register_fixed(Audiolib_foley_list.persona_SAFE_bank, Persona_SAFE_cues, filename, data, data_size);
    }
    else if (strstr(filename, "HANDBOOK") != nullptr || strstr(filename, "handbook") != nullptr)
    {
        registered = audiolib_foley_bank_register_fixed(Audiolib_foley_list.persona_HANDBOOK_bank, Persona_HANDBOOK_cues, filename, data, data_size);
    }
    else if (strstr(filename, "TUTORIAL") != nullptr || strstr(filename, "tutorial") != nullptr)
    {
        registered = audiolib_foley_bank_register_fixed(Audiolib_foley_list.persona_TUTORIAL_bank, Persona_TUTORIAL_cues, filename, data, data_size);
    }
    else
    {
        // The first free persona bank that parses. Original: no size check (395 slots per bank).
        for (int i = 0; i < 64 && !registered; ++i)
        {
            audiolib_foley_bank_data& bank = Audiolib_foley_list.persona_banks[i];
            if (bank.source_data == nullptr &&
                audiolib_parse_foley_persona_data((volition_foley_cue_header_persona_info*)(data + 0x10), bank.start_index, header->num_cues) == AUDIOLIB_ERR_NONE)
            {
                audiolib_foley_bank_set(bank, filename, data, data_size, header->num_cues);
                registered = true;
            }
        }
    }
    Audiolib_foley_list_cs.leave();
    return registered ? AUDIOLIB_ERR_NONE : AUDIOLIB_ERR_NO_FOLEY_SLOTS;
}

// Clears a fixed persona bank's slots and its registration.
static void audiolib_foley_bank_unregister_fixed(audiolib_foley_bank_data& bank, int cues)
{
    audiolib_unparse_foley_persona_data(&bank, cues);
    audiolib_foley_bank_reset(bank);
}

// 0x00487870 (:2691)
audiolib_result audiolib_foley_bank_unregister(const char* filename)
{
    if (!Audiolib_state.initialized)
    {
        return AUDIOLIB_ERR_NOT_INITED;
    }
    if (filename == nullptr)
    {
        return AUDIOLIB_ERR_BAD_PARAM;
    }
    Audiolib_foley_list_cs.enter();
    audiolib_result result = AUDIOLIB_ERR_NONE;
    if (is_player_persona_name(filename))
    {
        audiolib_foley_bank_unregister_fixed(Audiolib_foley_list.persona_PCM01_bank, Persona_PCM01_cues);
    }
    else if (is_offcam_persona_name(filename))
    {
        audiolib_foley_bank_unregister_fixed(Audiolib_foley_list.persona_OFFCAM_bank, Persona_OFFCAM_cues);
    }
    else if (is_rfc_persona_name(filename))
    {
        audiolib_foley_bank_unregister_fixed(Audiolib_foley_list.persona_RFC_bank, Persona_RFC_cues);
    }
    else if (is_safe_persona_name(filename))
    {
        audiolib_foley_bank_unregister_fixed(Audiolib_foley_list.persona_SAFE_bank, Persona_SAFE_cues);
    }
    else if (strstr(filename, "HANDBOOK") != nullptr || strstr(filename, "handbook") != nullptr)
    {
        audiolib_foley_bank_unregister_fixed(Audiolib_foley_list.persona_HANDBOOK_bank, Persona_HANDBOOK_cues);
    }
    else if (strstr(filename, "TUTORIAL") != nullptr || strstr(filename, "tutorial") != nullptr)
    {
        audiolib_foley_bank_unregister_fixed(Audiolib_foley_list.persona_TUTORIAL_bank, Persona_TUTORIAL_cues);
    }
    else
    {
        result = AUDIOLIB_ERR_NO_FOLEY_SLOTS;
        for (int i = 0; i < 64; ++i)
        {
            audiolib_foley_bank_data& bank = Audiolib_foley_list.persona_banks[i];
            if (strcmp(bank.persona_filename, filename) == 0)
            {
                audiolib_foley_bank_unregister_fixed(bank, Persona_bank_slots);
                result = AUDIOLIB_ERR_NONE;
                break;
            }
        }
    }
    Audiolib_foley_list_cs.leave();
    return result;
}
