// Original: volition/rfg/code/havok/havok_collision_callbacks.h
// Implementation: volition/rfg/code/havok/havok_collision_callbacks.cpp
//
// The game's collision responses, registered per object type pair with the collision event queue
// (havok_collision_events.h) by havok_collision_callbacks_system_init: damage from impacts for debris, vehicles,
// weapons, items and movers, and the mass changers that make vehicles and projectiles push each other plausibly.
// havok_collision_calculate_damage turns the impact of a collision event into damage at the best contact point.
#pragma once


struct collision_event;
class vector;

void havok_collide_debris(const collision_event* c_event); // 0x0080F460 (:527)
void havok_collide_vehicle(const collision_event* c_event); // 0x0082A560 (:558)
void havok_collide_weapon(const collision_event* c_event); // 0x0081ACE0 (:766)
void havok_collide_item(const collision_event* c_event); // 0x0081ADA0 (:797)
void havok_collide_mover(const collision_event* c_event); // 0x0081AE90 (:838)
void havok_collision_callbacks_system_init(); // 0x0082AEC0 (:1056)
void havok_collision_callbacks_system_shutdown(); // 0x0081B1D0 (:1109)
float havok_collision_calculate_damage(const collision_event* c_event, vector& position, vector& normal, vector& velocity, unsigned int& best_collision); // 0x0080F570 (:1118)
