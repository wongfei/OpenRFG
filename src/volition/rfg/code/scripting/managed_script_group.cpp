// Original: volition/rfg/code/scripting/managed_script_group.cpp
//
// Managed script groups: a handle per group name.

#include "volition/rfg/code/scripting/managed_script_group.h"

#include "keen/base/trace.hpp"
#include "volition/rfg/code/rfg.h"
#include "volition/vlib/code/memory/rfg_split_mempool.h"

#include <new>

int Managed_script_handle_free; // 0x03611248
str_data_hash_table<int,106,32,unsigned char>* Managed_script_table; // 0x03611414

// 0x00B3DA10 (:24)
void managed_script_system_init()
{
    Managed_script_handle_free = 0;
    // Magic number: sizeof(str_data_hash_table<int,106,32,unsigned char>).
    void* memory = Permanent_mempool.alloc(0x116C, 0);
    if (memory == nullptr)
    {
        keen::debug::nativePrintArgs("Insufficient memory to allocate the managed script hash table");
        Managed_script_table = nullptr;
        return;
    }
    Managed_script_table = new (memory) str_data_hash_table<int,106,32,unsigned char>();
}

// 0x00B33640 (:39)
int managed_script_create_handle(const char* group_name)
{
    if (Managed_script_table == nullptr || Managed_script_table->exists(group_name))
    {
        return -1;
    }
    const int handle = Managed_script_handle_free++;
    return Managed_script_table->insert_key_data(group_name, handle) ? handle : -1;
}

// 0x00B336C0 (:59)
int managed_script_get_handle(const char* group_name)
{
    int handle;
    return Managed_script_table->get(group_name, handle) ? handle : -1;
}

// 0x00B3A6B0 (:70)
const char* managed_script_get_name(int group_handle)
{
    for (int* data = Managed_script_table->get_first(); data != nullptr; data = Managed_script_table->get_next(data))
    {
        if (*data == group_handle)
        {
            const char* key;
            Managed_script_table->get_key(data, key);
            return key;
        }
    }
    return nullptr;
}
