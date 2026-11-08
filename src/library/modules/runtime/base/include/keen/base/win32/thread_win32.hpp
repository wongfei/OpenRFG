// Original: library/modules/runtime/base/include/keen/base/win32/thread_win32.hpp
// Implementation: library/modules/runtime/base/sources/win32/thread_win32.cpp
#pragma once

#include "keen/base/types.hpp"

namespace keen
{
    // Win32 thread wrapper. The entry function receives the Thread itself; the user argument is fetched via getArgument().
    // size 0x54
    class Thread
    {
    public:
        enum
        {
            TimeOutInfinite = -1,
        };

        Thread(); // 0x00BEDCF0
        ~Thread(); // 0x00BEDD00
        void create(const char* pIdentifier, int (*pFunction)(const Thread&), int priority, uint32 stackSize, uint32 threadAffinity); // 0x00BF4770
        void destroy(); // 0x00BEDD10
        void start(void* pArgument); // 0x00BEDD40
        bool join(uint32 timeOutInMs); // 0x00BEDD60
        void terminate(); // 0x00BEDD30
        const char* getName() const;
        uint32 getId() const;
        void* getArgument() const; // 0x00458920
        void requestQuit(); // 0x00458930
        bool isQuitRequested() const; // 0x00458940

    private:
        static uint32 __stdcall threadStartFunction(void* pArgument); // 0x00BEDDB0

        void* m_threadHandle; // 0x00
        uint32 m_threadId; // 0x04
        volatile void* m_pArgument; // 0x08
        char m_identifier[64]; // 0x0C
        bool m_quitRequested; // 0x4C
        int (*m_pFunction)(const Thread&); // 0x50
    };
}
