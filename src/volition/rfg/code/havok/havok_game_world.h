// Original: volition/rfg/code/havok/havok_game_world.h
// Implementation: volition/rfg/code/havok/havok_game_world.cpp
//
// Game-side Havok world objects that catch objects leaving the world: the broadphase border deletes the object of
// an entity that leaves the world bounds (delete_object_outside_bpo; in multiplayer the delete may be deferred), and
// game_aabb_phantom does the same for objects overlapping it, except that a human is put back on the terrain with its
// velocity cleared (killed when there is no terrain below it). Decompiled.
#pragma once

#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/dynamics/phantom/hkpaabbphantom.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/dynamics/world/broadphaseborder/hkpbroadphaseborder.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/dynamics/world/hkpworldcinfo.h"

class hkAabb;
class hkpCollidable;
class hkpEntity;
class hkpWorld;
class object;

// size 0x44
class game_broad_phase_border : public hkpBroadPhaseBorder
{
public:
    game_broad_phase_border(hkpWorld* world, hkpWorldCinfo::BroadPhaseBorderBehaviour type); // 0x00811780 (:72)
    virtual ~game_broad_phase_border(); // vtbl+0x00 0x008117D0 (:77)
    virtual void maxPositionExceededCallback(hkpEntity* body); // vtbl+0x10 0x0081C1D0 (:82)

};

// size 0xD0
class game_aabb_phantom : public hkpAabbPhantom
{
public:
    game_aabb_phantom(const hkAabb& aabb); // 0x008117F0 (:108)
    virtual void addOverlappingCollidable(hkpCollidable* collidable); // vtbl+0x24 0x0083BE10 (:113)
    virtual ~game_aabb_phantom(); // vtbl+0x00 0x00811840

};

void delete_object_outside_bpo(object* objp); // 0x0081C0D0 (:21)
