// Original: volition/render_lib/code/rl_planar_decal.h
// Implementation: volition/render_lib/code/rl_planar_decal.cpp
//
// rl_planar_decal, a decal on a plane (rl_planar_decal_parameters). Placement of the parameters inferred.
#pragma once

#include "volition/render_lib/code/pool_list.h"
#include "volition/render_lib/code/rl_decal.h"

class matrix;
class mempool;
class rl_class_factory_info;
class rl_decal_parameters;
class rl_renderer;
class vector;

// size 0xC0
class rl_planar_decal : public rl_decal
{
protected:
    rl_planar_decal(); // 0x004F07B0 (:47)
    virtual ~rl_planar_decal(); // vtbl+0x00 0x004C44B0 (:52)
private:
    static const pool_list<rl_planar_decal*>& get_object_pool(); // 0x004BCB40 (.h:35)
public:
    static void set_objects_max(int);
    static int get_objects_max(); // 0x004BCB50 (.h:35)
    static int get_objects_used();
    static int get_objects_free(); // 0x004E3960 (.h:35)
    static void factory_init(mempool& factory_mempool); // 0x00513FD0 (:44)
    static rl_planar_decal* create_placement(rl_planar_decal* _data_p); // 0x005140D0 (:44)
    static rl_planar_decal* create(); // 0x0052C610 (:44)
    static bool can_create(); // 0x004F07A0 (:44)
    virtual void release(); // vtbl+0x04 0x00514100 (:44)
    void init(const rl_decal_parameters* params, const vector& parent_pos, const matrix& parent_orient); // 0x004F07F0 (:60)
    float get_width() const; // 0x004BCB60 (.h:40)
    float get_length() const; // 0x004BCB70 (.h:41)
    float get_depth() const; // 0x004BCB80 (.h:42)
    float get_min_fade_angle() const; // 0x004BCB90 (.h:43)
    float get_max_fade_angle() const; // 0x004BCBA0 (.h:44)
    float get_min_fade_depth() const; // 0x004BCBB0 (.h:45)
    float get_max_fade_depth() const; // 0x004BCBC0 (.h:46)
    virtual unsigned int get_material() const; // vtbl+0x08 0x004C4520 (.h:48)

    static const unsigned int m___class_id; // 0x012BECE0
    static rl_class_factory_info ms_class_factory_info; // 0x01514FB0
protected:
    static pool_list<rl_planar_decal*> ms_object_pool; // 0x01514FE0
public:
    rl_planar_decal* reclaim_next; // 0x94
    rl_planar_decal* reclaim_prev; // 0x98
    bool m_added_to_reclaim_list; // 0x9C
protected:
    float m_width; // 0xA0
    float m_length; // 0xA4
    float m_depth; // 0xA8
    float m_min_fade_angle; // 0xAC
    float m_max_fade_angle; // 0xB0
    float m_min_fade_depth; // 0xB4
    float m_max_fade_depth; // 0xB8
    unsigned int m_material; // 0xBC
};

// size 0x24
struct rl_planar_decal_parameters
{
    float m_width; // 0x00
    float m_length; // 0x04
    float m_size; // 0x08
    float m_depth; // 0x0C
    float m_min_fade_angle; // 0x10
    float m_max_fade_angle; // 0x14
    float m_min_fade_depth; // 0x18
    float m_max_fade_depth; // 0x1C
    unsigned int m_material_handle; // 0x20
};

rl_decal* rl_planar_decal_create(const rl_decal_parameters* params, const vector& parent_pos, const matrix& parent_orient); // 0x0052C680 (:83)
void rl_planar_decal_setup_decal_for_render(rl_renderer& renderer, rl_planar_decal* decal_p); // 0x004F0A10 (:120)
void rl_planar_decal_setup_decal_instance_for_render(rl_renderer& renderer, rl_planar_decal* decal_p); // 0x004F0C60 (:152)
