// Original: volition/render_lib/code/gr/grd.cpp
//
// Partial: what the ported code calls (stub_misc.md group 5: camera_free_render_debug; final_push.md step 5: the
// control morale text, the air bombing boxes). Retail strips the debug drawing, so most of this file's functions are
// empty or return false in rfg.exe; the safe area ones are real.

#include "volition/render_lib/code/gr/grd.h"

#include "volition/render_lib/code/gr/gr.h"

// 0x01B76C00 (named here): the debug draw color as bytes, a b g r from the top. In rfg.exe only grd_set_alpha reads it,
// to write it back.
static unsigned int Grd_color;

// 0x004B8DF0 (:45)
// Stripped in rfg.exe: draws nothing.
void grd_string(int sx, int sy, const char* s, const gr_state& state)
{
}

// 0x004B8E10 (:55)
// Stripped in rfg.exe: draws nothing.
void grd_string(int sx, int sy, const wchar_t* s, const gr_state& state)
{
}

// 0x004B8EA0 (:95)
void grd_set_color(int r, int g, int b, int a)
{
    Grd_color = ((unsigned int)(a & 0xFF) << 24) | ((unsigned int)(b & 0xFF) << 16) | ((unsigned int)(g & 0xFF) << 8) | (unsigned int)(r & 0xFF);
}

// 0x004B8F10 (:111)
// Stripped in rfg.exe: draws nothing.
bool grd_sphere(const vector& center, float radius, const gr_state& state)
{
    return false;
}

// 0x004B8F50 (:130)
// Stripped in rfg.exe: draws nothing.
bool grd_bbox_aligned(const vector& mn, const vector& mx, const gr_state& state)
{
    return false;
}

// 0x004DD9E0 (:281)
// rfg.exe has gr_safe_area_right inlined: the screen width less the right border, less one.
int grd_safe_area_right()
{
    return gr_safe_area_right();
}

// 0x004B9080 (:286)
// rfg.exe has gr_safe_area_top inlined: the top border.
int grd_safe_area_top()
{
    return gr_safe_area_top();
}
