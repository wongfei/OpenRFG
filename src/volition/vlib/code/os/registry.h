// Original: volition/vlib/code/os/registry.h
// Implementation: volition/vlib/code/os/registry.cpp
//
// Windows registry access for settings.
#pragma once

void registry_parse_key(char* full_path, const char* key); // 0x005ADA80 (:45)
int registry_write(const char* key, const char* name, int reg_flags, unsigned char* data, int size); // 0x005ADBE0 (:124)
int registry_init(const char* company, const char* appname); // 0x0058B6C0 (:177)
int registry_read_string(const char* key, const char* name, char* result); // 0x005ADCC0 (:208)
int registry_read_uint(const char* key, const char* name, unsigned int* value, unsigned int def_value); // 0x005ADD50 (:230)
int registry_read_float(const char* key, const char* name, float* value, float def_value); // 0x005ADD80 (:250)
int registry_write_string(const char* key, const char* name, char* data); // 0x005ADDB0 (:269)
int registry_write_uint(const char* key, const char* name, unsigned int data); // 0x005ADDF0 (:283)
int registry_write_float(const char* key, const char* name, float data); // 0x005ADE10 (:297)
int registry_remove(const char* key, const char* name); // 0x005ADE30 (:310)
