// Original: volition/rfg/code/os/dlmalloc/dmalloc.h
// Implementation: volition/rfg/code/os/dlmalloc/dmalloc.cpp
//
// Doug Lea's malloc (dlmalloc 2.7-style), used as a general heap on a memory block given at init (dl_system_init), with
// the game's additions (level init, free chunk walking, usage queries). The allocator internals (malloc_state,
// malloc_chunk) stay in dmalloc.cpp. Placement of free_chunk_in/free_chunk_out/get_free_chunk_state inferred.
#pragma once

enum get_free_chunk_state
{
    GET_FREE_CHUNK_STATE_SYSTEM = 0,
    GET_FREE_CHUNK_STATE_TOP = 1,
    GET_FREE_CHUNK_STATE_FAST = 2,
    GET_FREE_CHUNK_STATE_NORMAL = 3,
};

// size 0x28
struct mallinfo
{
    int arena; // 0x00
    int ordblks; // 0x04
    int smblks; // 0x08
    int hblks; // 0x0C
    int hblkhd; // 0x10
    int usmblks; // 0x14
    int fsmblks; // 0x18
    int uordblks; // 0x1C
    int fordblks; // 0x20
    int keepcost; // 0x24
};

// size 0xC
struct free_chunk_in
{
    get_free_chunk_state state; // 0x00
    int next_index; // 0x04
    void* next_ptr; // 0x08
};

// size 0x8
struct free_chunk_out
{
    unsigned int pos; // 0x00
    unsigned int size; // 0x04
};

void* dlmalloc(unsigned int bytes); // 0x00AE9C90 (:1654)
void dlfree(void* m); // 0x00AE90F0 (:1665)
void* dlrealloc(void* m, unsigned int bytes); // 0x00AE9CC0 (:1674)
void* dlmemalign(unsigned int alignment, unsigned int bytes); // 0x00AE9CF0 (:1684)
void* dlvalloc(unsigned int bytes); // 0x00AE9D20 (:1695)
void* dlpvalloc(unsigned int bytes); // 0x00AE9D70 (:1706)
void* dlcalloc(unsigned int n, unsigned int elem_size); // 0x00AE9DC0 (:1717)
void** dlindependent_calloc(unsigned int n, unsigned int elem_size, void** chunks); // 0x00AE9E30 (:1729)
void** dlindependent_comalloc(unsigned int n, unsigned int* sizes, void** chunks); // 0x00AE9E70 (:1740)
void dlcfree(void* m); // 0x00AE9110 (:1751)
int dlmalloc_trim(unsigned int s); // 0x00AE8E20 (:1760)
unsigned int dlmalloc_usable_size(void* m); // 0x00AE8E50 (:1771)
void dlmalloc_stats(); // 0x00AE8EB0 (:1782)
mallinfo dlmallinfo(); // 0x00AE8ED0 (:1791)
int dlmallopt(int p, int v); // 0x00AE8F70 (:1803)
void dl_system_init(void* mem, int bytes); // 0x00AE83D0 (:5682)
void dl_level_init(); // 0x00AE83F0 (:5691)
void dl_level_deinit(); // 0x00AE8400 (:5705)
int dl_get_allocated_bytes(); // 0x00AE90A0 (:5717)
bool dl_get_next_free_chunk(free_chunk_out* chunk_out, free_chunk_in* chunk_in); // 0x00AE8410 (:5731)
int dl_get_heap_size(); // 0x00AE8650 (:5872)
int dl_get_size(void* p); // 0x00AE8660 (:5877)
