// Original: volition/rfg/code/multi/multi_game/multi_game_camera.h
// Implementation: volition/rfg/code/multi/multi_game/multi_game_camera.cpp
//
// The multiplayer camera: the spectator and action cameras (points of interest, switch reasons), the end of round
// aquarium view, and camera shakes from explosions. not_a_vector is a constructor-less vector (convertible to vector) so
// that a point of interest can hold an object handle or a location in a union; its conversions are in
// multi_game_camera.cpp. Placement of the camera types inferred.
#pragma once


class multi_object_marker;
class player;
class vector;

enum multi_camera_mode
{
    MCM_NONE = -1,
    MCM_MANUAL = 0,
    MCM_ACTION = 1,
    MCM_PLAYER = 2,
    MCM_AQUARIUM = 3,
    NUM_MULTI_CAMERA_MODES = 4,
};

enum action_camera_modes
{
    ACM_NONE = -1,
    ACM_PLAYER_TRACKING = 0,
    ACM_NEAR_TRACKING = 1,
    ACM_FORWARD = 2,
    ACM_REVERSE = 3,
    ACM_ENEMY_TRACK = 4,
    ACM_LANDMARK = 5,
    NUM_ACTION_CAMERA_MODES = 6,
};

enum action_camera_switch_reason
{
    ACSR_INIT = 0,
    ACSR_TIME = 1,
    ACSR_BUTTON_PRESS = 2,
    ACSR_TRACKED_PLAYER_DIED = 3,
    ACSR_DEAD_OR_NOT_VISIBLE = 4,
    NUM_ACTION_CAMERA_SWITCH_REASONS = 5,
};

enum point_of_interest_type
{
    POIT_OBJECT_HANDLE = 0,
    POIT_LOCATION = 1,
};

// size 0xC
struct not_a_vector
{
    operator vector&();
    operator const vector&();
    const vector* operator&() const;
    vector* operator&();

    float x; // 0x00
    float y; // 0x04
    float z; // 0x08
};

// size 0x20
struct point_of_interest
{
    // size 0xC
    union _point
    {
        unsigned int object_handle; // 0x00
        not_a_vector location; // 0x00
    };

    point_of_interest_type type; // 0x00
    _point point; // 0x04
    float interest_blend_in_pct; // 0x10
    float interest_weight; // 0x14
    float fov_size; // 0x18
    float lifetime; // 0x1C
};

// size 0x10
struct aquarium_score_info
{
    aquarium_score_info(const multi_object_marker* _marker, bool _frustrum_check, bool _orient_check, const player* pp); // 0x0094F640 (:186)

    const multi_object_marker* marker; // 0x00
    bool frustrum_check; // 0x04
    bool orient_check; // 0x05
    const player* must_see_player; // 0x08
    float fov; // 0x0C
};

void unused_dcf_i_killed_me(); // 0x009AC290 (:450)
void multi_game_camera_set_spectator_camera_to_action(); // 0x009507F0 (:1153)
bool action_find_best_marker(unsigned int best_player, unsigned int exclude_marker_handle); // 0x009ACB30 (:1160)
void process_view_mode(); // 0x00979E90 (:1384)
// template instance: unsigned int get_subsequent_common<16>(farray<unsigned int,16>& handles, unsigned int cur_handle, bool next); // 0x00989630 (:1541)
// template instance: unsigned int get_subsequent_common<128>(farray<unsigned int,128>& handles, unsigned int cur_handle, bool next); // 0x009895B0 (:1541)
void multi_game_camera_shutdown(); // 0x009508F0 (:2104)
void multi_game_camera_init_post(); // 0x00950950 (:2113)
void multi_game_camera_start(); // 0x009970A0 (:2123)
void multi_game_camera_stop(); // 0x0097A580 (:2154)
void multi_game_camera_process(); // 0x009AE230 (:2184)
multi_camera_mode multi_game_camera_get_mode(); // 0x009509A0 (:2270)
bool multi_game_camera_following_killer(); // 0x009509B0 (:2275)
void multi_game_camera_player_killed(player* victim, player* killer); // 0x00997190 (:2280)
void multi_game_camera_explosion_occured(const vector& position, float human_damage_radius); // 0x009AE520 (:2318)
void multi_game_camera_player_respawn(player* pp); // 0x009AE730 (:2370)
bool multi_game_camera_is_running(); // 0x009509C0 (:2412)
bool multi_game_camera_is_spectator_hud_visible(); // 0x009509D0 (:2417)
void multi_game_camera_show_spectator_hud(bool show); // 0x009509F0 (:2422)
void multi_game_camera_level_init_pre(); // 0x00950A20 (:2433)
void multi_game_camera_get_control_hints(char* dest, unsigned int dest_size); // 0x0097A640 (:2439)
void multi_game_camera_get_control_hints(wchar_t* dest, unsigned int dest_size); // 0x00950A30 (:2446)
const wchar_t* multi_game_camera_get_camera_mode_name(); // 0x00950E40 (:2518)
const vector* multi_game_camera_get_third_person_offset(const player* target_pp); // 0x00950FC0 (:2576)
