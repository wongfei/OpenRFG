// Original: volition/render_lib/code/rl_partition_system.h
// Implementation: volition/render_lib/code/rl_partition_system.cpp
//
// rl_partition_system, the spatial partition of a scene (a tree of rl_partition_sector).
#pragma once

#include "volition/render_lib/code/rl_base_object.h"

class rl_aabb;
class rl_partition_sector;
class rl_scene_entity;

// size 0xC
class rl_partition_system : public rl_base_object
{
protected:
    rl_partition_system(); // 0x004C4470 (:13)
    virtual ~rl_partition_system(); // vtbl+0x00 0x004C4480 (:18)
public:
    virtual void insert(rl_scene_entity*) = 0; // vtbl+0x08
    virtual void remove(rl_scene_entity*) = 0; // vtbl+0x0C
    virtual void clean() = 0; // vtbl+0x10
    virtual int query_aabb(rl_scene_entity**, int, const rl_aabb&, int) = 0; // vtbl+0x14
    rl_partition_sector* get_root_sector() const; // 0x004B86E0 (.h:52)
protected:
    void set_root_sector(rl_partition_sector* root_sector_p); // 0x004B86F0 (.h:57)

private:
    rl_partition_sector* m_root_sector_p; // 0x08
};

bool RL_auto_partition_update(); // 0x004C44A0 (:25)
