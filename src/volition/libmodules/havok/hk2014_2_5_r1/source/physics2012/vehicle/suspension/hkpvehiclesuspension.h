// Original: volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/vehicle/suspension/hkpvehiclesuspension.h
//
// Havok 2014.2.5: full declaration from rfg.pdb (stub_impl.md S8).
#pragma once

#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/memory/memoryclasses/hkmemoryclasses.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/container/array/hkarray.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/container/hkcontainerallocators.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/math/vector/hkvector4f.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/object/hkreferencedobject.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/vehicle/wheelcollide/hkpvehiclewheelcollide.h"

class hkClass;
class hkFinishLoadedObjectFlag;
struct hkInternalClassMember;
class hkpVehicleInstance;

// size 0x14
class hkpVehicleSuspension : public hkReferencedObject
{
public:
    // size 0x30
    struct alignas(16) SuspensionWheelParameters
    {
        HK_DECLARE_NONVIRTUAL_CLASS_ALLOCATOR(SuspensionWheelParameters);
        static const hkClass& staticClass();

        static const hkInternalClassMember Members[];
        hkVector4f m_hardpointChassisSpace; // 0x00
        hkVector4f m_directionChassisSpace; // 0x10
        float m_length; // 0x20
    };

    HK_DECLARE_CLASS_ALLOCATOR(); // delete(void* p, unsigned int nbytes) 0x00B9AF90
    static const hkClass& staticClass();
    hkpVehicleSuspension(hkFinishLoadedObjectFlag);
    hkpVehicleSuspension(); // 0x00B9AF70 (.h:33)
    virtual ~hkpVehicleSuspension(); // vtbl+0x00 0x00B9AFD0
    virtual void calcSuspension(const float, const hkpVehicleInstance*, const hkpVehicleWheelCollide::CollisionDetectionWheelOutput*, float*) = 0; // vtbl+0x10

    static const hkInternalClassMember Members[];
    hkArray<SuspensionWheelParameters,hkContainerHeapAllocator> m_wheelParams; // 0x08
};
