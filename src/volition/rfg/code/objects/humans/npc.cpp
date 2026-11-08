// Original: volition/rfg/code/objects/humans/npc.cpp
//
// NPCs (stub_impl.md S9): a human with an ai_system, created by the spawn system. Not ported yet, still stand-ins: the
// weapon drift (aim error while firing), the cower animation, npc::clear with npc_reset_all, and the console commands.

#include "volition/rfg/code/objects/humans/npc.h"

#include "volition/rfg/code/ai/ai_system.h"
#include "volition/rfg/code/camera/rfg_camera.h"
#include "volition/rfg/code/gameplay/difficulty.h"
#include "volition/rfg/code/gamewide/game_render.h"
#include "volition/rfg/code/geom/anim.h"
#include "volition/rfg/code/geom/character_instance.h"
#include "volition/rfg/code/geom/vanim.h"
#include "volition/rfg/code/geom/vanim_shared.h"
#include "volition/rfg/code/havok/havok.h"
#include "volition/rfg/code/havok/havok_ragdoll.h"
#include "volition/rfg/code/hud/minimap.h"
#include "volition/rfg/code/level/rfg_prop_name_shared.h"
#include "volition/rfg/code/level/world.h"
#include "volition/rfg/code/multi/multi_core/include/multi.h"
#include "volition/rfg/code/multi/multi_core/include/multi_core_objects.h"
#include "volition/rfg/code/objects/humans/human_combat.h"
#include "volition/rfg/code/objects/humans/human_combat_turret.h"
#include "volition/rfg/code/objects/humans/human_info.h"
#include "volition/rfg/code/objects/humans/human_weapons.h"
#include "volition/rfg/code/objects/humans/npc_sound_info.h"
#include "volition/rfg/code/objects/humans/player.h"
#include "volition/rfg/code/objects/inventory_item.h"
#include "volition/rfg/code/objects/object_prop_names_shared.h"
#include "volition/rfg/code/objects/turret.h"
#include "volition/rfg/code/pathfinding/bb_pathfinding.h"
#include "volition/rfg/code/spawn/ambient_prop_info.h"
#include "volition/rfg/code/spawn/spawn_resource_manager.h"
#include "volition/rfg/code/vehicle/vehicle_class.h"
#include "volition/rfg/code/weapons/aim_drift.h"
#include "volition/rfg/code/weapons/weapons.h"
#include "volition/vlib/code/math/bbox.h"
#include "volition/vlib/code/math/matrix.h"
#include "volition/vlib/code/math/rand.h"
#include "volition/vlib/code/memory/object_pool.h"
#include "volition/vlib/code/os/frametime.h"
#include "volition/vlib/code/util/dictionary.h"
#include "volition/vlib/code/util/varray.h"

#include <math.h>
#include <string.h>

// Magic number: rfg.exe's pool.
static object_pool<npc,100,1> npcpool; // 0x03560AB0
static const char* DESPAWN_TYPE_NAMES[5] = { "Low", "Medium", "High", "Withdraw", "Retreat" }; // 0x0169443C
static farray<unsigned int,100> Offscreen_update_queue; // 0x01695428
static int Queue_position; // 0x0302F0A8
static bool Marauders_should_retreat; // 0x0302F0A2
static timestamp Marauder_retreat_check_timer; // 0x016955C8

// Magic numbers: a despawn of high urgency ends within 20 s; at most four NPCs update their visibility per frame; corpses
// above seven start fading; a default NPC streams at 50 m and unstreams at 55 m.
static const int HIGH_DESPAWN_MS = 20000;
static const int VISIBILITY_UPDATES_PER_FRAME = 4;
static const int MAX_CORPSES = 7;
static const float DEFAULT_LOAD_DISTANCE_SQUARED = 2500.0f;
static const float DEFAULT_UNLOAD_DISTANCE_SQUARED = 3025.0f;

// 0x00A0F150 (:534)
static int get_despawn_time(npc_despawn_type despawn_type)
{
    return despawn_type == NPC_DESPAWN_TYPE_HIGH || despawn_type == NPC_DESPAWN_TYPE_RETREAT ? HIGH_DESPAWN_MS : 0;
}

// 0x00A33250 (:552)
static void update_npc_visibility()
{
    int count = Offscreen_update_queue.size();
    if (count > VISIBILITY_UPDATES_PER_FRAME)
    {
        count = VISIBILITY_UPDATES_PER_FRAME;
    }
    for (int i = 0; i < count; i++)
    {
        if (Queue_position < 0 || Queue_position >= Offscreen_update_queue.size())
        {
            Queue_position = 0;
        }
        unsigned int handle = Offscreen_update_queue[Queue_position];
        npc* np = handle != 0xFFFFFFFF ? World.object_from_handle<npc>(handle) : nullptr;
        if (np == nullptr)
        {
            // Ordered removal: the queue position stays on the next entry.
            for (int j = Queue_position; j < Offscreen_update_queue.size() - 1; j++)
            {
                Offscreen_update_queue[j] = Offscreen_update_queue[j + 1];
            }
            Offscreen_update_queue.resize(Offscreen_update_queue.size() - 1);
        }
        else
        {
            np->update_offscreen();
            Queue_position++;
        }
    }
}

// 0x00A33320 (:592)
static void remove_npc_from_visibility_list(const npc* np)
{
    for (int i = 0; i < Offscreen_update_queue.size(); i++)
    {
        if (Offscreen_update_queue[i] == np->get_handle())
        {
            for (int j = i; j < Offscreen_update_queue.size() - 1; j++)
            {
                Offscreen_update_queue[j] = Offscreen_update_queue[j + 1];
            }
            Offscreen_update_queue.resize(Offscreen_update_queue.size() - 1);
            if (i < Queue_position)
            {
                Queue_position--;
            }
            return;
        }
    }
}

// Defined below.
void npc_process_cover_leanout_blend(npc* np); // 0x00AB8120 (:801)

// One entry of despawn_corpses' list.
struct corpse_info
{
    npc* np;
    float dist_sqrd;
    bool visible;
};

// 0x00A5AB30 (:612): fades corpses whose time is up. Above the limit, the farthest corpse out of sight goes at once, else
// the oldest one in sight whose ragdoll has settled fades over a second.
static void despawn_corpses()
{
    corpse_info corpses[100];
    int num_corpses = 0;
    for (int i = 0; i < World.get_num_objects<npc>(); i++)
    {
        npc* np = World.get_object<npc>(i);
        if (!np->obj_flags._dont_use_me || np->npc_flags.despawn_after_fade || !np->is_killed() || !np->hflags.death_func_done || np->npc_flags.spawned_as_corpse)
        {
            continue;
        }
        if (np->corpse_cleanup_timer.elapsed())
        {
            np->fade_out_and_despawn(1000, false);
            continue;
        }
        float dx = np->pos.x - Rfg_camera.real_pos.x;
        float dz = np->pos.z - Rfg_camera.real_pos.z;
        vector p0 = np->approx_local_bmin.copy_unrotate(np->orient);
        vector p1 = np->approx_local_bmax.copy_unrotate(np->orient);
        p0.x += np->pos.x;
        p0.y += np->pos.y;
        p0.z += np->pos.z;
        p1.x += np->pos.x;
        p1.y += np->pos.y;
        p1.z += np->pos.z;
        bbox bounds;
        bbox_create_from_points(bounds, p0, p1);
        corpses[num_corpses].np = np;
        corpses[num_corpses].dist_sqrd = dx * dx + dz * dz;
        corpses[num_corpses].visible = game_render_is_object_visible(bounds.min, bounds.max);
        num_corpses++;
    }
    if (num_corpses <= MAX_CORPSES)
    {
        return;
    }
    corpse_info* farthest_hidden = nullptr;
    corpse_info* oldest_visible = nullptr;
    for (int i = 0; i < num_corpses; i++)
    {
        corpse_info* corpse = &corpses[i];
        if (!corpse->visible)
        {
            if (farthest_hidden == nullptr || farthest_hidden->dist_sqrd < corpse->dist_sqrd)
            {
                farthest_hidden = corpse;
            }
        }
        else if (oldest_visible == nullptr ||
                 (corpse->np->ragdoll_idx != -1 && corpse->np->corpse_cleanup_timer.time_since() > oldest_visible->np->corpse_cleanup_timer.time_since() && !havok_ragdoll_is_simulating(corpse->np->ragdoll_idx)))
        {
            oldest_visible = corpse;
        }
    }
    if (farthest_hidden != nullptr)
    {
        farthest_hidden->np->fade_out_and_despawn(0, false);
    }
    else if (oldest_visible != nullptr)
    {
        oldest_visible->np->fade_out_and_despawn(1000, false);
    }
}

// 0x00A0F190 (:855)
void types_npc(signed char& type, signed char& subtype)
{
    type = OT_HUMAN;
    subtype = OT_SUB_HUMAN_NPC;
}

// 0x00A0F1A0 (:855)
int bpo_index_npc()
{
    return 0;
}

// 0x00A0F1B0 (:855)
const char* classname_npc()
{
    return "npc";
}

// 0x00A5AF00 (:855)
object* alloc_npc()
{
    return npcpool.alloc(0);
}

static obj_class_instantiator npc_instantiator("npc", alloc_npc, types_npc, bpo_index_npc, classname_npc, npc::pool_remaining, 0x54E80, 100); // 0x033C1448

// 0x00A0F1C0 (:855)
unsigned int npc::get_classname_hash()
{
    return npc_instantiator.this_hash;
}

// 0x00A333A0 (:855)
int npc::pool_remaining()
{
    return npcpool.get_free();
}

// 0x00A83A90 (:855)
void npc::pool_free()
{
    npcpool.free(this);
}

// 0x00A9DD10 (:856)
npc::npc()
{
}

// 0x00A9DD70
npc::~npc()
{
}

// 0x00A06030 (.h:277)
const char* npc::get_classname_static()
{
    return "npc";
}

// 0x00A9DD60 (.h:277)
const char* npc::get_classname() const
{
    return "npc";
}

// 0x00A333B0 (:861)
void npc::destroy_notify()
{
    human::destroy_notify();
    if (is_killed())
    {
        ai->do_death_func();
    }
    else
    {
        ai->leave_squad();
    }
}

// 0x00A333E0 (:888)
bool npc::is_offscreen() const
{
    update_offscreen();
    return time_offscreen.valid();
}

// 0x00A0F2E0 (:897)
int npc::get_offscreen_period() const
{
    return time_offscreen.valid() ? time_offscreen.time_since() : -1;
}

// 0x00A0F300 (:909): a 1.25 m sphere at chest height.
void npc::update_offscreen() const
{
    npc* self = const_cast<npc*>(this);
    if (obj_flags.streamed)
    {
        vector center(pos.x, pos.y + 0.85f, pos.z);
        if (game_render_cull_sphere(center, 1.25f))
        {
            if (!time_offscreen.valid())
            {
                self->time_offscreen.set(0);
            }
            return;
        }
    }
    self->time_offscreen.invalidate();
}

// 0x00ADD9C0 (:942)
bool npc::serialize(obj_prop_block& pblock, bool load)
{
    npc_flags_struct prop_flags;
    prop_flags.reset();
    if (!human::serialize(pblock, load))
    {
        return false;
    }
    if (!load)
    {
        return true;
    }
    obj_flags.no_save = 1;
    start_navpoint = nullptr;
    npc_flags.reset();
    management_system = NPC_MANAGER_INVALID;
    drift_info.desired_error_mag = 0.0f;
    drift_info.actual_error_mag = 0.0f;
    drift_info.acquire_time.set(0);
    drift_info.change_time.set(0);
    drift_info.next_error_pos.x = 1.0f;
    drift_info.next_error_pos.y = 0.0f;
    drift_info.prev_error_pos.x = -1.0f;
    drift_info.prev_error_pos.y = 0.0f;
    mp_flags.is_multiplayer = 0;
    mp_flags.jump_id = 0;
    final_despawn_timer.invalidate();
    time_offscreen.invalidate();
    spawn_node_handle = 0xFFFFFFFF;
    despawn_obj_handle = 0xFFFFFFFF;
    srm_register_npc(this);
    if (pblock.get_buf_copy_optional(NPC_PROP_NPC_FLAGS, (unsigned char*)&prop_flags, sizeof(prop_flags)) != 0)
    {
        npc_flags_set_flags(npc_flags, prop_flags);
    }
    pblock.get_optional(HUMAN_PROP_SPAWN_PRIORITY, spawn_priority);
    pblock.get_optional(HUMAN_PROP_MANAGEMENT_SYSTEM, management_system);
    if (!pblock.get_optional(HUMAN_PROP_CREATION_SYSTEM, creation_system))
    {
        creation_system = management_system;
    }
    ai = ai_system_create();
    if (ai == nullptr)
    {
        return false;
    }
    ai->init(this);
    if (Multiplayer && multi_is_server())
    {
        remote_obj_flags.remote_object = 1;
        multi_core_object_create_remote_object(5, get_handle());
    }
    m_stream_load_squared_dist = DEFAULT_LOAD_DISTANCE_SQUARED;
    m_stream_unload_squared_dist = DEFAULT_UNLOAD_DISTANCE_SQUARED;
    if (npc_flags.use_default_team)
    {
        set_team(h_info->default_team);
    }
    obj_flags.no_save = 1;
    obj_flags.destroy_on_stream = 1;
    return true;
}

// 0x00AC1D30 (:1088)
bool npc::stream(bool load)
{
    if (load)
    {
        Offscreen_update_queue.add_unique(get_handle());
    }
    else
    {
        remove_npc_from_visibility_list(this);
        if (!is_killed())
        {
            ai->register_destream();
        }
    }
    if (!human::stream(load))
    {
        return false;
    }
    if (load)
    {
        if (!npc_flags.immediate_spawn)
        {
            hflags.fading_in = 1;
            hflags.fading_out_from_nano = 0;
            hflags.fading_out = 0;
            fade_timer.set(0);
            fade_time = 1000;
        }
        npc_flags.immediate_spawn = 0;
        if (amb_prop_info != nullptr)
        {
            amb_prop_create(amb_prop_info, this);
        }
    }
    return true;
}

// 0x00ACCDC0 (:1138)
void npc::free_this()
{
    ai->leave_squad();
    ai->reset();
    ai_system_release(ai);
    srm_remove_npc(this);
    if (npc_flags.remove_icon_on_death)
    {
        Cartographer.remove_icon(get_handle());
    }
    human::free_this();
}

// 0x00A0F390 (:1177)
void npc::get_display_name(char* output)
{
    if (name[0] == '\0')
    {
        strcpy(output, h_info->name);
        return;
    }
    size_t length = strcspn(name, "#01234567890");
    if (length == 0)
    {
        strcpy(output, name);
        return;
    }
    strncpy_safe(output, name, (int)length + 1);
}

// 0x00A0F410 (:1197)
bool npc::should_active_despawn() const
{
    return npc_flags.despawn_npc && final_despawn_timer.valid() && final_despawn_timer.elapsed();
}

// 0x00A0F450 (:1207)
bool npc::should_despawn() const
{
    return npc_flags.despawn_npc;
}

// 0x00A0F460 (:1213)
void npc::set_despawn_flags(npc_despawn_type desired_despawn_type)
{
    npc_flags.despawn_npc = 1;
    switch (desired_despawn_type)
    {
    case NPC_DESPAWN_TYPE_LOW:
        break;
    case NPC_DESPAWN_TYPE_MEDUIM:
        npc_flags.high_priority_despawn = 1;
        break;
    case NPC_DESPAWN_TYPE_HIGH:
    case NPC_DESPAWN_TYPE_WITHDRAW:
    case NPC_DESPAWN_TYPE_RETREAT:
        npc_flags.high_priority_despawn = 1;
        npc_flags.disallow_despawn_engagement = 1;
        break;
    default:
        // Original: Int3 critical function here.
        break;
    }
}

// 0x00A0F4D0 (:1260)
void npc::start_active_despawn(npc_despawn_type desired_despawn_type, const object* despawn_objp)
{
    if (npc_flags.converted_guerrilla)
    {
        return;
    }
    set_despawn_flags(desired_despawn_type);
    int despawn_time = get_despawn_time(desired_despawn_type);
    if (despawn_time != 0 && (!final_despawn_timer.valid() || final_despawn_timer.time_until() > despawn_time))
    {
        final_despawn_timer.set(despawn_time);
    }
    if (despawn_objp != nullptr)
    {
        despawn_obj_handle = despawn_objp->get_handle();
    }
}

// 0x00A0F540 (:1296)
object* npc::get_despawn_object() const
{
    return World.object_from_handle(despawn_obj_handle);
}

// 0x00A0F560 (:1302)
bool npc::despawn_overdue() const
{
    return final_despawn_timer.valid() && final_despawn_timer.elapsed() && final_despawn_timer.time_since() > 0;
}

// 0x00A33400 (:1321)
bool npc::is_in_combat() const
{
    // Magic number: bin 1 holds the hostile beliefs.
    return ai->perception.belief_bins[1].beliefs != nullptr;
}

// 0x00A0F5A0 (:1407)
void npc::load_resources()
{
    // Original: Int3 critical function here.
}

// 0x00A0F5C0 (:1412)
bool npc::set_team(human_teams new_team)
{
    human_teams old_team = current_team;
    if ((int)new_team < -1 || (int)new_team >= NUM_HUMAN_TEAMS)
    {
        return false;
    }
    current_team = new_team;
    if (new_team != old_team)
    {
        ai->register_team_change(old_team, new_team);
        if (char_inst != nullptr && char_inst->vp != nullptr && old_team == HUMAN_TEAM_CIVILIAN && new_team != HUMAN_TEAM_CIVILIAN)
        {
            switch (vanim_get_base_animation_set_variant(char_inst->vp))
            {
            case ANIM_SET_VARIANT_WOUNDED:
            case ANIM_SET_VARIANT_WOUNDED_BACK:
            case ANIM_SET_VARIANT_WOUNDED_LEFT_LEG:
            case ANIM_SET_VARIANT_WOUNDED_RIGHT_LEG:
            case ANIM_SET_VARIANT_DRUNK:
            case ANIM_SET_VARIANT_AFRAID:
            case ANIM_SET_VARIANT_OPPRESSED:
            case ANIM_SET_VARIANT_TIRED:
                vanim_set_base_animation_set_variant(char_inst->vp, ANIM_SET_VARIANT_NONE);
                break;
            default:
                break;
            }
        }
    }
    return true;
}

// 0x00A0F660 (:1449)
void npc::fade_out_and_despawn(int _fade_time, bool fading_from_nano)
{
    if (npc_flags.despawn_after_fade)
    {
        return;
    }
    npc_flags.despawn_after_fade = 1;
    hflags.fading_out = 1;
    hflags.fading_in = 0;
    hflags.fading_out_from_nano = fading_from_nano;
    fade_timer.set(0);
    fade_time = _fade_time;
}

// 0x00A5AF50 (:1511)
void npc::weapon_drift_start_acquire()
{
    const weapon_info* info = human_weapons_get_weapon_info(this);
    if (info != nullptr)
    {
        drift_info.acquire_time.set(info->npc_aim_drift_profile->acquire_time);
    }
}

// 0x00A0F6C0 (:1519). LTCG: heading in EAX, box_coords in EDI, adpp in ESI, the rest on the stack. The error sideways is
// box_coords.x of err_radians; the error up or down leans from one scale to the other with box_coords.y, within the angle
// that max_vertical_error_dist makes at this distance.
static void calculate_drift_params(float& heading, float& pitch, const vector2& box_coords, float err_radians, float dist_to_target, const aim_drift_profile* adpp)
{
    if (adpp == nullptr)
    {
        return;
    }
    heading = box_coords.x * err_radians;
    const float max_angle = (float)atan2(adpp->max_vertical_error_dist, dist_to_target);
    float up = adpp->pos_vertical_scale * err_radians;
    float down = -(adpp->neg_vertical_scale * err_radians);
    if (0.0f <= up - max_angle)
    {
        up = max_angle;
    }
    if (down - -max_angle < 0.0f)
    {
        down = -max_angle;
    }
    pitch = -((1.0f - box_coords.y) * down + box_coords.y * up);
}

// 0x00AB82B0 (:1540): how far off this npc's aim is, into the spine bend.
void npc::weapon_drift_process()
{
    unsigned int weapon_handle;
    const turret* manned = human_combat_get_manned_turret(this);
    if (manned != nullptr)
    {
        weapon_handle = manned->primary_weapon_handle;
    }
    else
    {
        if (equipped_inv_item == nullptr)
        {
            return;
        }
        weapon_handle = equipped_inv_item->weapon_handle;
    }
    object* weapon_obj = weapon_handle != 0xFFFFFFFF ? World.object_from_handle(weapon_handle) : nullptr;
    if (weapon_obj == nullptr || weapon_obj->get_obj_type() != OT_ITEM || weapon_obj->get_sub_type() != OT_SUB_ITEM_WEAPON)
    {
        return;
    }
    const weapon* wp = static_cast<const weapon*>(weapon_obj);
    // Magic numbers: the ammo types that drift, as numbers in the asm.
    const int ammo_type = (int)wp->w_info->ammo_type;
    if (ammo_type != 0 && ammo_type != 1 && ammo_type != 4 && ammo_type != 5 && ammo_type != 6 && ammo_type != 7)
    {
        return;
    }
    const aim_drift_profile* profile = wp->w_info->npc_aim_drift_profile;
    if (profile == nullptr)
    {
        return;
    }
    if (wp->is_reloading())
    {
        weapon_drift_start_acquire();
        return;
    }
    const object* turret_obj = wp->turret_handle != 0xFFFFFFFF ? World.object_from_handle(wp->turret_handle) : nullptr;
    if ((turret_obj == nullptr || turret_obj->get_obj_type() != OT_TURRET) && (look_at_handle == 0xFFFFFFFF || hflags.aim_at_pos == 0))
    {
        return;
    }
    if (drift_info.change_time.elapsed())
    {
        drift_info.prev_error_pos = drift_info.next_error_pos;
        drift_info.next_error_pos.x = -drift_info.next_error_pos.x;
        drift_info.next_error_pos.y = fl_rand_range(0.0f, 1.0f);
        drift_info.change_time.set(int_rand_range(profile->min_change_direction_time, profile->max_change_direction_time));
    }
    object* target = World.object_from_handle(look_at_handle);
    const human* target_human = target != nullptr && target->get_obj_type() == OT_HUMAN ? static_cast<const human*>(target) : nullptr;
    const vehicle* target_vehicle = target != nullptr && target->get_obj_type() == OT_VEHICLE ? reinterpret_cast<const vehicle*>(target) : nullptr;
    if (target_human == this)
    {
        return;
    }
    float target_speed = 0.0f;
    if (target_human != nullptr)
    {
        target_speed = target_human->actual_velocity.mag();
    }
    else if (target_vehicle != nullptr)
    {
        vehicle_get_speed(target_vehicle, target_speed);
    }
    drift_info.desired_error_mag = profile->min_horizontal_error;
    if (is_offscreen())
    {
        drift_info.desired_error_mag += profile->offscreen_penalty;
    }
    if (object_is_player(target_human))
    {
        drift_info.desired_error_mag += profile->player_penalty;
        if (human_is_in_cover(target_human))
        {
            drift_info.desired_error_mag += profile->player_in_cover_penalty;
        }
    }
    if (!drift_info.acquire_time.elapsed())
    {
        drift_info.desired_error_mag += drift_info.acquire_time.percent() * profile->max_acquire_penalty;
    }
    drift_info.desired_error_mag += actual_velocity_mag * profile->my_speed_penalty;
    drift_info.desired_error_mag += profile->target_speed_penalty * target_speed;
    if (drift_info.desired_error_mag - 0.0f < 0.0f)
    {
        drift_info.desired_error_mag = 0.0f;
    }
    if (0.0f <= drift_info.desired_error_mag - profile->max_horizontal_error)
    {
        drift_info.desired_error_mag = profile->max_horizontal_error;
    }
    drift_info.desired_error_mag *= h_info->aim_error_pct;
    if (cur_stance == HST_CROUCH && ((cur_state == HS_STANDARD && cur_movement_state == HMS_IDLE) || cur_state == HS_COVER))
    {
        drift_info.desired_error_mag *= profile->kneeling_bonus_pct;
    }
    if (npc_flags.return_fire_aim_bonus != 0)
    {
        drift_info.desired_error_mag *= profile->return_fire_bonus_pct;
    }
    float change = drift_info.desired_error_mag - drift_info.actual_error_mag;
    change *= difficulty_get_value(DIFFICULTY_ENEMY_AIM_DRIFT, NUM_DIFFICULTY_LEVELS);
    if (change <= 0.0f)
    {
        const float limit = -(profile->max_decay_rate * Frametime);
        if (change - limit < 0.0f)
        {
            change = limit;
        }
    }
    else
    {
        const float limit = profile->max_grow_rate * Frametime;
        if (0.0f <= change - limit)
        {
            change = limit;
        }
    }
    drift_info.actual_error_mag += change;
    // Magic number: degrees to radians, as rfg.exe's constant reads.
    const float err_radians = drift_info.actual_error_mag * 0.017453f;
    const float percent = drift_info.change_time.percent();
    drift_info.cur_error_pos.x = percent * drift_info.prev_error_pos.x + drift_info.next_error_pos.x * (1.0f - percent);
    drift_info.cur_error_pos.y = drift_info.prev_error_pos.y * percent + drift_info.next_error_pos.y * (1.0f - percent);
    vector look_at;
    human_get_look_at_vector(this, look_at);
    calculate_drift_params(spinebend_data.heading_drift, spinebend_data.pitch_drift, drift_info.cur_error_pos, err_radians, look_at.mag(), profile);
}

// 0x00AB8120 (:801): the blend input of a lean out of cover, from where the npc looks against its node.
void npc_process_cover_leanout_blend(npc* np)
{
    const unsigned int node_handle = np->combat.reserved_cover_node;
    const object* node = node_handle != 0xFFFFFFFF ? World.object_from_handle(node_handle) : nullptr;
    if (node == nullptr || node->get_obj_type() != OT_COVER_NODE)
    {
        return;
    }
    vector eye_pos;
    human_get_eye_pos_orient(np, eye_pos, nullptr);
    vector look_at;
    human_get_look_at_vector_from_eye_pos(np, eye_pos, look_at);
    look_at.normalize();
    float control_value = -1.0f;
    switch (np->movement_submode)
    {
    case HMSM_COVER_RIGHT_LEAN:
        control_value = look_at.copy_flatten_normalize_safe() * node->orient.rvec;
        break;
    case HMSM_COVER_LEFT_LEAN:
        control_value = -(look_at.copy_flatten_normalize_safe() * node->orient.rvec);
        break;
    case HMSM_COVER_RIGHT_LEANUP:
    case HMSM_COVER_LEFT_LEANUP:
        control_value = look_at.y;
        break;
    default:
        break;
    }
    vanim_set_blend_tree_control_input(np->char_inst->vp, VANIM_STATE_DESIRED, control_value);
}

// 0x00A05FE0 (.h:257)
void npc_weapon_drift_info::clear()
{
    desired_error_mag = 0.0f;
    actual_error_mag = 0.0f;
    acquire_time.set(0);
    change_time.set(0);
    next_error_pos.x = 1.0f;
    next_error_pos.y = 0.0f;
    prev_error_pos.x = -1.0f;
    prev_error_pos.y = 0.0f;
}

// 0x00A0F1D0
npc_weapon_drift_info::npc_weapon_drift_info()
{
}

// 0x00A0F7C0 (:1792)
void npc_level_init_post()
{
    Marauder_retreat_check_timer.set(5000);
    NPC_male_guerrilla_anim_set = anim_set_find("Male");
}

// 0x00A83DD0 (:1800)
void npc_level_shutdown()
{
    for (int i = 0; i < World.get_num_objects<npc>(); i++)
    {
        World.destroy_object(World.get_object<npc>(i), false);
    }
}

// 0x00AE6560 (:1807)
void npc_process_all()
{
    bb_pf_check_for_obstructed_destinations();
    bb_pf_check_extended_stuck();
    npc_sound_info::send_queued();
    ai_system_process_alert_level_change();
    int count = World.get_num_objects<npc>();
    for (int i = 0; i < count; i++)
    {
        npc* np = World.get_object<npc>(i);
        np->hflags.processed_this_frame = 0;
        if (np->obj_flags.destroyed)
        {
            continue;
        }
        if (!World.is_zone_streamed(np->pos))
        {
            World.destroy_object(np, false);
        }
        else if (!np->hflags.hidden && np->rdd.is_loaded() && np->char_inst != nullptr && np->char_inst->rdd.is_loaded())
        {
            np->hflags.processed_this_frame = 1;
            np->get_ai()->do_frame_pre();
        }
    }
    for (int i = 0; i < count; i++)
    {
        npc* np = World.get_object<npc>(i);
        if (np->hflags.processed_this_frame)
        {
            np->get_ai()->do_frame();
            if (!np->is_killed())
            {
                np->weapon_drift_process();
            }
            if (np->cur_state == HS_COVER && (unsigned int)(np->movement_submode - HMSM_COVER_RIGHT_LEAN) < 4)
            {
                npc_process_cover_leanout_blend(np);
            }
            human_do_frame(np, true);
        }
    }
    update_npc_visibility();
    despawn_corpses();
}

// 0x00AB2F10 (:1925)
void npc_process_all_no_animation()
{
    for (int i = 0; i < World.get_num_objects<npc>(); i++)
    {
        npc* np = World.get_object<npc>(i);
        if (np->hflags.processed_this_frame)
        {
            human_do_frame_no_animation(np);
        }
    }
}

// 0x00AD5900 (:1937)
void npc_process_post_transform(npc* np)
{
    if (np->npc_flags.spawned_as_corpse && !np->is_killed())
    {
        human_damage(np, nullptr, -1.0f, DBT_NONE, nullptr, nullptr, true, false);
        inventory_item* item = np->inventory;
        while (item != nullptr)
        {
            inventory_item* next = item->next;
            bool last = next == np->inventory;
            human_delete_item_from_inventory(np, item, false, false);
            item = last ? nullptr : next;
        }
        human_switch_to_dead_state(np);
        if (np->ragdoll_idx != -1)
        {
            vector velocity;
            velocity.rand_sphere();
            havok_ragdoll_set_velocity(np->ragdoll_idx, velocity);
        }
    }
    human_weapons_process_human(np);
    if (np->npc_flags.despawn_after_fade && !np->hflags.fading_out)
    {
        World.destroy_object(np, false);
    }
}

// 0x00A5AF80 (:1968)
void npc_kill_all()
{
    for (int i = 0; i < World.get_num_objects<npc>(); i++)
    {
        World.destroy_object(World.get_object<npc>(i), false);
    }
}

// 0x00A5AFD0 (:1979)
void npc_free_all()
{
    for (int i = 0; i < World.get_num_objects<npc>(); i++)
    {
        World.destroy_object(World.get_object<npc>(i), false);
    }
}

// 0x00A0F7F0 (:2152)
void npc_flags_bitwise_or(const npc_flags_struct& s1, const npc_flags_struct& s2, npc_flags_struct& return_value)
{
    for (int i = 0; i < sizeof(npc_flags_struct); i++)
    {
        ((unsigned char*)&return_value)[i] = ((const unsigned char*)&s1)[i] | ((const unsigned char*)&s2)[i];
    }
}

// 0x00A0F820 (:2169)
void npc_flags_set_flags(npc_flags_struct& flags, const npc_flags_struct& flags_mask)
{
    for (int i = 0; i < sizeof(npc_flags_struct); i++)
    {
        ((unsigned char*)&flags)[i] |= ((const unsigned char*)&flags_mask)[i];
    }
}

// 0x00A0F840 (:2183)
void npc_flags_bitwise_clear(const npc_flags_struct& s1, const npc_flags_struct& s2, npc_flags_struct& return_value)
{
    for (int i = 0; i < sizeof(npc_flags_struct); i++)
    {
        ((unsigned char*)&return_value)[i] = ~((const unsigned char*)&s1)[i] & ((const unsigned char*)&s2)[i];
    }
}

// 0x00A0F870 (:2275)
void npc_set_load_distance(npc* p_npc, float distance)
{
    p_npc->m_stream_load_squared_dist = distance * distance;
    p_npc->m_stream_unload_squared_dist = (distance + 5.0f) * (distance + 5.0f);
}

// 0x00A0F8B0 (:2282)
bool npc_marauders_should_retreat()
{
    return Marauders_should_retreat;
}

// 0x00A0F8C0 (:2288)
void npc_maybe_set_male_guerrilla_anim_set(npc* np)
{
    if (NPC_male_guerrilla_anim_set != nullptr && !np->h_info->flags.female && (np->current_team == HUMAN_TEAM_GUERILLA || np->current_team == HUMAN_TEAM_CIVILIAN) && np->char_inst != nullptr && np->char_inst->vp != nullptr)
    {
        np->char_inst->vp->animation_set = NPC_male_guerrilla_anim_set;
    }
}

// 0x00A33520 (:2306)
bool npc_is_defending_player(const human* const hp)
{
    if (hp == nullptr || hp->get_obj_type() != OT_HUMAN || hp->get_sub_type() != OT_SUB_HUMAN_NPC || ((const npc*)hp)->get_ai() == nullptr)
    {
        return false;
    }
    const object* defended = ((const npc*)hp)->get_ai()->get_defend_object();
    return defended != nullptr && defended->get_obj_type() == OT_HUMAN && defended->get_sub_type() == OT_SUB_HUMAN_PLAYER;
}

// 0x00A0F910 (:2324)
npc_despawn_type npc_get_despawn_type(const char* despawn_type)
{
    for (int i = 0; i < NUM_NPC_DESPAWN_PRIORITIES; i++)
    {
        if (v_stricmp(DESPAWN_TYPE_NAMES[i], despawn_type) == 0)
        {
            return (npc_despawn_type)i;
        }
    }
    return INVALID_NPC_DESPAWN_PRIORITY;
}

// 0x005FB670 (.h:100)
npc_flags_struct::npc_flags_struct()
{
    reset();
}

// 0x005DCAA0 (.h:243)
void npc_flags_struct::reset()
{
    memset(this, 0, sizeof(*this));
}

// 0x005DCAE0 (.h:373)
bool npc::vehicle_exit_banned() const
{
    return hflags.disallow_vehicle_exit || hflags.limited_vehicle_exit;
}

// 0x00A06040 (.h:388)
void npc::change_manager(npc_manager_systems new_manager)
{
    management_system = new_manager;
}

// 0x00A06050 (.h:390)
npc_manager_systems npc::get_creator() const
{
    return creation_system;
}

// 0x00A83CB0 (:1468). States 10 and 11 stand, 12 and 13 crouch; someone armed only ever gets 10.
void npc::play_random_cower_animation(bool crouch_only)
{
    const turret* tp = human_combat_get_manned_turret(this);
    unsigned int weapon_handle = 0xFFFFFFFF;
    if (tp != nullptr)
    {
        weapon_handle = tp->primary_weapon_handle;
    }
    else if (equipped_inv_item != nullptr)
    {
        weapon_handle = equipped_inv_item->weapon_handle;
    }
    const object* weapon_obj = weapon_handle != 0xFFFFFFFF ? World.object_from_handle(weapon_handle) : nullptr;
    const bool armed = weapon_obj != nullptr && weapon_obj->get_obj_type() == OT_ITEM && weapon_obj->get_sub_type() == OT_SUB_ITEM_WEAPON;
    int state;
    if (armed)
    {
        if (crouch_only)
        {
            return;
        }
        state = 10;
    }
    else
    {
        state = int_rand_range(crouch_only ? 12 : 10, 13);
    }
    int action = -1;
    switch (state)
    {
    case 10:
        cur_stance = HST_STAND;
        action = 356; // numeric in rfg.exe
        break;
    case 11:
        cur_stance = HST_STAND;
        break;
    case 12:
    case 13:
        cur_stance = HST_CROUCH;
        break;
    default:
        keen::debug::nativePrintArgs("Int3 critical function at %s(%d)\n");
        return;
    }
    char_inst->set_state(index_alias(state, ANIM_ALIAS_NONE), index_alias(action, ANIM_ALIAS_NONE), 0.3f, false);
}
