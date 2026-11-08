// Original: volition/interface_lib/code/objects/noninteractive/vint_element_text.cpp
//
// The text element (UI.md UI2): a localized string (by tag crc, else the tag text itself, wide-encoded or UTF-8) with
// up to 32 insertion values substituted (%s, {}, {n}, {n:text_tag_crc}, {n:not_localized}, {symbol}), laid out by
// vint_build_instruction_list into a render instruction list kept per element, and drawn with vint_render_
// instruction_list. Changes mark the text dirty; it is laid out again before rendering or when its size is needed
// (twice if the size changed the layout). The element's size is the laid-out text's. A pool of 432. Compiled in
// interface_lib_unity.cpp.

#include "volition/interface_lib/code/objects/noninteractive/vint_element_text.h"

#include "volition/interface_lib/code/internal/vint_memory.h"
#include "volition/interface_lib/code/internal/vint_prop_macros.h"
#include "volition/interface_lib/code/resources/vint_resource_bitmap.h"
#include "volition/interface_lib/code/resources/vint_resource_font.h"
#include "volition/vlib/code/math/bbox2d.h"
#include "volition/vlib/code/memory/mempool_base.h"
#include "volition/vlib/code/util/color.h"
#include "volition/vlib/code/util/string_comp.h"
#include "volition/vlib/code/util/unicode_tools.h"
#include "library/modules/runtime/base/include/keen/base/stringencoding.hpp"
#include "volition/vlib/code/vlib_core/vlib_assert.h"

#include <math.h>
#include <new>
#include <string.h>

// ---- class statics
unsigned int vint_element_text::Int_obj_type = 0xFFFFFFFF; // 0x014F58E4
const unsigned int& vint_element_text::Object_type = vint_element_text::Int_obj_type; // 0x014F58E8

// ---- file-private data (rfg.pdb)
static const wchar_t* Insertion_tags[2] = {L"text_tag_crc", L"not_localized"}; // 0x014F5934
static const vint_prop_type_enum_info Internal_vint_element_text_vert_align_infos[3] = {{"top", 0}, {"center", 1}, {"bottom", 2}}; // 0x014F58EC
static const vint_prop_type_enum_info Internal_vint_element_text_horz_align_infos[3] = {{"left", 0}, {"center", 1}, {"right", 2}}; // 0x014F5904
static const vint_prop_type_enum_info Internal_vint_element_text_force_case_infos[3] = {{"none", 0}, {"upper", 1}, {"lower", 2}}; // 0x014F591C
static vint_prop_type_enum_wrapper Internal_vint_element_text_vert_align_wrapper(Internal_vint_element_text_vert_align_infos, 3); // 0x014F6DB8
static vint_prop_type_enum_wrapper Internal_vint_element_text_horz_align_wrapper(Internal_vint_element_text_horz_align_infos, 3); // 0x014F6DC0
static vint_prop_type_enum_wrapper Internal_vint_element_text_force_case_wrapper(Internal_vint_element_text_force_case_infos, 3); // 0x014F6DC8

// ---- file-static globals
static vint_prop_lookup Prop_lookup_vint_element_text[16] = { // 0x014F6920
    VINT_PROP_ENTRY(vint_element_text, font, VINT_PROP_TYPE_FONT, 0, true),
    VINT_PROP_ENTRY(vint_element_text, text_tag, VINT_PROP_TYPE_STRING, (const char*)nullptr, true),
    VINT_PROP_ENTRY(vint_element_text, text_tag_crc, VINT_PROP_TYPE_UINT, 0, false),
    VINT_PROP_ENTRY(vint_element_text, text_scale, VINT_PROP_TYPE_VECTOR2F, vector2(1.0f, 1.0f), true),
    VINT_PROP_ENTRY(vint_element_text, word_wrap, VINT_PROP_TYPE_BOOL, false, true),
    VINT_PROP_ENTRY(vint_element_text, wrap_width, VINT_PROP_TYPE_FLOAT, 200.0f, true),
    VINT_PROP_ENTRY(vint_element_text, vert_align, VINT_PROP_TYPE_ENUM, 0, true),
    VINT_PROP_ENTRY(vint_element_text, horz_align, VINT_PROP_TYPE_ENUM, 0, true),
    VINT_PROP_ENTRY(vint_element_text, force_case, VINT_PROP_TYPE_ENUM, 0, true),
    VINT_PROP_ENTRY(vint_element_text, leading, VINT_PROP_TYPE_INT, 0, true),
    // not indexed in the table (the index callbacks below are unused)
    VINT_PROP_ENTRY(vint_element_text, insert_values, VINT_PROP_TYPE_VARIABLE, (const char*)nullptr, false),
    VINT_PROP_ENTRY(vint_element_text, screen_size, VINT_PROP_TYPE_VECTOR2F, vector2(0.0f, 0.0f), false),
    VINT_PROP_ENTRY(vint_element_text, line_frame_enable, VINT_PROP_TYPE_BOOL, false, true),
    VINT_PROP_ENTRY(vint_element_text, line_frame_w, VINT_PROP_TYPE_BITMAP, 0u, true),
    VINT_PROP_ENTRY(vint_element_text, line_frame_m, VINT_PROP_TYPE_BITMAP, 0u, true),
    VINT_PROP_ENTRY(vint_element_text, line_frame_e, VINT_PROP_TYPE_BITMAP, 0u, true),
};
static vint_prop_mgr Prop_mgr_vint_element_text; // 0x01A4F034
static vint_object_factory_imp<vint_element_text> Factory_vint_element_text;

// A memory callback free (rfg.exe inlines the callback check).
static void vint_element_text_free(void* _addy)
{
    if (Memory_callbacks != nullptr && Memory_callbacks->free != nullptr)
    {
        Memory_callbacks->free(_addy);
    }
}

// A memory callback realloc, or null without the callback.
static void* vint_element_text_realloc(void* _addy, unsigned int _bytes)
{
    if (Memory_callbacks != nullptr && Memory_callbacks->realloc != nullptr)
    {
        return Memory_callbacks->realloc(_addy, _bytes);
    }
    return nullptr;
}

// A line frame bitmap's setter (the same for the three images).
static bool vint_element_text_set_frame_bitmap(unsigned int& _handle, const vint_variant& _val)
{
    unsigned int handle = 0;
    vint_resource_bitmap* bitmap = nullptr;
    switch (_val.get_type())
    {
    case VINT_PROP_TYPE_UINT:
        handle = (unsigned int)_val;
        bitmap = vint_resource_bitmap::find(handle);
        // Original: an unknown handle is kept (without a reference)
        break;
    case VINT_PROP_TYPE_STRING:
    {
        const char* name = (const char*)_val;
        if (name != nullptr && *name != '\0')
        {
            bitmap = vint_resource_bitmap::load(name);
            if (bitmap == nullptr)
            {
                return false;
            }
            handle = bitmap->get_handle();
            // load took the reference
            bitmap = nullptr;
        }
        break;
    }
    case VINT_PROP_TYPE_BITMAP:
        bitmap = vint_resource_bitmap::find((unsigned int)_val);
        if (bitmap != nullptr)
        {
            handle = bitmap->get_handle();
        }
        break;
    default:
        return false;
    }
    if (bitmap != nullptr)
    {
        bitmap->add_ref();
    }
    if (handle != _handle)
    {
        if (vint_resource_bitmap* old = vint_resource_bitmap::find(_handle))
        {
            old->release();
        }
        _handle = handle;
    }
    // Original: a handle equal to the current one keeps the new reference (a leak)
    return true;
}

// ---- vint_text_render_params

// 0x004688F0 (.h:68)
vint_text_render_params::vint_text_render_params()
{
    set_default_values();
}

// 0x0045E510 (.h:70)
void vint_text_render_params::set_default_values()
{
    font_h = 0;
    text_area.x = 1.0f;
    text_area.y = 1.0f;
    scale.x = 1.0f;
    scale.y = 1.0f;
    valign = VINT_TEXT_VALIGN_TOP;
    halign = VINT_TEXT_HALIGN_LEFT;
    word_wrap = false;
    wrap_width = 0.0f;
    force_case = VINT_TEXT_CASE_NONE;
    enable_line_framing = false;
    leading = 0;
    frame_w_h = 0;
    frame_m_h = 0;
    frame_e_h = 0;
}

// ---- header-inline members

// 0x00468940 (.h:89)
vint_element_text::vint_element_text()
{
}

// 0x00477730 (.h:90)
vint_element_text::~vint_element_text()
{
    clean_up();
}

// 0x004689E0 (.h:98)
bool vint_element_text::is_type(unsigned int _type) const
{
    return _type == Object_type || _type == vint_element_base::Object_type;
}

// 0x00468A10 (.h:98)
unsigned int vint_element_text::get_type() const
{
    return Object_type;
}

// 0x0045E560 (.h:121)
void vint_element_text::set_text_dirty()
{
    m_text_is_dirty = true;
}

// ---- insertions

// 0x0045E630 (:128)
static unsigned int vint_parse_uint(const wchar_t* _string)
{
    // the decimal digits at the start
    const wchar_t* end = _string;
    while (*end >= L'0' && *end <= L'9')
    {
        ++end;
    }
    unsigned int value = 0;
    unsigned int place = 1;
    for (const wchar_t* d = end - 1; d >= _string; --d)
    {
        value += (unsigned int)(*d - L'0') * place;
        place *= 10;
    }
    return value;
}

// 0x00477750 (:151)
// LTCG: _source in EAX.
static unsigned int vint_string_parse_insertions(wchar_t* _dest, const wchar_t* _source, unsigned int _size, const vint_variant* _insersions, unsigned int _num_insertions)
{
    if (_dest == nullptr || _source == nullptr || _size == 0)
    {
        return 0;
    }
    unsigned int mark = Vint_temp_mempool->mark();
    wchar_t* wide = (wchar_t*)Vint_temp_mempool->alloc(0x800, 0);
    char* narrow = (char*)Vint_temp_mempool->alloc(0x400, 0);
    if (wide == nullptr || narrow == nullptr)
    {
        // Original bug kept: the temp pool is not restored
        return 0;
    }
    wchar_t* out = _dest;
    const wchar_t* src = _source;
    unsigned int next_insertion = 0;
    const vint_variant* insertion = _insersions;
    do
    {
        const wchar_t* text = nullptr;
        bool copy_char = true;
        if (*src == L'%')
        {
            if (src[1] == L'%')
            {
                *out++ = L'%';
                src += 2;
            }
            else if (src[1] != L's' && src[1] != L'S')
            {
                // Original: the % is dropped
                ++src;
            }
            else
            {
                // %s: the next insertion as text
                if (next_insertion < _num_insertions)
                {
                    insertion->to_string(narrow, 0x400);
                    copy_to_wide(wide, narrow, 0x400);
                    text = wide;
                }
                ++next_insertion;
                ++insertion;
                src += 2;
                copy_char = text == nullptr;
            }
        }
        else if (*src == L'{')
        {
            if (src[1] == L'{')
            {
                *out++ = L'{';
                src += 2;
            }
            else
            {
                do
                {
                    do
                    {
                        ++src;
                    } while (*src == L' ');
                } while (*src == L'\t');
                if (*src == L'}')
                {
                    // {}: the next insertion
                    if (next_insertion < _num_insertions)
                    {
                        insertion->to_string(narrow, 0x400);
                        copy_to_wide(wide, narrow, 0x400);
                        text = wide;
                    }
                    ++next_insertion;
                    ++insertion;
                }
                else if (*src < L'0' || *src > L'9')
                {
                    // {symbol}
                    const wchar_t* end = src;
                    while (*end != L'\0' && *end != L'}')
                    {
                        ++end;
                    }
                    text = vint_get_symbol_text(src, (unsigned int)(end - src));
                    if (text == nullptr)
                    {
                        text = L"{Symbol not found!}";
                    }
                }
                else
                {
                    // {n} or {n:format}
                    unsigned int n = vint_parse_uint(src);
                    while ((*src >= L'0' && *src <= L'9') || *src == L' ' || *src == L'\t')
                    {
                        ++src;
                    }
                    if (n < _num_insertions)
                    {
                        int format = -1;
                        if (*src == L':')
                        {
                            // Original bug kept: a NUL is skipped like white space here
                            wchar_t c;
                            do
                            {
                                do
                                {
                                    c = src[1];
                                    ++src;
                                } while (c == L' ');
                            } while (c == L'\t' || c == L'\0');
                            for (int i = 0; i < 2; ++i)
                            {
                                if (_v_strnicmp(src, Insertion_tags[i], (unsigned int)wcslen(Insertion_tags[i])) == 0)
                                {
                                    format = i;
                                    break;
                                }
                            }
                        }
                        vint_variant value;
                        value = _insersions[n];
                        if (value.get_type() == VINT_PROP_TYPE_NONE)
                        {
                            text = L"{No insertion value specified!}";
                        }
                        else if (format == VINT_INSERSION_TEXT_TAG_CRC)
                        {
                            if (value.get_type() == VINT_PROP_TYPE_UINT)
                            {
                                text = vint_get_localized_string_from_hash((unsigned int)value);
                                if (text == nullptr)
                                {
                                    text = L"{Hash not found!}";
                                }
                            }
                            else
                            {
                                text = L"{Invalid insertion value for a hash!}";
                            }
                        }
                        else if (value.get_type() == VINT_PROP_TYPE_STRING)
                        {
                            // localized unless marked otherwise (or not found)
                            const char* string = (const char*)value;
                            if (format == VINT_INSERSION_NOT_LOCALIZED || (text = vint_get_localized_string(string)) == nullptr)
                            {
                                copy_to_wide(wide, string, 0x400);
                                text = wide;
                            }
                        }
                        else
                        {
                            value.to_string(narrow, 0x400);
                            copy_to_wide(wide, narrow, 0x400);
                            text = wide;
                        }
                    }
                }
                // past the closing brace (to the end of the string without one)
                while (*src != L'}' && *src != L'\0')
                {
                    ++src;
                }
                if (*src == L'}')
                {
                    ++src;
                }
                copy_char = text == nullptr;
            }
        }
        if (!copy_char)
        {
            // the insertion's text, as far as the buffer goes
            for (const wchar_t* t = text; *t != L'\0' && (unsigned int)(out - _dest) < _size; ++t)
            {
                *out++ = *t;
            }
            continue;
        }
        *out++ = *src;
        if (*src == L'\0')
        {
            break;
        }
        ++src;
    } while ((unsigned int)(out - _dest) < _size);
    _dest[_size - 1] = L'\0';
    Vint_temp_mempool->restore_to_mark(mark);
    return (unsigned int)(out - _dest);
}

// ---- vint_element_text

// 0x00472120 (:320)
bool vint_element_text::system_init()
{
    Int_obj_type = vint_register_object_class("text", vint_element_base::Object_type, Prop_mgr_vint_element_text, Prop_lookup_vint_element_text, 16, Factory_vint_element_text, 432);
    Internal_vint_element_text_horz_align_wrapper.init(Object_type, "horz_align");
    Internal_vint_element_text_vert_align_wrapper.init(Object_type, "vert_align");
    Internal_vint_element_text_force_case_wrapper.init(Object_type, "force_case");
    return true;
}

// 0x0045E6D0 (:330)
void vint_element_text::system_shutdown()
{
}

// 0x004750B0 (:335)
bool vint_element_text::init()
{
    if (!vint_element_base::init())
    {
        return false;
    }
    m_text_tag_crc = 0;
    m_text_tag = nullptr;
    m_text_size.zero();
    m_render_instructions = nullptr;
    m_render_instruction_len = 0;
    m_text_is_dirty = false;
    m_insert_values = nullptr;
    m_insert_values_buff = nullptr;
    m_text_params.set_default_values();
    return true;
}

// 0x00472230 (:355)
void vint_element_text::destroy()
{
    clean_up();
    vint_object_base::destroy();
}

// 0x00475170 (:361)
void vint_element_text::clean_up()
{
    if (m_insert_values_buff != nullptr)
    {
        // Original: the buffer is freed but m_insert_values and the buffer pointer are kept (dangling)
        for (int i = 0; i < 32; ++i)
        {
            vint_variant& value = m_insert_values[i];
            if (value.get_type() == VINT_PROP_TYPE_STRING && (const char*)value != nullptr)
            {
                vint_element_text_free((void*)(const char*)value);
            }
            value.clear();
        }
        vint_element_text_free(m_insert_values_buff);
    }
    if (m_render_instructions != nullptr)
    {
        vint_element_text_free(m_render_instructions);
        m_render_instructions = nullptr;
    }
    vint_resource_font::release(m_text_params.font_h);
    m_render_instruction_len = 0;
    if (m_text_tag != nullptr)
    {
        vint_element_text_free(m_text_tag);
        m_text_tag = nullptr;
    }
    // Original: the frame handles are released but kept
    if (m_text_params.frame_w_h != 0)
    {
        vint_resource_bitmap::release(m_text_params.frame_w_h);
    }
    if (m_text_params.frame_m_h != 0)
    {
        vint_resource_bitmap::release(m_text_params.frame_m_h);
    }
    if (m_text_params.frame_e_h != 0)
    {
        vint_resource_bitmap::release(m_text_params.frame_e_h);
    }
}

// 0x00472250 (:396)
void vint_element_text::process()
{
}

// 0x0047DFA0 (:401)
void vint_element_text::pre_render()
{
    // a layout that changed the size is done again
    if (m_text_is_dirty)
    {
        reformat_text();
        if (m_text_is_dirty)
        {
            reformat_text();
        }
    }
}

// 0x00472260 (:412)
void vint_element_text::render_object(const vint_render_params& _render_params)
{
    if (m_render_instructions == nullptr)
    {
        return;
    }
    color c;
    c.red = (unsigned char)(int)((double)_render_params.tint.r * 255.0);
    c.green = (unsigned char)(int)((double)_render_params.tint.g * 255.0);
    c.blue = (unsigned char)(int)((double)_render_params.tint.b * 255.0);
    c.alpha = (unsigned char)(int)((double)_render_params.alpha * 255.0);
    if (Render_callbacks != nullptr && Render_callbacks->set_color != nullptr)
    {
        Render_callbacks->set_color(c);
    }
    vint_render_instruction_list(m_render_instructions, m_render_instruction_len, _render_params.object_xform, _render_params.object_pos, _render_params.tint, _render_params.alpha, _render_params.res_offset_x, _render_params.res_offset_y, _render_params.res_scale_x, _render_params.res_scale_y);
}

// 0x00477B70 (:425)
void vint_element_text::reformat_text()
{
    if (!m_text_is_dirty)
    {
        return;
    }
    m_text_is_dirty = false;
    vint_resource_font* rfont = vint_resource_font::find(m_text_params.font_h);
    if (rfont == nullptr || (m_text_tag_crc == 0 && m_text_tag == nullptr))
    {
        vint_element_text_free(m_render_instructions);
        m_render_instructions = nullptr;
        m_render_instruction_len = 0;
        return;
    }
    unsigned int mark = Vint_temp_mempool->mark();
    const wchar_t* source = nullptr;
    if (Localization_callbacks != nullptr && Localization_callbacks->get_localized_string_from_hash != nullptr)
    {
        source = Localization_callbacks->get_localized_string_from_hash(m_text_tag_crc);
    }
    wchar_t* text = (wchar_t*)Vint_temp_mempool->alloc(0x4000, 0);
    if (source == nullptr)
    {
        if (m_text_tag == nullptr)
        {
            m_text_tag_crc = 0;
            vint_element_text_free(m_render_instructions);
            m_render_instructions = nullptr;
            m_render_instruction_len = 0;
            Vint_temp_mempool->restore_to_mark(mark);
            return;
        }
        // the tag is the text itself: wide-encoded, else UTF-8
        wchar_t* decoded = (wchar_t*)Vint_temp_mempool->alloc(0x800, 0);
        if (!vint_decode_wide_text(decoded, 0x400, m_text_tag))
        {
            keen::convertUTF8ToUTF16((uint16*)decoded, 0x800, m_text_tag);
        }
        source = decoded;
    }
    vint_string_parse_insertions(text, source, 0x2000, m_insert_values, m_insert_values != nullptr ? 32 : 0);
    unsigned int old_len = m_render_instruction_len;
    unsigned char instructions[0x2000];
    // Original: both start uninitialized and stay so without the resource callbacks
    bool reversed = false;
    bool use_hyphens = true;
    if (Resource_callbacks != nullptr)
    {
        if (Resource_callbacks->font_is_reversed != nullptr && !Resource_callbacks->font_is_reversed(rfont->get_title_handle(), reversed))
        {
            reversed = false;
        }
        if (Resource_callbacks != nullptr && Resource_callbacks->font_uses_hyphens != nullptr && !Resource_callbacks->font_uses_hyphens(rfont->get_title_handle(), use_hyphens))
        {
            use_hyphens = true;
        }
    }
    if (reversed)
    {
        // a reversed font lays out right to left only if the text starts with a reversed character
        vint_char_direction direction;
        rfont->char_is_reversed((unsigned short)*text, direction);
        if (direction != VINT_CHAR_DIR_REVERSED)
        {
            reversed = false;
        }
    }
    m_render_instruction_len = vint_build_instruction_list(instructions, sizeof(instructions), m_text_size, reversed, use_hyphens, text, m_size, m_text_params);
    if (m_render_instruction_len == 0)
    {
        vint_element_text_free(m_render_instructions);
        m_render_instructions = nullptr;
        m_render_instruction_len = 0;
    }
    else
    {
        if (old_len != m_render_instruction_len || m_render_instructions == nullptr)
        {
            m_render_instructions = (unsigned char*)vint_element_text_realloc(m_render_instructions, m_render_instruction_len);
        }
        if (m_render_instructions == nullptr)
        {
            m_render_instruction_len = 0;
        }
        else
        {
            memcpy(m_render_instructions, instructions, m_render_instruction_len);
        }
    }
    // a changed size needs another layout (the size is the text area)
    if (fabs((double)m_size.x - (double)m_text_size.x) > 0.001 || fabs((double)m_size.y - (double)m_text_size.y) > 0.001)
    {
        m_text_is_dirty = true;
    }
    m_size.x = m_text_size.x;
    m_size.y = m_text_size.y;
    update_auto_offset();
    Vint_temp_mempool->restore_to_mark(mark);
}

// 0x0045E6E0 (:539)
bool vint_element_text::refresh_string(vint_object_base* _t)
{
    if (_t != nullptr && _t->is_type(Object_type))
    {
        ((vint_element_text*)_t)->m_text_is_dirty = true;
    }
    return true;
}

// 0x00468A80 (:547)
void vint_element_text::refresh_all_strings()
{
    Factory_vint_element_text.for_each_active(refresh_string);
}

// 0x00477F30 (:552)
void vint_element_text::get_bbox(bbox2d& box, bool adjust_min_and_max) const
{
    const_cast<vint_element_text*>(this)->reformat_text();
    vint_element_base::get_bbox(box, adjust_min_and_max);
}

// 0x0045E710 (:557), 0x0045E730 (:569)
VINT_PROP_WRAPPERS(vint_element_text, text_tag)

// 0x00475380 (:558)
bool vint_element_text::get_text_tag(vint_variant& _val, int _index) const
{
    if (m_text_tag != nullptr)
    {
        _val = (const char*)m_text_tag;
        return true;
    }
    return false;
}

// 0x00468AC0 (:570)
bool vint_element_text::set_text_tag(const vint_variant& _val, int _index)
{
    if (_val.get_type() == VINT_PROP_TYPE_UINT)
    {
        m_text_tag_crc = (unsigned int)_val;
        vint_element_text_free(m_text_tag);
        m_text_tag = nullptr;
        m_text_is_dirty = true;
        return true;
    }
    const char* tag = (const char*)_val;
    if (_val.get_type() != VINT_PROP_TYPE_STRING || tag == nullptr)
    {
        return false;
    }
    m_text_tag_crc = vint_localization_get_tag_hash(tag);
    unsigned int old_len = m_text_tag != nullptr ? (unsigned int)strlen(m_text_tag) : 0;
    unsigned int new_len = (unsigned int)strlen(tag);
    if (old_len == 0 || old_len != new_len)
    {
        m_text_tag = (char*)vint_element_text_realloc(m_text_tag, new_len + 1);
    }
    if (m_text_tag != nullptr)
    {
        strcpy(m_text_tag, tag);
    }
    m_text_is_dirty = true;
    return true;
}

// 0x0045E750 (:609), 0x0045E770 (:617)
VINT_PROP_WRAPPERS(vint_element_text, text_tag_crc)

// 0x00472330 (:610)
bool vint_element_text::get_text_tag_crc(vint_variant& _val, int _index) const
{
    _val = m_text_tag_crc;
    return true;
}

// 0x0045E790 (:618)
bool vint_element_text::set_text_tag_crc(const vint_variant& _val, int _index)
{
    m_text_tag_crc = (unsigned int)_val;
    vint_element_text_free(m_text_tag);
    m_text_tag = nullptr;
    m_text_is_dirty = true;
    return true;
}

// 0x0045E840 (:632), 0x0045E860 (:646)
VINT_PROP_WRAPPERS(vint_element_text, font)

// 0x00472350 (:633)
bool vint_element_text::get_font(vint_variant& _val, int _index) const
{
    vint_resource_font* f = vint_resource_font::find(m_text_params.font_h);
    if (f != nullptr)
    {
        _val = f;
        return true;
    }
    return false;
}

// 0x00468BF0 (:647)
bool vint_element_text::set_font(const vint_variant& _val, int _index)
{
    vint_resource_font* f = nullptr;
    switch (_val.get_type())
    {
    case VINT_PROP_TYPE_UINT:
    case VINT_PROP_TYPE_FONT:
        f = vint_resource_font::find((unsigned int)_val);
        if (f != nullptr)
        {
            f->add_ref();
        }
        break;
    case VINT_PROP_TYPE_STRING:
        f = vint_resource_font::load((const char*)_val);
        break;
    default:
        return false;
    }
    unsigned int handle = f != nullptr ? f->get_handle() : 0;
    if (handle != m_text_params.font_h)
    {
        vint_resource_font::release(m_text_params.font_h);
        m_text_params.font_h = handle;
        m_text_is_dirty = true;
    }
    // Original: the same font again keeps the new reference (a leak)
    return handle != 0;
}

// 0x0045E880 (:692), 0x0045E8E0 (:700)
VINT_PROP_WRAPPERS(vint_element_text, text_scale)

// 0x0045E8A0 (:693)
bool vint_element_text::get_text_scale(vint_variant& _val, int _index) const
{
    _val = m_text_params.scale;
    return true;
}

// 0x0045E900 (:701)
bool vint_element_text::set_text_scale(const vint_variant& _val, int _index)
{
    m_text_params.scale = (vector2)_val;
    m_text_is_dirty = true;
    return true;
}

// 0x0045E950 (:709), 0x0045E970 (:718)
VINT_PROP_WRAPPERS(vint_element_text, screen_size)

// 0x0047DFD0 (:710)
bool vint_element_text::get_screen_size(vint_variant& _val, int _index) const
{
    vint_element_text* self = const_cast<vint_element_text*>(this);
    self->reformat_text();
    if (get_type() == Object_type)
    {
        self->reformat_text();
    }
    bbox2d box;
    get_bbox(box, true);
    _val = vector2((float)((double)box.max.x - (double)box.min.x), (float)((double)box.max.y - (double)box.min.y));
    return true;
}

// 0x00468CE0 (:719)
bool vint_element_text::set_screen_size(const vint_variant& _val, int _index)
{
    // the wrap area of a word-wrapped text (at least 1x1), in the element's unscaled units
    if (!m_text_params.word_wrap)
    {
        return false;
    }
    vector2 accum;
    if (parent_object != nullptr)
    {
        ((const vint_element_base*)parent_object)->get_accum_scale(accum);
    }
    else
    {
        accum.x = 1.0f;
        accum.y = 1.0f;
    }
    float scale_x = m_scale.x;
    float scale_y = m_scale.y;
    m_size = (vector2)_val;
    float size_x = m_size.x - 1.0f >= 0.0f ? m_size.x : 1.0f;
    m_size.x = size_x;
    float size_y = m_size.y - 1.0f < 0.0f ? 1.0f : m_size.y;
    m_size.y = size_y;
    // Original: the accumulated scale only decides the zero test; the size is divided by the element's own scale
    if ((double)scale_x * (double)accum.x == 0.0)
    {
        m_size.x = 1.0f;
    }
    else
    {
        m_size.x = (float)((double)size_x / (double)scale_x);
    }
    if ((double)scale_y * (double)accum.y != 0.0)
    {
        m_size.y = (float)((double)size_y / (double)scale_y);
    }
    else
    {
        m_size.y = 1.0f;
    }
    m_text_is_dirty = true;
    return true;
}

// 0x0045E990 (:753), 0x0045E9E0 (:761)
VINT_PROP_WRAPPERS(vint_element_text, word_wrap)

// 0x0045E9B0 (:754)
bool vint_element_text::get_word_wrap(vint_variant& _val, int _index) const
{
    _val = m_text_params.word_wrap;
    return true;
}

// 0x0045EA00 (:762)
bool vint_element_text::set_word_wrap(const vint_variant& _val, int _index)
{
    m_text_params.word_wrap = (bool)_val;
    m_text_is_dirty = true;
    return true;
}

// 0x0045EAC0 (:770), 0x0045EB20 (:778)
VINT_PROP_WRAPPERS(vint_element_text, wrap_width)

// 0x0045EAE0 (:771)
bool vint_element_text::get_wrap_width(vint_variant& _val, int _index) const
{
    _val = m_text_params.wrap_width;
    return true;
}

// 0x0045EB40 (:779)
bool vint_element_text::set_wrap_width(const vint_variant& _val, int _index)
{
    // at least 20
    float width = (float)_val;
    m_text_params.wrap_width = width;
    if (20.0f - width >= 0.0f)
    {
        width = 20.0f;
    }
    m_text_params.wrap_width = width;
    m_text_is_dirty = true;
    return true;
}

// 0x0045EBA0 (:788), 0x0045EBC0 (:796)
VINT_PROP_WRAPPERS(vint_element_text, vert_align)

// 0x00468E70 (:789)
bool vint_element_text::get_vert_align(vint_variant& _val, int _index) const
{
    _val = (int)m_text_params.valign;
    return true;
}

// 0x0045EBE0 (:797)
bool vint_element_text::set_vert_align(const vint_variant& _val, int _index)
{
    // not range checked
    m_text_params.valign = (vint_text_vert_align)(int)_val;
    m_text_is_dirty = true;
    return true;
}

// 0x0045EC70 (:805), 0x0045EC90 (:813)
VINT_PROP_WRAPPERS(vint_element_text, horz_align)

// 0x00468E90 (:806)
bool vint_element_text::get_horz_align(vint_variant& _val, int _index) const
{
    _val = (int)m_text_params.halign;
    return true;
}

// 0x0045ECB0 (:814)
bool vint_element_text::set_horz_align(const vint_variant& _val, int _index)
{
    m_text_params.halign = (vint_text_horz_align)(int)_val;
    m_text_is_dirty = true;
    return true;
}

// 0x0045ED40 (:822), 0x0045ED60 (:830)
VINT_PROP_WRAPPERS(vint_element_text, force_case)

// 0x00468EB0 (:823)
bool vint_element_text::get_force_case(vint_variant& _val, int _index) const
{
    _val = (int)m_text_params.force_case;
    return true;
}

// 0x0045ED80 (:831)
bool vint_element_text::set_force_case(const vint_variant& _val, int _index)
{
    m_text_params.force_case = (vint_text_force_case)(int)_val;
    m_text_is_dirty = true;
    return true;
}

// 0x0045EE10 (:839), 0x0045EE30 (:851)
VINT_PROP_WRAPPERS(vint_element_text, insert_values)

// 0x004753A0 (:840)
bool vint_element_text::get_insert_values(vint_variant& _val, int _index) const
{
    if (m_insert_values != nullptr && _index < 32 && m_insert_values[_index].get_type() != VINT_PROP_TYPE_NONE)
    {
        _val = m_insert_values[_index];
        return true;
    }
    return false;
}

// 0x004753E0 (:852)
bool vint_element_text::set_insert_values(const vint_variant& _val, int _index)
{
    if ((unsigned int)_index > 31)
    {
        return false;
    }
    if (m_insert_values == nullptr)
    {
        // operator new[] through the memory callback: the count, then 32 variants
        unsigned int* buff = (unsigned int*)vint_element_text_realloc(nullptr, 0x400);
        m_insert_values_buff = buff;
        if (buff == nullptr)
        {
            return false;
        }
        *buff = 32;
        vint_variant* values = (vint_variant*)(buff + 1);
        for (int i = 0; i < 32; ++i)
        {
            new (&values[i]) vint_variant();
        }
        m_insert_values = values;
    }
    vint_variant& value = m_insert_values[_index];
    if (value.get_type() == VINT_PROP_TYPE_STRING && (const char*)value != nullptr)
    {
        vint_element_text_free((void*)(const char*)value);
    }
    value.clear();
    const char* string = (const char*)_val;
    if (_val.get_type() == VINT_PROP_TYPE_STRING && string != nullptr)
    {
        // a copy of the string
        char* copy = (char*)vint_element_text_realloc(nullptr, (unsigned int)strlen(string) + 1);
        strcpy(copy, string);
        value = (const char*)copy;
        m_text_is_dirty = true;
        // Original: reports failure although the value was set
        return false;
    }
    value = _val;
    m_text_is_dirty = true;
    // Original: reports failure although the value was set
    return false;
}

// 0x0045EE50 (:888)
bool vint_element_text::internal_get_first_index_insert_values(const vint_object_base* _obj, int& _index)
{
    if (_obj == nullptr)
    {
        return false;
    }
    return static_cast<const vint_element_text*>(_obj)->get_first_index_insert_values(_index);
}

// 0x0045EE70 (:889)
bool vint_element_text::get_first_index_insert_values(int& _index) const
{
    if (m_insert_values_buff == nullptr)
    {
        return false;
    }
    _index = 0;
    return true;
}

// 0x0045EE90 (:900)
bool vint_element_text::internal_get_next_index_insert_values(const vint_object_base* _obj, int& _index)
{
    if (_obj == nullptr)
    {
        return false;
    }
    return static_cast<const vint_element_text*>(_obj)->get_next_index_insert_values(_index);
}

// 0x0045EEB0 (:901)
bool vint_element_text::get_next_index_insert_values(int& _index) const
{
    if (m_insert_values_buff != nullptr && _index < 32)
    {
        ++_index;
        return true;
    }
    return false;
}

// 0x0045EF90 (:926), 0x0045EEE0 (:912)
VINT_PROP_WRAPPERS(vint_element_text, line_frame_enable)

// 0x0045EF00 (:913)
bool vint_element_text::set_line_frame_enable(const vint_variant& _val, int _index)
{
    bool enable = (bool)_val;
    if (enable != m_text_params.enable_line_framing)
    {
        m_text_params.enable_line_framing = enable;
        m_text_is_dirty = true;
    }
    return true;
}

// 0x0045EFB0 (:927)
bool vint_element_text::get_line_frame_enable(vint_variant& _val, int _index) const
{
    _val = m_text_params.enable_line_framing;
    return true;
}

// 0x0045F000 (:989), 0x0045EFE0 (:934)
VINT_PROP_WRAPPERS(vint_element_text, line_frame_w)

// 0x00475510 (:935)
bool vint_element_text::set_line_frame_w(const vint_variant& _val, int _index)
{
    return vint_element_text_set_frame_bitmap(m_text_params.frame_w_h, _val);
}

// 0x00472390 (:990)
bool vint_element_text::get_line_frame_w(vint_variant& _val, int _index) const
{
    _val = vint_resource_bitmap::find(m_text_params.frame_w_h);
    return true;
}

// 0x0045F040 (:1052), 0x0045F020 (:997)
VINT_PROP_WRAPPERS(vint_element_text, line_frame_m)

// 0x004755B0 (:998)
bool vint_element_text::set_line_frame_m(const vint_variant& _val, int _index)
{
    return vint_element_text_set_frame_bitmap(m_text_params.frame_m_h, _val);
}

// 0x00472400 (:1053)
bool vint_element_text::get_line_frame_m(vint_variant& _val, int _index) const
{
    _val = vint_resource_bitmap::find(m_text_params.frame_m_h);
    return true;
}

// 0x0045F080 (:1115), 0x0045F060 (:1060)
VINT_PROP_WRAPPERS(vint_element_text, line_frame_e)

// 0x00475650 (:1061)
bool vint_element_text::set_line_frame_e(const vint_variant& _val, int _index)
{
    return vint_element_text_set_frame_bitmap(m_text_params.frame_e_h, _val);
}

// 0x00472470 (:1116)
bool vint_element_text::get_line_frame_e(vint_variant& _val, int _index) const
{
    _val = vint_resource_bitmap::find(m_text_params.frame_e_h);
    return true;
}

// 0x0045F0C0 (:1144), 0x0045F0A0 (:1123)
VINT_PROP_WRAPPERS(vint_element_text, leading)

// 0x00468ED0 (:1124)
bool vint_element_text::set_leading(const vint_variant& _val, int _index)
{
    // the value plus the font's own leading offset, clamped to a signed byte
    int leading = (int)_val;
    int offset = 0;
    if (vint_resource_font* f = vint_resource_font::find(m_text_params.font_h))
    {
        if (Resource_callbacks != nullptr && Resource_callbacks->font_get_leading_offset != nullptr)
        {
            if (!Resource_callbacks->font_get_leading_offset(offset, f->get_title_handle()))
            {
                VLIB_CRITICAL_FAILURE();
                offset = 0;
            }
        }
    }
    leading += offset;
    if (leading < -126)
    {
        m_text_params.leading = -127;
        m_text_is_dirty = true;
        return true;
    }
    if (leading > 128)
    {
        leading = 128;
    }
    // Original bug kept: 128 does not fit the signed char and becomes -128
    m_text_params.leading = (signed char)leading;
    m_text_is_dirty = true;
    return true;
}

// 0x00468FD0 (:1145)
bool vint_element_text::get_leading(vint_variant& _val, int _index) const
{
    _val = (int)m_text_params.leading;
    return true;
}
