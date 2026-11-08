// Original: volition/render_lib/code/rl_factory_rtti.h
// Implementation: volition/render_lib/code/rl_factory_rtti.cpp
//
// render_lib RTTI: rl_class_factory_info (per concrete class: id, name, create function, object pool) and
// rl_class_dynamic_info (per abstract class), linked into global lists.
#pragma once

#include "volition/render_lib/code/linked_list_p.h"
#include "volition/render_lib/code/pool_list.h"

#include <new>

class mempool;
class rl_base_object;

// size 0x30
class rl_class_factory_info
{
public:
    typedef void (*class_factory_init)(mempool&);

    rl_class_factory_info* get_prev() const;
    void set_prev(rl_class_factory_info*);
    rl_class_factory_info* get_next() const;
    void set_next(rl_class_factory_info*);
    static void register_class(rl_class_factory_info* class_factory_info_p); // 0x004DC8B0 (.h:518)
    rl_class_factory_info(const char* class_name, unsigned int class_id, void (*factory_init)(mempool&), int class_size, const pool_list<rl_base_object*>* local_object_pool_p, int objects_max); // 0x004DC910 (.h:535)
    bool get_display_exhausted_factories_warning(); // 0x004B6880 (.h:542)
    void set_display_exhausted_factories_warning(bool);
    static void increment_num_exhausted_factories(); // 0x004B6890 (.h:546)
    static void decrement_num_exhausted_factories(); // 0x004B68A0 (.h:547)
    static int get_num_exhausted_factories();

    rl_class_factory_info* prev; // 0x00
    rl_class_factory_info* next; // 0x04
    const char* m_class_name; // 0x08
    unsigned int m_class_id; // 0x0C
    int m_class_size; // 0x10
    void (*m_factory_init)(mempool&); // 0x14
    mempool* m_factory_mempool_p; // 0x18
    const pool_list<rl_base_object*>* m_local_object_pool_p; // 0x1C
    int m_objects_max; // 0x20
    bool m_display_exhausted_factories_warning; // 0x24
    unsigned char* m_object_data_base; // 0x28
    unsigned int m_object_data_size; // 0x2C
    static linked_list_p<rl_class_factory_info>* ms_class_factories; // 0x01B7BCFC
    static int ms_num_exhausted_factories; // 0x01B7BD00
};

// size 0x18
class rl_class_dynamic_info
{
public:
    rl_class_dynamic_info* get_prev() const;
    void set_prev(rl_class_dynamic_info*);
    rl_class_dynamic_info* get_next() const;
    void set_next(rl_class_dynamic_info*);
    static void register_class(rl_class_dynamic_info* class_dynamic_info_p); // 0x004DC970 (.h:564)
    rl_class_dynamic_info(const char* class_name, int class_size); // 0x004DC9D0 (.h:575)

    rl_class_dynamic_info* prev; // 0x00
    rl_class_dynamic_info* next; // 0x04
    const char* m_class_name; // 0x08
    int m_class_size; // 0x0C
    int m_objects_peak; // 0x10
    int m_objects_allocated; // 0x14
    static linked_list_p<rl_class_dynamic_info>* ms_dynamic_classes; // 0x01B7BD0C
};

// The class factory macros of render_lib (names inferred; the expansions are recorded at the class's .cpp line, for
// example rl_shader.cpp:6). A factory class owns a pool of objects_max preallocated objects carved from the factory
// mempool by factory_init (called by ::factory_init during rl_init); create() pops one, release() destructs it in
// place and pushes it back.
//
// The class ids, names, sizes and pool maxima are the constant-initialized rl_class_factory_info objects in .data.
#define RL_FACTORY_CLASS_IMPLEMENTATION(CLASS, CLASS_ID, OBJECTS_MAX) \
    RL_FACTORY_CLASS_CORE(CLASS, CLASS_ID, OBJECTS_MAX) \
    RL_FACTORY_CLASS_RELEASE(CLASS)

// Everything but release(), for classes whose release differs (scene entities defer it to rl_add_deferred_op while
// render_lib is locked).
#define RL_FACTORY_CLASS_CORE(CLASS, CLASS_ID, OBJECTS_MAX) \
    const unsigned int CLASS::m___class_id = CLASS_ID; \
    pool_list<CLASS*> CLASS::ms_object_pool; \
    rl_class_factory_info CLASS::ms_class_factory_info(#CLASS, CLASS_ID, &CLASS::factory_init, sizeof(CLASS), (const pool_list<rl_base_object*>*)&CLASS::ms_object_pool, OBJECTS_MAX); \
    const pool_list<CLASS*>& CLASS::get_object_pool() \
    { \
        return ms_object_pool; \
    } \
    int CLASS::get_objects_max() \
    { \
        return ms_class_factory_info.m_objects_max; \
    } \
    void CLASS::factory_init(mempool& factory_mempool) \
    { \
        ms_class_factory_info.m_factory_mempool_p = &factory_mempool; \
        CLASS** elements = (CLASS**)factory_mempool.alloc(ms_class_factory_info.m_objects_max * sizeof(CLASS*), 4); \
        ms_object_pool.init(elements, ms_class_factory_info.m_objects_max, 0); \
        /* Magic number: objects are 16-byte aligned in the factory mempool. */ \
        CLASS* objects = (CLASS*)factory_mempool.alloc(ms_class_factory_info.m_objects_max * sizeof(CLASS), 16); \
        for (int i = 0; i < ms_class_factory_info.m_objects_max; i++) \
        { \
            ms_object_pool.add(&objects[i]); \
        } \
        ms_class_factory_info.m_object_data_base = (unsigned char*)objects; \
        ms_class_factory_info.m_object_data_size = ms_class_factory_info.m_objects_max * sizeof(CLASS); \
    } \
    CLASS* CLASS::create_placement(CLASS* _data_p) \
    { \
        CLASS* object_p = _data_p != nullptr ? new (_data_p) CLASS() : nullptr; \
        /* Notes (decompiled behaviour, kept as is): the class id is written even when _data_p is null. */ \
        object_p->set_leaf_class_id(CLASS_ID); \
        return object_p; \
    } \
    CLASS* CLASS::create() \
    { \
        if (ms_object_pool.size() < 1) \
        { \
            return nullptr; \
        } \
        CLASS* data_p = ms_object_pool.remove_back(); \
        CLASS* object_p = data_p != nullptr ? new (data_p) CLASS() : nullptr; \
        object_p->set_leaf_class_id(CLASS_ID); \
        if (ms_object_pool.size() < 1 && ms_class_factory_info.get_display_exhausted_factories_warning()) \
        { \
            rl_class_factory_info::increment_num_exhausted_factories(); \
        } \
        return object_p; \
    } \
    bool CLASS::can_create() \
    { \
        return ms_object_pool.size() > 0; \
    }

#define RL_FACTORY_CLASS_RELEASE(CLASS) \
    void CLASS::release() \
    { \
        if (ms_class_factory_info.m_factory_mempool_p->contains_address(this)) \
        { \
            if (ms_object_pool.size() < 1 && ms_class_factory_info.get_display_exhausted_factories_warning()) \
            { \
                rl_class_factory_info::decrement_num_exhausted_factories(); \
            } \
            ms_object_pool.add(this); \
        } \
        this->~CLASS(); \
    }

// A dynamic class is allocated from the heap and counted in its rl_class_dynamic_info.
#define RL_DYNAMIC_CLASS_INFO(CLASS) \
    rl_class_dynamic_info CLASS::ms_class_dynamic_info(#CLASS, sizeof(CLASS));

#define RL_DYNAMIC_CLASS_CREATE(CLASS) \
    CLASS* CLASS::create() \
    { \
        CLASS* object_p = new CLASS(); \
        object_p->set_leaf_class_id(0xFFFFFFFF); \
        ms_class_dynamic_info.m_objects_allocated++; \
        if (ms_class_dynamic_info.m_objects_peak < ms_class_dynamic_info.m_objects_allocated) \
        { \
            ms_class_dynamic_info.m_objects_peak = ms_class_dynamic_info.m_objects_allocated; \
        } \
        return object_p; \
    } \
    bool CLASS::can_create() \
    { \
        return true; \
    }

#define RL_DYNAMIC_CLASS_RELEASE(CLASS) \
    void CLASS::release() \
    { \
        ms_class_dynamic_info.m_objects_allocated--; \
        if (this != nullptr) \
        { \
            delete this; \
        } \
    }

#define RL_DYNAMIC_CLASS_IMPLEMENTATION(CLASS) \
    RL_DYNAMIC_CLASS_INFO(CLASS) \
    RL_DYNAMIC_CLASS_CREATE(CLASS) \
    RL_DYNAMIC_CLASS_RELEASE(CLASS)
