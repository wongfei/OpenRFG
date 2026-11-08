// Original: volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/vehicle/simulation/default/hkpvehicledefaultsimulation.h
//
// Havok 2014.2.5: full declaration from rfg.pdb (stub_impl.md S8).
#pragma once

#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/memory/memoryclasses/hkmemoryclasses.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/container/array/hkarray.h"
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
class hkpVehicleDefaultSimulation : public hkpVehicleSimulation
{
public:
    HK_DECLARE_CLASS_ALLOCATOR(); // new(unsigned int nbytes) 0x00F83140, delete(void* p, unsigned int nbytes) 0x00F85800
    static const hkClass& staticClass();
    hkpVehicleDefaultSimulation(hkFinishLoadedObjectFlag);
    hkpVehicleDefaultSimulation(); // 0x00F83EC0
    virtual void init(hkpVehicleInstance* instance); // vtbl+0x10 0x00F84750
    virtual hkpVehicleSimulation* clone(hkpVehicleInstance* instance); // vtbl+0x14 0x00F84980
    virtual void simulateVehicle(hkpVehicleInstance* instance, const hkStepInfo& stepInfo, const hkpVehicleSimulation::SimulationInput& simulInput, hkpVehicleJobResults* vehicleJobResults); // vtbl+0x20 0x00F85430
public:
    void prepareAxleParams(const hkpVehicleInstance& vehicleInstance, float deltaTime, const hkArray<float,hkContainerHeapAllocator>& suspensionForceAtWheel, const hkArray<float,hkContainerHeapAllocator>& totalLinearForceAtWheel, hkpRigidBody** groundBody, hkpVehicleFrictionSolverParams& frictionParams, const hkStepInfo& stepInfo, hkpVelocityAccumulator* groundAccum, hkpVelocityAccumulator* groundAccumAtIntegration); // 0x00F85020
    void handleFixedGroundAccum(hkpRigidBody* ground, hkpVelocityAccumulator& accum); // 0x00F83E20
    void getAxleParamsFromWheel(const hkpVehicleInstance& vehicleInstance, int w_it, float linearForceAtWheel, float suspensionForceAtWheel, float estimatedCarSpeed, hkpVehicleFrictionSolverAxleParams& axle_params); // 0x00F84E10
    void prepareChassisParams(const hkpVehicleInstance& vehicleInstance, const hkStepInfo& stepInfo, hkpVehicleFrictionSolverParams& frictionParams); // 0x00F83F40
    void applyAerodynamicDrag(const hkpVehicleInstance& vehicleInstance, const hkpVehicleAerodynamics::AerodynamicsDragOutput& aerodynamicsDragInfo, hkpVehicleFrictionSolverParams& frictionParams, const float deltaTime); // 0x00F84060
    void applyVelocityDamping(const float deltaTime, hkpVehicleFrictionSolverParams& frictionParams, const hkpVehicleVelocityDamper* damper); // 0x00F84190
    void applySuspensionForces(const hkpVehicleInstance& vehicleInstance, float deltaTime, const hkArray<float,hkContainerHeapAllocator>& suspensionForceAtWheel, hkpRigidBody** groundBody, hkpVehicleFrictionSolverParams& frictionParams, hkVector4f* suspensionForcesOnGround); // 0x00F849E0
    void applyDampingToAxleAccumulators(const hkStepInfo& stepInfo, hkpRigidBody** groundBodies, hkpVehicleFrictionSolverParams& frictionParams, hkVector4f* originalLinearVels, hkVector4f* originalAngularVels); // 0x00F84280
    void applyDampingToChassisAccumulator(const hkStepInfo& stepInfo, hkpVelocityAccumulator& accumulator, const hkpMotion* motion); // 0x00F84420
    void getExtraTorqueFactor(const hkpVehicleInstance& vehicleInstance, float deltaTime, hkpVelocityAccumulator& accumulatorForChassis) const; // 0x00F83E70
    void applyFrictionSolver(const hkpVehicleInstance& vehicleInstance, const hkStepInfo& stepInfo, hkpVehicleFrictionSolverParams& frictionParams, hkpVehicleFrictionStatus& frictionStatus); // 0x00F844D0
    void calcChassisVelocities(const hkpVehicleInstance& vehicleInstance, hkpVelocityAccumulator& accumulatorForChassis, hkpVehicleJobResults& vehicleResults); // 0x00F84510
    void calcGroundBodyVelocities(const hkpVehicleInstance& vehicleInstance, float deltaTime, const hkpVehicleFrictionSolverAxleParams* axleParams, hkpRigidBody** groundBody, hkpVehicleJobResults& vehicleResults, hkVector4f* originalLinearVels, hkVector4f* originalAngularVels); // 0x00F84560
public:
    virtual ~hkpVehicleDefaultSimulation(); // vtbl+0x00 0x00F85840

    static const hkInternalClassMember Members[];
public:
    hkpVehicleFrictionStatus m_frictionStatus; // 0x08
    hkRefPtr<hkpVehicleFrictionDescription> m_frictionDescription; // 0x50
};
