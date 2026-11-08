// Original: volition/render_lib/code/rl_material_map.h
// Implementation: volition/render_lib/code/rl_material_map.cpp
//
// rl_material_map, maps material indices of a mesh to materials (serialized as rl_material_map_data).
#pragma once

#include "volition/render_lib/code/pool_list.h"
#include "volition/render_lib/code/rl_base_object.h"
#include "volition/vlib/code/util/endian_type.h"

struct cfile;
class mempool;
class mempool_base;
class rl_class_factory_info;
struct rl_material_map_data;
class rl_serial_block;

// size 0x8
struct rl_material_map_data_runtime
{
    rl_material_map_data_runtime(const rl_material_map_data& in_data); // 0x005216B0 (.h:31)
    rl_material_map_data_runtime(); // 0x004BBB20 (.h:28)
    void fill(const rl_material_map_data& in_data); // 0x00509F30 (.h:41)

    unsigned int* materials; // 0x00
    unsigned int num_materials; // 0x04
};

// size 0x14
class rl_material_map : public rl_base_object
{
    // Access inferred from rl_clone_mesh_instance::render (it reads m_isSet and the material handles).
    friend class rl_clone_mesh_instance;
    // Access inferred (stub_impl.md S3): rl_destroyable_model_instance::compile_cache_data reads m_isSet and the handles.
    friend class rl_destroyable_model_instance;
protected:
    rl_material_map(); // 0x004C2D20 (:18)
    virtual ~rl_material_map(); // vtbl+0x00 0x004C2D30 (:25)
private:
    static const pool_list<rl_material_map*>& get_object_pool(); // 0x004BBB30 (.h:48)
public:
    static void set_objects_max(int objects_max); // 0x007B9990 (.h:48)
    static int get_objects_max(); // 0x004BBB40 (.h:48)
    static int get_objects_used();
    static int get_objects_free();
    static void factory_init(mempool& factory_mempool); // 0x00511490 (:13)
    static rl_material_map* create_placement(rl_material_map* _data_p); // 0x004E9260 (:13)
    static rl_material_map* create(); // 0x00527D50 (:13)
    static bool can_create(); // 0x004E9290 (:13)
    virtual void release(); // vtbl+0x04 0x00511590 (:13)
    unsigned int get_material(int index) const; // 0x004C2D40 (:32)
    bool set_material(int index, unsigned int handle); // 0x004C2D50 (:40)
    int find_material_index(const char* name); // 0x004E92D0 (:48)
    int get_num_materials() const; // 0x004BBB50 (.h:60)
    void serialize(rl_serial_block&, bool); // 0x004C2D70 (:66)
    rl_material_map* clone(mempool_base* pool) const; // 0x00527DB0 (:74)
    void set_map_data(rl_material_map_data* map_data); // 0x00527E00 (:95)
    void set_map_data(rl_material_map_data_runtime* map_data); // 0x004C2D80 (:87)
    bool validate() const;

    static const unsigned int m___class_id; // 0x012B80DC
    static rl_class_factory_info ms_class_factory_info; // 0x01514D6C
protected:
    static pool_list<rl_material_map*> ms_object_pool; // 0x01514D9C
private:
    rl_material_map_data_runtime m_data; // 0x08
    bool m_isSet; // 0x10
};

// size 0x8
struct rl_material_map_data
{
    et_ptr_offset<unsigned int,0> materials; // 0x00
    unsigned int num_materials; // 0x04
};

// 0x005216B0 (.h:31)
inline rl_material_map_data_runtime::rl_material_map_data_runtime(const rl_material_map_data& in_data)
{
    materials = in_data.materials.to_pointer();
    num_materials = in_data.num_materials;
}

// 0x004BBB20 (.h:28)
inline rl_material_map_data_runtime::rl_material_map_data_runtime()
{
}

// 0x00509F30 (.h:41)
inline void rl_material_map_data_runtime::fill(const rl_material_map_data& in_data)
{
    materials = in_data.materials.to_pointer();
    num_materials = in_data.num_materials;
}

// 0x004BBB50 (.h:60)
inline int rl_material_map::get_num_materials() const
{
    return m_isSet ? (int)m_data.num_materials : 0;
}

bool rl_material_map_data_write(cfile* f, rl_material_map_data* data); // 0x004E9350 (:108)
rl_material_map_data* rl_material_map_data_load(unsigned char* buf, int& offset); // 0x0053F1A0 (:126)
