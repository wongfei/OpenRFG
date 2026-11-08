// Original: volition/render_lib/code/gr/gr_font.h
// Implementation: volition/render_lib/code/gr/gr_font.cpp
//
// gr's text API on top of vfont (fonts, sizes, splitting) and rl_draw (drawing), plus the font lookups: up to 16
// names mapped to font handles (ui_initialize_fonts registers "font_body", "font_header", ...), found by the
// case-insensitive CRC of the name. A string drawn at x == 0x8000 is centred (rl_draw's centred path).
// orig_main.md OM5: ported whole (gr_3d_string since final_push.md step 5).
#pragma once

class gr_state;
class matrix;
class mempool;
class vector;

int gr_init_font(const char* typeface, mempool* pool, int baseline_offset); // 0x004B9E00 (:60)
void gr_get_string_size(int& w, int& h, const char* text, int len, int fontnum); // 0x004B9E30 (:90)
void gr_get_string_size(int& w, int& h, const wchar_t* text, int len, int fontnum); // 0x004B9E60 (:96)
int gr_get_font_height(int fontnum); // 0x004B9E90 (:105)
int gr_split_str(int* n_chars, int* start_indices, const char* src, int max_pixel_w, int max_lines, char ignore_char, int font, float scale); // 0x004B9EA0 (:129)
const char* gr_split_str_once(int& length, const char* src, int max_pixel_w, int font, float scale); // 0x004B9EE0 (:147)
void gr_truncate_string(char* output, int max_size, const char* text, int max_width); // 0x004B9F10 (:159)
void gr_string(int sx, int sy, const char* s, int fontnum, const gr_state& state); // 0x0055ED50 (:177)
void gr_string(int sx, int sy, const wchar_t* s, int fontnum, const gr_state& state); // 0x0055EE00 (:186)
void gr_string_scale(int sx, int sy, const char* s, float scale, int fontnum, const gr_state& state); // 0x005554B0 (:206)
void gr_string_scale(int sx, int sy, const wchar_t* s, float scale, int fontnum, const gr_state& state); // 0x00555550 (:215)
void gr_3d_string(const vector& pos, const matrix& orient, float scale, const char* s, int fontnum, const gr_state& state); // 0x005555F0 (:236)
void gr_clear_font_lookups(); // 0x004B9F30 (:242)
unsigned int gr_set_font_lookup(const char* name, int font_num); // 0x004B9F90 (:253)
int gr_get_font_lookup_main_font(unsigned int handle); // 0x004B9FE0 (:263)
unsigned int gr_get_font_lookup(const char* name); // 0x004BA000 (:269)
