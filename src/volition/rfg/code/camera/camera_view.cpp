// Original: volition/rfg/code/camera/camera_view.cpp
//
// Camera views (player_minimal.md P6): camera.xtbl's views (fov, distance behind the player, depth of field, a camera
// effect), the player's current one (camera_set_view_player by zoom state) and the blend to a new one: the third
// person distance, the fov (Rfg_camera.ideal_fov), the focus on the aim target and the depth of field values the
// renderer reads. Compiled in rfg_camera_unity.cpp. Not ported: the debug console command (unused_dcf_*).
// The blends are float as in the decompile; their double rounding was not checked against the asm.

#include "volition/rfg/code/camera/camera_view.h"

#include "volition/render_lib/code/rl_render_lib.h"
#include "volition/render_lib/code/rl_renderable_instance.h"
#include "volition/render_lib/code/rl_screen_info.h"
#include "volition/rfg/code/camera/camera_free.h"
#include "volition/rfg/code/camera/rfg_camera.h"
#include "volition/rfg/code/effects/effects.h"
#include "volition/rfg/code/effects/effects_shared.h"
#include "volition/rfg/code/effects/object_effect.h"
#include "volition/rfg/code/gameplay/activities/activity_convoy.h"
#include "volition/rfg/code/gamewide/game_render.h"
#include "volition/rfg/code/gamewide/tweak_table.h"
#include "volition/rfg/code/level/world.h"
#include "volition/rfg/code/multi/multi_core/include/multi.h"
#include "volition/rfg/code/objects/humans/human_combat_turret.h"
#include "volition/rfg/code/objects/humans/human_weapons.h"
#include "volition/rfg/code/objects/humans/player.h"
#include "volition/rfg/code/vehicle/automobile.h"
#include "volition/rfg/code/vehicle/vehicle_class.h"
#include "volition/rfg/code/vehicle/vehicle_effects.h"
#include "volition/rfg/code/vehicle/vehicle_info.h"
#include "volition/rfg/code/vehicle/vehicle_system.h"
#include "volition/rfg/code/weapons/weapons.h"
#include "volition/vfxlib/code/vfx_effect.h"
#include "volition/vlib/code/math/floating.h"
#include "volition/vlib/code/memory/vmemory.h"
#include "volition/vlib/code/parse/xml_table_tools.h"
#include "volition/vlib/code/util/string_util.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

void fb_accum_blur_set_enable(bool enable);
void fb_accum_blur_change_percentage(float delta_percentage);

// ---- TU-private types (rfg.pdb; camera_view.h names it by pointer only)
// One camera view of camera.xtbl (Camera_view_table): fov, depth of field and focus parameters.
// size 0x50
struct camera_view_table_entry
{
    char* name; // 0x00
    int change_view_time; // 0x04
    int out_time_override; // 0x08
    int target_focus_speed; // 0x0C
    int terrain_focus_speed; // 0x10
    float fov_min; // 0x14
    float fov_min_sp; // 0x18
    float fov_max; // 0x1C
    float fov_zoom_percent; // 0x20
    float focal_distance_min; // 0x24
    float focal_distance_max; // 0x28
    float focal_range_near; // 0x2C
    float focal_range_far; // 0x30
    float focused_distance_normal; // 0x34
    float focused_distance_zoom; // 0x38
    float focused_dist_target_multiplier; // 0x3C
    float camera_distance; // 0x40
    bool use_dof; // 0x44
    bool use_weapon_fov; // 0x45
    bool use_far_values; // 0x46
    bool use_fov_zoom; // 0x47
    bool override_weapon_zoom; // 0x48
    bool override_weapon_max_range; // 0x49
    unsigned int effect_handle; // 0x4C
};

// ---- file-static globals
static const char CAMERA_VIEW_FILE_NAME[12] = "camera.xtbl"; // 0x012CFAD8
static const char CAMERA_VIEW_ROOT_TAG[12] = "camera_view"; // 0x012CFAE4
static const char CAMERA_VIEW_NAME_TAG[5] = "name"; // 0x012CFAF0
static const char CAMERA_VIEW_CHANGE_VIEW_TIME_TAG[17] = "change_view_time"; // 0x012CFAF8
static const char CAMERA_VIEW_OUT_TIME_OVERRIDE_TAG[18] = "out_time_override"; // 0x012CFB0C
static const char CAMERA_VIEW_DISTANCE_TAG[9] = "distance"; // 0x012CFB20
static const char CAMERA_VIEW_NEAR_FOV_TAG[8] = "fov_min"; // 0x012CFB2C
static const char CAMERA_VIEW_NEAR_DOF_TAG[9] = "near_dof"; // 0x012CFB34
static const char CAMERA_VIEW_USE_WEAPON_FOV_TAG[15] = "use_weapon_fov"; // 0x012CFB40
static const char CAMERA_VIEW_USE_DOF_TAG[8] = "use_dof"; // 0x012CFB50
static const char CAMERA_VIEW_FOCAL_DISTANCE_MIN_TAG[19] = "focal_distance_min"; // 0x012CFB58
static const char CAMERA_VIEW_FOCAL_RANGE_NEAR_TAG[17] = "focal_range_near"; // 0x012CFB6C
static const char CAMERA_VIEW_FOCUSED_DISTANCE_NORMAL_TAG[24] = "focused_distance_normal"; // 0x012CFB80
static const char CAMERA_VIEW_TARGET_FOCUSED_MULTIPLIER_TAG[31] = "focused_dist_target_multiplier"; // 0x012CFB98
static const char CAMERA_VIEW_FAR_DOF_TAG[8] = "far_dof"; // 0x012CFBB8
static const char CAMERA_VIEW_FOV_ZOOM_TAG[9] = "fov_zoom"; // 0x012CFBC0
static const char CAMERA_VIEW_FOV_ZOOM_PERCENT_TAG[17] = "fov_zoom_percent"; // 0x012CFBCC
static const char CAMERA_VIEW_WEAPON_ZOOM_OVERRIDE_TAG[21] = "weapon_zoom_override"; // 0x012CFBE0
static const char CAMERA_VIEW_FOV_MAX_TAG[8] = "fov_max"; // 0x012CFBF8
static const char CAMERA_VIEW_OVERRIDE_WEAPON_MAX_RANGE_TAG[26] = "override_weapon_max_range"; // 0x012CFC00
static const char CAMERA_VIEW_FOCAL_DISTANCE_MAX_TAG[19] = "focal_distance_max"; // 0x012CFC1C
static const char CAMERA_VIEW_FOCUSED_DISTANCE_ZOOM_TAG[22] = "focused_distance_zoom"; // 0x012CFC30
static const char CAMERA_VIEW_FOCAL_RANGE_FAR_TAG[16] = "focal_range_far"; // 0x012CFC48
static const char CAMERA_VIEW_TARGET_FOCUS_SPEED_TAG[19] = "target_focus_speed"; // 0x012CFC58
static const char CAMERA_VIEW_TERRAIN_FOCUS_SPEED_TAG[20] = "terrain_focus_speed"; // 0x012CFC6C
static int Game_fov = 45; // 0x016488F8, the options' fov of the normal view
static bool Camera_view_is_widescreen = true; // 0x016488FC
static bool Camera_focus_everything = true; // 0x016488FD
static bool Camera_vehicle_radial_blur = true; // 0x016488FE (placement inferred)
static bool Refocus_to_blur = true; // 0x016488FF
static float Camera_third_person_distance = 2.6f; // 0x01648900
static float Camera_third_person_distance_close = 1.5f; // 0x01648904
static float Camera_third_person_distance_scope = 1.0f; // 0x01648908
static float Camera_third_person_distance_sprint = 1.8f; // 0x0164890C
static float Camera_43_fov_modifier = 1.125f; // 0x01648910, a 4:3 screen widens the fov
static int Camera_focus_stall = 1000; // 0x01648914, ms the focus stays on a lost target
static float Camera_change_focus_tolerance = 1.0f; // 0x01648918
static float Camera_scoped_fire_max_blur = 1.0f; // 0x0164891C
static int Camera_scoped_fire_blur_time = 1200; // 0x01648920
static float Camera_refocus_tolerance = 20.0f; // 0x01648924
static float Camera_refocus_max_scope_blur = 0.6f; // 0x01648928
static float Camera_refocus_max_fineaim_blur = 0.35f; // 0x0164892C
static int Camera_refocus_to_blur_time = 200; // 0x01648930
static int Camera_refocus_to_focused_time = 1000; // 0x01648934
static float Camera_zoom_fog_override_scale = 0.6f; // 0x01648938
static timestamp_percent Camera_scoped_fire_blur_timestamp; // 0x01648C0C, invalid until set (constructor)
static timestamp_percent Camera_refocus_blur_timestamp; // 0x01648C14, invalid until set (constructor)
static float Camera_vehicle_falling_multiplier; // 0x01DE451C (a tweak; 0 until the tweak table sets it)
static camera_view_table_entry Camera_view_table[10]; // 0x01DE4568
static bool Camera_view_table_loaded; // 0x01DE48D8
static bool Camera_focus_move_near; // 0x01DE48D9
static int Camera_view_num_items; // 0x01DE48DC
static float Refocus_blur_value; // 0x01DE48E0
static tweak_table_item Tweak_table_item_Camera_vehicle_falling_multiplier("Camera_vehicle_falling_multiplier", &Camera_vehicle_falling_multiplier); // 0x01DE496C

// The global at 0x016BA434, 40.2336 m/s (90 mph) unless unused_dcf_vehicle_max_speed changes it. Defined in automobile.cpp
// (rfg_vehicle); the test programs, which do not link that library, take a stand-in from tests/stubs/rfg/vehicle.
extern float VEHICLE_MAX_SPEED;
static const float MPH_TO_METERS_PER_SECOND = 0.44704f;

static bool use_43_aspect_fov_modifier(); // 0x006C7D50 (:130)
static const camera_view_table_entry* camera_view_from_name(const char* name); // 0x006C7D60 (:140)
static void camera_view_load_data(bool reload); // 0x006C7DC0 (:158)
static void start_refocus_blur(); // 0x006C8400 (:284)
static void process_refocus_blur(); // 0x006C8420 (:290)

// 0x006C7D50 (:130)
static bool use_43_aspect_fov_modifier()
{
    return !Camera_view_is_widescreen;
}

// 0x006C7D60 (:140)
static const camera_view_table_entry* camera_view_from_name(const char* name)
{
    if (!Camera_view_table_loaded || name == nullptr)
    {
        return nullptr;
    }
    for (int i = 0; i < Camera_view_num_items; i++)
    {
        if (v_stricmp(name, Camera_view_table[i].name) == 0)
        {
            return &Camera_view_table[i];
        }
    }
    return nullptr;
}

// 0x006C7DC0 (:158)
// A reload keeps the names (allocated once from the permanent pool).
static void camera_view_load_data(bool reload)
{
    Camera_view_table_loaded = false;
    Camera_view_num_items = 0;
    xml_element* root = xml_table_parse(CAMERA_VIEW_FILE_NAME);
    if (root == nullptr)
    {
        VLIB_CRITICAL_FAILURE();
        xml_table_close(nullptr);
        return;
    }
    for (xml_element* elem = xml_find(root, CAMERA_VIEW_ROOT_TAG); elem != nullptr; elem = xml_find_next(root, elem, CAMERA_VIEW_ROOT_TAG))
    {
        camera_view_table_entry& view = Camera_view_table[Camera_view_num_items];
        if (!reload)
        {
            const char* name = xml_table_get_req_string_ref(elem, CAMERA_VIEW_NAME_TAG);
            const unsigned int size = (unsigned int)strlen(name) + 1;
            view.name = (char*)Permanent_mempool.alloc(size, 1);
            strncpy_safe(view.name, name, size);
        }
        xml_table_get_req_int(view.change_view_time, elem, CAMERA_VIEW_CHANGE_VIEW_TIME_TAG);
        if (!xml_table_get_opt_int(view.out_time_override, elem, CAMERA_VIEW_OUT_TIME_OVERRIDE_TAG))
        {
            view.out_time_override = -1;
        }
        view.camera_distance = Camera_third_person_distance;
        if (v_stricmp(xml_table_get_req_string_ref(elem, CAMERA_VIEW_DISTANCE_TAG), "close") == 0)
        {
            view.camera_distance = Camera_third_person_distance_close;
        }
        else if (v_stricmp(xml_table_get_req_string_ref(elem, CAMERA_VIEW_DISTANCE_TAG), "scope") == 0)
        {
            view.camera_distance = Camera_third_person_distance_scope;
        }
        else if (v_stricmp(xml_table_get_req_string_ref(elem, CAMERA_VIEW_DISTANCE_TAG), "sprint") == 0)
        {
            view.camera_distance = Camera_third_person_distance_sprint;
        }
        // The single player fov of the normal and sprint views follows the options (Game_fov); 11 degrees wider when
        // sprinting.
        if (xml_table_get_opt_float(view.fov_min, elem, CAMERA_VIEW_NEAR_FOV_TAG))
        {
            if (v_stricmp(view.name, "normal") == 0)
            {
                view.fov_min_sp = (float)Game_fov;
            }
            else if (v_stricmp(view.name, "sprint") == 0)
            {
                view.fov_min_sp = (float)(Game_fov + 11);
            }
        }
        else
        {
            view.fov_min = 45.0f;
            view.fov_min_sp = v_stricmp(view.name, "normal") == 0 ? (float)Game_fov : 45.0f;
        }
        xml_element* near_dof = xml_table_find_req(elem, CAMERA_VIEW_NEAR_DOF_TAG);
        xml_table_get_req_bool(view.use_weapon_fov, near_dof, CAMERA_VIEW_USE_WEAPON_FOV_TAG);
        xml_table_get_req_bool(view.use_dof, near_dof, CAMERA_VIEW_USE_DOF_TAG);
        xml_table_get_req_float(view.focal_distance_min, near_dof, CAMERA_VIEW_FOCAL_DISTANCE_MIN_TAG);
        xml_table_get_req_float(view.focal_range_near, near_dof, CAMERA_VIEW_FOCAL_RANGE_NEAR_TAG);
        xml_table_get_req_float(view.focused_distance_normal, near_dof, CAMERA_VIEW_FOCUSED_DISTANCE_NORMAL_TAG);
        view.override_weapon_zoom = false;
        view.override_weapon_max_range = false;
        view.use_far_values = false;
        view.use_fov_zoom = false;
        view.focused_distance_zoom = 300.0f;
        view.focal_range_far = 300.0f;
        xml_element* far_dof = xml_table_find_opt(elem, CAMERA_VIEW_FAR_DOF_TAG);
        if (far_dof != nullptr)
        {
            view.use_far_values = true;
            xml_element* fov_zoom = xml_table_find_opt(far_dof, CAMERA_VIEW_FOV_ZOOM_TAG);
            if (fov_zoom != nullptr)
            {
                view.use_fov_zoom = true;
                xml_table_get_req_float(view.fov_zoom_percent, fov_zoom, CAMERA_VIEW_FOV_ZOOM_PERCENT_TAG);
            }
            xml_element* weapon_zoom = xml_table_find_opt(far_dof, CAMERA_VIEW_WEAPON_ZOOM_OVERRIDE_TAG);
            if (weapon_zoom != nullptr)
            {
                view.override_weapon_zoom = true;
                xml_table_get_req_float(view.fov_max, weapon_zoom, CAMERA_VIEW_FOV_MAX_TAG);
                view.fov_max = (float)((1.0 / (double)view.fov_max) * 45.0); // a magnification to a fov
            }
            xml_element* max_range = xml_table_find_opt(far_dof, CAMERA_VIEW_OVERRIDE_WEAPON_MAX_RANGE_TAG);
            if (max_range != nullptr)
            {
                view.override_weapon_max_range = true;
                xml_table_get_req_float(view.focal_distance_max, max_range, CAMERA_VIEW_FOCAL_DISTANCE_MAX_TAG);
            }
            xml_table_get_req_float(view.focused_distance_zoom, far_dof, CAMERA_VIEW_FOCUSED_DISTANCE_ZOOM_TAG);
            xml_table_get_req_float(view.focused_dist_target_multiplier, far_dof, CAMERA_VIEW_TARGET_FOCUSED_MULTIPLIER_TAG);
            xml_table_get_req_float(view.focal_range_far, far_dof, CAMERA_VIEW_FOCAL_RANGE_FAR_TAG);
            xml_table_get_req_int(view.target_focus_speed, far_dof, CAMERA_VIEW_TARGET_FOCUS_SPEED_TAG);
            xml_table_get_req_int(view.terrain_focus_speed, far_dof, CAMERA_VIEW_TERRAIN_FOCUS_SPEED_TAG);
        }
        char effect_name[64];
        if (xml_table_get_opt_string(effect_name, sizeof(effect_name), elem, "Effect"))
        {
            view.effect_handle = effects_get_handle(effect_name);
        }
        else
        {
            view.effect_handle = 0xFFFFFFFF;
        }
        Camera_view_num_items++;
    }
    xml_table_close(root);
    if (Camera_view_num_items == 0)
    {
        VLIB_CRITICAL_FAILURE();
        return;
    }
    Camera_view_table_loaded = true;
}

// 0x006C8400 (:284)
static void start_refocus_blur()
{
    Refocus_to_blur = true;
    Camera_refocus_blur_timestamp.set(Camera_refocus_to_blur_time);
}

// 0x006C8420 (:290)
// Blurs (Refocus_to_blur) and then sharpens the frame over the refocus times when the focus distance jumps.
static void process_refocus_blur()
{
    if (Refocus_to_blur && Camera_refocus_blur_timestamp.valid() && Camera_refocus_blur_timestamp.elapsed())
    {
        Refocus_to_blur = false;
        Camera_refocus_blur_timestamp.set(Camera_refocus_to_focused_time);
    }
    if (Camera_refocus_blur_timestamp.valid() && !Camera_refocus_blur_timestamp.elapsed())
    {
        float pct = Camera_refocus_blur_timestamp.percent();
        float max_blur = Camera_refocus_max_fineaim_blur;
        if (Refocus_to_blur)
        {
            pct = 1.0f - pct;
        }
        if (player_is_scoped(player_get_local_player()))
        {
            max_blur = Camera_refocus_max_scope_blur;
        }
        const float blur = max_blur * pct * pct;
        const bool keep = Refocus_to_blur ? blur < Refocus_blur_value : Refocus_blur_value < blur;
        if (!keep)
        {
            Refocus_blur_value = blur;
        }
        game_render_set_camera_frame_override_blur(Refocus_blur_value);
    }
}

// 0x006C39B0 (.h:61)
const timestamp_percent camera_view_data::get_transition_timestamp()
{
    return m_transition_timestamp;
}

// 0x006C8550 (:332)
// The timestamps start invalid (their constructors); no camera effect is playing yet.
camera_view_data::camera_view_data()
{
    m_effect_info_handle = 0xFFFFFFFF; // invalid handle
    m_effect_handle = 0xFFFFFFFF; // invalid handle
}

// 0x006C8570 (:597)
// The distance behind the player, blended over the view transition; a temporary override restarts the transition.
void camera_view_data::process_distance()
{
    const float override_distance = m_temp_distance_override;
    float target = m_target_camera_distance;
    if (override_distance != 0.0f)
    {
        m_temp_distance_override = 0.0f;
        target = override_distance;
        if (m_transition_timestamp.valid() && !m_transition_timestamp.elapsed() && m_current_view != nullptr)
        {
            m_transition_timestamp.set(m_current_view->change_view_time);
        }
    }
    if (m_transition_timestamp.valid() && !m_transition_timestamp.elapsed())
    {
        m_camera_distance = m_transition_timestamp.percent() * (m_last_camera_distance - target) + target;
        return;
    }
    m_camera_distance = target;
    m_last_camera_distance = target;
}

// 0x006C8620 (:615)
// The renderer's depth of field ranges: in focus from focus_start_B to focus_end_A.
void camera_view_data::set_camera_dof()
{
    Rfg_camera.m_focus_start_A = m_focal_range_near;
    Rfg_camera.m_focus_start_B = m_focal_distance;
    if (Camera_focus_move_near)
    {
        Rfg_camera.m_focus_start_A = Rfg_camera.m_focus_start_B - Rfg_camera.m_focus_start_A;
    }
    if (Rfg_camera.m_focus_start_B <= Rfg_camera.m_focus_start_A)
    {
        Rfg_camera.m_focus_start_A = Rfg_camera.m_focus_start_B - 1.0f;
    }
    Rfg_camera.m_focus_end_A = m_focused_distance + Rfg_camera.m_focus_start_B;
    Rfg_camera.m_focus_end_B = m_focal_range_far;
    if (Rfg_camera.m_focus_end_A - 0.0f < 0.0f)
    {
        Rfg_camera.m_focus_end_A = 0.0f;
    }
    if (Rfg_camera.m_focus_end_A - 310.0f >= 0.0f)
    {
        Rfg_camera.m_focus_end_A = 310.0f;
    }
    if (Rfg_camera.m_focus_end_B - Rfg_camera.m_focus_end_A < 20.0f)
    {
        Rfg_camera.m_focus_end_B = Rfg_camera.m_focus_end_A + 20.0f;
    }
    if (!m_dof_enable)
    {
        // everything in focus
        Rfg_camera.m_focus_start_A = -1.0f;
        Rfg_camera.m_focus_end_A = 10000.0f;
        Rfg_camera.m_focus_start_B = 0.0f;
        Rfg_camera.m_focus_end_B = 1001.0f;
    }
    Rfg_camera.m_dof_enable = m_dof_enable;
    if (Rfg_camera.mode == CAMERA_SLEW_MODE)
    {
        Rfg_camera.m_dof_enable = false;
    }
}

// 0x006C8780 (:656)
void camera_view_data::set_camera_fov()
{
    if (Rfg_camera.mode != CAMERA_SLEW_MODE && !Camera_third_person_lock)
    {
        Rfg_camera.ideal_fov = m_fov;
        if (!Camera_view_is_widescreen)
        {
            Rfg_camera.ideal_fov = Rfg_camera.ideal_fov * Camera_43_fov_modifier;
        }
    }
}

// 0x006C87C0 (:868)
void camera_view_data::set_fov(float new_fov, int transition_time)
{
    m_last_fov = m_fov;
    m_target_fov = new_fov;
    if (transition_time < 1)
    {
        m_fov = new_fov;
        set_camera_fov(); // inlined in the binary
        return;
    }
    m_target_zoom_timestamp.set(transition_time);
}

// 0x006C8830 (:880)
void camera_view_data::set_temp_distance_override(float temp_override)
{
    m_temp_distance_override = temp_override;
}

// 0x006C8840 (:904)
void camera_view_set_fov(float new_fov, int transition_time)
{
    Rfg_camera.m_camera_view_data.set_fov(new_fov, transition_time); // inlined in the binary
}

// 0x006C88C0 (:963)
float camera_view_get_current_fov()
{
    return Rfg_camera.m_camera_view_data.m_fov;
}

// 0x006C88D0 (:968)
int camera_view_current_transition_time_left()
{
    timestamp t = Rfg_camera.m_camera_view_data.get_transition_timestamp();
    if (t.valid() && !t.elapsed())
    {
        return t.time_until();
    }
    return 0;
}

// 0x006C8920 (:981)
void camera_view_one_frame_target_distance_override(float temp_target)
{
    Rfg_camera.m_camera_view_data.set_temp_distance_override(temp_target); // inlined in the binary
}

// 0x006C8930 (:987)
void camera_view_do_scoped_fire_blur()
{
    if (Rfg_camera.m_camera_view_data.m_dof_enable)
    {
        Camera_scoped_fire_blur_timestamp.set(Camera_scoped_fire_blur_time);
    }
}

// 0x006C8950 (:997)
int camera_view_get_single_player_fov()
{
    return Game_fov;
}

// 0x006C8960 (:1036)
int camera_view_get_fov_raw()
{
    return Game_fov;
}

// The blend fraction of the view transition, else of the zoom to a target, clamped to 0..1 (process_fov and
// process_dof start alike).
static float camera_view_blend(const timestamp_percent& transition, const timestamp_percent& zoom, bool& in_transition)
{
    float pct = 0.0f;
    in_transition = false;
    if (transition.valid() && !transition.elapsed())
    {
        pct = transition.percent();
        in_transition = true;
    }
    else if (zoom.valid() && !zoom.elapsed())
    {
        pct = zoom.percent();
    }
    if (pct - 0.0f < 0.0f)
    {
        pct = 0.0f;
    }
    if (pct - 1.0f >= 0.0f)
    {
        pct = 1.0f;
    }
    return pct;
}

// 0x006CF320 (:338)
void camera_view_data::process_fov()
{
    if (m_last_view == nullptr || m_current_view == nullptr)
    {
        return;
    }
    bool in_transition;
    const float pct = camera_view_blend(m_transition_timestamp, m_target_zoom_timestamp, in_transition);
    if (pct <= 0.0f)
    {
        m_fov = m_target_fov;
    }
    else
    {
        m_fov = (m_last_fov - m_target_fov) * pct + m_target_fov;
    }
    set_camera_fov(); // inlined in the binary
}

// 0x006CF450 (:362)
// The depth of field follows the aim target (a human or a manned vehicle) after the focus speed; without one it
// returns to the view's terrain focus once the target was lost for Camera_focus_stall.
void camera_view_data::focus_on_target(unsigned int target_handle, float target_distance)
{
    object* target = World.object_from_handle(target_handle);
    float max_range = m_max_range;
    bool manned = false;
    if (max_range < target_distance)
    {
        max_range = target_distance + 1.0f;
    }
    float range = max_range - m_current_view->focal_distance_min;
    if (range == 0.0f)
    {
        range = 1.0f;
    }
    if (target != nullptr)
    {
        if (target->get_obj_type() == OT_VEHICLE)
        {
            vehicle* vp = World.object_from_handle<vehicle>(target_handle);
            human* driver = vehicle_passenger_in_seat(vp, VSI_DRIVER);
            if (activity_convoy_vehicle_destroy(vp) || driver != nullptr)
            {
                manned = true;
            }
        }
        if (target->get_obj_type() == OT_HUMAN || manned)
        {
            const unsigned int last_target = m_current_target;
            if (target_handle != last_target)
            {
                const float last_focused = m_target_focused_distance;
                m_last_fov = m_fov;
                m_last_focused_distance = m_focused_distance;
                m_target_zoom_timestamp.invalidate();
                m_current_target = target_handle;
                m_target_focused_distance = (last_focused / range) * (m_current_view->focused_distance_zoom - m_current_view->focused_distance_normal) + m_current_view->focused_distance_normal;
                m_target_focused_distance = m_current_view->focused_dist_target_multiplier * m_target_focused_distance;
            }
            float focal = target_distance - m_target_focused_distance * 0.5f;
            const float focal_min = m_current_view->focal_distance_min;
            if (focal <= focal_min)
            {
                focal = focal_min;
                if (target_distance <= focal_min)
                {
                    focal = target_distance - 1.0f;
                }
            }
            m_last_focal_distance = m_focal_distance;
            if (focal - 0.0f < 0.0f)
            {
                focal = 0.0f;
            }
            if (focal - max_range >= 0.0f)
            {
                focal = max_range;
            }
            m_target_focal_distance = focal;
            if (target_handle != last_target)
            {
                m_target_fov = (focal / range) * (m_min_zoom - m_max_zoom) + m_max_zoom;
                m_target_zoom_timestamp.set(m_current_view->target_focus_speed);
                const float jump = m_target_focal_distance - m_last_focal_distance;
                if (Camera_refocus_tolerance <= jump || Camera_refocus_tolerance <= -jump || last_target == 0xFFFFFFFF)
                {
                    start_refocus_blur(); // inlined in the binary
                }
            }
            m_lost_target.set(Camera_focus_stall);
            return;
        }
    }
    if (m_lost_target.valid() && !m_lost_target.elapsed())
    {
        return;
    }
    if (!Camera_focus_everything)
    {
        if (m_current_target != 0xFFFFFFFF)
        {
            m_target_zoom_timestamp.invalidate();
            m_last_fov = m_fov;
            m_current_target = 0xFFFFFFFF;
            m_target_fov = m_min_zoom;
            m_last_focused_distance = m_focused_distance;
            m_target_focused_distance = m_current_view->focused_distance_normal;
            m_last_focal_distance = m_focal_distance;
            m_target_focal_distance = m_current_view->focal_distance_min;
            m_target_zoom_timestamp.set(m_current_view->terrain_focus_speed);
        }
        return;
    }
    if (m_current_target != 0xFFFFFFFF)
    {
        const float last_focused = m_target_focused_distance;
        m_last_fov = m_fov;
        m_target_zoom_timestamp.invalidate();
        m_target_fov = m_min_zoom;
        m_current_target = 0xFFFFFFFF;
        m_last_focused_distance = m_focused_distance;
        m_target_focused_distance = (last_focused / range) * (m_current_view->focused_distance_zoom - m_current_view->focused_distance_normal) + m_current_view->focused_distance_normal;
    }
    float focal = target_distance - m_target_focused_distance * 0.5f;
    const float focal_min = m_current_view->focal_distance_min;
    if (focal <= focal_min)
    {
        focal = focal_min;
        if (target_distance <= focal_min)
        {
            focal = target_distance - 1.0f;
        }
    }
    const float change = focal - m_target_focal_distance;
    if (m_dof_enable && (Camera_change_focus_tolerance <= change || Camera_change_focus_tolerance <= -change))
    {
        m_last_fov = m_fov;
        m_target_fov = m_min_zoom;
        m_last_focal_distance = m_focal_distance;
        m_last_focused_distance = m_focused_distance;
        if (focal - 0.0f < 0.0f)
        {
            focal = 0.0f;
        }
        if (focal - max_range >= 0.0f)
        {
            focal = max_range;
        }
        m_target_focal_distance = focal;
        m_target_zoom_timestamp.set(m_current_view->terrain_focus_speed);
        const float jump = m_target_focal_distance - m_last_focal_distance;
        if (Camera_refocus_tolerance <= jump || Camera_refocus_tolerance <= -jump)
        {
            start_refocus_blur();
        }
    }
}

// 0x006CF9F0 (:489)
// After a view transition, with a weapon and no turret: the focus follows the player's aim (views with far values).
void camera_view_data::process_focus()
{
    if (m_transition_timestamp.valid() && !m_transition_timestamp.elapsed())
    {
        return;
    }
    if (m_current_view == nullptr || !m_current_view->use_far_values)
    {
        return;
    }
    player* pp = player_get_local_player();
    if (pp == nullptr)
    {
        return;
    }
    const weapon_info* wi = human_weapons_get_weapon_info(pp);
    if (wi == nullptr || human_combat_get_manned_turret(pp) != nullptr)
    {
        return;
    }
    m_max_range = m_current_view->override_weapon_max_range ? m_current_view->focal_distance_max : wi->max_range;
    focus_on_target(pp->aim_target, vec_dist(pp->aim_pos, pp->pos));
    if (!m_dof_enable)
    {
        return;
    }
    if (Camera_scoped_fire_blur_timestamp.valid() && !Camera_scoped_fire_blur_timestamp.elapsed())
    {
        float pct = Camera_scoped_fire_blur_timestamp.percent();
        if (pct - 0.0f < 0.0f)
        {
            pct = 0.0f;
        }
        if (pct - 1.0f >= 0.0f)
        {
            pct = 1.0f;
        }
        game_render_set_camera_frame_override_blur(Camera_scoped_fire_max_blur * fl_parametric_accel_decel(pct, 2));
    }
    process_refocus_blur();
}

// 0x006CFB70 (:531)
// The view's camera effect (the scope's lens), started when the view transition is over and drawn last, in front.
void camera_view_data::process_effects()
{
    if (m_effect_info_handle == 0xFFFFFFFF || m_effect_handle != 0xFFFFFFFF)
    {
        return;
    }
    timestamp transition = get_transition_timestamp();
    if (!transition.elapsed())
    {
        return;
    }
    m_effect_handle = effects_play(m_effect_info_handle, Rfg_camera.real_pos, IDENTITY_MATRIX, true, 0xFFFFFFFF, -1, 0xFFFFFFFF, 0.0f, true, EFFECTS_CATEGORY_NONE);
    if (m_effect_handle != 0xFFFFFFFF)
    {
        object_effect* effect = (object_effect*)World.object_from_handle(m_effect_handle);
        if (effect != nullptr && effect->get_obj_type() == OT_EFFECT)
        {
            effect->attach_to_camera_orient();
            // The effect's meshes only distort, back to front and unbatched. Magic number: w -10000 in the instance
            // parameters (read by the effect's shader).
            // Original: an effect whose data is not loaded reads through a null vfx_effect here.
            static const vector4 instance_params(0.0f, 0.0f, 0.0f, -10000.0f);
            vfx_effect* vfx = effect->get_vfx_effect(); // inlined in the binary
            rl_renderable_instance* first = vfx->m_renderable_mesh_insts.first();
            for (rl_renderable_instance* ri = first; ri != nullptr;)
            {
                ri->set_distortion_only(true);
                ri->set_sort_method(rl_renderable_instance::SORT_METHOD_BACK_TO_FRONT);
                ri->set_instance_params(instance_params);
                ri->set_is_batchable(false);
                ri = ri->next;
                if (ri == first)
                {
                    break;
                }
            }
        }
    }
    game_render_set_scope_diffraction_enabled(true);
}

// 0x006CFCD0 (:559)
// The depth of field blended over the view transition (and the zoom to a target).
void camera_view_data::process_dof()
{
    if (m_last_view == nullptr || m_current_view == nullptr)
    {
        return;
    }
    bool in_transition;
    const float pct = camera_view_blend(m_transition_timestamp, m_target_zoom_timestamp, in_transition);
    if (pct <= 0.0f)
    {
        m_focal_distance = m_target_focal_distance;
        m_focused_distance = m_target_focused_distance;
    }
    else
    {
        m_focal_distance = (m_last_focal_distance - m_target_focal_distance) * pct + m_target_focal_distance;
        m_focused_distance = (m_last_focused_distance - m_target_focused_distance) * pct + m_target_focused_distance;
    }
    if (in_transition && 0.0f < pct)
    {
        m_focal_range_far = (m_last_range_far - m_target_range_far) * pct + m_target_range_far;
        m_focal_range_near = (m_last_range_near - m_target_range_near) * pct + m_target_range_near;
        set_camera_dof();
        return;
    }
    m_focal_range_near = m_target_range_near;
    m_focal_range_far = m_target_range_far;
    // Original: the near range is set twice, the second time from the view.
    m_focal_range_near = m_current_view->focal_range_near;
    m_dof_enable = m_current_view->use_dof;
    set_camera_dof();
}

// 0x006CFE80 (:669)
// Stops the view's camera effect.
void camera_view_data::set_camera_effect()
{
    if (m_effect_handle == 0xFFFFFFFF)
    {
        return;
    }
    object_effect* effect = (object_effect*)World.object_from_handle(m_effect_handle);
    if (effect != nullptr && effect->get_obj_type() == OT_EFFECT)
    {
        effect->stop(object_effect::SS_STOP_LOOPING);
    }
    m_effect_handle = 0xFFFFFFFF;
    game_render_set_scope_diffraction_enabled(false);
}

// 0x006CFED0 (:681)
void camera_view_data::process()
{
    if (Camera_view_table_loaded)
    {
        process_dof();
        process_distance();
        process_effects();
    }
}

// 0x006CFF00 (:690)
void camera_view_data::update_fov()
{
    if (Camera_view_table_loaded)
    {
        process_fov();
        process_focus();
    }
}

// 0x006CFF20 (:698)
// A car widens the fov with its speed, blurs radially and shakes the camera above the vehicle's speeds.
void camera_view_data::update_vehicle_fov(const vehicle* vp)
{
    if (Camera_third_person_lock || vp == nullptr || vp->get_obj_type() != OT_VEHICLE || vp->get_sub_type() != OT_SUB_VEHICLE_AUTO)
    {
        return;
    }
    vector velocity;
    vehicle_get_velocity(vp, velocity);
    if (!automobile_any_wheel_on_ground((const automobile*)vp) && velocity.y < 0.0f)
    {
        velocity.y = (float)((double)Camera_vehicle_falling_multiplier * (double)velocity.y);
    }
    const float base_fov = Rfg_camera.free_params.submode_params->free_fov;
    const float speed = (float)sqrt((double)(float)((double)velocity.x * (double)velocity.x + (double)velocity.y * (double)velocity.y + (double)velocity.z * (double)velocity.z));
    const float fov_min_speed = (float)((double)vp->info->m_camera_fov_min_speed * (double)MPH_TO_METERS_PER_SECOND);
    m_fov = base_fov;
    if ((double)speed > (double)fov_min_speed)
    {
        const float range = (float)((double)VEHICLE_MAX_SPEED - (double)fov_min_speed);
        if (range > 0.0f)
        {
            const float t = (float)(((double)speed - (double)fov_min_speed) / (double)range);
            const float multiplier = (float)((double)t * ((double)vp->info->m_camera_fov_multiplier - 1.0) + 1.0);
            m_fov = (float)((double)multiplier * (double)base_fov);
        }
    }
    if (Rfg_camera.mode != CAMERA_SLEW_MODE && !Camera_third_person_lock)
    {
        Rfg_camera.ideal_fov = use_43_aspect_fov_modifier() ? (float)((double)Camera_43_fov_modifier * (double)m_fov) : m_fov;
    }
    const float blur_min_speed = (float)((double)vp->info->m_radial_blur_min_speed * (double)MPH_TO_METERS_PER_SECOND);
    if (Camera_vehicle_radial_blur && (double)speed > (double)blur_min_speed)
    {
        const float range = (float)((double)VEHICLE_MAX_SPEED - (double)blur_min_speed);
        if (range > 0.0f)
        {
            const float t = (float)(((double)speed - (double)blur_min_speed) / (double)range);
            float blur = (float)((double)t * ((double)vp->info->m_radial_blur_max - 0.0) + 0.0);
            if ((float)((double)blur - 0.0) < 0.0f)
            {
                blur = 0.0f;
            }
            if ((float)((double)blur - 1.0) >= 0.0f)
            {
                blur = 1.0f;
            }
            fb_accum_blur_set_enable(true);
            fb_accum_blur_change_percentage(blur);
        }
    }
    const float shake_min_speed = (float)((double)vp->info->m_camera_shake_min_speed * (double)MPH_TO_METERS_PER_SECOND);
    if ((double)speed > (double)shake_min_speed)
    {
        const float strength = fl_parameter(speed, shake_min_speed, (float)((double)vp->info->enforced_max_speed * (double)MPH_TO_METERS_PER_SECOND), 0.0f, 1.0f, true);
        vehicle_camera_shake_play_fast(vp, strength);
    }
}

// 0x006D0290 (:754)
// Without a transition (or the first view) the values jump; otherwise the view's values become the targets of a
// transition over the old view's out time (or the new view's change time).
void camera_view_data::set_view(const camera_view_table_entry* view, bool skip_transition)
{
    if (view == nullptr)
    {
        return;
    }
    float min_zoom = view->fov_min_sp;
    if (Multiplayer && min_zoom - 60.0f >= 0.0f)
    {
        min_zoom = 60.0f;
    }
    m_min_zoom = min_zoom;
    if (view->use_weapon_fov)
    {
        player* pp = player_get_local_player();
        if (pp != nullptr)
        {
            const weapon_info* wi = human_weapons_get_weapon_info(pp);
            if (wi == nullptr)
            {
                return;
            }
            m_min_zoom = wi->zoom_magnification * 45.0f;
        }
    }
    m_max_zoom = m_min_zoom;
    if (view->use_fov_zoom)
    {
        m_max_zoom = view->fov_zoom_percent * m_min_zoom;
    }
    if (skip_transition || m_current_view == nullptr)
    {
        if (m_current_view == nullptr)
        {
            m_current_view = view;
        }
        m_last_view = m_current_view;
        m_current_view = view;
        m_camera_distance = view->camera_distance;
        m_last_camera_distance = m_camera_distance;
        m_target_camera_distance = view->camera_distance;
        m_focused_distance = view->focused_distance_normal;
        m_last_focused_distance = m_focused_distance;
        m_target_focused_distance = view->focused_distance_normal;
        m_focal_distance = view->focal_distance_min;
        m_last_focal_distance = m_focal_distance;
        m_target_focal_distance = view->focal_distance_min;
        m_focal_range_near = view->focal_range_near;
        m_last_range_near = m_focal_range_near;
        m_target_range_near = view->focal_range_near;
        m_focal_range_far = view->focal_range_far;
        m_last_range_far = m_focal_range_far;
        m_target_range_far = view->focal_range_far;
        const float fov = m_min_zoom;
        m_fov = fov;
        m_target_fov = fov;
        m_last_fov = fov;
        float max_zoom = view->fov_min_sp;
        if (Multiplayer && max_zoom - 60.0f >= 0.0f)
        {
            max_zoom = 60.0f;
        }
        m_max_zoom = max_zoom;
        m_dof_enable = view->use_dof;
        m_current_target = 0xFFFFFFFF;
        set_camera_effect(); // inlined in the binary
        m_effect_info_handle = view->effect_handle;
        m_effect_handle = 0xFFFFFFFF;
        set_camera_dof();
        set_camera_fov(); // inlined in the binary
        return;
    }
    if (m_current_view == view)
    {
        return;
    }
    set_camera_effect();
    m_last_camera_distance = m_camera_distance;
    const camera_view_table_entry* last = m_current_view;
    m_effect_info_handle = view->effect_handle;
    m_effect_handle = 0xFFFFFFFF;
    m_last_view = last;
    m_current_view = view;
    m_target_camera_distance = view->camera_distance;
    m_dof_enable = view->use_dof || last->use_dof;
    m_last_focused_distance = m_focused_distance;
    m_target_focused_distance = view->focused_distance_normal;
    m_last_focal_distance = m_focal_distance;
    m_target_focal_distance = view->focal_distance_min;
    m_last_range_near = m_focal_range_near;
    m_target_range_near = view->focal_range_near;
    m_last_range_far = m_focal_range_far;
    m_target_range_far = view->focal_range_far;
    m_last_fov = m_fov;
    m_target_fov = m_min_zoom;
    int time = view->change_view_time;
    if (last != nullptr && last->out_time_override >= 1)
    {
        time = last->out_time_override;
    }
    m_transition_timestamp.set(time);
    m_target_zoom_timestamp.invalidate();
    m_current_target = 0xFFFFFFFF;
    if (!Multiplayer)
    {
        camera_free_set_fov(m_current_view->fov_min_sp);
        return;
    }
    const float mp_fov = m_current_view->fov_min;
    camera_free_set_fov(mp_fov - 60.0f >= 0.0f ? 60.0f : mp_fov);
}

// 0x006D05B0 (:899)
void camera_view_set_default()
{
    Rfg_camera.m_camera_view_data.set_view(camera_view_from_name("normal"), true);
}

// 0x006D05D0 (:909)
// The view of a zoom state: normal, sprint, fine aim (a turret's own) or scope (without blur for penetrating
// bullets); the scope also thins the fog.
void camera_set_view_player(player_zoom_state zoom, bool instant_change)
{
    player* pp = player_get_local_player();
    const bool on_turret = pp != nullptr && human_combat_get_manned_turret(pp) != nullptr;
    Camera_scoped_fire_blur_timestamp.invalidate();
    Camera_refocus_blur_timestamp.invalidate();
    game_render_fog_set_density_override_scale(1.0f);
    const camera_view_table_entry* view;
    if (zoom == PZS_SPRINT)
    {
        view = camera_view_from_name("sprint");
    }
    else if (zoom == PZS_FINE_AIM)
    {
        view = camera_view_from_name(on_turret ? "fine_aim_turret" : "fine_aim");
    }
    else if (zoom == PZS_SCOPE)
    {
        view = camera_view_from_name("scope");
        if (pp != nullptr)
        {
            game_render_fog_set_density_override_scale(Camera_zoom_fog_override_scale);
            weapon* wp = pp->get_equipped_weapon();
            if (wp != nullptr && wp->w_info->flags.penetrating_bullets)
            {
                view = camera_view_from_name("scope_no_blur");
            }
        }
    }
    else
    {
        view = camera_view_from_name("normal");
    }
    if (view == nullptr)
    {
        VLIB_CRITICAL_FAILURE();
        return;
    }
    Rfg_camera.m_camera_view_data.set_view(view, instant_change);
}

// 0x006D06D0 (:1002)
// The options' fov (45-85): the normal and sprint views follow it.
void camera_view_set_custom_fov(int fov, bool update_tables)
{
    if (fov < 45)
    {
        fov = 45;
    }
    else if (85 < fov)
    {
        fov = 85;
    }
    Game_fov = fov;
    if (update_tables)
    {
        for (int i = 0; i < Camera_view_num_items; i++)
        {
            if (v_stricmp(Camera_view_table[i].name, "normal") == 0)
            {
                Camera_view_table[i].fov_min_sp = (float)Game_fov;
            }
            else if (v_stricmp(Camera_view_table[i].name, "sprint") == 0)
            {
                Camera_view_table[i].fov_min_sp = (float)(Game_fov + 11);
            }
        }
    }
    player* pp = player_get_local_player();
    player_zoom_state zoom = PZS_OFF;
    if (pp != nullptr && pp->zoom_state != PZS_OFF)
    {
        zoom = pp->zoom_state;
    }
    camera_set_view_player(zoom, true);
    Rfg_camera.m_camera_view_data.set_view(camera_view_from_name("normal"), true);
}

// 0x006D07A0 (:1031)
int camera_view_get_fov()
{
    return Multiplayer && 59 < Game_fov ? 60 : Game_fov;
}

// 0x006D4BE0 (:885)
// The view table, the normal view, and whether the screen is wide (an aspect below 1.4 widens the fov).
void camera_view_system_init()
{
    const bool reload = Camera_view_table_loaded;
    camera_view_load_data(reload);
    if (!reload)
    {
        Rfg_camera.m_camera_view_data.set_view(camera_view_from_name("normal"), true);
        if (rl_render_lib::get_instance()->get_screen_info()->get_aspect_ratio() < 1.4f)
        {
            Camera_view_is_widescreen = false;
        }
    }
}
