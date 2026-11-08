// Original: volition/rfg/code/ai/plan_conditions.cpp
//
// rfg.exe passes conditions by bit number and the names are not in rfg.pdb, so the working set below is written with the
// numbers read from its masks (data[n] bit b = n * 32 + b).

#include "volition/rfg/code/ai/plan_conditions.h"

#include "volition/rfg/code/ai/ai_system.h"
#include "volition/rfg/code/ai/beliefs.h"
#include "volition/rfg/code/ai/combat/combat_common.h"
#include "volition/rfg/code/ai/misc_actions.h"
#include "volition/rfg/code/objects/humans/human_combat_cover.h"
#include "volition/rfg/code/objects/humans/human_combat_turret.h"
#include "volition/rfg/code/objects/humans/human_weapons.h"
#include "volition/rfg/code/objects/humans/npc.h"
#include "volition/rfg/code/objects/inventory_item.h"
#include "volition/rfg/code/objects/object_convoy.h"
#include "volition/rfg/code/objects/object_cover_node.h"
#include "volition/rfg/code/objects/turret.h"
#include "volition/rfg/code/weapons/weapons.h"
#include "volition/rfg/code/level/world.h"

#include <string.h>

ai_plan_conditions ai_plan_conditions_from_bits(std::initializer_list<unsigned int> bits)
{
    ai_plan_conditions result;
    for (unsigned int bit : bits)
    {
        result |= ai_plan_conditions(bit);
    }
    return result;
}

// 0x005F02E0 (:39)
ai_plan_conditions::ai_plan_conditions(unsigned int bit_index)
{
    memset(data, 0, sizeof(data));
    data[bit_index >> 5] = 1u << (bit_index & 0x1F);
}

// 0x005DB5C0 (.h:33)
ai_plan_conditions::ai_plan_conditions()
{
    memset(data, 0, sizeof(data));
}

// 0x005DB5E0 (.h:34)
ai_plan_conditions::ai_plan_conditions(const ai_plan_conditions& other)
{
    memcpy(data, other.data, sizeof(data));
}

// 0x005DB610 (.h:38)
ai_plan_conditions& ai_plan_conditions::operator=(const ai_plan_conditions& other)
{
    memcpy(data, other.data, sizeof(data));
    return *this;
}

// 0x005DB640 (.h:41)
bool ai_plan_conditions::operator==(const ai_plan_conditions& other) const
{
    return memcmp(data, other.data, sizeof(data)) == 0;
}

// 0x005F0320 (:50)
ai_plan_conditions& ai_plan_conditions::operator&=(const ai_plan_conditions& other)
{
    for (int i = 0; i < 5; i++)
    {
        data[i] &= other.data[i];
    }
    return *this;
}

// 0x005F0350 (:59)
ai_plan_conditions& ai_plan_conditions::operator|=(const ai_plan_conditions& other)
{
    for (int i = 0; i < 5; i++)
    {
        data[i] |= other.data[i];
    }
    return *this;
}

// 0x005FB330 (.h:47)
ai_plan_conditions ai_plan_conditions::operator&(const ai_plan_conditions& other) const
{
    ai_plan_conditions result(*this);
    result &= other;
    return result;
}

// 0x005FB370 (.h:48)
ai_plan_conditions ai_plan_conditions::operator|(const ai_plan_conditions& other) const
{
    ai_plan_conditions result(*this);
    result |= other;
    return result;
}

// 0x005F0380 (:68)
ai_plan_conditions ai_plan_conditions::operator~() const
{
    ai_plan_conditions result;
    for (int i = 0; i < 5; i++)
    {
        result.data[i] = ~data[i];
    }
    return result;
}

// 0x005F03D0 (:79)
ai_plan_conditions::operator bool() const
{
    for (int i = 0; i < 5; i++)
    {
        if (data[i] != 0)
        {
            return true;
        }
    }
    return false;
}

// 0x005FB3B0 (.h:53)
bool ai_plan_conditions::operator!() const
{
    for (int i = 0; i < 5; i++)
    {
        if (data[i] != 0)
        {
            return false;
        }
    }
    return true;
}

// 0x005F03F0 (:90): rfg.exe inlines a parallel bit count for each word.
int ai_plan_conditions::bit_count() const
{
    int count = 0;
    for (int i = 0; i < 5; i++)
    {
        unsigned int v = data[i];
        v = v - ((v >> 1) & 0x55555555);
        v = (v & 0x33333333) + ((v >> 2) & 0x33333333);
        v = (v + (v >> 4)) & 0x0F0F0F0F;
        count += (v * 0x01010101) >> 24;
    }
    return count;
}

// 0x006A7760 (:106)
ai_plan_conditions ai_plan_conditions_create_working_set(const npc* np)
{
    const ai_system* ai = np->get_ai();
    ai_plan_conditions result;

    if (human_weapons_get_weapon(np) == nullptr)
    {
        result |= ai_plan_conditions(7);
        if (inv_item_get_best_weapon(np, false) == nullptr)
        {
            result |= ai_plan_conditions(6);
        }
    }

    if (np->cur_stance != HST_PRONE && ai->planner.get_current_action() != scramble_to_feet_action::get_instance())
    {
        result |= ai_plan_conditions(9);
    }

    if (ai->is_at_guard_post())
    {
        result |= ai_plan_conditions(36);
    }

    const ai_plan_conditions manning_turret(144);
    turret* reserved = human_combat_get_reserved_turret(np);
    if (reserved == nullptr || reserved->turret_flags.vehicle_turret)
    {
        result |= ai_plan_conditions(145);
    }
    else if (human_combat_get_manned_turret(np) != nullptr)
    {
        result |= ai_plan_conditions(143) | manning_turret;
    }
    else
    {
        if (vec_dist_squared(reserved->get_operation_position(), np->pos) <= 1.0f && ai_combat_is_oriented_with_turret(np, reserved, nullptr))
        {
            result |= ai_plan_conditions(143);
        }
    }

    rfg_mover* building;
    // Bits 13 and 14 are "in a building" and its opposite.
    if (ai->perception.is_in_closest_building(np, &building, 0.0f, 1.25f))
    {
        result |= ai_plan_conditions(13);
    }
    else
    {
        result |= ai_plan_conditions(14);
    }

    const ai_plan_conditions target_in_los(23);
    unsigned int target = np->combat.target_handle;
    if (target != 0xFFFFFFFF && ai->perception.enemy_in_los(target))
    {
        result |= target_in_los;
    }

    result |= ai_plan_conditions(human_in_vehicle(np) != nullptr ? 49 : 47);

    if (!(result & manning_turret))
    {
        weapon* wp = human_weapons_get_weapon(np);
        if (wp != nullptr && wp->turret_handle == 0xFFFFFFFF)
        {
            if (wp->w_info->is_firearm())
            {
                result |= ai_plan_conditions(2);
            }
            else if (wp->w_info->ammo_type == WEAPON_AMMO_TYPE_MELEE)
            {
                result |= ai_plan_conditions(3);
            }
            if (wp->is_loaded())
            {
                result |= ai_plan_conditions(0);
            }
            if (wp->get_rounds_in_magazine() == wp->get_magazine_size() || !wp->uses_ammo())
            {
                result |= ai_plan_conditions(1);
            }
        }
    }

    if (ai->perception.belief_bins[AI_BELIEF_ENEMY_POSITION].count == 0)
    {
        result |= ai_plan_conditions(78);
    }
    if (ai->perception.belief_bins[AI_BELIEF_CLOSEST_ALLY].beliefs != nullptr)
    {
        result |= ai_plan_conditions(5);
    }

    cover_node* node = human_combat_cover_get_node(np);
    if (node == nullptr)
    {
        result |= ai_plan_conditions(58);
    }
    else
    {
        const ai_plan_conditions in_cover(60);
        if (human_combat_cover_is_stepped_in(np))
        {
            result |= ai_plan_conditions(59) | in_cover | ai_plan_conditions(61);
        }
        else if (human_combat_cover_is_stepped_out(np))
        {
            result |= ai_plan_conditions(59) | in_cover | ai_plan_conditions(62);
        }

        if ((result & in_cover) && (node->flags.fire_left || node->flags.fire_right || node->flags.fire_popup)
            && !(result & target_in_los) && ai->in_cover_timer.valid())
        {
            const ai_belief* contact = ai->perception.belief_bins[AI_BELIEF_LAST_CONTACT].beliefs;
            if (contact != nullptr)
            {
                // Bit 23 is also set when the last contact is newer than the time in cover + 1 s (0x3E8), unless cover
                // was left 0.5 s (0x1F4) or more ago.
                const int time_in_cover = ai->in_cover_timer.time_since();
                if (contact->update_time.time_since() < time_in_cover + 1000
                    && (!ai->out_of_cover_timer.valid() || ai->out_of_cover_timer.time_since() < 500))
                {
                    result |= target_in_los;
                }
            }
        }
    }

    if (np->has_entered_head_action_node())
    {
        result |= ai_plan_conditions(134);
    }
    if (np->get_current_action_node(false) == nullptr)
    {
        result |= ai_plan_conditions(136);
    }
    if (np->amb_prop_info == nullptr)
    {
        result |= ai_plan_conditions(137);
    }

    const object_convoy* convoy = ai->get_convoy_object();
    if (convoy == nullptr || !convoy->in_convoy_formation(ai->get_squad_handle()))
    {
        result |= ai_plan_conditions(153);
    }
    return result;
}

// 0x006401B0 (:285)
ai_plan_conditions ai_plan_conditions_get_for_loaded_firearm(const npc* np)
{
    ai_plan_conditions result;

    weapon* wp = human_weapons_get_weapon(np);
    bool equipped = false;
    if (wp != nullptr && wp->turret_handle == 0xFFFFFFFF)
    {
        switch (wp->w_info->ammo_type)
        {
        case WEAPON_AMMO_TYPE_BULLET:
        case WEAPON_AMMO_TYPE_PROJECTILE:
        case WEAPON_AMMO_TYPE_ELECTRICITY:
        case WEAPON_AMMO_TYPE_REPAIR:
        case WEAPON_AMMO_TYPE_BLANKS:
        case WEAPON_AMMO_TYPE_FLAME:
            equipped = true;
            break;
        default:
            break;
        }
    }

    if (!equipped)
    {
        // Original quirk kept: with no firearm in the inventory wp stays the unsuitable held weapon (it can be null, a
        // melee weapon or a turret's), and its reserve below still asks for bit 0.
        inventory_item* item = inv_item_get_best_firearm(np);
        if (item != nullptr)
        {
            if (item->weapon_handle == 0xFFFFFFFF)
            {
                return result;
            }
            object* objp = World.object_from_handle(item->weapon_handle);
            if (objp == nullptr || objp->get_obj_type() != OT_ITEM || objp->get_sub_type() != OT_SUB_ITEM_WEAPON)
            {
                return result;
            }
            wp = static_cast<weapon*>(objp);
            result |= ai_plan_conditions(2);
        }
    }

    if (wp != nullptr && wp->get_rounds_in_reserve() > 0 && !(equipped && wp->is_loaded()))
    {
        result |= ai_plan_conditions(0);
    }
    return result;
}
