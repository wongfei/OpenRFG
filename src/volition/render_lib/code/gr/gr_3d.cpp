// Original: volition/render_lib/code/gr/gr_3d.cpp
//
// The 3D half of the legacy gr API on top of rl_draw's state (local-to-clip and clip-to-screen transforms, the render
// offset).
//
// Partial port (VFX.md FX5): the world-to-screen projection fb_accum_blur.cpp needs (gr_rotate_vertex,
// gr_project_vertex, gr_screen_coords_from_world_coords) and the bounding box rectangle (stub_impl.md S3:
// gr_get_bbox_2d_rect and its _ex/_robust forms); the rest (instances, unprojection, the clipped bounding box
// rectangles, the projection matrix builders) is not ported.

#include "volition/render_lib/code/gr/gr.h"

#include "volition/render_lib/code/rl_camera.h"
#include "volition/render_lib/code/rl_draw.h"
#include "volition/render_lib/code/rl_rect.h"
#include "volition/render_lib/code/rl_render_lib.h"
#include "volition/render_lib/code/rl_state_manager.h"
#include "volition/render_lib/code/rl_utility.h"
#include "volition/vlib/code/math/bbox.h"
#include "volition/vlib/code/math/ix.h"
#include "volition/vlib/code/math/matrix.h"
#include "volition/vlib/code/math/plane.h"
#include "volition/vlib/code/math/vector3.h"
#include "volition/vlib/code/math/vector4.h"

#include <math.h>

// 0x00509620 (:167)
// The world position relative to rl_draw's render offset, rotated and projected; false for a null output, a point
// outside the clip codes allowed (behind the eye only with allow_off_edges, else any edge too) or one not projected.
bool gr_screen_coords_from_world_coords(const vector& world_pos, float* screen_x, float* screen_y, float* screen_z, int allow_off_edges)
{
    if (screen_x == nullptr || screen_y == nullptr || screen_z == nullptr)
    {
        return false;
    }
    const vector& offset = rl_draw::get_instance()->get_rl_state().get_render_offset();
    const vector local_pos(world_pos.x - offset.x, world_pos.y - offset.y, world_pos.z - offset.z);
    vertex v;
    const unsigned int codes = gr_rotate_vertex(v, local_pos);
    // Magic numbers: the clip codes that reject the point, behind (0x80) with allow_off_edges, else also off the
    // left, right, bottom and top edges (0xBC).
    const unsigned int reject = allow_off_edges != 0 ? 0x80 : 0xBC;
    if ((codes & reject) != 0)
    {
        return false;
    }
    gr_project_vertex(v);
    if ((v.flags & 1) == 0)
    {
        return false;
    }
    *screen_x = v.sx;
    *screen_y = v.sy;
    *screen_z = v.sw;
    return true;
}

// 0x004DDB00 (:361)
// The point through rl_draw's local-to-clip transform, with the clip codes: 8 right (x > w), 4 left (x < -w), 0x20
// top (y > w), 0x10 bottom (y < -w), 0x80 behind (z <= 0). The flags are cleared.
unsigned int gr_rotate_vertex(vertex& dest, const vector& src)
{
    const matrix44& m = rl_draw::get_instance()->get_rl_state().get_local_clip_transform();
    dest.rx = m.element(1, 0) * src.y + m.element(0, 0) * src.x + m.element(2, 0) * src.z + m.element(3, 0);
    dest.ry = m.element(1, 1) * src.y + m.element(0, 1) * src.x + m.element(2, 1) * src.z + m.element(3, 1);
    dest.rz = m.element(1, 2) * src.y + m.element(0, 2) * src.x + m.element(2, 2) * src.z + m.element(3, 2);
    const float w = m.element(1, 3) * src.y + m.element(0, 3) * src.x + m.element(2, 3) * src.z + m.element(3, 3);
    dest.w = w;
    unsigned int codes = 0;
    dest.flags = 0;
    if (w < dest.rx)
    {
        codes = 8;
    }
    if (dest.rx < -w)
    {
        codes += 4;
    }
    if (w < dest.ry)
    {
        codes += 0x20;
    }
    if (dest.ry < -w)
    {
        codes += 0x10;
    }
    if (dest.rz <= 0.0f)
    {
        codes += 0x80;
    }
    dest.codes = (unsigned char)codes;
    return codes;
}

// 0x004DDD10 (:420)
// Once (flags 1 projected, 2 rejected): a point at or behind the eye gets flag 2; otherwise the clip point through
// rl_draw's clip-to-screen transform, divided by w (flag 1). Returns the flags.
unsigned int gr_project_vertex(vertex& p)
{
    unsigned int flags = p.flags;
    if ((p.flags & 3) != 0)
    {
        return flags;
    }
    if (p.rz <= 0.0f)
    {
        p.flags = (unsigned char)(flags | 2);
        return flags | 2;
    }
    vector4 v(p.rx, p.ry, p.rz, p.w);
    v.rotate(rl_draw::get_instance()->get_rl_state().get_clip_screen_transform());
    const float inverse_w = 1.0f / v.w;
    flags |= 1;
    p.sx = inverse_w * v.x;
    p.sy = inverse_w * v.y;
    p.sw = inverse_w * v.z;
    p.flags = (unsigned char)flags;
    return flags;
}

// The three corners sharing an edge with each corner of bbox_get_verts (rl_render_lib.cpp has the same table).
static const int Bbox_neighbors[8][3] = // 0x014FA0A0
{
    { 1, 3, 4 }, { 2, 0, 5 }, { 3, 1, 6 }, { 0, 2, 7 }, { 5, 7, 0 }, { 6, 4, 1 }, { 7, 5, 2 }, { 4, 6, 3 },
};

// Not in the binary: a render-offset relative point through rl_draw's local-to-clip transform, then the screen point
// of it (both inlined in gr_get_bbox_2d_rect_robust, in its float order). False when the point is at or behind the
// eye.
static bool gr_bbox_project_point(float& sx, float& sy, const vector& p, const matrix44& clip_screen)
{
    const matrix44& m = rl_draw::get_instance()->get_rl_state().get_local_clip_transform();
    vector4 clip(m.element(1, 0) * p.y + m.element(0, 0) * p.x + m.element(2, 0) * p.z + m.element(3, 0),
                 m.element(0, 1) * p.x + m.element(1, 1) * p.y + m.element(2, 1) * p.z + m.element(3, 1),
                 m.element(0, 2) * p.x + m.element(1, 2) * p.y + m.element(2, 2) * p.z + m.element(3, 2),
                 m.element(0, 3) * p.x + m.element(1, 3) * p.y + m.element(2, 3) * p.z + m.element(3, 3));
    if (clip.z <= 0.0f)
    {
        return false;
    }
    clip.rotate(clip_screen);
    const float inverse_w = 1.0f / clip.w;
    sx = inverse_w * clip.x;
    sy = inverse_w * clip.y;
    return true;
}

// 0x004DE000 (:539)
// The screen rectangle of a render-offset relative box that crosses the near plane: the box corners in front of a
// plane just past the camera's near clip, and where the edges from the corners behind it cross it, projected; the
// rectangle (floored origin, ceiled size) is clamped to screen_rect's size. False when a point still projects behind
// the eye; an empty rectangle when nothing is in front.
static bool gr_get_bbox_2d_rect_robust(const vector& bmin, const vector& bmax, float& x, float& y, float& w, float& h, const rl_rect& screen_rect, const matrix44& clip_screen)
{
    const float screen_w = (float)(screen_rect.m_right - screen_rect.m_left);
    const float screen_h = (float)(screen_rect.m_bottom - screen_rect.m_top);
    float min_x = screen_w;
    float min_y = screen_h;
    float max_x = 0.0f;
    float max_y = 0.0f;
    bool any = false;
    rl_state_manager& state = rl_draw::get_instance()->get_rl_state();
    const rl_camera* camera = state.get_active_camera();
    const vector& offset = state.get_render_offset();
    const vector& fvec = camera->get_rotation().fvec;
    // Magic number: the plane is 0.001 past the near clip plane.
    const float plane_dist = camera->get_near_clip() + 0.001f;
    const vector& eye = camera->get_position();
    const vector plane_point((eye.x - offset.x) + plane_dist * fvec.x, (eye.y - offset.y) + fvec.y * plane_dist, (eye.z - offset.z) + fvec.z * plane_dist);
    plane near_plane;
    near_plane.set(fvec, plane_point);
    vector verts[8];
    bbox_get_verts(verts, bmin, bmax);
    unsigned int behind = 0;
    for (int i = 0; i < 8; i++)
    {
        if (verts[i].y * near_plane.normal.y + verts[i].x * near_plane.normal.x + verts[i].z * near_plane.normal.z + near_plane.offset < 0.0f)
        {
            behind |= 1u << i;
        }
    }
    for (int i = 0; i < 8; i++)
    {
        vector points[3];
        int num_points = 0;
        if ((behind & (1u << i)) == 0)
        {
            points[num_points++] = verts[i];
        }
        else
        {
            for (int k = 0; k < 3; k++)
            {
                const int n = Bbox_neighbors[i][k];
                if ((behind & (1u << n)) != 0)
                {
                    continue;
                }
                line edge;
                edge.point = verts[i];
                edge.dir.set(verts[n].x - verts[i].x, verts[n].y - verts[i].y, verts[n].z - verts[i].z);
                vector crossing;
                ix_line_plane(crossing, edge, near_plane);
                points[num_points++] = crossing;
            }
        }
        for (int p = 0; p < num_points; p++)
        {
            float sx;
            float sy;
            if (!gr_bbox_project_point(sx, sy, points[p], clip_screen))
            {
                return false;
            }
            if (0.0f <= min_x - sx)
            {
                min_x = sx;
            }
            if (0.0f <= min_y - sy)
            {
                min_y = sy;
            }
            if (max_x - sx < 0.0f)
            {
                max_x = sx;
            }
            if (max_y - sy < 0.0f)
            {
                max_y = sy;
            }
            any = true;
        }
    }
    if (!any)
    {
        x = 0.0f;
        y = 0.0f;
        w = 0.0f;
        h = 0.0f;
        return true;
    }
    // Clamped to [0, limit] as rfg.exe compares (v - 0 < 0, then 0 <= v - limit).
    float value = (float)(int)floor((double)min_x);
    value = value - 0.0f < 0.0f ? 0.0f : value;
    x = 0.0f <= value - screen_w ? screen_w : value;
    value = (float)(int)floor((double)min_y);
    value = value - 0.0f < 0.0f ? 0.0f : value;
    y = 0.0f <= value - screen_h ? screen_h : value;
    value = (float)(int)(float)ceil((double)(max_x - x));
    value = value - 0.0f < 0.0f ? 0.0f : value;
    w = 0.0f <= value - (screen_w - x) ? screen_w - x : value;
    value = (float)(int)(float)ceil((double)(max_y - y));
    value = value - 0.0f < 0.0f ? 0.0f : value;
    h = 0.0f <= value - (screen_h - y) ? screen_h - y : value;
    return true;
}

// 0x004DEF30 (:632)
// The screen rectangle of a world box: the silhouette of the box seen from the camera (render-offset relative),
// projected through clip_screen (gr_project_vertex_ex inlined); the whole screen_rect (and false) when there is no
// silhouette; the robust form when a silhouette point projects behind the eye. Original: the maximum is kept as a
// truncated integer while the points are folded in.
static bool gr_get_bbox_2d_rect_ex(const vector& bmin, const vector& bmax, float& x, float& y, float& w, float& h, const rl_rect& screen_rect, const matrix44& clip_screen)
{
    rl_state_manager& state = rl_draw::get_instance()->get_rl_state();
    const vector& offset = state.get_render_offset();
    const vector rel_min(bmin.x - offset.x, bmin.y - offset.y, bmin.z - offset.z);
    const vector rel_max(bmax.x - offset.x, bmax.y - offset.y, bmax.z - offset.z);
    const vector& eye = state.get_active_camera()->get_position();
    const vector rel_eye(eye.x - offset.x, eye.y - offset.y, eye.z - offset.z);
    // Magic number: a box silhouette has at most 6 corners.
    vector silhouette[6];
    const int num_points = bbox_find_silhouette(silhouette, rel_eye, rel_min, rel_max);
    if (num_points < 1)
    {
        x = (float)screen_rect.m_left;
        y = (float)screen_rect.m_top;
        w = (float)(screen_rect.m_right - screen_rect.m_left);
        h = (float)(screen_rect.m_bottom - screen_rect.m_top);
        return false;
    }
    vertex verts[6];
    int max_x = 0;
    int max_y = 0;
    for (int i = 0; i < num_points; i++)
    {
        vertex& v = verts[i];
        gr_rotate_vertex(v, silhouette[i]);
        unsigned char flags = v.flags;
        if ((flags & 3) == 0)
        {
            if (0.0f < v.rz)
            {
                vector4 clip(v.rx, v.ry, v.rz, v.w);
                clip.rotate(clip_screen);
                const float inverse_w = 1.0f / clip.w;
                v.sx = inverse_w * clip.x;
                v.sy = inverse_w * clip.y;
                v.sw = inverse_w * clip.z;
                flags |= 1;
            }
            else
            {
                flags |= 2;
            }
            v.flags = flags;
        }
        if ((flags & 2) != 0)
        {
            return gr_get_bbox_2d_rect_robust(rel_min, rel_max, x, y, w, h, screen_rect, clip_screen);
        }
        if (i == 0)
        {
            x = v.sx;
            y = v.sy;
            max_x = (int)x;
            max_y = (int)v.sy;
            continue;
        }
        if (0.0f <= x - v.sx)
        {
            x = v.sx;
        }
        if (0.0f <= y - v.sy)
        {
            y = v.sy;
        }
        max_x = (int)((float)max_x - v.sx < 0.0f ? v.sx : (float)max_x);
        max_y = (int)((float)max_y - v.sy < 0.0f ? v.sy : (float)max_y);
    }
    w = (float)max_x - x;
    h = (float)max_y - y;
    return true;
}

// 0x004DF4F0 (:687)
// gr_get_bbox_2d_rect_ex on the whole screen and rl_draw's clip-to-screen transform.
bool gr_get_bbox_2d_rect(const vector& bmin, const vector& bmax, float& x, float& y, float& w, float& h)
{
    const rl_screen_info* screen = rl_render_lib::get_instance()->get_screen_info();
    const rl_rect screen_rect(0, 0, screen->get_screen_width(), screen->get_screen_height());
    return gr_get_bbox_2d_rect_ex(bmin, bmax, x, y, w, h, screen_rect, rl_draw::get_instance()->get_rl_state().get_clip_screen_transform());
}
