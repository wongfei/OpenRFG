// Original: volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/vehicle/hkpvehicleinstance.h
//
// Havok 2014.2.5: full declaration from rfg.pdb (stub_impl.md S8).
#pragma once

#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/memory/memoryclasses/hkmemoryclasses.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/container/array/hkarray.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/container/hkcontainerallocators.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/math/quaternion/hkquaternionf.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/math/vector/hkvector4f.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/types/hkbasetypes.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/types/physics/contactpoint/hkcontactpoint.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/dynamics/action/hkpunaryaction.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/vehicle/aerodynamics/hkpvehicleaerodynamics.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/vehicle/brake/hkpvehiclebrake.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/vehicle/driverinput/hkpvehicledriverinput.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/vehicle/transmission/hkpvehicletransmission.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/vehicle/wheelcollide/hkpvehiclewheelcollide.h"

class hkClass;
struct hkInternalClassMember;
class hkStepInfo;
class hkpAction;
class hkpEntity;
class hkpMotion;
class hkpPhantom;
class hkpRigidBody;
class hkpTyremarksInfo;
class hkpVehicleData;
class hkpVehicleEngine;
struct hkpVehicleFrictionSolverAxleParams;
struct hkpVehicleFrictionSolverParams;
struct hkpVehicleJobResults;
class hkpVehicleSimulation;
class hkpVehicleSteering;
class hkpVehicleSuspension;
class hkpVehicleVelocityDamper;
class hkpVelocityAccumulator;
class hkpWorld;

// size 0x94
class hkpVehicleInstance : public hkpUnaryAction
{
public:
    // size 0xE0
    struct WheelInfo
    {
        enum
        {
            MAX_NUM_SHAPE_KEYS = 8,
        };

        HK_DECLARE_NONVIRTUAL_CLASS_ALLOCATOR(WheelInfo);
        static const hkClass& staticClass();
        void init(); // 0x00F81730

        static const hkInternalClassMember Members[];
        hkContactPoint m_contactPoint; // 0x00
        float m_contactFriction; // 0x20
        hkpRigidBody* m_contactBody; // 0x24
        unsigned int m_contactShapeKey[8]; // 0x28
        hkVector4f m_hardPointWs; // 0x50
        hkVector4f m_rayEndPointWs; // 0x60
        float m_currentSuspensionLength; // 0x70
        hkVector4f m_suspensionDirectionWs; // 0x80
        hkVector4f m_spinAxisChassisSpace; // 0x90
        hkVector4f m_spinAxisWs; // 0xA0
        hkQuaternionf m_steeringOrientationChassisSpace; // 0xB0
        float m_spinVelocity; // 0xC0
        float m_noSlipIdealSpinVelocity; // 0xC4
        float m_spinAngle; // 0xC8
        float m_skidEnergyDensity; // 0xCC
        float m_sideForce; // 0xD0
        float m_forwardSlipVelocity; // 0xD4
        float m_sideSlipVelocity; // 0xD8
    };

    HK_DECLARE_CLASS_ALLOCATOR(); // new(unsigned int nbytes) 0x00B715F0, delete(void* p, unsigned int nbytes) 0x00B9D100
    static const hkClass& staticClass();
    hkpVehicleInstance(hkFinishLoadedObjectFlag);
    hkpVehicleInstance(hkpRigidBody* chassis); // 0x00F82100
    virtual ~hkpVehicleInstance(); // vtbl+0x00 0x00F82170
    virtual void applyAction(const hkStepInfo& stepInfo); // vtbl+0x10 0x00F82D00
    virtual void getPhantoms(hkArray<hkpPhantom*,hkContainerHeapAllocator>& phantomsOut); // vtbl+0x18 0x00F814A0
    virtual hkpAction* clone(const hkArray<hkpEntity*,hkContainerHeapAllocator>& newEntities, const hkArray<hkpPhantom*,hkContainerHeapAllocator>& newPhantoms) const; // vtbl+0x20 0x00F824C0
    void stepVehicle(const hkStepInfo& stepInfo); // 0x00F82C10
    void updateBeforeCollisionDetection(); // 0x00F817E0
    void stepVehicleUsingWheelCollideOutput(const hkStepInfo& stepInfo, const hkpVehicleWheelCollide::CollisionDetectionWheelOutput* cdInfo); // 0x00F82AB0
    virtual void init(); // vtbl+0x24 0x00F81C80
    virtual void addToWorld(hkpWorld* world); // vtbl+0x28 0x00F816D0
    virtual void removeFromWorld(); // vtbl+0x2C 0x00F81700
    virtual void calcCurrentPositionAndRotation(const hkpRigidBody* chassis, const hkpVehicleSuspension* suspension, int wheelNo, hkVector4f& posOut, hkQuaternionf& rotOut); // vtbl+0x30 0x00F819D0
    virtual float calcRPM() const; // vtbl+0x34 0x00F81600
    virtual float calcKMPH() const; // vtbl+0x38 0x00F82020
    virtual float calcMPH() const; // vtbl+0x3C 0x00F82090
    virtual void handleFixedGroundAccum(hkpRigidBody* ground, hkpVelocityAccumulator& accum); // vtbl+0x40 0x00F81650
    unsigned char getNumWheels() const; // 0x00B632D0 (hkpvehicleinstance.inl:15)
    void updateComponents(const hkStepInfo& stepInfo, const hkpVehicleWheelCollide::CollisionDetectionWheelOutput* cdInfo, hkpVehicleAerodynamics::AerodynamicsDragOutput& aerodynamicsDragInfo, hkArray<float,hkContainerHeapAllocator>& suspensionForceAtWheel, hkArray<float,hkContainerHeapAllocator>& totalLinearForceAtWheel); // 0x00F82850
    void simulateVehicle(const hkStepInfo& stepInfo, const hkpVehicleAerodynamics::AerodynamicsDragOutput& aerodynamicsDragInfo, const hkArray<float,hkContainerHeapAllocator>& suspensionForceAtWheel, const hkArray<float,hkContainerHeapAllocator>& totalLinearForceAtWheel, hkpVehicleJobResults* vehicleJobResults); // 0x00F81930
    void updateWheels(float deltaTime, const hkpVehicleWheelCollide::CollisionDetectionWheelOutput* cdInfo); // 0x00F822D0
    void updateDriverInput(float deltaTime, hkpVehicleDriverInput::FilteredDriverInputOutput& filteredDriverInputInfo); // 0x00F814B0
    void updateSteering(float deltaTime, const hkpVehicleDriverInput::FilteredDriverInputOutput& filteredDriverInputInfo); // 0x00F82710
    void updateTransmission(float deltaTime, hkpVehicleTransmission::TransmissionOutput& transmissionInfo); // 0x00F814F0
    void updateEngine(float deltaTime, const hkpVehicleDriverInput::FilteredDriverInputOutput& filteredDriverInputInfo, const hkpVehicleTransmission::TransmissionOutput& transmissionInfo); // 0x00F81570
    void updateBrake(float deltaTime, const hkpVehicleDriverInput::FilteredDriverInputOutput& filteredDriverInputInfo, hkpVehicleBrake::WheelBreakingOutput& wheelBreakingInfo); // 0x00F81F30
    void updateSuspension(float deltaTime, const hkpVehicleWheelCollide::CollisionDetectionWheelOutput* cdInfo, hkArray<float,hkContainerHeapAllocator>& suspensionForces); // 0x00F816A0
    void updateAerodynamics(float deltaTime, hkpVehicleAerodynamics::AerodynamicsDragOutput& aerodynamicsDragInfo); // 0x00F815D0
    void prepareChassisParams(const hkStepInfo&, hkpVehicleFrictionSolverParams&);
    void prepareAxleParams(float, const hkArray<float,hkContainerHeapAllocator>&, const hkArray<float,hkContainerHeapAllocator>&, hkpRigidBody**, hkpVehicleFrictionSolverParams&, const hkStepInfo&, hkpVelocityAccumulator*, hkpVelocityAccumulator*);
    void getAxleParamsFromWheel(int, float, float, float, hkpVehicleFrictionSolverAxleParams&);
    void applyAerodynamicDrag(const hkpVehicleAerodynamics::AerodynamicsDragOutput&, hkpVehicleFrictionSolverParams&, const float);
    void applyVelocityDamping(const float, hkpVehicleFrictionSolverParams&, const hkpVehicleVelocityDamper*);
    void applySuspensionForces(float, const hkArray<float,hkContainerHeapAllocator>&, hkpRigidBody**, hkpVehicleFrictionSolverParams&, hkVector4f*);
    void applyDampingToAxleAccumulators(const hkStepInfo&, hkpRigidBody**, hkpVehicleFrictionSolverParams&, hkVector4f*, hkVector4f*);
    void applyDampingToChassisAccumulator(const hkStepInfo&, hkpVelocityAccumulator&, const hkpMotion*);
    void getExtraTorqueFactor(float, hkpVelocityAccumulator&) const;
    void applyFrictionSolver(const hkStepInfo&, hkpVehicleFrictionSolverParams&);
    void calcChassisVelocities(hkpVelocityAccumulator&, hkpVehicleJobResults&);
    void calcGroundBodyVelocities(float, const hkpVehicleFrictionSolverAxleParams*, hkpRigidBody**, hkpVehicleJobResults&, hkVector4f*, hkVector4f*);
    void applyResultsToWheelInfo(float, const hkpRigidBody* const *, const hkpVehicleFrictionSolverParams&);
    hkpRigidBody* getChassis() const; // 0x00B632C0 (hkpvehicleinstance.inl:10)
    void setChassis(hkpRigidBody* chassis); // 0x00F81490
    void afterReflectNew();

    static const hkInternalClassMember Members[];
    static const unsigned char s_maxNumLocalWheels; // 0x012C917C
    hkpVehicleData* m_data; // 0x1C
    hkpVehicleDriverInput* m_driverInput; // 0x20
    hkpVehicleSteering* m_steering; // 0x24
    hkpVehicleEngine* m_engine; // 0x28
    hkpVehicleTransmission* m_transmission; // 0x2C
    hkpVehicleBrake* m_brake; // 0x30
    hkpVehicleSuspension* m_suspension; // 0x34
    hkpVehicleAerodynamics* m_aerodynamics; // 0x38
    hkpVehicleWheelCollide* m_wheelCollide; // 0x3C
    hkpTyremarksInfo* m_tyreMarks; // 0x40
    hkpVehicleVelocityDamper* m_velocityDamper; // 0x44
    hkpVehicleSimulation* m_vehicleSimulation; // 0x48
    hkArray<WheelInfo,hkContainerHeapAllocator> m_wheelsInfo; // 0x4C
    hkpVehicleDriverInputStatus* m_deviceStatus; // 0x58
    hkArray<hkBool,hkContainerHeapAllocator> m_isFixed; // 0x5C
    float m_wheelsTimeSinceMaxPedalInput; // 0x68
    hkBool m_tryingToReverse; // 0x6C
    float m_torque; // 0x70
    float m_rpm; // 0x74
    float m_mainSteeringAngle; // 0x78
    float m_mainSteeringAngleAssumingNoReduction; // 0x7C
    hkArray<float,hkContainerHeapAllocator> m_wheelsSteeringAngle; // 0x80
    hkBool m_isReversing; // 0x8C
    signed char m_currentGear; // 0x8D
    hkBool m_delayed; // 0x8E
    float m_clutchDelayCountdown; // 0x90
};
