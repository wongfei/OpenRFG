// Original: volition/vlib/code/util/xml_util.h
// Implementation: volition/vlib/code/util/xml_util.cpp
//
// Typed child-element readers and writers (read_xml_int(parent, "tag", val, default), write_xml_vector(f, ...)). The
// writers emit <tag>value</tag> lines through stdio (_iobuf = FILE). The *_hex readers take IEEE bit patterns written
// as 8 hex digits (read_float_from_string, hex_float), which round-trips floats exactly.
// Placement of hex_float inferred.
#pragma once

#include "volition/vlib/code/util/fastxml.h"

class vector;
class vector2;
class vector4;
class matrix;
class matrix43;
class quaternion;
struct bbox;
class plane;
struct _iobuf;

// size 0x4
union hex_float
{
    char bytes[4]; // 0x00
    float value; // 0x00
    unsigned int int_val; // 0x00
};

bool read_xml_int(const xml_element* parent, const char* tag_name, int& val, int default_val); // 0x005B27F0 (:100)
bool read_xml_int(const xml_element* e, int& val); // 0x005942F0 (:114)
bool write_xml_int(_iobuf* f, const char* tag_name, int val, const char* whitespace_prefix); // 0x00594320 (:153)
bool read_xml_float(const xml_element* parent, const char* tag_name, float& val, float default_val); // 0x005B2860 (:172)
bool read_xml_float(const xml_element* e, float& val); // 0x00594350 (:186)
bool write_xml_float(_iobuf* f, const char* tag_name, float val, const char* whitespace_prefix); // 0x00594380 (:226)
bool read_xml_bool(const xml_element* parent, const char* tag_name, bool& val, bool default_val); // 0x005B28E0 (:245)
bool read_xml_bool(const xml_element* e, bool& val); // 0x005943C0 (:259)
bool write_xml_bool(_iobuf* f, const char* tag_name, bool val, const char* whitespace_prefix); // 0x00594470 (:309)
bool read_xml_vector(const xml_element* parent, const char* tag_name, vector& val, const vector& default_val); // 0x005B2940 (:328)
bool read_xml_vector(const xml_element* e, vector& val); // 0x005944B0 (:342)
bool read_xml_vector2(const xml_element* parent, const char* tag_name, vector2& val, const vector2& default_val); // 0x005B29B0 (:366)
bool read_xml_vector2(const xml_element* e, vector2& val); // 0x00594580 (:380)
bool read_xml_vector4(const xml_element* parent, const char* tag_name, vector4& val, const vector4& default_val); // 0x005B2A10 (:402)
bool read_xml_vector4(const xml_element* e, vector4& val); // 0x00594630 (:416)
bool write_xml_vector(_iobuf* f, const char* tag_name, const vector& val, const char* whitespace_prefix); // 0x00594720 (:467)
bool write_xml_vector(_iobuf* f, const char* tag_name, const vector2& val, const char* whitespace_prefix); // 0x00594780 (:478)
bool write_xml_vector4(_iobuf* f, const char* tag_name, const vector4& val, const char* whitespace_prefix); // 0x005947D0 (:489)
bool read_xml_matrix(const xml_element* parent, const char* tag_name, matrix& val, const matrix& default_val); // 0x005B2A80 (:508)
bool read_xml_matrix(const xml_element* e, matrix& val); // 0x00594840 (:522)
bool write_xml_matrix(_iobuf* f, const char* tag_name, const matrix& val, const char* whitespace_prefix); // 0x00594970 (:590)
bool read_xml_bbox(const xml_element* parent, const char* tag_name, bbox& val, const bbox& default_val); // 0x005B2B10 (:619)
bool read_xml_bbox(const xml_element* e, bbox& val); // 0x00594A30 (:633)
bool write_xml_bbox(_iobuf* f, const char* tag_name, const bbox& val, const char* whitespace_prefix); // 0x00594B20 (:693)
bool read_xml_string(const xml_element* parent, const char* tag_name, char* val, int max_len, const char* default_val); // 0x005B2B90 (:720)
bool read_xml_string(const xml_element* e, char* val, int max_len); // 0x00594BB0 (:734)
bool write_xml_string(_iobuf* f, const char* tag_name, const char* val, const char* whitespace_prefix); // 0x00594BE0 (:775)
bool read_xml_transform(const xml_element* parent, const char* tag_name, matrix43& val, const matrix43& default_val); // 0x005C5BD0 (:918)
bool read_xml_transform(const xml_element* e, matrix43& val); // 0x005BFC80 (:933)
bool write_xml_transform(_iobuf* f, const char* tag_name, const matrix43& val, const char* whitespace_prefix); // 0x005BFE00 (:959)
bool read_xml_transform(const xml_element* parent, const char* tag_name, vector& pos, quaternion& orient); // 0x005B2C10 (:977)
bool read_xml_transform(const xml_element* e, vector& pos, quaternion& orient); // 0x00594C10 (:991)
bool read_xml_transform_hex(xml_element* parent, vector& position, quaternion& rotation); // 0x005C0260 (:1054)
bool read_xml_transform_hex(xml_element* parent, matrix43& trans); // 0x005C02F0 (:1078)
bool write_xml_transform(_iobuf* f, const char* tag_name, const vector& pos, const quaternion& orient, const char* whitespace_prefix); // 0x00594D40 (:1097)
bool read_xml_plane(const xml_element* parent, const char* tag_name, plane& val); // 0x005B2C70 (:1124)
bool read_xml_plane(const xml_element* e, plane& val); // 0x00594DE0 (:1138)
bool write_xml_plane(_iobuf* f, const char* tag_name, const plane& val, const char* whitespace_prefix); // 0x00594E90 (:1184)
hex_float read_float_from_string(const char* str); // 0x00594F00 (:1202)
int read_xml_flag(xml_element* parent, const char* elem_name, int flag); // 0x005B2CC0 (:1224)
bool read_xml_flag(xml_element* parent, const char* elem_name); // 0x005B2D10 (:1237)
bool read_xml_float_hex(xml_element* parent, float& f_out, const char* attribute_name); // 0x005B2D60 (:1255)
bool read_xml_vector_hex(vector& v_out, xml_element* parent, xml_element** previous); // 0x005B2E10 (:1291)
bool read_xml_vector_as_hex(vector& vec_out, xml_element* parent, const char* tag, xml_element** previous); // 0x005B2F00 (:1341)
bool read_xml_vector4_as_hex(vector4& vec_out, xml_element* parent, const char* tag, xml_element** previous); // 0x005B2FB0 (:1388)
bool read_xml_vector2_as_hex(vector2& vec_out, xml_element* parent, const char* tag, xml_element** previous); // 0x005B3070 (:1438)
bool read_xml_plane_as_hex(plane& plane_out, xml_element* parent, const char* tag, xml_element** previous); // 0x005B3110 (:1483)
bool read_xml_quaternion_hex(quaternion& q_out, xml_element* parent, const char* tag_name); // 0x005B31D0 (:1530)
void write_xml_flag(_iobuf* f, char* flag_name, int value); // 0x00595010 (:1557)
void write_xml_vector_as_hex(_iobuf* f, const vector& v, const char* whitespace_prefix, const char* tag); // 0x00595040 (:1563)
void write_xml_vector2_as_hex(_iobuf* f, const vector2& v, const char* whitespace_prefix, const char* tag); // 0x005950A0 (:1573)
void write_xml_plane_as_hex(_iobuf* f, const plane& p, const char* whitespace_prefix, const char* tag); // 0x005950F0 (:1581)
char* dup_xml_attr(xml_element* e, const char* attr_name); // 0x00595160 (:1595)
void copy_xml_attr(char* dest, xml_element* e, const char* attr_name); // 0x005B32B0 (:1608)
void make_tab_string(char* dest, int max_size, int num_tabs); // 0x005B3300 (:1619)
