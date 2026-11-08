// Original: volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/vehicle/simulation/default/hkpvehicledefaultsimulation.cpp
//
// Havok 2014.2.5 (stub_impl.md S8): the vehicle simulation step, from rfg.exe (0x00F83E20-0x00F85860). This is the class the
// game's vehicles have: automobiles set no simulation, so hkpVehicleInstance::init creates this one. Their step is
// hk_custom_vehicle_instance::applyHk550Action, though, which reads only m_frictionDescription from it.

#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/vehicle/simulation/default/hkpvehicledefaultsimulation.h"

#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics/constraintsolver/vehiclefriction/hkpvehiclefrictionsolver.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/dynamics/entity/hkprigidbody.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/dynamics/motion/util/hkprigidmotionutil.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/vehicle/hkpvehicledata.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/vehicle/hkpvehicleinstance.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/vehicle/manager/multithreadedvehicle/hkpvehiclejobs.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/vehicle/suspension/hkpvehiclesuspension.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/vehicle/velocitydamper/default/hkpvehicledefaultvelocitydamper.h"

// 0x00B63280 (hkpvehiclesimulation.h:57)
hkpVehicleSimulation::hkpVehicleSimulation()
{
}

// 0x00F83EC0
// The hkRefPtr default constructor is empty, so the null of 0x00F83F2C is written here: init releases what the member
// holds, and a reused heap block crashed there on the first automobile of a territory reload.
hkpVehicleDefaultSimulation::hkpVehicleDefaultSimulation()
    : m_frictionDescription(nullptr)
{
}

#define VEHICLE_SIMULATION_CLASS hkpVehicleDefaultSimulation
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/vehicle/simulation/default/hkpvehiclesimulationbody.inl"
#undef VEHICLE_SIMULATION_CLASS

// 0x00B632B0
hkpVehicleSimulation::~hkpVehicleSimulation()
{
}

// 0x00F85840
// hkRefPtr has no destructor, so the release is written here.
hkpVehicleDefaultSimulation::~hkpVehicleDefaultSimulation()
{
    m_frictionDescription = nullptr;
}
