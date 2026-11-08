// Original: volition/rfg/code/ai/beliefs.cpp
//
// The belief pool, its free list, the type descriptors and the shared requirement filters (stub_impl.md S9).

#include "volition/rfg/code/ai/beliefs.h"

const ai_belief_requirements BELIEF_REQUIREMENTS_NONE(0, 0, 0); // 0x01D680B4
const ai_belief_requirements BELIEF_REQUIREMENTS_INVESTIGATED(BF_INVESTIGATED, 0, 0); // 0x01642408
const ai_belief_requirements BELIEF_REQUIREMENTS_NOT_INVESTIGATED(0, 0, BF_INVESTIGATED); // 0x01642414
const ai_belief_requirements BELIEF_REQUIREMENTS_ACKNOWLEDGED(BF_ACKNOWLEDGED, 0, 0); // 0x01642420
const ai_belief_requirements BELIEF_REQUIREMENTS_NOT_ACKNOWLEDGED(0, 0, BF_ACKNOWLEDGED); // 0x0164242C
const ai_belief_requirements BELIEF_REQUIREMENTS_UNOCCUPIED(0, 0, BF_OCCUPIED_MASK); // 0x01642438
const ai_belief_requirements BELIEF_REQUIREMENTS_NEARBY(BF_NEARBY, 0, 0); // 0x01642444
const ai_belief_requirements BELIEF_REQUIREMENTS_SAFE(0, 0, BF_UNSAFE); // 0x01642450
const ai_belief_requirements BELIEF_REQUIREMENTS_SAFE_FRIENDLY(0, 0, BF_SAFE_FRIENDLY_NONE); // 0x0164245C
const ai_belief_requirements BELIEF_REQUIREMENTS_UNSAFE(BF_UNSAFE, 0, 0); // 0x01642468
const ai_belief_requirements BELIEF_REQUIREMENTS_FLEEING_FROM(BF_FLEEING_FROM, 0, 0); // 0x01642474
const ai_belief_requirements BELIEF_REQUIREMENTS_SUCCESSFUL_HIT(0, BF_SHOT_GOOD_OR_HIT, 0); // 0x01642480
const ai_belief_requirements BELIEF_REQUIREMENTS_NEAR_HIT(0, BF_SHOT_NEAR_OR_BETTER, 0); // 0x0164248C
const ai_belief_requirements BELIEF_REQUIREMENTS_BAD_SHOT(BF_SHOT_BAD, 0, 0); // 0x01642498
const ai_belief_requirements BELIEF_REQUIREMENTS_ATTACHED_TO_ME(BF_ATTACHED_TO_ME, 0, 0); // 0x016424A4
const ai_belief_requirements BELIEF_REQUIREMENTS_NEARBY_OR_FLEEING_FROM(0, BF_NEARBY | BF_FLEEING_FROM, 0); // 0x016424B0
const ai_belief_requirements BELIEF_REQUIREMENTS_OCCUPIED_BY_ENEMY(BF_OCCUPIED_BY_ENEMY, 0, 0); // 0x016424BC
const ai_belief_requirements BELIEF_REQUIREMENTS_HEAVY_VEHICLE_THREAT(0x420, 0x81000, 0x40); // 0x016424C8

// name, max per npc, seconds to expire. A max of 0 is a "special" belief, kept singly (update_special_belief);
// negative seconds never expire.
ai_belief_descriptor Ai_belief_descriptors[NUM_AI_BELIEF_TYPES] = // 0x01642258
{
    ai_belief_descriptor(AI_BELIEF_ENEMY_EXISTS, "enemy exists", 15, -1),
    ai_belief_descriptor(AI_BELIEF_ENEMY_POSITION, "enemy location", 15, 30),
    ai_belief_descriptor(AI_BELIEF_SHOT_RESULT, "shot result", 0, -1),
    ai_belief_descriptor(AI_BELIEF_SHOT_AT, "shot at", 0, 15),
    ai_belief_descriptor(AI_BELIEF_LAST_CONTACT, "last contact", 15, 30),
    ai_belief_descriptor(AI_BELIEF_FIRST_CONTACT, "first contact", 15, -1),
    ai_belief_descriptor(AI_BELIEF_CLOSEST_ALLY, "closest ally", 0, -2),
    ai_belief_descriptor(AI_BELIEF_CLOSEST_FRIENDLY, "closest friendly", 0, -2),
    ai_belief_descriptor(AI_BELIEF_UNFRIENDLY, "unfriendly", 4, 8),
    ai_belief_descriptor(AI_BELIEF_BEATDOWN_TARGET, "beatdown target", 0, -1),
    ai_belief_descriptor(AI_BELIEF_CORPSE, "corpse", 4, 30),
    ai_belief_descriptor(AI_BELIEF_FRIENDLY_FIRE, "friendly fire", 0, 3),
    ai_belief_descriptor(AI_BELIEF_BUILDING, "building", 4, -2),
    ai_belief_descriptor(AI_BELIEF_VEHICLE, "vehicle", 6, 1),
    ai_belief_descriptor(AI_BELIEF_TURRET, "turret", 4, 30),
    ai_belief_descriptor(AI_BELIEF_ALLY_ON_TURRET, "ally on turret", 4, -2),
    ai_belief_descriptor(AI_BELIEF_PROJECTILE, "projectile", 4, 15),
    ai_belief_descriptor(AI_BELIEF_HAZARD, "hazard", 4, 1),
    ai_belief_descriptor(AI_BELIEF_MISC_SOUND, "misc. sound", 4, 10),
    ai_belief_descriptor(AI_BELIEF_COMBAT_SOUND, "combat sound", 6, 60),
    ai_belief_descriptor(AI_BELIEF_BULLET_IMPACT, "bullet impact", 6, 60),
    ai_belief_descriptor(AI_BELIEF_EXPLOSION, "explosion", 4, 60),
    ai_belief_descriptor(AI_BELIEF_BUILDING_STRESS, "building stress", 4, 15),
    ai_belief_descriptor(AI_BELIEF_LARGE_DESTRUCTION, "large destruction", 1, 2),
    ai_belief_descriptor(AI_BELIEF_DAMAGE_TAKEN, "damage taken", 6, 60),
    ai_belief_descriptor(AI_BELIEF_LAST_DAMAGE_TAKEN, "last damage taken", 0, 60),
    ai_belief_descriptor(AI_BELIEF_SHOULD_ENGAGE_ENEMY, "should engage enemy", 6, 15),
};

ai_belief* Ai_belief_free_list; // 0x01D65720
static ai_belief Ai_belief_pool[AI_BELIEF_POOL_SIZE];

// 0x005DF6A0
ai_belief::ai_belief()
{
}

// 0x005DB520 (.h:214)
ai_belief_descriptor::ai_belief_descriptor(ai_belief_types type_, const char* name_, int max_per_npc_, int sec_to_expire_)
    : type(type_), name(name_), max_per_npc(max_per_npc_), sec_to_expire(sec_to_expire_)
{
}

// 0x005DB540 (.h:223)
ai_belief_requirements::ai_belief_requirements(unsigned int all_, unsigned int any_, unsigned int none_)
    : all(all_), any(any_), none(none_)
{
}

// 0x005DB560 (.h:227)
bool ai_belief_requirements::test(unsigned int flags) const
{
    return (all & flags) == all && (any == 0 || (flags & any) != 0) && (none & flags) == 0;
}

// 0x005DB590 (.h:280)
ai_belief_specification::ai_belief_specification(ai_belief_types type_, unsigned int handle_, int time_since_, const ai_belief_requirements& requirements_)
    : type(type_), handle(handle_), time_since(time_since_), requirements(requirements_)
{
}

// 0x005E0160 (:176)
void free_ai_belief(ai_belief* belief)
{
    if (belief == nullptr)
    {
        return;
    }
    ai_belief* head = Ai_belief_free_list;
    if (head == nullptr)
    {
        belief->next = belief;
        belief->prev = belief;
        Ai_belief_free_list = belief;
        return;
    }
    belief->prev = head->prev;
    belief->next = head;
    head->prev->next = belief;
    head->prev = belief;
}

// 0x005E01A0 (:182)
ai_belief* get_free_ai_belief()
{
    ai_belief* belief = Ai_belief_free_list;
    if (belief == nullptr)
    {
        return nullptr;
    }
    if (belief->next == belief)
    {
        Ai_belief_free_list = nullptr;
    }
    else
    {
        Ai_belief_free_list = belief->next;
        belief->prev->next = belief->next;
        belief->next->prev = belief->prev;
    }
    belief->prev = nullptr;
    belief->next = nullptr;
    return belief;
}

// 0x005E0200 (:195)
void free_ai_belief_list(ai_belief*& head)
{
    ai_belief* list = head;
    if (list == nullptr)
    {
        return;
    }
    ai_belief* free_head = Ai_belief_free_list;
    if (free_head == nullptr)
    {
        Ai_belief_free_list = list;
        head = nullptr;
        return;
    }
    // Splice the two circular lists.
    free_head->prev->next = list;
    list->prev->next = free_head;
    ai_belief* list_tail = list->prev;
    list->prev = free_head->prev;
    free_head->prev = list_tail;
    head = nullptr;
}

// 0x005E0250 (:243)
void ai_belief_system_init()
{
    Ai_belief_free_list = nullptr;
    for (int i = 0; i < AI_BELIEF_POOL_SIZE; ++i)
    {
        free_ai_belief(&Ai_belief_pool[i]);
    }
}
