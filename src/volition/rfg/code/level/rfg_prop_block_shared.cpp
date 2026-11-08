// Original: volition/rfg/code/level/rfg_prop_block_shared.cpp
//
// Property blocks (decompiled). A block that points into loaded data (zone files) is read-only (owner == 0):
// set_internal refuses to add to it. A block that starts empty allocates its records from Prop_data_pool (64 KB of
// 1 KB pages) and grows by realloc, or by allocate and copy when the run cannot grow in place. A record is the 8-byte
// obj_prop header and size bytes of data, padded to 4. Types: 4 string, 5 plain data (get<T>/set<T>, checked by
// size), 6 buffer.
//
// Notes (decompiled behaviour, kept as is):
// - get_buf_copy / get_buf_copy_optional copy 0 bytes from the prop_name when the property is missing.
// - set_flags_as_str ORs a stale value for a key whose lookup fails (cannot happen: the keys come from the table).
// - get_str_as_enum only formats its "Could not find object parameter" message into the thread's scratch buffer.

#include "volition/rfg/code/level/rfg_prop_block_shared.h"

#include "volition/vlib/code/cfile/cfile.h"
#include "volition/vlib/code/math/crc.h"
#include "volition/vlib/code/memory/page_mempool.h"

#include <string.h>

// ---- file-static globals
static unsigned char Prop_data_buf[65536]; // 0x02CBB678
static page_mempool Prop_data_pool(Prop_data_buf, sizeof(Prop_data_buf), 0x400, "prop block pool", 1, 0); // 0x02F9C7C8

// 0x00A05450 (rfg_prop_name_shared.h:27)
prop_name::prop_name(const char* _str)
{
    str = _str;
    crc_init();
    crc = crc_stri(str, 0);
}

// 0x0091A550 (:60)
rfg_prop_block::rfg_prop_block()
{
    num_props = 0;
    owner = 0;
    buf_pos = 0;
    buf_size = 0;
    prop_list = nullptr;
}

// 0x00921950 (:69)
rfg_prop_block::~rfg_prop_block()
{
    clear();
}

// 0x0091A560 (:76)
void rfg_prop_block::clear()
{
    num_props = 0;
    if (owner != 0 && prop_list != nullptr)
    {
        Prop_data_pool.release_bytes((void*)prop_list);
    }
    owner = 0;
    buf_pos = 0;
    buf_size = 0;
    prop_list = nullptr;
}

// 0x0091A590 (:90)
void rfg_prop_block::destructive_copy(rfg_prop_block& src)
{
    num_props = src.num_props;
    owner = src.owner;
    buf_pos = src.buf_pos;
    buf_size = src.buf_size;
    prop_list = src.prop_list;
    src.num_props = 0;
    src.owner = 0;
    src.buf_pos = 0;
    src.buf_size = 0;
    src.prop_list = nullptr;
}

// 0x0091A5D0 (:126)
obj_prop* rfg_prop_block::find_prop(const prop_name& name, const unsigned int name_crc) const
{
    obj_prop* prop = (obj_prop*)prop_list;
    for (int i = 0; i < (int)num_props; ++i)
    {
        if (name_crc == prop->name_crc)
        {
            return prop;
        }
        const unsigned int record_size = prop->size + 8;
        const unsigned int pad = (record_size & 3) == 0 ? 0 : 4 - (record_size & 3);
        prop = (obj_prop*)((unsigned char*)prop + record_size + pad);
    }
    return nullptr;
}

// 0x0091A620 (:173)
bool rfg_prop_block::get_internal(obj_prop*& prop_ref, const prop_name& name, int type) const
{
    prop_ref = find_prop(name, name.crc);
    if (prop_ref == nullptr)
    {
        return false;
    }
    return prop_ref->type == type;
}

// 0x0091A650 (:192)
bool rfg_prop_block::expand_buffer(int req)
{
    const int required = (int)buf_pos + req;
    if ((int)buf_size < required)
    {
        if (required > 0xFFFF)
        {
            return false;
        }
        unsigned char* mem = (unsigned char*)Prop_data_pool.realloc(prop_list, required);
        if (mem == nullptr)
        {
            mem = (unsigned char*)Prop_data_pool.alloc(required, 0);
            if (mem == nullptr)
            {
                return false;
            }
            if (prop_list != nullptr)
            {
                memcpy(mem, prop_list, buf_size);
                Prop_data_pool.release_bytes((void*)prop_list);
            }
        }
        prop_list = mem;
        buf_size = (unsigned short)Prop_data_pool.size(mem);
    }
    return true;
}

// 0x0091A6E0 (:229)
obj_prop* rfg_prop_block::set_internal(const prop_name& name, int size, int type)
{
    if (prop_list == nullptr)
    {
        buf_pos = 0;
        buf_size = 0;
        num_props = 0;
        owner = 1;
    }
    else if (owner == 0)
    {
        return nullptr;
    }
    obj_prop* prop = find_prop(name, name.crc);
    if (prop != nullptr)
    {
        // An existing property is reused only when its type and size match.
        if (prop->type == type && prop->size == size)
        {
            return prop;
        }
        return nullptr;
    }
    const int record_size = size + 8;
    const int pad = (record_size & 3) == 0 ? 0 : 4 - (record_size & 3);
    if (!expand_buffer(record_size + pad))
    {
        return nullptr;
    }
    prop = (obj_prop*)(prop_list + buf_pos);
    buf_pos += (unsigned short)(record_size + pad);
    prop->size = (unsigned short)size;
    prop->type = (unsigned short)type;
    prop->name_crc = name.crc;
    ++num_props;
    return prop;
}

// 0x0091A790 (:282)
bool rfg_prop_block::load(unsigned char* data)
{
    num_props = *(unsigned short*)data;
    const unsigned short size = *(unsigned short*)(data + 2);
    prop_list = data + 4;
    buf_pos = size;
    buf_size = size;
    return true;
}

// 0x0091A7C0 (:296)
bool rfg_prop_block::load(cfile* f)
{
    unsigned short* data = (unsigned short*)cf_memory_fp(f);
    num_props = data[0];
    const unsigned short size = data[1];
    prop_list = (unsigned char*)(data + 2);
    buf_pos = size;
    buf_size = size;
    return true;
}

// 0x0091A7F0 (:302)
bool rfg_prop_block::save(cfile* f) const
{
    cf_write_uint16(num_props, f);
    cf_write_uint16(buf_pos, f);
    if (num_props != 0)
    {
        cf_write(prop_list, buf_pos, f);
    }
    return true;
}

// 0x00921980 (:316)
void rfg_prop_block::get(const prop_name& name, char* str) const
{
    obj_prop* prop = find_prop(name, name.crc);
    if (prop != nullptr && prop->type == 4)
    {
        strcpy(str, (const char*)prop->data);
    }
}

// 0x0091A830 (:326)
bool rfg_prop_block::get_optional(const prop_name& name, char* str) const
{
    obj_prop* prop = find_prop(name, name.crc);
    if (prop != nullptr && prop->type == 4)
    {
        strcpy(str, (const char*)prop->data);
        return true;
    }
    return false;
}

// 0x0091A8C0 (:356)
void rfg_prop_block::set(const prop_name& name, char* str)
{
    obj_prop* prop = set_internal(name, (int)strlen(str) + 1, 4);
    if (prop != nullptr)
    {
        strcpy((char*)prop->data, str);
    }
}

// 0x0091A870 (:343)
void rfg_prop_block::set(const prop_name& name, const char* str)
{
    obj_prop* prop = set_internal(name, (int)strlen(str) + 1, 4);
    if (prop != nullptr)
    {
        strcpy((char*)prop->data, str);
    }
}

// 0x009219B0 (:362)
int rfg_prop_block::get_buf(const prop_name& name, unsigned char*& buf) const
{
    obj_prop* prop = find_prop(name, name.crc);
    if (prop != nullptr && prop->type == 6)
    {
        buf = prop->data;
        return prop->size;
    }
    return 0;
}

// 0x0091A910 (:375)
int rfg_prop_block::get_buf_optional(const prop_name& name, unsigned char*& buf) const
{
    obj_prop* prop = find_prop(name, name.crc);
    if (prop != nullptr && prop->type == 6)
    {
        buf = prop->data;
        return prop->size;
    }
    return 0;
}

// 0x009219E0 (:392)
int rfg_prop_block::get_buf_copy(const prop_name& name, unsigned char* buf, int max_size) const
{
    obj_prop* prop = find_prop(name, name.crc);
    const void* src = &name;
    int size = 0;
    if (prop != nullptr && prop->type == 6)
    {
        size = prop->size;
        src = prop->data;
    }
    if (max_size < size)
    {
        return 0;
    }
    memcpy(buf, src, size);
    return size;
}

// 0x0091A940 (:404)
int rfg_prop_block::get_buf_copy_optional(const prop_name& name, unsigned char* buf, int max_size) const
{
    obj_prop* prop = find_prop(name, name.crc);
    const void* src = &name;
    int size = 0;
    if (prop != nullptr && prop->type == 6)
    {
        size = prop->size;
        src = prop->data;
    }
    if (max_size < size)
    {
        return 0;
    }
    memcpy(buf, src, size);
    return size;
}

// 0x0091A990 (:416)
void rfg_prop_block::set_buf(const prop_name& name, const unsigned char* buf, int size)
{
    obj_prop* prop = set_internal(name, size, 6);
    if (prop != nullptr)
    {
        memcpy(prop->data, buf, size);
    }
}

// 0x00921A30 (:429)
const char* rfg_prop_block::get_str_ref(const prop_name& name) const
{
    obj_prop* prop = find_prop(name, name.crc);
    if (prop != nullptr && prop->type == 4)
    {
        return (const char*)prop->data;
    }
    return nullptr;
}

// 0x0091A9C0 (:441)
const char* rfg_prop_block::get_str_ref_optional(const prop_name& name) const
{
    obj_prop* prop = find_prop(name, name.crc);
    if (prop != nullptr && prop->type == 4)
    {
        return (const char*)prop->data;
    }
    return nullptr;
}

// Space separated flag names, ORed together through the table (unknown names add nothing).
static int flags_from_str(char* str, str_data_hash_table<unsigned int, 32, 32, unsigned char>& table)
{
    int flags = 0;
    for (char* key = strtok(str, " "); key != nullptr; key = strtok(nullptr, " "))
    {
        unsigned int value = 0;
        table.get(key, value);
        flags |= value;
    }
    return flags;
}

// 0x009309D0 (:455)
void rfg_prop_block::get_str_as_flags(const prop_name& name, int& flags, str_data_hash_table<unsigned int, 32, 32, unsigned char>& table) const
{
    obj_prop* prop = find_prop(name, name.crc);
    if (prop != nullptr && prop->type == 4)
    {
        char buf[508];
        strcpy(buf, (const char*)prop->data);
        flags = flags_from_str(buf, table);
    }
}

// 0x00928CF0 (:465)
bool rfg_prop_block::get_str_as_flags_optional(const prop_name& name, int& flags, str_data_hash_table<unsigned int, 32, 32, unsigned char>& table) const
{
    obj_prop* prop = find_prop(name, name.crc);
    if (prop == nullptr || prop->type != 4)
    {
        return false;
    }
    char buf[508];
    strcpy(buf, (const char*)prop->data);
    flags = flags_from_str(buf, table);
    return true;
}

// 0x00928E00 (:489)
void rfg_prop_block::set_flags_as_str(const prop_name& name, int flags, str_data_hash_table<unsigned int, 32, 32, unsigned char>& table)
{
    // The names of the set flags in table order, separated by spaces.
    char buf[2044];
    int len = 0;
    const char* key;
    unsigned int value = 0;
    if (table.get_first_key(key))
    {
        do
        {
            table.get(key, value);
            if ((value & flags) != 0)
            {
                if (len != 0)
                {
                    buf[len++] = ' ';
                }
                for (const char* c = key; *c != '\0'; ++c)
                {
                    buf[len++] = *c;
                }
            }
        } while (table.get_next_key(key, key));
    }
    buf[len] = '\0';
    obj_prop* prop = set_internal(name, (int)strlen(buf) + 1, 4);
    if (prop != nullptr)
    {
        strcpy((char*)prop->data, buf);
    }
}

// 0x0091A9F0 (:526)
void rfg_prop_level_init_pre()
{
}

// 0x0091AA00 (:530)
void rfg_prop_set_thread_ownership()
{
    Prop_data_pool.set_thread_ownership();
}
