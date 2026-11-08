// Original: volition/rfg/code/vehicle/vehicle_deformation.h
// Implementation: volition/rfg/code/vehicle/vehicle_deformation.cpp
//
// Vehicle deformation: impacts add deform spheres to the chassis, wheels and components (synced in multiplayer), unlatched
// hinges, and the render update of the deformed meshes.
#pragma once

#include "volition/rfg/code/objects/level_object_properties_shared.h"

struct mesh_deform_sphere;
struct mesh_deform_sphere_link;
class vector;
class vehicle;
class vehicle_component;

extern bool Vehicle_deformation_enabled; // 0x016BA489

void vehicle_deform(vehicle* vp, vehicle_component* const* damaged_components, unsigned int num_damaged_components, const vector* position, float radius, int hit_points, damaged_by_types damage_from); // 0x00BA1120 (:311)
void vehicle_deform_unlatched_hinge(vehicle_component* component, const vehicle* vp); // 0x00B7C3B0 (:381)
void vehicle_deform_update_render(vehicle* vp); // 0x00B7C5D0 (:436)
void vehicle_deform_free_data(vehicle* vp); // 0x00B663A0 (:497)
void vehicle_deform_add_sphere_mpsync(unsigned int v_handle, const mesh_deform_sphere* deform_sphere); // 0x00BA12F0 (:518)
unsigned int vehicle_get_deform_sphere_count(unsigned int v_handle); // 0x00B7C6C0 (:598)
mesh_deform_sphere_link** vehicle_get_mesh_deform_sphere_link(unsigned int v_handle); // 0x00B7C6F0 (:615)
