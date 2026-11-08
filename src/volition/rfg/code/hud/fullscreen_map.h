// Original: volition/rfg/code/hud/fullscreen_map.h
// Implementation: volition/rfg/code/hud/fullscreen_map.cpp
//
// The full screen map (single player): its stream allocator (GSA_FSM), zoom, zone images and streaming. Placement of
// fsmap_zoom_info inferred. Values of zone_stream_state from the asm.
#pragma once

#include "volition/interface_lib/code/vint_external.h"
#include "volition/rfg/code/gamewide/gameseq.h"
#include "volition/rfg/code/interface/ui_message_box.h"
#include "volition/stream2/code/stream2_allocator.h"
#include "volition/vlib/code/math/bbox.h"
#include "volition/vlib/code/math/vector2.h"
#include "volition/vlib/code/math/vector3.h"
#include "volition/vlib/code/time/timestamp.h"
#include "volition/vlib/code/util/varray.h"

class mission_info;
class rl_renderer;
class vint_data_item;
struct bm_image;

// size 0xC
struct fsmap_zoom_info
{
    float curr_zoom; // 0x00
    float min_zoom; // 0x04
    float max_zoom; // 0x08
};

// size 0x4
struct fs_flags
{
    unsigned int wants_exit : 1; // 0x00:0
    unsigned int exiting : 1; // 0x00:1
    unsigned int can_exit : 1; // 0x00:2
    unsigned int force_exit : 1; // 0x00:3
    unsigned int unlocked_env : 1; // 0x00:4
    unsigned int allocated : 1; // 0x00:5
    unsigned int processing : 1; // 0x00:6
    unsigned int fully_loaded : 1; // 0x00:7
    unsigned int initial_grids_ready : 1; // 0x00:8
    unsigned int rendered_a_grid : 1; // 0x00:9
    unsigned int loaded_one_in : 1; // 0x00:10
    unsigned int scrolling : 1; // 0x00:11
    unsigned int zooming_in : 1; // 0x00:12
    unsigned int zooming_out : 1; // 0x00:13
    unsigned int updated_gids : 1; // 0x00:14
    unsigned int has_single_static_gids : 1; // 0x00:15
    unsigned int targeting : 1; // 0x00:16
    unsigned int dont_center : 1; // 0x00:17
};

// size 0x14
struct audio_info
{
    int play_static_blink; // 0x00
    int play_static_screen; // 0x04
    int play_scroll_loop; // 0x08
    int play_zoom_in_alr; // 0x0C
    int play_zoom_out_alr; // 0x10
};

enum zone_stream_state
{
    ZSS_UNLOADED = 0,
    ZSS_UNLOADING = 1,
    ZSS_STREAMING = 2,
    ZSS_LOADED = 3,
    ZSS_LOADED_BOGUS = 4,
};

// size 0x18
struct zone_map_information
{
    bool fully_obscured_by_fow; // 0x00
    unsigned char corner_info; // 0x01
    int bitmap_handle; // 0x04
    float alpha_not_loaded; // 0x08
    vector2i gid; // 0x0C
    zone_stream_state stream_state; // 0x14
};

// size 0x10
struct stream_information
{
    bool exists; // 0x00
    bool size_ok; // 0x01
    unsigned int srid; // 0x04
    float corner_radius; // 0x08
    unsigned int district_handle; // 0x0C
};

// size 0x23, read from the .fsmib file as it stands
#pragma pack(push, 1)
struct fsm_poi
{
    unsigned char x; // 0x00
    unsigned char z; // 0x01
    unsigned char zone_xz; // 0x02
    char poi_text[32]; // 0x03
};
#pragma pack(pop)

// size 0x1CA8
struct fullscreen
{
    fs_flags flags; // 0x00
    int misc_static_id; // 0x04
    int cpu_mempool_mark; // 0x08
    int gpu_mempool_mark; // 0x0C
    int inteface_mempool_mark; // 0x10
    int objectives_shown; // 0x14
    int objectives_min; // 0x18
    int objectives_max; // 0x1C
    int objective_selected; // 0x20
    unsigned int control_id; // 0x24
    int doc_handle; // 0x28
    int data_group_handle; // 0x2C
    int data_item_handles[6]; // 0x30
    int num_data_group_items; // 0x48
    vint_data_item* data_group_items[6]; // 0x4C
    int poi_group_handle; // 0x64
    game_state desired_state; // 0x68
    bool desired_state_priority; // 0x6C
    float zoom_level; // 0x70
    float static_alpha_time; // 0x74
    bbox current_view_bbox; // 0x78
    bbox map_render_window; // 0x90
    bbox world_extents_bbox; // 0xA8
    vector map_center; // 0xC0
    vector temp_center; // 0xCC
    vector target_position; // 0xD8
    int static_bitmap_id; // 0xE4
    int static_bitmap_flip; // 0xE8
    int side_bitmap_id; // 0xEC
    int corner_bitmap_id; // 0xF0
    bm_image* side_image_info; // 0xF4
    bool display_gps_warning; // 0xF8
    char gps_warning_caption[64]; // 0xF9
    char gps_warning_desc[64]; // 0x139
    timestamp_realtime message_timer; // 0x17C
    audio_info audio_info; // 0x180
    stream_information stream_information[256]; // 0x194
    zone_map_information zone_map_info[77]; // 0x1194
    char territory_name[32]; // 0x18CC
    farray<zone_map_information*,77> zone_free_info; // 0x18EC
    farray<zone_map_information*,77> zone_used_info; // 0x1A30
    int district_value; // 0x1B74
    unsigned int last_district_handle; // 0x1B78
    farray<const mission_info*,64> mission_infos; // 0x1B7C
    zone_map_information full_screen_image_info; // 0x1C8C
    unsigned int full_screen_image_srid; // 0x1CA4
};

extern fullscreen Fs_info; // 0x02BD9328, a file static of fullscreen_map.cpp, the same unity as minimap.cpp
extern stream2_slot_allocator Fullscreen_map_stream_allocator; // 0x016612AC
extern bool Fullscreen_map_has_been_opened_through_script; // 0x02B9DD59

void unused_dcf_reload_fsmi(); // 0x0087A680 (:387)
int fsm_get_selected_district_show_type(); // 0x0083FF50 (:456)
bool fsm_is_targeting_player(); // 0x0083FF60 (:462)
bool fsm_get_district_color(const char* dist_name, unsigned char& r, unsigned char& g, unsigned char& b, unsigned char& a); // 0x00850C30 (:468)
bool fsm_get_district_color(const char* dist_name, float& r, float& g, float& b, float& a); // 0x00850C80 (:485)
void fsm_cancel_streaming(); // 0x00867220 (:656)
bool fsm_warp_to_safehouse_cb(int mbox_handle, int selection_index, msgbox_choice choice); // 0x008542A0 (:2967)
void fsm_free_streaming(); // 0x00867F20 (:3640)
void fsm_loaded_callback(unsigned int srid); // 0x00854CD0 (:3848)
void fsm_unloaded_callback(unsigned int srid); // 0x00854E00 (:3880)
void fullscreen_map_terr_info_init(int dlc_id); // 0x00876960 (:3903)
void fullscreen_map_init(); // 0x00882C90 (:3952)
void fullscreen_map_init_streaming_info(); // 0x00868020 (:3979)
void fullscreen_map_enter(game_state prev_state); // 0x00876AC0 (:4087)
void fullscreen_map_exit(game_state new_state); // 0x00868390 (:4143)
void fullscreen_map_process(); // 0x0087F2C0 (:4187)
void fullscreen_map_handshake(rl_renderer* pRenderer); // 0x0087BAB0 (:4240)
void fullscreen_map_render(rl_renderer*); // 0x00876C10 (:4292)
void fullscreen_map_handle_state_change(game_state new_state, bool high_priority); // 0x00841120 (:4363)
void fullscreen_map_get_zoom_info(fsmap_zoom_info& fzi); // 0x00841220 (:4394)
bool fsm_posn_is_locked_on(const vector& pos); // 0x00841250 (:4401)
float fullscreen_map_get_zoom_pct(); // 0x00841340 (:4406)
void fsm_display_gps_warning(const char* caption, const char* desc); // 0x00841360 (:4412)
vector fsm_get_map_center(); // 0x008413C0 (:4426)
bool fullscreen_map_is_loaded(); // 0x008413F0 (:4431)
bool fullscreen_map_exit_notify(game_state new_state); // 0x00841410 (:4436)
