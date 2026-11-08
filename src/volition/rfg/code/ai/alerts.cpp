// Original: volition/rfg/code/ai/alerts.cpp
//
// The shouts npcs hear from each other (stub_impl.md S9). Each one carries a handle and hands it to the listener's
// perception when it is relevant.

#include "volition/rfg/code/ai/alerts.h"

#include "volition/rfg/code/ai/ai_system.h"
#include "volition/rfg/code/ai/percept.h"
#include "volition/rfg/code/destruction/rfg_mover.h"
#include "volition/rfg/code/level/world.h"
#include "volition/rfg/code/objects/humans/human.h"
#include "volition/rfg/code/objects/humans/human_teams.h"
#include "volition/rfg/code/objects/humans/npc.h"
#include "volition/rfg/code/objects/projectile.h"
#include "volition/rfg/code/vehicle/vehicle_class.h"
#include "volition/vlib/code/math/vector3.h"

extern float Ai_fov_length_low; // percept.cpp, 0x01D6598C

static const float ALERT_SHOUT_MAX_DIST = 25.0f; // 0x012C840C
static const float THREATEN_PROPAGATE_DIST = 5.0f; // 0x012C8410
static const int THREATENED_ACKNOWLEDGE_MS = 10000;
static const float UNFRIENDLY_NEARBY_RELEVANCE = 10.0f;
static const float CORPSE_ATTACKER_RELEVANCE = 5.0f; // same as percept.cpp

// rfg.exe inlines these in almost every function below.
static object* typed_object_from_handle(unsigned int handle, object_type type)
{
    if (handle == 0xFFFFFFFF)
    {
        return nullptr;
    }
    object* objp = World.object_from_handle(handle);
    return objp != nullptr && objp->get_obj_type() == type ? objp : nullptr;
}

static npc* npc_from_handle(unsigned int handle)
{
    object* objp = typed_object_from_handle(handle, OT_HUMAN);
    return objp != nullptr && objp->get_sub_type() == OT_SUB_HUMAN_NPC ? static_cast<npc*>(objp) : nullptr;
}

// The attention target of the shouting npc when it is of the type.
static unsigned int attention_handle_of_type(const npc* source, object_type type)
{
    const object* objp = source->get_ai()->perception.get_attention_target();
    return objp != nullptr && objp->get_obj_type() == type ? objp->get_handle() : 0xFFFFFFFF;
}

// ---- alert_generic

// 0x005F9D40
alert_generic::alert_generic(const alert_generic& __that)
    : npc_sound_info(__that)
{
}

// 0x005DF640 (:199)
alert_generic::alert_generic(const npc* _source)
    : npc_sound_info(_source)
{
}

// 0x0060C5F0 (:33)
bool alert_generic::is_relevant_to_npc(npc* listener) const
{
    const npc* source = npc_from_handle(sound_source);
    return source != nullptr && listener != nullptr && human_teams_get_disposition(listener, source) == TEAM_DISPOSITION_ALLIED;
}

// 0x005DF660
alert_generic::~alert_generic()
{
}

// ---- alert_shout_threaten

// 0x0064B0F0
alert_shout_threaten::alert_shout_threaten(const alert_shout_threaten& __that)
    : npc_sound_info(__that), threatener(__that.threatener)
{
}

// 0x005DF790 (:290): the sound comes from the target, who is the one reacting.
alert_shout_threaten::alert_shout_threaten(const npc* source_, const human* target_)
    : npc_sound_info(target_), threatener(source_->get_handle())
{
}

// 0x0060C690 (:180)
void alert_shout_threaten::propagate(int inst)
{
    npc_sound_info::propagate(inst);
    object* objp = typed_object_from_handle(sound_source, OT_HUMAN);
    if (objp != nullptr)
    {
        static_cast<human*>(objp)->acknowledged_time.set(THREATENED_ACKNOWLEDGE_MS);
    }
}

// 0x0060C640 (:42)
bool alert_shout_threaten::is_relevant_to_npc(npc* listener) const
{
    const npc* source = npc_from_handle(threatener);
    // Disposition below 2: hostile or unfriendly to the threatener.
    return source != nullptr && static_cast<int>(human_teams_get_disposition(listener, source)) < 2;
}

// 0x00674990 (:157)
void alert_shout_threaten::inform_npc(npc* listener) const
{
    listener->get_ai()->perception.alert_threatened(npc_from_handle(threatener));
}

// 0x005DF620 (:60)
float alert_shout_threaten::get_default_sound_distance() const
{
    return THREATEN_PROPAGATE_DIST;
}

// 0x005DF7C0
alert_shout_threaten::~alert_shout_threaten()
{
}

// ---- alert_shout

// 0x0064B1A0
alert_shout::alert_shout(const alert_shout& __that)
    : alert_generic(__that)
{
}

// 0x005DF670 (:204)
alert_shout::alert_shout(const npc* _source)
    : alert_generic(_source)
{
}

// 0x005DF610 (:54)
float alert_shout::get_default_sound_distance() const
{
    return ALERT_SHOUT_MAX_DIST;
}

// 0x005DF690
alert_shout::~alert_shout()
{
}

// ---- alert_shout_disturbance

// 0x00673100
alert_shout_disturbance::alert_shout_disturbance(const alert_shout_disturbance& __that)
    : alert_shout(__that), belief(__that.belief)
{
}

// 0x0060C720 (:209)
alert_shout_disturbance::alert_shout_disturbance(const npc* _source, const ai_belief* source_belief)
    : alert_shout(_source)
{
    if (source_belief != nullptr)
    {
        belief = *source_belief;
        belief.next = nullptr;
        belief.prev = nullptr;
    }
}

// 0x006BBCD0 (:67)
void alert_shout_disturbance::inform_npc(npc* listener) const
{
    listener->get_ai()->perception.grant_belief(&belief);
}

// 0x005DF6B0
alert_shout_disturbance::~alert_shout_disturbance()
{
}

// ---- alert_shout_enemy

// 0x006731B0
alert_shout_enemy::alert_shout_enemy(const alert_shout_enemy& __that)
    : alert_shout(__that), spotted_enemy(__that.spotted_enemy)
{
}

// 0x005DF7D0 (:299)
alert_shout_enemy::alert_shout_enemy(const npc* _source, const human* _enemy)
    : alert_shout(_source), spotted_enemy(_enemy->get_handle())
{
}

// 0x0060C7F0 (:233)
alert_shout_enemy::alert_shout_enemy(const npc* _source)
    : alert_shout(_source), spotted_enemy(0xFFFFFFFF)
{
    const object* target = _source->get_ai()->perception.get_attention_target();
    if (target != nullptr && target->get_obj_type() == OT_HUMAN && target != _source)
    {
        spotted_enemy = target->get_handle();
    }
}

// 0x006BBCF0 (:80)
void alert_shout_enemy::inform_npc(npc* listener) const
{
    const object* enemy = typed_object_from_handle(spotted_enemy, OT_HUMAN);
    if (enemy != nullptr)
    {
        listener->get_ai()->perception.alert_enemy(npc_from_handle(sound_source), static_cast<const human*>(enemy));
    }
}

// 0x005DF740
alert_shout_enemy::~alert_shout_enemy()
{
}

// ---- alert_shout_unfriendly

// 0x006731F0
alert_shout_unfriendly::alert_shout_unfriendly(const alert_shout_unfriendly& __that)
    : alert_shout(__that), spotted_target(__that.spotted_target)
{
}

// 0x005DF6C0 (:217)
alert_shout_unfriendly::alert_shout_unfriendly(const npc* _source, const human* target)
    : alert_shout(_source), spotted_target(target->get_handle())
{
}

// 0x00674720 (:90)
void alert_shout_unfriendly::inform_npc(npc* listener) const
{
    const object* objp = typed_object_from_handle(spotted_target, OT_HUMAN);
    if (objp == nullptr)
    {
        return;
    }
    const human* hp = static_cast<const human*>(objp);
    ai_perception& perception = listener->get_ai()->perception;
    const float relevance = vec_dist(listener->pos, hp->pos) * 10.0f / Ai_fov_length_low;
    const unsigned int flags = perception.get_human_belief_flags(hp) | (relevance > UNFRIENDLY_NEARBY_RELEVANCE ? 0 : BF_NEARBY);
    perception.update_or_add_belief(AI_BELIEF_UNFRIENDLY, hp, relevance, flags, AI_BELIEF_UPDATE_OVERWRITE);
}

// 0x005DF6F0
alert_shout_unfriendly::~alert_shout_unfriendly()
{
}

// ---- alert_shout_projectile

// 0x005DF800 (:305)
alert_shout_projectile::alert_shout_projectile(const npc* _source, const projectile* _incoming)
    : alert_shout(_source), threatening_projectile(_incoming->get_handle())
{
}

// 0x0060C860 (:243)
alert_shout_projectile::alert_shout_projectile(const npc* _source)
    : alert_shout(_source), threatening_projectile(0xFFFFFFFF)
{
    const object* target = _source->get_ai()->perception.get_attention_target();
    if (target != nullptr && target->get_obj_type() == OT_ITEM && target->get_sub_type() == OT_SUB_ITEM_PROJECTILE)
    {
        threatening_projectile = target->get_handle();
    }
}

// 0x006747D0 (:103)
void alert_shout_projectile::inform_npc(npc* listener) const
{
    const object* hazard = typed_object_from_handle(threatening_projectile, OT_ITEM);
    if (hazard != nullptr && hazard->get_sub_type() == OT_SUB_ITEM_PROJECTILE)
    {
        listener->get_ai()->perception.alert_hazard(npc_from_handle(sound_source), hazard);
    }
}

// 0x005DF830
alert_shout_projectile::~alert_shout_projectile()
{
}

// ---- alert_shout_mover

// 0x005DF840 (:310)
alert_shout_mover::alert_shout_mover(const npc* _source, const object_mover* _incoming)
    : alert_shout(_source), mover(_incoming->get_handle())
{
}

// 0x0060C8D0 (:252)
alert_shout_mover::alert_shout_mover(const npc* _source)
    : alert_shout(_source), mover(attention_handle_of_type(_source, OT_MOVER))
{
}

// 0x00674840 (:113)
void alert_shout_mover::inform_npc(npc* listener) const
{
    const object* hazard = typed_object_from_handle(mover, OT_MOVER);
    if (hazard != nullptr)
    {
        listener->get_ai()->perception.alert_hazard(npc_from_handle(sound_source), hazard);
    }
}

// 0x005DF870
alert_shout_mover::~alert_shout_mover()
{
}

// ---- alert_shout_vehicle

// 0x005DF880 (:315)
alert_shout_vehicle::alert_shout_vehicle(const npc* _source, const vehicle* _vehicle)
    : alert_shout(_source), rogue_vehicle(_vehicle->get_handle())
{
}

// 0x0060C940 (:261)
alert_shout_vehicle::alert_shout_vehicle(const npc* _source)
    : alert_shout(_source), rogue_vehicle(attention_handle_of_type(_source, OT_VEHICLE))
{
}

// 0x006748B0 (:123)
void alert_shout_vehicle::inform_npc(npc* listener) const
{
    const object* hazard = typed_object_from_handle(rogue_vehicle, OT_VEHICLE);
    if (hazard != nullptr)
    {
        listener->get_ai()->perception.alert_hazard(npc_from_handle(sound_source), hazard);
    }
}

// 0x005DF8B0
alert_shout_vehicle::~alert_shout_vehicle()
{
}

// ---- alert_shout_collapsing_building

// 0x005DF8C0 (:320)
alert_shout_collapsing_building::alert_shout_collapsing_building(const npc* _source, const rfg_mover* building)
    : alert_shout(_source), falling_building(building->get_handle())
{
}

// 0x0060C9B0 (:270)
alert_shout_collapsing_building::alert_shout_collapsing_building(const npc* _source)
    : alert_shout(_source), falling_building(0xFFFFFFFF)
{
    const object* target = _source->get_ai()->perception.get_attention_target();
    if (target != nullptr && target->get_obj_type() == OT_MOVER && target->get_sub_type() == OT_SUB_MOVER_RFG)
    {
        falling_building = target->get_handle();
    }
}

// 0x00674920 (:133)
void alert_shout_collapsing_building::inform_npc(npc* listener) const
{
    const object* building = typed_object_from_handle(falling_building, OT_MOVER);
    if (building != nullptr && building->get_sub_type() == OT_SUB_MOVER_RFG)
    {
        listener->get_ai()->perception.alert_collapsing_building(npc_from_handle(sound_source), static_cast<const rfg_mover*>(building));
    }
}

// 0x005DF8F0
alert_shout_collapsing_building::~alert_shout_collapsing_building()
{
}

// ---- alert_shout_corpse

// 0x00673170
alert_shout_corpse::alert_shout_corpse(const alert_shout_corpse& __that)
    : alert_shout(__that), corpse(__that.corpse), killer(__that.killer)
{
}

// 0x005DF750 (:279)
alert_shout_corpse::alert_shout_corpse(const npc* source_, const human* corpse_, const human* killer_)
    : alert_shout(source_), corpse(corpse_->get_handle()), killer(killer_->get_handle())
{
}

// 0x006C1E20 (:143)
void alert_shout_corpse::inform_npc(npc* listener) const
{
    const human* dead = static_cast<const human*>(typed_object_from_handle(corpse, OT_HUMAN));
    const human* enemy = static_cast<const human*>(typed_object_from_handle(killer, OT_HUMAN));
    if (dead != nullptr && enemy != nullptr)
    {
        ai_perception& perception = listener->get_ai()->perception;
        perception.update_corpse_belief(dead);
        perception.suggest_enemy_exists(enemy, dead->pos, CORPSE_ATTACKER_RELEVANCE, true);
    }
}

// 0x005DF780
alert_shout_corpse::~alert_shout_corpse()
{
}

// ---- meaningless_shout

// 0x005DF700 (:223)
meaningless_shout::meaningless_shout(const npc* _source)
    : alert_shout(_source)
{
}

// 0x005DF630 (:172)
void meaningless_shout::propagate(int /*inst*/)
{
}

// 0x005DF720 (.h:92)
void meaningless_shout::inform_npc(npc* /*listener*/) const
{
}

// 0x005DF730
meaningless_shout::~meaningless_shout()
{
}
