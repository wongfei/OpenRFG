// Original: volition/interface_lib/code/objects/noninteractive/vint_element_bitmap_circle.cpp
//
// The bitmap circle element (UI.md UI2): a bitmap drawn as a pie between two angles in a number of wedges through the
// draw_bitmap_circle callback (meters and timers). Setting the image takes its scale into the element's scale. A pool
// of one. Compiled in interface_lib_unity.cpp.

#include "volition/interface_lib/code/objects/noninteractive/vint_element_bitmap_circle.h"

#include "volition/interface_lib/code/internal/vint_prop_macros.h"
#include "volition/interface_lib/code/resources/vint_resource_bitmap.h"
#include "volition/vlib/code/math/floating.h"
#include "volition/vlib/code/util/color.h"

// ---- class statics
unsigned int vint_element_bitmap_circle::Int_obj_type = 0xFFFFFFFF; // 0x014F58BC
const unsigned int& vint_element_bitmap_circle::Object_type = vint_element_bitmap_circle::Int_obj_type; // 0x014F58C0

// ---- file-static globals
static vint_prop_lookup Prop_lookup_vint_element_bitmap_circle[7] = { // 0x014F6470
    VINT_PROP_ENTRY(vint_element_bitmap_circle, image, VINT_PROP_TYPE_BITMAP, 0, true),
    // the base element's screen size
    VINT_PROP_ENTRY(vint_element_base, screen_size, VINT_PROP_TYPE_VECTOR2F, vector2(0.0f, 0.0f), false),
    VINT_PROP_ENTRY(vint_element_bitmap_circle, source_nw, VINT_PROP_TYPE_VECTOR2F, vector2(0.0f, 0.0f), true),
    VINT_PROP_ENTRY(vint_element_bitmap_circle, source_se, VINT_PROP_TYPE_VECTOR2F, vector2(0.0f, 0.0f), true),
    VINT_PROP_ENTRY(vint_element_bitmap_circle, start_angle, VINT_PROP_TYPE_FLOAT, 0.0f, true),
    VINT_PROP_ENTRY(vint_element_bitmap_circle, end_angle, VINT_PROP_TYPE_FLOAT, 6.2831855f, true),
    VINT_PROP_ENTRY(vint_element_bitmap_circle, num_wedges, VINT_PROP_TYPE_UINT, 16, true),
};
static vint_prop_mgr Prop_mgr_vint_element_bitmap_circle; // 0x01A77374
static vint_object_factory_imp<vint_element_bitmap_circle> Factory_vint_element_bitmap_circle;

// 0x00477370 (.h:35)
vint_element_bitmap_circle::vint_element_bitmap_circle()
{
    m_image.set_type(VINT_PROP_TYPE_BITMAP);
}

// 0x00477400 (.h:36)
vint_element_bitmap_circle::~vint_element_bitmap_circle()
{
}

// 0x004773C0 (.h:45)
bool vint_element_bitmap_circle::is_type(unsigned int _type) const
{
    return _type == Object_type || _type == vint_element_base::Object_type;
}

// 0x004773F0 (.h:45)
unsigned int vint_element_bitmap_circle::get_type() const
{
    return Object_type;
}

// 0x0047D940 (:74)
bool vint_element_bitmap_circle::system_init()
{
    Int_obj_type = vint_register_object_class("bitmap_circle", vint_element_base::Object_type, Prop_mgr_vint_element_bitmap_circle, Prop_lookup_vint_element_bitmap_circle, 7, Factory_vint_element_bitmap_circle, 1);
    return true;
}

// 0x0045DCA0 (:80)
void vint_element_bitmap_circle::system_shutdown()
{
}

// 0x00474E80 (:85)
bool vint_element_bitmap_circle::init()
{
    if (!vint_element_base::init())
    {
        return false;
    }
    // 0u: operator=(unsigned int) clears a bitmap-typed variant; the int overload (a plain 0) leaves it alone.
    m_image = 0u;
    m_scale.x = 1.0f;
    m_scale.y = 1.0f;
    m_source_nw.x = 0;
    m_source_nw.y = 0;
    m_source_se.x = 0;
    m_source_se.y = 0;
    m_start_angle = 0.0f;
    m_end_angle = 6.2831855f;
    m_num_wedges = 16;
    return true;
}

// 0x00474F00 (:102)
void vint_element_bitmap_circle::clean_up()
{
    m_image.clear();
    m_image.set_type(VINT_PROP_TYPE_BITMAP);
}

// 0x00471DB0 (:108)
void vint_element_bitmap_circle::process()
{
}

// 0x00471DC0 (:113)
void vint_element_bitmap_circle::render_object(const vint_render_params& _render_params)
{
    if (m_image.get_type() != VINT_PROP_TYPE_BITMAP)
    {
        return;
    }
    vint_resource_bitmap* bitmap = vint_resource_bitmap::find((unsigned int)m_image);
    if (bitmap == nullptr)
    {
        return;
    }
    color c;
    c.red = (unsigned char)(int)((double)_render_params.tint.r * 255.0);
    c.green = (unsigned char)(int)((double)_render_params.tint.g * 255.0);
    c.blue = (unsigned char)(int)((double)_render_params.tint.b * 255.0);
    c.alpha = (unsigned char)(int)((double)_render_params.alpha * 255.0);
    if (Render_callbacks != nullptr && Render_callbacks->set_color != nullptr)
    {
        Render_callbacks->set_color(c);
    }
    bitmap->render_circle(_render_params.object_xform, _render_params.object_pos, m_source_nw, m_source_se, m_start_angle, m_end_angle, m_num_wedges, _render_params.res_offset_x, _render_params.res_offset_y, _render_params.res_scale_x, _render_params.res_scale_y);
}

// 0x0045DCB0 (:123), 0x0045DCD0 (:136)
VINT_PROP_WRAPPERS(vint_element_bitmap_circle, image)

// 0x00474F30 (:124)
bool vint_element_bitmap_circle::get_image(vint_variant& _val, int _index) const
{
    if (m_image.get_type() == VINT_PROP_TYPE_BITMAP && vint_resource_bitmap::find((unsigned int)m_image) != nullptr)
    {
        _val = m_image;
        return true;
    }
    return false;
}

// 0x00474F70 (:137)
bool vint_element_bitmap_circle::set_image(const vint_variant& _val, int _index)
{
    vint_resource_bitmap* bitmap;
    switch (_val.get_type())
    {
    case VINT_PROP_TYPE_UINT:
    case VINT_PROP_TYPE_BITMAP:
        bitmap = vint_resource_bitmap::find((unsigned int)_val);
        if (bitmap == nullptr)
        {
            return false;
        }
        bitmap->add_ref();
        break;
    case VINT_PROP_TYPE_STRING:
        bitmap = vint_resource_bitmap::load((const char*)_val);
        if (bitmap == nullptr)
        {
            return false;
        }
        break;
    default:
        return false;
    }
    m_image = bitmap;
    // the image's scale becomes the element's scale; the source is always the whole image
    bitmap->get_dimensions(m_image_size, m_scale);
    m_source_se = m_image_size;
    m_source_nw.x = 0;
    m_source_nw.y = 0;
    m_size.x = (float)m_image_size.x;
    m_size.y = (float)m_image_size.y;
    bitmap->release();
    update_auto_offset();
    return true;
}

// 0x0045DCF0 (:188), 0x0045DD50 (:196)
VINT_PROP_WRAPPERS(vint_element_bitmap_circle, source_nw)

// 0x0045DD10 (:189)
bool vint_element_bitmap_circle::get_source_nw(vint_variant& _val, int _index) const
{
    _val.set_vector2f((float)m_source_nw.x, (float)m_source_nw.y);
    return true;
}

// 0x00468520 (:197)
bool vint_element_bitmap_circle::set_source_nw(const vint_variant& _val, int _index)
{
    vector2 v = (vector2)_val;
    m_source_nw.x = fl_round(v.x);
    m_source_nw.y = fl_round(v.y);
    if (m_image.get_type() == VINT_PROP_TYPE_BITMAP && vint_resource_bitmap::find((unsigned int)m_image) != nullptr)
    {
        // clamped to the image
        if (m_image_size.y <= m_source_nw.y)
        {
            m_source_nw.y = m_image_size.y;
        }
        if (m_source_nw.x >= m_image_size.x)
        {
            m_source_nw.x = m_image_size.x;
        }
    }
    return true;
}

// 0x0045DD70 (:214), 0x0045DDD0 (:222)
VINT_PROP_WRAPPERS(vint_element_bitmap_circle, source_se)

// 0x0045DD90 (:215)
bool vint_element_bitmap_circle::get_source_se(vint_variant& _val, int _index) const
{
    _val.set_vector2f((float)m_source_se.x, (float)m_source_se.y);
    return true;
}

// 0x004685D0 (:223)
bool vint_element_bitmap_circle::set_source_se(const vint_variant& _val, int _index)
{
    vector2 v = (vector2)_val;
    m_source_se.x = fl_round(v.x);
    m_source_se.y = fl_round(v.y);
    if (m_image.get_type() == VINT_PROP_TYPE_BITMAP && vint_resource_bitmap::find((unsigned int)m_image) != nullptr)
    {
        if (m_image_size.y <= m_source_se.y)
        {
            m_source_se.y = m_image_size.y;
        }
        if (m_source_se.x >= m_image_size.x)
        {
            m_source_se.x = m_image_size.x;
        }
    }
    // Original: reports failure although the value was set.
    return false;
}

// 0x0045DDF0 (:240), 0x0045DE50 (:248)
VINT_PROP_WRAPPERS(vint_element_bitmap_circle, start_angle)

// 0x0045DE10 (:241)
bool vint_element_bitmap_circle::get_start_angle(vint_variant& _val, int _index) const
{
    _val = m_start_angle;
    return true;
}

// 0x0045DE70 (:249)
bool vint_element_bitmap_circle::set_start_angle(const vint_variant& _val, int _index)
{
    m_start_angle = (float)_val;
    return true;
}

// 0x0045DE90 (:256), 0x0045DEF0 (:264)
VINT_PROP_WRAPPERS(vint_element_bitmap_circle, end_angle)

// 0x0045DEB0 (:257)
bool vint_element_bitmap_circle::get_end_angle(vint_variant& _val, int _index) const
{
    _val = m_end_angle;
    return true;
}

// 0x0045DF10 (:265)
bool vint_element_bitmap_circle::set_end_angle(const vint_variant& _val, int _index)
{
    m_end_angle = (float)_val;
    // Original: reports failure although the value was set.
    return false;
}

// 0x0045DF30 (:272), 0x0045DF50 (:280)
VINT_PROP_WRAPPERS(vint_element_bitmap_circle, num_wedges)

// 0x00471EC0 (:273)
bool vint_element_bitmap_circle::get_num_wedges(vint_variant& _val, int _index) const
{
    _val = m_num_wedges;
    return true;
}

// 0x0045DF70 (:281)
bool vint_element_bitmap_circle::set_num_wedges(const vint_variant& _val, int _index)
{
    m_num_wedges = (unsigned int)_val;
    return true;
}
