// Original: library/modules/runtime/input/sources/steam/controllersteam.hpp
// Implementation: library/modules/runtime/input/sources/steam/controllersteam.cpp
//
// ControllerSteam, a controller driven through the Steam controller API (action sets, digital and analog actions,
// origins). Placement of ControllerSteamAnalogAction inferred.
#pragma once

#include "keen/base/types.hpp"
#include "keen/input/inputsystem.hpp"

namespace keen
{
    // size 0x38
    class ControllerSteam
    {
    public:
        enum Max
        {
            MaxNumActionSets = 10,
            MaxNumActions = 128,
            MaxNumAnalogActions = 48,
        };

        static void registerHandles(const SteamControllerAction* pActions, uint32 actionCount); // 0x00C793A0 (:22)
        static uint32 getRegisteredControllers(uint64* pHandles, uint32 handleCapacity); // 0x00C79500 (:60)
        ControllerSteam(); // 0x00C746D0 (.h:16)
        void create(uint64 steamControllerHandle); // 0x00C76170 (:81)
        void fillInGenericPadInput(input::GenericPadInput* pInput); // 0x00C795A0 (:124)
        void setActionSet(uint32 setIndex); // 0x00C761D0 (:95)
        uint32 getActionSet(); // 0x00C76220 (:104)
        void activateActionSetLayer(uint32 setIndex); // 0x00C76230 (:109)
        void deactivateActionSetLayer(uint32 setIndex); // 0x00C76270 (:114)
        void clearActionSetLayers(); // 0x00C762B0 (:119)
        bool isController(uint64 handle); // 0x00C746E0 (.h:24)
        void getActionOrigins(const SteamControllerAction& action, uint32 setIndex, SteamControllerActionOrigin* pOriginsOut, uint32 bufferLength, uint32* pCountOut) const; // 0x00C79B00 (:241)
        void getActionOrigins(const SteamControllerAction& action, SteamControllerActionOrigin* pOriginsOut, uint32 bufferLength, uint32* pCountOut) const; // 0x00C79BE0 (:273)
        bool showBindingPanel() const; // 0x00C762D0 (:278)
        void setRumble(float mainActuatorValue, float subActuatorValue); // 0x00C76310 (:288)
        void resetGyroOrientationOrigin(); // 0x00C76330 (:294)
        static const char* getSteamControllerActionOriginText(SteamControllerActionOrigin origin); // 0x00C762F0 (:283)

    private:
        uint64 m_steamControllerHandle; // 0x00
        uint32 m_setIndex; // 0x08
        float m_leftMotorIntensity; // 0x0C
        float m_rightMotorIntensity; // 0x10
        float4 m_orientationOrigin; // 0x14
        float4 m_lastOrientation; // 0x24
    };

    // size 0x10
    struct ControllerSteamAnalogAction
    {
        uint64 handle; // 0x00
        SteamControllerAxisMode mode; // 0x08
    };
}
