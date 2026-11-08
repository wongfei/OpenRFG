// Original: volition/rfg/code/gameplay/upgrades.h
// Implementation: volition/rfg/code/gameplay/upgrades.cpp
//
// Upgrades: the upgrade definitions and levels read from the upgrade table, their availability and unlocks (liberation,
// activities, DLC), and the queries weapons and the player use (backpacks, rockets, arc welder, armor, sledgehammer).
// Placement of the enums and list items inferred.
#pragma once


struct weapon_info;
struct xml_element;

enum upgrade_definition_names
{
    UPGRADE_DEFINITION_JETPACK = 0,
    UPGRADE_DEFINITION_REMOTE_CHARGE_COUNT = 1,
    UPGRADE_DEFINITION_REMOTE_CHARGES = 2,
    UPGRADE_DEFINITION_ARC_WELDER = 3,
    UPGRADE_DEFINITION_ARC_WELDER_3RD_ARC = 4,
    UPGRADE_DEFINITION_ARC_WELDER_SMART = 5,
    UPGRADE_DEFINITION_GRINDER = 6,
    UPGRADE_DEFINITION_GRINDER_FASTER = 7,
    UPGRADE_DEFINITION_GRINDER_EXPLOSIVE = 8,
    UPGRADE_DEFINITION_PROX_MINE = 9,
    UPGRADE_DEFINITION_PROX_MINE_SMART = 10,
    UPGRADE_DEFINITION_ROCKET = 11,
    UPGRADE_DEFINITION_ROCKET_MULTI = 12,
    UPGRADE_DEFINITION_ROCKET_HEAT_SEEK = 13,
    UPGRADE_DEFINITION_THERMO_ROCKET = 14,
    UPGRADE_DEFINITION_NANO_RIFLE = 15,
    UPGRADE_DEFINITION_NANO_ENHANCER = 16,
    UPGRADE_DEFINITION_PERSONNEL = 17,
    UPGRADE_DEFINITION_RADIATION = 18,
    UPGRADE_DEFINITION_SLEDGEHAMMER_LEVEL = 19,
    UPGRADE_DEFINITION_SLEDGEHAMMER_LEVEL1 = 20,
    UPGRADE_DEFINITION_SLEDGEHAMMER_LEVEL2 = 21,
    UPGRADE_DEFINITION_GUNS_OF_THARSIS_COMPLETE = 22,
    UPGRADE_DEFINITION_GUNS_OF_THARSIS_UNLOCK1 = 23,
    UPGRADE_DEFINITION_GUNS_OF_THARSIS_UNLOCK2 = 24,
    UPGRADE_DEFINITION_PISTOL = 25,
    UPGRADE_DEFINITION_PLAYER_ARMOR = 26,
    UPGRADE_DEFINITION_EOS_REAL_MISSIONS = 27,
    UPGRADE_DEFINITION_ORE_BONUS = 28,
    UPGRADE_DEFINITION_SAFEHOUSE_WARP = 29,
    UPGRADE_DEFINITION_SALVAGE_COLLECTOR = 30,
    UPGRADE_DEFINITION_SINGULARITY_INCREASE = 31,
    NUM_OF_ORIGINAL_UPGRADE_DEFINITIONS = 32,
    UPGRADE_DEFINITION_RAIL_DRIVER = 32,
    UPGRADE_DEFINITION_SPIKER = 33,
    UPGRADE_DEFINITION_GAUSS_RIFLE = 34,
    UPGRADE_DEFINITION_SHOTGUN = 35,
    UPGRADE_DEFINITION_GUTTER = 36,
    UPGRADE_DEFINITION_MISSILEPOD = 37,
    UPGRADE_DEFINITION_SUPER_GAUSS = 38,
    UPGRADE_DEFINITION_RECONSTRUCTOR = 39,
    NUM_OF_UPGRADE_DEFINITIONS_DLC = 39,
    UPGRADE_DEFINITION_SLEDGEHAMMER_LEVEL_GOLD = 40,
    UPGRADE_DEFINITION_SLEDGEHAMMER_LEVEL3 = 41,
    UPGRADE_DEFINITION_SLEDGEHAMMER_LEVEL4 = 42,
    UPGRADE_DEFINITION_SLEDGEHAMMER_LEVEL5 = 43,
    UPGRADE_DEFINITION_SLEDGEHAMMER_LEVEL6 = 44,
    UPGRADE_DEFINITION_SLEDGEHAMMER_LEVEL7 = 45,
    UPGRADE_DEFINITION_SLEDGEHAMMER_LEVEL8 = 46,
    UPGRADE_DEFINITION_SLEDGEHAMMER_LEVEL9 = 47,
    UPGRADE_DEFINITION_SLEDGEHAMMER_LEVEL10 = 48,
    UPGRADE_DEFINITION_SLEDGEHAMMER_LEVEL11 = 49,
    UPGRADE_DEFINITION_SLEDGEHAMMER_LEVEL12 = 50,
    UPGRADE_DEFINITION_SLEDGEHAMMER_LEVEL13 = 51,
    UPGRADE_DEFINITION_SLEDGEHAMMER_LEVEL14 = 52,
    UPGRADE_DEFINITION_BACKPACK_THRUST = 53,
    UPGRADE_DEFINITION_BACKPACK_FLEETFOOT = 54,
    UPGRADE_DEFINITION_BACKPACK_STEALTH = 55,
    UPGRADE_DEFINITION_BACKPACK_VISION = 56,
    UPGRADE_DEFINITION_BACKPACK_COMMANDO = 57,
    UPGRADE_DEFINITION_BACKPACK_SHOCKWAVE = 58,
    UPGRADE_DEFINITION_BACKPACK_HEAL = 59,
    UPGRADE_DEFINITION_BACKPACK_RHINO = 60,
    UPGRADE_DEFINITION_BACKPACK_TREMOR = 61,
    NUM_OF_UPGRADE_DEFINITIONS = 62,
    NUM_OF_RESERVED_UPGRADE_DEFINITIONS = 128,
};

enum upgrade_types
{
    UPGRADE_TYPE_GENERAL = 0,
    UPGRADE_TYPE_VEHICLE = 1,
    UPGRADE_TYPE_WEAPON = 2,
    UPGRADE_TYPE_WEAPON_UPGRADE = 3,
    NUM_UPGRADE_TYPES = 4,
};

enum upgrade_node_types
{
    UPGRADE_NODE_WEAPONS = 0,
    UPGRADE_NODE_VEHICLES = 1,
    NUM_UPGRADE_NODE_TYPES = 2,
};

enum upgrade_unlock_actions
{
    UPGRADE_WAS_UNLOCKED = 0,
    UPGRADE_WAS_PURCHASED = 1,
    NUM_UPGRADE_UNLOCK_ACTIONS = 2,
};

enum upgrade101
{
    UPGR101_INIT = 0,
    UPGR101_REQUESTED = 1,
    UPGR101_RECEIVED = 2,
    UPGR101_WORKING = 3,
};

// size 0x20
class upgrade_level
{
public:
    const unsigned short get_cost(unsigned int upgrade_id, bool ignore_cheats) const; // 0x00775840 (:492)
    void set_cost(unsigned short new_cost); // 0x00753D80 (.h:146)

    unsigned int name_crc; // 0x00
    unsigned int description_crc; // 0x04
    unsigned int unlock_upgrade_id; // 0x08
    unsigned char value; // 0x0C
    unsigned char unlock_action; // 0x0D
    unsigned char unlock_upgrade_level; // 0x0E
    unsigned int activity_count; // 0x10
    bool show_locked; // 0x14
    const char* unlock_district_name; // 0x18
private:
    unsigned short cost; // 0x1C
};

// size 0x228
class upgrade_definition_data
{
public:
    upgrade_definition_data(); // 0x00768C80 (.h:156)
    void reset(); // 0x0075F500 (:537)
    void set_id(unsigned int index); // 0x0075F530 (:553)
    bool load(xml_element* xml_root); // 0x00775940 (:566)
    unsigned short get_default_availability() const; // 0x00A05740 (.h:170)
    const upgrade_level* get_upgrade_level(unsigned int level_index) const; // 0x0075F550 (:717)
    const unsigned int get_max_level() const; // 0x00A05750 (.h:172)
    const unsigned int get_id() const; // 0x00753D90 (.h:173)
    const unsigned char get_upgrade_type() const; // 0x00753DA0 (.h:174)
    const weapon_info* get_weapon_info() const; // 0x00753DB0 (.h:175)
    const unsigned int get_name_crc() const; // 0x0074F380 (.h:176)
    const unsigned int get_post_purchase_tool_tip_crc() const; // 0x0083F5D0 (.h:177)
    const char* get_icon_name() const; // 0x0083F5E0 (.h:178)
    const char* get_blueprint_name() const; // 0x0083F5F0 (.h:179)
    const int get_voice_line() const; // 0x00A05760 (.h:180)
    void print_data(); // 0x0075F570 (:725)
    bool always_show() const; // 0x00753DC0 (.h:184)

private:
    unsigned short availability_bitfield; // 0x00
    unsigned short show_locked_bitfield; // 0x02
    unsigned char upgrade_type; // 0x04
    unsigned char num_levels; // 0x05
    unsigned int upgrade_name_crc; // 0x08
    unsigned int upgrade_id; // 0x0C
    int voice_line; // 0x10
    unsigned int post_purchase_tool_tip_crc; // 0x14
    upgrade_level levels[16]; // 0x18
    const weapon_info* w_info; // 0x218
    const char* icon_name; // 0x21C
    const char* blueprint_name; // 0x220
    bool always_show_upgrade; // 0x224
};

// size 0x1C
struct upgrade_list_item
{
    upgrade_definition_names index; // 0x00
    unsigned int name_crc; // 0x04
    unsigned int description_crc; // 0x08
    const char* icon_name; // 0x0C
    const char* blueprint_name; // 0x10
    bool locked; // 0x14
    bool available; // 0x15
    bool is_new; // 0x16
    int price; // 0x18
};

// size 0x8
struct upgrade_index_type
{
    unsigned int name_crc; // 0x00
    upgrade_definition_names index; // 0x04
};

// size 0xC
struct hammer_upgrade_data
{
    upgrade_definition_names upgrade_id; // 0x00
    const char* weapon_name; // 0x04
    bool unlocked_with_golden_hammer; // 0x08
};

const upgrade_definition_data* upgrade_get_data_by_index(unsigned int upgrade_id); // 0x0075F600 (:759)
const upgrade_definition_data* upgrade_get_data_by_name(unsigned int upgrade_name_crc); // 0x00775D20 (:769)
void upgrade_system_init(int dlc_id); // 0x00790DB0 (:778)
void upgrade_level_init(); // 0x0075F620 (:795)
unsigned int upgrades_get_handle(unsigned int upgrade_name_crc); // 0x0075F630 (:800)
upgrade_definition_names upgrade_get_backpack_type(); // 0x00775D70 (:812)
float upgrade_jetpack_charge_multiplier(); // 0x00775DD0 (:855)
int upgrade_get_multi_rockets(); // 0x00775E50 (:874)
bool upgrade_has_heat_seeking_rockets(); // 0x00775EA0 (:891)
bool upgrade_has_arc_welder_3rd_arc(); // 0x00775EF0 (:903)
bool upgrade_has_arc_welder_smart(); // 0x00775F30 (:920)
int upgrade_get_max_charges(); // 0x00775F70 (:937)
bool upgrade_has_fast_grinder(); // 0x00775FC0 (:954)
bool upgrade_has_grinder_explosive(); // 0x00776000 (:966)
bool upgrade_has_smart_proximity_mines(); // 0x00776040 (:981)
bool upgrade_has_personnel_detector(); // 0x00776080 (:993)
bool upgrade_has_radiation_shield(); // 0x007760C0 (:1005)
int upgrade_get_sledgehammer_upgrade_level(); // 0x00776100 (:1017)
const char* upgrade_get_sledgehamer_weapon_name_from_upgrade_level(); // 0x00776170 (:1039)
bool upgrade_completed_guns_of_tharsis(); // 0x00776180 (:1045)
bool upgrade_weapon_has_extra_ammo(const weapon_info* w_info); // 0x007761C0 (:1057)
int upgrade_get_armor_level(); // 0x00776240 (:1080)
float upgrade_get_player_armor(); // 0x00776280 (:1095)
bool upgrade_show_eos_hidden_missions(); // 0x00776310 (:1128)
float upgrade_get_nano_enhancer(); // 0x00776350 (:1140)
bool upgrade_has_nano_rifle(); // 0x007763A0 (:1152)
bool upgrade_has_safehouse_warp(); // 0x007763E0 (:1164)
bool upgrade_has_salvage_collector(); // 0x00776420 (:1176)
bool upgrade_has_singularity_increase(); // 0x00776460 (:1188)
bool upgrade_has_ore_bonus(); // 0x007764A0 (:1200)
void upgrade_unlock_from_liberation(unsigned int district_handle); // 0x00790E20 (:1212)
bool upgrade_unlock_from_activities(); // 0x007764E0 (:1266)
bool upgrade_show_new_upgrades(bool from_mission, int num_activities_completed); // 0x0075F680 (:1333)
void upgrade_unlock_free_dlc_upgrades(); // 0x0075F750 (:1370)
