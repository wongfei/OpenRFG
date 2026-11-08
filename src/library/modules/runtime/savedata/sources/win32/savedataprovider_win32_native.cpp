// New code, not recovered: see savedataprovider_win32_native.hpp. The operations mirror SaveDataProviderWin32Steam
// (savedataprovider_win32_steam.cpp) so that SaveDataHandler's flow is the one rfg.exe runs on Steam. Everything runs in
// the caller's update: the Steam provider's FileWrite blocks there too, and a read of this size from a local disk is
// comparable to one write.

#include "library/modules/runtime/savedata/sources/win32/savedataprovider_win32_native.hpp"
#include "keen/base/filepath.hpp"
#include "keen/base/memoryallocator.hpp"
#include "keen/base/nativefile.hpp"
#include "keen/base/string.hpp"
#include "keen/base/trace.hpp"
#include "keen/useraccount/useraccounttypes.hpp"

#include <windows.h>

#include <stdio.h>
#include <string.h>

namespace keen
{
    namespace SaveData
    {
        // Magic number: 0xFFFF, no connected profile (as in the Steam provider).
        static const uint32 NoConnectedProfile = 0xFFFFu;

        SaveDataProviderWin32Native::SaveDataProviderWin32Native()
        {
            m_isOperationFinished = false;
            m_operationResult = OperationResult_Ok;
            m_pAllocator = nullptr;
            m_pFormats = nullptr;
            m_formatCount = 0;
            m_connectedProfileIndex = NoConnectedProfile;
            m_directory[0] = '\0';
        }

        SaveDataProviderWin32Native::~SaveDataProviderWin32Native()
        {
        }

        // The folder is the --save-dir option's, else "<roaming app data>/OpenRFG/save".
        bool SaveDataProviderWin32Native::create(MemoryAllocator* pAllocator, const SaveDataSystemCreationParameters& parameters)
        {
            m_pAllocator = pAllocator;
            m_pFormats = parameters.pSaveDataFormats;
            m_formatCount = parameters.saveDataFormatCount;
            const SaveDataSystemCreationProviderParametersWin32* pProviderParameters = (const SaveDataSystemCreationProviderParametersWin32*)parameters.pProviderParameters;
            if (pProviderParameters != nullptr && pProviderParameters->pSaveDirectory != nullptr)
            {
                copyString(m_directory, sizeof(m_directory), pProviderParameters->pSaveDirectory);
            }
            else
            {
                char folder[MaxPathLength];
                if (!getKnownWindowsFolder(folder, sizeof(folder), KnownWindowsFolderId_RoamingAppData))
                {
                    debug::nativePrintArgs("[savedata] The application data folder was not found: saving is off.\n");
                    return false;
                }
                FilePath path(folder, true);
                path.pushDirectory("OpenRFG");
                path.pushDirectory("save");
                copyString(m_directory, sizeof(m_directory), path.getCompletePath());
            }
            const IoError error = createNativeDirectoryPath(m_directory);
            if (error != IoError_Ok && error != IoError_AlreadyExists)
            {
                debug::nativePrintArgs("[savedata] Could not create the save folder '%s'.\n", m_directory);
                m_directory[0] = '\0';
                return false;
            }
            debug::nativePrintArgs("[savedata] Local saves in '%s'.\n", m_directory);
            return true;
        }

        void SaveDataProviderWin32Native::destroy(MemoryAllocator*)
        {
            m_pAllocator = nullptr;
            m_pFormats = nullptr;
            m_formatCount = 0;
        }

        bool SaveDataProviderWin32Native::enumerateProfiles(SizedArray<Profile>* pResult)
        {
            Profile profile;
            profile.isUsed = true;
            copyString(profile.name, sizeof(profile.name), "Local");
            pResult->clear();
            pResult->pushBack(profile);
            return true;
        }

        bool SaveDataProviderWin32Native::enumerateSaveData(SizedArray<SaveData>* pResult, uint32, uint32 formatIndex)
        {
            OperationResult result;
            return enumerateSaveData(&result, pResult, formatIndex);
        }

        // The save data files of the format with the format's size. As in the Steam provider a file of another size makes
        // the result Corrupted unless a good one was found.
        bool SaveDataProviderWin32Native::enumerateSaveData(OperationResult* pResultCode, SizedArray<SaveData>* pResult, uint32 formatIndex) const
        {
            pResult->clear();
            *pResultCode = OperationResult_Missing;
            if (m_directory[0] == '\0')
            {
                return true;
            }
            const uint32 expectedSize = getFormatSize(formatIndex);
            for (uint32 id = 0; id < m_pFormats[formatIndex].maximumInstallments && pResult->getSize() != pResult->getCapacity(); ++id)
            {
                SaveData saveData;
                saveData.formatIndex = formatIndex;
                saveData.id = id;
                char path[MaxPathLength];
                generateFilePath(path, sizeof(path), saveData);
                WIN32_FILE_ATTRIBUTE_DATA attributes;
                if (!GetFileAttributesExA(path, GetFileExInfoStandard, &attributes))
                {
                    continue;
                }
                if (attributes.nFileSizeHigh == 0 && attributes.nFileSizeLow == expectedSize)
                {
                    pResult->pushBack(saveData);
                    *pResultCode = OperationResult_Ok;
                }
                else
                {
                    debug::nativePrintArgs("[savedata] The file '%s' does not have the %u bytes of its format.\n", path, expectedSize);
                    if (*pResultCode != OperationResult_Ok)
                    {
                        *pResultCode = OperationResult_Corrupted;
                    }
                }
            }
            return true;
        }

        bool SaveDataProviderWin32Native::connectProfileWithUser(uint32 profileIndex, UserAccountId)
        {
            if (m_connectedProfileIndex != NoConnectedProfile)
            {
                debug::nativePrintArgs("[savedata] Can't connect a second user.\n");
                return false;
            }
            m_connectedProfileIndex = profileIndex;
            return true;
        }

        bool SaveDataProviderWin32Native::disconnectProfile(uint32 profileIndex)
        {
            if (m_connectedProfileIndex != profileIndex)
            {
                debug::nativePrintArgs("[savedata] Profile with index %u is not connected.\n", profileIndex);
                return false;
            }
            m_connectedProfileIndex = NoConnectedProfile;
            return true;
        }

        bool SaveDataProviderWin32Native::isProfileConnected(uint32 profileIndex) const
        {
            return m_connectedProfileIndex == profileIndex;
        }

        void SaveDataProviderWin32Native::updateCreateProfile(uint32, const Profile&)
        {
            setOperationFinished(OperationResult_Error);
        }

        void SaveDataProviderWin32Native::updateUpdateProfile(uint32, const Profile&)
        {
            setOperationFinished(OperationResult_Error);
        }

        void SaveDataProviderWin32Native::updateEraseProfile(uint32)
        {
            setOperationFinished(OperationResult_Error);
        }

        void SaveDataProviderWin32Native::updateConnectProfile(uint32, const Profile&)
        {
        }

        void SaveDataProviderWin32Native::updateDisconnectProfile(uint32, const Profile&)
        {
        }

        // Every format has at least its minimum installments of good files.
        void SaveDataProviderWin32Native::updateCheckSaveDataSet(uint32)
        {
            m_isOperationFinished = false;
            for (uint32 formatIndex = 0; formatIndex < m_formatCount; ++formatIndex)
            {
                SizedArray<SaveData> saveDatas;
                // Magic number: 16-byte aligned array, as in the Steam provider.
                saveDatas.create(m_pAllocator, m_pFormats[formatIndex].maximumInstallments, 0x10u);
                OperationResult result;
                enumerateSaveData(&result, &saveDatas, formatIndex);
                const uint32 count = saveDatas.getSize();
                saveDatas.destroy(m_pAllocator);
                if (result != OperationResult_Ok || count < m_pFormats[formatIndex].minimumInstallments)
                {
                    setOperationFinished(result != OperationResult_Ok ? result : OperationResult_Corrupted);
                    return;
                }
            }
            setOperationFinished(OperationResult_Ok);
        }

        // The set is the files themselves.
        void SaveDataProviderWin32Native::updateCreateSaveDataSet(uint32)
        {
            setOperationFinished(m_directory[0] != '\0' ? OperationResult_Ok : OperationResult_Error);
        }

        // Every installment file of every format is deleted. The ".bak" files stay: an erase is only reached through the
        // corrupt save data box, and the backup is then the way back.
        void SaveDataProviderWin32Native::updateEraseSaveDataSet(uint32)
        {
            m_isOperationFinished = false;
            for (uint32 formatIndex = 0; formatIndex < m_formatCount; ++formatIndex)
            {
                for (uint32 id = 0; id < m_pFormats[formatIndex].maximumInstallments; ++id)
                {
                    SaveData saveData;
                    saveData.formatIndex = formatIndex;
                    saveData.id = id;
                    char path[MaxPathLength];
                    generateFilePath(path, sizeof(path), saveData);
                    if (!DeleteFileA(path) && GetLastError() != ERROR_FILE_NOT_FOUND)
                    {
                        setOperationFinished(OperationResult_Error);
                        return;
                    }
                }
            }
            setOperationFinished(OperationResult_Ok);
        }

        // A new installment at the first unused id.
        void SaveDataProviderWin32Native::updateCreateSaveData(SaveData* pSaveData, uint32, const StorageOperation& operation)
        {
            m_isOperationFinished = false;
            const uint32 maximum = m_pFormats[pSaveData->formatIndex].maximumInstallments;
            for (uint32 id = 0; id < maximum; ++id)
            {
                pSaveData->id = id;
                char path[MaxPathLength];
                generateFilePath(path, sizeof(path), *pSaveData);
                if (GetFileAttributesA(path) == INVALID_FILE_ATTRIBUTES)
                {
                    setOperationFinished(writeSaveData(*pSaveData, operation));
                    return;
                }
            }
            // Magic number: no id, as in the Steam provider.
            pSaveData->id = 0xFFFFFFFFu;
            setOperationFinished(OperationResult_MaxInstallmentsReached);
        }

        void SaveDataProviderWin32Native::updateWriteSaveData(uint32, const SaveData& saveData, const StorageOperation& operation)
        {
            m_isOperationFinished = false;
            setOperationFinished(writeSaveData(saveData, operation));
        }

        // Each segment of the operation directly into its buffer.
        void SaveDataProviderWin32Native::updateReadSaveData(StorageOperation* pOperation, uint32, const SaveData& saveData)
        {
            m_isOperationFinished = false;
            char path[MaxPathLength];
            generateFilePath(path, sizeof(path), saveData);
            HANDLE file = CreateFileA(path, GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, FILE_FLAG_SEQUENTIAL_SCAN, nullptr);
            if (file == INVALID_HANDLE_VALUE)
            {
                debug::nativePrintArgs("[savedata] Save data file '%s' doesn't exist.\n", path);
                setOperationFinished(OperationResult_Missing);
                return;
            }
            OperationResult result = OperationResult_Ok;
            for (; pOperation != nullptr; pOperation = pOperation->pNext)
            {
                const uint32 size = m_pFormats[saveData.formatIndex].pSegmentSizes[pOperation->segmentIndex];
                DWORD bytesRead = 0;
                if (SetFilePointer(file, (LONG)getSegmentOffset(saveData.formatIndex, pOperation->segmentIndex), nullptr, FILE_BEGIN) == INVALID_SET_FILE_POINTER ||
                    !ReadFile(file, pOperation->pBuffer, size, &bytesRead, nullptr) || bytesRead != size)
                {
                    debug::nativePrintArgs("[savedata] %u of %u bytes read from file '%s'.\n", (uint32)bytesRead, size, path);
                    result = OperationResult_Corrupted;
                    break;
                }
            }
            CloseHandle(file);
            setOperationFinished(result);
        }

        void SaveDataProviderWin32Native::updateEraseSaveData(uint32, const SaveData& saveData)
        {
            char path[MaxPathLength];
            generateFilePath(path, sizeof(path), saveData);
            setOperationFinished(DeleteFileA(path) || GetLastError() == ERROR_FILE_NOT_FOUND ? OperationResult_Ok : OperationResult_Error);
        }

        bool SaveDataProviderWin32Native::hasFinishedOperation()
        {
            return m_isOperationFinished;
        }

        OperationResult SaveDataProviderWin32Native::getOperationResult()
        {
            return m_operationResult;
        }

        uint32 SaveDataProviderWin32Native::getSegmentCount(uint32 formatIndex) const
        {
            return m_pFormats[formatIndex].numSegments;
        }

        void SaveDataProviderWin32Native::setOperationFinished(OperationResult result)
        {
            m_operationResult = result;
            m_isOperationFinished = true;
        }

        // The Steam provider's file name, in the local folder.
        void SaveDataProviderWin32Native::generateFilePath(char* pPath, uint32 capacity, const SaveData& saveData) const
        {
            formatString(pPath, capacity, "%s/keen_savegame_%u_%u.sav", m_directory, saveData.formatIndex, saveData.id);
        }

        uint32 SaveDataProviderWin32Native::getFormatSize(uint32 formatIndex) const
        {
            return getSegmentOffset(formatIndex, m_pFormats[formatIndex].numSegments);
        }

        uint32 SaveDataProviderWin32Native::getSegmentOffset(uint32 formatIndex, uint32 segmentIndex) const
        {
            uint32 offset = 0;
            for (uint32 i = 0; i < segmentIndex; ++i)
            {
                offset += m_pFormats[formatIndex].pSegmentSizes[i];
            }
            return offset;
        }

        // The operation's segments into a temporary file (named after the process: two OpenRFG can share the folder),
        // flushed, then swapped in. Two processes writing one folder keep whichever finished last, but never a torn file.
        OperationResult SaveDataProviderWin32Native::writeSaveData(const SaveData& saveData, const StorageOperation& operation)
        {
            if (m_directory[0] == '\0')
            {
                return OperationResult_Error;
            }
            char path[MaxPathLength];
            generateFilePath(path, sizeof(path), saveData);
            char temporaryPath[MaxPathLength + 32];
            char backupPath[MaxPathLength + 32];
            formatString(temporaryPath, sizeof(temporaryPath), "%s.%u.tmp", path, (uint32)GetCurrentProcessId());
            // Deviation from rfg.exe: it only has Steam remote storage. This local provider writes through a temporary
            // file and keeps the previous file as .bak.
            formatString(backupPath, sizeof(backupPath), "%s.bak", path);
            HANDLE file = CreateFileA(temporaryPath, GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL | FILE_FLAG_SEQUENTIAL_SCAN, nullptr);
            if (file == INVALID_HANDLE_VALUE)
            {
                debug::nativePrintArgs("[savedata] Could not create '%s' (error %u).\n", temporaryPath, (uint32)GetLastError());
                return OperationResult_Error;
            }
            OperationResult result = OperationResult_Ok;
            for (const StorageOperation* pOperation = &operation; pOperation != nullptr; pOperation = pOperation->pNext)
            {
                const uint32 size = m_pFormats[saveData.formatIndex].pSegmentSizes[pOperation->segmentIndex];
                DWORD bytesWritten = 0;
                if (SetFilePointer(file, (LONG)getSegmentOffset(saveData.formatIndex, pOperation->segmentIndex), nullptr, FILE_BEGIN) == INVALID_SET_FILE_POINTER ||
                    !WriteFile(file, pOperation->pBuffer, size, &bytesWritten, nullptr) || bytesWritten != size)
                {
                    const DWORD error = GetLastError();
                    debug::nativePrintArgs("[savedata] Writing %u bytes to file '%s' failed (error %u).\n", size, temporaryPath, (uint32)error);
                    result = error == ERROR_DISK_FULL || error == ERROR_HANDLE_DISK_FULL ? OperationResult_InsufficientSpace : OperationResult_Error;
                    break;
                }
            }
            if (result == OperationResult_Ok && !FlushFileBuffers(file))
            {
                result = OperationResult_Error;
            }
            CloseHandle(file);
            if (result == OperationResult_Ok)
            {
                const bool exists = GetFileAttributesA(path) != INVALID_FILE_ATTRIBUTES;
                const BOOL replaced = exists ? ReplaceFileA(path, temporaryPath, backupPath, REPLACEFILE_IGNORE_MERGE_ERRORS, nullptr, nullptr)
                                             : MoveFileExA(temporaryPath, path, MOVEFILE_WRITE_THROUGH);
                if (!replaced)
                {
                    debug::nativePrintArgs("[savedata] Could not replace '%s' (error %u).\n", path, (uint32)GetLastError());
                    result = OperationResult_Error;
                }
            }
            if (result != OperationResult_Ok)
            {
                DeleteFileA(temporaryPath);
            }
            return result;
        }
    }
}
