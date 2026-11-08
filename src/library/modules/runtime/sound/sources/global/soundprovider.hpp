// Original: library/modules/runtime/sound/sources/global/soundprovider.hpp
//
// SoundProvider, decodes or streams sound data into voices (StreamInterface for streamed files). Placement of
// StreamInterface inferred.
#pragma once

#include "keen/base/types.hpp"

namespace keen
{
    namespace SoundSystem
    {
        struct System;
        struct VoiceSound;
    }
}

namespace keen
{
    namespace SoundSystem
    {
        // size 0x4
        class SoundProvider
        {
        public:
            virtual ~SoundProvider(); // vtbl+0x00 0x00C32BD0 (.h:16)
            virtual bool startSound(VoiceSound*, float, uint32, System*) = 0; // vtbl+0x04
            virtual void stopSound(VoiceSound*, System*) = 0; // vtbl+0x08
            virtual void pauseSound(VoiceSound*) = 0; // vtbl+0x0C
            virtual void resumeSound(VoiceSound*) = 0; // vtbl+0x10
            virtual int getNumAllocatedElements(); // vtbl+0x14 0x00C32BE0 (.h:24)
            SoundProvider(); // 0x00C32F80

            // 0x00 vfptr
        };

        // size 0x1
        class StreamInterface
        {
        };

    }
}
