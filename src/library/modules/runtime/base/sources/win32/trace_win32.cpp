// Original: library/modules/runtime/base/sources/win32/trace_win32.cpp
//
// Debug output: nativePrint sends text to the debugger (OutputDebugStringW) and, once initializeLogFile has run, to
// the log file. ANSI escape sequences ("\x1b[...") are stripped, and the text is flushed in chunks of at most 1024
// bytes.
//
// OpenRFG addition: with OPENRFG_NATIVE_PRINT_STDOUT defined, every chunk is also written to stdout, so the headless
// builds show the log in the console.

#include "keen/base/trace.hpp"
#include "keen/base/filepath.hpp"
#include "keen/base/ioresult.hpp"
#include "keen/base/nativefile.hpp"
#include "keen/base/string.hpp"
#include "keen/base/stringencoding.hpp"

#include <stdarg.h>
#include <windows.h>

#if defined(OPENRFG_NATIVE_PRINT_STDOUT)
#include <stdio.h>
#endif

static wchar_t s_logFileName[260]; // 0x038B63B0
static void* s_logFileHandle = INVALID_HANDLE_VALUE; // 0x016EBEF8

namespace keen
{
    namespace debug
    {
        // Not in a header; called by nothing else in the binary.
        void writeToLogFile(const char* pString);

        static void flushNativePrintChunk(char* pChunk)
        {
            wchar_t wideText[1026];
            convertUTF8ToUTF16((uint16*)wideText, sizeof(wideText) - 2, pChunk);
            OutputDebugStringW(wideText);
#if defined(OPENRFG_NATIVE_PRINT_STDOUT)
            fputs(pChunk, stdout);
#endif
            if (s_logFileHandle != INVALID_HANDLE_VALUE)
            {
                DWORD length = 0;
                while (pChunk[length] != '\0')
                {
                    ++length;
                }
                DWORD written = 0;
                if (!WriteFile(s_logFileHandle, pChunk, length, &written, nullptr))
                {
                    getIoErrorFromLastError();
                }
            }
        }

        // 0x00C0B080 (:20)
        void initializeLogFile(const char* pFileName)
        {
            // A relative name is placed next to the executable.
            FilePath path;
            path.setCompletePath(pFileName, false);
            const char* pPrefix = path.getPrefix();
            const char* pDirectory = path.getDirectory();
            const bool hasDrive = pPrefix[0] != '\0' && pPrefix[1] == ':' && pPrefix[2] == '\0';
            const bool isNetworkPath = pPrefix[0] == '/' && pPrefix[1] == '/';
            if (!hasDrive && !isNetworkPath && pDirectory[0] != '/' && pDirectory[0] != ':')
            {
                wchar_t wideExePath[260];
                char exePath[260];
                GetModuleFileNameW(nullptr, wideExePath, 260);
                convertUTF16ToUTF8(exePath, sizeof(exePath), (const uint16*)wideExePath);
                FilePath exeFilePath;
                exeFilePath.setCompletePath(exePath, false);
                FilePath combinedPath;
                combinedPath.setPrefix(exeFilePath.getPrefix());
                combinedPath.setDirectory("");
                combinedPath.pushDirectory(exeFilePath.getDirectory());
                combinedPath.setFileNameWithExtension(path.getFileNameWithExtension());
                path = combinedPath;
            }

            // Inlined keen::openFile(path, FileAccessMode_Write) from nativefile_win32.cpp.
            char fileName[260];
            wchar_t wideFileName[520];
            void* fileHandle = INVALID_HANDLE_VALUE;
            IoError error = IoError_InvalidArgument;
            if (copyString(fileName, sizeof(fileName), path.getCompletePath()) <= sizeof(fileName))
            {
                for (char* p = fileName; *p != '\0'; ++p)
                {
                    if (*p == '/')
                    {
                        *p = '\\';
                    }
                }
                if (convertUTF8ToUTF16((uint16*)wideFileName, sizeof(wideFileName), fileName) <= sizeof(wideFileName))
                {
                    fileHandle = CreateFileW(wideFileName, GENERIC_WRITE, FILE_SHARE_READ, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
                    error = fileHandle == INVALID_HANDLE_VALUE ? getIoErrorFromLastError() : IoError_Ok;
                }
            }
            if (error == IoError_Ok)
            {
                s_logFileHandle = fileHandle;
            }
        }

        // 0x00BEDEE0 (:52)
        void shutdownLogFile()
        {
            void* fileHandle = s_logFileHandle;
            if (fileHandle != INVALID_HANDLE_VALUE)
            {
                s_logFileHandle = INVALID_HANDLE_VALUE;
                MemoryBarrier();
                if (!CloseHandle(fileHandle))
                {
                    getIoErrorFromLastError();
                }
            }
        }

        // 0x00BF48D0
        void writeToLogFile(const char* pString)
        {
            if (s_logFileHandle == INVALID_HANDLE_VALUE)
            {
                return;
            }
            DWORD length = 0;
            if (pString != nullptr)
            {
                while (pString[length] != '\0')
                {
                    ++length;
                }
            }
            DWORD written = 0;
            if (!WriteFile(s_logFileHandle, pString, length, &written, nullptr))
            {
                getIoErrorFromLastError();
            }
        }

        // 0x00BF4920 (:71)
        void nativePrint(const char* pMessage)
        {
            char chunk[1028];
            uint32 chunkLength = 0;
            while (*pMessage != '\0')
            {
                const bool isEscape = pMessage[0] == '\x1b' && pMessage[1] == '[';
                if (chunkLength != 0 && (isEscape || chunkLength == 1024))
                {
                    chunk[chunkLength] = '\0';
                    flushNativePrintChunk(chunk);
                    chunkLength = 0;
                }
                if (chunkLength == 0 && isEscape)
                {
                    // Skip the parameter bytes up to and including the final byte (0x40..0x7E).
                    const char* p = pMessage + 2;
                    while (*p != '\0')
                    {
                        if (*p > '?' && *p < '\x7f')
                        {
                            ++p;
                            break;
                        }
                        ++p;
                    }
                    pMessage = p;
                    continue;
                }
                chunk[chunkLength++] = *pMessage++;
            }
            if (chunkLength != 0)
            {
                chunk[chunkLength] = '\0';
                flushNativePrintChunk(chunk);
            }
        }

        // 0x00C07580 (:139)
        void nativePrintArgs(const char* pFormat, ...)
        {
            char buffer[2048];
            va_list arguments;
            va_start(arguments, pFormat);
            formatStringVarArgs(buffer, sizeof(buffer), pFormat, arguments);
            va_end(arguments);
            nativePrint(buffer);
        }
    }
}
