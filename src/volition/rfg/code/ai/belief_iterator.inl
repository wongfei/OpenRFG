// Original: volition/rfg/code/ai/belief_iterator.inl
//
// Template bodies of belief_obj_iterator (stub_impl.md S9). get_obj is specialized in belief_iterator.cpp for <object>;
// the other instances filter on the object's type, as rfg.exe inlines them.
#pragma once

#include "volition/rfg/code/ai/ai_system.h"
#include "volition/rfg/code/ai/beliefs.h"
#include "volition/rfg/code/ai/percept.h"
#include "volition/rfg/code/level/world.h"
#include "volition/rfg/code/objects/humans/player.h"

// .h:32
template<typename T>
belief_obj_iterator<T>::belief_obj_iterator()
    : per(nullptr), belief(nullptr), obj(nullptr)
{
}

// (:12) World.object_cast<T>: null for a dead handle or another type.
template<typename T>
T* belief_obj_iterator<T>::get_obj(unsigned int handle)
{
    object* objp = World.object_from_handle(handle);
    return objp != nullptr ? World.object_cast<T>(objp) : nullptr;
}

// 0x005E0380 (belief_iterator.cpp:12)
template<>
inline object* belief_obj_iterator<object>::get_obj(unsigned int handle)
{
    return World.object_from_handle(handle);
}

// belief_iterator.inl:24
template<typename T>
void belief_obj_iterator<T>::seek_obj()
{
    while (belief != nullptr)
    {
        obj = get_obj(belief->handle);
        if (obj != nullptr)
        {
            return;
        }
        belief = per->get_next_belief(belief);
    }
    obj = nullptr;
}

// belief_iterator.inl:54
template<typename T>
void belief_obj_iterator<T>::init(const ai_system* ai, ai_belief_types type)
{
    per = &ai->perception;
    belief = per->get_first_belief(type);
    obj = nullptr;
    if (belief != nullptr)
    {
        seek_obj();
    }
}

// .h:44
template<typename T>
belief_obj_iterator<T>::operator bool() const
{
    return obj != nullptr;
}

// .h:49
template<typename T>
T* belief_obj_iterator<T>::operator->() const
{
    return obj;
}

template<typename T>
belief_obj_iterator<T>::operator T*() const
{
    return obj;
}

// belief_iterator.inl:66
template<typename T>
belief_obj_iterator<T>& belief_obj_iterator<T>::operator++()
{
    if (belief != nullptr)
    {
        belief = per->get_next_belief(belief);
        obj = nullptr;
        if (belief != nullptr)
        {
            seek_obj();
        }
    }
    return *this;
}

// .h:58
template<typename T>
const ai_belief* belief_obj_iterator<T>::get_belief() const
{
    return belief;
}
