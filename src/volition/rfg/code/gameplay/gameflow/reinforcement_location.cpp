// Original: volition/rfg/code/gameplay/gameflow/reinforcement_location.cpp
//
// gameflow.md GF2. Where reinforcements should head: the last position an EDF npc saw the player at and the last position
// the player damaged something from.

#include "volition/rfg/code/gameplay/gameflow/reinforcement_location.h"

#include "volition/rfg/code/ai/ai_system.h"
#include "volition/rfg/code/gameplay/gameflow/gameflow_enums.h"
#include "volition/rfg/code/gamewide/tweak_table.h"
#include "volition/rfg/code/level/world.h"
#include "volition/rfg/code/objects/humans/human_teams.h"
#include "volition/rfg/code/objects/humans/npc.h"
#include "volition/rfg/code/objects/humans/player.h"
#include "volition/vlib/code/math/ix.h"
#include "volition/vlib/code/math/vector3.h"
#include "volition/vlib/code/time/timestamp.h"

extern gfm_alert_levels Current_alert_level; // 0x022278B4 (alert_level_manager.cpp)

// Magic number: an object's box grows by at least 30 m per axis before the player counts as near it.
static const float MIN_BBOX_SIZE = 30.0f; // 0x012DBC74

static vector Last_player_pos_los; // 0x022481C8
static vector Last_player_pos_damage; // 0x0224CF8C
static timestamp Time_since_last_pos_los; // 0x016576A4
static timestamp Time_since_last_pos_damage; // 0x016576A8
static timestamp Lost_los_polling_timer; // 0x016576AC
static bool Lost_los_reported; // 0x022258A4
static int Lost_los_report_period_ms; // 0x022258B8
static tweak_table_item Tweak_table_item_Lost_los_report_period_ms("Lost_los_report_period_ms", &Lost_los_report_period_ms);

// 0x0075DFC0 (:67). Volition's empty body: the voice line it played is gone.
static void report_lost_los(npc* np)
{
    (void)np;
}

// 0x0075DFD0 (:80)
void reinforcement_loc_level_init_pre()
{
    Last_player_pos_los.zero();
    Time_since_last_pos_los.invalidate();
    Last_player_pos_damage.zero();
    Time_since_last_pos_damage.invalidate();
    Lost_los_polling_timer.set(0);
    Lost_los_reported = true;
}

// 0x0078C210 (:92)
void reinforcement_loc_do_frame()
{
    if (Lost_los_reported || (int)Current_alert_level <= -1 || ((int)Current_alert_level < 4 && (int)Current_alert_level <= 0))
    {
        return;
    }
    if (!Lost_los_polling_timer.elapsed())
    {
        return;
    }
    // Magic numbers: poll every 250 to 500 ms.
    Lost_los_polling_timer.set_rand(250, 500);
    if (Lost_los_report_period_ms >= Time_since_last_pos_los.time_since())
    {
        return;
    }
    const player* pp = player_get_local_player();
    float best_dist = 3.4028235e+38f;
    npc* best_npc = nullptr;
    const int num = World.get_num_objects<npc>();
    for (int i = 0; i < num; i++)
    {
        npc* np = World.get_object<npc>(i);
        if (np->is_killed() || human_teams_get_disposition(np, HUMAN_TEAM_EDF) != TEAM_DISPOSITION_ALLIED || np->get_current_voice_line_handle() != INVALID_VOICE_LINE_HANDLE)
        {
            continue;
        }
        const float dist = vec_dist_squared(pp->pos, np->pos);
        // Magic number: 50 m squared.
        if (dist <= 2500.0f && dist < best_dist)
        {
            best_dist = dist;
            best_npc = np;
        }
    }
    // Original: the closest npc is chosen and then nothing is done with it; report_lost_los is empty and not called.
    (void)best_npc;
    Lost_los_reported = true;
}

// 0x0075E010 (:156)
void reinforcement_report_player_pos(const npc* np, const vector& player_pos)
{
    // Below orange, only npcs on defend orders report the player.
    if (np != nullptr && ((int)Current_alert_level < 0 || ((int)Current_alert_level < 4 && (int)Current_alert_level < 2)) && np->get_ai()->get_orders_type() != GFM_ORDERS_TYPE_DEFEND)
    {
        return;
    }
    Last_player_pos_los = player_pos;
    Time_since_last_pos_los.set(0);
    Lost_los_reported = false;
}

// 0x0075E070 (:176)
void reinforcement_report_damage_pos(unsigned int damaged_object)
{
    object* objp = World.object_from_handle(damaged_object);
    if (objp == nullptr)
    {
        return;
    }
    vector bmin;
    vector bmax;
    objp->get_bbox(bmin, bmax);
    float size_x = bmax.x - bmin.x;
    float size_y = bmax.y - bmin.y;
    float size_z = bmax.z - bmin.z;
    if (size_x < MIN_BBOX_SIZE)
    {
        size_x = MIN_BBOX_SIZE;
    }
    if (size_y < MIN_BBOX_SIZE)
    {
        size_y = MIN_BBOX_SIZE;
    }
    if (size_z < MIN_BBOX_SIZE)
    {
        size_z = MIN_BBOX_SIZE;
    }
    const vector grown_min(bmin.x - size_x, bmin.y - size_y, bmin.z - size_z);
    const vector grown_max(bmax.x + size_x, bmax.y + size_y, bmax.z + size_z);
    const player* pp = player_get_local_player();
    if (ix_point_in_box(pp->pos, grown_min, grown_max))
    {
        Last_player_pos_damage = pp->pos;
        Time_since_last_pos_damage.set(0);
    }
}

// 0x0075E260 (:218)
const vector& reinforcement_get_best_position()
{
    const bool have_los = Time_since_last_pos_los.valid();
    const bool have_damage = Time_since_last_pos_damage.valid();
    if (!have_los)
    {
        if (have_damage)
        {
            return Last_player_pos_damage;
        }
        const player* pp = player_get_local_player();
        Last_player_pos_los = pp->pos;
        Time_since_last_pos_los.set(0);
        Lost_los_reported = false;
        return pp->pos;
    }
    if (have_damage && Time_since_last_pos_damage.time_since() < Time_since_last_pos_los.time_since())
    {
        return Last_player_pos_damage;
    }
    return Last_player_pos_los;
}
