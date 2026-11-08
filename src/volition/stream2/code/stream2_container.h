// Original: volition/stream2/code/stream2_container.h
// Implementation: volition/stream2/code/stream2_container.cpp
//
// A streamed container: a named set of primitives that is loaded and released as a unit. Its handle (srid) is the
// container pointer cast to unsigned int (decompiled from stream2_get_status).
// stream2_container::flags (decompiled):
//   0x001 loaded            0x002 queued (Stream_q / Stream_q_passive)  0x004 queued for unload
//   0x008 read in flight    0x010 aborting                              0x020 request again after the abort
//   0x040 error             0x080 primitives are read from <name>.str2_pc with one chain read
//   0x100 passive (loads only through the passive queue)                0x200 no stream generation
// Loading: stream2_request -> enqueue (by cur_priority) -> queue_process (stream2.cpp) calls alloc() through the
// container type's allocator and issues the reads -> stream_wait polls them, hands the container to the loading thread
// (g_pStream2ContainerToLoad, g_stream2ContainerLoadEvent) for load_threaded primitives, then runs the other load_cb on
// the sim thread and finally the container type's load_cb.
// mode (stream2_container_mode): SCM_CONTAINER allocates the whole container in one block, SCM_PRIM allocates each
// primitive separately. last_error points into Error_pool when a load failed.
// stream2_container_header is the header of a container assembly file (.asm_pc, load_container); placement inferred.
#pragma once

#include "volition/stream2/code/stream2_os.h"
#include "volition/vlib/code/memory/object_pool.h"
#include "volition/vlib/code/vlib_core/vlib_core.h"
#include "keen/base/event.hpp"
#include "keen/base/thread.hpp"

struct cfile;
class mempool_base;
class stream2_allocator;
class stream2_prim;

template<typename D, int N, typename I>
class str_data_hash_table_cached;

enum stream2_container_mode
{
    SCM_NONE = 0,
    SCM_CONTAINER = 1,
    SCM_PRIM = 2,
};

// size 0x1C
class stream2_container_def
{
public:
    stream2_container_def(); // 0x004AA460 (:115)
    bool valid(); // 0x004AA480 (:125)

    void (*load_cb)(unsigned int); // 0x00
    void (*unload_cb)(unsigned int); // 0x04
    char* name; // 0x08
    stream2_container_mode mode; // 0x0C
    unsigned char container_type; // 0x10
    stream2_allocator* allocator; // 0x14
    int allocator_idx; // 0x18
};

// size 0x8
struct prim_add_entry
{
    stream2_prim* prim; // 0x00
    int add_index; // 0x04
};

// size 0x44
class stream2_container
{
public:
    // size 0x200
    struct last_error_c
    {
        char error_str[512]; // 0x00
    };

    // size 0x8
    struct entry
    {
        unsigned char enable : 1; // 0x00:0
        unsigned char user0 : 1; // 0x00:1
        unsigned char user1 : 1; // 0x00:2
        unsigned char flags; // 0x01
        stream2_prim* prim; // 0x04
    };

    void init(const char* _name, mempool_base* pool, stream2_container_def* _cdef, bool passive, bool force_no_stream_generation); // 0x004AA490 (:140)
    void clear(); // 0x004AD1D0 (:172)
    void abort(); // 0x004AF7A0 (:225)
    bool abort_wait(); // 0x004B0430 (:242)
    void abort_restart(); // 0x004AA550 (:220)
    bool stream(bool do_alloc); // 0x004AF7C0 (:309)
    bool stream_wait(); // 0x004AFB20 (:431)
    stream2_os_req_status req_poll(); // 0x004AD1F0 (:513)
    void req_abort(); // 0x004AD2F0 (:556)
    void req_release(); // 0x004AD340 (:580)
    bool add_prims(str_data_hash_table_cached<prim_add_entry,1024,unsigned short>* prims, mempool_base* pool, bool skip_validation); // 0x004B04B0 (:626)
    bool set_passive(bool passive); // 0x004AA520 (:185)
    bool enable_prim(const char* prim_name, bool enable); // 0x004AD3A0 (:703)
    bool contains_prim(const char* namev) const; // 0x004AD410 (:750)
    bool contains_prim(stream2_prim* prim) const; // 0x004AA570 (:736)
    const stream2_prim* get_prim(const char* namev) const; // 0x004AA5F0 (:769)
    stream2_prim* get_prim(const char* namev); // 0x004AA5A0 (:755)
    int num_prims() const; // 0x004AA640 (:784)
    void inc_ref_count(); // 0x004AD430 (:790)
    void dec_ref_count(); // 0x004AD460 (:807)
    int get_ref_count() const; // 0x004AA650 (:826)
    bool is_idle() const; // 0x004AA660 (:832)
    bool is_allocated() const; // 0x004AA6A0 (:1205)
    bool alloc(); // 0x004AD7D0 (:949)
    stream2_container_mode get_mode() const; // 0x004A9B90 (.h:141)
    void get_structure_size(unsigned int& cpu_size, bool count_prim_dups) const; // 0x004ADEB0 (:1373)
    void get_loaded_size(unsigned int&, unsigned int&) const;
    bool load(bool fromLoadThread); // 0x004ADC70 (:1211)
    void unload(); // 0x004ADD20 (:1277)
    void calc_container_requirements(int& cpu_bytes_needed, int& cpu_align_needed, int& gpu_bytes_needed, int& gpu_align_needed, int& audio_bytes_needed, int& audio_align_needed) const; // 0x004AD5F0 (:893)
    void calc_container_requirements(int& cpu_bytes_needed, int& cpu_align_needed, int& gpu_bytes_needed, int& gpu_align_needed) const; // 0x004AD490 (:850)
    void get_stream_data_filename(vlib_platform plat, char* filename) const; // 0x004AA6B0 (:1413)
    static bool load_container(cfile* asm_file, mempool_base* pool, bool report_errors, bool is_temp, bool patch_data); // 0x004B1180 (:1749)
private:
    void signal_last_error(const char*, ...); // 0x004AA560 (:603)

    static const int STREAM2_LAST_ERROR_MAX; // 0x012B80D0
    static object_pool<last_error_c,50,1> Error_pool; // 0x01ACBB90
    unsigned char* cpu_allocated; // 0x00
    unsigned char* gpu_allocated; // 0x04
    short ref_count; // 0x08
public:
    short pf_chain_size; // 0x0A
    unsigned int pf_base_offset; // 0x0C
    int pf_total_compressed_read_size; // 0x10
    unsigned int* pf_chain_read_size; // 0x14
    void** pf_chain_read_dest; // 0x18
    entry* entries; // 0x1C
    short num_entries; // 0x20
    short flags; // 0x22
    short cur_priority; // 0x24
    short cur_category; // 0x26
    stream2_container_def* cdef; // 0x28
    char* name; // 0x2C
    last_error_c* last_error; // 0x30
    stream2_container* q_next; // 0x34
    stream2_container* q_prev; // 0x38
    stream2_container* next; // 0x3C
    stream2_container* prev; // 0x40
};

// size 0x8
struct stream2_container_header
{
    unsigned int signature; // 0x00
    unsigned short version; // 0x04
    unsigned short num_containers; // 0x06
};

extern volatile bool g_stream2ContainerLoaded; // 0x01AC3B4A
extern volatile stream2_container* g_pStream2ContainerToLoad; // 0x01AC3B60
extern keen::Event g_stream2ContainerLoadEvent; // 0x01AC3B64
extern keen::Thread g_stream2ContainerLoadThread; // 0x01AD5120

int stream2_container_load(const keen::Thread& thread); // 0x004AF680 (:68)
void stream2_container_create_loading_thread(void (*pInitFunction)()); // 0x004AF760 (:93)
void stream2_container_destroy_loading_thread(); // 0x004AA420 (:100)
