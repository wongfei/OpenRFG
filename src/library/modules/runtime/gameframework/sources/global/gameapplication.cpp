// Original: library/modules/runtime/gameframework/sources/global/gameapplication.cpp
//
// GameApplication's base members (the game overrides the rest) and GameApplicationParameters, whose constructor is
// inline in gameapplication.hpp.

#include "keen/gameframework/gameapplication.hpp"

namespace keen
{
    // 0x00C0DEC0 (gameapplication.hpp:18, inline)
    GameApplicationParameters::GameApplicationParameters(int inArgc, char** inArgv)
    {
        argc = inArgc;
        argv = inArgv;
    }

    // 0x00C0DEF0
    GameApplication::GameApplication()
    {
        m_isShutdownRequested = false;
    }

    // 0x00C0DF30
    GameApplication::~GameApplication()
    {
    }

    // 0x00C0DF00 (gameapplication.hpp, inline)
    void GameApplication::shutdownRequested(GameFrameworkSystem*)
    {
    }

    // 0x00C0DF10 (gameapplication.hpp, inline)
    bool GameApplication::isReadyToShutdown(GameFrameworkSystem*)
    {
        return true;
    }

    // 0x00C0DF20 (gameapplication.hpp, inline)
    bool GameApplication::processInputEvent(const InputEvent&)
    {
        return false;
    }

    bool GameApplication::isShutdownRequested() const
    {
        return m_isShutdownRequested;
    }
}
