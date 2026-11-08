// Original: volition/rfg/code/level/undergrowth.h
// Implementation: volition/rfg/code/level/undergrowth.cpp
//
// Undergrowth (grass and small foliage): each territory cell's layers place foliage mesh instances in active cells
// around the camera (randomly, evenly or singly), fade cells out, sway them with wind, and flatten them with
// destruction events (a request to flatten undergrowth in a sphere after an explosion or nano attack, from
// Destruction_event_pool, 8 entries). The zones, layers and cells are TU-private. Placement of the event types inferred.
#pragma once

#include "volition/vlib/code/math/vector3.h"

struct fading_cell;
struct rfg_terrain;
struct undergrowth_zone;

enum undergrowth_destruction_event_type
{
    UDET_EXPLOSION = 0,
    UDET_NANO = 1,
    NUM_UNDERGROWTH_DESTRUCTION_EVENT_TYPES = 2,
};

enum undergrowth_destruction_event_shape
{
    UDES_SPHERE = 0,
    NUM_UNDERGROWTH_DESTRUCTION_EVENT_SHAPES = 1,
};

// size 0x18
struct destruction_event
{
    destruction_event(); // 0x0091FB10

    undergrowth_destruction_event_type type; // 0x00
    undergrowth_destruction_event_shape shape; // 0x04
    vector pos; // 0x08
    float radius; // 0x14
};

void destroy_fading_cell(undergrowth_zone* zone_p, fading_cell* fcell_p); // 0x00932B50 (:249)
void undergrowth_system_init(); // 0x00934470 (:1186)
void undergrowth_system_shutdown(); // 0x0091BF10 (:1216)
void undergrowth_level_init(); // 0x00923560 (:1228)
void undergrowth_level_shutdown(); // 0x009344E0 (:1239)
void undergrowth_process(); // 0x009345B0 (:1258)
void undergrowth_register_terrain_data(const rfg_terrain* terrain_data); // 0x009346C0 (:1283)
void undergrowth_unregister_terrain_data(const rfg_terrain* terrain_data); // 0x009346F0 (:1298)
void undergrowth_set_thread_ownership(); // 0x0091BF30 (:1309)
void undergrowth_create_destruction_event(const vector& pos, float radius, undergrowth_destruction_event_type ugde_type); // 0x0092CCC0 (:1314)

// Not in the binary (lod.md L2 test census): what the undergrowth holds now.
struct undergrowth_census
{
    int zones; // undergrowth zones (territory cells with undergrowth layers)
    int layers; // their layers
    int grid_cells; // active cells in the layers' grids
    int cells_to_create; // placed cells waiting for the update-render task
    int instances; // active cells with a clone mesh instance
    int clones; // clones of those instances
    int fading_cells;
    int pending_events; // destruction events not processed yet
    int pool_clones_used; // Undergrowth_clone_pool's clones in use
    int pool_clones_max;
    int drawn_instances; // instances the latest render drew (rl_clone_mesh_take_census)
    int drawn_by_lod[4]; // of those, the instances drawing LOD 0-3
    int clones_by_lod[4]; // and their clones
};

void undergrowth_take_census(undergrowth_census& census);
