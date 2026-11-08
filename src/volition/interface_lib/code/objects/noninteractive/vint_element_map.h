// Original: volition/interface_lib/code/objects/noninteractive/vint_element_map.h
// Implementation: volition/interface_lib/code/objects/noninteractive/vint_element_map.cpp
//
// A map element: the game draws the minimap or the full screen map into it (render callback draw_map).
#pragma once

#include "volition/interface_lib/code/objects/noninteractive/vint_element_bitmap.h"

class vint_object_base;
struct vint_render_params;
class vint_variant;

// size 0x1C8
class vint_element_map : public vint_element_bitmap
{
public:
    vint_element_map(); // 0x00477670 (.h:34)
    virtual ~vint_element_map(); // vtbl+0x00 0x00477700 (.h:35)
    static bool system_init(); // 0x0047DF00 (:65)
    virtual bool is_type(unsigned int _type) const; // vtbl+0x18 0x004776C0 (.h:42)
    virtual unsigned int get_type() const; // vtbl+0x1C 0x004776F0 (.h:42)
    static bool internal_get_prop_map_bogus(const vint_object_base* _obj, vint_variant& _val, int _index); // 0x0045E450 (:79)
    static bool internal_set_prop_map_bogus(vint_object_base* _obj, const vint_variant& _val, int _index); // 0x0045E470 (:86)
    virtual bool get_map_bogus(vint_variant& _val, int _index) const; // vtbl+0x10C 0x004688D0 (:80)
    virtual bool set_map_bogus(const vint_variant& _val, int _index); // vtbl+0x110 0x0045E490 (:87)
protected:
    virtual void render_object(const vint_render_params& _render_params); // vtbl+0xC0 0x004688B0 (:73)

private:
    static unsigned int Int_obj_type; // 0x014F58DC
public:
    static const unsigned int& Object_type; // 0x014F58E0
private:
    int m_map_bogus; // 0x1C0
};
