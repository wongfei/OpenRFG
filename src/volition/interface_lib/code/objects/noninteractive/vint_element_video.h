// Original: volition/interface_lib/code/objects/noninteractive/vint_element_video.h
// Implementation: volition/interface_lib/code/objects/noninteractive/vint_element_video.cpp
//
// A video element (Bink through the render callback draw_video).
#pragma once

#include "volition/interface_lib/code/objects/vint_element_base.h"

class vint_object_base;
class vint_variant;

// size 0x184
class vint_element_video : public vint_element_base
{
public:
    vint_element_video(); // 0x00468FF0 (.h:33)
    virtual ~vint_element_video(); // vtbl+0x00 0x0045F0E0 (.h:34)
    static bool system_init(); // 0x004724E0 (:65)
    static void system_shutdown(); // 0x0045F0F0 (:72)
    virtual void process(); // vtbl+0x0C 0x00472580 (:92)
    virtual bool is_type(unsigned int _type) const; // vtbl+0x18 0x00469020 (.h:41)
    virtual unsigned int get_type() const; // vtbl+0x1C 0x00469050 (.h:41)
    static bool internal_get_prop_vid_id_handle(const vint_object_base* _obj, vint_variant& _val, int _index); // 0x0045F120 (:126)
    static bool internal_set_prop_vid_id_handle(vint_object_base* _obj, const vint_variant& _val, int _index); // 0x0045F140 (:134)
    virtual bool get_vid_id_handle(vint_variant& _val, int _index) const; // vtbl+0xCC 0x00469080 (:127)
    virtual bool set_vid_id_handle(const vint_variant& _val, int _index); // vtbl+0xD0 0x0045F160 (:135)
protected:
    virtual bool init(); // vtbl+0x04 0x004756F0 (:77)
    virtual void clean_up(); // vtbl+0x10 0x0045F100 (:88)
    virtual void render_object(const vint_render_params& _render_params); // vtbl+0xC0 0x00469060 (:97)
    virtual void render_post_process(); // vtbl+0xC4 0x0045F110 (:123)

private:
    static unsigned int Int_obj_type; // 0x014F593C
public:
    static const unsigned int& Object_type; // 0x014F5940
private:
    int m_vid_id_handle; // 0x180
};
