// Original: volition/interface_lib/code/objects/noninteractive/vint_element_text.h
// Implementation: volition/interface_lib/code/objects/noninteractive/vint_element_text.cpp
//
// A text element: font, text tag or localized string, alignment, wrap width, leading, line count, insertion values,
// forced case, and the parameters passed to string processing.
#pragma once

#include "volition/interface_lib/code/objects/vint_element_base.h"
#include "volition/interface_lib/code/vint_string_processing.h"
#include "volition/vlib/code/math/vector2.h"

struct bbox2d;
class vint_object_base;
class vint_variant;

// size 0x38
struct vint_text_render_params
{
    vint_text_render_params(); // 0x004688F0 (.h:68)
    void set_default_values(); // 0x0045E510 (.h:70)

    unsigned int font_h; // 0x00
    vector2 text_area; // 0x04
    vector2 scale; // 0x0C
    vint_text_vert_align valign; // 0x14
    vint_text_horz_align halign; // 0x18
    bool word_wrap; // 0x1C
    float wrap_width; // 0x20
    vint_text_force_case force_case; // 0x24
    bool enable_line_framing; // 0x28
    signed char leading; // 0x29
    unsigned int frame_w_h; // 0x2C
    unsigned int frame_m_h; // 0x30
    unsigned int frame_e_h; // 0x34
};

// size 0x1DC
class vint_element_text : public vint_element_base
{
public:
    vint_element_text(); // 0x00468940 (.h:89)
    virtual ~vint_element_text(); // vtbl+0x00 0x00477730 (.h:90)
    virtual void destroy(); // vtbl+0x08 0x00472230 (:355)
    static bool system_init(); // 0x00472120 (:320)
    static void system_shutdown(); // 0x0045E6D0 (:330)
    virtual void process(); // vtbl+0x0C 0x00472250 (:396)
    virtual bool is_type(unsigned int _type) const; // vtbl+0x18 0x004689E0 (.h:98)
    virtual unsigned int get_type() const; // vtbl+0x1C 0x00468A10 (.h:98)
    static bool internal_get_prop_font(const vint_object_base* _obj, vint_variant& _val, int _index); // 0x0045E840 (:632)
    static bool internal_set_prop_font(vint_object_base* _obj, const vint_variant& _val, int _index); // 0x0045E860 (:646)
    virtual bool get_font(vint_variant& _val, int _index) const; // vtbl+0xCC 0x00472350 (:633)
    virtual bool set_font(const vint_variant& _val, int _index); // vtbl+0xD0 0x00468BF0 (:647)
    static bool internal_get_prop_text_tag(const vint_object_base* _obj, vint_variant& _val, int _index); // 0x0045E710 (:557)
    static bool internal_set_prop_text_tag(vint_object_base* _obj, const vint_variant& _val, int _index); // 0x0045E730 (:569)
    virtual bool get_text_tag(vint_variant& _val, int _index) const; // vtbl+0xD4 0x00475380 (:558)
    virtual bool set_text_tag(const vint_variant& _val, int _index); // vtbl+0xD8 0x00468AC0 (:570)
    static bool internal_get_prop_text_tag_crc(const vint_object_base* _obj, vint_variant& _val, int _index); // 0x0045E750 (:609)
    static bool internal_set_prop_text_tag_crc(vint_object_base* _obj, const vint_variant& _val, int _index); // 0x0045E770 (:617)
    virtual bool get_text_tag_crc(vint_variant& _val, int _index) const; // vtbl+0xDC 0x00472330 (:610)
    virtual bool set_text_tag_crc(const vint_variant& _val, int _index); // vtbl+0xE0 0x0045E790 (:618)
    static bool internal_get_prop_text_scale(const vint_object_base* _obj, vint_variant& _val, int _index); // 0x0045E880 (:692)
    static bool internal_set_prop_text_scale(vint_object_base* _obj, const vint_variant& _val, int _index); // 0x0045E8E0 (:700)
    virtual bool get_text_scale(vint_variant& _val, int _index) const; // vtbl+0xE4 0x0045E8A0 (:693)
    virtual bool set_text_scale(const vint_variant& _val, int _index); // vtbl+0xE8 0x0045E900 (:701)
    static bool internal_get_prop_word_wrap(const vint_object_base* _obj, vint_variant& _val, int _index); // 0x0045E990 (:753)
    static bool internal_set_prop_word_wrap(vint_object_base* _obj, const vint_variant& _val, int _index); // 0x0045E9E0 (:761)
    virtual bool get_word_wrap(vint_variant& _val, int _index) const; // vtbl+0xEC 0x0045E9B0 (:754)
    virtual bool set_word_wrap(const vint_variant& _val, int _index); // vtbl+0xF0 0x0045EA00 (:762)
    static bool internal_get_prop_wrap_width(const vint_object_base* _obj, vint_variant& _val, int _index); // 0x0045EAC0 (:770)
    static bool internal_set_prop_wrap_width(vint_object_base* _obj, const vint_variant& _val, int _index); // 0x0045EB20 (:778)
    virtual bool get_wrap_width(vint_variant& _val, int _index) const; // vtbl+0xF4 0x0045EAE0 (:771)
    virtual bool set_wrap_width(const vint_variant& _val, int _index); // vtbl+0xF8 0x0045EB40 (:779)
    static bool internal_get_prop_screen_size(const vint_object_base* _obj, vint_variant& _val, int _index); // 0x0045E950 (:709)
    static bool internal_set_prop_screen_size(vint_object_base* _obj, const vint_variant& _val, int _index); // 0x0045E970 (:718)
    virtual bool get_screen_size(vint_variant& _val, int _index) const; // vtbl+0x6C 0x0047DFD0 (:710)
    virtual bool set_screen_size(const vint_variant& _val, int _index); // vtbl+0x70 0x00468CE0 (:719)
    static bool internal_get_prop_horz_align(const vint_object_base* _obj, vint_variant& _val, int _index); // 0x0045EC70 (:805)
    static bool internal_set_prop_horz_align(vint_object_base* _obj, const vint_variant& _val, int _index); // 0x0045EC90 (:813)
    virtual bool get_horz_align(vint_variant& _val, int _index) const; // vtbl+0xFC 0x00468E90 (:806)
    virtual bool set_horz_align(const vint_variant& _val, int _index); // vtbl+0x100 0x0045ECB0 (:814)
    static bool internal_get_prop_vert_align(const vint_object_base* _obj, vint_variant& _val, int _index); // 0x0045EBA0 (:788)
    static bool internal_set_prop_vert_align(vint_object_base* _obj, const vint_variant& _val, int _index); // 0x0045EBC0 (:796)
    virtual bool get_vert_align(vint_variant& _val, int _index) const; // vtbl+0x104 0x00468E70 (:789)
    virtual bool set_vert_align(const vint_variant& _val, int _index); // vtbl+0x108 0x0045EBE0 (:797)
    static bool internal_get_prop_force_case(const vint_object_base* _obj, vint_variant& _val, int _index); // 0x0045ED40 (:822)
    static bool internal_set_prop_force_case(vint_object_base* _obj, const vint_variant& _val, int _index); // 0x0045ED60 (:830)
    virtual bool get_force_case(vint_variant& _val, int _index) const; // vtbl+0x10C 0x00468EB0 (:823)
    virtual bool set_force_case(const vint_variant& _val, int _index); // vtbl+0x110 0x0045ED80 (:831)
    static bool internal_get_prop_leading(const vint_object_base* _obj, vint_variant& _val, int _index); // 0x0045F0C0 (:1144)
    static bool internal_set_prop_leading(vint_object_base* _obj, const vint_variant& _val, int _index); // 0x0045F0A0 (:1123)
    virtual bool get_leading(vint_variant& _val, int _index) const; // vtbl+0x114 0x00468FD0 (:1145)
    virtual bool set_leading(const vint_variant& _val, int _index); // vtbl+0x118 0x00468ED0 (:1124)
    static bool internal_get_prop_line_frame_enable(const vint_object_base* _obj, vint_variant& _val, int _index); // 0x0045EF90 (:926)
    static bool internal_set_prop_line_frame_enable(vint_object_base* _obj, const vint_variant& _val, int _index); // 0x0045EEE0 (:912)
    virtual bool get_line_frame_enable(vint_variant& _val, int _index) const; // vtbl+0x11C 0x0045EFB0 (:927)
    virtual bool set_line_frame_enable(const vint_variant& _val, int _index); // vtbl+0x120 0x0045EF00 (:913)
    static bool internal_get_prop_line_frame_w(const vint_object_base* _obj, vint_variant& _val, int _index); // 0x0045F000 (:989)
    static bool internal_set_prop_line_frame_w(vint_object_base* _obj, const vint_variant& _val, int _index); // 0x0045EFE0 (:934)
    virtual bool get_line_frame_w(vint_variant& _val, int _index) const; // vtbl+0x124 0x00472390 (:990)
    virtual bool set_line_frame_w(const vint_variant& _val, int _index); // vtbl+0x128 0x00475510 (:935)
    static bool internal_get_prop_line_frame_m(const vint_object_base* _obj, vint_variant& _val, int _index); // 0x0045F040 (:1052)
    static bool internal_set_prop_line_frame_m(vint_object_base* _obj, const vint_variant& _val, int _index); // 0x0045F020 (:997)
    virtual bool get_line_frame_m(vint_variant& _val, int _index) const; // vtbl+0x12C 0x00472400 (:1053)
    virtual bool set_line_frame_m(const vint_variant& _val, int _index); // vtbl+0x130 0x004755B0 (:998)
    static bool internal_get_prop_line_frame_e(const vint_object_base* _obj, vint_variant& _val, int _index); // 0x0045F080 (:1115)
    static bool internal_set_prop_line_frame_e(vint_object_base* _obj, const vint_variant& _val, int _index); // 0x0045F060 (:1060)
    virtual bool get_line_frame_e(vint_variant& _val, int _index) const; // vtbl+0x134 0x00472470 (:1116)
    virtual bool set_line_frame_e(const vint_variant& _val, int _index); // vtbl+0x138 0x00475650 (:1061)
    static bool internal_get_prop_insert_values(const vint_object_base* _obj, vint_variant& _val, int _index); // 0x0045EE10 (:839)
    static bool internal_set_prop_insert_values(vint_object_base* _obj, const vint_variant& _val, int _index); // 0x0045EE30 (:851)
    virtual bool get_insert_values(vint_variant& _val, int _index) const; // vtbl+0x13C 0x004753A0 (:840)
    virtual bool set_insert_values(const vint_variant& _val, int _index); // vtbl+0x140 0x004753E0 (:852)
    static bool internal_get_first_index_insert_values(const vint_object_base* _obj, int& _index); // 0x0045EE50 (:888)
    static bool internal_get_next_index_insert_values(const vint_object_base* _obj, int& _index); // 0x0045EE90 (:900)
    virtual bool get_first_index_insert_values(int& _index) const; // vtbl+0x144 0x0045EE70 (:889)
    virtual bool get_next_index_insert_values(int& _index) const; // vtbl+0x148 0x0045EEB0 (:901)
    static void refresh_all_strings(); // 0x00468A80 (:547)
    static bool refresh_string(vint_object_base* _t); // 0x0045E6E0 (:539)
    bool is_text_dirty() const;
    void set_text_dirty(); // 0x0045E560 (.h:121)
    void reformat_text(); // 0x00477B70 (:425)
    virtual void get_bbox(bbox2d& box, bool adjust_min_and_max) const; // vtbl+0x24 0x00477F30 (:552)
protected:
    static vint_element_text load_from_xml();
    virtual bool init(); // vtbl+0x04 0x004750B0 (:335)
    virtual void clean_up(); // vtbl+0x10 0x00475170 (:361)
    virtual void render_object(const vint_render_params& _render_params); // vtbl+0xC0 0x00472260 (:412)
    virtual void pre_render(); // vtbl+0xBC 0x0047DFA0 (:401)

private:
    static unsigned int Int_obj_type; // 0x014F58E4
public:
    static const unsigned int& Object_type; // 0x014F58E8
private:
    vector2 m_text_size; // 0x180
    unsigned int m_text_tag_crc; // 0x188
    char* m_text_tag; // 0x18C
    vint_text_render_params m_text_params; // 0x190
    void* m_insert_values_buff; // 0x1C8
    vint_variant* m_insert_values; // 0x1CC
    unsigned char* m_render_instructions; // 0x1D0
    unsigned int m_render_instruction_len; // 0x1D4
    bool m_text_is_dirty; // 0x1D8
};
