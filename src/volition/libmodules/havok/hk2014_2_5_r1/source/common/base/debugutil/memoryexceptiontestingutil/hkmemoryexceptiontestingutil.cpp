// Original: volition/libmodules/havok/hk2014_2_5_r1/source/common/base/debugutil/memoryexceptiontestingutil/hkmemoryexceptiontestingutil.cpp
//
// Havok 2014.2.5 (havok_impl.md HK1): the out-of-memory state (the memory exception testing of the SDK is not used).

#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/debugutil/memoryexceptiontestingutil/hkmemoryexceptiontestingutil.h"

// ---- globals
hkMemoryAllocator::MemoryState hkOutOfMemoryState; // 0x03A26FFC

// 0x00CF25D0 (:22)
void hkSetOutOfMemoryState(hkMemoryAllocator::MemoryState state)
{
    hkOutOfMemoryState = state;
}
