// Original: volition/libmodules/havok/hk2014_2_5_r1/source/common/serialize/copier/hkdeepcopier.cpp
//
// Havok 2014.2.5 (stub_impl.md S4, serializer SE), from rfg.exe: a copy of an object and every object it reaches
// (through pointers with a class) in one heap block, laid out by hkObjectCopier in the host layout. The block ends
// with the finished objects' {type info, object} pairs and their count (0 without a type registry), which
// freeDeepCopy uses to clean them up.

#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/serialize/copier/hkdeepcopier.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/serialize/resource/hkobjectresource.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/container/string/hkstring.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/memory/router/hkmemoryrouter.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/reflection/attributes/hkattributes.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/reflection/hkclass.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/reflection/hktypeinfo.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/reflection/registry/hktypeinforegistry.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/reflection/registry/hkvtableclassregistry.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/system/io/ostream/hkostream.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/serialize/serialize/hkrelocationinfo.h"

namespace
{
    // An object still to copy
    struct Pending
    {
        const void* object; // 0x00
        const hkClass* klass; // 0x04
    };

    // An object copied into the buffer
    struct Copy
    {
        Copy(int f, const void* o, const hkClass* c) : copyOffset(f), original(o), klass(c) {} // 0x00F7AD90 (:20)

        int copyOffset; // 0x00
        const void* original; // 0x04
        const hkClass* klass; // 0x08
    };
}

// 0x00F7AF80 (:43): breadth first from dataIn; a pointer already copied before (previousCopies) points at that
// copy, other targets are copied once each. Virtual objects are copied as their exact class (from the vtable
// registry, which must be given for them). Null when nothing was written.
void* hkDeepCopier::deepCopy(const void* dataIn, const hkClass& klassIn, hkPointerMap<void const*,void*,hkContainerHeapAllocator>* previousCopies, hkVtableClassRegistry* vtableClassRegistry, hkTypeInfoRegistry* loadedObjectRegistry, hkFlags<enum hkObjectCopier::ObjectCopierFlagBits,unsigned int> flags)
{
    hkArray<char,hkContainerHeapAllocator> buffer;
    hkOstream os(buffer);
    hkRelocationInfo reloc;

    hkArray<Pending,hkContainerHeapAllocator> todo;
    hkPointerMap<const void*,int,hkContainerHeapAllocator> todoSet;
    Pending& first = todo.expandOne();
    first.object = dataIn;
    first.klass = &klassIn;
    todoSet.insert(dataIn, 1);

    hkPointerMap<const void*,int,hkContainerHeapAllocator> copiedOffsets; // original -> offset in the buffer
    hkArray<Copy,hkContainerHeapAllocator> copies;
    hkObjectCopier copier(hkStructureLayout(hkStructureLayout::HostLayoutRules), hkStructureLayout(hkStructureLayout::HostLayoutRules), flags);

    for (int i = 0; i < todo.getSize(); i++)
    {
        const void* object = todo[i].object;
        const hkClass* klass = todo[i].klass;
        copiedOffsets.insert(object, buffer.getSize());
        const int globalsBefore = reloc.m_global.getSize();
        if (klass->hasVtable())
        {
            klass = vtableClassRegistry->getClassFromVirtualInstance(object);
        }
        copies.pushBack(Copy(buffer.getSize(), object, klass));
        copier.copyObject(object, *klass, os.getStreamWriter(), *klass, reloc);

        for (int g = globalsBefore; g < reloc.m_global.getSize(); g++)
        {
            hkRelocationInfo::Global& global = reloc.m_global[g];
            void* target = global.m_toAddress;
            const hkClass* targetClass = global.m_toClass;
            if (target == nullptr || targetClass == nullptr)
            {
                continue;
            }
            void* previous = previousCopies != nullptr ? previousCopies->getWithDefault(target, nullptr) : nullptr;
            if (previous != nullptr)
            {
                global.m_toAddress = previous;
            }
            else if (todoSet.getWithDefault(target, 0) == 0)
            {
                Pending& p = todo.expandOne();
                p.object = target;
                p.klass = targetClass;
                todoSet.insert(target, 1);
            }
        }
    }

    const int dataSize = buffer.getSize();
    if (dataSize == 0)
    {
        return nullptr;
    }
    const int tailSize = loadedObjectRegistry != nullptr ? copies.getSize() * 8 + 4 : 4;
    char* result = static_cast<char*>(hkMemoryRouter::easyAlloc(hkMemoryRouter::getInstance().heap(), dataSize + tailSize));
    char* tail = result + buffer.getSize();
    hkString::memCpy(result, buffer.begin(), buffer.getSize());
    for (int i = 0; i < reloc.m_local.getSize(); i++)
    {
        const hkRelocationInfo::Local& l = reloc.m_local[i];
        *reinterpret_cast<char**>(result + l.m_fromOffset) = result + l.m_toOffset;
    }
    for (int i = 0; i < reloc.m_global.getSize(); i++)
    {
        const hkRelocationInfo::Global& g = reloc.m_global[i];
        void* target = g.m_toAddress;
        const int offset = copiedOffsets.getWithDefault(target, -1);
        if (offset != -1)
        {
            target = result + offset;
        }
        *reinterpret_cast<void**>(result + g.m_fromOffset) = target;
    }
    if (previousCopies != nullptr)
    {
        for (int i = 0; i < copies.getSize(); i++)
        {
            previousCopies->insert(copies[i].original, result + copies[i].copyOffset);
        }
    }
    if (loadedObjectRegistry == nullptr)
    {
        *reinterpret_cast<int*>(tail) = 0;
    }
    else
    {
        void** pairs = reinterpret_cast<void**>(tail);
        for (int i = 0; i < copies.getSize(); i++)
        {
            void* object = result + copies[i].copyOffset;
            loadedObjectRegistry->finishLoadedObject(object, copies[i].klass->getName());
            pairs[2 * i] = const_cast<hkTypeInfo*>(loadedObjectRegistry->getTypeInfo(copies[i].klass->getName()));
            pairs[2 * i + 1] = result + copies[i].copyOffset;
        }
        for (int i = 0; i < copies.getSize(); i++)
        {
            if (const hkVariant* attr = copies[i].klass->getAttribute("hk.PostFinish"))
            {
                static_cast<const hkPostFinishAttribute*>(attr->m_object)->m_postFinishFunction(result + copies[i].copyOffset);
            }
        }
        *reinterpret_cast<int*>(tail + copies.getSize() * 8) = copies.getSize();
    }
    return result;
}

// 0x00F7AF00 (:200): the objects listed at the end of the block are cleaned up, then the block is freed. The count
// is read at the end of the allocation's size as the allocator reports it.
hkResult hkDeepCopier::freeDeepCopy(void* data)
{
    char* block = static_cast<char*>(data);
    const int size = static_cast<int>(hkMemoryRouter::getEasyAllocSize(hkMemoryRouter::getInstance().heap(), data));
    const int count = *reinterpret_cast<int*>(block + size - 4);
    if (count != 0)
    {
        char* pairs = block + size - count * 8 - 4;
        for (int i = 0; i < count; i++)
        {
            const hkTypeInfo* typeInfo = *reinterpret_cast<const hkTypeInfo* const*>(pairs + i * 8);
            if (typeInfo != nullptr)
            {
                typeInfo->cleanupLoadedObject(*reinterpret_cast<void* const*>(pairs + i * 8 + 4));
            }
        }
    }
    hkMemoryRouter::easyFree(hkMemoryRouter::getInstance().heap(), data);
    return HK_SUCCESS;
}
