// Original: volition/libmodules/havok/hk2014_2_5_r1/source/common/base/thread/criticalsection/hkcriticalsection.h
//
// Havok 2014.2.5: full declaration from rfg.pdb (havok_impl.md HK0).
#pragma once

#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/memory/memoryclasses/hkmemoryclasses.h"
// size 0x18
class hkCriticalSection
{
public:
    HK_DECLARE_NONVIRTUAL_CLASS_ALLOCATOR(hkCriticalSection); // new(unsigned int nbytes) 0x00CFE880, delete(void* p, unsigned int nbytes) 0x00CFE8A0
    hkCriticalSection(int spinCount); // 0x00CFE910
    ~hkCriticalSection(); // 0x00CE7470
    void enter(); // 0x00CE7480
    void leave(); // 0x00CE7490
    static void setTimersEnabled();
    static void setTimersDisabled();
    static unsigned int atomicExchangeAdd(unsigned int* var, int increment); // 0x00CFE310

    unsigned char m_section[0x18]; /* _RTL_CRITICAL_SECTION */ // 0x00
};
