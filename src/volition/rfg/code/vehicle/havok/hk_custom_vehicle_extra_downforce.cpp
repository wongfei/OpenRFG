// Original: volition/rfg/code/vehicle/havok/hk_custom_vehicle_extra_downforce.cpp
//
// An action that pushes a vehicle's chassis while every wheel has a contact body.

#include "volition/rfg/code/vehicle/havok/hk_custom_vehicle_extra_downforce.h"

#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/dynamics/entity/hkprigidbody.h"
#include "volition/rfg/code/vehicle/havok/hk_custom_vehicle_instance.h"

// 0x00B63AA0 (:24)
hk_custom_vehicle_extra_downforce::hk_custom_vehicle_extra_downforce(hkpEntity* hk_entity)
    : hkpUnaryAction(hk_entity, 0)
{
    m_acceleration = 0.0f;
    m_vehicle = NULL;
    m_enable = true;
}

// 0x00B63AF0 (:34)
hk_custom_vehicle_extra_downforce::hk_custom_vehicle_extra_downforce(hk_custom_vehicle_instance* p_vehicle)
    : hkpUnaryAction(p_vehicle->m_entity, 0)
{
    m_vehicle = p_vehicle;
    m_acceleration = 0.0f;
    m_enable = true;
}

// 0x00B63AE0
hk_custom_vehicle_extra_downforce::~hk_custom_vehicle_extra_downforce()
{
}

// 0x00B74440 (:45). The vehicle is null with the entity constructor, which only clone uses, and is read here unchecked.
void hk_custom_vehicle_extra_downforce::applyAction(const hkStepInfo& step_info)
{
    if (!m_enable || m_acceleration == 0.0f)
    {
        return;
    }
    const int num_wheels = m_vehicle->m_wheelsInfo.getSize();
    int num_contacts = 0;
    for (int w = 0; w < num_wheels; w++)
    {
        if (m_vehicle->m_wheelsInfo[w].m_contactBody != NULL)
        {
            num_contacts++;
        }
    }
    if (num_contacts == num_wheels)
    {
        hkpEntity* entity = m_vehicle->m_entity;
        hkVector4f force;
        force.set(0.0f, static_cast<hkpRigidBody*>(entity)->getMass() * m_acceleration, 0.0f, 0.0f);
        entity->activate();
        entity->getMotion()->applyForce(step_info.m_deltaTime, force);
    }
}

// 0x00B74550 (:106). rfg.exe prints "Int3 critical function" first (a debug trap left in), and copies into the new object
// even if the allocation failed.
hkpAction* hk_custom_vehicle_extra_downforce::clone(const hkArray<hkpEntity*,hkContainerHeapAllocator>& new_entities, const hkArray<hkpPhantom*,hkContainerHeapAllocator>& new_phantoms) const
{
    hk_custom_vehicle_extra_downforce* action = new hk_custom_vehicle_extra_downforce(new_entities[0]);
    action->m_acceleration = m_acceleration;
    action->m_enable = m_enable;
    return action;
}

// 0x00B63AD0 (.h:68)
void hk_custom_vehicle_extra_downforce::enable(bool enable)
{
    m_enable = enable;
}
