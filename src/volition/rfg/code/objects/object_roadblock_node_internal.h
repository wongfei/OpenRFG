// Original: volition/rfg/code/objects/object_roadblock_node_internal.h
//
// roadblock_construction_info, the state while a roadblock's objects are created (position, orientation, bounding box
// and dummy nodes).
#pragma once

#include "volition/vlib/code/math/matrix.h"
#include "volition/vlib/code/math/vector3.h"
#include "volition/vlib/code/util/varray.h"

class object_bounding_box;
class object_dummy;

// size 0x444
class roadblock_construction_info
{
public:
    roadblock_construction_info(); // 0x00A6A4A0 (.h:204)
    void reset(); // 0x00A3D890 (.h:215)
    ~roadblock_construction_info(); // 0x00A6AEE0

    vector pos; // 0x00
    matrix orient; // 0x0C
    object_bounding_box* bbp; // 0x30
    farray<object_dummy*,256> dummy_node_list; // 0x34
};
