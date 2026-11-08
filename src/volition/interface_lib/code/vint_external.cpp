// Original: volition/interface_lib/code/vint_external.cpp
//
// The library's public API (UI.md UI2): vint_system_init sets up every class (pools, factories, property tables) and
// parses the project; the game's callback sets; vint_process (data events, groups, items, then tweens) and vint_render
// (the active documents sorted by depth); the colour table (64 named colours, names in a string pool); the render
// wrappers, each a no-op without its callback; the wide text encoding of strings passed through narrow channels.
// Compiled last in interface_lib_unity.cpp (it reads the document and text factory statics). Also holds the library's
// memory globals and helpers (internal/vint_memory.h, no .cpp in rfg.pdb).

#include "volition/interface_lib/code/vint_external.h"

#include "volition/interface_lib/code/internal/vint_memory.h"
#include "volition/interface_lib/code/objects/animation/vint_object_animation.h"
#include "volition/interface_lib/code/objects/animation/vint_object_tween.h"
#include "volition/interface_lib/code/objects/drawing/vint_element_point.h"
#include "volition/interface_lib/code/objects/noninteractive/vint_element_bitmap.h"
#include "volition/interface_lib/code/objects/noninteractive/vint_element_bitmap_circle.h"
#include "volition/interface_lib/code/objects/noninteractive/vint_element_clip.h"
#include "volition/interface_lib/code/objects/noninteractive/vint_element_frame_buffer.h"
#include "volition/interface_lib/code/objects/noninteractive/vint_element_group.h"
#include "volition/interface_lib/code/objects/noninteractive/vint_element_map.h"
#include "volition/interface_lib/code/objects/noninteractive/vint_element_text.h"
#include "volition/interface_lib/code/objects/noninteractive/vint_element_video.h"
#include "volition/interface_lib/code/objects/vint_element_base.h"
#include "volition/interface_lib/code/objects/vint_object_base.h"
#include "volition/interface_lib/code/objects/vint_object_types.h"
#include "volition/interface_lib/code/resources/vint_resource_bitmap.h"
#include "volition/interface_lib/code/resources/vint_resource_font.h"
#include "volition/interface_lib/code/resources/vint_resource_sound.h"
#include "volition/interface_lib/code/vint_callback.h"
#include "volition/interface_lib/code/vint_data_event.h"
#include "volition/interface_lib/code/vint_data_group.h"
#include "volition/interface_lib/code/vint_data_item.h"
#include "volition/interface_lib/code/vint_data_responder.h"
#include "volition/interface_lib/code/vint_document.h"
#include "volition/interface_lib/code/vint_input.h"
#include "volition/interface_lib/code/vint_project.h"
#include "volition/interface_lib/code/vint_variant.h"
#include "volition/vlib/code/math/vector2.h"
#include "volition/vlib/code/os/thread.h"
#include "volition/vlib/code/util/color.h"
#include "volition/vlib/code/util/string_comp.h"
#include "volition/vlib/code/util/string_pool.h"
#include "volition/vlib/code/util/unicode_tools.h"

#include <stdlib.h>

// ---- file-private types (rfg.pdb)
// size 0x10
struct vint_color_table_entry
{
    const wchar_t* m_name; // 0x00
    vint_variant_color m_color; // 0x04
};
static_assert(sizeof(vint_color_table_entry) == 0x10, "vint_color_table_entry");

// ---- public globals
mempool_base* Vint_perm_mempool; // 0x01A48624
mempool_base* Vint_temp_mempool; // 0x01A48628
mempool* Vint_xml_mempool; // 0x01A4862C
unsigned int Vint_memory_usage; // 0x01A48630
vint_resource_callbacks* Resource_callbacks; // 0x01A486D4
vint_render_callbacks* Render_callbacks; // 0x01A486D8
vint_sound_callbacks* Sound_callbacks; // 0x01A486DC
vint_localization_callbacks* Localization_callbacks; // 0x01A486E0
vint_memory_callbacks* Memory_callbacks; // 0x01A486E4
vint_stencil_mode Current_stencil_mode; // 0x01A486EC
volatile unsigned int Vint_thread_id; // 0x01A486F0

// ---- file-static globals
static char Color_table_data[4096]; // 0x01A380F0
static vint_color_table_entry Color_table[64]; // 0x01A47798
static unsigned int Num_color_table_entries; // 0x01A486D0
void (*Doc_load_resource_callback)(vint_document*); // 0x01A486E8
static string_pool Color_table_strings(Color_table_data, 0x1000, "Vint color table strings"); // 0x01A4EFC0

// ---- internal/vint_memory.h

// 0x0045BA70 (vint_memory.h:54)
void* vint_realloc(void* _addy, unsigned int _bytes)
{
    if (Memory_callbacks != nullptr && Memory_callbacks->realloc != nullptr)
    {
        return Memory_callbacks->realloc(_addy, _bytes);
    }
    return nullptr;
}

// 0x0045BA90 (vint_memory.h:70)
void vint_free(void* _addy)
{
    if (Memory_callbacks != nullptr && Memory_callbacks->free != nullptr)
    {
        Memory_callbacks->free(_addy);
    }
}

// ---- header-inline members (vint_external.h)

// 0x0045B890 (.h:121)
vector2i::vector2i(int _x, int _y)
{
    x = _x;
    y = _y;
}

// 0x0045B8B0 (.h:122)
vector2i::vector2i()
{
}

// 0x0045B8C0 (.h:125)
void vector2i::set(int _x, int _y)
{
    x = _x;
    y = _y;
}

// 0x0045B8D0 (.h:137)
rect_i::rect_i()
{
}

// 0x0045B8E0 (.h:138)
rect_i::rect_i(int _left, int _top, int _right, int _bottom)
{
    nw.x = _left;
    nw.y = _top;
    se.x = _right;
    se.y = _bottom;
}

// ---- vint_external.cpp

// 0x004818D0 (:114)
bool vint_system_init(const char* project_fname, const vector2i& _screen_dimensions, const char* language, float _aspect_ratio, const rect_i& _safe_area, mempool_base& _static_mempool, mempool_base& _temp_mempool)
{
    // The aspect ratio and the safe area are not used. Memory_callbacks is not required.
    if (Resource_callbacks == nullptr || Render_callbacks == nullptr || Sound_callbacks == nullptr || Localization_callbacks == nullptr || &_static_mempool == nullptr)
    {
        return false;
    }
    Vint_perm_mempool = &_static_mempool;
    Vint_temp_mempool = &_temp_mempool;
    vint_callback_native::system_init();
    vint_input_system_init();
    vint_document::system_init();
    vint_data_group::system_init();
    vint_data_item::system_init();
    vint_data_event::system_init();
    vint_data_responder::system_init();
    vint_resource_font::system_init();
    vint_resource_bitmap::system_init();
    vint_object_types_system_init();
    for (int i = 0; i < 64; ++i)
    {
        Name_hash_buckets[i] = nullptr;
    }
    vint_element_base::system_init();
    vint_object_tween::system_init();
    vint_object_animation::system_init();
    vint_element_bitmap::system_init();
    vint_element_text::system_init();
    vint_element_group::system_init();
    vint_element_point::system_init();
    vint_element_clip::system_init();
    vint_element_bitmap_circle::system_init();
    vint_element_map::system_init();
    vint_element_video::system_init();
    vint_element_frame_buffer::system_init();
    vint_resource_sound::system_init();
    if (!vint_parse_project(project_fname, Vint_temp_mempool))
    {
        return false;
    }
    vint_project_set_resolution_mode(vint_project_find_best_res_mode(_screen_dimensions, language));
    vint_color_table_clear();
    return true;
}

// 0x00464CF0 (:179)
void vint_system_shutdown()
{
}

// 0x00464D00 (:198)
void vint_set_memory_callbacks(vint_memory_callbacks* _cb)
{
    Memory_callbacks = _cb;
}

// 0x00464D10 (:204)
void vint_set_thread_ownership()
{
    Vint_thread_id = get_current_thread_id();
}

// 0x00464D20 (:209)
void vint_set_doc_load_resource_callback(void (*_cb)(vint_document*))
{
    Doc_load_resource_callback = _cb;
}

// 0x00464D30 (:215)
void vint_set_resource_callbacks(vint_resource_callbacks* _cb)
{
    Resource_callbacks = _cb;
}

// 0x00464D40 (:221)
void vint_set_rendering_callbacks(vint_render_callbacks* _cb)
{
    Render_callbacks = _cb;
}

// 0x00464D50 (:227)
void vint_set_sound_callbacks(vint_sound_callbacks* _cb)
{
    Sound_callbacks = _cb;
}

// 0x00464D60 (:233)
void vint_set_localization_callbacks(vint_localization_callbacks* _cb)
{
    Localization_callbacks = _cb;
}

// 0x0047F640 (:239)
void vint_process(float dtime)
{
    // dtime is not used: the documents' time advances in vint_document::process_all.
    vint_data_event::process_all();
    vint_data_group::process_all();
    vint_data_item::process_all();
    vint_object_tween::process_all();
}

// 0x0047CDB0 (:254)
void vint_render()
{
    vint_document* docs[68];
    unsigned int num_docs = 0;
    vint_document::Elements_rendered = 0;
    vint_document* doc = Active_docs;
    while (doc != nullptr)
    {
        docs[num_docs++] = doc;
        doc = doc->document_next;
        if (doc == Active_docs)
        {
            break;
        }
    }
    qsort(docs, num_docs, sizeof(vint_document*), compare_docs_for_render);
    for (unsigned int i = 0; i < num_docs; ++i)
    {
        docs[i]->render();
    }
}

// 0x0046C2B0 (:259)
void vint_refresh_all_strings()
{
    Factory_vint_element_text.for_each_active(vint_element_text::refresh_string);
}

// 0x00464D70 (:264)
void vint_color_table_clear()
{
    Color_table_strings.clear();
    Num_color_table_entries = 0;
}

// 0x00464D90 (:270)
void vint_color_table_add(const wchar_t* _name, const vint_variant_color& _color)
{
    if (_name == nullptr || Num_color_table_entries >= 64)
    {
        return;
    }
    unsigned int length = (unsigned int)wcslen(_name);
    wchar_t* name = (wchar_t*)Color_table_strings.alloc(length * sizeof(wchar_t) + sizeof(wchar_t), 0);
    if (name == nullptr)
    {
        return;
    }
    wcscpy(name, _name);
    Color_table[Num_color_table_entries].m_name = name;
    Color_table[Num_color_table_entries].m_color = _color;
    ++Num_color_table_entries;
}

// 0x0046C2C0 (:288)
const vint_variant_color* vint_color_table_find(const char* _name, int len)
{
    if (_name == nullptr)
    {
        return nullptr;
    }
    wchar_t name[256];
    unsigned int size = (len < 1 || len > 255) ? 256 : (unsigned int)len;
    copy_to_wide(name, _name, size);
    return vint_color_table_find(name, len);
}

// 0x00464E20 (:298)
const vint_variant_color* vint_color_table_find(const wchar_t* _name, int len)
{
    if (_name == nullptr)
    {
        return nullptr;
    }
    for (unsigned int i = 0; i < Num_color_table_entries; ++i)
    {
        // Original: with len < 1 a failed full compare falls through to the length compare (with len as the count).
        if ((len < 1 && _v_stricmp(_name, Color_table[i].m_name) == 0) || _v_strnicmp(_name, Color_table[i].m_name, len) == 0)
        {
            return &Color_table[i].m_color;
        }
    }
    return nullptr;
}

// 0x00464E90 (:314)
const wchar_t* vint_get_localized_string(const char* _key)
{
    if (Localization_callbacks != nullptr && Localization_callbacks->get_localized_string != nullptr)
    {
        return Localization_callbacks->get_localized_string(_key);
    }
    return nullptr;
}

// 0x00464EB0 (:321)
const wchar_t* vint_get_localized_string_from_hash(unsigned int _key_crc)
{
    if (Localization_callbacks != nullptr && Localization_callbacks->get_localized_string_from_hash != nullptr)
    {
        return Localization_callbacks->get_localized_string_from_hash(_key_crc);
    }
    return nullptr;
}

// 0x00464ED0 (:327)
bool vint_get_localization_is_loading()
{
    if (Localization_callbacks != nullptr && Localization_callbacks->get_localization_is_loading != nullptr)
    {
        return Localization_callbacks->get_localization_is_loading();
    }
    return false;
}

// 0x00464EF0 (:333)
unsigned int vint_localization_get_tag_hash(const char* _key)
{
    if (Localization_callbacks != nullptr && Localization_callbacks->get_tag_hash != nullptr)
    {
        return Localization_callbacks->get_tag_hash(_key);
    }
    return 0;
}

// 0x00464F10 (:339)
const wchar_t* vint_get_symbol_text(const wchar_t* _symbol, unsigned int _length)
{
    if (Localization_callbacks != nullptr && Localization_callbacks->get_symbol_text != nullptr)
    {
        return Localization_callbacks->get_symbol_text(_symbol, _length);
    }
    return nullptr;
}

// 0x00464F30 (:345)
bool vint_font_get_height(int& _height, int _font_handle)
{
    if (Resource_callbacks != nullptr && Resource_callbacks->font_get_height != nullptr)
    {
        return Resource_callbacks->font_get_height(_height, _font_handle);
    }
    return false;
}

// 0x00464F50 (:352)
bool vint_font_char_start_line_break(int _font_handle, unsigned int _char, bool& _can_start_lb)
{
    if (Resource_callbacks != nullptr && Resource_callbacks->font_char_start_line_break != nullptr)
    {
        return Resource_callbacks->font_char_start_line_break(_font_handle, _char, _can_start_lb);
    }
    return false;
}

// 0x00464F70 (:359)
bool vint_font_char_end_line_break(int _font_handle, unsigned int _char, bool& _can_end_lb)
{
    if (Resource_callbacks != nullptr && Resource_callbacks->font_char_end_line_break != nullptr)
    {
        return Resource_callbacks->font_char_end_line_break(_font_handle, _char, _can_end_lb);
    }
    return false;
}

// 0x00464F90 (:366)
vint_resource_callbacks* vint_get_resource_callbacks()
{
    return Resource_callbacks;
}

// 0x0046C350 (:371)
void vint_render_set_color(const vint_variant_color& _color, float _alpha)
{
    color c((unsigned char)(int)(_color.r * 255.0f), (unsigned char)(int)(_color.g * 255.0f), (unsigned char)(int)(_color.b * 255.0f), (unsigned char)(int)(_alpha * 255.0f));
    if (Render_callbacks != nullptr && Render_callbacks->set_color != nullptr)
    {
        Render_callbacks->set_color(c);
    }
}

// 0x00464FA0 (:377)
void vint_render_set_color(const color& _color)
{
    if (Render_callbacks != nullptr && Render_callbacks->set_color != nullptr)
    {
        Render_callbacks->set_color(_color);
    }
}

// 0x0046C3D0 (:384)
void vint_render_get_color(vint_variant_color& _color, float _alpha)
{
    // Original bugs kept: the colour is not initialized when there is no get_color callback (rfg.exe: whatever ECX
    // held), the channels are divided as integers (so 0 or 1), and _alpha is not used.
    color c;
    *(unsigned int*)&c = 0;
    if (Render_callbacks != nullptr && Render_callbacks->get_color != nullptr)
    {
        Render_callbacks->get_color(c);
    }
    _color.r = (float)(c.red / 0xFF);
    _color.b = (float)(c.blue / 0xFF);
    _color.g = (float)(c.green / 0xFF);
}

// 0x00464FC0 (:395)
void vint_render_get_color(color& _color)
{
    if (Render_callbacks != nullptr && Render_callbacks->get_color != nullptr)
    {
        Render_callbacks->get_color(_color);
    }
}

// 0x00464FE0 (:402)
void vint_render_text(int _handle, const unsigned int* _text, unsigned int _first_char, unsigned int _num_chars, const matrix23& _xform, const vector2& _pos, bool reversed, vint_res_offset _res_offset_x, vint_res_offset _res_offset_y, vint_res_scale _res_scale_x, vint_res_scale _res_scale_y)
{
    if (Render_callbacks != nullptr && Render_callbacks->draw_text != nullptr)
    {
        Render_callbacks->draw_text(_handle, _text, _first_char, _num_chars, _xform, _pos, reversed, _res_offset_x, _res_offset_y, _res_scale_x, _res_scale_y);
    }
}

// 0x00465000 (:409)
void vint_render_bitmap(int _handle, const matrix23& _xform, const vector2& _pos, const vector2i& _source_pos, const vector2i& _source_size, vint_res_offset _res_offset_x, vint_res_offset _res_offset_y, vint_res_scale _res_scale_x, vint_res_scale _res_scale_y)
{
    if (Render_callbacks != nullptr && Render_callbacks->draw_bitmap != nullptr)
    {
        Render_callbacks->draw_bitmap(_handle, _xform, _pos, _source_pos, _source_size, _res_offset_x, _res_offset_y, _res_scale_x, _res_scale_y);
    }
}

// 0x00465020 (:417)
void vint_render_bitmap_circle(int _handle, const matrix23& _xform, const vector2& _pos, const vector2i& _source_pos, const vector2i& _source_size, float _start_angle, float _end_angle, unsigned int _num_wedges, vint_res_offset _res_offset_x, vint_res_offset _res_offset_y, vint_res_scale _res_scale_x, vint_res_scale _res_scale_y)
{
    if (Render_callbacks != nullptr && Render_callbacks->draw_bitmap_circle != nullptr)
    {
        Render_callbacks->draw_bitmap_circle(_handle, _xform, _pos, _source_pos, _source_size, _start_angle, _end_angle, _num_wedges, _res_offset_x, _res_offset_y, _res_scale_x, _res_scale_y);
    }
}

// 0x00465090 (:424)
void vint_render_set_clip(const rect_i& _region, vint_res_offset _res_offset_x, vint_res_offset _res_offset_y, vint_res_scale _res_scale_x, vint_res_scale _res_scale_y)
{
    if (Render_callbacks != nullptr && Render_callbacks->set_clip_region != nullptr)
    {
        Render_callbacks->set_clip_region(_region, _res_offset_x, _res_offset_y, _res_scale_x, _res_scale_y);
    }
}

// 0x004650B0 (:431)
void vint_render_reset_clip()
{
    if (Render_callbacks != nullptr && Render_callbacks->reset_clip_region != nullptr)
    {
        Render_callbacks->reset_clip_region();
    }
}

// 0x004650D0 (:438)
void vint_render_clear_stencil()
{
    if (Render_callbacks != nullptr && Render_callbacks->stencil_clear != nullptr)
    {
        Render_callbacks->stencil_clear();
    }
}

// 0x004650F0 (:445)
void vint_render_set_stencil_mode(vint_stencil_mode _mode)
{
    if (Render_callbacks != nullptr && Render_callbacks->set_stencil_mode != nullptr && _mode != Current_stencil_mode)
    {
        Render_callbacks->set_stencil_mode(_mode);
        Current_stencil_mode = _mode;
    }
}

// 0x00465120 (:455)
vint_stencil_mode vint_render_get_stencil_mode()
{
    return Current_stencil_mode;
}

// 0x00465130 (:460)
void vint_render_reset_to_default()
{
    if (Render_callbacks != nullptr && Render_callbacks->reset_to_default != nullptr)
    {
        Render_callbacks->reset_to_default();
    }
}

// 0x00465150 (:467)
void vint_render_set_mode(vint_render_mode _mode)
{
    if (Render_callbacks != nullptr && Render_callbacks->set_render_mode != nullptr)
    {
        Render_callbacks->set_render_mode(_mode);
    }
}

// 0x00465170 (:474)
void vint_render_draw_box(const vector2& _anchor, const vector2& _offset, const vector2& _size, float _rotation, vint_res_offset _res_offset_x, vint_res_offset _res_offset_y, vint_res_scale _res_scale_x, vint_res_scale _res_scale_y)
{
    if (Render_callbacks != nullptr && Render_callbacks->draw_box != nullptr)
    {
        Render_callbacks->draw_box(_anchor, _offset, _size, _rotation, _res_offset_x, _res_offset_y, _res_scale_x, _res_scale_y);
    }
}

// 0x004651C0 (:481)
void vint_render_video(int _handle)
{
    if (Render_callbacks != nullptr && Render_callbacks->draw_video != nullptr)
    {
        Render_callbacks->draw_video(_handle);
    }
}

// 0x004651E0 (:488)
void vint_render_frame_buffer(int _handle, const vector2& _anchor, const vector2& _offset, const vector2& _size)
{
    if (Render_callbacks != nullptr && Render_callbacks->draw_frame_buffer != nullptr)
    {
        Render_callbacks->draw_frame_buffer(_handle, _anchor, _offset, _size);
    }
}

// 0x00465200 (:495)
void vint_render_map(int _handle)
{
    if (Render_callbacks != nullptr && Render_callbacks->draw_map != nullptr)
    {
        Render_callbacks->draw_map(_handle);
    }
}

// 0x00465220 (:502)
void vint_render_badge(unsigned int badge, const vector2& pos, vint_res_offset _res_offset_x, vint_res_offset _res_offset_y, vint_res_scale _res_scale_x, vint_res_scale _res_scale_y, float alpha, float scale)
{
    if (Render_callbacks != nullptr && Render_callbacks->draw_badge != nullptr)
    {
        Render_callbacks->draw_badge(badge, (int)pos.x, (int)pos.y, alpha, scale, _res_offset_x, _res_offset_y, _res_scale_x, _res_scale_y);
    }
}

// 0x00465280 (:514)
void vint_encode_wide_text(char* _dst, unsigned int _dst_size, const wchar_t* _src)
{
    // 0x80, then 3 bytes per character: flags (1, +2 when the low byte is 0, +4 when the high byte is 0; a zero byte is
    // written as 0xFF), low byte, high byte; then the terminator 8, 0.
    if (_dst == nullptr || _dst_size < 4)
    {
        return;
    }
    if (_src == nullptr || *_src == L'\0')
    {
        *_dst = '\0';
        return;
    }
    *_dst = '\x80';
    char* end = _dst + (_dst_size - 1);
    char* d = _dst + 1;
    while (*_src != L'\0' && d < end)
    {
        unsigned char lo = (unsigned char)(*_src & 0xFF);
        unsigned char hi = (unsigned char)(*_src >> 8);
        d[0] = 1;
        if (lo == 0)
        {
            d[0] = 3;
            d[1] = '\xFF';
        }
        else
        {
            d[1] = (char)lo;
        }
        if (hi == 0)
        {
            d[0] += 4;
            d[2] = '\xFF';
        }
        else
        {
            d[2] = (char)hi;
        }
        ++_src;
        d += 3;
    }
    if (d >= end - 1)
    {
        end[-1] = 8;
        end[0] = '\0';
        return;
    }
    d[0] = 8;
    d[1] = '\0';
}

// 0x00465310 (:577)
bool vint_decode_wide_text(wchar_t* _dst, unsigned int _dst_size, const char* _src)
{
    if (_src == nullptr || _dst_size == 0 || _dst == nullptr || (unsigned char)*_src != 0x80)
    {
        return false;
    }
    const unsigned char* s = (const unsigned char*)(_src + 1);
    wchar_t* end = _dst + (_dst_size - 1);
    while (*s != 8 && _dst < end)
    {
        unsigned char lo = (*s & 2) == 0 ? s[1] : 0;
        unsigned char hi = (*s & 4) == 0 ? s[2] : 0;
        *_dst = (wchar_t)(lo | (hi << 8));
        s += 3;
        ++_dst;
    }
    *_dst = L'\0';
    return true;
}

// 0x0046C470 (:627)
int vint_reset_resolution(const vector2i& _screen_dimensions, const char* language)
{
    int mode = vint_project_find_best_res_mode(_screen_dimensions, language);
    if (mode < Num_resolution_modes)
    {
        Vint_current_resolution_mode = mode;
    }
    return mode;
}
