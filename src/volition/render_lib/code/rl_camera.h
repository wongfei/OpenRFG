// Original: volition/render_lib/code/rl_camera.h
// Implementation: volition/render_lib/code/rl_camera.cpp
//
// rl_camera, a camera: view and projection, FOV, clip planes, frustum and screen mapping.
#pragma once

#include "volition/render_lib/code/rl_frustum.h"
#include "volition/render_lib/code/rl_scene_entity.h"
#include "volition/vlib/code/math/matrix.h"
#include "volition/vlib/code/math/vector2.h"

class rl_class_dynamic_info;
class rl_renderer;
class vector;

// size 0x5E0
class rl_camera : public rl_scene_entity
{
    // Access inferred: the scene renderer reads and writes these members directly.
    friend class rl_scene_renderer;
    // Access inferred from rl_terrain_renderer::render_landmarks_internal.
    friend class rl_terrain_renderer;
    // Access inferred: rl_shadow_system copies the frustum and sets up the camera transforms (inlined accessors).
    friend class rl_shadow_system;
    // Access inferred: rl_particle_renderer::render_distortion sets the camera transforms up again (inlined).
    friend class rl_particle_renderer;
public:
    enum rl_camera_type
    {
        RLCT_PERSPECTIVE = 0,
        RLCT_ORTHO = 1,
        NUM_RL_CAMERA_TYPES = 2,
    };

    enum motion_blur_type
    {
        MOTION_BLUR_TYPE_RADIAL = 0,
        MOTION_BLUR_TYPE_CAMERA = 1,
    };

protected:
    rl_camera(); // 0x005374C0 (:28)
    virtual ~rl_camera(); // vtbl+0x00 0x00537610 (:32)
public:
    static int get_objects_peak();
    static int get_objects_used();
    static rl_camera* create(); // 0x00548E70 (:8)
    static bool can_create(); // 0x004BA9D0 (:8)
    virtual void release(); // vtbl+0x04 0x004BA9E0 (:8)
    void render_begin(rl_renderer& renderer); // 0x00537660 (:146)
    void render_refresh(rl_renderer& renderer); // 0x005215D0 (:155)
    void render_end(rl_renderer& renderer); // 0x00509F10 (:166)
    const matrix44& get_view_transform() const; // 0x007C1810 (.h:247)
    matrix44 get_game_view_transform(const vector& offset) const; // 0x004DD700 (.h:252)
    const matrix44& get_projection_transform() const; // 0x004B8B30 (.h:259)
    void set_fov(float fov); // 0x007C17D0 (.h:163)
    float get_fov() const;
    void get_fovs(float& horizontal_fov, float& vertical_fov) const; // 0x004DFBA0 (:171)
    void set_near_clip(float near_clip); // 0x004B8AA0 (.h:174)
    float get_near_clip() const; // 0x004B8AC0 (.h:179)
    void set_far_clip(float far_clip); // 0x004B8AD0 (.h:185)
    float get_far_clip() const; // 0x004B8AF0 (.h:190)
    void set_high_lod_far_clip(float far_clip); // 0x007C17F0 (.h:195)
    float get_high_lod_far_clip() const; // 0x004B8B00 (.h:200)
    const rl_view_frustum& get_frustum() const; // 0x004B8B10 (.h:205)
    void set_frustum(const rl_view_frustum&);
    const rl_view_frustum& get_high_lod_frustum() const; // 0x004B8B20 (.h:220)
    void set_high_lod_frustum(const rl_view_frustum&);
    float get_focus_start_A() const; // 0x004B8BA0 (.h:363)
    void set_focus_start_A(float focus_start_A); // 0x007C18D0 (.h:368)
    float get_focus_start_B() const; // 0x004B8BB0 (.h:373)
    void set_focus_start_B(float focus_start_B); // 0x007C18F0 (.h:378)
    float get_focus_end_A() const; // 0x004B8BC0 (.h:384)
    void set_focus_end_A(float focus_end_A); // 0x007C1910 (.h:389)
    float get_focus_end_B() const; // 0x004B8BD0 (.h:395)
    void set_focus_end_B(float focus_end_B); // 0x007C1930 (.h:400)
    bool get_depth_of_field_enabled() const; // 0x004B8B40 (.h:287)
    void set_depth_of_field_enabled(bool depth_of_field_enabled); // 0x007C1820 (.h:292)
    bool get_motion_blur_enabled() const;
    void set_motion_blur_enabled(bool motion_blur_enabled); // 0x007C1840 (.h:314)
    motion_blur_type get_motion_blur_type() const; // 0x004B8B50 (.h:304)
    void set_motion_blur_type(motion_blur_type motion_blur_type); // 0x007C1830 (.h:309)
    float get_motion_blur_amount() const; // 0x004B8B60 (.h:319)
    void set_motion_blur_amount(float motion_blur_amount); // 0x007C1850 (.h:324)
    float get_radial_blur_radius() const; // 0x004B8B70 (.h:330)
    void set_radial_blur_radius(float radial_blur_radius); // 0x007C1870 (.h:335)
    const vector2& get_radial_blur_position() const; // 0x004B8B80 (.h:341)
    void set_radial_blur_position(const vector2& radial_blur_position); // 0x007C1890 (.h:346)
    void set_frame_override_blur(float blur_amount); // 0x007C18B0 (.h:352)
    float get_frame_override_blur(); // 0x004B8B90 (.h:357)
    float get_signal_noise_frequency() const; // 0x004B8BF0 (.h:417)
    void set_signal_noise_frequency(float signal_noise_frequency); // 0x007C1970 (.h:422)
    float get_signal_noise_scale() const; // 0x004B8BE0 (.h:406)
    void set_signal_noise_scale(float signal_noise_scale); // 0x007C1950 (.h:411)
    void set_type(rl_camera_type);
    rl_camera_type get_type() const;
    void set_use_pixel_aspect_ratio(bool);
    bool get_use_pixel_aspect_ratio();
private:
    void setup_perspective(rl_renderer& renderer); // 0x00520D70 (:84)
    void setup_ortho(rl_renderer& renderer); // 0x005211C0 (:111)
    void setup_view(matrix44& view_matrix, const vector& offset) const; // 0x004BAA00 (:136)
public:
    rl_camera& operator=(const rl_camera& __that); // 0x0052E170

    static rl_class_dynamic_info ms_class_dynamic_info; // 0x015147C0
private:
    matrix44 m_projection_transform; // 0xC0
    matrix44 m_view_transform; // 0x100
    float m_fov; // 0x140
    rl_view_frustum m_frustum; // 0x150
    rl_view_frustum m_high_lod_frustum; // 0x370
    float m_near_clip; // 0x590
    float m_far_clip; // 0x594
    float m_high_lod_far_clip; // 0x598
    rl_camera_type m_type; // 0x59C
    bool m_use_pixel_aspect_ratio; // 0x5A0
    bool m_depth_of_field_enabled; // 0x5A1
    float m_focus_start_A; // 0x5A4
    float m_focus_start_B; // 0x5A8
    float m_focus_end_A; // 0x5AC
    float m_focus_end_B; // 0x5B0
    bool m_motion_blur_enabled; // 0x5B4
    motion_blur_type m_motion_blur_type; // 0x5B8
    float m_motion_blur_amount; // 0x5BC
    float m_radial_blur_radius; // 0x5C0
    vector2 m_radial_blur_position; // 0x5C4
    float m_frame_override_blur; // 0x5CC
    float m_signal_noise_scale; // 0x5D0
    float m_signal_noise_frequency; // 0x5D4
};

// 0x0052E170
// Compiler-generated: memberwise.
inline rl_camera& rl_camera::operator=(const rl_camera& __that) = default;
