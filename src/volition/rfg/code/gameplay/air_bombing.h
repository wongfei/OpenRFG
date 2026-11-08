// Original: volition/rfg/code/gameplay/air_bombing.h
// Implementation: volition/rfg/code/gameplay/air_bombing.cpp
//
// Air bombing: scripted air strikes that launch projectiles at targets (attack and avoid lists, a forced attack object),
// read from the air bomb definitions; the bombs are timed list items (obj_timed_list_item).
#pragma once

#include "volition/rfg/code/objects/projectile.h"
#include "volition/vlib/code/math/vector3.h"
#include "volition/vlib/code/time/timestamp.h"

struct weapon_info;

enum air_bomb_handle
{
    INVALID_AIR_BOMB_HANDLE = -1,
    AIR_BOMB_HANDLE_FORCE_TO_32_BIT = 0x7FFFFFFF,
};

// size 0x40
struct air_bomb_info
{
    char* name; // 0x00
    const weapon_info* w_info; // 0x04
    int num_projectiles; // 0x08
    float min_spread; // 0x0C
    float max_spread; // 0x10
    float min_projectile_delay; // 0x14
    float max_projectile_delay; // 0x18
    float height; // 0x1C
    float radius; // 0x20
    float veh_radius; // 0x24
    float rate_min; // 0x28
    float rate_max; // 0x2C
    float projectile_travel_time; // 0x30
    float angle_min; // 0x34
    float angle_max; // 0x38
    float min_player_fvec_dot_prod; // 0x3C
};

// size 0xC
struct obj_timed_list_item
{
    unsigned int handle; // 0x00
    timestamp_percent timer; // 0x04
};

// size 0xA0
class air_bomb
{
    bool setup_projectiles(); // 0x007A33B0 (:396)
    bool valid_projectile_target(const vector& pos, const vector& dir, float radius, bool attackingp); // 0x0079BB20 (:647)
    void launch_next_projectile(); // 0x00772880 (:787)
public:
    air_bomb(); // 0x00772800 (:203)
    void clear(); // 0x0075C6E0 (:380)
    bool is_running() const; // 0x0075C150 (:209)
    const air_bomb_info* get_current_info() const; // 0x0075C160 (:212)
    void process(); // 0x007A40F0 (:851)
    bool initialize(const air_bomb_info* ab_info, unsigned int follow_obj, unsigned int origin_obj, bool following); // 0x0075C760 (:901)
    void clear_pending_launches(); // 0x0075C720 (:826)
    void set_foce_attack_object(unsigned int target_handle); // 0x0075C730 (:835)

private:
    const air_bomb_info* info; // 0x00
    unsigned int focus; // 0x04
    vector position; // 0x08
    vector launch_position; // 0x14
    projectile_info projectiles[5]; // 0x20
    int next_projectile; // 0x84
    vector origin_position; // 0x88
    float left_over_time; // 0x94
    bool attacking; // 0x98
    unsigned int force_attack_handle; // 0x9C
};

void unused_dcf_air_bomb_start(); // 0x00772760 (:69)
void unused_dcf_air_bomb_reload(); // 0x007727E0 (:91)
void air_bomb_init(); // 0x0075C7E0 (:931)
void air_bomb_level_init(); // 0x0078B2D0 (:938)
void air_bomb_level_shutdown(); // 0x007729F0 (:944)
void air_bomb_do_frame(); // 0x007A4190 (:949)
void air_bomb_render_debug(); // 0x00772A00 (:956)
int air_bomb_start(const air_bomb_info* ab_info, unsigned int target_handle, unsigned int origin_handle, bool following); // 0x0075C800 (:987)
void air_bomb_stop(air_bomb_handle handle); // 0x0075C8A0 (:1011)
void air_bomb_stop_type(const air_bomb_info* ab_info); // 0x0075C8F0 (:1018)
void air_bomb_stop_all(); // 0x0075CAB0 (:1028)
void air_bomb_avoid_list_add(unsigned int avoid_handle, int time); // 0x0078B2E0 (:1037)
void air_bomb_avoid_list_remove(unsigned int avoid_handle); // 0x00772BE0 (:1057)
void air_bomb_avoid_list_clear(); // 0x00772C50 (:1069)
void air_bomb_attack_list_add(unsigned int attack_handle, int time); // 0x0078B3B0 (:1075)
void air_bomb_attack_list_remove(unsigned int attack_handle); // 0x00772C60 (:1093)
void air_bomb_attack_list_clear(); // 0x00772CD0 (:1105)
void air_bomb_clear_pending_launches(); // 0x0075CBF0 (:1110)
void air_bomb_destroy_target(unsigned int target_handle, int time); // 0x0078B470 (:1117)
// The grinder is the air strike's target designator.
extern bool Air_bomb_weapon; // 0x02229602
void air_bomb_kill_player(); // 0x0078B4C0 (:1135)
void air_bomb_hit_target(unsigned int target_handle); // 0x0075CC80 (:1149)
bool air_bomb_is_running(); // 0x0075CCD0 (:1162)
bool air_bomb_is_running_by_index(int index); // 0x00772CE0 (:1173)
const air_bomb_info* air_bomb_definition_from_index(int index); // 0x0075CCF0 (:1185)
int air_bomb_definition_get_index_from_name(const char* name); // 0x0075CD00 (:1192)
