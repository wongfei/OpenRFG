// Original: library/modules/runtime/gameframework/sources/global/gameframeworkmain.cpp
//
// GameFramework::main, the process's frame loop (called from WinMain), and the per-frame driver of the application:
// Internal::initialize (arguments, the application's parameters, the framework, the application), update (the time
// step), render (one graphics frame), and the shutdown handshake.

#include "keen/base/framenumber.hpp"
#include "keen/base/string.hpp"
#include "keen/base/stringscanner.hpp"
#include "keen/base/thread.hpp"
#include "keen/base/trace.hpp"
#include "keen/gameframework/gameapplication.hpp"
#include "keen/graphics/graphicssystem.hpp"
#include "game/build/win32/steam/master/generated_sources/version.hpp"
#include "library/modules/runtime/gameframework/sources/global/gameframework_platform.hpp"
#include "library/modules/runtime/gameframework/sources/global/gameframeworkinternal.hpp"
#include "library/modules/runtime/gameframework/sources/global/gameframeworksystem.hpp"

namespace keen
{
    // Debug time controls; nothing in rfg.exe sets them (only Internal::update reads them).
    static bool s_pauseGameFramework; // 0x038B6BD4
    static bool s_stopMotionEnabled; // 0x038B6BD5
    static bool s_ultraSlowMotionEnabled; // 0x038B6BD6
    static bool s_slowMotionEnabled; // 0x038B6BD7

    // 0x00C0E6C0 (:27)
    // -username NAME and +connect_lobby STEAMID (the Steam boot invite).
    // Original behaviour kept: an option at the last position is ignored; after -username the second check looks at
    // its value; the lobby id is read in place, so argv's entry is left pointing past the digits.
    void parseArguments(GameFrameworkParameters* pParameters, int argc, char** pArgv)
    {
        for (int i = 0; i < argc; ++i)
        {
            if (isStringEqual(pArgv[i], "-username") && i + 1 < argc)
            {
                ++i;
                pParameters->pUserName = pArgv[i];
            }
            if (isStringEqual(pArgv[i], "+connect_lobby") && i + 1 < argc)
            {
                uint64 lobbyId;
                if (readSignLessUint64FromDecimalString(&lobbyId, (const char**)&pArgv[i + 1]) == StringReadResult_Ok)
                {
                    pParameters->bootInviteLobbySteamId64 = lobbyId;
                }
                else
                {
                    debug::nativePrintArgs("[gameframework] Failed to parse target lobby id for boot invite from args.\n");
                }
                ++i;
            }
        }
    }

    namespace GameFramework
    {
        namespace Internal
        {
            // 0x00C10B50
            void initialize(int argc, char** argv, GameFrameworkSystem* pGameFramework, const char* pProjectName, const char* pGamebuildPath)
            {
                pGameFramework->isShutdownRequested = false;
                GameFrameworkParameters parameters(pProjectName, pGamebuildPath);
                parsePlatformArguments(&parameters, argc, argv);
                parseArguments(&parameters, argc, argv);
                const GameApplicationParameters applicationParameters(argc, argv);
                GameApplication& application = getGameApplication();
                if (isStringEmpty(parameters.pProjectGameBuildDirectory))
                {
                    parameters.pProjectGameBuildDirectory = getGamebuildDirectoryString();
                }
                application.fillGameframeworkParameters(&parameters, applicationParameters);
                GameFramework::initialize(pGameFramework, parameters);
                application.initialize(pGameFramework, applicationParameters);
            }

            // 0x00C0E780
            // The application's time step: 0 when paused, 1/100 in ultra slow motion, 1/5 in slow motion, 1/30 s with a
            // 300 ms sleep in stop motion.
            void update(GameFrameworkSystem*)
            {
                GameApplication& application = getGameApplication();
                float timeStep = getLastFrameDurationInSeconds();
                if (s_pauseGameFramework)
                {
                    application.updateTime(0.0f);
                    return;
                }
                if (s_ultraSlowMotionEnabled)
                {
                    application.updateTime(timeStep * 0.01f);
                    return;
                }
                if (s_slowMotionEnabled)
                {
                    application.updateTime(timeStep * 0.2f);
                    return;
                }
                if (s_stopMotionEnabled)
                {
                    timeStep = 1.0f / 30.0f;
                    sleepCurrentThreadMilliseconds(300u);
                }
                application.updateTime(timeStep);
            }

            // 0x00C0FAC0
            // GameFramework::beginFrame/endFrame inlined; with rendering suspended or no command buffer it sleeps 15 ms.
            void render(GameFrameworkSystem* pGameFramework)
            {
                if (pGameFramework->renderingSuspended)
                {
                    sleepCurrentThreadMilliseconds(15u);
                    return;
                }
                GameApplication& application = getGameApplication();
                GraphicsCommandBuffer* pTarget = graphics::beginFrame(pGameFramework->pGraphicsSystem);
                if (pTarget != nullptr)
                {
                    GraphicsCommandWriter* pWriter = &pGameFramework->commandWriter;
                    pWriter->beginWriting(pTarget, pGameFramework->pGraphicsSystem);
                    application.render(pWriter);
                    pWriter->endWriting();
                    graphics::endFrame(pGameFramework->pGraphicsSystem);
                    return;
                }
                sleepCurrentThreadMilliseconds(15u);
            }

            // 0x00C0FB50
            // The framework keeps updating (without the application's frame) until the application is ready.
            void updateShutdownRequested(GameFrameworkSystem* pGameFramework)
            {
                GameApplication& application = getGameApplication();
                application.shutdownRequested(pGameFramework);
                while (!application.isReadyToShutdown(pGameFramework))
                {
                    GameFramework::update(pGameFramework);
                    update(pGameFramework);
                }
            }

            // 0x00C0FBA0
            void shutdown(GameFrameworkSystem* pGameFramework)
            {
                getGameApplication().shutdown(pGameFramework);
                GameFramework::shutdown(pGameFramework);
            }
        }

        // 0x00C10D40
        // The framework state lives on this stack frame for the lifetime of the process. The application's frame runs
        // only while the process is not in the background; the framework's update runs every iteration (window
        // messages, input).
        int main(int argc, char** argv)
        {
            GameFrameworkSystem gameFramework;
            Internal::initialize(argc, argv, &gameFramework, "default_game", "");
            update(&gameFramework);
            while (!gameFramework.quit)
            {
                if (gameFramework.currentProcessState != ApplicationProcessState_Background)
                {
                    Internal::update(&gameFramework);
                    Internal::render(&gameFramework);
                }
                update(&gameFramework);
            }
            Internal::updateShutdownRequested(&gameFramework);
            getGameApplication().shutdown(&gameFramework);
            shutdown(&gameFramework);
            debug::nativePrintArgs("[gameframework] ---- main() ended ----.\n ");
            return 0;
        }
    }
}
