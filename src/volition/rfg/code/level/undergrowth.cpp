// Original: volition/rfg/code/level/undergrowth.cpp
//
// Undergrowth (lod.md L2): grass, scrub and small rocks drawn as clone meshes around the camera.
//
// Data. A territory cell's terrain (rfg_terrainx_shared.h) has undergrowth layers (a model list with probabilities,
// colours and scale noise, a maximum density, a fade range and a placement method) and a 64x64 grid of 8 m cells over
// the cell's box; each grid cell lists its layers with a density and an 8x8 bit mask of 1 m squares where clones may
// stand, and optionally single placements (fixed positions, scales and colours: the rocks).
//
// Zones and layers. undergrowth_register_terrain_data (inlined in terrain_load_terrain) makes a zone per terrain with
// a layer per undergrowth layer. Each layer keeps a square grid of active cells around the eye (grid_size = 2 * the
// fade distance in cells), each grid entry a ring of active cells (one per model). undergrowth_process (the
// simulation frame, after the movers' post-simulation) moves every layer's grid with the camera (the player with
// Undergrowth_use_player_pos): cells leaving the grid are destroyed, cells entering it are placed (create_cells).
//
// Placement. create_cells seeds the C runtime's rand() with the grid cell's position and the layer's seed, so a cell
// always gets the same clones: the single placements, then num_objects = density / 255 * max_density clones placed
// evenly (a jittered square grid) or randomly; each clone picks a model by probability, stands on the terrain
// (havok_terrain_height), optionally tilted to the terrain normal (model flag 1), turned randomly, scaled by the scale
// noise and coloured between the model's two colours, and is kept only on the cell's bit mask. The clones of each
// model become one active cell, queued on the layer's cells_to_create.
//
// Rendering. undergrowth_update_render_all (an update-render task, so the render half) gives each queued cell a clone
// mesh instance of its foliage mesh (from Undergrowth_clone_pool, 8192 clones in Undergrowth_mempool) with the
// layer's fade range and the mesh's LOD descriptions, and moves it into the grid. It then processes the destruction
// events: every clone inside an explosion's sphere (its radius * Foliage_explosion_radius_mult) is deleted; nano
// events are dropped. Fading cells are never created in the binary; the code that ages them out remains.
//
// Wind. process_zone also moves 8 wind slots (Undergrowth_wind_infos) towards new random headings and sine speeds every
// 1-2.2 s and makes Render_wind_info's control vectors from them and the weather's wind, which
// rl_clone_mesh_instance::set_wind_info ignores (empty in the binary). The wind's new values come from the global
// random generator (fl_rand_range, int_rand_range) on the simulation thread, so a game with undergrowth draws
// differently from one without it (lod.md 1.6). The placement uses the C runtime's generator, which the simulation
// does not use.
//
// Notes (decompiled behaviour, kept as is): the zone's register and unregister are hashed by the terrain's address;
// the wind is updated once per zone per frame (a slot's timer only expires once, so further zones only recompute the
// vectors); the active cell pool holds 256 cells and the clone pool 8192 clones, and cells that do not fit are dropped.
// The wind's x87 arithmetic is evaluated in double here (its results reach nothing).

#include "volition/rfg/code/level/undergrowth.h"

#include "volition/render_lib/code/rl_clone_mesh_instance.h"
#include "volition/render_lib/code/rl_mesh.h"
#include "volition/rfg/code/camera/rfg_camera.h"
#include "volition/rfg/code/code_effects/material_effects.h"
#include "volition/rfg/code/game_render/game_render_task.h"
#include "volition/rfg/code/gamewide/game_render.h"
#include "volition/rfg/code/geom/foliage_mesh.h"
#include "volition/rfg/code/havok/havok.h"
#include "volition/rfg/code/level/rfg_terrainx.h"
#include "volition/rfg/code/level/rfg_terrainx_shared.h"
#include "volition/rfg/code/level/undergrowth_shared.h"
#include "volition/rfg/code/objects/humans/player.h"
#include "volition/rfg/code/rfg.h"
#include "volition/rfg/code/weather/wind.h"
#include "volition/vlib/code/cfile/cfile.h"
#include "volition/vlib/code/math/ix.h"
#include "volition/vlib/code/math/matrix.h"
#include "volition/vlib/code/math/rand.h"
#include "volition/vlib/code/math/vector2.h"
#include "volition/vlib/code/memory/object_pool.h"
#include "volition/vlib/code/memory/page_mempool.h"
#include "volition/vlib/code/memory/rfg_split_mempool.h"
#include "volition/vlib/code/memory/virtual_mempool.h"
#include "volition/vlib/code/memory/vmemory.h"
#include "volition/vlib/code/os/cs_wrapper.h"
#include "volition/vlib/code/os/frametime.h"
#include "volition/vlib/code/time/timestamp.h"
#include "volition/vlib/code/util/hash_table.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

struct undergrowth_active_cell;
struct undergrowth_layer;

// ---- TU-private types

// size 0x5C
// An undergrowth layer of a zone: its active cell grid around the eye (cell_grid[grid_size * y + x], each a ring of
// active cells, -1 while a cell is still to be placed), offset into the terrain's 64x64 undergrowth grid.
struct undergrowth_layer
{
    undergrowth_layer_data* base_data; // 0x00
    uint32 orig_index; // 0x04, the layer's index in the terrain
    undergrowth_active_cell* cells_to_create; // 0x08, placed cells waiting for a clone mesh instance (a ring)
    uint32 grid_size; // 0x0C
    int grid_offset_x; // 0x10
    int grid_offset_y; // 0x14
    undergrowth_active_cell** cell_grid; // 0x18
    foliage_mesh* meshes[16]; // 0x1C, the models' foliage meshes (looked up until streamed in)
};

// size 0x38
// The clones of one model in one grid cell: placed elements until the render task gives them an instance.
struct undergrowth_active_cell
{
    undergrowth_active_cell(); // 0x0091FB00

    rl_clone_mesh_instance* r_inst; // 0x00
    int num_clone_elements; // 0x04
    rl_clone_mesh_element* clone_elements; // 0x08
    int mesh_index; // 0x0C
    sint16 grid_x; // 0x10
    sint16 grid_y; // 0x12
    vector position; // 0x14, the grid cell's centre on the terrain (the instance's position)
    vector2 ws_bbox_min; // 0x20, unused
    vector2 ws_bbox_max; // 0x28, unused
    undergrowth_active_cell* prev; // 0x30
    undergrowth_active_cell* next; // 0x34
};

// size 0xC
struct fading_cell
{
    fading_cell(); // 0x0091FB20

    rl_clone_mesh_instance* r_inst; // 0x00
    uint32 mat_fx_handle; // 0x04
    timestamp timer; // 0x08
};

// size 0x48
struct undergrowth_zone
{
    undergrowth_zone(); // 0x00926D40

    const rfg_terrain* base_data; // 0x00
    object_pool_client<undergrowth_layer,1> layers; // 0x08, from Layer_pool
    object_pool_client<fading_cell,1> fading_cells; // 0x28, from Fading_cell_pool
};

// size 0x18
// A wind slot: its heading offset (degrees) moves from prev to next between the updates, its sine input advances by
// the step (radians per second, scaled by the wind speed).
struct undergrowth_wind_info
{
    float prev_wind_offset; // 0x00
    float next_wind_offset; // 0x04
    float cur_sin_input; // 0x08
    float cur_sin_step; // 0x0C
    uint32 next_update_time; // 0x10
    uint32 last_update_time; // 0x14
};

static_assert(sizeof(undergrowth_layer) == 0x5C, "sizeof(undergrowth_layer)");
static_assert(sizeof(undergrowth_active_cell) == 0x38, "sizeof(undergrowth_active_cell)");
static_assert(sizeof(fading_cell) == 0xC, "sizeof(fading_cell)");
static_assert(sizeof(undergrowth_zone) == 0x48, "sizeof(undergrowth_zone)");
static_assert(sizeof(undergrowth_wind_info) == 0x18, "sizeof(undergrowth_wind_info)");

// ---- tweakables (retail values)

static float Foliage_ref_wind_mph = 400.0f; // 0x01668ACC, the wind speed of full sway
static float Foliage_max_wind_heading_offset = 45.0f; // 0x01668AD0, degrees
static float Foliage_min_wind_sin_offset = 180.0f; // 0x01668AD4, degrees per second
static float Foliage_max_wind_sin_offset = 360.0f; // 0x01668AD8
static int Foliage_min_update_ms = 1000; // 0x01668ADC
static int Foliage_max_update_ms = 2200; // 0x01668AE0
static float Foliage_wind_sin_strength = 0.9f; // 0x01668AE4
static float Foliage_explosion_radius_mult = 0.65f; // 0x01668AEC
static bool Undergrowth_use_player_pos = false; // 0x02CA1672

// ---- state

static data_hash_table<unsigned int,undergrowth_zone*,&hash_uint,9,unsigned char> Zone_hash(false); // 0x01668B00
static undergrowth_wind_info Undergrowth_wind_infos[8]; // 0x02CCBA88
static bool Undergrowth_init; // 0x02CEC1E2
static rl_clone_mesh_pool* Undergrowth_clone_pool; // 0x02CEC1E4
static object_pool<destruction_event,8,1> Destruction_event_pool; // 0x02F972E0
static object_pool<undergrowth_active_cell,256,1> Active_cell_pool; // 0x02F9C820
static object_pool<undergrowth_zone,9,1> Zone_pool; // 0x02FD98E0
static game_render_task Update_render_task_undergrowth; // 0x02FDD868
static object_pool<undergrowth_layer,32,1> Layer_pool; // 0x02FDD890
static object_pool<fading_cell,16,1> Fading_cell_pool; // 0x02FDE550
static rl_clone_mesh_wind_info Render_wind_info; // 0x02FE50B0

// Magic numbers: the undergrowth grid cell is 8 m (0.125 cells per metre); a territory cell has 64x64 of them.
static const float UNDERGROWTH_CELL_SIZE = 8.0f;
static const int UNDERGROWTH_GRID_RES = 64;
// Magic number: a layer grid that was never placed (grid_offset_x/y).
static const int UNDERGROWTH_NO_OFFSET = -0x7FFFFFFF;
// Magic number: a grid entry still to be placed.
static undergrowth_active_cell* const UNDERGROWTH_CELL_PENDING = (undergrowth_active_cell*)-1;
// Magic number: the clones of Undergrowth_clone_pool.
static const int UNDERGROWTH_MAX_CLONES = 0x2000;
// Model flags: 1 tilts the clones to the terrain normal (single placements: only turned about up; without it they
// tumble), 2 makes them sway in the wind.
static const unsigned char MODEL_FLAG_ALIGN_TO_GROUND = 1;
static const unsigned char MODEL_FLAG_WIND_ANIMATED = 2;
// Magic numbers: the degrees-to-radians factor, pi and two pi of the binary (floats widened to double).
static const double DEG_TO_RAD = (double)0.017453f;
static const double PI_D = (double)3.1415927f;
static const double TWO_PI_D = (double)6.2831855f;

// 0x0091FB00
// Uninitialized.
undergrowth_active_cell::undergrowth_active_cell()
{
}

// 0x0091FB20
fading_cell::fading_cell()
{
}

// 0x0091FB10
// Uninitialized.
destruction_event::destruction_event()
{
}

// 0x00926D40
undergrowth_zone::undergrowth_zone()
{
}

// ---- rings of active cells (inlined list code, not in the binary as functions)

// Appends the cell to the ring at head (the head stays).
static void cell_ring_append(undergrowth_active_cell*& head, undergrowth_active_cell* cell)
{
    if (head == nullptr)
    {
        cell->next = cell;
        cell->prev = cell;
        head = cell;
        return;
    }
    cell->prev = head->prev;
    cell->next = head;
    head->prev->next = cell;
    head->prev = cell;
}

// Unlinks the cell from the ring at head.
static void cell_ring_remove(undergrowth_active_cell*& head, undergrowth_active_cell* cell)
{
    if (head == cell)
    {
        if (cell->next == cell)
        {
            cell->next = nullptr;
            cell->prev = nullptr;
            head = nullptr;
            return;
        }
        head = cell->next;
    }
    if (cell->prev != nullptr)
    {
        cell->prev->next = cell->next;
    }
    if (cell->next != nullptr)
    {
        cell->next->prev = cell->prev;
    }
    cell->next = nullptr;
    cell->prev = nullptr;
}

// 0x00932D60 (:665, LTCG: cell_p in ESI)
// Frees the cell's placed clones, its instance and the cell.
static void destroy_cell(undergrowth_active_cell* cell_p)
{
    if (cell_p == nullptr)
    {
        return;
    }
    if (cell_p->clone_elements != nullptr)
    {
        Undergrowth_mempool.release_bytes(cell_p->clone_elements);
        cell_p->clone_elements = nullptr;
    }
    if (cell_p->r_inst != nullptr)
    {
        cell_p->r_inst->release();
        cell_p->r_inst = nullptr;
    }
    Active_cell_pool.free(cell_p);
}

// Destroys every cell of the ring at head (the walk of destroy_zone and move_cells, inlined).
static void destroy_cell_ring(undergrowth_active_cell*& head)
{
    undergrowth_active_cell* cell = head;
    while (cell != nullptr)
    {
        // The next cell, unless the ring is back at its head.
        undergrowth_active_cell* const next = cell->next != head ? cell->next : nullptr;
        cell_ring_remove(head, cell);
        destroy_cell(cell);
        cell = next;
    }
}

// 0x00932B50 (:249)
// Removes the cell's material effect, releases its instance and frees it.
void destroy_fading_cell(undergrowth_zone* zone_p, fading_cell* fcell_p)
{
    if (fcell_p->mat_fx_handle != 0xFFFFFFFF)
    {
        material_effects_remove(fcell_p->mat_fx_handle);
        fcell_p->mat_fx_handle = 0xFFFFFFFF;
    }
    if (fcell_p->r_inst != nullptr)
    {
        fcell_p->r_inst->release();
        fcell_p->r_inst = nullptr;
    }
    zone_p->fading_cells.free(fcell_p);
}

// 0x00923140 (:264)
// Deletes the clones of the layer's grid cells under the event's sphere that lie inside it. The grid cells are the
// layer grid's (clamped) cells overlapping the sphere's box.
static void process_destruction_event_for_layer(undergrowth_layer* cur_layer, const destruction_event* cur_event, undergrowth_zone* zone_p)
{
    mempool_mark_scope mark(Vlib_scratch_pool);
    const terrain_data* data = zone_p->base_data->terrain_data_p;
    const float rel_x = cur_event->pos.x - data->bmin.x;
    const float rel_z = cur_event->pos.z - data->bmin.z;
    const float radius = cur_event->radius;
    const float cell_scale = 1.0f / UNDERGROWTH_CELL_SIZE;
    const int grid_size = (int)cur_layer->grid_size;
    int x_start = (int)floorf((rel_x - radius) * cell_scale) - cur_layer->grid_offset_x;
    int x_end = (int)ceilf((radius + rel_x) * cell_scale) - cur_layer->grid_offset_x;
    int y_start = (int)floorf((rel_z - radius) * cell_scale) - cur_layer->grid_offset_y;
    int y_end = (int)ceilf((radius + rel_z) * cell_scale) - cur_layer->grid_offset_y;
    x_start = x_start < 0 ? 0 : (x_start > grid_size ? grid_size : x_start);
    y_start = y_start < 0 ? 0 : (y_start > grid_size ? grid_size : y_start);
    x_end = x_end < 0 ? 0 : (x_end > grid_size ? grid_size : x_end);
    y_end = y_end < 0 ? 0 : (y_end > grid_size ? grid_size : y_end);
    for (int x = x_start; x < x_end; x++)
    {
        for (int y = y_start; y < y_end; y++)
        {
            const int index = grid_size * y + x;
            undergrowth_active_cell* cell = cur_layer->cell_grid[index];
            while (cell != nullptr)
            {
                rl_clone_mesh_instance* r_inst = cell->r_inst;
                if (r_inst != nullptr)
                {
                    int num_clones;
                    const rl_clone_mesh_element* clones = r_inst->get_clones(num_clones);
                    int* clone_indices = (int*)Vlib_scratch_pool->alloc(num_clones * sizeof(int), 0);
                    const vector& inst_pos = r_inst->get_position();
                    const vector local_pos(cur_event->pos.x - inst_pos.x, cur_event->pos.y - inst_pos.y, cur_event->pos.z - inst_pos.z);
                    int num_hit = 0;
                    for (int i = 0; i < num_clones; i++)
                    {
                        rl_clone_mesh_info info;
                        clones[i].get(info);
                        if (ix_point_in_sphere(local_pos, cur_event->radius, info.pos))
                        {
                            clone_indices[num_hit++] = i;
                        }
                    }
                    if (num_hit > 0)
                    {
                        r_inst->delete_clones(clone_indices, num_hit);
                    }
                }
                cell = cell->next;
                if (cell == cur_layer->cell_grid[index])
                {
                    break;
                }
            }
        }
    }
}

// 0x00932B90 (:365)
// Every pending event against every zone's layers, then freed. Nano events only are freed.
static void process_destruction_events()
{
    csspinlock_scope lock(Destruction_event_pool.thread_access);
    destruction_event* cur_event = Destruction_event_pool.get_first(0);
    while (cur_event != nullptr)
    {
        destruction_event* const next_event = Destruction_event_pool.get_next(cur_event);
        if (cur_event->type != UDET_NANO)
        {
            csspinlock_scope zone_lock(Zone_pool.thread_access);
            for (undergrowth_zone* zone_p = Zone_pool.get_first(0); zone_p != nullptr; zone_p = Zone_pool.get_next(zone_p))
            {
                csspinlock_scope layer_lock(zone_p->layers.thread_access);
                for (undergrowth_layer* cur_layer = zone_p->layers.get_first(0); cur_layer != nullptr; cur_layer = zone_p->layers.get_next(cur_layer))
                {
                    process_destruction_event_for_layer(cur_layer, cur_event, zone_p);
                }
            }
        }
        Destruction_event_pool.free(cur_event);
        cur_event = next_event;
    }
}

// 0x0092B590 (:389, LTCG: total_weights in EAX)
// A model by probability (rand() % the probabilities' sum); 0 without any.
static int pick_model(undergrowth_layer* layer, int total_weights)
{
    if (total_weights == 0)
    {
        return 0;
    }
    int r = rand() % total_weights;
    const undergrowth_layer_data* layer_data = layer->base_data;
    for (int i = 0; i < layer_data->num_models; i++)
    {
        r -= layer_data->models[i].model_probability;
        if (r < 0)
        {
            return i;
        }
    }
    return 0;
}

// 0x0091BEE0 (:409, LTCG: height in ECX, norm in EAX)
// Not called in the binary (the placements call havok_terrain_height themselves).
static bool get_terrain_height_and_norm(float& height, vector& norm, undergrowth_zone* zone, vector pos)
{
    return havok_terrain_height(height, pos.x, pos.z, false, &norm);
}

// Not in the binary (inlined in the placements): the sum of the layer's model probabilities.
static int total_model_weights(const undergrowth_layer* layer)
{
    const undergrowth_layer_data* layer_data = layer->base_data;
    int total_weights = 0;
    for (int i = 0; i < layer_data->num_models; i++)
    {
        total_weights += layer_data->models[i].model_probability;
    }
    return total_weights;
}

// Not in the binary (inlined in the placements): a colour channel of a model colour (0xAARRGGBB).
static float color_channel(unsigned int color, int shift)
{
    // Magic number: 8-bit channels.
    return (float)((double)((color >> shift) & 0xFF) / 255.0);
}

// Not in the binary (inlined in the placements): the model's colour lerp(color_1, color_2, t), computed in double.
static rl_color_float model_color(const undergrowth_layer_model_data& model_data, float t)
{
    const float r1 = color_channel(model_data.color_1, 16);
    const float g1 = color_channel(model_data.color_1, 8);
    const float b1 = color_channel(model_data.color_1, 0);
    const float a1 = color_channel(model_data.color_1, 24);
    const float r2 = color_channel(model_data.color_2, 16);
    const float g2 = color_channel(model_data.color_2, 8);
    const float b2 = color_channel(model_data.color_2, 0);
    const float a2 = color_channel(model_data.color_2, 24);
    return rl_color_float((float)(((double)r2 - (double)r1) * (double)t + (double)r1), (float)(((double)g2 - (double)g1) * (double)t + (double)g1),
                          (float)(((double)b2 - (double)b1) * (double)t + (double)b1), (float)(((double)a2 - (double)a1) * (double)t + (double)a1));
}

// Not in the binary (inlined in the placements): the orientation scaled by s.
static void scale_matrix(matrix& orient, float s)
{
    orient.rvec.x *= s;
    orient.rvec.y *= s;
    orient.rvec.z *= s;
    orient.uvec.x *= s;
    orient.uvec.y *= s;
    orient.uvec.z *= s;
    orient.fvec.x *= s;
    orient.fvec.y *= s;
    orient.fvec.z *= s;
}

// Not in the binary (inlined in the placements): the model's active cell of the grid cell (allocated on the first
// clone, with room for alloc_count clones; none when the cell pool or the mempool is out), then the clone at pos
// (world) relative to the cell's position.
static void add_clone(undergrowth_active_cell** model_cells, int model, int num_objects, int alloc_count, const vector& cell_wpos, float x, float y, float z, const matrix& orient,
                      const rl_color_float& color)
{
    undergrowth_active_cell* cell = model_cells[model];
    if (cell == nullptr)
    {
        if (!Active_cell_pool.can_alloc() || !Undergrowth_mempool.can_alloc(num_objects * sizeof(rl_clone_mesh_element), 0))
        {
            return;
        }
        cell = Active_cell_pool.alloc(0);
        model_cells[model] = cell;
        cell->mesh_index = model;
        cell->num_clone_elements = 0;
        cell->clone_elements = (rl_clone_mesh_element*)Undergrowth_mempool.alloc(alloc_count * sizeof(rl_clone_mesh_element), 0);
        cell->r_inst = nullptr;
        cell->position = cell_wpos;
    }
    rl_clone_mesh_element* element = cell->clone_elements + cell->num_clone_elements++;
    rl_clone_mesh_info info;
    info.pos.set(x - cell_wpos.x, y - cell_wpos.y, z - cell_wpos.z);
    info.orient = orient;
    info.color = color;
    // Magic number: the random value is 0-7 (the shader's sway phase).
    info.rand_value = (float)((double)(float)(rand() % 1000) / 1000.0 * 7.0);
    element->set(info);
}

// Not in the binary (inlined in the placements): one clone of the even or random placement at (x, z) on the terrain:
// tilted to the ground (model flag 1), turned, scaled and coloured, kept where the cell's bit mask allows.
static void place_ground_clone(undergrowth_active_cell** model_cells, int num_objects, undergrowth_layer* layer, const undergrowth_cell_layer_data* cell_layer_data, const vector2& min_xz,
                               const vector& cell_wpos, int model, float x, float z)
{
    float height;
    vector normal;
    havok_terrain_height(height, x, z, false, &normal);
    matrix orient(IDENTITY_MATRIX);
    const undergrowth_layer_model_data& model_data = layer->base_data->models[model];
    if ((model_data.model_flags & MODEL_FLAG_ALIGN_TO_GROUND) != 0)
    {
        // The up axis along the terrain normal.
        matrix ground;
        ground.make_rvec(normal, RIGHT_VECTOR);
        orient.rvec = ground.rvec;
        orient.uvec = ground.fvec;
        orient.fvec.set(ground.uvec.x * -1.0f, ground.uvec.y * -1.0f, ground.uvec.z * -1.0f);
    }
    // Magic number: whole degrees.
    orient.rotate_around_local_y((float)((double)(float)(rand() % 360) / 180.0 * PI_D));
    // Magic numbers: the scale noise is a byte of 1/255 steps, applied as +-noise.
    const float scale = (float)(1.0 - ((double)(float)(rand() % 1000) / 500.0 - 1.0) * ((double)(float)model_data.scale_noise / 255.0));
    const rl_color_float color = model_color(model_data, (float)((double)(float)(rand() % 1000) / 1000.0));
    scale_matrix(orient, scale);
    // Magic number: the bit mask is 8x8 squares of 1 m.
    const int bit = (int)((double)x - (double)min_xz.x) + (int)((double)z - (double)min_xz.y) * 8;
    if ((cell_layer_data->bit_mask[bit / 8] & (1 << (bit % 8))) == 0)
    {
        return;
    }
    add_clone(model_cells, model, num_objects, num_objects, cell_wpos, x, height, z, orient, color);
}

// 0x0092B5E0 (:417)
// num_objects clones at random positions in the cell. LTCG drops zone (unused).
static void randomly_place_objects(undergrowth_active_cell** model_cells, int num_objects, undergrowth_zone* zone, undergrowth_layer* layer, const undergrowth_cell_layer_data* cell_layer_data,
                                   const vector2& min_xz, const vector2& max_xz, const vector& cell_wpos)
{
    const int total_weights = total_model_weights(layer);
    for (int i = 0; i < num_objects; i++)
    {
        const int model = pick_model(layer, total_weights);
        // Magic number: positions in 1/1000 steps of the cell.
        const float x = (float)((double)(float)(rand() % 1000) / 1000.0 * ((double)max_xz.x - (double)min_xz.x) + (double)min_xz.x);
        const float z = (float)((double)(float)(rand() % 1000) / 1000.0 * ((double)max_xz.y - (double)min_xz.y) + (double)min_xz.y);
        place_ground_clone(model_cells, num_objects, layer, cell_layer_data, min_xz, cell_wpos, model, x, z);
    }
}

// 0x0092BE00 (:505)
// A square grid of (int)sqrt(num_objects) clones per side, each jittered within 3/4 of its square. LTCG drops zone.
static void evenly_place_objects(undergrowth_active_cell** model_cells, int num_objects, undergrowth_zone* zone, undergrowth_layer* layer, const undergrowth_cell_layer_data* cell_layer_data,
                                 const vector2& min_xz, const vector2& max_xz, const vector& cell_wpos)
{
    const int total_weights = total_model_weights(layer);
    const int per_side = (int)(float)sqrt((double)(float)num_objects);
    const float step = (float)(((double)max_xz.x - (double)min_xz.x) / (double)(float)per_side);
    // Magic numbers: 3/4 of a square is jitter, starting 1/8 of a square in.
    const float jitter = (float)((double)step * 0.75);
    if (per_side <= 0)
    {
        return;
    }
    const double start = (double)step * 0.25 * 0.5;
    for (int row = 0; row < per_side; row++)
    {
        const float z_offset = (float)((double)(float)row * (double)step + start);
        for (int col = 0; col < per_side; col++)
        {
            const float x_offset = (float)((double)(float)col * (double)step + start);
            const int model = pick_model(layer, total_weights);
            const float x = (float)(((double)(float)(rand() % 1000) / 1000.0 * (double)jitter + (double)x_offset) + (double)min_xz.x);
            const float z = (float)(((double)(float)(rand() % 1000) / 1000.0 * (double)jitter + (double)z_offset) + (double)min_xz.y);
            place_ground_clone(model_cells, num_objects, layer, cell_layer_data, min_xz, cell_wpos, model, x, z);
        }
    }
}

// 0x0092C720 (:603, LTCG: cell_layer_data in EDI; zone dropped)
// The cell's single placements: each at its position with its scale and colour lerp, turned about up (and tumbled
// about x unless model flag 1). Their cells have room for the model's extra clones too.
// Notes (decompiled behaviour, kept as is): the mempool check is for num_objects clones, the allocation for
// num_objects plus the model's extra clones.
static void place_single_objects(undergrowth_active_cell** model_cells, int num_objects, undergrowth_zone* zone, undergrowth_layer* layer, const single_undergrowth_cell_layer_data* cell_layer_data,
                                 const vector& cell_wpos)
{
    for (int i = 0; i < (int)cell_layer_data->num_single_ug; i++)
    {
        const single_undergrowth_data& single = cell_layer_data->single_ug[i];
        const int model = (int)single.mesh_index;
        matrix orient;
        orient.make_ry((float)((double)(float)(rand() % 360) / 180.0 * PI_D));
        const undergrowth_layer_model_data& model_data = layer->base_data->models[model];
        if ((model_data.model_flags & MODEL_FLAG_ALIGN_TO_GROUND) == 0)
        {
            matrix tumble;
            tumble.make_rx((float)((double)(float)(rand() % 360) / 180.0 * PI_D));
            orient = orient * tumble;
        }
        const rl_color_float color = model_color(model_data, single.color_lerp);
        scale_matrix(orient, single.scale);
        const int alloc_count = cell_layer_data->num_extra_models[model] + num_objects;
        add_clone(model_cells, model, num_objects, alloc_count, cell_wpos, single.pos.x, single.pos.y, single.pos.z, orient, color);
    }
}

// 0x00932DB0 (:694)
// Places the layer's clones in grid cell (x, y) of its grid: the terrain grid cell's layer entry (none: nothing),
// its single placements, its density's worth of even or random clones, then each model's clones moved into an
// exact-size block and queued for the render task.
static void create_cells(undergrowth_zone* zone, undergrowth_layer* layer, int x, int y)
{
    mempool_mark_scope mark(Vlib_scratch_pool);
    const int grid_y = layer->grid_offset_y + y;
    const int grid_x = layer->grid_offset_x + x;
    terrain_data* data = zone->base_data->terrain_data_p;
    const undergrowth_cell_data& cell_data = data->m_ug_cell_data[grid_y * UNDERGROWTH_GRID_RES + grid_x];
    int cell_layer_index = -1;
    for (int i = 0; i < cell_data.num_layer_datas; i++)
    {
        if (data->m_ug_cell_layer_data[cell_data.layer_data_index + i].layer_index == layer->orig_index)
        {
            cell_layer_index = cell_data.layer_data_index + i;
            break;
        }
    }
    if (cell_layer_index < 0)
    {
        return;
    }
    const undergrowth_cell_layer_data* cell_layer_data = &data->m_ug_cell_layer_data[cell_layer_index];
    const single_undergrowth_cell_layer_data* singles = data->m_sug_cell_layer_data.to_pointer() != nullptr ? &data->m_sug_cell_layer_data[cell_layer_index] : nullptr;
    if ((cell_layer_data == nullptr || cell_layer_data->density == 0) && singles == nullptr)
    {
        return;
    }
    const float min_x = (float)((double)(float)grid_x * (double)UNDERGROWTH_CELL_SIZE + (double)data->bmin.x);
    const float min_z = (float)((double)(float)grid_y * (double)UNDERGROWTH_CELL_SIZE + (double)data->bmin.z);
    const vector2 min_xz(min_x, min_z);
    const vector2 max_xz((float)((double)min_x + (double)UNDERGROWTH_CELL_SIZE), (float)((double)min_z + (double)UNDERGROWTH_CELL_SIZE));
    const float center_x = (float)(((double)max_xz.x - (double)min_x) * 0.5 + (double)min_x);
    const float center_z = (float)(((double)max_xz.y - (double)min_z) * 0.5 + (double)min_z);
    float height;
    vector normal;
    havok_terrain_height(height, center_x, center_z, false, &normal);
    const vector cell_wpos(center_x, height, center_z);
    undergrowth_layer_data* layer_data = layer->base_data;
    // Magic number: the density is a byte of 1/255 steps of the layer's maximum.
    const int num_objects = (int)((double)(float)((double)(float)cell_layer_data->density / 255.0) * (double)layer_data->max_density);
    undergrowth_active_cell** model_cells = (undergrowth_active_cell**)Vlib_scratch_pool->alloc(layer_data->num_models * sizeof(undergrowth_active_cell*), 0);
    memset(model_cells, 0, layer_data->num_models * sizeof(undergrowth_active_cell*));
    // Magic numbers: the seed is the grid cell's position (x in bits 0-7, y in bits 8-15) and the layer's seed above.
    srand(((unsigned int)(grid_y & 0xFF) << 8) + (unsigned int)(grid_x & 0xFF) | ((unsigned int)layer_data->rand_seed << 16));
    if (singles != nullptr)
    {
        place_single_objects(model_cells, num_objects, zone, layer, singles, cell_wpos);
    }
    if (layer_data->placement_method == 0)
    {
        evenly_place_objects(model_cells, num_objects, zone, layer, cell_layer_data, min_xz, max_xz, cell_wpos);
    }
    else if (layer_data->placement_method == 1)
    {
        randomly_place_objects(model_cells, num_objects, zone, layer, cell_layer_data, min_xz, max_xz, cell_wpos);
    }
    for (int i = 0; i < layer_data->num_models; i++)
    {
        undergrowth_active_cell* cell = model_cells[i];
        if (cell == nullptr)
        {
            continue;
        }
        // The placed clones move through scratch memory into a block of their exact size.
        mempool_mark_scope copy_mark(Vlib_scratch_pool);
        const unsigned int size = cell->num_clone_elements * sizeof(rl_clone_mesh_element);
        rl_clone_mesh_element* placed = cell->clone_elements;
        void* copy = Vlib_scratch_pool->alloc(size, 0);
        memcpy(copy, placed, size);
        Undergrowth_mempool.release_bytes(placed);
        if (Undergrowth_mempool.can_alloc(size, 0))
        {
            cell->clone_elements = (rl_clone_mesh_element*)Undergrowth_mempool.alloc(cell->num_clone_elements * sizeof(rl_clone_mesh_element), 0);
            if (cell->clone_elements != nullptr)
            {
                memcpy(cell->clone_elements, copy, cell->num_clone_elements * sizeof(rl_clone_mesh_element));
                cell->grid_x = (sint16)x;
                cell->grid_y = (sint16)y;
                cell_ring_append(layer->cells_to_create, cell);
                continue;
            }
        }
        cell->num_clone_elements = 0;
        cell->clone_elements = nullptr;
        destroy_cell(cell);
    }
}

// 0x009332C0 (:807, LTCG: terrain_data in ECX)
// A zone for the terrain with a layer per undergrowth layer (while the pools last): its grid size from the fade
// distance, an empty grid (from Undergrowth_mempool; a layer that does not fit is dropped) and its models' foliage
// meshes as far as they are streamed in. None when the zone pool is full.
// Notes (decompiled behaviour, kept as is): the model name is copied without its extension and the copy is unused;
// at most 16 models are looked up (meshes[16]).
static undergrowth_zone* create_zone(const rfg_terrain* terrain_data)
{
    if (!Zone_pool.can_alloc())
    {
        return nullptr;
    }
    undergrowth_zone* zone_p = Zone_pool.alloc(0);
    zone_p->layers.init(&Layer_pool);
    zone_p->fading_cells.init(&Fading_cell_pool);
    zone_p->base_data = terrain_data;
    struct terrain_data* data = terrain_data->terrain_data_p;
    for (unsigned int i = 0; i < data->m_num_ug_layers; i++)
    {
        undergrowth_layer_data* layer_data = &data->m_ug_layers[i];
        if (!Layer_pool.can_alloc())
        {
            continue;
        }
        undergrowth_layer* layer_p = zone_p->layers.alloc(0);
        layer_p->base_data = layer_data;
        layer_p->orig_index = i;
        layer_p->grid_offset_x = UNDERGROWTH_NO_OFFSET;
        layer_p->grid_offset_y = UNDERGROWTH_NO_OFFSET;
        layer_p->cells_to_create = nullptr;
        layer_p->grid_size = (int)ceilf(layer_data->max_fade_dist * (1.0f / UNDERGROWTH_CELL_SIZE)) * 2;
        const unsigned int grid_bytes = layer_p->grid_size * layer_p->grid_size * sizeof(undergrowth_active_cell*);
        if (!Undergrowth_mempool.can_alloc(grid_bytes, 0))
        {
            zone_p->layers.free(layer_p);
            continue;
        }
        layer_p->cell_grid = (undergrowth_active_cell**)Undergrowth_mempool.alloc(grid_bytes, 0);
        memset(layer_p->cell_grid, 0, grid_bytes);
        memset(layer_p->meshes, 0, sizeof(layer_p->meshes));
        for (int m = 0; m < layer_data->num_models && m < 16; m++)
        {
            const char* model_name = layer_data->models[m].model_name.to_pointer();
            char name[64];
            cf_drop_ext(name, sizeof(name), model_name);
            layer_p->meshes[m] = foliage_mesh_find(model_name);
        }
    }
    return zone_p;
}

// 0x00933490 (:873)
// Frees the zone's fading cells, its layers' queued and grid cells and the layers, and the zone.
// Notes (decompiled behaviour, kept as is): the layers' grids stay allocated in Undergrowth_mempool.
static void destroy_zone(undergrowth_zone* zone_p)
{
    {
        csspinlock_scope lock(zone_p->fading_cells.thread_access);
        fading_cell* fcell_p = zone_p->fading_cells.get_first(0);
        while (fcell_p != nullptr)
        {
            fading_cell* const next = zone_p->fading_cells.get_next(fcell_p);
            destroy_fading_cell(zone_p, fcell_p);
            fcell_p = next;
        }
    }
    csspinlock_scope lock(zone_p->layers.thread_access);
    undergrowth_layer* layer_p = zone_p->layers.get_first(0);
    while (layer_p != nullptr)
    {
        undergrowth_layer* const next_layer = zone_p->layers.get_next(layer_p);
        destroy_cell_ring(layer_p->cells_to_create);
        for (unsigned int i = 0; i < layer_p->grid_size * layer_p->grid_size; i++)
        {
            destroy_cell_ring(layer_p->cell_grid[i]);
        }
        zone_p->layers.free(layer_p);
        layer_p = next_layer;
    }
    Zone_hash.remove((unsigned int)zone_p->base_data);
    Zone_pool.free(zone_p);
}

// 0x00933790 (:913)
// The grid moved by (offset_x, offset_y) cells: cells still inside move to their new entry, the others are destroyed;
// new entries are pending (-1).
static void move_cells(undergrowth_layer* layer_p, int offset_x, int offset_y)
{
    mempool_mark_scope mark(Vlib_scratch_pool);
    const int grid_size = (int)layer_p->grid_size;
    undergrowth_active_cell** new_grid = (undergrowth_active_cell**)Vlib_scratch_pool->alloc(grid_size * grid_size * sizeof(undergrowth_active_cell*), 0);
    for (int i = 0; i < grid_size * grid_size; i++)
    {
        new_grid[i] = UNDERGROWTH_CELL_PENDING;
    }
    for (int x = 0; x < grid_size; x++)
    {
        const int new_x = x - offset_x;
        for (int y = 0; y < grid_size; y++)
        {
            const int new_y = y - offset_y;
            const int index = y * grid_size + x;
            if (new_x < 0 || new_x >= grid_size || new_y < 0 || new_y >= grid_size)
            {
                destroy_cell_ring(layer_p->cell_grid[index]);
            }
            else
            {
                new_grid[new_y * grid_size + new_x] = layer_p->cell_grid[index];
            }
        }
    }
    memcpy(layer_p->cell_grid, new_grid, grid_size * grid_size * sizeof(undergrowth_active_cell*));
}

// 0x00933950 (:955, LTCG: eye_xz in ECX)
// Each layer: its models' meshes looked up again until streamed in, its grid centred on the eye's cell (moved, or
// all pending on the first frame) and its pending entries placed (entries outside the terrain's 64x64 grid stay
// empty). Then the wind.
static void process_zone(undergrowth_zone* zone_p, const vector2& eye_xz)
{
    const terrain_data* data = zone_p->base_data->terrain_data_p;
    const float cell_scale = 1.0f / UNDERGROWTH_CELL_SIZE;
    const int eye_x = (int)floorf((eye_xz.x - data->bmin.x) * cell_scale);
    const int eye_y = (int)floorf((eye_xz.y - data->bmin.z) * cell_scale);
    {
        csspinlock_scope lock(zone_p->layers.thread_access);
        for (undergrowth_layer* layer_p = zone_p->layers.get_first(0); layer_p != nullptr; layer_p = zone_p->layers.get_next(layer_p))
        {
            for (int m = 0; m < layer_p->base_data->num_models && m < 16; m++)
            {
                if (layer_p->meshes[m] == nullptr)
                {
                    layer_p->meshes[m] = foliage_mesh_find(layer_p->base_data->models[m].model_name.to_pointer());
                }
            }
            const int half_size = (int)(float)ceil((double)(layer_p->base_data->max_fade_dist * cell_scale));
            const int offset_x = eye_x - half_size;
            const int offset_y = eye_y - half_size;
            if (offset_x == layer_p->grid_offset_x && offset_y == layer_p->grid_offset_y)
            {
                continue;
            }
            if (layer_p->grid_offset_x == UNDERGROWTH_NO_OFFSET || layer_p->grid_offset_y == UNDERGROWTH_NO_OFFSET)
            {
                for (unsigned int i = 0; i < layer_p->grid_size * layer_p->grid_size; i++)
                {
                    layer_p->cell_grid[i] = UNDERGROWTH_CELL_PENDING;
                }
            }
            else
            {
                move_cells(layer_p, offset_x - layer_p->grid_offset_x, offset_y - layer_p->grid_offset_y);
            }
            layer_p->grid_offset_x = offset_x;
            layer_p->grid_offset_y = offset_y;
            const int grid_size = (int)layer_p->grid_size;
            for (int x = 0; x < grid_size; x++)
            {
                for (int y = 0; y < grid_size; y++)
                {
                    const int index = grid_size * y + x;
                    if ((unsigned int)(layer_p->grid_offset_x + x) < (unsigned int)UNDERGROWTH_GRID_RES && (unsigned int)(layer_p->grid_offset_y + y) < (unsigned int)UNDERGROWTH_GRID_RES)
                    {
                        if (layer_p->cell_grid[index] == UNDERGROWTH_CELL_PENDING)
                        {
                            layer_p->cell_grid[index] = nullptr;
                            create_cells(zone_p, layer_p, x, y);
                        }
                    }
                    else
                    {
                        layer_p->cell_grid[index] = nullptr;
                    }
                }
            }
        }
    }

    // The wind: each slot draws a new heading offset, sine speed and update time when its time is up, then gives a
    // control vector (the heading's direction scaled by the sway and the wind speed).
    float wind_mph = 0.0f;
    float wind_dir = 0.0f;
    weather_wind_get_speed(&wind_mph, &wind_dir);
    const float wind_scale = (float)((double)wind_mph / (double)Foliage_ref_wind_mph);
    for (int i = 0; i < 8; i++)
    {
        undergrowth_wind_info& wind = Undergrowth_wind_infos[i];
        if (wind.next_update_time <= Frametime_total_milliseconds)
        {
            wind.prev_wind_offset = wind.next_wind_offset;
            wind.next_wind_offset = fl_rand_range((float)((double)Foliage_max_wind_heading_offset * -1.0), Foliage_max_wind_heading_offset);
            wind.cur_sin_step = (float)((double)fl_rand_range(Foliage_min_wind_sin_offset, Foliage_max_wind_sin_offset) * DEG_TO_RAD);
            wind.last_update_time = Frametime_total_milliseconds;
            wind.next_update_time = int_rand_range(Foliage_min_update_ms, Foliage_max_update_ms) + Frametime_total_milliseconds;
        }
        const float t = (float)((double)(Frametime_total_milliseconds - wind.last_update_time) / (double)(wind.next_update_time - wind.last_update_time));
        const float heading = (float)((((double)wind.next_wind_offset - (double)wind.prev_wind_offset) * (double)t + (double)wind.prev_wind_offset) * DEG_TO_RAD);
        vector dir(0.0f, 0.0f, 1.0f);
        dir.rotate_around_y_axis((float)((double)heading + (double)wind_dir));
        wind.cur_sin_input = (float)((double)wind.cur_sin_step * (double)Frametime * (double)wind_scale + (double)wind.cur_sin_input);
        if ((double)wind.cur_sin_input > TWO_PI_D)
        {
            wind.cur_sin_input = (float)((double)wind.cur_sin_input - TWO_PI_D);
        }
        const float wave = (float)sin((double)wind.cur_sin_input);
        const float sway = (float)(1.0 - (double)(float)((double)wave * 0.5 + 1.0) * (double)Foliage_wind_sin_strength);
        Render_wind_info.control_vectors[i].set((float)((double)(float)((double)dir.x * (double)sway) * (double)wind_scale), (float)((double)(float)((double)dir.y * (double)sway) * (double)wind_scale),
                                                (float)((double)(float)((double)sway * (double)dir.z) * (double)wind_scale));
    }
}

// 0x00933F50 (:1074)
// The render half: each layer's queued cells get a clone mesh instance (when the factory, the cell's foliage mesh and
// the clone pool allow; else the cell is destroyed) at the cell's position with the layer's fade range and the mesh's
// LODs, their placed clones are freed (the instance copied them into the clone pool) and the cell moves into its grid
// entry. Expired fading cells go. Then the wind (ignored) and the destruction events.
// Notes (decompiled behaviour, kept as is): a cell whose instance could not be created still moves into the grid,
// without an instance.
static void undergrowth_update_render_all(void*, rl_renderer*)
{
    {
        csspinlock_scope zone_lock(Zone_pool.thread_access);
        for (undergrowth_zone* zone_p = Zone_pool.get_first(0); zone_p != nullptr; zone_p = Zone_pool.get_next(zone_p))
        {
            {
                csspinlock_scope layer_lock(zone_p->layers.thread_access);
                for (undergrowth_layer* layer_p = zone_p->layers.get_first(0); layer_p != nullptr; layer_p = zone_p->layers.get_next(layer_p))
                {
                    undergrowth_active_cell* cell = layer_p->cells_to_create;
                    while (cell != nullptr)
                    {
                        undergrowth_active_cell* const next = cell->next != layer_p->cells_to_create ? cell->next : nullptr;
                        cell_ring_remove(layer_p->cells_to_create, cell);
                        for (int m = 0; m < layer_p->base_data->num_models && m < 16; m++)
                        {
                            if (layer_p->meshes[m] == nullptr)
                            {
                                layer_p->meshes[m] = foliage_mesh_find(layer_p->base_data->models[m].model_name.to_pointer());
                            }
                        }
                        foliage_mesh* fmesh = layer_p->meshes[cell->mesh_index];
                        if (!rl_clone_mesh_instance::can_create() || fmesh == nullptr || !Undergrowth_clone_pool->can_alloc(cell->num_clone_elements))
                        {
                            destroy_cell(cell);
                            cell = next;
                            continue;
                        }
                        cell->r_inst = rl_clone_mesh_instance::create();
                        if (cell->r_inst != nullptr)
                        {
                            rl_mesh* mesh = fmesh->m_shared_header.m_mesh.to_pointer();
                            cell->r_inst->init(mesh, fmesh->m_shared_header.m_material_map.to_pointer(), Undergrowth_clone_pool, cell->clone_elements, cell->num_clone_elements, RLCMET_INSTANCES, 0.0f,
                                               0);
                            cell->r_inst->set_position(cell->position);
                            cell->r_inst->set_rotation(IDENTITY_MATRIX);
                            rl_clone_mesh_fade_range fade_range;
                            fade_range.start = layer_p->base_data->min_fade_dist;
                            fade_range.end = layer_p->base_data->max_fade_dist;
                            cell->r_inst->set_fade_range(fade_range);
                            cell->r_inst->set_lod_descs(fmesh->m_lod_descs.to_pointer(), fmesh->m_num_lods);
                            cell->r_inst->update_partition_system();
                            vector bmin;
                            vector bmax;
                            mesh->get_subpiece_local_bb(fmesh->m_lod_descs.to_pointer()[0].submesh_index, bmin, bmax);
                            cell->r_inst->set_wind_reference_height(bmax.y);
                            cell->r_inst->set_wind_animated((layer_p->base_data->models[cell->mesh_index].model_flags & MODEL_FLAG_WIND_ANIMATED) != 0);
                        }
                        Undergrowth_mempool.release_bytes(cell->clone_elements);
                        cell->clone_elements = nullptr;
                        cell->num_clone_elements = 0;
                        cell_ring_append(layer_p->cell_grid[cell->grid_y * (int)layer_p->grid_size + cell->grid_x], cell);
                        cell = next;
                    }
                }
            }
            csspinlock_scope fading_lock(zone_p->fading_cells.thread_access);
            fading_cell* fcell_p = zone_p->fading_cells.get_first(0);
            while (fcell_p != nullptr)
            {
                fading_cell* const next = zone_p->fading_cells.get_next(fcell_p);
                if (!fcell_p->timer.valid() || fcell_p->timer.elapsed())
                {
                    destroy_fading_cell(zone_p, fcell_p);
                }
                fcell_p = next;
            }
        }
    }
    rl_clone_mesh_instance::set_wind_info(Render_wind_info);
    process_destruction_events();
}

// 0x00934470 (:1186)
// The foliage containers' definitions, the update-render task, and the clone pool (8192 clones in
// Undergrowth_mempool) with the wind cleared.
void undergrowth_system_init()
{
    if (Undergrowth_init)
    {
        return;
    }
    Undergrowth_init = true;
    undergrowth_load_shared(&Common_mempool, &Permanent_mempool, PLATFORM_PC);
    Update_render_task_undergrowth.m_callback = undergrowth_update_render_all;
    game_render_update_render_task_register(&Update_render_task_undergrowth);
    Undergrowth_clone_pool = rl_clone_mesh_pool::create();
    if (Undergrowth_clone_pool != nullptr)
    {
        Undergrowth_clone_pool->init(UNDERGROWTH_MAX_CLONES, &Undergrowth_mempool);
        memset(Undergrowth_wind_infos, 0, sizeof(Undergrowth_wind_infos));
    }
}

// 0x0091BF10 (:1216)
// Notes (decompiled behaviour, kept as is): nothing is released.
void undergrowth_system_shutdown()
{
    if (Undergrowth_init)
    {
        Undergrowth_init = false;
    }
}

// 0x00923560 (:1228)
void undergrowth_level_init()
{
    Zone_hash.clear();
}

// 0x009344E0 (:1239)
// Every zone is destroyed.
void undergrowth_level_shutdown()
{
    {
        csspinlock_scope lock(Zone_pool.thread_access);
        undergrowth_zone* zone_p = Zone_pool.get_first(0);
        while (zone_p != nullptr)
        {
            undergrowth_zone* const next = Zone_pool.get_next(zone_p);
            destroy_zone(zone_p);
            zone_p = next;
        }
    }
    Zone_hash.clear();
}

// 0x009345B0 (:1258)
// Every zone around the eye: the camera's position (the local player's with Undergrowth_use_player_pos).
void undergrowth_process()
{
    vector2 eye_xz;
    if (Undergrowth_use_player_pos && player_get_local_player() != nullptr)
    {
        const player* pp = player_get_local_player();
        eye_xz = vector2(pp->pos.x, pp->pos.z);
    }
    else
    {
        eye_xz = vector2(Rfg_camera.real_pos.x, Rfg_camera.real_pos.z);
    }
    csspinlock_scope lock(Zone_pool.thread_access);
    for (undergrowth_zone* zone_p = Zone_pool.get_first(0); zone_p != nullptr; zone_p = Zone_pool.get_next(zone_p))
    {
        process_zone(zone_p, eye_xz);
    }
}

// 0x009346C0 (:1283)
// Inlined in terrain_load_terrain (rfg_terrainx.cpp); no callers in the binary.
void undergrowth_register_terrain_data(const rfg_terrain* terrain_data)
{
    undergrowth_zone* zone_p = create_zone(terrain_data);
    if (zone_p != nullptr)
    {
        Zone_hash.insert_key_data((unsigned int)terrain_data, zone_p);
    }
}

// 0x009346F0 (:1298)
void undergrowth_unregister_terrain_data(const rfg_terrain* terrain_data)
{
    undergrowth_zone* zone_p = nullptr;
    Zone_hash.get((unsigned int)terrain_data, zone_p);
    if (zone_p != nullptr)
    {
        destroy_zone(zone_p);
    }
}

// 0x0091BF30 (:1309)
void undergrowth_set_thread_ownership()
{
    Undergrowth_mempool.set_thread_ownership();
}

// 0x0092CCC0 (:1314)
// Queues a flattening sphere of radius * Foliage_explosion_radius_mult; dropped when 8 are pending.
void undergrowth_create_destruction_event(const vector& pos, float radius, undergrowth_destruction_event_type ugde_type)
{
    destruction_event* event_p = Destruction_event_pool.alloc(0);
    if (event_p == nullptr)
    {
        return;
    }
    event_p->type = ugde_type;
    event_p->shape = UDES_SPHERE;
    event_p->pos = pos;
    event_p->radius = Foliage_explosion_radius_mult * radius;
}

// Not in the binary (lod.md L2 test census).
void undergrowth_take_census(undergrowth_census& census)
{
    memset(&census, 0, sizeof(census));
    for (undergrowth_zone* zone_p = Zone_pool.get_first(0); zone_p != nullptr; zone_p = Zone_pool.get_next(zone_p))
    {
        census.zones++;
        for (fading_cell* fcell_p = zone_p->fading_cells.get_first(0); fcell_p != nullptr; fcell_p = zone_p->fading_cells.get_next(fcell_p))
        {
            census.fading_cells++;
        }
        for (undergrowth_layer* layer_p = zone_p->layers.get_first(0); layer_p != nullptr; layer_p = zone_p->layers.get_next(layer_p))
        {
            census.layers++;
            undergrowth_active_cell* queued = layer_p->cells_to_create;
            while (queued != nullptr)
            {
                census.cells_to_create++;
                queued = queued->next != layer_p->cells_to_create ? queued->next : nullptr;
            }
            for (unsigned int i = 0; i < layer_p->grid_size * layer_p->grid_size; i++)
            {
                undergrowth_active_cell* const head = layer_p->cell_grid[i];
                if (head == UNDERGROWTH_CELL_PENDING)
                {
                    continue;
                }
                undergrowth_active_cell* cell = head;
                while (cell != nullptr)
                {
                    census.grid_cells++;
                    if (cell->r_inst != nullptr)
                    {
                        int num_clones;
                        cell->r_inst->get_clones(num_clones);
                        census.instances++;
                        census.clones += num_clones;
                    }
                    cell = cell->next != head ? cell->next : nullptr;
                }
            }
        }
    }
    for (destruction_event* event_p = Destruction_event_pool.get_first(0); event_p != nullptr; event_p = Destruction_event_pool.get_next(event_p))
    {
        census.pending_events++;
    }
    rl_clone_mesh_census clones;
    rl_clone_mesh_take_census(Undergrowth_clone_pool, clones);
    census.pool_clones_used = clones.pool_clones_used;
    census.pool_clones_max = clones.pool_clones_max;
    census.drawn_instances = clones.drawn_instances;
    for (int i = 0; i < 4; i++)
    {
        census.drawn_by_lod[i] = clones.drawn_by_lod[i];
        census.clones_by_lod[i] = clones.clones_by_lod[i];
    }
}
