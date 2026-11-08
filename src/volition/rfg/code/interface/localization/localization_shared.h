// Original: volition/rfg/code/interface/localization/localization_shared.h
// Implementation: volition/rfg/code/interface/localization/localization_shared.cpp
//
// Localized strings: the string tables of the selected language (menu, HUD, tool tips, ...), lookups by name and hash,
// date formats and the language change that refreshes the documents.
#pragma once

#include "keen/base/language.hpp"
#include "volition/vlib/code/util/hash_table.h"

enum localization_date_format
{
    DATE_FORMAT_NONE = -1,
    DATE_FORMAT_YYYY_MM_DD = 0,
    DATE_FORMAT_MM_DD_YYYY = 1,
    DATE_FORMAT_DD_MM_YYYY = 2,
    NUM_DATE_FORMATS = 3,
};

extern keen::LanguageId Preferred_text_language; // 0x0166312C
extern keen::LanguageId Preferred_audio_language; // 0x01663130
extern keen::LanguageId Current_language; // 0x01663170
extern keen::LanguageId Current_audio_language; // 0x01663174
extern localization_date_format Current_date_format; // 0x01663178
extern keen::LanguageId Default_language; // 0x0166317C
extern bool Localization_initialized; // 0x02C03518
extern bool s_localization_is_loading; // 0x01663023 (file static in the binary)
// UI.md UI6: file statics of rfg.exe's interface unity (localization_shared.cpp), external since UI6 (the game options).
extern keen::LanguageId s_supported_languages[12]; // 0x01663138
extern keen::LanguageId s_keen_system_language_id; // 0x01663134
extern bool s_langauge_has_localized_voices[12]; // 0x02C0350C
extern bool s_localization_is_audio_loading; // 0x02C03509
extern data_hash_table<unsigned int,wchar_t*,&hash_uint,8192,unsigned int> Localization_hash; // 0x02C7FC90

void localization_set_keen_system_language(keen::LanguageId lang_id); // 0x008890B0 (:287)
void localization_system_init(); // 0x008890C0 (:296)
bool localization_load_current_file(); // 0x008DE340 (:345)
bool localization_load_file(const char* filename, unsigned char* data, unsigned int data_size); // 0x008CEB50 (:390)
void localization_unload_current_file(); // 0x008ABAD0 (:444)
const wchar_t* localization_get_display_string_req(const char* id_name, const wchar_t* default_text); // 0x008DE4F0 (:455)
const wchar_t* localization_get_display_string_opt(const char* id_name); // 0x008CEBA0 (:468)
void localization_set_and_save_selected_language(keen::LanguageId text_lang, keen::LanguageId audio_lang); // 0x008F17D0 (:503)
void localization_set_language(keen::LanguageId language_id); // 0x008DE540 (:530)
void localization_set_audio_language(keen::LanguageId language_id); // 0x008ABB30 (:589)
void localization_try_finish_audio_language_change(); // 0x00889210 (:651)
bool localization_language_is_set(); // 0x00889260 (:679)
keen::LanguageId localization_get_language(); // 0x00889280 (:685)
keen::LanguageId localization_get_audio_language(); // 0x00889290 (:690)
bool localization_is_current_language_non_audio(); // 0x008892A0 (:695)
localization_date_format localization_get_date_format(); // 0x008892D0 (:709)
bool localization_get_score_reversed(); // 0x008892E0 (:714)
const char* localization_get_language_name(keen::LanguageId lang); // 0x008892F0 (:719)
unsigned int localization_get_id_hash(const char* tag_id); // 0x008CEBE0 (:724)
const wchar_t* localization_get_display_string_req(unsigned int tag_hash); // 0x008CEC30 (:737)
const wchar_t* localization_get_display_string_opt(unsigned int tag_hash); // 0x008BF970 (:748)
bool localization_does_tag_exist(const char* localize_tag); // 0x008CEC70 (:799)
bool localization_use_imperial_units_not_metric(); // 0x00889320 (:812)
bool localization_explosion_gibs_allowed(); // 0x00889330 (:825)
bool localization_allow_humans_on_fire(); // 0x00889340 (:830)
bool localization_allow_blood(); // 0x00889350 (:835)
bool localization_allow_ragdolls(); // 0x00889360 (:840)
bool localization_allow_pain_sounds(); // 0x00889370 (:845)
bool localization_allow_ragdoll_interaction(); // 0x00889390 (:856)
bool localization_allow_attaching_projectiles(); // 0x008893B0 (:866)
bool localization_is_loading(); // 0x008893D0 (:876)
bool localization_is_audio_loading(); // 0x008893E0 (:881)
void localization_set_loading(keen::LanguageId text_lang, keen::LanguageId audio_lang); // 0x008ABC10 (:886)
bool localization_is_language_selectable(keen::LanguageId l); // 0x008ABCB0 (:907)
bool localization_non_selectable_language_supported(); // 0x008893F0 (:913)
unsigned int localization_get_number_supported_languages(); // 0x00889400 (:919)
keen::LanguageId localization_get_supported_language(unsigned int index); // 0x00889410 (:924)
unsigned int localization_get_number_supported_audio_languages(); // 0x00889420 (:931)
keen::LanguageId localization_get_supported_audio_language(unsigned int index); // 0x00889470 (:946)
keen::LanguageId localization_query_system_language(); // 0x008894B0 (:968)
keen::LanguageId localization_auto_detect_language(); // 0x008894D0 (:973)
bool localization_is_language_supported(keen::LanguageId l); // 0x008894F0 (:985)
bool localization_is_language_non_audio(keen::LanguageId l); // 0x00889510 (:996)
unsigned int localization_generate_hash_from_key(const char* key); // 0x00889540 (:1009)
