// Original: library/modules/runtime/graphics/sources/d3d11/graphicssystem_d3d11.hpp
// Implementation: library/modules/runtime/graphics/sources/d3d11/graphicssystem_d3d11.cpp
//
// GraphicsSystem (device, immediate context, skinning, immediate and deferred command buffers, swap chains, display
// mode state), RenderSwapChain and GraphicsSystemParameters.
#pragma once

#include "keen/base/types.hpp"
#include <d3d11.h>
#include <d3d11.h>
#include "keen/base/memory.hpp"
#include "keen/graphics/graphicssystem.hpp"
#include "library/modules/runtime/graphics/sources/d3d11/fragmentshader_d3d11.hpp"
#include "library/modules/runtime/graphics/sources/d3d11/graphicscommandbuffer_d3d11.hpp"
#include "library/modules/runtime/graphics/sources/d3d11/rendertarget_d3d11.hpp"
#include "library/modules/runtime/graphics/sources/d3d11/skinning_d3d11.hpp"
#include "library/modules/runtime/graphics/sources/d3d11/texturedata_d3d11.hpp"
#include "library/modules/runtime/graphics/sources/global/graphicssystembase.hpp"
#include "keen/graphics/pixelformat.hpp"

struct HWND__;
struct ID3D11DepthStencilView;
struct ID3D11Device;
struct ID3D11DeviceContext;
struct ID3D11RenderTargetView;
struct IDXGISwapChain;

namespace keen
{
    struct FileSystem;
    struct GraphicsSystem;
    struct ScreenCapture;
    struct TaskSystem;

    // size 0x38
    struct GraphicsSystemParameters
    {
        GraphicsSystemParameters(); // 0x00C0E120 (.h:28)

        FileSystem* pShaderFileSystem; // 0x00
        void* pWindowHandle; // 0x04
        const char* pWindowTitle; // 0x08
        void* hInstance; // 0x0C
        int nShowCmd; // 0x10
        MemoryBlock shaderObjectMemory; // 0x14
        uint32 immediateGeometryBufferSize; // 0x1C
        bool supportScaling; // 0x20
        bool createDefaultSwapChain; // 0x21
        PixelFormat defaultDepthBufferFormat; // 0x24
        TaskSystem* pTaskSystem; // 0x28
        uint32 skinningBufferMaxInstanceCount; // 0x2C
        uint32 skinningBufferSizeInBytes; // 0x30
        uint32 defaultSwapChainPresentationInterval; // 0x34
    };

    // size 0x194
    struct RenderSwapChain
    {
        RenderSwapChain(); // 0x00C6A980 (:92)

        GraphicsSystem* pGraphicsSystem; // 0x00
        PixelFormat depthBufferFormat; // 0x04
        HWND__* windowHandle; // 0x08
        TextureData backBufferColorData; // 0x0C
        TextureData backBufferDepthData; // 0x2C
        RenderTarget backBufferRenderTarget; // 0x4C
        RenderTarget noGammaBackBufferRenderTarget; // 0xC4
        IDXGISwapChain* pSwapChain; // 0x13C
        DXGI_SWAP_CHAIN_DESC swapChainDescription; // 0x140
        ID3D11RenderTargetView* pBackBufferRenderTargetView; // 0x17C
        ID3D11RenderTargetView* pNoGammaBackBufferRenderTargetView; // 0x180
        ID3D11DepthStencilView* pBackBufferDepthView; // 0x184
        uint32 windowWidth; // 0x188
        uint32 windowHeight; // 0x18C
        uint32 presentationInterval; // 0x190
    };

    // size 0x20374
    struct GraphicsSystem : public GraphicsSystemBase
    {
        GraphicsSystem(); // 0x00C6EFE0 (:42)
        ~GraphicsSystem(); // 0x00C6ED40

        ID3D11Device* pDevice; // 0x258
        ID3D11DeviceContext* pImmediateContext; // 0x25C
        float screenAspectRatio; // 0x260
        HWND__* createdWindowHandle; // 0x264
        bool isWindowDestroyed; // 0x268
        SkinningD3D11 skinningBuffer; // 0x26C
        uint32 ownerThreadId; // 0x2C0
        uint32 frontThreadId; // 0x2C4
        GraphicsCommandBuffer immediateCommandBuffer; // 0x2C8
        GraphicsCommandBuffer deferredCommandBuffer; // 0x10304
        FragmentShader emptyFragmentShader; // 0x20340
        RenderSwapChain* pDefaultSwapChain; // 0x20344
        RenderSwapChain* pCurrentSwapChain; // 0x20348
        uint32 currentFrameNumber; // 0x2034C
        ScreenCapture* pScreenCapture; // 0x20350
        graphics::WindowMode previousFullscreenMode; // 0x20354
        graphics::WindowMode fullscreenMode; // 0x20358
        uint32 exclusiveModeWidth; // 0x2035C
        uint32 exclusiveModeHeight; // 0x20360
        uint32 exclusiveModeRefreshRateNumerator; // 0x20364
        uint32 exclusiveModeRefreshRateDenominator; // 0x20368
        uint32 windowModeWidth; // 0x2036C
        uint32 windowModeHeight; // 0x20370
    };

    namespace graphics
    {
        extern uint32 g_allocatedResources; // 0x038BCCB0, live texture count (checked for leaks on shutdown)
        extern bool s_enableDebugChecks; // 0x038BCCB4

        float getRefreshRateFloat(const DisplayMode& displayMode); // 0x008833F0 (.h:103)
        void setFlipRate(GraphicsSystem* pGraphicsSystem, uint32 rate); // 0x00458AC0 (.h:124)
    }
}
