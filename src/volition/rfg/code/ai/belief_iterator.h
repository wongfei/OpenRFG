// Original: volition/rfg/code/ai/belief_iterator.h
// Implementation: volition/rfg/code/ai/belief_iterator.cpp
//
// belief_obj_iterator<T>: iterates the beliefs of an npc's perception of one belief type, skipping beliefs whose
// object is gone or not a T (object, human, npc, player, vehicle, turret, rfg_mover).
#pragma once

#include "volition/rfg/code/ai/ai_types.h"

struct ai_belief;
class ai_perception;
class ai_system;

// size (<object>) 0xC
template<typename T>
class belief_obj_iterator
{
public:
    typedef T value_type;

    belief_obj_iterator(const belief_obj_iterator<T>&);
    belief_obj_iterator(); // <object> 0x005F60D0 (.h:32)
    belief_obj_iterator<T>& operator=(const belief_obj_iterator<T>&);
    void init(const ai_system* ai, ai_belief_types type); // <object> 0x00649580 (belief_iterator.inl:54)
    operator bool() const; // <object> 0x005F60E0 (.h:44)
    bool operator!() const;
    T& operator*() const;
    T* operator->() const; // <object> 0x005F60F0 (.h:49)
    operator T*() const;
    belief_obj_iterator<T> operator++(int);
    belief_obj_iterator<T>& operator++(); // <object> 0x006495B0 (belief_iterator.inl:66)
    const ai_belief* get_belief() const; // <object> 0x005F6110 (.h:58)
    float get_relevance() const;
private:
    static T* get_obj(unsigned int handle); // <object> 0x005E0380 (:12)
    void seek_obj(); // <object> 0x005FA370 (belief_iterator.inl:24)

    const ai_perception* per; // 0x00
    const ai_belief* belief; // 0x04
    T* obj; // 0x08
};

// template instance: void __thiscall belief_obj_iterator<turret>::belief_obj_iterator<turret>(); // 0x005F67E0 (.h:32)
// template instance: void __thiscall belief_obj_iterator<player>::belief_obj_iterator<player>(); // 0x005F6E90 (.h:32)
// template instance: void __thiscall belief_obj_iterator<object>::belief_obj_iterator<object>(); // 0x005F60D0 (.h:32)
// template instance: void __thiscall belief_obj_iterator<vehicle>::belief_obj_iterator<vehicle>(); // 0x005F6940 (.h:32)
// template instance: void __thiscall belief_obj_iterator<human>::belief_obj_iterator<human>(); // 0x005F6680 (.h:32)
// template instance: void __thiscall belief_obj_iterator<rfg_mover>::belief_obj_iterator<rfg_mover>(); // 0x005F6870 (.h:32)
// template instance: void __thiscall belief_obj_iterator<npc>::belief_obj_iterator<npc>(); // 0x005F66D0 (.h:32)

#include "volition/rfg/code/ai/belief_iterator.inl"
