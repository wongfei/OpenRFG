// Original: volition/render_lib/code/rl_factory_rtti.cpp
//
// The class registries of render_lib: every rl_class_factory_info and rl_class_dynamic_info registers itself in a
// circular list during static initialization. ::factory_init (rl_renderer_core.cpp) walks the factory list.

#include "volition/render_lib/code/rl_factory_rtti.h"

#include <stdlib.h>

linked_list_p<rl_class_factory_info>* rl_class_factory_info::ms_class_factories = nullptr; // 0x01B7BCFC
int rl_class_factory_info::ms_num_exhausted_factories = 0; // 0x01B7BD00
linked_list_p<rl_class_dynamic_info>* rl_class_dynamic_info::ms_dynamic_classes = nullptr; // 0x01B7BD0C

// Appends elem_p to the circular list (linked_list_p::append, inline in both register_class functions).
template<typename T>
static void append_to_class_list(linked_list_p<T>*& list_p, T* elem_p)
{
    if (list_p == nullptr)
    {
        // The list is malloc'ed: it is created during static initialization, before any allocator exists.
        list_p = (linked_list_p<T>*)malloc(sizeof(linked_list_p<T>));
        if (list_p != nullptr)
        {
            new (list_p) linked_list_p<T>();
        }
    }
    list_p->append(elem_p);
}

// 0x004DC8B0 (.h:518)
void rl_class_factory_info::register_class(rl_class_factory_info* class_factory_info_p)
{
    append_to_class_list(ms_class_factories, class_factory_info_p);
}

// 0x004DC910 (.h:535)
rl_class_factory_info::rl_class_factory_info(const char* class_name, unsigned int class_id, void (*factory_init)(mempool&), int class_size, const pool_list<rl_base_object*>* local_object_pool_p, int objects_max)
{
    m_class_name = class_name;
    m_class_id = class_id;
    m_class_size = class_size;
    m_factory_init = factory_init;
    m_factory_mempool_p = nullptr;
    m_local_object_pool_p = local_object_pool_p;
    m_objects_max = objects_max;
    m_display_exhausted_factories_warning = true;
    register_class(this);
    m_object_data_base = nullptr;
    m_object_data_size = 0;
}

// 0x004B6880 (.h:542)
bool rl_class_factory_info::get_display_exhausted_factories_warning()
{
    return m_display_exhausted_factories_warning;
}

// 0x004B6890 (.h:546)
void rl_class_factory_info::increment_num_exhausted_factories()
{
    ms_num_exhausted_factories++;
}

// 0x004B68A0 (.h:547)
void rl_class_factory_info::decrement_num_exhausted_factories()
{
    ms_num_exhausted_factories--;
}

// 0x004DC970 (.h:564)
void rl_class_dynamic_info::register_class(rl_class_dynamic_info* class_dynamic_info_p)
{
    append_to_class_list(ms_dynamic_classes, class_dynamic_info_p);
}

// 0x004DC9D0 (.h:575)
rl_class_dynamic_info::rl_class_dynamic_info(const char* class_name, int class_size)
{
    m_class_name = class_name;
    m_class_size = class_size;
    register_class(this);
    m_objects_peak = 0;
    m_objects_allocated = 0;
}
