// Original: volition/rfg/code/havok/havok_mt_query.cpp
//
// Multithreaded pair linear casts (player_minimal.md P1), from rfg.exe: a collidable is swept to a destination
// against a strided array of collidables on a Havok scheduler job; havok_mt_query_wait waits for all jobs queued
// since the last wait. The character proxies cast through here (hk_custom_character_proxy::mt_integrate_linear_cast).
// Part of rfg_havok_unity.cpp (Havok_mt_query_world is havok.cpp's).

#include "volition/rfg/code/havok/havok_mt_query.h"

#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/math/vector/hkvector4f.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/agent/collidable/hkpcollidable.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/agent/hkpprocesscollisioninput.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/agent/query/hkplinearcastcollisioninput.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/dispatch/hkpcollisiondispatcher.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/shape/hkpshape.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/dynamics/world/hkpworld.h"
#include "volition/vlib/code/os/scheduler.h"
#include "volition/vlib/code/os/thread.h"
#include "volition/vlib/code/vlib_core/vlib_assert.h"

class hkpCdPointCollector;
class hkpCollidable;

// ---- TU-private types (rfg.pdb; no header names them)
// One linear cast job: m_collidable_to_cast swept to m_destination_position against a strided array of collidables
// (the character proxies' casts, run as scheduler jobs).
// size 0x40
struct pair_linearcast_job_parameters
{
    pair_linearcast_job_parameters(); // 0x00811B10

    const hkpCollidable* m_collidable_to_cast; // 0x00
    hkVector4f m_destination_position; // 0x10
    const hkpCollidable* const* m_collidables_to_cast_against; // 0x20
    unsigned int m_num_collidables_to_cast_against; // 0x24
    unsigned int m_collidables_to_cast_against_stride; // 0x28
    float m_tolerance; // 0x2C
    float m_max_extra_penetration; // 0x30
    hkpCdPointCollector* m_cast_collector; // 0x34
    hkpCdPointCollector* m_start_collector; // 0x38
};

// ---- file-static globals
static event_trigger* Havok_mt_query_event_trigger; // 0x02B822B0
static unsigned int Havok_mt_query_pair_linearcast_num_job_parameters; // 0x02B822B4
// hkpWorld* Havok_mt_query_world (0x02B822B8) is used here too; placed in havok.cpp.
static pair_linearcast_job_parameters Havok_mt_query_pair_linearcast_job_parameters[100]; // 0x02B82320

// 0x00811B10
pair_linearcast_job_parameters::pair_linearcast_job_parameters()
{
}

// 0x00811B70 (:234)
void havok_mt_query_initialize(hkpWorld* p_world)
{
    Havok_mt_query_world = p_world;
}

// 0x00811B20 (:113): the next free job slot (100 per wait)
static pair_linearcast_job_parameters* havok_mt_query_allocate_linearcast_job_parameters()
{
    if (Havok_mt_query_pair_linearcast_num_job_parameters > 99)
    {
        keen::debug::nativePrintArgs("Too many linearcast jobs!");
        return nullptr;
    }
    return &Havok_mt_query_pair_linearcast_job_parameters[Havok_mt_query_pair_linearcast_num_job_parameters++];
}

// 0x00811B50 (:212): the first query since the last wait gets a new trigger and starts the slots over
static void havok_mt_query_allocate_event_trigger()
{
    if (Havok_mt_query_event_trigger == nullptr)
    {
        Havok_mt_query_event_trigger = scheduler_get_event_trigger();
        Havok_mt_query_pair_linearcast_num_job_parameters = 0;
    }
}

// 0x0082B0E0 (:134): the dispatcher's linear cast against every collidable, with the world's collision input.
// LTCG: query_job_params in ESI.
static void havok_mt_query_job_pair_linearcast(pair_linearcast_job_parameters* query_job_params)
{
    hkpLinearCastCollisionInput input;
    input.set(*Havok_mt_query_world->m_collisionInput);
    hkVector4f path;
    path.setSub(query_job_params->m_destination_position, query_job_params->m_collidable_to_cast->getTransform().getTranslation());
    input.setPathAndTolerance(path, query_job_params->m_tolerance);
    input.m_maxExtraPenetration = query_job_params->m_max_extra_penetration;

    const hkpCollidable* toCast = query_job_params->m_collidable_to_cast;
    const hkpCollisionDispatcher* dispatcher = input.m_dispatcher.val();
    const unsigned char* against = (const unsigned char*)query_job_params->m_collidables_to_cast_against;
    for (unsigned int i = query_job_params->m_num_collidables_to_cast_against; i != 0; i--)
    {
        const hkpCollidable* other = *(const hkpCollidable* const*)against;
        dispatcher->getLinearCastFunc(static_cast<hkcdShapeType::ShapeTypeEnum>(toCast->m_shape->m_type.m_storage), static_cast<hkcdShapeType::ShapeTypeEnum>(other->m_shape->m_type.m_storage))(
            *toCast, *other, input, *query_job_params->m_cast_collector, query_job_params->m_start_collector);
        against += query_job_params->m_collidables_to_cast_against_stride;
    }
}

// 0x0082B260 (:194): arg1 is the query type (0: pair linear cast), arg2 the parameters
static void havok_mt_query_job(cpu_job* p_job, cpu_job_arg arg1, cpu_job_arg arg2)
{
    if (arg1.int_val == 0)
    {
        havok_mt_query_job_pair_linearcast((pair_linearcast_job_parameters*)arg2.pointer_val);
    }
}

// 0x0082B280 (:260): queues the cast as a Havok job on the query trigger (inlines the two allocators above)
void havok_mt_query_pair_linearcast(const hkpCollidable* p_collidable_to_cast, const hkVector4f& destination_position, const hkpCollidable* const* p_collidables_to_cast_against, unsigned int num_collidables_to_cast_against, unsigned int collidables_to_cast_against_stride, float tolerance, float max_extra_penetration, hkpCdPointCollector& p_cast_collector, hkpCdPointCollector* p_start_collector)
{
    havok_mt_query_allocate_event_trigger();
    pair_linearcast_job_parameters* params = havok_mt_query_allocate_linearcast_job_parameters();
    if (params == nullptr)
    {
        return;
    }
    params->m_collidable_to_cast = p_collidable_to_cast;
    params->m_destination_position = destination_position;
    params->m_collidables_to_cast_against = p_collidables_to_cast_against;
    params->m_num_collidables_to_cast_against = num_collidables_to_cast_against;
    params->m_tolerance = tolerance;
    params->m_collidables_to_cast_against_stride = collidables_to_cast_against_stride;
    params->m_max_extra_penetration = max_extra_penetration;
    params->m_cast_collector = &p_cast_collector;
    params->m_start_collector = p_start_collector;

    cpu_job job;
    job.trigger = nullptr;
    job.flags = 0;
    job.scratch_buffer = nullptr;
    job.scratch_buffer_size = 0;
    job.thread_id = get_current_thread_id();
    job.job_func = havok_mt_query_job;
    job.arg1.int_val = 0;
    job.type = ccjt_havok;
    job.arg2.pointer_val = params;
    scheduler_add_job(nullptr, &job, Havok_mt_query_event_trigger);
}

// 0x00811B80 (:298): runs or waits for the queued casts
void havok_mt_query_wait()
{
    if (Havok_mt_query_event_trigger != nullptr)
    {
        scheduler_relinquish_cpu(Havok_mt_query_event_trigger, ccjt_any);
        Havok_mt_query_event_trigger = nullptr;
    }
}
