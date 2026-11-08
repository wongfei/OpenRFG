// Original: volition/rfg/code/objects/humans/human_ragdoll.h
// Implementation: volition/rfg/code/objects/humans/human_ragdoll.cpp
//
// Human ragdolls: starting a ragdoll (impulse, flinch, knockdown timer), impacts and crushing, and getting up again:
// human_ragdoll_get_up_check sphere casts for a place to stand and records why a position was rejected.
#pragma once

#include "volition/vlib/code/math/vector3.h"

class human;
class object;

enum human_ragdoll_get_up_check_errors
{
    GET_UP_ERROR_NOT_CHECKED = -1,
    GET_UP_ERROR_NONE = 0,
    GET_UP_ERROR_BAD_TERRAIN = 1,
    GET_UP_ERROR_TOO_LOW = 2,
    GET_UP_ERROR_X_OBJECT = 3,
    GET_UP_ERROR_X_VEHICLE = 4,
    GET_UP_ERROR_UNDER_BBOX = 5,
    NUM_HUMAN_RAGDOLL_GET_UP_CHECK_ERRORS = 6,
};

// size 0x28
struct human_ragdoll_get_up_check
{
    human_ragdoll_get_up_check(); // 0x00A0B900 (:97)
    void set(vector _position, unsigned int _ignore_obj, float _eval_fitness); // 0x00A0BB10 (:256)
    const vector& get_position(); // 0x00A0B920 (:101)
    void get_errors(vector from_pos); // 0x00A80DE0 (:288)
    void debug_errors(vector from_pos); // 0x00A0BB40 (:264)

private:
    vector position; // 0x00
    unsigned int ignore_handle; // 0x0C
public:
    float eval_fitness; // 0x10
    vector destination; // 0x14
    const object* blocking_object; // 0x20
    human_ragdoll_get_up_check_errors error; // 0x24
};

void unused_dcf_ragdoll_player(); // 0x00AD4F30 (:116)
bool human_ragdoll_get_up_spherecast_cb(object* hit_objp, void* info); // 0x00A0BAB0 (:231)
bool human_ragdoll_get_up_collect_cb(object* hit_objp, unsigned int shape_key, const void* user_data); // 0x00A0BAE0 (:252)
bool human_find_place_to_stand_from_list(vector& destination, const vector& human_pos, const vector* const vec_list, int num_points, unsigned int ignore_obj); // 0x00A818A0 (:641)
bool human_find_place_to_stand(vector& destination, human* p_human, float rand_check_radius, const vector* const hint_pos); // 0x00A819D0 (:669)
bool human_ragdoll_start_flinch(human* hp, bool immediate); // 0x00A31420 (:966)
bool human_ragdoll_start_impulse(human* hp, const vector& impulse, const vector& angular_impulse); // 0x00AD4F50 (:996)
void human_ragdoll_set_knockdown_timer(human* hp); // 0x00A31480 (:1027)
bool human_ragdoll_start(human* hp, bool forced_by_server, unsigned int perp); // 0x00AC9B00 (:1072)
bool human_ragdoll_stop(human* hp, bool hold_pos); // 0x00AC1850 (:1329)
void human_ragdoll_on_impact(human* hp, float time, float min_velocity, bool all_collisions); // 0x00A0BC50 (:1396)
bool human_ragdoll_can_be_crushed(const human* const hp); // 0x00A31580 (:1425)
bool human_ragdoll_get_up(human* hp, bool search_for_valid_ground); // 0x00AD50D0 (:1438)
void human_update_valid_position_before_ragdoll(human* hp); // 0x00A0BD30 (:1678)
bool human_move_fall_collide_head(human* hp); // 0x00A82980 (:1720)
void human_ragdoll_set_pos(human* hp, const vector& pos); // 0x00A0BDE0 (:1803)
void human_ragdoll_set_velocity(human* hp, const vector& velocity); // 0x00A0BE20 (:1810)
