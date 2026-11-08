// Original: volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/vehicle/steering/default/hkpvehicledefaultsteering.h
//
// Havok 2014.2.5: full declaration from rfg.pdb (stub_impl.md S8).
#pragma once

#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/memory/memoryclasses/hkmemoryclasses.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/container/array/hkarray.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/container/hkcontainerallocators.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/types/hkbasetypes.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/vehicle/driverinput/hkpvehicledriverinput.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/vehicle/steering/hkpvehiclesteering.h"

class hkClass;
struct hkInternalClassMember;
class hkpVehicleInstance;

// size 0x1C
class hkpVehicleDefaultSteering : public hkpVehicleSteering
{
public:
    HK_DECLARE_CLASS_ALLOCATOR(); // new(unsigned int nbytes) 0x00B71A70, delete(void* p, unsigned int nbytes) 0x00B9D860
    static const hkClass& staticClass();
    hkpVehicleDefaultSteering(hkFinishLoadedObjectFlag);
    hkpVehicleDefaultSteering(); // 0x00F83A70
    virtual ~hkpVehicleDefaultSteering(); // vtbl+0x00 0x00B9B2A0
    virtual void calcSteering(const float deltaTime, const hkpVehicleInstance* vehicle, const hkpVehicleDriverInput::FilteredDriverInputOutput& filteredInfoOutput, hkpVehicleSteering::SteeringAnglesOutput& steeringOutput); // vtbl+0x10 0x00F83960
    virtual void calcMainSteeringAngle(const float deltaTime, const hkpVehicleInstance* vehicle, const hkpVehicleDriverInput::FilteredDriverInputOutput& filteredInfoOutput, hkpVehicleSteering::SteeringAnglesOutput& steeringOutput); // vtbl+0x14 0x00F839C0

    static const hkInternalClassMember Members[];
    float m_maxSteeringAngle; // 0x08
    float m_maxSpeedFullSteeringAngle; // 0x0C
    hkArray<hkBool,hkContainerHeapAllocator> m_doesWheelSteer; // 0x10
};
