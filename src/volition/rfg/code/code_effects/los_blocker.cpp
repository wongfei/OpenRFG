// Original: volition/rfg/code/code_effects/los_blocker.cpp
//
// Everything but the debug draw los_blocker_render_debug (stub_impl.md S3). A blocker is a sphere at a position or
// following an object; AI line of sight through blockers adds up their opacities (1 blocks it).

#include "volition/rfg/code/code_effects/los_blocker.h"

#include "volition/rfg/code/level/world.h"
#include "volition/rfg/code/objects/object.h"
#include "volition/vlib/code/math/ix.h"
#include "volition/vlib/code/math/matrix.h"
#include "volition/vlib/code/memory/object_pool.h"

static object_pool<los_blocker, 25, 1> LOS_blocker_pool(nullptr, nullptr, 0); // 0x01E24F00

// 0x006E5E20
los_blocker::los_blocker()
{
    life_span.invalidate(); // value -1
}

// 0x006E4470 (:45)
// The parent's position, plus pos un-rotated by the parent's orientation when flag 1 is set; pos without a parent.
void los_blocker::get_pos(vector& real_pos)
{
    object* parent = World.object_from_handle(parent_handle);
    if (parent == nullptr)
    {
        real_pos = pos;
        return;
    }
    real_pos = parent->pos;
    if ((flags & 1) != 0)
    {
        const vector offset = pos.copy_unrotate(parent->orient);
        real_pos.x = offset.x + real_pos.x;
        real_pos.y = offset.y + real_pos.y;
        real_pos.z = offset.z + real_pos.z;
    }
}

// 0x006E4530 (:63)
void los_blocker_system_init()
{
}

// 0x006F6150 (:68)
void los_blocker_level_init_pre()
{
    LOS_blocker_pool.clear(0); // the loop freeing the head of the in-use list until all 25 are free, inlined
}

// 0x006ED480 (:87)
// Null when all 25 are in use; a life_span of 0 or less never expires.
los_blocker* los_blocker_create(const vector& pos, const float radius, const float opacity, const int life_span, unsigned int parent_handle)
{
    if (LOS_blocker_pool.get_free() < 1)
    {
        return nullptr;
    }
    los_blocker* lbp = LOS_blocker_pool.alloc(0);
    lbp->pos = pos;
    lbp->radius = radius;
    lbp->opacity = opacity;
    if (life_span < 1)
    {
        lbp->life_span.invalidate();
    }
    else
    {
        lbp->life_span.set(life_span);
    }
    lbp->parent_handle = parent_handle;
    object* parent = World.object_from_handle(parent_handle);
    if (parent != nullptr)
    {
        vector offset;
        offset.x = pos.x - parent->pos.x;
        offset.y = pos.y - parent->pos.y;
        offset.z = pos.z - parent->pos.z;
        if (0.0f < offset.y * offset.y + offset.x * offset.x + offset.z * offset.z)
        {
            // original bug kept: the offset is rotated into the parent's frame but never stored, so pos stays the
            // world position that get_pos then treats as an offset
            offset.rotate(parent->orient);
            lbp->flags |= 1;
        }
    }
    return lbp;
}

// 0x006F1290 (:122)
void los_blocker_destroy(los_blocker* lbp)
{
    LOS_blocker_pool.free(lbp);
}

// 0x006F12A0 (:129)
// Frees the blockers whose life span ran out or whose parent is gone.
void los_blocker_do_frame()
{
    LOS_blocker_pool.thread_access.enter();
    los_blocker* next = LOS_blocker_pool.get_first(0);
    while (next != nullptr)
    {
        los_blocker* lbp = next;
        next = LOS_blocker_pool.get_next(lbp); // inlined
        if (lbp->life_span.elapsed())
        {
            LOS_blocker_pool.free(lbp);
        }
        // original bug kept: an expired blocker whose parent is also gone is freed a second time
        if (lbp->parent_handle != 0xFFFFFFFF && World.object_from_handle(lbp->parent_handle) == nullptr)
        {
            LOS_blocker_pool.free(lbp);
        }
    }
    LOS_blocker_pool.thread_access.leave();
}

// 0x006ED5A0 (:148)
// The summed opacity of the blockers the segment passes through, 1 once it reaches 1.
float los_blocker_percent_blocked(const vector& start, const vector& end)
{
    float blocked = 0.0f;
    LOS_blocker_pool.thread_access.enter();
    for (los_blocker* lbp = LOS_blocker_pool.get_first(0); lbp != nullptr; lbp = LOS_blocker_pool.get_next(lbp))
    {
        vector center;
        lbp->get_pos(center); // inlined
        vector hit;
        if (ix_linesegment_sphere(hit, start, end, center, lbp->radius))
        {
            blocked = lbp->opacity + blocked;
            if (1.0f <= blocked)
            {
                blocked = 1.0f;
                break;
            }
        }
    }
    LOS_blocker_pool.thread_access.leave();
    return blocked;
}
