// Original: volition/interface_lib/code/objects/noninteractive/vint_element_bitmap_circle.h
// Implementation: volition/interface_lib/code/objects/noninteractive/vint_element_bitmap_circle.cpp
//
// A bitmap drawn as a circle segment (start and end angle): meters and timers.
#pragma once

#include "volition/interface_lib/code/objects/vint_element_base.h"
#include "volition/interface_lib/code/vint_external.h"
#include "volition/interface_lib/code/vint_variant.h"

class vint_object_base;

// size 0x1C8
class vint_element_bitmap_circle : public vint_element_base
{
public:
    vint_element_bitmap_circle(); // 0x00477370 (.h:35)
    virtual ~vint_element_bitmap_circle(); // vtbl+0x00 0x00477400 (.h:36)
    static bool system_init(); // 0x0047D940 (:74)
    static void system_shutdown(); // 0x0045DCA0 (:80)
    virtual void process(); // vtbl+0x0C 0x00471DB0 (:108)
    static vint_element_bitmap_circle* load_from_xml();
    virtual bool is_type(unsigned int _type) const; // vtbl+0x18 0x004773C0 (.h:45)
    virtual unsigned int get_type() const; // vtbl+0x1C 0x004773F0 (.h:45)
    static bool internal_get_prop_image(const vint_object_base* _obj, vint_variant& _val, int _index); // 0x0045DCB0 (:123)
    static bool internal_set_prop_image(vint_object_base* _obj, const vint_variant& _val, int _index); // 0x0045DCD0 (:136)
    virtual bool get_image(vint_variant& _val, int _index) const; // vtbl+0xCC 0x00474F30 (:124)
    virtual bool set_image(const vint_variant& _val, int _index); // vtbl+0xD0 0x00474F70 (:137)
    static bool internal_get_prop_source_nw(const vint_object_base* _obj, vint_variant& _val, int _index); // 0x0045DCF0 (:188)
    static bool internal_set_prop_source_nw(vint_object_base* _obj, const vint_variant& _val, int _index); // 0x0045DD50 (:196)
    virtual bool get_source_nw(vint_variant& _val, int _index) const; // vtbl+0xD4 0x0045DD10 (:189)
    virtual bool set_source_nw(const vint_variant& _val, int _index); // vtbl+0xD8 0x00468520 (:197)
    static bool internal_get_prop_source_se(const vint_object_base* _obj, vint_variant& _val, int _index); // 0x0045DD70 (:214)
    static bool internal_set_prop_source_se(vint_object_base* _obj, const vint_variant& _val, int _index); // 0x0045DDD0 (:222)
    virtual bool get_source_se(vint_variant& _val, int _index) const; // vtbl+0xDC 0x0045DD90 (:215)
    virtual bool set_source_se(const vint_variant& _val, int _index); // vtbl+0xE0 0x004685D0 (:223)
    static bool internal_get_prop_start_angle(const vint_object_base* _obj, vint_variant& _val, int _index); // 0x0045DDF0 (:240)
    static bool internal_set_prop_start_angle(vint_object_base* _obj, const vint_variant& _val, int _index); // 0x0045DE50 (:248)
    virtual bool get_start_angle(vint_variant& _val, int _index) const; // vtbl+0xE4 0x0045DE10 (:241)
    virtual bool set_start_angle(const vint_variant& _val, int _index); // vtbl+0xE8 0x0045DE70 (:249)
    static bool internal_get_prop_end_angle(const vint_object_base* _obj, vint_variant& _val, int _index); // 0x0045DE90 (:256)
    static bool internal_set_prop_end_angle(vint_object_base* _obj, const vint_variant& _val, int _index); // 0x0045DEF0 (:264)
    virtual bool get_end_angle(vint_variant& _val, int _index) const; // vtbl+0xEC 0x0045DEB0 (:257)
    virtual bool set_end_angle(const vint_variant& _val, int _index); // vtbl+0xF0 0x0045DF10 (:265)
    static bool internal_get_prop_num_wedges(const vint_object_base* _obj, vint_variant& _val, int _index); // 0x0045DF30 (:272)
    static bool internal_set_prop_num_wedges(vint_object_base* _obj, const vint_variant& _val, int _index); // 0x0045DF50 (:280)
    virtual bool get_num_wedges(vint_variant& _val, int _index) const; // vtbl+0xF4 0x00471EC0 (:273)
    virtual bool set_num_wedges(const vint_variant& _val, int _index); // vtbl+0xF8 0x0045DF70 (:281)
protected:
    virtual bool init(); // vtbl+0x04 0x00474E80 (:85)
    virtual void clean_up(); // vtbl+0x10 0x00474F00 (:102)
    virtual void render_object(const vint_render_params& _render_params); // vtbl+0xC0 0x00471DC0 (:113)

private:
    static unsigned int Int_obj_type; // 0x014F58BC
public:
    static const unsigned int& Object_type; // 0x014F58C0
protected:
    vint_variant m_image; // 0x180
    vector2i m_source_nw; // 0x1A0
    vector2i m_source_se; // 0x1A8
    vector2i m_image_size; // 0x1B0
    float m_start_angle; // 0x1B8
    float m_end_angle; // 0x1BC
    unsigned int m_num_wedges; // 0x1C0
};
