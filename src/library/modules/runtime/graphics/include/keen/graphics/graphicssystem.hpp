// Original: library/modules/runtime/graphics/include/keen/graphics/graphicssystem.hpp
// Implementation: library/modules/runtime/graphics/sources/d3d11/graphicssystem_d3d11.cpp
//
// The graphics system API: creation (GraphicsSystemParameters, see sources/d3d11/graphicssystem_d3d11.hpp), frames
// (beginFrame/endFrame), display modes, render state and shader pipeline creation, constant buffers, and the settings
// records passed to command writers (ClearSettings, ViewportSettings, ScissorRectangle). GraphicsSystem itself is
// defined in sources/d3d11/graphicssystem_d3d11.hpp. Placement of the types without inline methods inferred.
#pragma once

#include "keen/base/types.hpp"
#include "keen/graphics/graphicsstateobjects.hpp"
#include "keen/graphics/texturedescription.hpp"
#include "keen/graphics/pixelformat.hpp"

enum DXGI_FORMAT;
struct ID3D11Device;

namespace keen
{
    struct BlendState;
    struct CombinedGraphicsStateObjectPoolParameters;
    struct DepthStencilState;
    struct DynamicConstantBuffer;
    struct FileSystem;
    struct FragmentShader;
    struct GeometryInstancingBuffer;
    struct GraphicsCommandBuffer;
    struct GraphicsSystem;
    struct GraphicsSystemBase;
    struct GraphicsSystemParameters;
    class MemoryAllocator;
    struct MemoryBlock;
    struct RasterizerState;
    struct RecordedCommandBuffer;
    struct RenderSwapChain;
    struct RenderTarget;
    struct RenderTargetBuffer;
    struct SamplerState;
    struct StaticConstantBuffer;
    struct TextureData;
    struct VertexAttributeDescription;
    struct VertexFormat;
    struct VertexInputBinding;
    struct VertexShader;

    enum GraphicsMapType
    {
        GraphicsMapType_Read = 0,
        GraphicsMapType_Write = 1,
        GraphicsMapType_ReadWrite = 2,
        GraphicsMapType_WriteDiscard = 3,
        GraphicsMapType_WriteNoOverwrite = 4,
        GraphicsMapType_Count = 5,
    };

    enum GraphicsMapFlagMask
    {
        GraphicsMapFlagMask_NonBlocking = 1,
    };

    enum GraphicsGeometryMode
    {
        GraphicsGeometryMode_Static = 0,
        GraphicsGeometryMode_Skinned = 1,
        GraphicsGeometryMode_Instanced = 2,
        GraphicsGeometryMode_Immediate = 3,
        GraphicsGeometryMode_Count = 4,
    };

    enum GraphicsGeometryModeMask
    {
        GraphicsGeometryModeMask_Static = 1,
        GraphicsGeometryModeMask_Skinned = 2,
        GraphicsGeometryModeMask_Instanced = 4,
        GraphicsGeometryModeMask_Immediate = 8,
        GraphicsGeometryModeMask_Default = 7,
    };

    enum ShaderMask
    {
        ShaderMask_Vertex = 1,
        ShaderMask_Fragment = 2,
        ShaderMask_VertexAndFragment = 3,
    };

    enum GraphicsStockVertexShaderId
    {
        GraphicsStockVertexShaderId_Immediate = 0,
        GraphicsStockVertexShaderId_Copy = 1,
        GraphicsStockVertexShaderId_Count = 2,
    };

    enum GraphicsStockFragmentShaderId
    {
        GraphicsStockFragmentShaderId_Immediate = 0,
        GraphicsStockFragmentShaderId_Immediate_VolumeSlice = 1,
        GraphicsStockFragmentShaderId_Copy = 2,
        GraphicsStockFragmentShaderId_Count = 3,
    };

    enum GraphicsStockShaderPipelineId
    {
        GraphicsStockShaderPipelineId_Immediate = 0,
        GraphicsStockShaderPipelineId_Immediate_VolumeSlice = 1,
        GraphicsStockShaderPipelineId_Copy = 2,
        GraphicsStockShaderPipelineId_Count = 3,
    };

    enum GraphicsStockVertexFormat
    {
        GraphicsStockVertexFormat_Pos2f = 0,
        GraphicsStockVertexFormat_Pos2f_Tex2f = 1,
        GraphicsStockVertexFormat_Immediate = 2,
        GraphicsStockVertexFormat_Count = 3,
    };

    enum GraphicsStockTextureId
    {
        GraphicsStockTextureId_White = 0,
        GraphicsStockTextureId_Count = 1,
    };

    namespace graphics
    {
        enum WindowMode
        {
            WindowMode_Windowed = 0,
            WindowMode_Borderless = 1,
            WindowMode_Fullscreen = 2,
            WindowMode_Count = 3,
        };

    }

    // size 0x10
    struct DisplayMode
    {
        uint32 width; // 0x00
        uint32 height; // 0x04
        uint32 refreshRateNumerator; // 0x08
        uint32 refreshRateDenominator; // 0x0C
    };

    // size 0x10
    struct ClearSettings
    {
        ClearSettings(uint32 inColor, float inDepth, uint8 inStencilValue, uint32 inClearMask); // 0x004B2350 (.h:54)
        ClearSettings(); // 0x004B2340 (.h:53)

        uint32 color; // 0x00
        float depth; // 0x04
        uint8 stencilValue; // 0x08
        uint32 clearMask; // 0x0C
    };

    // size 0x18
    struct ViewportSettings
    {
        uint32 x; // 0x00
        uint32 y; // 0x04
        uint32 width; // 0x08
        uint32 height; // 0x0C
        float minDepth; // 0x10
        float maxDepth; // 0x14
    };

    // size 0x10
    struct ScissorRectangle
    {
        uint32 x; // 0x00
        uint32 y; // 0x04
        uint32 width; // 0x08
        uint32 height; // 0x0C
    };

    // size 0x8
    struct ShaderPipeline
    {
        const VertexShader* pVertexShader; // 0x00
        const FragmentShader* pFragmentShader; // 0x04
    };

    // size 0x8
    struct ShaderPipelineParameters
    {
        ShaderPipelineParameters(const VertexShader* pVertexShader, const FragmentShader* pFragmentShader); // 0x00456D00 (.h:221)
        ShaderPipelineParameters();

        const VertexShader* pVertexShader; // 0x00
        const FragmentShader* pFragmentShader; // 0x04
    };

    // size 0x8
    struct MappedTextureInfo
    {
        uint32 rowPitch; // 0x00
        uint32 slicePitch; // 0x04
    };

    namespace graphics
    {
        uint32 getBackBufferWidth(const GraphicsSystem* pGraphicsSystem); // 0x004B2380 (.h:269)
        uint32 getBackBufferHeight(const GraphicsSystem* pGraphicsSystem); // 0x004B23A0 (.h:270)
        const RasterizerState* createRasterizerState(GraphicsSystem* pGraphicsSystem, CullMode cullMode, FillMode fillMode, WindingOrder windingOrder, uint32 flags); // 0x004DB660 (.h:324)
        const BlendState* createBlendState(GraphicsSystem* pGraphicsSystem, BlendOperation blendOp, BlendFactor sourceFactor, BlendFactor destFactor, uint8 colorWriteMask0); // 0x004DB6D0 (.h:334)
        const SamplerState* createSamplerState(GraphicsSystem* pGraphicsSystem, SamplerAddressMode addressU, SamplerAddressMode addressV, SamplerAddressMode addressW, SamplerFilterMode minFilter, SamplerFilterMode magFilter, SamplerFilterMode mipFilter); // 0x004DB760 (.h:369)
        const DepthStencilState* createDepthStencilState(GraphicsSystem* pGraphicsSystem, bool enableDepthTest, ComparisonFunction depthCompareFunction, bool enableDepthWrite); // 0x004DB7E0 (.h:381)
        GraphicsSystem* createGraphicsSystem(MemoryAllocator* pAllocator, const GraphicsSystemParameters& parameters); // 0x00C73AB0 (graphicssystem_d3d11.cpp:164)
        uint32 getCurrentFrameNumber(const GraphicsSystem* pGraphicsSystem); // 0x00C5BB30 (graphicssystem_d3d11.cpp:329)
        void destroyGraphicsSystem(GraphicsSystem* pGraphicsSystem); // 0x00C72E80 (graphicssystem_d3d11.cpp:336)
        void handleWindowMessages(GraphicsSystem* pGraphicsSystem); // 0x00C5BB40 (graphicssystem_d3d11.cpp:371)
        bool isWindowDestroyed(const GraphicsSystem* pGraphicsSystem); // 0x00C5BBB0 (graphicssystem_d3d11.cpp:381)
        RenderSwapChain* createRenderSwapChain(GraphicsSystem* pGraphicsSystem, void* pWindowHandle, PixelFormat depthBufferFormat, uint32 presentationInterval, uint32 width, uint32 height); // 0x00C6DA10 (graphicssystem_d3d11.cpp:386)
        void destroyRenderSwapChain(RenderSwapChain* pSwapChain); // 0x00C6AAE0 (graphicssystem_d3d11.cpp:497)
        void setSwapChainPresentationInterval(GraphicsSystem* pGraphicsSystem, RenderSwapChain* pSwapChain, uint32 presentationInterval); // 0x00C5BBC0 (graphicssystem_d3d11.cpp:513)
        bool resizeRenderSwapChain(RenderSwapChain* pSwapChain, uint32 newWidth, uint32 newHeight); // 0x00C6AB20 (graphicssystem_d3d11.cpp:525)
        GraphicsCommandBuffer* beginFrame(GraphicsSystem* pGraphicsSystem); // 0x00C6DD00 (graphicssystem_d3d11.cpp:567)
        GraphicsCommandBuffer* beginFrame(GraphicsSystem* pGraphicsSystem, RenderSwapChain* pSwapChain); // 0x00C6ABD0 (graphicssystem_d3d11.cpp:590)
        bool endFrame(GraphicsSystem* pGraphicsSystem); // 0x00C70320 (graphicssystem_d3d11.cpp:620)
        bool switchToFullScreenMode(GraphicsSystem* pGraphicsSystem, RenderSwapChain* pSwapChain, uint32 width, uint32 height, uint32 refreshRateNumerator, uint32 refreshRateDenominator, WindowMode windowMode); // 0x00C6DDD0 (graphicssystem_d3d11.cpp:657)
        void setWindowSize(GraphicsSystem* pGraphicsSystem, uint32 windowWidth, uint32 windowHeight, RenderSwapChain* pSwapChain); // 0x00C5BBF0 (graphicssystem_d3d11.cpp:774)
        bool switchToWindowedMode(GraphicsSystem* pGraphicsSystem, RenderSwapChain* pSwapChain); // 0x00C6AC70 (graphicssystem_d3d11.cpp:798)
        bool toggleWindowMode(GraphicsSystem* pGraphicsSystem, RenderSwapChain* pSwapChain); // 0x00C6E100 (graphicssystem_d3d11.cpp:837)
        WindowMode getCurrentWindowMode(const GraphicsSystem* pGraphicsSystem, const RenderSwapChain* pSwapChain); // 0x00C61600 (graphicssystem_d3d11.cpp:862)
        bool isInBorderlessMode(const GraphicsSystem* pGraphicsSystem, const RenderSwapChain* pSwapChain); // 0x00C5BCB0 (graphicssystem_d3d11.cpp:883)
        bool isInFullScreenMode(const GraphicsSystem* pGraphicsSystem, const RenderSwapChain* pSwapChain); // 0x00C5BCF0 (graphicssystem_d3d11.cpp:903)
        void regainFocus(GraphicsSystem* pGraphicsSystem); // 0x00C6E170 (graphicssystem_d3d11.cpp:928)
        uint32 enumerateDisplayModes(DisplayMode* pDisplayModes, uint32 displayModeCount, GraphicsSystem* pGraphicsSystem, RenderSwapChain* pSwapChain); // 0x00C61670 (graphicssystem_d3d11.cpp:937)
        bool isDeviceLost(const GraphicsSystem* pGraphicsSystem); // 0x00C5BD50 (graphicssystem_d3d11.cpp:1042)
        bool resetDevice(GraphicsSystem* pGraphicsSystem); // 0x00C5BD60 (graphicssystem_d3d11.cpp:1051)
        const RenderTarget* getBackBufferRenderTarget(const GraphicsSystem* pGraphicsSystem); // 0x00C61930 (graphicssystem_d3d11.cpp:1061)
        const RenderTarget* getNoGammaBackBufferRenderTarget(const GraphicsSystem* pGraphicsSystem); // 0x00C61970 (graphicssystem_d3d11.cpp:1083)
        uint32 getRenderTargetWidth(const RenderTarget* pRenderTarget); // 0x00C5BD90 (graphicssystem_d3d11.cpp:1104)
        uint32 getRenderTargetHeight(const RenderTarget* pRenderTarget); // 0x00C5BDA0 (graphicssystem_d3d11.cpp:1113)
        const TextureData* getRenderTargetColorBuffer(const RenderTarget* pRenderTarget, uint32 index); // 0x00C5BDB0 (graphicssystem_d3d11.cpp:1122)
        const TextureData* getRenderTargetDepthBuffer(const RenderTarget* pRenderTarget); // 0x00C5BDD0 (graphicssystem_d3d11.cpp:1135)
        float getScreenAspectRatio(const GraphicsSystem* pGraphicsSystem); // 0x00C619B0 (graphicssystem_d3d11.cpp:1149)
        ID3D11Device* getD3dDevice(GraphicsSystem* pGraphicsSystem); // 0x00C5BDF0 (graphicssystem_d3d11.cpp:1169)
        GraphicsCommandBuffer* getImmediateCommandBuffer(GraphicsSystem* pGraphicsSystem); // 0x00C5BE00 (graphicssystem_d3d11.cpp:1175)
        bool createShaderPipeline(ShaderPipeline* pTarget, GraphicsSystem* pGraphicsSystem, const ShaderPipelineParameters& parameters); // 0x00C5BE10 (graphicssystem_d3d11.cpp:1182)
        void destroyShaderPipeline(GraphicsSystem* pGraphicsSystem, ShaderPipeline* pPipeline); // 0x00C5BE30 (graphicssystem_d3d11.cpp:1193)
        const FragmentShader* getEmptyFragmentShader(const GraphicsSystem* pGraphicsSystem); // 0x00C5BE50 (graphicssystem_d3d11.cpp:1201)
        const BlendState* createBlendState(GraphicsSystem* pGraphicsSystem, const BlendParametersEx& parameters); // 0x00C71AC0 (graphicssystem_d3d11.cpp:1206)
        void destroyBlendState(GraphicsSystem* pGraphicsSystem, const BlendState* pBlendState); // 0x00C703D0 (graphicssystem_d3d11.cpp:1262)
        const RasterizerState* createRasterizerState(GraphicsSystem* pGraphicsSystem, const RasterizerParameters& parameters); // 0x00C71D70 (graphicssystem_d3d11.cpp:1274)
        void destroyRasterizerState(GraphicsSystem* pGraphicsSystem, const RasterizerState* pRasterizerState); // 0x00C70400 (graphicssystem_d3d11.cpp:1354)
        const SamplerState* createSamplerState(GraphicsSystem* pGraphicsSystem, const SamplerParameters& parameters); // 0x00C71EE0 (graphicssystem_d3d11.cpp:1469)
        void destroySamplerState(GraphicsSystem* pGraphicsSystem, const SamplerState* pSamplerState); // 0x00C70430 (graphicssystem_d3d11.cpp:1512)
        const DepthStencilState* createDepthStencilState(GraphicsSystem* pGraphicsSystem, const DepthStencilParameters& parameters); // 0x00C72160 (graphicssystem_d3d11.cpp:1522)
        void destroyDepthStencilState(GraphicsSystem* pGraphicsSystem, const DepthStencilState* pDepthStencilState); // 0x00C70460 (graphicssystem_d3d11.cpp:1569)
        const VertexFormat* createVertexFormat(GraphicsSystem* pGraphicsSystem, const VertexAttributeDescription* pAttributes, uint32 attributeCount); // 0x00C72620 (graphicssystem_d3d11.cpp:1581)
        void destroyVertexFormat(GraphicsSystem* pGraphicsSystem, const VertexFormat* pFormat); // 0x00C70490 (graphicssystem_d3d11.cpp:1597)
    }

    DXGI_FORMAT getD3dVertexAttributeFormat(uint32 format); // 0x00C5BE60 (graphicssystem_d3d11.cpp:1635)

    namespace graphics
    {
        const VertexInputBinding* createVertexInputBinding(GraphicsSystem* pGraphicsSystem, const VertexFormat* pVertexFormat, const ShaderPipeline* pPipeline, uint32 geometryModeMask); // 0x00C72670 (graphicssystem_d3d11.cpp:1706)
        void destroyVertexInputBinding(GraphicsSystem* pGraphicsSystem, const VertexInputBinding* pInputBinding); // 0x00C704B0 (graphicssystem_d3d11.cpp:1771)
        const RenderTarget* createRenderTarget(GraphicsSystem* pGraphicsSystem, uint32 width, uint32 height, const RenderTargetBuffer* pColorBuffers, uint32 colorBufferCount, const RenderTargetBuffer* pDepthBuffer, TilingMode); // 0x00C6AD00 (graphicssystem_d3d11.cpp:1802)
        void destroyRenderTarget(GraphicsSystem* pGraphicsSystem, const RenderTarget* pRenderTarget); // 0x00C61A70 (graphicssystem_d3d11.cpp:1918)
        const RenderTarget* createRenderTarget(GraphicsSystem* pGraphicsSystem, const RenderTarget* pColorBufferSource, const RenderTarget* pDepthStencilBufferSource); // 0x00C61AC0 (graphicssystem_d3d11.cpp:1937)
        DynamicConstantBuffer* createDynamicConstantBuffer(GraphicsSystem* pGraphicsSystem, uint32 firstRegisterIndex, uint32 sizeInBytes); // 0x00C6B180 (graphicssystem_d3d11.cpp:1988)
        void destroyDynamicConstantBuffer(GraphicsSystem* pGraphicsSystem, DynamicConstantBuffer* pConstantBuffer); // 0x00C61C00 (graphicssystem_d3d11.cpp:2026)
        uint32 getDynamicConstantBufferSize(const DynamicConstantBuffer* pBuffer); // 0x00C5BF70 (graphicssystem_d3d11.cpp:2038)
        StaticConstantBuffer* createStaticConstantBuffer(GraphicsSystem* pGraphicsSystem, uint32 firstRegisterIndex, uint32 sizeInBytes); // 0x00C6B220 (graphicssystem_d3d11.cpp:2043)
        void destroyStaticConstantBuffer(GraphicsSystem* pGraphicsSystem, StaticConstantBuffer* pConstantBuffer); // 0x00C61C30 (graphicssystem_d3d11.cpp:2081)
        uint32 getStaticConstantBufferSize(const StaticConstantBuffer* pBuffer); // 0x00C5BF80 (graphicssystem_d3d11.cpp:2094)
        GeometryInstancingBuffer* createGeometryInstancingBuffer(GraphicsSystem* pGraphicsSystem, MemoryAllocator* pInstanceDataAllocator, uint32 instanceBufferSizeInBytes); // 0x00C6B2C0 (graphicssystem_d3d11.cpp:2099)
        void destroyGeometryInstancingBuffer(GeometryInstancingBuffer* pInstancingBuffer, GraphicsSystem* pGraphicsSystem, MemoryAllocator* pInstanceDataAllocator); // 0x00C61C60 (graphicssystem_d3d11.cpp:2143)
        void flipGeometryInstancingBuffer(GeometryInstancingBuffer* pBuffer); // 0x00C5BF90 (graphicssystem_d3d11.cpp:2164)
        uint32 getSwapChainWidth(const RenderSwapChain& swapChain); // 0x00C5C230 (graphicssystem_d3d11.cpp:2768)
        uint32 getSwapChainHeight(const RenderSwapChain& swapChain); // 0x00C5C240 (graphicssystem_d3d11.cpp:2773)
        void* getSwapChainHWND(GraphicsSystem* pGraphicsSystem, const RenderSwapChain* pSwapChain); // 0x00C5C250 (graphicssystem_d3d11.cpp:2778)
        void startScreenCapture(GraphicsSystem* pGraphicsSystem, FileSystem* pFileSystem, uint32 width, uint32 height, uint32 bufferCount, const char* pBasePath); // 0x00C741D0 (graphicssystem_d3d11.cpp:2794)
        void stopScreenCapture(GraphicsSystem* pGraphicsSystem); // 0x00C72FA0 (graphicssystem_d3d11.cpp:2803)
        bool isScreenCaptureActive(GraphicsSystem* pGraphicsSystem); // 0x00C5C280 (graphicssystem_d3d11.cpp:2816)
        bool isScreenCaptureSavingActive(uint32* pIndex, GraphicsSystem* pGraphicsSystem); // 0x00C62280 (graphicssystem_d3d11.cpp:2823)
        void saveScreenCaptureFrames(GraphicsSystem* pGraphicsSystem, const char* pSubPath, const char* pFilenamePrefix, uint32 frameCount); // 0x00C6B3F0 (graphicssystem_d3d11.cpp:2830)
        RecordedCommandBuffer* createCommandBuffer(GraphicsSystem* pGraphicsSystem, MemoryBlock); // 0x00C622C0 (graphicssystem_d3d11.cpp:2840)
        void destroyCommandBuffer(GraphicsSystem* pGraphicsSystem, RecordedCommandBuffer* pCommandBuffer); // 0x00C5C290 (graphicssystem_d3d11.cpp:2882)
        GraphicsCommandBuffer* beginCommandBuffer(GraphicsSystem* pGraphicsSystem, RecordedCommandBuffer* pCommandBuffer); // 0x00C5C2D0 (graphicssystem_d3d11.cpp:2897)
        void endCommandBuffer(GraphicsSystem*, GraphicsCommandBuffer* pCommandBuffer); // 0x00C5C310 (graphicssystem_d3d11.cpp:2915)
        bool setOutputGamma(GraphicsSystem* pGraphicsSystem, float gammaScale); // 0x00C5C340 (graphicssystem_d3d11.cpp:2926)
        void acquireThreadOwnership(GraphicsSystem* pGraphicsSystem); // 0x00C5C4C0 (graphicssystem_d3d11.cpp:2962)
        void releaseThreadOwnership(GraphicsSystem* pGraphicsSystem); // 0x00C5C4D0 (graphicssystem_d3d11.cpp:2967)
        void handleFatalHResult(GraphicsSystem* pGraphicsSystem, long result, const char* pFunctionDescription); // 0x00C623D0 (graphicssystem_d3d11.cpp:2982)
    }

    void createGraphicsSystemBase(GraphicsSystemBase* pGraphicsSystemBase, MemoryAllocator* pSystemAllocator, FileSystem* pShaderFileSystem, MemoryBlock shaderObjectMemoryBlock, MemoryBlock shaderCodeMemoryBlock, const CombinedGraphicsStateObjectPoolParameters& poolParameters); // 0x00C706F0 (:13)
    void createGraphicsSystemBaseObjects(GraphicsSystemBase* pGraphicsSystemBase, MemoryAllocator* pTextureAllocator); // 0x00C73230 (:24)
    void destroyGraphicsSystemBase(GraphicsSystemBase* pGraphicsSystemBase, MemoryAllocator* pTextureAllocator); // 0x00C72BD0 (:30)

    namespace graphics
    {
        const FragmentShader* createFragmentShaderFromRetainedMemoryBlock(GraphicsSystem* pGraphicsSystem, MemoryBlock memBlock); // 0x00C6C450 (:39)
        const FragmentShader* createFragmentShaderFromFile(GraphicsSystem* pGraphicsSystem, const char* pFileName, const char* pDebugName); // 0x00C6F700 (:46)
        void destroyFragmentShader(GraphicsSystem* pGraphicsSystem, const FragmentShader* pFragmentShader); // 0x00C6C470 (:66)
        const VertexShader* createVertexShaderFromRetainedMemoryBlock(GraphicsSystem* pGraphicsSystem, MemoryBlock memBlock); // 0x00C6F730 (:73)
        const VertexShader* createVertexShaderFromFile(GraphicsSystem* pGraphicsSystem, const char* pFileName, const char* pDebugName); // 0x00C6F750 (:80)
        void destroyVertexShader(GraphicsSystem* pGraphicsSystem, const VertexShader* pVertexShader); // 0x00C6F780 (:101)
        const VertexFormat* getStockVertexFormat(const GraphicsSystem* pGraphicsSystem, GraphicsStockVertexFormat formatId); // 0x00C5D110 (:108)
        const ShaderPipeline* getStockShaderPipeline(GraphicsSystem* pGraphicsSystem, GraphicsStockShaderPipelineId pipelineId); // 0x00C5D120 (:115)
        const TextureData* getStockTexture(const GraphicsSystem* pGraphicsSystem, GraphicsStockTextureId textureId); // 0x00C5D130 (:122)
    }
}
