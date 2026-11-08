// Original: volition/render_lib/code/rl_draw.cpp
//
// Partial: creation and initialize, and the 2D drawing: state, color and texture, strings, rectangles, lines, circles,
// bitmaps, and the 2D strip and line lists. Of the 3D drawing only string_3d and the 3D strip are ported
// (final_push.md step 5); the 3D debug primitives are dead in rfg.exe (render_debug.md).
// The 2D primitives write rl_primitive_2d_vert vertices (screen x/y, u/v, color) into rl_primitive_renderer: drawn at
// once in immediate mode (with m_pRenderer), recorded for the render thread in deferred mode.

#include "volition/render_lib/code/rl_draw.h"

#include "volition/render_lib/code/rl_color.h"
#include "volition/render_lib/code/rl_factory_rtti.h"
#include "volition/render_lib/code/rl_primitive_state.h"
#include "volition/render_lib/code/gr/gr.h"
#include "volition/render_lib/code/rl_primitive_defines.h"
#include "volition/render_lib/code/rl_primitive_renderer.h"
#include "volition/render_lib/code/rl_render_lib.h"
#include "volition/render_lib/code/rl_screen_info.h"
#include "volition/render_lib/code/rl_texture.h"
#include "volition/vlib/code/bmpman/bmpman.h"
#include "volition/vlib/code/math/matrix.h"
#include "volition/vlib/code/util/unicode_tools.h"
#include "volition/vlib/code/util/vfont.h"

#include <math.h>

// :192
// <rl_draw::release> 0x004BEB80 (:192)
// <rl_draw::create> 0x0050DBA0 (:192)
RL_DYNAMIC_CLASS_IMPLEMENTATION(rl_draw)

// 0x004E4D00 (:197)
rl_draw::rl_draw()
{
}

// 0x004E4D40 (:201)
rl_draw::~rl_draw()
{
}

// 0x005D1960 (rl_draw.h:42, inline; emitted in rfg.cpp)
// The renderer the 2D drawing records into (orig_main.md OM3: the boot's frames set it around their drawing).
void rl_draw::set_renderer(rl_renderer* pRenderer)
{
    m_pRenderer = pRenderer;
}

// 0x004B8C00 (.h:165)
// In the render half since VFX.md FX5 (gr_3d.cpp's projection reads the state in every render program).
rl_state_manager& rl_draw::get_rl_state()
{
    return m_state;
}

// 0x00523FF0 (:207)
// White tint and fade, full saturation, and a macro state writing RGBA only (bits 0x28000).
void rl_draw::initialize()
{
    m_pRenderer = nullptr;
    m_state.set_tint_color(rl_color_float(1.0f, 1.0f, 1.0f, 1.0f));
    m_state.set_fade_color(rl_color_float(1.0f, 1.0f, 1.0f, 1.0f));
    m_state.set_saturation(1.0f);
    rl_primitive_state macro_state;
    macro_state.set_color_write_mode(RLPCW_RGBA);
    m_state.set_macro_state(macro_state);
}


// The primitive color as vertex color bytes (truncated, as the original's float to int conversions).
static rl_color rl_draw_vertex_color(const rl_color_float& clr)
{
    return rl_color((unsigned char)(int)(clr.red * 255.0), (unsigned char)(int)(clr.green * 255.0), (unsigned char)(int)(clr.blue * 255.0), (unsigned char)(int)(clr.alpha * 255.0));
}

static void rl_draw_set_vert(rl_primitive_2d_vert& vert, float x, float y, float u, float v, const rl_color& clr)
{
    vert.pt.x = x;
    vert.pt.y = y;
    vert.uvs.x = u;
    vert.uvs.y = v;
    vert.color = clr;
}

static int rl_draw_screen_width()
{
    return rl_render_lib::get_instance()->get_screen_info()->get_screen_width();
}

// 0x004E4C20 (:74)
// LTCG_REGARGS: s in ECX, fontnum in EAX, scale on the stack. The x that centres the first line of s on the screen.
static float get_centered_x(const char* s, int fontnum, const float scale)
{
    int w = 0;
    int h = 0;
    font_get_string_size(w, h, s, -1, fontnum, true);
    return (float)(((double)rl_draw_screen_width() - (double)(float)((double)w * (double)scale)) * 0.5);
}

// 0x004E4C90 (:86)
// LTCG_REGARGS: s in ECX, fontnum in EAX, scale on the stack.
static float get_centered_x(const wchar_t* s, int fontnum, const float scale)
{
    int w = 0;
    int h = 0;
    font_get_string_size(w, h, s, -1, fontnum, true);
    return (float)(((double)rl_draw_screen_width() - (double)(float)((double)w * (double)scale)) * 0.5);
}

// The 2D strip type for the current textures: bink planes, a texture, a material or none.
static rl_primitive_render_type rl_draw_tristrip_type(const rl_state_manager& state)
{
    int y = -1;
    int cr = -1;
    int cb = -1;
    state.get_primitive_bink_textures(y, cr, cb);
    if (y != -1 && cr != -1 && cb != -1)
    {
        return RLPRT_BINKED_TRISTRIP_2D;
    }
    if (state.get_primitive_texture() != -1)
    {
        return RLPRT_TEXTURED_TRISTRIP_2D;
    }
    if (state.get_primitive_material() != 0xFFFFFFFF)
    {
        return RLPRT_MATERIAL_TRISTRIP_2D;
    }
    return RLPRT_TRISTRIP_2D;
}

static rl_primitive_2d_vert* rl_draw_allocate(int num_verts, rl_state_manager& state)
{
    return (rl_primitive_2d_vert*)rl_primitive_renderer::get_instance()->allocate_verts(num_verts, state);
}

// 0x0050DC30 (:213)
// White tint and fade, full saturation, RGBA writes (macro state bits 0x28000).
void rl_draw::initState()
{
    m_state.set_tint_color(rl_color_float(1.0f, 1.0f, 1.0f, 1.0f));
    m_state.set_fade_color(rl_color_float(1.0f, 1.0f, 1.0f, 1.0f));
    m_state.set_saturation(1.0f);
    rl_primitive_state macro_state;
    macro_state.set_color_write_mode(RLPCW_RGBA);
    m_state.set_macro_state(macro_state);
}

// 0x0050DCC0 (:237)
void rl_draw::set_color(rl_color_float clr)
{
    m_state.set_primitive_color(clr);
}

// 0x004E4D60 (:242)
void rl_draw::get_color(rl_color_float& clr)
{
    clr = m_state.get_primitive_color();
}

// 0x0050DCE0 (:247)
void rl_draw::set_alpha(int a)
{
    const rl_color_float& clr = m_state.get_primitive_color();
    m_state.set_primitive_color(rl_color_float(clr.red, clr.green, clr.blue, (float)((double)a / 255.0)));
}

// 0x0053A980 (:254)
void rl_draw::set_texture(int bm_handle)
{
    m_state.set_primitive_texture(bm_handle);
}

// 0x00524080 (:264)
void rl_draw::set_material(unsigned int mat_handle)
{
    m_state.set_primitive_material(mat_handle);
}

// 0x00555BC0 (:283)
void rl_draw::string(float sx, float sy, const char* s, bool centered, int fontnum, const rl_primitive_state& state)
{
    string_scale(sx, sy, s, 1.0f, centered, fontnum, state);
}

// 0x00555C10 (:289)
void rl_draw::string(float sx, float sy, const wchar_t* s, bool centered, int fontnum, const rl_primitive_state& state)
{
    string_scale(sx, sy, s, 1.0f, centered, fontnum, state);
}

// 0x005514B0 (:305)
// One textured quad per glyph from the font's bitmap: a glyph is its masquerade-scaled width by the font's render
// height (times the masquerade scale), times scale; its texels are its u/v in the bitmap, inset by half a texel. A
// newline starts the next line at sx (centred again when centered); other control characters are skipped.
// Notes (decompiled behaviour, kept as is): a glyph the font lacks (index -1) reads the u/v tables before their start.
void rl_draw::string_scale(float sx, float sy, const char* s, float scale, bool centered, int fontnum, const rl_primitive_state& state)
{
    const font* f = font_get_font(fontnum);
    if (f == nullptr || s == nullptr || *s == '\0')
    {
        return;
    }
    float x = sx;
    const float render_h = (float)f->render_h;
    if (centered)
    {
        x = get_centered_x(s, fontnum, scale);
    }
    set_texture(f->bitmap_id);
    const rl_color clr = rl_draw_vertex_color(m_state.get_primitive_color());
    int bm_w = 0;
    int bm_h = 0;
    float bm_scale_x = 0.0f;
    float bm_scale_y = 0.0f;
    bm_get_dimensions(f->bitmap_id, bm_w, bm_h, bm_scale_x, bm_scale_y);
    const float half_u = 0.5f / (float)bm_w;
    const float half_v = 0.5f / (float)bm_h;
    const float masq_scale = font_masq_get_scale(fontnum);
    const float inv_masq_scale = 1.0f / masq_scale;
    const float line_h = masq_scale * render_h;
    m_state.set_macro_state(state);
    rl_primitive_renderer::get_instance()->begin_primitive(RLPRT_TEXTURED_TRISTRIP_2D, state);
    float y = sy;
    char c = *s;
    while (c != '\0')
    {
        s++;
        if ((unsigned char)c < 0x20)
        {
            if (c == '\n')
            {
                y = line_h * scale + y;
                x = sx;
                if (centered)
                {
                    int w = 0;
                    int h = 0;
                    font_get_string_size(w, h, s, -1, fontnum, true);
                    x = ((float)rl_draw_screen_width() - (float)w * scale) * 0.5f;
                }
            }
        }
        else
        {
            int width = 0;
            int spacing = 0;
            const int index = font_get_char_width(c, *s, &width, &spacing, fontnum);
            const int* bm_u = f->bm_u;
            const int* bm_v = f->bm_v;
            const float u0 = (float)bm_u[index] / (float)bm_w + half_u;
            const float v0 = (float)bm_v[index] / (float)bm_h + half_v;
            const float u1 = (((float)width * inv_masq_scale) / (float)bm_w + u0) - half_u;
            const float v1 = (render_h / (float)bm_h + v0) - half_v;
            rl_primitive_2d_vert* verts = rl_draw_allocate(4, m_state);
            const float x1 = (float)width * scale + x;
            const float y1 = line_h * scale + y;
            rl_draw_set_vert(verts[0], x1, y, u1, v0, clr);
            rl_draw_set_vert(verts[1], x1, y1, u1, v1, clr);
            rl_draw_set_vert(verts[2], x, y, u0, v0, clr);
            rl_draw_set_vert(verts[3], x, y1, u0, v1, clr);
            x = (float)spacing * scale + x;
        }
        c = *s;
    }
    rl_primitive_renderer::get_instance()->end_primitive(m_state, m_pRenderer);
}

// 0x00551A20 (:409)
// The wide variant: presentation glyphs (Arabic forms), and the private use characters U+FDD0-U+FDD4 drawn as
// '%', '[', '\', '{' and ' '.
void rl_draw::string_scale(float sx, float sy, const wchar_t* s, float scale, bool centered, int fontnum, const rl_primitive_state& state)
{
    const font* f = font_get_font(fontnum);
    if (f == nullptr || s == nullptr || *s == L'\0')
    {
        return;
    }
    float x = sx;
    const float render_h = (float)f->render_h;
    if (centered)
    {
        x = get_centered_x(s, fontnum, scale);
    }
    set_texture(f->bitmap_id);
    const rl_color clr = rl_draw_vertex_color(m_state.get_primitive_color());
    int bm_w = 0;
    int bm_h = 0;
    float bm_scale_x = 0.0f;
    float bm_scale_y = 0.0f;
    bm_get_dimensions(f->bitmap_id, bm_w, bm_h, bm_scale_x, bm_scale_y);
    const float half_u = 0.5f / (float)bm_w;
    const float half_v = 0.5f / (float)bm_h;
    const float masq_scale = font_masq_get_scale(fontnum);
    const float inv_masq_scale = 1.0f / masq_scale;
    const float line_h = masq_scale * render_h;
    m_state.set_macro_state(state);
    rl_primitive_renderer::get_instance()->begin_primitive(RLPRT_TEXTURED_TRISTRIP_2D, state);
    unsigned int previous_char = 0;
    float y = sy;
    while (*s != L'\0')
    {
        unsigned int c = 0;
        s += localization_get_next_presentation_glyph_utf16(s, previous_char, c);
        const unsigned int glyph_char = c;
        if (c < 0x20)
        {
            if (c == '\n')
            {
                y = line_h * scale + y;
                x = sx;
                if (centered)
                {
                    int w = 0;
                    int h = 0;
                    font_get_string_size(w, h, s, -1, fontnum, true);
                    x = ((float)rl_draw_screen_width() - (float)w * scale) * 0.5f;
                }
            }
        }
        else
        {
            // Magic numbers: the private use escapes U+FDD0-U+FDD4.
            switch (c)
            {
            case 0xFDD0:
                c = '%';
                break;
            case 0xFDD1:
                c = '[';
                break;
            case 0xFDD2:
                c = '\\';
                break;
            case 0xFDD3:
                c = '{';
                break;
            case 0xFDD4:
                c = ' ';
                break;
            default:
                break;
            }
            unsigned int next_c = 0;
            localization_get_next_presentation_glyph_utf16(s, c, next_c);
            int width = 0;
            int spacing = 0;
            const int index = font_get_char_width(c, next_c, &width, &spacing, fontnum);
            const int* bm_u = f->bm_u;
            const int* bm_v = f->bm_v;
            const float u0 = (float)bm_u[index] / (float)bm_w + half_u;
            const float v0 = (float)bm_v[index] / (float)bm_h + half_v;
            const float u1 = (((float)width * inv_masq_scale) / (float)bm_w + u0) - half_u;
            const float v1 = (render_h / (float)bm_h + v0) - half_v;
            rl_primitive_2d_vert* verts = rl_draw_allocate(4, m_state);
            const float x1 = (float)width * scale + x;
            const float y1 = line_h * scale + y;
            rl_draw_set_vert(verts[0], x1, y, u1, v0, clr);
            rl_draw_set_vert(verts[1], x1, y1, u1, v1, clr);
            rl_draw_set_vert(verts[2], x, y, u0, v0, clr);
            rl_draw_set_vert(verts[3], x, y1, u0, v1, clr);
            x = (float)spacing * scale + x;
        }
        previous_char = glyph_char;
    }
    rl_primitive_renderer::get_instance()->end_primitive(m_state, m_pRenderer);
}

// 0x00552000 (:539)
// A quad over the whole texture (or untextured, bink or material, by the current state).
void rl_draw::rect(float sx, float sy, float w, float h, const rl_primitive_state& state)
{
    const rl_color clr = rl_draw_vertex_color(m_state.get_primitive_color());
    const rl_primitive_render_type prim_type = rl_draw_tristrip_type(m_state);
    m_state.set_macro_state(state);
    rl_primitive_renderer::get_instance()->begin_primitive(prim_type, state);
    rl_primitive_2d_vert* verts = rl_draw_allocate(4, m_state);
    rl_draw_set_vert(verts[0], sx + w, sy, 1.0f, 0.0f, clr);
    rl_draw_set_vert(verts[1], sx + w, sy + h, 1.0f, 1.0f, clr);
    rl_draw_set_vert(verts[2], sx, sy, 0.0f, 0.0f, clr);
    rl_draw_set_vert(verts[3], sx, sy + h, 0.0f, 1.0f, clr);
    rl_primitive_renderer::get_instance()->end_primitive(m_state, m_pRenderer);
}

// 0x005521E0 (:608)
// A frame of the given thickness (at most half the rectangle) as one strip of 10 vertices around it; the texture
// coordinates follow the frame. Bink planes are not considered.
void rl_draw::rect_outline(float sx, float sy, float w, float h, float thickness, const rl_primitive_state& state)
{
    const rl_color clr = rl_draw_vertex_color(m_state.get_primitive_color());
    rl_primitive_render_type prim_type = RLPRT_TRISTRIP_2D;
    if (m_state.get_primitive_texture() != -1)
    {
        prim_type = RLPRT_TEXTURED_TRISTRIP_2D;
    }
    else if (m_state.get_primitive_material() != 0xFFFFFFFF)
    {
        prim_type = RLPRT_MATERIAL_TRISTRIP_2D;
    }
    m_state.set_macro_state(state);
    rl_primitive_renderer::get_instance()->begin_primitive(prim_type, state);
    rl_primitive_2d_vert* verts = rl_draw_allocate(10, m_state);
    float tu = thickness / w;
    if (tu - 0.5f >= 0.0f)
    {
        tu = 0.5f;
    }
    float tv = thickness / h;
    if (tv - 0.5f >= 0.0f)
    {
        tv = 0.5f;
    }
    float tx = w * 0.5f;
    if (thickness - w * 0.5f < 0.0f)
    {
        tx = thickness;
    }
    float ty = thickness;
    if (thickness - h * 0.5f >= 0.0f)
    {
        ty = h * 0.5f;
    }
    const float right = sx + w;
    const float bottom = sy + h;
    rl_draw_set_vert(verts[0], sx, sy, 0.0f, 0.0f, clr);
    rl_draw_set_vert(verts[1], tx + sx, ty + sy, tu, tv, clr);
    rl_draw_set_vert(verts[2], right, sy, 1.0f, 0.0f, clr);
    rl_draw_set_vert(verts[3], right - tx, ty + sy, 1.0f - tu, tv, clr);
    rl_draw_set_vert(verts[4], right, bottom, 1.0f, 1.0f, clr);
    rl_draw_set_vert(verts[5], right - tx, bottom - ty, 1.0f - tu, 1.0f - tv, clr);
    rl_draw_set_vert(verts[6], sx, bottom, 0.0f, 1.0f, clr);
    rl_draw_set_vert(verts[7], tx + sx, bottom - ty, tu, 1.0f - tv, clr);
    verts[8] = verts[0];
    verts[9] = verts[1];
    rl_primitive_renderer::get_instance()->end_primitive(m_state, m_pRenderer);
}

// 0x005526A0 (:708)
// A w by h quad centred on (csx, csy), rotated by angle (radians).
void rl_draw::rect_rotate(float csx, float csy, float w, float h, float angle, const rl_primitive_state& state)
{
    const rl_color clr = rl_draw_vertex_color(m_state.get_primitive_color());
    const float c = (float)cos((double)angle);
    const float s = (float)sin((double)angle);
    const rl_primitive_render_type prim_type = rl_draw_tristrip_type(m_state);
    m_state.set_macro_state(state);
    rl_primitive_renderer::get_instance()->begin_primitive(prim_type, state);
    rl_primitive_2d_vert* verts = rl_draw_allocate(4, m_state);
    const float wc = w * 0.5f * c;
    const float hs = h * 0.5f * s;
    const float ws = w * 0.5f * s;
    const float hc = h * 0.5f * c;
    rl_draw_set_vert(verts[0], csx + hs + wc, (ws - hc) + csy, 1.0f, 0.0f, clr);
    rl_draw_set_vert(verts[1], (wc - hs) + csx, csy + hc + ws, 1.0f, 1.0f, clr);
    rl_draw_set_vert(verts[2], (hs - wc) + csx, csy - (hc + ws), 0.0f, 0.0f, clr);
    rl_draw_set_vert(verts[3], csx - (hs + wc), (hc - ws) + csy, 0.0f, 1.0f, clr);
    rl_primitive_renderer::get_instance()->end_primitive(m_state, m_pRenderer);
}

// 0x00552990 (:781)
// Untextured.
void rl_draw::line(float sx1, float sy1, float sx2, float sy2, const rl_primitive_state& state)
{
    const rl_color clr = rl_draw_vertex_color(m_state.get_primitive_color());
    m_state.set_macro_state(state);
    rl_primitive_renderer::get_instance()->begin_primitive(RLPRT_LINELIST_2D, state);
    rl_primitive_2d_vert* verts = rl_draw_allocate(2, m_state);
    rl_draw_set_vert(verts[0], sx1, sy1, 0.0f, 0.0f, clr);
    rl_draw_set_vert(verts[1], sx2, sy2, 1.0f, 1.0f, clr);
    rl_primitive_renderer::get_instance()->end_primitive(m_state, m_pRenderer);
}

// 0x00552AD0 (:822)
// Line segments, radius * pi of them (5 to 128), from the bottom (angle 0: sin, cos) around; the last one closes at
// (sx, sy + radius).
void rl_draw::circle(float sx, float sy, float radius, const rl_primitive_state& state)
{
    const rl_color clr = rl_draw_vertex_color(m_state.get_primitive_color());
    // Magic number: pi as a double.
    int segments = (int)((double)radius * 3.141592653589793);
    if (segments < 5)
    {
        segments = 5;
    }
    else if (segments > 128)
    {
        segments = 128;
    }
    m_state.set_macro_state(state);
    // Magic numbers: 360 degrees and the float of pi / 180 (the original's degree to radian constant).
    const float step = (float)((360.0 / (double)(float)segments) * (double)0.017453292f);
    rl_primitive_renderer::get_instance()->begin_primitive(RLPRT_LINELIST_2D, state);
    float angle = 0.0f;
    for (int i = segments; i > 0; i--)
    {
        rl_primitive_2d_vert* verts = rl_draw_allocate(2, m_state);
        const float next_angle = (float)((double)angle + (double)step);
        rl_draw_set_vert(verts[0], (float)sin((double)angle) * radius + sx, (float)cos((double)angle) * radius + sy, 0.0f, 0.0f, clr);
        rl_draw_set_vert(verts[1], (float)sin((double)next_angle) * radius + sx, (float)cos((double)next_angle) * radius + sy, 0.0f, 0.0f, clr);
        angle = next_angle;
    }
    rl_primitive_2d_vert* verts = rl_draw_allocate(2, m_state);
    rl_draw_set_vert(verts[0], (float)sin((double)angle) * radius + sx, (float)cos((double)angle) * radius + sy, 0.0f, 0.0f, clr);
    rl_draw_set_vert(verts[1], sx, (float)((double)sy + (double)radius), 0.0f, 0.0f, clr);
    rl_primitive_renderer::get_instance()->end_primitive(m_state, m_pRenderer);
}

// 0x00552DA0 (:891)
void rl_draw::bitmap(int bitmap_id, float sx, float sy, const rl_primitive_state& state)
{
    int w = 0;
    int h = 0;
    float scale_x = 0.0f;
    float scale_y = 0.0f;
    int mip_levels = 0;
    rl_texture_get_dimensions(bitmap_id, w, h, scale_x, scale_y, mip_levels);
    set_texture(bitmap_id);
    rect(sx, sy, (float)w, (float)h, state);
}

// 0x00552E50 (:909)
void rl_draw::bitmap_center(int bitmap_id, float csx, float csy, const rl_primitive_state& state)
{
    int w = 0;
    int h = 0;
    float scale_x = 0.0f;
    float scale_y = 0.0f;
    bm_get_dimensions(bitmap_id, w, h, scale_x, scale_y);
    set_texture(bitmap_id);
    rect(csx - (float)w * 0.5f, csy - (float)h * 0.5f, (float)w, (float)h, state);
}

// 0x00552F40 (:933)
// The (bx, by, bw, bh) texel rectangle of the bitmap drawn at (sx, sy, sw, sh), optionally mirrored.
void rl_draw::bitmap_ex(int bitmap_id, float sx, float sy, float sw, float sh, float bx, float by, float bw, float bh, bool flip_x, bool flip_y, const rl_primitive_state& state)
{
    set_texture(bitmap_id);
    const rl_color clr = rl_draw_vertex_color(m_state.get_primitive_color());
    int w = 0;
    int h = 0;
    float scale_x = 0.0f;
    float scale_y = 0.0f;
    bm_get_dimensions(bitmap_id, w, h, scale_x, scale_y);
    float u_left = bx / (float)w;
    float u_right = (bx + bw) / (float)w;
    if (flip_x)
    {
        const float swap = u_left;
        u_left = u_right;
        u_right = swap;
    }
    float v_top = by / (float)h;
    float v_bottom = (by + bh) / (float)h;
    if (flip_y)
    {
        const float swap = v_top;
        v_top = v_bottom;
        v_bottom = swap;
    }
    rl_primitive_render_type prim_type = RLPRT_TEXTURED_TRISTRIP_2D;
    if (m_state.get_primitive_material() != 0xFFFFFFFF)
    {
        prim_type = RLPRT_MATERIAL_TRISTRIP_2D;
    }
    m_state.set_macro_state(state);
    rl_primitive_renderer::get_instance()->begin_primitive(prim_type, state);
    rl_primitive_2d_vert* verts = rl_draw_allocate(4, m_state);
    rl_draw_set_vert(verts[0], sx + sw, sy, u_right, v_top, clr);
    rl_draw_set_vert(verts[1], sx + sw, sy + sh, u_right, v_bottom, clr);
    rl_draw_set_vert(verts[2], sx, sy, u_left, v_top, clr);
    rl_draw_set_vert(verts[3], sx, sy + sh, u_left, v_bottom, clr);
    rl_primitive_renderer::get_instance()->end_primitive(m_state, m_pRenderer);
}

// 0x00553260 (:1021)
void rl_draw::bitmap_rotate(int bitmap_id, float csx, float csy, float angle, float scale, const rl_primitive_state& state)
{
    set_texture(bitmap_id);
    int w = 0;
    int h = 0;
    float scale_x = 0.0f;
    float scale_y = 0.0f;
    bm_get_dimensions(bitmap_id, w, h, scale_x, scale_y);
    rect_rotate(csx, csy, (float)w * scale, (float)h * scale, angle, state);
}

// 0x0050DDA0 (:1039)
void rl_draw::tristrip_2d_begin(const rl_primitive_state& state)
{
    const rl_primitive_render_type prim_type = rl_draw_tristrip_type(m_state);
    m_state.set_macro_state(state);
    rl_primitive_renderer::get_instance()->begin_primitive(prim_type, state);
}

// 0x00553340 (:1062)
rl_primitive_2d_vert* rl_draw::tristrip_2d_allocate_tris(int num_tris)
{
    if (num_tris < 1)
    {
        return nullptr;
    }
    return rl_draw_allocate(num_tris + 2, m_state);
}

// 0x00553370 (:1070)
void rl_draw::tristrip_2d_end()
{
    rl_primitive_renderer::get_instance()->end_primitive(m_state, m_pRenderer);
}

// 0x0050DD50 (:259)
void rl_draw::set_bink_textures(int y_handle, int cr_handle, int cb_handle)
{
    m_state.set_primitive_bink_textures(y_handle, cr_handle, cb_handle);
    if (y_handle != -1 && cr_handle != -1 && cb_handle != -1)
    {
        m_state.set_primitive_texture(-1);
        m_state.set_primitive_material(0xFFFFFFFF);
    }
}

// 0x0050DE30 (:2118)
void rl_draw::tristrip_3d_begin(const rl_primitive_state& state)
{
    rl_primitive_render_type prim_type = RLPRT_TRISTRIP_3D;
    if (m_state.get_primitive_texture() != -1)
    {
        prim_type = RLPRT_TEXTURED_TRISTRIP_3D;
    }
    else if (m_state.get_primitive_material() != 0xFFFFFFFF)
    {
        prim_type = RLPRT_MATERIAL_TRISTRIP_3D;
    }
    m_state.set_macro_state(state);
    rl_primitive_renderer::get_instance()->begin_primitive(prim_type, state);
}

// 0x00553C80 (:2137)
rl_primitive_3d_vert* rl_draw::tristrip_3d_allocate_tris(int num_tris)
{
    if (num_tris < 1)
    {
        return nullptr;
    }
    return (rl_primitive_3d_vert*)rl_primitive_renderer::get_instance()->allocate_verts(num_tris + 2, m_state);
}

// 0x00553CB0 (:2145)
void rl_draw::tristrip_3d_end()
{
    rl_primitive_renderer::get_instance()->end_primitive(m_state, m_pRenderer);
}

// 0x0050DE00 (:1079)
void rl_draw::linelist_2d_begin(const rl_primitive_state& state)
{
    const rl_primitive_render_type prim_type = m_state.get_primitive_texture() != -1 ? RLPRT_TEXTURED_LINELIST_2D : RLPRT_LINELIST_2D;
    m_state.set_macro_state(state);
    rl_primitive_renderer::get_instance()->begin_primitive(prim_type, state);
}

// 0x00553390 (:1096)
void rl_draw::linelist_2d_add_line(float sx1, float sy1, float sx2, float sy2)
{
    const rl_color clr = rl_draw_vertex_color(m_state.get_primitive_color());
    rl_primitive_2d_vert* verts = rl_draw_allocate(2, m_state);
    rl_draw_set_vert(verts[0], sx1, sy1, 0.0f, 0.0f, clr);
    rl_draw_set_vert(verts[1], sx2, sy2, 1.0f, 1.0f, clr);
}

// 0x00553490 (:1128)
void rl_draw::linelist_2d_end()
{
    rl_primitive_renderer::get_instance()->end_primitive(m_state, m_pRenderer);
}

static void rl_draw_set_vert_3d(rl_primitive_3d_vert& vert, const vector& pt, float u, float v, const rl_color& clr)
{
    vert.pt = pt;
    vert.uvs.x = u;
    vert.uvs.y = v;
    vert.color = clr;
}

// 0x005534B0 (:1147)
// Text on a plane in the world: one textured quad per glyph from the font's bitmap, centred on pt vertically (half the
// render height up and down along orient's uvec) and advancing along its rvec, all times scale. A newline moves down
// the font's line height along uvec from pt. Only gr_3d_string calls it (salvage_render_debug, dead in rfg.exe).
// Notes (decompiled behaviour, kept as is):
// - The glyph widths and spacing come from the default font (font_get_char_width's font -1), not from fontnum.
// - Unlike string_scale there is no half texel inset, no masquerade scale and no null or empty check of s.
void rl_draw::string_3d(const vector& pt, const matrix& orient, float scale, const char* s, int fontnum, const rl_primitive_state& state)
{
    int line = 0;
    const font* f = font_get_font(fontnum);
    if (f == nullptr)
    {
        return;
    }
    int bm_w = 0;
    int bm_h = 0;
    float bm_scale_x = 0.0f;
    float bm_scale_y = 0.0f;
    bm_get_dimensions(f->bitmap_id, bm_w, bm_h, bm_scale_x, bm_scale_y);
    // Magic number: 0.5 is a double constant; (double)render_h * 0.5 is exact, so the float result is too.
    const float half_h = (float)((double)(int)f->render_h * 0.5);
    const vector up(half_h * orient.uvec.x * scale, half_h * orient.uvec.y * scale, half_h * orient.uvec.z * scale);
    vector pos = pt;
    const rl_color clr = rl_draw_vertex_color(m_state.get_primitive_color());
    set_texture(f->bitmap_id);
    m_state.set_macro_state(state);
    rl_primitive_renderer::get_instance()->begin_primitive(RLPRT_TEXTURED_TRISTRIP_3D, state);
    const int* bm_u = f->bm_u;
    const int* bm_v = f->bm_v;
    while (true)
    {
        if (*s == '\n')
        {
            int line_y = f->h * line;
            do
            {
                line_y += f->h;
                line++;
                pos.x = pt.x - orient.uvec.x * (float)line_y * scale;
                pos.y = pt.y - orient.uvec.y * (float)line_y * scale;
                pos.z = pt.z - orient.uvec.z * (float)line_y * scale;
                s++;
            } while (*s == '\n');
        }
        if (*s == '\0')
        {
            break;
        }
        int width = 0;
        int spacing = 0;
        const int index = font_get_char_width(*s, s[1], &width, &spacing, -1);
        s++;
        if (index < 0)
        {
            continue;
        }
        rl_primitive_3d_vert* verts = (rl_primitive_3d_vert*)rl_primitive_renderer::get_instance()->allocate_verts(4, m_state);
        const float u0 = (float)bm_u[index] / (float)bm_w;
        const float v0 = (float)bm_v[index] / (float)bm_h;
        const float u1 = (float)width / (float)bm_w + u0;
        const float v1 = (float)(int)f->render_h / (float)bm_h + v0;
        const vector right(orient.rvec.x * (float)width * scale, orient.rvec.y * (float)width * scale, orient.rvec.z * (float)width * scale);
        const vector top(pos.x + up.x, pos.y + up.y, pos.z + up.z);
        const vector bottom(pos.x - up.x, pos.y - up.y, pos.z - up.z);
        rl_draw_set_vert_3d(verts[0], top, u0, v0, clr);
        rl_draw_set_vert_3d(verts[1], vector(top.x + right.x, top.y + right.y, top.z + right.z), u1, v0, clr);
        rl_draw_set_vert_3d(verts[2], bottom, u0, v1, clr);
        rl_draw_set_vert_3d(verts[3], vector(bottom.x + right.x, bottom.y + right.y, bottom.z + right.z), u1, v1, clr);
        pos.x = orient.rvec.x * (float)spacing * scale + pos.x;
        pos.y = orient.rvec.y * (float)spacing * scale + pos.y;
        pos.z = orient.rvec.z * (float)spacing * scale + pos.z;
    }
    rl_primitive_renderer::get_instance()->end_primitive(m_state, m_pRenderer);
}

// 0x0050DEA0 (:2206)
void rl_draw::set_state(const rl_primitive_state& state)
{
    m_state.set_macro_state(state);
}

// 0x00884BE0 (.h:166)
rl_renderer* rl_draw::get_renderer()
{
    return m_pRenderer;
}

// 0x0055DFB0 (.h:185)
void rl_draw::string(int sx, int sy, const char* s, bool centered, int fontnum, const rl_primitive_state& state)
{
    string_scale((float)sx, (float)sy, s, 1.0f, centered, fontnum, state);
}

// 0x005548B0 (.h:190)
void rl_draw::string_scale(int sx, int sy, const char* s, float scale, bool centered, int fontnum, const rl_primitive_state& state)
{
    string_scale((float)sx, (float)sy, s, scale, centered, fontnum, state);
}

// 0x0055E000 (.h:196)
void rl_draw::string(int sx, int sy, const wchar_t* s, bool centered, int fontnum, const rl_primitive_state& state)
{
    string_scale((float)sx, (float)sy, s, 1.0f, centered, fontnum, state);
}

// 0x00554900 (.h:201)
void rl_draw::string_scale(int sx, int sy, const wchar_t* s, float scale, bool centered, int fontnum, const rl_primitive_state& state)
{
    string_scale((float)sx, (float)sy, s, scale, centered, fontnum, state);
}

// 0x00554950 (.h:207)
void rl_draw::rect(int sx, int sy, int w, int h, const rl_primitive_state& state)
{
    rect((float)sx, (float)sy, (float)w, (float)h, state);
}

// 0x005549A0 (.h:212)
void rl_draw::rect_outline(int sx, int sy, int w, int h, int thickness, const rl_primitive_state& state)
{
    rect_outline((float)sx, (float)sy, (float)w, (float)h, (float)thickness, state);
}

// 0x00554A00 (.h:223)
void rl_draw::line(int sx1, int sy1, int sx2, int sy2, const rl_primitive_state& state)
{
    line((float)sx1, (float)sy1, (float)sx2, (float)sy2, state);
}

// 0x00554A50 (.h:228)
void rl_draw::circle(int sx, int sy, int radius, const rl_primitive_state& state)
{
    circle((float)sx, (float)sy, (float)radius, state);
}

// 0x00554A90 (.h:233)
void rl_draw::bitmap(int bitmap_id, int sx, int sy, const rl_primitive_state& state)
{
    bitmap(bitmap_id, (float)sx, (float)sy, state);
}

// 0x00554AD0 (.h:238)
void rl_draw::bitmap_center(int bitmap_id, int csx, int csy, const rl_primitive_state& state)
{
    bitmap_center(bitmap_id, (float)csx, (float)csy, state);
}

// 0x00554B10 (.h:243)
void rl_draw::bitmap_ex(int bitmap_id, int sx, int sy, int sw, int sh, int bx, int by, int bw, int bh, bool flip_x, bool flip_y, const rl_primitive_state& state)
{
    bitmap_ex(bitmap_id, (float)sx, (float)sy, (float)sw, (float)sh, (float)bx, (float)by, (float)bw, (float)bh, flip_x, flip_y, state);
}
