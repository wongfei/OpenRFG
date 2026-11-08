// Original: library/modules/runtime/graphics/include/keen/graphics/graphicsstateobjects.hpp
//
// Render state descriptions (blend, rasterizer, depth/stencil, sampler) and their enums. The state objects are cached
// and reference counted by hash (GraphicsStateObject; see sources/global/graphicsstateobjectpool.hpp); their D3D11
// definitions are in sources/d3d11/graphicssystem_d3d11.hpp. Placement of the enums and BlendParameters/
// BlendStateParameters inferred.
#pragma once

#include "keen/base/types.hpp"

namespace keen
{
    enum GraphicsStateObjectType
    {
        GraphicsStateObjectType_BlendState = 0,
        GraphicsStateObjectType_RasterizerState = 1,
        GraphicsStateObjectType_SamplerState = 2,
        GraphicsStateObjectType_DepthStencilState = 3,
        GraphicsStateObjectType_VertexFormat = 4,
        GraphicsStateObjectType_VertexInputBinding = 5,
        GraphicsStateObjectType_Count = 6,
    };

    enum BlendOperation
    {
        BlendOperation_Invalid = -1,
        BlendOperation_None = 0,
        BlendOperation_Add = 1,
        BlendOperation_Subtract = 2,
        BlendOperation_RevSubtract = 3,
        BlendOperation_Min = 4,
        BlendOperation_Max = 5,
        BlendOperation_Count = 6,
    };

    enum BlendFactor
    {
        BlendFactor_Invalid = -1,
        BlendFactor_Zero = 0,
        BlendFactor_One = 1,
        BlendFactor_SrcColor = 2,
        BlendFactor_InvSrcColor = 3,
        BlendFactor_SrcAlpha = 4,
        BlendFactor_InvSrcAlpha = 5,
        BlendFactor_DestAlpha = 6,
        BlendFactor_InvDestAlpha = 7,
        BlendFactor_DestColor = 8,
        BlendFactor_InvDestColor = 9,
        BlendFactor_SrcAlphaSat = 10,
        BlendFactor_BlendFactor = 11,
        BlendFactor_InvBlendFactor = 12,
        BlendFactor_Count = 13,
    };

    enum BlendMode
    {
        BlendMode_Invalid = -1,
        BlendMode_Solid = 0,
        BlendMode_Over = 1,
        BlendMode_Add = 2,
        BlendMode_Count = 3,
    };

    enum ColorWriteMask
    {
        ColorWriteMask_Red = 1,
        ColorWriteMask_Green = 2,
        ColorWriteMask_Blue = 4,
        ColorWriteMask_Alpha = 8,
        ColorWriteMask_RGB = 7,
        ColorWriteMask_All = 15,
    };

    enum CullMode
    {
        CullMode_Invalid = -1,
        CullMode_None = 0,
        CullMode_Front = 1,
        CullMode_Back = 2,
        CullMode_Count = 3,
    };

    enum FillMode
    {
        FillMode_Invalid = -1,
        FillMode_Wireframe = 0,
        FillMode_Solid = 1,
        FillMode_Count = 2,
    };

    enum WindingOrder
    {
        WindingOrder_Invalid = -1,
        WindingOrder_Ccw = 0,
        WindingOrder_Cw = 1,
        WindingOrder_Count = 2,
    };

    enum RasterizerFlagMask
    {
        RasterizerFlagMask_CullAgainstFrustum = 1,
        RasterizerFlagMask_EnableScissorTest = 2,
        RasterizerFlagMask_Default = 1,
    };

    enum ComparisonFunction
    {
        ComparisonFunction_Invalid = -1,
        ComparisonFunction_Never = 0,
        ComparisonFunction_Less = 1,
        ComparisonFunction_Equal = 2,
        ComparisonFunction_LessEqual = 3,
        ComparisonFunction_Greater = 4,
        ComparisonFunction_NotEqual = 5,
        ComparisonFunction_GreaterEqual = 6,
        ComparisonFunction_Always = 7,
        ComparisonFunction_Count = 8,
    };

    enum StencilOperation
    {
        StencilOperation_Keep = 0,
        StencilOperation_Zero = 1,
        StencilOperation_Replace = 2,
        StencilOperation_Increment = 3,
        StencilOperation_Decrement = 4,
        StencilOperation_IncrementWrap = 5,
        StencilOperation_DecrementWrap = 6,
        StencilOperation_Invert = 7,
        StencilOperation_Count = 8,
    };

    enum SamplerFilterMode
    {
        SamplerFilterMode_Invalid = -1,
        SamplerFilterMode_None = 0,
        SamplerFilterMode_Nearest = 1,
        SamplerFilterMode_Linear = 2,
        SamplerFilterMode_Anisotropic = 3,
        SamplerFilterMode_Count = 4,
    };

    enum SamplerAddressMode
    {
        SamplerAddressMode_Invalid = -1,
        SamplerAddressMode_Wrap = 0,
        SamplerAddressMode_Mirror = 1,
        SamplerAddressMode_Clamp = 2,
        SamplerAddressMode_MirrorOnce = 3,
        SamplerAddressMode_Border = 4,
        SamplerAddressMode_Count = 5,
    };

    // size 0x8
    struct GraphicsStateObject
    {
        uint32 hash; // 0x00
        uint32 refCount; // 0x04
    };

    // size 0x14
    struct BlendParameters
    {
        BlendParameters();
        uint32 getHash() const;

        BlendOperation blendOp; // 0x00
        BlendFactor sourceFactor; // 0x04
        BlendFactor destFactor; // 0x08
        uint8 colorWriteMask[8]; // 0x0C
    };

    // size 0xC
    struct BlendStateParameters
    {
        BlendOperation blendOp; // 0x00
        BlendFactor sourceFactor; // 0x04
        BlendFactor destFactor; // 0x08
    };

    // size 0x20
    struct BlendParametersEx
    {
        BlendParametersEx(); // 0x004DB490 (.h:198)
        uint32 getHash() const; // 0x00C5A150 (.h:210)

        BlendOperation blendOpColor; // 0x00
        BlendFactor sourceFactorColor; // 0x04
        BlendFactor destFactorColor; // 0x08
        BlendOperation blendOpAlpha; // 0x0C
        BlendFactor sourceFactorAlpha; // 0x10
        BlendFactor destFactorAlpha; // 0x14
        uint8 colorWriteMask[8]; // 0x18
    };

    // size 0x1C
    struct RasterizerParameters
    {
        RasterizerParameters(); // 0x004DB4C0 (.h:227)
        uint32 getHash() const; // 0x00C5A160 (.h:239)

        CullMode cullMode; // 0x00
        FillMode fillMode; // 0x04
        WindingOrder windingOrder; // 0x08
        uint32 flags; // 0x0C
        float depthBias; // 0x10
        float depthBiasSlopeScale; // 0x14
        float depthBiasClamp; // 0x18
    };

    // size 0x40
    struct DepthStencilParameters
    {
        // size 0x1C
        struct StencilParameters
        {
            StencilParameters(); // 0x004B2190 (.h:308)
            void set(bool, ComparisonFunction, uint32, uint32, StencilOperation, StencilOperation, StencilOperation);

            bool testEnabled; // 0x00
            ComparisonFunction testFunc; // 0x04
            uint32 testMask; // 0x08
            uint32 writeMask; // 0x0C
            StencilOperation opFail; // 0x10
            StencilOperation opDepthFail; // 0x14
            StencilOperation opDepthPass; // 0x18
        };

        DepthStencilParameters(); // 0x004DB550 (.h:293)
        uint32 getHash() const; // 0x00C5A180 (.h:301)

        ComparisonFunction depthComparisonFunction; // 0x00
        bool depthTestEnabled; // 0x04
        bool depthWriteEnabled; // 0x05
        StencilParameters frontStencil; // 0x08
        StencilParameters backStencil; // 0x24
    };

    // size 0x30
    struct SamplerParameters
    {
        SamplerParameters(); // 0x004DB500 (.h:254)
        uint32 getHash() const; // 0x00C5A170 (.h:272)

        float mipLodBias; // 0x00
        float minLod; // 0x04
        float maxLod; // 0x08
        uint32 maxAnisotropyLevel; // 0x0C
        SamplerFilterMode minFilter; // 0x10
        SamplerFilterMode magFilter; // 0x14
        SamplerFilterMode mipFilter; // 0x18
        SamplerAddressMode addressU; // 0x1C
        SamplerAddressMode addressV; // 0x20
        SamplerAddressMode addressW; // 0x24
        ComparisonFunction comparisonFunction; // 0x28
        bool alphaKillEnabled; // 0x2C
    };
}
