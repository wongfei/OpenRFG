// Original: library/modules/runtime/input/sources/global/inputsystem.cpp
//
// The platform independent input system: the event queue the platform part fills (createPlatformEvents, the window
// messages) and GameFramework::update empties, the stored events (buttons held and axes off centre, so that a
// disconnect or a focus change can release them), the controller table (class, type, local player) with the
// auto-catch of a controller for a local player, and the generic pad events made from the difference of two pad
// states.
//
// Local players are LocalPlayerIdStructureType* handles whose value is the player's index (0xFF: none); an event and
// a controller info keep the index in a byte.
//
// Partial (orig_main.md OM2): what the framework and the win32 platform part reach; stub_impl.md S2: the rumble and the
// gyro origin of the pads. Not ported: touch, the raw axis and gyro event helpers.

#include "keen/input/inputsystem.hpp"
#include "keen/base/memoryallocator.hpp"
#include "keen/base/trace.hpp"
#include "keen/input/win32/inputsystem_win32.hpp"
#include "library/modules/runtime/input/sources/global/inputsystem_private.hpp"

#include <new>
#include <stdlib.h>
#include <string.h>

namespace keen
{
    static InputSystem* s_pInputSystemInstance; // 0x038BD0B0

    static uint8 toPlayerIndex(const LocalPlayerIdStructureType* playerId)
    {
        return (uint8)(uintptr_t)playerId;
    }

    // 0x00C7B0F0
    InputSystem::InputSystem()
    {
    }

    // 0x00C7DB40
    InputSystem::~InputSystem()
    {
    }

    namespace input
    {
        // 0x00C74530 (inputsystem_private.hpp:42)
        void ButtonBitfield::setBit(uint64 bit)
        {
            if (bit < 64u)
            {
                low |= (uint64)1u << bit;
            }
            else
            {
                high |= (uint64)1u << (bit - 64u);
            }
        }

        // 0x00C745C0 (inputsystem_private.hpp:48)
        bool ButtonBitfield::getBitSet(uint64 bit) const
        {
            if (bit < 64u)
            {
                return (low & ((uint64)1u << bit)) != 0;
            }
            return (high & ((uint64)1u << (bit - 64u))) != 0;
        }

        // 0x00C76A20 (:48)
        void GenericPadInput::clear()
        {
            memset(this, 0, sizeof(GenericPadInput));
        }

        // 0x00C7ACF0 (:858)
        // Appends an event to the queue (dropped when the queue is full or the controller id is out of range). The
        // event gets its controller's class and local player; an event of a controller no player owns is the auto-catch
        // candidate: when it matches the auto-catch type the controller goes to the auto-catch player (with every
        // keyboard and mouse when it is a keyboard or a mouse) and the auto-catch ends. A connect or type change never
        // catches. Events without a controller (0xFF) get no class and no player.
        // Magic numbers: |axis| above 0.2 catches with the "anything including connected" type; the start button is
        // StandardButtonId_StartPause.
        void addInputEvent(InputSystem* pInputSystem, const InputEvent& inputEvent)
        {
            if (inputEvent.controllerId > 0x10u && inputEvent.controllerId != 0xFFu)
            {
                debug::nativePrintArgs("Illegal controllerId sent to addInputEvent!");
                return;
            }
            if (pInputSystem->eventQueue.isFull())
            {
                return;
            }
            InputEvent event = inputEvent;
            if (event.controllerId == 0xFFu)
            {
                event.controllerClass = (uint8)ControllerClass_None;
                event.localPlayerId = 0xFFu;
            }
            else
            {
                ControllerInfo& info = pInputSystem->controllerInfos[event.controllerId];
                const uint8 controllerClass = (uint8)info.controllerClass;
                event.controllerClass = controllerClass;
                uint8 playerId = info.localPlayerId;
                const InputSystemControllerAutoCatchType autoCatchType = pInputSystem->autoCatchType;
                if (playerId == 0xFFu && autoCatchType != InputSystemControllerAutoCatchType_Disabled &&
                    (controllerClass == ControllerClass_Controller || controllerClass == ControllerClass_Keyboard || controllerClass == ControllerClass_Mouse) &&
                    event.type != InputEventType_ControllerDisconnected && event.type != InputEventType_ControllerTypeChanged)
                {
                    bool caught;
                    if (autoCatchType == InputSystemControllerAutoCatchType_StartButtonOnly && event.type == InputEventType_RawButtonDown &&
                        event.data.button.standardButton == StandardButtonId_StartPause)
                    {
                        caught = true;
                    }
                    else if (autoCatchType == InputSystemControllerAutoCatchType_AnyButton && event.type == InputEventType_RawButtonDown)
                    {
                        caught = true;
                    }
                    else if (autoCatchType == InputSystemControllerAutoCatchType_Anything && event.type != InputEventType_ControllerConnected)
                    {
                        caught = true;
                    }
                    else if (autoCatchType == InputSystemControllerAutoCatchType_AnythingIncludingControllerConnected)
                    {
                        if (event.type == InputEventType_RawAxis)
                        {
                            // |value| as the original computes it: value - (-value) >= 0 picks value.
                            const float value = event.data.axis.value;
                            const float negated = -value;
                            const float magnitude = (float)((double)value - (double)negated) >= 0.0f ? value : negated;
                            caught = (double)magnitude > 0.2;
                        }
                        else
                        {
                            caught = true;
                        }
                    }
                    else
                    {
                        caught = autoCatchType == InputSystemControllerAutoCatchType_ControllerConnected && event.type == InputEventType_ControllerConnected;
                    }
                    if (caught)
                    {
                        playerId = toPlayerIndex(pInputSystem->autoCatchPlayerId);
                        info.localPlayerId = playerId;
                        pInputSystem->autoCatchPlayerId = (const LocalPlayerIdStructureType*)0xFF;
                        pInputSystem->autoCatchType = InputSystemControllerAutoCatchType_Disabled;
                        if (controllerClass == ControllerClass_Keyboard || controllerClass == ControllerClass_Mouse)
                        {
                            for (uint32 i = 0; i < 16u; ++i)
                            {
                                ControllerInfo& other = pInputSystem->controllerInfos[i];
                                if (other.controllerClass == ControllerClass_Keyboard || other.controllerClass == ControllerClass_Mouse)
                                {
                                    other.localPlayerId = playerId;
                                }
                            }
                        }
                    }
                }
                event.localPlayerId = playerId;
            }
            pInputSystem->eventQueue.push(event);
        }

        // 0x00C76A40 (:833)
        // A button down or an axis off centre is remembered (once per button; an axis keeps its latest values).
        // Original behaviour kept: nothing is stored when the stored events are full; any other matching event type
        // exits the process.
        void storeEvent(InputSystem* pInputSystem, const InputEvent& inputEvent)
        {
            SizedArray<InputEvent>& storedEvents = pInputSystem->storedEvents;
            if (storedEvents.isFull())
            {
                return;
            }
            for (InputEvent* pStored = storedEvents.getStart(); pStored != storedEvents.getEnd(); ++pStored)
            {
                if (pStored->type != inputEvent.type || pStored->controllerId != inputEvent.controllerId)
                {
                    continue;
                }
                if (inputEvent.type == InputEventType_RawButtonDown)
                {
                    if (pStored->data.button.buttonIndex == inputEvent.data.button.buttonIndex)
                    {
                        return;
                    }
                }
                else
                {
                    if (inputEvent.type != InputEventType_RawAxis)
                    {
                        exit(1);
                    }
                    if (pStored->data.axis.axisIndex == inputEvent.data.axis.axisIndex)
                    {
                        pStored->data.axis.value = inputEvent.data.axis.value;
                        pStored->data.axis.previousValue = inputEvent.data.axis.previousValue;
                        return;
                    }
                }
            }
            storedEvents.pushBack(inputEvent);
        }

        // 0x00C7AEF0 (:806)
        // A button up removes its stored button down, an axis back at the centre its stored axis (the first match).
        // Original behaviour kept: any other event type exits the process once a stored event of the controller is
        // found.
        void removeEvent(InputSystem* pInputSystem, const InputEvent& inputEvent)
        {
            SizedArray<InputEvent>& storedEvents = pInputSystem->storedEvents;
            for (InputEvent* pStored = storedEvents.getStart(); pStored != storedEvents.getEnd(); ++pStored)
            {
                if ((pStored->type == InputEventType_RawButtonDown && inputEvent.type != InputEventType_RawButtonUp) ||
                    (pStored->type == InputEventType_RawAxis && inputEvent.type != InputEventType_RawAxis) ||
                    pStored->controllerId != inputEvent.controllerId)
                {
                    continue;
                }
                if (inputEvent.type == InputEventType_RawButtonUp)
                {
                    if (pStored->data.button.buttonIndex != inputEvent.data.button.buttonIndex)
                    {
                        continue;
                    }
                }
                else
                {
                    if (inputEvent.type != InputEventType_RawAxis)
                    {
                        exit(1);
                    }
                    if (pStored->data.axis.axisIndex != inputEvent.data.axis.axisIndex)
                    {
                        continue;
                    }
                }
                storedEvents.erase(pStored);
                return;
            }
        }

        // 0x00C7AFC0 (:879)
        // Releases what a controller (0xFF: every controller) holds: each stored button down becomes a button up, each
        // stored axis goes back to 0, and the stored events are removed.
        // Original behaviour kept: the controller's player is looked up first, so 0xFF reads past the controller table
        // (addFocusLostEvent passes it; the reset then depends on that byte); a controller without a player releases
        // nothing.
        void sendResetEvents(InputSystem* pInputSystem, uint32 controllerId)
        {
            if (pInputSystem->controllerInfos[controllerId].localPlayerId == 0xFFu)
            {
                return;
            }
            SizedArray<InputEvent>& storedEvents = pInputSystem->storedEvents;
            InputEvent* pStored = storedEvents.getStart();
            while (pStored != storedEvents.getEnd())
            {
                InputEvent event = *pStored;
                if (controllerId != 0xFFu && event.controllerId != controllerId)
                {
                    ++pStored;
                    continue;
                }
                if (event.type == InputEventType_RawButtonDown)
                {
                    event.type = InputEventType_RawButtonUp;
                }
                else if (event.type == InputEventType_RawAxis)
                {
                    event.data.axis.previousValue = event.data.axis.value;
                    event.data.axis.value = 0.0f;
                }
                else
                {
                    exit(1);
                }
                addInputEvent(pInputSystem, event);
                pStored = storedEvents.erase(pStored);
            }
        }

        // 0x00C7AC30 (:87)
        void initializeGestureHelper(InputSystem* pInputSystem, uint32 controllerId, MemoryAllocator* pAllocator)
        {
            pInputSystem->gestureHelper.initialize(controllerId, pAllocator);
        }

        // 0x00C7AC90 (:95)
        void shutdownGestureHelper(InputSystem* pInputSystem, MemoryAllocator* pAllocator)
        {
            pInputSystem->gestureHelper.shutdown(pAllocator);
        }

        // 0x00C7E0B0 (:103)
        // The events between two states of a pad: connect (and a type change), every changed button (down events are
        // stored, up events remove them), every changed axis (stored while off centre), the gyro values; then the
        // state is remembered. A pad that went away is disconnected (its held buttons released).
        // Original behaviour kept: the type change event carries the old type and features.
        void generateGenericPadEvents(InputSystem* pInputSystem, GenericPadInput* pLastState, const GenericPadInput& currentState, uint32 controllerId)
        {
            if (!currentState.isConnected)
            {
                if (pLastState->isConnected)
                {
                    addControllerDisconnectedEvent(pInputSystem, controllerId);
                    memset(pLastState, 0, sizeof(GenericPadInput));
                }
                return;
            }
            ControllerInfo& info = pInputSystem->controllerInfos[controllerId];
            if (info.type == ControllerType_Count)
            {
                addControllerConnectedEvent(pInputSystem, controllerId);
            }
            ControllerInfo currentInfo;
            fillPlatformControllerInfo(&currentInfo, &pInputSystem->controllerState, controllerId);
            if (info.type != currentInfo.type || info.features != currentInfo.features)
            {
                InputEvent event;
                event.controllerId = (uint8)controllerId;
                event.type = InputEventType_ControllerTypeChanged;
                event.data.connected.controllerType = info.type;
                event.data.connected.features = info.features;
                addInputEvent(pInputSystem, event);
                info.type = currentInfo.type;
                info.features = currentInfo.features;
            }
            for (uint32 button = 0; button < 0x80u; ++button)
            {
                const bool isDown = currentState.buttons.getBitSet(button);
                const bool wasDown = pLastState->buttons.getBitSet(button);
                if (isDown == wasDown)
                {
                    continue;
                }
                InputEvent event;
                event.controllerId = (uint8)controllerId;
                event.data.button.buttonIndex = button;
                event.data.button.standardButton = getStandardButtonIdFromButtonIndex(pInputSystem, info.type, button);
                if (isDown)
                {
                    event.type = InputEventType_RawButtonDown;
                    storeEvent(pInputSystem, event);
                }
                else
                {
                    event.type = InputEventType_RawButtonUp;
                    removeEvent(pInputSystem, event);
                }
                addInputEvent(pInputSystem, event);
            }
            for (uint32 axis = 0; axis < 0x30u; ++axis)
            {
                const float value = currentState.axis[axis];
                if (value == pLastState->axis[axis])
                {
                    continue;
                }
                InputEvent event;
                event.controllerId = (uint8)controllerId;
                event.type = InputEventType_RawAxis;
                event.data.axis.axisIndex = axis;
                event.data.axis.value = value;
                event.data.axis.previousValue = pLastState->axis[axis];
                // Magic number: an axis within 1e-20 (relative, at least absolute) of 0 is at the centre.
                const float negated = -value;
                const float magnitude = (value - negated) >= 0.0f ? value : negated;
                float epsilon = magnitude * 1e-20f;
                if (epsilon - 1e-20f < 0.0f)
                {
                    epsilon = 1e-20f;
                }
                if (magnitude <= epsilon)
                {
                    removeEvent(pInputSystem, event);
                }
                else
                {
                    storeEvent(pInputSystem, event);
                }
                addInputEvent(pInputSystem, event);
            }
            if (currentState.acceleration.x != pLastState->acceleration.x || currentState.acceleration.y != pLastState->acceleration.y ||
                currentState.acceleration.z != pLastState->acceleration.z)
            {
                InputEvent event;
                event.controllerId = (uint8)controllerId;
                event.type = InputEventType_GyroAcceleration;
                event.data.gyro.orientation.x = currentState.acceleration.x;
                event.data.gyro.orientation.y = currentState.acceleration.y;
                event.data.gyro.orientation.z = currentState.acceleration.z;
                event.data.gyro.orientation.w = 0.0f;
                addInputEvent(pInputSystem, event);
            }
            if (currentState.angularVelocity.x != pLastState->angularVelocity.x || currentState.angularVelocity.y != pLastState->angularVelocity.y ||
                currentState.angularVelocity.z != pLastState->angularVelocity.z)
            {
                InputEvent event;
                event.controllerId = (uint8)controllerId;
                event.type = InputEventType_GyroAngularVelocity;
                event.data.gyro.orientation.x = currentState.angularVelocity.x;
                event.data.gyro.orientation.y = currentState.angularVelocity.y;
                event.data.gyro.orientation.z = currentState.angularVelocity.z;
                event.data.gyro.orientation.w = 0.0f;
                addInputEvent(pInputSystem, event);
            }
            if (currentState.orientation.x != pLastState->orientation.x || currentState.orientation.y != pLastState->orientation.y ||
                currentState.orientation.z != pLastState->orientation.z || currentState.orientation.w != pLastState->orientation.w)
            {
                InputEvent event;
                event.controllerId = (uint8)controllerId;
                event.type = InputEventType_GyroOrientation;
                event.data.gyro.orientation = currentState.orientation;
                addInputEvent(pInputSystem, event);
            }
            *pLastState = currentState;
        }

        // 0x00C7F140 (:464)
        // Every controller starts disconnected without a player; no auto-catch.
        // Magic numbers: 16 stored events.
        InputSystem* createInputSystem(MemoryAllocator* pAllocator, const InputSystemCreationParameters& parameters)
        {
            void* pMemory = pAllocator->allocate(sizeof(InputSystem), 8u, 0u, nullptr);
            InputSystem* pInputSystem = pMemory != nullptr ? new (pMemory) InputSystem() : nullptr;
            pInputSystem->eventQueue.create(pAllocator, parameters.maxEventCount, 0x10u);
            pInputSystem->storedEvents.create(pAllocator, 0x10u, 0x10u);
            for (uint32 i = 0; i < 16u; ++i)
            {
                pInputSystem->controllerInfos[i].type = ControllerType_Count;
                pInputSystem->controllerInfos[i].localPlayerId = 0xFFu;
            }
            pInputSystem->autoCatchType = InputSystemControllerAutoCatchType_Disabled;
            pInputSystem->autoCatchPlayerId = (const LocalPlayerIdStructureType*)0xFF;
            createPlatformControllerState(&pInputSystem->controllerState, pInputSystem, pAllocator, parameters);
            memset(pInputSystem->mappedAxisButtonStates, 0, sizeof(pInputSystem->mappedAxisButtonStates));
            s_pInputSystemInstance = pInputSystem;
            return pInputSystem;
        }

        // 0x00C7F270 (:525)
        // Every connected controller is disconnected (its held buttons released), the window procedure restored.
        // Original behaviour kept: the platform state (DirectInput devices and configs) is not released.
        void destroyInputSystem(InputSystem* pInputSystem, MemoryAllocator* pAllocator)
        {
            for (uint32 controllerId = 0; controllerId < 16u; ++controllerId)
            {
                if (pInputSystem->controllerInfos[controllerId].type != ControllerType_Count)
                {
                    addControllerDisconnectedEvent(pInputSystem, controllerId);
                }
            }
            unregisterWindowProc(pInputSystem);
            pInputSystem->storedEvents.destroy(pAllocator);
            pInputSystem->eventQueue.destroy(pAllocator);
            pAllocator->free(pInputSystem);
        }

        // 0x00C7B190 (:554)
        // The queued events of the controller change player too.
        void setControllerPlayerId(InputSystem* pInputSystem, uint32 controllerId, const LocalPlayerIdStructureType* playerId)
        {
            if (controllerId < 16u)
            {
                pInputSystem->controllerInfos[controllerId].localPlayerId = toPlayerIndex(playerId);
            }
            for (uint32 i = 0; i < pInputSystem->eventQueue.getSize(); ++i)
            {
                InputEvent& event = pInputSystem->eventQueue[i];
                if (event.controllerId == (uint8)controllerId)
                {
                    event.localPlayerId = toPlayerIndex(playerId);
                }
            }
        }

        // 0x00C7B1E0 (:573)
        void setControllerPlayerId(InputSystem* pInputSystem, const InputEvent& event, const LocalPlayerIdStructureType* playerId)
        {
            setControllerPlayerId(pInputSystem, event.controllerId, playerId);
            const_cast<InputEvent&>(event).localPlayerId = toPlayerIndex(playerId);
        }

        // 0x00C74700 (:580)
        void clearControllerPlayerId(InputSystem* pInputSystem, const LocalPlayerIdStructureType* playerId)
        {
            for (uint32 i = 0; i < 16u; ++i)
            {
                if (pInputSystem->controllerInfos[i].localPlayerId == toPlayerIndex(playerId))
                {
                    pInputSystem->controllerInfos[i].localPlayerId = 0xFFu;
                }
            }
        }

        // 0x00C74720 (:591)
        // The next matching event of an unowned controller goes to the player (addInputEvent), unless the player
        // already has a controller, keyboard or mouse. No player (0xFF) ends the auto-catch.
        void setControllerAutoCatch(InputSystem* pInputSystem, const LocalPlayerIdStructureType* playerId, InputSystemControllerAutoCatchType type)
        {
            if (toPlayerIndex(playerId) == 0xFFu)
            {
                pInputSystem->autoCatchPlayerId = playerId;
                pInputSystem->autoCatchType = InputSystemControllerAutoCatchType_Disabled;
                return;
            }
            for (uint32 i = 0; i < 16u; ++i)
            {
                const ControllerInfo& info = pInputSystem->controllerInfos[i];
                if ((info.controllerClass == ControllerClass_Controller || info.controllerClass == ControllerClass_Keyboard ||
                     info.controllerClass == ControllerClass_Mouse) &&
                    info.localPlayerId == toPlayerIndex(playerId))
                {
                    return;
                }
            }
            pInputSystem->autoCatchType = type;
            pInputSystem->autoCatchPlayerId = playerId;
        }

        // 0x00C74790 (:617)
        InputSystemControllerAutoCatchType getControllerAutoCatch(InputSystem* pInputSystem, const LocalPlayerIdStructureType* playerId)
        {
            if (pInputSystem->autoCatchPlayerId == playerId)
            {
                return pInputSystem->autoCatchType;
            }
            return InputSystemControllerAutoCatchType_Disabled;
        }

        // 0x00C7B210 (:627)
        void addRawButtonDownEvent(InputSystem* pInputSystem, uint32 controllerId, uint32 buttonIndex)
        {
            InputEvent event;
            event.controllerId = (uint8)controllerId;
            event.type = InputEventType_RawButtonDown;
            event.data.button.buttonIndex = buttonIndex;
            event.data.button.standardButton = getStandardButtonIdFromButtonIndex(pInputSystem, pInputSystem->controllerInfos[controllerId].type, buttonIndex);
            storeEvent(pInputSystem, event);
            addInputEvent(pInputSystem, event);
        }

        // 0x00C7B260 (:639)
        void addRawButtonUpEvent(InputSystem* pInputSystem, uint32 controllerId, uint32 buttonIndex)
        {
            InputEvent event;
            event.controllerId = (uint8)controllerId;
            event.type = InputEventType_RawButtonUp;
            event.data.button.buttonIndex = buttonIndex;
            event.data.button.standardButton = getStandardButtonIdFromButtonIndex(pInputSystem, pInputSystem->controllerInfos[controllerId].type, buttonIndex);
            removeEvent(pInputSystem, event);
            addInputEvent(pInputSystem, event);
        }

        // 0x00C7B390 (:674)
        // Focus lost (hasFocus false) or restored (true); everything held is released first.
        void addFocusLostEvent(InputSystem* pInputSystem, bool hasFocus)
        {
            InputEvent event;
            event.controllerId = 0xFFu;
            event.type = (uint8)(InputEventType_FocusLost + (hasFocus ? 1 : 0));
            sendResetEvents(pInputSystem, 0xFFu);
            addInputEvent(pInputSystem, event);
        }

        // 0x00C7B3D0 (:685)
        void addKeyEvent(InputSystem* pInputSystem, uint32 controllerId, uint32 keyCode)
        {
            InputEvent event;
            event.controllerId = (uint8)controllerId;
            event.type = InputEventType_Key;
            event.data.key.keyCode = keyCode;
            addInputEvent(pInputSystem, event);
        }

        // 0x00C7B400 (:695)
        void addMouseMoveEvent(InputSystem* pInputSystem, uint32 controllerId, float2 position)
        {
            InputEvent event;
            event.controllerId = (uint8)controllerId;
            event.type = InputEventType_MouseMove;
            event.data.mouse.position = position;
            addInputEvent(pInputSystem, event);
        }

        // 0x00C7B430 (:705)
        void addMouseRelativeMovementEvent(InputSystem* pInputSystem, uint32 controllerId, float2 relativeMovement)
        {
            InputEvent event;
            event.controllerId = (uint8)controllerId;
            event.type = InputEventType_MouseRelativeMovement;
            event.data.mouse.position = relativeMovement;
            addInputEvent(pInputSystem, event);
        }

        // 0x00C7B460 (:715)
        void addMouseWheelEvent(InputSystem* pInputSystem, uint32 controllerId, int wheelData)
        {
            InputEvent event;
            event.controllerId = (uint8)controllerId;
            event.type = InputEventType_MouseWheel;
            event.data.wheel.wheelDelta = wheelData;
            addInputEvent(pInputSystem, event);
        }

        // 0x00C7B580 (:758)
        // The controller's info is filled from the platform; keyboards and mice lose their player (they are caught
        // again by the auto-catch).
        void addControllerConnectedEvent(InputSystem* pInputSystem, uint32 controllerId)
        {
            InputEvent event;
            event.controllerId = (uint8)controllerId;
            event.type = InputEventType_ControllerConnected;
            ControllerInfo& info = pInputSystem->controllerInfos[controllerId];
            fillPlatformControllerInfo(&info, &pInputSystem->controllerState, controllerId);
            event.data.connected.controllerType = info.type;
            event.data.connected.features = info.features;
            if (info.type == ControllerType_Keyboard || info.type == ControllerType_Mouse)
            {
                setControllerPlayerId(pInputSystem, controllerId & 0xFFu, (const LocalPlayerIdStructureType*)0xFF);
                event.localPlayerId = 0xFFu;
            }
            addInputEvent(pInputSystem, event);
        }

        // 0x00C7B610 (:780)
        void addControllerDisconnectedEvent(InputSystem* pInputSystem, uint32 controllerId)
        {
            ControllerInfo& info = pInputSystem->controllerInfos[controllerId];
            InputEvent event;
            event.controllerId = (uint8)controllerId;
            event.type = InputEventType_ControllerDisconnected;
            event.data.connected.controllerType = info.type;
            event.data.connected.features = info.features;
            sendResetEvents(pInputSystem, controllerId);
            info.type = ControllerType_Count;
            addInputEvent(pInputSystem, event);
        }

        // 0x00C7B7E0 (:935)
        bool fetchNextInputEvent(InputEvent* pEvent, InputSystem* pInputSystem)
        {
            if (pInputSystem->eventQueue.isEmpty())
            {
                return false;
            }
            const InputEvent& event = pInputSystem->eventQueue.pop();
            if (pEvent != nullptr)
            {
                *pEvent = event;
            }
            return true;
        }

        // 0x00C7F220 (:965)
        void updateInputSystem(InputSystem* pInputSystem, float timeStep)
        {
            createPlatformEvents(pInputSystem, &pInputSystem->controllerState);
            pInputSystem->gestureHelper.update(pInputSystem, timeStep);
        }

        // 0x00C747B0 (:1002)
        // Original bug kept: id 16 passes the check (one past the table).
        bool isControllerConnected(const InputSystem* pInputSystem, uint32 controllerId)
        {
            if (controllerId > 0x10u)
            {
                return false;
            }
            return pInputSystem->controllerInfos[controllerId].type != ControllerType_Count;
        }

        // 0x00C76BB0 (:1058)
        // Magic numbers: keen's keyboard ids 0x10 (Enter), 0x12 (Escape), 0x18-0x1B (the arrow keys); mouse buttons
        // 0 (left) and 2 (right).
        StandardButtonId getStandardButtonIdFromButtonIndex(const InputSystem* pInputSystem, ControllerType controllerType, uint32 buttonIndex)
        {
            if (controllerType == ControllerType_Keyboard)
            {
                switch (buttonIndex)
                {
                case 0x10u:
                    return StandardButtonId_Confirm;
                case 0x12u:
                    return StandardButtonId_Exit;
                case 0x18u:
                    return StandardButtonId_Left;
                case 0x19u:
                    return StandardButtonId_Up;
                case 0x1Au:
                    return StandardButtonId_Right;
                case 0x1Bu:
                    return StandardButtonId_Down;
                default:
                    return StandardButtonId_Unknown;
                }
            }
            if (controllerType == ControllerType_Mouse)
            {
                if (buttonIndex == 0)
                {
                    return StandardButtonId_Confirm;
                }
                return buttonIndex == 2u ? StandardButtonId_Exit : StandardButtonId_Unknown;
            }
            return getPlatformStandardButtonIdFromButtonIndex(pInputSystem, controllerType, buttonIndex);
        }

        // 0x00C76C50 (:1102)
        void getControllerInfo(ControllerInfo* pResult, uint32 controllerId)
        {
            InputSystem* pInputSystem = s_pInputSystemInstance;
            fillPlatformControllerInfo(pResult, &pInputSystem->controllerState, controllerId);
            if (controllerId < 16u)
            {
                pResult->localPlayerId = pInputSystem->controllerInfos[controllerId].localPlayerId;
                return;
            }
            pResult->localPlayerId = 0xFFu;
        }

        // 0x00C76C90 (:1117)
        // Original behaviour kept: the local player is not filled in.
        void getControllerInfo(ControllerInfo* pResult, const InputEvent& inputEvent)
        {
            fillPlatformControllerInfo(pResult, &s_pInputSystemInstance->controllerState, inputEvent.controllerId);
        }

        // 0x00C7B860 (:1097): Steam controllers (ids 14 and up) only
        void resetGyroOrientationOrigin(InputSystem* pInputSystem, uint32 controllerId)
        {
            // Magic number: 14, the first Steam controller id. Original behaviour kept: the check admits one id past
            // the registered controllers.
            if (controllerId > 13u && controllerId <= pInputSystem->controllerState.steamControllerCount + 14u)
            {
                pInputSystem->controllerState.steamController[controllerId - 14u].resetGyroOrientationOrigin();
            }
        }

        // 0x00C74840 (:1124)
        uint32 getControllerIdForPlayer(const LocalPlayerIdStructureType* playerId, ControllerClass controllerClass)
        {
            for (uint32 i = 0; i < 16u; ++i)
            {
                const ControllerInfo& info = s_pInputSystemInstance->controllerInfos[i];
                if (info.localPlayerId == toPlayerIndex(playerId) && info.controllerClass == controllerClass)
                {
                    return i;
                }
            }
            return 0xFFFFFFFFu;
        }

        // 0x00C74870 (:1137)
        const LocalPlayerIdStructureType* getPlayerIdForController(uint32 controllerId)
        {
            if (controllerId < 16u)
            {
                return (const LocalPlayerIdStructureType*)(uintptr_t)s_pInputSystemInstance->controllerInfos[controllerId].localPlayerId;
            }
            return (const LocalPlayerIdStructureType*)0xFF;
        }

        // 0x00C7E5D0 (:1163): the XInput pads (ids 2-5) and the Steam controllers (ids 14 and up)
        void setRumble(InputSystem* pInputSystem, uint32 controllerId, float mainActuatorValue, float subActuatorValue)
        {
            if (controllerId - 2u < 4u)
            {
                pInputSystem->controllerState.controllers[controllerId - 2u].setRumble(controllerId - 2u, mainActuatorValue, subActuatorValue);
                return;
            }
            // Original behaviour kept: as resetGyroOrientationOrigin.
            if (controllerId > 13u && controllerId <= pInputSystem->controllerState.steamControllerCount + 14u)
            {
                pInputSystem->controllerState.steamController[controllerId - 14u].setRumble(mainActuatorValue, subActuatorValue);
            }
        }

        // 0x00C7B8D0 (:1179)
        bool containsInputEvent(InputSystem* pInputSystem, InputEventType eventType, uint32 controllerId, uint32 buttonIndex)
        {
            for (uint32 i = 0; i < pInputSystem->eventQueue.getSize(); ++i)
            {
                const InputEvent& event = pInputSystem->eventQueue[i];
                if (event.type == (uint8)eventType && event.controllerId == (uint8)controllerId && event.data.button.buttonIndex == buttonIndex)
                {
                    return true;
                }
            }
            return false;
        }

        // 0x00C7B930 (:1194)
        // The first queued matching event gets the new button index.
        void replaceInputEvent(InputSystem* pInputSystem, InputEventType eventType, uint32 controllerId, uint32 buttonIndex, uint32 newButtonIndex)
        {
            for (uint32 i = 0; i < pInputSystem->eventQueue.getSize(); ++i)
            {
                InputEvent& event = pInputSystem->eventQueue[i];
                if (event.type == (uint8)eventType && event.controllerId == (uint8)controllerId && event.data.button.buttonIndex == buttonIndex)
                {
                    event.data.button.buttonIndex = newButtonIndex;
                    return;
                }
            }
        }

        // 0x00C748A0 (:1210)
        PlatformControllerState* getPlatformControllerState(InputSystem* pInputSystem)
        {
            return &pInputSystem->controllerState;
        }

        // 0x00C748B0 (:1217)
        // The player's device class: a controller wins over a keyboard, a keyboard over a mouse or touch.
        ControllerClass getPrimaryInputDeviceClass(InputSystem* pInputSystem, const LocalPlayerIdStructureType* playerId)
        {
            ControllerClass primary = ControllerClass_None;
            for (uint32 i = 0; i < 16u; ++i)
            {
                const ControllerInfo& info = pInputSystem->controllerInfos[i];
                const ControllerClass controllerClass = info.controllerClass;
                if ((int)controllerClass >= ControllerClass_Count || controllerClass == ControllerClass_None || info.localPlayerId != toPlayerIndex(playerId))
                {
                    continue;
                }
                if (primary == ControllerClass_Keyboard)
                {
                    if (controllerClass != ControllerClass_Keyboard && controllerClass != ControllerClass_Controller)
                    {
                        continue;
                    }
                }
                else if (primary == ControllerClass_Controller)
                {
                    if (controllerClass != ControllerClass_Controller)
                    {
                        continue;
                    }
                }
                primary = controllerClass;
            }
            return primary;
        }

        // 0x00C74980 (:1240)
        ControllerType getPrimaryInputDeviceType(InputSystem* pInputSystem, const LocalPlayerIdStructureType* playerId, ControllerClass controllerClass)
        {
            for (uint32 i = 0; i < 16u; ++i)
            {
                const ControllerInfo& info = pInputSystem->controllerInfos[i];
                if (info.controllerClass == controllerClass && info.localPlayerId == toPlayerIndex(playerId))
                {
                    return info.type;
                }
            }
            return ControllerType_Unkown;
        }
    }
}
