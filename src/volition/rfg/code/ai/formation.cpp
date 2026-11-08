// Original: volition/rfg/code/ai/formation.cpp

#include "volition/rfg/code/ai/formation.h"

#include "volition/rfg/code/ai/formation_internal.h"
#include "volition/rfg/code/level/world.h"
#include "volition/rfg/code/objects/object.h"
#include "volition/vlib/code/memory/object_pool.h"
#include "volition/vlib/code/os/cs_wrapper.h"

// Handle: pool index in the low byte (24 formations), a counter above it.
object_pool<ai_formation, 24, 1> Ai_formation_pool(&ai_formation::alloc, &ai_formation::release, 0); // 0x01D68900
object_pool<ai_formation_member, 180, 1> Ai_formation_member_pool(&ai_formation_member::alloc, nullptr, 0); // 0x01DCA890
static int Ai_formation_handle_counter; // 0x01D67748

// 0x00631EB0 (:118). LTCG: handle ECX.
static ai_formation* formation_from_handle(unsigned int handle)
{
    if (handle == 0xFFFFFFFF || (handle & 0xFF) >= Ai_formation_pool.get_capacity())
    {
        return nullptr;
    }
    ai_formation* formation = Ai_formation_pool.get_entry(handle & 0xFF);
    return formation->handle == handle ? formation : nullptr;
}

// 0x00631EF0 (:135)
static unsigned int get_new_formation_handle(const ai_formation* formation)
{
    if (formation == nullptr)
    {
        return 0xFFFFFFFF;
    }
    if (Ai_formation_handle_counter > 0xFFFFFD)
    {
        Ai_formation_handle_counter = 0;
    }
    const unsigned int counter = Ai_formation_handle_counter << 8;
    Ai_formation_handle_counter++;
    return static_cast<unsigned int>(Ai_formation_pool.get_index(formation)) | counter;
}

// 0x006A0B20 (:154): one obstruction check per frame, whichever member has waited longest.
static void do_obstruction_frame()
{
    int max_time_since = -1;
    ai_formation_member* member = nullptr;
    ai_formation* owner = nullptr;
    {
        csspinlock_scope lock(Ai_formation_pool.thread_access);
        for (ai_formation* formation = Ai_formation_pool.get_first(0); formation != nullptr; formation = Ai_formation_pool.get_next(formation))
        {
            ai_formation_member* candidate = formation->get_obstruction_check_member(max_time_since);
            if (candidate != nullptr)
            {
                member = candidate;
                owner = formation;
            }
        }
    }
    if (member != nullptr)
    {
        member->obstruction_period.set(500);
        owner->check_obstructions(member);
    }
}

// 0x006A0C00 (:205)
void ai_formation_level_init_pre()
{
    Ai_formation_handle_counter = 0;
    {
        csspinlock_scope lock(Ai_formation_pool.thread_access);
        for (ai_formation* formation = Ai_formation_pool.get_first(0); formation != nullptr; formation = Ai_formation_pool.free(formation))
        {
        }
    }
    csspinlock_scope lock(Ai_formation_member_pool.thread_access);
    for (ai_formation_member* member = Ai_formation_member_pool.get_first(0); member != nullptr; member = Ai_formation_member_pool.free(member))
    {
    }
}

// 0x006B7310 (:217)
void ai_formation_do_frame()
{
    {
        csspinlock_scope lock(Ai_formation_pool.thread_access);
        ai_formation* next = Ai_formation_pool.get_first(0);
        while (next != nullptr)
        {
            ai_formation* formation = next;
            next = Ai_formation_pool.get_next(formation);
            if (!formation->do_frame())
            {
                Ai_formation_pool.free(formation);
            }
        }
    }
    do_obstruction_frame();
}

// 0x006A0C90 (:265)
unsigned int ai_formation_follow(object* follower, const object* target, ai_formation_types type)
{
    if (follower == nullptr || target == nullptr || follower == target)
    {
        return 0xFFFFFFFF;
    }
    {
        csspinlock_scope lock(Ai_formation_pool.thread_access);
        for (ai_formation* formation = Ai_formation_pool.get_first(0); formation != nullptr; formation = Ai_formation_pool.get_next(formation))
        {
            if (formation->get_leader_handle() == target->get_handle())
            {
                return formation->add_follower(follower) ? formation->handle : 0xFFFFFFFF;
            }
        }
    }
    ai_formation* formation = Ai_formation_pool.alloc(0);
    if (formation == nullptr)
    {
        return 0xFFFFFFFF;
    }
    const unsigned int handle = get_new_formation_handle(formation);
    if (handle == 0xFFFFFFFF)
    {
        return 0xFFFFFFFF;
    }
    if (formation->init(handle, type, target, follower))
    {
        return formation->handle;
    }
    Ai_formation_pool.free(formation);
    return 0xFFFFFFFF;
}

// 0x0065E580 (:316)
int ai_formation_get_num_followers(unsigned int leader_handle)
{
    int count = 0;
    csspinlock_scope lock(Ai_formation_pool.thread_access);
    for (ai_formation* formation = Ai_formation_pool.get_first(0); formation != nullptr; formation = Ai_formation_pool.get_next(formation))
    {
        if (formation->get_leader_handle() == leader_handle)
        {
            count += formation->get_num_followers();
        }
    }
    return count;
}

// 0x0065E630 (:334)
unsigned int ai_formation_from_leader(unsigned int leader_handle)
{
    csspinlock_scope lock(Ai_formation_pool.thread_access);
    for (ai_formation* formation = Ai_formation_pool.get_first(0); formation != nullptr; formation = Ai_formation_pool.get_next(formation))
    {
        if (formation->get_leader_handle() == leader_handle)
        {
            return formation->handle;
        }
    }
    return 0xFFFFFFFF;
}

// 0x0065E6A0 (:350)
unsigned int ai_formation_from_follower(unsigned int follower_handle)
{
    csspinlock_scope lock(Ai_formation_pool.thread_access);
    for (ai_formation* formation = Ai_formation_pool.get_first(0); formation != nullptr; formation = Ai_formation_pool.get_next(formation))
    {
        if (formation->is_following(follower_handle))
        {
            return formation->handle;
        }
    }
    return 0xFFFFFFFF;
}

// 0x00631F40 (:367)
unsigned int ai_formation_get_leader_handle(unsigned int formation_handle)
{
    const ai_formation* formation = formation_from_handle(formation_handle);
    return formation != nullptr ? formation->get_leader_handle() : 0xFFFFFFFF;
}

// 0x0065E730 (:379): the leader, at index 0, is not a member here.
bool ai_formation_is_member(unsigned int formation_handle, unsigned int member_handle)
{
    const ai_formation* formation = formation_from_handle(formation_handle);
    return formation != nullptr && formation->is_formation_member(member_handle);
}

// 0x00631F80 (:392)
object* ai_formation_get_leader(unsigned int formation_handle)
{
    const ai_formation* formation = formation_from_handle(formation_handle);
    return formation != nullptr ? World.object_from_handle(formation->get_leader_handle()) : nullptr;
}

// 0x0065E790 (:402)
object* ai_formation_get_leader(const object* follower)
{
    if (follower == nullptr)
    {
        return nullptr;
    }
    const unsigned int formation_handle = ai_formation_from_follower(follower->get_handle());
    return formation_handle != 0xFFFFFFFF ? ai_formation_get_leader(formation_handle) : nullptr;
}

// 0x00631FC0 (:413)
bool ai_formation_is_leader(unsigned int leader_handle, unsigned int formation_handle)
{
    const ai_formation* formation = formation_from_handle(formation_handle);
    return formation != nullptr && formation->get_leader_handle() == leader_handle;
}

// 0x006B73E0 (:424)
bool ai_formation_remove_follower(unsigned int formation_handle, unsigned int follower_handle)
{
    ai_formation* formation = formation_from_handle(formation_handle);
    return formation != nullptr && formation->remove_follower(follower_handle);
}

// 0x0067B330 (:440)
bool ai_formation_remove_all_followers(unsigned int formation_handle)
{
    ai_formation* formation = formation_from_handle(formation_handle);
    if (formation == nullptr)
    {
        return false;
    }
    Ai_formation_pool.free(formation);
    return true;
}

// 0x0067B370 (:457)
const follow_information* ai_formation_get_follower_position(unsigned int formation_handle, const object* obj)
{
    const ai_formation* formation = formation_from_handle(formation_handle);
    return formation != nullptr ? formation->get_follow_position(obj) : nullptr;
}

// 0x0065E7C0 (:468)
bool ai_formation_is_following(const object* follower, unsigned int handle)
{
    if (follower == nullptr)
    {
        return false;
    }
    const ai_formation* formation = formation_from_handle(handle);
    return formation != nullptr && formation->is_following(follower->get_handle());
}

// 0x0065E830 (:481)
bool ai_formation_is_follower_of(const object* follower, const object* leader)
{
    csspinlock_scope lock(Ai_formation_pool.thread_access);
    for (ai_formation* formation = Ai_formation_pool.get_first(0); formation != nullptr; formation = Ai_formation_pool.get_next(formation))
    {
        if (formation->get_leader_handle() == leader->get_handle() && formation->is_following(follower->get_handle()))
        {
            return true;
        }
    }
    return false;
}

// 0x0065E8E0 (:515)
int ai_formation_collect_followers(const object* leader, unsigned int* followers, int max_collect, bool (*callback)(unsigned int, void*), void* callback_data)
{
    if (max_collect < 1)
    {
        return 0;
    }
    int count = 0;
    csspinlock_scope lock(Ai_formation_pool.thread_access);
    for (ai_formation* formation = Ai_formation_pool.get_first(0); formation != nullptr; formation = Ai_formation_pool.get_next(formation))
    {
        if (formation->get_leader_handle() == leader->get_handle())
        {
            count += formation->collect_followers(followers + count, max_collect - count, callback, callback_data);
            if (count == max_collect)
            {
                break;
            }
        }
    }
    return count;
}

// 0x00632000 (:539)
bool ai_formation_is_rigid(unsigned int handle)
{
    const ai_formation* formation = formation_from_handle(handle);
    return formation != nullptr && formation->is_rigid_formation();
}

// 0x00602C90 (.h:55)
follow_information::follow_information()
{
    reset();
}

// 0x005DDA80 (.h:58)
void follow_information::reset()
{
    ideal_formation_pos = ZERO_VECTOR;
    formation_pos = ZERO_VECTOR;
    formation_dir = ZERO_VECTOR;
    formation_speed = 0.0f;
    my_leader = nullptr;
    follow_mode = AI_FFM_INVALID;
    position_invalid = false;
}
