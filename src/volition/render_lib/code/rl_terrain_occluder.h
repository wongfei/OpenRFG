// Original: volition/render_lib/code/rl_terrain_occluder.h
// Implementation: volition/render_lib/code/rl_terrain_occluder.cpp
//
// rl_terrain_occluder, terrain occluders generated from height data.
#pragma once

#include "volition/render_lib/code/pool_list.h"
#include "volition/render_lib/code/rl_frustum.h"
#include "volition/render_lib/code/rl_occluder.h"
#include "volition/vlib/code/math/plane.h"
#include "volition/vlib/code/math/vector2.h"
#include "volition/vlib/code/math/vector3.h"

class mempool;
class rl_3d_entity;
class rl_aabb;
class rl_class_factory_info;
class rl_partition_sector;
class rl_scene_entity;

// size 0x390
class rl_terrain_occluder : public rl_occluder
{
protected:
    rl_terrain_occluder(); // 0x00546F00 (.h:117)
    virtual ~rl_terrain_occluder(); // vtbl+0x00 0x00546F30 (.h:122)
private:
    static const pool_list<rl_terrain_occluder*>& get_object_pool(); // 0x004D3CD0 (.h:37)
public:
    static void set_objects_max(int);
    static int get_objects_max(); // 0x004D3CE0 (.h:37)
    static int get_objects_used();
    static int get_objects_free();
    static void factory_init(mempool& factory_mempool); // 0x005196B0 (:12)
    static rl_terrain_occluder* create_placement(rl_terrain_occluder* _data_p); // 0x00546FD0 (:12)
    static rl_terrain_occluder* create(); // 0x00546F40 (:12)
    static bool can_create(); // 0x004FF460 (:12)
    virtual void release(); // vtbl+0x04 0x005197B0 (:12)
    virtual void update(const vector& viewer_position); // vtbl+0x10 0x00519830 (:27)
    virtual bool is_occluded(const rl_aabb& aabb) const; // vtbl+0x14 0x004D3E20 (:281)
    virtual bool is_occluded(const rl_occluder* occluder_p) const; // vtbl+0x18 0x004D3DE0 (:263)
    virtual bool is_occluded(const rl_partition_sector* partition_sector_p) const; // vtbl+0x1C 0x0051A4B0 (:249)
    virtual bool is_occluded(const rl_scene_entity* entity_p) const; // vtbl+0x20 0x0051A480 (:233)
    virtual bool is_useful(const rl_3d_entity* viewer_p) const; // vtbl+0x24 0x004D3E40 (:290)
    virtual void debug_render() const; // vtbl+0x3C 0x004D3DD0 (:159)
    virtual void merge(rl_occluder* occluder_p) const; // vtbl+0x28 0x004D40F0 (:316)
    virtual rl_occluder* create_merged(rl_occluder* occluder_p) const; // vtbl+0x2C 0x00547020 (:370)
    virtual bool is_partially_occluded(rl_partition_sector* partition_sector_p) const; // vtbl+0x30 0x0051A500 (:406)
    virtual bool is_partially_occluded(rl_scene_entity* entity_p) const; // vtbl+0x34 0x0051A4D0 (:390)
    virtual bool is_partially_occluded(rl_occluder* occluder_p) const; // vtbl+0x38 0x004D4240 (:422)
    const vector2& get_point1() const; // 0x004D3CF0 (.h:128)
    void set_point1(const vector2& point1); // 0x004D3D00 (.h:133)
    const vector2& get_point2() const; // 0x004D3D20 (.h:140)
    void set_point2(const vector2& point2); // 0x004D3D30 (.h:145)
    float get_height_base() const; // 0x004D3D50 (.h:152)
    void set_height_base(float height_base); // 0x004D3D60 (.h:157)
    float get_height() const; // 0x004D3D80 (.h:164)
    void set_height(float height); // 0x004D3D90 (.h:169)
    const vector& get_top_point1() const; // 0x004D3DB0 (.h:176)
    const vector& get_top_point2() const; // 0x004D3DC0 (.h:182)

    static const unsigned int m___class_id; // 0x012BEE5C
    static rl_class_factory_info ms_class_factory_info; // 0x015163A0
protected:
    static pool_list<rl_terrain_occluder*> ms_object_pool; // 0x015163D0
private:
    vector2 m_point1; // 0xD0
    vector2 m_point2; // 0xD8
    float m_height_base; // 0xE0
    float m_height; // 0xE4
    vector m_top_point1; // 0xE8
    vector m_top_point2; // 0xF4
    rl_frustum m_frustum; // 0x100
    vector m_view_position; // 0x310
    vector m_occlude_plane_normal; // 0x31C
    bool m_updated; // 0x328
    vector m_abc1; // 0x32C
    vector m_abc2; // 0x338
    vector m_abc3; // 0x344
    vector m_abc4; // 0x350
    vector m_abc5; // 0x35C
    plane m_occlude_plane; // 0x368
    vector m_plane_pos; // 0x378
};
