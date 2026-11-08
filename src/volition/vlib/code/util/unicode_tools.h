// Original: volition/vlib/code/util/unicode_tools.h
// Implementation: volition/vlib/code/util/unicode_tools.cpp
//
// Unicode helpers: conversion between ASCII/UTF-8 and wide strings, code point iteration and glyph presentation.
#pragma once

#include <wchar.h>

// 0x00565A50 (.h:83)
// Always terminated (dst[sz - 1]); a size of 0 writes dst[-1] (original).
inline wchar_t* strncpy_safe(wchar_t* dst, const wchar_t* src, unsigned int sz)
{
    wcsncpy(dst, src, sz);
    dst[sz - 1] = L'\0';
    return dst;
}
wchar_t* copy_to_wide(wchar_t* dest, const wchar_t* source, unsigned int size); // 0x006F68B0 (.h:86)
wchar_t* copy_to_unicode(wchar_t* dest, const char* source, unsigned int size); // 0x00598260 (.h:87)
wchar_t* copy_to_unicode(wchar_t* dest, const wchar_t* source, unsigned int size); // 0x00883440 (.h:88)
char* strncpy_safe(char* dst, const wchar_t* src, unsigned int sz); // 0x00883470 (.h:95)
wchar_t* strncpy_safe(wchar_t* dst, const char* src, unsigned int sz); // 0x0045E5C0 (.h:96)
// 0x0045E5D0 (.h:106)
inline wchar_t* strcpy(wchar_t* d, const wchar_t* s)
{
    return wcscpy(d, s);
}
wchar_t* strncpy(wchar_t* dst, const wchar_t* src, unsigned int sz); // 0x00565A80 (.h:107)
// 0x00753A00 (.h:109): no bound on a (original).
inline void strcat(wchar_t* a, const wchar_t* b)
{
    wcscat(a, b);
}
void strncat(wchar_t* a, const wchar_t* b, unsigned int s); // 0x0083F510 (.h:110)
int strcmp(const wchar_t* a, const wchar_t* b); // 0x00883480 (.h:112)
wchar_t* strchr(const wchar_t* a, wchar_t b); // 0x008834C0 (.h:115)
unsigned int strlen(const wchar_t* a); // 0x0045E600 (.h:117)
int _snprintf_safe(wchar_t*, unsigned int, const char*, ...); // 0x005B2700 (:159)
int _snwprintf_safe(wchar_t*, unsigned int, const wchar_t*, ...); // 0x00593A10 (:190)
int sprintf(wchar_t*, const wchar_t*, ...); // 0x00593A40 (:212)
int _snprintf_safe(wchar_t*, unsigned int, const wchar_t*, ...); // 0x00593A60 (:231)
const wchar_t* str_copy_to_upper(wchar_t* dest, unsigned int size, const wchar_t* source); // 0x005B27C0 (:254)
char* copy_to_ascii(char* dest, const wchar_t* source, unsigned int size); // 0x00593A90 (:263)
void str_int_to_comma_delimited_string(wchar_t* output, unsigned int size, int number); // 0x00593AD0 (:278)
bool unicode_insert_no_break_spaces(wchar_t* string, unsigned int size); // 0x00593C10 (:337)
wchar_t* copy_to_wide(wchar_t* dest, const char* source, unsigned int size); // 0x00593C60 (:357)
int strcmp(const wchar_t* const lhs, const char* const rhs); // 0x00593CA0 (:372)
void strupr(wchar_t* str_src); // 0x00593D80 (:385)
unsigned int localization_get_next_presentation_glyph_utf16(const wchar_t* string, unsigned int previous_char, unsigned int& out_char); // 0x00593DC0 (:556)
// template instance: unsigned int localization_get_next_code_point_from_string<wchar_t>(const wchar_t* string, unsigned int& out_char); // 0x00565A90 (unicode_tools.inl:4)
// template instance: unsigned int localization_get_next_code_point_from_string<char>(const char* string, unsigned int& out_char); // 0x00565AE0 (unicode_tools.inl:23)
// template instance: unsigned int localization_get_next_presentation_glyph<wchar_t>(const wchar_t* string, unsigned int previous_char, unsigned int& out_char); // 0x0045E620 (unicode_tools.inl:34)
// template instance: unsigned int localization_get_next_presentation_glyph<char>(const char* string, unsigned int previous_char, unsigned int& out_char); // 0x00565B00 (unicode_tools.inl:40)
