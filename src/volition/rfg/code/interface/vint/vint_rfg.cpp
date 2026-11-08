// Original: volition/rfg/code/interface/vint/vint_rfg.cpp
//
// UI.md UI3: whole. The game side of vint: the callback sets vint_rfg_system_init hands to vint (memory on dlmalloc,
// localization, sound, bitmap/font/sound resources, rendering through gr and rl_draw's 2D path), the project set-up and
// the input map. The sound callbacks are empty in rfg.exe (the documents' sounds go through the game's own audio calls);
// render_draw_video and render_draw_map call the video player and the cartographer.

#include "volition/rfg/code/interface/vint/vint_rfg.h"

#include "keen/base/crc32.hpp"
#include "keen/base/language.hpp"
#include "keen/base/trace.hpp"
#include "volition/interface_lib/code/internal/vint_memory.h"
#include "volition/interface_lib/code/scripting/vint_lua_actions.h"
#include "volition/interface_lib/code/vint_external.h"
#include "volition/interface_lib/code/vint_input.h"
#include "volition/interface_lib/code/vint_input_internal.h"
#include "volition/interface_lib/code/vint_project.h"
#include "volition/interface_lib/code/vint_variant.h"
#include "volition/lua_lib/code/lua_scripts.h"
#include "volition/render_lib/code/gr/gr.h"
#include "volition/render_lib/code/gr/gr_font.h"
#include "volition/render_lib/code/rl_color.h"
#include "volition/render_lib/code/rl_draw.h"
#include "volition/render_lib/code/rl_primitive_defines.h"
#include "volition/render_lib/code/rl_primitive_renderer.h"
#include "volition/render_lib/code/rl_primitive_state.h"
#include "volition/render_lib/code/rl_rect.h"
#include "volition/render_lib/code/rl_render_lib.h"
#include "volition/render_lib/code/rl_screen_info.h"
#include "volition/render_lib/code/rl_state_manager.h"
#include "volition/render_lib/code/rl_texture.h"
#include "volition/rfg/code/control/control_map.h"
#include "volition/rfg/code/hud/minimap.h"
#include "volition/rfg/code/interface/localization/localization_shared.h"
#include "volition/rfg/code/interface/vint/rfg_lua_actions.h"
#include "volition/rfg/code/multi/multi_game/multi_game_badge.h"
#include "volition/rfg/code/os/dlmalloc/dmalloc.h"
#include "volition/rfg/code/rfg.h"
#include "volition/rfg/code/video_player/video_player.h"
#include "volition/vlib/code/bmpman/bmpman.h"
#include "volition/vlib/code/cfile/cfile.h"
#include "volition/vlib/code/memory/page_pool.h"
#include "volition/vlib/code/memory/virtual_mempool.h"
#include "volition/vlib/code/util/unicode_tools.h"
#include "volition/vlib/code/util/vfont.h"
#include "volition/vlib/code/util/string_comp.h"

#include <math.h>
#include <stdio.h>

// ---- file-static globals
// bool s_localization_is_loading (0x01663023) is used here too; placed in localization_shared.h.
static vint_render_mode Vint_render_mode = VINT_RENDER_ADDITIVE; // 0x01665020, initial data 01000000
static vint_memory_callbacks Memory_callbacks_rfg = {dlrealloc, dlfree}; // 0x01665024
static unsigned int Num_button_mappings = 9; // 0x0166502C
// The data at 0x01665030: {controller, button, threshold, event}; the controller ids are set by vint_rfg_update_input_map.
static vint_button_map Button_map[9] = {
    {0, 8, 0.5f, VINT_INPUT_EVENT_NAV_RIGHT},
    {0, 10, 0.5f, VINT_INPUT_EVENT_NAV_LEFT},
    {0, 9, 0.5f, VINT_INPUT_EVENT_NAV_UP},
    {0, 11, 0.5f, VINT_INPUT_EVENT_NAV_DOWN},
    {0, 0, 0.5f, VINT_INPUT_EVENT_SELECT},
    {0, 2, 0.5f, VINT_INPUT_EVENT_ALT_SELECT},
    {0, 1, 0.5f, VINT_INPUT_EVENT_BACK},
    {0, 4, 0.5f, VINT_INPUT_EVENT_WHITE},
    {0, 5, 0.5f, VINT_INPUT_EVENT_BLACK},
};
// The data at 0x016650C0: {controller, stick, direction, threshold, event, delay}
static vint_joy_map Joy_map[4] = {
    {0, 0, INPUT_DIR_RIGHT, 0.5f, VINT_INPUT_EVENT_JOY_RIGHT, 0},
    {0, 0, INPUT_DIR_LEFT, -0.5f, VINT_INPUT_EVENT_JOY_LEFT, 0},
    {0, 0, INPUT_DIR_UP, 0.5f, VINT_INPUT_EVENT_JOY_UP, 0},
    {0, 0, INPUT_DIR_DOWN, -0.5f, VINT_INPUT_EVENT_JOY_DOWN, 0},
};
static unsigned int Num_joy_mappings = 4; // 0x01665120
// The data at 0x01665124: {key code, event}
static vint_kb_map Kb_map[4] = {
    {0x2C, VINT_INPUT_EVENT_KEY_RIGHT},
    {0x29, VINT_INPUT_EVENT_KEY_LEFT},
    {0x3F, VINT_INPUT_EVENT_KEY_UP},
    {0x3B, VINT_INPUT_EVENT_KEY_DOWN},
};
static unsigned int Num_kb_mappings = 4; // 0x01665144
// object_pool<rfg_mover,4000,1> rfg_moverpool (0x01F59580) is used here too; placed in rfg_mover.cpp.
static wchar_t Vint_localization_buffer[4096]; // 0x02BE7E00
static vint_resource_callbacks vint_resource_cb; // 0x02BF4348
static vint_localization_callbacks vint_localization_cb; // 0x02C00E40
static vint_render_callbacks Vint_render_cb; // 0x02C02C78
// vint_sound_callbacks vint_sound_cb (0x02C02E10) is used here too; placed in ui_memory_manager.cpp.
extern vint_sound_callbacks vint_sound_cb;
float Aspect_ratio; // 0x02C08098
float Pixel_aspect_ratio; // 0x02C0809C
static bool Vint_scissor_mode; // 0x02C081E6
static vint_stencil_mode Vint_stencil_mode; // 0x02C081EC
virtual_mempool* Vint_memory; // 0x02C081F0
static rl_primitive_state Vint_render_state; // 0x02C4C254

// Magic numbers: rl_primitive_state's packed bits as rfg.exe builds them: the alpha mode in bits 0-4, the stencil
// mode in bits 9-12, the scissor flag in bit 18.
static const unsigned int Prim_alpha_mask = 0x1F;
static const unsigned int Prim_stencil_mask = 0x1E00;
static const unsigned int Prim_scissor_bit = 0x40000;

static rl_draw* vint_draw()
{
    return rl_draw::get_instance();
}

static const rl_screen_info* vint_screen()
{
    return rl_render_lib::get_instance()->get_screen_info();
}

// The primitive color as bytes, (int)(f * 255.0) truncated as rfg.exe converts it.
static void vint_color_bytes(const rl_color_float& clr, unsigned char& r, unsigned char& g, unsigned char& b, unsigned char& a)
{
    r = (unsigned char)(int)(clr.red * 255.0);
    g = (unsigned char)(int)(clr.green * 255.0);
    b = (unsigned char)(int)(clr.blue * 255.0);
    a = (unsigned char)(int)(clr.alpha * 255.0);
}

static void vint_set_vert_color(rl_color& out, unsigned char r, unsigned char g, unsigned char b, unsigned char a)
{
    out.red = r;
    out.green = g;
    out.blue = b;
    out.alpha = a;
}

// 0x008DD050 (:375)
const wchar_t* get_localized_string(const char* _key)
{
    if (_key != nullptr)
    {
        const unsigned int key = keen::getCrc32LwrValue(_key);
        if (key != 0 && !s_localization_is_loading)
        {
            wchar_t* text = nullptr;
            Localization_hash.get(key, text);
            return text;
        }
    }
    return nullptr;
}

// 0x008CD350 (:388)
static const wchar_t* get_localized_string_from_hash(unsigned int _key_crc)
{
    if (_key_crc != 0 && !s_localization_is_loading)
    {
        wchar_t* text = nullptr;
        Localization_hash.get(_key_crc, text);
        return text;
    }
    return nullptr;
}

// 0x008DD090 (:394)
// The tag's hash when the string table has it, else 0.
static unsigned int get_localization_hash(const char* _key)
{
    if (_key == nullptr)
    {
        return 0;
    }
    const unsigned int key = keen::getCrc32LwrValue(_key);
    if (key != 0 && !s_localization_is_loading)
    {
        wchar_t* text = nullptr;
        Localization_hash.get(key, text);
        return text != nullptr ? key : 0;
    }
    return 0;
}

// The variant's value union (offset 8), read raw as rfg.exe's inlined accessors do (the members are private).
template<typename T>
static T vint_variant_raw(const vint_variant* v)
{
    return *(const T*)((const unsigned char*)v + 8);
}

// 0x008A1D60 (:400)
// A data value as text: "number_large"/"number_default" as an integer, "cash_default" as dollars, else by type.
const wchar_t* get_localized_data(const char* _format, const vint_variant* _data)
{
    // Magic number: 0x1000, the buffer's characters (rfg.exe's swprintf takes the count).
    const unsigned int size = 0x1000;
    if (_data == nullptr)
    {
        return nullptr;
    }
    if (_format != nullptr)
    {
        if (v_stricmp(_format, "number_large") == 0 || v_stricmp(_format, "number_default") == 0)
        {
            int value;
            switch (_data->get_type())
            {
            case VINT_PROP_TYPE_INT:
            case VINT_PROP_TYPE_UINT:
            case VINT_PROP_LAST_VALID_FOR_VARIANT:
                value = vint_variant_raw<int>(_data);
                break;
            case VINT_PROP_TYPE_FLOAT:
                value = (int)vint_variant_raw<float>(_data);
                break;
            case VINT_PROP_TYPE_BOOL:
                // Original: -(value_bool != false), so true prints -1.
                value = vint_variant_raw<bool>(_data) ? -1 : 0;
                break;
            default:
                value = 0;
                break;
            }
            swprintf(Vint_localization_buffer, size, L"%i", value);
            return Vint_localization_buffer;
        }
        if (v_stricmp(_format, "cash_default") == 0)
        {
            swprintf(Vint_localization_buffer, size, L"$%.00f", (double)(float)*_data);
            return Vint_localization_buffer;
        }
    }
    switch (_data->get_type())
    {
    case VINT_PROP_TYPE_INT:
        swprintf(Vint_localization_buffer, size, L"%i", (int)*_data);
        break;
    case VINT_PROP_TYPE_UINT:
        swprintf(Vint_localization_buffer, size, L"%u", (unsigned int)*_data);
        break;
    case VINT_PROP_TYPE_FLOAT:
        swprintf(Vint_localization_buffer, size, L"%f", (double)(float)*_data);
        break;
    case VINT_PROP_TYPE_STRING:
        copy_to_wide(Vint_localization_buffer, vint_variant_raw<const char*>(_data), size);
        break;
    case VINT_PROP_TYPE_BOOL:
        // Original bug kept: the format is always L"true" (0x013028EC), whatever the value.
        swprintf(Vint_localization_buffer, size, L"true");
        break;
    case VINT_PROP_TYPE_COLOR:
    {
        const vint_variant_color clr = (vint_variant_color)*_data;
        swprintf(Vint_localization_buffer, size, L"%f, %f, %f", (double)clr.r, (double)clr.g, (double)clr.b);
        break;
    }
    case VINT_PROP_TYPE_VECTOR2F:
    {
        const vector2 v = (vector2)*_data;
        swprintf(Vint_localization_buffer, size, L"%f, %f", (double)v.x, (double)v.y);
        break;
    }
    default:
        Vint_localization_buffer[0] = L'\0';
        break;
    }
    return Vint_localization_buffer;
}

// 0x008A1FE0 (:474)
static bool sound_play(int _sound_handle, unsigned int& _instance_handle)
{
    return true;
}

// 0x008A1FF0 (:475)
static bool get_sound_state(unsigned int _handle, vint_sound_state& _state)
{
    return true;
}

// 0x008A2000 (:476)
static bool sound_pause(unsigned int _handle, bool _pause)
{
    return true;
}

// 0x008A2010 (:477)
static bool sound_stop(unsigned int _handle)
{
    return true;
}

// 0x008A2020 (:478)
static int sound_load(const char* _name)
{
    return 1;
}

// 0x008A2030 (:479)
static int sound_stream(const char* _name, void (*_callback)(int, bool))
{
    return 0;
}

// 0x008A2040 (:480)
static void sound_release(int _handle)
{
}

// 0x008A2050 (:481)
static bool sound_get_duration(int _handle, float& _duration)
{
    return false;
}

// 0x008A2060 (:485)
static int bitmap_load(const char* _name)
{
    if (_name == nullptr)
    {
        return -1;
    }
    char filename[256];
    cf_add_ext(filename, sizeof(filename), _name, ".tga", false);
    return bm_load(filename, false);
}

// 0x008A20E0 (:498)
static int bitmap_stream(const char* _name, void (*_callback)(int, bool))
{
    if (_name == nullptr || _callback == nullptr)
    {
        return -1;
    }
    char filename[256];
    cf_add_ext(filename, sizeof(filename), _name, ".tga", false);
    const int handle = bm_load(filename, false);
    if (handle != -1)
    {
        _callback(handle, true);
    }
    return handle;
}

// 0x008A2180 (:517)
static void bitmap_release(int _handle)
{
}

// 0x008A2190 (:526)
static bool bitmap_get_dimensions(int& _w, int& _h, float& _scale_x, float& _scale_y, int _handle)
{
    if (_handle == -1)
    {
        return false;
    }
    int mip_levels;
    rl_texture_get_dimensions(_handle, _w, _h, _scale_x, _scale_y, mip_levels);
    return true;
}

// 0x008A21D0 (:537)
static const char* bitmap_get_name(unsigned int _handle)
{
    if (_handle == 0xFFFFFFFF)
    {
        return nullptr;
    }
    return bm_get_filename((int)_handle);
}

// 0x008A21F0 (:544)
static bool bitmap_is_valid(unsigned int _handle)
{
    return bm_get_entry((int)_handle, true) != nullptr;
}

// 0x008A2210 (:550)
static int font_load(const char* _alias)
{
    if (_alias != nullptr)
    {
        const int font = gr_get_font_lookup_main_font(gr_get_font_lookup(_alias));
        if (font != -1)
        {
            return font;
        }
    }
    return -1;
}

// 0x008A2230 (:580)
static int font_stream(const char* _name, void (*_callback)(int, bool))
{
    if (_name == nullptr)
    {
        return -1;
    }
    const int font = gr_get_font_lookup_main_font(gr_get_font_lookup(_name));
    if (font == -1)
    {
        return -1;
    }
    _callback(font, true);
    return font;
}

// 0x008A2270 (:593)
static void font_release(int _handle)
{
}

// 0x008A2280 (:599)
static bool font_get_height(int& _height, int _handle)
{
    _height = 0;
    if (_handle == -1)
    {
        return false;
    }
    font* f = font_get_font(_handle);
    const float scale = font_masq_get_scale(_handle);
    if (f != nullptr)
    {
        _height = (int)ceil((double)((float)f->h * scale));
    }
    return f != nullptr;
}

// 0x008A2300 (:620)
static bool font_get_char_spacing(int& _w, unsigned int _char, unsigned int _next_char, int _handle)
{
    int width;
    return font_get_char_width(_char, _next_char, &width, &_w, _handle) >= 0;
}

// 0x008BA760 (:629)
// The sum of the spacings of _num_chars presentation glyphs from _start_pos.
static bool font_get_string_width(int& _width, int _handle, const wchar_t* _string, int _start_pos, int _num_chars)
{
    font* f = font_get_font(_handle);
    unsigned int previous_char = 0;
    _width = 0;
    int spacing = 0;
    const wchar_t* s = _string + _start_pos;
    if (f != nullptr)
    {
        for (int i = 0; *s != L'\0' && i < _num_chars; i++)
        {
            unsigned int c;
            s += localization_get_next_presentation_glyph_utf16(s, previous_char, c);
            previous_char = c;
            unsigned int next_c;
            localization_get_next_presentation_glyph_utf16(s, c, next_c);
            int width;
            font_get_char_width(c, next_c, &width, &spacing, _handle);
            _width += spacing;
        }
    }
    return true;
}

// 0x008A2330 (:655)
static bool font_get_leading_offset(int& _leading, int _handle)
{
    font* f = font_get_font(_handle);
    if (f != nullptr)
    {
        _leading = f->leading_offset;
        return true;
    }
    return false;
}

// 0x008A2360 (:670)
static bool font_is_reversed(int _handle, bool& reversed)
{
    font* f = font_get_font(_handle);
    if (f != nullptr)
    {
        reversed = f->is_reversed != 0;
        return true;
    }
    return false;
}

// 0x008A2390 (:684)
bool vint_rfg_font_is_reversed(int _handle)
{
    font* f = font_get_font(_handle);
    if (f != nullptr)
    {
        return f->is_reversed != 0;
    }
    return false;
}

// 0x008A23C0 (:692)
static bool font_uses_hyphens(int _handle, bool& uses_hyphens)
{
    font* f = font_get_font(_handle);
    if (f != nullptr)
    {
        uses_hyphens = f->uses_hyphens != 0;
        return true;
    }
    return false;
}

// 0x008A23F0 (:707)
// In a right-to-left font: spaces, controls, punctuation and the private use escapes have no direction; Latin, the
// registered sign and the Arabic-Indic digits read left to right; the rest is reversed.
static bool font_char_is_reversed(int m_handle, unsigned int _char, vint_char_direction& direction)
{
    direction = VINT_CHAR_DIR_NORMAL;
    font* f = font_get_font(m_handle);
    if (f == nullptr)
    {
        return false;
    }
    if (f->is_reversed != 0)
    {
        // Magic numbers: ' ' and below except '\r', '*'-'/', ';'-'@', '[', '_', '%', '~' and DEL, U+FDD0-U+FDEF.
        const bool neutral = (_char <= 0x20 && _char != 0x0D) || (_char >= 0x2A && _char <= 0x2F) ||
                             (_char >= 0x3B && _char <= 0x40) || _char == 0x5B || _char == 0x5F || _char == 0x25 ||
                             (_char >= 0x7E && _char <= 0x7F) || (_char >= 0xFDD0 && _char <= 0xFDEF);
        if (neutral)
        {
            direction = VINT_CHAR_DIR_NONE;
        }
        // Magic numbers: ASCII, U+00AE, U+0660-U+0669 (Arabic-Indic digits).
        else if (_char != 0x0D && (_char < 0x80 || _char == 0xAE || (_char >= 0x660 && _char <= 0x669)))
        {
            direction = VINT_CHAR_DIR_NORMAL;
        }
        else
        {
            direction = VINT_CHAR_DIR_REVERSED;
        }
    }
    return true;
}

// 0x008CD390 (:743)
// Glyph user data bit 0: the character cannot start a line.
static bool font_char_start_line_break(int m_handle, unsigned int _char, bool& can_start_lb)
{
    can_start_lb = true;
    font* f = font_get_font(m_handle);
    if (f != nullptr)
    {
        const unsigned int index = font_get_char_data_index(_char, (const font*)f);
        if (index != 0xFFFFFFFF && index < f->num_chars && (f->char_data[index].user_data & 1) != 0)
        {
            can_start_lb = false;
        }
    }
    return f != nullptr;
}

// 0x008CD3F0 (:761)
// Glyph user data bit 1: the character cannot end a line.
static bool font_char_end_line_break(int m_handle, unsigned int _char, bool& can_end_lb)
{
    can_end_lb = true;
    font* f = font_get_font(m_handle);
    if (f != nullptr)
    {
        const unsigned int index = font_get_char_data_index(_char, (const font*)f);
        if (index != 0xFFFFFFFF && index < f->num_chars && (f->char_data[index].user_data & 2) != 0)
        {
            can_end_lb = false;
        }
    }
    return f != nullptr;
}

// 0x008BA800 (:779)
// Vint_render_state from the render mode (alpha mode), the stencil mode and the scissor flag, made current.
static void render_update_state()
{
    unsigned int bits;
    switch (Vint_render_mode)
    {
    case VINT_RENDER_FILTER:
        bits = rl_primitive_state::FILTER.get_bits();
        break;
    case VINT_RENDER_ADDITIVE:
        bits = rl_primitive_state::ADDITIVE.get_bits();
        break;
    case VINT_RENDER_ADDITIVE_ALPHA:
        bits = rl_primitive_state::ADDITIVE_ALPHA.get_bits();
        break;
    // Magic numbers: the alpha modes 9 (subtractive), 10 (subtractive alpha), 12 (multiply) over FILTER.
    case VINT_RENDER_SUBTRACTIVE:
        bits = (rl_primitive_state::FILTER.get_bits() & ~Prim_alpha_mask) | 9;
        break;
    case VINT_RENDER_SUBTRACTIVE_ALPHA:
        bits = (rl_primitive_state::FILTER.get_bits() & ~Prim_alpha_mask) | 10;
        break;
    case VINT_RENDER_MULTIPLY:
        bits = (rl_primitive_state::FILTER.get_bits() & ~Prim_alpha_mask) | 12;
        break;
    default:
        return;
    }
    // Magic numbers: the stencil modes 0 (none), 2 (test, 0x400), 1 (write, 0x200).
    if (Vint_stencil_mode == VINT_STENCIL_DISABLED)
    {
        bits &= ~Prim_stencil_mask;
    }
    else if (Vint_stencil_mode == VINT_STENCIL_TEST)
    {
        bits = (bits & ~Prim_stencil_mask) | 0x400;
    }
    else if (Vint_stencil_mode == VINT_STENCIL_WRITE)
    {
        bits = (bits & ~Prim_stencil_mask) | 0x200;
    }
    else
    {
        return;
    }
    bits = (bits & ~Prim_scissor_bit) | (Vint_scissor_mode ? Prim_scissor_bit : 0);
    Vint_render_state.set_bits(bits);
    const rl_primitive_state state = Vint_render_state;
    vint_draw()->get_rl_state().set_macro_state(state);
}

// 0x008BA8D0 (:840)
static void render_set_mode(vint_render_mode _mode)
{
    if (_mode != Vint_render_mode)
    {
        Vint_render_mode = _mode;
        render_update_state();
    }
}

// 0x008BA8F0 (:851)
static void render_set_clip_region(const rect_i& _region, vint_res_offset _res_offset_x, vint_res_offset _res_offset_y, vint_res_scale _res_scale_x, vint_res_scale _res_scale_y)
{
    const vector2 scale = vint_project_get_scale(_res_scale_x, _res_scale_y);
    const vector2 offset = vint_project_get_offset(_res_offset_x, _res_offset_y);
    const rl_rect region((int)((float)_region.nw.x * scale.x + offset.x), (int)((float)_region.nw.y * scale.y + offset.y),
                         (int)((float)_region.se.x * scale.x + offset.x), (int)((float)_region.se.y * scale.y + offset.y));
    Vint_scissor_mode = true;
    vint_draw()->get_rl_state().set_scissor_region(region);
    render_update_state();
}

// 0x008BA9F0 (:867)
static void render_reset_clip_region()
{
    const int h = vint_screen()->get_screen_height();
    const rl_rect region(0, 0, vint_screen()->get_screen_width(), h);
    vint_draw()->get_rl_state().set_scissor_region(region);
    Vint_scissor_mode = false;
    render_update_state();
}

// 0x008BAA50 (:876)
static void render_reset_to_default()
{
    Vint_render_mode = VINT_RENDER_FILTER;
    Vint_stencil_mode = VINT_STENCIL_DISABLED;
    render_update_state();
    render_reset_clip_region();
    vint_draw()->get_rl_state().set_primitive_color(rl_color_float(1.0f, 1.0f, 1.0f, 1.0f));
}

// 0x008BAAF0 (:887)
static void render_set_stencil_mode(vint_stencil_mode _mode)
{
    if (_mode != Vint_stencil_mode)
    {
        Vint_stencil_mode = _mode;
        render_update_state();
    }
}

// 0x008BAB10 (:899)
// Clears the stencil with an untextured full-screen rect in stencil mode 5 (clear); the state is restored after.
static void render_stencil_clear()
{
    const rl_primitive_state saved = Vint_render_state;
    // Magic number: stencil mode 5 (0xA00).
    Vint_render_state.set_bits((Vint_render_state.get_bits() & ~Prim_stencil_mask) | 0xA00);
    const int w = vint_screen()->get_screen_width();
    const int h = vint_screen()->get_screen_height();
    gr_set_texture(-1);
    vint_draw()->get_rl_state().set_primitive_color(rl_color_float(0.0f, 0.0f, 0.0f, 1.0f));
    vint_draw()->rect(0.0f, 0.0f, (float)w, (float)h, Vint_render_state);
    Vint_render_state = saved;
}

// 0x008BABD0 (:926)
static void render_set_color(const color& _color)
{
    vint_draw()->get_rl_state().set_primitive_color(rl_color_float((float)_color.red / 255.0f, (float)_color.green / 255.0f,
                                                                   (float)_color.blue / 255.0f, (float)_color.alpha / 255.0f));
}

// 0x008BAC60 (:939)
static void render_get_color(color& _color)
{
    const rl_color_float& clr = vint_draw()->get_rl_state().get_primitive_color();
    vint_color_bytes(clr, _color.red, _color.green, _color.blue, _color.alpha);
}

// 0x008BACD0 (:950)
// A quad of the bitmap's source rectangle, transformed by _xform at _pos, scaled and offset into the screen.
static void render_draw_bitmap(int _handle, const matrix23& _xform, const vector2& _pos, const vector2i& _source_pos, const vector2i& _source_size, vint_res_offset _res_offset_x, vint_res_offset _res_offset_y, vint_res_scale _res_scale_x, vint_res_scale _res_scale_y)
{
    gr_set_texture(_handle);
    unsigned char r, g, b, a;
    vint_color_bytes(vint_draw()->get_rl_state().get_primitive_color(), r, g, b, a);
    if (a == 0)
    {
        return;
    }
    // Original: the dimensions stay uninitialized for handle -1 (no texture, so the UVs do not matter); zero here.
    int bm_w = 0;
    int bm_h = 0;
    float bm_scale_x = 0.0f;
    float bm_scale_y = 0.0f;
    if (_handle != -1)
    {
        int mip_levels;
        rl_texture_get_dimensions(_handle, bm_w, bm_h, bm_scale_x, bm_scale_y, mip_levels);
    }
    const float u0 = ((float)_source_pos.x / (float)bm_w) / bm_scale_x;
    const float v0 = ((float)_source_pos.y / (float)bm_h) / bm_scale_y;
    const float u1 = ((float)_source_size.x / (float)bm_w) / bm_scale_x + u0;
    const float v1 = ((float)_source_size.y / (float)bm_h) / bm_scale_y + v0;
    const vector2 scale = vint_project_get_scale(_res_scale_x, _res_scale_y);
    const vector2 offset = vint_project_get_offset(_res_offset_x, _res_offset_y);
    const float px = _pos.x;
    const float py = _pos.y;
    // The transformed width and height edges.
    const float ux = _xform.elem[0][0] * (float)_source_size.x + _xform.elem[0][1] * 0.0f + _xform.elem[0][2];
    const float uy = _xform.elem[1][0] * (float)_source_size.x + _xform.elem[1][1] * 0.0f + _xform.elem[1][2];
    const float vx = _xform.elem[0][1] * (float)_source_size.y + _xform.elem[0][0] * 0.0f + _xform.elem[0][2];
    const float vy = _xform.elem[1][1] * (float)_source_size.y + _xform.elem[1][0] * 0.0f + _xform.elem[1][2];
    vint_draw()->tristrip_2d_begin(Vint_render_state);
    rl_primitive_2d_vert* verts = vint_draw()->tristrip_2d_allocate_tris(2);
    if (verts != nullptr)
    {
        verts[0].pt.x = scale.x * px + offset.x;
        verts[0].pt.y = scale.y * py + offset.y;
        verts[0].uvs.x = u0;
        verts[0].uvs.y = v0;
        verts[1].pt.x = scale.x * (ux + px) + offset.x;
        verts[1].pt.y = scale.y * (uy + py) + offset.y;
        verts[1].uvs.x = u1;
        verts[1].uvs.y = v0;
        verts[2].pt.x = (vx + px) * scale.x + offset.x;
        verts[2].pt.y = (vy + py) * scale.y + offset.y;
        verts[2].uvs.x = u0;
        verts[2].uvs.y = v1;
        verts[3].pt.x = (vx + ux + px) * scale.x + offset.x;
        verts[3].pt.y = (vy + uy + py) * scale.y + offset.y;
        verts[3].uvs.x = u1;
        verts[3].uvs.y = v1;
        for (int i = 0; i < 4; i++)
        {
            vint_set_vert_color(verts[i].color, r, g, b, a);
        }
    }
    vint_draw()->tristrip_2d_end();
}

// 0x008BB1C0 (:1045)
// A fan of _num_wedges triangles from the bitmap's centre between the two angles, each its own tristrip.
static void render_draw_bitmap_circle(int _handle, const matrix23& _xform, const vector2& _pos, const vector2i& _source_pos, const vector2i& _source_size, float _start_angle, float _end_angle, unsigned int _num_wedges, vint_res_offset _res_offset_x, vint_res_offset _res_offset_y, vint_res_scale _res_scale_x, vint_res_scale _res_scale_y)
{
    gr_set_texture(_handle);
    const rl_color_float& clr = vint_draw()->get_rl_state().get_primitive_color();
    const float red = clr.red;
    const unsigned char g = (unsigned char)(int)(clr.green * 255.0);
    const unsigned char b = (unsigned char)(int)(clr.blue * 255.0);
    const unsigned char a = (unsigned char)(int)(clr.alpha * 255.0);
    // Original: uninitialized for handle -1, as in render_draw_bitmap.
    int bm_w = 0;
    int bm_h = 0;
    float bm_scale_x = 0.0f;
    float bm_scale_y = 0.0f;
    if (_handle != -1)
    {
        int mip_levels;
        rl_texture_get_dimensions(_handle, bm_w, bm_h, bm_scale_x, bm_scale_y, mip_levels);
    }
    const float u0 = ((float)_source_pos.x / (float)bm_w) / bm_scale_x;
    const float v0 = ((float)_source_pos.y / (float)bm_h) / bm_scale_y;
    const float du = ((float)_source_size.x / (float)bm_w) / bm_scale_x;
    const float half_w = (float)_source_size.x * 0.5f;
    const float half_h = (float)_source_size.y * 0.5f;
    const float dv = ((float)_source_size.y / (float)bm_h) / bm_scale_y;
    const vector2 scale = vint_project_get_scale(_res_scale_x, _res_scale_y);
    const vector2 offset = vint_project_get_offset(_res_offset_x, _res_offset_y);
    const float e00 = _xform.elem[0][0];
    const float e01 = _xform.elem[0][1];
    const float e02 = _xform.elem[0][2];
    const float e10 = _xform.elem[1][0];
    const float e11 = _xform.elem[1][1];
    const float uy = e10 * (float)_source_size.x + e11 * 0.0f + _xform.elem[1][2];
    // The centre. Original: the x and y scales are mixed in the half extent (x and y of both edges summed), kept.
    const float cx = (((float)_source_size.y * e01 + e00 * 0.0f + e02) * scale.y + (e00 * (float)_source_size.x + e01 * 0.0f + e02) * scale.x) * 0.5f * scale.x + _pos.x + offset.x;
    const float cy = (((float)_source_size.y * e11 + e10 * 0.0f + _xform.elem[1][2]) * scale.y + uy * scale.x) * 0.5f * scale.y + _pos.y + offset.y;
    const float wedge = (_end_angle - _start_angle) / (float)_num_wedges;
    // The angle is a running float sum: each wedge's second edge adds one wedge.
    float angle = _start_angle;
    for (unsigned int i = 0; i < _num_wedges; i++)
    {
        vint_draw()->tristrip_2d_begin(Vint_render_state);
        rl_primitive_2d_vert* verts = vint_draw()->tristrip_2d_allocate_tris(1);
        if (verts == nullptr)
        {
            vint_draw()->tristrip_2d_end();
            return;
        }
        const unsigned char r = (unsigned char)(int)(red * 255.0);
        verts[0].pt.x = cx;
        verts[0].pt.y = cy;
        verts[0].uvs.x = du * 0.5f + u0;
        verts[0].uvs.y = dv * 0.5f + v0;
        vint_set_vert_color(verts[0].color, r, g, b, a);
        for (int k = 1; k <= 2; k++)
        {
            if (k == 2)
            {
                angle += wedge;
            }
            const float c = (float)cos((double)angle);
            const float s = (float)sin((double)angle);
            // The unit vector (0.5, 0) rotated, in the bitmap's UV space and through the transform.
            const float ru = -s * 0.0f + c * 0.5f + 0.0f;
            const float rv = s * 0.5f + c * 0.0f + 0.0f;
            const float m00 = e00 * c + e01 * s;
            const float m01 = -s * e00 + e01 * c;
            const float m02 = e01 * 0.0f + e00 * 0.0f + e02;
            const float t10 = _xform.elem[1][0];
            const float t11 = _xform.elem[1][1];
            verts[k].pt.x = (m01 * 0.0f + m00 * 0.5f + m02) * 2.0f * half_w + cx;
            verts[k].pt.y = ((t10 * -s + t11 * c) * 0.0f + (t10 * c + t11 * s) * 0.5f + t10 * 0.0f + t11 * 0.0f + _xform.elem[1][2]) * 2.0f * half_h + cy;
            verts[k].uvs.x = (ru + 0.5f) * du + u0;
            verts[k].uvs.y = (rv + 0.5f) * dv + v0;
            vint_set_vert_color(verts[k].color, r, g, b, a);
        }
        vint_draw()->tristrip_2d_end();
    }
}

// 0x008CD450 (:1133)
// The text's glyphs (vint's preprocessed glyph indices) as textured quads of the font bitmap; nothing while the
// string tables load.
static void render_draw_text(int _handle, const unsigned int* _text, unsigned int _start_char, unsigned int _num_chars, const matrix23& _xform, const vector2& _pos, bool reversed, vint_res_offset _res_offset_x, vint_res_offset _res_offset_y, vint_res_scale _res_scale_x, vint_res_scale _res_scale_y)
{
    if (s_localization_is_loading)
    {
        return;
    }
    font* f = font_get_font(_handle);
    if (f == nullptr || _text == nullptr || *_text == 0)
    {
        return;
    }
    const vector2 scale = vint_project_get_scale(_res_scale_x, _res_scale_y);
    const vector2 offset = vint_project_get_offset(_res_offset_x, _res_offset_y);
    const unsigned int* p = _text + _start_char;
    float x = _pos.x * scale.x + offset.x;
    float y = _pos.y * scale.y + offset.y;
    const float render_h = (float)f->render_h;
    gr_set_texture(f->bitmap_id);
    unsigned char r, g, b, a;
    vint_color_bytes(vint_draw()->get_rl_state().get_primitive_color(), r, g, b, a);
    if (a == 0)
    {
        return;
    }
    int bm_w;
    int bm_h;
    float bm_scale_x;
    float bm_scale_y;
    bm_get_dimensions(f->bitmap_id, bm_w, bm_h, bm_scale_x, bm_scale_y);
    const float half_u = 0.5f / (float)bm_w;
    const float half_v = 0.5f / (float)bm_h;
    const float masq_scale = font_masq_get_scale(_handle);
    const float inv_masq_scale = 1.0f / masq_scale;
    // The transformed unit x and y axes, scaled to the screen.
    const float ax = (_xform.elem[0][1] * 0.0f + _xform.elem[0][0] + _xform.elem[0][2]) * scale.x;
    const float ay = (_xform.elem[1][1] * 0.0f + _xform.elem[1][0] + _xform.elem[1][2]) * scale.x;
    const float line_h = masq_scale * render_h;
    const float bx = (_xform.elem[0][0] * 0.0f + _xform.elem[0][1] + _xform.elem[0][2]) * scale.y;
    const float by = (_xform.elem[1][0] * 0.0f + _xform.elem[1][1] + _xform.elem[1][2]) * scale.y;
    rl_primitive_renderer::get_instance()->begin_primitive(RLPRT_TEXTURED_TRISTRIP_2D, Vint_render_state);
    unsigned int c = *p;
    unsigned int count = 0;
    while (c != 0 && (_num_chars == 0 || count < _num_chars))
    {
        p++;
        count++;
        // Magic number: glyphs below ' ' draw nothing.
        if (c != 0xFFFFFFFF && f->char_data[c].unicode_code_point > 0x1F)
        {
            int width;
            int spacing;
            const unsigned int index = font_get_char_width_preprocessed(c, *p, &width, &spacing, _handle);
            const float u0 = (float)f->bm_u[index] / (float)bm_w + half_u;
            const float v0 = (float)f->bm_v[index] / (float)bm_h + half_v;
            const float u1 = (((float)width * inv_masq_scale) / (float)bm_w + u0) - half_u;
            const float v1 = (render_h / (float)bm_h + v0) - half_v;
            if (reversed)
            {
                x -= (float)width * ax;
            }
            rl_primitive_2d_vert* verts = (rl_primitive_2d_vert*)rl_primitive_renderer::get_instance()->allocate_verts(4, vint_draw()->get_rl_state());
            if (verts == nullptr)
            {
                // Original: ends with rl_draw's tristrip end, not the primitive renderer's.
                vint_draw()->tristrip_2d_end();
                return;
            }
            const float bot_x = bx * line_h + x;
            const float bot_y = by * line_h + y;
            verts[0].pt.x = (float)width * ax + x;
            verts[0].pt.y = (float)width * ay + y;
            verts[0].uvs.x = u1;
            verts[0].uvs.y = v0;
            verts[1].pt.x = (float)width * ax + bot_x;
            verts[1].pt.y = (float)width * ay + bot_y;
            verts[1].uvs.x = u1;
            verts[1].uvs.y = v1;
            verts[2].pt.x = x;
            verts[2].pt.y = y;
            verts[2].uvs.x = u0;
            verts[2].uvs.y = v0;
            verts[3].pt.x = bot_x;
            verts[3].pt.y = bot_y;
            verts[3].uvs.x = u0;
            verts[3].uvs.y = v1;
            for (int i = 0; i < 4; i++)
            {
                vint_set_vert_color(verts[i].color, r, g, b, a);
            }
            float advance;
            if (reversed)
            {
                advance = (float)width - (float)spacing;
                y -= ay * (float)spacing;
            }
            else
            {
                advance = (float)spacing;
                y += ay * (float)spacing;
            }
            x += advance * ax;
        }
        c = *p;
    }
    rl_primitive_renderer::get_instance()->end_primitive(vint_draw()->get_rl_state(), vint_draw()->get_renderer());
}

// 0x008BBC10 (:1275)
static void render_draw_line(const vector2& _start, const vector2& _end, float _line_thick, vint_res_offset _res_offset_x, vint_res_offset _res_offset_y, vint_res_scale _res_scale_x, vint_res_scale _res_scale_y)
{
    const vector2 scale = vint_project_get_scale(_res_scale_x, _res_scale_y);
    const vector2 offset = vint_project_get_offset(_res_offset_x, _res_offset_y);
    vint_draw()->line(_start.x * scale.x + offset.x, _start.y * scale.y + offset.y, _end.x * scale.x + offset.x,
                      _end.y * scale.y + offset.y, Vint_render_state);
}

// 0x008BBD00 (:1283)
// Original: the size is scaled and offset like a position, and the rotation is ignored.
static void render_draw_box(const vector2& _anchor, const vector2& _offset, const vector2& _size, float _rotation, vint_res_offset _res_offset_x, vint_res_offset _res_offset_y, vint_res_scale _res_scale_x, vint_res_scale _res_scale_y)
{
    const float ox = _offset.x;
    const float ax = _anchor.x;
    const float oy = _offset.y;
    const float ay = _anchor.y;
    const vector2 scale = vint_project_get_scale(_res_scale_x, _res_scale_y);
    const vector2 offset = vint_project_get_offset(_res_offset_x, _res_offset_y);
    vint_draw()->rect((float)(int)((ox + ax) * scale.x + offset.x), (float)(int)((ay + oy) * scale.y + offset.y),
                      (float)(int)(_size.x * scale.x + offset.x), (float)(int)(_size.y * scale.y + offset.y), Vint_render_state);
}

// 0x008A24B0 (:1292)
static void render_draw_video(int _handle)
{
    if (_handle != 0)
    {
        video_player_render(_handle);
    }
}

// 0x008BBE40 (:1300)
// The frame buffer element: the render target texture over the whole screen, alpha test, opaque.
static void render_draw_frame_buffer(int _handle, const vector2& _anchor, const vector2& _offset, const vector2& _size)
{
    if (_handle == 0)
    {
        return;
    }
    rl_draw* draw = vint_draw();
    draw->set_texture(_handle);
    rl_color_float clr;
    draw->get_color(clr);
    const float alpha = clr.alpha;
    draw->set_color(rl_color_float(clr.red, clr.green, clr.blue, 1.0f));
    // Magic numbers: ALPHA_TEST with bits 23-30 cleared but bit 25 (the clamp mode set to 4).
    rl_primitive_state state;
    state.set_bits((rl_primitive_state::ALPHA_TEST.get_bits() & 0x827FFFFF) | 0x2000000);
    const int h = vint_screen()->get_screen_height();
    draw->rect(0.0f, 0.0f, (float)vint_screen()->get_screen_width(), (float)h, state);
    draw->set_color(rl_color_float(clr.red, clr.green, clr.blue, alpha));
    draw->set_texture(-1);
}

// 0x008A24D0 (:1344)
// The map element: the minimap (UI9; cartographer::render is a stand-in until then).
static void render_draw_map(int _handle)
{
    Cartographer.render();
}

// 0x008A24E0 (:1350)
static void render_draw_badge(unsigned int badge, int x, int y, float alpha, float scale, vint_res_offset _res_offset_x, vint_res_offset _res_offset_y, vint_res_scale _res_scale_x, vint_res_scale _res_scale_y)
{
    const vector2 res_scale = vint_project_get_scale(_res_scale_x, _res_scale_y);
    const vector2 offset = vint_project_get_offset(_res_offset_x, _res_offset_y);
    multi_game_badge_render(badge, (int)((float)x * res_scale.x + offset.x), (int)((float)y * res_scale.y + offset.y), alpha,
                            res_scale.x * scale);
}

// 0x008DD0E0 (:1370)
// Lua on dlmalloc, the four callback sets, the Vint pool, vint itself with rfg.vint_proj at the screen size and safe
// area, the Lua state with the game's natives, rfg_ui_globals.lua and the input map.
bool vint_rfg_system_init()
{
    lua_system_init(dlrealloc, dlfree, dl_get_heap_size, dl_get_allocated_bytes);
    vint_set_memory_callbacks(&Memory_callbacks_rfg);
    vint_localization_cb.get_localized_string = get_localized_string;
    vint_localization_cb.get_localized_string_from_hash = get_localized_string_from_hash;
    vint_localization_cb.get_tag_hash = get_localization_hash;
    vint_localization_cb.get_localization_is_loading = localization_is_loading;
    vint_set_localization_callbacks(&vint_localization_cb);
    vint_sound_cb.play = sound_play;
    vint_sound_cb.get_state = get_sound_state;
    vint_sound_cb.pause = sound_pause;
    vint_sound_cb.stop = sound_stop;
    vint_set_sound_callbacks(&vint_sound_cb);
    vint_resource_cb.bitmap_load = bitmap_load;
    vint_resource_cb.bitmap_stream = bitmap_stream;
    vint_resource_cb.bitmap_release = bitmap_release;
    vint_resource_cb.bitmap_get_dimensions = bitmap_get_dimensions;
    vint_resource_cb.bitmap_get_name = bitmap_get_name;
    vint_resource_cb.bitmap_is_valid = bitmap_is_valid;
    vint_resource_cb.font_load = font_load;
    vint_resource_cb.font_stream = font_stream;
    vint_resource_cb.font_release = font_release;
    vint_resource_cb.font_get_height = font_get_height;
    vint_resource_cb.font_get_string_width = font_get_string_width;
    vint_resource_cb.font_get_char_spacing = font_get_char_spacing;
    vint_resource_cb.font_get_leading_offset = font_get_leading_offset;
    vint_resource_cb.font_is_reversed = font_is_reversed;
    vint_resource_cb.font_uses_hyphens = font_uses_hyphens;
    vint_resource_cb.font_char_is_reversed = font_char_is_reversed;
    vint_resource_cb.font_char_start_line_break = font_char_start_line_break;
    vint_resource_cb.font_char_end_line_break = font_char_end_line_break;
    vint_resource_cb.sound_load = sound_load;
    vint_resource_cb.sound_stream = sound_stream;
    vint_resource_cb.sound_release = sound_release;
    vint_resource_cb.sound_get_duration = sound_get_duration;
    vint_resource_cb.button_display_info = controls_get_display_info;
    vint_set_resource_callbacks(&vint_resource_cb);
    Vint_render_cb.set_render_mode = render_set_mode;
    Vint_render_cb.reset_to_default = render_reset_to_default;
    Vint_render_cb.set_clip_region = render_set_clip_region;
    Vint_render_cb.reset_clip_region = render_reset_clip_region;
    Vint_render_cb.set_stencil_mode = render_set_stencil_mode;
    Vint_render_cb.stencil_clear = render_stencil_clear;
    Vint_render_cb.set_color = render_set_color;
    Vint_render_cb.get_color = render_get_color;
    Vint_render_cb.draw_bitmap = render_draw_bitmap;
    Vint_render_cb.draw_bitmap_circle = render_draw_bitmap_circle;
    Vint_render_cb.draw_text = render_draw_text;
    Vint_render_cb.draw_line = render_draw_line;
    Vint_render_cb.draw_box = render_draw_box;
    Vint_render_cb.draw_video = render_draw_video;
    Vint_render_cb.draw_frame_buffer = render_draw_frame_buffer;
    Vint_render_cb.draw_map = render_draw_map;
    Vint_render_cb.draw_badge = render_draw_badge;
    vint_set_rendering_callbacks(&Vint_render_cb);
    // Magic number: alpha mode 5 in the state's low bits.
    Vint_render_state.set_bits((Vint_render_state.get_bits() & ~Prim_alpha_mask) | 5);
    Vint_memory = Common_page_pool.get_virtual_mempool("Vint", 0);
    const vector2i screen(vint_screen()->get_screen_width(), vint_screen()->get_screen_height());
    const int bottom = gr_safe_area_bottom();
    const int right = gr_safe_area_right();
    const int top = gr_safe_area_top();
    rect_i safe_area;
    safe_area.nw.x = gr_safe_area_left();
    safe_area.nw.y = top;
    safe_area.se.x = right;
    safe_area.se.y = bottom;
    Aspect_ratio = vint_screen()->get_aspect_ratio();
    char project[512] = "rfg.vint_proj";
    Pixel_aspect_ratio = 1.0f;
    const bool result = vint_system_init(project, screen, "US", Aspect_ratio, safe_area, Vint_perm_interface_mempool, *Vint_memory);
    vint_set_thread_ownership();
    vint_lua_init(rfg_lua_register_functions, Vint_temp_mempool);
    if (!lua_load_script("rfg_ui_globals.lua", vint_lua_get_state(), Vint_temp_mempool))
    {
        keen::debug::nativePrintArgs("Unable to open custom lua library");
    }
    vint_rfg_update_input_map();
    return result;
}

// 0x008A25B0 (:1486)
void vint_rfg_system_shutdown()
{
    vint_system_shutdown();
    Common_page_pool.release_virtual_mempool(Vint_memory);
}

// 0x008A25D0 (:1498)
// The button and stick maps follow the player's controller (0 when there is none).
void vint_rfg_update_input_map()
{
    const unsigned int controller = Player_controller_index != 0xFFFFFFFF ? Player_controller_index : 0;
    for (unsigned int i = 0; i < Num_button_mappings; i++)
    {
        Button_map[i].controller_id = controller;
    }
    vint_input_button_map(Button_map, Num_button_mappings);
    for (unsigned int i = 0; i < Num_joy_mappings; i++)
    {
        Joy_map[i].controller_id = controller;
    }
    vint_input_joy_map(Joy_map, Num_joy_mappings);
    vint_input_kb_map(Kb_map, Num_kb_mappings);
}

// 0x008BBF90 (:1518)
// The project's best resolution mode for the screen and the language (the current one when lang is not valid).
void vint_rfg_set_language(keen::LanguageId lang)
{
    const int h = vint_screen()->get_screen_height();
    vector2i screen;
    screen.x = vint_screen()->get_screen_width();
    keen::LanguageId language = Current_language;
    // Magic numbers: 0x31 language ids, -1 unset.
    if ((int)lang < 0x31 && lang != (keen::LanguageId)-1)
    {
        language = lang;
    }
    screen.y = h;
    vint_project_set_resolution_mode(vint_project_find_best_res_mode(screen, keen::getLanguageIdString(language)));
}
