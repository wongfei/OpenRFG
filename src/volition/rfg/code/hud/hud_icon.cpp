// Original: volition/rfg/code/hud/hud_icon.cpp
//
// Icons drawn over world objects (stub_impl.md S7): team icons, objectives, flags, with status rings, fades and the text
// lines above them.

#include "volition/rfg/code/hud/hud_icon.h"

#include "volition/interface_lib/code/bitmap_sheets.h"
#include "volition/interface_lib/code/vint_project.h"
#include "volition/rfg/code/camera/rfg_camera.h"
#include "volition/rfg/code/hud/hud.h"
#include "volition/rfg/code/hud/minimap.h"
#include "volition/rfg/code/interface/ui_document_hud.h"
#include "volition/rfg/code/level/world.h"
#include "volition/rfg/code/multi/multi_game/multi_game.h"
#include "volition/rfg/code/multi/multi_game/multi_game_camera.h"
#include "volition/rfg/code/objects/humans/player.h"
#include "volition/rfg/code/rfg.h"
#include "volition/render_lib/code/gr/gr.h"
#include "volition/render_lib/code/gr/gr_font.h"
#include "volition/vlib/code/math/floating.h"
#include "volition/vlib/code/math/vector2.h"
#include "volition/vlib/code/memory/object_pool.h"
#include "volition/vlib/code/os/timer.h"

#include <stdio.h>
#include <string.h>

// ---- file-static globals
static const char* Hud_icon_names[14] = {"ui_map_icon_attack", "ui_map_icon_defend", "ui_map_icon_guerrilla", "ui_map_icon_edf", "ui_map_icon_ring", "ui_map_icon_mute", "ui_map_icon_ring2", "ui_map_icon_flag", "ui_map_icon_flagman", "ui_hud_icon_lock", "ui_map_icon_bagdown", "ui_map_icon_bag", "ui_map_icon_demoman", "ui_map_icon_star_white"}; // 0x01660F10
static int Hud_icon_bitmap_ids[14]; // 0x02B958C0
static int Input_id_complete; // 0x02B958F8
static object_pool<hud_icon,64,1> Hud_icons; // 0x02BD31F0

// 0x008669F0
hud_icon::hud_icon()
{
}

// 0x00843E90
void hud_icon_system_init()
{
    for (int& id : Hud_icon_bitmap_ids)
    {
        id = -1;
    }
}

// 0x00843EE0
void hud_icon_level_init_post()
{
    for (int i = 0; i < NUM_HUD_ICONS; i++)
    {
        Hud_icon_bitmap_ids[i] = vint_bms_load(Hud_icon_names[i]);
    }
}

// 0x0087BBE0
void hud_icon_level_shutdown()
{
    for (int& id : Hud_icon_bitmap_ids)
    {
        if (id != -1)
        {
            vint_bms_release(id);
            id = -1;
        }
    }
    hud_icon_remove_all();
}

// 0x0087BCB0
void hud_icon_render()
{
    if (!hud_ui_is_loaded() || multi_game_spectating())
    {
        return;
    }
    const float res_scale = vint_project_get_scale(VINT_RES_SCALE_MAINTAIN_RATIO, VINT_RES_SCALE_MAINTAIN_RATIO).y;
    local_player_info local_info;
    player_get_local_player_info(&local_info);

    Hud_icons.thread_access.enter();
    hud_icon* icon = Hud_icons.get_first(0);
    // The next icon is taken first: an icon can remove itself in its callback and below.
    hud_icon* next = Hud_icons.get_next(icon);
    while (icon != nullptr)
    {
        vector pos = ZERO_VECTOR;
        bool object_gone = false;
        bool have_pos = true;
        object* objp = World.object_from_handle(icon->obj);
        if (objp == nullptr)
        {
            if (icon->internal_state.first_frame_setup)
            {
                pos = icon->internal_state.obj_pos_last_frame;
                object_gone = true;
            }
            else
            {
                have_pos = false;
            }
        }
        else
        {
            pos = objp->pos;
            icon->internal_state.first_frame_setup = true;
            icon->internal_state.obj_pos_last_frame = pos;
        }

        if (have_pos)
        {
            if (icon->callback != nullptr)
            {
                icon->visible = icon->callback(pos, icon);
            }
            vector to_icon = *local_info.pos - pos;
            float distance = to_icon.mag_approx();
            if (icon->fade_distance == 0.0f || distance < icon->fade_distance)
            {
                if (object_gone)
                {
                    icon->visible = false;
                }
                const int now = timer_get(TIMER_MILLISECONDS);
                float fade = hud_icon_get_fade_percent(icon);
                int fade_time = 0;
                bool fade_changed = false;
                if (!icon->visible && icon->internal_state.visible_last_frame && icon->fade_out_time_ms != 0)
                {
                    icon->internal_state.fading_in = false;
                    icon->internal_state.fading_out = true;
                    fade_time = icon->fade_out_time_ms;
                    fade_changed = true;
                }
                else if (icon->visible && !icon->internal_state.visible_last_frame && icon->fade_in_time_ms != 0)
                {
                    icon->internal_state.fading_out = false;
                    icon->internal_state.fading_in = true;
                    fade_time = icon->fade_in_time_ms;
                    fade_changed = true;
                }
                if (fade_changed)
                {
                    // A fade that interrupts another starts from where that one was.
                    int elapsed = 0;
                    if (icon->internal_state.fade_start_time_ms != 0 && fade < 1.0f)
                    {
                        elapsed = (int)((1.0f - fade) * (float)fade_time);
                    }
                    icon->internal_state.fade_start_time_ms = now - elapsed;
                    fade = hud_icon_get_fade_percent(icon);
                }
                if (icon->internal_state.fade_start_time_ms != 0 && fade == 1.0f)
                {
                    icon->internal_state.fading_in = false;
                    icon->internal_state.fading_out = false;
                    icon->internal_state.fade_start_time_ms = 0;
                }
                icon->internal_state.visible_last_frame = icon->visible;

                if (object_gone && !icon->internal_state.fading_out)
                {
                    if (icon->remove_icon_when_obj_is_destroyed)
                    {
                        hud_icon* gone = hud_icon_get(icon->obj);
                        if (gone != nullptr)
                        {
                            Hud_icons.free(gone);
                        }
                    }
                }
                else if ((icon->visible || icon->internal_state.fading_in || icon->internal_state.fading_out) && !multi_game_camera_is_running())
                {
                    const bm_image* image = vint_bms_get_image_data(icon->bitmap_id);
                    const int bitmap_width = (int)((float)image->bmw * icon->icon_scale * res_scale);
                    const int bitmap_height = (int)((float)image->bmh * icon->icon_scale * res_scale);
                    bool behind_camera = false;
                    bool stickied = false;
                    vector screen_pos;
                    if (hud_icon_get_screen_pos(screen_pos, pos, icon->snap_to_edge, bitmap_width, bitmap_height, &behind_camera, &stickied))
                    {
                        float alpha = 1.0f;
                        if (icon->internal_state.fading_out)
                        {
                            alpha = 1.0f - fade;
                        }
                        else if (icon->internal_state.fading_in)
                        {
                            alpha = fade;
                        }
                        if (icon->fade_distance > 0.0f)
                        {
                            alpha = (1.0f - distance / icon->fade_distance) * alpha;
                        }
                        color icon_color = icon->icon_color;
                        color text_color = icon->text_color;
                        // Original bug kept: the text's alpha comes from the icon's alpha, already faded, faded again.
                        icon_color.alpha = (unsigned char)(unsigned int)((float)icon->icon_color.alpha * alpha);
                        text_color.alpha = (unsigned char)(int)((float)icon_color.alpha * alpha);
                        if (behind_camera)
                        {
                            vector to_camera = pos - Rfg_camera.real_pos;
                            to_camera.normalize_safe();
                            // Magic numbers: 25 % at the camera's back, up to 100 % beside it.
                            const float behind = (to_camera.y * Rfg_camera.real_orient.fvec.y + to_camera.x * Rfg_camera.real_orient.fvec.x + to_camera.z * Rfg_camera.real_orient.fvec.z + 1.0f) * 0.75f + 0.25f;
                            icon_color.alpha = (unsigned char)(int)(behind * (float)icon_color.alpha);
                            text_color.alpha = (unsigned char)(int)(behind * (float)text_color.alpha);
                        }
                        if (icon->internal_state.use_status_rings)
                        {
                            const int end_time = icon->internal_state.status_ring_end_time;
                            const int start_time = icon->internal_state.status_ring_start_time;
                            const int emit_time = (end_time - icon->status_ring_lifetime_ms) - start_time;
                            if (now < end_time)
                            {
                                int elapsed = now - start_time;
                                if (elapsed < 1)
                                {
                                    elapsed = 0;
                                }
                                int age;
                                if (elapsed < emit_time)
                                {
                                    age = elapsed % icon->status_ring_interval_ms;
                                }
                                else
                                {
                                    age = (emit_time % icon->status_ring_interval_ms - emit_time) + elapsed;
                                }
                                while (age < icon->status_ring_lifetime_ms && age <= elapsed)
                                {
                                    const float life = 1.0f - (float)age / (float)icon->status_ring_lifetime_ms;
                                    const float ring_scale = (1.0f - life) * icon->status_ring_max_scale * res_scale;
                                    gr_set_color((int)icon->status_ring_color.red, (int)icon->status_ring_color.green, (int)icon->status_ring_color.blue, (int)(life * alpha * (float)icon->status_ring_color.alpha));
                                    Cartographer.render_bitmap_alt(icon->status_ring_bitmap_id, screen_pos.x, screen_pos.y, 0.0f, ring_scale, ring_scale, GM_FILTER);
                                    age += icon->status_ring_interval_ms;
                                }
                            }
                            else
                            {
                                icon->internal_state.use_status_rings = false;
                            }
                        }
                        gr_set_color(icon_color);
                        const float icon_scale = icon->icon_scale * res_scale;
                        Cartographer.render_bitmap_alt(icon->bitmap_id, screen_pos.x, screen_pos.y, 0.0f, icon_scale, icon_scale, GM_FILTER);
                        if (icon->post_render_callback != nullptr)
                        {
                            icon->post_render_callback(screen_pos, icon);
                        }
                        if (!stickied)
                        {
                            if (icon->display_distance)
                            {
                                char text[68];
                                sprintf(text, "%.1fm", (double)distance);
                                gr_set_color(255, 255, 255, behind_camera ? 0x80 : 0xFF);
                                int text_w = 0;
                                int text_h = 0;
                                gr_get_string_size(text_w, text_h, text, -1, Body_font);
                                // Magic number: the distance is drawn at 75 % scale.
                                text_w = (int)((float)text_w * 0.75f * res_scale);
                                text_h = (int)((float)text_h * 0.75f * res_scale);
                                const int x = (int)screen_pos.x - text_w / 2;
                                const int y = (int)(icon->text_scale * 0.6f * res_scale * (float)bitmap_height) + (int)screen_pos.y;
                                gr_set_color(0, 0, 0, (int)text_color.alpha);
                                const float scale = res_scale * 0.75f;
                                gr_string_scale(x + 2, y + 2, text, scale, Body_font, GM_FILTER);
                                gr_set_color(text_color);
                                gr_string_scale(x, y, text, scale, Body_font, GM_FILTER);
                            }
                            if (!behind_camera)
                            {
                                int y = (int)screen_pos.y - bitmap_height / 2;
                                for (int i = 0; i < 3; i++)
                                {
                                    if (icon->text[i][0] == L'\0')
                                    {
                                        break;
                                    }
                                    int text_w = 0;
                                    int text_h = 0;
                                    gr_get_string_size(text_w, text_h, icon->text[i], -1, icon->text_font);
                                    const int x = (int)(screen_pos.x - (float)(int)((float)text_w * icon->text_scale * res_scale) * 0.5f);
                                    y = y - (int)((float)text_h * icon->text_scale * res_scale);
                                    gr_set_color(0, 0, 0, (int)text_color.alpha);
                                    gr_string_scale(x + 2, y + 2, icon->text[i], icon->text_scale * res_scale, icon->text_font, GM_FILTER);
                                    gr_set_color(text_color);
                                    gr_string_scale(x, y, icon->text[i], icon->text_scale * res_scale, icon->text_font, GM_FILTER);
                                }
                            }
                        }
                    }
                }
            }
        }
        icon = next;
        next = Hud_icons.get_next(next);
    }
    Hud_icons.thread_access.leave();
}

// 0x00843F10 (LTCG: icon in ESI)
static void hud_icon_reset(hud_icon* icon)
{
    if (icon != nullptr)
    {
        memset(icon, 0, sizeof(hud_icon));
        icon->obj = 0xFFFFFFFF;
        icon->visible = true;
        icon->icon_color = color::WHITE;
        icon->text_color = color::WHITE;
        icon->icon_scale = 1.0f;
        icon->text_scale = 1.0f;
        icon->internal_state.fade_start_time_ms = 0;
        icon->remove_icon_when_obj_is_destroyed = true;
    }
}

// 0x0087C8C0
void hud_icon_add(unsigned int obj, int bitmap_id, float scale, const color& icon_color, bool display_distance, bool snap_to_edge)
{
    hud_icon* icon = hud_icon_get(obj);
    if (icon == nullptr)
    {
        icon = Hud_icons.alloc(0);
        hud_icon_reset(icon);
        if (icon == nullptr)
        {
            return;
        }
    }
    icon->bitmap_id = bitmap_id;
    icon->obj = obj;
    icon->icon_scale = scale;
    icon->icon_color = icon_color;
    icon->display_distance = display_distance;
    icon->snap_to_edge = snap_to_edge;
}

// 0x00876E40
hud_icon* hud_icon_add(unsigned int obj)
{
    hud_icon* icon = hud_icon_get(obj);
    if (icon == nullptr)
    {
        icon = Hud_icons.alloc(0);
        hud_icon_reset(icon);
        if (icon == nullptr)
        {
            return nullptr;
        }
    }
    icon->obj = obj;
    return icon;
}

// 0x008685F0
hud_icon* hud_icon_get(unsigned int obj)
{
    Hud_icons.thread_access.enter();
    for (hud_icon* icon = Hud_icons.get_first(0); icon != nullptr; icon = Hud_icons.get_next(icon))
    {
        if (icon->obj == obj)
        {
            Hud_icons.thread_access.leave();
            return icon;
        }
    }
    Hud_icons.thread_access.leave();
    return nullptr;
}

// 0x00868660
void hud_icon_enable_status_rings(unsigned int obj, bool enabled)
{
    hud_icon* icon = hud_icon_get(obj);
    if (icon != nullptr)
    {
        if (enabled)
        {
            const int now = timer_get(TIMER_MILLISECONDS);
            if (!icon->internal_state.use_status_rings)
            {
                icon->internal_state.status_ring_start_time = now;
            }
            icon->internal_state.status_ring_end_time = icon->status_ring_total_lifetime_ms + now;
        }
        icon->internal_state.use_status_rings = enabled;
    }
}

// 0x00843F70
float hud_icon_get_fade_percent(hud_icon* icon)
{
    if (icon->internal_state.fade_start_time_ms == 0)
    {
        return 1.0f;
    }
    const int now = timer_get(TIMER_MILLISECONDS);
    const int fade_time = icon->internal_state.fading_in ? icon->fade_in_time_ms : icon->fade_out_time_ms;
    const int start = icon->internal_state.fade_start_time_ms;
    return fl_parameter((float)now, (float)start, (float)(fade_time + start), true);
}

// 0x00876E80
void hud_icon_remove(unsigned int obj)
{
    hud_icon* icon = hud_icon_get(obj);
    if (icon != nullptr)
    {
        Hud_icons.free(icon);
    }
}

// 0x00876EA0
void hud_icon_remove_all()
{
    Hud_icons.thread_access.enter();
    hud_icon* icon = Hud_icons.get_first(0);
    while (icon != nullptr)
    {
        hud_icon* next = Hud_icons.get_next(icon);
        hud_icon* found = hud_icon_get(icon->obj);
        icon = next;
        if (found != nullptr)
        {
            Hud_icons.free(found);
        }
    }
    Hud_icons.thread_access.leave();
}

// 0x00843FF0
bool hud_icon_get_screen_pos(vector& screen_pos, const vector& world_pos, bool sticky, int bitmap_width, int bitmap_height, bool* behind_camera, bool* stickied)
{
    const float center_x = (float)(gr_safe_area_left() + gr_safe_area_width() / 2);
    const float center_y = (float)(gr_safe_area_top() + gr_safe_area_height() / 2);
    screen_pos.x = center_x;
    screen_pos.y = center_y;
    screen_pos.z = 0.0f;
    float near_dist = Rfg_camera.m_near_clip_dist * 2.0f;
    vector pos = world_pos;
    float plane_dist = vec_dist_to_plane(world_pos, Rfg_camera.real_orient.fvec, Rfg_camera.real_pos);
    if (plane_dist < 0.0f)
    {
        if (sticky)
        {
            if (-near_dist < plane_dist)
            {
                plane_dist = plane_dist - near_dist;
            }
            // Mirror the point to the camera's front, pushed vertically the further it is behind.
            const float mirror = -plane_dist * 2.0f;
            pos.x = mirror * Rfg_camera.real_orient.fvec.x + pos.x;
            pos.y = mirror * Rfg_camera.real_orient.fvec.y + pos.y;
            pos.z = pos.z + mirror * Rfg_camera.real_orient.fvec.z;
            vector to_pos = world_pos - Rfg_camera.real_pos;
            to_pos.normalize_safe();
            float push = (to_pos.y * Rfg_camera.real_orient.fvec.y + to_pos.x * Rfg_camera.real_orient.fvec.x + to_pos.z * Rfg_camera.real_orient.fvec.z) * -1.0f;
            // Magic number: 15 m at the camera's back.
            push = push * push * 15.0f;
            if (pos.y <= Rfg_camera.real_pos.y)
            {
                pos.y = pos.y - push;
            }
            else
            {
                pos.y = push + pos.y;
            }
        }
    }
    else if (plane_dist < near_dist)
    {
        pos.x = Rfg_camera.real_orient.fvec.x * near_dist + pos.x;
        pos.y = Rfg_camera.real_orient.fvec.y * near_dist + pos.y;
        pos.z = Rfg_camera.real_orient.fvec.z * near_dist + pos.z;
        plane_dist = near_dist;
    }

    bool on_screen = gr_screen_coords_from_world_coords(pos, &screen_pos.x, &screen_pos.y, &screen_pos.z, 1);
    if (on_screen && sticky && plane_dist < 0.0f)
    {
        // Behind the camera: out to the edge of the safe area.
        const vector from_center(screen_pos.x - center_x, screen_pos.y - center_y, screen_pos.z);
        const float edge_x = ((float)(from_center.x >= 0.0f ? gr_safe_area_right() : gr_safe_area_left()) - center_x) / from_center.x;
        const float edge_y = ((float)(from_center.y >= 0.0f ? gr_safe_area_bottom() : gr_safe_area_top()) - center_y) / from_center.y;
        const float edge = edge_y <= edge_x ? edge_y : edge_x;
        screen_pos.x = screen_pos.x + edge * from_center.x;
        screen_pos.y = edge * from_center.y + screen_pos.y;
        screen_pos.z = screen_pos.z + from_center.z * edge;
        on_screen = true;
    }

    const vector unclamped = screen_pos;
    if (sticky)
    {
        vector direction(unclamped.x - center_x, unclamped.y - center_y, unclamped.z);
        direction.normalize_safe();
        if (direction.is_unit(0.01f))
        {
            const float res_scale = X_res_multiplier - Y_res_multiplier >= 0.0f ? Y_res_multiplier : X_res_multiplier;
            // Magic numbers: at most 1040 by 480 pixels (at 1280x720) from the center.
            float limit = res_scale * 1040.0f * direction.x + center_x;
            const float over_x = screen_pos.x - limit;
            if (direction.x >= 0.0f ? over_x < 0.0f : over_x >= 0.0f)
            {
                limit = screen_pos.x;
            }
            screen_pos.x = limit;
            const float limit_y = res_scale * 480.0f * direction.y + center_y;
            const float over_y = screen_pos.y - limit_y;
            if (direction.y >= 0.0f ? over_y >= 0.0f : over_y < 0.0f)
            {
                screen_pos.y = limit_y;
            }
        }
        const float left = (float)(gr_safe_area_left() + bitmap_width / 2);
        const float top = (float)(gr_safe_area_top() + bitmap_height / 2);
        const float right = (float)(gr_safe_area_right() - bitmap_width / 2);
        const float bottom = (float)(gr_safe_area_bottom() - bitmap_height / 2);
        float x = screen_pos.x;
        if (x - left < 0.0f)
        {
            x = left;
        }
        if (x - right >= 0.0f)
        {
            x = right;
        }
        screen_pos.x = x;
        float y = screen_pos.y;
        if (y - top < 0.0f)
        {
            y = top;
        }
        if (y - bottom >= 0.0f)
        {
            y = bottom;
        }
        screen_pos.y = y;
    }

    if (behind_camera != nullptr)
    {
        *behind_camera = plane_dist < 0.0f;
    }
    if (stickied != nullptr)
    {
        *stickied = !vec_equal_robust(unclamped, screen_pos, 0.0001f);
    }
    return on_screen;
}

// 0x00844880
int hud_icon_get_bitmap_id(hud_icons icon)
{
    // Unsigned compare (JA at 0x00844887), so HI_INVALID (-1) answers -1.
    if ((unsigned int)icon < (unsigned int)NUM_HUD_ICONS)
    {
        return Hud_icon_bitmap_ids[icon];
    }
    return -1;
}
