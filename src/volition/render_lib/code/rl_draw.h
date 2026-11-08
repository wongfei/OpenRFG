// Original: volition/render_lib/code/rl_draw.h
// Implementation: volition/render_lib/code/rl_draw.cpp
//
// rl_draw, the singleton immediate drawing API on top of rl_primitive_renderer (shapes, bitmaps, strings).
#pragma once

#include "volition/render_lib/code/rl_base_object.h"
#include "volition/render_lib/code/rl_singleton.h"
#include "volition/render_lib/code/rl_state_manager.h"

class matrix;
class plane;
class rl_class_dynamic_info;
class rl_color_float;
struct rl_primitive_2d_vert;
struct rl_primitive_3d_vert;
class rl_primitive_state;
class rl_renderer;
class vector;

// size 0x5E0
class rl_draw : public rl_base_object, public rl_singleton<rl_draw>
{
protected:
    rl_draw(); // 0x004E4D00 (:197)
    virtual ~rl_draw(); // vtbl+0x00 0x004E4D40 (:201)
public:
    static int get_objects_peak();
    static int get_objects_used();
    static rl_draw* create(); // 0x0050DBA0 (:192)
    static bool can_create(); // 0x004BEB70 (:192)
    virtual void release(); // vtbl+0x04 0x004BEB80 (:192)
    void initialize(); // 0x00523FF0 (:207)
    void set_renderer(rl_renderer* pRenderer); // 0x005D1960 (.h:42)
    void initState(); // 0x0050DC30 (:213)
    void set_color(rl_color_float clr); // 0x0050DCC0 (:237)
    void get_color(rl_color_float& clr); // 0x004E4D60 (:242)
    void set_alpha(int a); // 0x0050DCE0 (:247)
    void set_texture(int bm_handle); // 0x0053A980 (:254)
    void set_bink_textures(int y_handle, int cr_handle, int cb_handle); // 0x0050DD50 (:259)
    void set_material(unsigned int mat_handle); // 0x00524080 (:264)
    void string(int sx, int sy, const wchar_t* s, bool centered, int fontnum, const rl_primitive_state& state); // 0x0055E000 (.h:196)
    void string(int sx, int sy, const char* s, bool centered, int fontnum, const rl_primitive_state& state); // 0x0055DFB0 (.h:185)
    void string(float sx, float sy, const wchar_t* s, bool centered, int fontnum, const rl_primitive_state& state); // 0x00555C10 (:289)
    void string(float sx, float sy, const char* s, bool centered, int fontnum, const rl_primitive_state& state); // 0x00555BC0 (:283)
    void string_scale(int sx, int sy, const wchar_t* s, float scale, bool centered, int fontnum, const rl_primitive_state& state); // 0x00554900 (.h:201)
    void string_scale(int sx, int sy, const char* s, float scale, bool centered, int fontnum, const rl_primitive_state& state); // 0x005548B0 (.h:190)
    void string_scale(float sx, float sy, const wchar_t* s, float scale, bool centered, int fontnum, const rl_primitive_state& state); // 0x00551A20 (:409)
    void string_scale(float sx, float sy, const char* s, float scale, bool centered, int fontnum, const rl_primitive_state& state); // 0x005514B0 (:305)
    void rect(int sx, int sy, int w, int h, const rl_primitive_state& state); // 0x00554950 (.h:207)
    void rect(float sx, float sy, float w, float h, const rl_primitive_state& state); // 0x00552000 (:539)
    void rect_outline(int sx, int sy, int w, int h, int thickness, const rl_primitive_state& state); // 0x005549A0 (.h:212)
    void rect_outline(float sx, float sy, float w, float h, float thickness, const rl_primitive_state& state); // 0x005521E0 (:608)
    void rect_rotate(int, int, int, int, float, const rl_primitive_state&);
    void rect_rotate(float csx, float csy, float w, float h, float angle, const rl_primitive_state& state); // 0x005526A0 (:708)
    void line(int sx1, int sy1, int sx2, int sy2, const rl_primitive_state& state); // 0x00554A00 (.h:223)
    void line(float sx1, float sy1, float sx2, float sy2, const rl_primitive_state& state); // 0x00552990 (:781)
    void circle(const vector& pt, float radius, const vector axis, const rl_primitive_state& state); // 0x00557B60 (:1501)
    void circle(int sx, int sy, int radius, const rl_primitive_state& state); // 0x00554A50 (.h:228)
    void circle(float sx, float sy, float radius, const rl_primitive_state& state); // 0x00552AD0 (:822)
    void bitmap(int bitmap_id, int sx, int sy, const rl_primitive_state& state); // 0x00554A90 (.h:233)
    void bitmap(int bitmap_id, float sx, float sy, const rl_primitive_state& state); // 0x00552DA0 (:891)
    void bitmap_center(int bitmap_id, int csx, int csy, const rl_primitive_state& state); // 0x00554AD0 (.h:238)
    void bitmap_center(int bitmap_id, float csx, float csy, const rl_primitive_state& state); // 0x00552E50 (:909)
    void bitmap_ex(int bitmap_id, int sx, int sy, int sw, int sh, int bx, int by, int bw, int bh, bool flip_x, bool flip_y, const rl_primitive_state& state); // 0x00554B10 (.h:243)
    void bitmap_ex(int bitmap_id, float sx, float sy, float sw, float sh, float bx, float by, float bw, float bh, bool flip_x, bool flip_y, const rl_primitive_state& state); // 0x00552F40 (:933)
    void bitmap_rotate(int, int, int, float, float, const rl_primitive_state&);
    void bitmap_rotate(int bitmap_id, float csx, float csy, float angle, float scale, const rl_primitive_state& state); // 0x00553260 (:1021)
    void tristrip_2d_begin(const rl_primitive_state& state); // 0x0050DDA0 (:1039)
    rl_primitive_2d_vert* tristrip_2d_allocate_tris(int num_tris); // 0x00553340 (:1062)
    void tristrip_2d_end(); // 0x00553370 (:1070)
    void linelist_2d_begin(const rl_primitive_state& state); // 0x0050DE00 (:1079)
    void linelist_2d_add_line(int, int, int, int);
    void linelist_2d_add_line(float sx1, float sy1, float sx2, float sy2); // 0x00553390 (:1096)
    void linelist_2d_end(); // 0x00553490 (:1128)
    void string_3d(const vector& pt, const matrix& orient, float scale, const char* s, int fontnum, const rl_primitive_state& state); // 0x005534B0 (:1147)
    void bitmap_3d(int bitmap_id, const vector& pt, const matrix& orient, float radius, const rl_primitive_state& state); // 0x00555C60 (:1252)
    void line_3d(const vector& pt1, const vector& pt2, const rl_primitive_state& state); // 0x005563C0 (:1319)
    void arrow_3d(const vector& pt1, const vector& pt2, const matrix& orient, float arrowhead_pct, const rl_primitive_state& state); // 0x00556420 (:1333)
    void box(const vector& pt, const matrix& orient, const vector& size, const rl_primitive_state& state); // 0x005568E0 (:1373)
    void bbox_aligned(const vector& mn, const vector& mx, const rl_primitive_state& state); // 0x005574C0 (:1418)
    void bbox_oriented(const vector& mn, const vector& mx, const vector& pt, const matrix& orient, const rl_primitive_state& state); // 0x005575D0 (:1433)
    void bbox_solid(const vector& mn, const vector& mx, const rl_primitive_state& state); // 0x00557760 (:1449)
    void circle_xz(const vector& pt, float radius, const rl_primitive_state& state); // 0x00557F10 (:1556)
    void sphere(const vector& pt, float radius, const rl_primitive_state& state); // 0x00557F60 (:1567)
    void sphere_solid(const vector& pt, float radius, const rl_primitive_state& state); // 0x00558720 (:1640)
    void cone(const vector& pt, const matrix& orient, float radius, float height, const rl_primitive_state& state); // 0x005593A0 (:1764)
    void cone_solid(const vector& pt, const matrix& orient, float radius, float height, const rl_primitive_state& state); // 0x005596F0 (:1815)
    void plane_3d(const vector& pt, const matrix& orient, float w, float h, const rl_primitive_state& state); // 0x00559960 (:1877)
    void plane_3d_solid(const vector& pt, const matrix& orient, const rl_primitive_state& state); // 0x00559FA0 (:1900)
    void axis(const vector& pt, const matrix& orient, float scale, const rl_primitive_state& state); // 0x0055A6B0 (:1933)
    void arc_xz(const vector& pt, const vector& start, float angle, const rl_primitive_state& state); // 0x0055AB80 (:1970)
    void cylinder(const vector& pt, const matrix& orient, float radius, float height, const rl_primitive_state& state); // 0x0055AFA0 (:2004)
    void frustum(const vector& pt, const plane* planes, int nplanes, const rl_primitive_state& state); // 0x00560170 (:2049)
    void frustum_solid(const vector& pt, const plane* planes, int nplanes, const rl_primitive_state& state); // 0x00560190 (:2061)
    void poly_3d(const rl_primitive_3d_vert* verts, int nverts, const rl_primitive_state& state); // 0x0055B530 (:2075)
    void tristrip_3d_begin(const rl_primitive_state& state); // 0x0050DE30 (:2118)
    rl_primitive_3d_vert* tristrip_3d_allocate_tris(int num_tris); // 0x00553C80 (:2137)
    void tristrip_3d_end(); // 0x00553CB0 (:2145)
    void linelist_3d_begin(const rl_primitive_state& state); // 0x0050DE70 (:2154)
    void linelist_3d_add_line(const vector& pt1, const vector& pt2); // 0x00553CD0 (:2171)
    void linelist_3d_end(); // 0x00553DD0 (:2201)
    void set_state(const rl_primitive_state& state); // 0x0050DEA0 (:2206)
    rl_state_manager& get_rl_state(); // 0x004B8C00 (.h:165)
    rl_renderer* get_renderer(); // 0x00884BE0 (.h:166)
protected:
    void frustum_internal(const vector& pt, const plane* planes, int nplanes, bool solid, const rl_primitive_state& state); // 0x0055F5B0 (:98)

public:
    static rl_class_dynamic_info ms_class_dynamic_info; // 0x01514A80
protected:
    rl_state_manager m_state; // 0x10
    rl_renderer* m_pRenderer; // 0x5D0
};
