// Original: volition/rfg/code/geom/cloth_simulation_shared_spu.cpp
//
// The cloth solver (stub_impl.md S3), written to run as a PS3 SPU job and run here as a scheduler job
// (cloth_sim_process_xbox_pc): Verlet integration of the simulated nodes with air resistance, wind and gravity, the
// owner's motion and the anchors interpolated over the passes, collisions of the nodes and of the links against the
// collider capsules, and the links' length constraints.
//
// Floating point: rfg.exe evaluates each scalar statement in double and rounds on assignment; vector operations round
// per operation (written here as float vector code). The double steps are taken from the asm; not verified bit-exact
// (no hkdiff case).
//
// Notes (decompiled behaviour, kept as is):
// - The wind strength moves toward wind_strength_offset in the direction given by comparing wind_heading_offset (not
//   the strength offset) with the current strength.
// - The per-node velocities live in a stack array of 150 entries; nothing checks the node count against it.
// - A non-capsule collider (is_capsule 0) only collides with nodes whose closest point lies inside its segment.

#include "volition/rfg/code/geom/cloth_simulation_shared_spu.h"

#include "volition/rfg/code/geom/cloth_simulation.h"
#include "volition/rfg/code/geom/cloth_simulation_shared.h"
#include "volition/vlib/code/math/matrix.h"
#include "volition/vlib/code/math/quaternion.h"

#include <math.h>

// Tuning values (file statics in .data; the debug menus of the SPU code edited them).
static float multiplier = 0.75f; // 0x0165F874, the share of an upward velocity kept
static float tmp = 0.65f; // 0x0165F878, the share of its own velocity a collided node keeps (the rest is the owner's)
static float blah = 5.0f; // 0x0165F87C, metres per second of owner motion the cloth absorbs before the air drags less
// Two statics named test_val (0 in .bss): a collision is skipped when the node lies on the far side of the collider
// in its local (bind) space and half_collisions is set.
static float test_val_link; // 0x0261F908
static float test_val_node; // 0x0261F90C

// Not in the binary: a vector's length as vector::len inlines it (the squared length rounded, the root in double).
static float cs_len(const vector& v)
{
    return (float)sqrt((double)(float)((double)v.x * v.x + (double)v.y * v.y + (double)v.z * v.z));
}

// Not in the binary: a dot product as the inlined operator computes it.
static float cs_dot(const vector& a, const vector& b)
{
    return (float)((double)a.x * b.x + (double)a.y * b.y + (double)a.z * b.z);
}

// Not in the binary: a fraction clamped to [0, 1] the way simulate compares (on the rounded differences).
static float cs_clamp01(float v)
{
    if ((float)((double)v - 0.0) < 0.0f)
    {
        v = 0.0f;
    }
    if (!((float)((double)v - 1.0) < 0.0f))
    {
        v = 1.0f;
    }
    return v;
}

// 0x007F2E90
cs_collision::cs_collision()
{
}

// 0x007F2720 (:36)
// The point of the segment p0-p1 closest to pnt in ix; returns its distance from p0 (0 for a zero-length segment).
float cs_closest_point_on_line_segment(vector& ix, const vector& pnt, const vector& p0, const vector& p1)
{
    const vector dir(p1.x - p0.x, p1.y - p0.y, p1.z - p0.z);
    const float len = cs_len(dir);
    if (len == 0.0f)
    {
        ix = p0;
        return 0.0f;
    }
    const vector n(dir.x / len, dir.y / len, dir.z / len);
    const vector d(pnt.x - p0.x, pnt.y - p0.y, pnt.z - p0.z);
    float t = (float)((double)d.y * n.y + (double)d.x * n.x + (double)d.z * n.z);
    if ((float)((double)t - 0.0) < 0.0f)
    {
        t = 0.0f;
    }
    const float dist = (float)((double)t - (double)len) < 0.0f ? t : len;
    ix.x = p0.x + n.x * dist;
    ix.y = p0.y + n.y * dist;
    ix.z = p0.z + n.z * dist;
    return dist;
}

// 0x007F29A0 (:67)
// The closest points of the lines p1-p2 and p3-p4 (fractions along each, unclamped); false when a line is shorter than
// 0.0001 on every axis or the lines are parallel.
bool cs_closest_line_line(cs_collision& col_a, cs_collision& col_b, const vector& p1, const vector& p2, const vector& p3, const vector& p4)
{
    // Magic number: the degenerate threshold (0.0001f compared in double).
    const double eps = (double)0.0001f;
    const vector p13(p1.x - p3.x, p1.y - p3.y, p1.z - p3.z);
    const vector p43(p4.x - p3.x, p4.y - p3.y, p4.z - p3.z);
    if (eps > fabs((double)p43.x) && eps > fabs((double)p43.y) && eps > fabs((double)p43.z))
    {
        return false;
    }
    const vector p21(p2.x - p1.x, p2.y - p1.y, p2.z - p1.z);
    if (eps > fabs((double)p21.x) && eps > fabs((double)p21.y) && eps > fabs((double)p21.z))
    {
        return false;
    }
    const float d1343 = (float)((double)p43.x * p13.x + (double)p43.y * p13.y + (double)p43.z * p13.z);
    const float d4321 = (float)((double)p21.y * p43.y + (double)p21.x * p43.x + (double)p21.z * p43.z);
    const float d4343 = (float)((double)p43.y * p43.y + (double)p43.x * p43.x + (double)p43.z * p43.z);
    const float d2121 = (float)((double)p21.y * p21.y + (double)p21.x * p21.x + (double)p21.z * p21.z);
    const float denom = (float)((double)d2121 * d4343 - (double)d4321 * d4321);
    if (eps > fabs((double)denom))
    {
        return false;
    }
    const float d1321 = (float)((double)p21.y * p13.y + (double)p21.x * p13.x + (double)p21.z * p13.z);
    const float numer = (float)((double)d1343 * d4321 - (double)d1321 * d4343);
    col_a.fraction = (float)((double)numer / (double)denom);
    col_b.fraction = (float)(((double)col_a.fraction * d4321 + (double)d1343) / (double)d4343);
    const float mua = col_a.fraction;
    col_a.hit_point.x = p1.x + p21.x * mua;
    col_a.hit_point.y = p21.y * mua + p1.y;
    col_a.hit_point.z = p1.z + p21.z * mua;
    const float mub = col_b.fraction;
    col_b.hit_point.x = p3.x + mub * p43.x;
    col_b.hit_point.y = mub * p43.y + p3.y;
    col_b.hit_point.z = p3.z + p43.z * mub;
    return true;
}

// 0x00805090 (:111)
// One frame of the cloth (pre_simulate prepared it): the velocities (Verlet, air resistance raised while the owner
// moves fast, wind), then per pass the anchors and colliders interpolated toward this frame, the nodes integrated with
// the owner's motion and gravity, pushed out of each collider capsule (nodes and links), and the links pulled toward
// their length (stretching resisted more than compressing, weighted toward collided or bending nodes). The colliders
// and anchors end at this frame's transforms.
void cloth_simulation::simulate()
{
    if (!processed)
    {
        return;
    }
    const cloth_sim_info* si = info;
    const simulated_node_info* node_infos = si->nodes.to_pointer();
    const simulated_node_link_info* link_infos = si->node_links.to_pointer();
    const cloth_sim_collision_primitive_info* collider_infos = si->colliders.to_pointer();
    const int num_links = si->num_node_links;
    const int num_anchors = si->num_anchor_nodes;
    const int num_nodes = si->num_simulated_nodes + num_anchors;
    const float pass_time = (float)((1.0 / (double)(float)si->num_passes) * (double)frametime);
    // The wind strength eases toward its target at 10 per second. Original: compared with the heading offset.
    cur_wind_heading = wind_heading_offset;
    float strength = cur_wind_strength;
    if ((double)wind_heading_offset > (double)strength)
    {
        strength = (float)((double)frametime * 10.0 + (double)strength);
        if (!((float)((double)strength - (double)wind_strength_offset) < 0.0f))
        {
            strength = wind_strength_offset;
        }
    }
    else
    {
        strength = (float)((double)strength - (double)frametime * 10.0);
        if ((float)((double)strength - (double)wind_strength_offset) < 0.0f)
        {
            strength = wind_strength_offset;
        }
    }
    cur_wind_strength = strength;
    if (do_explosion_gust)
    {
        wind_dir = explosion_gust;
        wind_dir.normalize_safe();
        wind_strength_offset = 250.0f; // magic number: an explosion's gust strength
    }
    // The air drags less (toward the owner's velocity inheritance) the faster the owner moved.
    float air = si->air_resistance;
    const float free_motion = (float)((double)frametime * (double)blah);
    const float diff_len_sq = (float)((double)obj_pos_diff.y * obj_pos_diff.y + (double)obj_pos_diff.x * obj_pos_diff.x + (double)obj_pos_diff.z * obj_pos_diff.z);
    if ((double)diff_len_sq > (double)free_motion * (double)free_motion)
    {
        float t = (float)(((double)(float)sqrt((double)diff_len_sq) - (double)free_motion) * 0.5 / (double)frametime);
        if (!((float)((double)t - 1.0) < 0.0f))
        {
            t = 1.0f;
        }
        air = (float)((double)(float)(1.0 - (double)si->object_velocity_inheritance) * (double)t + (1.0 - (double)t) * (double)si->air_resistance);
    }
    // Magic numbers: the wind force is a tenth of the gust strength, at least 0.001.
    float wind = (float)((double)si->wind_multiplier * (double)wind_strength_offset * (double)0.1f);
    if (!((float)((double)0.001f - (double)wind) < 0.0f))
    {
        wind = 0.001f;
    }
    wind = (float)((double)rand_wind_mult * (double)wind);
    // Magic number: 150 nodes at most (not checked).
    vector velocities[150];
    for (int i = num_anchors; i < num_nodes; i++)
    {
        simulated_node& node = nodes[i];
        vector v(node.ws_position.x - node.last_frame_ws_position.x, node.ws_position.y - node.last_frame_ws_position.y, node.ws_position.z - node.last_frame_ws_position.z);
        if (node.collided)
        {
            const vector own(v.x * tmp, v.y * tmp, v.z * tmp);
            const float other = (float)(1.0 - (double)tmp);
            v.set(other * obj_pos_diff.x + own.x, other * obj_pos_diff.y + own.y, other * obj_pos_diff.z + own.z);
        }
        const float passes = (float)si->num_passes;
        vector& vel = velocities[i];
        vel.set((v.x * air) / passes, (v.y * air) / passes, (v.z * air) / passes);
        if (0.0f < vel.y)
        {
            vel.y = vel.y * multiplier;
        }
        const float node_wind = node_infos[i].wind_multiplier;
        vel.x = (((wind_dir.x * wind) * pass_time) * node_wind) / passes + vel.x;
        vel.y = (((wind_dir.y * wind) * pass_time) * node_wind) / passes + vel.y;
        vel.z = (((wind_dir.z * wind) * pass_time) * node_wind) / passes + vel.z;
        node.last_frame_ws_position = node.ws_position;
    }
    // The owner's motion spread over the passes.
    const float inv_passes = (float)(1.0 / (double)(float)si->num_passes);
    obj_pos_diff.set(inv_passes * obj_pos_diff.x, inv_passes * obj_pos_diff.y, inv_passes * obj_pos_diff.z);
    const float inheritance = si->object_velocity_inheritance;
    obj_pos_diff.set(inheritance * obj_pos_diff.x, inheritance * obj_pos_diff.y, inheritance * obj_pos_diff.z);
    for (int pass = 0; pass < si->num_passes; pass++)
    {
        const float frac = (float)(((double)(float)pass + 1.0) / (double)(float)si->num_passes);
        const float rest = (float)(1.0 - (double)frac);
        for (int i = 0; i < num_anchors; i++)
        {
            const vector& a = anchor_pos[i];
            const vector& ws = nodes[i].ws_position;
            sim_pos[i].set(frac * a.x + rest * ws.x, a.y * frac + ws.y * rest, a.z * frac + ws.z * rest);
        }
        // Magic numbers: gravity 9.8 scaled by 0.0333 and halved (the float constants in double).
        const float gravity_y = (float)((double)(float)((double)(float)((double)pass_time * (double)9.8f) * (double)0.0333f) * 0.5);
        for (int i = num_anchors; i < num_nodes; i++)
        {
            simulated_node& node = nodes[i];
            vector& sim = sim_pos[i];
            node.collided = 0;
            sim.x = (sim.x - node.ws_position.x) + sim.x;
            sim.y = sim.y + (sim.y - node.ws_position.y);
            sim.z = (sim.z - node.ws_position.z) + sim.z;
            sim.x = velocities[i].x + sim.x;
            sim.y = velocities[i].y + sim.y;
            sim.z = sim.z + velocities[i].z;
            sim.x = sim.x + obj_pos_diff.x;
            sim.y = sim.y + obj_pos_diff.y;
            sim.z = sim.z + obj_pos_diff.z;
            node.ws_position = sim;
            const float gravity_mult = si->gravity_multiplier;
            sim.x = sim.x - 0.0f * gravity_mult;
            sim.y = sim.y - gravity_mult * gravity_y;
            sim.z = sim.z - gravity_mult * 0.0f;
        }
        for (int c = 0; c < si->num_colliders; c++)
        {
            const cloth_sim_collision_primitive_info& ci = collider_infos[c];
            const unsigned int mask = 1u << c;
            vector cpos;
            if (!inherite_obj_motion && colliders[c].valid)
            {
                const vector& desired = collider_desired_pos[c];
                const vector& last = colliders[c].last_frame_pos;
                cpos.set(desired.x * frac + rest * last.x, desired.y * frac + last.y * rest, desired.z * frac + last.z * rest);
            }
            else
            {
                cpos = collider_desired_pos[c];
            }
            matrix corient;
            if (si->num_passes == 1)
            {
                corient = collider_desired_orient[c];
            }
            else
            {
                quaternion q;
                pltind_slerp(q, collider_init_qorient[c], collider_desired_qorient[c], frac);
                q.extract_matrix(corient);
            }
            // The capsule's ends, and its segment in the bone's (bind) space.
            const vector top((float)((double)(ci.height * corient.uvec.x) * 0.5) + cpos.x, cpos.y + (float)((double)(ci.height * corient.uvec.y) * 0.5), cpos.z + (float)((double)(ci.height * corient.uvec.z) * 0.5));
            const vector bottom(cpos.x - (float)((double)(ci.height * corient.uvec.x) * 0.5), cpos.y - (float)((double)(corient.uvec.y * ci.height) * 0.5), cpos.z - (float)((double)(corient.uvec.z * ci.height) * 0.5));
            const vector local_start = ci.local_space_pos;
            const vector local_end(ci.axis.x * ci.height + local_start.x, local_start.y + ci.axis.y * ci.height, local_start.z + ci.axis.z * ci.height);
            for (int i = num_anchors; i < num_nodes; i++)
            {
                if ((node_infos[i].collide & mask) == 0)
                {
                    continue;
                }
                vector& sim = sim_pos[i];
                vector closest;
                const float along = cs_closest_point_on_line_segment(closest, sim, top, bottom); // inlined
                // Magic number: a segment-only collider ignores nodes within 0.002 of its far end.
                if (ci.is_capsule == 0 && !(0.0f < along && (double)ci.height - (double)0.002f > (double)along))
                {
                    continue;
                }
                const vector d(sim.x - closest.x, sim.y - closest.y, sim.z - closest.z);
                const float dist_sq = (float)((double)d.y * d.y + (double)d.x * d.x + (double)d.z * d.z);
                // Magic number: the nodes keep 0.03 off the capsule's surface.
                const float radius = (float)((double)ci.radius + (double)0.03f);
                if (!((double)dist_sq < (double)radius * (double)radius))
                {
                    continue;
                }
                if (half_collisions)
                {
                    vector local_closest;
                    cs_closest_point_on_line_segment(local_closest, node_infos[i].local_space_pos, local_start, local_end); // inlined
                    const vector& node_local = node_infos[i].local_space_pos;
                    vector local_normal(local_closest.x - node_local.x, local_closest.y - node_local.y, local_closest.z - node_local.z);
                    local_normal.normalize();
                    if (ci.bone_index != -1)
                    {
                        local_normal.unrotate(saved_collider_matrices[ci.bone_index].m_rotation);
                        local_normal.unrotate(saved_obj_orient);
                    }
                    vector dn = d;
                    dn.normalize();
                    if (test_val_node < cs_dot(dn, local_normal))
                    {
                        continue;
                    }
                }
                float len = 1.0f;
                vector n = d;
                if (0.0f < dist_sq)
                {
                    len = (float)sqrt((double)dist_sq);
                    const float inv = (float)(1.0 / (double)len);
                    n.set(inv * d.x, d.y * inv, d.z * inv);
                }
                nodes[i].collide_correction = (float)((double)radius - (double)len);
                nodes[i].collided = 1;
                sim.set(radius * n.x + closest.x, n.y * radius + closest.y, n.z * radius + closest.z);
            }
            for (int k = 0; k < num_links; k++)
            {
                const simulated_node_link_info& li = link_infos[k];
                if ((li.collide & mask) == 0)
                {
                    continue;
                }
                cs_collision col_a;
                cs_collision col_b;
                if (!cs_closest_line_line(col_a, col_b, top, bottom, sim_pos[li.node_index_1], sim_pos[li.node_index_2]))
                {
                    continue;
                }
                if (col_a.fraction < 0.0f)
                {
                    col_a.hit_point = top;
                }
                else if (1.0f < col_a.fraction)
                {
                    col_a.hit_point = bottom;
                }
                if (col_b.fraction < 0.0f)
                {
                    col_b.hit_point = sim_pos[li.node_index_1];
                }
                else if (1.0f < col_b.fraction)
                {
                    col_b.hit_point = sim_pos[li.node_index_2];
                }
                const vector d(col_b.hit_point.x - col_a.hit_point.x, col_b.hit_point.y - col_a.hit_point.y, col_b.hit_point.z - col_a.hit_point.z);
                const float radius = ci.radius;
                const float dist_sq = (float)((double)d.x * d.x + (double)d.y * d.y + (double)d.z * d.z);
                if (!((double)dist_sq < (double)radius * (double)radius))
                {
                    continue;
                }
                if (half_collisions)
                {
                    col_a.fraction = cs_clamp01(col_a.fraction);
                    col_b.fraction = cs_clamp01(col_b.fraction);
                    const float ra = (float)(1.0 - (double)col_a.fraction);
                    const float rb = (float)(1.0 - (double)col_b.fraction);
                    const vector& l1 = node_infos[li.node_index_1].local_space_pos;
                    const vector& l2 = node_infos[li.node_index_2].local_space_pos;
                    vector local_normal((ra * local_start.x + col_a.fraction * local_end.x) - (rb * l1.x + col_b.fraction * l2.x),
                                        (local_start.y * ra + local_end.y * col_a.fraction) - (l1.y * rb + l2.y * col_b.fraction),
                                        (local_start.z * ra + local_end.z * col_a.fraction) - (l1.z * rb + l2.z * col_b.fraction));
                    local_normal.normalize();
                    if (ci.bone_index != -1)
                    {
                        local_normal.unrotate(saved_collider_matrices[ci.bone_index].m_rotation);
                        local_normal.unrotate(saved_obj_orient);
                    }
                    vector dn = d;
                    if (!dn.is_null())
                    {
                        dn.normalize();
                    }
                    if (test_val_link < cs_dot(dn, local_normal))
                    {
                        continue;
                    }
                }
                vector n = d;
                n.normalize_safe();
                // The push goes to the link's ends by where it was hit; an anchored end does not move.
                float w = cs_clamp01(col_b.fraction);
                const int n1 = li.node_index_1;
                const int n2 = li.node_index_2;
                if (nodes[n1].anchor)
                {
                    w = 1.0f;
                }
                else if (nodes[n2].anchor)
                {
                    w = 0.0f;
                }
                const float push = (float)(((double)radius - (double)(float)sqrt((double)dist_sq)) * 2.0);
                const float w1 = 1.0f - w;
                const vector pn(n.x * push, n.y * push, n.z * push);
                sim_pos[n1].set(pn.x * w1 + sim_pos[n1].x, pn.y * w1 + sim_pos[n1].y, pn.z * w1 + sim_pos[n1].z);
                sim_pos[n2].set(pn.x * w + sim_pos[n2].x, pn.y * w + sim_pos[n2].y, pn.z * w + sim_pos[n2].z);
                nodes[n1].collide_correction = w1 * push;
                nodes[n2].collide_correction = w * push;
                nodes[n1].collided = 1;
                nodes[n2].collided = 1;
            }
        }
        for (int k = 0; k < num_links; k++)
        {
            const simulated_node_link_info& li = link_infos[k];
            const int n1 = li.node_index_1;
            const int n2 = li.node_index_2;
            const bool anchor1 = nodes[n1].anchor != 0;
            const bool anchor2 = nodes[n2].anchor != 0;
            const vector d(sim_pos[n2].x - sim_pos[n1].x, sim_pos[n2].y - sim_pos[n1].y, sim_pos[n2].z - sim_pos[n1].z);
            const float len = (float)sqrt((double)(float)((double)d.y * d.y + (double)d.x * d.x + (double)d.z * d.z));
            simulated_node_link& link = node_links[k];
            // Magic number: a link shorter than 0.0001 keeps last frame's direction.
            if ((double)len > (double)0.0001f)
            {
                link.last_frame_dir.set(d.x / len, d.y / len, d.z / len);
            }
            const float error = (float)((double)li.length - (double)len);
            float ratio = (float)((double)(float)fabs((double)error) / ((double)li.stretch_len * (double)li.length));
            ratio = cs_clamp01(ratio);
            if (0.0f < error)
            {
                // Magic numbers: a stretched link corrects a quarter (a fifth of that for a node's gravity link).
                ratio = (float)((double)ratio * 0.25);
                if (node_infos[n2].gravity_link == k)
                {
                    ratio = (float)((double)ratio * (double)0.2f);
                }
            }
            const float correction = (float)((double)ratio * (double)error);
            const vector dc(link.last_frame_dir.x * correction, link.last_frame_dir.y * correction, link.last_frame_dir.z * correction);
            link.error_correction = (float)((double)correction / (double)li.length);
            float w1 = 0.5f;
            float w2 = 0.5f;
            if (anchor1)
            {
                w1 = 0.0f;
                w2 = 1.0f;
            }
            else if (anchor2)
            {
                w1 = 1.0f;
                w2 = 0.0f;
            }
            else if (node_infos[n2].gravity_link == k)
            {
                // The more the chain bends at n1, the more n1 moves (up to half).
                const vector& other = node_links[node_infos[n1].gravity_link].last_frame_dir;
                const vector& cur = link.last_frame_dir;
                float bend = (float)(1.0 - (double)(float)((double)other.y * cur.y + (double)cur.x * other.x + (double)other.z * cur.z));
                if ((float)((double)bend - 0.0) < 0.0f)
                {
                    bend = 0.0f;
                }
                if ((float)((double)bend - 0.5) < 0.0f)
                {
                    w1 = bend;
                }
                w2 = (float)(1.0 - (double)w1);
            }
            else if (nodes[n1].collided || nodes[n2].collided)
            {
                // Magic number: 0.0003 keeps the weights finite; the more corrected node moves less.
                const float c1 = nodes[n1].collided ? nodes[n1].collide_correction : 0.0f;
                const float c2 = nodes[n2].collided ? nodes[n2].collide_correction : 0.0f;
                const float a = (float)((double)c1 + (double)0.0003f);
                const float b = (float)((double)c2 + (double)0.0003f);
                const double total = (double)a + (double)b;
                w1 = (float)((double)b / total);
                w2 = (float)((double)a / total);
            }
            sim_pos[n1].set(sim_pos[n1].x - dc.x * w1, sim_pos[n1].y - dc.y * w1, sim_pos[n1].z - dc.z * w1);
            sim_pos[n2].set(sim_pos[n2].x + dc.x * w2, dc.y * w2 + sim_pos[n2].y, dc.z * w2 + sim_pos[n2].z);
        }
    }
    for (int c = 0; c < si->num_colliders; c++)
    {
        colliders[c].valid = true;
        colliders[c].last_frame_pos = collider_desired_pos[c];
        colliders[c].last_frame_orient = collider_desired_orient[c];
    }
    for (int i = 0; i < num_anchors; i++)
    {
        nodes[i].ws_position = anchor_pos[i];
    }
}
