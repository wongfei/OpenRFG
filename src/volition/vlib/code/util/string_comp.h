// Original: volition/vlib/code/util/string_comp.h
// Implementation: volition/vlib/code/util/string_comp.cpp
//
// Case-insensitive string comparison (v_stricmp, v_strnicmp).
#pragma once

int v_stricmp(const wchar_t* str1, const wchar_t* str2); // 0x0045AFD0 (.h:60)
int v_strnicmp(const wchar_t* str1, const wchar_t* str2, unsigned int n); // 0x0045AFE0 (.h:61)
int _v_stricmp(const char* str1, const char* str2); // 0x005926C0 (:70)
int _v_strnicmp(const char* str1, const char* str2, unsigned int n); // 0x00592720 (:90)
int _v_stricmp(const wchar_t* str1, const wchar_t* str2); // 0x00592770 (:112)
int _v_strnicmp(const wchar_t* str1, const wchar_t* str2, unsigned int n); // 0x005927D0 (:132)
int v_stricmp(const char* str1, const char* str2); // 0x00592830 (:162)
int v_strnicmp(const char* str1, const char* str2, unsigned int n); // 0x00592840 (:176)
