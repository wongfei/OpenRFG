// Original: volition/render_lib/code/rl_clone_mesh_decal.h
// Implementation: volition/render_lib/code/rl_clone_mesh_decal.cpp
//
// rl_clone_mesh_decal, decals drawn with clone mesh instancing (rl_clone_mesh_decal_parameters). Placement of the
// parameters inferred.
#pragma once

#include "volition/render_lib/code/pool_list.h"
#include "volition/render_lib/code/rl_decal.h"

class matrix;
class mempool;
class rl_class_factory_info;
class rl_clone_mesh_element;
class rl_clone_mesh_pool;
class rl_decal_parameters;
class vector;

// size 0x9C
class rl_clone_mesh_decal : public rl_decal
{
protected:
    rl_clone_mesh_decal();
    virtual ~rl_clone_mesh_decal(); // vtbl+0x00
private:
    static const pool_list<rl_clone_mesh_decal*>& get_object_pool();
public:
    static void set_objects_max(int);
    static int get_objects_max();
    static int get_objects_used();
    static int get_objects_free();
    static void factory_init(mempool&);
    static rl_clone_mesh_decal* create_placement(rl_clone_mesh_decal*);
    static rl_clone_mesh_decal* create();
    static bool can_create();
    virtual void release(); // vtbl+0x04
    void init(const rl_decal_parameters*, const vector&, const matrix&);

    static const unsigned int m___class_id; // 0x012BECE4
    static rl_class_factory_info ms_class_factory_info;
protected:
    static pool_list<rl_clone_mesh_decal*> ms_object_pool;
    rl_clone_mesh_element* m_clone; // 0x94
    rl_clone_mesh_pool* m_parent_pool; // 0x98
};

// size 0x14
struct rl_clone_mesh_decal_parameters
{
    rl_clone_mesh_pool* m_parent_pool_p; // 0x00
    float m_instance_data[4]; // 0x04
};
