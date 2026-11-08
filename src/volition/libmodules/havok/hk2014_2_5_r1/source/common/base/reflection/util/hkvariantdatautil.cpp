// Original: volition/libmodules/havok/hk2014_2_5_r1/source/common/base/reflection/util/hkvariantdatautil.cpp
//
// Havok 2014.2.5 (stub_impl.md S4, serializer SB): reads, writes, constructs and converts reflected values by their
// member type, from rfg.exe. A real is stored to a half by truncation (hkHalf::operator=(const float&)).

#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/reflection/util/hkvariantdatautil.h"

#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/container/string/hkstring.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/container/string/hkstringptr.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/object/hkreferencedobject.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/reflection/hkclass.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/reflection/hktypeinfo.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/reflection/registry/hkclassnameregistry.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/reflection/registry/hktypeinforegistry.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/reflection/registry/hkvtableclassregistry.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/system/error/hkerror.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/serialize/resource/hkobjectresource.h"

#include <intrin.h>
#include <new>

// The arrays handled here are hkArrays of any element type (TYPE_ARRAY members); their storage seen as bytes.
typedef hkArray<char> DummyArray;

// 0x00D184E0 (:23)
int hkVariantDataUtil::calcElementSize(hkClassMember::Type type, const hkClass* cls)
{
    if (type == hkClassMember::TYPE_STRUCT)
    {
        return cls->getObjectSize();
    }
    return hkClassMember::getClassMemberTypeProperties(type).m_size;
}

// 0x00D18500 (:28): 0 for a type that is not a real
float hkVariantDataUtil::getReal(hkClassMember::Type type, const void* data)
{
    switch (type)
    {
        case hkClassMember::TYPE_REAL:
            return *static_cast<const float*>(data);
        case hkClassMember::TYPE_HALF:
            return static_cast<const hkHalf*>(data)->getReal();
        default:
            return 0.0f;
    }
}

// 0x00D19830 (:41)
void hkVariantDataUtil::setReal(hkClassMember::Type type, void* data, float value)
{
    switch (type)
    {
        case hkClassMember::TYPE_REAL:
            *static_cast<float*>(data) = value;
            break;
        case hkClassMember::TYPE_HALF:
            *static_cast<hkHalf*>(data) = value;
            break;
        default:
            break;
    }
}

// 0x00D18F80 (:62)
void hkVariantDataUtil::setHalf(hkClassMember::Type type, void* data, hkHalf value)
{
    switch (type)
    {
        case hkClassMember::TYPE_REAL:
            value.store(static_cast<float*>(data));
            break;
        case hkClassMember::TYPE_HALF:
            *static_cast<hkHalf*>(data) = value;
            break;
        default:
            break;
    }
}

// 0x00D18530 (:83): sign or zero extended by the type; 0 for a type that is not an integer
int64_t hkVariantDataUtil::getInt(hkClassMember::Type type, const void* data)
{
    switch (type)
    {
        case hkClassMember::TYPE_BOOL:
        case hkClassMember::TYPE_CHAR:
        case hkClassMember::TYPE_INT8:
            return *static_cast<const hkInt8*>(data);
        case hkClassMember::TYPE_UINT8:
            return *static_cast<const hkUint8*>(data);
        case hkClassMember::TYPE_INT16:
            return *static_cast<const hkInt16*>(data);
        case hkClassMember::TYPE_UINT16:
            return *static_cast<const hkUint16*>(data);
        case hkClassMember::TYPE_INT32:
            return *static_cast<const hkInt32*>(data);
        case hkClassMember::TYPE_UINT32:
            return *static_cast<const hkUint32*>(data);
        case hkClassMember::TYPE_INT64:
        case hkClassMember::TYPE_UINT64:
            return *static_cast<const hkInt64*>(data);
        case hkClassMember::TYPE_ULONG:
            return *static_cast<const hkUlong*>(data);
        default:
            return 0;
    }
}

// 0x00D185E0 (:109): enums and flags by their storage type
int64_t hkVariantDataUtil::getInt(hkClassMember::Type type, hkClassMember::Type subType, const void* data)
{
    if (type == hkClassMember::TYPE_ENUM || type == hkClassMember::TYPE_FLAGS)
    {
        return getInt(subType, data);
    }
    return getInt(type, data);
}

// 0x00D18610 (:122): truncated to the type
void hkVariantDataUtil::setInt(hkClassMember::Type type, void* data, int64_t value)
{
    switch (type)
    {
        case hkClassMember::TYPE_BOOL:
            *static_cast<hkBool*>(data) = value != 0;
            break;
        case hkClassMember::TYPE_CHAR:
        case hkClassMember::TYPE_INT8:
            *static_cast<hkInt8*>(data) = static_cast<hkInt8>(value);
            break;
        case hkClassMember::TYPE_UINT8:
            *static_cast<hkUint8*>(data) = static_cast<hkUint8>(value);
            break;
        case hkClassMember::TYPE_INT16:
            *static_cast<hkInt16*>(data) = static_cast<hkInt16>(value);
            break;
        case hkClassMember::TYPE_UINT16:
            *static_cast<hkUint16*>(data) = static_cast<hkUint16>(value);
            break;
        case hkClassMember::TYPE_INT32:
            *static_cast<hkInt32*>(data) = static_cast<hkInt32>(value);
            break;
        case hkClassMember::TYPE_UINT32:
            *static_cast<hkUint32*>(data) = static_cast<hkUint32>(value);
            break;
        case hkClassMember::TYPE_INT64:
        case hkClassMember::TYPE_UINT64:
            *static_cast<hkInt64*>(data) = value;
            break;
        case hkClassMember::TYPE_ULONG:
            *static_cast<hkUlong*>(data) = static_cast<hkUlong>(value);
            break;
        default:
            break;
    }
}

// 0x00D18700 (:185)
void hkVariantDataUtil::setInt(hkClassMember::Type type, hkClassMember::Type subType, void* data, int64_t value)
{
    if (type == hkClassMember::TYPE_ENUM || type == hkClassMember::TYPE_FLAGS)
    {
        setInt(subType, data, value);
    }
    else
    {
        setInt(type, data, value);
    }
}

// 0x00D19870 (:203)
const char* hkVariantDataUtil::getString(hkClassMember::Type type, const void* data)
{
    switch (type)
    {
        case hkClassMember::TYPE_CSTRING:
            return *static_cast<const char* const*>(data);
        case hkClassMember::TYPE_STRINGPTR:
            return static_cast<const hkStringPtr*>(data)->cString();
        default:
            return nullptr;
    }
}

// 0x00D18750 (:217): a C string is only pointed to, an hkStringPtr copies
void hkVariantDataUtil::setString(hkClassMember::Type type, void* data, const char* value)
{
    switch (type)
    {
        case hkClassMember::TYPE_CSTRING:
            *static_cast<const char**>(data) = value;
            break;
        case hkClassMember::TYPE_STRINGPTR:
            *static_cast<hkStringPtr*>(data) = value;
            break;
        default:
            break;
    }
}

// 0x00D18FB0 (:238): pointers, arrays, variants and string pointers, and the structs with a finish or cleanup function
hkBool hkVariantDataUtil::needsConstruction(const hkTypeInfoRegistry* infoReg, hkClassMember& mem)
{
    switch (mem.getType())
    {
        case hkClassMember::TYPE_POINTER:
        case hkClassMember::TYPE_ARRAY:
        case hkClassMember::TYPE_VARIANT:
        case hkClassMember::TYPE_STRINGPTR:
            return true;
        case hkClassMember::TYPE_STRUCT:
        {
            const hkTypeInfo* info = infoReg->getTypeInfo(mem.getClass()->getName());
            return info != nullptr && (info->hasCleanupFunction() || info->hasFinishFunction());
        }
        default:
            return false;
    }
}

// 0x00D19040 (:262): zeroed; a virtual object gets its memory size and one reference (set atomically)
void hkVariantDataUtil::newInstance(const hkClass& cls, void* data)
{
    hkString::memSet(data, 0, cls.getObjectSize());
    if (cls.hasVtable())
    {
        const unsigned int memSizeAndRefCount = (unsigned int)(cls.getObjectSize() << 16) | 1;
        // hkReferencedObject::m_memSizeAndRefCount
        volatile long* word = reinterpret_cast<volatile long*>(static_cast<char*>(data) + 4);
        long oldValue;
        do
        {
            oldValue = *word;
        } while (_InterlockedCompareExchange(word, (long)memSizeAndRefCount, oldValue) != oldValue);
    }
}

// 0x00D18780 (:279)
void hkVariantDataUtil::finishObject(const hkTypeInfoRegistry* infoReg, const hkClass& cls, void* data)
{
    const hkTypeInfo* info = infoReg->getTypeInfo(cls.getName());
    if (info != nullptr && info->hasFinishFunction())
    {
        info->finishLoadedObject(data, 1);
    }
}

// 0x00D187C0 (:292)
void hkVariantDataUtil::finishObjectWithoutTracker(const hkTypeInfoRegistry* infoReg, const hkClass& cls, void* data)
{
    const hkTypeInfo* info = infoReg->getTypeInfo(cls.getName());
    if (info != nullptr && info->hasFinishFunction())
    {
        info->finishLoadedObjectWithoutTracker(data, 1);
    }
}

// 0x00D18800 (:305): only types with a finish function are cleaned up (original)
void hkVariantDataUtil::deleteInstance(const hkTypeInfoRegistry* infoReg, const hkClass& cls, void* data)
{
    const hkTypeInfo* info = infoReg->getTypeInfo(cls.getName());
    if (info != nullptr && info->hasFinishFunction())
    {
        info->cleanupLoadedObject(data);
    }
}

// 0x00D18840 (:315): each element zeroed and finished; nothing at all for a type without a type info
void hkVariantDataUtil::newArray(const hkTypeInfoRegistry* infoReg, const hkClass& cls, void* array, int size, int stride)
{
    const hkTypeInfo* info = infoReg->getTypeInfo(cls.getName());
    if (info != nullptr)
    {
        char* p = static_cast<char*>(array);
        for (int i = 0; i < size; ++i)
        {
            hkString::memSet(p, 0, cls.getObjectSize());
            if (info->hasFinishFunction())
            {
                info->finishLoadedObjectWithoutTracker(p, 1);
            }
            p += stride;
        }
    }
}

// 0x00D188A0 (:337)
void hkVariantDataUtil::finishObjectArray(const hkTypeInfoRegistry* infoReg, const hkClass& cls, void* array, int size,
                                          int stride)
{
    const hkTypeInfo* info = infoReg->getTypeInfo(cls.getName());
    if (info != nullptr && info->hasFinishFunction())
    {
        char* p = static_cast<char*>(array);
        for (int i = 0; i < size; ++i)
        {
            info->finishLoadedObjectWithoutTracker(p, 1);
            p += stride;
        }
    }
}

// 0x00D190A0 (:356): the elements constructed by type (null pointers and variants, empty string pointers, finished
// structs); the other types are left as they are
void hkVariantDataUtil::newArray(const hkTypeInfoRegistry* infoReg, hkClassMember::Type type, const hkClass* cls,
                                 void* array, int size, int stride)
{
    char* p = static_cast<char*>(array);
    switch (type)
    {
        case hkClassMember::TYPE_POINTER:
            for (int i = 0; i < size; ++i, p += stride)
            {
                *reinterpret_cast<void**>(p) = nullptr;
            }
            break;
        case hkClassMember::TYPE_STRUCT:
            newArray(infoReg, *cls, array, size, stride);
            break;
        case hkClassMember::TYPE_VARIANT:
            for (int i = 0; i < size; ++i, p += stride)
            {
                hkVariant* v = reinterpret_cast<hkVariant*>(p);
                v->m_class = nullptr;
                v->m_object = nullptr;
            }
            break;
        case hkClassMember::TYPE_STRINGPTR:
            for (int i = 0; i < size; ++i, p += stride)
            {
                new (p) hkStringPtr();
            }
            break;
        default:
            break;
    }
}

// 0x00D188F0 (:401): only types with a finish function are cleaned up (original)
void hkVariantDataUtil::deleteArray(const hkTypeInfoRegistry* infoReg, const hkClass& cls, void* array, int size, int stride)
{
    const hkTypeInfo* info = infoReg->getTypeInfo(cls.getName());
    if (info != nullptr && info->hasFinishFunction())
    {
        char* p = static_cast<char*>(array);
        for (int i = 0; i < size; ++i)
        {
            info->cleanupLoadedObject(p);
            p += stride;
        }
    }
}

// 0x00D19DD0 (:418)
void hkVariantDataUtil::deleteArray(const hkTypeInfoRegistry* infoReg, hkClassMember::Type type, const hkClass* cls,
                                    void* array, int size, int stride)
{
    if (type == hkClassMember::TYPE_STRUCT)
    {
        deleteArray(infoReg, *cls, array, size, stride);
    }
    else if (type == hkClassMember::TYPE_STRINGPTR)
    {
        char* p = static_cast<char*>(array);
        for (int i = 0; i < size; ++i, p += stride)
        {
            reinterpret_cast<hkStringPtr*>(p)->~hkStringPtr();
        }
    }
}

// 0x00D19E20 (:443): emptied, the storage kept
void hkVariantDataUtil::clearArray(const hkTypeInfoRegistry* infoReg, void* arrayIn, hkClassMember::Type type,
                                   const hkClass* cls)
{
    DummyArray& array = *static_cast<DummyArray*>(arrayIn);
    if (array.getSize() != 0)
    {
        deleteArray(infoReg, type, cls, array.begin(), array.getSize(), calcElementSize(type, cls));
        array.setSizeUnchecked(0);
    }
}

// 0x00D19E60 (:459): the elements past size destroyed, the new ones constructed; the storage returned
void* hkVariantDataUtil::setArraySize(const hkTypeInfoRegistry* infoReg, void* arrayIn, hkClassMember::Type type,
                                      const hkClass* cls, int size)
{
    DummyArray& array = *static_cast<DummyArray*>(arrayIn);
    const int oldSize = array.getSize();
    if (oldSize != size)
    {
        const int sizeElem = calcElementSize(type, cls);
        if (size < array.getSize())
        {
            deleteArray(infoReg, type, cls, array.begin() + sizeElem * size, oldSize - size, sizeElem);
            array.setSizeUnchecked(size);
            return array.begin();
        }
        if (array.getCapacity() < size)
        {
            hkArrayUtil::_reserve(hkContainerHeapAllocator::s_alloc, &array, size, sizeElem);
        }
        newArray(infoReg, type, cls, array.begin() + sizeElem * oldSize, size - oldSize, sizeElem);
        array.setSizeUnchecked(size);
    }
    return array.begin();
}

// 0x00D19170 (:491)
int hkVariantDataUtil::getArraySize(void* arrayIn)
{
    return static_cast<DummyArray*>(arrayIn)->getSize();
}

// 0x00D19180 (:497)
void* hkVariantDataUtil::reserveArray(void* arrayIn, hkClassMember::Type type, const hkClass* cls, int size)
{
    const int sizeElem = calcElementSize(type, cls);
    DummyArray& array = *static_cast<DummyArray*>(arrayIn);
    hkArrayUtil::_reserve(hkContainerHeapAllocator::s_alloc, &array, size, sizeElem);
    return array.begin();
}

// 0x00D18940 (:507): the reals in a tuple of the type (at least one element); 0 for a type without reals
int hkVariantDataUtil::calcNumReals(hkClassMember::Type type, int tupleSize)
{
    tupleSize = tupleSize < 1 ? 1 : tupleSize;
    switch (type)
    {
        case hkClassMember::TYPE_REAL:
        case hkClassMember::TYPE_HALF:
            return tupleSize;
        case hkClassMember::TYPE_VECTOR4:
        case hkClassMember::TYPE_QUATERNION:
            return tupleSize * 4;
        case hkClassMember::TYPE_MATRIX3:
        case hkClassMember::TYPE_ROTATION:
        case hkClassMember::TYPE_QSTRANSFORM:
            return tupleSize * 12;
        case hkClassMember::TYPE_MATRIX4:
        case hkClassMember::TYPE_TRANSFORM:
            return tupleSize * 16;
        default:
            return 0;
    }
}

// 0x00D198A0 (:544): numRealsIn reals (all of them when negative); nothing when there are more than the type holds
void hkVariantDataUtil::setReals(hkClassMember::Type type, int tupleSize, const float* src, void* data, int numRealsIn)
{
    int numReals = calcNumReals(type, tupleSize);
    if (numRealsIn >= 0)
    {
        if (numRealsIn > numReals)
        {
            return;
        }
        numReals = numRealsIn;
    }
    if (type == hkClassMember::TYPE_HALF)
    {
        hkHalf* dst = static_cast<hkHalf*>(data);
        for (int i = 0; i < numReals; ++i)
        {
            dst[i] = src[i];
        }
    }
    else
    {
        float* dst = static_cast<float*>(data);
        for (int i = 0; i < numReals; ++i)
        {
            dst[i] = src[i];
        }
    }
}

// 0x00D19F10 (:571): the data itself for reals, halves widened into buffer
const float* hkVariantDataUtil::getReals(hkClassMember::Type type, int tupleSize, const void* data,
                                         hkArray<unsigned char,hkContainerHeapAllocator>& buffer)
{
    const int numReals = calcNumReals(type, tupleSize);
    if (numReals < 1)
    {
        return nullptr;
    }
    if (type != hkClassMember::TYPE_HALF)
    {
        return static_cast<const float*>(data);
    }
    buffer.setSize(numReals * (int)sizeof(float));
    float* dst = reinterpret_cast<float*>(buffer.begin());
    const hkHalf* src = static_cast<const hkHalf*>(data);
    for (int i = 0; i < numReals; ++i)
    {
        dst[i] = src[i].getReal();
    }
    return dst;
}

// 0x00D189B0 (:596)
void hkVariantDataUtil::setObject(const hkVariant& var, hkClassMember::Type type, void* data)
{
    switch (type)
    {
        case hkClassMember::TYPE_POINTER:
            *static_cast<void**>(data) = var.m_object;
            break;
        case hkClassMember::TYPE_VARIANT:
            *static_cast<hkVariant*>(data) = var;
            break;
        default:
            break;
    }
}

// 0x00D189E0 (:624): the class registered for the object's vtable, looked up by name in classReg
const hkClass* hkVariantDataUtil::findMostDerivedClass(const void* object, const hkVtableClassRegistry* vtable,
                                                       const hkClassNameRegistry* classReg)
{
    const hkClass* klass = vtable->getClassFromVirtualInstance(object);
    if (klass != nullptr)
    {
        return classReg->getClassByName(klass->getName());
    }
    return nullptr;
}

// 0x00D18A10 (:634): a virtual object with its most derived class (both null, with a warning, when that is unknown)
hkVariant hkVariantDataUtil::getVariantWithMostDerivedClass(const hkClass* klass, const void* object,
                                                            const hkVtableClassRegistry* vtable,
                                                            const hkClassNameRegistry* classReg)
{
    hkVariant v;
    v.m_object = nullptr;
    v.m_class = nullptr;
    if (klass != nullptr && object != nullptr)
    {
        if (klass->hasVtable())
        {
            const hkClass* derived = findMostDerivedClass(object, vtable, classReg);
            if (derived == nullptr)
            {
                // HK_WARN(0x3476d70f, ...): the release build keeps the warnings
                char buf[512];
                hkErrStream os(buf, sizeof(buf));
                os << "Could not find the most derived class for virtual object " << klass->getName() << " at "
                   << object << ". The object is replaced with nullptr.";
                hkErrorFwd::messageWarning(0x3476d70f, buf, "Reflection\\Util\\hkVariantDataUtil.cpp", 651);
                return v;
            }
            v.m_object = const_cast<void*>(object);
            v.m_class = derived;
            return v;
        }
        v.m_object = const_cast<void*>(object);
        v.m_class = klass;
    }
    return v;
}

// 0x00D18B20 (:665): the object a member of the type holds
hkVariant hkVariantDataUtil::getObject(hkClassMember::Type type, const hkClass* cls,
                                       const hkVtableClassRegistry* vtableReg, const hkClassNameRegistry* classReg,
                                       const void* data)
{
    switch (type)
    {
        case hkClassMember::TYPE_POINTER:
            return getVariantWithMostDerivedClass(cls, *static_cast<void* const*>(data), vtableReg, classReg);
        case hkClassMember::TYPE_STRUCT:
            return getVariantWithMostDerivedClass(cls, data, vtableReg, classReg);
        case hkClassMember::TYPE_VARIANT:
            return *static_cast<const hkVariant*>(data);
        default:
        {
            hkVariant v;
            v.m_object = nullptr;
            v.m_class = nullptr;
            return v;
        }
    }
}

// 0x00D191C0 (:692): a referenced virtual object is counted (the new one first)
void hkVariantDataUtil::setPointer(const hkClass& cls, void* obj, void** dst, hkBool isReferenced)
{
    if (cls.hasVtable() && isReferenced)
    {
        if (obj != nullptr)
        {
            static_cast<hkReferencedObject*>(obj)->addReference();
        }
        if (*dst != nullptr)
        {
            static_cast<hkReferencedObject*>(*dst)->removeReference();
        }
    }
    *dst = obj;
}

// 0x00D18B80 (:712)
unsigned int hkVariantDataUtil::calcBasicElementSize(hkClassMember::Type type, const hkClass* klass)
{
    if (type == hkClassMember::TYPE_STRUCT)
    {
        return klass->getObjectSize();
    }
    return hkClassMember::getClassMemberTypeProperties(type).m_size;
}

// 0x00D18BA0 (:729): an array member is an hkArray (12 bytes); a tuple counts each element
unsigned int hkVariantDataUtil::calcElementSize(hkClassMember::Type type, hkClassMember::Type subType,
                                                const hkClass* klass, int tupleSize)
{
    if (type == hkClassMember::TYPE_ARRAY)
    {
        return sizeof(DummyArray);
    }
    const hkClassMember::Type basicType =
        (type == hkClassMember::TYPE_ENUM || type == hkClassMember::TYPE_FLAGS) ? subType : type;
    const unsigned int size = calcBasicElementSize(basicType, klass);
    return tupleSize > 0 ? size * tupleSize : size;
}

// 0x00D19220 (:759)
void hkVariantDataUtil::convertTypeToInt32Array(hkClassMember::Type srcType, const void* srcIn, int* dst, int size)
{
    switch (srcType)
    {
        case hkClassMember::TYPE_BOOL:
            convertBoolToTypeArray(static_cast<const hkBool*>(srcIn), hkClassMember::TYPE_INT32, dst, size);
            break;
        case hkClassMember::TYPE_CHAR:
        case hkClassMember::TYPE_INT8:
        {
            const hkInt8* src = static_cast<const hkInt8*>(srcIn);
            for (int i = 0; i < size; ++i)
            {
                dst[i] = src[i];
            }
            break;
        }
        case hkClassMember::TYPE_UINT8:
        {
            const hkUint8* src = static_cast<const hkUint8*>(srcIn);
            for (int i = 0; i < size; ++i)
            {
                dst[i] = src[i];
            }
            break;
        }
        case hkClassMember::TYPE_INT16:
        {
            const hkInt16* src = static_cast<const hkInt16*>(srcIn);
            for (int i = 0; i < size; ++i)
            {
                dst[i] = src[i];
            }
            break;
        }
        case hkClassMember::TYPE_UINT16:
        {
            const hkUint16* src = static_cast<const hkUint16*>(srcIn);
            for (int i = 0; i < size; ++i)
            {
                dst[i] = src[i];
            }
            break;
        }
        case hkClassMember::TYPE_INT32:
        case hkClassMember::TYPE_UINT32:
            hkString::memCpy(dst, srcIn, size * (int)sizeof(int));
            break;
        case hkClassMember::TYPE_INT64:
        case hkClassMember::TYPE_UINT64:
            convertInt64ToTypeArray(static_cast<const int64_t*>(srcIn), hkClassMember::TYPE_INT32, dst, size);
            break;
        default:
            break;
    }
}

// 0x00D19370 (:803)
void hkVariantDataUtil::convertInt32ToTypeArray(const int* src, hkClassMember::Type dstType, void* dstIn, int size)
{
    switch (dstType)
    {
        case hkClassMember::TYPE_BOOL:
            convertTypeToBoolArray(hkClassMember::TYPE_INT32, src, static_cast<hkBool*>(dstIn), size);
            break;
        case hkClassMember::TYPE_CHAR:
        case hkClassMember::TYPE_INT8:
        case hkClassMember::TYPE_UINT8:
        {
            hkInt8* dst = static_cast<hkInt8*>(dstIn);
            for (int i = 0; i < size; ++i)
            {
                dst[i] = static_cast<hkInt8>(src[i]);
            }
            break;
        }
        case hkClassMember::TYPE_INT16:
        case hkClassMember::TYPE_UINT16:
        {
            hkInt16* dst = static_cast<hkInt16*>(dstIn);
            for (int i = 0; i < size; ++i)
            {
                dst[i] = static_cast<hkInt16>(src[i]);
            }
            break;
        }
        case hkClassMember::TYPE_INT32:
        case hkClassMember::TYPE_UINT32:
            hkString::memCpy(dstIn, src, size * (int)sizeof(int));
            break;
        case hkClassMember::TYPE_INT64:
        case hkClassMember::TYPE_UINT64:
        {
            // sign extended for both (original)
            hkInt64* dst = static_cast<hkInt64*>(dstIn);
            for (int i = 0; i < size; ++i)
            {
                dst[i] = src[i];
            }
            break;
        }
        default:
            break;
    }
}

// 0x00D19500 (:843)
void hkVariantDataUtil::convertUint32ToTypeArray(const unsigned int* src, hkClassMember::Type dstType, void* dstIn,
                                                 int size)
{
    switch (dstType)
    {
        case hkClassMember::TYPE_BOOL:
            convertTypeToBoolArray(hkClassMember::TYPE_UINT32, src, static_cast<hkBool*>(dstIn), size);
            break;
        case hkClassMember::TYPE_CHAR:
        case hkClassMember::TYPE_INT8:
        case hkClassMember::TYPE_UINT8:
        {
            hkUint8* dst = static_cast<hkUint8*>(dstIn);
            for (int i = 0; i < size; ++i)
            {
                dst[i] = static_cast<hkUint8>(src[i]);
            }
            break;
        }
        case hkClassMember::TYPE_INT16:
        case hkClassMember::TYPE_UINT16:
        {
            hkUint16* dst = static_cast<hkUint16*>(dstIn);
            for (int i = 0; i < size; ++i)
            {
                dst[i] = static_cast<hkUint16>(src[i]);
            }
            break;
        }
        case hkClassMember::TYPE_INT32:
        case hkClassMember::TYPE_UINT32:
            hkString::memCpy(dstIn, src, size * (int)sizeof(unsigned int));
            break;
        case hkClassMember::TYPE_INT64:
        case hkClassMember::TYPE_UINT64:
        {
            hkUint64* dst = static_cast<hkUint64*>(dstIn);
            for (int i = 0; i < size; ++i)
            {
                dst[i] = src[i];
            }
            break;
        }
        default:
            break;
    }
}

// 0x00D18BF0 (:883): non-zero is true; the source read by its storage size
void hkVariantDataUtil::convertTypeToBoolArray(hkClassMember::Type srcType, const void* srcIn, hkBool* dst, int size)
{
    if (srcType == hkClassMember::TYPE_BOOL)
    {
        hkString::memCpy(dst, srcIn, size);
        return;
    }
    switch (hkClassMember::getClassMemberTypeProperties(srcType).m_size)
    {
        case 1:
        {
            const hkUint8* src = static_cast<const hkUint8*>(srcIn);
            for (int i = 0; i < size; ++i)
            {
                dst[i] = src[i] != 0;
            }
            break;
        }
        case 2:
        {
            const hkUint16* src = static_cast<const hkUint16*>(srcIn);
            for (int i = 0; i < size; ++i)
            {
                dst[i] = src[i] != 0;
            }
            break;
        }
        case 4:
        {
            const hkUint32* src = static_cast<const hkUint32*>(srcIn);
            for (int i = 0; i < size; ++i)
            {
                dst[i] = src[i] != 0;
            }
            break;
        }
        case 8:
        {
            const hkUint64* src = static_cast<const hkUint64*>(srcIn);
            for (int i = 0; i < size; ++i)
            {
                dst[i] = src[i] != 0;
            }
            break;
        }
        default:
            break;
    }
}

// 0x00D18D00 (:938): 0 or 1 in the destination's storage size
void hkVariantDataUtil::convertBoolToTypeArray(const hkBool* src, hkClassMember::Type dstType, void* dstIn, int size)
{
    if (dstType == hkClassMember::TYPE_BOOL)
    {
        hkString::memCpy(dstIn, src, size);
        return;
    }
    switch (hkClassMember::getClassMemberTypeProperties(dstType).m_size)
    {
        case 1:
        {
            hkUint8* dst = static_cast<hkUint8*>(dstIn);
            for (int i = 0; i < size; ++i)
            {
                dst[i] = src[i].m_bool != 0;
            }
            break;
        }
        case 2:
        {
            hkUint16* dst = static_cast<hkUint16*>(dstIn);
            for (int i = 0; i < size; ++i)
            {
                dst[i] = src[i].m_bool != 0;
            }
            break;
        }
        case 4:
        {
            hkUint32* dst = static_cast<hkUint32*>(dstIn);
            for (int i = 0; i < size; ++i)
            {
                dst[i] = src[i].m_bool != 0;
            }
            break;
        }
        case 8:
        {
            hkUint64* dst = static_cast<hkUint64*>(dstIn);
            for (int i = 0; i < size; ++i)
            {
                dst[i] = src[i].m_bool != 0;
            }
            break;
        }
        default:
            break;
    }
}

// 0x00D18E10 (:992)
void hkVariantDataUtil::convertInt64ToTypeArray(const int64_t* src, hkClassMember::Type dstType, void* dstIn, int size)
{
    switch (dstType)
    {
        case hkClassMember::TYPE_BOOL:
            convertTypeToBoolArray(hkClassMember::TYPE_INT64, src, static_cast<hkBool*>(dstIn), size);
            break;
        case hkClassMember::TYPE_CHAR:
        case hkClassMember::TYPE_INT8:
        case hkClassMember::TYPE_UINT8:
        {
            hkInt8* dst = static_cast<hkInt8*>(dstIn);
            for (int i = 0; i < size; ++i)
            {
                dst[i] = static_cast<hkInt8>(src[i]);
            }
            break;
        }
        case hkClassMember::TYPE_INT16:
        case hkClassMember::TYPE_UINT16:
        {
            hkInt16* dst = static_cast<hkInt16*>(dstIn);
            for (int i = 0; i < size; ++i)
            {
                dst[i] = static_cast<hkInt16>(src[i]);
            }
            break;
        }
        case hkClassMember::TYPE_INT32:
        case hkClassMember::TYPE_UINT32:
        {
            hkInt32* dst = static_cast<hkInt32*>(dstIn);
            for (int i = 0; i < size; ++i)
            {
                dst[i] = static_cast<hkInt32>(src[i]);
            }
            break;
        }
        case hkClassMember::TYPE_INT64:
        case hkClassMember::TYPE_UINT64:
            hkString::memCpy(dstIn, src, size * (int)sizeof(int64_t));
            break;
        default:
            break;
    }
}

// 0x00D19A00 (:1032): integers through the 32-bit and 64-bit converters (the small ones 64 at a time through an int
// buffer), reals to and from halves; the other types are not converted
void hkVariantDataUtil::convertTypeToTypeArray(hkClassMember::Type srcType, const void* src,
                                               hkClassMember::Type dstType, void* dst, int size)
{
    if (srcType == dstType)
    {
        hkString::memCpy(dst, src, hkClassMember::getClassMemberTypeProperties(srcType).m_size * size);
        return;
    }
    switch (srcType)
    {
        case hkClassMember::TYPE_BOOL:
            convertBoolToTypeArray(static_cast<const hkBool*>(src), dstType, dst, size);
            break;
        case hkClassMember::TYPE_CHAR:
        case hkClassMember::TYPE_INT8:
        case hkClassMember::TYPE_UINT8:
        case hkClassMember::TYPE_INT16:
        case hkClassMember::TYPE_UINT16:
        {
            if (dstType == hkClassMember::TYPE_BOOL)
            {
                convertTypeToBoolArray(srcType, src, static_cast<hkBool*>(dst), size);
                break;
            }
            const int srcSize = hkClassMember::getClassMemberTypeProperties(srcType).m_size;
            const int dstSize = hkClassMember::getClassMemberTypeProperties(dstType).m_size;
            if (srcSize == dstSize)
            {
                hkString::memCpy(dst, src, srcSize * size);
                break;
            }
            const int bufferSize = 64; // elements converted per pass
            int buffer[bufferSize];
            const char* s = static_cast<const char*>(src);
            char* d = static_cast<char*>(dst);
            while (size > 0)
            {
                const int num = size > bufferSize ? bufferSize : size;
                convertTypeToInt32Array(srcType, s, buffer, num);
                convertInt32ToTypeArray(buffer, dstType, d, num);
                s += srcSize * bufferSize;
                d += dstSize * bufferSize;
                size -= num;
            }
            break;
        }
        case hkClassMember::TYPE_INT32:
            convertInt32ToTypeArray(static_cast<const int*>(src), dstType, dst, size);
            break;
        case hkClassMember::TYPE_UINT32:
            convertUint32ToTypeArray(static_cast<const unsigned int*>(src), dstType, dst, size);
            break;
        case hkClassMember::TYPE_INT64:
        case hkClassMember::TYPE_UINT64:
            convertInt64ToTypeArray(static_cast<const int64_t*>(src), dstType, dst, size);
            break;
        case hkClassMember::TYPE_REAL:
            if (dstType == hkClassMember::TYPE_HALF)
            {
                const float* s = static_cast<const float*>(src);
                hkHalf* d = static_cast<hkHalf*>(dst);
                for (int i = 0; i < size; ++i)
                {
                    d[i] = s[i];
                }
            }
            break;
        case hkClassMember::TYPE_HALF:
            if (dstType == hkClassMember::TYPE_REAL)
            {
                const hkHalf* s = static_cast<const hkHalf*>(src);
                float* d = static_cast<float*>(dst);
                for (int i = 0; i < size; ++i)
                {
                    s[i].store(&d[i]);
                }
            }
            break;
        default:
            break;
    }
}

// 0x00D196A0 (:1138): element by element (by word when the elements, strides and addresses are aligned to their size)
void hkVariantDataUtil::stridedCopy(const void* src, int srcStride, void* dst, int dstStride, int eleSize, int size)
{
    if (eleSize == srcStride && eleSize == dstStride)
    {
        hkString::memCpy(dst, src, eleSize * size);
        return;
    }
    const char* s = static_cast<const char*>(src);
    char* d = static_cast<char*>(dst);
    const hkUlong alignment = hkUlong(src) | hkUlong(srcStride) | hkUlong(dst) | hkUlong(dstStride);
    switch (eleSize)
    {
        case 1:
            for (int i = 0; i < size; ++i, s += srcStride, d += dstStride)
            {
                *d = *s;
            }
            return;
        case 2:
            if ((alignment & 1) == 0)
            {
                for (int i = 0; i < size; ++i, s += srcStride, d += dstStride)
                {
                    *reinterpret_cast<hkUint16*>(d) = *reinterpret_cast<const hkUint16*>(s);
                }
                return;
            }
            break;
        case 4:
            if ((alignment & 3) == 0)
            {
                for (int i = 0; i < size; ++i, s += srcStride, d += dstStride)
                {
                    *reinterpret_cast<hkUint32*>(d) = *reinterpret_cast<const hkUint32*>(s);
                }
                return;
            }
            break;
        case 8:
            if ((alignment & 7) == 0)
            {
                for (int i = 0; i < size; ++i, s += srcStride, d += dstStride)
                {
                    *reinterpret_cast<hkUint64*>(d) = *reinterpret_cast<const hkUint64*>(s);
                }
                return;
            }
            break;
        case 16:
            if ((alignment & 15) == 0)
            {
                for (int i = 0; i < size; ++i, s += srcStride, d += dstStride)
                {
                    reinterpret_cast<hkUint64*>(d)[0] = reinterpret_cast<const hkUint64*>(s)[0];
                    reinterpret_cast<hkUint64*>(d)[1] = reinterpret_cast<const hkUint64*>(s)[1];
                }
                return;
            }
            break;
        default:
            break;
    }
    for (int i = 0; i < size; ++i, s += srcStride, d += dstStride)
    {
        hkString::memCpy(d, s, eleSize);
    }
}

// 0x00D19FF0 (:1250): a strided copy when the types match; otherwise converted, through a packed temporary when one
// side is strided
void hkVariantDataUtil::convertArray(const hkStridedBasicArray& src, const hkStridedBasicArray& dst)
{
    if (src.m_size <= 0)
    {
        return;
    }
    if (src.m_type == dst.m_type && src.m_tupleSize == dst.m_tupleSize)
    {
        stridedCopy(src.m_data, src.m_stride, dst.m_data, dst.m_stride,
                    hkClassMember::getClassMemberTypeProperties(src.m_type).m_size * src.m_tupleSize, src.m_size);
        return;
    }
    const int srcEleSize = hkClassMember::getClassMemberTypeProperties(src.m_type).m_size * src.m_tupleSize;
    const int dstEleSize = hkClassMember::getClassMemberTypeProperties(dst.m_type).m_size * dst.m_tupleSize;
    if (srcEleSize == src.m_stride && dstEleSize == dst.m_stride)
    {
        convertTypeToTypeArray(src.m_type, src.m_data, dst.m_type, dst.m_data, src.m_size * src.m_tupleSize);
        return;
    }
    hkArray<char, hkContainerTempAllocator> buffer;
    if (srcEleSize == src.m_stride)
    {
        buffer.setSize(src.m_size * dstEleSize);
        convertTypeToTypeArray(src.m_type, src.m_data, dst.m_type, buffer.begin(), src.m_tupleSize * src.m_size);
        stridedCopy(buffer.begin(), dstEleSize, dst.m_data, dst.m_stride, dstEleSize, src.m_size);
    }
    else
    {
        buffer.setSize(src.m_size * srcEleSize);
        stridedCopy(src.m_data, src.m_stride, buffer.begin(), srcEleSize, srcEleSize, src.m_size);
        convertTypeToTypeArray(src.m_type, buffer.begin(), dst.m_type, dst.m_data, src.m_tupleSize * src.m_size);
    }
}
