// Original: volition/libmodules/havok/hk2014_2_5_r1/source/common/serialize/data/util/hkdataworldcloner.h
//
// Havok 2014.2.5: full declaration from rfg.pdb (stub_impl.md S4, serializer).
#pragma once

#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/memory/memoryclasses/hkmemoryclasses.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/container/array/hkarray.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/container/hkcontainerallocators.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/container/pointermap/hkmap.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/serialize/data/hkdataobject.h"

class hkDataClassImpl;
struct hkDataClass_MemberInfo;
struct hkResult;

// size 0x14
class hkDataWorldCloner
{
public:
    typedef hkMap<hkDataObject_Handle,hkDataObject_Handle,hkMapOperations<hkDataObject_Handle>,hkContainerHeapAllocator> Map;

    HK_DECLARE_NONVIRTUAL_CLASS_ALLOCATOR(hkDataWorldCloner);
    hkDataWorldCloner(hkDataWorld& dstWorld, const hkDataWorld& srcWorld); // 0x00F743D0
    hkResult clone(); // 0x00F74B30
public:
    void copyArray(hkDataArray& dst, const hkDataArray& src, int arraySize); // 0x00F73AF0
    void copyValue(hkDataObject& dstObj, const hkDataObject& srcObj, const hkDataClass_MemberInfo& minfo); // 0x00F74190
    hkDataClassImpl* findDestClass(const char* classname); // 0x00F74400
    void getClassMemberInfos(hkArray<hkDataClass_MemberInfo,hkContainerTempAllocator>& dstInfos, const hkDataClass& dstClass, const hkDataClass& srcClass); // 0x00F74760
    void copyObjectMembers(hkDataObject& dstObject, const hkDataObject& srcObject); // 0x00F74900
    hkDataObject copyObject(const hkDataObject& srcObject); // 0x00F749C0
public:
    ~hkDataWorldCloner(); // 0x00F68220

public:
    hkDataWorld& m_dstWorld; // 0x00
    const hkDataWorld& m_srcWorld; // 0x04
    hkMap<hkDataObject_Handle,hkDataObject_Handle,hkMapOperations<hkDataObject_Handle>,hkContainerHeapAllocator> m_copied; // 0x08
};
