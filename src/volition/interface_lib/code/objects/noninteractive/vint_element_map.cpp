// Original: volition/interface_lib/code/objects/noninteractive/vint_element_map.cpp
//
// The map element (UI.md UI2): a bitmap element whose drawing is the game's map (the minimap) through the draw_map
// callback, always with id 0 (a pool of two). Compiled in interface_lib_unity.cpp after vint_element_bitmap.cpp.

#include "volition/interface_lib/code/objects/noninteractive/vint_element_map.h"

#include "volition/interface_lib/code/internal/vint_prop_macros.h"

// ---- class statics
unsigned int vint_element_map::Int_obj_type = 0xFFFFFFFF; // 0x014F58DC
const unsigned int& vint_element_map::Object_type = vint_element_map::Int_obj_type; // 0x014F58E0

// ---- file-static globals
static vint_prop_lookup Prop_lookup_vint_element_map[1] = { // 0x014F68C0
    VINT_PROP_ENTRY(vint_element_map, map_bogus, VINT_PROP_TYPE_INT, 0, false),
};
static vint_prop_mgr Prop_mgr_vint_element_map; // 0x01A77368
static vint_object_factory_imp<vint_element_map> Factory_vint_element_map;

// 0x00477670 (.h:34)
vint_element_map::vint_element_map()
{
}

// 0x00477700 (.h:35)
vint_element_map::~vint_element_map()
{
}

// 0x004776C0 (.h:42)
bool vint_element_map::is_type(unsigned int _type) const
{
    return _type == Object_type || _type == vint_element_bitmap::Object_type || _type == vint_element_base::Object_type;
}

// 0x004776F0 (.h:42)
unsigned int vint_element_map::get_type() const
{
    return Object_type;
}

// 0x0047DF00 (:65)
bool vint_element_map::system_init()
{
    Int_obj_type = vint_register_object_class("map", vint_element_bitmap::Object_type, Prop_mgr_vint_element_map, Prop_lookup_vint_element_map, 1, Factory_vint_element_map, 2);
    return true;
}

// 0x004688B0 (:73)
void vint_element_map::render_object(const vint_render_params& _render_params)
{
    if (Render_callbacks != nullptr && Render_callbacks->draw_map != nullptr)
    {
        Render_callbacks->draw_map(0);
    }
}

// 0x0045E450 (:79), 0x0045E470 (:86)
VINT_PROP_WRAPPERS(vint_element_map, map_bogus)

// 0x004688D0 (:80)
bool vint_element_map::get_map_bogus(vint_variant& _val, int _index) const
{
    _val = m_map_bogus;
    return true;
}

// 0x0045E490 (:87)
bool vint_element_map::set_map_bogus(const vint_variant& _val, int _index)
{
    m_map_bogus = (int)_val;
    return true;
}
