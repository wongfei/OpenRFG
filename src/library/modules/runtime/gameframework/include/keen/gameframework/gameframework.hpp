// Original: library/modules/runtime/gameframework/include/keen/gameframework/gameframework.hpp
// Implementation: library/modules/runtime/gameframework/sources/global/gameframework.cpp,
//                 library/modules/runtime/gameframework/sources/win32/gameframework_win32.cpp,
//                 library/modules/runtime/gameframework/sources/win32/backtraceuploader.cpp
#pragma once

#include "keen/base/filesystemtypes.hpp"
#include "keen/base/ioresult.hpp"
#include "keen/base/language.hpp"
#include "keen/base/types.hpp"
#include "keen/input/inputsystem.hpp"
#include "keen/savedata/savedatasystem.hpp"
#include "keen/sound/soundsystem.hpp"

namespace unit4
{
    struct SystemServices;
    struct SystemServicesParameters;
}

namespace keen
{
    class MemoryAllocator;
    struct MemorySystem;
    struct FileSystem;
    struct TaskSystem;
    struct GraphicsSystem;
    struct RenderEffectSystem;
    class GraphicsCommandWriter;
    struct UserAccountSystem;
    struct SaveDataSystem;
    class SteamworksSystem;
    struct GameFrameworkSystem;

    enum ApplicationProcessState
    {
        ApplicationProcessState_Foreground = 0,
        ApplicationProcessState_ForegroundWithSystemMessageOpen = 1,
        ApplicationProcessState_Background = 2,
    };

    enum FatalErrorTypeId
    {
        FatalErrorTypeId_ContentError = 0,
        FatalErrorTypeId_Count = 1,
    };

    enum RegionId
    {
        RegionId_Unknown = 0,
        RegionId_Europe = 1,
        RegionId_NorthAmerica = 2,
        RegionId_Count = 3,
    };

    // size 0xC
    struct WindowAspectRatioConstraintData
    {
        bool isConstrained; // 0x00
        float minAspectRatio; // 0x04
        float maxAspectRatio; // 0x08
    };

    // Filled by parseArguments, GameFramework::parsePlatformArguments and GameApplication::fillGameframeworkParameters.
    // size 0xE0
    struct GameFrameworkParameters
    {
        GameFrameworkParameters(const char* pInProjectName, const char* pInGameBuildDirectory); // 0x00C0F620
        void init(); // 0x00C0E050

        const char* pProjectName; // 0x00
        const char* pUserName; // 0x04
        const char* pProjectGameBuildDirectory; // 0x08
        const char* pPlatform; // 0x0C
        const char* pRegionCode; // 0x10
        const char* pPakFileNameFormat; // 0x14
        int windowPosX; // 0x18
        int windowPosY; // 0x1C
        uint32 screenWidth; // 0x20
        uint32 screenHeight; // 0x24
        const wchar_t* windowIcon; // 0x28
        bool isStandardIcon; // 0x2C
        void* pParentWindowHandle; // 0x30
        bool autoPosWindows; // 0x34
        bool quitWithEscape; // 0x35
        bool resizeableWindow; // 0x36
        bool simulateTouchInput; // 0x37
        void (*pCustomCrashDumpCallback)(const char*, const char*); // 0x38
        bool hideMouseCursorInFullscreen; // 0x3C
        WindowAspectRatioConstraintData aspectRatioConstraintData; // 0x40
        bool borderLessWindow; // 0x4C
        bool headLess; // 0x4D
        uint32 immediateGeometryBufferSize; // 0x50
        uint32 maxTaskSystemWorkerThreads; // 0x54
        FileSystemCreationParameters fileSystemParameters; // 0x58
        bool createDepthSurface; // 0x80
        bool useNetworkFileSystem; // 0x81
        bool initializeSoundSystem; // 0x82
        SoundSystem::InitializationParameters soundSystemParameters; // 0x84
        bool useSteam; // 0xA4
        uint32 steamAppId; // 0xA8
        uint64 bootInviteLobbySteamId64; // 0xB0
        SaveData::SaveDataSystemCreationParameters* pSaveDataSystemParameters; // 0xB8
        const unit4::SystemServicesParameters* pSystemServicesParameters; // 0xBC
        uint32 shaderAllocatorSize; // 0xC0
        InputSystemCreationParameters inputSystemCreationParameters; // 0xC4
    };

    // size 0xC
    struct GameFrameworkWindowInfo
    {
        uint32 width; // 0x00
        uint32 height; // 0x04
        void* pWindowHandle; // 0x08
    };

    namespace GameFramework
    {
        // Entry point called from WinMain (gameframeworkmain.cpp).
        int main(int argc, char** argv); // 0x00C10D40

        // Service accessors (gameframework.cpp).
        MemorySystem* getMemorySystem(GameFrameworkSystem* pGameFramework); // 0x00C0E300
        MemoryAllocator* getDefaultAllocator(GameFrameworkSystem* pGameFramework); // 0x00C0E310
        FileSystem* getFileSystem(GameFrameworkSystem* pGameFramework); // 0x00C0E320
        TaskSystem* getTaskSystem(GameFrameworkSystem* pGameFramework); // 0x00C0E330
        GraphicsSystem* getGraphicsSystem(GameFrameworkSystem* pGameFramework); // 0x00C0E340
        RenderEffectSystem* getRenderEffectSystem(GameFrameworkSystem* pGameFramework); // 0x00C0E350
        InputSystem* getInputSystem(GameFrameworkSystem* pGameFramework); // 0x00C0E360
        float getLastFrameTimeInterval(const GameFrameworkSystem* pGameFramework); // 0x00C0E370
        SoundSystem::System* getSoundSystem(GameFrameworkSystem* pGameFramework); // 0x00C0E400
        UserAccountSystem* getUserAccountSystem(GameFrameworkSystem* pGameFramework); // 0x00C0E490
        SaveDataSystem* getSaveDataSystem(GameFrameworkSystem* pGameFramework); // 0x00C0E4A0
        unit4::SystemServices* getSystemServices(GameFrameworkSystem* pGameFramework); // 0x00C0E4B0
        SteamworksSystem* getSteamworksSystem(GameFrameworkSystem* pGameFramework); // 0x00C0E4C0
        IoResult<uint32> mountHostPath(GameFrameworkSystem* pGameFramework, const char* pTargetPath, const char* pHostPath); // 0x00C0E670
        const char* getLocalUserName(const GameFrameworkSystem* pGameFramework); // 0x00C0E690
        ApplicationProcessState getCurrentProcessState(const GameFrameworkSystem* pGameFramework); // 0x00C0E6B0

        // Platform queries and control (gameframework_win32.cpp).
        LanguageId getSystemLanguage(const GameFrameworkSystem* pGameFramework); // 0x00C0EE90
        RegionId getSystemRegion(const GameFrameworkSystem* pGameFramework); // 0x00C0EF80
        void processFatalError(GameFrameworkSystem* pGameFramework, FatalErrorTypeId type); // 0x00C0EFD0
        bool wantsToQuit(const GameFrameworkSystem* pGameFramework); // 0x00C0F520
        bool getGameFrameworkWindowInfo(GameFrameworkWindowInfo* pWindowInfo, const GameFrameworkSystem* pGameFramework); // 0x00C0FC30
        void quitGame(GameFrameworkSystem* pFramework); // 0x00C0F560
        void minimizeGameWindow(GameFrameworkSystem* pFramework); // 0x00C0F570

        // Crash reporting (backtraceuploader.cpp). rfg::Application registers the publisher's Backtrace endpoint here.
        void setBacktraceURL(const char* pUrl); // 0x00C0E850
    }
}
