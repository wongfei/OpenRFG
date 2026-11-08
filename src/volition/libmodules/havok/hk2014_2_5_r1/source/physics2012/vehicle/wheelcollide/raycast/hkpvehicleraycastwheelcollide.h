// Original: volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/vehicle/wheelcollide/raycast/hkpvehicleraycastwheelcollide.h
//
// Havok 2014.2.5: full declaration from rfg.pdb (stub_impl.md S8).
#pragma once

#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/memory/memoryclasses/hkmemoryclasses.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/container/array/hkarray.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/vehicle/hkpvehicleinstance.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/vehicle/wheelcollide/hkpvehiclewheelcollide.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/vehicle/wheelcollide/rejectchassislistener/hkprejectchassislistener.h"

class hkAabb;
class hkClass;
struct hkContainerHeapAllocator;
class hkFinishLoadedObjectFlag;
struct hkInternalClassMember;
class hkpAabbPhantom;
class hkpCollisionFilter;
class hkpPhantom;
class hkpRigidBody;
struct hkpShapeRayCastCommand;
class hkpWorld;
struct hkpWorldRayCastOutput;

// size 0x24
class hkpVehicleRayCastWheelCollide : public hkpVehicleWheelCollide
{
public:
    HK_DECLARE_CLASS_ALLOCATOR(); // new(unsigned int nbytes) 0x00B71AB0, delete(void* p, unsigned int nbytes) 0x00BAFF50
    static const hkClass& staticClass();
    hkpVehicleRayCastWheelCollide(hkFinishLoadedObjectFlag);
    hkpVehicleRayCastWheelCollide(); // 0x00F7EE20
    virtual ~hkpVehicleRayCastWheelCollide(); // vtbl+0x00 0x00F7EA20
    virtual void init(const hkpVehicleInstance* vehicle); // vtbl+0x10 0x00F7EE60
    virtual void collideWheels(const float deltaTime, const hkpVehicleInstance* vehicle, hkpVehicleWheelCollide::CollisionDetectionWheelOutput* cdInfoOut); // vtbl+0x14 0x00F7EB90
    virtual void getPhantoms(hkArray<hkpPhantom*,hkContainerHeapAllocator>& phantomsOut); // vtbl+0x18 0x00F7F1C0
    virtual void updateBeforeCollisionDetection(const hkpVehicleInstance* vehicle); // vtbl+0x1C 0x00F7EB00
    virtual hkpVehicleWheelCollide* clone(const hkpRigidBody& newChassis, const hkArray<hkpPhantom*,hkContainerHeapAllocator>& newPhantoms) const; // vtbl+0x20 0x00F7EEF0
    virtual void addToWorld(hkpWorld* world); // vtbl+0x24 0x00F7EA00
    virtual void removeFromWorld(); // vtbl+0x28 0x00F7EDF0
    virtual void setCollisionFilterInfo(unsigned int filterInfo); // vtbl+0x2C 0x00F7EE00
    virtual int buildRayCastCommands(const hkpVehicleInstance* vehicle, const hkpCollisionFilter* collisionFilter, int filterSize, hkpShapeRayCastCommand* commandStorage, hkpWorldRayCastOutput* outputStorage) const; // vtbl+0x34 0x00F7ED00
    virtual void calcWheelsAABB(const hkpVehicleInstance* vehicle, hkAabb& aabbOut) const; // vtbl+0x38 0x00F7EA70
    virtual void castSingleWheel(const hkpVehicleInstance::WheelInfo& wheelInfo, hkpWorldRayCastOutput& output) const; // vtbl+0x3C 0x00F7EB40
    virtual void getCollisionOutputFromCastResult(const hkpVehicleInstance* vehicle, unsigned char wheelInfoNum, const hkpWorldRayCastOutput& rayCastOutput, hkpVehicleWheelCollide::CollisionDetectionWheelOutput& cdInfo) const; // vtbl+0x40 0x00F7EFA0
    virtual void getCollisionOutputWithoutHit(const hkpVehicleInstance* vehicle, unsigned char wheelNum, hkpVehicleWheelCollide::CollisionDetectionWheelOutput& cdInfo) const; // vtbl+0x44 0x00F7EC70
    void afterReflectNew();

    static const hkInternalClassMember Members[];
    unsigned int m_wheelCollisionFilterInfo; // 0x0C
    hkpAabbPhantom* m_phantom; // 0x10
    hkpRejectChassisListener m_rejectRayChassisListener; // 0x14
};
