// Original: volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/dynamics/entity/hkpentitylistener.h
//
// Havok 2014.2.5 (player_minimal.md P1): the listener base, from rfg.exe (inline in the SDK header).

#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/dynamics/entity/hkpentitylistener.h"

#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/dynamics/entity/hkpentity.h"

// 0x00B64060
hkpEntityListener::hkpEntityListener()
{
}

// 0x00B62FE0
hkpEntityListener::~hkpEntityListener()
{
}

// 0x00833A40
void hkpEntityListener::entityAddedCallback(hkpEntity* entity)
{
}

// 0x00B62FF0
void hkpEntityListener::entityRemovedCallback(hkpEntity* entity)
{
}

// 0x00833A50: a shape change in the world reads as a remove and an add
void hkpEntityListener::entityShapeSetCallback(hkpEntity* entity)
{
    if (entity->m_world != nullptr)
    {
        entityRemovedCallback(entity);
        entityAddedCallback(entity);
    }
}

// 0x00833A80
void hkpEntityListener::entitySetMotionTypeCallback(hkpEntity* entity)
{
}

// 0x00833A90
void hkpEntityListener::entityDeletedCallback(hkpEntity* entity)
{
}
