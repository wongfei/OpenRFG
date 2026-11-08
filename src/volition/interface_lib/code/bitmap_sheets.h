// Original: volition/interface_lib/code/bitmap_sheets.h
// Implementation: volition/interface_lib/code/bitmap_sheets.cpp
//
// Bitmap sheets: images packed into sheets (pegs), found by name checksum through Bitmap_image_lookup_table; load,
// release and image data (sheet, position and size of an image).
#pragma once


class checksum_stri;
class mempool;
class mempool_base;

// size 0x18
struct bm_image
{
    char* name; // 0x00
    unsigned int sheet_idx; // 0x04
    unsigned int sx; // 0x08
    unsigned int sy; // 0x0C
    unsigned int bmw; // 0x10
    unsigned int bmh; // 0x14
};

// size 0x1
struct bitmap_image_hasher
{
    enum constants
    {
        NULL_REFERENCE = 65535,
    };

    unsigned int hash(const checksum_stri& hash_key); // 0x0045B900 (:50)
    bool valid(const unsigned int& hash_ref, const checksum_stri& hash_key); // 0x0045B910 (:107)

};

extern unsigned int Num_bitmap_images; // 0x01A48620
extern unsigned int Num_bitmap_sheets; // 0x01A4861C (file static in the binary)

void bitmap_sheets_system_init(mempool* _mempool); // 0x00467FD0 (:184)
int bms_load(const char* const image_name); // 0x00471740 (:207)
int bms_load(const checksum_stri& _crc); // 0x00471790 (:219)
void bms_get_dimensions(int image_id, int& w, int& h, float& scale_x, float& scale_y); // 0x00468030 (:233)
void bitmap_sheets_load_all_bitmaps(int start_index); // 0x0045B990 (:268)
void bitmap_sheets_set_thread_ownership(); // 0x0045B9D0 (:279)
void bitmap_sheets_parse_table(const char* _fname, mempool_base* _perm_mempool, mempool_base* _temp_mempool); // 0x004717D0 (:286)
int vint_bms_load(const char* const image_name); // 0x00471940 (:343)
int vint_bms_load(const checksum_stri& name_crc); // 0x00471990 (:359)
void vint_bms_add_ref(int image_id); // 0x0045B9E0 (:374)
void vint_bms_release(int image_id); // 0x0045B9F0 (:389)
int vint_bms_get_bitmap(int image_id); // 0x0045BA00 (:405)
const bm_image* vint_bms_get_image_data(int image_id); // 0x0045BA30 (:417)
const char* bms_get_image_name(int image_id); // 0x0045BA50 (:442)
