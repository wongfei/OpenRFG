// Original: volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/vehicle/brake/default/hkpvehicledefaultbrake.h
//
// Havok 2014.2.5: full declaration from rfg.pdb (stub_impl.md S8).
#pragma once

#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/memory/memoryclasses/hkmemoryclasses.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/container/array/hkarray.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/container/hkcontainerallocators.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/types/hkbasetypes.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/vehicle/brake/hkpvehiclebrake.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/vehicle/driverinput/hkpvehicledriverinput.h"

class hkClass;
struct hkInternalClassMember;
class hkpVehicleInstance;

// size 0x18
class hkpVehicleDefaultBrake : public hkpVehicleBrake
{
public:
    // size 0xC
    struct WheelBrakingProperties
    {
        HK_DECLARE_NONVIRTUAL_CLASS_ALLOCATOR(WheelBrakingProperties);
        static const hkClass& staticClass();
        WheelBrakingProperties(); // 0x00B70D80

        static const hkInternalClassMember Members[];
        float m_maxBreakingTorque; // 0x00
        float m_minPedalInputToBlock; // 0x04
        hkBool m_isConnectedToHandbrake; // 0x08
    };

    HK_DECLARE_CLASS_ALLOCATOR(); // new(unsigned int nbytes) 0x00B71840, delete(void* p, unsigned int nbytes) 0x00B9B210
    static const hkClass& staticClass();
    hkpVehicleDefaultBrake(hkFinishLoadedObjectFlag);
    hkpVehicleDefaultBrake(); // 0x00F80C60
    virtual ~hkpVehicleDefaultBrake(); // vtbl+0x00 0x00B9B1A0
    virtual void calcBreakingInfo(const float deltaTime, const hkpVehicleInstance* vehicle, const hkpVehicleDriverInput::FilteredDriverInputOutput& filteredDriverInfo, hkpVehicleBrake::WheelBreakingOutput& breakingInfo); // vtbl+0x10 0x00F80AA0

    static const hkInternalClassMember Members[];
    hkArray<WheelBrakingProperties,hkContainerHeapAllocator> m_wheelBrakingProperties; // 0x08
    float m_wheelsMinTimeToBlock; // 0x14
};
