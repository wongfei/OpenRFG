// Original: volition/rfg/code/interface/ui_menu.h
// Implementation: volition/rfg/code/interface/ui_menu.cpp
//
// The generic menu: items (function, char list, bool, increment), hints, the documents a menu belongs to, navigation
// and sounds. Placement of the enums and info structs inferred.
#pragma once

#include "volition/rfg/code/ai/vehicle/ai_vehicle_logic.h"
#include "volition/rfg/code/control/control_map.h"
#include "volition/rfg/code/interface/ui_document_manager.h"
#include "volition/vlib/code/util/varray.h"

class ui_menu;

// The menu sounds (cUI_menu_sound_name cues: 0 up, 1 down, 11 the "same" selection, ...). A file static of rfg.exe's
// interface unity, external since UI5 (the multiplayer menus play them).
extern int Audio_info[12]; // 0x02BF21D0

enum menu_item_type
{
    MI_INVALID = -1,
    MI_FUNCTION = 0,
    MI_CHAR_LIST = 1,
    MI_BOOL = 2,
    MI_INCR = 3,
    MI_LAST = 4,
};

enum menu_incr_type
{
    MIT_INVALID = -1,
    MIT_INT = 0,
    MIT_EXPONENTIAL = 1,
    MIT_SLIDER = 2,
    MIT_LAST = 3,
};

enum increment_type
{
    IT_UP = 0,
    IT_DOWN = 1,
};

enum menu_sound_type
{
    MENU_SOUND_MOVE_DOWN = 0,
    MENU_SOUND_MOVE_UP = 1,
    MENU_SOUND_SLIDER_ADD = 2,
    MENU_SOUND_SLIDER_SUBTRACT = 3,
    MENU_SOUND_PRESS_SELECT = 4,
    MENU_SOUND_PRESS_ABORT = 5,
    MENU_SOUND_PRESS_MISC = 6,
    MENU_SOUND_SCROLL_LEFT = 7,
    MENU_SOUND_SCROLL_RIGHT = 8,
    MENU_SOUND_BUMPER_LEFT = 9,
    MENU_SOUND_BUMPER_RIGHT = 10,
    MENU_SOUND_INVALID_INPUT = 11,
    NUM_MENU_SOUNDS = 12,
};

// size 0x4
union variable_type
{
    bool m_bool; // 0x00
    int m_int; // 0x00
};

// size 0x12C
struct hint_info
{
    hint_info(); // 0x008A08B0 (:122)
    void set_info(hint_type htype, control_button_action action, const char* text, const char* image); // 0x008A0900 (:130)

    hint_type type; // 0x00
    control_button_action button_action; // 0x04
    char hint_text[256]; // 0x08
    char hint_image_name[32]; // 0x108
    bool is_visible; // 0x128
};

// size 0x90
struct document_info
{
    document_info(); // 0x008CAFC0 (:151)
    void clear(); // 0x008B75F0 (:158)
    unsigned int get_num_hints(); // 0x008B7620 (:168)
    unsigned int get_num_populate_items(); // 0x008B7630 (:175)
    ~document_info(); // 0x008B75E0

    int needs_cleanup; // 0x00
    int doc_handle; // 0x04
    int data_handle_tween_on_select; // 0x08
    int data_handle_title; // 0x0C
    int data_handle_populate_group; // 0x10
    int data_handle_hint_group; // 0x14
    char event_surtag[8]; // 0x18
    farray<int,4> data_handle_hints; // 0x20
    farray<int,16> data_handle_populate_items; // 0x40
};

// size 0x94
class ui_menu_item_base
{
public:
    ui_menu_item_base(menu_item_type menu_type, const char* caption, ui_menu* parent); // 0x008B7C10 (:847)
    ui_menu_item_base(); // 0x008A0B50 (:841)
    virtual ~ui_menu_item_base(); // vtbl+0x00 0x00883690 (.h:161)
protected:
    void ui_menu_item_base_init(menu_item_type menu_type, const char* caption, ui_menu* parent); // 0x008A0B90 (:854)
public:
    bool get_selected() const;
    void set_selected(bool);
    void set_enabled(bool enabled); // 0x008836A0 (.h:170)
    bool get_enabled() const; // 0x008836B0 (.h:171)
    void set_visible(bool visible); // 0x008836C0 (.h:173)
    bool get_visible() const; // 0x008836D0 (.h:174)
    int get_handle(); // 0x008836E0 (.h:176)
    const char* get_caption() const; // 0x008836F0 (.h:178)
    const ui_menu* get_menu() const;
    menu_item_type get_menu_item_type(); // 0x00883700 (.h:181)
    virtual void get_text_value(char*, int) const = 0; // vtbl+0x04
    bool get_play_change_sound(); // 0x00883710 (.h:184)
    void set_play_change_sound(bool play); // 0x00883720 (.h:185)

    // 0x00 vfptr
protected:
    bool m_play_change_sound; // 0x04
    bool m_selected; // 0x05
    bool m_enabled; // 0x06
    bool m_visible; // 0x07
    int m_handle; // 0x08
    menu_item_type m_type; // 0x0C
    char m_caption[128]; // 0x10
    ui_menu* m_parent; // 0x90
};

// size 0x9C
class ui_menu_item_func : public ui_menu_item_base
{
public:
    ui_menu_item_func(const char* caption, ui_menu* parent, void (*scb)(int)); // 0x008B7C80 (:880)
    ui_menu_item_func(); // 0x008A0BE0 (:875)
    virtual ~ui_menu_item_func(); // vtbl+0x00 0x00883750 (.h:207)
    void ui_menu_item_func_init(const char* caption, ui_menu* parent, void (*scb)(int)); // 0x008A0C40 (:886)
    void set_should_tween_out(bool value); // 0x008A0CB0 (:895)
    bool get_should_tween_out(); // 0x008A0CC0 (:901)
    void set_surpress_accept_sound(bool value); // 0x008A0CD0 (:908)
    bool get_surpress_accept_sound(); // 0x008A0CE0 (:914)
    void set_should_set_active_controller(bool value); // 0x008A0D20 (:938)
    bool get_should_set_active_controller(); // 0x008A0D30 (:944)
    virtual void handle_selection(); // vtbl+0x08 0x008B7CE0 (:920)
    virtual void get_text_value(char* buff, int buff_size) const; // vtbl+0x04 0x008A0CF0 (:930)

protected:
    void (*m_selection_callback)(int); // 0x94
    bool m_should_tween_out; // 0x98
    bool m_surpress_accept_sound; // 0x99
    bool m_set_active_controller; // 0x9A
};

// size 0xA8
class ui_menu_item_char_list : public ui_menu_item_base
{
public:
    ui_menu_item_char_list(const char* caption, ui_menu* parent, const char** char_list, int num_chars, void (*iccb)(int, void*)); // 0x008B7D50 (:961)
    ui_menu_item_char_list(); // 0x008A0D40 (:956)
    virtual ~ui_menu_item_char_list(); // vtbl+0x00 0x00883760 (.h:237)
    void ui_menu_item_char_list_init(const char* caption, ui_menu* parent, const char** char_list, int num_chars, void (*iccb)(int, void*), int selected_idx); // 0x008A0DB0 (:967)
    virtual bool handle_input(increment_type type); // vtbl+0x08 0x008B7DC0 (:990)
    virtual void get_text_value(char* buff, int buff_size) const; // vtbl+0x04 0x008A0E50 (:1015)
    void set_selected_char(int index); // 0x008A0EB0 (:1029)
    int get_num_chars(); // 0x00883770 (.h:245)

protected:
    int m_which_char; // 0x94
    int m_num_chars; // 0x98
    const char** m_variable; // 0x9C
    void (*m_selection_callback)(int); // 0xA0
    void (*m_item_change_callback)(int, void*); // 0xA4
};

// size 0x9C
class ui_menu_item_bool : public ui_menu_item_base
{
public:
    ui_menu_item_bool(const char* caption, ui_menu* parent, void* variable, void (*iccb)(int, void*)); // 0x008B7E40 (:1046)
    ui_menu_item_bool(); // 0x008A0ED0 (:1041)
    virtual ~ui_menu_item_bool(); // vtbl+0x00 0x00883780 (.h:264)
    void ui_menu_item_bool_init(const char* caption, ui_menu* parent, void* variable, void (*iccb)(int, void*)); // 0x008A0F40 (:1052)
    virtual bool handle_input(increment_type type); // vtbl+0x08 0x008B7EA0 (:1062)
    virtual void get_text_value(char* buff, int buff_size) const; // vtbl+0x04 0x008A0FB0 (:1076)

protected:
    void* m_variable; // 0x94
    void (*m_item_change_callback)(int, void*); // 0x98
};

// size 0xAC
class ui_menu_item_incr : public ui_menu_item_base
{
public:
    ui_menu_item_incr(menu_incr_type type, const char* caption, ui_menu* parent, void* variable, void (*iccb)(int, void*)); // 0x008B7EF0 (:1099)
    ui_menu_item_incr(); // 0x008A1010 (:1094)
    virtual ~ui_menu_item_incr(); // vtbl+0x00 0x00883790 (.h:285)
    void ui_menu_item_incr_init(menu_incr_type type, const char* caption, ui_menu* parent, void* variable, void (*iccb)(int, void*)); // 0x008A1080 (:1105)
    menu_incr_type get_incr_type() const; // 0x008837A0 (.h:289)
    void set_info(variable_type min, variable_type max, variable_type incr); // 0x008A1130 (:1135)
    void get_info(variable_type& min, variable_type& max, variable_type& incr); // 0x008A1160 (:1152)
    virtual bool handle_input(increment_type type); // vtbl+0x08 0x008B7F50 (:1169)
    virtual void get_text_value(char* buff, int buff_size) const; // vtbl+0x04 0x008A1190 (:1223)

protected:
    variable_type m_min; // 0x94
    variable_type m_max; // 0x98
    variable_type m_incr; // 0x9C
    menu_incr_type m_incr_type; // 0xA0
    void* m_variable; // 0xA4
    void (*m_item_change_callback)(int, void*); // 0xA8
};

// size 0x5F0
class ui_menu
{
public:
    ui_menu(int doc_handle, const char* title); // 0x008CB570 (:1270)
    ui_menu(); // 0x008CB4D0 (:1261)
    virtual ~ui_menu(); // vtbl+0x00 0x008DBF60 (:1309)
    void init(int doc_handle, const char* title, void (*change_cb)(int), void (*back_cb)(hint_type), int input_id); // 0x008B8080 (:1279)
    void process(float dtime, bool is_topmost); // 0x008DC450 (:1752)
    int get_handle(); // 0x008837B0 (.h:320)
    int get_num_items(); // 0x008A1320 (:1768)
    ui_menu_item_base* get_menu_item_via_index(int which); // 0x008A1360 (:1786)
    ui_menu_item_base* get_menu_item_via_handle(int handle); // 0x008A1380 (:1796)
    bool select_menu_item_by_index(int which); // 0x008A13C0 (:1811)
    bool select_menu_item(int handle); // 0x008A13E0 (:1824)
    int get_menu_item_handle(int index); // 0x008A1420 (:1843)
    int get_menu_item_index(int handle); // 0x008A1440 (:1854)
    bool enable_menu_item(int handle, bool enable); // 0x008B83E0 (:1870)
    bool is_menu_item_enabled(unsigned int index); // 0x008A14C0 (:1910)
    bool is_menu_item_enabled(int handle); // 0x008A1480 (:1894)
    bool show_menu_item(int handle, bool visible); // 0x008B8440 (:1921)
    bool is_menu_item_visible(unsigned int index); // 0x008A1520 (:1961)
    bool is_menu_item_visible(int handle); // 0x008A14E0 (:1945)
    menu_item_type get_menu_item_type(int handle, menu_incr_type& incr_type); // 0x008A1540 (:1972)
    bool remove_item(int handle); // 0x008CB820 (:1992)
    bool remove_all_items(); // 0x008CB8B0 (:2036)
    const char* get_title();
    int add_function_item(const char* caption, void (*scb)(int), bool tween_out, bool surpress_accept_sound, bool set_active_controller); // 0x008B84A0 (:2047)
    int add_int_item(const char* caption, int* variable, int min, int max, int incr, void (*iccb)(int, void*)); // 0x008B8510 (:2068)
    int add_exp_item(const char* caption, int* variable, int min, int max, int exp, void (*iccb)(int, void*)); // 0x008B85D0 (:2091)
    int add_slider_item(const char* caption, int* variable, int min, int max, int incr, void (*iccb)(int, void*)); // 0x008B86A0 (:2114)
    int add_bool_item(const char* caption, bool* variable, void (*iccb)(int, void*)); // 0x008B8760 (:2137)
    int add_char_list_item(const char* caption, const char** list, int num_chars, void (*iccb)(int, void*), int selected_idx); // 0x008B87C0 (:2156)
    void set_doc_info(document_info* dinfo); // 0x008A1590 (:2174)
    document_info* get_doc_info() const; // 0x008837C0 (.h:355)
    bool get_awaiting_on_select_end() const; // 0x008837D0 (.h:357)
    bool get_awaiting_fade_in_end() const; // 0x008837E0 (.h:358)
    int get_choose_on_enter_index() const;
    int get_selected_item() const; // 0x008837F0 (.h:360)
    void set_should_tween_out(bool value); // 0x008A15F0 (:2212)
    void set_awaiting_on_select_end(bool value); // 0x008A15A0 (:2181)
    void set_awaiting_fade_in_end(bool value); // 0x008A15E0 (:2206)
    void set_surpress_accept_sounds(bool value); // 0x008A1600 (:2218)
    void set_choose_on_enter_index(int index); // 0x008A1610 (:2224)
    void force_data_item_update(); // 0x00883800 (.h:368)
    bool add_hint(hint_type ht, control_button_action action, const char* hint_text, const char* image_name); // 0x008CBA50 (:2299)
    void remove_hint(hint_type ht); // 0x008B8D60 (:2286)
    void process_mouseover(int index); // 0x008A16F0 (:2373)
    void process_mouse_click(int index, bool leftArrow, bool rightArrow); // 0x008DC4C0 (:2384)
    int get_num_visible_items(); // 0x008A1330 (:1773)
    int get_menu_item_index_by_location(int index); // 0x008A1720 (:2463)
protected:
    button_choices get_select_button(); // 0x008B8820 (:2230)
    button_choices get_abort_button(); // 0x008B8970 (:2244)
    button_choices get_misc_button(); // 0x008B8AC0 (:2258)
    button_choices get_misc_2_button(); // 0x008B8C10 (:2272)
    void adjust_selected_item(bool move_up); // 0x008A1690 (:2345)
    void adjust_selected_item(); // 0x008A1620 (:2322)
    void process_data_items(); // 0x008DC420 (:1549)
    void process_input(); // 0x008DBF80 (:1316)
    void process_populate_items(); // 0x008B8130 (:1561)
    void process_title(); // 0x008A1210 (:1637)
    void process_tween_on_select_end(); // 0x008A12A0 (:1651)
    void process_hints(); // 0x008CB5C0 (:1669)
    void process_mouse();

    // 0x00 vfptr
private:
    bool m_surpress_accept_sounds; // 0x04
    bool m_should_tween_out; // 0x05
    bool m_awaiting_tween_on_select_end; // 0x06
    bool m_awaiting_tween_fade_in_end; // 0x07
    bool m_force_update; // 0x08
    int m_choose_on_enter; // 0x0C
    int m_handle; // 0x10
    int m_doc_handle; // 0x14
    int m_input_id; // 0x18
    int m_selected; // 0x1C
    unsigned int m_num_items; // 0x20
    ui_menu_item_base* m_items[16]; // 0x24
    char m_title[192]; // 0x64
    void (*m_selection_hint_cb)(hint_type); // 0x124
    void (*m_selection_change_cb)(int); // 0x128
    document_info* m_doc_info; // 0x12C
    farray<hint_info,4> m_hint_info; // 0x130
};

ui_menu* ui_menu_get_via_handle(int handle); // 0x008B7730 (:289)
void unused_dcf_enable_menu_item(); // 0x008CB390 (:779)
void unused_dcf_show_menu_item(); // 0x008CB430 (:807)
void ui_menu_init(); // 0x008CBB20 (:2480)
void ui_menu_shutdown(); // 0x008DC640 (:2503)
int ui_menu_add(int doc_handle, const char* title, void (*change_cb)(int), void (*back_cb)(hint_type), const char* event_surtag, bool fade_in, int choose_on_enter_index, int input_id, bool surpress_accept_sounds); // 0x008CBD10 (:2521)
void ui_menu_process(float dtime); // 0x008DC7C0 (:2583)
bool ui_menu_destroy(int menu_handle); // 0x008CBE00 (:2620)
bool ui_menu_get_tween_on_select_end(int menu_handle); // 0x008B8DE0 (:2666)
void ui_menu_set_tween_on_select_end(int menu_handle); // 0x008B8E30 (:2677)
void ui_menu_tween_on_select_end(int vint_doc_handle); // 0x008CBF20 (:2686)
void ui_menu_tween_fade_in_end(int vint_doc_handle); // 0x008CBF90 (:2717)
void ui_menu_enable_menu_item(int menu_handle, int index, bool enabled); // 0x008B8E70 (:2754)
bool ui_menu_is_menu_item_enabled(int menu_handle, int index); // 0x008B8ED0 (:2766)
void ui_menu_show_menu_item(int menu_handle, int index, bool visible); // 0x008B8F30 (:2780)
bool ui_menu_is_menu_item_visible(int menu_handle, int index); // 0x008B8F90 (:2792)
int ui_menu_add_function_item(int menu_handle, const char* caption, void (*scbz)(int), bool tween_out, bool surpress_accept_sound, bool set_active_controller); // 0x008B8FF0 (:2806)
int ui_menu_add_int_item(int menu_handle, const char* caption, int* variable, int min, int max, int incr, void (*iccb)(int, void*), bool play_change_sound); // 0x008B9050 (:2818)
int ui_menu_add_exp_item(int menu_handle, const char* caption, int* variable, int min, int max, int exp, void (*iccb)(int, void*), bool play_change_sound); // 0x008B90D0 (:2833)
int ui_menu_add_slider_item(int menu_handle, const char* caption, int* variable, int min, int max, int incr, void (*iccb)(int, void*), bool play_change_sound); // 0x008B9150 (:2848)
int ui_menu_add_bool_item(int menu_handle, const char* caption, bool* variable, void (*iccb)(int, void*), bool play_change_sound); // 0x008B91D0 (:2863)
int ui_menu_add_char_list_item(int menu_handle, const char* caption, const char** list, int num_chars, void (*iccb)(int, void*), int selected_idx, bool play_change_sound); // 0x008B9260 (:2878)
bool ui_menu_add_selection_hint(int menu_handle, const char* hint_text, control_button_action action); // 0x008CBFE0 (:2893)
void ui_menu_remove_selection_hint(int menu_handle); // 0x008B92E0 (:2907)
void ui_menu_remove_all_hints(int menu_handle); // 0x008B9330 (:2915)
bool ui_menu_add_abort_hint(int menu_handle, const char* hint_text, control_button_action action); // 0x008CC080 (:2926)
void ui_menu_remove_abort_hint(int menu_handle); // 0x008B9380 (:2940)
bool ui_menu_add_misc_hint(int menu_handle, const char* hint_text, control_button_action action); // 0x008CC120 (:2949)
bool ui_menu_add_misc_hint_2(int menu_handle, const char* hint_text, control_button_action action); // 0x008CC1C0 (:2964)
bool ui_menu_set_char_list_index(int menu_handle, int index, int select_idx); // 0x008B93D0 (:2979)
int ui_menu_get_selected_item(int menu_handle); // 0x008B9440 (:2994)
void ui_menu_set_input_enabled(bool enabled); // 0x008A1750 (:3004)
void ui_menu_play_sound(menu_sound_type sound_type); // 0x008A1790 (:3014)
void ui_menu_set_selected_item(int menu_handle, int sel_idx); // 0x008B9490 (:3021)
int ui_menu_get_visible_item_count(int menu_handle); // 0x008B94E0 (:3030)
int ui_menu_get_total_item_count(int menu_handle); // 0x008B9540 (:3038)
void ui_menu_select_item_by_index(int menu_handle, int index); // 0x008B9580 (:3045)
void ui_menu_selection_clicked(int menu_handle, int index, bool leftArrow, bool rightArrow); // 0x008DC930 (:3054)
int ui_menu_get_index_by_location(int menu_handle, int index); // 0x008B95E0 (:3063)
bool ui_menu_is_fading_in(int menu_handle); // 0x008B9640 (:3074)
document_info* ui_menu_get_doc_info(int menu_handle); // 0x008B9690 (:3084)
