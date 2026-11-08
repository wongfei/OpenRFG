// Original: library/modules/runtime/sound/sources/global/temporarysounddefinition.cpp
//
// TemporarySoundDefinition, a sound definition made at run time for one sound file (SFX.md SX1): audio_lib plays its
// wavebank waves through these (createDynamicSoundDefinition2d/3d).

#include "library/modules/runtime/sound/sources/global/temporarysounddefinition.hpp"

namespace keen
{
    namespace SoundSystem
    {
        // 0x00C38920 (:10)
        TemporarySoundDefinition::TemporarySoundDefinition()
            : SoundDefinitionResource(0)
        {
        }

        // 0x00C36DB0 (:14)
        TemporarySoundDefinition::~TemporarySoundDefinition()
        {
            SoundDefinitionResource::destroy();
        }

        // 0x00C36DC0 (:19)
        void TemporarySoundDefinition::create()
        {
            m_soundFiles[0].chance = 1.0f;
            m_soundFiles[0].soundFile = nullptr;
            m_data.soundFiles.setData(m_soundFiles, 1);
            m_inUse = false;
            SoundDefinitionResource::create(&m_data);
        }

        // 0x00C336D0 (:36)
        // Listener sounds take the 2D parameters, world sounds the 3D ones; without them the defaults apply (pan
        // distance 1, distances 1 to 50).
        void TemporarySoundDefinition::setSoundFile(const SoundFileHandleType* soundFile, uint32 soundBus, Positioning positioning, const SoundDefinitionParameter2d* p2dParameter, const SoundDefinitionParameter3d* p3dParameter, sint8 countGroup, uint8 maxCount, uint8 minDelay, sint8 priority)
        {
            m_soundFiles[0].soundFile = soundFile;
            m_data.output_bus = (uint8)soundBus;
            m_data.priority = priority;
            m_data.positioning = (uint8)positioning;
            m_data.flags = 0;
            m_data.pitch = 0.0f;
            m_data.pitchVariant = 0.0f;
            m_data.volume = 0.0f;
            m_data.volumeVariant = 0.0f;
            if (positioning == Positioning_listener && p2dParameter != nullptr)
            {
                m_data.centerFraction = p2dParameter->centerFraction;
                m_data.panAngle = p2dParameter->panAngle;
                m_data.panDistance = p2dParameter->panDistance;
                m_data.panAngleVariation = p2dParameter->panAngleVariation;
            }
            else if (positioning == Positioning_world && p3dParameter != nullptr)
            {
                m_data.minDistance = p3dParameter->minDistance;
                m_data.maxDistance = p3dParameter->maxDistance;
                m_data.positionVariation = p3dParameter->positionVariation;
            }
            else
            {
                // Magic numbers: the SoundDefinitionParameter2d/3d defaults.
                m_data.panDistance = 1.0f;
                m_data.minDistance = 1.0f;
                m_data.centerFraction = 0.0f;
                m_data.panAngle = 0.0f;
                m_data.panAngleVariation = 0.0f;
                m_data.maxDistance = 50.0f;
                m_data.positionVariation = 0.0f;
            }
            m_data.countGroup = countGroup;
            m_data.maxCount = maxCount;
            m_data.minimumDelay = minDelay;
            m_data.avoidRepeatingLastX = 0;
            m_inUse = true;
        }

        // 0x00C337B0 (:88)
        void TemporarySoundDefinition::clearSoundFile()
        {
            m_soundFiles[0].soundFile = nullptr;
        }

        // 0x00C337C0 (:93)
        void TemporarySoundDefinition::clearUsage()
        {
            m_inUse = false;
        }

        // 0x00C337D0 (:98)
        bool TemporarySoundDefinition::isInUse()
        {
            return m_inUse;
        }

        // 0x00C337E0 (:103)
        bool TemporarySoundDefinition::isFreeable()
        {
            return m_soundFiles[0].soundFile == nullptr;
        }
    }
}
