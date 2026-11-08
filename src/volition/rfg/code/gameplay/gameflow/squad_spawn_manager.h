// Original: volition/rfg/code/gameplay/gameflow/squad_spawn_manager.h
// Implementation: volition/rfg/code/gameplay/gameflow/squad_spawn_manager.cpp
//
// The squad spawn manager (ssm): spawns, simulates and despawns squad members as the player moves.
#pragma once

#include "volition/vlib/code/time/timestamp.h"

class object_squad;

// size 0xC
struct squad_evaluation_struct
{
    squad_evaluation_struct(); // 0x0075E4A0 (:88)

    object_squad* squad; // 0x00
    float distance_sqrd; // 0x04
    bool should_be_loaded; // 0x08
};

// size 0x24
class squad_member_manager_data
{
public:
    squad_member_manager_data(); // 0x00774350 (:106)
    void reset(); // 0x0075E4C0 (:323)
    bool do_frame(); // 0x0078D5B0 (:588)
    bool do_squad_simulation(); // 0x0078D2C0 (:448)
    bool do_clean_up(); // 0x0078D4E0 (:518)
    bool do_spawning(); // 0x0078D1A0 (:343)
    bool do_despawning(); // 0x0078D230 (:387)

    timestamp squad_evaluation; // 0x00
    timestamp clean_up_period; // 0x04
    timestamp attempt_spawn; // 0x08
    timestamp attempt_despawn; // 0x0C
    timestamp attempt_respawn; // 0x10
    timestamp can_poll_squads; // 0x14
    float current_squad_load_radius_sqrd; // 0x18
    float current_squad_unload_radius_sqrd; // 0x1C
    struct
    {
        bool squads_dirty : 1; // 0x00:0
        bool dead_squad : 1; // 0x00:1
    } sm_flags; // 0x20
};

void ssm_game_init(); // 0x0075E520 (:627)
void ssm_level_init_pre(); // 0x0075E530 (:633)
bool ssm_do_frame(); // 0x0078D600 (:638)
