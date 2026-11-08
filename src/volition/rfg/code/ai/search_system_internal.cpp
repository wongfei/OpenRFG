// Original: volition/rfg/code/ai/search_system_internal.cpp

#include "volition/rfg/code/ai/search_system_internal.h"

#include "volition/rfg/code/ai/ai_system.h"
#include "volition/rfg/code/ai/search_actions.h"
#include "volition/rfg/code/level/world.h"
#include "volition/rfg/code/objects/humans/npc.h"

ai_search_object_manager Ai_search_object_manager; // 0x01D72710

static const int SEARCHER_KEEPALIVE_MS = 15000;
static const int LOCATION_RETRY_MS = 20000;
static const int TARGET_EXPIRE_MS = 5000;
static const unsigned int MAX_LOCATIONS = 100; // the pool sizes
static const unsigned int MAX_TARGETS = 12;
static const unsigned int MAX_SEARCHERS = 25;

// 0x005F1770
ai_search_location_info::ai_search_location_info()
{
}

// 0x0066B460
ai_search_searcher_info::ai_search_searcher_info()
{
}

// 0x0066B480
ai_search_searcher_info::~ai_search_searcher_info()
{
}

// 0x005F1730 (.h:40)
ai_search_states ai_search_location::get_state() const
{
    return state;
}

// 0x005F1740 (.h:41)
unsigned int ai_search_location::get_handle() const
{
    return handle;
}

// 0x005F1750 (.h:42)
int ai_search_location::get_num_searchers() const
{
    return searcher_ref_count;
}

// 0x005F1960 (:60)
void ai_search_location::set_state(ai_search_states new_state)
{
    if (state != new_state)
    {
        state = new_state;
        if (new_state == AI_SS_FAILED)
        {
            retry_timer.set(LOCATION_RETRY_MS);
        }
        else
        {
            retry_timer.invalidate();
        }
    }
}

// 0x00641810 (.h:45)
ai_search_location::ai_search_location()
    : handle(0xFFFFFFFF)
{
}

// 0x00641820 (.h:57)
void ai_search_location::init()
{
    searcher_ref_count = 0;
    search_ref_count = 0;
    if (state != AI_SS_NOT_SEARCHED)
    {
        state = AI_SS_NOT_SEARCHED;
        retry_timer.invalidate();
    }
}

// 0x005F1760 (.h:58)
void ai_search_location::release()
{
    handle = 0xFFFFFFFF;
}

// 0x0066B920 (:97)
bool ai_search_target::get_locations(farray<ai_search_location*,10>& list_out) const
{
    for (int i = 0; i < locations.size(); i++)
    {
        ai_search_location* location = Ai_search_object_manager.location_from_handle(locations[i]);
        if (location != nullptr)
        {
            if (list_out.full())
            {
                return false;
            }
            list_out.add(location);
        }
    }
    return true;
}

// 0x00641960 (:118)
bool ai_search_target::has_location(const ai_search_location* location) const
{
    if (location == nullptr)
    {
        return false;
    }
    for (int i = 0; i < locations.size(); i++)
    {
        if (locations[i] == location->get_handle())
        {
            return true;
        }
    }
    return false;
}

// 0x0066B430 (.h:80)
ai_search_target::ai_search_target()
    : handle(0xFFFFFFFF)
{
}

// 0x00641840 (.h:92)
void ai_search_target::init()
{
    searcher_ref_count = 0;
    locations.remove_all();
    expire_timer.invalidate();
}

// 0x005F1780 (.h:93)
void ai_search_target::release()
{
    handle = 0xFFFFFFFF;
}

// 0x005F1990 (:134)
void ai_search_searcher::reset_keepalive()
{
    keepalive.set(SEARCHER_KEEPALIVE_MS);
}

// 0x0066B9B0 (:139)
ai_search_location* ai_search_searcher::get_location() const
{
    return Ai_search_object_manager.location_from_handle(current_location);
}

// 0x0066B9F0 (:144)
ai_search_target* ai_search_searcher::get_target() const
{
    return Ai_search_object_manager.search_from_handle(current_target);
}

// 0x00684A40 (.h:106)
ai_search_searcher::ai_search_searcher()
{
    info.search_handle = 0xFFFFFFFF;
}

// 0x005F1790 (.h:116)
void ai_search_searcher::init()
{
    current_location = 0xFFFFFFFF;
    current_target = 0xFFFFFFFF;
    keepalive.set(0);
}

// 0x005F17B0 (.h:117)
void ai_search_searcher::release()
{
    info.search_handle = 0xFFFFFFFF;
}

// 0x006BF2F0 (:162)
ai_search_object_manager::ai_search_object_manager()
    : locations(&ai_search_location::init, &ai_search_location::release, 0)
    , targets(&ai_search_target::init, &ai_search_target::release, 0)
    , searchers(&ai_search_searcher::init, &ai_search_searcher::release, 0)
{
}

// 0x006853F0
ai_search_object_manager::~ai_search_object_manager()
{
}

// 0x006A9910 (:168)
void ai_search_object_manager::level_init()
{
    search_handle_uid_counter = 0;
    locations.clear();
    targets.clear();
    searchers.clear();
}

// 0x006A99F0 (:185)
void ai_search_object_manager::do_frame()
{
    searchers.thread_access.enter();
    for (ai_search_searcher* searcher = searchers.get_first(0); searcher != nullptr;)
    {
        ai_search_searcher* cur = searcher;
        searcher = searchers.get_next(cur);
        const object* objp = cur->info.searcher_handle != 0xFFFFFFFF ? World.object_from_handle(cur->info.searcher_handle) : nullptr;
        if (objp == nullptr || objp->get_obj_type() != OT_HUMAN || objp->get_sub_type() != OT_SUB_HUMAN_NPC || objp->is_killed() || cur->keepalive.elapsed())
        {
            free_searcher_internal(cur);
            continue;
        }
        const ai_system* ai = static_cast<const npc*>(objp)->get_ai();
        if (ai->planner.get_current_goal() == search_goal::get_instance() && ai->desired_search_handle == cur->info.search_handle)
        {
            cur->keepalive.set(SEARCHER_KEEPALIVE_MS);
        }
        if (location_from_handle(cur->current_location) == nullptr || search_from_handle(cur->current_target) == nullptr)
        {
            free_searcher_internal(cur);
        }
    }
    searchers.thread_access.leave();

    targets.thread_access.enter();
    for (ai_search_target* target = targets.get_first(0); target != nullptr;)
    {
        ai_search_target* cur = target;
        target = targets.get_next(cur);
        const object* objp = cur->target_handle != 0xFFFFFFFF ? World.object_from_handle(cur->target_handle) : nullptr;
        if (objp == nullptr || objp->get_obj_type() != OT_HUMAN || objp->is_killed())
        {
            free_target_internal(cur);
        }
        else if (cur->searcher_ref_count == 0 && !cur->expire_timer.valid())
        {
            cur->expire_timer.set(TARGET_EXPIRE_MS);
        }
        else if (cur->expire_timer.valid() && cur->expire_timer.elapsed())
        {
            free_target_internal(cur);
        }
    }
    targets.thread_access.leave();

    locations.thread_access.enter();
    for (ai_search_location* location = locations.get_first(0); location != nullptr;)
    {
        ai_search_location* cur = location;
        location = locations.get_next(cur);
        if (cur->search_ref_count == 0 && cur->searcher_ref_count == 0)
        {
            free_location_internal(cur);
        }
        else if (cur->retry_timer.valid() && cur->retry_timer.elapsed())
        {
            cur->set_state(AI_SS_NOT_SEARCHED);
        }
    }
    locations.thread_access.leave();
}

// 0x0066BA20 (:280)
ai_search_target* ai_search_object_manager::get_new_search()
{
    ai_search_target* target = targets.alloc(0);
    if (target != nullptr)
    {
        target->handle = get_new_search_handle(targets.get_index(target));
    }
    return target;
}

// 0x0066BA80 (:295)
ai_search_location* ai_search_object_manager::get_new_location()
{
    ai_search_location* location = locations.alloc(0);
    if (location != nullptr)
    {
        location->handle = get_new_search_handle(locations.get_index(location));
    }
    return location;
}

// 0x0066BAE0 (:310)
ai_search_searcher* ai_search_object_manager::get_new_searcher()
{
    ai_search_searcher* searcher = searchers.alloc(0);
    if (searcher != nullptr)
    {
        searcher->info.search_handle = get_new_search_handle(searchers.get_index(searcher));
    }
    return searcher;
}

// 0x00685400 (:327)
bool ai_search_object_manager::add_searcher_to_search(ai_search_searcher* searcher, ai_search_target* target)
{
    if (searcher == nullptr || searcher->info.search_handle == 0xFFFFFFFF || target == nullptr || target->handle == 0xFFFFFFFF)
    {
        return false;
    }
    ai_search_target* old_target = searcher->get_target();
    if (old_target != target)
    {
        if (old_target != nullptr)
        {
            remove_searcher_from_search(searcher, old_target, false);
        }
        searcher->current_target = target->handle;
        target->searcher_ref_count++;
    }
    ai_search_location* location = searcher->get_location();
    if (location == nullptr)
    {
        searcher->current_location = 0xFFFFFFFF;
    }
    else if (!target->has_location(location))
    {
        unassign_searcher_location(searcher, location);
    }
    return true;
}

// 0x0066BB40 (:364)
bool ai_search_object_manager::add_location_to_search(ai_search_location* location, ai_search_target* target)
{
    if (location == nullptr || location->handle == 0xFFFFFFFF || target == nullptr || target->handle == 0xFFFFFFFF)
    {
        return false;
    }
    if (!target->has_location(location))
    {
        if (target->locations.full())
        {
            return false;
        }
        location->search_ref_count++;
        target->locations.add(location->handle);
    }
    return true;
}

// 0x0066BBA0 (:392)
bool ai_search_object_manager::assign_searcher_location(ai_search_searcher* searcher, ai_search_location* location)
{
    if (searcher == nullptr || searcher->info.search_handle == 0xFFFFFFFF || location == nullptr || location->handle == 0xFFFFFFFF)
    {
        return false;
    }
    ai_search_location* old_location = location_from_handle(searcher->current_location);
    if (old_location == location)
    {
        return true;
    }
    if (old_location != nullptr)
    {
        unassign_searcher_location(searcher, old_location);
    }
    searcher->current_location = location->handle;
    location->searcher_ref_count++;
    return true;
}

// 0x00641990 (:417)
bool ai_search_object_manager::remove_location_from_search(ai_search_location* location, ai_search_target* target)
{
    if (location == nullptr || location->handle == 0xFFFFFFFF || target == nullptr || target->handle == 0xFFFFFFFF)
    {
        return false;
    }
    for (int i = 0; i < target->locations.size(); i++)
    {
        if (target->locations[i] == location->handle)
        {
            target->locations.remove_unsorted(i);
            location->search_ref_count--;
            return true;
        }
    }
    return false;
}

// 0x006854A0 (:439)
bool ai_search_object_manager::unassign_searcher(ai_search_searcher* searcher)
{
    if (searcher == nullptr)
    {
        return false;
    }
    ai_search_target* target = search_from_handle(searcher->current_target);
    if (target == nullptr || searcher->info.search_handle == 0xFFFFFFFF)
    {
        return false;
    }
    ai_search_location* location = location_from_handle(searcher->current_location);
    if (location != nullptr)
    {
        unassign_searcher_location(searcher, location);
    }
    searcher->current_target = 0xFFFFFFFF;
    target->searcher_ref_count--;
    return true;
}

// 0x0066BC10 (:452)
bool ai_search_object_manager::remove_searcher_from_search(ai_search_searcher* searcher, ai_search_target* target, bool clear_location)
{
    if (searcher == nullptr || searcher->info.search_handle == 0xFFFFFFFF || target == nullptr || target->handle == 0xFFFFFFFF || searcher->current_target != target->handle)
    {
        return false;
    }
    if (clear_location)
    {
        ai_search_location* location = location_from_handle(searcher->current_location);
        if (location != nullptr)
        {
            unassign_searcher_location(searcher, location);
        }
    }
    searcher->current_target = 0xFFFFFFFF;
    target->searcher_ref_count--;
    return true;
}

// 0x005F19B0 (:471)
void ai_search_object_manager::unassign_searcher_location(ai_search_searcher* searcher, ai_search_location* location)
{
    if (searcher != nullptr && searcher->info.search_handle != 0xFFFFFFFF && location != nullptr && location->handle != 0xFFFFFFFF && searcher->current_location == location->handle)
    {
        searcher->current_location = 0xFFFFFFFF;
        location->searcher_ref_count--;
    }
}

// 0x0066BC80 (:491)
ai_search_searcher* ai_search_object_manager::find_searcher(unsigned int searcher_handle)
{
    csspinlock_scope lock(searchers.thread_access);
    for (ai_search_searcher* searcher = searchers.get_first(0); searcher != nullptr; searcher = searchers.get_next(searcher))
    {
        if (searcher->info.searcher_handle == searcher_handle)
        {
            return searcher;
        }
    }
    return nullptr;
}

// 0x0066BCF0 (:504)
ai_search_target* ai_search_object_manager::find_target(unsigned int target_handle)
{
    csspinlock_scope lock(targets.thread_access);
    for (ai_search_target* target = targets.get_first(0); target != nullptr; target = targets.get_next(target))
    {
        if (target->target_handle == target_handle)
        {
            return target;
        }
    }
    return nullptr;
}

// 0x006419F0 (:517). The low 7 bits of a handle are the pool index (get_new_search_handle).
ai_search_target* ai_search_object_manager::search_from_handle(unsigned int handle)
{
    if (handle == 0xFFFFFFFF || (handle & 0x7F) >= MAX_TARGETS)
    {
        return nullptr;
    }
    ai_search_target* target = targets.get_entry(handle & 0x7F);
    return target != nullptr && target->handle == handle ? target : nullptr;
}

// 0x00641A20 (:542)
ai_search_location* ai_search_object_manager::location_from_handle(unsigned int handle)
{
    if (handle == 0xFFFFFFFF || (handle & 0x7F) >= MAX_LOCATIONS)
    {
        return nullptr;
    }
    ai_search_location* location = locations.get_entry(handle & 0x7F);
    return location != nullptr && location->handle == handle ? location : nullptr;
}

// 0x00641A50 (:567)
ai_search_searcher* ai_search_object_manager::searcher_from_handle(unsigned int handle)
{
    if (handle == 0xFFFFFFFF || (handle & 0x7F) >= MAX_SEARCHERS)
    {
        return nullptr;
    }
    ai_search_searcher* searcher = searchers.get_entry(handle & 0x7F);
    return searcher != nullptr && searcher->info.search_handle == handle ? searcher : nullptr;
}

// 0x005F17C0 (.h:153)
cs_spinlock& ai_search_object_manager::get_searcher_cs_wrapper()
{
    return searchers.thread_access;
}

// 0x00641880 (.h:154)
ai_search_searcher* ai_search_object_manager::get_first_searcher()
{
    return searchers.get_first(0);
}

// 0x0066B490 (.h:155)
ai_search_searcher* ai_search_object_manager::get_next_searcher(const ai_search_searcher* prev_searcher)
{
    return searchers.get_next(prev_searcher);
}

// 0x005F17D0 (.h:156)
cs_spinlock& ai_search_object_manager::get_locations_cs_wrapper()
{
    return locations.thread_access;
}

// 0x006418B0 (.h:157)
ai_search_location* ai_search_object_manager::get_first_location()
{
    return locations.get_first(0);
}

// 0x0066B4A0 (.h:158)
ai_search_location* ai_search_object_manager::get_next_location(const ai_search_location* prev_location)
{
    return locations.get_next(prev_location);
}

// 0x00685530 (:595)
void ai_search_object_manager::free_location_internal(ai_search_location* location)
{
    if (location != nullptr && location->handle != 0xFFFFFFFF)
    {
        locations.free(location);
    }
}

// 0x00685550 (:604)
void ai_search_object_manager::free_target_internal(ai_search_target* target)
{
    if (target == nullptr || target->handle == 0xFFFFFFFF)
    {
        return;
    }
    for (int i = 0; i < target->locations.size(); i++)
    {
        ai_search_location* location = location_from_handle(target->locations[i]);
        if (location != nullptr)
        {
            location->search_ref_count--;
        }
    }
    target->locations.remove_all();
    targets.free(target);
}

// 0x006855B0 (:624)
void ai_search_object_manager::free_searcher_internal(ai_search_searcher* searcher)
{
    if (searcher == nullptr || searcher->info.search_handle == 0xFFFFFFFF)
    {
        return;
    }
    ai_search_target* target = search_from_handle(searcher->current_target);
    if (target != nullptr)
    {
        remove_searcher_from_search(searcher, target, true);
    }
    else
    {
        ai_search_location* location = location_from_handle(searcher->current_location);
        if (location != nullptr)
        {
            unassign_searcher_location(searcher, location);
        }
    }
    searchers.free(searcher);
}

// 0x005F19F0 (:650)
unsigned int ai_search_object_manager::get_new_search_handle(int index)
{
    if ((unsigned int)index < 0x7F)
    {
        const unsigned int uid = search_handle_uid_counter;
        search_handle_uid_counter = (uid + 1) & 0x1FFFFFF; // 25 bits above the 7 index bits
        return uid << 7 | index;
    }
    return 0xFFFFFFFF;
}

// 0x005F17E0 (.h:208)
const ai_search_searcher* searcher_info_to_data(const ai_search_searcher_info* info)
{
    return reinterpret_cast<const ai_search_searcher*>(info);
}

// 0x005F17F0 (.h:209)
ai_search_searcher_info* searcher_data_to_info(ai_search_searcher* node)
{
    return &node->info;
}
