// Original: volition/rfg/code/vehicle/havok/hk_wheel_info_contact_body_listener.h
// Implementation: volition/rfg/code/vehicle/havok/hk_wheel_info_contact_body_listener.cpp
//
// Clears a wheel's contact body when that body is removed from the world or deleted.
#pragma once

#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/dynamics/entity/hkpentitylistener.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/vehicle/hkpvehicleinstance.h"

class hkpEntity;
class hkpRigidBody;

// size 0xC
class hk_wheel_info_contact_body_listener : public hkpEntityListener
{
public:
    hk_wheel_info_contact_body_listener(hkpVehicleInstance::WheelInfo* wheel_info); // 0x00B75BE0 (:17)
    virtual ~hk_wheel_info_contact_body_listener(); // vtbl+0x00 0x00B64070 (:24)
    virtual void entityDeletedCallback(hkpEntity* entity); // vtbl+0x14 0x00B640A0 (:39)
    virtual void entityRemovedCallback(hkpEntity* entity); // vtbl+0x08 0x00B640B0 (:52)
    virtual void update_contact_body(); // vtbl+0x18 0x00B640D0 (:67)

protected:
    hkpVehicleInstance::WheelInfo* m_wheel_info; // 0x04
    hkpRigidBody* m_contact_body; // 0x08
};
