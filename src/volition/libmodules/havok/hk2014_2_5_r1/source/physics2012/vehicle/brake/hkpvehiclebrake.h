// Original: volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/vehicle/brake/hkpvehiclebrake.h
//
// Havok 2014.2.5: full declaration from rfg.pdb (stub_impl.md S8).
#pragma once

#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/memory/memoryclasses/hkmemoryclasses.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/container/array/hkarray.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/container/hkcontainerallocators.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/object/hkreferencedobject.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/types/hkbasetypes.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/vehicle/driverinput/hkpvehicledriverinput.h"

class hkClass;
struct hkInternalClassMember;
class hkpVehicleInstance;

// size 0x8
class hkpVehicleBrake : public hkReferencedObject
{
public:
    // size 0xBC
    struct WheelBreakingOutput
    {
        HK_DECLARE_NONVIRTUAL_CLASS_ALLOCATOR(WheelBreakingOutput);
        WheelBreakingOutput(); // 0x00BA7BC0
        ~WheelBreakingOutput(); // 0x00BA7BF0

        hkInplaceArray<float,32,hkContainerHeapAllocator> m_brakingTorque; // 0x00
        hkInplaceArray<hkBool,32,hkContainerHeapAllocator> m_isFixed; // 0x8C
        float m_wheelsTimeSinceMaxPedalInput; // 0xB8
    };

    HK_DECLARE_CLASS_ALLOCATOR(); // delete(void* p, unsigned int nbytes) 0x00F80CC0
    static const hkClass& staticClass();
    virtual void calcBreakingInfo(const float, const hkpVehicleInstance*, const hkpVehicleDriverInput::FilteredDriverInputOutput&, WheelBreakingOutput&) = 0; // vtbl+0x10
public:
    hkpVehicleBrake(); // 0x00F80C90
public:
    hkpVehicleBrake(hkFinishLoadedObjectFlag);
    virtual ~hkpVehicleBrake(); // vtbl+0x00 0x00B63180

    static const hkInternalClassMember Members[];
};
