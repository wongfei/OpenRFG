// Original: volition/rfg/code/geom/cloth_simulation.h
// Implementation: volition/rfg/code/geom/cloth_simulation.cpp
//
// Cloth simulation (capes, flags, tarps on characters and objects): a cloth simulation instance runs the streamed cloth
// data against its owner's bones as a batched scheduler job (pre-simulate, simulate, post-simulate), with wind; the
// stream2 load callbacks registered by game_stream2_register_types, the pool (Pool_cloth_simulations, 115) and the list
// of preloaded cloth files.
#pragma once

#include "volition/vlib/code/math/matrix.h"
#include "volition/vlib/code/math/vector3.h"
#include "volition/vlib/code/time/timestamp.h"

class anim_rig;
struct cloth_sim_collision_primitive;
struct cloth_sim_info;
struct cpu_job;
union cpu_job_arg;
class event_trigger;
class object;
class quaternion;
// size 0x44 (placement inferred)
struct simulated_node
{
    vector ws_position; // 0x00
    vector last_frame_ws_position; // 0x0C
    matrix parent_rotation; // 0x18
    float collide_correction; // 0x3C
    unsigned int anchor : 1; // 0x40:0
    unsigned int collided : 1; // 0x40:1
};

// size 0x10 (placement inferred)
struct simulated_node_link
{
    vector last_frame_dir; // 0x00
    float error_correction; // 0x0C
};

// size 0x110
class cloth_simulation
{
public:
    void simulate(); // 0x00805090 (cloth_simulation_shared_spu.cpp:111)
    void pre_simulate(object* objp, matrix43* bone_matrices, matrix43* collider_matrices, const vector* collider_ws_pos, const matrix* collider_ws_orient); // 0x008028A0 (:202)
    void post_simulate(matrix43* bone_matrices); // 0x008040B0 (:573)
    void cloth_sim_setup_job(); // 0x007F2580 (:197)
    cloth_simulation(); // 0x007FAF20

    cloth_simulation* next; // 0x00
    cloth_simulation* prev; // 0x04
    cloth_sim_info* info; // 0x08
    void* buffer; // 0x0C
    simulated_node* nodes; // 0x10
    simulated_node_link* node_links; // 0x14
    cloth_sim_collision_primitive* colliders; // 0x18
    vector* objp_pos; // 0x1C
    matrix* objp_orient; // 0x20
    int aim_handle; // 0x24
    vector last_frame_pos; // 0x28
    matrix last_frame_orient; // 0x34
    vector obj_pos_diff; // 0x58
    unsigned int initialize_nodes : 1; // 0x64:0
    unsigned int do_explosion_gust : 1; // 0x64:1
    unsigned int processed : 1; // 0x64:2
    unsigned int inherite_obj_motion : 1; // 0x64:3
    unsigned int half_collisions : 1; // 0x64:4
    float cur_wind_heading; // 0x68
    float cur_wind_strength; // 0x6C
    float wind_heading_offset; // 0x70
    float wind_strength_offset; // 0x74
    timestamp wind_recalc_ts; // 0x78
    vector* anchor_pos; // 0x7C
    vector* sim_pos; // 0x80
    vector* collider_desired_pos; // 0x84
    matrix* collider_desired_orient; // 0x88
    quaternion* collider_desired_qorient; // 0x8C
    quaternion* collider_init_qorient; // 0x90
    vector saved_obj_pos; // 0x94
    matrix saved_obj_orient; // 0xA0
    matrix43* saved_collider_matrices; // 0xC4
    anim_rig* collider_rig; // 0xC8
    vector wind_dir; // 0xCC
    vector explosion_gust; // 0xD8
    float rand_wind_mult; // 0xE4
    matrix43* saved_bone_matrices; // 0xE8
    unsigned int data_size; // 0xEC
    timestamp wind_foley_ts; // 0xF0
    unsigned int output_data_start[2]; // 0xF4
    unsigned int output_data_size[2]; // 0xFC
    float frametime; // 0x104
    int info_index; // 0x108
    int padding[1]; // 0x10C
};

extern cloth_sim_info* Cloth_simulation_infos[64]; // 0x025E7F90
extern int Cloth_simulation_data_sizes[64]; // 0x025C1480
extern int Num_cloth_simulation_infos; // 0x025EF8C4
extern unsigned int Cloth_simulation_srids[64]; // 0x025DCD80
extern int Num_cloth_simulation_srids; // 0x025EF8C8
extern int Num_cloth_simulation_srids_dlc_mark; // 0x025337C4
extern event_trigger* Cloth_sim_scheduler_trigger; // 0x025EF8A0
extern cloth_simulation* Cloth_sim_batch_jobs; // 0x025EF8CC
extern int Num_cloth_sim_jobs; // 0x025EF8D0
extern bool Cloth_sim_disabled; // 0x025EF8D7
extern bool Cloth_debug; // 0x025EF8D6
extern bool Cloth_debug_colliders; // 0x025EF8D8

bool cloth_sim_load_cb(const char* filename, unsigned int srid, unsigned char* cpu_data, int cpu_data_size, unsigned char* gpu_data, int gpu_data_size); // 0x0080BD50 (:758)
void cloth_sim_unload_cb(const char* filename, unsigned int srid); // 0x007F25C0 (:778)
cloth_simulation* cloth_sim_create(char* mesh_name, vector* objp_pos, matrix* objp_orient, int aim_handle); // 0x00804640 (:784)
void cloth_sim_delete(cloth_simulation* cp); // 0x0080AA80 (:879)
bool cloth_sim_init(); // 0x007F25D0 (:896)
void cloth_sim_system_init(int dlc_id); // 0x00804870 (:901)
void cloth_sim_system_shutdown(); // 0x007F25E0 (:919)
void cloth_sim_set_thread_ownership(); // 0x007F2620 (:932)
void cloth_sim_dlc_reset(int dlc_id); // 0x008048B0 (:937)
void cloth_sim_process_xbox_pc(cpu_job* lj, cpu_job_arg arg1, cpu_job_arg arg2); // 0x0080AAE0 (:955)
void cloth_sim_batch_process_start(); // 0x0080AAF0 (:963)
void cloth_sim_batch_process_end(); // 0x00804AA0 (:1065)
void cloth_sim_set_always_windy(bool always_windy); // 0x007F2630 (:1091)
