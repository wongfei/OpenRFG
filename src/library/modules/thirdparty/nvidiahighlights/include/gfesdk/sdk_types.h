// Original: library/modules/thirdparty/nvidiahighlights/include/gfesdk/sdk_types.h (placement inferred)
//
// NVIDIA GeForce Experience SDK (GfeSDK.dll), third party (stub_impl.md S2): the core types rfg.exe uses, as the PDB
// records them (pdbdump header NVGSDK_*). The functions are in isdk.h and highlights/ihighlights.h.
#pragma once

#include <stdint.h>

extern "C"
{
    typedef enum _NVGSDK_RetCode
    {
        NVGSDK_SUCCESS = 0,
        NVGSDK_SUCCESS_VERSION_OLD_SDK = 1001,
        NVGSDK_SUCCESS_VERSION_OLD_GFE = 1002,
        NVGSDK_SUCCESS_PENDING = 1003,
        NVGSDK_SUCCESS_USER_NOT_INTERESTED = 1004,
        NVGSDK_SUCCESS_PERMISSION_GRANTED = 1005,
        NVGSDK_SUCCESS_LINKED = 1006,
        NVGSDK_ERR_GENERIC = -1001,
        NVGSDK_ERR_GFE_VERSION = -1002,
        NVGSDK_ERR_SDK_VERSION = -1003,
        NVGSDK_ERR_NOT_IMPLEMENTED = -1004,
        NVGSDK_ERR_INVALID_PARAMETER = -1005,
        NVGSDK_ERR_NOT_SET = -1006,
        NVGSDK_ERR_SHADOWPLAY_IR_DISABLED = -1007,
        NVGSDK_ERR_SDK_IN_USE = -1008,
        NVGSDK_ERR_GROUP_NOT_FOUND = -1009,
        NVGSDK_ERR_FILE_NOT_FOUND = -1010,
        NVGSDK_ERR_HIGHLIGHTS_SETUP_FAILED = -1011,
        NVGSDK_ERR_HIGHLIGHTS_NOT_CONFIGURED = -1012,
        NVGSDK_ERR_HIGHLIGHTS_SAVE_FAILED = -1013,
        NVGSDK_ERR_UNEXPECTED_EXCEPTION = -1014,
        NVGSDK_ERR_NO_HIGHLIGHTS = -1015,
        NVGSDK_ERR_NO_CONNECTION = -1016,
        NVGSDK_ERR_PERMISSION_NOT_GRANTED = -1017,
        NVGSDK_ERR_PERMISSION_DENIED = -1018,
        NVGSDK_ERR_INVALID_HANDLE = -1019,
        NVGSDK_ERR_UNHANDLED_EXCEPTION = -1020,
        NVGSDK_ERR_OUT_OF_MEMORY = -1021,
        NVGSDK_ERR_LOAD_LIBRARY = -1022,
        NVGSDK_ERR_LIB_CALL_FAILED = -1023,
        NVGSDK_ERR_IPC_FAILED = -1024,
        NVGSDK_ERR_CONNECTION = -1025,
        NVGSDK_ERR_MODULE_NOT_LOADED = -1026,
        NVGSDK_ERR_LIB_CALL_TIMEOUT = -1027,
        NVGSDK_ERR_APPLICATION_LOOKUP_FAILED = -1028,
        NVGSDK_ERR_APPLICATION_NOT_KNOWN = -1029,
        NVGSDK_ERR_FEATURE_DISABLED = -1030,
        NVGSDK_ERR_APP_NO_OPTIMIZATION = -1031,
        NVGSDK_ERR_APP_SETTINGS_READ = -1032,
        NVGSDK_ERR_APP_SETTINGS_WRITE = -1033,
    } NVGSDK_RetCode;

    // The SDK instance (opaque).
    struct NVGSDK_HANDLE;

    typedef enum NVGSDK_Scope
    {
        NVGSDK_SCOPE_HIGHLIGHTS = 0,
        NVGSDK_SCOPE_HIGHLIGHTS_VIDEO = 1,
        NVGSDK_SCOPE_HIGHLIGHTS_SCREENSHOT = 2,
        NVGSDK_SCOPE_OPS = 3,
        NVGSDK_SCOPE_MAX = 4,
    } NVGSDK_Scope;

    typedef enum NVGSDK_Permission
    {
        NVGSDK_PERMISSION_GRANTED = 0,
        NVGSDK_PERMISSION_MUST_ASK = 1,
        NVGSDK_PERMISSION_DENIED = 2,
        NVGSDK_PERMISSION_UNKNOWN = 3,
        NVGSDK_PERMISSION_MAX = 4,
    } NVGSDK_Permission;

    typedef enum NVGSDK_NotificationType
    {
        NVGSDK_NOTIFICATION_PERMISSIONS_CHANGED = 0,
        NVGSDK_NOTIFICATION_OVERLAY_STATE_CHANGED = 1,
        NVGSDK_NOTIFICATION_MAX = 2,
    } NVGSDK_NotificationType;

    typedef enum NVGSDK_OverlayState
    {
        NVGSDK_OVERLAY_STATE_MAIN = 0,
        NVGSDK_OVERLAY_STATE_PERMISSION = 1,
        NVGSDK_OVERLAY_STATE_HIGHLIGHTS_SUMMARY = 2,
        NVGSDK_OVERLAY_STATE_MAX = 3,
    } NVGSDK_OverlayState;

    // size 0x8
    typedef struct NVGSDK_ScopePermission
    {
        NVGSDK_Scope scope; // 0x00
        NVGSDK_Permission permission; // 0x04
    } NVGSDK_ScopePermission;

    // size 0x8
    typedef struct NVGSDK_PermissionsChangedNotification
    {
        NVGSDK_ScopePermission* scopePermissionTable; // 0x00
        uint32_t scopePermissionTableSize; // 0x04
    } NVGSDK_PermissionsChangedNotification;

    // size 0x8
    typedef struct NVGSDK_OverlayStateChangedNotification
    {
        bool open; // 0x00
        NVGSDK_OverlayState state; // 0x04
    } NVGSDK_OverlayStateChangedNotification;

    // size 0xC
    typedef struct NVGSDK_Notification
    {
        void* context; // 0x00
        union
        {
            NVGSDK_PermissionsChangedNotification permissionsChanged; // 0x04
            NVGSDK_OverlayStateChangedNotification overlayStateChanged; // 0x04
        };
    } NVGSDK_Notification;

    typedef void (__stdcall* NVGSDK_NotificationCallback)(NVGSDK_NotificationType, const NVGSDK_Notification*, void* context);
    typedef void (__stdcall* NVGSDK_EmptyCallback)(NVGSDK_RetCode, void* context);

    // size 0x20
    typedef struct NVGSDK_CreateInputParams
    {
        const char* appName; // 0x00
        NVGSDK_Scope* scopeTable; // 0x04
        uint32_t scopeTableSize; // 0x08
        bool pollForCallbacks; // 0x0C
        NVGSDK_NotificationCallback notificationCallback; // 0x10
        void* notificationCallbackContext; // 0x14
        uint32_t targetPid; // 0x18
        const char* targetPath; // 0x1C
    } NVGSDK_CreateInputParams;

    // size 0x10C
    typedef struct NVGSDK_CreateResponse
    {
        uint16_t versionMajor; // 0x00
        uint16_t versionMinor; // 0x02
        char gfeVersionStr[256]; // 0x04
        NVGSDK_ScopePermission* scopePermissionTable; // 0x104
        uint32_t scopePermissionTableSize; // 0x108
    } NVGSDK_CreateResponse;

    // size 0x8
    typedef struct NVGSDK_RequestPermissionsParams
    {
        NVGSDK_Scope* scopeTable; // 0x00
        uint32_t scopeTableSize; // 0x04
    } NVGSDK_RequestPermissionsParams;

    // size 0x8
    typedef struct NVGSDK_LocalizedPair
    {
        const char* localeCode; // 0x00
        const char* localizedString; // 0x04
    } NVGSDK_LocalizedPair;
}
