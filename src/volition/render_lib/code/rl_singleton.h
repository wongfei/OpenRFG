// Original: volition/render_lib/code/rl_singleton.h
//
// rl_singleton<T> and rl_singleton_phoenix<T> (recreated on demand), the singleton bases of rl_render_lib, rl_draw,
// rl_shader_library, rl_primitive_renderer, rl_particle_renderer, rl_star_field, rl_shadow_system and
// rl_meteor_shower. Written by hand; the addresses are of the instantiations named per member.
#pragma once

// size 0x4
template<typename T>
class rl_singleton
{
protected:
    rl_singleton(); // <rl_draw> 0x004D8620 (.h:41)
    virtual ~rl_singleton(); // vtbl+0x00 <rl_draw> 0x004D8640 (.h:47)
public:
    static T* get_instance(); // <rl_draw> 0x004D8660 (.h:53)
    static void set_instance(T*);
    static void release_instance();

    // 0x00 vfptr
protected:
    static T* ms_instance_p; // <rl_draw> 0x01CD26F8
};

// size 0x4
template<typename T>
class rl_singleton_phoenix
{
protected:
    rl_singleton_phoenix(); // <rl_star_field> 0x004D9330 (.h:93)
    virtual ~rl_singleton_phoenix(); // vtbl+0x00 <rl_star_field> 0x004D9350 (.h:99)
public:
    static T* get_instance(); // <rl_star_field> 0x0051F6F0 (.h:105)
    static void set_instance(T*);
    static void release_instance();

    // 0x00 vfptr
protected:
    static T* ms_instance_p; // <rl_star_field> 0x01CD2708
};

#include "volition/render_lib/code/rl_singleton.inl"
