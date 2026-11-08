// Original: volition/render_lib/code/rl_frustum.cpp
//
// rl_frustum_base, rl_view_frustum and rl_frustum: culling volumes of up to 9 planes. A plane value
// n . p + w > 0 is outside. Each plane keeps the indices of the box corners farthest against and along its normal
// (bbox_find_np_vertices), so box tests check a single corner per plane. The view frustum has left, top, right, bottom,
// far and near planes and caches its 8 corner points (update).
//
// Partial: create_omni_shadow_cull_frustum (omni light shadows)
// and debug_render_frustum are not implemented. The header-inline members of rl_frustum.h are defined here as well.

#include "volition/render_lib/code/rl_frustum.h"

#include "volition/render_lib/code/rl_aabb.h"
#include "volition/render_lib/code/rl_bvolume.h"
#include "volition/render_lib/code/rl_sphere.h"
#include "volition/vlib/code/math/bbox.h"
#include "volition/vlib/code/math/matrix.h"
#include "volition/vlib/code/math/plane.h"

#include <math.h>
#include <string.h>

// The custom bounding volume culling of is_occluded(rl_bvolume) (on in retail: the box against the absolute plane
// normals, then against the cached frustum corners).
static bool g_frustumUseCustomBVCulling = true; // 0x014FA135
static bool g_frustumUseCustomBVCullingExtended = true; // 0x014FA136

// The box corner bbox_find_ith_vertex numbers (inlined in the box tests).
static vector aabb_vertex(const rl_aabb& aabb, int index)
{
    vector v;
    bbox_find_ith_vertex(v, index, aabb.min, aabb.max);
    return v;
}

// 0x004B83F0 (.h:64)
rl_frustum_base::plane_info::plane_info()
{
    m_plane.x = 0.0f;
    m_plane.y = 0.0f;
    m_plane.z = 0.0f;
    m_plane.w = 0.0f;
    m_neg_verts = 0;
    m_pos_verts = 0;
    m_active = false;
}

// 0x004B8420 (.h:67)
rl_frustum_base::plane_info::plane_info(const plane_info& another_plane)
{
    m_plane.x = another_plane.m_plane.x;
    m_plane.y = another_plane.m_plane.y;
    m_plane.z = another_plane.m_plane.z;
    m_plane.w = another_plane.m_plane.w;
    m_neg_verts = another_plane.m_neg_verts;
    m_pos_verts = another_plane.m_pos_verts;
    m_active = another_plane.m_active;
}

// 0x004B84D0 (compiler-generated)
rl_frustum_base::frustum_cache::frustum_cache()
{
}

// 0x004C1C10 (compiler-generated)
rl_frustum_base::frustum_cache::frustum_cache(const frustum_cache& __that)
{
    memcpy(this, &__that, sizeof(*this));
}

// 0x004DCEB0 (.h:199)
rl_frustum_base::rl_frustum_base()
{
    m_num_active_planes = 0;
    m_max_planes = 0;
    m_cache.m_dirty = true;
}

// 0x004E7AA0 (compiler-generated)
rl_frustum_base::rl_frustum_base(const rl_frustum_base& __that)
    : m_num_active_planes(__that.m_num_active_planes)
    , m_max_planes(__that.m_max_planes)
    , m_cache(__that.m_cache)
{
    for (int i = 0; i < 9; i++)
    {
        m_planes[i] = __that.m_planes[i];
    }
}

// 0x004B84E0 (.h:204)
rl_frustum_base::~rl_frustum_base()
{
}

// 0x004B84F0 (.h:208)
void rl_frustum_base::set_plane(int index, const plane& _plane)
{
    m_planes[index].m_plane.x = _plane.normal.x;
    m_planes[index].m_plane.y = _plane.normal.y;
    m_planes[index].m_plane.z = _plane.normal.z;
    m_planes[index].m_plane.w = _plane.offset;
    bbox_find_np_vertices(m_planes[index].m_neg_verts, m_planes[index].m_pos_verts, _plane.normal);
    m_cache.m_dirty = true;
}

// 0x004B8570 (.h:217)
plane rl_frustum_base::get_plane(int index) const
{
    plane result;
    result.normal.x = m_planes[index].m_plane.x;
    result.normal.y = m_planes[index].m_plane.y;
    result.normal.z = m_planes[index].m_plane.z;
    result.offset = m_planes[index].m_plane.w;
    return result;
}

// 0x004B85A0 (.h:224)
void rl_frustum_base::set_num_active_planes(int num_active_planes)
{
    m_num_active_planes = num_active_planes & 0xFFFF;
    m_cache.m_dirty = true;
}

// 0x004B85C0 (.h:231)
int rl_frustum_base::get_num_active_planes() const
{
    return m_num_active_planes;
}

// 0x004B8460 (.h:98)
const rl_frustum_base::plane_info* rl_frustum_base::get_plane_info_array() const
{
    return m_planes;
}

const rl_frustum_base::frustum_cache& rl_frustum_base::getFrustumCache() const
{
    return m_cache;
}

// 0x004B8470 (.h:104)
bool rl_frustum_base::getCornerPoint(int index, vector& corner) const
{
    if (index < m_cache.m_numValidCornerPoints)
    {
        corner = m_cache.m_corners[index];
        return true;
    }
    return false;
}

// The plane value n . p + w.
static float plane_value(const vector4& p, const vector& point)
{
    return point.x * p.x + point.y * p.y + point.z * p.z + p.w;
}

// 0x004BEFE0 (:113)
// Outside when the sphere is fully on the outer side of any plane.
bool rl_frustum_base::is_occluded(const rl_sphere& sphere) const
{
    for (int i = 0; i < m_num_active_planes; i++)
    {
        if (sphere.m_radius < plane_value(m_planes[i].m_plane, sphere.m_position))
        {
            return true;
        }
    }
    return false;
}

// 0x004BF090 (:237)
// Outside when the corner farthest against a plane's normal is still on its outer side (or on the plane).
bool rl_frustum_base::is_occluded(const rl_aabb& aabb) const
{
    for (int i = 0; i < m_num_active_planes; i++)
    {
        const vector corner = aabb_vertex(aabb, m_planes[i].m_neg_verts);
        if (0.0f <= m_planes[i].m_plane.y * corner.y + m_planes[i].m_plane.x * corner.x + m_planes[i].m_plane.z * corner.z + m_planes[i].m_plane.w)
        {
            return true;
        }
    }
    return false;
}

// 0x004BF1F0 (:408)
// aabb_extents is the full size of the box.
bool rl_frustum_base::is_occluded(const vector& aabb_center, const vector& aabb_extents) const
{
    const float hx = aabb_extents.x * 0.5f;
    const float hy = aabb_extents.y * 0.5f;
    const float hz = aabb_extents.z * 0.5f;
    const rl_aabb aabb(vector(aabb_center.x - hx, aabb_center.y - hy, aabb_center.z - hz), vector(aabb_center.x + hx, aabb_center.y + hy, aabb_center.z + hz));
    return is_occluded(aabb);
}

// 0x004E5D40 (:522)
bool rl_frustum_base::is_completely_inside(const vector& aabb_center, const vector& aabb_extents) const
{
    const float hx = aabb_extents.x * 0.5f;
    const float hy = aabb_extents.y * 0.5f;
    const float hz = aabb_extents.z * 0.5f;
    const rl_aabb aabb(vector(aabb_center.x - hx, aabb_center.y - hy, aabb_center.z - hz), vector(aabb_center.x + hx, aabb_center.y + hy, aabb_center.z + hz));
    return is_completely_inside(aabb);
}

// Whether count of the cached corner points have the coordinate above (above) or below the value (the extended
// custom culling: the box is outside when all corners are on one side of it on one axis).
static bool all_corners(const rl_frustum_base::frustum_cache& cache, int axis, float value, bool above)
{
    int count = 0;
    for (int i = 0; i < cache.m_numValidCornerPoints; i++)
    {
        const float c = axis == 0 ? cache.m_corners[i].x : (axis == 1 ? cache.m_corners[i].y : cache.m_corners[i].z);
        count += (above ? value < c : c < value) ? 1 : 0;
    }
    return count == cache.m_numValidCornerPoints;
}

// 0x0050EB30 (:650)
// With g_frustumUseCustomBVCulling (retail): the box against each plane through the cached absolute plane normals, then
// (extended) the cached frustum corners against the box faces (outside when all corners are beyond one face).
// Otherwise the sphere test, then the box test.
bool rl_frustum_base::is_occluded(const rl_bvolume& bvolume) const
{
    if (!g_frustumUseCustomBVCulling)
    {
        if (is_occluded(bvolume.m_sphere))
        {
            return true;
        }
        rl_aabb aabb;
        bvolume.get_aabb(aabb);
        return is_occluded(aabb);
    }
    for (int i = 0; i < m_num_active_planes; i++)
    {
        const vector& abs_plane = m_cache.m_absPlanes[i];
        const vector4& p = m_planes[i].m_plane;
        if (abs_plane.x * bvolume.m_half_vec.x + abs_plane.y * bvolume.m_half_vec.y + bvolume.m_half_vec.z * abs_plane.z + -p.y * bvolume.m_sphere.m_position.y + -p.x * bvolume.m_sphere.m_position.x + -p.z * bvolume.m_sphere.m_position.z <= p.w)
        {
            return true;
        }
    }
    if (g_frustumUseCustomBVCullingExtended && m_cache.m_numValidCornerPoints != 0)
    {
        const vector& c = bvolume.m_sphere.m_position;
        const vector& h = bvolume.m_half_vec;
        return all_corners(m_cache, 0, c.x + h.x, true) || all_corners(m_cache, 0, c.x - h.x, false) || all_corners(m_cache, 1, c.y + h.y, true) || all_corners(m_cache, 1, c.y - h.y, false) || all_corners(m_cache, 2, c.z + h.z, true) ||
               all_corners(m_cache, 2, c.z - h.z, false);
    }
    return false;
}

// 0x004BF300 (:918)
bool rl_frustum_base::is_completely_inside(const vector& point) const
{
    for (int i = 0; i < m_num_active_planes; i++)
    {
        if (0.0f <= plane_value(m_planes[i].m_plane, point))
        {
            return false;
        }
    }
    return true;
}

// 0x004BF3B0 (:929)
bool rl_frustum_base::is_completely_inside(const rl_sphere& sphere) const
{
    const int count = m_num_active_planes;
    if (count != 0)
    {
        for (int i = 0; i < count; i++)
        {
            if (-sphere.m_radius <= plane_value(m_planes[i].m_plane, sphere.m_position))
            {
                return false;
            }
        }
    }
    return true;
}

// 0x0050F2C0 (:1003)
bool rl_frustum_base::is_completely_inside(const rl_bvolume& bvolume) const
{
    if (is_completely_inside(bvolume.m_sphere))
    {
        return true;
    }
    rl_aabb aabb;
    bvolume.get_aabb(aabb);
    return is_completely_inside(aabb);
}

// 0x004BF470 (:1242)
// Inside when the corner farthest along each plane's normal is on its inner side.
bool rl_frustum_base::is_completely_inside(const rl_aabb& aabb) const
{
    const int count = m_num_active_planes;
    if (count == 0)
    {
        return true;
    }
    for (int i = 0; i < count; i++)
    {
        const vector corner = aabb_vertex(aabb, m_planes[i].m_pos_verts);
        if (0.0f <= m_planes[i].m_plane.y * corner.y + m_planes[i].m_plane.x * corner.x + m_planes[i].m_plane.z * corner.z + m_planes[i].m_plane.w)
        {
            return false;
        }
    }
    return true;
}

// 0x004BF5D0 (:1327)
// The absolute plane normals of the cache; the corner points are invalidated.
void rl_frustum_base::update()
{
    for (int i = 0; i < m_num_active_planes; i++)
    {
        m_cache.m_absPlanes[i].x = fabsf(m_planes[i].m_plane.x);
        m_cache.m_absPlanes[i].y = fabsf(m_planes[i].m_plane.y);
        m_cache.m_absPlanes[i].z = fabsf(m_planes[i].m_plane.z);
    }
    m_cache.m_dirty = false;
    m_cache.m_numValidCornerPoints = 0;
}

// 0x004DD040 (.h:237)
rl_frustum::rl_frustum()
{
    m_max_planes = 9;
}

// 0x004E7CF0 (:1774)
rl_frustum::rl_frustum(const rl_view_frustum& view_frustum)
{
    m_max_planes = 9;
    memcpy(m_planes, view_frustum.get_plane_info_array(), view_frustum.get_num_active_planes() * sizeof(plane_info));
    m_num_active_planes = view_frustum.get_num_active_planes() & 0xFFFF;
    m_cache.m_dirty = true;
    rl_frustum_base::update();
}

// 0x004B85D0 (.h:242)
rl_frustum::~rl_frustum()
{
}

// 0x004DD060 (.h:249)
rl_view_frustum::rl_view_frustum()
{
    m_view_point = ZERO_VECTOR;
    m_max_planes = 6;
}

// 0x0050F3D0 (compiler-generated)
rl_view_frustum::rl_view_frustum(const rl_view_frustum& __that)
    : rl_frustum_base(__that)
    , m_view_point(__that.m_view_point)
{
}

// 0x004B85E0 (.h:254)
rl_view_frustum::~rl_view_frustum()
{
}

// 0x004B84B0 (.h:156)
void rl_view_frustum::set_view_point(const vector& view_point)
{
    m_view_point = view_point;
}

const vector& rl_view_frustum::get_view_point() const
{
    return m_view_point;
}

// 0x004E5E50 (:1364)
// The base cache, then the 8 corner points (get_point 1..8).
void rl_view_frustum::update()
{
    rl_frustum_base::update();
    m_cache.m_numValidCornerPoints = 8;
    for (int index = 1; index < 9; index++)
    {
        m_cache.m_corners[index - 1] = get_point(index);
    }
}

// The side planes through the view point and the corners of a rectangle one unit ahead (x_size/y_size along rvec/uvec),
// and the near and far planes (shared by both init overloads).
static void init_view_planes(rl_view_frustum& frustum, const vector& view_point, const matrix& view_orient, float x_size, float y_size, float near_clip_dist, float far_clip_dist)
{
    const vector ahead(view_point.x + view_orient.fvec.x, view_orient.fvec.y + view_point.y, view_orient.fvec.z + view_point.z);
    const vector top_right(view_orient.rvec.x * x_size + view_orient.uvec.x * y_size + ahead.x, view_orient.rvec.y * x_size + view_orient.uvec.y * y_size + ahead.y, view_orient.rvec.z * x_size + view_orient.uvec.z * y_size + ahead.z);
    const vector top_left(view_orient.rvec.x * -x_size + view_orient.uvec.x * y_size + ahead.x, view_orient.rvec.y * -x_size + view_orient.uvec.y * y_size + ahead.y, view_orient.rvec.z * -x_size + view_orient.uvec.z * y_size + ahead.z);
    const vector bottom_left(view_orient.rvec.x * -x_size + view_orient.uvec.x * -y_size + ahead.x, view_orient.rvec.y * -x_size + view_orient.uvec.y * -y_size + ahead.y, view_orient.rvec.z * -x_size + view_orient.uvec.z * -y_size + ahead.z);
    const vector bottom_right(view_orient.rvec.x * x_size + view_orient.uvec.x * -y_size + ahead.x, view_orient.rvec.y * x_size + view_orient.uvec.y * -y_size + ahead.y, view_orient.rvec.z * x_size + view_orient.uvec.z * -y_size + ahead.z);
    frustum.set_view_point(view_point);
    frustum.set_num_active_planes(6);
    plane side;
    side.set(view_point, bottom_left, top_left);
    frustum.set_plane(rl_view_frustum::LEFT_PLANE, side);
    side.set(view_point, top_left, top_right);
    frustum.set_plane(rl_view_frustum::TOP_PLANE, side);
    side.set(view_point, top_right, bottom_right);
    frustum.set_plane(rl_view_frustum::RIGHT_PLANE, side);
    side.set(view_point, bottom_right, bottom_left);
    frustum.set_plane(rl_view_frustum::BOTTOM_PLANE, side);
    // Magic number: -1 flips the forward vector for the near plane.
    const vector near_point(view_point.x + view_orient.fvec.x * near_clip_dist, view_point.y + view_orient.fvec.y * near_clip_dist, view_orient.fvec.z * near_clip_dist + view_point.z);
    plane near_plane;
    near_plane.set(vector(view_orient.fvec.x * -1.0f, view_orient.fvec.y * -1.0f, view_orient.fvec.z * -1.0f), near_point);
    frustum.set_plane(rl_view_frustum::NEAR_PLANE, near_plane);
    const vector far_point(view_point.x + view_orient.fvec.x * far_clip_dist, view_point.y + view_orient.fvec.y * far_clip_dist, view_orient.fvec.z * far_clip_dist + view_point.z);
    plane far_plane;
    far_plane.set(view_orient.fvec, far_point);
    frustum.set_plane(rl_view_frustum::FAR_PLANE, far_plane);
}

// 0x004E5EA0 (:1384)
// From a perspective projection: the rectangle half sizes are m22 / m00 and m22 / m11.
// Notes (decompiled behaviour, kept as is): m22 is far / (far - near), not 1, so the side planes are slightly wider
// than the projection's (the planes still pass through the view point, so only their angle changes).
void rl_view_frustum::init(const vector& view_point, const matrix& view_orient, const matrix44& projection, float near_clip_dist, float far_clip_dist)
{
    const float m22 = projection.element(2, 2);
    init_view_planes(*this, view_point, view_orient, m22 / projection.element(0, 0), m22 / projection.element(1, 1), near_clip_dist, far_clip_dist);
    update();
}

// 0x004E6810 (:1428)
// From the fields of view in degrees. Magic number: 0.017453 (degrees to radians, as the binary rounds it).
void rl_view_frustum::init(const vector& view_point, const matrix& view_orient, float horizontal_fov, float vertical_fov, float near_clip_dist, float far_clip_dist)
{
    const float x_size = (float)tan((double)(horizontal_fov * 0.5f * 0.017453f));
    const float y_size = (float)tan((double)(vertical_fov * 0.5f * 0.017453f));
    init_view_planes(*this, view_point, view_orient, x_size, y_size, near_clip_dist, far_clip_dist);
    update();
}

// 0x004E71D0 (:1472)
// An orthographic box: the side planes face out along -rvec, uvec, rvec and -uvec through the edges of the
// horizontal_scale x vertical_scale rectangle one unit ahead.
void rl_view_frustum::init_ortho(const vector& view_point, const matrix& view_orient, float horizontal_scale, float vertical_scale, float near_clip_dist, float far_clip_dist)
{
    const vector ahead(view_point.x + view_orient.fvec.x, view_point.y + view_orient.fvec.y, view_orient.fvec.z + view_point.z);
    const vector top_right(view_orient.rvec.x * horizontal_scale + view_orient.uvec.x * vertical_scale + ahead.x, view_orient.rvec.y * horizontal_scale + view_orient.uvec.y * vertical_scale + ahead.y,
                           view_orient.rvec.z * horizontal_scale + view_orient.uvec.z * vertical_scale + ahead.z);
    const vector top_left(view_orient.rvec.x * -horizontal_scale + view_orient.uvec.x * vertical_scale + ahead.x, view_orient.rvec.y * -horizontal_scale + view_orient.uvec.y * vertical_scale + ahead.y,
                          view_orient.rvec.z * -horizontal_scale + view_orient.uvec.z * vertical_scale + ahead.z);
    const vector bottom_left(view_orient.rvec.x * -horizontal_scale + view_orient.uvec.x * -vertical_scale + ahead.x, view_orient.rvec.y * -horizontal_scale + view_orient.uvec.y * -vertical_scale + ahead.y,
                             view_orient.rvec.z * -horizontal_scale + view_orient.uvec.z * -vertical_scale + ahead.z);
    set_view_point(view_point);
    set_num_active_planes(6);
    plane side;
    side.set(vector(-view_orient.rvec.x, -view_orient.rvec.y, -view_orient.rvec.z), top_left);
    set_plane(LEFT_PLANE, side);
    side.set(view_orient.uvec, top_left);
    set_plane(TOP_PLANE, side);
    side.set(view_orient.rvec, top_right);
    set_plane(RIGHT_PLANE, side);
    side.set(vector(-view_orient.uvec.x, -view_orient.uvec.y, -view_orient.uvec.z), bottom_left);
    set_plane(BOTTOM_PLANE, side);
    const vector near_point(view_point.x + view_orient.fvec.x * near_clip_dist, view_point.y + view_orient.fvec.y * near_clip_dist, view_orient.fvec.z * near_clip_dist + view_point.z);
    plane near_plane;
    near_plane.set(vector(view_orient.fvec.x * -1.0f, view_orient.fvec.y * -1.0f, view_orient.fvec.z * -1.0f), near_point);
    set_plane(NEAR_PLANE, near_plane);
    const vector far_point(view_point.x + view_orient.fvec.x * far_clip_dist, view_point.y + view_orient.fvec.y * far_clip_dist, view_orient.fvec.z * far_clip_dist + view_point.z);
    plane far_plane;
    far_plane.set(view_orient.fvec, far_point);
    set_plane(FAR_PLANE, far_plane);
    update();
}

// The direction of the edge where planes a and b meet (a x b).
static vector edge_direction(const vector4& a, const vector4& b)
{
    return vector(a.y * b.z - a.z * b.y, a.z * b.x - b.z * a.x, b.y * a.x - a.y * b.x);
}

// The directional shadow culling tables: the 16 directed edges of the view pyramid (view point 0, far corners 1-4)
// with their reverse edge, and the edge range of each of the 5 planes (left, top, right, bottom, far).
// size 0x10
struct cull_frustum_edge
{
    int pt1; // 0x00
    int pt2; // 0x04
    int reverse_edge; // 0x08
    bool eliminated; // 0x0C: the edge's plane faces away from the light (written per call)
};

// size 0x8
struct cull_frustum_plane
{
    int start_idx; // 0x00
    int end_idx; // 0x04
};

static cull_frustum_edge Cull_frustum_edges[16] = { // 0x014FA140
    { 0, 3, 11, false }, { 3, 1, 12, false }, { 1, 0, 3, false },
    { 0, 1, 2, false }, { 1, 2, 15, false }, { 2, 0, 6, false },
    { 0, 2, 5, false }, { 2, 4, 14, false }, { 4, 0, 9, false },
    { 0, 4, 8, false }, { 4, 3, 13, false }, { 3, 0, 0, false },
    { 1, 3, 1, false }, { 3, 4, 10, false }, { 4, 2, 7, false }, { 2, 1, 4, false },
};

static const cull_frustum_plane Cull_frustum_planes[5] = { // 0x014FA240
    { 0, 2 }, { 3, 5 }, { 6, 8 }, { 9, 11 }, { 12, 15 },
};

// 0x004C0070 (:1607)
// The volume of everything that can cast a shadow into the view: the view planes facing away from the light, the
// silhouette edges (an edge whose plane faces the light while its reverse edge's plane does not) extruded along the
// light, and a far plane max_dist along the light from the view point.
void rl_view_frustum::create_directional_shadow_cull_frustum(rl_frustum& cull_frustum, const vector& light_dir, float max_dist) const
{
    // The view point and the far corners (the side plane edges cut by the far plane, get_point inlined).
    vector points[5];
    points[0] = m_view_point;
    for (int i = 1; i < 5; i++)
    {
        points[i] = get_point(i);
    }
    int num_planes = 0;
    for (int p = 0; p < 5; p++)
    {
        const vector4& view_plane = m_planes[p].m_plane;
        const bool faces_light = 0.0f < light_dir.y * view_plane.y + view_plane.x * light_dir.x + view_plane.z * light_dir.z;
        for (int e = Cull_frustum_planes[p].start_idx; e <= Cull_frustum_planes[p].end_idx; e++)
        {
            Cull_frustum_edges[e].eliminated = faces_light;
        }
        if (!faces_light)
        {
            plane copy;
            copy.normal = vector(view_plane.x, view_plane.y, view_plane.z);
            copy.offset = view_plane.w;
            cull_frustum.set_plane(num_planes, copy);
            num_planes++;
        }
    }
    for (int e = 0; e < 16; e++)
    {
        const cull_frustum_edge& edge = Cull_frustum_edges[e];
        if (edge.eliminated && !Cull_frustum_edges[edge.reverse_edge].eliminated)
        {
            const vector& pt1 = points[edge.pt1];
            const vector extruded(pt1.x + light_dir.x, pt1.y + light_dir.y, pt1.z + light_dir.z);
            plane silhouette;
            silhouette.set(extruded, pt1, points[edge.pt2]);
            cull_frustum.set_plane(num_planes, silhouette);
            num_planes++;
        }
    }
    const vector far_point(m_view_point.x + max_dist * light_dir.x, light_dir.y * max_dist + m_view_point.y, max_dist * light_dir.z + m_view_point.z);
    plane far_plane;
    far_plane.set(light_dir, far_point);
    cull_frustum.set_plane(num_planes, far_plane);
    cull_frustum.set_num_active_planes(num_planes + 1);
    cull_frustum.update();
}

// 0x004C0C00 (:1685)
// The box around the view point and the four far corners.
void rl_view_frustum::create_aabb(rl_aabb& frustum_aabb) const
{
    frustum_aabb.min = m_view_point;
    frustum_aabb.max = m_view_point;
    for (int index = 1; index <= 4; index++)
    {
        const vector corner = get_point(index);
        bbox_expand(frustum_aabb.min, frustum_aabb.max, corner);
    }
}

// 0x004C1350 (:1706)
// Point 0 is the view point; 1-4 the far corners and 5-8 the near corners, where the side plane edges (top-left,
// top-right, bottom-left, bottom-right) meet the far and near distances; ZERO_VECTOR for other indices.
// Notes (decompiled behaviour, kept as is): the near corners are scaled by the near distance over the edge's
// projection on the far plane normal (not the near plane's), which is the same for an exact frustum.
vector rl_view_frustum::get_point(int index) const
{
    if (index == 0)
    {
        return m_view_point;
    }
    const vector edges[4] = { edge_direction(m_planes[TOP_PLANE].m_plane, m_planes[LEFT_PLANE].m_plane), edge_direction(m_planes[RIGHT_PLANE].m_plane, m_planes[TOP_PLANE].m_plane),
                              edge_direction(m_planes[LEFT_PLANE].m_plane, m_planes[BOTTOM_PLANE].m_plane), edge_direction(m_planes[BOTTOM_PLANE].m_plane, m_planes[RIGHT_PLANE].m_plane) };
    const vector far_normal(m_planes[FAR_PLANE].m_plane.x, m_planes[FAR_PLANE].m_plane.y, m_planes[FAR_PLANE].m_plane.z);
    const float far_distance = 0.0f - (far_normal.y * m_view_point.y + far_normal.x * m_view_point.x + far_normal.z * m_view_point.z + m_planes[FAR_PLANE].m_plane.w);
    const float near_distance = m_planes[NEAR_PLANE].m_plane.y * m_view_point.y + m_planes[NEAR_PLANE].m_plane.x * m_view_point.x + m_planes[NEAR_PLANE].m_plane.z * m_view_point.z + m_planes[NEAR_PLANE].m_plane.w;
    if (index < 1 || index > 8)
    {
        return ZERO_VECTOR;
    }
    const vector& edge = edges[(index - 1) & 3];
    const float scale = (index <= 4 ? far_distance : near_distance) / (far_normal * edge);
    return vector(edge.x * scale + m_view_point.x, edge.y * scale + m_view_point.y, edge.z * scale + m_view_point.z);
}

// 0x00524960 (:1755)
// The frustum moved into the frame orient/pos (plane::copy_rotate per plane, the view point relative to pos and
// rotated).
rl_view_frustum rl_view_frustum::copy_rotate(const matrix& orient, const vector& pos) const
{
    rl_view_frustum result;
    for (int i = 0; i < m_num_active_planes; i++)
    {
        plane p;
        p.normal.x = m_planes[i].m_plane.x;
        p.normal.y = m_planes[i].m_plane.y;
        p.normal.z = m_planes[i].m_plane.z;
        p.offset = m_planes[i].m_plane.w;
        result.set_plane(i, p.copy_rotate(orient, pos));
    }
    vector view_point(m_view_point.x - pos.x, m_view_point.y - pos.y, m_view_point.z - pos.z);
    view_point.rotate(orient);
    result.m_view_point = view_point;
    result.m_num_active_planes = m_num_active_planes & 0xFFFF;
    result.m_cache.m_dirty = true;
    return result;
}
