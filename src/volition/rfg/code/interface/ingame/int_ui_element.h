// Original: volition/rfg/code/interface/ingame/int_ui_element.h
// Implementation: volition/rfg/code/interface/ingame/int_ui_element.cpp
//
// Simple drawn UI elements (slider, list box) of the in-game interface.
#pragma once

enum ui_element_type
{
    UI_ELEMENT_INVALID = -1,
    UI_ELEMENT_SLIDER = 0,
    UI_ELEMENT_LIST_BOX = 1,
    NUM_UI_ELEMENT_TYPES = 2,
};

// size 0x34
class ui_element
{
public:
    virtual ~ui_element(); // vtbl+0x00
    virtual int process(); // vtbl+0x04
    virtual void render(); // vtbl+0x08
    void create_element(ui_element_type _type); // 0x00887F70 (.h:76)

    // 0x00 vfptr
    ui_element_type m_type; // 0x04
    bool m_active; // 0x08
    ui_element* next; // 0x0C
    ui_element* prev; // 0x10
    int m_screen_x; // 0x14
    int m_screen_y; // 0x18
    char m_name[24]; // 0x1C
};

// size 0x4C
class ui_slider : public ui_element
{
public:
    ui_slider(char*, int, int, int, float, int, int, int, int);
    void init(char* name, int sx, int sy, int size, float value, int left_bitmap, int bar_bitmap, int right_bitmap, int slide_bitmap); // 0x00887F80 (:107)
    float get_value(); // 0x00887FE0 (:129)
    void set_value(float value); // 0x00887FF0 (:133)
    virtual int process(); // vtbl+0x04 0x00888000 (:143)
    virtual void render(); // vtbl+0x08 0x008AA080 (:180)

    int m_left_bitmap; // 0x34
    int m_bar_bitmap; // 0x38
    int m_right_bitmap; // 0x3C
    int m_slide_bitmap; // 0x40
    int m_size; // 0x44
    float m_value; // 0x48
};

// size 0x4C
class ui_list_box : public ui_element
{
public:
    ui_list_box(const char*, int, int, int, int, const char**, int);
    void init(const char* name, int sx, int sy, int size, int num_options, const char** options, int selected_index); // 0x00888010 (:272)
    int get_selected_index(); // 0x00888070 (:288)
    void set_selected_index(int selected_index); // 0x00888080 (:293)
    virtual int process(); // vtbl+0x04 0x008AA350 (:298)
    virtual void render(); // vtbl+0x08 0x008AA510 (:348)

    int m_size; // 0x34
    int m_selected_index; // 0x38
    int m_num_options; // 0x3C
    const char** m_options; // 0x40
    float m_scroll_timer; // 0x44
    bool m_is_expanded; // 0x48
};
