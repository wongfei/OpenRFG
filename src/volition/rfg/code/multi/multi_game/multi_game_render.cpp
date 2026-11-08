// Original: volition/rfg/code/multi/multi_game/multi_game_render.cpp
//
// Multiplayer rendering: the HUD icons over the players' heads (name, team icon, distance fade, the damage/firing
// status rings, the talking icon), the local player's line of sight to the other players, and the team colors of
// icons, outlines and rim lighting. rfg.exe's render entry points (system/match init and shutdown, render player
// names) are empty.
// Part of the multi unity TU (rfg_multi_unity.obj, every multi/ .cpp in path order): the file statics are shared
// by the whole tree, and a shared one is placed in its first user (tools/hdrgen/specs/mp0_stubs.py).

#include "volition/rfg/code/multi/multi_game/multi_game_render.h"
#include "volition/rfg/code/multi/multi_game/multi_game.h"
#include "volition/rfg/code/multi/multi_game/multi_game_data.h"
#include "volition/rfg/code/multi/multi_game/multi_game_object_backpack.h"
#include "volition/rfg/code/multi/multi_game/multi_game_settings.h"
#include "volition/rfg/code/multi/multi_game/multi_gameplay_mode.h"
#include "volition/rfg/code/multi/multi_core/include/multi_voice.h"
#include "volition/rfg/code/camera/rfg_camera.h"
#include "volition/rfg/code/hud/hud_icon.h"
#include "volition/rfg/code/hud/minimap.h"
#include "volition/rfg/code/level/world.h"
#include "volition/rfg/code/objects/humans/human.h"
#include "volition/rfg/code/objects/humans/human_move.h"
#include "volition/rfg/code/objects/humans/human_teams.h"
#include "volition/rfg/code/objects/humans/player.h"
#include "volition/rfg/code/objects/humans/player_autoaim.h"
#include "volition/rfg/code/rfg.h"
#include "volition/rfg/code/weapons/weapons.h"
#include "volition/interface_lib/code/bitmap_sheets.h"
#include "volition/render_lib/code/gr/gr.h"
#include "volition/vlib/code/math/floating.h"
#include "volition/vlib/code/math/vector3.h"
#include "volition/vlib/code/util/color.h"
#include "volition/rfg/code/gamewide/tweak_table.h"

#include <wchar.h>

// ---- file-static globals
// No reader: the two references Ghidra lists are Multi_gameplay_mode_funcs[mode].alter_player_icon offsets.
static unsigned int Spectated_object_handle_spectator = 0xFFFFFFFF; // 0x0167DEDC
// multi_gameplay_mode Multi_gameplay_mode (0x0167DEE0) is used here too; placed in multi_gameplay_mode.cpp.
// unsigned int Local_game_data_handles[4] (0x0167E5F4) is used here too; placed in multi_join.cpp.
// How directly the camera must look at an enemy (the dot product) for its icon to show. External linkage:
// multi_game_mode_capture_the_flag.cpp (earlier in the unity TU) registers its tweak.
float Mp_enemy_player_name_min_dot = 0.9f; // 0x0167F298
// The same for a crouching enemy (its tweak item is multi_voice.cpp's, not ported).
static float Mp_enemy_player_name_crouching_min_dot = 0.95f; // 0x0167F29C
// Placed with its variable: rfg.exe's item is multi_voice.cpp's (later in this unity TU).
static tweak_table_item Tweak_table_item_Mp_enemy_player_name_crouching_min_dot("Mp_enemy_player_name_crouching_min_dot", &Mp_enemy_player_name_crouching_min_dot); // 0x02FEBC88
static hud_icons Multi_team_icons[2] = {HI_TEAM_GUERRILLA, HI_TEAM_EDF}; // 0x0167F2A0
// float Mp_glow_multiplier (0x0167F2A8) is used here too; placed in multi_game_mode_demolition.cpp.
// The icon fades out between these distances (meters, divided by the level's distance multiplier).
static float Mp_player_name_min_visible_distance = 25.0f; // 0x0167F2AC
static float Mp_player_name_max_visible_distance = 100.0f; // 0x0167F2B0
// color Mp_team_colors[3][2] (0x016932A8) is used here too; placed in multi_game_mode_bagman.cpp.
static color Status_damage_color_game_render(0xFF, 0x00, 0x00, 0xFF); // 0x016932C0
static color Status_firing_color(0xFF, 0xFF, 0x99, 0xFF); // 0x016932C4
// mp_game_data_wrapper* Mp_game_data (0x02FEB698) is used here too; placed in multi_connection.cpp.
// tweak_table_item Tweak_table_item_Mp_enemy_player_name_min_dot (0x02FEB878) is used here too; placed in multi_game_mode_capture_the_flag.cpp.

// ---- internal functions

// 0x009610F0 (:113)
void multi_game_render_system_init()
{
}

// 0x00961100 (:117)
void multi_game_render_system_shutdown()
{
}

// 0x00961110 (:121)
void multi_game_render_match_init()
{
}

// 0x009A0040 (:125): the icon of another player: over its head (higher with the distance), faded with the distance,
// the team color and icon; an enemy only when the local player can see it (or the mode shows it), never a fully
// stealthed one; grey when dead; status rings when it lost health or fired.
static bool player_icon_callback(vector& world_pos, hud_icon* icon)
{
    player* local_player = player_get_local_player();
    if (local_player == nullptr || icon->obj == 0xFFFFFFFF)
    {
        return false;
    }
    object* objp = World.object_from_handle(icon->obj);
    if (objp == nullptr || objp->get_obj_type() != OT_HUMAN || objp->get_sub_type() != OT_SUB_HUMAN_PLAYER)
    {
        return false;
    }
    player* pp = (player*)objp;
    if (local_player->is_killed())
    {
        return false;
    }
    float dist = vec_dist_approx(Rfg_camera.real_pos, pp->pos);
    const bool hostile = human_teams_get_disposition(pp, local_player) == TEAM_DISPOSITION_HOSTILE;
    icon->text_font = Latin_num_font;
    icon->visible = true;
    human_get_head_pos(pp, world_pos);
    // Magic numbers: 0.25 m over the head and 2 cm more per meter of distance.
    world_pos.y = (float)((double)dist * 0.02 + 0.25 + (double)world_pos.y);
    if (hostile)
    {
        if (pp->backpack != nullptr && pp->backpack->is_fully_stealthed())
        {
            return false;
        }
        // rfg.pdb names the object flag bit _dont_use_me; multi_game_process_player_names tests it too.
        if (!pp->obj_flags._dont_use_me)
        {
            return false;
        }
        if (!pp->pflags.mp_local_player_can_see_me)
        {
            icon->visible = false;
        }
    }
    color& icon_color = icon->icon_color;
    icon_color = color::WHITE;
    multi_game_render_get_team_color(icon_color, MTCT_ICON, pp);
    icon->bitmap_id = multi_game_render_get_team_icon(pp->current_team);
    const float min_dist = Mp_player_name_min_visible_distance;
    dist = (float)((double)dist / (double)Mp_server_options.get_level()->distance_multiplier);
    if ((float)((double)dist - (double)min_dist) < 0.0f)
    {
        dist = min_dist;
    }
    if ((float)((double)dist - (double)Mp_player_name_max_visible_distance) >= 0.0f)
    {
        dist = Mp_player_name_max_visible_distance;
    }
    const float fade = 1.0f - fl_parameter(dist, min_dist, Mp_player_name_max_visible_distance, false);
    // Magic number: the alpha scale (0 + fade * 255 in rfg.exe's doubles).
    icon_color.alpha = (unsigned char)(int)((double)fade * 255.0 + 0.0);
    bool mode_shows = false;
    if (multi_gameplay_mode_alter_player_icon(pp, icon))
    {
        icon->visible = true;
        mode_shows = true;
    }
    if (Mp_server_options.generic_options.enemy_icon_visibility == MGEIV_NONE && hostile && !mode_shows)
    {
        return false;
    }
    if (pp->is_killed())
    {
        icon_color.alpha = icon_color.alpha; // (sic) rfg.exe stores the alpha back
        // Magic numbers: the dead player's icon colors (enemy dark grey, ally grey blue).
        if (hostile)
        {
            icon_color.red = 0x32;
            icon_color.green = 0x32;
            icon_color.blue = 0x32;
        }
        else
        {
            icon_color.red = 0x3C;
            icon_color.green = 0x55;
            icon_color.blue = 0x5F;
        }
        hud_icon_enable_status_rings(icon->obj, false);
        if (hostile)
        {
            icon->visible = false;
            // Magic number: a dead enemy's icon fades out over 1.5 s.
            icon->fade_out_time_ms = 1500;
        }
    }
    else
    {
        if (!hostile || mode_shows)
        {
            if (pp->pflags.mp_lost_hp)
            {
                icon->status_ring_color.red = Status_damage_color_game_render.red;
                icon->status_ring_color.green = Status_damage_color_game_render.green;
                icon->status_ring_color.blue = Status_damage_color_game_render.blue;
                icon->status_ring_color.alpha = icon_color.alpha;
                hud_icon_enable_status_rings(icon->obj, true);
                pp->pflags.mp_lost_hp = 0;
            }
            else if (pp->pflags.mp_remote_player_fired_this_frame)
            {
                // (the firing flag is not cleared here)
                icon->status_ring_color.red = Status_firing_color.red;
                icon->status_ring_color.green = Status_firing_color.green;
                icon->status_ring_color.blue = Status_firing_color.blue;
                icon->status_ring_color.alpha = icon_color.alpha;
                hud_icon_enable_status_rings(icon->obj, true);
            }
        }
        if (hostile)
        {
            // Magic number: an enemy's icon fades out over 250 ms when it goes out of sight.
            icon->fade_out_time_ms = 250;
        }
    }
    icon->text_color = icon_color;
    if (Mp_server_options.generic_options.enemy_icon_visibility == MGEIV_ALWAYS)
    {
        return true;
    }
    return icon->visible;
}

// 0x00983E00 (:252): the talking icon left of the player's icon.
static void player_icon_post_render_callback(const vector& screen_pos, hud_icon* icon)
{
    if (icon->obj == 0xFFFFFFFF)
    {
        return;
    }
    object* objp = World.object_from_handle(icon->obj);
    if (objp == nullptr || objp->get_obj_type() != OT_HUMAN || objp->get_sub_type() != OT_SUB_HUMAN_PLAYER)
    {
        return;
    }
    const multi_game_data* data = multi_game_data_get(((player*)objp)->mp_data, false, false);
    if (data == nullptr)
    {
        return;
    }
    // (inlined in rfg.exe)
    if (multi_voice_get_icon_talk_state(data->player_uid) != MVTS_TALKING || vint_bms_get_image_data(icon->bitmap_id) == nullptr)
    {
        return;
    }
    const int image_id = hud_icon_get_bitmap_id(HI_MUTE);
    if (vint_bms_get_image_data(image_id) != nullptr)
    {
        // Magic numbers: 10 % larger than the text, one pixel to the left.
        const float scale = (float)((double)icon->text_scale * 1.1);
        gr_set_color(0xFF, 0xFF, 0xFF, icon->icon_color.alpha);
        minimap_render_bitmap(image_id, (float)((double)screen_pos.x - 1.0), screen_pos.y, 0.0f, scale, scale, GM_FILTER);
    }
}

// 0x009A0390 (:284): the player's icon, added on first use with its name, callbacks and status rings.
static hud_icon* add_hud_icon(player* pp)
{
    player* local_player = player_get_local_player();
    if (local_player == nullptr)
    {
        return nullptr;
    }
    const multi_game_data* data = multi_game_data_get(pp->mp_data, false, false);
    if (data == nullptr)
    {
        return nullptr;
    }
    hud_icon* icon = hud_icon_get(pp->get_handle());
    if (icon != nullptr)
    {
        return icon;
    }
    icon = hud_icon_add(pp->get_handle());
    // Original: hud_icon_add's result is not checked.
    icon->callback = player_icon_callback;
    icon->post_render_callback = player_icon_post_render_callback;
    icon->snap_to_edge = true;
    wcsncpy(icon->text[0], data->display_name, 32);
    icon->text[0][31] = L'\0';
    icon->status_ring_bitmap_id = hud_icon_get_bitmap_id(HI_STATUS_RING);
    // Magic numbers: a ring every 400 ms, each living 800 ms, for 2 s, up to 3 times the icon's size.
    icon->status_ring_interval_ms = 400;
    icon->status_ring_lifetime_ms = 800;
    icon->status_ring_total_lifetime_ms = 2000;
    icon->status_ring_max_scale = 3.0f;
    if (human_teams_get_disposition(pp, local_player) == TEAM_DISPOSITION_HOSTILE)
    {
        // Magic numbers: an enemy's icon fades in over 150 ms, 2.5 s with the delayed visibility option.
        icon->fade_in_time_ms = 150;
        if (Mp_server_options.generic_options.enemy_icon_visibility == MGEIV_DELAYED)
        {
            icon->fade_in_time_ms = 2500;
        }
    }
    else
    {
        icon->fade_in_time_ms = 0;
    }
    icon->icon_scale = 1.5f;
    icon->text_scale = 0.9f;
    return icon;
}

// 0x009A04F0 (:343): every frame, the other players' icons and whether the local player can see each one: a clear
// line from the camera to the head (or the aimed line through the head hits it) and the head within the min dot of
// the camera direction.
void multi_game_process_player_names()
{
    player* local_player = player_get_local_player();
    if (local_player == nullptr)
    {
        return;
    }
    const vector aim = player_autoaim_get_aim_vector();
    const int count = World.get_num_objects<player>();
    for (int i = 0; i < count; i++)
    {
        player* pp = World.get_object<player>(i);
        if (pp->obj_flags.destroyed || object_is_local_player(pp))
        {
            hud_icon_remove(pp->get_handle());
            continue;
        }
        if (add_hud_icon(pp) == nullptr)
        {
            // Original: a player without game data ends the loop; the players after it are not processed.
            return;
        }
        pp->pflags.mp_local_player_can_see_me = 0;
        if (!pp->obj_flags._dont_use_me)
        {
            continue;
        }
        vector head_pos;
        human_get_head_pos(pp, head_pos);
        weapon_fire_info fire_info(Rfg_camera.real_pos, head_pos, nullptr);
        if (weapons_test_bullet(fire_info, local_player, true, 0.0f, vector(0.0f, 0.0f, 0.0f)) && fire_info.hit_handle != pp->get_handle())
        {
            // something else is in the way: try the aimed line 5 m past the head
            const float dist = (float)((double)vec_dist(Rfg_camera.real_pos, head_pos) + 5.0);
            fire_info.end_pos.x = aim.x * dist + Rfg_camera.real_pos.x;
            fire_info.end_pos.y = aim.y * dist + Rfg_camera.real_pos.y;
            fire_info.end_pos.z = aim.z * dist + Rfg_camera.real_pos.z;
            if (!weapons_test_bullet(fire_info, local_player, true, 0.0f, vector(0.0f, 0.0f, 0.0f)) || fire_info.hit_handle != pp->get_handle())
            {
                continue;
            }
        }
        float min_dot = Mp_enemy_player_name_min_dot;
        if (human_move_is_crouching(pp))
        {
            min_dot = Mp_enemy_player_name_crouching_min_dot;
        }
        if (vec_dot_to_point(Rfg_camera.real_orient.fvec, Rfg_camera.real_pos, head_pos) >= min_dot)
        {
            pp->pflags.mp_local_player_can_see_me = 1;
        }
    }
}

// 0x00961120 (:410)
void multi_game_render_player_names()
{
}

// 0x00961130 (:416)
int multi_game_render_get_team_icon(human_teams team)
{
    if (team == HUMAN_TEAM_NONE)
    {
        return -1;
    }
    if (team == HUMAN_TEAM_CIVILIAN)
    {
        return hud_icon_get_bitmap_id(HI_STAR);
    }
    // Original: HUMAN_TEAM_MARAUDER (3) reads past the two team icons.
    return hud_icon_get_bitmap_id(Multi_team_icons[team]);
}

// 0x00983F80 (:429): the friendly or enemy color of the type; the outline color is scaled by the level's glow (the
// default or the alternative time of day's). LTCG: output_color in ECX, type in EAX.
static void get_team_color(color& output_color, multi_team_color_types type, bool friendly)
{
    if ((unsigned int)type >= NUM_MULTI_TEAM_COLOR_TYPES)
    {
        return;
    }
    output_color = Mp_team_colors[type][!friendly];
    if (type != MTCT_OUTLINE)
    {
        return;
    }
    float glow = Multi_match_info.use_default_time_of_day ? Mp_server_options.get_level()->glow_multiplier : Mp_server_options.get_level()->alt_glow_multiplier;
    glow = (float)((double)Mp_glow_multiplier * (double)glow);
    output_color.red = (unsigned char)(int)((double)output_color.red * (double)glow);
    output_color.green = (unsigned char)(int)((double)output_color.green * (double)glow);
    output_color.blue = (unsigned char)(int)((double)output_color.blue * (double)glow);
    output_color.alpha = (unsigned char)(int)((double)output_color.alpha * (double)glow);
}

// 0x00984050 (:452): friendly for an ally of the local player or the local player itself; white without an object.
void multi_game_render_get_team_color(color& output_color, multi_team_color_types type, const object* obj)
{
    if (obj == nullptr)
    {
        output_color = color::WHITE;
        return;
    }
    local_player_info info;
    player_get_local_player_info(&info);
    const bool friendly = human_teams_get_disposition(info.team, human_teams_get_team(obj)) == TEAM_DISPOSITION_ALLIED ||
                          player_get_local_player() == obj;
    get_team_color(output_color, type, friendly);
}

// 0x009840C0 (:472): friendly for an ally of the local player's team (a spectator's: the local player's); white
// without local game data or a team.
void multi_game_render_get_team_color(color& output_color, multi_team_color_types type, human_teams team)
{
    const multi_game_data* my_data = multi_game_data_get_my_data(ST_GAME_SNAPSHOT, true, false);
    if (my_data == nullptr || team == HUMAN_TEAM_NONE)
    {
        output_color = color::WHITE;
        return;
    }
    human_teams my_team = my_data->team;
    if (my_team == HUMAN_TEAM_MP_SPECTATOR)
    {
        local_player_info info;
        player_get_local_player_info(&info);
        my_team = info.team;
    }
    get_team_color(output_color, type, human_teams_get_disposition(my_team, team) == TEAM_DISPOSITION_ALLIED);
}

// 0x00984170 (:493)
void multi_game_render_get_friendly_team_color(color& output)
{
    output = Mp_team_colors[0][0];
}

// 0x00984180 (:498)
void multi_game_render_get_enemy_team_color(color& output)
{
    output = Mp_team_colors[0][1];
}
