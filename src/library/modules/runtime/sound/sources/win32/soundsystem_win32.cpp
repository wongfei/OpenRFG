// Original: library/modules/runtime/sound/sources/win32/soundsystem_win32.cpp
//
// SoundSystem::initialize/shutdown/update on win32 (SFX.md SX1): XAudio2 2.7 through COM (XAudio2_7.DLL from the
// DirectX June 2010 redistributable, CLSID_XAudio2), with the null provider when the device cannot be created; and the
// device change callbacks.

#include "keen/sound/soundsystem.hpp"
#include "keen/base/memoryallocator.hpp"
#include "keen/base/trace.hpp"
#include "library/modules/runtime/sound/sources/global/soundsysteminternal.hpp"

#include <new>

// ---- file-static globals
// Sound output on: rfg.exe never clears it (0 would leave the system on the null provider).
static bool s_soundSystemInitialize = true; // 0x016EFE74

namespace keen
{
    namespace SoundSystem
    {
        HINSTANCE g_XAudioDLL = nullptr; // 0x038B92C8

        // 0x00C39C00 (:31)
        // COM is initialized multithreaded on the calling (main) thread; the other threads that call keen sound do not
        // initialize COM (original).
        System* initialize(MemoryAllocator* pAllocator, const InitializationParameters& parameter)
        {
            void* pMemory = pAllocator->allocate(sizeof(System), 4, 0, nullptr);
            System* pSystem = pMemory != nullptr ? new (pMemory) System() : nullptr;
            initializeBase(pSystem, pAllocator, parameter);
            if (s_soundSystemInitialize == false)
            {
                pSystem->soundProviders[0] = &pSystem->nullProvider;
                return pSystem;
            }
            CoInitializeEx(nullptr, COINIT_MULTITHREADED);
            g_XAudioDLL = LoadLibraryExW(L"XAudio2_7.DLL", nullptr, LOAD_LIBRARY_SEARCH_SYSTEM32);
            // XAudio2Create inlined (release engine, default processor).
            IXAudio2* pAudioDevice = nullptr;
            HRESULT result = CoCreateInstance(CLSID_XAudio2, nullptr, CLSCTX_INPROC_SERVER, IID_IXAudio2, (void**)&pAudioDevice);
            if (SUCCEEDED(result))
            {
                result = pAudioDevice->Initialize(0, XAUDIO2_DEFAULT_PROCESSOR);
                if (SUCCEEDED(result))
                {
                    pSystem->xaudio2State.pAudioDevice = pAudioDevice;
                }
                else
                {
                    pAudioDevice->Release();
                }
            }
            if (result != S_OK)
            {
                debug::nativePrintArgs("[SoundSystem]: Unable to create Audio Device. Result : %x\n", result);
                pSystem->soundProviders[0] = &pSystem->nullProvider;
                pSystem->xaudio2State.pAudioDevice = nullptr;
                return pSystem;
            }
            create(&pSystem->xaudio2State, pAllocator, parameter, 0);
            createBusses(pSystem);
            pSystem->soundProviders[0] = &pSystem->xaudio2State.waveProvider;
            return pSystem;
        }

        // 0x00C39D20 (:80)
        // The System is freed without its destructor (original).
        void shutdown(System* pSoundSystem, MemoryAllocator* pAllocator)
        {
            shutdown(&pSoundSystem->xaudio2State, pAllocator);
            shutdownBase(pSoundSystem, pAllocator);
            pAllocator->free(pSoundSystem);
            if (g_XAudioDLL != nullptr)
            {
                FreeLibrary(g_XAudioDLL);
                g_XAudioDLL = nullptr;
            }
        }

        // 0x00C39D70 (:95)
        void update(System* pSystem, float timeStep)
        {
            updateBase(pSystem, timeStep);
            updateDeviceSelection(pSystem, timeStep);
            if (pSystem->xaudio2State.pAudioDevice != nullptr)
            {
                // Rounded to the nearest ms (+0.5, truncated).
                const uint32 timeStepInms = (uint32)(sint64)((double)timeStep * 1000.0 + 0.5);
                pSystem->xaudio2State.waveProvider.update(pSystem->voiceSounds.getStart(), pSystem->voiceSounds.getSize(), pSystem, timeStepInms);
            }
        }

        // 0x00C339F0 (:110)
        IXAudio2* getXAudio2(System* pSoundSystem)
        {
            if (pSoundSystem == nullptr)
            {
                return nullptr;
            }
            return pSoundSystem->xaudio2State.pAudioDevice;
        }

        // 0x00C33A10 (:115)
        bool isXaudio2Working(System* pSoundSystem)
        {
            if (pSoundSystem == nullptr)
            {
                return false;
            }
            return pSoundSystem->xaudio2State.pMasteringVoice != nullptr;
        }

        // 0x00C33A30 (:120)
        bool isInDeviceChangeCallback(System* pSoundSystem)
        {
            if (pSoundSystem == nullptr)
            {
                return false;
            }
            return pSoundSystem->xaudio2State.isInDeviceChangeCallback;
        }

        // 0x00C38960 (:125)
        void registerSoundDeviceChangedCallback(System* pSoundSystem, SoundDeviceChangedCallback* pCallback)
        {
            pSoundSystem->deviceChangedCallbacks.pushFront(pCallback);
        }

        // 0x00C38980 (:130)
        void unregisterSoundDeviceChangedCallback(System* pSoundSystem, SoundDeviceChangedCallback* pCallback)
        {
            pSoundSystem->deviceChangedCallbacks.erase(pCallback);
        }
    }

    // 0x00CD0450
    SoundDeviceChangedCallback::SoundDeviceChangedCallback()
    {
    }
}
