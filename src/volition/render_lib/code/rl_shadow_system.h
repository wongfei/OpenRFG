// Original: volition/render_lib/code/rl_shadow_system.h
// Implementation: volition/render_lib/code/rl_shadow_system.cpp
//
// rl_shadow_system, the cascaded shadow maps: creation and render parameters, LODs, map ranges and passes. Placement
// of the shadow enums and records inferred.
#pragma once

#include "volition/render_lib/code/rl_base_object.h"
#include "volition/render_lib/code/rl_singleton.h"
#include "volition/vlib/code/math/matrix.h"

class rl_aabb;
struct rl_bounding_cone;
class rl_camera;
class rl_class_dynamic_info;
class rl_frustum;
class rl_light;
class rl_point_set;
class rl_renderer;
struct rl_shadow_creation_parameters;
struct rl_shadow_render_parameters;
class rl_sphere;
class vector;
class vector2;

namespace keen
{
    struct TextureData;
    struct float4;
}

enum rl_shadow_mode
{
    RLSM_STANDARD = 0,
    RLSM_CLB = 1,
};

enum rl_shadow_lod
{
    RL_SHADOW_LOD_OFF = 0,
    RL_SHADOW_LOD_LOW = 1,
    RL_SHADOW_LOD_MED = 2,
    RL_SHADOW_LOD_HIGH = 3,
    RL_NUM_SHADOW_LODS = 4,
};

enum rl_shadow_map_pass
{
    SMP_NEAR = 0,
    SMP_MID = 1,
    SMP_FAR = 2,
    SMP_TERRAIN_EXTENDED = 3,
    SMP_DROP = 4,
    NUM_SHADOW_MAP_PASSES = 5,
};

enum rl_shadow_map_range
{
    SMR_NEAR = 0,
    SMR_MID = 1,
    SMR_FAR = 2,
    SMR_FULL = 3,
    SMR_TERRAIN_EXTENDED = 4,
    NUM_SHADOW_MAP_RANGES = 5,
};

enum rl_shadow_map_type
{
    SMT_UNIFORM = 0,
    SMT_PSM = 1,
    SMT_LISPSM = 2,
    SMT_TSM = 3,
};

// size 0xC
struct rl_shadow_pass_parameters
{
    int m_shadow_lod; // 0x00
    float m_world_zbias; // 0x04
    float m_slope_zbias; // 0x08
};

// size 0xC
class rl_shadow_system : public rl_base_object, public rl_singleton_phoenix<rl_shadow_system>
{
protected:
    rl_shadow_system(); // 0x004F6340 (:374)
    virtual ~rl_shadow_system(); // vtbl+0x00 0x004F6370 (:379)
public:
    static int get_objects_peak();
    static int get_objects_used();
    static rl_shadow_system* create(); // 0x00518720 (:371)
    static bool can_create(); // 0x004C8A80 (:371)
    virtual void release(); // vtbl+0x04 0x004C8A90 (:371)
private:
    void calculate_virtual_camera2(rl_renderer& renderer, matrix44& virtual_camera, const vector& light_dir, const rl_camera* camera_p); // 0x004F7370 (:452)
    void make_bounding_cone2(rl_bounding_cone& bounding_cone, const rl_point_set& casting_volume, const matrix44& trans, const vector& apex); // 0x004F7C90 (:552)
    void build_perspective_shadowmap_matrix2(rl_renderer& renderer, matrix44& perspective_matrix, float& desired_zbias, float& desired_slope_zbias, const vector& light_dir, const rl_point_set& casting_volume, const rl_camera* camera_p); // 0x004F84E0 (:645)
    void build_uniform_shadowmap_matrix(matrix44& uniform_matrix, float& desired_zbias, float& desired_slope_zbias, const vector& light_dir, const rl_sphere& view_sphere, const rl_point_set& casting_volume); // 0x004CA790 (:857)
    void build_uniform_shadowmap_matrix(matrix44& uniform_matrix, float& desired_zbias, float& desired_slope_zbias, const vector& light_dir, const rl_point_set& casting_volume); // 0x004C8AE0 (:762)
    void build_lispsm_shadowmap_matrix(rl_renderer& renderer, matrix44& lispsm_matrix, float& desired_zbias, float& desired_slope_zbias, const vector& light_dir, const rl_point_set& casting_volume, const rl_camera* camera_p); // 0x004F93E0 (:1247)
    void build_uniform_shadowmap_matrix_drop(matrix44& uniform_matrix, float& desired_zbias, float& desired_slope_zbias, const vector& light_pos, const vector& light_dir, const rl_point_set& casting_volume); // 0x004CF770 (:1328)
    void build_tsm_shadowmap_matrix(matrix44& tsm_matrix, float& desired_zbias, float& desired_slope_zbias, const vector& light_dir, const rl_point_set& casting_volume, const vector& view_dir, const vector& view_point, float shadow_min_y, float shadow_max_y); // 0x004CCEF0 (:1058)
public:
    virtual void initialize(); // vtbl+0x08 0x004C8AB0 (:384)
    virtual void shutdown(); // vtbl+0x0C 0x004C8AC0 (:389)
    bool build_shadow_params_base(rl_renderer& renderer, rl_shadow_creation_parameters& creation_params, rl_shadow_render_parameters& render_params, rl_shadow_map_type type, rl_camera* camera_p, const rl_light* light_p, const vector& batch_render_offset, float shadow_min_y, float shadow_max_y, rl_shadow_map_pass pass, const rl_aabb* caster_aabb, rl_shadow_map_range desired_range, rl_aabb* casters_bbox_out); // 0x0052EB60 (:1445)
    void build_shadow_params_base_clb(rl_shadow_render_parameters& render_params); // 0x004D0A70 (:1827)
    bool build_shadow_params_lighting(rl_renderer& renderer, rl_shadow_creation_parameters& creation_params, rl_shadow_render_parameters& render_parameters, const rl_light* light_p); // 0x004F6390 (:403)
    bool build_shadow_params_base_drop(rl_shadow_creation_parameters& creation_params, rl_shadow_render_parameters& render_params, rl_camera* camera_p, const rl_light* light_p, const vector& batch_render_offset, rl_aabb* casters_bbox_out); // 0x004FBA80 (:1893)
    void build_shadow_culling_frustum(rl_frustum& shadow_culling_frustum, const rl_camera* camera_p, const rl_light* light_p, rl_shadow_map_range desired_range); // 0x005187B0 (:1399)
    void build_drop_shadow_culling_frustum(rl_frustum& shadow_culling_frustum, const rl_camera* camera_p, const rl_light* light_p); // 0x004D0BD0 (:1866)
    void get_shadow_range_fade_params(float* fade_params, rl_shadow_map_range cur_range, const rl_camera* camera_p) const; // 0x004D0750 (:1784)
    void build_shadow_map_ranges(); // 0x004D0AB0 (:1838)
    void render_debug(rl_renderer& renderer, rl_light* light_p, rl_shadow_map_range desired_range); // 0x00553EC0 (:1650)
    static bool isDropShadowEnabled(); // 0x004C8830 (:250)

    static rl_class_dynamic_info ms_class_dynamic_info; // 0x01516290
    static bool m_shadow_map_16_sample; // 0x014FA525
    static float m_shadow_map_min_inf_z; // 0x014FA518
    static float m_shadow_map_max_inf_z; // 0x014FA51C
    static float m_shadow_map_max_dist;
    static float m_shadow_map_fade_pct;
    static float m_shadow_map_near_clip_hack; // 0x014FA520
    static bool m_shadow_map_slideback; // 0x014FA524
    static float m_shadow_map_slope_zbias_perspective; // 0x014FA528
    static float m_shadow_map_zbias_perspective; // 0x014FA52C
    static float m_shadow_range_blending_pct; // 0x014FA534
    static float m_z_epsilon; // 0x014FA508
    static rl_shadow_mode m_shadow_mode; // 0x014FA554
    static float m_perspective_degenerate_threshold; // 0x014FA50C
    static bool m_shadow_map_pssm; // 0x014FA526
    static float m_shadow_map_pssm_split_weight; // 0x014FA530
    static float m_tod_shadow_percent;
    static rl_light* m_drop_shadow_light; // 0x01B7BD28
    static float m_drop_shadow_forward_shift; // 0x014FA544
    static float m_shadow_map_slope_zbias_tsm; // 0x014FA538
    static float m_shadow_map_zbias_tsm; // 0x014FA53C
    static bool m_terrain_casts_shadows; // 0x014FA527
    static bool m_terrain_casts_shadows_extended; // 0x014FA54C
    static bool m_shadow_map_uniform_stabilize;
    static bool m_shadow_map_uniform_fixed_size;
    static keen::TextureData* m_pCloudTexture; // 0x01B7BD30
    static keen::float4 m_cloudUVScroll; // 0x01B7BC10
};

// size 0x5C
struct rl_shadow_render_parameters
{
    rl_shadow_render_parameters(); // 0x004BAD60 (.h:95)

    matrix44 shadow_texture_matrix; // 0x00
    unsigned int shadows_enabled; // 0x40
    unsigned int use_clb_shadows; // 0x44
    float shadow_max_dist; // 0x48
    float shadow_fade_start; // 0x4C
    unsigned int shadow_aa_enabled; // 0x50
    float shadow_percent; // 0x54
    float shadow_percent_bias; // 0x58
};

// size 0x48
struct rl_shadow_creation_parameters
{
    rl_shadow_creation_parameters(); // 0x004BADB0 (.h:113)

    matrix44 shadow_map_matrix; // 0x00
    float shadow_zbias; // 0x40
    float shadow_slope_zbias; // 0x44
};

// 0x004BAD60 (.h:95)
// Shadows off: identity texture matrix, max distance 1000, fade start and percent 1.
inline rl_shadow_render_parameters::rl_shadow_render_parameters()
{
    shadow_texture_matrix = IDENTITY_MATRIX44;
    // Magic number: the default shadow distance.
    shadow_max_dist = 1000.0f;
    shadow_fade_start = 1.0f;
    shadow_percent = 1.0f;
    shadows_enabled = 0;
    use_clb_shadows = 0;
    shadow_aa_enabled = 0;
    shadow_percent_bias = 0.0f;
}

// 0x004BADB0 (.h:113)
inline rl_shadow_creation_parameters::rl_shadow_creation_parameters()
{
    shadow_map_matrix = IDENTITY_MATRIX44;
    shadow_zbias = 0.0f;
    shadow_slope_zbias = 0.0f;
}

void unused_dcf_Rl_shadow_map_mode(); // 0x004C8760 (:160)
void unused_dcf_rl_shadow_map_world_zbias(); // 0x004C8840 (:258)
void unused_dcf_rl_shadow_map_slope_zbias(); // 0x004C88C0 (:278)
void intersect(vector2& i, const vector& g0, const vector& g1, const vector& h0, const vector& h1); // 0x004CC650 (:977)
