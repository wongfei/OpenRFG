// Original: volition/interface_lib/code/objects/noninteractive/vint_element_clip.h
// Implementation: volition/interface_lib/code/objects/noninteractive/vint_element_clip.cpp
//
// A clip element: its children draw inside its rectangle (the document's clip stack, stencil or scissor).
#pragma once

#include "volition/interface_lib/code/objects/vint_element_base.h"
#include "volition/vlib/code/math/vector2.h"

class vint_object_base;
class vint_variant;

// size 0x18C
class vint_element_clip : public vint_element_base
{
public:
    vint_element_clip(); // 0x00468680 (.h:30)
    virtual ~vint_element_clip(); // vtbl+0x00 0x0045E000 (.h:31)
    static bool system_init(); // 0x00471EE0 (:63)
    static void system_shutdown(); // 0x0045E010 (:70)
    virtual void process(); // vtbl+0x0C 0x00471F80 (:91)
    virtual bool is_type(unsigned int _type) const; // vtbl+0x18 0x004686B0 (.h:38)
    virtual unsigned int get_type() const; // vtbl+0x1C 0x004686E0 (.h:38)
    static bool internal_get_prop_clip_size(const vint_object_base* _obj, vint_variant& _val, int _index); // 0x0045E030 (:146)
    static bool internal_set_prop_clip_size(vint_object_base* _obj, const vint_variant& _val, int _index); // 0x0045E090 (:154)
    virtual bool get_clip_size(vint_variant& _val, int _index) const; // vtbl+0xCC 0x0045E050 (:147)
    virtual bool set_clip_size(const vint_variant& _val, int _index); // vtbl+0xD0 0x0045E0B0 (:155)
    static bool internal_get_prop_clip_enabled(const vint_object_base* _obj, vint_variant& _val, int _index); // 0x0045E100 (:162)
    static bool internal_set_prop_clip_enabled(vint_object_base* _obj, const vint_variant& _val, int _index); // 0x0045E150 (:170)
    virtual bool get_clip_enabled(vint_variant& _val, int _index) const; // vtbl+0xD4 0x0045E120 (:163)
    virtual bool set_clip_enabled(const vint_variant& _val, int _index); // vtbl+0xD8 0x0045E170 (:171)
protected:
    virtual bool init(); // vtbl+0x04 0x00475050 (:75)
    virtual void clean_up(); // vtbl+0x10 0x0045E020 (:87)
    virtual void render_object(const vint_render_params& _render_params); // vtbl+0xC0 0x0047D9E0 (:96)
    virtual void render_post_process(); // vtbl+0xC4 0x00471F90 (:140)

private:
    static unsigned int Int_obj_type; // 0x014F58C4
public:
    static const unsigned int& Object_type; // 0x014F58C8
private:
    vector2 m_clip_size; // 0x180
    bool m_clip_enabled; // 0x188
};
