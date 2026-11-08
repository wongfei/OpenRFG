// Original: volition/interface_lib/code/vint_external.h
// Implementation: volition/interface_lib/code/vint_external.cpp
//
// The interface library's public API: vint_system_init/shutdown (the project file, screen size, language, aspect ratio,
// safe area and memory), the game's callback sets (memory, localization, sound, resources, rendering), the resolution
// scale and offset modes, render and stencil modes, thread ownership (Vint_thread_id: vint asserts that the calling
// thread owns it), and the documents, data, input and string calls the game makes. The integer point and rectangle are
// the library's own (vector2i, rect_i). Placement of the callback structs and the enums inferred.
#pragma once


class color;
class matrix23;
class mempool_base;
class vector2;
class vint_document;
struct vint_variant_color;

enum vint_res_offset
{
    VINT_RES_OFFSET_INHERIT = 0,
    VINT_RES_OFFSET_START = 1,
    VINT_RES_OFFSET_CENTER = 2,
    VINT_RES_OFFSET_END = 3,
    VINT_RES_OFFSET_NEG_START = 4,
    VINT_RES_OFFSET_NEG_START_TWICE = 5,
    VINT_RES_OFFSET_NEG_START_X = 6,
    VINT_RES_OFFSET_NEG_START_Y = 7,
    NUM_VINT_RES_OFFSET = 8,
};

enum vint_res_scale
{
    VINT_RES_SCALE_INHERIT = 0,
    VINT_RES_SCALE_MAINTAIN_RATIO = 1,
    VINT_RES_SCALE_FILL = 2,
    VINT_RES_SCALE_CUTOFF = 3,
    VINT_RES_SCALE_CUTOFF_X = 4,
    VINT_RES_SCALE_CUTOFF_Y = 5,
    NUM_VINT_RES_SCALE = 6,
};

enum vint_render_mode
{
    VINT_RENDER_DEFAULT = 0,
    VINT_RENDER_FILTER = 1,
    VINT_RENDER_ADDITIVE = 2,
    VINT_RENDER_ADDITIVE_ALPHA = 3,
    VINT_RENDER_SUBTRACTIVE = 4,
    VINT_RENDER_SUBTRACTIVE_ALPHA = 5,
    VINT_RENDER_MULTIPLY = 6,
    NUM_VINT_RENDER_MODES = 7,
};

enum vint_stencil_mode
{
    VINT_STENCIL_DISABLED = 0,
    VINT_STENCIL_TEST = 1,
    VINT_STENCIL_WRITE = 2,
    NUM_VINT_STENCIL_MODES = 3,
};

enum vint_char_direction
{
    VINT_CHAR_DIR_NORMAL = 0,
    VINT_CHAR_DIR_REVERSED = 1,
    VINT_CHAR_DIR_NONE = 2,
    NUM_VINT_CHAR_DIRECTIONS = 3,
};

enum vint_sound_state
{
    VINT_SOUND_NOT_PLAYING = 0,
    VINT_SOUND_PLAYING = 1,
    VINT_SOUND_PAUSED = 2,
    NUM_VINT_SOUND_STATES = 3,
};

// size 0x8
class vector2i
{
public:
    vector2i(); // 0x0045B8B0 (.h:122)
    vector2i(int _x, int _y); // 0x0045B890 (.h:121)
    void set(int _x, int _y); // 0x0045B8C0 (.h:125)

    int x; // 0x00
    int y; // 0x04
};

// size 0x10
class rect_i
{
public:
    rect_i(const vector2i&, const vector2i&);
    rect_i(int _left, int _top, int _right, int _bottom); // 0x0045B8E0 (.h:138)
    rect_i(); // 0x0045B8D0 (.h:137)

    vector2i nw; // 0x00
    vector2i se; // 0x08
};

// size 0x8
struct vint_memory_callbacks
{
    void* (*realloc)(void*, unsigned int); // 0x00
    void (*free)(void*); // 0x04
};

// size 0x14
struct vint_localization_callbacks
{
    const wchar_t* (*get_localized_string)(const char*); // 0x00
    const wchar_t* (*get_localized_string_from_hash)(unsigned int); // 0x04
    unsigned int (*get_tag_hash)(const char*); // 0x08
    const wchar_t* (*get_symbol_text)(const wchar_t*, unsigned int); // 0x0C
    bool (*get_localization_is_loading)(); // 0x10
};

// size 0x10
struct vint_sound_callbacks
{
    bool (*play)(int, unsigned int&); // 0x00
    bool (*get_state)(unsigned int, vint_sound_state&); // 0x04
    bool (*stop)(unsigned int); // 0x08
    bool (*pause)(unsigned int, bool); // 0x0C
};

// size 0x5C
struct vint_resource_callbacks
{
    int (*bitmap_load)(const char*); // 0x00
    int (*bitmap_stream)(const char*, void (*)(int, bool)); // 0x04
    void (*bitmap_release)(int); // 0x08
    bool (*bitmap_get_dimensions)(int&, int&, float&, float&, int); // 0x0C
    const char* (*bitmap_get_name)(unsigned int); // 0x10
    bool (*bitmap_is_valid)(unsigned int); // 0x14
    int (*font_load)(const char*); // 0x18
    int (*font_stream)(const char*, void (*)(int, bool)); // 0x1C
    void (*font_release)(int); // 0x20
    bool (*font_get_height)(int&, int); // 0x24
    bool (*font_get_char_spacing)(int&, unsigned int, unsigned int, int); // 0x28
    bool (*font_get_string_width)(int&, int, const wchar_t*, int, int); // 0x2C
    bool (*font_get_leading_offset)(int&, int); // 0x30
    bool (*font_is_reversed)(int, bool&); // 0x34
    bool (*font_uses_hyphens)(int, bool&); // 0x38
    bool (*font_char_is_reversed)(int, unsigned int, vint_char_direction&); // 0x3C
    bool (*font_char_start_line_break)(int, unsigned int, bool&); // 0x40
    bool (*font_char_end_line_break)(int, unsigned int, bool&); // 0x44
    int (*sound_load)(const char*); // 0x48
    int (*sound_stream)(const char*, void (*)(int, bool)); // 0x4C
    void (*sound_release)(int); // 0x50
    bool (*sound_get_duration)(int, float&); // 0x54
    bool (*button_display_info)(char*, char*, int, const char*); // 0x58
};

// size 0x44
struct vint_render_callbacks
{
    void (*reset_to_default)(); // 0x00
    void (*set_color)(const color&); // 0x04
    void (*get_color)(color&); // 0x08
    void (*set_stencil_mode)(vint_stencil_mode); // 0x0C
    void (*stencil_clear)(); // 0x10
    void (*set_clip_region)(const rect_i&, vint_res_offset, vint_res_offset, vint_res_scale, vint_res_scale); // 0x14
    void (*reset_clip_region)(); // 0x18
    void (*set_render_mode)(vint_render_mode); // 0x1C
    void (*draw_bitmap)(int, const matrix23&, const vector2&, const vector2i&, const vector2i&, vint_res_offset, vint_res_offset, vint_res_scale, vint_res_scale); // 0x20
    void (*draw_bitmap_circle)(int, const matrix23&, const vector2&, const vector2i&, const vector2i&, float, float, unsigned int, vint_res_offset, vint_res_offset, vint_res_scale, vint_res_scale); // 0x24
    void (*draw_text)(int, const unsigned int*, unsigned int, unsigned int, const matrix23&, const vector2&, bool, vint_res_offset, vint_res_offset, vint_res_scale, vint_res_scale); // 0x28
    void (*draw_line)(const vector2&, const vector2&, float, vint_res_offset, vint_res_offset, vint_res_scale, vint_res_scale); // 0x2C
    void (*draw_box)(const vector2&, const vector2&, const vector2&, float, vint_res_offset, vint_res_offset, vint_res_scale, vint_res_scale); // 0x30
    void (*draw_video)(int); // 0x34
    void (*draw_frame_buffer)(int, const vector2&, const vector2&, const vector2&); // 0x38
    void (*draw_map)(int); // 0x3C
    void (*draw_badge)(unsigned int, int, int, float, float, vint_res_offset, vint_res_offset, vint_res_scale, vint_res_scale); // 0x40
};

extern vint_resource_callbacks* Resource_callbacks; // 0x01A486D4
extern vint_render_callbacks* Render_callbacks; // 0x01A486D8
extern vint_sound_callbacks* Sound_callbacks; // 0x01A486DC
extern vint_localization_callbacks* Localization_callbacks; // 0x01A486E0
extern vint_memory_callbacks* Memory_callbacks; // 0x01A486E4
extern void (*Doc_load_resource_callback)(class vint_document*); // 0x01A486E8
extern vint_stencil_mode Current_stencil_mode; // 0x01A486EC
extern volatile unsigned int Vint_thread_id; // 0x01A486F0

bool vint_system_init(const char* project_fname, const vector2i& _screen_dimensions, const char* language, float _aspect_ratio, const rect_i& _safe_area, mempool_base& _static_mempool, mempool_base& _temp_mempool); // 0x004818D0 (:114)
void vint_system_shutdown(); // 0x00464CF0 (:179)
void vint_set_memory_callbacks(vint_memory_callbacks* _cb); // 0x00464D00 (:198)
void vint_set_thread_ownership(); // 0x00464D10 (:204)
void vint_set_doc_load_resource_callback(void (*_cb)(vint_document*)); // 0x00464D20 (:209)
void vint_set_resource_callbacks(vint_resource_callbacks* _cb); // 0x00464D30 (:215)
void vint_set_rendering_callbacks(vint_render_callbacks* _cb); // 0x00464D40 (:221)
void vint_set_sound_callbacks(vint_sound_callbacks* _cb); // 0x00464D50 (:227)
void vint_set_localization_callbacks(vint_localization_callbacks* _cb); // 0x00464D60 (:233)
void vint_process(float dtime); // 0x0047F640 (:239)
void vint_render(); // 0x0047CDB0 (:254)
void vint_refresh_all_strings(); // 0x0046C2B0 (:259)
void vint_color_table_clear(); // 0x00464D70 (:264)
void vint_color_table_add(const wchar_t* _name, const vint_variant_color& _color); // 0x00464D90 (:270)
const vint_variant_color* vint_color_table_find(const char* _name, int len); // 0x0046C2C0 (:288)
const vint_variant_color* vint_color_table_find(const wchar_t* _name, int len); // 0x00464E20 (:298)
const wchar_t* vint_get_localized_string(const char* _key); // 0x00464E90 (:314)
const wchar_t* vint_get_localized_string_from_hash(unsigned int _key_crc); // 0x00464EB0 (:321)
bool vint_get_localization_is_loading(); // 0x00464ED0 (:327)
unsigned int vint_localization_get_tag_hash(const char* _key); // 0x00464EF0 (:333)
const wchar_t* vint_get_symbol_text(const wchar_t* _symbol, unsigned int _length); // 0x00464F10 (:339)
bool vint_font_get_height(int& _height, int _font_handle); // 0x00464F30 (:345)
bool vint_font_char_start_line_break(int _font_handle, unsigned int _char, bool& _can_start_lb); // 0x00464F50 (:352)
bool vint_font_char_end_line_break(int _font_handle, unsigned int _char, bool& _can_end_lb); // 0x00464F70 (:359)
vint_resource_callbacks* vint_get_resource_callbacks(); // 0x00464F90 (:366)
void vint_render_set_color(const vint_variant_color& _color, float _alpha); // 0x0046C350 (:371)
void vint_render_set_color(const color& _color); // 0x00464FA0 (:377)
void vint_render_get_color(vint_variant_color& _color, float _alpha); // 0x0046C3D0 (:384)
void vint_render_get_color(color& _color); // 0x00464FC0 (:395)
void vint_render_text(int _handle, const unsigned int* _text, unsigned int _first_char, unsigned int _num_chars, const matrix23& _xform, const vector2& _pos, bool reversed, vint_res_offset _res_offset_x, vint_res_offset _res_offset_y, vint_res_scale _res_scale_x, vint_res_scale _res_scale_y); // 0x00464FE0 (:402)
void vint_render_bitmap(int _handle, const matrix23& _xform, const vector2& _pos, const vector2i& _source_pos, const vector2i& _source_size, vint_res_offset _res_offset_x, vint_res_offset _res_offset_y, vint_res_scale _res_scale_x, vint_res_scale _res_scale_y); // 0x00465000 (:409)
void vint_render_bitmap_circle(int _handle, const matrix23& _xform, const vector2& _pos, const vector2i& _source_pos, const vector2i& _source_size, float _start_angle, float _end_angle, unsigned int _num_wedges, vint_res_offset _res_offset_x, vint_res_offset _res_offset_y, vint_res_scale _res_scale_x, vint_res_scale _res_scale_y); // 0x00465020 (:417)
void vint_render_set_clip(const rect_i& _region, vint_res_offset _res_offset_x, vint_res_offset _res_offset_y, vint_res_scale _res_scale_x, vint_res_scale _res_scale_y); // 0x00465090 (:424)
void vint_render_reset_clip(); // 0x004650B0 (:431)
void vint_render_clear_stencil(); // 0x004650D0 (:438)
void vint_render_set_stencil_mode(vint_stencil_mode _mode); // 0x004650F0 (:445)
vint_stencil_mode vint_render_get_stencil_mode(); // 0x00465120 (:455)
void vint_render_reset_to_default(); // 0x00465130 (:460)
void vint_render_set_mode(vint_render_mode _mode); // 0x00465150 (:467)
void vint_render_draw_box(const vector2& _anchor, const vector2& _offset, const vector2& _size, float _rotation, vint_res_offset _res_offset_x, vint_res_offset _res_offset_y, vint_res_scale _res_scale_x, vint_res_scale _res_scale_y); // 0x00465170 (:474)
void vint_render_video(int _handle); // 0x004651C0 (:481)
void vint_render_frame_buffer(int _handle, const vector2& _anchor, const vector2& _offset, const vector2& _size); // 0x004651E0 (:488)
void vint_render_map(int _handle); // 0x00465200 (:495)
void vint_render_badge(unsigned int badge, const vector2& pos, vint_res_offset _res_offset_x, vint_res_offset _res_offset_y, vint_res_scale _res_scale_x, vint_res_scale _res_scale_y, float alpha, float scale); // 0x00465220 (:502)
void vint_encode_wide_text(char* _dst, unsigned int _dst_size, const wchar_t* _src); // 0x00465280 (:514)
bool vint_decode_wide_text(wchar_t* _dst, unsigned int _dst_size, const char* _src); // 0x00465310 (:577)
int vint_reset_resolution(const vector2i& _screen_dimensions, const char* language); // 0x0046C470 (:627)
