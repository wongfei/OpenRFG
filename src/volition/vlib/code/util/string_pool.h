// Original: volition/vlib/code/util/string_pool.h
// Implementation: volition/vlib/code/util/string_pool.cpp
//
// string_pool: a mempool that stores NUL-terminated strings back to back (add, add_unique, iteration with
// get_first/get_next). string_hash_table adds a hash index over the pool (entries of et_ptr_offset to the string plus a
// 16-bit user value), and can be saved to and loaded from a cfile image.
// Placement of hash_entry (string_hash_table's entry) inferred.
#pragma once

#include "volition/vlib/code/memory/vmemory.h"
#include "volition/vlib/code/util/endian_type.h"

struct cfile;

// size 0x5C
class string_pool : public mempool
{
public:
    string_pool(char* address, unsigned int size, const char* name); // 0x0045AFF0 (.h:34)
    string_pool(const char* name); // 0x00592850 (:42)
    virtual void init(const void* address, unsigned int size, const char* namev, unsigned int unused_one, unsigned char unused_two); // vtbl+0x50 0x00592880 (:58)
    void restore_from_mem(char* address, unsigned int size, int num_strings, const char* namev); // 0x005928C0 (:76)
    void reset_mark(); // 0x00592900 (:87)
    char* strings() const; // 0x00563BC0 (.h:45)
    wchar_t* add(const wchar_t* s); // 0x005B21B0 (:168)
    char* add(const char* s); // 0x005B20F0 (:118)
    wchar_t* add_unique(const wchar_t* s); // 0x005B2250 (:200)
    char* add_unique(const char* s); // 0x005B2330 (:228)
    char* add_unique_case_sensitive(const char* s); // 0x005B2440 (:258)
    const char* get_first(); // 0x00592910 (:291)
    const char* get_next(const char* cur_str_pointer); // 0x00592930 (:308)
    const wchar_t* wget_first(); // 0x00592920 (:297)
    const wchar_t* wget_next(const wchar_t* cur_str_pointer); // 0x00592990 (:334)
    int get_number_of_strings();
    virtual bool clear(); // vtbl+0x28 0x005B20B0 (:95)
    virtual unsigned int mark(); // vtbl+0x30 0x005929F0 (:353)
    virtual bool restore_to_mark(unsigned int ignored); // vtbl+0x34 0x005B2550 (:364)
    virtual void set_thread_ownership(); // vtbl+0x00 0x00592A00 (:378)
    virtual unsigned int space_used() const; // vtbl+0x10 0x0045B090 (.h:73)
    virtual unsigned int space_free() const; // vtbl+0x0C 0x0045B0B0 (.h:74)
    virtual unsigned int space_max() const; // vtbl+0x14 0x0045B0C0 (.h:75)
    virtual ~string_pool(); // vtbl+0x40 0x0045B0D0

protected:
    int number_of_strings; // 0x50
    int mark_number_of_strings; // 0x54
    unsigned int mark_pos; // 0x58
};

// size 0x8
struct hash_entry
{
    hash_entry(); // 0x005B25E0
    hash_entry& operator=(const hash_entry& __that); // 0x005C8E70

    et_ptr_offset<char,0> str; // 0x00
    short value; // 0x04
    short list_size; // 0x06
};

// size 0x70
class string_hash_table : public string_pool
{
protected:
    unsigned int get_index(unsigned short hash_val); // 0x00563BD0 (.h:111)
    void* alloc_array(unsigned int array_size, unsigned int element_size, unsigned int alignment); // 0x00592A10 (:398)
public:
    string_hash_table(const char* name); // 0x005B2570 (:427)
    static unsigned short hash_string(const char* str); // 0x00592A30 (:441)
    short find_string(const char* str); // 0x005BFA70 (:472)
    char* find_string_ptr(const char* str); // 0x005BFB40 (:505)
    void save(cfile* fp); // 0x005CD9B0 (:532)
    void save_endian_safe(cfile* fp); // 0x005C8EA0 (:591)
    void load(const unsigned char* data, int& offset); // 0x005C90B0 (:653)
    char* strings() const; // 0x00563BE0 (.h:136)
    void create_empty(int hash_table_size, mempool_base* _mempool_to_use, int string_pool_size); // 0x005C9250 (:710)
    char* add_string(const char* str, short user_data); // 0x005C92E0 (:744)
    unsigned int estimate_maximum_memory_usage(int hash_table_size, int string_pool_size); // 0x00592A70 (:830)
    virtual void set_thread_ownership(); // vtbl+0x00 0x00563BF0 (.h:143)
    virtual ~string_hash_table(); // vtbl+0x40 0x00597540

protected:
    bool dynamic; // 0x5C
    int num_hash_table_entries; // 0x60
    et_ptr_offset<hash_entry,1>* hash_table; // 0x64
    mempool_base* mempool_to_use; // 0x68
    int initial_string_pool_size; // 0x6C
};
