// Original: volition/rfg/code/ai/ai_rate_limiters.cpp
//
// AI rate limiters (stub_impl.md S9). Linked by OpenRFG only.

#include "volition/rfg/code/ai/ai_rate_limiters.h"

#include "volition/rfg/code/objects/humans/npc.h"

// Constructor arguments read from the initialised .data of rfg.exe (min_cap, initial_cap, max_starvation_frames,
// decrease_threshold, decrease_check_interval).
per_frame_rate_limiter Ai_notice_humans_rate_limiter(4, 6, 8, 4, 100); // 0x01641600
per_frame_rate_limiter Ai_collect_buildings_rate_limiter(1, 2, 8, 3, 100); // 0x01641624
per_frame_rate_limiter Ai_update_can_shoot_at_limiter(1, 2, 6, 3, 100); // 0x01641648
per_frame_rate_limiter Ai_sidestep_limiter(1, 2, 6, 3, 100); // 0x0164166C
ai_fixed_rate_limiter<3> Ai_cover_search_limiter("cover search"); // 0x01641690, name unused and not in the binary
ai_queued_rate_limiter<ai_pathfind_priorities,3> Ai_pathfind_rate_limiter(1, 3, "pathfind"); // 0x016416B0
ai_queued_rate_limiter<ai_replan_priorities,5> Ai_replan_rate_limiter(8, 3, "replan"); // 0x01641C30

// 0x005DD560 (:54)
unsigned int ai_rate_limiters_handle_from_npc(const npc* np)
{
    return np->get_handle();
}

// 0x005FE5E0 (:68)
void per_frame_rate_limiter::do_frame()
{
    const int ops = max_ops;
    if (requests > ops)
    {
        ++starvation_frames;
        if (starvation_frames > longest_starvation_time)
        {
            longest_starvation_time = starvation_frames;
        }
    }
    else
    {
        starvation_frames = 0;
    }

    if (longest_starvation_time >= max_starvation_frames)
    {
        longest_starvation_time = 0;
        decrease_frames = 0;
        starvation_frames = 0;
        requests = 0;
        max_ops = ops + 1;
        return;
    }
    if (decrease_frames > decrease_check_interval)
    {
        if (longest_starvation_time < decrease_threshold && ops > min_cap)
        {
            max_ops = ops - 1;
            starvation_frames = 0;
        }
        longest_starvation_time = 0;
        decrease_frames = 0;
        requests = 0;
        return;
    }
    requests = 0;
    ++decrease_frames;
}

// 0x006B4F30 (:114)
void ai_rate_limiters_level_init_pre()
{
    Ai_pathfind_rate_limiter.reset();
    Ai_replan_rate_limiter.reset();
}

// 0x0068EE90 (:120)
void ai_rate_limiters_do_frame()
{
    Ai_collect_buildings_rate_limiter.do_frame();
    Ai_notice_humans_rate_limiter.do_frame();
    Ai_update_can_shoot_at_limiter.do_frame();
    Ai_sidestep_limiter.do_frame();
    Ai_cover_search_limiter.do_frame();
    Ai_pathfind_rate_limiter.do_frame();
    Ai_replan_rate_limiter.do_frame();
}
