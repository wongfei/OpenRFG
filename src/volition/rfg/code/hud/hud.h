// Original: volition/rfg/code/hud/hud.h
// Implementation: volition/rfg/code/hud/hud.cpp
//
// The non-vint HUD: damage indicators (hud_register_hit), the pickup message, the respawn reason, the resolution
// multipliers, hud_system_init/process/render and the user interface table (parse_user_interface_table).
#pragma once

#include "volition/rfg/code/objects/humans/human_teams.h"

class color;
class gr_state;
class vector;
class vector2;
struct xml_element;

extern float X_res_multiplier; // 0x02B953E8
extern float Y_res_multiplier; // 0x02B95600
extern bool Show_minimap; // 0x01660FF3
extern int Center_screen_x; // 0x02B953EC
extern int Center_screen_y; // 0x02B953F0
extern bool Debug_show_hud; // 0x01660EE9
extern int Hide_hud_ref_count; // 0x02B9DC68

void dcf_hud_off(); // 0x00854E80 (:242)
void dcf_hud_on(); // 0x00854E90 (:247)
xml_element* hud_parse_offset(xml_element* root, const char* ratio_string, const char* desired_cluster_name, int* pos_x, int* pos_y, bool resi); // 0x00841530 (:325)
void hud_render_control_morale_bar(human_teams team, int x, int y, int width, int height, int border, float percent, float alpha); // 0x00841930 (:918)
void hud_refresh_user_interface(); // 0x00855390 (:1213)
void hud_system_critical_init(); // 0x008553A0 (:1218)
void hud_system_init(); // 0x00855420 (:1233)
void hud_level_init_post(); // 0x00868500 (:1267)
void hud_level_shutdown(); // 0x0087F3F0 (:1283)
void hud_reset_damage_indicators(); // 0x00855520 (:1290)
void hud_process_damage_indicators(); // 0x00855560 (:1301)
void hud_process(); // 0x00855750 (:1319)
void hud_render(); // 0x0087F400 (:1340)
void hud_hide(bool hide); // 0x00841A90 (:1472)
bool hud_is_hidden(); // 0x00841AC0 (:1482)
void hud_enable_button_input(bool enabled); // 0x00841AE0 (:1487)
bool hud_is_button_input_enabled(); // 0x00841AF0 (:1492)
void hud_display_progress_bar(float percent_complete, int x, int y, int width, int height, int border, const color& border_color, const color& bg_color, const color& complete_color); // 0x00841B00 (:1510)
void hud_display_cash(bool show, unsigned int display_time_ms); // 0x00841BC0 (:1529)
void hud_display_followers(bool show, unsigned int display_time_ms); // 0x00841BF0 (:1545)
void hud_register_hit(const vector& direction, float percent_intensity); // 0x008557F0 (:1555)
void hud_display_firearm_interface(bool display); // 0x00841C20 (:1590)
void hud_display_pickup_messages(bool display); // 0x00841C30 (:1595)
void hud_display_default(); // 0x00841C40 (:1602)
void hud_bm_res_independent(int bitmap_id, int cx, int cy, float scale, gr_state state); // 0x00841C50 (:1612)
void hud_bm_res_independent_ex_flip(int bitmap_id, int cx, int cy, bool flip_x, bool flip_y, gr_state state); // 0x00841D80 (:1641)
void hud_bm_pos_res_independent(int bitmap_id, int cx, int cy, float scale, gr_state state); // 0x00841EA0 (:1669)
void hud_bm_pos_res_independent_ex_cluster(int bitmap_id, int cx, int cy, int xo, int yo, float scale, gr_state state); // 0x00842030 (:1698)
void hud_bm_pos_res_independent_rect(int x, int y, int xo, int yo, int w, int h, float scale, gr_state state); // 0x00842270 (:1729)
void hud_bm_pos_res_independent_ex_stretch(int bitmap_id, int x, int y, float scale, float sw, float sh, gr_state state); // 0x00842460 (:1755)
void hud_bm_pos_res_independent_ex_bitmap(int bitmap_id, int x, int y, int xo, int yo, int bx, int by, int bw, int bh, float scale, gr_state state); // 0x00842650 (:1788)
int hud_string_wordwrap(int sx, int sy, int max_w, const char* s, int fontnum, const gr_state& state, bool skip_render); // 0x00842860 (:1818)
void hud_screen_render(vector2& center, vector2& diameter, int bm_crn, int bm_left, int bm_top, int bm_mid, int bm_lend, int bm_lside, float complete, const float alpha); // 0x00842960 (:1879)
void hud_display_distance(bool display); // 0x00842D80 (:1934)
void hud_distance_set_controls(unsigned int handle, float min, float max, float shaded_min, float shaded_max); // 0x00842D90 (:1939)
bool hud_damage_indicators_enabled(); // 0x00842DE0 (:1949)
