// Original: library/modules/runtime/gameframework/sources/global/gameframeworkinternal.hpp (private)
//
// Functions shared between gameframework.cpp and gameframeworkmain.cpp. The exact split between this
// header and gameframework_platform.hpp is inferred from where each function is defined.
#pragma once

#include "keen/gameframework/gameframework.hpp"

namespace keen
{
    class GameApplication;
    struct InputEvent;

    // gameframeworkmain.cpp:27
    void parseArguments(GameFrameworkParameters* pParameters, int argc, char** pArgv); // 0x00C0E6C0

    namespace GameFramework
    {
        // Lifecycle of the framework services (gameframework.cpp).
        void initialize(GameFrameworkSystem* pGameFramework, const GameFrameworkParameters& parameters); // 0x00C10190
        void shutdown(GameFrameworkSystem* pGameFramework); // 0x00C0F6C0
        void update(GameFrameworkSystem* pGameFramework); // 0x00C0F820
        void render(GameFrameworkSystem* pGameFramework, GraphicsCommandWriter* pCommandWriter); // 0x00C0E380
        GraphicsCommandWriter* beginFrame(GameFrameworkSystem* pGameFramework); // 0x00C0E390
        void endFrame(GameFrameworkSystem* pGameFramework, GameApplication* pApplication); // 0x00C0E3D0
        void processInputEvent(GameFrameworkSystem* pGameFramework, GameApplication* pApplication, const InputEvent& inputEvent); // 0x00C0E410
        void createFileSystem(GameFrameworkSystem* pGameFramework, const GameFrameworkParameters& parameters); // 0x00C0F9F0
        void destroyFileSystem(GameFrameworkSystem* pGameFramework); // 0x00C0E4D0
        void createTaskSystem(GameFrameworkSystem* pGameFramework, const GameFrameworkParameters& parameters); // 0x00C0E510
        void destroyTaskSystem(GameFrameworkSystem* pGameFramework); // 0x00C0E640

        namespace Internal
        {
            // gameframework.cpp
            bool getShowTextConsole(); // 0x00C0E2F0
            bool isRenderingSuspended(GameFrameworkSystem* pGameFramework); // 0x00C0E6A0

            // gameframeworkmain.cpp: the per-frame driver used by GameFramework::main.
            void initialize(int argc, char** argv, GameFrameworkSystem* pGameFramework, const char* pProjectName, const char* pGamebuildPath); // 0x00C10B50
            void update(GameFrameworkSystem* pGameFramework); // 0x00C0E780
            void render(GameFrameworkSystem* pGameFramework); // 0x00C0FAC0
            void updateShutdownRequested(GameFrameworkSystem* pGameFramework); // 0x00C0FB50
            void shutdown(GameFrameworkSystem* pGameFramework); // 0x00C0FBA0
        }
    }
}
