// Original: volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/vehicle/simulation/hkpvehiclesimulation.h
//
// Havok 2014.2.5: full declaration from rfg.pdb (stub_impl.md S8).
#pragma once

#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/memory/memoryclasses/hkmemoryclasses.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/container/array/hkarray.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/object/hkreferencedobject.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/vehicle/aerodynamics/hkpvehicleaerodynamics.h"

class hkClass;
struct hkContainerHeapAllocator;
class hkFinishLoadedObjectFlag;
struct hkInternalClassMember;
class hkStepInfo;
class hkpVehicleInstance;
struct hkpVehicleJobResults;
class hkpWorld;

// size 0x8
class hkpVehicleSimulation : public hkReferencedObject
{
public:
    // size 0xC
    struct SimulationInput
    {
        SimulationInput(const hkpVehicleAerodynamics::AerodynamicsDragOutput& aerodynamics, const hkArray<float,hkContainerHeapAllocator>& suspensions, const hkArray<float,hkContainerHeapAllocator>& linearForces); // 0x00F82E20

        const hkpVehicleAerodynamics::AerodynamicsDragOutput& aerodynamicsDragInfo; // 0x00
        const hkArray<float,hkContainerHeapAllocator>& suspensionForceAtWheel; // 0x04
        const hkArray<float,hkContainerHeapAllocator>& totalLinearForceAtWheel; // 0x08
    };

    HK_DECLARE_CLASS_ALLOCATOR(); // delete(void* p, unsigned int nbytes) 0x00B71560
    static const hkClass& staticClass();
    virtual void init(hkpVehicleInstance*) = 0; // vtbl+0x10
    virtual hkpVehicleSimulation* clone(hkpVehicleInstance*) = 0; // vtbl+0x14
    virtual void addToWorld(hkpWorld* world); // vtbl+0x18 0x00B63290 (.h:47)
    virtual void removeFromWorld(); // vtbl+0x1C 0x00B632A0 (.h:48)
    virtual void simulateVehicle(hkpVehicleInstance*, const hkStepInfo&, const SimulationInput&, hkpVehicleJobResults*) = 0; // vtbl+0x20
public:
    hkpVehicleSimulation(); // 0x00B63280 (.h:57)
public:
    hkpVehicleSimulation(hkFinishLoadedObjectFlag);
    virtual ~hkpVehicleSimulation(); // vtbl+0x00 0x00B632B0

    static const hkInternalClassMember Members[];
};
