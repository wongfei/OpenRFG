// Original: volition/rfg/code/multi/multi_game/multi_game_settings.h
// Implementation: volition/rfg/code/multi/multi_game/multi_game_settings.cpp
//
// The multiplayer game settings: the levels, the game options (generic and per mode, read from option sets), the server
// options sent to the clients (Mp_server_options, friendly fire among them), DLC availability.
#pragma once

#include "volition/rfg/code/multi/multi_game/multi_gameplay_mode.h"
#include "volition/vlib/code/math/crc.h"

class mempool_base;
struct xml_element;

enum multi_rubberbanding_levels
{
    MRL_NONE = -1,
    MRL_DEFAULT = 0,
    MRL_LOW = 1,
    MRL_MEDIUM = 2,
    MRL_HIGH = 3,
    NUM_MULTI_RUBBERBANDING_LEVELS = 4,
};

enum multi_game_enemy_icon_visibilities
{
    MGEIV_NONE = 0,
    MGEIV_DELAYED = 1,
    MGEIV_DEFAULT = 2,
    MGEIV_ALWAYS = 3,
    NUM_MULTI_GAME_ENEMY_ICON_VISIBILITIES = 4,
};

enum multi_game_times_of_day
{
    MGTOD_RANDOM = 0,
    MGTOD_DEFAULT = 1,
    MGTOD_ALTERNATE = 2,
    NUM_MULTI_GAME_TIMES_OF_DAY = 3,
};

enum multi_game_starting_backpack_options
{
    MGSBO_NONE = 0,
    MGSBO_RANDOM = 1,
    MGSBO_SPECIFIC = 2,
    NUM_MULTI_GAME_STARTING_BACKPACK_OPTIONS = 3,
};

enum multi_game_backpack_spawn_options
{
    MGBSO_DEFAULT = 0,
    MGBSO_SHUFFLE = 1,
    MGBSO_NONE = 2,
    MGBSO_SPECIFIC = 3,
    NUM_MULTI_GAME_BACKPACK_SPAWN_OPTIONS = 4,
};

enum multi_game_weapon_respawn_speeds
{
    MGWRS_NORMAL = 0,
    MGWRS_FAST = 1,
    MGWRS_ALWAYS = 2,
    MGWRS_NONE = 3,
    MGWRS_SLOW = 4,
    NUM_MULTI_GAME_WEAPON_RESPAWN_SPEEDS = 5,
};

enum king_of_the_hill_multipliers
{
    KOTHM_NORMAL = 0,
    KOTHM_NONE = 1,
    KOTHM_ONLY = 2,
    NUM_KING_OF_THE_HULL_MULTIPLIERS = 3,
};

enum multi_game_lockable_weapons
{
    MGLW_PISTOL = 0,
    MGLW_ASSAULT_RIFLE = 1,
    MGLW_PEACEKEEPER = 2,
    MGLW_ENFORCER = 3,
    MGLW_SNOWBALL_GUN = 4,
    MGLW_SNIPER_RIFLE = 5,
    MGLW_RAIL_DRIVER = 6,
    MGLW_SING_BOMB = 7,
    MGLW_PROX_MINE = 8,
    MGLW_REMOTE_CHARGES = 9,
    MGLW_ROCKET_LAUNCHER = 10,
    MGLW_THERMOBARIC_ROCKET = 11,
    MGLW_NANO_RIFLE = 12,
    MGLW_GRINDER = 13,
    MGLW_ARC_WELDER = 14,
    MGLW_SHOTGUN = 15,
    NUM_MULTI_GAME_LOCKABLE_WEAPONS = 16,
};

enum multi_game_lockable_backpacks
{
    MGLB_JETPACK = 0,
    MGLB_THRUST = 1,
    MGLB_FLEETFOOT = 2,
    MGLB_STEALTH = 3,
    MGLB_VISION = 4,
    MGLB_FIREPOWER = 5,
    MGLB_CONCUSSION = 6,
    MGLB_HEAL = 7,
    MGLB_RHINO = 8,
    MGLB_TREMOR = 9,
    NUM_MULTI_GAME_LOCKABLE_BACKPACKS = 10,
};

enum multi_game_options
{
    MGO_GAME_MODE = 0,
    MGO_LEVEL = 1,
    MGO_SCORE_LIMIT = 2,
    MGO_TIME_LIMIT = 3,
    MGO_FRIENDLY_FIRE = 4,
    MGO_RUBBERBANDING_LEVEL = 5,
    MGO_ENEMY_ICONS = 6,
    MGO_TIME_OF_DAY = 7,
    MGO_RESPAWN_DELAY = 8,
    MGO_BACKPACKS = 9,
    MGO_STARTING_BACKPACK = 10,
    MGO_HEALTH_REGEN_FACTOR = 11,
    MGO_HEALTH_FACTOR = 12,
    MGO_MOVEMENT_FACTOR = 13,
    MGO_UNLIMITED_AMMO = 14,
    MGO_STARTING_WEAPON = 15,
    MGO_THIRD_WEAPON = 16,
    MGO_FOURTH_WEAPON = 17,
    MGO_WEAPON_RESPAWN_SPEED = 18,
    MGO_ANARCHY_LEADER_HIGHLIGHTING = 19,
    MGO_CTF_FLAG_RETURN_TIME = 20,
    MGO_CTF_FLAG_TOUCH_RETURN = 21,
    MGO_CTF_CARRIER_ICON_ALWAYS_ON = 22,
    MGO_SIEGE_DEFENSE_STARTING_BACKPACK = 23,
    MGO_SIEGE_DEFENSE_STARTING_WEAPON = 24,
    MGO_SIEGE_DEFENSE_THIRD_WEAPON = 25,
    MGO_SIEGE_DEFENSE_FOURTH_WEAPON = 26,
    MGO_DC_ALL_TARGET_MULTIPLIER = 27,
    MGO_BAGMAN_RETURN_TIME = 28,
    MGO_BAGMAN_KILL_BONUS = 29,
    MGO_TBAGMAN_RETURN_TIME = 30,
    MGO_TBAGMAN_KILL_BONUS = 31,
    MGO_DEMOLITION_ENGINEER_ICON = 32,
    MGO_LOCK_WEAPON_PISTOL = 33,
    MGO_LOCK_WEAPON_ASSAULT_RIFLE = 34,
    MGO_LOCK_WEAPON_PEACEKEEPER = 35,
    MGO_LOCK_WEAPON_ENFORCER = 36,
    MGO_LOCK_WEAPON_SNOWBALL_GUN = 37,
    MGO_LOCK_WEAPON_SNIPER_RIFLE = 38,
    MGO_LOCK_WEAPON_RAIL_DRIVER = 39,
    MGO_LOCK_WEAPON_SING_BOMB = 40,
    MGO_LOCK_WEAPON_PROX_MINE = 41,
    MGO_LOCK_WEAPON_REMOTE_CHARGES = 42,
    MGO_LOCK_WEAPON_ROCKET_LAUNCHER = 43,
    MGO_LOCK_WEAPON_THERMOBARIC_ROCKET = 44,
    MGO_LOCK_WEAPON_NANO_RIFLE = 45,
    MGO_LOCK_WEAPON_GRINDER = 46,
    MGO_LOCK_WEAPON_ARC_WELDER = 47,
    MGO_LOCK_WEAPON_SHOTGUN = 48,
    MGO_LOCK_BACKPACK_JETPACK = 49,
    MGO_LOCK_BACKPACK_THRUST = 50,
    MGO_LOCK_BACKPACK_FLEETFOOT = 51,
    MGO_LOCK_BACKPACK_STEALTH = 52,
    MGO_LOCK_BACKPACK_VISION = 53,
    MGO_LOCK_BACKPACK_FIREPOWER = 54,
    MGO_LOCK_BACKPACK_CONCUSSION = 55,
    MGO_LOCK_BACKPACK_HEAL = 56,
    MGO_LOCK_BACKPACK_RHINO = 57,
    MGO_LOCK_BACKPACK_TREMOR = 58,
    NUM_MULTI_GAME_OPTIONS = 59,
};

// size 0x40
struct multi_game_generic_options
{
    unsigned short point_limit; // 0x00
    unsigned char num_rounds; // 0x02
    unsigned char round_time_limit_minutes; // 0x03
    bool friendly_fire; // 0x04
    unsigned char respawn_time_seconds; // 0x05
    unsigned char health_regen_speed; // 0x06
    unsigned short max_health; // 0x08
    unsigned char movement_speed; // 0x0A
    multi_rubberbanding_levels rubberbanding_level; // 0x0C
    multi_game_enemy_icon_visibilities enemy_icon_visibility; // 0x10
    multi_game_times_of_day time_of_day; // 0x14
    multi_game_starting_backpack_options starting_backpack; // 0x18
    int starting_backpack_type; // 0x1C
    multi_game_backpack_spawn_options backpack_spawns; // 0x20
    int backpack_spawn_type; // 0x24
    bool unlimited_ammo; // 0x28
    bool replace_starting_weapon; // 0x29
    unsigned int starting_weapon; // 0x2C
    unsigned int third_weapon; // 0x30
    unsigned int fourth_weapon; // 0x34
    multi_game_weapon_respawn_speeds weapon_respawn_speed; // 0x38
    unsigned short locked_weapons_mask; // 0x3C
    unsigned short locked_backpacks_mask; // 0x3E
};

// size 0x14
union multi_game_mode_options
{
    // size 0x1
    struct deathmatch_options
    {
        bool leader_highlighting; // 0x00
    };

    // size 0x6
    struct capture_the_flag_options
    {
        unsigned char flag_carrier_speed; // 0x00
        unsigned char flag_carrier_regen_speed; // 0x01
        unsigned char flag_return_time_seconds; // 0x02
        bool civilian_flag_carrier; // 0x03
        bool flag_touch_return; // 0x04
        bool flag_carrier_icon_always_on; // 0x05
    };

    // size 0x14
    struct siege_options
    {
        unsigned char percent_destruction_required; // 0x00
        unsigned int defense_starting_weapon; // 0x04
        unsigned int defense_third_weapon; // 0x08
        unsigned int defense_fourth_weapon; // 0x0C
        int defense_starting_backpack_type; // 0x10
    };

    // size 0x4
    struct king_of_the_hill_options
    {
        king_of_the_hill_multipliers all_target_multiplier; // 0x00
    };

    // size 0x2
    struct bagman_options
    {
        unsigned char bag_return_time_seconds; // 0x00
        bool bagman_kill_bonus; // 0x01
    };

    // size 0x2
    struct team_bagman_options
    {
        unsigned char bag_return_time_seconds; // 0x00
        bool bagman_kill_bonus; // 0x01
    };

    // size 0x8
    struct demolition_options
    {
        bool engineer_icon_always_on; // 0x00
        unsigned int engineer_weapon; // 0x04
    };

    union
    {
        deathmatch_options deathmatch; // 0x00
        capture_the_flag_options capture_the_flag; // 0x00
        siege_options siege; // 0x00
        king_of_the_hill_options king_of_the_hill; // 0x00
        bagman_options bagman; // 0x00
        team_bagman_options team_bagman; // 0x00
        demolition_options demolition; // 0x00
    };
};

// size 0x8
struct multi_game_options_set
{
    const multi_game_generic_options* generic_options; // 0x00
    const multi_game_mode_options* mode_specific_options; // 0x04
};

// size 0x44
struct multi_game_options_full_set
{
    multi_game_options_full_set(); // 0x0096BCE0

    checksum_stri name_crc; // 0x00
    multi_game_options_set sets[8]; // 0x04
};

// size 0x22C
struct multi_level_info
{
    multi_level_info(); // 0x00962EC0

    char filename[64]; // 0x00
    char display_name[64]; // 0x40
    checksum_stri filename_crc; // 0x80
    char description[256]; // 0x84
    char level_image_name[32]; // 0x184
    char level_load_image[32]; // 0x1A4
    bool is_downloadable_content; // 0x1C4
    int dlc_bundle_id; // 0x1C8
    int ideal_min_players; // 0x1CC
    int ideal_max_players; // 0x1D0
    int time_of_day_hour; // 0x1D4
    int time_of_day_minute; // 0x1D8
    float glow_multiplier; // 0x1DC
    int alt_time_of_day_hour; // 0x1E0
    int alt_time_of_day_minute; // 0x1E4
    float alt_glow_multiplier; // 0x1E8
    float wind_intensity; // 0x1EC
    float wind_direction; // 0x1F0
    bool play_wind_dust_effect; // 0x1F4
    float distance_multiplier; // 0x1F8
    float demolition_damage_multiplier; // 0x1FC
    unsigned int player_weapons[8]; // 0x200
    int num_player_weapons; // 0x220
    unsigned int suitable_mode_flags; // 0x224
    const multi_game_options_full_set* options; // 0x228
};

// size 0x80
class multi_game_settings
{
public:
    void reinit(); // 0x009B33B0 (:2429)
    void set_mode(multi_gameplay_mode mode); // 0x009A1EC0 (:2441)
    multi_gameplay_mode get_mode() const; // 0x00962F90 (:2461)
    multi_gameplay_mode next_mode() const; // 0x009A1F20 (:2466)
    multi_gameplay_mode prev_mode() const; // 0x009A2020 (:2480)
    multi_gameplay_mode rand_mode() const; // 0x009A2120 (:2494)
    void set_level(const multi_level_info* info); // 0x00962FA0 (:2531)
    const multi_level_info* get_level() const; // 0x00963040 (:2547)
    const multi_level_info* next_level() const; // 0x009A2290 (:2552)
    const multi_level_info* prev_level() const; // 0x009A2340 (:2567)
    const multi_level_info* rand_level() const; // 0x009A2420 (:2582)
    void reset_options_to_default(multi_gameplay_mode previous_mode); // 0x00963050 (:2613)
    bool is_equal(const multi_game_settings& rhs) const; // 0x009630D0 (:2626)

private:
    multi_gameplay_mode current_mode; // 0x00
    const multi_level_info* current_level; // 0x04
    const multi_level_info* mode_levels[8]; // 0x08
public:
    multi_game_generic_options generic_options; // 0x28
    multi_game_mode_options mode_options; // 0x68
    bool last_match_was_custom_game; // 0x7C
};

extern multi_game_settings Mp_server_options; // 0x02FE6C80

void multi_game_settings_preparse_levels(mempool_base& perm_pool); // 0x009B2AA0 (:2095)
void multi_game_settings_system_init(mempool_base& perm_pool, mempool_base& temp_pool); // 0x009B2B70 (:2123)
void multi_game_settings_system_shutdown(); // 0x00962ED0 (:2139)
const multi_level_info* multi_game_settings_find_level_by_name(const char* name); // 0x009A1BF0 (:2144)
const multi_level_info* multi_game_settings_find_level_by_filename(const char* filename); // 0x009A1C60 (:2157)
const multi_game_options_full_set* multi_game_settings_find_game_option_full_set(const char* set_name); // 0x009A1CD0 (:2170)
bool multi_game_settings_parse_game_option_full_set(multi_game_options_full_set& set, xml_element* options_base, mempool_base& perm_pool, mempool_base& temp_pool); // 0x009A1D50 (:2185)
void multi_game_settings_parse_game_option_set(multi_game_options_set& set, xml_element* options_base, mempool_base& perm_pool, multi_gameplay_mode mode, bool mode_specific_required, const multi_game_generic_options* generic_defaults, const multi_game_mode_options* mode_specific_defaults); // 0x00962EE0 (:2233)
void multi_game_append_server_options_to_packet(unsigned char* packet_data, unsigned short& packet_size, const multi_game_settings* const src); // 0x00984510 (:2264)
void multi_game_read_server_options_from_packet(multi_game_settings* const dest, unsigned char* packet_data, int& packet_offset); // 0x009B2C60 (:2338)
xml_element* multi_game_settings_retrieve_settings_file(const char* filename); // 0x00963140 (:2650)
unsigned int multi_game_settings_version(); // 0x009631E0 (:2675)
unsigned int multi_game_settings_available_dlc_flags(); // 0x009631F0 (:2684)
bool multi_game_settings_is_option_default(const multi_game_settings* set, multi_game_options option); // 0x009A25A0 (:2692)
const char* multi_game_settings_get_option_name(multi_game_options option); // 0x00963200 (:2711)
void multi_game_settings_get_option_description(wchar_t* buffer, int buffer_len, multi_game_options option); // 0x00963220 (:2722)
void multi_game_settings_get_option_text(char* output_buffer, int buffer_size_in, const multi_game_settings* set, multi_game_options option); // 0x009632B0 (:2753)
int multi_game_settings_num_levels(); // 0x00984B00 (:3280)
const char* multi_game_settings_get_level_name_by_index(int index); // 0x00984B10 (:3285)
int multi_game_settings_get_level_name_crc_by_index(int index); // 0x00984B30 (:3290)
const multi_level_info* multi_game_settings_get_level_by_index(int index); // 0x00984B60 (:3295)
void multi_game_settings_parse_time_of_day(int* hour, int* minute, const char* time_of_day); // 0x00984B80 (:3300)
void multi_game_settings_swap_weapon_bit(multi_game_settings* settings, multi_game_lockable_weapons weapon); // 0x00963A10 (:3338)
void multi_game_settings_swap_backpack_bit(multi_game_settings* settings, multi_game_lockable_backpacks backpack); // 0x00963A30 (:3343)
bool multi_game_settings_is_weapon_locked(multi_game_settings* settings, multi_game_lockable_weapons weapon); // 0x00963A50 (:3348)
bool multi_game_settings_is_backpack_locked(multi_game_settings* settings, multi_game_lockable_backpacks backpack); // 0x00963A70 (:3353)
bool multi_game_settings_is_backpack_locked(int backpack_id); // 0x00963A90 (:3358)
// template instance: void multi_game_settings_process_numeric_option_down<unsigned char>(unsigned char& num, unsigned char step, unsigned char min, unsigned char max); // 0x008A59E0 (.h:424)
// template instance: void multi_game_settings_process_numeric_option_down<enum multi_game_enemy_icon_visibilities>(multi_game_enemy_icon_visibilities& num, multi_game_enemy_icon_visibilities step, multi_game_enemy_icon_visibilities min, multi_game_enemy_icon_visibilities max); // 0x008A5A60 (.h:424)
// template instance: void multi_game_settings_process_numeric_option_down<enum multi_game_backpack_spawn_options>(multi_game_backpack_spawn_options& num, multi_game_backpack_spawn_options step, multi_game_backpack_spawn_options min, multi_game_backpack_spawn_options max); // 0x008A5AE0 (.h:424)
// template instance: void multi_game_settings_process_numeric_option_down<unsigned short>(unsigned short& num, unsigned short step, unsigned short min, unsigned short max); // 0x008A5980 (.h:424)
// template instance: void multi_game_settings_process_numeric_option_down<enum multi_game_weapon_respawn_speeds>(multi_game_weapon_respawn_speeds& num, multi_game_weapon_respawn_speeds step, multi_game_weapon_respawn_speeds min, multi_game_weapon_respawn_speeds max); // 0x008A5B60 (.h:424)
// template instance: void multi_game_settings_process_numeric_option_down<enum multi_game_times_of_day>(multi_game_times_of_day& num, multi_game_times_of_day step, multi_game_times_of_day min, multi_game_times_of_day max); // 0x008A5AA0 (.h:424)
// template instance: void multi_game_settings_process_numeric_option_down<enum king_of_the_hill_multipliers>(king_of_the_hill_multipliers& num, king_of_the_hill_multipliers step, king_of_the_hill_multipliers min, king_of_the_hill_multipliers max); // 0x008A5BA0 (.h:424)
// template instance: void multi_game_settings_process_numeric_option_down<enum multi_game_starting_backpack_options>(multi_game_starting_backpack_options& num, multi_game_starting_backpack_options step, multi_game_starting_backpack_options min, multi_game_starting_backpack_options max); // 0x008A5B20 (.h:424)
// template instance: void multi_game_settings_process_numeric_option_down<enum multi_rubberbanding_levels>(multi_rubberbanding_levels& num, multi_rubberbanding_levels step, multi_rubberbanding_levels min, multi_rubberbanding_levels max); // 0x008A5A20 (.h:424)
// template instance: void multi_game_settings_process_numeric_option_up<enum multi_game_times_of_day>(multi_game_times_of_day& num, multi_game_times_of_day step, multi_game_times_of_day min, multi_game_times_of_day max); // 0x008A5AC0 (.h:434)
// template instance: void multi_game_settings_process_numeric_option_up<enum multi_game_enemy_icon_visibilities>(multi_game_enemy_icon_visibilities& num, multi_game_enemy_icon_visibilities step, multi_game_enemy_icon_visibilities min, multi_game_enemy_icon_visibilities max); // 0x008A5A80 (.h:434)
// template instance: void multi_game_settings_process_numeric_option_up<enum king_of_the_hill_multipliers>(king_of_the_hill_multipliers& num, king_of_the_hill_multipliers step, king_of_the_hill_multipliers min, king_of_the_hill_multipliers max); // 0x008A5BC0 (.h:434)
// template instance: void multi_game_settings_process_numeric_option_up<enum multi_game_starting_backpack_options>(multi_game_starting_backpack_options& num, multi_game_starting_backpack_options step, multi_game_starting_backpack_options min, multi_game_starting_backpack_options max); // 0x008A5B40 (.h:434)
// template instance: void multi_game_settings_process_numeric_option_up<enum multi_game_weapon_respawn_speeds>(multi_game_weapon_respawn_speeds& num, multi_game_weapon_respawn_speeds step, multi_game_weapon_respawn_speeds min, multi_game_weapon_respawn_speeds max); // 0x008A5B80 (.h:434)
// template instance: void multi_game_settings_process_numeric_option_up<unsigned short>(unsigned short& num, unsigned short step, unsigned short min, unsigned short max); // 0x008A59B0 (.h:434)
// template instance: void multi_game_settings_process_numeric_option_up<enum multi_rubberbanding_levels>(multi_rubberbanding_levels& num, multi_rubberbanding_levels step, multi_rubberbanding_levels min, multi_rubberbanding_levels max); // 0x008A5A40 (.h:434)
// template instance: void multi_game_settings_process_numeric_option_up<enum multi_game_backpack_spawn_options>(multi_game_backpack_spawn_options& num, multi_game_backpack_spawn_options step, multi_game_backpack_spawn_options min, multi_game_backpack_spawn_options max); // 0x008A5B00 (.h:434)
// template instance: void multi_game_settings_process_numeric_option_up<unsigned char>(unsigned char& num, unsigned char step, unsigned char min, unsigned char max); // 0x008A5A00 (.h:434)

// UI.md UI5 (the game options menu): the templates of the instances above, from their decompiles. A step past min
// (or an underflow) wraps to max; past max (or an overflow) to min.
template<typename T>
inline void multi_game_settings_process_numeric_option_down(T& num, T step, T min, T max) // (.h:424)
{
    T value = (T)(num - step);
    if (value < min || num < value)
    {
        value = max;
    }
    num = value;
}

template<typename T>
inline void multi_game_settings_process_numeric_option_up(T& num, T step, T min, T max) // (.h:434)
{
    T value = (T)(num + step);
    if (max < value || value < num)
    {
        value = min;
    }
    num = value;
}
