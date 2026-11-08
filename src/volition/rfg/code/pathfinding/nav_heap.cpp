// Original: volition/rfg/code/pathfinding/nav_heap.cpp
//
// The open lists of the nav cell and navpoint A* searches (stub_impl.md S9). A nav_heap session takes its arrays from
// Common_mempool and gives them back at the end; navpoint_heap keeps its 512 entries inline.

#include "volition/rfg/code/pathfinding/nav_heap.h"

#include "volition/rfg/code/objects/object_navpoint.h"
#include "volition/rfg/code/pathfinding/nav_cell.h"
#include "volition/rfg/code/rfg.h"
#include "volition/vlib/code/memory/virtual_mempool.h"

nav_heap Nav_heap; // 0x035BB618
navpoint_heap Navpoint_heap; // 0x035BAE08

// Magic number: rfg.exe's cap of the nav cells a search can visit.
static const unsigned int NAV_HEAP_MAX_SIZE = 0x1000;

// 0x00AE9F50 (.h:59)
nav_cell** nav_heap::get_entry_from_index(int index)
{
    if (index < num_nav_cells) {
        return nav_cells + index;
    }
    return nullptr;
}

// 0x00AE9F70 (.h:65)
float nav_heap::get_pathfinding_cost(const nav_cell* ncp) const
{
    return heap_data[ncp->heap_data_index].est_cost_to_goal + heap_data[ncp->heap_data_index].arrival_cost;
}

// 0x005DB9A0 (.h:90)
nav_heap_cell_data* nav_heap::get_cell_data(const nav_cell* ncp)
{
    if (ncp->heap_session_id == session_id) {
        return heap_data + ncp->heap_data_index;
    }
    return nullptr;
}

// 0x00AF3110 (:184)
void nav_heap::init()
{
    in_session = false;
    session_id = 0;
    num_allocated_heap_data = 0;
    num_nav_cells = 0;
}

// 0x00AF97B0 (:201)
unsigned int nav_heap::start_session()
{
    end_session();
    num_allocated_heap_data = 0;
    num_nav_cells = 0;
    mark = Common_mempool.mark();
    // Magic number: 0x3C = sizeof(nav_heap_cell_data) + sizeof(nav_cell*) + the 0x20 bytes of the cell itself, as rfg.exe
    // divides the free space.
    unsigned int size = Common_mempool.space_free() / 0x3C;
    if (size > NAV_HEAP_MAX_SIZE - 1) {
        size = NAV_HEAP_MAX_SIZE;
    }
    max_size = size;
    heap_data = (nav_heap_cell_data*)Common_mempool.alloc(size * sizeof(nav_heap_cell_data), 4);
    nav_cells = (nav_cell**)Common_mempool.alloc(max_size * sizeof(nav_cell*), 4);
    post_alloc_mark = Common_mempool.mark();
    session_id++;
    in_session = true;
    if (session_id == 0) {
        session_id = 1;
    }
    return session_id;
}

// 0x00AF3120 (:247)
void nav_heap::end_session()
{
    if (in_session) {
        in_session = false;
        Common_mempool.restore_to_mark(mark);
        heap_data = nullptr;
        nav_cells = nullptr;
    }
}

// 0x00AF9870 (:270)
bool nav_heap::insert_cell(nav_cell* ncp, float arrival_cost, float est_cost_to_goal, char arrival_edge, const vector& arrival_pos)
{
    if (max_size <= num_allocated_heap_data) {
        return false;
    }
    ncp->heap_data_index = (unsigned short)num_allocated_heap_data;
    ncp->heap_session_id = session_id;
    nav_heap_cell_data* data = heap_data + num_allocated_heap_data++;
    data->arrival_cost = arrival_cost;
    data->arrival_edge = arrival_edge;
    data->is_open = true;
    data->est_cost_to_goal = est_cost_to_goal;
    data->arrival_pos = arrival_pos;
    data->heap_index = (unsigned short)num_nav_cells;
    nav_cells[num_nav_cells] = ncp;
    num_nav_cells++;
    percolate(data->heap_index);
    return true;
}

// 0x00AF9900 (:309)
void nav_heap::adjust_cell(int heap_index)
{
    percolate(heap_index);
}

// 0x00AF9910 (:320)
nav_cell* nav_heap::remove_best_cell()
{
    if (num_nav_cells < 1) {
        return nullptr;
    }
    nav_cell* best = nav_cells[0];
    if (num_nav_cells < 2) {
        num_nav_cells = 0;
    } else {
        nav_cells[0] = nav_cells[num_nav_cells - 1];
        get_cell_data(nav_cells[0])->heap_index = 0;
        num_nav_cells--;
        heapify(0);
    }
    get_cell_data(best)->is_open = false;
    return best;
}

// 0x00AF3150 (:357)
void nav_heap::heapify(int index)
{
    nav_cell** entry = get_entry_from_index(index);
    if (entry == nullptr) {
        return;
    }
    float cost = get_pathfinding_cost(*entry);
    for (;;) {
        int left = index * 2 + 1;
        int right = index * 2 + 2;
        int best_index = index;
        nav_cell** best_entry = entry;
        float best_cost = cost;
        nav_cell** child = get_entry_from_index(left);
        if (child != nullptr) {
            float child_cost = get_pathfinding_cost(*child);
            if (child_cost < best_cost) {
                best_entry = child;
                best_index = left;
                best_cost = child_cost;
            }
        }
        child = get_entry_from_index(right);
        if (child != nullptr && get_pathfinding_cost(*child) < best_cost) {
            best_entry = child;
            best_index = right;
        }
        if (best_index == index) {
            break;
        }
        nav_cell* moved_down = *entry;
        nav_cell* moved_up = *best_entry;
        *best_entry = moved_down;
        get_cell_data(moved_down)->heap_index = (unsigned short)best_index;
        *entry = moved_up;
        get_cell_data(moved_up)->heap_index = (unsigned short)index;
        entry = best_entry;
        index = best_index;
    }
}

// 0x00AF32E0 (:432)
void nav_heap::percolate(int index)
{
    nav_cell** entry = get_entry_from_index(index);
    if (entry == nullptr) {
        return;
    }
    nav_cell* cell = *entry;
    int parent = (index - 1) >> 1;
    nav_cell** parent_entry = get_entry_from_index(parent);
    while (index > 0) {
        nav_cell* parent_cell = *parent_entry;
        if (get_pathfinding_cost(parent_cell) <= get_pathfinding_cost(cell)) {
            break;
        }
        *entry = parent_cell;
        get_cell_data(parent_cell)->heap_index = (unsigned short)index;
        entry = parent_entry;
        index = parent;
        parent = (index - 1) >> 1;
        parent_entry = get_entry_from_index(parent);
    }
    *entry = cell;
    get_cell_data(cell)->heap_index = (unsigned short)index;
    heapify(index);
}

// 0x00AF3430 (:488)
navpoint** navpoint_heap::get_entry_from_index(int index, bool allow_out_of_range)
{
    if (!allow_out_of_range && num_navpoints <= index) {
        return nullptr;
    }
    return heap + index;
}

// 0x00AF3450 (:499)
void navpoint_heap::init()
{
    num_navpoints = 0;
    in_session = false;
    session_id = 0;
}

// 0x00AF3470 (:510)
unsigned int navpoint_heap::start_session()
{
    session_id++;
    num_navpoints = 0;
    in_session = true;
    if (session_id == 0) {
        session_id = 1;
    }
    return session_id;
}

// 0x00AF34A0 (:529)
void navpoint_heap::end_session()
{
    in_session = false;
}

// 0x00AF9990 (:540)
void navpoint_heap::insert_cell(navpoint* navp)
{
    // Original bug kept: no check against the 512 entries of heap.
    heap[num_navpoints] = navp;
    navp->heap_data.heap_index = (unsigned short)num_navpoints;
    num_navpoints++;
    percolate(num_navpoints - 1);
}

// 0x00AF99D0 (:560)
void navpoint_heap::adjust_cell(navpoint* navp)
{
    percolate(navp->heap_data.heap_index);
}

// 0x00AF99F0 (:570)
navpoint* navpoint_heap::remove_best_cell()
{
    if (num_navpoints < 1) {
        return nullptr;
    }
    navpoint* best = heap[0];
    navpoint* last = *get_entry_from_index(num_navpoints - 1, false);
    heap[0] = last;
    last->heap_data.heap_index = 0;
    num_navpoints--;
    heapify(0);
    return best;
}

// 0x00AF34B0 (:600)
void navpoint_heap::heapify(int index)
{
    navpoint** entry = get_entry_from_index(index, false);
    if (entry == nullptr) {
        return;
    }
    float cost = navpoint_pathfinding_cost(*entry);
    for (;;) {
        int left = index * 2 + 1;
        int right = index * 2 + 2;
        int best_index = index;
        navpoint** best_entry = entry;
        float best_cost = cost;
        if (left < num_navpoints) {
            float child_cost = navpoint_pathfinding_cost(heap[left]);
            if (child_cost < cost) {
                best_index = left;
                best_entry = heap + left;
                best_cost = child_cost;
            }
        }
        if (right < num_navpoints) {
            float child_cost = navpoint_pathfinding_cost(heap[right]);
            if (child_cost < best_cost) {
                best_index = right;
                best_entry = heap + right;
            }
        }
        if (best_index == index) {
            break;
        }
        navpoint* moved_down = *entry;
        navpoint* moved_up = *best_entry;
        *best_entry = moved_down;
        moved_down->heap_data.heap_index = (unsigned short)best_index;
        *entry = moved_up;
        moved_up->heap_data.heap_index = (unsigned short)index;
        entry = best_entry;
        index = best_index;
    }
}

// 0x00AF35B0 (:669)
void navpoint_heap::percolate(int index)
{
    navpoint** entry = get_entry_from_index(index, false);
    if (entry == nullptr) {
        return;
    }
    navpoint* navp = *entry;
    int parent = (index - 1) >> 1;
    navpoint** parent_entry = get_entry_from_index(parent, false);
    while (index > 0) {
        if (navpoint_pathfinding_cost(*parent_entry) <= navpoint_pathfinding_cost(navp)) {
            break;
        }
        navpoint* moved_down = *parent_entry;
        *entry = moved_down;
        moved_down->heap_data.heap_index = (unsigned short)index;
        entry = parent_entry;
        index = parent;
        parent = (index - 1) >> 1;
        parent_entry = get_entry_from_index(parent, false);
    }
    *entry = navp;
    navp->heap_data.heap_index = (unsigned short)index;
    heapify(index);
}

// 0x00AF3670 (:716)
void navpoint_heap::validate(int index, navpoint* parent)
{
    // A debug walk whose asserts were compiled out; the cost calls stay because rfg.exe still makes them.
    for (navpoint** entry = get_entry_from_index(index, false); entry != nullptr; entry = get_entry_from_index(index, false)) {
        if (parent != nullptr) {
            navpoint_pathfinding_cost(*entry);
            navpoint_pathfinding_cost(parent);
        }
        validate(index * 2 + 1, *entry);
        parent = *entry;
        index = index * 2 + 2;
    }
}

// 0x00AF36D0 (:744)
void nav_heap_level_init_pre()
{
    Nav_heap.init();
    Navpoint_heap.init();
}
