// Original: volition/rfg/code/gameplay/activities/activity_rewards.cpp
//
// The standard activity rewards.

#include "volition/rfg/code/gameplay/activities/activity_rewards.h"

#include "volition/rfg/code/gameplay/gameflow/gfm_control_morale.h"
#include "volition/rfg/code/interface/ui_document_hud.h"
#include "volition/rfg/code/objects/humans/player.h"
#include "volition/vlib/code/util/fastxml.h"

// 0x0075A010 (:36): a table without the element, or a value without its tag, rewards nothing.
bool load_standard_rewards(activity_standard_rewards& rewards_out, xml_element* xml_root)
{
    rewards_out.control = 0.0f;
    rewards_out.morale = 0.0f;
    rewards_out.salvage = 0;
    xml_element* rewards = xml_find(xml_root, "Standard_Rewards");
    if (rewards != nullptr)
    {
        xml_element* value = xml_find(rewards, "EDF_Control_Reduction");
        if (value != nullptr)
        {
            rewards_out.control = xml_get_float(value, 0.0f);
        }
        value = xml_find(rewards, "Morale_Increase");
        if (value != nullptr)
        {
            rewards_out.morale = xml_get_float(value, 0.0f);
        }
        value = xml_find(rewards, "Salvage_Reward");
        if (value != nullptr)
        {
            rewards_out.salvage = xml_get_int(value, 0);
        }
    }
    return true;
}

// 0x007B2E30 (:72)
void apply_standard_rewards(const activity_standard_rewards& rewards_in, district* dp)
{
    if (rewards_in.control != 0.0f)
    {
        gfm_change_edf_control(-rewards_in.control, dp, nullptr, nullptr);
    }
    if (rewards_in.morale != 0.0f)
    {
        gfm_change_district_morale(rewards_in.morale, dp, nullptr, nullptr);
    }
    if (rewards_in.salvage != 0)
    {
        player_get_meta_data()->add_salvage(rewards_in.salvage, false);
        item_pickup_info info;
        info.total_val = player_get_meta_data()->get_salvage();
        info.num_picked_up = rewards_in.salvage;
        info.item_name = "ui_hud_icon_salvage";
        hud_ui_add_item_pickup_message(&info, 1);
    }
}
