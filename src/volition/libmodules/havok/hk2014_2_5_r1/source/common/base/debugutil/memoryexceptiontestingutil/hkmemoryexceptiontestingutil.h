// Original: volition/libmodules/havok/hk2014_2_5_r1/source/common/base/debugutil/memoryexceptiontestingutil/hkmemoryexceptiontestingutil.h
//
// Havok 2014.2.5 (havok_impl.md HK1): the out-of-memory state. The heap allocator sets it when the bytes in use reach
// its soft limit; the game resets it after each Havok step (havok_update_status). Declaration as Havok 2014.1
// hkMemoryExceptionTestingUtil.h.
// Implementation: hkmemoryexceptiontestingutil.cpp.
#pragma once

#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/memory/allocator/hkmemoryallocator.h"

extern hkMemoryAllocator::MemoryState hkOutOfMemoryState; // 0x03A26FFC
void hkSetOutOfMemoryState(hkMemoryAllocator::MemoryState state); // 0x00CF25D0 (:22)
