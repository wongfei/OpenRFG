// Original: volition/rfg/code/pathfinding/road_heap.cpp
//
// The open list of the road spline A* search (stub_impl.md S9). A session takes its arrays from Common_mempool.

#include "volition/rfg/code/pathfinding/road_heap.h"

#include "volition/rfg/code/ai/road_route.h"
#include "volition/rfg/code/objects/object_path_road.h"
#include "volition/rfg/code/rfg.h"
#include "volition/vlib/code/memory/virtual_mempool.h"

road_heap Road_heap; // 0x035B6058

// Magic number: rfg.exe's cap of the spline points a search can visit.
static const unsigned int ROAD_HEAP_MAX_SIZE = 0xBF8;

// 0x00AE9FA0 (.h:85)
road_spline_identifier* road_heap::get_entry_from_index(int index)
{
    if (index < num_road_splines) {
        return rsi + index;
    }
    return nullptr;
}

// 0x00AE9FC0 (.h:102)
float road_heap::get_pathfinding_cost(const road_spline_identifier* rsip) const
{
    const road_heap_data& data = heap_data[rsip->road_spline->heap_reference[rsip->point].heap_data_index];
    return data.est_cost_to_goal + data.arrival_cost;
}

// 0x005DFB30 (.h:140)
road_heap_data* road_heap::get_road_spline_data(const road_spline_identifier* rsip)
{
    const object_path_road_heap_reference& reference = rsip->road_spline->heap_reference[rsip->point];
    if (reference.heap_session_id == session_id) {
        return heap_data + reference.heap_data_index;
    }
    return nullptr;
}

// 0x00AF3700 (:93)
void road_heap::init()
{
    in_session = false;
    session_id = 0;
    num_allocated_heap_data = 0;
    num_road_splines = 0;
}

// 0x00AF9A30 (:110)
unsigned int road_heap::start_session()
{
    end_session();
    num_allocated_heap_data = 0;
    num_road_splines = 0;
    mark = Common_mempool.mark();
    // Magic number: rfg.exe divides the free space by 0x3C here too.
    unsigned int size = Common_mempool.space_free() / 0x3C;
    if (size > ROAD_HEAP_MAX_SIZE - 1) {
        size = ROAD_HEAP_MAX_SIZE;
    }
    max_size = size;
    heap_data = (road_heap_data*)Common_mempool.alloc(size * sizeof(road_heap_data), 4);
    rsi = (road_spline_identifier*)Common_mempool.alloc(max_size * sizeof(road_spline_identifier), 4);
    post_alloc_mark = Common_mempool.mark();
    session_id++;
    in_session = true;
    if (session_id == 0) {
        session_id = 1;
    }
    return session_id;
}

// 0x00AF3710 (:156)
void road_heap::end_session()
{
    if (in_session) {
        in_session = false;
        Common_mempool.restore_to_mark(mark);
        heap_data = nullptr;
        rsi = nullptr;
    }
}

// 0x00AF9AF0 (:402)
bool road_heap::insert_road_spline(object_path_road* road_spline, unsigned short point, object_path_road* arrival_spline, float arrival_cost, float est_cost_to_goal, unsigned short arrival_point)
{
    if (max_size <= num_allocated_heap_data) {
        return false;
    }
    road_spline->heap_reference[point].heap_data_index = (unsigned short)num_allocated_heap_data;
    road_spline->heap_reference[point].heap_session_id = session_id;
    road_heap_data* data = heap_data + num_allocated_heap_data++;
    data->arrival_cost = arrival_cost;
    data->is_open = true;
    data->est_cost_to_goal = est_cost_to_goal;
    data->arrival_spline = arrival_spline != nullptr ? arrival_spline->get_handle() : 0xFFFFFFFF;
    data->arrival_point = arrival_point;
    data->heap_index = (unsigned short)num_road_splines;
    rsi[num_road_splines].road_spline = road_spline;
    rsi[num_road_splines].point = point;
    num_road_splines++;
    percolate(data->heap_index);
    return true;
}

// 0x00AF9BB0 (:447)
void road_heap::adjust_road_spline(int heap_index)
{
    percolate(heap_index);
}

// 0x00AF9BC0 (:458)
road_spline_identifier road_heap::remove_best_road_spline()
{
    road_spline_identifier best;
    if (num_road_splines < 1) {
        best.road_spline = nullptr;
        best.point = 0;
        return best;
    }
    best = rsi[0];
    if (num_road_splines < 2) {
        num_road_splines = 0;
    } else {
        rsi[0] = rsi[num_road_splines - 1];
        get_road_spline_data(rsi)->heap_index = 0;
        num_road_splines--;
        heapify(0);
    }
    get_road_spline_data(&best)->is_open = false;
    return best;
}

// 0x00AF3740 (:498)
void road_heap::heapify(int index)
{
    road_spline_identifier* entry = get_entry_from_index(index);
    if (entry == nullptr) {
        return;
    }
    float cost = get_pathfinding_cost(entry);
    for (;;) {
        int left = index * 2 + 1;
        int right = index * 2 + 2;
        int best_index = index;
        road_spline_identifier* best_entry = entry;
        float best_cost = cost;
        road_spline_identifier* child = get_entry_from_index(left);
        if (child != nullptr) {
            float child_cost = get_pathfinding_cost(child);
            if (child_cost < best_cost) {
                best_index = left;
                best_entry = child;
                best_cost = child_cost;
            }
        }
        child = get_entry_from_index(right);
        if (child != nullptr && get_pathfinding_cost(child) < best_cost) {
            best_entry = child;
            best_index = right;
        }
        if (best_index == index) {
            break;
        }
        road_spline_identifier moved_up = *best_entry;
        *best_entry = *entry;
        get_road_spline_data(best_entry)->heap_index = (unsigned short)best_index;
        *entry = moved_up;
        get_road_spline_data(entry)->heap_index = (unsigned short)index;
        entry = best_entry;
        index = best_index;
    }
}

// 0x00AF3920 (:569)
void road_heap::percolate(int index)
{
    road_spline_identifier* entry = get_entry_from_index(index);
    if (entry == nullptr) {
        return;
    }
    road_spline_identifier moving = *entry;
    int parent = (index - 1) >> 1;
    road_spline_identifier* parent_entry = get_entry_from_index(parent);
    while (index > 0) {
        if (get_pathfinding_cost(parent_entry) <= get_pathfinding_cost(&moving)) {
            break;
        }
        *entry = *parent_entry;
        get_road_spline_data(entry)->heap_index = (unsigned short)index;
        entry = parent_entry;
        index = parent;
        parent = (index - 1) >> 1;
        parent_entry = get_entry_from_index(parent);
    }
    *entry = moving;
    get_road_spline_data(entry)->heap_index = (unsigned short)index;
    heapify(index);
}

// 0x00AF3AB0 (:624)
void road_heap_level_init_pre()
{
    Road_heap.init();
}
