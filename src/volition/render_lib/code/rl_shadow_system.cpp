// Original: volition/render_lib/code/rl_shadow_system.cpp
//
// The CLB (cascaded light buffer) shadow maps of the sun: each cascade covers a slice of the view between
// Shadow_map_range_pcts * g_shadowMapMaxDist (near 0-12.5 m, mid 12.5-41.5 m, far 41.5-100 m with 10% blending into
// the neighbours, and the terrain-only extension to g_terrainShadowsMaxDist). build_shadow_params_base fits an
// orthographic light projection around the slice (uniform shadow maps, the retail Rl_shadow_map_type) and gives the
// shadow map creation matrix and the projection (texture) matrix; build_shadow_culling_frustum gives the volume of the
// casters of a slice.
//
// Partial: the perspective, light-space perspective, trapezoidal and fixed-size uniform projections (dev console
// choices, Rl_shadow_map_type and g_shadowMapUniformFixedSize), drop shadows, the lighting pass parameters and the
// debug rendering are not ported; they print "not ported".

#include "volition/render_lib/code/rl_shadow_system.h"

#include "volition/render_lib/code/rl_aabb.h"
#include "volition/render_lib/code/rl_camera.h"
#include "volition/render_lib/code/rl_factory_rtti.h"
#include "volition/render_lib/code/rl_frustum.h"
#include "volition/render_lib/code/rl_light.h"
#include "volition/render_lib/code/rl_render_lib.h"
#include "volition/render_lib/code/rl_renderer.h"
#include "volition/render_lib/code/rl_shadow_system_helpers.h"
#include "volition/render_lib/code/rl_sphere.h"
#include "volition/vlib/code/math/floating.h"
#include "volition/vlib/code/math/plane.h"
#include "keen/base/trace.hpp"

#include <math.h>

float calculate_horizontal_fov_perspective(float vertical_fov, bool use_pixel_aspect_ratio); // 0x004DFB20 (rl_camera.cpp)

extern float g_shadowMapMaxDist; // 0x014FA510 (rl_scene_renderer.cpp)
extern float g_shadowMapFadePct; // 0x014FA514 (rl_scene_renderer.cpp)
extern float g_shadowPercent; // 0x014FA540 (rl_scene_renderer.cpp)
extern float g_terrainShadowsMaxDist; // 0x014FA550 (rl_scene_shadows.cpp)

// :371
// <rl_shadow_system::release> 0x004C8A90 (:371)
// <rl_shadow_system::create> 0x00518720 (:371)
RL_DYNAMIC_CLASS_IMPLEMENTATION(rl_shadow_system)

float rl_shadow_system::m_z_epsilon = 0.001f; // 0x014FA508
float rl_shadow_system::m_perspective_degenerate_threshold = 0.008f; // 0x014FA50C
float rl_shadow_system::m_shadow_map_min_inf_z = 1.25f; // 0x014FA518
float rl_shadow_system::m_shadow_map_max_inf_z = 2.25f; // 0x014FA51C
float rl_shadow_system::m_shadow_map_near_clip_hack = 0.6f; // 0x014FA520
bool rl_shadow_system::m_shadow_map_slideback = true; // 0x014FA524
bool rl_shadow_system::m_shadow_map_16_sample = true; // 0x014FA525
bool rl_shadow_system::m_shadow_map_pssm = true; // 0x014FA526
bool rl_shadow_system::m_terrain_casts_shadows = true; // 0x014FA527
float rl_shadow_system::m_shadow_map_slope_zbias_perspective = -2.0f; // 0x014FA528
float rl_shadow_system::m_shadow_map_zbias_perspective = 5.0e-5f; // 0x014FA52C
float rl_shadow_system::m_shadow_map_pssm_split_weight = 0.5f; // 0x014FA530
float rl_shadow_system::m_shadow_range_blending_pct = 0.1f; // 0x014FA534
float rl_shadow_system::m_shadow_map_slope_zbias_tsm = -4.0f; // 0x014FA538
float rl_shadow_system::m_shadow_map_zbias_tsm = 1.0e-4f; // 0x014FA53C
float rl_shadow_system::m_drop_shadow_forward_shift = 5.0f; // 0x014FA544
bool rl_shadow_system::m_terrain_casts_shadows_extended = true; // 0x014FA54C
rl_shadow_mode rl_shadow_system::m_shadow_mode = RLSM_CLB; // 0x014FA554
rl_light* rl_shadow_system::m_drop_shadow_light = nullptr; // 0x01B7BD28
keen::TextureData* rl_shadow_system::m_pCloudTexture = nullptr; // 0x01B7BD30
keen::float4 rl_shadow_system::m_cloudUVScroll; // 0x01B7BC10

// The [start, end] share of g_shadowMapMaxDist of each rl_shadow_map_range (near, mid, far, full, terrain extended;
// the terrain range uses the far end and g_terrainShadowsMaxDist instead). 0.415f is 0x3ED4FDF4.
float Shadow_map_range_pcts[NUM_SHADOW_MAP_RANGES][2] = { { 0.0f, 0.125f }, { 0.125f, 0.415f }, { 0.415f, 1.0f }, { 0.0f, 1.0f }, { 1.0f, 1.0f } }; // 0x014FA558
static const float Shadow_map_range_pcts_orig[NUM_SHADOW_MAP_RANGES][2] = { { 0.0f, 0.125f }, { 0.125f, 0.415f }, { 0.415f, 1.0f }, { 0.0f, 1.0f }, { 1.0f, 1.0f } }; // 0x014FA580

// The shadow LOD (the projector's sample pattern) and depth biases of each rl_shadow_map_pass. -0.06f is 0xBD75C28F.
rl_shadow_pass_parameters Shadow_pass_parameters[NUM_SHADOW_MAP_PASSES] = { // 0x014FA5A8
    { 1, -0.06f, -2.0f },
    { 1, -0.06f, -2.0f },
    { 1, -0.06f, -2.0f },
    { 3, -0.06f, -2.0f },
    { 1, -0.06f, -16.0f },
};

static bool g_shadowMapStabilize = false; // 0x01B7BD2D
static bool g_shadowMapUniformFixedSize = false; // 0x01B7BD2E
static bool RL_test_use_oblique_projection = false; // 0x01B7BD2F
static bool do_compute_sphere = false; // 0x01CD28CC
// The distance of the uniform shadow map's light view from the casters' center (the depth range is twice it).
static float override_bounds = 500.0f; // 0x0151674C

// The blending share clamped to [0, 1].
static float clamped_blending_pct()
{
    float pct = rl_shadow_system::m_shadow_range_blending_pct;
    if (pct - 0.0f < 0.0f)
    {
        pct = 0.0f;
    }
    if (0.0f <= pct - 1.0f)
    {
        pct = 1.0f;
    }
    return pct;
}

// The near and far distances of a range (the near distance blends into the previous range, the far one into the next;
// the terrain range starts where the far range ends).
static void get_range_distances(rl_shadow_map_range range, float& near_dist, float& far_dist)
{
    far_dist = Shadow_map_range_pcts[range][1] * g_shadowMapMaxDist;
    near_dist = Shadow_map_range_pcts[range][0] * g_shadowMapMaxDist;
    if (range == SMR_TERRAIN_EXTENDED)
    {
        near_dist = Shadow_map_range_pcts[SMR_FAR][1] * g_shadowMapMaxDist;
        far_dist = g_terrainShadowsMaxDist;
    }
    else if (range != SMR_FULL)
    {
        if (range != SMR_FAR)
        {
            far_dist = (Shadow_map_range_pcts[range][1] - Shadow_map_range_pcts[range][0]) * g_shadowMapMaxDist * clamped_blending_pct() + far_dist;
        }
        if (range != SMR_NEAR)
        {
            near_dist = near_dist - (Shadow_map_range_pcts[range - 1][1] - Shadow_map_range_pcts[range - 1][0]) * g_shadowMapMaxDist * clamped_blending_pct();
        }
    }
}

// The camera's view frustum cut to [near_dist, far_dist] along its far plane normal (planes 4 and 5).
static rl_view_frustum range_frustum(const rl_view_frustum& camera_frustum, const vector& camera_pos, float near_dist, float far_dist)
{
    rl_view_frustum frustum = camera_frustum;
    const vector dir = frustum.get_plane(rl_view_frustum::FAR_PLANE).normal;
    plane far_plane;
    far_plane.set(dir, vector(dir.x * far_dist + camera_pos.x, camera_pos.y + dir.y * far_dist, camera_pos.z + dir.z * far_dist));
    frustum.set_plane(rl_view_frustum::FAR_PLANE, far_plane);
    plane near_plane;
    near_plane.set(vector(-dir.x, -dir.y, -dir.z), vector(dir.x * near_dist + camera_pos.x, camera_pos.y + dir.y * near_dist, camera_pos.z + dir.z * near_dist));
    frustum.set_plane(rl_view_frustum::NEAR_PLANE, near_plane);
    return frustum;
}

// 0x004F6340 (:374)
rl_shadow_system::rl_shadow_system()
{
}

// 0x004F6370 (:379)
rl_shadow_system::~rl_shadow_system()
{
}

// 0x004C8AB0 (:384)
void rl_shadow_system::initialize()
{
}

// 0x004C8AC0 (:389)
void rl_shadow_system::shutdown()
{
}

// 0x004C8AE0 (:762)
// An orthographic light view around the casting volume: the view looks along -light_dir from override_bounds
// (500 m) behind the volume's center (up is the world up, or forward when the light is vertical), the x/y extents are
// the transformed points' bounds and the depth covers 0..1000 m. Both biases are 0 (the caller takes the slope bias
// from the pass parameters). With g_shadowMapStabilize the origin is snapped to 1/1024 texel steps.
void rl_shadow_system::build_uniform_shadowmap_matrix(matrix44& uniform_matrix, float& desired_zbias, float& desired_slope_zbias, const vector& light_dir, const rl_point_set& casting_volume)
{
    const bbox& bounds = casting_volume.bounds();
    const vector center((bounds.max.x + bounds.min.x) * 0.5f, (bounds.max.y + bounds.min.y) * 0.5f, (bounds.min.z + bounds.max.z) * 0.5f);
    // The original also measures the bounds' diagonal (vec_dist) and does not use it.
    const vector eye(light_dir.x * override_bounds + center.x, light_dir.y * override_bounds + center.y, light_dir.z * override_bounds + center.z);
    if (RL_test_use_oblique_projection)
    {
        keen::debug::nativePrintArgs("rl_shadow_system::build_uniform_shadowmap_matrix: the oblique projection test is not ported\n");
    }
    vector up = UP_VECTOR;
    const float up_dot = fabsf(light_dir.x * UP_VECTOR.x + light_dir.y * UP_VECTOR.y + light_dir.z * UP_VECTOR.z);
    float epsilon = fabsf(up_dot);
    if (epsilon - 1.0f < 0.0f)
    {
        epsilon = 1.0f;
    }
    if (0.0f <= 1.0f - epsilon)
    {
        epsilon = 1.0f;
    }
    epsilon = epsilon * m_z_epsilon;
    if (1.0f - epsilon <= up_dot && up_dot <= epsilon + 1.0f)
    {
        up = FORWARD_VECTOR;
    }
    const vector forward((eye.x - light_dir.x) - eye.x, (eye.y - light_dir.y) - eye.y, (eye.z - light_dir.z) - eye.z);
    matrix orient;
    orient.make_uvec(forward, up);
    matrix44 view;
    view.set(orient.rvec.x, orient.uvec.x, orient.fvec.x, 0.0f,
             orient.rvec.y, orient.uvec.y, orient.fvec.y, 0.0f,
             orient.rvec.z, orient.uvec.z, orient.fvec.z, 0.0f,
             -(orient.rvec.x * eye.x + orient.rvec.y * eye.y + orient.rvec.z * eye.z),
             -(orient.uvec.x * eye.x + orient.uvec.y * eye.y + orient.uvec.z * eye.z),
             -(orient.fvec.x * eye.x + orient.fvec.y * eye.y + orient.fvec.z * eye.z), 1.0f);
    vector light_min(3.4028235e+38f, 3.4028235e+38f, 3.4028235e+38f);
    vector light_max(-3.4028235e+38f, -3.4028235e+38f, -3.4028235e+38f);
    for (const vector* p = casting_volume.begin(); p != casting_volume.end(); p++)
    {
        vector4 v(p->x, p->y, p->z, 1.0f);
        v.rotate(view);
        const float inv_w = 1.0f / v.w;
        bbox_expand(light_min, light_max, vector(inv_w * v.x, inv_w * v.y, inv_w * v.z));
    }
    // Depth 0..2 * override_bounds.
    const float near_z = 0.0f;
    const float far_z = override_bounds * 2.0f;
    const float scale_z = 1.0f / (far_z - near_z);
    const float scale_x = 2.0f / (light_max.x - light_min.x);
    const float scale_y = 2.0f / (light_max.y - light_min.y);
    const float offset_x = (light_min.x + light_max.x) / (light_min.x - light_max.x);
    const float offset_y = (light_min.y + light_max.y) / (light_min.y - light_max.y);
    const float offset_z = near_z / (near_z - far_z);
    matrix44 projection;
    projection.set(scale_x, 0.0f, 0.0f, 0.0f, 0.0f, scale_y, 0.0f, 0.0f, 0.0f, 0.0f, scale_z, 0.0f, offset_x, offset_y, offset_z, 1.0f);
    desired_zbias = 0.0f;
    desired_slope_zbias = 0.0f;
    uniform_matrix = view * projection;
    if (g_shadowMapStabilize)
    {
        // Magic number: 1024 steps per unit of the projected origin.
        vector4 origin(0.0f, 0.0f, 0.0f, 1.0f);
        origin.rotate(uniform_matrix);
        origin = origin * 1024.0f;
        const float shift_x = ((float)fl_round(origin.x) - origin.x) * 0.0009765625f;
        const float shift_y = ((float)fl_round(origin.y) - origin.y) * 0.0009765625f;
        for (int row = 0; row < 4; row++)
        {
            uniform_matrix.element(row, 0) = uniform_matrix.element(row, 0) + shift_x * uniform_matrix.element(row, 3);
            uniform_matrix.element(row, 1) = uniform_matrix.element(row, 1) + shift_y * uniform_matrix.element(row, 3);
        }
    }
}

// 0x004CA790 (:857)
void rl_shadow_system::build_uniform_shadowmap_matrix(matrix44& uniform_matrix, float& desired_zbias, float& desired_slope_zbias, const vector& light_dir, const rl_sphere& view_sphere, const rl_point_set& casting_volume)
{
    keen::debug::nativePrintArgs("rl_shadow_system::build_uniform_shadowmap_matrix (fixed size): not ported\n");
    build_uniform_shadowmap_matrix(uniform_matrix, desired_zbias, desired_slope_zbias, light_dir, casting_volume);
}

// 0x004F84E0 (:645)
void rl_shadow_system::build_perspective_shadowmap_matrix2(rl_renderer& renderer, matrix44& perspective_matrix, float& desired_zbias, float& desired_slope_zbias, const vector& light_dir, const rl_point_set& casting_volume, const rl_camera* camera_p)
{
    keen::debug::nativePrintArgs("rl_shadow_system::build_perspective_shadowmap_matrix2: not ported\n");
    build_uniform_shadowmap_matrix(perspective_matrix, desired_zbias, desired_slope_zbias, light_dir, casting_volume);
}

// 0x004F93E0 (:1247)
void rl_shadow_system::build_lispsm_shadowmap_matrix(rl_renderer& renderer, matrix44& lispsm_matrix, float& desired_zbias, float& desired_slope_zbias, const vector& light_dir, const rl_point_set& casting_volume, const rl_camera* camera_p)
{
    keen::debug::nativePrintArgs("rl_shadow_system::build_lispsm_shadowmap_matrix: not ported\n");
    build_uniform_shadowmap_matrix(lispsm_matrix, desired_zbias, desired_slope_zbias, light_dir, casting_volume);
}

// 0x004CCEF0 (:1058)
void rl_shadow_system::build_tsm_shadowmap_matrix(matrix44& tsm_matrix, float& desired_zbias, float& desired_slope_zbias, const vector& light_dir, const rl_point_set& casting_volume, const vector& view_dir, const vector& view_point, float shadow_min_y, float shadow_max_y)
{
    keen::debug::nativePrintArgs("rl_shadow_system::build_tsm_shadowmap_matrix: not ported\n");
    build_uniform_shadowmap_matrix(tsm_matrix, desired_zbias, desired_slope_zbias, light_dir, casting_volume);
}

// 0x0052EB60 (:1445)
// The shadow map of one range: the casting volume is the camera frustum cut to the range (its 8 corners) or the
// caster box, relative to the batch render offset; the light projection is fitted around it (type). The creation
// matrix is the projection itself with the pass's slope bias; the render (texture) matrix is the projection moved
// toward the light by the pass's world bias, then scaled and biased to texture space. Also resets the renderer's
// camera transforms and eye position (the shadow pass overrides them).
// Notes (decompiled behaviour, kept as is): the creation zbias is always 0 and use_clb_shadows is left 0; the view
// sphere (only read by the fixed-size uniform projection) adds the near distance to every component of its center.
bool rl_shadow_system::build_shadow_params_base(rl_renderer& renderer, rl_shadow_creation_parameters& creation_params, rl_shadow_render_parameters& render_params, rl_shadow_map_type type, rl_camera* camera_p, const rl_light* light_p, const vector& batch_render_offset, float shadow_min_y, float shadow_max_y, rl_shadow_map_pass pass, const rl_aabb* caster_aabb, rl_shadow_map_range desired_range, rl_aabb* casters_bbox_out)
{
    float near_dist;
    float far_dist;
    get_range_distances(desired_range, near_dist, far_dist);
    if (desired_range != SMR_TERRAIN_EXTENDED && desired_range != SMR_FULL && near_dist - camera_p->m_near_clip < 0.0f)
    {
        near_dist = camera_p->m_near_clip;
    }
    rl_view_frustum frustum = range_frustum(camera_p->m_frustum, camera_p->get_position(), near_dist, far_dist);
    rl_aabb casters_bbox;
    if (caster_aabb == nullptr)
    {
        frustum.create_aabb(casters_bbox);
    }
    else
    {
        casters_bbox.min = caster_aabb->min;
        casters_bbox.max = caster_aabb->max;
    }
    const vector light_dir = light_p->m_light_direction;
    // The original also collects the same points without the render offset (unused).
    rl_point_set casting_volume;
    if (caster_aabb == nullptr)
    {
        for (int i = 1; i < 9; i++)
        {
            const vector corner = frustum.get_point(i);
            casting_volume.push_back(vector(corner.x - batch_render_offset.x, corner.y - batch_render_offset.y, corner.z - batch_render_offset.z));
        }
    }
    else
    {
        for (int i = 0; i < 8; i++)
        {
            vector corner;
            bbox_find_ith_vertex(corner, i, caster_aabb->min, caster_aabb->max);
            casting_volume.push_back(vector(corner.x - batch_render_offset.x, corner.y - batch_render_offset.y, corner.z - batch_render_offset.z));
        }
    }

    // The sphere around the range slice.
    // Magic numbers: 0.017453 (degrees to radians) and 57.29583 (radians to degrees), as in the original.
    const float fov = camera_p->m_fov;
    const float aspect = rl_render_lib::get_instance()->get_screen_info()->get_aspect_ratio();
    float horizontal_fov;
    if (camera_p->m_type == rl_camera::RLCT_PERSPECTIVE)
    {
        const float half_angle = (float)atan((double)((float)tan((double)(fov * 0.017453f * 0.5f)) * aspect));
        horizontal_fov = (half_angle + half_angle) * 57.29583f;
    }
    else
    {
        horizontal_fov = aspect * fov;
    }
    const float half_height = (float)tan((double)(fov * 0.017453f * 0.5f)) * far_dist;
    const float half_width = (float)tan((double)(horizontal_fov * 0.017453f * 0.5f)) * far_dist;
    const float depth = far_dist - near_dist;
    const float half_depth = depth * 0.5f;
    const float corner_z = (near_dist + half_depth) - (depth + near_dist);
    rl_sphere view_sphere;
    view_sphere.m_radius = sqrtf((0.0f - half_height) * (0.0f - half_height) + (0.0f - half_width) * (0.0f - half_width) + corner_z * corner_z);
    const matrix& camera_orient = camera_p->get_rotation();
    const vector& camera_pos = camera_p->get_position();
    view_sphere.m_position = vector((half_depth * camera_orient.fvec.x + near_dist + camera_pos.x) - batch_render_offset.x,
                                    (camera_pos.y + half_depth * camera_orient.fvec.y + near_dist) - batch_render_offset.y,
                                    (camera_pos.z + half_depth * camera_orient.fvec.z + near_dist) - batch_render_offset.z);
    if (do_compute_sphere)
    {
        keen::debug::nativePrintArgs("rl_shadow_system::build_shadow_params_base: do_compute_sphere is not ported\n");
    }

    float zbias = 0.0f;
    float slope_zbias = 0.0f;
    if (camera_p->m_type == rl_camera::RLCT_PERSPECTIVE)
    {
        camera_p->setup_perspective(renderer);
    }
    else
    {
        camera_p->setup_ortho(renderer);
    }
    const vector& render_offset = renderer.get_render_offset();
    renderer.set_eye_position(vector(camera_pos.x - render_offset.x, camera_pos.y - render_offset.y, camera_pos.z - render_offset.z));
    matrix44 shadow_matrix;
    switch (type)
    {
    case SMT_UNIFORM:
        if (!g_shadowMapUniformFixedSize)
        {
            build_uniform_shadowmap_matrix(shadow_matrix, zbias, slope_zbias, light_dir, casting_volume);
        }
        else
        {
            build_uniform_shadowmap_matrix(shadow_matrix, zbias, slope_zbias, light_dir, view_sphere, casting_volume);
        }
        slope_zbias = Shadow_pass_parameters[pass].m_slope_zbias;
        break;
    case SMT_PSM:
        build_perspective_shadowmap_matrix2(renderer, shadow_matrix, zbias, slope_zbias, light_dir, casting_volume, camera_p);
        break;
    case SMT_LISPSM:
        build_lispsm_shadowmap_matrix(renderer, shadow_matrix, zbias, slope_zbias, light_dir, casting_volume, camera_p);
        break;
    case SMT_TSM:
        build_tsm_shadowmap_matrix(shadow_matrix, zbias, slope_zbias, light_dir, casting_volume, camera_orient.fvec, vector(camera_pos.x - batch_render_offset.x, camera_pos.y - batch_render_offset.y, camera_pos.z - batch_render_offset.z), shadow_min_y, shadow_max_y);
        break;
    }
    // Clip space to texture space: x and y from [-1, 1] to [0, 1] (y flipped).
    static matrix44 scale_and_bias;
    static bool scale_and_bias_init = false;
    if (!scale_and_bias_init)
    {
        scale_and_bias_init = true;
        scale_and_bias.set(0.5f, 0.0f, 0.0f, 0.0f, 0.0f, -0.5f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.5f, 0.5f, 0.0f, 1.0f);
    }
    creation_params.shadow_zbias = 0.0f;
    creation_params.shadow_slope_zbias = slope_zbias;
    creation_params.shadow_map_matrix = shadow_matrix;
    render_params.shadow_max_dist = g_shadowMapMaxDist;
    render_params.shadow_fade_start = g_shadowMapFadePct;
    render_params.shadows_enabled = 1;
    render_params.use_clb_shadows = 0;
    render_params.shadow_aa_enabled = m_shadow_map_16_sample ? 1 : 0;
    // The world bias: the lookup moves toward the light by -m_world_zbias (a translation before the projection).
    const float world_zbias = -Shadow_pass_parameters[pass].m_world_zbias;
    matrix44 bias;
    bias.set(1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, world_zbias * light_dir.x, world_zbias * light_dir.y, world_zbias * light_dir.z, 1.0f);
    render_params.shadow_percent = g_shadowPercent;
    render_params.shadow_texture_matrix = (bias * shadow_matrix) * scale_and_bias;
    if (casters_bbox_out != nullptr)
    {
        casters_bbox_out->min = casters_bbox.min;
        casters_bbox_out->max = casters_bbox.max;
    }
    return true;
}

// 0x005187B0 (:1399)
// The casters of a range: the camera frustum cut to the range (without the near clip limit of
// build_shadow_params_base) extruded toward the light up to the camera's far clip.
void rl_shadow_system::build_shadow_culling_frustum(rl_frustum& shadow_culling_frustum, const rl_camera* camera_p, const rl_light* light_p, rl_shadow_map_range desired_range)
{
    float near_dist;
    float far_dist;
    get_range_distances(desired_range, near_dist, far_dist);
    const rl_view_frustum frustum = range_frustum(camera_p->m_frustum, camera_p->get_position(), near_dist, far_dist);
    vector light_dir;
    if (light_p->m_light_type == rl_light::LIGHT_TYPE_DIRECTIONAL)
    {
        light_dir = light_p->m_light_direction;
    }
    else
    {
        const vector& light_pos = light_p->get_position();
        const vector& camera_pos = camera_p->get_position();
        light_dir = vector(light_pos.x - camera_pos.x, light_pos.y - camera_pos.y, light_pos.z - camera_pos.z).copy_normalize();
    }
    frustum.create_directional_shadow_cull_frustum(shadow_culling_frustum, light_dir, camera_p->m_far_clip);
}

// 0x004D0BD0 (:1866)
void rl_shadow_system::build_drop_shadow_culling_frustum(rl_frustum& shadow_culling_frustum, const rl_camera* camera_p, const rl_light* light_p)
{
    keen::debug::nativePrintArgs("rl_shadow_system::build_drop_shadow_culling_frustum: not ported\n");
}

// 0x004D0750 (:1784)
// The projector's fade parameters of a range: the near and far distances and the reciprocal fade-in and fade-out
// widths (twice the blended share, at least FLT_MIN). The terrain range fades in like the far range blends and fades
// out over 6 m past g_terrainShadowsMaxDist when that is inside the far clip.
void rl_shadow_system::get_shadow_range_fade_params(float* fade_params, rl_shadow_map_range cur_range, const rl_camera* camera_p) const
{
    // Magic number: 1.1754944e-38 is FLT_MIN.
    const float min_width = 1.1754944e-38f;
    float fade_in_width = min_width;
    float fade_out_width = min_width;
    float far_dist = Shadow_map_range_pcts[cur_range][1] * g_shadowMapMaxDist;
    float near_dist = Shadow_map_range_pcts[cur_range][0] * g_shadowMapMaxDist;
    if (cur_range == SMR_TERRAIN_EXTENDED)
    {
        const float blend = (Shadow_map_range_pcts[SMR_FAR][1] - Shadow_map_range_pcts[SMR_FAR][0]) * g_shadowMapMaxDist * clamped_blending_pct();
        const float width = blend * 2.0f;
        near_dist = near_dist - blend;
        if (0.0f <= width - min_width)
        {
            fade_in_width = width;
        }
        far_dist = g_terrainShadowsMaxDist;
        if (g_terrainShadowsMaxDist < camera_p->m_far_clip)
        {
            // Magic number: 6.0, the fade-out width past the terrain shadow distance.
            fade_out_width = 6.0f;
        }
    }
    else if (cur_range != SMR_FULL)
    {
        const float blend_out = (Shadow_map_range_pcts[cur_range][1] - Shadow_map_range_pcts[cur_range][0]) * g_shadowMapMaxDist * clamped_blending_pct();
        far_dist = far_dist - blend_out;
        fade_out_width = blend_out * 2.0f;
        if (fade_out_width - min_width < 0.0f)
        {
            fade_out_width = min_width;
        }
        if (cur_range == SMR_NEAR)
        {
            fade_in_width = min_width;
        }
        else
        {
            const float blend_in = (Shadow_map_range_pcts[cur_range - 1][1] - Shadow_map_range_pcts[cur_range - 1][0]) * g_shadowMapMaxDist * clamped_blending_pct();
            fade_in_width = blend_in * 2.0f;
            near_dist = near_dist - blend_in;
            if (fade_in_width - min_width < 0.0f)
            {
                fade_in_width = min_width;
            }
        }
    }
    fade_params[0] = near_dist;
    fade_params[1] = far_dist;
    fade_params[2] = 1.0f / fade_in_width;
    fade_params[3] = 1.0f / fade_out_width;
}

// 0x004D0A70 (:1827)
void rl_shadow_system::build_shadow_params_base_clb(rl_shadow_render_parameters& render_params)
{
    render_params.shadow_fade_start = g_shadowMapFadePct;
    render_params.shadow_max_dist = g_shadowMapMaxDist;
    render_params.shadows_enabled = 1;
    render_params.use_clb_shadows = 1;
    render_params.shadow_percent = g_shadowPercent;
}

// 0x004D0AB0 (:1838)
// Resets the range table; with m_shadow_map_pssm the near/mid and mid/far splits mix the uniform and logarithmic
// splits of [1, g_shadowMapMaxDist] by m_shadow_map_pssm_split_weight. No caller in the retail build.
void rl_shadow_system::build_shadow_map_ranges()
{
    for (int i = 0; i < NUM_SHADOW_MAP_RANGES; i++)
    {
        Shadow_map_range_pcts[i][0] = Shadow_map_range_pcts_orig[i][0];
        Shadow_map_range_pcts[i][1] = Shadow_map_range_pcts_orig[i][1];
    }
    if (m_shadow_map_pssm)
    {
        const double range = (double)g_shadowMapMaxDist - 1.0;
        const double uniform_weight = 1.0 - (double)m_shadow_map_pssm_split_weight;
        for (int i = 1; i < 3; i++)
        {
            // Magic number: 3 cascades.
            const float t = (float)((double)i / 3.0);
            const float log_split = (float)pow((double)g_shadowMapMaxDist, (double)t);
            const float uniform_split = (float)((double)t * range + 1.0);
            const float split = (float)(((double)(log_split * m_shadow_map_pssm_split_weight) + (double)uniform_split * uniform_weight) / (double)g_shadowMapMaxDist);
            Shadow_map_range_pcts[i - 1][1] = split;
            Shadow_map_range_pcts[i][0] = split;
        }
    }
}
