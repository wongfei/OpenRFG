// Original: volition/vlib/code/parse/xml_table_tools.h
// Implementation: volition/vlib/code/parse/xml_table_tools.cpp
//
// Readers for the .xtbl tables (decompiled): xml_table_parse(name) parses the file into a fresh virtual_mempool from
// Xml_table_page_pool (xml_set_page_pool, set by rfg_pre_initialize_game) and returns the <Table> element;
// xml_table_close releases the pool again. The _req getters log "Could not find required attribute" style errors, the
// _opt getters return false. Values are the text of a child element named attribute_name; lists are <Element>
// children, flags are <Flag> children, vectors have <X>/<Y>/<Z>, matrices <Rvec>/<Uvec>/<Fvec> or <Heading>, colors
// <R>/<G>/<B>, file names a <Filename> child.
#pragma once

#include "volition/vlib/code/util/fastxml.h"

class page_pool;
class vector;
class matrix;
class color;

void xml_set_page_pool(page_pool* pPool); // 0x0058E530 (:23)
bool parse_bool(const char* buf, unsigned int buf_size); // 0x005B1110 (:64)
unsigned int parse_uint(char* buf, int buf_size); // 0x005B11B0 (:94)
int parse_int(char* buf, int buf_size); // 0x005B12A0 (:147)
float parse_float(char* buf, int buf_size); // 0x005B12D0 (:185)
xml_element* xml_table_parse(const char* filename); // 0x005CFB60 (:277)
xml_element* xml_table_parse_from_memory(unsigned char* file_loc); // 0x005C5A60 (:328)
void xml_table_close(xml_element* root); // 0x005B1480 (:376)
xml_element* xml_table_find_req(xml_element* root, const char* attribute_name); // 0x005BE380 (:408)
xml_element* xml_table_find_opt(xml_element* root, const char* attribute_name); // 0x005BE410 (:433)
xml_element* xml_table_find_next(xml_element* root, xml_element* previous, const char* attribute_name); // 0x005BE450 (:453)
int xml_table_count(xml_element* root, const char* name); // 0x005BE490 (:465)
char* xml_table_get_req_string_ref(xml_element* root, const char* attribute_name); // 0x005BE4A0 (:482)
char* xml_table_get_opt_string_ref(xml_element* root, const char* attribute_name); // 0x005BE550 (:535)
void xml_table_get_req_string(char* buf, int buf_size, xml_element* root, const char* attribute_name); // 0x005BE580 (:572)
bool xml_table_get_opt_string(char* buf, int buf_size, xml_element* root, const char* attribute_name); // 0x005BE680 (:631)
void xml_table_get_req_int(int& value, xml_element* root, const char* attribute_name); // 0x005BE6D0 (:667)
bool xml_table_get_opt_int(int& value, xml_element* root, const char* attribute_name); // 0x005BE760 (:688)
void xml_table_get_req_uint(unsigned int& value, xml_element* root, const char* attribute_name); // 0x005BE830 (:712)
bool xml_table_get_opt_uint(unsigned int& value, xml_element* root, const char* attribute_name); // 0x005BE8A0 (:733)
void xml_table_get_req_float(float& value, xml_element* root, const char* attribute_name); // 0x005BE950 (:757)
bool xml_table_get_opt_float(float& value, xml_element* root, const char* attribute_name); // 0x005BE9C0 (:779)
float xml_table_get_float(const xml_element* e); // 0x005B14A0 (:799)
void xml_table_get_req_bool(bool& value, xml_element* root, const char* attribute_name); // 0x005BEA70 (:816)
bool xml_table_get_opt_bool(bool& value, xml_element* root, const char* attribute_name); // 0x005BEAE0 (:838)
int xml_table_get_string_list(char* list, int max_string_len, int list_max, xml_element* root); // 0x005BEB90 (:863)
int xml_table_get_int_list(int* list, int list_max, bool (*func)(int&, int, const char*), xml_element* root); // 0x005BEC70 (:907)
int xml_table_get_float_list(float* list, int list_max, xml_element* root); // 0x005BEE20 (:964)
int xml_table_get_flags(const char* const* list, int list_size, xml_element* root, bool ignore_unknown_flags); // 0x005BEF10 (:1006)
int xml_table_get_type_from_list(const char* const* list, int list_size, xml_element* root); // 0x005BF080 (:1054)
void xml_table_get_vector(vector& vec, xml_element* root); // 0x005BF150 (:1080)
void xml_table_get_req_vector(vector& vec, xml_element* root, const char* const attribute_name); // 0x005BF210 (:1095)
void xml_table_get_matrix(matrix& mat, xml_element* root); // 0x005BF260 (:1116)
void xml_table_get_req_color(color& col, xml_element* root, const char* attribute_name); // 0x005C5AE0 (:1156)
bool xml_table_get_opt_color(color& col, xml_element* root, const char* attribute_name); // 0x005BF3E0 (:1173)
char* xml_table_get_req_filename_ref(xml_element* root, const char* attribute_name); // 0x005BF4C0 (:1219)
char* xml_table_get_opt_filename_ref(xml_element* root, const char* attribute_name); // 0x005BF500 (:1244)
void xml_table_get_req_filename(char* buf, int buf_size, xml_element* root, const char* attribute_name); // 0x005BF540 (:1277)
bool xml_table_get_opt_filename(char* buf, int buf_size, xml_element* root, const char* attribute_name); // 0x005BF580 (:1302)
