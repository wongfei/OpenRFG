// Original: library/modules/runtime/base/sources/win32/thread_win32.cpp
//
// keen::Thread over _beginthreadex. Threads are created suspended and run once start() hands over the argument.
// keen priorities map onto Win32 priorities in steps of 300 (>= 1000 is time critical, < -999 idle).

#include "keen/base/thread.hpp"
#include "keen/base/string.hpp"
#include "keen/base/stringscanner.hpp"

#include <float.h>
#include <process.h>
#include <stdlib.h>
#include <windows.h>

// The running keen::Thread of the calling thread, set by Thread::threadStartFunction.
static __declspec(thread) keen::Thread* s_pCurrentThread; // thread local

namespace
{
    // The structure the Visual Studio debugger reads for exception 0x406D1388 (SetThreadName).
    struct ThreadNameInfo
    {
        DWORD type;
        const char* pName;
        DWORD threadId;
        DWORD flags;
    };
}

namespace keen
{
    // 0x00BEDCF0
    Thread::Thread()
    {
        m_threadHandle = INVALID_HANDLE_VALUE;
        m_quitRequested = false;
    }

    // 0x00BEDD00
    Thread::~Thread()
    {
    }

    // 0x00BF4770
    void Thread::create(const char* pIdentifier, int (*pFunction)(const Thread&), int priority, uint32 stackSize, uint32 threadAffinity)
    {
        copyString(m_identifier, sizeof(m_identifier), pIdentifier);
        m_pFunction = pFunction;
        m_threadHandle = (void*)_beginthreadex(nullptr, stackSize, threadStartFunction, this, CREATE_SUSPENDED, &m_threadId);
        if (m_threadHandle == INVALID_HANDLE_VALUE)
        {
            exit(1);
        }

        int nativePriority;
        if (priority >= 1000)
        {
            nativePriority = THREAD_PRIORITY_TIME_CRITICAL;
        }
        else if (priority >= 600)
        {
            nativePriority = THREAD_PRIORITY_HIGHEST;
        }
        else if (priority >= 300)
        {
            nativePriority = THREAD_PRIORITY_ABOVE_NORMAL;
        }
        else if (priority >= -299)
        {
            nativePriority = THREAD_PRIORITY_NORMAL;
        }
        else if (priority >= -599)
        {
            nativePriority = THREAD_PRIORITY_BELOW_NORMAL;
        }
        else if (priority >= -999)
        {
            nativePriority = THREAD_PRIORITY_LOWEST;
        }
        else
        {
            nativePriority = THREAD_PRIORITY_IDLE;
        }
        SetThreadPriority(m_threadHandle, nativePriority);
        if (threadAffinity != 0)
        {
            SetThreadAffinityMask(m_threadHandle, threadAffinity);
        }
        m_quitRequested = false;
    }

    // 0x00BEDD10
    void Thread::destroy()
    {
        CloseHandle(m_threadHandle);
        m_threadHandle = INVALID_HANDLE_VALUE;
        m_threadId = 0;
    }

    // 0x00BEDD30
    void Thread::terminate()
    {
    }

    // 0x00BEDD40
    void Thread::start(void* pArgument)
    {
        m_pArgument = pArgument;
        ResumeThread(m_threadHandle);
    }

    // 0x00BEDD60
    bool Thread::join(uint32 timeOutInMs)
    {
        if (m_threadHandle == INVALID_HANDLE_VALUE)
        {
            return true;
        }
        if (timeOutInMs == (uint32)TimeOutInfinite)
        {
            WaitForSingleObject(m_threadHandle, INFINITE);
        }
        else if (WaitForSingleObject(m_threadHandle, timeOutInMs) == WAIT_TIMEOUT)
        {
            return false;
        }
        CloseHandle(m_threadHandle);
        m_threadHandle = INVALID_HANDLE_VALUE;
        return true;
    }

    // 0x00458920
    void* Thread::getArgument() const
    {
        return (void*)m_pArgument;
    }

    // 0x00458930
    void Thread::requestQuit()
    {
        m_quitRequested = true;
    }

    // 0x00458940
    bool Thread::isQuitRequested() const
    {
        return m_quitRequested;
    }

    // 0x00BEDDB0
    uint32 __stdcall Thread::threadStartFunction(void* pArgument)
    {
        Thread* pThread = (Thread*)pArgument;
        _control87(_DN_FLUSH, _MCW_DN);
        s_pCurrentThread = pThread;

        ThreadNameInfo info;
        info.type = 0x1000;
        info.pName = pThread->m_identifier;
        info.threadId = (DWORD)-1;
        info.flags = 0;
        __try
        {
            RaiseException(0x406D1388, 0, sizeof(info) / sizeof(ULONG_PTR), (const ULONG_PTR*)&info);
        }
        __except (EXCEPTION_EXECUTE_HANDLER)
        {
        }
        return (uint32)pThread->m_pFunction(*pThread);
    }

    // 0x00BEDE80
    void sleepCurrentThreadMilliseconds(uint32 milliseconds)
    {
        Sleep(milliseconds);
    }

    // 0x00BEDE90
    void sleepCurrentThreadMicroseconds(uint32 nanoseconds)
    {
        Sleep(nanoseconds / 1000);
    }

    // 0x00BEDEB0
    uint32 getCurrentThreadId()
    {
        return GetCurrentThreadId();
    }

    // 0x00BEDEC0
    Thread* getCurrentThread()
    {
        return s_pCurrentThread;
    }

    // 0x00BF4840
    uint32 getAvailableCpuHardwareThreadMask()
    {
        char buffer[12];
        const char* pString = buffer;
        size_t length;
        uint32 processorCount = 0;
        // Magic number: the buffer is passed as 10 characters.
        if (getenv_s(&length, buffer, 10, "NUMBER_OF_PROCESSORS") != 0 || readUint32FromDecimalString(&processorCount, &pString) != StringReadResult_Ok || processorCount == 0)
        {
            processorCount = 1;
        }
        uint32 mask = 0;
        for (uint32 i = 0; i < processorCount; ++i)
        {
            mask |= 1u << (i & 31);
        }
        return mask;
    }
}
