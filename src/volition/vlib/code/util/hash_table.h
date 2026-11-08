// Original: volition/vlib/code/util/hash_table.h
// Implementation: volition/vlib/code/util/hash_table.cpp (hash functions), hash_table.inl (template code)
//
// Hash tables (behaviour from the layouts and the decompiled dynamic_object_pool users):
// - hash_table<K, HASH, N, I>: fixed capacity N keys, chained through index arrays of type I (unsigned char or
//   unsigned short, chosen so N fits). slot[] holds the first entry per bucket, next[] the chain, entry_key[] the
//   keys; `free` is the next unused entry. HASH(key, N) picks the bucket. The base comp_keys compares keys with ==
//   (pointer identity for strings); the str_* tables override it with a case-insensitive v_stricmp (decompiled). save() writes the table to a cfile, initialize_from_data() reads that image back.
// - data_hash_table<K, D, HASH, N, I> adds a parallel entry_data[N].
// - str_data_hash_table<D, N, L, I> / str_hash_table<N, L, I> own copies of their string keys (str_data[N][L]);
//   the *_cached variants keep only the pointers (the strings live elsewhere) and compare by contents.
// - dynamic_hash_table<K, D, HASH> allocates entries from a dynamic_object_pool and a slot array from a mempool
//   (init); dynamic_uint_hash_table<D> and dynamic_string_hash_table<D, L> are the common forms.
// Method addresses are for one instantiation, named in <>.
#pragma once

#include <stdint.h>

#include "volition/vlib/code/memory/object_pool.h"

struct cfile;
class mempool_base;

unsigned int uint_hash(unsigned int _key, unsigned int hash_size); // 0x0058FDE0 (hash_table.cpp:18)
unsigned int uint64_hash(uint64_t _key, unsigned int hash_size); // 0x0058FE20 (hash_table.cpp:25)
unsigned int string_hash(const char* key, unsigned int hash_size); // 0x0058FE90 (hash_table.cpp:38), h = h * 33 ^ tolower(c), modulo hash_size
unsigned int hash_mix_32(unsigned int val); // 0x00482E50 (.h:447)
unsigned int hash_uint64(uint64_t val, unsigned int size); // 0x006E1100 (.h:461)
unsigned int hash_uint(unsigned int val, unsigned int size); // 0x00482E90 (.h:474)

// size: vfptr, I free, I slot[N], I next[N], K entry_key[N] (natural alignment)
template<typename K, unsigned int (*HASH)(K, unsigned int), int N, typename I>
class hash_table
{
protected:
    I alloc_entry(); // <char const *,&string_hash,128,unsigned char> 0x006C9930 (hash_table.inl:66)
    I find(K key, I n) const; // <char const *,&string_hash,128,unsigned char> 0x006C9870 (hash_table.inl:83)
    I add(K key); // <char const *,&string_hash,128,unsigned char> 0x006D0FE0 (hash_table.inl:139)
    I get_next_slot(I) const;
    virtual bool comp_keys(K key1, K key2) const; // vtbl+0x00 <char const *,&string_hash,128,unsigned char> 0x006C9780 (hash_table.inl:18)
public:
    hash_table(bool skip_init); // <char const *,&string_hash,128,unsigned char> 0x006D10D0 (hash_table.inl:45)
    virtual ~hash_table(); // vtbl+0x04 <char const *,&string_hash,128,unsigned char> 0x006C9680 (.h:142)
    virtual bool insert(K key); // vtbl+0x08 <char const *,&string_hash,128,unsigned char> 0x006D4CF0 (hash_table.inl:171)
    bool exists(K key) const; // <char const *,&string_hash,32768,unsigned short> 0x005B3B20 (hash_table.inl:106)
    I get_count() const;
    void clear(); // <char const *,&string_hash,128,unsigned char> 0x006C99D0 (hash_table.inl:26)
    void remove(K key); // <char const *,&string_hash,128,unsigned char> 0x00766BD0 (hash_table.inl:179)
    bool get_first_key(K&) const;
    bool get_next_key(K&, K) const;
    virtual bool save(cfile* fp) const; // vtbl+0x0C <char const *,&string_hash,128,unsigned char> 0x006C9790 (hash_table.inl:282)
    void initialize_from_data(I, const I*, const I*, const K*);

    // 0x00 vfptr
protected:
    I free; // 0x04
    I slot[N]; // 0x05 for I == unsigned char
    I next[N];
    K entry_key[N];
};

template<typename K, typename D, unsigned int (*HASH)(K, unsigned int), int N, typename I>
class data_hash_table : public hash_table<K, HASH, N, I>
{
protected:
    I add(K key, const D& data); // <char const *,int,&string_hash,128,unsigned char> 0x0077D6E0 (hash_table.inl:445)
public:
    data_hash_table(bool skip_init); // <char const *,int,&string_hash,128,unsigned char> 0x0077C200 (hash_table.inl:435)
    virtual bool insert_key_data(K key, const D& data); // vtbl+0x10 <char const *,int,&string_hash,128,unsigned char> 0x00793640 (hash_table.inl:460)
    D* get_ptr(K);
    bool get(K key, D& data) const; // <char const *,int,&string_hash,128,unsigned char> 0x0077C2E0 (hash_table.inl:487)
    D* get_first();
    D* get_next(D*);
    bool get_key(D*, K&);
    virtual bool save(cfile* fp) const; // vtbl+0x0C <char const *,int,&string_hash,128,unsigned char> 0x0077C270 (hash_table.inl:554)
    void initialize_from_data(I, const I*, const I*, const K*, const D*, int);
    virtual ~data_hash_table(); // vtbl+0x04 <char const *,int,&string_hash,128,unsigned char> 0x00770FD0

private:
    D entry_data[N];
};

// String keys copied into the table (up to L - 1 characters).
template<typename D, int N, int L, typename I>
class str_data_hash_table : public data_hash_table<const char*, D, &string_hash, N, I>
{
    virtual bool comp_keys(const char* key1, const char* key2) const; // vtbl+0x00 <int,106,32,unsigned char> 0x00B3B970 (hash_table.inl:663)
public:
    virtual bool insert_key_data(const char* key, const D& data); // vtbl+0x10 <int,106,32,unsigned char> 0x00B3B9D0 (hash_table.inl:671)
    str_data_hash_table(const char*, ...);
    str_data_hash_table(); // <int,106,32,unsigned char> 0x00B3B910 (hash_table.inl:694)
    virtual ~str_data_hash_table(); // vtbl+0x04 <int,106,32,unsigned char> 0x00B3B9A0

private:
    char str_data[N][L];
};

// String keys owned by the caller.
template<typename D, int N, typename I>
class str_data_hash_table_cached : public data_hash_table<const char*, D, &string_hash, N, I>
{
    virtual bool comp_keys(const char* key1, const char* key2) const; // vtbl+0x00 <unsigned int,4096,unsigned short> 0x005C44A0 (hash_table.inl:742)
public:
    str_data_hash_table_cached(); // <unsigned int,4096,unsigned short> 0x005C4470
    virtual ~str_data_hash_table_cached(); // vtbl+0x04 <unsigned int,4096,unsigned short> 0x005C44D0
};

template<int N, int L, typename I>
class str_hash_table : public hash_table<const char*, &string_hash, N, I>
{
    virtual bool comp_keys(const char* key1, const char* key2) const; // vtbl+0x00 <255,64,unsigned char> 0x009A6E60 (hash_table.inl:625)
public:
    virtual bool insert(const char* key); // vtbl+0x08 <255,64,unsigned char> 0x009A6EC0 (hash_table.inl:633)
    str_hash_table(const char*, ...);
    str_hash_table(); // <255,64,unsigned char> 0x009A6E10 (hash_table.inl:598)
    virtual ~str_hash_table(); // vtbl+0x04 <255,64,unsigned char> 0x009A6E90

private:
    char str_data[N][L];
};

template<int N, typename I>
class str_hash_table_cached : public hash_table<const char*, &string_hash, N, I>
{
    virtual bool comp_keys(const char* key1, const char* key2) const; // vtbl+0x00 <32768,unsigned short> 0x005B5F40 (hash_table.inl:728)
public:
    str_hash_table_cached(); // <32768,unsigned short> 0x005B5F20
    virtual ~str_hash_table_cached(); // vtbl+0x04 <32768,unsigned short> 0x005B5F70
};

// size 0x48
template<typename K, typename D, unsigned int (*HASH)(K, unsigned int)>
class dynamic_hash_table
{
public:
    // Entries of one bucket form a list; `root` is the bucket index.
    // size 0x10 + sizeof(D) (natural alignment)
    struct hash_entry
    {
        D data;
        hash_entry* next;
        hash_entry* prev;
        K key;
        unsigned int root;
    };

protected:
    hash_entry* find(K key, bool find_or_add); // <unsigned int,split_memblk *,&uint_hash> 0x005C1890 (hash_table.inl:826)
public:
    bool insert(K key, D data); // <unsigned int,split_memblk *,&uint_hash> 0x005C6790 (hash_table.inl:755)
    D* insert_and_get_ptr(K, D);
    D* get_ptr(K);
    D get(K key); // <unsigned int,split_memblk *,&uint_hash> 0x005C67B0 (hash_table.inl:870)
    bool exists(K);
    void clear(); // <unsigned int,split_memblk *,&uint_hash> 0x005C9860 (hash_table.inl:810)
    void remove(K);
    D get_and_remove(K key); // <unsigned int,split_memblk *,&uint_hash> 0x005C6810 (hash_table.inl:931)
    D* get_first();
    D* get_next(const D*);
    const K get_key(const D*);
    dynamic_hash_table(); // <unsigned int,split_memblk *,&uint_hash> 0x005C68C0 (hash_table.inl:785)
    void init(mempool_base& pool, unsigned int num_objects); // <unsigned int,split_memblk *,&uint_hash> 0x005C6900 (hash_table.inl:798)
    unsigned int bytes_allocated() const;
    ~dynamic_hash_table(); // <unsigned int,split_memblk *,&uint_hash> 0x005BC9C0

    dynamic_object_pool<hash_entry, 1> entries; // 0x00
    hash_entry** slot; // 0x40
protected:
    unsigned int size; // 0x44
};

// size 0x48
template<typename D>
class dynamic_uint_hash_table : public dynamic_hash_table<unsigned int, D, &uint_hash>
{
public:
    dynamic_uint_hash_table(); // <split_memblk *> 0x005C7FC0
    ~dynamic_uint_hash_table(); // <split_memblk *> 0x005C43F0
};

// size 0x48
template<typename D, int L>
class dynamic_string_hash_table
{
public:
    // size 0x14 + sizeof(D) + L (natural alignment)
    struct hash_entry
    {
        D data;
        hash_entry* next;
        hash_entry* prev;
        char key[L];
        unsigned int root;
    };

protected:
    hash_entry* find(const char* key, bool find_or_add); // <preloaded_table_data,64> 0x005C7090 (hash_table.inl:1036)
public:
    bool insert(const char* key, D data); // <preloaded_table_data,64> 0x005C98C0 (hash_table.inl:1080)
    bool get(const char* key, D& data); // <preloaded_table_data,64> 0x005C98F0 (hash_table.inl:1097)
    dynamic_string_hash_table(); // <preloaded_table_data,64> 0x005C6960 (hash_table.inl:1016)
    void init(mempool_base& pool, unsigned int num_objects); // <preloaded_table_data,64> 0x005C69A0 (hash_table.inl:1023)
    ~dynamic_string_hash_table(); // <preloaded_table_data,64> 0x005BF5C0

    dynamic_object_pool<hash_entry, 1> entries; // 0x00
    hash_entry** slot; // 0x40
protected:
    unsigned int size; // 0x44
};

#include "volition/vlib/code/util/hash_table.inl"
