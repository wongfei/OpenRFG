// Original: volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/internal/collide/mopp/code/hkpmoppcode.cpp
//
// Havok 2014.2.5 (havok_impl.md HK3): the MOPP byte code and the frame it is built in (m_info.m_offset: the origin,
// and in w the scale to the 16.16 fixed point integers of the code), from rfg.exe.

#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/internal/collide/mopp/code/hkpmoppcode.h"

// 0x008319B0 (hkpmoppcode.inl): over external code bytes, which it does not own
hkpMoppCode::hkpMoppCode(const CodeInfo& info, const unsigned char* moppData, int moppBytes, BuildType buildType)
{
    m_info.m_offset = info.m_offset;
    m_data.m_size = moppBytes;
    m_data.m_capacityAndFlags = moppBytes | hkArrayBase<unsigned char>::DONT_DEALLOCATE_FLAG;
    m_data.m_data = const_cast<unsigned char*>(moppData);
    m_buildType.m_storage = (signed char)buildType;
}

// 0x00E33F30
hkpMoppCode::hkpMoppCode(hkFinishLoadedObjectFlag f)
    : hkReferencedObject(f)
    , m_data(f)
{
}

// 0x00831A30 (.h:108)
hkpMoppCode::~hkpMoppCode()
{
}

// 0x00D7C860
int hkpMoppCode::getCodeSize() const
{
    return m_data.getSize();
}

// 0x00D9A610
// The members of a code object built in raw storage (the shapes make one on the stack from their copies of the code
// info and data). Original quirk kept: the capacity has no DONT_DEALLOCATE flag; such an object is never destroyed.
void hkpMoppCode::initialize(const CodeInfo& info, const unsigned char* moppData, int moppBytes, BuildType buildType)
{
    m_info.m_offset = info.m_offset;
    m_data.m_size = moppBytes;
    m_data.m_capacityAndFlags = moppBytes;
    m_data.m_data = const_cast<unsigned char*>(moppData);
    m_buildType.m_storage = (signed char)buildType;
}

// 0x00D9A5A0
float hkpMoppCode::CodeInfo::getScale() const
{
    return m_offset(3);
}
