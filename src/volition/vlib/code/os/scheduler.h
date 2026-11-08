// Original: volition/vlib/code/os/scheduler.h
// Implementation: volition/vlib/code/os/scheduler.cpp
//
// vlib job scheduler (decompiled behaviour):
// - Jobs come from a fixed pool (Cpu_jobs[1536]) and wait in per-type queues (Pending_sim/d3d/d3d_cpu/d3d_any/
//   havok_jobs). Each job gets an ordinal that restarts every frame (Framecount).
// - scheduler_add_job(input, job, output): copies the job, attaches it as an input to `output` (the output trigger
//   counts it down), then either queues it and wakes one cpu (no input trigger) or hangs it on `input`, which
//   queues it when it fires.
// - Triggers come from pools: event_trigger (a sync_event the caller waits on) and scheduling_trigger (releases up
//   to 16 dependent jobs).
// - cpu_controller is a worker thread per hardware thread (Cpu_controllers[16]); scheduler_init configures 8 from
//   the static table Normal_cpu_setup (job type, explicit/auto activation, Havok jobs) and starts them at
//   THREAD_PRIOR_LOW (explicit) or THREAD_PRIOR_LOWEST (auto).
// - scheduler_relinquish_cpu_until_event lets the calling thread's own controller run jobs of `acceptable_type`
//   while it waits on the event.
// - Thread roles are tracked by id: scheduler_register_sim_thread (called by keen::rfg::Application::updateSimulation),
//   _d3d_thread, _culling_thread. scheduler_is_sim_thread() is true before registration.
#pragma once

#include "volition/vlib/code/memory/vmemory.h"
#include "volition/vlib/code/os/cs_wrapper.h"
#include "volition/vlib/code/os/sync_event.h"
#include "volition/vlib/code/os/thread.h"

class mempool_base;
class page_pool;
class virtual_mempool;
class trigger_base;

enum cpu_thread
{
    CPU_THREAD_ANY = -1,
    CPU_THREAD_0 = 0,
    CPU_THREAD_1 = 1,
    CPU_THREAD_2 = 2,
    CPU_THREAD_3 = 3,
    CPU_THREAD_4 = 4,
    CPU_THREAD_5 = 5,
    CPU_THREAD_6 = 6,
    CPU_THREAD_7 = 7,
};

enum cpu_job_type
{
    ccjt_any = 0,
    ccjt_d3d = 1,
    ccjt_d3d_cpu = 2,
    ccjt_sim = 3,
    ccjt_havok = 4,
    ccjt_d3d_any = 5,
    ccjt_sim_main = 6,
    ccjt_num_job_types = 7,
};

enum cpu_state
{
    ccs_uninitialized = 0,
    ccs_inactive = 1,
    ccs_idle = 2,
    ccs_work_pending = 3,
    ccs_working = 4,
};

// explicit: woken only by scheduler calls; auto: woken when work of its type is queued (inferred from the names).
enum cpu_activation_type
{
    ccat_explicit = 0,
    ccat_auto = 1,
};

// size 0x4
union cpu_job_arg
{
    unsigned int int_val; // 0x00
    void* pointer_val; // 0x00
};

// size 0x28
struct cpu_job
{
    cpu_job(); // 0x005ADF80 (.h:243)
    cpu_job& operator=(const cpu_job& rhs); // 0x0058B800 (.h:248)

    void (*job_func)(cpu_job*, cpu_job_arg, cpu_job_arg); // 0x00
    trigger_base* trigger; // 0x04, output trigger
    cpu_job_arg arg1; // 0x08
    cpu_job_arg arg2; // 0x0C
    cpu_job_type type; // 0x10
    int ordinal; // 0x14
    unsigned int thread_id; // 0x18
    unsigned int flags; // 0x1C
    unsigned char* scratch_buffer; // 0x20
    int scratch_buffer_size; // 0x24
};

// size 0xC8
class cpu_controller
{
public:
    cpu_controller(); // 0x005ADFB0 (.h:285)
    ~cpu_controller(); // 0x005AE070 (.h:286)
    void init(cpu_thread thread_num, thread_prior priority, void (*_fp)(cpu_controller*)); // 0x005BDAD0 (:924)
    void shutdown(); // 0x005BDC00 (:998)
    void set_state(cpu_state _state); // 0x0058B840 (.h:292)
    cpu_state get_state() const; // 0x0058B870 (.h:299)
    void deactivate(); // 0x0058B9C0 (:747)
    void wait_until_inactive(); // 0x0058BA00 (:762)
    bool activate_if_idle(); // 0x005AE490 (:715)
    void wait_for_event(); // 0x005AE0A0 (.h:304)
    void call_function(); // 0x0058B880 (.h:305)
    cpu_job* get_current_job(); // 0x0058B890 (.h:306)
    static void default_sim_cpu_function(cpu_controller* this_cpu); // 0x005BD990 (:842)
    cpu_job* get_next_job(); // 0x005BD880 (:785)
private:
    void activate(); // 0x005AE0B0 (.h:333)
public:
    void init_cpu_virtual_mempool(); // 0x005AE810 (:1624)
    void release_cpu_virtual_mempool(); // 0x005AE840 (:1633)
    virtual_mempool& get_cpu_virtual_mempool() { return *cpu_controller_virtual_mempool; } // 0x005D0BF0 (.h:347)
    void register_scratch_mempool(); // 0x0058B8A0 (.h:352)
    void override_scratch_mempool_for_job(cpu_job* job); // 0x0058BA10 (:1018)
    void restore_scratch_mempool(); // 0x0058BA40 (:1030)

    volatile bool thread_exit_pending; // 0x00
    volatile bool reached_initial_wait; // 0x01
    cpu_thread cpu_thread_num; // 0x04
    unsigned int thread_id; // 0x08
    cs_wrapper state_cs; // 0x0C
    volatile bool deactivate_pending; // 0x48
private:
    cpu_job* current_job; // 0x4C
    volatile cpu_state state; // 0x50
    void (*fp)(cpu_controller*); // 0x54
public:
    cpu_job_type job_type; // 0x58
    cpu_activation_type activation_type; // 0x5C
    bool allow_havok_jobs; // 0x60
    bool havok_memory_initialized; // 0x61
private:
    sync_event event_handle; // 0x64
public:
    unsigned int thread_handle; // 0x68
private:
    virtual_mempool* cpu_controller_virtual_mempool; // 0x6C
    mempool scratch_mempool; // 0x70
    unsigned char* scratch_buffer; // 0xC0
    int scratch_buffer_size; // 0xC4
};

// A job dependency. attach_input() adds one to countdown per input job; each finishing input calls fire(), which
// decrements it and acts once it reaches zero while the trigger is active (decompiled).
// size 0x4C
class trigger_base
{
public:
    trigger_base(); // 0x0058BA70 (:1043)
    virtual ~trigger_base(); // vtbl+0x00 0x0058B780 (.h:185)
    virtual void attach_input() = 0; // vtbl+0x04
    virtual void attach_output(cpu_job*) = 0; // vtbl+0x08
    virtual void fire() = 0; // vtbl+0x0C
    virtual void activate() = 0; // vtbl+0x10

    // 0x00 vfptr
    volatile unsigned int countdown; // 0x04
    volatile bool active; // 0x08
    volatile bool in_use; // 0x09
    cs_wrapper active_cs; // 0x0C
    volatile int num_inputs; // 0x48
};

// On the last input: deactivates `cpu` (if set), sets fired and signals `event`.
// size 0x58
class event_trigger : public trigger_base
{
public:
    event_trigger(); // 0x005AE4D0 (:1046)
    virtual ~event_trigger(); // vtbl+0x00 0x005ADEF0 (.h:202)
    virtual void attach_input(); // vtbl+0x04 0x0058BB30 (:1112)
    virtual void attach_output(cpu_job* job); // vtbl+0x08 0x0058BB60 (:1130)
    virtual void fire(); // vtbl+0x0C 0x005AE530 (:1055)
    virtual void activate(); // vtbl+0x10 0x005AE570 (:1153)

    sync_event event; // 0x4C
    cpu_controller* cpu; // 0x50
    volatile bool fired; // 0x54
};

// On the last input: queues the dependent jobs and returns itself to the free pool.
// size 0x90
class scheduling_trigger : public trigger_base
{
public:
    scheduling_trigger(); // 0x0058BAB0 (:1049)
    virtual ~scheduling_trigger(); // vtbl+0x00 0x0058B7E0 (.h:219)
    virtual void attach_input(); // vtbl+0x04 0x0058BB50 (:1125)
    virtual void attach_output(cpu_job* job); // vtbl+0x08 0x0058BB70 (:1141)
    virtual void fire(); // vtbl+0x0C 0x005BDC60 (:1092)
    virtual void activate(); // vtbl+0x10 0x005BDD00 (:1202)

    cpu_job* dependent_jobs[16]; // 0x4C
    volatile unsigned int num_dependent_jobs; // 0x8C
};

// Placement inferred.
// size 0x4C
struct scheduler_status
{
    int num_pending_sim_jobs; // 0x00
    int num_pending_d3d_jobs; // 0x04
    int num_pending_d3d_cpu_jobs; // 0x08
    cpu_state cpu_states[16]; // 0x0C
};

extern int Scheduler_status; // 0x01519594
extern unsigned int Scheduler_sim_thread_id; // 0x01CE8778
extern unsigned int Scheduler_d3d_thread_id; // 0x01CE8774
extern unsigned int Scheduler_culling_thread_id; // 0x01CE877C

void unused_dcf_scheduler(); // 0x005BD420 (:200)
int scheduler_get_processor_number(); // 0x005AE2C0 (:281)
void scheduler_register_sim_thread(); // 0x005AE300 (:322)
void scheduler_register_d3d_thread(); // 0x005AE310 (:327)
void scheduler_register_culling_thread(); // 0x005AE320 (:332)
bool scheduler_is_sim_thread(); // 0x005AE330 (:337)
bool scheduler_is_render_thread(); // 0x005AE350 (:342)
bool scheduler_is_pre_threading(); // 0x0058B920 (:347)
void scheduler_assert_render_thread(); // 0x0058B930 (:352)
void scheduler_assert_sim_thread(); // 0x0058B940 (:357)
void scheduler_assert_pre_threading(); // 0x0058B950 (:362)
// Called from rfg_init_stage_1_done with (nullptr, &Common_page_pool, &RFG_scratch_pool).
void scheduler_init(void (*thread_creation_cb)(), page_pool* scheduler_page_pool, mempool_base* scheduler_scratch_pool); // 0x005C4540 (:367)
void scheduler_shutdown(); // 0x005C47F0 (:445)
void scheduler_add_cpu_thread(cpu_thread thread_num); // 0x0058B960 (:465)
void scheduler_remove_cpu_thread(cpu_thread thread_num); // 0x005AE370 (:480)
void scheduler_q_job(cpu_job* this_job); // 0x005BD790 (:565)
void scheduler_free_event_trigger(event_trigger* trig); // 0x005BD820 (:612)
void scheduler_free_scheduling_trigger(scheduling_trigger* trig); // 0x005BD850 (:624)
void job_deactivate_cpu_thread(cpu_job* de_job, unsigned int arg1, unsigned int arg2); // 0x005AE3C0 (:635)
void job_null(cpu_job* de_job, unsigned int arg1, unsigned int arg2); // 0x0058B9B0 (:644)
int cpu_thread_proc(void* lpParameter); // 0x005AE410 (:668)
event_trigger* scheduler_get_event_trigger(); // 0x005BDD80 (:1224)
scheduling_trigger* scheduler_get_scheduling_trigger(); // 0x005BDDC0 (:1244)
cpu_job* scheduler_add_job(trigger_base* input_trig, cpu_job* job, trigger_base* output_trig); // 0x005BDDF0 (:1259)
void scheduler_activate_trigger(trigger_base* trig); // 0x0058BB90 (:1286)
void scheduler_relinquish_cpu_until_event(sync_event& event, cpu_job_type acceptable_type); // 0x005AE5F0 (:1291)
void scheduler_relinquish_cpu(event_trigger* trig, cpu_job_type acceptable_type); // 0x005BDEB0 (:1441)
int scheduler_get_d3d_cpu_thread_num(); // 0x0058BBA0 (:1489)
void scheduler_set_thread_priority(unsigned int cpu_num, thread_prior priority); // 0x0058BBC0 (:1500)
cpu_controller* scheduler_get_this_context_controller(); // 0x005AE6C0 (:1515)
cpu_job_type scheduler_get_related_job_type(); // 0x005AE700 (:1528)
cpu_controller* scheduler_get_controller(int hw_thread_num); // 0x0058BBD0 (:1548)
int scheduler_get_num_controllers(); // 0x0058BBE0 (:1557)
int scheduler_get_num_havok_controllers(); // 0x0058BBF0 (:1564)
bool scheduler_in_render_thread_job(); // 0x005AE760 (:1580)
void scheduler_get_status_snapshot(scheduler_status* status); // 0x0058BC30 (:1594)
unsigned int scheduler_get_thread_id(unsigned int cpu_controller_index); // 0x0058BC60 (:1607)
void scheduler_set_controller_type(int controller, cpu_job_type type); // 0x005AE7B0 (:1613)
