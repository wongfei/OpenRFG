// Original: volition/rfg/code/code_effects/material_effects.h
// Implementation: volition/rfg/code/code_effects/material_effects.cpp
//
// Material effects: timed material overrides on render instances (damage glow, reconstructor shards, nano rifle,
// cloaking). Applying one swaps the instance's material map for a copy from the map library (keyed by the original map
// and effect), finalized once its materials are loaded; the instance's tracker restores the original map when the
// effect ends or the instance dies.
#pragma once

#include "volition/render_lib/code/rl_callback_widget.h"
#include "volition/render_lib/code/rl_material_map.h"
#include "volition/vlib/code/math/vector3.h"

class material_effect;
struct material_effect_info;
struct material_map_library_entry;
struct ri_effects_tracker;
class rl_layered_material;
class rl_material_layer;
class rl_renderable_instance;

// size 0x24
class material_effect_parameters
{
public:
    material_effect_parameters(const material_effect_parameters& __that); // 0x006E4920
    material_effect_parameters(); // 0x006E1340 (.h:38)

    int lifetime_ms; // 0x00
    int time_passed_ms; // 0x04
    unsigned int flags; // 0x08
    void* cb_data; // 0x0C
    void (*update_render_done_cb)(void*, rl_renderable_instance*); // 0x10
    vector world_pos; // 0x14
    bool use_local_pos; // 0x20
    bool use_time_in_w; // 0x21
};

// size 0x10
class material_effect_ri_dead_cb : public rl_callback_widget
{
public:
    virtual void on_callback(); // vtbl+0x04 0x006F14E0 (:297)
    material_effect_ri_dead_cb(); // 0x006E4540
    virtual ~material_effect_ri_dead_cb(); // vtbl+0x00 0x006E4560

    material_effect* m_mat_effect; // 0x0C
};

// size 0x54
class material_effect
{
public:
    material_effect(); // 0x006E98E0 (:91)
    ~material_effect();

    unsigned int handle; // 0x00
    const material_effect_info* info; // 0x04
    int birthtime_ms; // 0x08
    float manual_time_pct; // 0x0C
    material_effect_parameters parameters; // 0x10
    rl_renderable_instance* ri_p; // 0x34
    material_map_library_entry* mat_info; // 0x38
    material_effect_ri_dead_cb on_ri_dead_cb; // 0x3C
    material_effect* prev; // 0x4C
    material_effect* next; // 0x50
};

// size 0x34
struct material_map_library_entry
{
    material_map_library_entry(); // 0x006E5E30

    const rl_material_map* orig_mat_map_p; // 0x00
    int effect_id; // 0x04
    rl_material_map* mat_map_p; // 0x08
    int ref_count; // 0x0C
    rl_material_map_data_runtime mat_map_data; // 0x10
    unsigned int* mat_map_handle_array; // 0x18
    int num_materials; // 0x1C
    rl_layered_material** materials; // 0x20
    rl_material_layer* material_layers; // 0x24
    bool materials_registered; // 0x28
    material_map_library_entry* next; // 0x2C
    material_map_library_entry* prev; // 0x30
};

extern material_map_library_entry* Finalize_map_list; // 0x01DEE4BC

void unused_dcf_test_matfx_on_player(); // 0x006F61A0 (:161)
ri_effects_tracker* get_ri_tracker(rl_renderable_instance* r_inst, material_effect* new_effect); // 0x006F1360 (:250)
void release_ri_tracker(rl_renderable_instance* r_inst, material_effect* effect); // 0x006F1420 (:272)
void maybe_destroy_map_library_entry(material_map_library_entry* lib_entry_p); // 0x006F1520 (:305)
void finalize_materials(material_map_library_entry* lib_entry_p); // 0x006E45C0 (:407)
material_map_library_entry* find_or_create_map_library_entry(int effect_id, const rl_material_map* orig_mat_map); // 0x006F19F0 (:511)
void material_effects_system_init(); // 0x006F6300 (:674)
void material_effects_system_shutdown(); // 0x006ED9D0 (:719)
unsigned int material_effects_apply(rl_renderable_instance* r_inst, const char* effect_name, const material_effect_parameters* parameters); // 0x006F63E0 (:729)
unsigned int material_effects_apply(rl_renderable_instance* r_inst, int effect_id, const material_effect_parameters* parameters); // 0x006F1DA0 (:738)
void material_effects_remove(unsigned int effect_handle); // 0x006F1F50 (:786)
void material_effects_destroy_all(); // 0x006F1FA0 (:815)
const rl_material_map* material_effects_get_original_material_map(rl_renderable_instance* r_inst); // 0x006ED9F0 (:834)
void material_effects_set_manual_time_pct(unsigned int effect_handle, float time_pct); // 0x006E9980 (:850)
void material_effects_force_finalize(); // 0x006E46D0 (:870)
