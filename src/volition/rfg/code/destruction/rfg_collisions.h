// Original: volition/rfg/code/destruction/rfg_collisions.h
// Implementation: volition/rfg/code/destruction/rfg_collisions.cpp
//
// Material effect candidates: collisions, explosions and breaks request a material effect (rfg_effect_candidate_collide
// and _create); candidates are pooled, filtered by distance and visibility, prioritized (a low priority candidate is
// dropped when the pool is full) and turned into effects after a delay by rfg_effect_candidate_post_frame.
#pragma once

#include "volition/rfg/code/destruction/rfg_materials.h"
#include "volition/vlib/code/math/matrix.h"
#include "volition/vlib/code/math/vector3.h"
#include "volition/vlib/code/time/timestamp.h"

struct collision_event;
struct explosion_info;
class object;

// size 0x68
struct effect_candidate
{
    bool do_frame(); // 0x0072BB70 (:684)
    void update_pos_and_radius(); // 0x0071B440 (:787)
    void render_debug(); // 0x00711340 (:816)
    effect_candidate(); // 0x00719770

    material_event_type event_type; // 0x00
    material_event_category event_cat; // 0x04
    material_event_size event_size; // 0x08
    vector pos; // 0x0C
    matrix orient; // 0x18
    union
    {
        unsigned int info_handle; // 0x3C
        explosion_info* exp_info; // 0x3C
    };
    float effect_volume; // 0x40
    rfg_effect_material* effect_material_p; // 0x44
    timestamp delay; // 0x48
    float duration; // 0x4C
    float radius; // 0x50
    float energy; // 0x54
    unsigned int owner_handle; // 0x58
    unsigned int shape_key; // 0x5C
    unsigned int effect_handle; // 0x60
    unsigned char flags; // 0x64
    unsigned char padding[3]; // 0x65
};

void rfg_effect_candidate_level_init_pre(); // 0x00732370 (:641)
void rfg_effect_candidate_post_frame(); // 0x007323D0 (:653)
material_event_size rfg_effect_candidate_get_size(unsigned char mat_index, material_event_type event_type, float input); // 0x0071B540 (:831)
bool rfg_effect_candidate_create(unsigned char material_index, const vector& pos, material_event_type type, material_event_size size, material_shape_type shape_type, bool priority, unsigned int delay, const matrix* orient, object* owner, unsigned int key, unsigned char other_mat, unsigned char melee_group_index); // 0x0072BDF0 (:850)
bool rfg_effect_candidate_collide(const collision_event* coll_event); // 0x0072BEF0 (:870)
void rfg_effect_candidate_render_debug(); // 0x00711480 (:1044)
