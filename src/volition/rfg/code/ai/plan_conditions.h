// Original: volition/rfg/code/ai/plan_conditions.h
// Implementation: volition/rfg/code/ai/plan_conditions.cpp
//
// ai_plan_conditions: the world-state bit sets of the planner (conditions that are known, and their values) that goals
// pursue and actions require and produce.
#pragma once

#include <initializer_list>

class npc;

// size 0x14
class ai_plan_conditions
{
public:
    ai_plan_conditions(unsigned int bit_index); // 0x005F02E0 (:39)
    ai_plan_conditions(const ai_plan_conditions& other); // 0x005DB5E0 (.h:34)
    ai_plan_conditions(); // 0x005DB5C0 (.h:33)
    ai_plan_conditions& operator=(const ai_plan_conditions& other); // 0x005DB610 (.h:38)
    bool operator==(const ai_plan_conditions& other) const; // 0x005DB640 (.h:41)
    bool operator!=(const ai_plan_conditions&) const;
    ai_plan_conditions& operator&=(const ai_plan_conditions& other); // 0x005F0320 (:50)
    ai_plan_conditions& operator|=(const ai_plan_conditions& other); // 0x005F0350 (:59)
    ai_plan_conditions operator&(const ai_plan_conditions& other) const; // 0x005FB330 (.h:47)
    ai_plan_conditions operator|(const ai_plan_conditions& other) const; // 0x005FB370 (.h:48)
    ai_plan_conditions operator~() const; // 0x005F0380 (:68)
    operator bool() const; // 0x005F03D0 (:79)
    bool operator!() const; // 0x005FB3B0 (.h:53)
    int bit_count() const; // 0x005F03F0 (:90)

private:
    unsigned int data[5]; // 0x00
};

// Not in rfg.exe: its action constructors or the bits together inline; tools/headless/ai_action_table.txt lists them as numbers.
ai_plan_conditions ai_plan_conditions_from_bits(std::initializer_list<unsigned int> bits);
ai_plan_conditions ai_plan_conditions_create_working_set(const npc* np); // 0x006A7760 (:106)
ai_plan_conditions ai_plan_conditions_get_for_loaded_firearm(const npc* np); // 0x006401B0 (:285)
