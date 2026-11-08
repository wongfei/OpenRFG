// Original: library/modules/thirdparty/nvidiahighlights/include/gfesdk/highlights/ihighlights.h (placement inferred)
//
// NVIDIA GeForce Experience SDK, third party (stub_impl.md S2): the highlights types (as the PDB records them) and the
// functions rfg.exe imports from GfeSDK.dll (__cdecl, C linkage).
#pragma once

#include "library/modules/thirdparty/nvidiahighlights/include/gfesdk/sdk_types.h"

extern "C"
{
    typedef enum NVGSDK_HighlightType
    {
        NVGSDK_HIGHLIGHT_TYPE_NONE = 0,
        NVGSDK_HIGHLIGHT_TYPE_MILESTONE = 1,
        NVGSDK_HIGHLIGHT_TYPE_ACHIEVEMENT = 2,
        NVGSDK_HIGHLIGHT_TYPE_INCIDENT = 4,
        NVGSDK_HIGHLIGHT_TYPE_STATE_CHANGE = 8,
        NVGSDK_HIGHLIGHT_TYPE_UNANNOUNCED = 16,
        NVGSDK_HIGHLIGHT_TYPE_MAX = 32,
    } NVGSDK_HighlightType;

    typedef enum NVGSDK_HighlightSignificance
    {
        NVGSDK_HIGHLIGHT_SIGNIFICANCE_NONE = 0,
        NVGSDK_HIGHLIGHT_SIGNIFICANCE_EXTREMELY_BAD = 1,
        NVGSDK_HIGHLIGHT_SIGNIFICANCE_VERY_BAD = 2,
        NVGSDK_HIGHLIGHT_SIGNIFICANCE_BAD = 4,
        NVGSDK_HIGHLIGHT_SIGNIFICANCE_NEUTRAL = 16,
        NVGSDK_HIGHLIGHT_SIGNIFICANCE_GOOD = 256,
        NVGSDK_HIGHLIGHT_SIGNIFICANCE_VERY_GOOD = 512,
        NVGSDK_HIGHLIGHT_SIGNIFICANCE_EXTREMELY_GOOD = 1024,
        NVGSDK_HIGHLIGHT_SIGNIFICANCE_MAX = 2048,
    } NVGSDK_HighlightSignificance;

    // size 0x18
    typedef struct NVGSDK_Highlight
    {
        const char* id; // 0x00
        bool userInterest; // 0x04
        NVGSDK_HighlightType highlightTags; // 0x08
        NVGSDK_HighlightSignificance significance; // 0x0C
        NVGSDK_LocalizedPair* nameTable; // 0x10
        uint32_t nameTableSize; // 0x14
    } NVGSDK_Highlight;

    // size 0xC
    typedef struct NVGSDK_HighlightConfigParams
    {
        NVGSDK_Highlight* highlightDefinitionTable; // 0x00
        uint32_t highlightTableSize; // 0x04
        const char* defaultLocale; // 0x08
    } NVGSDK_HighlightConfigParams;

    // size 0xC
    typedef struct NVGSDK_HighlightOpenGroupParams
    {
        const char* groupId; // 0x00
        NVGSDK_LocalizedPair* groupDescriptionTable; // 0x04
        uint32_t groupDescriptionTableSize; // 0x08
    } NVGSDK_HighlightOpenGroupParams;

    // size 0x8
    typedef struct NVGSDK_HighlightCloseGroupParams
    {
        const char* groupId; // 0x00
        bool destroyHighlights; // 0x04
    } NVGSDK_HighlightCloseGroupParams;

    // size 0x8
    typedef struct NVGSDK_ScreenshotHighlightParams
    {
        const char* groupId; // 0x00
        const char* highlightId; // 0x04
    } NVGSDK_ScreenshotHighlightParams;

    // size 0x10
    typedef struct NVGSDK_VideoHighlightParams
    {
        const char* groupId; // 0x00
        const char* highlightId; // 0x04
        int startDelta; // 0x08
        int endDelta; // 0x0C
    } NVGSDK_VideoHighlightParams;

    // size 0xC
    typedef struct NVGSDK_GroupView
    {
        const char* groupId; // 0x00
        NVGSDK_HighlightType tagsFilter; // 0x04
        NVGSDK_HighlightSignificance significanceFilter; // 0x08
    } NVGSDK_GroupView;

    // size 0x8
    typedef struct NVGSDK_SummaryParams
    {
        NVGSDK_GroupView* groupSummaryTable; // 0x00
        uint32_t groupSummaryTableSize; // 0x04
    } NVGSDK_SummaryParams;

    // size 0x2
    typedef struct NVGSDK_Highlights_NumberOfHighlights
    {
        uint16_t numberOfHighlights; // 0x00
    } NVGSDK_Highlights_NumberOfHighlights;

    typedef void (__stdcall* NVGSDK_Highlights_GetNumberOfHighlightsCallback)(NVGSDK_RetCode, const NVGSDK_Highlights_NumberOfHighlights*, void* context);

    void NVGSDK_Highlights_ConfigureAsync(NVGSDK_HANDLE* pHandle, const NVGSDK_HighlightConfigParams* pParams, NVGSDK_EmptyCallback callback, void* pContext);
    void NVGSDK_Highlights_OpenGroupAsync(NVGSDK_HANDLE* pHandle, const NVGSDK_HighlightOpenGroupParams* pParams, NVGSDK_EmptyCallback callback, void* pContext);
    void NVGSDK_Highlights_CloseGroupAsync(NVGSDK_HANDLE* pHandle, const NVGSDK_HighlightCloseGroupParams* pParams, NVGSDK_EmptyCallback callback, void* pContext);
    void NVGSDK_Highlights_SetScreenshotHighlightAsync(NVGSDK_HANDLE* pHandle, const NVGSDK_ScreenshotHighlightParams* pParams, NVGSDK_EmptyCallback callback, void* pContext);
    void NVGSDK_Highlights_SetVideoHighlightAsync(NVGSDK_HANDLE* pHandle, const NVGSDK_VideoHighlightParams* pParams, NVGSDK_EmptyCallback callback, void* pContext);
    void NVGSDK_Highlights_OpenSummaryAsync(NVGSDK_HANDLE* pHandle, const NVGSDK_SummaryParams* pParams, NVGSDK_EmptyCallback callback, void* pContext);
    void NVGSDK_Highlights_GetNumberOfHighlightsAsync(NVGSDK_HANDLE* pHandle, const NVGSDK_GroupView* pParams, NVGSDK_Highlights_GetNumberOfHighlightsCallback callback, void* pContext);
}
