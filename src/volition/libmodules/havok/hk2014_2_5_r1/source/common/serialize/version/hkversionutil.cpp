// Original: volition/libmodules/havok/hk2014_2_5_r1/source/common/serialize/version/hkversionutil.cpp
//
// Havok 2014.2.5 (stub_impl.md S4, serializer SD; recomputeClassMemberOffsets in SE), from rfg.exe.

#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/serialize/version/hkversionutil.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/container/pointermap/hkpointermap.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/reflection/hkclass.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/serialize/util/hkstructurelayout.h"

// 0x00F658B0 (:19)
const char* hkVersionUtil::getCurrentVersion()
{
    return "hk_2014.2.5-r1";
}

// 0x00F659E0: the null-terminated class list's metadata updated from classVersion, then every class (and the
// classes its members use) laid out for the host. For the current version's classes this rewrites the offsets and
// object sizes the generated reflection already holds (rfg.exe's host values).
void hkVersionUtil::recomputeClassMemberOffsets(hkClass* const* classes, int classVersion)
{
    hkClass::updateMetadataInplace(const_cast<hkClass**>(classes), classVersion);
    hkStructureLayout layout;
    hkPointerMap<hkClass const*,int,hkContainerHeapAllocator> classesDone;
    for (int i = 0; classes[i] != nullptr; i++)
    {
        layout.computeMemberOffsetsInplace(*classes[i], classesDone, true);
    }
}
