// Original: volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/vehicle/wheelcollide/hkpvehiclewheelcollide.h
//
// Havok 2014.2.5: full declaration from rfg.pdb (stub_impl.md S8).
#pragma once

#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/memory/memoryclasses/hkmemoryclasses.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/container/array/hkarray.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/object/hkreferencedobject.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/types/hkbasetypes.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/types/physics/contactpoint/hkcontactpoint.h"

class hkClass;
struct hkContainerHeapAllocator;
struct hkInternalClassMember;
class hkpPhantom;
class hkpRigidBody;
class hkpVehicleInstance;
class hkpWorld;

// size 0xC
class hkpVehicleWheelCollide : public hkReferencedObject
{
public:
    enum WheelCollideType
    {
        INVALID_WHEEL_COLLIDE = 0,
        RAY_CAST_WHEEL_COLLIDE = 1,
        LINEAR_CAST_WHEEL_COLLIDE = 2,
        USER_WHEEL_COLLIDE1 = 3,
        USER_WHEEL_COLLIDE2 = 4,
        USER_WHEEL_COLLIDE3 = 5,
        USER_WHEEL_COLLIDE4 = 6,
        USER_WHEEL_COLLIDE5 = 7,
    };

    // size 0x60
    struct alignas(16) CollisionDetectionWheelOutput
    {
        enum
        {
            MAX_NUM_SHAPE_KEYS = 8,
        };

        HK_DECLARE_NONVIRTUAL_CLASS_ALLOCATOR(CollisionDetectionWheelOutput);

        hkContactPoint m_contactPoint; // 0x00
        float m_contactFriction; // 0x20
        hkpRigidBody* m_contactBody; // 0x24
        unsigned int m_contactShapeKey[8]; // 0x28
        float m_currentSuspensionLength; // 0x48
        float m_suspensionClosingSpeed; // 0x4C
        float m_suspensionScalingFactor; // 0x50
    };

    HK_DECLARE_CLASS_ALLOCATOR(); // delete(void* p, unsigned int nbytes) 0x00F7F290
    static const hkClass& staticClass();
    virtual ~hkpVehicleWheelCollide(); // vtbl+0x00 0x00F7F210
    virtual void init(const hkpVehicleInstance*) = 0; // vtbl+0x10
    virtual void collideWheels(const float, const hkpVehicleInstance*, CollisionDetectionWheelOutput*) = 0; // vtbl+0x14
    virtual void getPhantoms(hkArray<hkpPhantom*,hkContainerHeapAllocator>&) = 0; // vtbl+0x18
    virtual void updateBeforeCollisionDetection(const hkpVehicleInstance*) = 0; // vtbl+0x1C
    virtual hkpVehicleWheelCollide* clone(const hkpRigidBody&, const hkArray<hkpPhantom*,hkContainerHeapAllocator>&) const = 0; // vtbl+0x20
    virtual void addToWorld(hkpWorld*) = 0; // vtbl+0x24
    virtual void removeFromWorld() = 0; // vtbl+0x28
    virtual void setCollisionFilterInfo(unsigned int) = 0; // vtbl+0x2C
    virtual void wheelCollideCallback(const hkpVehicleInstance* vehicle, unsigned char wheelIndex, CollisionDetectionWheelOutput& cdInfo); // vtbl+0x30 0x00BAFF30 (.h:101)
    WheelCollideType getType() const;
    hkpVehicleWheelCollide(hkFinishLoadedObjectFlag);
public:
    hkpVehicleWheelCollide(); // 0x00F7F270
public:
    void afterReflectNew();

    static const hkInternalClassMember Members[];
    hkBool m_alreadyUsed; // 0x08
    hkEnum<enum WheelCollideType,unsigned char> m_type; // 0x09
};
