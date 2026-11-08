// Original: volition/rfg/code/code_effects/tracers.h
// Implementation: volition/rfg/code/code_effects/tracers.cpp
//
// Bullet tracers: straight and curved tracers (rail, sniper, nano and enforcer variants) from a fixed buffer of 64 kept in
// used and free lists, drawn as particle effects. Placement of weapon_tracer_type inferred (weapon_info holds it).
#pragma once

#include "volition/vlib/code/math/spline.h"
#include "volition/vlib/code/math/vector3.h"
#include "volition/vlib/code/time/timestamp.h"

class vfx_particle_parametric;

enum weapon_tracer_type
{
    WEAPON_TRACER_DEFAULT = 0,
    WEAPON_TRACER_RAIL = 1,
    WEAPON_TRACER_NANO = 2,
    WEAPON_TRACER_SNIPER = 3,
    WEAPON_TRACER_ENFORCER = 4,
    NUM_WEAPON_TRACER_TYPES = 5,
};

// size 0x174
struct tracer
{
    bool is_curved() const; // 0x006E4BA0 (:61)
    bool is_rail() const; // 0x006E4BB0 (:62)
    bool is_sniper() const; // 0x006E4BC0 (:63)
    bool is_nano() const;
    tracer(); // 0x006E4BD0

    vector start_pos; // 0x00
    catmull_rom_spline path_spline; // 0x0C
    vector spline_points[6]; // 0x14
    vector end_pos; // 0x5C
    vector fvec; // 0x68
    vector pos; // 0x74
    float duration; // 0x80
    weapon_tracer_type tracer_type; // 0x84
    timestamp age_timer; // 0x88
    unsigned int owner; // 0x8C
    unsigned int tracer_effect; // 0x90
    bool destroy_next_frame; // 0x94
    bool one_frame_delay; // 0x95
    vfx_particle_parametric* tracer_particle_start_p; // 0x98
    vfx_particle_parametric* tracer_particle_end_p; // 0x9C
    vfx_particle_parametric* trail_particle_p; // 0xA0
    vfx_particle_parametric* trail_particles[50]; // 0xA4
    tracer* next; // 0x16C
    tracer* prev; // 0x170
};

extern tracer Tracer_buf[64]; // 0x01E14310
extern tracer* Tracer_used_list; // 0x01DEB6E8
extern tracer* Tracer_free_list; // 0x01DEE424
extern unsigned int Tracer_effect_handle; // 0x01DEE3F0
extern unsigned int Curved_tracer_effect_handle; // 0x01DEB6EC

void tracers_system_init(); // 0x006E4BF0 (:84)
void tracers_level_init_pre(); // 0x006E4C20 (:90)
void tracers_create(unsigned int owner, const vector& start_pos, const vector& end_pos, unsigned int tracer_effect, weapon_tracer_type w_tracer_type); // 0x006EDBE0 (:102)
void curved_tracers_create(unsigned int owner, const vector& start_pos, const vector& inner_spline_pos, const vector& inner_spline_pos2, const vector& end_pos, unsigned int tracer_effect, weapon_tracer_type w_tracer_type); // 0x006EE0E0 (:263)
void tracers_process(); // 0x006E9D10 (:446)
void tracers_render(); // 0x006E4D80 (:533)
