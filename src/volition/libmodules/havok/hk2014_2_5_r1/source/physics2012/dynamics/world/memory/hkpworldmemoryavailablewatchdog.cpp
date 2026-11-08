// Original: volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/dynamics/world/memory/hkpworldmemoryavailablewatchdog.cpp
//
// Havok 2014.2.5 (havok_impl.md HK1): the base of the world memory watch dog (the game derives
// havok_memory_watch_dog from it), with the inline functions of hkpworldmemoryavailablewatchdog.inl.

#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/dynamics/world/memory/hkpworldmemoryavailablewatchdog.h"

#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/types/hkbasetypes.h"

// 0x0080CF10 (.inl:10)
hkWorldMemoryAvailableWatchDog::hkWorldMemoryAvailableWatchDog()
{
}

// 0x00DEFC50 (.inl:16)
hkWorldMemoryAvailableWatchDog::hkWorldMemoryAvailableWatchDog(hkFinishLoadedObjectFlag flag)
    : hkReferencedObject(flag)
{
}

// 0x0080CF20
hkWorldMemoryAvailableWatchDog::~hkWorldMemoryAvailableWatchDog()
{
}
