// Original: volition/rfg/code/objects/movers/mover_fader.h
// Implementation: volition/rfg/code/objects/movers/mover_fader.cpp
//
// Mover faders: render pieces fading in or out (shards deleted, pieces regrown or repaired) after the mover's own
// instance stops drawing them. A fader (TU-private, Fader_pool of 255, keyed by owner and uid) gets its own render
// instance, moves the decals over and animates from its transform to the target.
#pragma once

#include "volition/vlib/code/math/matrix.h"
#include "volition/vlib/code/math/vector3.h"

struct mover_fader;
class rl_material_map;
class rl_mesh;

// size 0x84
class mover_fader_info
{
public:
    mover_fader_info(); // 0x007164B0 (.h:39)

    rl_mesh* mesh_p; // 0x00
    const rl_material_map* material_map_p; // 0x04
    unsigned int submesh_index; // 0x08
    float fade_duration; // 0x0C
    vector pos; // 0x10
    matrix orient; // 0x1C
    vector target_pos; // 0x40
    matrix target_orient; // 0x4C
    int material_effect_id; // 0x70
    unsigned int mat_fx_flags; // 0x74
    unsigned int flags; // 0x78
    void (*process_done_cb)(unsigned int, unsigned int); // 0x7C
    void (*update_render_done_cb)(unsigned int, unsigned int); // 0x80
};

void calculate_animated_transform(mover_fader* fader_p, matrix& out_orient, vector& out_pos); // 0x00A13410 (:209)
void mover_fader_system_init(); // 0x00AA2320 (:234)
void mover_fader_level_shutdown(); // 0x00A8B510 (:250)
void mover_fader_system_shutdown(); // 0x00AA23D0 (:268)
void mover_fader_process(); // 0x00A8B610 (:284)
unsigned int mover_fader_create(unsigned int owner, unsigned int uid, mover_fader_info* c_info); // 0x00A8B8C0 (:327)
void mover_fader_destroy(unsigned int owner, unsigned int uid); // 0x00A8BBD0 (:381)
void mover_fader_destroy_all(unsigned int owner); // 0x00A5EFD0 (:403)
void mover_fader_get_current_transform(unsigned int owner, unsigned int uid, matrix& rot, vector& pos); // 0x00A5F020 (:415)

// 0x007164B0 (.h:39)
// The targets are left uninitialized; only the repair fades (flags 0x31) set them.
inline mover_fader_info::mover_fader_info()
{
    mesh_p = nullptr;
    material_map_p = nullptr;
    submesh_index = 0;
    fade_duration = 1.0f;
    pos = ZERO_VECTOR;
    orient = IDENTITY_MATRIX;
    flags = 0;
    process_done_cb = nullptr;
    update_render_done_cb = nullptr;
    material_effect_id = -1;
    mat_fx_flags = 0;
}
