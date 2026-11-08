// Original: volition/rfg/code/objects/object.h (template functions)
//
// object_stream_resource<T>: the data that exists only while an object is streamed in, with its loaded flag.
#pragma once

// <character_instance::resource_dependent_data> 0x007FA440 (.h:209)
template<typename T>
void object_stream_resource<T>::set_loaded(bool loaded)
{
    m_loaded = loaded;
}

// <character_instance::resource_dependent_data> 0x005F5A20 (.h:214)
template<typename T>
bool object_stream_resource<T>::is_loaded() const
{
    return m_loaded;
}

// <character_instance::resource_dependent_data> 0x006C9660 (.h:232)
// nullptr while not loaded.
template<typename T>
T* object_stream_resource<T>::get_data()
{
    return m_loaded == true ? &m_data : nullptr;
}

template<typename T>
const T* object_stream_resource<T>::get_data() const
{
    return m_loaded == true ? &m_data : nullptr;
}
