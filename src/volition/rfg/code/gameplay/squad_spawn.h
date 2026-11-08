// Original: volition/rfg/code/gameplay/squad_spawn.h
// Implementation: volition/rfg/code/gameplay/squad_spawn.cpp
//
// Squad spawning: the creation parameters of a squad (team, members, vehicle, orders) and the spawn parameters that
// spawn it, create its orders and destroy it again.
#pragma once

#include "volition/rfg/code/gameplay/gameflow/gfm_orders_manager.h"
#include "volition/rfg/code/hud/minimap.h"
#include "volition/rfg/code/objects/humans/human.h"
#include "volition/rfg/code/objects/humans/npc.h"
#include "volition/rfg/code/objects/object_squad.h"
#include "volition/rfg/code/vehicle/vehicle_class.h"
#include "volition/vlib/code/math/matrix.h"
#include "volition/vlib/code/math/vector3.h"

class object_spawn_region;
class rfg_mover;
class vehicle_spawn_group_info;

// size 0x78
class squad_creation_parameters
{
public:
    squad_creation_parameters(const vector& new_squad_pos, const squad_definition* new_squad_def); // 0x00775590 (:163)
    squad_creation_parameters(); // 0x0060CC70 (.h:26)
    void reset(); // 0x0075F3C0 (:174)
    bool is_valid() const; // 0x0075F460 (:193)
    vector get_squad_pos() const;
    object_squad* create_squad() const; // 0x007755E0 (:208)

    vector squad_pos; // 0x00
    const object_spawn_region* osrp; // 0x0C
    matrix squad_orient; // 0x10
    const squad_definition* squad_def; // 0x34
    const vehicle_spawn_group_info* veh_spawn_group; // 0x38
    unsigned int orders_handle; // 0x3C
    unsigned int spawn_node_handle; // 0x40
    squad_flags_struct squad_flags; // 0x44
    npc_flags_struct npc_flags; // 0x48
    human_flags hflags; // 0x54
    vehicle_flags vflags; // 0x64
    float vehicle_hp_mult; // 0x6C
    unsigned int parent_handle; // 0x70
    minimap_elements squad_map_icon; // 0x74
};

// size 0x9C
class squad_spawn_parameters
{
public:
    squad_spawn_parameters(); // 0x0060CCA0 (.h:96)
    void reset(); // 0x0075F490 (:272)
    object_squad* spawn_squad(); // 0x007B0E00 (:282)
    void destroy_squad(); // 0x007A5E60 (:309)
    gfm_orders* create_orders(); // 0x007AC170 (:323)
    void destroy_orders(); // 0x0079E580 (:344)
    bool spawn_squad_members(); // 0x0075F4E0 (:357)

    squad_creation_parameters squad; // 0x00
    object_squad* spawned_squad; // 0x78
    gfm_orders_creation_params orders; // 0x7C
    gfm_orders* created_orders; // 0x98
};

const rfg_mover* find_local_building(const vector& pos); // 0x00790AE0 (:35)
void unused_dcf_drop_squad(); // 0x007B5290 (:76)
