// Original: volition/vlib/code/os/scheduler.cpp
//
// vlib_unity.obj is a unity build, so file statics are placed by the functions that use them. Initial values are read
// from the binary's .data section. In OpenRFG only the program on the original entry (OpenRFG, orig_main.md OM4)
// links this file; the test programs keep the inline stand-in of tests/stubs/vlib/vlib_noboot.cpp.
//
// Notes (decompiled behaviour, kept as is):
// - scheduler_q_job queues nothing for ccjt_any and ccjt_sim_main: such a job is taken from the free pool and never
//   runs (its output trigger is never fired). No caller in rfg.exe adds one.
// - The calling thread's controller (scheduler_relinquish_cpu, _until_event, scheduler_get_processor_number) is 0 for
//   the sim thread, 1 for the d3d thread, 1 for the culling thread and 0 for every other thread (the stage 2 thread,
//   the boot screen thread, a scheduler thread).
// - Controllers 8-15 are never initialized: scheduler_get_controller/_thread_id return them as they are.
// - scheduler_in_render_thread_job reads the current job of a controller without checking it.
// - The free pools are not checked: scheduler_add_job, scheduler_get_event_trigger and
//   scheduler_get_scheduling_trigger write through a null node when their pool is empty.
// - The atomic counters of scheduling_trigger are interlocked (LOCK prefixed in rfg.exe); num_inputs is a plain
//   increment.

#include "volition/vlib/code/os/lockfree_queues.h"
#include "volition/vlib/code/os/scheduler.h"

#include "volition/vlib/code/memory/page_pool.h"
#include "volition/vlib/code/memory/virtual_mempool.h"
#include "volition/vlib/code/os/frametime.h"
#include "volition/vlib/code/os/vconsole.h"

#include "keen/base/string.hpp"
#include "keen/base/thread.hpp"

#include <intrin.h>
#include <stdio.h>
#include <string.h>

// One worker configuration entry of Normal_cpu_setup.
// size 0x10
struct cpu_setup
{
    cpu_thread cpu_num; // 0x00
    cpu_job_type job_type; // 0x04
    cpu_activation_type activation_type; // 0x08
    bool allow_havok_jobs; // 0x0C
};

// How well a controller of one job type suits a job of another (Job_match_table_restrictive).
enum cpu_job_match
{
    match_good = 0,
    match_okay = 1,
    match_bad = 2,
};

int Scheduler_status = 1; // 0x01519594, 0 off, 1 on, 2 serial (unused_dcf_scheduler)
unsigned int Scheduler_sim_thread_id; // 0x01CE8778
unsigned int Scheduler_d3d_thread_id; // 0x01CE8774
unsigned int Scheduler_culling_thread_id; // 0x01CE877C

// The controllers the scheduler starts: 0 the sim helper (woken only by scheduler_relinquish_cpu), 1 the d3d helper,
// 2-7 workers for any job; the Havok jobs go to 0 and 2-5.
static cpu_setup Normal_cpu_setup[8] = { // 0x01519660
    { CPU_THREAD_0, ccjt_sim, ccat_explicit, true },
    { CPU_THREAD_1, ccjt_d3d, ccat_explicit, false },
    { CPU_THREAD_2, ccjt_any, ccat_auto, true },
    { CPU_THREAD_3, ccjt_any, ccat_auto, true },
    { CPU_THREAD_4, ccjt_any, ccat_auto, true },
    { CPU_THREAD_5, ccjt_any, ccat_auto, true },
    { CPU_THREAD_6, ccjt_any, ccat_auto, false },
    { CPU_THREAD_7, ccjt_any, ccat_auto, false },
};
// [job type][controller job type]
static cpu_job_match Job_match_table_restrictive[7][7] = { // 0x01519598
    { match_good, match_good, match_good, match_good, match_bad, match_bad, match_good }, // ccjt_any
    { match_good, match_good, match_bad, match_bad, match_bad, match_bad, match_bad }, // ccjt_d3d
    { match_good, match_bad, match_good, match_bad, match_bad, match_bad, match_bad }, // ccjt_d3d_cpu
    { match_good, match_bad, match_bad, match_good, match_bad, match_bad, match_good }, // ccjt_sim
    { match_bad, match_bad, match_bad, match_good, match_bad, match_bad, match_bad }, // ccjt_havok
    { match_good, match_bad, match_bad, match_bad, match_good, match_okay, match_good }, // ccjt_d3d_any
    { match_bad, match_bad, match_bad, match_bad, match_bad, match_bad, match_bad }, // ccjt_sim_main
};
// The job type a thread's work relates to, by its controller's job type (scheduler_get_related_job_type).
static const cpu_job_type Related_job_types[7] = { // 0x012C3314
    ccjt_d3d_any, ccjt_d3d_any, ccjt_d3d_any, ccjt_sim, ccjt_havok, ccjt_d3d_any, ccjt_sim,
};
static lf_node_queue<cpu_job> Free_jobs; // 0x0163E720
static lf_node_queue<event_trigger> Free_event_triggers; // 0x0163E770
static lf_node_queue<scheduling_trigger> Free_scheduling_triggers; // 0x0163E7C0
static lf_node_queue<cpu_job> Pending_sim_jobs; // 0x0163E810
static lf_node_queue<cpu_job> Pending_d3d_jobs; // 0x0163E860
static lf_node_queue<cpu_job> Pending_d3d_cpu_jobs; // 0x0163E8B0
static lf_node_queue<cpu_job> Pending_d3d_any_jobs; // 0x0163E900
static lf_node_queue<cpu_job> Pending_havok_jobs; // 0x0163E950
static bool Scheduler_init; // 0x01CE876A
static void (*Thread_creation_cb)(); // 0x01CE8780, run first on each scheduler thread
static page_pool* Scheduler_page_pool; // 0x01CE8784
static mempool_base* Scheduler_scratch_pool; // 0x01CE8788
static int Job_ordinal; // 0x01CE878C, restarts when Framecount changes
static lf_node<scheduling_trigger> Scheduling_triggers[64]; // 0x01D08F80
static lf_node<event_trigger> Event_triggers[64]; // 0x01D0D0C8
static lf_node<cpu_job> Cpu_jobs[1536]; // 0x01D3E0A8
static cpu_controller Cpu_controllers[16]; // 0x01D50120

static int get_job_ordinal(); // 0x0058B8F0 (:267)
static void scheduler_activate_one_cpu(cpu_job_type job_type); // 0x005BD650 (:491)

// ---- cpu_job

// 0x005ADF80 (.h:243)
cpu_job::cpu_job()
{
    trigger = nullptr;
    flags = 0;
    scratch_buffer = nullptr;
    scratch_buffer_size = 0;
    thread_id = keen::getCurrentThreadId();
}

// 0x0058B800 (.h:248)
// The ordinal is not copied.
cpu_job& cpu_job::operator=(const cpu_job& rhs)
{
    job_func = rhs.job_func;
    trigger = rhs.trigger;
    arg1 = rhs.arg1;
    arg2 = rhs.arg2;
    type = rhs.type;
    thread_id = rhs.thread_id;
    flags = rhs.flags;
    scratch_buffer = rhs.scratch_buffer;
    scratch_buffer_size = rhs.scratch_buffer_size;
    return *this;
}

// ---- the console command

// 0x005BD420 (:200)
// "scheduler on|off|serial": sets Scheduler_status, then prints it. Not registered in rfg.exe.
void unused_dcf_scheduler()
{
    char arg[256];
    char text[256];
    bool show_usage = true;
    if (Console_command != 0)
    {
        console_get_arg(3, 0);
        if (Console_arg_type == 1)
        {
            show_usage = false;
        }
        else
        {
            // Notes (decompiled behaviour): rfg.exe copies only a non-empty argument and compares its uninitialized
            // buffer otherwise; an empty one is copied here.
            strcpy(arg, Console_arg);
            if (keen::compareStringNoCase(arg, "on") == 0)
            {
                Scheduler_status = 1;
                show_usage = false;
            }
            else if (keen::compareStringNoCase(arg, "off") == 0)
            {
                Scheduler_status = 0;
                show_usage = false;
            }
            else if (keen::compareStringNoCase(arg, "serial") == 0)
            {
                Scheduler_status = 2;
                show_usage = false;
            }
            else
            {
                sprintf(text, "unrecognized arg: %s\n", arg);
                console_output(text, nullptr, false);
            }
        }
    }
    if (show_usage)
    {
        console_output("Usage: scheduler <scheduler state> = on/off/serial ", nullptr, false);
        console_output("<no argument> = show status", nullptr, false);
        console_output("on = enable", nullptr, false);
        console_output("off = disable", nullptr, false);
        console_output("serial = on, but execute jobs serially on one cpu", nullptr, false);
    }
    sprintf(text, "current scheduler status: ");
    strcat(text, Scheduler_status == 1 ? "on" : Scheduler_status == 0 ? "off" : "serial");
    console_output(text, nullptr, false);
}

// 0x0058B8F0 (:267)
static int get_job_ordinal()
{
    static int last_frame; // 0x01D579A4
    if (last_frame != Framecount)
    {
        last_frame = Framecount;
        Job_ordinal = 1;
        return 0;
    }
    return Job_ordinal++;
}

// ---- the thread roles

// 0x005AE2C0 (:281)
int scheduler_get_processor_number()
{
    if (Scheduler_sim_thread_id == keen::getCurrentThreadId())
    {
        return 0;
    }
    if (Scheduler_d3d_thread_id == keen::getCurrentThreadId())
    {
        return 1;
    }
    return Scheduler_culling_thread_id == keen::getCurrentThreadId() ? 1 : 0;
}

// 0x005AE300 (:322)
void scheduler_register_sim_thread()
{
    Scheduler_sim_thread_id = keen::getCurrentThreadId();
}

// 0x005AE310 (:327)
void scheduler_register_d3d_thread()
{
    Scheduler_d3d_thread_id = keen::getCurrentThreadId();
}

// 0x005AE320 (:332)
void scheduler_register_culling_thread()
{
    Scheduler_culling_thread_id = keen::getCurrentThreadId();
}

// 0x005AE330 (:337)
// True before the sim thread is known.
bool scheduler_is_sim_thread()
{
    return Scheduler_sim_thread_id == keen::getCurrentThreadId() || Scheduler_sim_thread_id == 0;
}

// 0x005AE350 (:342)
bool scheduler_is_render_thread()
{
    return Scheduler_d3d_thread_id == keen::getCurrentThreadId();
}

// 0x0058B920 (:347)
// Until game_render_thread_init registers the d3d thread.
bool scheduler_is_pre_threading()
{
    return Scheduler_d3d_thread_id == 0;
}

// 0x0058B930 (:352)
void scheduler_assert_render_thread()
{
}

// 0x0058B940 (:357)
void scheduler_assert_sim_thread()
{
}

// 0x0058B950 (:362)
void scheduler_assert_pre_threading()
{
}

// ---- start-up and shutdown

// 0x005C4540 (:367)
// The calling thread is the sim thread until scheduler_register_sim_thread. The pools are filled, the controllers
// configured from Normal_cpu_setup and started (explicit ones at THREAD_PRIOR_LOW, auto ones at THREAD_PRIOR_LOWEST),
// then the auto controllers made idle so that scheduler_add_job can wake them.
void scheduler_init(void (*thread_creation_cb)(), page_pool* scheduler_page_pool, mempool_base* scheduler_scratch_pool)
{
    if (Scheduler_init)
    {
        return;
    }
    Scheduler_init = true;
    Thread_creation_cb = thread_creation_cb;
    Scheduler_page_pool = scheduler_page_pool;
    Scheduler_scratch_pool = scheduler_scratch_pool;
    Scheduler_sim_thread_id = keen::getCurrentThreadId();
    for (int i = 0; i < 1536; i++)
    {
        Free_jobs.push_node(&Cpu_jobs[i]);
    }
    for (int i = 0; i < 64; i++)
    {
        Event_triggers[i].value.in_use = false;
        Free_event_triggers.push_node(&Event_triggers[i]);
    }
    for (int i = 0; i < 64; i++)
    {
        Free_scheduling_triggers.push_node(&Scheduling_triggers[i]);
    }
    for (int i = 0; i < 8; i++)
    {
        cpu_controller& controller = Cpu_controllers[Normal_cpu_setup[i].cpu_num];
        controller.job_type = Normal_cpu_setup[i].job_type;
        controller.activation_type = Normal_cpu_setup[i].activation_type;
        controller.allow_havok_jobs = Normal_cpu_setup[i].allow_havok_jobs;
    }
    // Notes (decompiled behaviour, kept as is): controller i takes the priority of Normal_cpu_setup[i], not of the
    // entry naming it (the same here: the entries are in controller order).
    for (int i = 0; i < 8; i++)
    {
        const thread_prior priority = Normal_cpu_setup[i].activation_type == ccat_explicit ? THREAD_PRIOR_LOW : THREAD_PRIOR_LOWEST;
        Cpu_controllers[i].init((cpu_thread)i, priority, nullptr);
    }
    for (int i = 0; i < 16; i++)
    {
        if (Cpu_controllers[i].activation_type == ccat_auto)
        {
            scheduler_add_cpu_thread((cpu_thread)i);
        }
    }
}

// 0x005C47F0 (:445)
// Every started controller is shut down (cpu_controller::shutdown, inlined), then the scheduler's scratch pool cleared.
void scheduler_shutdown()
{
    if (!Scheduler_init)
    {
        return;
    }
    Scheduler_init = false;
    for (int i = 0; i < 16; i++)
    {
        Cpu_controllers[i].shutdown();
    }
    Scheduler_page_pool = nullptr;
    if (Scheduler_scratch_pool != nullptr)
    {
        Scheduler_scratch_pool->set_thread_ownership();
        Scheduler_scratch_pool->clear();
        Scheduler_scratch_pool = nullptr;
    }
}

// 0x0058B960 (:465)
// An inactive controller becomes idle (a pending deactivation is dropped).
void scheduler_add_cpu_thread(cpu_thread thread_num)
{
    cpu_controller& controller = Cpu_controllers[thread_num];
    controller.state_cs.enter();
    controller.deactivate_pending = false;
    if (controller.get_state() == ccs_inactive)
    {
        controller.set_state(ccs_idle);
    }
    controller.state_cs.leave();
}

// 0x005AE370 (:480)
void scheduler_remove_cpu_thread(cpu_thread thread_num)
{
    if ((int)thread_num > 15)
    {
        return;
    }
    Cpu_controllers[thread_num].deactivate();
}

// 0x005BD650 (:491)
// Wakes the first idle controller that suits the job type: for Havok jobs any that allows them; else a good match,
// then an okay one. Nothing while the scheduler is off or serial.
static void scheduler_activate_one_cpu(cpu_job_type job_type)
{
    if (Scheduler_status == 0 || Scheduler_status == 2)
    {
        return;
    }
    if (job_type == ccjt_havok)
    {
        for (int i = 0; i < 16; i++)
        {
            if (Cpu_controllers[i].allow_havok_jobs && Cpu_controllers[i].activate_if_idle())
            {
                return;
            }
        }
        return;
    }
    for (int i = 0; i < 16; i++)
    {
        if (Job_match_table_restrictive[job_type][Cpu_controllers[i].job_type] == match_good && Cpu_controllers[i].activate_if_idle())
        {
            return;
        }
    }
    for (int i = 0; i < 16; i++)
    {
        if (Job_match_table_restrictive[job_type][Cpu_controllers[i].job_type] == match_okay && Cpu_controllers[i].activate_if_idle())
        {
            return;
        }
    }
}

// 0x005BD790 (:565)
void scheduler_q_job(cpu_job* this_job)
{
    switch (this_job->type)
    {
    case ccjt_d3d:
        Pending_d3d_jobs.push(this_job);
        break;
    case ccjt_d3d_cpu:
        Pending_d3d_cpu_jobs.push(this_job);
        break;
    case ccjt_sim:
        Pending_sim_jobs.push(this_job);
        break;
    case ccjt_havok:
        Pending_havok_jobs.push(this_job);
        break;
    case ccjt_d3d_any:
        Pending_d3d_any_jobs.push(this_job);
        break;
    default:
        // ccjt_any and ccjt_sim_main: not queued (see the notes at the top).
        break;
    }
}

// 0x005BD820 (:612)
void scheduler_free_event_trigger(event_trigger* trig)
{
    trig->countdown = 0;
    trig->active = false;
    trig->cpu = nullptr;
    trig->in_use = false;
    Free_event_triggers.push(trig);
}

// 0x005BD850 (:624)
void scheduler_free_scheduling_trigger(scheduling_trigger* trig)
{
    trig->countdown = 0;
    trig->active = false;
    trig->num_dependent_jobs = 0;
    Free_scheduling_triggers.push(trig);
}

// 0x005AE3C0 (:635)
// arg1: the controller.
void job_deactivate_cpu_thread(cpu_job* de_job, unsigned int arg1, unsigned int arg2)
{
    Cpu_controllers[arg1].deactivate();
}

// 0x0058B9B0 (:644)
void job_null(cpu_job* de_job, unsigned int arg1, unsigned int arg2)
{
}

// 0x005AE410 (:668)
// A controller's thread: registers itself (thread id, its thread scratch pool), reports that it reached its first wait
// (cpu_controller::init spins on it), then runs its function each time it is signalled until it is asked to exit.
int cpu_thread_proc(void* lpParameter)
{
    cpu_controller* controller = (cpu_controller*)lpParameter;
    if (Thread_creation_cb != nullptr)
    {
        Thread_creation_cb();
    }
    controller->thread_id = keen::getCurrentThreadId();
    controller->register_scratch_mempool();
    controller->reached_initial_wait = true;
    controller->wait_for_event();
    while (!controller->thread_exit_pending)
    {
        controller->call_function();
        controller->wait_for_event();
    }
    return 0;
}

// ---- cpu_controller

// 0x005ADFB0 (.h:285)
cpu_controller::cpu_controller()
    : state_cs(0), event_handle(false, false)
{
    thread_exit_pending = false;
    reached_initial_wait = false;
    deactivate_pending = false;
    state = ccs_uninitialized;
    havok_memory_initialized = false;
    thread_handle = 0;
    cpu_controller_virtual_mempool = nullptr;
}

// 0x005AE070 (.h:286)
cpu_controller::~cpu_controller()
{
}

// 0x0058B840 (.h:292)
void cpu_controller::set_state(cpu_state _state)
{
    state_cs.enter();
    state = _state;
    state_cs.leave();
}

// 0x0058B870 (.h:299)
cpu_state cpu_controller::get_state() const
{
    return state;
}

// 0x0058B880 (.h:305)
void cpu_controller::call_function()
{
    fp(this);
}

// 0x0058B890 (.h:306)
cpu_job* cpu_controller::get_current_job()
{
    return current_job;
}

// 0x0058B8A0 (.h:352)
// memory_scratch_thread_init for the calling thread, inlined.
void cpu_controller::register_scratch_mempool()
{
    Vlib_scratch_pool = &scratch_mempool;
    Vlib_scratch_pool_list[Vlib_scratch_pool_list_size] = &scratch_mempool;
    Vlib_scratch_pool_list_size++;
}

// 0x0058B9C0 (:747)
// Idle: inactive at once; otherwise once the current work is done (default_sim_cpu_function).
void cpu_controller::deactivate()
{
    state_cs.enter();
    if (state == ccs_idle)
    {
        state = ccs_inactive;
        state_cs.leave();
        return;
    }
    deactivate_pending = true;
    state_cs.leave();
}

// 0x0058BA00 (:762)
void cpu_controller::wait_until_inactive()
{
    while (state != ccs_inactive)
    {
    }
}

// 0x0058BA10 (:1018)
// Magic number: 16, the pool's alignment.
void cpu_controller::override_scratch_mempool_for_job(cpu_job* job)
{
    scratch_mempool.init(job->scratch_buffer, job->scratch_buffer_size, "Job Specific Scratch Pool", 16, 0);
}

// 0x0058BA40 (:1030)
// Magic number: 16, the pool's alignment.
void cpu_controller::restore_scratch_mempool()
{
    scratch_mempool.init(scratch_buffer, scratch_buffer_size, "Scheduler Thread Scratch Pool", 16, 0);
}

// 0x005AE0A0 (.h:304)
void cpu_controller::wait_for_event()
{
    event_handle.wait();
}

// 0x005AE0B0 (.h:333)
void cpu_controller::activate()
{
    event_handle.signal();
    state = ccs_work_pending;
}

// 0x005AE490 (:715)
// An idle controller is signalled (and a pending deactivation dropped); true when it was idle.
bool cpu_controller::activate_if_idle()
{
    state_cs.enter();
    const bool idle = state == ccs_idle;
    if (idle)
    {
        activate();
        deactivate_pending = false;
    }
    state_cs.leave();
    return idle;
}

// 0x005BD880 (:785)
// A Havok job first when the controller allows them; then by its job type: any takes d3d, d3d_any, d3d_cpu and sim
// jobs; d3d takes d3d and d3d_any; d3d_cpu takes d3d_cpu and d3d_any; sim and sim_main take sim jobs; havok and d3d_any
// controllers nothing else.
cpu_job* cpu_controller::get_next_job()
{
    if (allow_havok_jobs)
    {
        cpu_job* job = Pending_havok_jobs.pop();
        if (job != nullptr)
        {
            return job;
        }
    }
    cpu_job* job = nullptr;
    switch (job_type)
    {
    case ccjt_any:
        job = Pending_d3d_jobs.pop();
        if (job == nullptr)
        {
            job = Pending_d3d_any_jobs.pop();
        }
        if (job == nullptr)
        {
            job = Pending_d3d_cpu_jobs.pop();
        }
        if (job == nullptr)
        {
            job = Pending_sim_jobs.pop();
        }
        break;
    case ccjt_d3d:
        job = Pending_d3d_jobs.pop();
        if (job == nullptr)
        {
            job = Pending_d3d_any_jobs.pop();
        }
        break;
    case ccjt_d3d_cpu:
        job = Pending_d3d_cpu_jobs.pop();
        if (job == nullptr)
        {
            job = Pending_d3d_any_jobs.pop();
        }
        break;
    case ccjt_sim:
    case ccjt_sim_main:
        job = Pending_sim_jobs.pop();
        break;
    default:
        break;
    }
    return job;
}

// 0x005BD990 (:842)
// The controller's work when signalled: jobs until none is left (then idle) or a deactivation is pending (then
// inactive). A job with its own scratch buffer gets the thread scratch pool over it while it runs; a finished job fires
// its output trigger and returns to the free pool.
void cpu_controller::default_sim_cpu_function(cpu_controller* this_cpu)
{
    this_cpu->set_state(ccs_working);
    for (;;)
    {
        if (this_cpu->deactivate_pending)
        {
            this_cpu->state_cs.enter();
            if (this_cpu->deactivate_pending)
            {
                this_cpu->state = ccs_inactive;
                this_cpu->deactivate_pending = false;
                this_cpu->state_cs.leave();
                return;
            }
            this_cpu->state_cs.leave();
        }
        cpu_job* job = this_cpu->get_next_job();
        this_cpu->current_job = job;
        if (job == nullptr)
        {
            // Again under the lock, so that a job queued meanwhile (whose activate_if_idle saw this controller
            // working) is not left behind.
            this_cpu->state_cs.enter();
            job = this_cpu->get_next_job();
            this_cpu->current_job = job;
            if (job == nullptr)
            {
                this_cpu->state = ccs_idle;
                this_cpu->state_cs.leave();
                return;
            }
            this_cpu->state_cs.leave();
        }
        else
        {
            this_cpu->state = ccs_working;
        }
        if (job->scratch_buffer != nullptr)
        {
            this_cpu->override_scratch_mempool_for_job(job);
        }
        job->job_func(job, job->arg1, job->arg2);
        if (job->scratch_buffer != nullptr)
        {
            this_cpu->restore_scratch_mempool();
        }
        if (job->trigger != nullptr)
        {
            job->trigger->fire();
        }
        this_cpu->current_job = nullptr;
        Free_jobs.push(job);
    }
}

// 0x005BDAD0 (:924)
// Its "cpu virtual pool" (1 MB, get_this_context_virtual_mempool), its thread scratch pool from the scheduler's scratch
// pool and its thread "scheduler thread N" (384 KB stack), which this waits for.
void cpu_controller::init(cpu_thread thread_num, thread_prior priority, void (*_fp)(cpu_controller*))
{
    char name[256];
    state_cs.enter();
    fp = _fp != nullptr ? _fp : default_sim_cpu_function;
    init_cpu_virtual_mempool();
    // Magic numbers: 96 KB of scratch; 32 KB for an auto sim controller (0xFFFF0000 + 0x18000 wraps; there is none).
    if (job_type == ccjt_d3d || job_type == ccjt_d3d_cpu || job_type == ccjt_any)
    {
        scratch_buffer_size = 0x18000;
    }
    else
    {
        scratch_buffer_size = activation_type != ccat_explicit ? 0x8000 : 0x18000;
    }
    scratch_buffer = (unsigned char*)Scheduler_scratch_pool->alloc(scratch_buffer_size, 0);
    restore_scratch_mempool();
    sprintf(name, "scheduler thread %d", (int)thread_num);
    // Magic number: 0x60000, the stack size.
    thread_handle = thread_spawn(cpu_thread_proc, this, 0, priority, name, 0x60000);
    cpu_thread_num = thread_num;
    state = ccs_inactive;
    state_cs.leave();
    while (!reached_initial_wait)
    {
    }
}

// 0x005BDC00 (:998)
// The thread is woken with its exit flag set and joined; the virtual pool goes back to the page pool.
void cpu_controller::shutdown()
{
    if (state == ccs_uninitialized)
    {
        return;
    }
    thread_exit_pending = true;
    state_cs.enter();
    activate();
    state_cs.leave();
    thread_wait_until_done(thread_handle);
    release_cpu_virtual_mempool();
    state = ccs_uninitialized;
}

// 0x005AE810 (:1624)
// Magic number: 1 MB.
void cpu_controller::init_cpu_virtual_mempool()
{
    if (Scheduler_page_pool != nullptr)
    {
        cpu_controller_virtual_mempool = Scheduler_page_pool->get_virtual_mempool("cpu virtual pool", 0x100000);
    }
}

// 0x005AE840 (:1633)
void cpu_controller::release_cpu_virtual_mempool()
{
    if (cpu_controller_virtual_mempool != nullptr)
    {
        Scheduler_page_pool->release_virtual_mempool(cpu_controller_virtual_mempool);
        cpu_controller_virtual_mempool = nullptr;
    }
}

// ---- triggers

// 0x0058BA70 (:1043)
trigger_base::trigger_base()
    : countdown(0), active(false), active_cs(0)
{
}

// 0x0058B780 (.h:185)
trigger_base::~trigger_base()
{
}

// 0x005AE4D0 (:1046)
event_trigger::event_trigger()
    : event(false, false)
{
}

// 0x005ADEF0 (.h:202)
event_trigger::~event_trigger()
{
}

// 0x0058BAB0 (:1049)
scheduling_trigger::scheduling_trigger()
    : num_dependent_jobs(0)
{
}

// 0x0058B7E0 (.h:219)
scheduling_trigger::~scheduling_trigger()
{
}

// 0x005AE530 (:1055)
// The last input of an active trigger: the waiting thread's controller stops helping and the waiter is woken.
void event_trigger::fire()
{
    active_cs.enter();
    countdown--;
    if (countdown == 0 && active)
    {
        if (cpu != nullptr)
        {
            cpu->deactivate();
        }
        fired = true;
        event.signal();
    }
    active_cs.leave();
}

// 0x005BDC60 (:1092)
// The last input of an active trigger queues the dependent jobs; the trigger goes back to the free pool.
void scheduling_trigger::fire()
{
    active_cs.enter();
    if (_InterlockedDecrement((volatile long*)&countdown) == 0 && active)
    {
        for (unsigned int i = 0; i < num_dependent_jobs; i++)
        {
            scheduler_q_job(dependent_jobs[i]);
            scheduler_activate_one_cpu(dependent_jobs[i]->type);
        }
        scheduler_free_scheduling_trigger(this);
    }
    active_cs.leave();
}

// 0x0058BB30 (:1112)
void event_trigger::attach_input()
{
    num_inputs++;
    active_cs.enter();
    countdown++;
    active_cs.leave();
}

// 0x0058BB50 (:1125)
void scheduling_trigger::attach_input()
{
    num_inputs++;
    _InterlockedIncrement((volatile long*)&countdown);
}

// 0x0058BB60 (:1130)
void event_trigger::attach_output(cpu_job* job)
{
}

// 0x0058BB70 (:1141)
// Magic number: 16 dependent jobs at most, not checked.
void scheduling_trigger::attach_output(cpu_job* job)
{
    const unsigned int index = (unsigned int)_InterlockedExchangeAdd((volatile long*)&num_dependent_jobs, 1);
    dependent_jobs[index] = job;
}

// 0x005AE570 (:1153)
// Waits for the inputs not finished yet, with the waiting thread's controller (cpu, set by scheduler_relinquish_cpu)
// woken to run jobs meanwhile.
void event_trigger::activate()
{
    active_cs.enter();
    active = true;
    const unsigned int remaining = countdown;
    active_cs.leave();
    if (num_inputs != 0 && remaining != 0)
    {
        if (cpu != nullptr)
        {
            cpu->state_cs.enter();
            scheduler_add_cpu_thread(cpu->cpu_thread_num);
            cpu->activate_if_idle();
            cpu->state_cs.leave();
        }
        event.wait();
    }
}

// 0x005BDD00 (:1202)
// Without pending inputs the dependent jobs are queued at once and the trigger goes back to the free pool.
void scheduling_trigger::activate()
{
    active_cs.enter();
    active = true;
    if (countdown == 0)
    {
        for (unsigned int i = 0; i < num_dependent_jobs; i++)
        {
            scheduler_q_job(dependent_jobs[i]);
            scheduler_activate_one_cpu(dependent_jobs[i]->type);
        }
        scheduler_free_scheduling_trigger(this);
    }
    active_cs.leave();
}

// ---- the job API

// 0x005BDD80 (:1224)
event_trigger* scheduler_get_event_trigger()
{
    event_trigger* trig = Free_event_triggers.pop();
    trig->in_use = true;
    trig->event.reset();
    trig->fired = false;
    trig->cpu = nullptr;
    trig->num_inputs = 0;
    return trig;
}

// 0x005BDDC0 (:1244)
scheduling_trigger* scheduler_get_scheduling_trigger()
{
    scheduling_trigger* trig = Free_scheduling_triggers.pop();
    trig->num_dependent_jobs = 0;
    trig->num_inputs = 0;
    return trig;
}

// 0x005BDDF0 (:1259)
// The job is copied into a pooled one with this frame's ordinal. With an output trigger the job becomes its input; with
// an input trigger it waits for that trigger, else it is queued and a controller woken.
cpu_job* scheduler_add_job(trigger_base* input_trig, cpu_job* job, trigger_base* output_trig)
{
    cpu_job* this_job = Free_jobs.pop();
    *this_job = *job;
    this_job->ordinal = get_job_ordinal();
    if (output_trig != nullptr)
    {
        this_job->trigger = output_trig;
        output_trig->attach_input();
    }
    if (input_trig == nullptr)
    {
        scheduler_q_job(this_job);
        scheduler_activate_one_cpu(this_job->type);
        return this_job;
    }
    input_trig->attach_output(this_job);
    return this_job;
}

// 0x0058BB90 (:1286)
void scheduler_activate_trigger(trigger_base* trig)
{
    trig->activate();
}

// 0x005AE5F0 (:1291)
// The calling thread's controller runs jobs (of acceptable_type, unless any) until the event is signalled.
void scheduler_relinquish_cpu_until_event(sync_event& event, cpu_job_type acceptable_type)
{
    const int index = scheduler_get_processor_number();
    cpu_controller& controller = Cpu_controllers[index];
    const cpu_job_type saved_type = controller.job_type;
    if (acceptable_type != ccjt_any)
    {
        controller.job_type = acceptable_type;
    }
    scheduler_add_cpu_thread((cpu_thread)index);
    controller.activate_if_idle();
    event.wait();
    controller.job_type = saved_type;
}

// 0x005BDEB0 (:1441)
// Waits for the trigger's inputs while the calling thread's controller runs jobs (of acceptable_type, unless any), then
// frees the trigger.
void scheduler_relinquish_cpu(event_trigger* trig, cpu_job_type acceptable_type)
{
    if (trig == nullptr)
    {
        return;
    }
    cpu_controller& controller = Cpu_controllers[scheduler_get_processor_number()];
    const cpu_job_type saved_type = controller.job_type;
    if (acceptable_type != ccjt_any)
    {
        controller.job_type = acceptable_type;
    }
    trig->cpu = &controller;
    trig->activate();
    scheduler_free_event_trigger(trig);
    controller.job_type = saved_type;
}

// ---- queries

// 0x0058BBA0 (:1489)
int scheduler_get_d3d_cpu_thread_num()
{
    for (int i = 0; i < 16; i++)
    {
        if (Cpu_controllers[i].job_type == ccjt_d3d)
        {
            return i;
        }
    }
    return -1;
}

// 0x0058BBC0 (:1500)
void scheduler_set_thread_priority(unsigned int cpu_num, thread_prior priority)
{
}

// 0x005AE6C0 (:1515)
// The controller whose thread this is, else null.
cpu_controller* scheduler_get_this_context_controller()
{
    const unsigned int thread_id = keen::getCurrentThreadId();
    for (int i = 0; i < 16; i++)
    {
        if (Cpu_controllers[i].thread_id == thread_id)
        {
            return &Cpu_controllers[i];
        }
    }
    return nullptr;
}

// 0x005AE700 (:1528)
cpu_job_type scheduler_get_related_job_type()
{
    cpu_controller* controller = scheduler_get_this_context_controller();
    if (controller != nullptr)
    {
        return Related_job_types[controller->job_type];
    }
    return Related_job_types[Scheduler_d3d_thread_id != keen::getCurrentThreadId() ? ccjt_sim : ccjt_d3d];
}

// 0x0058BBD0 (:1548)
cpu_controller* scheduler_get_controller(int hw_thread_num)
{
    return &Cpu_controllers[hw_thread_num];
}

// 0x0058BBE0 (:1557)
int scheduler_get_num_controllers()
{
    return 16;
}

// 0x0058BBF0 (:1564)
int scheduler_get_num_havok_controllers()
{
    int count = 0;
    for (int i = 0; i < 16; i++)
    {
        if (Cpu_controllers[i].allow_havok_jobs)
        {
            count++;
        }
    }
    return count;
}

// 0x005AE760 (:1580)
// The calling scheduler thread runs a job that the d3d thread added.
bool scheduler_in_render_thread_job()
{
    cpu_controller* controller = scheduler_get_this_context_controller();
    if (controller == nullptr)
    {
        return false;
    }
    return controller->get_current_job()->thread_id == Scheduler_d3d_thread_id;
}

// 0x0058BC30 (:1594)
// Only the controller states are written.
void scheduler_get_status_snapshot(scheduler_status* status)
{
    for (int i = 0; i < 16; i++)
    {
        status->cpu_states[i] = Cpu_controllers[i].get_state();
    }
}

// 0x0058BC60 (:1607)
unsigned int scheduler_get_thread_id(unsigned int cpu_controller_index)
{
    return Cpu_controllers[cpu_controller_index].thread_id;
}

// 0x005AE7B0 (:1613)
void scheduler_set_controller_type(int controller, cpu_job_type type)
{
    Cpu_controllers[controller].job_type = type;
    Cpu_controllers[controller].activate_if_idle();
}
