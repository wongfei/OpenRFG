// Original: volition/interface_lib/code/objects/noninteractive/vint_element_bitmap.cpp
//
// The bitmap element (UI.md UI2): draws a bitmap resource (by name, name crc or handle) or a raw bmpman handle, or a
// badge (through the draw_badge callback, scaled by the smaller axis of the object transform). The source rectangle
// is the whole image unless custom source coordinates are set; the element's size is the image size. A pool of 800.
// Compiled in interface_lib_unity.cpp.

#include "volition/interface_lib/code/objects/noninteractive/vint_element_bitmap.h"

#include "volition/interface_lib/code/internal/vint_prop_macros.h"
#include "volition/interface_lib/code/resources/vint_resource_bitmap.h"
#include "volition/vlib/code/bmpman/bmpman.h"
#include "volition/vlib/code/math/crc.h"
#include "volition/vlib/code/math/floating.h"
#include "volition/vlib/code/util/color.h"

#include <math.h>

// ---- class statics
unsigned int vint_element_bitmap::Int_obj_type = 0xFFFFFFFF; // 0x014F58B4
const unsigned int& vint_element_bitmap::Object_type = vint_element_bitmap::Int_obj_type; // 0x014F58B8

// ---- file-static globals
static vint_prop_lookup Prop_lookup_vint_element_bitmap[8] = { // 0x014F6218
    VINT_PROP_ENTRY(vint_element_bitmap, custom_source_coords, VINT_PROP_TYPE_BOOL, false, true),
    VINT_PROP_ENTRY(vint_element_bitmap, image, VINT_PROP_TYPE_BITMAP, 0u, true),
    VINT_PROP_ENTRY(vint_element_bitmap, image_crc, VINT_PROP_TYPE_UINT, 0, false),
    VINT_PROP_ENTRY(vint_element_bitmap, image_raw, VINT_PROP_TYPE_UINT, -1, false),
    VINT_PROP_ENTRY(vint_element_bitmap, image_badge, VINT_PROP_TYPE_UINT, -1, false),
    VINT_PROP_ENTRY(vint_element_bitmap, source_nw, VINT_PROP_TYPE_VECTOR2F, vector2(0.0f, 0.0f), true),
    VINT_PROP_ENTRY(vint_element_bitmap, source_se, VINT_PROP_TYPE_VECTOR2F, vector2(0.0f, 0.0f), true),
    VINT_PROP_ENTRY(vint_element_bitmap, image_size, VINT_PROP_TYPE_VECTOR2F, vector2(0.0f, 0.0f), false),
};
static vint_prop_mgr Prop_mgr_vint_element_bitmap; // 0x01A4EAD8
static vint_object_factory_imp<vint_element_bitmap> Factory_vint_element_bitmap;

// The render color: tint and alpha to 0..255 (rfg.exe inlines it per element).
static color vint_element_render_color(const vint_render_params& _render_params)
{
    color c;
    c.red = (unsigned char)(int)((double)_render_params.tint.r * 255.0);
    c.green = (unsigned char)(int)((double)_render_params.tint.g * 255.0);
    c.blue = (unsigned char)(int)((double)_render_params.tint.b * 255.0);
    c.alpha = (unsigned char)(int)((double)_render_params.alpha * 255.0);
    return c;
}

// 0x00477180 (.h:36)
vint_element_bitmap::vint_element_bitmap()
{
    m_image.set_type(VINT_PROP_TYPE_BITMAP);
}

// 0x00477210 (.h:37)
vint_element_bitmap::~vint_element_bitmap()
{
}

// 0x004771D0 (.h:46)
bool vint_element_bitmap::is_type(unsigned int _type) const
{
    return _type == Object_type || _type == vint_element_base::Object_type;
}

// 0x00477200 (.h:46)
unsigned int vint_element_bitmap::get_type() const
{
    return Object_type;
}

// 0x0047D8A0 (:76)
bool vint_element_bitmap::system_init()
{
    Int_obj_type = vint_register_object_class("bitmap", vint_element_base::Object_type, Prop_mgr_vint_element_bitmap, Prop_lookup_vint_element_bitmap, 8, Factory_vint_element_bitmap, 800);
    return true;
}

// 0x0045D750 (:82)
void vint_element_bitmap::system_shutdown()
{
}

// 0x00474AF0 (:87)
bool vint_element_bitmap::init()
{
    if (!vint_element_base::init())
    {
        return false;
    }
    // 0u: operator=(unsigned int) clears a bitmap-typed variant; the int overload (a plain 0) leaves it alone.
    m_image = 0u;
    m_raw_image = 0;
    m_raw_image_is_badge = false;
    m_scale.x = 1.0f;
    m_scale.y = 1.0f;
    m_source_nw.x = 0;
    m_source_nw.y = 0;
    m_source_se.x = 0;
    m_source_se.y = 0;
    m_custom_source_coords = false;
    return true;
}

// 0x00474B60 (:108)
void vint_element_bitmap::clean_up()
{
    m_image.clear();
    m_image.set_type(VINT_PROP_TYPE_BITMAP);
}

// 0x00471B90 (:114)
void vint_element_bitmap::process()
{
}

// 0x00471BA0 (:119)
void vint_element_bitmap::render_object(const vint_render_params& _render_params)
{
    if (m_image.get_type() == VINT_PROP_TYPE_BITMAP)
    {
        vint_resource_bitmap* bitmap = vint_resource_bitmap::find((unsigned int)m_image);
        if (bitmap != nullptr)
        {
            color c = vint_element_render_color(_render_params);
            if (Render_callbacks != nullptr && Render_callbacks->set_color != nullptr)
            {
                Render_callbacks->set_color(c);
            }
            bitmap->render(_render_params.object_xform, _render_params.object_pos, m_source_nw, m_source_se, _render_params.res_offset_x, _render_params.res_offset_y, _render_params.res_scale_x, _render_params.res_scale_y);
            return;
        }
    }
    if (m_raw_image != 0 && !m_raw_image_is_badge)
    {
        vint_render_set_color(_render_params.tint, _render_params.alpha);
        if (Render_callbacks != nullptr && Render_callbacks->draw_bitmap != nullptr)
        {
            Render_callbacks->draw_bitmap(m_raw_image, _render_params.object_xform, _render_params.object_pos, m_source_nw, m_source_se, _render_params.res_offset_x, _render_params.res_offset_y, _render_params.res_scale_x, _render_params.res_scale_y);
        }
    }
    else if (m_raw_image_is_badge)
    {
        float scale = _render_params.object_xform.elem[0][0];
        if (scale - _render_params.object_xform.elem[1][1] >= 0.0f)
        {
            scale = _render_params.object_xform.elem[1][1];
        }
        vint_render_badge(m_raw_image, _render_params.object_pos, _render_params.res_offset_x, _render_params.res_offset_y, _render_params.res_scale_x, _render_params.res_scale_y, _render_params.alpha, scale);
    }
}

// 0x0045D760 (:138), 0x0045D780 (:160)
VINT_PROP_WRAPPERS(vint_element_bitmap, image)

// 0x00474B90 (:139)
bool vint_element_bitmap::get_image(vint_variant& _val, int _index) const
{
    if (m_image.get_type() == VINT_PROP_TYPE_BITMAP && vint_resource_bitmap::find((unsigned int)m_image) != nullptr)
    {
        _val = m_image;
        return true;
    }
    return false;
}

// 0x00474BD0 (:161)
bool vint_element_bitmap::set_image(const vint_variant& _val, int _index)
{
    // the new bitmap with a temporary reference
    vint_resource_bitmap* bitmap = nullptr;
    switch (_val.get_type())
    {
    case VINT_PROP_TYPE_UINT:
        bitmap = vint_resource_bitmap::find((unsigned int)_val);
        if (bitmap != nullptr)
        {
            bitmap->add_ref();
        }
        // Original bug kept: an unknown handle goes on with no bitmap (get_dimensions reads through null).
        break;
    case VINT_PROP_TYPE_STRING:
        bitmap = vint_resource_bitmap::load((const char*)_val);
        if (bitmap == nullptr)
        {
            return false;
        }
        break;
    case VINT_PROP_TYPE_BITMAP:
        bitmap = vint_resource_bitmap::find((unsigned int)_val);
        if (bitmap == nullptr)
        {
            return false;
        }
        bitmap->add_ref();
        break;
    default:
        return false;
    }
    m_image = bitmap;
    vector2 scale;
    bitmap->get_dimensions(m_image_size, scale);
    if (!m_custom_source_coords)
    {
        m_source_se = m_image_size;
        m_source_nw.x = 0;
        m_source_nw.y = 0;
    }
    m_size.x = (float)m_image_size.x;
    m_size.y = (float)m_image_size.y;
    update_auto_offset();
    bitmap->release();
    m_raw_image = 0;
    return true;
}

// 0x0045D7A0 (:241), 0x0045D7D0 (:249)
VINT_PROP_WRAPPERS(vint_element_bitmap, image_crc)

// 0x0045D7C0 (:242)
bool vint_element_bitmap::get_image_crc(vint_variant& _val, int _index) const
{
    return false;
}

// 0x00474D20 (:250)
bool vint_element_bitmap::set_image_crc(const vint_variant& _val, int _index)
{
    unsigned int name_crc;
    if (_val.get_type() == VINT_PROP_TYPE_FLOAT)
    {
        // x87 fistp: the current (round to nearest) mode, low 32 bits
        name_crc = (unsigned int)(int64_t)llrintf((float)_val);
    }
    else
    {
        name_crc = (unsigned int)_val;
    }
    vint_resource_bitmap* bitmap = vint_resource_bitmap::load_by_name_crc(checksum_stri(name_crc));
    if (bitmap == nullptr)
    {
        return false;
    }
    m_image = bitmap;
    vector2 scale;
    bitmap->get_dimensions(m_image_size, scale);
    if (!m_custom_source_coords)
    {
        m_source_se = m_image_size;
        m_source_nw.x = 0;
        m_source_nw.y = 0;
    }
    m_size.x = (float)m_image_size.x;
    m_size.y = (float)m_image_size.y;
    update_auto_offset();
    m_raw_image = 0;
    bitmap->release();
    return true;
}

// 0x0045D7F0 (:284), 0x0045D820 (:291)
VINT_PROP_WRAPPERS(vint_element_bitmap, image_raw)

// 0x0045D810 (:285)
bool vint_element_bitmap::get_image_raw(vint_variant& _val, int _index) const
{
    return false;
}

// 0x00477240 (:292)
bool vint_element_bitmap::set_image_raw(const vint_variant& _val, int _index)
{
    m_raw_image = 0;
    m_image = 0u; // operator=(unsigned int), see init
    if (_val.get_type() != VINT_PROP_TYPE_INT && _val.get_type() != VINT_PROP_TYPE_UINT)
    {
        return false;
    }
    m_raw_image_is_badge = false;
    if ((unsigned int)_val == 0xFFFFFFFF)
    {
        // Original bug kept: the bogus bitmap goes to set_image as an int, which set_image rejects.
        vint_variant bogus(bm_get_bogus_bitmap_handle());
        return set_image(bogus, 0);
    }
    m_raw_image = (unsigned int)_val;
    m_image_size.x = 2;
    m_image_size.y = 2;
    if (Resource_callbacks != nullptr)
    {
        // Original: the callback itself is not null-checked.
        float scale_x;
        float scale_y;
        Resource_callbacks->bitmap_get_dimensions(m_image_size.x, m_image_size.y, scale_x, scale_y, m_raw_image);
        if (!m_custom_source_coords)
        {
            m_source_se = m_image_size;
            m_source_nw.x = 0;
            m_source_nw.y = 0;
        }
    }
    m_size.x = (float)m_image_size.x;
    m_size.y = (float)m_image_size.y;
    update_auto_offset();
    return true;
}

// 0x0045D840 (:341), 0x0045D870 (:348)
VINT_PROP_WRAPPERS(vint_element_bitmap, image_badge)

// 0x0045D860 (:342)
bool vint_element_bitmap::get_image_badge(vint_variant& _val, int _index) const
{
    return false;
}

// 0x00471D60 (:349)
bool vint_element_bitmap::set_image_badge(const vint_variant& _val, int _index)
{
    m_raw_image = 0;
    m_image = 0u; // operator=(unsigned int), see init: the document's image must not hide the badge in render_object
    m_raw_image_is_badge = false;
    if (_val.get_type() == VINT_PROP_TYPE_INT || _val.get_type() == VINT_PROP_TYPE_UINT)
    {
        m_raw_image_is_badge = true;
        m_raw_image = (unsigned int)_val;
        return true;
    }
    return false;
}

// 0x0045D890 (:376), 0x0045D8F0 (:384)
VINT_PROP_WRAPPERS(vint_element_bitmap, source_nw)

// 0x0045D8B0 (:377)
bool vint_element_bitmap::get_source_nw(vint_variant& _val, int _index) const
{
    _val.set_vector2f((float)m_source_nw.x, (float)m_source_nw.y);
    return true;
}

// 0x0045D910 (:385)
bool vint_element_bitmap::set_source_nw(const vint_variant& _val, int _index)
{
    vector2 v = (vector2)_val;
    m_source_nw.x = fl_round(v.x);
    m_source_nw.y = fl_round(v.y);
    return true;
}

// 0x0045D970 (:404), 0x0045D9D0 (:413)
VINT_PROP_WRAPPERS(vint_element_bitmap, source_se)

// 0x0045D990 (:405)
bool vint_element_bitmap::get_source_se(vint_variant& _val, int _index) const
{
    _val.set_vector2f((float)m_source_se.x, (float)m_source_se.y);
    return true;
}

// 0x0045D9F0 (:414)
bool vint_element_bitmap::set_source_se(const vint_variant& _val, int _index)
{
    vector2 v = (vector2)_val;
    m_source_se.x = fl_round(v.x);
    m_source_se.y = fl_round(v.y);
    // Original: reports failure although the value was set.
    return false;
}

// 0x0045DB10 (:442), 0x0045DA50 (:434)
VINT_PROP_WRAPPERS(vint_element_bitmap, custom_source_coords)

// 0x0045DA70 (:435)
bool vint_element_bitmap::set_custom_source_coords(const vint_variant& _val, int _index)
{
    m_custom_source_coords = (bool)_val;
    return true;
}

// 0x0045DB30 (:443)
bool vint_element_bitmap::get_custom_source_coords(vint_variant& _val, int _index) const
{
    _val = m_custom_source_coords;
    return true;
}

// 0x0045DB60 (:451), 0x0045DBC0 (:460)
VINT_PROP_WRAPPERS(vint_element_bitmap, image_size)

// 0x0045DB80 (:452)
bool vint_element_bitmap::get_image_size(vint_variant& _val, int _index) const
{
    _val.set_vector2f((float)m_image_size.x, (float)m_image_size.y);
    return true;
}

// 0x0045DBE0 (:461)
bool vint_element_bitmap::set_image_size(const vint_variant& _val, int _index)
{
    vector2 v = (vector2)_val;
    m_image_size.x = fl_round(v.x);
    m_image_size.y = fl_round(v.y);
    if (!m_custom_source_coords)
    {
        m_source_se = m_image_size;
        m_source_nw.x = 0;
        m_source_nw.y = 0;
    }
    m_size.x = (float)m_image_size.x;
    m_size.y = (float)m_image_size.y;
    update_auto_offset();
    // Original: reports failure although the value was set.
    return false;
}
