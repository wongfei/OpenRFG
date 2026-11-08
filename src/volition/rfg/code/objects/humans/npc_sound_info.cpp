// Original: volition/rfg/code/objects/humans/npc_sound_info.cpp
//
// Sounds NPCs can hear (stub_impl.md S9). Linked by OpenRFG only; the test programs keep their stand-ins in
// tests/stubs/rfg/objects/objects.cpp.

#include "volition/rfg/code/objects/humans/npc_sound_info.h"

#include "volition/audio_lib/code/audiolib_api.h"
#include "volition/rfg/code/ai/ai_system.h"
#include "volition/rfg/code/code_effects/explosion_shared.h"
#include "volition/rfg/code/destruction/rfg_mover.h"
#include "volition/rfg/code/gameplay/activities/activity_house_arrest.h"
#include "volition/rfg/code/level/world.h"
#include "volition/rfg/code/objects/humans/human.h"
#include "volition/rfg/code/objects/humans/human_teams.h"
#include "volition/rfg/code/objects/humans/npc.h"
#include "volition/rfg/code/objects/humans/player.h"
#include "volition/rfg/code/objects/projectile.h"
#include "volition/rfg/code/vehicle/vehicle_class.h"
#include "volition/rfg/code/weapons/weapons.h"
#include "volition/vlib/code/math/vector3.h"
#include "volition/rfg/code/gamewide/tweak_table.h"

#include <math.h>

unsigned char npc_sound_info::queue_memory[10240]; // 0x030269D0
async_mempool npc_sound_info::queue_mempool(npc_sound_info::queue_memory, sizeof(npc_sound_info::queue_memory), "npc sound info pool", 4, 0); // 0x03043A80 (0x01075720)
slist_lock_free_stack npc_sound_info::queued_infos; // 0x016955D0

float Running_player_footstep_sound_dist; // 0x03024E58
static tweak_table_item Tweak_table_item_Running_player_footstep_sound_dist("Running_player_footstep_sound_dist", &Running_player_footstep_sound_dist); // 0x0304B0E4

// Magic numbers: each kind of sound's default carrying distance in metres, read from the fld immediates of rfg.exe.
static const float DEFAULT_SOUND_DISTANCE = 10.0f;
static const float PAIN_SOUND_DISTANCE = 15.0f;
static const float DEATH_SOUND_DISTANCE = 20.0f;
static const float SILENT_DEATH_SOUND_DISTANCE = 15.0f;
static const float BULLET_STRIKE_SOUND_DISTANCE = 12.0f;
static const float BUILDING_DAMAGE_SOUND_DISTANCE = 30.0f;
static const float VEHICLE_COLLISION_SOUND_DISTANCE = 30.0f;
static const float VEHICLE_DAMAGE_SOUND_DISTANCE = 15.0f;
static const float STRESS_SOUND_DISTANCE = 50.0f;
// A collapse carries 1.5 x the square root of the building's bbox volume, within 25 to 75 m; 30 m if the building is gone.
static const float COLLAPSE_VOLUME_SCALE = 1.5f;
static const float COLLAPSE_MIN_DISTANCE = 25.0f;
static const float COLLAPSE_MAX_DISTANCE = 75.0f;
static const float COLLAPSE_NO_BUILDING_DISTANCE = 30.0f;
// A silent kill closer than 3 m (squared) is heard as pain, further away as a noise.
static const float SILENT_DEATH_PAIN_DISTANCE_SQ = 9.0f;
// is_in_closest_building's tolerances for a listener of building damage.
static const float BUILDING_DAMAGE_TERRAIN_TOLERANCE = 1.25f;

static human* human_from_sound_handle(unsigned int handle)
{
    return handle != 0xFFFFFFFF ? World.object_from_handle<human>(handle) : nullptr;
}

static rfg_mover* rfg_mover_from_sound_handle(unsigned int handle)
{
    return handle != 0xFFFFFFFF ? World.object_from_handle<rfg_mover>(handle) : nullptr;
}

static projectile* projectile_from_sound_handle(unsigned int handle)
{
    return handle != 0xFFFFFFFF ? World.object_from_handle<projectile>(handle) : nullptr;
}

static vehicle* vehicle_from_sound_handle(unsigned int handle)
{
    return handle != 0xFFFFFFFF ? World.object_from_handle<vehicle>(handle) : nullptr;
}

// 0x00A0F950 (:85)
bool npc_sound_info::is_relevant_to_npc(npc* listener) const
{
    return listener->get_handle() != sound_source;
}

// 0x00A0F970 (:91)
bool npc_sound_info_pain::is_relevant_to_npc(npc* listener) const
{
    return listener->get_handle() != sound_source;
}

// 0x00A5B020 (:98)
bool npc_sound_info_building_damage::is_relevant_to_npc(npc* listener) const
{
    const human* attacker_hp = get_attacker();
    if (attacker_hp == nullptr || !human_teams_are_unfriendly(listener, attacker_hp))
    {
        return false;
    }
    if (listener->get_team() == HUMAN_TEAM_EDF)
    {
        return true;
    }
    // kept: the building is not checked for null before its team is read.
    const rfg_mover* building = get_building();
    if (human_teams_are_allies(listener, building->team) || activity_house_arrest_is_house(building))
    {
        return true;
    }
    ai_system* ai = listener->get_ai();
    if ((const object*)ai->get_defend_object() == (const object*)building)
    {
        return true;
    }
    rfg_mover* closest = nullptr;
    return ai->perception.is_in_closest_building(&closest, 0.0f, BUILDING_DAMAGE_TERRAIN_TOLERANCE) && closest == building;
}

// 0x00A5B120 (:138)
bool npc_sound_info_vehicle_damage::is_relevant_to_npc(npc* listener) const
{
    const human* attacker_hp = get_attacker();
    if (attacker_hp == nullptr || !human_teams_are_unfriendly(listener, attacker_hp))
    {
        return false;
    }
    const vehicle* vp = get_target();
    if (vp == nullptr)
    {
        return false;
    }
    if (human_teams_are_allies(listener, vp->info->default_team))
    {
        return true;
    }
    const human* driver_hp = get_driver();
    if (driver_hp != nullptr && human_teams_are_allies(listener, driver_hp))
    {
        return true;
    }
    return listener->vehicle_handle == vp->get_handle();
}

// 0x00A0F990 (:173)
bool npc_sound_info_unimportant_vehicle_damage::is_relevant_to_npc(npc* listener) const
{
    return listener->get_handle() != attacker;
}

// 0x00A33560 (:178)
bool npc_sound_info_footsteps::is_relevant_to_npc(npc* listener) const
{
    const player* pp = sound_source != 0xFFFFFFFF ? World.object_from_handle<player>(sound_source) : nullptr;
    return human_teams_are_unfriendly(listener, pp);
}

// 0x00A5B1D0 (:184)
bool npc_sound_info_bomb_set::is_relevant_to_npc(npc* listener) const
{
    const human* owner_hp = get_owner();
    return owner_hp != nullptr && human_teams_are_allies(owner_hp, listener);
}

// 0x00A0F9B0 (:193)
float npc_sound_info::get_default_sound_distance() const
{
    return DEFAULT_SOUND_DISTANCE;
}

// 0x00A0F9C0 (:198)
float npc_sound_info_pain::get_default_sound_distance() const
{
    return PAIN_SOUND_DISTANCE;
}

// 0x00A0F9D0 (:203)
float npc_sound_info_death::get_default_sound_distance() const
{
    return DEATH_SOUND_DISTANCE;
}

// 0x00A0F9E0 (:208)
float npc_sound_info_silent_death::get_default_sound_distance() const
{
    return SILENT_DEATH_SOUND_DISTANCE;
}

// 0x00A0F9F0 (:213)
float npc_sound_info_firearm::get_default_sound_distance() const
{
    return w_info->firing_sound_radius;
}

// 0x00A0FA00 (:218)
float npc_sound_info_bullet_strike::get_default_sound_distance() const
{
    return BULLET_STRIKE_SOUND_DISTANCE;
}

// 0x00A0FA10 (:223)
float npc_sound_info_building_damage::get_default_sound_distance() const
{
    return BUILDING_DAMAGE_SOUND_DISTANCE;
}

// 0x00A5B210 (:228)
float npc_sound_info_building_collapse::get_default_sound_distance() const
{
    const rfg_mover* building = get_building();
    if (building == nullptr)
    {
        return COLLAPSE_NO_BUILDING_DISTANCE;
    }
    vector bmin;
    vector bmax;
    building->get_local_bbox(bmin, bmax);
    float distance = sqrtf((bmax.y - bmin.y) * (bmax.x - bmin.x) * (bmax.z - bmin.z)) * COLLAPSE_VOLUME_SCALE;
    if (distance - COLLAPSE_MIN_DISTANCE < 0.0f)
    {
        distance = COLLAPSE_MIN_DISTANCE;
    }
    if (0.0f <= distance - COLLAPSE_MAX_DISTANCE)
    {
        distance = COLLAPSE_MAX_DISTANCE;
    }
    return distance;
}

// 0x00A0FA20 (:240)
float npc_sound_info_vehicle_damage::get_default_sound_distance() const
{
    return was_vehicle ? VEHICLE_COLLISION_SOUND_DISTANCE : VEHICLE_DAMAGE_SOUND_DISTANCE;
}

// 0x00A0FA50 (:245)
float npc_sound_info_stress::get_default_sound_distance() const
{
    return STRESS_SOUND_DISTANCE;
}

// 0x00A0FA60 (:250)
float npc_sound_info_footsteps::get_default_sound_distance() const
{
    return Running_player_footstep_sound_dist;
}

// 0x00A335E0 (:255)
float npc_sound_info_bomb_set::get_default_sound_distance() const
{
    const projectile* proj = projectile_from_sound_handle(sound_source);
    return proj != nullptr ? proj->w_info->m_explosion_info->m_flinch_radius : 0.0f;
}

// 0x00A33620 (:264)
void npc_sound_info_firearm::inform_npc(npc* listener) const
{
    listener->get_ai()->perception.weapon_sound_heard(get_attacker(), w_info);
}

// 0x00A33660 (:271)
void npc_sound_info_bullet_strike::inform_npc(npc* listener) const
{
    listener->get_ai()->perception.bullet_sound_heard(get_attacker(), w_info, impact_pos);
}

// 0x00A5B310 (:278)
void npc_sound_info_building_damage::inform_npc(npc* listener) const
{
    const human* attacker_hp = get_attacker();
    if (attacker_hp != nullptr)
    {
        listener->get_ai()->perception.building_damage_sound_heard(get_building(), attacker_hp, damage_pos);
    }
}

// 0x00A336B0 (:287)
void npc_sound_info_unimportant_building_damage::inform_npc(npc* listener) const
{
    const human* attacker_hp = get_attacker();
    if (attacker_hp != nullptr)
    {
        listener->get_ai()->perception.misc_sound_heard(attacker_hp, damage_pos);
    }
}

// 0x00A5B380 (:296)
void npc_sound_info_building_collapse::inform_npc(npc* listener) const
{
    const rfg_mover* building = get_building();
    if (building != nullptr)
    {
        listener->get_ai()->perception.building_collapse_heard(building);
    }
}

// 0x00A5B3C0 (:305)
void npc_sound_info_vehicle_damage::inform_npc(npc* listener) const
{
    const human* attacker_hp = get_attacker();
    const vehicle* target = get_target();
    if (attacker_hp != nullptr && target != nullptr)
    {
        listener->get_ai()->perception.vehicle_damage_heard(target, attacker_hp, damage_pos, was_vehicle);
    }
}

// 0x00A5B440 (:315)
void npc_sound_info_unimportant_vehicle_damage::inform_npc(npc* listener) const
{
    const human* attacker_hp = get_attacker();
    const vehicle* target = get_target();
    if (attacker_hp == nullptr || target == nullptr)
    {
        return;
    }
    ai_perception& perception = listener->get_ai()->perception;
    if (human_in_vehicle_interaction(listener, target))
    {
        perception.vehicle_damage_heard(target, attacker_hp, damage_pos, was_vehicle);
    }
    else
    {
        perception.misc_sound_heard(attacker_hp, damage_pos);
    }
}

// 0x00A336F0 (:329)
void npc_sound_info_pain::inform_npc(npc* listener) const
{
    const human* attacker_hp = get_attacker();
    if (attacker_hp != nullptr)
    {
        listener->get_ai()->perception.pain_sound_heard(get_target(), attacker_hp, w_info);
    }
}

// 0x00A33760 (:339)
void npc_sound_info_silent_death::inform_npc(npc* listener) const
{
    const human* target = get_target();
    const human* attacker_hp = get_attacker();
    if (attacker_hp == nullptr)
    {
        return;
    }
    // kept: the target is not checked for null before its position is read.
    ai_perception& perception = listener->get_ai()->perception;
    if (vec_dist_squared(listener->pos, target->pos) <= SILENT_DEATH_PAIN_DISTANCE_SQ)
    {
        perception.pain_sound_heard(target, attacker_hp, w_info);
    }
    else
    {
        perception.misc_sound_heard(attacker_hp, target->pos);
    }
}

// 0x00A33810 (:354)
void npc_sound_info_stress::inform_npc(npc* listener) const
{
    listener->get_ai()->perception.building_stress_sound_heard(rfg_mover_from_sound_handle(sound_source), stress_amount);
}

// 0x00A33860 (:361)
void npc_sound_info_footsteps::inform_npc(npc* listener) const
{
    listener->get_ai()->perception.footstep_sound_heard(human_from_sound_handle(sound_source));
}

// 0x00A338A0 (:368)
void npc_sound_info_bomb_set::inform_npc(npc* listener) const
{
    listener->get_ai()->perception.register_bomb_set(projectile_from_sound_handle(sound_source));
}

// 0x00A338E0 (:375)
void npc_sound_info_charge_placed::inform_npc(npc* listener) const
{
    listener->get_ai()->perception.register_charge_placed(projectile_from_sound_handle(sound_source));
}

// 0x00A33920 (:382)
vector npc_sound_info::get_sound_source_location() const
{
    return World.object_from_handle(sound_source)->pos;
}

// 0x00A0FA70 (:388)
vector npc_sound_info_bullet_strike::get_sound_source_location() const
{
    return impact_pos;
}

// 0x00A0FA90 (:393)
vector npc_sound_info_building_damage::get_sound_source_location() const
{
    return damage_pos;
}

// 0x00A0FAB0 (:398)
vector npc_sound_info_vehicle_damage::get_sound_source_location() const
{
    return damage_pos;
}

// 0x00A0FAD0 (:410)
npc_sound_info::npc_sound_info(const object* source)
{
    sound_source = source->get_handle();
    lfn.value = this;
}

// 0x005DD8E0 (.h:37)
npc_sound_info::~npc_sound_info()
{
}

// 0x00A0FAF0 (:416)
npc_sound_info_firearm::npc_sound_info_firearm(const human* _attacker, const weapon_info* _w_info)
    : npc_sound_info(_attacker)
{
    w_info = _w_info;
}

// 0x00A0FB20
npc_sound_info_firearm::~npc_sound_info_firearm()
{
}

// 0x00A0FB30 (:421)
npc_sound_info_bullet_strike::npc_sound_info_bullet_strike(const human* _attacker, const weapon_info* _w_info, const vector& pos)
    : npc_sound_info_firearm(_attacker, _w_info)
{
    impact_pos = pos;
}

// 0x00A0FB70
npc_sound_info_bullet_strike::~npc_sound_info_bullet_strike()
{
}

// 0x00A0FB80 (:426)
// kept: attacker_ is not checked
npc_sound_info_building_damage::npc_sound_info_building_damage(const rfg_mover* building, const human* attacker_, const vector& damage_pos_, float damage_amount_)
    : npc_sound_info(building)
{
    attacker = attacker_->get_handle();
    damage_pos = damage_pos_;
    damage_amount = damage_amount_;
}

// 0x00716A40
npc_sound_info_building_damage::~npc_sound_info_building_damage()
{
}

// 0x00A0FBD0 (:431)
npc_sound_info_unimportant_building_damage::npc_sound_info_unimportant_building_damage(const rfg_mover* building, const human* attacker_, const vector& damage_pos_, float damage_amount_)
    : npc_sound_info_building_damage(building, attacker_, damage_pos_, damage_amount_)
{
}

// 0x00A0FC20
npc_sound_info_unimportant_building_damage::~npc_sound_info_unimportant_building_damage()
{
}

// 0x00A0FC30 (:436)
npc_sound_info_building_collapse::npc_sound_info_building_collapse(const rfg_mover* building)
    : npc_sound_info(building)
{
}

// 0x00716A30
npc_sound_info_building_collapse::~npc_sound_info_building_collapse()
{
}

// 0x00A0FC50 (:441)
// kept: attacker_ is not checked, only the driver may be null
npc_sound_info_vehicle_damage::npc_sound_info_vehicle_damage(const vehicle* target, const human* driver_, const human* attacker_, const vector& damage_pos_, float damage_amount_, bool was_vehicle_)
    : npc_sound_info(target)
{
    attacker = attacker_->get_handle();
    driver = driver_ != nullptr ? driver_->get_handle() : 0xFFFFFFFF;
    damage_pos = damage_pos_;
    damage_amount = damage_amount_;
    was_vehicle = was_vehicle_;
}

// 0x00A0FCC0
npc_sound_info_vehicle_damage::~npc_sound_info_vehicle_damage()
{
}

// 0x00A0FCD0 (:446)
npc_sound_info_unimportant_vehicle_damage::npc_sound_info_unimportant_vehicle_damage(const vehicle* target, const human* driver_, const human* attacker_, const vector& damage_pos_, float damage_amount_, bool was_vehicle_)
    : npc_sound_info_vehicle_damage(target, driver_, attacker_, damage_pos_, damage_amount_, was_vehicle_)
{
}

// 0x00A0FD40
npc_sound_info_unimportant_vehicle_damage::~npc_sound_info_unimportant_vehicle_damage()
{
}

// 0x00A0FD50 (:451)
npc_sound_info_pain::npc_sound_info_pain(const human* _target, const human* _attacker, const weapon_info* _w_info)
    : npc_sound_info(_target)
{
    attacker = _attacker->get_handle();
    w_info = _w_info;
}

// 0x005EE380
npc_sound_info_pain::~npc_sound_info_pain()
{
}

// 0x00A0FD80 (:456)
npc_sound_info_death::npc_sound_info_death(const human* _target, const human* _attacker, const weapon_info* _w_info)
    : npc_sound_info_pain(_target, _attacker, _w_info)
{
}

// 0x00A2DAA0
npc_sound_info_death::~npc_sound_info_death()
{
}

// 0x00A0FDC0 (:461)
npc_sound_info_silent_death::npc_sound_info_silent_death(const human* _target, const human* _attacker, const weapon_info* _w_info)
    : npc_sound_info_pain(_target, _attacker, _w_info)
{
}

// 0x00A2DA90
npc_sound_info_silent_death::~npc_sound_info_silent_death()
{
}

// 0x00A0FE00 (:466)
npc_sound_info_stress::npc_sound_info_stress(rfg_mover* stressed_object, float stress_amount_)
    : npc_sound_info(stressed_object)
{
    stress_amount = stress_amount_;
}

// 0x007186C0
npc_sound_info_stress::~npc_sound_info_stress()
{
}

// 0x00A0FE30 (:471)
npc_sound_info_footsteps::npc_sound_info_footsteps(player* pp)
    : npc_sound_info(pp)
{
}

// 0x00A06D20
npc_sound_info_footsteps::~npc_sound_info_footsteps()
{
}

// 0x00A0FE70 (:476)
npc_sound_info_bomb_set::npc_sound_info_bomb_set(projectile* proj, const human* _owner)
    : npc_sound_info(proj)
{
    owner = _owner->get_handle();
}

// 0x00A0E660
npc_sound_info_bomb_set::~npc_sound_info_bomb_set()
{
}

// 0x00A0FEC0 (:481)
npc_sound_info_charge_placed::npc_sound_info_charge_placed(projectile* proj, const human* _owner)
    : npc_sound_info_bomb_set(proj, _owner)
{
}

// 0x00A0FEF0
npc_sound_info_charge_placed::~npc_sound_info_charge_placed()
{
}

// 0x00A33A50 (:485)
const human* npc_sound_info_firearm::get_attacker() const
{
    return human_from_sound_handle(sound_source);
}

// 0x00A33A70 (:490)
const rfg_mover* npc_sound_info_building_damage::get_building() const
{
    return rfg_mover_from_sound_handle(sound_source);
}

// 0x00A33AA0 (:495)
const human* npc_sound_info_building_damage::get_attacker() const
{
    return human_from_sound_handle(attacker);
}

// 0x00A33AC0 (:500)
const rfg_mover* npc_sound_info_building_collapse::get_building() const
{
    return rfg_mover_from_sound_handle(sound_source);
}

// 0x00A33AF0 (:505)
const vehicle* npc_sound_info_vehicle_damage::get_target() const
{
    return vehicle_from_sound_handle(sound_source);
}

// 0x00A33B10 (:510)
const human* npc_sound_info_vehicle_damage::get_driver() const
{
    return human_from_sound_handle(driver);
}

// 0x00A33B30 (:515)
const human* npc_sound_info_vehicle_damage::get_attacker() const
{
    return human_from_sound_handle(attacker);
}

// 0x00A33B50 (:520)
const human* npc_sound_info_pain::get_target() const
{
    return human_from_sound_handle(sound_source);
}

// 0x00A33B70 (:525)
const human* npc_sound_info_pain::get_attacker() const
{
    return human_from_sound_handle(attacker);
}

// 0x00A33B90 (:530)
const human* npc_sound_info_bomb_set::get_owner() const
{
    return human_from_sound_handle(owner);
}

// 0x00A83DE0 (:541)
void npc_sound_info::propagate(int inst)
{
    if (World.object_from_handle(sound_source) == nullptr)
    {
        return;
    }
    float min_range;
    float max_range = 0.0f;
    if (audiolib_play_get_state(inst, nullptr) == AUDIOLIB_ERR_NONE)
    {
        audiolib_play_get_audible_range(inst, &min_range, &max_range);
    }
    if (max_range <= 0.0f)
    {
        max_range = get_default_sound_distance();
    }
    const float max_range_sq = max_range * max_range;
    const vector source_pos = get_sound_source_location();
    const int count = World.get_num_objects<npc>();
    for (int i = 0; i < count; i++)
    {
        npc* listener = World.get_object<npc>(i);
        if (listener == nullptr || listener->is_killed() || listener->hflags.hidden || listener->npc_flags.deaf || !is_relevant_to_npc(listener))
        {
            continue;
        }
        vector head_pos;
        human_get_head_pos(listener, head_pos);
        if (vec_dist_squared(source_pos, head_pos) <= max_range_sq)
        {
            inform_npc(listener);
        }
    }
}

// 0x00A0FF00 (:606)
const object* npc_sound_info::get_source() const
{
    return World.object_from_handle(sound_source);
}

// 0x00A0FF10 (:618)
void npc_sound_info::level_init_pre()
{
    queue_mempool.clear();
    queued_infos.clear();
}

// 0x00A0FF50 (:633)
void npc_sound_info::send_queued()
{
    while (t_base_node* node = queued_infos.pop())
    {
        // Magic number: an lf_node's value follows the base node; -1 is "no audio instance", so propagate uses the
        // default distance.
        ((lf_node<npc_sound_info*>*)node)->value->propagate(-1);
    }
    queue_mempool.clear();
    queued_infos.clear();
}
