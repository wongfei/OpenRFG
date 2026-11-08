// Original: volition/audio_lib/code/audiolib_soundbank.h
// Implementation: volition/audio_lib/code/audiolib_soundbank.cpp
//
// Sound and wave banks: the bank list (from the soundbank and wavebank tables), load states, register/load/unload, the
// panic restore, and the DLC and voice banks. Placement of audiolib_soundbank_load_state inferred.
#pragma once

#include "volition/audio_lib/code/audiolib_enums.h"
#include "volition/audio_lib/code/audiolib_hardware_types.h"

class mempool;
struct xml_element;

enum audiolib_soundbank_load_state
{
    AUDIOLIB_SOUNDBANK_STATE_WAITING_FOR_SLOT = 0,
    AUDIOLIB_SOUNDBANK_STATE_LOADING = 1,
    AUDIOLIB_SOUNDBANK_STATE_READY = 2,
    AUDIOLIB_SOUNDBANK_STATE_UNLOADING = 3,
    AUDIOLIB_SOUNDBANK_STATE_LOAD_FAILED = 4,
    NUM_AUDIOLIB_SOUNDBANK_STATES = 5,
};

// size 0x140
struct audiolib_soundbank_data
{
    bool is_valid() const; // 0x00482FC0 (.h:79)
    void invalidate(); // 0x00482FD0 (.h:80)
    void drop(); // 0x0048A0B0 (:82)
    audiolib_soundbank_data(); // 0x00482FE0 (.h:86)

    unsigned int bank_name_crc; // 0x00
    char bank_name[64]; // 0x04
    char sub_dir[64]; // 0x44
    short index; // 0x84
    audiolib_hw_soundbank_id hw_bank_id; // 0x88
    char* bank_buffer; // 0x124
    short alignment; // 0x128
    int bank_size; // 0x12C
    short sector_read; // 0x130
    void* file_handle; // 0x134
    bool load_at_boot : 1; // 0x138:0
    bool is_loaded : 1; // 0x138:1
    bool valid : 1; // 0x138:2
    bool music : 1; // 0x138:3
    bool dlc : 1; // 0x138:4
    bool isVoiceBank : 1; // 0x138:5
    bool isVehicleBank : 1; // 0x138:6
    short bank_type : 4; // 0x13A:0
    short rolling_counter; // 0x13C
};

void audiolib_internal_panic_restore_all_soundbanks(); // 0x0049D140 (:92)
void audiolib_internal_panic_destroy_all_soundbanks(); // 0x00496510 (:106)
audiolib_soundbank_data* audiolib_internal_soundbank_id_to_ptr(short id, bool must_be_ready); // 0x004965D0 (:121)
audiolib_soundbank_load_state audiolib_internal_get_soundbank_state(audiolib_soundbank_data* bank); // 0x00496640 (:151)
void audiolib_debug_print_soundbank_status(); // 0x00496680 (:171)
bool audiolib_process_soundbank_list(void* user_data, int time, int time_step); // 0x0049D230 (:238)
void string_tolower(char* name); // 0x0048A0E0 (:334)
audiolib_result audiolib_internal_parse_soundbank(xml_element* bank, bool voice, bool dlc, mempool* pool); // 0x004A2050 (:342)
audiolib_result audiolib_parse_soundbank_xtbl(const char* filename, bool voice, bool dlc, mempool* pool); // 0x004A2140 (:381)
audiolib_result audiolib_internal_parse_wavebank(xml_element* bank, bool voice, bool dlc); // 0x004A2280 (:402)
audiolib_result audiolib_parse_wavebank_xtbl(const char* filename, bool voice, bool dlc); // 0x004A2440 (:487)
bool audiolib_internal_soundbank_system_init(); // 0x004A2670 (:512)
void audiolib_internal_soundbank_system_shutdown(); // 0x004A26D0 (:534)
bool audiolib_sound_bank_is_valid(short bank_id, bool is_registered); // 0x004968A0 (:565)
short audiolib_sound_bank_get_id(unsigned int bank_name_crc, short bank_type); // 0x004968F0 (:583)
short audiolib_sound_bank_get_id(const char* bank_name, short bank_type); // 0x00496B30 (:639)
short audiolib_sound_bank_get_id_from_load_state(const char* bank_name, short bank_type, int load_state); // 0x00496B50 (:646)
const char* audiolib_sound_bank_get_name_by_id(short bank_id); // 0x00496C40 (:665)
audiolib_result audiolib_sound_bank_get_filename_by_id(short bank_id, char* filename); // 0x00496CD0 (:681)
short audiolib_internal_soundbank_get_id_by_index(int bank_index); // 0x00492350 (:711)
int audiolib_internal_soundbank_get_count(); // 0x00496E40 (:726)
audiolib_result audiolib_sound_bank_register(short bank_id, char* data, int data_size); // 0x0049D410 (:737)
audiolib_result audiolib_sound_bank_unregister(short bank_id, bool wait_for_unload); // 0x004A8D00 (:804)
audiolib_result audiolib_sound_bank_prepare_for_unload(short bank_id); // 0x004A8880 (:843)
audiolib_result audiolib_sound_bank_ready_for_unload(short bank_id, bool* status); // 0x0049D4F0 (:872)
audiolib_result audiolib_sound_bank_load(const char* filename, const char* dir, short* bank_id, short bank_type, short sector_read, bool load_now, mempool* pool); // 0x0049D560 (:911)
audiolib_result audiolib_sound_bank_unload(short bank_id, bool free, bool wait_for_unload); // 0x004A8DF0 (:1118)
audiolib_result audiolib_internal_soundbank_get_cue_index(short bank_id, const char* cue_name, int* cue_index); // 0x00496E80 (:1156)
bool audiolib_internal_soundbank_is_loaded(short bank_id); // 0x00496F40 (:1189)
bool audiolib_internal_soundbank_is_bank_in_RAM(short wavebank_id); // 0x00496FC0 (:1212)
bool audiolib_sounbank_get_wavebank_name(int index, char** name); // 0x004923C0 (:1243)
void audiolib_soundbank_dlc_init(); // 0x00497040 (:1263)
void audiolib_soundbank_dlc_shutdown(); // 0x004A8EF0 (:1290)
void audiolib_soundbank_voice_init(); // 0x00497150 (:1328)
void audiolib_soundbank_voice_shutdown(); // 0x004A9060 (:1355)
