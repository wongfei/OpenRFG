// Original: library/modules/runtime/input/include/keen/input/controllermapping.hpp
// Implementation: library/modules/runtime/input/sources/global/controllermapping.cpp
//
// Controller mapping: GameInputMapper maps game inputs (InputDescriptor: type, controller input, axis function) to
// controller buttons and axes per local player (ControllerMap, mapping entries, button id mappings). Placement of the
// mapping records inferred.
#pragma once

#include "keen/base/types.hpp"
#include "keen/base/array.hpp"
#include "keen/base/queue.hpp"
#include "keen/base/sizedarray.hpp"
#include "keen/input/inputevents.hpp"

namespace keen
{
    struct ControllerMap;
    struct InputDescriptor;
    class MemoryAllocator;

    enum InputType
    {
        InputType_Binary = 0,
        InputType_1dAnalogValue = 1,
        InputType_2dAnalogValue = 2,
    };

    enum ControllerInput
    {
        ControllerInput_Axis = 0,
        ControllerInput_Button = 1,
        ControllerInput_ButtonToAxis = 2,
        ControllerInput_MouseToAxis = 3,
    };

    enum AxisFunction
    {
        AxisFunction_None = 0,
        AxisFunction_Flip = 1,
    };

    // size 0x40
    class GameInputMapper
    {
    public:
        // size 0x8
        struct Input
        {
            uint32 index; // 0x00
            InputType type; // 0x04
        };

        // size 0x10
        struct AxisData
        {
            uint8 controllerId; // 0x00
            uint8 controllerClass; // 0x01
            uint8 localPlayerId; // 0x02
            float2 value; // 0x04
            uint16 buttonMask; // 0x0C
            bool dirty; // 0x0E
        };

        void create(MemoryAllocator* pAllocator, uint32 numCreatedInputEvents, uint32 numInputs, const InputDescriptor* pInputDescription, uint32 inputDescriptionSize, uint32 numInputMaps); // 0x00C7DB50 (:7)
        void destroy(MemoryAllocator* pAllocator); // 0x00C7DCB0 (:48)
        void registerControllerMap(const ControllerMap* pMap); // 0x00C76900 (:56)
        void unregisterControllerMap(const ControllerMap* pMap); // 0x00C7A1B0 (:68)
        bool processInputEvent(const InputEvent& inputEvent); // 0x00C7A210 (:80)
        bool fetchNextInputEvent(InputEvent* pEvent); // 0x00C7A8A0 (:338)

    private:
        SizedArray<ControllerMap const*> m_mappings; // 0x00
        Array<Input> m_inputMap; // 0x0C
        Queue<InputEvent> m_cookedEvents; // 0x14
        Array<AxisData> m_axisAssembler; // 0x28
        uint32 m_nextEvent; // 0x30
        float2 m_lastMousePosition; // 0x34
        bool m_lastMousePositionValid; // 0x3C
    };

    // size 0x8
    struct InputDescriptor
    {
        uint32 input; // 0x00
        InputType type; // 0x04
    };

    // size 0x8
    struct ButtonIdMapping
    {
        uint32 virtualKey; // 0x00
        uint32 index; // 0x04
    };

    // size 0x18
    struct ControllerMappingEntry
    {
        uint32 resultingGameInput; // 0x00
        ControllerInput inputType; // 0x04
        uint32 controllerButtonIndexA; // 0x08
        uint32 controllerButtonIndexB; // 0x0C
        uint32 controllerButtonIndexC; // 0x10
        uint32 controllerButtonIndexD; // 0x14
    };

    // size 0x10
    struct ControllerMap
    {
        void set(ControllerType, uint8, ControllerMappingEntry*, uint32);

        ControllerType type; // 0x00
        uint8 subType; // 0x04
        ControllerMappingEntry* pEntries; // 0x08
        uint32 numEntries; // 0x0C
    };

    // size 0x1
    struct MappedAxisButtonState
    {
        bool isPressed; // 0x00
    };
}
