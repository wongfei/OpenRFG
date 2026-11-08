// Original: volition/rfg/code/vehicle/vehicle_cm_dynamics_info.h
//
// Mass properties of a vehicle collision model (index, MOPP code, volume, centre of mass, inertia tensor), stored in
// .ccar_pc files. Placement inferred.
#pragma once

#include "volition/vlib/code/math/matrix.h"
#include "volition/vlib/code/util/endian_type.h"

class hkpMoppCode;

// size 0x48
struct vehicle_cm_dynamics_info
{
    unsigned int m_cm_index; // 0x00
    et_sized_native_pointer<hkpMoppCode const > m_mopp_code; // 0x08
    float m_volume; // 0x10
    vector m_center_of_mass; // 0x14
    matrix m_inertia_tensor; // 0x20
};
