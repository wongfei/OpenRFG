// Original: volition/libmodules/havok/hk2014_2_5_r1/source/common/serialize/data/util/hkdataworldcloner.cpp
//
// Havok 2014.2.5 (stub_impl.md S4, serializer SD), from rfg.exe: the cloner's constructor and destructor, which
// hkDataObjectUtil::deepCopyWorld runs around clone. The copy itself (clone, copyObject, copyObjectMembers,
// copyValue, copyArray, findDestClass, getClassMemberInfos) is not ported yet: no load path copies a world.

#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/serialize/data/util/hkdataworldcloner.h"

// 0x00F743D0
hkDataWorldCloner::hkDataWorldCloner(hkDataWorld& dstWorld, const hkDataWorld& srcWorld)
    : m_dstWorld(dstWorld), m_srcWorld(srcWorld)
{
}

// 0x00F68220: the copied map is freed
hkDataWorldCloner::~hkDataWorldCloner()
{
}
