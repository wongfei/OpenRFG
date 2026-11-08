// Original: volition/vlib/code/util/vfont.h
// Implementation: volition/vlib/code/util/vfont.cpp
//
// vfont, the Volition bitmap font: creation, glyph data, string measuring and wrapping (decompiled, orig_main.md OM5):
// - font_init reads "<typeface>.vf3_pc" (a font header followed by its kerning pairs, glyphs and glyph u/v tables,
//   pointers fixed up after the read) into the given pool and loads the font's bitmap peg (".cvbm_pc", else
//   ".cpeg_pc"). Up to 9 fonts; the first one becomes the default font (fontnum -1).
// - Masquerades: font_masq_add(font, scale) returns a font number below -1 (index - 0x7FFFFFFF) that draws another
//   font scaled; the size and width functions apply the scale.
// - Glyphs are found by binary search of the code point (font_get_char_data_index); a missing glyph falls back to
//   U+25A1 (white square), else it has no width and the font's width as spacing.
#pragma once

#include "volition/vlib/code/util/unicode_tools.h"

struct font;
class mempool;

// size 0x8
struct font_masquerade
{
    float font_scale; // 0x00
    int masq_font; // 0x04
};

extern const char* Font_platform_ext[8]; // 0x01519BA0, ".vf3_pc" for every platform

// 0x00467A60 (.h:183) <wchar_t>, 0x00596A20 (.h:183) <char>
template<typename T>
unsigned int font_get_next_presentation_glyph(const T* string, const font* current_font, unsigned int previous_char, unsigned int& out_char);

// <char> 0x00596A20 (.h:183): one byte, no shaping.
template<>
inline unsigned int font_get_next_presentation_glyph<char>(const char* string, const font*, unsigned int, unsigned int& out_char)
{
    out_char = (unsigned int)(int)*string;
    return 1;
}

// <wchar_t> 0x00467A60 (.h:183): UTF-16 with Arabic presentation forms.
template<>
inline unsigned int font_get_next_presentation_glyph<wchar_t>(const wchar_t* string, const font*, unsigned int previous_char, unsigned int& out_char)
{
    return localization_get_next_presentation_glyph_utf16(string, previous_char, out_char);
}

int font_init(const char* typeface, mempool* pool, int baseline_offset, float scale); // 0x005D0560 (:228)
void font_unload_all(); // 0x005C9690 (:255)
font* font_get_font(int fontnum); // 0x00594140 (:272)
void font_get_string_size(int& w, int& h, const char* text, int len, int fontnum, bool first_line_only); // 0x005C9770 (:293)
void font_get_string_size(int& w, int& h, const wchar_t* text, int len, int fontnum, bool first_line_only); // 0x005C9780 (:299)
int font_get_height(int fontnum); // 0x00594190 (:306)
int font_get_render_height(int fontnum); // 0x005941C0 (:321)
float font_get_render_scale(int fontnum); // 0x005941F0 (:336)
int font_get_char_width(unsigned int c, unsigned int next_c, int* width, int* spacing, int fontnum); // 0x005C5B30 (:351)
int font_get_char_width(char c, char next_c, int* width, int* spacing, int fontnum); // 0x005C5B40 (:356)
unsigned int font_get_char_width_preprocessed(unsigned int c, unsigned int next_c, int* width, int* spacing, int fontnum); // 0x005C5B50 (:361)
int font_split_string(int* n_chars, int* start_indices, const char* src, int max_pixel_w, int max_lines, char ignore_char, int font, float scale); // 0x005C9790 (:386)
const char* font_split_string_once(int& length, const char* src, int max_pixel_w, int font, float scale); // 0x005C97D0 (:405)
void font_truncate_string(char* output, int max_size, const char* text, int max_width, int fontnum); // 0x005C9800 (:417)
float font_masq_get_scale(const int desired_font); // 0x00594200 (:423)
int font_masq_add(int target_font, float scale); // 0x00594240 (:438)
void font_masq_remove_all(); // 0x00594280 (:451)
unsigned int font_get_char_data_index(unsigned int unicode_character, int fontnum); // 0x005C5B60 (:462)
unsigned int font_get_char_data_index(unsigned int unicode_character, const font* current_font); // 0x005BFC10 (:471)
