// Original: volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/vehicle/suspension/default/hkpvehicledefaultsuspension.h
//
// Havok 2014.2.5: full declaration from rfg.pdb (stub_impl.md S8).
#pragma once

#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/memory/memoryclasses/hkmemoryclasses.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/container/array/hkarray.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/container/hkcontainerallocators.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/vehicle/suspension/hkpvehiclesuspension.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/vehicle/wheelcollide/hkpvehiclewheelcollide.h"

class hkClass;
class hkFinishLoadedObjectFlag;
struct hkInternalClassMember;
class hkpVehicleInstance;

// size 0x20
class hkpVehicleDefaultSuspension : public hkpVehicleSuspension
{
public:
    // size 0xC
    struct WheelSpringSuspensionParameters
    {
        HK_DECLARE_NONVIRTUAL_CLASS_ALLOCATOR(WheelSpringSuspensionParameters);
        static const hkClass& staticClass();

        static const hkInternalClassMember Members[];
        float m_strength; // 0x00
        float m_dampingCompression; // 0x04
        float m_dampingRelaxation; // 0x08
    };

    HK_DECLARE_CLASS_ALLOCATOR(); // new(unsigned int nbytes) 0x00B71A90, delete(void* p, unsigned int nbytes) 0x00B9B320
    static const hkClass& staticClass();
    hkpVehicleDefaultSuspension(hkFinishLoadedObjectFlag);
    hkpVehicleDefaultSuspension(); // 0x00B9B2F0 (.h:25)
    virtual ~hkpVehicleDefaultSuspension(); // vtbl+0x00 0x00B9B360
    virtual void calcSuspension(const float deltaTime, const hkpVehicleInstance* vehicle, const hkpVehicleWheelCollide::CollisionDetectionWheelOutput* cdInfo, float* suspensionForceOut); // vtbl+0x10 0x00F80DD0

    static const hkInternalClassMember Members[];
    hkArray<WheelSpringSuspensionParameters,hkContainerHeapAllocator> m_wheelSpringParams; // 0x14
};
