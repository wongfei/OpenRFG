// Original: library/modules/runtime/sound/sources/global/soundvolume.cpp
//
// SoundVolume, a volume fading linearly towards a target (SFX.md SX1). Float rule as soundsystem.cpp.

#include "library/modules/runtime/sound/sources/global/soundvolume.hpp"

namespace keen
{
    namespace SoundSystem
    {
        // 0x00C36D70 (:14)
        SoundVolume::SoundVolume()
        {
            m_currentVolume = 0.0f;
            m_targetVolume = 0.0f;
            m_fadeStep = 0.0f;
            m_mixedVolume = 0.0f;
        }

        // 0x00C335D0 (:26)
        void SoundVolume::initialize(float startVolume)
        {
            m_currentVolume = startVolume;
            m_targetVolume = startVolume;
            m_fadeStep = 0.0f;
            m_mixedVolume = startVolume;
        }

        // 0x00C335F0 (:45)
        void SoundVolume::update(float timeStep, float toMixVolume)
        {
            const float targetVolume = m_targetVolume;
            if (m_currentVolume != targetVolume)
            {
                const float currentVolume = (float)((double)m_fadeStep * (double)timeStep + (double)m_currentVolume);
                m_currentVolume = currentVolume;
                const bool overshot = m_fadeStep < 0.0f ? targetVolume > currentVolume : currentVolume > targetVolume;
                if (overshot)
                {
                    m_currentVolume = targetVolume;
                }
            }
            m_mixedVolume = toMixVolume * m_currentVolume;
        }

        // 0x00C33680 (:78)
        void SoundVolume::setTargetVolume(float newTargetVolume, float durationInS)
        {
            m_targetVolume = newTargetVolume;
            if (durationInS > 0.0f)
            {
                m_fadeStep = (newTargetVolume - m_currentVolume) / durationInS;
            }
            else
            {
                m_fadeStep = 0.0f;
                m_currentVolume = newTargetVolume;
            }
        }
    }
}
