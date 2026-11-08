// Original: volition/rfg/code/objects/projectile.cpp
//
// Partial (multiplayer.md MP8): the projectile object (class registration, serialization, streaming, freeing), its
// per-frame processing (rockets and their propellant, grenades, proxy mines, remote charges, cloud splines, fuses,
// the safe broadphase), collisions (detonate, bounce, attach to objects and humans, damage on impact, shield
// deflection, nano rifle and repair hits), detonation, the charge and mine helpers and the multiplayer lifetime limits.
// weapons.md WP1 added the harpoon/grinder impaling, WP3 the singularity bomb, WP5 the mass driver hits.
// Compiled in rfg_objects_unity.cpp (after object.cpp, whose Charge_list it uses).
// The external interface is declared in projectile.h.

#include "volition/rfg/code/objects/projectile.h"

#include "volition/audio_lib/code/audiolib_api.h"
#include "volition/rfg/code/ai/ai_system.h"
#include "volition/rfg/code/ai/percept.h"
#include "volition/rfg/code/camera/camera_shake.h"
#include "volition/rfg/code/camera/rfg_camera.h"
#include "volition/rfg/code/code_effects/bullet_sparks.h"
#include "volition/rfg/code/code_effects/explosion.h"
#include "volition/rfg/code/code_effects/explosion_shared.h"
#include "volition/rfg/code/destruction/rfg_dynamics.h"
#include "volition/rfg/code/destruction/rfg_materials.h"
#include "volition/rfg/code/destruction/rfg_mover.h"
#include "volition/rfg/code/effects/effects.h"
#include "volition/rfg/code/effects/object_decal.h"
#include "volition/rfg/code/effects/object_effect.h"
#include "volition/rfg/code/gameplay/activities/activity_courier_chase.h"
#include "volition/rfg/code/gameplay/air_bombing.h"
#include "volition/rfg/code/gameplay/gameplay.h"
#include "volition/rfg/code/gameplay/missions.h"
#include "volition/rfg/code/gameplay/upgrades.h"
#include "volition/rfg/code/gameplay/wrecking_crew/wrecking_crew.h"
#include "volition/rfg/code/gamewide/tweak_table.h"
#include "volition/rfg/code/geom/character_instance.h"
#include "volition/rfg/code/havok/havok.h"
#include "volition/rfg/code/havok/havok_raycast.h"
#include "volition/rfg/code/havok/havok_ragdoll.h"
#include "volition/rfg/code/hud/hud.h"
#include "volition/rfg/code/interface/localization/localization_shared.h"
#include "volition/rfg/code/interface/ui_document_hud_tool_tips.h"
#include "volition/rfg/code/level/world.h"
#include "volition/rfg/code/multi/multi_core/include/multi.h"
#include "volition/rfg/code/multi/multi_game/modes/multi_game_mode_common.h"
#include "volition/rfg/code/multi/multi_game/multi_game_client_create.h"
#include "volition/rfg/code/multi/multi_game/multi_game_object_projectile.h"
#include "volition/rfg/code/multi/multi_game/multi_game_settings.h"
#include "volition/rfg/code/multi/multi_game/multi_game_weapons.h"
#include "volition/rfg/code/multi/multi_game/multi_game_xp.h"
#include "volition/rfg/code/objects/humans/human.h"
#include "volition/rfg/code/objects/humans/human_combat.h"
#include "volition/rfg/code/objects/humans/human_ragdoll.h"
#include "volition/rfg/code/objects/humans/human_weapons.h"
#include "volition/rfg/code/objects/humans/npc.h"
#include "volition/rfg/code/objects/humans/npc_sound_info.h"
#include "volition/rfg/code/objects/humans/player.h"
#include "volition/rfg/code/objects/movers/general_mover.h"
#include "volition/rfg/code/objects/inventory_item.h"
#include "volition/rfg/code/objects/object_constraint.h"
#include "volition/rfg/code/objects/object_prop_names_shared.h"
#include "volition/rfg/code/objects/turret.h"
#include "volition/rfg/code/scripting/action_script/global_event_data.h"
#include "volition/rfg/code/scripting/action_script/global_event_manager.h"
#include "volition/rfg/code/vehicle/vehicle_class.h"
#include "volition/rfg/code/vehicle/vehicle_system.h"
#include "volition/rfg/code/vehicle/walker.h"
#include "volition/rfg/code/weapons/weapons.h"
#include "volition/vlib/code/math/floating.h"
#include "volition/vlib/code/math/rand.h"
#include "volition/vlib/code/os/frametime.h"
#include "volition/vlib/code/util/string_comp.h"

#include "keen/base/trace.hpp"

#include <math.h>

// The bits of weapon_info::projectile_info.flags, in the order of Weapon_info_projectile_flag_names (the names are in
// rfg.exe, these constants are not).
enum projectile_info_flag
{
    PIF_ROCKET_FLIGHT = 0x1,
    PIF_ATTACH_ON_IMPACT = 0x2,
    PIF_DETONATE_ON_IMPACT = 0x4,
    PIF_DETONATE_ON_EXPLOSION_IMPACT = 0x8,
    PIF_BOUNCE_ON_IMPACT = 0x10,
    PIF_ATTACH_TO_GROUND = 0x20,
    PIF_ALIGN_ORIENT_TO_FLIGHTPATH = 0x40,
    PIF_DONT_ATTACH_TO_HUMANS = 0x400,
    PIF_DAMAGE_ON_IMPACT = 0x1000,
    PIF_AIR_BOMB = 0x2000,
    PIF_USE_CLOUD_SPLINE = 0x4000,
    PIF_IMMEDIATE_EFFECT_DETACH = 0x8000,
    PIF_ALLOW_MID_AIR_DETONATION = 0x10000,
    PIF_ATTACH_ORIENT_TO_FLIGHT_PATH = 0x20000,
};

// ---- file-static globals
// const float SILENT_DEATH_TREAT_AS_NORMAL_DIST_SQ (0x01310FFC) is used here too; placed in player.cpp.
// const float COVER_NODE_POPUP_HEIGHT_ADJUSTMENT (0x01311870) is used here too; placed in human.cpp.
// const float NEAR_FOLLOW_DEST_DIST_SQ (0x01311B1C) is used here too; placed in human_spinebend.cpp.
static int8_t Reconstructor_passes = 2; // 0x01694701, initial data 0201ff78 (the low byte)
static bool Projectile_random_spline = true; // 0x01694702
static int RFG_charge_delay = 250; // 0x01694728, ms between the charges of one detonation
static float Grinder_attach_speed = 30.0f; // 0x0169472C
static float Grinder_attach_dp = 0.6f; // 0x01694730
static float Grinder_attach_offset = 0.11f; // 0x01694734
static float Grinder_ragdoll_impulse_scale = 1.0f; // 0x0169473C
static float Harpoon_attach_speed = 15.0f; // 0x01694740
static float Harpoon_attach_dp = 0.85f; // 0x01694744
static float Harpoon_ragdoll_impulse_scale = 0.2f; // 0x01694748
static float Harpoon_effect_offset = 0.25f; // 0x0169474C
static int Mass_driver_vehicle_damage = 3000; // 0x01694754
static float Reconstructor_radius = 3.0f; // 0x0169475C
static int Client_predict_settle_time = 50; // 0x01694760, ms a client keeps its prediction of an attached projectile
static float Singularity_bomb_vacuum_multiplier = 1.0f; // 0x01694764
static float Singularity_bomb_ragdoll_start_range = 5.0f; // 0x01694768
// farray<object*,16384> Object_update_render_list (0x0169D028) is used here too; placed in object.cpp.
// Placed here: player.cpp has no copy. A detonation lets the local player fire a charge again for 250 ms.
timestamp Local_player_recent_charge_detonation; // 0x016AE114
// These get their values from tweak_table.xtbl only.
static float SB_full_falloff; // 0x03017C68
static float SB_full_falloff_wc; // 0x0301A21C
static int Proxy_mine_activation_delay_ms; // 0x0301A220
static float SB_initial_falloff_wc; // 0x0301AFCC
// player* Player (0x03023874) is used here too; placed in player.cpp.
static float Proxy_mine_vehicle_radius; // 0x03024E5C
// float Mp_head_damage_multiplier (0x03024E60) is used here too; placed in human_combat.cpp.
static float Max_simultaneous_proxies; // 0x03024E8C
static int Proxy_mine_detonation_delay_ms; // 0x03024E94
// float Mp_leg_damage_multiplier (0x030266A4) is used here too; placed in human_combat.cpp.
static float Proxy_mine_vehicle_detonation_vel; // 0x030291D4
static float SB_initial_falloff; // 0x0302A164
static int Proxy_mine_vehicle_detonation_delay_ms; // 0x0302AD68
static int Thermo_mid_air_detonation_delay_ms; // 0x0302F020
// Also read by bb_pathfinding.cpp (rfg_pf_get_projectile_repulsor_radius).
float Proxy_mine_radius; // 0x03025A9C
// projectile* Charge_list (0x0302F220) is used here too; placed in object.cpp.
static object_pool<projectile,128,1> projectilepool; // 0x0328B740
// read from rfg.exe: pool usage 0x14EC0 bytes, 128 projectiles.
static obj_class_instantiator projectile_instantiator("projectile", alloc_projectile, types_projectile, bpo_index_projectile, classname_projectile, projectile::pool_remaining, 0x14EC0, 128); // 0x03537E60
bbox projectile::proj_safe_broadphase; // 0x03067ABC

static tweak_table_item Tweak_table_item_Proxy_mine_radius("Proxy_mine_radius", &Proxy_mine_radius);
static tweak_table_item Tweak_table_item_Proxy_mine_vehicle_radius("Proxy_mine_vehicle_radius", &Proxy_mine_vehicle_radius);
static tweak_table_item Tweak_table_item_Max_simultaneous_proxies("Max_simultaneous_proxies", &Max_simultaneous_proxies);
static tweak_table_item Tweak_table_item_Proxy_mine_detonation_delay_ms("Proxy_mine_detonation_delay_ms", &Proxy_mine_detonation_delay_ms);
static tweak_table_item Tweak_table_item_Proxy_mine_vehicle_detonation_vel("Proxy_mine_vehicle_detonation_vel", &Proxy_mine_vehicle_detonation_vel);
static tweak_table_item Tweak_table_item_Proxy_mine_vehicle_detonation_delay_ms("Proxy_mine_vehicle_detonation_delay_ms", &Proxy_mine_vehicle_detonation_delay_ms);
static tweak_table_item Tweak_table_item_Proxy_mine_activation_delay_ms("Proxy_mine_activation_delay_ms", &Proxy_mine_activation_delay_ms);
static tweak_table_item Tweak_table_item_Thermo_mid_air_detonation_delay_ms("Thermo_mid_air_detonation_delay_ms", &Thermo_mid_air_detonation_delay_ms);
static tweak_table_item Tweak_table_item_SB_full_falloff("SB_full_falloff", &SB_full_falloff);
static tweak_table_item Tweak_table_item_SB_full_falloff_wc("SB_full_falloff_wc", &SB_full_falloff_wc);
static tweak_table_item Tweak_table_item_SB_initial_falloff("SB_initial_falloff", &SB_initial_falloff);
static tweak_table_item Tweak_table_item_SB_initial_falloff_wc("SB_initial_falloff_wc", &SB_initial_falloff_wc);

// ---- internal functions (forward)
static bool projectile_filter_callback(object* hit_objp, void* ptr); // 0x00A1E4D0 (:301)
static void maybe_play_remote_charge_help_text(const human* const thrower, const weapon_info* w_info); // 0x00A40E40 (:329)

// Stops one of the projectile's effects (inlined everywhere).
static void projectile_stop_effect(unsigned int& handle)
{
    object_effect* effect = handle != 0xFFFFFFFF ? World.object_from_handle<object_effect>(handle) : nullptr;
    if (effect != nullptr)
    {
        effect->stop(object_effect::SS_STOP_EMITTING);
    }
    handle = 0xFFFFFFFF;
}

// ---- header inlines (projectile.h)
// 0x005DDDD0 (.h:75)
projectile_flags::projectile_flags()
{
    *(unsigned short*)this = 0;
}

// 0x0075C170
projectile_info::projectile_info()
{
}

// 0x005DDDE0 (.h:91)
const object_type projectile::get_type_static()
{
    return OT_ITEM;
}

// 0x005DDDF0 (.h:91)
const object_sub_type projectile::get_subtype_static()
{
    return OT_SUB_ITEM_PROJECTILE;
}

// 0x00A06A90 (.h:91)
const char* projectile::get_classname_static()
{
    return "projectile";
}

// 0x00A06AA0 (.h:91)
int projectile::get_bpo_index_static()
{
    return 1;
}

// 0x00A1E680 (.h:91)
const char* projectile::get_classname() const
{
    return "projectile";
}

// 0x00A1E690 (.h:167)
obj_serialize_mode projectile::serialize_mode()
{
    return OSM_NONE;
}

// 0x00756BE0 (.h:188)
void projectile::set_detonate_time(int ms, unsigned int detonator)
{
    detonate_stamp.set(ms);
    detonator_handle = detonator;
}

// 0x00756C10 (.h:189)
bool projectile::is_detonation_ts_valid() const
{
    return detonate_stamp.valid();
}

// 0x00A06AB0 (.h:190)
int projectile::detonate_time_since() const
{
    return detonate_stamp.time_since();
}

// 0x005DDE00 (.h:191)
vector projectile::get_start_pos() const
{
    return start_pos;
}

// 0x005DDE30 (.h:192)
vector projectile::get_start_velocity() const
{
    return start_velocity;
}

// 0x00756C20 (.h:194)
void projectile::set_heat_seeking_target(unsigned int target)
{
    heat_seeking_target = target;
}

// 0x0094CF70 (.h:198)
void projectile::set_fade_out_time(int ms)
{
    fade_stamp.set(ms);
    pflags.fading = 1;
}

// 0x0094CF90 (.h:199)
bool projectile::is_fading_out() const
{
    return pflags.fading;
}

// 0x00A06AC0 (.h:200)
bool projectile::is_fade_out_complete() const
{
    return pflags.fading && fade_stamp.elapsed();
}

// 0x0094CFA0 (.h:201)
bool projectile::is_ballistic() const
{
    return pflags.ballistic;
}

// 0x0094CFB0 (.h:202)
void projectile::set_ballistic(bool ballistic)
{
    pflags.ballistic = ballistic;
}

// 0x005DDE60 (.h:204)
bool projectile::is_thrown_weapon() const
{
    const weapon_class_type wc = w_info->weapon_class;
    return wc == WEAPON_CLASS_THROWN_MINE || wc == WEAPON_CLASS_THROWN_CHARGE || wc == WEAPON_CLASS_THROWN_GRENADE || wc == WEAPON_CLASS_SINGULARITY_BOMB;
}

// 0x005DDE90 (.h:206)
bool projectile::is_rocket_weapon() const
{
    return w_info->weapon_class == WEAPON_CLASS_LAUNCHER;
}

// 0x005DDEA0 (.h:207)
bool projectile::is_propellant_started() const
{
    return pflags.propellant_started;
}

// 0x005DDEB0 (.h:208)
bool projectile::is_propellant_expired() const
{
    return pflags.propellant_expired;
}

// 0x0080D330 (.h:210)
bool projectile::is_parented() const
{
    return get_parent() != 0xFFFFFFFF;
}

// 0x005DDEC0 (.h:215)
bool projectile::was_investigated() const
{
    return pflags.investigated;
}

// 0x00756C30 (.h:218)
void projectile::set_as_investigated()
{
    pflags.investigated = 1;
}

// 0x005DDED0 (.h:221)
bool projectile::was_spotted() const
{
    return pflags.spotted;
}

// 0x005DDEE0 (.h:224)
void projectile::set_as_spotted()
{
    pflags.spotted = 1;
}

// 0x00A06AF0 (.h:227)
void projectile::invalidate_pending_detonation()
{
    detonate_stamp.invalidate();
}

// 0x005DDEF0 (.h:232)
int projectile::get_age() const
{
    return age_timer.time_since();
}

// 0x0094CFD0 (.h:233)
void projectile::enable_havok_object()
{
    pflags.release_havok_object = 0;
}

// 0x006058C0 (.h:320): the launch properties. Original: the flags argument is not stored (PROJ_PROP_FLAGS is 0).
obj_projectile_props::obj_projectile_props(unsigned int owner_handle, item_info* info, const obj_item_flags& flags, const weapon_info* w_info, const vector& pos, const matrix& orient, const vector& velocity,
                                           const vector& target, const unsigned int create_id)
    : obj_item_props(info, pos, orient, flags, COLLIDE_PROJECTILE, -1.0f, INVALID_SALVAGE_MATERIAL)
{
    set<unsigned int>(PROJ_PROP_OWNER_HANDLE, owner_handle);
    set<const weapon_info*>(PROJ_PROP_WEAPON_INFO, w_info);
    set<unsigned short>(PROJ_PROP_FLAGS, 0);
    set<vector>(PROJ_PROP_VEL, velocity);
    set<vector>(PROJ_PROP_TARGET, target);
    set<unsigned int>(PROJ_PROP_CLIENT_NONCE, create_id);
}

// 0x005ECEB0: the base destructors release the property buffer.
obj_projectile_props::~obj_projectile_props()
{
}

// ---- console commands (the dcf table is not ported)
// 0x00A1E390 (:99): the projectile limits: the Havok broadphase shrunk by 25 m on every side.
void projectile::recompute_safe_bp()
{
    proj_safe_broadphase.max.y = Havok_safe_broadphase.max.y - 25.0f;
    proj_safe_broadphase.max.z = Havok_safe_broadphase.max.z - 25.0f;
    proj_safe_broadphase.min.x = Havok_safe_broadphase.min.x + 25.0f;
    proj_safe_broadphase.min.y = Havok_safe_broadphase.min.y + 25.0f;
    proj_safe_broadphase.max.x = Havok_safe_broadphase.max.x - 25.0f;
    proj_safe_broadphase.min.z = Havok_safe_broadphase.min.z + 25.0f;
}

// 0x00ACDDA0 (:225)
void unused_dcf_detonate_charges()
{
    if (Console_command != 0)
    {
        detonate_charges(Player->get_handle());
    }
}

// 0x00A95370 (:230)
void unused_dcf_detonate_mines()
{
    if (Console_command != 0)
    {
        detonate_mines(Player->get_handle());
    }
}

// ---- internal functions
// 0x00A1E4D0 (:301): a ray that only hits the given object.
static bool projectile_filter_callback(object* hit_objp, void* ptr)
{
    return hit_objp == ptr;
}

// 0x00A1E4E0 (:312) (LTCG: w_info in ECX)
static bool projectile_causes_knockdown_on_impact(const weapon_info* w_info)
{
    return w_info->weapon_class == WEAPON_CLASS_NANO_RIFLE;
}

// 0x00A1E4F0 (:317) (LTCG: proj in EAX)
static bool projectile_is_proxy_mine(projectile* proj)
{
    if (proj == nullptr)
    {
        return false;
    }
    return proj->w_info->weapon_class == WEAPON_CLASS_THROWN_MINE;
}

// 0x00A1E510 (:323) (LTCG: proj in EAX)
static bool projectile_is_remote_charge(projectile* proj)
{
    if (proj == nullptr)
    {
        return false;
    }
    return proj->w_info->weapon_class == WEAPON_CLASS_THROWN_CHARGE;
}

// 0x00A40E40 (:329): the single player tool tips after placing a charge (how to detonate; low on charges).
static void maybe_play_remote_charge_help_text(const human* const thrower, const weapon_info* w_info)
{
    if (Multiplayer)
    {
        return;
    }
    if (thrower != nullptr && (player*)thrower == Player && w_info->weapon_class == WEAPON_CLASS_THROWN_CHARGE)
    {
        ui_tool_tips_add_tip("Remote_Charge_Detonate", 0, nullptr);
        if (Multiplayer)
        {
            return;
        }
    }
    if (mission_get_mode() == MISSION_MODE_TUTORIAL || thrower == nullptr || (player*)thrower != Player)
    {
        return;
    }
    const inventory_item* iip = inv_item_get_item_from_weapon_info(thrower, weapons_get_weapon_info_handle(w_info));
    const weapon* wp = iip != nullptr && iip->weapon_handle != 0xFFFFFFFF ? World.object_from_handle<weapon>(iip->weapon_handle) : nullptr;
    // Magic number: fewer than 5 charges left in reserve.
    if (wp != nullptr && wp->get_rounds_in_reserve() < 5)
    {
        ui_tool_tips_add_tip("RC_Ammo", 0, nullptr);
    }
}

// ---- charges and mines
// 0x00A719A0 (:368)
int count_charges(unsigned int owner_handle)
{
    int num_attached;
    return count_charges(owner_handle, num_attached);
}

// 0x00A40EF0 (:374): the owner's remote charges and how many of them stick to something.
int count_charges(unsigned int owner_handle, int& num_attached)
{
    int num_charges = 0;
    num_attached = 0;
    projectile* first = Charge_list;
    for (projectile* proj = first; proj != nullptr;)
    {
        if (proj->get_handle() != 0xFFFFFFFF && owner_handle == proj->owner_handle)
        {
            num_charges++;
            if (proj->is_attached())
            {
                num_attached++;
            }
        }
        proj = proj->p_next;
        if (proj == first)
        {
            break;
        }
    }
    return num_charges;
}

// 0x00A95390 (:393)
void projectile_count_charges_proxy_mines(unsigned int owner_handle, int& num_charges, int& num_proxy_mines)
{
    int num_attached_charges;
    projectile_count_charges_proxy_mines(owner_handle, num_charges, num_attached_charges, num_proxy_mines);
}

// 0x00A719C0 (:399): the owner's charges and proxy mines among all projectiles. Original: attached projectiles of
// any kind count as attached charges.
void projectile_count_charges_proxy_mines(unsigned int owner_handle, int& num_charges, int& num_attached_charges, int& num_proxy_mines)
{
    num_charges = 0;
    num_attached_charges = 0;
    num_proxy_mines = 0;
    const int num = World.get_num_objects<projectile>();
    for (int i = 0; i < num; i++)
    {
        projectile* proj = World.get_object<projectile>(i);
        if (proj->get_handle() == 0xFFFFFFFF || owner_handle != proj->owner_handle)
        {
            continue;
        }
        if (proj->w_info->weapon_class == WEAPON_CLASS_THROWN_CHARGE)
        {
            num_charges++;
        }
        else if (proj->w_info->weapon_class == WEAPON_CLASS_THROWN_MINE)
        {
            num_proxy_mines++;
        }
        if (proj->is_attached())
        {
            num_attached_charges++;
        }
    }
}

// 0x00AE5200 (:421): the owner's mid-air detonations (thermobaric rockets) once their delay passed. True when the
// owner has one in flight.
bool detonate_in_air(unsigned int owner_handle)
{
    bool found = false;
    const int num = World.get_num_objects<projectile>();
    for (int i = 0; i < num; i++)
    {
        projectile* proj = World.get_object<projectile>(i);
        if ((proj->w_info->projectile_info.flags & PIF_ALLOW_MID_AIR_DETONATION) != 0 && proj->get_handle() != 0xFFFFFFFF && owner_handle == proj->owner_handle)
        {
            if (proj->get_age() >= Thermo_mid_air_detonation_delay_ms)
            {
                proj->detonate(nullptr, nullptr, nullptr);
            }
            found = true;
        }
    }
    return found;
}

// 0x00A1E530 (:440)
bool projectile_invalidate_pending_charges()
{
    bool invalidated = false;
    projectile* proj = Charge_list;
    if (proj == nullptr)
    {
        return false;
    }
    do
    {
        if (proj->get_handle() != 0xFFFFFFFF && proj->detonate_stamp.valid())
        {
            proj->detonate_stamp.invalidate();
            invalidated = true;
        }
        proj = proj->p_next;
    } while (proj != Charge_list && proj != nullptr);
    return invalidated;
}

// 0x00AC55D0 (:461): the owner's attached remote charges go off one after the other (RFG_charge_delay apart; ones
// already counting down for less than 500 ms are left alone, unstreamed ones are just removed). A player with no
// charges left in hand switches weapon, or reequips the charge when no more can be thrown.
bool detonate_charges(unsigned int owner_handle)
{
    int delay_ms = 0;
    bool detonated = false;
    player* pp = owner_handle != 0xFFFFFFFF ? World.object_from_handle<player>(owner_handle) : nullptr;
    if (pp != nullptr)
    {
        // Original: looks for the charge weapon in the inventory and never uses what it found.
        for (const inventory_item* iip = pp->inventory; iip != nullptr;)
        {
            const weapon* wp = iip->weapon_handle != 0xFFFFFFFF ? World.object_from_handle<weapon>(iip->weapon_handle) : nullptr;
            if (wp != nullptr && wp->w_info->weapon_class == WEAPON_CLASS_THROWN_CHARGE)
            {
                break;
            }
            iip = iip->next;
            if (iip == pp->inventory)
            {
                break;
            }
        }
    }
    projectile* first = Charge_list;
    for (projectile* proj = first; proj != nullptr;)
    {
        if (proj->get_handle() != 0xFFFFFFFF && proj->w_info->weapon_class == WEAPON_CLASS_THROWN_CHARGE && owner_handle == proj->owner_handle && proj->is_attached())
        {
            if (!proj->obj_flags.streamed)
            {
                World.destroy_object(proj, false);
                detonated = true;
            }
            // Magic number: a charge already counting down for less than 500 ms is not restarted.
            else if (!proj->detonate_stamp.valid() || proj->detonate_stamp.time_since() >= 501)
            {
                proj->detonate_stamp.set(delay_ms);
                delay_ms += RFG_charge_delay;
                proj->detonator_handle = owner_handle;
                detonated = true;
            }
        }
        proj = proj->p_next;
        if (proj == Charge_list)
        {
            break;
        }
    }
    if (pp == nullptr)
    {
        return detonated;
    }
    // The weapon in hand: the reserved turret's weapon when the player operates it, else the equipped one.
    const weapon* wp = nullptr;
    const turret* tp = pp->combat.reserved_turret != 0xFFFFFFFF ? World.object_from_handle<turret>(pp->combat.reserved_turret) : nullptr;
    unsigned int weapon_handle = 0xFFFFFFFF;
    if (tp != nullptr && World.object_from_handle<human>(tp->current_operator) == pp)
    {
        weapon_handle = tp->primary_weapon_handle;
    }
    else if (pp->equipped_inv_item != nullptr)
    {
        weapon_handle = pp->equipped_inv_item->weapon_handle;
    }
    if (weapon_handle != 0xFFFFFFFF)
    {
        wp = World.object_from_handle<weapon>(weapon_handle);
    }
    const weapon_info* w_info = nullptr;
    if (wp != nullptr)
    {
        w_info = wp->w_info;
        if (w_info->weapon_class == WEAPON_CLASS_THROWN_CHARGE)
        {
            if (wp->get_rounds_total() == 0)
            {
                player_weapon_autoswap(pp);
            }
            else if (!can_fire_charge(owner_handle, false) && detonated)
            {
                inventory_item* iip = pp->equipped_inv_item;
                human_weapons_immediately_holster_weapon(pp);
                human_equip_inventory_item(pp, iip);
            }
        }
    }
    if (detonated == true)
    {
        gameplay_register_player_attack(pp, w_info, DBT_EXPLOSION, true);
        player_fires_weapon_or_detonates_event event;
        gem_post_event(event);
        if (pp == Player)
        {
            // Magic number: 250 ms.
            Local_player_recent_charge_detonation.set(250);
        }
    }
    return detonated;
}

// 0x00A71A70 (:555): the owner's proxy mines go off now.
void detonate_mines(unsigned int owner_handle)
{
    const int num = World.get_num_objects<projectile>();
    for (int i = 0; i < num; i++)
    {
        projectile* proj = World.get_object<projectile>(i);
        if (proj->owner_handle == owner_handle && proj->w_info->weapon_class == WEAPON_CLASS_THROWN_MINE)
        {
            proj->detonate_stamp.set(0);
            proj->detonator_handle = owner_handle;
        }
    }
}

// 0x00A953B0 (:571): whether the player's charge weapon (or proxy mines in multiplayer, or the singularity bomb) can
// be thrown: loaded, a free projectile, and fewer charges out than the upgrade's limit (scaled by the backpack's ammo
// factor; mines count too in multiplayer). At the limit, the local player may still throw within 250 ms of a
// detonation when asked.
bool can_fire_charge(unsigned int owner_handle, bool recent_detonation_override)
{
    if (projectile::pool_remaining() < 1)
    {
        return false;
    }
    human* hp = owner_handle != 0xFFFFFFFF ? World.object_from_handle<player>(owner_handle) : nullptr;
    // Original: hp is used unchecked from here.
    weapon* wp = human_weapons_get_weapon(hp);
    if (wp == nullptr)
    {
        return false;
    }
    const weapon_class_type wc = wp->w_info->weapon_class;
    if (wc != WEAPON_CLASS_THROWN_CHARGE && wc != WEAPON_CLASS_SINGULARITY_BOMB && (!Multiplayer || wc != WEAPON_CLASS_THROWN_MINE))
    {
        return false;
    }
    if (!wp->is_loaded() && wp->w_info->weapon_class != WEAPON_CLASS_SINGULARITY_BOMB)
    {
        return false;
    }
    int max_charges = upgrade_get_max_charges();
    if (hp != nullptr && ((player*)hp)->backpack != nullptr)
    {
        max_charges = (int)(((player*)hp)->backpack->get_backpack_abilities()->ammo_factor * (float)max_charges);
    }
    int num_charges;
    int num_attached;
    int num_mines;
    projectile_count_charges_proxy_mines(owner_handle, num_charges, num_attached, num_mines);
    if (Multiplayer)
    {
        num_charges += num_mines;
    }
    if (num_charges >= max_charges && wp->w_info->weapon_class != WEAPON_CLASS_SINGULARITY_BOMB)
    {
        // The charge held for the throw goes back.
        item* offhand = World.object_from_handle<item>(hp->offhand_projectile_handle);
        if (offhand != nullptr)
        {
            offhand->hold_item(hp, false);
        }
        if (!recent_detonation_override || !object_is_local_player(hp) || !Local_player_recent_charge_detonation.valid() || Local_player_recent_charge_detonation.elapsed())
        {
            return false;
        }
    }
    return true;
}

// ---- class registration (:797)
// 0x00A1E5B0 (:797)
const char* classname_projectile()
{
    return "projectile";
}

// 0x00A1E5A0 (:797)
int bpo_index_projectile()
{
    return 1;
}

// 0x00A40F50 (:797)
int projectile::pool_remaining()
{
    return projectilepool.get_free();
}

// 0x00A1E590 (:797)
void types_projectile(signed char& type, signed char& subtype)
{
    type = OT_ITEM;
    subtype = OT_SUB_ITEM_PROJECTILE;
}

// 0x00A95520 (:797)
void projectile::pool_free()
{
    projectilepool.free(this);
}

// 0x00A71B00 (:797)
object* alloc_projectile()
{
    return projectilepool.alloc(0);
}

// 0x00A1E5C0 (:797)
unsigned int projectile::get_classname_hash()
{
    return projectile_instantiator.this_hash;
}

// 0x00A1E5D0 (:798): the item base sets its flags (cast shadows, simulate), the timestamps start invalid.
projectile::projectile()
{
}

// 0x00A1E6A0
projectile::~projectile()
{
}

// 0x00AAE3C0 (:810): the launch: owner, weapon, velocity and target from the properties; multi rockets and the heat
// seeking target for the player's rockets (single player); the mine's arming delay, the inaccuracy, the charge list
// and the flight sound.
bool projectile::serialize(obj_prop_block& pblock, bool load)
{
    if (!item::serialize(pblock, load))
    {
        return false;
    }
    if (!load)
    {
        return true;
    }
    *(unsigned short*)&pflags = 0;
    p_prev = nullptr;
    p_next = nullptr;
    sound_info = -1;
    pblock.get<unsigned int>(PROJ_PROP_OWNER_HANDLE, owner_handle);
    pblock.get<const weapon_info*>(PROJ_PROP_WEAPON_INFO, w_info);
    if (w_info == nullptr)
    {
        return false;
    }
    pblock.get<projectile_flags>(PROJ_PROP_FLAGS, pflags);
    pblock.get<vector>(PROJ_PROP_VEL, start_velocity);
    pblock.get<vector>(PROJ_PROP_TARGET, target_pos);
    pblock.get_optional<unsigned int>(PROJ_PROP_CLIENT_NONCE, mp_create_id);
    pflags.ballistic = 1;
    mp_no_sync_timer.invalidate();
    heat_seeking_target = 0xFFFFFFFF;
    pflags.is_rocket = (w_info->projectile_info.flags & PIF_ROCKET_FLIGHT) != 0;
    secondary_rockets_to_fire = upgrade_get_multi_rockets() - 1;
    if (!pflags.is_rocket || Multiplayer)
    {
        secondary_rockets_to_fire = 0;
    }
    else
    {
        human* owner = World.object_from_handle<human>(owner_handle);
        if (owner != nullptr)
        {
            weapon* wp = human_weapons_get_weapon(owner);
            if (wp != nullptr)
            {
                secondary_rockets_to_fire = wp->secondary_rockets_to_fire;
            }
        }
        if (upgrade_has_heat_seeking_rockets() && object_is_player(owner_handle) && Player != nullptr && Player->target_is_locked())
        {
            heat_seeking_target = Player->heat_seeking_target;
        }
    }
    start_pos = pos;
    is_primary_rocket = true;
    age_timer.set(0);
    detonate_stamp.invalidate();
    active_stamp.invalidate();
    if (w_info->weapon_class == WEAPON_CLASS_THROWN_MINE)
    {
        active_stamp.set(Proxy_mine_activation_delay_ms);
    }
    exploded = false;
    fade_stamp.invalidate();
    ca_time_offset_sec = 0.0f;
    if (w_info->projectile_info.inaccurate_flight > 0.0f)
    {
        inaccuracy_vector.rand_sphere();
        inaccuracy_vector.z = 0.0f;
        const float inaccuracy = w_info->projectile_info.inaccurate_flight;
        inaccuracy_vector.x = inaccuracy * inaccuracy_vector.x;
        inaccuracy_vector.y = inaccuracy_vector.y * inaccuracy;
        inaccuracy_vector.z = inaccuracy_vector.z * inaccuracy;
    }
    if (w_info->projectile_info.obj_item_info != nullptr)
    {
        if (w_info->weapon_class == WEAPON_CLASS_THROWN_CHARGE)
        {
            if (Charge_list == nullptr)
            {
                p_next = this;
                p_prev = this;
                Charge_list = this;
            }
            else
            {
                p_prev = Charge_list->p_prev;
                p_next = Charge_list;
                Charge_list->p_prev->p_next = this;
                Charge_list->p_prev = this;
            }
        }
        sound_info = audiolib_cue_play_3d(w_info->projectile_info.sound, &pos, &orient.fvec, nullptr, nullptr);
    }
    client_prediction_net_time = 0;
    constraint_handle = 0xFFFFFFFF;
    mp_sim_state = nullptr;
    return true;
}

// 0x00AAE730 (:930): the launch once streamed: the flight effects (a grenade blinks: friendly or enemy light in
// multiplayer), the velocities (owner ignored by the body), gravity, and for "use cloud spline" weapons a random
// spline to the target. A projectile the client creates in multiplayer becomes a remote object on the server; the
// local owner's own creation is sent (multi_game_client_create).
bool projectile::stream(bool load)
{
    if (!item::stream(load))
    {
        return false;
    }
    if (!load)
    {
        return true;
    }
    if (havok_handle == 0xFFFFFFFF)
    {
        return false;
    }
    attach_point_index = get_tag_index_from_name("attach_point");
    blinking_light_point_index = get_tag_index_from_name("blinking_light");
    detach_effects(true, true, true, true);
    if (!is_attached())
    {
        unsigned int effect_handle;
        if (!pflags.is_rocket)
        {
            if (w_info->weapon_class == WEAPON_CLASS_THROWN_GRENADE && blinking_light_point_index != -1)
            {
                vector light_pos;
                get_tag_world_coords(blinking_light_point_index, light_pos, nullptr, nullptr, nullptr);
                const player* local_player = Player;
                const human* owner = World.object_from_handle<human>(owner_handle);
                const unsigned int friendly_effect = w_info->projectile_info.effect3;
                if (!Multiplayer || !human_teams_are_friendly(local_player, owner) || friendly_effect == 0xFFFFFFFF)
                {
                    effect_obj_handle2 = effects_play(w_info->projectile_info.effect2, light_pos, orient, true, get_handle(), -1, 0xFFFFFFFF, 1.0f, false, EFFECTS_CATEGORY_NONE);
                }
                else
                {
                    effect_obj_handle3 = effects_play(friendly_effect, light_pos, orient, true, get_handle(), -1, 0xFFFFFFFF, 1.0f, false, EFFECTS_CATEGORY_NONE);
                }
            }
            else
            {
                pflags.propellant_started = w_info->projectile_info.acceleration > 0.0f;
            }
            effect_handle = w_info->projectile_info.effect1;
        }
        else
        {
            effect_handle = 0xFFFFFFFF;
        }
        if (!pflags.is_rocket)
        {
            effect_obj_handle = effects_play(effect_handle, start_pos, orient, true, get_handle(), -1, 0xFFFFFFFF, 1.0f, false, EFFECTS_CATEGORY_NONE);
        }
        else
        {
            effect_obj_handle3 = effects_play(w_info->projectile_info.effect3, start_pos, orient, true, get_handle(), -1, 0xFFFFFFFF, 1.0f, false, EFFECTS_CATEGORY_NONE);
        }
        hkpRigidBody* rb = havok_body_get_pointer(havok_handle);
        if (rb != nullptr)
        {
            havok_set_ignore_property(rb, owner_handle);
            if (Multiplayer)
            {
                havok_body_set_linear_dampening(rb, 0.0f);
            }
            // The mass driver and the harpoon remember their launch velocity.
            if ((unsigned int)(w_info->weapon_class - WEAPON_CLASS_MASS_DRIVER) < 2)
            {
                previous_velocity = start_velocity;
            }
        }
        havok_body_set_linear_velocity(havok_handle, start_velocity);
        if (!Multiplayer || (w_info->projectile_info.flags & PIF_ALIGN_ORIENT_TO_FLIGHTPATH) == 0)
        {
            havok_body_set_angular_velocity(havok_handle, info->angular_velocity.copy_unrotate(orient));
        }
    }
    else
    {
        handle_collision_attach_effects();
    }
    // Gravity is a custom force on the body (current_accel starts at the world's -9.8).
    current_accel.x = 0.0f;
    current_accel.y = -9.8f;
    current_accel.z = 0.0f;
    vector gravity(0.0f, w_info->projectile_info.gravity, 0.0f);
    set_acceleration(gravity);
    start_accel = gravity;
    ballistic_accel = gravity;
    spline_t = 1.0f;
    spline_length = 0.0f;
    ballistic_ignore_handle = owner_handle;
    // Magic number: a target at least 0.1 m from the origin.
    if (target_pos.x * target_pos.x + target_pos.y * target_pos.y + target_pos.z * target_pos.z > 0.01f && (w_info->projectile_info.flags & PIF_USE_CLOUD_SPLINE) != 0)
    {
        // Magic numbers: the control points along the way to the target (and before the start), lifted along uvec by a
        // fifth of the distance.
        const vector to_target = target_pos - pos;
        const float lift = to_target.mag() * 0.2f;
        const vector up = orient.uvec * lift;
        spline_pts[0] = pos - to_target * 0.175f;
        spline_pts[1] = pos;
        spline_pts[2] = to_target * 0.333f + pos + up * 0.33f;
        spline_pts[3] = to_target * 0.666f + pos + up;
        spline_pts[4] = to_target * 0.888f + pos + up * 0.5f;
        spline_pts[5] = target_pos;
        spline_pts[6] = target_pos;
        if (Projectile_random_spline)
        {
            vector offset;
            offset.rand_sphere();
            spline_pts[2] = offset * lift * fl_rand_range(0.0f, 0.33f) + spline_pts[2];
            offset.rand_sphere();
            spline_pts[3] = spline_pts[3] + offset * lift * fl_rand_range(0.0f, 0.66f);
            offset.rand_sphere();
            spline_pts[4] = offset * lift * fl_rand_range(0.0f, 0.25f) + spline_pts[4];
            offset.rand_sphere();
            const float spread = fl_rand_range(0.0f, 3.0f);
            spline_pts[5] = offset * spread + spline_pts[5];
            spline_pts[6] = spline_pts[5];
        }
        spline.set_points(7, spline_pts);
        spline.set_tension(0.0f);
        pflags.spline_based_cloud = 1;
        // The length, sampled in quarters of the segments from the start point.
        vector prev_point = spline_pts[1];
        vector point;
        for (float t = 1.25f; t < 5.0f; t += 0.25f)
        {
            spline.get_point(point, t);
            spline_length = (point - prev_point).mag() + spline_length;
            prev_point = point;
        }
        spline_length = (spline_pts[5] - point).mag() + spline_length;
    }
    if (Multiplayer && multi_game_client_creates_weapon(w_info))
    {
        remote_obj_flags.client_sync = 1;
        if (Multiplayer == true && multi_is_server())
        {
            remote_obj_flags.remote_object = 1;
            multi_core_object_create_remote_object(1, get_handle());
            obj_flags.no_save = 1;
            obj_flags.destroy_on_stream = 1;
            return true;
        }
        if (mp_create_id != 0xFFFFFFFF && Player != nullptr && Player->get_handle() == owner_handle)
        {
            ca_create_pos = pos;
            multi_game_client_create(this);
        }
    }
    obj_flags.no_save = 1;
    obj_flags.destroy_on_stream = 1;
    return true;
}

// 0x00A95530 (:1172): moves the projectile (and its broadphase box, its body when asked) and queues the render
// update (render flag 4).
void projectile::update_pos_and_orient(const vector& update_pos, const matrix& update_orient, bool set_havok_data)
{
    pos = update_pos;
    orient = update_orient;
    if (bp_handle != nullptr)
    {
        get_bbox(last_known_bmin, last_known_bmax);
        havok_bpo_update(bp_handle, last_known_bmin, last_known_bmax);
    }
    if (set_havok_data && havok_handle != 0xFFFFFFFF)
    {
        havok_body_set_pos_orient(havok_handle, update_pos, update_orient);
    }
    // Inlined render flag 4 (set_render_flags).
    set_render_flags(4);
}

// 0x00A95540 (:1210)
void projectile::free_this()
{
    if (Multiplayer)
    {
        multi_core_object_process_object_free(this);
    }
    // A wrecking crew projectile that never went off still counts as an attack.
    if (wrecking_crew_is_active() && !exploded)
    {
        player* pp = owner_handle != 0xFFFFFFFF ? World.object_from_handle<player>(owner_handle) : nullptr;
        gameplay_register_player_attack(pp, w_info, DBT_EXPLOSION, false);
    }
    audiolib_play_stop(sound_info);
    projectile_stop_effect(effect_obj_handle);
    projectile_stop_effect(effect_obj_handle2);
    projectile_stop_effect(effect_obj_handle3);
    projectile_stop_effect(effect_obj_handle4);
    if (w_info != nullptr && w_info->weapon_class == WEAPON_CLASS_THROWN_CHARGE && p_next != nullptr)
    {
        if (Charge_list == this && (Charge_list = p_next, p_next == this))
        {
            Charge_list = nullptr;
        }
        else
        {
            p_prev->p_next = p_next;
            p_next->p_prev = p_prev;
        }
        p_prev = nullptr;
        p_next = nullptr;
    }
    item::free_this();
}

// 0x00A1E6B0 (:1247): the opacity follows the fade (1 s); the server removes a faded projectile.
void projectile::process_fade_out()
{
    float alpha = 1.0f;
    if (pflags.fading)
    {
        // Magic number: the fade lasts the last second.
        alpha = (float)fade_stamp.time_until() / 1000.0f;
        opacity = alpha;
        if (alpha - 0.0f < 0.0f)
        {
            alpha = 0.0f;
        }
        if (!(alpha - 1.0f < 0.0f))
        {
            alpha = 1.0f;
        }
    }
    opacity = alpha;
    if (pflags.fading && fade_stamp.elapsed() && Multiplayer == true && multi_is_server())
    {
        World.destroy_object(this, false);
    }
}

// 0x00AE52A0 (:1267)
void projectile::process()
{
    if (detonate_stamp.elapsed())
    {
        // A client never sets off a proxy mine itself.
        if (Multiplayer != true || multi_is_server() || w_info->weapon_class != WEAPON_CLASS_THROWN_MINE)
        {
            detonate(nullptr, nullptr, World.object_from_handle(detonator_handle));
        }
        else
        {
            detonate_stamp.invalidate();
        }
    }
    if (!rdd.is_loaded())
    {
        return;
    }
    if (pflags.release_havok_object && havok_handle != 0xFFFFFFFF)
    {
        havok_body_free(havok_handle, false, true, false);
        havok_handle = 0xFFFFFFFF;
    }
    else if (!pflags.release_havok_object && havok_handle == 0xFFFFFFFF)
    {
        create_physics_object(this);
    }
    if (pflags.restart_ballistic)
    {
        restart_ballistic_props();
    }
    // Out of the safe broadphase: removed (single player or the server).
    if ((!Multiplayer || multi_is_server()) &&
        (pos.x <= proj_safe_broadphase.min.x || pos.y <= proj_safe_broadphase.min.y || pos.z <= proj_safe_broadphase.min.z || proj_safe_broadphase.max.x <= pos.x ||
         proj_safe_broadphase.max.y <= pos.y || proj_safe_broadphase.max.z <= pos.z))
    {
        World.destroy_object(this, false);
        return;
    }
    audiolib_play_set_3d_position(sound_info, &pos, nullptr, nullptr);
    process_fade_out();
    if (pflags.is_rocket)
    {
        process_rocket();
    }
    if (w_info->weapon_class == WEAPON_CLASS_GRINDER || w_info->weapon_class == WEAPON_CLASS_HARPOON)
    {
        process_grinder();
    }
    if (w_info->weapon_class == WEAPON_CLASS_THROWN_MINE)
    {
        process_mine();
    }
    if (w_info->weapon_class == WEAPON_CLASS_THROWN_CHARGE)
    {
        process_charge();
    }
    if (w_info->weapon_class == WEAPON_CLASS_SINGULARITY_BOMB)
    {
        process_singularity_bomb();
    }
    // A held projectile (not a charge) goes back to its idle state once its animation blended in.
    if (char_inst != nullptr && (is_owned() || item_flags.item_held))
    {
        const v_anim* desired = char_inst->vp->desired_state;
        if (desired != nullptr && desired->blend == 1.0f && w_info->weapon_class != WEAPON_CLASS_THROWN_CHARGE)
        {
            // Magic numbers: state 0x66 (idle), a 0.3 s blend.
            char_inst->set_state(index_alias(0x66, (vanim_aliases)-1), index_alias(-1, (vanim_aliases)-1), 0.3f, false);
        }
    }
    if (havok_handle == 0xFFFFFFFF || obj_flags.destroyed)
    {
        return;
    }
    if (w_info->weapon_class == WEAPON_CLASS_REPAIR)
    {
        process_repair();
    }
    if (w_info->projectile_info.fuse_time != 0 && (int)(unsigned int)w_info->projectile_info.fuse_time <= age_timer.time_since())
    {
        detonate(nullptr, nullptr, nullptr);
        return;
    }
    vector dir;
    havok_body_get_linear_velocity(havok_handle, dir);
    float speed = dir.mag_and_normalize_safe();
    if (!pflags.propellant_started || pflags.propellant_expired)
    {
        if ((w_info->projectile_info.flags & PIF_AIR_BOMB) != 0 && air_bomb_is_running())
        {
            air_bomb_seek_target(speed, dir);
        }
    }
    else
    {
        // The propellant's push falls off with the square of the speed toward max_speed.
        const float max_speed = w_info->projectile_info.max_speed;
        const float acceleration = w_info->projectile_info.acceleration;
        float push = (1.0f - (speed * speed) / (max_speed * max_speed)) * acceleration;
        if (push - 0.0f < 0.0f)
        {
            push = 0.0f;
        }
        if (0.0f <= push - acceleration)
        {
            push = acceleration;
        }
        rocket_seek_target(speed, dir);
        const vector delta_v(orient.fvec.x * push * Frametime, orient.fvec.y * push * Frametime, orient.fvec.z * push * Frametime);
        havok_body_apply_linear_velocity(havok_handle, delta_v);
    }
    // A grenade's fuse sound plays before the end of its fuse.
    if (w_info->weapon_class == WEAPON_CLASS_THROWN_GRENADE && !pflags.propellant_started && w_info->special_sound != -1 &&
        (float)w_info->projectile_info.fuse_time - w_info->projectile_info.time_until_prop_expire <= (float)age_timer.time_since())
    {
        audiolib_cue_play_3d(w_info->special_sound, &pos, nullptr, nullptr, nullptr);
        pflags.propellant_started = 1;
    }
    if (w_info->projectile_info.max_speed > 0.0f)
    {
        havok_body_get_linear_velocity(havok_handle, dir);
        speed = dir.mag_and_normalize_safe();
        const float max_speed = w_info->projectile_info.max_speed;
        if ((!Multiplayer || !pflags.ballistic) && max_speed < speed)
        {
            havok_body_set_linear_velocity(havok_handle, dir * max_speed);
            speed = max_speed;
        }
    }
    if (is_attached())
    {
        return;
    }
    if (w_info->projectile_info.inaccurate_flight > 0.0f)
    {
        matrix flight;
        flight.make_uvec(dir, orient.uvec);
        const vector drift = inaccuracy_vector.copy_unrotate(flight) * speed;
        havok_body_apply_linear_impulse(havok_handle, drift * Frametime);
    }
    if (pflags.spline_based_cloud)
    {
        // Magic numbers: the spline is followed 10% ahead (in 4 segments), a quarter segment ahead of the projectile.
        spline_t = ((Frametime * speed) / spline_length) * 1.1f * 4.0f + spline_t;
        float t = spline_t + 0.25f;
        if (t - 1.0f < 0.0f)
        {
            t = 1.0f;
        }
        if (0.0f <= t - 5.0f)
        {
            t = 5.0f;
        }
        vector ahead;
        spline.get_point(ahead, t);
        vector to_ahead = ahead - pos;
        to_ahead.normalize_safe();
        if (0.0f <= to_ahead.x * dir.x + to_ahead.y * dir.y + to_ahead.z * dir.z)
        {
            havok_body_set_linear_velocity(havok_handle, to_ahead * speed);
        }
        else
        {
            // Past the end of the spline.
            detonate(nullptr, nullptr, nullptr);
        }
    }
    // Magic number: turned along the flight above 0.1 m/s.
    if ((w_info->projectile_info.flags & PIF_ALIGN_ORIENT_TO_FLIGHTPATH) != 0 && speed > 0.1f)
    {
        orient.fvec.x = orient.fvec.x + dir.x;
        orient.fvec.y = orient.fvec.y + dir.y;
        orient.fvec.z = orient.fvec.z + dir.z;
        orient.fix();
        havok_body_set_orient(havok_handle, orient);
    }
}

// 0x00AE0600 (:1456): a heat seeking rocket turns half way toward its target (a human's head; it goes off within
// 0.25 m of it), slowed by the angle it turns.
void projectile::rocket_seek_target(float current_speed, vector current_direction)
{
    object* target = World.object_from_handle(heat_seeking_target);
    if (target == nullptr || (target->get_obj_type() != OT_HUMAN && target->get_obj_type() != OT_VEHICLE))
    {
        return;
    }
    vector offset = ZERO_VECTOR;
    if (target->get_obj_type() == OT_HUMAN)
    {
        const human* hp = target->get_handle() != 0xFFFFFFFF ? World.object_from_handle<human>(target->get_handle()) : nullptr;
        // Original: hp is passed unchecked.
        human_get_head_pos(hp, offset);
        // Magic number: 0.25 m.
        if (vec_dist(pos, offset) < 0.25f)
        {
            detonate(nullptr, nullptr, nullptr);
            return;
        }
        offset = offset - target->pos;
    }
    const vector to_target = (target->pos + offset) - pos;
    vector new_dir = (current_direction * current_speed - to_target) * 0.5f + to_target;
    new_dir.normalize();
    vector axis = current_direction ^ to_target;
    const float angle = vec_delta_ang(current_direction, to_target, axis);
    // Magic number: pi, the speed falls with the turn.
    const float new_speed = ((3.1415927f - fabsf(angle)) / 3.1415927f) * current_speed;
    havok_body_set_linear_velocity(havok_handle, new_dir * new_speed);
    orient.make((target->pos + offset) - pos);
}

// 0x00A40F80 (:1486): an air bomb steers toward its living target: below a vehicle's predicted position (5 frames
// ahead, snapped to the ground) or ahead of a human (2 frames of its velocity).
void projectile::air_bomb_seek_target(float current_speed, vector current_direction)
{
    object* target = World.object_from_handle(heat_seeking_target);
    if (target == nullptr || target->is_killed() || (target->get_obj_type() != OT_HUMAN && target->get_obj_type() != OT_VEHICLE))
    {
        return;
    }
    vector aim = ZERO_VECTOR;
    human* hp = nullptr;
    vehicle* vp = nullptr;
    const unsigned int target_handle = target->get_handle();
    if (target_handle == 0xFFFFFFFF)
    {
        return;
    }
    if (target->get_obj_type() == OT_HUMAN)
    {
        hp = World.object_from_handle<human>(target_handle);
        if (hp == nullptr)
        {
            return;
        }
        if (hp->vehicle_handle != 0xFFFFFFFF)
        {
            vp = World.object_from_handle<vehicle>(hp->vehicle_handle);
        }
    }
    else
    {
        vp = World.object_from_handle<vehicle>(target_handle);
        if (vp == nullptr)
        {
            return;
        }
    }
    if (vp != nullptr)
    {
        vector velocity;
        vehicle_get_velocity(vp, velocity);
        // Magic numbers: 5 frames ahead, 1 m below the ground.
        aim = vp->pos + velocity * Frametime * 5.0f;
        rfg_snap_to_ground(aim);
        aim.y = aim.y - 1.0f;
    }
    else
    {
        if (hp == nullptr)
        {
            return;
        }
        // Magic number: 2 frames of the human's velocity ahead.
        aim = hp->pos + hp->velocity * Frametime * 2.0f;
    }
    vector new_velocity = aim - pos;
    new_velocity = (current_direction * current_speed - new_velocity) * 0.2f + new_velocity;
    new_velocity.normalize();
    havok_body_set_linear_velocity(havok_handle, new_velocity * current_speed);
    orient.make(target->pos - pos);
}

// 0x00A71B10 (:1538): an armed proxy mine (after its activation delay) watches for living humans other than its
// owner: within the mine radius on foot or the vehicle radius in a vehicle, not friendly in multiplayer, not the
// player's squad, and (with smart mines) unfriendly. A vehicle's speed shortens the delay. A mine whose owner is
// gone fades (multiplayer server).
void projectile::process_mine()
{
    static float mine_radius_squared = Proxy_mine_radius * Proxy_mine_radius;
    static float vehicle_mine_radius_squared = Proxy_mine_vehicle_radius * Proxy_mine_vehicle_radius;
    if (active_stamp.valid())
    {
        if (active_stamp.elapsed())
        {
            active_stamp.invalidate();
            sound_info = audiolib_cue_play_3d(w_info->secondary_sound, &pos, nullptr, nullptr, nullptr);
        }
        return;
    }
    if (detonate_stamp.valid())
    {
        return;
    }
    human* owner = owner_handle != 0xFFFFFFFF ? World.object_from_handle<human>(owner_handle) : nullptr;
    if (owner == nullptr)
    {
        if (Multiplayer == true && multi_is_server() && !pflags.fading)
        {
            // Magic number: a 1 s fade.
            fade_stamp.set(1000);
            pflags.fading = 1;
        }
        return;
    }
    const int num_humans = World.get_num_objects<human>();
    for (int i = 0; i < num_humans; i++)
    {
        human* hp = World.get_object<human>(i);
        if (hp->get_handle() == owner_handle || hp->is_killed())
        {
            continue;
        }
        vehicle* vp = human_in_vehicle(hp);
        float dist_squared;
        float radius_squared;
        if (vp == nullptr)
        {
            const vector d = hp->pos - pos;
            dist_squared = d.x * d.x + d.y * d.y + d.z * d.z;
            radius_squared = mine_radius_squared;
        }
        else
        {
            const vector d = vp->pos - pos;
            dist_squared = d.x * d.x + d.y * d.y + d.z * d.z;
            radius_squared = vehicle_mine_radius_squared;
        }
        if (dist_squared > radius_squared || (Multiplayer == true && human_teams_are_friendly(owner, hp)))
        {
            continue;
        }
        if (hp->get_obj_type() == OT_HUMAN && hp->get_sub_type() == OT_SUB_HUMAN_NPC && ((npc*)hp)->get_ai()->is_in_player_squad(true) != nullptr)
        {
            continue;
        }
        if (upgrade_has_smart_proximity_mines() && !human_teams_are_unfriendly(owner, hp))
        {
            continue;
        }
        sound_info = audiolib_cue_play_3d(w_info->special_sound, &pos, nullptr, nullptr, nullptr);
        int delay_ms = Proxy_mine_detonation_delay_ms;
        float vehicle_speed;
        if (vehicle_get_speed(vp, vehicle_speed))
        {
            delay_ms = (int)fl_parameter(fabsf(vehicle_speed), 0.0f, Proxy_mine_vehicle_detonation_vel, (float)Proxy_mine_detonation_delay_ms, (float)Proxy_mine_vehicle_detonation_delay_ms, true);
        }
        detonate_stamp.set(delay_ms);
        detonator_handle = owner_handle;
        return;
    }
}

// 0x00AE0A90 (:1678): a player's charges go off with the player in single player when the player dies, and when the
// player holds the detonator (remote_charge_time above 0.5 s).
void projectile::process_charge()
{
    player* pp = owner_handle != 0xFFFFFFFF ? World.object_from_handle<player>(owner_handle) : nullptr;
    if (pp == nullptr)
    {
        return;
    }
    if (pp->is_killed())
    {
        if (!Multiplayer)
        {
            detonate(nullptr, nullptr, nullptr);
        }
        return;
    }
    // Magic number: 0.5 s.
    if (!detonate_stamp.valid() && pp->remote_charge_time > 0.5f)
    {
        audiolib_play_stop(sound_info);
        detonate_charges(pp->get_handle());
    }
}

// 0x00AE2810 (:1759): the rocket's stages by age: before the propellant (secondary rockets launch late; gravity pulls
// it down a little), the propellant (stops the launch effects; the primary rocket fires the multi rockets, spread
// sideways, and starts the flight effects) and after it (gravity again, the smoke effect, then a lift that falls off
// until the drop time). The server sets off a rocket past its drop time.
void projectile::process_rocket()
{
    const float age = (float)age_timer.time_since();
    const float prop_expire = w_info->projectile_info.time_until_prop_expire;
    const float drop = w_info->projectile_info.time_until_drop;
    vector delta_v(0.0f, -9.8f, 0.0f);
    bool apply = false;
    if (age < prop_expire)
    {
        if ((!is_primary_rocket || w_info->projectile_info.time_until_propelled < age) && !pflags.propellant_started)
        {
            pflags.propellant_started = 1;
            projectile_stop_effect(effect_obj_handle);
            projectile_stop_effect(effect_obj_handle2);
            projectile_stop_effect(effect_obj_handle3);
            projectile_stop_effect(effect_obj_handle4);
            if (upgrade_get_multi_rockets() > 1 && is_primary_rocket)
            {
                vector velocity;
                havok_body_get_linear_velocity(havok_handle, velocity);
                // Magic numbers: the sideways spread in m/s per pair (wider for heat seeking rockets).
                const float spread = heat_seeking_target == 0xFFFFFFFF ? 8.0f : 80.0f;
                for (int i = 0; i < secondary_rockets_to_fire; i++)
                {
                    matrix side;
                    side.make_uvec(velocity.copy_flatten_normalize_safe(), UP_VECTOR);
                    // Alternating right and left, one more step every pair.
                    const float offset = (float)((i % 2 != 0 ? 2 : 0) - 1) * spread * (float)((i + 2) / 2);
                    const vector rocket_velocity = velocity + side.rvec * offset;
                    obj_projectile_props props(owner_handle, w_info->projectile_info.obj_item_info, item_flags, w_info, pos, orient, rocket_velocity, target_pos, mp_create_id);
                    projectile* rocket = World.create_object<projectile>(props, 0xFFFFFFFF);
                    if (rocket != nullptr)
                    {
                        rocket->is_primary_rocket = false;
                        rocket->heat_seeking_target = heat_seeking_target;
                    }
                }
            }
            effect_obj_handle = effects_play(w_info->projectile_info.effect1, pos, orient, true, get_handle(), -1, 0xFFFFFFFF, 1.0f, false, EFFECTS_CATEGORY_NONE);
            effect_obj_handle4 = effects_play(w_info->projectile_info.effect4, pos, orient, false, get_handle(), -1, 0xFFFFFFFF, 1.0f, false, EFFECTS_CATEGORY_NONE);
        }
        else if (age < w_info->projectile_info.time_until_propelled)
        {
            delta_v.x = Frametime * 0.0f;
            delta_v.y = Frametime * -9.8f;
            delta_v.z = delta_v.x;
            apply = true;
        }
    }
    else
    {
        if (!pflags.propellant_expired)
        {
            pflags.propellant_expired = 1;
            set_acceleration(delta_v);
            projectile_stop_effect(effect_obj_handle);
            projectile_stop_effect(effect_obj_handle2);
            projectile_stop_effect(effect_obj_handle3);
            projectile_stop_effect(effect_obj_handle4);
            effect_obj_handle = effects_play(w_info->projectile_info.effect2, pos, orient, true, get_handle(), -1, 0xFFFFFFFF, 1.0f, false, EFFECTS_CATEGORY_NONE);
        }
        if (age < drop)
        {
            // Magic number: half of gravity at the end of the propellant, nothing at the drop time.
            const float lift = (drop - age) / (drop - prop_expire);
            delta_v.x = Frametime * lift * 0.0f;
            delta_v.y = lift * 4.9f * Frametime;
            delta_v.z = delta_v.x;
            apply = true;
        }
    }
    if (apply)
    {
        havok_body_apply_linear_velocity(havok_handle, delta_v);
    }
    if (Multiplayer == true && multi_is_server() && drop < age)
    {
        detonate(nullptr, nullptr, nullptr);
    }
}

// The pull of process_singularity_bomb, one frame (rfg.exe has the loop twice, inline in both phases; not a function
// there). Every object in 8 m of the bomb but other singularity bombs:
// - humans (not invulnerable; in single player outside missions not the guerrilla tech, RFC or old coot): a standing
//   one beyond Singularity_bomb_ragdoll_start_range walks toward the centre (its velocity, flat); a closer or ragdolled
//   one gets a ragdoll impulse toward it (x13) with a sideways part, and the bomb's owner as the ragdoll's source;
// - bodies (anything else with a Havok body; not infinite mass vehicles, not items in multiplayer): a velocity toward
//   the centre (x0.3), less for heavy ones (the SB falloff by mass); a mover at rest first breaks where a ray from the
//   centre past it (randomly within a 60 degree cone) meets a building (huge_damage_to_apply).
// In multiplayer allies of the owner are left alone without friendly fire. The pull falls off linearly to 8.5 m.
static void singularity_bomb_pull(const projectile* bomb, const vector& bomb_pos, const vector& center, float vacuum_multiplier)
{
    static unsigned int huge_damage_to_apply = 500000; // 0x016AFE60, enough to break any piece
    human* attacker = nullptr;
    if (bomb->get_owner_handle() != 0xFFFFFFFF)
    {
        attacker = (human*)World.object_from_handle(bomb->get_owner_handle());
        if (attacker != nullptr && (attacker->get_obj_type() != OT_HUMAN || attacker->get_sub_type() != OT_SUB_HUMAN_PLAYER))
        {
            attacker = nullptr;
        }
    }

    unsigned int handles[50]; // Magic number: 50 objects
    const int count = havok_collect_objects(bomb_pos, 8.0f, handles, 50, COLLIDE_OBJECT_COLLECTOR, nullptr, nullptr); // Magic number: 8 m
    float falloff_range = SB_full_falloff - SB_initial_falloff;
    float full_falloff = SB_full_falloff;
    if (wrecking_crew_is_active())
    {
        falloff_range = SB_full_falloff_wc - SB_initial_falloff_wc;
        full_falloff = SB_full_falloff_wc;
    }
    // Magic number: 8.5 m, where the pull ends.
    const float pull_range = 8.5f;
    for (int i = 0; i < count; i++)
    {
        object* objp = World.object_from_handle(handles[i]);
        if (objp == nullptr)
        {
            continue;
        }
        if (objp->get_obj_type() == OT_ITEM && objp->get_sub_type() == OT_SUB_ITEM_PROJECTILE && ((projectile*)objp)->w_info->weapon_class == WEAPON_CLASS_SINGULARITY_BOMB)
        {
            continue;
        }
        human* hp = nullptr;
        if (objp->get_obj_type() == OT_HUMAN)
        {
            hp = (human*)objp;
            const human_info* h_info = hp->h_info;
            if (hp->hflags.invulnerable ||
                (!Multiplayer && !wrecking_crew_is_active() && (h_info->flags.guerrilla_tech || h_info->flags.guerrilla_rfc || h_info->flags.old_coot) && !mission_is_active()))
            {
                continue;
            }
        }
        if (objp->get_obj_type() == OT_VEHICLE && ((const vehicle*)objp)->get_infinite_mass())
        {
            continue;
        }
        if (Multiplayer)
        {
            if (objp->get_obj_type() == OT_ITEM)
            {
                continue;
            }
            if (hp != nullptr && attacker != nullptr && attacker != hp && !Mp_server_options.generic_options.friendly_fire && human_teams_are_friendly(attacker, hp))
            {
                continue;
            }
        }
        if (hp != nullptr)
        {
            const human_info* h_info = hp->h_info;
            const bool immune = h_info->flags.guerrilla_tech || h_info->flags.guerrilla_rfc || h_info->flags.old_coot || hp->hflags.disallow_flinches_and_ragdolls;
            if (!Multiplayer && !wrecking_crew_is_active() && immune)
            {
                continue;
            }
            const human_ragdoll_state state = hp->ragdoll_state;
            const bool standing = state == HUMAN_RAGDOLL_STATE_BLEND_OUT || state == HUMAN_RAGDOLL_STATE_NONE || state == HUMAN_RAGDOLL_STATE_FLINCH ||
                                  (state == HUMAN_RAGDOLL_STATE_ACTIVE_ON_IMPACT && !hp->hflags.ragdoll_on_impact_use_ragdoll_pos);
            if (standing && vec_dist(bomb_pos, hp->pos) >= Singularity_bomb_ragdoll_start_range)
            {
                hp->hflags.use_current_velocity = 1;
                const vector to_center(center.x - hp->pos.x, center.y - hp->pos.y, center.z - hp->pos.z);
                const float strength = pull_range - vec_dist(hp->pos, center);
                const vector dir = to_center.copy_normalize();
                // Magic number: x2.
                hp->velocity = vector(dir.x * strength / pull_range * vacuum_multiplier * 2.0f, dir.y * strength / pull_range * vacuum_multiplier * 2.0f,
                                      dir.z * strength / pull_range * vacuum_multiplier * 2.0f);
                hp->velocity.y = 0.0f;
                continue;
            }
            const vector to_center(center.x - hp->pos.x, center.y - hp->pos.y, center.z - hp->pos.z);
            const vector sideways((float)((double)to_center.z * UP_VECTOR.y - (double)to_center.y * UP_VECTOR.z),
                                  (float)((double)to_center.x * UP_VECTOR.z - (double)to_center.z * UP_VECTOR.x),
                                  (float)((double)to_center.y * UP_VECTOR.x - (double)to_center.x * UP_VECTOR.y));
            const float strength = pull_range - vec_dist(hp->pos, center);
            const vector dir = to_center.copy_normalize();
            // Magic number: x13.
            const vector impulse(strength * dir.x * vacuum_multiplier * 13.0f, dir.y * strength * vacuum_multiplier * 13.0f, dir.z * strength * vacuum_multiplier * 13.0f);
            human_combat_apply_explosion_ragdoll_impulse(hp, impulse, sideways, attacker);
            const human_ragdoll_state new_state = hp->ragdoll_state;
            const bool ragdolled = new_state != HUMAN_RAGDOLL_STATE_BLEND_OUT && new_state != HUMAN_RAGDOLL_STATE_NONE && new_state != HUMAN_RAGDOLL_STATE_FLINCH &&
                                   (new_state != HUMAN_RAGDOLL_STATE_ACTIVE_ON_IMPACT || hp->hflags.ragdoll_on_impact_use_ragdoll_pos);
            if (ragdolled && !hp->is_killed())
            {
                hp->ragdoll_source_human_handle = bomb->get_owner_handle();
            }
            continue;
        }
        if (objp->havok_handle == 0xFFFFFFFF)
        {
            continue;
        }
        // The x87 difference of the full falloff and the mass, divided by the range, then clamped to [0, 1].
        float falloff = (float)(((double)full_falloff - (double)objp->get_mass()) / (double)falloff_range);
        if ((float)((double)falloff - 0.0) < 0.0f)
        {
            falloff = 0.0f;
        }
        if ((float)((double)falloff - 1.0) >= 0.0f)
        {
            falloff = 1.0f;
        }
        if (objp->get_obj_type() == OT_MOVER)
        {
            object_mover* mp = World.object_from_handle<rfg_mover>(objp->get_handle());
            if (mp == nullptr)
            {
                mp = World.object_from_handle<general_mover>(objp->get_handle());
            }
            if (mp == nullptr)
            {
                continue;
            }
            vector velocity;
            havok_body_get_linear_velocity(mp->havok_handle, velocity);
            if (velocity.is_null())
            {
                raycast_input ray_in;
                ray_in.radius = 0.0f;
                ray_in.filter_params = nullptr;
                ray_in.filter_callback = nullptr;
                ray_in.m_flags = 0;
                const float dist = vec_dist(bomb_pos, objp->pos);
                ray_in.start = center;
                vector dir = vector(objp->pos.x - center.x, objp->pos.y - center.y, objp->pos.z - center.z).copy_normalize_safe();
                // Original: the direction is randomized in place (the same vector in and out). Magic number: 0.5 (a
                // 60 degree cone).
                dir.rand_around_dir(dir, 0.5f, 1.0f);
                dir = vector(dir.x * dist, dir.y * dist, dir.z * dist);
                ray_in.end = vector(center.x + dir.x, center.y + dir.y, center.z + dir.z);
                ray_in.m_filter_info = COLLIDE_RAY_RFG_DESTROYABLE;
                ray_in.m_hit_collector_type = raycast_input::COLLECT_CLOSEST_HIT;
                raycast_output ray_out;
                if (havok_raycast(ray_in, ray_out))
                {
                    rfg_mover* hit = World.object_from_handle<rfg_mover>(ray_out.m_hits[0].hit_object_handle);
                    if (hit != nullptr)
                    {
                        sp_combined_direct_damage_raw damage;
                        damage.shape_key = ray_out.m_hits[0].hit_havok_shape_key;
                        damage.damage = huge_damage_to_apply;
                        rfg_dyn_add_pre_collide_direct_damage(hit, nullptr, 1, &damage, ZERO_VECTOR, ZERO_VECTOR, false);
                    }
                }
            }
        }
        const vector to_center(center.x - objp->pos.x, center.y - objp->pos.y, center.z - objp->pos.z);
        const float strength = pull_range - vec_dist(objp->pos, center);
        const vector dir = to_center.copy_normalize();
        // Magic number: x0.3.
        const vector pull(strength * dir.x * vacuum_multiplier * 0.3f * falloff, dir.y * strength * vacuum_multiplier * 0.3f * falloff,
                          dir.z * strength * vacuum_multiplier * 0.3f * falloff);
        havok_body_apply_linear_velocity(objp->havok_handle, pull);
    }
}

// 0x00AE0B20 (:1702)
// A planted singularity bomb by its age (the weapon's projectile times): nothing until time_until_propelled; then it
// lifts (detached, no collision; effect4 above it, the camera shakes within 55 m) and pulls around a centre that
// jitters up to 2 m sideways and 1-2.5 m up; after time_until_prop_expire the propellant effects stop and it pulls
// twice as hard around 1 m above its start; at time_until_drop the single player game or the multiplayer server stops
// its effects and detonates it at its start position.
void projectile::process_singularity_bomb()
{
    const float age = (float)age_timer.time_since();
    const float time_until_drop = w_info->projectile_info.time_until_drop;
    const float time_until_propelled = w_info->projectile_info.time_until_propelled;
    if (age < w_info->projectile_info.time_until_prop_expire)
    {
        if (age <= time_until_propelled || pflags.propellant_started)
        {
            // Original: at exactly time_until_propelled it pulls once before it has lifted.
            if (time_until_propelled <= age)
            {
                // Magic numbers: the jitter of the centre, up to 2 m sideways and 1-2.5 m up (x, y, z drawn in order).
                const float jitter_x = fl_rand_range(0.0f, 2.0f);
                const float jitter_y = fl_rand_range(1.0f, 2.5f);
                const float jitter_z = fl_rand_range(0.0f, 2.0f);
                const vector center(start_pos.x + jitter_x, jitter_y + start_pos.y, jitter_z + start_pos.z);
                singularity_bomb_pull(this, start_pos, center, Singularity_bomb_vacuum_multiplier);
            }
            return;
        }
        // The lift: the bomb leaves its surface and stops colliding.
        pflags.propellant_started = 1;
        detach();
        start_pos = pos;
        collision_type = COLLIDE_NO_COLLISION;
        const vector effect_pos(start_pos.x + UP_VECTOR.x, start_pos.y + UP_VECTOR.y, start_pos.z + UP_VECTOR.z);
        effect_obj_handle4 = effects_play(w_info->projectile_info.effect4, effect_pos, IDENTITY_MATRIX, false, 0xFFFFFFFF, -1, 0xFFFFFFFF, 1.0f, false, EFFECTS_CATEGORY_NONE);
        if (w_info != nullptr && w_info->secondary_camera_shake != nullptr)
        {
            local_player_info player_info;
            player_get_local_player_info(&player_info);
            float shake = 0.0f;
            if (player_info.pos != nullptr)
            {
                const vector to_bomb(start_pos.x - player_info.pos->x, start_pos.y - player_info.pos->y, start_pos.z - player_info.pos->z);
                shake = (float)(1.0 - (double)to_bomb.mag() / 55.0); // Magic number: none beyond 55 m
            }
            if ((float)((double)shake - 0.0) < 0.0f)
            {
                shake = 0.0f;
            }
            if ((float)((double)shake - 1.0) >= 0.0f)
            {
                shake = 1.0f;
            }
            camera_shake_play(w_info->secondary_camera_shake, shake, false);
        }
        return;
    }
    if (!pflags.propellant_expired)
    {
        pflags.propellant_expired = 1;
        projectile_stop_effect(effect_obj_handle2);
        projectile_stop_effect(effect_obj_handle3);
    }
    if (time_until_drop <= age)
    {
        if (!Multiplayer || multi_is_server())
        {
            projectile_stop_effect(effect_obj_handle);
            projectile_stop_effect(effect_obj_handle2);
            projectile_stop_effect(effect_obj_handle3);
            projectile_stop_effect(effect_obj_handle4);
            detonate(&start_pos, nullptr, nullptr);
        }
        return;
    }
    // The last pull, twice as hard, around 1 m above the start.
    const vector center(start_pos.x + 0.0f, start_pos.y + 1.0f, start_pos.z + 0.0f);
    singularity_bomb_pull(this, start_pos, center, (float)((double)Singularity_bomb_vacuum_multiplier * 2.0));
}

// 0x00A71ED0 (:1839): grinder and harpoon blades are removed after 5 s when the pool is full, after 5 s in flight,
// after 10 s at all, and after 5 s when they stopped; a slow blade stops cutting.
void projectile::process_grinder()
{
    const bool authority = !Multiplayer || multi_is_server();
    // Magic numbers: the lifetimes in ms.
    if (pool_remaining() < 1 && age_timer.time_since() > 4999 && authority)
    {
        World.destroy_object(this, false);
        return;
    }
    if (authority && pflags.ballistic && age_timer.time_since() > 5000)
    {
        World.destroy_object(this, false);
    }
    if (authority && age_timer.time_since() > 10000)
    {
        World.destroy_object(this, false);
    }
    if (is_attached() || havok_handle == 0xFFFFFFFF)
    {
        return;
    }
    vector velocity;
    havok_body_get_linear_velocity(havok_handle, velocity);
    const float speed_squared = velocity.x * velocity.x + velocity.y * velocity.y + velocity.z * velocity.z;
    // Magic number: 10 (m/s squared).
    if (speed_squared < 10.0f && age_timer.time_since() > 4999)
    {
        if (!authority)
        {
            return;
        }
        World.destroy_object(this, false);
        return;
    }
    if (speed_squared < w_info->low_scale_damage.scale_value)
    {
        detach_effects(true, true, true, true);
        exploded = true;
    }
}

// 0x00A1E770 (:1899)
void projectile::process_mass_driver()
{
}

// 0x00A41480 (:1951): the multiplayer server's reconstructor repairs around its hit point each pass, then goes.
void projectile::process_repair()
{
    if (Multiplayer && multi_is_server() && !pflags.ballistic && !repair_collided_this_frame)
    {
        human* hp = owner_handle != 0xFFFFFFFF ? World.object_from_handle<player>(owner_handle) : nullptr;
        if (hp == nullptr)
        {
            return;
        }
        // Magic number: the refire delay in ms.
        rfg_dyn_repair_sphere(repair_collision_point, Reconstructor_radius, (int)(w_info->default_refire_delay * 1000.0f), hp);
        repair_passes_left--;
        if (repair_passes_left == 0)
        {
            World.destroy_object(this, false);
        }
    }
    repair_collided_this_frame = false;
}

// 0x00A1E780 (:1972): the effect material and shape type of what the projectile hit (the defaults without one).
void projectile::get_collision_data(const collision_event* c_event, unsigned char* hit_material, material_shape_type* hit_shape_type, unsigned int* hit_shape_key)
{
    object* other = c_event->m_object_a;
    unsigned int shape_key = 0xFFFFFFFF;
    if (other == this && c_event->m_object_b != nullptr)
    {
        shape_key = c_event->m_collisions[0].m_shapekey_b;
        other = c_event->m_object_b;
    }
    else if (other != nullptr)
    {
        shape_key = c_event->m_collisions[0].m_shapekey_a;
    }
    if (other != nullptr)
    {
        *hit_material = other->get_effect_material(shape_key, &c_event->m_collisions[0].m_collision_point, nullptr);
        *hit_shape_type = rfg_material_get_shape_type(other, shape_key);
    }
    else
    {
        *hit_material = Rfg_effect_material_default_index;
        *hit_shape_type = MATERIAL_EVENT_SHAPE_SOLID;
    }
    if (hit_shape_key != nullptr)
    {
        *hit_shape_key = shape_key;
    }
}

// 0x00A1E800 (:1999): the custom force on the body moves from the current to the target acceleration.
void projectile::set_acceleration(const vector& target_accel)
{
    const vector delta = target_accel - current_accel;
    // Magic number: changes below 0.001 are skipped.
    if (1.0000001e-06f < delta.y * delta.y + delta.x * delta.x + delta.z * delta.z)
    {
        havok_body_add_custom_force(havok_handle, delta);
    }
    current_accel = target_accel;
}

// 0x00A1E8D0 (:2017)
void projectile::remove_dead_harpoon()
{
    if (w_info->weapon_class == WEAPON_CLASS_HARPOON)
    {
        World.destroy_object(this, false);
    }
}

// 0x00AE2E50 (:2029)
// A grinder blade or harpoon hits (once: exploded marks it). Too slow, it falls dead (a harpoon is removed). A human
// takes damage by speed (head and leg multipliers), a dead one ragdolls with the blade stuck in the bone, a live one
// flinches; the server adds the spark, effect and decal and tells the clients (PT_IMPALING_COLLIDE). Anything else:
// sparks at the strongest contact, the server damages movers, items and vehicles, and a fast enough steep hit sticks.
// A harpoon that neither hit a human nor stuck is destroyed. With the explosive upgrade (Wrecking Crew) grinder blades
// detonate instead. The decompile of the non-human tail is wrong (shared stack slots); it was read from the assembly.
void projectile::impaling_collide(const collision_event* c_event, object* other_obj, const vector& human_hit_pos, int human_hit_bone)
{
    static float ragdoll_damage = 100.0f; // 0x016AFE8C, the damage above which a dead victim starts to ragdoll
    float ragdoll_impulse_scale;
    float attach_dp;
    float attach_speed;
    float attach_offset;
    if (w_info->weapon_class == WEAPON_CLASS_HARPOON)
    {
        ragdoll_impulse_scale = Harpoon_ragdoll_impulse_scale;
        attach_dp = Harpoon_attach_dp;
        attach_speed = Harpoon_attach_speed;
        attach_offset = 0.0f;
    }
    else
    {
        ragdoll_impulse_scale = Grinder_ragdoll_impulse_scale;
        attach_dp = Grinder_attach_dp;
        attach_speed = Grinder_attach_speed;
        attach_offset = Grinder_attach_offset;
    }
    if (havok_handle == 0xFFFFFFFF || child_constraint_ptr != nullptr || parent != 0xFFFFFFFF)
    {
        return;
    }
    projectile_stop_effect(effect_obj_handle);
    projectile_stop_effect(effect_obj_handle2);
    projectile_stop_effect(effect_obj_handle3);
    projectile_stop_effect(effect_obj_handle4);
    if (exploded)
    {
        if (w_info->weapon_class == WEAPON_CLASS_HARPOON)
        {
            World.destroy_object(this, false);
        }
        return;
    }
    exploded = true;
    vector velocity;
    havok_body_get_linear_velocity(havok_handle, velocity);
    const bool is_grinder = w_info->weapon_class == WEAPON_CLASS_GRINDER;
    float speed_squared = (float)((double)velocity.x * velocity.x + (double)velocity.y * velocity.y + (double)velocity.z * velocity.z);
    const float low_scale = w_info->low_scale_damage.scale_value;
    // Too slow: dead (a grinder blade at rest checks its launch speed instead; the explosive upgrade never is).
    if (speed_squared < low_scale &&
        (!is_grinder || (!upgrade_has_grinder_explosive() && (speed_squared != 0.0f || (speed_squared = start_velocity.mag_squared()) < low_scale))))
    {
        remove_dead_harpoon();
        return;
    }
    // Items: only weapons and explosive items are hit.
    item* hit_item = other_obj != nullptr && other_obj->get_obj_type() == OT_ITEM ? (item*)other_obj : nullptr;
    if (hit_item != nullptr && hit_item->get_sub_type() != OT_SUB_ITEM_WEAPON && !hit_item->damage_info.flags.can_explode)
    {
        remove_dead_harpoon();
        return;
    }
    // Set when the human hit handled the projectile or it stuck: then a harpoon stays.
    bool handled = false;
    if (other_obj != nullptr && other_obj->get_obj_type() == OT_HUMAN)
    {
        human* victim = (human*)other_obj;
        if (is_grinder && upgrade_has_grinder_explosive())
        {
            exploded = false;
            detonate(nullptr, nullptr, nullptr);
            return;
        }
        if (victim->is_shielded(pos, false))
        {
            remove_dead_harpoon();
            return;
        }
        human* attacker = World.object_from_handle<human>(owner_handle);
        if (Multiplayer && multi_game_mode_has_teams() && !Mp_server_options.generic_options.friendly_fire && human_teams_are_allies(victim, attacker))
        {
            if (w_info->weapon_class == WEAPON_CLASS_HARPOON)
            {
                World.destroy_object(this, false);
            }
            return;
        }
        const weapon_damage_category category = weapons_get_damage_category(victim);
        float damage = fl_parameter(speed_squared, low_scale, w_info->high_scale_damage.scale_value, w_info->low_scale_damage.damage[category],
                                    w_info->high_scale_damage.damage[category], true);
        const object_hit_location hit_location = human_get_hit_location_from_bone_index(victim, human_hit_bone);
        if (hit_location == HIT_HEAD)
        {
            damage = (Multiplayer ? Mp_head_damage_multiplier : w_info->headshot_multiplier) * damage;
        }
        else if (hit_location > HIT_HEAD && hit_location <= HIT_RIGHT_LEG && Multiplayer)
        {
            damage = Mp_leg_damage_multiplier * damage;
        }
        const bool was_dead = human_is_dead(victim);
        human_damage(victim, attacker, damage, DBT_NONE, w_info, nullptr, true, false);
        if (Multiplayer == true && !was_dead && human_is_dead(victim) && hit_location == HIT_HEAD)
        {
            player* pp = World.object_cast<player>(attacker);
            if (pp != nullptr && (object*)pp != other_obj && human_teams_are_enemies(attacker, victim))
            {
                // Magic numbers: one repetition, no weapon (0xFF) or backpack (-1) id.
                multi_game_xp_trigger_event(pp->mp_data, MXE_HEADSHOT_KILL, 1, 0xFF, -1, false);
            }
        }
        if (object_is_local_player(victim))
        {
            human_combat_process_hit_indicator(victim, attacker != nullptr ? &attacker->pos : nullptr, nullptr, damage);
            if (!Multiplayer)
            {
                remove_dead_harpoon();
            }
        }
        vector hit_pos = pos;
        if (human_hit_bone != -1)
        {
            victim->get_tag_world_coords(human_hit_bone, hit_pos, nullptr, nullptr, nullptr);
        }
        if (!human_is_ragdolled(victim) && human_in_vehicle(victim) == nullptr)
        {
            const vector to_hit(human_hit_pos.x - hit_pos.x, human_hit_pos.y - hit_pos.y, human_hit_pos.z - hit_pos.z);
            if (human_is_dead(victim))
            {
                if (damage > ragdoll_damage)
                {
                    human_ragdoll_start(victim, false, 0xFFFFFFFF);
                }
                const vector impulse(velocity.x * ragdoll_impulse_scale, velocity.y * ragdoll_impulse_scale, velocity.z * ragdoll_impulse_scale);
                havok_ragdoll_apply_velocity(victim->ragdoll_idx, impulse, true);
                havok_ragdoll_apply_point_impulse(victim->ragdoll_idx, impulse, pos);
                if (localization_allow_attaching_projectiles())
                {
                    // The blade sticks 0.3 m from the bone towards the hit point.
                    const vector dir = to_hit.copy_normalize_safe();
                    pos = vector(dir.x * 0.3f + hit_pos.x, dir.y * 0.3f + hit_pos.y, dir.z * 0.3f + hit_pos.z);
                    other_obj->set_child(this, human_hit_bone, -1, true, true);
                    pflags.release_havok_object = 1;
                }
                else
                {
                    velocity = vector(velocity.x * 0.3f, velocity.y * 0.3f, velocity.z * 0.3f);
                    havok_body_set_linear_velocity(havok_handle, velocity);
                }
            }
            else if (!victim->char_inst->is_playing_action(0x3E4)) // Magic number: action alias 0x3E4 (no flinch while it plays)
            {
                object_hit_location flinch_location = human_get_point_hit_location(victim, pos, 0.05f, nullptr); // Magic number: 5 cm
                if (flinch_location == HIT_NONE)
                {
                    flinch_location = HIT_GENERAL;
                }
                int large_flinch_percent = (int)((double)ragdoll_impulse_scale * 100.0);
                if (large_flinch_percent >= 100)
                {
                    large_flinch_percent = 100;
                }
                const vector dir = to_hit.copy_normalize_safe();
                human_combat_play_flinch(victim, pos, dir, flinch_location, (unsigned int)large_flinch_percent);
                velocity = vector(velocity.x * 0.3f, velocity.y * 0.3f, velocity.z * 0.3f);
                havok_body_set_linear_velocity(havok_handle, velocity);
            }
            handled = true;
        }
        if (!Multiplayer || multi_is_server())
        {
            const unsigned char material = victim->get_effect_material(0xFFFFFFFF, nullptr, nullptr);
            const vector dir = velocity.copy_normalize_safe();
            const float speed = (float)sqrt((double)speed_squared);
            const vector back(-dir.x, -dir.y, -dir.z);
            bullet_spark_create(hit_pos, back, w_info, material, MATERIAL_EVENT_SHAPE_SOLID, nullptr, 0xFFFFFFFF, speed);
            matrix orient_back;
            orient_back.make(back);
            effect_obj_handle2 = effects_play(w_info->projectile_info.effect2, hit_pos, orient_back, false, 0xFFFFFFFF, -1, 0xFFFFFFFF, 1.0f, false, EFFECTS_CATEGORY_NONE);
            object_decal_create_bullet_impact_decal(other_obj, attacker, w_info, hit_pos, back, damage, nullptr);
            if (Multiplayer == true && multi_is_server())
            {
                multi_gw_send_impaling_collide(hit_pos, back, w_info, material, MATERIAL_EVENT_SHAPE_SOLID, victim, speed);
            }
        }
    }
    else if (c_event != nullptr)
    {
        if (is_grinder && upgrade_has_grinder_explosive())
        {
            exploded = false;
            // Magic number: a blade older than 50 ms goes off at once; a fresh one waits for its fuse.
            if (age_timer.time_since() > 50)
            {
                collision_detonate(c_event, other_obj);
                return;
            }
            detonate_stamp.invalidate();
        }
        // The strongest contact (the highest relative normal velocity). Original: an event without contacts would read
        // through a null pointer; collision events always carry at least one.
        const collision_event::collision_info* best = nullptr;
        for (unsigned int i = 0; i < c_event->m_num_collisions; i++)
        {
            if (best == nullptr || best->m_relative_normal_velocity < c_event->m_collisions[i].m_relative_normal_velocity)
            {
                best = &c_event->m_collisions[i];
            }
        }
        unsigned char material;
        material_shape_type shape_type;
        unsigned int shape_key;
        get_collision_data(c_event, &material, &shape_type, &shape_key);
        if (best->m_relative_normal_velocity > 2.0f) // Magic number: sparks above 2 m/s
        {
            vector spark_pos = best->m_collision_point;
            if (w_info->weapon_class == WEAPON_CLASS_HARPOON)
            {
                const vector& n = best->m_collision_normal;
                spark_pos = vector((float)((double)n.x * Harpoon_effect_offset + spark_pos.x), (float)((double)n.y * Harpoon_effect_offset + spark_pos.y),
                                   (float)((double)n.z * Harpoon_effect_offset + spark_pos.z));
            }
            bullet_spark_create(spark_pos, best->m_collision_normal, w_info, material, shape_type, other_obj, shape_key, (float)sqrt((double)speed_squared));
            matrix effect_orient;
            effect_orient.make(best->m_collision_normal);
            effect_obj_handle2 = effects_play(w_info->projectile_info.effect2, spark_pos, effect_orient, false, get_handle(), -1, 0xFFFFFFFF, 1.0f, false, EFFECTS_CATEGORY_NONE);
        }
        const vector& n = best->m_collision_normal;
        const float dp = fabsf((float)((double)n.y * orient.uvec.y + (double)orient.uvec.x * n.x + (double)n.z * orient.uvec.z));
        object_mover* hit_mover = other_obj != nullptr && other_obj->get_obj_type() == OT_MOVER ? (object_mover*)other_obj : nullptr;
        object_mover* hit_general_mover = hit_mover != nullptr && other_obj->get_sub_type() == OT_SUB_MOVER_GENERAL ? hit_mover : nullptr;
        vehicle* hit_vehicle = other_obj != nullptr && other_obj->get_obj_type() == OT_VEHICLE ? (vehicle*)other_obj : nullptr;
        object* owner = World.object_from_handle(owner_handle);
        // Magic number: 350 damage to a mover or an item (the vtable damage functions, +0x88 and +0x8C).
        if (hit_mover != nullptr)
        {
            if (!Multiplayer || multi_is_server())
            {
                hit_mover->apply_damage(350.0f, DBT_BULLET, best->m_collision_point, best->m_collision_normal, owner, shape_key, false);
            }
        }
        else if (hit_item != nullptr)
        {
            if (!Multiplayer || multi_is_server())
            {
                hit_item->apply_damage(350.0f, DBT_BULLET, best->m_collision_point, best->m_collision_normal, owner, 0xFFFFFFFF, false);
            }
        }
        else if (hit_vehicle != nullptr && (!Multiplayer || multi_is_server()))
        {
            const weapon_damage_category category = weapons_get_damage_category(hit_vehicle);
            const float damage = fl_parameter(speed_squared, low_scale, w_info->high_scale_damage.scale_value, w_info->low_scale_damage.damage[category],
                                              w_info->high_scale_damage.damage[category], true);
            vehicle_apply_damage(hit_vehicle, owner, w_info, DBT_BULLET, &best->m_collision_point, &best->m_collision_normal, 1.0f, (int)damage, nullptr, false);
        }
        // A steep enough, fast enough hit sticks; into an item or a general mover only when it can explode.
        if (dp < attach_dp && best->m_relative_normal_velocity > attach_speed)
        {
            const bool explosive_target = (hit_item != nullptr && hit_item->damage_info.flags.can_explode) ||
                                          (hit_general_mover != nullptr && hit_general_mover->m_level_obj_properties->m_flags.m_death_explosion);
            if ((hit_general_mover == nullptr && hit_item == nullptr) || explosive_target)
            {
                if (!is_grinder || other_obj == nullptr || other_obj->get_obj_type() != OT_ZONE)
                {
                    if (is_grinder && upgrade_has_grinder_explosive())
                    {
                        exploded = false;
                        collision_detonate(c_event, other_obj);
                        return;
                    }
                    collision_attach(c_event, other_obj, attach_offset, &orient);
                    handled = true;
                }
            }
        }
    }
    if (other_obj == nullptr || other_obj->get_obj_type() != OT_HUMAN)
    {
        hkpRigidBody* rb = havok_body_get_pointer(havok_handle);
        if (havok_has_ignore_property(rb))
        {
            havok_remove_ignore_property(rb);
        }
    }
    if (!handled && w_info->weapon_class == WEAPON_CLASS_HARPOON)
    {
        World.destroy_object(this, false);
    }
}

// 0x00AD7650 (:2362)
// A mass driver slug (a turret weapon) passes through items and humans: a human takes damage by speed (team allies
// not without friendly fire), a non-player one ragdolls above ragdoll_damage with an impulse along the slug, and the
// slug keeps its previous velocity. Anything else: sparks at the strongest contact above 2 m/s; the server damages
// vehicles (Mass_driver_vehicle_damage) and movers (100); the local player in a hit vehicle sees a HUD hit.
void projectile::mass_driver_collide(const collision_event* c_event, object* other_obj)
{
    static float ragdoll_impulse_scale = 0.2f; // 0x016AFE64
    static float ragdoll_damage = 100.0f; // 0x016AFE68, the damage above which a non-player starts to ragdoll
    if (other_obj != nullptr && other_obj->get_obj_type() == OT_ITEM)
    {
        return;
    }
    if (havok_handle == 0xFFFFFFFF)
    {
        return;
    }
    vector velocity;
    havok_body_get_linear_velocity(havok_handle, velocity);
    const float speed_squared = (float)((double)velocity.x * velocity.x + (double)velocity.y * velocity.y + (double)velocity.z * velocity.z);
    const float low_scale = w_info->low_scale_damage.scale_value;
    if (speed_squared < low_scale)
    {
        return;
    }
    if (other_obj != nullptr && other_obj->get_obj_type() == OT_HUMAN)
    {
        human* victim = (human*)other_obj;
        human* attacker = nullptr;
        if (owner_handle != 0xFFFFFFFF)
        {
            attacker = (human*)World.object_from_handle(owner_handle);
            if (attacker != nullptr && attacker->get_obj_type() != OT_HUMAN)
            {
                attacker = nullptr;
            }
        }
        if (Multiplayer && multi_game_mode_has_teams() && !Mp_server_options.generic_options.friendly_fire && human_teams_are_allies(victim, attacker))
        {
            return;
        }
        const weapon_damage_category category = weapons_get_damage_category(victim);
        const float damage = fl_parameter(speed_squared, low_scale, w_info->high_scale_damage.scale_value, w_info->low_scale_damage.damage[category],
                                          w_info->high_scale_damage.damage[category], true);
        human_damage(victim, attacker, damage, DBT_BULLET, w_info, nullptr, true, false);
        if ((player*)victim == Player)
        {
            human_combat_process_hit_indicator(victim, attacker != nullptr ? &attacker->pos : nullptr, nullptr, damage);
        }
        if (victim->get_sub_type() != OT_SUB_HUMAN_PLAYER)
        {
            if (damage > ragdoll_damage)
            {
                human_ragdoll_start(victim, false, 0xFFFFFFFF);
            }
            const vector impulse(ragdoll_impulse_scale * velocity.x, ragdoll_impulse_scale * velocity.y, velocity.z * ragdoll_impulse_scale);
            havok_ragdoll_apply_point_impulse(victim->ragdoll_idx, impulse, victim->pos);
        }
        const unsigned char material = victim->get_effect_material(0xFFFFFFFF, nullptr, nullptr);
        const vector dir = velocity.copy_normalize_safe();
        bullet_spark_create(pos, vector(-dir.x, -dir.y, -dir.z), w_info, material, MATERIAL_EVENT_SHAPE_SOLID, victim, 0xFFFFFFFF, -1.0f);
        // The slug passes through: it keeps the velocity it had before the hit.
        havok_body_set_linear_velocity(havok_handle, previous_velocity);
        return;
    }
    if (c_event == nullptr)
    {
        return;
    }
    // The strongest contact (the highest relative normal velocity). Original: an event without contacts would read
    // through a null pointer; collision events always carry at least one.
    const collision_event::collision_info* best = nullptr;
    for (unsigned int i = 0; i < c_event->m_num_collisions; i++)
    {
        if (best == nullptr || best->m_relative_normal_velocity < c_event->m_collisions[i].m_relative_normal_velocity)
        {
            best = &c_event->m_collisions[i];
        }
    }
    // Inlined in rfg.exe (the same code as get_collision_data).
    unsigned char material;
    material_shape_type shape_type;
    get_collision_data(c_event, &material, &shape_type, nullptr);
    if (best->m_relative_normal_velocity > 2.0f) // Magic number: sparks above 2 m/s
    {
        bullet_spark_create(best->m_collision_point, best->m_collision_normal, w_info, material, shape_type, other_obj, 0xFFFFFFFF, -1.0f);
    }
    if (other_obj == nullptr)
    {
        return;
    }
    if (other_obj->get_obj_type() == OT_VEHICLE)
    {
        vehicle* vp = (vehicle*)other_obj;
        if (!Multiplayer || multi_is_server())
        {
            object* attacker = World.object_from_handle(owner_handle);
            vehicle_apply_damage(vp, attacker, nullptr, DBT_BULLET, &best->m_collision_point, &best->m_collision_normal, 1.0f, Mass_driver_vehicle_damage, &velocity, false);
        }
        player* pp = Player;
        if (pp != nullptr && human_in_vehicle(pp, vp))
        {
            const float hit_damage = (float)Mass_driver_vehicle_damage;
            // Original: the local player is compared with itself before the death check.
            if (pp != Player || !pp->is_killed())
            {
                hud_register_hit(velocity, (float)((double)hit_damage / 50.0)); // Magic number: 50 damage per full HUD hit
            }
        }
    }
    if (other_obj->get_obj_type() == OT_MOVER && (!Multiplayer || multi_is_server()))
    {
        object* attacker = World.object_from_handle(owner_handle);
        // Magic number: 100 damage (rfg.exe reads the shared constant NEAR_FOLLOW_DEST_DIST_SQ, 100.0f); no shape key.
        ((object_mover*)other_obj)->apply_damage(100.0f, DBT_BULLET, best->m_collision_point, best->m_collision_normal, attacker, 0xFFFFFFFF, false);
    }
}

// 0x00A1E8F0 (:2488): a nano rifle shot goes off at once, with sparks where it hit.
void projectile::nano_rifle_collide(const collision_event* c_event, object* other_obj)
{
    detonate_stamp.set(0);
    if (c_event == nullptr)
    {
        return;
    }
    const vector hit_pos = c_event->m_collisions[0].m_collision_point;
    const vector hit_normal = c_event->m_collisions[0].m_collision_normal;
    unsigned char material;
    material_shape_type shape_type;
    get_collision_data(c_event, &material, &shape_type, nullptr);
    bullet_spark_create(hit_pos, hit_normal, w_info, material, shape_type, other_obj, 0xFFFFFFFF, -1.0f);
}

// 0x00A956E0 (:2506): a reconstructor shot stops showing and colliding; the server starts its repairs at the hit.
void projectile::repair_collide(const collision_event* c_event, object* other_obj)
{
    World.destroy_object(effect_obj_handle, false);
    obj_flags._dont_use_me = 0;
    effect_obj_handle = 0xFFFFFFFF;
    // Inlined render flag 2 (set_render_flags).
    set_render_flags(2);
    if (havok_handle != 0xFFFFFFFF)
    {
        havok_body_set_collision_type(havok_handle, COLLIDE_NO_COLLISION, true, false);
    }
    if (Multiplayer == true && !multi_is_server())
    {
        return;
    }
    human* hp = pflags.ballistic && owner_handle != 0xFFFFFFFF ? World.object_from_handle<player>(owner_handle) : nullptr;
    if (hp != nullptr && c_event != nullptr && c_event->m_num_collisions != 0)
    {
        repair_collision_point = c_event->m_collisions[0].m_collision_point;
        repair_passes_left = Reconstructor_passes - 1;
        repair_collided_this_frame = true;
        rfg_dyn_repair_sphere(repair_collision_point, Reconstructor_radius, (int)(w_info->default_refire_delay * 1000.0f), hp);
    }
}

// 0x00A1E9E0 (:2541): what a multiplayer projectile may hit: remote objects, rfg movers and zones (sub type 1 movers,
// object type 0x13).
bool projectile_is_valid_mp_collision_target(const object* collision_object)
{
    if (collision_object->remote_obj_flags.remote_object)
    {
        return true;
    }
    if (collision_object->get_obj_type() == OT_MOVER && collision_object->get_sub_type() == OT_SUB_MOVER_RFG)
    {
        return true;
    }
    return collision_object->get_obj_type() == OT_ZONE;
}

// 0x00A41540 (:2560): a free "attach on impact" projectile sticks to humans (unless it must not).
bool projectile::allow_collide_human(const object* target_obj)
{
    if (pflags.no_attach_to_humans || target_obj == nullptr || target_obj->get_obj_type() != OT_HUMAN)
    {
        return false;
    }
    const unsigned int flags = w_info->projectile_info.flags;
    if ((flags & PIF_ATTACH_ON_IMPACT) == 0 || (flags & PIF_DONT_ATTACH_TO_HUMANS) != 0 || is_attached())
    {
        return false;
    }
    return get_parent() == 0xFFFFFFFF;
}

// 0x00A1EA00 (:2594): the line to a hit human must only meet humans (or the projectile).
static bool projectile_human_collide_raycast_callback(object* hit_objp, void* cb_data)
{
    return hit_objp->get_obj_type() != OT_HUMAN && (void*)(uintptr_t)hit_objp->get_handle() != cb_data;
}

// 0x00AE3F10 (:2605): what the projectile does on a hit, by weapon: a human behind cover is not hit; multiplayer
// projectiles only collide with valid targets; the player feels the hit; riot shields deflect blades; certain
// vehicle shapes (collision filter 0x2F) bounce it; then damage on impact, detonate on impact (a rocket kills the
// human it hits outright in multiplayer), attach to humans, attach/bounce/detonate on everything else; the foley.
void projectile::collide(const collision_event* c_event, object* other_obj)
{
    object* hit = other_obj;
    if (obj_flags.destroyed || is_attached())
    {
        return;
    }
    if (other_obj != nullptr && other_obj->get_obj_type() == OT_HUMAN)
    {
        raycast_input ray_in;
        ray_in.start = pos;
        ray_in.end = c_event->m_collisions[0].m_collision_point;
        ray_in.m_filter_info = collision_type;
        ray_in.radius = 0.0f;
        ray_in.filter_params = (void*)(uintptr_t)get_handle();
        ray_in.m_flags = 0;
        ray_in.m_hit_collector_type = raycast_input::COLLECT_ALL_HITS;
        ray_in.filter_callback = projectile_human_collide_raycast_callback;
        raycast_output ray_out;
        if (havok_raycast(ray_in, ray_out))
        {
            return;
        }
    }
    object* owner = World.object_from_handle(owner_handle);
    human* attacker = owner != nullptr && owner->get_obj_type() == OT_HUMAN ? (human*)owner : nullptr;
    if (Multiplayer && hit != nullptr && !projectile_is_valid_mp_collision_target(hit))
    {
        return;
    }
    if (hit != nullptr)
    {
        if ((player*)hit == Player && w_info->projectile_info.hit_camera_shake != nullptr && (!Multiplayer || (pflags.ballistic && multi_gw_can_hit_target(attacker, hit))))
        {
            camera_shake_play(w_info->projectile_info.hit_camera_shake, 1.0f, false);
        }
        // Item info flags 0x3000: a shield.
        if (hit->get_obj_type() == OT_ITEM && (((item*)hit)->info->flags & 0x3000) != 0)
        {
            const weapon_class_type wc = w_info->weapon_class;
            if (wc == WEAPON_CLASS_GRINDER || wc == WEAPON_CLASS_HARPOON || (w_info->projectile_info.flags & (PIF_ATTACH_ON_IMPACT | PIF_BOUNCE_ON_IMPACT | PIF_ATTACH_TO_GROUND)) != 0)
            {
                collision_shield_deflect(c_event, (item*)hit);
                return;
            }
        }
    }
    // Magic number: vehicle collision filter 0x2F (the shapes attachments bounce off).
    const auto vehicle_bounces = [&]() {
        object* body_obj;
        unsigned int shape_key;
        const hkpRigidBody* rb;
        if (c_event->m_object_a == this)
        {
            body_obj = c_event->m_object_b;
            shape_key = c_event->m_collisions[0].m_shapekey_b;
            rb = c_event->m_collisions[0].m_hk_rigid_body_b;
        }
        else
        {
            body_obj = c_event->m_object_a;
            shape_key = c_event->m_collisions[0].m_shapekey_a;
            rb = c_event->m_collisions[0].m_hk_rigid_body_a;
        }
        const int alt_body = body_obj->get_alt_hk_body_handle_index(havok_rigid_body_get_handle(rb));
        return (vehicle_get_collision_filter((vehicle*)hit, alt_body, shape_key) & 0xFFFF) == 0x2F;
    };
    if ((w_info->projectile_info.flags & (PIF_ATTACH_ON_IMPACT | PIF_ATTACH_TO_GROUND)) != 0 && hit != nullptr && hit->get_obj_type() == OT_VEHICLE && vehicle_bounces())
    {
        collision_bounce(c_event, hit);
        return;
    }
    if (w_info->weapon_class == WEAPON_CLASS_REPAIR)
    {
        repair_collide(c_event, hit);
    }
    pflags.ballistic = 0;
    if (Multiplayer)
    {
        if (!mp_no_sync_timer.valid())
        {
            mp_no_sync_timer.set(0);
        }
        // Magic number: the item's damping is restored once it differs by more than 0.001.
        if (havok_handle != 0xFFFFFFFF)
        {
            const float damping = w_info->projectile_info.obj_item_info->linear_damping;
            if (0.001f < fabsf(havok_body_get_linear_dampening(havok_handle) - damping))
            {
                havok_body_set_linear_dampening(havok_handle, damping);
            }
        }
    }
    switch (w_info->weapon_class)
    {
    case WEAPON_CLASS_GRINDER:
    {
        const vector gravity(0.0f, -9.8f, 0.0f);
        set_acceleration(gravity);
    }
    // fall through
    case WEAPON_CLASS_HARPOON:
        if (c_event != nullptr)
        {
            if (hit != nullptr && hit->get_obj_type() == OT_VEHICLE && vehicle_bounces())
            {
                collision_bounce(c_event, hit);
                return;
            }
            impaling_collide(c_event, hit, c_event->m_collisions[0].m_collision_point, c_event->m_collisions[0].m_shapekey_b);
            return;
        }
        break;
    case WEAPON_CLASS_NANO_RIFLE:
        if (!detonate_stamp.valid() && !is_attached())
        {
            nano_rifle_collide(c_event, hit);
        }
        break;
    case WEAPON_CLASS_MASS_DRIVER:
        mass_driver_collide(c_event, hit);
        return;
    case WEAPON_CLASS_REPAIR:
        return;
    default:
        break;
    }
    const unsigned int flags = w_info->projectile_info.flags;
    if ((flags & PIF_DAMAGE_ON_IMPACT) != 0)
    {
        collision_damage(c_event, hit);
        return;
    }
    if (c_event == nullptr || (hit != nullptr && hit->get_obj_type() == OT_HUMAN))
    {
        if (w_info->projectile_info.fuse_time == 0 && (flags & PIF_DETONATE_ON_IMPACT) != 0)
        {
            if (Multiplayer && !multi_is_server())
            {
                // The client hides its copy and waits for the server's explosion.
                if (havok_handle == 0xFFFFFFFF)
                {
                    return;
                }
                detach_effects(true, true, true, true);
                havok_body_set_collision_type(havok_handle, COLLIDE_NO_COLLISION, true, false);
                client_prediction_net_time = multi_core_clock_get_time();
                return;
            }
            if (Multiplayer == true && hit != nullptr && hit->get_obj_type() == OT_HUMAN)
            {
                // Magic number: a direct rocket hit kills.
                const multi_custom_weapons wep = w_info->name != nullptr && v_stricmp(w_info->name, "thermobaric_rockets") == 0 ? MCW_THERMOBARIC_ROCKET_HIT : MCW_ROCKET_HIT;
                human_damage((human*)hit, attacker, 100000.0f, DBT_EXPLOSION, multi_gw_get_custom_weapon_info(wep), owner, true, false);
            }
            const vector* detonate_pos = nullptr;
            const vector* surface_normal = nullptr;
            if (c_event != nullptr)
            {
                detonate_pos = &c_event->m_collisions[0].m_collision_point;
                surface_normal = &c_event->m_collisions[0].m_collision_normal;
            }
            detonate(detonate_pos, surface_normal, nullptr);
            return;
        }
        if (!allow_collide_human(hit) || hit == nullptr || hit->get_obj_type() != OT_HUMAN || c_event->m_num_collisions == 0)
        {
            return;
        }
        if (((human*)hit)->is_shielded(pos, false))
        {
            return;
        }
        attach_to_human((human*)hit, c_event->m_collisions[0].m_collision_point, c_event->m_collisions[0].m_collision_normal, c_event->m_collisions[0].m_shapekey_b);
        return;
    }
    if (w_info->projectile_info.fuse_time == 0)
    {
        if ((flags & (PIF_ATTACH_ON_IMPACT | PIF_ATTACH_TO_GROUND)) != 0)
        {
            collision_attach(c_event, hit, 0.0f, (flags & PIF_ATTACH_ORIENT_TO_FLIGHT_PATH) != 0 ? &orient : nullptr);
        }
        else if ((flags & PIF_BOUNCE_ON_IMPACT) != 0)
        {
            collision_bounce(c_event, hit);
        }
        else if ((flags & PIF_DETONATE_ON_IMPACT) != 0)
        {
            collision_detonate(c_event, hit);
        }
    }
    else if (Multiplayer == true && multi_is_server() && (w_info->projectile_info.flags & (PIF_ATTACH_ON_IMPACT | PIF_ATTACH_TO_GROUND)) != 0)
    {
        collision_attach(c_event, hit, 0.0f, nullptr);
    }
    // Magic number: the foley plays for impacts faster than 0.5 m/s (an "attach to ground" projectile once attached).
    if (info->m_foley_collision != -1 && !audiolib_is_playing(m_foley_collision_instance, true) && c_event->m_num_collisions != 0 &&
        0.5f < fabsf(c_event->m_collisions[0].m_relative_normal_velocity) && ((w_info->projectile_info.flags & PIF_ATTACH_TO_GROUND) == 0 || is_attached()))
    {
        const vector foley_pos = c_event->m_collisions[0].m_collision_point;
        m_foley_collision_instance = audiolib_cue_play_3d(info->m_foley_collision, &foley_pos, nullptr, nullptr, nullptr);
    }
}

// 0x00AE45F0 (:2900): a thermobaric rocket may be set off in the air after its delay.
bool projectile::mid_air_detonate(const vector* detonate_pos, const vector* surface_normal, object* attacker)
{
    if ((w_info->projectile_info.flags & PIF_ALLOW_MID_AIR_DETONATION) != 0 && age_timer.time_since() >= Thermo_mid_air_detonation_delay_ms)
    {
        detonate(detonate_pos, surface_normal, attacker);
        return true;
    }
    return false;
}

// 0x00AD7BF0 (:2919): the explosion (single player or the server; a client hides its copy, a client charge waits).
// The explosion faces the surface (a randomly turned identity elsewhere); its source is the owner when an attacker
// that is not the owner set it off (multiplayer). A charge stuck to a player kills that player (multiplayer server).
// Npcs' projectiles use the AI explosion.
void projectile::detonate(const vector* detonate_pos, const vector* surface_normal, object* attacker)
{
    if (obj_flags.destroyed || exploded)
    {
        return;
    }
    if ((w_info->projectile_info.flags & PIF_IMMEDIATE_EFFECT_DETACH) != 0)
    {
        detach_effects(true, true, true, true);
    }
    if (!Multiplayer || multi_is_server())
    {
        obj_flags._dont_use_me = 0;
        // Inlined render flag 2 (set_render_flags).
        set_render_flags(2);
        World.destroy_object(this, false);
    }
    else if (w_info->weapon_class != WEAPON_CLASS_THROWN_CHARGE)
    {
        object_set_visibility_plus_children(this, false);
        havok_body_set_collision_type(havok_handle, COLLIDE_NO_COLLISION, true, false);
        return;
    }
    if (Multiplayer == true && !multi_is_server())
    {
        return;
    }
    exploded = true;
    if (Multiplayer == true && w_info->weapon_class == WEAPON_CLASS_GRINDER)
    {
        return;
    }
    if (w_info->m_explosion_info == nullptr)
    {
        return;
    }
    if (detonate_pos == nullptr)
    {
        detonate_pos = &pos;
    }
    vector up;
    vector reverse_normal;
    matrix explosion_orient;
    bool face_surface = true;
    if ((w_info->projectile_info.flags & PIF_DETONATE_ON_IMPACT) == 0)
    {
        if (w_info->weapon_class != WEAPON_CLASS_THROWN_CHARGE)
        {
            up = UP_VECTOR;
            explosion_orient.make_identity();
            face_surface = false;
        }
        else
        {
            // A charge blows away from what it sticks to.
            up = pflags.is_attached_backwards ? orient.fvec : vector(-orient.fvec.x, -orient.fvec.y, -orient.fvec.z);
            reverse_normal = vector(-up.x, -up.y, -up.z);
            surface_normal = &reverse_normal;
        }
    }
    else
    {
        up = *detonate_pos - pos;
        up.normalize_safe();
        if (surface_normal == nullptr)
        {
            explosion_orient.make_identity();
            face_surface = false;
        }
    }
    if (face_surface)
    {
        explosion_orient.make(*surface_normal);
        // Magic number: pi/2.
        explosion_orient.rotate_around_local_x(1.5707964f);
    }
    // Magic number: 2 pi.
    explosion_orient.rotate_around_local_y(fl_rand_range(0.0f, 6.2831855f));
    object* owner = World.object_from_handle(owner_handle);
    if (attacker == nullptr)
    {
        attacker = owner;
    }
    object* source = this;
    if (Multiplayer)
    {
        player* owner_player = owner != nullptr && owner->get_obj_type() == OT_HUMAN && owner->get_sub_type() == OT_SUB_HUMAN_PLAYER ? (player*)owner : nullptr;
        // The human at the root of the attacker (an attacker item's parents).
        object* root = attacker;
        while (root != nullptr && root->get_obj_type() != OT_HUMAN)
        {
            root = World.object_from_handle(root->get_parent());
        }
        if (owner_player != nullptr && root != owner_player)
        {
            source = owner_player;
        }
    }
    const explosion_info* e_info;
    if (w_info->weapon_class == WEAPON_CLASS_NANO_RIFLE)
    {
        e_info = w_info->m_explosion_info;
        source = this;
    }
    else
    {
        if (Multiplayer == true && multi_is_server())
        {
            player* stuck_to = World.object_from_handle<player>(get_parent());
            if (stuck_to != nullptr)
            {
                human* killer = attacker != nullptr && attacker->get_obj_type() == OT_HUMAN ? (human*)attacker : nullptr;
                // Magic number: a charge stuck to a player kills.
                human_damage(stuck_to, killer, 100000.0f, DBT_EXPLOSION, multi_gw_get_custom_weapon_info(MCW_CHARGE_STICK), attacker, true, false);
            }
        }
        e_info = w_info->m_explosion_info;
        if (w_info->m_ai_explosion_info != nullptr && !object_is_player(attacker))
        {
            e_info = w_info->m_ai_explosion_info;
        }
    }
    explosion_create(e_info, source, attacker, *detonate_pos, explosion_orient, &up, w_info, false);
    if (attacker != nullptr && attacker->get_obj_type() == OT_HUMAN && attacker->get_sub_type() == OT_SUB_HUMAN_PLAYER)
    {
        gameplay_register_player_attack((player*)attacker, w_info, DBT_EXPLOSION, false);
    }
}

// 0x00AD8030 (:3085): a "detonate on explosion impact" projectile goes off when an explosion hits it.
void projectile::process_explosion_hit(const vector* linear_impulse, const vector* angular_impulse, human* attacker)
{
    if (w_info != nullptr && (w_info->projectile_info.flags & PIF_DETONATE_ON_EXPLOSION_IMPACT) != 0)
    {
        detonate(nullptr, nullptr, attacker);
    }
}

// 0x00AD8060 (:3102): a shot sets the projectile off (single player: not in flight; multiplayer: the server only),
// unless it is a free attaching (or air bomb) projectile or stuck to the shooter.
void projectile::process_bullet_hit(human* attacker, float damage, const weapon_fire_info& fire_info)
{
    if (Multiplayer)
    {
        if (!multi_is_server())
        {
            return;
        }
    }
    else if (pflags.ballistic)
    {
        return;
    }
    if ((is_attached() && (attacker == nullptr || get_parent() != attacker->get_handle())) || (w_info->projectile_info.flags & (PIF_ATTACH_ON_IMPACT | PIF_ATTACH_TO_GROUND | PIF_AIR_BOMB)) == 0)
    {
        detonate(nullptr, nullptr, attacker);
    }
}

// 0x00AD80D0 (:3126): the sledgehammer sets a projectile off; other melee hits pass through.
bool projectile::process_melee_hit(object* attacker, float damage, const vector& impulse, const vector& hit_pos, const vector& hit_normal, unsigned int shape_key, const weapon_info& melee_w_info)
{
    if (melee_w_info.weapon_class == WEAPON_CLASS_SLEDGEHAMMER)
    {
        detonate(nullptr, nullptr, attacker);
        return false;
    }
    return true;
}

// 0x00A1EA20 (:3157): whether a mover stops a shattering projectile: one faster than 4.5 m/s shatters glass (a
// general mover of a breakable material, an rfg mover's shatterable shape of a mover that is not invulnerable) and goes on.
bool projectile_object_mover_impact_stops_projectile(const projectile* proj, object_mover* target, int shape_key, bool& gmover_shatter, bool& rmover_shatter)
{
    gmover_shatter = false;
    rmover_shatter = false;
    if (!proj->w_info->flags.shatter)
    {
        return true;
    }
    vector velocity;
    havok_body_get_linear_velocity(proj->havok_handle, velocity);
    // Magic number: 20.25 = 4.5 m/s squared.
    if (velocity.x * velocity.x + velocity.y * velocity.y + velocity.z * velocity.z < 20.25f)
    {
        return true;
    }
    if (target->get_sub_type() == OT_SUB_MOVER_GENERAL)
    {
        const rfg_physical_material* material = rfg_get_physical_material_by_index(target->get_physical_material(0xFFFFFFFF, nullptr, nullptr));
        if ((material->flags & 1) != 0)
        {
            gmover_shatter = true;
            return false;
        }
    }
    else if (target->get_sub_type() == OT_SUB_MOVER_RFG)
    {
        if (((rfg_mover*)target)->is_shatterable(shape_key) && !((rfg_mover*)target)->flags.invulnerable)
        {
            rmover_shatter = true;
            return false;
        }
    }
    return true;
}

// 0x00A1EB10 (:3210): a one-contact collision event (point velocities when both bodies are known).
void projectile_collide_fill_out_collision_event(collision_event* c_event, object* obj_a, object* obj_b, const vector& collision_pos, const vector& collision_normal, int shape_key_a, int shape_key_b,
                                                 const hkpRigidBody* rigid_body_a, const hkpRigidBody* rigid_body_b)
{
    c_event->m_num_collisions = 0;
    c_event->m_object_a = obj_a;
    c_event->m_object_b = obj_b;
    collision_event::collision_info info;
    info.m_collision_point.zero();
    info.m_collision_normal.zero();
    info.m_relative_point_velocity.zero();
    info.m_a_point_velocity.zero();
    info.m_b_point_velocity.zero();
    info.m_collision_normal = collision_normal;
    info.m_relative_normal_velocity = 0.0f;
    info.m_collision_point = collision_pos;
    info.m_shapekey_a = shape_key_a;
    info.m_hk_rigid_body_a = (hkpRigidBody*)rigid_body_a;
    info.m_hk_rigid_body_b = (hkpRigidBody*)rigid_body_b;
    info.m_shapekey_b = shape_key_b;
    if (rigid_body_a != nullptr && rigid_body_b != nullptr)
    {
        hkVector4f havok_pos;
        game_to_havok_vector(havok_pos, collision_pos);
        info.setup_point_velocities(&havok_pos);
    }
    // Magic number: 4 contacts at most.
    if (c_event->m_num_collisions < 4)
    {
        c_event->m_collisions[c_event->m_num_collisions] = info;
        c_event->m_num_collisions++;
    }
}

// 0x00A95820 (:3237): the multiplayer flight's collision between two positions: the first hit (not the owner) that is
// a valid target and stops the projectile (glass shatters on the way); a human counts where the line meets its hit
// capsules.
bool projectile::get_mp_collision_info(const vector& start_posp, const vector& end_pos, object** collide_obj, collision_event& c_event)
{
    *collide_obj = nullptr;
    c_event.m_num_collisions = 0;
    c_event.m_object_a = this;
    raycast_input ray_in;
    ray_in.start = start_posp;
    ray_in.end = end_pos;
    ray_in.radius = 0.0f;
    ray_in.filter_callback = nullptr;
    ray_in.m_flags = 0;
    // Magic number: collision filter 0x10.
    ray_in.m_filter_info = 0x10;
    ray_in.m_hit_collector_type = raycast_input::COLLECT_ALL_HITS;
    ray_in.filter_params = this;
    raycast_output ray_out;
    object* owner = World.object_from_handle(owner_handle);
    if (!havok_raycast(ray_in, ray_out))
    {
        return false;
    }
    for (int i = 0; i < ray_out.m_hits.getSize(); i++)
    {
        const raycast_output::hit_info& hit = ray_out.m_hits[i];
        vector hit_pos = hit.hit_pos;
        vector hit_normal = hit.hit_normal;
        if (hit.hit_object_handle == owner_handle)
        {
            continue;
        }
        object* objp = World.object_from_handle(hit.hit_object_handle);
        *collide_obj = objp;
        // Original: objp is used unchecked.
        if (!objp->remote_obj_flags.remote_object && !(objp->get_obj_type() == OT_MOVER && objp->get_sub_type() == OT_SUB_MOVER_RFG) && objp->get_obj_type() != OT_ZONE)
        {
            continue;
        }
        if (objp->get_obj_type() != OT_HUMAN)
        {
            if (objp->get_obj_type() == OT_MOVER)
            {
                bool gmover_shatter;
                bool rmover_shatter;
                const bool stops = projectile_object_mover_impact_stops_projectile(this, (object_mover*)objp, hit.hit_havok_shape_key, gmover_shatter, rmover_shatter);
                if (rmover_shatter)
                {
                    vector local_pos;
                    vector local_dir;
                    convert_point_direction_from_world_to_local(local_pos, local_dir, hit_pos, hit_normal, objp->pos, objp->orient);
                    rfg_dyn_add_pre_collide_shatter((rfg_mover*)objp, owner, hit.hit_havok_shape_key, 0, local_pos, local_dir, false);
                }
                if (!stops)
                {
                    continue;
                }
            }
            projectile_collide_fill_out_collision_event(&c_event, this, objp, hit.hit_pos, hit.hit_normal, -1, hit.hit_havok_shape_key, havok_body_get_pointer(havok_handle),
                                                        havok_body_get_pointer(hit.hit_havok_handle));
            return true;
        }
        const vector dir = (end_pos - start_posp).copy_normalize_safe();
        int bone_index;
        if (human_get_hit_location((human*)objp, start_posp, dir, hit_pos, get_bounding_radius(), &bone_index, &hit_normal) != HIT_NONE && bone_index != -1)
        {
            projectile_collide_fill_out_collision_event(&c_event, this, objp, hit_pos, hit_normal, -1, bone_index, nullptr, nullptr);
            return true;
        }
    }
    return false;
}

// 0x00AD8100 (:3352): sticking to what was hit (not the Old Coot's chair): the exact surface from a ray along the
// flight (rfg movers and vehicles), glass shattered on the way, humans by hit location, a courier's vehicle damaged,
// walkers' parts parented.
void projectile::collision_attach(const collision_event* c_event, object* other_obj, float attach_offset, const matrix* attach_orient_override)
{
    if (is_attached())
    {
        return;
    }
    // Original: other_obj is used unchecked; a sub type 0 object is taken for a mover (its level properties' name).
    if (other_obj->get_sub_type() == OT_SUB_MOVER_GENERAL && v_stricmp(((object_mover*)other_obj)->m_level_obj_properties->m_base_properties->m_name, "Old_Coot_Chair") == 0)
    {
        return;
    }
    unsigned int body_handle = 0xFFFFFFFF;
    unsigned int shape_key = 0xFFFFFFFF;
    bool self_is_a = true;
    vector flight_dir;
    if (c_event->m_object_a == this)
    {
        flight_dir = c_event->m_collisions[0].m_a_point_velocity.copy_normalize();
        if (c_event->m_collisions[0].m_hk_rigid_body_b != nullptr)
        {
            body_handle = havok_rigid_body_get_handle(c_event->m_collisions[0].m_hk_rigid_body_b);
            shape_key = c_event->m_collisions[0].m_shapekey_b;
        }
    }
    else
    {
        flight_dir = c_event->m_collisions[0].m_b_point_velocity.copy_normalize();
        if (c_event->m_collisions[0].m_hk_rigid_body_a != nullptr)
        {
            body_handle = havok_rigid_body_get_handle(c_event->m_collisions[0].m_hk_rigid_body_a);
            shape_key = c_event->m_collisions[0].m_shapekey_a;
        }
        self_is_a = false;
    }
    int alt_body_index = -1;
    const int alt_index = other_obj->get_alt_hk_body_handle_index(body_handle);
    if (alt_index != 0)
    {
        alt_body_index = alt_index;
    }
    // Magic number: an "attach to ground" projectile only sticks to surfaces within about 52 degrees of level.
    if ((w_info->projectile_info.flags & PIF_ATTACH_TO_GROUND) != 0 && !(0.607f < c_event->m_collisions[0].m_collision_normal.y || c_event->m_collisions[0].m_collision_normal.y == 0.607f))
    {
        return;
    }
    vector surface_normal = c_event->m_collisions[0].m_collision_normal;
    vector surface_pos = c_event->m_collisions[0].m_collision_point;
    int attach_index = (int)shape_key;
    if (other_obj->get_sub_type() == OT_SUB_MOVER_RFG || other_obj->get_obj_type() == OT_VEHICLE)
    {
        raycast_input ray_in;
        ray_in.radius = 0.0f;
        ray_in.start = pos - flight_dir;
        ray_in.m_filter_info = COLLIDE_RAY_DEFAULT;
        ray_in.m_hit_collector_type = raycast_input::COLLECT_CLOSEST_HIT;
        ray_in.filter_callback = projectile_filter_callback;
        // Weapon flag 8 doubles into ray flag 0x10.
        ray_in.m_flags = (*(const unsigned char*)&w_info->flags & 8) * 2;
        ray_in.filter_params = other_obj;
        raycast_output ray_out;
        for (unsigned int i = 0; i < c_event->m_num_collisions; i++)
        {
            ray_in.end = c_event->m_collisions[i].m_collision_point + flight_dir;
            if (havok_raycast(ray_in, ray_out))
            {
                surface_pos = ray_out.m_hits[0].hit_pos;
                surface_normal = ray_out.m_hits[0].hit_normal;
                attach_index = (int)(short)ray_out.m_hits[0].hit_havok_shape_key;
                break;
            }
        }
    }
    if (other_obj->get_sub_type() == OT_SUB_MOVER_RFG && (w_info->projectile_info.flags & PIF_ATTACH_ON_IMPACT) == 0)
    {
        rfg_mover* rmp = World.object_cast<rfg_mover>(other_obj);
        for (unsigned int i = 0; i < c_event->m_num_collisions; i++)
        {
            const unsigned int key = self_is_a ? c_event->m_collisions[i].m_shapekey_b : c_event->m_collisions[i].m_shapekey_a;
            if (rmp->is_shatterable(key))
            {
                object* owner = World.object_from_handle(owner_handle);
                vector local_pos;
                vector local_dir;
                convert_point_direction_from_world_to_local(local_pos, local_dir, surface_pos, surface_normal, other_obj->pos, other_obj->orient);
                rfg_dyn_add_pre_collide_shatter(rmp, owner, key, 0, local_pos, local_dir, false);
            }
        }
    }
    if (other_obj->get_obj_type() == OT_HUMAN)
    {
        int bone_index;
        human_get_hit_location((human*)other_obj, pos, flight_dir, surface_pos, get_bounding_radius(), &bone_index, &surface_normal);
        attach_to_human((human*)other_obj, surface_pos, surface_normal, bone_index);
        return;
    }
    if (other_obj->get_obj_type() == OT_VEHICLE)
    {
        npc* driver = (npc*)vehicle_passenger_in_seat((vehicle*)other_obj, VSI_DRIVER);
        if (driver != nullptr && driver->get_sub_type() == OT_SUB_HUMAN_NPC && activity_cc_is_courier(driver))
        {
            // Magic number: a token 0.01 damage tells the courier's vehicle it was hit.
            vehicle_apply_damage((vehicle*)other_obj, World.object_from_handle(owner_handle), w_info, DBT_BULLET, &surface_pos, &surface_normal, 0.01f, 1, nullptr, false);
        }
    }
    attach(other_obj, attach_index, surface_normal, surface_pos, alt_body_index, attach_offset, attach_orient_override);
    if (other_obj->get_sub_type() == OT_SUB_VEHICLE_WALKER)
    {
        // A walker's part (alt body) becomes the parent tag (component index | 0x100 in the high word).
        int parent_prop_point = -1;
        walker* wp = World.object_cast<walker>(other_obj);
        if (wp != nullptr && alt_body_index > -1)
        {
            const unsigned int component = wp->get_component_idx_from_alt_body_index(alt_body_index);
            if (component != 0xFFFFFFFF)
            {
                parent_prop_point = (int)((component | 0x100) << 16);
            }
        }
        other_obj->set_child(this, parent_prop_point, -1, true, false);
    }
}

// 0x00AAF750 (:3507): stuck where it hit: turrets near a charge or mine are not reused for a while; the projectile is
// placed against the surface (its attach point on it, minus the offset), and a prismatic constraint with zero travel
// holds it to the host. Attached explosives keep colliding (projectile attached), other projectiles stop.
void projectile::attach(object* other_obj, int attach_index, const vector& surface_normal, const vector& surface_pos, int host_alt_body_index, float attach_offset, const matrix* attach_orient_override)
{
    pflags.ballistic = 0;
    if (w_info->weapon_class == WEAPON_CLASS_THROWN_CHARGE && attach_point_index != -1)
    {
        update_animlib(0.0f, true);
    }
    const weapon_class_type wc = w_info->weapon_class;
    if (wc == WEAPON_CLASS_THROWN_CHARGE || wc == WEAPON_CLASS_THROWN_MINE)
    {
        const int num_turrets = World.get_num_objects<turret>();
        for (int i = 0; i < num_turrets; i++)
        {
            turret* tp = World.get_object<turret>(i);
            // Magic number: within 3 m.
            if (vec_dist_squared(pos, tp->pos) < 9.0f)
            {
                tp->disable_for_npcs_time.set(Ai_reuse_turret_after_nearby_explosive * 1000);
            }
        }
    }
    if (other_obj == nullptr)
    {
        keen::debug::nativePrintArgs("[projectile.cpp] <attach> other_obj is nullpointer! \n");
        return;
    }
    const vector into_surface(-surface_normal.x, -surface_normal.y, -surface_normal.z);
    pflags.is_attached_backwards = 1;
    matrix attach_orient;
    if (attach_orient_override == nullptr)
    {
        attach_orient.make_quick(into_surface.copy_normalize_safe());
    }
    else
    {
        attach_orient = *attach_orient_override;
    }
    vector attach_pos = surface_pos;
    if (attach_point_index != -1)
    {
        vector tag_pos;
        get_tag_local_coords(attach_point_index, tag_pos, nullptr);
        tag_pos.unrotate(attach_orient);
        attach_pos = attach_pos - tag_pos;
    }
    // Magic number: offsets under 1 mm are ignored.
    if (0.001f <= attach_offset - 0.0f || 0.001f <= -(attach_offset - 0.0f))
    {
        attach_pos = attach_pos - into_surface * attach_offset;
    }
    havok_body_set_linear_velocity(havok_handle, ZERO_VECTOR);
    update_pos_and_orient(attach_pos, attach_orient, true);
    constraint_template ct;
    ct.body_b_transform.m_translation = attach_pos - other_obj->pos;
    ct.type = HAVOK_CONSTRAINT_PRISMATIC;
    ct.body_b_transform.m_translation.rotate(other_obj->orient);
    // No travel, no friction.
    ct.prismatic_data.limit_min = 0.0f;
    ct.prismatic_data.limit_max = 0.0f;
    ct.prismatic_data.limit_friction = -1.0f;
    obj_constraint_props props(&ct, other_obj->get_handle(), get_handle(), attach_pos, attach_orient, attach_index, attach_point_index, host_alt_body_index, -1);
    // Original: the constraint is used unchecked.
    constraint* cp = World.create_object<constraint>(props, 0xFFFFFFFF);
    constraint_handle = cp->get_handle();
    if (Multiplayer == true && !multi_is_server())
    {
        client_prediction_net_time = multi_core_clock_get_time() + Client_predict_settle_time;
    }
    havok_body_set_collision_type(havok_handle, w_info->is_explosive() ? COLLIDE_PROJECTILE_ATTACHED : COLLIDE_NO_COLLISION, true, false);
    hkpRigidBody* rb = havok_body_get_pointer(havok_handle);
    if (havok_has_ignore_property(rb))
    {
        havok_remove_ignore_property(rb);
    }
    attach_common();
}

// 0x00ACDDC0 (:3604): stuck to a human's bone (not its owner; in single player not to invulnerable humans or ones
// that refuse projectiles): facing into the hit, child of the bone. An npc notices it; a nano rifle bolt in an
// npc's bone ragdolls the npc instead.
void projectile::attach_to_human(human* target, const vector& hit_pos, const vector& hit_normal, int hit_bone_index)
{
    if (hit_bone_index == -1)
    {
        return;
    }
    if (!Multiplayer && !wrecking_crew_is_active() && (target->h_info->flags.no_attaching_projectiles || target->is_invulnerable()))
    {
        return;
    }
    if (owner_handle == target->get_handle())
    {
        return;
    }
    pflags.ballistic = 0;
    matrix attach_orient(orient);
    if (hit_normal.is_valid() && !hit_normal.is_null())
    {
        const vector n = hit_normal.copy_normalize_safe();
        attach_orient.make_quick(vector(-n.x, -n.y, -n.z));
    }
    vector attach_pos = hit_pos;
    if (attach_point_index != -1)
    {
        vector tag_pos;
        get_tag_local_coords(attach_point_index, tag_pos, nullptr);
        tag_pos.unrotate(attach_orient);
        attach_pos = attach_pos - tag_pos;
    }
    pos = attach_pos;
    orient = attach_orient;
    target->set_child(this, hit_bone_index, -1, true, true);
    if (target->get_obj_type() == OT_HUMAN && target->get_sub_type() == OT_SUB_HUMAN_NPC)
    {
        if (hit_bone_index < 0 || w_info->weapon_class != WEAPON_CLASS_NANO_RIFLE)
        {
            if (!target->is_killed())
            {
                ((npc*)target)->get_ai()->perception.register_projectile_attached(this);
            }
        }
        else
        {
            human_ragdoll_start(target, false, 0xFFFFFFFF);
            // Magic number: a 20 impulse into the surface.
            const vector impulse(-hit_normal.x * 20.0f, -hit_normal.y * 20.0f, -hit_normal.z * 20.0f);
            havok_ragdoll_apply_impulse_to_single_bone(target->ragdoll_idx, impulse, hit_pos);
        }
    }
    havok_body_set_linear_velocity(havok_handle, ZERO_VECTOR);
    havok_body_set_collision_type(havok_handle, COLLIDE_PROJECTILE_ATTACHED, true, false);
    if (Multiplayer == true && !multi_is_server())
    {
        client_prediction_net_time = multi_core_clock_get_time() + Client_predict_settle_time;
    }
    attach_common();
}

// 0x00A95BA0 (:3674): the effects of sticking, no more gravity; a placed charge or mine is heard by npcs and the
// player gets the charge tool tips.
void projectile::attach_common()
{
    handle_collision_attach_effects();
    set_acceleration(ZERO_VECTOR);
    const weapon_class_type wc = w_info->weapon_class;
    if ((wc == WEAPON_CLASS_THROWN_CHARGE || wc == WEAPON_CLASS_THROWN_MINE) && owner_handle != 0xFFFFFFFF)
    {
        human* thrower = World.object_from_handle<human>(owner_handle);
        if (thrower != nullptr)
        {
            npc_sound_info_charge_placed sound(this, thrower);
            npc_sound_info::queue<npc_sound_info_charge_placed>(sound);
            maybe_play_remote_charge_help_text(thrower, w_info);
        }
    }
}

// 0x00A95CF0 (:3694): loose again: off its parent (a physics body again; after a human no more humans), colliding
// as a projectile with gravity. A singularity bomb floats without colliding.
void projectile::detach()
{
    if (obj_flags.destroyed)
    {
        return;
    }
    constraint_handle = 0xFFFFFFFF;
    object* parent_obj = World.object_from_handle(get_parent());
    if (parent_obj != nullptr)
    {
        parent_obj->remove_child(this);
        if (havok_handle == 0xFFFFFFFF)
        {
            create_physics_object(this);
        }
        if (parent_obj->get_obj_type() == OT_HUMAN)
        {
            pflags.no_attach_to_humans = 1;
            pflags.release_havok_object = 0;
        }
    }
    if (w_info->weapon_class != WEAPON_CLASS_SINGULARITY_BOMB)
    {
        havok_body_force_activate(havok_handle);
        havok_body_set_collision_type(havok_handle, COLLIDE_PROJECTILE, true, false);
        const vector gravity(0.0f, -9.8f, 0.0f);
        set_acceleration(gravity);
        return;
    }
    set_acceleration(ZERO_VECTOR);
    havok_body_set_collision_type(havok_handle, COLLIDE_NO_COLLISION, true, false);
}

// 0x00A72090 (:3742): the flight effects stop; a charge folds its legs (state 0x66); the blinking light starts
// (friendly or enemy colour in multiplayer).
void projectile::handle_collision_attach_effects()
{
    audiolib_play_stop(sound_info);
    projectile_stop_effect(effect_obj_handle);
    projectile_stop_effect(effect_obj_handle2);
    projectile_stop_effect(effect_obj_handle3);
    if (w_info->weapon_class == WEAPON_CLASS_THROWN_CHARGE && char_inst != nullptr)
    {
        // Magic number: a 0.12 s blend.
        char_inst->set_state(index_alias(0x66, (vanim_aliases)-1), index_alias(-1, (vanim_aliases)-1), 0.12f, false);
    }
    if (blinking_light_point_index == -1)
    {
        return;
    }
    vector light_pos;
    matrix light_orient;
    get_tag_world_coords(blinking_light_point_index, light_pos, &light_orient, &pos, &orient);
    const player* local_player = Player;
    const human* owner = owner_handle != 0xFFFFFFFF ? World.object_from_handle<human>(owner_handle) : nullptr;
    const unsigned int friendly_effect = w_info->projectile_info.effect3;
    if (Multiplayer && local_player != nullptr && owner != nullptr && human_teams_are_friendly(local_player, owner) && friendly_effect != 0xFFFFFFFF)
    {
        // Original: the friendly light uses the projectile's orientation, the other one the tag's.
        effect_obj_handle3 = effects_play(friendly_effect, light_pos, orient, true, get_handle(), blinking_light_point_index, 0xFFFFFFFF, 1.0f, false, EFFECTS_CATEGORY_NONE);
        return;
    }
    effect_obj_handle2 = effects_play(w_info->projectile_info.effect2, light_pos, light_orient, true, get_handle(), blinking_light_point_index, 0xFFFFFFFF, 1.0f, false, EFFECTS_CATEGORY_NONE);
}

// 0x00A1EC30 (:3774): pushed off the surface; a grenade's fuse starts on its first bounce (single player or the
// server).
void projectile::collision_bounce(const collision_event* c_event, object* other_obj)
{
    havok_body_apply_linear_velocity(havok_handle, c_event->m_collisions[0].m_collision_normal);
    if ((!Multiplayer || multi_is_server()) && !detonate_stamp.valid() && w_info->weapon_class == WEAPON_CLASS_THROWN_GRENADE)
    {
        detonate_stamp.set((int)(unsigned int)w_info->projectile_info.fuse_time);
    }
}

// 0x00A1EC90 (:3789): a blade hitting a riot shield sparks and is reflected off its face (and moved out in front of
// it).
void projectile::collision_shield_deflect(const collision_event* c_event, item* shield_item)
{
    if (c_event == nullptr || c_event->m_num_collisions == 0 || (w_info->weapon_class != WEAPON_CLASS_GRINDER && w_info->weapon_class != WEAPON_CLASS_HARPOON))
    {
        return;
    }
    vector velocity;
    havok_body_get_linear_velocity(havok_handle, velocity);
    const vector hit_pos = c_event->m_collisions[0].m_collision_point;
    const vector dir = velocity.copy_normalize_safe();
    const unsigned char material = shield_item->get_effect_material(0xFFFFFFFF, nullptr, nullptr);
    const vector spark_dir(-dir.x, -dir.y, -dir.z);
    bullet_spark_create(hit_pos, spark_dir, w_info, material, MATERIAL_EVENT_SHAPE_SHEET, shield_item, 0xFFFFFFFF, -1.0f);
    // Item flag 0x2000: the shield's face is its uvec, not its fvec.
    const vector face = ((shield_item->info->flags >> 13) & 1) == 0 ? shield_item->orient.fvec.copy_normalize_safe() : shield_item->orient.uvec.copy_normalize_safe();
    float dot = velocity.y * face.y + face.x * velocity.x + velocity.z * face.z;
    if (dot >= 0.0f)
    {
        return;
    }
    // Magic number: reflected with 1.4 times the normal component (0.4 restitution).
    dot = dot * 1.4f;
    velocity = velocity - face * dot;
    havok_body_set_linear_velocity(havok_handle, velocity);
    vector new_pos = pos;
    float depth = (new_pos.y - shield_item->pos.y) * face.y + (new_pos.x - shield_item->pos.x) * face.x + (new_pos.z - shield_item->pos.z) * face.z;
    if (depth < 0.0f)
    {
        depth = depth * 2.0f;
        new_pos = new_pos - face * depth;
        update_pos_and_orient(new_pos, orient, true);
    }
}

// 0x00AD8650 (:3842): goes off where the flight meets the hit object (a client hides its copy and waits for the
// server's explosion).
void projectile::collision_detonate(const collision_event* c_event, object* other_obj)
{
    if (Multiplayer && !multi_is_server())
    {
        object_set_visibility_plus_children(this, false);
        havok_body_set_collision_type(havok_handle, COLLIDE_NO_COLLISION, true, false);
        projectile_stop_effect(effect_obj_handle);
        projectile_stop_effect(effect_obj_handle2);
        projectile_stop_effect(effect_obj_handle3);
        projectile_stop_effect(effect_obj_handle4);
        client_prediction_net_time = multi_core_clock_get_time();
        return;
    }
    vector flight_dir;
    if (c_event->m_object_a == this)
    {
        flight_dir = c_event->m_collisions[0].m_a_point_velocity.copy_normalize();
        // Original: the other body's handle is read and not used.
        if (c_event->m_collisions[0].m_hk_rigid_body_b != nullptr)
        {
            havok_rigid_body_get_handle(c_event->m_collisions[0].m_hk_rigid_body_b);
        }
    }
    else
    {
        flight_dir = c_event->m_collisions[0].m_b_point_velocity.copy_normalize();
        if (c_event->m_collisions[0].m_hk_rigid_body_a != nullptr)
        {
            havok_rigid_body_get_handle(c_event->m_collisions[0].m_hk_rigid_body_a);
        }
    }
    raycast_input ray_in;
    ray_in.radius = 0.0f;
    ray_in.start = pos - flight_dir;
    vector hit_normal = c_event->m_collisions[0].m_collision_normal;
    vector hit_pos = c_event->m_collisions[0].m_collision_point;
    ray_in.m_filter_info = COLLIDE_RAY_DEFAULT;
    ray_in.m_hit_collector_type = raycast_input::COLLECT_CLOSEST_HIT;
    ray_in.m_flags = 0;
    ray_in.filter_callback = projectile_filter_callback;
    raycast_output ray_out;
    if (other_obj == nullptr)
    {
        return;
    }
    ray_in.filter_params = other_obj;
    for (unsigned int i = 0; i < c_event->m_num_collisions; i++)
    {
        ray_in.end = c_event->m_collisions[i].m_collision_point + flight_dir;
        if (havok_raycast(ray_in, ray_out))
        {
            hit_pos = ray_out.m_hits[0].hit_pos;
            hit_normal = ray_out.m_hits[0].hit_normal;
            break;
        }
    }
    detonate(&hit_pos, &hit_normal, nullptr);
}

// 0x00AD89D0 (:3917): a "damage on impact" projectile hits like a bullet (the damage by the distance flown) and goes
// off. Without a collision event the hit is tested along a frame of flight at max speed.
void projectile::collision_damage(const collision_event* c_event, object* other_obj)
{
    weapon_fire_info fire_info;
    fire_info.w_info = w_info;
    fire_info.hit_handle = 0xFFFFFFFF;
    fire_info.hit_bone = 0;
    fire_info.hit_location = HIT_NONE;
    human* owner = owner_handle != 0xFFFFFFFF ? World.object_from_handle<human>(owner_handle) : nullptr;
    if (c_event == nullptr)
    {
        if (other_obj == nullptr || other_obj->get_obj_type() != OT_HUMAN || (human*)other_obj == owner)
        {
            return;
        }
        const float step = w_info->projectile_info.max_speed;
        fire_info.start_pos = pos;
        fire_info.end_pos.z = pos.z + orient.fvec.z * step * Frametime;
        fire_info.end_pos.y = pos.y + orient.fvec.y * step * Frametime;
        fire_info.end_pos.x = orient.fvec.x * step * Frametime + fire_info.start_pos.x;
        // Original: the owner is used unchecked.
        weapons_test_bullet(fire_info, owner, false, 0.0f, ZERO_VECTOR);
        other_obj = World.object_from_handle(fire_info.hit_handle);
    }
    else
    {
        object* hit;
        if (c_event->m_object_a == this && c_event->m_object_b != nullptr)
        {
            fire_info.hit_shape_key = c_event->m_collisions[0].m_shapekey_b;
            fire_info.hit_effect_material = c_event->m_object_b->get_effect_material(fire_info.hit_shape_key, &c_event->m_collisions[0].m_collision_point, nullptr);
            hit = c_event->m_object_b;
        }
        else
        {
            if (c_event->m_object_a == nullptr)
            {
                return;
            }
            fire_info.hit_shape_key = c_event->m_collisions[0].m_shapekey_a;
            fire_info.hit_effect_material = c_event->m_object_a->get_effect_material(fire_info.hit_shape_key, &c_event->m_collisions[0].m_collision_point, nullptr);
            hit = c_event->m_object_a;
        }
        fire_info.hit_physical_material = hit->get_physical_material(fire_info.hit_shape_key, &c_event->m_collisions[0].m_collision_point, nullptr);
        fire_info.hit_pos = c_event->m_collisions[0].m_collision_point;
        fire_info.hit_normal = c_event->m_collisions[0].m_collision_normal;
    }
    if (other_obj == nullptr)
    {
        return;
    }
    const vector flown = start_pos - pos;
    const float damage = weapons_get_bullet_damage(w_info, other_obj, sqrtf(flown.x * flown.x + flown.y * flown.y + flown.z * flown.z));
    other_obj->process_bullet_hit(owner, damage, fire_info);
    object_decal_create_bullet_impact_decal(other_obj, owner, w_info, fire_info.hit_pos, fire_info.hit_normal, damage, &fire_info);
    if (w_info->projectile_info.damage_effect != 0xFFFFFFFF)
    {
        effects_play(w_info->projectile_info.damage_effect, pos, orient, false, 0xFFFFFFFF, -1, 0xFFFFFFFF, 1.0f, false, EFFECTS_CATEGORY_NONE);
    }
    detonate(nullptr, nullptr, nullptr);
}

// 0x00A1EFE0 (:3988): where the launch would be after time_sec of ballistic flight.
void projectile::get_ballistic_position_velocity_time(vector& ballistic_position, vector& ballistic_linear_velocity, float time_sec)
{
    ballistic_position.x = start_pos.x + start_velocity.x * time_sec + start_accel.x * 0.5f * time_sec * time_sec;
    ballistic_position.y = start_pos.y + start_velocity.y * time_sec + start_accel.y * 0.5f * time_sec * time_sec;
    ballistic_position.z = start_pos.z + start_velocity.z * time_sec + start_accel.z * 0.5f * time_sec * time_sec;
    ballistic_linear_velocity.x = start_velocity.x + time_sec * start_accel.x;
    ballistic_linear_velocity.y = start_velocity.y + start_accel.y * time_sec;
    ballistic_linear_velocity.z = start_velocity.z + start_accel.z * time_sec;
}

// 0x00A41590 (:3997): the same at the projectile's age (at most 7 s).
void projectile::get_ballistic_position_velocity(vector& ballistic_position, vector& ballistic_linear_velocity)
{
    int age_ms = age_timer.time_since();
    // Magic number: 7 s, the multiplayer ballistic lifetime.
    if (age_ms > 6999)
    {
        age_ms = 7000;
    }
    const float t = (float)age_ms * 0.001f;
    ballistic_position.x = start_pos.x + start_velocity.x * t + start_accel.x * 0.5f * t * t;
    ballistic_position.y = start_pos.y + t * start_velocity.y + start_accel.y * 0.5f * t * t;
    ballistic_position.z = start_pos.z + t * start_velocity.z + start_accel.z * 0.5f * t * t;
    ballistic_linear_velocity.x = start_velocity.x + start_accel.x * t;
    ballistic_linear_velocity.y = t * start_accel.y + start_velocity.y;
    ballistic_linear_velocity.z = t * start_accel.z + start_velocity.z;
}

// 0x00A1F220 (:4005): back to the launch's gravity and ignored object.
void projectile::restart_ballistic_props()
{
    if (havok_handle == 0xFFFFFFFF)
    {
        return;
    }
    const vector delta = ballistic_accel - current_accel;
    if (1.0000001e-06f < delta.y * delta.y + delta.x * delta.x + delta.z * delta.z)
    {
        havok_body_add_custom_force(havok_handle, delta);
    }
    current_accel = ballistic_accel;
    hkpRigidBody* rb = havok_body_get_pointer(havok_handle);
    if (rb != nullptr)
    {
        if (havok_has_ignore_property(rb))
        {
            havok_remove_ignore_property(rb);
        }
        havok_set_ignore_property(rb, ballistic_ignore_handle);
    }
    pflags.restart_ballistic = 0;
}

// 0x00A41830 (:4024)
void projectile::detach_effects(bool detach_effect1, bool detach_effect2, bool detach_effect3, bool detach_effect4)
{
    if (detach_effect1)
    {
        projectile_stop_effect(effect_obj_handle);
    }
    if (detach_effect2)
    {
        projectile_stop_effect(effect_obj_handle2);
    }
    if (detach_effect3)
    {
        projectile_stop_effect(effect_obj_handle3);
    }
    if (detach_effect4)
    {
        projectile_stop_effect(effect_obj_handle4);
    }
}

// 0x00A72290 (:4061): the fading opacity on the projectile's mesh (scene entity flag 0x20 while translucent).
void projectile::update_render(rl_renderer* pRenderer)
{
    if (char_inst != nullptr && char_inst->rdd.is_loaded() && char_inst->rdd.get_data()->renderable_instance_p != nullptr)
    {
        // Inlined set_opacity (translucent below 1).
        char_inst->rdd.get_data()->renderable_instance_p->set_opacity(opacity);
    }
    item::update_render(pRenderer);
}

// 0x00A1F340 (:4075)
void projectile::mp_restart_effect()
{
    effect_obj_handle = effects_play(w_info->projectile_info.effect1, pos, orient, true, get_handle(), -1, 0xFFFFFFFF, 1.0f, false, EFFECTS_CATEGORY_NONE);
}

// 0x00A722E0 (:4081): handle_collision_attach_effects for the replicated attach (a copy of it in rfg.exe).
void projectile::mp_handle_collision_attach_effects()
{
    handle_collision_attach_effects();
}

// ---- system (:4154)
// 0x00A1F390 (:4154)
void projectile_system_init()
{
}

// 0x00A1F3A0 (:4160)
void projectile_level_init_post()
{
    Charge_list = nullptr;
}

// Every projectile goes without a destroy packet (inlined in projectile_level_shutdown and projectile_destroy_all).
// Original: the remote flags are set on an uninitialized pointer (decompiled), so nothing is flagged here.
static void projectile_destroy_all_internal()
{
    const int num = World.get_num_objects<projectile>();
    for (int i = 0; i < num; i++)
    {
        World.destroy_object(World.get_object<projectile>(i), false);
    }
}

// 0x00A95DD0 (:4168)
void projectile_level_shutdown()
{
    projectile_destroy_all_internal();
}

// 0x00A722F0 (:4174): past Max_simultaneous_proxies mines the oldest other one goes (single player).
static int projectile_enforce_proxy_limit(projectile* proj, int proxy_count)
{
    if (proj->w_info->weapon_class != WEAPON_CLASS_THROWN_MINE)
    {
        return proxy_count;
    }
    if ((float)proxy_count < Max_simultaneous_proxies)
    {
        return proxy_count + 1;
    }
    int oldest_age = 0;
    unsigned int oldest = 0xFFFFFFFF;
    const int num = World.get_num_objects<projectile>();
    for (int i = 0; i < num; i++)
    {
        projectile* other = World.get_object<projectile>(i);
        if (other->w_info->weapon_class == WEAPON_CLASS_THROWN_MINE && other != proj && oldest_age < other->get_age())
        {
            oldest = other->get_handle();
            oldest_age = other->get_age();
        }
    }
    World.destroy_object(oldest, false);
    return proxy_count;
}

// 0x00A1F3B0 (:4210): the multiplayer server removes projectiles that flew more than 7 s (LTCG: proj in ESI).
static void projectile_mp_enforce_max_ballistic_lifetime(projectile* proj)
{
    // Magic number: 7 s.
    if (Multiplayer && multi_is_server() && proj->pflags.ballistic && proj->get_age() > 7000)
    {
        World.destroy_object(proj, false);
    }
}

// 0x00AE5C60 (:4226): once per frame after the simulation.
void projectile_process()
{
    projectile::recompute_safe_bp();
    if (Multiplayer)
    {
        multi_game_enforce_mp_projectile_limit();
    }
    int proxy_count = 0;
    const int num = World.get_num_objects<projectile>();
    for (int i = 0; i < num; i++)
    {
        projectile* proj = World.get_object<projectile>(i);
        if (!Multiplayer)
        {
            proxy_count = projectile_enforce_proxy_limit(proj, proxy_count);
        }
        else
        {
            // Inlined projectile_mp_enforce_max_ballistic_lifetime.
            if (Multiplayer == true)
            {
                projectile_mp_enforce_max_ballistic_lifetime(proj);
            }
            proxy_count++;
        }
        proj->process();
    }
}

// 0x00A723E0 (:4249)
projectile* projectile_find_by_create_id(unsigned int& create_id)
{
    if (create_id == 0xFFFFFFFF)
    {
        return nullptr;
    }
    const int num = World.get_num_objects<projectile>();
    for (int i = 0; i < num; i++)
    {
        projectile* proj = World.get_object<projectile>(i);
        if (proj->mp_create_id == create_id)
        {
            return proj;
        }
    }
    return nullptr;
}

// 0x00A41910 (:4266)
void validate_projectile_destruction(const object* obj)
{
}

// 0x00A72430 (:4277)
void projectile_destroy_all()
{
    projectile_destroy_all_internal();
}

// ---- not ported: the debug console functions, and singularity_bomb_vacuum (no references in rfg.exe: dead code)
#if 0
static int8_t Reconstructor_passes_dcf; // unused_dcf_reconstructor_passes (0x00A1E470) edits Reconstructor_passes
void unused_dcf_reconstructor_passes(); // 0x00A1E470 (:193)
void unused_dcf_remove_explosives(); // 0x00A718D0 (:237)
void singularity_bomb_vacuum(projectile* grenade, float dest_pos_x, float dest_pos_y, float dest_pos_z, float vacuum_multiplier); // 0x00AD69F0 (:641)
#endif
