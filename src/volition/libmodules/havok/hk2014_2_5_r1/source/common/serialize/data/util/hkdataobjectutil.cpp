// Original: volition/libmodules/havok/hk2014_2_5_r1/source/common/serialize/data/util/hkdataobjectutil.cpp
//
// Havok 2014.2.5 (stub_impl.md S4, serializer SC and SD): the Copier, which turns a data world's objects into native
// objects (hkDataObjectToNative), and the mapping of hkClassMember types to data world types. From the decompile and
// asm.
//
// The Copier walks the objects reachable from the top object depth first: every pointer to an object not copied yet
// becomes a patch waiting for it, and the first owning pointer queues the object. A copied object fills the patches
// waiting for it and counts its owning references (its reference count once finished, when it is an
// hkReferencedObject). With objects on the heap a cycle of owning pointers fails the copy.
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/serialize/data/util/hkdataobjectutil.h"

#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/reflection/attributes/hkattributes.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/container/pointermap/hkmap.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/reflection/hkclass.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/reflection/registry/hktypeinforegistry.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/serialize/data/hkdataobject.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/serialize/data/util/hkdataobjecttonative.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/serialize/data/util/hkdataworldcloner.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/serialize/packfile/hkpackfiledata.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/serialize/resource/hkobjectresource.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/serialize/util/hkbuiltintyperegistry.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/serialize/util/hkserializemultimap.h"

#include <intrin.h>

extern const hkClass hkReferencedObjectClass;

// size 0x5C
struct Copier
{
    // size 0x70
    // The packfile data that tracks the copies' memory until the copy succeeded (then it lets go of it).
    class TrackedData : public hkPackfileData
    {
    public:
        // 0x00F68100 (:62)
        void stopTrackingAllocations()
        {
            m_destructorsEnabled = false;
            m_memory.setSizeUnchecked(0);
            m_chunks.setSizeUnchecked(0);
        }
    };

    // size 0x10
    struct LiveObjectInfo
    {
        LiveObjectInfo() : m_refCount(0), m_size(-1) // 0x00F664B0 (:73)
        {
            m_variant.m_object = nullptr;
            m_variant.m_class = nullptr;
        }

        hkVariant m_variant; // 0x00
        int m_refCount; // 0x08: the owning pointers to it
        int m_size; // 0x0C: its allocation, -1 when it is not a chunk of its own
    };

    // size 0x8
    // A pointer (or variant) waiting for an object.
    struct PointerPatch
    {
        PointerPatch() {} // 0x00F67970

        void* m_addr; // 0x00
        hkBool m_isVariant; // 0x04
        hkBool m_isOwning; // 0x05
    };

    typedef hkPointerMap<void*, int, hkContainerHeapAllocator> ReferenceCountFromObjectPointer;
    typedef hkMap<hkDataObject_Handle, int, hkMapOperations<hkDataObject_Handle>, hkContainerHeapAllocator> IndexMapType;
    typedef hkMap<hkDataObject_Handle, LiveObjectInfo, hkMapOperations<hkDataObject_Handle>, hkContainerHeapAllocator> LiveObjectInfoFromHandle;

    // 0x00F68CE0 (:104)
    Copier(const hkClassNameRegistry* creg, hkBool32 allocatedOnHeap, hkBool32 destroyOnCopy)
        : m_classReg(creg), m_trackedData(nullptr), m_allocatedOnHeap(allocatedOnHeap), m_destroyOnCopy(destroyOnCopy)
    {
    }

    // 0x00F68D60 (:108)
    ~Copier()
    {
        m_trackedData = nullptr;
        m_classReg = nullptr;
    }

    // 0x00F664D0 (:115)
    hkBool32 isReferencedObject(const hkClass& klass)
    {
        return klass.hasVtable() && hkReferencedObjectClass.isSuperClass(klass);
    }

    void markAsDone(const hkDataObject_Handle& handle, hkVariant& v, int objectSize);
    hkResult resolvePointer(const PointerPatch& patch, const hkDataObject_Handle& handle);
    void finishAndTrackObjectsInPackfile(hkPackfileData* trackedData);
    void finishAndRefcountLiveObjects(const hkTypeInfoRegistry* typeReg);
    void postFinishObjects();
    hkResult processPointersAndAllocations(const hkDataObjectToNative::CopyInfoOut& infoOut);
    hkVariant abortDeepCopy();
    hkVariant deepCopyToNative(const hkDataObject& topObj);
    hkObjectResource* toObject(const hkDataObject& obj, const hkTypeInfoRegistry* typeReg);
    hkPackfileData* toPackfile(const hkDataObject& topObj);

    hkArray<hkDataObject_Handle, hkContainerHeapAllocator> m_todos; // 0x00: objects to copy (a null handle closes one)
    hkSerializeMultiMap<hkDataObject_Handle, PointerPatch, IndexMapType> m_pointerPatchesFromHandle; // 0x0C
    LiveObjectInfoFromHandle m_liveInfoFromHandle; // 0x28: the objects copied
    hkArray<hkDataObject_Handle, hkContainerHeapAllocator> m_copyStack; // 0x34: the objects being copied
    hkArray<hkVariant, hkContainerHeapAllocator> m_postFinishObjects; // 0x40
    hkRefPtr<const hkClassNameRegistry> m_classReg; // 0x4C
    hkRefPtr<hkPackfileData> m_trackedData; // 0x50
    hkBool32 m_allocatedOnHeap; // 0x54
    hkBool32 m_destroyOnCopy; // 0x58
};

// 0x00F68110 (:121): the waiting patches filled; the owning ones are the reference count of a referenced object
void Copier::markAsDone(const hkDataObject_Handle& handle, hkVariant& v, int objectSize)
{
    int numOwning = 0;
    for (int i = m_pointerPatchesFromHandle.getFirstIndex(handle); i != -1; i = m_pointerPatchesFromHandle.getNextIndex(i))
    {
        const PointerPatch& patch = m_pointerPatchesFromHandle.getValue(i);
        void** addr = static_cast<void**>(patch.m_addr);
        addr[0] = v.m_object;
        if (patch.m_isVariant)
        {
            addr[1] = const_cast<hkClass*>(v.m_class);
        }
        numOwning += patch.m_isOwning ? 1 : 0;
    }
    m_pointerPatchesFromHandle.removeKey(handle);
    LiveObjectInfo info;
    info.m_variant = v;
    info.m_refCount = isReferencedObject(*v.m_class) ? numOwning : 0;
    info.m_size = objectSize;
    m_liveInfoFromHandle.insert(handle, info);
}

// 0x00F68F30 (:153)
hkResult Copier::resolvePointer(const PointerPatch& patch, const hkDataObject_Handle& handle)
{
    if (m_allocatedOnHeap && patch.m_isOwning)
    {
        // an owning pointer back to an object being copied: a cycle of heap objects
        for (int i = 0; i < m_copyStack.getSize(); i++)
        {
            if (m_copyStack[i] == handle)
            {
                return HK_FAILURE;
            }
        }
    }
    LiveObjectInfoFromHandle::Iterator it = m_liveInfoFromHandle.findKey(handle);
    if (m_liveInfoFromHandle.isValid(it))
    {
        LiveObjectInfo info = m_liveInfoFromHandle.getValue(it);
        void** addr = static_cast<void**>(patch.m_addr);
        addr[0] = info.m_variant.m_object;
        if (patch.m_isVariant)
        {
            addr[1] = const_cast<hkClass*>(info.m_variant.m_class);
        }
        info.m_refCount += patch.m_isOwning ? 1 : 0;
        m_liveInfoFromHandle.setValue(it, info);
        return HK_SUCCESS;
    }
    // queued with its first owning pointer
    if (patch.m_isOwning)
    {
        bool owned = false;
        for (int i = m_pointerPatchesFromHandle.getFirstIndex(handle); i != -1; i = m_pointerPatchesFromHandle.getNextIndex(i))
        {
            if (m_pointerPatchesFromHandle.getValue(i).m_isOwning)
            {
                owned = true;
                break;
            }
        }
        if (!owned)
        {
            m_todos.pushBack(handle);
        }
    }
    m_pointerPatchesFromHandle.insert(handle, patch);
    return HK_SUCCESS;
}

// 0x00F68910 (:215)
void Copier::finishAndTrackObjectsInPackfile(hkPackfileData* trackedData)
{
    for (LiveObjectInfoFromHandle::Iterator it = m_liveInfoFromHandle.getIterator(); m_liveInfoFromHandle.isValid(it);
         it = m_liveInfoFromHandle.getNext(it))
    {
        const hkVariant v = m_liveInfoFromHandle.getValue(it).m_variant;
        trackedData->trackObject(v.m_object, v.m_class->getName());
        if (v.m_class->getAttribute("hk.PostFinish") != nullptr)
        {
            trackedData->m_postFinishObjects.pushBack(v);
        }
    }
}

// 0x00F68A10 (:233): the reference counts and memory sizes written, then the finishing constructors run
void Copier::finishAndRefcountLiveObjects(const hkTypeInfoRegistry* typeReg)
{
    for (LiveObjectInfoFromHandle::Iterator it = m_liveInfoFromHandle.getIterator(); m_liveInfoFromHandle.isValid(it);
         it = m_liveInfoFromHandle.getNext(it))
    {
        const LiveObjectInfo info = m_liveInfoFromHandle.getValue(it);
        const hkVariant& v = info.m_variant;
        if (v.m_class->getAttribute("hk.PostFinish") != nullptr)
        {
            m_postFinishObjects.pushBack(v);
        }
        if ((short)info.m_refCount > 0)
        {
            const unsigned int memSize = info.m_size < 0 ? (unsigned int)v.m_class->getObjectSize() : (unsigned int)info.m_size;
            // hkReferencedObject::m_memSizeAndRefCount (+0x04), written atomically
            volatile long* word = (volatile long*)(static_cast<char*>(v.m_object) + 4);
            const long value = (long)((memSize << 16) | (unsigned short)info.m_refCount);
            long old;
            do
            {
                old = *word;
            } while (_InterlockedCompareExchange(word, value, old) != old);
        }
        typeReg->finishLoadedObject(v.m_object, v.m_class->getName());
        v.m_class->hasVtable(); // an assert in Havok (a virtual class needs a type info), compiled to a dropped call
    }
}

// 0x00F679C0 (:284): the post-finish functions of the classes that declare one (hk.PostFinish)
void Copier::postFinishObjects()
{
    for (hkVariant* v = m_postFinishObjects.begin(); v < m_postFinishObjects.begin() + m_postFinishObjects.getSize(); v++)
    {
        const hkVariant* attr = v->m_class->getAttribute("hk.PostFinish");
        static_cast<const hkPostFinishAttribute*>(attr->m_object)->m_postFinishFunction(v->m_object);
    }
}

// 0x00F690C0 (:300): the allocations go to the tracked data, the pointers out are resolved
hkResult Copier::processPointersAndAllocations(const hkDataObjectToNative::CopyInfoOut& infoOut)
{
    for (int i = 0; i < infoOut.allocs.getSize(); i++)
    {
        const hkDataObjectToNative::Alloc& a = infoOut.allocs[i];
        if (a.m_size == -1)
        {
            m_trackedData->addAllocation(a.m_addr);
        }
        else
        {
            m_trackedData->addChunk(a.m_addr, a.m_size, HK_MEMORY_CLASS_SERIALIZE);
        }
    }
    for (int i = 0; i < infoOut.pointersOut.getSize(); i++)
    {
        const hkDataObjectToNative::PointerInfo& pi = infoOut.pointersOut[i];
        PointerPatch patch;
        patch.m_addr = pi.m_addr;
        patch.m_isVariant = pi.m_isVariant;
        patch.m_isOwning = pi.m_isOwning;
        if (resolvePointer(patch, pi.m_handle) == HK_FAILURE)
        {
            return HK_FAILURE;
        }
    }
    return HK_SUCCESS;
}

// 0x00F685B0 (:329)
hkVariant Copier::abortDeepCopy()
{
    m_pointerPatchesFromHandle.clear();
    m_todos.setSizeUnchecked(0);
    m_liveInfoFromHandle.clear();
    hkVariant v;
    v.m_object = nullptr;
    v.m_class = nullptr;
    return v;
}

// 0x00F69200 (:338). Original quirk kept: fillNativeMembers' result is not checked.
hkVariant Copier::deepCopyToNative(const hkDataObject& topObj)
{
    hkDataWorld* world = const_cast<hkDataWorld*>(topObj.getClass().getWorld());
    hkDataObjectToNative toNative(m_classReg, m_allocatedOnHeap);
    hkDataObjectToNative::CopyInfoOut infoOut;
    hkVariant topVariant;
    topVariant.m_object = nullptr;
    topVariant.m_class = nullptr;
    {
        PointerPatch patch;
        patch.m_addr = &topVariant;
        patch.m_isVariant = true;
        patch.m_isOwning = true;
        resolvePointer(patch, topObj.getHandle());
    }
    while (m_todos.getSize())
    {
        const hkDataObject_Handle handle = m_todos[m_todos.getSize() - 1];
        if (handle.p0 == nullptr)
        {
            // the end of an object's copy
            m_todos.setSizeUnchecked(m_todos.getSize() - 1);
            m_copyStack.setSizeUnchecked(m_copyStack.getSize() - 1);
            continue;
        }
        m_copyStack.pushBack(handle);
        m_todos[m_todos.getSize() - 1] = hkDataObject_Handle();
        hkDataObject obj = world->findObject(handle);
        hkVariant v = toNative.allocateObject(obj, infoOut);
        if (v.m_object == nullptr)
        {
            return abortDeepCopy();
        }
        const hkDataObjectToNative::Alloc& last = infoOut.allocs[infoOut.allocs.getSize() - 1];
        const int objectSize = last.m_addr == v.m_object ? last.m_size : -1;
        markAsDone(handle, v, objectSize);
        toNative.fillNativeMembers(v.m_object, obj, infoOut);
        if (processPointersAndAllocations(infoOut) != HK_SUCCESS)
        {
            return abortDeepCopy();
        }
        infoOut.allocs.setSizeUnchecked(0);
        infoOut.pointersOut.setSizeUnchecked(0);
        if (m_destroyOnCopy)
        {
            obj.destroy();
        }
    }
    return topVariant;
}

// 0x00F694E0 (:418)
hkObjectResource* Copier::toObject(const hkDataObject& obj, const hkTypeInfoRegistry* typeReg)
{
    m_trackedData.setAndDontIncrementRefCount(new hkPackfileData(nullptr));
    hkVariant v = deepCopyToNative(obj);
    if (v.m_object == nullptr)
    {
        m_trackedData->disableDestructors();
        m_trackedData = nullptr;
        return nullptr;
    }
    finishAndRefcountLiveObjects(typeReg);
    postFinishObjects();
    hkObjectResource* resource = new hkObjectResource(v);
    resource->setClassNameRegistry(m_classReg);
    resource->setTypeInfoRegistry(typeReg);
    static_cast<TrackedData*>(m_trackedData.val())->stopTrackingAllocations();
    m_trackedData = nullptr;
    return resource;
}

// 0x00F695E0 (:441)
hkPackfileData* Copier::toPackfile(const hkDataObject& topObj)
{
    if (topObj.isNull())
    {
        return nullptr;
    }
    m_trackedData.setAndDontIncrementRefCount(new hkPackfileData(m_classReg));
    hkVariant v = deepCopyToNative(topObj);
    if (v.m_object != nullptr && v.m_class != nullptr)
    {
        finishAndTrackObjectsInPackfile(m_trackedData);
        m_trackedData->setContentsWithName(v.m_object, v.m_class->getName());
        hkPackfileData* data = m_trackedData;
        data->addReference();
        m_trackedData = nullptr;
        return data;
    }
    m_trackedData = nullptr;
    return nullptr;
}

// 0x00F66970 (:469)
hkObjectResource* hkDataObjectUtil::toObject(const hkDataObject& src, hkBool32 destroyOnCopy)
{
    hkBuiltinTypeRegistry& reg = hkBuiltinTypeRegistry::getInstance();
    const hkTypeInfoRegistry* typeReg = reg.getTypeInfoRegistry();
    const hkClassNameRegistry* classReg = reg.getClassNameRegistry();
    return toObjectWithRegistry(src, classReg, typeReg, destroyOnCopy);
}

// 0x00F66820 (:474)
hkObjectResource* hkDataObjectUtil::toObjectWithRegistry(const hkDataObject& src, const hkClassNameRegistry* classReg, const hkTypeInfoRegistry* typeReg, hkBool32 destroyOnCopy)
{
    Copier copier(classReg, true, destroyOnCopy);
    return copier.toObject(src, typeReg);
}

// 0x00F669A0 (:480)
hkResource* hkDataObjectUtil::toResource(const hkDataObject& src, hkBool32 destroyOnCopy)
{
    return toResourceWithRegistry(src, hkBuiltinTypeRegistry::getInstance().getClassNameRegistry(), destroyOnCopy);
}

// 0x00F66860 (:485)
hkResource* hkDataObjectUtil::toResourceWithRegistry(const hkDataObject& src, const hkClassNameRegistry* classReg, hkBool32 destroyOnCopy)
{
    Copier copier(classReg, false, destroyOnCopy);
    return copier.toPackfile(src);
}

namespace
{
    // size 0x4
    // How a hkClassMember::Type maps to a type of the data world.
    struct TypeLookup
    {
        enum Kind
        {
            KIND_INVALID = 0,
            KIND_BASIC = 1, // m_subType built in
            KIND_ARRAY = 2, // an array of the member's subtype
            KIND_ENUM = 3, // stored as the member's subtype
            KIND_TUPLE = 4, // m_tupleSize of the built-in m_subType
            KIND_POINTER = 5, // a pointer to the member's class
            KIND_STRUCT = 6, // the member's class
            KIND_VARIANT = 7, // a pointer to the homogeneous class
        };
        unsigned char m_type; // 0x00 (Kind)
        unsigned char m_subType; // 0x01 (hkTypeManager::SubType)
        unsigned char m_tupleSize; // 0x02
    };

    // 0x01373C40: one entry per hkClassMember::Type, TYPE_VOID .. TYPE_MAX (36)
    const TypeLookup s_lut[] = {
        {1, 1, 0},  // VOID: void
        {1, 2, 0},  // BOOL: byte
        {1, 2, 0},  // CHAR: byte
        {1, 4, 0},  // INT8: int
        {1, 2, 0},  // UINT8: byte
        {1, 4, 0},  // INT16
        {1, 4, 0},  // UINT16
        {1, 4, 0},  // INT32
        {1, 4, 0},  // UINT32
        {1, 4, 0},  // INT64
        {1, 4, 0},  // UINT64
        {1, 3, 0},  // REAL: real
        {4, 3, 4},  // VECTOR4: {4} real
        {4, 3, 4},  // QUATERNION
        {4, 3, 12}, // MATRIX3
        {4, 3, 12}, // ROTATION
        {4, 3, 12}, // QSTRANSFORM
        {4, 3, 16}, // MATRIX4
        {4, 3, 16}, // TRANSFORM
        {1, 1, 0},  // ZERO: void
        {5, 0, 0},  // POINTER
        {1, 1, 0},  // FUNCTIONPOINTER: void
        {2, 0, 0},  // ARRAY
        {2, 0, 0},  // INPLACEARRAY
        {3, 0, 0},  // ENUM
        {6, 0, 0},  // STRUCT
        {2, 0, 0},  // SIMPLEARRAY
        {2, 0, 0},  // HOMOGENEOUSARRAY
        {7, 0, 0},  // VARIANT
        {1, 5, 0},  // CSTRING: cstring
        {1, 4, 0},  // ULONG: int
        {3, 1, 0},  // FLAGS
        {1, 3, 0},  // HALF: real
        {1, 5, 0},  // STRINGPTR: cstring
        {2, 0, 0},  // RELARRAY
        {1, 1, 0},  // MAX: void
    };
}

// 0x00F66510 (:562)
// A pointer member without a class name points at the homogeneous class.
hkTypeManager::Type* hkDataObjectUtil::getBasicType(hkTypeManager& typeManager, hkClassMember::Type type, const char* typeName)
{
    const TypeLookup& lut = s_lut[type];
    switch (lut.m_type)
    {
        case TypeLookup::KIND_BASIC:
            return typeManager.getSubType((hkTypeManager::SubType)lut.m_subType);
        case TypeLookup::KIND_TUPLE:
            return typeManager.makeTuple(typeManager.getSubType((hkTypeManager::SubType)lut.m_subType), lut.m_tupleSize);
        case TypeLookup::KIND_POINTER:
            if (typeName)
            {
                return typeManager.makePointer(typeManager.addClass(typeName));
            }
            return typeManager.makePointer(typeManager.getHomogenousClass());
        case TypeLookup::KIND_STRUCT:
            return typeManager.addClass(typeName);
        case TypeLookup::KIND_VARIANT:
            return typeManager.makePointer(typeManager.getHomogenousClass());
        default:
            return nullptr;
    }
}

// 0x00F665C0 (:609)
// count is the c-style array size: the element type becomes a tuple of it (for an array, the elements do).
hkTypeManager::Type* hkDataObjectUtil::getTypeFromMemberTypeClassName(hkTypeManager& typeManager, hkClassMember::Type mtype, hkClassMember::Type stype, const char* typeName, int count)
{
    const TypeLookup& lut = s_lut[mtype];
    hkTypeManager::Type* type = nullptr;
    switch (lut.m_type)
    {
        case TypeLookup::KIND_BASIC:
            type = typeManager.getSubType((hkTypeManager::SubType)lut.m_subType);
            break;
        case TypeLookup::KIND_ARRAY:
            type = stype == hkClassMember::TYPE_STRUCT ? typeManager.addClass(typeName) : getBasicType(typeManager, stype, typeName);
            if (count)
            {
                type = typeManager.makeTuple(type, count);
            }
            return typeManager.makeArray(type);
        case TypeLookup::KIND_ENUM:
            type = getBasicType(typeManager, stype, nullptr);
            break;
        case TypeLookup::KIND_TUPLE:
            type = typeManager.makeTuple(typeManager.getSubType((hkTypeManager::SubType)lut.m_subType), lut.m_tupleSize);
            break;
        case TypeLookup::KIND_POINTER:
            type = typeManager.makePointer(getBasicType(typeManager, stype, typeName));
            break;
        case TypeLookup::KIND_STRUCT:
        case TypeLookup::KIND_VARIANT:
            type = getBasicType(typeManager, mtype, typeName);
            break;
        default:
            break;
    }
    if (count)
    {
        type = typeManager.makeTuple(type, count);
    }
    return type;
}

// 0x00F666C0 (:681)
hkTypeManager::Type* hkDataObjectUtil::getTypeFromMemberType(hkTypeManager& typeManager, hkClassMember::Type mtype, hkClassMember::Type stype, const hkClass* klass, int count)
{
    return getTypeFromMemberTypeClassName(typeManager, mtype, stype, klass ? klass->getName() : nullptr, count);
}

// 0x00F66710 (:698)
// The storage the object's hkRelArray members need after the object (16-byte aligned per member).
int hkDataObjectUtil::getExtraStorageSize(const hkClass* klass, const hkDataObject& obj)
{
    int size = 0;
    for (int i = 0; i < klass->getNumMembers(); i++)
    {
        const hkClassMember& mem = klass->getMember(i);
        if (mem.getType() == hkClassMember::TYPE_RELARRAY)
        {
            hkDataArray arr = obj[mem.getName()].asArray();
            size += (mem.getArrayMemberSize() * arr.getSize() + 15) & ~15;
        }
    }
    return size;
}

// 0x00F667E0 (:693)
hkResult hkDataObjectUtil::deepCopyWorld(hkDataWorld& dst, const hkDataWorld& src)
{
    hkDataWorldCloner cloner(dst, src);
    return cloner.clone();
}
