// Original: volition/stream2/code/stream2_os.h
// Implementation: volition/stream2/code/stream2_os.cpp
//
// Read devices of the streaming system (decompiled). stream2_os_system_init creates up to three stream2_os_device
// (Stream2_device, file static in stream2_os.cpp): "Disc" (only when Disc_build), "Memory" (packfiles preloaded into
// memory; stream2_os_init_memory_thread) and "HDD", each with its own thread ("dvd thread", "mem_thread",
// "stream2 hdd thread") that runs process() until shutdown. stream2_os_read picks the device, queues a
// stream2_os_request ordered by priority and returns its handle; stream2_os_poll maps the request status to
// stream2_os_req_status. A chain read (stream2_os_file::chain_read) reads one compressed block and inflates it into
// several destinations (one container's primitives from its .str2_pc file).
#pragma once

#include "volition/vlib/code/os/cs_wrapper.h"
#include "volition/vlib/code/os/sync_event.h"

struct cfile;
class mempool_base;
class stream2_os_device;

enum stream2_os_req_status
{
    SOS_INVALID = 0,
    SOS_BUSY = 1,
    SOS_DONE_SUCCESS = 2,
    SOS_DONE_ERROR = 3,
};

enum req_status
{
    SREQ_UNKNOWN = 0,
    SREQ_UNUSED = 1,
    SREQ_PENDING = 2,
    SREQ_ACTIVE = 3,
    SREQ_CANCEL_PENDING = 4,
    SREQ_CANCEL = 5,
    SREQ_DONE = 6,
    SREQ_ERROR_FOPEN = 7,
    SREQ_ERROR_UNKNOWN = 8,
};

// size 0x64
struct stream2_os_file
{
    stream2_os_file(); // 0x0048CEC0 (.h:81)
    void reset(); // 0x00482EE0 (.h:87)

    char filename[64]; // 0x00
    unsigned int offset; // 0x40
    unsigned int size; // 0x44
    unsigned char* dest; // 0x48
    bool chain_read : 1; // 0x4C:0
    bool encrypted : 1; // 0x4C:1
    int chain_num_reads; // 0x50
    unsigned int* chain_size; // 0x54
    unsigned char** chain_dest; // 0x58
    unsigned short priority; // 0x5C
    unsigned short category; // 0x5E
    cfile* preopened; // 0x60
};

// size 0x90
struct stream2_os_request
{
    stream2_os_request(); // 0x004ADFD0 (:75)
    void clear(); // 0x004AA6D0 (:81)

    stream2_os_file file; // 0x00
    cfile* p_cfile; // 0x64
    unsigned int bytes_read; // 0x68
    volatile req_status status; // 0x6C
    stream2_os_device* device; // 0x70
    int arrival_time; // 0x74
    int service_start; // 0x78
    int service_open_start; // 0x7C
    int service_seek_start; // 0x80
    int service_read_start; // 0x84
    unsigned short next; // 0x88
    unsigned short prev; // 0x8A
    unsigned short q_next; // 0x8C
    unsigned short q_prev; // 0x8E
};

// size 0x90
class stream2_os_device
{
public:
    stream2_os_device(const char* _name, mempool_base* pool, int max_requests); // 0x004AE040 (:175)
    unsigned int read(stream2_os_file& _request); // 0x004AA770 (:211)
    void cancel(unsigned int _req); // 0x004AAB90 (:248)
    req_status poll(unsigned int _req); // 0x004AACD0 (:284)
    void release_handle(unsigned int _req); // 0x004AACF0 (:299)
    bool process(); // 0x004AAF20 (:321)
    void set_thread_handle(unsigned int t); // 0x004AA700 (:125)
    void shutdown_everything(); // 0x004AB3A0 (:543)
    void pause_processing(); // 0x004AA720 (:197)
    void unpause_processing(); // 0x004AA740 (:203)
    bool is_paused(); // 0x004AA710 (:137)

private:
    sync_event trigger_event; // 0x00
    cs_wrapper cs; // 0x04
    unsigned int thread; // 0x40
    bool thread_shutdown; // 0x44
    char name[64]; // 0x45
    volatile bool processing_paused; // 0x85
    unsigned short q; // 0x86
    unsigned short request_free; // 0x88
    unsigned short request_used; // 0x8A
    stream2_os_request* request_pool; // 0x8C
};

void stream2_os_system_init(mempool_base* pool, int max_requests); // 0x004B0680 (:604)
void stream2_os_system_shutdown(); // 0x004AE240 (:663)
bool stream2_os_init_memory_thread(mempool_base* pool, int max_requests); // 0x004AFC50 (:678)
unsigned int stream2_os_read(stream2_os_file& request); // 0x004AE2A0 (:707)
void stream2_os_cancel(unsigned int req); // 0x004AB3D0 (:764)
stream2_os_req_status stream2_os_poll(unsigned int req); // 0x004AB3F0 (:773)
void stream2_os_release_handle(unsigned int req); // 0x004AB440 (:803)
void stream2_os_pause_disc_processing(); // 0x004AE3B0 (:812)
void stream2_os_unpause_disc_processing(); // 0x004AE3F0 (:822)
bool stream2_os_disc_is_paused(); // 0x004AE430 (:833)
