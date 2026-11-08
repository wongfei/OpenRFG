// Original: volition/interface_lib/code/resources/vint_resource_bitmap.cpp
//
// Bitmap resources (UI.md UI2): a pool of 384. A name is first looked up in the bitmap sheets (an image of a sheet),
// then loaded through the game's bitmap callbacks; a bitmap can also wrap a bmpman id. A handle is the pool index plus
// a running count in the high half; find accepts a bmpman (title) handle too when the low half is out of the pool's
// range. Compiled in interface_lib_unity.cpp after bitmap_sheets.cpp, whose image and sheet arrays it reads.

#include "volition/interface_lib/code/resources/vint_resource_bitmap.h"

#include "volition/interface_lib/code/bitmap_sheets.h"
#include "volition/interface_lib/code/internal/vint_dlist.h"
#include "volition/vlib/code/math/crc.h"
#include "volition/vlib/code/math/vector2.h"

// ---- public globals
vint_resource_bitmap* Loaded_bmps; // 0x01A48648
vint_resource_bitmap* Avail_bmps; // 0x01A4864C

// ---- file-static globals
static unsigned short Bitmap_count; // 0x01A48650
static vint_resource_bitmap Bitmap_resources[384]; // 0x01A4F628

// 0x0045D6D0 (.h:44)
vint_resource_bitmap::vint_resource_bitmap()
{
}

// 0x0045D6E0 (.h:45)
vint_resource_bitmap::~vint_resource_bitmap()
{
}

// 0x0045D6F0 (.h:47)
void vint_resource_bitmap::add_ref() const
{
    ++const_cast<vint_resource_bitmap*>(this)->m_ref_count;
}

// 0x004684E0 (.h:51)
void vint_resource_bitmap::add_ref(unsigned int _handle)
{
    if (vint_resource_bitmap* b = find(_handle))
    {
        b->add_ref();
    }
}

// 0x00468500 (.h:59)
void vint_resource_bitmap::release(unsigned int _handle)
{
    if (vint_resource_bitmap* b = find(_handle))
    {
        b->release();
    }
}

// 0x0045D700 (.h:67)
unsigned int vint_resource_bitmap::get_handle() const
{
    return m_internal_handle;
}

// 0x0045D710 (.h:71)
int vint_resource_bitmap::get_title_handle() const
{
    return m_title_handle;
}

// 0x0045D720 (.h:87)
int vint_resource_bitmap::get_bms_id() const
{
    return m_bms_image_id;
}

// 0x00460D30 (:67)
void vint_resource_bitmap::system_init()
{
    Loaded_bmps = nullptr;
    Avail_bmps = nullptr;
    for (int i = 0; i < 384; ++i)
    {
        vint_dlist_add(Avail_bmps, &Bitmap_resources[i], &vint_resource_bitmap::next, &vint_resource_bitmap::prev);
    }
    Bitmap_count = 0;
}

// 0x00460EE0 (:80)
void vint_resource_bitmap::system_shutdown()
{
}

// 0x004728D0 (:85)
vint_resource_bitmap* vint_resource_bitmap::load_by_name_crc(const checksum_stri& _crc)
{
    if (Loaded_bmps != nullptr)
    {
        vint_resource_bitmap* b = Loaded_bmps;
        do
        {
            if (b->m_name_crc == _crc.get_crc())
            {
                ++b->m_ref_count;
                return b;
            }
            b = b->next;
        } while (b != Loaded_bmps && b != nullptr);
    }
    unsigned int* reference;
    bitmap_image_hasher hasher;
    if (!Bitmap_image_lookup_table.find(reference, _crc, hasher) || reference == nullptr)
    {
        return nullptr;
    }
    unsigned int image_id = *reference;
    if ((image_id != 0xFFFFFFFF && image_id >= Num_bitmap_images) || (int)image_id < 0 || Avail_bmps == nullptr)
    {
        return nullptr;
    }
    vint_resource_bitmap* b = Avail_bmps;
    b->m_source = VINT_BMP_SOURCE_BMS_IMAGE;
    b->m_title_handle = vint_bms_get_bitmap(image_id);
    b->m_bms_image_id = image_id;
    ++Bitmap_count;
    b->m_internal_handle = (unsigned int)(b - Bitmap_resources) + (unsigned int)Bitmap_count * 0x10000;
    b->m_name_crc = _crc.get_crc();
    b->m_ref_count = 1;
    vint_dlist_remove(Avail_bmps, b, &vint_resource_bitmap::next, &vint_resource_bitmap::prev);
    vint_dlist_add(Loaded_bmps, b, &vint_resource_bitmap::next, &vint_resource_bitmap::prev);
    return b;
}

// 0x00472A60 (:138)
vint_resource_bitmap* vint_resource_bitmap::load(const char* _name)
{
    if (_name == nullptr)
    {
        return nullptr;
    }
    checksum_stri name_crc(_name, 0);
    if (vint_resource_bitmap* b = load_by_name_crc(name_crc))
    {
        return b;
    }
    vint_resource_bitmap* b = Avail_bmps;
    if (b == nullptr || Resource_callbacks == nullptr || Resource_callbacks->bitmap_load == nullptr)
    {
        return nullptr;
    }
    // The free bitmap's title handle is written even when the load fails (it stays -1 then).
    b->m_title_handle = Resource_callbacks->bitmap_load(_name);
    if (b->m_title_handle == -1)
    {
        return nullptr;
    }
    b->m_source = VINT_BMP_SOURCE_BITMAP;
    ++Bitmap_count;
    b->m_internal_handle = (unsigned int)(b - Bitmap_resources) + (unsigned int)Bitmap_count * 0x10000;
    b->m_name_crc = name_crc.get_crc();
    b->m_ref_count = 1;
    vint_dlist_remove(Avail_bmps, b, &vint_resource_bitmap::next, &vint_resource_bitmap::prev);
    vint_dlist_add(Loaded_bmps, b, &vint_resource_bitmap::next, &vint_resource_bitmap::prev);
    return b;
}

// 0x00469860 (:182)
vint_resource_bitmap* vint_resource_bitmap::load_by_bmp_id(unsigned int bitmap_id)
{
    // Original: bitmap_load is the callback checked, bitmap_is_valid the one called.
    if (Resource_callbacks == nullptr || Resource_callbacks->bitmap_load == nullptr)
    {
        return nullptr;
    }
    vint_resource_bitmap* b = Avail_bmps;
    if (b == nullptr)
    {
        return nullptr;
    }
    if (!Resource_callbacks->bitmap_is_valid(bitmap_id))
    {
        return nullptr;
    }
    b->m_title_handle = bitmap_id;
    b->m_source = VINT_BMP_SOURCE_BITMAP;
    ++Bitmap_count;
    b->m_internal_handle = (unsigned int)(b - Bitmap_resources) + (unsigned int)Bitmap_count * 0x10000;
    b->m_name_crc = 0;
    b->m_ref_count = 1;
    vint_dlist_remove(Avail_bmps, b, &vint_resource_bitmap::next, &vint_resource_bitmap::prev);
    vint_dlist_add(Loaded_bmps, b, &vint_resource_bitmap::next, &vint_resource_bitmap::prev);
    return b;
}

// 0x00460EF0 (:232)
vint_resource_bitmap* vint_resource_bitmap::find_by_title(int _handle)
{
    // Original: only the first Bitmap_count slots are searched (the count of loads, not of slots in use).
    for (int i = 0; i < (int)Bitmap_count; ++i)
    {
        if (Bitmap_resources[i].m_title_handle == _handle)
        {
            return &Bitmap_resources[i];
        }
    }
    return nullptr;
}

// 0x00460F20 (:255)
vint_resource_bitmap* vint_resource_bitmap::find(unsigned int _handle)
{
    if (_handle == 0)
    {
        return nullptr;
    }
    unsigned int index = _handle & 0xFFFF;
    if (index < 384)
    {
        if (Bitmap_resources[index].m_internal_handle == _handle)
        {
            return &Bitmap_resources[index];
        }
        return nullptr;
    }
    // out of the pool's range: a bmpman handle
    return find_by_title((int)_handle);
}

// 0x00469980 (:285)
void vint_resource_bitmap::release()
{
    if (m_ref_count != 1)
    {
        --m_ref_count;
        return;
    }
    if (m_source == VINT_BMP_SOURCE_BITMAP && Resource_callbacks != nullptr && Resource_callbacks->bitmap_release != nullptr)
    {
        Resource_callbacks->bitmap_release(m_title_handle);
    }
    m_internal_handle = 0;
    m_title_handle = -1;
    vint_dlist_remove(Loaded_bmps, this, &vint_resource_bitmap::next, &vint_resource_bitmap::prev);
    vint_dlist_add(Avail_bmps, this, &vint_resource_bitmap::next, &vint_resource_bitmap::prev);
}

// 0x00469A40 (:327)
bool vint_resource_bitmap::get_dimensions(vector2i& _size, vector2& _scale) const
{
    if (m_title_handle == -1)
    {
        return false;
    }
    _scale.x = 1.0f;
    _scale.y = 1.0f;
    if (m_source == VINT_BMP_SOURCE_BITMAP)
    {
        if (Resource_callbacks != nullptr && Resource_callbacks->bitmap_get_dimensions != nullptr)
        {
            return Resource_callbacks->bitmap_get_dimensions(_size.x, _size.y, _scale.x, _scale.y, m_title_handle);
        }
    }
    else if (m_source == VINT_BMP_SOURCE_BMS_IMAGE)
    {
        bms_get_dimensions(m_bms_image_id, _size.x, _size.y, _scale.x, _scale.y);
        return true;
    }
    return false;
}

// 0x00469AC0 (:362)
void vint_resource_bitmap::render(const matrix23& _xform, const vector2& _pos, const vector2i& _source_pos, const vector2i& _source_size, vint_res_offset _res_offset_x, vint_res_offset _res_offset_y, vint_res_scale _res_scale_x, vint_res_scale _res_scale_y)
{
    if (m_title_handle == -1)
    {
        return;
    }
    vector2i source_pos = _source_pos;
    if (m_source == VINT_BMP_SOURCE_BMS_IMAGE)
    {
        // a sheet image: its position in the sheet
        if (const bm_image* image = vint_bms_get_image_data(m_bms_image_id))
        {
            source_pos.x += image->sx;
            source_pos.y += image->sy;
        }
    }
    if (Render_callbacks != nullptr && Render_callbacks->draw_bitmap != nullptr)
    {
        Render_callbacks->draw_bitmap(m_title_handle, _xform, _pos, source_pos, _source_size, _res_offset_x, _res_offset_y, _res_scale_x, _res_scale_y);
    }
}

// 0x00469B60 (:382)
void vint_resource_bitmap::render_circle(const matrix23& _xform, const vector2& _pos, const vector2i& _source_pos, const vector2i& _source_size, float _start_angle, float _end_angle, unsigned int _num_wedges, vint_res_offset _res_offset_x, vint_res_offset _res_offset_y, vint_res_scale _res_scale_x, vint_res_scale _res_scale_y)
{
    if (m_title_handle == -1)
    {
        return;
    }
    vector2i source_pos = _source_pos;
    if (m_source == VINT_BMP_SOURCE_BMS_IMAGE)
    {
        if (const bm_image* image = vint_bms_get_image_data(m_bms_image_id))
        {
            source_pos.x += image->sx;
            source_pos.y += image->sy;
        }
    }
    if (Render_callbacks != nullptr && Render_callbacks->draw_bitmap_circle != nullptr)
    {
        Render_callbacks->draw_bitmap_circle(m_title_handle, _xform, _pos, source_pos, _source_size, _start_angle, _end_angle, _num_wedges, _res_offset_x, _res_offset_y, _res_scale_x, _res_scale_y);
    }
}
