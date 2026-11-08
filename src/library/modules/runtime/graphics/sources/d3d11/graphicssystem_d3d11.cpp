// Original: library/modules/runtime/graphics/sources/d3d11/graphicssystem_d3d11.cpp
//
// The D3D11 graphics system: device and immediate context, the render window (created here unless the caller passes
// one), swap chains with their back buffer render targets, frames, window modes, state objects, render targets,
// constant buffers, instancing buffers and deferred (recorded) command buffers.
//
// A swap chain's back buffer is R8G8B8A8_UNORM_SRGB; when it is, a second "no gamma" render target view with the
// UNORM format is created over the same texture. The depth buffer is a D24S8 texture unless the swap chain was
// created with PixelFormat_Invalid.
//
// Partial: screen capture (screencapture_d3d11.cpp) is not ported; the functions here that forward to it are.

#include "library/modules/runtime/graphics/sources/d3d11/graphicssystem_d3d11.hpp"
#include "library/modules/runtime/graphics/sources/d3d11/direct3d_d3d11.hpp"
#include "library/modules/runtime/graphics/sources/d3d11/geometryinstancing_d3d11.hpp"
#include "library/modules/runtime/graphics/sources/d3d11/screencapture_d3d11.hpp"
#include "library/modules/runtime/graphics/sources/d3d11/vertexshader_d3d11.hpp"
#include "library/modules/runtime/graphics/sources/global/shaderfactory.hpp"
#include "keen/graphics/downsampledepth.hpp"
#include "keen/graphics/vertexformat.hpp"
#include "keen/base/crc32.hpp"
#include "keen/base/string.hpp"
#include "keen/base/memoryallocator.hpp"
#include "keen/base/stringencoding.hpp"
#include "keen/base/thread.hpp"
#include "keen/base/trace.hpp"

#include <comdef.h>
#include <math.h>
#include <new>
#include <stdlib.h>
#include <string.h>

namespace keen
{
    namespace graphics
    {
        uint32 g_allocatedResources = 0;

        // File static in the original (false in the shipped build): creates the device with the D3D11 debug layer.
        // Not static here so that tests can enable the debug layer.
        bool s_enableDebugChecks = false;

        static bool s_useReferenceDevice = false; // 0x038BCCB5, WARP instead of hardware
        static bool s_forceNewWindow = false; // 0x038BCCB7

        // 0x016F0050: semantic name and index per vertex attribute id.
        struct SemanticInfo
        {
            const char* pName;
            uint32 index;
        };

        static const SemanticInfo s_pVertexAttributeSemanticInfos[VertexAttributeId_Count] =
        {
            { "POSITION", 0 }, { "NORMAL", 0 }, { "TANGENT", 0 }, { "BINORMAL", 0 },
            { "COLOR", 0 }, { "COLOR", 1 }, { "COLOR", 2 },
            { "TEXCOORD", 0 }, { "TEXCOORD", 1 }, { "TEXCOORD", 2 }, { "TEXCOORD", 3 },
            { "TEXCOORD", 4 }, { "TEXCOORD", 5 }, { "TEXCOORD", 6 }, { "TEXCOORD", 7 },
            { "BONEINDEX", 0 }, { "BONEWEIGHT", 0 },
        };

        // 0x013410D8 / 0x013411D8: D3D11 filters by [minFilter][magFilter][mipFilter] (SamplerFilterMode None,
        // Nearest, Linear, Anisotropic). Anisotropic anywhere gives the anisotropic filter.
        static const D3D11_FILTER s_d3d11Filter[4][4][4] =
        {
            {
                { D3D11_FILTER_MIN_MAG_MIP_POINT, D3D11_FILTER_MIN_MAG_MIP_POINT, D3D11_FILTER_MIN_MAG_POINT_MIP_LINEAR, D3D11_FILTER_ANISOTROPIC },
                { D3D11_FILTER_MIN_MAG_MIP_POINT, D3D11_FILTER_MIN_MAG_MIP_POINT, D3D11_FILTER_MIN_MAG_POINT_MIP_LINEAR, D3D11_FILTER_ANISOTROPIC },
                { D3D11_FILTER_MIN_POINT_MAG_LINEAR_MIP_POINT, D3D11_FILTER_MIN_POINT_MAG_LINEAR_MIP_POINT, D3D11_FILTER_MIN_POINT_MAG_MIP_LINEAR, D3D11_FILTER_ANISOTROPIC },
                { D3D11_FILTER_ANISOTROPIC, D3D11_FILTER_ANISOTROPIC, D3D11_FILTER_ANISOTROPIC, D3D11_FILTER_ANISOTROPIC },
            },
            {
                { D3D11_FILTER_MIN_MAG_MIP_POINT, D3D11_FILTER_MIN_MAG_MIP_POINT, D3D11_FILTER_MIN_MAG_POINT_MIP_LINEAR, D3D11_FILTER_ANISOTROPIC },
                { D3D11_FILTER_MIN_MAG_MIP_POINT, D3D11_FILTER_MIN_MAG_MIP_POINT, D3D11_FILTER_MIN_MAG_POINT_MIP_LINEAR, D3D11_FILTER_ANISOTROPIC },
                { D3D11_FILTER_MIN_POINT_MAG_LINEAR_MIP_POINT, D3D11_FILTER_MIN_POINT_MAG_LINEAR_MIP_POINT, D3D11_FILTER_MIN_POINT_MAG_MIP_LINEAR, D3D11_FILTER_ANISOTROPIC },
                { D3D11_FILTER_ANISOTROPIC, D3D11_FILTER_ANISOTROPIC, D3D11_FILTER_ANISOTROPIC, D3D11_FILTER_ANISOTROPIC },
            },
            {
                { D3D11_FILTER_MIN_LINEAR_MAG_MIP_POINT, D3D11_FILTER_MIN_LINEAR_MAG_MIP_POINT, D3D11_FILTER_MIN_LINEAR_MAG_POINT_MIP_LINEAR, D3D11_FILTER_ANISOTROPIC },
                { D3D11_FILTER_MIN_LINEAR_MAG_MIP_POINT, D3D11_FILTER_MIN_LINEAR_MAG_MIP_POINT, D3D11_FILTER_MIN_LINEAR_MAG_POINT_MIP_LINEAR, D3D11_FILTER_ANISOTROPIC },
                { D3D11_FILTER_MIN_MAG_LINEAR_MIP_POINT, D3D11_FILTER_MIN_MAG_LINEAR_MIP_POINT, D3D11_FILTER_MIN_MAG_MIP_LINEAR, D3D11_FILTER_ANISOTROPIC },
                { D3D11_FILTER_ANISOTROPIC, D3D11_FILTER_ANISOTROPIC, D3D11_FILTER_ANISOTROPIC, D3D11_FILTER_ANISOTROPIC },
            },
            {
                { D3D11_FILTER_ANISOTROPIC, D3D11_FILTER_ANISOTROPIC, D3D11_FILTER_ANISOTROPIC, D3D11_FILTER_ANISOTROPIC },
                { D3D11_FILTER_ANISOTROPIC, D3D11_FILTER_ANISOTROPIC, D3D11_FILTER_ANISOTROPIC, D3D11_FILTER_ANISOTROPIC },
                { D3D11_FILTER_ANISOTROPIC, D3D11_FILTER_ANISOTROPIC, D3D11_FILTER_ANISOTROPIC, D3D11_FILTER_ANISOTROPIC },
                { D3D11_FILTER_ANISOTROPIC, D3D11_FILTER_ANISOTROPIC, D3D11_FILTER_ANISOTROPIC, D3D11_FILTER_ANISOTROPIC },
            },
        };

        static const D3D11_FILTER s_d3d11FilterComparison[4][4][4] =
        {
            {
                { D3D11_FILTER_COMPARISON_MIN_MAG_MIP_POINT, D3D11_FILTER_COMPARISON_MIN_MAG_MIP_POINT, D3D11_FILTER_COMPARISON_MIN_MAG_POINT_MIP_LINEAR, D3D11_FILTER_COMPARISON_ANISOTROPIC },
                { D3D11_FILTER_COMPARISON_MIN_MAG_MIP_POINT, D3D11_FILTER_COMPARISON_MIN_MAG_MIP_POINT, D3D11_FILTER_COMPARISON_MIN_MAG_POINT_MIP_LINEAR, D3D11_FILTER_COMPARISON_ANISOTROPIC },
                { D3D11_FILTER_COMPARISON_MIN_POINT_MAG_LINEAR_MIP_POINT, D3D11_FILTER_COMPARISON_MIN_POINT_MAG_LINEAR_MIP_POINT, D3D11_FILTER_COMPARISON_MIN_POINT_MAG_MIP_LINEAR, D3D11_FILTER_COMPARISON_ANISOTROPIC },
                { D3D11_FILTER_COMPARISON_ANISOTROPIC, D3D11_FILTER_COMPARISON_ANISOTROPIC, D3D11_FILTER_COMPARISON_ANISOTROPIC, D3D11_FILTER_COMPARISON_ANISOTROPIC },
            },
            {
                { D3D11_FILTER_COMPARISON_MIN_MAG_MIP_POINT, D3D11_FILTER_COMPARISON_MIN_MAG_MIP_POINT, D3D11_FILTER_COMPARISON_MIN_MAG_POINT_MIP_LINEAR, D3D11_FILTER_COMPARISON_ANISOTROPIC },
                { D3D11_FILTER_COMPARISON_MIN_MAG_MIP_POINT, D3D11_FILTER_COMPARISON_MIN_MAG_MIP_POINT, D3D11_FILTER_COMPARISON_MIN_MAG_POINT_MIP_LINEAR, D3D11_FILTER_COMPARISON_ANISOTROPIC },
                { D3D11_FILTER_COMPARISON_MIN_POINT_MAG_LINEAR_MIP_POINT, D3D11_FILTER_COMPARISON_MIN_POINT_MAG_LINEAR_MIP_POINT, D3D11_FILTER_COMPARISON_MIN_POINT_MAG_MIP_LINEAR, D3D11_FILTER_COMPARISON_ANISOTROPIC },
                { D3D11_FILTER_COMPARISON_ANISOTROPIC, D3D11_FILTER_COMPARISON_ANISOTROPIC, D3D11_FILTER_COMPARISON_ANISOTROPIC, D3D11_FILTER_COMPARISON_ANISOTROPIC },
            },
            {
                { D3D11_FILTER_COMPARISON_MIN_LINEAR_MAG_MIP_POINT, D3D11_FILTER_COMPARISON_MIN_LINEAR_MAG_MIP_POINT, D3D11_FILTER_COMPARISON_MIN_LINEAR_MAG_POINT_MIP_LINEAR, D3D11_FILTER_COMPARISON_ANISOTROPIC },
                { D3D11_FILTER_COMPARISON_MIN_LINEAR_MAG_MIP_POINT, D3D11_FILTER_COMPARISON_MIN_LINEAR_MAG_MIP_POINT, D3D11_FILTER_COMPARISON_MIN_LINEAR_MAG_POINT_MIP_LINEAR, D3D11_FILTER_COMPARISON_ANISOTROPIC },
                { D3D11_FILTER_COMPARISON_MIN_MAG_LINEAR_MIP_POINT, D3D11_FILTER_COMPARISON_MIN_MAG_LINEAR_MIP_POINT, D3D11_FILTER_COMPARISON_MIN_MAG_MIP_LINEAR, D3D11_FILTER_COMPARISON_ANISOTROPIC },
                { D3D11_FILTER_COMPARISON_ANISOTROPIC, D3D11_FILTER_COMPARISON_ANISOTROPIC, D3D11_FILTER_COMPARISON_ANISOTROPIC, D3D11_FILTER_COMPARISON_ANISOTROPIC },
            },
            {
                { D3D11_FILTER_COMPARISON_ANISOTROPIC, D3D11_FILTER_COMPARISON_ANISOTROPIC, D3D11_FILTER_COMPARISON_ANISOTROPIC, D3D11_FILTER_COMPARISON_ANISOTROPIC },
                { D3D11_FILTER_COMPARISON_ANISOTROPIC, D3D11_FILTER_COMPARISON_ANISOTROPIC, D3D11_FILTER_COMPARISON_ANISOTROPIC, D3D11_FILTER_COMPARISON_ANISOTROPIC },
                { D3D11_FILTER_COMPARISON_ANISOTROPIC, D3D11_FILTER_COMPARISON_ANISOTROPIC, D3D11_FILTER_COMPARISON_ANISOTROPIC, D3D11_FILTER_COMPARISON_ANISOTROPIC },
                { D3D11_FILTER_COMPARISON_ANISOTROPIC, D3D11_FILTER_COMPARISON_ANISOTROPIC, D3D11_FILTER_COMPARISON_ANISOTROPIC, D3D11_FILTER_COMPARISON_ANISOTROPIC },
            },
        };

        static bool createDirect3D(GraphicsSystem* pGraphicsSystem);
        static void destroyDirect3D(GraphicsSystem* pGraphicsSystem);
        static bool createSwapChainBuffers(RenderSwapChain* pSwapChain, uint32 newWidth, uint32 newHeight);
        static void destroySwapChainBuffers(RenderSwapChain* pSwapChain);
        static D3D11_BLEND getD3dColorBlendFactor(BlendFactor blendFactor);
        static D3D11_BLEND getD3dAlphaBlendFactor(BlendFactor blendFactor);
        static D3D11_BLEND_OP getD3dBlendOperation(BlendOperation blendOp);
        static D3D11_TEXTURE_ADDRESS_MODE getD3dAddressMode(SamplerAddressMode addressMode);
        static D3D11_COMPARISON_FUNC getD3dComparisonFunction(ComparisonFunction func);
        static D3D11_STENCIL_OP getD3dStencilOperation(StencilOperation operation);

        // 0x008833F0 (graphicssystem_d3d11.hpp:103)
        float getRefreshRateFloat(const DisplayMode& displayMode)
        {
            return (float)displayMode.refreshRateNumerator / (float)displayMode.refreshRateDenominator;
        }

        // 0x00458AC0 (graphicssystem_d3d11.hpp:124)
        void setFlipRate(GraphicsSystem* pGraphicsSystem, uint32 rate)
        {
            setSwapChainPresentationInterval(pGraphicsSystem, nullptr, rate);
        }
    }

    // 0x00C0E120 (graphicssystem_d3d11.hpp:28)
    GraphicsSystemParameters::GraphicsSystemParameters()
    {
        pShaderFileSystem = nullptr;
        pWindowHandle = nullptr;
        pWindowTitle = "keen graphics window";
        hInstance = nullptr;
        nShowCmd = SW_SHOW;
        // Magic number: 64 KB immediate geometry buffer.
        immediateGeometryBufferSize = 0x10000;
        supportScaling = true;
        createDefaultSwapChain = true;
        defaultDepthBufferFormat = PixelFormat_Default_Depth;
        defaultSwapChainPresentationInterval = 1;
        pTaskSystem = nullptr;
        skinningBufferMaxInstanceCount = 0;
        skinningBufferSizeInBytes = 0;
        shaderObjectMemory.pStart = nullptr;
        shaderObjectMemory.size = 0;
    }

    // 0x004B20E0 (rendertarget.hpp:24)
    RenderTargetBuffer::RenderTargetBuffer(PixelFormat pixelFormat, TextureData* pTextureData)
    {
        format = pixelFormat;
        pDataBuffer = pTextureData;
    }

    // Inlined into RenderTarget::RenderTarget.
    RenderTargetBuffer::RenderTargetBuffer()
    {
        format = PixelFormat_None;
        pDataBuffer = nullptr;
    }

    // 0x00C615C0
    RenderTarget::RenderTarget()
    {
    }

    // 0x00C6EFE0 (:42)
    GraphicsSystem::GraphicsSystem()
    {
        emptyFragmentShader.pPixelShader = nullptr;
        pShaderFactory = nullptr;
        pSystemAllocator = nullptr;
        pDevice = nullptr;
        pImmediateContext = nullptr;
        createdWindowHandle = nullptr;
        isWindowDestroyed = false;
        ownerThreadId = 0;
        currentFrameNumber = 0;
        pDefaultSwapChain = nullptr;
        pCurrentSwapChain = nullptr;
        screenAspectRatio = 1.0f;
        previousFullscreenMode = graphics::WindowMode_Count;
    }

    // 0x00C6ED40
    GraphicsSystem::~GraphicsSystem()
    {
    }

    // 0x00C6A980 (:92)
    RenderSwapChain::RenderSwapChain()
    {
        pGraphicsSystem = nullptr;
        windowHandle = nullptr;
        pSwapChain = nullptr;
        backBufferColorData.pTexture = nullptr;
        backBufferDepthData.pTexture = nullptr;
        pBackBufferRenderTargetView = nullptr;
        pBackBufferDepthView = nullptr;
        windowWidth = 0;
        windowHeight = 0;
        presentationInterval = 0;
    }

    namespace graphics
    {
        // 0x00C5BAF0 (:146)
        static LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
        {
            if (message == WM_DESTROY)
            {
                GraphicsSystem* pGraphicsSystem = (GraphicsSystem*)GetWindowLongW(hWnd, 0);
                pGraphicsSystem->isWindowDestroyed = true;
            }
            return DefWindowProcW(hWnd, message, wParam, lParam);
        }

        // 0x00C73AB0 (:164)
        GraphicsSystem* createGraphicsSystem(MemoryAllocator* pAllocator, const GraphicsSystemParameters& parameters)
        {
            void* pMemory = pAllocator->allocate(sizeof(GraphicsSystem), 4, 0, nullptr);
            GraphicsSystem* pGraphicsSystem = pMemory != nullptr ? new (pMemory) GraphicsSystem() : nullptr;
            pGraphicsSystem->isWindowDestroyed = false;
            const uint32 threadId = getCurrentThreadId();
            pGraphicsSystem->ownerThreadId = threadId;
            pGraphicsSystem->frontThreadId = threadId;
            pGraphicsSystem->emptyFragmentShader.pPixelShader = nullptr;
            pGraphicsSystem->pScreenCapture = nullptr;
            pGraphicsSystem->previousFullscreenMode = WindowMode_Windowed;
            pGraphicsSystem->fullscreenMode = WindowMode_Windowed;
            pGraphicsSystem->exclusiveModeRefreshRateDenominator = 0;
            pGraphicsSystem->exclusiveModeRefreshRateNumerator = 0;
            pGraphicsSystem->exclusiveModeHeight = 0;
            pGraphicsSystem->exclusiveModeWidth = 0;
            if (!createDirect3D(pGraphicsSystem))
            {
                debug::nativePrintArgs("[graphics] Could not create Direct3D!\n");
                deleteObject(pGraphicsSystem, pAllocator);
                return nullptr;
            }

            // Magic numbers: state object capacities (blend, rasterizer, sampler, depth/stencil, vertex format and
            // 0x400 vertex input bindings, more than the pool defaults).
            CombinedGraphicsStateObjectPoolParameters poolParameters;
            poolParameters.stateObjectCapacity[GraphicsStateObjectType_BlendState] = 0x100;
            poolParameters.stateObjectCapacity[GraphicsStateObjectType_RasterizerState] = 0x40;
            poolParameters.stateObjectCapacity[GraphicsStateObjectType_SamplerState] = 0x20;
            poolParameters.stateObjectCapacity[GraphicsStateObjectType_DepthStencilState] = 0x80;
            poolParameters.stateObjectCapacity[GraphicsStateObjectType_VertexFormat] = 0x40;
            poolParameters.stateObjectCapacity[GraphicsStateObjectType_VertexInputBinding] = 0x400;
            MemoryBlock noShaderCodeMemory;
            noShaderCodeMemory.pStart = nullptr;
            noShaderCodeMemory.size = 0;
            createGraphicsSystemBase(pGraphicsSystem, pAllocator, parameters.pShaderFileSystem, parameters.shaderObjectMemory, noShaderCodeMemory, poolParameters);

            if (!parameters.createDefaultSwapChain)
            {
                pGraphicsSystem->pDefaultSwapChain = nullptr;
            }
            else
            {
                if (parameters.pWindowHandle == nullptr || s_forceNewWindow)
                {
                    WNDCLASSEXW windowClass;
                    windowClass.cbSize = sizeof(WNDCLASSEXW);
                    windowClass.style = CS_HREDRAW | CS_VREDRAW;
                    windowClass.lpfnWndProc = WndProc;
                    windowClass.cbClsExtra = 0;
                    windowClass.cbWndExtra = sizeof(GraphicsSystem*);
                    windowClass.hInstance = (HINSTANCE)parameters.hInstance;
                    windowClass.hIcon = LoadIconW(windowClass.hInstance, MAKEINTRESOURCEW(32512)) /* IDI_APPLICATION */;
                    windowClass.hCursor = LoadCursorW(nullptr, MAKEINTRESOURCEW(32512)) /* IDC_ARROW */;
                    windowClass.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
                    windowClass.lpszMenuName = nullptr;
                    windowClass.lpszClassName = L"keen_win32_d3d11_renderwindow";
                    windowClass.hIconSm = LoadIconW(windowClass.hInstance, MAKEINTRESOURCEW(32512)) /* IDI_APPLICATION */;
                    if (RegisterClassExW(&windowClass) == 0)
                    {
                        debug::nativePrintArgs("Call to RegisterClassEx failed!");
                        deleteObject(pGraphicsSystem, pGraphicsSystem->pSystemAllocator);
                        return nullptr;
                    }
                    WCHAR windowTitle[256];
                    convertUTF8ToUTF16((uint16*)windowTitle, sizeof(windowTitle), parameters.pWindowTitle);
                    pGraphicsSystem->createdWindowHandle = CreateWindowExW(0, L"keen_win32_d3d11_renderwindow", windowTitle, WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, nullptr, nullptr, windowClass.hInstance, nullptr);
                    if (pGraphicsSystem->createdWindowHandle == nullptr)
                    {
                        debug::nativePrintArgs("Call to CreateWindow failed!");
                        deleteObject(pGraphicsSystem, pGraphicsSystem->pSystemAllocator);
                        return nullptr;
                    }
                    SetWindowLongW(pGraphicsSystem->createdWindowHandle, 0, (LONG)pGraphicsSystem);
                    ShowWindow(pGraphicsSystem->createdWindowHandle, parameters.nShowCmd);
                }
                else
                {
                    pGraphicsSystem->createdWindowHandle = nullptr;
                }
                void* pWindowHandle = pGraphicsSystem->createdWindowHandle != nullptr ? (void*)pGraphicsSystem->createdWindowHandle : parameters.pWindowHandle;
                pGraphicsSystem->pDefaultSwapChain = createRenderSwapChain(pGraphicsSystem, pWindowHandle, parameters.defaultDepthBufferFormat, parameters.defaultSwapChainPresentationInterval, 0, 0);
                if (pGraphicsSystem->pDefaultSwapChain == nullptr)
                {
                    debug::nativePrintArgs("[graphics] Could not create default swap chain!\n");
                    if (pGraphicsSystem->createdWindowHandle != nullptr)
                    {
                        DestroyWindow(pGraphicsSystem->createdWindowHandle);
                    }
                    deleteObject(pGraphicsSystem, pGraphicsSystem->pSystemAllocator);
                    return nullptr;
                }
            }

            HWND clientWindow = pGraphicsSystem->createdWindowHandle != nullptr ? pGraphicsSystem->createdWindowHandle : (HWND)parameters.pWindowHandle;
            RECT clientRect;
            GetClientRect(clientWindow, &clientRect);
            pGraphicsSystem->windowModeWidth = clientRect.right - clientRect.left;
            pGraphicsSystem->windowModeHeight = clientRect.bottom - clientRect.top;

            pGraphicsSystem->skinningBuffer.create(pGraphicsSystem->pSystemAllocator, pGraphicsSystem, parameters.pTaskSystem, parameters.skinningBufferMaxInstanceCount, parameters.skinningBufferSizeInBytes);
            pGraphicsSystem->immediateCommandBuffer.pContext = pGraphicsSystem->pImmediateContext;
            pGraphicsSystem->immediateCommandBuffer.pSkinningBuffer = &pGraphicsSystem->skinningBuffer;
            pGraphicsSystem->immediateCommandBuffer.pRenderCommandBufferStorage = nullptr;

            D3D11_BUFFER_DESC bufferDesc;
            bufferDesc.ByteWidth = parameters.immediateGeometryBufferSize;
            bufferDesc.Usage = D3D11_USAGE_DYNAMIC;
            bufferDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
            bufferDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
            bufferDesc.MiscFlags = 0;
            bufferDesc.StructureByteStride = 0;
            const HRESULT result = pGraphicsSystem->pDevice->CreateBuffer(&bufferDesc, nullptr, &pGraphicsSystem->immediateCommandBuffer.pImmediateVertexData);
            if (FAILED(result))
            {
                debug::nativePrintArgs("[graphics] Could not create immediate geometry vertex buffer (%x)!\n", result);
                if (pGraphicsSystem->createdWindowHandle != nullptr)
                {
                    DestroyWindow(pGraphicsSystem->createdWindowHandle);
                }
                deleteObject(pGraphicsSystem, pGraphicsSystem->pSystemAllocator);
                return nullptr;
            }

            pGraphicsSystem->stockObjects.create(pGraphicsSystem, nullptr);
            pGraphicsSystem->immediateCommandBuffer.immediateVertexBufferOffset = 0;
            pGraphicsSystem->immediateCommandBuffer.immediateVertexBufferSize = parameters.immediateGeometryBufferSize;
            pGraphicsSystem->immediateCommandBuffer.immediateVertexDataStride = 0;
            pGraphicsSystem->immediateCommandBuffer.pMappedConstantBuffer = nullptr;
            pGraphicsSystem->immediateCommandBuffer.pDownsampleDepthContext = createDownsampleDepthContext(pGraphicsSystem->pSystemAllocator, pGraphicsSystem, ColorWriteMask_All);
            pGraphicsSystem->immediateCommandBuffer.pRenderCommandBufferStorage = nullptr;
            pGraphicsSystem->currentFrameNumber = 0;
            if (pGraphicsSystem->createdWindowHandle != nullptr)
            {
                UpdateWindow(pGraphicsSystem->createdWindowHandle);
            }
            return pGraphicsSystem;
        }

        // 0x00C5BB30 (:329)
        uint32 getCurrentFrameNumber(const GraphicsSystem* pGraphicsSystem)
        {
            return pGraphicsSystem->currentFrameNumber;
        }

        // 0x00C72E80 (:336)
        void destroyGraphicsSystem(GraphicsSystem* pGraphicsSystem)
        {
            pGraphicsSystem->immediateCommandBuffer.pImmediateVertexData->Release();
            pGraphicsSystem->immediateCommandBuffer.pImmediateVertexData = nullptr;
            destroyDownsampleDepthContext(pGraphicsSystem->immediateCommandBuffer.pDownsampleDepthContext, pGraphicsSystem->pSystemAllocator, pGraphicsSystem);
            pGraphicsSystem->immediateCommandBuffer.pDownsampleDepthContext = nullptr;
            pGraphicsSystem->skinningBuffer.destroy(pGraphicsSystem->pSystemAllocator);
            pGraphicsSystem->immediateCommandBuffer.pContext = nullptr;
            pGraphicsSystem->pImmediateContext->ClearState();
            pGraphicsSystem->pImmediateContext->Flush();
            pGraphicsSystem->stockObjects.destroy(pGraphicsSystem, nullptr);
            pGraphicsSystem->stateObjectPool.destroy(pGraphicsSystem->pSystemAllocator);
            destroyShaderFactory(pGraphicsSystem->pShaderFactory, pGraphicsSystem->pSystemAllocator);
            destroyDirect3D(pGraphicsSystem);
            if (pGraphicsSystem->createdWindowHandle != nullptr)
            {
                DestroyWindow(pGraphicsSystem->createdWindowHandle);
                pGraphicsSystem->createdWindowHandle = nullptr;
            }
            deleteObject(pGraphicsSystem, pGraphicsSystem->pSystemAllocator);
            if (g_allocatedResources != 0)
            {
                debug::nativePrintArgs("[graphics] Still have %d resources allocated. Check for Resource leaks.", g_allocatedResources);
            }
        }

        // 0x00C5BB40 (:371)
        void handleWindowMessages(GraphicsSystem* pGraphicsSystem)
        {
            MSG message;
            while (PeekMessageW(&message, pGraphicsSystem->createdWindowHandle, 0, 0, PM_REMOVE))
            {
                TranslateMessage(&message);
                DispatchMessageW(&message);
            }
        }

        // 0x00C5BBB0 (:381)
        bool isWindowDestroyed(const GraphicsSystem* pGraphicsSystem)
        {
            return pGraphicsSystem->isWindowDestroyed;
        }

        static void releaseSwapChain(RenderSwapChain* pSwapChain)
        {
            destroySwapChainBuffers(pSwapChain);
            if (pSwapChain->pSwapChain != nullptr)
            {
                pSwapChain->pSwapChain->Release();
                pSwapChain->pSwapChain = nullptr;
            }
        }

        // 0x00C6DA10 (:386)
        RenderSwapChain* createRenderSwapChain(GraphicsSystem* pGraphicsSystem, void* pWindowHandle, PixelFormat depthBufferFormat, uint32 presentationInterval, uint32 width, uint32 height)
        {
            void* pMemory = pGraphicsSystem->pSystemAllocator->allocate(sizeof(RenderSwapChain), 4, 0, nullptr);
            RenderSwapChain* pSwapChain = pMemory != nullptr ? new (pMemory) RenderSwapChain() : nullptr;
            pSwapChain->pGraphicsSystem = pGraphicsSystem;
            pSwapChain->depthBufferFormat = depthBufferFormat;
            pSwapChain->windowHandle = (HWND)pWindowHandle;
            pSwapChain->presentationInterval = presentationInterval;

            RECT clientRect;
            if (!GetClientRect((HWND)pWindowHandle, &clientRect) && clientRect.right > 0 && clientRect.bottom > 0)
            {
                // Notes (decompiled behaviour, kept as is): the 100x100 fallback is only taken when GetClientRect
                // failed and still left a positive rectangle behind; otherwise its (possibly stale) values are used.
                pSwapChain->windowWidth = 100;
                pSwapChain->windowHeight = 100;
            }
            else
            {
                pSwapChain->windowWidth = clientRect.right;
                pSwapChain->windowHeight = clientRect.bottom;
            }
            if (width == 0)
            {
                width = pSwapChain->windowWidth;
            }
            if (height == 0)
            {
                height = pSwapChain->windowHeight;
            }

            DXGI_SWAP_CHAIN_DESC& description = pSwapChain->swapChainDescription;
            memset(&description, 0, sizeof(description));
            description.BufferDesc.Width = width;
            description.BufferDesc.Height = height;
            // Magic number: 60 Hz.
            description.BufferDesc.RefreshRate.Numerator = 60;
            description.BufferDesc.RefreshRate.Denominator = 1;
            description.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
            description.BufferDesc.ScanlineOrdering = DXGI_MODE_SCANLINE_ORDER_UNSPECIFIED;
            description.BufferDesc.Scaling = DXGI_MODE_SCALING_UNSPECIFIED;
            description.SampleDesc.Count = 1;
            description.SampleDesc.Quality = 0;
            description.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
            description.BufferCount = 1;
            description.OutputWindow = pSwapChain->windowHandle;
            description.Windowed = TRUE;
            description.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;
            description.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;

            IDXGIDevice* pDxgiDevice = nullptr;
            IDXGIAdapter* pAdapter = nullptr;
            IDXGIFactory* pFactory = nullptr;
            HRESULT result = pGraphicsSystem->pDevice->QueryInterface(__uuidof(IDXGIDevice), (void**)&pDxgiDevice);
            if (SUCCEEDED(result))
            {
                result = pDxgiDevice->GetParent(__uuidof(IDXGIAdapter), (void**)&pAdapter);
            }
            if (SUCCEEDED(result))
            {
                result = pAdapter->GetParent(__uuidof(IDXGIFactory), (void**)&pFactory);
            }
            if (SUCCEEDED(result))
            {
                result = pFactory->CreateSwapChain(pGraphicsSystem->pDevice, &description, &pSwapChain->pSwapChain);
            }
            if (SUCCEEDED(result))
            {
                // Notes (decompiled behaviour, kept as is): this second factory reference is never released.
                IDXGIFactory1* pSwapChainFactory = nullptr;
                if (SUCCEEDED(pSwapChain->pSwapChain->GetParent(__uuidof(IDXGIFactory1), (void**)&pSwapChainFactory)))
                {
                    pSwapChainFactory->MakeWindowAssociation((HWND)pWindowHandle, DXGI_MWA_NO_ALT_ENTER);
                }
            }
            if (pFactory != nullptr)
            {
                pFactory->Release();
            }
            if (pAdapter != nullptr)
            {
                pAdapter->Release();
            }
            if (pDxgiDevice != nullptr)
            {
                pDxgiDevice->Release();
            }
            if (FAILED(result))
            {
                releaseSwapChain(pSwapChain);
                pSwapChain->pGraphicsSystem->pSystemAllocator->free(pSwapChain);
                debug::nativePrintArgs("[graphics] createRenderSwapChain failed! result = 0x%08x!\n", result);
                return nullptr;
            }
            if (!createSwapChainBuffers(pSwapChain, width, height))
            {
                releaseSwapChain(pSwapChain);
                pSwapChain->pGraphicsSystem->pSystemAllocator->free(pSwapChain);
                return nullptr;
            }
            return pSwapChain;
        }

        // 0x00C6AAE0 (:497)
        void destroyRenderSwapChain(RenderSwapChain* pSwapChain)
        {
            if (pSwapChain != nullptr)
            {
                releaseSwapChain(pSwapChain);
                pSwapChain->pGraphicsSystem->pSystemAllocator->free(pSwapChain);
            }
        }

        // 0x00C5BBC0 (:513)
        void setSwapChainPresentationInterval(GraphicsSystem* pGraphicsSystem, RenderSwapChain* pSwapChain, uint32 presentationInterval)
        {
            if (pSwapChain == nullptr)
            {
                pSwapChain = pGraphicsSystem->pDefaultSwapChain;
            }
            if (pSwapChain != nullptr)
            {
                pSwapChain->presentationInterval = presentationInterval;
            }
        }

        // 0x00C6AB20 (:525)
        bool resizeRenderSwapChain(RenderSwapChain* pSwapChain, uint32 newWidth, uint32 newHeight)
        {
            if (newWidth == 0 || newHeight == 0)
            {
                return false;
            }
            DXGI_SWAP_CHAIN_DESC& description = pSwapChain->swapChainDescription;
            if (description.BufferDesc.Width == newWidth && description.BufferDesc.Height == newHeight)
            {
                return true;
            }
            destroySwapChainBuffers(pSwapChain);
            pSwapChain->windowWidth = newWidth;
            pSwapChain->windowHeight = newHeight;
            const HRESULT result = pSwapChain->pSwapChain->ResizeBuffers(description.BufferCount, newWidth, newHeight, description.BufferDesc.Format, description.Flags);
            if (FAILED(result))
            {
                handleFatalHResult(nullptr, result, "ResizeBuffers");
            }
            if (!createSwapChainBuffers(pSwapChain, newWidth, newHeight))
            {
                debug::nativePrintArgs("[graphics] Could not create new swap chain!\n");
                return false;
            }
            description.BufferDesc.Width = newWidth;
            description.BufferDesc.Height = newHeight;
            return true;
        }

        // 0x00C6DD00 (:567)
        GraphicsCommandBuffer* beginFrame(GraphicsSystem* pGraphicsSystem)
        {
            if (pGraphicsSystem->pDefaultSwapChain == nullptr)
            {
                debug::nativePrintArgs("[graphics] No default swap chain created!\n");
                return nullptr;
            }
            RECT clientRect;
            if (!GetClientRect(pGraphicsSystem->pDefaultSwapChain->windowHandle, &clientRect))
            {
                // Magic numbers: 100x100 fallback client size.
                SetRect(&clientRect, 0, 0, 100, 100);
            }
            if (!resizeRenderSwapChain(pGraphicsSystem->pDefaultSwapChain, clientRect.right - clientRect.left, clientRect.bottom - clientRect.top))
            {
                debug::nativePrintArgs("[graphics] Could not resize RenderSwapChain!\n");
                return nullptr;
            }
            return beginFrame(pGraphicsSystem, pGraphicsSystem->pDefaultSwapChain);
        }

        // 0x00C6ABD0 (:590)
        GraphicsCommandBuffer* beginFrame(GraphicsSystem* pGraphicsSystem, RenderSwapChain* pSwapChain)
        {
            if (pGraphicsSystem->createdWindowHandle != nullptr)
            {
                handleWindowMessages(pGraphicsSystem);
            }
            if (pGraphicsSystem->isWindowDestroyed)
            {
                return nullptr;
            }
            pGraphicsSystem->skinningBuffer.flip();
            ++pGraphicsSystem->currentFrameNumber;
            pGraphicsSystem->pCurrentSwapChain = pSwapChain;
            pGraphicsSystem->immediateCommandBuffer.pContext->ClearState();
            pGraphicsSystem->immediateCommandBuffer.pCurrentRenderTarget = nullptr;
            pGraphicsSystem->immediateCommandBuffer.pMappedConstantBuffer = nullptr;
            return &pGraphicsSystem->immediateCommandBuffer;
        }

        // 0x00C70320 (:620)
        bool endFrame(GraphicsSystem* pGraphicsSystem)
        {
            pGraphicsSystem->immediateCommandBuffer.pContext->ClearState();
            if (pGraphicsSystem->isWindowDestroyed)
            {
                pGraphicsSystem->pCurrentSwapChain = nullptr;
                return false;
            }
            if (pGraphicsSystem->pScreenCapture != nullptr)
            {
                captureFrame(pGraphicsSystem->pScreenCapture, &pGraphicsSystem->immediateCommandBuffer, &pGraphicsSystem->pCurrentSwapChain->backBufferRenderTarget);
            }
            RenderSwapChain* pSwapChain = pGraphicsSystem->pCurrentSwapChain;
            if (pSwapChain != nullptr)
            {
                const HRESULT result = pSwapChain->pSwapChain->Present(pSwapChain->presentationInterval, 0);
                if (FAILED(result))
                {
                    handleFatalHResult(pGraphicsSystem, result, "Present");
                }
                pGraphicsSystem->pCurrentSwapChain = nullptr;
            }
            if (pGraphicsSystem->createdWindowHandle != nullptr)
            {
                handleWindowMessages(pGraphicsSystem);
            }
            return true;
        }

        static bool isWindowWithoutFrame(HWND windowHandle)
        {
            return (GetWindowLongW(windowHandle, GWL_STYLE) & WS_OVERLAPPEDWINDOW) == 0;
        }

        // 0x00C6DDD0 (:657)
        bool switchToFullScreenMode(GraphicsSystem* pGraphicsSystem, RenderSwapChain* pSwapChain, uint32 width, uint32 height, uint32 refreshRateNumerator, uint32 refreshRateDenominator, WindowMode windowMode)
        {
            debug::nativePrintArgs("switchToFullScreenMode : %dx%d @ %.2f Hz, %d\n", width, height, (double)refreshRateNumerator / (double)refreshRateDenominator, windowMode);
            if (pSwapChain == nullptr)
            {
                pSwapChain = pGraphicsSystem->pDefaultSwapChain;
                if (pSwapChain == nullptr)
                {
                    return false;
                }
            }
            const WindowMode currentMode = getCurrentWindowMode(pGraphicsSystem, pSwapChain);
            pGraphicsSystem->previousFullscreenMode = windowMode;
            pGraphicsSystem->fullscreenMode = windowMode;
            if (windowMode == WindowMode_Borderless)
            {
                if (currentMode == WindowMode_Borderless)
                {
                    return false;
                }
            }
            else if (windowMode == WindowMode_Fullscreen && currentMode == WindowMode_Fullscreen && width == getBackBufferWidth(pGraphicsSystem) && height == getBackBufferHeight(pGraphicsSystem)
                && refreshRateNumerator == pGraphicsSystem->exclusiveModeRefreshRateNumerator && refreshRateDenominator == pGraphicsSystem->exclusiveModeRefreshRateDenominator)
            {
                return false;
            }

            bool foundMode = false;
            DXGI_MODE_DESC closestMode;
            memset(&closestMode, 0, sizeof(closestMode));
            if (windowMode == WindowMode_Windowed || windowMode == WindowMode_Count)
            {
                windowMode = WindowMode_Borderless;
            }
            if (isInFullScreenMode(pGraphicsSystem, pSwapChain) || (pSwapChain->windowHandle != nullptr && isWindowWithoutFrame(pSwapChain->windowHandle)))
            {
                switchToWindowedMode(pGraphicsSystem, pSwapChain);
            }
            pGraphicsSystem->previousFullscreenMode = windowMode;
            pGraphicsSystem->fullscreenMode = windowMode;

            if (windowMode == WindowMode_Borderless)
            {
                HWND windowHandle = pGraphicsSystem->pDefaultSwapChain->windowHandle;
                const LONG style = GetWindowLongW(windowHandle, GWL_STYLE);
                if ((style & WS_OVERLAPPEDWINDOW) != 0)
                {
                    SetWindowLongW(windowHandle, GWL_STYLE, style & ~WS_OVERLAPPEDWINDOW);
                    SetWindowPos(windowHandle, nullptr, 0, 0, GetSystemMetrics(SM_CXSCREEN), GetSystemMetrics(SM_CYSCREEN), SWP_NOZORDER | SWP_FRAMECHANGED);
                    return true;
                }
            }
            else if (windowMode == WindowMode_Fullscreen)
            {
                if (width != 0 && height != 0)
                {
                    DXGI_MODE_DESC targetMode;
                    memset(&targetMode, 0, sizeof(targetMode));
                    targetMode.Width = width;
                    targetMode.Height = height;
                    targetMode.RefreshRate.Numerator = refreshRateNumerator;
                    targetMode.RefreshRate.Denominator = refreshRateDenominator;
                    IDXGIOutput* pOutput = nullptr;
                    if (pSwapChain->pSwapChain->GetContainingOutput(&pOutput) == S_OK)
                    {
                        pOutput->FindClosestMatchingMode(&targetMode, &closestMode, pGraphicsSystem->pDevice);
                        pOutput->Release();
                        foundMode = true;
                    }
                }
                HRESULT result = pSwapChain->pSwapChain->SetFullscreenState(TRUE, nullptr);
                if (FAILED(result))
                {
                    debug::nativePrintArgs("[graphics] could not switch to full screen mode! result = 0x%08x!\n", result);
                    return false;
                }
                if (foundMode)
                {
                    result = pSwapChain->pSwapChain->ResizeTarget(&closestMode);
                    if (FAILED(result))
                    {
                        debug::nativePrintArgs("[graphics] failed to set target resolution! result = 0x%08x!\n", result);
                    }
                    else
                    {
                        pGraphicsSystem->exclusiveModeWidth = closestMode.Width;
                        pGraphicsSystem->exclusiveModeHeight = closestMode.Height;
                        pGraphicsSystem->exclusiveModeRefreshRateNumerator = closestMode.RefreshRate.Numerator;
                        pGraphicsSystem->exclusiveModeRefreshRateDenominator = closestMode.RefreshRate.Denominator;
                    }
                    debug::nativePrintArgs("Resized Target to %d x %d: %x\n", closestMode.Width, closestMode.Height, result);
                }
                return true;
            }
            return false;
        }

        // 0x00C5BBF0 (:774)
        void setWindowSize(GraphicsSystem* pGraphicsSystem, uint32 windowWidth, uint32 windowHeight, RenderSwapChain* pSwapChain)
        {
            if (pSwapChain == nullptr)
            {
                pSwapChain = pGraphicsSystem->pDefaultSwapChain;
                if (pSwapChain == nullptr)
                {
                    return;
                }
            }
            // Notes (decompiled behaviour, kept as is): the style is read from the default swap chain's window.
            const LONG style = GetWindowLongW(pGraphicsSystem->pDefaultSwapChain->windowHandle, GWL_STYLE) | WS_OVERLAPPEDWINDOW;
            SetWindowLongW(pSwapChain->windowHandle, GWL_STYLE, style);
            RECT windowRect;
            windowRect.left = 0;
            windowRect.top = 0;
            windowRect.right = windowWidth;
            windowRect.bottom = windowHeight;
            AdjustWindowRect(&windowRect, style, FALSE);
            SetWindowPos(pSwapChain->windowHandle, nullptr, 0, 0, windowRect.right - windowRect.left, windowRect.bottom - windowRect.top, SWP_NOMOVE | SWP_NOZORDER | SWP_FRAMECHANGED);
        }

        // 0x00C6AC70 (:798)
        bool switchToWindowedMode(GraphicsSystem* pGraphicsSystem, RenderSwapChain* pSwapChain)
        {
            if (pSwapChain == nullptr)
            {
                pSwapChain = pGraphicsSystem->pDefaultSwapChain;
                if (pSwapChain == nullptr)
                {
                    return false;
                }
            }
            const WindowMode currentMode = getCurrentWindowMode(pGraphicsSystem, pSwapChain);
            pGraphicsSystem->fullscreenMode = WindowMode_Windowed;
            if (currentMode == WindowMode_Windowed)
            {
                return false;
            }
            if (isInFullScreenMode(pGraphicsSystem, pSwapChain))
            {
                const HRESULT result = pSwapChain->pSwapChain->SetFullscreenState(FALSE, nullptr);
                if (FAILED(result))
                {
                    debug::nativePrintArgs("[graphics] could not switch to windowed mode! result = 0x%08x!\n", result);
                    return false;
                }
            }
            else
            {
                setWindowSize(pGraphicsSystem, pGraphicsSystem->windowModeWidth, pGraphicsSystem->windowModeHeight, nullptr);
            }
            return true;
        }

        // 0x00C6E100 (:837)
        bool toggleWindowMode(GraphicsSystem* pGraphicsSystem, RenderSwapChain* pSwapChain)
        {
            if (pSwapChain == nullptr)
            {
                pSwapChain = pGraphicsSystem->pDefaultSwapChain;
                if (pSwapChain == nullptr)
                {
                    return false;
                }
            }
            const WindowMode currentMode = getCurrentWindowMode(pGraphicsSystem, pSwapChain);
            if (currentMode == WindowMode_Windowed)
            {
                return switchToFullScreenMode(pGraphicsSystem, pSwapChain, pGraphicsSystem->exclusiveModeWidth, pGraphicsSystem->exclusiveModeHeight, pGraphicsSystem->exclusiveModeRefreshRateNumerator, pGraphicsSystem->exclusiveModeRefreshRateDenominator, pGraphicsSystem->previousFullscreenMode);
            }
            if (currentMode != WindowMode_Count)
            {
                return switchToWindowedMode(pGraphicsSystem, pSwapChain);
            }
            return false;
        }

        // 0x00C61600 (:862)
        WindowMode getCurrentWindowMode(const GraphicsSystem* pGraphicsSystem, const RenderSwapChain* pSwapChain)
        {
            if (pSwapChain == nullptr)
            {
                pSwapChain = pGraphicsSystem->pDefaultSwapChain;
            }
            if (isInFullScreenMode(pGraphicsSystem, pSwapChain))
            {
                return WindowMode_Fullscreen;
            }
            if (pSwapChain == nullptr)
            {
                pSwapChain = pGraphicsSystem->pDefaultSwapChain;
            }
            if (pSwapChain != nullptr && pSwapChain->windowHandle != nullptr && isWindowWithoutFrame(pSwapChain->windowHandle))
            {
                return WindowMode_Borderless;
            }
            return WindowMode_Windowed;
        }

        // 0x00C5BCB0 (:883)
        bool isInBorderlessMode(const GraphicsSystem* pGraphicsSystem, const RenderSwapChain* pSwapChain)
        {
            if (pSwapChain == nullptr)
            {
                pSwapChain = pGraphicsSystem->pDefaultSwapChain;
            }
            if (pSwapChain != nullptr && pSwapChain->windowHandle != nullptr)
            {
                return isWindowWithoutFrame(pSwapChain->windowHandle);
            }
            return false;
        }

        // 0x00C5BCF0 (:903)
        bool isInFullScreenMode(const GraphicsSystem* pGraphicsSystem, const RenderSwapChain* pSwapChain)
        {
            if (pSwapChain == nullptr)
            {
                pSwapChain = pGraphicsSystem->pDefaultSwapChain;
                if (pSwapChain == nullptr)
                {
                    return false;
                }
            }
            BOOL fullscreen = FALSE;
            IDXGIOutput* pTarget = nullptr;
            const HRESULT result = pSwapChain->pSwapChain->GetFullscreenState(&fullscreen, &pTarget);
            if (FAILED(result))
            {
                debug::nativePrintArgs("[graphics] could not get full screen mode! result = 0x%08x!\n", result);
                return false;
            }
            // Notes (decompiled behaviour, kept as is): the returned output reference is not released.
            return fullscreen == TRUE;
        }

        // 0x00C6E170 (:928)
        void regainFocus(GraphicsSystem* pGraphicsSystem)
        {
            if (pGraphicsSystem->fullscreenMode == WindowMode_Fullscreen && getCurrentWindowMode(pGraphicsSystem, pGraphicsSystem->pCurrentSwapChain) != WindowMode_Fullscreen)
            {
                switchToFullScreenMode(pGraphicsSystem, pGraphicsSystem->pCurrentSwapChain, pGraphicsSystem->exclusiveModeWidth, pGraphicsSystem->exclusiveModeHeight, pGraphicsSystem->exclusiveModeRefreshRateNumerator, pGraphicsSystem->exclusiveModeRefreshRateDenominator, WindowMode_Fullscreen);
            }
        }

        // 0x00C61670 (:937)
        uint32 enumerateDisplayModes(DisplayMode* pDisplayModes, uint32 displayModeCount, GraphicsSystem* pGraphicsSystem, RenderSwapChain* pSwapChain)
        {
            if (pSwapChain == nullptr)
            {
                pSwapChain = pGraphicsSystem->pDefaultSwapChain;
                if (pSwapChain == nullptr)
                {
                    return 0;
                }
            }
            IDXGIOutput* pOutput = nullptr;
            if (pSwapChain->pSwapChain->GetContainingOutput(&pOutput) != S_OK)
            {
                return 0;
            }
            MemoryAllocator* pAllocator = pGraphicsSystem->pSystemAllocator;
            DXGI_MODE_DESC* pModes = nullptr;
            UINT modeCount = 0;
            for (;;)
            {
                modeCount = 0;
                if (FAILED(pOutput->GetDisplayModeList(DXGI_FORMAT_R8G8B8A8_UNORM_SRGB, 0, &modeCount, nullptr)))
                {
                    break;
                }
                pModes = (DXGI_MODE_DESC*)pAllocator->allocate(modeCount * sizeof(DXGI_MODE_DESC), 16, 0, nullptr);
                if (pModes == nullptr)
                {
                    break;
                }
                const HRESULT result = pOutput->GetDisplayModeList(DXGI_FORMAT_R8G8B8A8_UNORM_SRGB, 0, &modeCount, pModes);
                if (SUCCEEDED(result))
                {
                    break;
                }
                pAllocator->free(pModes);
                pModes = nullptr;
                if (result != DXGI_ERROR_MORE_DATA)
                {
                    break;
                }
            }
            if (pModes == nullptr)
            {
                debug::nativePrintArgs("[graphicssystem] Getting DisplayModeList failed.\n");
                pOutput->Release();
                return 0;
            }

            // Modes that only differ in scanline ordering or scaling, or whose refresh rates are equal within 0.1%,
            // are listed once.
            uint32 count = 0;
            for (UINT i = 0; i < modeCount; ++i)
            {
                if (count >= displayModeCount)
                {
                    continue;
                }
                const DXGI_MODE_DESC& mode = pModes[i];
                const float refreshRate = (float)mode.RefreshRate.Numerator / (float)mode.RefreshRate.Denominator;
                bool isDuplicate = false;
                for (uint32 j = 0; j < count; ++j)
                {
                    if (pDisplayModes[j].width == mode.Width && pDisplayModes[j].height == mode.Height)
                    {
                        const float otherRate = (float)pDisplayModes[j].refreshRateNumerator / (float)pDisplayModes[j].refreshRateDenominator;
                        float tolerance = fabsf(refreshRate) > fabsf(otherRate) ? fabsf(refreshRate) : fabsf(otherRate);
                        // Magic numbers: relative tolerance 0.001, at least 0.001.
                        tolerance *= 0.001f;
                        if (tolerance < 0.001f)
                        {
                            tolerance = 0.001f;
                        }
                        if (fabsf(refreshRate - otherRate) <= tolerance)
                        {
                            isDuplicate = true;
                        }
                    }
                }
                if (isDuplicate)
                {
                    continue;
                }
                pDisplayModes[count].width = mode.Width;
                pDisplayModes[count].height = mode.Height;
                pDisplayModes[count].refreshRateNumerator = mode.RefreshRate.Numerator;
                pDisplayModes[count].refreshRateDenominator = mode.RefreshRate.Denominator;
                ++count;
            }
            pAllocator->free(pModes);
            pOutput->Release();
            return count;
        }

        // 0x00C5BD50 (:1042)
        bool isDeviceLost(const GraphicsSystem* pGraphicsSystem)
        {
            return false;
        }

        // 0x00C5BD60 (:1051)
        bool resetDevice(GraphicsSystem* pGraphicsSystem)
        {
            pGraphicsSystem->immediateCommandBuffer.pContext->ClearState();
            pGraphicsSystem->immediateCommandBuffer.pContext->Flush();
            return true;
        }

        // 0x00C61930 (:1061)
        const RenderTarget* getBackBufferRenderTarget(const GraphicsSystem* pGraphicsSystem)
        {
            if (pGraphicsSystem->pScreenCapture != nullptr)
            {
                return pGraphicsSystem->pScreenCapture->pCaptureTarget;
            }
            if (pGraphicsSystem->pCurrentSwapChain != nullptr)
            {
                return &pGraphicsSystem->pCurrentSwapChain->backBufferRenderTarget;
            }
            if (pGraphicsSystem->pDefaultSwapChain != nullptr)
            {
                return &pGraphicsSystem->pDefaultSwapChain->backBufferRenderTarget;
            }
            return nullptr;
        }

        // 0x00C61970 (:1083)
        const RenderTarget* getNoGammaBackBufferRenderTarget(const GraphicsSystem* pGraphicsSystem)
        {
            if (pGraphicsSystem->pScreenCapture != nullptr)
            {
                return pGraphicsSystem->pScreenCapture->pNoGammaCaptureTarget;
            }
            if (pGraphicsSystem->pCurrentSwapChain != nullptr)
            {
                return &pGraphicsSystem->pCurrentSwapChain->noGammaBackBufferRenderTarget;
            }
            if (pGraphicsSystem->pDefaultSwapChain != nullptr)
            {
                return &pGraphicsSystem->pDefaultSwapChain->noGammaBackBufferRenderTarget;
            }
            return nullptr;
        }

        // 0x00C5BD90 (:1104)
        uint32 getRenderTargetWidth(const RenderTarget* pRenderTarget)
        {
            return pRenderTarget != nullptr ? pRenderTarget->width : 0;
        }

        // 0x00C5BDA0 (:1113)
        uint32 getRenderTargetHeight(const RenderTarget* pRenderTarget)
        {
            return pRenderTarget != nullptr ? pRenderTarget->height : 0;
        }

        // 0x00C5BDB0 (:1122)
        const TextureData* getRenderTargetColorBuffer(const RenderTarget* pRenderTarget, uint32 index)
        {
            if (index < pRenderTarget->colorBufferCount)
            {
                return pRenderTarget->colorBuffers[index].pDataBuffer;
            }
            return nullptr;
        }

        // 0x00C5BDD0 (:1135)
        const TextureData* getRenderTargetDepthBuffer(const RenderTarget* pRenderTarget)
        {
            if (pRenderTarget->pDepthBufferView != nullptr)
            {
                return pRenderTarget->depthBuffer.pDataBuffer;
            }
            return nullptr;
        }

        // 0x00C619B0 (:1149)
        float getScreenAspectRatio(const GraphicsSystem* pGraphicsSystem)
        {
            const ScreenCapture* pScreenCapture = pGraphicsSystem->pScreenCapture;
            if (pScreenCapture != nullptr)
            {
                if (pScreenCapture->height != 0)
                {
                    return (float)pScreenCapture->width / (float)pScreenCapture->height;
                }
                return 1.0f;
            }
            const RenderSwapChain* pSwapChain = pGraphicsSystem->pCurrentSwapChain != nullptr ? pGraphicsSystem->pCurrentSwapChain : pGraphicsSystem->pDefaultSwapChain;
            if (pSwapChain != nullptr && pSwapChain->windowHeight != 0)
            {
                return (float)pSwapChain->windowWidth / (float)pSwapChain->windowHeight;
            }
            return pGraphicsSystem->screenAspectRatio;
        }

        // 0x00C5BDF0 (:1169)
        ID3D11Device* getD3dDevice(GraphicsSystem* pGraphicsSystem)
        {
            return pGraphicsSystem->pDevice;
        }

        // 0x00C5BE00 (:1175)
        GraphicsCommandBuffer* getImmediateCommandBuffer(GraphicsSystem* pGraphicsSystem)
        {
            return &pGraphicsSystem->immediateCommandBuffer;
        }

        // 0x00C5BE10 (:1182)
        bool createShaderPipeline(ShaderPipeline* pTarget, GraphicsSystem* pGraphicsSystem, const ShaderPipelineParameters& parameters)
        {
            pTarget->pVertexShader = parameters.pVertexShader;
            pTarget->pFragmentShader = parameters.pFragmentShader;
            return true;
        }

        // 0x00C5BE30 (:1193)
        void destroyShaderPipeline(GraphicsSystem* pGraphicsSystem, ShaderPipeline* pPipeline)
        {
            pPipeline->pVertexShader = nullptr;
            pPipeline->pFragmentShader = nullptr;
        }

        // 0x00C5BE50 (:1201)
        const FragmentShader* getEmptyFragmentShader(const GraphicsSystem* pGraphicsSystem)
        {
            return &pGraphicsSystem->emptyFragmentShader;
        }

        // 0x00C71AC0 (:1206)
        const BlendState* createBlendState(GraphicsSystem* pGraphicsSystem, const BlendParametersEx& parameters)
        {
            BlendState* pState = nullptr;
            if (!pGraphicsSystem->stateObjectPool.allocateBlendState(&pState, parameters))
            {
                pState->pState->AddRef();
                return pState;
            }
            D3D11_BLEND_DESC description;
            memset(&description, 0, sizeof(description));
            description.AlphaToCoverageEnable = FALSE;
            description.IndependentBlendEnable = FALSE;
            D3D11_RENDER_TARGET_BLEND_DESC& target = description.RenderTarget[0];
            target.RenderTargetWriteMask = parameters.colorWriteMask[0];
            if (parameters.blendOpColor == BlendOperation_None && parameters.blendOpAlpha == BlendOperation_None)
            {
                target.BlendEnable = FALSE;
                target.SrcBlend = D3D11_BLEND_ONE;
                target.DestBlend = D3D11_BLEND_ZERO;
                target.BlendOp = D3D11_BLEND_OP_ADD;
                target.SrcBlendAlpha = D3D11_BLEND_ONE;
                target.DestBlendAlpha = D3D11_BLEND_ZERO;
                target.BlendOpAlpha = D3D11_BLEND_OP_ADD;
            }
            else
            {
                target.BlendEnable = TRUE;
                target.SrcBlend = getD3dColorBlendFactor(parameters.sourceFactorColor);
                target.DestBlend = getD3dColorBlendFactor(parameters.destFactorColor);
                target.BlendOp = getD3dBlendOperation(parameters.blendOpColor);
                target.SrcBlendAlpha = getD3dAlphaBlendFactor(parameters.sourceFactorAlpha);
                target.DestBlendAlpha = getD3dAlphaBlendFactor(parameters.destFactorAlpha);
                target.BlendOpAlpha = getD3dBlendOperation(parameters.blendOpAlpha);
            }
            const HRESULT result = pGraphicsSystem->pDevice->CreateBlendState(&description, &pState->pState);
            if (FAILED(result))
            {
                handleFatalHResult(pGraphicsSystem, result, "CreateBlendState");
            }
            return pState;
        }

        // 0x00C703D0 (:1262)
        void destroyBlendState(GraphicsSystem* pGraphicsSystem, const BlendState* pBlendState)
        {
            if (pBlendState != nullptr)
            {
                pBlendState->pState->Release();
                pGraphicsSystem->stateObjectPool.freeBlendState(pBlendState);
            }
        }

        // 0x00C71D70 (:1274)
        const RasterizerState* createRasterizerState(GraphicsSystem* pGraphicsSystem, const RasterizerParameters& parameters)
        {
            RasterizerState* pState = nullptr;
            if (!pGraphicsSystem->stateObjectPool.allocateRasterizerState(&pState, parameters))
            {
                pState->pState->AddRef();
                return pState;
            }
            D3D11_RASTERIZER_DESC description;
            switch (parameters.fillMode)
            {
            case FillMode_Wireframe: description.FillMode = D3D11_FILL_WIREFRAME; break;
            case FillMode_Solid: description.FillMode = D3D11_FILL_SOLID; break;
            default: exit(1);
            }
            switch (parameters.cullMode)
            {
            case CullMode_None: description.CullMode = D3D11_CULL_NONE; break;
            case CullMode_Front: description.CullMode = D3D11_CULL_FRONT; break;
            case CullMode_Back: description.CullMode = D3D11_CULL_BACK; break;
            default: exit(1);
            }
            switch (parameters.windingOrder)
            {
            case WindingOrder_Ccw: description.FrontCounterClockwise = TRUE; break;
            case WindingOrder_Cw: description.FrontCounterClockwise = FALSE; break;
            default: exit(1);
            }
            // The depth bias is rounded half away from zero.
            description.DepthBias = (INT)(parameters.depthBias > 0.0f ? parameters.depthBias + 0.5f : parameters.depthBias - 0.5f);
            description.DepthBiasClamp = parameters.depthBiasClamp;
            description.SlopeScaledDepthBias = parameters.depthBiasSlopeScale;
            description.DepthClipEnable = TRUE;
            description.ScissorEnable = (parameters.flags & RasterizerFlagMask_EnableScissorTest) == RasterizerFlagMask_EnableScissorTest;
            description.MultisampleEnable = FALSE;
            description.AntialiasedLineEnable = FALSE;
            const HRESULT result = pGraphicsSystem->pDevice->CreateRasterizerState(&description, &pState->pState);
            if (FAILED(result))
            {
                handleFatalHResult(pGraphicsSystem, result, "CreateRasterizerState");
            }
            return pState;
        }

        // 0x00C70400 (:1354)
        void destroyRasterizerState(GraphicsSystem* pGraphicsSystem, const RasterizerState* pRasterizerState)
        {
            if (pRasterizerState != nullptr)
            {
                pRasterizerState->pState->Release();
                pGraphicsSystem->stateObjectPool.freeRasterizerState(pRasterizerState);
            }
        }

        // 0x00C71EE0 (:1469)
        const SamplerState* createSamplerState(GraphicsSystem* pGraphicsSystem, const SamplerParameters& parameters)
        {
            SamplerState* pState = nullptr;
            if (!pGraphicsSystem->stateObjectPool.allocateSamplerState(&pState, parameters))
            {
                pState->pState->AddRef();
                return pState;
            }
            D3D11_SAMPLER_DESC description;
            if (parameters.comparisonFunction == ComparisonFunction_Never)
            {
                description.Filter = s_d3d11Filter[parameters.minFilter][parameters.magFilter][parameters.mipFilter];
            }
            else
            {
                description.Filter = s_d3d11FilterComparison[parameters.minFilter][parameters.magFilter][parameters.mipFilter];
            }
            description.AddressU = getD3dAddressMode(parameters.addressU);
            description.AddressV = getD3dAddressMode(parameters.addressV);
            description.AddressW = getD3dAddressMode(parameters.addressW);
            description.MipLODBias = parameters.mipLodBias;
            // Magic number: D3D11 allows at most 16x anisotropy.
            if (parameters.maxAnisotropyLevel == 0)
            {
                description.MaxAnisotropy = 1;
            }
            else
            {
                description.MaxAnisotropy = parameters.maxAnisotropyLevel < 17 ? parameters.maxAnisotropyLevel : 16;
            }
            description.ComparisonFunc = getD3dComparisonFunction(parameters.comparisonFunction);
            description.BorderColor[0] = 0.0f;
            description.BorderColor[1] = 0.0f;
            description.BorderColor[2] = 0.0f;
            description.BorderColor[3] = 0.0f;
            description.MinLOD = parameters.minLod;
            description.MaxLOD = parameters.maxLod;
            if (FAILED(pGraphicsSystem->pDevice->CreateSamplerState(&description, &pState->pState)))
            {
                debug::nativePrintArgs("[graphics] Could not create sampler state!\n");
                pGraphicsSystem->stateObjectPool.freeSamplerState(pState);
                return nullptr;
            }
            return pState;
        }

        // 0x00C70430 (:1512)
        void destroySamplerState(GraphicsSystem* pGraphicsSystem, const SamplerState* pSamplerState)
        {
            // Notes (decompiled behaviour, kept as is): no null check, unlike the other destroy functions.
            pSamplerState->pState->Release();
            pGraphicsSystem->stateObjectPool.freeSamplerState(pSamplerState);
        }

        // 0x00C72160 (:1522)
        const DepthStencilState* createDepthStencilState(GraphicsSystem* pGraphicsSystem, const DepthStencilParameters& parameters)
        {
            DepthStencilState* pState = nullptr;
            if (!pGraphicsSystem->stateObjectPool.allocateDepthStencilState(&pState, parameters))
            {
                pState->pState->AddRef();
                return pState;
            }
            D3D11_DEPTH_STENCIL_DESC description;
            description.DepthEnable = parameters.depthTestEnabled ? TRUE : FALSE;
            description.DepthWriteMask = parameters.depthWriteEnabled ? D3D11_DEPTH_WRITE_MASK_ALL : D3D11_DEPTH_WRITE_MASK_ZERO;
            description.DepthFunc = getD3dComparisonFunction(parameters.depthComparisonFunction);
            // Notes (decompiled behaviour, kept as is): the front face settings decide StencilEnable and the masks;
            // the back face only contributes its operations.
            description.StencilEnable = parameters.frontStencil.testEnabled ? TRUE : FALSE;
            description.StencilReadMask = (UINT8)parameters.frontStencil.testMask;
            description.StencilWriteMask = (UINT8)parameters.frontStencil.writeMask;
            description.FrontFace.StencilFailOp = getD3dStencilOperation(parameters.frontStencil.opFail);
            description.FrontFace.StencilDepthFailOp = getD3dStencilOperation(parameters.frontStencil.opDepthFail);
            description.FrontFace.StencilPassOp = getD3dStencilOperation(parameters.frontStencil.opDepthPass);
            description.FrontFace.StencilFunc = getD3dComparisonFunction(parameters.frontStencil.testFunc);
            description.BackFace.StencilFailOp = getD3dStencilOperation(parameters.backStencil.opFail);
            description.BackFace.StencilDepthFailOp = getD3dStencilOperation(parameters.backStencil.opDepthFail);
            description.BackFace.StencilPassOp = getD3dStencilOperation(parameters.backStencil.opDepthPass);
            description.BackFace.StencilFunc = getD3dComparisonFunction(parameters.backStencil.testFunc);
            if (FAILED(pGraphicsSystem->pDevice->CreateDepthStencilState(&description, &pState->pState)))
            {
                debug::nativePrintArgs("[graphics] Could not create depth/stencil state!\n");
                pGraphicsSystem->stateObjectPool.freeDepthStencilState(pState);
                return nullptr;
            }
            return pState;
        }

        // 0x00C70460 (:1569)
        void destroyDepthStencilState(GraphicsSystem* pGraphicsSystem, const DepthStencilState* pDepthStencilState)
        {
            if (pDepthStencilState != nullptr)
            {
                pDepthStencilState->pState->Release();
                pGraphicsSystem->stateObjectPool.freeDepthStencilState(pDepthStencilState);
            }
        }

        // 0x00C72620 (:1581)
        const VertexFormat* createVertexFormat(GraphicsSystem* pGraphicsSystem, const VertexAttributeDescription* pAttributes, uint32 attributeCount)
        {
            VertexFormat* pFormat = nullptr;
            if (pGraphicsSystem->stateObjectPool.allocateVertexFormat(&pFormat, pAttributes, attributeCount) && !pFormat->fill(pAttributes, attributeCount))
            {
                pGraphicsSystem->stateObjectPool.freeVertexFormat(pFormat);
                return nullptr;
            }
            return pFormat;
        }

        // 0x00C70490 (:1597)
        void destroyVertexFormat(GraphicsSystem* pGraphicsSystem, const VertexFormat* pFormat)
        {
            if (pFormat != nullptr)
            {
                pGraphicsSystem->stateObjectPool.freeVertexFormat(pFormat);
            }
        }
    }

    // 0x00C5BE60 (:1635)
    DXGI_FORMAT getD3dVertexAttributeFormat(uint32 format)
    {
        switch (format)
        {
        case VertexAttributeFormat_x32_float: return DXGI_FORMAT_R32_FLOAT;
        case VertexAttributeFormat_x32y32_float: return DXGI_FORMAT_R32G32_FLOAT;
        case VertexAttributeFormat_x32y32z32_float: return DXGI_FORMAT_R32G32B32_FLOAT;
        case VertexAttributeFormat_x32y32z32w32_float: return DXGI_FORMAT_R32G32B32A32_FLOAT;
        case VertexAttributeFormat_x16y16z16w16_float: return DXGI_FORMAT_R16G16B16A16_FLOAT;
        case VertexAttributeFormat_x16y16z16w16_sint: return DXGI_FORMAT_R16G16B16A16_SINT;
        case VertexAttributeFormat_x16y16z16w16_uint: return DXGI_FORMAT_R16G16B16A16_UINT;
        case VertexAttributeFormat_x16y16z16w16_snorm: return DXGI_FORMAT_R16G16B16A16_SNORM;
        case VertexAttributeFormat_x16y16z16w16_unorm: return DXGI_FORMAT_R16G16B16A16_UNORM;
        case VertexAttributeFormat_x8y8z8w8_unorm: return DXGI_FORMAT_R8G8B8A8_UNORM;
        case VertexAttributeFormat_x8y8z8w8_uint: return DXGI_FORMAT_R8G8B8A8_UINT;
        case VertexAttributeFormat_x8y8z8w8_snorm: return DXGI_FORMAT_R8G8B8A8_SNORM;
        case VertexAttributeFormat_x8y8z8w8_sint: return DXGI_FORMAT_R8G8B8A8_SINT;
        case VertexAttributeFormat_x16y16_float: return DXGI_FORMAT_R16G16_FLOAT;
        case VertexAttributeFormat_x16y16_sint: return DXGI_FORMAT_R16G16_SINT;
        case VertexAttributeFormat_x16y16_snorm: return DXGI_FORMAT_R16G16_SNORM;
        case VertexAttributeFormat_x16y16_uint: return DXGI_FORMAT_R16G16_UINT;
        case VertexAttributeFormat_x16y16_unorm: return DXGI_FORMAT_R16G16_UNORM;
        // Notes (decompiled behaviour, kept as is): the "snorm" 10:10:10:2 format is read as UINT.
        case VertexAttributeFormat_x10y10z10w2_snorm: return DXGI_FORMAT_R10G10B10A2_UINT;
        case VertexAttributeFormat_z8y8x8w8_unorm: return DXGI_FORMAT_B8G8R8A8_UNORM;
        default: return DXGI_FORMAT_UNKNOWN;
        }
    }

    namespace graphics
    {
        // 0x00C72670 (:1706)
        const VertexInputBinding* createVertexInputBinding(GraphicsSystem* pGraphicsSystem, const VertexFormat* pVertexFormat, const ShaderPipeline* pPipeline, uint32 geometryModeMask)
        {
            if (pVertexFormat == nullptr || pPipeline->pVertexShader == nullptr)
            {
                debug::nativePrintArgs("[graphics] You have to specify a vertex format and a vertex shader to create a vertex input binding!\n");
                return nullptr;
            }
            VertexInputBinding* pBinding = nullptr;
            if (!pGraphicsSystem->stateObjectPool.allocateVertexInputBinding(&pBinding, pVertexFormat, pPipeline->pVertexShader, geometryModeMask))
            {
                pBinding->pLayout->AddRef();
                pGraphicsSystem->stateObjectPool.addVertexFormatReference(pVertexFormat);
                return pBinding;
            }
            pBinding->pVertexFormat = pVertexFormat;
            D3D11_INPUT_ELEMENT_DESC elements[VertexFormat::MaxAttributeCount];
            uint32 elementCount = 0;
            for (; elementCount < pVertexFormat->attributeCount; ++elementCount)
            {
                const VertexAttributeDescription& attribute = pVertexFormat->attributes[elementCount];
                const DXGI_FORMAT format = getD3dVertexAttributeFormat(attribute.format);
                if (format == DXGI_FORMAT_UNKNOWN)
                {
                    debug::nativePrintArgs("[graphics] Unsupported vertex attribute format '%s'\n", getVertexAttributeFormatInfo((VertexAttributeFormat)attribute.format)->pName);
                    pGraphicsSystem->stateObjectPool.freeVertexInputBinding(pBinding);
                    return nullptr;
                }
                D3D11_INPUT_ELEMENT_DESC& element = elements[elementCount];
                element.SemanticName = s_pVertexAttributeSemanticInfos[attribute.id].pName;
                element.SemanticIndex = s_pVertexAttributeSemanticInfos[attribute.id].index;
                element.Format = format;
                element.InputSlot = attribute.inputStreamIndex;
                element.AlignedByteOffset = D3D11_APPEND_ALIGNED_ELEMENT;
                element.InputSlotClass = attribute.instanceStepRate != 0 ? D3D11_INPUT_PER_INSTANCE_DATA : D3D11_INPUT_PER_VERTEX_DATA;
                element.InstanceDataStepRate = attribute.instanceStepRate;
            }
            const VertexShader* pVertexShader = pPipeline->pVertexShader;
            if (FAILED(pGraphicsSystem->pDevice->CreateInputLayout(elements, elementCount, pVertexShader->shaderCode.getStart(), pVertexShader->shaderCode.getSize(), &pBinding->pLayout)))
            {
                pGraphicsSystem->stateObjectPool.freeVertexInputBinding(pBinding);
                return nullptr;
            }
            pBinding->geometryModeMask = geometryModeMask;
            pGraphicsSystem->stateObjectPool.addVertexFormatReference(pVertexFormat);
            return pBinding;
        }

        // 0x00C704B0 (:1771)
        void destroyVertexInputBinding(GraphicsSystem* pGraphicsSystem, const VertexInputBinding* pInputBinding)
        {
            if (pInputBinding != nullptr)
            {
                pGraphicsSystem->stateObjectPool.freeVertexFormat(pInputBinding->pVertexFormat);
                pInputBinding->pLayout->Release();
                pGraphicsSystem->stateObjectPool.freeVertexInputBinding(pInputBinding);
            }
        }

        // 0x00C5BF50 (:1783)
        static void updateRenderTargetBuffer(RenderTarget* pRenderTarget, const TextureData* pDataBuffer)
        {
            if (pRenderTarget->width == 0 && pRenderTarget->height == 0)
            {
                pRenderTarget->width = pDataBuffer->description.width;
                pRenderTarget->height = pDataBuffer->description.height;
            }
        }

        static DXGI_FORMAT getRenderTargetViewFormat(PixelFormat format)
        {
            switch (format)
            {
            case PixelFormat_Bc1: return DXGI_FORMAT_BC1_UNORM;
            case PixelFormat_Bc1_Gamma: return DXGI_FORMAT_BC1_UNORM_SRGB;
            case PixelFormat_Bc2: return DXGI_FORMAT_BC2_UNORM;
            case PixelFormat_Bc2_Gamma: return DXGI_FORMAT_BC2_UNORM_SRGB;
            case PixelFormat_Bc3: return DXGI_FORMAT_BC3_UNORM;
            case PixelFormat_Bc3_Gamma: return DXGI_FORMAT_BC3_UNORM_SRGB;
            case PixelFormat_A8: return DXGI_FORMAT_A8_UNORM;
            case PixelFormat_L8: return DXGI_FORMAT_R8_UNORM;
            case PixelFormat_R8G8: return DXGI_FORMAT_R8G8_UNORM;
            case PixelFormat_R32F: return DXGI_FORMAT_R32_FLOAT;
            case PixelFormat_Y16X16: return DXGI_FORMAT_R16G16_UNORM;
            case PixelFormat_R8G8B8A8: return DXGI_FORMAT_R8G8B8A8_UNORM;
            case PixelFormat_R8G8B8A8_Gamma: return DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
            case PixelFormat_A16B16G16R16F: return DXGI_FORMAT_R16G16B16A16_FLOAT;
            case PixelFormat_A32B32G32R32F: return DXGI_FORMAT_R32G32B32A32_FLOAT;
            case PixelFormat_Depth16: return DXGI_FORMAT_R16_TYPELESS;
            case PixelFormat_Depth24Stencil8: return DXGI_FORMAT_R24G8_TYPELESS;
            case PixelFormat_Depth32F: return DXGI_FORMAT_R32_TYPELESS;
            case PixelFormat_R11G11B10F: return DXGI_FORMAT_R11G11B10_FLOAT;
            default: return DXGI_FORMAT_UNKNOWN;
            }
        }

        // 0x00C6AD00 (:1802)
        const RenderTarget* createRenderTarget(GraphicsSystem* pGraphicsSystem, uint32 width, uint32 height, const RenderTargetBuffer* pColorBuffers, uint32 colorBufferCount, const RenderTargetBuffer* pDepthBuffer, TilingMode)
        {
            void* pMemory = pGraphicsSystem->pSystemAllocator->allocate(sizeof(RenderTarget), 4, 0, nullptr);
            RenderTarget* pRenderTarget = pMemory != nullptr ? new (pMemory) RenderTarget() : nullptr;
            pRenderTarget->width = width;
            pRenderTarget->height = height;
            pRenderTarget->colorBufferCount = colorBufferCount;
            for (uint32 i = 0; i < colorBufferCount; ++i)
            {
                updateRenderTargetBuffer(pRenderTarget, pColorBuffers[i].pDataBuffer);
                // A buffer without an explicit format views the texture with its own format.
                const PixelFormat viewFormat = pColorBuffers[i].format != PixelFormat_None ? pColorBuffers[i].format : (PixelFormat)pColorBuffers[i].pDataBuffer->description.format;
                D3D11_RENDER_TARGET_VIEW_DESC viewDesc;
                viewDesc.Format = getRenderTargetViewFormat(viewFormat);
                viewDesc.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2D;
                viewDesc.Texture2D.MipSlice = 0;
                const HRESULT result = pGraphicsSystem->pDevice->CreateRenderTargetView(pColorBuffers[i].pDataBuffer->pTexture, &viewDesc, &pRenderTarget->renderTargetViews[i]);
                if (FAILED(result))
                {
                    handleFatalHResult(pGraphicsSystem, result, "CreateRenderTargetView");
                }
                pRenderTarget->colorBuffers[i].format = pColorBuffers[i].format;
                pRenderTarget->colorBuffers[i].pDataBuffer = pColorBuffers[i].pDataBuffer;
            }
            for (uint32 i = colorBufferCount; i < 8; ++i)
            {
                pRenderTarget->renderTargetViews[i] = nullptr;
            }
            if (pDepthBuffer == nullptr)
            {
                pRenderTarget->pDepthBufferView = nullptr;
                return pRenderTarget;
            }
            updateRenderTargetBuffer(pRenderTarget, pDepthBuffer->pDataBuffer);
            D3D11_DEPTH_STENCIL_VIEW_DESC viewDesc;
            switch (pDepthBuffer->format)
            {
            case PixelFormat_None:
                // A buffer without an explicit format is matched by its (typeless) texture format.
                switch (pDepthBuffer->pDataBuffer->d3dFormat)
                {
                case DXGI_FORMAT_R16_TYPELESS: viewDesc.Format = DXGI_FORMAT_D16_UNORM; break;
                case DXGI_FORMAT_R24G8_TYPELESS: viewDesc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT; break;
                case DXGI_FORMAT_R32_TYPELESS: viewDesc.Format = DXGI_FORMAT_D32_FLOAT; break;
                default:
                    debug::nativePrintArgs("[graphics] Unsupported depth buffer texture format!\n");
                    pGraphicsSystem->pSystemAllocator->free(pRenderTarget);
                    return nullptr;
                }
                break;
            case PixelFormat_Depth16: viewDesc.Format = DXGI_FORMAT_D16_UNORM; break;
            case PixelFormat_Depth24Stencil8: viewDesc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT; break;
            case PixelFormat_Depth32F: viewDesc.Format = DXGI_FORMAT_D32_FLOAT; break;
            default:
                // Notes (decompiled behaviour, kept as is): the render target and its views leak here.
                debug::nativePrintArgs("[graphics] Unsupported pixel format!\n");
                return nullptr;
            }
            viewDesc.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2D;
            viewDesc.Flags = 0;
            viewDesc.Texture2D.MipSlice = 0;
            const HRESULT result = pGraphicsSystem->pDevice->CreateDepthStencilView(pDepthBuffer->pDataBuffer->pTexture, &viewDesc, &pRenderTarget->pDepthBufferView);
            if (FAILED(result))
            {
                handleFatalHResult(pGraphicsSystem, result, "CreateDepthStencilView");
            }
            pRenderTarget->depthBuffer.format = pDepthBuffer->format;
            pRenderTarget->depthBuffer.pDataBuffer = pDepthBuffer->pDataBuffer;
            return pRenderTarget;
        }

        // 0x00C61A70 (:1918)
        void destroyRenderTarget(GraphicsSystem* pGraphicsSystem, const RenderTarget* pRenderTarget)
        {
            if (pRenderTarget == nullptr)
            {
                return;
            }
            for (uint32 i = 0; i < pRenderTarget->colorBufferCount; ++i)
            {
                pRenderTarget->renderTargetViews[i]->Release();
            }
            if (pRenderTarget->pDepthBufferView != nullptr)
            {
                pRenderTarget->pDepthBufferView->Release();
            }
            pGraphicsSystem->pSystemAllocator->free(const_cast<RenderTarget*>(pRenderTarget));
        }

        // 0x00C61AC0 (:1937)
        const RenderTarget* createRenderTarget(GraphicsSystem* pGraphicsSystem, const RenderTarget* pColorBufferSource, const RenderTarget* pDepthStencilBufferSource)
        {
            // Shares the views of existing render targets (adding a reference to each).
            void* pMemory = pGraphicsSystem->pSystemAllocator->allocate(sizeof(RenderTarget), 4, 0, nullptr);
            RenderTarget* pRenderTarget = pMemory != nullptr ? new (pMemory) RenderTarget() : nullptr;
            pRenderTarget->width = 0;
            pRenderTarget->height = 0;
            pRenderTarget->colorBufferCount = 0;
            if (pColorBufferSource != nullptr)
            {
                pRenderTarget->colorBufferCount = pColorBufferSource->colorBufferCount;
                for (uint32 i = 0; i < pColorBufferSource->colorBufferCount; ++i)
                {
                    updateRenderTargetBuffer(pRenderTarget, pColorBufferSource->colorBuffers[i].pDataBuffer);
                    pRenderTarget->renderTargetViews[i] = pColorBufferSource->renderTargetViews[i];
                    pRenderTarget->renderTargetViews[i]->AddRef();
                    pRenderTarget->colorBuffers[i] = pColorBufferSource->colorBuffers[i];
                }
            }
            for (uint32 i = pRenderTarget->colorBufferCount; i < 8; ++i)
            {
                pRenderTarget->renderTargetViews[i] = nullptr;
                pRenderTarget->colorBuffers[i].pDataBuffer = nullptr;
            }
            if (pDepthStencilBufferSource == nullptr)
            {
                pRenderTarget->pDepthBufferView = nullptr;
                pRenderTarget->depthBuffer.pDataBuffer = nullptr;
                return pRenderTarget;
            }
            updateRenderTargetBuffer(pRenderTarget, pDepthStencilBufferSource->depthBuffer.pDataBuffer);
            pRenderTarget->pDepthBufferView = pDepthStencilBufferSource->pDepthBufferView;
            pRenderTarget->pDepthBufferView->AddRef();
            pRenderTarget->depthBuffer = pDepthStencilBufferSource->depthBuffer;
            return pRenderTarget;
        }

        static void createConstantBufferObject(GraphicsSystem* pGraphicsSystem, uint32 sizeInBytes, ID3D11Buffer** ppBuffer, const char* pDescription)
        {
            D3D11_BUFFER_DESC description;
            description.ByteWidth = sizeInBytes;
            description.Usage = D3D11_USAGE_DYNAMIC;
            description.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
            description.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
            description.MiscFlags = 0;
            description.StructureByteStride = 0;
            const HRESULT result = pGraphicsSystem->pDevice->CreateBuffer(&description, nullptr, ppBuffer);
            if (FAILED(result))
            {
                handleFatalHResult(pGraphicsSystem, result, pDescription);
            }
        }

        // 0x00C6B180 (:1988)
        DynamicConstantBuffer* createDynamicConstantBuffer(GraphicsSystem* pGraphicsSystem, uint32 firstRegisterIndex, uint32 sizeInBytes)
        {
            DynamicConstantBuffer* pBuffer = (DynamicConstantBuffer*)pGraphicsSystem->pSystemAllocator->allocate(sizeof(DynamicConstantBuffer), 4, 0, nullptr);
            createConstantBufferObject(pGraphicsSystem, sizeInBytes, &pBuffer->pBuffer, "Create Dynamic Constant Buffer");
            pBuffer->sizeInBytes = sizeInBytes;
            return pBuffer;
        }

        // 0x00C61C00 (:2026)
        void destroyDynamicConstantBuffer(GraphicsSystem* pGraphicsSystem, DynamicConstantBuffer* pConstantBuffer)
        {
            if (pConstantBuffer != nullptr)
            {
                pConstantBuffer->pBuffer->Release();
                pGraphicsSystem->pSystemAllocator->free(pConstantBuffer);
            }
        }

        // 0x00C5BF70 (:2038)
        uint32 getDynamicConstantBufferSize(const DynamicConstantBuffer* pBuffer)
        {
            return pBuffer->sizeInBytes;
        }

        // 0x00C6B220 (:2043)
        StaticConstantBuffer* createStaticConstantBuffer(GraphicsSystem* pGraphicsSystem, uint32 firstRegisterIndex, uint32 sizeInBytes)
        {
            // Notes (decompiled behaviour, kept as is): static constant buffers are dynamic buffers too.
            StaticConstantBuffer* pBuffer = (StaticConstantBuffer*)pGraphicsSystem->pSystemAllocator->allocate(sizeof(StaticConstantBuffer), 4, 0, nullptr);
            createConstantBufferObject(pGraphicsSystem, sizeInBytes, &pBuffer->pBuffer, "Create Static Constant Buffer");
            pBuffer->sizeInBytes = sizeInBytes;
            return pBuffer;
        }

        // 0x00C61C30 (:2081)
        void destroyStaticConstantBuffer(GraphicsSystem* pGraphicsSystem, StaticConstantBuffer* pConstantBuffer)
        {
            if (pConstantBuffer != nullptr)
            {
                pConstantBuffer->pBuffer->Release();
                pGraphicsSystem->pSystemAllocator->free(pConstantBuffer);
            }
        }

        // 0x00C5BF80 (:2094)
        uint32 getStaticConstantBufferSize(const StaticConstantBuffer* pBuffer)
        {
            return pBuffer->sizeInBytes;
        }

        // 0x00C6B2C0 (:2099)
        GeometryInstancingBuffer* createGeometryInstancingBuffer(GraphicsSystem* pGraphicsSystem, MemoryAllocator* pInstanceDataAllocator, uint32 instanceBufferSizeInBytes)
        {
            GeometryInstancingBuffer* pBuffer = (GeometryInstancingBuffer*)pGraphicsSystem->pSystemAllocator->allocate(sizeof(GeometryInstancingBuffer), 4, 0, nullptr);
            if (pBuffer == nullptr)
            {
                return nullptr;
            }
            D3D11_BUFFER_DESC description;
            description.ByteWidth = instanceBufferSizeInBytes;
            description.Usage = D3D11_USAGE_DYNAMIC;
            description.BindFlags = D3D11_BIND_VERTEX_BUFFER;
            description.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
            description.MiscFlags = 0;
            description.StructureByteStride = 0;
            const HRESULT result = pGraphicsSystem->pDevice->CreateBuffer(&description, nullptr, &pBuffer->pVertexBuffer);
            if (FAILED(result))
            {
                handleFatalHResult(pGraphicsSystem, result, "Create Vertex Buffer");
            }
            pBuffer->size = instanceBufferSizeInBytes;
            pBuffer->pContext = nullptr;
            pBuffer->currentPosition = 0;
            return pBuffer;
        }

        // 0x00C61C60 (:2143)
        void destroyGeometryInstancingBuffer(GeometryInstancingBuffer* pInstancingBuffer, GraphicsSystem* pGraphicsSystem, MemoryAllocator* pInstanceDataAllocator)
        {
            if (pInstancingBuffer == nullptr)
            {
                return;
            }
            if (pInstancingBuffer->pVertexBuffer != nullptr)
            {
                pInstancingBuffer->pVertexBuffer->Release();
                pInstancingBuffer->pVertexBuffer = nullptr;
            }
            pInstancingBuffer->size = 0;
            pInstancingBuffer->currentPosition = 0;
            pGraphicsSystem->pSystemAllocator->free(pInstancingBuffer);
        }

        // 0x00C5BF90 (:2164)
        void flipGeometryInstancingBuffer(GeometryInstancingBuffer* pBuffer)
        {
            pBuffer->currentPosition = 0;
        }

        // 0x00C61CA0 (:2172)
        static bool createDirect3D(GraphicsSystem* pGraphicsSystem)
        {
            pGraphicsSystem->screenAspectRatio = (float)((double)GetSystemMetrics(SM_CXFULLSCREEN) / (double)GetSystemMetrics(SM_CYFULLSCREEN));
            const D3D_FEATURE_LEVEL featureLevels[3] = { D3D_FEATURE_LEVEL_11_0, D3D_FEATURE_LEVEL_10_1, D3D_FEATURE_LEVEL_10_0 };
            D3D_FEATURE_LEVEL featureLevel;
            const HRESULT result = D3D11CreateDevice(nullptr, s_useReferenceDevice ? D3D_DRIVER_TYPE_WARP : D3D_DRIVER_TYPE_HARDWARE, nullptr, s_enableDebugChecks ? D3D11_CREATE_DEVICE_DEBUG : 0,
                featureLevels, 3, D3D11_SDK_VERSION, &pGraphicsSystem->pDevice, &featureLevel, &pGraphicsSystem->pImmediateContext);
            if (FAILED(result))
            {
                debug::nativePrintArgs("[graphics] Could not create Direct3D11 Device (%x)!\n", result);
                return false;
            }

            IDXGIDevice* pDxgiDevice = nullptr;
            if (SUCCEEDED(pGraphicsSystem->pDevice->QueryInterface(__uuidof(IDXGIDevice), (void**)&pDxgiDevice)))
            {
                IDXGIAdapter* pAdapter = nullptr;
                if (SUCCEEDED(pDxgiDevice->GetParent(__uuidof(IDXGIAdapter), (void**)&pAdapter)))
                {
                    DXGI_ADAPTER_DESC adapterDesc;
                    memset(&adapterDesc, 0, sizeof(adapterDesc));
                    if (pAdapter->GetDesc(&adapterDesc) == S_OK)
                    {
                        char adapterName[256];
                        convertUTF16ToUTF8(adapterName, sizeof(adapterName), (const uint16*)adapterDesc.Description);
                        debug::nativePrintArgs("[graphics] ------------------------------------------------\n");
                        debug::nativePrintArgs("[graphics] D3DDevice Created.\n   VID %04x & SubSysID %08x & Device ID %04x, Revision %x\n   (%s)\n", adapterDesc.VendorId, adapterDesc.SubSysId, adapterDesc.DeviceId, adapterDesc.Revision, adapterName);
                        debug::nativePrintArgs("[graphics] ------------------------------------------------\n");
                    }
                    pAdapter->Release();
                }
                pDxgiDevice->Release();
            }
            return true;
        }

        // 0x00C6B380 (:2251)
        static void destroyDirect3D(GraphicsSystem* pGraphicsSystem)
        {
            RenderSwapChain* pSwapChain = pGraphicsSystem->pDefaultSwapChain;
            if (pSwapChain != nullptr)
            {
                releaseSwapChain(pSwapChain);
                pSwapChain->pGraphicsSystem->pSystemAllocator->free(pSwapChain);
                pGraphicsSystem->pDefaultSwapChain = nullptr;
            }
            pGraphicsSystem->pImmediateContext->Release();
            pGraphicsSystem->pImmediateContext = nullptr;
            pGraphicsSystem->pDevice->Release();
            pGraphicsSystem->pDevice = nullptr;
        }

        // 0x00C5BFA0 (:2264)
        static D3D11_TEXTURE_ADDRESS_MODE getD3dAddressMode(SamplerAddressMode addressMode)
        {
            switch (addressMode)
            {
            case SamplerAddressMode_Wrap: return D3D11_TEXTURE_ADDRESS_WRAP;
            case SamplerAddressMode_Mirror: return D3D11_TEXTURE_ADDRESS_MIRROR;
            case SamplerAddressMode_Clamp: return D3D11_TEXTURE_ADDRESS_CLAMP;
            case SamplerAddressMode_Border: return D3D11_TEXTURE_ADDRESS_BORDER;
            default: exit(1);
            }
        }

        // 0x00C5BFE0 (:2286)
        static D3D11_BLEND getD3dColorBlendFactor(BlendFactor blendFactor)
        {
            switch (blendFactor)
            {
            case BlendFactor_Zero: return D3D11_BLEND_ZERO;
            case BlendFactor_One: return D3D11_BLEND_ONE;
            case BlendFactor_SrcColor: return D3D11_BLEND_SRC_COLOR;
            case BlendFactor_InvSrcColor: return D3D11_BLEND_INV_SRC_COLOR;
            case BlendFactor_SrcAlpha: return D3D11_BLEND_SRC_ALPHA;
            case BlendFactor_InvSrcAlpha: return D3D11_BLEND_INV_SRC_ALPHA;
            case BlendFactor_DestAlpha: return D3D11_BLEND_DEST_ALPHA;
            case BlendFactor_InvDestAlpha: return D3D11_BLEND_INV_DEST_ALPHA;
            case BlendFactor_DestColor: return D3D11_BLEND_DEST_COLOR;
            case BlendFactor_InvDestColor: return D3D11_BLEND_INV_DEST_COLOR;
            case BlendFactor_SrcAlphaSat: return D3D11_BLEND_SRC_ALPHA_SAT;
            case BlendFactor_BlendFactor: return D3D11_BLEND_BLEND_FACTOR;
            case BlendFactor_InvBlendFactor: return D3D11_BLEND_INV_BLEND_FACTOR;
            default: exit(1);
            }
        }

        // 0x00C5C080 (:2335)
        static D3D11_BLEND getD3dAlphaBlendFactor(BlendFactor blendFactor)
        {
            // Color factors are mapped to their alpha counterparts.
            switch (blendFactor)
            {
            case BlendFactor_Zero: return D3D11_BLEND_ZERO;
            case BlendFactor_One: return D3D11_BLEND_ONE;
            case BlendFactor_SrcColor:
            case BlendFactor_SrcAlpha: return D3D11_BLEND_SRC_ALPHA;
            case BlendFactor_InvSrcColor:
            case BlendFactor_InvSrcAlpha: return D3D11_BLEND_INV_SRC_ALPHA;
            case BlendFactor_DestAlpha:
            case BlendFactor_DestColor: return D3D11_BLEND_DEST_ALPHA;
            case BlendFactor_InvDestAlpha:
            case BlendFactor_InvDestColor: return D3D11_BLEND_INV_DEST_ALPHA;
            case BlendFactor_SrcAlphaSat: return D3D11_BLEND_SRC_ALPHA_SAT;
            case BlendFactor_BlendFactor: return D3D11_BLEND_BLEND_FACTOR;
            case BlendFactor_InvBlendFactor: return D3D11_BLEND_INV_BLEND_FACTOR;
            default: exit(1);
            }
        }

        // 0x00C5C100 (:2384)
        static D3D11_BLEND_OP getD3dBlendOperation(BlendOperation blendOp)
        {
            switch (blendOp)
            {
            case BlendOperation_Add: return D3D11_BLEND_OP_ADD;
            // keen's Subtract is destination minus source.
            case BlendOperation_Subtract: return D3D11_BLEND_OP_REV_SUBTRACT;
            case BlendOperation_RevSubtract: return D3D11_BLEND_OP_SUBTRACT;
            case BlendOperation_Min: return D3D11_BLEND_OP_MIN;
            case BlendOperation_Max: return D3D11_BLEND_OP_MAX;
            default: exit(1);
            }
        }

        // 0x00C5C150 (:2409)
        static D3D11_COMPARISON_FUNC getD3dComparisonFunction(ComparisonFunction func)
        {
            switch (func)
            {
            case ComparisonFunction_Never: return D3D11_COMPARISON_NEVER;
            case ComparisonFunction_Less: return D3D11_COMPARISON_LESS;
            case ComparisonFunction_Equal: return D3D11_COMPARISON_EQUAL;
            case ComparisonFunction_LessEqual: return D3D11_COMPARISON_LESS_EQUAL;
            case ComparisonFunction_Greater: return D3D11_COMPARISON_GREATER;
            case ComparisonFunction_NotEqual: return D3D11_COMPARISON_NOT_EQUAL;
            case ComparisonFunction_GreaterEqual: return D3D11_COMPARISON_GREATER_EQUAL;
            case ComparisonFunction_Always: return D3D11_COMPARISON_ALWAYS;
            default: exit(1);
            }
        }

        // 0x00C5C1C0 (:2443)
        static D3D11_STENCIL_OP getD3dStencilOperation(StencilOperation operation)
        {
            switch (operation)
            {
            case StencilOperation_Keep: return D3D11_STENCIL_OP_KEEP;
            case StencilOperation_Zero: return D3D11_STENCIL_OP_ZERO;
            case StencilOperation_Replace: return D3D11_STENCIL_OP_REPLACE;
            case StencilOperation_Increment: return D3D11_STENCIL_OP_INCR_SAT;
            case StencilOperation_Decrement: return D3D11_STENCIL_OP_DECR_SAT;
            case StencilOperation_IncrementWrap: return D3D11_STENCIL_OP_INCR;
            case StencilOperation_DecrementWrap: return D3D11_STENCIL_OP_DECR;
            case StencilOperation_Invert: return D3D11_STENCIL_OP_INVERT;
            default: exit(1);
            }
        }

        // 0x00C61E90 (:2594)
        static bool createSwapChainBuffers(RenderSwapChain* pSwapChain, uint32 newWidth, uint32 newHeight)
        {
            ID3D11Device* pDevice = pSwapChain->pGraphicsSystem->pDevice;
            ID3D11Texture2D* pBackBuffer = nullptr;
            HRESULT result = pSwapChain->pSwapChain->GetBuffer(0, __uuidof(ID3D11Texture2D), (void**)&pBackBuffer);
            if (FAILED(result))
            {
                debug::nativePrintArgs("[graphics] Could not get SwapChainBuffer (%x)!\n", result);
                return false;
            }
            // Notes (decompiled behaviour, kept as is): the GetBuffer reference is kept as the back buffer texture
            // and released with the swap chain buffers.
            pSwapChain->backBufferColorData.pTexture = pBackBuffer;
            D3D11_TEXTURE2D_DESC backBufferDesc;
            pBackBuffer->GetDesc(&backBufferDesc);
            debug::nativePrintArgs("[graphics] Swapchain buffers created %dx%d\n", backBufferDesc.Width, backBufferDesc.Height);
            const DXGI_FORMAT noGammaFormat = backBufferDesc.Format == DXGI_FORMAT_R8G8B8A8_UNORM_SRGB ? DXGI_FORMAT_R8G8B8A8_UNORM : backBufferDesc.Format;

            result = pDevice->CreateRenderTargetView(pBackBuffer, nullptr, &pSwapChain->pBackBufferRenderTargetView);
            if (FAILED(result))
            {
                debug::nativePrintArgs("[graphics] Could not create RenderTargetView (%x)!\n", result);
                return false;
            }
            if (noGammaFormat == backBufferDesc.Format)
            {
                pSwapChain->pNoGammaBackBufferRenderTargetView = nullptr;
            }
            else
            {
                D3D11_RENDER_TARGET_VIEW_DESC viewDesc;
                viewDesc.Format = noGammaFormat;
                viewDesc.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2D;
                viewDesc.Texture2D.MipSlice = 0;
                result = pDevice->CreateRenderTargetView(pBackBuffer, &viewDesc, &pSwapChain->pNoGammaBackBufferRenderTargetView);
                if (FAILED(result))
                {
                    debug::nativePrintArgs("[graphics] Could not create noGamme RenderTargetView (%x)!\n", result);
                    return false;
                }
            }

            if (pSwapChain->depthBufferFormat == PixelFormat_Invalid)
            {
                pSwapChain->backBufferDepthData.description.width = 0;
                pSwapChain->backBufferDepthData.description.height = 0;
                pSwapChain->backBufferDepthData.pTexture = nullptr;
                pSwapChain->pBackBufferDepthView = nullptr;
            }
            else
            {
                D3D11_TEXTURE2D_DESC depthDesc;
                memset(&depthDesc, 0, sizeof(depthDesc));
                depthDesc.Width = newWidth;
                depthDesc.Height = newHeight;
                depthDesc.MipLevels = 1;
                depthDesc.ArraySize = 1;
                // Notes (decompiled behaviour, kept as is): always D24S8, whatever depth format was asked for.
                depthDesc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
                depthDesc.SampleDesc = pSwapChain->swapChainDescription.SampleDesc;
                depthDesc.Usage = D3D11_USAGE_DEFAULT;
                depthDesc.BindFlags = D3D11_BIND_DEPTH_STENCIL;
                result = pDevice->CreateTexture2D(&depthDesc, nullptr, (ID3D11Texture2D**)&pSwapChain->backBufferDepthData.pTexture);
                if (FAILED(result))
                {
                    debug::nativePrintArgs("[graphics] Could not create Texture2D (%x)!\n", result);
                    return false;
                }
                D3D11_DEPTH_STENCIL_VIEW_DESC viewDesc;
                memset(&viewDesc, 0, sizeof(viewDesc));
                viewDesc.Format = depthDesc.Format;
                viewDesc.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2D;
                viewDesc.Texture2D.MipSlice = 0;
                result = pDevice->CreateDepthStencilView(pSwapChain->backBufferDepthData.pTexture, &viewDesc, &pSwapChain->pBackBufferDepthView);
                if (FAILED(result))
                {
                    debug::nativePrintArgs("[graphics] Could not create DepthStencilView (%x)!\n", result);
                    return false;
                }
                pSwapChain->backBufferDepthData.description.width = (uint16)newWidth;
                pSwapChain->backBufferDepthData.description.height = (uint16)newHeight;
            }

            pSwapChain->backBufferColorData.description.width = (uint16)newWidth;
            pSwapChain->backBufferColorData.description.height = (uint16)newHeight;

            RenderTarget& backBuffer = pSwapChain->backBufferRenderTarget;
            backBuffer.renderTargetViews[0] = pSwapChain->pBackBufferRenderTargetView;
            backBuffer.width = newWidth;
            backBuffer.height = newHeight;
            backBuffer.colorBufferCount = 1;
            backBuffer.colorBuffers[0].format = PixelFormat_None;
            backBuffer.colorBuffers[0].pDataBuffer = &pSwapChain->backBufferColorData;
            backBuffer.depthBuffer.format = PixelFormat_None;
            backBuffer.depthBuffer.pDataBuffer = &pSwapChain->backBufferDepthData;
            backBuffer.pDepthBufferView = pSwapChain->pBackBufferDepthView;

            if (pSwapChain->pNoGammaBackBufferRenderTargetView != nullptr)
            {
                RenderTarget& noGammaBackBuffer = pSwapChain->noGammaBackBufferRenderTarget;
                noGammaBackBuffer.width = newWidth;
                noGammaBackBuffer.height = newHeight;
                noGammaBackBuffer.renderTargetViews[0] = pSwapChain->pNoGammaBackBufferRenderTargetView;
                noGammaBackBuffer.colorBuffers[0].format = PixelFormat_None;
                noGammaBackBuffer.colorBuffers[0].pDataBuffer = &pSwapChain->backBufferColorData;
                noGammaBackBuffer.depthBuffer.format = PixelFormat_None;
                noGammaBackBuffer.colorBufferCount = 1;
                noGammaBackBuffer.depthBuffer.pDataBuffer = &pSwapChain->backBufferDepthData;
                noGammaBackBuffer.pDepthBufferView = pSwapChain->pBackBufferDepthView;
            }
            else
            {
                pSwapChain->noGammaBackBufferRenderTarget = backBuffer;
            }
            return true;
        }

        // 0x00C62200 (:2737)
        static void destroySwapChainBuffers(RenderSwapChain* pSwapChain)
        {
            memset(&pSwapChain->backBufferRenderTarget, 0, sizeof(pSwapChain->backBufferRenderTarget));
            if (pSwapChain->pNoGammaBackBufferRenderTargetView != nullptr)
            {
                pSwapChain->pNoGammaBackBufferRenderTargetView->Release();
                pSwapChain->pNoGammaBackBufferRenderTargetView = nullptr;
            }
            if (pSwapChain->pBackBufferRenderTargetView != nullptr)
            {
                pSwapChain->pBackBufferRenderTargetView->Release();
                pSwapChain->pBackBufferRenderTargetView = nullptr;
            }
            if (pSwapChain->pBackBufferDepthView != nullptr)
            {
                pSwapChain->pBackBufferDepthView->Release();
                pSwapChain->pBackBufferDepthView = nullptr;
            }
            if (pSwapChain->backBufferDepthData.pTexture != nullptr)
            {
                pSwapChain->backBufferDepthData.pTexture->Release();
                pSwapChain->backBufferDepthData.pTexture = nullptr;
            }
            if (pSwapChain->backBufferColorData.pTexture != nullptr)
            {
                pSwapChain->backBufferColorData.pTexture->Release();
                pSwapChain->backBufferColorData.pTexture = nullptr;
            }
        }

        // 0x00C5C230 (:2768)
        uint32 getSwapChainWidth(const RenderSwapChain& swapChain)
        {
            return swapChain.windowWidth;
        }

        // 0x00C5C240 (:2773)
        uint32 getSwapChainHeight(const RenderSwapChain& swapChain)
        {
            return swapChain.windowHeight;
        }

        // 0x00C5C250 (:2778)
        void* getSwapChainHWND(GraphicsSystem* pGraphicsSystem, const RenderSwapChain* pSwapChain)
        {
            if (pSwapChain == nullptr)
            {
                pSwapChain = pGraphicsSystem->pCurrentSwapChain;
                if (pSwapChain == nullptr)
                {
                    return pGraphicsSystem->pDefaultSwapChain->windowHandle;
                }
            }
            return pSwapChain->windowHandle;
        }

        // 0x00C741D0 (:2794)
        void startScreenCapture(GraphicsSystem* pGraphicsSystem, FileSystem* pFileSystem, uint32 width, uint32 height, uint32 bufferCount, const char* pBasePath)
        {
            pGraphicsSystem->pScreenCapture = createScreenCapture(pGraphicsSystem->pSystemAllocator, pGraphicsSystem, pFileSystem, width, height, bufferCount, pBasePath);
        }

        // 0x00C72FA0 (:2803)
        void stopScreenCapture(GraphicsSystem* pGraphicsSystem)
        {
            if (pGraphicsSystem->pScreenCapture != nullptr)
            {
                destroySceenCapture(pGraphicsSystem->pSystemAllocator, pGraphicsSystem->pScreenCapture);
                pGraphicsSystem->pScreenCapture = nullptr;
            }
        }

        // 0x00C5C280 (:2816)
        bool isScreenCaptureActive(GraphicsSystem* pGraphicsSystem)
        {
            return pGraphicsSystem->pScreenCapture != nullptr;
        }

        // 0x00C62280 (:2823)
        bool isScreenCaptureSavingActive(uint32* pIndex, GraphicsSystem* pGraphicsSystem)
        {
            const ScreenCapture* pScreenCapture = pGraphicsSystem->pScreenCapture;
            if (pScreenCapture != nullptr && pScreenCapture->state == ScreenCaptureState_Writing)
            {
                if (pIndex != nullptr)
                {
                    *pIndex = pScreenCapture->saveFrameIndex;
                }
                return true;
            }
            return false;
        }

        // 0x00C6B3F0 (:2830)
        void saveScreenCaptureFrames(GraphicsSystem* pGraphicsSystem, const char* pSubPath, const char* pFilenamePrefix, uint32 frameCount)
        {
            saveFrames(pGraphicsSystem->pScreenCapture, pSubPath, pFilenamePrefix, frameCount);
        }

        // 0x00C622C0 (:2840)
        RecordedCommandBuffer* createCommandBuffer(GraphicsSystem* pGraphicsSystem, MemoryBlock)
        {
            RecordedCommandBuffer* pCommandBuffer = (RecordedCommandBuffer*)pGraphicsSystem->pSystemAllocator->allocate(sizeof(RecordedCommandBuffer), 16, 0, nullptr);
            if (pCommandBuffer == nullptr)
            {
                return nullptr;
            }
            memset(pCommandBuffer, 0, sizeof(RecordedCommandBuffer));
            if (pGraphicsSystem->pDevice->CreateDeferredContext(0, &pCommandBuffer->commandBuffer.pContext) != S_OK)
            {
                pGraphicsSystem->pSystemAllocator->free(pCommandBuffer);
                return nullptr;
            }
            D3D11_BUFFER_DESC bufferDesc;
            bufferDesc.ByteWidth = pGraphicsSystem->immediateCommandBuffer.immediateVertexBufferSize;
            bufferDesc.Usage = D3D11_USAGE_DYNAMIC;
            bufferDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
            bufferDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
            bufferDesc.MiscFlags = 0;
            bufferDesc.StructureByteStride = 0;
            if (FAILED(pGraphicsSystem->pDevice->CreateBuffer(&bufferDesc, nullptr, &pCommandBuffer->commandBuffer.pImmediateVertexData)))
            {
                // Notes (decompiled behaviour, kept as is): the command buffer and its context leak here.
                debug::nativePrintArgs("[graphics] Could not create Command Buffer immediate vertices!\n");
                return nullptr;
            }
            pCommandBuffer->commandBuffer.immediateVertexBufferSize = bufferDesc.ByteWidth;
            pCommandBuffer->pCommandList = nullptr;
            return pCommandBuffer;
        }

        // 0x00C5C290 (:2882)
        void destroyCommandBuffer(GraphicsSystem* pGraphicsSystem, RecordedCommandBuffer* pCommandBuffer)
        {
            if (pCommandBuffer->pCommandList != nullptr)
            {
                pCommandBuffer->pCommandList->Release();
            }
            pCommandBuffer->commandBuffer.pImmediateVertexData->Release();
            pCommandBuffer->commandBuffer.pContext->Release();
            pGraphicsSystem->pSystemAllocator->free(pCommandBuffer);
        }

        // 0x00C5C2D0 (:2897)
        GraphicsCommandBuffer* beginCommandBuffer(GraphicsSystem* pGraphicsSystem, RecordedCommandBuffer* pCommandBuffer)
        {
            if (pCommandBuffer->pCommandList != nullptr)
            {
                pCommandBuffer->pCommandList->Release();
                pCommandBuffer->pCommandList = nullptr;
            }
            pCommandBuffer->commandBuffer.pDownsampleDepthContext = pGraphicsSystem->immediateCommandBuffer.pDownsampleDepthContext;
            // endCommandBuffer finishes the command list into the recorded buffer.
            pCommandBuffer->commandBuffer.pRenderCommandBufferStorage = pCommandBuffer;
            // A full immediate buffer: the first immediate draw discards it.
            pCommandBuffer->commandBuffer.immediateVertexBufferOffset = pCommandBuffer->commandBuffer.immediateVertexBufferSize;
            return &pCommandBuffer->commandBuffer;
        }

        // 0x00C5C310 (:2915)
        void endCommandBuffer(GraphicsSystem*, GraphicsCommandBuffer* pCommandBuffer)
        {
            RecordedCommandBuffer* pRecordedBuffer = (RecordedCommandBuffer*)pCommandBuffer->pRenderCommandBufferStorage;
            const HRESULT result = pCommandBuffer->pContext->FinishCommandList(FALSE, &pRecordedBuffer->pCommandList);
            if (result != S_OK)
            {
                debug::nativePrintArgs("[graphics] FinishCommandList failed with %x\n", result);
            }
        }

        // 0x00C5C340 (:2926)
        bool setOutputGamma(GraphicsSystem* pGraphicsSystem, float gammaScale)
        {
            IDXGISwapChain* pSwapChain = pGraphicsSystem->pDefaultSwapChain->pSwapChain;
            IDXGIOutput* pOutput = nullptr;
            if (pSwapChain->GetContainingOutput(&pOutput) != S_OK)
            {
                return false;
            }
            DXGI_GAMMA_CONTROL_CAPABILITIES capabilities;
            if (pOutput->GetGammaControlCapabilities(&capabilities) != S_OK)
            {
                pOutput->Release();
                return false;
            }
            DXGI_GAMMA_CONTROL gammaControl;
            gammaControl.Scale.Red = 1.0f;
            gammaControl.Scale.Green = 1.0f;
            gammaControl.Scale.Blue = 1.0f;
            gammaControl.Offset.Red = 0.0f;
            gammaControl.Offset.Green = 0.0f;
            gammaControl.Offset.Blue = 0.0f;
            for (UINT i = 0; i < capabilities.NumGammaControlPoints; ++i)
            {
                const float value = powf(capabilities.ControlPointPositions[i], gammaScale);
                gammaControl.GammaCurve[i].Red = value;
                gammaControl.GammaCurve[i].Green = value;
                gammaControl.GammaCurve[i].Blue = value;
            }
            pOutput->SetGammaControl(&gammaControl);
            pOutput->Release();
            return capabilities.NumGammaControlPoints != 0;
        }

        // 0x00C5C4C0 (:2962)
        void acquireThreadOwnership(GraphicsSystem* pGraphicsSystem)
        {
            pGraphicsSystem->ownerThreadId = getCurrentThreadId();
        }

        // 0x00C5C4D0 (:2967)
        void releaseThreadOwnership(GraphicsSystem* pGraphicsSystem)
        {
            pGraphicsSystem->ownerThreadId = pGraphicsSystem->frontThreadId;
        }

        // 0x00C623D0 (:2982)
        void handleFatalHResult(GraphicsSystem* pGraphicsSystem, long result, const char* pFunctionDescription)
        {
            if (result == DXGI_ERROR_DEVICE_REMOVED)
            {
                result = pGraphicsSystem->pDevice->GetDeviceRemovedReason();
            }
            debug::nativePrintArgs("[graphics] %s failed with error code 0x%x.\n", pFunctionDescription, result);
            char message[1024];
            if ((result & 0x20000000) == 0)
            {
                // Not a customer-defined code: append the system's description.
                const _com_error error(result);
                formatString(message, sizeof(message), "%s failed with error code 0x%x.\n\n%s", pFunctionDescription, result, error.ErrorMessage());
            }
            else
            {
                formatString(message, sizeof(message), "%s failed with error code 0x%x.", pFunctionDescription, result);
            }
            // OpenRFG deviation: the message box is replaced by a log line.
            // MessageBoxA(nullptr, message, "Fatal error", MB_OK);
            debug::nativePrintArgs("[Fatal error] %s\n", message);
            _exit(1);
        }
    }
}
