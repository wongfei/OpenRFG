// Original: volition/rfg/code/camera/camera_view.h
// Implementation: volition/rfg/code/camera/camera_view.cpp
//
// Camera views: the view parameters (fov, distance, depth of field, effects) read from the camera view table and
// blended over a transition; camera_set_view_player picks the view for the player's zoom state.
#pragma once

#include "volition/vlib/code/time/timestamp.h"

struct camera_view_table_entry;
class vehicle;

enum player_zoom_state;

// size 0x84
class camera_view_data
{
public:
    camera_view_data(); // 0x006C8550 (:332)
    void process(); // 0x006CFED0 (:681)
    void update_fov(); // 0x006CFF00 (:690)
    void update_vehicle_fov(const vehicle* vp); // 0x006CFF20 (:698)
    void set_view(const camera_view_table_entry* view, bool skip_transition); // 0x006D0290 (:754)
    void set_fov(float new_fov, int transition_time); // 0x006C87C0 (:868)
    const timestamp_percent get_transition_timestamp(); // 0x006C39B0 (.h:61)
    void set_temp_distance_override(float temp_override); // 0x006C8830 (:880)
private:
    void process_fov(); // 0x006CF320 (:338)
    void focus_on_target(unsigned int target_handle, float target_distance); // 0x006CF450 (:362)
    void process_focus(); // 0x006CF9F0 (:489)
    void process_dof(); // 0x006CFCD0 (:559)
    void process_distance(); // 0x006C8570 (:597)
    void process_effects(); // 0x006CFB70 (:531)
    void set_camera_dof(); // 0x006C8620 (:615)
    void set_camera_fov(); // 0x006C8780 (:656)
    void set_camera_effect(); // 0x006CFE80 (:669)

public:
    float m_camera_distance; // 0x00
    float m_focused_distance; // 0x04
    float m_focal_distance; // 0x08
    float m_focal_range_near; // 0x0C
    float m_focal_range_far; // 0x10
    float m_fov; // 0x14
    bool m_dof_enable; // 0x18
private:
    const camera_view_table_entry* m_last_view; // 0x1C
    const camera_view_table_entry* m_current_view; // 0x20
    timestamp_percent m_transition_timestamp; // 0x24
    timestamp_percent m_target_zoom_timestamp; // 0x2C
    timestamp m_lost_target; // 0x34
    unsigned int m_current_target; // 0x38
    float m_temp_distance_override; // 0x3C
    float m_last_camera_distance; // 0x40
    float m_target_camera_distance; // 0x44
    float m_last_focal_distance; // 0x48
    float m_target_focal_distance; // 0x4C
    float m_last_focused_distance; // 0x50
    float m_target_focused_distance; // 0x54
    float m_last_range_near; // 0x58
    float m_target_range_near; // 0x5C
    float m_last_range_far; // 0x60
    float m_target_range_far; // 0x64
    float m_last_fov; // 0x68
    float m_target_fov; // 0x6C
    float m_min_zoom; // 0x70
    float m_max_zoom; // 0x74
    float m_max_range; // 0x78
    unsigned int m_effect_info_handle; // 0x7C
    unsigned int m_effect_handle; // 0x80
};

void unused_dcf_reload_camera_view(); // 0x006D6680 (:124)
void camera_view_system_init(); // 0x006D4BE0 (:885)
void camera_view_set_default(); // 0x006D05B0 (:899)
void camera_view_set_fov(float new_fov, int transition_time); // 0x006C8840 (:904)
void camera_set_view_player(player_zoom_state zoom, bool instant_change); // 0x006D05D0 (:909)
float camera_view_get_current_fov(); // 0x006C88C0 (:963)
int camera_view_current_transition_time_left(); // 0x006C88D0 (:968)
void camera_view_one_frame_target_distance_override(float temp_target); // 0x006C8920 (:981)
void camera_view_do_scoped_fire_blur(); // 0x006C8930 (:987)
int camera_view_get_single_player_fov(); // 0x006C8950 (:997)
void camera_view_set_custom_fov(int fov, bool update_tables); // 0x006D06D0 (:1002)
int camera_view_get_fov(); // 0x006D07A0 (:1031)
int camera_view_get_fov_raw(); // 0x006C8960 (:1036)
