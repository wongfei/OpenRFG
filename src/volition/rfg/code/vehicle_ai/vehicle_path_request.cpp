// Original: volition/rfg/code/vehicle_ai/vehicle_path_request.cpp
//
// Vehicles queue up for a nav mesh path, so only the vehicle at the head of the queue pathfinds in a frame. An entry that
// was not serviced by the end of the frame goes to the back of the queue.

#include "volition/rfg/code/vehicle_ai/vehicle_path_request.h"

#include <cstring>

#include "volition/rfg/code/level/world.h"
#include "volition/rfg/code/vehicle/vehicle_class.h"
#include "volition/vlib/code/time/timestamp.h"

namespace
{
    // size 0xC
    struct navmesh_pathfind_request
    {
        unsigned int vehicle_handle;
        timestamp request_time;
        struct
        {
            unsigned int serviced : 1;
        } flags;
    };
}

static const int MAX_NAVMESH_PATHFIND_REQUESTS = 24;
static const int REQUEST_TIMEOUT_MS = 2000; // a request older than this is dropped

static int Num_navmesh_pathfind_requests; // 0x0387512C
static navmesh_pathfind_request Navmesh_pathfind_requests[MAX_NAVMESH_PATHFIND_REQUESTS]; // 0x038781B0

// 0x00BC7520 (:53): LTCG, vp in EDI.
static void vehicle_navmesh_path_add_request(const vehicle* vp, int time_offset)
{
    const unsigned int handle = vp->get_handle();
    for (int i = 0; i < Num_navmesh_pathfind_requests; i++)
    {
        if (Navmesh_pathfind_requests[i].vehicle_handle == handle)
        {
            Navmesh_pathfind_requests[i].request_time.set(time_offset);
            return;
        }
    }
    if (Num_navmesh_pathfind_requests < MAX_NAVMESH_PATHFIND_REQUESTS)
    {
        navmesh_pathfind_request& req = Navmesh_pathfind_requests[Num_navmesh_pathfind_requests];
        req.flags.serviced = 0;
        req.vehicle_handle = handle;
        Num_navmesh_pathfind_requests++;
        // Original bug kept: a new entry takes the time 0 and not time_offset, so a requeued request starts its timeout
        // over.
        req.request_time.set(0);
    }
}

// 0x00BC7580 (:82)
void vehicle_navmesh_path_requests_level_init_pre()
{
    memset(Navmesh_pathfind_requests, 0, sizeof(Navmesh_pathfind_requests));
    Num_navmesh_pathfind_requests = 0;
}

// 0x00BC75A0 (:94)
bool vehicle_navmesh_path_can_pathfind(unsigned int vehicle_handle)
{
    return Navmesh_pathfind_requests[0].vehicle_handle == vehicle_handle;
}

// Whether the request's vehicle exists, is alive and still wants a path.
static bool request_is_live(navmesh_pathfind_request& req)
{
    if (req.vehicle_handle == 0xFFFFFFFF)
    {
        return false;
    }
    vehicle* vp = World.object_from_handle<vehicle>(req.vehicle_handle);
    return vp != NULL && !vp->obj_flags.destroyed && !vehicle_is_corpse(vp) && req.request_time.time_since() < REQUEST_TIMEOUT_MS;
}

// 0x00BCA860 (:102)
void vehicle_navmesh_path_requests_process()
{
    if (Num_navmesh_pathfind_requests == 0)
    {
        return;
    }
    navmesh_pathfind_request& head = Navmesh_pathfind_requests[0];
    if (!head.flags.serviced && request_is_live(head))
    {
        vehicle* vp = World.object_from_handle<vehicle>(head.vehicle_handle);
        const int elapsed = head.request_time.time_since();
        head.vehicle_handle = 0xFFFFFFFF;
        vehicle_navmesh_path_add_request(vp, -elapsed);
    }
    head.vehicle_handle = 0xFFFFFFFF;
    for (int i = 0; i < Num_navmesh_pathfind_requests; i++)
    {
        if (!request_is_live(Navmesh_pathfind_requests[i]))
        {
            memmove(&Navmesh_pathfind_requests[i], &Navmesh_pathfind_requests[i + 1], (Num_navmesh_pathfind_requests - i - 1) * sizeof(navmesh_pathfind_request));
            i--;
            Num_navmesh_pathfind_requests--;
        }
    }
}

// 0x00BC75B0 (:150)
void vehicle_navmesh_path_add_request(const vehicle* vp)
{
    vehicle_navmesh_path_add_request(vp, 0);
}

// 0x00BC75D0 (:159)
void vehicle_navmesh_path_service_request(unsigned int vehicle_handle)
{
    // The handle is not used: only the head of the queue can be pathfinding.
    Navmesh_pathfind_requests[0].flags.serviced = 1;
}
