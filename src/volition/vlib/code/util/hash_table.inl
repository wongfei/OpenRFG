// Original: volition/vlib/code/util/hash_table.inl (template code, included by hash_table.h)
//
// Partial: hash_table<K, HASH, N, I>, data_hash_table, str_data_hash_table, str_hash_table, str_hash_table_cached,
// dynamic_hash_table / dynamic_uint_hash_table and dynamic_string_hash_table (the members recorded with addresses), and
// initialize_from_data.
// The other tables are not implemented yet.
//
// The invalid index is (I)-1. clear() empties every bucket and chains all entries into the free list
// (next[i] = i + 1). add() refuses keys that are already present; new entries go to the front of their bucket.
#pragma once

#include "volition/vlib/code/cfile/cfile.h"
#include "volition/vlib/code/memory/mempool_base.h"

#include "volition/vlib/code/util/string_comp.h"
#include "volition/vlib/code/util/string_util.h"

#include "keen/base/string.hpp"

#include <stdarg.h>
#include <string.h>

template<typename I>
static inline I hash_table_invalid_index()
{
    return (I)~(I)0;
}

// <char const *,&string_hash,128,unsigned char> 0x006D10D0 (:45)
template<typename K, unsigned int (*HASH)(K, unsigned int), int N, typename I>
hash_table<K, HASH, N, I>::hash_table(bool skip_init)
{
    if (!skip_init)
    {
        clear();
    }
}

// <char const *,&string_hash,128,unsigned char> 0x006C9680 (.h:142)
template<typename K, unsigned int (*HASH)(K, unsigned int), int N, typename I>
hash_table<K, HASH, N, I>::~hash_table()
{
}

// <char const *,&string_hash,128,unsigned char> 0x006C9780 (:18)
template<typename K, unsigned int (*HASH)(K, unsigned int), int N, typename I>
bool hash_table<K, HASH, N, I>::comp_keys(K key1, K key2) const
{
    return key1 == key2;
}

// <char const *,&string_hash,128,unsigned char> 0x006C99D0 (:26)
template<typename K, unsigned int (*HASH)(K, unsigned int), int N, typename I>
void hash_table<K, HASH, N, I>::clear()
{
    free = 0;
    for (int i = 0; i < N; ++i)
    {
        slot[i] = hash_table_invalid_index<I>();
    }
    for (int i = 0; i < N - 1; ++i)
    {
        next[i] = (I)(i + 1);
    }
    next[N - 1] = hash_table_invalid_index<I>();
}

// <char const *,&string_hash,128,unsigned char> 0x006C9930 (:66)
template<typename K, unsigned int (*HASH)(K, unsigned int), int N, typename I>
I hash_table<K, HASH, N, I>::alloc_entry()
{
    const I entry = free;
    if (entry == hash_table_invalid_index<I>())
    {
        return entry;
    }
    free = next[entry];
    return entry;
}

// <char const *,&string_hash,128,unsigned char> 0x006C9870 (:83)
template<typename K, unsigned int (*HASH)(K, unsigned int), int N, typename I>
I hash_table<K, HASH, N, I>::find(K key, I n) const
{
    for (I entry = slot[n]; entry != hash_table_invalid_index<I>(); entry = next[entry])
    {
        if (comp_keys(entry_key[entry], key))
        {
            return entry;
        }
    }
    return hash_table_invalid_index<I>();
}

// <char const *,&string_hash,32768,unsigned short> 0x005B3B20 (:106)
template<typename K, unsigned int (*HASH)(K, unsigned int), int N, typename I>
bool hash_table<K, HASH, N, I>::exists(K key) const
{
    return find(key, (I)HASH(key, N)) != hash_table_invalid_index<I>();
}

// <char const *,&string_hash,128,unsigned char> 0x006D0FE0 (:139)
template<typename K, unsigned int (*HASH)(K, unsigned int), int N, typename I>
I hash_table<K, HASH, N, I>::add(K key)
{
    const I bucket = (I)HASH(key, N);
    if (find(key, bucket) != hash_table_invalid_index<I>())
    {
        return hash_table_invalid_index<I>();
    }
    const I entry = alloc_entry();
    if (entry == hash_table_invalid_index<I>())
    {
        return entry;
    }
    entry_key[entry] = key;
    next[entry] = slot[bucket];
    slot[bucket] = entry;
    return entry;
}

// <char const *,&string_hash,128,unsigned char> 0x006D4CF0 (:171)
template<typename K, unsigned int (*HASH)(K, unsigned int), int N, typename I>
bool hash_table<K, HASH, N, I>::insert(K key)
{
    return add(key) != hash_table_invalid_index<I>();
}

// <char const *,&string_hash,128,unsigned char> 0x00766BD0 (:179)
template<typename K, unsigned int (*HASH)(K, unsigned int), int N, typename I>
void hash_table<K, HASH, N, I>::remove(K key)
{
    const I bucket = (I)HASH(key, N);
    I previous = hash_table_invalid_index<I>();
    for (I entry = slot[bucket]; entry != hash_table_invalid_index<I>(); entry = next[entry])
    {
        if (comp_keys(entry_key[entry], key))
        {
            if (previous == hash_table_invalid_index<I>())
            {
                slot[bucket] = next[entry];
            }
            else
            {
                next[previous] = next[entry];
            }
            next[entry] = free;
            free = entry;
            return;
        }
        previous = entry;
    }
}

// <char const *,&string_hash,32,unsigned char> 0x007FB130 (:217)
template<typename K, unsigned int (*HASH)(K, unsigned int), int N, typename I>
I hash_table<K, HASH, N, I>::get_next_slot(I slot_idx) const
{
    // The next non-empty bucket after slot_idx.
    for (unsigned int i = (unsigned int)slot_idx + 1; i < (unsigned int)N; ++i)
    {
        if (slot[i] != hash_table_invalid_index<I>())
        {
            return (I)i;
        }
    }
    return hash_table_invalid_index<I>();
}

// <char const *,&string_hash,32,unsigned char> 0x007FAAE0 (:232)
template<typename K, unsigned int (*HASH)(K, unsigned int), int N, typename I>
bool hash_table<K, HASH, N, I>::get_first_key(K& key) const
{
    // Iteration order: bucket by bucket, each chain front to back.
    for (unsigned int i = 0; i < (unsigned int)N; ++i)
    {
        if (slot[i] != hash_table_invalid_index<I>())
        {
            key = entry_key[slot[i]];
            return true;
        }
    }
    return false;
}

// <char const *,&string_hash,32,unsigned char> 0x00801320 (:248)
template<typename K, unsigned int (*HASH)(K, unsigned int), int N, typename I>
bool hash_table<K, HASH, N, I>::get_next_key(K& next_key, K key) const
{
    const I bucket = (I)HASH(key, N);
    const I entry = find(key, bucket);
    if (entry == hash_table_invalid_index<I>())
    {
        return false;
    }
    if (next[entry] != hash_table_invalid_index<I>())
    {
        next_key = entry_key[next[entry]];
        return true;
    }
    const I next_bucket = get_next_slot(bucket);
    if (next_bucket == hash_table_invalid_index<I>())
    {
        return false;
    }
    next_key = entry_key[slot[next_bucket]];
    return true;
}

// <char const *,&string_hash,128,unsigned char> 0x006C9790 (:282)
template<typename K, unsigned int (*HASH)(K, unsigned int), int N, typename I>
bool hash_table<K, HASH, N, I>::save(cfile* fp) const
{
    // Writes the whole table image, vfptr included (inlined cf_write in the original).
    return cf_write(this, sizeof(*this), fp) == sizeof(*this);
}

// <char const *,int,&string_hash,128,unsigned char> 0x0077C200 (:435)
template<typename K, typename D, unsigned int (*HASH)(K, unsigned int), int N, typename I>
data_hash_table<K, D, HASH, N, I>::data_hash_table(bool skip_init)
    : hash_table<K, HASH, N, I>(skip_init)
{
    // The data is cleared even with skip_init.
    memset(entry_data, 0, sizeof(entry_data));
}

// <char const *,int,&string_hash,128,unsigned char> 0x00770FD0
template<typename K, typename D, unsigned int (*HASH)(K, unsigned int), int N, typename I>
data_hash_table<K, D, HASH, N, I>::~data_hash_table()
{
}

// <char const *,int,&string_hash,128,unsigned char> 0x0077D6E0 (:445)
template<typename K, typename D, unsigned int (*HASH)(K, unsigned int), int N, typename I>
I data_hash_table<K, D, HASH, N, I>::add(K key, const D& data)
{
    const I entry = hash_table<K, HASH, N, I>::add(key);
    if (entry != hash_table_invalid_index<I>())
    {
        entry_data[entry] = data;
    }
    return entry;
}

// <char const *,int,&string_hash,128,unsigned char> 0x00793640 (:460)
template<typename K, typename D, unsigned int (*HASH)(K, unsigned int), int N, typename I>
bool data_hash_table<K, D, HASH, N, I>::insert_key_data(K key, const D& data)
{
    return add(key, data) != hash_table_invalid_index<I>();
}

// <char const *,int,&string_hash,128,unsigned char> 0x0077C2E0 (:487)
template<typename K, typename D, unsigned int (*HASH)(K, unsigned int), int N, typename I>
bool data_hash_table<K, D, HASH, N, I>::get(K key, D& data) const
{
    const I entry = this->find(key, (I)HASH(key, N));
    if (entry == hash_table_invalid_index<I>())
    {
        return false;
    }
    data = entry_data[entry];
    return true;
}

// <char const *,int,&string_hash,128,unsigned char> 0x0077C270 (:554)
template<typename K, typename D, unsigned int (*HASH)(K, unsigned int), int N, typename I>
bool data_hash_table<K, D, HASH, N, I>::save(cfile* fp) const
{
    // The table image up to the data, padding to 4 bytes, then the data.
    const unsigned int header_size = (unsigned int)((const char*)entry_data - (const char*)this);
    if (cf_write(this, header_size, fp) != header_size)
    {
        return false;
    }
    const unsigned int pos = cf_tell(fp);
    cf_seek(fp, (int)(pos + ((pos & 3) == 0 ? 0 : 4 - (pos & 3))), CF_SEEK_SET);
    return cf_write(entry_data, sizeof(entry_data), fp) == sizeof(entry_data);
}

// <int,106,32,unsigned char> 0x00B3B910 (:694)
template<typename D, int N, int L, typename I>
str_data_hash_table<D, N, L, I>::str_data_hash_table()
    : data_hash_table<const char*, D, &string_hash, N, I>(false)
{
}

// <unsigned int,32,32,unsigned char> 0x0079FC20 (:703)
template<typename D, int N, int L, typename I>
str_data_hash_table<D, N, L, I>::str_data_hash_table(const char* key, ...)
    : data_hash_table<const char*, D, &string_hash, N, I>(false)
{
    // Pairs of key and data, ended by a null key.
    va_list args;
    va_start(args, key);
    while (key != nullptr)
    {
        const D data = va_arg(args, D);
        insert_key_data(key, data);
        key = va_arg(args, const char*);
    }
    va_end(args);
}

// <int,106,32,unsigned char> 0x00B3B9A0
template<typename D, int N, int L, typename I>
str_data_hash_table<D, N, L, I>::~str_data_hash_table()
{
}

// <int,106,32,unsigned char> 0x00B3B970 (:663)
template<typename D, int N, int L, typename I>
bool str_data_hash_table<D, N, L, I>::comp_keys(const char* key1, const char* key2) const
{
    return key1 == key2 || v_stricmp(key1, key2) == 0;
}

// <int,106,32,unsigned char> 0x00B3B9D0 (:671)
template<typename D, int N, int L, typename I>
bool str_data_hash_table<D, N, L, I>::insert_key_data(const char* key, const D& data)
{
    // The key is copied into the table (cut to L - 1 characters) and the entry points to the copy.
    const I entry = this->add(key, data);
    if (entry == hash_table_invalid_index<I>())
    {
        return false;
    }
    strncpy_safe(str_data[entry], key, L);
    this->entry_key[entry] = str_data[entry];
    return true;
}

// <255,64,unsigned char> 0x009A6E10 (:598)
template<int N, int L, typename I>
str_hash_table<N, L, I>::str_hash_table()
    : hash_table<const char*, &string_hash, N, I>(false)
{
}

// <255,64,unsigned char> 0x009A6E90
template<int N, int L, typename I>
str_hash_table<N, L, I>::~str_hash_table()
{
}

// <255,64,unsigned char> 0x009A6E60 (:625)
template<int N, int L, typename I>
bool str_hash_table<N, L, I>::comp_keys(const char* key1, const char* key2) const
{
    return key1 == key2 || v_stricmp(key1, key2) == 0;
}

// <255,64,unsigned char> 0x009A6EC0 (:633): the key is copied into the table (cut to L - 1 characters)
template<int N, int L, typename I>
bool str_hash_table<N, L, I>::insert(const char* key)
{
    const I entry = this->add(key);
    if (entry == hash_table_invalid_index<I>())
    {
        return false;
    }
    strncpy_safe(str_data[entry], key, L);
    this->entry_key[entry] = str_data[entry];
    return true;
}

// <32768,unsigned short> 0x005B5F20
template<int N, typename I>
str_hash_table_cached<N, I>::str_hash_table_cached()
    : hash_table<const char*, &string_hash, N, I>(false)
{
}

// <32768,unsigned short> 0x005B5F70
template<int N, typename I>
str_hash_table_cached<N, I>::~str_hash_table_cached()
{
}

// <32768,unsigned short> 0x005B5F40 (:728)
template<int N, typename I>
bool str_hash_table_cached<N, I>::comp_keys(const char* key1, const char* key2) const
{
    return key1 == key2 || keen::compareStringNoCase(key1, key2) == 0;
}

// <preloaded_table_data,64> 0x005C6960 (:1016)
template<typename D, int L>
dynamic_string_hash_table<D, L>::dynamic_string_hash_table()
    : slot(nullptr)
    , size(0)
{
}

// <preloaded_table_data,64> 0x005BF5C0
template<typename D, int L>
dynamic_string_hash_table<D, L>::~dynamic_string_hash_table()
{
}

// <preloaded_table_data,64> 0x005C69A0 (:1023)
template<typename D, int L>
void dynamic_string_hash_table<D, L>::init(mempool_base& pool, unsigned int num_objects)
{
    // num_objects is both the bucket count and the entry capacity.
    size = num_objects;
    slot = (hash_entry**)pool.alloc(num_objects * sizeof(hash_entry*), 4);
    entries.init(pool, num_objects);
    memset(slot, 0, size * sizeof(hash_entry*));
}

// <preloaded_table_data,64> 0x005C7090 (:1036)
template<typename D, int L>
typename dynamic_string_hash_table<D, L>::hash_entry* dynamic_string_hash_table<D, L>::find(const char* key, bool find_or_add)
{
    // Keys compare case-insensitively; a new entry goes to the front of its bucket (keys are cut to L - 1 chars).
    const unsigned int bucket = string_hash(key, size);
    for (hash_entry* entry = slot[bucket]; entry != nullptr; entry = entry->next)
    {
        if (keen::compareStringNoCase(key, entry->key) == 0)
        {
            return entry;
        }
    }
    if (!find_or_add || entries.get_free() <= 0)
    {
        return nullptr;
    }
    hash_entry* entry = entries.alloc(0);
    strncpy(entry->key, key, L);
    entry->root = bucket;
    entry->next = slot[bucket];
    entry->prev = nullptr;
    if (slot[bucket] != nullptr)
    {
        slot[bucket]->prev = entry;
    }
    slot[bucket] = entry;
    return entry;
}

// <preloaded_table_data,64> 0x005C98C0 (:1080)
template<typename D, int L>
bool dynamic_string_hash_table<D, L>::insert(const char* key, D data)
{
    hash_entry* entry = find(key, true);
    if (entry == nullptr)
    {
        return false;
    }
    entry->data = data;
    return true;
}

// <preloaded_table_data,64> 0x005C98F0 (:1097)
template<typename D, int L>
bool dynamic_string_hash_table<D, L>::get(const char* key, D& data)
{
    for (hash_entry* entry = slot[string_hash(key, size)]; entry != nullptr; entry = entry->next)
    {
        if (keen::compareStringNoCase(key, entry->key) == 0)
        {
            data = entry->data;
            return true;
        }
    }
    return false;
}

// ---- dynamic_hash_table<K, D, HASH>: buckets of doubly linked entries from an object pool.

// <unsigned int,split_memblk *,&uint_hash> 0x005C68C0 (:785)
template<typename K, typename D, unsigned int (*HASH)(K, unsigned int)>
dynamic_hash_table<K, D, HASH>::dynamic_hash_table()
    : slot(nullptr)
    , size(0)
{
}

// <unsigned int,split_memblk *,&uint_hash> 0x005BC9C0
template<typename K, typename D, unsigned int (*HASH)(K, unsigned int)>
dynamic_hash_table<K, D, HASH>::~dynamic_hash_table()
{
}

// <unsigned int,split_memblk *,&uint_hash> 0x005C6900 (:798)
template<typename K, typename D, unsigned int (*HASH)(K, unsigned int)>
void dynamic_hash_table<K, D, HASH>::init(mempool_base& pool, unsigned int num_objects)
{
    // num_objects is both the bucket count and the entry capacity.
    size = num_objects;
    slot = (hash_entry**)pool.alloc(num_objects * sizeof(hash_entry*), 4);
    entries.init(pool, num_objects);
    memset(slot, 0, size * sizeof(hash_entry*));
}

// <unsigned int,split_page_mempool_header,&uint_hash> 0x005CBA60 (:792)
template<typename K, typename D, unsigned int (*HASH)(K, unsigned int)>
unsigned int dynamic_hash_table<K, D, HASH>::bytes_allocated() const
{
    return entries.bytes_allocated() + size * sizeof(hash_entry*);
}

// <unsigned int,split_memblk *,&uint_hash> 0x005C9860 (:810)
template<typename K, typename D, unsigned int (*HASH)(K, unsigned int)>
void dynamic_hash_table<K, D, HASH>::clear()
{
    entries.thread_access.enter();
    for (hash_entry* entry = entries.get_first(0); entry != nullptr; entry = entries.free(entry))
    {
    }
    entries.thread_access.leave();
    memset(slot, 0, size * sizeof(hash_entry*));
}

// <unsigned int,split_memblk *,&uint_hash> 0x005C1890 (:826)
template<typename K, typename D, unsigned int (*HASH)(K, unsigned int)>
typename dynamic_hash_table<K, D, HASH>::hash_entry* dynamic_hash_table<K, D, HASH>::find(K key, bool find_or_add)
{
    // A new entry goes to the front of its bucket.
    const unsigned int bucket = HASH(key, size);
    for (hash_entry* entry = slot[bucket]; entry != nullptr; entry = entry->next)
    {
        if (key == entry->key)
        {
            return entry;
        }
    }
    if (!find_or_add || entries.get_free() <= 0)
    {
        return nullptr;
    }
    hash_entry* entry = entries.alloc(0);
    entry->key = key;
    entry->root = bucket;
    entry->next = slot[bucket];
    entry->prev = nullptr;
    if (slot[bucket] != nullptr)
    {
        slot[bucket]->prev = entry;
    }
    slot[bucket] = entry;
    return entry;
}

// <unsigned int,split_memblk *,&uint_hash> 0x005C6790 (:755)
template<typename K, typename D, unsigned int (*HASH)(K, unsigned int)>
bool dynamic_hash_table<K, D, HASH>::insert(K key, D data)
{
    hash_entry* entry = find(key, true);
    if (entry == nullptr)
    {
        return false;
    }
    entry->data = data;
    return true;
}

// <unsigned int,split_page_mempool_header,&uint_hash> 0x005C6580 (:770)
template<typename K, typename D, unsigned int (*HASH)(K, unsigned int)>
D* dynamic_hash_table<K, D, HASH>::insert_and_get_ptr(K key, D data)
{
    hash_entry* entry = find(key, true);
    if (entry == nullptr)
    {
        return nullptr;
    }
    entry->data = data;
    return &entry->data;
}

// <unsigned int,split_memblk *,&uint_hash> 0x005C67B0 (:870)
template<typename K, typename D, unsigned int (*HASH)(K, unsigned int)>
D dynamic_hash_table<K, D, HASH>::get(K key)
{
    for (hash_entry* entry = slot[HASH(key, size)]; entry != nullptr; entry = entry->next)
    {
        if (key == entry->key)
        {
            return entry->data;
        }
    }
    return D();
}

// <unsigned int,split_page_mempool_header,&uint_hash> 0x005C65B0 (:884)
template<typename K, typename D, unsigned int (*HASH)(K, unsigned int)>
D* dynamic_hash_table<K, D, HASH>::get_ptr(K key)
{
    for (hash_entry* entry = slot[HASH(key, size)]; entry != nullptr; entry = entry->next)
    {
        if (key == entry->key)
        {
            return &entry->data;
        }
    }
    return nullptr;
}

// <unsigned int,network_game_handle *,&uint_hash> 0x009B4120 (:897)
template<typename K, typename D, unsigned int (*HASH)(K, unsigned int)>
bool dynamic_hash_table<K, D, HASH>::exists(K key)
{
    return get_ptr(key) != nullptr;
}

// <unsigned int,split_page_mempool_header,&uint_hash> 0x005C6610 (:904)
template<typename K, typename D, unsigned int (*HASH)(K, unsigned int)>
void dynamic_hash_table<K, D, HASH>::remove(K key)
{
    hash_entry* entry = slot[HASH(key, size)];
    while (entry != nullptr && key != entry->key)
    {
        entry = entry->next;
    }
    if (entry == nullptr)
    {
        return;
    }
    if (entry != slot[entry->root])
    {
        if (entry->prev != nullptr)
        {
            entry->prev->next = entry->next;
        }
        if (entry->next != nullptr)
        {
            entry->next->prev = entry->prev;
        }
    }
    else
    {
        // The new bucket head keeps its prev pointer to the removed entry (as decompiled).
        slot[entry->root] = entry->next;
    }
    entries.free(entry);
}

// <unsigned int,split_memblk *,&uint_hash> 0x005C6810 (:931)
template<typename K, typename D, unsigned int (*HASH)(K, unsigned int)>
D dynamic_hash_table<K, D, HASH>::get_and_remove(K key)
{
    hash_entry* entry = slot[HASH(key, size)];
    while (entry != nullptr && key != entry->key)
    {
        entry = entry->next;
    }
    if (entry == nullptr)
    {
        return D();
    }
    if (entry != slot[entry->root])
    {
        if (entry->prev != nullptr)
        {
            entry->prev->next = entry->next;
        }
        if (entry->next != nullptr)
        {
            entry->next->prev = entry->prev;
        }
    }
    else
    {
        slot[entry->root] = entry->next;
    }
    const D data = entry->data;
    entries.free(entry);
    return data;
}

// <split_memblk *> 0x005C7FC0
template<typename D>
dynamic_uint_hash_table<D>::dynamic_uint_hash_table()
{
}

// <split_memblk *> 0x005C43F0
template<typename D>
dynamic_uint_hash_table<D>::~dynamic_uint_hash_table()
{
}

// ---- hash_table / data_hash_table iteration and str_data_hash_table_cached

// <char const *,&string_hash,1024,unsigned short> 0x004AB8B0 (:120)
template<typename K, unsigned int (*HASH)(K, unsigned int), int N, typename I>
I hash_table<K, HASH, N, I>::get_count() const
{
    // Counts the entries chain by chain.
    I count = 0;
    for (int i = 0; i < N; ++i)
    {
        for (I entry = slot[i]; entry != hash_table_invalid_index<I>(); entry = next[entry])
        {
            ++count;
        }
    }
    return count;
}

// <char const *,prim_add_entry,&string_hash,1024,unsigned short> 0x004ABCF0 (:468)
template<typename K, typename D, unsigned int (*HASH)(K, unsigned int), int N, typename I>
D* data_hash_table<K, D, HASH, N, I>::get_ptr(K key)
{
    const I entry = this->find(key, (I)HASH(key, N));
    if (entry == hash_table_invalid_index<I>())
    {
        return nullptr;
    }
    return &entry_data[entry];
}

// <char const *,prim_add_entry,&string_hash,1024,unsigned short> 0x004AE810 (:508)
template<typename K, typename D, unsigned int (*HASH)(K, unsigned int), int N, typename I>
D* data_hash_table<K, D, HASH, N, I>::get_first()
{
    K key;
    if (!this->get_first_key(key))
    {
        return nullptr;
    }
    return get_ptr(key);
}

// <char const *,int,&string_hash,512,unsigned short> 0x0074B370 (:521)
template<typename K, typename D, unsigned int (*HASH)(K, unsigned int), int N, typename I>
D* data_hash_table<K, D, HASH, N, I>::get_next(D* data)
{
    // The entry index of data selects the current key.
    K key;
    if (!this->get_next_key(key, this->entry_key[(I)(data - entry_data)]))
    {
        return nullptr;
    }
    return get_ptr(key);
}

template<typename K, typename D, unsigned int (*HASH)(K, unsigned int), int N, typename I>
bool data_hash_table<K, D, HASH, N, I>::get_key(D* data, K& key)
{
    key = this->entry_key[(I)(data - entry_data)];
    return true;
}

// <unsigned int,4096,unsigned short> 0x005C4470
template<typename D, int N, typename I>
str_data_hash_table_cached<D, N, I>::str_data_hash_table_cached()
    : data_hash_table<const char*, D, &string_hash, N, I>(false)
{
}

// <unsigned int,4096,unsigned short> 0x005C44D0
template<typename D, int N, typename I>
str_data_hash_table_cached<D, N, I>::~str_data_hash_table_cached()
{
}

// <unsigned int,4096,unsigned short> 0x005C44A0 (:742)
template<typename D, int N, typename I>
bool str_data_hash_table_cached<D, N, I>::comp_keys(const char* key1, const char* key2) const
{
    return key1 == key2 || keen::compareStringNoCase(key1, key2) == 0;
}

// ---- loading a table image (zone files keep their object relation table in this form)

// <unsigned int,&uint_hash,7280,unsigned short> 0x0091E4D0
template<typename K, unsigned int (*HASH)(K, unsigned int), int N, typename I>
void hash_table<K, HASH, N, I>::initialize_from_data(I in_free, const I* in_slot, const I* in_next, const K* in_entry_key)
{
    free = in_free;
    memcpy(slot, in_slot, sizeof(slot));
    memcpy(next, in_next, sizeof(next));
    memcpy(entry_key, in_entry_key, sizeof(entry_key));
}

// <unsigned int,unsigned int,&uint_hash,7280,unsigned short> 0x00924610
template<typename K, typename D, unsigned int (*HASH)(K, unsigned int), int N, typename I>
void data_hash_table<K, D, HASH, N, I>::initialize_from_data(I in_free, const I* in_slot, const I* in_next, const K* in_entry_key, const D* in_entry_data, int in_entry_data_num_objects)
{
    // Only the first in_entry_data_num_objects data entries come from the image; the rest stay zero.
    this->free = in_free;
    memcpy(this->slot, in_slot, sizeof(this->slot));
    memcpy(this->next, in_next, sizeof(this->next));
    memcpy(this->entry_key, in_entry_key, sizeof(this->entry_key));
    memset(entry_data, 0, sizeof(entry_data));
    memcpy(entry_data, in_entry_data, in_entry_data_num_objects * sizeof(D));
}
