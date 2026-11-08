// Original: library/modules/thirdparty/nvidiahighlights/include/gfesdk/isdk.h (placement inferred)
//
// NVIDIA GeForce Experience SDK, third party (stub_impl.md S2): the core functions rfg.exe imports from GfeSDK.dll
// (__cdecl, C linkage; linked through the import library of the shipped DLL, tools/dll_implib/dll_implib.py).
#pragma once

#include "library/modules/thirdparty/nvidiahighlights/include/gfesdk/sdk_types.h"

extern "C"
{
    NVGSDK_RetCode NVGSDK_Create(NVGSDK_HANDLE** ppHandle, const NVGSDK_CreateInputParams* pParams, NVGSDK_CreateResponse* pResponse);
    NVGSDK_RetCode NVGSDK_Release(NVGSDK_HANDLE* pHandle);
    void NVGSDK_Poll(NVGSDK_HANDLE* pHandle);
    void NVGSDK_RequestPermissionsAsync(NVGSDK_HANDLE* pHandle, const NVGSDK_RequestPermissionsParams* pParams, NVGSDK_EmptyCallback callback, void* pContext);
}
