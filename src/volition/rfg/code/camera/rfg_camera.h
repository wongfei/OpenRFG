// Original: volition/rfg/code/camera/rfg_camera.h
// Implementation: volition/rfg/code/camera/rfg_camera.cpp
//
// The game camera (Rfg_camera): a stack of camera modes, each processed by its module (free, lookaround, third person,
// scripts, cutscenes, turrets, slew), smoothed into the real position, orientation and fov, and collision tested.
// The world streams around the camera position when there is no local player (world::do_frame reads
// Rfg_camera.real_pos), a level load moves the camera to the player start (camera_set_pos), and game_render_setup_frame copies the render members into the main render camera and the
// culling frustum (rfg_camera_render.cpp). Camera_main_scene_p is a file static of rfg_camera.cpp in the binary;
// external here so that a driver without the camera system can create it.
#pragma once

#include "volition/render_lib/code/rl_frustum.h"
#include "volition/rfg/code/camera/camera_cutscene.h"
#include "volition/rfg/code/camera/camera_free.h"
#include "volition/rfg/code/camera/camera_lookaround.h"
#include "volition/rfg/code/camera/camera_ms.h"
#include "volition/rfg/code/camera/camera_third_person.h"
#include "volition/rfg/code/camera/camera_view.h"
#include "volition/rfg/code/control/control_map.h"
#include "volition/vlib/code/math/matrix.h"
#include "volition/vlib/code/math/vector3.h"

class object;
class rl_camera;

enum camera_mode
{
    CAMERA_FREE_MODE = 0,
    CAMERA_SLEW_MODE = 1,
    CAMERA_CURSOR_MODE = 2,
    CAMERA_MINI_SCRIPT_MODE = 3,
    CAMERA_CUTSCENE_SCRIPT_MODE = 4,
    CAMERA_DUMB_MODE = 5,
    CAMERA_DEATH_MODE = 6,
    CAMERA_LOOKAROUND_MODE = 7,
    CAMERA_FIRST_PERSON_MODE = 8,
    CAMERA_TURRET_MODE = 9,
    CAMERA_THIRD_PERSON_MODE = 10,
    CAMERA_TEST_SUITE_MODE = 11,
    CAMERA_SATELLITE_MODE = 12,
    NUM_CAMERA_MODES = 13,
};

// size 0xA30
class rfg_camera
{
public:
    rfg_camera(); // 0x006D07C0 (:758)
    const rl_view_frustum& get_culling_frustum(); // 0x006D4C40 (:761)
    void compute_culling_frustrum(const vector& cull_pos, const matrix& cull_orient, float cull_fov, rl_view_frustum& cull_frustum); // 0x006D0850 (:771)
    ~rfg_camera(); // 0x006C8CA0

    camera_mode mode; // 0x00
    camera_mode mode_stack[8]; // 0x04
    int mode_stack_size; // 0x24
    camera_mode level_mode; // 0x28
    vector real_pos; // 0x2C
    vector ideal_pos; // 0x38
    vector last_pos; // 0x44
    matrix real_orient; // 0x50
    matrix ideal_orient; // 0x74
    matrix last_orient; // 0x98
    float real_fov; // 0xBC
    float ideal_fov; // 0xC0
    float last_fov; // 0xC4
    unsigned int target_handle; // 0xC8
    vector last_target_pos; // 0xCC
    matrix last_target_orient; // 0xD8
    float last_aim_point_dist; // 0xFC
    float last_collision_fraction; // 0x100
    float vertical_velocity; // 0x104
    unsigned int flags; // 0x108
    vector render_pos; // 0x10C
    matrix render_orient; // 0x118
    free_mode_params free_params; // 0x13C
    lookaround_mode_params lookaround_params; // 0x218
    thirdperson_mode_params third_person_params; // 0x25C
    mini_script_params mscript_params; // 0x27C
    cutscene_mode_params cscript_params; // 0x72C
    camera_view_data m_camera_view_data; // 0x764
    bool m_dof_enable; // 0x7E8
    float m_focus_start_A; // 0x7EC
    float m_focus_start_B; // 0x7F0
    float m_focus_end_A; // 0x7F4
    float m_focus_end_B; // 0x7F8
    float m_near_clip_dist; // 0x7FC
    float m_far_clip_dist; // 0x800
    float m_high_lod_far_clip_dist; // 0x804
    bool culling_frustum_valid; // 0x808
private:
    rl_view_frustum m_culling_frustum; // 0x810
};

extern rfg_camera Rfg_camera; // 0x01DE4B50
extern rl_camera* Camera_main_scene_p; // 0x01DE48E4 (file static in the binary)
extern bool Camera_third_person_lock; // 0x01DE48DB (rfg_camera_render.cpp)
extern camera_mode RFG_on_foot_camera_mode; // 0x0164894C, CAMERA_THIRD_PERSON_MODE
extern bool Camera_smoothing; // 0x01DE48EB
extern bool Freeze_camera; // 0x01DE48F2

void unused_dcf_camera_turret_smooth_damp(); // 0x006C8970 (:67)
void unused_dcf_camera_turret_smooth_strength(); // 0x006C89D0 (:78)
void unused_dcf_camera_smoothing_factor(); // 0x006C8A30 (:100)
void unused_dcf_camera_invert_y(); // 0x006C8AD0 (:118)
void unused_dcf_camera_invert_x(); // 0x006C8BC0 (:143)
void camera_system_init(); // 0x006E0C40 (:938)
void camera_system_shutdown(); // 0x006C9160 (:968)
void camera_level_init_post(); // 0x006E0D70 (:984)
void camera_do_frame_pre(); // 0x006C9190 (:1071)
void camera_do_frame(); // 0x006DFFA0 (:1080)
void camera_do_frame_post(); // 0x006D0900 (:1255)
void camera_snap_to_default_pos(); // 0x006C91B0 (:1415)
vector camera_move(const vector& cur_pos, const vector& desired_pos, bool slide); // 0x006D66E0 (:1426)
void camera_start_slew_mode(); // 0x006D09B0 (:1465)
void camera_stop_slew_mode(); // 0x006D09C0 (:1469)
void camera_set_pos(const vector& p); // 0x006C91C0 (:1474)
void camera_set_orient(const matrix& o); // 0x006C91F0 (:1479)
void camera_set_transform(const vector& p, const matrix& o); // 0x006C9280 (:1489)
void camera_start_cursor_mode(); // 0x006D09D0 (:1495)
void camera_stop_cursor_mode(); // 0x006D09E0 (:1499)
void camera_start_dumb_mode(); // 0x006D09F0 (:1504)
void camera_stop_dumb_mode(); // 0x006D0A00 (:1509)
void camera_start_turret_mode(bool smooth); // 0x006D0A10 (:1514)
void camera_stop_turret_mode(bool smooth); // 0x006D0B00 (:1538)
void camera_turret_set_rotation(float pitch, float heading, control_axis_action h_action, control_axis_action v_action); // 0x006C92C0 (:1555)
void camera_push_mode(camera_mode m); // 0x006C9300 (:1579)
void camera_pop_mode(camera_mode m); // 0x006C9390 (:1619)
camera_mode camera_get_mode(); // 0x006C9440 (:1672)
void camera_set_level_mode(camera_mode m, bool smooth); // 0x006C9450 (:1677)
bool camera_mode_is_level_mode(camera_mode m); // 0x006C9510 (:1700)
bool camera_mode_is_on_stack(camera_mode m); // 0x006C9520 (:1714)
void camera_get_xz_orient(vector& fvec, vector& rvec); // 0x006C9550 (:1735)
void camera_freeze(); // 0x006C95A0 (:1752)
void camera_unfreeze(); // 0x006C95B0 (:1761)
bool camera_is_frozen(); // 0x006C95C0 (:1769)
bool process_camera_collision_callback(object* hit_objp, void* info); // 0x006D0BD0 (:1775)
bool camera_collision_test(const vector& start_pos, const vector& end_pos, vector* hit_pos, float radius, unsigned int filter); // 0x006D6CE0 (:1871)
rl_camera* camera_get_main_render_camera(); // 0x006C95D0 (:1950)
