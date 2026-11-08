// Original: volition/rfg/code/interface/ingame/int_thin_controls.h
// Implementation: volition/rfg/code/interface/ingame/int_thin_controls.cpp
//
// The thin controls: a small immediate-mode control set (text, buttons, toggles, sliders, color picker, list box,
// progress bar, groups, layouts and sliding panels) bound to variables (thin_control_variable<T>).
#pragma once

#include "volition/vlib/code/math/bbox2d.h"
#include "volition/vlib/code/time/timestamp.h"

// size 0x24
class thin_control
{
public:
    thin_control(); // 0x008A8890 (.h:51)
    virtual ~thin_control(); // vtbl+0x00 0x00884B60 (.h:52)
    virtual void render() = 0; // vtbl+0x04
    virtual bool process(bool, bool) = 0; // vtbl+0x08
    virtual void set_pos(int, int) = 0; // vtbl+0x0C
    virtual void calculate_size() = 0; // vtbl+0x10

    // 0x00 vfptr
    bbox2d control_box; // 0x04
    bool active; // 0x14
    thin_control* next; // 0x18
    thin_control* prev; // 0x1C
    thin_control* parent; // 0x20
};

// size 0x28
class thin_control_text : public thin_control
{
public:
    virtual void render(); // vtbl+0x04 0x00884C00 (:170)
    virtual bool process(bool process_controls, bool calc_size); // vtbl+0x08 0x00884CC0 (:193)
    virtual void set_pos(int x, int y); // vtbl+0x0C 0x00884D20 (:210)
    virtual void calculate_size(); // vtbl+0x10 0x00884DC0 (:225)
    thin_control_text(const char* _text); // 0x008A8A00 (:155)
    thin_control_text(); // 0x008A88A0 (.h:74)
    void init(const char* _text); // 0x00884BF0 (:160)
    virtual ~thin_control_text(); // vtbl+0x00 0x00884B90

    const char* text; // 0x24
};

// size 0x34
class thin_control_text_button : public thin_control_text
{
public:
    virtual void render(); // vtbl+0x04 0x00884E70 (:381)
    virtual bool process(bool process_controls, bool calc_size); // vtbl+0x08 0x008850D0 (:427)
    virtual void set_pos(int x, int y); // vtbl+0x0C 0x00885210 (:485)
    virtual void calculate_size(); // vtbl+0x10 0x008852B0 (:500)
    thin_control_text_button(const char* _text, int _repeat_delay, bool _show_border); // 0x008A8A20 (:363)
    thin_control_text_button(); // 0x008A88D0 (.h:113)
    void init(const char* _text, int _repeat_delay, bool _show_border); // 0x00884E40 (:368)
    virtual ~thin_control_text_button(); // vtbl+0x00 0x00884BA0

    int repeat_delay; // 0x28
    bool highlighted; // 0x2C
    bool down; // 0x2D
    bool show_border; // 0x2E
    timestamp repeat_timer; // 0x30
};

// size 0x30
class thin_control_toggle_button : public thin_control
{
public:
    virtual void render(); // vtbl+0x04 0x00885350 (:535)
    virtual bool process(bool process_controls, bool calc_size); // vtbl+0x08 0x008855B0 (:569)
    virtual void set_pos(int x, int y); // vtbl+0x0C 0x008856A0 (:612)
    virtual void calculate_size(); // vtbl+0x10 0x00885740 (:627)
    thin_control_toggle_button(const char* _text, bool* _value); // 0x008A8A50 (:519)
    thin_control_toggle_button();
    void init(const char* _text, bool* _value); // 0x00885330 (:524)
    virtual ~thin_control_toggle_button(); // vtbl+0x00 0x008A8A80

    bool* value; // 0x24
    const char* text; // 0x28
    bool highlighted; // 0x2C
};

// size 0x84
class thin_control_group : public thin_control
{
public:
    virtual void render(); // vtbl+0x04 0x00885840 (:690)
    virtual bool process(bool process_controls, bool calc_size); // vtbl+0x08 0x00885910 (:718)
    virtual void set_pos(int x, int y); // vtbl+0x0C 0x008859D0 (:765)
    virtual void calculate_size(); // vtbl+0x10 0x00885B00 (:799)
    thin_control_group(const char* _title, thin_control** _children, int num_children, bool default_hidden); // 0x008A8B30 (:661)
    thin_control_group(const char* _title, bool default_hidden); // 0x008A8AB0 (:655)
    thin_control_group(); // 0x008A8910 (.h:152)
    void init(const char* _title, bool default_hidden); // 0x008857F0 (:672)
    void add_child(thin_control* child); // 0x00885BD0 (:833)
    void remove_child(thin_control* child); // 0x00885C20 (:841)
    virtual ~thin_control_group(); // vtbl+0x00 0x00884BB0

    thin_control_text title; // 0x24
    thin_control_text_button hide_show; // 0x4C
    thin_control* children; // 0x80
    static const char* const show_string; // 0x013005A0
    static const char* const hide_string; // 0x013005A4
};

// size 0x28
class thin_control_layout : public thin_control
{
public:
    // size 0xC
    struct control_desc
    {
        int x; // 0x00
        int y; // 0x04
        thin_control* control; // 0x08
    };

    virtual void render(); // vtbl+0x04 0x00885C80 (:875)
    virtual bool process(bool process_controls, bool calc_size); // vtbl+0x08 0x00885CB0 (:887)
    virtual void set_pos(int x, int y); // vtbl+0x0C 0x00885D10 (:911)
    virtual void calculate_size(); // vtbl+0x10 0x00885D20 (:921)
    thin_control_layout(control_desc* _children, int num_children); // 0x008A8C00 (:861)
    thin_control_layout();
    void add_child(thin_control* child); // 0x00885D50 (:931)
    void remove_child(thin_control* child); // 0x00885D90 (:939)
    virtual ~thin_control_layout(); // vtbl+0x00 0x008A8C80

    thin_control* children; // 0x24
};

// size 0x110
class thin_control_slider : public thin_control
{
public:
    virtual void render(); // vtbl+0x04 0x00885EE0 (:1036)
    virtual bool process(bool process_controls, bool calc_size); // vtbl+0x08 0x00886280 (:1086)
    virtual void set_pos(int x, int y); // vtbl+0x0C 0x00886580 (:1163)
    virtual void calculate_size(); // vtbl+0x10 0x00886820 (:1202)
    thin_control_slider(float* _value, const char* _name, float _min, float _max, float _step, bool _horiz); // 0x008BEE90 (:957)
    thin_control_slider(); // 0x008A8970 (.h:206)
    void init(float* _value, const char* _name, float _min, float _max, int max_dimension, bool _horiz); // 0x008A8D80 (:983)
    void init(float* _value, const char* _name, float _min, float _max, float _step, bool _horiz); // 0x008A8CB0 (:962)
    void resize(float _min, float _max, int max_dimension); // 0x00885E10 (:1011)
    void resize(float _min, float _max, float _step); // 0x00885DE0 (:1004)
    virtual ~thin_control_slider(); // vtbl+0x00 0x00884BC0

    float* value; // 0x24
    float min; // 0x28
    float max; // 0x2C
    float step; // 0x30
    bool horiz; // 0x34
    thin_control_text_button decrease_button; // 0x38
    thin_control_text_button increase_button; // 0x6C
    thin_control_text name; // 0xA0
    thin_control_text val_display; // 0xC8
    char value_text[16]; // 0xF0
    bbox2d slider_box; // 0x100
    static const char* const left; // 0x013005A8
    static const char* const right; // 0x013005AC
    static const char* const up; // 0x013005B0
    static const char* const down; // 0x013005B4
};

// size 0x148
class thin_control_sliding_panel : public thin_control
{
public:
    virtual void render(); // vtbl+0x04 0x008A8F40 (:1274)
    virtual bool process(bool process_input, bool calc_size); // vtbl+0x08 0x00886A80 (:1326)
    virtual void set_pos(int x, int y); // vtbl+0x0C 0x00886C00 (:1360)
    virtual void calculate_size(); // vtbl+0x10 0x00886D10 (:1385)
    thin_control_sliding_panel(int _max_height, int _max_width, thin_control** _children, int num_children); // 0x008BF020 (:1254)
    thin_control_sliding_panel(int _max_height, int _max_width); // 0x008BEF20 (:1248)
    thin_control_sliding_panel();
    void init(int _max_height, int _max_width); // 0x008A8EF0 (:1265)
    void add_child(thin_control* child); // 0x00886F00 (:1428)
    void remove_child(thin_control* child); // 0x00886F50 (:1436)
    virtual ~thin_control_sliding_panel(); // vtbl+0x00 0x008BEFC0

    float cur_height_offset; // 0x24
    float max_height_offset; // 0x28
    thin_control_slider vert_slider; // 0x2C
    int max_height; // 0x13C
    int max_width; // 0x140
    thin_control* children; // 0x144
};

// size 0x6FC
class thin_control_color_picker : public thin_control_group
{
public:
    enum rgb_hsv_slider
    {
        RED_SLIDER = 0,
        GREEN_SLIDER = 1,
        BLUE_SLIDER = 2,
        HUE_SLIDER = 3,
        SAT_SLIDER = 4,
        VALUE_SLIDER = 5,
        NUM_RBG_HSV_SLIDERS = 6,
    };

    virtual bool process(bool process_controls, bool calc_size); // vtbl+0x08 0x00886FB0 (:1483)
    thin_control_color_picker(float* _r, float* _g, float* _b, const char* _title); // 0x008BF130 (:1452)
    thin_control_color_picker();
    void init(float* _r, float* _g, float* _b, const char* _title); // 0x008A9210 (:1457)
    virtual ~thin_control_color_picker(); // vtbl+0x00 0x008BF330

    float temp_h; // 0x84
    float temp_s; // 0x88
    float temp_v; // 0x8C
    float* r; // 0x90
    float* g; // 0x94
    float* b; // 0x98
    thin_control_slider sliders[6]; // 0x9C
    static const char* const slider_names[6]; // 0x013005B8
};

// size 0xB4
class thin_control_list_box : public thin_control
{
public:
    virtual void render(); // vtbl+0x04 0x008A9840 (:1613)
    virtual bool process(bool process_controls, bool calc_size); // vtbl+0x08 0x008A9A10 (:1647)
    virtual void set_pos(int x, int y); // vtbl+0x0C 0x008874E0 (:1732)
    virtual void calculate_size(); // vtbl+0x10 0x00887800 (:1777)
    thin_control_list_box(const char** _vals, int _num_vals, int* _selected_val, const char* _title); // 0x008A9760 (:1586)
    thin_control_list_box();
    void init(const char** _vals, int _num_vals, int* _selected_val, const char* _title); // 0x00887470 (:1591)
    virtual ~thin_control_list_box(); // vtbl+0x00 0x008A97F0

    static const int MAX_THIN_LIST_BOX_ENTRIES; // 0x01301620
    static thin_control_text_button Temp_thin_list_box_entries[64]; // 0x02C0E2F0
    static thin_control_slider Temp_thin_list_box_slider; // 0x01665C10
    static float Temp_thin_list_box_slider_pos; // 0x02BE9E28
    static bbox2d Temp_list_border; // 0x02C108C0
    thin_control_text title; // 0x24
    thin_control_text value_display; // 0x4C
    thin_control_text_button drop_down_button; // 0x74
    const char** vals; // 0xA8
    int num_vals; // 0xAC
    int* selected_val; // 0xB0
    static const char* const drop_down_char; // 0x013005D0
};

// size 0x58
class thin_control_progress_bar : public thin_control
{
public:
    virtual void render(); // vtbl+0x04 0x00887A10 (:1842)
    virtual bool process(bool process_input, bool calc_size); // vtbl+0x08 0x00887CA0 (:1871)
    virtual void set_pos(int x, int y); // vtbl+0x0C 0x00887D10 (:1889)
    virtual void calculate_size(); // vtbl+0x10 0x00887DB0 (:1902)
    thin_control_progress_bar(const char* _title, float* _value, int _width, int _height); // 0x008A9CE0 (:1829)
    thin_control_progress_bar();
    void init(const char* _title, float* _value, int _width, int _height); // 0x008879E0 (:1834)
    virtual ~thin_control_progress_bar(); // vtbl+0x00 0x008A9D20

    thin_control_text title; // 0x24
    float* value; // 0x4C
    int width; // 0x50
    int height; // 0x54
};

// size (<short>) 0x2C
template<typename T>
class thin_control_variable : public thin_control
{
public:
    virtual void render(); // vtbl+0x04 <short> 0x008A45A0 (:273)
    virtual bool process(bool process_controls, bool calc_size); // vtbl+0x08 <short> 0x008A4670 (:298)
    virtual void set_pos(int x, int y); // vtbl+0x0C <short> 0x008A46D0 (:316)
    virtual void calculate_size(); // vtbl+0x10 <short> 0x008A4770 (:332)
    virtual const char* generate_string(); // vtbl+0x14 <short> 0x008A47F0 (:262)
    thin_control_variable(const char* _text, const T* _value); // <short> 0x008BD490 (:247)
    thin_control_variable(); // <short> 0x008BC1E0 (.h:95)
    void init(const char* _text, const T* _value); // <short> 0x008A4820 (:253)
    virtual ~thin_control_variable(); // vtbl+0x00 <short> 0x008A3570

    const T* value; // 0x24
    const char* text; // 0x28
};

// template instance: void __thiscall thin_control_variable<char const*>::thin_control_variable<char const*>(); // 0x008BC240 (.h:95)
// template instance: void __thiscall thin_control_variable<double>::thin_control_variable<double>(); // 0x008BC1D0 (.h:95)
// template instance: void __thiscall thin_control_variable<unsigned short>::thin_control_variable<unsigned short>(); // 0x008BC1F0 (.h:95)
// template instance: void __thiscall thin_control_variable<unsigned int>::thin_control_variable<unsigned int>(); // 0x008BC210 (.h:95)
// template instance: void __thiscall thin_control_variable<int>::thin_control_variable<int>(); // 0x008BC200 (.h:95)
// template instance: void __thiscall thin_control_variable<__int64>::thin_control_variable<__int64>(); // 0x008BC220 (.h:95)
// template instance: void __thiscall thin_control_variable<float>::thin_control_variable<float>(); // 0x008BC1C0 (.h:95)
// template instance: void __thiscall thin_control_variable<unsigned __int64>::thin_control_variable<unsigned __int64>(); // 0x008BC230 (.h:95)
// template instance: void __thiscall thin_control_variable<short>::thin_control_variable<short>(); // 0x008BC1E0 (.h:95)
// template instance: void __thiscall thin_control_variable<unsigned __int64>::thin_control_variable<unsigned __int64>(const char* _text, const uint64_t* _value); // 0x008BD580 (:247)
// template instance: void __thiscall thin_control_variable<double>::thin_control_variable<double>(const char* _text, const double* _value); // 0x008BD460 (:247)
// template instance: void __thiscall thin_control_variable<int>::thin_control_variable<int>(const char* _text, const int* _value); // 0x008BD4F0 (:247)
// template instance: void __thiscall thin_control_variable<char const*>::thin_control_variable<char const*>(const char* _text, const char* const* _value); // 0x008BD5B0 (:247)
// template instance: void __thiscall thin_control_variable<short>::thin_control_variable<short>(const char* _text, const short* _value); // 0x008BD490 (:247)
// template instance: void __thiscall thin_control_variable<unsigned int>::thin_control_variable<unsigned int>(const char* _text, const unsigned int* _value); // 0x008BD520 (:247)
// template instance: void __thiscall thin_control_variable<float>::thin_control_variable<float>(const char* _text, const float* _value); // 0x008BD430 (:247)
// template instance: void __thiscall thin_control_variable<unsigned short>::thin_control_variable<unsigned short>(const char* _text, const unsigned short* _value); // 0x008BD4C0 (:247)
// template instance: void __thiscall thin_control_variable<__int64>::thin_control_variable<__int64>(const char* _text, const int64_t* _value); // 0x008BD550 (:247)
bool thin_controls_process(thin_control* root, bool process_controls); // 0x008A9D60 (:1924)
void thin_controls_render(thin_control* root); // 0x00887E70 (:1974)
