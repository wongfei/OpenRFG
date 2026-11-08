// Original: volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/dynamics/world/extensions/hkpworldextension.cpp
//
// Havok 2014.2.5 (havok_impl.md HK7): the world extension base, from rfg.exe, with the out-of-line copies of
// hkpworldextension.inl. requireExtension<hkpCollisionCallbackUtil> comes with the collision callback util (HK8).

#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/dynamics/world/extensions/hkpworldextension.h"

// 0x00E1F920
hkpWorldExtension::hkpWorldExtension(int id)
{
    m_world = nullptr;
    m_id = id;
}

// 0x00DE88F0
hkpWorldExtension::~hkpWorldExtension()
{
}

// 0x00DE88E0
int hkpWorldExtension::getId() const
{
    return m_id;
}

// 0x00DEAA10 (hkpworldextension.inl): the world holds a reference while the extension is attached
void hkpWorldExtension::addedToWorld(hkpWorld* world)
{
    addReference();
    m_world = world;
    performAttachments(world);
}

// 0x00DEAA60 (hkpworldextension.inl)
void hkpWorldExtension::removedFromWorld(hkpWorld* world)
{
    performDetachments(world);
    m_world = nullptr;
    removeReference();
}
