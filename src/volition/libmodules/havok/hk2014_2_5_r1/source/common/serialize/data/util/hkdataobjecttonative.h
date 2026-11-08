// Original: volition/libmodules/havok/hk2014_2_5_r1/source/common/serialize/data/util/hkdataobjecttonative.h
// Implementation: volition/libmodules/havok/hk2014_2_5_r1/source/common/serialize/data/util/hkdataobjecttonative.cpp
//
// Havok 2014.2.5 (stub_impl.md S4, serializer SD): copies data objects into native objects (layouts from rfg.pdb,
// inline code of 2014.1 hkDataObjectToNative.h).
#pragma once

#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/memory/memoryclasses/hkmemoryclasses.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/container/array/hkarray.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/container/hkcontainerallocators.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/reflection/hkclassmember.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/types/hkbasetypes.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/serialize/data/hkdataobject.h"

class hkClass;
class hkClassNameRegistry;
struct hkVariant;

// size 0x8
class hkDataObjectToNative
{
public:
    HK_DECLARE_NONVIRTUAL_CLASS_ALLOCATOR(hkDataObjectToNative);

    // 0x00F66460 (.h:24)
    hkDataObjectToNative(const hkClassNameRegistry* classReg, hkBool32 allocatedOnHeap)
        : m_classReg(classReg), m_allocatedOnHeap(allocatedOnHeap)
    {
    }

    // size 0x10
    // A pointer (or variant) out of a copied object, to the object with m_handle. The constructor (0x00F77CF0) is
    // compiler-generated.
    struct PointerInfo
    {
        HK_DECLARE_NONVIRTUAL_CLASS_ALLOCATOR(PointerInfo);

        hkDataObject_Handle m_handle; // 0x00
        void* m_addr; // 0x08
        hkBool m_isVariant; // 0x0C
        hkBool m_isOwning; // 0x0D
    };

    // size 0x8
    // An allocation made for a copy (m_size -1: a string, freed as a whole).
    struct Alloc
    {
        HK_DECLARE_NONVIRTUAL_CLASS_ALLOCATOR(Alloc);
        // 0x00F75540 (.h:38)
        void set(void* addr, int size)
        {
            m_addr = addr;
            m_size = size;
        }

        void* m_addr; // 0x00
        int m_size; // 0x04
    };

    // size 0xC
    // The layout of an hkArray (or the first two words of an hkSimpleArray).
    struct DummyArray
    {
        HK_DECLARE_NONVIRTUAL_CLASS_ALLOCATOR(DummyArray);

        void* data; // 0x00
        int size; // 0x04
        int capacity; // 0x08
    };

    // size 0x4
    // The layout of an hkRelArray.
    struct DummyRelArray
    {
        HK_DECLARE_NONVIRTUAL_CLASS_ALLOCATOR(DummyRelArray);

        unsigned short size; // 0x00
        unsigned short offset; // 0x02
    };

    // size 0x18
    // What copying objects produced: the pointers to resolve and the allocations made. The destructor (0x00F68BC0,
    // emitted in hkDataObjectUtil.obj) is compiler-generated.
    struct CopyInfoOut
    {
        HK_DECLARE_NONVIRTUAL_CLASS_ALLOCATOR(CopyInfoOut);

        // 0x00F68840 (.h:59)
        CopyInfoOut()
        {
            pointersOut.reserve(32);
        }

        // 0x00F78600 (.h:64): nothing for a null object
        void addPointer(const hkDataObject& obj, void* addr, hkBool32 isOwning)
        {
            hkDataObject_Handle handle = obj.getHandle();
            if (handle.p0 || handle.p1)
            {
                PointerInfo& info = pointersOut.expandOne();
                info.m_handle = handle;
                info.m_addr = addr;
                info.m_isOwning = isOwning != 0;
                info.m_isVariant = false;
            }
        }

        // 0x00F78690 (.h:76)
        void addVariant(const hkDataObject& obj, hkVariant* addr, hkBool32 isOwning)
        {
            hkDataObject_Handle handle = obj.getHandle();
            if (handle.p0 || handle.p1)
            {
                PointerInfo& info = pointersOut.expandOne();
                info.m_handle = handle;
                info.m_addr = addr;
                info.m_isOwning = isOwning != 0;
                info.m_isVariant = true;
            }
        }

        // 0x00F78590 (.h:88)
        void addAlloc(void* p, int size)
        {
            allocs.expandOne().set(p, size);
        }

        hkArray<PointerInfo,hkContainerTempAllocator> pointersOut; // 0x00
        hkArray<Alloc,hkContainerTempAllocator> allocs; // 0x0C
    };

    hkVariant allocateObject(const hkDataObject& obj, CopyInfoOut& infoOut); // 0x00F756D0
    hkResult fillNativeMembers(void* dstObject, const hkDataObject& srcObj, CopyInfoOut& copyInfoOut); // 0x00F77100

protected:
    const hkClass* findClassOf(const hkDataObject& obj); // 0x00F75550
    // <hkDataArray_Value> 0x00F78720, <hkDataObject_Value> 0x00F78C30 (:689)
    template<typename Value>
    hkResult fillNativeSimpleMember(void* address, hkClassMember::Type mtype, const Value& value, const hkClass* structClass, hkBool32 owningReference, CopyInfoOut& copyInfoOut);
    hkResult copyIntoNativeArray(void* address, const hkClassMember& member, const hkDataArray& srcArray, CopyInfoOut& copyInfoOut); // 0x00F757E0
    hkResult copyIntoRelArray(void* address, const hkClassMember& member, const hkDataArray& srcArray, CopyInfoOut& copyInfoOut, void*& relArrayAddress, void* objectStart); // 0x00F76730
    hkResult fillNativeEnumMember(void* address, const hkClassMember& member, const hkDataObject_Value& value, const hkClass& klass); // 0x00F75670
    // A data object that may be assigned to the member: the class to copy it as
    const hkClass* getMemberClassAndCheck(const hkClassMember& member, const hkDataObject& obj); // 0x00F755A0

    const hkClassNameRegistry* m_classReg; // 0x00
    hkBool32 m_allocatedOnHeap; // 0x04
};
