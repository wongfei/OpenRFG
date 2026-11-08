// Original: volition/libmodules/havok/hk2014_2_5_r1/source/common/serialize/util/hkobjectinspector.cpp
//
// Havok 2014.2.5 (stub_impl.md S4, serializer SE), from rfg.exe: the pointers an object holds to other objects
// (members, arrays, variants), and a walk over the object graph they form.

#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/serialize/util/hkobjectinspector.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/serialize/resource/hkobjectresource.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/container/hkcontainerallocators.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/reflection/hkclass.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/reflection/hkclassmember.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/system/error/hkerror.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/system/io/ostream/hkostream.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/types/hkbasetypes.h"

typedef hkArray<hkObjectInspector::Pointer,hkContainerTempAllocator> PointerArray;

// 0x010571F0 (:91)
static int calcCArraySize(const hkClassMember& member)
{
    return member.getCstyleArraySize() != 0 ? member.getCstyleArraySize() : 1;
}

// 0x01057280 (:96): one entry per pointer slot
static void scanArrayOfPointers(const void* arrayStart, int arraySize, const hkClass* klass, PointerArray& ioPointers)
{
    hkObjectInspector::Pointer* p = ioPointers.expandBy(arraySize);
    void** location = static_cast<void**>(const_cast<void*>(arrayStart));
    for (int i = 0; i < arraySize; i++)
    {
        p[i].location = location + i;
        p[i].klass = klass;
    }
}

// 0x010572F0 (:111): each variant's object pointer, with the variant's class
static void scanArrayOfVariants(const void* arrayStart, int arraySize, PointerArray& ioPointers)
{
    hkObjectInspector::Pointer* p = ioPointers.expandBy(arraySize);
    hkVariant* v = static_cast<hkVariant*>(const_cast<void*>(arrayStart));
    for (int i = 0; i < arraySize; i++)
    {
        p[i].location = &v[i].m_object;
        p[i].klass = v[i].m_class;
    }
}

static hkResult scanBody(const void* data, const hkClass& klass, PointerArray& containedPointers);

// 0x01057210 (:123)
static hkResult scanArrayOfStructs(const void* arrayStart, int arraySize, const hkClass* klass, PointerArray& ioPointers)
{
    const int size = klass->getObjectSize();
    const char* s = static_cast<const char*>(arrayStart);
    for (int i = 0; i < arraySize; i++)
    {
        if (hkObjectInspector::getPointers(s, *klass, ioPointers) == HK_FAILURE)
        {
            return HK_FAILURE;
        }
        s += size;
    }
    return HK_SUCCESS;
}

// 0x01057360 (:137): pointers to structs (other pointers are not followed), the arrays' pointers, structs and
// variants, the homogeneous arrays' structs
static hkResult scanBody(const void* data, const hkClass& klass, PointerArray& containedPointers)
{
    for (int memberIndex = 0; memberIndex < klass.getNumMembers(); memberIndex++)
    {
        const hkClassMember& member = klass.getMember(memberIndex);
        const void* memberAddress = static_cast<const char*>(data) + member.getOffset();
        switch (member.getType())
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
        case hkClassMember::TYPE_ZERO:
        case hkClassMember::TYPE_FUNCTIONPOINTER:
        case hkClassMember::TYPE_ENUM:
        case hkClassMember::TYPE_CSTRING:
        case hkClassMember::TYPE_ULONG:
        case hkClassMember::TYPE_FLAGS:
        case hkClassMember::TYPE_HALF:
        case hkClassMember::TYPE_STRINGPTR:
            break;
        case hkClassMember::TYPE_POINTER:
            if (member.getSubType() == hkClassMember::TYPE_STRUCT)
            {
                scanArrayOfPointers(memberAddress, calcCArraySize(member), &member.getStructClass(), containedPointers);
            }
            break;
        case hkClassMember::TYPE_ARRAY:
        case hkClassMember::TYPE_INPLACEARRAY:
        case hkClassMember::TYPE_SIMPLEARRAY:
        {
            const hkClassMember::Type arrayType = member.getArrayType();
            const void* arrayData = *static_cast<void* const*>(memberAddress);
            const int arraySize = *reinterpret_cast<const int*>(static_cast<const char*>(memberAddress) + sizeof(void*));
            if (arrayData != nullptr)
            {
                if (arrayType == hkClassMember::TYPE_POINTER)
                {
                    scanArrayOfPointers(arrayData, arraySize, &member.getStructClass(), containedPointers);
                }
                else if (arrayType == hkClassMember::TYPE_STRUCT)
                {
                    if (scanArrayOfStructs(arrayData, arraySize, &member.getStructClass(), containedPointers) == HK_FAILURE)
                    {
                        return HK_FAILURE;
                    }
                }
                else if (arrayType == hkClassMember::TYPE_VARIANT)
                {
                    scanArrayOfVariants(arrayData, arraySize, containedPointers);
                }
            }
            break;
        }
        case hkClassMember::TYPE_STRUCT:
            if (scanArrayOfStructs(memberAddress, calcCArraySize(member), &member.getStructClass(), containedPointers) == HK_FAILURE)
            {
                return HK_FAILURE;
            }
            break;
        case hkClassMember::TYPE_HOMOGENEOUSARRAY:
        {
            // {const hkClass* klass, void* data, int size}
            const hkClass* arrayClass = *static_cast<const hkClass* const*>(memberAddress);
            const void* arrayData = *reinterpret_cast<void* const*>(static_cast<const char*>(memberAddress) + sizeof(void*));
            const int arraySize = *reinterpret_cast<const int*>(static_cast<const char*>(memberAddress) + 2 * sizeof(void*));
            if (arrayData != nullptr && arrayClass != nullptr)
            {
                if (scanArrayOfStructs(arrayData, arraySize, arrayClass, containedPointers) == HK_FAILURE)
                {
                    return HK_FAILURE;
                }
            }
            break;
        }
        case hkClassMember::TYPE_VARIANT:
            scanArrayOfVariants(memberAddress, calcCArraySize(member), containedPointers);
            break;
        case hkClassMember::TYPE_RELARRAY:
        {
            // {hkUint16 size, hkUint16 offset from the member}
            const hkClassMember::Type arrayType = member.getArrayType();
            const hkUint16* rel = static_cast<const hkUint16*>(memberAddress);
            const int arraySize = rel[0];
            if (arraySize != 0)
            {
                const void* arrayData = static_cast<const char*>(memberAddress) + rel[1];
                if (arrayType == hkClassMember::TYPE_POINTER)
                {
                    scanArrayOfPointers(arrayData, arraySize, &member.getStructClass(), containedPointers);
                }
                else if (arrayType == hkClassMember::TYPE_STRUCT)
                {
                    if (scanArrayOfStructs(arrayData, arraySize, &member.getStructClass(), containedPointers) == HK_FAILURE)
                    {
                        return HK_FAILURE;
                    }
                }
                else if (arrayType == hkClassMember::TYPE_VARIANT)
                {
                    scanArrayOfVariants(arrayData, arraySize, containedPointers);
                }
            }
            break;
        }
        default:
        {
            char buf[512];
            hkErrStream os(buf, sizeof(buf));
            os << "Unknown class member found during write of data.";
            if (hkErrorFwd::messageError(0x641E3E03, buf, "Util\\hkObjectInspector.cpp", 281) != 0)
            {
                __debugbreak(); // HK_BREAKPOINT
            }
            return HK_FAILURE;
        }
        }
    }
    return HK_SUCCESS;
}

// 0x01057640 (:290)
hkResult hkObjectInspector::getPointers(const void* object, const hkClass& klass, hkArray<hkObjectInspector::Pointer,hkContainerTempAllocator>& outList)
{
    return scanBody(object, klass, outList);
}

// 0x01057670 (:302): the listener sees each object before the objects it points to (depth first; shared and
// cyclic references are walked again)
hkResult hkObjectInspector::walkPointers(const void* object, const hkClass& klass, hkObjectInspector::ObjectListener* listener)
{
    PointerArray pointers;
    pointers.reserve(128);
    if (getPointers(object, klass, pointers) == HK_FAILURE || listener->objectCallback(object, klass, pointers) == HK_FAILURE)
    {
        return HK_FAILURE;
    }
    for (int i = 0; i < pointers.getSize(); i++)
    {
        if (*pointers[i].location != nullptr && walkPointers(*pointers[i].location, *pointers[i].klass, listener) == HK_FAILURE)
        {
            return HK_FAILURE;
        }
    }
    return HK_SUCCESS;
}
