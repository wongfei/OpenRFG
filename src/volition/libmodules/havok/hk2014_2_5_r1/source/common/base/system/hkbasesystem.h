// Original: volition/libmodules/havok/hk2014_2_5_r1/source/common/base/system/hkbasesystem.h
//
// Havok 2014.2.5: full declaration from rfg.pdb (havok_impl.md HK0).
#pragma once


class hkBool;
class hkMemoryRouter;
struct hkResult;

namespace hkBaseSystem
{
    void initSingletons(); // 0x00CE6320
    void quitSingletons(); // 0x00CE6520
    hkResult init(hkMemoryRouter* memoryRouter, void (*errorReportFunction)(const char*, void*), void* errorReportObject); // 0x00CE6440
    hkResult initThread(hkMemoryRouter* memoryRouter); // 0x00CE6290
    hkResult quitThread(); // 0x00CE62C0
    hkResult quit(); // 0x00CE6640
    hkBool isInitialized(); // 0x00CE6280
}
