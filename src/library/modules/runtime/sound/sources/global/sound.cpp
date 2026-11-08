// Original: library/modules/runtime/sound/sources/global/sound.cpp
//
// Playing sounds (SFX.md SX1): VoiceSound and BaseSound, and keen's own panning and distance attenuation (no X3DAudio
// in rfg.exe): the attenuation and angle of a sound relative to the listener and the per-speaker volumes for stereo,
// 5.1 and 7.1 outputs.
//
// Float rule as soundsystem.cpp: single float operations stay float, fused expressions are double, and the x87 CRT
// calls (sin, cos, atan2, pow) take float arguments widened to double and are rounded to float at once.

#include "library/modules/runtime/sound/sources/global/sound.hpp"
#include "keen/math/matrix43.hpp"
#include "library/modules/runtime/sound/sources/global/soundsystemstate.hpp"

#include <math.h>

namespace keen
{
    namespace SoundSystem
    {
        const uint8 BaseSound::MaxVoiceSoundIndex = 0xFD; // 0x01340B23
        const uint8 BaseSound::InvalidVoiceSoundEnded = 0xFE; // 0x01340B24
        const uint8 BaseSound::InvalidVoice = 0xFF; // 0x01340B25

        // Speaker angles in radians, as float constants: 30, 110 and 150 degrees.
        static const float Angle30 = 0.52359879f;
        static const float Angle110 = 1.9198622f;
        static const float Angle150 = 2.6179938f;

        static inline float clamp01(float value)
        {
            if (0.0f > value)
            {
                return 0.0f;
            }
            if (value > 1.0f)
            {
                return 1.0f;
            }
            return value;
        }

        // One speaker's share: cos(panAngle - speakerAngle) * volume, at least 0, plus the overdrive share.
        static inline float calculateSpeakerVolume(float volume, float panAngle, float speakerAngle, float overdrive)
        {
            const float angle = (float)((double)panAngle - (double)speakerAngle);
            const float cosine = (float)cos((double)angle);
            float speakerVolume = cosine * volume;
            if (!(speakerVolume >= 0.0f))
            {
                speakerVolume = 0.0f;
            }
            return (float)((double)speakerVolume + (double)overdrive);
        }

        // The overdrive share: 10% of the volume above 1.
        static inline float calculateOverdrive(float volume)
        {
            const float above = volume - 1.0f;
            const float overdrive = above >= 0.0f ? above : 0.0f;
            return (float)((double)overdrive * (double)0.1f);
        }

        // 0x00C32A70 (:12)
        VoiceSound::VoiceSound()
        {
            m_pBaseSound = nullptr;
            m_providerType = 0xFF;
            m_pProviderData = nullptr;
        }

        // 0x00C32A80 (:19)
        bool VoiceSound::isUsed() const
        {
            return m_providerType != 0xFF;
        }

        // 0x00C32A90 (:24)
        void VoiceSound::free()
        {
            m_providerType = 0xFF;
            if (m_pBaseSound != nullptr)
            {
                m_pBaseSound->voiceIndex = BaseSound::InvalidVoice;
                m_pBaseSound = nullptr;
            }
        }

        // 0x00C32AB0 (:34)
        void VoiceSound::soundEndedAndFree()
        {
            m_providerType = 0xFF;
            if (m_pBaseSound != nullptr)
            {
                m_pBaseSound->voiceIndex = BaseSound::InvalidVoiceSoundEnded;
                m_pBaseSound = nullptr;
            }
        }

        // 0x00C32AD0 (:218)
        BaseSound::BaseSound()
        {
            definition = nullptr;
            pSoundFileData = nullptr;
            pAutomation = nullptr;
            runCountAndFlags = 0;
            pauseGroup = 0xFF;
            voiceIndex = 0xFF;
        }

        // 0x00C328B0 (sound.hpp:57)
        bool BaseSound::isUsed() const
        {
            return definition != nullptr;
        }

        // 0x00C328C0 (sound.hpp:63)
        const SoundDefinition* BaseSound::getDefinition() const
        {
            return keen::getDefinition(definition);
        }

        // 0x00C34BE0 (:44)
        // World sounds: the listener-space position (hrtfMatrix is the inverse listener matrix) gives the distance and the
        // angle. The attenuation is a dB rolloff, -96.3 dB at maxDistance, with a linear fade over the last 10%.
        // Listener sounds: the definition's pan angle; panDistance below 1 is the inner distance (1 / panDistance
        // attenuation above 1).
        void calculateAttenuationAndAngle(PositionedSoundData* pData, const BaseSound* pSound, const Matrix43& hrtfMatrix)
        {
            const SoundDefinition* pDefinition = keen::getDefinition(pSound->definition);
            if (pDefinition->positioning == Positioning_world)
            {
                const double x = pSound->position.x;
                const double y = pSound->position.y;
                const double z = pSound->position.z;
                const float localX = (float)((double)hrtfMatrix.rot.y.x * y + (double)hrtfMatrix.rot.x.x * x + (double)hrtfMatrix.rot.z.x * z + (double)hrtfMatrix.pos.x);
                const float localY = (float)((double)hrtfMatrix.rot.y.y * y + (double)hrtfMatrix.rot.x.y * x + (double)hrtfMatrix.rot.z.y * z + (double)hrtfMatrix.pos.y);
                const float localZ = (float)((double)hrtfMatrix.rot.y.z * y + (double)hrtfMatrix.rot.x.z * x + (double)hrtfMatrix.rot.z.z * z + (double)hrtfMatrix.pos.z);
                const float squareDistance = (float)((double)localX * localX + (double)localY * localY + (double)localZ * localZ);
                const float distanceToListener = (float)sqrt((double)squareDistance);
                float distance = (float)((double)distanceToListener - (double)pDefinition->minDistance);
                const float maxDistance = pDefinition->maxDistance;
                if (!(distance >= 0.0f))
                {
                    distance = 0.0f;
                }
                if ((float)((double)distance - (double)maxDistance) >= 0.0f)
                {
                    distance = maxDistance;
                }
                const float ratio = (float)((double)distance / (double)maxDistance);
                // Magic numbers: the linear fade starts at 90% of the distance range.
                float fadeRatio = ratio;
                if ((float)((double)0.9f - (double)ratio) >= 0.0f)
                {
                    fadeRatio = 0.9f;
                }
                // Magic numbers: -96.3 dB at the full range (the rolloff uses the unclamped ratio).
                const float decibel = (float)((double)ratio * (double)-96.3f);
                const float exponent = (float)((double)decibel / 20.0);
                const float rolloff = (float)pow(10.0, (double)exponent);
                const float fade = (float)(1.0 - ((double)fadeRatio - (double)0.9f) * 10.0);
                pData->attenuation = (float)((double)rolloff * (double)fade);

                const float atanValue = (float)atan2((double)localZ, (double)localX);
                float angle = (float)((double)atanValue - (double)1.5707964f);
                if (angle < 0.0f)
                {
                    angle = (float)((double)angle + (double)6.2831855f);
                }
                pData->angle = angle;
                pData->innerDistance = 1.0f;
                return;
            }

            float panDistance = pDefinition->panDistance;
            if (!((float)((double)panDistance - 1.0) >= 0.0f))
            {
                panDistance = 1.0f;
            }
            pData->attenuation = (float)(1.0 / (double)panDistance);
            pData->angle = pDefinition->panAngle;
            const float innerDistance = pDefinition->panDistance;
            if (0.0f > innerDistance)
            {
                pData->innerDistance = 0.0f;
            }
            else if (innerDistance > 1.0f)
            {
                pData->innerDistance = 1.0f;
            }
            else
            {
                pData->innerDistance = innerDistance;
            }
        }

        // 0x00C34EE0 (:77)
        // Constant power panning between left and right (sin of the pan angle), the inner distance moving energy from the
        // front pair [0], [1] to the second pair [2], [3].
        void calculateStereoVolumes(float* pOutput, float volume, float panAngle, float innerDistance)
        {
            if (!(volume > 0.0f))
            {
                pOutput[2] = 0.0f;
                pOutput[1] = 0.0f;
                pOutput[0] = 0.0f;
                pOutput[3] = 0.0f;
                return;
            }
            const float sine = (float)sin((double)panAngle);
            const float pan = (float)(-(double)sine * 0.5 + 0.5);
            const float counterPan = (float)(1.0 - (double)pan);
            const float leftShare = (float)sqrt((double)pan);
            const float rightShare = (float)sqrt((double)counterPan);
            const float energy = (float)((double)(rightShare * rightShare) + (double)(leftShare * leftShare));
            const float scale = (float)sqrt((double)(float)((double)volume / (double)energy));
            if (!(innerDistance >= 0.0f))
            {
                innerDistance = 0.0f;
            }
            if ((float)((double)innerDistance - 1.0) >= 0.0f)
            {
                innerDistance = 1.0f;
            }
            const float frontPart = (float)(((double)innerDistance + 1.0) * 0.5);
            const float backShare = (float)sqrt((double)(float)(1.0 - (double)frontPart));
            const float frontShare = (float)sqrt((double)frontPart);
            const float left = clamp01(scale * rightShare);
            const float right = clamp01(scale * leftShare);
            pOutput[0] = frontShare * left;
            pOutput[1] = frontShare * right;
            pOutput[2] = backShare * left;
            pOutput[3] = backShare * right;
        }

        // 0x00C350D0 (:113)
        // Four speakers at +-30 and +-110 degrees, normalized to the volume's energy: [0] 30, [1] -30, [2] 110, [3] -110.
        void calculate5point1Volumes(float* pOutput, float volume, float panAngle)
        {
            if (!(volume > 0.0f))
            {
                pOutput[1] = 0.0f;
                pOutput[2] = 0.0f;
                pOutput[3] = 0.0f;
                pOutput[0] = 0.0f;
                return;
            }
            const float overdrive = calculateOverdrive(volume);
            const float speakers[4] =
            {
                calculateSpeakerVolume(volume, panAngle, Angle30, overdrive),
                calculateSpeakerVolume(volume, panAngle, -Angle30, overdrive),
                calculateSpeakerVolume(volume, panAngle, Angle110, overdrive),
                calculateSpeakerVolume(volume, panAngle, -Angle110, overdrive),
            };
            double energy = (double)(speakers[0] * speakers[0]) + (double)(speakers[1] * speakers[1]);
            energy += (double)(speakers[2] * speakers[2]);
            energy += (double)(speakers[3] * speakers[3]);
            const float scale = (float)sqrt((double)(float)((double)volume / (double)(float)energy));
            for (uint32 i = 0; i < 4; ++i)
            {
                pOutput[i] = clamp01(scale * speakers[i]);
            }
        }

        // 0x00C35420 (:160)
        // Six speakers at +-30, +-110 and +-150 degrees: [0] 30, [1] -30, [2] 110, [3] -110, [4] 150, [5] -150.
        void calculate7point1Volumes(float* pOutput, float volume, float panAngle)
        {
            if (!(volume > 0.0f))
            {
                for (uint32 i = 0; i < 6; ++i)
                {
                    pOutput[i] = 0.0f;
                }
                return;
            }
            const float overdrive = calculateOverdrive(volume);
            const float speakers[6] =
            {
                calculateSpeakerVolume(volume, panAngle, Angle30, overdrive),
                calculateSpeakerVolume(volume, panAngle, -Angle30, overdrive),
                calculateSpeakerVolume(volume, panAngle, Angle110, overdrive),
                calculateSpeakerVolume(volume, panAngle, -Angle110, overdrive),
                calculateSpeakerVolume(volume, panAngle, Angle150, overdrive),
                calculateSpeakerVolume(volume, panAngle, -Angle150, overdrive),
            };
            double energy = (double)(speakers[0] * speakers[0]) + (double)(speakers[1] * speakers[1]);
            for (uint32 i = 2; i < 6; ++i)
            {
                energy += (double)(speakers[i] * speakers[i]);
            }
            const float scale = (float)sqrt((double)(float)((double)volume / (double)(float)energy));
            for (uint32 i = 0; i < 6; ++i)
            {
                pOutput[i] = clamp01(scale * speakers[i]);
            }
        }
    }
}
