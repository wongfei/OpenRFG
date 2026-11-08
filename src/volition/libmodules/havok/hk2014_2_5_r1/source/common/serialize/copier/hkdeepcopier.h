// Original: volition/libmodules/havok/hk2014_2_5_r1/source/common/serialize/copier/hkdeepcopier.h
//
// Havok 2014.2.5: full declaration from rfg.pdb (stub_impl.md S4, serializer).
#pragma once

#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/container/pointermap/hkpointermap.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/types/hkbasetypes.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/serialize/copier/hkobjectcopier.h"

class hkClass;
struct hkContainerHeapAllocator;
class hkTypeInfoRegistry;
class hkVtableClassRegistry;

namespace hkDeepCopier
{
    void* deepCopy(const void* dataIn, const hkClass& klassIn, hkPointerMap<void const*,void*,hkContainerHeapAllocator>* previousCopies, hkVtableClassRegistry* vtableClassRegistry, hkTypeInfoRegistry* loadedObjectRegistry, hkFlags<enum hkObjectCopier::ObjectCopierFlagBits,unsigned int> flags); // 0x00F7AF80
    hkResult freeDeepCopy(void* data); // 0x00F7AF00
}
