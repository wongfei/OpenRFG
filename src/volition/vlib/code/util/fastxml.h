// Original: volition/vlib/code/util/fastxml.h
// Implementation: volition/vlib/code/util/fastxml.cpp
//
// Minimal XML DOM used by every table (decompiled):
// - xml_element is a node: name, first child (elements), next sibling, text. Attributes do not exist; values are child
//   elements. Nodes and the file buffer are allocated from Xml_mempool (xml_system_init, or the dest argument), so a
//   whole document is freed by restoring or clearing that pool.
// - xml_parse reads the file (open_table_file) into the pool and parses it in place (xml_parse_from_string); entity and
//   numeric character references are decoded there. Errors go to Xml_parse_error.
// - Table precache: xml_enable_precache(packfile) loads a condensed packfile of tables (packfile_read_condensed) and
//   indexes every file in table_hash_table; while Xml_precaching_enabled, open_table_file serves a table from that
//   memory (cf_open over the buffer) unless the priority packfile has the file.
// - Xml_append_cache remembers the last child of recently appended parents so xml_append is not quadratic.
// Placement of preloaded_table_data and xml_append_cache inferred.
#pragma once

#include "volition/vlib/code/util/hash_table.h"

struct cfile;
struct v_packfile;
class mempool_base;
struct _iobuf;

// size 0x10
struct xml_element
{
    const char* name; // 0x00
    xml_element* next; // 0x04
    xml_element* elements; // 0x08
    char* text; // 0x0C
};

// size 0x8
struct preloaded_table_data
{
    unsigned char* data; // 0x00
    unsigned int size; // 0x04
};

// size 0x8
struct xml_append_cache
{
    xml_element* parent; // 0x00
    xml_element* last_child; // 0x04
};

extern char Xml_parse_error[512]; // 0x01CDA3B8
extern dynamic_string_hash_table<preloaded_table_data,64> table_hash_table; // 0x0163E9B8

xml_element* add_xml_text(xml_element* element, const char* text); // 0x005BF710 (:243)
bool write_xml_file(xml_element* element, unsigned int depth, cfile* p_cfile); // 0x0058F120 (:493)
void xml_system_init(mempool_base* dest); // 0x0058F430 (:615)
xml_element* xml_new(const char* tag, const char* text, xml_element* parent, mempool_base* dest); // 0x005B1840 (:635)
xml_element* xml_new_dup(const char* tag, const char* text, xml_element* parent, mempool_base* dest); // 0x005B18E0 (:688)
xml_element* xml_parse(const char* filename, mempool_base* dest); // 0x005CD3D0 (:737)
xml_element* xml_parse_from_string(char* buf, mempool_base* dest, const char* filename_orig); // 0x005BF960 (:830)
bool xml_write(const char* filename, xml_element* root); // 0x005CD510 (:894)
bool xml_write(_iobuf* f, xml_element* root, unsigned int depth); // 0x0058F590 (:957)
bool xml_write(cfile* p_cfile, xml_element* element, unsigned int depth); // 0x0058F700 (:1004)
xml_element* xml_find(const xml_element* element, const char* tag); // 0x005B19C0 (:1065)
xml_element* xml_find_next(xml_element* element, xml_element* cur, const char* tag); // 0x005B1A00 (:1093)
xml_element* xml_find_nth(xml_element* element, const char* tag, unsigned int index); // 0x005B1A40 (:1116)
bool xml_remove(xml_element* root, xml_element* element); // 0x0058F950 (:1147)
bool xml_append(xml_element* root, xml_element* element); // 0x0058F9B0 (:1246)
bool xml_insert(xml_element* root, xml_element* element); // 0x0058FA40 (:1320)
bool xml_insert_before(xml_element* root, xml_element* element, xml_element* element_to_insert); // 0x0058FA80 (:1356)
unsigned int xml_count(xml_element* element, const char* tag); // 0x005B1A90 (:1397)
unsigned int xml_checksum(xml_element* root, unsigned int accumulator); // 0x0058FAE0 (:1426)
xml_element* xml_clone(xml_element* root, mempool_base* dest); // 0x0058FB30 (:1457)
char* xml_get_string(xml_element* e, char* default_val); // 0x0058FB50 (:1474)
bool xml_get_bool(xml_element* e, bool default_val); // 0x005B1AD0 (:1487)
float xml_get_float(xml_element* e, float default_val); // 0x0058FB70 (:1504)
float xml_get_float_hex(xml_element* e, float default_val); // 0x0058FBA0 (:1517)
int xml_get_int(xml_element* e, int default_val); // 0x0058FBD0 (:1533)
unsigned int xml_get_uint(const xml_element* e, unsigned int default_val); // 0x0058FC00 (:1546)
unsigned int xml_get_flags(xml_element* e, const char** flags, unsigned int num_flags, unsigned int default_val); // 0x0058FC40 (:1566)
bool xml_clear_cache(); // 0x0058FD70 (:1623)
void xml_disable_precache(); // 0x0058FD90 (:1639)
bool xml_precaching_enabled(); // 0x0058FDA0 (:1644)
bool xml_enable_precache(const char* filename, mempool_base* table_preload_pool_ptr, mempool_base* temp_mempool, unsigned int max_table_files); // 0x005CFBE0 (:1649)
bool xml_enable_precache(v_packfile* pf, mempool_base* table_preload_pool_ptr, unsigned int max_table_files); // 0x005CD570 (:1665)
