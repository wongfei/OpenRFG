// Original: volition/render_lib/code/rl_frustum.h
// Implementation: volition/render_lib/code/rl_frustum.cpp
//
// rl_frustum_base, rl_frustum and rl_view_frustum, culling frustums built from planes or cameras.
#pragma once

#include "volition/vlib/code/math/vector3.h"
#include "volition/vlib/code/math/vector4.h"

class matrix;
class matrix44;
class plane;
class rl_aabb;
class rl_bvolume;
class rl_frustum;
class rl_sphere;

// size 0x210
class rl_frustum_base
{
    // Access inferred: the scene renderer reads and writes these members directly.
    friend class rl_scene_renderer;
public:
    // size 0x20
    struct plane_info
    {
        plane_info(const plane_info& another_plane); // 0x004B8420 (.h:67)
        plane_info(const plane&, bool);
        plane_info(); // 0x004B83F0 (.h:64)

        vector4 m_plane; // 0x00
        int m_neg_verts; // 0x10
        int m_pos_verts; // 0x14
        bool m_active; // 0x18
    };

    // size 0xD4
    struct frustum_cache
    {
        frustum_cache(const frustum_cache& __that); // 0x004C1C10
        frustum_cache(); // 0x004B84D0

        vector m_absPlanes[9]; // 0x00
        int m_numValidCornerPoints; // 0x6C
        vector m_corners[8]; // 0x70
        bool m_dirty; // 0xD0
    };

    rl_frustum_base(const rl_frustum_base& __that); // 0x004E7AA0
    rl_frustum_base(); // 0x004DCEB0 (.h:199)
    ~rl_frustum_base(); // 0x004B84E0 (.h:204)
    void set_plane(int index, const plane& _plane); // 0x004B84F0 (.h:208)
    plane get_plane(int index) const; // 0x004B8570 (.h:217)
    void set_num_active_planes(int num_active_planes); // 0x004B85A0 (.h:224)
    int get_num_active_planes() const; // 0x004B85C0 (.h:231)
    bool is_occluded(const vector& aabb_center, const vector& aabb_extents) const; // 0x004BF1F0 (:408)
    bool is_occluded(const rl_bvolume& bvolume) const; // 0x0050EB30 (:650)
    bool is_occluded(const rl_aabb& aabb) const; // 0x004BF090 (:237)
    bool is_occluded(const rl_sphere& sphere) const; // 0x004BEFE0 (:113)
    bool is_completely_inside(const vector& aabb_center, const vector& aabb_extents) const; // 0x004E5D40 (:522)
    bool is_completely_inside(const rl_aabb& aabb) const; // 0x004BF470 (:1242)
    bool is_completely_inside(const rl_sphere& sphere) const; // 0x004BF3B0 (:929)
    bool is_completely_inside(const vector& point) const; // 0x004BF300 (:918)
    bool is_completely_inside(const rl_bvolume& bvolume) const; // 0x0050F2C0 (:1003)
    void debug_render_frustum(const vector&);
    const plane_info* get_plane_info_array() const; // 0x004B8460 (.h:98)
    const frustum_cache& getFrustumCache() const;
    void update(); // 0x004BF5D0 (:1327)
    bool getCornerPoint(int index, vector& corner) const; // 0x004B8470 (.h:104)

protected:
    int m_num_active_planes; // 0x00
    int m_max_planes; // 0x04
    plane_info m_planes[9]; // 0x10
    frustum_cache m_cache; // 0x130
};

// size 0x220
class rl_view_frustum : public rl_frustum_base
{
public:
    enum plane_id
    {
        LEFT_PLANE = 0,
        TOP_PLANE = 1,
        RIGHT_PLANE = 2,
        BOTTOM_PLANE = 3,
        FAR_PLANE = 4,
        NEAR_PLANE = 5,
    };

    enum constants
    {
        MAX_FRUSTUM_PLANES = 6,
    };

    rl_view_frustum(const rl_view_frustum& __that); // 0x0050F3D0
    rl_view_frustum(); // 0x004DD060 (.h:249)
    ~rl_view_frustum(); // 0x004B85E0 (.h:254)
    void init(const vector& view_point, const matrix& view_orient, float horizontal_fov, float vertical_fov, float near_clip_dist, float far_clip_dist); // 0x004E6810 (:1428)
    void init(const vector& view_point, const matrix& view_orient, const matrix44& projection, float near_clip_dist, float far_clip_dist); // 0x004E5EA0 (:1384)
    void init_ortho(const vector& view_point, const matrix& view_orient, float horizontal_scale, float vertical_scale, float near_clip_dist, float far_clip_dist); // 0x004E71D0 (:1472)
    void set_view_point(const vector& view_point); // 0x004B84B0 (.h:156)
    const vector& get_view_point() const;
    bool create_omni_shadow_cull_frustum(rl_frustum& cull_frustum, const vector& light_pos, float max_radius) const; // 0x004BF650 (:1517)
    void create_directional_shadow_cull_frustum(rl_frustum& cull_frustum, const vector& light_dir, float max_dist) const; // 0x004C0070 (:1607)
    void create_aabb(rl_aabb& frustum_aabb) const; // 0x004C0C00 (:1685)
    vector get_point(int index) const; // 0x004C1350 (:1706)
    rl_view_frustum copy_rotate(const matrix& orient, const vector& pos) const; // 0x00524960 (:1755)
private:
    void update(); // 0x004E5E50 (:1364)

    vector m_view_point; // 0x210
};

// size 0x210
class rl_frustum : public rl_frustum_base
{
public:
    enum constants
    {
        MAX_FRUSTUM_PLANES = 9,
    };

    rl_frustum(const rl_view_frustum& view_frustum); // 0x004E7CF0 (:1774)
    rl_frustum(); // 0x004DD040 (.h:237)
    ~rl_frustum(); // 0x004B85D0 (.h:242)

};
