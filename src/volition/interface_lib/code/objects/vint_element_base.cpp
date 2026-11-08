// Original: volition/interface_lib/code/objects/vint_element_base.cpp
//
// The element base (UI.md UI2): the visible part of the object tree. Rendering walks the tree with vint_render_params:
// each element combines its render mode, mask flag, tint, alpha, anchor (moves the origin), rotation and scale (into
// the object transform), offset (the object position) and resolution modes into a copy of its parent's parameters,
// draws itself, then draws its element children sorted masks first, then by depth (higher first). A mask child
// starts stencil writing (after a clear); the next non-mask child switches to stencil testing. Bounds come from the
// four corners of the transformed size. The properties are reflected through Prop_lookup_vint_element_base; enum
// properties register their name tables. The transforms use matrix23 (vlib), in double as rfg.exe's SSE2 code.
// Compiled in interface_lib_unity.cpp.

#include "volition/interface_lib/code/objects/vint_element_base.h"

#include "volition/interface_lib/code/internal/vint_prop_macros.h"
#include "volition/interface_lib/code/objects/noninteractive/vint_element_text.h"
#include "volition/interface_lib/code/objects/vint_object_types.h"
#include "volition/vlib/code/math/bbox2d.h"

#include <stdlib.h>

// ---- file-private data (rfg.pdb)
static const vint_prop_type_enum_info Internal_vint_element_base_auto_offset_infos[10] = {{"none", 0}, {"c", 1}, {"w", 2}, {"n", 3}, {"e", 4}, {"s", 5}, {"nw", 6}, {"ne", 7}, {"se", 8}, {"sw", 9}}; // 0x014F5950
static const vint_prop_type_enum_info Internal_vint_element_base_render_mode_infos[7] = {{"default", 0}, {"filter", 1}, {"additive", 2}, {"additive_alpha", 3}, {"subtractive", 4}, {"subtractive_alpha", 5}, {"multiply", 6}}; // 0x014F59A0
static const vint_prop_type_enum_info Internal_vint_element_base_res_offset_x_infos[8] = {{"inherit", 0}, {"start", 1}, {"center", 2}, {"end", 3}, {"neg_start", 4}, {"neg_start_twice", 5}, {"neg_start_x", 6}, {"neg_start_y", 7}}; // 0x014F59D8
static const vint_prop_type_enum_info Internal_vint_element_base_res_offset_y_infos[8] = {{"inherit", 0}, {"start", 1}, {"center", 2}, {"end", 3}, {"neg_start", 4}, {"neg_start_twice", 5}, {"neg_start_x", 6}, {"neg_start_y", 7}}; // 0x014F5A18
static const vint_prop_type_enum_info Internal_vint_element_base_res_scale_x_infos[6] = {{"inherit", 0}, {"maintain_ratio", 1}, {"fill", 2}, {"cutoff", 3}, {"cutoff_x", 4}, {"cutoff_y", 5}}; // 0x014F5A58
static const vint_prop_type_enum_info Internal_vint_element_base_res_scale_y_infos[6] = {{"inherit", 0}, {"maintain_ratio", 1}, {"fill", 2}, {"cutoff", 3}, {"cutoff_x", 4}, {"cutoff_y", 5}}; // 0x014F5A88
static vint_prop_type_enum_wrapper Internal_vint_element_base_auto_offset_wrapper(Internal_vint_element_base_auto_offset_infos, 10); // 0x014F7340
static vint_prop_type_enum_wrapper Internal_vint_element_base_render_mode_wrapper(Internal_vint_element_base_render_mode_infos, 7); // 0x014F7348
static vint_prop_type_enum_wrapper Internal_vint_element_base_res_offset_x_wrapper(Internal_vint_element_base_res_offset_x_infos, 8); // 0x014F7350
static vint_prop_type_enum_wrapper Internal_vint_element_base_res_offset_y_wrapper(Internal_vint_element_base_res_offset_y_infos, 8); // 0x014F7358
static vint_prop_type_enum_wrapper Internal_vint_element_base_res_scale_x_wrapper(Internal_vint_element_base_res_scale_x_infos, 6); // 0x014F7360
static vint_prop_type_enum_wrapper Internal_vint_element_base_res_scale_y_wrapper(Internal_vint_element_base_res_scale_y_infos, 6); // 0x014F7368

// ---- class statics
unsigned int vint_element_base::Int_obj_type = 0xFFFFFFFF; // 0x014F5944
const unsigned int& vint_element_base::Object_type = vint_element_base::Int_obj_type; // 0x014F5948

// An enum property's setter: the value as an int, rejected above the enum's last value (rfg.exe inlines it per
// property; negative values are not checked).
template<typename E>
static bool vint_element_set_enum(E& _member, const vint_variant& _val, int _max)
{
    int value = (int)_val;
    if (value > _max)
    {
        return false;
    }
    _member = (E)value;
    return true;
}

// ---- header-inline members

// 0x004683C0 (.h:88)
vint_element_base::vint_element_base()
{
}

// 0x0045D600 (.h:90)
vint_element_base::~vint_element_base()
{
}

// 0x004683F0 (.h:104)
bool vint_element_base::is_type(unsigned int _type) const
{
    return _type == Object_type;
}

// 0x00468410 (.h:104)
unsigned int vint_element_base::get_type() const
{
    return Object_type;
}

// 0x00468420 (.h:131)
void vint_element_base::clean_up()
{
}

// 0x00468430 (.h:132)
void vint_element_base::pre_render()
{
}

// 0x00468440 (.h:134)
void vint_element_base::render_post_process()
{
}

// ---- vint_render_params

// 0x0045F1E0 (:150)
vint_render_params::vint_render_params()
{
    // the rest is set by the caller (the document's root parameters)
    final_scale.x = 1.0f;
    final_scale.y = 1.0f;
    origin_xform.make_identity();
    object_xform.make_identity();
    origin_pos.zero();
}

// 0x00477FC0 (:158)
void vint_render_params::combine(const vint_element_base& _element)
{
    if (_element.m_render_mode != VINT_RENDER_DEFAULT)
    {
        mode = _element.m_render_mode;
    }
    is_mask = is_mask || _element.m_is_mask;
    tint.r = (float)((double)_element.m_tint.r * (double)tint.r);
    tint.g = (float)((double)_element.m_tint.g * (double)tint.g);
    tint.b = (float)((double)_element.m_tint.b * (double)tint.b);
    alpha = (float)((double)_element.m_alpha * (double)alpha);

    // the anchor moves the origin in the parent's space; rotation and scale apply about it
    origin_xform = object_xform;
    matrix23 rotation;
    rotation.make_rotation(_element.m_rotation);
    vint_variant anchor_value;
    _element.get_anchor(anchor_value, 0);
    vector2 anchor = (vector2)anchor_value;
    vector2 anchor_moved = origin_xform * anchor;
    origin_pos.x = (float)((double)origin_pos.x + (double)anchor_moved.x);
    origin_pos.y = (float)((double)origin_pos.y + (double)anchor_moved.y);
    object_xform = origin_xform * rotation;
    matrix23 scale;
    scale.make_scale(_element.m_scale);
    object_xform = object_xform * scale;
    vector2 offset = object_xform * _element.m_offset;
    object_pos.x = (float)((double)origin_pos.x + (double)offset.x);
    object_pos.y = (float)((double)origin_pos.y + (double)offset.y);
    final_scale.x = (float)((double)final_scale.x * (double)_element.m_scale.x);
    final_scale.y = (float)((double)_element.m_scale.y * (double)final_scale.y);

    if (_element.m_res_offset_x != VINT_RES_OFFSET_INHERIT)
    {
        res_offset_x = _element.m_res_offset_x;
    }
    if (_element.m_res_offset_y != VINT_RES_OFFSET_INHERIT)
    {
        res_offset_y = _element.m_res_offset_y;
    }
    if (_element.m_res_scale_x != VINT_RES_SCALE_INHERIT)
    {
        res_scale_x = _element.m_res_scale_x;
    }
    if (_element.m_res_scale_y != VINT_RES_SCALE_INHERIT)
    {
        res_scale_y = _element.m_res_scale_y;
    }
}

// ---- property table
static vint_prop_lookup Prop_lookup_vint_element_base[18] = { // 0x014F6E30
    VINT_PROP_ENTRY(vint_element_base, render_mode, VINT_PROP_TYPE_ENUM, 0, true),
    VINT_PROP_ENTRY(vint_element_base, visible, VINT_PROP_TYPE_BOOL, true, true),
    VINT_PROP_ENTRY(vint_element_base, mask, VINT_PROP_TYPE_BOOL, false, true),
    VINT_PROP_ENTRY(vint_element_base, offset, VINT_PROP_TYPE_VECTOR2F, vector2(0.0f, 0.0f), true),
    VINT_PROP_ENTRY(vint_element_base, anchor, VINT_PROP_TYPE_VECTOR2F, vector2(0.0f, 0.0f), true),
    // the editor default is 255 per channel (the editor's scale)
    VINT_PROP_ENTRY(vint_element_base, tint, VINT_PROP_TYPE_COLOR, (vint_variant_color{255.0f, 255.0f, 255.0f}), true),
    VINT_PROP_ENTRY(vint_element_base, alpha, VINT_PROP_TYPE_FLOAT, 1.0f, true),
    VINT_PROP_ENTRY(vint_element_base, depth, VINT_PROP_TYPE_INT, 0, true),
    VINT_PROP_ENTRY(vint_element_base, screen_size, VINT_PROP_TYPE_VECTOR2F, vector2(0.0f, 0.0f), false),
    VINT_PROP_ENTRY(vint_element_base, screen_nw, VINT_PROP_TYPE_VECTOR2F, vector2(0.0f, 0.0f), false),
    VINT_PROP_ENTRY(vint_element_base, screen_se, VINT_PROP_TYPE_VECTOR2F, vector2(0.0f, 0.0f), false),
    VINT_PROP_ENTRY(vint_element_base, rotation, VINT_PROP_TYPE_FLOAT, 0.0f, true),
    VINT_PROP_ENTRY(vint_element_base, scale, VINT_PROP_TYPE_VECTOR2F, vector2(1.0f, 1.0f), true),
    VINT_PROP_ENTRY(vint_element_base, auto_offset, VINT_PROP_TYPE_ENUM, 0, true),
    VINT_PROP_ENTRY(vint_element_base, res_offset_x, VINT_PROP_TYPE_ENUM, 0, true),
    VINT_PROP_ENTRY(vint_element_base, res_offset_y, VINT_PROP_TYPE_ENUM, 0, true),
    VINT_PROP_ENTRY(vint_element_base, res_scale_x, VINT_PROP_TYPE_ENUM, 0, true),
    VINT_PROP_ENTRY(vint_element_base, res_scale_y, VINT_PROP_TYPE_ENUM, 0, true),
};
static vint_prop_mgr Prop_mgr_vint_element_base; // 0x01A4EB14

// ---- vint_element_base

// 0x00472590 (:211)
bool vint_element_base::system_init()
{
    Prop_mgr_vint_element_base.init(0xFFFFFFFF, "element", Prop_lookup_vint_element_base, 18);
    // the base element has no factory (abstract)
    Int_obj_type = vint_object_types_register_type("element", 0xFFFFFFFF, &Prop_mgr_vint_element_base);
    Internal_vint_element_base_auto_offset_wrapper.init(Object_type, "auto_offset");
    Internal_vint_element_base_render_mode_wrapper.init(Object_type, "render_mode");
    Internal_vint_element_base_res_offset_x_wrapper.init(Object_type, "res_offset_x");
    Internal_vint_element_base_res_offset_y_wrapper.init(Object_type, "res_offset_y");
    Internal_vint_element_base_res_scale_x_wrapper.init(Object_type, "res_scale_x");
    Internal_vint_element_base_res_scale_y_wrapper.init(Object_type, "res_scale_y");
    return true;
}

// 0x004726E0 (:223)
bool vint_element_base::init()
{
    vint_object_base::init();
    m_tint.set(1.0f, 1.0f, 1.0f);
    m_anchor.set(0.0f, 0.0f);
    m_offset.set(0.0f, 0.0f);
    m_size.set(1.0f, 1.0f);
    m_scale.set(1.0f, 1.0f);
    m_rotation = 0.0f;
    m_alpha = 1.0f;
    m_auto_offset = VINT_COMPASS_NONE;
    m_render_mode = VINT_RENDER_DEFAULT;
    m_is_visible = true;
    m_is_mask = false;
    m_render_depth = 0;
    m_res_offset_x = VINT_RES_OFFSET_INHERIT;
    m_res_offset_y = VINT_RES_OFFSET_INHERIT;
    m_res_scale_x = VINT_RES_SCALE_INHERIT;
    m_res_scale_y = VINT_RES_SCALE_INHERIT;
    return true;
}

// 0x004690A0 (:253)
void vint_element_base::process()
{
}

// 0x0045F240 (:258)
// LTCG: no register arguments (qsort callback).
static int compare_children_for_render(const void* _left, const void* _right)
{
    const vint_element_base* left = *(const vint_element_base* const*)_left;
    const vint_element_base* right = *(const vint_element_base* const*)_right;
    // masks first, then the higher depth
    if (left->m_is_mask != right->m_is_mask)
    {
        return left->m_is_mask ? -1 : 1;
    }
    if (left->m_render_depth > right->m_render_depth)
    {
        return -1;
    }
    return left->m_render_depth < right->m_render_depth ? 1 : 0;
}

// 0x00478520 (:283)
void vint_element_base::render(const vint_render_params& _render_params, vint_object_base** _render_stack, unsigned int _stack_size)
{
    if (!m_is_visible)
    {
        return;
    }
    pre_render();
    vint_render_params params = _render_params;
    params.combine(*this);
    if (Render_callbacks != nullptr && Render_callbacks->set_render_mode != nullptr)
    {
        Render_callbacks->set_render_mode(params.mode);
    }
    render_object(params);
    if (first_child != nullptr)
    {
        // the element children go on the render stack (as many as fit), sorted
        unsigned int num_children = 0;
        for (vint_object_base* child = first_child; child != nullptr && num_children < _stack_size; child = child->next_sibling)
        {
            if (child->is_type(Object_type))
            {
                _render_stack[num_children++] = child;
            }
        }
        if (num_children > 1)
        {
            qsort(_render_stack, num_children, sizeof(vint_object_base*), compare_children_for_render);
        }
        if (!params.is_mask)
        {
            bool used_stencil = false;
            for (unsigned int i = 0; i < num_children; ++i)
            {
                vint_element_base* child = (vint_element_base*)_render_stack[i];
                if (Current_stencil_mode == VINT_STENCIL_DISABLED)
                {
                    if (child->m_is_mask)
                    {
                        if (Render_callbacks != nullptr && Render_callbacks->stencil_clear != nullptr)
                        {
                            Render_callbacks->stencil_clear();
                        }
                        vint_render_set_stencil_mode(VINT_STENCIL_WRITE);
                        used_stencil = true;
                    }
                }
                else if (Current_stencil_mode == VINT_STENCIL_WRITE && !child->m_is_mask)
                {
                    vint_render_set_stencil_mode(VINT_STENCIL_TEST);
                }
                child->render(params, _render_stack + num_children, _stack_size - num_children);
            }
            if (used_stencil && Render_callbacks != nullptr && Render_callbacks->set_stencil_mode != nullptr && Current_stencil_mode != VINT_STENCIL_DISABLED)
            {
                Render_callbacks->set_stencil_mode(VINT_STENCIL_DISABLED);
                Current_stencil_mode = VINT_STENCIL_DISABLED;
            }
        }
        else
        {
            // inside a mask: no stencil changes
            for (unsigned int i = 0; i < num_children; ++i)
            {
                ((vint_element_base*)_render_stack[i])->render(params, _render_stack + num_children, _stack_size - num_children);
            }
        }
    }
    render_post_process();
}

// 0x00478730 (:371)
vint_element_base* vint_element_base::pick_recursive(const vector2& pos, float tolerance)
{
    // the deepest hit among the element children (lowest depth wins among hits), else this element's bounds
    vint_element_base* best = nullptr;
    int best_depth = 0x7FFFFFFF;
    for (vint_object_base* obj = first_child; obj != nullptr; obj = obj->next_sibling)
    {
        if (!obj->is_type(Object_type))
        {
            continue;
        }
        vint_element_base* child = (vint_element_base*)obj;
        vint_variant depth;
        child->get_depth(depth, 0);
        if (best != nullptr && (int)depth > best_depth)
        {
            continue;
        }
        vint_element_base* hit = child->pick_recursive(pos, tolerance);
        if (hit != nullptr)
        {
            best = hit;
            best_depth = (int)depth;
        }
    }
    if (best != nullptr)
    {
        return best;
    }
    return is_point_inside(pos, tolerance) ? this : nullptr;
}

// 0x0045F290 (:401)
bool vint_element_base::is_point_inside(const vector2& pos, float tolerance) const
{
    bbox2d box;
    get_bbox(box, true);
    return (double)pos.x >= (double)(float)((double)box.min.x - (double)tolerance)
        && (double)pos.y >= (double)(float)((double)box.min.y - (double)tolerance)
        && (double)(float)((double)box.max.x + (double)tolerance) >= (double)pos.x
        && (double)(float)((double)box.max.y + (double)tolerance) >= (double)pos.y;
}

// 0x0047E080 (:415)
void vint_element_base::get_corners(vector2& _nw, vector2& _ne, vector2& _sw, vector2& _se) const
{
    vector2 anchor;
    matrix23 child_xform;
    matrix23 offset_xform;
    get_global_anchor(anchor, child_xform, offset_xform);
    vint_variant offset_value;
    get_offset(offset_value, 0);
    vector2 offset = child_xform * (vector2)offset_value;
    vector2 nw;
    nw.x = (float)((double)anchor.x + (double)offset.x);
    nw.y = (float)((double)anchor.y + (double)offset.y);
    vector2 width = child_xform * vector2(m_size.x, 0.0f);
    vector2 height = child_xform * vector2(0.0f, m_size.y);
    _nw = nw;
    _ne.x = (float)((double)nw.x + (double)width.x);
    _ne.y = (float)((double)nw.y + (double)width.y);
    // Original: _sw gets the corner across from nw and _se the one below nw (the names are swapped).
    _sw.x = (float)((double)_ne.x + (double)height.x);
    _sw.y = (float)((double)_ne.y + (double)height.y);
    _se.x = (float)((double)height.x + (double)nw.x);
    _se.y = (float)((double)height.y + (double)nw.y);
}

// 0x0045F380 (:438)
void vint_element_base::get_bbox(bbox2d& box, bool) const
{
    vector2 nw;
    vector2 ne;
    vector2 sw;
    vector2 se;
    get_corners(nw, ne, sw, se);
    bbox2d_create_from_points(box, nw, ne);
    bbox2d_expand(box, sw);
    bbox2d_expand(box, se);
    for (vint_object_base* child = first_child; child != nullptr; child = child->next_sibling)
    {
        if (child->is_type(Object_type))
        {
            bbox2d child_box;
            ((vint_element_base*)child)->get_bbox(child_box, true);
            bbox2d_expand(box, child_box);
        }
    }
}

// 0x00478950 (:466)
void vint_element_base::get_global_anchor(vector2& _anchor, matrix23& _child_xform, matrix23& _offset_xform) const
{
    // the parent elements' transforms down to this element (as combine builds them while rendering)
    if (parent_object != nullptr && parent_object->is_type(Object_type))
    {
        ((const vint_element_base*)parent_object)->get_global_anchor(_anchor, _child_xform, _offset_xform);
    }
    else
    {
        _anchor.zero();
        _child_xform.make_identity();
        _offset_xform.make_identity();
    }
    vint_variant anchor_value;
    get_anchor(anchor_value, 0);
    vector2 anchor_moved = _child_xform * (vector2)anchor_value;
    _anchor.x = (float)((double)_anchor.x + (double)anchor_moved.x);
    _anchor.y = (float)((double)_anchor.y + (double)anchor_moved.y);
    matrix23 rotation;
    rotation.make_rotation(m_rotation);
    _offset_xform = _child_xform * rotation;
    matrix23 scale;
    scale.make_scale(m_scale);
    _child_xform = _offset_xform * scale;
}

// 0x00478D40 (:492)
void vint_element_base::get_global_anchor(vector2& _anchor) const
{
    matrix23 child_xform;
    matrix23 offset_xform;
    get_global_anchor(_anchor, child_xform, offset_xform);
}

// 0x0045F430 (:498)
void vint_element_base::get_accum_scale(vector2& _scale) const
{
    // Original: the parent is assumed to be an element.
    if (parent_object == nullptr)
    {
        _scale.x = 1.0f;
        _scale.y = 1.0f;
    }
    else
    {
        ((const vint_element_base*)parent_object)->get_accum_scale(_scale);
    }
    _scale.x = (float)((double)m_scale.x * (double)_scale.x);
    _scale.y = (float)((double)m_scale.y * (double)_scale.y);
}

// 0x0045F4A0 (:516)
void vint_element_base::update_auto_offset()
{
    // the offset that puts the anchor at the compass point of the size
    switch (m_auto_offset)
    {
    case VINT_COMPASS_CENTER:
    case VINT_COMPASS_N:
    case VINT_COMPASS_S:
        m_offset.x = (float)-((double)m_size.x * 0.5);
        break;
    case VINT_COMPASS_W:
    case VINT_COMPASS_NW:
    case VINT_COMPASS_SW:
        m_offset.x = 0.0f;
        break;
    case VINT_COMPASS_E:
    case VINT_COMPASS_NE:
    case VINT_COMPASS_SE:
        m_offset.x = -m_size.x;
        break;
    default:
        return;
    }
    switch (m_auto_offset)
    {
    case VINT_COMPASS_CENTER:
    case VINT_COMPASS_W:
    case VINT_COMPASS_E:
        m_offset.y = (float)-((double)m_size.y * 0.5);
        break;
    case VINT_COMPASS_N:
    case VINT_COMPASS_NW:
    case VINT_COMPASS_NE:
        m_offset.y = 0.0f;
        break;
    case VINT_COMPASS_S:
    case VINT_COMPASS_SE:
    case VINT_COMPASS_SW:
        m_offset.y = -m_size.y;
        break;
    default:
        break;
    }
}

// 0x0045F590 (:564), 0x0045F5B0 (:571)
VINT_PROP_WRAPPERS(vint_element_base, render_mode)

// 0x004690B0 (:565)
bool vint_element_base::get_render_mode(vint_variant& _val, int _index) const
{
    _val = (int)m_render_mode;
    return true;
}

// 0x0045F5D0 (:572)
bool vint_element_base::set_render_mode(const vint_variant& _val, int _index)
{
    return vint_element_set_enum(m_render_mode, _val, NUM_VINT_RENDER_MODES - 1);
}

// 0x0045F6A0 (:582), 0x0045F6C0 (:589)
VINT_PROP_WRAPPERS(vint_element_base, res_offset_x)

// 0x004690D0 (:583)
bool vint_element_base::get_res_offset_x(vint_variant& _val, int _index) const
{
    _val = (int)m_res_offset_x;
    return true;
}

// 0x0045F6E0 (:590)
bool vint_element_base::set_res_offset_x(const vint_variant& _val, int _index)
{
    return vint_element_set_enum(m_res_offset_x, _val, NUM_VINT_RES_OFFSET - 1);
}

// 0x0045F7B0 (:600), 0x0045F7D0 (:607)
VINT_PROP_WRAPPERS(vint_element_base, res_offset_y)

// 0x004690F0 (:601)
bool vint_element_base::get_res_offset_y(vint_variant& _val, int _index) const
{
    _val = (int)m_res_offset_y;
    return true;
}

// 0x0045F7F0 (:608)
bool vint_element_base::set_res_offset_y(const vint_variant& _val, int _index)
{
    return vint_element_set_enum(m_res_offset_y, _val, NUM_VINT_RES_OFFSET - 1);
}

// 0x0045F8C0 (:618), 0x0045F8E0 (:625)
VINT_PROP_WRAPPERS(vint_element_base, res_scale_x)

// 0x00469110 (:619)
bool vint_element_base::get_res_scale_x(vint_variant& _val, int _index) const
{
    _val = (int)m_res_scale_x;
    return true;
}

// 0x0045F900 (:626)
bool vint_element_base::set_res_scale_x(const vint_variant& _val, int _index)
{
    return vint_element_set_enum(m_res_scale_x, _val, NUM_VINT_RES_SCALE - 1);
}

// 0x0045F9D0 (:636), 0x0045F9F0 (:643)
VINT_PROP_WRAPPERS(vint_element_base, res_scale_y)

// 0x00469130 (:637)
bool vint_element_base::get_res_scale_y(vint_variant& _val, int _index) const
{
    _val = (int)m_res_scale_y;
    return true;
}

// 0x0045FA10 (:644)
bool vint_element_base::set_res_scale_y(const vint_variant& _val, int _index)
{
    return vint_element_set_enum(m_res_scale_y, _val, NUM_VINT_RES_SCALE - 1);
}

// 0x0045FAE0 (:655)
void vint_element_base::get_res_offsets_and_scales_inherited(vint_res_offset& _res_offset_x, vint_res_offset& _res_offset_y, vint_res_scale& _res_scale_x, vint_res_scale& _res_scale_y)
{
    // Original: only the x offset decides how far up the elements to go; all four come from that element.
    const vint_element_base* element = this;
    while (element->m_res_offset_x == VINT_RES_OFFSET_INHERIT && element->parent_object != nullptr && element->parent_object->is_type(Object_type))
    {
        element = (const vint_element_base*)element->parent_object;
    }
    _res_offset_x = element->m_res_offset_x;
    _res_offset_y = element->m_res_offset_y;
    _res_scale_x = element->m_res_scale_x;
    _res_scale_y = element->m_res_scale_y;
}

// 0x0045FB50 (:669), 0x0045FBA0 (:676)
VINT_PROP_WRAPPERS(vint_element_base, visible)

// 0x0045FB70 (:670)
bool vint_element_base::get_visible(vint_variant& _val, int _index) const
{
    _val = m_is_visible;
    return true;
}

// 0x0045FBC0 (:677)
bool vint_element_base::set_visible(const vint_variant& _val, int _index)
{
    m_is_visible = (bool)_val;
    return true;
}

// 0x0045FC60 (:683), 0x0045FCB0 (:690)
VINT_PROP_WRAPPERS(vint_element_base, mask)

// 0x0045FC80 (:684)
bool vint_element_base::get_mask(vint_variant& _val, int _index) const
{
    _val = m_is_mask;
    return true;
}

// 0x0045FCD0 (:691)
bool vint_element_base::set_mask(const vint_variant& _val, int _index)
{
    m_is_mask = (bool)_val;
    return true;
}

// 0x0045FD70 (:697), 0x0045FDD0 (:704)
VINT_PROP_WRAPPERS(vint_element_base, anchor)

// 0x0045FD90 (:698)
bool vint_element_base::get_anchor(vint_variant& _val, int _index) const
{
    _val = m_anchor;
    return true;
}

// 0x0045FDF0 (:705)
bool vint_element_base::set_anchor(const vint_variant& _val, int _index)
{
    m_anchor = (vector2)_val;
    return true;
}

// 0x0045FE40 (:711), 0x0045FEA0 (:719)
VINT_PROP_WRAPPERS(vint_element_base, offset)

// 0x0045FE60 (:712)
bool vint_element_base::get_offset(vint_variant& _val, int _index) const
{
    _val = m_offset;
    return true;
}

// 0x0045FEC0 (:720)
bool vint_element_base::set_offset(const vint_variant& _val, int _index)
{
    m_offset = (vector2)_val;
    return true;
}

// 0x0045FF10 (:726), 0x0045FF60 (:733)
VINT_PROP_WRAPPERS(vint_element_base, tint)

// 0x0045FF30 (:727)
bool vint_element_base::get_tint(vint_variant& _val, int _index) const
{
    _val = m_tint;
    return true;
}

// 0x0045FF80 (:734)
bool vint_element_base::set_tint(const vint_variant& _val, int _index)
{
    m_tint = (vint_variant_color)_val;
    return true;
}

// 0x0045FFD0 (:740), 0x00460030 (:747)
VINT_PROP_WRAPPERS(vint_element_base, alpha)

// 0x0045FFF0 (:741)
bool vint_element_base::get_alpha(vint_variant& _val, int _index) const
{
    _val = m_alpha;
    return true;
}

// 0x00460050 (:748)
bool vint_element_base::set_alpha(const vint_variant& _val, int _index)
{
    m_alpha = (float)_val;
    return true;
}

// 0x00460070 (:754), 0x00460090 (:761)
VINT_PROP_WRAPPERS(vint_element_base, depth)

// 0x00469150 (:755)
bool vint_element_base::get_depth(vint_variant& _val, int _index) const
{
    _val = (int)m_render_depth;
    return true;
}

// 0x004600B0 (:762)
bool vint_element_base::set_depth(const vint_variant& _val, int _index)
{
    m_render_depth = (signed char)(int)_val;
    return true;
}

// 0x00460130 (:769), 0x004601B0 (:779)
VINT_PROP_WRAPPERS(vint_element_base, screen_nw)

// 0x00460150 (:770)
bool vint_element_base::get_screen_nw(vint_variant& _val, int _index) const
{
    bbox2d box;
    get_bbox(box, true);
    _val = box.min;
    return true;
}

// 0x004601D0 (:780)
bool vint_element_base::set_screen_nw(const vint_variant& _val, int _index)
{
    return false;
}

// 0x004601E0 (:787), 0x00460260 (:797)
VINT_PROP_WRAPPERS(vint_element_base, screen_se)

// 0x00460200 (:788)
bool vint_element_base::get_screen_se(vint_variant& _val, int _index) const
{
    bbox2d box;
    get_bbox(box, true);
    _val = box.max;
    return true;
}

// 0x00460280 (:798)
bool vint_element_base::set_screen_se(const vint_variant& _val, int _index)
{
    return false;
}

// 0x00460290 (:805), 0x004602B0 (:826)
VINT_PROP_WRAPPERS(vint_element_base, screen_size)

// 0x00478D80 (:806)
bool vint_element_base::get_screen_size(vint_variant& _val, int _index) const
{
    // a text element lays its text out first (its size depends on it)
    if (get_type() == vint_element_text::Object_type)
    {
        ((vint_element_text*)this)->reformat_text();
    }
    bbox2d box;
    get_bbox(box, true);
    _val = vector2((float)((double)box.max.x - (double)box.min.x), (float)((double)box.max.y - (double)box.min.y));
    return true;
}

// 0x00478E30 (:827)
bool vint_element_base::set_screen_size(const vint_variant& _val, int _index)
{
    // scales the element so that its bounds get the given size (at least 0.0001 each way)
    vector2 target = (vector2)_val;
    const double min_size = (double)0.0001f;
    if ((float)((double)target.x - min_size) < 0.0f)
    {
        target.x = 0.0001f;
    }
    if ((float)((double)target.y - min_size) < 0.0f)
    {
        target.y = 0.0001f;
    }
    bbox2d box;
    get_bbox(box, true);
    vector2 current((float)((double)box.max.x - (double)box.min.x), (float)((double)box.max.y - (double)box.min.y));
    vector2 accum;
    if (parent_object != nullptr)
    {
        ((const vint_element_base*)parent_object)->get_accum_scale(accum);
    }
    else
    {
        accum.x = 1.0f;
        accum.y = 1.0f;
    }
    vector2 total_scale((float)((double)m_scale.x * (double)accum.x), (float)((double)m_scale.y * (double)accum.y));
    vint_variant scale_value;
    get_scale(scale_value, 0);
    vector2 scale = (vector2)scale_value;
    float new_x = (float)((double)(float)((double)target.x / (double)current.x * (double)total_scale.x) / ((double)total_scale.x / (double)scale.x));
    float new_y = (float)((double)(float)((double)target.y / (double)current.y * (double)total_scale.y) / ((double)total_scale.y / (double)scale.y));
    vint_variant new_scale(vector2(new_x, new_y));
    set_scale(new_scale, 0);
    return true;
}

// 0x004602D0 (:862), 0x00460330 (:870)
VINT_PROP_WRAPPERS(vint_element_base, rotation)

// 0x004602F0 (:863)
bool vint_element_base::get_rotation(vint_variant& _val, int _index) const
{
    _val = m_rotation;
    return true;
}

// 0x00460350 (:871)
bool vint_element_base::set_rotation(const vint_variant& _val, int _index)
{
    m_rotation = (float)_val;
    return true;
}

// 0x00460370 (:878), 0x004603D0 (:886)
VINT_PROP_WRAPPERS(vint_element_base, scale)

// 0x00460390 (:879)
bool vint_element_base::get_scale(vint_variant& _val, int _index) const
{
    _val = m_scale;
    return true;
}

// 0x004603F0 (:887)
bool vint_element_base::set_scale(const vint_variant& _val, int _index)
{
    m_scale = (vector2)_val;
    return true;
}

// 0x00460440 (:894), 0x00460460 (:902)
VINT_PROP_WRAPPERS(vint_element_base, auto_offset)

// 0x00469170 (:895)
bool vint_element_base::get_auto_offset(vint_variant& _val, int _index) const
{
    _val = (int)m_auto_offset;
    return true;
}

// 0x00460480 (:903)
bool vint_element_base::set_auto_offset(const vint_variant& _val, int _index)
{
    // not range checked
    m_auto_offset = (vint_compass)(int)_val;
    update_auto_offset();
    return true;
}
