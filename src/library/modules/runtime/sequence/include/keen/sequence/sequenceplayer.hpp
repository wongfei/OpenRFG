// Original: library/modules/runtime/sequence/include/keen/sequence/sequenceplayer.hpp
// Implementation: library/modules/runtime/sequence/sources/global/sequenceplayer.cpp
//
// Sequence playback: a SequencePlaybackState advances through the timed events of a sequence.
#pragma once

#include "keen/base/types.hpp"

namespace keen
{
    struct SequenceData;
    struct SequenceEvent;

    // size 0x1C
    struct SequencePlaybackState
    {
        SequencePlaybackState(); // 0x00CE59D0 (:352)

        uint32 flags; // 0x00
        uint32 quantizedTime; // 0x04
        float time; // 0x08
        float sequenceLength; // 0x0C
        const SequenceData* pSequence; // 0x10
        uint32 currentSequenceEventIndex; // 0x14
        uint32 lastQueriedSequenceEventIndex; // 0x18
    };

    namespace Sequence
    {
        bool isLooped(const SequencePlaybackState* pState); // 0x00CE5910 (:29)
        bool hasEnded(const SequencePlaybackState* pState); // 0x00CE5930 (:42)
        bool isRunning(const SequencePlaybackState* pState); // 0x00CE5970 (:55)
        bool hasEventsToCome(const SequencePlaybackState* pState); // 0x00CE5AB0 (:68)
        void startSequence(SequencePlaybackState* pState, const SequenceData* pSequence, uint32 flags, float startTime); // 0x00CE5E40 (:84)
        void stopSequence(SequencePlaybackState* pState); // 0x00CE5980 (:113)
        bool updateTime(SequencePlaybackState* pState, float timeStep); // 0x00CE5F40 (:121)
        void seekToEnd(SequencePlaybackState* pState); // 0x00CE5AD0 (:149)
        void incrementCurrentSequenceEventIndex(SequencePlaybackState* pState, uint32 step); // 0x00CE59A0 (:160)
        void updateCurrentSequenceEventIndexLooped(SequencePlaybackState* pState, uint32 startOffset, uint32 oldTime, uint32 newTime, bool ignoreOldTime); // 0x00CE5B10 (:172)
        void updateCurrentSequenceEventIndexNonLooped(SequencePlaybackState* pState, uint32 startOffset, uint32 oldTime, uint32 newTime, bool ignoreOldTime); // 0x00CE5C00 (:213)
        void updateCurrentSequenceEventIndex(SequencePlaybackState* pState, uint32 startOffset, uint32 oldTime, uint32 newTime, bool ignoreOldTime); // 0x00CE5C60 (:235)
        uint32 getEvents(SequenceEvent* pEventBuffer, uint32 eventBufferCapacity, SequencePlaybackState* pState); // 0x00CE5CC0 (:249)
        uint32 getEvents(SequenceEvent* pEventBuffer, uint32 eventBufferCapacity, const SequenceData* pSequence, float startTime, float endTime, uint32 flags); // 0x00CE6030 (:282)
        uint32 getEventsFiltered(SequenceEvent* pEventBuffer, uint32 eventBufferCapacity, uint32 filteredTypesMask, SequencePlaybackState* pState); // 0x00CE5D50 (:291)
        uint32 getEventsFiltered(SequenceEvent* pEventBuffer, uint32 eventBufferCapacity, uint32 filteredTypesMask, const SequenceData* pSequence, float startTime, float endTime, uint32 flags); // 0x00CE60A0 (:336)
        uint32 getPendingEventsCount(const SequencePlaybackState* pState); // 0x00CE59C0 (:345)
    }
}
