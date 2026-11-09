// Original: volition/rfg/code/interface/ui_document_manager.cpp
//
// UI.md UI3: whole. The document manager: vint documents loaded by name into vint_doc_info records (streamed through the
// memory manager's container pairs, synchronous, or permanent), their fades, the per-frame lists of documents to
// process and render, the control id stack (only the top control id processes and renders), the button input of the
// menus and the input id stack, the screen blur and the fade to black, ui_init, and the mouse helpers of the PC menus.
//
// Statics of rfg.exe's interface unity that live in other files here are external there (loading.h, ui_message_box.h)
// or declared below.

#include "volition/rfg/code/interface/ui_document_manager.h"

#include "keen/base/language.hpp"
#include "keen/base/string.hpp"
#include "volition/render_lib/code/gr/gr.h"
#include "volition/render_lib/code/gr/gr_font.h"
#include "volition/rfg/code/interface/localization/localization_shared.h"
#include "volition/rfg/code/rfg.h"
#include "volition/vlib/code/memory/rfg_split_mempool.h"
#include "volition/vlib/code/memory/vmemory.h"
#include "volition/vlib/code/util/vfont.h"
#include "keen/base/stringencoding.hpp"
#include "keen/base/trace.hpp"
#include "volition/interface_lib/code/bitmap_sheets.h"
#include "volition/interface_lib/code/internal/vint_memory.h"
#include "volition/interface_lib/code/objects/animation/vint_object_animation.h"
#include "volition/interface_lib/code/objects/vint_element_base.h"
#include "volition/interface_lib/code/objects/vint_object_base.h"
#include "volition/interface_lib/code/vint_data_event.h"
#include "volition/interface_lib/code/vint_data_group.h"
#include "volition/interface_lib/code/vint_data_item.h"
#include "volition/interface_lib/code/vint_document.h"
#include "volition/interface_lib/code/vint_external.h"
#include "volition/interface_lib/code/vint_project.h"
#include "volition/interface_lib/code/vint_variant.h"
#include "volition/lua_lib/code/lua_scripts.h"
#include "volition/render_lib/code/rl_draw.h"
#include "volition/render_lib/code/rl_primitive_defines.h"
#include "volition/render_lib/code/rl_primitive_state.h"
#include "volition/render_lib/code/rl_render_lib.h"
#include "volition/render_lib/code/rl_renderer_core.h"
#include "volition/render_lib/code/rl_scene.h"
#include "volition/render_lib/code/rl_scene_renderer.h"
#include "volition/render_lib/code/rl_screen_info.h"
#include "volition/rfg/code/gamewide/gamemusic.h"
#include "volition/rfg/code/control/control_gameplay.h"
#include "volition/rfg/code/cutscene/video_cutscene.h"
#include "volition/rfg/code/gamewide/game_clock.h"
#include "volition/rfg/code/gamewide/game_render.h"
#include "volition/rfg/code/gamewide/gameseq.h"
#include "volition/rfg/code/hud/fog_of_war.h"
#include "volition/rfg/code/hud/hud_heartbeat_effect.h"
#include "volition/rfg/code/hud/hud_ui_handbook.h"
#include "volition/rfg/code/hud/hud_ui_mission_briefing.h"
#include "volition/rfg/code/hud/hud_ui_weapon_cabinet.h"
#include "volition/rfg/code/hud/minimap.h"
#include "volition/rfg/code/hud/minimap_icon_info.h"
#include "volition/rfg/code/interface/loading.h"
#include "volition/rfg/code/interface/main_menu.h"
#include "volition/rfg/code/interface/ui_document_hud_missions.h"
#include "volition/rfg/code/interface/ui_document_hud_tool_tips.h"
#include "volition/rfg/code/interface/ui_document_shared.h"
#include "volition/rfg/code/interface/ui_memory_manager.h"
#include "volition/rfg/code/interface/ui_menu.h"
#include "volition/rfg/code/interface/ui_message_box.h"
#include "volition/rfg/code/interface/vint/vint_rfg.h"
#include "volition/rfg/code/interface/vint/vint_settings.h"
#include "volition/rfg/code/multi/multi_core/include/multi.h"
#include "volition/rfg/code/multi/multi_core/include/multi_core_platform.h"
#include "volition/rfg/code/objects/humans/player.h"
#include "volition/rfg/code/scripting/action_script/global_event.h"
#include "volition/rfg/code/scripting/action_script/global_event_manager.h"
#include "volition/rfg/code/video_player/video_player.h"
#include "volition/stream2/code/stream2.h"
#include "volition/vlib/code/bmpman/bmpman.h"
#include "volition/vlib/code/input/input.h"
#include "volition/vlib/code/input/mouse.h"
#include "volition/vlib/code/math/bbox2d.h"
#include "volition/vlib/code/math/crc.h"
#include "volition/vlib/code/memory/page_pool.h"
#include "volition/vlib/code/memory/virtual_mempool.h"
#include "volition/vlib/code/os/frametime.h"
#include "volition/vlib/code/os/os.h"
#include "volition/vlib/code/util/string_comp.h"
#include "volition/vlib/code/util/string_util.h"
#include "volition/vlib/code/util/unicode_tools.h"
#include "volition/vlib/code/util/color.h"
#include "volition/vlib/code/vlib_core/vlib_assert.h"

#include <math.h>

#include <string.h>

bool fonts_are_initializing; // 0x02C08091
int Debug_font = -1; // 0x02BF221C

// The font file of a language ("font_body_%s" with en_us), lower case with '_' for '-'.
static void ui_font_name(char* name, unsigned int size, const char* format, keen::LanguageId language)
{
    keen::formatString(name, size, format, keen::getLanguageIdString(language));
    keen::changeStringToLowercase(name);
    keen::replaceStringCharacters(name, '-', '_');
}

// 0x0089B4D0 (:1094)
// Reloads the fonts of the current language (en-US when unset) into Interface_font_mempool, cleared first: body,
// header and numbers, the debug font and the credits fonts. Each becomes a masquerade with its font's global scale,
// and the lookups name them for vint.
void ui_initialize_fonts()
{
    fonts_are_initializing = true;
    font_unload_all();
    gr_clear_font_lookups();
    font_masq_remove_all();
    // Magic number: 0x40, the split pool's flag for separate CPU and GPU parts.
    if ((Interface_font_mempool.get_flags() & 0x40) == 0)
    {
        Interface_font_mempool.set_thread_ownership();
        Interface_font_mempool.restore_to_mark(0);
    }
    else
    {
        Interface_font_mempool.set_thread_ownership_cpu();
        Interface_font_mempool.set_thread_ownership_gpu();
        Interface_font_mempool.restore_to_mark_cpu(0);
        Interface_font_mempool.restore_to_mark_gpu(0);
    }
    keen::LanguageId language = Current_language;
    // Magic numbers: 0x31 language ids, -1 unset.
    if ((int)Current_language > 0x30 || Current_language == (keen::LanguageId)-1)
    {
        language = keen::LanguageId_En_Us;
    }
    mempool* pool = (mempool*)&Interface_font_mempool;
    char name[256];
    ui_font_name(name, sizeof(name), "font_body_%s", language);
    Body_font = gr_init_font(name, pool, -1);
    const float body_scale = font_get_font(Body_font)->global_scale;
    ui_font_name(name, sizeof(name), "font_header_%s", language);
    Header_font = gr_init_font(name, pool, -1);
    const float header_scale = font_get_font(Header_font)->global_scale;
    ui_font_name(name, sizeof(name), "font_numbers_%s", language);
    Number_font = gr_init_font(name, pool, -1);
    const float number_scale = font_get_font(Number_font)->global_scale;
    // rfg.exe's packfiles have no "ug-debug" font: Debug_font is -1, a masquerade of the default font.
    Debug_font = gr_init_font("ug-debug", pool, -1);
    Credits_body_font = gr_init_font("font_body_credits", pool, -1);
    const float credits_body_scale = font_get_font(Credits_body_font)->global_scale;
    Credits_header_font = gr_init_font("font_header_credits", pool, -1);
    const float credits_header_scale = font_get_font(Credits_header_font)->global_scale;
    Foreign_font = Header_font;
    Latin_num_font = Body_font;
    Latin_header_font = Header_font;
    Debug_font = font_masq_add(Debug_font, 1.0f);
    Body_font = font_masq_add(Body_font, body_scale);
    Header_font = font_masq_add(Header_font, header_scale);
    Number_font = font_masq_add(Number_font, number_scale);
    Latin_num_font = font_masq_add(Latin_num_font, body_scale);
    Latin_header_font = font_masq_add(Latin_header_font, header_scale);
    Credits_body_font = font_masq_add(Credits_body_font, credits_body_scale);
    Credits_header_font = font_masq_add(Credits_header_font, credits_header_scale);
    gr_set_font_lookup("debug", Debug_font);
    gr_set_font_lookup("font_body", Body_font);
    gr_set_font_lookup("font_header", Header_font);
    gr_set_font_lookup("font_numbers", Number_font);
    gr_set_font_lookup("font_latin_numbers", Latin_num_font);
    gr_set_font_lookup("font_header_latin_numbers", Latin_header_font);
    gr_set_font_lookup("credits_body", Credits_body_font);
    gr_set_font_lookup("credits_header", Credits_header_font);
    fonts_are_initializing = false;
}

// 0x0089B8A0 (:1232)
bool ui_are_fonts_initializing()
{
    return fonts_are_initializing;
}

// 0x0089BDA0 (:3461)
// The lookup index of a font name (gr_get_font_lookup inlined), -1 when there is none.
int ui_get_font(const char* font_name)
{
    return (int)gr_get_font_lookup(font_name);
}


// ---- file-private types (rfg.pdb)
// button_info and ui_button_info are in ui_document_manager.h since UI4.
static_assert(sizeof(button_info) == 0x18, "button_info");
static_assert(sizeof(ui_button_info) == 0x31C, "ui_button_info");

// size 0x10
struct screen_blur
{
    int screen_blur_count; // 0x00
    bool pause_music; // 0x04
    bool last_screen_blur_flag; // 0x05
    bool actively_blurring; // 0x06
    bool was_actively_blurring; // 0x07
    bool paused_game; // 0x08
    float screen_blur_time; // 0x0C
};
static_assert(sizeof(screen_blur) == 0x10, "screen_blur");

// size 0x2
struct internal_flags
{
    bool has_processed; // 0x00
    bool has_rendered; // 0x01
};
static_assert(sizeof(internal_flags) == 0x2, "internal_flags");

// ---- globals
// fonts_are_initializing (0x02C08091) is defined with the fonts above.
bool Ui_unload_assets_only = true; // 0x0166411F
bool Ui_cursor_visible = true; // 0x01664FD3
Vint_settings* gVint_settings; // 0x01664B48

// ---- file-static globals
bool Input_enabled = true; // 0x01664A4A (file static in the binary; external since UI4, ui_message_box.cpp)
int Choice_last = -1; // 0x01664B38 (external: ui_document_hud_multiplayer.cpp)
static float Max_frametime_uiman = 0.5f; // 0x01664B3C
static float Mouse_highlight_max_opacity = 0.6f; // 0x01664B54
static float Mouse_highlight_min_opacity = 0.3f; // 0x01664B58
static float Mouse_highlight_anim_speed = 3.5f; // 0x01664B5C
static farray<int,16> Unloading_handles; // 0x01667198
static farray<int,16> Unload_handles; // 0x016671E8
static farray<int,16> Unload_assets_handles; // 0x01667238
static farray<int,16> Load_handles; // 0x01667288
static farray<int,16> Process_handles; // 0x016672D8
static farray<int,16> Render_handles; // 0x01667328
static unsigned int id; // 0x01668590, the last control id handed out
static screen_blur Screen_blur_info; // 0x02BE9E04
ui_button_info Button_info; // 0x02BF1E30 (file static in the binary; external since UI4)
static internal_flags Internal_flags; // 0x02C00E2C
// Global in rfg.pdb: downloadable_content.cpp marks them (dlc_interface_mark).
unsigned int Num_bitmap_sheets_data_mark; // 0x02C08084
unsigned int Num_bitmap_images_data_mark; // 0x02C08088
unsigned int Num_sheets_data_mark; // 0x02C0808C
static int8_t Vint_doc_handle_count; // 0x02C08092
static bool Initialized; // 0x02C08093
static bool Shutdown; // 0x02C08095
static bool Fade_paused; // 0x02C08096
static float Fade_alpha; // 0x02C080AC
static bool Show_hint_icons; // 0x02C080B0
static vint_doc_info_set Vint_doc_info_set; // 0x02C48A90
// The button pegs of each platform (Platform_button_names[0] is the PC's), lists at 0x01664E74.
static const char* Platform_button_names_pc[6] = {"interface-pc", "interface-ps4", "interface-xbone", "interface-switch", "interface-steam", nullptr};
static const char* Platform_button_names_ps2[2] = {"interface-ps2", nullptr};
static const char* Platform_button_names_ps3[2] = {"interface-ps3", nullptr};
static const char* Platform_button_names_xbox[2] = {"interface-xbox", nullptr};
static const char* Platform_button_names_xbox2[2] = {"interface-xbox", nullptr};
static const char* Platform_button_names_xbone[2] = {"interface-xbone", nullptr};
static const char* Platform_button_names_ps4[2] = {"interface-ps4", nullptr};
static const char* Platform_button_names_switch[3] = {"interface-switch", "interface-steam", nullptr};
static const char** Platform_button_names[8] = {Platform_button_names_pc, Platform_button_names_ps2, Platform_button_names_ps3,
    Platform_button_names_xbox, Platform_button_names_xbox2, Platform_button_names_xbone, Platform_button_names_ps4,
    Platform_button_names_switch}; // 0x01664EC8

// Statics of rfg.exe's interface unity owned by the other files of this port (external there).
extern vector2i Screen_size; // 0x0166716C, rfg_lua_actions.cpp
extern Vint_settings My_vint_project_settings; // 0x01667174, ui_document_shared.cpp
extern float Aspect_ratio; // 0x02C08098, vint_rfg.cpp
extern float Pixel_aspect_ratio; // 0x02C0809C, vint_rfg.cpp
extern virtual_mempool* Vint_memory; // 0x02C081F0, vint_rfg.cpp
extern game_sheet_info Sheets[10]; // 0x02C02CD0, ui_memory_manager.cpp
extern unsigned int Num_sheets; // 0x02C0819C, ui_memory_manager.cpp
extern int Doc_handle_main; // 0x016631AC, ui_menu.cpp
extern menu_option_types Main_menu_delayed_selection; // 0x016631C8, main_menu.cpp
extern unsigned int Current_tool_tip; // 0x02C08074, ui_document_hud.cpp
extern tool_tip_instance Tool_tip_queue[4]; // 0x02C108D0, ui_document_hud.cpp
extern bool Active; // 0x02C03754, ui_document_hud_tool_tips.cpp (the tool tip document is shown)
extern int Doc_handle_uitips; // 0x01664AC8, ui_document_hud_tool_tips.cpp
extern int cur_id; // 0x01668594, loading.cpp
extern bool min_pause; // 0x02C08090, loading.cpp
extern bool Radio_persona_names_fixed_up; // 0x02C07FEE, localization_shared.cpp
bool game_sheet_info_set(game_sheet_info* gsi, const char* name); // 0x008A0370, ui_memory_manager.cpp
// File statics of ui_message_box.cpp (UI.md UI3 ports the ones ui_init needs).
void ui_msgbox_init_variables(); // 0x008EFB90
void ui_init_utility_doc(); // 0x00908E70
void ui_init_debug_text_items(); // 0x008CC3E0
void my_selection_hint_cb(hint_type hint); // 0x008F0350

// Magic numbers: the document fade types (doc_fadein_type/doc_fadeout_type 1 alpha, 2 to/from black), the shortest
// fade (0.01 s) and the streaming timeout of a document (35 s).
static const int Fade_type_alpha = 1;
static const int Fade_type_black = 2;
static const float Min_fade_time = 0.01f;
static const unsigned int Doc_stream_timeout_ms = 35000;
// Magic numbers: the button choices of the menus (Button_info.binfo), 33 of them; index 0 and 9-32 count as presses.
static const int Num_buttons = 33;

// The documents in the pool's in-use list, in order (the iteration rfg.exe inlines everywhere).
static vint_doc_info* doc_info_first()
{
    return Vint_doc_info_set.m_doc_info_pool.get_first(0);
}

static vint_doc_info* doc_info_next(const vint_doc_info* info)
{
    return Vint_doc_info_set.m_doc_info_pool.get_next(info);
}

// The control id on top of the stack (1 when it is empty).
static unsigned int top_control_id()
{
    if (Controller_slots.size() < 1)
    {
        return 1;
    }
    return Controller_slots[Controller_slots.size() - 1];
}

// base_array::add as rfg.exe inlines it: a critical failure when the array is full.
static void handle_list_add(farray<int,16>& list, int handle)
{
    if (list.size() + 1 > list.capacity())
    {
        VLIB_CRITICAL_FAILURE();
        return;
    }
    list.add(handle);
}

// The index of a handle in a list, -1 when it is not there.
static int handle_list_find(const farray<int,16>& list, int handle)
{
    for (int i = 0; i < list.size(); i++)
    {
        if (list[i] == handle)
        {
            return i;
        }
    }
    return -1;
}

// 0x008F60B0 (:140)
// The pool's records are cleared when they are allocated.
vint_doc_info_set::vint_doc_info_set()
    : m_doc_info_pool(nullptr, &vint_doc_info::clear)
{
}

// 0x008C6DF0
vint_doc_info_set::~vint_doc_info_set()
{
}

// 0x0089AC10 (:320)
// A debug console function: writes to address 0 to test the crash handler.
void unused_dcf_Test_crash_handling()
{
    *(volatile int*)nullptr = 1;
}

// 0x0089AC20 (:328)
void unused_dcf_Test_assert_handling()
{
}

// 0x008C6E00 (:335)
void unused_dcf_Test_text_scale()
{
    ui_get_usage_character_height(L"[scale:0.5]Hello", 0);
    ui_get_usage_character_width(L"[scale:0.5]Hello", 0);
}

// 0x0089AC30 (:346)
// Copies format into result with every printf conversion replaced by its own text formatted with no argument
// (a conversion's characters run from '%' to one of "Xdisxcf", or ' '/'%'). Original: the conversions are formatted
// from the 32-byte copy with keen::formatString and no argument list; the varargs cursor (it moves 4 bytes, 8 for
// 'f') is never read.
int ui_localize_string(char* result, int max_result_len, char* format, ...)
{
    int length = 0;
    if (result == nullptr || max_result_len < 1 || format == nullptr)
    {
        return 0;
    }
    memset(result, 0, max_result_len);
    char converted[32] = {};
    const int format_len = (int)strlen(format);
    int start = -1;
    if (format_len < 1)
    {
        return 0;
    }
    char spec[32];
    for (int i = 0; i < format_len; i++)
    {
        if (max_result_len - 1 <= length)
        {
            return length;
        }
        if (start == -1)
        {
            if (format[i] == '%')
            {
                memset(spec, 0, sizeof(spec));
                spec[0] = format[i];
                start = i;
            }
            else
            {
                result[length++] = format[i];
            }
            continue;
        }
        spec[i - start] = format[i];
        const char c = format[i];
        if (c == ' ' || c == '%')
        {
            keen::formatString(converted, sizeof(converted), "%%");
        }
        else if (c == 'X' || c == 'd' || c == 'i' || c == 's' || c == 'x' || c == 'c' || c == 'f')
        {
            keen::formatString(converted, sizeof(converted), spec);
        }
        else if (c == '.' || c == 'l' || (c >= '0' && c <= '9'))
        {
            continue;
        }
        else
        {
            keen::formatString(converted, sizeof(converted), spec);
        }
        start = -1;
        if (length == 0)
        {
            strcpy(result, converted);
        }
        else
        {
            strcat(result, converted);
        }
        length = (int)strlen(result);
    }
    return length;
}

// 0x008ECD60 (:440)
static void ui_init_variables()
{
    Initialized = false;
    Shutdown = false;
    gVint_settings = &My_vint_project_settings;
    // Original: under the pool's spinlock.
    for (vint_doc_info* info = doc_info_first(); info != nullptr;)
    {
        info = Vint_doc_info_set.m_doc_info_pool.free(info);
    }
    Screen_blur_info.screen_blur_count = 0;
    Screen_blur_info.last_screen_blur_flag = false;
    Screen_blur_info.actively_blurring = false;
    Screen_blur_info.was_actively_blurring = false;
    Screen_blur_info.paused_game = false;
    Screen_blur_info.screen_blur_time = 0.0f;
    Internal_flags.has_processed = false;
    Internal_flags.has_rendered = false;
    Fade_paused = false;
    Fade_out = false;
    Fade_ref = 0;
    Fade_alpha = 0.0f;
    Screen_size.x = rl_render_lib::get_instance()->get_screen_info()->get_screen_width();
    Screen_size.y = rl_render_lib::get_instance()->get_screen_info()->get_screen_height();
}

// 0x008B40A0 (:471)
// vint's resolution mode for the screen size and language, the mouse's scale and offset from it, the documents'
// overrides again, and the cursor's centring refreshed.
void ui_update_resolution(int w, int h)
{
    keen::LanguageId language = Current_language;
    // Magic numbers: 0x31 language ids, -1 unset.
    if ((int)Current_language > 0x30 || Current_language == (keen::LanguageId)-1)
    {
        language = keen::LanguageId_En_Us;
    }
    const vector2i screen(w, h);
    vint_reset_resolution(screen, keen::getLanguageIdString(language));
    const vector2 scale = vint_project_get_scale(VINT_RES_SCALE_MAINTAIN_RATIO, VINT_RES_SCALE_MAINTAIN_RATIO);
    if (scale.x != 0.0f && scale.y != 0.0f)
    {
        Mouse_x_scale = 1.0f / scale.x;
        Mouse_y_scale = 1.0f / scale.y;
    }
    const vector2 offset = vint_project_get_offset(VINT_RES_OFFSET_START, VINT_RES_OFFSET_START);
    Mouse_x_offset = -offset.x;
    Mouse_y_offset = -offset.y;
    Aspect_ratio = rl_render_lib::get_instance()->get_screen_info()->get_aspect_ratio();
    vint_document::reload_overrides_all(Vint_temp_mempool);
    const bool visible = Cursor_visible;
    if (Cursor_visible)
    {
        Cursor_visible = false;
        mouse_auto_centering(true);
        if (Cursor_visible != visible)
        {
            Cursor_visible = visible;
            mouse_auto_centering(false);
        }
    }
}

// 0x008B41E0 (:502)
void ui_update_resolution_from_screen_size()
{
    ui_update_resolution(Screen_size.x, Screen_size.y);
}

// The bitmap name of an [image:name] tag (narrowed, at most 63 characters, up to ']').
static int ui_image_tag_bitmap(const wchar_t* source_string)
{
    char name[64];
    unsigned int i = 0;
    while (i < 0x3F && source_string[i] != L']' && source_string[i] != L'\0')
    {
        name[i] = (char)source_string[i];
        i++;
    }
    name[i] = '\0';
    return bms_load(name);
}

// 0x0089AF70 (:508)
// The scaled width of an [image:] tag's bitmap.
static int ui_get_image_width(const wchar_t* source_string)
{
    int w = 0;
    float scale_x = 1.0f;
    if (source_string != nullptr)
    {
        const int image_id = ui_image_tag_bitmap(source_string);
        if (image_id != -1)
        {
            int h;
            float scale_y;
            bms_get_dimensions(image_id, w, h, scale_x, scale_y);
        }
    }
    return (int)((float)w * scale_x);
}

// 0x0089B060 (:543)
static int ui_get_image_height(const wchar_t* source_string)
{
    int h = 0;
    float scale_y = 1.0f;
    if (source_string != nullptr)
    {
        const int image_id = ui_image_tag_bitmap(source_string);
        if (image_id != -1)
        {
            int w;
            float scale_x;
            bms_get_dimensions(image_id, w, h, scale_x, scale_y);
        }
    }
    return (int)((float)h * scale_y);
}

// 0x0089B150 (:578)
// The control actions of the 33 menu buttons (button_choices order), all released and waiting for a release.
static void ui_init_input_info()
{
    // The control_button_action of each button_choices value.
    static const signed char choices[Num_buttons] = {
        ':', '=', '>', '?', 'A', 'C', 'D', 'E', 'F', 'G', 'I', 'J', 'K', 'L', 'M', 'N', 'O',
        'P', 'Q', 'R', ';', '<', '@', 'B', '4', '6', '2', '8', 'Y', 'V', 'Z', '[', '\\',
    };
    Button_info.m_received_input = false;
    Button_info.m_button_pressed = false;
    for (int i = 0; i < Num_buttons; i++)
    {
        Button_info.binfo[i].m_button_choice = choices[i];
        Button_info.binfo[i].m_button_input = 0.0f;
        Button_info.binfo[i].m_controller_id = -1;
        Button_info.binfo[i].m_button_actual_input = 0.0f;
        Button_info.binfo[i].m_button_repeat_time = 0.0f;
        Button_info.binfo[i].m_button_beyond_init = false;
        Button_info.binfo[i].m_button_wait_for_up = true;
    }
}

// 0x0089B3A0 (:703)
// A menu action's input, the alternate directions swapped by the player's stick inversion.
float ui_get_menu_action_input(control_button_action choice, unsigned int controller_id)
{
    switch (choice)
    {
    case CBA_MENU_ALT_UP:
        if (input_get_invert_y(controller_id, 1))
        {
            choice = CBA_MENU_ALT_DOWN;
        }
        break;
    case CBA_MENU_ALT_DOWN:
        if (input_get_invert_y(controller_id, 1))
        {
            choice = CBA_MENU_ALT_UP;
        }
        break;
    case CBA_MENU_ALT_LEFT:
        if (input_get_invert_x(controller_id, 1))
        {
            choice = CBA_MENU_ALT_RIGHT;
        }
        break;
    case CBA_MENU_ALT_RIGHT:
        if (input_get_invert_x(controller_id, 1))
        {
            choice = CBA_MENU_ALT_LEFT;
        }
        break;
    default:
        return controls_is_button_down(choice, controller_id) ? 1.0f : 0.0f;
    }
    return (float)controls_is_button_down(choice, controller_id);
}

// 0x008B4200 (:752)
// Each button's input for the menus: in the boot, the main menu and its option states any controller counts (the
// first found), else the player's controller; a held button repeats after 0.4 s every 0.1 s; a button that is held
// when it starts waiting for its release reads 0. Only the input id on top (or the locked one) gets input.
static void ui_process_input()
{
    Button_info.m_received_input = false;
    Button_info.m_button_pressed = false;
    bool any_controller = false;
    switch (gameseq_get_state())
    {
    case GS_MAINMENU:
        any_controller = Main_menu_delayed_selection == MENU_OPTION_NONE;
        break;
    case GS_MAINMENU_OPTIONS:
        // Original: the state is read again, so GS_MAINMENU_IDLE cannot match here.
        if (gameseq_get_state() == GS_MAINMENU_IDLE || Doc_handle_main == -1)
        {
            any_controller = false;
            break;
        }
    case GS_BOOT:
    case GS_MAINMENU_OPTIONS_EXTRAS:
    case GS_MAINMENU_IDLE:
        any_controller = true;
        break;
    default:
        break;
    }
    int input_index = Current_input_lock_index;
    if (Current_input_lock_index == -1)
    {
        input_index = Input_types.size() - 1;
    }
    if (!Input_enabled)
    {
        return;
    }
    bool clear_last = false;
    for (int i = 0; i < Num_buttons; i++)
    {
        button_info& b = Button_info.binfo[i];
        float repeat_time = b.m_button_repeat_time;
        float actual = 0.0f;
        float input = 0.0f;
        if (input_index < 0 || Input_types[input_index].type == IRT_UNLIMITED_INPUT)
        {
            if (any_controller)
            {
                // Magic number: one controller on PC.
                for (int controller = 0; controller < 1; controller++)
                {
                    actual = (float)controls_is_button_down((control_button_action)b.m_button_choice, controller);
                    if (fabsf(actual) >= 0.0001f)
                    {
                        b.m_controller_id = (int)(signed char)controller;
                        break;
                    }
                }
            }
            else
            {
                actual = (float)controls_is_button_down((control_button_action)b.m_button_choice, Player_controller_index);
            }
            if (b.m_button_wait_for_up != true || fabsf(actual) < 0.0001f)
            {
                b.m_button_wait_for_up = false;
            }
            else
            {
                actual = 0.0f;
            }
            input = actual;
            if (fabsf(b.m_button_actual_input - actual) > 0.0001f)
            {
                // A change: the repeat starts again.
                repeat_time = 0.0f;
                b.m_button_beyond_init = false;
            }
            else
            {
                repeat_time = Frametime_real + repeat_time;
                // Magic numbers: the repeat starts after 0.4 s, then every 0.1 s (0.5 - 0.1 back).
                if (repeat_time <= 0.5f)
                {
                    input = 0.0f;
                    if (repeat_time > 0.4f)
                    {
                        input = b.m_button_beyond_init ? 0.0f : actual;
                        b.m_button_beyond_init = true;
                    }
                    else
                    {
                        b.m_button_beyond_init = false;
                    }
                }
                else
                {
                    repeat_time = repeat_time - 0.099999994f;
                }
            }
            if (input_index < 0)
            {
                if (fabsf(input) >= 0.0001f)
                {
                    Choice_last = i;
                }
                if (Choice_last == i && actual > -0.0001f && actual < 0.0001f)
                {
                    clear_last = true;
                }
            }
        }
        b.m_button_input = input;
        b.m_button_actual_input = actual;
        b.m_button_repeat_time = repeat_time;
        Button_info.m_received_input = Button_info.m_received_input || fabsf(input) > 0.0f;
        if (i == 0 || (i > 8 && i < Num_buttons))
        {
            Button_info.m_button_pressed = Button_info.m_button_pressed || fabsf(input) > 0.0f;
        }
    }
    if (clear_last)
    {
        Choice_last = -1;
    }
}

// Appends a copy of a name to a settings array (a critical failure when it is full).
static void settings_add_name(base_array<char*>& names, const char* name)
{
    const unsigned int size = (unsigned int)strlen(name) + 1;
    char* copy = new char[size];
    if (copy == nullptr)
    {
        return;
    }
    keen::formatString(copy, size, name);
    if (names.size() + 1 > names.capacity())
    {
        VLIB_CRITICAL_FAILURE();
        return;
    }
    names.add(copy);
}

// 0x008C6E20 (:970)
// The project's include pegs (without the platform button pegs), then the platform button pegs. Original bug kept:
// the second list is skipped only when the peg after the project's last ("interface-pc") is that name.
static void ui_obtain_bmp_peg_info()
{
    const int num_pegs = (int)vint_project_settings_get_num_bmp_pegs();
    int last = 0;
    for (int i = 0; i < num_pegs; i++)
    {
        const char* peg = Vint_project_settings.peg_name[i];
        bool is_button_peg = false;
        for (const char* const* button = Platform_button_names[0]; *button != nullptr; button++)
        {
            if (v_stricmp(peg, *button) == 0)
            {
                is_button_peg = true;
                break;
            }
        }
        if (!is_button_peg)
        {
            settings_add_name(My_vint_project_settings.include_bmp_pegs, peg);
        }
        last = num_pegs;
    }
    for (const char* const* button = Platform_button_names[0]; *button != nullptr; button++)
    {
        if (v_stricmp(Vint_project_settings.peg_name[last], "interface-pc") != 0)
        {
            settings_add_name(My_vint_project_settings.include_bmp_pegs, *button);
        }
    }
}

// 0x008C6FB0 (:1022)
// Original bug kept: the project's first sheet peg is added once per sheet.
static void ui_obtain_bmp_sheet_info()
{
    for (int i = (int)vint_project_settings_get_num_bmp_sheets(); i > 0; i--)
    {
        settings_add_name(My_vint_project_settings.include_bmp_sheets, Vint_project_settings.bmp_sheet_peg_name);
    }
}

// 0x008C7040 (:1041)
// LTCG: the handle in EDI. The first document record of the handle.
static vint_doc_info* ui_get_vint_doc_info(int handle)
{
    for (vint_doc_info* info = doc_info_first(); info != nullptr; info = doc_info_next(info))
    {
        if (info->m_handle == handle)
        {
            return info;
        }
    }
    return nullptr;
}

// 0x008C70C0 (:1068)
static bool ui_obtain_project_settings()
{
    ui_obtain_bmp_peg_info();
    ui_obtain_bmp_sheet_info();
    return true;
}

// 0x008B4510 (:1077)
// The include pegs (but "startup") into the permanent interface pool, the peg extension added when missing.
static void ui_initialize_permanent_pegs()
{
    const int num = My_vint_project_settings.include_bmp_pegs.size();
    for (int i = 0; i < num; i++)
    {
        const char* peg = My_vint_project_settings.include_bmp_pegs[i];
        if (peg == nullptr || v_stricmp(peg, "startup") == 0)
        {
            continue;
        }
        char filename[128];
        _snprintf_safe(filename, sizeof(filename), peg);
        if (strstr(peg, ".") == nullptr)
        {
            // Original: the PC peg extension (Peg_platform_ext's first entry).
            strcat(filename, Peg_platform_ext[0]);
        }
        bm_load_peg(filename, &Interface_perm_mempool, true);
    }
}

// 0x008C70D0 (:1238)
// The bitmap sheets (vint's sheet tables): the sheets system in the permanent interface pool, each include sheet's
// table once (at most 10), then every sheet's bitmaps.
static void ui_initialize_bitmap_sheet_table_file()
{
    bitmap_sheets_system_init((mempool*)&Interface_perm_mempool);
    const int num = gVint_settings->include_bmp_sheets.size();
    for (int i = 0; i < num; i++)
    {
        const char* sheet = gVint_settings->include_bmp_sheets[i];
        // Original: a null name is added too (game_sheet_info_set does nothing with it but the count goes up).
        if (sheet == nullptr || mem_manager_has_sheet(sheet) == -1)
        {
            if (Num_sheets < 10)
            {
                game_sheet_info_set(&Sheets[Num_sheets], sheet);
                Num_sheets++;
            }
        }
    }
    bitmap_sheets_load_all_bitmaps(0);
}

// 0x008C71A0 (:1254)
// A DLC's sheets on top of the base ones, or (dlc_id < 0) back to the base ones.
void ui_reload_bitmat_sheet_table_file(int dlc_id)
{
    if (dlc_id < 0)
    {
        base_array<char*>& sheets = My_vint_project_settings.include_bmp_sheets;
        const int old_count = sheets.size();
        // Original bug kept: the removal shifts the next entry into i, which the loop then skips, so those names leak.
        // The bound is the old count (0x008C7277), so the walk goes past the new end and the count ends at the mark.
        for (int i = (int)Num_sheets_data_mark; i < old_count; i++)
        {
            char* const name = sheets[i];
            delete[] name;
            sheets.remove(i);
            // Deviation from rfg.exe: past the new end it deletes stale copies of a name again, a double free. Those
            // copies are cleared here so that the delete above does nothing for them.
            for (int j = i + 1; j < old_count; j++)
            {
                if (sheets[j] == name)
                {
                    sheets[j] = nullptr;
                }
            }
        }
        Num_bitmap_sheets = Num_bitmap_sheets_data_mark;
        Num_bitmap_images = Num_bitmap_images_data_mark;
        Num_sheets = Num_sheets_data_mark;
        return;
    }
    bitmap_sheets_set_thread_ownership();
    ui_obtain_bmp_sheet_info();
    const int num = gVint_settings->include_bmp_sheets.size();
    for (int i = (int)Num_sheets; i < num; i++)
    {
        if (mem_manager_has_sheet(gVint_settings->include_bmp_sheets[i]) == -1 && Num_sheets < 10)
        {
            game_sheet_info_set(&Sheets[Num_sheets], gVint_settings->include_bmp_sheets[i]);
            Num_sheets++;
        }
    }
    bitmap_sheets_load_all_bitmaps(Num_bitmap_sheets_data_mark);
}

// 0x008C72A0 (:1291)
static void ui_load_appropriate_settings_info()
{
    ui_initialize_permanent_pegs();
    ui_initialize_fonts();
    ui_initialize_bitmap_sheet_table_file();
}

// 0x008B45F0 (:1385)
// The menus' screen blur: it ramps over 0.1 s to 0.9 (or the heartbeat's blur, or 0.9 for a dead single player
// player) and pauses the game while on; the scene's frame buffer is locked while the blur is held.
static void ui_handle_screen_blur(float dtime)
{
    // Magic numbers: the 0.1 s ramp and the 0.9 blur.
    bool ramp_done = false;
    bool unpause = false;
    Screen_blur_info.actively_blurring = false;
    bool blur;
    if (Screen_blur_info.screen_blur_count < 1)
    {
        ramp_done = true;
        if (Screen_blur_info.last_screen_blur_flag)
        {
            Screen_blur_info.screen_blur_time = 0.1f;
            blur = true;
        }
        else if (Screen_blur_info.screen_blur_time <= 0.0f)
        {
            if (Screen_blur_info.was_actively_blurring || Screen_blur_info.screen_blur_time == 0.0f)
            {
                unpause = true;
            }
            blur = false;
        }
        else
        {
            blur = true;
        }
        if (blur)
        {
            Screen_blur_info.screen_blur_time -= dtime;
            if (Screen_blur_info.screen_blur_time <= 0.0f)
            {
                Screen_blur_info.screen_blur_time = 0.0f;
            }
        }
    }
    else if (Screen_blur_info.screen_blur_time >= 0.1f)
    {
        blur = false;
    }
    else
    {
        Screen_blur_info.screen_blur_time += dtime;
        if (Screen_blur_info.screen_blur_time >= 0.1f)
        {
            Screen_blur_info.screen_blur_time = 0.1f;
            ramp_done = true;
        }
        blur = true;
    }
    if (blur)
    {
        Screen_blur_info.actively_blurring = true;
        float frame_override_blur = (Screen_blur_info.screen_blur_time / 0.1f) * 0.9f;
        float heartbeat_blur = heartbeat_get_current_blur();
        if (!Multiplayer)
        {
            player* local_player = player_get_local_player();
            if (local_player != nullptr && local_player->is_killed())
            {
                heartbeat_blur = 0.9f;
            }
        }
        if (frame_override_blur < heartbeat_blur)
        {
            frame_override_blur = heartbeat_blur;
        }
        game_render_set_camera_frame_override_blur(frame_override_blur);
        if (!Screen_blur_info.paused_game)
        {
            game_pause(Screen_blur_info.pause_music);
            Screen_blur_info.paused_game = true;
        }
    }
    else if (Screen_blur_info.screen_blur_count > 0)
    {
        game_render_set_camera_frame_override_blur(0.9f);
    }
    if (ramp_done)
    {
        rl_scene* scene = rl_get_active_scene();
        if (scene != nullptr && scene->get_scene_renderer()->get_framebuffer_is_locked() != (Screen_blur_info.screen_blur_count > 0))
        {
            scene->get_scene_renderer()->lock_framebuffer(Screen_blur_info.screen_blur_count > 0);
        }
    }
    if (unpause)
    {
        if (Screen_blur_info.paused_game && game_is_paused())
        {
            game_unpause();
        }
        Screen_blur_info.paused_game = false;
    }
    Screen_blur_info.last_screen_blur_flag = Screen_blur_info.screen_blur_count > 0;
    Screen_blur_info.was_actively_blurring = Screen_blur_info.actively_blurring;
}

// Sets the root element's alpha (fade type 1) or tint (type 2, grey level) for a fade.
static void ui_doc_fade_apply(vint_element_base* root, int fade_type, float value)
{
    if (fade_type == Fade_type_alpha)
    {
        root->set_alpha(vint_variant(value), 0);
    }
    else if (fade_type == Fade_type_black)
    {
        vint_variant tint;
        tint.set_type(VINT_PROP_TYPE_COLOR);
        tint.set_color(value, value, value);
        root->set_tint(tint, 0);
    }
}

// 0x008C72B0 (:1513)
// LTCG: the handle in EDI. Ends a document's fade at its final value (alpha or tint 1 after a fade in, 0 after a
// fade out) and clears it.
static bool ui_destroy_doc_tween(int handle)
{
    vint_doc_info* info = ui_get_vint_doc_info(handle);
    if (info == nullptr || info->m_doc == nullptr)
    {
        return false;
    }
    vint_element_base* root = info->m_doc->get_root_element();
    if (info->fade_type == Fade_type_alpha || info->fade_type == Fade_type_black)
    {
        ui_doc_fade_apply(root, info->fade_type, info->is_fadein ? 1.0f : 0.0f);
    }
    info->fade_type = 0;
    info->fade_duration = 0.0f;
    info->fade_running_time = 0.0f;
    info->is_fadein = true;
    return true;
}

// 0x008B4830 (:1568)
// LTCG: the handle in ESI, posn in EDI. The handle's index in Unloading_handles.
static bool ui_is_doc_unloading(int handle, int& posn)
{
    posn = handle_list_find(Unloading_handles, handle);
    return posn != -1;
}

// 0x008ECE20 (:1583)
// LTCG: the handle in EAX/EDI. Advances a loaded document's fade by dtime; when it ends, the fade is destroyed and
// posn is the document's index in Unloading_handles (-1 otherwise).
static void ui_update_doc_fade_status(int handle, float dtime, int& posn)
{
    posn = -1;
    vint_doc_info* info = ui_get_vint_doc_info(handle);
    if (info == nullptr || info->fade_duration <= Min_fade_time || info->m_doc == nullptr)
    {
        return;
    }
    vint_element_base* root = info->m_doc->get_root_element();
    if (root == nullptr || ui_get_doc_asset_state(handle) != ST_LOADED)
    {
        return;
    }
    const float running = info->fade_running_time + dtime;
    const float duration = info->fade_duration;
    info->fade_running_time = running;
    if (duration - running <= 0.0f)
    {
        ui_destroy_doc_tween(handle);
        ui_is_doc_unloading(handle, posn);
        return;
    }
    const float t = info->is_fadein ? running / duration : 1.0f - running / duration;
    ui_doc_fade_apply(root, info->fade_type, t);
}

// 0x008ED090 (:1652)
// LTCG: vdi in EAX, fade_type in ECX, fade_time in XMM1. Starts a fade of type 1 or 2 (other types leave the fade as
// it was) and applies its first step.
static bool ui_setup_doc_tween(bool fadein, unsigned int fade_type, float fade_time, vint_doc_info* vdi)
{
    bool result = false;
    if (vdi == nullptr || fade_time <= Min_fade_time || vdi->m_doc == nullptr || vdi->m_doc->get_root_element() == nullptr)
    {
        return false;
    }
    if ((int)fade_type > 0 && (int)fade_type < 3)
    {
        vdi->is_fadein = fadein;
        result = true;
        vdi->fade_running_time = 0.0f;
        vdi->fade_duration = fade_time;
        vdi->fade_type = (int)fade_type;
    }
    int posn;
    ui_update_doc_fade_status(vdi->m_handle, 0.0f, posn);
    return result;
}

// 0x008ED110 (:1702)
// LTCG: the handle in EDI.
static bool ui_setup_doc_tween(bool fadein, unsigned int fade_type, float fade_time, int handle)
{
    vint_doc_info* info = ui_get_vint_doc_info(handle);
    if (info != nullptr)
    {
        return ui_setup_doc_tween(fadein, fade_type, fade_time, info);
    }
    return false;
}

// 0x008C73F0 (:1712)
static vint_doc_info* ui_get_doc_info(const char* name)
{
    for (vint_doc_info* info = doc_info_first(); info != nullptr; info = doc_info_next(info))
    {
        if (v_stricmp(info->m_name, name) == 0)
        {
            return info;
        }
    }
    return nullptr;
}

// 0x008B4860 (:1727)
void ui_remove_handle_from_process_rendering(int handle)
{
    const int process_index = handle_list_find(Process_handles, handle);
    if (process_index != -1)
    {
        Process_handles.remove(process_index);
    }
    const int render_index = handle_list_find(Render_handles, handle);
    if (render_index != -1)
    {
        Render_handles.remove(render_index);
    }
}

// 0x008DA860 (:1748)
// LTCG: the handle in ESI. Queues the document's containers; the record goes when they cannot be queued.
static void ui_load_document_internal(int handle)
{
    vint_doc_info* info = ui_get_vint_doc_info(handle);
    if (info != nullptr && !mem_manager_load_doc(info->peg_srid))
    {
        VLIB_CRITICAL_FAILURE();
        Vint_doc_info_set.m_doc_info_pool.free(info);
    }
}

// 0x008DA8B0 (:1761)
// LTCG: the handle in EDI. Destroys the document and its data, releases its containers (not a permanent one's) and
// its record.
static void ui_unload_document_internal(int handle)
{
    vint_doc_info* info = ui_get_vint_doc_info(handle);
    if (info == nullptr)
    {
        return;
    }
    if (info->m_doc != nullptr)
    {
        info->m_doc->destroy();
        info->release_data_groups();
        info->release_data_items();
        info->release_data_events();
    }
    if (info->m_load_type != DLT_PERMANENTLY_LOAD)
    {
        mem_manager_unload_doc(info->peg_srid);
    }
    Vint_doc_info_set.m_doc_info_pool.free(info);
    ui_remove_handle_from_process_rendering(handle);
}

// 0x008DA910 (:1800)
// LTCG: the handle in ESI.
static void ui_unload_doc_assets_internal(int handle)
{
    vint_doc_info* info = ui_get_vint_doc_info(handle);
    if (info != nullptr)
    {
        mem_manager_unload_doc(info->peg_srid);
        info->m_assets_loaded = false;
    }
}

// 0x008C7480 (:1826)
// LTCG: the handle in EDI (external for loading.cpp, see the header).
vint_doc_info* ui_get_doc_info(int handle)
{
    return ui_get_vint_doc_info(handle);
}

// 0x008C7500 (:1841)
// LTCG: the srid in EDI. The document of a peg container.
static vint_doc_info* ui_get_doc_info(unsigned int srid)
{
    for (vint_doc_info* info = doc_info_first(); info != nullptr; info = doc_info_next(info))
    {
        if (info->peg_srid == srid)
        {
            return info;
        }
    }
    return nullptr;
}

// Moves an unloading document (its index in Unloading_handles) to the unload list at once, its fade ended.
static void ui_force_unload_complete_at(int index)
{
    const int handle = Unloading_handles[index];
    ui_destroy_doc_tween(handle);
    handle_list_add(Unload_handles, handle);
    Unloading_handles.remove(index);
}

// 0x008C7580 (:1857)
// LTCG: the handle in EDI.
static void ui_force_unload_complete_on_handle(int handle)
{
    const int index = handle_list_find(Unloading_handles, handle);
    if (index != -1)
    {
        ui_force_unload_complete_at(index);
    }
}

// 0x008C7610 (:1880)
static void ui_force_unload_complete_on_handle(const char* name)
{
    if (name == nullptr)
    {
        return;
    }
    const int num = Unloading_handles.size();
    for (int i = 0; i < num; i++)
    {
        vint_doc_info* info = ui_get_vint_doc_info(Unloading_handles[i]);
        if (info != nullptr && v_stricmp(info->m_name, name) == 0)
        {
            ui_force_unload_complete_at(i);
            return;
        }
    }
}

// 0x0089B8B0 (:1914)
// The loading box's callback: the box is gone, so is the loading screen.
static bool loading_screen_cb(int mbox_handle, int selection_index, msgbox_choice choice)
{
    if (Loading_screen_handle != mbox_handle)
    {
        return false;
    }
    Loading_screen_handle = -1;
    Loading_screen_ref_cnt = 0;
    return true;
}

// 0x008C76D0 (:1939)
// Each record takes the next handle (a signed byte counter).
vint_doc_info::vint_doc_info()
{
    m_event_handles.clear();
    m_handle = (int)Vint_doc_handle_count++;
    m_doc = nullptr;
    m_name[0] = '\0';
    m_paused = false;
    m_group_handles.clear();
    m_data_handles.clear();
    is_fadein = true;
    fade_type = 0;
    fade_duration = 0.0f;
    fade_running_time = 0.0f;
    peg_srid = 0;
    m_assets_loaded = false;
    m_stream_timer.invalidate();
}

// 0x008B48F0 (:1946)
void vint_doc_info::release_data_groups()
{
    const int num = m_group_handles.size();
    for (int i = 0; i < num; i++)
    {
        vint_data_group* group = vint_data_group::find(m_group_handles[i]);
        if (group == nullptr)
        {
            VLIB_CRITICAL_FAILURE();
        }
        else
        {
            group->destroy();
        }
    }
    m_group_handles.clear();
}

// 0x008B4950 (:1964)
void vint_doc_info::release_data_items()
{
    const int num = m_data_handles.size();
    for (int i = 0; i < num; i++)
    {
        vint_data_item* item = vint_data_item::find(m_data_handles[i]);
        if (item == nullptr)
        {
            VLIB_CRITICAL_FAILURE();
        }
        else
        {
            item->destroy();
        }
    }
    m_data_handles.clear();
}

// 0x008B49C0 (:1983)
void vint_doc_info::release_data_events()
{
    const int num = m_event_handles.size();
    for (int i = 0; i < num; i++)
    {
        vint_data_event* data_event = vint_data_event::find(m_event_handles[i]);
        if (data_event == nullptr)
        {
            VLIB_CRITICAL_FAILURE();
        }
        else
        {
            data_event->destroy();
        }
    }
    m_event_handles.clear();
}

// 0x008B4A30 (:2002)
// The pool's init callback. Original: the event handles are not cleared.
void vint_doc_info::clear()
{
    m_doc = nullptr;
    m_name[0] = '\0';
    m_paused = false;
    m_group_handles.clear();
    m_data_handles.clear();
    is_fadein = true;
    fade_type = 0;
    fade_duration = 0.0f;
    fade_running_time = 0.0f;
    peg_srid = 0;
    m_assets_loaded = false;
    m_stream_timer.invalidate();
}

// 0x0089B8E0 (:2028)
// LTCG: a in EAX, new_scale in ECX. The decimal number at a ("12.5"), its end returned.
static const wchar_t* ui_parse_scale(const wchar_t* a, float& new_scale)
{
    const wchar_t* p = a;
    if (*p >= L'0' && *p <= L'9')
    {
        while (*p >= L'0' && *p <= L'9')
        {
            p++;
        }
        // The integer part from its last digit up.
        float place = 1.0f;
        new_scale = 0.0f;
        for (const wchar_t* d = p - 1; d >= a; d--)
        {
            new_scale = (float)(int)(*d - L'0') * place + new_scale;
            place = place * 10.0f;
        }
        if (*p == L'.')
        {
            p++;
            place = 0.1f;
            while (*p >= L'0' && *p <= L'9')
            {
                new_scale = (float)(int)(*p - L'0') * place + new_scale;
                place = place * 0.1f;
                p++;
            }
        }
    }
    return p;
}

// 0x008C77A0 (:2067)
int ui_get_usage_character_width(const char* input, int fontnum)
{
    wchar_t wide[512];
    // Magic number: 0x400, the buffer's bytes.
    keen::convertUTF8ToUTF16((uint16*)wide, 0x400, input);
    return ui_get_usage_character_width(wide, fontnum);
}

// 0x008B4A80 (:2074)
// The text's width in the font with vint's markup: [image:]/[image_tint:] count their bitmap's width, [color:] is
// skipped, [scale:] scales the whole width (the last one wins). Original: after a tag name the next character is
// skipped as well, and the last character is not counted when it closes a tag.
int ui_get_usage_character_width(const wchar_t* input, int fontnum)
{
    font_get_font(fontnum);
    int width = 0;
    float scale = 1.0f;
    if (input == nullptr)
    {
        return (int)((float)width * scale);
    }
    unsigned int c;
    unsigned int next_c;
    const wchar_t* s = input + localization_get_next_presentation_glyph_utf16(input, 0, c);
    unsigned int previous_c = c;
    localization_get_next_presentation_glyph_utf16(s, c, next_c);
    bool in_tag = false;
    bool was_in_tag = false;
    int char_w;
    int spacing;
    if (next_c != 0)
    {
        do
        {
            was_in_tag = in_tag;
            if (in_tag)
            {
                input = s;
                if (c == L']')
                {
                    in_tag = false;
                }
            }
            else if (c == L'[')
            {
                // Magic numbers: the tag lengths.
                const bool is_image = _v_strnicmp(input, L"[image:", 7) == 0;
                const bool is_image_tint = _v_strnicmp(input, L"[image_tint:", 12) == 0;
                if (is_image)
                {
                    const wchar_t* name = input + 7;
                    input = s + 7;
                    in_tag = true;
                    width += ui_get_image_width(name);
                }
                else if (is_image_tint)
                {
                    const wchar_t* name = input + 12;
                    input = s + 12;
                    in_tag = true;
                    width += ui_get_image_width(name);
                }
                else if (_v_strnicmp(input, L"[color:", 7) == 0)
                {
                    input = s + 7;
                    in_tag = true;
                }
                else if (_v_strnicmp(input, L"[scale:", 7) == 0)
                {
                    ui_parse_scale(input + 7, scale);
                    input = s + 7;
                    in_tag = true;
                }
                else
                {
                    font_get_char_width(c, next_c, &char_w, &spacing, fontnum);
                    width += spacing;
                    input = s;
                }
            }
            else
            {
                font_get_char_width(c, next_c, &char_w, &spacing, fontnum);
                width += spacing;
                input = s;
            }
            s = input + localization_get_next_presentation_glyph_utf16(input, previous_c, c);
            previous_c = c;
            localization_get_next_presentation_glyph_utf16(s, c, next_c);
        } while (next_c != 0);
        if (was_in_tag)
        {
            return (int)((float)width * scale);
        }
    }
    font_get_char_width(c, 0, &char_w, &spacing, fontnum);
    width += spacing;
    return (int)((float)width * scale);
}

// 0x008B4C70 (:2158)
int ui_get_usage_character_height(const char* input, int fontnum)
{
    wchar_t wide[512];
    keen::convertUTF8ToUTF16((uint16*)wide, 0x400, input);
    return ui_get_usage_character_height(wide, fontnum);
}

// 0x0089BB10 (:2165)
// The font's line height, or the tallest [image:] in the text. Original: the last character is not looked at, and
// [scale:] is parsed but not applied.
int ui_get_usage_character_height(const wchar_t* input, int fontnum)
{
    int height = 0;
    if (input != nullptr)
    {
        font* f = font_get_font(fontnum);
        if (f == nullptr)
        {
            return 0;
        }
        height = (int)((float)f->render_h * font_masq_get_scale(fontnum));
        bool in_tag = false;
        float scale = 1.0f;
        for (const wchar_t* next = input + 1; *next != L'\0'; input++, next++)
        {
            if (in_tag)
            {
                if (*input == L']')
                {
                    in_tag = false;
                }
            }
            else if (*input == L'[')
            {
                const bool is_image = _v_strnicmp(input, L"[image:", 7) == 0;
                const bool is_image_tint = _v_strnicmp(input, L"[image_tint:", 12) == 0;
                if (is_image || is_image_tint)
                {
                    const int skip = is_image ? 7 : 12;
                    input += skip;
                    next += skip;
                    in_tag = true;
                    const int image_h = ui_get_image_height(input);
                    if (height < image_h)
                    {
                        height = image_h;
                    }
                }
                else if (_v_strnicmp(input, L"[color:", 7) == 0)
                {
                    input += 7;
                    next += 7;
                    in_tag = true;
                }
                else if (_v_strnicmp(input, L"[scale:", 7) == 0)
                {
                    input += 7;
                    next += 7;
                    in_tag = true;
                    ui_parse_scale(input, scale);
                }
            }
        }
    }
    return (int)((float)height * 1.0f);
}

// 0x00910F40 (:2234)
// Once: the variables, the menus, the include peg and sheet arrays (10 each), vint and Lua (vint_rfg_system_init),
// the mouse's scale and offset, the memory manager, the include pegs and sheets, the fonts, the input, the message
// boxes, the utility, debug text and tool tips documents, the stream allocators and the minimap icons.
void ui_init()
{
    if (Initialized)
    {
        return;
    }
    ui_init_variables();
    ui_menu_init();
    // Magic numbers: 10 names each, 16-byte aligned.
    My_vint_project_settings.include_bmp_pegs.init((char**)Permanent_mempool.alloc(0x28, 0x10), 10, 0);
    My_vint_project_settings.include_bmp_sheets.init((char**)Permanent_mempool.alloc(0x28, 0x10), 10, 0);
    if (!vint_rfg_system_init())
    {
        return;
    }
    const vector2 scale = vint_project_get_scale(VINT_RES_SCALE_MAINTAIN_RATIO, VINT_RES_SCALE_MAINTAIN_RATIO);
    if (scale.x != 0.0f && scale.y != 0.0f)
    {
        Mouse_x_scale = 1.0f / scale.x;
        Mouse_y_scale = 1.0f / scale.y;
    }
    const vector2 offset = vint_project_get_offset(VINT_RES_OFFSET_START, VINT_RES_OFFSET_START);
    Mouse_x_offset = -offset.x;
    Mouse_y_offset = -offset.y;
    mem_manager_initialize();
    ui_obtain_bmp_peg_info();
    ui_obtain_bmp_sheet_info();
    ui_initialize_permanent_pegs();
    ui_initialize_fonts();
    ui_initialize_bitmap_sheet_table_file();
    ui_init_input_info();
    ui_msgbox_init_variables();
    ui_init_utility_doc();
    ui_init_debug_text_items();
    ui_tool_tips_doc_init();
    mem_manager_initialize_allocators();
    minimap_load_icon_table(-1);
    Initialized = true;
}

// 0x008ED130 (:2315)
// The documents' containers (from the packfiles when Use_packfiles), then the tool tips table.
void ui_init_stage_2()
{
    ui_document_load_shared(&Common_mempool, (mempool*)&Interface_perm_mempool, Use_packfiles, PLATFORM_PC, false, -1);
    ui_tool_tips_init_2();
}

// 0x008DA940 (:2329)
void ui_level_init_pre()
{
    hud_ui_weapon_cabinet_level_init_pre();
    notify_info notify;
    notify.data = nullptr;
    notify.callback = tool_tips_report_dialog_line_finished;
    notify.type = EVENT_DIALOGUE_FINISHED;
    gem_register_for_event(notify);
    ui_tool_tips_system_load_from_profile();
    Radio_persona_names_fixed_up = false;
}

// 0x0089BC80 (:2337)
// rfg.exe inlines hud_ui_missions_remove_objective: strncpy_safe of "" into Activity_set.m_mission_caption (0x01667014).
void ui_level_init_post()
{
    hud_ui_missions_remove_objective();
}

// 0x008DA980 (:2343)
// Between frames: in single player the game and videos pause while the window is minimized (but in gameplay);
// then the queued unloads, asset unloads and loads.
void ui_handshake()
{
    if (!Multiplayer)
    {
        const game_state state = gameseq_get_state();
        const bool minimized = os_minimized();
        if (state != GS_GAMEPLAY && minimized)
        {
            if (!min_pause)
            {
                game_pause(true);
                video_player_pause_all();
                min_pause = true;
            }
        }
        else if (min_pause && !minimized)
        {
            video_player_unpause_all();
            game_unpause();
            min_pause = false;
        }
    }
    Internal_flags.has_processed = false;
    Internal_flags.has_rendered = false;
    for (int i = 0; i < Unload_handles.size(); i++)
    {
        ui_unload_document_internal(Unload_handles[i]);
    }
    Unload_handles.clear();
    for (int i = 0; i < Unload_assets_handles.size(); i++)
    {
        ui_unload_doc_assets_internal(Unload_assets_handles[i]);
    }
    Unload_assets_handles.clear();
    for (int i = 0; i < Load_handles.size(); i++)
    {
        ui_load_document_internal(Load_handles[i]);
    }
    Load_handles.clear();
}

// 0x008F60D0 (:2394)
// Unloads the documents (at most 16), drops the input ids, shuts the message boxes, vint, the memory manager, the
// minimap and the menus down.
void ui_shutdown()
{
    if (!Initialized)
    {
        return;
    }
    Initialized = false;
    int count = 0;
    for (vint_doc_info* info = doc_info_first(); info != nullptr && count < 16; info = doc_info_next(info), count++)
    {
        if (info->m_doc != nullptr)
        {
            ui_unload_document(info->m_handle, DFOT_NONE, 0.5f);
            info->m_paused = false;
        }
    }
    Input_types.clear();
    Current_input_lock_index = -1;
    Current_input_lock_id = -1;
    ui_msgbox_shutdown();
    vint_system_shutdown();
    Common_page_pool.release_virtual_mempool(Vint_memory);
    mem_manager_shutdown();
    Cartographer.shutdown();
    ui_menu_shutdown();
    Shutdown = true;
}

// 0x008C7800 (:2448)
void ui_system_save(cfile* file)
{
    if (file != nullptr)
    {
        fog_of_war_save(file);
        Cartographer.minimap_save(file);
        ui_tool_tips_system_save(file);
        hud_ui_handbook_save(file);
        hud_ui_weapon_cabinet_save(file);
    }
}

// 0x008C7840 (:2462)
void ui_system_load(cfile* file)
{
    if (file != nullptr)
    {
        fog_of_war_load(file);
        Cartographer.minimap_load(file);
        ui_tool_tips_system_load(file);
        hud_ui_handbook_load(file);
        hud_ui_weapon_cabinet_load(file);
    }
}

// 0x00904C60 (:2476)
// The loading box (ref counted): the pause menu's hint shown, the pause fade-in restarted in single player gameplay,
// then a MBT_LOADING box in the utility document with its own locked input id. The first box pauses single player
// gameplay (and its briefing and handbook videos) or locks the multiplayer player's controls.
void ui_show_loading_screen(bool pause_music, const char* display_string)
{
    if (ui_is_msgbox_active(Loading_screen_handle))
    {
        Loading_screen_ref_cnt++;
        return;
    }
    int handle = -1;
    vint_doc_info* utility = ui_get_doc_info(Utility_doc_handle);
    if (utility != nullptr && utility->m_doc != nullptr)
    {
        vint_object_base* hint = vint_object_base::find_object("highlight_hint", utility->m_doc);
        if (hint != nullptr)
        {
            hint->set_property_value("visible", vint_variant(true), 0);
        }
    }
    if (Utility_doc_handle != -1 && !Message_box_info_set.save_load_active)
    {
        Message_box_info_set.save_load_active = true;
        if (gameseq_get_state() == GS_GAMEPLAY && !Multiplayer)
        {
            vint_doc_info* info = ui_get_doc_info(Utility_doc_handle);
            vint_document* doc = info != nullptr ? info->m_doc : nullptr;
            if (doc != nullptr)
            {
                vint_object_base* anim = vint_object_base::find_object("popup_game_pause_fade_in", doc);
                if (anim != nullptr && anim->is_type(vint_object_animation::Object_type))
                {
                    anim->set_property_value("start_time", vint_variant(doc->get_time_index()), 0);
                    // Original: is_paused set from a null property value.
                    anim->set_property_value("is_paused", (const char*)nullptr, 0);
                }
            }
        }
        msgbox_info* box = Message_box_info_set.msgbox_info_pool.alloc(0);
        if (box != nullptr)
        {
            strncpy(box->loading_text, display_string, sizeof(box->loading_text));
            const char* title = cBlank_string;
            handle = (int)box->handle;
            box->callback = loading_screen_cb;
            box->type = MBT_LOADING;
            // Magic number: the first box (fewer than 2 in use).
            if (Message_box_info_set.msgbox_info_pool.get_capacity() - Message_box_info_set.msgbox_info_pool.get_free() < 2)
            {
                Paused_game = false;
                Locked_controls = false;
                if (gameseq_state_is_active(GS_VIDEO_CUTSCENE_PLAY))
                {
                    video_cutscene_pause();
                }
                if (!Multiplayer && gameseq_state_is_active(GS_GAMEPLAY))
                {
                    if (gameseq_state_is_active(GS_INGAME_MISSION_BRIEFING))
                    {
                        hud_ui_mission_briefing_pause(true, false);
                    }
                    if (gameseq_state_is_active(GS_INGAME_HANDBOOK_HUD))
                    {
                        hud_ui_handbook_pause_video(true);
                    }
                    game_pause(pause_music);
                    Paused_game = true;
                }
                else if (Multiplayer && gameseq_state_is_active(GS_GAMEPLAY))
                {
                    Player_input_was_enabled = control_player_input_enabled();
                    Player_camera_input_was_enabled = control_player_camera_input_enabled();
                    control_enable_player_input(false);
                    control_enable_player_camera_input(false);
                    Locked_controls = true;
                }
            }
            if (cur_id == -1)
            {
                cur_id = 1;
            }
            int input_id;
            // Magic number: 16, the size of Input_types.
            if (Input_types.size() < 16)
            {
                input_request_data request;
                request.type = IRT_UNLIMITED_INPUT;
                request.locked = true;
                request.id = cur_id;
                Current_input_lock_index = Input_types.add(request);
                Current_input_lock_id = cur_id;
                input_id = cur_id;
                cur_id++;
            }
            else
            {
                VLIB_CRITICAL_FAILURE();
                input_id = -1;
            }
            box->input_id = input_id;
            strncpy(box->caption, "", sizeof(box->caption));
            box->menu2_handle = ui_menu_add(Utility_doc_handle, title, nullptr, my_selection_hint_cb, "_MB", true, 1, box->input_id, false);
            if (box->menu2_handle == -1)
            {
                ui_hide_msgbox(handle, false);
                handle = -1;
            }
            else
            {
                vint_data_item* item = vint_data_item::find((unsigned int)Msg_box_data_item_handle);
                if (item != nullptr)
                {
                    // m_updated (protected; 0x42C) as the inlined set writes it.
                    *(bool*)((unsigned char*)item + 0x42C) = true;
                }
            }
        }
        Input_enabled_uimen = true;
        input_set_feedback(Player_controller_index != 0xFFFFFFFF ? Player_controller_index : 0, 0.0f, 0.0f);
    }
    Loading_screen_handle = handle;
    Loading_screen_ref_cnt++;
}

// 0x008ED160 (:2488)
void ui_remove_loading_screen()
{
    const int handle = Loading_screen_handle;
    if (Loading_screen_ref_cnt > 0)
    {
        Loading_screen_ref_cnt--;
        if (Loading_screen_ref_cnt < 1)
        {
            if (Loading_screen_handle != -1 && ui_is_msgbox_active(Loading_screen_handle))
            {
                ui_hide_msgbox(handle, false);
            }
            Loading_screen_handle = -1;
        }
    }
}

// 0x0089BCA0 (:2503)
bool ui_loading_screen_is_open()
{
    return Loading_screen_ref_cnt > 0;
}

// 0x00904FE0 (:2510)
// Loads a document by name (its stream info's name), or takes its record again when it is loaded:
// - DLT_PERMANENTLY_LOAD: vint loads the document file itself, at once;
// - DLT_SYNCHRONOUS_HARDLOAD: the queued unloads and loads run, then the document's containers are preloaded;
// - DLT_NORMAL: the peg container is streamed (35 s timeout), optionally behind the loading box; the record is
//   queued for ui_handshake.
// Returns the document handle, -1 on failure.
int ui_load_document(const char* name, doc_fadein_type dft, float fadein_time, doc_load_type load_type, bool show_loading_msg, stream2_priority priority, bool pause_music)
{
    if (name == nullptr)
    {
        return -1;
    }
    ui_force_unload_complete_on_handle(name);
    vint_doc_info* info = ui_get_doc_info(name);
    if (info != nullptr)
    {
        const int handle = info->m_handle;
        if (ui_get_doc_asset_state(handle) == ST_UNLOADED)
        {
            ui_reload_doc_assets(handle);
            info->m_stream_timer.set(Doc_stream_timeout_ms);
        }
        if (handle_list_find(Unload_handles, info->m_handle) != -1)
        {
            Unload_handles.remove_matching(info->m_handle);
        }
        else if (handle_list_find(Unloading_handles, info->m_handle) != -1)
        {
            Unloading_handles.remove_matching(info->m_handle);
        }
        info->release_data_groups();
        info->release_data_items();
        info->release_data_events();
        info->fade_duration = fadein_time;
        info->fade_running_time = 0.0f;
        info->fade_type = dft;
        info->m_paused = false;
        if (info->m_doc != nullptr)
        {
            info->m_doc->set_time_index(0.0f);
        }
        return handle;
    }
    info = Vint_doc_info_set.m_doc_info_pool.alloc(0);
    if (info == nullptr)
    {
        return -1;
    }
    const int handle = info->m_handle;
    info->m_load_type = load_type;
    strncpy(info->m_name, name, sizeof(info->m_name));
    if (load_type == DLT_PERMANENTLY_LOAD)
    {
        vint_document* doc = vint_document::load(name, Vint_temp_mempool);
        if (doc != nullptr)
        {
            info->m_doc = doc;
            info->m_paused = false;
            info->m_assets_loaded = true;
            info->m_create_doc = false;
            ui_setup_doc_tween(true, (unsigned int)dft, fadein_time, info);
            doc->pause(false);
            return handle;
        }
    }
    else if (load_type == DLT_SYNCHRONOUS_HARDLOAD)
    {
        for (int i = 0; i < Unload_handles.size(); i++)
        {
            ui_unload_document_internal(Unload_handles[i]);
        }
        Unload_handles.clear();
        for (int i = 0; i < Unload_assets_handles.size(); i++)
        {
            ui_unload_doc_assets_internal(Unload_assets_handles[i]);
        }
        Unload_assets_handles.clear();
        for (int i = 0; i < Load_handles.size(); i++)
        {
            ui_load_document_internal(Load_handles[i]);
        }
        Load_handles.clear();
        mem_manager_force_synchronous_process();
        vint_doc_stream_info* stream_info = mem_manager_find_doc_stream_info(name);
        if (stream_info != nullptr)
        {
            info->fade_type = dft;
            info->fade_duration = fadein_time;
            info->m_paused = true;
            info->m_assets_loaded = false;
            info->m_create_doc = true;
            info->m_show_loading_msg = false;
            info->m_stream_timer.invalidate();
            info->peg_srid = stream_info->srid2;
            const bool doc_loaded = stream2_preload(stream_info->srid);
            const bool pegs_loaded = stream2_preload(stream_info->srid2);
            if (doc_loaded && pegs_loaded)
            {
                return handle;
            }
            stream2_release(stream_info->srid);
            stream2_release(stream_info->srid2);
        }
    }
    else
    {
        const unsigned int srid = mem_manager_get_doc_srid(name);
        info->peg_srid = srid;
        if (srid != 0)
        {
            stream2_set_priority(srid, (unsigned char)priority);
            info->fade_type = dft;
            info->fade_duration = fadein_time;
            info->m_paused = true;
            info->m_assets_loaded = false;
            info->m_create_doc = true;
            info->m_show_loading_msg = show_loading_msg;
            info->m_stream_timer.set(Doc_stream_timeout_ms);
            if (show_loading_msg)
            {
                ui_show_loading_screen(pause_music, "MENU_LOADING");
            }
            Load_handles.add(info->m_handle);
            return handle;
        }
    }
    VLIB_CRITICAL_FAILURE();
    Vint_doc_info_set.m_doc_info_pool.free(info);
    return -1;
}

// 0x008ED1B0 (:2703)
// Unloads a document: a document not created yet is queued for ui_handshake at once (and false returned); with no
// fade it is queued too (an unloading one first ends its fade); with a fade it unloads when the fade ends.
bool ui_unload_document(int handle, doc_fadeout_type dft, float fadeout_time)
{
    vint_doc_info* info = ui_get_vint_doc_info(handle);
    if (info == nullptr)
    {
        return false;
    }
    if (info->m_show_loading_msg && !info->m_assets_loaded)
    {
        ui_remove_loading_screen();
        info->m_show_loading_msg = false;
    }
    if (info->m_doc == nullptr)
    {
        handle_list_add(Unload_handles, info->m_handle);
        return false;
    }
    int posn = -1;
    if (dft == DFOT_NONE)
    {
        ui_is_doc_unloading(handle, posn);
        if (posn != -1)
        {
            ui_destroy_doc_tween(handle);
            Unloading_handles.remove(posn);
        }
        Unload_handles.add(info->m_handle);
        return true;
    }
    // Original: the unloading test's result is not used, so a document can be in the list twice.
    ui_is_doc_unloading(handle, posn);
    Unloading_handles.add(info->m_handle);
    if (fadeout_time > Min_fade_time && info->m_doc != nullptr && info->m_doc->get_root_element() != nullptr)
    {
        if ((int)dft > 0 && (int)dft < 3)
        {
            info->is_fadein = false;
            info->fade_type = dft;
            info->fade_duration = fadeout_time;
            info->fade_running_time = 0.0f;
        }
        ui_update_doc_fade_status(handle, 0.0f, posn);
    }
    return true;
}

// Adds a handle (0 or more) to a frame list once.
static void ui_add_frame_handle(farray<int,16>& list, int doc_handle)
{
    if (doc_handle < 0 || handle_list_find(list, doc_handle) != -1)
    {
        return;
    }
    handle_list_add(list, doc_handle);
}

// 0x008DAB40 (:2774)
void ui_add_for_processing(const farray<int,16>& doc_handles)
{
    for (int i = 0; i < doc_handles.size(); i++)
    {
        ui_add_frame_handle(Process_handles, doc_handles[i]);
    }
}

// 0x008C7880 (:2785)
void ui_add_for_processing(int doc_handle)
{
    ui_add_frame_handle(Process_handles, doc_handle);
}

// 0x008DABD0 (:2809)
void ui_add_for_rendering(const farray<int,16>& doc_handles)
{
    for (int i = 0; i < doc_handles.size(); i++)
    {
        ui_add_frame_handle(Render_handles, doc_handles[i]);
    }
}

// 0x008C78E0 (:2820)
void ui_add_for_rendering(int doc_handle)
{
    ui_add_frame_handle(Render_handles, doc_handle);
}

// 0x009110B0 (:2844)
// Once a frame for the control id on top: the fade to black, the screen blur, the memory manager, the menus and
// message boxes, the tool tips, the input, vint, then each document added for processing (its fade, then the
// document) and each unloading one (in Frametime steps; one whose fade ended goes to the unload list), and Lua.
// input_only stops after vint (the tool tips only when one runs).
void ui_process(unsigned int control_id, bool input_only)
{
    if (fonts_are_initializing)
    {
        return;
    }
    if (control_id != top_control_id() || Internal_flags.has_processed)
    {
        return;
    }
    Internal_flags.has_processed = true;
    const float dtime = Frametime_real <= Max_frametime_uiman ? Frametime_real : Max_frametime_uiman;
    ui_fade_to_black_process();
    ui_handle_screen_blur(dtime);
    if (!Shutdown)
    {
        mem_manager_process();
    }
    ui_menu_process(dtime);
    ui_msgbox_process(dtime);
    if (input_only)
    {
        vint_process(dtime);
        if (Tool_tip_queue[Current_tool_tip].running)
        {
            ui_tool_tips_process(dtime);
        }
        ui_process_input();
        return;
    }
    ui_tool_tips_process(dtime);
    ui_process_input();
    vint_process(dtime);
    const int num_process = Process_handles.size();
    for (int i = 0; i < num_process; i++)
    {
        const int handle = Process_handles[i];
        vint_doc_info* info = ui_get_doc_info(handle);
        if (info != nullptr && info->m_doc != nullptr)
        {
            int posn;
            ui_update_doc_fade_status(handle, dtime, posn);
            info->m_doc->process(dtime);
        }
    }
    int num_unloading = Unloading_handles.size();
    for (int i = 0; i < num_unloading; i++)
    {
        const int handle = Unloading_handles[i];
        vint_doc_info* info = ui_get_doc_info(handle);
        if (info == nullptr || info->m_doc == nullptr)
        {
            continue;
        }
        int posn = -1;
        ui_update_doc_fade_status(handle, dtime, posn);
        if (posn == -1)
        {
            float left = dtime;
            if (Frametime < dtime)
            {
                do
                {
                    info->m_doc->process(Frametime);
                    left -= Frametime;
                } while (Frametime < left);
            }
            // Magic number: a remainder above 1 ms.
            if (left > 0.001f)
            {
                info->m_doc->process(left);
            }
        }
        else
        {
            Unloading_handles.remove(posn);
            handle_list_add(Unload_handles, handle);
            num_unloading--;
            i--;
        }
    }
    lua_do_frame();
    Process_handles.clear();
}

// Whether the message box pool is empty (rfg.exe compares its capacity with its free count).
static bool ui_no_msgbox()
{
    return Message_box_info_set.msgbox_info_pool.get_capacity() == (unsigned int)Message_box_info_set.msgbox_info_pool.get_free();
}

// 0x008F61D0 (:2997)
// Once a frame for the control id on top (not while minimized): a new screen size updates the resolution; the fade to
// black (before the documents, except in gameplay or the credits or under a message box, where it comes after them),
// the documents added for rendering and the unloading ones, the tool tip (while the game runs, or a tip that runs
// paused), then the utility document with the cursor. The render list is cleared.
bool ui_render(unsigned int control_id, bool skip_doc_list)
{
    if (os_minimized())
    {
        return false;
    }
    if (control_id != top_control_id() || Internal_flags.has_rendered)
    {
        return false;
    }
    Internal_flags.has_rendered = true;
    const rl_screen_info* screen = rl_render_lib::get_instance()->get_screen_info();
    if (Screen_size.x != screen->get_screen_width() || Screen_size.y != screen->get_screen_height())
    {
        Screen_size.x = screen->get_screen_width();
        Screen_size.y = screen->get_screen_height();
        ui_update_resolution(Screen_size.x, Screen_size.y);
    }
    const bool fade_after = gameseq_get_state() == GS_GAMEPLAY || gameseq_get_state() == GS_CREDITS ||
                            (Utility_doc_handle != -1 && !ui_no_msgbox());
    if (!fade_after)
    {
        ui_fade_to_black_render();
    }
    if (!skip_doc_list)
    {
        const int num_render = Render_handles.size();
        for (int i = 0; i < num_render; i++)
        {
            vint_doc_info* info = ui_get_doc_info(Render_handles[i]);
            if (info != nullptr && info->m_doc != nullptr)
            {
                info->m_doc->render();
            }
        }
        const int num_unloading = Unloading_handles.size();
        for (int i = 0; i < num_unloading; i++)
        {
            vint_doc_info* info = ui_get_doc_info(Unloading_handles[i]);
            if (info != nullptr && info->m_doc != nullptr)
            {
                info->m_doc->render();
            }
        }
    }
    if (!Active)
    {
        vint_doc_info* tips = ui_get_doc_info(Doc_handle_uitips);
        if (tips != nullptr && tips->m_doc != nullptr)
        {
            const tool_tip_instance& tip = Tool_tip_queue[Current_tool_tip];
            if (!game_is_paused() ||
                (tip.running && tip.base_tool_tip != nullptr && (tip.base_tool_tip->flags.pause_game || tip.base_tool_tip->flags.run_paused)))
            {
                tips->m_doc->render();
            }
        }
    }
    if (gameseq_get_state() == GS_GAMEPLAY || gameseq_get_state() == GS_CREDITS || (Utility_doc_handle != -1 && !ui_no_msgbox()))
    {
        ui_fade_to_black_render();
    }
    vint_doc_info* utility = ui_get_doc_info(Utility_doc_handle);
    if (utility != nullptr && utility->m_doc != nullptr)
    {
        ui_cursor_update();
        utility->m_doc->render();
    }
    Render_handles.clear();
    return true;
}

// 0x008C7940 (:3096)
// A new control id on top of the stack (0 when it is full).
unsigned int ui_request_control_id()
{
    // Magic number: 16 control slots.
    if (Controller_slots.size() > 15)
    {
        return 0;
    }
    id++;
    if (Controller_slots.size() + 1 > Controller_slots.capacity())
    {
        VLIB_CRITICAL_FAILURE();
        return id;
    }
    Controller_slots.add(id);
    return id;
}

// 0x008B4CD0 (:3111)
bool ui_relinquish_control_id(unsigned int control_id)
{
    for (int i = 0; i < Controller_slots.size(); i++)
    {
        if (Controller_slots[i] == control_id)
        {
            Controller_slots.remove(i);
            return true;
        }
    }
    return false;
}

// 0x008B4D20 (:3125)
unsigned int ui_get_current_control_id()
{
    return top_control_id();
}

// 0x0089BCB0 (:3135)
// Clamped to 1/30 .. 0.5 s.
void ui_set_max_frametime(float max)
{
    // Magic numbers: 0.5 s and 1/30 s.
    if (max > 0.5f)
    {
        max = 0.5f;
    }
    else if (max < 0.033333335f)
    {
        max = 0.033333335f;
    }
    Max_frametime_uiman = max;
}

// Adds a data handle to a document's list once (false when the list is full). Original: a full array after the
// size test is a critical failure that still returns true.
template<int N>
static bool ui_add_doc_data_handle(farray<unsigned int,N>& handles, unsigned int data_handle)
{
    const int num = handles.size();
    for (int i = 0; i < num; i++)
    {
        if (handles[i] == data_handle)
        {
            return true;
        }
    }
    if (num >= N)
    {
        return false;
    }
    if (num + 1 > handles.capacity())
    {
        VLIB_CRITICAL_FAILURE();
        return true;
    }
    handles.add(data_handle);
    return true;
}

// 0x008C79A0 (:3147)
bool ui_add_data_group(int doc_handle, int data_group_handle)
{
    vint_doc_info* info = ui_get_doc_info(doc_handle);
    if (info == nullptr || info->m_doc == nullptr)
    {
        return false;
    }
    return ui_add_doc_data_handle(info->m_group_handles, (unsigned int)data_group_handle);
}

// 0x008C7A30 (:3181)
bool ui_remove_data_group(int doc_handle, int data_group_handle)
{
    vint_doc_info* info = ui_get_doc_info(doc_handle);
    if (info != nullptr && info->m_doc != nullptr)
    {
        info->m_group_handles.remove_matching((unsigned int)data_group_handle);
        return true;
    }
    return false;
}

// 0x008C7A70 (:3202)
bool ui_add_data_item(int doc_handle, int data_item_handle)
{
    vint_doc_info* info = ui_get_doc_info(doc_handle);
    if (info == nullptr || info->m_doc == nullptr)
    {
        return false;
    }
    return ui_add_doc_data_handle(info->m_data_handles, (unsigned int)data_item_handle);
}

// 0x008C7B10 (:3236)
bool ui_add_data_event(int doc_handle, int data_event_handle)
{
    vint_doc_info* info = ui_get_doc_info(doc_handle);
    if (info == nullptr || info->m_doc == nullptr)
    {
        return false;
    }
    return ui_add_doc_data_handle(info->m_event_handles, (unsigned int)data_event_handle);
}

// 0x008ED350 (:3270)
// LTCG: the srid in EDI. The memory manager's callback once a document's containers are loaded: a new document is
// created from its data and first Lua script (the document is dropped without one, original); a document whose
// assets were reloaded runs again and starts its fade in.
void ui_notify_doc_assets_loaded(unsigned int srid)
{
    vint_doc_info* info = ui_get_doc_info(srid);
    if (info == nullptr)
    {
        return;
    }
    if (info->m_show_loading_msg)
    {
        ui_remove_loading_screen();
        info->m_show_loading_msg = false;
    }
    if (!info->m_create_doc)
    {
        info->m_assets_loaded = true;
        info->m_doc->pause(false);
        if (info->fade_duration > Min_fade_time && info->m_doc != nullptr && info->m_doc->get_root_element() != nullptr)
        {
            if (info->fade_type > 0 && info->fade_type < 3)
            {
                info->is_fadein = true;
                info->fade_running_time = 0.0f;
            }
            int posn;
            ui_update_doc_fade_status(info->m_handle, 0.0f, posn);
        }
        return;
    }
    vint_doc_stream_info* stream_info = mem_manager_find_doc_stream_info(srid);
    if (stream_info != nullptr)
    {
        unsigned int script = 0;
        while (script < stream_info->num_meta_data_entries && stream_info->meta_data_entries[script].type != DOC_RESOURCE_TYPE_LUA_SCRIPT)
        {
            script++;
        }
        // Original: no script, no document (and the record is kept).
        if (script == stream_info->num_meta_data_entries)
        {
            return;
        }
        vint_document* doc = vint_document::load(stream_info->name, stream_info->vint_doc.cpu_data, stream_info->vint_doc.cpu_size,
                                                 stream_info->meta_data_entries[script].cpu_data,
                                                 stream_info->meta_data_entries[script].cpu_size, Vint_temp_mempool);
        if (doc != nullptr)
        {
            info->m_paused = false;
            info->m_doc = doc;
            info->m_assets_loaded = true;
            info->m_create_doc = false;
            info->m_stream_timer.invalidate();
            doc->pause(false);
            doc->set_time_index(0.0f);
            return;
        }
    }
    VLIB_CRITICAL_FAILURE();
    Vint_doc_info_set.m_doc_info_pool.free(info);
}

// 0x008C7BB0 (:3332)
int ui_get_doc_handle(const char* name)
{
    vint_doc_info* info = ui_get_doc_info(name);
    if (info != nullptr)
    {
        return info->m_handle;
    }
    return -1;
}

// 0x008C7BD0 (:3344)
int ui_get_doc_handle_from_vint_doc_handle(int vint_doc_handle)
{
    for (vint_doc_info* info = doc_info_first(); info != nullptr; info = doc_info_next(info))
    {
        if (info->m_doc != nullptr && info->m_doc->get_handle() == (unsigned int)vint_doc_handle)
        {
            return info->m_handle;
        }
    }
    return -1;
}

// 0x008C7C60 (:3359)
vint_document* ui_get_vint_document(int handle)
{
    vint_doc_info* info = ui_get_doc_info(handle);
    if (info != nullptr)
    {
        return info->m_doc;
    }
    return nullptr;
}

// 0x0089BCE0 (:3370)
bool ui_has_received_input()
{
    return Button_info.m_received_input || mouse_just_pressed(0) || mouse_just_moved();
}

// 0x0089BD10 (:3379)
bool ui_has_button_been_pressed()
{
    return Button_info.m_button_pressed;
}

// 0x008C7C80 (:3396)
// A button's menu input for an input id: only the locked id (or the one on top) gets it, -1 asks without an id when
// nothing is locked; a button waiting for its release reads 0.
float ui_get_button_info(int which_btn, int input_id)
{
    float value = 0.0f;
    if (input_id != -1 || (Current_input_lock_id != -1 && Current_input_lock_id != 0))
    {
        int current = Current_input_lock_id;
        if (Current_input_lock_id == -1)
        {
            current = Input_types.size() - 1 < 0 ? 1 : Input_types[Input_types.size() - 1].id;
        }
        if (input_id != current)
        {
            return 0.0f;
        }
    }
    if ((unsigned int)which_btn < (unsigned int)Num_buttons && !Button_info.binfo[which_btn].m_button_wait_for_up)
    {
        value = Button_info.binfo[which_btn].m_button_input;
    }
    return value;
}

// 0x008C7D00 (:3414)
// The button's input; a press then waits for its release.
float ui_get_button_info_and_wait(int which_btn, int input_id)
{
    const float value = ui_get_button_info(which_btn, input_id);
    if (value > 0.0f)
    {
        Button_info.binfo[which_btn].m_button_wait_for_up = true;
        Choice_last = -1;
    }
    return value;
}

// 0x0089BD20 (:3424)
int ui_get_button_controller_id(int which_btn)
{
    return Button_info.binfo[which_btn].m_controller_id;
}

// 0x0089BD30 (:3430)
float ui_get_button_raw(int which_btn)
{
    float value = 0.0f;
    if ((unsigned int)which_btn < (unsigned int)Num_buttons)
    {
        value = Button_info.binfo[which_btn].m_button_actual_input;
    }
    return value;
}

// 0x0089BD60 (:3443)
void ui_wait_for_button_ups()
{
    for (int i = 0; i < Num_buttons; i++)
    {
        Button_info.binfo[i].m_button_wait_for_up = true;
    }
    Choice_last = -1;
}

// 0x0089BD80 (:3453)
void ui_wait_for_button_ups(int which_btn)
{
    Button_info.binfo[which_btn].m_button_wait_for_up = true;
    Choice_last = -1;
}

// 0x0089BDB0 (:3484)
const vector2i& ui_get_screen_dimensions()
{
    return Screen_size;
}

// 0x0089BDC0 (:3490)
float ui_get_aspect_ratio()
{
    return Aspect_ratio;
}

// 0x0089BDD0 (:3496)
void ui_set_aspect_ratio(float aspect_ratio)
{
    Aspect_ratio = aspect_ratio;
}

// 0x0089BDE0 (:3502)
float ui_get_pixel_aspect_ratio()
{
    return Pixel_aspect_ratio;
}

// 0x0089BDF0 (:3508)
void ui_set_pixel_aspect_ratio(float aspect_ratio)
{
    Pixel_aspect_ratio = aspect_ratio;
}

// 0x008C7D40 (:3515)
void ui_set_pause_state_for_doc(int handle, bool state)
{
    vint_doc_info* info = ui_get_doc_info(handle);
    if (info != nullptr)
    {
        info->m_paused = state;
    }
}

// 0x008C7D60 (:3524)
bool ui_get_pause_state_for_doc(int handle)
{
    vint_doc_info* info = ui_get_doc_info(handle);
    if (info != nullptr)
    {
        return info->m_paused;
    }
    return false;
}

// 0x008C7D80 (:3535)
// Every unloading document goes to the unload list at once, its fade ended.
void ui_force_unloads_complete()
{
    while (Unloading_handles.size() != 0)
    {
        ui_force_unload_complete_at(0);
    }
}

// 0x008C7E30 (:3554)
// A new input id on top of the input stack (locked: it alone gets input until relinquished); -1 when the stack is
// full or the type is invalid.
int ui_register_for_processed_input(input_register_type type, bool lock_input)
{
    if (cur_id == -1)
    {
        cur_id = 1;
    }
    if (type == IRT_INVALID)
    {
        return -1;
    }
    // Magic number: 16, the size of Input_types.
    if (Input_types.size() >= 16)
    {
        VLIB_CRITICAL_FAILURE();
        return -1;
    }
    input_request_data request;
    request.type = type;
    request.locked = lock_input;
    request.id = cur_id;
    const int index = Input_types.add(request);
    if (lock_input)
    {
        Current_input_lock_id = cur_id;
        Current_input_lock_index = index;
    }
    return cur_id++;
}

// 0x008B4D40 (:3587)
// Removes the (last) entry of the id; when it was the lock, the next locked entry below it becomes the lock.
// Original bug kept: the search for the next lock starts one entry below the removed one's index (or at -2 when the
// id is not in the stack).
void ui_relinquish_input_id(int input_id)
{
    int index = Input_types.size() - 1;
    for (; index >= 0; index--)
    {
        if (Input_types[index].id == input_id)
        {
            Input_types.remove(index);
            break;
        }
    }
    if (Current_input_lock_id != input_id)
    {
        return;
    }
    index--;
    Current_input_lock_id = -1;
    Current_input_lock_index = -1;
    for (; index >= 0; index--)
    {
        if (Input_types[index].locked)
        {
            Current_input_lock_id = Input_types[index].id;
            Current_input_lock_index = index;
            return;
        }
    }
}

// 0x008B4DF0 (:3617)
int ui_get_current_input_id()
{
    if (Current_input_lock_id != -1)
    {
        return Current_input_lock_id;
    }
    if (Input_types.size() - 1 >= 0)
    {
        return Input_types[Input_types.size() - 1].id;
    }
    return 1;
}

// 0x0089BE00 (:3631)
// The locked id, 0 when none.
int ui_get_input_locked_status()
{
    return Current_input_lock_id != -1 ? Current_input_lock_id : 0;
}

// 0x0089BE10 (:3637)
void ui_enable_processed_input()
{
    Input_enabled = true;
}

// 0x0089BE20 (:3643)
void ui_disable_processed_input()
{
    Input_enabled = false;
}

// 0x0089BE30 (:3649)
// The blur is reference counted; the first one pauses the game.
void ui_set_screen_blur(bool on, bool pause_music)
{
    if (on)
    {
        Screen_blur_info.screen_blur_count++;
    }
    else
    {
        gamemusic_event_occurred(GAMEMUSIC_UI_BLUR_END);
        Screen_blur_info.screen_blur_count--;
        if (Screen_blur_info.screen_blur_count < 0)
        {
            VLIB_CRITICAL_FAILURE();
            Screen_blur_info.screen_blur_count = 0;
        }
    }
    Screen_blur_info.pause_music = pause_music;
    if (on && !Screen_blur_info.paused_game)
    {
        game_pause(pause_music);
        Screen_blur_info.paused_game = true;
    }
}

// 0x0089BEA0 (:3688)
bool ui_get_screen_blurring()
{
    return Screen_blur_info.actively_blurring;
}

// 0x008C7EC0 (:3694)
// The document's time stops and its assets unload in ui_handshake.
void ui_unload_doc_assets(int handle)
{
    vint_doc_info* info = ui_get_doc_info(handle);
    if (info == nullptr)
    {
        return;
    }
    if (info->m_doc != nullptr)
    {
        info->m_doc->pause(true);
    }
    handle_list_add(Unload_assets_handles, info->m_handle);
}

// 0x008DAC60 (:3713)
void ui_reload_doc_assets(int handle)
{
    vint_doc_info* info = ui_get_doc_info(handle);
    if (info == nullptr)
    {
        return;
    }
    if (info->m_doc == nullptr)
    {
        info->m_create_doc = true;
    }
    else
    {
        info->m_create_doc = false;
        info->m_doc->pause(info->m_paused);
    }
    info->m_stream_timer.set(Doc_stream_timeout_ms);
    mem_manager_load_doc(info->peg_srid);
}

// 0x008ED4C0 (:3738)
// Starts a fade in (alpha or from black): at once for a loaded document, else when its assets arrive.
// Original: the handle must be below 16.
bool ui_fade_in_doc(int handle, doc_fadein_type type, float fadein_time)
{
    if ((unsigned int)(type - DFIT_ALPHA) >= 2 || (unsigned int)handle >= 16)
    {
        return false;
    }
    vint_doc_info* info = ui_get_doc_info(handle);
    if (info == nullptr)
    {
        return false;
    }
    if (info->m_assets_loaded)
    {
        info = ui_get_vint_doc_info(handle);
        if (info != nullptr && fadein_time > Min_fade_time && info->m_doc != nullptr && info->m_doc->get_root_element() != nullptr)
        {
            info->fade_duration = fadein_time;
            info->is_fadein = true;
            info->fade_type = type;
            info->fade_running_time = 0.0f;
            int posn;
            ui_update_doc_fade_status(handle, 0.0f, posn);
            return true;
        }
    }
    else
    {
        info->fade_type = type;
        info->fade_duration = fadein_time;
    }
    return true;
}

// 0x008DACC0 (:3764)
// The document's state: a permanent one is loaded once created; a streamed one is loading until its document and
// peg containers are loaded and it is created (and no loading box is up), with a 35 s timeout (an error after it).
stream2_status_val ui_get_doc_asset_state(int handle)
{
    vint_doc_info* info = ui_get_doc_info(handle);
    if (info == nullptr)
    {
        return ST_UNLOADED;
    }
    if (info->m_load_type == DLT_PERMANENTLY_LOAD)
    {
        return info->m_doc != nullptr ? ST_LOADED : ST_UNLOADED;
    }
    vint_doc_stream_info* stream_info = mem_manager_find_doc_stream_info(info->peg_srid);
    if (stream_info == nullptr)
    {
        return ST_UNLOADED;
    }
    char message[128];
    const stream2_status_val doc_status = stream2_get_status(stream_info->srid);
    if (doc_status == ST_LOADING)
    {
        if (!info->m_stream_timer.elapsed())
        {
            return ST_LOADING;
        }
        VLIB_CRITICAL_FAILURE();
        return ST_ERROR;
    }
    bool timed_out;
    if (doc_status == ST_UNLOADED && info->m_doc == nullptr)
    {
        if (!mem_manager_doc_in_loading_q(stream_info) && !Load_handles.contains(info->m_handle))
        {
            return ST_UNLOADED;
        }
        timed_out = info->m_stream_timer.elapsed();
    }
    else
    {
        if (doc_status == ST_ERROR)
        {
            // Original: the format's %s gets no argument.
            keen::formatString(message, sizeof(message), "Error loading assets for VINT doc, \"%s\"");
            keen::debug::nativePrintArgs(message);
            return ST_ERROR;
        }
        const stream2_status_val peg_status = stream2_get_status(stream_info->srid2);
        if (peg_status == ST_ERROR)
        {
            keen::formatString(message, sizeof(message), "Error loading assets for VINT doc, \"%s\"");
            keen::debug::nativePrintArgs(message);
            return ST_ERROR;
        }
        if (peg_status != ST_UNLOADED)
        {
            if (peg_status != ST_LOADED)
            {
                return peg_status;
            }
            if (info->m_doc != nullptr && (Loading_screen_handle == -1 || !ui_is_msgbox_active(Loading_screen_handle)))
            {
                return ST_LOADED;
            }
            return ST_LOADING;
        }
        if (!mem_manager_doc_in_loading_q(stream_info))
        {
            return ST_UNLOADED;
        }
        timed_out = info->m_stream_timer.elapsed();
    }
    if (timed_out)
    {
        VLIB_CRITICAL_FAILURE();
        return ST_ERROR;
    }
    return ST_LOADING;
}

// 0x0089BEB0 (:3866)
bool ui_request_image(const char* img_filename)
{
    if (img_filename != nullptr)
    {
        const unsigned int srid = stream2_container_find(img_filename);
        if (srid != 0)
        {
            return stream2_request(srid);
        }
    }
    return false;
}

// 0x0089BEE0 (:3880)
stream2_status_val ui_get_image_status(const char* img_filename)
{
    if (img_filename != nullptr)
    {
        const unsigned int srid = stream2_container_find(img_filename);
        if (srid != 0)
        {
            return stream2_get_status(srid);
        }
    }
    return ST_ERROR;
}

// 0x0089BF10 (:3895)
bool ui_release_image(const char* img_filename)
{
    if (img_filename == nullptr)
    {
        return false;
    }
    const unsigned int srid = stream2_container_find(img_filename);
    if (srid == 0)
    {
        return false;
    }
    if (stream2_get_status(srid) != ST_UNLOADED)
    {
        stream2_release(srid);
    }
    return true;
}

// 0x008C7F20 (:3914)
void ui_destroy_doc_data_info(int handle, bool destroy_data_items, bool destroy_data_groups, bool destroy_data_events)
{
    vint_doc_info* info = ui_get_doc_info(handle);
    if (info == nullptr)
    {
        return;
    }
    if (destroy_data_groups)
    {
        info->release_data_groups();
    }
    if (destroy_data_items)
    {
        info->release_data_items();
    }
    if (destroy_data_events)
    {
        info->release_data_events();
    }
}

// 0x0089BF50 (:3934)
control_button_action ui_get_control_button_value(button_choices btn)
{
    if (btn < BC_NUM_CHOICES)
    {
        return (control_button_action)Button_info.binfo[btn].m_button_choice;
    }
    return CBA_BUTTON_INVALID;
}

// 0x0089BF70 (:3943)
void ui_fade_to_black_init()
{
    Fade_paused = false;
    Fade_out = false;
    Fade_ref = 0;
    Fade_alpha = 0.0f;
}

// 0x0089BFA0 (:3951)
// The black's alpha moves one unit a second; a fade that paused the game unpauses it once clear.
void ui_fade_to_black_process()
{
    if (Fade_out)
    {
        Fade_alpha = Frametime_real + Fade_alpha;
        if (Fade_alpha > 1.0f)
        {
            Fade_alpha = 1.0f;
        }
    }
    else if (Fade_alpha > 0.0f)
    {
        Fade_alpha = Fade_alpha - Frametime_real;
        if (Fade_alpha < 0.0f)
        {
            Fade_alpha = 0.0f;
        }
    }
    else
    {
        Fade_alpha = 0.0f;
        if (Fade_paused)
        {
            if (game_is_paused())
            {
                game_unpause();
            }
            Fade_paused = false;
        }
    }
}

// 0x008B4E20 (:3976)
// A black quad over the screen at Fade_alpha.
void ui_fade_to_black_render()
{
    if (Fade_alpha <= 0.0f)
    {
        return;
    }
    rl_draw* draw = rl_draw::get_instance();
    draw->set_texture(-1);
    draw->tristrip_2d_begin(rl_primitive_state::FILTER);
    rl_primitive_2d_vert* verts = draw->tristrip_2d_allocate_tris(2);
    if (verts == nullptr)
    {
        return;
    }
    // The color is black with the alpha in the top byte.
    const unsigned int black = (unsigned int)(int)(Fade_alpha * 255.0) << 24;
    for (int i = 0; i < 4; i++)
    {
        verts[i].color.uint_color = black;
    }
    verts[0].uvs.x = 0.0f;
    verts[0].uvs.y = 0.0f;
    verts[1].uvs.x = 1.0f;
    verts[1].uvs.y = 0.0f;
    verts[2].uvs.x = 1.0f;
    verts[2].uvs.y = 1.0f;
    verts[3].uvs.x = 0.0f;
    verts[3].uvs.y = 1.0f;
    verts[0].pt.x = 0.0f;
    verts[0].pt.y = (float)Screen_size.y;
    verts[1].pt.x = (float)Screen_size.x;
    verts[1].pt.y = (float)Screen_size.y;
    verts[2].pt.x = 0.0f;
    verts[2].pt.y = 0.0f;
    verts[3].pt.x = (float)Screen_size.x;
    verts[3].pt.y = 0.0f;
    draw->tristrip_2d_end();
}

// 0x0089C040 (:4008)
void ui_fade_to_black_fade_in(bool clear_ref_count)
{
    Fade_ref--;
    if (Fade_ref < 0)
    {
        Fade_ref = 0;
    }
    if (clear_ref_count)
    {
        Fade_ref = 0;
        Fade_out = false;
        return;
    }
    if (Fade_ref == 0)
    {
        Fade_out = false;
    }
}

// 0x0089C080 (:4026)
void ui_fade_to_black_fade_out(bool pause)
{
    Fade_ref++;
    if (Fade_ref == 1)
    {
        Fade_out = true;
    }
    if (pause && !Fade_paused)
    {
        game_pause(false);
        Fade_paused = true;
    }
}

// 0x0089C0C0 (:4039)
void ui_fade_to_black_instant_fade_in()
{
    Fade_out = false;
    Fade_alpha = 0.0f;
    Fade_ref = 0;
}

// 0x0089C0E0 (:4046)
void ui_fade_to_black_instant_fade_out()
{
    Fade_ref++;
    Fade_out = true;
    Fade_alpha = 1.0f;
}

// 0x0089C110 (:4052)
bool ui_fade_to_black_is_faded()
{
    return Fade_ref > 0 && Fade_out && Fade_alpha >= 1.0f;
}

// 0x0089C140 (:4053)
bool ui_fade_to_black_is_fade_in_complete()
{
    return Fade_alpha <= 0.0f && Fade_ref < 1;
}

// 0x008B4FB0 (:4059)
static bool ui_mouse_is_available()
{
    return !multi_core_platform_ui_is_active() && Cursor_visible;
}

// The variant's value union (offset 8), read raw as rfg.exe's inlined accessors do (the members are private).
template<typename T>
static T variant_raw(const vint_variant& v, unsigned int offset = 8)
{
    return *(const T*)((const unsigned char*)&v + offset);
}

// A vector2 property, (0, 0) when it is not one.
static vector2 ui_get_vector2_property(vint_object_base* obj, const char* name, vint_variant& value)
{
    obj->get_property_value(name, value, 0);
    if (value.get_type() != VINT_PROP_TYPE_VECTOR2F)
    {
        return vector2(0.0f, 0.0f);
    }
    return vector2(variant_raw<float>(value, 8), variant_raw<float>(value, 12));
}

// The object's resolution offset and scale (center, maintain ratio, or an element's inherited ones).
static void ui_get_obj_res_transform(vint_object_base* obj, vector2& offset, vector2& scale)
{
    vint_res_offset offset_x = VINT_RES_OFFSET_CENTER;
    vint_res_offset offset_y = VINT_RES_OFFSET_CENTER;
    vint_res_scale scale_x = VINT_RES_SCALE_MAINTAIN_RATIO;
    vint_res_scale scale_y = VINT_RES_SCALE_MAINTAIN_RATIO;
    if (obj->is_type(vint_element_base::Object_type))
    {
        static_cast<vint_element_base*>(obj)->get_res_offsets_and_scales_inherited(offset_x, offset_y, scale_x, scale_y);
    }
    offset = vint_project_get_offset(offset_x, offset_y);
    scale = vint_project_get_scale(scale_x, scale_y);
}

// Whether a "visible" variant is on (by its type: non-zero numbers, the bool, a non-zero pair).
static bool ui_variant_visible(const vint_variant& value)
{
    switch (value.get_type())
    {
    case VINT_PROP_TYPE_INT:
    case VINT_PROP_TYPE_UINT:
        // Original: the integer's bits compared as a float with 0.
        return variant_raw<float>(value) != 0.0f;
    case VINT_PROP_TYPE_FLOAT:
        return variant_raw<float>(value) != 0.0f;
    case VINT_PROP_TYPE_BOOL:
        return variant_raw<bool>(value);
    case VINT_PROP_LAST_VALID_FOR_VARIANT:
        return variant_raw<float>(value, 8) != 0.0f || variant_raw<float>(value, 12) != 0.0f;
    default:
        return false;
    }
}

// Sets a float property by its name's crc.
static void ui_set_float_property(vint_object_base* obj, const char* name, float value)
{
    obj->set_property_value(crc_str(name, 0), vint_variant(value), 0);
}

// 0x008C7F60 (:4065)
bool ui_is_mouse_over_vint_object(const char* objectName, vint_document* doc)
{
    return ui_is_mouse_over_vint_object(vint_object_base::find_object(objectName, doc));
}

// 0x008C7F80 (:4072)
bool ui_is_mouse_over_vint_object(const char* objectName, const char* parentName, vint_document* doc)
{
    vint_object_base* parent = vint_object_base::find_object(parentName, doc);
    if (parent != nullptr)
    {
        vint_object_base* obj = parent->find_child(crc_str(objectName, 0));
        if (obj != nullptr)
        {
            return ui_is_mouse_over_vint_object(obj);
        }
    }
    return false;
}

// 0x008B4FD0 (:4084)
// The mouse (in the interface space) within the object's screen box.
bool ui_is_mouse_over_vint_object(vint_object_base* obj)
{
    if (!ui_mouse_is_available() || obj == nullptr || !Ui_cursor_visible)
    {
        return false;
    }
    vint_variant value;
    const vector2 nw = ui_get_vector2_property(obj, "screen_nw", value);
    const vector2 se = ui_get_vector2_property(obj, "screen_se", value);
    vector2 offset;
    vector2 scale;
    ui_get_obj_res_transform(obj, offset, scale);
    return nw.x * Mouse_x_scale * scale.x + Mouse_x_scale * offset.x <= Mouse_scaled_x &&
           Mouse_scaled_x <= se.x * Mouse_x_scale * scale.x + Mouse_x_scale * offset.x &&
           nw.y * Mouse_y_scale * scale.y + Mouse_y_scale * offset.y <= Mouse_scaled_y &&
           Mouse_scaled_y <= se.y * Mouse_y_scale * scale.y + Mouse_y_scale * offset.y;
}

// 0x0089C160 (:4135)
bool ui_is_mouse_over_extents(const bbox2d& extents)
{
    const float out = (extents.min.x <= Mouse_scaled_x ? 0.0f : 1.0f) + (extents.min.y <= Mouse_scaled_y ? 0.0f : 1.0f) +
                      (Mouse_scaled_x <= extents.max.x ? 0.0f : 1.0f) + (Mouse_scaled_y <= extents.max.y ? 0.0f : 1.0f);
    return out == 0.0f;
}

// 0x008C7FD0 (:4143)
// The hint the mouse releases on (its index), -1 otherwise; with highlight the hovered hints get alpha 1, the others
// 0.5. Original: a highlight before a release returns the first hovered hint only on release.
int ui_mouse_process_hints(const char** hintNames, int numHints, int docHandle, bool doHighlight)
{
    if (!ui_mouse_is_available())
    {
        return -1;
    }
    vint_doc_info* info = ui_get_doc_info(docHandle);
    if (info == nullptr || info->m_doc == nullptr)
    {
        return -1;
    }
    for (int i = 0; i < numHints; i++)
    {
        const char* name = hintNames[i];
        if (name == nullptr || *name == '\0')
        {
            continue;
        }
        vint_object_base* hint = vint_object_base::find_object(name, info->m_doc);
        if (hint == nullptr)
        {
            continue;
        }
        vint_variant visible;
        hint->get_property_value("visible", visible, 0);
        if (!(bool)visible)
        {
            continue;
        }
        if (ui_is_mouse_over_vint_object(hint))
        {
            if (doHighlight)
            {
                ui_set_float_property(hint, "alpha", 1.0f);
            }
            if (mouse_just_released(0))
            {
                return i;
            }
        }
        else if (doHighlight)
        {
            ui_set_float_property(hint, "alpha", 0.5f);
        }
    }
    return -1;
}

// 0x008C8160 (:4198)
// The menu item under the mouse in a column of items (selected when it changes), -1 otherwise.
int ui_mouse_over_menu_item(int menu_handle, float left, float top, float item_height, float item_width)
{
    if (!ui_mouse_is_available())
    {
        return -1;
    }
    const int count = ui_menu_get_visible_item_count(menu_handle);
    if (count > 0 && item_height > 0.0f && left <= Mouse_scaled_x && Mouse_scaled_x < left + item_width && top <= Mouse_scaled_y &&
        Mouse_scaled_y <= (float)count * item_height + top)
    {
        const int index = ui_menu_get_index_by_location(menu_handle, (int)((Mouse_scaled_y - top) / item_height));
        if (index < 0)
        {
            return -1;
        }
        if (ui_menu_get_selected_item(menu_handle) != index)
        {
            ui_menu_select_item_by_index(menu_handle, index);
        }
        return index;
    }
    return -1;
}

// 0x0089C240 (:4230)
// The object's screen size in the interface space (-1, -1 when the cursor is hidden).
void ui_get_vint_obj_size(vint_object_base* obj, float& x, float& y)
{
    vint_variant value;
    const vector2 size = ui_get_vector2_property(obj, "screen_size", value);
    x = size.x;
    y = size.y;
    vector2 offset;
    vector2 scale;
    ui_get_obj_res_transform(obj, offset, scale);
    x = Mouse_x_scale * scale.x * x;
    y = Mouse_y_scale * scale.y * y;
    if (!Ui_cursor_visible)
    {
        x = -1.0f;
        y = -1.0f;
    }
}

// 0x0089C3C0 (:4261)
// The object's screen position in the interface space ((0, 0) when the cursor is hidden).
void ui_get_vint_obj_position(vint_object_base* obj, float& x, float& y)
{
    vint_variant value;
    const vector2 nw = ui_get_vector2_property(obj, "screen_nw", value);
    x = nw.x;
    y = nw.y;
    vector2 offset;
    vector2 scale;
    ui_get_obj_res_transform(obj, offset, scale);
    x = Mouse_x_scale * scale.x * x;
    y = Mouse_y_scale * scale.y * y;
    x = Mouse_x_scale * offset.x + x;
    y = Mouse_y_scale * offset.y + y;
    if (!Ui_cursor_visible)
    {
        x = 0.0f;
        y = 0.0f;
    }
}

// 0x008B5250 (:4294)
bool ui_vint_object_visible(const char* objectName, vint_document* doc)
{
    if (objectName != nullptr && doc != nullptr)
    {
        return ui_vint_object_visible(vint_object_base::find_object(objectName, doc));
    }
    return false;
}

// 0x0089C590 (:4302)
bool ui_vint_object_visible(vint_object_base* obj)
{
    if (obj == nullptr)
    {
        return false;
    }
    vint_variant visible;
    obj->get_property_value("visible", visible, 0);
    return ui_variant_visible(visible);
}

// 0x008B5280 (:4313)
void ui_set_vint_object_visible(const char* objectName, vint_document* doc, bool visible)
{
    if (objectName != nullptr && doc != nullptr)
    {
        vint_object_base* obj = vint_object_base::find_object(objectName, doc);
        if (obj != nullptr)
        {
            obj->set_property_value("visible", vint_variant(visible), 0);
        }
    }
}

// 0x0089C670 (:4320)
void ui_set_vint_object_visible(vint_object_base* obj, bool visible)
{
    if (obj != nullptr)
    {
        obj->set_property_value("visible", vint_variant(visible), 0);
    }
}

// The cloned menu item object's name: "name" for the first, "name<index + 1>" after. The documents' Lua numbers its
// clones from 1 and leaves the first object as it is ("text_group", "text_group2", ...: rfg_ui_main_menu.lua
// mm_group_add), so there is no "name1". rfg.exe inlines this with ui_menu_get_cloned_name's isOneBased = true.
static void ui_menu_item_object_name(char* result, unsigned int size, const char* name, int index)
{
    if (index == 0)
    {
        keen::formatString(result, size, "%s", name);
    }
    else
    {
        keen::formatString(result, size, "%s%d", name, index + 1);
    }
}

// 0x008DAF40 (:4332)
// On a mouse move or release: the first enabled, visible menu item whose child object item_name (in the group's
// clones) is under the mouse becomes the selection; its index, -1 otherwise.
int ui_menu_process_mouseover(int docHandle, int menu_handle, const char* group_name, const char* item_name)
{
    if (!ui_mouse_is_available() || (!mouse_just_moved() && !mouse_just_released(0)))
    {
        return -1;
    }
    vint_doc_info* info = ui_get_doc_info(docHandle);
    if (info == nullptr || info->m_doc == nullptr)
    {
        return -1;
    }
    const int count = ui_menu_get_total_item_count(menu_handle);
    for (int index = 0; index < count; index++)
    {
        char name[128];
        ui_menu_item_object_name(name, sizeof(name), group_name, index);
        vint_object_base* group = vint_object_base::find_object(name, info->m_doc);
        if (group == nullptr)
        {
            continue;
        }
        vint_object_base* obj = group->find_child(crc_str(item_name, 0));
        if (obj != nullptr && ui_is_mouse_over_vint_object(obj) && ui_menu_is_menu_item_enabled(menu_handle, index) &&
            ui_menu_is_menu_item_visible(menu_handle, index))
        {
            ui_menu_handle_mouseover(menu_handle, index);
            return index;
        }
    }
    return -1;
}

// 0x008DB0C0 (:4373)
// As above with the item objects themselves (clones of item_name), on a move, release or double click.
int ui_menu_process_mouseover(int docHandle, int menu_handle, const char* item_name)
{
    if (!ui_mouse_is_available())
    {
        return -1;
    }
    if (!mouse_just_moved() && !mouse_just_released(0) && !mouse_double_clicked(0))
    {
        return -1;
    }
    vint_doc_info* info = ui_get_doc_info(docHandle);
    if (info == nullptr || info->m_doc == nullptr)
    {
        return -1;
    }
    const int count = ui_menu_get_total_item_count(menu_handle);
    for (int index = 0; index < count; index++)
    {
        char name[128];
        ui_menu_item_object_name(name, sizeof(name), item_name, index);
        vint_object_base* obj = vint_object_base::find_object(name, info->m_doc);
        if (obj != nullptr && ui_is_mouse_over_vint_object(obj) && ui_menu_is_menu_item_enabled(menu_handle, index) &&
            ui_menu_is_menu_item_visible(menu_handle, index))
        {
            ui_menu_handle_mouseover(menu_handle, index);
            return index;
        }
    }
    return -1;
}

// ui_menu's members as rfg.exe's inlined menu lookups read them (private in ui_menu.h).
// Magic numbers: m_handle 0x10, m_selected 0x1C, m_num_items 0x20, m_items 0x24; the item's m_enabled 0x06,
// m_handle 0x08.
extern farray<ui_menu*,8> Menus; // 0x01667698, ui_menu.cpp

static ui_menu* ui_find_menu(int menu_handle)
{
    for (int i = 0; i < Menus.size(); i++)
    {
        if (*(const int*)((const unsigned char*)Menus[i] + 0x10) == menu_handle)
        {
            return Menus[i];
        }
    }
    return nullptr;
}

static ui_menu_item_base* ui_menu_item(ui_menu* menu, unsigned int index)
{
    return ((ui_menu_item_base* const*)((const unsigned char*)menu + 0x24))[index];
}

// 0x008DB210 (:4416)
// As ui_menu_process_mouseover with the items' extents: each item's box is its object's position, the height of its
// size and the widest item's width (kept in partial_extents); force_check skips the move/release test.
int ui_menu_process_mouseover_extend(int docHandle, int menu_handle, const char* group_name, const char* item_name, bool force_check)
{
    static bbox2d partial_extents[16]; // 0x02CA1540
    if (!ui_mouse_is_available())
    {
        return -1;
    }
    if (!force_check && !mouse_just_moved() && !mouse_just_released(0))
    {
        return -1;
    }
    float widest = 0.0f;
    vint_doc_info* info = ui_get_doc_info(docHandle);
    if (info == nullptr || info->m_doc == nullptr)
    {
        return -1;
    }
    const int count = ui_menu_get_total_item_count(menu_handle);
    for (int index = 0; index < count; index++)
    {
        char name[128];
        ui_menu_item_object_name(name, sizeof(name), group_name, index);
        vint_object_base* obj = vint_object_base::find_object(name, info->m_doc);
        if (obj == nullptr)
        {
            continue;
        }
        if (item_name != nullptr)
        {
            obj = obj->find_child(crc_str(item_name, 0));
            if (obj == nullptr)
            {
                continue;
            }
        }
        bbox2d& box = partial_extents[index];
        ui_get_vint_obj_position(obj, box.min.x, box.min.y);
        float width;
        ui_get_vint_obj_size(obj, width, box.max.y);
        box.max.y = box.min.y + box.max.y;
        if (widest < width)
        {
            widest = width;
        }
    }
    for (int index = 0; index < count; index++)
    {
        bbox2d& box = partial_extents[index];
        box.max.x = box.min.x + widest;
        const float out = (Mouse_scaled_x < box.min.x ? 1.0f : 0.0f) + (Mouse_scaled_y < box.min.y ? 1.0f : 0.0f) +
                          (box.max.x < Mouse_scaled_x ? 1.0f : 0.0f) + (box.max.y < Mouse_scaled_y ? 1.0f : 0.0f);
        if (out != 0.0f)
        {
            continue;
        }
        ui_menu* menu = ui_find_menu(menu_handle);
        if (menu == nullptr)
        {
            continue;
        }
        const unsigned int num_items = *(const unsigned int*)((const unsigned char*)menu + 0x20);
        if ((unsigned int)index >= num_items)
        {
            continue;
        }
        // The item by its handle (rfg.exe looks the index's handle up again).
        const int item_handle = *(const int*)((const unsigned char*)ui_menu_item(menu, index) + 0x08);
        if (item_handle == -1)
        {
            continue;
        }
        for (unsigned int i = 0; i < num_items; i++)
        {
            ui_menu_item_base* item = ui_menu_item(menu, i);
            if (*(const int*)((const unsigned char*)item + 0x08) == item_handle)
            {
                if (*(const bool*)((const unsigned char*)item + 0x06) && ui_menu_is_menu_item_visible(menu_handle, index))
                {
                    ui_menu_handle_mouseover(menu_handle, index);
                    return index;
                }
                break;
            }
        }
    }
    return -1;
}

// 0x008C8270 (:4477)
// The child object item_name of each of the group's clones.
void ui_menu_collect_obj_handles(int docHandle, int menu_handle, const char* group_name, const char* item_name, vint_object_base** vint_objects, int item_count)
{
    vint_doc_info* info = ui_get_doc_info(docHandle);
    if (info == nullptr || info->m_doc == nullptr)
    {
        return;
    }
    for (int index = 0; index < item_count; index++)
    {
        char name[128];
        ui_menu_item_object_name(name, sizeof(name), group_name, index);
        vint_object_base* group = vint_object_base::find_object(name, info->m_doc);
        if (group != nullptr)
        {
            vint_object_base* obj = group->find_child(crc_str(item_name, 0));
            if (obj != nullptr)
            {
                vint_objects[index] = obj;
            }
        }
    }
}

// 0x008C8370 (:4500)
void ui_menu_handle_mouseover(int menu_handle, int index)
{
    ui_menu* menu = ui_find_menu(menu_handle);
    const int selected = menu != nullptr ? *(const int*)((const unsigned char*)menu + 0x1C) : -1;
    if (selected != index)
    {
        ui_menu_select_item_by_index(menu_handle, index);
    }
}

// 0x0089C6B0 (:4509)
void ui_menu_get_cloned_name(char* result, unsigned int max_length, const char* initial_name, int index, bool isOneBased)
{
    if (index == 0)
    {
        keen::formatString(result, max_length, "%s", initial_name);
    }
    else
    {
        keen::formatString(result, max_length, "%s%d", initial_name, index + (isOneBased ? 1 : 0));
    }
}

// 0x0089C700 (:4519)
vint_object_base* ui_get_parented_vint_object(const char* parent_obj_name, const char* object_name, vint_document* doc)
{
    if (doc == nullptr)
    {
        return nullptr;
    }
    vint_object_base* parent = vint_object_base::find_object(parent_obj_name, doc);
    if (parent == nullptr)
    {
        return nullptr;
    }
    return parent->find_child(crc_str(object_name, 0));
}

// 0x008C83C0 (:4529)
bool ui_mouse_over_hint(const char* hint_text, int docHandle, bool do_highlight)
{
    if (!ui_mouse_is_available() || hint_text == nullptr)
    {
        return false;
    }
    vint_doc_info* info = ui_get_doc_info(docHandle);
    if (info != nullptr && info->m_doc != nullptr)
    {
        return ui_mouse_over_hint(vint_object_base::find_object(hint_text, info->m_doc), docHandle, do_highlight);
    }
    return false;
}

// 0x008B52E0 (:4542)
// A hint under the mouse: alpha 1 with highlight, true when the mouse releases on it; elsewhere alpha 0.5.
bool ui_mouse_over_hint(vint_object_base* hint_text, int docHandle, bool do_highlight)
{
    if (!ui_mouse_is_available() || hint_text == nullptr)
    {
        return false;
    }
    vint_variant nw_value;
    vint_variant se_value;
    const vector2 nw = ui_get_vector2_property(hint_text, "screen_nw", nw_value);
    const vector2 se = ui_get_vector2_property(hint_text, "screen_se", se_value);
    vector2 offset;
    vector2 scale;
    ui_get_obj_res_transform(hint_text, offset, scale);
    vector2 min(nw.x * Mouse_x_scale * scale.x + Mouse_x_scale * offset.x, nw.y * Mouse_y_scale * scale.y + Mouse_y_scale * offset.y);
    float max_x = se.x * Mouse_x_scale * scale.x + Mouse_x_scale * offset.x;
    float max_y = se.y * Mouse_y_scale * scale.y + Mouse_y_scale * offset.y;
    if (!Ui_cursor_visible)
    {
        min.zero();
        max_x = -1.0f;
        max_y = -1.0f;
    }
    if (Mouse_scaled_x < min.x || max_x < Mouse_scaled_x || Mouse_scaled_y < min.y || max_y < Mouse_scaled_y)
    {
        ui_set_float_property(hint_text, "alpha", 0.5f);
        return false;
    }
    if (do_highlight)
    {
        ui_set_float_property(hint_text, "alpha", 1.0f);
    }
    return mouse_just_released(0);
}

// 0x008C8420 (:4611)
bool ui_mouse_over_hint_combo(const char* hint_image, const char* hint_text, int docHandle, bool do_highlight)
{
    if (!ui_mouse_is_available() || hint_image == nullptr || hint_text == nullptr)
    {
        return false;
    }
    vint_doc_info* info = ui_get_doc_info(docHandle);
    if (info == nullptr || info->m_doc == nullptr)
    {
        return false;
    }
    return ui_mouse_over_hint_combo(vint_object_base::find_object(hint_image, info->m_doc),
                                    vint_object_base::find_object(hint_text, info->m_doc), docHandle, do_highlight);
}

// The object's screen box in the interface space.
static void ui_get_obj_interface_box(vint_object_base* obj, const vector2& nw, const vector2& se, vector2& min, vector2& max)
{
    vector2 offset;
    vector2 scale;
    ui_get_obj_res_transform(obj, offset, scale);
    min.x = nw.x * Mouse_x_scale * scale.x + offset.x * Mouse_x_scale;
    min.y = nw.y * scale.y * Mouse_y_scale + offset.y * Mouse_y_scale;
    max.x = se.x * Mouse_x_scale * scale.x + offset.x * Mouse_x_scale;
    max.y = se.y * scale.y * Mouse_y_scale + offset.y * Mouse_y_scale;
}

// 0x008B56A0 (:4627)
// A hint of an image and a text (when the image is visible): the union of their boxes, as ui_mouse_over_hint for both.
bool ui_mouse_over_hint_combo(vint_object_base* hint_image, vint_object_base* hint_text, int docHandle, bool do_highlight)
{
    if (!ui_mouse_is_available() || hint_image == nullptr || hint_text == nullptr)
    {
        return false;
    }
    vint_variant visible;
    hint_image->get_property_value("visible", visible, 0);
    if (!(bool)visible)
    {
        return false;
    }
    vint_variant value;
    hint_image->get_property_value("screen_nw", value, 0);
    const vint_variant_vector2f image_nw = (vint_variant_vector2f)value;
    hint_image->get_property_value("screen_se", value, 0);
    const vint_variant_vector2f image_se = (vint_variant_vector2f)value;
    hint_text->get_property_value("screen_nw", value, 0);
    const vint_variant_vector2f text_nw = (vint_variant_vector2f)value;
    hint_text->get_property_value("screen_se", value, 0);
    const vint_variant_vector2f text_se = (vint_variant_vector2f)value;
    vector2 image_min;
    vector2 image_max;
    ui_get_obj_interface_box(hint_image, vector2(image_nw.x, image_nw.y), vector2(image_se.x, image_se.y), image_min, image_max);
    vector2 min;
    vector2 max;
    ui_get_obj_interface_box(hint_text, vector2(text_nw.x, text_nw.y), vector2(text_se.x, text_se.y), min, max);
    if (image_min.y - min.y < 0.0f)
    {
        min.y = image_min.y;
    }
    if (image_min.x - min.x < 0.0f)
    {
        min.x = image_min.x;
    }
    if (image_max.y - max.y >= 0.0f)
    {
        max.y = image_max.y;
    }
    if (image_max.x - max.x >= 0.0f)
    {
        max.x = image_max.x;
    }
    if (!Ui_cursor_visible)
    {
        min.zero();
        max.x = -1.0f;
        max.y = -1.0f;
    }
    if (Mouse_scaled_x < min.x || max.x < Mouse_scaled_x || Mouse_scaled_y < min.y || max.y < Mouse_scaled_y)
    {
        ui_set_float_property(hint_text, "alpha", 0.5f);
        ui_set_float_property(hint_image, "alpha", 0.5f);
        return false;
    }
    if (do_highlight)
    {
        ui_set_float_property(hint_text, "alpha", 1.0f);
        ui_set_float_property(hint_image, "alpha", 1.0f);
    }
    return mouse_just_released(0);
}

// 0x008C8490 (:4728)
// The named objects (false at the first missing one).
bool ui_pc_collect_vint_obj_handles_by_name(const char** names, int count, vint_object_base** vint_objects, int doc_handle)
{
    vint_doc_info* info = ui_get_doc_info(doc_handle);
    if (info == nullptr || info->m_doc == nullptr)
    {
        return false;
    }
    for (int i = 0; i < count; i++)
    {
        vint_object_base* obj = vint_object_base::find_object(names[i], info->m_doc);
        if (obj == nullptr)
        {
            return false;
        }
        vint_objects[i] = obj;
    }
    return true;
}

// 0x008B5DE0 (:4749)
// A pulsing alpha (0.25..1 at 2 rad/s of game time) on the objects. Original: opacity is not used.
void ui_pc_process_doc_object_opacity(vint_object_base** objects, int count, float opacity)
{
    static float elapsedTime; // 0x02CA1504
    static float kAnimationSpeed = 2.0f; // 0x01667F14
    elapsedTime = Frametime + elapsedTime;
    const float pulse = (float)cos((double)(kAnimationSpeed * elapsedTime));
    for (int i = 0; i < count; i++)
    {
        vint_object_base* obj = objects[i];
        if (obj != nullptr)
        {
            ui_pc_set_object_opacity(obj, (pulse + 1.0f) * 0.5f * 0.75f + 0.25f);
        }
    }
}

// 0x0089C740 (:4769)
void ui_pc_set_object_opacity(vint_object_base* obj, float opacity)
{
    static unsigned int crc_alpha = crc_str("alpha", 0);
    if (obj != nullptr)
    {
        obj->set_property_value(crc_alpha, vint_variant(opacity), 0);
    }
}

// 0x0089C7B0 (:4777)
void ui_pc_set_object_visible(vint_object_base* obj, bool visible)
{
    static unsigned int crc_visible = crc_str("visible", 0);
    if (obj != nullptr)
    {
        obj->set_property_value(crc_visible, vint_variant(visible), 0);
    }
}

// 0x0089C820 (:4785)
void ui_pc_set_object_color(vint_object_base* obj, const color& tint)
{
    if (obj != nullptr)
    {
        vint_variant value;
        value.set_type(VINT_PROP_TYPE_COLOR);
        value.set_color((float)tint.red / 255.0f, (float)tint.green / 255.0f, (float)tint.blue / 255.0f);
        obj->set_property_value(crc_str("tint", 0), value, 0);
    }
}

// 0x0089C8C0 (:4794)
bool ui_show_hint_icons()
{
    return Show_hint_icons;
}

// 0x0089C8D0 (:4799)
void ui_set_hint_icons_enabled(bool enabled)
{
    Show_hint_icons = enabled;
}

// 0x008B5EC0 (:4806)
// The visible clickable objects: the hovered one white at alpha 1, the others orange at a pulsing alpha (with
// do_animation); the one the mouse releases on (its index), -1 otherwise.
int ui_mouse_process_clickables(vint_object_base** vint_objects, int count, bool do_animation)
{
    static float kCurrentOpacity = Mouse_highlight_min_opacity;
    static color kHighlightTint = color::WHITE;
    static color kStandardTint = color::ORANGE;
    static float elapsedTime; // 0x02CA1508
    if (!ui_mouse_is_available())
    {
        return -1;
    }
    elapsedTime = Frametime + elapsedTime;
    const float pulse = (float)cos((double)(Mouse_highlight_anim_speed * elapsedTime));
    int clicked = -1;
    const float opacity = (pulse + 1.0f) * 0.5f * (Mouse_highlight_max_opacity - Mouse_highlight_min_opacity) + Mouse_highlight_min_opacity;
    for (int i = 0; i < count; i++)
    {
        vint_object_base* obj = vint_objects[i];
        if (obj == nullptr)
        {
            continue;
        }
        vint_variant visible;
        obj->get_property_value("visible", visible, 0);
        if (!ui_variant_visible(visible))
        {
            continue;
        }
        if (ui_is_mouse_over_vint_object(obj))
        {
            if (mouse_just_released(0))
            {
                clicked = i;
            }
            if (do_animation)
            {
                ui_pc_set_object_opacity(obj, 1.0f);
                ui_pc_set_object_color(obj, kHighlightTint);
            }
        }
        else if (do_animation)
        {
            ui_pc_set_object_opacity(obj, opacity);
            ui_pc_set_object_color(obj, kStandardTint);
        }
    }
    return clicked;
}

// 0x0089C8E0 (:4854)
void ui_set_vint_property_on_multiple_objects(vint_object_base** objects, int count, const char* property_name, vint_variant property_value)
{
    for (int i = 0; i < count; i++)
    {
        if (objects[i] != nullptr)
        {
            objects[i]->set_property_value(property_name, property_value, 0);
        }
    }
}
