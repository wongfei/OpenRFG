// Original: volition/render_lib/code/gr/grd.h
// Implementation: volition/render_lib/code/gr/grd.cpp
//
// grd, gr debug drawing (lines, boxes, spheres, text in 3D) queued for the debug overlay.
#pragma once


class color;
class gr_state;
class matrix;
class vector;

void grd_set_color(const color& c); // 0x007111B0 (.h:49)
gr_state grd_get_state(); // 0x004B8DB0 (:29)
void grd_set_state(const gr_state& state); // 0x004B8DD0 (:34)
void grd_string(int sx, int sy, const char* s, const gr_state& state); // 0x004B8DF0 (:45)
void grd_string_scale(int sx, int sy, const char* s, float scale, const gr_state& state); // 0x004B8E00 (:50)
void grd_string(int sx, int sy, const wchar_t* s, const gr_state& state); // 0x004B8E10 (:55)
void grd_string_scale(int sx, int sy, const wchar_t* s, float scale, const gr_state& state); // 0x004B8E20 (:59)
void grd_3d_string(const vector& pos, const matrix& orient, float scale, const char* s, const gr_state& state); // 0x004B8E30 (:65)
void grd_get_string_size(int& w, int& h, const char* text, int len); // 0x004B8E40 (:70)
void grd_get_string_size(int& w, int& h, const wchar_t* text, int len); // 0x004B8E60 (:77)
int grd_get_font_height(); // 0x004B8E80 (:83)
void grd_rect(int x, int y, int w, int h, const gr_state& state); // 0x004B8E90 (:88)
void grd_set_color(int r, int g, int b, int a); // 0x004B8EA0 (:95)
void grd_set_alpha(int a); // 0x004B8ED0 (:100)
void grd_line(int x1, int y1, int x2, int y2, const gr_state& state, bool clip); // 0x004B8F00 (:106)
bool grd_sphere(const vector& center, float radius, const gr_state& state); // 0x004B8F10 (:111)
bool grd_sphere_solid(const vector& center, float radius, int min_sections, const gr_state& state); // 0x004B8F20 (:116)
void grd_3d_arrow(const vector& v0, const vector& v1, float arrowhead_pc, const gr_state& state); // 0x004B8F30 (:121)
void grd_3d_line(const vector& v0, const vector& v1, const gr_state& state); // 0x004B8F40 (:125)
bool grd_bbox_aligned(const vector& mn, const vector& mx, const gr_state& state); // 0x004B8F50 (:130)
void grd_bbox_solid(const vector& bmin, const vector& bmax, const gr_state& state); // 0x004B8F60 (:135)
bool grd_bbox_oriented(const vector& mn, const vector& mx, const vector& pos, const matrix& orient, const gr_state& state); // 0x004B8F70 (:139)
bool grd_circle(const vector& center, float radius, const vector axis, int segments, const gr_state& state); // 0x004B8F80 (:144)
void grd_circle(int x, int y, int radius, const gr_state& state); // 0x004B8F90 (:149)
void grd_circle_xz(const vector& center, float radius, const gr_state& state); // 0x004B8FA0 (:154)
void grd_rect_outline(int x, int y, int w, int h, int thickness, const gr_state& state); // 0x004B8FB0 (:159)
void grd_cylinder(const vector& center, const matrix& orient, float radius, float height, const gr_state& state); // 0x004B8FC0 (:164)
void grd_start_instance(const vector& pos, const matrix& orient, const matrix* scale); // 0x004B8FD0 (:169)
void grd_stop_instance(); // 0x004B8FE0 (:174)
bool grd_screen_coords_from_world_coords(const vector& world_pos, float* screen_x, float* screen_y, float* screen_z, int allow_off_edges); // 0x004B8FF0 (:179)
bool grd_cone(const vector& cone_point_pos, const matrix& orient, float radius, float height, const gr_state& state); // 0x004B9020 (:188)
void grd_axis(const vector& pos, const matrix& orient, float scale, const gr_state& state); // 0x004B9030 (:193)
void grd_rect(float x, float y, float w, float h, const gr_state& state); // 0x004B9040 (:198)
bool grd_box(const vector& center, const matrix& orient, const vector& size, const gr_state& state); // 0x004B9050 (:203)
void grd_instance_world_pos(vector& local_pos, const vector& world_pos); // 0x004B9060 (:208)
int grd_safe_area_left(); // 0x004B9070 (:276)
int grd_safe_area_right(); // 0x004DD9E0 (:281)
int grd_safe_area_top(); // 0x004B9080 (:286)
int grd_safe_area_bottom(); // 0x004DDA00 (:291)
int grd_safe_area_width(); // 0x004DDA20 (:296)
int grd_safe_area_height(); // 0x004DDA40 (:301)
void grd_get_safe_area_pct(float& percentage_x, float& percentage_y); // 0x004B9090 (:306)
