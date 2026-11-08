// Original: volition/rfg/code/ai/search_system.cpp

#include "volition/rfg/code/ai/search_system.h"

#include "volition/rfg/code/ai/ai_building_logic.h"
#include "volition/rfg/code/ai/ai_system.h"
#include "volition/rfg/code/ai/beliefs.h"
#include "volition/rfg/code/ai/search_system_internal.h"
#include "volition/rfg/code/destruction/rfg_mover.h"
#include "volition/rfg/code/level/world.h"
#include "volition/rfg/code/objects/humans/human.h"
#include "volition/rfg/code/objects/humans/npc.h"
#include "volition/rfg/code/objects/object_cover_node.h"
#include "volition/rfg/code/objects/object_navpoint.h"
#include "volition/rfg/code/pathfinding/nav_cell.h"

static const float SAME_EXTERNAL_LOCATION_DIST_SQ = 56.25f; // 7.5 m
static const float BUILDING_SEARCH_DIST = 15.0f; // m
static const signed char NAVPOINT_TYPE_TRANSITION = 11; // numeric in rfg.exe, name not recovered
static const int MAX_CANDIDATE_BUILDINGS = 9; // one location of 10 is the outside area

// 0x005F1800 (:78)
bool locations_are_equivalent(const ai_search_location_info& first, const ai_search_location_info& second)
{
    if (first.search_type != second.search_type)
    {
        return false;
    }
    if (first.search_type == AI_ST_EXTERNAL_AREA)
    {
        return vec_dist_squared(first.pos, second.pos) < SAME_EXTERNAL_LOCATION_DIST_SQ;
    }
    return first.search_obj == second.search_obj;
}

// 0x005F1850 (:93)
bool building_collect_cb(object* hit_objp, unsigned int, const void*)
{
    const rfg_mover* building = static_cast<const rfg_mover*>(hit_objp);
    return building->flags.is_ai_building && !building->flags.fully_destroyed && !building->flags.is_ai_subpiece;
}

// 0x005F1880 (:98)
bool building_has_transition_cb(object* hit_objp, unsigned int, const void*)
{
    return static_cast<const navpoint*>(hit_objp)->type == NAVPOINT_TYPE_TRANSITION;
}

// 0x0066B4B0 (:110). Sorted by distance, nearest first.
static int get_search_candidate_buildings(ai_search_building_candidate* building_candidates, const vector& pos)
{
    int num_candidates = 0;
    unsigned int handles[25];
    const int num = World.collect_objects<rfg_mover>(pos, BUILDING_SEARCH_DIST, handles, 25, building_collect_cb, nullptr);
    for (int i = 0; i < num; i++)
    {
        rfg_mover* building = handles[i] != 0xFFFFFFFF ? World.object_from_handle<rfg_mover>(handles[i]) : nullptr;
        float dist_sq = 0.0f;
        ai_building_find_dist_squared(dist_sq, pos, building, 0.0f);
        // Original bug kept: a squared distance compared with a distance, so only about 3.9 m away count.
        if (dist_sq > BUILDING_SEARCH_DIST)
        {
            continue;
        }
        farray<navpoint*, 1> transitions;
        ai_building_collect_navpoints(transitions, building, true, building_has_transition_cb, nullptr);
        if (transitions.size() <= 0)
        {
            continue;
        }
        int insert_at = 0;
        while (insert_at < num_candidates && building_candidates[insert_at].dist_sq <= dist_sq)
        {
            insert_at++;
        }
        if (insert_at < num_candidates)
        {
            num_candidates = num_candidates + 1 > MAX_CANDIDATE_BUILDINGS ? MAX_CANDIDATE_BUILDINGS : num_candidates + 1;
            for (int j = num_candidates - 1; j > insert_at; j--)
            {
                building_candidates[j] = building_candidates[j - 1];
            }
            building_candidates[insert_at].building = building;
            building_candidates[insert_at].dist_sq = dist_sq;
        }
        else if (num_candidates < MAX_CANDIDATE_BUILDINGS)
        {
            building_candidates[num_candidates].building = building;
            building_candidates[num_candidates].dist_sq = dist_sq;
            num_candidates++;
        }
    }
    return num_candidates;
}

// 0x00684A60 (:169). The terrain height rfg.exe reads when the best guess is in a building and on a nav cell is never used.
static void update_search_locations(ai_search_target* target)
{
    ai_search_building_candidate candidates[MAX_CANDIDATE_BUILDINGS];
    const int num_candidates = get_search_candidate_buildings(candidates, target->best_guess_position);
    farray<ai_search_location_info, 10> wanted;
    ai_search_location_info outside;
    outside.search_type = AI_ST_EXTERNAL_AREA;
    outside.search_obj = 0xFFFFFFFF;
    outside.pos = target->best_guess_position;
    wanted.add(outside);
    for (int i = 0; i < num_candidates && !wanted.full(); i++)
    {
        ai_search_location_info interior;
        interior.search_type = AI_ST_BUILDING_INTERIOR;
        interior.search_obj = candidates[i].building->get_handle();
        interior.pos = candidates[i].building->pos;
        wanted.add(interior);
    }
    farray<ai_search_location*, 10> current;
    target->get_locations(current);
    Ai_search_object_manager.get_locations_cs_wrapper().enter();
    for (ai_search_location* location = Ai_search_object_manager.get_first_location(); location != nullptr;)
    {
        ai_search_location* cur = location;
        location = Ai_search_object_manager.get_next_location(cur);
        const bool in_search = current.find(cur, false) != -1;
        bool found = false;
        for (int i = wanted.size() - 1; i >= 0; i--)
        {
            if (locations_are_equivalent(cur->info, wanted[i]))
            {
                wanted.remove_unsorted(i);
                found = true;
            }
        }
        if (found)
        {
            if (!in_search)
            {
                Ai_search_object_manager.add_location_to_search(cur, target);
            }
        }
        else if (in_search)
        {
            Ai_search_object_manager.remove_location_from_search(cur, target);
        }
    }
    Ai_search_object_manager.get_locations_cs_wrapper().leave();
    for (int i = 0; i < wanted.size(); i++)
    {
        ai_search_location* location = Ai_search_object_manager.get_new_location();
        if (location != nullptr)
        {
            location->info = wanted[i];
            Ai_search_object_manager.add_location_to_search(location, target);
        }
    }
}

// 0x00684F90 (:291)
static void update_existing_search(ai_search_target* target, const human* target_hp, const vector& new_pos)
{
    if (!target->position_update_timer.elapsed())
    {
        return;
    }
    const float old_dist = vec_dist(target->best_guess_position, target_hp->pos);
    if (vec_dist(new_pos, target_hp->pos) <= old_dist - 15.0f) // m
    {
        target->best_guess_position = new_pos;
        target->position_update_timer.set(10000); // ms
        update_search_locations(target);
    }
}

// 0x00685000 (:324). LTCG: target EAX, searcher on the stack.
static bool generate_search_assignment(ai_search_searcher* searcher, ai_search_target* target)
{
    if (searcher == nullptr || target == nullptr)
    {
        return false;
    }
    ai_search_location* current = searcher->get_location();
    if (current != nullptr)
    {
        if (current->get_state() == AI_SS_NOT_SEARCHED || current->get_state() == AI_SS_IN_PROGRESS)
        {
            return true;
        }
        Ai_search_object_manager.unassign_searcher_location(searcher, current);
    }
    const object* objp = searcher->info.searcher_handle != 0xFFFFFFFF ? World.object_from_handle(searcher->info.searcher_handle) : nullptr;
    if (objp == nullptr || objp->get_obj_type() != OT_HUMAN || objp->get_sub_type() != OT_SUB_HUMAN_NPC)
    {
        return false;
    }
    const npc* np = static_cast<const npc*>(objp);
    rfg_mover* building = nullptr;
    const bool in_building = np->get_ai()->perception.is_in_closest_building(np, &building, 0.0f, 1.25f);
    farray<ai_search_location*, 10> locations;
    target->get_locations(locations);
    ai_search_location* best = nullptr;
    float best_score = -1.0f;
    for (int i = 0; i < locations.size(); i++)
    {
        ai_search_location* location = locations[i];
        const ai_search_states state = location->get_state();
        if (state == AI_SS_SEARCHED || state == AI_SS_FAILED)
        {
            continue;
        }
        const bool interior = location->info.search_type == AI_ST_BUILDING_INTERIOR;
        if (interior && state == AI_SS_IN_PROGRESS)
        {
            continue;
        }
        // The location with the fewest searchers; a building counts a little against, unless the npc is in it.
        float score = (float)location->get_num_searchers();
        if (interior)
        {
            score += in_building && location->info.search_obj == building->get_handle() ? -0.3f : 0.3f;
        }
        if (best == nullptr || score < best_score)
        {
            best = location;
            best_score = score;
        }
    }
    if (best == nullptr)
    {
        return false;
    }
    Ai_search_object_manager.assign_searcher_location(searcher, best);
    if (best->get_state() == AI_SS_NOT_SEARCHED)
    {
        best->set_state(AI_SS_IN_PROGRESS);
    }
    if (best->info.search_type != AI_ST_EXTERNAL_AREA)
    {
        searcher->info.location_history.clear();
    }
    return true;
}

// 0x006A9670 (:427)
static void do_search_success(ai_search_searcher* searcher)
{
    ai_search_location* location = searcher->get_location();
    if (location != nullptr)
    {
        location->set_state(AI_SS_SEARCHED);
    }
    Ai_search_object_manager.unassign_searcher(searcher);
}

// 0x006A96D0 (:441). LTCG: searcher ESI.
static void do_search_failure(ai_search_searcher* searcher)
{
    ai_search_location* location = searcher->get_location();
    if (location != nullptr)
    {
        location->set_state(AI_SS_FAILED);
    }
    Ai_search_object_manager.unassign_searcher(searcher);
}

// 0x00641850
ai_search_location_history::ai_search_location_history()
{
}

// 0x00641870
ai_search_location_history::~ai_search_location_history()
{
}

// 0x006418D0 (:462)
void ai_search_location_history::clear()
{
    old.remove_all();
    has_current = false;
}

// 0x005F18A0 (:468)
void ai_search_location_history::set_current(const vector& pos, const vector& los_pos, const nav_cell* ncp, const cover_node* c_node)
{
    current_target = pos;
    los_position = los_pos;
    navcell = nav_cell_get_handle(ncp);
    has_current = true;
    cover = c_node != nullptr ? c_node->get_handle() : 0xFFFFFFFF;
}

// 0x006418F0 (:478)
void ai_search_location_history::finish_current(const vector& npc_pos)
{
    if (has_current)
    {
        old.add(npc_pos);
        has_current = false;
    }
}

// 0x005F1900 (:492)
void ai_search_location_history::abandon_current()
{
    has_current = false;
}

// 0x005F1910 (:498)
void ai_search_location_history::abandon_cover_node()
{
    cover = 0xFFFFFFFF;
}

// 0x006BB0A0 (:507)
void ai_search_system_level_init_pre()
{
    Ai_search_object_manager.level_init();
}

// 0x006BB0B0 (:513)
void ai_search_system_do_frame()
{
    Ai_search_object_manager.do_frame();
}

// 0x006851F0 (:537)
bool ai_search_is_anyone_searching_for_target(const human* target_hp)
{
    return target_hp != nullptr && Ai_search_object_manager.find_target(target_hp->get_handle()) != nullptr;
}

// 0x00685210 (:551)
bool ai_search_is_searching(const npc* np, const human* target_hp)
{
    if (np == nullptr)
    {
        return false;
    }
    const ai_search_searcher* searcher = Ai_search_object_manager.find_searcher(np->get_handle());
    if (searcher == nullptr)
    {
        return false;
    }
    if (target_hp != nullptr)
    {
        const ai_search_target* target = searcher->get_target();
        return target != nullptr && target->target_handle == target_hp->get_handle();
    }
    return true;
}

// 0x00685250 (:581)
bool ai_search_is_fully_populated(const human* target_hp)
{
    // Original bug kept: rfg.exe walks the locations looking for one with no searchers, then answers true either way.
    return target_hp != nullptr && Ai_search_object_manager.find_target(target_hp->get_handle()) != nullptr;
}

// 0x006852D0 (:613)
bool ai_search_get_best_guess_location(vector& pos, const human* target_hp)
{
    if (target_hp == nullptr)
    {
        return false;
    }
    const ai_search_target* target = Ai_search_object_manager.find_target(target_hp->get_handle());
    if (target == nullptr)
    {
        return false;
    }
    pos = target->best_guess_position;
    return true;
}

// 0x006A9730 (:637)
unsigned int ai_search_request_search(const npc* np, const human* target_hp)
{
    const unsigned int target_handle = target_hp->get_handle();
    const ai_belief* belief = np->get_ai()->perception.get_belief(AI_BELIEF_ENEMY_EXISTS, target_handle);
    if (belief == nullptr)
    {
        return 0xFFFFFFFF;
    }
    ai_search_target* target = Ai_search_object_manager.find_target(target_handle);
    if (target == nullptr)
    {
        target = Ai_search_object_manager.get_new_search();
        if (target == nullptr)
        {
            return 0xFFFFFFFF;
        }
        target->best_guess_position = belief->pos;
        target->position_update_timer.set(10000); // ms
        target->target_handle = target_handle;
        update_search_locations(target);
    }
    else
    {
        update_existing_search(target, target_hp, belief->pos);
    }
    ai_search_searcher* searcher = Ai_search_object_manager.find_searcher(np->get_handle());
    if (searcher == nullptr)
    {
        searcher = Ai_search_object_manager.get_new_searcher();
        if (searcher == nullptr)
        {
            return 0xFFFFFFFF;
        }
        searcher->info.searcher_handle = np->get_handle();
        searcher->reset_keepalive();
    }
    if (!Ai_search_object_manager.add_searcher_to_search(searcher, target))
    {
        return 0xFFFFFFFF;
    }
    if (generate_search_assignment(searcher, target))
    {
        return searcher->info.search_handle;
    }
    Ai_search_object_manager.unassign_searcher(searcher);
    return 0xFFFFFFFF;
}

// 0x00685310 (:701)
const ai_search_location_info* ai_search_get_location_info(unsigned int search_handle)
{
    const ai_search_searcher* searcher = Ai_search_object_manager.searcher_from_handle(search_handle);
    const ai_search_location* location = searcher != nullptr ? searcher->get_location() : nullptr;
    return location != nullptr ? &location->info : nullptr;
}

// 0x00685380 (:720)
ai_search_target_info* ai_search_get_target_info(unsigned int search_handle)
{
    const ai_search_searcher* searcher = Ai_search_object_manager.searcher_from_handle(search_handle);
    ai_search_target* target = searcher != nullptr ? searcher->get_target() : nullptr;
    return target != nullptr ? &target->info : nullptr;
}

// 0x006A9850 (:740)
void ai_search_found_target(unsigned int search_handle, const human* enemy)
{
    if (enemy != nullptr)
    {
        ai_search_searcher* searcher = Ai_search_object_manager.searcher_from_handle(search_handle);
        if (searcher != nullptr)
        {
            Ai_search_object_manager.unassign_searcher(searcher);
        }
    }
}

// 0x006A98A0 (:813)
bool ai_search_update_progress(unsigned int search_handle, ai_results result)
{
    ai_search_searcher* searcher = Ai_search_object_manager.searcher_from_handle(search_handle);
    if (searcher == nullptr)
    {
        return false;
    }
    switch (result)
    {
    case AI_RESULT_FAILURE:
        do_search_failure(searcher);
        break;
    case AI_RESULT_SUCCESS:
        do_search_success(searcher);
        break;
    case AI_RESULT_IN_PROGRESS:
        searcher->reset_keepalive();
        break;
    }
    return true;
}

// 0x0066B660 (:852)
bool ai_search_get_current_exterior_search_pos(vector& pos, vector& los_pos, nav_cell*& ncp, unsigned int search_handle, cover_node** c_node)
{
    const ai_search_searcher* searcher = Ai_search_object_manager.searcher_from_handle(search_handle);
    if (searcher == nullptr || !searcher->info.location_history.has_current)
    {
        return false;
    }
    const ai_search_location_history& history = searcher->info.location_history;
    pos = history.current_target;
    los_pos = history.los_position;
    ncp = nav_cell_from_handle(history.navcell);
    if (c_node != nullptr)
    {
        object* objp = history.cover != 0xFFFFFFFF ? World.object_from_handle(history.cover) : nullptr;
        *c_node = objp != nullptr && objp->get_obj_type() == OT_COVER_NODE ? static_cast<cover_node*>(objp) : nullptr;
    }
    return ncp != nullptr;
}

// 0x0066B720 (:880)
bool ai_search_set_current_exterior_search_pos(unsigned int search_handle, const vector& pos, const vector& los_pos, const nav_cell* ncp, const cover_node* c_node)
{
    if (ncp == nullptr)
    {
        return false;
    }
    ai_search_searcher* searcher = Ai_search_object_manager.searcher_from_handle(search_handle);
    if (searcher == nullptr)
    {
        return false;
    }
    ai_search_location_history& history = searcher->info.location_history;
    if (history.old.size() == 0)
    {
        // Original quirk kept: no null check of the searcher, which is alive here because it owns a search.
        const object* objp = searcher->info.searcher_handle != 0xFFFFFFFF ? World.object_from_handle(searcher->info.searcher_handle) : nullptr;
        history.old.add(objp != nullptr && objp->get_obj_type() == OT_HUMAN ? objp->pos : ZERO_VECTOR);
    }
    history.set_current(pos, los_pos, ncp, c_node);
    return true;
}

// 0x0066B7C0 (:906)
bool ai_search_finish_exterior_search_pos(unsigned int search_handle, const vector& current_pos)
{
    ai_search_searcher* searcher = Ai_search_object_manager.searcher_from_handle(search_handle);
    if (searcher == nullptr)
    {
        return false;
    }
    searcher->info.location_history.finish_current(current_pos);
    return true;
}

// 0x0066B810 (:923)
bool ai_search_abandon_exterior_search_pos(unsigned int search_handle)
{
    ai_search_searcher* searcher = Ai_search_object_manager.searcher_from_handle(search_handle);
    if (searcher == nullptr)
    {
        return false;
    }
    searcher->info.location_history.abandon_current();
    return true;
}

// 0x0066B850 (:940)
bool ai_search_abandon_exterior_search_cover(unsigned int search_handle)
{
    ai_search_searcher* searcher = Ai_search_object_manager.searcher_from_handle(search_handle);
    if (searcher == nullptr)
    {
        return false;
    }
    searcher->info.location_history.abandon_cover_node();
    return true;
}

// 0x0066B890 (:958)
bool ai_search_set_avoidance_point(unsigned int search_handle, const vector& avoid_pos)
{
    ai_search_searcher* searcher = Ai_search_object_manager.searcher_from_handle(search_handle);
    if (searcher == nullptr)
    {
        return false;
    }
    searcher->info.location_history.old.add(avoid_pos);
    return true;
}

// 0x005F1920 (:969)
cs_spinlock& ai_search_get_searchers_cs_wrapper()
{
    return Ai_search_object_manager.get_searcher_cs_wrapper();
}

// 0x00641910 (:978)
const ai_search_searcher_info* ai_search_get_first_searcher()
{
    const ai_search_searcher* searcher = Ai_search_object_manager.get_first_searcher();
    return searcher != nullptr ? &searcher->info : nullptr;
}

// 0x0066B8E0 (:990)
const ai_search_searcher_info* ai_search_get_next_searcher(const ai_search_searcher_info* prev_info)
{
    const ai_search_searcher* searcher = Ai_search_object_manager.get_next_searcher(searcher_info_to_data(prev_info));
    return searcher != nullptr ? &searcher->info : nullptr;
}
