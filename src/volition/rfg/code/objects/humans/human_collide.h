// Original: volition/rfg/code/objects/humans/human_collide.h
// Implementation: volition/rfg/code/objects/humans/human_collide.cpp
//
// Collisions of humans with humans, vehicles and movers: the damage they take (by speed, with head and falling
// multipliers and a per-pair timer), knockdowns and the ragdoll perpetrator of a car hit.
#pragma once


class human;
class object;
class vector;
class vehicle;

unsigned int get_ragdoll_car_perp_handle(vehicle* p_car); // 0x00A2EFD0 (:102)
float human_collide_calculate_damage_with_timer(human* hp, float damage); // 0x00A093B0 (:127)
void human_collide_vehicle(const vector& hit_pos, const vector& hit_normal, float rel_speed, unsigned int human_handle, unsigned int car_handle, unsigned int component_idx, bool run_over); // 0x00ACF910 (:152)
void human_collide_human(human* hp1, human* hp2); // 0x00AD0850 (:659)
float human_collide_mover_damage(human* hp, object* other_obj, const vector& pos, const vector& object_velocity, const vector& collision_normal); // 0x00AD08A0 (:685)
void human_collide_level_init(); // 0x00A09480 (:832)
void human_collide_level_shutdown(); // 0x00A09490 (:849)
