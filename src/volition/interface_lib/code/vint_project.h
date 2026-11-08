// Original: volition/interface_lib/code/vint_project.h
// Implementation: volition/interface_lib/code/vint_project.cpp
//
// The project (rfg.vint_proj, XML): resolution modes (screen size, scale and offset), fonts with their aliases, the
// documents and the include pegs; parse, best resolution mode, scale and offset. Placement of the project types
// inferred.
#pragma once

#include "volition/interface_lib/code/vint_external.h"

class mempool;
class mempool_base;
class vector2;

// size 0x34
struct vint_resolution_mode
{
    char id[32]; // 0x00
    char language[6]; // 0x20
    int hres; // 0x28
    int vres; // 0x2C
    float aspect_ratio; // 0x30
};

// size 0xC0
struct vint_project_font
{
    char alias[64]; // 0x00
    char base_font_name[64]; // 0x40
    char overlay_font_name[64]; // 0x80
};

// size 0x940
struct vint_project_settings
{
    char data_directory[6][128]; // 0x00
    char peg_name[6][64]; // 0x300
    char bmp_sheet_peg_name[64]; // 0x480
    vint_project_font fonts[6]; // 0x4C0
};

extern vint_project_settings Vint_project_settings; // 0x01A47B98
extern int Vint_current_resolution_mode; // 0x014F5AC0
extern int Num_document_lookup_entries; // 0x01A48720
extern int Num_document_lookup_entries_mark; // 0x01A48724, restored by vint_restore_document_lookup_entries

int vint_project_get_resolution_mode(); // 0x00465BB0 (:174)
bool vint_project_set_resolution_mode(int _mode); // 0x00465BC0 (:179)
const vint_resolution_mode* get_project_get_resolution_mode_info(int _mode); // 0x00465BE0 (:189)
void vint_project_remove_std_extension(char* _s); // 0x00465C00 (:366)
bool vint_parse_project(const char* _name, mempool_base* _mempool); // 0x004732C0 (:380)
bool vint_reparse_project(const char* _name, mempool_base* _mempool); // 0x004733B0 (:420)
void vint_restore_document_lookup_entries(); // 0x00465CA0 (:455)
bool vint_save_project(const char* _name, mempool* xml_mempool); // 0x0046CBE0 (:461)
int vint_project_get_num_resolution_modes(); // 0x00465CB0 (:600)
void vint_project_get_resolution_mode(int& _hres, int& _vres, int _index); // 0x00465CC0 (:606)
const char* vint_project_get_resolution_mode_surname(int _index); // 0x00465D00 (:617)
int vint_project_find_resolution_mode(const char* _surname); // 0x00465D20 (:628)
bool vint_project_add_resolution_mode(const char* _id, const char* _language, int _hres, int _vres); // 0x00465D80 (:645)
bool vint_project_set_resolution_mode_surname(int _index, const char* _surname); // 0x00465E20 (:665)
bool vint_project_delete_resolution_mode(int _index); // 0x00465E60 (:679)
bool vint_project_swap_resolutions(int _index1, int _index2); // 0x00465F40 (:709)
int vint_project_get_num_filenames(); // 0x00466070 (:746)
int vint_project_get_num_dlc_filenames(); // 0x00466080 (:752)
const char* vint_project_get_doc_filename(int idx); // 0x00466090 (:758)
const char* vint_project_get_doc_name(int idx); // 0x004660C0 (:777)
const char* vint_project_get_doc_filename(const char* _name); // 0x004660F0 (:796)
bool vint_project_add_doc_filename(const char* _filename, const char* _doc_alias); // 0x00466130 (:819)
bool vint_project_has_filename(const char* _filename); // 0x00466220 (:861)
int vint_project_find_best_res_mode(const vector2i& _screen_dims, const char* _language_name); // 0x004662E0 (:883)
bool vint_project_settings_add_dir_name(const char* dir_name); // 0x0046CFD0 (:985)
bool vint_project_settings_add_font_info(const char* font_name, const char* font_alias, const char* font_overlay); // 0x0046D030 (:1006)
bool vint_project_settings_add_bmp_peg(const char* bmp_name); // 0x0046D0D0 (:1035)
bool vint_project_settings_add_bmp_sheet(const char* sheet_name); // 0x00466720 (:1057)
bool vint_project_settings_has_dir(const char* dir); // 0x00466750 (:1069)
const vint_project_font* vint_project_settings_find_font(const char* alias); // 0x004667A0 (:1094)
bool vint_project_settings_has_bmp_peg(const char* peg); // 0x004667F0 (:1114)
bool vint_project_settings_has_bmp_sheet(const char* sheet); // 0x00466840 (:1139)
unsigned int vint_project_settings_get_num_dirs(); // 0x00466870 (:1158)
unsigned int vint_project_settings_get_num_fonts(); // 0x00466890 (:1173)
unsigned int vint_project_settings_get_num_bmp_pegs(); // 0x004668B0 (:1188)
unsigned int vint_project_settings_get_num_bmp_sheets(); // 0x004668D0 (:1203)
void vint_project_clear_settings(); // 0x004668E0 (:1215)
vector2 vint_project_get_scale(vint_res_scale scale_mode_x, vint_res_scale scale_mode_y); // 0x00466940 (:1229)
vector2 vint_project_get_offset(vint_res_offset offset_mode_x, vint_res_offset offset_mode_y); // 0x00466960 (:1234)
