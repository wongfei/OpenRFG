// Original: volition/interface_lib/code/resources/vint_resource_bitmap.h
// Implementation: volition/interface_lib/code/resources/vint_resource_bitmap.cpp
//
// A bitmap resource of the documents (a bitmap or a sheet image, reference counted) and the loaded/free lists.
// Placement of vint_bitmap_source inferred.
#pragma once

#include "volition/interface_lib/code/vint_external.h"

class checksum_stri;
class matrix23;
class vector2;

enum vint_bitmap_source
{
    VINT_BMP_SOURCE_UNKNOWN = -1,
    VINT_BMP_SOURCE_BITMAP = 0,
    VINT_BMP_SOURCE_BMS_IMAGE = 1,
    NUM_VINT_BITMAP_SOURCES = 2,
};

// size 0x24
class vint_resource_bitmap
{
public:
    vint_resource_bitmap(); // 0x0045D6D0 (.h:44)
    virtual ~vint_resource_bitmap(); // vtbl+0x00 0x0045D6E0 (.h:45)
    static void add_ref(unsigned int _handle); // 0x004684E0 (.h:51)
    void add_ref() const; // 0x0045D6F0 (.h:47)
    static void release(unsigned int _handle); // 0x00468500 (.h:59)
    virtual void release(); // vtbl+0x04 0x00469980 (:285)
    unsigned int get_handle() const; // 0x0045D700 (.h:67)
    int get_title_handle() const; // 0x0045D710 (.h:71)
    bool get_dimensions(vector2i& _size, vector2& _scale) const; // 0x00469A40 (:327)
    vint_bitmap_source get_source() const;
    int get_bms_id() const; // 0x0045D720 (.h:87)
    void render(const matrix23& _xform, const vector2& _pos, const vector2i& _source_pos, const vector2i& _source_size, vint_res_offset _res_offset_x, vint_res_offset _res_offset_y, vint_res_scale _res_scale_x, vint_res_scale _res_scale_y); // 0x00469AC0 (:362)
    void render_circle(const matrix23& _xform, const vector2& _pos, const vector2i& _source_pos, const vector2i& _source_size, float _start_angle, float _end_angle, unsigned int _num_wedges, vint_res_offset _res_offset_x, vint_res_offset _res_offset_y, vint_res_scale _res_scale_x, vint_res_scale _res_scale_y); // 0x00469B60 (:382)
    static void system_init(); // 0x00460D30 (:67)
    static void system_shutdown(); // 0x00460EE0 (:80)
    static vint_resource_bitmap* load(const char* _name); // 0x00472A60 (:138)
    static vint_resource_bitmap* load_by_name_crc(const checksum_stri& _crc); // 0x004728D0 (:85)
    static vint_resource_bitmap* load_by_bmp_id(unsigned int bitmap_id); // 0x00469860 (:182)
    static vint_resource_bitmap* find(unsigned int _handle); // 0x00460F20 (:255)
    static vint_resource_bitmap* find_by_title(int _handle); // 0x00460EF0 (:232)

    // 0x00 vfptr
private:
    int m_title_handle; // 0x04
    int m_bms_image_id; // 0x08
    vint_bitmap_source m_source; // 0x0C
    unsigned int m_name_crc; // 0x10
    unsigned int m_internal_handle; // 0x14
    int m_ref_count; // 0x18
    vint_resource_bitmap* next; // 0x1C
    vint_resource_bitmap* prev; // 0x20
};

extern vint_resource_bitmap* Loaded_bmps; // 0x01A48648
extern vint_resource_bitmap* Avail_bmps; // 0x01A4864C
