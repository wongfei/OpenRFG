// Original: volition/rfg/code/geom/mesh_morph.cpp
//
// The mesh morph stream2 primitive (the character head and face morph targets, stub_impl.md S3): a loaded file takes a
// slot of character_instance.cpp's Morph_array and is found by name in Mesh_morph_hash (both are file statics of the
// same unity TU, rfg_geom_unity.cpp).

#include "volition/rfg/code/geom/mesh_morph.h"

#include "volition/render_lib/code/rl_morph_data.h"
#include "volition/render_lib/code/rl_morph_target.h"
#include "volition/vlib/code/cfile/cfile.h"

#include "keen/base/trace.hpp"

#include <stdio.h>

static mesh_morph_entry* Morph_free_list; // 0x025EF8E0

// The thread's message buffer (TLS +0x38 in rfg.exe, shared by the unity TU's messages).
static __declspec(thread) char Mesh_morph_message[1024];

// 0x00808610 (:71)
// The morph data into a new morph container in a free slot, registered under the file name without extension. A full
// table or bad data is reported and the file stays unloaded (the load fails).
bool mesh_morph_load(const char* filename, unsigned int srid, unsigned char* cpu_data, int cpu_data_size, unsigned char* gpu_data, int gpu_data_size)
{
    char name[64];
    cf_drop_ext(name, sizeof(name), filename);
    mesh_morph_entry* entry = Morph_free_list;
    if (entry == nullptr)
    {
        sprintf(Mesh_morph_message, "Out of morph slots, when loading %s\n This is non-fatal, ignoring it will stop the morph from loading/playing.", filename);
        keen::debug::nativePrintArgs(Mesh_morph_message);
        return false;
    }
    int cpu_offset = 0;
    int gpu_offset = 0;
    rl_morph_data* morph_data = rl_morph_data_load(cpu_data, gpu_data, cpu_offset, gpu_offset);
    if (morph_data == nullptr)
    {
        sprintf(Mesh_morph_message, "Failed to load morph data for %s", filename);
        keen::debug::nativePrintArgs(Mesh_morph_message);
        return false;
    }
    // original: the container factory's result is not checked
    rl_morph_container* container = rl_morph_container::create();
    container->set_morph_data(morph_data);
    entry->data = container;
    if (Morph_free_list == entry && (Morph_free_list = entry->next) == entry)
    {
        Morph_free_list = nullptr;
    }
    else
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
    entry->prev = nullptr;
    entry->next = nullptr;
    Mesh_morph_hash.insert(name, srid, entry);
    return true;
}

// 0x0080B3F0 (:126)
// The container released and the slot back on the free list.
void mesh_morph_unload(const char* filename, unsigned int srid)
{
    char name[64];
    cf_drop_ext(name, sizeof(name), filename);
    mesh_morph_entry* entry = nullptr;
    Mesh_morph_hash.get(name, srid, entry);
    if (entry == nullptr || entry->data == nullptr)
    {
        return;
    }
    if (srid != 0)
    {
        Mesh_morph_hash.remove(name, srid);
    }
    entry->data->release();
    entry->data = nullptr;
    if (Morph_free_list == nullptr)
    {
        entry->next = entry;
        entry->prev = entry;
        Morph_free_list = entry;
        return;
    }
    entry->prev = Morph_free_list->prev;
    entry->next = Morph_free_list;
    Morph_free_list->prev->next = entry;
    Morph_free_list->prev = entry;
}

// 0x007F46C0 (:156)
// Every slot empty, numbered and on the free list in array order.
void mesh_morph_system_init()
{
    for (int i = 0; i < 256; i++)
    {
        Morph_array[i].index = i;
        Morph_array[i].data = nullptr;
    }
    Morph_free_list = nullptr;
    for (int i = 0; i < 256; i++)
    {
        mesh_morph_entry* entry = &Morph_array[i];
        if (Morph_free_list == nullptr)
        {
            entry->next = entry;
            entry->prev = entry;
            Morph_free_list = entry;
        }
        else
        {
            entry->prev = Morph_free_list->prev;
            entry->next = Morph_free_list;
            Morph_free_list->prev->next = entry;
            Morph_free_list->prev = entry;
        }
    }
}

// 0x007F4790 (:176)
void mesh_morph_level_init_pre()
{
}

// 0x007F47A0 (:182)
void mesh_morph_level_shutdown()
{
}

// 0x0080B4E0 (:196)
// The slot index of the morph file (name without extension), -1 when it is not loaded.
int mesh_morph_find_by_name(const char* name, unsigned int srid)
{
    mesh_morph_entry* entry = nullptr;
    Mesh_morph_hash.get(name, srid, entry);
    if (entry == nullptr)
    {
        return -1;
    }
    return entry->index;
}

// 0x007F47B0 (:217)
rl_morph_container* mesh_morph_get_data(int handle)
{
    if ((unsigned int)handle < 256)
    {
        return Morph_array[handle].data;
    }
    return nullptr;
}

// 0x007F47D0 (:228)
const char* mesh_morph_get_debug_name(int handle)
{
    if ((unsigned int)handle > 0xFF)
    {
        return "out-of-bounds";
    }
    return "Names_only_in_Debug_Builds";
}
