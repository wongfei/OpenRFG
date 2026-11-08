// Original: volition/rfg/code/vehicle/havok/hk_wheel_info_contact_body_listener.cpp
//
// Clears a wheel's contact body when that body is removed from the world or deleted.

#include "volition/rfg/code/vehicle/havok/hk_wheel_info_contact_body_listener.h"

#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/dynamics/entity/hkprigidbody.h"

// 0x00B75BE0 (:17)
hk_wheel_info_contact_body_listener::hk_wheel_info_contact_body_listener(hkpVehicleInstance::WheelInfo* wheel_info)
{
    m_wheel_info = wheel_info;
    m_contact_body = NULL;
}

// 0x00B64070 (:24)
hk_wheel_info_contact_body_listener::~hk_wheel_info_contact_body_listener()
{
    if (m_contact_body != NULL)
    {
        m_contact_body->removeEntityListener(this);
    }
    m_contact_body = NULL;
    m_wheel_info = NULL;
}

// 0x00B640A0 (:39)
void hk_wheel_info_contact_body_listener::entityDeletedCallback(hkpEntity* entity)
{
    m_contact_body = NULL;
    m_wheel_info->m_contactBody = NULL;
}

// 0x00B640B0 (:52)
void hk_wheel_info_contact_body_listener::entityRemovedCallback(hkpEntity* entity)
{
    m_contact_body = NULL;
    m_wheel_info->m_contactBody = NULL;
    entity->removeEntityListener(this);
}

// 0x00B640D0 (:67)
void hk_wheel_info_contact_body_listener::update_contact_body()
{
    if (m_contact_body != m_wheel_info->m_contactBody)
    {
        if (m_contact_body != NULL)
        {
            m_contact_body->removeEntityListener(this);
        }
        if (m_wheel_info->m_contactBody != NULL)
        {
            m_wheel_info->m_contactBody->addEntityListener(this);
        }
        m_contact_body = m_wheel_info->m_contactBody;
    }
}
