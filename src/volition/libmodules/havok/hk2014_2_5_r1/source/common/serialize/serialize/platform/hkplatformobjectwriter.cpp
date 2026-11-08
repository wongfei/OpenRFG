// Original: volition/libmodules/havok/hk2014_2_5_r1/source/common/serialize/serialize/platform/hkplatformobjectwriter.cpp
//
// Havok 2014.2.5 (stub_impl.md S4, serializer SE), from rfg.exe: writes host objects in a target platform's
// layout through an hkObjectCopier, with the classes retargeted to the layout (deep copies, cached).

#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/serialize/serialize/platform/hkplatformobjectwriter.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/container/string/hkstring.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/reflection/hkclass.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/system/io/writer/hkstreamwriter.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/system/io/writer/offsetonly/hkoffsetonlystreamwriter.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/serialize/copier/hkdeepcopier.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/serialize/serialize/hkrelocationinfo.h"

extern const hkClass hkClassClass;

// 0x00F729F0 (:16)
hkPlatformObjectWriter::Cache::Cache()
{
}

// 0x00F72A30 (:20): the retargeted classes are deep copies
hkPlatformObjectWriter::Cache::~Cache()
{
    for (int i = 0; i < m_allocations.getSize(); i++)
    {
        hkDeepCopier::freeDeepCopy(m_allocations[i]);
    }
}

// 0x00F727F0 (:28): a deep copy of the class (and of the classes it references, through the shared copy map) with
// its member offsets computed for the layout
hkClass* hkPlatformObjectWriter::Cache::get(const hkClass* klass, const hkStructureLayout& layout)
{
    void* platformClass;
    if (m_platformClassFromHostClass.get(klass, &platformClass) == HK_SUCCESS)
    {
        return static_cast<hkClass*>(platformClass);
    }
    hkClass* copy = static_cast<hkClass*>(hkDeepCopier::deepCopy(klass, hkClassClass, &m_platformClassFromHostClass, nullptr, nullptr, hkObjectCopier::FLAG_NONE));
    m_allocations.pushBack(copy);
    layout.computeMemberOffsetsInplace(*copy, m_platformClassComputed, true);
    return copy;
}

// 0x00F72AD0 (:42): no cache when the layout is the host's (the classes need no retargeting)
hkPlatformObjectWriter::hkPlatformObjectWriter(const hkStructureLayout& layout, Cache* cache, hkFlags<enum hkObjectCopier::ObjectCopierFlagBits,unsigned int> flags)
{
    m_copier = new hkObjectCopier(hkStructureLayout(hkStructureLayout::HostLayoutRules), layout, flags);
    if (getLayout().getRules() == hkStructureLayout::HostLayoutRules)
    {
        m_cache = nullptr;
    }
    else if (cache != nullptr)
    {
        cache->addReference();
        m_cache = cache;
    }
    else
    {
        m_cache = new Cache();
    }
}

// 0x00F727C0 (:66)
hkPlatformObjectWriter::~hkPlatformObjectWriter()
{
    m_copier->removeReference();
    if (m_cache != nullptr)
    {
        m_cache->removeReference();
    }
}

// 0x00F72890 (:77): with a cache, an hkClass object is written as its retargeted copy (relocations thrown away),
// then the original is copied into an offset-only writer at the same position for the relocations; the result is
// that writer's state then.
hkResult hkPlatformObjectWriter::writeObject(hkStreamWriter* writer, const void* dataIn, const hkClass& klass, hkRelocationInfo& reloc)
{
    const hkClass* klassOut = &klass;
    if (m_cache != nullptr)
    {
        klassOut = m_cache->get(&klass, getLayout());
    }
    hkOffsetOnlyStreamWriter dummyWriter;
    hkStreamWriter* out = writer;
    if (m_cache != nullptr && hkString::strCmp(klassOut->getName(), "hkClass") == 0)
    {
        klassOut = m_cache->get(&hkClassClass, getLayout());
        const hkClass* platformData = m_cache->get(static_cast<const hkClass*>(dataIn), getLayout());
        dummyWriter.seek(writer->tell(), hkStreamWriter::STREAM_SET);
        hkRelocationInfo ignoredReloc;
        m_copier->copyObject(platformData, klass, writer, *klassOut, ignoredReloc);
        out = &dummyWriter;
    }
    m_copier->copyObject(dataIn, klass, out, *klassOut, reloc);
    return out->isOk() ? HK_SUCCESS : HK_FAILURE;
}

// 0x00F72780 (:114)
hkResult hkPlatformObjectWriter::writeRaw(hkStreamWriter* writer, const void* data, int dataLen)
{
    return writer->write(data, dataLen) == dataLen ? HK_SUCCESS : HK_FAILURE;
}

// 0x00F727B0 (:121)
const hkStructureLayout& hkPlatformObjectWriter::getLayout() const
{
    return m_copier->getLayoutOut();
}
