// Original: volition/vlib/code/cfile/vdir_internal.cpp
//
// Hash table of the loose-file directory: 32768 buckets holding the offset of the first entry (-1 when empty).

#include "volition/vlib/code/cfile/vdir_internal.h"

#include <string.h>

unsigned char* Vdir_file_entry_pool; // 0x01CE86A4
unsigned int Vdir_file_entry_pool_size; // 0x01CE86A8
unsigned int Vdir_file_entry_pool_used; // 0x01CE86AC
char* Vdir_dir_string_pool; // 0x01CE86B0
int* Vdir_hash_table; // 0x01CE86B4

// 0x00566A30 (:53)
void vdir_hash_system_init()
{
    Vdir_hash_table = new int[0x8000];
}

// 0x00566A50 (:61)
void vdir_clear_hash_table()
{
    for (int i = 0; i < 0x8000; ++i)
    {
        Vdir_hash_table[i] = -1;
    }
}

// 0x00566A80 (:76)
unsigned int vdir_hash(const char* s)
{
    unsigned int hash = 0;
    for (; *s != '\0'; ++s)
    {
        hash = ((unsigned char)*s + hash) * 0x7F & 0x7FFF;
    }
    return hash;
}

// 0x00566AB0 (:93)
vdir_file_entry* vdir_internal_find(const char* filename)
{
    // filename must be lower case; names are compared case-sensitively.
    if (Vdir_file_entry_pool == nullptr || Vdir_hash_table == nullptr)
    {
        return nullptr;
    }
    for (int offset = Vdir_hash_table[vdir_hash(filename)]; offset != -1;)
    {
        vdir_file_entry* entry = (vdir_file_entry*)(Vdir_file_entry_pool + offset);
        if (strcmp(filename, (const char*)(entry + 1)) == 0)
        {
            return entry;
        }
        offset = entry->next;
    }
    return nullptr;
}
