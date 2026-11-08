// Original: volition/interface_lib/code/objects/noninteractive/vint_element_frame_buffer.cpp
//
// The frame buffer element (UI.md UI2): draws a render target (by texture handle) at its global anchor with its offset
// and scale through the draw_frame_buffer callback, tinted by the render parameters (a pool of one). Compiled in
// interface_lib_unity.cpp.

#include "volition/interface_lib/code/objects/noninteractive/vint_element_frame_buffer.h"

#include "volition/interface_lib/code/internal/vint_prop_macros.h"
#include "volition/vlib/code/util/color.h"

// ---- class statics
unsigned int vint_element_frame_buffer::Int_obj_type = 0xFFFFFFFF; // 0x014F58CC
const unsigned int& vint_element_frame_buffer::Object_type = vint_element_frame_buffer::Int_obj_type; // 0x014F58D0

// ---- file-static globals
static vint_prop_lookup Prop_lookup_vint_element_frame_buffer[1] = { // 0x014F6728
    VINT_PROP_ENTRY(vint_element_frame_buffer, texture_handle, VINT_PROP_TYPE_UINT, 0, true),
};
static vint_prop_mgr Prop_mgr_vint_element_frame_buffer; // 0x01A78990
static vint_object_factory_imp<vint_element_frame_buffer> Factory_vint_element_frame_buffer;

// 0x004686F0 (.h:33)
vint_element_frame_buffer::vint_element_frame_buffer()
{
}

// 0x0045E210 (.h:34)
vint_element_frame_buffer::~vint_element_frame_buffer()
{
}

// 0x00468720 (.h:41)
bool vint_element_frame_buffer::is_type(unsigned int _type) const
{
    return _type == Object_type || _type == vint_element_base::Object_type;
}

// 0x00468750 (.h:41)
unsigned int vint_element_frame_buffer::get_type() const
{
    return Object_type;
}

// 0x00471FC0 (:65)
bool vint_element_frame_buffer::system_init()
{
    Int_obj_type = vint_register_object_class("framebuffer", vint_element_base::Object_type, Prop_mgr_vint_element_frame_buffer, Prop_lookup_vint_element_frame_buffer, 1, Factory_vint_element_frame_buffer, 1);
    return true;
}

// 0x0045E220 (:72)
void vint_element_frame_buffer::system_shutdown()
{
}

// 0x00475080 (:77)
bool vint_element_frame_buffer::init()
{
    if (!vint_element_base::init())
    {
        return false;
    }
    m_texture_handle = 0;
    return true;
}

// 0x0045E230 (:88)
void vint_element_frame_buffer::clean_up()
{
}

// 0x00472060 (:92)
void vint_element_frame_buffer::process()
{
}

// 0x0047DBE0 (:97)
void vint_element_frame_buffer::render_object(const vint_render_params& _render_params)
{
    vector2 anchor;
    matrix23 child_xform;
    matrix23 offset_xform;
    get_global_anchor(anchor, child_xform, offset_xform);
    color c;
    c.red = (unsigned char)(int)((double)_render_params.tint.r * 255.0);
    c.alpha = (unsigned char)(int)((double)_render_params.alpha * 255.0);
    c.green = (unsigned char)(int)((double)_render_params.tint.g * 255.0);
    c.blue = (unsigned char)(int)((double)_render_params.tint.b * 255.0);
    if (Render_callbacks == nullptr)
    {
        return;
    }
    if (Render_callbacks->set_color != nullptr)
    {
        Render_callbacks->set_color(c);
    }
    if (Render_callbacks != nullptr && Render_callbacks->draw_frame_buffer != nullptr)
    {
        // the element's own offset and scale (not the combined ones)
        Render_callbacks->draw_frame_buffer(m_texture_handle, anchor, m_offset, m_scale);
    }
}

// 0x0045E240 (:126)
void vint_element_frame_buffer::render_post_process()
{
}

// 0x0045E250 (:129), 0x0045E270 (:137)
VINT_PROP_WRAPPERS(vint_element_frame_buffer, texture_handle)

// 0x00468760 (:130)
bool vint_element_frame_buffer::get_texture_handle(vint_variant& _val, int _index) const
{
    _val = m_texture_handle;
    return true;
}

// 0x0045E290 (:138)
bool vint_element_frame_buffer::set_texture_handle(const vint_variant& _val, int _index)
{
    m_texture_handle = (int)_val;
    return true;
}
