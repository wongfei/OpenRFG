// Original: volition/libmodules/havok/hk2014_2_5_r1/source/common/serialize/data/hkdataobjectimpl.cpp
//
// Havok 2014.2.5 (stub_impl.md S4, serializer SC): assignment of a generic value by its type. From the decompile and
// asm.
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/serialize/data/hkdataobjectimpl.h"

#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/serialize/data/hkdataobject.h"

// 0x00F6EC20 (:18)
// Original: an array value is not assigned (the case does nothing), nor a tuple of anything but reals.
void hkDataObjectImpl::assignValueImpl(hkDataObjectImpl* dstObj, MemberHandle handle, const hkDataObject_Value& valueIn)
{
    hkTypeManager::Type* type = valueIn.getType();
    switch (type->getSubType())
    {
        case hkTypeManager::SUB_TYPE_BYTE:
        case hkTypeManager::SUB_TYPE_INT:
            dstObj->assign(handle, valueIn.asInt());
            break;
        case hkTypeManager::SUB_TYPE_REAL:
            dstObj->assign(handle, valueIn.asReal());
            break;
        case hkTypeManager::SUB_TYPE_CSTRING:
            dstObj->assign(handle, valueIn.asString());
            break;
        case hkTypeManager::SUB_TYPE_CLASS:
        case hkTypeManager::SUB_TYPE_POINTER:
        {
            hkDataObject obj = valueIn.asObject();
            dstObj->assign(handle, obj.getImplementation());
            break;
        }
        case hkTypeManager::SUB_TYPE_ARRAY:
            break;
        case hkTypeManager::SUB_TYPE_TUPLE:
            if (type->getParent()->isReal())
            {
                const int nreal = type->getTupleSize();
                dstObj->assign(handle, valueIn.asVec(nreal), nreal);
            }
            break;
        default:
            break;
    }
}

// 0x00F6ED70 (:70)
// An array of tuples (other than vecs) stores arrays.
void hkDataArrayImpl::set(int index, const hkDataObject_Value& val)
{
    hkTypeManager::Type* type = getType();
    if (type->isTuple() && !type->getParent()->isReal())
    {
        hkDataArray arr = val.asArray();
        setArray(index, arr.getImplementation());
        return;
    }
    switch (getType()->getSubType())
    {
        case hkTypeManager::SUB_TYPE_BYTE:
        case hkTypeManager::SUB_TYPE_INT:
            setInt(index, val.asInt());
            break;
        case hkTypeManager::SUB_TYPE_REAL:
            setReal(index, val.asReal());
            break;
        case hkTypeManager::SUB_TYPE_CSTRING:
            setString(index, val.asString());
            break;
        case hkTypeManager::SUB_TYPE_CLASS:
        case hkTypeManager::SUB_TYPE_POINTER:
        {
            hkDataObject obj = val.asObject();
            setObject(index, obj.getImplementation());
            break;
        }
        case hkTypeManager::SUB_TYPE_ARRAY:
        {
            hkDataArray arr = val.asArray();
            setArray(index, arr.getImplementation());
            break;
        }
        case hkTypeManager::SUB_TYPE_TUPLE:
            if (type->getParent()->isReal())
            {
                setVec(index, val.asVec(type->getTupleSize()));
            }
            break;
        default:
            break;
    }
}
