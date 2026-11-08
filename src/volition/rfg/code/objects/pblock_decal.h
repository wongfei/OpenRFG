// Original: volition/rfg/code/objects/pblock_decal.h
//
// A decal placement as stored in zone and terrain data (decal name hash, position, orientation, size and depth in
// centimetres). Placement inferred.
#pragma once

#include "volition/vlib/code/math/vector3.h"
#include "volition/vlib/code/math/quaternion.h"

// size 0x24
struct pblock_decal
{
    unsigned int name_hash; // 0x00
    vector pos; // 0x04
    quaternion orient; // 0x10
    unsigned short size; // 0x20
    unsigned short depth; // 0x22
};
