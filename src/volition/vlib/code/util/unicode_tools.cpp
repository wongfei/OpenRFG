// Original: volition/vlib/code/util/unicode_tools.cpp
//
// Partial (orig_main.md OM5): localization_get_next_presentation_glyph_utf16, which the font code uses to walk wide
// strings (UI2: and copy_to_wide from ASCII, for vint); the wide printf helpers and unicode_insert_no_break_spaces
// (stub_impl.md S3). The other wide string helpers are not ported.

#include "volition/vlib/code/util/unicode_tools.h"

#include <stdarg.h>
#include <stdio.h>

// One Arabic letter (or the lam-alef pair: character followed by character2) and its presentation forms.
// size 0x18
struct glyph_remap
{
    unsigned int character; // 0x00
    unsigned int character2; // 0x04
    unsigned int isoGlyph; // 0x08
    unsigned int endGlyph; // 0x0C
    unsigned int midGlyph; // 0x10
    unsigned int iniGlyph; // 0x14
};

// Read from .data: the Arabic letters U+0621-U+064A and lam-alef with their isolated, final, medial and initial forms.
static const glyph_remap Presentation_glyph_remaps[37] = // 0x01519728
{
    {0x0644, 0x0627, 0xFEFB, 0xFEFC, 0x0000, 0x0000},
    {0x0621, 0x0000, 0xFE80, 0xFE80, 0x0000, 0x0000},
    {0x0622, 0x0000, 0xFE81, 0xFE82, 0x0000, 0x0000},
    {0x0623, 0x0000, 0xFE83, 0xFE84, 0x0000, 0x0000},
    {0x0624, 0x0000, 0xFE85, 0xFE86, 0x0000, 0x0000},
    {0x0625, 0x0000, 0xFE87, 0xFE88, 0x0000, 0x0000},
    {0x0626, 0x0000, 0xFE89, 0xFE8A, 0xFE8C, 0xFE8B},
    {0x0627, 0x0000, 0xFE8D, 0xFE8E, 0x0000, 0x0000},
    {0x0628, 0x0000, 0xFE8F, 0xFE90, 0xFE92, 0xFE91},
    {0x0629, 0x0000, 0xFE93, 0xFE94, 0x0000, 0x0000},
    {0x062A, 0x0000, 0xFE95, 0xFE96, 0xFE98, 0xFE97},
    {0x062B, 0x0000, 0xFE99, 0xFE9A, 0xFE9C, 0xFE9B},
    {0x062C, 0x0000, 0xFE9D, 0xFE9E, 0xFEA0, 0xFE9F},
    {0x062D, 0x0000, 0xFEA1, 0xFEA2, 0xFEA4, 0xFEA3},
    {0x062E, 0x0000, 0xFEA5, 0xFEA6, 0xFEA8, 0xFEA7},
    {0x062F, 0x0000, 0xFEA9, 0xFEAA, 0x0000, 0x0000},
    {0x0630, 0x0000, 0xFEAB, 0xFEAC, 0x0000, 0x0000},
    {0x0631, 0x0000, 0xFEAD, 0xFEAE, 0x0000, 0x0000},
    {0x0632, 0x0000, 0xFEAF, 0xFEB0, 0x0000, 0x0000},
    {0x0633, 0x0000, 0xFEB1, 0xFEB2, 0xFEB4, 0xFEB3},
    {0x0634, 0x0000, 0xFEB5, 0xFEB6, 0xFEB8, 0xFEB7},
    {0x0635, 0x0000, 0xFEB9, 0xFEBA, 0xFEBC, 0xFEBB},
    {0x0636, 0x0000, 0xFEBD, 0xFEBE, 0xFEC0, 0xFEBF},
    {0x0637, 0x0000, 0xFEC1, 0xFEC2, 0xFEC4, 0xFEC3},
    {0x0638, 0x0000, 0xFEC5, 0xFEC6, 0xFEC8, 0xFEC7},
    {0x0639, 0x0000, 0xFEC9, 0xFECA, 0xFECC, 0xFECB},
    {0x063A, 0x0000, 0xFECD, 0xFECE, 0xFED0, 0xFECF},
    {0x0641, 0x0000, 0xFED1, 0xFED2, 0xFED4, 0xFED3},
    {0x0642, 0x0000, 0xFED5, 0xFED6, 0xFED8, 0xFED7},
    {0x0643, 0x0000, 0xFED9, 0xFEDA, 0xFEDC, 0xFEDB},
    {0x0644, 0x0000, 0xFEDD, 0xFEDE, 0xFEE0, 0xFEDF},
    {0x0645, 0x0000, 0xFEE1, 0xFEE2, 0xFEE4, 0xFEE3},
    {0x0646, 0x0000, 0xFEE5, 0xFEE6, 0xFEE8, 0xFEE7},
    {0x0647, 0x0000, 0xFEE9, 0xFEEA, 0xFEEC, 0xFEEB},
    {0x0648, 0x0000, 0xFEED, 0xFEEE, 0x0000, 0x0000},
    {0x0649, 0x0000, 0xFEEF, 0xFEF0, 0xFEF0, 0xFEEF},
    {0x064A, 0x0000, 0xFEF1, 0xFEF2, 0xFEF4, 0xFEF3},
};

// The thread's "the previous letter joins the next one" flag (a field of vlib's thread-local block at +0x838 in
// rfg.exe), reset when a string starts (previous_char 0).
static thread_local bool Presentation_glyph_joined;

// One UTF-16 code point (a surrogate pair is combined).
static unsigned int utf16_code_point(const wchar_t* string, unsigned int& units)
{
    const unsigned int c = (unsigned short)string[0];
    // Magic numbers: 0xD800-0xDFFF are surrogates; the pair is 0x10000 + (high & 0x3FF) << 10 + (low & 0x3FF).
    if ((c & 0xF800) == 0xD800)
    {
        units = 2;
        return ((c & 0x3FF) + 0x40) * 0x400 + ((unsigned short)string[1] & 0x3FF);
    }
    units = 1;
    return c;
}

// 0x00593DC0 (:556)
// The next code point of string as the glyph to draw (out_char) and the number of UTF-16 units it takes. An Arabic
// letter becomes its isolated, initial, medial or final form, depending on whether the previous letter joined it and
// whether an Arabic letter follows; lam followed by alef is one glyph (both units consumed).
unsigned int localization_get_next_presentation_glyph_utf16(const wchar_t* string, unsigned int previous_char, unsigned int& out_char)
{
    unsigned int used = 0;
    const unsigned int c = utf16_code_point(string, used);
    unsigned int next = 0;
    unsigned int next_used = 0;
    unsigned int after_next = 0;
    if (c != 0)
    {
        next = utf16_code_point(string + used, next_used);
        if (next != 0)
        {
            unsigned int after_used = 0;
            after_next = utf16_code_point(string + used + next_used, after_used);
        }
    }
    out_char = c;
    if (previous_char == 0)
    {
        Presentation_glyph_joined = false;
    }
    unsigned int consumed = used;
    int remap = 0;
    for (;; remap++)
    {
        if (remap == 37)
        {
            return used;
        }
        if (c != Presentation_glyph_remaps[remap].character)
        {
            continue;
        }
        const unsigned int pair = Presentation_glyph_remaps[remap].character2;
        if (pair == 0)
        {
            break;
        }
        if (next == pair)
        {
            consumed = used + next_used;
            next = after_next;
            break;
        }
    }
    bool next_is_letter = false;
    for (int i = 0; i < 37; i++)
    {
        if (next == Presentation_glyph_remaps[i].character)
        {
            next_is_letter = true;
            break;
        }
    }
    const glyph_remap& r = Presentation_glyph_remaps[remap];
    if (!Presentation_glyph_joined)
    {
        if (!next_is_letter)
        {
            out_char = r.isoGlyph;
            Presentation_glyph_joined = false;
            return consumed;
        }
        out_char = r.iniGlyph;
        Presentation_glyph_joined = true;
        if (out_char == 0)
        {
            out_char = r.isoGlyph;
            Presentation_glyph_joined = false;
        }
        return consumed;
    }
    if (!next_is_letter)
    {
        out_char = r.endGlyph;
        Presentation_glyph_joined = false;
        return consumed;
    }
    out_char = r.midGlyph;
    Presentation_glyph_joined = true;
    if (out_char == 0)
    {
        out_char = r.endGlyph;
        Presentation_glyph_joined = false;
    }
    return consumed;
}

// 0x00593C60 (:357)
// Bytes widened (no decoding); the last element is always a terminator.
wchar_t* copy_to_wide(wchar_t* dest, const char* source, unsigned int size)
{
    for (unsigned int i = 0; i < size; ++i)
    {
        dest[i] = (wchar_t)(unsigned char)source[i];
        if (source[i] == 0)
        {
            break;
        }
    }
    // Original bug kept: a size of 0 writes dest[-1]
    dest[size - 1] = 0;
    return dest;
}

// 0x005B2700 (:159)
// The format is widened byte by byte into a 1024 character buffer first (no decoding; longer formats are cut), then
// printed wide; dest is always terminated.
int _snprintf_safe(wchar_t* dest, unsigned int dest_chr_count, const char* format, ...)
{
    wchar_t wide_format[1024];
    for (unsigned int i = 0; i < 1024; i++)
    {
        wide_format[i] = (wchar_t)(unsigned char)format[i];
        if (format[i] == '\0')
        {
            break;
        }
    }
    wide_format[1023] = L'\0';
    va_list args;
    va_start(args, format);
    const int result = _vswprintf_c_l(dest, dest_chr_count, wide_format, nullptr, args);
    va_end(args);
    dest[dest_chr_count - 1] = L'\0';
    return result;
}

// 0x00593A40 (:212)
// Prints at most 1024 characters (magic number: the destination is assumed that large).
int sprintf(wchar_t* dest, const wchar_t* format, ...)
{
    va_list args;
    va_start(args, format);
    const int result = _vswprintf_c_l(dest, 1024, format, nullptr, args);
    va_end(args);
    return result;
}

// 0x00593A10 (:190)
int _snwprintf_safe(wchar_t* dest, unsigned int dest_chr_count, const wchar_t* format, ...)
{
    va_list args;
    va_start(args, format);
    const int result = _vswprintf_c_l(dest, dest_chr_count, format, nullptr, args);
    va_end(args);
    dest[dest_chr_count - 1] = L'\0';
    return result;
}

// 0x00593A60 (:231)
int _snprintf_safe(wchar_t* dest, unsigned int dest_chr_count, const wchar_t* format, ...)
{
    va_list args;
    va_start(args, format);
    const int result = _vswprintf_c_l(dest, dest_chr_count, format, nullptr, args);
    va_end(args);
    dest[dest_chr_count - 1] = L'\0';
    return result;
}

// 0x00593C10 (:337)
// Turns every space of the first size characters into U+00A0; true when one was replaced.
bool unicode_insert_no_break_spaces(wchar_t* string, unsigned int size)
{
    if (string == nullptr || size == 0)
    {
        return false;
    }
    bool replaced = false;
    for (unsigned int i = 0; i < size && string[i] != L'\0'; i++)
    {
        if (string[i] == L' ')
        {
            string[i] = L'\xA0';
            replaced = true;
        }
    }
    return replaced;
}

// 0x00593A90 (:unicode_tools.cpp)
char* copy_to_ascii(char* dest, const wchar_t* source, unsigned int size)
{
    // Original bug kept: a size of 0 writes before the buffer.
    for (unsigned int i = 0; i < size; i++)
    {
        dest[i] = (char)source[i];
        if (source[i] == L'\0')
        {
            break;
        }
    }
    dest[size - 1] = '\0';
    return dest;
}
