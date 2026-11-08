// Stand-ins (new code, not recovered) for keen/graphics functions and globals that are not implemented yet; each one is
// removed when its original definition lands. The keen framework's platform services: linked by the programs on the
// original entry (OpenRFG).

#include "stub_trace.h"

#include "keen/gameframework/gameframework.hpp"

#include <stdlib.h>

namespace keen
{
    struct GraphicsCommandBuffer;
    struct GraphicsSystem;
    struct RenderTarget;
    struct ScreenCapture;

    namespace graphics
    {
        // keen graphics' screen capture (screencapture_d3d11.cpp, not ported). Nothing starts a capture, so captureFrame
        // and saveFrames are not reached.
        ScreenCapture* createScreenCapture(MemoryAllocator*, GraphicsSystem*, FileSystem*, uint32, uint32, uint32, const char*) // STUB: no screen capture
        {
            GAME_STUB_HIT("keen::graphics::createScreenCapture");
            return nullptr;
        }

        void destroySceenCapture(MemoryAllocator*, ScreenCapture*) // STUB: no screen capture
        {
            GAME_STUB_HIT("keen::graphics::destroySceenCapture");
        }

        void captureFrame(ScreenCapture*, GraphicsCommandBuffer*, RenderTarget*) // STUB: no screen capture
        {
            GAME_STUB_HIT("keen::graphics::captureFrame");
            abort();
        }

        void saveFrames(ScreenCapture*, const char*, const char*, uint32) // STUB: no screen capture
        {
            GAME_STUB_HIT("keen::graphics::saveFrames");
            abort();
        }
    }
}
