// Original: volition/stream2/code/stream2_os.cpp
//
// A request handle is the address of its stream2_os_request (0xFFFFFFFF = none). Each device keeps its requests in a
// pool with two circular index lists (request_free / request_used, linked through next/prev) and a service queue `q`
// (linked through q_next/q_prev). The device thread takes the queue head, opens the file (or uses `preopened`),
// seeks, reads (a chain read inflates one compressed block into several destinations) and sets the final status;
// the owner polls the status and releases the handle, which returns the request to the free list.
// Request status: PENDING in the queue, ACTIVE while serviced, CANCEL_PENDING when cancelled while active (the cfile
// read is aborted), then DONE, CANCEL, ERROR_FOPEN or ERROR_UNKNOWN.
//
// Notes (decompiled behaviour, kept as is):
// - The device constructor pushes every request at the front of the free list.
// - read() never puts a request in front of the queue head (the head may be in service): it scans from the tail
//   for the first request of at least the same priority and inserts after it, or after the head.
// - stream2_os_read dereferences a null request; when cf_find_file fails the media type is not set (zero here, HDD).
//   A Disc media type selects device index -1 unless Disc_build.

#include "volition/stream2/code/stream2_os.h"

#include "volition/rfg/code/rfg.h"
#include "volition/vlib/code/cfile/cfile.h"
#include "volition/vlib/code/memory/mempool_base.h"
#include "volition/vlib/code/os/thread.h"
#include "volition/vlib/code/util/varray.h"
#include "volition/vlib/code/vlib_core/vlib_assert.h"

#include <new>
#include <string.h>

static farray<stream2_os_device*, 3> Stream2_device; // 0x014F9D50
static int Stream2_device_dvd = -1; // 0x014F9D1C, index in Stream2_device or -1 ("Disc", Disc_build only)
static int Stream2_device_hdd = -1; // 0x014F9D20 ("HDD")
static int Stream2_device_memory = -1; // 0x014F9D24 ("Memory", preloaded packfiles)
static bool Stream2_os_init; // 0x01AC3B4B

// ---- index list helpers (inlined in the original)

static unsigned short request_index(const stream2_os_request* pool, const stream2_os_request* r)
{
    return (unsigned short)(r - pool);
}

// Removes r from the circular list `head` (next/prev links).
static void request_list_remove(stream2_os_request* pool, volatile unsigned short& head, stream2_os_request* r)
{
    const unsigned short index = request_index(pool, r);
    if (head == index)
    {
        if (r->next == index)
        {
            r->next = 0xFFFF;
            r->prev = 0xFFFF;
            head = 0xFFFF;
            return;
        }
        head = r->next;
    }
    pool[r->prev].next = r->next;
    pool[r->next].prev = r->prev;
    r->next = 0xFFFF;
    r->prev = 0xFFFF;
}

// Appends r at the tail of the circular list `head` (next/prev links).
static void request_list_append(stream2_os_request* pool, volatile unsigned short& head, stream2_os_request* r)
{
    const unsigned short index = request_index(pool, r);
    if (head == 0xFFFF)
    {
        r->next = index;
        r->prev = index;
        head = index;
        return;
    }
    r->prev = pool[head].prev;
    r->next = head;
    pool[pool[head].prev].next = index;
    pool[head].prev = index;
}

// Removes r from the service queue (q_next/q_prev links).
static void request_queue_remove(stream2_os_request* pool, volatile unsigned short& q, stream2_os_request* r)
{
    const unsigned short index = request_index(pool, r);
    if (q == index)
    {
        if (r->q_next == index)
        {
            r->q_next = 0xFFFF;
            r->q_prev = 0xFFFF;
            q = 0xFFFF;
            return;
        }
        q = r->q_next;
    }
    pool[r->q_prev].q_next = r->q_next;
    pool[r->q_next].q_prev = r->q_prev;
    r->q_next = 0xFFFF;
    r->q_prev = 0xFFFF;
}

// ---- stream2_os_file (inline in the header)

// 0x0048CEC0 (.h:81)
stream2_os_file::stream2_os_file()
{
    reset();
}

// 0x00482EE0 (.h:87)
void stream2_os_file::reset()
{
    chain_read = false;
    encrypted = false;
    filename[0] = '\0';
    size = 0;
    dest = nullptr;
    chain_num_reads = 0;
    chain_size = nullptr;
    chain_dest = nullptr;
    priority = 0;
    category = 0;
    preopened = nullptr;
}

// ---- stream2_os_request

// 0x004ADFD0 (:75)
stream2_os_request::stream2_os_request()
{
    p_cfile = nullptr;
    bytes_read = 0;
    status = SREQ_UNUSED;
    arrival_time = -1;
    service_start = -1;
    service_open_start = -1;
    service_seek_start = -1;
    service_read_start = -1;
}

// 0x004AA6D0 (:81)
void stream2_os_request::clear()
{
    p_cfile = nullptr;
    bytes_read = 0;
    status = SREQ_UNUSED;
    arrival_time = -1;
    service_start = -1;
    service_open_start = -1;
    service_seek_start = -1;
    service_read_start = -1;
}

// ---- stream2_os_device

// 0x004AA700 (:125)
void stream2_os_device::set_thread_handle(unsigned int t)
{
    thread = t;
}

// 0x004AA710 (:137)
bool stream2_os_device::is_paused()
{
    return processing_paused;
}

// 0x004AE020 (:167)
static int thread_dummy(void* data)
{
    stream2_os_device* const device = (stream2_os_device*)data;
    while (device->process())
    {
    }
    return 0;
}

// 0x004AE040 (:175)
stream2_os_device::stream2_os_device(const char* _name, mempool_base* pool, int max_requests)
    : trigger_event(false, false)
    , cs(0)
{
    q = 0xFFFF;
    thread_shutdown = false;
    processing_paused = false;
    request_free = 0xFFFF;
    request_used = 0xFFFF;
    strcpy(name, _name);
    request_pool = (stream2_os_request*)pool->alloc((unsigned int)max_requests * sizeof(stream2_os_request), 0);
    for (int i = 0; i < max_requests; i++)
    {
        stream2_os_request* const r = new (&request_pool[i]) stream2_os_request();
        const unsigned short index = (unsigned short)i;
        if (request_free == 0xFFFF)
        {
            r->next = index;
            r->prev = index;
        }
        else
        {
            request_pool[request_pool[request_free].prev].next = index;
            r->prev = request_pool[request_free].prev;
            request_pool[request_free].prev = index;
            r->next = request_free;
        }
        request_free = index;
    }
}

// 0x004AA720 (:197)
void stream2_os_device::pause_processing()
{
    cs.enter();
    processing_paused = true;
    cs.leave();
}

// 0x004AA740 (:203)
void stream2_os_device::unpause_processing()
{
    cs.enter();
    processing_paused = false;
    trigger_event.signal();
    cs.leave();
}

// 0x004AA770 (:211)
unsigned int stream2_os_device::read(stream2_os_file& _request)
{
    cs.enter();
    if (request_free == 0xFFFF)
    {
        cs.leave();
        return 0xFFFFFFFF;
    }
    stream2_os_request* const r = &request_pool[request_free];
    request_list_remove(request_pool, request_free, r);
    request_list_append(request_pool, request_used, r);
    r->clear();
    r->file = _request;
    r->status = SREQ_PENDING;
    r->device = this;

    const unsigned short index = request_index(request_pool, r);
    stream2_os_request* after = nullptr;
    if (q != 0xFFFF)
    {
        stream2_os_request* const head = &request_pool[q];
        after = &request_pool[head->q_prev];
        while (after != head && _request.priority > after->file.priority)
        {
            after = &request_pool[after->q_prev];
        }
    }
    if (after == nullptr)
    {
        if (q == 0xFFFF)
        {
            r->q_next = index;
            r->q_prev = index;
        }
        else
        {
            request_pool[request_pool[q].q_prev].q_next = index;
            r->q_prev = request_pool[q].q_prev;
            request_pool[q].q_prev = index;
            r->q_next = q;
        }
        q = index;
    }
    else
    {
        r->q_next = after->q_next;
        r->q_prev = request_index(request_pool, after);
        request_pool[after->q_next].q_prev = index;
        after->q_next = index;
    }
    trigger_event.signal();
    cs.leave();
    return (unsigned int)r;
}

// 0x004AAB90 (:248)
void stream2_os_device::cancel(unsigned int _req)
{
    if (_req == 0xFFFFFFFF)
    {
        return;
    }
    stream2_os_request* const r = (stream2_os_request*)_req;
    cs.enter();
    if (r->status == SREQ_UNUSED)
    {
        VLIB_CRITICAL_FAILURE();
    }
    else if (r->status == SREQ_PENDING)
    {
        request_queue_remove(request_pool, q, r);
        r->status = SREQ_CANCEL;
    }
    else if (r->status == SREQ_ACTIVE)
    {
        if (r->p_cfile != nullptr)
        {
            cf_abort_async_read(r->p_cfile);
        }
        r->status = SREQ_CANCEL_PENDING;
    }
    cs.leave();
}

// 0x004AACD0 (:284)
req_status stream2_os_device::poll(unsigned int _req)
{
    if (_req == 0xFFFFFFFF)
    {
        return SREQ_UNKNOWN;
    }
    return ((stream2_os_request*)_req)->status;
}

// 0x004AACF0 (:299)
void stream2_os_device::release_handle(unsigned int _req)
{
    if (_req == 0xFFFFFFFF)
    {
        VLIB_CRITICAL_FAILURE();
        return;
    }
    stream2_os_request* const r = (stream2_os_request*)_req;
    const req_status status = r->status;
    if (status == SREQ_UNKNOWN || status == SREQ_UNUSED || status == SREQ_PENDING || status == SREQ_ACTIVE || status == SREQ_CANCEL_PENDING)
    {
        return;
    }
    cs.enter();
    request_list_remove(request_pool, request_used, r);
    request_list_append(request_pool, request_free, r);
    cs.leave();
}

// 0x004AAF20 (:321)
bool stream2_os_device::process()
{
    trigger_event.wait();
    cs.enter();
    if (thread_shutdown)
    {
        cs.leave();
        return false;
    }
    cs.leave();
    if (processing_paused)
    {
        return true;
    }

    cs.enter();
    for (;;)
    {
        if (thread_shutdown)
        {
            cs.leave();
            return false;
        }
        cs.leave();
        if (processing_paused)
        {
            return true;
        }
        cs.enter();
        if (q == 0xFFFF)
        {
            cs.leave();
            return true;
        }
        stream2_os_request* const r = &request_pool[q];
        r->status = SREQ_ACTIVE;
        cs.leave();

        if (r->status == SREQ_CANCEL_PENDING)
        {
            cs.enter();
            request_queue_remove(request_pool, q, r);
            r->status = SREQ_CANCEL;
        }
        else
        {
            cfile* f = r->file.preopened;
            if (f == nullptr)
            {
                f = cf_open(r->file.filename, "rb", PLATFORM_PC, true);
            }
            r->p_cfile = f;
            if (f == nullptr)
            {
                cs.enter();
                request_queue_remove(request_pool, q, r);
                r->status = SREQ_ERROR_FOPEN;
            }
            else if (r->status == SREQ_CANCEL_PENDING)
            {
                if (r->file.preopened == nullptr)
                {
                    cf_close(r->p_cfile);
                }
                r->p_cfile = nullptr;
                cs.enter();
                request_queue_remove(request_pool, q, r);
                r->status = SREQ_CANCEL;
            }
            else
            {
                cf_seek(r->p_cfile, (int)r->file.offset, CF_SEEK_SET);
                if (r->file.chain_read)
                {
                    r->bytes_read = cf_read_compressed_chain(r->file.chain_dest, r->file.size, r->file.chain_size, (unsigned int)r->file.chain_num_reads, r->p_cfile);
                }
                else
                {
                    r->bytes_read = cf_read(r->file.dest, r->file.size, r->p_cfile, true);
                    cf_wait_async(r->p_cfile);
                }
                cs.enter();
                const cf_error_codes error = cf_error(r->p_cfile);
                if (r->file.preopened == nullptr)
                {
                    cf_close(r->p_cfile);
                }
                r->p_cfile = nullptr;
                if (error == CF_ERROR_NONE)
                {
                    r->status = SREQ_DONE;
                }
                else
                {
                    r->status = error != CF_ERROR_ABORT ? SREQ_ERROR_UNKNOWN : SREQ_CANCEL;
                }
                request_queue_remove(request_pool, q, r);
            }
        }
        cs.leave();
        cs.enter();
    }
}

// 0x004AB3A0 (:543)
void stream2_os_device::shutdown_everything()
{
    cs.enter();
    thread_shutdown = true;
    trigger_event.signal();
    cs.leave();
    thread_wait_until_done(thread);
}

// ---- system

// Creates a device with its thread and adds it to Stream2_device; returns its index or -1.
static int stream2_os_create_device(const char* name, const char* thread_name, mempool_base* pool, int max_requests)
{
    stream2_os_device* device = (stream2_os_device*)pool->alloc(sizeof(stream2_os_device), 0);
    if (device != nullptr)
    {
        new (device) stream2_os_device(name, pool, max_requests);
    }
    device->set_thread_handle(thread_spawn(thread_dummy, device, 0, THREAD_PRIOR_HIGH, thread_name, 0x10000));
    return Stream2_device.add(device);
}

// 0x004B0680 (:604)
void stream2_os_system_init(mempool_base* pool, int max_requests)
{
    if (Stream2_os_init)
    {
        return;
    }
    Stream2_os_init = true;
    if (Disc_build)
    {
        Stream2_device_dvd = stream2_os_create_device("Disc", "dvd thread", pool, max_requests);
    }
    stream2_os_init_memory_thread(pool, max_requests);
    Stream2_device_hdd = stream2_os_create_device("HDD", "stream2 hdd thread", pool, max_requests);
}

// 0x004AE240 (:663)
void stream2_os_system_shutdown()
{
    if (!Stream2_os_init)
    {
        return;
    }
    Stream2_os_init = false;
    for (int i = 0; i < Stream2_device.size(); i++)
    {
        Stream2_device[i]->shutdown_everything();
    }
}

// 0x004AFC50 (:678)
bool stream2_os_init_memory_thread(mempool_base* pool, int max_requests)
{
    if ((Use_packfiles || Game_preload) && Stream2_device_memory < 0)
    {
        Stream2_device_memory = stream2_os_create_device("Memory", "mem thread", pool, max_requests);
    }
    return true;
}

// 0x004AE2A0 (:707)
unsigned int stream2_os_read(stream2_os_file& request)
{
    if (request.filename[0] == '\0' && request.preopened == nullptr)
    {
        return 0xFFFFFFFF;
    }
    if (request.chain_read)
    {
        if (request.chain_dest == nullptr)
        {
            return 0xFFFFFFFF;
        }
    }
    else if (request.dest == (unsigned char*)0xD || request.dest == nullptr)
    {
        return 0xFFFFFFFF;
    }

    cf_found_info found_info = {};
    if (request.preopened == nullptr)
    {
        cf_find_file(&found_info, request.filename, false);
    }
    else
    {
        cf_size(request.preopened);
        found_info.m_media_type = cf_media_type(request.preopened);
    }
    stream2_os_device* device;
    if (found_info.m_media_type == CF_IO_MEDIA_DVD)
    {
        device = Stream2_device[Stream2_device_dvd];
    }
    else if (found_info.m_media_type == CF_IO_MEDIA_MEMORY)
    {
        device = Stream2_device[Stream2_device_memory];
    }
    else
    {
        device = Stream2_device[Stream2_device_hdd];
    }
    return device->read(request);
}

// 0x004AB3D0 (:764)
void stream2_os_cancel(unsigned int req)
{
    if (req != 0xFFFFFFFF)
    {
        ((stream2_os_request*)req)->device->cancel(req);
    }
}

// 0x004AB3F0 (:773)
stream2_os_req_status stream2_os_poll(unsigned int req)
{
    if (req != 0xFFFFFFFF)
    {
        switch (((stream2_os_request*)req)->status)
        {
        case SREQ_PENDING:
        case SREQ_ACTIVE:
        case SREQ_CANCEL_PENDING:
            return SOS_BUSY;
        case SREQ_CANCEL:
        case SREQ_DONE:
            return SOS_DONE_SUCCESS;
        case SREQ_ERROR_FOPEN:
        case SREQ_ERROR_UNKNOWN:
            return SOS_DONE_ERROR;
        default:
            break;
        }
    }
    return SOS_INVALID;
}

// 0x004AB440 (:803)
void stream2_os_release_handle(unsigned int req)
{
    if (req != 0xFFFFFFFF)
    {
        ((stream2_os_request*)req)->device->release_handle(req);
    }
}

// 0x004AE3B0 (:812)
void stream2_os_pause_disc_processing()
{
    if (Stream2_device_dvd >= 0 && Stream2_device[Stream2_device_dvd] != nullptr)
    {
        Stream2_device[Stream2_device_dvd]->pause_processing();
    }
}

// 0x004AE3F0 (:822)
void stream2_os_unpause_disc_processing()
{
    if (Stream2_device_dvd >= 0 && Stream2_device[Stream2_device_dvd] != nullptr)
    {
        Stream2_device[Stream2_device_dvd]->unpause_processing();
    }
}

// 0x004AE430 (:833)
bool stream2_os_disc_is_paused()
{
    if (Stream2_device_dvd >= 0 && Stream2_device[Stream2_device_dvd] != nullptr)
    {
        return Stream2_device[Stream2_device_dvd]->is_paused();
    }
    return false;
}
