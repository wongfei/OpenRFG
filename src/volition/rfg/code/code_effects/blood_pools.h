// Original: volition/rfg/code/code_effects/blood_pools.h
// Implementation: volition/rfg/code/code_effects/blood_pools.cpp
//
// Blood pools: a pool grows under a dead human from a fixed buffer of pools (24) and faces (148), each face a decal-like
// quad projected onto the ground, kept in used and free lists.
#pragma once

#include "volition/vlib/code/math/vector3.h"
#include "volition/vlib/code/time/timestamp.h"

// size 0x44
struct blood_pool_face
{
    blood_pool_face(); // 0x006E1420

    vector p[3]; // 0x00
    float uv[3][2]; // 0x24
    blood_pool_face* next; // 0x3C
    blood_pool_face* prev; // 0x40
};

// size 0x24
struct blood_pool
{
    blood_pool(); // 0x006E1430

    blood_pool_face* faces; // 0x00
    float radius; // 0x04
    int bitmap; // 0x08
    timestamp blood_stamp; // 0x0C
    timestamp blood_lifetime; // 0x10
    timestamp blood_fade_out_stamp; // 0x14
    timestamp blood_fade_in_stamp; // 0x18
    blood_pool* next; // 0x1C
    blood_pool* prev; // 0x20
};

extern int Blood_pool_default_bitmap; // 0x01648E94
extern blood_pool Blood_pool_buf[24]; // 0x01E253C0
extern blood_pool_face Blood_pool_face_buf[148]; // 0x01E22448
extern blood_pool* Blood_pool_used_list; // 0x01DE6CD4
extern blood_pool* Blood_pool_free_list; // 0x01DEE3F4
extern blood_pool_face* Blood_pool_face_free_list; // 0x01DE6CD0

void blood_pool_system_init(); // 0x006E1450 (:127)
void blood_pool_level_init(); // 0x006E1480 (:137)
void blood_pool_level_shutdown(); // 0x006E1750 (:153)
void blood_pool_render_frame(); // 0x006E1770 (:161)
int blood_pool_create(int bitmap, vector& ground_pt, float radius); // 0x006E1780 (:244)
void unused_dcf_blood_pool(); // 0x006E1790 (:384)
