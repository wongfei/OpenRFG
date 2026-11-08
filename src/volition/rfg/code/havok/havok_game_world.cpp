// Original: volition/rfg/code/havok/havok_game_world.cpp
//
// Objects leaving the Havok world (decompiled): the broadphase border and game_aabb_phantom delete the object of an
// entity that leaves the world bounds or overlaps the phantom, children first, unless multiplayer defers the delete.
// Compiled in rfg_havok_unity.cpp.
//
// Partial:
// - Humans are not handled: delete_object_outside_bpo does not kill a human (human_damage, the AI death function)
//   before deleting it, and game_aabb_phantom::addOverlappingCollidable leaves a player where it is instead of
//   putting it back on the terrain.

#include "volition/rfg/code/havok/havok_game_world.h"

#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/agent/collidable/hkpcollidable.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/dynamics/entity/hkpentity.h"
#include "volition/rfg/code/havok/havok.h"
#include "volition/rfg/code/level/world.h"
#include "volition/rfg/code/multi/multi_game/multi_game.h"
#include "volition/rfg/code/objects/object.h"

// 0x0081C0D0 (:21)
// Deletes the object and its children; players and zones are kept.
void delete_object_outside_bpo(object* objp)
{
    if (objp == nullptr)
    {
        return;
    }
    if (objp->get_obj_type() == OT_HUMAN)
    {
        if (objp->get_sub_type() == OT_SUB_HUMAN_PLAYER)
        {
            return;
        }
        // human_damage(hp, nullptr, -1.0f, DBT_NONE, ...) and, for NPCs, ai_system::do_death_func: not implemented
    }
    else if (objp->get_obj_type() == OT_ZONE)
    {
        return;
    }
    object* child = objp->child_ptr;
    while (child != nullptr)
    {
        delete_object_outside_bpo(child);
        child = child->child_next;
        if (child == objp->child_ptr)
        {
            break;
        }
    }
    World.destroy_object(objp, false);
}

// ---- game_broad_phase_border
static hkBool havok_bool(bool b)
{
    hkBool result;
    result.m_bool = b;
    return result;
}

// 0x00811780 (:72)
// Callbacks are not postponed.
game_broad_phase_border::game_broad_phase_border(hkpWorld* world, hkpWorldCinfo::BroadPhaseBorderBehaviour type)
    : hkpBroadPhaseBorder(world, type, havok_bool(false))
{
}

// 0x008117D0 (:77)
game_broad_phase_border::~game_broad_phase_border()
{
}

// 0x0081C1D0 (:82)
// An entity with an object is deleted (or its delete deferred); others get Havok's border behaviour.
void game_broad_phase_border::maxPositionExceededCallback(hkpEntity* body)
{
    if (body != nullptr && body->hasProperty(HAVOK_PROPERTY_OBJECT, hkpWorldObject::MULTI_THREADING_CHECKS_ENABLE))
    {
        object* objp = (object*)body->getProperty(HAVOK_PROPERTY_OBJECT, hkpWorldObject::MULTI_THREADING_CHECKS_ENABLE).m_data;
        if (objp != nullptr)
        {
            if (!multi_game_havok_broadphase_defer_delete(objp))
            {
                delete_object_outside_bpo(objp);
            }
            return;
        }
    }
    hkpBroadPhaseBorder::maxPositionExceededCallback(body);
}

// ---- game_aabb_phantom
// 0x008117F0 (:108)
game_aabb_phantom::game_aabb_phantom(const hkAabb& aabb)
    : hkpAabbPhantom(aabb, COLLIDE_DELETE_OBJECT)
{
}

// 0x0083BE10 (:113)
void game_aabb_phantom::addOverlappingCollidable(hkpCollidable* collidable)
{
    const hkpWorldObject* wo = (const hkpWorldObject*)collidable->getOwner();
    if (wo == nullptr || !wo->hasProperty(HAVOK_PROPERTY_OBJECT, hkpWorldObject::MULTI_THREADING_CHECKS_ENABLE))
    {
        return;
    }
    object* objp = (object*)wo->getProperty(HAVOK_PROPERTY_OBJECT, hkpWorldObject::MULTI_THREADING_CHECKS_ENABLE).m_data;
    if (objp == nullptr || multi_game_havok_broadphase_defer_delete(objp))
    {
        return;
    }
    // A player (sub type 3) is put back on the terrain (havok_terrain_height, update_pos_and_orient, character
    // controller velocity cleared) or killed without terrain below: not implemented (see Partial); the delete below
    // keeps players.
    delete_object_outside_bpo(objp);
}

// 0x00811840
game_aabb_phantom::~game_aabb_phantom()
{
}
