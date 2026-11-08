// Original: volition/interface_lib/code/objects/vint_element_base.h
// Implementation: volition/interface_lib/code/objects/vint_element_base.cpp
//
// The base of every visible element: position, scale, rotation, anchor and auto offset (compass), color, alpha, depth,
// visibility, render mode, the resolution scale/offset modes, and the render parameters an element passes to its
// children. Placement of vint_compass inferred.
#pragma once

#include "volition/interface_lib/code/objects/vint_object_base.h"
#include "volition/interface_lib/code/vint_external.h"
#include "volition/interface_lib/code/vint_variant.h"
#include "volition/vlib/code/math/matrix.h"
#include "volition/vlib/code/math/vector2.h"

struct bbox2d;
class vint_element_base;

enum vint_compass
{
    VINT_COMPASS_NONE = 0,
    VINT_COMPASS_CENTER = 1,
    VINT_COMPASS_W = 2,
    VINT_COMPASS_N = 3,
    VINT_COMPASS_E = 4,
    VINT_COMPASS_S = 5,
    VINT_COMPASS_NW = 6,
    VINT_COMPASS_NE = 7,
    VINT_COMPASS_SE = 8,
    VINT_COMPASS_SW = 9,
    NUM_VINT_COMPASS = 10,
};

// size 0x94
struct vint_render_params
{
    vint_render_params(); // 0x0045F1E0 (:150)
    void combine(const vint_element_base& _element); // 0x00477FC0 (:158)

    matrix23 origin_xform; // 0x00
    matrix23 object_xform; // 0x18
    vector2 final_offset; // 0x30
    vector2 final_scale; // 0x38
    vector2 origin_pos; // 0x40
    vector2 object_pos; // 0x48
    float alpha; // 0x50
    float final_rot; // 0x54
    vint_render_mode mode; // 0x58
    vint_variant_color tint; // 0x5C
    bool is_mask; // 0x68
    vint_res_offset res_offset_x; // 0x6C
    vint_res_offset res_offset_y; // 0x70
    vint_res_scale res_scale_x; // 0x74
    vint_res_scale res_scale_y; // 0x78
private:
    matrix23 parent_xform; // 0x7C
};

// size 0x180
class vint_element_base : public vint_object_base
{
    // friends are not recorded in rfg.pdb: combine reads the element's transform members
    friend struct vint_render_params;

public:
    vint_element_base(); // 0x004683C0 (.h:88)
    virtual ~vint_element_base(); // vtbl+0x00 0x0045D600 (.h:90)
    static bool system_init(); // 0x00472590 (:211)
    virtual void process() = 0; // vtbl+0x0C 0x004690A0 (:253)
    void render(const vint_render_params& _render_params, vint_object_base** _render_stack, unsigned int _stack_size); // 0x00478520 (:283)
    vint_element_base* pick_recursive(const vector2& pos, float tolerance); // 0x00478730 (:371)
    bool is_point_inside(const vector2& pos, float tolerance) const; // 0x0045F290 (:401)
    virtual void get_bbox(bbox2d& box, bool) const; // vtbl+0x24 0x0045F380 (:438)
protected:
    void get_global_anchor(vector2& _anchor, matrix23& _child_xform, matrix23& _offset_xform) const; // 0x00478950 (:466)
public:
    void get_global_anchor(vector2& _anchor) const; // 0x00478D40 (:492)
    void get_accum_scale(vector2& _scale) const; // 0x0045F430 (:498)
    virtual void get_corners(vector2& _nw, vector2& _ne, vector2& _sw, vector2& _se) const; // vtbl+0x28 0x0047E080 (:415)
    virtual bool is_type(unsigned int _type) const; // vtbl+0x18 0x004683F0 (.h:104)
    virtual unsigned int get_type() const; // vtbl+0x1C 0x00468410 (.h:104)
    static bool internal_get_prop_render_mode(const vint_object_base* _obj, vint_variant& _val, int _index); // 0x0045F590 (:564)
    static bool internal_set_prop_render_mode(vint_object_base* _obj, const vint_variant& _val, int _index); // 0x0045F5B0 (:571)
    virtual bool get_render_mode(vint_variant& _val, int _index) const; // vtbl+0x2C 0x004690B0 (:565)
    virtual bool set_render_mode(const vint_variant& _val, int _index); // vtbl+0x30 0x0045F5D0 (:572)
    static bool internal_get_prop_visible(const vint_object_base* _obj, vint_variant& _val, int _index); // 0x0045FB50 (:669)
    static bool internal_set_prop_visible(vint_object_base* _obj, const vint_variant& _val, int _index); // 0x0045FBA0 (:676)
    virtual bool get_visible(vint_variant& _val, int _index) const; // vtbl+0x34 0x0045FB70 (:670)
    virtual bool set_visible(const vint_variant& _val, int _index); // vtbl+0x38 0x0045FBC0 (:677)
    static bool internal_get_prop_mask(const vint_object_base* _obj, vint_variant& _val, int _index); // 0x0045FC60 (:683)
    static bool internal_set_prop_mask(vint_object_base* _obj, const vint_variant& _val, int _index); // 0x0045FCB0 (:690)
    virtual bool get_mask(vint_variant& _val, int _index) const; // vtbl+0x3C 0x0045FC80 (:684)
    virtual bool set_mask(const vint_variant& _val, int _index); // vtbl+0x40 0x0045FCD0 (:691)
    static bool internal_get_prop_anchor(const vint_object_base* _obj, vint_variant& _val, int _index); // 0x0045FD70 (:697)
    static bool internal_set_prop_anchor(vint_object_base* _obj, const vint_variant& _val, int _index); // 0x0045FDD0 (:704)
    virtual bool get_anchor(vint_variant& _val, int _index) const; // vtbl+0x44 0x0045FD90 (:698)
    virtual bool set_anchor(const vint_variant& _val, int _index); // vtbl+0x48 0x0045FDF0 (:705)
    static bool internal_get_prop_offset(const vint_object_base* _obj, vint_variant& _val, int _index); // 0x0045FE40 (:711)
    static bool internal_set_prop_offset(vint_object_base* _obj, const vint_variant& _val, int _index); // 0x0045FEA0 (:719)
    virtual bool get_offset(vint_variant& _val, int _index) const; // vtbl+0x4C 0x0045FE60 (:712)
    virtual bool set_offset(const vint_variant& _val, int _index); // vtbl+0x50 0x0045FEC0 (:720)
    static bool internal_get_prop_tint(const vint_object_base* _obj, vint_variant& _val, int _index); // 0x0045FF10 (:726)
    static bool internal_set_prop_tint(vint_object_base* _obj, const vint_variant& _val, int _index); // 0x0045FF60 (:733)
    virtual bool get_tint(vint_variant& _val, int _index) const; // vtbl+0x54 0x0045FF30 (:727)
    virtual bool set_tint(const vint_variant& _val, int _index); // vtbl+0x58 0x0045FF80 (:734)
    static bool internal_get_prop_alpha(const vint_object_base* _obj, vint_variant& _val, int _index); // 0x0045FFD0 (:740)
    static bool internal_set_prop_alpha(vint_object_base* _obj, const vint_variant& _val, int _index); // 0x00460030 (:747)
    virtual bool get_alpha(vint_variant& _val, int _index) const; // vtbl+0x5C 0x0045FFF0 (:741)
    virtual bool set_alpha(const vint_variant& _val, int _index); // vtbl+0x60 0x00460050 (:748)
    static bool internal_get_prop_depth(const vint_object_base* _obj, vint_variant& _val, int _index); // 0x00460070 (:754)
    static bool internal_set_prop_depth(vint_object_base* _obj, const vint_variant& _val, int _index); // 0x00460090 (:761)
    virtual bool get_depth(vint_variant& _val, int _index) const; // vtbl+0x64 0x00469150 (:755)
    virtual bool set_depth(const vint_variant& _val, int _index); // vtbl+0x68 0x004600B0 (:762)
    static bool internal_get_prop_screen_size(const vint_object_base* _obj, vint_variant& _val, int _index); // 0x00460290 (:805)
    static bool internal_set_prop_screen_size(vint_object_base* _obj, const vint_variant& _val, int _index); // 0x004602B0 (:826)
    virtual bool get_screen_size(vint_variant& _val, int _index) const; // vtbl+0x6C 0x00478D80 (:806)
    virtual bool set_screen_size(const vint_variant& _val, int _index); // vtbl+0x70 0x00478E30 (:827)
    static bool internal_get_prop_screen_nw(const vint_object_base* _obj, vint_variant& _val, int _index); // 0x00460130 (:769)
    static bool internal_set_prop_screen_nw(vint_object_base* _obj, const vint_variant& _val, int _index); // 0x004601B0 (:779)
    virtual bool get_screen_nw(vint_variant& _val, int _index) const; // vtbl+0x74 0x00460150 (:770)
    virtual bool set_screen_nw(const vint_variant& _val, int _index); // vtbl+0x78 0x004601D0 (:780)
    static bool internal_get_prop_screen_se(const vint_object_base* _obj, vint_variant& _val, int _index); // 0x004601E0 (:787)
    static bool internal_set_prop_screen_se(vint_object_base* _obj, const vint_variant& _val, int _index); // 0x00460260 (:797)
    virtual bool get_screen_se(vint_variant& _val, int _index) const; // vtbl+0x7C 0x00460200 (:788)
    virtual bool set_screen_se(const vint_variant& _val, int _index); // vtbl+0x80 0x00460280 (:798)
    static bool internal_get_prop_rotation(const vint_object_base* _obj, vint_variant& _val, int _index); // 0x004602D0 (:862)
    static bool internal_set_prop_rotation(vint_object_base* _obj, const vint_variant& _val, int _index); // 0x00460330 (:870)
    virtual bool get_rotation(vint_variant& _val, int _index) const; // vtbl+0x84 0x004602F0 (:863)
    virtual bool set_rotation(const vint_variant& _val, int _index); // vtbl+0x88 0x00460350 (:871)
    static bool internal_get_prop_scale(const vint_object_base* _obj, vint_variant& _val, int _index); // 0x00460370 (:878)
    static bool internal_set_prop_scale(vint_object_base* _obj, const vint_variant& _val, int _index); // 0x004603D0 (:886)
    virtual bool get_scale(vint_variant& _val, int _index) const; // vtbl+0x8C 0x00460390 (:879)
    virtual bool set_scale(const vint_variant& _val, int _index); // vtbl+0x90 0x004603F0 (:887)
    static bool internal_get_prop_auto_offset(const vint_object_base* _obj, vint_variant& _val, int _index); // 0x00460440 (:894)
    static bool internal_set_prop_auto_offset(vint_object_base* _obj, const vint_variant& _val, int _index); // 0x00460460 (:902)
    virtual bool get_auto_offset(vint_variant& _val, int _index) const; // vtbl+0x94 0x00469170 (:895)
    virtual bool set_auto_offset(const vint_variant& _val, int _index); // vtbl+0x98 0x00460480 (:903)
    static bool internal_get_prop_res_offset_x(const vint_object_base* _obj, vint_variant& _val, int _index); // 0x0045F6A0 (:582)
    static bool internal_set_prop_res_offset_x(vint_object_base* _obj, const vint_variant& _val, int _index); // 0x0045F6C0 (:589)
    virtual bool get_res_offset_x(vint_variant& _val, int _index) const; // vtbl+0x9C 0x004690D0 (:583)
    virtual bool set_res_offset_x(const vint_variant& _val, int _index); // vtbl+0xA0 0x0045F6E0 (:590)
    static bool internal_get_prop_res_offset_y(const vint_object_base* _obj, vint_variant& _val, int _index); // 0x0045F7B0 (:600)
    static bool internal_set_prop_res_offset_y(vint_object_base* _obj, const vint_variant& _val, int _index); // 0x0045F7D0 (:607)
    virtual bool get_res_offset_y(vint_variant& _val, int _index) const; // vtbl+0xA4 0x004690F0 (:601)
    virtual bool set_res_offset_y(const vint_variant& _val, int _index); // vtbl+0xA8 0x0045F7F0 (:608)
    static bool internal_get_prop_res_scale_x(const vint_object_base* _obj, vint_variant& _val, int _index); // 0x0045F8C0 (:618)
    static bool internal_set_prop_res_scale_x(vint_object_base* _obj, const vint_variant& _val, int _index); // 0x0045F8E0 (:625)
    virtual bool get_res_scale_x(vint_variant& _val, int _index) const; // vtbl+0xAC 0x00469110 (:619)
    virtual bool set_res_scale_x(const vint_variant& _val, int _index); // vtbl+0xB0 0x0045F900 (:626)
    static bool internal_get_prop_res_scale_y(const vint_object_base* _obj, vint_variant& _val, int _index); // 0x0045F9D0 (:636)
    static bool internal_set_prop_res_scale_y(vint_object_base* _obj, const vint_variant& _val, int _index); // 0x0045F9F0 (:643)
    virtual bool get_res_scale_y(vint_variant& _val, int _index) const; // vtbl+0xB4 0x00469130 (:637)
    virtual bool set_res_scale_y(const vint_variant& _val, int _index); // vtbl+0xB8 0x0045FA10 (:644)
    void get_res_offsets_and_scales_inherited(vint_res_offset& _res_offset_x, vint_res_offset& _res_offset_y, vint_res_scale& _res_scale_x, vint_res_scale& _res_scale_y); // 0x0045FAE0 (:655)
protected:
    virtual bool init() = 0; // vtbl+0x04 0x004726E0 (:223)
    virtual void clean_up(); // vtbl+0x10 0x00468420 (.h:131)
    virtual void pre_render(); // vtbl+0xBC 0x00468430 (.h:132)
    virtual void render_object(const vint_render_params&) = 0; // vtbl+0xC0
    virtual void render_post_process(); // vtbl+0xC4 0x00468440 (.h:134)
    virtual void update_auto_offset(); // vtbl+0xC8 0x0045F4A0 (:516)

public:
    bool m_is_mask; // 0x12C
    signed char m_render_depth; // 0x12D
private:
    static unsigned int Int_obj_type; // 0x014F5944
public:
    static const unsigned int& Object_type; // 0x014F5948
protected:
    vint_variant_color m_tint; // 0x130
    vector2 m_anchor; // 0x13C
    vector2 m_offset; // 0x144
    vector2 m_size; // 0x14C
    vector2 m_scale; // 0x154
    float m_rotation; // 0x15C
    float m_alpha; // 0x160
    vint_compass m_auto_offset; // 0x164
    vint_render_mode m_render_mode; // 0x168
    bool m_is_visible; // 0x16C
    vint_res_offset m_res_offset_x; // 0x170
    vint_res_offset m_res_offset_y; // 0x174
    vint_res_scale m_res_scale_x; // 0x178
    vint_res_scale m_res_scale_y; // 0x17C
};
