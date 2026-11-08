// Original: library/modules/runtime/input/sources/steam/controllersteam.cpp
//
// Steam controllers (stub_impl.md S2): the game's actions (action sets, digital buttons, analog axes) resolved to
// Steam action handles once, then per controller its connection, buttons, axes, motion data (gyro) and rumble
// through ISteamController. Without Steam (no ISteamController) no controller is registered.

#include "library/modules/runtime/input/sources/steam/controllersteam.hpp"
#include "keen/base/trace.hpp"
#include "library/modules/thirdparty/emgiec/emgiec_steam_142/sdk/public/steam/steam_api_internal.h"

#include <string.h>

namespace keen
{
    static ISteamController* s_pSteamController;
    static uint64 s_actionSetHandles[ControllerSteam::MaxNumActionSets];
    static uint64 s_actionHandles[ControllerSteam::MaxNumActions];
    static ControllerSteamAnalogAction s_axisValueHandles[ControllerSteam::MaxNumAnalogActions];

    // Magic number: Steam's motion data are signed 16-bit fixed point (1/32768).
    static const float SteamMotionScale = 3.0517578e-05f;
    // Magic number: 2π (6.28319) for the angular velocity.
    static const float TwoPi = 6.28319f;

    // 0x00C746D0 (controllersteam.hpp:16, inline)
    ControllerSteam::ControllerSteam()
    {
        m_steamControllerHandle = 0;
    }

    // 0x00C793A0 (:22): the action sets in order, the buttons and axes at their indices
    void ControllerSteam::registerHandles(const SteamControllerAction* pActions, uint32 actionCount)
    {
        debug::nativePrintArgs("[input:SteamController] reading handles.\n");
        s_pSteamController = SteamController();
        if (s_pSteamController == nullptr)
        {
            debug::nativePrintArgs("[input:SteamController] failed to acquire ISteamController object.\n");
            return;
        }
        s_pSteamController->Init();
        uint64* pSetHandle = s_actionSetHandles;
        for (uint32 i = 0; i < actionCount; ++i)
        {
            const SteamControllerAction& action = pActions[i];
            if (action.type == SteamControllerActionType_Set)
            {
                *pSetHandle = s_pSteamController->GetActionSetHandle(action.pName);
                debug::nativePrintArgs("[input:SteamController] SetHandle %s : %llu\n", action.pName, *pSetHandle);
                ++pSetHandle;
            }
            else if (action.type == SteamControllerActionType_Button)
            {
                s_actionHandles[action.buttonIndex] = s_pSteamController->GetDigitalActionHandle(action.pName);
                debug::nativePrintArgs("[input:SteamController] ActionHandle %s : %llu\n", action.pName, s_actionHandles[action.buttonIndex]);
            }
            else if (action.type == SteamControllerActionType_Axis)
            {
                s_axisValueHandles[action.buttonIndex].handle = s_pSteamController->GetAnalogActionHandle(action.pName);
                s_axisValueHandles[action.buttonIndex].mode = action.axisMode;
                debug::nativePrintArgs("[input:SteamController] ActionHandle %s : %llu\n", action.pName, s_axisValueHandles[action.buttonIndex].handle);
            }
        }
    }

    // 0x00C79500 (:60)
    uint32 ControllerSteam::getRegisteredControllers(uint64* pHandles, uint32 handleCapacity)
    {
        if (s_pSteamController == nullptr)
        {
            return 0;
        }
        s_pSteamController->RunFrame();
        ControllerHandle_t handles[STEAM_CONTROLLER_MAX_COUNT];
        const uint32 count = (uint32)s_pSteamController->GetConnectedControllers(handles);
        debug::nativePrintArgs("[input:SteamController] %d controllers connected\n", count);
        if (count < handleCapacity)
        {
            handleCapacity = count;
        }
        memcpy(pHandles, handles, handleCapacity * sizeof(uint64));
        return handleCapacity;
    }

    // 0x00C76170 (:81): the first action set active, no rumble, no gyro origin
    void ControllerSteam::create(uint64 steamControllerHandle)
    {
        m_steamControllerHandle = steamControllerHandle;
        s_pSteamController->ActivateActionSet(steamControllerHandle, s_actionSetHandles[0]);
        m_setIndex = 0;
        m_leftMotorIntensity = 0.0f;
        m_rightMotorIntensity = 0.0f;
        m_orientationOrigin.x = 0.0f;
        m_orientationOrigin.y = 0.0f;
        m_orientationOrigin.z = 0.0f;
        m_orientationOrigin.w = 0.0f;
    }

    // 0x00C761D0 (:95)
    void ControllerSteam::setActionSet(uint32 setIndex)
    {
        if (setIndex < MaxNumActionSets && s_actionSetHandles[setIndex] != 0)
        {
            s_pSteamController->ActivateActionSet(m_steamControllerHandle, s_actionSetHandles[setIndex]);
            m_setIndex = setIndex;
        }
    }

    // 0x00C76220 (:104)
    uint32 ControllerSteam::getActionSet()
    {
        return m_setIndex;
    }

    // 0x00C76230 (:109)
    void ControllerSteam::activateActionSetLayer(uint32 setIndex)
    {
        s_pSteamController->ActivateActionSetLayer(m_steamControllerHandle, s_actionSetHandles[setIndex]);
    }

    // 0x00C76270 (:114)
    void ControllerSteam::deactivateActionSetLayer(uint32 setIndex)
    {
        s_pSteamController->DeactivateActionSetLayer(m_steamControllerHandle, s_actionSetHandles[setIndex]);
    }

    // 0x00C762B0 (:119)
    void ControllerSteam::clearActionSetLayers()
    {
        s_pSteamController->DeactivateAllActionSetLayers(m_steamControllerHandle);
    }

    // 0x00C795A0 (:124): the generic pad state. Steam's motion axes are converted to keen's (y and z swapped), the
    // orientation is made relative to the gyro origin, and the rumble is sent with every update.
    void ControllerSteam::fillInGenericPadInput(input::GenericPadInput* pInput)
    {
        if (s_pSteamController != nullptr)
        {
            s_pSteamController->RunFrame();
        }
        ISteamController* pSteamController = s_pSteamController;
        memset(pInput, 0, sizeof(*pInput));
        if (m_steamControllerHandle == 0)
        {
            pInput->isConnected = false;
            return;
        }
        ControllerHandle_t handles[STEAM_CONTROLLER_MAX_COUNT];
        const int count = pSteamController->GetConnectedControllers(handles);
        bool isConnected = false;
        for (int i = 0; i < count; ++i)
        {
            if (handles[i] == m_steamControllerHandle)
            {
                isConnected = true;
            }
        }
        pInput->isConnected = isConnected;
        if (!isConnected)
        {
            return;
        }
        for (uint32 i = 0; i < MaxNumActions; ++i)
        {
            if (s_actionHandles[i] != 0 && s_pSteamController->GetDigitalActionData(m_steamControllerHandle, s_actionHandles[i]).bState)
            {
                pInput->buttons.setBit(i);
            }
        }
        for (uint32 i = 0; i < MaxNumAnalogActions; ++i)
        {
            if (s_axisValueHandles[i].handle == 0)
            {
                continue;
            }
            const ControllerAnalogActionData_t data = s_pSteamController->GetAnalogActionData(m_steamControllerHandle, s_axisValueHandles[i].handle);
            const SteamControllerAxisMode mode = s_axisValueHandles[i].mode;
            if (mode == SteamControllerAxisMode_XOnly || data.eMode == k_EControllerSourceMode_Trigger)
            {
                pInput->axis[i] = data.x;
            }
            else if (mode == SteamControllerAxisMode_YOnly)
            {
                pInput->axis[i] = data.y;
            }
            else if (mode == SteamControllerAxisMode_XAndY)
            {
                pInput->axis[i] = data.x;
                pInput->axis[i + 1] = data.y;
            }
        }
        const ControllerMotionData_t motion = s_pSteamController->GetMotionData(m_steamControllerHandle);
        // Magic numbers: the acceleration's range is ±2 g, the angular velocity's ±2000 degrees per second (4 turns).
        pInput->acceleration.x = motion.posAccelX * SteamMotionScale * 2.0f;
        pInput->acceleration.y = motion.posAccelZ * SteamMotionScale * 2.0f;
        pInput->acceleration.z = -motion.posAccelY * SteamMotionScale * 2.0f;
        pInput->angularVelocity.x = motion.rotVelX * SteamMotionScale * TwoPi * 4.0f;
        pInput->angularVelocity.y = motion.rotVelZ * SteamMotionScale * TwoPi * 4.0f;
        pInput->angularVelocity.z = motion.rotVelY * SteamMotionScale * TwoPi * 4.0f;
        pInput->orientation.x = motion.rotQuatX * SteamMotionScale;
        pInput->orientation.y = motion.rotQuatZ * SteamMotionScale;
        pInput->orientation.z = -motion.rotQuatY * SteamMotionScale;
        pInput->orientation.w = motion.rotQuatW * SteamMotionScale;
        m_lastOrientation = pInput->orientation;
        // orientation * conjugate(origin)
        const float ox = -m_orientationOrigin.x;
        const float oy = -m_orientationOrigin.y;
        const float oz = -m_orientationOrigin.z;
        const float ow = m_orientationOrigin.w;
        const float px = pInput->orientation.x;
        const float py = pInput->orientation.y;
        const float pz = pInput->orientation.z;
        const float pw = pInput->orientation.w;
        pInput->orientation.x = (ow * px + ox * pw + oz * py) - oy * pz;
        pInput->orientation.y = (oy * pw + ow * py + ox * pz) - oz * px;
        pInput->orientation.z = (oz * pw + ow * pz + oy * px) - ox * py;
        pInput->orientation.w = ((ow * pw - ox * px) - oy * py) - oz * pz;
        // Magic number: 65535, the full motor speed.
        pSteamController->TriggerVibration(m_steamControllerHandle, (uint16)(int)((double)m_leftMotorIntensity * 65535.0), (uint16)(int)((double)m_rightMotorIntensity * 65535.0));
    }

    // 0x00C746E0 (controllersteam.hpp:24)
    bool ControllerSteam::isController(uint64 handle)
    {
        return handle == m_steamControllerHandle;
    }

    // 0x00C79B00 (:241): at most bufferLength of the action's origins in the set
    void ControllerSteam::getActionOrigins(const SteamControllerAction& action, uint32 setIndex, SteamControllerActionOrigin* pOriginsOut, uint32 bufferLength, uint32* pCountOut) const
    {
        EControllerActionOrigin origins[STEAM_CONTROLLER_MAX_ORIGINS];
        uint32 count = 0;
        if (action.type == SteamControllerActionType_Button)
        {
            count = (uint32)s_pSteamController->GetDigitalActionOrigins(m_steamControllerHandle, s_actionSetHandles[setIndex], s_actionHandles[action.buttonIndex], origins);
        }
        else if (action.type == SteamControllerActionType_Axis)
        {
            count = (uint32)s_pSteamController->GetAnalogActionOrigins(m_steamControllerHandle, s_actionSetHandles[setIndex], s_axisValueHandles[action.buttonIndex].handle, origins);
        }
        if (count > bufferLength)
        {
            count = bufferLength;
        }
        for (uint32 i = 0; i < count; ++i)
        {
            pOriginsOut[i] = (SteamControllerActionOrigin)origins[i];
        }
        *pCountOut = count;
    }

    // 0x00C79BE0 (:273): in the active set
    void ControllerSteam::getActionOrigins(const SteamControllerAction& action, SteamControllerActionOrigin* pOriginsOut, uint32 bufferLength, uint32* pCountOut) const
    {
        getActionOrigins(action, m_setIndex, pOriginsOut, bufferLength, pCountOut);
    }

    // 0x00C762D0 (:278)
    bool ControllerSteam::showBindingPanel() const
    {
        return s_pSteamController->ShowBindingPanel(m_steamControllerHandle);
    }

    // 0x00C762F0 (:283)
    const char* ControllerSteam::getSteamControllerActionOriginText(SteamControllerActionOrigin origin)
    {
        return s_pSteamController->GetStringForActionOrigin((EControllerActionOrigin)origin);
    }

    // 0x00C76310 (:288): sent by the next update
    void ControllerSteam::setRumble(float mainActuatorValue, float subActuatorValue)
    {
        m_leftMotorIntensity = mainActuatorValue;
        m_rightMotorIntensity = subActuatorValue;
    }

    // 0x00C76330 (:294): the current orientation becomes the origin
    void ControllerSteam::resetGyroOrientationOrigin()
    {
        m_orientationOrigin = m_lastOrientation;
    }
}
