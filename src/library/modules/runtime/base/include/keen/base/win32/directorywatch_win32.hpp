// Original: library/modules/runtime/base/include/keen/base/win32/directorywatch_win32.hpp
//
// DirectoryWatch, watches directories for file changes (hot reload of resources).
#pragma once

#include "keen/base/types.hpp"
#include <windows.h>
#include "keen/base/dynamicarray.hpp"
#include "keen/base/win32/mutex_win32.hpp"
#include "keen/base/win32/thread_win32.hpp"

namespace keen
{
    class MemoryAllocator;

    // size 0xD8
    class DirectoryWatch
    {
    public:
        struct WatchInfo;
        typedef DynamicArray<WatchInfo> WatchInfos;
        struct ChangeInfo;
        typedef DynamicArray<ChangeInfo> ChangeInfos;

        // size 0x10C
        struct ChangeInfo
        {
            uint32 flags; // 0x00
            char filename[260]; // 0x04
            uint32 time; // 0x108
        };

        // size 0x1120
        struct WatchInfo
        {
            char directory[260]; // 0x00
            void* directoryHandle; // 0x104
            uint8 buffer[4096]; // 0x108
            unsigned long bufferSize; // 0x1108
            _OVERLAPPED overlapped; // 0x110C
        };

        DirectoryWatch(); // 0x00BF41D0 (:10)
        ~DirectoryWatch(); // 0x00BF4220 (:15)
        void create(MemoryAllocator* pAllocator); // 0x00C0A030 (:20)
        void destroy(); // 0x00BFC2B0 (:38)
        void addDirectory(const char* pDirectory); // 0x00C0A0C0 (:57)
        void clearDirectories(); // 0x00BF4230 (:50)
        void start(uint32 watchFlags); // 0x00C0CFA0 (:103)
        void stop(); // 0x00BF4240 (:126)
        bool hasFileChanged(uint32* pFlags, char* pFilename, uint32 filenameCapacity); // 0x00C00FC0 (:144)
        bool hasErrorOccoured() const;
    private:
        static int threadFunction(const Thread& thread); // 0x00C0C9E0 (:175)
        void startWatch(); // 0x00C0A150 (:190)
        void updateWatch(); // 0x00C0A310 (:236)
        void endWatch(); // 0x00C01100 (:334)
        void addFileChange(const char* pDirectory, const char* pFilename, uint32 flags); // 0x00C071C0 (:356)

        Mutex m_mutex; // 0x00
        MemoryAllocator* m_pAllocator; // 0x38
        Thread m_watchThread; // 0x3C
        DynamicArray<WatchInfo> m_directories; // 0x90
        DynamicArray<ChangeInfo> m_changedFiles; // 0xAC
        uint32 m_watchFlags; // 0xC8
        unsigned long m_watchFilter; // 0xCC
        void* m_ioPort; // 0xD0
        bool m_isRunning; // 0xD4
        bool m_hasErrorOccured; // 0xD5
    };
}
