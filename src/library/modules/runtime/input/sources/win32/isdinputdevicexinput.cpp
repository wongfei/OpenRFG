// Original: library/modules/runtime/input/sources/win32/isdinputdevicexinput.cpp
//
// The DirectX documentation's IsXInputDevice: a DirectInput product GUID is an XInput device when a plug and play
// device with "IG_" in its id has the same vendor and product id (WMI's Win32_PNPEntity). DirectInput enumeration
// leaves those to XInput.

#include "library/modules/runtime/input/sources/win32/controllerdinput_win32.hpp"

#include <oleauto.h>
#include <wbemidl.h>
#include <stdio.h>
#include <wchar.h>

namespace keen
{
    // 0x00C75B90 (:23)
    // Magic numbers: the devices are fetched 20 at a time, each fetch waits up to 10 seconds.
    int dxDocu_IsXInputDevice(const GUID* pGuidProductFromDirectInput)
    {
        IWbemLocator* pIWbemLocator = nullptr;
        IEnumWbemClassObject* pEnumDevices = nullptr;
        IWbemClassObject* pDevices[20] = {};
        IWbemServices* pIWbemServices = nullptr;
        BSTR bstrDeviceID = nullptr;
        DWORD uReturned = 0;
        bool bIsXinputDevice = false;
        const HRESULT hrCoInit = CoInitialize(nullptr);
        HRESULT hr = CoCreateInstance(__uuidof(WbemLocator), nullptr, CLSCTX_INPROC_SERVER, __uuidof(IWbemLocator), (LPVOID*)&pIWbemLocator);
        BSTR bstrNamespace = nullptr;
        BSTR bstrClassName = nullptr;
        if (SUCCEEDED(hr) && pIWbemLocator != nullptr)
        {
            bstrNamespace = SysAllocString(L"\\\\.\\root\\cimv2");
            if (bstrNamespace != nullptr)
            {
                bstrClassName = SysAllocString(L"Win32_PNPEntity");
                if (bstrClassName != nullptr && (bstrDeviceID = SysAllocString(L"DeviceID")) != nullptr &&
                    SUCCEEDED(pIWbemLocator->ConnectServer(bstrNamespace, nullptr, nullptr, 0L, 0L, nullptr, nullptr, &pIWbemServices)) &&
                    pIWbemServices != nullptr)
                {
                    CoSetProxyBlanket(pIWbemServices, RPC_C_AUTHN_WINNT, RPC_C_AUTHZ_NONE, nullptr, RPC_C_AUTHN_LEVEL_CALL, RPC_C_IMP_LEVEL_IMPERSONATE, nullptr, EOAC_NONE);
                    hr = pIWbemServices->CreateInstanceEnum(bstrClassName, 0, nullptr, &pEnumDevices);
                    if (SUCCEEDED(hr) && pEnumDevices != nullptr)
                    {
                        for (;;)
                        {
                            hr = pEnumDevices->Next(10000, 20, pDevices, &uReturned);
                            if (FAILED(hr) || uReturned == 0)
                            {
                                break;
                            }
                            for (UINT iDevice = 0; iDevice < uReturned; ++iDevice)
                            {
                                VARIANT var;
                                hr = pDevices[iDevice]->Get(bstrDeviceID, 0L, &var, nullptr, nullptr);
                                if (SUCCEEDED(hr) && var.vt == VT_BSTR && var.bstrVal != nullptr && wcsstr(var.bstrVal, L"IG_") != nullptr)
                                {
                                    DWORD dwPid = 0;
                                    DWORD dwVid = 0;
                                    const WCHAR* strVid = wcsstr(var.bstrVal, L"VID_");
                                    if (strVid != nullptr && swscanf_s(strVid, L"VID_%4X", &dwVid) != 1)
                                    {
                                        dwVid = 0;
                                    }
                                    const WCHAR* strPid = wcsstr(var.bstrVal, L"PID_");
                                    if (strPid != nullptr && swscanf_s(strPid, L"PID_%4X", &dwPid) != 1)
                                    {
                                        dwPid = 0;
                                    }
                                    if (MAKELONG(dwVid, dwPid) == (LONG)pGuidProductFromDirectInput->Data1)
                                    {
                                        bIsXinputDevice = true;
                                        goto LCleanup;
                                    }
                                }
                                if (pDevices[iDevice] != nullptr)
                                {
                                    pDevices[iDevice]->Release();
                                    pDevices[iDevice] = nullptr;
                                }
                            }
                        }
                    }
                }
            LCleanup:
                SysFreeString(bstrNamespace);
                if (bstrDeviceID != nullptr)
                {
                    SysFreeString(bstrDeviceID);
                }
                if (bstrClassName != nullptr)
                {
                    SysFreeString(bstrClassName);
                }
            }
        }
        for (UINT iDevice = 0; iDevice < 20u; ++iDevice)
        {
            if (pDevices[iDevice] != nullptr)
            {
                pDevices[iDevice]->Release();
                pDevices[iDevice] = nullptr;
            }
        }
        if (pEnumDevices != nullptr)
        {
            pEnumDevices->Release();
        }
        if (pIWbemLocator != nullptr)
        {
            pIWbemLocator->Release();
        }
        if (pIWbemServices != nullptr)
        {
            pIWbemServices->Release();
        }
        if (SUCCEEDED(hrCoInit))
        {
            CoUninitialize();
        }
        return bIsXinputDevice ? 1 : 0;
    }
}
