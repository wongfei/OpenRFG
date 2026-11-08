// Original: volition/vlib/code/util/string_pool.cpp
//
// string_pool over a mempool buffer (decompiled). Narrow strings are stored with their NUL and padded to an even size
// with one more NUL (one or two padding bytes, so the size is len + 2 for an even len and len + 3 for an odd one); the
// padding also leaves a double NUL after the last string. get_next and add_unique walk the strings with that size.
// Wide strings are stored back to back with their NUL and no padding.
//
// string_hash_table indexes the strings with num_hash_table_entries buckets; each bucket is a self-relative pointer to
// an array of hash_entry records. The first record's list_size holds the array capacity, and the list ends at the
// first record whose value is -1 (add_string grows the array while one free record is left). Strings are matched
// case-insensitively. A loaded table (load) points into the loaded image; a dynamic one (create_empty) allocates its
// pool and bucket arrays from mempool_to_use.
//
// Partial: save, save_endian_safe, find_string_ptr, estimate_maximum_memory_usage and hash_entry's members are not
// implemented.
//
// Notes (decompiled behaviour, kept as is):
// - The capacity check of the wide add/add_unique counts characters, not bytes (current_pos + 1 + len against
//   max_size), so a wide string can be written past the end of the pool.
// - clear() rewinds through the virtual restore_to_mark, which for string_pool goes back to mark_pos, not to 0.
// - add_string grows the string pool when space_free (end_pos - current_pos) is short, but only max_size and pool
//   change: end_pos stays, so every later add_string reallocates the pool again (the old pools are not freed, and the
//   hash entries of the strings added before still point into them).

#include "volition/vlib/code/util/string_pool.h"
#include "volition/vlib/code/util/string_comp.h"

#include "keen/base/string.hpp"
#include "keen/base/trace.hpp"

#include <string.h>
#include <wchar.h>

// Bytes taken by a narrow string of the given length: the NUL plus one or two padding NULs up to an even size.
static unsigned int string_pool_entry_size(unsigned int len)
{
    return len + ((len & 1) != 0 ? 3 : 2);
}

// 0x0045AFF0 (.h:34)
string_pool::string_pool(char* address, unsigned int size, const char* name)
{
    init(address, size, name, 0, 0);
}

// 0x00592850 (:42)
string_pool::string_pool(const char* name)
    : mempool(name)
{
    flags = 0;
    max_size = 0;
    mark_pos = 0;
    current_pos = 0;
    number_of_strings = 0;
    mark_number_of_strings = 0;
    pool = nullptr;
}

// 0x0045B0D0
string_pool::~string_pool()
{
}

// 0x00592880 (:58)
void string_pool::init(const void* address, unsigned int size, const char* namev, unsigned int unused_one, unsigned char unused_two)
{
    if (namev == nullptr)
    {
        namev = "string_pool";
    }
    init_static_mempool(address, size, namev, 1, 0);
    end_pos = size;
    number_of_strings = 0;
    mark_number_of_strings = 0;
}

// 0x005928C0 (:76)
void string_pool::restore_from_mem(char* address, unsigned int size, int num_strings, const char* namev)
{
    if (namev == nullptr)
    {
        namev = "string_pool";
    }
    init_static_mempool(address, size, namev, 1, 0);
    end_pos = size;
    current_pos = size;
    number_of_strings = num_strings;
    mark_number_of_strings = 0;
}

// 0x00592900 (:87)
void string_pool::reset_mark()
{
    mark_number_of_strings = 0;
    mark_pos = 0;
}

// 0x00563BC0 (.h:45)
char* string_pool::strings() const
{
    return (char*)pool;
}

// 0x005B21B0 (:168)
wchar_t* string_pool::add(const wchar_t* s)
{
    if (!assert_permission())
    {
        keen::debug::nativePrintArgs("No permissions in string pool.");
        return nullptr;
    }
    const unsigned int len = (unsigned int)wcslen(s);
    // Counts characters, not bytes (see the notes above).
    if (max_size < current_pos + 1 + len)
    {
        keen::debug::nativePrintArgs("Length of string pool exceeded.");
        return nullptr;
    }
    wchar_t* dst = (wchar_t*)(pool + current_pos);
    memcpy(dst, s, (len + 1) * sizeof(wchar_t));
    number_of_strings++;
    current_pos = current_pos + 2 + len * 2;
    return dst;
}

// 0x005B20F0 (:118)
char* string_pool::add(const char* s)
{
    if (!assert_permission())
    {
        keen::debug::nativePrintArgs("No permissions in string pool.");
        return nullptr;
    }
    const unsigned int len = (unsigned int)strlen(s);
    if (max_size < current_pos + string_pool_entry_size(len))
    {
        keen::debug::nativePrintArgs("Length of string pool exceeded.");
        return nullptr;
    }
    char* dst = (char*)(pool + current_pos);
    memcpy(dst, s, len + 1);
    current_pos = current_pos + 1 + len;
    if ((current_pos & 1) != 0)
    {
        pool[current_pos - 1] = 0;
        pool[current_pos] = 0;
        current_pos = current_pos + 1;
    }
    else
    {
        pool[current_pos] = 0;
        pool[current_pos + 1] = 0;
        current_pos = current_pos + 2;
    }
    number_of_strings++;
    return dst;
}

// 0x005B2250 (:200)
wchar_t* string_pool::add_unique(const wchar_t* s)
{
    wchar_t* cur = (wchar_t*)pool;
    for (int i = 0; i < number_of_strings; i++)
    {
        if (_v_stricmp(cur, s) == 0)
        {
            return cur;
        }
        cur += wcslen(cur) + 1;
    }
    return add(s);
}

// 0x005B2330 (:228)
char* string_pool::add_unique(const char* s)
{
    char* cur = (char*)pool;
    for (int i = 0; i < number_of_strings; i++)
    {
        if (keen::compareStringNoCase(cur, s) == 0)
        {
            return cur;
        }
        cur += string_pool_entry_size((unsigned int)strlen(cur));
    }
    return add(s);
}

// 0x005B2440 (:258)
char* string_pool::add_unique_case_sensitive(const char* s)
{
    char* cur = (char*)pool;
    for (int i = 0; i < number_of_strings; i++)
    {
        if (keen::isStringEqual(cur, s))
        {
            return cur;
        }
        cur += string_pool_entry_size((unsigned int)strlen(cur));
    }
    return add(s);
}

// 0x00592910 (:291)
const char* string_pool::get_first()
{
    return (const char*)pool;
}

// 0x00592930 (:308)
const char* string_pool::get_next(const char* cur_str_pointer)
{
    const char* begin = (const char*)pool;
    const char* end = begin + max_size;
    if (cur_str_pointer < begin || cur_str_pointer >= end)
    {
        return nullptr;
    }
    const char* next = cur_str_pointer + string_pool_entry_size((unsigned int)strlen(cur_str_pointer));
    return next < end ? next : nullptr;
}

// 0x00592920 (:297)
const wchar_t* string_pool::wget_first()
{
    return (const wchar_t*)pool;
}

// 0x00592990 (:334)
const wchar_t* string_pool::wget_next(const wchar_t* cur_str_pointer)
{
    const wchar_t* begin = (const wchar_t*)pool;
    const wchar_t* end = (const wchar_t*)(pool + max_size);
    if (cur_str_pointer < begin || cur_str_pointer >= end)
    {
        return nullptr;
    }
    const wchar_t* next = cur_str_pointer + wcslen(cur_str_pointer) + 1;
    // The wide pool has no end marker: an empty string ends the iteration.
    if (next < begin || next >= end || wcslen(next) == 0)
    {
        return nullptr;
    }
    return next;
}

// 0x005B20B0 (:95)
bool string_pool::clear()
{
    if (!assert_permission())
    {
        return false;
    }
    if (restore_to_mark(0))
    {
        restore_to_mark_at_end(max_size);
    }
    number_of_strings = 0;
    mark_number_of_strings = 0;
    return true;
}

// 0x005929F0 (:353)
unsigned int string_pool::mark()
{
    mark_number_of_strings = number_of_strings;
    mark_pos = current_pos;
    return mark_pos;
}

// 0x005B2550 (:364)
bool string_pool::restore_to_mark(unsigned int ignored)
{
    const unsigned int pos = mark_pos;
    if (assert_permission())
    {
        current_pos = pos;
    }
    number_of_strings = mark_number_of_strings;
    return true;
}

// 0x00592A00 (:378)
void string_pool::set_thread_ownership()
{
}

// 0x0045B090 (.h:73)
unsigned int string_pool::space_used() const
{
    return max_size - space_free();
}

// 0x0045B0B0 (.h:74)
unsigned int string_pool::space_free() const
{
    return end_pos - current_pos;
}

// 0x0045B0C0 (.h:75)
unsigned int string_pool::space_max() const
{
    return max_size;
}

// ---- string_hash_table

// Magic numbers: a new bucket array has 16 records (0x80 bytes), and a full one grows by 16 records.
static const int STRING_HASH_BUCKET_RECORDS = 0x10;

// The bucket's record array (nullptr when the bucket is empty).
static hash_entry* string_hash_bucket(et_ptr_offset<hash_entry,1>& bucket)
{
    return bucket.to_pointer();
}

// 0x005B2570 (:427)
string_hash_table::string_hash_table(const char* name)
    : string_pool(name)
{
    dynamic = false;
    num_hash_table_entries = 0;
    hash_table = nullptr;
    mempool_to_use = nullptr;
}

// 0x00597540
string_hash_table::~string_hash_table()
{
}

// 0x00563BD0 (.h:111)
unsigned int string_hash_table::get_index(unsigned short hash_val)
{
    return (unsigned int)hash_val % (unsigned int)num_hash_table_entries;
}

// 0x00592A10 (:398)
void* string_hash_table::alloc_array(unsigned int array_size, unsigned int element_size, unsigned int alignment)
{
    return mempool_to_use->alloc(array_size * element_size, alignment);
}

// 0x00592A30 (:441)
// Rotate left by 6 and xor each character (A-Z lowered; other characters sign-extended); 0xFFFF for nullptr.
unsigned short string_hash_table::hash_string(const char* str)
{
    if (str == nullptr)
    {
        return 0xFFFF;
    }
    unsigned int hash = 0;
    for (; *str != 0; str++)
    {
        unsigned int c = (unsigned int)(int)*str;
        if (c - 'A' < 26)
        {
            c += 0x20;
        }
        hash = (hash << 6 | hash >> 26) ^ c;
    }
    return (unsigned short)hash;
}

// 0x005BFA70 (:472)
short string_hash_table::find_string(const char* str)
{
    if (num_hash_table_entries < 1)
    {
        return -1;
    }
    const unsigned int index = get_index(hash_string(str));
    hash_entry* records = string_hash_bucket(hash_table[index]);
    if (records == nullptr)
    {
        return -1;
    }
    for (int i = 0;; i++)
    {
        if (records[i].value == -1)
        {
            return -1;
        }
        if (keen::compareStringNoCase(records[i].str.to_pointer(), str) == 0)
        {
            return records[i].value;
        }
    }
}

// 0x005C90B0 (:653)
// Image layout: max_size, current_pos, the 32-byte pool name, mark_pos, def_align (16 bits), flags (8 bits), the
// string pool, pad to 4, number_of_strings, mark_number_of_strings, num_hash_table_entries, the bucket table (record
// counts) and then each bucket's records (string offsets relative to the pool, made self-relative here).
void string_hash_table::load(const unsigned char* data, int& offset)
{
    dynamic = false;
    max_size = *(const unsigned int*)(data + offset);
    current_pos = *(const unsigned int*)(data + offset + 4);
    end_pos = current_pos;
    offset += 8;
    memcpy(name, data + offset, sizeof(name));
    offset += sizeof(name);
    mark_pos = *(const unsigned int*)(data + offset);
    offset += 4;
    def_align = *(const unsigned short*)(data + offset);
    offset += 2;
    flags = data[offset];
    offset += 1;
    pool = (unsigned char*)(data + offset);
    offset += max_size;
    if ((offset & 3) != 0)
    {
        offset += 4 - (offset & 3);
    }
    number_of_strings = *(const int*)(data + offset);
    mark_number_of_strings = *(const int*)(data + offset + 4);
    offset += 8;
    num_hash_table_entries = *(const int*)(data + offset);
    offset += 4;
    hash_table = (et_ptr_offset<hash_entry,1>*)(data + offset);
    offset += num_hash_table_entries * 4;
    for (int i = 0; i < num_hash_table_entries; i++)
    {
        const int num_records = hash_table[i].get_offset();
        if (num_records == 0)
        {
            continue;
        }
        hash_table[i].from_pointer((hash_entry*)(data + offset));
        offset += num_records * (int)sizeof(hash_entry);
        hash_entry* records = string_hash_bucket(hash_table[i]);
        for (int j = 0; j < num_records; j++)
        {
            const int pool_offset = records[j].str.get_offset();
            if (pool_offset != -1)
            {
                records[j].str.set_offset(pool_offset - (int)((unsigned char*)&records[j] - pool));
            }
        }
    }
}

// 0x005C9250 (:710)
void string_hash_table::create_empty(int hash_table_size, mempool_base* _mempool_to_use, int string_pool_size)
{
    dynamic = true;
    mempool_to_use = _mempool_to_use;
    if (string_pool_size < 0)
    {
        string_pool_size = 0x32000; // magic number: the default string pool size
    }
    initial_string_pool_size = string_pool_size;
    void* mem = _mempool_to_use->alloc(string_pool_size, 4);
    init_static_mempool(mem, string_pool_size, "string_hash_table", 1, 0);
    end_pos = string_pool_size;
    number_of_strings = 0;
    mark_number_of_strings = 0;
    hash_table = (et_ptr_offset<hash_entry,1>*)mempool_to_use->alloc(hash_table_size * 4, 4);
    num_hash_table_entries = hash_table_size;
    for (int i = 0; i < num_hash_table_entries; i++)
    {
        hash_table[i].set_offset(-1);
    }
}

// 0x005C92E0 (:744)
char* string_hash_table::add_string(const char* str, short user_data)
{
    if (space_free() < strlen(str) + 1)
    {
        // Grow the pool (see the file comment).
        int grow_size = initial_string_pool_size;
        if (grow_size > 0x31FFF)
        {
            grow_size = 0x32000;
        }
        const unsigned int old_max = space_max();
        unsigned char* old_pool = pool;
        unsigned char* new_pool = (unsigned char*)mempool_to_use->alloc(grow_size + old_max, 0);
        memcpy(new_pool, old_pool, space_used());
        max_size = grow_size + old_max;
        pool = new_pool;
    }
    char* added = string_pool::add(str);
    const unsigned int index = get_index(hash_string(str));
    et_ptr_offset<hash_entry,1>& bucket = hash_table[index];
    if (string_hash_bucket(bucket) == nullptr)
    {
        bucket.from_pointer((hash_entry*)mempool_to_use->alloc(STRING_HASH_BUCKET_RECORDS * sizeof(hash_entry), 1));
        hash_entry* records = string_hash_bucket(bucket);
        if (records == nullptr)
        {
            return nullptr;
        }
        records[0].list_size = STRING_HASH_BUCKET_RECORDS;
        for (int i = 0; i < STRING_HASH_BUCKET_RECORDS; i++)
        {
            records[i].value = -1;
        }
    }
    int slot = 0;
    while (string_hash_bucket(bucket)[slot].value != -1)
    {
        slot++;
        hash_entry* records = string_hash_bucket(bucket);
        if (records[0].list_size - 1 <= slot)
        {
            // Grow to slot + 16 records: copy the old records (rebasing their self-relative string offsets), mark
            // the new ones free and store the new capacity in the first record.
            const int new_capacity = slot + STRING_HASH_BUCKET_RECORDS;
            hash_entry* grown = (hash_entry*)mempool_to_use->alloc(new_capacity * sizeof(hash_entry), 0);
            for (short i = 0; i < records[0].list_size; i++)
            {
                grown[i].str.set_offset(records[i].str.get_offset() + (int)((char*)&records[i] - (char*)&grown[i]));
                grown[i].value = records[i].value;
                grown[i].list_size = records[i].list_size;
            }
            for (int i = slot; i < new_capacity; i++)
            {
                grown[i].value = -1;
            }
            bucket.from_pointer(grown);
            if (string_hash_bucket(bucket) != nullptr)
            {
                string_hash_bucket(bucket)[0].list_size = (short)new_capacity;
            }
        }
    }
    hash_entry& record = string_hash_bucket(bucket)[slot];
    record.str.from_pointer(added);
    record.value = user_data;
    return added;
}

// 0x00563BE0 (.h:136)
char* string_hash_table::strings() const
{
    return (char*)pool;
}

// 0x00563BF0 (.h:143)
void string_hash_table::set_thread_ownership()
{
}
