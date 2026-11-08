// Original: volition/interface_lib/code/objects/noninteractive/vint_element_frame_buffer.h
// Implementation: volition/interface_lib/code/objects/noninteractive/vint_element_frame_buffer.cpp
//
// A frame buffer element: draws a render target the game fills (the multiplayer character preview).
#pragma once

#include "volition/interface_lib/code/objects/vint_element_base.h"

class vint_object_base;
class vint_variant;

// size 0x184
class vint_element_frame_buffer : public vint_element_base
{
public:
    vint_element_frame_buffer(); // 0x004686F0 (.h:33)
    virtual ~vint_element_frame_buffer(); // vtbl+0x00 0x0045E210 (.h:34)
    static bool system_init(); // 0x00471FC0 (:65)
    static void system_shutdown(); // 0x0045E220 (:72)
    virtual void process(); // vtbl+0x0C 0x00472060 (:92)
    virtual bool is_type(unsigned int _type) const; // vtbl+0x18 0x00468720 (.h:41)
    virtual unsigned int get_type() const; // vtbl+0x1C 0x00468750 (.h:41)
    static bool internal_get_prop_texture_handle(const vint_object_base* _obj, vint_variant& _val, int _index); // 0x0045E250 (:129)
    static bool internal_set_prop_texture_handle(vint_object_base* _obj, const vint_variant& _val, int _index); // 0x0045E270 (:137)
    virtual bool get_texture_handle(vint_variant& _val, int _index) const; // vtbl+0xCC 0x00468760 (:130)
    virtual bool set_texture_handle(const vint_variant& _val, int _index); // vtbl+0xD0 0x0045E290 (:138)
protected:
    virtual bool init(); // vtbl+0x04 0x00475080 (:77)
    virtual void clean_up(); // vtbl+0x10 0x0045E230 (:88)
    virtual void render_object(const vint_render_params& _render_params); // vtbl+0xC0 0x0047DBE0 (:97)
    virtual void render_post_process(); // vtbl+0xC4 0x0045E240 (:126)

private:
    static unsigned int Int_obj_type; // 0x014F58CC
public:
    static const unsigned int& Object_type; // 0x014F58D0
private:
    int m_texture_handle; // 0x180
};
