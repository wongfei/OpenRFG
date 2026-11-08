// Original: volition/rfg/code/vehicle_ai/vehicle_offscreen_pathfollow.h
// Implementation: volition/rfg/code/vehicle_ai/vehicle_offscreen_pathfollow.cpp
//
// Offscreen path following: moving AI vehicles the player cannot see along their path without physics.
#pragma once


class vehicle_ai;

float vehicle_hidden_calc_speed(vehicle_ai* vai); // 0x00BCA4A0 (:94)
void vehicle_hidden_do_movement(vehicle_ai* vai); // 0x00BCA580 (:133)
