// Original: volition/interface_lib/code/objects/noninteractive/vint_element_group.h
// Implementation: volition/interface_lib/code/objects/noninteractive/vint_element_group.cpp
//
// A group element: a container that transforms its children.
#pragma once

#include "volition/interface_lib/code/objects/vint_element_base.h"

struct bbox2d;
class vector2;
class vint_object_base;
class vint_variant;

// size 0x180
class vint_element_group : public vint_element_base
{
public:
    vint_element_group(); // 0x00468780 (.h:33)
    virtual ~vint_element_group(); // vtbl+0x00 0x0045E310 (.h:34)
    static bool system_init(); // 0x00472070 (:68)
    static void system_shutdown(); // 0x0045E320 (:75)
    virtual void get_bbox(bbox2d& box, bool adjust_min_and_max) const; // vtbl+0x24 0x0047DCD0 (:154)
    virtual void process(); // vtbl+0x0C 0x00472110 (:93)
    virtual bool is_type(unsigned int _type) const; // vtbl+0x18 0x004687B0 (.h:43)
    virtual unsigned int get_type() const; // vtbl+0x1C 0x004687E0 (.h:43)
    static bool internal_get_prop_screen_size(const vint_object_base* _obj, vint_variant& _val, int _index); // 0x0045E360 (:192)
    static bool internal_set_prop_screen_size(vint_object_base* _obj, const vint_variant& _val, int _index); // 0x0045E380 (:199)
    virtual bool get_screen_size(vint_variant& _val, int _index) const; // vtbl+0x6C 0x0047DE50 (:193)
    virtual bool set_screen_size(const vint_variant& _val, int _index); // vtbl+0x70 0x0045E3A0 (:200)
    static bool internal_get_prop_screen_se(const vint_object_base* _obj, vint_variant& _val, int _index); // 0x0045E3B0 (:206)
    static bool internal_set_prop_screen_se(vint_object_base* _obj, const vint_variant& _val, int _index); // 0x0045E3D0 (:213)
    virtual bool get_screen_se(vint_variant& _val, int _index) const; // vtbl+0x7C 0x004687F0 (:207)
    virtual bool set_screen_se(const vint_variant& _val, int _index); // vtbl+0x80 0x0045E3F0 (:214)
    static bool internal_get_prop_screen_nw(const vint_object_base* _obj, vint_variant& _val, int _index); // 0x0045E400 (:221)
    static bool internal_set_prop_screen_nw(vint_object_base* _obj, const vint_variant& _val, int _index); // 0x0045E420 (:227)
    virtual bool get_screen_nw(vint_variant& _val, int _index) const; // vtbl+0x74 0x00468850 (:222)
    virtual bool set_screen_nw(const vint_variant& _val, int _index); // vtbl+0x78 0x0045E440 (:228)
protected:
    virtual bool init(); // vtbl+0x04 0x004750A0 (:80)
    virtual void clean_up(); // vtbl+0x10 0x0045E330 (:89)
    virtual void render_object(const vint_render_params& _render_params); // vtbl+0xC0 0x0045E340 (:98)
    void get_screen_dims(vector2& nw, vector2& se) const; // 0x004775D0 (:137)
    static void derive_dims_from_children_rc(const vint_element_base* _element, vector2& _nw, vector2& _se); // 0x00477430 (:104)

private:
    static unsigned int Int_obj_type; // 0x014F58D4
public:
    static const unsigned int& Object_type; // 0x014F58D8
};
