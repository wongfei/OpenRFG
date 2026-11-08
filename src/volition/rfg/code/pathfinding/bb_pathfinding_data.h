// Original: volition/rfg/code/pathfinding/bb_pathfinding_data.h
//
// The pathfinding graph layers of a zone (obj_zone::pfg_zone): one for humans and one for vehicles, each with its nav
// cells, vertices and the outer edges that link it to the neighbouring zones (et_ptr_offset data loaded with the zone),
// plus its shape and Havok handle.
#pragma once

#include "volition/rfg/code/pathfinding/nav_cell.h"
#include "volition/vlib/code/math/vector3.h"
#include "volition/vlib/code/util/endian_type.h"

class matrix;

// size 0x8
struct nav_cell_outer_edge
{
    unsigned short nav_cell_index; // 0x00
    unsigned char edge_index; // 0x02
    unsigned int adjacent_zone; // 0x04
};

// size 0x3C
struct bb_pfg_layer
{
    void transform(const vector&, const matrix&);

    unsigned int pfg_sig; // 0x00
    unsigned int pfg_version; // 0x04
    unsigned short num_vertices; // 0x08
    et_ptr_offset<vector,0> vertices; // 0x0C
    unsigned short num_nav_cells; // 0x10
    et_ptr_offset<nav_cell,0> nav_cells; // 0x14
    et_ptr_offset<nav_cell*,0> nav_cell_heap; // 0x18
    unsigned short num_nav_cell_outer_edges; // 0x1C
    et_ptr_offset<nav_cell_outer_edge,0> nav_cell_outer_edges; // 0x20
    vector bbox_min; // 0x24
    vector bbox_max; // 0x30
};

// size 0x18
struct bb_pfg_zone
{
    void clear(); // 0x00A05680 (.h:227)

    bb_pfg_layer* pfg; // 0x00
    unsigned int pfg_shape; // 0x04
    unsigned int pfg_havok_handle; // 0x08
    bb_pfg_layer* vehicle_pfg; // 0x0C
    unsigned int vehicle_pfg_shape; // 0x10
    unsigned int vehicle_pfg_havok_handle; // 0x14
};
