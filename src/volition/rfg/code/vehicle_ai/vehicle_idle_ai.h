// Original: volition/rfg/code/vehicle_ai/vehicle_idle_ai.h
// Implementation: volition/rfg/code/vehicle_ai/vehicle_idle_ai.cpp
//
// The idle state of the vehicle AI.
#pragma once


class vehicle;
class vehicle_ai;

void vehicle_ai_process_idle(vehicle* vp); // 0x00BCA1E0 (:220)
void vehicle_ai_exit_idle(vehicle_ai* vai); // 0x00BC56C0 (:254)
void vehicle_ai_enter_idle(vehicle_ai* vai); // 0x00BCA260 (:258)
