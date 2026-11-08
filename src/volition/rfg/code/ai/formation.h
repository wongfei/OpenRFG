// Original: volition/rfg/code/ai/formation.h
// Implementation: volition/rfg/code/ai/formation.cpp
//
// Formations: a leader and its followers (squads, escorts, bodyguards) with formation types and follow modes; the
// followers' slot positions are updated along the leader's path. ai_system_do_frame calls ai_formation_do_frame.
#pragma once

#include "volition/vlib/code/math/vector3.h"

class object;

enum ai_formation_types
{
    AI_FORMATION_NONE = -1,
    AI_FORMATION_WEDGE = 0,
    AI_FORMATION_RANKS = 1,
    AI_FORMATION_COLUMNS = 2,
    AI_FORMATION_SURROUND = 3,
    AI_FORMATION_VEHICLE_ESCORT = 4,
    AI_FORMATION_ESCORT_PLAYER = 5,
    NUM_AI_FORMATION_RIGID_FORMATIONS = 6,
    AI_FORMATION_FLOCK = 6,
    AI_FORMATION_FOLLOW_THE_LEADER = 7,
    AI_FORMATION_FOLLOW_THE_LEADER_TIGHT = 8,
    NUM_AI_FORMATION_TYPES = 9,
};

enum ai_formation_follow_modes
{
    AI_FFM_INVALID = -1,
    AI_FFM_NORMAL = 0,
    AI_FFM_FOLLOW_THE_LEADER = 1,
    AI_FFM_FOLLOW_THE_LEADER_MUST_PATH = 2,
    AI_FFM_FLOCK = 3,
    NUM_AI_FORMATION_FOLLOW_MODES = 4,
};

// size 0x34
struct follow_information
{
    follow_information(); // 0x00602C90 (.h:55)
    void reset(); // 0x005DDA80 (.h:58)

    vector ideal_formation_pos; // 0x00
    vector formation_pos; // 0x0C
    vector formation_dir; // 0x18
    float formation_speed; // 0x24
    const object* my_leader; // 0x28
    bool position_invalid; // 0x2C
    ai_formation_follow_modes follow_mode; // 0x30
};

void ai_formation_level_init_pre(); // 0x006A0C00 (:205)
void ai_formation_do_frame(); // 0x006B7310 (:217)
unsigned int ai_formation_follow(object* follower, const object* target, ai_formation_types type); // 0x006A0C90 (:265)
int ai_formation_get_num_followers(unsigned int leader_handle); // 0x0065E580 (:316)
unsigned int ai_formation_from_leader(unsigned int leader_handle); // 0x0065E630 (:334)
unsigned int ai_formation_from_follower(unsigned int follower_handle); // 0x0065E6A0 (:350)
unsigned int ai_formation_get_leader_handle(unsigned int formation_handle); // 0x00631F40 (:367)
bool ai_formation_is_member(unsigned int formation_handle, unsigned int member_handle); // 0x0065E730 (:379)
object* ai_formation_get_leader(unsigned int formation_handle); // 0x00631F80 (:392)
object* ai_formation_get_leader(const object* follower); // 0x0065E790 (:402)
bool ai_formation_is_leader(unsigned int leader_handle, unsigned int formation_handle); // 0x00631FC0 (:413)
bool ai_formation_remove_follower(unsigned int formation_handle, unsigned int follower_handle); // 0x006B73E0 (:424)
bool ai_formation_remove_all_followers(unsigned int formation_handle); // 0x0067B330 (:440)
const follow_information* ai_formation_get_follower_position(unsigned int formation_handle, const object* obj); // 0x0067B370 (:457)
bool ai_formation_is_following(const object* follower, unsigned int handle); // 0x0065E7C0 (:468)
bool ai_formation_is_follower_of(const object* follower, const object* leader); // 0x0065E830 (:481)
int ai_formation_collect_followers(const object* leader, unsigned int* followers, int max_collect, bool (*callback)(unsigned int, void*), void* callback_data); // 0x0065E8E0 (:515)
bool ai_formation_is_rigid(unsigned int handle); // 0x00632000 (:539)
