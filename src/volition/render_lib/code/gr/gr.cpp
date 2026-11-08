// Original: volition/render_lib/code/gr/gr.cpp
//
// Partial (orig_main.md OM5): the gr state (gr_state), the safe area, the scissor region, the color and texture, and the
// 2D drawing (rectangles, lines, bitmaps, circles), all forwarded to rl_draw. The 3D functions (gr_3d_*, gr_sphere,
// gr_box, gr_frustum, ...) are not ported: they forward to rl_draw's 3D path, which is not ported either.
// Every 2D function converts its gr_state to an rl_primitive_state inline (the same bit shuffle as
// gr_convert_to_rl_state); here they call gr_convert_to_rl_state.

#include "volition/render_lib/code/gr/gr.h"

#include "volition/render_lib/code/rl_color.h"
#include "volition/render_lib/code/rl_draw.h"
#include "volition/render_lib/code/rl_primitive_state.h"
#include "volition/render_lib/code/rl_rect.h"
#include "volition/render_lib/code/rl_render_lib.h"
#include "volition/render_lib/code/rl_screen_info.h"
#include "volition/vlib/code/util/color.h"

// The 2D states (constant-initialized in .data). Magic numbers: a constant alpha of 0xFF; "NO_CLIP" states have
// TNL_CLIP_MODE_NONE, the others TNL_CLIP_MODE_CLIP.
gr_state GM_NO_ALPHA(ALPHA_BLEND_NONE, CLAMP_MODE_WRAP, ZBUF_NONE, STENCIL_NONE, 0xFF, CULL_MODE_NONE, TNL_CLIP_MODE_CLIP, TNL_LIGHT_MODE_NONE); // 0x0150A630
gr_state GM_NO_ALPHA_WRAP(ALPHA_BLEND_NONE, CLAMP_MODE_WRAP, ZBUF_NONE, STENCIL_NONE, 0xFF, CULL_MODE_NONE, TNL_CLIP_MODE_CLIP, TNL_LIGHT_MODE_NONE); // 0x0150A638
gr_state GM_NO_ALPHA_Z(ALPHA_BLEND_NONE, CLAMP_MODE_WRAP, ZBUF_FULL, STENCIL_NONE, 0xFF, CULL_MODE_NONE, TNL_CLIP_MODE_CLIP, TNL_LIGHT_MODE_NONE); // 0x0150A640
gr_state GM_FILTER(ALPHA_BLEND_FILTER, CLAMP_MODE_WRAP, ZBUF_NONE, STENCIL_NONE, 0xFF, CULL_MODE_NONE, TNL_CLIP_MODE_CLIP, TNL_LIGHT_MODE_NONE); // 0x0150A648
gr_state GM_FILTER_WRAP(ALPHA_BLEND_FILTER, CLAMP_MODE_WRAP, ZBUF_NONE, STENCIL_NONE, 0xFF, CULL_MODE_NONE, TNL_CLIP_MODE_CLIP, TNL_LIGHT_MODE_NONE); // 0x0150A650
gr_state GM_FILTER_CLAMP(ALPHA_BLEND_FILTER, CLAMP_MODE_CLAMP, ZBUF_NONE, STENCIL_NONE, 0xFF, CULL_MODE_NONE, TNL_CLIP_MODE_CLIP, TNL_LIGHT_MODE_NONE); // 0x0150A658
gr_state GM_FILTER_Z(ALPHA_BLEND_FILTER, CLAMP_MODE_WRAP, ZBUF_FULL, STENCIL_NONE, 0xFF, CULL_MODE_NONE, TNL_CLIP_MODE_CLIP, TNL_LIGHT_MODE_NONE); // 0x0150A660
gr_state GM_FILTER_Z_LIT(ALPHA_BLEND_FILTER, CLAMP_MODE_WRAP, ZBUF_FULL, STENCIL_NONE, 0xFF, CULL_MODE_NONE, TNL_CLIP_MODE_CLIP, TNL_LIGHT_MODE_LIGHT); // 0x0150A668
gr_state GM_FILTER_Z_READ(ALPHA_BLEND_FILTER, CLAMP_MODE_WRAP, ZBUF_READ, STENCIL_NONE, 0xFF, CULL_MODE_NONE, TNL_CLIP_MODE_CLIP, TNL_LIGHT_MODE_NONE); // 0x0150A670
gr_state GM_FILTER_Z_READ_LIT(ALPHA_BLEND_FILTER, CLAMP_MODE_WRAP, ZBUF_READ, STENCIL_NONE, 0xFF, CULL_MODE_NONE, TNL_CLIP_MODE_CLIP, TNL_LIGHT_MODE_LIGHT); // 0x0150A678
gr_state GM_FILTER_STENCIL_WRITE(ALPHA_BLEND_FILTER, CLAMP_MODE_WRAP, ZBUF_NONE, STENCIL_WRITE, 0xFF, CULL_MODE_NONE, TNL_CLIP_MODE_CLIP, TNL_LIGHT_MODE_NONE); // 0x0150A680
gr_state GM_ADDITIVE(ALPHA_BLEND_ADDITIVE, CLAMP_MODE_WRAP, ZBUF_NONE, STENCIL_NONE, 0xFF, CULL_MODE_NONE, TNL_CLIP_MODE_CLIP, TNL_LIGHT_MODE_NONE); // 0x0150A688
gr_state GM_ADDITIVE_ALPHA(ALPHA_BLEND_ALPHA_ADDITIVE, CLAMP_MODE_WRAP, ZBUF_NONE, STENCIL_NONE, 0xFF, CULL_MODE_NONE, TNL_CLIP_MODE_CLIP, TNL_LIGHT_MODE_NONE); // 0x0150A690
gr_state GM_ADDITIVE_Z(ALPHA_BLEND_ADDITIVE, CLAMP_MODE_WRAP, ZBUF_FULL, STENCIL_NONE, 0xFF, CULL_MODE_NONE, TNL_CLIP_MODE_CLIP, TNL_LIGHT_MODE_NONE); // 0x0150A698
gr_state GM_FILTER_SCREEN(ALPHA_BLEND_FILTER_SCREEN, CLAMP_MODE_WRAP, ZBUF_NONE, STENCIL_NONE, 0xFF, CULL_MODE_NONE, TNL_CLIP_MODE_NONE, TNL_LIGHT_MODE_NONE); // 0x0150A6A0
gr_state GM_STENCIL_WRITE(ALPHA_BLEND_NONE, CLAMP_MODE_WRAP, ZBUF_NONE, STENCIL_WRITE, 0xFF, CULL_MODE_NONE, TNL_CLIP_MODE_NONE, TNL_LIGHT_MODE_NONE); // 0x0150A6A8
gr_state GM_STENCIL_TEST(ALPHA_BLEND_NONE, CLAMP_MODE_WRAP, ZBUF_NONE, STENCIL_TEST, 0xFF, CULL_MODE_NONE, TNL_CLIP_MODE_NONE, TNL_LIGHT_MODE_NONE); // 0x0150A6B0
gr_state GM_FILTER_STENCIL_TEST(ALPHA_BLEND_FILTER, CLAMP_MODE_WRAP, ZBUF_NONE, STENCIL_TEST, 0xFF, CULL_MODE_NONE, TNL_CLIP_MODE_NONE, TNL_LIGHT_MODE_NONE); // 0x0150A6B8
gr_state GM_NO_ALPHA_NO_CLIP(ALPHA_BLEND_NONE, CLAMP_MODE_WRAP, ZBUF_NONE, STENCIL_NONE, 0xFF, CULL_MODE_NONE, TNL_CLIP_MODE_NONE, TNL_LIGHT_MODE_NONE); // 0x0150A6C0
gr_state GM_NO_ALPHA_Z_NO_CLIP(ALPHA_BLEND_NONE, CLAMP_MODE_WRAP, ZBUF_FULL, STENCIL_NONE, 0xFF, CULL_MODE_NONE, TNL_CLIP_MODE_NONE, TNL_LIGHT_MODE_NONE); // 0x0150A6C8
gr_state GM_FILTER_NO_CLIP(ALPHA_BLEND_FILTER, CLAMP_MODE_WRAP, ZBUF_NONE, STENCIL_NONE, 0xFF, CULL_MODE_NONE, TNL_CLIP_MODE_NONE, TNL_LIGHT_MODE_NONE); // 0x0150A6D0
gr_state GM_FILTER_Z_NO_CLIP(ALPHA_BLEND_FILTER, CLAMP_MODE_WRAP, ZBUF_FULL, STENCIL_NONE, 0xFF, CULL_MODE_NONE, TNL_CLIP_MODE_NONE, TNL_LIGHT_MODE_NONE); // 0x0150A6D8
gr_state GM_FILTER_Z_LIT_NO_CLIP(ALPHA_BLEND_FILTER, CLAMP_MODE_WRAP, ZBUF_FULL, STENCIL_NONE, 0xFF, CULL_MODE_NONE, TNL_CLIP_MODE_NONE, TNL_LIGHT_MODE_LIGHT); // 0x0150A6E0
gr_state GM_FILTER_Z_READ_NO_CLIP(ALPHA_BLEND_FILTER, CLAMP_MODE_WRAP, ZBUF_READ, STENCIL_NONE, 0xFF, CULL_MODE_NONE, TNL_CLIP_MODE_NONE, TNL_LIGHT_MODE_NONE); // 0x0150A6E8
gr_state GM_FILTER_Z_READ_LIT_NO_CLIP(ALPHA_BLEND_FILTER, CLAMP_MODE_WRAP, ZBUF_READ, STENCIL_NONE, 0xFF, CULL_MODE_NONE, TNL_CLIP_MODE_NONE, TNL_LIGHT_MODE_LIGHT); // 0x0150A6F0
gr_state GM_ADDITIVE_NO_CLIP(ALPHA_BLEND_ADDITIVE, CLAMP_MODE_WRAP, ZBUF_NONE, STENCIL_NONE, 0xFF, CULL_MODE_NONE, TNL_CLIP_MODE_NONE, TNL_LIGHT_MODE_NONE); // 0x0150A6F8
gr_state GM_ADDITIVE_ALPHA_NO_CLIP(ALPHA_BLEND_ALPHA_ADDITIVE, CLAMP_MODE_WRAP, ZBUF_NONE, STENCIL_NONE, 0xFF, CULL_MODE_NONE, TNL_CLIP_MODE_NONE, TNL_LIGHT_MODE_NONE); // 0x0150A700
gr_state GM_ADDITIVE_ALPHA_Z_READ_NO_CLIP(ALPHA_BLEND_ALPHA_ADDITIVE, CLAMP_MODE_WRAP, ZBUF_READ, STENCIL_NONE, 0xFF, CULL_MODE_NONE, TNL_CLIP_MODE_NONE, TNL_LIGHT_MODE_NONE); // 0x0150A708
gr_state GM_ADDITIVE_Z_NO_CLIP(ALPHA_BLEND_ADDITIVE, CLAMP_MODE_WRAP, ZBUF_FULL, STENCIL_NONE, 0xFF, CULL_MODE_NONE, TNL_CLIP_MODE_NONE, TNL_LIGHT_MODE_NONE); // 0x0150A710

// The safe area borders in pixels (gr_set_safe_area) and its percentages.
static int Gr_sa_border_left; // 0x01B7BC20
static int Gr_sa_border_right; // 0x01B7BC24
static int Gr_sa_border_top; // 0x01B7BC28
static int Gr_sa_border_bottom; // 0x01B7BC2C
static float Gr_sa_pct_x; // 0x01B7BC30
static float Gr_sa_pct_y; // 0x01B7BC34

// Magic number: 1/255, the byte to float color scale (multiplied, not divided).
static const float Gr_color_byte_scale = 0.003921569f;

static const rl_screen_info& gr_screen_info()
{
    return *rl_render_lib::get_instance()->get_screen_info();
}

static rl_primitive_state gr_rl_state(const gr_state& state)
{
    rl_primitive_state rl_state;
    gr_convert_to_rl_state(rl_state, state);
    return rl_state;
}

// 0x004DCAC0 (gr.h:23)
int gr_screen_width()
{
    return gr_screen_info().get_screen_width();
}

// 0x004DCAD0 (gr.h:28)
int gr_screen_height()
{
    return gr_screen_info().get_screen_height();
}

// 0x00509060 (:194)
// LTCG_REGARGS: border_left in EAX and border_right in ECX, border_top and border_bottom on the stack. Unreferenced.
[[maybe_unused]] static void gr_set_safe_area_ex(int border_left, int border_top, int border_right, int border_bottom)
{
    Gr_sa_border_left = border_left;
    Gr_sa_border_right = border_right;
    Gr_sa_border_top = border_top;
    Gr_sa_border_bottom = border_bottom;
}

// 0x00509080 (:211)
// The borders are half of the screen outside the given percentages, the same on both sides.
void gr_set_safe_area(float percentage_x, float percentage_y)
{
    Gr_sa_pct_x = percentage_x;
    Gr_sa_pct_y = percentage_y;
    Gr_sa_border_left = (int)((float)gr_screen_info().get_screen_width() * (1.0 - percentage_x) * 0.5);
    Gr_sa_border_top = (int)((float)gr_screen_info().get_screen_height() * (1.0 - percentage_y) * 0.5);
    Gr_sa_border_right = Gr_sa_border_left;
    Gr_sa_border_bottom = Gr_sa_border_top;
}

// 0x004B8C10 (:226)
int gr_safe_area_left()
{
    return Gr_sa_border_left;
}

// 0x004DD8A0 (:231)
int gr_safe_area_right()
{
    return gr_screen_info().get_screen_width() - Gr_sa_border_right - 1;
}

// 0x004B8C20 (:236)
int gr_safe_area_top()
{
    return Gr_sa_border_top;
}

// 0x004DD8C0 (:241)
int gr_safe_area_bottom()
{
    return gr_screen_info().get_screen_height() - Gr_sa_border_bottom - 1;
}

// 0x004DD8E0 (:246)
int gr_safe_area_width()
{
    return gr_screen_info().get_screen_width() - Gr_sa_border_left - Gr_sa_border_right;
}

// 0x004DD900 (:251)
int gr_safe_area_height()
{
    return gr_screen_info().get_screen_height() - Gr_sa_border_top - Gr_sa_border_bottom;
}

// 0x004B8C30 (:256)
void gr_get_safe_area_pct(float& percentage_x, float& percentage_y)
{
    percentage_x = Gr_sa_pct_x;
    percentage_y = Gr_sa_pct_y;
}

// 0x00509120 (:263)
// The region only; scissoring itself is enabled by the state manager's flag.
void gr_set_scissor_region(int left, int top, int right, int bottom, int)
{
    rl_draw::get_instance()->get_rl_state().set_scissor_region(rl_rect(left, top, right, bottom));
}

// 0x004DD920 (:274)
void gr_get_scissor_region(int& left, int& top, int& right, int& bottom)
{
    const rl_rect& region = rl_draw::get_instance()->get_rl_state().get_scissor_region();
    left = region.m_left;
    top = region.m_top;
    right = region.m_right;
    bottom = region.m_bottom;
}

// 0x00520A90 (:284)
void gr_set_state(const gr_state& state)
{
    rl_draw::get_instance()->set_state(gr_rl_state(state));
}

// 0x004DD970 (:291)
void gr_get_color(int& r, int& g, int& b, int& a)
{
    const rl_color_float& clr = rl_draw::get_instance()->get_rl_state().get_primitive_color();
    r = (int)(clr.red * 255.0);
    g = (int)(clr.green * 255.0);
    b = (int)(clr.blue * 255.0);
    a = (int)(clr.alpha * 255.0);
}

// 0x00509170 (:309)
void gr_set_color(int r, int g, int b, int a)
{
    rl_draw::get_instance()->get_rl_state().set_primitive_color(rl_color_float((float)r * Gr_color_byte_scale, (float)g * Gr_color_byte_scale, (float)b * Gr_color_byte_scale, (float)a * Gr_color_byte_scale));
}

// 0x00509210 (:321)
// The components are 0-255 here too (truncated to integers first).
void gr_set_color(float r, float g, float b, float a)
{
    gr_set_color((int)r, (int)g, (int)b, (int)a);
}

// 0x005092C0 (:326)
void gr_set_color(const color& c)
{
    gr_set_color((int)c.red, (int)c.green, (int)c.blue, (int)c.alpha);
}

// 0x00509360 (:331)
void gr_set_alpha(int a)
{
    rl_state_manager& state = rl_draw::get_instance()->get_rl_state();
    const rl_color_float& clr = state.get_primitive_color();
    state.set_primitive_color(rl_color_float(clr.red, clr.green, clr.blue, (float)a * Gr_color_byte_scale));
}

// 0x00536D60 (:342)
void gr_set_texture(int bitmap_id1)
{
    rl_draw::get_instance()->set_texture(bitmap_id1);
}

// 0x00554BB0 (:347)
// Untextured.
void gr_rect_outline(int x, int y, int w, int h, int t, const gr_state& state)
{
    rl_draw::get_instance()->get_rl_state().set_primitive_texture(-1);
    rl_draw::get_instance()->rect_outline((float)x, (float)y, (float)w, (float)h, (float)t, gr_rl_state(state));
}

// 0x00554C70 (:356)
// Untextured.
void gr_rect(int x1, int y1, int w, int h, const gr_state& state)
{
    rl_draw::get_instance()->get_rl_state().set_primitive_texture(-1);
    rl_draw::get_instance()->rect((float)x1, (float)y1, (float)w, (float)h, gr_rl_state(state));
}

// 0x00554D20 (:366)
// Untextured.
void gr_rect(float x, float y, float w, float h, const gr_state& state)
{
    rl_draw::get_instance()->get_rl_state().set_primitive_texture(-1);
    rl_draw::get_instance()->rect(x, y, w, h, gr_rl_state(state));
}

// 0x00554DD0 (:387)
void gr_line(float x1, float y1, float x2, float y2, const gr_state& state, bool)
{
    rl_draw::get_instance()->line(x1, y1, x2, y2, gr_rl_state(state));
}

// 0x00554E60 (:394)
void gr_line(int x1, int y1, int x2, int y2, const gr_state& state, bool)
{
    rl_draw::get_instance()->line((float)x1, (float)y1, (float)x2, (float)y2, gr_rl_state(state));
}

// 0x00554F00 (:401)
void gr_bitmap(int bitmap_id, int x, int y, const gr_state& state)
{
    rl_draw::get_instance()->bitmap(bitmap_id, (float)x, (float)y, gr_rl_state(state));
}

// 0x00554F80 (:414)
void gr_bitmap_scaled(int bitmap_id, int x, int y, int w, int h, const gr_state& state)
{
    const rl_primitive_state rl_state = gr_rl_state(state);
    rl_draw::get_instance()->set_texture(bitmap_id);
    rl_draw::get_instance()->rect((float)x, (float)y, (float)w, (float)h, rl_state);
}

// 0x00555060 (:422)
void gr_bitmap_scaled(int bitmap_id, float x, float y, float w, float h, const gr_state& state)
{
    const rl_primitive_state rl_state = gr_rl_state(state);
    rl_draw::get_instance()->set_texture(bitmap_id);
    rl_draw::get_instance()->rect(x, y, w, h, rl_state);
}

// 0x00555130 (:434)
void gr_bitmap_centered(int bitmap_id, int cx, int cy, const gr_state& state)
{
    rl_draw::get_instance()->bitmap_center(bitmap_id, (float)cx, (float)cy, gr_rl_state(state));
}

// 0x005551B0 (:441)
void gr_bitmap_ex(int bitmap_id, int x, int y, int w, int h, int sx, int sy, int sw, int sh, bool flip_x, bool flip_y, const gr_state& state)
{
    rl_draw::get_instance()->bitmap_ex(bitmap_id, (float)x, (float)y, (float)w, (float)h, (float)sx, (float)sy, (float)sw, (float)sh, flip_x, flip_y, gr_rl_state(state));
}

// 0x005552A0 (:450)
// Notes (decompiled behaviour, kept as is): scalew and scaleh are passed as the rectangle's width and height.
void gr_bitmap_rotated_scale(int bitmap_id, float x, float y, float angle, float scalew, float scaleh, const gr_state& state)
{
    const rl_primitive_state rl_state = gr_rl_state(state);
    rl_draw::get_instance()->set_texture(bitmap_id);
    rl_draw::get_instance()->rect_rotate(x, y, scalew, scaleh, angle, rl_state);
}

// 0x00555380 (:470)
void gr_bitmap_rotated(int bitmap_id, float x, float y, float angle, float scale, const gr_state& state)
{
    rl_draw::get_instance()->bitmap_rotate(bitmap_id, x, y, angle, scale, gr_rl_state(state));
}

// 0x00555420 (:484)
void gr_circle(int x, int y, int radius, const gr_state& state)
{
    rl_draw::get_instance()->circle((float)x, (float)y, (float)radius, gr_rl_state(state));
}

// 0x004B8C60 (:704)
// gr_state: alpha mode bits 0-3, clamp 4-7, z-buffer 8-11, stencil 12-15, constant alpha 20-27; tnl_value: cull 0-3,
// clip 4-7, light 8-11. The rl_primitive_state packs the alpha mode at 0, the clamp mode's low bit at 5, the z-buffer
// mode at 6, the stencil mode at 9, the clip mode at 13 and the constant alpha at 23.
void gr_convert_to_rl_state(rl_primitive_state& rl_state, const gr_state& state)
{
    const unsigned int value = state.value;
    const unsigned int tnl_value = state.tnl_value;
    unsigned int bits = value & 0xF;
    bits ^= (value * 2) & 0x20;
    bits ^= (tnl_value & 3) << 13;
    bits ^= (value >> 3) & 0x1E00;
    bits ^= (value >> 2) & 0x1C0;
    bits ^= (value * 8) & 0x7F800000;
    rl_state.set_bits(bits);
}

// 0x0083F520 (gr_defs.h:132)
void gr_state::set_stencil_mode(gr_stencil_mode stm)
{
    value = (stm << 0xC) | (value & 0xFFFF0FFF);
}

// 0x004B8CD0 (:716)
gr_state::gr_state()
{
    value = 0xFFFFFFFF;
    tnl_value = 0xFFFFFFFF;
}

// 0x004B8CE0 (:722)
gr_state::gr_state(gr_alpha_mode am, gr_clamp_mode cm, gr_zbuf_mode zm, gr_stencil_mode stm, unsigned int const_alpha, gr_cull_mode tclm, gr_tnl_clip_mode tcm, gr_tnl_light_mode tlm)
{
    tnl_value = ((tlm << 4 | tcm) << 4) | tclm;
    value = ((((const_alpha << 8 | stm) << 4 | zm) << 4 | cm) << 4) | am;
}

// 0x004B8D20 (:728)
gr_state::gr_state(const gr_state& _v)
{
    value = _v.value;
    tnl_value = _v.tnl_value;
}

// 0x004B8D40 (:734)
void gr_state::invalidate()
{
    value = 0xFFFFFFFF;
    tnl_value = 0xFFFFFFFF;
}

// 0x004B8D50 (:740)
bool gr_state::valid()
{
    return value != 0xFFFFFFFF && tnl_value != 0xFFFFFFFF;
}

// 0x004B8D70 (:745)
bool gr_state::operator==(const gr_state& state)
{
    return state.value == value && state.tnl_value == tnl_value;
}

// 0x004B8D90 (:750)
void gr_state::operator=(const gr_state& state)
{
    value = state.value;
    tnl_value = state.tnl_value;
}
