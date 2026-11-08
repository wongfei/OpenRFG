// Original: volition/rfg/code/scripting/target_outline.cpp
//
// Target outlines (gameflow.md GF1): four corner bitmaps around objects on screen, for the objects in Target_objects
// (HIGHLIGHT_STYLE_FRAME, and scripts) and for the local player's heat seeking target, which grows and turns opaque as
// the lock builds up.
//
// Floating point: the scalar statements are double chains rounded on assignment, as in the asm (stub_impl.md S3).

#include "volition/rfg/code/scripting/target_outline.h"

#include "volition/interface_lib/code/bitmap_sheets.h"
#include "volition/render_lib/code/gr/gr.h"
#include "volition/rfg/code/camera/rfg_camera.h"
#include "volition/rfg/code/destruction/rfg_mover.h"
#include "volition/rfg/code/hud/minimap.h"
#include "volition/rfg/code/interface/ui_document_hud_weapons.h"
#include "volition/rfg/code/level/world.h"
#include "volition/rfg/code/multi/multi_core/include/multi.h"
#include "volition/rfg/code/objects/humans/player.h"
#include "volition/vlib/code/math/vector3.h"
#include "volition/vlib/code/util/varray.h"

#include <cmath>

static int Target_corner_bitmap_id = -1; // 0x016B0D84
static farray<unsigned int,256> Target_objects; // 0x016B3A70
static int Local_player_targeting_int = 0; // 0x03611434
// Written by its setter and read by nothing in rfg.exe.
static int Local_player_locking_int = 0; // 0x03611438

// 0x00B2EB80 (:79)
void target_outline_level_init()
{
    Target_objects.remove_all();
}

// 0x00B24280 (:84)
void target_outline_system_init()
{
    Target_corner_bitmap_id = vint_bms_load("ui_hud_target_corner");
}

// 0x00B35390 (:91)
void target_outline_add_object(unsigned int handle)
{
    if (handle != 0xFFFFFFFF && !Target_objects.full())
    {
        Target_objects.add_unique(handle);
    }
}

// 0x00B353C0 (:104)
void target_outline_remove_object(unsigned int handle)
{
    if (handle != 0xFFFFFFFF)
    {
        Target_objects.remove_matching(handle);
    }
}

// 0x00B2EB90 (:113)
void target_outline_remove_all()
{
    Target_objects.remove_all();
}

// 0x00B242A0 (:118)
void target_outline_set_local_player_targeting_int(int targeting_int)
{
    Local_player_targeting_int = targeting_int;
}

// 0x00B242B0 (:123)
void target_outline_set_local_player_locking_int(int locking_int)
{
    Local_player_locking_int = locking_int;
}

// One corner at center + offset * radius, when that is on screen.
static void outline_render_corner(const vector& center, const vector& offset, float radius, float sign, float angle, float scale)
{
    vector world_pos;
    world_pos.x = (float)((double)sign * (double)(float)((double)offset.x * (double)radius) + (double)center.x);
    world_pos.y = (float)((double)sign * (double)(float)((double)offset.y * (double)radius) + (double)center.y);
    world_pos.z = (float)((double)sign * (double)(float)((double)offset.z * (double)radius) + (double)center.z);
    float screen_x = 0.0f;
    float screen_y = 0.0f;
    float screen_z = 0.0f;
    if (gr_screen_coords_from_world_coords(world_pos, &screen_x, &screen_y, &screen_z, 0))
    {
        minimap_render_bitmap(Target_corner_bitmap_id, screen_x, screen_y, angle, scale, scale, GM_FILTER);
    }
}

// 0x00B353E0 (:129)
// A negative size_override is the plain outline: it keeps the current color, so the color worked out below goes unused
// (kept from rfg.exe). Otherwise size_override scales the corners and is opaque at exactly 1.
void outline_specific_object(unsigned int handle, vector cam_orient_offset_ul, vector cam_orient_offset_ur, float size_override)
{
    object* op = World.object_from_handle(handle);
    if (op == nullptr)
    {
        target_outline_remove_object(handle); // inlined
        return;
    }
    if (op->get_sub_type() == OT_SUB_MOVER_RFG)
    {
        // A building as far gone as the game's destroyed percentage counts as dead.
        rfg_mover* mover = op->get_obj_type() == OT_MOVER ? static_cast<rfg_mover*>(op) : nullptr;
        if (mover->percent_destroyed() >= mover->game_destroyed_pct)
        {
            return;
        }
    }
    else if ((op->get_obj_type() == OT_VEHICLE || op->get_obj_type() == OT_HUMAN) && op->is_killed())
    {
        return;
    }

    vector bmin = ZERO_VECTOR;
    vector bmax = ZERO_VECTOR;
    op->get_bbox(bmin, bmax);
    vector center;
    center.x = (float)(((double)bmin.x + (double)bmax.x) * 0.5);
    center.y = (float)(((double)bmin.y + (double)bmax.y) * 0.5);
    center.z = (float)(((double)bmin.z + (double)bmax.z) * 0.5);
    const float radius = (float)(((double)(float)fabs((double)bmax.x - (double)center.x)
        + (double)(float)fabs((double)bmax.y - (double)center.y)
        + (double)(float)fabs((double)bmax.z - (double)center.z)) / 3.0);

    const float dist_squared = vec_xz_dist_squared(op->pos, Rfg_camera.real_pos);
    if (dist_squared > 62500.0f) // Magic number: 250 m
    {
        return;
    }
    // Magic numbers: the corners are drawn smaller with distance, full size (0.75) until 25 m, at 0.01 from 99 m on.
    float scale = (float)((100.0 - (double)(float)sqrt((double)dist_squared)) / 100.0);
    if ((float)((double)scale - 0.01) < 0.0f)
    {
        scale = 0.01f;
    }
    if ((float)((double)scale - 0.75) >= 0.0f)
    {
        scale = 0.75f;
    }

    int r;
    int g;
    int b;
    int a;
    gr_get_color(r, g, b, a);
    if (!Multiplayer)
    {
        player* pp = player_get_local_player();
        Local_player_targeting_int = hud_ui_weapons_determine_object_targeted(pp);
        // Magic numbers: yellow, or once locked green on a target (1) and red on a friend (2).
        r = 0xEB;
        g = 0xCD;
        b = 0;
        if (pp != nullptr && pp->heat_seeking_target_locking_status() == WLOS_LOCKED)
        {
            if (Local_player_targeting_int == 1)
            {
                r = 0;
                g = 0xB9;
            }
            else if (Local_player_targeting_int == 2)
            {
                g = 0;
            }
        }
    }
    if (size_override >= 0.0f)
    {
        scale = (float)((double)scale * (double)size_override);
        gr_set_color(r, g, b, size_override == 1.0f ? 0xFF : 100);
    }

    // Upper left, lower right, lower left, upper right, each bitmap turned to its corner.
    outline_render_corner(center, cam_orient_offset_ul, radius, 1.0f, 0.0f, scale);
    outline_render_corner(center, cam_orient_offset_ul, radius, -1.0f, 3.1415927f, scale);
    outline_render_corner(center, cam_orient_offset_ur, radius, -1.0f, 4.712389f, scale);
    outline_render_corner(center, cam_orient_offset_ur, radius, 1.0f, 1.5707964f, scale);
}

// 0x00B35BA0 (:250)
void target_outline_render()
{
    const matrix& orient = Rfg_camera.real_orient;
    vector offset_ul;
    offset_ul.x = (float)((double)orient.uvec.x - (double)orient.rvec.x);
    offset_ul.y = (float)((double)orient.uvec.y - (double)orient.rvec.y);
    offset_ul.z = (float)((double)orient.uvec.z - (double)orient.rvec.z);
    vector offset_ur;
    offset_ur.x = (float)((double)orient.rvec.x + (double)orient.uvec.x);
    offset_ur.y = (float)((double)orient.uvec.y + (double)orient.rvec.y);
    offset_ur.z = (float)((double)orient.uvec.z + (double)orient.rvec.z);

    for (int i = 0; i < Target_objects.size(); i++)
    {
        outline_specific_object(Target_objects[i], offset_ul, offset_ur, -1.0f);
    }
    player* pp = player_get_local_player();
    if (pp != nullptr)
    {
        // Magic number: the lock's outline reaches full size after 2 s on target.
        float size = (float)((double)pp->aim_target_duration.time_since() / 2000.0);
        if ((float)((double)size - 0.0) < 0.0f)
        {
            size = 0.0f;
        }
        if ((float)((double)size - 1.0) >= 0.0f)
        {
            size = 1.0f;
        }
        outline_specific_object(pp->heat_seeking_target, offset_ul, offset_ur, size);
    }
}
