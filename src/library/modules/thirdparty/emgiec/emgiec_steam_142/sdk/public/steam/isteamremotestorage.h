// Original: library/modules/thirdparty/emgiec/emgiec_steam_142/sdk/public/steam/isteamremotestorage.h
//
// Steamworks SDK 1.42, third party (stub_impl.md S2): ISteamRemoteStorage (STEAMREMOTESTORAGE_INTERFACE_VERSION014),
// the Steam cloud the save data provider writes to, from the PDB's vtable.
#pragma once

#include "library/modules/thirdparty/emgiec/emgiec_steam_142/sdk/public/steam/steamclientpublic.h"
#include "library/modules/thirdparty/emgiec/emgiec_steam_142/sdk/public/steam/steamtypes.h"

enum ERemoteStoragePlatform : int;
enum EUGCReadAction : int;
enum ERemoteStoragePublishedFileVisibility : int;
enum EWorkshopFileType : int;
enum EWorkshopVideoProvider : int;
enum EWorkshopFileAction : int;
enum EWorkshopEnumerationType : int;

// size 0x8
struct SteamParamStringArray_t
{
    const char** m_ppStrings; // 0x00
    int m_nNumStrings; // 0x04
};

// size 0x4
class ISteamRemoteStorage
{
public:
    virtual bool FileWrite(const char* pchFile, const void* pvData, int cubData) = 0; // vtbl+0x00
    virtual int FileRead(const char* pchFile, void* pvData, int cubDataToRead) = 0; // vtbl+0x04
    virtual SteamAPICall_t FileWriteAsync(const char* pchFile, const void* pvData, uint32 cubData) = 0; // vtbl+0x08
    virtual SteamAPICall_t FileReadAsync(const char* pchFile, uint32 nOffset, uint32 cubToRead) = 0; // vtbl+0x0C
    virtual bool FileReadAsyncComplete(SteamAPICall_t hReadCall, void* pvBuffer, uint32 cubToRead) = 0; // vtbl+0x10
    virtual bool FileForget(const char* pchFile) = 0; // vtbl+0x14
    virtual bool FileDelete(const char* pchFile) = 0; // vtbl+0x18
    virtual SteamAPICall_t FileShare(const char* pchFile) = 0; // vtbl+0x1C
    virtual bool SetSyncPlatforms(const char* pchFile, ERemoteStoragePlatform eRemoteStoragePlatform) = 0; // vtbl+0x20
    virtual uint64 FileWriteStreamOpen(const char* pchFile) = 0; // vtbl+0x24
    virtual bool FileWriteStreamWriteChunk(uint64 writeHandle, const void* pvData, int cubData) = 0; // vtbl+0x28
    virtual bool FileWriteStreamClose(uint64 writeHandle) = 0; // vtbl+0x2C
    virtual bool FileWriteStreamCancel(uint64 writeHandle) = 0; // vtbl+0x30
    virtual bool FileExists(const char* pchFile) = 0; // vtbl+0x34
    virtual bool FilePersisted(const char* pchFile) = 0; // vtbl+0x38
    virtual int GetFileSize(const char* pchFile) = 0; // vtbl+0x3C
    virtual sint64 GetFileTimestamp(const char* pchFile) = 0; // vtbl+0x40
    virtual ERemoteStoragePlatform GetSyncPlatforms(const char* pchFile) = 0; // vtbl+0x44
    virtual int GetFileCount() = 0; // vtbl+0x48
    virtual const char* GetFileNameAndSize(int iFile, int* pnFileSizeInBytes) = 0; // vtbl+0x4C
    virtual bool GetQuota(uint64* pnTotalBytes, uint64* puAvailableBytes) = 0; // vtbl+0x50
    virtual bool IsCloudEnabledForAccount() = 0; // vtbl+0x54
    virtual bool IsCloudEnabledForApp() = 0; // vtbl+0x58
    virtual void SetCloudEnabledForApp(bool bEnabled) = 0; // vtbl+0x5C
    virtual SteamAPICall_t UGCDownload(UGCHandle_t hContent, uint32 unPriority) = 0; // vtbl+0x60
    virtual bool GetUGCDownloadProgress(UGCHandle_t hContent, int* pnBytesDownloaded, int* pnBytesExpected) = 0; // vtbl+0x64
    virtual bool GetUGCDetails(UGCHandle_t hContent, AppId_t* pnAppID, char** ppchName, int* pnFileSizeInBytes, CSteamID* pSteamIDOwner) = 0; // vtbl+0x68
    virtual int UGCRead(UGCHandle_t hContent, void* pvData, int cubDataToRead, uint32 cOffset, EUGCReadAction eAction) = 0; // vtbl+0x6C
    virtual int GetCachedUGCCount() = 0; // vtbl+0x70
    virtual UGCHandle_t GetCachedUGCHandle(int iCachedContent) = 0; // vtbl+0x74
    virtual SteamAPICall_t PublishWorkshopFile(const char* pchFile, const char* pchPreviewFile, AppId_t nConsumerAppId, const char* pchTitle, const char* pchDescription, ERemoteStoragePublishedFileVisibility eVisibility, SteamParamStringArray_t* pTags, EWorkshopFileType eWorkshopFileType) = 0; // vtbl+0x78
    virtual uint64 CreatePublishedFileUpdateRequest(uint64 unPublishedFileId) = 0; // vtbl+0x7C
    virtual bool UpdatePublishedFileFile(uint64 updateHandle, const char* pchFile) = 0; // vtbl+0x80
    virtual bool UpdatePublishedFilePreviewFile(uint64 updateHandle, const char* pchPreviewFile) = 0; // vtbl+0x84
    virtual bool UpdatePublishedFileTitle(uint64 updateHandle, const char* pchTitle) = 0; // vtbl+0x88
    virtual bool UpdatePublishedFileDescription(uint64 updateHandle, const char* pchDescription) = 0; // vtbl+0x8C
    virtual bool UpdatePublishedFileVisibility(uint64 updateHandle, ERemoteStoragePublishedFileVisibility eVisibility) = 0; // vtbl+0x90
    virtual bool UpdatePublishedFileTags(uint64 updateHandle, SteamParamStringArray_t* pTags) = 0; // vtbl+0x94
    virtual SteamAPICall_t CommitPublishedFileUpdate(uint64 updateHandle) = 0; // vtbl+0x98
    virtual SteamAPICall_t GetPublishedFileDetails(uint64 unPublishedFileId, uint32 unMaxSecondsOld) = 0; // vtbl+0x9C
    virtual SteamAPICall_t DeletePublishedFile(uint64 unPublishedFileId) = 0; // vtbl+0xA0
    virtual SteamAPICall_t EnumerateUserPublishedFiles(uint32 unStartIndex) = 0; // vtbl+0xA4
    virtual SteamAPICall_t SubscribePublishedFile(uint64 unPublishedFileId) = 0; // vtbl+0xA8
    virtual SteamAPICall_t EnumerateUserSubscribedFiles(uint32 unStartIndex) = 0; // vtbl+0xAC
    virtual SteamAPICall_t UnsubscribePublishedFile(uint64 unPublishedFileId) = 0; // vtbl+0xB0
    virtual bool UpdatePublishedFileSetChangeDescription(uint64 updateHandle, const char* pchChangeDescription) = 0; // vtbl+0xB4
    virtual SteamAPICall_t GetPublishedItemVoteDetails(uint64 unPublishedFileId) = 0; // vtbl+0xB8
    virtual SteamAPICall_t UpdateUserPublishedItemVote(uint64 unPublishedFileId, bool bVoteUp) = 0; // vtbl+0xBC
    virtual SteamAPICall_t GetUserPublishedItemVoteDetails(uint64 unPublishedFileId) = 0; // vtbl+0xC0
    virtual SteamAPICall_t EnumerateUserSharedWorkshopFiles(CSteamID steamId, uint32 unStartIndex, SteamParamStringArray_t* pRequiredTags, SteamParamStringArray_t* pExcludedTags) = 0; // vtbl+0xC4
    virtual SteamAPICall_t PublishVideo(EWorkshopVideoProvider eVideoProvider, const char* pchVideoAccount, const char* pchVideoIdentifier, const char* pchPreviewFile, AppId_t nConsumerAppId, const char* pchTitle, const char* pchDescription, ERemoteStoragePublishedFileVisibility eVisibility, SteamParamStringArray_t* pTags) = 0; // vtbl+0xC8
    virtual SteamAPICall_t SetUserPublishedFileAction(uint64 unPublishedFileId, EWorkshopFileAction eAction) = 0; // vtbl+0xCC
    virtual SteamAPICall_t EnumeratePublishedFilesByUserAction(EWorkshopFileAction eAction, uint32 unStartIndex) = 0; // vtbl+0xD0
    virtual SteamAPICall_t EnumeratePublishedWorkshopFiles(EWorkshopEnumerationType eEnumerationType, uint32 unStartIndex, uint32 unCount, uint32 unDays, SteamParamStringArray_t* pTags, SteamParamStringArray_t* pUserTags) = 0; // vtbl+0xD4
    virtual SteamAPICall_t UGCDownloadToLocation(UGCHandle_t hContent, const char* pchLocation, uint32 unPriority) = 0; // vtbl+0xD8

    // 0x00 vfptr
};
