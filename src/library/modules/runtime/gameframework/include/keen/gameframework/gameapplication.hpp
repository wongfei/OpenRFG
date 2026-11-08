// Original: library/modules/runtime/gameframework/include/keen/gameframework/gameapplication.hpp
// Implementation: library/modules/runtime/gameframework/sources/global/gameapplication.cpp
#pragma once

#include "keen/base/types.hpp"

namespace keen
{
    struct GameFrameworkParameters;
    struct GameFrameworkSystem;
    class GraphicsCommandWriter;
    struct InputEvent;

    // size 0x8
    struct GameApplicationParameters
    {
        GameApplicationParameters(int inArgc, char** inArgv); // 0x00C0DEC0

        int argc; // 0x00
        char** argv; // 0x04
    };

    // Interface the game implements; GameFramework::main drives it:
    //   fillGameframeworkParameters -> initialize -> loop { updateTime, render } -> shutdownRequested/isReadyToShutdown -> shutdown.
    // size 0x8
    class GameApplication
    {
    public:
        GameApplication(); // 0x00C0DEF0
        virtual ~GameApplication() = 0; // vtbl+0x00 0x00C0DF30
        virtual void fillGameframeworkParameters(GameFrameworkParameters* pParameters, const GameApplicationParameters& appParameters) = 0; // vtbl+0x04
        virtual void initialize(GameFrameworkSystem* pGameFramework, const GameApplicationParameters& parameters) = 0; // vtbl+0x08
        virtual void shutdownRequested(GameFrameworkSystem* pGameFramework); // vtbl+0x0C 0x00C0DF00
        virtual bool isReadyToShutdown(GameFrameworkSystem* pGameFramework); // vtbl+0x10 0x00C0DF10
        virtual void shutdown(GameFrameworkSystem* pGameFramework) = 0; // vtbl+0x14
        virtual void updateTime(float timeStep) = 0; // vtbl+0x18
        virtual void render(GraphicsCommandWriter* pGraphicsCommandWriter) const = 0; // vtbl+0x1C
        virtual bool processInputEvent(const InputEvent&); // vtbl+0x20 0x00C0DF20
        bool isShutdownRequested() const;

        // 0x00 vfptr
    private:
        bool m_isShutdownRequested; // 0x04
    };

    // Implemented by the game (game/sources/application.cpp): returns the application singleton.
    GameApplication& getGameApplication(); // 0x0045A850
}
