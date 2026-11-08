// Original: volition/interface_lib/code/resources/vint_resource_font.cpp
//
// Font resources (UI.md UI2): a pool of 16, loaded by name through the game's font callbacks (a failed load is -1) and
// reference counted; the metrics go through the same callbacks with the font's title handle. A handle is the pool
// index in the low half and a running count in the high half. Compiled in interface_lib_unity.cpp.

#include "volition/interface_lib/code/resources/vint_resource_font.h"

#include "volition/interface_lib/code/internal/vint_dlist.h"
#include "volition/vlib/code/math/crc.h"
#include "volition/vlib/code/vlib_core/vlib_assert.h"

// ---- public globals
vint_resource_font* Loaded_fonts; // 0x01A48654
vint_resource_font* Avail_fonts; // 0x01A48658

// ---- file-static globals
static unsigned short Font_count; // 0x01A4865C
static vint_resource_font Font_resources[16]; // 0x01A4F4A8

// 0x0045E570 (.h:31)
vint_resource_font::vint_resource_font()
{
}

// 0x0045E580 (.h:32)
vint_resource_font::~vint_resource_font()
{
}

// 0x0045E590 (.h:39)
void vint_resource_font::add_ref() const
{
    ++const_cast<vint_resource_font*>(this)->m_ref_count;
}

// 0x00468A20 (.h:48)
void vint_resource_font::add_ref(unsigned int _handle)
{
    if (vint_resource_font* f = find(_handle))
    {
        f->add_ref();
    }
}

// 0x00468A50 (.h:56)
void vint_resource_font::release(unsigned int _handle)
{
    if (vint_resource_font* f = find(_handle))
    {
        f->release();
    }
}

// 0x0045E5A0 (.h:64)
unsigned int vint_resource_font::get_handle() const
{
    return m_internal_handle;
}

// 0x0045E5B0 (.h:69)
int vint_resource_font::get_title_handle() const
{
    return m_title_handle;
}

// 0x00460F80 (:67)
bool vint_resource_font::system_init()
{
    Loaded_fonts = nullptr;
    Avail_fonts = nullptr;
    for (int i = 0; i < 16; ++i)
    {
        vint_dlist_add(Avail_fonts, &Font_resources[i], &vint_resource_font::next, &vint_resource_font::prev);
    }
    Font_count = 0;
    return true;
}

// 0x00461090 (:82)
void vint_resource_font::system_shutdown()
{
}

// 0x004610A0 (:87)
vint_resource_font* vint_resource_font::load(const char* _name)
{
    if (_name == nullptr)
    {
        return nullptr;
    }
    unsigned int name_crc = crc_stri(_name, 0);
    vint_resource_font* f = Loaded_fonts;
    while (f != nullptr)
    {
        if (f->m_name_crc == name_crc)
        {
            ++f->m_ref_count;
            return f;
        }
        f = f->next;
        if (f == Loaded_fonts)
        {
            break;
        }
    }
    f = Avail_fonts;
    if (f == nullptr || Resource_callbacks == nullptr || Resource_callbacks->font_load == nullptr)
    {
        return nullptr;
    }
    int title_handle = Resource_callbacks->font_load(_name);
    if (title_handle == -1)
    {
        return nullptr;
    }
    ++Font_count;
    f->m_internal_handle = ((unsigned int)(f - Font_resources) & 0xFFFF) + (unsigned int)Font_count * 0x10000;
    f->m_title_handle = title_handle;
    f->m_name_crc = crc_stri(_name, 0);
    f->m_ref_count = 1;
    vint_dlist_remove(Avail_fonts, f, &vint_resource_font::next, &vint_resource_font::prev);
    vint_dlist_add(Loaded_fonts, f, &vint_resource_font::next, &vint_resource_font::prev);
    return f;
}

// 0x00461200 (:139)
vint_resource_font* vint_resource_font::find(unsigned int _handle)
{
    if (_handle == 0 || (_handle & 0xFFFF) >= 16)
    {
        return nullptr;
    }
    vint_resource_font* f = &Font_resources[_handle & 0xFFFF];
    return f->m_internal_handle == _handle ? f : nullptr;
}

// 0x00461230 (:171)
vint_resource_font* vint_resource_font::find_by_title_handle(unsigned int _title_handle)
{
    if (_title_handle == 0)
    {
        return nullptr;
    }
    for (int i = 0; i < 16; ++i)
    {
        if ((unsigned int)Font_resources[i].m_title_handle == _title_handle)
        {
            return &Font_resources[i];
        }
    }
    return nullptr;
}

// 0x00461260 (:201)
void vint_resource_font::release()
{
    if (m_ref_count != 1)
    {
        --m_ref_count;
        return;
    }
    if (Resource_callbacks != nullptr && Resource_callbacks->font_release != nullptr)
    {
        Resource_callbacks->font_release(m_title_handle);
    }
    m_internal_handle = 0;
    m_title_handle = -1;
    vint_dlist_remove(Loaded_fonts, this, &vint_resource_font::next, &vint_resource_font::prev);
    vint_dlist_add(Avail_fonts, this, &vint_resource_font::next, &vint_resource_font::prev);
}

// 0x00461320 (:229)
int vint_resource_font::get_height() const
{
    if (Resource_callbacks == nullptr || Resource_callbacks->font_get_height == nullptr)
    {
        return 0;
    }
    int height = 0;
    Resource_callbacks->font_get_height(height, m_title_handle);
    return height;
}

// 0x00461360 (:239)
int vint_resource_font::get_string_width(const wchar_t* _string, int _start, int _num_chars) const
{
    if (Resource_callbacks == nullptr || Resource_callbacks->font_get_string_width == nullptr)
    {
        return 0;
    }
    int width;
    if (!Resource_callbacks->font_get_string_width(width, m_title_handle, _string, _start, _num_chars))
    {
        return 0;
    }
    return width;
}

// 0x004613B0 (:253)
int vint_resource_font::get_leading_offset()
{
    if (Resource_callbacks != nullptr && Resource_callbacks->font_get_leading_offset != nullptr)
    {
        int offset;
        if (Resource_callbacks->font_get_leading_offset(offset, m_title_handle))
        {
            return offset;
        }
        VLIB_CRITICAL_FAILURE();
    }
    return 0;
}

// 0x00461400 (:267)
int vint_resource_font::get_char_spacing(unsigned int _char, unsigned int _next_char) const
{
    if (Resource_callbacks == nullptr || Resource_callbacks->font_get_char_spacing == nullptr)
    {
        return 0;
    }
    int spacing;
    if (!Resource_callbacks->font_get_char_spacing(spacing, _char, _next_char, m_title_handle))
    {
        return 0;
    }
    return spacing;
}

// 0x00461440 (:281)
bool vint_resource_font::is_reversed(bool& reversed) const
{
    if (Resource_callbacks == nullptr || Resource_callbacks->font_is_reversed == nullptr)
    {
        return false;
    }
    bool result = Resource_callbacks->font_is_reversed(m_title_handle, reversed);
    if (!result)
    {
        reversed = false;
    }
    return result;
}

// 0x00461470 (:294)
bool vint_resource_font::should_use_hyphens(bool& use_hyphens) const
{
    if (Resource_callbacks == nullptr || Resource_callbacks->font_uses_hyphens == nullptr)
    {
        return false;
    }
    bool result = Resource_callbacks->font_uses_hyphens(m_title_handle, use_hyphens);
    if (!result)
    {
        use_hyphens = true;
    }
    return result;
}

// 0x004614A0 (:307)
bool vint_resource_font::char_is_reversed(unsigned int _char, vint_char_direction& direction) const
{
    if (Resource_callbacks == nullptr || Resource_callbacks->font_char_is_reversed == nullptr)
    {
        return false;
    }
    bool result = Resource_callbacks->font_char_is_reversed(m_title_handle, _char, direction);
    if (!result)
    {
        direction = VINT_CHAR_DIR_NONE;
    }
    return result;
}

// 0x004614E0 (:320)
bool vint_resource_font::char_can_start_lb(unsigned int _char, bool& can_start)
{
    if (Resource_callbacks == nullptr || Resource_callbacks->font_char_start_line_break == nullptr)
    {
        return false;
    }
    bool result = Resource_callbacks->font_char_start_line_break(m_title_handle, _char, can_start);
    if (!result)
    {
        can_start = true;
    }
    return result;
}

// 0x00461520 (:333)
bool vint_resource_font::char_can_end_lb(unsigned int _char, bool& can_end)
{
    if (Resource_callbacks == nullptr || Resource_callbacks->font_char_end_line_break == nullptr)
    {
        return false;
    }
    bool result = Resource_callbacks->font_char_end_line_break(m_title_handle, _char, can_end);
    if (!result)
    {
        can_end = true;
    }
    return result;
}
