// Original: library/modules/runtime/gameframework/sources/global/gameframework_platform.hpp (private)
//
// Per-platform hooks implemented in sources/win32/gameframework_win32.cpp. Contents inferred from where
// each function is defined.
#pragma once

#include "keen/gameframework/gameframework.hpp"

namespace keen
{
    namespace GameFramework
    {
        void parsePlatformArguments(GameFrameworkParameters* pParameters, int argc, char** argv); // 0x00C0EF90
        void initializeBase(GameFrameworkSystem* pGameFramework, const GameFrameworkParameters& parameters); // 0x00C0EFE0
        void shutdownBase(GameFrameworkSystem* pGameFramework); // 0x00C0F300
        GraphicsSystem* initializeDisplay(GameFrameworkSystem* pGameFramework, const GameFrameworkParameters& parameters); // 0x00C0F310
        void shutdownDisplay(GameFrameworkSystem* pGameFramework); // 0x00C0F420
        void updateBase(GameFrameworkSystem* pGameFramework, float timeStep); // 0x00C0F470

        namespace Internal
        {
            void renderPlatform(GameFrameworkSystem*, GraphicsCommandWriter*); // 0x00C0F500
            void lateInitializePlatform(GameFrameworkSystem*); // 0x00C0F510
            bool getLocalAccountName(char* pName, uint32 capacity); // 0x00C0F530
        }
    }
}
