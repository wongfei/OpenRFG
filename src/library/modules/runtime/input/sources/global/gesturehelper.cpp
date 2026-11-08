// Original: library/modules/runtime/input/sources/global/gesturehelper.cpp
//
// Drag and flick gestures from the touch events of one controller (initializeGestureHelper chooses it): a drag begins
// with the touch, moves by the frame's accumulated distance, ends with the touch; a fast enough straight move at the end
// is a flick. rfg.exe never chooses a controller, so update returns at once there.
//
// Partial: the members the input system's update reaches.

#include "keen/input/gesturehelper.hpp"
#include "keen/base/memoryallocator.hpp"
#include "keen/input/inputevents.hpp"
#include "library/modules/runtime/input/sources/global/inputsystem_private.hpp"

#include <math.h>

namespace keen
{
    // |value| as the original computes it: value - (-value) >= 0 picks value.
    static float gestureAbs(float value)
    {
        const float negated = -value;
        return (float)((double)value - (double)negated) >= 0.0f ? value : negated;
    }

    // 0x00C7AB60 (:11)
    GestureHelper::GestureHelper()
    {
        m_isTouchInProgress = false;
        m_startDragPosition.x = 0.0f;
        m_startDragPosition.y = 0.0f;
        m_lastPosition.x = 0.0f;
        m_lastPosition.y = 0.0f;
        m_accumulatedFrameDeltaPosition.x = 0.0f;
        m_accumulatedFrameDeltaPosition.y = 0.0f;
        m_currentDeltaTime = 0.0f;
        m_controllerId = (uint32)InvalidControllerId;
    }

    // 0x00C7ABA0 (:24)
    GestureHelper::~GestureHelper()
    {
    }

    // 0x00C7ABB0 (:29)
    void GestureHelper::initialize(uint32 controllerId, MemoryAllocator* pAllocator)
    {
        m_controllerId = controllerId;
        m_moveHistory.create(pAllocator, MaxHistoryLength, 0x10u);
    }

    // 0x00C7ABF0 (:39)
    void GestureHelper::shutdown(MemoryAllocator* pAllocator)
    {
        if (m_controllerId == (uint32)InvalidControllerId)
        {
            return;
        }
        m_moveHistory.destroy(pAllocator);
        m_controllerId = (uint32)InvalidControllerId;
    }

    // 0x00C74610 (:52)
    void GestureHelper::startFrame(float timeStep)
    {
        m_currentDeltaTime = timeStep;
        m_accumulatedFrameDeltaPosition.x = 0.0f;
        m_accumulatedFrameDeltaPosition.y = 0.0f;
    }

    // 0x00C7ECC0 (:65)
    void GestureHelper::endFrame(InputSystem* pInputSystem)
    {
        if (!m_isTouchInProgress)
        {
            return;
        }
        logHistoryEntry();
        if (!hasMovedThisFrame())
        {
            return;
        }
        addDragEventFromAccumulatedFrameDeltaPosition(pInputSystem);
        m_accumulatedFrameDeltaPosition.x = 0.0f;
        m_accumulatedFrameDeltaPosition.y = 0.0f;
    }

    // 0x00C7DD30 (:89)
    // The drag begin event carries the position after an unset first data word (as a touch event's).
    void GestureHelper::handleTouchBegin(InputSystem* pInputSystem, const TouchEventData& touch)
    {
        m_isTouchInProgress = true;
        m_lastPosition = touch.position;
        m_touchId = touch.touchId;
        m_accumulatedFrameDeltaPosition.x = 0.0f;
        m_accumulatedFrameDeltaPosition.y = 0.0f;
        m_startDragPosition = touch.position;
        InputEvent event = {};
        event.controllerId = (uint8)m_controllerId;
        event.type = InputEventType_DragBegin;
        event.data.touch.position = touch.position;
        input::addInputEvent(pInputSystem, event);
    }

    // 0x00C76940 (:107)
    void GestureHelper::handleTouchMove(InputSystem*, const TouchEventData& touch)
    {
        float2 delta;
        delta.x = (float)((double)touch.position.x - (double)m_lastPosition.x);
        delta.y = (float)((double)touch.position.y - (double)m_lastPosition.y);
        addToAccumulatedDeltaPosition(delta);
        m_lastPosition = touch.position;
    }

    // 0x00C7ED80 (:125)
    // The last move, a drag event when it moved, the flick, then the drag end (its data like the drag begin's).
    void GestureHelper::handleTouchEnd(InputSystem* pInputSystem, const TouchEventData& touch)
    {
        handleTouchMove(pInputSystem, touch);
        logHistoryEntry();
        if (hasMovedThisFrame())
        {
            addDragEventFromAccumulatedFrameDeltaPosition(pInputSystem);
            m_accumulatedFrameDeltaPosition.x = 0.0f;
            m_accumulatedFrameDeltaPosition.y = 0.0f;
        }
        evaluateFlick(pInputSystem);
        InputEvent event = {};
        event.controllerId = (uint8)m_controllerId;
        event.type = InputEventType_DragEnd;
        event.data.touch.position = m_startDragPosition;
        input::addInputEvent(pInputSystem, event);
        m_isTouchInProgress = false;
        m_startDragPosition.x = 0.0f;
        m_startDragPosition.y = 0.0f;
        m_moveHistory.clear();
    }

    // 0x00C74630 (:155)
    void GestureHelper::handleTouchCanceled()
    {
        m_isTouchInProgress = false;
        m_accumulatedFrameDeltaPosition.x = 0.0f;
        m_accumulatedFrameDeltaPosition.y = 0.0f;
        m_lastPosition.x = 0.0f;
        m_lastPosition.y = 0.0f;
    }

    // 0x00C74650 (:164)
    void GestureHelper::addToAccumulatedDeltaPosition(float2 deltaPosition)
    {
        m_accumulatedFrameDeltaPosition.x = (float)((double)deltaPosition.x + (double)m_accumulatedFrameDeltaPosition.x);
        m_accumulatedFrameDeltaPosition.y = (float)((double)m_accumulatedFrameDeltaPosition.y + (double)deltaPosition.y);
    }

    // 0x00C769C0 (:170)
    bool GestureHelper::hasMovedThisFrame() const
    {
        return gestureAbs(m_accumulatedFrameDeltaPosition.x) > 0.0f || gestureAbs(m_accumulatedFrameDeltaPosition.y) > 0.0f;
    }

    // 0x00C7DDA0 (:175)
    void GestureHelper::addDragEventFromAccumulatedFrameDeltaPosition(InputSystem* pInputSystem)
    {
        InputEvent event = {};
        event.controllerId = (uint8)m_controllerId;
        event.type = InputEventType_Drag;
        event.data.drag.startPosition = m_startDragPosition;
        event.data.drag.deltaPosition = m_accumulatedFrameDeltaPosition;
        input::addInputEvent(pInputSystem, event);
    }

    // 0x00C7EEF0 (:180)
    // startFrame, the queued touch events of the controller (handleTouchMove and handleTouchCanceled inlined), then
    // endFrame (inlined).
    void GestureHelper::update(InputSystem* pInputSystem, float timeStep)
    {
        if (m_controllerId == (uint32)InvalidControllerId)
        {
            return;
        }
        startFrame(timeStep);
        const uint32 eventCount = pInputSystem->eventQueue.getSize();
        for (uint32 i = 0; i < eventCount; ++i)
        {
            const InputEvent& event = pInputSystem->eventQueue[i];
            if ((uint32)event.controllerId != m_controllerId)
            {
                continue;
            }
            switch (event.type)
            {
            case InputEventType_TouchBegin:
                handleTouchBegin(pInputSystem, event.data.touch);
                break;
            case InputEventType_TouchEnd:
                handleTouchEnd(pInputSystem, event.data.touch);
                break;
            case InputEventType_TouchCancel:
                handleTouchCanceled();
                break;
            case InputEventType_TouchMove:
                handleTouchMove(pInputSystem, event.data.touch);
                break;
            default:
                break;
            }
        }
        endFrame(pInputSystem);
    }

    // 0x00C7DDE0 (:228)
    void GestureHelper::logHistoryEntry()
    {
        MoveData move;
        move.distance = m_accumulatedFrameDeltaPosition;
        move.time = m_currentDeltaTime;
        m_moveHistory.pushBack(move);
    }

    // 0x00C7DE60 (:237)
    // The moves of the history summed (in double, rounded to float each step): a flick needs one axis to dominate the
    // other twice over, a distance of at least 1 and a speed above 1000 per second. The direction follows the dominant
    // axis; the speed is distance / time.
    void GestureHelper::evaluateFlick(InputSystem* pInputSystem)
    {
        float distanceX = 0.0f;
        float distanceY = 0.0f;
        float time = 0.0f;
        for (RingBuffer<MoveData>::Iterator it = m_moveHistory.getStart(); it != m_moveHistory.getEnd(); ++it)
        {
            const MoveData& move = *it;
            distanceX = (float)((double)move.distance.x + (double)distanceX);
            distanceY = (float)((double)move.distance.y + (double)distanceY);
            time = (float)((double)move.time + (double)time);
        }
        FlickDirection direction;
        if ((double)gestureAbs(distanceX) > (double)gestureAbs((float)((double)distanceY * 2.0)))
        {
            direction = distanceX > 0.0f ? FlickDirection_Right : FlickDirection_Left;
        }
        else if ((double)gestureAbs(distanceY) > (double)gestureAbs((float)((double)distanceX * 2.0)))
        {
            direction = distanceY > 0.0f ? FlickDirection_Down : FlickDirection_Up;
        }
        else
        {
            return;
        }
        const float distance = (float)sqrt((double)(float)((double)distanceY * (double)distanceY + (double)distanceX * (double)distanceX));
        if (1.0f > distance || time <= 0.0f)
        {
            return;
        }
        const float speed = (float)((double)distance / (double)time);
        if ((double)speed <= 1000.0)
        {
            return;
        }
        InputEvent event = {};
        event.controllerId = (uint8)m_controllerId;
        event.type = InputEventType_Flick;
        event.data.flick.direction = direction;
        event.data.flick.speed = speed;
        event.data.flick.startPosition = m_startDragPosition;
        input::addInputEvent(pInputSystem, event);
    }
}
