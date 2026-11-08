// Original: volition/rfg/code/vehicle/havok/hk_custom_vehiclesimulation.cpp
//
// Volition's copy of hkpVehicleDefaultSimulation, which is dead code in rfg.exe: nothing calls the constructor (0x00B895E0),
// and the vtable is referenced only by it and by clone. The body is shared with Havok's class; see the note in
// hkpvehiclesimulationbody.inl.

#include "volition/rfg/code/vehicle/havok/hk_custom_vehiclesimulation.h"

#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics/constraintsolver/vehiclefriction/hkpvehiclefrictionsolver.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/dynamics/entity/hkprigidbody.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/dynamics/motion/util/hkprigidmotionutil.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/vehicle/hkpvehicledata.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/vehicle/hkpvehicleinstance.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/vehicle/manager/multithreadedvehicle/hkpvehiclejobs.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/vehicle/suspension/hkpvehiclesuspension.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/vehicle/velocitydamper/default/hkpvehicledefaultvelocitydamper.h"

// 0x00B895E0 (:22)
// The hkRefPtr default constructor is empty, so the null of 0x00B8964C is written here.
hkpCustomVehicleSimulation::hkpCustomVehicleSimulation()
    : m_frictionDescription(nullptr)
{
}

// 0x00B896A0
// hkRefPtr has no destructor, so the release is written here.
hkpCustomVehicleSimulation::~hkpCustomVehicleSimulation()
{
    m_frictionDescription = nullptr;
}

// 0x00B895D0 (.h:51)
const hkpVehicleFrictionDescription& hkpCustomVehicleSimulation::getFrictionDescription() const
{
    return *m_frictionDescription;
}

#define VEHICLE_SIMULATION_CLASS hkpCustomVehicleSimulation
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/vehicle/simulation/default/hkpvehiclesimulationbody.inl"
#undef VEHICLE_SIMULATION_CLASS
