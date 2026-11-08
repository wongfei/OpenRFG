// Original: library/modules/runtime/gameframework/sources/global/gameframeworksystem.hpp (private)
#pragma once

#include "keen/base/tlsfmemoryallocator.hpp"
#include "keen/gameframework/gameframework.hpp"
#include "keen/graphics/graphicscommandwriter.hpp"

#include "../win32/gameframeworksystem_win32.hpp"

namespace keen
{
    // All framework state. GameFramework::main keeps it on its stack for the lifetime of the process;
    // the game only sees it as an opaque pointer.
    // size 0x304
    struct GameFrameworkSystem : public GameFrameworkSystemNative
    {
        GameFrameworkSystem(); // 0x00C10CB0
        ~GameFrameworkSystem(); // 0x00C10C30

        MemorySystem* pMemorySystem; // 0x0C
        TlsfMemoryAllocator defaultAllocator; // 0x10
        MemoryAllocator* pDefaultAllocator; // 0xF0
        FileSystem* pFileSystem; // 0xF4
        TaskSystem* pTaskSystem; // 0xF8
        GraphicsSystem* pGraphicsSystem; // 0xFC
        RenderEffectSystem* pRenderEffectSystem; // 0x100
        GraphicsCommandWriter commandWriter; // 0x104
        uint32 shaderMountHandle; // 0x28C
        bool useSound; // 0x290
        uint32 lastUpdateTime; // 0x294
        SoundSystem::System* pSoundSystem; // 0x298
        UserAccountSystem* pUserAccountSystem; // 0x29C
        SaveDataSystem* pSaveDataSystem; // 0x2A0
        unit4::SystemServices* pSystemServices; // 0x2A4
        bool useSteam; // 0x2A8
        SteamworksSystem* pSteamworksSystem; // 0x2AC
        uint32 debugMountHandle; // 0x2B0
        char userName[64]; // 0x2B4
        InputSystem* pInputSystem; // 0x2F4
        bool hideMouseCursorInFullscreen; // 0x2F8
        bool wasInFullscreen; // 0x2F9
        ApplicationProcessState currentProcessState; // 0x2FC
        bool isShutdownRequested; // 0x300
        bool renderingSuspended; // 0x301
    };
}
