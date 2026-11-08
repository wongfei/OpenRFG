// Original: volition/rfg/code/vehicle/havok/hk_custom_vehiclesimulation.h
// Implementation: volition/rfg/code/vehicle/havok/hk_custom_vehiclesimulation.cpp
//
// The vehicle simulation step (Havok's hkpVehicleDefaultSimulation, copied and modified): prepares the chassis and
// axle parameters, applies aerodynamic drag, damping and suspension forces, runs the friction solver and writes the
// resulting chassis and ground body velocities.
#pragma once

#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/container/array/hkarray.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/memory/memoryclasses/hkmemoryclasses.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/types/hkrefptr.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics/constraintsolver/vehiclefriction/hkpvehiclefriction.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/vehicle/aerodynamics/hkpvehicleaerodynamics.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/vehicle/simulation/hkpvehiclesimulation.h"

class hkClass;
struct hkContainerHeapAllocator;
class hkFinishLoadedObjectFlag;
struct hkInternalClassMember;
class hkStepInfo;
class hkVector4f;
class hkpMotion;
class hkpRigidBody;
struct hkpVehicleFrictionSolverAxleParams;
struct hkpVehicleFrictionSolverParams;
class hkpVehicleInstance;
struct hkpVehicleJobResults;
class hkpVehicleVelocityDamper;
class hkpVelocityAccumulator;

// size 0x54
class hkpCustomVehicleSimulation : public hkpVehicleSimulation
{
public:
    struct MustEndWithSemiColon; // no definition in PDB

    struct DefaultStruct; // no definition in PDB

    HK_DECLARE_CLASS_ALLOCATOR(); // new(unsigned int nbytes) 0x00B737F0 (.h:24), delete(void* p, unsigned int nbytes) 0x00B89660 (.h:24)
    static const hkClass& staticClass();
    hkpCustomVehicleSimulation(hkFinishLoadedObjectFlag);
    hkpCustomVehicleSimulation(); // 0x00B895E0 (:22)
    virtual void init(hkpVehicleInstance* instance); // vtbl+0x10 0x00B89B80 (:1021)
    virtual hkpVehicleSimulation* clone(hkpVehicleInstance* instance); // vtbl+0x14 0x00B89DE0 (:1069)
    virtual void simulateVehicle(hkpVehicleInstance* instance, const hkStepInfo& stepInfo, const hkpVehicleSimulation::SimulationInput& simulInput, hkpVehicleJobResults* vehicleJobResults); // vtbl+0x20 0x00BADEE0 (:451)
    const hkpVehicleFrictionDescription& getFrictionDescription() const; // 0x00B895D0 (.h:51)
protected:
    void prepareAxleParams(const hkpVehicleInstance& vehicleInstance, float deltaTime, const hkArray<float,hkContainerHeapAllocator>& suspensionForceAtWheel, const hkArray<float,hkContainerHeapAllocator>& totalLinearForceAtWheel, hkpRigidBody** groundBody, hkpVehicleFrictionSolverParams& frictionParams, const hkStepInfo& stepInfo, hkpVelocityAccumulator* groundAccum, hkpVelocityAccumulator* groundAccumAtIntegration); // 0x00BA7700 (:572)
    void handleFixedGroundAccum(hkpRigidBody* ground, hkpVelocityAccumulator& accum); // 0x00B73860 (:663)
    void getAxleParamsFromWheel(const hkpVehicleInstance& vehicleInstance, int w_it, float linearForceAtWheel, float suspensionForceAtWheel, float estimatedCarSpeed, hkpVehicleFrictionSolverAxleParams& axle_params); // 0x00B9C940 (:670)
    void prepareChassisParams(const hkpVehicleInstance& vehicleInstance, const hkStepInfo& stepInfo, hkpVehicleFrictionSolverParams& frictionParams); // 0x00B738B0 (:714)
    void applyAerodynamicDrag(const hkpVehicleInstance& vehicleInstance, const hkpVehicleAerodynamics::AerodynamicsDragOutput& aerodynamicsDragInfo, hkpVehicleFrictionSolverParams& frictionParams, const float deltaTime); // 0x00B739E0 (:734)
    void applyVelocityDamping(const float deltaTime, hkpVehicleFrictionSolverParams& frictionParams, const hkpVehicleVelocityDamper* damper); // 0x00B896D0 (:754)
    void applySuspensionForces(const hkpVehicleInstance& vehicleInstance, float deltaTime, const hkArray<float,hkContainerHeapAllocator>& suspensionForceAtWheel, hkpRigidBody** groundBody, hkpVehicleFrictionSolverParams& frictionParams, hkVector4f* suspensionForcesOnGround); // 0x00B897C0 (:776)
    void applyDampingToAxleAccumulators(const hkStepInfo& stepInfo, hkpRigidBody** groundBodies, hkpVehicleFrictionSolverParams& frictionParams, hkVector4f* originalLinearVels, hkVector4f* originalAngularVels); // 0x00B73AC0 (:859)
    void applyDampingToChassisAccumulator(const hkStepInfo& stepInfo, hkpVelocityAccumulator& accumulator, const hkpMotion* motion); // 0x00B73C60 (:911)
    void getExtraTorqueFactor(const hkpVehicleInstance& vehicleInstance, float deltaTime, hkpVelocityAccumulator& accumulatorForChassis) const; // 0x00B73D10 (:935)
    void applyFrictionSolver(const hkpVehicleInstance& vehicleInstance, const hkStepInfo& stepInfo, hkpVehicleFrictionSolverParams& frictionParams, hkpVehicleFrictionStatus& frictionStatus); // 0x00B89B40 (:948)
    void calcChassisVelocities(const hkpVehicleInstance& vehicleInstance, hkpVelocityAccumulator& accumulatorForChassis, hkpVehicleJobResults& vehicleResults); // 0x00B73D80 (:958)
    void calcGroundBodyVelocities(const hkpVehicleInstance& vehicleInstance, float deltaTime, const hkpVehicleFrictionSolverAxleParams* axleParams, hkpRigidBody** groundBody, hkpVehicleJobResults& vehicleResults, hkVector4f* originalLinearVels, hkVector4f* originalAngularVels); // 0x00B73DD0 (:968)
public:
    virtual ~hkpCustomVehicleSimulation(); // vtbl+0x00 0x00B896A0

    static const hkInternalClassMember Members[];
    hkpVehicleFrictionStatus m_frictionStatus; // 0x08
protected:
    hkRefPtr<hkpVehicleFrictionDescription> m_frictionDescription; // 0x50
};
