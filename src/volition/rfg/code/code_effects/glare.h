// Original: volition/rfg/code/code_effects/glare.h
// Implementation: volition/rfg/code/code_effects/glare.cpp
//
// Glares (light flares and coronas on lights, muzzle flashes): the glare table (Glare_info, up to 64 types), glares
// created at a position or attached to an object tag, culled by radius and checked for line of sight before drawing.
#pragma once

#include "volition/vlib/code/math/vector3.h"
#include "volition/vlib/code/util/color.h"

class object;

// size 0x5C
struct glare_info
{
    glare_info(); // 0x006E2290

    char name[32]; // 0x00
    color light_color; // 0x20
    int glare_bitmap; // 0x24
    float fixed_radius; // 0x28
    float max_radius; // 0x2C
    float radius_scale_factor; // 0x30
    float radius_dist_factor; // 0x34
    float diminish_distance; // 0x38
    int volumetric_bitmap; // 0x3C
    float volumetric_height; // 0x40
    float volumetric_length; // 0x44
    float volumetric_intensity; // 0x48
    float angle; // 0x4C
    float intensity; // 0x50
    bool view_corona_from_rear; // 0x54
    int uid; // 0x58
};

// size 0x48
class glare
{
public:
    glare(); // 0x006E2280

    glare* next; // 0x00
    glare* prev; // 0x04
    glare* next_single[1]; // 0x08
    vector pos; // 0x0C
    vector fvec; // 0x18
    float last_rendered_intensity; // 0x24
    float last_rendered_radius; // 0x28
    glare_info* info; // 0x2C
    color light_color; // 0x30
    float zbuf_radius; // 0x34
    float cull_radius; // 0x38
    object* parent; // 0x3C
    int prop_point_handle; // 0x40
    int flags; // 0x44
};

extern glare_info Glare_info[64]; // 0x01E1A770
extern int Num_glare_types; // 0x01DEE498
extern glare Glare_info_list; // 0x01E25330

void unused_dcf_glares_toggle(); // 0x006E22A0 (:72)
void unused_dcf_glare_free_all(); // 0x006E8750 (:86)
float glare_calculate_cull_radius(int info_index); // 0x006E2E60 (:515)
void glare_system_init(); // 0x006E87D0 (:536)
void glare_level_init_pre(); // 0x006E2ED0 (:551)
void glare_parse_table(); // 0x006E2EE0 (:556)
glare* glare_create(const vector& pos, const vector& dir, int info_index, bool ignore_los, bool no_screen_tint, float zbuf_radius); // 0x006E3180 (:607)
glare* glare_create_attached(object* parent, int prop_point_handle, int info_index, float zbuf_radius); // 0x006E3280 (:649)
glare* glare_create_muzzle_flash(const vector& pos, const vector& dir, int info_index, bool cull); // 0x006E3370 (:687)
void glare_delete(glare* gp); // 0x006E34E0 (:703)
void glare_change_type_attached(object* parent, int new_info_index); // 0x006E34F0 (:710)
int glare_info_lookup(const char* name); // 0x006E3540 (:728)
void glare_render_all(); // 0x006E3580 (:748)
void glare_turn_on(glare* gp); // 0x006E3680 (:800)
void glare_turn_off(glare* gp); // 0x006E3690 (:805)
