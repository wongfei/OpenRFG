// Original: volition/rfg/code/ai/ai_rate_limiters.h
// Implementation: volition/rfg/code/ai/ai_rate_limiters.cpp
//
// AI rate limiters: per-frame caps on expensive AI work (building collection, noticing humans, can-shoot updates,
// sidesteps, cover searches), fixed-size per-npc request limiters, and the queued limiters for pathfinding and
// replanning, whose requests wait by priority until granted or expired. ai_system_do_frame advances them
// (the body of ai_rate_limiters_do_frame, inlined).
#pragma once

#include "volition/vlib/code/memory/object_pool.h"
#include "volition/vlib/code/util/varray.h"

class npc;

enum ai_pathfind_priorities
{
    AI_PATHFIND_PRIORITY_LOW = 0,
    AI_PATHFIND_PRIORITY_NORMAL = 1,
    AI_PATHFIND_PRIORITY_HIGH = 2,
    NUM_AI_PATHFIND_PRIORITIES = 3,
};

enum ai_replan_priorities
{
    AI_REPLAN_PRIORITY_LOW = 0,
    AI_REPLAN_PRIORITY_NORMAL = 1,
    AI_REPLAN_PRIORITY_IN_REPLAN_NORMAL = 2,
    AI_REPLAN_PRIORITY_HIGH = 3,
    AI_REPLAN_PRIORITY_IN_REPLAN_HIGH = 4,
    NUM_AI_REPLAN_PRIORITIES = 5,
};

// size 0x8
struct ai_queued_request
{
    unsigned int requesting_npc; // 0x00
    unsigned short since_first_request; // 0x04
    unsigned short since_last_request; // 0x06
};

// size 0x24
class per_frame_rate_limiter
{
public:
    per_frame_rate_limiter(int min_cap_, int initial_cap, int max_starvation_frames_, int decrease_threshold_, int decrease_check_interval_); // 0x005DD4F0 (.h:47)
    int get_limit() const;
    int get_num_requests() const;
    void do_frame(); // 0x005FE5E0 (:68)
    bool request(); // 0x005DD530 (.h:54)
    void release();

private:
    const int max_starvation_frames; // 0x00
    const int decrease_threshold; // 0x04
    const int decrease_check_interval; // 0x08
    const int min_cap; // 0x0C
    int max_ops; // 0x10
    int requests; // 0x14
    int starvation_frames; // 0x18
    int decrease_frames; // 0x1C
    int longest_starvation_time; // 0x20
};

// size (<3>) 0x20
template<int N>
class ai_fixed_rate_limiter
{
public:
    // size 0x4
    struct request_record
    {
        unsigned int np_handle; // 0x00
    };

    ai_fixed_rate_limiter(const char* limiter_name); // <3> 0x00671900 (.h:89)
    int get_limit() const;
    int get_num_requests() const;
    bool request(const npc* np); // <3> 0x00671E80 (ai_rate_limiters.inl:33)
    bool npc_has_request_pending(const npc* np); // <3> 0x006494D0 (ai_rate_limiters.inl:70)
    void do_frame(); // <3> 0x00649510 (ai_rate_limiters.inl:93)
    ~ai_fixed_rate_limiter(); // <3> 0x0064D6E0

private:
    int num_requests; // 0x00
    farray<request_record,N> requests; // 0x04
};

// size (<enum ai_pathfind_priorities,3>) 0x580
template<typename P, int N>
class ai_queued_rate_limiter
{
public:
    ai_queued_rate_limiter(int per_frame, int expire_frames, const char* name); // <enum ai_pathfind_priorities,3> 0x006BB670 (ai_rate_limiters.inl:119)
    void reset(); // <enum ai_pathfind_priorities,3> 0x006B4BD0 (ai_rate_limiters.inl:132)
    void request_queue(const npc*, P);
    bool request_immediate(const npc* np, P priority); // <enum ai_pathfind_priorities,3> 0x0068DE00 (ai_rate_limiters.inl:243)
    void do_frame(); // <enum ai_pathfind_priorities,3> 0x0068DEE0 (ai_rate_limiters.inl:322)
private:
    ai_queued_request* find_request(int& queue_pos, P& old_priority, unsigned int np_handle); // <enum ai_pathfind_priorities,3> 0x006733D0 (ai_rate_limiters.inl:156)
    void add_new_request(unsigned int np_handle, P priority); // <enum ai_pathfind_priorities,3> 0x006734A0 (ai_rate_limiters.inl:181)
public:
    ~ai_queued_rate_limiter(); // <enum ai_pathfind_priorities,3> 0x00673C90

    bool visualize; // 0x00
private:
    const int granted_requests_per_frame; // 0x04
    const int request_expire_frames; // 0x08
    int granted_requests_this_frame; // 0x0C
    object_pool<ai_queued_request,100,N> request_pool; // 0x10
};

extern per_frame_rate_limiter Ai_notice_humans_rate_limiter; // 0x01641600
extern per_frame_rate_limiter Ai_collect_buildings_rate_limiter; // 0x01641624
extern per_frame_rate_limiter Ai_update_can_shoot_at_limiter; // 0x01641648
extern per_frame_rate_limiter Ai_sidestep_limiter; // 0x0164166C
extern ai_fixed_rate_limiter<3> Ai_cover_search_limiter; // 0x01641690
extern ai_queued_rate_limiter<ai_pathfind_priorities,3> Ai_pathfind_rate_limiter; // 0x016416B0
extern ai_queued_rate_limiter<ai_replan_priorities,5> Ai_replan_rate_limiter; // 0x01641C30

unsigned int ai_rate_limiters_handle_from_npc(const npc* np); // 0x005DD560 (:54)
void ai_rate_limiters_level_init_pre(); // 0x006B4F30 (:114)
void ai_rate_limiters_do_frame(); // 0x0068EE90 (:120)

#include "volition/rfg/code/ai/ai_rate_limiters.inl"
// template instance: void __thiscall ai_queued_rate_limiter<enum ai_pathfind_priorities,3>::ai_queued_rate_limiter<enum ai_pathfind_priorities,3>(int per_frame, int expire_frames, const char* name); // 0x006BB670 (ai_rate_limiters.inl:119)
// template instance: void __thiscall ai_queued_rate_limiter<enum ai_replan_priorities,5>::ai_queued_rate_limiter<enum ai_replan_priorities,5>(int per_frame, int expire_frames, const char* name); // 0x006BB6B0 (ai_rate_limiters.inl:119)
