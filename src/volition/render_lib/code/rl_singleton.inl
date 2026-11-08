// Original: volition/render_lib/code/rl_singleton.h (inline template members; included at the end of rl_singleton.h)
//
// The constructor registers the object as the instance and the destructor clears it. rl_singleton_phoenix::get_instance
// recreates the instance through T::create() when it is gone.
#pragma once

template<typename T>
T* rl_singleton<T>::ms_instance_p = nullptr;

template<typename T>
T* rl_singleton_phoenix<T>::ms_instance_p = nullptr;

// <rl_draw> 0x004D8620 (.h:41)
template<typename T>
rl_singleton<T>::rl_singleton()
{
    ms_instance_p = static_cast<T*>(this);
}

// <rl_draw> 0x004D8640 (.h:47)
template<typename T>
rl_singleton<T>::~rl_singleton()
{
    ms_instance_p = nullptr;
}

// <rl_draw> 0x004D8660 (.h:53)
template<typename T>
T* rl_singleton<T>::get_instance()
{
    return ms_instance_p;
}

// <rl_star_field> 0x004D9330 (.h:93)
template<typename T>
rl_singleton_phoenix<T>::rl_singleton_phoenix()
{
    ms_instance_p = static_cast<T*>(this);
}

// <rl_star_field> 0x004D9350 (.h:99)
template<typename T>
rl_singleton_phoenix<T>::~rl_singleton_phoenix()
{
    ms_instance_p = nullptr;
}

// <rl_star_field> 0x0051F6F0 (.h:105)
template<typename T>
T* rl_singleton_phoenix<T>::get_instance()
{
    if (ms_instance_p == nullptr)
    {
        ms_instance_p = T::create();
    }
    return ms_instance_p;
}
