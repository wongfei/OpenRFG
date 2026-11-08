// Original: volition/rfg/code/ai/ai_defense_coordination.cpp

#include "volition/rfg/code/ai/ai_defense_coordination.h"

#include "volition/rfg/code/ai/ai_defense_coordination_internal.h"
#include "volition/rfg/code/ai/ai_system.h"
#include "volition/rfg/code/level/world.h"
#include "volition/rfg/code/objects/humans/human.h"
#include "volition/rfg/code/objects/humans/human_combat_turret.h"
#include "volition/rfg/code/objects/humans/npc.h"
#include "volition/rfg/code/objects/object.h"
#include "volition/rfg/code/objects/object_guard_node.h"
#include "volition/rfg/code/objects/object_restricted_area.h"
#include "volition/rfg/code/vehicle/vehicle_class.h"
#include "volition/vlib/code/math/rand.h"

#include <float.h>

static const float DEFENSE_TARGET_SHARE_DIST_SQ = 144.0f; // 12 m: targets this close share threats and threaten stages
static const float DEFENDER_REASSIGN_MARGIN = 4.0f; // m: a new defender must be this much closer

// 0x012C82D0, read from rfg.exe: the time a target stays in each threaten stage, ms.
static const int Ai_defense_stage_times[NUM_AI_DEFENSE_THREATEN_STAGES] = {500, 0, 0, 0};

// 0x0064CB80 (:44)
static void initialize_new_target(ai_defense_target* target_info, const human* target)
{
    target_info->target_handle = target->get_handle();
    for (ai_defense_target* other = Ai_defense_object_manager.get_first_target(); other != nullptr; other = Ai_defense_object_manager.get_next_target(other))
    {
        if (other == target_info || other->target_handle == 0xFFFFFFFF)
        {
            continue;
        }
        const object* objp = World.object_from_handle(other->target_handle);
        if (objp != nullptr && objp->get_obj_type() == OT_HUMAN && vec_dist_squared(target->pos, objp->pos) <= DEFENSE_TARGET_SHARE_DIST_SQ && target_info->threaten_stage < other->threaten_stage)
        {
            target_info->threaten_stage = other->threaten_stage;
            target_info->stage_timer.set(other->stage_timer.time_until());
        }
    }
}

// 0x00673A70 (:80)
void arbitrate_defense_target_request(ai_defense_defender* defender_info, const human* target)
{
    ai_defense_target* target_info = Ai_defense_object_manager.find_target(target->get_handle());
    if (target_info == nullptr)
    {
        target_info = Ai_defense_object_manager.get_new_target();
        if (target_info == nullptr)
        {
            return;
        }
        initialize_new_target(target_info, target);
    }
    ai_defense_target* current = Ai_defense_object_manager.target_from_handle(defender_info->current_target);
    if (current == nullptr)
    {
        defender_info->current_target = target_info->get_handle();
        return;
    }
    if (current == target_info)
    {
        return;
    }
    const unsigned int handle = defender_info->get_handle();
    const bool threatening = current->threatening_defender == handle;
    if ((threatening && current->flags.in_progress) || current->accompanying_defender == handle)
    {
        return;
    }
    defender_info->current_target = target_info->get_handle();
    if (threatening)
    {
        const unsigned int accompanying = current->accompanying_defender;
        current->threatening_defender = 0xFFFFFFFF;
        if (Ai_defense_object_manager.defender_from_handle(accompanying) != nullptr)
        {
            current->threatening_defender = accompanying;
            current->accompanying_defender = 0xFFFFFFFF;
        }
    }
}

// 0x0064CC80 (:161). LTCG: max_dist_sq XMM0.
static ai_defense_defender* get_closest_defender(const vector& pos, unsigned int target_handle, unsigned int skip_handle, float max_dist_sq, bool mobile_only)
{
    ai_defense_defender* closest = nullptr;
    for (ai_defense_defender* defender = Ai_defense_object_manager.get_first_defender(); defender != nullptr; defender = Ai_defense_object_manager.get_next_defender(defender))
    {
        if (defender->current_target != target_handle || defender->get_handle() == skip_handle || !defender->failed_timer.elapsed())
        {
            continue;
        }
        if (mobile_only && (defender->flags.no_path || defender->flags.anchored))
        {
            continue;
        }
        if (defender->defender_handle == 0xFFFFFFFF)
        {
            continue;
        }
        const object* objp = World.object_from_handle(defender->defender_handle);
        if (objp == nullptr || objp->get_obj_type() != OT_HUMAN || objp->get_sub_type() != OT_SUB_HUMAN_NPC)
        {
            continue;
        }
        const float dist_sq = vec_dist_squared(objp->pos, pos);
        if (dist_sq < max_dist_sq && restricted_area_in_any_region(objp->pos, true))
        {
            closest = defender;
            max_dist_sq = dist_sq;
        }
    }
    return closest;
}

// 0x0064CDD0 (:206). rfg.exe's only caller passes target_info on the stack (0x0064D07E); defender_info is dropped by LTCG.
static void arbitrate_assignment_request(ai_defense_target* target_info)
{
    if (!target_info->arbitration_timer.elapsed())
    {
        return;
    }
    target_info->arbitration_timer.set(500); // ms
    const object* target = target_info->target_handle != 0xFFFFFFFF ? World.object_from_handle(target_info->target_handle) : nullptr;
    if (target == nullptr || target->get_obj_type() != OT_HUMAN)
    {
        keen::debug::nativePrintArgs("Int3 critical function at %s(%d)\n");
        return;
    }
    ai_defense_defender* threatening = Ai_defense_object_manager.defender_from_handle(target_info->threatening_defender);
    const object* threatening_obj = nullptr;
    if (threatening != nullptr && threatening->defender_handle != 0xFFFFFFFF)
    {
        const object* objp = World.object_from_handle(threatening->defender_handle);
        if (objp != nullptr && objp->get_obj_type() == OT_HUMAN && objp->get_sub_type() == OT_SUB_HUMAN_NPC)
        {
            threatening_obj = objp;
        }
    }
    if (threatening != nullptr && threatening_obj != nullptr)
    {
        // A defender at least DEFENDER_REASSIGN_MARGIN closer takes over.
        const float margin = vec_dist(threatening_obj->pos, target->pos) - DEFENDER_REASSIGN_MARGIN;
        if (margin > 0.0f)
        {
            ai_defense_defender* closer = get_closest_defender(target->pos, target_info->get_handle(), threatening->get_handle(), margin * margin, false);
            if (closer != nullptr)
            {
                threatening = closer;
                target_info->threatening_defender = closer->get_handle();
            }
        }
    }
    else
    {
        if (threatening != nullptr)
        {
            target_info->threatening_defender = 0xFFFFFFFF;
        }
        target_info->accompanying_defender = 0xFFFFFFFF;
        bool already_threatened = false;
        for (ai_defense_target* other = Ai_defense_object_manager.get_first_target(); other != nullptr; other = Ai_defense_object_manager.get_next_target(other))
        {
            if (other == target_info || other->target_handle == 0xFFFFFFFF)
            {
                continue;
            }
            const object* objp = World.object_from_handle(other->target_handle);
            if (objp == nullptr || objp->get_obj_type() != OT_HUMAN || vec_dist_squared(target->pos, objp->pos) > DEFENSE_TARGET_SHARE_DIST_SQ)
            {
                continue;
            }
            if (target_info->threaten_stage < other->threaten_stage)
            {
                target_info->threaten_stage = other->threaten_stage;
                target_info->stage_timer.set(other->stage_timer.time_until());
            }
            if (other->threatening_defender != 0xFFFFFFFF)
            {
                already_threatened = true;
            }
        }
        if (already_threatened)
        {
            return;
        }
        threatening = get_closest_defender(target->pos, target_info->get_handle(), 0xFFFFFFFF, FLT_MAX, false);
        if (threatening == nullptr)
        {
            return;
        }
        target_info->threatening_defender = threatening->get_handle();
    }
    if (threatening->flags.no_path || threatening->flags.anchored || !target_info->no_path_timer.elapsed())
    {
        target_info->accompanying_defender = 0xFFFFFFFF;
        return;
    }
    ai_defense_defender* accompanying = get_closest_defender(target->pos, target_info->get_handle(), threatening->get_handle(), FLT_MAX, true);
    if (accompanying != nullptr)
    {
        target_info->accompanying_defender = accompanying->get_handle();
    }
}

// 0x005FB6C0 (:332)
static ai_defense_threaten_stages get_next_threaten_stage(ai_defense_threaten_stages last_stage)
{
    const int next = last_stage + 1;
    return next > AI_DEFENSE_THREATEN_DASH_AND_BASH ? AI_DEFENSE_THREATEN_KILL : (ai_defense_threaten_stages)next;
}

// 0x006B4C10 (:345). LTCG: np EAX.
bool npc_is_anchored(const npc* np)
{
    if (human_combat_get_manned_turret(np) != nullptr)
    {
        return true;
    }
    const ai_system* ai = np->get_ai();
    const object_guard_node* node = ai->get_guard_node();
    return node != nullptr && node->is_reserved(np->get_handle()) && !node->on_ground_floor() && ai->is_at_guard_post();
}

// 0x00673B30 (:371). LTCG: def_handle ECX, target EDX.
ai_defense_target* get_target_info(unsigned int def_handle, const human* target)
{
    ai_defense_defender* defender = Ai_defense_object_manager.defender_from_handle(def_handle);
    if (defender == nullptr)
    {
        // Original bug kept: the target it finds is dropped and null returned (XOR EAX,EAX after the call).
        if (target != nullptr)
        {
            Ai_defense_object_manager.find_target(target->get_handle());
        }
        return nullptr;
    }
    return Ai_defense_object_manager.target_from_handle(defender->current_target);
}

// 0x005FB6D0 (:400)
object_restricted_area* ai_defense_is_human_in_restricted_area(const human* hp, bool include_warning_areas)
{
    vehicle* vp = human_in_vehicle(hp);
    const object* objp = vp != nullptr ? static_cast<const object*>(vp) : hp;
    const vector pos = objp->pos;
    object_restricted_area* area = restricted_area_find_area(pos, include_warning_areas, false);
    if (area != nullptr)
    {
        // Ambient pedestrians are allowed in the areas that say so. obj_type and sub_type say npc.
        if (hp != nullptr && hp->get_obj_type() == OT_HUMAN && hp->get_sub_type() == OT_SUB_HUMAN_NPC && static_cast<const npc*>(hp)->npc_flags.ambient_spawn && area->allow_ambient_peds())
        {
            return nullptr;
        }
        return area;
    }
    if (object_is_player(hp) && vp != nullptr && human_is_driving(hp))
    {
        // A driving player is in the area when any corner of the vehicle is.
        vector bmin;
        vector bmax;
        vp->get_local_bbox(bmin, bmax);
        vector corners[4] = {
            vector(bmin.x, 0.0f, bmin.z),
            vector(bmin.x, 0.0f, bmax.z),
            vector(bmax.x, 0.0f, bmax.z),
            vector(bmax.x, 0.0f, bmin.z),
        };
        for (int i = 0; i < 4; i++)
        {
            vec_change_to_global_coords(corners[i], vp->pos, vp->orient);
            area = restricted_area_find_area(corners[i], include_warning_areas, false);
            if (area != nullptr)
            {
                return area;
            }
        }
    }
    return nullptr;
}

// 0x006B4C80 (:465)
bool ai_defense_is_anchored(const npc* np, unsigned int def_handle)
{
    if (npc_is_anchored(np))
    {
        return true;
    }
    const ai_defense_defender* defender = Ai_defense_object_manager.defender_from_handle(def_handle);
    return defender != nullptr && defender->flags.no_path;
}

// 0x006B4CD0 (:487)
unsigned int ai_defense_request_assignment(const npc* np, const human* target, unsigned int prev_handle)
{
    if (np == nullptr || target == nullptr)
    {
        return 0xFFFFFFFF;
    }
    ai_defense_defender* defender = Ai_defense_object_manager.defender_from_handle(prev_handle);
    if (defender == nullptr)
    {
        defender = Ai_defense_object_manager.get_new_defender();
        if (defender == nullptr)
        {
            return 0xFFFFFFFF;
        }
        defender->defender_handle = np->get_handle();
    }
    else if (defender->defender_handle != np->get_handle())
    {
        return 0xFFFFFFFF;
    }
    defender->flags.anchored = npc_is_anchored(np);
    arbitrate_defense_target_request(defender, target);
    return defender->get_handle();
}

// 0x0064D010 (:532)
ai_defense_assignments ai_defense_get_assignment(unsigned int def_handle)
{
    ai_defense_defender* defender = Ai_defense_object_manager.defender_from_handle(def_handle);
    if (defender == nullptr)
    {
        return AI_DEFENSE_ASSIGNMENT_GUARD;
    }
    ai_defense_target* target = Ai_defense_object_manager.target_from_handle(defender->current_target);
    if (target == nullptr)
    {
        return AI_DEFENSE_ASSIGNMENT_GUARD;
    }
    if (!target->flags.in_progress && target->flags.acknowledged)
    {
        arbitrate_assignment_request(target);
    }
    if (target->threaten_stage == AI_DEFENSE_THREATEN_KILL && target->stage_timer.elapsed())
    {
        return AI_DEFENSE_ASSIGNMENT_THREATEN;
    }
    if (!target->flags.acknowledged)
    {
        return AI_DEFENSE_ASSIGNMENT_GUARD;
    }
    if (target->threatening_defender == def_handle)
    {
        return AI_DEFENSE_ASSIGNMENT_THREATEN;
    }
    return target->accompanying_defender == def_handle ? AI_DEFENSE_ASSIGNMENT_ACCOMPANY : AI_DEFENSE_ASSIGNMENT_GUARD;
}

// 0x00673BA0 (:580)
ai_defense_threaten_stages ai_defense_get_threaten_stage(unsigned int def_handle, const human* target)
{
    const ai_defense_target* target_info = get_target_info(def_handle, target);
    return target_info != nullptr && target_info->flags.acknowledged ? target_info->threaten_stage : AI_DEFENSE_THREATEN_NONE;
}

// 0x00673BC0 (:600)
bool ai_defense_should_wait_to_threaten(unsigned int def_handle, const human* target)
{
    const ai_defense_target* target_info = get_target_info(def_handle, target);
    return target_info == nullptr || !target_info->flags.acknowledged || !target_info->stage_timer.elapsed();
}

// 0x00673BF0 (:619)
bool ai_defense_should_play_beatdown_line(unsigned int def_handle, const human* target)
{
    const ai_defense_target* target_info = get_target_info(def_handle, target);
    return target_info != nullptr && target_info->threaten_stage == AI_DEFENSE_THREATEN_DASH_AND_BASH && !target_info->flags.beatdown_line_played;
}

// 0x00673C20 (:628)
void ai_defense_report_played_beatdown_line(unsigned int def_handle, const human* target)
{
    ai_defense_target* target_info = get_target_info(def_handle, target);
    if (target_info != nullptr)
    {
        target_info->flags.beatdown_line_played = true;
    }
}

// 0x0064D0C0 (:644)
npc* ai_defense_get_threatening_npc(unsigned int def_handle)
{
    ai_defense_defender* defender = Ai_defense_object_manager.defender_from_handle(def_handle);
    if (defender == nullptr)
    {
        return nullptr;
    }
    ai_defense_target* target = Ai_defense_object_manager.target_from_handle(defender->current_target);
    if (target == nullptr || target->accompanying_defender != def_handle)
    {
        return nullptr;
    }
    ai_defense_defender* threatening = Ai_defense_object_manager.defender_from_handle(target->threatening_defender);
    if (threatening == nullptr || threatening->defender_handle == 0xFFFFFFFF)
    {
        return nullptr;
    }
    object* objp = World.object_from_handle(threatening->defender_handle);
    return objp != nullptr && objp->get_obj_type() == OT_HUMAN && objp->get_sub_type() == OT_SUB_HUMAN_NPC ? static_cast<npc*>(objp) : nullptr;
}

// 0x0064D160 (:684)
bool ai_defense_report_threaten_start(unsigned int def_handle)
{
    ai_defense_defender* defender = Ai_defense_object_manager.defender_from_handle(def_handle);
    if (defender == nullptr)
    {
        return false;
    }
    ai_defense_target* target = Ai_defense_object_manager.target_from_handle(defender->current_target);
    if (target == nullptr || target->threatening_defender != def_handle)
    {
        return false;
    }
    if (target->stage_timer.elapsed() && !target->flags.in_progress)
    {
        target->flags.in_progress = true;
        return true;
    }
    return false;
}

// 0x0064D1F0 (:725)
void ai_defense_report_threaten_done(unsigned int def_handle)
{
    ai_defense_defender* defender = Ai_defense_object_manager.defender_from_handle(def_handle);
    if (defender == nullptr)
    {
        return;
    }
    ai_defense_target* target = Ai_defense_object_manager.target_from_handle(defender->current_target);
    if (target == nullptr || target->threatening_defender != def_handle || !target->flags.in_progress)
    {
        return;
    }
    target->flags.in_progress = false;
    const ai_defense_threaten_stages next_stage = get_next_threaten_stage(target->threaten_stage);
    if (next_stage != target->threaten_stage)
    {
        target->threaten_stage = next_stage;
        target->stage_timer.set(Ai_defense_stage_times[next_stage]);
    }
}

// 0x0064D290 (:759)
void ai_defense_report_threaten_failed(unsigned int def_handle)
{
    ai_defense_defender* defender = Ai_defense_object_manager.defender_from_handle(def_handle);
    if (defender == nullptr)
    {
        return;
    }
    ai_defense_target* target = Ai_defense_object_manager.target_from_handle(defender->current_target);
    if (target != nullptr && target->threatening_defender == def_handle)
    {
        defender->failed_timer.set(10000); // ms
        target->flags.in_progress = false;
        target->threatening_defender = target->accompanying_defender;
        target->accompanying_defender = 0xFFFFFFFF;
    }
}

// 0x0064D320 (:788)
void ai_defense_report_no_path(unsigned int def_handle)
{
    ai_defense_defender* defender = Ai_defense_object_manager.defender_from_handle(def_handle);
    if (defender == nullptr)
    {
        return;
    }
    defender->flags.no_path = true;
    ai_defense_target* target = Ai_defense_object_manager.target_from_handle(defender->current_target);
    if (target != nullptr && target->threatening_defender == def_handle)
    {
        target->no_path_timer.set(5000); // ms
    }
}

// 0x006B4D60 (:812)
void ai_defense_coordination_level_init_pre()
{
    Ai_defense_object_manager.level_init();
}

// 0x006B4D70 (:817)
void ai_defense_coordination_do_frame()
{
    Ai_defense_object_manager.do_frame();
    for (ai_defense_target* target = Ai_defense_object_manager.get_first_target(); target != nullptr; target = Ai_defense_object_manager.get_next_target(target))
    {
        human* hp = target->target_handle != 0xFFFFFFFF ? World.object_from_handle<human>(target->target_handle) : nullptr;
        if (ai_defense_is_human_in_restricted_area(hp, true) != nullptr)
        {
            target->flags.shooed_away = false;
            target->last_in_area_time.set(0);
            continue;
        }
        if (!target->flags.in_progress && target->threaten_stage != AI_DEFENSE_THREATEN_NONE)
        {
            target->stage_timer.set(Ai_defense_stage_times[target->threaten_stage]);
        }
        if (target->flags.shooed_away || target->last_in_area_time.time_since() <= 2000) // ms
        {
            continue;
        }
        target->flags.shooed_away = true;
        // The threatening npc reacts to the target leaving, unless it already knows about an enemy.
        const ai_defense_defender* threatening = Ai_defense_object_manager.defender_from_handle(target->threatening_defender);
        if (threatening == nullptr || threatening->defender_handle == 0xFFFFFFFF)
        {
            continue;
        }
        object* objp = World.object_from_handle(threatening->defender_handle);
        if (objp == nullptr || objp->get_obj_type() != OT_HUMAN || objp->get_sub_type() != OT_SUB_HUMAN_NPC)
        {
            continue;
        }
        ai_system* ai = static_cast<npc*>(objp)->get_ai();
        if (ai->perception.get_first_belief(AI_BELIEF_ENEMY_EXISTS) == nullptr && target->target_handle != 0xFFFFFFFF)
        {
            ai->reaction_info.belief_type = AI_BELIEF_UNDEFINED;
            ai->reaction_info.target_handle = target->target_handle;
            ai->reaction_info.reason = 0x20;
            ai->reaction_info.time_left.set(int_rand_range(2000, 3000));
        }
    }
}
