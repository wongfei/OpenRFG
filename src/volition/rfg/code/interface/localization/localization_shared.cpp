// Original: volition/rfg/code/interface/localization/localization_shared.cpp
//
// The string tables, the languages, the date format and the content switches (orig_main.md OM5, UI.md UI4). rfg_ui
// compiles the file; the test programs, which do not link rfg_ui, have their stand-ins in tests/stubs.

#include "volition/rfg/code/interface/localization/localization_shared.h"

#include "keen/base/crc32.hpp"
#include "keen/base/string.hpp"
#include "keen/base/trace.hpp"
#include "volition/audio_lib/code/audiolib_api.h"
#include "volition/audio_lib/code/audiolib_foley_category.h"
#include "volition/interface_lib/code/vint_external.h"
#include "volition/rfg/code/gameplay/wrecking_crew/wrecking_crew.h"
#include "volition/rfg/code/gamewide/stats.h"
#include "volition/rfg/code/interface/ui_document_manager.h"
#include "volition/rfg/code/multi/multi_core/include/multi.h"
#include "volition/rfg/code/objects/humans/human_voice/human_voice.h"
#include "volition/rfg/code/rfg.h"
#include "volition/stream2/code/stream2.h"
#include "volition/vlib/code/cfile/cfile.h"
#include "volition/vlib/code/cfile/packfile.h"
#include "volition/vlib/code/memory/rfg_split_mempool.h"
#include "volition/vlib/code/util/string_pool.h"

// A file static of the interface unity build in rfg.exe, read by loading.cpp's boot screen too (orig_main.md OM3);
// external here. True until the string tables are loaded.
bool s_localization_is_loading = true; // 0x01663023

// The mission names' string pool (placement unknown; owned by the mission code, not ported).
extern string_pool Mission_name_string_pool; // 0x02248050

// ---- globals
keen::LanguageId Preferred_text_language = (keen::LanguageId)-1; // 0x0166312C
keen::LanguageId Preferred_audio_language = (keen::LanguageId)-1; // 0x01663130
keen::LanguageId Current_language = (keen::LanguageId)-1; // 0x01663170
keen::LanguageId Current_audio_language = (keen::LanguageId)-1; // 0x01663174
localization_date_format Current_date_format = DATE_FORMAT_NONE; // 0x01663178
keen::LanguageId Default_language = keen::LanguageId_En_Us; // 0x0166317C
bool Localization_initialized; // 0x02C03518
data_hash_table<unsigned int, wchar_t*, &hash_uint, 8192, unsigned int> Localization_hash(false); // 0x02C7FC90
// A file static of the interface unity build in rfg.exe (the radio's persona names are fixed up again after a
// language change); external here.
bool Radio_persona_names_fixed_up; // 0x02C07FEE

// ---- file-static globals
keen::LanguageId s_keen_system_language_id = keen::LanguageId_En_Us; // 0x01663134, localization_set_keen_system_language (file static in the binary; external since UI4)
// Magic number: 12 supported languages (text and audio).
keen::LanguageId s_supported_languages[12] = // 0x01663138 (file static in the binary; external since UI4)
{
    keen::LanguageId_En_Us, keen::LanguageId_De_De, keen::LanguageId_Fr_Fr, keen::LanguageId_It_It,
    keen::LanguageId_Es_Es, keen::LanguageId_Ja_Jp, keen::LanguageId_Ru_Ru, keen::LanguageId_Pl_Pl,
    keen::LanguageId_Cs_Cz, keen::LanguageId_Ar_Eg, keen::LanguageId_Ko_Kr, keen::LanguageId_Zh_Cn,
};
bool s_langauge_has_localized_voices[12]; // 0x02C0350C, set by localization_system_init (external since UI6)
bool s_localization_is_audio_loading; // 0x02C03509 (external since UI6)
static bool s_localization_finish_audio_language_switch; // 0x02C0350A
static bool Allow_humans_on_fire = true; // 0x01663168
static bool Allow_blood = true; // 0x01663169
static bool Allow_ragdolls = true; // 0x0166316A
static bool Allow_pain_sounds = true; // 0x0166316B
static bool Allow_ragdoll_interaction = true; // 0x0166316C
static bool Allow_attaching_projectiles = true; // 0x0166316D
static bool Explosion_allow_gibs; // 0x02C0350B

// Magic numbers: 12 supported languages, 0x31 language ids (LanguageId_Count), -1 for an unset language.
static const unsigned int Num_supported_languages = 12;
static const int Num_language_ids = 0x31;
static const keen::LanguageId Language_unset = (keen::LanguageId)-1;
// Magic number: the string table file's magic, then version 3.
static const unsigned int Localization_file_magic = 0xA84C7F73;
static const unsigned int Localization_file_version = 3;

static bool localization_language_valid(keen::LanguageId lang)
{
    return (int)lang < Num_language_ids && lang != Language_unset;
}

// The file name "<prefix><language><suffix>" in lower case with '_' for '-' (en-US: en_us).
static void localization_language_file_name(char* name, unsigned int size, const char* format, keen::LanguageId lang)
{
    keen::formatString(name, size, format, keen::getLanguageIdString(lang));
    keen::changeStringToLowercase(name);
    keen::replaceStringCharacters(name, '-', '_');
}

// 0x00889040 (:195)
// Day first for Europe, Latin America and the Arab countries, year first for Japan, Korea and China, else month first.
static localization_date_format localization_auto_detect_date_format()
{
    switch (s_keen_system_language_id)
    {
    case keen::LanguageId_En_En:
    case keen::LanguageId_De_De:
    case keen::LanguageId_Fr_Fr:
    case keen::LanguageId_It_It:
    case keen::LanguageId_Es_Es:
    case keen::LanguageId_Pl_Pl:
    case keen::LanguageId_Ru_Ru:
    case keen::LanguageId_Pt_Br:
    case keen::LanguageId_Pt_Pt:
    case keen::LanguageId_Cs_Cz:
    case keen::LanguageId_Es_Ar:
    case keen::LanguageId_Es_Bo:
    case keen::LanguageId_Es_Cl:
    case keen::LanguageId_Es_Co:
    case keen::LanguageId_Es_Cr:
    case keen::LanguageId_Es_Do:
    case keen::LanguageId_Es_Ec:
    case keen::LanguageId_Es_Gt:
    case keen::LanguageId_Es_Hn:
    case keen::LanguageId_Es_Mx:
    case keen::LanguageId_Es_Ni:
    case keen::LanguageId_Es_Pa:
    case keen::LanguageId_Es_Pe:
    case keen::LanguageId_Es_Pr:
    case keen::LanguageId_Es_Py:
    case keen::LanguageId_Es_Sv:
    case keen::LanguageId_Es_Uy:
    case keen::LanguageId_Es_Ve:
    case keen::LanguageId_Ar_Eg:
    case keen::LanguageId_Ar_Dz:
    case keen::LanguageId_Ar_Bh:
    case keen::LanguageId_Ar_Iq:
    case keen::LanguageId_Ar_Jo:
    case keen::LanguageId_Ar_Kw:
    case keen::LanguageId_Ar_Lb:
    case keen::LanguageId_Ar_Ly:
    case keen::LanguageId_Ar_Ma:
    case keen::LanguageId_Ar_Om:
    case keen::LanguageId_Ar_Qa:
    case keen::LanguageId_Ar_Sa:
    case keen::LanguageId_Ar_Sy:
    case keen::LanguageId_Ar_Tn:
    case keen::LanguageId_Ar_Ae:
    case keen::LanguageId_Ar_Ye:
        return DATE_FORMAT_DD_MM_YYYY;
    case keen::LanguageId_Ja_Jp:
    case keen::LanguageId_Ko_Kr:
    case keen::LanguageId_Zh_Cn:
    case keen::LanguageId_Zh_Tw:
        return DATE_FORMAT_YYYY_MM_DD;
    default:
        return DATE_FORMAT_MM_DD_YYYY;
    }
}

// 0x008890B0 (:287)
void localization_set_keen_system_language(keen::LanguageId lang_id)
{
    s_keen_system_language_id = lang_id;
}

// 0x008890C0 (:296)
// Which languages have voices ("/gamebuild\voices_<lang>.vpp_pc"; en-US always), the default language (en-US unless
// set) and the date format of the system language.
void localization_system_init()
{
    Default_language = Language_unset;
    game_enable_standard_access();
    s_langauge_has_localized_voices[0] = true;
    for (unsigned int i = 0; i < Num_supported_languages; i++)
    {
        char filename[260];
        keen::formatString(filename, sizeof(filename), "%s\\voices_%s%s", "/gamebuild", keen::getLanguageIdString(s_supported_languages[i]), ".vpp_pc");
        keen::changeStringToLowercase(filename);
        keen::replaceStringCharacters(filename, '-', '_');
        if (cf_exist(filename))
        {
            s_langauge_has_localized_voices[i] = true;
        }
    }
    game_disable_standard_access();
    if (!localization_language_valid(Default_language))
    {
        Default_language = keen::LanguageId_En_Us;
    }
    if (Current_date_format == DATE_FORMAT_NONE)
    {
        Current_date_format = localization_auto_detect_date_format();
    }
    Localization_initialized = true;
}

// 0x008DE340 (:345)
// "locatexts_<lang>.rfglocatext" of the current language (else of the default language) into Localization_mempool.
// Notes (decompiled behaviour, kept as is): the file is not closed when it is empty or a read fails, and
// s_localization_is_loading stays set on a failure.
bool localization_load_current_file()
{
    s_localization_is_loading = true;
    char filename[64];
    localization_language_file_name(filename, sizeof(filename), "locatexts_%s.rfglocatext", Current_language);
    if (!cf_exist(filename))
    {
        keen::debug::nativePrintArgs("[localization] Didn't find file '%s' for language %s, trying to fall back to language %s.\n", filename, keen::getLanguageIdString(Current_language), keen::getLanguageIdString(Default_language));
        localization_language_file_name(filename, sizeof(filename), "locatexts_%s.rfglocatext", Default_language);
    }
    cfile* p_cfile = cf_open(filename, "rb", PLATFORM_PC, false);
    if (p_cfile == nullptr)
    {
        return false;
    }
    unsigned int size = cf_size(p_cfile);
    if (size == 0)
    {
        return false;
    }
    const unsigned int pos = Localization_mempool.mark();
    unsigned char* data = (unsigned char*)Localization_mempool.alloc(size, 4);
    if (data == nullptr)
    {
        return false;
    }
    size = cf_read(data, size, p_cfile, false);
    if (size == 0 || cf_error(p_cfile) != CF_ERROR_NONE)
    {
        return false;
    }
    cf_close(p_cfile);
    if (localization_load_file(filename, data, size))
    {
        s_localization_is_loading = false;
        return true;
    }
    Localization_mempool.restore_to_mark(pos);
    return false;
}

// 0x008CEB50 (:390)
// After the magic, the version and the count: 12-byte records whose second and third words are the string's hash and
// the offset of its text in the file. The texts stay in the loaded data.
bool localization_load_file(const char*, unsigned char* data, unsigned int)
{
    const unsigned int* header = (const unsigned int*)data;
    if (header[0] != Localization_file_magic || header[1] != Localization_file_version)
    {
        return false;
    }
    const unsigned int* record = header;
    for (unsigned int count = header[2]; count != 0; count--)
    {
        wchar_t* text = (wchar_t*)(data + record[4]);
        Localization_hash.insert_key_data(record[3], text);
        record += 3;
    }
    return true;
}

// 0x008ABAD0 (:444)
// The hash table is emptied (its free list rebuilt, inlined) and the pool cleared.
void localization_unload_current_file()
{
    Localization_hash.clear();
    Localization_mempool.set_thread_ownership();
    Localization_mempool.clear();
}

// 0x008DE4F0 (:455)
const wchar_t* localization_get_display_string_req(const char* id_name, const wchar_t* default_text)
{
    if (id_name != nullptr)
    {
        const unsigned int key = keen::getCrc32LwrValue(id_name);
        if (key != 0 && !s_localization_is_loading)
        {
            wchar_t* text = nullptr;
            Localization_hash.get(key, text);
            if (text != nullptr)
            {
                return text;
            }
        }
    }
    return default_text;
}

// 0x008CEBA0 (:468)
const wchar_t* localization_get_display_string_opt(const char* id_name)
{
    if (id_name != nullptr)
    {
        const unsigned int key = keen::getCrc32LwrValue(id_name);
        if (key != 0 && !s_localization_is_loading)
        {
            wchar_t* text = nullptr;
            Localization_hash.get(key, text);
            return text;
        }
    }
    return nullptr;
}

// 0x008F17D0 (:503)
// Unset languages become the supported language closest to the system language.
void localization_set_and_save_selected_language(keen::LanguageId text_lang, keen::LanguageId audio_lang)
{
    Preferred_text_language = text_lang;
    Preferred_audio_language = audio_lang;
    if (!localization_language_valid(text_lang))
    {
        text_lang = keen::findSupportedLanguage(s_keen_system_language_id, s_supported_languages, Num_supported_languages);
    }
    if (!localization_language_valid(Preferred_audio_language))
    {
        audio_lang = keen::findSupportedLanguage(s_keen_system_language_id, s_supported_languages, Num_supported_languages);
    }
    localization_set_language(text_lang);
    localization_set_audio_language(audio_lang);
}

// 0x008DE540 (:530)
// A new text language (the default one when it is not supported): the fonts are reloaded, then the string table.
// Notes (decompiled behaviour, kept as is): s_localization_is_loading is set before the checks, so it stays set when
// localization is not initialized or the language is invalid.
void localization_set_language(keen::LanguageId language_id)
{
    if (language_id == Current_language)
    {
        return;
    }
    s_localization_is_loading = true;
    if (!Localization_initialized || (int)language_id >= Num_language_ids || language_id == Language_unset)
    {
        return;
    }
    Current_language = localization_is_language_supported(language_id) ? language_id : Default_language;
    ui_initialize_fonts();
    localization_unload_current_file();
    localization_load_current_file();
    stats_reload_display_texts();
    s_localization_is_loading = false;
    vint_refresh_all_strings();
    Radio_persona_names_fixed_up = false;
}

// 0x008ABB30 (:589)
// A new audio language (the default one when it has no voices). Changing it after it was set reloads the voices and
// their packfiles; localization_try_finish_audio_language_change ends the switch.
void localization_set_audio_language(keen::LanguageId language_id)
{
    const keen::LanguageId previous_language = Current_audio_language;
    unsigned int i = 0;
    for (; i < Num_supported_languages; i++)
    {
        if (s_supported_languages[i] == language_id)
        {
            break;
        }
    }
    if (i == Num_supported_languages || !s_langauge_has_localized_voices[i])
    {
        language_id = Default_language;
    }
    if (language_id != Current_audio_language)
    {
        Current_audio_language = language_id;
        audiolib_set_game_language_hack(language_id);
        if (localization_language_valid(previous_language) && human_voice_initialized())
        {
            s_localization_finish_audio_language_switch = false;
            s_localization_is_audio_loading = true;
            Human_voice_always_loaded_pool.set_thread_ownership();
            Permanent_mempool.set_thread_ownership();
            Mission_name_string_pool.set_thread_ownership();
            Permanent_mempool.unlock();
            stream2_set_thread_ownership();
            packfile_set_thread_ownership();
            human_voice_system_shutdown();
            audiolib_voices_shutdown();
            rfg_reload_packfiles_for_audio_language_switch(previous_language);
            audiolib_voices_init(Current_audio_language);
            human_voice_system_init();
            Permanent_mempool.lock();
        }
    }
    s_localization_finish_audio_language_switch = true;
}

// 0x00889210 (:651)
void localization_try_finish_audio_language_change()
{
    if (s_localization_is_audio_loading && s_localization_finish_audio_language_switch)
    {
        Human_voice_always_loaded_pool.set_thread_ownership();
        Permanent_mempool.set_thread_ownership();
        Mission_name_string_pool.set_thread_ownership();
        stream2_set_thread_ownership();
        packfile_set_thread_ownership();
        s_localization_finish_audio_language_switch = false;
        s_localization_is_audio_loading = false;
    }
}

// 0x00889260 (:679)
bool localization_language_is_set()
{
    return localization_language_valid(Current_language);
}

// 0x00889280 (:685)
keen::LanguageId localization_get_language()
{
    return Current_language;
}

// 0x00889290 (:690)
keen::LanguageId localization_get_audio_language()
{
    return Current_audio_language;
}

// 0x008892A0 (:695)
bool localization_is_current_language_non_audio()
{
    return localization_is_language_non_audio(Current_language);
}

// 0x008892D0 (:709)
localization_date_format localization_get_date_format()
{
    return Current_date_format;
}

// 0x008892E0 (:714)
// Arabic shows scores right to left.
bool localization_get_score_reversed()
{
    return Current_language == keen::LanguageId_Ar_Eg;
}

// 0x008892F0 (:719)
const char* localization_get_language_name(keen::LanguageId lang)
{
    if (localization_language_valid(lang))
    {
        return keen::getLanguageIdString(lang);
    }
    return keen::getLanguageIdString(Current_language);
}

// 0x008CEBE0 (:724)
// The tag's hash when the string table has it, else 0.
unsigned int localization_get_id_hash(const char* tag_id)
{
    if (tag_id == nullptr)
    {
        return 0;
    }
    const unsigned int key = keen::getCrc32LwrValue(tag_id);
    if (key == 0 || s_localization_is_loading)
    {
        return 0;
    }
    wchar_t* text = nullptr;
    Localization_hash.get(key, text);
    return text != nullptr ? key : 0;
}

// 0x008CEC30 (:737)
const wchar_t* localization_get_display_string_req(unsigned int tag_hash)
{
    if (tag_hash != 0 && !s_localization_is_loading)
    {
        wchar_t* text = nullptr;
        Localization_hash.get(tag_hash, text);
        if (text != nullptr)
        {
            return text;
        }
    }
    return L"!!Unrecognized hash value";
}

// 0x008BF970 (:748)
const wchar_t* localization_get_display_string_opt(unsigned int tag_hash)
{
    if (tag_hash != 0 && !s_localization_is_loading)
    {
        wchar_t* text = nullptr;
        Localization_hash.get(tag_hash, text);
        return text;
    }
    return nullptr;
}

// 0x008CEC70 (:799)
bool localization_does_tag_exist(const char* localize_tag)
{
    if (localize_tag == nullptr)
    {
        return false;
    }
    const unsigned int key = keen::getCrc32LwrValue(localize_tag);
    if (key == 0 || s_localization_is_loading)
    {
        return false;
    }
    wchar_t* text = nullptr;
    Localization_hash.get(key, text);
    return text != nullptr;
}

// 0x00889320 (:812)
bool localization_use_imperial_units_not_metric()
{
    return s_keen_system_language_id == keen::LanguageId_En_Us;
}

// 0x00889330 (:825)
bool localization_explosion_gibs_allowed()
{
    return Explosion_allow_gibs;
}

// 0x00889340 (:830)
bool localization_allow_humans_on_fire()
{
    return Allow_humans_on_fire;
}

// 0x00889350 (:835)
bool localization_allow_blood()
{
    return Allow_blood;
}

// 0x00889360 (:840)
bool localization_allow_ragdolls()
{
    return Allow_ragdolls;
}

// 0x00889370 (:845)
// Always in multiplayer and Wrecking Crew.
bool localization_allow_pain_sounds()
{
    if (!Multiplayer && !wrecking_crew_is_active())
    {
        return Allow_pain_sounds;
    }
    return true;
}

// 0x00889390 (:856)
bool localization_allow_ragdoll_interaction()
{
    if (!Multiplayer && !wrecking_crew_is_active())
    {
        return Allow_ragdoll_interaction;
    }
    return true;
}

// 0x008893B0 (:866)
bool localization_allow_attaching_projectiles()
{
    if (!Multiplayer && !wrecking_crew_is_active())
    {
        return Allow_attaching_projectiles;
    }
    return true;
}

// 0x008893D0 (:876)
bool localization_is_loading()
{
    return s_localization_is_loading;
}

// 0x008893E0 (:881)
bool localization_is_audio_loading()
{
    return s_localization_is_audio_loading;
}

// 0x008ABC10 (:886)
// Marks the text and audio as loading when a language change is coming (an unset language means the system's).
void localization_set_loading(keen::LanguageId text_lang, keen::LanguageId audio_lang)
{
    bool text_changes = false;
    if (text_lang != Current_language)
    {
        text_changes = !(text_lang == Language_unset && Current_language == keen::findSupportedLanguage(s_keen_system_language_id, s_supported_languages, Num_supported_languages));
    }
    bool audio_changes = false;
    if (audio_lang != Current_audio_language)
    {
        audio_changes = !(audio_lang == Language_unset && Current_audio_language == keen::findSupportedLanguage(s_keen_system_language_id, s_supported_languages, Num_supported_languages));
    }
    if (!s_localization_is_loading && text_changes)
    {
        s_localization_is_loading = true;
    }
    if (!s_localization_is_audio_loading && audio_changes)
    {
        s_localization_is_audio_loading = true;
    }
}

// 0x008ABCB0 (:907)
bool localization_is_language_selectable(keen::LanguageId l)
{
    return localization_is_language_supported(l);
}

// 0x008893F0 (:913)
bool localization_non_selectable_language_supported()
{
    return false;
}

// 0x00889400 (:919)
unsigned int localization_get_number_supported_languages()
{
    return Num_supported_languages;
}

// 0x00889410 (:924)
keen::LanguageId localization_get_supported_language(unsigned int index)
{
    return s_supported_languages[index];
}

// 0x00889420 (:931)
unsigned int localization_get_number_supported_audio_languages()
{
    unsigned int count = 0;
    for (unsigned int i = 0; i < Num_supported_languages; i++)
    {
        if (s_langauge_has_localized_voices[i])
        {
            count++;
        }
    }
    return count;
}

// 0x00889470 (:946)
// The index-th language with voices, else the first supported language.
keen::LanguageId localization_get_supported_audio_language(unsigned int index)
{
    unsigned int found = 0;
    for (unsigned int i = 0; i < Num_supported_languages; i++)
    {
        if (s_langauge_has_localized_voices[i])
        {
            if (index == found)
            {
                return s_supported_languages[i];
            }
            found++;
        }
    }
    return s_supported_languages[0];
}

// 0x008894B0 (:968)
keen::LanguageId localization_query_system_language()
{
    return keen::findSupportedLanguage(s_keen_system_language_id, s_supported_languages, Num_supported_languages);
}

// 0x008894F0 (:985)
bool localization_is_language_supported(keen::LanguageId l)
{
    for (unsigned int i = 0; i < Num_supported_languages; i++)
    {
        if (s_supported_languages[i] == l)
        {
            return true;
        }
    }
    return false;
}

// 0x008894D0 (:973)
// The system language when supported, else the first supported language (keen::findSupportedLanguage).
keen::LanguageId localization_auto_detect_language()
{
    // Magic number: 12 supported languages.
    return keen::findSupportedLanguage(s_keen_system_language_id, s_supported_languages, 12);
}

// 0x00889510 (:996)
// An unsupported language counts as one without voices.
bool localization_is_language_non_audio(keen::LanguageId l)
{
    for (unsigned int i = 0; i < Num_supported_languages; i++)
    {
        if (s_supported_languages[i] == l)
        {
            return !s_langauge_has_localized_voices[i];
        }
    }
    return true;
}

// 0x00889540 (:1009): the case-insensitive CRC32 of a string table key.
unsigned int localization_generate_hash_from_key(const char* key)
{
    return keen::addCrc32LwrValue(0, key);
}
