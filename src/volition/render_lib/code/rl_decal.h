// Original: volition/render_lib/code/rl_decal.h
// Implementation: volition/render_lib/code/rl_decal.cpp
//
// rl_decal, a projected decal on a renderable instance.
#pragma once

#include "volition/render_lib/code/rl_aabb.h"
#include "volition/render_lib/code/rl_base_object.h"
#include "volition/render_lib/code/rl_color.h"
#include "volition/render_lib/code/rl_decal_defines.h"
#include "volition/vlib/code/math/matrix.h"
#include "volition/vlib/code/math/vector3.h"

class rl_decal_deferred_info;
class rl_decal_parameters;
class rl_renderable_instance;
class rl_renderer;

// size 0x94
class rl_decal : public rl_base_object
{
protected:
    rl_decal(); // 0x004E3F80 (:62)
    virtual ~rl_decal(); // vtbl+0x00 0x004BD9D0 (:66)
public:
    void init(const rl_decal_parameters* params); // 0x004E3FD0 (:74)
    void set_flags(unsigned int);
    void clear_flags(unsigned int);
    bool check_flags(unsigned int _flags) const; // 0x004BBE20 (.h:43)
    rl_decal_type get_type() const; // 0x004BBE30 (.h:45)
    const rl_aabb& get_aabb() const; // 0x004BBE40 (.h:47)
    const rl_color_float& get_color() const; // 0x004BBE50 (.h:49)
    void update_color(const rl_color_float& new_color); // 0x004BBE60 (.h:50)
    void fade_out(int fadetime_ms); // 0x004E4030 (:92)
    float get_fade_pct() const; // 0x004BBE80 (.h:54)
    void update_fade_pct(float new_fade_pct); // 0x004BBE90 (.h:55)
    float get_age_ms() const; // 0x004E4060 (:101)
    void set_decal_handle(unsigned int handle); // 0x004BBEB0 (.h:59)
    unsigned int get_decal_handle(); // 0x004BBEC0 (.h:60)
    void set_generic_ri_handle(unsigned int handle); // 0x004BBED0 (.h:62)
    unsigned int get_generic_ri_handle() const; // 0x004BBEE0 (.h:63)
    void set_owner(rl_renderable_instance* owner); // 0x004BD9E0 (:87)
    rl_renderable_instance* get_owner() const; // 0x004BBEF0 (.h:66)
    virtual unsigned int get_material() const; // vtbl+0x08 0x004E3F90 (.h:68)

    rl_decal* sys_prev; // 0x08
    rl_decal* sys_next; // 0x0C
    rl_decal* next; // 0x10
    rl_decal* prev; // 0x14
    vector m_relative_pos; // 0x18
    matrix m_relative_orient; // 0x24
    int m_birth_time; // 0x48
    int m_fade_time; // 0x4C
    int m_lifetime; // 0x50
protected:
    rl_decal_type m_decal_type; // 0x54
    unsigned int m_decal_flags; // 0x58
    rl_aabb m_local_aabb; // 0x5C
    rl_color_float m_color; // 0x74
    float m_fade_pct; // 0x84
    rl_renderable_instance* m_owner; // 0x88
    unsigned int m_generic_ri_handle; // 0x8C
    unsigned int m_handle; // 0x90
};

unsigned int rl_decal_register(rl_decal* decal_p, unsigned int existing_handle); // 0x005236D0 (:110)
void rl_decal_unregister(rl_decal* decal_p); // 0x0050C510 (:163)
void rl_decal_release(unsigned int handle); // 0x00539080 (:189)
rl_decal* rl_decal_create(const rl_decal_parameters* params, const vector& parent_pos, const matrix& parent_orient); // 0x00539140 (:226)
bool rl_decal_maybe_defer_creation(unsigned int& reserved_decal_handle, const rl_decal_parameters* params, rl_renderable_instance* inst_p); // 0x00539190 (:253)
void rl_decal_process_deferred_create(rl_decal_deferred_info* info); // 0x005237C0 (:279)
void rl_decal_setup_decal_for_render(rl_renderer& renderer, rl_decal* decal_p); // 0x0050C590 (:287)
void rl_decal_setup_decal_instance_for_render(rl_renderer& renderer, rl_decal* decal_p); // 0x0050C5C0 (:302)
void rl_decal_get_local_aabb(rl_aabb& local_aabb, const rl_decal_parameters* decal_params); // 0x004BD9F0 (:316)
void rl_decal_update_color(unsigned int handle, const rl_color_float& new_color, unsigned int gen_handle); // 0x0050C5F0 (:333)
void rl_decal_fade_out(unsigned int handle, int fadetime_ms, unsigned int gen_handle); // 0x0050C660 (:353)
void rl_decal_process_decals(rl_decal** decal_list); // 0x0050C6F0 (:374)

// ---- inline members

inline void rl_decal::set_flags(unsigned int flags)
{
    m_decal_flags |= flags;
}

inline void rl_decal::clear_flags(unsigned int flags)
{
    m_decal_flags &= ~flags;
}

// 0x004BBE20 (.h:43)
inline bool rl_decal::check_flags(unsigned int _flags) const
{
    return (m_decal_flags & _flags) != 0;
}

// 0x004BBE30 (.h:45)
inline rl_decal_type rl_decal::get_type() const
{
    return m_decal_type;
}

// 0x004BBE40 (.h:47)
inline const rl_aabb& rl_decal::get_aabb() const
{
    return m_local_aabb;
}

// 0x004BBE50 (.h:49)
inline const rl_color_float& rl_decal::get_color() const
{
    return m_color;
}

// 0x004BBE60 (.h:50)
inline void rl_decal::update_color(const rl_color_float& new_color)
{
    m_color = new_color;
}

// 0x004BBE80 (.h:54)
inline float rl_decal::get_fade_pct() const
{
    return m_fade_pct;
}

// 0x004BBE90 (.h:55)
inline void rl_decal::update_fade_pct(float new_fade_pct)
{
    m_fade_pct = new_fade_pct;
}

// 0x004BBEB0 (.h:59)
inline void rl_decal::set_decal_handle(unsigned int handle)
{
    m_handle = handle;
}

// 0x004BBEF0 (.h:66)
inline rl_renderable_instance* rl_decal::get_owner() const
{
    return m_owner;
}

// 0x004BBEC0 (.h:60)
inline unsigned int rl_decal::get_decal_handle()
{
    return m_handle;
}

// 0x004BBED0 (.h:62)
inline void rl_decal::set_generic_ri_handle(unsigned int handle)
{
    m_generic_ri_handle = handle;
}

// 0x004BBEE0 (.h:63)
inline unsigned int rl_decal::get_generic_ri_handle() const
{
    return m_generic_ri_handle;
}
