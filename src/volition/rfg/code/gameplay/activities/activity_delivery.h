// Original: volition/rfg/code/gameplay/activities/activity_delivery.h
// Implementation: volition/rfg/code/gameplay/activities/activity_delivery.cpp
//
// Delivery ("Guerrilla Express") activity: vehicle preparation and speed limits, start, replay and exit, the end node
// streaming, remaining time and distance for the interface, and the score results.
#pragma once


class object_delivery_node;
class object_vehicle_spawn_node;
class vehicle;

bool this_instance_of_delivery_is_disabled(object_delivery_node* delivery_node); // 0x007970B0 (:78)
bool vehicle_is_ready_to_start(vehicle* vp, const object_vehicle_spawn_node* spawn_node); // 0x00758000 (:88)
void set_vehicle_max_speed(unsigned int vehicle_handle, float speed); // 0x0076C4B0 (:96)
bool activity_delivery_start_activity(const object_delivery_node* delivery_node); // 0x007A0FB0 (:1232)
void activity_guerrilla_express_exit(); // 0x007B5E70 (:1245)
void activity_guerrilla_express_replay(); // 0x007B5EE0 (:1254)
void activity_guerrilla_express_process_player_death(); // 0x0076C8E0 (:1297)
void activity_delivery_end_node_stream_in(const object_delivery_node* delivery_node); // 0x00758350 (:1304)
void activity_delivery_end_node_stream_out(const object_delivery_node* delivery_node); // 0x007583B0 (:1313)
float activity_delivery_get_destination_dist(); // 0x00758400 (:1320)
unsigned int activity_delivery_get_remaining_time(); // 0x0076C8F0 (:1326)
void activity_delivery_get_display_time(int& num_minutes, int& num_seconds, unsigned int time); // 0x00758410 (:1331)
void activity_guerrilla_express_get_score_results(float& par_time, float& target_time, float& best_time, float& current_time); // 0x0076C900 (:1338)
object_vehicle_spawn_node* activity_delivery_get_vehicle_spawn(const object_delivery_node* delivery_node); // 0x0076C9A0 (:1347)
