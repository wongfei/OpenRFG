// Original: volition/rfg/code/ai/beliefs.h
// Implementation: volition/rfg/code/ai/beliefs.cpp
//
// Beliefs: what an npc's perception records about another object (position, visibility, threat, relevance), allocated
// from a shared free list, with the descriptors, requirements and specifications that select beliefs by type.
#pragma once

#include "volition/rfg/code/ai/ai_types.h"
#include "volition/rfg/code/objects/object_type.h"
#include "volition/vlib/code/math/vector3.h"
#include "volition/vlib/code/time/timestamp.h"

// Shared pool all npcs allocate beliefs from.
static const int AI_BELIEF_POOL_SIZE = 3600;

// Bits of ai_belief::flags. Several bits mean different things per belief type; named from the BELIEF_REQUIREMENTS_*
// that test them and the functions that set them (inferred).
static const unsigned int BF_ACKNOWLEDGED = 0x00000001;
static const unsigned int BF_INVESTIGATED = 0x00000004;
static const unsigned int BF_FROM_ORDERS = 0x00000006;
static const unsigned int BF_FLEEING_FROM = 0x00000008;
static const unsigned int BF_IN_RESTRICTED_AREA = 0x00000010;
static const unsigned int BF_STICKY_MASK = 0x0000001B; // survive update_existing_belief
static const unsigned int BF_OCCUPIED = 0x00000020;
static const unsigned int BF_ATTACHED_TO_ME = 0x00000040;
static const unsigned int BF_OCCUPIED_BY_ENEMY = 0x00000080;
static const unsigned int BF_OCCUPIED_BY_FRIEND = 0x00000100;
static const unsigned int BF_OCCUPIED_BY_NEUTRAL = 0x00000200;
static const unsigned int BF_OCCUPIED_MASK = 0x00000380;
static const unsigned int BF_HEAVY_VEHICLE = 0x00000400;
static const unsigned int BF_NEARBY = 0x00001000;
static const unsigned int BF_UNCONFIRMED = 0x00002000; // enemy position beliefs
static const unsigned int BF_PROJECTILE_UNSEEN = 0x00002000; // projectile beliefs
static const unsigned int BF_LAST_POSITION_VISIBLE = 0x00004000; // enemy position beliefs
static const unsigned int BF_PROJECTILE_SEEN = 0x00004000; // projectile beliefs
static const unsigned int BF_SHOT_AT_ME = 0x00008000;
static const unsigned int BF_PROJECTILE_THREAT = 0x00010000;
static const unsigned int BF_STATIONARY = 0x00020000;
static const unsigned int BF_EXPLOSIVE = 0x00040000;
static const unsigned int BF_COMING_AT_ME = 0x00080000;
static const unsigned int BF_UNSAFE = 0x00200000;
static const unsigned int BF_UNFRIENDLY_BUILDING = 0x00400000;
static const unsigned int BF_SAFE_FRIENDLY_NONE = 0x00600080;
static const unsigned int BF_SHOT_HIT = 0x08000000;
static const unsigned int BF_SHOT_GOOD_OR_HIT = 0x18000000;
static const unsigned int BF_SHOT_NEAR_OR_BETTER = 0x38000000;
static const unsigned int BF_SHOT_BAD = 0x40000000;

// size 0x28
struct ai_belief
{
    friend class ai_perception; // the belief bins are sorted on relevance and keyed on type (inferred)

    float get_relevance() const { return relevance; } // 0x005DB4D0 (.h:160)
    ai_belief_types get_type() const { return static_cast<ai_belief_types>(type); } // 0x005DB4E0 (.h:162)
    object_type get_obj_type() const { return static_cast<object_type>(obj_type); } // 0x005DB4F0 (.h:163)
private:
    void set_type(ai_belief_types new_type) { type = static_cast<signed char>(new_type); } // 0x005DB500 (.h:175)
    void set_obj_type(object_type new_type) { obj_type = static_cast<signed char>(new_type); } // 0x005DB510 (.h:176)
public:
    ai_belief(const ai_belief& __that) = default; // 0x005F9CB0
    ai_belief(); // 0x005DF6A0

    unsigned int handle; // 0x00
    vector pos; // 0x04
    unsigned int flags; // 0x10
    timestamp update_time; // 0x14
    ai_belief* next; // 0x18
    ai_belief* prev; // 0x1C
private:
    float relevance; // 0x20
    signed char type; // 0x24
    signed char obj_type; // 0x25
};

// size 0x10
struct ai_belief_descriptor
{
    ai_belief_descriptor(ai_belief_types type_, const char* name_, int max_per_npc_, int sec_to_expire_); // 0x005DB520 (.h:214)

    ai_belief_types type; // 0x00
    const char* name; // 0x04
    int max_per_npc; // 0x08
    int sec_to_expire; // 0x0C
};

// size 0xC
struct ai_belief_requirements
{
    ai_belief_requirements(unsigned int all_, unsigned int any_, unsigned int none_); // 0x005DB540 (.h:223)
    bool test(unsigned int flags) const; // 0x005DB560 (.h:227)

    unsigned int all; // 0x00
    unsigned int any; // 0x04
    unsigned int none; // 0x08
};

// size 0x18
struct ai_belief_specification
{
    ai_belief_specification(ai_belief_types type_, unsigned int handle_, int time_since_, const ai_belief_requirements& requirements_); // 0x005DB590 (.h:280)

    ai_belief_types type; // 0x00
    unsigned int handle; // 0x04
    int time_since; // 0x08
    ai_belief_requirements requirements; // 0x0C
};

extern const ai_belief_requirements BELIEF_REQUIREMENTS_NONE; // 0x01D680B4
extern const ai_belief_requirements BELIEF_REQUIREMENTS_INVESTIGATED; // 0x01642408
extern const ai_belief_requirements BELIEF_REQUIREMENTS_NOT_INVESTIGATED; // 0x01642414
extern const ai_belief_requirements BELIEF_REQUIREMENTS_ACKNOWLEDGED; // 0x01642420
extern const ai_belief_requirements BELIEF_REQUIREMENTS_NOT_ACKNOWLEDGED; // 0x0164242C
extern const ai_belief_requirements BELIEF_REQUIREMENTS_UNOCCUPIED; // 0x01642438
extern const ai_belief_requirements BELIEF_REQUIREMENTS_NEARBY; // 0x01642444
extern const ai_belief_requirements BELIEF_REQUIREMENTS_SAFE; // 0x01642450
extern const ai_belief_requirements BELIEF_REQUIREMENTS_SAFE_FRIENDLY; // 0x0164245C
extern const ai_belief_requirements BELIEF_REQUIREMENTS_UNSAFE; // 0x01642468
extern const ai_belief_requirements BELIEF_REQUIREMENTS_FLEEING_FROM; // 0x01642474
extern const ai_belief_requirements BELIEF_REQUIREMENTS_SUCCESSFUL_HIT; // 0x01642480
extern const ai_belief_requirements BELIEF_REQUIREMENTS_NEAR_HIT; // 0x0164248C
extern const ai_belief_requirements BELIEF_REQUIREMENTS_BAD_SHOT; // 0x01642498
extern const ai_belief_requirements BELIEF_REQUIREMENTS_ATTACHED_TO_ME; // 0x016424A4
extern const ai_belief_requirements BELIEF_REQUIREMENTS_NEARBY_OR_FLEEING_FROM; // 0x016424B0
extern const ai_belief_requirements BELIEF_REQUIREMENTS_OCCUPIED_BY_ENEMY; // 0x016424BC
extern const ai_belief_requirements BELIEF_REQUIREMENTS_HEAVY_VEHICLE_THREAT; // 0x016424C8
extern ai_belief_descriptor Ai_belief_descriptors[NUM_AI_BELIEF_TYPES]; // 0x01642258
extern ai_belief* Ai_belief_free_list; // 0x01D65720

void free_ai_belief(ai_belief* belief); // 0x005E0160 (:176)
ai_belief* get_free_ai_belief(); // 0x005E01A0 (:182)
void free_ai_belief_list(ai_belief*& head); // 0x005E0200 (:195)
void ai_belief_system_init(); // 0x005E0250 (:243)
