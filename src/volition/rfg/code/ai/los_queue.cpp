// Original: volition/rfg/code/ai/los_queue.cpp
//
// The line-of-sight queue (stub_impl.md S9): pairs of humans wait here for a raycast, a few a frame, and the result
// is cached in a record the perception reads back.

#include "volition/rfg/code/ai/los_queue.h"

#include "volition/rfg/code/ai/ai_system.h"
#include "volition/rfg/code/havok/havok_raycast.h"
#include "volition/rfg/code/level/world.h"
#include "volition/rfg/code/objects/humans/human.h"
#include "volition/rfg/code/objects/humans/human_combat_turret.h"
#include "volition/rfg/code/objects/humans/npc.h"
#include "volition/rfg/code/objects/object_type.h"
#include "volition/vlib/code/math/vector3.h"
#include "volition/vlib/code/memory/object_pool.h"

static object_pool<los_request, 400, NUM_LOS_REQUEST_PRIORITIES> Los_request_pool(&los_request::init);
static object_pool<los_record, 600, 1> Los_record_pool;

static const int LOS_REQUEST_UPGRADE_MS = 2000; // a low priority request becomes high after this long
static const int LOS_RECORD_CHECK_MS = 500;
static const int LOS_RECORD_MAX_AGE_MS = 3000;
static const float LOS_RECORD_MAX_MOVE_SQ = 4.0f; // 2 m: a record no longer holds if either human moved further
static const int LOS_MIN_REQUESTS_PER_FRAME = 3;
static const int LOS_MAX_REQUESTS_PER_FRAME = 8;
static const unsigned int LOS_RAYCAST_FLAGS = 0xE;

// 0x005EBA00 (:57)
void los_request::init()
{
    flags.human1_interested = 0;
    flags.human2_interested = 0;
}

// 0x005EBA10 (:61)
void los_request::set_priority(los_request_priority pri)
{
    priority = static_cast<unsigned char>(pri);
    priority_upgrade_timer.set(LOS_REQUEST_UPGRADE_MS);
}

// 0x005EBA30 (:65)
los_request_priority los_request::get_priority() const
{
    return static_cast<los_request_priority>(priority);
}

// 0x005F9C50
los_request::los_request()
{
}

// 0x005EBA40 (:96)
human_stances los_record::get_stance(int which) const
{
    return static_cast<human_stances>(stances[which]);
}

// 0x005EBA50 (:100)
void los_record::set_stance(int which, human_stances stance)
{
    stances[which] = static_cast<unsigned char>(stance);
}

// 0x005F9D00
los_record::los_record_human_info::los_record_human_info()
{
}

// 0x0064B0E0
los_record::los_record()
{
}

// 0x0063A160 (:449)
los_raycast_callback_data::obj_data::obj_data(const object* objp_)
    : objp(objp_), vp(nullptr), tp(nullptr)
{
    if (objp_ == nullptr || objp_->get_obj_type() != OT_HUMAN)
    {
        return;
    }
    const human* hp = static_cast<const human*>(objp_);
    if (hp->vehicle_handle != 0xFFFFFFFF)
    {
        const object* vehicle_obj = World.object_from_handle(hp->vehicle_handle);
        if (vehicle_obj != nullptr && vehicle_obj->get_obj_type() == OT_VEHICLE)
        {
            vp = reinterpret_cast<const vehicle*>(vehicle_obj);
        }
    }
    tp = human_combat_get_manned_turret(hp);
}

// 0x0063A1D0 (:462)
los_raycast_callback_data::los_raycast_callback_data(const object* src_ptr, const object* target_ptr)
    : src(src_ptr), target(target_ptr)
{
}

// 0x005EBA80 (:480)
bool los_raycast_filter_callback(object* hit_objp, void* cb_data_ptr)
{
    if (hit_objp == nullptr)
    {
        return false;
    }
    switch (hit_objp->get_obj_type())
    {
    case OT_UNDEFINED:
    case OT_HUMAN:
    case OT_ITEM:
    case OT_TURRET:
        return false;
    default:
        break;
    }
    // Only the target and the two vehicles are let through: src.objp is a human, filtered above, and the turrets in
    // the callback data are unused (original).
    const los_raycast_callback_data* data = static_cast<const los_raycast_callback_data*>(cb_data_ptr);
    const object* hit = hit_objp;
    return hit != data->target.objp
        && hit != reinterpret_cast<const object*>(data->src.vp)
        && hit != reinterpret_cast<const object*>(data->target.vp);
}

// 0x0063A060
static bool should_cull_record(const los_record* record)
{
    if (record->created_time.time_since() > LOS_RECORD_MAX_AGE_MS)
    {
        return true;
    }
    if (record->human_info[0].handle == 0xFFFFFFFF || record->human_info[1].handle == 0xFFFFFFFF)
    {
        return true;
    }
    object* obj1 = World.object_from_handle(record->human_info[0].handle);
    if (obj1 == nullptr || obj1->get_obj_type() != OT_HUMAN || obj1->is_killed())
    {
        return true;
    }
    object* obj2 = World.object_from_handle(record->human_info[1].handle);
    if (obj2 == nullptr || obj2->get_obj_type() != OT_HUMAN || obj2->is_killed())
    {
        return true;
    }
    const human* hp1 = static_cast<const human*>(obj1);
    const human* hp2 = static_cast<const human*>(obj2);
    if (hp1->cur_state == HS_COVER || hp2->cur_state == HS_COVER)
    {
        return true;
    }
    if (hp1->cur_stance != record->get_stance(0) || hp2->cur_stance != record->get_stance(1))
    {
        return true;
    }
    return vec_dist_squared(record->human_info[0].pos, obj1->pos) > LOS_RECORD_MAX_MOVE_SQ
        || vec_dist_squared(record->human_info[1].pos, obj2->pos) > LOS_RECORD_MAX_MOVE_SQ;
}

// 0x0067FC90
static void cull_old_los_records()
{
    Los_record_pool.thread_access.enter();
    los_record* record = Los_record_pool.get_first(0);
    while (record != nullptr)
    {
        los_record* next = Los_record_pool.get_next(record);
        if (record->check_time.elapsed())
        {
            if (should_cull_record(record))
            {
                Los_record_pool.free(record);
            }
            else
            {
                record->check_time.set(LOS_RECORD_CHECK_MS);
            }
        }
        record = next;
    }
    Los_record_pool.thread_access.leave();
}

// 0x0067FD60
static void increment_request_priorities()
{
    Los_request_pool.thread_access.enter();
    los_request* request = Los_request_pool.get_first(LOS_REQUEST_PRIORITY_LOW);
    while (request != nullptr)
    {
        los_request* next = Los_request_pool.get_next(request);
        if (request->priority_upgrade_timer.valid() && request->priority_upgrade_timer.elapsed())
        {
            request->set_priority(LOS_REQUEST_PRIORITY_HIGH);
            Los_request_pool.move(request, LOS_REQUEST_PRIORITY_HIGH);
        }
        request = next;
    }
    Los_request_pool.thread_access.leave();
}

// 0x00662EE0: records and requests order the pair by handle.
static los_record* find_los_record(unsigned int human1_uid, unsigned int human2_uid)
{
    const unsigned int low = human2_uid < human1_uid ? human2_uid : human1_uid;
    const unsigned int high = human2_uid < human1_uid ? human1_uid : human2_uid;
    los_record* found = nullptr;
    Los_record_pool.thread_access.enter();
    for (los_record* record = Los_record_pool.get_first(0); record != nullptr; record = Los_record_pool.get_next(record))
    {
        if (record->human_info[0].handle == low && record->human_info[1].handle == high)
        {
            found = record;
            break;
        }
    }
    Los_record_pool.thread_access.leave();
    return found;
}

// 0x00662E30
static los_request* find_los_request(unsigned int human1_uid, unsigned int human2_uid)
{
    const unsigned int low = human2_uid < human1_uid ? human2_uid : human1_uid;
    const unsigned int high = human2_uid < human1_uid ? human1_uid : human2_uid;
    los_request* found = nullptr;
    for (int pri = 0; pri < NUM_LOS_REQUEST_PRIORITIES && found == nullptr; pri++)
    {
        Los_request_pool.thread_access.enter();
        for (los_request* request = Los_request_pool.get_first(pri); request != nullptr; request = Los_request_pool.get_next(request))
        {
            if (request->human1_uid == low && request->human2_uid == high)
            {
                found = request;
                break;
            }
        }
        Los_request_pool.thread_access.leave();
    }
    return found;
}

// 0x00662F90: the first pair with no record yet, else the pair with the oldest record.
static los_request* get_best_request_for_priority(los_request_priority pri)
{
    los_request* best = nullptr;
    int best_age = -1;
    Los_request_pool.thread_access.enter();
    for (los_request* request = Los_request_pool.get_first(pri); request != nullptr; request = Los_request_pool.get_next(request))
    {
        const los_record* record = find_los_record(request->human1_uid, request->human2_uid);
        if (record == nullptr)
        {
            best = request;
            break;
        }
        const int age = record->created_time.time_since();
        if (age > best_age)
        {
            best = request;
            best_age = age;
        }
    }
    Los_request_pool.thread_access.leave();
    return best;
}

static human* human_from_handle(unsigned int handle)
{
    if (handle == 0xFFFFFFFF)
    {
        return nullptr;
    }
    object* objp = World.object_from_handle(handle);
    if (objp == nullptr || objp->get_obj_type() != OT_HUMAN)
    {
        return nullptr;
    }
    return static_cast<human*>(objp);
}

// 0x006BD3F0
static bool process_los_request(los_request* request)
{
    const unsigned int handle1 = request->human1_uid;
    const unsigned int handle2 = request->human2_uid;
    // Original: freed first, and flags are read from the freed entry below. The pool leaves its objects' memory alone
    // and nothing allocates from it in between.
    Los_request_pool.free(request);

    human* hp1 = human_from_handle(handle1);
    human* hp2 = human_from_handle(handle2);
    if (hp1 == nullptr || hp2 == nullptr)
    {
        return false;
    }
    los_record* record = find_los_record(handle1, handle2);
    if (record == nullptr)
    {
        record = Los_record_pool.alloc(0);
        if (record == nullptr)
        {
            return false;
        }
    }

    vector eye1;
    vector eye2;
    human_get_eye_pos_orient(hp1, eye1, nullptr);
    human_get_eye_pos_orient(hp2, eye2, nullptr);
    if (!eye1.is_valid() || !eye2.is_valid())
    {
        return false;
    }

    los_raycast_callback_data cb_data(hp1, hp2);
    raycast_input input;
    input.start = eye1;
    input.end = eye2;
    input.radius = 0.0f;
    input.filter_params = &cb_data;
    input.filter_callback = los_raycast_filter_callback;
    input.m_filter_info = COLLIDE_RAY_DEFAULT;
    input.m_hit_collector_type = raycast_input::COLLECT_CLOSEST_HIT;
    input.m_flags = LOS_RAYCAST_FLAGS;
    raycast_output output;
    const bool has_los = !havok_raycast(input, output);

    record->human_info[0].handle = handle1;
    record->human_info[0].pos = hp1->pos;
    record->stances[0] = static_cast<unsigned char>(hp1->cur_stance);
    record->human_info[1].handle = handle2;
    record->human_info[1].pos = hp2->pos;
    record->stances[1] = static_cast<unsigned char>(hp2->cur_stance);
    record->created_time.set(0);
    record->check_time.set(LOS_RECORD_CHECK_MS);
    record->flags.positive = has_los ? 1 : 0;

    if (!hp1->is_killed() && !hp2->is_killed())
    {
        if (hp1->get_sub_type() == OT_SUB_HUMAN_NPC && request->flags.human1_interested != 0)
        {
            static_cast<npc*>(hp1)->get_ai()->perception.record_los_result(hp2, has_los);
        }
        if (hp2->get_sub_type() == OT_SUB_HUMAN_NPC && request->flags.human2_interested != 0)
        {
            static_cast<npc*>(hp2)->get_ai()->perception.record_los_result(hp1, has_los);
        }
    }
    return has_los;
}

// 0x006BD6A0
static void process_los_requests()
{
    // An eighth of the queue, between 3 and 8 raycasts a frame.
    int remaining = (Los_request_pool.get_capacity() - Los_request_pool.get_free()) >> 3;
    if (remaining > LOS_MAX_REQUESTS_PER_FRAME)
    {
        remaining = LOS_MAX_REQUESTS_PER_FRAME;
    }
    else if (remaining < LOS_MIN_REQUESTS_PER_FRAME)
    {
        remaining = LOS_MIN_REQUESTS_PER_FRAME;
    }
    int pri = LOS_REQUEST_PRIORITY_HIGH;
    while (remaining > 0 && pri < NUM_LOS_REQUEST_PRIORITIES)
    {
        los_request* request = get_best_request_for_priority(static_cast<los_request_priority>(pri));
        if (request == nullptr)
        {
            pri++;
        }
        else
        {
            process_los_request(request);
            remaining--;
        }
    }
}

// 0x006A4D80 (:519)
void los_queue_level_init_post()
{
    Los_request_pool.clear();
    Los_record_pool.clear();
}

// 0x006BD700 (:538)
void los_queue_do_frame()
{
    cull_old_los_records();
    increment_request_priorities();
    process_los_requests();
}

// 0x0067FE20 (:558)
bool los_queue_remove_request(const human* hp1, const human* hp2)
{
    const unsigned int human1_uid = hp1->get_handle();
    los_request* request = find_los_request(human1_uid, hp2->get_handle());
    if (request == nullptr)
    {
        return false;
    }
    if (human1_uid == request->human1_uid)
    {
        request->flags.human1_interested = 0;
    }
    else
    {
        request->flags.human2_interested = 0;
    }
    if (request->flags.human1_interested == 0 && request->flags.human2_interested == 0)
    {
        Los_request_pool.free(request);
    }
    return true;
}

// 0x0067FE70 (:593)
bool los_queue_add_request(const human* hp1, const human* hp2, los_request_priority priority)
{
    const unsigned int human1_uid = hp1->get_handle();
    const unsigned int human2_uid = hp2->get_handle();
    los_request* request = find_los_request(human1_uid, human2_uid);
    if (request == nullptr)
    {
        request = Los_request_pool.alloc(priority);
        if (request == nullptr)
        {
            return false;
        }
        if (human2_uid < human1_uid)
        {
            request->flags.human2_interested = 1;
            request->human1_uid = human2_uid;
            request->human2_uid = human1_uid;
        }
        else
        {
            request->flags.human1_interested = 1;
            request->human1_uid = human1_uid;
            request->human2_uid = human2_uid;
        }
        request->set_priority(priority);
        return true;
    }
    if (priority < request->get_priority())
    {
        request->set_priority(priority);
        Los_request_pool.move(request, priority);
    }
    if (human1_uid == request->human1_uid)
    {
        request->flags.human1_interested = 1;
    }
    else
    {
        request->flags.human2_interested = 1;
    }
    return true;
}

// 0x00663060 (:657)
los_record_check_result los_queue_check_records(unsigned int human1_uid, unsigned int human2_uid)
{
    const los_record* record = find_los_record(human1_uid, human2_uid);
    if (record != nullptr)
    {
        return record->flags.positive != 0 ? LOS_RECORD_POSITIVE : LOS_RECORD_NEGATIVE;
    }
    return find_los_request(human1_uid, human2_uid) != nullptr ? LOS_RECORD_REQUESTED : LOS_RECORD_NOT_FOUND;
}

// 0x006630A0 (:689)
bool los_record_get_pos_from_record(unsigned int human1_uid, unsigned int human2_uid, vector& human1_pos)
{
    human1_pos = ZERO_VECTOR;
    const los_record* record = find_los_record(human1_uid, human2_uid);
    if (record == nullptr)
    {
        return false;
    }
    human1_pos = record->human_info[0].handle == human1_uid ? record->human_info[0].pos : record->human_info[1].pos;
    return true;
}
