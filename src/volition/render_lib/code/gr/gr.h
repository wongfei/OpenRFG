// Original: volition/render_lib/code/gr/gr.h
// Implementation: volition/render_lib/code/gr/gr.cpp
//
// gr, the legacy Volition 2D/3D drawing API kept on top of render_lib: safe area, scissor, color and texture state
// (gr_state), rectangles, lines, bitmaps and text. gr_3d.cpp and gr_font.cpp have no headers of their own; the
// gr_3d.cpp functions ported so far (VFX.md FX5: the world-to-screen projection) are declared here.
#pragma once

#include "volition/render_lib/code/gr/gr_defs.h"
#include "volition/vlib/code/util/endian_type.h"

class color;
class matrix;
class plane;
class rl_primitive_state;
class vector;

// size 0x14
struct font_char
{
    unsigned int unicode_code_point; // 0x00
    int spacing; // 0x04
    int byte_width; // 0x08
    int offset; // 0x0C
    short kerning_entry; // 0x10
    unsigned short user_data; // 0x12
};

// size 0xC
struct font_kernpair
{
    unsigned int c1; // 0x00
    unsigned int c2; // 0x04
    signed char offset; // 0x08
    unsigned char pad[3]; // 0x09
};

// size 0x2C
// A point of the legacy 3D API: clip space (rx, ry, rz, w), screen (sx, sy, sw), the clip codes and flags of
// gr_rotate_vertex/gr_project_vertex. Placement inferred.
struct vertex
{
    float rx; // 0x00
    float ry; // 0x04
    float rz; // 0x08
    float w; // 0x0C
    float sx; // 0x10
    float sy; // 0x14
    float sw; // 0x18
    unsigned char codes; // 0x1C
    unsigned char flags; // 0x1D
    unsigned char pad2; // 0x1E
    float u1; // 0x20
    float v1; // 0x24
    unsigned char r; // 0x28
    unsigned char g; // 0x29
    unsigned char b; // 0x2A
    unsigned char a; // 0x2B
};

// size 0x8
class gr_state
{
public:
    gr_alpha_mode get_alpha_mode() const; // 0x004B66C0 (gr_defs.h:119)
    gr_clamp_mode get_clamp_mode() const; // 0x004B66D0 (gr_defs.h:120)
    gr_zbuf_mode get_zbuf_mode() const; // 0x004B66E0 (gr_defs.h:121)
    gr_stencil_mode get_stencil_mode() const; // 0x004B66F0 (gr_defs.h:122)
    unsigned int get_const_alpha() const; // 0x004B6700 (gr_defs.h:123)
    gr_cull_mode get_cull_mode() const; // 0x004B6710 (gr_defs.h:124)
    gr_tnl_clip_mode get_tnl_clip_mode() const;
    gr_tnl_light_mode get_tnl_light_mode() const;
    void set_alpha_mode(gr_alpha_mode);
    void set_clamp_mode(gr_clamp_mode);
    void set_zbuf_mode(gr_zbuf_mode);
    void set_stencil_mode(gr_stencil_mode stm); // 0x0083F520 (gr_defs.h:132)
    void set_const_alpha(unsigned int);
    void set_cull_mode(gr_cull_mode);
    void set_tnl_clip_mode(gr_tnl_clip_mode);
    void set_tnl_light_mode(gr_tnl_light_mode);
    gr_state(const gr_state& _v); // 0x004B8D20 (:728)
    gr_state(gr_alpha_mode am, gr_clamp_mode cm, gr_zbuf_mode zm, gr_stencil_mode stm, unsigned int const_alpha, gr_cull_mode tclm, gr_tnl_clip_mode tcm, gr_tnl_light_mode tlm); // 0x004B8CE0 (:722)
    gr_state(); // 0x004B8CD0 (:716)
    void invalidate(); // 0x004B8D40 (:734)
    bool valid(); // 0x004B8D50 (:740)
    bool operator==(const gr_state& state); // 0x004B8D70 (:745)
    void operator=(const gr_state& state); // 0x004B8D90 (:750)

private:
    // Access inferred: the converter reads the packed modes (orig_main.md OM5).
    friend void gr_convert_to_rl_state(rl_primitive_state& rl_state, const gr_state& state);

    unsigned int value; // 0x00
    unsigned int tnl_value; // 0x04
};

// size 0xD8
struct font
{
    int id; // 0x00
    int version; // 0x04
    unsigned int language_id; // 0x08
    unsigned int num_chars; // 0x0C
    unsigned int w; // 0x10
    short h; // 0x14
    short render_h; // 0x16
    int baseline_offset; // 0x18
    int character_spacing; // 0x1C
    unsigned int num_kern_pairs; // 0x20
    short vert_offset; // 0x24
    short hori_offset; // 0x26
    char peg_name[64]; // 0x28
    char bitmap_name[64]; // 0x68
    et_sized_native_pointer<font_kernpair> kern_data; // 0xA8
    et_sized_native_pointer<font_char> char_data; // 0xB0
    int bitmap_id; // 0xB8
    et_sized_native_pointer<int> bm_u; // 0xC0
    et_sized_native_pointer<int> bm_v; // 0xC8
    float global_scale; // 0xD0
    short leading_offset; // 0xD4
    unsigned char is_reversed; // 0xD6
    unsigned char uses_hyphens; // 0xD7
};

int gr_screen_width(); // 0x004DCAC0 (.h:23)
int gr_screen_height(); // 0x004DCAD0 (.h:28)
void unused_dcf_set_safe_area(); // 0x00520970 (:168)
void gr_set_safe_area(float percentage_x, float percentage_y); // 0x00509080 (:211)
int gr_safe_area_left(); // 0x004B8C10 (:226)
int gr_safe_area_right(); // 0x004DD8A0 (:231)
int gr_safe_area_top(); // 0x004B8C20 (:236)
int gr_safe_area_bottom(); // 0x004DD8C0 (:241)
int gr_safe_area_width(); // 0x004DD8E0 (:246)
int gr_safe_area_height(); // 0x004DD900 (:251)
void gr_get_safe_area_pct(float& percentage_x, float& percentage_y); // 0x004B8C30 (:256)
void gr_set_scissor_region(int left, int top, int right, int bottom, int); // 0x00509120 (:263)
void gr_get_scissor_region(int& left, int& top, int& right, int& bottom); // 0x004DD920 (:274)
void gr_set_state(const gr_state& state); // 0x00520A90 (:284)
void gr_get_color(int& r, int& g, int& b, int& a); // 0x004DD970 (:291)
// The 2D states of gr.cpp (orig_main.md OM5; MP4 found GM_FILTER, the multiplayer timer's state).
extern gr_state GM_NO_ALPHA; // 0x0150A630
extern gr_state GM_NO_ALPHA_WRAP; // 0x0150A638
extern gr_state GM_NO_ALPHA_Z; // 0x0150A640
extern gr_state GM_FILTER; // 0x0150A648
extern gr_state GM_FILTER_WRAP; // 0x0150A650
extern gr_state GM_FILTER_CLAMP; // 0x0150A658
extern gr_state GM_FILTER_Z; // 0x0150A660
extern gr_state GM_FILTER_Z_LIT; // 0x0150A668
extern gr_state GM_FILTER_Z_READ; // 0x0150A670
extern gr_state GM_FILTER_Z_READ_LIT; // 0x0150A678
extern gr_state GM_FILTER_STENCIL_WRITE; // 0x0150A680
extern gr_state GM_ADDITIVE; // 0x0150A688
extern gr_state GM_ADDITIVE_ALPHA; // 0x0150A690
extern gr_state GM_ADDITIVE_Z; // 0x0150A698
extern gr_state GM_FILTER_SCREEN; // 0x0150A6A0
extern gr_state GM_STENCIL_WRITE; // 0x0150A6A8
extern gr_state GM_STENCIL_TEST; // 0x0150A6B0
extern gr_state GM_FILTER_STENCIL_TEST; // 0x0150A6B8
extern gr_state GM_NO_ALPHA_NO_CLIP; // 0x0150A6C0
extern gr_state GM_NO_ALPHA_Z_NO_CLIP; // 0x0150A6C8
extern gr_state GM_FILTER_NO_CLIP; // 0x0150A6D0
extern gr_state GM_FILTER_Z_NO_CLIP; // 0x0150A6D8
extern gr_state GM_FILTER_Z_LIT_NO_CLIP; // 0x0150A6E0
extern gr_state GM_FILTER_Z_READ_NO_CLIP; // 0x0150A6E8
extern gr_state GM_FILTER_Z_READ_LIT_NO_CLIP; // 0x0150A6F0
extern gr_state GM_ADDITIVE_NO_CLIP; // 0x0150A6F8
extern gr_state GM_ADDITIVE_ALPHA_NO_CLIP; // 0x0150A700
extern gr_state GM_ADDITIVE_ALPHA_Z_READ_NO_CLIP; // 0x0150A708
extern gr_state GM_ADDITIVE_Z_NO_CLIP; // 0x0150A710

void gr_set_color(int r, int g, int b, int a); // 0x00509170 (:309)
void gr_set_color(float r, float g, float b, float a); // 0x00509210 (:321)
void gr_set_color(const color& c); // 0x005092C0 (:326)
void gr_set_alpha(int a); // 0x00509360 (:331)
void gr_set_texture(int bitmap_id1); // 0x00536D60 (:342)
void gr_rect_outline(int x, int y, int w, int h, int t, const gr_state& state); // 0x00554BB0 (:347)
void gr_rect(int x1, int y1, int w, int h, const gr_state& state); // 0x00554C70 (:356)
void gr_rect(float x, float y, float w, float h, const gr_state& state); // 0x00554D20 (:366)
void gr_line(float x1, float y1, float x2, float y2, const gr_state& state, bool); // 0x00554DD0 (:387)
void gr_line(int x1, int y1, int x2, int y2, const gr_state& state, bool); // 0x00554E60 (:394)
void gr_bitmap(int bitmap_id, int x, int y, const gr_state& state); // 0x00554F00 (:401)
void gr_bitmap_scaled(int bitmap_id, int x, int y, int w, int h, const gr_state& state); // 0x00554F80 (:414)
void gr_bitmap_scaled(int bitmap_id, float x, float y, float w, float h, const gr_state& state); // 0x00555060 (:422)
void gr_bitmap_centered(int bitmap_id, int cx, int cy, const gr_state& state); // 0x00555130 (:434)
void gr_bitmap_ex(int bitmap_id, int x, int y, int w, int h, int sx, int sy, int sw, int sh, bool flip_x, bool flip_y, const gr_state& state); // 0x005551B0 (:441)
void gr_bitmap_rotated_scale(int bitmap_id, float x, float y, float angle, float scalew, float scaleh, const gr_state& state); // 0x005552A0 (:450)
void gr_bitmap_rotated(int bitmap_id, float x, float y, float angle, float scale, const gr_state& state); // 0x00555380 (:470)
void gr_circle(int x, int y, int radius, const gr_state& state); // 0x00555420 (:484)
void gr_3d_bitmap(int bitmap_id, const vector& pnt, const matrix& orient, float radius, const gr_state& state); // 0x0055E050 (:493)
void gr_3d_line(const vector& v0, const vector& v1, const gr_state& state); // 0x0055E0D0 (:505)
void gr_3d_arrow(const vector& v1, const vector& v2, float arrowhead_pct, const gr_state& state); // 0x0055E190 (:514)
bool gr_sphere(const vector& center, float radius, const gr_state& state); // 0x0055E230 (:532)
bool gr_sphere_solid(const vector& center, float radius, int, const gr_state& state); // 0x0055E2C0 (:542)
bool gr_cone(const vector& cone_point_pos, const matrix& orient, float radius, float height, const gr_state& state); // 0x0055E350 (:552)
bool gr_box(const vector& center, const matrix& orient, const vector& size, const gr_state& state); // 0x0055E3F0 (:570)
bool gr_bbox_aligned(const vector& mn, const vector& mx, const gr_state& state); // 0x0055E470 (:580)
bool gr_bbox_oriented(const vector& mn, const vector& mx, const vector& pos, const matrix& orient, const gr_state& state); // 0x0055E5F0 (:590)
void gr_bbox_solid(const vector& bmin, const vector& bmax, const gr_state& state); // 0x0055E7E0 (:600)
void gr_cylinder(const vector& center, const matrix& orient, float radius, float height, const gr_state& state); // 0x0055E860 (:608)
void gr_plane(const vector& pt, const matrix& orient, float w, float h, const gr_state& state); // 0x0055E900 (:616)
void gr_plane_solid(const plane& pl, const gr_state& state); // 0x0055E9A0 (:624)
void gr_axis(const vector& pos, const matrix& orient, float scale, const gr_state& state); // 0x0055EA90 (:638)
bool gr_circle(const vector& center, float radius, const vector axis, int, const gr_state& state); // 0x0055EB20 (:653)
void gr_circle_xz(const vector& center, float radius, const gr_state& state); // 0x0055EBF0 (:669)
void gr_arc_xz(const vector& center, const vector& start, float angle, const gr_state& state); // 0x0055ECC0 (:677)
// VFX.md FX5: the world-to-screen projection of gr_3d.cpp (the radial blur position).
bool gr_screen_coords_from_world_coords(const vector& world_pos, float* screen_x, float* screen_y, float* screen_z, int allow_off_edges); // 0x00509620 (gr_3d.cpp:167)
unsigned int gr_rotate_vertex(vertex& dest, const vector& src); // 0x004DDB00 (gr_3d.cpp:361)
unsigned int gr_project_vertex(vertex& p); // 0x004DDD10 (gr_3d.cpp:420)
// multiplayer.md MP10: the screen rectangle of a world box (the backpack rack popups).
bool gr_get_bbox_2d_rect(const vector& bmin, const vector& bmax, float& x, float& y, float& w, float& h); // 0x004DF4F0 (gr_3d.cpp:687)
void gr_frustum_solid(const vector& eye_pos, const plane* planes, int nplanes, const gr_state& state); // 0x00561C20 (:686)
void gr_frustum(const vector& eye_pos, const plane* planes, int nplanes, const gr_state& state); // 0x00561CA0 (:694)
void gr_convert_to_rl_state(rl_primitive_state& rl_state, const gr_state& state); // 0x004B8C60 (:704)
