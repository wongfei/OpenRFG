// Original: volition/render_lib/code/rl_occluder.h
// Implementation: volition/render_lib/code/rl_occluder.cpp
//
// rl_occluder, an occluder volume used to cull entities behind it.
#pragma once

#include "volition/render_lib/code/rl_handle.h"
#include "volition/render_lib/code/rl_scene_entity.h"

class rl_3d_entity;
class rl_aabb;
class rl_partition_sector;
class vector;

// size 0xD0
class rl_occluder : public rl_scene_entity
{
    // Access inferred: rl_scene reads and writes the protected members directly (inlined accessors).
    friend class rl_scene;
protected:
    rl_occluder(); // 0x00541A00 (:8)
    virtual ~rl_occluder(); // vtbl+0x00 0x00541A80 (:17)
public:
    rl_occluder* get_prev() const;
    void set_prev(rl_occluder*);
    rl_occluder* get_next() const;
    void set_next(rl_occluder*);
    virtual void update(const vector& viewer_position); // vtbl+0x10 0x00513760 (:28)
    virtual bool is_occluded(const rl_aabb&) const = 0; // vtbl+0x14
    virtual bool is_occluded(const rl_occluder*) const = 0; // vtbl+0x18
    virtual bool is_occluded(const rl_partition_sector*) const = 0; // vtbl+0x1C
    virtual bool is_occluded(const rl_scene_entity*) const = 0; // vtbl+0x20
    virtual bool is_useful(const rl_3d_entity*) const = 0; // vtbl+0x24
    virtual void merge(rl_occluder*) const = 0; // vtbl+0x28
    virtual rl_occluder* create_merged(rl_occluder*) const = 0; // vtbl+0x2C
    virtual bool is_partially_occluded(rl_partition_sector*) const = 0; // vtbl+0x30
    virtual bool is_partially_occluded(rl_scene_entity*) const = 0; // vtbl+0x34
    virtual bool is_partially_occluded(rl_occluder*) const = 0; // vtbl+0x38
    virtual void debug_render() const; // vtbl+0x3C 0x00513870 (:36)
    bool get_occluded() const; // 0x004BBD20 (.h:68)
    void set_occluded(bool occluded); // 0x004BBD30 (.h:73)
    const rl_handle& get_handle() const; // 0x004BBD40 (.h:78)
    void set_handle(const rl_handle& handle_inst); // 0x004BBD50 (.h:83)

    rl_occluder* prev; // 0xC0
    rl_occluder* next; // 0xC4
private:
    rl_handle m_handle; // 0xC8
    bool m_occluded; // 0xCC
};
