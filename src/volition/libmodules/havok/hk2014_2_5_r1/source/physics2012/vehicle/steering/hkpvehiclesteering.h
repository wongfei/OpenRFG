// Original: volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/vehicle/steering/hkpvehiclesteering.h
//
// Havok 2014.2.5: full declaration from rfg.pdb (stub_impl.md S8).
#pragma once

#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/memory/memoryclasses/hkmemoryclasses.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/container/array/hkarray.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/container/hkcontainerallocators.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/object/hkreferencedobject.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/vehicle/driverinput/hkpvehicledriverinput.h"

class hkClass;
class hkFinishLoadedObjectFlag;
struct hkInternalClassMember;
class hkpVehicleInstance;

// size 0x8
class hkpVehicleSteering : public hkReferencedObject
{
public:
    // size 0x94
    struct SteeringAnglesOutput
    {
        HK_DECLARE_NONVIRTUAL_CLASS_ALLOCATOR(SteeringAnglesOutput);
        SteeringAnglesOutput(); // 0x00BA7B60
        ~SteeringAnglesOutput(); // 0x00BA7B80

        float m_mainSteeringAngle; // 0x00
        float m_mainSteeringAngleAssumingNoReduction; // 0x04
        hkInplaceArray<float,32,hkContainerHeapAllocator> m_wheelsSteeringAngle; // 0x08
    };

    HK_DECLARE_CLASS_ALLOCATOR(); // delete(void* p, unsigned int nbytes) 0x00F83AB0
    static const hkClass& staticClass();
    virtual void calcSteering(const float, const hkpVehicleInstance*, const hkpVehicleDriverInput::FilteredDriverInputOutput&, SteeringAnglesOutput&) = 0; // vtbl+0x10
public:
    hkpVehicleSteering(); // 0x00F83AA0
public:
    hkpVehicleSteering(hkFinishLoadedObjectFlag);
    virtual ~hkpVehicleSteering(); // vtbl+0x00 0x00B631B0

    static const hkInternalClassMember Members[];
};
