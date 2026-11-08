// Original: volition/libmodules/havok/hk2014_2_5_r1/source/common/serialize/util/hkobjectinspector.h
//
// Havok 2014.2.5: full declaration from rfg.pdb (stub_impl.md S4, serializer).
#pragma once

#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/memory/memoryclasses/hkmemoryclasses.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/container/array/hkarray.h"

class hkClass;
struct hkContainerTempAllocator;
struct hkResult;

namespace hkObjectInspector
{
    // size 0x8
    struct Pointer
    {
        HK_DECLARE_NONVIRTUAL_CLASS_ALLOCATOR(Pointer);

        void** location; // 0x00
        const hkClass* klass; // 0x04
    };

    // size 0x4
    class ObjectListener
    {
    public:
        HK_DECLARE_PLACEMENT_ALLOCATOR(); // delete(void*, unsigned int) 0x00FAB640
        virtual ~ObjectListener() {} // vtbl+0x00 0x00FAB630 (.h:34)
        virtual hkResult objectCallback(const void*, const hkClass&, hkArray<Pointer,hkContainerTempAllocator>&) = 0; // vtbl+0x04
        ObjectListener() {} // 0x00FAB660

        // 0x00 vfptr
    };
}

namespace hkObjectInspector
{
    hkResult getPointers(const void* object, const hkClass& klass, hkArray<hkObjectInspector::Pointer,hkContainerTempAllocator>& outList); // 0x01057640
    hkResult walkPointers(const void* object, const hkClass& klass, hkObjectInspector::ObjectListener* listener); // 0x01057670
}
