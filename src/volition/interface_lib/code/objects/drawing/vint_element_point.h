// Original: volition/interface_lib/code/objects/drawing/vint_element_point.h
// Implementation: volition/interface_lib/code/objects/drawing/vint_element_point.cpp
//
// A point element (an anchor with no drawing).
#pragma once

#include "volition/interface_lib/code/objects/vint_element_base.h"

class vint_object_base;
class vint_variant;

// size 0x180
class vint_element_point : public vint_element_base
{
public:
    vint_element_point(); // 0x00468470 (.h:33)
    virtual ~vint_element_point(); // vtbl+0x00 0x0045D610 (.h:34)
    static bool system_init(); // 0x00471AE0 (:65)
    static void system_shutdown(); // 0x0045D620 (:72)
    virtual void process(); // vtbl+0x0C 0x00471B80 (:92)
    virtual bool is_type(unsigned int _type) const; // vtbl+0x18 0x004684A0 (.h:41)
    virtual unsigned int get_type() const; // vtbl+0x1C 0x004684D0 (.h:41)
    static bool internal_get_prop_screen_size(const vint_object_base* _obj, vint_variant& _val, int _index); // 0x0045D650 (:102)
    static bool internal_set_prop_screen_size(vint_object_base* _obj, const vint_variant& _val, int _index); // 0x0045D6A0 (:110)
    virtual bool get_screen_size(vint_variant& _val, int _index) const; // vtbl+0x6C 0x0045D670 (:103)
    virtual bool set_screen_size(const vint_variant& _val, int _index); // vtbl+0x70 0x0045D6C0 (:111)
protected:
    virtual bool init(); // vtbl+0x04 0x00474AC0 (:77)
    virtual void clean_up(); // vtbl+0x10 0x0045D630 (:88)
    virtual void render_object(const vint_render_params& _render_params); // vtbl+0xC0 0x0045D640 (:97)

private:
    static unsigned int Int_obj_type; // 0x014F58AC
public:
    static const unsigned int& Object_type; // 0x014F58B0
};
