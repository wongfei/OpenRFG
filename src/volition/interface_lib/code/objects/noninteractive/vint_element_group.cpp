// Original: volition/interface_lib/code/objects/noninteractive/vint_element_group.cpp
//
// The group element (UI.md UI2): draws nothing; its bounds are the union of its element children's (or, with none,
// the point at its anchor plus rotated offset). The screen corners come from the bounds; they cannot be set. A pool
// of 314. Compiled in interface_lib_unity.cpp.

#include "volition/interface_lib/code/objects/noninteractive/vint_element_group.h"

#include "volition/interface_lib/code/internal/vint_prop_macros.h"
#include "volition/interface_lib/code/objects/noninteractive/vint_element_text.h"
#include "volition/vlib/code/math/bbox2d.h"

// ---- class statics
unsigned int vint_element_group::Int_obj_type = 0xFFFFFFFF; // 0x014F58D4
const unsigned int& vint_element_group::Object_type = vint_element_group::Int_obj_type; // 0x014F58D8

// ---- file-static globals
static vint_prop_lookup Prop_lookup_vint_element_group[4] = { // 0x014F6788
    VINT_PROP_ENTRY(vint_element_group, screen_size, VINT_PROP_TYPE_VECTOR2F, vector2(0.0f, 0.0f), false),
    VINT_PROP_ENTRY(vint_element_group, screen_se, VINT_PROP_TYPE_VECTOR2F, vector2(0.0f, 0.0f), false),
    VINT_PROP_ENTRY(vint_element_group, screen_nw, VINT_PROP_TYPE_VECTOR2F, vector2(0.0f, 0.0f), false),
    // the base element's offset, repeated in the group's table
    VINT_PROP_ENTRY(vint_element_base, offset, VINT_PROP_TYPE_VECTOR2F, vector2(0.0f, 0.0f), true),
};
static vint_prop_mgr Prop_mgr_vint_element_group; // 0x01A7899C
static vint_object_factory_imp<vint_element_group> Factory_vint_element_group;

// 0x00468780 (.h:33)
vint_element_group::vint_element_group()
{
}

// 0x0045E310 (.h:34)
vint_element_group::~vint_element_group()
{
}

// 0x004687B0 (.h:43)
bool vint_element_group::is_type(unsigned int _type) const
{
    return _type == Object_type || _type == vint_element_base::Object_type;
}

// 0x004687E0 (.h:43)
unsigned int vint_element_group::get_type() const
{
    return Object_type;
}

// 0x00472070 (:68)
bool vint_element_group::system_init()
{
    Int_obj_type = vint_register_object_class("group", vint_element_base::Object_type, Prop_mgr_vint_element_group, Prop_lookup_vint_element_group, 4, Factory_vint_element_group, 314);
    return true;
}

// 0x0045E320 (:75)
void vint_element_group::system_shutdown()
{
}

// 0x004750A0 (:80)
bool vint_element_group::init()
{
    return vint_element_base::init();
}

// 0x0045E330 (:89)
void vint_element_group::clean_up()
{
}

// 0x00472110 (:93)
void vint_element_group::process()
{
}

// 0x0045E340 (:98)
void vint_element_group::render_object(const vint_render_params& _render_params)
{
}

// 0x00477430 (:104)
void vint_element_group::derive_dims_from_children_rc(const vint_element_base* _element, vector2& _nw, vector2& _se)
{
    if (_element == nullptr)
    {
        return;
    }
    vint_variant nw_value;
    vint_variant se_value;
    _element->get_screen_nw(nw_value, 0);
    _element->get_screen_se(se_value, 0);
    vector2 nw = (vector2)nw_value;
    vector2 se = (vector2)se_value;
    if ((float)((double)nw.x - (double)_nw.x) < 0.0f)
    {
        _nw.x = nw.x;
    }
    if ((float)((double)nw.y - (double)_nw.y) < 0.0f)
    {
        _nw.y = nw.y;
    }
    if ((float)((double)se.x - (double)_se.x) >= 0.0f)
    {
        _se.x = se.x;
    }
    if ((float)((double)se.y - (double)_se.y) >= 0.0f)
    {
        _se.y = se.y;
    }
    // Original: a group's bounds already cover its children, so the walk stops there; but so does the walk over the
    // group's following siblings.
    if (_element->is_type(Object_type))
    {
        return;
    }
    vint_object_base* sibling = _element->next_sibling;
    if (sibling != nullptr && sibling->is_type(vint_element_base::Object_type))
    {
        derive_dims_from_children_rc((const vint_element_base*)sibling, _nw, _se);
    }
    vint_object_base* child = _element->first_child;
    if (child != nullptr && child->is_type(vint_element_base::Object_type))
    {
        derive_dims_from_children_rc((const vint_element_base*)child, _nw, _se);
    }
}

// 0x004775D0 (:137)
void vint_element_group::get_screen_dims(vector2& nw, vector2& se) const
{
    nw.x = 1000000.0f;
    nw.y = 1000000.0f;
    se.x = -1000000.0f;
    se.y = -1000000.0f;
    if (first_child != nullptr && first_child->is_type(vint_element_base::Object_type))
    {
        derive_dims_from_children_rc((const vint_element_base*)first_child, nw, se);
    }
    if (se.x < nw.x || se.y < nw.y)
    {
        nw.zero();
        se.zero();
    }
}

// 0x0047DCD0 (:154)
void vint_element_group::get_bbox(bbox2d& box, bool adjust_min_and_max) const
{
    vint_object_base* child = first_child;
    while (child != nullptr && !child->is_type(vint_element_base::Object_type))
    {
        child = child->next_sibling;
    }
    if (child != nullptr)
    {
        ((const vint_element_base*)child)->get_bbox(box, true);
        child = child->next_sibling;
    }
    else
    {
        // no element children: the point at the anchor plus the rotated (unscaled) offset
        matrix23 child_xform;
        matrix23 offset_xform;
        get_global_anchor(box.max, child_xform, offset_xform);
        vector2 offset = offset_xform * m_offset;
        box.max.x = (float)((double)box.max.x + (double)offset.x);
        box.max.y = (float)((double)box.max.y + (double)offset.y);
        box.min = box.max;
    }
    for (; child != nullptr; child = child->next_sibling)
    {
        if (child->is_type(vint_element_base::Object_type))
        {
            bbox2d child_box;
            ((const vint_element_base*)child)->get_bbox(child_box, true);
            bbox2d_expand(box, child_box);
        }
    }
}

// 0x0045E360 (:192), 0x0045E380 (:199)
VINT_PROP_WRAPPERS(vint_element_group, screen_size)

// 0x0047DE50 (:193)
bool vint_element_group::get_screen_size(vint_variant& _val, int _index) const
{
    if (get_type() == vint_element_text::Object_type)
    {
        ((vint_element_text*)this)->reformat_text();
    }
    bbox2d box;
    get_bbox(box, true);
    _val = vector2((float)((double)box.max.x - (double)box.min.x), (float)((double)box.max.y - (double)box.min.y));
    return true;
}

// 0x0045E3A0 (:200)
bool vint_element_group::set_screen_size(const vint_variant& _val, int _index)
{
    return false;
}

// 0x0045E3B0 (:206), 0x0045E3D0 (:213)
VINT_PROP_WRAPPERS(vint_element_group, screen_se)

// 0x004687F0 (:207)
bool vint_element_group::get_screen_se(vint_variant& _val, int _index) const
{
    bbox2d box;
    get_bbox(box, true);
    _val = box.max;
    return true;
}

// 0x0045E3F0 (:214)
bool vint_element_group::set_screen_se(const vint_variant& _val, int _index)
{
    return false;
}

// 0x0045E400 (:221), 0x0045E420 (:227)
VINT_PROP_WRAPPERS(vint_element_group, screen_nw)

// 0x00468850 (:222)
bool vint_element_group::get_screen_nw(vint_variant& _val, int _index) const
{
    bbox2d box;
    get_bbox(box, true);
    _val = box.min;
    return true;
}

// 0x0045E440 (:228)
bool vint_element_group::set_screen_nw(const vint_variant& _val, int _index)
{
    return false;
}
