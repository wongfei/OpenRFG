// Original: volition/interface_lib/code/vint_string_processing.cpp
//
// Text markup and layout (UI.md UI2). A string is parsed into a render instruction list (opcodes as uints followed by
// their data): TEXT (a run of font glyph indices in one direction), IMAGE (a bitmap sheet image, optionally untinted,
// optionally with a caption), COLOR, STOP. Tags in brackets: [image:name], [image_tint:name] (with ",x,y,hexutf8"
// caption fields), [color:#rrggbb(aa)] or [color:name] (the colour table), [scale:n], [button:name] and
// [button_tint:name] (the game's button display info, parsed as images), [format] and [/format] (push/pop of scale
// and colour). "[[" is a bracket, "\n" and "\s" escapes, U+FDD0..FDD4 stand for %, [, \, { and space. Lines break at
// spaces, hyphens or (without hyphenation) where the font allows, or with a hyphen inserted. Each run, image and
// caption is a section; after the parse each line gets the tallest section's height, the lines are aligned, and every
// section's position is moved to its line (centred vertically). The arithmetic is in double, as rfg.exe's SSE2 code.
// Compiled in interface_lib_unity.cpp after bitmap_sheets.cpp, whose image and sheet arrays the renderer reads.

#include "volition/interface_lib/code/vint_string_processing.h"

#include "volition/interface_lib/code/bitmap_sheets.h"
#include "volition/interface_lib/code/internal/vint_memory.h"
#include "volition/interface_lib/code/objects/noninteractive/vint_element_text.h"
#include "volition/interface_lib/code/resources/vint_resource_bitmap.h"
#include "volition/interface_lib/code/resources/vint_resource_font.h"
#include "volition/interface_lib/code/vint_variant.h"
#include "volition/render_lib/code/gr/gr.h"
#include "volition/vlib/code/math/matrix.h"
#include "volition/vlib/code/memory/mempool_base.h"
#include "volition/vlib/code/util/color.h"
#include "volition/vlib/code/util/string_util.h"
#include "volition/vlib/code/util/unicode_tools.h"
#include "volition/vlib/code/util/vfont.h"
#include "volition/vlib/code/vlib_core/vlib_assert.h"
#include "library/modules/runtime/base/include/keen/base/string.hpp"
#include "library/modules/runtime/base/include/keen/base/stringencoding.hpp"
#include "library/modules/runtime/base/include/keen/base/stringscanner.hpp"
#include "library/modules/runtime/base/include/keen/base/trace.hpp"

#include <string.h>

// ---- file-private types (rfg.pdb)
// size 0xC
struct tag_parse_lookup
{
    const wchar_t* m_tag; // 0x00
    unsigned int m_tag_length; // 0x04
    const wchar_t* (*m_func)(const wchar_t*, unsigned char**, unsigned int*, vint_text_parse_status&); // 0x08
};
static_assert(sizeof(tag_parse_lookup) == 0xC, "tag_parse_lookup");

static const wchar_t* vint_string_parse_format(const wchar_t* _source_string, unsigned char** _instr_buffer, unsigned int* _instr_buffer_len, vint_text_parse_status& _status);
static const wchar_t* vint_string_parse_slash_format(const wchar_t* _source_string, unsigned char** _instr_buffer, unsigned int* _instr_buffer_len, vint_text_parse_status& _status);
static const wchar_t* vint_string_parse_image(const wchar_t* _source_string, unsigned char** _instr_buffer, unsigned int* _instr_buffer_len, vint_text_parse_status& _status);
static const wchar_t* vint_string_parse_scale(const wchar_t* _source_string, unsigned char** _instr_buffer, unsigned int* _instr_buffer_len, vint_text_parse_status& _status);
static const wchar_t* vint_string_parse_color(const wchar_t* _source_string, unsigned char** _instr_buffer, unsigned int* _instr_buffer_len, vint_text_parse_status& _status);
static const wchar_t* vint_string_parse_button(const wchar_t* _source_string, unsigned char** _instr_buffer, unsigned int* _instr_buffer_len, vint_text_parse_status& _status);

// ---- file-static globals
static unsigned int Num_parse_funcs = 8; // 0x014F5AC4
static tag_parse_lookup Tag_parse_func_table[8] = { // 0x014F5AC8
    {L"image:", 6, vint_string_parse_image},
    {L"image_tint:", 11, vint_string_parse_image},
    {L"color:", 6, vint_string_parse_color},
    {L"scale:", 6, vint_string_parse_scale},
    {L"button:", 7, vint_string_parse_button},
    {L"button_tint:", 12, vint_string_parse_button},
    {L"format", 6, vint_string_parse_format},
    {L"/format", 7, vint_string_parse_slash_format},
};

// ---- vint_string_processing.h constructors without line info

// 0x00466C00
vint_render_data_image::vint_render_data_image()
{
}

// 0x00466BF0
vint_render_data_text::vint_render_data_text()
{
}

// 0x004669E0
vint_text_format::vint_text_format()
{
}

// ---- vint_text_parse_status

// 0x0046D130 (:111)
vint_text_parse_status::vint_text_parse_status()
{
    line_infos = nullptr;
    leading = 0;
    format_depth = 0;
    line_frame = nullptr;
}

// 0x0046D150 (:114)
vector2& vint_text_parse_status::scale()
{
    // the depth may pass the stack's end (16 pushes); the last entry is used then
    return format_stack[format_depth < 15 ? format_depth : 15].scale;
}

// 0x0046D170 (:119)
vint_render_data_color& vint_text_parse_status::color()
{
    return format_stack[format_depth < 15 ? format_depth : 15].color;
}

// 0x00466980 (:124)
void vint_text_parse_status::format_push()
{
    // Original bug kept: at depth 15 the copy goes one entry past the stack (into format_depth's place, 0x1B0).
    if (format_depth < 16)
    {
        format_stack[format_depth + 1] = format_stack[format_depth];
    }
    ++format_depth;
}

// 0x004669C0 (:133)
void vint_text_parse_status::format_pop()
{
    if (format_depth != 0)
    {
        --format_depth;
    }
}

// ---- helpers

// 0x004669F0 (:195)
// LTCG: _c in ECX.
static bool is_white_space(wchar_t _c)
{
    // tab, space and every control character from 1 to 31
    return _c == L'\t' || _c == L' ' || (unsigned short)(_c - 1) <= 0x1E;
}

// 0x00466A10 (:211)
// LTCG: _s in ESI.
static void skip_white_space(const wchar_t*& _s)
{
    while (is_white_space(*_s))
    {
        ++_s;
    }
}

// 0x00466A30 (:220)
// LTCG: c in EAX, force_case in ECX.
static unsigned int fix_case(unsigned int c, vint_text_force_case force_case)
{
    // ASCII only
    if (force_case == VINT_TEXT_CASE_UPPER)
    {
        if (c - 'a' < 26)
        {
            c -= 0x20;
        }
    }
    else if (force_case == VINT_TEXT_CASE_LOWER && c - 'A' < 26)
    {
        return c + 0x20;
    }
    return c;
}

// <vint_render_data_text> 0x00467AB0, <vint_render_data_image> 0x00467AE0, <vint_render_data_color> 0x00467B10 (:250)
// LTCG: _instr_list_len in ECX, _instr_list in EDX.
template<typename T>
static T* vint_instr_list_append(unsigned char*& _instr_list, unsigned int& _instr_list_len)
{
    if (_instr_list_len < sizeof(T))
    {
        keen::debug::nativePrintArgs("Instruction Buffer Full");
        return nullptr;
    }
    T* data = (T*)_instr_list;
    _instr_list += sizeof(T);
    _instr_list_len -= sizeof(T);
    return data;
}

// <unsigned int> 0x00467A80 (:272)
// LTCG: _instr_list_len in EAX, _instr_list in ECX, _data on the stack.
static bool vint_instr_list_append(const unsigned int& _data, unsigned char*& _instr_list, unsigned int& _instr_list_len)
{
    if (_instr_list_len < 4)
    {
        keen::debug::nativePrintArgs("Instruction Buffer Full");
        return false;
    }
    *(unsigned int*)_instr_list = _data;
    _instr_list += 4;
    _instr_list_len -= 4;
    return true;
}

// 0x00466A50 (:285)
// LTCG: presentation_glyph in EAX, render_font in ESI.
static unsigned int vint_presentation_glyph_to_font_index(unsigned int presentation_glyph, font* render_font)
{
    if (render_font == nullptr)
    {
        return 0;
    }
    unsigned int index = font_get_char_data_index(presentation_glyph, render_font);
    if (index == 0xFFFFFFFF || index >= render_font->num_chars)
    {
        // U+25A1, the white square
        index = font_get_char_data_index(0x25A1, render_font);
        if (index == 0xFFFFFFFF || index >= render_font->num_chars)
        {
            return 0xFFFFFFFF;
        }
    }
    return index;
}

// 0x0046D190 (:305)
// LTCG: _status in EDI.
static const wchar_t* vint_string_parse_text(const wchar_t* _source_string, unsigned char** _instr_buffer, unsigned int* _instr_buffer_len, vint_text_parse_status& _status)
{
    if (_source_string == nullptr || _instr_buffer == nullptr || _instr_buffer_len == nullptr)
    {
        return nullptr;
    }
    const wchar_t* string = _source_string;
    // the last place the line may break: its source position, list position, glyph count and width
    const wchar_t* break_source = nullptr;
    unsigned char* break_list = nullptr;
    unsigned int break_num_chars = 0;
    float break_width = 0.0f;
    vint_resource_font* rfont = _status.font;
    vint_text_line_info* line = &_status.line_infos[_status.num_lines];
    float width = line->dims.x;
    font* render_font = font_get_font(rfont->get_title_handle());
    unsigned int len = *_instr_buffer_len;
    unsigned char* list = *_instr_buffer;
    // the list position before the last glyph appended (a forced hyphen replaces that glyph)
    unsigned char* last_glyph_list = list;
    vint_char_direction run_direction = VINT_CHAR_DIR_NONE;
    unsigned int glyph = 0;
    localization_get_next_presentation_glyph_utf16(string, 0, glyph);
    rfont->char_is_reversed(fix_case(glyph, _status.force_case), run_direction);

    // the TEXT header
    unsigned int opcode = VINT_RENDER_TEXT;
    if (!vint_instr_list_append(opcode, list, len))
    {
        return string;
    }
    vint_render_data_text* text = vint_instr_list_append<vint_render_data_text>(list, len);
    if (text == nullptr)
    {
        return string;
    }
    text->handle = _status.font->get_title_handle();
    text->num_chars = 0;
    text->pos.y = 0.0f;
    text->scale = _status.scale();
    text->pos.x = _status.font_reverse ? -_status.line_infos[_status.num_lines].dims.x : _status.line_infos[_status.num_lines].dims.x;
    vint_string_section_info* section = &_status.section_infos[_status.num_section_infos];
    section->pos = &text->pos;
    section->line_num = _status.num_lines;
    section->pos_offset.zero();
    section->height = (float)((double)_status.font->get_height() * (double)_status.scale().y);
    _status.start_new_line = false;
    if (line->dims.x > 0.0f)
    {
        // a run that does not start the line may break before itself
        break_source = string;
        break_width = width;
        break_list = list;
        break_num_chars = 0;
    }

    const wchar_t* next = string + 1;
    unsigned int prev_glyph = 0;
    for (;;)
    {
        unsigned int raw_glyph = 0;
        unsigned int glyph_len = localization_get_next_presentation_glyph_utf16(string, prev_glyph, raw_glyph);
        glyph = fix_case(raw_glyph, _status.force_case);
        prev_glyph = glyph;
        unsigned int fixed_glyph = glyph;
        wchar_t c = *string;
        bool add_glyph = true;
        if (c > L'[')
        {
            // the escapes and the private use stand-ins
            if (c < 0xFDD1)
            {
                if (c == 0xFDD0)
                {
                    glyph = '%';
                }
                else if (c == L'\\')
                {
                    if (*next == L's' || *next == L'S')
                    {
                        ++string;
                        ++next;
                        glyph = ' ';
                    }
                    else if (*next == L'n' || *next == L'N')
                    {
                        ++string;
                        _status.start_new_line = true;
                        ++string;
                        break;
                    }
                }
            }
            else
            {
                switch (c)
                {
                case 0xFDD1:
                    glyph = '[';
                    break;
                case 0xFDD2:
                    glyph = '\\';
                    break;
                case 0xFDD3:
                    glyph = '{';
                    break;
                case 0xFDD4:
                    glyph = ' ';
                    break;
                default:
                    break;
                }
            }
        }
        else if (c == L'[')
        {
            if (*next != L'[')
            {
                // a tag ends the run
                break;
            }
            ++string;
            ++next;
        }
        else if (c == L'\0')
        {
            break;
        }
        else if (c == L'\n')
        {
            _status.start_new_line = true;
            ++string;
            break;
        }
        else if (c == L'\r')
        {
            ++next;
            ++string;
            vint_char_direction direction;
            rfont->char_is_reversed(fixed_glyph, direction);
            if (direction != run_direction && direction == VINT_CHAR_DIR_REVERSED)
            {
                break;
            }
            add_glyph = false;
        }
        else if (c == L' ')
        {
            break_list = list;
            break_num_chars = text->num_chars;
            break_width = width;
            break_source = next;
        }
        if (!add_glyph)
        {
            continue;
        }

        // a change of direction ends the run (a space before it goes to the next run)
        vint_char_direction direction;
        rfont->char_is_reversed(glyph, direction);
        if (direction != run_direction)
        {
            if (run_direction == VINT_CHAR_DIR_NONE)
            {
                run_direction = direction;
            }
            else if (direction != VINT_CHAR_DIR_NONE)
            {
                if (string != _source_string && string[-1] == L' ')
                {
                    --string;
                }
                break;
            }
        }
        unsigned int next_glyph = 0;
        localization_get_next_presentation_glyph_utf16(string + glyph_len, glyph, next_glyph);
        next_glyph = fix_case(next_glyph, _status.force_case);
        const vector2& scale = _status.scale();
        float advance = (float)((double)(float)_status.font->get_char_spacing(glyph, next_glyph) * (double)scale.x);
        if (string > _source_string && _status.wrap_width > 0.0f && (double)width + (double)advance > (double)_status.wrap_width)
        {
            _status.start_new_line = true;
            if (break_width > 0.0f)
            {
                // back to the last break
                width = break_width;
                list = break_list;
                text->num_chars = break_num_chars;
                string = break_source;
            }
            else if (_status.use_hyphens)
            {
                // Original: the hyphen replaces the last glyph appended (at its list position) and the glyph is
                // laid out again on the next line; the run's glyph count and width are not adjusted.
                string -= glyph_len;
                glyph = '-';
                unsigned int index = vint_presentation_glyph_to_font_index('-', render_font);
                if (!vint_instr_list_append(index, last_glyph_list, len))
                {
                    return string;
                }
            }
            break;
        }
        if (glyph == '-')
        {
            // after a hyphen
            break_list = list + 4;
            break_width = (float)((double)width + (double)advance);
            break_source = next;
            break_num_chars = text->num_chars + 1;
        }
        else if (!_status.use_hyphens)
        {
            bool can_start = false;
            bool can_end = false;
            rfont->char_can_start_lb(glyph, can_start);
            rfont->char_can_end_lb(next_glyph, can_end);
            if (can_start && can_end)
            {
                break_num_chars = text->num_chars;
                break_list = list;
                break_width = width;
                break_source = string;
            }
        }
        last_glyph_list = list;
        unsigned int index = vint_presentation_glyph_to_font_index(glyph, render_font);
        if (!vint_instr_list_append(index, list, len))
        {
            return string;
        }
        ++text->num_chars;
        // Original: the source advances one wchar per glyph (a surrogate pair is read again)
        ++string;
        ++next;
        width = (float)((double)width + (double)advance);
    }

    if (text->num_chars == 0)
    {
        return string;
    }
    if ((text->num_chars & 1) != 0)
    {
        // an even glyph count
        unsigned int pad = 0;
        if (!vint_instr_list_append(pad, list, len))
        {
            return string;
        }
        ++text->num_chars;
    }
    text->reversed = run_direction == VINT_CHAR_DIR_REVERSED;
    if (_status.font_reverse != text->reversed)
    {
        // a run against the line's direction is placed from its other end
        if (!_status.font_reverse)
        {
            text->pos.x = (float)(((double)width - (double)line->dims.x) + (double)text->pos.x);
        }
        else
        {
            text->pos.x = (float)((double)text->pos.x - ((double)width - (double)line->dims.x));
        }
    }
    *_instr_buffer = list;
    line->dims.x = width;
    *_instr_buffer_len = len;
    ++_status.num_section_infos;
    return string;
}

// ---- vint_line_frame

// 0x0046D880 (:608)
vint_line_frame::vint_line_frame(const vint_text_render_params& _params)
{
    line_frame_w_bms = -1;
    line_frame_m_bms = -1;
    line_frame_e_bms = -1;
    w_header = nullptr;
    m_header = nullptr;
    e_header = nullptr;
    if (!_params.enable_line_framing)
    {
        return;
    }
    vector2 scale;
    if (vint_resource_bitmap* b = vint_resource_bitmap::find(_params.frame_w_h))
    {
        b->get_dimensions(line_frame_w_dims, scale);
        line_frame_w_bms = b->get_bms_id();
    }
    if (vint_resource_bitmap* b = vint_resource_bitmap::find(_params.frame_m_h))
    {
        b->get_dimensions(line_frame_m_dims, scale);
        line_frame_m_bms = b->get_bms_id();
    }
    if (vint_resource_bitmap* b = vint_resource_bitmap::find(_params.frame_e_h))
    {
        b->get_dimensions(line_frame_e_dims, scale);
        line_frame_e_bms = b->get_bms_id();
    }
}

// 0x0046D940 (:646)
void vint_line_frame::process_eol(vint_text_parse_status& _params, const vint_text_line_info& _line_info, unsigned char*& _instr_list, unsigned int& _instr_list_len)
{
    // the frame images of the line: west cap before it, middle stretched over it, east cap after it
    if (w_header != nullptr && _params.font_reverse)
    {
        w_header->pos.x = (float)(-(double)_line_info.dims.x - (double)_params.scale().x * (double)(float)line_frame_w_dims.x);
    }
    if (m_header != nullptr)
    {
        m_header->pos.x = _params.font_reverse ? -_line_info.dims.x : 0.0f;
        m_header->pos.y = (float)((double)(float)-line_frame_m_dims.y * 0.5 * (double)m_header->scale.y);
        m_header->scale.x = (float)((double)_line_info.dims.x / (double)(float)line_frame_m_dims.x);
        m_header = nullptr;
    }
    if (e_header != nullptr)
    {
        e_header->pos.x = _params.font_reverse ? 0.0f : _line_info.dims.x;
        e_header->pos.y = (float)((double)(float)-line_frame_e_dims.y * 0.5 * (double)e_header->scale.y);
        e_header = nullptr;
    }
}

// 0x0046DA80 (:680)
void vint_line_frame::process_bol(vint_text_parse_status& _params, unsigned char*& _instr_list, unsigned int& _instr_list_len)
{
    unsigned int opcode = VINT_RENDER_IMAGE;
    if (line_frame_w_bms != -1)
    {
        if (!vint_instr_list_append(opcode, _instr_list, _instr_list_len))
        {
            return;
        }
        w_header = vint_instr_list_append<vint_render_data_image>(_instr_list, _instr_list_len);
        if (w_header == nullptr)
        {
            return;
        }
        w_header->pos.x = (float)((double)_params.scale().x * (double)(float)-line_frame_w_dims.x);
        w_header->pos.y = (float)((double)_params.scale().y * (double)(float)-line_frame_w_dims.y * 0.5);
        w_header->handle = line_frame_w_bms;
        w_header->scale = _params.scale();
        w_header->tinted = false;
        vint_string_section_info& section = _params.section_infos[_params.num_section_infos];
        section.line_num = _params.num_lines;
        section.pos = &w_header->pos;
        section.pos_offset.zero();
        section.height = (float)((double)w_header->scale.y * (double)(float)line_frame_w_dims.y);
        ++_params.num_section_infos;
    }
    if (line_frame_m_bms != -1)
    {
        if (!vint_instr_list_append(opcode, _instr_list, _instr_list_len))
        {
            return;
        }
        m_header = vint_instr_list_append<vint_render_data_image>(_instr_list, _instr_list_len);
        if (m_header == nullptr)
        {
            return;
        }
        m_header->handle = line_frame_m_bms;
        m_header->scale = _params.scale();
        m_header->tinted = false;
        vint_string_section_info& section = _params.section_infos[_params.num_section_infos];
        section.line_num = _params.num_lines;
        section.pos = &m_header->pos;
        section.pos_offset.zero();
        // Original: the middle and east images' heights use the west image's height
        section.height = (float)((double)_params.scale().y * (double)(float)line_frame_w_dims.y);
        ++_params.num_section_infos;
    }
    if (line_frame_e_bms != -1)
    {
        if (!vint_instr_list_append(opcode, _instr_list, _instr_list_len))
        {
            return;
        }
        e_header = vint_instr_list_append<vint_render_data_image>(_instr_list, _instr_list_len);
        if (e_header == nullptr)
        {
            return;
        }
        e_header->handle = line_frame_e_bms;
        e_header->scale = _params.scale();
        e_header->tinted = false;
        vint_string_section_info& section = _params.section_infos[_params.num_section_infos];
        section.line_num = _params.num_lines;
        section.pos = &e_header->pos;
        section.pos_offset.zero();
        section.height = (float)((double)_params.scale().y * (double)(float)line_frame_w_dims.y);
        ++_params.num_section_infos;
    }
}

// 0x00466A90 (:782)
// LTCG: _left in EAX, _right in ECX, _num_chars on the stack.
static bool vint_compare_chars(const wchar_t* _left, const wchar_t* _right, unsigned int _num_chars)
{
    // case-insensitive (ASCII) on the first _num_chars characters
    if (_left == nullptr || _right == nullptr || _num_chars < 1)
    {
        return false;
    }
    for (unsigned int i = 0; i < _num_chars; ++i)
    {
        wchar_t l = _left[i];
        wchar_t r = _right[i];
        if ((unsigned short)(l - L'A') <= 25)
        {
            l += 0x20;
        }
        if ((unsigned short)(r - L'A') <= 25)
        {
            r += 0x20;
        }
        if (r != l)
        {
            return false;
        }
    }
    return true;
}

// 0x00466AF0 (:815)
// LTCG: _s in EDI, _c on the stack.
static void scan_to_chr(const wchar_t*& _s, const wchar_t* _c)
{
    // to the first of the characters (or the end)
    if (_s == nullptr || _c == nullptr)
    {
        return;
    }
    unsigned int num_c = (unsigned int)wcslen(_c);
    if (num_c == 0 || *_s == L'\0')
    {
        return;
    }
    do
    {
        for (unsigned int i = 0; i < num_c; ++i)
        {
            if (*_s == _c[i])
            {
                return;
            }
        }
        ++_s;
    } while (*_s != L'\0');
}

// 0x00466B50 (:841)
// LTCG: _source in EAX.
static void consume_tag(const wchar_t*& _source)
{
    scan_to_chr(_source, L"]");
    if (_source != nullptr && *_source == L']')
    {
        ++_source;
    }
}

// 0x0046DE10 (:850)
// LTCG: _instr_buffer in EDI.
static void create_color_instruction(unsigned char** _instr_buffer, unsigned int* _instr_buffer_len, vint_text_parse_status& _status)
{
    if (_instr_buffer == nullptr || _instr_buffer_len == nullptr)
    {
        return;
    }
    unsigned int len = *_instr_buffer_len;
    unsigned char* list = *_instr_buffer;
    if (len < 4)
    {
        keen::debug::nativePrintArgs("Instruction Buffer Full");
        return;
    }
    // the opcode is written before the size check of the data (not committed on failure)
    len -= 4;
    *(unsigned int*)list = VINT_RENDER_COLOR;
    if (len < sizeof(vint_render_data_color))
    {
        keen::debug::nativePrintArgs("Instruction Buffer Full");
        return;
    }
    *(vint_render_data_color*)(list + 4) = _status.color();
    *_instr_buffer = list + 4 + sizeof(vint_render_data_color);
    *_instr_buffer_len = len - sizeof(vint_render_data_color);
}

// 0x00466B80 (:874)
static const wchar_t* vint_string_parse_format(const wchar_t* _source_string, unsigned char** _instr_buffer, unsigned int* _instr_buffer_len, vint_text_parse_status& _status)
{
    _status.format_push();
    consume_tag(_source_string);
    return _source_string;
}

// 0x0046DE90 (:888)
static const wchar_t* vint_string_parse_slash_format(const wchar_t* _source_string, unsigned char** _instr_buffer, unsigned int* _instr_buffer_len, vint_text_parse_status& _status)
{
    // back to the outer colour
    _status.format_pop();
    create_color_instruction(_instr_buffer, _instr_buffer_len, _status);
    consume_tag(_source_string);
    return _source_string;
}

// 0x00473480 (:904)
static const wchar_t* vint_string_parse_image(const wchar_t* _source_string, unsigned char** _instr_buffer, unsigned int* _instr_buffer_len, vint_text_parse_status& _status)
{
    if (_source_string == nullptr || _instr_buffer == nullptr || _instr_buffer_len == nullptr)
    {
        return nullptr;
    }
    const wchar_t* string = _source_string;
    bool tinted = vint_compare_chars(_source_string, L"image_tint:", 11);
    scan_to_chr(string, L":]");
    if (*string == L':')
    {
        ++string;
        skip_white_space(string);
        // name,x,y,caption (the caption in hexadecimal UTF-8)
        char name[64];
        char fields[5][64];
        memset(fields[1], 0, sizeof(fields) - sizeof(fields[0]));
        unsigned int name_len = 0;
        unsigned int field = 0;
        unsigned int field_len = 0;
        while (!is_white_space(*string) && *string != L']' && *string != L'\0')
        {
            if (*string == L',')
            {
                ++field;
                field_len = 0;
            }
            else if (field == 0)
            {
                // Original: not bounded (64 bytes)
                name[name_len++] = (char)*string;
            }
            else if (field < 5 && field_len < 63)
            {
                fields[field][field_len++] = (char)*string;
            }
            ++string;
        }
        if (name_len < 64)
        {
            name[name_len] = '\0';
        }
        else
        {
            name[63] = '\0';
        }
        bool has_caption = false;
        int caption_x = 0;
        int caption_y = 0;
        char caption[64];
        memset(caption, 0, sizeof(caption));
        if ((int)field > 2)
        {
            has_caption = true;
            const char* p = fields[1];
            keen::readSint32FromDecimalString(&caption_x, &p);
            p = fields[2];
            keen::readSint32FromDecimalString(&caption_y, &p);
            const char* hex = fields[3];
            unsigned int hex_len = keen::getStringLength(hex);
            if ((hex_len & 1) == 0)
            {
                for (unsigned int i = 0; i < 63 && (unsigned int)(hex - fields[3]) < hex_len; ++i)
                {
                    char digits[4] = {hex[0], hex[1], 0, 0};
                    const char* d = digits;
                    uint32 value = 0;
                    keen::readUint32FromHexadecimalString(&value, &d);
                    hex += 2;
                    caption[i] = (char)value;
                }
            }
        }
        int image_id = bms_load(name);
        if (image_id != -1)
        {
            int image_w;
            int image_h;
            float image_scale_x;
            float image_scale_y;
            bms_get_dimensions(image_id, image_w, image_h, image_scale_x, image_scale_y);
            vint_text_line_info* line = &_status.line_infos[_status.num_lines];
            float image_width = (float)((double)(float)image_w * (double)_status.scale().x);
            float new_width = (float)((double)line->dims.x + (double)image_width);
            if (line->dims.x > 0.0f && _status.wrap_width > 0.0f && _status.wrap_width < new_width)
            {
                // on the next line: the tag is parsed again
                _status.start_new_line = true;
                return _source_string;
            }
            unsigned char* list = *_instr_buffer;
            unsigned int len = *_instr_buffer_len;
            unsigned int opcode = VINT_RENDER_IMAGE;
            if (!vint_instr_list_append(opcode, list, len))
            {
                return string;
            }
            vint_render_data_image* image = vint_instr_list_append<vint_render_data_image>(list, len);
            if (image == nullptr)
            {
                return string;
            }
            image->pos.x = _status.font_reverse ? (float)(-(double)line->dims.x - (double)image_width) : line->dims.x;
            image->handle = image_id;
            image->scale.x = _status.scale().x;
            image->scale.y = _status.scale().y;
            image->pos.y = 0.0f;
            image->tinted = tinted;
            if (has_caption)
            {
                // the caption: white at 60% of the scale, offset from the image's corner
                vint_string_parse_format(L"format]", &list, &len, _status);
                vint_render_data_color& c = _status.color();
                c.channels[0] = 1.0f;
                c.channels[1] = 1.0f;
                c.channels[2] = 1.0f;
                c.channels[3] = -1.0f;
                create_color_instruction(&list, &len, _status);
                opcode = VINT_RENDER_TEXT;
                vint_render_data_text* text = nullptr;
                if (vint_instr_list_append(opcode, list, len))
                {
                    text = vint_instr_list_append<vint_render_data_text>(list, len);
                }
                if (text == nullptr)
                {
                    vint_string_parse_slash_format(L"/format]", &list, &len, _status);
                    return string;
                }
                font* render_font = font_get_font(_status.font->get_title_handle());
                text->handle = _status.font->get_title_handle();
                text->num_chars = 0;
                text->pos.y = (float)((double)(float)caption_y + (double)image->pos.y);
                text->scale.x = (float)((double)_status.scale().x * 0.6);
                text->scale.y = (float)((double)_status.scale().y * 0.6);
                wchar_t caption_utf16[64];
                keen::convertUTF8ToUTF16((uint16*)caption_utf16, sizeof(caption_utf16), caption);
                const wchar_t* p = caption_utf16;
                unsigned int glyph = 0;
                unsigned int glyph_len = localization_get_next_presentation_glyph_utf16(p, 0, glyph);
                vint_char_direction direction = VINT_CHAR_DIR_NONE;
                text->reversed = _status.font != nullptr && (_status.font->char_is_reversed(glyph, direction), direction == VINT_CHAR_DIR_REVERSED);
                if (!text->reversed)
                {
                    text->pos.x = (float)((double)(float)caption_x + (double)image->pos.x);
                }
                else
                {
                    text->pos.x = (float)(((double)(float)image_w * (double)image->scale.x + (double)image->pos.x) - (double)(float)caption_x);
                }
                while (*p != L'\0')
                {
                    unsigned int index = vint_presentation_glyph_to_font_index(glyph, render_font);
                    if (!vint_instr_list_append(index, list, len))
                    {
                        vint_string_parse_slash_format(L"/format]", &list, &len, _status);
                        return string;
                    }
                    ++text->num_chars;
                    p += glyph_len;
                    glyph_len = localization_get_next_presentation_glyph_utf16(p, glyph, glyph);
                }
                vint_string_parse_slash_format(L"/format]", &list, &len, _status);
                vint_string_section_info& section = _status.section_infos[_status.num_section_infos];
                section.line_num = _status.num_lines;
                section.pos = &text->pos;
                section.pos_offset.zero();
                section.pos_offset.y = text->pos.y;
                section.height = (float)((double)image->scale.y * (double)(float)image_h);
                ++_status.num_section_infos;
            }
            line->dims.x = new_width;
            vint_string_section_info& section = _status.section_infos[_status.num_section_infos];
            section.line_num = _status.num_lines;
            section.pos = &image->pos;
            section.pos_offset.zero();
            section.height = (float)((double)image->scale.y * (double)(float)image_h);
            ++_status.num_section_infos;
            *_instr_buffer = list;
            *_instr_buffer_len = len;
        }
    }
    consume_tag(string);
    return string;
}

// 0x0046DEF0 (:1158)
static const wchar_t* vint_string_parse_scale(const wchar_t* _source_string, unsigned char** _instr_buffer, unsigned int* _instr_buffer_len, vint_text_parse_status& _status)
{
    if (_source_string == nullptr || _instr_buffer == nullptr || _instr_buffer_len == nullptr)
    {
        return nullptr;
    }
    scan_to_chr(_source_string, L":]");
    if (*_source_string == L':')
    {
        const wchar_t* start = _source_string + 1;
        skip_white_space(start);
        float sign = 1.0f;
        if (*start == L'-')
        {
            ++start;
            sign = -1.0f;
        }
        _source_string = start;
        if (*start >= L'0' && *start <= L'9')
        {
            while (*_source_string >= L'0' && *_source_string <= L'9')
            {
                ++_source_string;
            }
            // the integer digits, from the last
            float value = 0.0f;
            float place = 1.0f;
            for (const wchar_t* d = _source_string - 1; d >= start; --d)
            {
                value = (float)((double)(float)(int)(*d - L'0') * (double)place + (double)value);
                place = (float)((double)place * 10.0);
            }
            if (*_source_string == L'.')
            {
                place = 0.1f;
                for (;;)
                {
                    wchar_t d = _source_string[1];
                    ++_source_string;
                    if (d < L'0' || d > L'9')
                    {
                        break;
                    }
                    value = (float)((double)(float)(int)(d - L'0') * (double)place + (double)value);
                    place = (float)((double)place * 0.1);
                }
            }
            vector2& scale = _status.scale();
            scale.x = (float)((double)value * (double)sign);
            scale.y = (float)((double)value * (double)sign);
        }
    }
    consume_tag(_source_string);
    return _source_string;
}

// 0x0046E0E0 (:1223)
static const wchar_t* vint_string_parse_color(const wchar_t* _source_string, unsigned char** _instr_buffer, unsigned int* _instr_buffer_len, vint_text_parse_status& _status)
{
    if (_source_string == nullptr || _instr_buffer == nullptr || _instr_buffer_len == nullptr)
    {
        return nullptr;
    }
    float channels[4] = {0.0f, 0.0f, 0.0f, 0.0f};
    scan_to_chr(_source_string, L":]");
    if (*_source_string == L':')
    {
        ++_source_string;
        skip_white_space(_source_string);
        const wchar_t* name = _source_string;
        bool valid = true;
        if (*_source_string == L'#')
        {
            // #rrggbb or #rrggbbaa (the alpha stays as it is without it)
            ++_source_string;
            int num_channels = 0;
            for (; num_channels < 4; ++num_channels)
            {
                float value = 0.0f;
                channels[num_channels] = value;
                bool complete = true;
                for (int digit = 0; digit < 2; ++digit)
                {
                    unsigned int c = *_source_string;
                    int v;
                    if (c >= L'A' && c <= L'F')
                    {
                        v = c - 0x37;
                    }
                    else if (c >= L'a' && c <= L'f')
                    {
                        v = c - 0x57;
                    }
                    else if (c >= L'0' && c <= L'9')
                    {
                        v = c - 0x30;
                    }
                    else
                    {
                        complete = false;
                        break;
                    }
                    ++_source_string;
                    value = (float)((double)(float)(v * (digit == 0 ? 16 : 1)) + (double)channels[num_channels]);
                    channels[num_channels] = value;
                }
                if (!complete)
                {
                    break;
                }
                channels[num_channels] = (float)((double)value / 255.0);
            }
            if (num_channels < 3)
            {
                valid = false;
            }
            else
            {
                for (int i = num_channels; i < 4; ++i)
                {
                    channels[i] = _status.color().channels[i];
                }
            }
        }
        else
        {
            // a colour table name
            int name_len = 0;
            while (!is_white_space(*_source_string) && *_source_string != L']' && *_source_string != L'\0')
            {
                ++_source_string;
                ++name_len;
            }
            const vint_variant_color* c = vint_color_table_find(name, name_len);
            if (c == nullptr)
            {
                valid = false;
            }
            else
            {
                channels[0] = c->r;
                channels[1] = c->g;
                channels[2] = c->b;
                channels[3] = _status.color().channels[3];
            }
        }
        if (valid)
        {
            vint_render_data_color& c = _status.color();
            for (int i = 0; i < 4; ++i)
            {
                c.channels[i] = channels[i];
            }
            create_color_instruction(_instr_buffer, _instr_buffer_len, _status);
        }
    }
    consume_tag(_source_string);
    return _source_string;
}

// 0x00473C40 (:1309)
static const wchar_t* vint_string_parse_button(const wchar_t* _source_string, unsigned char** _instr_buffer, unsigned int* _instr_buffer_len, vint_text_parse_status& _status)
{
    if (_source_string == nullptr || _instr_buffer == nullptr || _instr_buffer_len == nullptr)
    {
        return nullptr;
    }
    const wchar_t* string = _source_string;
    scan_to_chr(string, L":]");
    bool tinted = vint_compare_chars(_source_string, L"button_tint:", 12);
    if (*string == L':')
    {
        ++string;
        skip_white_space(string);
        char name[64];
        unsigned int name_len = 0;
        for (; !is_white_space(*string) && *string != L']' && *string != L'\0'; ++string)
        {
            // Original: not bounded (64 bytes)
            name[name_len++] = (char)*string;
        }
        if (name_len < 64)
        {
            name[name_len] = '\0';
        }
        else
        {
            name[63] = '\0';
        }
        // the game gives up to two images (a key and its modifier), each parsed as an image tag
        char images[2][256];
        memset(images, 0, sizeof(images));
        if (Resource_callbacks != nullptr && Resource_callbacks->button_display_info != nullptr)
        {
            Resource_callbacks->button_display_info(images[0], images[1], 256, name);
        }
        for (unsigned int i = 0; i < 2; ++i)
        {
            if (images[i][0] == '\0')
            {
                continue;
            }
            char tag[512];
            keen::formatString(tag, sizeof(tag), "[%s:%s]", tinted ? "image_tint" : "image", images[i]);
            wchar_t tag_utf16[512];
            keen::convertUTF8ToUTF16((uint16*)tag_utf16, sizeof(tag_utf16), tag);
            vint_string_parse_all_sections(tag_utf16, _instr_buffer, _instr_buffer_len, _status);
        }
    }
    consume_tag(string);
    return string;
}

// 0x0046E340 (:1381)
const wchar_t* vint_string_parse_section(const wchar_t* _source_string, unsigned char** _instr_buffer, unsigned int* _instr_buffer_len, vint_text_parse_status& _status)
{
    if (_source_string == nullptr || _instr_buffer == nullptr)
    {
        return nullptr;
    }
    if (*_source_string == L'[')
    {
        const wchar_t* tag = _source_string + 1;
        skip_white_space(tag);
        for (unsigned int i = 0; i < Num_parse_funcs; ++i)
        {
            if (vint_compare_chars(tag, Tag_parse_func_table[i].m_tag, Tag_parse_func_table[i].m_tag_length))
            {
                const wchar_t* result = Tag_parse_func_table[i].m_func(tag, _instr_buffer, _instr_buffer_len, _status);
                // a tag that did not fit on the line is parsed again on the next one
                if (!_status.start_new_line || *tag == L'\0')
                {
                    return result;
                }
                return _source_string;
            }
        }
    }
    return vint_string_parse_text(_source_string, _instr_buffer, _instr_buffer_len, _status);
}

// 0x0046E400 (:1417)
const wchar_t* vint_string_parse_all_sections(const wchar_t* _source_string, unsigned char** _instr_buffer, unsigned int* _instr_buffer_len, vint_text_parse_status& _status)
{
    const wchar_t* string = _source_string;
    unsigned int num_stalls = 0;
    while (*string != L'\0')
    {
        const wchar_t* next = vint_string_parse_section(string, _instr_buffer, _instr_buffer_len, _status);
        bool new_line = _status.start_new_line;
        if (new_line)
        {
            _status.line_frame->process_eol(_status, _status.line_infos[_status.num_lines], *_instr_buffer, *_instr_buffer_len);
            ++_status.num_lines;
            _status.line_infos[_status.num_lines].dims.x = 0.0f;
            _status.line_infos[_status.num_lines].dims.y = 0.0f;
            _status.start_new_line = false;
            _status.line_frame->process_bol(_status, *_instr_buffer, *_instr_buffer_len);
        }
        if (next == string)
        {
            // no progress: allowed once after a line break
            char text[1024];
            if (!new_line)
            {
                _snprintf_safe(text, sizeof(text), "%ls", _source_string);
                VLIB_CRITICAL_FAILURE();
                return next;
            }
            if (++num_stalls > 1)
            {
                _snprintf_safe(text, sizeof(text), "%ls", _source_string);
                VLIB_CRITICAL_FAILURE();
                return next;
            }
        }
        else
        {
            num_stalls = 0;
        }
        string = next;
    }
    return string;
}

// 0x0046E550 (:1479)
unsigned int vint_build_instruction_list(unsigned char* _buffer, unsigned int _buff_size, vector2& _size, bool reverse_text, bool use_hyphens, const wchar_t* _source, const vector2& _text_area, const vint_text_render_params& _params)
{
    if (_source == nullptr || _buffer == nullptr || _buff_size == 0)
    {
        return 0;
    }
    unsigned int mark = Vint_temp_mempool->mark();
    unsigned char* list = _buffer;
    vint_text_line_info* line_infos = (vint_text_line_info*)Vint_temp_mempool->alloc(0x4000, 0);
    vint_string_section_info* section_infos = (vint_string_section_info*)Vint_temp_mempool->alloc(0x5000, 0);
    vint_line_frame line_frame(_params);
    vint_text_parse_status status;
    status.format_stack[0].scale = _params.scale;
    status.format_depth = 0;
    status.wrap_width = _params.word_wrap ? _params.wrap_width : 0.0f;
    status.font = vint_resource_font::find(_params.font_h);
    status.force_case = _params.force_case;
    status.font_reverse = reverse_text;
    status.leading = _params.leading;
    status.use_hyphens = use_hyphens;
    status.line_frame = &line_frame;
    status.num_lines = 0;
    status.num_section_infos = 0;
    status.section_infos = section_infos;
    _size.x = 0.0f;
    _size.y = 0.0f;
    unsigned int len = _buff_size;
    line_infos[0].dims.x = 0.0f;
    line_infos[0].dims.y = 0.0f;
    status.max_line_infos = 1023;
    status.max_section_infos = 1024;
    // -1: the element's colour and alpha
    status.format_stack[0].color.channels[0] = -1.0f;
    status.format_stack[0].color.channels[1] = -1.0f;
    status.format_stack[0].color.channels[2] = -1.0f;
    status.format_stack[0].color.channels[3] = -1.0f;
    status.start_new_line = false;
    status.line_infos = line_infos;
    line_frame.process_bol(status, list, len);
    vint_string_parse_all_sections(_source, &list, &len, status);
    line_frame.process_eol(status, line_infos[status.num_lines], list, len);
    unsigned int num_lines = status.num_lines + 1;
    unsigned int num_sections = status.num_section_infos;
    if (len < 4)
    {
        keen::debug::nativePrintArgs("Instruction Buffer Full");
    }
    else
    {
        *(unsigned int*)list = VINT_RENDER_STOP;
        list += 4;
    }

    // each line is as tall as its tallest section; the text's size
    for (unsigned int i = 0; i < num_sections; ++i)
    {
        vint_text_line_info& line = line_infos[section_infos[i].line_num];
        if (line.dims.y - section_infos[i].height < 0.0f)
        {
            line.dims.y = section_infos[i].height;
        }
    }
    _size.x = 0.0f;
    _size.y = 0.0f;
    for (unsigned int i = 0; i < num_lines; ++i)
    {
        if (_size.x - line_infos[i].dims.x < 0.0f)
        {
            _size.x = line_infos[i].dims.x;
        }
        _size.y = (float)((double)line_infos[i].dims.y + (double)_size.y);
    }

    // the lines' offsets in the text area
    float y = 0.0f;
    if (_params.valign == VINT_TEXT_VALIGN_CENTER)
    {
        y = (float)((double)_text_area.y * 0.5 - (double)_size.y * 0.5);
    }
    else if (_params.valign == VINT_TEXT_VALIGN_BOTTOM)
    {
        y = (float)((double)_text_area.y - (double)_size.y);
    }
    for (unsigned int i = 0; i < num_lines; ++i)
    {
        vint_text_line_info& line = line_infos[i];
        switch (_params.halign)
        {
        case VINT_TEXT_HALIGN_LEFT:
            line.offset.x = reverse_text ? line.dims.x : 0.0f;
            break;
        case VINT_TEXT_HALIGN_CENTER:
            if (reverse_text)
            {
                line.offset.x = (float)((double)line.dims.x * 0.5 + (double)_text_area.x * 0.5);
            }
            else
            {
                line.offset.x = (float)((double)_text_area.x * 0.5 - (double)line.dims.x * 0.5);
            }
            break;
        case VINT_TEXT_HALIGN_RIGHT:
            line.offset.x = reverse_text ? _text_area.x : (float)((double)_text_area.x - (double)line.dims.x);
            break;
        default:
            break;
        }
        line.offset.y = y;
        y = (float)((double)line.dims.y + (double)y);
    }

    // every section onto its line, centred vertically
    for (unsigned int i = 0; i < num_sections; ++i)
    {
        vint_string_section_info& section = section_infos[i];
        vint_text_line_info& line = line_infos[section.line_num];
        section.pos->x = (float)((double)section.pos_offset.x + (double)line.offset.x + (double)section.pos->x);
        section.pos->y = (float)(((double)line.dims.y * 0.5 + (double)line.offset.y) - (double)section.height * 0.5 + (double)section.pos_offset.y);
    }
    Vint_temp_mempool->restore_to_mark(mark);
    return (unsigned int)(list - _buffer);
}

// <vint_render_data_text> 0x00467BC0, <vector2> 0x00467B80, <unsigned int> 0x00467B40,
// <vint_render_data_image> 0x00467C10, <vint_render_data_color> 0x00467C60 (:1617)
// LTCG: register arguments per instantiation.
template<typename T>
static bool instruction_list_extract(T& _out, const unsigned char* _list, unsigned int& _index, unsigned int _length)
{
    if (_list == nullptr || _index >= _length)
    {
        return false;
    }
    if (_index + sizeof(T) > _length)
    {
        keen::debug::nativePrintArgs("Instruction list overrun");
        return false;
    }
    memcpy(&_out, _list + _index, sizeof(T));
    _index += sizeof(T);
    return true;
}

// 0x0046EE50 (:1633)
void vint_render_instruction_list(const unsigned char* _instruction_list, unsigned int _instruction_len, const matrix23& _xform, const vector2& _pos, const vint_variant_color& _color, float _alpha, vint_res_offset _res_offset_x, vint_res_offset _res_offset_y, vint_res_scale _res_scale_x, vint_res_scale _res_scale_y)
{
    if (_instruction_list == nullptr || _instruction_len < 2)
    {
        return;
    }
    unsigned int index = 0;
    while (index < _instruction_len)
    {
        unsigned int opcode;
        if (!instruction_list_extract(opcode, _instruction_list, index, _instruction_len))
        {
            return;
        }
        switch (opcode)
        {
        case VINT_RENDER_STOP:
            return;
        case VINT_RENDER_MOVE_ABS:
        {
            // the position is transformed and not used
            vector2 pos;
            if (!instruction_list_extract(pos, _instruction_list, index, _instruction_len))
            {
                return;
            }
            break;
        }
        case VINT_RENDER_TEXT:
        {
            vint_render_data_text text;
            if (!instruction_list_extract(text, _instruction_list, index, _instruction_len))
            {
                return;
            }
            vector2 pos = _xform * text.pos;
            matrix23 scale;
            scale.make_scale(text.scale);
            matrix23 xform = _xform * scale;
            pos.x = (float)((double)pos.x + (double)_pos.x);
            pos.y = (float)((double)_pos.y + (double)pos.y);
            if (Render_callbacks != nullptr && Render_callbacks->draw_text != nullptr)
            {
                Render_callbacks->draw_text(text.handle, (const unsigned int*)(_instruction_list + index), 0, text.num_chars, xform, pos, text.reversed, _res_offset_x, _res_offset_y, _res_scale_x, _res_scale_y);
            }
            index += text.num_chars * 4;
            break;
        }
        case VINT_RENDER_IMAGE:
        {
            vint_render_data_image image;
            if (!instruction_list_extract(image, _instruction_list, index, _instruction_len))
            {
                return;
            }
            const bm_image* data = vint_bms_get_image_data(image.handle);
            if (data == nullptr)
            {
                break;
            }
            matrix23 scale;
            scale.make_scale(image.scale);
            matrix23 xform = _xform * scale;
            vector2i source_pos((int)data->sx, (int)data->sy);
            vector2i source_size((int)data->bmw, (int)data->bmh);
            vector2 pos = _xform * image.pos;
            pos.x = (float)((double)pos.x + (double)_pos.x);
            pos.y = (float)((double)_pos.y + (double)pos.y);
            color saved;
            if (Render_callbacks != nullptr && Render_callbacks->get_color != nullptr)
            {
                Render_callbacks->get_color(saved);
            }
            if (!image.tinted)
            {
                // drawn white (the current alpha)
                color white(0xFF, 0xFF, 0xFF, saved.alpha);
                if (Render_callbacks != nullptr && Render_callbacks->set_color != nullptr)
                {
                    Render_callbacks->set_color(white);
                }
            }
            int bitmap = vint_bms_get_bitmap(image.handle);
            if (Render_callbacks != nullptr && Render_callbacks->draw_bitmap != nullptr)
            {
                Render_callbacks->draw_bitmap(bitmap, xform, pos, source_pos, source_size, _res_offset_x, _res_offset_y, _res_scale_x, _res_scale_y);
            }
            if (!image.tinted && Render_callbacks != nullptr && Render_callbacks->set_color != nullptr)
            {
                Render_callbacks->set_color(saved);
            }
            break;
        }
        case VINT_RENDER_COLOR:
        {
            vint_render_data_color data;
            if (!instruction_list_extract(data, _instruction_list, index, _instruction_len))
            {
                return;
            }
            // -1 channels: the element's colour (red decides for all three) and alpha
            color c;
            if (data.channels[0] == -1.0f)
            {
                c.red = (unsigned char)(int)((double)_color.r * 255.0);
                c.green = (unsigned char)(int)((double)_color.g * 255.0);
                c.blue = (unsigned char)(int)((double)_color.b * 255.0);
            }
            else
            {
                c.red = (unsigned char)(int)((double)data.channels[0] * 255.0);
                c.green = (unsigned char)(int)((double)data.channels[1] * 255.0);
                c.blue = (unsigned char)(int)((double)data.channels[2] * 255.0);
            }
            float alpha = data.channels[3] != -1.0f ? data.channels[3] : _alpha;
            c.alpha = (unsigned char)(int)((double)alpha * 255.0);
            if (Render_callbacks != nullptr && Render_callbacks->set_color != nullptr)
            {
                Render_callbacks->set_color(c);
            }
            break;
        }
        default:
            keen::debug::nativePrintArgs("Unrecognized render instruction");
            return;
        }
    }
}
