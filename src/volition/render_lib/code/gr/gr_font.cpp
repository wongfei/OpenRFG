// Original: volition/render_lib/code/gr/gr_font.cpp
//
// orig_main.md OM5 and final_push.md step 5: whole. The text functions forward to vfont and rl_draw;
// gr_get_font_height inlines vfont's font_get_height (here it calls it).

#include "volition/render_lib/code/gr/gr_font.h"

#include "volition/render_lib/code/gr/gr.h"
#include "volition/render_lib/code/rl_draw.h"
#include "volition/render_lib/code/rl_primitive_state.h"
#include "volition/vlib/code/math/crc.h"
#include "volition/vlib/code/util/vfont.h"

// One named font of the lookup table (placement inferred).
// size 0x8
struct vint_font_lookup
{
    vint_font_lookup(); // 0x004B9DF0

    checksum_stri font_name_crc; // 0x00
    int font_handle; // 0x04
};

// Magic number: 16 lookups.
static vint_font_lookup Font_lookups[16]; // 0x01C32050
static unsigned int Num_font_lookups; // 0x01B7BC38

// Magic number: 0x8000, the x coordinate that centres a string.
static const int Gr_string_centered_x = 0x8000;

// 0x004B9DF0
vint_font_lookup::vint_font_lookup()
    : font_name_crc(checksum_stri::INVALID)
{
}

static rl_primitive_state gr_font_rl_state(const gr_state& state)
{
    rl_primitive_state rl_state;
    gr_convert_to_rl_state(rl_state, state);
    return rl_state;
}

// 0x004B9E00 (:60)
int gr_init_font(const char* typeface, mempool* pool, int baseline_offset)
{
    const int font = font_init(typeface, pool, baseline_offset, 1.0f);
    if (font < 0)
    {
        return -1;
    }
    return font;
}

// 0x004B9E30 (:90)
void gr_get_string_size(int& w, int& h, const char* text, int len, int fontnum)
{
    font_get_string_size(w, h, text, len, fontnum, false);
}

// 0x004B9E60 (:96)
void gr_get_string_size(int& w, int& h, const wchar_t* text, int len, int fontnum)
{
    font_get_string_size(w, h, text, len, fontnum, false);
}

// 0x004B9E90 (:105)
int gr_get_font_height(int fontnum)
{
    return font_get_height(fontnum);
}

// 0x004B9EA0 (:129)
int gr_split_str(int* n_chars, int* start_indices, const char* src, int max_pixel_w, int max_lines, char ignore_char, int font, float scale)
{
    return font_split_string(n_chars, start_indices, src, max_pixel_w, max_lines, ignore_char, font, scale);
}

// 0x004B9EE0 (:147)
const char* gr_split_str_once(int& length, const char* src, int max_pixel_w, int font, float scale)
{
    return font_split_string_once(length, src, max_pixel_w, font, scale);
}

// 0x004B9F10 (:159)
// The default font.
void gr_truncate_string(char* output, int max_size, const char* text, int max_width)
{
    font_truncate_string(output, max_size, text, max_width, -1);
}

// 0x0055ED50 (:177)
void gr_string(int sx, int sy, const char* s, int fontnum, const gr_state& state)
{
    rl_draw::get_instance()->string_scale((float)sx, (float)sy, s, 1.0f, sx == Gr_string_centered_x, fontnum, gr_font_rl_state(state));
}

// 0x0055EE00 (:186)
void gr_string(int sx, int sy, const wchar_t* s, int fontnum, const gr_state& state)
{
    rl_draw::get_instance()->string_scale((float)sx, (float)sy, s, 1.0f, sx == Gr_string_centered_x, fontnum, gr_font_rl_state(state));
}

// 0x005554B0 (:206)
void gr_string_scale(int sx, int sy, const char* s, float scale, int fontnum, const gr_state& state)
{
    rl_draw::get_instance()->string_scale((float)sx, (float)sy, s, scale, sx == Gr_string_centered_x, fontnum, gr_font_rl_state(state));
}

// 0x00555550 (:215)
void gr_string_scale(int sx, int sy, const wchar_t* s, float scale, int fontnum, const gr_state& state)
{
    rl_draw::get_instance()->string_scale((float)sx, (float)sy, s, scale, sx == Gr_string_centered_x, fontnum, gr_font_rl_state(state));
}

// 0x005555F0 (:236)
// Only salvage_render_debug calls it (dead in rfg.exe: retail never draws the salvage counts).
void gr_3d_string(const vector& pos, const matrix& orient, float scale, const char* s, int fontnum, const gr_state& state)
{
    rl_draw::get_instance()->string_3d(pos, orient, scale, s, fontnum, gr_font_rl_state(state));
}

// 0x004B9F30 (:242)
void gr_clear_font_lookups()
{
    for (unsigned int i = 0; i < Num_font_lookups; i++)
    {
        Font_lookups[i].font_name_crc = checksum_stri("", 0);
        Font_lookups[i].font_handle = -1;
    }
    Num_font_lookups = 0;
}

// 0x004B9F90 (:253)
// Notes (decompiled behaviour, kept as is): only 15 of the 16 lookups can be used (count + 1 < 16).
unsigned int gr_set_font_lookup(const char* name, int font_num)
{
    if (name == nullptr || Num_font_lookups + 1 >= 16)
    {
        return 0xFFFFFFFF;
    }
    const unsigned int handle = Num_font_lookups;
    Font_lookups[handle].font_name_crc = checksum_stri(name, 0);
    Font_lookups[handle].font_handle = font_num;
    Num_font_lookups++;
    return handle;
}

// 0x004B9FE0 (:263)
int gr_get_font_lookup_main_font(unsigned int handle)
{
    if (handle >= Num_font_lookups)
    {
        return 0;
    }
    return Font_lookups[handle].font_handle;
}

// 0x004BA000 (:269)
unsigned int gr_get_font_lookup(const char* name)
{
    if (name != nullptr)
    {
        const checksum_stri crc(name, 0);
        for (unsigned int i = 0; i < Num_font_lookups; i++)
        {
            if (crc == Font_lookups[i].font_name_crc)
            {
                return i;
            }
        }
    }
    return 0xFFFFFFFF;
}
