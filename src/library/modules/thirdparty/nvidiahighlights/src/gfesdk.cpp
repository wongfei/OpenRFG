// Stand-in for GfeSDK.dll (isdk.h, highlights/ihighlights.h), new code: built instead of linking lib/win32/GfeSDK.lib
// when CMake's OPENRFG_WITH_CUSTOM_GFESDK is on. GeForce Experience is never reached: NVGSDK_Create fails with
// NVGSDK_ERR_NO_CONNECTION, which nvidia_highlights_init already treats as no GeForce Experience (it returns false and
// the highlights stay uninitialized, so no other call is made). The other functions do nothing and never call their
// callbacks (the SDK runs them from NVGSDK_Poll, and there is nothing to report). The program does not need GfeSDK.dll
// next to it.
//
// Only the 11 functions rfg.exe imports are provided (isdk.h, ihighlights.h); the DLL's other exports are not used.
#include "library/modules/thirdparty/nvidiahighlights/include/gfesdk/isdk.h"
#include "library/modules/thirdparty/nvidiahighlights/include/gfesdk/highlights/ihighlights.h"

extern "C"
{
    NVGSDK_RetCode NVGSDK_Create(NVGSDK_HANDLE** ppHandle, const NVGSDK_CreateInputParams* pParams, NVGSDK_CreateResponse* pResponse)
    {
        (void)pParams;
        (void)pResponse;
        if (ppHandle != nullptr)
        {
            *ppHandle = nullptr;
        }
        return NVGSDK_ERR_NO_CONNECTION;
    }

    NVGSDK_RetCode NVGSDK_Release(NVGSDK_HANDLE* pHandle)
    {
        (void)pHandle;
        return NVGSDK_SUCCESS;
    }

    void NVGSDK_Poll(NVGSDK_HANDLE* pHandle)
    {
        (void)pHandle;
    }

    void NVGSDK_RequestPermissionsAsync(NVGSDK_HANDLE* pHandle, const NVGSDK_RequestPermissionsParams* pParams, NVGSDK_EmptyCallback callback, void* pContext)
    {
        (void)pHandle;
        (void)pParams;
        (void)callback;
        (void)pContext;
    }

    void NVGSDK_Highlights_ConfigureAsync(NVGSDK_HANDLE* pHandle, const NVGSDK_HighlightConfigParams* pParams, NVGSDK_EmptyCallback callback, void* pContext)
    {
        (void)pHandle;
        (void)pParams;
        (void)callback;
        (void)pContext;
    }

    void NVGSDK_Highlights_OpenGroupAsync(NVGSDK_HANDLE* pHandle, const NVGSDK_HighlightOpenGroupParams* pParams, NVGSDK_EmptyCallback callback, void* pContext)
    {
        (void)pHandle;
        (void)pParams;
        (void)callback;
        (void)pContext;
    }

    void NVGSDK_Highlights_CloseGroupAsync(NVGSDK_HANDLE* pHandle, const NVGSDK_HighlightCloseGroupParams* pParams, NVGSDK_EmptyCallback callback, void* pContext)
    {
        (void)pHandle;
        (void)pParams;
        (void)callback;
        (void)pContext;
    }

    void NVGSDK_Highlights_SetScreenshotHighlightAsync(NVGSDK_HANDLE* pHandle, const NVGSDK_ScreenshotHighlightParams* pParams, NVGSDK_EmptyCallback callback, void* pContext)
    {
        (void)pHandle;
        (void)pParams;
        (void)callback;
        (void)pContext;
    }

    void NVGSDK_Highlights_SetVideoHighlightAsync(NVGSDK_HANDLE* pHandle, const NVGSDK_VideoHighlightParams* pParams, NVGSDK_EmptyCallback callback, void* pContext)
    {
        (void)pHandle;
        (void)pParams;
        (void)callback;
        (void)pContext;
    }

    void NVGSDK_Highlights_OpenSummaryAsync(NVGSDK_HANDLE* pHandle, const NVGSDK_SummaryParams* pParams, NVGSDK_EmptyCallback callback, void* pContext)
    {
        (void)pHandle;
        (void)pParams;
        (void)callback;
        (void)pContext;
    }

    void NVGSDK_Highlights_GetNumberOfHighlightsAsync(NVGSDK_HANDLE* pHandle, const NVGSDK_GroupView* pParams, NVGSDK_Highlights_GetNumberOfHighlightsCallback callback, void* pContext)
    {
        (void)pHandle;
        (void)pParams;
        (void)callback;
        (void)pContext;
    }
}
