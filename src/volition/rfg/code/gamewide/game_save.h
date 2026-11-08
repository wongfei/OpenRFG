// Original: volition/rfg/code/gamewide/game_save.h
// Implementation: volition/rfg/code/gamewide/game_save_generic.cpp
//
// Save slots and the save system front end (game_save_generic.cpp). game_save_info describes one save slot (progress
// summary, date, slot, unlocks). game_save_process (decompiled) runs every main loop frame: until the save data is
// available it asks the local game session for it (unit4::LocalGameSession::getSaveData), then reads the profile flags
// and signals the ini reader thread. Loading and writing run on threads (game_save_load_thread / _write_thread).
// Placement of game_save_info and game_save_state inferred.
#pragma once

#include <stdint.h>

namespace unit4
{
    class SaveDataDescriptionInterface;
}

namespace keen
{
    namespace rfg
    {
        struct RfgSaveData;
    }
}

enum game_save_state
{
    GSS_NONE = 0,
    GSS_LOADING_CONTENT_CREATE = 1,
    GSS_LOADING_READ = 2,
    GSS_WAITING_ON_DELETE = 3,
    GSS_WAITING_FOR_DEVICE = 4,
    GSS_SAVING_CONTENT_CREATE = 5,
    GSS_SAVING_WRITE = 6,
    GSS_NOT_SIGNED_IN_ERROR = 7,
    GSS_SELECTING_MEDIA = 8,
    GSS_UPDATING_SAVE_INFO = 9,
    NUM_GAME_SAVE_STATE = 10,
};

// size 0xB
struct game_save_info_new_data
{
    struct
    {
        uint64_t get(); // 0x00919ED0 (.h:127)
        void set(uint64_t val); // 0x00919EE0 (.h:132)

        unsigned char raw_data[8]; // 0x00
    } hammers_unlocked_large; // 0x00
    struct
    {
        unsigned short get(); // 0x00919EF0 (.h:143)
        void set(unsigned short val); // 0x00919F00 (.h:148)

        unsigned char raw_data[2]; // 0x00
    } backpacks_unlocked; // 0x08
    unsigned char jetpack_unlock_level; // 0x0A
};

// size 0xBC
struct game_save_info
{
    int num_missions_completed; // 0x00
    int num_activities_completed; // 0x04
    int districts_liberated; // 0x08
    int territory_index; // 0x0C
    int district_index; // 0x10
    int day; // 0x14
    int month; // 0x18
    int year; // 0x1C
    int hours; // 0x20
    int minutes; // 0x24
    int seconds; // 0x28
    bool auto_save; // 0x2C
    bool used_slot; // 0x2D
    int dlc_id; // 0x30
    unsigned int save_slot_index; // 0x34
    unsigned char hammers_unlocked; // 0x38
    bool golden_hammer_desired; // 0x39
    game_save_info_new_data new_data; // 0x3A
    unsigned char padding[117]; // 0x45
};

// The save system's state (game_save_generic.cpp): the user's save data once the local game session has it (null
// before), and the ini file reader/writer thread's flags.
extern keen::rfg::RfgSaveData* Save_system_save_data; // 0x025338E4
extern bool Save_system_ini_reader_is_active; // 0x025338F1
// The saves of the original release that can be imported (filled by the ini thread); the profile is one more row.
extern bool Has_imported_any_save_data; // 0x025338F3
extern bool Can_import_profile_data; // 0x025338F2
extern unsigned int Num_imported_save_file_infos; // 0x025338F4
extern game_save_info Imported_save_file_infos[257]; // 0x02505528
extern unsigned int Imported_save_file_info_slots[514]; // 0x0252D2C0, 257 in rfg.exe (see the definition)

int game_save_delete(game_save_info* game); // 0x007C3570 (game_save_generic.cpp:201)
void game_save_import_profile_data(); // 0x007D99D0 (game_save_generic.cpp:342)
void game_save_import_save_slot(game_save_info* target, unsigned int imported_slot); // 0x007E9080 (game_save_generic.cpp:377)
bool game_save_is_writing_ini_file(); // 0x007C36A0 (game_save_generic.cpp:460)
void game_save_system_init(); // 0x007EEA20 (game_save_generic.cpp:469)
void game_save_process(); // 0x007C36B0 (game_save_generic.cpp:486)
bool game_save_op_completed(); // 0x007C36F0 (game_save_generic.cpp:512)
game_save_state game_save_get_state(); // 0x007C3710 (game_save_generic.cpp:522)
int game_save_get_list_buf_size(int max_list_size); // 0x007C3730 (game_save_generic.cpp:532)
void game_save_check_load_directory_cache(); // 0x007D0F40 (game_save_generic.cpp:537)
void game_save_clear_load_directory_cache(); // 0x007C3740 (game_save_generic.cpp:594)
int game_save_get_list(game_save_info* buf, int buf_size, int max_list_size, bool displayAllSavegames); // 0x007D1010 (game_save_generic.cpp:599)
bool game_save_is_autosave_present(); // 0x007C3750 (game_save_generic.cpp:629)
bool game_save_is_dlc_autosave_present(); // 0x007C3760 (game_save_generic.cpp:634)
game_save_info* game_save_get_autosave(game_save_info* buf, int buf_size, int max_list_size, int op, bool displayAllSavegames, int dlc_id); // 0x007D1160 (game_save_generic.cpp:640)
int game_save_write(game_save_info* game, void* data, int len); // 0x007D11C0 (game_save_generic.cpp:661)
int game_save_load(game_save_info* game, void* buf, void (*completion_callback)(void*, int)); // 0x007EFEB0 (game_save_generic.cpp:702)
bool game_save_is_saving_enbaled(); // 0x007D12A0 (game_save_generic.cpp:713)
void game_save_start_get_save_data(); // 0x007C3770 (game_save_generic.cpp:721)
bool game_save_has_save_data(); // 0x007C3780 (game_save_generic.cpp:726)
void game_save_clear_save_data(); // 0x007C37A0 (game_save_generic.cpp:735)
int game_save_load_options(); // 0x007EFEF0 (game_save_generic.cpp:741)
int game_save_write_options(); // 0x007D12F0 (game_save_generic.cpp:749)
int game_save_write_profile_data(); // 0x007D1380 (game_save_generic.cpp:759)
unit4::SaveDataDescriptionInterface* game_save_get_save_interface_for_game_session(); // 0x007C37B0 (game_save_generic.cpp:769)
