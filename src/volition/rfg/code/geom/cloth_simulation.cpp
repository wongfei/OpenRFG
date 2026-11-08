// Original: volition/rfg/code/geom/cloth_simulation.cpp
//
// Cloth simulation (stub_impl.md S3): capes, flags and tarps on characters and items. The cloth files (.sim_pc) are
// preloaded at start-up (cloth_simulation_shared.cpp); cloth_sim_create matches a mesh name to one and carves the
// runtime arrays out of Cloth_sim_runtime_mempool. Each frame the owner calls pre_simulate (anchors from the bones,
// collider transforms, wind; queues the instance), cloth_sim_batch_process_start runs simulate
// (cloth_simulation_shared_spu.cpp) for every queued instance as a scheduler job, and cloth_sim_batch_process_end waits
// for them and writes the result back into the bone matrices (post_simulate, inlined there).
//
// Floating point: rfg.exe evaluates each scalar statement in double and rounds on assignment; vector operations round
// per operation (written here as float vector code). The double steps are taken from the asm; not verified bit-exact
// (no hkdiff case).
//
// Notes (decompiled behaviour, kept as is):
// - pre_simulate reads the parent player's fields without checking the handle lookup (object_is_player said yes).
// - post_simulate reads the bone of node_info[parent_node_index] even for a node without a parent (index -1).
// - post_simulate leaves a node whose two directions are exactly opposite (dot -1) unrotated and out of the bone list.

#include "volition/rfg/code/geom/cloth_simulation.h"

#include "volition/anim_lib/code/anim_lib.h"
#include "volition/audio_lib/code/audiolib_api.h"
#include "volition/rfg/code/camera/rfg_camera.h"
#include "volition/rfg/code/geom/character_instance.h"
#include "volition/rfg/code/geom/cloth_simulation_shared.h"
#include "volition/rfg/code/geom/vanim.h"
#include "volition/rfg/code/level/world.h"
#include "volition/rfg/code/objects/humans/human.h"
#include "volition/rfg/code/objects/humans/human_combat_turret.h"
#include "volition/rfg/code/objects/humans/human_enums.h"
#include "volition/rfg/code/objects/humans/player.h"
#include "volition/rfg/code/objects/object.h"
#include "volition/rfg/code/objects/object_item.h"
#include "volition/rfg/code/objects/turret.h"
#include "volition/rfg/code/objects/turret_info.h"
#include "volition/rfg/code/rfg.h"
#include "volition/rfg/code/weather/wind.h"
#include "volition/stream2/code/stream2.h"
#include "volition/stream2/code/stream2_container.h"
#include "volition/vlib/code/cfile/cfile.h"
#include "volition/vlib/code/math/floating.h"
#include "volition/vlib/code/math/quaternion.h"
#include "volition/vlib/code/math/rand.h"
#include "volition/vlib/code/memory/object_pool.h"
#include "volition/vlib/code/memory/page_mempool.h"
#include "volition/vlib/code/os/frametime.h"
#include "volition/vlib/code/os/scheduler.h"
#include "volition/vlib/code/os/thread.h"
#include "volition/vlib/code/util/string_util.h"

#include <math.h>
#include <string.h>

int Cloth_simulation_data_sizes[64]; // 0x025C1480
unsigned int Cloth_simulation_srids[64]; // 0x025DCD80
cloth_sim_info* Cloth_simulation_infos[64]; // 0x025E7F90
int Num_cloth_simulation_infos; // 0x025EF8C4
int Num_cloth_simulation_srids; // 0x025EF8C8
int Num_cloth_simulation_srids_dlc_mark; // 0x025337C4
event_trigger* Cloth_sim_scheduler_trigger; // 0x025EF8A0
cloth_simulation* Cloth_sim_batch_jobs; // 0x025EF8CC
int Num_cloth_sim_jobs; // 0x025EF8D0
bool Cloth_sim_disabled; // 0x025EF8D7
bool Cloth_debug; // 0x025EF8D6
bool Cloth_debug_colliders; // 0x025EF8D8
static bool Always_windy; // 0x025EF8D4
static bool Cloth_sim_init; // 0x025EF8D5
static object_pool<cloth_simulation, 115, 1> Pool_cloth_simulations(nullptr, nullptr, 0); // 0x02617C70
// The owner moved further than this many metres per second of frame time: the cloth starts over at the bind pose.
static float diff_reset = 30.0f; // 0x0165F870

// The cloth file's container type and primitive type (GSP_CLOTH_SIM).
static const unsigned char CONTAINER_TYPE_CLOTH_SIM = 9;

// 0x007FAF20
// Only the two timestamps' constructors (unset, -1).
cloth_simulation::cloth_simulation()
{
}

// 0x007F2550 (:183)
static void cloth_sim_unload_all()
{
    for (int i = 0; i < Num_cloth_simulation_srids; i++)
    {
        stream2_release(Cloth_simulation_srids[i]);
    }
}

// 0x007F2580 (:197)
// The instance appended to the batch (a ring through next/prev).
void cloth_simulation::cloth_sim_setup_job()
{
    if (Cloth_sim_batch_jobs == nullptr)
    {
        next = this;
        prev = this;
        Cloth_sim_batch_jobs = this;
        return;
    }
    prev = Cloth_sim_batch_jobs->prev;
    next = Cloth_sim_batch_jobs;
    Cloth_sim_batch_jobs->prev->next = this;
    Cloth_sim_batch_jobs->prev = this;
}

// Not in the binary: the bone or parent-node bone a node info hangs from, in the owner's model space, turned into world
// space through the owner's transform (the pre_simulate node and anchor set-up).
static void cloth_node_to_world(vector& pos, int bone, const matrix43* bone_matrices, const vector& obj_pos, const matrix& obj_orient)
{
    if (bone != -1)
    {
        const matrix43& m = bone_matrices[bone];
        pos.unrotate(m.m_rotation);
        pos.x = pos.x + m.m_translation.x;
        pos.y = m.m_translation.y + pos.y;
        pos.z = m.m_translation.z + pos.z;
    }
    pos.unrotate(obj_orient);
    pos.x = pos.x + obj_pos.x;
    pos.y = obj_pos.y + pos.y;
    pos.z = obj_pos.z + pos.z;
}

// 0x008028A0 (:202)
// The frame's set-up: a turret operator's cloth follows the turret (or restarts when it is not animated), a player's
// item stops looping its animations; the anchors from the bones, the owner's motion (restarting the cloth when it
// jumped), the collider transforms (ends of each capsule; slerp keys when there are several passes), the wind gusts
// and their foley; then the instance is queued for the batch.
void cloth_simulation::pre_simulate(object* objp, matrix43* bone_matrices, matrix43* collider_matrices, const vector* collider_ws_pos, const matrix* collider_ws_orient)
{
    processed = 0;
    if (Cloth_sim_disabled)
    {
        initialize_nodes = 1;
        return;
    }
    const int num_anchor_nodes = info->num_anchor_nodes;
    const int num_node_links = info->num_node_links;
    const int num_nodes = info->num_simulated_nodes + num_anchor_nodes;
    const simulated_node_info* node_infos = info->nodes.to_pointer();
    const simulated_node_link_info* link_infos = info->node_links.to_pointer();
    const cloth_sim_collision_primitive_info* collider_infos = info->colliders.to_pointer();
    bool operating_turret = false;
    if (objp != nullptr)
    {
        objp->adjust_transforms();
        if (object_is_player(objp->get_parent()))
        {
            human* hp = nullptr;
            if (objp->get_parent() != 0xFFFFFFFF)
            {
                object* parent = World.object_from_handle(objp->get_parent());
                if (parent != nullptr && parent->get_obj_type() == OT_HUMAN && parent->get_sub_type() == OT_SUB_HUMAN_PLAYER)
                {
                    hp = (human*)parent;
                }
            }
            if (hp->vehicle_handle == 0xFFFFFFFF || (hp->cur_state != HS_VEHICLE_DRIVING && hp->cur_state != HS_VEHICLE_SITTING))
            {
                if (objp->get_obj_type() == OT_ITEM)
                {
                    character_instance* ci = ((item*)objp)->char_inst;
                    if (ci->vp->desired_state != nullptr)
                    {
                        animlib_clear_looping_animations(ci->aim_handle, false);
                        memset(ci->vp->desired_state, 0, sizeof(v_anim));
                        ci->vp->desired_state->anim_handle = -1;
                        ci->vp->desired_state = nullptr;
                    }
                }
            }
            else
            {
                turret* t = human_combat_get_manned_turret(hp);
                if (t == nullptr || t->t_info->flags.no_operator_animation || t->should_autoaim())
                {
                    initialize_nodes = 1;
                    return;
                }
                operating_turret = true;
            }
            half_collisions = hp->cur_stance == HST_CROUCH ? 0 : 1;
        }
    }
    if (objp_pos != nullptr)
    {
        saved_obj_pos = *objp_pos;
        saved_obj_orient = *objp_orient;
    }
    vector collider_pos;
    if (collider_ws_pos == nullptr)
    {
        collider_pos.zero();
    }
    else
    {
        collider_pos = *collider_ws_pos;
    }
    matrix43 root;
    animlib_get_bone_transform(aim_handle, 0, root);
    vector root_pos = root.m_translation;
    root_pos.unrotate(*objp_orient);
    root_pos.x = root_pos.x + saved_obj_pos.x;
    root_pos.y = saved_obj_pos.y + root_pos.y;
    root_pos.z = saved_obj_pos.z + root_pos.z;
    obj_pos_diff.x = root_pos.x - last_frame_pos.x;
    obj_pos_diff.y = root_pos.y - last_frame_pos.y;
    obj_pos_diff.z = root_pos.z - last_frame_pos.z;
    const float diff_len_sq = (float)((double)obj_pos_diff.x * obj_pos_diff.x + (double)obj_pos_diff.y * obj_pos_diff.y + (double)obj_pos_diff.z * obj_pos_diff.z);
    if ((double)(float)sqrt((double)diff_len_sq) > (double)Frametime * (double)diff_reset)
    {
        initialize_nodes = 1;
    }
    // The owner's rotation since last frame.
    matrix delta_orient = *objp_orient * last_frame_orient;
    last_frame_orient = objp_orient->copy_inverse();
    if (initialize_nodes)
    {
        last_frame_pos = root_pos;
        obj_pos_diff.zero();
        delta_orient.make_identity();
        wind_foley_ts.set(int_rand_range(3000, 12000));
        for (int i = 0; i < num_nodes; i++)
        {
            simulated_node& node = nodes[i];
            node.ws_position = node_infos[i].pos;
            int bone = node_infos[i].bone_index;
            if (bone == -1 && node_infos[i].parent_node_index != -1)
            {
                bone = node_infos[node_infos[i].parent_node_index].bone_index;
            }
            cloth_node_to_world(node.ws_position, bone, bone_matrices, saved_obj_pos, *objp_orient);
            node.last_frame_ws_position = node.ws_position;
            node.collided = 0;
            node.anchor = 0;
        }
        for (int i = 0; i < num_node_links; i++)
        {
            const vector& a = nodes[link_infos[i].node_index_1].ws_position;
            const vector& b = nodes[link_infos[i].node_index_2].ws_position;
            node_links[i].last_frame_dir.set(a.x - b.x, a.y - b.y, a.z - b.z);
        }
        for (int i = 0; i < num_anchor_nodes; i++)
        {
            nodes[i].anchor = 1;
        }
        for (int i = 0; i < info->num_colliders; i++)
        {
            colliders[i].valid = false;
        }
        initialize_nodes = 0;
    }
    processed = 1;
    // Magic numbers: the share of the owner's motion the anchors take (more when on a turret).
    const float motion_mult = operating_turret ? 0.9f : 0.6f;
    for (int i = 0; i < num_anchor_nodes; i++)
    {
        vector& anchor = anchor_pos[i];
        anchor = node_infos[i].pos;
        cloth_node_to_world(anchor, node_infos[i].bone_index, bone_matrices, saved_obj_pos, *objp_orient);
        if (inherite_obj_motion)
        {
            vector& ws = nodes[i].ws_position;
            ws.x = ws.x - last_frame_pos.x;
            ws.y = ws.y - last_frame_pos.y;
            ws.z = ws.z - last_frame_pos.z;
            ws.unrotate(delta_orient);
            ws.x = ws.x + (last_frame_pos.x + obj_pos_diff.x * motion_mult);
            ws.y = ws.y + (last_frame_pos.y + obj_pos_diff.y * motion_mult);
            ws.z = ws.z + (last_frame_pos.z + obj_pos_diff.z * motion_mult);
        }
    }
    if (!inherite_obj_motion)
    {
        for (int i = num_anchor_nodes; i < num_nodes; i++)
        {
            sim_pos[i] = nodes[i].ws_position;
        }
    }
    else
    {
        for (int i = num_anchor_nodes; i < num_nodes; i++)
        {
            simulated_node& node = nodes[i];
            vector& sim = sim_pos[i];
            sim.set(node.ws_position.x - last_frame_pos.x, node.ws_position.y - last_frame_pos.y, node.ws_position.z - last_frame_pos.z);
            sim.unrotate(delta_orient);
            sim.x = (last_frame_pos.x + obj_pos_diff.x * motion_mult) + sim.x;
            sim.y = (last_frame_pos.y + obj_pos_diff.y * motion_mult) + sim.y;
            sim.z = (last_frame_pos.z + obj_pos_diff.z * motion_mult) + sim.z;
            node.last_frame_ws_position.x = (sim.x - node.ws_position.x) + node.last_frame_ws_position.x;
            node.last_frame_ws_position.y = (sim.y - node.ws_position.y) + node.last_frame_ws_position.y;
            node.last_frame_ws_position.z = (sim.z - node.ws_position.z) + node.last_frame_ws_position.z;
            node.ws_position = sim;
        }
        const float keep = (float)(1.0 - (double)motion_mult);
        obj_pos_diff.x = keep * obj_pos_diff.x;
        obj_pos_diff.y = obj_pos_diff.y * keep;
        obj_pos_diff.z = obj_pos_diff.z * keep;
    }
    last_frame_pos = root_pos;
    for (int c = 0; c < info->num_colliders; c++)
    {
        const cloth_sim_collision_primitive_info& ci = collider_infos[c];
        vector pos = ci.pos;
        // Magic number: an axis within 0.1 of horizontal builds the frame from world up z, else from x.
        vector up;
        if ((double)fabs((double)ci.axis.y) >= (double)0.1f)
        {
            up.set(0.0f, 0.0f, 1.0f);
        }
        else
        {
            up.set(1.0f, 0.0f, 0.0f);
        }
        matrix orient;
        orient.make_uvec(up, ci.axis);
        if (ci.bone_index == -1)
        {
            pos.unrotate(*objp_orient);
            pos.x = pos.x + saved_obj_pos.x;
            pos.y = pos.y + saved_obj_pos.y;
            pos.z = pos.z + saved_obj_pos.z;
            orient = orient * *objp_orient;
            orient.orthogonalize();
        }
        else if (collider_matrices != nullptr)
        {
            const matrix43& cm = collider_matrices[ci.bone_index];
            pos.unrotate(cm.m_rotation);
            pos.x = pos.x + cm.m_translation.x;
            pos.y = cm.m_translation.y + pos.y;
            pos.z = cm.m_translation.z + pos.z;
            pos.unrotate(*collider_ws_orient);
            pos.x = collider_pos.x + pos.x;
            pos.y = collider_pos.y + pos.y;
            pos.z = collider_pos.z + pos.z;
            orient = (orient * cm.m_rotation) * *collider_ws_orient;
            orient.orthogonalize();
        }
        // The middle of the capsule.
        const float height = ci.height;
        pos.y = (float)((double)(orient.uvec.y * height) * 0.5) + pos.y;
        pos.x = (float)((double)(orient.uvec.x * height) * 0.5) + pos.x;
        pos.z = (float)((double)(orient.uvec.z * height) * 0.5) + pos.z;
        collider_desired_pos[c] = pos;
        collider_desired_orient[c] = orient;
        if (info->num_passes > 1)
        {
            collider_desired_qorient[c].set(orient);
            collider_init_qorient[c].set(colliders[c].valid ? colliders[c].last_frame_orient : orient);
        }
        if (Cloth_debug_colliders)
        {
            // The collider debug draw (stripped from retail) built its frame here.
            matrix debug_orient;
            debug_orient.make(orient.uvec);
        }
    }
    float wind_mph;
    float wind_dir_heading;
    weather_wind_get_speed(&wind_mph, &wind_dir_heading);
    wind_dir.set(0.0f, 0.0f, 1.0f);
    if (wind_recalc_ts.elapsed())
    {
        do_explosion_gust = 0;
        if (!(wind_mph < 0.0f))
        {
            // Magic numbers: one of five gust kinds (four when always windy), each with its duration in ms.
            switch (int_rand_range(1, (Always_windy == false) + 4))
            {
            case 1:
            case 2:
                wind_recalc_ts.set(int_rand_range(300, 700));
                wind_strength_offset = fl_rand_range((float)((double)wind_mph * 0.5), wind_mph);
                wind_heading_offset = fl_rand_range(-0.1f, 0.1f) + wind_heading_offset;
                break;
            case 3:
                wind_recalc_ts.set(int_rand_range(800, 1500));
                wind_strength_offset = fl_rand_range(wind_mph, (float)((double)wind_mph * 1.5));
                wind_heading_offset = fl_rand_range(-0.1f, 0.1f) + wind_heading_offset;
                break;
            case 4:
                wind_recalc_ts.set(int_rand_range(300, 700));
                wind_heading_offset = fl_rand_range(-0.7f, 0.7f) + wind_dir_heading;
                break;
            case 5:
            {
                wind_recalc_ts.set(int_rand_range(500, 3000));
                float strength = info->wind_const;
                if (0.0f <= strength - wind_mph)
                {
                    strength = wind_mph;
                }
                wind_strength_offset = strength;
                wind_heading_offset = fl_rand_range(-0.1f, 0.1f) + wind_heading_offset;
                break;
            }
            default:
                break;
            }
        }
    }
    // Magic numbers: the flapping sound of an item's cloth plays in a strong wind (50 mph) within 10 m of the local player
    // (or the camera), every 3 to 5 s.
    if (objp != nullptr && wind_foley_ts.elapsed() && 50.0 < (double)wind_strength_offset && objp->get_obj_type() == OT_ITEM && ((item*)objp)->info->cloth_foley != -1)
    {
        vector listener = Rfg_camera.real_pos;
        player* local = player_get_local_player();
        if (local != nullptr)
        {
            listener = local->pos;
        }
        const float dx = objp->pos.x - listener.x;
        const float dy = objp->pos.y - listener.y;
        const float dz = objp->pos.z - listener.z;
        if ((double)(float)((double)dx * dx + (double)dy * dy + (double)dz * dz) < 100.0)
        {
            wind_foley_ts.set(int_rand_range(3000, 5000));
            audiolib_cue_play_3d(((item*)objp)->info->cloth_foley, &objp->pos, nullptr, nullptr, nullptr);
        }
    }
    wind_dir.rotate_around_y_axis(cur_wind_heading);
    rand_wind_mult = fl_rand_range(0.0f, 1.0f);
    saved_bone_matrices = bone_matrices;
    frametime = Frametime;
    saved_collider_matrices = collider_matrices;
    cloth_sim_setup_job(); // inlined
}

// 0x008040B0 (:573)
// The simulated nodes back into the owner's bones: each node's bone (or its parent's) turned so it points at the node
// from its parent, and moved onto the node; the bones touched revalidated in the animation cache.
void cloth_simulation::post_simulate(matrix43* bone_matrices)
{
    if (!processed)
    {
        return;
    }
    const int num_anchor_nodes = info->num_anchor_nodes;
    const int num_nodes = info->num_simulated_nodes + num_anchor_nodes;
    const simulated_node_info* node_infos = info->nodes.to_pointer();
    // Magic number: up to 300 bones touched (two per node).
    int bone_list[300];
    int num_bones = 0;
    for (int i = num_anchor_nodes; i < num_nodes; i++)
    {
        vector& sim = sim_pos[i];
        nodes[i].ws_position = sim;
        sim.x = sim.x - saved_obj_pos.x;
        sim.y = sim.y - saved_obj_pos.y;
        sim.z = sim.z - saved_obj_pos.z;
        sim.rotate(*objp_orient);
        const int bone = node_infos[i].bone_index;
        const int parent_node = node_infos[i].parent_node_index;
        // Original: read for a node without a parent too (node_infos[-1]).
        const int parent_bone = node_infos[parent_node].bone_index;
        matrix43& m = bone_matrices[parent_bone];
        vector bind_dir;
        if (bone == -1)
        {
            bind_dir = node_infos[i].pos;
            bind_dir.unrotate(m.m_rotation);
        }
        else
        {
            bind_dir.set(bone_matrices[bone].m_translation.x - m.m_translation.x, bone_matrices[bone].m_translation.y - m.m_translation.y, bone_matrices[bone].m_translation.z - m.m_translation.z);
        }
        vector cur_dir;
        if (nodes[parent_node].anchor == 0)
        {
            cur_dir = sim_pos[parent_node];
        }
        else
        {
            cur_dir = m.m_translation;
        }
        cur_dir.y = sim.y - cur_dir.y;
        cur_dir.x = sim.x - cur_dir.x;
        cur_dir.z = sim.z - cur_dir.z;
        bind_dir.normalize_safe();
        cur_dir.normalize_safe();
        float dot = (float)((double)bind_dir.y * cur_dir.y + (double)bind_dir.x * cur_dir.x + (double)bind_dir.z * cur_dir.z);
        if ((float)((double)dot - -1.0) < 0.0f)
        {
            dot = -1.0f;
        }
        if ((float)((double)dot - 1.0) < 0.0f)
        {
            if (dot == -1.0f)
            {
                continue;
            }
        }
        else
        {
            dot = 1.0f;
        }
        const float angle = fl_acos(dot);
        vector axis;
        axis.x = (float)((double)bind_dir.y * cur_dir.z - (double)bind_dir.z * cur_dir.y);
        axis.y = (float)((double)bind_dir.z * cur_dir.x - (double)cur_dir.z * bind_dir.x);
        axis.z = (float)((double)cur_dir.y * bind_dir.x - (double)bind_dir.y * cur_dir.x);
        axis.normalize_safe();
        matrix rot;
        rot.set(axis, angle);
        m.m_rotation = m.m_rotation * rot;
        bone_list[num_bones++] = parent_bone;
        if (bone != -1)
        {
            bone_list[num_bones++] = bone;
        }
    }
    for (int i = num_anchor_nodes; i < num_nodes; i++)
    {
        if (node_infos[i].bone_index != -1)
        {
            bone_matrices[node_infos[i].bone_index].m_translation = sim_pos[i];
        }
    }
    animlib_validate_cache_bone_list(aim_handle, num_bones, bone_list, true, false);
    animlib_invalidate_cache_bone_list(aim_handle, num_bones, bone_list, false, true);
}

// 0x0080BD50 (:758)
// The cloth file's offsets into pointers (the node infos follow the header, then the links, then the colliders), and
// the file in the lists cloth_sim_create searches.
bool cloth_sim_load_cb(const char* filename, unsigned int srid, unsigned char* cpu_data, int cpu_data_size, unsigned char* gpu_data, int gpu_data_size)
{
    cloth_sim_info* sim_info = (cloth_sim_info*)cpu_data;
    sim_info->nodes.from_pointer((simulated_node_info*)(cpu_data + sizeof(cloth_sim_info)));
    sim_info->node_links.from_pointer((simulated_node_link_info*)(sim_info->nodes.to_pointer() + (sim_info->num_simulated_nodes + sim_info->num_anchor_nodes)));
    sim_info->colliders.from_pointer((cloth_sim_collision_primitive_info*)(sim_info->node_links.to_pointer() + sim_info->num_node_links));
    Cloth_simulation_infos[Num_cloth_simulation_infos] = sim_info;
    Cloth_simulation_srids[Num_cloth_simulation_infos] = srid;
    Cloth_simulation_data_sizes[Num_cloth_simulation_infos] = cpu_data_size;
    Num_cloth_simulation_infos++;
    return true;
}

// 0x007F25C0 (:778)
void cloth_sim_unload_cb(const char* filename, unsigned int srid)
{
}

// 0x00804640 (:784)
// An instance for the cloth file named like the mesh (without extension): the runtime arrays in one block of the
// runtime pool, 16-byte aligned (nodes, links, colliders, anchor positions, simulated positions, the collider desired
// positions, orientations and the two slerp keys). Null when there is no such file or no room.
cloth_simulation* cloth_sim_create(char* mesh_name, vector* objp_pos, matrix* objp_orient, int aim_handle)
{
    char name[28];
    cf_drop_ext(name, sizeof(name), mesh_name);
    int index = 0;
    if (Num_cloth_simulation_infos <= 0)
    {
        return nullptr;
    }
    while (v_stricmp(Cloth_simulation_infos[index]->name, name) != 0)
    {
        index++;
        if (Num_cloth_simulation_infos <= index)
        {
            return nullptr;
        }
    }
    cloth_sim_info* sim_info = Cloth_simulation_infos[index];
    if (sim_info == nullptr)
    {
        return nullptr;
    }
    cloth_simulation* cp = Pool_cloth_simulations.alloc(0);
    if (cp == nullptr)
    {
        return nullptr;
    }
    const int num_nodes = sim_info->num_anchor_nodes + sim_info->num_simulated_nodes;
    const int num_colliders = sim_info->num_colliders;
    const int nodes_size = num_nodes * (int)sizeof(simulated_node);
    const int links_size = sim_info->num_node_links * (int)sizeof(simulated_node_link);
    const int colliders_size = num_colliders * (int)sizeof(cloth_sim_collision_primitive);
    const int anchors_size = sim_info->num_anchor_nodes * (int)sizeof(vector);
    const int sim_size = num_nodes * (int)sizeof(vector);
    const int desired_pos_size = num_colliders * (int)sizeof(vector);
    const int desired_orient_size = num_colliders * (int)sizeof(matrix);
    const int qorient_size = num_colliders * (int)sizeof(quaternion);
    // 0x50 per collider: the primitive's desired position, orientation and two quaternions; 0x50 per node: the node and
    // its simulated position.
    cp->data_size = num_colliders * 0x50 + num_nodes * 0x50 + anchors_size + colliders_size + links_size;
    // Magic number: room to align the block to 16 bytes (and more).
    const unsigned int alloc_size = cp->data_size + 0x20;
    if (!Cloth_sim_runtime_mempool.can_alloc(alloc_size, 0))
    {
        cp->buffer = nullptr;
        return nullptr;
    }
    cp->buffer = Cloth_sim_runtime_mempool.alloc(alloc_size, 0);
    const unsigned int misalign = (unsigned int)cp->buffer & 0xF;
    unsigned char* p = (unsigned char*)cp->buffer + (misalign == 0 ? 0 : 0x10 - misalign);
    cp->nodes = (simulated_node*)p;
    p += nodes_size;
    cp->node_links = (simulated_node_link*)p;
    p += links_size;
    cp->colliders = (cloth_sim_collision_primitive*)p;
    p += colliders_size;
    cp->anchor_pos = (vector*)p;
    p += anchors_size;
    cp->sim_pos = (vector*)p;
    p += sim_size;
    cp->info_index = index;
    cp->collider_desired_pos = (vector*)p;
    p += desired_pos_size;
    cp->objp_orient = objp_orient;
    cp->collider_desired_orient = (matrix*)p;
    p += desired_orient_size;
    cp->collider_desired_qorient = (quaternion*)p;
    p += qorient_size;
    cp->collider_init_qorient = (quaternion*)p;
    cp->objp_pos = objp_pos;
    cp->info = sim_info;
    cp->aim_handle = aim_handle;
    cp->initialize_nodes = 1;
    cp->inherite_obj_motion = 0;
    cp->do_explosion_gust = 0;
    cp->wind_recalc_ts.set(0);
    cp->cur_wind_strength = 0.0f;
    cp->cur_wind_heading = 0.0f;
    cp->saved_obj_pos.zero();
    cp->saved_obj_orient.make_identity();
    return cp;
}

// 0x0080AA80 (:879)
void cloth_sim_delete(cloth_simulation* cp)
{
    if (cp->nodes != nullptr)
    {
        Cloth_sim_runtime_mempool.release_bytes(cp->buffer);
        cp->nodes = nullptr;
        cp->node_links = nullptr;
        cp->colliders = nullptr;
        cp->anchor_pos = nullptr;
        cp->sim_pos = nullptr;
        cp->collider_desired_pos = nullptr;
        cp->collider_desired_orient = nullptr;
        cp->collider_desired_qorient = nullptr;
        cp->collider_init_qorient = nullptr;
    }
    Pool_cloth_simulations.free(cp);
}

// 0x007F25D0 (:896)
bool cloth_sim_init()
{
    return Cloth_sim_init;
}

// 0x00804870 (:901)
void cloth_sim_system_init(int dlc_id)
{
    if (Cloth_sim_init)
    {
        return;
    }
    Cloth_sim_init = true;
    Num_cloth_simulation_srids = 0;
    Num_cloth_simulation_infos = 0;
    cloth_sim_preload_all_shared(&Common_mempool, &Cloth_sim_mempool, Use_packfiles, PLATFORM_PC, dlc_id);
}

// 0x007F25E0 (:919)
void cloth_sim_system_shutdown()
{
    if (!Cloth_sim_init)
    {
        return;
    }
    Cloth_sim_init = false;
    cloth_sim_unload_all(); // inlined
}

// 0x007F2620 (:932)
void cloth_sim_set_thread_ownership()
{
    Cloth_sim_runtime_mempool.set_thread_ownership();
}

// 0x008048B0 (:937)
// The cloth files reloaded for a DLC change: every container released and deleted, the pool cleared, and the preload
// of cloth_sim_preload_all_shared again (inlined) with the pools of cloth_sim_system_init.
void cloth_sim_dlc_reset(int dlc_id)
{
    Cloth_sim_mempool.set_thread_ownership();
    for (int i = 0; i < Num_cloth_simulation_srids; i++)
    {
        stream2_release(Cloth_simulation_srids[i]);
        stream2_container_delete(Cloth_simulation_srids[i]);
    }
    Cloth_sim_mempool.clear();
    Num_cloth_simulation_srids = 0;
    Num_cloth_simulation_infos = 0;
    cloth_sim_preload_all_shared(&Common_mempool, &Cloth_sim_mempool, Use_packfiles, PLATFORM_PC, dlc_id);
}

// 0x0080AAE0 (:955)
void cloth_sim_process_xbox_pc(cpu_job* lj, cpu_job_arg arg1, cpu_job_arg arg2)
{
    ((cloth_simulation*)arg1.pointer_val)->simulate();
}

// 0x0080AAF0 (:963)
// One Havok-type scheduler job per queued instance, all feeding one event trigger.
void cloth_sim_batch_process_start()
{
    Cloth_sim_scheduler_trigger = scheduler_get_event_trigger();
    cpu_job job;
    job.trigger = nullptr;
    job.flags = 0;
    job.scratch_buffer = nullptr;
    job.scratch_buffer_size = 0;
    job.thread_id = get_current_thread_id();
    cloth_simulation* cp = Cloth_sim_batch_jobs;
    while (cp != nullptr)
    {
        job.job_func = cloth_sim_process_xbox_pc;
        job.type = ccjt_havok;
        job.arg1.pointer_val = cp;
        scheduler_add_job(nullptr, &job, Cloth_sim_scheduler_trigger);
        cp = cp->next;
        if (cp == Cloth_sim_batch_jobs)
        {
            break;
        }
    }
}

// 0x00804AA0 (:1065)
// The batch's jobs waited for (this thread helps), then each instance's result in its bones (post_simulate inlined)
// and its skinning matrices updated; the batch emptied.
void cloth_sim_batch_process_end()
{
    scheduler_relinquish_cpu(Cloth_sim_scheduler_trigger, ccjt_any);
    cloth_simulation* cp = Cloth_sim_batch_jobs;
    while (cp != nullptr)
    {
        cp->post_simulate(cp->saved_bone_matrices);
        animlib_update_vertex_to_model_matrices(cp->aim_handle);
        cp = cp->next;
        if (cp == Cloth_sim_batch_jobs)
        {
            break;
        }
    }
    Cloth_sim_batch_jobs = nullptr;
    Num_cloth_sim_jobs = 0;
}

// 0x007F2630 (:1091)
void cloth_sim_set_always_windy(bool always_windy)
{
    Always_windy = always_windy;
}
