// Original: volition/libmodules/havok/hk2014_2_5_r1/source/common/base/thread/thread/hkthreadlocaldata.h
//
// Havok 2014.2.5 (hand-written from the PDB instantiations, havok_impl.md HK0): hkThreadLocalData, a Win32 TLS slot (TlsAlloc in the
// constructor, TlsFree in the destructor). HK_THREAD_LOCAL(T) is hkThreadLocalData<T> on Win32 (Havok 2014.1
// hkThreadLocalData.h), so the memory router's per-thread instance does not use the image's .tls section.
#pragma once

// size 0x4
template<typename T>
class hkThreadLocalData
{
public:
    hkThreadLocalData(); // <hkMemoryRouter *> 0x00CE7840
    ~hkThreadLocalData(); // <hkMemoryRouter *> 0x00CE7850
    T getData() const; // <hkMemoryRouter *> 0x00718740 (.h:69)
    void setData(T p); // <hkMemoryRouter *> 0x00CE7860

    unsigned long m_slotID; // 0x00
};
