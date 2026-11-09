// Original: library/modules/runtime/gameframework/sources/global/gameframework.cpp
//
// The framework services: their creation in initialize (memory system, default allocator, Steam, window, file system,
// task system, display, render effects, sound, user accounts, save data, system services, input), the per-frame
// update (frame timing, window messages, input events to the application), the frame begin/end and the shutdown.
//
// Deviation (orig_main.md 3.1, stub_impl.md S2): Application::fillGameframeworkParameters sets useSteam = false when
// OpenRFG runs with --no-steam; initialize then creates no Steam services (Steamworks, user accounts, save data, system
// services). The deviations are commented at their lines.

#include "library/modules/runtime/gameframework/sources/global/gameframeworksystem.hpp"

#include "game/build/win32/steam/master/generated_sources/memorylayoutdefinition.hpp"
#include "game/build/win32/steam/master/generated_sources/version.hpp"
#include "keen/base/filepath.hpp"
#include "keen/base/filesystem.hpp"
#include "keen/base/framenumber.hpp"
#include "keen/base/inivariables.hpp"
#include "keen/base/memorysystem.hpp"
#include "keen/base/string.hpp"
#include "keen/base/stringencoding.hpp"
#include "keen/base/systemtimer.hpp"
#include "keen/base/thread.hpp"
#include "keen/base/trace.hpp"
#include "keen/gameframework/gameapplication.hpp"
#include "keen/graphics/graphicssystem.hpp"
#include "keen/linkedresources/linkedresources.hpp"
#include "keen/rendereffect/rendereffectsystem.hpp"
#include "keen/steam/steamworkssystem.hpp"
#include "keen/task/tasksystem.hpp"
#include "keen/useraccount/steam/useraccountsystem_steam.hpp"
#include "keen/useraccount/useraccountsystem.hpp"
#include "library/modules/runtime/gameframework/sources/global/gameframework_platform.hpp"
#include "library/modules/runtime/gameframework/sources/global/gameframeworkinternal.hpp"
#include "library/modules/runtime/graphics/sources/d3d11/graphicscommandbuffer_d3d11.hpp"
#include "library/modules/runtime/graphics/sources/d3d11/rendertarget_d3d11.hpp"
#include "library/modules/runtime/network/sources/global/networkplatform.hpp"
#include "library/modules/runtime/systemservices/sources/steam/systemservices_steam.hpp"

#include <stdlib.h>
#include <windows.h>

namespace keen
{
    namespace GameFramework
    {
        // 0x00C0E300
        MemorySystem* getMemorySystem(GameFrameworkSystem* pGameFramework)
        {
            return pGameFramework->pMemorySystem;
        }

        // 0x00C0E310
        MemoryAllocator* getDefaultAllocator(GameFrameworkSystem* pGameFramework)
        {
            return pGameFramework->pDefaultAllocator;
        }

        // 0x00C0E330
        TaskSystem* getTaskSystem(GameFrameworkSystem* pGameFramework)
        {
            return pGameFramework->pTaskSystem;
        }
    }
}

namespace keen
{
    static bool s_showTextConsole; // 0x038B6BCB (only read: always false)
    // Magic number: frames are at least 2 ms apart (GameFramework::update sleeps the rest).
    static uint32 s_minFrameTime = 2; // 0x016EF8C8

    // The string table entries of the framework's message boxes: each message has one entry per language family, at
    // these offsets from its first id (the original builds a 15 entry table on the stack for each message; the
    // families Pt and Br, and the three Chinese ones, share entries).
    static const uint32 s_messageIdOffsetsByLanguageFamily[15] = {0, 1, 2, 3, 4, 6, 5, 7, 8, 9, 10, 4, 11, 11, 12};

    // The message of one family from the exe's string table (gameframeworklocatexts.rc). Deviation: the buffer is
    // emptied first, so a failed lookup logs nothing instead of the uninitialized stack buffer (LoadStringW finding
    // nothing leaves the buffer as it was).
    static void loadFrameworkMessage(WCHAR* pBuffer, int capacity, uint32 firstId, LanguageFamily family)
    {
        pBuffer[0] = L'\0';
        LoadStringW(nullptr, firstId + s_messageIdOffsetsByLanguageFamily[family], pBuffer, capacity);
    }

    // OpenRFG addition: the framework's message boxes are replaced by log lines. A short English description of the
    // message is printed with the localized string table text (see loadFrameworkMessage).
    static void logFrameworkMessage(const char* pCaption, const char* pDescription, const WCHAR* pText)
    {
        char text[4096];
        convertUTF16ToUTF8(text, sizeof(text), (const uint16*)pText);
        debug::nativePrintArgs("[%s] %s. %s\n", pCaption, pDescription, text);
    }

    // 0x00C0F620 (gameframework.hpp, inline)
    GameFrameworkParameters::GameFrameworkParameters(const char* pInProjectName, const char* pInGameBuildDirectory)
    {
        // Magic numbers: the file system's limits (32 mount points, memory streams and files, 16 devices and native
        // streams, 4 pak and alias streams, pak files up to 550,000,000 bytes, 64 KB native read buffer).
        fileSystemParameters.maxMountPoints = 0x20u;
        fileSystemParameters.maxMemoryStreamCount = 0x20u;
        fileSystemParameters.maxMemoryFileCount = 0x20u;
        fileSystemParameters.maxDeviceCount = 0x10u;
        fileSystemParameters.maxNativeStreamCount = 0x10u;
        fileSystemParameters.maxPakStreamCount = 4u;
        fileSystemParameters.maxAliasStreamCount = 4u;
        fileSystemParameters.maxPakFileSize = 550000000u;
        fileSystemParameters.nativeThreadBufferSize = 0x10000u;
        fileSystemParameters.enableDefaultMount = true;
        fileSystemParameters.useReadThreads = false;
        init();
        pProjectName = pInProjectName;
        pProjectGameBuildDirectory = pInGameBuildDirectory;
    }

    // 0x00C0E050 (gameframework.hpp, inline)
    void GameFrameworkParameters::init()
    {
        pProjectName = nullptr;
        pProjectGameBuildDirectory = nullptr;
        pUserName = nullptr;
        pPlatform = "ps3";
        pRegionCode = "eu";
        pPakFileNameFormat = nullptr;
        createDepthSurface = true;
        useSteam = false;
        steamAppId = 0;
        bootInviteLobbySteamId64 = 0;
        // ~0: as many task workers as hardware threads (createTaskSystem)
        maxTaskSystemWorkerThreads = 0xFFFFFFFFu;
        pSaveDataSystemParameters = nullptr;
        pSystemServicesParameters = nullptr;
        useNetworkFileSystem = false;
        initializeSoundSystem = false;
        // Magic numbers: the window at (60, 60), the size from the application (0: 1280x720, initializeBase).
        windowPosX = 0x3C;
        windowPosY = 0x3C;
        screenWidth = 0;
        screenHeight = 0;
        // Magic number: IDI_APPLICATION (a standard icon).
        windowIcon = MAKEINTRESOURCEW(0x7F00);
        isStandardIcon = true;
        pParentWindowHandle = nullptr;
        autoPosWindows = false;
        quitWithEscape = false;
        resizeableWindow = false;
        simulateTouchInput = false;
        pCustomCrashDumpCallback = nullptr;
        hideMouseCursorInFullscreen = true;
        aspectRatioConstraintData.isConstrained = false;
        borderLessWindow = false;
        headLess = false;
        // ~0: initializeDisplay keeps its default immediate geometry buffer size
        immediateGeometryBufferSize = 0xFFFFFFFFu;
    }

    // 0x00C0DFE0 (inputsystem.hpp:51, inline)
    InputSystemCreationParameters::InputSystemCreationParameters()
    {
        maxEventCount = DefaultMaxEventCount;
        pWindowHandle = nullptr;
        pSecondaryWindowHandle = nullptr;
        pSteamWorksSystem = nullptr;
        pControllerActions = nullptr;
        controllerActionCount = 0;
        enableGyroSupport = false;
    }

    // 0x00C10CB0
    GameFrameworkSystem::GameFrameworkSystem()
    {
    }

    // 0x00C10C30
    GameFrameworkSystem::~GameFrameworkSystem()
    {
    }

    namespace GameFramework
    {
        namespace Internal
        {
            // 0x00C0E2F0
            bool getShowTextConsole()
            {
                return s_showTextConsole;
            }

            // 0x00C0E6A0
            bool isRenderingSuspended(GameFrameworkSystem* pGameFramework)
            {
                return pGameFramework->renderingSuspended;
            }
        }

        // 0x00C0E320
        FileSystem* getFileSystem(GameFrameworkSystem* pGameFramework)
        {
            return pGameFramework->pFileSystem;
        }

        // 0x00C0E340
        GraphicsSystem* getGraphicsSystem(GameFrameworkSystem* pGameFramework)
        {
            return pGameFramework->pGraphicsSystem;
        }

        // 0x00C0E350
        RenderEffectSystem* getRenderEffectSystem(GameFrameworkSystem* pGameFramework)
        {
            return pGameFramework->pRenderEffectSystem;
        }

        // 0x00C0E360
        InputSystem* getInputSystem(GameFrameworkSystem* pGameFramework)
        {
            return pGameFramework->pInputSystem;
        }

        // 0x00C0E370
        float getLastFrameTimeInterval(const GameFrameworkSystem*)
        {
            return getLastFrameDurationInSeconds();
        }

        // 0x00C0E380
        void render(GameFrameworkSystem*, GraphicsCommandWriter*)
        {
        }

        // 0x00C0E390
        GraphicsCommandWriter* beginFrame(GameFrameworkSystem* pGameFramework)
        {
            GraphicsCommandBuffer* pTarget = graphics::beginFrame(pGameFramework->pGraphicsSystem);
            if (pTarget == nullptr)
            {
                return nullptr;
            }
            pGameFramework->commandWriter.beginWriting(pTarget, pGameFramework->pGraphicsSystem);
            return &pGameFramework->commandWriter;
        }

        // 0x00C0E3D0
        void endFrame(GameFrameworkSystem* pGameFramework, GameApplication*)
        {
            pGameFramework->commandWriter.endWriting();
            graphics::endFrame(pGameFramework->pGraphicsSystem);
        }

        // 0x00C0E400
        SoundSystem::System* getSoundSystem(GameFrameworkSystem* pGameFramework)
        {
            return pGameFramework->pSoundSystem;
        }

        // 0x00C0E410
        // Escape quits when the parameters ask for it; while an Alt+Enter is pending (wndProc counts them, updateBase
        // toggles the window mode) its Enter key is swallowed; a focus gained gives the graphics system its focus back.
        // A button or axis of a player's controller keeps the system awake. Everything else goes to the application.
        // Magic numbers: keen's key ids 0x12 (Escape) and 0x10 (Enter).
        // Original bug kept: a Key event's code is a WM_CHAR character, not a keen key id, so the Key tests below match
        // Ctrl+R (0x12) and Ctrl+P (0x10) rather than Escape and Enter.
        void processInputEvent(GameFrameworkSystem* pGameFramework, GameApplication* pApplication, const InputEvent& inputEvent)
        {
            if (pGameFramework->quitWithEscape && inputEvent.type == InputEventType_Key && inputEvent.data.key.keyCode == 0x12u)
            {
                pGameFramework->quit = true;
                return;
            }
            if (pGameFramework->triggerAltEnter > 0 &&
                (inputEvent.type == InputEventType_Key ||
                 (inputEvent.type == InputEventType_RawButtonDown && inputEvent.controllerClass == ControllerClass_Keyboard)) &&
                inputEvent.data.key.keyCode == 0x10u)
            {
                return;
            }
            if (inputEvent.type == InputEventType_FocusRestored && pGameFramework->pGraphicsSystem != nullptr)
            {
                graphics::regainFocus(pGameFramework->pGraphicsSystem);
            }
            if (inputEvent.localPlayerId != 0xFFu &&
                (inputEvent.type == InputEventType_RawButtonDown || inputEvent.type == InputEventType_RawButtonUp ||
                 inputEvent.type == InputEventType_RawAxis))
            {
                input::resetSystemIdleState();
            }
            pApplication->processInputEvent(inputEvent);
        }

        // 0x00C0E490
        UserAccountSystem* getUserAccountSystem(GameFrameworkSystem* pGameFramework)
        {
            return pGameFramework->pUserAccountSystem;
        }

        // 0x00C0E4A0
        SaveDataSystem* getSaveDataSystem(GameFrameworkSystem* pGameFramework)
        {
            return pGameFramework->pSaveDataSystem;
        }

        // 0x00C0E4B0
        unit4::SystemServices* getSystemServices(GameFrameworkSystem* pGameFramework)
        {
            return pGameFramework->pSystemServices;
        }

        // 0x00C0E4C0
        SteamworksSystem* getSteamworksSystem(GameFrameworkSystem* pGameFramework)
        {
            return pGameFramework->pSteamworksSystem;
        }

        // 0x00C0F9F0
        // The linked resources at /rom (the stock shaders, also at /shader) and the publisher's debug directory at
        // /debug (missing on any other machine: the mount fails and the handle is ~0).
        void createFileSystem(GameFrameworkSystem* pGameFramework, const GameFrameworkParameters& parameters)
        {
            pGameFramework->pFileSystem = file::createFileSystem(pGameFramework->pDefaultAllocator, parameters.fileSystemParameters);
            mountLinkedResources(pGameFramework->pFileSystem, "/rom");
            FilePath debugPath;
            debugPath.createCombinedPath(getProjectBaseDirectoryString(), "debug");
            const IoResult<uint32> debugMount = file::mountNativeHostPath(pGameFramework->pFileSystem, "/debug", debugPath.getCompletePath(), false);
            if (debugMount.error == IoError_Ok)
            {
                pGameFramework->debugMountHandle = debugMount.result;
            }
            else
            {
                pGameFramework->debugMountHandle = 0xFFFFFFFFu;
            }
            const IoResult<uint32> shaderMount = file::mountAliasPath(pGameFramework->pFileSystem, "/shader", "/rom");
            pGameFramework->shaderMountHandle = shaderMount.result;
        }

        // 0x00C0E4D0
        // Original behaviour kept: the /debug mount is not unmounted.
        void destroyFileSystem(GameFrameworkSystem* pGameFramework)
        {
            unmountLinkedResources(pGameFramework->pFileSystem);
            file::unmount(pGameFramework->pFileSystem, pGameFramework->shaderMountHandle);
            file::destroyFileSystem(pGameFramework->pFileSystem);
            pGameFramework->pFileSystem = nullptr;
        }

        // 0x00C0E510
        // The workers get every hardware thread but the first; with maxTaskSystemWorkerThreads 0 (rfg.exe) there are
        // none.
        // Original bug kept: while there are more threads than allowed, the loop clears ~(i * 2) instead of a single
        // thread's bit (i counting from 0), and it counts the bits of the mask before the first thread was removed.
        void createTaskSystem(GameFrameworkSystem* pGameFramework, const GameFrameworkParameters& parameters)
        {
            TaskSystemParameters taskParameters;
            const uint32 availableMask = getAvailableCpuHardwareThreadMask();
            taskParameters.threadAffinityMask = availableMask & 0xFFFFFFFEu;
            if (parameters.maxTaskSystemWorkerThreads == 0)
            {
                taskParameters.threadAffinityMask = 0;
            }
            else
            {
                uint32 bitCount = ((availableMask >> 1) & 0x55555555u) + (availableMask & 0x55555554u);
                bitCount = ((bitCount >> 2) & 0x33333333u) + (bitCount & 0x33333333u);
                bitCount = ((bitCount >> 4) & 0x0F0F0F0Fu) + (bitCount & 0x0F0F0F0Fu);
                bitCount = ((bitCount >> 8) & 0x00FF00FFu) + (bitCount & 0x00FF00FFu);
                bitCount = (bitCount >> 16) + (bitCount & 0xFFFFu);
                for (uint32 i = 0; parameters.maxTaskSystemWorkerThreads < bitCount; ++i)
                {
                    taskParameters.threadAffinityMask &= ~(i * 2u);
                    bitCount = ((taskParameters.threadAffinityMask >> 1) & 0x55555555u) + (taskParameters.threadAffinityMask & 0x55555555u);
                    bitCount = ((bitCount >> 2) & 0x33333333u) + (bitCount & 0x33333333u);
                    bitCount = ((bitCount >> 4) & 0x0F0F0F0Fu) + (bitCount & 0x0F0F0F0Fu);
                    bitCount = ((bitCount >> 8) & 0x00FF00FFu) + (bitCount & 0x00FF00FFu);
                    bitCount = (bitCount >> 16) + (bitCount & 0xFFFFu);
                }
            }
            pGameFramework->pTaskSystem = task::createTaskSystem(pGameFramework->pDefaultAllocator, taskParameters);
        }

        // 0x00C0E640
        void destroyTaskSystem(GameFrameworkSystem* pGameFramework)
        {
            task::destroyTaskSystem(pGameFramework->pDefaultAllocator, pGameFramework->pTaskSystem);
            pGameFramework->pTaskSystem = nullptr;
        }

        // 0x00C0E670
        IoResult<uint32> mountHostPath(GameFrameworkSystem* pGameFramework, const char* pTargetPath, const char* pHostPath)
        {
            return file::mountNativeHostPath(pGameFramework->pFileSystem, pTargetPath, pHostPath, false);
        }

        // 0x00C0E690
        const char* getLocalUserName(const GameFrameworkSystem* pGameFramework)
        {
            return pGameFramework->userName;
        }

        // 0x00C0E6B0
        ApplicationProcessState getCurrentProcessState(const GameFrameworkSystem* pGameFramework)
        {
            return pGameFramework->currentProcessState;
        }

        // 0x00C10190
        void initialize(GameFrameworkSystem* pGameFramework, const GameFrameworkParameters& parameters)
        {
            pGameFramework->currentProcessState = ApplicationProcessState_Foreground;
            pGameFramework->renderingSuspended = false;
            // Magic number: 15 is past the 15 blocks (0-14), so there is no debug block.
            pGameFramework->pMemorySystem = memory::createMemorySystem(getMemoryLayoutDefinition(), 15u);
            if (pGameFramework->pMemorySystem == nullptr)
            {
                exit(1);
            }
            // Magic numbers: block 10 is GameFramework_Default; creation flags 7.
            const MemoryBlock defaultBlock = memory::lockMemoryBlock(pGameFramework->pMemorySystem, 10u);
            pGameFramework->defaultAllocator.create(defaultBlock, 7u, "GameFramework");
            pGameFramework->pDefaultAllocator = &pGameFramework->defaultAllocator;
            pGameFramework->pGraphicsSystem = nullptr;
            pGameFramework->pRenderEffectSystem = nullptr;
            pGameFramework->pFileSystem = nullptr;
            pGameFramework->useSound = false;
            pGameFramework->hideMouseCursorInFullscreen = false;
            pGameFramework->wasInFullscreen = false;
            pGameFramework->lastUpdateTime = SystemTimer::getCurrentMilliseconds();
            IniVariables::initialize();

            pGameFramework->useSteam = parameters.useSteam;
            if (!parameters.useSteam)
            {
                pGameFramework->pSteamworksSystem = nullptr;
            }
            else
            {
                SteamError steamError;
                pGameFramework->pSteamworksSystem = createSteamworksSystem(pGameFramework->pDefaultAllocator, &steamError, parameters.steamAppId);
                WCHAR message[4048];
                // Magic numbers: the first string table ids of the "Steam failed" and "not subscribed" messages.
                if (steamError == SteamError_FailedInitialization)
                {
                    loadFrameworkMessage(message, 0xFD0, 0x155u, getLanguageFamily(getSystemLanguage(pGameFramework)));
                    // OpenRFG deviation: the message box is replaced by a log line.
                    // MessageBoxW(nullptr, message, L"Steamworks", 0);
                    logFrameworkMessage("Steamworks", "Steam initialization failed", message);
                }
                else if (steamError == SteamError_NotSubscribedToApp)
                {
                    loadFrameworkMessage(message, 0xFD0, 0x83u, getLanguageFamily(getSystemLanguage(pGameFramework)));
                    // OpenRFG deviation: the message box is replaced by a log line.
                    // MessageBoxW(nullptr, message, L"Steamworks", 0);
                    logFrameworkMessage("Steamworks", "Not subscribed to the app", message);
                }
                if (steamError != SteamError_Ok)
                {
                    _exit(1);
                }
            }

            initializeBase(pGameFramework, parameters);
            debug::nativePrintArgs("%s(%s) - version %s (%s)\n", getBuildProjectName(), getBuildBranchNameString(), getBuildVersionString(), getBuildConfigurationString());
            createFileSystem(pGameFramework, parameters);
            createTaskSystem(pGameFramework, parameters);

            // The user name: -username, else %USERNAME%, else "NoName".
            if (parameters.pUserName != nullptr)
            {
                copyString(pGameFramework->userName, sizeof(pGameFramework->userName), parameters.pUserName);
            }
            else
            {
                size_t length = 0;
                if (getenv_s(&length, pGameFramework->userName, sizeof(pGameFramework->userName), "USERNAME") != 0 || length == 0)
                {
                    copyString(pGameFramework->userName, sizeof(pGameFramework->userName), "NoName");
                }
            }

            pGameFramework->pGraphicsSystem = initializeDisplay(pGameFramework, parameters);
            if (pGameFramework->pGraphicsSystem == nullptr && !parameters.headLess)
            {
                WCHAR caption[4048];
                WCHAR text[4050];
                // Magic numbers: the first string table ids of the "no graphics" caption and text.
                loadFrameworkMessage(caption, 0xFD0, 0xA1u, getLanguageFamily(getSystemLanguage(pGameFramework)));
                loadFrameworkMessage(text, 0xFD0, 0xBFu, getLanguageFamily(getSystemLanguage(pGameFramework)));
                // OpenRFG deviation: the message box is replaced by a log line.
                // MessageBoxW(nullptr, text, caption, 0);
                logFrameworkMessage("GameFramework", "Could not initialize the graphics system", text);
                task::destroyTaskSystem(pGameFramework->pDefaultAllocator, pGameFramework->pTaskSystem);
                pGameFramework->pTaskSystem = nullptr;
                destroyFileSystem(pGameFramework);
                memory::destroyMemorySystem(pGameFramework->pMemorySystem);
                exit(1);
            }
            // Magic number: room for 64 render effects.
            pGameFramework->pRenderEffectSystem = rendereffect::createRenderEffectSystem(0x40u, pGameFramework->pDefaultAllocator);

            if (!parameters.initializeSoundSystem)
            {
                pGameFramework->pSoundSystem = nullptr;
            }
            else
            {
                SoundSystem::InitializationParameters soundParameters;
                static_cast<SoundSystem::InitializationParametersBase&>(soundParameters) = parameters.soundSystemParameters;
                pGameFramework->pSoundSystem = SoundSystem::initialize(pGameFramework->pDefaultAllocator, soundParameters);
                pGameFramework->useSound = true;
            }

            UserAccountSystemCreationParameters userAccountParameters;
            userAccountParameters.pSteamworksSystem = pGameFramework->pSteamworksSystem;
            copyString(userAccountParameters.userNameWin32, sizeof(userAccountParameters.userNameWin32), pGameFramework->userName);
            pGameFramework->pUserAccountSystem = useraccount::createUserAccountSystem(pGameFramework->pDefaultAllocator, userAccountParameters);

            // Deviation (application.cpp deviation 4): rfg.exe only has Steam's save data system. The application chooses
            // the local one on request, so the failure check below is for Steam's alone.
            if (parameters.pSaveDataSystemParameters == nullptr)
            {
                pGameFramework->pSaveDataSystem = nullptr;
            }
            else
            {
                const bool steamSaveData = parameters.pSaveDataSystemParameters->saveDataSystemType == SaveData::SaveDataSystemType_Steam;
                if (parameters.useSteam && steamSaveData)
                {
                    // The Steam provider's parameters start with the Steamworks system.
                    *(SteamworksSystem**)parameters.pSaveDataSystemParameters->pProviderParameters = pGameFramework->pSteamworksSystem;
                }
                pGameFramework->pSaveDataSystem = SaveData::create(pGameFramework->pDefaultAllocator, *parameters.pSaveDataSystemParameters);
                if (steamSaveData && SaveData::hasSteamRemoteStorageFailed(pGameFramework->pSaveDataSystem))
                {
                    WCHAR text[4050];
                    // Magic number: the first string table id of the "Steam cloud failed" message.
                    loadFrameworkMessage(text, 0xFD0, 0xDDu, getLanguageFamily(getSystemLanguage(pGameFramework)));
                    // OpenRFG deviation: the message box is replaced by a log line.
                    // MessageBoxW(nullptr, text, L"Steamworks", 0);
                    logFrameworkMessage("Steamworks", "Steam remote storage failed", text);
                }
            }

            // Deviation (orig_main.md 2.5, OM4): winsock. Nothing on rfg.exe's LAN path starts it (curl's global init
            // only runs for a crash upload); with Steam the process has it started already (inferred: the Steam
            // client). Without it the LAN link's socket and its own address lookup fail, and
            // multi_core_platform_set_my_player_uid exits. Started here, where the system services are created.
            network::initializePlatform();
            // Deviation (stub_impl.md S2, OpenRFG with --no-steam): the system services are Steam's (stats,
            // achievements, rankings, presence); without Steam there are none, and their functions answer for a null
            // system at their first line. The pointer is set to null here: rfg.exe leaves it unwritten when there are
            // no parameters, and the framework system's constructor initializes nothing.
            if (parameters.pSystemServicesParameters != nullptr && parameters.useSteam)
            {
                unit4::SystemServicesParameters servicesParameters = *parameters.pSystemServicesParameters;
                servicesParameters.pSteamworksSystem = pGameFramework->pSteamworksSystem;
                pGameFramework->pSystemServices = unit4::systemservices::createSystemServices(pGameFramework->pDefaultAllocator, &servicesParameters, pGameFramework->pUserAccountSystem);
            }
            else
            {
                pGameFramework->pSystemServices = nullptr;
            }

            // One frame cleared to black (opaque).
            if (pGameFramework->pGraphicsSystem != nullptr)
            {
                GraphicsCommandBuffer* pCommandBuffer = graphics::beginFrame(pGameFramework->pGraphicsSystem);
                const RenderTarget* pBackBuffer = graphics::getBackBufferRenderTarget(pGameFramework->pGraphicsSystem);
                // Magic numbers: color 0xFF000000 (A=255), clear mask 2 (color only).
                const ClearSettings clearSettings(0xFF000000u, 0.0f, 0u, 2u);
                graphics::writeBeginRenderPassCommand(pCommandBuffer, pBackBuffer, (RenderTargetPartFlagMask)0, &clearSettings);
                graphics::writeEndRenderPassCommand(pCommandBuffer, (RenderTargetPartFlagMask)0, nullptr);
                graphics::endFrame(pGameFramework->pGraphicsSystem);
            }

            InputSystemCreationParameters inputParameters = parameters.inputSystemCreationParameters;
            inputParameters.pWindowHandle = pGameFramework->hwnd;
            pGameFramework->pInputSystem = input::createInputSystem(pGameFramework->pDefaultAllocator, inputParameters);
        }

        // 0x00C0F6C0
        // shutdownDisplay and destroyFileSystem are inlined.
        // Original behaviour kept: the system services are not destroyed.
        void shutdown(GameFrameworkSystem* pGameFramework)
        {
            if (pGameFramework->pSaveDataSystem != nullptr)
            {
                SaveData::destroy(pGameFramework->pSaveDataSystem);
            }
            if (pGameFramework->pUserAccountSystem != nullptr)
            {
                useraccount::destroyUserAccountSystem(pGameFramework->pUserAccountSystem, pGameFramework->pDefaultAllocator);
            }
            if (pGameFramework->useSound)
            {
                SoundSystem::shutdown(pGameFramework->pSoundSystem, pGameFramework->pDefaultAllocator);
            }
            shutdownDisplay(pGameFramework);
            rendereffect::destroyRenderEffectSystem(pGameFramework->pRenderEffectSystem);
            pGameFramework->pRenderEffectSystem = nullptr;
            task::destroyTaskSystem(pGameFramework->pDefaultAllocator, pGameFramework->pTaskSystem);
            pGameFramework->pTaskSystem = nullptr;
            unmountLinkedResources(pGameFramework->pFileSystem);
            file::unmount(pGameFramework->pFileSystem, pGameFramework->shaderMountHandle);
            file::destroyFileSystem(pGameFramework->pFileSystem);
            pGameFramework->pFileSystem = nullptr;
            input::destroyInputSystem(pGameFramework->pInputSystem, pGameFramework->pDefaultAllocator);
            if (pGameFramework->pSteamworksSystem != nullptr)
            {
                destroySteamworksSystem(pGameFramework->pDefaultAllocator, pGameFramework->pSteamworksSystem);
                pGameFramework->pSteamworksSystem = nullptr;
            }
            IniVariables::shutdown();
            pGameFramework->pDefaultAllocator = nullptr;
            pGameFramework->defaultAllocator.destroy();
            memory::unlockMemoryBlock(pGameFramework->pMemorySystem, 10u);
            memory::destroyMemorySystem(pGameFramework->pMemorySystem);
            pGameFramework->pMemorySystem = nullptr;
        }

        // 0x00C0F820
        // Sleeps until s_minFrameTime has passed since the last update; in the foreground the frame number advances
        // and the frame's duration is the input's time step (0 in the background). Then the window messages (updateBase),
        // the input events (processInputEvent inlined), the sound and the user accounts.
        // Original behaviour kept: updateBase gets the last frame duration even in the background.
        void update(GameFrameworkSystem* pGameFramework)
        {
            uint32 currentTimeInMs = SystemTimer::getCurrentMilliseconds();
            uint32 elapsedMs = currentTimeInMs - pGameFramework->lastUpdateTime;
            while (elapsedMs < s_minFrameTime)
            {
                sleepCurrentThreadMilliseconds(s_minFrameTime - elapsedMs);
                currentTimeInMs = SystemTimer::getCurrentMilliseconds();
                elapsedMs = currentTimeInMs - pGameFramework->lastUpdateTime;
            }
            float timeStep;
            if (pGameFramework->currentProcessState == ApplicationProcessState_Foreground)
            {
                pGameFramework->lastUpdateTime = currentTimeInMs;
                advanceFrameNumber(currentTimeInMs);
                timeStep = getLastFrameDurationInSeconds();
            }
            else
            {
                timeStep = 0.0f;
            }
            updateBase(pGameFramework, getLastFrameDurationInSeconds());
            if (pGameFramework->currentProcessState == ApplicationProcessState_Foreground)
            {
                input::updateInputSystem(pGameFramework->pInputSystem, timeStep);
            }
            GameApplication* pApplication = &getGameApplication();
            InputEvent inputEvent;
            while (input::fetchNextInputEvent(&inputEvent, pGameFramework->pInputSystem))
            {
                processInputEvent(pGameFramework, pApplication, inputEvent);
            }
            if (pGameFramework->useSound && pGameFramework->currentProcessState == ApplicationProcessState_Foreground)
            {
                SoundSystem::update(pGameFramework->pSoundSystem, timeStep);
            }
            if (pGameFramework->pUserAccountSystem != nullptr)
            {
                useraccount::updateUserAccountSystem(pGameFramework->pUserAccountSystem);
            }
        }
    }
}
