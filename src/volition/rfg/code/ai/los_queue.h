// Original: volition/rfg/code/ai/los_queue.h
// Implementation: volition/rfg/code/ai/los_queue.cpp
//
// The line-of-sight queue: pairs of humans whose visibility is checked by raycasts spread over frames by priority, with
// cached records the perception reads. gamestate_gp_simulate_frame calls los_queue_do_frame.
#pragma once

#include "volition/rfg/code/objects/humans/human_enums.h"
#include "volition/vlib/code/math/vector3.h"
#include "volition/vlib/code/time/timestamp.h"

class human;
class object;
class turret;
class vehicle;

enum los_request_priority
{
    LOS_REQUEST_PRIORITY_HIGH = 0,
    LOS_REQUEST_PRIORITY_LOW = 1,
    NUM_LOS_REQUEST_PRIORITIES = 2,
};

enum los_record_check_result
{
    LOS_RECORD_NOT_FOUND = 0,
    LOS_RECORD_REQUESTED = 1,
    LOS_RECORD_POSITIVE = 2,
    LOS_RECORD_NEGATIVE = 3,
    NUM_LOS_RECORD_CHECK_RESULTS = 4,
};

// size 0x10
struct los_request
{
    // size 0x1
    struct los_request_flags
    {
        unsigned char human1_interested : 1; // 0x00:0
        unsigned char human2_interested : 1; // 0x00:1
    };

    void init(); // 0x005EBA00 (:57)
    void set_priority(los_request_priority pri); // 0x005EBA10 (:61)
    los_request_priority get_priority() const; // 0x005EBA30 (:65)
    los_request(); // 0x005F9C50

    unsigned int human1_uid; // 0x00
    unsigned int human2_uid; // 0x04
    timestamp priority_upgrade_timer; // 0x08
    unsigned char priority; // 0x0C
    los_request_flags flags; // 0x0D
};

// size 0x2C
struct los_record
{
    // size 0x10
    struct los_record_human_info
    {
        los_record_human_info(); // 0x005F9D00

        unsigned int handle; // 0x00
        vector pos; // 0x04
    };

    // size 0x1
    struct los_record_flags
    {
        unsigned char positive : 1; // 0x00:0
    };

    human_stances get_stance(int which) const; // 0x005EBA40 (:96)
    void set_stance(int which, human_stances stance); // 0x005EBA50 (:100)
    los_record(); // 0x0064B0E0

    los_record_human_info human_info[2]; // 0x00
    timestamp created_time; // 0x20
    timestamp check_time; // 0x24
    unsigned char stances[2]; // 0x28
    los_record_flags flags; // 0x2A
};

// size 0x18
struct los_raycast_callback_data
{
    // size 0xC
    struct obj_data
    {
        obj_data(const object* objp_); // 0x0063A160 (:449)

        const object* objp; // 0x00
        const vehicle* vp; // 0x04
        const turret* tp; // 0x08
    };

    los_raycast_callback_data(const object* src_ptr, const object* target_ptr); // 0x0063A1D0 (:462)

    obj_data src; // 0x00
    obj_data target; // 0x0C
};

bool los_raycast_filter_callback(object* hit_objp, void* cb_data_ptr); // 0x005EBA80 (:480)
void los_queue_level_init_post(); // 0x006A4D80 (:519)
void los_queue_do_frame(); // 0x006BD700 (:538)
bool los_queue_remove_request(const human* hp1, const human* hp2); // 0x0067FE20 (:558)
bool los_queue_add_request(const human* hp1, const human* hp2, los_request_priority priority); // 0x0067FE70 (:593)
los_record_check_result los_queue_check_records(unsigned int human1_uid, unsigned int human2_uid); // 0x00663060 (:657)
bool los_record_get_pos_from_record(unsigned int human1_uid, unsigned int human2_uid, vector& human1_pos); // 0x006630A0 (:689)
