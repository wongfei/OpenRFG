// Original: volition/rfg/code/gameplay/activities/activity_tanks_a_lot.cpp

#include "volition/rfg/code/gameplay/activities/activity_tanks_a_lot_internal.h"

#include "volition/rfg/code/gameplay/gameflow/gfm_control_morale.h"
#include "volition/rfg/code/hud/hud.h"
#include "volition/rfg/code/level/world.h"
#include "volition/rfg/code/multi/multi_core/include/multi.h"
#include "volition/rfg/code/objects/humans/human.h"
#include "volition/rfg/code/objects/humans/human_teams.h"
#include "volition/rfg/code/objects/humans/player.h"
#include "volition/rfg/code/objects/movers/object_mover.h"
#include "volition/rfg/code/rfg.h"
#include "volition/rfg/code/vehicle/vehicle_class.h"
#include "volition/rfg/code/vehicle/vehicle_info.h"
#include "volition/render_lib/code/gr/gr.h"
#include "volition/render_lib/code/gr/gr_font.h"
#include "volition/vlib/code/math/floating.h"
#include "volition/vlib/code/memory/virtual_mempool.h"
#include "volition/vlib/code/os/cs_wrapper.h"
#include "volition/vlib/code/util/color.h"
#include "volition/vlib/code/util/fastxml.h"

#include <math.h>
#include <stdio.h>

activity_tanks_a_lot Tanks_a_lot_activity; // 0x01655FB0

static timestamp Return_timer; // 0x01656470
static cs_wrapper Tanks_a_lot_cs(0); // 0x0226C5AC

// 0x0077E4E0 (.h:53)
activity_tanks_a_lot::activity_tanks_a_lot()
    : activity_base_modal(ACTIVITY_MODE_TANKS_A_LOT, false)
{
}

// 0x00768F30
activity_tanks_a_lot::~activity_tanks_a_lot()
{
}

// 0x0075A170. The local player sits in a tank.
bool player_local_in_tank()
{
    player* pp = player_get_local_player();
    if (pp == nullptr)
    {
        return false;
    }
    const vehicle* vp = human_in_vehicle(pp);
    return vp != nullptr && vp->info->vehicle_classification == BVT_TANK;
}

// 0x00770950
void activity_tal_building_damaged(unsigned int building_handle, float percent_damaged)
{
    if (building_handle == 0xFFFFFFFF)
    {
        return;
    }
    object* objp = World.object_from_handle(building_handle);
    if (objp == nullptr || objp->get_obj_type() != OT_MOVER || objp->get_sub_type() != OT_SUB_MOVER_RFG)
    {
        return;
    }
    object_mover* mover = static_cast<object_mover*>(objp);
    if (mover->team != HUMAN_TEAM_EDF)
    {
        return;
    }
    int value;
    switch (mover->get_importance_level())
    {
    case OBJECT_IMPORTANCE_LEVEL_NONE:
        value = Tanks_a_lot_activity.non_priority_value;
        break;
    case OBJECT_IMPORTANCE_LEVEL_LOW:
        value = Tanks_a_lot_activity.low_priority_value;
        break;
    case OBJECT_IMPORTANCE_LEVEL_MEDIUM:
        value = Tanks_a_lot_activity.medium_priority_value;
        break;
    case OBJECT_IMPORTANCE_LEVEL_HIGH:
        value = Tanks_a_lot_activity.high_priority_value;
        break;
    default:
        return;
    }
    Tanks_a_lot_activity.report_damage((float)value * percent_damaged);
}

// 0x0075A1A0 (:111). Original: the common pool is only restored when the table has its levels element.
void activity_tanks_a_lot::system_init()
{
    current_damage = 0.0f;
    current_level = 0;
    max_level = 0;
    non_priority_value = 0;
    low_priority_value = 0;
    medium_priority_value = 0;
    high_priority_value = 0;
    hud_message = INVALID_HUD_MESSAGE_HANDLE;
    const unsigned int mark = Common_mempool.mark();
    xml_element* root = xml_parse("activity_tanks_a_lot.xtbl", reinterpret_cast<mempool_base*>(&Common_mempool));
    if (root == nullptr)
    {
        return;
    }
    root = xml_find(root, "Table");
    if (root == nullptr)
    {
        return;
    }
    root = xml_find(root, "tanks_a_lot");
    if (root == nullptr)
    {
        return;
    }
    non_priority_value = xml_get_int(xml_find(root, "non_priority_value"), 0);
    low_priority_value = xml_get_int(xml_find(root, "low_priority_value"), 0);
    medium_priority_value = xml_get_int(xml_find(root, "medium_priority_value"), 0);
    high_priority_value = xml_get_int(xml_find(root, "high_priority_value"), 0);
    xml_element* levels = xml_find(root, "levels");
    if (levels == nullptr)
    {
        return;
    }
    for (xml_element* entry = xml_find(levels, "level"); entry != nullptr && max_level < 10; entry = xml_find_next(levels, entry, "level"))
    {
        level[max_level].damage_amount = (float)xml_get_int(xml_find(entry, "destruction_amount"), 0);
        level[max_level].reward = xml_get_int(xml_find(entry, "reward"), 0);
        max_level++;
    }
    Common_mempool.restore_to_mark(mark);
}

// 0x0075A350 (:198)
void activity_tanks_a_lot::level_init()
{
    activity_base_modal::level_init(); // rfg.exe inlines it
    sub_mode = ATALSM_INACTIVE;
    current_level = 0;
    current_damage = 0.0f;
    hud_message = INVALID_HUD_MESSAGE_HANDLE;
}

// 0x007AEAE0 (:209)
bool activity_tanks_a_lot::activity_start()
{
    if (Multiplayer || current_level >= max_level || !player_local_in_tank() || !activity_base_modal::activity_start())
    {
        return false;
    }
    current_damage = 0.0f;
    sub_mode = ATALSM_ACTIVE;
    if (hud_message != INVALID_HUD_MESSAGE_HANDLE)
    {
        hud_message_remove(hud_message);
        hud_message = INVALID_HUD_MESSAGE_HANDLE;
    }
    return true;
}

// 0x00799D60 (:241)
void activity_tanks_a_lot::activity_end()
{
    activity_base_modal::activity_end(); // rfg.exe inlines it
    sub_mode = ATALSM_INACTIVE;
}

// 0x00799D80 (:249)
void activity_tanks_a_lot::activity_kill()
{
    activity_base_modal::activity_kill(); // rfg.exe inlines it
    sub_mode = ATALSM_INACTIVE;
}

// 0x007B2EE0 (:256)
void activity_tanks_a_lot::activity_do_frame_active()
{
    static timestamp msg_timer; // 0x02273E2C
    char text[256];
    switch (sub_mode)
    {
    case ATALSM_ACTIVE:
        if (level[current_level].damage_amount <= current_damage)
        {
            msg_timer.set(5000); // Magic number: the result shows for five seconds
            sub_mode = ATALSM_SUCCESS;
            sprintf(text, "Rampage: Level %d completed", current_level + 1);
            hud_message_add(text, 5.0f, nullptr, nullptr, nullptr);
            sprintf(text, "You received a %d control bonus as a reward", level[current_level].reward);
            hud_message_add(text, 5.0f, nullptr, nullptr, nullptr);
            gfm_change_edf_control((float)-level[current_level].reward, nullptr, nullptr, nullptr);
            current_level++;
        }
        else if (!player_local_in_tank())
        {
            sub_mode = ATALSM_PAUSED;
            Return_timer.set(10000); // Magic number: ten seconds to get back in
        }
        break;
    case ATALSM_PAUSED:
        if (player_local_in_tank())
        {
            sub_mode = ATALSM_ACTIVE;
        }
        else if (Return_timer.elapsed())
        {
            msg_timer.set(5000);
            sub_mode = ATALSM_FAILURE;
            sprintf(text, "Rampage: Level %d failed", current_level + 1);
            hud_message_add(text, 5.0f, nullptr, nullptr, nullptr);
        }
        break;
    case ATALSM_SUCCESS:
    case ATALSM_FAILURE:
        if (msg_timer.elapsed())
        {
            activity_end();
        }
        break;
    default:
        break;
    }
}

// 0x00770A00 (:321). Original: a tank rampage that reaches ready is killed at once, so no spawn ever runs it.
void activity_tanks_a_lot::activity_do_frame_ready()
{
    activity_base::activity_do_frame_ready();
    activity_kill();
}

// 0x0075A3B0 (:328)
void activity_tanks_a_lot::activity_do_frame_inactive()
{
}

// 0x0075A3C0 (:334). Original: looks up the player's vehicle and does nothing with it.
void activity_tanks_a_lot::render_icons() const
{
}

// 0x00770A20 (:357)
bool activity_tanks_a_lot::render_hud(int x_pos, int y_pos)
{
    color white(255, 255, 255, 255);
    color border(255, 255, 255, 255);
    color background(0, 0, 0, 255);
    color complete(255, 255, 0, 255);
    gr_set_color(white);
    const int margin = fl_round(Y_res_multiplier * 5.0f);
    const int font_height = fl_round((float)gr_get_font_height(General_font) * Y_res_multiplier);
    const int x = x_pos + margin;
    const int line = font_height + margin;
    char text[1024];
    if (sub_mode != ATALSM_ACTIVE)
    {
        if (sub_mode != ATALSM_PAUSED)
        {
            return false;
        }
        sprintf(text, "You have %d seconds to return to your vehicle", (int)ceil((double)((float)Return_timer.time_until() / 1000.0f)));
        hud_message = hud_message_change_message(hud_message, text, (float)Return_timer.time_until() / 1000.0f, nullptr, nullptr);
    }
    const float target = level[current_level].damage_amount;
    if (target == 0.0f)
    {
        return false;
    }
    const float scale = X_res_multiplier < Y_res_multiplier ? X_res_multiplier : Y_res_multiplier;
    sprintf(text, "Rampage: Level %d", current_level + 1);
    int y = y_pos + margin + line;
    gr_string_scale(x, y, text, scale, General_font, GM_FILTER);
    float percent = current_damage / target;
    if (0.0f <= percent - 1.0f)
    {
        percent = 1.0f;
    }
    sprintf(text, "(%d/%d)", (int)current_damage, (int)target);
    y += line;
    const int bar_width = fl_round(X_res_multiplier * 200.0f); // Magic number: the bar is 200 by 20
    gr_string_scale(bar_width + 0x14 + x, y, text, scale, General_font, GM_FILTER);
    const int bar_height = fl_round(Y_res_multiplier * 20.0f);
    int offset = font_height - bar_height;
    if (offset < 0)
    {
        offset = 0;
    }
    hud_display_progress_bar(percent, x, y + offset / 2, bar_width, bar_height, fl_round(scale * 2.0f), border, background, complete);
    return false;
}

// 0x0075A3E0 (:422)
void activity_tanks_a_lot::report_damage(float damage_amount)
{
    if (is_active())
    {
        Tanks_a_lot_cs.enter();
        current_damage += damage_amount;
        Tanks_a_lot_cs.leave();
    }
}
