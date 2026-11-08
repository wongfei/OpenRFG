// Original: volition/rfg/code/ai/combat/combat_common.cpp

#include "volition/rfg/code/ai/combat/combat_common.h"

#include "volition/audio_lib/code/audiolib_api.h"
#include "volition/rfg/code/ai/ai_planning_data.h"
#include "volition/rfg/code/ai/ai_system.h"
#include "volition/rfg/code/ai/beliefs.h"
#include "volition/rfg/code/ai/combat/combat_cover_actions.h"
#include "volition/rfg/code/ai/combat/defensive_combat_actions.h"
#include "volition/rfg/code/gameplay/gameflow/alert_level_manager.h"
#include "volition/rfg/code/geom/character_instance.h"
#include "volition/rfg/code/geom/vanim.h"
#include "volition/rfg/code/havok/havok.h"
#include "volition/rfg/code/havok/havok_raycast.h"
#include "volition/rfg/code/objects/humans/human_combat_cover.h"
#include "volition/rfg/code/objects/humans/human_teams.h"
#include "volition/rfg/code/objects/humans/player.h"
#include "volition/rfg/code/objects/object_convoy.h"
#include "volition/rfg/code/objects/object_cover_node.h"
#include "volition/rfg/code/objects/object_squad.h"
#include "volition/rfg/code/objects/turret.h"
#include "volition/rfg/code/scripting/action_script/global_event.h"
#include "volition/rfg/code/scripting/action_script/global_event_data.h"
#include "volition/rfg/code/scripting/action_script/global_event_manager.h"
#include "volition/rfg/code/vehicle/vehicle_class.h"
#include "volition/rfg/code/vehicle/vehicle_system.h"
#include "volition/rfg/code/code_effects/explosion_shared.h"
#include "volition/rfg/code/weapons/weapon_firing_patterns.h"
#include "volition/vlib/code/math/bbox.h"
#include "volition/vlib/code/math/ix.h"
#include "volition/rfg/code/level/world.h"
#include "volition/rfg/code/objects/humans/human_combat.h"
#include "volition/rfg/code/objects/humans/human_weapons.h"
#include "volition/rfg/code/objects/humans/npc.h"
#include "volition/rfg/code/objects/inventory_item.h"
#include "volition/rfg/code/weapons/weapons.h"

static bool Turret_fire_test_use_spherecast = true; // 0x016413E1, .data 1, never written

// 0x005DD7A0: rfg.exe's constructor sets nothing.
ai_combat_action_parameters::ai_combat_action_parameters()
{
}

// 0x0061D920 (:414)
void ai_combat_action_parameters::init_npc(npc* np_)
{
    np = np_;
    target = nullptr;
    target_weap_inst = nullptr;
    node = nullptr;
    set_weapon(human_weapons_get_weapon(np_));
}

// 0x005E2F50 (:423)
void ai_combat_action_parameters::set_weapon(weapon* new_wp)
{
    weapon_inst = new_wp;
    penetrating_weapon = new_wp != nullptr && new_wp->w_info->flags.penetrating_bullets;
    in_penetrating_range = target != nullptr && new_wp != nullptr && dist_to_target < new_wp->w_info->max_ai_penetrating_dist;
}

// 0x0061D9B0 (:433)
bool ai_combat_action_parameters::init_target(bool use_attention_target)
{
    if (use_attention_target)
    {
        object* attended = World.object_from_handle(np->get_ai()->get_target_handle());
        target = (attended != nullptr && attended->get_obj_type() == OT_HUMAN && !attended->is_killed()) ? static_cast<human*>(attended) : nullptr;
    }
    else
    {
        target = human_combat_get_target(np);
    }
    if (target == nullptr)
    {
        return false;
    }

    target_dir.x = target->pos.x - np->pos.x;
    target_dir.y = target->pos.y - np->pos.y;
    target_dir.z = target->pos.z - np->pos.z;
    dist_to_target = target_dir.mag_and_normalize_safe();
    if (dist_to_target - 0.1f < 0.0f)
    {
        dist_to_target = 0.1f;
    }
    target_weap_inst = human_weapons_get_weapon(target);
    if (penetrating_weapon)
    {
        in_penetrating_range = dist_to_target < weapon_inst->w_info->max_ai_penetrating_dist;
    }
    return true;
}

// 0x0061E430 (:1398)
const weapon* ai_combat_get_action_firearm(const npc* np)
{
    weapon* wp = human_weapons_get_weapon(np);
    if (wp != nullptr && wp->w_info->is_firearm())
    {
        return wp;
    }
    const inventory_item* item = inv_item_get_best_firearm(np);
    if (item == nullptr)
    {
        // Original quirk kept: an unsuitable held weapon is returned when the inventory has no firearm.
        return wp;
    }
    return item->weapon_handle != 0xFFFFFFFF ? World.object_from_handle<weapon>(item->weapon_handle) : nullptr;
}

// 0x0061D710 (:141). LTCG: np EAX, wp ECX. True when another vehicle of the convoy is on the way to the target.
static bool convoy_vehicle_in_line_of_fire(const npc* np, const object_convoy* convoy, const weapon* wp, const object* target)
{
    const int my_index = convoy->get_member_index(np->get_ai()->get_squad_handle());
    if (my_index < 0)
    {
        return false;
    }
    vector muzzle_pos;
    matrix muzzle_orient;
    wp->get_muzzle_transform(muzzle_pos, &muzzle_orient);
    const float dist = vec_dist(muzzle_pos, target->pos);
    vector end;
    end.x = muzzle_orient.fvec.x * dist + muzzle_pos.x;
    end.y = muzzle_pos.y + muzzle_orient.fvec.y * dist;
    end.z = muzzle_pos.z + muzzle_orient.fvec.z * dist;
    float explosion_radius = 0.0f;
    if (wp->w_info->m_explosion_info != nullptr)
    {
        explosion_radius = wp->w_info->m_ai_explosion_info != nullptr ? wp->w_info->m_ai_explosion_info->m_radius : wp->w_info->m_explosion_info->m_radius;
    }
    for (int i = 0; i < 12; i++) // 0xC member squads
    {
        if (i == my_index)
        {
            continue;
        }
        const object_squad* squad = convoy->get_member_squad(i);
        const vehicle* vp = squad != nullptr ? squad->get_vehicle() : nullptr;
        if (vp == nullptr)
        {
            continue;
        }
        vector bmin;
        vector bmax;
        vp->get_local_bbox(bmin, bmax);
        const float radius = vec_xz_dist(bmin, bmax) * 0.5f * 1.21f + explosion_radius;
        vector closest;
        ix_closest_point_on_line_segment(closest, vp->pos, muzzle_pos, end);
        if (vec_dist_squared(vp->pos, closest) < radius * radius)
        {
            return true;
        }
    }
    return false;
}

// 0x005E2E50 (:233). LTCG: wp EAX, wfpp ECX. False when the burst is over.
bool ai_combat_fire_burst(npc* np, const weapon* wp, const weapon_firing_pattern* wfpp, float eff_range_pct, bool secondary_weapon)
{
    ai_system* ai = np->get_ai();
    if (!wp->flags.trigger_down)
    {
        int burst = wfpp->get_burst_size(eff_range_pct);
        // Twice as long at a player sprinting in with a melee weapon from within 20 m.
        const player* pl = player_get_local_player();
        if (pl != nullptr && pl->pflags.do_sprint != 0 && human_combat_get_target(np) == pl)
        {
            const weapon* player_weapon = pl->get_equipped_weapon();
            if (player_weapon != nullptr && player_weapon->w_info->ammo_type == WEAPON_AMMO_TYPE_MELEE && vec_dist_squared(pl->pos, np->pos) < 400.0f)
            {
                burst *= 2;
            }
        }
        if (burst > 0)
        {
            ai->set_desired_burst_size(burst);
        }
    }
    else if (ai->get_desired_burst_size() <= wp->num_rounds_fired || wfpp->get_max_burst_size(eff_range_pct) <= wp->num_rounds_fired)
    {
        ai->set_desired_burst_size(0);
        return false;
    }
    if (secondary_weapon)
    {
        ai->flags.alternate_fire_this_frame = true;
    }
    else
    {
        ai->flags.fire_weapon_this_frame = true;
    }
    return true;
}

// 0x006574B0 (:305)
bool cover_me_collect_cb(object* obj, unsigned int, const void* cb_data)
{
    if (cb_data == obj)
    {
        return false;
    }
    return static_cast<npc*>(obj)->get_ai()->can_provide_covering_fire(static_cast<const npc*>(cb_data));
}

// 0x006574D0 (:346). LTCG: wp EAX, target EDI. From the asm, the decompile hides the results: how far into the weapon's range
// a shot is, with the shooter's and the target's situation modifying it.
float get_effective_range_percent(const npc* np, float distance, const weapon* wp, const human* target, const weapon_firing_pattern* wfpp, bool do_adjustments)
{
    if (np->should_active_despawn())
    {
        return 1.0f;
    }
    const float range_pct = distance / wp->w_info->max_range;
    if (wfpp == nullptr)
    {
        return range_pct;
    }
    float multiplier = 1.0f;
    float adjustment = 0.0f;
    float min_range = 0.0f;
    if (do_adjustments)
    {
        const float ammo_pct = (float)wp->rounds_in_magazine / (float)wp->w_info->magazine_size;
        const bool stepped_out = human_combat_cover_is_stepped_out(np);
        const vehicle* vp = target != nullptr ? World.object_from_handle<vehicle>(target->vehicle_handle) : nullptr;
        const bool heavy = vp != nullptr && (vp->get_sub_type() == OT_SUB_VEHICLE_WALKER || vehicle_is_tank(vp));
        const weapon_firing_pattern::modifier* mod = nullptr;
        if (vp != nullptr && vp->get_sub_type() == OT_SUB_VEHICLE_FLYER)
        {
            mod = &wfpp->flyer_mod;
        }
        else if (heavy)
        {
            mod = &wfpp->heavy_vehicle_mod;
        }
        else if (vp != nullptr)
        {
            mod = &wfpp->normal_vehicle_mod;
        }
        else if (gsm_get_alert_level() <= 1)
        {
            mod = &wfpp->alert_mod;
        }
        if (mod != nullptr)
        {
            multiplier = mod->multiplier;
            adjustment = mod->adjustment;
        }
        if (!stepped_out)
        {
            min_range = wfpp->get_min_effective_range(ammo_pct);
        }
    }
    if (np->get_ai()->is_providing_covering_fire() != nullptr)
    {
        multiplier *= wfpp->covering_fire_mod.multiplier;
        adjustment += wfpp->covering_fire_mod.adjustment;
    }
    const float result = multiplier * range_pct + adjustment;
    return result - min_range < 0.0f ? min_range : result;
}

// 0x005E2FB0 (:460)
bool ai_combat_action_parameters::init_cover_node()
{
    node = human_combat_cover_get_node(np);
    if (node == nullptr)
    {
        return false;
    }
    node_to_npc_dir.x = np->pos.x - node->pos.x;
    node_to_npc_dir.y = np->pos.y - node->pos.y;
    node_to_npc_dir.z = np->pos.z - node->pos.z;
    node_to_npc_dist = node_to_npc_dir.mag_and_normalize();
    return true;
}

// 0x005DBE30 (global_event_data.h:233)
player_under_attack_event::player_under_attack_event()
    : event_data(EVENT_PLAYER_UNDER_ATTACK)
{
}

// 0x005DBE40 (global_event_data.h:237)
vehicle_under_attack_event::vehicle_under_attack_event()
    : event_data(EVENT_VEHICLE_UNDER_ATTACK)
{
}

// 0x0061DB20 (:481)
void ai_combat_post_attack_events(npc* np, const object* target)
{
    if (object_is_local_player(target))
    {
        player_under_attack_event event;
        event.handle = np->get_handle();
        gem_post_event(event);
    }
    if (target != nullptr && target->get_obj_type() == OT_HUMAN && static_cast<const human*>(target)->vehicle_handle != 0xFFFFFFFF)
    {
        vehicle_under_attack_event event;
        event.attacker_handle = np->get_handle();
        event.vehicle_handle = static_cast<const human*>(target)->vehicle_handle;
        gem_post_event(event);
    }
}

// 0x0061DBA0 (:513)
ai_combat_should_charge_results ai_combat_should_charge_target(const npc* np)
{
    const ai_system* ai = np->get_ai();
    const human* target = human_combat_get_target(np);
    if (target == nullptr)
    {
        return AI_COMBAT_SHOULD_NOT_CHARGE;
    }
    if (!ai->persona_flags.remote_charge_panic && ai->perception.get_belief(AI_BELIEF_PROJECTILE, BELIEF_REQUIREMENTS_ATTACHED_TO_ME, OT_UNDEFINED) != nullptr)
    {
        return AI_COMBAT_MUST_CHARGE;
    }
    if (ai->get_tension_level() != AI_TENSION_LEVEL_HIGH)
    {
        return AI_COMBAT_SHOULD_NOT_CHARGE;
    }
    const weapon_info* info = human_weapons_get_weapon_info(target);
    if (info == nullptr || !info->is_explosive() || vec_dist_squared(np->pos, target->pos) > 1600.0f || target->pos == ZERO_VECTOR || ai->outside_defensive_perimeter(target->pos, false, nullptr))
    {
        return AI_COMBAT_SHOULD_NOT_CHARGE;
    }
    return AI_COMBAT_SHOULD_CHARGE;
}

// 0x0061DE10 (:549)
ai_combat_should_charge_results ai_combat_should_charge_target(const ai_combat_action_parameters& params)
{
    const ai_system* ai = params.np->get_ai();
    if (!ai->persona_flags.remote_charge_panic && ai->perception.get_belief(AI_BELIEF_PROJECTILE, BELIEF_REQUIREMENTS_ATTACHED_TO_ME, OT_UNDEFINED) != nullptr)
    {
        return AI_COMBAT_MUST_CHARGE;
    }
    if (ai->get_tension_level() != AI_TENSION_LEVEL_HIGH || params.target_weap_inst == nullptr || !params.target_weap_inst->w_info->is_explosive() ||
        params.dist_to_target > 40.0f || human_in_vehicle(params.target) != nullptr || params.target->pos == ZERO_VECTOR ||
        ai->outside_defensive_perimeter(params.target->pos, false, nullptr))
    {
        return AI_COMBAT_SHOULD_NOT_CHARGE;
    }
    return AI_COMBAT_SHOULD_CHARGE;
}

// 0x0061E0E0 (:593)
bool ai_combat_should_request_covering_fire(npc* np, const cover_node* node, const human* target)
{
    if (vec_dist_squared(np->pos, node->pos) < 25.0f)
    {
        return false;
    }
    vector to_np;
    to_np.x = np->pos.x - target->pos.x;
    to_np.y = np->pos.y - target->pos.y;
    to_np.z = np->pos.z - target->pos.z;
    if (target->orient.fvec * to_np < 0.0f)
    {
        return false;
    }
    const weapon_info* info = human_weapons_get_weapon_info(target);
    if (info == nullptr)
    {
        return false;
    }
    const float range_sq = info->max_engagement_dist * info->max_engagement_dist;
    if (range_sq < vec_dist_squared(target->pos, node->pos) && range_sq < vec_dist_squared(target->pos, np->pos))
    {
        return false;
    }
    return np->get_ai()->can_receive_covering_fire();
}

// 0x00657770 (:629): the nearest within 15 m that can.
npc* ai_combat_find_someone_to_cover_me(npc* np, const human*)
{
    unsigned int handles[10];
    const int count = World.collect_objects<npc>(np->pos, 15.0f, handles, 10, cover_me_collect_cb, np);
    npc* best = nullptr;
    float best_dist_sq = -1.0f;
    for (int i = 0; i < count; i++)
    {
        npc* other = World.object_from_handle<npc>(handles[i]);
        if (other != nullptr)
        {
            const float dist_sq = vec_dist_squared(np->pos, other->pos);
            if (best == nullptr || dist_sq < best_dist_sq)
            {
                best = other;
                best_dist_sq = dist_sq;
            }
        }
    }
    return best;
}

// 0x005E3050 (:658)
bool ai_combat_is_going_to_cover(const npc* np)
{
    const ai_action* act = np->get_ai()->planner.get_current_action();
    return act != nullptr && (act == go_to_cover_action::get_instance() || act == defensive_combat_cover_action::get_instance());
}

// 0x005E3090 (:683): true when there is nothing to reload.
bool ai_combat_do_reload(npc* np)
{
    weapon* wp = human_weapons_get_weapon(np);
    if (wp == nullptr || !wp->w_info->is_firearm())
    {
        return true;
    }
    if (wp->reload_delay_timer.valid())
    {
        return false;
    }
    if (wp->rounds_in_magazine == wp->get_magazine_size() || !wp->uses_ammo())
    {
        return true;
    }
    if (np->char_inst->is_playing_action(ANIM_ALIAS_FIRE) || np->char_inst->is_playing_action(ANIM_ALIAS_RELOAD) || np->hflags.playing_equip_anim != 0 || np->hflags.playing_unequip_anim != 0)
    {
        return false;
    }
    audiolib_cue_play_3d(wp->w_info->reload_sound, &np->pos, nullptr, nullptr, nullptr);
    int reload_time = 1000;
    if (human_in_vehicle(np) == nullptr || human_weapons_is_weapon_raised(np))
    {
        const animation_action anim = human_weapons_get_reload_animation(wp->w_info->weapon_class, np);
        if (anim != ANIMATION_ACTION_NONE)
        {
            np->char_inst->play_action(anim, ANIM_ALIAS_RELOAD, 1.0f, 0x20);
            reload_time = (int)(vanim_get_anim_length(np->char_inst->vp, anim) * 1000.0); // from the FPU code: s to ms
        }
    }
    wp->start_reload(reload_time);
    human_set_combat_ready(np, true, reload_time);
    return false;
}

// 0x00693F20 (:781)
bool ai_combat_fire_at_target(const ai_combat_action_parameters& params, bool ignore_range)
{
    npc* np = params.np;
    const weapon* wp = params.weapon_inst;
    ai_system* ai = np->get_ai();
    if (wp == nullptr || params.target == nullptr)
    {
        return false;
    }
    const weapon_info* info = wp->w_info;
    if (ai->perception.get_attention_mode() != ATTENTION_AIM_AT_OBJECT)
    {
        ai->perception.set_attention_mode(ATTENTION_AIM_AT_OBJECT, ATTENTION_STEERING_USE_CURRENT_MODE, ATTENTION_AWARENESS_NORMAL);
    }
    timestamp& shoot_timer = np->combat.shoot_enemy_timestamp;
    if (shoot_timer.valid() && !shoot_timer.elapsed())
    {
        return false;
    }

    bool can_fire = true;
    if (!params.in_penetrating_range)
    {
        if (!ignore_range)
        {
            can_fire = ai->perception.can_hit_close_to_target();
        }
        else if (!ai->perception.aiming_at(params.target, wp, false))
        {
            can_fire = false;
        }
        else
        {
            // Bad shots wait out the pattern's longest interval.
            const ai_belief* shot = ai->perception.get_belief(AI_BELIEF_SHOT_RESULT);
            if (shot != nullptr && shot->handle == params.target->get_handle())
            {
                can_fire = info->npc_firing_pattern->get_max_firing_interval() <= shot->update_time.time_since() || (shot->flags & 0x38000000) != 0;
            }
        }
    }

    bool firing = wp->flags.trigger_down;
    if (!firing && can_fire)
    {
        // An ally within 15 m just started firing: don't all fire at once.
        const ai_belief* ally_belief = ai->perception.get_belief(AI_BELIEF_CLOSEST_ALLY);
        const npc* ally = ally_belief != nullptr ? World.object_from_handle<npc>(ally_belief->handle) : nullptr;
        if (ally != nullptr && vec_dist_squared(np->pos, ally->pos) <= 225.0f)
        {
            const weapon* ally_weapon = human_weapons_get_weapon(ally);
            if (ally_weapon != nullptr && ally_weapon->flags.trigger_down && ally_weapon->num_rounds_fired < 4)
            {
                shoot_timer.set_rand(200, 400);
                return false;
            }
        }
    }

    const float range_pct = get_effective_range_percent(np, params.dist_to_target, wp, params.target, info->npc_firing_pattern, true);
    if (can_fire)
    {
        firing = ai_combat_fire_burst(np, wp, info->npc_firing_pattern, range_pct, false);
    }
    else
    {
        ai->set_desired_burst_size(0);
    }
    if (!firing)
    {
        shoot_timer.set(info->npc_firing_pattern->get_firing_interval(range_pct));
    }
    return firing;
}

// 0x00657850 (:911)
bool ai_combat_do_suppressing_fire(npc* np, const vector& target_pos)
{
    ai_system* ai = np->get_ai();
    const weapon* wp = human_weapons_get_weapon(np);
    if (wp == nullptr || !wp->w_info->is_firearm())
    {
        return false;
    }
    const weapon_info* info = wp->w_info;
    const float dist = vec_dist(np->pos, target_pos);
    timestamp& shoot_timer = np->combat.shoot_enemy_timestamp;
    if (shoot_timer.valid() && !shoot_timer.elapsed())
    {
        return false;
    }
    bool can_fire = ai->perception.can_hit_close_to_target();
    // Only where the target was last seen, within 4 m.
    const ai_belief* last_contact = ai->perception.get_belief(AI_BELIEF_LAST_CONTACT, np->combat.target_handle);
    if (last_contact == nullptr || vec_dist_squared(last_contact->pos, target_pos) > 16.0f)
    {
        can_fire = false;
    }
    const float range_pct = get_effective_range_percent(np, dist, wp, nullptr, info->npc_firing_pattern, false);
    if (can_fire)
    {
        if (ai_combat_fire_burst(np, wp, info->npc_firing_pattern, range_pct, false))
        {
            return true;
        }
    }
    else
    {
        ai->set_desired_burst_size(0);
    }
    shoot_timer.set(info->npc_firing_pattern->get_firing_interval(range_pct));
    return false;
}

// 0x0061E220 (:968)
bool ai_combat_fire_check_range(const ai_combat_action_parameters& params)
{
    const ai_system* ai = params.np->get_ai();
    if ((int)params.weapon_inst->get_range_category(params.dist_to_target) <= 2)
    {
        return true;
    }
    if (ai->perception.get_belief(AI_BELIEF_SHOULD_ENGAGE_ENEMY, params.target->get_handle()) != nullptr)
    {
        return true;
    }
    return human_teams_get_disposition(params.np, HUMAN_TEAM_EDF) == TEAM_DISPOSITION_ALLIED && (int)gsm_get_alert_level() >= 2;
}

// 0x00694180 (:998)
bool ai_combat_fire_at_target_if_available(npc* np, bool quick_lower, bool plant_feet)
{
    ai_system* ai = np->get_ai();
    ai_combat_action_parameters params;
    params.init_npc(np);
    bool has_target = false;
    bool fire = false;
    if ((int)np->cur_movement_state < 3 && ai->perception.get_belief(AI_BELIEF_FIRST_CONTACT, np->combat.target_handle) != nullptr && params.init_target(false) &&
        ai->perception.get_belief(AI_BELIEF_ENEMY_POSITION, params.target) != nullptr && params.weapon_inst != nullptr &&
        params.weapon_inst->w_info->ammo_type != WEAPON_AMMO_TYPE_MELEE && params.weapon_inst->w_info->ammo_type != WEAPON_AMMO_TYPE_THROWN &&
        ai->perception.can_engage_target_reason(params.target) == AI_ENGAGE_ALLOWED)
    {
        has_target = true;
        fire = ai_combat_fire_check_range(params);
    }
    if (fire)
    {
        human_set_combat_ready(np, true, 0);
        if (params.target != nullptr)
        {
            ai->perception.pay_attention(params.target->get_handle(), AI_BELIEF_UNDEFINED, ATTENTION_AIM_AT_OBJECT, ATTENTION_STEERING_USE_DEFAULT, ATTENTION_AWARENESS_NORMAL);
        }
        return ai_combat_fire_at_target(params, false);
    }
    if (quick_lower)
    {
        human_set_combat_ready(np, false, 0);
    }
    const attention_steering_modes steering = plant_feet ? ATTENTION_STEERING_PLANT_FEET : ATTENTION_STEERING_USE_DEFAULT;
    if (!has_target)
    {
        ai->perception.set_attention_mode(ATTENTION_IDLE, steering, ATTENTION_AWARENESS_NORMAL);
    }
    else if (params.target != nullptr)
    {
        ai->perception.pay_attention(params.target->get_handle(), AI_BELIEF_UNDEFINED, ATTENTION_LOOK_AT_OBJECT, steering, ATTENTION_AWARENESS_NORMAL);
    }
    ai->set_desired_burst_size(0);
    return false;
}

// 0x006943C0 (:1059)
void ai_combat_fire_turret_at_target(npc* np, turret* tp, const human* target_hp, bool alternate_fire)
{
    ai_system* ai = np->get_ai();
    weapon* primary = tp->get_weapon_inst();
    weapon* wp = primary;
    if (alternate_fire)
    {
        wp = tp->get_secondary_weapon_inst();
        if (wp == nullptr)
        {
            alternate_fire = false;
            wp = primary;
        }
    }
    if (wp == nullptr)
    {
        return;
    }
    const weapon_info* info = wp->w_info;
    timestamp& shoot_timer = np->combat.shoot_enemy_timestamp;
    if (shoot_timer.valid() && !shoot_timer.elapsed())
    {
        return;
    }
    const object_convoy* convoy = ai->get_convoy_object();
    if (convoy != nullptr && ((target_hp != nullptr && !ai->perception.can_hit_target()) || convoy_vehicle_in_line_of_fire(np, convoy, wp, target_hp)))
    {
        wp->refire_delay_time = 0.0f;
        return;
    }
    if (info->is_explosive() && wp->ready_to_fire() && !ai_combat_turret_test_destructive_shot(np, tp, 0.1f))
    {
        wp->refire_delay_time = 0.0f;
        return;
    }
    vector target_pos;
    tp->get_target_pos(target_pos);
    const float range_pct = get_effective_range_percent(np, vec_dist(tp->pos, target_pos), wp, nullptr, info->npc_firing_pattern, false);
    if (!ai_combat_fire_burst(np, wp, info->npc_firing_pattern, range_pct, alternate_fire))
    {
        shoot_timer.set(info->npc_firing_pattern->get_firing_interval(range_pct));
    }
}

// 0x00694560 (:1130)
void ai_combat_fire_turret_at_pos(npc* np, turret* tp, const vector& pos, bool alternate_fire)
{
    weapon* wp = alternate_fire ? tp->get_secondary_weapon_inst() : tp->get_weapon_inst();
    if (wp == nullptr)
    {
        return;
    }
    const weapon_info* info = wp->w_info;
    timestamp& shoot_timer = np->combat.shoot_enemy_timestamp;
    if (shoot_timer.valid() && !shoot_timer.elapsed())
    {
        return;
    }
    if (info->is_explosive() && wp->ready_to_fire() && !ai_combat_turret_test_destructive_shot(np, tp, 0.1f))
    {
        wp->refire_delay_time = 0.0f;
        return;
    }
    const float range_pct = get_effective_range_percent(np, vec_dist(tp->pos, pos), wp, nullptr, info->npc_firing_pattern, false);
    if (!ai_combat_fire_burst(np, wp, info->npc_firing_pattern, range_pct, alternate_fire))
    {
        shoot_timer.set(info->npc_firing_pattern->get_firing_interval(range_pct));
    }
}

// cb_data of test_destructive_shot_callback.
struct destructive_shot_data
{
    const npc* np;
    const turret* tp;
    const object* vehicle_obj;
};

// 0x0061E290 (:1203)
bool test_destructive_shot_callback(object* hit_objp, void* cb_data)
{
    const destructive_shot_data* data = static_cast<const destructive_shot_data*>(cb_data);
    if (hit_objp == nullptr || data->np == hit_objp || data->vehicle_obj == hit_objp)
    {
        return false;
    }
    if (data->tp != nullptr && data->tp->object_is_physical_part(hit_objp->get_handle()))
    {
        return false;
    }
    // Passengers of the vehicle don't block the shot.
    return data->vehicle_obj == nullptr || hit_objp->get_obj_type() != OT_HUMAN || static_cast<human*>(hit_objp)->vehicle_handle != data->vehicle_obj->get_handle();
}

// 0x0061E2E0 (:1228)
bool ai_combat_target_too_close_to_explosive_turret(const turret* tp, const object* target)
{
    if (tp == nullptr || target == nullptr)
    {
        return false;
    }
    const weapon* wp = tp->get_weapon_inst();
    if (wp == nullptr || !wp->w_info->is_explosive())
    {
        return false;
    }
    vector muzzle_pos;
    wp->get_muzzle_transform(muzzle_pos, nullptr);
    const float safe_dist = wp->w_info->m_explosion_info->m_knockdown_radius + 1.25f + vec_xz_dist(tp->pos, muzzle_pos);
    float dist_sq;
    const vehicle* vp = target->get_obj_type() == OT_HUMAN ? human_in_vehicle(static_cast<const human*>(target)) : nullptr;
    if (vp != nullptr)
    {
        vector bmin;
        vector bmax;
        vp->get_bbox(bmin, bmax);
        vector closest;
        dist_sq = bbox_find_closest_point(closest, bmin, bmax, tp->pos);
    }
    else
    {
        dist_sq = vec_dist_squared(tp->pos, target->pos);
    }
    return dist_sq <= safe_dist * safe_dist;
}

// 0x00676100 (:1277): true when nothing blocks the shot within the blast's knockdown radius.
bool ai_combat_turret_test_destructive_shot(const npc* np, const turret* tp, float test_radius)
{
    const weapon* wp = tp->get_weapon_inst();
    if (wp == nullptr)
    {
        return false;
    }
    vector muzzle_pos;
    matrix muzzle_orient;
    wp->get_muzzle_transform(muzzle_pos, &muzzle_orient);
    const explosion_info* e_info = wp->w_info->m_ai_explosion_info != nullptr ? wp->w_info->m_ai_explosion_info : wp->w_info->m_explosion_info;
    if (e_info == nullptr)
    {
        return false;
    }
    const float reach = e_info->m_knockdown_radius + 1.25f;
    vector ahead;
    ahead.x = muzzle_orient.fvec.x * reach;
    ahead.y = muzzle_orient.fvec.y * reach;
    ahead.z = muzzle_orient.fvec.z * reach;
    vector behind;
    behind.x = -muzzle_orient.fvec.x;
    behind.y = -muzzle_orient.fvec.y;
    behind.z = -muzzle_orient.fvec.z;

    destructive_shot_data data;
    data.np = np;
    data.tp = tp;
    data.vehicle_obj = World.object_from_handle<vehicle>(np->vehicle_handle);
    raycast_input ray_in;
    raycast_output ray_out;
    ray_in.radius = 0.0f;
    ray_in.m_hit_collector_type = raycast_input::COLLECT_CLOSEST_HIT;
    ray_in.filter_params = &data;
    ray_in.m_filter_info = COLLIDE_PROJECTILE;
    ray_in.m_flags = 2;
    ray_in.filter_callback = test_destructive_shot_callback;
    bool clear = true;
    if (!Turret_fire_test_use_spherecast)
    {
        // Four rays round the barrel, up, right, down and left.
        const vector offsets[4] = {
            vector(muzzle_orient.uvec.x * test_radius, muzzle_orient.uvec.y * test_radius, muzzle_orient.uvec.z * test_radius),
            vector(muzzle_orient.rvec.x * test_radius, muzzle_orient.rvec.y * test_radius, muzzle_orient.rvec.z * test_radius),
            vector(-muzzle_orient.uvec.x * test_radius, -muzzle_orient.uvec.y * test_radius, -muzzle_orient.uvec.z * test_radius),
            vector(-muzzle_orient.rvec.x * test_radius, -muzzle_orient.rvec.y * test_radius, -muzzle_orient.rvec.z * test_radius),
        };
        for (int i = 0; i < 4 && clear; i++)
        {
            const vector origin(muzzle_pos.x + offsets[i].x, muzzle_pos.y + offsets[i].y, muzzle_pos.z + offsets[i].z);
            ray_in.start = vector(origin.x + behind.x, origin.y + behind.y, origin.z + behind.z);
            ray_in.end = vector(origin.x + ahead.x, origin.y + ahead.y, origin.z + ahead.z);
            if (havok_raycast(ray_in, ray_out))
            {
                clear = false;
            }
        }
    }
    else
    {
        ray_in.start = vector(muzzle_pos.x + behind.x, muzzle_pos.y + behind.y, muzzle_pos.z + behind.z);
        ray_in.end = vector(muzzle_pos.x + ahead.x, muzzle_pos.y + ahead.y, muzzle_pos.z + ahead.z);
        ray_in.radius = test_radius;
        if (havok_spherecast(ray_in, ray_out))
        {
            clear = false;
        }
    }
    return clear;
}

// 0x005E3200 (:1366)
bool ai_combat_is_close_enough_to_use_turret(const npc* np, const turret* tp, float tolerance, vector* use_pos)
{
    const vector pos = tp->get_operation_position();
    if (use_pos != nullptr)
    {
        *use_pos = pos;
    }
    return vec_dist_squared(pos, np->pos) <= tolerance * tolerance;
}

// 0x005E3270 (:1376): within 15 degrees of the turret's heading.
bool ai_combat_is_oriented_with_turret(const npc* np, const turret* tp, matrix* use_orient)
{
    matrix orient = tp->get_operation_orient();
    const float heading = orient.extract_heading();
    orient.set(0.0f, 0.0f, heading);
    if (use_orient != nullptr)
    {
        *use_orient = orient;
    }
    return 0.9659258f <= np->orient.fvec.y * orient.fvec.y + orient.fvec.x * np->orient.fvec.x + np->orient.fvec.z * orient.fvec.z;
}

// 0x0061E4B0 (:1416)
bool ai_combat_has_scoped_weapon(const npc* np)
{
    const weapon* wp = ai_combat_get_action_firearm(np);
    return wp != nullptr && wp->w_info->flags.can_scope;
}
