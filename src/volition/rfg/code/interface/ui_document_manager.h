// Original: volition/rfg/code/interface/ui_document_manager.h
// Implementation: volition/rfg/code/interface/ui_document_manager.cpp
//
// The document manager: load and unload by name (doc_load_type), fades in and out, the documents of each control id,
// ui_process and ui_render (once per frame for the control id on top of Controller_slots), the input requests of the
// documents, and ui_init. Placement of input_register_type inferred.
#pragma once

#include "volition/rfg/code/control/control_map.h"
#include "volition/stream2/code/stream2.h"
#include "volition/vlib/code/memory/object_pool.h"
#include "volition/vlib/code/time/timestamp.h"
#include "volition/vlib/code/util/varray.h"

struct bbox2d;
struct cfile;
class color;
class vector2i;
class vint_document;
class vint_object_base;
class vint_variant;

enum doc_load_type
{
    DLT_NORMAL = 0,
    DLT_SYNCHRONOUS_HARDLOAD = 1,
    DLT_PERMANENTLY_LOAD = 2,
};

enum doc_fadein_type
{
    DFIT_NONE = 0,
    DFIT_ALPHA = 1,
    DFIT_FADE_FROM_BLACK = 2,
    DFIT_LAST = 3,
};

enum doc_fadeout_type
{
    DFOT_NONE = 0,
    DFOT_ALPHA = 1,
    DFOT_FADE_TO_BLACK = 2,
    DFOT_LAST = 3,
};

enum button_choices
{
    BC_MAP = 0,
    BC_UP = 1,
    BC_DOWN = 2,
    BC_LEFT = 3,
    BC_RIGHT = 4,
    BC_ALT_UP = 5,
    BC_ALT_DOWN = 6,
    BC_ALT_LEFT = 7,
    BC_ALT_RIGHT = 8,
    BC_START = 9,
    BC_SELECT = 10,
    BC_ABORT = 11,
    BC_PGUP = 12,
    BC_PGDOWN = 13,
    BC_X = 14,
    BC_Y = 15,
    BC_L1 = 16,
    BC_L2 = 17,
    BC_R1 = 18,
    BC_R2 = 19,
    BC_DPAD_UP = 20,
    BC_DPAD_DOWN = 21,
    BC_DPAD_LEFT = 22,
    BC_DPAD_RIGHT = 23,
    BC_SLOT0 = 24,
    BC_SLOT1 = 25,
    BC_SLOT2 = 26,
    BC_SLOT3 = 27,
    BC_START_GAME = 28,
    BC_GAME_PAUSE = 29,
    BC_DELETE_SAVEGAME = 30,
    BC_FSM_PLACE_BEACON = 31,
    BC_VIEW_GAMERCARD = 32,
    BC_NUM_CHOICES = 33,
};

enum input_register_type
{
    IRT_UNLIMITED_INPUT = 0,
    IRT_INVALID = 1,
};

// Placement inferred (TU-private in rfg.pdb; loading.cpp holds Input_types).
// size 0xC
struct input_request_data
{
    input_register_type type; // 0x00
    int id; // 0x04
    bool locked; // 0x08
};

// Placement inferred (TU-private in rfg.pdb; the main menu's attract check reads Button_info, UI.md UI4).
// size 0x18
struct button_info
{
    signed char m_button_choice; // 0x00
    int m_controller_id; // 0x04
    float m_button_input; // 0x08
    float m_button_actual_input; // 0x0C
    float m_button_repeat_time; // 0x10
    bool m_button_beyond_init; // 0x14
    bool m_button_wait_for_up; // 0x15
};

// size 0x31C
struct ui_button_info
{
    bool m_received_input; // 0x00
    bool m_button_pressed; // 0x01
    button_info binfo[33]; // 0x04
};

extern int Choice_last; // 0x01664B38
extern ui_button_info Button_info; // 0x02BF1E30 (file static in the binary; external since UI4)

// size 0x140
struct vint_doc_info
{
    void clear(); // 0x008B4A30 (:2002)
    void release_data_groups(); // 0x008B48F0 (:1946)
    void release_data_items(); // 0x008B4950 (:1964)
    void release_data_events(); // 0x008B49C0 (:1983)
    vint_doc_info(); // 0x008C76D0 (:1939)

    char m_name[64]; // 0x00
    int m_handle; // 0x40
    bool m_paused; // 0x44
    vint_document* m_doc; // 0x48
    farray<unsigned int,16> m_group_handles; // 0x4C
    farray<unsigned int,16> m_data_handles; // 0x9C
    farray<unsigned int,8> m_event_handles; // 0xEC
    bool is_fadein; // 0x11C
    int fade_type; // 0x120
    float fade_duration; // 0x124
    float fade_running_time; // 0x128
    unsigned int peg_srid; // 0x12C
    timestamp_realtime m_stream_timer; // 0x130
    bool m_assets_loaded; // 0x134
    bool m_create_doc; // 0x135
    doc_load_type m_load_type; // 0x138
    bool m_show_loading_msg; // 0x13C
};

// size 0x1490
struct vint_doc_info_set
{
    vint_doc_info_set(); // 0x008F60B0 (:140)
    ~vint_doc_info_set(); // 0x008C6DF0

    object_pool<vint_doc_info,16,1> m_doc_info_pool; // 0x00
};

extern bool Ui_unload_assets_only; // 0x0166411F
extern bool Ui_cursor_visible; // 0x01664FD3
extern bool fonts_are_initializing; // 0x02C08091
extern int Debug_font; // 0x02BF221C
extern unsigned int Num_bitmap_sheets_data_mark; // 0x02C08084
extern unsigned int Num_bitmap_images_data_mark; // 0x02C08088
extern unsigned int Num_sheets_data_mark; // 0x02C0808C

void unused_dcf_Test_crash_handling(); // 0x0089AC10 (:320)
void unused_dcf_Test_assert_handling(); // 0x0089AC20 (:328)
void unused_dcf_Test_text_scale(); // 0x008C6E00 (:335)
int ui_localize_string(char*, int, char*, ...); // 0x0089AC30 (:346)
void ui_update_resolution(int w, int h); // 0x008B40A0 (:471)
void ui_update_resolution_from_screen_size(); // 0x008B41E0 (:502)
float ui_get_menu_action_input(control_button_action choice, unsigned int controller_id); // 0x0089B3A0 (:703)
void ui_initialize_fonts(); // 0x0089B4D0 (:1094)
bool ui_are_fonts_initializing(); // 0x0089B8A0 (:1232)
void ui_reload_bitmat_sheet_table_file(int dlc_id); // 0x008C71A0 (:1254)
void ui_remove_handle_from_process_rendering(int handle); // 0x008B4860 (:1727)
int ui_get_usage_character_width(const char* input, int fontnum); // 0x008C77A0 (:2067)
int ui_get_usage_character_width(const wchar_t* input, int fontnum); // 0x008B4A80 (:2074)
int ui_get_usage_character_height(const char* input, int fontnum); // 0x008B4C70 (:2158)
int ui_get_usage_character_height(const wchar_t* input, int fontnum); // 0x0089BB10 (:2165)
void ui_init(); // 0x00910F40 (:2234)
void ui_init_stage_2(); // 0x008ED130 (:2315)
void ui_level_init_pre(); // 0x008DA940 (:2329)
void ui_level_init_post(); // 0x0089BC80 (:2337)
void ui_handshake(); // 0x008DA980 (:2343)
void ui_shutdown(); // 0x008F60D0 (:2394)
void ui_system_save(cfile* file); // 0x008C7800 (:2448)
void ui_system_load(cfile* file); // 0x008C7840 (:2462)
void ui_show_loading_screen(bool pause_music, const char* display_string); // 0x00904C60 (:2476)
void ui_remove_loading_screen(); // 0x008ED160 (:2488)
bool ui_loading_screen_is_open(); // 0x0089BCA0 (:2503)
int ui_load_document(const char* name, doc_fadein_type dft, float fadein_time, doc_load_type load_type, bool show_loading_msg, stream2_priority priority, bool pause_music); // 0x00904FE0 (:2510)
bool ui_unload_document(int handle, doc_fadeout_type dft, float fadeout_time); // 0x008ED1B0 (:2703)
void ui_add_for_processing(const farray<int,16>& doc_handles); // 0x008DAB40 (:2774)
void ui_add_for_processing(int doc_handle); // 0x008C7880 (:2785)
void ui_add_for_rendering(const farray<int,16>& doc_handles); // 0x008DABD0 (:2809)
void ui_add_for_rendering(int doc_handle); // 0x008C78E0 (:2820)
void ui_process(unsigned int control_id, bool input_only); // 0x009110B0 (:2844)
bool ui_render(unsigned int control_id, bool skip_doc_list); // 0x008F61D0 (:2997)
unsigned int ui_request_control_id(); // 0x008C7940 (:3096)
bool ui_relinquish_control_id(unsigned int id); // 0x008B4CD0 (:3111)
unsigned int ui_get_current_control_id(); // 0x008B4D20 (:3125)
void ui_set_max_frametime(float max); // 0x0089BCB0 (:3135)
bool ui_add_data_group(int doc_handle, int data_group_handle); // 0x008C79A0 (:3147)
bool ui_remove_data_group(int doc_handle, int data_group_handle); // 0x008C7A30 (:3181)
bool ui_add_data_item(int doc_handle, int data_item_handle); // 0x008C7A70 (:3202)
bool ui_add_data_event(int doc_handle, int data_event_handle); // 0x008C7B10 (:3236)
void ui_notify_doc_assets_loaded(unsigned int srid); // 0x008ED350 (:3270)
int ui_get_doc_handle(const char* name); // 0x008C7BB0 (:3332)
int ui_get_doc_handle_from_vint_doc_handle(int vint_doc_handle); // 0x008C7BD0 (:3344)
vint_document* ui_get_vint_document(int handle); // 0x008C7C60 (:3359)
bool ui_has_received_input(); // 0x0089BCE0 (:3370)
bool ui_has_button_been_pressed(); // 0x0089BD10 (:3379)
float ui_get_button_info(int which_btn, int input_id); // 0x008C7C80 (:3396)
float ui_get_button_info_and_wait(int which_btn, int input_id); // 0x008C7D00 (:3414)
int ui_get_button_controller_id(int which_btn); // 0x0089BD20 (:3424)
float ui_get_button_raw(int which_btn); // 0x0089BD30 (:3430)
void ui_wait_for_button_ups(); // 0x0089BD60 (:3443)
void ui_wait_for_button_ups(int which_btn); // 0x0089BD80 (:3453)
int ui_get_font(const char* font_name); // 0x0089BDA0 (:3461)
const vector2i& ui_get_screen_dimensions(); // 0x0089BDB0 (:3484)
float ui_get_aspect_ratio(); // 0x0089BDC0 (:3490)
void ui_set_aspect_ratio(float aspect_ratio); // 0x0089BDD0 (:3496)
float ui_get_pixel_aspect_ratio(); // 0x0089BDE0 (:3502)
void ui_set_pixel_aspect_ratio(float aspect_ratio); // 0x0089BDF0 (:3508)
void ui_set_pause_state_for_doc(int handle, bool state); // 0x008C7D40 (:3515)
bool ui_get_pause_state_for_doc(int handle); // 0x008C7D60 (:3524)
void ui_force_unloads_complete(); // 0x008C7D80 (:3535)
int ui_register_for_processed_input(input_register_type type, bool lock_input); // 0x008C7E30 (:3554)
void ui_relinquish_input_id(int input_id); // 0x008B4D40 (:3587)
int ui_get_current_input_id(); // 0x008B4DF0 (:3617)
int ui_get_input_locked_status(); // 0x0089BE00 (:3631)
void ui_enable_processed_input(); // 0x0089BE10 (:3637)
void ui_disable_processed_input(); // 0x0089BE20 (:3643)
void ui_set_screen_blur(bool on, bool pause_music); // 0x0089BE30 (:3649)
bool ui_get_screen_blurring(); // 0x0089BEA0 (:3688)
void ui_unload_doc_assets(int handle); // 0x008C7EC0 (:3694)
void ui_reload_doc_assets(int handle); // 0x008DAC60 (:3713)
bool ui_fade_in_doc(int handle, doc_fadein_type type, float fadein_time); // 0x008ED4C0 (:3738)
stream2_status_val ui_get_doc_asset_state(int handle); // 0x008DACC0 (:3764)
bool ui_request_image(const char* img_filename); // 0x0089BEB0 (:3866)
stream2_status_val ui_get_image_status(const char* img_filename); // 0x0089BEE0 (:3880)
bool ui_release_image(const char* img_filename); // 0x0089BF10 (:3895)
void ui_destroy_doc_data_info(int handle, bool destroy_data_items, bool destroy_data_groups, bool destroy_data_events); // 0x008C7F20 (:3914)
control_button_action ui_get_control_button_value(button_choices btn); // 0x0089BF50 (:3934)
void ui_fade_to_black_init(); // 0x0089BF70 (:3943)
void ui_fade_to_black_process(); // 0x0089BFA0 (:3951)
void ui_fade_to_black_render(); // 0x008B4E20 (:3976)
void ui_fade_to_black_fade_in(bool clear_ref_count); // 0x0089C040 (:4008)
void ui_fade_to_black_fade_out(bool pause); // 0x0089C080 (:4026)
void ui_fade_to_black_instant_fade_in(); // 0x0089C0C0 (:4039)
void ui_fade_to_black_instant_fade_out(); // 0x0089C0E0 (:4046)
bool ui_fade_to_black_is_faded(); // 0x0089C110 (:4052)
bool ui_fade_to_black_is_fade_in_complete(); // 0x0089C140 (:4053)
bool ui_is_mouse_over_vint_object(const char* objectName, vint_document* doc); // 0x008C7F60 (:4065)
bool ui_is_mouse_over_vint_object(const char* objectName, const char* parentName, vint_document* doc); // 0x008C7F80 (:4072)
bool ui_is_mouse_over_vint_object(vint_object_base* obj); // 0x008B4FD0 (:4084)
bool ui_is_mouse_over_extents(const bbox2d& extents); // 0x0089C160 (:4135)
int ui_mouse_process_hints(const char** hintNames, int numHints, int docHandle, bool doHighlight); // 0x008C7FD0 (:4143)
int ui_mouse_over_menu_item(int menu_handle, float left, float top, float item_height, float item_width); // 0x008C8160 (:4198)
void ui_get_vint_obj_size(vint_object_base* obj, float& x, float& y); // 0x0089C240 (:4230)
void ui_get_vint_obj_position(vint_object_base* obj, float& x, float& y); // 0x0089C3C0 (:4261)
bool ui_vint_object_visible(const char* objectName, vint_document* doc); // 0x008B5250 (:4294)
bool ui_vint_object_visible(vint_object_base* obj); // 0x0089C590 (:4302)
void ui_set_vint_object_visible(const char* objectName, vint_document* doc, bool visible); // 0x008B5280 (:4313)
void ui_set_vint_object_visible(vint_object_base* obj, bool visible); // 0x0089C670 (:4320)
int ui_menu_process_mouseover(int docHandle, int menu_handle, const char* group_name, const char* item_name); // 0x008DAF40 (:4332)
int ui_menu_process_mouseover(int docHandle, int menu_handle, const char* item_name); // 0x008DB0C0 (:4373)
int ui_menu_process_mouseover_extend(int docHandle, int menu_handle, const char* group_name, const char* item_name, bool force_check); // 0x008DB210 (:4416)
void ui_menu_collect_obj_handles(int docHandle, int menu_handle, const char* group_name, const char* item_name, vint_object_base** vint_objects, int item_count); // 0x008C8270 (:4477)
void ui_menu_handle_mouseover(int menu_handle, int index); // 0x008C8370 (:4500)
void ui_menu_get_cloned_name(char* result, unsigned int max_length, const char* initial_name, int index, bool isOneBased); // 0x0089C6B0 (:4509)
vint_object_base* ui_get_parented_vint_object(const char* parent_obj_name, const char* object_name, vint_document* doc); // 0x0089C700 (:4519)
bool ui_mouse_over_hint(const char* hint_text, int docHandle, bool do_highlight); // 0x008C83C0 (:4529)
bool ui_mouse_over_hint(vint_object_base* hint_text, int docHandle, bool do_highlight); // 0x008B52E0 (:4542)
bool ui_mouse_over_hint_combo(const char* hint_image, const char* hint_text, int docHandle, bool do_highlight); // 0x008C8420 (:4611)
bool ui_mouse_over_hint_combo(vint_object_base* hint_image, vint_object_base* hint_text, int docHandle, bool do_highlight); // 0x008B56A0 (:4627)
bool ui_pc_collect_vint_obj_handles_by_name(const char** names, int count, vint_object_base** vint_objects, int doc_handle); // 0x008C8490 (:4728)
void ui_pc_process_doc_object_opacity(vint_object_base** objects, int count, float opacity); // 0x008B5DE0 (:4749)
void ui_pc_set_object_opacity(vint_object_base* obj, float opacity); // 0x0089C740 (:4769)
void ui_pc_set_object_visible(vint_object_base* obj, bool visible); // 0x0089C7B0 (:4777)
void ui_pc_set_object_color(vint_object_base* obj, const color& tint); // 0x0089C820 (:4785)
bool ui_show_hint_icons(); // 0x0089C8C0 (:4794)
void ui_set_hint_icons_enabled(bool enabled); // 0x0089C8D0 (:4799)
int ui_mouse_process_clickables(vint_object_base** vint_objects, int count, bool do_animation); // 0x008B5EC0 (:4806)
void ui_set_vint_property_on_multiple_objects(vint_object_base** objects, int count, const char* property_name, vint_variant property_value); // 0x0089C8E0 (:4854)

// Seam: a file static of ui_document_manager.cpp in rfg.exe (LTCG: the handle in EDI), external here so the programs
// can stand in for it (null: no document).
vint_doc_info* ui_get_doc_info(int handle); // 0x008C7480 (:1826)

// The data item's m_updated (protected; 0x42C) as rfg.exe's inlined code writes it (vint_data_item's inline set). Shared
// by the UI5/UI6 documents.
inline void ui_data_item_set_updated(class vint_data_item* item)
{
    *(bool*)((unsigned char*)item + 0x42C) = true;
}
