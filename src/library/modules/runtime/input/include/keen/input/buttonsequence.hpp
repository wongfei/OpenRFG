// Original: library/modules/runtime/input/include/keen/input/buttonsequence.hpp
// Implementation: library/modules/runtime/input/sources/global/buttonsequence.cpp
//
// ButtonSequence, detects a timed sequence of up to four button presses (cheat/debug combos).
#pragma once

#include "keen/base/types.hpp"
#include "keen/input/inputevents.hpp"

namespace keen
{
    // size 0x44
    class ButtonSequence
    {
    public:
        enum
        {
            MaxElementCount = 4,
        };

        // size 0x10
        struct Element
        {
            bool isButton() const; // 0x00C74330 (.h:26)
            bool isAxis() const; // 0x00C74350 (.h:27)

            uint32 index; // 0x00
            ControllerType type; // 0x04
            float threshold; // 0x08
            bool isDown; // 0x0C
        };

        ButtonSequence(); // 0x00C74370 (:6)
        bool addButtonElement(ControllerType type, uint32 buttonIndex); // 0x00C74380 (:11)
        bool addAxisElement(ControllerType type, uint32 axisIndex, float threshold); // 0x00C743C0 (:27)
        bool isMatching() const; // 0x00C74420 (:86)
        void processEvent(const InputEvent& inputEvent); // 0x00C7A090 (:44)
        void reset(); // 0x00C74400 (:78)

    private:
        Element m_elements[4]; // 0x00
        uint32 m_elementCount; // 0x40
    };
}
