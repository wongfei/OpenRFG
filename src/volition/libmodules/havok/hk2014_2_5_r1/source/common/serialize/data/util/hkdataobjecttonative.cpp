// Original: volition/libmodules/havok/hk2014_2_5_r1/source/common/serialize/data/util/hkdataobjecttonative.cpp
//
// Havok 2014.2.5 (stub_impl.md S4, serializer SD), from rfg.exe (copyIntoNativeArray, copyIntoRelArray and
// fillNativeMembers from the asm, their decompiles are LTCG-garbled): fills native objects from data objects, member
// by member through the class reflection. Pointers out are only recorded (CopyInfoOut) for the Copier to resolve;
// every allocation made is recorded too.

#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/serialize/data/util/hkdataobjecttonative.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/container/string/hkstring.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/math/matrix/hktransformf.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/memory/router/hkmemoryrouter.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/reflection/attributes/hkattributes.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/reflection/hkclass.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/reflection/registry/hkclassnameregistry.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/reflection/util/hkvariantdatautil.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/serialize/data/hkdataobjectimpl.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/serialize/data/util/hkdataobjectutil.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/serialize/resource/hkobjectresource.h"

namespace
{
    // The storage of an hkArray<T>(n) on the heap, handed to the native array (the allocator may round the size up:
    // the capacity is what it gave).
    template<typename T>
    T* allocArray(int n, int& capacity)
    {
        int numBytes = n * (int)sizeof(T);
        T* p = static_cast<T*>(hkMemoryRouter::getInstance().heap().bufAlloc(numBytes));
        capacity = numBytes / (int)sizeof(T);
        return p;
    }

    // An hkHalf from a float: its top 16 bits
    hkInt16 toHalf(float f)
    {
        union
        {
            float f;
            hkInt32 i;
        } u;
        u.f = f;
        return (hkInt16)(u.i >> 16);
    }
}

// 0x00F75550 (:50)
const hkClass* hkDataObjectToNative::findClassOf(const hkDataObject& obj)
{
    return m_classReg->getClassByName(obj.getClass().getName());
}

// 0x00F756D0 (:59): zeroed heap memory for the object and the storage of its relArrays, recorded as an allocation
hkVariant hkDataObjectToNative::allocateObject(const hkDataObject& obj, CopyInfoOut& infoOut)
{
    hkVariant v;
    if (obj.getClass().getImplementation() != nullptr && obj.getClass().getName() != nullptr)
    {
        const hkClass* klass = m_classReg->getClassByName(obj.getClass().getName());
        if (klass != nullptr)
        {
            obj.getClass().getVersion(); // compared with klass->getDescribedVersion() in an assert
            klass->getDescribedVersion();
            const int size = klass->getObjectSize() + hkDataObjectUtil::getExtraStorageSize(klass, obj);
            void* p = hkMemoryRouter::getInstance().heap().blockAlloc(size);
            infoOut.addAlloc(p, size);
            hkString::memSet(p, 0, size);
            v.m_class = klass;
            v.m_object = p;
            return v;
        }
    }
    v.m_class = nullptr;
    v.m_object = nullptr;
    return v;
}

// 0x00F75580 (:91): the member types hkVariantDataUtil::convertArray copies
static hkBool32 canFastCopy(hkClassMember::Type type)
{
    return (type > hkClassMember::TYPE_VOID && type < hkClassMember::TYPE_ZERO) || type == hkClassMember::TYPE_ULONG;
}

// 0x00F757E0 (:133): an hkArray (TYPE_ARRAY, not owning its storage unless on the heap) or hkSimpleArray of the
// member's subtype; basic types converted in one go, the others element by element
hkResult hkDataObjectToNative::copyIntoNativeArray(void* address, const hkClassMember& member, const hkDataArray& srcArray, CopyInfoOut& copyInfoOut)
{
    int capacity = -1;
    if (srcArray.getSize() == 0)
    {
        return HK_SUCCESS;
    }
    DummyArray* dst = static_cast<DummyArray*>(address);
    const hkClassMember::Type stype = member.m_subtype;
    const hkClassMember::TypeProperties& props = hkClassMember::getClassMemberTypeProperties(stype);
    if (props.m_size > 0 && canFastCopy(stype))
    {
        hkStridedBasicArray src;
        if (srcArray.asStridedBasicArray(src) == HK_SUCCESS)
        {
            const int elemSize = props.m_size;
            const int numBytes = src.m_size * elemSize;
            int allocBytes = numBytes;
            void* data = hkMemoryRouter::getInstance().heap().bufAlloc(allocBytes);
            capacity = allocBytes / elemSize;
            hkStridedBasicArray d;
            d.m_type = stype;
            d.m_tupleSize = 1;
            d.m_data = data;
            d.m_size = src.m_size;
            d.m_stride = elemSize;
            switch (stype)
            {
                case hkClassMember::TYPE_VECTOR4:
                case hkClassMember::TYPE_QUATERNION:
                    d.m_tupleSize = 4;
                    d.m_type = hkClassMember::TYPE_REAL;
                    break;
                case hkClassMember::TYPE_MATRIX3:
                case hkClassMember::TYPE_ROTATION:
                case hkClassMember::TYPE_QSTRANSFORM:
                    d.m_tupleSize = 12;
                    d.m_type = hkClassMember::TYPE_REAL;
                    break;
                case hkClassMember::TYPE_MATRIX4:
                case hkClassMember::TYPE_TRANSFORM:
                    d.m_tupleSize = 16;
                    d.m_type = hkClassMember::TYPE_REAL;
                    break;
                default:
                    break;
            }
            hkVariantDataUtil::convertArray(src, d);
            if (elemSize > 0)
            {
                dst->data = data;
                dst->size = src.m_size;
                copyInfoOut.addAlloc(data, numBytes);
            }
            if (member.m_type == hkClassMember::TYPE_ARRAY)
            {
                dst->capacity = m_allocatedOnHeap ? capacity : (capacity | hkArrayBase<char>::DONT_DEALLOCATE_FLAG);
            }
            return HK_SUCCESS;
        }
    }

    hkResult result = HK_SUCCESS;
    const hkBool32 isOwning = (member.m_flags.get() & hkClassMember::NOT_OWNED) == 0;
    const int n = srcArray.getSize();
    void* data = nullptr;
    int elemSize = 0;
    switch (stype)
    {
        case hkClassMember::TYPE_VOID:
            goto capacityOnly;
        case hkClassMember::TYPE_BOOL:
        {
            hkBool* p = allocArray<hkBool>(n, capacity);
            data = p;
            elemSize = 1;
            for (int i = 0; i < n; i++)
            {
                p[i] = srcArray[i].asInt() != 0;
            }
            break;
        }
        case hkClassMember::TYPE_CHAR:
        case hkClassMember::TYPE_INT8:
        case hkClassMember::TYPE_UINT8:
        {
            hkUint8* p = allocArray<hkUint8>(n, capacity);
            data = p;
            elemSize = 1;
            for (int i = 0; i < n; i++)
            {
                p[i] = (hkUint8)srcArray[i].asInt();
            }
            break;
        }
        case hkClassMember::TYPE_INT16:
        case hkClassMember::TYPE_UINT16:
        {
            hkInt16* p = allocArray<hkInt16>(n, capacity);
            data = p;
            elemSize = 2;
            for (int i = 0; i < n; i++)
            {
                p[i] = (hkInt16)srcArray[i].asInt();
            }
            break;
        }
        case hkClassMember::TYPE_INT32:
        case hkClassMember::TYPE_UINT32:
        {
            hkInt32* p = allocArray<hkInt32>(n, capacity);
            data = p;
            elemSize = 4;
            for (int i = 0; i < n; i++)
            {
                p[i] = srcArray[i].asInt();
            }
            break;
        }
        case hkClassMember::TYPE_INT64:
        case hkClassMember::TYPE_UINT64:
        {
            hkInt64* p = allocArray<hkInt64>(n, capacity);
            data = p;
            elemSize = 8;
            for (int i = 0; i < n; i++)
            {
                p[i] = srcArray[i].asInt64();
            }
            break;
        }
        case hkClassMember::TYPE_ULONG:
        {
            hkUlong* p = allocArray<hkUlong>(n, capacity);
            data = p;
            elemSize = 4;
            for (int i = 0; i < n; i++)
            {
                p[i] = (hkUlong)srcArray[i].asInt64();
            }
            break;
        }
        case hkClassMember::TYPE_REAL:
        {
            float* p = allocArray<float>(n, capacity);
            data = p;
            elemSize = 4;
            for (int i = 0; i < n; i++)
            {
                p[i] = srcArray[i].asReal();
            }
            break;
        }
        case hkClassMember::TYPE_HALF:
        {
            hkInt16* p = allocArray<hkInt16>(n, capacity);
            data = p;
            elemSize = 2;
            for (int i = 0; i < n; i++)
            {
                p[i] = toHalf(srcArray[i].asReal());
            }
            break;
        }
        case hkClassMember::TYPE_VECTOR4:
        case hkClassMember::TYPE_QUATERNION:
        case hkClassMember::TYPE_MATRIX3:
        case hkClassMember::TYPE_ROTATION:
        case hkClassMember::TYPE_QSTRANSFORM:
        case hkClassMember::TYPE_MATRIX4:
        case hkClassMember::TYPE_TRANSFORM:
        {
            // the source's reals are contiguous: one copy from the first element
            elemSize = (stype == hkClassMember::TYPE_VECTOR4 || stype == hkClassMember::TYPE_QUATERNION) ? 16
                     : (stype == hkClassMember::TYPE_MATRIX4 || stype == hkClassMember::TYPE_TRANSFORM) ? 64 : 48;
            const int numBytes = n * elemSize;
            int allocBytes = numBytes;
            data = hkMemoryRouter::getInstance().heap().bufAlloc(allocBytes);
            capacity = allocBytes / elemSize;
            hkString::memCpy(data, srcArray.getImplementation()->asVec(0), numBytes);
            break;
        }
        case hkClassMember::TYPE_STRUCT:
        {
            const hkClass* sclass;
            {
                hkDataObject first = srcArray[0].asObject();
                sclass = getMemberClassAndCheck(member, first);
            }
            if (sclass == nullptr)
            {
                goto capacityOnly;
            }
            int allocBytes = sclass->getObjectSize() * n;
            char* p = static_cast<char*>(hkMemoryRouter::getInstance().heap().bufAlloc(allocBytes));
            capacity = allocBytes / sclass->getObjectSize();
            elemSize = sclass->getObjectSize();
            data = p;
            hkString::memSet(p, 0, sclass->getObjectSize() * srcArray.getSize());
            for (int i = 0; i < n; i++)
            {
                if (result != HK_SUCCESS)
                {
                    break;
                }
                hkDataObject obj = srcArray[i].asObject();
                result = fillNativeMembers(p + sclass->getObjectSize() * i, obj, copyInfoOut);
            }
            if (elemSize <= 0)
            {
                goto capacityOnly;
            }
            break;
        }
        case hkClassMember::TYPE_POINTER:
        {
            void** p = allocArray<void*>(n, capacity);
            data = p;
            hkString::memSet(p, 0, n * 4);
            elemSize = 4;
            for (int i = 0; i < n; i++)
            {
                hkDataObject obj = srcArray[i].asObject();
                copyInfoOut.addPointer(obj, &p[i], isOwning);
            }
            break;
        }
        case hkClassMember::TYPE_VARIANT:
        {
            hkVariant* p = allocArray<hkVariant>(n, capacity);
            data = p;
            hkString::memSet(p, 0, n * 8);
            elemSize = 8;
            for (int i = 0; i < n; i++)
            {
                hkDataObject obj = srcArray[i].asObject();
                copyInfoOut.addVariant(obj, &p[i], isOwning);
            }
            break;
        }
        case hkClassMember::TYPE_CSTRING:
        case hkClassMember::TYPE_STRINGPTR:
        {
            char** p = allocArray<char*>(n, capacity);
            data = p;
            elemSize = 4;
            for (int i = 0; i < n; i++)
            {
                if (result != HK_SUCCESS)
                {
                    break;
                }
                result = fillNativeSimpleMember(&p[i], stype, srcArray[i], nullptr, isOwning, copyInfoOut);
            }
            break;
        }
        default:
            return HK_FAILURE;
    }
    dst->data = data;
    dst->size = n;
    copyInfoOut.addAlloc(data, n * elemSize);
capacityOnly:
    if (member.m_type == hkClassMember::TYPE_ARRAY)
    {
        dst->capacity = m_allocatedOnHeap ? capacity : (capacity | hkArrayBase<char>::DONT_DEALLOCATE_FLAG);
    }
    return result;
}

// 0x00F76730 (:449): an hkRelArray's elements go to relArrayAddress (the storage after the object, 16-byte aligned
// per array); the array holds its size and its offset from itself
hkResult hkDataObjectToNative::copyIntoRelArray(void* address, const hkClassMember& member, const hkDataArray& srcArray, CopyInfoOut& copyInfoOut, void*& relArrayAddress, void* objectStart)
{
    DummyRelArray* dst = static_cast<DummyRelArray*>(address);
    hkResult result = HK_SUCCESS;
    if (srcArray.getSize() == 0)
    {
        *static_cast<hkUint32*>(address) = 0;
        return HK_SUCCESS;
    }
    const hkBool32 isOwning = (member.m_flags.get() & hkClassMember::NOT_OWNED) == 0;
    const hkClassMember::Type stype = member.m_subtype;
    unsigned short n = 0;
    int elemSize = 0;
    switch (stype)
    {
        case hkClassMember::TYPE_VOID:
            return result;
        case hkClassMember::TYPE_BOOL:
        {
            n = (unsigned short)srcArray.getSize();
            elemSize = 1;
            for (int i = 0; i < srcArray.getSize(); i++)
            {
                static_cast<hkBool*>(relArrayAddress)[i] = srcArray[i].asInt() != 0;
            }
            break;
        }
        case hkClassMember::TYPE_CHAR:
        case hkClassMember::TYPE_INT8:
        case hkClassMember::TYPE_UINT8:
        {
            n = (unsigned short)srcArray.getSize();
            elemSize = 1;
            for (int i = 0; i < srcArray.getSize(); i++)
            {
                static_cast<hkUint8*>(relArrayAddress)[i] = (hkUint8)srcArray[i].asInt();
            }
            break;
        }
        case hkClassMember::TYPE_INT16:
        case hkClassMember::TYPE_UINT16:
        {
            n = (unsigned short)srcArray.getSize();
            elemSize = 2;
            for (int i = 0; i < srcArray.getSize(); i++)
            {
                static_cast<hkInt16*>(relArrayAddress)[i] = (hkInt16)srcArray[i].asInt();
            }
            break;
        }
        case hkClassMember::TYPE_INT32:
        case hkClassMember::TYPE_UINT32:
        {
            n = (unsigned short)srcArray.getSize();
            elemSize = 4;
            for (int i = 0; i < srcArray.getSize(); i++)
            {
                static_cast<hkInt32*>(relArrayAddress)[i] = srcArray[i].asInt();
            }
            break;
        }
        case hkClassMember::TYPE_INT64:
        case hkClassMember::TYPE_UINT64:
        {
            n = (unsigned short)srcArray.getSize();
            elemSize = 8;
            for (int i = 0; i < srcArray.getSize(); i++)
            {
                static_cast<hkInt64*>(relArrayAddress)[i] = srcArray[i].asInt64();
            }
            break;
        }
        case hkClassMember::TYPE_ULONG:
        {
            n = (unsigned short)srcArray.getSize();
            elemSize = 4;
            for (int i = 0; i < srcArray.getSize(); i++)
            {
                static_cast<hkUlong*>(relArrayAddress)[i] = (hkUlong)srcArray[i].asInt64();
            }
            break;
        }
        case hkClassMember::TYPE_REAL:
        {
            n = (unsigned short)srcArray.getSize();
            elemSize = 4;
            for (int i = 0; i < srcArray.getSize(); i++)
            {
                static_cast<float*>(relArrayAddress)[i] = srcArray[i].asReal();
            }
            break;
        }
        case hkClassMember::TYPE_HALF:
        {
            n = (unsigned short)srcArray.getSize();
            elemSize = 2;
            for (int i = 0; i < srcArray.getSize(); i++)
            {
                static_cast<hkInt16*>(relArrayAddress)[i] = toHalf(srcArray[i].asReal());
            }
            break;
        }
        case hkClassMember::TYPE_VECTOR4:
        case hkClassMember::TYPE_QUATERNION:
        case hkClassMember::TYPE_MATRIX3:
        case hkClassMember::TYPE_ROTATION:
        case hkClassMember::TYPE_QSTRANSFORM:
        case hkClassMember::TYPE_MATRIX4:
        case hkClassMember::TYPE_TRANSFORM:
        {
            n = (unsigned short)srcArray.getSize();
            elemSize = (stype == hkClassMember::TYPE_VECTOR4 || stype == hkClassMember::TYPE_QUATERNION) ? 16
                     : (stype == hkClassMember::TYPE_MATRIX4 || stype == hkClassMember::TYPE_TRANSFORM) ? 64 : 48;
            hkString::memCpy(relArrayAddress, srcArray.getImplementation()->asVec(0), n * elemSize);
            break;
        }
        case hkClassMember::TYPE_STRUCT:
        {
            const hkClass* sclass;
            {
                hkDataObject first = srcArray[0].asObject();
                sclass = getMemberClassAndCheck(member, first);
            }
            if (sclass == nullptr)
            {
                return result;
            }
            n = (unsigned short)srcArray.getSize();
            elemSize = sclass->getObjectSize();
            hkString::memSet(relArrayAddress, 0, sclass->getObjectSize() * srcArray.getSize());
            for (int i = 0; i < srcArray.getSize(); i++)
            {
                if (result != HK_SUCCESS)
                {
                    break;
                }
                char* p = static_cast<char*>(relArrayAddress) + sclass->getObjectSize() * i;
                hkDataObject obj = srcArray[i].asObject();
                result = fillNativeMembers(p, obj, copyInfoOut);
            }
            if (elemSize < 1)
            {
                return result;
            }
            break;
        }
        case hkClassMember::TYPE_POINTER:
        {
            n = (unsigned short)srcArray.getSize();
            elemSize = 4;
            hkString::memSet(relArrayAddress, 0, n * 4);
            for (int i = 0; i < srcArray.getSize(); i++)
            {
                hkDataObject obj = srcArray[i].asObject();
                copyInfoOut.addPointer(obj, &static_cast<void**>(relArrayAddress)[i], isOwning);
            }
            break;
        }
        case hkClassMember::TYPE_VARIANT:
        {
            n = (unsigned short)srcArray.getSize();
            elemSize = 8;
            hkString::memSet(relArrayAddress, 0, n * 8);
            for (int i = 0; i < srcArray.getSize(); i++)
            {
                hkDataObject obj = srcArray[i].asObject();
                copyInfoOut.addVariant(obj, &static_cast<hkVariant*>(relArrayAddress)[i], isOwning);
            }
            break;
        }
        case hkClassMember::TYPE_CSTRING:
        case hkClassMember::TYPE_STRINGPTR:
        {
            n = (unsigned short)srcArray.getSize();
            elemSize = 4;
            for (int i = 0; i < srcArray.getSize(); i++)
            {
                if (result != HK_SUCCESS)
                {
                    break;
                }
                result = fillNativeSimpleMember(&static_cast<char**>(relArrayAddress)[i], stype, srcArray[i], nullptr,
                                                isOwning, copyInfoOut);
            }
            break;
        }
        default:
            return HK_FAILURE;
    }
    dst->offset = (unsigned short)((hkUlong)relArrayAddress - (hkUlong)address);
    relArrayAddress = static_cast<char*>(relArrayAddress) + ((n * elemSize + 15) & ~15);
    dst->size = n;
    return result;
}

// 0x00F75670 (:649)
hkResult hkDataObjectToNative::fillNativeEnumMember(void* address, const hkClassMember& member, const hkDataObject_Value& value, const hkClass& klass)
{
    if (member.m_type != hkClassMember::TYPE_ENUM && member.m_type != hkClassMember::TYPE_FLAGS)
    {
        return HK_FAILURE;
    }
    if (member.m_enum != nullptr)
    {
        member.setEnumValue(address, value.asInt());
    }
    return HK_SUCCESS;
}

// :689 (<hkDataArray_Value> 0x00F78720, <hkDataObject_Value> 0x00F78C30): one value of a basic type, a struct,
// a pointer or variant (recorded) or a string. Strings cannot be owned by heap objects except as hkStringPtr (whose
// low bit marks an owned string).
template<typename Value>
hkResult hkDataObjectToNative::fillNativeSimpleMember(void* address, hkClassMember::Type mtype, const Value& value, const hkClass* structClass, hkBool32 owningReference, CopyInfoOut& copyInfoOut)
{
    switch (mtype)
    {
        case hkClassMember::TYPE_BOOL:
            *static_cast<hkBool*>(address) = value.asInt() != 0;
            return HK_SUCCESS;
        case hkClassMember::TYPE_CHAR:
        case hkClassMember::TYPE_INT8:
        case hkClassMember::TYPE_UINT8:
            *static_cast<hkUint8*>(address) = (hkUint8)value.asInt();
            return HK_SUCCESS;
        case hkClassMember::TYPE_INT16:
        case hkClassMember::TYPE_UINT16:
            *static_cast<hkInt16*>(address) = (hkInt16)value.asInt();
            return HK_SUCCESS;
        case hkClassMember::TYPE_INT32:
        case hkClassMember::TYPE_UINT32:
            *static_cast<hkInt32*>(address) = value.asInt();
            return HK_SUCCESS;
        case hkClassMember::TYPE_INT64:
        case hkClassMember::TYPE_UINT64:
            *static_cast<hkInt64*>(address) = value.asInt64();
            return HK_SUCCESS;
        case hkClassMember::TYPE_REAL:
            *static_cast<float*>(address) = value.asReal();
            return HK_SUCCESS;
        case hkClassMember::TYPE_VECTOR4:
        case hkClassMember::TYPE_QUATERNION:
        {
            const float* r = value.asVec(4);
            float* d = static_cast<float*>(address);
            for (int i = 0; i < 4; i++)
            {
                d[i] = r[i];
            }
            return HK_SUCCESS;
        }
        case hkClassMember::TYPE_MATRIX3:
        case hkClassMember::TYPE_ROTATION:
        case hkClassMember::TYPE_QSTRANSFORM:
        {
            const float* r = value.asVec(12);
            float* d = static_cast<float*>(address);
            for (int i = 0; i < 12; i++)
            {
                d[i] = r[i];
            }
            return HK_SUCCESS;
        }
        case hkClassMember::TYPE_MATRIX4:
        {
            const float* r = value.asVec(16);
            float* d = static_cast<float*>(address);
            for (int i = 0; i < 16; i++)
            {
                d[i] = r[i];
            }
            return HK_SUCCESS;
        }
        case hkClassMember::TYPE_TRANSFORM:
            *static_cast<hkTransformf*>(address) = *reinterpret_cast<const hkTransformf*>(value.asVec(16));
            return HK_SUCCESS;
        case hkClassMember::TYPE_POINTER:
        {
            if (structClass != nullptr)
            {
                hkDataObject obj = value.asObject();
                copyInfoOut.addPointer(obj, address, owningReference);
            }
            return HK_SUCCESS;
        }
        case hkClassMember::TYPE_STRUCT:
        {
            if (structClass != nullptr)
            {
                hkDataObject obj = value.asObject();
                if (fillNativeMembers(address, obj, copyInfoOut) == HK_FAILURE)
                {
                    return HK_FAILURE;
                }
            }
            return HK_SUCCESS;
        }
        case hkClassMember::TYPE_VARIANT:
        {
            if (m_allocatedOnHeap)
            {
                return HK_FAILURE;
            }
            hkDataObject obj = value.asObject();
            copyInfoOut.addVariant(obj, static_cast<hkVariant*>(address), owningReference);
            return HK_SUCCESS;
        }
        case hkClassMember::TYPE_CSTRING:
        {
            if (m_allocatedOnHeap)
            {
                return HK_FAILURE;
            }
            if (value.asString() == nullptr)
            {
                *static_cast<char**>(address) = nullptr;
                return HK_SUCCESS;
            }
            char* s = hkString::strDup(value.asString());
            if (s != nullptr)
            {
                copyInfoOut.addAlloc(s, -1);
            }
            *static_cast<char**>(address) = s;
            return HK_SUCCESS;
        }
        case hkClassMember::TYPE_ULONG:
            *static_cast<hkUlong*>(address) = (hkUlong)value.asInt64();
            return HK_SUCCESS;
        case hkClassMember::TYPE_HALF:
            *static_cast<hkInt16*>(address) = toHalf(value.asReal());
            return HK_SUCCESS;
        case hkClassMember::TYPE_STRINGPTR:
        {
            if (value.asString() == nullptr)
            {
                *static_cast<char**>(address) = nullptr;
                return HK_SUCCESS;
            }
            char* s = hkString::strDup(value.asString());
            if (s != nullptr)
            {
                if (m_allocatedOnHeap)
                {
                    *static_cast<char**>(address) = s + 1; // hkStringPtr's owned flag
                    return HK_SUCCESS;
                }
                copyInfoOut.addAlloc(s, -1);
            }
            *static_cast<char**>(address) = s;
            return HK_SUCCESS;
        }
        default:
            return HK_FAILURE;
    }
}

// 0x00F77100 (:842): the members of the class and its parents. A member the object does not set keeps the
// declared default when the object was created during patching; a member without a value is still filled when the
// world has a default for it, and so is an embedded struct that is serialized.
hkResult hkDataObjectToNative::fillNativeMembers(void* dstObject, const hkDataObject& srcObj, CopyInfoOut& copyInfoOut)
{
    hkResult result = HK_SUCCESS;
    const hkClass* klass = findClassOf(srcObj);
    void* relArrayAddress = static_cast<char*>(dstObject) + ((klass->getObjectSize() + 15) & ~15);
    for (; klass != nullptr; klass = klass->getParent())
    {
        for (int memberIndex = 0; memberIndex < klass->getNumDeclaredMembers(); memberIndex++)
        {
            const hkClassMember& member = klass->getDeclaredMember(memberIndex);
            const hkClassMember::Type mtype = member.m_type;
            void* address = static_cast<char*>(dstObject) + member.m_offset;
            const hkDataObject_Value value = srcObj[member.m_name];
            if (!value.isSet())
            {
                hkDataClass_MemberInfo minfo;
                if (value.m_impl != nullptr)
                {
                    value.getMemberInfo(minfo);
                }
                if (mtype == hkClassMember::TYPE_ARRAY)
                {
                    static_cast<DummyArray*>(address)->capacity = hkArrayBase<char>::DONT_DEALLOCATE_FLAG;
                }
                if (klass->hasDeclaredDefault(memberIndex) &&
                    const_cast<hkDataObjectImpl*>(srcObj.getImplementation())->createdDuringPatching())
                {
                    hkString::memCpy(address, klass->getDeclaredDefault(memberIndex), member.getSizeInBytes());
                    continue;
                }
                const bool hasWorldDefault = value.m_impl != nullptr && minfo.m_valuePtr != nullptr;
                const bool isSerializedStruct = mtype == hkClassMember::TYPE_STRUCT &&
                                                (member.m_flags.get() & hkClassMember::SERIALIZE_IGNORED) == 0;
                if (!hasWorldDefault && !isSerializedStruct)
                {
                    continue;
                }
            }
            int nelem;
            int elemSize;
            if (member.getCstyleArraySize() != 0)
            {
                nelem = member.getCstyleArraySize();
                elemSize = member.getSizeInBytes() / nelem;
            }
            else
            {
                nelem = 1;
                elemSize = member.getSizeInBytes();
            }
            const hkBool32 isOwning = (member.m_flags.get() & hkClassMember::NOT_OWNED) == 0;
            switch (mtype)
            {
                case hkClassMember::TYPE_BOOL:
                case hkClassMember::TYPE_CHAR:
                case hkClassMember::TYPE_INT8:
                case hkClassMember::TYPE_UINT8:
                case hkClassMember::TYPE_INT16:
                case hkClassMember::TYPE_UINT16:
                case hkClassMember::TYPE_INT32:
                case hkClassMember::TYPE_UINT32:
                case hkClassMember::TYPE_INT64:
                case hkClassMember::TYPE_UINT64:
                case hkClassMember::TYPE_REAL:
                case hkClassMember::TYPE_VECTOR4:
                case hkClassMember::TYPE_QUATERNION:
                case hkClassMember::TYPE_MATRIX3:
                case hkClassMember::TYPE_ROTATION:
                case hkClassMember::TYPE_QSTRANSFORM:
                case hkClassMember::TYPE_MATRIX4:
                case hkClassMember::TYPE_TRANSFORM:
                case hkClassMember::TYPE_STRUCT:
                case hkClassMember::TYPE_VARIANT:
                case hkClassMember::TYPE_CSTRING:
                case hkClassMember::TYPE_ULONG:
                case hkClassMember::TYPE_HALF:
                case hkClassMember::TYPE_STRINGPTR:
                {
                    if (nelem == 1)
                    {
                        result = fillNativeSimpleMember(address, mtype, value, member.getClass(), isOwning, copyInfoOut);
                    }
                    else
                    {
                        // a C-style array: a tuple in the world
                        hkDataArray a = value.asArray();
                        for (int j = 0; j < a.getSize() && result == HK_SUCCESS; j++)
                        {
                            result = fillNativeSimpleMember(address, mtype, a[j], member.getClass(), isOwning, copyInfoOut);
                            address = static_cast<char*>(address) + elemSize;
                        }
                    }
                    break;
                }
                case hkClassMember::TYPE_POINTER:
                {
                    if (member.m_subtype == hkClassMember::TYPE_CHAR)
                    {
                        // char*: a string
                        if (nelem == 1)
                        {
                            result = fillNativeSimpleMember(address, hkClassMember::TYPE_CSTRING, value, nullptr, isOwning, copyInfoOut);
                        }
                        else
                        {
                            hkDataArray a = value.asArray();
                            for (int j = 0; j < a.getSize() && result == HK_SUCCESS; j++)
                            {
                                result = fillNativeSimpleMember(address, hkClassMember::TYPE_CSTRING, a[j], nullptr, isOwning, copyInfoOut);
                                address = static_cast<char*>(address) + elemSize;
                            }
                        }
                    }
                    else if (member.m_subtype == hkClassMember::TYPE_STRUCT)
                    {
                        if (nelem == 1)
                        {
                            hkDataObject obj = value.asObject();
                            result = fillNativeSimpleMember(address, mtype, value, getMemberClassAndCheck(member, obj), isOwning, copyInfoOut);
                        }
                        else
                        {
                            hkDataArray a = value.asArray();
                            for (int j = 0; j < a.getSize() && result == HK_SUCCESS; j++)
                            {
                                hkDataObject obj = a[j].asObject();
                                result = fillNativeSimpleMember(address, mtype, a[j], getMemberClassAndCheck(member, obj), isOwning, copyInfoOut);
                                address = static_cast<char*>(address) + elemSize;
                            }
                        }
                    }
                    break;
                }
                case hkClassMember::TYPE_ARRAY:
                case hkClassMember::TYPE_SIMPLEARRAY:
                {
                    // a heap object cannot own simple arrays, variants or C strings
                    if (m_allocatedOnHeap && (mtype == hkClassMember::TYPE_SIMPLEARRAY ||
                                              member.m_subtype == hkClassMember::TYPE_VARIANT ||
                                              member.m_subtype == hkClassMember::TYPE_CSTRING))
                    {
                        return HK_FAILURE;
                    }
                    hkDataArray a = value.asArray();
                    result = copyIntoNativeArray(address, member, a, copyInfoOut);
                    break;
                }
                case hkClassMember::TYPE_RELARRAY:
                {
                    if (m_allocatedOnHeap && (mtype == hkClassMember::TYPE_SIMPLEARRAY ||
                                              member.m_subtype == hkClassMember::TYPE_VARIANT ||
                                              member.m_subtype == hkClassMember::TYPE_CSTRING))
                    {
                        return HK_FAILURE;
                    }
                    hkDataArray a = value.asArray();
                    result = copyIntoRelArray(address, member, a, copyInfoOut, relArrayAddress, dstObject);
                    break;
                }
                case hkClassMember::TYPE_ENUM:
                case hkClassMember::TYPE_FLAGS:
                {
                    result = fillNativeEnumMember(address, member, value, *klass);
                    break;
                }
                case hkClassMember::TYPE_HOMOGENEOUSARRAY:
                {
                    // { class, data, size }: the class of the first element for all of them
                    if (m_allocatedOnHeap)
                    {
                        return HK_FAILURE;
                    }
                    struct Homogeneous
                    {
                        const hkClass* klass;
                        void* data;
                        int size;
                    };
                    Homogeneous* h = static_cast<Homogeneous*>(address);
                    hkDataArray a = value.asArray();
                    {
                        hkDataObject first = a[0].asObject();
                        h->klass = getMemberClassAndCheck(member, first);
                    }
                    h->size = a.getSize();
                    const int objectSize = h->klass->getObjectSize();
                    int allocBytes = h->size * objectSize;
                    h->data = hkMemoryRouter::getInstance().heap().bufAlloc(allocBytes);
                    hkString::memSet(h->data, 0, allocBytes);
                    copyInfoOut.addAlloc(h->data, h->size * objectSize);
                    for (int j = 0; j < h->size && result == HK_SUCCESS; j++)
                    {
                        hkDataObject obj = a[j].asObject();
                        result = fillNativeMembers(static_cast<char*>(h->data) + j * objectSize, obj, copyInfoOut);
                    }
                    break;
                }
                default:
                    return HK_FAILURE;
            }
            if (result == HK_FAILURE)
            {
                return HK_FAILURE;
            }
        }
    }
    return result;
}

// 0x00F755A0 (:1080): the object's class when the member's class is it or a base of it (or a base of the class the
// member's hk.DataObjectType attribute names); the member's class otherwise
const hkClass* hkDataObjectToNative::getMemberClassAndCheck(const hkClassMember& member, const hkDataObject& obj)
{
    if (obj.isNull())
    {
        return nullptr;
    }
    const hkClass* memberClass = member.getClass();
    const char* objClassName = obj.getClass().getName();
    const hkClass* objClass = objClassName != nullptr ? m_classReg->getClassByName(objClassName) : nullptr;
    if (memberClass == nullptr)
    {
        return objClass;
    }
    if (objClass == nullptr)
    {
        return nullptr;
    }
    if (hkString::strCmp(memberClass->getName(), objClassName) != 0)
    {
        if (memberClass->isSuperClass(*objClass))
        {
            return objClass;
        }
        const hkVariant* attr = member.getAttribute("hk.DataObjectType");
        if (attr != nullptr)
        {
            memberClass = m_classReg->getClassByName(static_cast<const hkDataObjectTypeAttribute*>(attr->m_object)->m_typeName);
            if (memberClass->isSuperClass(*objClass))
            {
                return objClass;
            }
        }
    }
    return memberClass;
}
