// Original: volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/dynamics/action/hkpaction.cpp
//
// Havok 2014.2.5 (havok_impl.md HK7): the action base (applied each step to the entities of its island), from
// rfg.exe, with the out-of-line copies of hkpaction.h/.inl.

#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/dynamics/action/hkpaction.h"

// 0x00DF5B30: not in a world or island yet
hkpAction::hkpAction(unsigned long userData)
{
    m_world = nullptr;
    m_island = nullptr;
    m_userData = userData;
}

// 0x00DEFFE0
hkpAction::hkpAction(hkFinishLoadedObjectFlag flag)
    : hkReferencedObject(flag)
    , m_name(flag)
{
}

// 0x00DEFFC0
hkpAction::~hkpAction()
{
}

// 0x00818120 (hkpaction.h): an action has no phantoms by default
void hkpAction::getPhantoms(hkArray<hkpPhantom*, hkContainerHeapAllocator>& phantomsOut)
{
}

// 0x00DE8790, 0x00DE8770, 0x00E0A880, 0x00DE8780
hkpWorld* hkpAction::getWorld()
{
    return m_world;
}

hkpSimulationIsland* hkpAction::getSimulationIsland()
{
    return m_island;
}

void hkpAction::setSimulationIsland(hkpSimulationIsland* isl)
{
    m_island = isl;
}

void hkpAction::setWorld(hkpWorld* world)
{
    m_world = world;
}
