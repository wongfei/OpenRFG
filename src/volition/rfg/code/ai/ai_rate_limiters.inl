// Original: volition/rfg/code/ai/ai_rate_limiters.inl
//
// Template bodies of ai_rate_limiters.h (stub_impl.md S9).
#pragma once

// 0x005DD4F0 (.h:47)
inline per_frame_rate_limiter::per_frame_rate_limiter(int min_cap_, int initial_cap, int max_starvation_frames_, int decrease_threshold_, int decrease_check_interval_)
    : max_starvation_frames(max_starvation_frames_)
    , decrease_threshold(decrease_threshold_)
    , decrease_check_interval(decrease_check_interval_)
    , min_cap(min_cap_)
    , max_ops(initial_cap)
    , requests(0)
    , starvation_frames(0)
    , decrease_frames(0)
    , longest_starvation_time(0)
{
}

// 0x005DD530 (.h:54)
inline bool per_frame_rate_limiter::request()
{
    ++requests;
    return requests <= max_ops;
}

inline int per_frame_rate_limiter::get_limit() const
{
    return max_ops;
}

inline int per_frame_rate_limiter::get_num_requests() const
{
    return requests;
}

// <3> 0x00671900 (.h:89)
template<int N>
ai_fixed_rate_limiter<N>::ai_fixed_rate_limiter(const char* limiter_name)
{
    num_requests = 0;
}

// <3> 0x0064D6E0
template<int N>
ai_fixed_rate_limiter<N>::~ai_fixed_rate_limiter()
{
}

template<int N>
int ai_fixed_rate_limiter<N>::get_limit() const
{
    return N;
}

template<int N>
int ai_fixed_rate_limiter<N>::get_num_requests() const
{
    return num_requests;
}

// <3> 0x00671E80 (:33)
template<int N>
bool ai_fixed_rate_limiter<N>::request(const npc* np)
{
    if (np == nullptr)
    {
        return false;
    }
    const unsigned int handle = ai_rate_limiters_handle_from_npc(np);
    ++num_requests;
    if (requests.size() >= N)
    {
        return false;
    }
    request_record record;
    record.np_handle = handle;
    requests.add(record);
    return true;
}

// <3> 0x006494D0 (:70)
template<int N>
bool ai_fixed_rate_limiter<N>::npc_has_request_pending(const npc* np)
{
    if (np == nullptr)
    {
        return false;
    }
    const unsigned int handle = ai_rate_limiters_handle_from_npc(np);
    for (int i = 0; i < requests.size(); ++i)
    {
        if (requests[i].np_handle == handle)
        {
            return true;
        }
    }
    return false;
}

// <3> 0x00649510 (:93)
template<int N>
void ai_fixed_rate_limiter<N>::do_frame()
{
    num_requests = 0;
    requests.clear();
}

// <enum ai_pathfind_priorities,3> 0x006BB670 (:119)
template<typename P, int N>
ai_queued_rate_limiter<P, N>::ai_queued_rate_limiter(int per_frame, int expire_frames, const char* name)
    : visualize(false)
    , granted_requests_per_frame(per_frame)
    , request_expire_frames(expire_frames)
    , request_pool(nullptr, nullptr, 0)
{
    reset();
}

// <enum ai_pathfind_priorities,3> 0x00673C90
template<typename P, int N>
ai_queued_rate_limiter<P, N>::~ai_queued_rate_limiter()
{
}

// <enum ai_pathfind_priorities,3> 0x006B4BD0 (:132)
template<typename P, int N>
void ai_queued_rate_limiter<P, N>::reset()
{
    request_pool.clear();
    granted_requests_this_frame = 0;
}

// <enum ai_pathfind_priorities,3> 0x006733D0 (:156)
template<typename P, int N>
ai_queued_request* ai_queued_rate_limiter<P, N>::find_request(int& queue_pos, P& old_priority, unsigned int np_handle)
{
    // The highest priority queue is ahead of the others, so queue_pos counts from there.
    queue_pos = 0;
    for (int priority = N - 1; priority >= 0; --priority)
    {
        csspinlock_scope lock(request_pool.thread_access);
        for (ai_queued_request* request = request_pool.get_first(priority); request != nullptr; request = request_pool.get_next(request))
        {
            if (request->requesting_npc == np_handle)
            {
                old_priority = (P)priority;
                return request;
            }
            ++queue_pos;
        }
    }
    return nullptr;
}

// <enum ai_pathfind_priorities,3> 0x006734A0 (:181)
template<typename P, int N>
void ai_queued_rate_limiter<P, N>::add_new_request(unsigned int np_handle, P priority)
{
    ai_queued_request* request = request_pool.alloc(priority);
    if (request != nullptr)
    {
        request->requesting_npc = np_handle;
        request->since_first_request = 0;
        request->since_last_request = 0;
    }
}

// <enum ai_replan_priorities,5> 0x0068DFB0 (:196)
template<typename P, int N>
void ai_queued_rate_limiter<P, N>::request_queue(const npc* np, P priority)
{
    if (np == nullptr || (int)priority >= N)
    {
        return;
    }
    const unsigned int np_handle = ai_rate_limiters_handle_from_npc(np);
    int queue_pos;
    P old_priority;
    ai_queued_request* request = find_request(queue_pos, old_priority, np_handle);
    if (request != nullptr)
    {
        if ((int)priority <= (int)old_priority)
        {
            request->since_last_request = 0;
            return;
        }
        request_pool.free(request);
    }
    add_new_request(np_handle, priority);
}

// <enum ai_pathfind_priorities,3> 0x0068DE00 (:243)
template<typename P, int N>
bool ai_queued_rate_limiter<P, N>::request_immediate(const npc* np, P priority)
{
    if (np == nullptr || (int)priority >= N)
    {
        return false;
    }
    const unsigned int np_handle = ai_rate_limiters_handle_from_npc(np);
    const int available = granted_requests_per_frame - granted_requests_this_frame;
    int queue_pos;
    P old_priority;
    ai_queued_request* request = find_request(queue_pos, old_priority, np_handle);
    if (request != nullptr)
    {
        if ((int)priority <= (int)old_priority)
        {
            if (queue_pos < available)
            {
                ++granted_requests_this_frame;
                request_pool.free(request);
                return true;
            }
            request->since_last_request = 0;
            return false;
        }
        request_pool.free(request);
    }
    int waiting_ahead = 0;
    for (int i = priority; i < N; ++i)
    {
        waiting_ahead += request_pool.get_count(i);
    }
    if (waiting_ahead < available)
    {
        ++granted_requests_this_frame;
        return true;
    }
    add_new_request(np_handle, priority);
    return false;
}

// <enum ai_pathfind_priorities,3> 0x0068DEE0 (:322)
template<typename P, int N>
void ai_queued_rate_limiter<P, N>::do_frame()
{
    granted_requests_this_frame = 0;
    for (int priority = 0; priority < N; ++priority)
    {
        csspinlock_scope lock(request_pool.thread_access);
        ai_queued_request* next;
        for (ai_queued_request* request = request_pool.get_first(priority); request != nullptr; request = next)
        {
            next = request_pool.get_next(request);
            ++request->since_first_request;
            ++request->since_last_request;
            if (request_expire_frames <= (int)request->since_last_request)
            {
                request_pool.free(request);
            }
        }
    }
}
