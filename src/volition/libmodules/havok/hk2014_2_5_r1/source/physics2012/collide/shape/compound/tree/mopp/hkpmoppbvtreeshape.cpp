// Original: volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/shape/compound/tree/mopp/hkpmoppbvtreeshape.cpp
//
// Havok 2014.2.5 (havok_impl.md HK3): a shape collection under a MOPP code (the tree of its keys), from rfg.exe. The
// queries run the MOPP virtual machines on a code object built in raw stack storage from the shape's copies of the
// code info and data (hkpMoppCode::initialize). The shipped build records "rcMopp"/"rcBundleMopp" monitor timers
// (TLS hkMonitorStream) around the ray casts; they are not reproduced.

#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/shape/compound/tree/mopp/hkpmoppbvtreeshape.h"

#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/types/geometry/aabb/hkaabb.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/shape/compound/collection/hkpshapecollection.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/shape/query/hkpshaperaybundlecastinput.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/shape/query/hkpshaperaycastoutput.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/internal/collide/mopp/code/hkpmoppcode.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/internal/collide/mopp/machine/hkpmopplongrayvirtualmachine.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/internal/collide/mopp/machine/hkpmoppobbvirtualmachine.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/internal/collide/mopp/machine/hkpmoppraybundlevirtualmachine.h"

namespace
{
// A code object in raw storage (no constructor or destructor runs) over the shape's copies.
struct alignas(16) MoppCodeStorage
{
    unsigned char bytes[sizeof(hkpMoppCode)];

    const hkpMoppCode* init(const hkMoppBvTreeShapeBase& shape)
    {
        hkpMoppCode* code = reinterpret_cast<hkpMoppCode*>(bytes);
        code->initialize(reinterpret_cast<const hkpMoppCode::CodeInfo&>(shape.m_codeInfoCopy), shape.m_moppData, shape.m_moppDataSize, hkpMoppCode::BUILD_NOT_SET);
        return code;
    }
};

// The box clipped to the code's range: its origin up to 2^24 code units (16.16 fixed point) past it.
void clipToCode(const hkVector4f& codeInfo, const hkAabb& aabb, hkAabb& clipped)
{
    clipped.m_min.setMax(codeInfo, aabb.m_min);
    hkVector4 extent; extent.setAll(16777216.0f / codeInfo(3)); // 2^24 / scale
    hkVector4 codeMax; codeMax.setAdd(extent, codeInfo);
    clipped.m_max.setMin(codeMax, aabb.m_max);
}
}

// ---- hkMoppBvTreeShapeBase

// 0x00D9A100: the code gets a reference; the shape keeps copies of its info and data
hkMoppBvTreeShapeBase::hkMoppBvTreeShapeBase(hkcdShapeType::ShapeTypeEnum type, const hkpMoppCode* code)
    : hkpBvTreeShape(type, BVTREE_MOPP)
{
    m_code = code;
    m_code->addReference();
    m_codeInfoCopy = code->m_info.m_offset;
    m_moppData = code->m_data.begin();
    m_moppDataSize = code->m_data.getSize();
}

// 0x00D7C940
hkMoppBvTreeShapeBase::hkMoppBvTreeShapeBase(hkFinishLoadedObjectFlag flag)
    : hkpBvTreeShape(flag)
{
    if (flag.m_finishing)
    {
        afterReflectNew();
    }
}

// 0x00D7C880
hkMoppBvTreeShapeBase::~hkMoppBvTreeShapeBase()
{
    m_code->removeReference();
}

// 0x00D7C870
void hkMoppBvTreeShapeBase::afterReflectNew()
{
    m_bvTreeType.m_storage = BVTREE_MOPP;
}

// 0x00D9A050
void hkMoppBvTreeShapeBase::queryAabb(const hkAabb& aabb, hkArray<unsigned int, hkContainerHeapAllocator>& hits) const
{
    hkAabb clipped;
    clipToCode(m_codeInfoCopy, aabb, clipped);
    hkpMoppObbVirtualMachine vm;
    vm.queryAabb(m_code, clipped, reinterpret_cast<hkArray<hkpMoppPrimitiveInfo, hkContainerHeapAllocator>*>(&hits));
}

// 0x00D9A460
// The keys go to hits (maxNumKeys of them; more grow a heap array that is dropped); returns the number found.
unsigned int hkMoppBvTreeShapeBase::queryAabbImpl(const hkAabb& aabb, unsigned int* hits, int maxNumKeys) const
{
    MoppCodeStorage codeStorage;
    const hkpMoppCode* code = codeStorage.init(*this);
    hkAabb clipped;
    clipToCode(m_codeInfoCopy, aabb, clipped);
    hkArray<hkpMoppPrimitiveInfo, hkContainerHeapAllocator> hitArray;
    hitArray.m_data = reinterpret_cast<hkpMoppPrimitiveInfo*>(hits);
    hitArray.m_size = 0;
    hitArray.m_capacityAndFlags = maxNumKeys | hkArrayBase<hkpMoppPrimitiveInfo>::DONT_DEALLOCATE_FLAG;
    hkpMoppObbVirtualMachine vm;
    vm.queryAabb(code, clipped, &hitArray);
    return (unsigned int)vm.m_primitives_out->getSize();
}

// 0x00D9A010
void hkMoppBvTreeShapeBase::queryObb(const hkTransformf& obbToMopp, const hkVector4f& extent, float tolerance, hkArray<unsigned int, hkContainerHeapAllocator>& hits) const
{
    hkpMoppObbVirtualMachine vm;
    vm.queryObb(m_code, obbToMopp, extent, tolerance, reinterpret_cast<hkArray<hkpMoppPrimitiveInfo, hkContainerHeapAllocator>*>(&hits));
}

// ---- hkpMoppBvTreeShape

// 0x00D9A170: the collection gets a reference
hkpMoppBvTreeShape::hkpMoppBvTreeShape(const hkpShapeCollection* collection, const hkpMoppCode* code)
    : hkMoppBvTreeShapeBase(hkcdShapeType::MOPP, code)
    , m_child(collection, hkpShapeContainer::REFERENCE_POLICY_INCREMENT)
{
}

// 0x00D7C990
hkpMoppBvTreeShape::hkpMoppBvTreeShape(hkFinishLoadedObjectFlag flag)
    : hkMoppBvTreeShapeBase(flag)
    , m_child(flag)
{
    if (flag.m_finishing)
    {
        afterReflectNew();
    }
}

// 0x00D9A1A0 (the child's reference goes with m_child, the code's with the base)
hkpMoppBvTreeShape::~hkpMoppBvTreeShape()
{
}

// 0x00D7C970: the copies of the loaded code
void hkpMoppBvTreeShape::afterReflectNew()
{
    m_type.m_storage = hkcdShapeType::MOPP;
    m_codeInfoCopy = m_code->m_info.m_offset;
    m_moppData = m_code->m_data.begin();
    m_moppDataSize = m_code->m_data.getSize();
}

// 0x00DB45E0
const hkpMoppCode* hkpMoppBvTreeShape::getMoppCode() const
{
    return m_code;
}

// 0x0080DBE0 (hkpmoppbvtreeshape.inl:28)
const hkpShapeCollection* hkpMoppBvTreeShape::getShapeCollection() const
{
    return static_cast<const hkpShapeCollection*>(m_child.getChild());
}

// 0x00E25910
const hkpShape* hkpMoppBvTreeShape::getChild() const
{
    return m_child.getChild();
}

// 0x00D7C9F0: the collection's container
const hkpShapeContainer* hkpMoppBvTreeShape::getContainer() const
{
    return m_child.getChild()->getContainer();
}

// 0x00D99FE0: the collection's box
void hkpMoppBvTreeShape::getAabb(const hkTransformf& localToWorld, float tolerance, hkAabb& out) const
{
    m_child.getChild()->getAabb(localToWorld, tolerance, out);
}

// 0x00D9A0B0: SPU collision needs a chunked code and a child of at most 256 bytes
int hkpMoppBvTreeShape::calcSizeForSpu(const hkpShape::CalcSizeForSpuInput& input, int spuBufferSizeLeft) const
{
    if (m_code->m_buildType.m_storage != hkpMoppCode::BUILT_WITH_CHUNK_SUBDIVISION)
    {
        return -1;
    }
    const int childSize = m_child.getChild()->calcSizeForSpu(input, 256); // 256: the SPU child shape buffer
    if (childSize < 0 || childSize > 256)
    {
        return -1;
    }
    const_cast<hkpMoppBvTreeShape*>(this)->m_childSize = childSize;
    return sizeof(*this);
}

// 0x00D9A1E0
hkBool hkpMoppBvTreeShape::castRay(const hkpShapeRayCastInput& input, hkpShapeRayCastOutput& results) const
{
    MoppCodeStorage codeStorage;
    const hkpMoppCode* code = codeStorage.init(*this);
    hkpMoppLongRayVirtualMachine vm;
    const hkpShapeCollection* collection = static_cast<const hkpShapeCollection*>(m_child.getChild());
    return vm.queryLongRay(collection, code, input, results);
}

// 0x00D9A390
void hkpMoppBvTreeShape::castRayWithCollector(const hkpShapeRayCastInput& input, const hkpCdBody& cdBody, hkpRayHitCollector& collector) const
{
    MoppCodeStorage codeStorage;
    const hkpMoppCode* code = codeStorage.init(*this);
    hkpMoppLongRayVirtualMachine vm;
    const hkpShapeCollection* collection = static_cast<const hkpShapeCollection*>(m_child.getChild());
    vm.queryLongRay(collection, code, input, cdBody, collector);
}

// 0x00D9A2C0
hkVector4fComparison hkpMoppBvTreeShape::castRayBundle(const hkpShapeRayBundleCastInput& input, hkpShapeRayBundleCastOutput& results, const hkVector4fComparison& mask) const
{
    MoppCodeStorage codeStorage;
    const hkpMoppCode* code = codeStorage.init(*this);
    hkpMoppRayBundleVirtualMachine vm;
    const hkpShapeCollection* collection = static_cast<const hkpShapeCollection*>(m_child.getChild());
    return vm.queryRayBundle(collection, code, input, results, mask);
}
