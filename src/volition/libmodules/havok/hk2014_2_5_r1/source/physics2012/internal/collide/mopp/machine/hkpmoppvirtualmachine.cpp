// Original: volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/internal/collide/mopp/machine/hkpmoppvirtualmachine.cpp
//
// Havok 2014.2.5 (havok_impl.md HK3): the base of the MOPP virtual machines (the out-of-line copies of its inline
// members), from rfg.exe.

#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/internal/collide/mopp/machine/hkpmoppvirtualmachine.h"

#include <xmmintrin.h>

// 0x00D99F60
hkpMoppVirtualMachine::hkpMoppVirtualMachine()
{
}

// 0x00D99F70
hkpMoppVirtualMachine::~hkpMoppVirtualMachine()
{
}

// 0x00D9A5B0
int hkpMoppVirtualMachine::getNumHits() const
{
    return m_primitives_out->getSize();
}

// 0x00E67B90
void hkpMoppVirtualMachine::initQuery(hkArray<hkpMoppPrimitiveInfo, hkContainerHeapAllocator>* primitives_out)
{
    m_primitives_out = primitives_out;
}

// 0x00E688A0
void hkpMoppVirtualMachine::addHit(unsigned int id, const unsigned int* properties)
{
    hkpMoppPrimitiveInfo& info = m_primitives_out->expandOne();
    info.ID = id;
}

// 0x00E68710: one below the truncation (cvttss2si)
int hkpMoppVirtualMachine::toIntMin(float x)
{
    return (int)((unsigned int)_mm_cvttss_si32(_mm_set_ss(x)) - 1);
}

// 0x00E68720: one above the truncation
int hkpMoppVirtualMachine::toIntMax(float x)
{
    return (int)((unsigned int)_mm_cvttss_si32(_mm_set_ss(x)) + 1);
}

// 0x00E6B830: a big-endian 24-bit value
int hkpMoppVirtualMachine::read24(const unsigned char* PC)
{
    return (PC[0] << 16) | (PC[1] << 8) | PC[2];
}
