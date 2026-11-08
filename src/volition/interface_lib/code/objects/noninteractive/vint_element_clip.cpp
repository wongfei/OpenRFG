// Original: volition/interface_lib/code/objects/noninteractive/vint_element_clip.cpp
//
// The clip element (UI.md UI2): while enabled, pushes a clip rectangle on its document's clip stack before its
// children draw and pops it after. The rectangle is the global anchor plus the scaled offset, sized by the scaled
// clip size (rotation is ignored), rounded to pixels (a pool of 32). Compiled in interface_lib_unity.cpp.

#include "volition/interface_lib/code/objects/noninteractive/vint_element_clip.h"

#include "volition/interface_lib/code/internal/vint_prop_macros.h"
#include "volition/interface_lib/code/vint_document.h"
#include "volition/vlib/code/math/floating.h"

// ---- class statics
unsigned int vint_element_clip::Int_obj_type = 0xFFFFFFFF; // 0x014F58C4
const unsigned int& vint_element_clip::Object_type = vint_element_clip::Int_obj_type; // 0x014F58C8

// ---- file-static globals
static vint_prop_lookup Prop_lookup_vint_element_clip[2] = { // 0x014F6680
    VINT_PROP_ENTRY(vint_element_clip, clip_size, VINT_PROP_TYPE_VECTOR2F, vector2(20.0f, 20.0f), true),
    VINT_PROP_ENTRY(vint_element_clip, clip_enabled, VINT_PROP_TYPE_BOOL, true, true),
};
static vint_prop_mgr Prop_mgr_vint_element_clip; // 0x01A789A8
static vint_object_factory_imp<vint_element_clip> Factory_vint_element_clip;

// 0x00468680 (.h:30)
vint_element_clip::vint_element_clip()
{
}

// 0x0045E000 (.h:31)
vint_element_clip::~vint_element_clip()
{
}

// 0x004686B0 (.h:38)
bool vint_element_clip::is_type(unsigned int _type) const
{
    return _type == Object_type || _type == vint_element_base::Object_type;
}

// 0x004686E0 (.h:38)
unsigned int vint_element_clip::get_type() const
{
    return Object_type;
}

// 0x00471EE0 (:63)
bool vint_element_clip::system_init()
{
    Int_obj_type = vint_register_object_class("clip", vint_element_base::Object_type, Prop_mgr_vint_element_clip, Prop_lookup_vint_element_clip, 2, Factory_vint_element_clip, 32);
    return true;
}

// 0x0045E010 (:70)
void vint_element_clip::system_shutdown()
{
}

// 0x00475050 (:75)
bool vint_element_clip::init()
{
    if (!vint_element_base::init())
    {
        return false;
    }
    m_clip_size.x = 20.0f;
    m_clip_size.y = 20.0f;
    m_clip_enabled = true;
    return true;
}

// 0x0045E020 (:87)
void vint_element_clip::clean_up()
{
}

// 0x00471F80 (:91)
void vint_element_clip::process()
{
}

// 0x0047D9E0 (:96)
void vint_element_clip::render_object(const vint_render_params& _render_params)
{
    if (!m_clip_enabled)
    {
        return;
    }
    vector2 anchor;
    matrix23 child_xform;
    matrix23 offset_xform;
    get_global_anchor(anchor, child_xform, offset_xform);
    vector2 scale;
    if (parent_object != nullptr)
    {
        ((const vint_element_base*)parent_object)->get_accum_scale(scale);
    }
    else
    {
        scale.x = 1.0f;
        scale.y = 1.0f;
    }
    scale.x = (float)((double)m_scale.x * (double)scale.x);
    scale.y = (float)((double)m_scale.y * (double)scale.y);
    vector2 nw;
    nw.x = (float)((double)(float)((double)m_offset.x * (double)scale.x) + (double)anchor.x);
    nw.y = (float)((double)(float)((double)m_offset.y * (double)scale.y) + (double)anchor.y);
    vector2 se;
    se.x = (float)((double)(float)((double)m_clip_size.x * (double)scale.x) + (double)nw.x);
    se.y = (float)((double)(float)((double)m_clip_size.y * (double)scale.y) + (double)nw.y);
    int bottom = fl_round(se.y);
    int right = fl_round(se.x);
    int top = fl_round(nw.y);
    int left = fl_round(nw.x);
    rect_i region;
    region.nw.x = left;
    region.se.x = right;
    if (right < left)
    {
        region.nw.x = right;
        region.se.x = left;
    }
    region.nw.y = top;
    region.se.y = bottom;
    if (bottom < top)
    {
        region.nw.y = bottom;
        region.se.y = top;
    }
    get_parent_document()->clip_push(region, _render_params.res_offset_x, _render_params.res_offset_y, _render_params.res_scale_x, _render_params.res_scale_y);
}

// 0x00471F90 (:140)
void vint_element_clip::render_post_process()
{
    if (m_clip_enabled)
    {
        get_parent_document()->clip_pop();
    }
}

// 0x0045E030 (:146), 0x0045E090 (:154)
VINT_PROP_WRAPPERS(vint_element_clip, clip_size)

// 0x0045E050 (:147)
bool vint_element_clip::get_clip_size(vint_variant& _val, int _index) const
{
    _val = m_clip_size;
    return true;
}

// 0x0045E0B0 (:155)
bool vint_element_clip::set_clip_size(const vint_variant& _val, int _index)
{
    m_clip_size = (vector2)_val;
    return true;
}

// 0x0045E100 (:162), 0x0045E150 (:170)
VINT_PROP_WRAPPERS(vint_element_clip, clip_enabled)

// 0x0045E120 (:163)
bool vint_element_clip::get_clip_enabled(vint_variant& _val, int _index) const
{
    _val = m_clip_enabled;
    return true;
}

// 0x0045E170 (:171)
bool vint_element_clip::set_clip_enabled(const vint_variant& _val, int _index)
{
    m_clip_enabled = (bool)_val;
    return true;
}
