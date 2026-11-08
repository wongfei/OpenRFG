// Original: volition/render_lib/code/rl_material_map.cpp (render half)
//
// The rl_material_map class (decompiled): a factory class (2048 preallocated objects) that maps the material indices
// of a mesh to material library handles. It lives in its own file because its factory is set up by rl_init:
// rl_material_map.cpp (rl_material_map_data_load) is in the GPU-free rfg library that the headless tests link with a
// stand-in of create/set_map_data, and this file is in rfg_render.
//
// Partial: rl_material_map_data_write and validate are not implemented.

#include "volition/render_lib/code/rl_material_map.h"

#include "volition/render_lib/code/rl_factory_rtti.h"
#include "volition/render_lib/code/rl_material.h"
#include "volition/render_lib/code/rl_material_library.h"
#include "volition/vlib/code/math/crc.h"
#include "volition/vlib/code/memory/mempool_base.h"

#include <string.h>

// :13
// <rl_material_map::factory_init> 0x00511490 (:13)
// <rl_material_map::release> 0x00511590 (:13)
// <rl_material_map::create> 0x00527D50 (:13)
RL_FACTORY_CLASS_IMPLEMENTATION(rl_material_map, 0xA55B00B5, 2048)

// 0x004C2D20 (:18)
rl_material_map::rl_material_map()
{
    m_isSet = false;
}

// 0x004C2D30 (:25)
rl_material_map::~rl_material_map()
{
    m_isSet = false;
}

// 0x007B9990 (.h:48)
void rl_material_map::set_objects_max(int objects_max)
{
    ms_class_factory_info.m_objects_max = objects_max;
}

// (.h:48) The factory's count of the maps in use (inlined away in the binary; the viewer's census reads it).
int rl_material_map::get_objects_used()
{
    return ms_class_factory_info.m_objects_max - ms_object_pool.size();
}

// 0x004C2D40 (:32)
unsigned int rl_material_map::get_material(int index) const
{
    return m_data.materials[index];
}

// 0x004C2D50 (:40)
bool rl_material_map::set_material(int index, unsigned int handle)
{
    m_data.materials[index] = handle;
    return true;
}

// 0x004E92D0 (:48)
// The index of the first material whose name checksum matches name (the materials must all be registered).
int rl_material_map::find_material_index(const char* name)
{
    const unsigned int name_checksum = crc_stri(name, 0);
    for (int i = 0; i < (int)m_data.num_materials; i++)
    {
        rl_material_base* mat_p = rl_material_library_get_material(m_data.materials[i]); // inlined
        if (name_checksum == mat_p->get_name_checksum())
        {
            return i;
        }
    }
    return -1;
}

// 0x004C2D70 (:66)
void rl_material_map::serialize(rl_serial_block&, bool)
{
}

// 0x00527DB0 (:74)
// A new map with its own copy of the handle array (from pool).
rl_material_map* rl_material_map::clone(mempool_base* pool) const
{
    rl_material_map* clone_p = create();
    if (clone_p != nullptr)
    {
        clone_p->m_data.num_materials = m_data.num_materials;
        clone_p->m_data.materials = (unsigned int*)pool->alloc(m_data.num_materials * 4, 0);
        memcpy(clone_p->m_data.materials, m_data.materials, m_data.num_materials * 4);
    }
    return clone_p;
}

// 0x00527E00 (:95)
void rl_material_map::set_map_data(rl_material_map_data* map_data)
{
    const unsigned int num_materials = map_data->num_materials;
    m_data.materials = map_data->materials.to_pointer();
    m_data.num_materials = num_materials;
    m_isSet = true;
}

// 0x004C2D80 (:87)
void rl_material_map::set_map_data(rl_material_map_data_runtime* map_data)
{
    m_data.materials = map_data->materials;
    m_data.num_materials = map_data->num_materials;
    m_isSet = true;
}
