// Original: volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/dynamics/action/hkpunaryaction.cpp
//
// Havok 2014.2.5 (havok_impl.md HK7): the action on one entity (it holds a reference on it), from rfg.exe.

#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/dynamics/action/hkpunaryaction.h"

#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/dynamics/entity/hkpentity.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/dynamics/world/hkpworld.h"

// 0x00DF59E0
hkpUnaryAction::hkpUnaryAction(hkpEntity* entity, unsigned long userData)
    : hkpAction(userData)
{
    m_entity = entity;
    if (entity != nullptr)
    {
        entity->addReference();
    }
}

// 0x00F15070 (hkpunaryaction.h)
hkpUnaryAction::hkpUnaryAction(hkFinishLoadedObjectFlag flag)
    : hkpAction(flag)
{
}

// 0x00DF5A30
hkpUnaryAction::~hkpUnaryAction()
{
    if (m_entity != nullptr)
    {
        m_entity->removeReference();
        m_entity = nullptr;
    }
}

// 0x00818130 (hkpunaryaction.h): the reflection class (hkpUnaryActionClass) is not part of this port
const hkClass* hkpUnaryAction::getClassType() const
{
    return nullptr;
}

// 0x00DF59C0: the action leaves the world with its entity
void hkpUnaryAction::entityRemovedCallback(hkpEntity* entity)
{
    if (m_world != nullptr)
    {
        m_world->removeActionImmediately(this);
    }
}

// 0x00DF5AF0
void hkpUnaryAction::getEntities(hkArray<hkpEntity*, hkContainerHeapAllocator>& entitiesOut)
{
    entitiesOut.pushBack(m_entity);
}

// 0x00B63020 (hkpunaryaction.inl)
hkpEntity* hkpUnaryAction::getEntity()
{
    return m_entity;
}

// 0x00DF5A70
void hkpUnaryAction::setEntity(hkpEntity* entity)
{
    if (m_entity != nullptr)
    {
        if (m_world != nullptr)
        {
            m_world->detachActionFromEntity(this, m_entity);
        }
        m_entity->removeReference();
        m_entity = nullptr;
    }
    m_entity = entity;
    entity->addReference();
    if (m_world != nullptr)
    {
        m_world->attachActionToEntity(this, m_entity);
    }
}
