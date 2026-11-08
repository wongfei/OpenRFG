// Original: volition/rfg/code/objects/humans/player_meta_data.h
// Implementation: volition/rfg/code/objects/humans/player_meta_data.cpp
//
// Persistent player progress (player::meta_data): salvage, upgrades (upgrade_item per upgrade: level and availability
// bits), statistics. Placement of upgrade_item inferred.
#pragma once

#include <stdint.h>

struct cfile;
class district;
enum upgrade_definition_names;
class upgrade_level;

// size 0x8
struct upgrade_item
{
    unsigned char current_level; // 0x00
    unsigned short availability_bitfield; // 0x02
    unsigned short unlocked_notified_bitfield; // 0x04
    unsigned short new_notified_bitfield; // 0x06
};

// size 0x420
class player_meta_data
{
public:
    player_meta_data(); // 0x00AD8FD0 (.h:19)
    virtual ~player_meta_data(); // vtbl+0x00 0x00A05770 (.h:20)
    void reset(); // 0x00AD6580 (:162)
    void reset_upgrades(); // 0x00ACDCF0 (:190)
    void clear_unlock_upgrade_notify(); // 0x00A36400 (:232)
    void clear_new_upgrade_notify(int upgrade_id); // 0x00A36460 (:252)
    void load_meta_data(cfile* save_file); // 0x00A5E3B0 (:327)
    void save_meta_data(cfile* save_file); // 0x00A11A20 (:387)
    void add_salvage(int amount, bool picked_up); // 0x00A11AA0 (:416)
    int get_salvage() const; // 0x00A11AE0 (:438)
    void apply_salvage_death_penalty(); // 0x00A11AF0 (:449)
    int get_upgrade_value(unsigned int upgrade_id); // 0x00A11B00 (:455)
    bool is_next_upgrade_available(unsigned int upgrade_id); // 0x00A11B40 (:473)
    bool is_next_upgrade_newly_unlocked(unsigned int upgrade_id); // 0x00A11B70 (:495)
    bool is_next_upgrade_new_to_the_store(unsigned int upgrade_id); // 0x00A11BB0 (:509)
    const upgrade_level* get_current_upgrade_level(unsigned int upgrade_id); // 0x00A11BF0 (:523)
    const upgrade_level* get_next_upgrade_level(unsigned int upgrade_id); // 0x00A365C0 (:534)
    void unlock_upgrade(unsigned int upgrade_id, unsigned int level, bool force_purchase); // 0x00A11C20 (:559)
    int purchase_upgrade(unsigned int upgrade_id, bool only_if_free, bool force_purchase); // 0x00AC3BA0 (:586)
    void mining_destroyed(unsigned int mine_handle); // 0x00A11C90 (:821)
    void supply_crate_destroyed(unsigned int supply_crate_handle); // 0x00A11E00 (:854)
    void update_play_time(); // 0x00A11F90 (:892)
    int get_play_time() // 0x00710E10 (.h:52)
    {
        return play_time;
    }
    void set_last_death_time(); // 0x00A11FE0 (:902)
    int get_last_death_time(); // 0x00710E20 (.h:54)
    void add_unlocked_weapons_to_weapons_cabinet(); // 0x00A119D0 (:266)
    void set_current_district(district* current_district); // 0x00A364E0 (:310)
    void select_jetpack(); // 0x00A12140 (:1040)

    // 0x00 vfptr
private:
    friend class rfg_mover; // maybe_flag_restore reads last_death_time (inferred)

    int salvage; // 0x04
    int mining_count; // 0x08
    int supply_crate_count; // 0x0C
    unsigned int district_hash; // 0x10
    int district_time; // 0x14
    upgrade_item upgrades[128]; // 0x18
    int play_time; // 0x418
    int last_death_time; // 0x41C
};

int meta_data_get_last_upgrade_voice_line(); // 0x00A11FF0 (:907)
void meta_data_clear_last_upgrade_voice_line(); // 0x00A12000 (:912)
bool meta_data_purchased_nano_rifle(); // 0x00A12010 (:917)
bool meta_data_has_hammer_unlocked(unsigned int hammer_upgrade_id); // 0x00A36620 (:929)
void meta_data_unlock_hammer(unsigned int hammer_upgrade_id); // 0x00A366A0 (:951)
uint64_t meta_data_get_hammers_unlocked(); // 0x00A12050 (:958)
void meta_data_set_hammers_unlocked(uint64_t hammers_unlocked); // 0x00A12060 (:963)
int meta_data_get_hammer_upgrade_index(upgrade_definition_names upgrade_id); // 0x00A12080 (:968)
const char* meta_data_get_hammer_upgrade_weapon_name(upgrade_definition_names upgrade_id); // 0x00A120B0 (:981)
bool meta_data_has_backpack_unlocked(unsigned int backpack_upgrade_id); // 0x00A366E0 (:994)
void meta_data_unlock_backpack(unsigned int backpack_upgrade_id); // 0x00A36710 (:1002)
unsigned short meta_data_get_backpacks_unlocked(); // 0x00A120E0 (:1010)
void meta_data_set_backpacks_unlocked(unsigned short backpacks_unlocked); // 0x00A120F0 (:1015)
int meta_data_get_backpack_upgrade_index(unsigned int upgrade_id); // 0x00A12100 (:1020)
unsigned char meta_data_get_jetpack_upgrade_level(); // 0x00A12120 (:1030)
void meta_data_set_jetpack_upgrade_level(unsigned char jetpack_upgrade_level); // 0x00A12130 (:1035)
