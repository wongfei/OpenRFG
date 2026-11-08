// Original: volition/interface_lib/code/bitmap_sheets.cpp
//
// Bitmap sheets (UI.md UI2): an XML table lists sheets (pegs, "<name>.tga") and the images packed in each (start
// position and size). Image and sheet names go to a string pool; the lookup table maps a name checksum to an image
// index. The sheets and images share the table and their indices: a sheet inserts its sheet index, which the lookup's
// validity check (Bitmap_image[index].name) then compares against an image's name (kept as in rfg.exe). Compiled first
// in interface_lib_unity.cpp: the image and sheet arrays are used by the resources and string processing.

#include "volition/interface_lib/code/bitmap_sheets.h"

#include "volition/interface_lib/code/vhashtable.inl"
#include "volition/interface_lib/code/vint_external.h"
#include "volition/vlib/code/bmpman/bmpman.h"
#include "volition/vlib/code/cfile/cfile.h"
#include "volition/vlib/code/math/crc.h"
#include "volition/vlib/code/memory/vmemory.h"
#include "volition/vlib/code/memory/mempool_base.h"
#include "volition/vlib/code/util/fastxml.h"
#include "volition/vlib/code/util/string_pool.h"

// ---- file-private types (rfg.pdb)
// size 0xC
struct bm_sheet
{
    const char* name; // 0x00
    int bmpman_id; // 0x04
    unsigned int mem_mark; // 0x08
};
static_assert(sizeof(bm_sheet) == 0xC, "bm_sheet");

// ---- public globals
unsigned int Num_bitmap_images; // 0x01A48620

// ---- file-static globals (shared with the unity's other files)
static vhashtable<checksum_stri,unsigned int,bitmap_image_hasher> Bitmap_image_lookup_table; // 0x014F5B64
static bm_sheet Bitmap_sheet[196]; // 0x01A390F0
static bm_image Bitmap_image[1536]; // 0x01A3E798
// External since UI3: the document manager restores it for the DLC sheets.
unsigned int Num_bitmap_sheets; // 0x01A4861C
static string_pool Bitmap_image_names("Bitmap Image Names"); // 0x01A4F440

#pragma warning(disable : 4661) // the const find overload has no body: nothing calls it
#pragma warning(push)
template class vhashtable<checksum_stri,unsigned int,bitmap_image_hasher>;
#pragma warning(pop)

// 0x0045B900 (:50)
unsigned int bitmap_image_hasher::hash(const checksum_stri& hash_key)
{
    return hash_key.get_crc();
}

// 0x0045B910 (:107)
bool bitmap_image_hasher::valid(const unsigned int& hash_ref, const checksum_stri& hash_key)
{
    return checksum_stri(Bitmap_image[hash_ref].name, 0).get_crc() == hash_key.get_crc();
}

// 0x0045B940 (:112)
// LTCG: _element in EAX, _tag in ECX, _default on the stack.
static unsigned int xml_get_req_uint(xml_element* _element, const char* _tag, unsigned int _default)
{
    if (_element == nullptr || _tag == nullptr)
    {
        return 0;
    }
    xml_element* e = xml_find(_element, _tag);
    if (e == nullptr)
    {
        return _default;
    }
    return xml_get_uint(e, 0);
}

// 0x0045B970 (:126)
// LTCG: _root in EAX, _tag in ECX, _default in ESI.
static const char* xml_get_req_string(const xml_element* _root, const char* _tag, const char* _default)
{
    if (_root != nullptr && _tag != nullptr)
    {
        xml_element* e = xml_find(_root, _tag);
        if (e != nullptr && e->text != nullptr)
        {
            return e->text;
        }
    }
    return _default;
}

// 0x004715F0 (:143)
// LTCG: root in EAX, index on the stack.
static void bitmap_sheets_parse_images(const unsigned int index, xml_element* const root)
{
    xml_element* images = xml_find(root, "Images");
    if (images == nullptr)
    {
        return;
    }
    for (xml_element* cur = xml_find(images, "Properties"); cur != nullptr; cur = xml_find_next(images, cur, "Properties"))
    {
        const char* name = xml_get_req_string(cur, "Name", "");
        if (Num_bitmap_images >= 1536)
        {
            break;
        }
        Bitmap_image[Num_bitmap_images].name = Bitmap_image_names.add_unique(name);
        bitmap_image_hasher hasher;
        Bitmap_image_lookup_table.insert(checksum_stri(name, 0), Num_bitmap_images, hasher);
        Bitmap_image[Num_bitmap_images].sheet_idx = index;
        Bitmap_image[Num_bitmap_images].sx = xml_get_req_uint(cur, "StartX", 0);
        Bitmap_image[Num_bitmap_images].sy = xml_get_req_uint(cur, "StartY", 0);
        Bitmap_image[Num_bitmap_images].bmw = xml_get_req_uint(cur, "ImageWidth", 0);
        Bitmap_image[Num_bitmap_images].bmh = xml_get_req_uint(cur, "ImageHeight", 0);
        ++Num_bitmap_images;
    }
}

// 0x00467FD0 (:184)
void bitmap_sheets_system_init(mempool* _mempool)
{
    // 0x900 slots for 196 sheets and 1536 images
    Bitmap_image_lookup_table.initialize((unsigned int*)_mempool->alloc(0x900 * sizeof(unsigned int), 0), 0x900);
    void* names = _mempool->alloc(0xC000, 0);
    Bitmap_image_names.init(names, 0xC000, "Bitmap Image Names", 0, 0);
}

// 0x00471740 (:207)
int bms_load(const char* const image_name)
{
    unsigned int* reference;
    bitmap_image_hasher hasher;
    if (Bitmap_image_lookup_table.find(reference, checksum_stri(image_name, 0), hasher) && reference != nullptr)
    {
        return *reference;
    }
    return -1;
}

// 0x00471790 (:219)
int bms_load(const checksum_stri& _crc)
{
    unsigned int* reference;
    bitmap_image_hasher hasher;
    if (Bitmap_image_lookup_table.find(reference, _crc, hasher) && reference != nullptr)
    {
        return *reference;
    }
    return -1;
}

// 0x00468030 (:233)
void bms_get_dimensions(int image_id, int& w, int& h, float& scale_x, float& scale_y)
{
    if (Resource_callbacks == nullptr || Resource_callbacks->bitmap_get_dimensions == nullptr)
    {
        return;
    }
    int bmpman_id = -1;
    if ((unsigned int)image_id < Num_bitmap_images)
    {
        bmpman_id = Bitmap_sheet[Bitmap_image[image_id].sheet_idx].bmpman_id;
    }
    int sheet_w = 0;
    int sheet_h = 0;
    float sx = 1.0f;
    float sy = 1.0f;
    // the sheet's scale; the size is the image's
    Resource_callbacks->bitmap_get_dimensions(sheet_w, sheet_h, sx, sy, bmpman_id);
    w = sheet_w;
    h = sheet_h;
    if ((unsigned int)image_id < Num_bitmap_images)
    {
        w = Bitmap_image[image_id].bmw;
        h = Bitmap_image[image_id].bmh;
    }
    scale_x = sx;
    scale_y = sy;
}

// 0x0045B990 (:268)
void bitmap_sheets_load_all_bitmaps(int start_index)
{
    for (unsigned int i = start_index; i < Num_bitmap_sheets; ++i)
    {
        Bitmap_sheet[i].bmpman_id = bm_load(Bitmap_sheet[i].name, false);
    }
}

// 0x0045B9D0 (:279)
void bitmap_sheets_set_thread_ownership()
{
    Bitmap_image_names.set_thread_ownership();
}

// 0x004717D0 (:286)
void bitmap_sheets_parse_table(const char* _fname, mempool_base* _perm_mempool, mempool_base* _temp_mempool)
{
    if (_fname == nullptr || _perm_mempool == nullptr || _temp_mempool == nullptr || _perm_mempool == _temp_mempool)
    {
        return;
    }
    unsigned int mark = _temp_mempool->mark();
    xml_element* root = xml_parse(_fname, _temp_mempool);
    if (root == nullptr)
    {
        return;
    }
    xml_element* table = xml_find(root, "Table");
    if (table == nullptr)
    {
        // Original bug kept: the temp pool is not restored to its mark when the table is missing.
        return;
    }
    xml_element* cur = xml_find(table, "BitmapSheets");
    if (cur == nullptr)
    {
        return;
    }
    do
    {
        if (Num_bitmap_sheets >= 196)
        {
            break;
        }
        const char* name = xml_get_req_string(cur, "Name", "");
        char filename[65];
        cf_add_ext(filename, 65, name, ".tga", false);
        Bitmap_sheet[Num_bitmap_sheets].name = Bitmap_image_names.add_unique(filename);
        bitmap_image_hasher hasher;
        Bitmap_image_lookup_table.insert(checksum_stri(filename, 0), Num_bitmap_sheets, hasher);
        Bitmap_sheet[Num_bitmap_sheets].bmpman_id = -1;
        bitmap_sheets_parse_images(Num_bitmap_sheets, cur);
        ++Num_bitmap_sheets;
        cur = xml_find_next(table, cur, "BitmapSheets");
    } while (cur != nullptr);
    _temp_mempool->restore_to_mark(mark);
}

// 0x00471940 (:343)
int vint_bms_load(const char* const image_name)
{
    unsigned int* reference;
    bitmap_image_hasher hasher;
    if (!Bitmap_image_lookup_table.find(reference, checksum_stri(image_name, 0), hasher) || reference == nullptr)
    {
        return -1;
    }
    int id = (int)*reference;
    if (id >= 0 && (unsigned int)id >= Num_bitmap_images)
    {
        return -1;
    }
    return id;
}

// 0x00471990 (:359)
int vint_bms_load(const checksum_stri& name_crc)
{
    unsigned int* reference;
    bitmap_image_hasher hasher;
    if (!Bitmap_image_lookup_table.find(reference, name_crc, hasher) || reference == nullptr)
    {
        return -1;
    }
    unsigned int id = *reference;
    if (id != 0xFFFFFFFF && id >= Num_bitmap_images)
    {
        return -1;
    }
    return (int)id;
}

// 0x0045B9E0 (:374)
void vint_bms_add_ref(int image_id)
{
}

// 0x0045B9F0 (:389)
void vint_bms_release(int image_id)
{
}

// 0x0045BA00 (:405)
int vint_bms_get_bitmap(int image_id)
{
    if ((unsigned int)image_id < Num_bitmap_images && Bitmap_image[image_id].sheet_idx < Num_bitmap_sheets)
    {
        return Bitmap_sheet[Bitmap_image[image_id].sheet_idx].bmpman_id;
    }
    return -1;
}

// 0x0045BA30 (:417)
const bm_image* vint_bms_get_image_data(int image_id)
{
    if ((unsigned int)image_id >= Num_bitmap_images)
    {
        return nullptr;
    }
    return &Bitmap_image[image_id];
}

// 0x0045BA50 (:442)
const char* bms_get_image_name(int image_id)
{
    if ((unsigned int)image_id >= Num_bitmap_images)
    {
        return nullptr;
    }
    return Bitmap_image[image_id].name;
}
