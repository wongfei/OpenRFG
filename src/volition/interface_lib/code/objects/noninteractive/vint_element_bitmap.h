// Original: volition/interface_lib/code/objects/noninteractive/vint_element_bitmap.h
// Implementation: volition/interface_lib/code/objects/noninteractive/vint_element_bitmap.cpp
//
// A bitmap element: image (bitmap or sheet image), UV, tiling, flips and custom source rectangle.
#pragma once

#include "volition/interface_lib/code/objects/vint_element_base.h"
#include "volition/interface_lib/code/vint_external.h"
#include "volition/interface_lib/code/vint_variant.h"

class vint_object_base;

// size 0x1C0
class vint_element_bitmap : public vint_element_base
{
public:
    vint_element_bitmap(); // 0x00477180 (.h:36)
    virtual ~vint_element_bitmap(); // vtbl+0x00 0x00477210 (.h:37)
    static bool system_init(); // 0x0047D8A0 (:76)
    static void system_shutdown(); // 0x0045D750 (:82)
    virtual void process(); // vtbl+0x0C 0x00471B90 (:114)
    static vint_element_bitmap* load_from_xml();
    virtual bool is_type(unsigned int _type) const; // vtbl+0x18 0x004771D0 (.h:46)
    virtual unsigned int get_type() const; // vtbl+0x1C 0x00477200 (.h:46)
    static bool internal_get_prop_image(const vint_object_base* _obj, vint_variant& _val, int _index); // 0x0045D760 (:138)
    static bool internal_set_prop_image(vint_object_base* _obj, const vint_variant& _val, int _index); // 0x0045D780 (:160)
    virtual bool get_image(vint_variant& _val, int _index) const; // vtbl+0xCC 0x00474B90 (:139)
    virtual bool set_image(const vint_variant& _val, int _index); // vtbl+0xD0 0x00474BD0 (:161)
    static bool internal_get_prop_image_crc(const vint_object_base* _obj, vint_variant& _val, int _index); // 0x0045D7A0 (:241)
    static bool internal_set_prop_image_crc(vint_object_base* _obj, const vint_variant& _val, int _index); // 0x0045D7D0 (:249)
    virtual bool get_image_crc(vint_variant& _val, int _index) const; // vtbl+0xD4 0x0045D7C0 (:242)
    virtual bool set_image_crc(const vint_variant& _val, int _index); // vtbl+0xD8 0x00474D20 (:250)
    static bool internal_get_prop_image_raw(const vint_object_base* _obj, vint_variant& _val, int _index); // 0x0045D7F0 (:284)
    static bool internal_set_prop_image_raw(vint_object_base* _obj, const vint_variant& _val, int _index); // 0x0045D820 (:291)
    virtual bool get_image_raw(vint_variant& _val, int _index) const; // vtbl+0xDC 0x0045D810 (:285)
    virtual bool set_image_raw(const vint_variant& _val, int _index); // vtbl+0xE0 0x00477240 (:292)
    static bool internal_get_prop_image_badge(const vint_object_base* _obj, vint_variant& _val, int _index); // 0x0045D840 (:341)
    static bool internal_set_prop_image_badge(vint_object_base* _obj, const vint_variant& _val, int _index); // 0x0045D870 (:348)
    virtual bool get_image_badge(vint_variant& _val, int _index) const; // vtbl+0xE4 0x0045D860 (:342)
    virtual bool set_image_badge(const vint_variant& _val, int _index); // vtbl+0xE8 0x00471D60 (:349)
    static bool internal_get_prop_source_nw(const vint_object_base* _obj, vint_variant& _val, int _index); // 0x0045D890 (:376)
    static bool internal_set_prop_source_nw(vint_object_base* _obj, const vint_variant& _val, int _index); // 0x0045D8F0 (:384)
    virtual bool get_source_nw(vint_variant& _val, int _index) const; // vtbl+0xEC 0x0045D8B0 (:377)
    virtual bool set_source_nw(const vint_variant& _val, int _index); // vtbl+0xF0 0x0045D910 (:385)
    static bool internal_get_prop_source_se(const vint_object_base* _obj, vint_variant& _val, int _index); // 0x0045D970 (:404)
    static bool internal_set_prop_source_se(vint_object_base* _obj, const vint_variant& _val, int _index); // 0x0045D9D0 (:413)
    virtual bool get_source_se(vint_variant& _val, int _index) const; // vtbl+0xF4 0x0045D990 (:405)
    virtual bool set_source_se(const vint_variant& _val, int _index); // vtbl+0xF8 0x0045D9F0 (:414)
    static bool internal_get_prop_custom_source_coords(const vint_object_base* _obj, vint_variant& _val, int _index); // 0x0045DB10 (:442)
    static bool internal_set_prop_custom_source_coords(vint_object_base* _obj, const vint_variant& _val, int _index); // 0x0045DA50 (:434)
    virtual bool get_custom_source_coords(vint_variant& _val, int _index) const; // vtbl+0xFC 0x0045DB30 (:443)
    virtual bool set_custom_source_coords(const vint_variant& _val, int _index); // vtbl+0x100 0x0045DA70 (:435)
    static bool internal_get_prop_image_size(const vint_object_base* _obj, vint_variant& _val, int _index); // 0x0045DB60 (:451)
    static bool internal_set_prop_image_size(vint_object_base* _obj, const vint_variant& _val, int _index); // 0x0045DBC0 (:460)
    virtual bool get_image_size(vint_variant& _val, int _index) const; // vtbl+0x104 0x0045DB80 (:452)
    virtual bool set_image_size(const vint_variant& _val, int _index); // vtbl+0x108 0x0045DBE0 (:461)
protected:
    virtual bool init(); // vtbl+0x04 0x00474AF0 (:87)
    virtual void clean_up(); // vtbl+0x10 0x00474B60 (:108)
    virtual void render_object(const vint_render_params& _render_params); // vtbl+0xC0 0x00471BA0 (:119)

private:
    static unsigned int Int_obj_type; // 0x014F58B4
public:
    static const unsigned int& Object_type; // 0x014F58B8
protected:
    vint_variant m_image; // 0x180
    vector2i m_source_nw; // 0x1A0
    vector2i m_source_se; // 0x1A8
    vector2i m_image_size; // 0x1B0
    unsigned int m_raw_image; // 0x1B8
    bool m_custom_source_coords; // 0x1BC
    bool m_raw_image_is_badge; // 0x1BD
};
