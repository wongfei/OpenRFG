// Original: library/modules/runtime/sound/sources/global/soundvolume.hpp
// Implementation: library/modules/runtime/sound/sources/global/soundvolume.cpp
//
// SoundVolume, a volume fading towards a target.
#pragma once

#include "keen/base/types.hpp"

namespace keen
{
    namespace SoundSystem
    {
        // size 0x10
        class SoundVolume
        {
        public:
            SoundVolume(); // 0x00C36D70 (:14)
            void initialize(float startVolume); // 0x00C335D0 (:26)
            void update(float timeStep, float toMixVolume); // 0x00C335F0 (:45)
            void setTargetVolume(float newTargetVolume, float durationInS); // 0x00C33680 (:78)
            float getVolume() const;
            float getMixedVolume() const;

        private:
            float m_mixedVolume; // 0x00
            float m_targetVolume; // 0x04
            float m_currentVolume; // 0x08
            float m_fadeStep; // 0x0C
        };

    }
}
