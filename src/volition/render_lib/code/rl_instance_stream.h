// Original: volition/render_lib/code/rl_instance_stream.h
// Implementation: volition/render_lib/code/rl_instance_stream.cpp
//
// rl_mesh_instance_stream, a stream of per-instance transforms for instanced drawing.
#pragma once

#include "volition/render_lib/code/pool_list.h"
#include "volition/render_lib/code/rl_base_object.h"

class mempool;
class rl_class_factory_info;
struct rl_gpu_mesh_instance;
struct rl_mesh_instance_info;

// size 0x1C
class rl_mesh_instance_stream : public rl_base_object
{
protected:
    rl_mesh_instance_stream(); // 0x004C2220 (:27)
    virtual ~rl_mesh_instance_stream(); // vtbl+0x00 0x004C2240 (:38)
private:
    static const pool_list<rl_mesh_instance_stream*>& get_object_pool(); // 0x004BA0E0 (.h:44)
public:
    static void set_objects_max(int);
    static int get_objects_max(); // 0x004BA0F0 (.h:44)
    static int get_objects_used();
    static int get_objects_free();
    static void factory_init(mempool& factory_mempool); // 0x005102F0 (:7)
    static rl_mesh_instance_stream* create_placement(rl_mesh_instance_stream* _data_p); // 0x004E85F0 (:7)
    static rl_mesh_instance_stream* create(); // 0x00526910 (:7)
    static bool can_create(); // 0x004E8630 (:7)
    virtual void release(); // vtbl+0x04 0x005103F0 (:7)
    bool init(); // 0x004E8680 (:46)
    void finalize(); // 0x004C22C0 (:150)
    int add_instance(const rl_mesh_instance_info& inst); // 0x004E86D0 (:66)
    int add_instances(const rl_mesh_instance_info* insts, int num_instances); // 0x004E8830 (:102)
    void set_as_render_instance_stream(); // 0x004C2270 (:124)
    int get_num_unique_instances() const;
    const rl_gpu_mesh_instance* get_instances() const; // 0x004BA100 (.h:63)
private:
    rl_gpu_mesh_instance* allocate_instances(int& max_allocated); // 0x004C2280 (:138)

public:
    static const unsigned int m___class_id; // 0x012BECA4
    static rl_class_factory_info ms_class_factory_info; // 0x01514BEC
protected:
    static pool_list<rl_mesh_instance_stream*> ms_object_pool; // 0x01514C1C
public:
    static const int RENDER_STREAM_INDEX; // 0x012BECA8
    static const int DYNAMIC_INSTB_INDEX; // 0x012BECAC
private:
    rl_gpu_mesh_instance* m_instances; // 0x08
    int m_max_instances; // 0x0C
    volatile unsigned int m_num_instances; // 0x10
    unsigned int m_sizeof_alloc; // 0x14
    void* m_base_ptr; // 0x18
};
