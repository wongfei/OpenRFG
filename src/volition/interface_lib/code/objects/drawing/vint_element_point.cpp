// Original: volition/interface_lib/code/objects/drawing/vint_element_point.cpp
//
// The point element (UI.md UI2): an anchor with a 1x1 size that draws nothing (a pool of one). Compiled in
// interface_lib_unity.cpp.

#include "volition/interface_lib/code/objects/drawing/vint_element_point.h"

#include "volition/interface_lib/code/internal/vint_prop_macros.h"

// ---- class statics
unsigned int vint_element_point::Int_obj_type = 0xFFFFFFFF; // 0x014F58AC
const unsigned int& vint_element_point::Object_type = vint_element_point::Int_obj_type; // 0x014F58B0

// ---- file-static globals
static vint_prop_lookup Prop_lookup_vint_element_point[1] = { // 0x014F61B8
    VINT_PROP_ENTRY(vint_element_point, screen_size, VINT_PROP_TYPE_VECTOR2F, vector2(1.0f, 1.0f), false),
};
static vint_prop_mgr Prop_mgr_vint_element_point; // 0x01A4872C
static vint_object_factory_imp<vint_element_point> Factory_vint_element_point;

// 0x00468470 (.h:33)
vint_element_point::vint_element_point()
{
}

// 0x0045D610 (.h:34)
vint_element_point::~vint_element_point()
{
}

// 0x004684A0 (.h:41)
bool vint_element_point::is_type(unsigned int _type) const
{
    return _type == Object_type || _type == vint_element_base::Object_type;
}

// 0x004684D0 (.h:41)
unsigned int vint_element_point::get_type() const
{
    return Object_type;
}

// 0x00471AE0 (:65)
bool vint_element_point::system_init()
{
    Int_obj_type = vint_register_object_class("point", vint_element_base::Object_type, Prop_mgr_vint_element_point, Prop_lookup_vint_element_point, 1, Factory_vint_element_point, 1);
    return true;
}

// 0x0045D620 (:72)
void vint_element_point::system_shutdown()
{
}

// 0x00474AC0 (:77)
bool vint_element_point::init()
{
    if (!vint_element_base::init())
    {
        return false;
    }
    m_size.x = 1.0f;
    m_size.y = 1.0f;
    return true;
}

// 0x0045D630 (:88)
void vint_element_point::clean_up()
{
}

// 0x00471B80 (:92)
void vint_element_point::process()
{
}

// 0x0045D640 (:97)
void vint_element_point::render_object(const vint_render_params& _render_params)
{
}

// 0x0045D650 (:102), 0x0045D6A0 (:110)
VINT_PROP_WRAPPERS(vint_element_point, screen_size)

// 0x0045D670 (:103)
bool vint_element_point::get_screen_size(vint_variant& _val, int _index) const
{
    _val.set_vector2f(1.0f, 1.0f);
    return true;
}

// 0x0045D6C0 (:111)
bool vint_element_point::set_screen_size(const vint_variant& _val, int _index)
{
    return false;
}
