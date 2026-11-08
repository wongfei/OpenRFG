// Original: library/modules/runtime/base/sources/win32/nativefile_win32.cpp
//
// Thin wrappers over the Win32 file API. Paths are UTF-8 with '/' or '\\' separators; files are opened shared for
// reading. Reads and writes take an absolute 64-bit position.

#include "keen/base/nativefile.hpp"
#include "keen/base/filepath.hpp"
#include "keen/base/string.hpp"
#include "keen/base/stringencoding.hpp"
#include "keen/base/trace.hpp"

#include <sys/stat.h>
#include <windows.h>
#include <objbase.h>
#include <shlobj.h>
#include <shlwapi.h>

#pragma comment(lib, "shlwapi.lib")
#pragma comment(lib, "ole32.lib")

namespace keen
{
    // 0x00BED860 (:18)
    IoError getIoErrorFromLastError()
    {
        switch (GetLastError())
        {
        case ERROR_SUCCESS:
            return IoError_Ok;
        case ERROR_FILE_NOT_FOUND:
        case ERROR_PATH_NOT_FOUND:
            return IoError_FileNotFound;
        case ERROR_ACCESS_DENIED:
            return IoError_NoPermission;
        case ERROR_INVALID_HANDLE:
            return IoError_InvalidArgument;
        case ERROR_NOT_ENOUGH_MEMORY:
        case ERROR_OUTOFMEMORY:
            return IoError_OutOfMemory;
        case ERROR_HANDLE_EOF:
            return IoError_EofReached;
        case ERROR_ALREADY_EXISTS:
            return IoError_AlreadyExists;
        default:
            return IoError_Generic;
        }
    }

    // 0x00C0A840 (:50)
    IoResult<void*> openFile(const char* pFilename, FileAccessMode accessMode)
    {
        IoResult<void*> result;
        result.result = INVALID_HANDLE_VALUE;

        DWORD desiredAccess;
        DWORD creationDisposition;
        switch (accessMode)
        {
        case FileAccessMode_Read:
            desiredAccess = GENERIC_READ;
            creationDisposition = OPEN_EXISTING;
            break;
        case FileAccessMode_Write:
            desiredAccess = GENERIC_WRITE;
            creationDisposition = CREATE_ALWAYS;
            break;
        case FileAccessMode_Append:
            desiredAccess = GENERIC_WRITE;
            creationDisposition = OPEN_ALWAYS;
            break;
        case FileAccessMode_ReadWrite:
            desiredAccess = GENERIC_READ | GENERIC_WRITE;
            creationDisposition = OPEN_ALWAYS;
            break;
        default:
            debug::nativePrintArgs("[base] Unsupported FileAccessMode!");
            result.error = IoError_InvalidArgument;
            return result;
        }

        char fileName[260];
        wchar_t wideFileName[520];
        if (copyString(fileName, sizeof(fileName), pFilename) > sizeof(fileName))
        {
            result.error = IoError_InvalidArgument;
            return result;
        }
        for (char* p = fileName; *p != '\0'; ++p)
        {
            if (*p == '/')
            {
                *p = '\\';
            }
        }
        if (convertUTF8ToUTF16((uint16*)wideFileName, sizeof(wideFileName), fileName) > sizeof(wideFileName))
        {
            result.error = IoError_InvalidArgument;
            return result;
        }
        HANDLE file = CreateFileW(wideFileName, desiredAccess, FILE_SHARE_READ, nullptr, creationDisposition, FILE_ATTRIBUTE_NORMAL, nullptr);
        if (file == INVALID_HANDLE_VALUE)
        {
            result.error = getIoErrorFromLastError();
            return result;
        }
        if (accessMode == FileAccessMode_Append)
        {
            LARGE_INTEGER distance;
            distance.QuadPart = 0;
            if (!SetFilePointerEx(file, distance, nullptr, FILE_END))
            {
                CloseHandle(file);
                result.error = getIoErrorFromLastError();
                return result;
            }
        }
        result.result = file;
        result.error = IoError_Ok;
        return result;
    }

    // 0x00BED980 (:121)
    IoError closeFile(void* file)
    {
        if (!CloseHandle(file))
        {
            return getIoErrorFromLastError();
        }
        return IoError_Ok;
    }

    // 0x00BF4370 (:130)
    IoResult<uint32> readFile(void* file, uint64 position, void* pDest, uint32 size)
    {
        IoResult<uint32> result;
        if (pDest == nullptr)
        {
            result.result = 0;
            result.error = IoError_Ok;
            return result;
        }
        LARGE_INTEGER distance;
        distance.QuadPart = (LONGLONG)position;
        if (!SetFilePointerEx(file, distance, nullptr, FILE_BEGIN))
        {
            result.result = 0;
            result.error = IoError_InvalidPosition;
            return result;
        }
        DWORD bytesRead = 0;
        if (!ReadFile(file, pDest, size, &bytesRead, nullptr))
        {
            result.result = bytesRead;
            result.error = getIoErrorFromLastError();
            return result;
        }
        if (bytesRead == 0)
        {
            result.result = 0;
            result.error = IoError_EofReached;
            return result;
        }
        result.result = bytesRead;
        result.error = IoError_Ok;
        return result;
    }

    // 0x00BF43F0 (:159)
    IoResult<uint32> writeFile(void* file, uint64 position, const void* pSrc, uint32 size)
    {
        IoResult<uint32> result;
        LARGE_INTEGER distance;
        distance.QuadPart = (LONGLONG)position;
        if (!SetFilePointerEx(file, distance, nullptr, FILE_BEGIN))
        {
            result.result = 0;
            result.error = IoError_InvalidPosition;
            return result;
        }
        DWORD bytesWritten = 0;
        if (!WriteFile(file, pSrc, size, &bytesWritten, nullptr))
        {
            result.result = bytesWritten;
            result.error = getIoErrorFromLastError();
            return result;
        }
        result.result = bytesWritten;
        result.error = IoError_Ok;
        return result;
    }

    // 0x00BF4450 (:178)
    IoResult<uint32> appendFile(void* file, const void* pSrc, uint32 size)
    {
        IoResult<uint32> result;
        DWORD bytesWritten = 0;
        if (!WriteFile(file, pSrc, size, &bytesWritten, nullptr))
        {
            result.result = bytesWritten;
            result.error = getIoErrorFromLastError();
            return result;
        }
        result.result = bytesWritten;
        result.error = IoError_Ok;
        return result;
    }

    // 0x00BF4490 (:189)
    IoResult<uint64> getFileSize(void* file)
    {
        IoResult<uint64> result;
        LARGE_INTEGER size;
        if (!GetFileSizeEx(file, &size))
        {
            result.result = 0;
            result.error = getIoErrorFromLastError();
            return result;
        }
        result.result = (uint64)size.QuadPart;
        result.error = IoError_Ok;
        return result;
    }

    // 0x00BED9A0 (:201)
    IoError flushFileWriteBuffer(void* file)
    {
        if (!FlushFileBuffers(file))
        {
            return getIoErrorFromLastError();
        }
        return IoError_Ok;
    }

    // 0x00BF44F0 (:210)
    IoError setFilePermissionByName(const char* pFileName, bool isWriteable)
    {
        wchar_t wideFileName[520];
        if (convertUTF8ToUTF16((uint16*)wideFileName, sizeof(wideFileName), pFileName) > sizeof(wideFileName))
        {
            return IoError_InvalidArgument;
        }
        DWORD attributes = GetFileAttributesW(wideFileName);
        if (attributes != INVALID_FILE_ATTRIBUTES)
        {
            attributes = isWriteable ? (attributes & ~FILE_ATTRIBUTE_READONLY) : (attributes | FILE_ATTRIBUTE_READONLY);
            if (SetFileAttributesW(wideFileName, attributes))
            {
                return IoError_Ok;
            }
        }
        return getIoErrorFromLastError();
    }

    // 0x00C0A9D0 (:241)
    bool getCurrentExePath(FilePath* pTarget)
    {
        wchar_t wideExePath[520];
        char exePath[260];
        GetModuleFileNameW(nullptr, wideExePath, 520);
        if (GetLastError() != ERROR_SUCCESS)
        {
            return false;
        }
        if (convertUTF16ToUTF8(exePath, sizeof(exePath), (const uint16*)wideExePath) > sizeof(exePath))
        {
            return false;
        }
        pTarget->setCompletePath(exePath, false);
        return true;
    }

    // 0x00BF45B0 (:263)
    IoError getNativeFileStatusByName(FileStatus* pStatus, const char* pFileName, uint32 valueMask)
    {
        if ((valueMask & FileStatus::ValueMask_NativeFileName) != 0)
        {
            copyString(pStatus->nativeFileName, sizeof(pStatus->nativeFileName), pFileName);
        }
        if ((valueMask & (FileStatus::ValueMask_LastModificationTime | FileStatus::ValueMask_Size | FileStatus::ValueMask_Writeable | FileStatus::ValueMask_Exists | FileStatus::ValueMask_Directory)) == 0)
        {
            return IoError_Ok;
        }
        wchar_t wideFileName[520];
        if (convertUTF8ToUTF16((uint16*)wideFileName, sizeof(wideFileName), pFileName) > sizeof(wideFileName))
        {
            return IoError_InvalidArgument;
        }
        WIN32_FILE_ATTRIBUTE_DATA data;
        if (!GetFileAttributesExW(wideFileName, GetFileExInfoStandard, &data))
        {
            return getIoErrorFromLastError();
        }
        if ((valueMask & FileStatus::ValueMask_LastModificationTime) != 0)
        {
            // Seconds since 1970 (inferred from the 64-bit division in the original).
            const uint64 fileTime = ((uint64)data.ftLastWriteTime.dwHighDateTime << 32) | data.ftLastWriteTime.dwLowDateTime;
            pStatus->lastModificationTime = (int)((fileTime - 116444736000000000ull) / 10000000ull);
        }
        if ((data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) == 0)
        {
            if ((valueMask & FileStatus::ValueMask_Size) != 0)
            {
                pStatus->size = ((uint64)data.nFileSizeHigh << 32) | data.nFileSizeLow;
            }
            if ((valueMask & FileStatus::ValueMask_Writeable) != 0)
            {
                pStatus->isWriteable = (data.dwFileAttributes & FILE_ATTRIBUTE_READONLY) == 0;
            }
        }
        else
        {
            if ((valueMask & FileStatus::ValueMask_Directory) != 0)
            {
                pStatus->isDirectory = true;
            }
            if ((valueMask & FileStatus::ValueMask_Writeable) != 0)
            {
                pStatus->isWriteable = true;
            }
            if ((valueMask & FileStatus::ValueMask_Size) != 0)
            {
                pStatus->size = 0;
            }
        }
        return IoError_Ok;
    }

    // Deviation (not in rfg.exe): getKnownWindowsFolder finds SHGetKnownFolderPath through GetModuleHandleW, so
    // shell32.dll must already be loaded. rfg.exe imports it (ShellExecuteW for startDefaultWebBrowser,
    // SHGetSpecialFolderPathW for pc_cfile), the port has neither yet: without this import every lookup failed and the
    // options ini (%APPDATA%/kaiko/rfg/config.ini) was never read.
#pragma comment(lib, "shell32.lib")
#pragma comment(linker, "/include:__imp__SHGetSpecialFolderPathW@16")

    // 0x00C0AA70 (:344)
    bool getKnownWindowsFolder(char* pTarget, uint32 capacity, KnownWindowsFolderId id)
    {
        typedef HRESULT(WINAPI * GetKnownFolderPathFunction)(const GUID&, DWORD, HANDLE, PWSTR*);
        static const GUID s_folderIds[KnownWindowsFolderId_Count] =
        {
            { 0x18989B1D, 0x99B5, 0x455B, { 0x84, 0x1C, 0xAB, 0x7C, 0x74, 0xE4, 0xDD, 0xFC } }, // FOLDERID_Videos
            { 0x33E28130, 0x4E1E, 0x4676, { 0x83, 0x5A, 0x98, 0x39, 0x5C, 0x3B, 0xC3, 0xBB } }, // FOLDERID_Pictures
            { 0x3EB685DB, 0x65F9, 0x4CF6, { 0xA0, 0x3A, 0xE3, 0xEF, 0x65, 0x72, 0x9F, 0x3D } }, // FOLDERID_RoamingAppData
            { 0xFDD39AD0, 0x238F, 0x46AF, { 0xAD, 0xB4, 0x6C, 0x85, 0x48, 0x03, 0x69, 0xC7 } }, // FOLDERID_Documents
        };
        CoInitializeEx(nullptr, COINIT_MULTITHREADED);
        GetKnownFolderPathFunction pGetKnownFolderPath = (GetKnownFolderPathFunction)GetProcAddress(GetModuleHandleW(L"shell32.dll"), "SHGetKnownFolderPath");
        if (pGetKnownFolderPath != nullptr)
        {
            PWSTR pPath = nullptr;
            if (SUCCEEDED(pGetKnownFolderPath(s_folderIds[id], KF_FLAG_CREATE | KF_FLAG_NO_ALIAS, nullptr, &pPath)))
            {
                convertUTF16ToUTF8(pTarget, capacity, (const uint16*)pPath);
                CoTaskMemFree(pPath);
                CoUninitialize();
                return true;
            }
        }
        CoUninitialize();
        return false;
    }

    // 0x00C0ABB0 (:379)
    IoError createNativeDirectoryPath(const char* pPathName)
    {
        // Creates the directory, and its parents when the direct attempt reports a missing path.
        FilePath path;
        path.setCompletePath(pPathName, true);
        if (!path.isAbsolute())
        {
            return IoError_InvalidArgument;
        }
        wchar_t widePath[520];
        const char* pCompletePath = path.getCompletePath();
        if (convertUTF8ToUTF16((uint16*)widePath, sizeof(widePath), pCompletePath) > sizeof(widePath))
        {
            return IoError_InvalidArgument;
        }
        if (CreateDirectoryW(widePath, nullptr))
        {
            return IoError_Ok;
        }
        if (GetLastError() != ERROR_PATH_NOT_FOUND)
        {
            return getIoErrorFromLastError();
        }

        const char* pSeparator = findFirstCharacterInString(pCompletePath + getStringLength(path.getPrefix()) + 1, '/');
        while (pSeparator != nullptr)
        {
            char partialPath[260];
            copyString(partialPath, pCompletePath, pSeparator, 0x104);
            if (convertUTF8ToUTF16((uint16*)widePath, sizeof(widePath), partialPath) > sizeof(widePath))
            {
                return IoError_InvalidArgument;
            }
            if (!CreateDirectoryW(widePath, nullptr) && GetLastError() != ERROR_ALREADY_EXISTS)
            {
                return getIoErrorFromLastError();
            }
            pSeparator = findFirstCharacterInString(pSeparator + 1, '/');
        }
        return IoError_Ok;
    }

    // 0x00BED9C0 (:436)
    sint64 getFileTime(const char* pFilename)
    {
        struct _stat64 status;
        _stat64(pFilename, &status);
        return status.st_mtime;
    }

    // 0x00C0ADE0 (:445)
    bool getTempFileName(char* pBuffer, uint32 bufferCapacity, const char* pPrefixString)
    {
        wchar_t tempPath[522];
        wchar_t prefix[128];
        wchar_t tempFileName[262];
        if (GetTempPathW(521, tempPath) == 0)
        {
            return false;
        }
        const wchar_t* pPrefix = L"keen";
        if (pPrefixString != nullptr)
        {
            convertUTF8ToUTF16((uint16*)prefix, sizeof(prefix), pPrefixString);
            pPrefix = prefix;
        }
        if (GetTempFileNameW(tempPath, pPrefix, 0, tempFileName) == 0)
        {
            return false;
        }
        convertUTF16ToUTF8(pBuffer, bufferCapacity, (const uint16*)tempFileName);
        return true;
    }

    // 0x00BF4700 (:471)
    void deleteFile(const char* pFilename)
    {
        wchar_t wideFileName[520];
        if (convertUTF8ToUTF16((uint16*)wideFileName, sizeof(wideFileName), pFilename) <= sizeof(wideFileName))
        {
            DeleteFileW(wideFileName);
        }
    }

    // 0x00C07440 (:481), LTCG (pPath in ESI)
    bool checkProjectGamebuildPath(FilePath* pPath)
    {
        pPath->setFileNameWithExtension("build.sqlite3");
        wchar_t widePath[520];
        if (convertUTF8ToUTF16((uint16*)widePath, sizeof(widePath), pPath->getCompletePath()) <= sizeof(widePath) && PathFileExistsW(widePath))
        {
            pPath->setFileNameWithExtension("");
            return true;
        }
        return false;
    }

    // 0x00C0AEB0 (:504)
    bool getCurrentProjectGamebuildPath(FilePath* pTarget, bool checkExecutablePath)
    {
        // Development helper: looks for build.sqlite3 next to the executable or in the project configured in
        // c:\projekte\config\projects.ini.
        if (pTarget == nullptr)
        {
            return false;
        }
        wchar_t project[520];
        wchar_t platform[520];
        char projectName[260];
        char platformName[260];
        if (checkExecutablePath)
        {
            GetModuleFileNameW(nullptr, project, 520);
            if (convertUTF16ToUTF8(projectName, sizeof(projectName), (const uint16*)project) > sizeof(projectName))
            {
                return false;
            }
            pTarget->setCompletePath(projectName, false);
            pTarget->setFileNameWithExtension("build.sqlite3");
            if (checkProjectGamebuildPath(pTarget))
            {
                return true;
            }
        }
        if (GetPrivateProfileStringW(L"Global", L"CurrentProject", nullptr, project, 520, L"c:\\projekte\\config\\projects.ini") == 0 ||
            GetPrivateProfileStringW(project, L"CurrentPlatform", nullptr, platform, 520, L"c:\\projekte\\config\\projects.ini") == 0 ||
            convertUTF16ToUTF8(projectName, sizeof(projectName), (const uint16*)project) > sizeof(projectName) ||
            convertUTF16ToUTF8(platformName, sizeof(platformName), (const uint16*)platform) > sizeof(platformName))
        {
            return false;
        }
        pTarget->setCompletePath("c:\\projekte", true);
        pTarget->pushDirectory(projectName);
        pTarget->pushDirectory("gamebuild");
        pTarget->pushDirectory(platformName);
        return checkProjectGamebuildPath(pTarget);
    }
}
