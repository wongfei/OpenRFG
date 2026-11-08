// Original: library/modules/runtime/graphics/sources/d3d11/screencapture_d3d11.hpp
// Implementation: library/modules/runtime/graphics/sources/d3d11/screencapture_d3d11.cpp
//
// ScreenCapture, copies the back buffer to staging textures for screenshots/video capture.
#pragma once

#include "keen/base/types.hpp"
#include <d3d11.h>
#include "keen/base/array.hpp"
#include "keen/base/win32/semaphore_win32.hpp"
#include "keen/base/win32/thread_win32.hpp"
#include "keen/graphics/immediaterenderer.hpp"
#include "library/modules/runtime/graphics/sources/d3d11/texturedata_d3d11.hpp"

namespace keen
{
    struct FileSystem;
    struct GraphicsCommandBuffer;
    struct GraphicsSystem;
    class MemoryAllocator;
    struct RenderTarget;

    enum ScreenCaptureState
    {
        ScreenCaptureState_Recording = 0,
        ScreenCaptureState_StartWriting = 1,
        ScreenCaptureState_Writing = 2,
        ScreenCaptureState_StartRecording = 3,
    };

    // size 0x768
    struct ScreenCapture
    {
        ScreenCapture(); // 0x00C62900
        ~ScreenCapture(); // 0x00C60C60

        GraphicsSystem* pGraphicsSystem; // 0x00
        FileSystem* pFileSystem; // 0x04
        uint32 width; // 0x08
        uint32 height; // 0x0C
        TextureData captureTargetData; // 0x10
        TextureData captureCopyData; // 0x30
        const RenderTarget* pCaptureTarget; // 0x50
        const RenderTarget* pNoGammaCaptureTarget; // 0x54
        Array<uint8*> frameBuffer; // 0x58
        uint32 frameBufferWriteIndex; // 0x60
        Array<uint8*> tempBuffer; // 0x64
        uint32 tempBufferWriteIndex; // 0x6C
        uint32 tempBufferReadIndex; // 0x70
        ImmediateRenderer immediateRenderer; // 0x74
        char basePath[128]; // 0x574
        char subPath[128]; // 0x5F4
        char filenamePrefix[128]; // 0x674
        Thread thread; // 0x6F4
        bool threadStarted; // 0x748
        Semaphore tempBufferDataCount; // 0x74C
        Semaphore tempBufferFreeCount; // 0x754
        uint32 captureCount; // 0x75C
        uint32 saveFrameIndex; // 0x760
        ScreenCaptureState state; // 0x764
    };

    namespace graphics
    {
        ScreenCapture* createScreenCapture(MemoryAllocator* pAllocator, GraphicsSystem* pGraphicsSystem, FileSystem* pFileSystem, uint32 width, uint32 height, uint32 bufferCount, const char* pBasePath); // 0x00C73EC0 (:76)
        void destroySceenCapture(MemoryAllocator* pAllocator, ScreenCapture* pScreenCapture); // 0x00C72840 (:191)
        bool isFrameSavingActive(uint32* pIndex, ScreenCapture* pScreenCapture); // 0x00C5CAA0 (:235)
        const RenderTarget* getScreenCaptureRenderTarget(ScreenCapture* pScreenCapture); // 0x00C5CAD0 (:249)
        const RenderTarget* getScreenCaptureNoGammaRenderTarget(ScreenCapture* pScreenCapture); // 0x00C5CAE0 (:255)
        float getScreenCaptureAspectRatio(ScreenCapture* pScreenCapture); // 0x00C5CAF0 (:261)
        void captureFrame(ScreenCapture* pScreenCapture, GraphicsCommandBuffer* pCommandBuffer, RenderTarget* pBackBufferRenderTarget); // 0x00C6F0B0 (:274)
        void saveFrames(ScreenCapture* pScreenCapture, const char* pSubPath, const char* pFilenamePrefix, uint32 frameCount); // 0x00C629B0 (:389)
    }
}
