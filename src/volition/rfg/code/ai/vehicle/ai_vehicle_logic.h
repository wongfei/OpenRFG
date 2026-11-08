// Original: volition/rfg/code/ai/vehicle/ai_vehicle_logic.h
// Implementation: volition/rfg/code/ai/vehicle/ai_vehicle_logic.cpp
//
// Vehicle logic of the AI: whether a vehicle is usable and available, seats and their types, entering (or teleporting
// into) vehicles, manned turrets, adopting vehicles, smashing buildings.
#pragma once

#include <stdint.h>
#include "volition/rfg/code/ai/ai_types.h"
#include "volition/rfg/code/vehicle/vehicle_shared_defines.h"

class ai_system;
class human;
class npc;
class object;
class object_path_road;
class turret;
class vector;
class vehicle;

enum hint_type
{
    HT_SELECTION = 0,
    HT_ABORT = 1,
    HT_MISC = 2,
    HT_MISC_2 = 3,
    HT_LAST = 4,
};

bool ai_vehicle_reserve(npc* np, vehicle* vp, ai_vehicle_seat_types seat_type); // 0x00673E20 (.h:82)
bool ai_should_smash_buildings(ai_system* aip); // 0x0068AB60 (:124)
bool ai_vehicle_ensure_seat_turret_manned(npc* np, vehicle* vp, turret* tp); // 0x005F41A0 (:399)
ai_results ai_vehicle_use_turret(npc* np, turret* tp, vehicle* vp, bool fire_at_alternate, object* objp, bool fire_even_if_not_in_los, vector pos); // 0x006ABD80 (:423)
bool ai_vehicle_use_turret_if_available(npc* np, vehicle* vp, bool fire_even_if_no_los, bool fire_at_alternate, object* objp, vector pos); // 0x006AC490 (:602)
bool ai_vehicle_has_manned_turret(const vehicle* vp); // 0x006454F0 (:624)
const turret* ai_vehicle_get_first_manned_turret(const vehicle* vp); // 0x00645580 (:657)
bool ai_vehicle_has_available_seat(const vehicle* vp, const npc* np, ai_vehicle_seat_types seat_type); // 0x0066E9E0 (:695)
bool ai_vehicle_is_seat_available(const vehicle* vp, const npc* np, vehicle_seat_index seat_idx); // 0x00645610 (:722)
bool ai_vehicle_is_usable_and_available(const vehicle* vp, const npc* np, ai_vehicle_seat_types seat_type); // 0x0068AC00 (:752)
bool ai_vehicle_is_usable(const vehicle* vp, const npc* np, ai_vehicle_seat_types seat_type); // 0x0066EA50 (:773)
ai_vehicle_seat_types ai_vehicle_get_seat_type(const npc* np, const vehicle* vp, ai_vehicle_seat_types hint_type); // 0x006456C0 (:1106)
ai_vehicle_seat_types ai_vehicle_get_entering_seat_type(const npc* np, const vehicle* vp, ai_vehicle_seat_types hint_type); // 0x00645730 (:1126)
vehicle* ai_vehicle_find_available_seat_type(const npc* np, ai_vehicle_seat_types seat_type, const vehicle* exclude_vehicle); // 0x0068AC40 (:1154)
vehicle* ai_flyer_find_available_seat_type(const npc* np, ai_vehicle_seat_types seat_type, const vehicle* exclude_vehicle); // 0x0068AD50 (:1176)
vehicle* ai_vehicle_locate_for_man_turret_in_vehicle(const npc* np); // 0x0068AE10 (:1234)
bool passenger_seat_available(const vehicle* vp, const npc* np); // 0x0066E7A0 (:247), goals.cpp calls it
turret* gunner_seat_available(const vehicle* vp, const npc* np, bool passenger_only, vehicle_seat_index* seat_idx); // 0x0066E880 (:269)
human* get_first_gunner(const vehicle* vp); // 0x00645470 (:336)
vehicle* ai_vehicle_locate_tank(const npc* np); // 0x0068B0D0 (:1338)
human* ai_vehicle_find_human_in_seat_type(const vehicle* vp, ai_vehicle_seat_types seat_type); // 0x006457B0 (:1384)
vehicle* ai_vehicle_get_vehicle_squad_leader_is_going_to(const npc* np, npc** squad_leaderp); // 0x00645810 (:1413)
ai_results ai_vehicle_enter_teleport(npc* np, vehicle* vp, ai_vehicle_seat_types seat_type); // 0x0066F290 (:1452)
ai_results ai_vehicle_enter(npc* np, vehicle* vp, ai_vehicle_seat_types seat_type, bool reserve_only); // 0x0066F390 (:1529)
bool ai_vehicle_should_stop_to_let_passenger_out(const vehicle* vp); // 0x006458A0 (:1594)
bool ai_vehicle_should_get_out_of_moving_vehicle(const npc* np, const vehicle* vp); // 0x005F4200 (:1639)
void ai_vehicle_override_speed(vehicle* vp, float speed); // 0x005F4260 (:1670)
void ai_vehicle_stop(vehicle* vp); // 0x005F42A0 (:1691)
bool ai_vehicle_should_update_travel_to_point(vehicle* vp); // 0x005F4300 (:1722)
ai_results ai_vehicle_travel_to_a_point_do_frame(npc* np, vehicle* vp, const vector& dest_point, const uint64_t dest_navcell_hint_handle, bool use_roads, float dist, bool dont_stop, bool use_light, bool use_disabled, bool use_smoothing); // 0x0068B1F0 (:2045)
bool ai_vehicle_pick_flee_destination_road(const vehicle* vp, object_path_road* prp, vector& destination_point, const vector& flee_pos); // 0x0066F4D0 (:2276)
bool ai_vehicle_maybe_adopt_vehicle(npc* np, vehicle* vp); // 0x006462D0 (:2549)
