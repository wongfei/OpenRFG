// Original: volition/rfg/code/gameplay/gameflow/continuous_support.h
// Implementation: volition/rfg/code/gameplay/gameflow/continuous_support.cpp
//
// Continuous support: squads that keep respawning to support an encounter in registered regions.
#pragma once

#include "volition/rfg/code/gameplay/gameflow/gameflow_enums.h"

class object_squad;
class object_squad_spawn_node;

void cont_support_level_init_pre(); // 0x007A4660 (:307)
void cont_support_do_frame(); // 0x007B70D0 (:314)
void cont_support_add_squad(const object_squad_spawn_node* squad_node, int new_respawn_delay_ms); // 0x007A46A0 (:327)
void cont_support_add_squad(const object_squad* squad, int new_respawn_delay_ms); // 0x007A4720 (:340)
void cont_support_remove_squad(unsigned int squad_node_handle); // 0x0079C8B0 (:354)
void cont_support_remove_all_squads(); // 0x007A4760 (:370)
void cont_support_register_region(const object_squad_spawn_node* squad_node, unsigned int spawn_region_handle, float min_spawn_distance); // 0x0078BBC0 (:384)
void cont_support_update_orders(const object_squad_spawn_node* squad_node, gfm_orders_types orders_type, unsigned int target_handle, unsigned int orders_handle); // 0x0078BC00 (:402)
