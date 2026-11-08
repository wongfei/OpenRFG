// Original: volition/render_lib/code/rl_camera.cpp
//
// rl_camera: a scene entity with a perspective or orthographic projection. render_begin makes it the renderer's active
// camera and sets up the view (the camera axes in the columns, relative to the render offset) and projection (D3D,
// depth 0..1) transforms, the eye position and the view frustums (full and high LOD far clip). The vertical field of
// view is in degrees. The header-inline members of rl_camera.h are defined here as well.

#include "volition/render_lib/code/rl_camera.h"

#include "volition/render_lib/code/rl_factory_rtti.h"
#include "volition/render_lib/code/rl_gpu_visibility.h"
#include "volition/render_lib/code/rl_render_lib.h"
#include "volition/render_lib/code/rl_renderer.h"
#include "volition/render_lib/code/rl_state_manager.h"

#include <math.h>

// <rl_camera::release> 0x004BA9E0 (:8)
// <rl_camera::create> 0x00548E70 (:8)
RL_DYNAMIC_CLASS_IMPLEMENTATION(rl_camera)

// Magic numbers: degrees to radians and back as the binary stores them (float precision in double constants).
static const double Deg_to_rad = 0.01745329238474369; // 0x012BFA28 (3F91DF3300000000)
static const double Rad_to_deg = 57.29578018188477; // 0x012BFD30 (404CA5DDC0000000)

// 0x005374C0 (:28)
// Magic numbers: 45 degrees, clip planes 0.01..10, motion blur 0.35, the radial blur centred on the screen.
// Notes (decompiled behaviour, kept as is): the focus distances are left uninitialized.
rl_camera::rl_camera()
{
    m_fov = 45.0f;
    m_near_clip = 0.01f;
    m_far_clip = 10.0f;
    m_high_lod_far_clip = 10.0f;
    m_motion_blur_amount = 0.35f;
    m_type = RLCT_PERSPECTIVE;
    m_motion_blur_enabled = false;
    m_use_pixel_aspect_ratio = true;
    m_depth_of_field_enabled = false;
    m_motion_blur_type = MOTION_BLUR_TYPE_CAMERA;
    m_radial_blur_radius = 0.0f;
    m_radial_blur_position.x = 0.5f;
    m_radial_blur_position.y = 0.5f;
    m_frame_override_blur = 0.0f;
    m_signal_noise_scale = 0.0f;
    m_signal_noise_frequency = 0.0f;
}

// 0x00537610 (:32)
rl_camera::~rl_camera()
{
}

// 0x004DFB20 (:43)
// The horizontal field of view (degrees) for the screen's aspect ratio. use_pixel_aspect_ratio is not used.
float calculate_horizontal_fov_perspective(float vertical_fov, bool use_pixel_aspect_ratio)
{
    const float half_tan = (float)tan((double)(float)((double)vertical_fov * Deg_to_rad * 0.5));
    const float scaled = half_tan * rl_render_lib::get_instance()->get_screen_info()->get_aspect_ratio();
    const float half_angle = (float)atan((double)scaled);
    return (float)((double)(half_angle + half_angle) * Rad_to_deg);
}

// 0x004DFB80 (:62)
// The horizontal size of the orthographic view: the vertical size times the aspect ratio. use_pixel_aspect_ratio is
// not used.
static float calculate_horizontal_fov_ortho(float vertical_fov, bool use_pixel_aspect_ratio)
{
    return rl_render_lib::get_instance()->get_screen_info()->get_aspect_ratio() * vertical_fov;
}

// 0x00520D70 (:84)
// The view transform (setup_view, inlined) and a D3D perspective projection: x scale 1 / tan(hfov / 2), y scale
// 1 / tan(vfov / 2), z mapped to 0..1 between the clip planes.
void rl_camera::setup_perspective(rl_renderer& renderer)
{
    setup_view(m_view_transform, renderer.get_render_offset());
    const float near_clip = m_near_clip;
    const float q = m_far_clip / (m_far_clip - near_clip);
    const float tan_vertical = (float)tan((double)(float)((double)m_fov * Deg_to_rad * 0.5));
    const float horizontal_fov = calculate_horizontal_fov_perspective(m_fov, m_use_pixel_aspect_ratio);
    const float tan_horizontal = (float)tan((double)(float)((double)horizontal_fov * Deg_to_rad * 0.5));
    m_projection_transform = matrix44(vector4(1.0f / tan_horizontal, 0.0f, 0.0f, 0.0f), vector4(0.0f, 1.0f / tan_vertical, 0.0f, 0.0f), vector4(0.0f, 0.0f, q, 1.0f), vector4(0.0f, 0.0f, -q * near_clip, 0.0f));
    renderer.set_3d_transforms(m_view_transform, m_projection_transform);
    const matrix43& world = get_world_transform();
    m_frustum.init(world.m_translation, world.m_rotation, m_projection_transform, m_near_clip, m_far_clip);
    m_high_lod_frustum.init(world.m_translation, world.m_rotation, m_projection_transform, m_near_clip, m_high_lod_far_clip);
}

// 0x005211C0 (:111)
// The view transform and an orthographic projection m_fov high and m_fov * aspect wide.
void rl_camera::setup_ortho(rl_renderer& renderer)
{
    setup_view(m_view_transform, renderer.get_render_offset());
    const float height = m_fov;
    const float width = rl_render_lib::get_instance()->get_screen_info()->get_aspect_ratio() * height;
    const float near_clip = m_near_clip;
    const float depth = m_far_clip - near_clip;
    m_projection_transform = matrix44(vector4(1.0f / width, 0.0f, 0.0f, 0.0f), vector4(0.0f, 1.0f / height, 0.0f, 0.0f), vector4(0.0f, 0.0f, 1.0f / depth, 0.0f), vector4(0.0f, 0.0f, -near_clip / depth, 1.0f));
    renderer.set_3d_transforms(m_view_transform, m_projection_transform);
    const matrix43& world = get_world_transform();
    m_frustum.init_ortho(world.m_translation, world.m_rotation, width, m_fov, m_near_clip, m_far_clip);
    m_high_lod_frustum.init_ortho(world.m_translation, world.m_rotation, width, m_fov, m_near_clip, m_high_lod_far_clip);
}

// 0x004BAA00 (:136)
// The world-to-view transform with the camera position taken relative to offset (row vectors: the camera axes are the
// columns).
void rl_camera::setup_view(matrix44& view_matrix, const vector& offset) const
{
    const matrix& orient = get_rotation();
    const vector& position = get_position();
    const float px = position.x - offset.x;
    const float py = position.y - offset.y;
    const float pz = position.z - offset.z;
    view_matrix = matrix44(vector4(orient.rvec.x, orient.uvec.x, orient.fvec.x, 0.0f), vector4(orient.rvec.y, orient.uvec.y, orient.fvec.y, 0.0f), vector4(orient.rvec.z, orient.uvec.z, orient.fvec.z, 0.0f),
                           vector4(-(orient.rvec.x * px + orient.rvec.y * py + orient.rvec.z * pz), -(orient.uvec.y * py + orient.uvec.x * px + orient.uvec.z * pz), -(orient.fvec.y * py + orient.fvec.x * px + orient.fvec.z * pz), 1.0f));
}

// 0x00537660 (:146)
void rl_camera::render_begin(rl_renderer& renderer)
{
    renderer.set_active_camera(this);
    render_refresh(renderer);
}

// 0x005215D0 (:155)
// The transforms and frustums, then the eye position relative to the render offset (rl_renderer::set_eye_position,
// inlined).
void rl_camera::render_refresh(rl_renderer& renderer)
{
    if (m_type == RLCT_PERSPECTIVE)
    {
        setup_perspective(renderer);
    }
    else
    {
        setup_ortho(renderer);
    }
    const vector& offset = renderer.get_render_offset();
    const vector& position = get_position();
    renderer.set_eye_position(vector(position.x - offset.x, position.y - offset.y, position.z - offset.z));
}

// 0x00509F10 (:166)
void rl_camera::render_end(rl_renderer& renderer)
{
    renderer.set_active_camera(nullptr);
}

// 0x004DFBA0 (:171)
void rl_camera::get_fovs(float& horizontal_fov, float& vertical_fov) const
{
    vertical_fov = m_fov;
    if (m_type == RLCT_PERSPECTIVE)
    {
        horizontal_fov = calculate_horizontal_fov_perspective(m_fov, m_use_pixel_aspect_ratio);
        return;
    }
    horizontal_fov = calculate_horizontal_fov_ortho(m_fov, m_use_pixel_aspect_ratio);
}

// 0x007C1810 (.h:247)
const matrix44& rl_camera::get_view_transform() const
{
    return m_view_transform;
}

// 0x004DD700 (.h:252)
matrix44 rl_camera::get_game_view_transform(const vector& offset) const
{
    matrix44 view_matrix;
    setup_view(view_matrix, offset);
    return view_matrix;
}

// 0x004B8B30 (.h:259)
const matrix44& rl_camera::get_projection_transform() const
{
    return m_projection_transform;
}

// 0x007C17D0 (.h:163)
void rl_camera::set_fov(float fov)
{
    m_fov = fov;
}

float rl_camera::get_fov() const
{
    return m_fov;
}

// 0x004B8AA0 (.h:174)
void rl_camera::set_near_clip(float near_clip)
{
    m_near_clip = near_clip;
}

// 0x004B8AC0 (.h:179)
float rl_camera::get_near_clip() const
{
    return m_near_clip;
}

// 0x004B8AD0 (.h:185)
void rl_camera::set_far_clip(float far_clip)
{
    m_far_clip = far_clip;
}

// 0x004B8AF0 (.h:190)
float rl_camera::get_far_clip() const
{
    return m_far_clip;
}

// 0x007C17F0 (.h:195)
void rl_camera::set_high_lod_far_clip(float far_clip)
{
    m_high_lod_far_clip = far_clip;
}

// 0x004B8B00 (.h:200)
float rl_camera::get_high_lod_far_clip() const
{
    return m_high_lod_far_clip;
}

// 0x004B8B10 (.h:205)
const rl_view_frustum& rl_camera::get_frustum() const
{
    return m_frustum;
}

// 0x004B8B20 (.h:220)
const rl_view_frustum& rl_camera::get_high_lod_frustum() const
{
    return m_high_lod_frustum;
}

// 0x004B8BA0 (.h:363)
float rl_camera::get_focus_start_A() const
{
    return m_focus_start_A;
}

// 0x007C18D0 (.h:368)
void rl_camera::set_focus_start_A(float focus_start_A)
{
    m_focus_start_A = focus_start_A;
}

// 0x004B8BB0 (.h:373)
float rl_camera::get_focus_start_B() const
{
    return m_focus_start_B;
}

// 0x007C18F0 (.h:378)
void rl_camera::set_focus_start_B(float focus_start_B)
{
    m_focus_start_B = focus_start_B;
}

// 0x004B8BC0 (.h:384)
float rl_camera::get_focus_end_A() const
{
    return m_focus_end_A;
}

// 0x007C1910 (.h:389)
void rl_camera::set_focus_end_A(float focus_end_A)
{
    m_focus_end_A = focus_end_A;
}

// 0x004B8BD0 (.h:395)
float rl_camera::get_focus_end_B() const
{
    return m_focus_end_B;
}

// 0x007C1930 (.h:400)
void rl_camera::set_focus_end_B(float focus_end_B)
{
    m_focus_end_B = focus_end_B;
}

// 0x004B8B40 (.h:287)
bool rl_camera::get_depth_of_field_enabled() const
{
    return m_depth_of_field_enabled;
}

// 0x007C1820 (.h:292)
void rl_camera::set_depth_of_field_enabled(bool depth_of_field_enabled)
{
    m_depth_of_field_enabled = depth_of_field_enabled;
}

bool rl_camera::get_motion_blur_enabled() const
{
    return m_motion_blur_enabled;
}

// 0x007C1840 (.h:314)
void rl_camera::set_motion_blur_enabled(bool motion_blur_enabled)
{
    m_motion_blur_enabled = motion_blur_enabled;
}

// 0x004B8B50 (.h:304)
rl_camera::motion_blur_type rl_camera::get_motion_blur_type() const
{
    return m_motion_blur_type;
}

// 0x007C1830 (.h:309)
void rl_camera::set_motion_blur_type(motion_blur_type motion_blur_type)
{
    m_motion_blur_type = motion_blur_type;
}

// 0x004B8B60 (.h:319)
float rl_camera::get_motion_blur_amount() const
{
    return m_motion_blur_amount;
}

// 0x007C1850 (.h:324)
void rl_camera::set_motion_blur_amount(float motion_blur_amount)
{
    m_motion_blur_amount = motion_blur_amount;
}

// 0x004B8B70 (.h:330)
float rl_camera::get_radial_blur_radius() const
{
    return m_radial_blur_radius;
}

// 0x007C1870 (.h:335)
void rl_camera::set_radial_blur_radius(float radial_blur_radius)
{
    m_radial_blur_radius = radial_blur_radius;
}

// 0x004B8B80 (.h:341)
const vector2& rl_camera::get_radial_blur_position() const
{
    return m_radial_blur_position;
}

// 0x007C1890 (.h:346)
void rl_camera::set_radial_blur_position(const vector2& radial_blur_position)
{
    m_radial_blur_position = radial_blur_position;
}

// 0x007C18B0 (.h:352)
void rl_camera::set_frame_override_blur(float blur_amount)
{
    m_frame_override_blur = blur_amount;
}

// 0x004B8B90 (.h:357)
float rl_camera::get_frame_override_blur()
{
    return m_frame_override_blur;
}

// 0x004B8BF0 (.h:417)
float rl_camera::get_signal_noise_frequency() const
{
    return m_signal_noise_frequency;
}

// 0x007C1970 (.h:422)
void rl_camera::set_signal_noise_frequency(float signal_noise_frequency)
{
    m_signal_noise_frequency = signal_noise_frequency;
}

// 0x004B8BE0 (.h:406)
float rl_camera::get_signal_noise_scale() const
{
    return m_signal_noise_scale;
}

// 0x007C1950 (.h:411)
void rl_camera::set_signal_noise_scale(float signal_noise_scale)
{
    m_signal_noise_scale = signal_noise_scale;
}

void rl_camera::set_type(rl_camera_type type)
{
    m_type = type;
}

rl_camera::rl_camera_type rl_camera::get_type() const
{
    return m_type;
}
