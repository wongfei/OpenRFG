// Original: volition/rfg/code/level/world_stream.cpp
//
// Streaming of zones and objects around the stream position (decompiled; the debug console commands are not
// implemented).
// - Zones: a world_zone whose box enters the terrain layer's stream radius (xz) requests its container
//   (WZS_STREAMING); once loaded, process_streaming_zone serializes its .rfgzone_pc and finds its obj_zone
//   (WZS_LOADED). A zone that leaves the radius is destreamed while streaming, or queued for deserialization (saving
//   its state) when loaded. In multiplayer and Wrecking Crew every zone stays.
// - Terrain subzones (3x3 per zone) are instantiated when their midpoint is in a level layer cell inside the level
//   radius that the cell under the stream position masks in (stream_grid_cell::stream_mask), and deinstantiated
//   otherwise (monitor_grid / monitor_entering_zones).
// - Objects: the broadphase (havok_bpo) reports islands of objects entering or leaving the streaming region. An
//   entering island is streamed in hierarchy order (parents first, constraints last) once every object's resource
//   (srid) is loaded; a leaving island is streamed out in reverse when no object is still inside the region or busy.
//   Each pass has a time budget unless hard_load (5 ms in, 2 ms out).
// - Zone handles: the upper byte of a handle is the zone's grid x/y, so global_zone_grid[(h >> 24 & 0xF) * 16 +
//   (h >> 28)] is the zone of handle h; positions map with x = (pos.x + 4088) / 511, y = (4088 - pos.z) / 511.

#include "volition/rfg/code/level/world_internal.h"

#include "volition/rfg/code/gameplay/wrecking_crew/wrecking_crew.h"
#include "volition/rfg/code/havok/havok_bp.h"
#include "volition/rfg/code/level/rfg_chunk_shared.h"
#include "volition/rfg/code/level/rfg_stream_layout_shared.h"
#include "volition/rfg/code/level/rfg_terrainx.h"
#include "volition/rfg/code/level/rfg_zonex.h"
#include "volition/rfg/code/multi/multi_core/include/multi.h"
#include "volition/rfg/code/objects/movers/object_mover.h"
#include "volition/rfg/code/objects/object_zone.h"
#include "volition/rfg/code/rfg.h"
#include "volition/rfg/code/util/special_allocators.h"
#include "volition/vlib/code/cfile/cfile.h"
#include "volition/vlib/code/cfile/packfile.h"
#include "volition/vlib/code/math/bbox.h"
#include "volition/vlib/code/math/ix.h"
#include "volition/vlib/code/memory/rfg_split_mempool.h"
#include "volition/vlib/code/memory/vmemory.h"
#include "volition/vlib/code/os/timer.h"
#include "volition/vlib/code/util/string_comp.h"
#include "volition/vlib/code/vlib_core/vlib_assert.h"

#include "keen/base/trace.hpp"

#include <math.h>
#include <stdio.h>

// ---- file-static globals
static float Stream_debug_cull_dist = 1277.5f; // 0x01668AF4
static farray<object*,4096> hierarchy_list; // 0x01678BC0
static bool Stream_debug_backup_fog; // 0x02CA1671
static float Stream_debug_backup_far_clip; // 0x02CCBA80
static bool Show_stream_objects; // 0x02CEC1ED
static bool Show_stream_grid; // 0x02CEC1EE
static bool Show_stream_region; // 0x02CEC1EF
static bool Show_zone_navmesh; // 0x02CEC1F0
static bool Stream_debug; // 0x02CEC1F1

// The zone index of a handle (its upper byte holds the zone's grid x and y), 257 or more for none.
static unsigned int handle_zone_index(unsigned int handle)
{
    return ((handle >> 24) & 0xF) * 16 + (handle >> 28);
}

// The zone index of a position (16x16 cells of 511 units from -4088), 257 or more outside the grid.
static unsigned int pos_zone_index(const vector& pos)
{
    const unsigned int x = (unsigned int)((pos.x + 4088.0f) / 511.0f);
    const unsigned int y = (unsigned int)((4088.0f - pos.z) / 511.0f);
    if (x >= 16 || y >= 16)
    {
        return 0xFFFFFFFF;
    }
    return y * 16 + x;
}

// Whether cell is one of the cells the cell under the stream position streams (its stream_mask holds one bit per cell
// of the (2 * cell_load_distance + 1)^2 square around it; inlined in monitor_grid and monitor_entering_zones).
static bool cell_is_masked_in(const stream_layer& layer, const stream_grid_cell* center, const stream_grid_cell* cell)
{
    const int width = layer.cell_load_distance * 2 + 1;
    const int bit = ((center->grid_z - cell->grid_z) * width + (width * width) / 2 - center->grid_x + cell->grid_x) & 0x1F;
    return (center->stream_mask & (1u << bit)) != 0;
}

// 0x0092E0C0 (:191)
static obj_zone* obj_zone_from_grid_id(unsigned short gid)
{
    // The zone object whose handle carries the grid id's high byte.
    for (int i = 0; i < World.get_num_objects<obj_zone>(); i++)
    {
        obj_zone* zone = World.get_object<obj_zone>(i);
        if ((unsigned int)(gid >> 8) == (zone->get_handle() >> 24))
        {
            return zone;
        }
    }
    return nullptr;
}

// 0x0093F9C0 (:213)
void world::zone_visit_cb(const char* filename, zone_file* zf)
{
    // Called for every zone file of the layout: records its border flag and preloads its handles.
    world_zone* wz = World.get_world_zone_by_name(filename);
    wz->is_border_zone = (zf->header.flags & 1) != 0;
    World.serialize_zone(wz, zf, true);
}

// 0x0093F9F0 (:236)
bool world::init_territory_streaming_resources(zone_file* persistent_zf)
{
    rfg_split_mempool* pool = salloc_pregame_split_lock(Multiplayer ? 0x400000 : 0x1000000, 0);
    if (pool == nullptr)
    {
        return false;
    }
    mempool_mark_scope scope(pool);
    grid = stream_layout_level_load(&Level_mempool, pool, nullptr, zone_visit_cb);
    if (grid == nullptr)
    {
        VLIB_CRITICAL_FAILURE();
        salloc_pregame_split_unlock(pool);
        return false;
    }
    // Each zone streams with the terrain layer cell that holds it; the persistent zone covers the whole grid.
    for (int i = 0; i < num_territory_zones; i++)
    {
        world_zone* wz = all_zones[i];
        if (wz == get_world_zone_by_index(15))
        {
            rfg_zonex_get_maximal_bbox(wz->bmin, wz->bmax);
        }
        else
        {
            bbox cell_boxes[9];
            int cells[9];
            grid->layers[LAYER_TERRAIN].get_cell_list(wz->bmin, wz->bmax, cell_boxes, cells, 1);
            wz->srid = grid->layers[LAYER_TERRAIN].cells[cells[0]]->srid[0];
        }
    }
    havok_bpo_do_frame();
    salloc_pregame_split_unlock(pool);
    return true;
}

// 0x009239A0 (:301)
void world::shutdown_territory_streaming_resources()
{
    stream_layout_level_shutdown();
    grid = nullptr;
}

// 0x009239E0 (:309)
bool world::is_inside_streaming_region(vector& bmin, vector& bmax)
{
    // The box of the active level layer cells (full height).
    vector region_min(1.0e7f, 1.0e7f, 1.0e7f);
    vector region_max(-1.0e7f, -1.0e7f, -1.0e7f);
    stream_layer& layer = grid->layers[LAYER_LEVEL];
    for (int i = 0; i < layer.active_cells.size(); i++)
    {
        stream_grid_cell* cell = layer.active_cells[i];
        vector cell_min((float)cell->grid_x * layer.cell_dim.x + layer.bmin.x, -10000.0f, layer.cell_dim.z * (float)cell->grid_z + layer.bmin.z);
        vector cell_max(layer.cell_dim.x + cell_min.x, 10000.0f, layer.cell_dim.z + cell_min.z);
        bbox_expand(region_min, region_max, cell_min, cell_max);
    }
    return ix_box_box_aligned(bmin, bmax, region_min, region_max);
}

// 0x0092E110 (:324)
bool world::is_zone_border_zone(const vector& pos) const
{
    const unsigned int index = pos_zone_index(pos);
    if (index < 257 && global_zone_grid[index] != nullptr)
    {
        return global_zone_grid[index]->is_border_zone;
    }
    return true;
}

// 0x0092E180 (:337)
bool world::is_zone_streamed(const vector& pos) const
{
    const unsigned int index = pos_zone_index(pos);
    return index < 257 && global_zone_grid[index] != nullptr && global_zone_grid[index]->state == WZS_LOADED;
}

// 0x0092E1F0 (:348)
bool world::is_zone_streamed(unsigned int handle) const
{
    const unsigned int index = handle_zone_index(handle);
    return index < 257 && global_zone_grid[index] != nullptr && global_zone_grid[index]->state == WZS_LOADED;
}

// 0x00923B60 (:353)
bool world::is_zone_streamed(unsigned short id) const
{
    const unsigned int index = ((id >> 8) & 0xF) * 16 + (id >> 12);
    return index < 257 && global_zone_grid[index] != nullptr && global_zone_grid[index]->state == WZS_LOADED;
}

// 0x0092E230 (:365)
void world::visit_object(base_array<object*>& list, base_array<object*>& constraints, object* objp)
{
    // Depth first: the object, then its children; constraints go to their own list.
    objp->obj_flags.visited = 1;
    if (objp->get_obj_type() == OT_CONSTRAINT)
    {
        constraints.add(objp);
    }
    else
    {
        list.add(objp);
    }
    for (object* child = objp->child_ptr; child != nullptr;)
    {
        if (!child->obj_flags.visited)
        {
            visit_object(list, constraints, child);
        }
        child = child->child_next;
        if (child == objp->child_ptr)
        {
            break;
        }
    }
}

// 0x0093D4D0 (:390)
void world::collect_sorted_list(base_array<object*>& list, havok_bpi* island)
{
    // The hierarchies of the island's objects (from their roots), constraints last.
    farray<object*,512> constraints;
    list.remove_all();
    if (island->obj_list != 0xFFFF)
    {
        for (havok_bpo* bpo = &Object_buf[island->obj_list]; bpo != nullptr; bpo = &Object_buf[bpo->next])
        {
            object* objp = (object*)bpo->user_data;
            if (!objp->obj_flags.visited && !objp->obj_flags.destroyed)
            {
                // Climb to the root (through stubs too, as the original does: a stub parent ends the climb with a
                // null object).
                unsigned int parent_handle = objp->parent;
                while (parent_handle != 0xFFFFFFFF)
                {
                    object** parent = object_handle_hash.get_ptr(parent_handle);
                    objp = parent != nullptr && *parent != nullptr && !(*parent)->obj_flags.stub ? *parent : nullptr;
                    parent_handle = objp->parent;
                }
                visit_object(list, constraints, objp);
            }
            if (bpo->next == island->obj_list)
            {
                break;
            }
        }
    }
    for (int i = 0; i < constraints.size(); i++)
    {
        list.add(constraints[i]);
    }
}

// 0x00935060 (:435)
bool world::monitor_grid()
{
    // The streaming region: the level radius around the stream position (the whole territory in multiplayer).
    stream_layer& level = grid->layers[LAYER_LEVEL];
    vector region_min;
    vector region_max;
    if (Multiplayer || wrecking_crew_is_active())
    {
        region_min = vector(3.4028235e38f, 3.4028235e38f, 3.4028235e38f);
        region_max = vector(-3.4028235e38f, -3.4028235e38f, -3.4028235e38f);
        for (int i = 0; i < num_territory_zones; i++)
        {
            world_zone* wz = all_zones[i];
            if (wz->srid != 0)
            {
                bbox_expand(region_min, region_max, wz->bmin, wz->bmax);
            }
        }
    }
    else
    {
        const float radius = level.stream_radius - 0.01f;
        region_min = vector(stream_pos.x - radius, stream_pos.y - radius, stream_pos.z - radius);
        region_max = vector(stream_pos.x + radius, stream_pos.y + radius, stream_pos.z + radius);
    }
    havok_bpo_region_update(region_min, region_max);

    // The level cells of that region and the cell under the stream position, whose mask says which of them stream.
    bbox cell_boxes[256];
    int cells[256];
    const int num_cells = level.get_cell_list(region_min, region_max, cell_boxes, cells, 256);
    int center_index;
    stream_grid_cell* center = nullptr;
    if (grid->layers[LAYER_LEVEL].get_cell_list(stream_pos, stream_pos, nullptr, &center_index, 1) > 0)
    {
        center = grid->layers[LAYER_LEVEL].cells[center_index];
    }

    // Instantiated subzones outside those cells are deinstantiated.
    {
        csspinlock_scope lock(Terrain_subzones.thread_access);
        for (rfg_terrain_subzone* subzone = Terrain_subzones.get_first(0); subzone != nullptr;)
        {
            rfg_terrain_subzone* next = Terrain_subzones.get_next(subzone);
            if (subzone->instanciated)
            {
                bool inside = false;
                for (int i = 0; i < num_cells; i++)
                {
                    stream_layer& layer = grid->layers[LAYER_LEVEL];
                    stream_grid_cell* cell = layer.cells[cells[i]];
                    if (cell != nullptr && (center == nullptr || cell_is_masked_in(layer, center, cell)) && ix_point_in_box(subzone->midpoint, cell_boxes[i].min, cell_boxes[i].max))
                    {
                        inside = true;
                        break;
                    }
                }
                if (!inside)
                {
                    terrain_data* data = nullptr;
                    world_zone* wz = World.get_world_zone_by_pos(subzone->midpoint);
                    if (wz != nullptr && wz->state != WZS_UNLOADED && wz->zone_objp != nullptr)
                    {
                        data = wz->zone_objp->get_terrain_data();
                    }
                    terrain_deinstantiate_subzone(subzone, data);
                }
            }
            subzone = next;
        }
    }
    grid->do_frame(stream_pos, sl_flags.pending_single_zone);
    return true;
}

// 0x0092E2E0 (:518)
void world::monitor_leaving_zones()
{
    // Zones outside the terrain radius: destream (still streaming) or deserialize (loaded); never in multiplayer.
    stream_layer& terrain = grid->layers[LAYER_TERRAIN];
    const vector region_min(stream_pos.x - terrain.stream_radius, -512.0f, stream_pos.z - terrain.stream_radius);
    const vector region_max(terrain.stream_radius + stream_pos.x, 512.0f, terrain.stream_radius + stream_pos.z);
    for (int i = 0; i < num_territory_zones; i++)
    {
        world_zone* wz = all_zones[i];
        if (ix_box_box_aligned_xz(wz->bmin, wz->bmax, region_min, region_max))
        {
            continue;
        }
        if (wz->state == WZS_STREAMING)
        {
            if (!Multiplayer && !wrecking_crew_is_active())
            {
                destream_zone(wz);
            }
        }
        else if (wz->state == WZS_LOADED && !Multiplayer && !wrecking_crew_is_active() && (deserialize_state == DS_IDLE || deserialize_state == DS_PENDING))
        {
            deserialize_zone(wz);
        }
    }
}

// 0x00940690 (:563)
void world::monitor_entering_zones()
{
    // Zones inside the terrain radius (all of them in multiplayer) are requested, then processed once loaded.
    stream_layer& terrain = grid->layers[LAYER_TERRAIN];
    const vector region_min(stream_pos.x - terrain.stream_radius, -512.0f, stream_pos.z - terrain.stream_radius);
    const vector region_max(stream_pos.x + terrain.stream_radius, 512.0f, stream_pos.z + terrain.stream_radius);
    for (int i = 0; i < num_territory_zones; i++)
    {
        world_zone* wz = all_zones[i];
        if (ix_box_box_aligned_xz(wz->bmin, wz->bmax, region_min, region_max) || Multiplayer || wrecking_crew_is_active())
        {
            if (wz->state == WZS_UNLOADED)
            {
                if (wz->srid != 0)
                {
                    wz->zone_objp = nullptr;
                    stream2_request(wz->srid);
                    wz->state = WZS_STREAMING;
                }
            }
            else if (wz->state == WZS_STREAMING)
            {
                process_streaming_zone(wz);
            }
        }
    }

    // Loaded subzones whose midpoint is in a masked-in level cell of the radius are instantiated once their zone object
    // (and so its terrain) exists.
    stream_layer& level = grid->layers[LAYER_LEVEL];
    const vector level_min(stream_pos.x - level.stream_radius, -512.0f, stream_pos.z - level.stream_radius);
    const vector level_max(stream_pos.x + level.stream_radius, 512.0f, stream_pos.z + level.stream_radius);
    bbox cell_boxes[256];
    int cells[256];
    const int num_cells = level.get_cell_list(level_min, level_max, cell_boxes, cells, 256);
    int center_index;
    stream_grid_cell* center = nullptr;
    if (grid->layers[LAYER_LEVEL].get_cell_list(stream_pos, stream_pos, nullptr, &center_index, 1) > 0)
    {
        center = grid->layers[LAYER_LEVEL].cells[center_index];
    }
    csspinlock_scope lock(Terrain_subzones.thread_access);
    for (rfg_terrain_subzone* subzone = Terrain_subzones.get_first(0); subzone != nullptr;)
    {
        rfg_terrain_subzone* next = Terrain_subzones.get_next(subzone);
        if (!subzone->instanciated)
        {
            for (int i = 0; i < num_cells; i++)
            {
                stream_layer& layer = grid->layers[LAYER_LEVEL];
                stream_grid_cell* cell = layer.cells[cells[i]];
                if (cell != nullptr && (center == nullptr || cell_is_masked_in(layer, center, cell)) && ix_point_in_box(subzone->midpoint, cell_boxes[i].min, cell_boxes[i].max))
                {
                    world_zone* wz = World.get_world_zone_by_pos(subzone->midpoint);
                    if (wz != nullptr && wz->state != WZS_UNLOADED && wz->zone_objp != nullptr)
                    {
                        terrain_data* data = wz->zone_objp->get_terrain_data();
                        if (data != nullptr)
                        {
                            terrain_instantiate_subzone(subzone, data);
                        }
                    }
                    break;
                }
            }
        }
        subzone = next;
    }
}

// 0x0093FB40 (:654)
bool world::process_streaming_objects(bool hard_load)
{
    if (all_objects.size() < 1)
    {
        return false;
    }
    hierarchy_list.remove_all();

    // Entering islands: stream in once every object's resource is loaded (5 ms budget).
    int start = timer_get(TIMER_MICROSECONDS);
    for (havok_bpi* island = havok_bpo_get_first_entering(); island != nullptr && (hard_load || timer_get(TIMER_MICROSECONDS) - start < 5000);)
    {
        havok_bpi* next = havok_bpo_get_next_entering(island);
        collect_sorted_list(hierarchy_list, island);
        bool ready = true;
        for (int i = 0; i < hierarchy_list.size(); i++)
        {
            object* objp = hierarchy_list[i];
            objp->obj_flags.visited = 0;
            if (!process_streaming_object(objp))
            {
                ready = false;
            }
        }
        if (ready)
        {
            for (int i = 0; i < hierarchy_list.size(); i++)
            {
                stream_object(hierarchy_list[i]);
            }
            havok_bpo_pop_entering(island);
        }
        island = next;
    }
    if (Multiplayer || wrecking_crew_is_active())
    {
        process_flagged_objects(2);
        process_flagged_objects(4);
        return num_streaming_objects > 0;
    }

    // Leaving islands: stream out (children first) when nothing is inside the radius or busy (2 ms budget).
    start = timer_get(TIMER_MICROSECONDS);
    for (havok_bpi* island = havok_bpo_get_first_leaving(); island != nullptr && (hard_load || timer_get(TIMER_MICROSECONDS) - start < 2000);)
    {
        havok_bpi* next = havok_bpo_get_next_leaving(island);
        collect_sorted_list(hierarchy_list, island);
        bool can_leave = true;
        for (int i = hierarchy_list.size() - 1; i >= 0; i--)
        {
            object* objp = hierarchy_list[i];
            objp->obj_flags.visited = 0;
            vector bmin;
            vector bmax;
            objp->get_bbox(bmin, bmax);
            const bool inside = ix_box_cylinder_xz(bmin, bmax, stream_pos, grid->layers[LAYER_LEVEL].stream_radius);
            const bool forced_outside = havok_bpo_is_forced_outside(objp->bp_handle);
            if (inside && !forced_outside)
            {
                can_leave = false;
            }
            if (!objp->ready_to_stream_out())
            {
                can_leave = false;
            }
        }
        if (can_leave)
        {
            for (int i = hierarchy_list.size() - 1; i >= 0; i--)
            {
                destream_object(hierarchy_list[i]);
            }
            havok_bpo_pop_leaving(island);
        }
        island = next;
    }
    process_flagged_objects(2);
    process_flagged_objects(4);
    return num_streaming_objects > 0;
}

// 0x0093D610 (:784)
bool world::process_streaming_object_immediate(object* objp)
{
    if (!process_streaming_object(objp))
    {
        return false;
    }
    object* parent = object_from_handle(objp->parent);
    return parent == nullptr || parent->obj_flags.streamed;
}

// 0x0092E430 (:797)
bool world::process_streaming_object(object* objp)
{
    // True when the object can be streamed in: its zone is loaded, it is inside the radius and its resource (if any)
    // is loaded. A resource that failed to load destroys the object, except chunks, which fall back to the missing
    // chunk (or destroyable).
    if (objp->obj_flags.destroyed)
    {
        return false;
    }
    const unsigned int index = pos_zone_index(objp->pos);
    if (index < 257 && global_zone_grid[index] != nullptr && global_zone_grid[index]->state != WZS_LOADED)
    {
        return false;
    }
    if (!Multiplayer && !wrecking_crew_is_active() && !ix_box_cylinder_xz(objp->last_known_bmin, objp->last_known_bmax, stream_pos, grid->layers[LAYER_LEVEL].stream_radius))
    {
        return false;
    }
    if (objp->srid == 0 || objp->obj_flags.streamed)
    {
        return true;
    }
    if (!objp->obj_flags.streaming && stream2_request(objp->srid))
    {
        objp->obj_flags.streaming = 1;
        num_streaming_objects++;
    }
    if (objp->obj_flags.streaming)
    {
        const stream2_status_val status = stream2_get_status(objp->srid);
        if (status != ST_ERROR)
        {
            return status == ST_LOADED;
        }
    }
    if (objp->get_obj_type() != OT_MOVER)
    {
        destroy_object(objp, false);
        return false;
    }
    char message[256];
    sprintf(message, "*** Warning: Chunk (%s) is getting replaced with the missing version ***\n", stream2_get_container_name(objp->srid));
    stream2_release(objp->srid);
    const char* missing_name = nullptr;
    if (objp->get_sub_type() == OT_SUB_MOVER_GENERAL)
    {
        missing_name = "missing_chunk.rfgchunkx";
    }
    else if (objp->get_sub_type() == OT_SUB_MOVER_RFG)
    {
        missing_name = "missing_destroyable.rfgchunkx";
    }
    if (missing_name != nullptr)
    {
        c_chunk_base* chunk = chunk_get_base_by_name(missing_name);
        objp->srid = stream2_container_find(missing_name);
        if (chunk != nullptr && objp->get_obj_type() == OT_MOVER)
        {
            ((object_mover*)objp)->chunk_obj_uid = chunk->object_identifiers.to_pointer()->unique_id;
        }
    }
    stream2_request(objp->srid);
    return true;
}

// 0x0093E3D0 (:898)
void world::stream_object(object* objp)
{
    if (!objp->obj_flags.streamed)
    {
        objp->obj_flags.streamed = 1;
        if (objp->obj_flags.streaming)
        {
            num_streaming_objects--;
        }
        objp->obj_flags.streaming = 0;
        objp->stream(true);
        stream_parent_notify(objp);
    }
}

// 0x0091D700 (:931)
void world::destream_object(object* objp)
{
    if (objp->obj_flags.streamed)
    {
        objp->stream(false);
    }
    if (objp->srid != 0 && (objp->obj_flags.streaming || objp->obj_flags.streamed))
    {
        stream2_release(objp->srid);
    }
    if (objp->obj_flags.streaming)
    {
        num_streaming_objects--;
    }
    objp->obj_flags.streaming = 0;
    objp->obj_flags.streamed = 0;
}

// 0x00940550 (:957)
bool world::process_streaming_zone(world_zone* wz)
{
    if (wz == nullptr || stream2_get_status(wz->srid) != ST_LOADED || wz->state == WZS_LOADED)
    {
        return false;
    }
    char filename[64];
    cf_add_ext(filename, sizeof(filename), wz->name, ".rfgzone_pc", false);
    zone_file* zf = zone_find(filename);
    if (zf == nullptr)
    {
        return false;
    }
    if (zf->header.version < 26)
    {
        // Old zone files have no zone object: create one from the zone's name and district.
        vector offset;
        rfg_zonex_get_zone_offset(wz->name, offset);
        obj_zone_props props(wz->name, offset, zf->header.district_hash);
        if (World.create_object_internal(obj_zone::get_classname_hash(), props, nullptr, 0xFFFFFFFF) == nullptr)
        {
            return false;
        }
    }
    if (!serialize_zone(wz, zf, false))
    {
        wz->state = WZS_UNLOADED;
        return false;
    }
    wz->zone_objp = obj_zone_from_grid_id(wz->gid);
    if (wz->zone_objp == nullptr)
    {
        return false;
    }
    wz->state = WZS_LOADED;
    return true;
}

// 0x0091D750 (:1027)
bool world::stream_zone(world_zone* wz)
{
    if (wz == nullptr || wz->srid == 0)
    {
        return false;
    }
    wz->zone_objp = nullptr;
    stream2_request(wz->srid);
    wz->state = WZS_STREAMING;
    return true;
}

// 0x0091D790 (:1061)
void world::destream_zone(world_zone* wz)
{
    if (wz == nullptr)
    {
        return;
    }
    if (wz->zone_objp != nullptr)
    {
        wz->zone_objp->deinstantiate_resources();
        destroy_object(wz->zone_objp, false);
    }
    wz->zone_objp = nullptr;
    stream2_release(wz->srid);
    wz->state = WZS_UNLOADED;
}

// 0x0091D7E0 (:1083)
void world::stream_request(object* objp, unsigned int srid, stream2_priority priority)
{
    // Only changes the priority of a resource that is neither requested nor loaded (and only if the object has one).
    if (!objp->obj_flags.streaming && !objp->obj_flags.streamed && objp->srid != 0)
    {
        objp->srid = srid;
        stream2_set_priority(srid, (unsigned char)priority);
    }
}

// 0x0091D810 (:1098)
void world::get_hierarchy_bbox(vector& bmin, vector& bmax, object* objp)
{
    objp->get_bbox(bmin, bmax);
    for (object* child = objp->child_ptr; child != nullptr;)
    {
        vector child_min;
        vector child_max;
        get_hierarchy_bbox(child_min, child_max, child);
        bbox_expand(bmin, bmax, child_min, child_max);
        child = child->child_next;
        if (child == objp->child_ptr)
        {
            break;
        }
    }
}

// 0x0093D650 (:1113)
void world::stream_parent_notify(object* child)
{
    // Every created ancestor is flagged to check whether all its children are streamed now.
    for (unsigned int parent_handle = child->parent; parent_handle != 0xFFFFFFFF;)
    {
        object** entry = World.object_handle_hash.get_ptr(parent_handle);
        if (entry == nullptr)
        {
            return;
        }
        object* parent = *entry;
        if (parent == nullptr || parent->obj_flags.stub)
        {
            return;
        }
        put_on_flagged_list(parent, 2);
        parent_handle = parent->parent;
    }
}

// 0x0091D880 (:1125)
bool world::apply_stream_parent_notify(object* parent)
{
    // Clears the notification (unless it is the object being processed) and tells the object once its whole hierarchy
    // is streamed; returns whether parent and all its descendants are streamed.
    const bool notified = (parent->obj_flags.flagged_list_state & 2) != 0;
    if (notified && parent != current_flagged_object && (parent->obj_flags.flagged_list_state & 2) != 0)
    {
        parent->obj_flags.flagged_list_state &= ~2;
        if ((parent->obj_flags.flagged_list_state & 5) == 0)
        {
            if (flagged_objects == parent && parent->flagged_next == parent)
            {
                parent->flagged_next = nullptr;
                parent->flagged_prev = nullptr;
                flagged_objects = nullptr;
            }
            else
            {
                if (flagged_objects == parent)
                {
                    flagged_objects = parent->flagged_next;
                }
                parent->flagged_prev->flagged_next = parent->flagged_next;
                parent->flagged_next->flagged_prev = parent->flagged_prev;
                parent->flagged_next = nullptr;
                parent->flagged_prev = nullptr;
            }
        }
    }
    for (object* child = parent->child_ptr; child != nullptr;)
    {
        if (!apply_stream_parent_notify(child))
        {
            return false;
        }
        child = child->child_next;
        if (child == parent->child_ptr)
        {
            break;
        }
    }
    if (notified)
    {
        parent->hierarchy_notify(HN_CHILDREN_STREAMED);
    }
    return parent->obj_flags.streamed;
}

// 0x0092E650 (:1156)
bool world::objects_original_zone_is_serialized(unsigned int handle)
{
    if (handle == 0xFFFFFFFF)
    {
        return false;
    }
    const unsigned int index = handle_zone_index(handle);
    if (index < 257 && World.global_zone_grid[index] != nullptr)
    {
        return World.global_zone_grid[index]->state == WZS_LOADED;
    }
    return false;
}

// 0x00923BA0 (:1174)
world_zone* world::get_world_zone_by_pos(const vector& pos) const
{
    const unsigned int index = pos_zone_index(pos);
    return index < 257 ? global_zone_grid[index] : nullptr;
}

// 0x0091D940 (:1185)
bool world::world_zone_is_border_zone(const world_zone* wz) const
{
    return wz->is_border_zone;
}

// 0x00923C00 (:1191)
world_zone* world::get_world_zone_by_object_handle(unsigned int handle) const
{
    if (handle == 0xFFFFFFFF)
    {
        return nullptr;
    }
    const unsigned int index = handle_zone_index(handle);
    return index < 257 ? global_zone_grid[index] : nullptr;
}

// 0x0091D950 (:1204)
world_zone* world::get_world_zone_by_index(int index) const
{
    return (unsigned int)index < 257 ? global_zone_grid[index] : nullptr;
}

// 0x0091D970 (:1215)
world_zone* world::get_world_zone_by_grid_id(unsigned short id) const
{
    const unsigned int index = ((id >> 8) & 0xF) * 16 + (id >> 12);
    return index < 257 ? global_zone_grid[index] : nullptr;
}

// 0x0091D9A0 (:1222)
world_zone* world::get_world_zone_by_name(const char* name) const
{
    for (int i = 0; i < num_territory_zones; i++)
    {
        if (v_stricmp(all_zones[i]->name, name) == 0)
        {
            return all_zones[i];
        }
    }
    return nullptr;
}

// 0x00923C40 (:1235)
int world::get_zones_by_bbox(const vector& _bmin, const vector& _bmax, world_zone** zones, int max_zones)
{
    // The cells of the 16x16 grid overlapped by the box (shrunk by 0.005), row by row.
    vector grid_min;
    vector grid_max;
    rfg_zonex_get_maximal_bbox(grid_min, grid_max);
    const int x_min = (int)floor(((_bmin.x + 0.005f) - grid_min.x) / 511.0f);
    const int x_max = (int)floor(((_bmax.x - 0.005f) - grid_min.x) / 511.0f);
    const int y_max = 15 - (int)floor(((_bmin.z + 0.005f) - grid_min.z) / 511.0f);
    int y = 15 - (int)floor(((_bmax.z - 0.005f) - grid_min.z) / 511.0f);
    int count = 0;
    for (; y <= y_max; y++)
    {
        for (int x = x_min; x <= x_max; x++)
        {
            const unsigned int index = y * 16 + x;
            if (index < 256)
            {
                if (global_zone_grid[index] != nullptr)
                {
                    zones[count++] = global_zone_grid[index];
                }
                if (count >= max_zones)
                {
                    return count;
                }
            }
        }
    }
    return count;
}

// 0x00923FF0 (:1276)
int world::get_obj_zones_by_bbox(const vector& bmin, const vector& bmax, obj_zone** zones, int max_zones)
{
    world_zone* world_zones[9];
    const int num = get_zones_by_bbox(bmin, bmax, world_zones, 9);
    int count = 0;
    for (int i = 0; i < max_zones && i < num; i++)
    {
        if (world_zones[i]->state == WZS_LOADED && world_zones[i]->zone_objp != nullptr)
        {
            zones[count++] = world_zones[i]->zone_objp;
        }
    }
    return count;
}

// 0x0091D9F0 (:1291)
obj_zone* world::get_object_zone_by_index(int index) const
{
    if ((unsigned int)index < 257 && global_zone_grid[index] != nullptr)
    {
        return global_zone_grid[index]->zone_objp;
    }
    return nullptr;
}

// 0x0091DA20 (:1298)
obj_zone* world::get_object_zone_by_grid_id(unsigned short id) const
{
    world_zone* wz = get_world_zone_by_grid_id(id);
    return wz != nullptr ? wz->zone_objp : nullptr;
}
