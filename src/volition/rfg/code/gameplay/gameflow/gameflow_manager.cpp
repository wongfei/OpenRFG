// Original: volition/rfg/code/gameplay/gameflow/gameflow_manager.cpp
//
// gameflow.md GF2. Initializes and advances the game flow modules. gameplay_do_frame has gfm_do_frame's body written out
// (gameplay.cpp), as rfg.exe inlines it there.

#include "volition/rfg/code/gameplay/gameflow/gameflow_manager.h"

#include "volition/rfg/code/cutscene/cutscene.h"
#include "volition/rfg/code/gameplay/activities/gfm_reinforcement_coordinator.h"
#include "volition/rfg/code/gameplay/activities/gfm_smash_building_coordinator.h"
#include "volition/rfg/code/gameplay/gameflow/continuous_support.h"
#include "volition/rfg/code/gameplay/gameflow/gfm_control_morale.h"
#include "volition/rfg/code/gameplay/gameflow/gfm_orders_manager.h"
#include "volition/rfg/code/gameplay/gameflow/gfm_structure_defense_coordinator.h"
#include "volition/rfg/code/gameplay/gameflow/gfm_tech_level.h"
#include "volition/rfg/code/gameplay/gameflow/reinforcement_location.h"
#include "volition/rfg/code/gameplay/gameflow/squad_spawn_manager.h"
#include "volition/rfg/code/gameplay/vehicle_drop_off.h"
#include "volition/rfg/code/havok/havok.h"
#include "volition/rfg/code/multi/multi_core/include/multi.h"
#include "volition/rfg/code/objects/object.h"
#include "volition/rfg/code/pathfinding/bb_pathfinding.h"
#include "volition/rfg/code/pathfinding/nav_cell.h"
#include "volition/vlib/code/math/ix.h"
#include "volition/vlib/code/math/vector3.h"
#include "volition/vlib/code/util/color.h"

extern gfm_smash_building_coordinator Smash_building_coordinator; // 0x016572EC (gameplay.cpp)

// Magic number: havok_collect_nav_cells gets at most 128 cells here.
static const int MAX_EXTERIOR_CELLS = 128;

// 0x0075D9C0 (:75), objp in EAX
static bool is_in_local_bbox(vector pos, const object* objp)
{
    if (objp == nullptr)
    {
        return false;
    }
    vector bmin;
    vector bmax;
    objp->get_local_bbox(bmin, bmax);
    return ix_point_in_box_oriented(pos, objp->pos, objp->orient, bmin, bmax);
}

// 0x007A4910 (:94)
void gameflow_manager_init()
{
    gfm_tech_level_init();
    rfc_system_init(-1);
}

// 0x007AAB60 (:107)
void gfm_level_init_pre()
{
    ssm_level_init_pre();
    Orders_manager.level_init();
    sdc_level_init_pre();
    reinforcement_loc_level_init_pre();
    rfc_level_init_pre();
    cont_support_level_init_pre();
    Smash_building_coordinator.level_init();
    vehicle_drop_off_level_init_pre();
    gfm_control_morale_level_init_pre();
}

// 0x007B97A0 (:129)
void gfm_do_frame()
{
    if (Multiplayer || cutscene_is_playing())
    {
        return;
    }
    sdc_do_frame();
    reinforcement_loc_do_frame();
    rfc_do_frame();
    rfc_render_minimap();
    cont_support_do_frame();
    Orders_manager.do_frame();
    vehicle_drop_off_do_frame();
    ssm_do_frame();
    Smash_building_coordinator.do_frame();
}

// 0x0078BCA0 (:157)
bool gfm_pick_exterior_point(const object* objp, vector& returned_pos)
{
    if (objp == nullptr)
    {
        return false;
    }
    vector bmin;
    vector bmax;
    objp->get_local_bbox(bmin, bmax);
    return gfm_pick_exterior_point(objp->pos, vec_dist(bmin, bmax), returned_pos, objp);
}

// 0x007735F0 (:172)
bool gfm_pick_exterior_point(const vector& point, float size, vector& returned_pos, const object* exclude_objp)
{
    if (point.is_null())
    {
        return false;
    }
    returned_pos.zero();
    vector ground = point;
    if (!rfg_snap_to_ground(ground))
    {
        return false;
    }
    nav_cell* cells[MAX_EXTERIOR_CELLS + 1];
    const int num_cells = havok_collect_nav_cells(ground, size, cells, MAX_EXTERIOR_CELLS, false);
    if (num_cells <= 0)
    {
        return false;
    }
    const nav_cell* best = nullptr;
    float best_dist = 3.4028235e+38f;
    vector best_pos;
    best_pos.zero();
    for (int i = 0; i < num_cells; i++)
    {
        const nav_cell* cell = cells[i];
        // Magic number: flag 8 is a cell that cannot be walked.
        if ((cell->flags & 8) != 0)
        {
            continue;
        }
        vector centroid;
        nav_cell_get_centroid(&centroid, cell);
        if (exclude_objp != nullptr && is_in_local_bbox(centroid, exclude_objp))
        {
            continue;
        }
        const float dist = vec_dist_squared(centroid, point);
        if (dist < best_dist)
        {
            best = cell;
            best_pos = centroid;
            best_dist = dist;
        }
    }
    if (best == nullptr)
    {
        return false;
    }
    returned_pos = best_pos;
    return true;
}

// 0x0075DA20 (:237). Volition's empty body.
void gfm_load(const cfile* input_file)
{
    (void)input_file;
}

// 0x0075DA30 (:246). Volition's empty body.
void gfm_save(cfile* output_file)
{
    (void)output_file;
}

// 0x00773770 (:255)
color gfm_get_alert_color(gfm_alert_levels alert_level)
{
    static const color Alert_colors[NUM_ALERT_LEVELS] = { // 0x02273D88
        color(0, 255, 0, 255),
        color(255, 255, 0, 255),
        color(255, 128, 0, 255),
        color(255, 0, 0, 255),
    };
    if ((unsigned int)alert_level < NUM_ALERT_LEVELS)
    {
        return Alert_colors[alert_level];
    }
    return color(255, 255, 255, 255);
}
