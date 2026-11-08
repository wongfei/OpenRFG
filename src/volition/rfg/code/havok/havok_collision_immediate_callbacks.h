// Original: volition/rfg/code/havok/havok_collision_immediate_callbacks.h
// Implementation: volition/rfg/code/havok/havok_collision_immediate_callbacks.cpp
//
// Contact point callbacks run inside the Havok step (contact_point_action: accept or reject the contact) for
// projectiles, weapons, vehicles and items; registered by havok_collision_immediate_callbacks_system_init.
#pragma once

void havok_collision_immediate_callbacks_system_init(); // 0x008249A0 (:262)
void havok_collision_immediate_callbacks_system_shutdown(); // 0x00810C80 (:268)
