// Original: volition/rfg/code/ai/ai_defense_coordination_internal.cpp

#include "volition/rfg/code/ai/ai_defense_coordination_internal.h"

#include "volition/rfg/code/ai/ai_system.h"
#include "volition/rfg/code/ai/goals.h"
#include "volition/rfg/code/level/world.h"
#include "volition/rfg/code/objects/humans/npc.h"

ai_defense_object_manager Ai_defense_object_manager; // 0x01D9FA00

// 0x005DD3A0 (:62)
void ai_defense_target::init()
{
    target_handle = 0xFFFFFFFF;
    threatening_defender = 0xFFFFFFFF;
    accompanying_defender = 0xFFFFFFFF;
    stage_timer.set(2500); // ms
    arbitration_timer.set(0);
    last_in_area_time.set(0);
    no_path_timer.set(0);
    keepalive_timer.set(20000); // ms
    threaten_stage = AI_DEFENSE_THREATEN_NONE;
    flags = ai_defense_target_flags();
}

// 0x005DD400 (:76)
void ai_defense_target::release()
{
    handle = 0xFFFFFFFF;
}

// 0x005DD410 (:84)
void ai_defense_defender::init()
{
    defender_handle = 0xFFFFFFFF;
    current_target = 0xFFFFFFFF;
    keepalive_timer.set(3000); // ms
    failed_timer.set(0);
    flags = ai_defense_defender_flags();
}

// 0x005DD440 (:93)
void ai_defense_defender::release()
{
    handle = 0xFFFFFFFF;
}

// 0x0068E9A0 (:110)
ai_defense_object_manager::ai_defense_object_manager()
    : targets(&ai_defense_target::init, &ai_defense_target::release, 0)
    , defenders(&ai_defense_defender::init, &ai_defense_defender::release, 0)
{
}

// 0x00673C40
ai_defense_object_manager::~ai_defense_object_manager()
{
}

// 0x0068E9E0 (:116)
void ai_defense_object_manager::level_init()
{
    defense_handle_uid_counter = 0;
    targets.clear();
    defenders.clear();
}

// 0x0068EA70 (:131)
void ai_defense_object_manager::do_frame()
{
    defenders.thread_access.enter();
    for (ai_defense_defender* defender = defenders.get_first(0); defender != nullptr;)
    {
        ai_defense_defender* cur = defender;
        defender = defenders.get_next(cur);
        object* objp = cur->defender_handle != 0xFFFFFFFF ? World.object_from_handle(cur->defender_handle) : nullptr;
        if (objp == nullptr || objp->get_obj_type() != OT_HUMAN || objp->get_sub_type() != OT_SUB_HUMAN_NPC || objp->is_killed())
        {
            free_defender_internal(cur);
        }
        else if (static_cast<npc*>(objp)->get_ai()->planner.get_current_goal() == confront_civilian_goal::get_instance())
        {
            cur->keepalive_timer.set(3000); // ms
            ai_defense_target* target = target_from_handle(cur->current_target);
            if (target != nullptr)
            {
                target->keepalive_timer.set(20000); // ms
            }
        }
        else if (cur->keepalive_timer.elapsed())
        {
            free_defender_internal(cur);
        }
    }
    defenders.thread_access.leave();

    targets.thread_access.enter();
    for (ai_defense_target* target = targets.get_first(0); target != nullptr;)
    {
        ai_defense_target* cur = target;
        target = targets.get_next(cur);
        object* objp = cur->target_handle != 0xFFFFFFFF ? World.object_from_handle(cur->target_handle) : nullptr;
        if (objp == nullptr || objp->get_obj_type() != OT_HUMAN || objp->is_killed() || cur->keepalive_timer.elapsed())
        {
            free_target_internal(cur);
            continue;
        }
        if (!cur->flags.acknowledged && cur->stage_timer.elapsed())
        {
            cur->flags.acknowledged = true;
            if (cur->threaten_stage == AI_DEFENSE_THREATEN_NONE)
            {
                cur->threaten_stage = AI_DEFENSE_THREATEN_FIRST_WARNING;
                cur->flags.in_progress = false;
                cur->stage_timer.set(500); // ms
            }
        }
        if (defender_from_handle(cur->threatening_defender) == nullptr)
        {
            cur->flags.in_progress = false;
            cur->threatening_defender = 0xFFFFFFFF;
        }
        if (defender_from_handle(cur->accompanying_defender) == nullptr)
        {
            cur->accompanying_defender = 0xFFFFFFFF;
        }
    }
    targets.thread_access.leave();
}

// 0x0064D3C0 (:224)
ai_defense_target* ai_defense_object_manager::get_new_target()
{
    ai_defense_target* target = targets.alloc(0);
    if (target == nullptr)
    {
        return nullptr;
    }
    target->handle = get_new_defense_assignment_handle(targets.get_index(target));
    return target;
}

// 0x0064D420 (:239)
ai_defense_defender* ai_defense_object_manager::get_new_defender()
{
    ai_defense_defender* defender = defenders.alloc(0);
    if (defender == nullptr)
    {
        return nullptr;
    }
    defender->handle = get_new_defense_assignment_handle(defenders.get_index(defender));
    return defender;
}

// 0x0064D480 (:256)
ai_defense_defender* ai_defense_object_manager::find_defender(unsigned int defender_handle)
{
    csspinlock_scope lock(defenders.thread_access);
    for (ai_defense_defender* defender = defenders.get_first(0); defender != nullptr; defender = defenders.get_next(defender))
    {
        if (defender->defender_handle == defender_handle)
        {
            return defender;
        }
    }
    return nullptr;
}

// 0x0064D540 (:269)
ai_defense_target* ai_defense_object_manager::find_target(unsigned int target_handle)
{
    csspinlock_scope lock(targets.thread_access);
    for (ai_defense_target* target = targets.get_first(0); target != nullptr; target = targets.get_next(target))
    {
        if (target->target_handle == target_handle)
        {
            return target;
        }
    }
    return nullptr;
}

// 0x005FE380 (:282)
ai_defense_target* ai_defense_object_manager::target_from_handle(unsigned int handle)
{
    if (handle == 0xFFFFFFFF)
    {
        return nullptr;
    }
    // The low 7 bits are the pool index (get_new_defense_assignment_handle). 10 is the pool's size.
    const unsigned int index = handle & 0x7F;
    if (index >= 10)
    {
        return nullptr;
    }
    ai_defense_target* target = targets.get_entry(index);
    return target != nullptr && target->get_handle() == handle ? target : nullptr;
}

// 0x005FE3B0 (:307)
ai_defense_defender* ai_defense_object_manager::defender_from_handle(unsigned int handle)
{
    if (handle == 0xFFFFFFFF)
    {
        return nullptr;
    }
    // The low 7 bits are the pool index. 50 is the pool's size.
    const unsigned int index = handle & 0x7F;
    if (index >= 50)
    {
        return nullptr;
    }
    ai_defense_defender* defender = defenders.get_entry(index);
    return defender != nullptr && defender->get_handle() == handle ? defender : nullptr;
}

// 0x00673C50 (:335)
void ai_defense_object_manager::free_target_internal(ai_defense_target* target)
{
    if (target != nullptr && target->get_handle() != 0xFFFFFFFF)
    {
        targets.free(target);
    }
}

// 0x00673C70 (:343)
void ai_defense_object_manager::free_defender_internal(ai_defense_defender* defender)
{
    if (defender != nullptr && defender->get_handle() != 0xFFFFFFFF)
    {
        defenders.free(defender);
    }
}

// 0x005DD450 (:354)
unsigned int ai_defense_object_manager::get_new_defense_assignment_handle(int index)
{
    if ((unsigned int)index < 0x7F)
    {
        const unsigned int uid = defense_handle_uid_counter;
        defense_handle_uid_counter = (uid + 1) & 0x1FFFFFF; // 25 bits above the 7 index bits
        return uid << 7 | index;
    }
    return 0xFFFFFFFF;
}

// 0x005DC610 (.h:44)
unsigned int ai_defense_target::get_handle() const
{
    return handle;
}

// 0x005FB570 (.h:45)
ai_defense_target::ai_defense_target()
    : handle(0xFFFFFFFF)
{
}

// 0x005DC620 (.h:71)
unsigned int ai_defense_defender::get_handle() const
{
    return handle;
}

// 0x005FB590 (.h:73)
ai_defense_defender::ai_defense_defender()
    : handle(0xFFFFFFFF)
{
}

// 0x005FB5A0 (.h:108)
ai_defense_defender* ai_defense_object_manager::get_first_defender()
{
    return defenders.get_first(0);
}

// 0x0064C960 (.h:109)
ai_defense_defender* ai_defense_object_manager::get_next_defender(const ai_defense_defender* prev_defender)
{
    return defenders.get_next(prev_defender);
}

// 0x005FB5D0 (.h:111)
ai_defense_target* ai_defense_object_manager::get_first_target()
{
    return targets.get_first(0);
}

// 0x0064C970 (.h:112)
ai_defense_target* ai_defense_object_manager::get_next_target(const ai_defense_target* prev_target)
{
    return targets.get_next(prev_target);
}
