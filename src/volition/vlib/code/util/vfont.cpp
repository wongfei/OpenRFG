// Original: volition/vlib/code/util/vfont.cpp
//
// orig_main.md OM5: whole. The exported functions jump into templates over the character type
// (font_get_char_width_template, font_get_string_size_template, font_split_str_template, ...), kept here as templates.
// Notes (decompiled behaviour, kept as is):
// - The kerning search of font_get_char_width compares the pairs' first entry with the code point and the second with
//   the next code point, but takes the pair when its second entry equals the next glyph's data index; the
//   preprocessed variant compares data indices throughout. The search reads the pair after the last one when no pair
//   stops it.
// - A char string's characters are sign extended before font_get_string_size looks them up (the char overload of
//   font_get_char_width zero extends).
// - font_split_str_template copies the line into a 512-byte buffer without a bound, and at a newline writes the next
//   line's length from its not yet set start index.

#include "volition/vlib/code/util/vfont.h"

#include "volition/render_lib/code/gr/gr.h"
#include "volition/vlib/code/bmpman/bmpman.h"
#include "volition/vlib/code/cfile/cfile.h"
#include "volition/vlib/code/memory/vmemory.h"

#include <string.h>

const char* Font_platform_ext[8] = {".vf3_pc", ".vf3_pc", ".vf3_pc", ".vf3_pc", ".vf3_pc", ".vf3_pc", ".vf3_pc", ".vf3_pc"}; // 0x01519BA0

// Magic numbers: 9 fonts and 16 masquerades.
static font* Fonts[9]; // 0x01CE8468
static int Num_fonts; // 0x01CE98F4
static font* Current_font; // 0x01CE98F8
static font* Default_font; // 0x01CE98FC
static font_masquerade Font_masquerades[16]; // 0x01CE2690
static int Num_font_masqs; // 0x01CE9900

// Magic number: U+25A1 (white square), the glyph of a character the font does not have.
static const unsigned int Font_missing_glyph = 0x25A1;
// Magic number: a masquerade's font number is its index - 0x7FFFFFFF.
static const int Font_masq_base = 0x7FFFFFFF;

// The font of a font number (font_get_font inlined): masquerades resolve to their target, -1 is the default font.
static font* font_resolve(int fontnum)
{
    if (fontnum < -1)
    {
        const int masq = fontnum + Font_masq_base;
        if (masq < 0 || Num_font_masqs <= masq)
        {
            fontnum = -1;
        }
        else
        {
            fontnum = Font_masquerades[masq].masq_font;
            if (fontnum < -1)
            {
                return nullptr;
            }
        }
    }
    if (Num_fonts <= fontnum)
    {
        return nullptr;
    }
    if (fontnum > -1)
    {
        return Fonts[fontnum];
    }
    return Default_font;
}

// The masquerade scale of a font number (font_masq_get_scale inlined).
static float font_scale(int fontnum)
{
    return font_masq_get_scale(fontnum);
}

// 0x005D0340 (:80)
// The header is followed by the kerning pairs (12 bytes each), the glyphs (20 bytes each), then the glyph u and v
// tables (4 bytes per glyph each).
static font* fonttool_read(const char* typeface, mempool* pool)
{
    char filename[64];
    cf_add_ext(filename, sizeof(filename), typeface, Font_platform_ext[0], false);
    cfile* p_cfile = cf_open(filename, "rb", PLATFORM_PC, false);
    if (p_cfile == nullptr)
    {
        return nullptr;
    }
    const unsigned int size = cf_size(p_cfile);
    font* dest_buf = (font*)pool->alloc(size, 16);
    cf_read(dest_buf, size, p_cfile, false);
    cf_close(p_cfile);
    unsigned char* const base = (unsigned char*)dest_buf;
    unsigned int offset = sizeof(font);
    if (dest_buf->render_h == 0)
    {
        dest_buf->render_h = dest_buf->h;
    }
    if (dest_buf->num_kern_pairs == 0)
    {
        dest_buf->kern_data = (font_kernpair*)nullptr;
    }
    else
    {
        dest_buf->kern_data = (font_kernpair*)(base + offset);
        offset += dest_buf->num_kern_pairs * sizeof(font_kernpair);
    }
    if (dest_buf->num_chars == 0)
    {
        dest_buf->char_data = (font_char*)nullptr;
        dest_buf->bm_u = (int*)nullptr;
        dest_buf->bm_v = (int*)nullptr;
    }
    else
    {
        dest_buf->char_data = (font_char*)(base + offset);
        offset += dest_buf->num_chars * sizeof(font_char);
        dest_buf->bm_u = (int*)(base + offset);
        dest_buf->bm_v = (int*)(base + offset + dest_buf->num_chars * sizeof(int));
    }
    char peg_filename[64];
    peg_header* peg = nullptr;
    cf_add_ext(peg_filename, sizeof(peg_filename), dest_buf->bitmap_name, Vbm_platform_ext[0], false);
    if (!bm_is_loaded(dest_buf->bitmap_name) && !bm_load_peg(peg_filename, (mempool_base*)pool, peg, true))
    {
        cf_add_ext(peg_filename, sizeof(peg_filename), dest_buf->peg_name, Peg_platform_ext[0], false);
        if (!bm_is_loaded(dest_buf->peg_name))
        {
            bm_load_peg(peg_filename, (mempool_base*)pool, peg, true);
        }
    }
    dest_buf->bitmap_id = bm_load(dest_buf->bitmap_name, false);
    return dest_buf;
}

// 0x005D0520 (:155)
// LTCG_REGARGS: typeface in ECX. Unreferenced; the same as font_init without the default font and baseline.
[[maybe_unused]] static int create_font(const char* typeface, mempool* pool, float)
{
    const int font_idx = Num_fonts++;
    font* f = fonttool_read(typeface, pool);
    if (f == nullptr)
    {
        Num_fonts--;
        return -1;
    }
    Fonts[font_idx] = f;
    return font_idx;
}

// 0x005C95F0 (:184)
// Unreferenced. The peg of the font's bitmap, else its own peg, is unloaded.
[[maybe_unused]] static void remove_font(int font_idx)
{
    font* f = Fonts[font_idx];
    if (f == nullptr)
    {
        return;
    }
    if (bm_is_loaded(f->bitmap_name))
    {
        bm_unload_peg(f->bitmap_name);
    }
    else if (bm_is_loaded(f->peg_name))
    {
        bm_unload_peg(f->peg_name);
    }
}

// 0x005D0560 (:228)
// Notes (decompiled behaviour, kept as is): scale is not used.
int font_init(const char* typeface, mempool* pool, int baseline_offset, float)
{
    const int font_idx = Num_fonts++;
    font* f = fonttool_read(typeface, pool);
    if (f == nullptr)
    {
        Num_fonts--;
        return -1;
    }
    Fonts[font_idx] = f;
    if (font_idx < 0)
    {
        return -1;
    }
    if (Default_font == nullptr)
    {
        Default_font = f;
    }
    if (baseline_offset > -1)
    {
        Fonts[font_idx]->baseline_offset = baseline_offset;
    }
    return font_idx;
}

// 0x005C9690 (:255)
// The fonts' pegs are unloaded (the font data stays in its pool).
void font_unload_all()
{
    for (int i = 0; i < Num_fonts; i++)
    {
        font* f = Fonts[i];
        if (f == nullptr)
        {
            continue;
        }
        if (bm_is_loaded(f->bitmap_name))
        {
            bm_unload_peg(f->bitmap_name);
        }
        else if (bm_is_loaded(f->peg_name))
        {
            bm_unload_peg(f->peg_name);
        }
    }
    Default_font = nullptr;
    Num_fonts = 0;
}

// 0x00594140 (:272)
font* font_get_font(int fontnum)
{
    return font_resolve(fontnum);
}

// 0x005C0E30 (:351) <unsigned int>, 0x005C0FF0 (:356) <unsigned char>
// The glyph's width and its advance (spacing plus the font's character spacing, adjusted by a kerning pair with the
// next character), scaled by the masquerade. Returns the glyph's data index, -1 when the font has neither the glyph
// nor the missing glyph, 0 without a font.
template<typename T>
static int font_get_char_width_template(T c, T next_c, int* width, int* spacing, int fontnum)
{
    font* current_font = font_resolve(fontnum);
    if (current_font == nullptr)
    {
        return 0;
    }
    const float scale = font_scale(fontnum);
    unsigned int index = font_get_char_data_index((unsigned int)c, current_font);
    if (index == 0xFFFFFFFF || current_font->num_chars <= index)
    {
        index = font_get_char_data_index(Font_missing_glyph, current_font);
        if (index == 0xFFFFFFFF || current_font->num_chars <= index)
        {
            *width = 0;
            *spacing = (int)current_font->w;
            *spacing = (int)((float)(int)(current_font->character_spacing + current_font->w) * scale);
            return -1;
        }
    }
    const font_char* chars = current_font->char_data;
    *width = chars[index].byte_width;
    *spacing = chars[index].spacing;
    const int advance = *spacing + current_font->character_spacing;
    *spacing = advance;
    unsigned int pair = (unsigned int)(int)chars[index].kerning_entry;
    if ((int)pair > -1 && next_c != 0 && next_c != '\n')
    {
        const unsigned int next_index = font_get_char_data_index((unsigned int)next_c, current_font);
        if (next_index != 0xFFFFFFFF && next_index < current_font->num_chars)
        {
            const font_kernpair* kp = (font_kernpair*)current_font->kern_data + pair;
            for (; pair < current_font->num_kern_pairs && kp->c1 == (unsigned int)c && kp->c2 < (unsigned int)next_c; kp++)
            {
                pair++;
            }
            if (kp->c2 == next_index)
            {
                *spacing = kp->offset + advance;
            }
        }
    }
    *width = (int)((float)*width * scale);
    *spacing = (int)((float)*spacing * scale);
    return (int)index;
}

// 0x005C6A00 (:293) <char>, 0x005C6B90 (:299) <wchar_t>
// The widest line and the height of the lines (render height of the first, font height for each further line),
// scaled by the masquerade.
template<typename T>
static void font_get_string_size_template(int& w, int& h, const T* text, int len, int fontnum, bool first_line_only);

template<>
void font_get_string_size_template<char>(int& w, int& h, const char* text, int len, int fontnum, bool first_line_only)
{
    font* f = font_resolve(fontnum);
    if (text == nullptr || f == nullptr)
    {
        w = 0;
        h = 0;
        return;
    }
    if (len < 0)
    {
        len = (int)strlen(text);
    }
    int height = f->render_h;
    int line_w = 0;
    int max_w = 0;
    int char_w = 0;
    int spacing = 0;
    char c = *text;
    for (;;)
    {
        if (c == '\0' || len < 1)
        {
            break;
        }
        len--;
        const unsigned int c1 = (unsigned int)(int)c;
        text++;
        if (c1 == '\n')
        {
            if (first_line_only)
            {
                break;
            }
            c = *text;
            line_w = 0;
            if (c == '\0')
            {
                break;
            }
            height += f->h;
            continue;
        }
        c = *text;
        font_get_char_width_template<unsigned int>(c1, (unsigned int)(int)c, &char_w, &spacing, fontnum);
        line_w += spacing;
        if (max_w < line_w)
        {
            max_w = line_w;
        }
    }
    const float scale = font_scale(fontnum);
    h = (int)((float)height * scale);
    w = (int)(float)max_w;
}

template<>
void font_get_string_size_template<wchar_t>(int& w, int& h, const wchar_t* text, int len, int fontnum, bool first_line_only)
{
    font* f = font_resolve(fontnum);
    if (text == nullptr || f == nullptr)
    {
        w = 0;
        h = 0;
        return;
    }
    if (len < 0)
    {
        len = (int)wcslen(text);
    }
    unsigned int previous_char = 0;
    int max_w = 0;
    int line_w = 0;
    int char_w = 0;
    int spacing = 0;
    int height = f->render_h;
    const wchar_t* p = text;
    if (*p != L'\0')
    {
        while (len > 0)
        {
            unsigned int c = 0;
            const unsigned int used = localization_get_next_presentation_glyph_utf16(p, previous_char, c);
            previous_char = c;
            len -= used;
            p += used;
            if (c == '\n')
            {
                if (first_line_only)
                {
                    break;
                }
                line_w = 0;
                if (*p == L'\0')
                {
                    break;
                }
                height += f->h;
            }
            else
            {
                unsigned int next_c = 0;
                localization_get_next_presentation_glyph_utf16(p, c, next_c);
                font_get_char_width_template<unsigned int>(previous_char, next_c, &char_w, &spacing, fontnum);
                line_w += spacing;
                if (max_w < line_w)
                {
                    max_w = line_w;
                }
            }
            if (*p == L'\0')
            {
                break;
            }
        }
    }
    const float scale = font_scale(fontnum);
    h = (int)((float)height * scale);
    w = (int)(float)max_w;
}

// 0x005C9770 (:293)
void font_get_string_size(int& w, int& h, const char* text, int len, int fontnum, bool first_line_only)
{
    font_get_string_size_template<char>(w, h, text, len, fontnum, first_line_only);
}

// 0x005C9780 (:299)
void font_get_string_size(int& w, int& h, const wchar_t* text, int len, int fontnum, bool first_line_only)
{
    font_get_string_size_template<wchar_t>(w, h, text, len, fontnum, first_line_only);
}

// 0x00594190 (:306)
// Notes (decompiled behaviour, kept as is): masquerade numbers (below -1) read Fonts out of bounds.
int font_get_height(int fontnum)
{
    Current_font = Default_font;
    if (fontnum > -1)
    {
        Current_font = Fonts[fontnum];
    }
    if (Current_font == nullptr)
    {
        // Magic number: 16 pixels without a font.
        return 16;
    }
    return Current_font->h;
}

// 0x005941C0 (:321)
int font_get_render_height(int fontnum)
{
    Current_font = Default_font;
    if (fontnum > -1)
    {
        Current_font = Fonts[fontnum];
    }
    if (Current_font == nullptr)
    {
        // Magic number: 16 pixels without a font.
        return 16;
    }
    return Current_font->render_h;
}

// 0x005941F0 (:336)
float font_get_render_scale(int)
{
    return 1.0f;
}

// 0x005C5B30 (:351)
int font_get_char_width(unsigned int c, unsigned int next_c, int* width, int* spacing, int fontnum)
{
    return font_get_char_width_template<unsigned int>(c, next_c, width, spacing, fontnum);
}

// 0x005C5B40 (:356)
int font_get_char_width(char c, char next_c, int* width, int* spacing, int fontnum)
{
    return font_get_char_width_template<unsigned char>((unsigned char)c, (unsigned char)next_c, width, spacing, fontnum);
}

// 0x005C5B50 (:361)
// c and next_c are glyph data indices (-1 is glyph 0).
unsigned int font_get_char_width_preprocessed(unsigned int c, unsigned int next_c, int* width, int* spacing, int fontnum)
{
    font* current_font = font_resolve(fontnum);
    if (current_font == nullptr)
    {
        return 0;
    }
    const float scale = font_scale(fontnum);
    if (c == 0xFFFFFFFF)
    {
        c = 0;
    }
    const font_char* chars = current_font->char_data;
    *width = chars[c].byte_width;
    *spacing = chars[c].spacing;
    const int advance = *spacing + current_font->character_spacing;
    *spacing = advance;
    unsigned int pair = (unsigned int)(int)chars[c].kerning_entry;
    if ((int)pair > -1 && chars[next_c].unicode_code_point != 0 && chars[next_c].unicode_code_point != '\n' && next_c != 0xFFFFFFFF && next_c < current_font->num_chars)
    {
        const font_kernpair* kp = (font_kernpair*)current_font->kern_data + pair;
        unsigned int c2 = 0;
        for (;;)
        {
            if (pair >= current_font->num_kern_pairs || kp->c1 != c)
            {
                c2 = kp->c2;
                break;
            }
            c2 = kp->c2;
            if (next_c <= c2)
            {
                break;
            }
            pair++;
            kp++;
        }
        if (c2 == next_c)
        {
            *spacing = kp->offset + advance;
        }
    }
    *width = (int)((float)*width * scale);
    *spacing = (int)((float)*spacing * scale);
    return c;
}

// 0x00596A40 (:?)
// LTCG: ch in AL.
static int is_white_space_template(char ch)
{
    return ch == ' ' || ch == '\t' || ch == '\n';
}

// 0x00596A60 (:?)
// LTCG: ch in AL.
static int is_gray_space_template(char ch)
{
    return ch == ' ' || ch == '\t';
}

// 0x005B4270 (:386)
// Splits src into at most max_lines lines of less than max_pixel_w pixels (at scale): n_chars and start_indices get
// each line's length and start. Lines break at a newline, before the last blank run or after a hyphen; words from
// ignore_char to the next white space are not measured. With respect_rainbow_tags, blanks inside <...> do not break.
static int font_split_str_template(int* n_chars, int* start_indices, const char* src, int max_pixel_w, int max_lines, char ignore_char, int font, float scale, void (*get_string_size_function)(int&, int&, const char*, int, int, bool), bool respect_rainbow_tags)
{
    int last_break = -1;
    int line = 0;
    int in_blank = 0;
    bool in_tag = false;
    char line_text[512];
    memset(line_text, 0, sizeof(line_text));
    int ignoring = 0;
    int i = 0;
    while (is_white_space_template(src[i]))
    {
        i++;
    }
    int new_line = 1;
    *start_indices = -1;
    int line_len = 0;
    while (i < (int)strlen(src))
    {
        if (max_lines <= line)
        {
            return line;
        }
        bool measure = true;
        if (new_line != 0)
        {
            start_indices[line] = -1;
            if (is_gray_space_template(src[i]))
            {
                measure = false;
            }
            else
            {
                start_indices[line] = i;
                last_break = -1;
                new_line = 0;
            }
        }
        if (measure)
        {
            const char c = src[i];
            if (ignoring == 0)
            {
                if (c == '\n')
                {
                    n_chars[line] = i - start_indices[line];
                    n_chars[line + 1] = i - start_indices[line + 1];
                    start_indices[line + 1] = i;
                    line++;
                    new_line = 1;
                    memset(line_text, 0, sizeof(line_text));
                    line_len = 0;
                }
                else if (c == ignore_char)
                {
                    ignoring = 1;
                }
                else
                {
                    if (i > 0 && src[i - 1] == '-')
                    {
                        last_break = i;
                    }
                    if (is_gray_space_template(c))
                    {
                        if (in_blank == 0 && (!respect_rainbow_tags || !in_tag))
                        {
                            last_break = i;
                        }
                        in_blank = 1;
                    }
                    else
                    {
                        in_blank = 0;
                        if (c == '<')
                        {
                            in_tag = true;
                        }
                        else if (c == '>')
                        {
                            in_tag = false;
                        }
                    }
                    line_text[line_len] = c;
                    line_len++;
                    line_text[line_len] = '\0';
                    int text_w = 0;
                    int text_h = 0;
                    get_string_size_function(text_w, text_h, line_text, -1, font, false);
                    text_w = (int)((float)text_w * scale);
                    text_h = (int)((float)text_h * scale);
                    if (max_pixel_w <= text_w)
                    {
                        int end = 0;
                        if (last_break < 0)
                        {
                            end = i;
                            i--;
                        }
                        else
                        {
                            end = last_break;
                            i = last_break;
                            if (last_break > 0 && src[last_break - 1] == '-')
                            {
                                i = last_break - 1;
                            }
                        }
                        n_chars[line] = end - start_indices[line];
                        if (line + 1 < max_lines)
                        {
                            start_indices[line + 1] = i;
                        }
                        line++;
                        new_line = 1;
                        memset(line_text, 0, sizeof(line_text));
                        line_len = 0;
                    }
                }
            }
            else if (is_white_space_template(c))
            {
                ignoring = 0;
            }
        }
        i++;
    }
    if (start_indices[line] > -1)
    {
        n_chars[line] = i - start_indices[line];
        line++;
    }
    return line;
}

// 0x005C6D70 (:405)
// The length of the first line of src that fits max_pixel_w and one line height (at scale); returns where the next
// line starts (blanks and one newline skipped), or null when it all fits or nothing is left.
static const char* font_split_str_once_template(int& length, const char* src, int max_pixel_w, int font, float scale, bool prefer_whitespace_splits)
{
    length = -1;
    bool in_blank = false;
    const int src_len = (int)strlen(src);
    int text_w = 0;
    int text_h = 0;
    font_get_string_size_template<char>(text_w, text_h, "X", -1, font, false);
    const float line_h = (float)text_h;
    font_get_string_size_template<char>(text_w, text_h, src, -1, font, false);
    text_w = (int)((float)text_w * scale);
    text_h = (int)((float)text_h * scale);
    if (text_w <= max_pixel_w && text_h <= (int)(line_h * scale))
    {
        length = src_len;
        return nullptr;
    }
    int i = 0;
    for (; i < src_len; i++)
    {
        font_get_string_size_template<char>(text_w, text_h, src, i, font, false);
        text_w = (int)((float)text_w * scale);
        text_h = (int)((float)text_h * scale);
        if (max_pixel_w < text_w)
        {
            break;
        }
        const char c = src[i];
        if (c == '\n')
        {
            length = i;
            return src + i + 1;
        }
        if (is_gray_space_template(c))
        {
            if (!in_blank)
            {
                length = i;
            }
            in_blank = true;
        }
        else
        {
            in_blank = false;
        }
    }
    if (length < 0 || !prefer_whitespace_splits)
    {
        length = i;
    }
    int next = length;
    while (next < src_len && is_gray_space_template(src[next]))
    {
        next++;
    }
    if (next < src_len && src[next] == '\n')
    {
        next++;
    }
    if (next == src_len)
    {
        return nullptr;
    }
    return src + next;
}

// 0x005C6F50 (:417)
// The text cut to max_width, ending in "...".
static void font_truncate_string_template(char* output, int max_size, const char* text, int max_width, int fontnum)
{
    strncpy(output, text, max_size);
    output[max_size - 1] = '\0';
    int text_w = 0;
    int text_h = 0;
    font_get_string_size_template<char>(text_w, text_h, output, -1, fontnum, false);
    if (text_w <= max_width)
    {
        return;
    }
    int len = (int)strlen(output);
    if (len < 4)
    {
        if (len > 0)
        {
            memset(output, '.', len);
        }
    }
    else
    {
        output[len - 2] = '.';
        output[len - 1] = '.';
        output[len - 3] = '.';
    }
    do
    {
        if (len > 3)
        {
            output[len - 4] = '.';
        }
        output[len - 1] = '\0';
        font_get_string_size_template<char>(text_w, text_h, output, -1, fontnum, false);
        len--;
    } while (max_width < text_w && len > 0);
}

// 0x005C9790 (:386)
int font_split_string(int* n_chars, int* start_indices, const char* src, int max_pixel_w, int max_lines, char ignore_char, int font, float scale)
{
    return font_split_str_template(n_chars, start_indices, src, max_pixel_w, max_lines, ignore_char, font, scale, font_get_string_size_template<char>, false);
}

// 0x005C97D0 (:405)
const char* font_split_string_once(int& length, const char* src, int max_pixel_w, int font, float scale)
{
    return font_split_str_once_template(length, src, max_pixel_w, font, scale, false);
}

// 0x005C9800 (:417)
void font_truncate_string(char* output, int max_size, const char* text, int max_width, int fontnum)
{
    font_truncate_string_template(output, max_size, text, max_width, fontnum);
}

// 0x00594200 (:423)
float font_masq_get_scale(const int desired_font)
{
    float scale = 1.0f;
    if (desired_font < -1)
    {
        const int masq = desired_font + Font_masq_base;
        if (masq > -1 && masq < Num_font_masqs)
        {
            scale = Font_masquerades[masq].font_scale;
        }
    }
    return scale;
}

// 0x00594240 (:438)
int font_masq_add(int target_font, float scale)
{
    // Magic number: 16 masquerades.
    if (Num_font_masqs >= 16 || target_font < -1)
    {
        return -1;
    }
    const int masq = Num_font_masqs;
    Font_masquerades[masq].masq_font = target_font;
    Font_masquerades[masq].font_scale = scale;
    Num_font_masqs++;
    return masq - Font_masq_base;
}

// 0x00594280 (:451)
void font_masq_remove_all()
{
    for (int i = 0; i < Num_font_masqs; i++)
    {
        Font_masquerades[i].masq_font = -1;
        Font_masquerades[i].font_scale = 1.0f;
    }
    Num_font_masqs = 0;
}

// 0x005C5B60 (:462)
unsigned int font_get_char_data_index(unsigned int unicode_character, int fontnum)
{
    return font_get_char_data_index(unicode_character, font_resolve(fontnum));
}

// 0x005BFC10 (:471)
// Binary search of the glyphs (sorted by code point); -1 when the font has no such glyph, 0 without a font.
unsigned int font_get_char_data_index(unsigned int unicode_character, const font* current_font)
{
    if (current_font == nullptr)
    {
        return 0;
    }
    const font_char* chars = current_font->char_data;
    unsigned int high = current_font->num_chars;
    unsigned int low = 0;
    unsigned int mid = high >> 1;
    if (high != 0)
    {
        unsigned int span = 0;
        do
        {
            const unsigned int code = chars[mid].unicode_code_point;
            if (code == unicode_character)
            {
                return mid;
            }
            if (unicode_character < code)
            {
                span = mid - low;
                high = mid;
            }
            else
            {
                low = mid + 1;
                span = high - low;
            }
            mid = (span >> 1) + low;
        } while (span != 0);
    }
    if (chars[mid].unicode_code_point != unicode_character)
    {
        mid = 0xFFFFFFFF;
    }
    return mid;
}
