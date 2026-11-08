// Original: volition/interface_lib/code/vint_project.cpp
//
// The project (UI.md UI2): rfg.vint_proj (XML, vint_doc_type "vint_project") lists the documents (name, filename),
// up to 6 resolution modes (id, language, size) and the settings (data directories, fonts with alias and overlay,
// bitmap pegs, one bitmap sheet peg). vint_project_find_best_res_mode picks the mode nearest to 16:9 (preferring the
// language) and fills the resolution scale and offset tables of the vint_res_scale/vint_res_offset modes for the
// screen size against the 1280x720 authoring size. The computations are in double, as rfg.exe's SSE2 code does them.
// The save path is the editor's. Compiled early in interface_lib_unity.cpp: the mode and document tables are read by
// the documents and objects.

#include "volition/interface_lib/code/vint_project.h"

#include "volition/vlib/code/math/crc.h"
#include "volition/vlib/code/math/vector2.h"
#include "volition/vlib/code/memory/vmemory.h"
#include "volition/vlib/code/memory/mempool_base.h"
#include "volition/vlib/code/util/fastxml.h"
#include "volition/vlib/code/util/string_comp.h"
#include "volition/vlib/code/util/string_util.h"
#include "library/modules/runtime/base/include/keen/base/trace.hpp"

#include <math.h>
#include <stdio.h>
#include <string.h>

// ---- file-private types (rfg.pdb)
// size 0xC8
struct vint_document_lookup
{
    unsigned int name_crc; // 0x00
    unsigned int fname_crc; // 0x04
    char name[64]; // 0x08
    char filename[64]; // 0x48
    char fname_base[64]; // 0x88
};
static_assert(sizeof(vint_document_lookup) == 0xC8, "vint_document_lookup");

// ---- public globals
vint_project_settings Vint_project_settings; // 0x01A47B98
int Vint_current_resolution_mode = -1; // 0x014F5AC0

// ---- file-static globals (shared with the unity's other files)
static vint_resolution_mode Resolution_modes[6]; // 0x01A39F20
static vint_document_lookup Document_lookup_table[68]; // 0x01A3A058
int Num_document_lookup_entries; // 0x01A48720 (global in rfg.pdb: downloadable_content.cpp marks it)
int Num_document_lookup_entries_mark; // 0x01A48724
static int Num_resolution_modes; // 0x01A48728
static vector2 Vint_res_scales[6]; // 0x01A4EAE4
static vector2 Vint_res_offsets[8]; // 0x01A55B28

// 0x004659C0 (:84)
// LTCG: root in EAX.
static void parse_resolutions(xml_element* root)
{
    if (root == nullptr)
    {
        return;
    }
    Num_resolution_modes = 0;
    Vint_current_resolution_mode = 0;
    xml_element* resolutions = xml_find(root, "resolutions");
    if (resolutions == nullptr)
    {
        return;
    }
    xml_element* cur = xml_find(resolutions, "resolution");
    while (cur != nullptr && Num_resolution_modes < 6)
    {
        xml_element* e;
        const char* id;
        const char* language;
        const char* text;
        int hres;
        int vres;
        if ((e = xml_find(cur, "id")) != nullptr && (id = xml_get_string(e, nullptr)) != nullptr
            && (e = xml_find(cur, "language")) != nullptr && (language = xml_get_string(e, nullptr)) != nullptr
            && (e = xml_find(cur, "hres")) != nullptr && (text = xml_get_string(e, nullptr)) != nullptr
            && sscanf(text, "%i", &hres) > 0
            && (e = xml_find(cur, "vres")) != nullptr && (text = xml_get_string(e, nullptr)) != nullptr
            && sscanf(text, "%i", &vres) > 0)
        {
            float aspect = (float)hres / (float)vres;
            strncpy_safe(Resolution_modes[Num_resolution_modes].id, id, 32);
            strncpy_safe(Resolution_modes[Num_resolution_modes].language, language, 6);
            Resolution_modes[Num_resolution_modes].hres = hres;
            Resolution_modes[Num_resolution_modes].vres = vres;
            Resolution_modes[Num_resolution_modes].aspect_ratio = aspect;
            ++Num_resolution_modes;
        }
        // Original: the resolution itself is passed as the parent (xml_find_next only checks it for null).
        cur = xml_find_next(cur, cur, "resolution");
    }
}

// 0x00465BB0 (:174)
int vint_project_get_resolution_mode()
{
    return Vint_current_resolution_mode;
}

// 0x00465BC0 (:179)
bool vint_project_set_resolution_mode(int _mode)
{
    if (_mode >= Num_resolution_modes)
    {
        return false;
    }
    Vint_current_resolution_mode = _mode;
    return true;
}

// 0x00465BE0 (:189)
const vint_resolution_mode* get_project_get_resolution_mode_info(int _mode)
{
    if (_mode < Num_resolution_modes)
    {
        return &Resolution_modes[_mode];
    }
    return nullptr;
}

// 0x00473090 (:200)
// LTCG: root in EAX.
static void parse_project_settings(xml_element* root)
{
    vint_project_clear_settings();
    if (root == nullptr)
    {
        return;
    }
    xml_element* settings = xml_find(root, "settings");
    if (settings == nullptr)
    {
        return;
    }
    int count = 0;
    for (xml_element* cur = xml_find(settings, "directory"); cur != nullptr && count < 6; cur = xml_find_next(settings, cur, "directory"))
    {
        xml_element* e = xml_find(cur, "path");
        const char* path = e != nullptr ? xml_get_string(e, nullptr) : nullptr;
        if (path != nullptr && path[0] != '\0' && vint_project_settings_add_dir_name(path))
        {
            ++count;
        }
    }
    count = 0;
    for (xml_element* cur = xml_find(settings, "font_info"); cur != nullptr && count < 6; cur = xml_find_next(settings, cur, "font_info"))
    {
        xml_element* e = xml_find(cur, "filename");
        const char* filename = e != nullptr ? xml_get_string(e, nullptr) : nullptr;
        e = xml_find(cur, "alias");
        const char* alias = e != nullptr ? xml_get_string(e, nullptr) : nullptr;
        e = xml_find(cur, "filename_overlay");
        const char* overlay = e != nullptr ? xml_get_string(e, nullptr) : nullptr;
        // Original: vint_project_settings_add_font_info always returns false, so the count never grows (the
        // limit of 6 is the settings' own).
        if (vint_project_settings_add_font_info(filename, alias, overlay))
        {
            ++count;
        }
    }
    count = 0;
    for (xml_element* cur = xml_find(settings, "bmp_file"); cur != nullptr && count < 6; cur = xml_find_next(settings, cur, "bmp_file"))
    {
        xml_element* e = xml_find(cur, "filename");
        const char* filename = e != nullptr ? xml_get_string(e, nullptr) : nullptr;
        if (filename != nullptr && filename[0] != '\0' && vint_project_settings_add_bmp_peg(filename))
        {
            ++count;
        }
    }
    xml_element* sheet = xml_find(settings, "bmp_sheet_file");
    if (sheet != nullptr && (sheet = xml_find(sheet, "filename")) != nullptr)
    {
        const char* filename = xml_get_string(sheet, nullptr);
        if (filename != nullptr && filename[0] != '\0')
        {
            vint_project_settings_add_bmp_sheet(filename);
        }
    }
}

// 0x0046CA60 (:296)
// LTCG: root in EAX.
static void parse_document_list(xml_element* root)
{
    if (root == nullptr)
    {
        return;
    }
    xml_element* documents = xml_find(root, "documents");
    if (documents == nullptr)
    {
        return;
    }
    xml_element* cur = xml_find(documents, "document");
    if (cur == nullptr)
    {
        return;
    }
    while (Num_document_lookup_entries < 68)
    {
        xml_element* e;
        const char* filename;
        const char* doc_name;
        if ((e = xml_find(cur, "filename")) != nullptr && (filename = xml_get_string(e, nullptr)) != nullptr
            && (e = xml_find(cur, "doc_name")) != nullptr && (doc_name = xml_get_string(e, nullptr)) != nullptr)
        {
            vint_document_lookup& entry = Document_lookup_table[Num_document_lookup_entries];
            strncpy_safe(entry.name, doc_name, 64);
            strncpy_safe(entry.filename, filename, 64);
            strncpy_safe(entry.fname_base, filename, 64);
            vint_project_remove_std_extension(entry.fname_base);
            entry.name_crc = crc_stri(doc_name, 0);
            entry.fname_crc = crc_stri(filename, 0);
            ++Num_document_lookup_entries;
        }
        cur = xml_find_next(documents, cur, "document");
        if (cur == nullptr)
        {
            return;
        }
    }
    keen::debug::nativePrintArgs("Too many documents defined in project, increase VINT_MAX_DOCUMENTS");
}

// 0x00465C00 (:366)
void vint_project_remove_std_extension(char* _s)
{
    unsigned int length = (unsigned int)strlen(_s);
    if (length <= 9)
    {
        return;
    }
    if (strcmp(_s + length - 9, ".vint_doc") == 0)
    {
        _s[length - 9] = '\0';
        return;
    }
    if (strcmp(_s + length - 10, ".vint_xdoc") == 0)
    {
        _s[length - 10] = '\0';
    }
}

// 0x004732C0 (:380)
bool vint_parse_project(const char* _name, mempool_base* _mempool)
{
    bool result = false;
    if (_name == nullptr || _mempool == nullptr)
    {
        Num_document_lookup_entries_mark = Num_document_lookup_entries;
        return false;
    }
    Num_document_lookup_entries = 0;
    unsigned int mark = _mempool->mark();
    xml_element* root = xml_parse(_name, _mempool);
    if (root != nullptr)
    {
        xml_element* type = xml_find(root, "vint_doc_type");
        if (type != nullptr)
        {
            if (strcmp(xml_get_string(type, nullptr), "vint_project") == 0)
            {
                parse_document_list(root);
                parse_resolutions(root);
                parse_project_settings(root);
                result = true;
            }
            else
            {
                keen::debug::nativePrintArgs("Invalid Vint project file");
            }
        }
    }
    _mempool->restore_to_mark(mark);
    // the DLC projects' documents are added after this mark (vint_project_get_num_dlc_filenames)
    Num_document_lookup_entries_mark = Num_document_lookup_entries;
    return result;
}

// 0x004733B0 (:420)
bool vint_reparse_project(const char* _name, mempool_base* _mempool)
{
    if (_name == nullptr || _mempool == nullptr)
    {
        return false;
    }
    unsigned int mark = _mempool->mark();
    xml_element* root = xml_parse(_name, _mempool);
    xml_element* type;
    if (root != nullptr && (type = xml_find(root, "vint_doc_type")) != nullptr)
    {
        if (strcmp(xml_get_string(type, nullptr), "vint_project") == 0)
        {
            // the resolutions are kept; the documents are appended
            parse_document_list(root);
            parse_project_settings(root);
            _mempool->restore_to_mark(mark);
            return true;
        }
        keen::debug::nativePrintArgs("Invalid Vint project file");
    }
    _mempool->restore_to_mark(mark);
    return false;
}

// 0x00465CA0 (:455)
void vint_restore_document_lookup_entries()
{
    Num_document_lookup_entries = Num_document_lookup_entries_mark;
}

// 0x0046CBE0 (:461)
bool vint_save_project(const char* _name, mempool* xml_mempool)
{
    if (_name == nullptr)
    {
        return false;
    }
    unsigned int mark = 0;
    if (xml_mempool != nullptr)
    {
        mark = xml_mempool->mark();
    }
    // The texts stay referenced by the elements until the write.
    char resolution_text[6][4][64];
    char doc_names[64][64];
    xml_element* root = xml_new("root", nullptr, nullptr, nullptr);
    xml_new("vint_doc_type", "vint_project", root, nullptr);
    xml_element* resolutions = xml_new("resolutions", nullptr, root, nullptr);
    for (int i = 0; i < Num_resolution_modes; ++i)
    {
        xml_element* resolution = xml_new("resolution", nullptr, resolutions, nullptr);
        _snprintf_safe(resolution_text[i][0], 64, "%s", Resolution_modes[i].id);
        xml_new("id", resolution_text[i][0], resolution, nullptr);
        _snprintf_safe(resolution_text[i][1], 64, "%s", Resolution_modes[i].language);
        xml_new("language", resolution_text[i][1], resolution, nullptr);
        _snprintf_safe(resolution_text[i][2], 64, "%d", Resolution_modes[i].hres);
        xml_new("hres", resolution_text[i][2], resolution, nullptr);
        _snprintf_safe(resolution_text[i][3], 64, "%d", Resolution_modes[i].vres);
        xml_new("vres", resolution_text[i][3], resolution, nullptr);
    }
    xml_new("languages", nullptr, root, nullptr);
    xml_element* settings = xml_new("settings", nullptr, root, nullptr);
    if (settings != nullptr)
    {
        unsigned int num_dirs = vint_project_settings_get_num_dirs();
        for (unsigned int i = 0; i < num_dirs; ++i)
        {
            xml_element* dir = xml_new("directory", nullptr, settings, nullptr);
            xml_new("path", Vint_project_settings.data_directory[i], dir, nullptr);
        }
        unsigned int num_fonts = vint_project_settings_get_num_fonts();
        for (unsigned int i = 0; i < num_fonts && Vint_project_settings.fonts[i].alias[0] != '\0'; ++i)
        {
            xml_element* font = xml_new("font_info", nullptr, settings, nullptr);
            xml_new("filename", Vint_project_settings.fonts[i].base_font_name, font, nullptr);
            xml_new("alias", Vint_project_settings.fonts[i].alias, font, nullptr);
            xml_new("filename_overlay", Vint_project_settings.fonts[i].overlay_font_name, font, nullptr);
        }
        unsigned int num_pegs = vint_project_settings_get_num_bmp_pegs();
        for (unsigned int i = 0; i < num_pegs; ++i)
        {
            xml_element* peg = xml_new("bmp_file", nullptr, settings, nullptr);
            xml_new("filename", Vint_project_settings.peg_name[i], peg, nullptr);
        }
        xml_element* sheet = xml_new("bmp_sheet_file", nullptr, settings, nullptr);
        xml_new("filename", Vint_project_settings.bmp_sheet_peg_name, sheet, nullptr);
    }
    xml_element* documents = xml_new("documents", nullptr, root, nullptr);
    // Original: the document names go to a 4096-byte stack buffer, 64 bytes each (64 documents; the table has 68).
    for (int i = 0; i < Num_document_lookup_entries && i < 64; ++i)
    {
        const char* filename = Document_lookup_table[i].filename;
        xml_element* document = xml_new("document", nullptr, documents, nullptr);
        xml_new("filename", filename, document, nullptr);
        // the doc name is the filename up to the first '.'
        memset(doc_names[i], 0, 64);
        unsigned int length = (unsigned int)strlen(filename);
        for (unsigned int c = 0; filename[c] != '.' && c < 64 && c < length; ++c)
        {
            doc_names[i][c] = filename[c];
        }
        xml_new("doc_name", doc_names[i], document, nullptr);
    }
    bool result = xml_write(_name, root);
    if (xml_mempool != nullptr)
    {
        xml_mempool->restore_to_mark(mark);
    }
    return result;
}

// 0x00465CB0 (:600)
int vint_project_get_num_resolution_modes()
{
    return Num_resolution_modes;
}

// 0x00465CC0 (:606)
void vint_project_get_resolution_mode(int& _hres, int& _vres, int _index)
{
    _vres = 0;
    _hres = 0;
    if (_index >= 0 && _index < Num_resolution_modes)
    {
        _hres = Resolution_modes[_index].hres;
        _vres = Resolution_modes[_index].vres;
    }
}

// 0x00465D00 (:617)
const char* vint_project_get_resolution_mode_surname(int _index)
{
    // an invalid index gives null (rfg.exe: the id of a null mode, at offset 0)
    if (_index >= 0 && _index < Num_resolution_modes)
    {
        return Resolution_modes[_index].id;
    }
    return nullptr;
}

// 0x00465D20 (:628)
int vint_project_find_resolution_mode(const char* _surname)
{
    if (_surname == nullptr)
    {
        return -1;
    }
    for (int i = 0; i < Num_resolution_modes; ++i)
    {
        if (strcmp(_surname, Resolution_modes[i].id) == 0)
        {
            return i;
        }
    }
    return -1;
}

// 0x00465D80 (:645)
bool vint_project_add_resolution_mode(const char* _id, const char* _language, int _hres, int _vres)
{
    if (Num_resolution_modes >= 6)
    {
        return false;
    }
    strncpy_safe(Resolution_modes[Num_resolution_modes].id, _id, 32);
    strncpy_safe(Resolution_modes[Num_resolution_modes].language, _language, 6);
    Resolution_modes[Num_resolution_modes].hres = _hres;
    Resolution_modes[Num_resolution_modes].vres = _vres;
    Resolution_modes[Num_resolution_modes].aspect_ratio = (float)_hres / (float)_vres;
    ++Num_resolution_modes;
    return true;
}

// 0x00465E20 (:665)
bool vint_project_set_resolution_mode_surname(int _index, const char* _surname)
{
    if (_index < 0 || _index >= Num_resolution_modes || _surname == nullptr)
    {
        return false;
    }
    // Original bug kept: writes the mode after the last one instead of _index.
    strncpy_safe(Resolution_modes[Num_resolution_modes].id, _surname, 32);
    return true;
}

// 0x00465E60 (:679)
bool vint_project_delete_resolution_mode(int _index)
{
    if (_index < 0 || _index >= Num_resolution_modes)
    {
        return false;
    }
    for (; _index < Num_resolution_modes - 1; ++_index)
    {
        Resolution_modes[_index].aspect_ratio = Resolution_modes[_index + 1].aspect_ratio;
        Resolution_modes[_index].hres = Resolution_modes[_index + 1].hres;
        Resolution_modes[_index].vres = Resolution_modes[_index + 1].vres;
        strncpy_safe(Resolution_modes[_index].id, Resolution_modes[_index + 1].id, 32);
        strncpy_safe(Resolution_modes[_index].language, Resolution_modes[_index + 1].language, 6);
    }
    Resolution_modes[_index].aspect_ratio = 0.0f;
    Resolution_modes[_index].hres = 0;
    Resolution_modes[_index].vres = 0;
    memset(Resolution_modes[_index].id, 0, sizeof(Resolution_modes[_index].id));
    memset(Resolution_modes[_index].language, 0, sizeof(Resolution_modes[_index].language));
    --Num_resolution_modes;
    return true;
}

// 0x00465F40 (:709)
bool vint_project_swap_resolutions(int _index1, int _index2)
{
    if (_index1 < 0 || _index1 >= Num_resolution_modes || _index2 < 0 || _index2 >= Num_resolution_modes || _index1 == _index2)
    {
        return false;
    }
    vint_resolution_mode temp;
    temp.hres = Resolution_modes[_index1].hres;
    temp.vres = Resolution_modes[_index1].vres;
    temp.aspect_ratio = Resolution_modes[_index1].aspect_ratio;
    strncpy_safe(temp.id, Resolution_modes[_index1].id, 32);
    strncpy_safe(temp.language, Resolution_modes[_index1].language, 6);
    Resolution_modes[_index1].hres = Resolution_modes[_index2].hres;
    Resolution_modes[_index1].vres = Resolution_modes[_index2].vres;
    Resolution_modes[_index1].aspect_ratio = Resolution_modes[_index2].aspect_ratio;
    strncpy_safe(Resolution_modes[_index1].id, Resolution_modes[_index2].id, 32);
    strncpy_safe(Resolution_modes[_index1].language, Resolution_modes[_index2].language, 6);
    Resolution_modes[_index2].hres = temp.hres;
    Resolution_modes[_index2].vres = temp.vres;
    Resolution_modes[_index2].aspect_ratio = temp.aspect_ratio;
    strncpy_safe(Resolution_modes[_index2].id, temp.id, 32);
    strncpy_safe(Resolution_modes[_index2].language, temp.language, 6);
    return true;
}

// 0x00466070 (:746)
int vint_project_get_num_filenames()
{
    return Num_document_lookup_entries;
}

// 0x00466080 (:752)
int vint_project_get_num_dlc_filenames()
{
    return Num_document_lookup_entries - Num_document_lookup_entries_mark;
}

// 0x00466090 (:758)
const char* vint_project_get_doc_filename(int idx)
{
    if (idx >= 0 && idx < Num_document_lookup_entries)
    {
        return Document_lookup_table[idx].filename;
    }
    return nullptr;
}

// 0x004660C0 (:777)
const char* vint_project_get_doc_name(int idx)
{
    if (idx >= 0 && idx < Num_document_lookup_entries)
    {
        return Document_lookup_table[idx].name;
    }
    return nullptr;
}

// 0x004660F0 (:796)
const char* vint_project_get_doc_filename(const char* _name)
{
    unsigned int name_crc = crc_stri(_name, 0);
    const char* filename = nullptr;
    // the last match wins (a DLC project's entry after the base one)
    for (int i = 0; i < Num_document_lookup_entries; ++i)
    {
        if (Document_lookup_table[i].name_crc == name_crc)
        {
            filename = Document_lookup_table[i].filename;
        }
    }
    return filename;
}

// 0x00466130 (:819)
bool vint_project_add_doc_filename(const char* _filename, const char* _doc_alias)
{
    if (_filename == nullptr || _doc_alias == nullptr)
    {
        return false;
    }
    unsigned int fname_crc = crc_stri(_filename, 0);
    int index = Num_document_lookup_entries;
    for (int i = 0; i < Num_document_lookup_entries; ++i)
    {
        if (Document_lookup_table[i].fname_crc == fname_crc)
        {
            if (v_stricmp(_filename, Document_lookup_table[i].filename) == 0)
            {
                return false;
            }
            // Original: a crc collision with another name overwrites that entry.
            index = i;
            break;
        }
    }
    if (index == Num_document_lookup_entries)
    {
        if (Num_document_lookup_entries >= 68)
        {
            keen::debug::nativePrintArgs("Not enough room for document in project");
            return false;
        }
        ++Num_document_lookup_entries;
    }
    // the name is not copied, only its crc
    strncpy_safe(Document_lookup_table[index].filename, _filename, 64);
    strncpy_safe(Document_lookup_table[index].fname_base, _filename, 64);
    vint_project_remove_std_extension(Document_lookup_table[index].fname_base);
    Document_lookup_table[index].name_crc = crc_stri(_doc_alias, 0);
    Document_lookup_table[index].fname_crc = crc_stri(_filename, 0);
    return true;
}

// 0x00466220 (:861)
bool vint_project_has_filename(const char* _filename)
{
    if (_filename == nullptr)
    {
        return false;
    }
    char base[64];
    strncpy_safe(base, _filename, 64);
    vint_project_remove_std_extension(base);
    // Original: every slot of the table is compared, used or not.
    for (int i = 0; i < 68; ++i)
    {
        if (strcmp(Document_lookup_table[i].fname_base, base) == 0)
        {
            return true;
        }
    }
    return false;
}

// 0x004662E0 (:883)
int vint_project_find_best_res_mode(const vector2i& _screen_dims, const char* _language_name)
{
    // 16:9 as a float, compared in double
    const double target_aspect = (double)(16.0f / 9.0f);
    const char* matched_language = nullptr;
    int best = -1;
    float best_diff = 50000.0f;
    for (int i = 0; i < Num_resolution_modes; ++i)
    {
        float diff = (float)fabs(target_aspect - (double)Resolution_modes[i].aspect_ratio);
        bool take;
        if (best == -1)
        {
            take = true;
        }
        else if (matched_language == nullptr && v_stricmp(Resolution_modes[i].language, _language_name) == 0)
        {
            take = true;
        }
        else if ((double)best_diff > (double)diff)
        {
            // Original bug kept: once a mode of the language was taken, the language test compares the language
            // with itself, so any nearer aspect ratio wins.
            take = matched_language == nullptr || v_stricmp(matched_language, _language_name) == 0;
        }
        else
        {
            take = false;
        }
        if (take)
        {
            best_diff = diff;
            best = i;
            if (v_stricmp(Resolution_modes[i].language, _language_name) == 0)
            {
                matched_language = _language_name;
            }
        }
    }

    // Original bug kept: with no modes, mode -1 is read.
    float scale_x = (float)((double)(float)_screen_dims.x / (double)(float)Resolution_modes[best].hres);
    float scale_y = (float)((double)(float)_screen_dims.y / (double)(float)Resolution_modes[best].vres);
    Vint_res_scales[VINT_RES_SCALE_FILL].x = scale_x;
    Vint_res_scales[VINT_RES_SCALE_FILL].y = scale_y;
    float scale_diff = (float)((double)scale_x - (double)scale_y);
    float min_scale = scale_diff < 0.0f ? scale_x : scale_y;
    float max_scale = scale_diff >= 0.0f ? scale_x : scale_y;

    // maintain ratio: the smaller scale, centred (offsets: start, none, centre, end)
    Vint_res_scales[VINT_RES_SCALE_MAINTAIN_RATIO].x = min_scale;
    Vint_res_scales[VINT_RES_SCALE_MAINTAIN_RATIO].y = min_scale;
    Vint_res_scales[VINT_RES_SCALE_INHERIT].x = min_scale;
    Vint_res_scales[VINT_RES_SCALE_INHERIT].y = min_scale;
    float offset_x = (float)(((double)(float)_screen_dims.x - (double)min_scale * 1280.0) * 0.5);
    float offset_y = (float)(((double)(float)_screen_dims.y - (double)min_scale * 720.0) * 0.5);
    Vint_res_offsets[VINT_RES_OFFSET_INHERIT].x = offset_x;
    Vint_res_offsets[VINT_RES_OFFSET_INHERIT].y = offset_y;
    Vint_res_offsets[VINT_RES_OFFSET_START].x = (float)((double)offset_x * 0.0);
    Vint_res_offsets[VINT_RES_OFFSET_START].y = (float)((double)offset_y * 0.0);
    Vint_res_offsets[VINT_RES_OFFSET_CENTER].x = offset_x;
    Vint_res_offsets[VINT_RES_OFFSET_CENTER].y = offset_y;
    Vint_res_offsets[VINT_RES_OFFSET_END].x = (float)((double)offset_x * 2.0);
    Vint_res_offsets[VINT_RES_OFFSET_END].y = (float)((double)offset_y * 2.0);

    // cutoff: the larger scale (negative offsets)
    Vint_res_scales[VINT_RES_SCALE_CUTOFF].x = max_scale;
    Vint_res_scales[VINT_RES_SCALE_CUTOFF].y = max_scale;
    float cut_offset_x = (float)(((double)(float)_screen_dims.x - (double)max_scale * 1280.0) * 0.5);
    float cut_offset_y = (float)(((double)(float)_screen_dims.y - (double)max_scale * 720.0) * 0.5);
    Vint_res_offsets[VINT_RES_OFFSET_NEG_START].x = cut_offset_x;
    Vint_res_offsets[VINT_RES_OFFSET_NEG_START].y = cut_offset_y;
    Vint_res_offsets[VINT_RES_OFFSET_NEG_START_TWICE].x = (float)((double)cut_offset_x * 2.0);
    Vint_res_offsets[VINT_RES_OFFSET_NEG_START_TWICE].y = (float)((double)cut_offset_y * 2.0);

    // cutoff in x when the screen is narrower than 16:9, cutoff in y when wider
    float aspect = (float)((double)(float)_screen_dims.x / (double)(float)_screen_dims.y);
    if (target_aspect > (double)aspect)
    {
        Vint_res_offsets[VINT_RES_OFFSET_NEG_START_X].x = cut_offset_x;
        Vint_res_offsets[VINT_RES_OFFSET_NEG_START_X].y = cut_offset_y;
        Vint_res_scales[VINT_RES_SCALE_CUTOFF_X].x = max_scale;
        Vint_res_scales[VINT_RES_SCALE_CUTOFF_X].y = max_scale;
    }
    else
    {
        Vint_res_offsets[VINT_RES_OFFSET_NEG_START_X].y = offset_y;
        Vint_res_offsets[VINT_RES_OFFSET_NEG_START_X].x = offset_x;
        Vint_res_scales[VINT_RES_SCALE_CUTOFF_X].x = min_scale;
        Vint_res_scales[VINT_RES_SCALE_CUTOFF_X].y = min_scale;
    }
    if ((double)aspect > target_aspect)
    {
        Vint_res_offsets[VINT_RES_OFFSET_NEG_START_Y].x = cut_offset_x;
        Vint_res_offsets[VINT_RES_OFFSET_NEG_START_Y].y = cut_offset_y;
        Vint_res_scales[VINT_RES_SCALE_CUTOFF_Y].x = max_scale;
        Vint_res_scales[VINT_RES_SCALE_CUTOFF_Y].y = max_scale;
    }
    else
    {
        Vint_res_scales[VINT_RES_SCALE_CUTOFF_Y].x = min_scale;
        Vint_res_offsets[VINT_RES_OFFSET_NEG_START_Y].y = offset_y;
        Vint_res_offsets[VINT_RES_OFFSET_NEG_START_Y].x = offset_x;
        Vint_res_scales[VINT_RES_SCALE_CUTOFF_Y].y = min_scale;
    }
    return best;
}

// 0x0046CFD0 (:985)
bool vint_project_settings_add_dir_name(const char* dir_name)
{
    if (dir_name == nullptr)
    {
        return false;
    }
    unsigned int index = vint_project_settings_get_num_dirs();
    if (index >= 6 || vint_project_settings_has_dir(dir_name))
    {
        return false;
    }
    strncpy_safe(Vint_project_settings.data_directory[index], dir_name, 128);
    return true;
}

// 0x0046D030 (:1006)
bool vint_project_settings_add_font_info(const char* font_name, const char* font_alias, const char* font_overlay)
{
    // Original: always returns false, even when the font was added.
    if (font_name == nullptr || font_alias == nullptr)
    {
        return false;
    }
    unsigned int index = vint_project_settings_get_num_fonts();
    if (index < 6 && vint_project_settings_find_font(font_alias) == nullptr)
    {
        strncpy_safe(Vint_project_settings.fonts[index].base_font_name, font_name, 64);
        strncpy_safe(Vint_project_settings.fonts[index].alias, font_alias, 64);
        if (font_overlay != nullptr)
        {
            strncpy_safe(Vint_project_settings.fonts[index].overlay_font_name, font_overlay, 64);
            return false;
        }
        Vint_project_settings.fonts[index].overlay_font_name[0] = '\0';
    }
    return false;
}

// 0x0046D0D0 (:1035)
bool vint_project_settings_add_bmp_peg(const char* bmp_name)
{
    if (bmp_name == nullptr)
    {
        return false;
    }
    unsigned int index = vint_project_settings_get_num_bmp_pegs();
    if (index >= 6 || vint_project_settings_has_bmp_peg(bmp_name))
    {
        return false;
    }
    // Original bug kept: copies up to 128 bytes into a 64-byte name (overflowing into the next one).
    strncpy_safe(Vint_project_settings.peg_name[index], bmp_name, 128);
    return true;
}

// 0x00466720 (:1057)
bool vint_project_settings_add_bmp_sheet(const char* sheet_name)
{
    memset(Vint_project_settings.bmp_sheet_peg_name, 0, sizeof(Vint_project_settings.bmp_sheet_peg_name));
    if (sheet_name != nullptr)
    {
        strncpy_safe(Vint_project_settings.bmp_sheet_peg_name, sheet_name, 64);
    }
    return false;
}

// 0x00466750 (:1069)
bool vint_project_settings_has_dir(const char* dir)
{
    if (dir == nullptr)
    {
        return false;
    }
    for (int i = 0; i < 6 && Vint_project_settings.data_directory[i][0] != '\0'; ++i)
    {
        if (v_stricmp(Vint_project_settings.data_directory[i], dir) == 0)
        {
            return true;
        }
    }
    return false;
}

// 0x004667A0 (:1094)
const vint_project_font* vint_project_settings_find_font(const char* alias)
{
    if (alias == nullptr)
    {
        return nullptr;
    }
    for (int i = 0; i < 6 && Vint_project_settings.fonts[i].alias[0] != '\0'; ++i)
    {
        if (v_stricmp(Vint_project_settings.fonts[i].alias, alias) == 0)
        {
            return &Vint_project_settings.fonts[i];
        }
    }
    return nullptr;
}

// 0x004667F0 (:1114)
bool vint_project_settings_has_bmp_peg(const char* peg)
{
    if (peg == nullptr)
    {
        return false;
    }
    for (int i = 0; i < 6 && Vint_project_settings.peg_name[i][0] != '\0'; ++i)
    {
        if (v_stricmp(Vint_project_settings.peg_name[i], peg) == 0)
        {
            return true;
        }
    }
    return false;
}

// 0x00466840 (:1139)
bool vint_project_settings_has_bmp_sheet(const char* sheet)
{
    return sheet != nullptr && Vint_project_settings.bmp_sheet_peg_name[0] != '\0' && v_stricmp(Vint_project_settings.bmp_sheet_peg_name, sheet) == 0;
}

// 0x00466870 (:1158)
unsigned int vint_project_settings_get_num_dirs()
{
    unsigned int count = 0;
    while (count < 6 && Vint_project_settings.data_directory[count][0] != '\0')
    {
        ++count;
    }
    return count;
}

// 0x00466890 (:1173)
unsigned int vint_project_settings_get_num_fonts()
{
    unsigned int count = 0;
    while (count < 6 && Vint_project_settings.fonts[count].alias[0] != '\0')
    {
        ++count;
    }
    return count;
}

// 0x004668B0 (:1188)
unsigned int vint_project_settings_get_num_bmp_pegs()
{
    unsigned int count = 0;
    while (count < 6 && Vint_project_settings.peg_name[count][0] != '\0')
    {
        ++count;
    }
    return count;
}

// 0x004668D0 (:1203)
unsigned int vint_project_settings_get_num_bmp_sheets()
{
    return Vint_project_settings.bmp_sheet_peg_name[0] != '\0' ? 1 : 0;
}

// 0x004668E0 (:1215)
void vint_project_clear_settings()
{
    for (int i = 0; i < 6; ++i)
    {
        memset(Vint_project_settings.data_directory[i], 0, 128);
        memset(Vint_project_settings.peg_name[i], 0, 64);
    }
    memset(Vint_project_settings.bmp_sheet_peg_name, 0, 64);
    memset(Vint_project_settings.fonts, 0, sizeof(Vint_project_settings.fonts));
}

// 0x00466940 (:1229)
vector2 vint_project_get_scale(vint_res_scale scale_mode_x, vint_res_scale scale_mode_y)
{
    return vector2(Vint_res_scales[scale_mode_x].x, Vint_res_scales[scale_mode_y].y);
}

// 0x00466960 (:1234)
vector2 vint_project_get_offset(vint_res_offset offset_mode_x, vint_res_offset offset_mode_y)
{
    return vector2(Vint_res_offsets[offset_mode_x].x, Vint_res_offsets[offset_mode_y].y);
}
