// Original: volition/vlib/code/util/string_util.h
// Implementation: volition/vlib/code/util/string_util.cpp
//
// String utilities: formatting numbers, case conversion, safe printf, hashing, static string buffers.
#pragma once

#include <stdint.h>

class color;
class vector;

bool str_blank(const char* text); // 0x00592AC0 (:56)
void str_ltrim(char* output, const char* text); // 0x00592B10 (:81)
void str_rtrim(char* output, const char* text); // 0x00592B70 (:105)
void str_trim(char* output, const char* text, int max_len); // 0x00592BD0 (:134)
void str_sprintf_rj(char*, int, int, const char*, const char*, ...); // 0x00592C60 (:189)
void str_sprintf_lj(char*, int, int, const char*, const char*, ...); // 0x00592DA0 (:249)
char* str_int_to_comma_delimited_string(char* output, int output_size, int number); // 0x00592EC0 (:298)
char* str_uint64_to_comma_delimited_string(char* output, int output_size, uint64_t number); // 0x00592FE0 (:360)
char* str_commas(uint64_t number); // 0x005930F0 (:408)
char* str_commas(unsigned int number); // 0x00593140 (:414)
char* str_commas(int number); // 0x00593190 (:420)
int str_to_int_commas(const char* str); // 0x005931F0 (:427)
bool str_is_number(const char* text); // 0x00593250 (:452)
void str_ccat(char* str, char c); // 0x00593390 (:533)
char* str_stri(const char* str, const char* substr); // 0x005933C0 (:548)
void str_make_lower(char* str); // 0x00593420 (:576)
void str_make_upper(char* str); // 0x00593460 (:590)
void str_copy_to_lower(char* dest, unsigned int dest_array_size, const char* source); // 0x005B25F0 (:606)
void str_copy_to_upper(char* dest, unsigned int dest_array_size, const char* source); // 0x005B2650 (:626)
void str_i2s(char* output, int max_size, int i); // 0x005934A0 (:646)
void str_i2s_hex(char* output, int max_size, int i); // 0x005934F0 (:662)
void str_f2s(char* output, int max_size, float f, int prec); // 0x00593540 (:679)
void str_size2s(char* output, int max_size, unsigned int size); // 0x005935A0 (:701)
void str_v2s(char* output, int max_size, const vector* v, int precision); // 0x00593670 (:730)
void str_x2s(char* output, int max_size, void* addr); // 0x00593710 (:748)
void str_color2s_hex(char* output, int max_size, const color& col, bool include_prefix); // 0x00593760 (:765)
unsigned int str_hash_string(const char* str); // 0x005937D0 (:786)
int _snprintf_safe(char*, unsigned int, const char*, ...); // 0x00593810 (:820)
int _vsnprintf_safe(char* dest, unsigned int dest_count, const char* format, char* arg_list); // 0x00593840 (:857)
char* strncpy_safe(char* dest, const char* src, unsigned int dest_count); // 0x00593870 (:884)
bool str_match(const char* name, const char* pattern); // 0x00593970 (:952)
bool str_matchi(const char* name, const char* pattern); // 0x00593990 (:958)
char* strcat_safe(char* dest, const char* src, unsigned int dest_count); // 0x005B26B0 (:973)
char* str_repeat(char c, int n); // 0x005939B0 (:997)
