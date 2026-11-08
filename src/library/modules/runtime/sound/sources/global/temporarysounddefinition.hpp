// Original: library/modules/runtime/sound/sources/global/temporarysounddefinition.hpp
// Implementation: library/modules/runtime/sound/sources/global/temporarysounddefinition.cpp
//
// TemporarySoundDefinition, a sound definition created at run time for one sound file.
#pragma once

#include "keen/base/types.hpp"
#include "keen/sound/soundbank.hpp"
#include "keen/sound/sounddefinition.hpp"
#include "keen/sound/soundsystem.hpp"

namespace keen
{
    struct SoundFileHandleType;

    namespace SoundSystem
    {
        // size 0x54
        class TemporarySoundDefinition : public SoundDefinitionResource
        {
        public:
            TemporarySoundDefinition(); // 0x00C38920 (:10)
            ~TemporarySoundDefinition(); // 0x00C36DB0 (:14)
            void create(); // 0x00C36DC0 (:19)
            void setSoundFile(const SoundFileHandleType* soundFile, uint32 soundBus, Positioning positioning, const SoundDefinitionParameter2d* p2dParameter, const SoundDefinitionParameter3d* p3dParameter, sint8 countGroup, uint8 maxCount, uint8 minDelay, sint8 priority); // 0x00C336D0 (:36)
            void clearSoundFile(); // 0x00C337B0 (:88)
            void clearUsage(); // 0x00C337C0 (:93)
            bool isInUse(); // 0x00C337D0 (:98)
            bool isFreeable(); // 0x00C337E0 (:103)

        private:
            bool m_inUse; // 0x0C
            SoundDefinition m_data; // 0x10
            SoundFileEntry m_soundFiles[1]; // 0x4C
        };

    }
}
