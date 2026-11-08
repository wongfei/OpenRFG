// Original: volition/stream2/code/stream2.h
// Implementation: volition/stream2/code/stream2.cpp, stream2.inl (stream2_data_hash)
//
// Streaming system front end (decompiled):
// - Types are registered once at boot (game_stream2_register_types): stream2_register_primitive_type and
//   stream2_register_container_type copy the registration into the defs tables (255 each, file static).
// - Containers are built between stream2_container_start and stream2_container_finish, adding primitives by file name
//   (stream2_container_add_prim); finish returns the srid. Containers and primitives are kept in hash tables by name
//   (primitives by name and allocator, so the same file in two allocators is two primitives).
// - stream2_request / stream2_release count references; a container with references is queued for loading, one without
//   is queued for unloading. stream2_do_frame(do_passive) runs on the owning thread (Stream_thread_id, set by
//   stream2_set_thread_ownership) from the sim/render handshake: unload queue, load queue, then the passive queue.
//   It returns true while anything is queued.
// - stream2_data_hash<T, N, I> maps (file name, srid) to loaded data for the load callbacks of other systems
//   (materials, character meshes, mesh morphs, voice personas).
#pragma once

#include "volition/stream2/code/stream2_container.h"
#include "volition/stream2/code/stream2_prim.h"
#include "volition/vlib/code/util/hash_table.h"

class mempool;
class stream2_allocator;

namespace keen
{
    struct TaskSystem;
}

enum stream2_priority
{
    STREAM_PRIORITY_INVALID = 0,
    STREAM_PRIORITY_AUDIO_PRECACHE_LOAD = 127,
    STREAM_PRIORITY_NORMAL = 128,
    STREAM_PRIORITY_AUDIO_STREAM_LOAD = 128,
    STREAM_PRIORITY_AUDIO_CACHE_LOAD = 128,
    STREAM_PRIORITY_HIGH = 200,
    STREAM_PRIORITY_OBJECT = 200,
    STREAM_PRIORITY_HUMAN = 201,
    STREAM_PRIORITY_AUDIO_FETCH_STREAMING_SOUND = 250,
    STREAM_PRIORITY_UI_DOC = 253,
    STREAM_PRIORITY_TERRAIN = 254,
    STREAM_PRIORITY_MAX = 255,
};

enum stream2_category
{
    STREAM_CAT_GENERAL = 0,
    STREAM_CAT_LEVEL = 1,
    STREAM_CAT_AUDIO = 2,
    STREAM_CAT_VEHICLES = 3,
    STREAM_CAT_HUMANS = 4,
    NUM_STREAM_CATEGORIES = 5,
};

enum stream2_status_val
{
    ST_UNLOADED = 0,
    ST_LOADING = 1,
    ST_ABORTING = 2,
    ST_LOADED = 3,
    ST_ERROR = 4,
};

// size 0x68
class stream2_primitive_type_reg
{
public:
    // size 0x1
    struct s2ptr_flags
    {
        unsigned char debug_skippable : 1; // 0x00:0
        unsigned char alloc_split : 1; // 0x00:1
        unsigned char load_threaded : 1; // 0x00:2
    };

    stream2_primitive_type_reg(); // 0x007B9920 (.h:59)

    unsigned char prim_type; // 0x00
    char name[64]; // 0x01
    const char* cpu_ext[2]; // 0x44
    const char* gpu_ext[2]; // 0x4C
    bool (*load_cb)(const char*, unsigned int, unsigned char*, int, unsigned char*, int); // 0x54
    void (*unload_cb)(const char*, unsigned int); // 0x58
    int cpu_memalign; // 0x5C
    int gpu_memalign; // 0x60
    s2ptr_flags flags; // 0x64
};

// size 0x54
class stream2_container_type_reg
{
public:
    stream2_container_type_reg(); // 0x007B9950 (.h:103)

    unsigned char container_type; // 0x00
    char name[64]; // 0x01
    void (*load_cb)(unsigned int); // 0x44
    void (*unload_cb)(unsigned int); // 0x48
    unsigned char allocator_type; // 0x4C
    stream2_container_mode mode; // 0x50
};

// size 0x4C
struct stream2_prim_record
{
    char name[64]; // 0x00
    unsigned char type; // 0x40
    unsigned char allocator; // 0x41
    unsigned char flags; // 0x42
    unsigned char split_ext_index; // 0x43
    int cpu_size; // 0x44
    int gpu_size; // 0x48
};

// Maps (file name, srid) to data owned by a load callback. The key is the 64-bit combination of the name hash and
// the srid (make_key).
// size 4 + data_hash_table (for example 0x1010 for <material*,256,unsigned short>)
template<typename T, int N, typename I>
class stream2_data_hash
{
public:
    bool get(const char* fname, unsigned int srid, T& data); // <material *,256,unsigned short> 0x00809A80 (.h:129)
    bool exists(const char*, unsigned int);
    bool insert(const char* fname, unsigned int srid, T data); // <material *,256,unsigned short> 0x00800FE0 (.h:145)
    void remove(const char* fname, unsigned int srid); // <material *,256,unsigned short> 0x00801030 (.h:153)
private:
    uint64_t make_key(const char* fname, unsigned int srid); // <material *,256,unsigned short> 0x007FA880 (.h:162)
public:
    stream2_data_hash(); // <material *,256,unsigned short> 0x0080B520
    ~stream2_data_hash(); // <material *,256,unsigned short> 0x00808AC0

private:
    data_hash_table<uint64_t, T, &uint64_hash, N, I> hash; // 0x00
};

extern stream2_container* Stream_containers; // 0x01AC3B50
extern stream2_prim* Stream_prims; // 0x01AC3B54
extern int Stream2_container_allocated; // 0x01AC3B5C

void stream2_container_system_init(); // 0x004AC1B0 (:506)
void stream2_container_system_shutdown(); // 0x004B0D10 (:524)
void stream2_set_priority(unsigned int srid, unsigned char priority); // 0x004A9DE0 (:540)
void stream2_set_category(unsigned int srid, unsigned char category); // 0x004A9E10 (:563)
bool stream2_container_get_passive(unsigned int srid); // 0x004A9E30 (:595)
bool stream2_container_set_passive(unsigned int srid, bool passive); // 0x004AC200 (:606)
void stream2_system_init(keen::TaskSystem* pTaskSystem); // 0x004AC230 (:1149)
void stream2_system_shutdown(); // 0x004B1320 (:1170)
bool stream2_do_frame(bool do_passive); // 0x004B0E00 (:1201)
void stream2_set_thread_ownership(); // 0x004A9E50 (:1235)
void stream2_q_pause(); // 0x004A9E60 (:1241)
void stream2_q_unpause(); // 0x004A9E70 (:1247)
bool stream2_q_busy(); // 0x004A9E80 (:1254)
bool stream2_request(unsigned int srid); // 0x004AEF40 (:1273)
bool stream2_preload(unsigned int srid); // 0x004B0E50 (:1323)
void stream2_release(unsigned int srid); // 0x004B02A0 (:1413)
stream2_status_val stream2_get_status(unsigned int srid); // 0x004AC250 (:1458)
int stream2_get_ref_count(unsigned int srid); // 0x004AC2A0 (:1513)
bool stream2_register_primitive_type(stream2_primitive_type_reg& prim, mempool* pool); // 0x004AC2B0 (:1526)
bool stream2_register_container_type(stream2_container_type_reg& container, mempool* pool); // 0x004A9EC0 (:1561)
void q_sanity_check(stream2_prim* prim); // 0x004AC380 (:1589)
bool stream2_container_start(const char* name, mempool_base* pool, unsigned char tpe, bool passive, bool force_no_stream_generation); // 0x004AC440 (:1611)
stream2_prim* stream2_container_add_prim(const char* filename, const char* category, unsigned char tpe, unsigned char allocator, bool report_errors_immediately, unsigned char add_flags, stream2_prim_record* precalc); // 0x004B0310 (:1655)
bool stream2_container_add_prim(stream2_prim* prim); // 0x004AEFA0 (:1752)
void stream2_container_abort(); // 0x004AC4F0 (:1774)
unsigned int stream2_container_finish(bool skip_allocator_size_verification, bool patch); // 0x004B0F10 (:1795)
bool stream2_container_delete(const char* name); // 0x004B1360 (:1855)
bool stream2_container_delete(unsigned int srid); // 0x004B1050 (:1864)
unsigned int stream2_container_find(const char* name); // 0x004AF030 (:1907)
stream2_container* stream_container_find_container(const char* name); // 0x004AF080 (:1916)
bool stream2_container_data_is_unique(unsigned int srid); // 0x004A9F50 (:1926)
bool stream2_container_enable_prim(unsigned int srid, const char* prim_name, bool enable); // 0x004AF0C0 (:1938)
const char* stream2_get_container_name(unsigned int srid); // 0x004A9F70 (:1956)
stream2_container* stream2_get_container(unsigned int srid); // 0x004A9F80 (:1968)
unsigned int stream2_get_srid(stream2_container* sc); // 0x004A9F90 (:1977)
void stream2_reset_allocated_stats(); // 0x004A9FA0 (:1987)
bool stream2_read_container_data(const char* filename, mempool_base* tmp_pool, mempool_base* pool, bool report_errors, bool is_temp, bool patch_data); // 0x004B1660 (:2105)
bool stream2_read_container_data(cfile* f, mempool_base* pool, bool report_errors, bool is_temp, bool patch_data); // 0x004B13B0 (:2142)
stream2_container* stream2_get_first_container(); // 0x004A9FB0 (:2185)
stream2_container* stream2_get_next_container(stream2_container* current); // 0x004A9FC0 (:2190)

#include "volition/stream2/code/stream2.inl"
