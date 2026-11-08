// Original: library/modules/runtime/input/include/keen/input/gesturehelper.hpp
// Implementation: library/modules/runtime/input/sources/global/gesturehelper.cpp
//
// GestureHelper, recognizes touch/mouse drag gestures from a ring buffer of moves.
#pragma once

#include "keen/base/types.hpp"
#include "keen/base/ringbuffer.hpp"

namespace keen
{
    struct InputSystem;
    class MemoryAllocator;
    struct TouchEventData;

    // size 0x3C
    class GestureHelper
    {
    public:
        enum
        {
            InvalidControllerId = -1,
            MaxHistoryLength = 2,
        };

        // size 0xC
        struct MoveData
        {
            float2 distance; // 0x00
            float time; // 0x08
        };

        GestureHelper(); // 0x00C7AB60 (:11)
        ~GestureHelper(); // 0x00C7ABA0 (:24)
        void initialize(uint32 controllerId, MemoryAllocator* pAllocator); // 0x00C7ABB0 (:29)
        void shutdown(MemoryAllocator* pAllocator); // 0x00C7ABF0 (:39)
        void update(InputSystem* pInputSystem, float timeStep); // 0x00C7EEF0 (:180)
    private:
        void startFrame(float timeStep); // 0x00C74610 (:52)
        void handleTouchBegin(InputSystem* pInputSystem, const TouchEventData& touch); // 0x00C7DD30 (:89)
        void handleTouchMove(InputSystem* pInputSystem, const TouchEventData& touch); // 0x00C76940 (:107)
        void handleTouchEnd(InputSystem* pInputSystem, const TouchEventData& touch); // 0x00C7ED80 (:125)
        void handleTouchCanceled(); // 0x00C74630 (:155)
        void endFrame(InputSystem* pInputSystem); // 0x00C7ECC0 (:65)
        void addToAccumulatedDeltaPosition(float2 deltaPosition); // 0x00C74650 (:164)
        void addDragEventFromAccumulatedFrameDeltaPosition(InputSystem* pInputSystem); // 0x00C7DDA0 (:175)
        void evaluateFlick(InputSystem* pInputSystem); // 0x00C7DE60 (:237)
        bool hasMovedThisFrame() const; // 0x00C769C0 (:170)
        void logHistoryEntry(); // 0x00C7DDE0 (:228)

        RingBuffer<MoveData> m_moveHistory; // 0x00
        bool m_isTouchInProgress; // 0x14
        float2 m_startDragPosition; // 0x18
        float2 m_lastPosition; // 0x20
        float2 m_accumulatedFrameDeltaPosition; // 0x28
        float m_currentDeltaTime; // 0x30
        uint32 m_controllerId; // 0x34
        uint32 m_touchId; // 0x38
    };
}
